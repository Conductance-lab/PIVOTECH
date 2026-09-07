/*
 * M3：自研 Python 子集解释器（供 AI 测试模式使用）
 * docs/AI测试模式设计方案.md §5.4 / §5.5 / §5.9
 *
 * v1 子集：数字/字符串/布尔/None、list（字面量/下标读写/append/len）、
 *   + - * / // % **、比较、and/or/not、if/elif/else、while、
 *   for x in range(...)（range 返回列表，循环次数受内存限制）、
 *   def（不支持递归）、return/break/continue/pass、print()、赋值（含下标）、注释 #。
 *
 * 内存：池/词法/符号表在 mp_load 时从堆分配；字符串与列表元素用轻量 arena。
 * 运行：mp_execute() 由 core0 阻塞执行；语句/迭代边界调用心跳，返回 true 即停。
 */
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/stdio_usb.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "CONFIG_FLO.hpp"

#include "read/mp_runtime.hpp"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstdarg>

// ================= 配置上限 =================
// 注意：这些池在 mp_load 时从堆一次性分配；RP2040 可用堆有限（约百余 KB），
// 取值需“够用 + 留足堆余量”，脚本过长会返回“内存不足/脚本过大”而非崩溃。
enum
{
    MP_POOL_CAP = 800,           // AST 节点（~32KB）
    MP_TOK_CAP = 1000,           // 词法单元（~24KB）
    MP_BIND_CAP = 192,           // 变量（~6KB）
    MP_ARENA_SIZE = 10 * 1024,   // 字符串 + list 元素（10KB）
    MP_MAX_NAT_ARGS = 12
};

// ================= 轻量 arena =================
static unsigned char *s_arena = nullptr;
static size_t s_arenaTop = 0;

static void *arenaAlloc(size_t n)
{
    n = (n + 7u) & ~7u;
    if (!s_arena || s_arenaTop + n > MP_ARENA_SIZE)
        return nullptr;
    void *p = s_arena + s_arenaTop;
    s_arenaTop += n;
    return p;
}
static char *arenaStr(const char *s, size_t len)
{
    char *p = (char *)arenaAlloc(len + 1);
    if (!p)
        return nullptr;
    memcpy(p, s, len);
    p[len] = '\0';
    return p;
}
static char *arenaDup(const char *s) { return arenaStr(s, strlen(s)); }

// ================= 值 =================
enum VType
{
    V_NONE, V_INT, V_FLT, V_BOOL, V_STR, V_LIST, V_FN, V_NAT
};

struct MList;
struct MVal
{
    int t;
    union { long long i; double f; void *p; };
    const char *s;
    int node;
};
struct MList
{
    int cap;
    int len;
    MVal *data;
};

static MVal g_none = {V_NONE, {0}, nullptr, -1};
static MVal g_true = {V_BOOL, {1}, nullptr, -1};
static MVal g_false = {V_BOOL, {0}, nullptr, -1};

static MVal mkInt(long long v) { MVal r = {V_INT, {0}, nullptr, -1}; r.i = v; return r; }
static MVal mkFlt(double v) { MVal r = {V_FLT, {0}, nullptr, -1}; r.f = v; return r; }
static MVal mkBool(bool b) { return b ? g_true : g_false; }
static MVal mkStr(const char *s) { MVal r = {V_STR, {0}, s, -1}; return r; }
static MVal mkList(MList *l) { MVal r = {V_LIST, {0}, nullptr, -1}; r.p = l; return r; }
static double valNum(const MVal *v)
{
    if (v->t == V_INT || v->t == V_BOOL)
        return (double)v->i;
    if (v->t == V_FLT)
        return v->f;
    return 0.0;
}

// ================= AST =================
enum NK
{
    N_NUM, N_STR, N_NAME, N_BOOL, N_NONE,
    N_LIST, N_UNARY, N_BIN, N_CALL, N_INDEX,
    N_ASSIGN, N_EXPR, N_IF, N_WHILE, N_FOR,
    N_DEF, N_RETURN, N_BREAK, N_CONTINUE, N_PASS,
    N_BLOCK
};

struct MNode
{
    int kind, line;
    int n0, n1, n2, n3;
    double f;
    const char *s;
};

static MNode *s_pool = nullptr;
static int s_poolLen = 0;

static MNode *nodeNew(int kind, int line)
{
    if (s_poolLen >= MP_POOL_CAP)
        return nullptr;
    MNode *nd = &s_pool[s_poolLen++];
    memset(nd, 0, sizeof(MNode));
    nd->kind = kind;
    nd->line = line;
    nd->n0 = nd->n1 = nd->n2 = nd->n3 = -1;
    return nd;
}

// 每个 BLOCK 节点维护自己的“直接语句索引表”（不能依赖池内连续区间，
// 因为表达式子节点会穿插分配，池序≠语句序）
struct BlkList
{
    int *d;
    int n;
    int cap;
};
static BlkList *s_blk = nullptr;

static int blkInitList(int idx)
{
    if (!s_blk)
        return -1;
    s_blk[idx].n = 0;
    if (s_blk[idx].cap == 0)
    {
        s_blk[idx].cap = 4;
        s_blk[idx].d = (int *)arenaAlloc(4 * sizeof(int));
        if (!s_blk[idx].d)
            return -1;
    }
    return 0;
}

static int blkAdd(int idx, int stmt)
{
    if (!s_blk || s_blk[idx].n >= s_blk[idx].cap)
    {
        int nc = s_blk ? s_blk[idx].cap * 2 : 4;
        int *nd = (int *)arenaAlloc((size_t)nc * sizeof(int));
        if (!nd)
            return -1;
        if (s_blk && s_blk[idx].n)
            memcpy(nd, s_blk[idx].d, (size_t)s_blk[idx].n * sizeof(int));
        s_blk[idx].d = nd;
        s_blk[idx].cap = nc;
    }
    s_blk[idx].d[s_blk[idx].n++] = stmt;
    return 0;
}

// ================= 错误 / 停止 =================
static int s_errLine = 0;
static char s_errMsg[96] = {0};
static volatile bool s_stopReq = false;
static MpHeartbeatFn s_hb = nullptr;
static MpPrintFn s_printCb = nullptr; // print/disp_* 输出后回调（事件驱动即时上屏）
static MpLineFn s_lineCb = nullptr;   // 执行行号变化回调（运行指示灯触发）
static int s_lastCbLine = 0;          // 上次已回调行（行变化检测用）
static int s_curLine = 0;
static const char *s_srcLines = nullptr; // 载入脚本副本（错误行回溯用）
static int s_callDepth = 0;               // 函数调用深度（防递归打爆栈）
static const int MAX_CALL_DEPTH = 40;

enum Flow
{
    F_NONE = 0, F_BREAK, F_CONT, F_RET, F_ERR
};

static void setErr(int line, const char *fmt, ...)
{
    s_errLine = line;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_errMsg, sizeof(s_errMsg), fmt, ap);
    va_end(ap);
}

// ================= 词法 =================
enum TK
{
    T_END = 0, T_NUM, T_NAME, T_STR, T_NEWLINE, T_INDENT, T_DEDENT, T_OP
};

struct MTok
{
    double num;
    const char *s;
    int type;
    int line;
};

static MTok *s_toks = nullptr;
static int s_tokLen = 0;

static bool tokPush(int type, int line, double num = 0, const char *s = nullptr)
{
    if (s_tokLen >= MP_TOK_CAP)
        return false;
    MTok *t = &s_toks[s_tokLen++];
    t->type = type;
    t->line = line;
    t->num = num;
    t->s = s;
    return true;
}

static bool isDigit(char c) { return c >= '0' && c <= '9'; }
static bool isAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static bool isAlnum(char c) { return isAlpha(c) || isDigit(c); }

static const char *OPS[] = {"**", "//", "<=", ">=", "==", "!=", "+", "-", "*", "/", "%", "<", ">", "=", "(", ")", ":", ",", "[", "]"};
static const int N_OPS = (int)(sizeof(OPS) / sizeof(OPS[0]));

