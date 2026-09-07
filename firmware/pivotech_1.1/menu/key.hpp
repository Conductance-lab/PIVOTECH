#ifndef MENU_KEY_HPP
#define MENU_KEY_HPP

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"

typedef struct {
    volatile bool isPressed;
    volatile bool isLongPress;
    uint32_t pressTime;
} KeyState;

void updateKeys(void);
bool timer_callback(repeating_timer_t *rt);
void Timer_Key_Init(void);

/* Try to send TB select voltage from core loops. Returns true if a send occurred. */
bool Key_TrySendSelectVoltage(void);
void Key_RequestSendSelectVoltage(void);

/* Export key states used elsewhere */
extern KeyState keyUp;
extern KeyState keyDown;
extern KeyState keyEnter;
extern KeyState keyCan;
extern KeyState keyLeft;
extern KeyState keyRight;

extern bool opnRight;
extern bool opnLeft;
extern bool opnEnter;
extern bool opnExit;
extern bool opnUp;
extern bool opnDown;
extern bool opnCtrlUp;
extern bool opnCtrlDown;
extern bool opnCtrl;

#endif // MENU_KEY_HPP