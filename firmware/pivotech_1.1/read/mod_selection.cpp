#ifndef _MAIN_MENU_CPP_
#define _MAIN_MENU_CPP_

#include "pico/stdlib.h"
#include "menu/ssd1306.hpp"
#include "menu/UI.hpp"
#include "menu/key.hpp"
#include "CONFIG_FLO.hpp"
#include "menu/core.hpp"
#include "read/encoderread.hpp"
#include "read/hc05.hpp"
#include "read/adcxy.hpp"
#include "read/ssd1306_demo.hpp"
#include "read/mpu6050_demo.hpp"
#include "read/select.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

bool opnPCchangetool = false; // 触发向上位机发送数据的标志
bool if_enter_tool = false;   // 是否进入工具界面（区别于模式切换界面）
extern bool i2c_run_in_core1; // 来自 hardware_core1.cpp 的全局变量，用于控制核心1的测试系统是否运行

// ---------- 菜单项配置 ----------
#define MENU_ITEM_COUNT 5

// 菜单项名称
static const char *menu_items[MENU_ITEM_COUNT] = {
    "A&B相位编码器",
    "SSD1306",
    "MPU6050",
    "HC-05 AT模式配置",
    "摇杆X/Y ADC转发"};

// 每个菜单项的高亮框坐标和尺寸
// 5项布局：
// 0 第一行全宽
// 1 第二行左半
// 2 第二行右半
// 3 第三行全宽
// 4 第四行全宽
static const struct
{
    uint8_t x;
    uint8_t y;
    uint8_t w;
    uint8_t h;
} item_rects[MENU_ITEM_COUNT] = {
    {1, 1, 125, 12},  // 第一行
    {1, 17, 62, 12},  // 第二行左侧
    {65, 17, 62, 12}, // 第二行右侧
    {1, 33, 125, 12}, // 第三行
    {1, 49, 125, 12}  // 第四行
};

// 当前选中的菜单项 (0~4)
int nowselect_moditem = 0;

// 动画全局变量（完全复刻原代码）
// static int xStart = 0, yStart = 0, lengthStart = 0, heightStart = 0;
static int xEnd = 0, yEnd = 0, lengthEnd = 0, heightEnd = 0;
static int menu_YPos_Start = 0, menu_YPos_End = 0;
static const uint16_t menu_modal_h = 30;
static const uint16_t menu_modal_w = 92;

// ---------- 绘制主界面（完全模仿原 draw_serialread_main 的结构）----------
static void draw_main_menu(int Ypos)
{
    // 清空屏幕（带偏移）
    DrawRectangle(buf, 0, Ypos, SSD1306_WIDTH, SSD1306_HEIGHT, 1, 0);

    // 4行主布局，第2行分成左右两个选项
    Paint_DrawString_EN_CenterAtX(buf, 64, Ypos + 3, (char *)menu_items[0], &Font12, 1);
    Paint_DrawString_EN_CenterAtX(buf, 32, Ypos + 19, (char *)menu_items[1], &Font12, 1);
    Paint_DrawString_EN_CenterAtX(buf, 96, Ypos + 19, (char *)menu_items[2], &Font12, 1);
    Paint_DrawString_EN_CenterAtX(buf, 64, Ypos + 35, (char *)menu_items[3], &Font12, 1);
    Paint_DrawString_EN_CenterAtX(buf, 64, Ypos + 51, (char *)menu_items[4], &Font12, 1);

    // 分割线（仿原代码风格）
    DrawLine(buf, 0, Ypos + 15, 127, Ypos + 15, 1);
    DrawLine(buf, 0, Ypos + 31, 127, Ypos + 31, 1);
    DrawLine(buf, 0, Ypos + 47, 127, Ypos + 47, 1);
}