static bool tokenize(const char *src)
{
    s_tokLen = 0;
    int line = 1;
    const char *p = src;
    int indentStack[64];
    int indentTop = 0;
    indentStack[0] = 0;
    bool atLineStart = true;
    int pendingSpaces = 0;

    while (*p)
    {
        char c = *p;
        if (c == '\r')
        {
            p++;
            continue;
        }
        if (atLineStart)
        {
            if (c == ' ' || c == '\t')
            {
                pendingSpaces += (c == ' ') ? 1 : 4;
                p++;
                continue;
            }
            if (c == '#')
            {
                while (*p && *p != '\n')
                    p++;
                continue;
            }
            if (c == '\n')
            {
                pendingSpaces = 0;
                p++;
                line++;
                continue;
            }
            int sp = pendingSpaces;
            pendingSpaces = 0;
            atLineStart = false;
            if (sp > indentStack[indentTop])
            {
                if (indentTop + 1 >= 64)
                {
                    setErr(line, "缩进过深");
                    return false;
                }
                indentTop++;
                indentStack[indentTop] = sp;
                if (!tokPush(T_INDENT, line))
                {
                    setErr(line, "词法缓冲满");
                    return false;
                }
            }
            else
            {
                while (sp < indentStack[indentTop])
                {
                    indentTop--;
                    if (!tokPush(T_DEDENT, line))
                    {
                        setErr(line, "词法缓冲满");
                        return false;
                    }
                }
                if (sp != indentStack[indentTop])
                {
                    setErr(line, "缩进不匹配");
                    return false;
                }
            }
        }

        if (c == '\n')
        {
            line++;
            p++;
            atLineStart = true;
            if (!tokPush(T_NEWLINE, line - 1))
            {
                setErr(line - 1, "词法缓冲满");
                return false;
            }
            continue;
        }
        if (c == ' ' || c == '\t')
        {
            p++;
            continue;
        }
        if (c == '#')
        {
            while (*p && *p != '\n')
                p++;
            continue;
        }

        if (isDigit(c))
        {
            const char *st = p;
            while (isDigit(*p))
                p++;
            if (*p == '.')
            {
                p++;
                while (isDigit(*p))
                    p++;
            }
            char tmp[40];
            size_t len = (size_t)(p - st);
            if (len >= sizeof(tmp))
                len = sizeof(tmp) - 1;
            memcpy(tmp, st, len);
            tmp[len] = '\0';
            if (!tokPush(T_NUM, line, atof(tmp)))
            {
                setErr(line, "词法缓冲满");
                return false;
            }
            continue;
        }
        if (isAlpha(c))
        {
            const char *st = p;
            while (isAlnum(*p))
                p++;
            char *nm = arenaStr(st, (size_t)(p - st));
            if (!nm)
            {
                setErr(line, "内存不足");
                return false;
            }
            if (!tokPush(T_NAME, line, 0, nm))
            {
                setErr(line, "词法缓冲满");
                return false;
            }
            continue;
        }
        if (c == '"' || c == '\'')
        {
            char q = c;
            p++;
            const char *st = p;
            const char *end = st;
            size_t outLen = 0;
            while (*p && *p != q && *p != '\n')
            {
                if (*p == '\\' && (p[1] == 'n' || p[1] == 't' || p[1] == '"' || p[1] == '\\' || p[1] == '\''))
                {
                    p++;
                    end++;
                }
                p++;
                end++;
                outLen++;
            }
            if (*p != q)
            {
                setErr(line, "字符串未闭合");
                return false;
            }
            char *out = (char *)arenaAlloc(outLen + 1);
            if (!out)
            {
                setErr(line, "内存不足");
                return false;
            }
            const char *sp = st;
            size_t oi = 0;
            while (sp < end)
            {
                if (*sp == '\\')
                {
                    char e = sp[1];
                    out[oi++] = (e == 'n') ? '\n' : (e == 't') ? '\t' : e;
                    sp += 2;
                }
                else
                    out[oi++] = *sp++;
            }
            out[oi] = '\0';
            p++;
            if (!tokPush(T_STR, line, 0, out))
            {
                setErr(line, "词法缓冲满");
                return false;
            }
            continue;
        }
        bool matched = false;
        for (int k = 0; k < N_OPS; ++k)
        {
            size_t ol = strlen(OPS[k]);
            if (strncmp(p, OPS[k], ol) == 0)
            {
                if (!tokPush(T_OP, line, 0, OPS[k]))
                {
                    setErr(line, "词法缓冲满");
                    return false;
                }
                p += ol;
                matched = true;
                break;
            }
        }
        if (matched)
            continue;
        setErr(line, "无法识别的字符 '%.*s'", 1, p);
        return false;
    }
    while (indentTop > 0)
    {
        indentTop--;
        if (!tokPush(T_DEDENT, line))
            return false;
    }
    if (!tokPush(T_END, line))
        return false;
    return true;
}

// ================= 语法（前置声明） =================
static int s_ti = 0;
static int parseExpr();
static int parseSuite(int stmtLine);
static int parseStmt();

static bool peekOp(const char *op) { return s_toks[s_ti].type == T_OP && strcmp(s_toks[s_ti].s, op) == 0; }
static bool peekName(const char *n) { return s_toks[s_ti].type == T_NAME && strcmp(s_toks[s_ti].s, n) == 0; }
static bool eatOp(const char *op)
{
    if (peekOp(op))
    {
        s_ti++;
        return true;
    }
    return false;
}
static bool expectOp(const char *op)
{
    if (!eatOp(op))
    {
        setErr(s_toks[s_ti].line, "语法错误：需要 '%s'", op);
        return false;
    }
    return true;
}

static int parseCmp();

static int parsePrimary()
{
    MTok *t = &s_toks[s_ti];
    int line = t->line;

    if (t->type == T_NUM)
    {
        s_ti++;
        MNode *nd = nodeNew(N_NUM, line);
        if (!nd)
        {
            setErr(line, "节点内存不足");
            return -1;
        }
        nd->f = t->num;
        return (int)(nd - s_pool);
    }
    if (t->type == T_STR)
    {
        s_ti++;
        MNode *nd = nodeNew(N_STR, line);
        if (!nd)
        {
            setErr(line, "节点内存不足");
            return -1;
        }
        nd->s = t->s;
        return (int)(nd - s_pool);
    }
    if (t->type == T_NAME)
    {
        const char *n = t->s;
        s_ti++;
        if (strcmp(n, "True") == 0 || strcmp(n, "False") == 0)
        {
            MNode *nd = nodeNew(N_BOOL, line);
            if (!nd)
            {
                setErr(line, "节点内存不足");
                return -1;
            }
            nd->n0 = (strcmp(n, "True") == 0) ? 1 : 0;
            return (int)(nd - s_pool);
        }
        if (strcmp(n, "None") == 0)
        {
            MNode *nd = nodeNew(N_NONE, line);
            if (!nd)
            {
                setErr(line, "节点内存不足");
                return -1;
            }
            return (int)(nd - s_pool);
        }
        if (strcmp(n, "not") == 0)
        {
            int operand = parseCmp();
            if (operand < 0)
                return -1;
            MNode *nd = nodeNew(N_UNARY, line);
            if (!nd)
            {
                setErr(line, "节点内存不足");
                return -1;
            }
            nd->n0 = 3;
            nd->n1 = operand;
            return (int)(nd - s_pool);
        }
        if (strcmp(n, "and") == 0 || strcmp(n, "or") == 0)
        {
            setErr(line, "语法错误：'%s' 用法不当", n);
            return -1;
        }

        MNode *nd = nodeNew(N_NAME, line);
        if (!nd)
        {
            setErr(line, "节点内存不足");
            return -1;
        }
        nd->s = n;
        int baseIdx = (int)(nd - s_pool);

        if (eatOp("("))
        {
            int roots[16];
            int rc = 0;
            if (!eatOp(")"))
            {
                for (;;)
                {
                    int a = parseExpr();
                    if (a < 0)
                        return -1;
                    if (rc >= 16)
                    {
                        setErr(line, "参数过多(>16)");
                        return -1;
                    }
                    roots[rc++] = a;
                    if (eatOp(")"))
                        break;
                    if (!expectOp(","))
                        return -1;
                }
            }
            MNode *call = nodeNew(N_CALL, line);
            if (!call)
            {
                setErr(line, "节点内存不足");
                return -1;
            }
            int ci = (int)(call - s_pool);
            call->n0 = baseIdx;
            call->n1 = rc; // 实参个数（实参根节点存子节点表）
            if (blkInitList(ci) < 0)
            {
                setErr(line, "内存不足");
                return -1;
            }
            for (int k = 0; k < rc; ++k)
                if (blkAdd(ci, roots[k]) < 0)
                {
                    setErr(line, "内存不足");
                    return -1;
                }
            return ci;
        }
        if (eatOp("["))
        {
            int idx = parseExpr();
            if (idx < 0)
                return -1;
            if (!expectOp("]"))
                return -1;
            MNode *ix = nodeNew(N_INDEX, line);
            if (!ix)
            {
                setErr(line, "节点内存不足");
                return -1;
            }
            ix->n0 = baseIdx;
            ix->n1 = idx;
            return (int)(ix - s_pool);
        }
        return baseIdx;
    }
    if (peekOp("("))
    {
        s_ti++;
        int e = parseExpr();
        if (e < 0)
            return -1;
        if (!expectOp(")"))
            return -1;
        return e;
    }
    if (peekOp("["))
    {
        s_ti++;
        MNode *lst = nodeNew(N_LIST, line);
        if (!lst)
        {
            setErr(line, "节点内存不足");
            return -1;
        }
        int li = (int)(lst - s_pool);
        int elems = 0;
        if (blkInitList(li) < 0)
        {
            setErr(line, "内存不足");
            return -1;
        }
        if (!eatOp("]"))
        {
            for (;;)
            {
                int a = parseExpr();
                if (a < 0)
                    return -1;
                if (blkAdd(li, a) < 0)
                {
                    setErr(line, "内存不足");
                    return -1;
                }
                elems++;
                if (eatOp("]"))
                    break;
                if (!expectOp(","))
                    return -1;
            }
        }
        lst->n0 = elems;
        return li;
    }
    if (peekOp("+") || peekOp("-"))
    {
        int opid = peekOp("+") ? 1 : 2;
        s_ti++;
        int operand = parsePrimary();
        if (operand < 0)
            return -1;
        MNode *nd = nodeNew(N_UNARY, line);
        if (!nd)
        {
            setErr(line, "节点内存不足");
            return -1;
        }
        nd->n0 = opid;
        nd->n1 = operand;
        return (int)(nd - s_pool);
    }
    setErr(line, "语法错误：意外的标记");
    return -1;
}

