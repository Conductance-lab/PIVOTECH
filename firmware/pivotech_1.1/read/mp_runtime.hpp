#ifndef READ_MP_RUNTIME_HPP
#define READ_MP_RUNTIME_HPP

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ============ 自研 Python 子集解释器（M3） ============
// docs/AI测试模式设计方案.md §5.4 / §5.9
//
// 运行模型：在 AI 模式下由 core0 调用 mp_execute() 阻塞执行；
// 解释器在每个语句/循环迭代边界回调“心跳”(mp_set_heartbeat)，
// 心跳返回 true 即请求停止（用户按键/上位机 AIC,STOP/切换模式）。

#ifdef __cplusplus
extern "C" {
#endif

// 执行结果
enum MpResult
{
    MP_DONE = 0, // 正常结束
    MP_ERROR,    // 出错（行号/消息见 mp_error_line/mp_error_msg）
    MP_STOPPED   // 被心跳/请求停止
};

typedef bool (*MpHeartbeatFn)(void); // 返回 true → 请求停止
typedef void (*MpPrintFn)(void);     // print/disp_* 输出回调（事件驱动即时上屏）
typedef void (*MpLineFn)(int line);  // 执行行号变化回调（运行指示灯等）

void  mp_init(void);
void  mp_free(void);
bool  mp_load(const char *src);      // 解析（失败见 mp_error_*）
int   mp_execute(void);              // 执行已载入脚本（阻塞，内部心跳）
void  mp_request_stop(void);
void  mp_set_heartbeat(MpHeartbeatFn cb);
void  mp_set_printcb(MpPrintFn cb);  // print/disp_text/disp_clear 后调用
void  mp_set_linecb(MpLineFn cb);    // 每次执行到新行号时回调（参数=当前行）

int         mp_error_line(void);
const char *mp_error_msg(void);
int         mp_cur_line(void);       // 当前执行行（0=未执行）

// 取脚本第 line(1基) 行文本到 out（供错误定位/调试回显）；返回 0=成功
int mp_source_line_text(int line, char *out, size_t cap);

// 脚本显示缓冲（disp_* 写入，AI 模式 heartbeat 期间绘制）
int   mp_disp_lines(void);           // 已用显示行数(<=4)
const char *mp_disp_line(int idx);   // idx 0..3

#ifdef __cplusplus
}
#endif

#endif // READ_MP_RUNTIME_HPP
