/*
 * AI 测试模式（M1+M2：骨架 + 收发/Flash/AIT 上报）
 * docs/AI测试模式设计方案.md v0.3
 *
 * 说明：
 * - 本模式由 core0 运行；进入时 core1 让出 stdin（ai_owns_stdin_flag）。
 * - core0 自建 #PIVOUSCO#..#PRS#（SYS,M / AIC,..）与 #PIVOAIUP#..#ENDAIC# 接收机。
 * - 脚本解释器（M3）未接入：RUN 当前仅提示“需 M3 解释器”，
 *   载入/保存/读取/清除/INFO/状态上报均已可用。
 */
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/stdio_usb.h"
#include "pico/multicore.h"
#include "CONFIG_FLO.hpp"
#include "menu/ssd1306.hpp"
#include "menu/UI.hpp"
#include "menu/key.hpp"
#include "menu/flash.hpp"
#include "menu/core.hpp" // core1_main
#include "menu/StateLED.hpp" // 运行指示灯 StartStateLED
#include "read/functions.hpp" // nowselect_function / Enable_PC_Interface
#include "read/aitest.hpp"
#include "read/mp_runtime.hpp"
#include "Fonts/fonts.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

// ---------------- 全局（跨文件） ----------------
bool ai_infunction = false;
bool ai_owns_stdin_flag = false;

bool ai_owns_stdin(void) { return ai_owns_stdin_flag; }

// ---------------- 内部状态 ----------------
static AiRunState s_state = AI_STATE_IDLE;
static char s_code[AI_CODE_MAX_LEN + 1] = {0}; // 当前缓冲脚本
static uint16_t s_code_len = 0;                 // 缓冲长度
static uint16_t s_run_line = 0;                 // 当前执行行（解释器 M3 用）
static bool s_saved = false;                    // 是否与 Flash 中一致
static char s_msg[64] = {0};                    // OLED/INFO 提示
static volatile bool s_report_pending = false;  // 有变化需上报
static bool s_vm_exit_mode = false;             // 运行中被“切模式”键终止
static uint32_t s_vm_last_draw = 0;
static bool s_run_finished = false;             // 本轮正常“运行结束”→ 任意键可重跑

static void ai_set_state(AiRunState st, const char *msg)
{
    s_state = st;
    if (msg)
    {
        strncpy(s_msg, msg, sizeof(s_msg) - 1);
        s_msg[sizeof(s_msg) - 1] = '\0';
    }
    s_report_pending = true;
    // 方案A：#PIVOAIOX# 只承载脚本 print()；系统状态文字不再自动回串口
}

// 出错：OLED 简洁显示“运行错误：L<行>”；完整行号+原因经 #PIVOAIER# 帧上报上位机（不污染 print 通道）
static void ai_set_error(int line, const char *reason)
{
    if (!reason)
        reason = "";
    char disp[24];
    snprintf(disp, sizeof(disp), "运行错误：L%d", line);
    s_run_line = (uint16_t)line;
    ai_set_state(AI_STATE_ERROR, disp);
    if (stdio_usb_connected())
        printf("#PIVOAIER#L%d:%s#PTS#\r\n", line, reason);
}

// ---------------- 上报（core0 直接打印，节流由调用方控制） ----------------
static void ai_send_ait(void)
{
    if (!stdio_usb_connected())
        return; // AI 模式自带帧，不依赖 Enable_PC_Interface 握手
    printf("#PIVOUSCO#AIT,%s,%u,%u,%u#PTS#\r\n",
           ai_state_str(), (unsigned)s_code_len, (unsigned)s_run_line, s_saved ? 1u : 0u);
}

static void ai_send_aio(const char *text)
{
    if (!text || !stdio_usb_connected())
        return; // AI 模式自带帧，不依赖 Enable_PC_Interface 握手
    printf("#PIVOAIOX#%s#PTS#\r\n", text);
}

// 把当前内存脚本整包回传上位机（#PIVOAIUP#..#ENDAIC#），供“从 Flash 载入”后回显
static void ai_send_ai_code(void)
{
    if (!stdio_usb_connected() || s_code_len == 0)
        return;
    printf("#PIVOAIUP#%s#ENDAIC#\r\n", s_code);
}