static int parseMul()
{
    int l = parsePrimary();
    if (l < 0)
        return -1;
    for (;;)
    {
        const char *op = nullptr;
        if (peekOp("**")) op = "**";
        else if (peekOp("*")) op = "*";
        else if (peekOp("/")) op = "/";
        else if (peekOp("//")) op = "//";
        else if (peekOp("%")) op = "%";
        if (!op)
            break;
        int line = s_toks[s_ti].line;
        s_ti++;
        int r = parseMul();
        if (r < 0)
            return -1;
        int opid = (strcmp(op, "*") == 0) ? 1 : (strcmp(op, "/") == 0) ? 2 : (strcmp(op, "//") == 0) ? 3 : (strcmp(op, "%") == 0) ? 4 : 5;
        MNode *b = nodeNew(N_BIN, line);
        if (!b)
        {
            setErr(line, "节点内存不足");
            return -1;
        }
        b->n0 = opid;
        b->n1 = l;
        b->n2 = r;
        l = (int)(b - s_pool);
    }
    return l;
}

static int parseAdd()
{
    int l = parseMul();
    if (l < 0)
        return -1;
    for (;;)
    {
        const char *op = nullptr;
        if (peekOp("+")) op = "+";
        else if (peekOp("-")) op = "-";
        if (!op)
            break;
        int line = s_toks[s_ti].line;
        s_ti++;
        int r = parseAdd();
        if (r < 0)
            return -1;
        int opid = (strcmp(op, "+") == 0) ? 1 : 2;
        MNode *b = nodeNew(N_BIN, line);
        if (!b)
        {
            setErr(line, "节点内存不足");
            return -1;
        }
        b->n0 = opid;
        b->n1 = l;
        b->n2 = r;
        l = (int)(b - s_pool);
    }
    return l;
}

static int parseCmp()
{
    int l = parseAdd();
    if (l < 0)
        return -1;
    for (;;)
    {
        const char *op = nullptr;
        if (peekOp("==")) op = "==";
        else if (peekOp("!=")) op = "!=";
        else if (peekOp("<")) op = "<";
        else if (peekOp("<=")) op = "<=";
        else if (peekOp(">")) op = ">";
        else if (peekOp(">=")) op = ">=";
        if (!op)
            break;
        int line = s_toks[s_ti].line;
        s_ti++;
        int r = parseAdd();
        if (r < 0)
            return -1;
        int opid = (strcmp(op, "==") == 0) ? 10 : (strcmp(op, "!=") == 0) ? 11 : (strcmp(op, "<") == 0) ? 12 : (strcmp(op, "<=") == 0) ? 13 : (strcmp(op, ">") == 0) ? 14 : 15;
        MNode *b = nodeNew(N_BIN, line);
        if (!b)
        {
            setErr(line, "节点内存不足");
            return -1;
        }
        b->n0 = opid;
        b->n1 = l;
        b->n2 = r;
        l = (int)(b - s_pool);
    }
    return l;
}

static int parseAnd()
{
    int l = parseCmp();
    if (l < 0)
        return -1;
    while (peekName("and"))
    {
        int line = s_toks[s_ti].line;
        s_ti++;
        int r = parseCmp();
        if (r < 0)
            return -1;
        MNode *b = nodeNew(N_BIN, line);
        if (!b)
        {
            setErr(line, "节点内存不足");
            return -1;
        }
        b->n0 = 20;
        b->n1 = l;
        b->n2 = r;
        l = (int)(b - s_pool);
    }
    return l;
}

static int parseExpr()
{
    int l = parseAnd();
    if (l < 0)
        return -1;
    while (peekName("or"))
    {
        int line = s_toks[s_ti].line;
        s_ti++;
        int r = parseAnd();
        if (r < 0)
            return -1;
        MNode *b = nodeNew(N_BIN, line);
        if (!b)
        {
            setErr(line, "节点内存不足");
            return -1;
        }
        b->n0 = 21;
        b->n1 = l;
        b->n2 = r;
        l = (int)(b - s_pool);
    }
    return l;
}

static int parseSuite(int stmtLine)
{
    if (!expectOp(":"))
        return -1;
    if (s_toks[s_ti].type != T_NEWLINE)
    {
        setErr(s_toks[s_ti].line, "需要换行的代码块");
        return -1;
    }
    s_ti++;
    if (s_toks[s_ti].type != T_INDENT)
    {
        setErr(s_toks[s_ti].line, "需要缩进块");
        return -1;
    }
    s_ti++;
    MNode *blk = nodeNew(N_BLOCK, stmtLine);
    if (!blk)
    {
        setErr(stmtLine, "节点内存不足");
        return -1;
    }
    int bi = (int)(blk - s_pool);
    if (blkInitList(bi) < 0)
    {
        setErr(stmtLine, "内存不足");
        return -1;
    }
    while (s_toks[s_ti].type != T_DEDENT && s_toks[s_ti].type != T_END)
    {
        int st = parseStmt();
        if (st < 0)
            return -1;
        if (st == 0) // 空语句（块尾的换行后），交给循环条件结束
            continue;
        if (blkAdd(bi, st) < 0)
        {
            setErr(stmtLine, "内存不足");
            return -1;
        }
    }
    if (s_toks[s_ti].type == T_DEDENT)
        s_ti++;
    else
    {
        setErr(s_toks[s_ti].line, "缺少缩进结束");
        return -1;
    }
    return bi;
}

static int parseIfChain(MNode *outer, int line)
{
    MNode *cur = outer;
    for (;;)
    {
        if (peekName("elif"))
        {
            int nl = s_toks[s_ti].line;
            s_ti++;
            MNode *nxt = nodeNew(N_IF, nl ? nl : line);
            if (!nxt)
            {
                setErr(line, "节点内存不足");
                return -1;
            }
            int cond = parseExpr();
            if (cond < 0)
                return -1;
            nxt->n0 = cond;
            int thenBlk = parseSuite(line);
            if (thenBlk < 0)
                return -1;
            nxt->n1 = thenBlk;
            nxt->n2 = -1;
            cur->n2 = (int)(nxt - s_pool);
            cur = nxt;
            continue;
        }
        if (peekName("else"))
        {
            s_ti++;
            int elseBlk = parseSuite(line);
            if (elseBlk < 0)
                return -1;
            cur->n2 = elseBlk;
        }
        return 0;
    }
}

static int parseDef()
{
    int line = s_toks[s_ti].line;
    s_ti++;
    if (s_toks[s_ti].type != T_NAME)
    {
        setErr(s_toks[s_ti].line, "函数名无效");
        return -1;
    }
    const char *fname = s_toks[s_ti].s;
    s_ti++;
    if (!expectOp("("))
        return -1;
    MNode *nd = nodeNew(N_DEF, line);
    if (!nd)
    {
        setErr(line, "节点内存不足");
        return -1;
    }
    nd->s = fname;
    int paramStart = s_poolLen;
    if (!eatOp(")"))
    {
        for (;;)
        {
            if (s_toks[s_ti].type != T_NAME)
            {
                setErr(s_toks[s_ti].line, "参数名无效");
                return -1;
            }
            MNode *pn = nodeNew(N_NAME, s_toks[s_ti].line);
            if (!pn)
            {
                setErr(s_toks[s_ti].line, "节点内存不足");
                return -1;
            }
            pn->s = s_toks[s_ti].s;
            s_ti++;
            if (eatOp(")"))
                break;
            if (!expectOp(","))
                return -1;
        }
    }
    nd->n0 = paramStart;
    nd->n1 = s_poolLen;
    int body = parseSuite(line);
    if (body < 0)
        return -1;
    nd->n2 = body;
    return (int)(nd - s_pool);
}

