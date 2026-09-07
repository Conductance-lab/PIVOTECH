#ifndef MENU_STATELED_HPP
#define MENU_STATELED_HPP

#include <stdbool.h>

extern bool if_StateLED_OPEN;
extern bool if_led_initmode;

// 考虑加一个设备连接上的暗提示状态

void Open_StateLED(void);
void Close_StateLED(void);
void StartStateLED(void);
void stateLED(void);

#endif // MENU_STATELED_HPP
