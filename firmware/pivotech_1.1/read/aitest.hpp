#ifndef READ_AITEST_HPP
#define READ_AITEST_HPP

#include <stdint.h>
#include <stdbool.h>

// AI 测试模式运行状态
typedef enum
{
    AI_STATE_IDLE = 0, // 待机（无脚本 / 已清空）
    AI_STATE_READY,    // 缓冲就绪（已载入/已保存），未运行
    AI_STATE_RUNNING,  // 脚本运行中（解释器 M3）
    AI_STATE_STOPPED,  // 已停止
    AI_STATE_ERROR     // 出错
} AiRunState;

extern bool ai_infunction;        // AI 模式激活标志（main.cpp 使用）
extern bool ai_owns_stdin_flag;   // AI 模式 core0 接管 stdin 读取

// 供 core1（core.cpp）判断是否让出 stdin
bool ai_owns_stdin(void);

// AI 测试模式入口（core0，阻塞直至切换模式）
void aitest_main(void);

// 供 core.cpp 状态上报 / 外部查询
const char *ai_state_str(void);
AiRunState ai_state(void);
uint16_t ai_code_len(void);
uint16_t ai_run_line(void);
bool ai_code_saved(void);

// 上位机控制（AIC,<sub> 与 #PIVOAIUP#..#ENDAIC#）
void ai_process_command(const char *sub);
void ai_handle_upload_payload(const char *payload, size_t len);
void ai_request_stop(void);

#endif // READ_AITEST_HPP
