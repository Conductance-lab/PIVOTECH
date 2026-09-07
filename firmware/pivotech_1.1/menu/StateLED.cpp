#ifndef _STATELED_CPP_
#define _STATELED_CPP_

#include "CONFIG_FLO.hpp"
#include "read/functions.hpp"
#include "read/mod_selection.hpp"

#define LEDholdtime 1
#define PWM_MAX 80
#define initial_brightness 2000

bool if_led_initmode = 1;

enum LED_STATE
{
    LED_OFF,
    LED_ON,
    LED_BLINK_FADE
};

LED_STATE led_mode = LED_OFF;
uint16_t current_brightness = 0;
uint16_t init_brightness = 0;

void Open_StateLED()
{
    led_mode = LED_ON;
}

void Close_StateLED()
{
    led_mode = LED_OFF;
}

void StartStateLED()
{
    led_mode = LED_BLINK_FADE;
    current_brightness = PWM_MAX;
}

// 50ms 中断调用，设计状态机
void stateLED()
{
    if (if_led_initmode == 1)
    {
        // 全部灯渐弱，从200开始
        static bool if_first_run = true;
        if (if_first_run)
        {
            init_brightness = init_brightness + 400;
            if (init_brightness >= initial_brightness)
            {
                init_brightness = initial_brightness;
                if_first_run = false;
            }
        }
        else
        {
            if (init_brightness > 0)
            {
                if (init_brightness < 2)
                    init_brightness = 0;
                else
                    init_brightness = (uint16_t)(init_brightness * 0.6f);
            }
        }

        pwm_set_gpio_level(LEDRUNPIN, init_brightness * 0.3);
        pwm_set_gpio_level(LEDTATBPIN, init_brightness);
        pwm_set_gpio_level(LEDSPIPIN, init_brightness);
        pwm_set_gpio_level(LEDTESTPIN, init_brightness);
        pwm_set_gpio_level(LEDTDPIN, init_brightness);
        pwm_set_gpio_level(LEDTCPIN, init_brightness);
        pwm_set_gpio_level(LEDDEBUGPIN, init_brightness);
        return;
    }
    switch (led_mode)
    {
    case LED_ON:
        // 快亮并持续
        if (current_brightness < PWM_MAX)
        {
            if (current_brightness > PWM_MAX - 20)
                current_brightness = PWM_MAX;
            else
                current_brightness += 34;
        }
        break;
    case LED_OFF:
        // 快灭并持续
        if (current_brightness > 0)
        {
            if (current_brightness < 2)
                current_brightness = 0;
            else
                current_brightness = (uint16_t)(current_brightness * 0.7f);
        }
        break;
    case LED_BLINK_FADE:
        // 快亮已经在 StartStateLED 中设置，这里执行慢灭
        if (current_brightness > 0)
        {
            if (current_brightness < 2)
            {
                current_brightness = 0;
                led_mode = LED_OFF;
            }
            else
            {
                current_brightness = (uint16_t)(current_brightness * 0.7f);
            }
        }
        else
        {
            led_mode = LED_OFF;
        }
        break;
    }

    if (current_brightness > PWM_MAX)
        current_brightness = PWM_MAX; // 硬件PWM最大值限制 {
    if (current_brightness < 0)
        current_brightness = 0; // 硬件PWM最小值限制
    pwm_set_gpio_level(LEDRUNPIN, current_brightness);

    // printf("[LED] Mode: %d, Brightness: %d\r\n", led_mode, current_brightness);

    extern int nowselect_moditem;
    // gemeral led

    // #define LEDTDPIN 10
    // #define LEDSPIPIN 23
    // #define LEDTESTPIN 24
    // #define LEDDEBUGPIN 25
    // #define LEDTATBPIN 28

    // TDPWMLED 直接交给 PWM 输出控制，其他根据功能切换显示状态

    switch (nowselect_function)
    {
    case 0:
    case 1:
        gpio_put(LEDTATBPIN, 1);
        gpio_put(LEDSPIPIN, 0);
        gpio_put(LEDTESTPIN, 0);
        gpio_put(LEDDEBUGPIN, 0);
        break;
    case 2:
        switch (nowselect_moditem)
        {
        case 0:
            gpio_put(LEDTATBPIN, 1);
            gpio_put(LEDSPIPIN, 0);
            gpio_put(LEDTESTPIN, 0);
            gpio_put(LEDDEBUGPIN, 0);
            /* code */
            break;

        default:
            gpio_put(LEDTATBPIN, 0);
            gpio_put(LEDSPIPIN, 0);
            gpio_put(LEDTESTPIN, 1);
            gpio_put(LEDDEBUGPIN, 0);
            break;
        }
        break;
    case 3: // I2C


        {
            gpio_put(LEDTATBPIN, 1);
            gpio_put(LEDSPIPIN, 0);
            gpio_put(LEDTESTPIN, 0);
            gpio_put(LEDDEBUGPIN, 0);
        }

        break;

    case 4: // SPI

        gpio_put(LEDTATBPIN, 0);
        gpio_put(LEDSPIPIN, 1);
        gpio_put(LEDTESTPIN, 0);
        gpio_put(LEDDEBUGPIN, 0);

        break;

    case 5:
        gpio_put(LEDTATBPIN, 1);
        gpio_put(LEDSPIPIN, 0);
        gpio_put(LEDTESTPIN, 0);
        gpio_put(LEDDEBUGPIN, 0);
        break;

    case 6: // AI 测试：指示灯交脚本控制，周期任务不覆盖
        break;

    case 7: // 自定义菜单（原 case 6 顺延为 7）
        if (menusetting_UARTA || menusetting_UARTB)
        {
            gpio_put(LEDTATBPIN, 1);
            gpio_put(LEDSPIPIN, 0);
            gpio_put(LEDTESTPIN, 0);
            gpio_put(LEDDEBUGPIN, 0);
        }
        else
        {
            gpio_put(LEDTATBPIN, 0);
            gpio_put(LEDSPIPIN, 0);
            gpio_put(LEDTESTPIN, 0);
            gpio_put(LEDDEBUGPIN, 0);
        }
        break;

    default:
        break;
    }
}

#endif