static int parseStmt()
{
    while (s_toks[s_ti].type == T_NEWLINE)
        s_ti++;
    MTok *t = &s_toks[s_ti];
    if (t->type == T_END || t->type == T_DEDENT)
        return 0; // 空块结束（块层处理）

    if (t->type == T_NAME && strcmp(t->s, "def") == 0)
        return parseDef();

    if (t->type == T_NAME)
    {
        int line = t->line;
        const char *kw = t->s;

        if (strcmp(kw, "pass") == 0 || strcmp(kw, "break") == 0 || strcmp(kw, "continue") == 0)
        {
            int kind = (strcmp(kw, "pass") == 0) ? N_PASS : (strcmp(kw, "break") == 0) ? N_BREAK : N_CONTINUE;
            s_ti++;
            MNode *nd = nodeNew(kind, line);
            if (!nd)
            {
                setErr(line, "节点内存不足");
                return -1;
            }
            return (int)(nd - s_pool);
        }
        if (strcmp(kw, "return") == 0)
        {
            s_ti++;
            MNode *nd = nodeNew(N_RETURN, line);
            if (!nd)
            {
                setErr(line, "节点内存不足");
                return -1;
            }
            if (s_toks[s_ti].type == T_NEWLINE || s_toks[s_ti].type == T_END || s_toks[s_ti].type == T_DEDENT)
                nd->n0 = -1;
            else
            {
                int e = parseExpr();
                if (e < 0)
                    return -1;
                nd->n0 = e;
            }
            return (int)(nd - s_pool);
        }
        if (strcmp(kw, "if") == 0)
        {
            s_ti++;
            int cond = parseExpr();
            if (cond < 0)
                return -1;
            MNode *nd = nodeNew(N_IF, line);
            if (!nd)
            {
                setErr(line, "节点内存不足");
                return -1;
            }
            nd->n0 = cond;
            int thenBlk = parseSuite(line);
            if (thenBlk < 0)
                return -1;
            nd->n1 = thenBlk;
            nd->n2 = -1;
            if (parseIfChain(nd, line) < 0)
                return -1;
            return (int)(nd - s_pool);
        }
        if (strcmp(kw, "while") == 0)
        {
            s_ti++;
            int cond = parseExpr();
            if (cond < 0)
                return -1;
            MNode *nd = nodeNew(N_WHILE, line);
            if (!nd)
            {
                setErr(line, "节点内存不足");
                return -1;
            }
            nd->n0 = cond;
            int body = parseSuite(line);
            if (body < 0)
                return -1;
            nd->n1 = body;
            return (int)(nd - s_pool);
        }
        if (strcmp(kw, "for") == 0)
        {
            s_ti++;
            if (s_toks[s_ti].type != T_NAME)
            {
                setErr(s_toks[s_ti].line, "for 变量无效");
                return -1;
            }
            const char *var = s_toks[s_ti].s;
            s_ti++;
            if (!peekName("in"))
            {
                setErr(s_toks[s_ti].line, "缺少 in");
                return -1;
            }
            s_ti++;
            MNode *nd = nodeNew(N_FOR, line);
            if (!nd)
            {
                setErr(line, "节点内存不足");
                return -1;
            }
            nd->s = var;
            int seq = parseExpr(); // 通常是 range(...) → 列表；也支持列表字面量
            if (seq < 0)
                return -1;
            nd->n0 = seq;
            int body = parseSuite(line);
            if (body < 0)
                return -1;
            nd->n1 = body;
            return (int)(nd - s_pool);
        }
    }

    // 普通语句：赋值或表达式
    {
        int line = t->line;
        int saved = s_ti;
        if (t->type == T_NAME)
        {
            const char *nm = t->s;
            s_ti++;
            MNode *tgt = nodeNew(N_NAME, line);
            if (!tgt)
            {
                setErr(line, "节点内存不足");
                return -1;
            }
            tgt->s = nm;
            int tgtIdx = (int)(tgt - s_pool);
            if (eatOp("["))
            {
                int e = parseExpr();
                if (e < 0)
                    return -1;
                if (!expectOp("]"))
                    return -1;
                MNode *ix = nodeNew(N_INDEX, line);
                if (!ix)
                {
                    setErr(line, "节点内存不足");
                    return -1;
                }
                ix->n0 = tgtIdx;
                ix->n1 = e;
                tgt = ix;
                tgtIdx = (int)(tgt - s_pool);
            }
            if (eatOp("="))
            {
                int val = parseExpr();
                if (val < 0)
                    return -1;
                MNode *as = nodeNew(N_ASSIGN, line);
                if (!as)
                {
                    setErr(line, "节点内存不足");
                    return -1;
                }
                as->n0 = tgtIdx;
                as->n1 = val;
                return (int)(as - s_pool);
            }
            s_ti = saved;
        }
        int e = parseExpr();
        if (e < 0)
            return -1;
        MNode *ex = nodeNew(N_EXPR, line);
        if (!ex)
        {
            setErr(line, "节点内存不足");
            return -1;
        }
        ex->n0 = e;
        return (int)(ex - s_pool);
    }
}

// ================= 变量（扁平命名空间） =================
static const char **s_names = nullptr;
static MVal *s_vals = nullptr;
static int s_bindCnt = 0;

static int findBind(const char *name)
{
    for (int i = s_bindCnt - 1; i >= 0; --i)
        if (strcmp(s_names[i], name) == 0)
            return i;
    return -1;
}
static bool setBind(const char *name, const MVal &v)
{
    int idx = findBind(name);
    if (idx >= 0)
    {
        s_vals[idx] = v;
        return true;
    }
    if (s_bindCnt >= MP_BIND_CAP)
        return false;
    s_names[s_bindCnt] = name;
    s_vals[s_bindCnt] = v;
    s_bindCnt++;
    return true;
}
static bool getBind(const char *name, MVal &out)
{
    int idx = findBind(name);
    if (idx < 0)
        return false;
    out = s_vals[idx];
    return true;
}

// ================= 内置 id =================
enum NatId
{
    N_PRINT, N_RANGE, N_LEN, N_INT, N_FLOAT, N_STR_FN, N_APPEND,
    N_PIN_INIT, N_PIN_DIR, N_PIN_OUT, N_PIN_IN, N_PIN_PULLUP, N_PWM_INIT, N_PWM_STOP,
    N_PULLUP_EN, N_TA_SWITCH, N_TB_SWITCH, N_TA_PWM, N_TB_PWM, N_TA_PULLUP, N_TB_PULLUP,
    N_TC_PUSH, N_TC_OD, N_TD_OD, N_TE_TX, N_TF_SCK,
    N_LED_TA_TB, N_LED_TC, N_LED_TD,
    N_KEY, N_ADC_RAW, N_ADC_VOLT,
    N_DISP_CLEAR, N_DISP_TEXT, N_SLEEP_MS, N_NOW_MS,
    N_NAT_COUNT
};

static const char *NAT_NAMES[N_NAT_COUNT] = {
    "print", "range", "len", "int", "float", "str", "append",
    "pin_init", "pin_dir", "pin_out", "pin_in", "pin_pullup", "pwm_init", "pwm_stop",
    "pullup_en", "ta_switch", "tb_switch", "ta_pwm", "tb_pwm", "ta_pullup", "tb_pullup",
    "tc_out_push", "tc_out_od", "td_out_od", "te_spi_tx", "tf_spi_sck",
    "led_ta_tb", "led_tc", "led_td",
    "key", "adc_raw", "adc_voltage",
    "disp_clear", "disp_text", "sleep_ms", "now_ms"};

static char s_disp[4][26];
static int s_dispUsed = 0;
static MVal s_retVal;
static int s_retFlag = 0;

static bool truthy(const MVal *v)
{
    switch (v->t)
    {
    case V_NONE: return false;
    case V_BOOL:
    case V_INT: return v->i != 0;
    case V_FLT: return v->f != 0.0;
    case V_STR: return v->s && v->s[0] != '\0';
    case V_LIST:
    {
        MList *l = (MList *)v->p;
        return l && l->len > 0;
    }
    default: return true;
    }
}

static MList *listNew()
{
    MList *l = (MList *)arenaAlloc(sizeof(MList));
    if (!l)
        return nullptr;
    l->cap = 4;
    l->len = 0;
    l->data = (MVal *)arenaAlloc(sizeof(MVal) * (size_t)l->cap);
    if (!l->data)
        return nullptr;
    return l;
}
static bool listPush(MList *l, const MVal &v)
{
    if (l->len >= l->cap)
    {
        int nc = l->cap * 2;
        MVal *nd = (MVal *)arenaAlloc(sizeof(MVal) * (size_t)nc);
        if (!nd)
            return false;
        memcpy(nd, l->data, sizeof(MVal) * (size_t)l->len);
        l->data = nd;
        l->cap = nc;
    }
    l->data[l->len++] = v;
    return true;
}