// ---------------- 查询（供 core.cpp 上报） ----------------
const char *ai_state_str(void)
{
    switch (s_state)
    {
    case AI_STATE_IDLE:    return "IDLE";
    case AI_STATE_READY:   return "READY";
    case AI_STATE_RUNNING: return "RUNNING";
    case AI_STATE_STOPPED: return "STOP";
    case AI_STATE_ERROR:   return "ERROR";
    default:               return "?";
    }
}
AiRunState ai_state(void) { return s_state; }
uint16_t ai_code_len(void) { return s_code_len; }
uint16_t ai_run_line(void) { return s_run_line; }
bool ai_code_saved(void) { return s_saved; }

// ---------------- 控制 ----------------
// 上位机打断运行【统一入口】：AIC,STOP。
// 预留：未来上位机 AI 页的“停止”按钮 = 发 #PIVOUSCO#AIC,STOP#PRS#，即走本函数。
// 说明：运行中解释器在心跳里轮询 ai_poll_host()，收到即置 s_stopReq，
// 解释器在最近检查点（循环迭代顶 / sleep_ms 每 2ms / 每条语句前）退出。
void ai_request_stop(void)
{
    mp_request_stop(); // 让解释器尽快退出（若在运行）
    if (s_state == AI_STATE_RUNNING)
        s_run_line = 0;
    ai_set_state(AI_STATE_STOPPED, "已停止");
}

// 载入整包（#PIVOAIUP#..#ENDAIC#）：替换缓冲
void ai_handle_upload_payload(const char *payload, size_t len)
{
    if (!payload)
        return;
    if (len > AI_CODE_MAX_LEN)
        len = AI_CODE_MAX_LEN;
    memcpy(s_code, payload, len);
    s_code[len] = '\0';
    s_code_len = (uint16_t)len;
    s_run_line = 0;
    s_saved = false; // 缓冲已变化，未保存
    char tmp[24];
    snprintf(tmp, sizeof(tmp), "已载入 %u 字节", (unsigned)len);
    ai_set_state(AI_STATE_READY, tmp);
}

// 运行（解释器 M3：真正执行已载入脚本）
static void ai_draw(void);
static void ai_poll_host(void);
static void ai_send_ait(void);

// 行回调：脚本每执行到新一行触发一次 → 运行指示灯亮一下
// （PWM 亮度由 10ms 定时器中断里的 stateLED() 状态机驱动，与其它功能一致）
static void ai_vm_line(int line)
{
    (void)line;
    StartStateLED();
}

// 心跳：解释器每语句/迭代回调 —— 渲染、轮询上位机、检测停止
static bool ai_vm_heartbeat(void)
{
    extern bool opnLeft, opnRight, opnPCchangemode;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    s_run_line = (uint16_t)mp_cur_line();
    if (now - s_vm_last_draw >= 100) // 运行期整屏刷新/上报 10Hz（print 仅更新内容，随心跳整屏刷出）
    {
        s_vm_last_draw = now;
        ai_draw();
        ai_send_ait();
    }
    // 通信电平(#PIVOSEVO#)不实时上报：与输入状态检测/UART 调试一致，由上位机按是否收到帧判定为非实时。
    ai_poll_host();
    // 运行期间：仅“切换模式”键（左/右/上位机切模式）让系统退出；OK/上/下/Cancel 一律不做
    // 系统反应——按键状态全部交由 AI 程序用 key() 轮询；停止交给脚本自身或上位机 AIC,STOP。
    if (opnLeft || opnRight || opnPCchangemode)
    {
        s_vm_exit_mode = true; // 通知上层：结束运行去切模式
        return true;
    }
    return false;
}

static void ai_start_run(void)
{
    extern bool opnEnter, opnExit, opnUp, opnDown;
    // 开始执行：清空四个按键的锁存消费状态（防空闲期残留键影响本轮；运行期按键由脚本 key() 读取）
    opnEnter = opnExit = opnUp = opnDown = false;
    if (s_code_len == 0)
    {
        ai_set_state(AI_STATE_IDLE, "无代码");
        return;
    }
    if (s_state == AI_STATE_RUNNING)
        return; // 已在运行（AIT 已表达 RUNNING）

    s_run_finished = false;
    if (!mp_load(s_code))
    {
        ai_set_error(mp_error_line(), mp_error_msg());
        return;
    }
    mp_set_heartbeat(ai_vm_heartbeat);
    mp_set_linecb(ai_vm_line); // 运行指示灯：执行行号变化时触发亮一次
    s_vm_exit_mode = false;
    s_vm_last_draw = 0;
    s_run_line = 0;
    ai_set_state(AI_STATE_RUNNING, "运行中");

    int r = mp_execute();
    mp_set_heartbeat(nullptr);
    mp_set_linecb(nullptr); // 运行结束停止行回调

    if (r == MP_DONE)
    {
        s_run_line = 0;
        s_run_finished = true; // 正常结束 → 主循环按键可重跑
        ai_set_state(AI_STATE_STOPPED, s_vm_exit_mode ? "已停止" : "运行结束");
    }
    else if (r == MP_STOPPED)
    {
        s_run_line = (uint16_t)mp_cur_line();
        ai_set_state(AI_STATE_STOPPED, s_vm_exit_mode ? "退出运行" : "已停止");
    }
    else
    {
        ai_set_error(mp_error_line(), mp_error_msg());
    }
    // 每次执行结束：清空四个按键的锁存消费状态（保留左/右切模式键供等待循环判断退出）
    opnEnter = opnExit = opnUp = opnDown = false;
}

