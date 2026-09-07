#ifndef READ_FUNCTIONS_HPP
#define READ_FUNCTIONS_HPP

#include <stdint.h>

extern uint16_t nowselect_function;

// 上位机接口控制标志位
extern bool Enable_PC_Interface;

int functionmenu();
void functionupdate();

// 协议解析与更新
int HandleUSBProtocolUpdate();

/**
 * @brief 获取最近一次解析成功的协议载荷
 * @return 指向 128 字节载荷缓冲区的指针。若数据无效或未更新，返回 NULL。
 */
const uint8_t* GetLastProtocolPayload();

// 仅在 functions.cpp 中需要这些定义进行反向赋值
#ifdef _FUNCTIONS_CPP_

struct ReadItem_t {
    double *param;
    double paramBackup;
    double pwmitem_step;
    char *title;
};

struct PwmOutputConfig {
    uint8_t output_mode;
    uint8_t run_mode;
    uint16_t cfg_value;
    double cfg_step;
};

struct PWMitem_t {
    bool open;
    ReadItem_t *duty;
    ReadItem_t *frec;
    PwmOutputConfig cfg;
};

extern PWMitem_t PWMitem[4];
extern int nowselect_pwmoutput_item;
extern "C" void Update_PwmOutput();
extern "C" void pwmoutput_apply_channel_state(uint8_t idx);

#endif

int ParseProtocol_Single(uint8_t ch);
void SendUSBProtocol_128Args(const uint8_t *args);

#endif