static void valToStr(const MVal *v, char *out, size_t cap)
{
    switch (v->t)
    {
    case V_NONE: snprintf(out, cap, "None"); break;
    case V_INT: snprintf(out, cap, "%lld", v->i); break;
    case V_FLT:
        if (v->f == (double)(long long)v->f)
            snprintf(out, cap, "%lld", (long long)v->f);
        else
            snprintf(out, cap, "%g", v->f);
        break;
    case V_BOOL: snprintf(out, cap, "%s", v->i ? "True" : "False"); break;
    case V_STR: snprintf(out, cap, "%s", v->s ? v->s : ""); break;
    case V_LIST:
    {
        out[0] = '[';
        MList *l = (MList *)v->p;
        size_t oi = 1;
        if (l)
        {
            for (int i = 0; i < l->len && oi + 8 < cap; ++i)
            {
                if (i)
                    out[oi++] = ',';
                char tmp[32];
                valToStr(&l->data[i], tmp, sizeof(tmp));
                size_t tl = strlen(tmp);
                if (oi + tl >= cap)
                    break;
                memcpy(out + oi, tmp, tl);
                oi += tl;
            }
        }
        out[oi++] = ']';
        out[oi] = 0;
        break;
    }
    default: snprintf(out, cap, "?"); break;
    }
}

static bool evalNode(int idx, MVal &out);
static int execNode(int idx);

static void consoleLine(const char *line)
{
    printf("#PIVOAIOX#%s#PTS#\r\n", line ? line : "");
    snprintf(s_disp[3], sizeof(s_disp[3]), "%s", line ? line : "");
    if (s_dispUsed < 4)
        s_dispUsed = 4;
    if (s_printCb) s_printCb(); // 立即上屏（不过滤速率）
}

static bool setPinOut(uint gp, bool v)
{
    if (gp > 29)
        return false;
    gpio_init(gp);
    gpio_set_dir(gp, GPIO_OUT);
    gpio_put(gp, v ? 1 : 0);
    return true;
}

static int doNative(int id, const MVal *args, int n, MVal &out, int line)
{
    switch (id)
    {
    case N_PRINT:
    {
        char buf[160];
        size_t oi = 0;
        buf[0] = '\0';
        for (int i = 0; i < n && oi < sizeof(buf) - 1; ++i)
        {
            if (i)
                buf[oi++] = ' ';
            char tmp[64];
            valToStr(&args[i], tmp, sizeof(tmp));
            size_t tl = strlen(tmp);
            if (oi + tl >= sizeof(buf) - 1)
                break;
            memcpy(buf + oi, tmp, tl);
            oi += tl;
        }
        buf[oi] = '\0';
        consoleLine(buf);
        out = g_none;
        return 0;
    }
    case N_RANGE:
    {
        long long a = 0, b = 0, c = 1;
        if (n == 1)
            b = (long long)valNum(&args[0]);
        else if (n == 2)
        {
            a = (long long)valNum(&args[0]);
            b = (long long)valNum(&args[1]);
        }
        else if (n >= 3)
        {
            a = (long long)valNum(&args[0]);
            b = (long long)valNum(&args[1]);
            c = (long long)valNum(&args[2]);
        }
        else
        {
            setErr(line, "range 参数数错误");
            return -1;
        }
        if (c == 0)
        {
            setErr(line, "range 步长不能为 0");
            return -1;
        }
        MList *l = listNew();
        if (!l)
        {
            setErr(line, "内存不足");
            return -1;
        }
        if (c > 0)
            for (long long v = a; v < b; v += c)
            {
                if (!listPush(l, mkInt(v)))
                {
                    setErr(line, "内存不足(range 过大)");
                    return -1;
                }
            }
        else
            for (long long v = a; v > b; v += c)
            {
                if (!listPush(l, mkInt(v)))
                {
                    setErr(line, "内存不足(range 过大)");
                    return -1;
                }
            }
        out = mkList(l);
        return 0;
    }
    case N_LEN:
    {
        if (n != 1)
        {
            setErr(line, "len 需要 1 参数");
            return -1;
        }
        if (args[0].t == V_STR)
            out = mkInt((long long)strlen(args[0].s ? args[0].s : ""));
        else if (args[0].t == V_LIST)
        {
            MList *l = (MList *)args[0].p;
            out = mkInt(l ? l->len : 0);
        }
        else
        {
            setErr(line, "len 仅支持 str/list");
            return -1;
        }
        return 0;
    }
    case N_APPEND:
    {
        if (n != 2 || args[0].t != V_LIST)
        {
            setErr(line, "append(list,item) 无效");
            return -1;
        }
        if (!listPush((MList *)args[0].p, args[1]))
        {
            setErr(line, "内存不足");
            return -1;
        }
        out = g_none;
        return 0;
    }
    case N_INT:
        out = (args[0].t == V_STR) ? mkInt(atoll(args[0].s ? args[0].s : "0")) : mkInt((long long)valNum(&args[0]));
        return 0;
    case N_FLOAT:
        out = (args[0].t == V_STR) ? mkFlt(atof(args[0].s ? args[0].s : "0")) : mkFlt(valNum(&args[0]));
        return 0;
    case N_STR_FN:
    {
        char buf[96];
        valToStr(&args[0], buf, sizeof(buf));
        out = mkStr(arenaDup(buf));
        return out.s ? 0 : -1;
    }
    case N_NOW_MS:
        out = mkInt((long long)to_ms_since_boot(get_absolute_time()));
        return 0;
    case N_SLEEP_MS:
    {
        if (n != 1)
        {
            setErr(line, "sleep_ms 需 1 参数");
            return -1;
        }
        long long ms = (long long)valNum(&args[0]);
        if (ms < 0)
            ms = 0;
        uint32_t end = to_ms_since_boot(get_absolute_time()) + (uint32_t)ms;
        while (to_ms_since_boot(get_absolute_time()) < end)
        {
            if (s_stopReq)
                break;
            // 睡眠期间也响应心跳（保持 UI/按键/上位机可中断）
            if (s_hb && s_hb())
            {
                s_stopReq = true;
                break;
            }
            sleep_ms(2);
        }
        out = g_none;
        return 0;
    }
    case N_KEY:
    {
        if (n != 1)
        {
            setErr(line, "key 需 1 参数");
            return -1;
        }
        int kid = (int)valNum(&args[0]);
        uint pin = 0xFF;
        // key: 0=上 1=下 2=OK 3=Cancel（左右不提供）
        switch (kid)
        {
        case 0: pin = KEYUP; break;
        case 1: pin = KEYDOWN; break;
        case 2: pin = KEYOK; break;
        case 3: pin = KEYCAN; break;
        default:
            setErr(line, "key id 需 0~3");
            return -1;
        }
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
        out = mkBool(!gpio_get(pin));
        return 0;
    }
    case N_ADC_RAW:
    case N_ADC_VOLT:
    {
        if (n != 1)
        {
            setErr(line, "adc 需 1 参数");
            return -1;
        }
        int ch = (int)valNum(&args[0]);
        if (ch < 0 || ch > 1)
        {
            setErr(line, "通道需 0(TA)或1(TB)");
            return -1;
        }
        adc_init();
        if (ch == 0)
        {
            adc_gpio_init(TAADCPIN);
            adc_select_input(TAADCPIN - 26);
        }
        else
        {
            adc_gpio_init(TBADCPIN);
            adc_select_input(TBADCPIN - 26);
        }
        int rawv = adc_read();
        if (id == N_ADC_RAW)
        {
            out = mkInt(rawv);
        }
        else
        {
            // ADC 最终电压值只保留小数点后两位（TA=adc_voltage(0)，TB=adc_voltage(1)，打印/参与运算均为两位精度）
            double v = (((double)rawv / 4096.0) * 30.8) - 0.15;
            v = (double)((long long)(v * 100.0 + (v >= 0.0 ? 0.5 : -0.5))) / 100.0;
            out = mkFlt(v);
        }
        return 0;
    }
    case N_DISP_CLEAR:
        for (int i = 0; i < 4; ++i)
            s_disp[i][0] = '\0';
        s_dispUsed = 0;
        out = g_none;
        if (s_printCb) s_printCb(); // 清屏即时可见
        return 0;
    case N_DISP_TEXT:
    {
        if (n != 2 || args[0].t != V_INT || args[1].t != V_STR)
        {
            setErr(line, "disp_text(line,text) 无效");
            return -1;
        }
        int li = (int)args[0].i;
        if (li < 0 || li >= 4)
        {
            setErr(line, "行号需 0~3");
            return -1;
        }
        snprintf(s_disp[li], sizeof(s_disp[li]), "%s", args[1].s ? args[1].s : "");
        if (li + 1 > s_dispUsed)
            s_dispUsed = li + 1;
        out = g_none;
        if (s_printCb) s_printCb(); // 立即上屏
        return 0;
    }
    case N_PIN_INIT:
    {
        uint gp = (uint)valNum(&args[0]);
        if (gp > 29)
        {
            setErr(line, "gpio 无效");
            return -1;
        }
        gpio_init(gp);
        gpio_set_dir(gp, ((int)valNum(&args[1])) ? GPIO_OUT : GPIO_IN);
        out = g_none;
        return 0;
    }
    case N_PIN_DIR:
    {
        uint gp = (uint)valNum(&args[0]);
        if (gp > 29)
        {
            setErr(line, "gpio 无效");
            return -1;
        }
        gpio_set_dir(gp, ((int)valNum(&args[1])) ? GPIO_OUT : GPIO_IN);
        out = g_none;
        return 0;
    }
    case N_PIN_OUT:
    {
        uint gp = (uint)valNum(&args[0]);
        if (!setPinOut(gp, truthy(&args[1])))
        {
            setErr(line, "gpio 无效");
            return -1;
        }
        out = g_none;
        return 0;
    }
    case N_PIN_IN:
    {
        uint gp = (uint)valNum(&args[0]);
        if (gp > 29)
        {
            setErr(line, "gpio 无效");
            return -1;
        }
        gpio_init(gp);
        gpio_set_dir(gp, GPIO_IN);
        out = mkBool(gpio_get(gp));
        return 0;
    }
    case N_PIN_PULLUP:
    {
        uint gp = (uint)valNum(&args[0]);
        if (gp > 29)
        {
            setErr(line, "gpio 无效");
            return -1;
        }
        gpio_init(gp);
        if (truthy(&args[1]))
            gpio_pull_up(gp);
        else
            gpio_disable_pulls(gp);
        out = g_none;
        return 0;
    }
    case N_PWM_INIT:
    {
        uint gp = (uint)valNum(&args[0]);
        double freq = valNum(&args[1]);
        double duty = valNum(&args[2]);
        if (gp > 29 || freq <= 0.0)
        {
            setErr(line, "pwm 参数无效");
            return -1;
        }
        if (duty < 0.0)
            duty = 0.0;
        if (duty > 100.0)
            duty = 100.0;
        gpio_set_function(gp, GPIO_FUNC_PWM);
        uint slice = pwm_gpio_to_slice_num(gp);
        double div = 1.0;
        uint32_t top = 0;
        for (int k = 0; k < 256; ++k)
        {
            top = (uint32_t)(125.0e6 / (div * freq));
            if (top >= 1 && top <= 0xFFFF)
                break;
            if (top > 0xFFFF && div < 250.0)
                div += 1.0;
            else
                break;
        }
        if (top < 1)
            top = 1;
        if (top > 0xFFFF)
            top = 0xFFFF;
        pwm_set_clkdiv(slice, div);
        pwm_set_wrap(slice, top);
        pwm_set_chan_level(slice, pwm_gpio_to_channel(gp), (uint32_t)((double)top * duty / 100.0));
        pwm_set_enabled(slice, true);
        out = g_none;
        return 0;
    }
    case N_PWM_STOP:
    {
        uint gp = (uint)valNum(&args[0]);
        if (gp > 29)
        {
            setErr(line, "gpio 无效");
            return -1;
        }
        uint slice = pwm_gpio_to_slice_num(gp);
        pwm_set_chan_level(slice, pwm_gpio_to_channel(gp), 0);
        pwm_set_enabled(slice, false);
        setPinOut(gp, false);
        out = g_none;
        return 0;
    }
    case N_PULLUP_EN:
    {
        gpio_init(PULLUP_EN_PIN);
        gpio_set_dir(PULLUP_EN_PIN, GPIO_OUT);
        gpio_put(PULLUP_EN_PIN, truthy(&args[0]) ? 1 : 0); // 1=开上拉 0=关
        out = g_none;
        return 0;
    }
    case N_TA_SWITCH:
    case N_TB_SWITCH:
    {
        uint gp = (id == N_TA_SWITCH) ? (uint)RP_TD : (uint)RP_RC;
        bool v = truthy(&args[0]);
        setPinOut(gp, v);
        setPinOut(LEDTATBPIN, v);
        out = g_none;
        return 0;
    }
    case N_TA_PWM:
    case N_TB_PWM:
    {
        uint gp = (id == N_TA_PWM) ? (uint)RP_TD : (uint)RP_RC;
        MVal tmp[3];
        tmp[0] = mkInt(gp);
        tmp[1] = args[0];
        tmp[2] = args[1];
        return doNative(N_PWM_INIT, tmp, 3, out, line);
    }
    case N_TA_PULLUP:
    case N_TB_PULLUP:
    {
        uint gp = (id == N_TA_PULLUP) ? (uint)RP_TD : (uint)RP_RC;
        bool v = truthy(&args[0]);
        gpio_init(gp);
        if (v)
            gpio_pull_up(gp);
        else
            gpio_disable_pulls(gp);
        gpio_init(PULLUP_EN_PIN);
        gpio_set_dir(PULLUP_EN_PIN, GPIO_OUT);
        gpio_put(PULLUP_EN_PIN, v ? 1 : 0);
        out = g_none;
        return 0;
    }
    case N_TC_PUSH:
    {
        bool v = truthy(&args[0]);
        setPinOut(TCPWMOUT, v);
        setPinOut(LEDTCPIN, v);
        out = g_none;
        return 0;
    }
    case N_TC_OD:
    case N_TD_OD:
    {
        uint gp = (id == N_TC_OD) ? (uint)TCPWMOUT : (uint)TDPWMOUT;
        uint led = (id == N_TC_OD) ? (uint)LEDTCPIN : (uint)LEDTDPIN;
        bool hi = truthy(&args[0]);
        gpio_init(gp);
        if (hi)
        {
            gpio_set_dir(gp, GPIO_IN);
            gpio_disable_pulls(gp);
            if (id == N_TD_OD)
                gpio_pull_up(gp);
        }
        else
        {
            gpio_set_dir(gp, GPIO_OUT);
            gpio_put(gp, 0);
        }
        setPinOut(led, !hi);
        out = g_none;
        return 0;
    }
    case N_TE_TX:
    case N_TF_SCK:
    {
        uint gp = (id == N_TE_TX) ? (uint)TEST_SPI_TX_PIN : (uint)TEST_SPI_SCK_PIN;
        bool v = truthy(&args[0]);
        gpio_init(gp);
        gpio_pull_up(gp);
        gpio_set_dir(gp, GPIO_OUT);
        gpio_put(gp, v ? 1 : 0);
        out = g_none;
        return 0;
    }
    case N_LED_TA_TB:
    case N_LED_TC:
    case N_LED_TD:
    {
        uint led = (id == N_LED_TA_TB) ? (uint)LEDTATBPIN : (id == N_LED_TC) ? (uint)LEDTCPIN : (uint)LEDTDPIN;
        setPinOut(led, truthy(&args[0]));
        out = g_none;
        return 0;
    }
    default:
        setErr(line, "内置函数未实现");
        return -1;
    }
}

