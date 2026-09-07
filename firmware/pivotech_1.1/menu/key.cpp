#ifndef _KEY_CPP_
#define _KEY_CPP_

#include "CONFIG_FLO.hpp"
#include "menu/key.hpp"
#include "menu/UI.hpp"
#include "menu/StateLED.hpp"
#include "read/functions.hpp"
#include "read/generalread.hpp"
#include "read/mod_selection.hpp"
#include "hardware/adc.h"
#include "hardware/watchdog.h"
#include "pico/stdio_usb.h"

#include <cstdio>

bool gpiosysmode_last_state; // 上次系统模式引脚状态（用于检测变化）

uint16_t LONG_PRESS_MS = 400; // 长按判定时长

KeyState keyUp = {false, false, 0};
KeyState keyDown = {false, false, 0};
KeyState keyEnter = {false, false, 0};
KeyState keyCan = {false, false, 0};
KeyState keyLeft = {false, false, 0};
KeyState keyRight = {false, false, 0};

bool CtrlUpStatePressed = false;
bool CtrlDownStatePressed = false;
bool CtrlState = false;

bool opnRight, opnLeft;
bool opnEnter, opnExit, opnUp, opnDown;
bool opnCtrlUp;   // CAN + UP
bool opnCtrlDown; // CAN + DOWN
bool opnCtrl;     // 是否按下

static uint8_t tb_adc_tick_500ms = 0;
static bool tb_adc_reader_ready = false;
static bool tb_adc_sample_pending = false;
static bool tb_adc_conversion_running = false;
bool tb_select_voltage_send_pending = false;

static void init_tb_adc_reader()
{
    if (tb_adc_reader_ready)
    {
        return;
    }

    adc_init();
    adc_gpio_init(TAADCPIN);
    adc_select_input(TAADCPIN - 26);
    adc_fifo_setup(true, false, 1, false, false);
    adc_run(false);
    adc_fifo_drain();
    tb_adc_reader_ready = true;
}

static void start_tb_adc_sample()
{
    if (!tb_adc_reader_ready)
    {
        init_tb_adc_reader();
    }

    if (tb_adc_conversion_running)
    {
        return;
    }

    adc_fifo_drain();
    adc_select_input(TAADCPIN - 26);
    adc_run(true);
    tb_adc_conversion_running = true;
    tb_adc_sample_pending = true;
}

static bool try_finish_tb_adc_sample()
{
    if (!tb_adc_sample_pending || !tb_adc_conversion_running)
    {
        return false;
    }

    if (adc_fifo_get_level() == 0)
    {
        return false;
    }

    uint16_t raw_value = (uint16_t)(adc_fifo_get() & 0x0FFF);
    adc_run(false);
    tb_adc_conversion_running = false;
    tb_adc_sample_pending = false;
    select_voltage = ((float)raw_value * 6.6f) / 4095.0f;
    tb_select_voltage_send_pending = true;
    return true;
}

static bool tb_select_voltage_send_allowed()
{
    extern bool if_enter_function;
    // 黑名单（运行期不实时上报 #PIVOSEVO#，仅进入窗口 if_enter_function 漏发一次留底，
    // 上位机据此按“是否持续收到帧”判定为灰色非实时值）：
    //   0           输入状态检测
    //   2 + moditem4 模块配置工具 → ADC 转发
    //   6           AI 脚本调试
    return !(nowselect_function == 0
             || (nowselect_function == 2 && nowselect_moditem == 4)
             || nowselect_function == 6)
           || if_enter_function;
}

// Exported wrapper so core loops can trigger the send from non-IRQ contexts.
bool Key_TrySendSelectVoltage(void)
{
    if (!tb_select_voltage_send_pending)
        return false;
    if (!tb_select_voltage_send_allowed())
    {
        return false;
    }
    if(!Enable_PC_Interface){
        return false;
    }
    if (!stdio_usb_connected())
        return false;
    if (select_voltage < 2.8f)
    {
        printf("#PIVOSEVO#0.0#PSV#\r\n");
    }
    else
    {
        printf("#PIVOSEVO#%.1f#PSV#\r\n", (double)select_voltage);
    }
    tb_select_voltage_send_pending = false;
    return true;
}

void Key_RequestSendSelectVoltage(void)
{
    tb_select_voltage_send_pending = true;
}

