/*
本部分描述
需读取PIO部分协同控制
现支持2X10Mhz 高频采集 完全不冲突 10khz以下支持0.01%精度PWM检测
基于复杂的中断管理 复用引脚功能 双核计算 pio可编程IO（core1同步高频信息） 实现高频率采集
*/

/*
在中断结束判定中间 根据F 决定：直流检测 根据双悬空 决定：电阻检测

直流电平判断规则
CMOS状态    非EN状态    判定
group
0           0          相对低电压
H_悬空+     0          相对高电压

group
H_悬空-     0          相对低电压
H_悬空+     0          相对高电压

H_悬空      0           悬空

H_悬空+     H_悬空+     正常电压

0           0          接地


只有双悬空会进入短路测试（理论最大允许相对压差3V3）
*/

#define VOL_EDGE1 2.7f // 电压判定边界，单位伏特。自动另一个阈值为 2.7-0.5=2.2V
#define VOL_EDGE2 3.8f // 电压判定边界，单位伏特。自动另一个阈值为 2.7-0.5=2.2V

enum TestMode
{
    TestMode_STEADY_G = 3, // 稳态对地电压
    TestMode_STEADY_D = 4, // 稳态差分电压 （与地隔绝）
    TestMode_PULSE = 2,    // 脉冲
    TestMode_OPEN = 0,     // 悬空
    TestMode_SHORT = 1,    // 短接
    TestMode_UNKNOWN = 255
};

#ifndef _GENERALREAD_CPP_
#define _GENERALREAD_CPP_
#include "pico/stdlib.h"
#include "menu/ssd1306.hpp"
#include "menu/ssd1306.hpp"
#include "Fonts/fonts.h"
#include "menu/UI.hpp"
#include "menu/key.hpp"
#include "menu/Curve.hpp"
#include "menu/StateLED.hpp"
#include "read/adcread.hpp"
#include "CONFIG_FLO.hpp"

#include <cstring>
#include <cstdio>
#include <cmath>
#include "hardware/irq.h"
#include "read/functions.hpp"

float TA_F, TB_F;
float TA_Duty; // TA 占空比，百分比（0.0~100.0）
float TB_Duty; // TB 占空比，百分比（0.0~100.0）

#define f_cal_num 30
#define freq_max 200000.0f
#define freq_min 5.0f

uint8_t ta_test_mode = TestMode_OPEN;
uint8_t tb_test_mode = TestMode_OPEN;

float ta_steady_voltage = 0.0f;
float tb_steady_voltage = 0.0f;
float ta_tb_diff_voltage = 0.0f;

volatile uint32_t ta_rise_tick = 0, ta_fall_tick = 0, tb_rise_tick = 0, tb_fall_tick = 0;

// 均在一个refresh周期内采集的数据
volatile float ta_freqs[f_cal_num] = {0}, tb_freqs[f_cal_num] = {0};
volatile float ta_dutys[f_cal_num] = {0}, tb_dutys[f_cal_num] = {0};
volatile int ta_idx = 0, tb_idx = 0;

uint32_t pio_ta_count_frec = 0, pio_tb_count_frec = 0;

uint32_t afreqactive_time_us = 0; // 全局变量用于存储TA时间（微秒）
uint32_t bfreqactive_time_us = 0; // 全局变量用于存储TB时间（微秒）

uint32_t last_refresh_time = 0; // 上次刷新时间
uint32_t last_pc_sync_time = 0; // 上次PC同步时间

extern uint32_t edge_count_a;
extern uint32_t edge_count_b;

// PIO  缓存算法
#define pico_f_cal_num 3
uint32_t edge_count_group_a[pico_f_cal_num] = {0};
uint32_t edge_count_group_b[pico_f_cal_num] = {0};
uint8_t edge_count_group_index = 0;
// PIO 平滑权重
#define WEIGHTED_SWOOP 0.98f

// 记录高频准入 定向开启中断
bool highspeed_ta_flag = false;
bool highspeed_tb_flag = false;

uint32_t last_ta_count_frec = 0; // 仅一次周期的计数结果 无滤波
uint32_t last_tb_count_frec = 0;