// AIC,<sub> 命令解析（载荷可能带 "AIC," 前缀，如 "#PIVOUSCO#AIC,RUN#PRS#"）
void ai_process_command(const char *sub)
{
    if (!sub)
        return;
    if (strncmp(sub, "AIC,", 4) == 0)
        sub += 4; // 剥离去前缀，兼容 "AIC,RUN" 与 "RUN"

    if (strcmp(sub, "RUN") == 0 || strcmp(sub, "run") == 0)
    {
        ai_start_run();
    }
    else if (strcmp(sub, "STOP") == 0 || strcmp(sub, "stop") == 0)
    {
        ai_request_stop(); // 上位机打断运行（预留：上位机页“停止”按钮发本帧）
    }
    else if (strcmp(sub, "SAVE") == 0 || strcmp(sub, "save") == 0)
    {
        if (s_code_len == 0)
        {
            ai_set_state(AI_STATE_IDLE, "无代码可存");
            return;
        }
        // 写内部 Flash 前必须先停 core1（其仍在 XIP Flash 取指，会与写冲突导致卡死）
        multicore_reset_core1();
        save_ai_code(s_code);
        multicore_launch_core1(core1_main);
        s_saved = true;
        ai_set_state(AI_STATE_READY, "已保存到 Flash");
    }
    else if (strcmp(sub, "LOAD") == 0 || strcmp(sub, "load") == 0)
    {
        if (!has_ai_code())
        {
            ai_set_state(AI_STATE_IDLE, "Flash 无已存代码");
            return;
        }
        load_ai_code(s_code);
        s_code_len = (uint16_t)strlen(s_code);
        s_run_line = 0;
        s_saved = true;
        char tmp[24];
        snprintf(tmp, sizeof(tmp), "已读取 %u 字节", (unsigned)s_code_len);
        ai_set_state(AI_STATE_READY, tmp);
        ai_send_ai_code(); // 回传整包，让上位机“从Flash载入”能显示代码
    }
    else if (strcmp(sub, "ERASE") == 0 || strcmp(sub, "erase") == 0)
    {
        // 擦除同样写 Flash：停 core1 防卡死
        multicore_reset_core1();
        erase_ai_code();
        multicore_launch_core1(core1_main);
        s_code[0] = '\0';
        s_code_len = 0;
        s_saved = false;
        ai_set_state(AI_STATE_IDLE, "已清除已存脚本");
    }
    else if (strcmp(sub, "INFO") == 0 || strcmp(sub, "info") == 0)
    {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "state=%s,len=%u,saved=%u,line=%u",
                 ai_state_str(), (unsigned)s_code_len, s_saved ? 1u : 0u, (unsigned)s_run_line);
        ai_send_aio(tmp);
    }
}

