#ifndef _ADC_JOYSTICK_CPP_
#define _ADC_JOYSTICK_CPP_

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "menu/ssd1306.hpp"
#include "menu/UI.hpp"
#include "menu/key.hpp"
#include "menu/core.hpp"
#include "read/uartconfig.hpp"
#include <cmath>
#include <cstring>
#include "CONFIG_FLO.hpp"

// 指数均值滤波参数 (α = 0.2)
#define FILTER_ALPHA 0.2f

// 滤波内部状态
static float filtered_x = 0.0f;
static float filtered_y = 0.0f;

// 用于文本显示的平滑值 (每200ms更新一次)
static int16_t text_x = 0;
static int16_t text_y = 0;

// Shared telemetry flags/data consumed by core sender.
volatile bool adcxy_tx_enable = false;
volatile bool adcxy_tx_pending = false;
volatile int16_t adcxy_tx_x = 0;
volatile int16_t adcxy_tx_y = 0;

// 上次文本更新时间 (微秒)
static uint64_t last_text_update_us = 0;

// 硬件配置

#define ADC_MAX 4095
#define JOY_AREA_SIZE 64 // 左侧摇杆显示区边长

// 菜单项索引（0=Zero Calib, 1=静态占位不可选, 2=Config）
static int nowselect_adc_item = 0; // 当前选中项，上下移动时跳过1
static int selsect_temp = 0;       // 暂存选中项（与原代码一致）

// 状态变量
static bool adcIsRunning = false; // 是否在Config子菜单中
static bool adc_infunction = false;
static bool adc_outfunction = false;

// 摇杆数据
static uint16_t raw_x = 2048, raw_y = 2048;
static int16_t offset_x = 0, offset_y = 0;   // 归零矫正偏移
static int16_t display_x = 0, display_y = 0; // 矫正后坐标

enum AdcDisplayMode
{
    ADC_DISPLAY_RAW = 0,
    ADC_DISPLAY_ZERO = 1
};

static AdcDisplayMode adc_display_mode = ADC_DISPLAY_RAW;

// UI动画参数（与原代码完全一致）
static uint16_t adc_item_height = 15;
static uint16_t adc_font_height = 12;
static uint16_t adc_font_width = 7;
static uint16_t adc_ChangeVal_Width = 84;
static uint8_t adc_itemHeightOffset = (adc_item_height - adc_font_height) / 2 + 1;

// 动画全局变量（与原代码相同）
// static int xStart = 0, yStart = 0, lengthStart = 0, heightStart = 0;
static int xEnd = 0, yEnd = 0, lengthEnd = 0, heightEnd = 0;
static int adc_menu_YPos_End = 0;
static bool adc_menu_need_draw_main = false;

// 函数声明
static void draw_adc_main();
static void read_joystick();
static void apply_zero_calibration();

// ---------- ADC初始化 ----------
static void adc_joystick_init()
{
    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN);
    adc_gpio_init(JOYSTICK_Y_PIN);
}

// 读取摇杆并应用零点
static void read_joystick()
{
    adc_select_input(0);
    raw_x = adc_read();
    adc_select_input(1);
    // 统一在输入层修正 Y 方向，确保归零/滤波/渲染使用同一坐标系。
    raw_y = (uint16_t)(ADC_MAX - adc_read());

    if (adc_display_mode == ADC_DISPLAY_ZERO)
    {
        display_x = (int16_t)raw_x - offset_x;
        display_y = (int16_t)raw_y - offset_y;
    }
    else
    {
        display_x = (int16_t)raw_x;
        display_y = (int16_t)raw_y;
    }

    // 指数均值滤波 (每次读取都更新滤波内部状态)
    filtered_x = FILTER_ALPHA * display_x + (1.0f - FILTER_ALPHA) * filtered_x;
    filtered_y = FILTER_ALPHA * display_y + (1.0f - FILTER_ALPHA) * filtered_y;
}

// 归零矫正：将当前位置设为零点
static void apply_zero_calibration()
{
    offset_x = raw_x;
    offset_y = raw_y;
}

