#ifndef READ_PWMOUTPUT_HPP
#define READ_PWMOUTPUT_HPP



void Pwm_Output_Init(void);
void pwmoutput(void);
extern bool pwmoutitem_infunction;

// 供 main.cpp 等模块在开机阶段刷新/复位四路 PWM 输出状态
extern "C" void Update_PwmOutput(void);



#endif // READ_PWMOUTPUT_HPP