void updateKeys()
{
    bool upState = !gpio_get(KEYUP);
    bool downState = !gpio_get(KEYDOWN);
    bool okState = !gpio_get(KEYOK);
    bool canState = !gpio_get(KEYCAN);
    bool leftState = !gpio_get(KEYLEFT);
    bool rightState = !gpio_get(KEYRIGHT);

    // 2. 处理UP键
    if (upState && !canState && !keyCan.isPressed /*组合键未释放时不进入常规按键*/)
    {

        if (!keyUp.isPressed)
        {
            // 首次按下：记录时间戳，标记短按
            keyUp.pressTime = to_ms_since_boot(get_absolute_time());
            keyUp.isPressed = true;
            keyUp.isLongPress = false;
            opnUp = true; // 短按立即触发
        }
        else
        {
            // 持续按下：检查是否达到长按
            uint32_t holdTime = to_ms_since_boot(get_absolute_time()) - keyUp.pressTime;
            if (holdTime >= LONG_PRESS_MS)
            {
                keyUp.isLongPress = true;
                opnUp = true; // 长按保持触发
                footlength = footlengthDefault * 2;
            }
        }
    }
    else
    {
        // 按键释放：重置状态
        if (keyUp.isPressed)
        {
            keyUp.isPressed = false;
            keyUp.isLongPress = false;
            footlength = footlengthDefault;
        }
    }

    // 3. 处理DOWN键（逻辑与UP键完全对称）
    if (downState && !canState && !keyCan.isPressed /*组合键未释放时不进入常规按键*/)
    {
        if (!keyDown.isPressed)
        {
            keyDown.pressTime = to_ms_since_boot(get_absolute_time());
            keyDown.isPressed = true;
            keyDown.isLongPress = false;
            opnDown = true;
        }
        else
        {
            uint32_t holdTime = to_ms_since_boot(get_absolute_time()) - keyDown.pressTime;
            if (holdTime >= LONG_PRESS_MS)
            {
                keyDown.isLongPress = true;
                opnDown = true;
                footlength = footlengthDefault * 2;
            }
        }
    }
    else
    {
        if (keyDown.isPressed)
        {
            keyDown.isPressed = false;
            keyDown.isLongPress = false;
            footlength = footlengthDefault;
        }
    }

    // 4. 处理OK键（仅短按逻辑）
    if (okState)
    {
        if (!keyEnter.isPressed)
        {
            keyEnter.isPressed = true;
            opnEnter = true; // 按下输出
        }
    }
    else
    {
        if (keyEnter.isPressed)
        {
            keyEnter.isPressed = false;
        }
    }

    // 5. 处理CAN键（独立按下逻辑）
    if (canState /*组合键也进入*/)
    {
        opnCtrl = 1;
        if (!keyCan.isPressed)
        {
            keyCan.pressTime = to_ms_since_boot(get_absolute_time());
            keyCan.isPressed = true;
        }

        if (!(upState && downState) /*排除掉UP DOWN同时按下的状况*/)
        {
            if (upState && !CtrlUpStatePressed)
            {
                opnCtrlUp = true; // 按下输出
                CtrlUpStatePressed = true;
                CtrlState = true;
                CtrlDownStatePressed = false; // 排除其他状态 避免某些抽象操作
            }
            if (downState && !CtrlDownStatePressed)
            {

                opnCtrlDown = true; // 按下输出
                CtrlDownStatePressed = true;
                CtrlState = true;
                CtrlUpStatePressed = false; // 排除其他状态 避免某些抽象操作
            }

            if ((!upState && CtrlUpStatePressed))
            {

                CtrlUpStatePressed = false;
            }
            if ((!downState && CtrlDownStatePressed))
            {

                CtrlDownStatePressed = false;
            }
        }
    }
    else
    {

        opnCtrl = 0;
        uint32_t holdTime = to_ms_since_boot(get_absolute_time()) - keyCan.pressTime;
        if (keyCan.isPressed && !CtrlState && holdTime < LONG_PRESS_MS)
        {
            opnExit = 1; // 状态输出
        }
        if (CtrlState)
        {
            CtrlState = 0;
        }
        keyCan.isPressed = false; // 清空状态
    }

    // 处理LEFT键（简化版）
    if (leftState)
    {
        if (!keyLeft.isPressed)
        {
            keyLeft.isPressed = true;
            opnLeft = true; // 按下时触发
        }
    }
    else
    {
        if (keyLeft.isPressed)
        {
            keyLeft.isPressed = false;
        }
    }

    // 处理RIGHT键（简化版）
    if (rightState)
    {
        if (!keyRight.isPressed)
        {
            keyRight.isPressed = true;
            opnRight = true; // 按下时触发
        }
    }
    else
    {
        if (keyRight.isPressed)
        {
            keyRight.isPressed = false;
        }
    }
    return; // 保持定时器运行
};

extern SoundtMode sound_mode;
extern SoundtMode last_sound_mode;

int sound_state = 0;
uint32_t sound_now_mode_start_time = 0;