static void toggle_adc_display_mode()
{
    if (adc_display_mode == ADC_DISPLAY_RAW)
    {
        adc_display_mode = ADC_DISPLAY_ZERO;
        apply_zero_calibration();
    }
    else
    {
        adc_display_mode = ADC_DISPLAY_RAW;
    }
}

// ---------- 绘制左侧摇杆区域（十字+实心圆）----------
static void draw_joystick_area(int left_x, int left_y, int size)
{
    // 辅助函数：限幅
    auto clamp = [](int val, int minv, int maxv) -> int
    {
        if (val < minv)
            return minv;
        if (val > maxv)
            return maxv;
        return val;
    };

    // 点的绝对坐标（基于原始 raw 值，与模式无关）
    int point_x = (raw_x * (size - 1)) / ADC_MAX;
    int point_y = (size - 1) - (((raw_y) * (size - 1)) / ADC_MAX);
    point_x = clamp(point_x, 0, size - 1);
    point_y = clamp(point_y, 0, size - 1);

    // 十字交叉线的坐标（模式相关）
    int origin_x, origin_y;
    if (adc_display_mode == ADC_DISPLAY_ZERO)
    {
        origin_x = (offset_x * (size - 1)) / ADC_MAX;
        origin_y = (size - 1) - ((offset_y * (size - 1)) / ADC_MAX);
    }
    else
    {
        origin_x = 0;
        origin_y = size - 1;
    }
    origin_x = clamp(origin_x, 0, size - 1);
    origin_y = clamp(origin_y, 0, size - 1);

    // 绘制十字线（水平 + 垂直）
    DrawLine(buf, left_x + origin_x, left_y, left_x + origin_x, left_y + size - 1, 1);
    DrawLine(buf, left_x, left_y + origin_y, left_x + size - 1, left_y + origin_y, 1);

    // 绘制实心圆点（标记摇杆当前位置）
    int r = 3;
    DrawEllipse(buf, left_x + point_x, left_y + point_y, r, r, 0, 1);
}

// ---------- 绘制主界面（左侧摇杆 + 右侧菜单）----------
static void draw_adc_main()
{
    // 清空全屏偏移区域
    // DrawRectangle(buf, 0, Ypos, 128, 64, 1, 0);

    // 左侧64x64摇杆区
    draw_joystick_area(0, 0, JOY_AREA_SIZE);

    // 右侧菜单区域（从x=64开始）
    int right_x = JOY_AREA_SIZE;
    int right_y = 0;
    int line_h = 15;
    int y_base = right_y + 4;

    // 清空右侧
    // DrawRectangle(buf, right_x, right_y, 128 - JOY_AREA_SIZE, 64, 1, 0);

    // 第一行：坐标（居中）
    char coord_str[20];
    snprintf(coord_str, sizeof(coord_str), "%+5d %+5d", text_x, text_y);
    Paint_DrawString_EN_CenterAtX(buf, right_x + 24, y_base, coord_str, &Font12, 1);

    // 第二行：模式切换
    const char *mode_str = (adc_display_mode == ADC_DISPLAY_ZERO) ? "归零" : "原始";
    Paint_DrawString_EN_CenterAtX(buf, right_x + 32, y_base + line_h, mode_str, &Font12, 1);

    // 第三行：静态文本 ->UART/USB（不可选）
    Paint_DrawString_EN_CenterAtX(buf, right_x + 32, y_base + line_h * 2, "->USB", &Font12, 1);
    // 第四行：进入 UART 配置页
    Paint_DrawString_EN_CenterAtX(buf, right_x + 32, y_base + line_h * 3, "->UART", &Font12, 1);

    // 分割线（可选）
    // DrawLine(buf, JOY_AREA_SIZE - 1, 0, JOY_AREA_SIZE - 1, 0 + 63, 1);
}