// ================= 表达式求值 =================
static bool evalNode(int idx, MVal &out)
{
    if (idx < 0)
    {
        out = g_none;
        return true;
    }
    const MNode *nd = &s_pool[idx];
    s_curLine = nd->line;

    switch (nd->kind)
    {
    case N_NUM:
        if (nd->f == (double)(long long)nd->f)
            out = mkInt((long long)nd->f);
        else
            out = mkFlt(nd->f);
        return true;
    case N_STR:
        out = mkStr(nd->s ? nd->s : "");
        return true;
    case N_BOOL:
        out = mkBool(nd->n0 != 0);
        return true;
    case N_NONE:
        out = g_none;
        return true;
    case N_NAME:
        return getBind(nd->s, out);
    case N_LIST:
    {
        MList *l = listNew();
        if (!l)
        {
            setErr(nd->line, "内存不足");
            return false;
        }
        int li = idx;
        for (int k = 0; k < s_blk[li].n; ++k)
        {
            MVal v;
            if (!evalNode(s_blk[li].d[k], v))
                return false;
            if (!listPush(l, v))
            {
                setErr(nd->line, "内存不足");
                return false;
            }
        }
        out = mkList(l);
        return true;
    }
    case N_INDEX:
    {
        MVal base, k;
        if (!evalNode(nd->n0, base) || !evalNode(nd->n1, k))
            return false;
        if (base.t != V_LIST)
        {
            setErr(nd->line, "只能对列表取下标");
            return false;
        }
        MList *l = (MList *)base.p;
        long long ii = (long long)valNum(&k);
        if (ii < 0)
            ii += l->len;
        if (ii < 0 || ii >= l->len)
        {
            setErr(nd->line, "列表下标越界");
            return false;
        }
        out = l->data[ii];
        return true;
    }
    case N_UNARY:
    {
        MVal v;
        if (!evalNode(nd->n1, v))
            return false;
        if (nd->n0 == 1)
        {
            out = (v.t == V_FLT) ? mkFlt(+v.f) : mkInt(+v.i);
            return true;
        }
        if (nd->n0 == 2)
        {
            if (v.t == V_FLT)
                out = mkFlt(-v.f);
            else if (v.t == V_INT)
                out = mkInt(-v.i);
            else if (v.t == V_BOOL)
                out = mkInt(v.i ? -1 : 0);
            else
            {
                setErr(nd->line, "一元 - 需数值");
                return false;
            }
            return true;
        }
        out = mkBool(!truthy(&v));
        return true;
    }
    case N_BIN:
    {
        MVal l, r;
        if (!evalNode(nd->n1, l))
            return false;
        int op = nd->n0;
        if (op == 20)
        {
            if (!truthy(&l))
            {
                out = l;
                return true;
            }
            return evalNode(nd->n2, out);
        }
        if (op == 21)
        {
            if (truthy(&l))
            {
                out = l;
                return true;
            }
            return evalNode(nd->n2, out);
        }
        if (!evalNode(nd->n2, r))
            return false;

        if (op >= 10 && op <= 15)
        {
            bool eq = false;
            if (l.t == V_STR && r.t == V_STR)
            {
                int c = strcmp(l.s ? l.s : "", r.s ? r.s : "");
                switch (op)
                {
                case 10: eq = (c == 0); break;
                case 11: eq = (c != 0); break;
                case 12: eq = (c < 0); break;
                case 13: eq = (c <= 0); break;
                case 14: eq = (c > 0); break;
                default: eq = (c >= 0); break;
                }
            }
            else if (l.t == V_LIST && r.t == V_LIST)
            {
                int an = ((MList *)l.p) ? ((MList *)l.p)->len : 0;
                int bn = ((MList *)r.p) ? ((MList *)r.p)->len : 0;
                switch (op)
                {
                case 10: eq = (an == bn); break;
                case 11: eq = (an != bn); break;
                case 12: eq = (an < bn); break;
                case 13: eq = (an <= bn); break;
                case 14: eq = (an > bn); break;
                default: eq = (an >= bn); break;
                }
            }
            else
            {
                double a = valNum(&l), b = valNum(&r);
                switch (op)
                {
                case 10: eq = (a == b); break;
                case 11: eq = (a != b); break;
                case 12: eq = (a < b); break;
                case 13: eq = (a <= b); break;
                case 14: eq = (a > b); break;
                default: eq = (a >= b); break;
                }
            }
            out = mkBool(eq);
            return true;
        }

        if (op == 1 && (l.t == V_STR || r.t == V_STR))
        {
            if (l.t != V_STR || r.t != V_STR)
            {
                setErr(nd->line, "字符串只能与字符串拼接");
                return false;
            }
            size_t al = strlen(l.s), bl = strlen(r.s);
            char *res = (char *)arenaAlloc(al + bl + 1);
            if (!res)
            {
                setErr(nd->line, "内存不足");
                return false;
            }
            memcpy(res, l.s, al);
            memcpy(res + al, r.s, bl);
            res[al + bl] = '\0';
            out = mkStr(res);
            return true;
        }

        if (l.t == V_FLT || r.t == V_FLT)
        {
            double a = valNum(&l), b = valNum(&r), res = 0;
            switch (op)
            {
            case 1: res = a + b; break;
            case 2: res = a - b; break;
            case 3: res = a / b; break;
            case 4: res = floor(a / b); break;
            case 5: res = fmod(a, b); break;
            default: res = pow(a, b); break;
            }
            out = mkFlt(res);
            return true;
        }

        long long a = (long long)valNum(&l), b = (long long)valNum(&r);
        switch (op)
        {
        case 1: out = mkInt(a + b); return true;
        case 2: out = mkInt(a - b); return true;
        case 3:
            if (b == 0)
            {
                setErr(nd->line, "除数为 0");
                return false;
            }
            out = mkFlt((double)a / (double)b);
            return true;
        case 4:
            if (b == 0)
            {
                setErr(nd->line, "除数为 0");
                return false;
            }
            out = mkInt(a / b);
            return true;
        case 5:
            if (b == 0)
            {
                setErr(nd->line, "取模除数为 0");
                return false;
            }
            out = mkInt(a % b);
            return true;
        default:
            out = mkInt((long long)pow((double)a, (double)b));
            return true;
        }
    }
    case N_CALL:
    {
        const MNode *fn = &s_pool[nd->n0];
        if (fn->kind != N_NAME)
        {
            setErr(nd->line, "仅支持直接函数调用");
            return false;
        }
        const char *fname = fn->s;
        int nat = -1;
        for (int k = 0; k < N_NAT_COUNT; ++k)
            if (strcmp(NAT_NAMES[k], fname) == 0)
            {
                nat = k;
                break;
            }
        int ci = idx;
        int argn = s_blk[ci].n;
        if (argn > MP_MAX_NAT_ARGS)
        {
            setErr(nd->line, "参数过多");
            return false;
        }
        MVal args[MP_MAX_NAT_ARGS];
        for (int i = 0; i < argn; ++i)
            if (!evalNode(s_blk[ci].d[i], args[i]))
                return false;

        if (nat >= 0)
            return doNative(nat, args, argn, out, nd->line) == 0;

        MVal fv;
        if (!getBind(fname, fv) || fv.t != V_FN)
        {
            setErr(nd->line, "未定义函数 '%s'", fname);
            return false;
        }
        const MNode *def = &s_pool[fv.node];
        int paramCnt = def->n1 - def->n0;
        if (argn != paramCnt)
        {
            setErr(nd->line, "函数参数个数不符");
            return false;
        }
        if (s_callDepth >= MAX_CALL_DEPTH)
        {
            setErr(nd->line, "递归/嵌套调用过深");
            return false;
        }
        s_callDepth++;
        // 简化帧：参数在扁平命名空间中临时绑定并在调用后移除（不支持遮蔽/递归）。
        for (int i = 0; i < paramCnt; ++i)
        {
            const MNode *pn = &s_pool[def->n0 + i];
            if (!setBind(pn->s, args[i]))
            {
                setErr(nd->line, "符号表满");
                s_callDepth--;
                return false;
            }
        }
        s_retFlag = 0;
        int flow = execNode(def->n2);
        s_callDepth--;
        // 还原/清理形参（保留函数内新建的其它局部变量——扁平化限制）
        for (int i = 0; i < paramCnt; ++i)
        {
            const MNode *pn = &s_pool[def->n0 + i];
            int idx = findBind(pn->s);
            if (idx >= 0)
            {
                // 还原为未定义：移除
                s_vals[idx] = s_vals[s_bindCnt - 1];
                s_names[idx] = s_names[s_bindCnt - 1];
                s_bindCnt--;
            }
        }
        if (flow == F_ERR)
            return false;
        out = (flow == F_RET && s_retFlag) ? s_retVal : g_none;
        return true;
    }
    default:
        setErr(nd->line, "内部错误：非法表达式节点");
        return false;
    }
}