// ---------------- core0 串口接收机（#PIVOUSCO# / #PIVOAIUP#） ----------------
static void ai_poll_host(void)
{
    enum
    {
        RX_IDLE,
        RX_USCO_DATA,
        RX_USCO_FOOT,
        RX_AIUP_DATA,
        RX_AIUP_FOOT
    };
    static uint8_t rx_mode = RX_IDLE;
    static size_t usco_idx = 0; // 帧头匹配进度（IDLE 态用）
    static size_t aiup_idx = 0;
    static char rx_buf[AI_CODE_MAX_LEN + 64];
    static size_t rx_len = 0;
    static size_t foot_idx = 0;

    const char *H_USCO = "#PIVOUSCO#";
    const char *F_USCO = "#PRS#";
    const char *H_AIUP = "#PIVOAIUP#";
    const char *F_AIUP = "#ENDAIC#";

    int c;
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT)
    {
        char ch = (char)c;

        if (rx_mode == RX_IDLE)
        {
            // 同时尝试匹配两个帧头
            bool is_usco = false, is_aiup = false;
            if (ch == H_USCO[usco_idx]) { usco_idx++; if (H_USCO[usco_idx] == '\0') { usco_idx = 0; is_usco = true; } }
            else usco_idx = (ch == H_USCO[0]) ? 1 : 0;
            if (ch == H_AIUP[aiup_idx]) { aiup_idx++; if (H_AIUP[aiup_idx] == '\0') { aiup_idx = 0; is_aiup = true; } }
            else aiup_idx = (ch == H_AIUP[0]) ? 1 : 0;

            if (is_usco)
            {
                aiup_idx = 0;
                rx_mode = RX_USCO_DATA;
                rx_len = 0;
                foot_idx = 0;
                continue;
            }
            if (is_aiup)
            {
                usco_idx = 0;
                rx_mode = RX_AIUP_DATA;
                rx_len = 0;
                foot_idx = 0;
                continue;
            }
            continue;
        }

        const char *footer = (rx_mode == RX_USCO_DATA || rx_mode == RX_USCO_FOOT) ? F_USCO : F_AIUP;

        if (rx_mode == RX_USCO_DATA || rx_mode == RX_AIUP_DATA)
        {
            if (ch == footer[0])
            {
                rx_mode = (rx_mode == RX_USCO_DATA) ? RX_USCO_FOOT : RX_AIUP_FOOT;
                foot_idx = 1;
                continue;
            }
            if (rx_len + 1 < sizeof(rx_buf))
            {
                rx_buf[rx_len++] = ch;
            }
            continue;
        }

        // footer 匹配中
        if (ch == footer[foot_idx])
        {
            foot_idx++;
            if (footer[foot_idx] == '\0')
            {
                rx_buf[rx_len] = '\0';
                if (rx_mode == RX_USCO_FOOT)
                {
                    // #PIVOUSCO# 载荷：SYS,M,x / SYS,C 或 AIC,..
                    if (strncmp(rx_buf, "SYS,M,", 6) == 0)
                    {
                        extern bool opnPCchangemode;
                        nowselect_function = (uint16_t)atoi(rx_buf + 6);
                        opnPCchangemode = 1; // 让 aitest_main 退出 → main 重选模式
                    }
                    else if (strcmp(rx_buf, "SYS,C") == 0)
                    {
                        // 刷新/重连握手：AI 独占 stdin 后 core1 让出读取，需 AI 自行应答。
                        // 与 core1 处理一致：应答系统信息并触发“重新进入当前模式”（重载已存脚本）
                        extern void Pc_ReplySystemInfo(void);
                        Pc_ReplySystemInfo();
                    }
                    else
                    {
                        ai_process_command(rx_buf); // 兼容 AIC,.. 与后续其它命令
                    }
                }
                else // RX_AIUP_FOOT
                {
                    ai_handle_upload_payload(rx_buf, rx_len);
                }
                rx_mode = RX_IDLE;
                rx_len = 0;
                foot_idx = 0;
                usco_idx = 0;
                aiup_idx = 0;
            }
            continue;
        }
        else
        {
            // 回退已匹配的 footer 前缀字符到数据
            if (rx_len + foot_idx + 1 < sizeof(rx_buf))
            {
                for (size_t i = 0; i < foot_idx; ++i)
                    rx_buf[rx_len++] = footer[i];
                rx_buf[rx_len++] = ch;
            }
            foot_idx = 0;
            rx_mode = (rx_mode == RX_USCO_FOOT) ? RX_USCO_DATA : RX_AIUP_DATA;
        }
    }
}

// ---------------- OLED ----------------
// 按屏宽(128px)截断文本，避免超长/中文溢出被截断显示
// ASCII ≈7px，中文(≥3字节) ≈14px
static void clamp_to_screen(const char *in, char *out, size_t cap)
{
    if (!in || !out || cap == 0)
        return;
    int w = 0;
    size_t o = 0;
    while (*in && o + 8 < cap)
    {
        unsigned char c = (unsigned char)*in;
        if (c < 0x80)
        {
            if (w + 7 > 124)
                break;
            w += 7;
            out[o++] = *in++;
        }
        else
        {
            int len = (c >= 0xE0) ? 3 : 2;
            if (w + 14 > 124)
                break;
            w += 14;
            for (int k = 0; k < len && *in; ++k)
                out[o++] = *in++;
        }
    }
    out[o] = '\0';
}