// ---------- 动画与渲染函数 ----------
static void anni_adc()
{
    // 仅两个高亮目标：归零/原始切换，UART 配置
    if (nowselect_adc_item == 0)
    {
        xEnd = 68;
        yEnd = 17;
        lengthEnd = 58;
        heightEnd = 12;
    }
    else
    {
        xEnd = 68;
        yEnd = 48;
        lengthEnd = 58;
        heightEnd = 12;
    }
    adc_menu_YPos_End = 0;

    bool no_anim = (xEnd == xStart && yEnd == yStart && lengthEnd == lengthStart && heightEnd == heightStart);
    if (!no_anim && footlength != 1)
    {
        for (float i = 0; i <= 1; i += footlength)
        {
            int x = easeInOutQuad(xStart, xEnd, i);
            int y = easeInOutQuad(yStart, yEnd, i);
            int w = easeInOutQuad(lengthStart, lengthEnd, i);
            int h = easeInOutQuad(heightStart, heightEnd, i);

            // 绘制主界面背景
            draw_adc_main();

            UIDrawSgate(x, y, w, h, 8, 6, 1, 1);

            render(buf, &frame_area);
            memset(buf, 0, SSD1306_BUF_LEN);
        }
    }

    // 更新起始值
    lengthStart = lengthEnd;
    heightStart = heightEnd;
    yStart = yEnd;
    xStart = xEnd;
    adc_menu_need_draw_main = false;

    // 最终静态绘制
    draw_adc_main();
    UIDrawSgate(xEnd, yEnd, lengthEnd, heightEnd, 8, 6, 1, 1);
    render(buf, &frame_area);
    memset(buf, 0, SSD1306_BUF_LEN);
}

// ---------- 主函数（完全模仿 serialread 结构）----------
void adc_joystick_main()
{
    // adc_joystick_init();
    ResetOpn();
    adcxy_tx_enable = true;
    adcxy_tx_pending = false;

    read_joystick();
    if (adc_display_mode == ADC_DISPLAY_ZERO)
    {
        apply_zero_calibration();
    }

    if (nowselect_adc_item != 0 && nowselect_adc_item != 2)
    {
        nowselect_adc_item = 0;
    }

    anni_adc();

    while (true)
    {
        // 实时读取摇杆数据（无论何种模式）
        read_joystick();

        uint64_t now_us = time_us_64();
        if (now_us - last_text_update_us >= 200000)
        { // 200000微秒 = 200ms
            text_x = (int16_t)roundf(filtered_x);
            text_y = (int16_t)roundf(filtered_y);
            adcxy_tx_x = text_x;
            adcxy_tx_y = text_y;
            extern bool send_pc_flag;
            send_pc_flag = true;
            last_text_update_us = now_us;
        }

        extern bool opnPCchangemode;
        extern bool opnPCchangetool;
        if (opnPCchangetool || opnEnter || opnExit || opnUp || opnDown || opnLeft || opnRight || opnPCchangemode)
        {
            if (opnUp || opnDown)
            {
                nowselect_adc_item = (nowselect_adc_item == 0) ? 2 : 0;
                adc_menu_need_draw_main = true;
            }
            else if (opnEnter)
            {

                if (nowselect_adc_item == 0)
                {
                    toggle_adc_display_mode();
                    extern bool send_pc_flag;
                    send_pc_flag = true; // 触发向上位机更新数据
                }
                else if (nowselect_adc_item == 2)
                {
                    bool uartconfigResult = uartconfig();
                    extern bool send_pc_flag;
                    send_pc_flag = true; // 触发向上位机更新数据
                    if (uartconfigResult)
                    {
                        adcxy_tx_enable = false;
                        adcxy_tx_pending = false;
                        return;
                    }
                }
            }
            else if (opnExit || opnLeft || opnRight || opnPCchangemode || opnPCchangetool)
            {
                adcxy_tx_enable = false;
                adcxy_tx_pending = false;
                return;
            }
            ResetOpn();
        }

        // 运行动画
        anni_adc();

    }
}

int adcxy_get_display_mode_code(void)
{
    return (int)adc_display_mode;
}

void adcxy_set_display_mode_code(int mode)
{
    if (mode == 0)
    {
        adc_display_mode = ADC_DISPLAY_RAW;
    }
    else
    {
        adc_display_mode = ADC_DISPLAY_ZERO;
        apply_zero_calibration();
    }
}

#endif // _ADC_JOYSTICK_CPP_