// ================= 语句执行 =================
static int execNode(int idx)
{
    if (idx < 0)
        return F_NONE;
    const MNode *nd = &s_pool[idx];
    s_curLine = nd->line;
    // 执行行号发生变化 → 触发行回调（供运行指示灯：每执行到新一行亮一次）
    if (nd->line != s_lastCbLine)
    {
        s_lastCbLine = nd->line;
        if (s_lineCb)
            s_lineCb(nd->line);
    }

    switch (nd->kind)
    {
    case N_BLOCK:
    {
        int bi = idx;
        for (int k = 0; k < s_blk[bi].n; ++k)
        {
            if (s_stopReq)
                return F_NONE;
            int fl = execNode(s_blk[bi].d[k]);
            if (fl != F_NONE)
                return fl;
        }
        return F_NONE;
    }
    case N_EXPR:
    {
        MVal v;
        return evalNode(nd->n0, v) ? F_NONE : F_ERR;
    }
    case N_ASSIGN:
    {
        MVal v;
        if (!evalNode(nd->n1, v))
            return F_ERR;
        const MNode *tgt = &s_pool[nd->n0];
        if (tgt->kind == N_NAME)
        {
            if (!setBind(tgt->s, v))
            {
                setErr(nd->line, "符号表满");
                return F_ERR;
            }
            return F_NONE;
        }
        if (tgt->kind == N_INDEX)
        {
            MVal base, k;
            if (!evalNode(tgt->n0, base) || !evalNode(tgt->n1, k))
                return F_ERR;
            if (base.t != V_LIST)
            {
                setErr(nd->line, "只能对列表元素赋值");
                return F_ERR;
            }
            MList *l = (MList *)base.p;
            long long ii = (long long)valNum(&k);
            if (ii < 0)
                ii += l->len;
            if (ii < 0 || ii >= l->len)
            {
                setErr(nd->line, "列表下标越界");
                return F_ERR;
            }
            l->data[ii] = v;
            return F_NONE;
        }
        setErr(nd->line, "赋值目标无效");
        return F_ERR;
    }
    case N_PASS:
        return F_NONE;
    case N_BREAK:
        return F_BREAK;
    case N_CONTINUE:
        return F_CONT;
    case N_RETURN:
    {
        if (nd->n0 >= 0)
        {
            if (!evalNode(nd->n0, s_retVal))
                return F_ERR;
        }
        else
            s_retVal = g_none;
        s_retFlag = 1;
        return F_RET;
    }
    case N_IF:
    {
        MVal c;
        if (!evalNode(nd->n0, c))
            return F_ERR;
        int blk = truthy(&c) ? nd->n1 : nd->n2;
        if (blk >= 0)
            return execNode(blk);
        return F_NONE;
    }
    case N_WHILE:
    {
        for (;;)
        {
            if (s_stopReq)
                return F_NONE;
            if (s_hb && s_hb())
            {
                s_stopReq = true;
                return F_NONE;
            }
            MVal c;
            if (!evalNode(nd->n0, c))
                return F_ERR;
            if (!truthy(&c))
                return F_NONE;
            int fl = execNode(nd->n1);
            if (fl == F_ERR)
                return F_ERR;
            if (fl == F_BREAK)
                return F_NONE;
        }
    }
    case N_FOR:
    {
        MVal seq;
        if (!evalNode(nd->n0, seq))
            return F_ERR;
        if (seq.t != V_LIST)
        {
            setErr(nd->line, "for 只能遍历 range()/列表");
            return F_ERR;
        }
        MList *l = (MList *)seq.p;
        const char *vn = nd->s;
        for (int i = 0; i < (l ? l->len : 0); ++i)
        {
            if (s_stopReq)
                return F_NONE;
            if (s_hb && s_hb())
            {
                s_stopReq = true;
                return F_NONE;
            }
            if (!setBind(vn, l->data[i]))
            {
                setErr(nd->line, "符号表满");
                return F_ERR;
            }
            int fl = execNode(nd->n1);
            if (fl == F_ERR)
                return F_ERR;
            if (fl == F_BREAK)
                return F_NONE;
        }
        return F_NONE;
    }
    case N_DEF:
    {
        MVal fv;
        fv.t = V_FN;
        fv.node = idx;
        if (!setBind(nd->s, fv))
        {
            setErr(nd->line, "符号表满");
            return F_ERR;
        }
        return F_NONE;
    }
    default:
        setErr(nd->line, "内部错误：非法语句节点");
        return F_ERR;
    }
}