void soundmode_update()
{
    if (sound_mode != last_sound_mode ||
        sound_mode == oneway_conduction1 ||
        sound_mode == oneway_conduction2)
    {
        if (sound_mode != last_sound_mode)
        {
            sound_state = 0; // 切换模式时重置状态
            sound_now_mode_start_time = to_ms_since_boot(get_absolute_time());
        }
        if (sound_mode == multi_conduction)
        {
            uint buzzer_slice = pwm_gpio_to_slice_num(BUZZERPIN);
            pwm_config buzzer_cfg = pwm_get_default_config();
            pwm_config_set_clkdiv(&buzzer_cfg, 125.0f); // 125MHz / 125 = 1MHz
            pwm_config_set_wrap(&buzzer_cfg, 1000);     // 1MHz / 1000 = 1kHz
            pwm_init(buzzer_slice, &buzzer_cfg, true);
            pwm_set_gpio_level(BUZZERPIN, 500);
        }
        else if (sound_mode == oneway_conduction1)
        {
            uint32_t now = to_ms_since_boot(get_absolute_time());
            uint32_t time_since_start = now - sound_now_mode_start_time;
            if (time_since_start % 500 < 125)
            {
                if (sound_state == 1)
                    return; // 避免重复设置同样的PWM参数
                sound_now_mode_start_time = now;
                sound_state = 1;
                uint buzzer_slice = pwm_gpio_to_slice_num(BUZZERPIN);
                pwm_config buzzer_cfg = pwm_get_default_config();
                pwm_config_set_clkdiv(&buzzer_cfg, 125.0f); // 125MHz / 125 = 1MHz
                pwm_config_set_wrap(&buzzer_cfg, 1000);     // 1MHz / 1000 = 1kHz
                pwm_init(buzzer_slice, &buzzer_cfg, true);
                pwm_set_gpio_level(BUZZERPIN, 500);
            }
            else if (time_since_start % 500 < 250)
            {
                if (sound_state == 2)
                    return; // 避免重复设置同样的PWM参数
                sound_state = 2;
                uint buzzer_slice = pwm_gpio_to_slice_num(BUZZERPIN);
                pwm_config buzzer_cfg = pwm_get_default_config();
                pwm_config_set_clkdiv(&buzzer_cfg, 125.0f); // 125MHz / 125 = 1MHz
                pwm_config_set_wrap(&buzzer_cfg, 500);      // 1MHz / 1000 = 1kHz
                pwm_init(buzzer_slice, &buzzer_cfg, true);
                pwm_set_gpio_level(BUZZERPIN, 250);
            }
            else
            {
                if (sound_state == 0)
                    return; // 避免重复设置同样的PWM参数
                sound_state = 0;
                pwm_set_gpio_level(BUZZERPIN, 0);
            }
        }
        else if (sound_mode == oneway_conduction2)
        {
            uint32_t now = to_ms_since_boot(get_absolute_time());
            uint32_t time_since_start = now - sound_now_mode_start_time;
            if (time_since_start % 500 < 125)
            {
                if (sound_state == 1)
                    return; // 避免重复设置同样的PWM参数
                sound_state = 1;
                sound_now_mode_start_time = now;
                uint buzzer_slice = pwm_gpio_to_slice_num(BUZZERPIN);
                pwm_config buzzer_cfg = pwm_get_default_config();
                pwm_config_set_clkdiv(&buzzer_cfg, 125.0f); // 125MHz / 125 = 1MHz
                pwm_config_set_wrap(&buzzer_cfg, 500);      // 1MHz / 1000 = 1kHz
                pwm_init(buzzer_slice, &buzzer_cfg, true);
                pwm_set_gpio_level(BUZZERPIN, 250);
            }
            else if (time_since_start % 500 < 250)
            {
                if (sound_state == 2)
                    return; // 避免重复设置同样的PWM参数
                sound_state = 2;
                uint buzzer_slice = pwm_gpio_to_slice_num(BUZZERPIN);
                pwm_config buzzer_cfg = pwm_get_default_config();
                pwm_config_set_clkdiv(&buzzer_cfg, 125.0f); // 125MHz / 125 = 1MHz
                pwm_config_set_wrap(&buzzer_cfg, 1000);     // 1MHz / 1000 = 1kHz
                pwm_init(buzzer_slice, &buzzer_cfg, true);
                pwm_set_gpio_level(BUZZERPIN, 500);
            }
            else
            {
                if (sound_state == 0)
                    return; // 避免重复设置同样的PWM参数
                sound_state = 0;
                pwm_set_gpio_level(BUZZERPIN, 0);
            }
        }
        else if (sound_mode == none)
        {
            pwm_set_gpio_level(BUZZERPIN, 0);
        }

        last_sound_mode = sound_mode;
    }
}
// 定时器回调（10ms间隔）
bool timer_callback(repeating_timer_t *rt)
{
    bool gpio0_state = gpio_get(SYS_MODE_PIN);
    if (gpio0_state != gpiosysmode_last_state)
        // printf("SYS MODE CHANGED: %d -> %d\r\n", gpiosysmode_last_state, gpio0_state);
        watchdog_reboot(0, 0, 0);

    try_finish_tb_adc_sample();

    updateKeys();
    stateLED();
    soundmode_update();
    tb_adc_tick_500ms++;
    if (tb_adc_tick_500ms >= 10)
    {
        tb_adc_tick_500ms = 0;
        start_tb_adc_sample();
    }
    return true;
}

repeating_timer_t timer;
void Timer_Key_Init()
{
    gpio_init(SYS_MODE_PIN);
    gpio_set_dir(SYS_MODE_PIN, GPIO_IN);
    gpio_pull_up(SYS_MODE_PIN);
    sleep_ms(1); // 等待GPIO状态稳定
    gpiosysmode_last_state = gpio_get(SYS_MODE_PIN);

    init_tb_adc_reader();

    add_repeating_timer_ms(50, timer_callback, NULL, &timer);
}

#endif