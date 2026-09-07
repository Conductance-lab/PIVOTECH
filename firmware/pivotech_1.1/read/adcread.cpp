#ifndef _ADCREAD_CPP_
#define _ADCREAD_CPP_

#include "CONFIG_FLO.hpp"
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/timer.h"

#define Recordtime 300
#define Adcreadgap 1

float ref_3V3 = 3.3f + 0.1f;
float ref_0V = 0.0f + 0.3f;
float ref_5V = 5.0f + 0.3f;

enum testpin_openstate
{
    PINSTATE_OPEN,
    PINSTATE_HIGH,
    PINSTATE_LOW
};

struct testpin_state
{
    testpin_openstate ta_pinstate;
    testpin_openstate tb_pinstate;
};

testpin_state checkpin()
{
    testpin_state res;
    // gpio_init(TAADCPIN);
    // gpio_set_dir(TAADCPIN, GPIO_IN);
    // gpio_pull_up(TAADCPIN);
    res.ta_pinstate = PINSTATE_OPEN;

    // gpio_init(TBADCPIN);
    // gpio_set_dir(TBADCPIN, GPIO_IN);
    // gpio_pull_up(TBADCPIN);
    res.tb_pinstate = PINSTATE_OPEN;

    return res;
}

// 获取ADC采样值并映射到实际电压（线性映射）
float read_adc_voltage(uint16_t adc_value)
{
    // ADC值范围：0-4095，电压范围：0-30.8V（假设使用了一个分压电路）

    // 手动校零后参数可调
    float voltage = (adc_value / 4096.0f) * 30.8f-0.15f; // 线性映射并校零，0.3V是为了补偿测量误差，可以根据实际情况调整

    return voltage;
}

struct ADCReadResult
{
    uint16_t ta;
    uint16_t tb;
};

// 包含2ms的延迟，确保采样稳定
ADCReadResult adc_read_refresh(bool if_wait = true)
{
    ADCReadResult res{0, 0};

    if(if_wait)
    {
        sleep_ms(1);                     // 确保采样稳定
    }
    adc_select_input(TAADCPIN - 26); // ADC0: GPIO26, ADC1: GPIO27, ADC2: GPIO28, ADC3: GPIO29
    res.ta = (uint16_t)adc_read();

    if(if_wait)
    {
        sleep_ms(1); // 确保采样稳定
    }
     adc_select_input(TBADCPIN - 26);

    res.tb = (uint16_t)adc_read();

    return res;
}

// 使用 alarm_pool_add_repeating_timer_ms 实现定时器，避免 add_repeating_timer_ms 冲突
// repeating_timer adc_timer;

// void Timer_Adcread_Init()
// {

// }

#endif