// ================= 公共接口 =================
static bool s_allocated = false;

void mp_init(void)
{
    if (s_allocated)
        return;
    if (!s_pool)
        s_pool = (MNode *)malloc((size_t)MP_POOL_CAP * sizeof(MNode));
    if (!s_toks)
        s_toks = (MTok *)malloc((size_t)MP_TOK_CAP * sizeof(MTok));
    if (!s_names)
        s_names = (const char **)malloc((size_t)MP_BIND_CAP * sizeof(char *));
    if (!s_vals)
        s_vals = (MVal *)malloc((size_t)MP_BIND_CAP * sizeof(MVal));
    if (!s_arena)
        s_arena = (unsigned char *)malloc(MP_ARENA_SIZE);
    if (!s_blk)
        s_blk = (BlkList *)malloc((size_t)MP_POOL_CAP * sizeof(BlkList));
    if (!s_pool || !s_toks || !s_names || !s_vals || !s_arena || !s_blk)
    {
        // 任一分配失败：全部释放，避免残留；下一次 RUN 再试
        free(s_pool);
        s_pool = nullptr;
        free(s_toks);
        s_toks = nullptr;
        free(s_names);
        s_names = nullptr;
        free(s_vals);
        s_vals = nullptr;
        free(s_arena);
        s_arena = nullptr;
        free(s_blk);
        s_blk = nullptr;
        s_errLine = 1;
        snprintf(s_errMsg, sizeof(s_errMsg), "解释器内存不足");
        return;
    }
    s_allocated = true;
}

void mp_free(void)
{
    free(s_pool);
    s_pool = nullptr;
    free(s_toks);
    s_toks = nullptr;
    free(s_names);
    s_names = nullptr;
    free(s_vals);
    s_vals = nullptr;
    free(s_arena);
    s_arena = nullptr;
    free(s_blk);
    s_blk = nullptr;
    s_allocated = false;
}

bool mp_load(const char *src)
{
    if (!s_allocated)
        mp_init();
    if (!s_pool || !s_toks || !s_names || !s_vals || !s_arena)
    {
        setErr(1, "解释器未就绪");
        return false;
    }
    s_arenaTop = 0;
    s_poolLen = 0;
    s_tokLen = 0;
    s_bindCnt = 0;
    s_errLine = 0;
    s_errMsg[0] = '\0';
    s_stopReq = false;
    s_curLine = 0;
    s_retFlag = 0;
    for (int i = 0; i < 4; ++i)
        s_disp[i][0] = '\0';
    s_dispUsed = 0;
    s_callDepth = 0;
    s_srcLines = nullptr;

    if (!src || !src[0])
    {
        setErr(1, "空脚本");
        return false;
    }
    // 行结束归一化：兼容 \n / \r\n / \r（部分串口工具只发 \r）
    const char *toParse = src;
    char *normBuf = nullptr;
    const char *rp;
    for (rp = src; *rp; ++rp)
        if (*rp == '\r')
            break;
    if (*rp == '\r')
    {
        size_t len = strlen(src);
        normBuf = (char *)malloc(len + 1);
        if (!normBuf)
        {
            setErr(1, "内存不足");
            return false;
        }
        size_t oi = 0;
        const char *sp = src;
        while (*sp)
        {
            if (*sp == '\r')
            {
                if (sp[1] == '\n')
                    sp++;
                normBuf[oi++] = '\n';
            }
            else
                normBuf[oi++] = *sp;
            sp++;
        }
        normBuf[oi] = '\0';
        toParse = normBuf;
    }
    bool tokOk = tokenize(toParse);
    if (normBuf)
        free(normBuf);
    if (!tokOk)
        return false;

    // 根块 = 节点 0
    s_poolLen = 1;
    s_pool[0].kind = N_BLOCK;
    s_pool[0].line = 1;
    s_pool[0].n0 = -1;
    s_pool[0].n1 = -1;
    s_pool[0].n2 = -1;
    s_pool[0].n3 = -1;
    s_pool[0].f = 0;
    s_pool[0].s = nullptr;
    // 清空所有块的语句表（arena 已重置，指针需重建）
    if (s_blk)
    {
        for (int i = 0; i < MP_POOL_CAP; ++i)
        {
            s_blk[i].d = nullptr;
            s_blk[i].n = 0;
            s_blk[i].cap = 0;
        }
    }
    if (blkInitList(0) < 0)
    {
        setErr(1, "内存不足");
        return false;
    }
    s_ti = 0;
    while (s_toks[s_ti].type != T_END)
    {
        while (s_toks[s_ti].type == T_NEWLINE)
            s_ti++;
        if (s_toks[s_ti].type == T_END)
            break;
        int st = parseStmt();
        if (st < 0)
        {
            if (s_errMsg[0] == '\0')
                setErr(s_toks[s_ti].line, "语法错误");
            return false;
        }
        if (st > 0 && blkAdd(0, st) < 0)
        {
            setErr(s_toks[s_ti].line, "内存不足");
            return false;
        }
    }
    // 保留源副本供错误行回溯（arena 在本轮 load 周期内有效）
    s_srcLines = arenaDup(toParse);
    if (!s_srcLines)
        s_srcLines = src;
    return true;
}

int mp_execute(void)
{
    if (s_poolLen == 0)
    {
        setErr(0, "未载入脚本");
        return MP_ERROR;
    }
    s_stopReq = false;
    s_lastCbLine = 0; // 每次执行都从首行触发（行1 != 0）
    int flow = execNode(0);
    if (flow == F_ERR)
        return MP_ERROR;
    if (s_stopReq)
        return MP_STOPPED;
    return MP_DONE;
}

void mp_request_stop(void) { s_stopReq = true; }
void mp_set_heartbeat(MpHeartbeatFn cb) { s_hb = cb; }
void mp_set_printcb(MpPrintFn cb) { s_printCb = cb; }
void mp_set_linecb(MpLineFn cb) { s_lineCb = cb; s_lastCbLine = 0; }

int mp_error_line(void) { return s_errLine; }
const char *mp_error_msg(void) { return s_errMsg; }
int mp_cur_line(void) { return s_curLine; }

int mp_source_line_text(int line, char *out, size_t cap)
{
    if (!out || cap == 0)
        return -1;
    out[0] = '\0';
    if (line < 1 || !s_srcLines)
        return -1;
    const char *p = s_srcLines;
    int cur = 1;
    while (*p)
    {
        if (cur == line)
        {
            size_t oi = 0;
            while (*p && *p != '\n' && oi + 1 < cap)
                out[oi++] = *p++;
            out[oi] = '\0';
            return 0;
        }
        while (*p && *p != '\n')
            p++;
        if (*p == '\n')
            p++;
        cur++;
    }
    return -1;
}

int mp_disp_lines(void) { return s_dispUsed; }
const char *mp_disp_line(int idx)
{
    if (idx < 0 || idx >= 4)
        return "";
    return s_disp[idx];
}