// ---------- 动画和渲染函数（完全复刻 anni_serialread 的逻辑，但去掉运行状态分支）----------
static void anni_main_menu()
{
    // 根据当前选中项确定目标高亮框的位置和大小
    xEnd = item_rects[nowselect_moditem].x;
    yEnd = item_rects[nowselect_moditem].y;
    lengthEnd = item_rects[nowselect_moditem].w;
    heightEnd = item_rects[nowselect_moditem].h;
    menu_YPos_End = 0;

    // 检查是否需要动画
    bool if_no_animation = (xEnd == xStart && yEnd == yStart && lengthEnd == lengthStart && heightEnd == heightStart);
    if (!if_no_animation && footlength != 1)
    {
        for (float i = 0; i <= 1; i += footlength)
        {
            int x = easeInOutQuad(xStart, xEnd, i);
            int y = easeInOutQuad(yStart, yEnd, i);
            int w = easeInOutQuad(lengthStart, lengthEnd, i);
            int h = easeInOutQuad(heightStart, heightEnd, i);
            int y_offset = X2line_down(menu_YPos_Start, menu_YPos_End, i);

            // 绘制主界面背景
            draw_main_menu(y_offset);

            // 绘制滑动的 SGate 高亮框（无模糊）
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
    menu_YPos_Start = menu_YPos_End;

    // 最终静态绘制
    draw_main_menu(menu_YPos_End);
    UIDrawSgate(xEnd, yEnd, lengthEnd, heightEnd, 8, 6, 1, 1);
    render(buf, &frame_area);
    memset(buf, 0, SSD1306_BUF_LEN);
}

void mod_process_enter()
{
    // 这里根据 nowselect_moditem 的值进入对应的子菜单
    switch (nowselect_moditem)
    {
    case 0:
        // 进入 A&B phase encoder 菜单
        Selector_TA_TX_TB_RX();
        encoder_init();
        encoder_read();
        encoder_Deinit();
        break;
    case 1:
        // 进入 SSD1306 菜单
        Selector_TA_TX_TB_RX();
        i2c_run_in_core1 = true;
        ssd1306_demo_main();
        i2c_run_in_core1 = false;
        break;
    case 2:
        // 进入 MPU6050 菜单
        Selector_TA_TX_TB_RX();
        i2c_run_in_core1 = true;
        mpu6050_demo_main();
        i2c_run_in_core1 = false;
        break;
    case 3:
        // 进入 HC-05 AT mode 菜单
        Selector_TA_TX_TB_RX();
        hc05_at_mode();
        break;
    case 4:
        // 进入 Stick X&Y ADC 菜单
        extern bool core1_usb_connect_uart; // 来自 menu/core.hpp 的全局变量，用于通知核心1是否启用USB转UART功能
        core1_usb_connect_uart = true;      // 通知core1启用USB转UART功能 同步广播ADC
        Selector_Read_XY_ADC();
        adc_joystick_main();
        Selector_TA_TX_TB_RX();
        core1_usb_connect_uart = false; // 通知core1关闭USB转UART功能
        break;
    default:
        break;
    }
}

// ---------- 主函数（完全模仿 serialread 的按键处理结构）----------
extern "C" void main_mod_selection(void)
{
    ResetOpn();
    extern bool send_pc_flag;
    send_pc_flag = true; // 触发向上位机更新数据

    menu_YPos_Start = 0;
    if_enter_tool = false;
    anni_main_menu();

    while (true)
    {
        if (opnPCchangetool || opnEnter || opnExit || opnUp || opnDown || opnCtrl || opnCtrlUp || opnCtrlDown)
        {
            if (opnUp)
            {
                nowselect_moditem--;
                if (nowselect_moditem < 0)
                    nowselect_moditem = MENU_ITEM_COUNT - 1;
                extern bool send_pc_flag;
                send_pc_flag = true; // 触发向上位机更新数据
            }
            else if (opnDown)
            {
                nowselect_moditem++;
                if (nowselect_moditem >= MENU_ITEM_COUNT)
                    nowselect_moditem = 0;
                extern bool send_pc_flag;
                send_pc_flag = true; // 触发向上位机更新数据
            }

            else if (opnExit)
            {
                ResetOpn();
                // return; //不退出mode
            }

            else if (opnEnter || opnPCchangetool)
            {
                opnPCchangetool = false; // 重置工具触发标志，避免重复触发
                // 阻塞进入子菜单占位页，接口逻辑后续再接
                if_enter_tool = true;
                mod_process_enter();
                if_enter_tool = false;
                extern bool send_pc_flag;
                send_pc_flag = true; // 触发向上位机更新数据
            }
        }
        // opnleft right 会传递至此
        extern bool opnPCchangemode;
        if (opnLeft || opnRight || opnPCchangemode)
        {
            // 退出串口读取程序
            return;
        }
        ResetOpn();

        // 运行动画和渲染
        anni_main_menu();
    }
}

#endif // _MAIN_MENU_CPP_