void cal_TAandTB_freq()
{
    uint32_t current_time = time_us_32();

    {
        last_ta_count_frec = pio_ta_count_frec * (1000000.0f / (float)refresh_generalread); // 仅一次周期的计数结果 无滤波

        if (last_ta_count_frec > 10000.0f)
        {
            highspeed_ta_flag = true;
            uint32_t TA_F_TEMP = 0;
            for (int i = 0; i < pico_f_cal_num; i++)
            {
                TA_F_TEMP += edge_count_group_a[i];
            }
            TA_F = WEIGHTED_SWOOP * TA_F_TEMP / ((refresh_generalread * 3) / 1000000.0f) + (1.0f - WEIGHTED_SWOOP) * TA_F;
            TA_Duty = -1.0f;
        }

        else
        {
            highspeed_ta_flag = false;
            // 计算 TA_F 平均值，占空比平均
            float ta_sum = 0.0f;
            float ta_duty_sum = 0.0f;
            int ta_count = 0;
            int ta_duty_count = 0;
            // 从索引1开始，跳过可能的初始无效数据
            for (int i = 1; i < ta_idx; ++i)
            {
                float ta_f = ta_freqs[i];
                if (ta_f < freq_min || ta_f > freq_max)
                    continue;
                float ta_d = ta_dutys[i];
                ta_sum += ta_f;
                ta_count++;
                ta_duty_sum += ta_d;
                ta_duty_count++;
            }

            TA_F = (ta_count > 0) ? (ta_sum / ta_count) : 0.0f;
            last_ta_count_frec = TA_F; // 更新全局变量以供其他部分使用
            TA_Duty = (ta_duty_count > 0) ? (ta_duty_sum / ta_duty_count) : 0.0f;
            ta_idx = 0; // 重置触发中断计数器 实现持续采样
        }
    }

    // 处理TB频率和占空比计算
    {
        last_tb_count_frec = pio_tb_count_frec * (1000000.0f / (float)refresh_generalread); // 仅一次周期的计数结果 无滤波

        if (last_tb_count_frec > 10000.0f)
        {
            highspeed_tb_flag = true;
            uint32_t TB_F_TEMP = 0;
            for (int i = 0; i < pico_f_cal_num; i++)
            {
                TB_F_TEMP += edge_count_group_b[i];
            }
            TB_F = WEIGHTED_SWOOP * TB_F_TEMP / ((refresh_generalread * 3) / 1000000.0f) + (1.0f - WEIGHTED_SWOOP) * TB_F;
            TB_Duty = -1.0f;
        }
        else
        {
            highspeed_tb_flag = false;
            // 计算 TB_F 平均值，占空比平均
            float tb_sum = 0.0f;
            float tb_duty_sum = 0.0f;
            int tb_count = 0;
            int tb_duty_count = 0;
            // 从索引1开始，跳过可能的初始无效数据
            for (int i = 1; i < tb_idx; ++i)
            {
                float tb_f = tb_freqs[i];
                if (tb_f < freq_min || tb_f > freq_max)
                    continue;
                float tb_d = tb_dutys[i];
                tb_sum += tb_f;
                tb_count++;
                tb_duty_sum += tb_d;
                tb_duty_count++;
            }

            TB_F = (tb_count > 0) ? (tb_sum / tb_count) : 0.0f;
            last_tb_count_frec = TB_F; // 更新全局变量以供其他部分使用
            TB_Duty = (tb_duty_count > 0) ? (tb_duty_sum / tb_duty_count) : 0.0f;
            tb_idx = 0; // 重置触发中断计数器 实现持续采样
        }

        // printf("A %d %.2f B %d %.2f\n", pio_ta_count_frec, TA_F, pio_tb_count_frec, TB_F);/////////////////////////////////////////////
    }
}