static void ai_draw(void)
{
    char line[40];
    char txt[40];

    memset(buf, 0, SSD1306_BUF_LEN);

    // 行1：状态 / 是否已存
    snprintf(line, sizeof(line), "ST:%s SAV:%d", ai_state_str(), s_saved ? 1 : 0);
    Paint_DrawString_EN_CenterAtX(buf, SSD1306_WIDTH / 2, 2, line, &Font12, 1);

    // 行2：代码长度 / 当前行
    if (s_run_line)
        snprintf(line, sizeof(line), "code:%u L:%u", (unsigned)s_code_len, (unsigned)s_run_line);
    else
        snprintf(line, sizeof(line), "code:%u/%d", (unsigned)s_code_len, (int)AI_CODE_MAX_LEN);
    Paint_DrawString_EN_CenterAtX(buf, SSD1306_WIDTH / 2, 15, line, &Font12, 1);

    // 行3：当前状态
    clamp_to_screen(s_msg, txt, sizeof(txt));
    if (!txt[0])
        snprintf(txt, sizeof(txt), " ");
    Paint_DrawString_EN_CenterAtX(buf, SSD1306_WIDTH / 2, 30, txt, &Font12, 1);

    // 行4：方框（下/左/右各留 1px，高度调低）+ 最近一次 print()（无输出则为空白）
    int by = 44, bh = 19;
    DrawRectangle(buf, 1, by, SSD1306_WIDTH - 2, bh, 0, 1); // x:1..126，y:44..62
    clamp_to_screen(mp_disp_line(3), txt, sizeof(txt));
    Paint_DrawString_EN(buf, 3, by + 4, txt, &Font12, 1);

    render(buf, &frame_area);
}

void aitest_main(void)
{
    extern bool opnPCchangemode;
    extern void ResetOpn(void);

    ai_owns_stdin_flag = true;
    s_run_line = 0;
    s_saved = false;

    // 进入：载入已存脚本但【不自动运行】，提示“按下开始”，由按键触发运行
    if (has_ai_code())
    {
        load_ai_code(s_code);
        s_code_len = (uint16_t)strlen(s_code);
        s_saved = true;
        s_run_finished = false;
        ai_set_state(AI_STATE_READY, "按下开始");
    }
    else
    {
        s_code[0] = '\0';
        s_code_len = 0;
        ai_set_state(AI_STATE_IDLE, "等待上位机代码");
    }

    // 进入等待前：清空四个按键的锁存消费状态，避免进入瞬间残留键误触发运行（左/右切模式键保留）
    opnEnter = opnExit = opnUp = opnDown = false;

    uint32_t last_draw = 0, last_report = 0;

    while (true)
    {
        if (opnLeft || opnRight || opnPCchangemode)
        {
            break; // 左右键 / 上位机切模式 → 退出，返回主循环重选模式
        }
        // 无运行时：按下四个按键（OK/上/下/Cancel）中任意一个 → 开始运行。
        // 运行结束后（自然结束 / 脚本读到按键自停 / 上位机 AIC,STOP）也由这四个键之一再次触发。
        if (s_code_len != 0 && (opnEnter || opnExit || opnUp || opnDown))
        {
            ResetOpn(); // 开始执行前清空按键消费状态
            ai_start_run();
            continue; // ai_start_run 阻塞至本次执行结束，返回前已清空四键残留
        }
        if (opnEnter || opnExit || opnUp || opnDown)
        {
            ResetOpn(); // 无代码时：消费已按下的键，避免上传代码后残留误启动
        }

        ai_poll_host(); // 读取并处理上位机帧

        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_draw >= 60)
        {
            last_draw = now;
            ai_draw();
        }
        if (now - last_report >= 200)
        {
            last_report = now;
            s_report_pending = false;
            // 空闲也周期上报 AIT：与其它模式的状态帧一致，供上位机识别当前处于 AI 模式并定位到“AI脚本调试”页签
            ai_send_ait();
        }

        sleep_ms(1); // 轮询时间片 1ms（更细，后续高精度预留）
    }

    ai_owns_stdin_flag = false;
    mp_set_heartbeat(nullptr);
    mp_free(); // 释放解释器内存
}