void freq_isr(uint gpio, uint32_t events)
{
    uint32_t now = time_us_32();

    if (now - last_refresh_time >= refresh_generalread - 100)
    {
        const uint freq_pins[] = {TAINPIN, TBINPIN};
        for (int i = 0; i < 2; ++i)
        {
            gpio_set_irq_enabled(freq_pins[i], GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
        }
        return;
    }

    if (gpio == TAINPIN)
    {
        afreqactive_time_us = now; // 更新TA时间
        if (events & GPIO_IRQ_EDGE_RISE)
        {
            // 上升沿

            ta_rise_tick = now;
        }
        if (events & GPIO_IRQ_EDGE_FALL)
        {
            // 下降沿

            if (ta_rise_tick != 0)
            {
                float high_time = now - ta_rise_tick;
                float period = now - ta_fall_tick;
                if (period > 0)
                {
                    ta_freqs[ta_idx % f_cal_num] = 1000000.0f / period;
                    ta_dutys[ta_idx % f_cal_num] = (high_time / period) * 100.0f;
                }
            }
            ta_fall_tick = now;

            ta_idx++;
        }
    }

    if (gpio == TBINPIN)
    {
        bfreqactive_time_us = now; // 更新TB时间
        if (events & GPIO_IRQ_EDGE_RISE)
        {
            // 上升沿

            tb_rise_tick = now;
        }
        if (events & GPIO_IRQ_EDGE_FALL)
        {
            // 下降沿

            if (tb_rise_tick != 0)
            {
                float high_time = now - tb_rise_tick;
                float period = now - tb_fall_tick;
                if (period > 0)
                {
                    tb_freqs[tb_idx % f_cal_num] = 1000000.0f / period;
                    tb_dutys[tb_idx % f_cal_num] = (high_time / period) * 100.0f;
                }
            }
            tb_fall_tick = now;

            tb_idx++;
        }
    }

    if (ta_idx >= f_cal_num)
    {

        ta_idx = f_cal_num - 1; // 结束采样
    }
    if (tb_idx >= f_cal_num)
    {

        tb_idx = f_cal_num - 1; // 结束采样
    }
}

void Frequency_Read_Init()
{
    // 重置复用引脚状态
    gpio_init(TEST_SPI_CS_PIN);
    gpio_set_dir(TEST_SPI_CS_PIN, GPIO_IN);
    gpio_disable_pulls(TEST_SPI_CS_PIN);

    // 初始化 TA/TB 频率采集引脚及中断
    const uint freq_pins[] = {TAINPIN, TBINPIN};
    for (int i = 0; i < 2; ++i)
    {
        gpio_init(freq_pins[i]);
        gpio_set_dir(freq_pins[i], GPIO_IN);
        // 设置内部上拉
        gpio_pull_up(freq_pins[i]);
        gpio_set_irq_enabled_with_callback(freq_pins[i],
                                           GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                           true,
                                           &freq_isr);
        gpio_pull_up(freq_pins[i]);
    }
}

void Frequency_Read_Deinit()
{
    // 取消 TA/TB 频率采集引脚及中断
    const uint freq_pins[] = {TAINPIN, TBINPIN};
    for (int i = 0; i < 2; ++i)
    {
        gpio_set_irq_enabled(freq_pins[i], GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
    }
}

bool compare_vol(float a, float b, float epsilon = 0.2f)
{
    return fabs(a - b) < epsilon;
}

extern float ref_3V3;
extern float ref_0V;
extern float ref_5V;

enum SoundtMode
{
    none,
    multi_conduction,
    oneway_conduction1,
    oneway_conduction2
};

SoundtMode sound_mode = none;
SoundtMode last_sound_mode = none;

bool generalreadIsRunning = false;

// 绘制二级窗口（设置窗口）
void draw_generalread_setting_window()
{
    int heigh = 32;
    int width = 84;
    int x = (SCREEN_WIDTH - width) / 2;
    int y = (SCREEN_HEIGHT - heigh) / 2 + 1;

    Paint_DrawString_EN_CenterAtX(buf, SCREEN_WIDTH / 2, y + 4, "设置", &Font12, 1);
    Paint_DrawString_EN_CenterAtX(buf, SCREEN_WIDTH / 2, y + 18, "按退出键", &Font12, 1);
}

// 进入通用读取菜单时的注意事项弹窗
static void draw_generalread_notice_window()
{
    int heigh = 40;
    int width = 116;
    int x = (SCREEN_WIDTH - width) / 2;
    int y = (SCREEN_HEIGHT - heigh) / 2 + 1;

    (void)x;
    Paint_DrawString_EN_CenterAtX(buf, SCREEN_WIDTH / 2, y + 8, "使用前请务必", &Font12, 1);
    Paint_DrawString_EN_CenterAtX(buf, SCREEN_WIDTH / 2, y + 22, "拔出其他连接", &Font12, 1);
}

bool draw_general_read(int8_t getdraw = 0)
{
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    static uint32_t last_draw_time = 0;
    if (current_time - last_refresh_time >= refresh_generalread)
    {

        edge_count_group_a[edge_count_group_index] = pio_ta_count_frec;
        edge_count_group_b[edge_count_group_index] = pio_tb_count_frec;
        edge_count_group_index = (edge_count_group_index + 1) % pico_f_cal_num;

        cal_TAandTB_freq();

        // printf("TA: %d -> %.2fHz duty: %.2f%%, TB: %d -> %.2fHz duty: %.2f%%\n", pio_ta_count_frec, TA_F, TA_Duty, pio_tb_count_frec, TB_F, TB_Duty); /////////////////////////////////////////////
        getdraw = 2; // 强制刷新显示
    }

    if (getdraw == 2 || ta_test_mode == TestMode_SHORT || tb_test_mode == TestMode_SHORT) // 强制刷新时是刚刚完成计算 开始细致分析
    {
        // 初始都为OPEN模式
        ta_test_mode = TestMode_UNKNOWN;
        tb_test_mode = TestMode_UNKNOWN;

        if (last_ta_count_frec < 5 ||
            last_tb_count_frec < 5) // 双脉冲模式不进行处理
        {

            // 内部上拉+外部上拉采样
            ADCReadResult res1 = adc_read_refresh();

            // 内部下拉+禁用外部上拉采样

            gpio_put(PULLUP_EN_PIN, 0); // 关闭上拉
            if (last_ta_count_frec < 5)
                gpio_pull_down(TAINPIN);
            if (last_tb_count_frec < 5)
                gpio_pull_down(TBINPIN);

            // sleep_ms(50); // 确保采样稳定 现在使用模糊判定 无需等待

            ADCReadResult res2 = adc_read_refresh();

            // 恢复在最后
            float res1_tb_vol = read_adc_voltage(res1.tb);

            float res1_ta_vol = read_adc_voltage(res1.ta);

            float res2_tb_vol = read_adc_voltage(res2.tb);

            float res2_ta_vol = read_adc_voltage(res2.ta);

            // printf("TA: %.3fV -> %.3fV, TB: %.3fV -> %.3fV\n", res1_ta_vol, res2_ta_vol, res1_tb_vol, res2_tb_vol); /////////////////////////////////////////////
            // printf("TA: %d -> %d, TB: %d -> %d\n", res1.ta, res2.ta, res1.tb, res2.tb);                             /////////////////////////////////////////////

            if (last_ta_count_frec < 5)
            {

                ta_test_mode = TestMode_STEADY_G; // 一般是对地稳态
                ta_steady_voltage = res2_ta_vol;

                if (res1_ta_vol > VOL_EDGE1 && (res1_ta_vol - res2_ta_vol) > 0.8f)
                {
                    ta_test_mode = TestMode_OPEN;    // 一般是悬空
                    ta_steady_voltage = res2_ta_vol; // 无效电压
                }
            }
            if (last_tb_count_frec < 5)
            {
                tb_test_mode = TestMode_STEADY_G; // 一般是对地稳态
                tb_steady_voltage = res2_tb_vol;

                if (res1_tb_vol > VOL_EDGE1 && (res1_tb_vol - res2_tb_vol) > 0.8f)
                {
                    tb_test_mode = TestMode_OPEN;    // 一般是悬空
                    tb_steady_voltage = res2_tb_vol; // 无效电压
                }
            }

            // 双悬空 进一步分析 可能出现短路态
            if (ta_test_mode == TestMode_OPEN && tb_test_mode == TestMode_OPEN &&
                (last_ta_count_frec < 5 && last_tb_count_frec < 5)) // 双悬空且至少有一个频率有效
            {

                // 暂时证明双悬空 开始证明 导通性质

                gpio_put(PULLUP_EN_PIN, 0); // 上拉

                // A→B
                gpio_init(TAINPIN);
                gpio_set_dir(TAINPIN, GPIO_OUT);
                gpio_pull_up(TAINPIN);

                gpio_init(TBINPIN);
                gpio_set_dir(TBINPIN, GPIO_IN);
                gpio_pull_up(TBINPIN);

                gpio_put(TAINPIN, 1);
                sleep_us(10);
                bool ture1 = gpio_get(TBINPIN);

                gpio_put(TAINPIN, 0);
                sleep_us(10);
                bool ture2 = !gpio_get(TBINPIN);

                gpio_put(TAINPIN, 1);
                sleep_us(20);
                bool ture3 = gpio_get(TBINPIN);

                gpio_put(TAINPIN, 0);
                sleep_us(20);
                bool ture4 = !gpio_get(TBINPIN);

                gpio_put(TAINPIN, 1);
                sleep_us(30);
                bool ture5 = gpio_get(TBINPIN);

                gpio_put(TAINPIN, 0);
                sleep_us(30);
                bool ture6 = !gpio_get(TBINPIN);

                // B→A
                gpio_init(TBINPIN);
                gpio_set_dir(TBINPIN, GPIO_OUT);
                gpio_pull_up(TBINPIN);

                gpio_init(TAINPIN);
                gpio_set_dir(TAINPIN, GPIO_IN);
                gpio_pull_up(TAINPIN);

                gpio_put(TBINPIN, 1);
                sleep_us(10);
                bool ture7 = gpio_get(TAINPIN);

                gpio_put(TBINPIN, 0);
                sleep_us(10);
                bool ture8 = !gpio_get(TAINPIN);

                gpio_put(TBINPIN, 1);
                sleep_us(20);
                bool ture9 = gpio_get(TAINPIN);

                gpio_put(TBINPIN, 0);
                sleep_us(20);
                bool ture10 = !gpio_get(TAINPIN);

                gpio_put(TBINPIN, 1);
                sleep_us(30);
                bool ture11 = gpio_get(TAINPIN);

                gpio_put(TBINPIN, 0);
                sleep_us(30);
                bool ture12 = !gpio_get(TAINPIN);

                // printf("A->B: %d%d%d%d%d%d, B->A: %d%d%d%d%d%d\n", ture1, ture2, ture3, ture4, ture5, ture6, ture7, ture8, ture9, ture10, ture11, ture12);/////////////////////////////////////////////

                bool ta_short = ture1 && ture2 && ture3 && ture4 && ture5 && ture6;
                bool tb_short = ture7 && ture8 && ture9 && ture10 && ture11 && ture12;
                if (ta_short && !tb_short)
                {
                    ta_test_mode = TestMode_SHORT;
                    tb_test_mode = TestMode_OPEN;
                }
                else if (!ta_short && tb_short)
                {
                    ta_test_mode = TestMode_OPEN;
                    tb_test_mode = TestMode_SHORT;
                }
                else if (ta_short && tb_short)
                {
                    ta_test_mode = TestMode_SHORT;
                    tb_test_mode = TestMode_SHORT;
                }
                else
                {
                    ta_test_mode = TestMode_OPEN;
                    tb_test_mode = TestMode_OPEN;
                }
            }

            // 如果双差值一样 开始第三次采样分析 可能TA TB 同时转为差分态
            if (last_ta_count_frec < 5 &&
                last_tb_count_frec < 5 &&
                compare_vol(res1_ta_vol - res1_tb_vol, res2_ta_vol - res2_tb_vol, 0.3) &&
                (res1_ta_vol - res1_tb_vol) > 0.2f &&
                (res1_ta_vol - res2_ta_vol) > 0.2f &&
                (ta_test_mode != TestMode_SHORT && tb_test_mode != TestMode_SHORT)) // 都没有频率 且两种模式下差值一样
            {

                ta_steady_voltage = res2_ta_vol;
                tb_steady_voltage = res2_tb_vol;
                ta_tb_diff_voltage = res1_ta_vol - res1_tb_vol;

                ta_test_mode = TestMode_STEADY_D;
                tb_test_mode = TestMode_STEADY_D;

                // printf("TA: %.3fV -> %.3fV, TB: %.3fV -> %.3fV\n", res1_ta_vol, res2_ta_vol, res1_tb_vol, res2_tb_vol); /////////////////////////////////////////////
            }

            // 恢复脉冲态的显示
            if (TA_F > 1.0f &&
                ta_test_mode != TestMode_SHORT &&
                tb_test_mode != TestMode_SHORT &&
                ta_test_mode != TestMode_STEADY_D &&
                tb_test_mode != TestMode_STEADY_D) // 仅TA频率有效
            {
                ta_test_mode = TestMode_PULSE;
                ta_steady_voltage = res2_ta_vol; // 其实没用
            }
            if (TB_F > 1.0f &&
                tb_test_mode != TestMode_SHORT &&
                ta_test_mode != TestMode_SHORT &&
                tb_test_mode != TestMode_STEADY_D &&
                ta_test_mode != TestMode_STEADY_D) // 仅TB频率有效
            {
                tb_test_mode = TestMode_PULSE;
                tb_steady_voltage = res2_tb_vol; // 其实没用
            }
            // 恢复上拉
            gpio_put(PULLUP_EN_PIN, 1); // 使能上拉
            gpio_set_dir(TAINPIN, GPIO_IN);
            gpio_set_dir(TBINPIN, GPIO_IN);
            gpio_pull_up(TAINPIN);
            gpio_pull_up(TBINPIN);

            // 打印按键状态和3个电压值
            // printf("TA Mode: %d, TB Mode: %d, TA Steady Voltage: %.3fV, TB Steady Voltage: %.3fV, TA-TB Diff Voltage: %.3fV\n",
            //    ta_test_mode, tb_test_mode, ta_steady_voltage, tb_steady_voltage, ta_tb_diff_voltage);
        }
        else
        { // 双频率模式 直接显示频率 不进行电压分析
            //  printf("TA: %d, TB: %d\n", pio_ta_count_frec, pio_tb_count_frec);/////////////////////////////////////////////

            ta_test_mode = TestMode_PULSE;
            tb_test_mode = TestMode_PULSE;
        }
    }

    else if (current_time - last_draw_time >= refresh_generalread_ssd1306_draw &&
             (ta_test_mode == TestMode_SHORT || tb_test_mode == TestMode_SHORT)) // 短路态
    {
        getdraw = 1;
    }

    if (getdraw)
    {
        last_draw_time = current_time;

        // 格式化 TA/TB 的显示内容
        char ta_v_str[9], tb_v_str[9];
        char ta_f_str[9], tb_f_str[9];
        char ta_duty_str[9], tb_duty_str[9];
        char ta_tb_diff_str[9];
        char ta_tb_vol[9];

        // 电压，保留2位小数
        snprintf(ta_v_str, sizeof(ta_v_str), "%.1fV", ta_steady_voltage);
        snprintf(tb_v_str, sizeof(tb_v_str), "%.1fV", tb_steady_voltage);

        // 差值电压
        if (ta_tb_diff_voltage > 0.0f)
        {
            snprintf(ta_tb_diff_str, sizeof(ta_tb_diff_str), "%.1fV", ta_tb_diff_voltage);
            snprintf(ta_tb_vol, sizeof(ta_tb_vol), "+  -");
        }
        else
        {
            snprintf(ta_tb_diff_str, sizeof(ta_tb_diff_str), "%.1fV", -ta_tb_diff_voltage);
            snprintf(ta_tb_vol, sizeof(ta_tb_vol), "-  +");
        }

        //  为后续

        // 频率显示格式化
        if (TA_F < 1000)
        {
            snprintf(ta_f_str, sizeof(ta_f_str), "%.2fHz", TA_F);
        }
        else if (TA_F < 10000)
        {
            snprintf(ta_f_str, sizeof(ta_f_str), "%.3fkHz", TA_F / 1000.0f);
        }
        else if (TA_F < 100000)
        {
            snprintf(ta_f_str, sizeof(ta_f_str), "%.2fkHz", TA_F / 1000.0f);
        }
        else if (TA_F < 1000000)
        {
            snprintf(ta_f_str, sizeof(ta_f_str), "%.1fkHz", TA_F / 1000.0f);
        }
        else if (TA_F < 4000000)
        {
            snprintf(ta_f_str, sizeof(ta_f_str), "%.3fMHz", TA_F / 1000000.0f);
        }
        else if (TA_F >= 4000000)
        {
            snprintf(ta_f_str, sizeof(ta_f_str), ">4MHz");
        }

        if (TB_F < 1000)
        {
            snprintf(tb_f_str, sizeof(tb_f_str), "%.2fHz", TB_F);
        }
        else if (TB_F < 10000)
        {
            snprintf(tb_f_str, sizeof(tb_f_str), "%.3fkHz", TB_F / 1000.0f);
        }
        else if (TB_F < 100000)
        {
            snprintf(tb_f_str, sizeof(tb_f_str), "%.2fkHz", TB_F / 1000.0f);
        }
        else if (TB_F < 1000000)
        {
            snprintf(tb_f_str, sizeof(tb_f_str), "%.1fkHz", TB_F / 1000.0f);
        }
        else if (TB_F < 4000000)
        {
            snprintf(tb_f_str, sizeof(tb_f_str), "%.3fMHz", TB_F / 1000000.0f);
        }
        else if (TB_F >= 4000000)
        {
            snprintf(tb_f_str, sizeof(tb_f_str), ">4MHz");
        }

        // 占空比，保留3位小数

        if (TA_Duty == -1.0)
        {
            snprintf(ta_duty_str, sizeof(ta_duty_str), "--%");
        }
        else
        {
            snprintf(ta_duty_str, sizeof(ta_duty_str), "%.2f%%", TA_Duty);
        }

        if (TB_Duty == -1.0)
        {
            snprintf(tb_duty_str, sizeof(tb_duty_str), "--%");
        }
        else
        {
            snprintf(tb_duty_str, sizeof(tb_duty_str), "%.2f%%", TB_Duty);
        }

        // 居中打印 TA/TB 的内容

        DrawRectangle(buf, 0, 0, 64, 16, 0, 1);
        DrawRectangle(buf, 64, 0, 64, 16, 0, 1);

        Paint_DrawString_EN_CenterAtX(buf, 32, 2, "TA", &Font16, 1);
        Paint_DrawString_EN_CenterAtX(buf, 96, 2, "TB", &Font16, 1);
        if (ta_test_mode == TestMode_SHORT || tb_test_mode == TestMode_SHORT)
        {

            if (ta_test_mode == TestMode_SHORT && tb_test_mode == TestMode_SHORT)
            {

                Paint_DrawString_EN_CenterAtX(buf, 64, 51, "双向导通", &Font12, 1);
                static int x_offset = 0;
                x_offset = (x_offset + 1) % 11;
                Paint_DrawString_EN_CenterAtX(buf, 64 + x_offset, 21, ">>>>>>>>", &Font16, 1);
                Paint_DrawString_EN_CenterAtX(buf, 64 - x_offset, 37, "<<<<<<<<", &Font16, 1);
                DrawRectangle(buf, 0, 17, 32, 35, 1, 0);
                DrawRectangle(buf, 96, 17, 32, 35, 1, 0);
                DrawLine(buf, 32, 16, 32, 43, 1);
                DrawLine(buf, 96, 16, 96, 43, 1);
                DrawLine(buf, 32, 27, 96, 27, 1);
                DrawLine(buf, 32, 43, 96, 43, 1);
                sound_mode = multi_conduction;
            }
            else if (ta_test_mode == TestMode_OPEN && tb_test_mode == TestMode_SHORT) // short可以作为低电平拉过来
            {
                Paint_DrawString_EN_CenterAtX(buf, 64, 51, "单向导通", &Font12, 1);
                static int x_offset = 0;
                x_offset = (x_offset + 1) % 11;
                Paint_DrawString_EN_CenterAtX(buf, 64 + x_offset, 21, ">>>>>>>>", &Font16, 1);
                Paint_DrawString_EN_CenterAtX(buf, 62, 37, "<   <", &Font16, 1);

                DrawRectangle(buf, 0, 17, 32, 35, 1, 0);
                DrawRectangle(buf, 96, 17, 32, 35, 1, 0);
                DrawLine(buf, 32, 16, 32, 43, 1);
                DrawLine(buf, 96, 16, 96, 43, 1);

                DrawLine(buf, 32, 27, 96, 27, 1);

                DrawLine(buf, 32, 43, 54, 43, 1);
                DrawLine(buf, 74, 43, 96, 43, 1);

                //  X
                DrawLine(buf, 60, 39, 68, 47, 1);
                DrawLine(buf, 68, 39, 60, 47, 1);
                sound_mode = oneway_conduction1;
            }
            else if (ta_test_mode == TestMode_SHORT && tb_test_mode == TestMode_OPEN) // short可以作为低电平拉过来
            {
                Paint_DrawString_EN_CenterAtX(buf, 64, 51, "单向导通", &Font12, 1);
                static int x_offset = 0;
                x_offset = (x_offset + 1) % 11;
                Paint_DrawString_EN_CenterAtX(buf, 64 - x_offset, 37, "<<<<<<<<", &Font16, 1);
                Paint_DrawString_EN_CenterAtX(buf, 66, 21, ">   >", &Font16, 1);

                DrawRectangle(buf, 0, 17, 32, 35, 1, 0);
                DrawRectangle(buf, 96, 17, 32, 35, 1, 0);
                DrawLine(buf, 32, 16, 32, 43, 1);
                DrawLine(buf, 96, 16, 96, 43, 1);

                DrawLine(buf, 32, 43, 96, 43, 1);

                DrawLine(buf, 32, 27, 54, 27, 1);
                DrawLine(buf, 74, 27, 96, 27, 1);

                // X
                DrawLine(buf, 60, 23, 68, 31, 1);
                DrawLine(buf, 68, 23, 60, 31, 1);
                sound_mode = oneway_conduction2;
            }
        }
        else if (ta_test_mode == TestMode_STEADY_D && tb_test_mode == TestMode_STEADY_D)
        {

            sound_mode = none;

            // 上面两端竖线
            DrawLine(buf, 32, 16, 32, 38, 1);
            DrawLine(buf, 96, 16, 96, 38, 1);

            // 上面 两个横线
            DrawLine(buf, 32, 27, 42, 27, 1);
            DrawLine(buf, 86, 27, 96, 27, 1);

            Paint_DrawString_EN_CenterAtX(buf, 64, 34, ta_tb_diff_str, &Font12, 1);
            Paint_DrawString_EN_CenterAtX(buf, 64, 22, ta_tb_vol, &Font12, 1);
            Paint_DrawString_EN_CenterAtX(buf, 64, 21, "V", &Font16, 1);

            // 两个X
            DrawLine(buf, 28, 41, 36, 49, 1);
            DrawLine(buf, 36, 41, 28, 49, 1);

            DrawLine(buf, 92, 41, 100, 49, 1);
            DrawLine(buf, 100, 41, 92, 49, 1);

            // 下面两段竖向
            DrawLine(buf, 32, 52, 32, 56, 1);
            DrawLine(buf, 96, 52, 96, 56, 1);

            // GND标识
            DrawLine(buf, 126, 55, 126, 57, 1);
            DrawLine(buf, 124, 53, 124, 59, 1);
            DrawLine(buf, 122, 51, 122, 61, 1);
            // GND连接横线
            DrawLine(buf, 32, 56, 122, 56, 1);
        }
        else
        {
            sound_mode = none;

            // 下面两段竖向
            DrawLine(buf, 32, 52, 32, 56, 1);
            DrawLine(buf, 96, 52, 96, 56, 1);

            // GND标识
            DrawLine(buf, 126, 55, 126, 57, 1);
            DrawLine(buf, 124, 53, 124, 59, 1);
            DrawLine(buf, 122, 51, 122, 61, 1);
            // GND连接横线
            DrawLine(buf, 32, 56, 122, 56, 1);

            // 以下AB分开显示
            // TA

            if (ta_test_mode == TestMode_PULSE)
            {
                Paint_DrawString_EN_CenterAtX(buf, 32, 17, ta_f_str, &Font12, 1);
                Paint_DrawString_EN_CenterAtX(buf, 32, 30, ta_duty_str, &Font12, 1);
                DrawLine(buf, 32, 43, 32, 52, 1);
            }
            else if (ta_test_mode == TestMode_STEADY_G)
            {
                if (ta_steady_voltage > 0.4f)
                {
                    Paint_DrawString_EN_CenterAtX(buf, 32, 17, "DC", &Font12, 1);
                    Paint_DrawString_EN_CenterAtX(buf, 32, 30, ta_v_str, &Font12, 1);

                    DrawLine(buf, 32, 43, 32, 52, 1);
                }
                else
                {
                    Paint_DrawString_EN_CenterAtX(buf, 32, 19, "GND", &Font12, 1);
                    DrawLine(buf, 32, 32, 32, 52, 1);
                }
            }
            else if (ta_test_mode == TestMode_OPEN)
            {
                Paint_DrawString_EN_CenterAtX(buf, 32, 19, "未连接", &Font12, 1);
                // 上面两端竖线
                DrawLine(buf, 32, 32, 32, 38, 1);

                // 两个X
                DrawLine(buf, 28, 41, 36, 49, 1);
                DrawLine(buf, 36, 41, 28, 49, 1);
            }

            // TB
            if (tb_test_mode == TestMode_PULSE)
            {
                Paint_DrawString_EN_CenterAtX(buf, 96, 17, tb_f_str, &Font12, 1);
                Paint_DrawString_EN_CenterAtX(buf, 96, 30, tb_duty_str, &Font12, 1);
                DrawLine(buf, 96, 43, 96, 52, 1);
            }
            else if (tb_test_mode == TestMode_STEADY_G)
            {
                if (tb_steady_voltage > 0.4f)
                {
                    Paint_DrawString_EN_CenterAtX(buf, 96, 17, "DC", &Font12, 1);
                    Paint_DrawString_EN_CenterAtX(buf, 96, 30, tb_v_str, &Font12, 1);

                    DrawLine(buf, 96, 43, 96, 52, 1);
                }
                else
                {
                    Paint_DrawString_EN_CenterAtX(buf, 96, 19, "GND", &Font12, 1);
                    DrawLine(buf, 96, 32, 96, 52, 1);
                }
            }
            else if (tb_test_mode == TestMode_OPEN)
            {
                Paint_DrawString_EN_CenterAtX(buf, 96, 19, "未连接", &Font12, 1);
                // 上面两端竖线

                DrawLine(buf, 96, 32, 96, 38, 1);

                // 两个X
                DrawLine(buf, 92, 41, 100, 49, 1);
                DrawLine(buf, 100, 41, 92, 49, 1);
            }
        }
        // Paint_DrawString_EN_CenterAtX(buf, 32, 19, ta_v_str, &Font12, 1);
        // Paint_DrawString_EN_CenterAtX(buf, 96, 19, tb_v_str, &Font12, 1);
        // Paint_DrawString_EN_CenterAtX(buf, 32, 35, ta_f_str, &Font12, 1);
        // Paint_DrawString_EN_CenterAtX(buf, 96, 35, tb_f_str, &Font12, 1);
        // Paint_DrawString_EN_CenterAtX(buf, 32, 51, ta_duty_str, &Font12, 1);
        // Paint_DrawString_EN_CenterAtX(buf, 96, 51, tb_duty_str, &Font12, 1);

        if (ta_test_mode != TestMode_OPEN && ta_test_mode != TestMode_UNKNOWN)
        {
            InvertRect(buf, 0, 0, 64, 16);
        }
        if (tb_test_mode != TestMode_OPEN && tb_test_mode != TestMode_UNKNOWN)
        {
            InvertRect(buf, 64, 0, 64, 16);
        }

        if (getdraw == 2 &&
            ta_test_mode != TestMode_SHORT && tb_test_mode != TestMode_SHORT) // 刚刚完成计算 重新启用中断
        {
            last_refresh_time = current_time;
            if (!highspeed_ta_flag)
            {
                gpio_set_irq_enabled(TAINPIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
            }

            if (!highspeed_tb_flag)
            {
                gpio_set_irq_enabled(TBINPIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
            }
        }

        return true;
    }
    return false;
}

#include "menu/key.hpp"

// 约2s的中断，检测两种状态下的值，求平均值进行归零设定
void correct_zero()
{

    gpio_put(PULLUP_EN_PIN, 1); // 关闭上拉
                                // 内部上拉+外部上拉采样
    ADCReadResult res1 = adc_read_refresh(false);

    // 内部下拉+禁用外部上拉采样

    gpio_put(PULLUP_EN_PIN, 0); // 关闭上拉

    ADCReadResult res2 = adc_read_refresh(false);

    // 恢复在最后
    float res1_tb_vol = read_adc_voltage(res1.tb);

    float res1_ta_vol = read_adc_voltage(res1.ta);

    float res2_tb_vol = read_adc_voltage(res2.tb);

    float res2_ta_vol = read_adc_voltage(res2.ta);
}

void general_read()
{
    // 重新进入一定不使用窗口
    generalreadIsRunning = false;
    extern void ResetOpn(void);
    extern bool opnPCchangemode;

    int xEnd = -3;
    int yEnd = -1;
    int widthEnd = SSD1306_WIDTH + 3;
    int heighEND = SSD1306_HEIGHT + 2;

    lengthStart = widthEnd;
    heightStart = heighEND;
    yStart = yEnd;
    xStart = xEnd;

    // 进入菜单后直接聚焦到小窗口，显示安全提示；3秒或任意按键后自动关闭
    if (footlength != 1)
    {
        int win_width = 116;
        int win_heigh = 40;
        int win_x = (SCREEN_WIDTH - win_width) / 2;
        int win_y = (SCREEN_HEIGHT - win_heigh) / 2 + 1;

        // 进入动画（主界面 -> 聚焦弹窗）
        for (float i = 0; i <= 1; i += footlength)
        {
            int x = easeInOutQuad(xStart, win_x, i);
            int y = easeInOutQuad(yStart, win_y, i);
            int width = easeInOutQuad(lengthStart, win_width, i);
            int heigh = easeInOutQuad(heightStart, win_heigh, i);

            draw_general_read(1);
            ApplyBlurEffect(buf, X2line_up(0, 1, i));
            UIDrawSgate(x, y, width, heigh, 8, 6, 1, 2);
            draw_generalread_notice_window();

            render(buf, &frame_area);
            memset(buf, 0, SSD1306_BUF_LEN);
        }

        xStart = win_x;
        yStart = win_y;
        lengthStart = win_width;
        heightStart = win_heigh;
        generalreadIsRunning = true;

        uint32_t notice_start_us = time_us_32();
        while (true)
        {
            if (opnLeft || opnRight || opnPCchangemode)
            {
                sound_mode = none;
                return;
            }

            bool any_key = opnEnter || opnExit || opnUp || opnDown || opnLeft || opnRight || opnCtrl || opnCtrlUp || opnCtrlDown || opnPCchangemode;

            draw_general_read(1);
            ApplyBlurEffect(buf, 1);
            UIDrawSgate(xStart, yStart, lengthStart, heightStart, 8, 6, 1, 2);
            draw_generalread_notice_window();
            render(buf, &frame_area);
            memset(buf, 0, SSD1306_BUF_LEN);

            if (any_key || (uint32_t)(time_us_32() - notice_start_us) >= 2000000u)
            {
                ResetOpn();
                break;
            }
        }

        // 退出动画（聚焦弹窗 -> 主界面）
        for (float i = 0; i <= 1; i += footlength)
        {
            int x = easeInOutQuad(xStart, xEnd, i);
            int y = easeInOutQuad(yStart, yEnd, i);
            int width = easeInOutQuad(lengthStart, widthEnd, i);
            int heigh = easeInOutQuad(heightStart, heighEND, i);

            draw_general_read(1);
            ApplyBlurEffect(buf, X2line_down(0.8, 0, i));
            UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);

            render(buf, &frame_area);
            memset(buf, 0, SSD1306_BUF_LEN);
        }

        xStart = xEnd;
        yStart = yEnd;
        lengthStart = widthEnd;
        heightStart = heighEND;
        generalreadIsRunning = false;
    }

    while (true)
    {

        if (opnLeft || opnRight ||opnPCchangemode)
        {
            sound_mode = none;
            // ResetOpn(); // 这里不需要在此处重置，放在function里
            return;
        }

        // if (opnEnter || opnExit)
        // {
        //     ResetOpn();
        //     if (footlength != 1)
        //     {
        //         int xEnd, yEnd, widthEnd, heighEND;
        //         if (!generalreadIsRunning) // 进入窗口
        //         {
        //             widthEnd = 84;
        //             heighEND = 32;
        //             xEnd = (SCREEN_WIDTH - widthEnd) / 2;
        //             yEnd = (SCREEN_HEIGHT - heighEND) / 2 + 1;
        //         }
        //         else // 退出窗口
        //         {
        //             xEnd = -3;
        //             yEnd = -1;
        //             widthEnd = SSD1306_WIDTH + 3;
        //             heighEND = SSD1306_HEIGHT + 2;
        //         }

        //         for (float i = 0; i <= 1; i += (footlength))
        //         {
        //             int x = easeInOutQuad(xStart, xEnd, i);
        //             int y = easeInOutQuad(yStart, yEnd, i);
        //             int width = easeInOutQuad(lengthStart, widthEnd, i);
        //             int heigh = easeInOutQuad(heightStart, heighEND, i);

        //             draw_general_read(1); // 绘制主内容背景

        //             if (!generalreadIsRunning) // 进入动画
        //             {
        //                 ApplyBlurEffect(buf, X2line_up(0, 1, i));
        //                 UIDrawSgate(x, y, width, heigh, 8, 6, 1, 2);
        //             }
        //             else // 退出动画
        //             {
        //                 ApplyBlurEffect(buf, X2line_down(0.8, 0, i));
        //                 UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);
        //             }

        //             if (!generalreadIsRunning)
        //             {
        //                 draw_generalread_setting_window();
        //             }

        //             render(buf, &frame_area);
        //             memset(buf, 0, SSD1306_BUF_LEN);
        //         }

        //         xStart = xEnd;
        //         yStart = yEnd;
        //         lengthStart = widthEnd;
        //         heightStart = heighEND;
        //     }

        //     { // 按键进入
        //         generalreadIsRunning = !generalreadIsRunning;
        //     }
        // }

        if (generalreadIsRunning)
        {
            draw_general_read(1);
            ApplyBlurEffect(buf, 1);
            UIDrawSgate(xStart, yStart, lengthStart, heightStart, 8, 6, 1, 2);
            draw_generalread_setting_window();
            render(buf, &frame_area);
            memset(buf, 0, SSD1306_BUF_LEN);
        }
        else if (draw_general_read(0))
        {
            render(buf, &frame_area);
            memset(buf, 0, SSD1306_BUF_LEN);
            extern bool send_pc_flag;
            send_pc_flag = true; // 触发向上位机更新数据
        }
    }
}

#endif
