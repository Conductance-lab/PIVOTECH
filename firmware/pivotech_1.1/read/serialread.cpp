

#ifndef _SERIALREAD_CPP_
#define _SERIALREAD_CPP_

#include "pico/stdlib.h"
#include "menu/ssd1306.hpp"
#include "menu/UI.hpp"
#include "menu/key.hpp"
#include "hardware/uart.h"
#include "CONFIG_FLO.hpp"
#include "menu/core.hpp"

#include "read/uartconfig.hpp"

#include <cmath>
#include <cstdint>
#include <string.h>

// 串口监视器配置
#define SERIAL_MONITOR_LINES 4
#define SERIAL_MAX_CHARS_PER_LINE 20

// 状态变量
bool serialreadIsRunning = false;
bool serialread_infunction = false;
bool serialread_outfunction = false;

// 当前选择项目 (0-2: TX/RX交换, 波特率选择器, 串口监视器)
int nowselect_serialread_item = 0;

// 全局变量：TA和TB的串口配置 (0=RXD, 1=TXD)
bool global_TA_serial_mode = 1;

// 全局变量：波特率配置 (0=9600, 1=19200, 2=38400, 3=57600, 4=115200)
int global_baudrate_selection = 4; // 默认115200

// 波特率数组
static const int baudrate_values[] = {9600, 19200, 38400, 57600, 115200};
static char *baudrate_names[] = {"9600", "19200", "38400", "57600", "115200"};
static char *serial_mode_names[] = {"RXD", "TXD"};

// UI参数
uint16_t serial_item_height = 15;
uint16_t serial_font_height = 12;
uint16_t serial_font_width = 7;
uint16_t serial_ChangeVal_Width = 84;
static uint8_t serial_itemHeightOffset = (serial_item_height - serial_font_height) / 2 + 1;

// 绘制主界面
void draw_serialread_main(int Ypos)
{

    // 清空屏幕
    DrawRectangle(buf, 0, Ypos, 128, 64, 1, 0);

    // 第一行：TA和TB串口状态（使用大字体，仅显示TA TB）
    DrawRectangle(buf, 0, Ypos, 64, 16, 0, 1);
    DrawRectangle(buf, 64, Ypos, 64, 16, 0, 1);
    DrawLine(buf, 0, Ypos + 31, 127, Ypos + 31, 1);
    Paint_DrawString_EN_CenterAtX(buf, 32, Ypos + 2, "TA", &Font16, 1);
    Paint_DrawString_EN_CenterAtX(buf, 96, Ypos + 2, "TB", &Font16, 1);

    // 根据串口输入输出状态进行反转显示
    if (global_TA_serial_mode == 0 && RXisReady)
    { // TXD模式且可写
        InvertRect(buf, 0, Ypos, 64, 32);
    }
    else if (global_TA_serial_mode == 1 && TXisReady)
    { // RXD模式且有数据接收
        InvertRect(buf, 0, Ypos, 64, 32);
    }

    if (global_TA_serial_mode == 1 && RXisReady)
    { // TXD模式且可写
        InvertRect(buf, 64, Ypos, 64, 32);
    }
    else if (global_TA_serial_mode == 0 && TXisReady)
    { // RXD模式且有数据接收
        InvertRect(buf, 64, Ypos, 64, 32);
    }

    TXisReady = 0;
    RXisReady = 0;

    // 第二行：直接显示当前模式（在菜单页面直接显示）
    Paint_DrawString_EN_CenterAtX(buf, 32, Ypos + 19, serial_mode_names[global_TA_serial_mode], &Font12, 1);
    Paint_DrawString_EN_CenterAtX(buf, 96, Ypos + 19, serial_mode_names[1 - global_TA_serial_mode], &Font12, 1);
    if (nowselect_serialread_item == 0)
    {
        // 绘制“<->”
        Paint_DrawString_EN_CenterAtX(buf, 64, Ypos + 19, "<->", &Font12, 1);
    }

    // 第三行：波特率设置（居中，2行窗口）
    char baud_str[20];
    build_active_summary(baud_str, sizeof(baud_str));
    Paint_DrawString_EN_CenterAtX(buf, 64, Ypos + 35, baud_str, &Font12, 1);

    // 第四行：串口监视器
    Paint_DrawString_EN_CenterAtX(buf, 64, Ypos + 51, "串口监视", &Font12, 1);

    // 分割线
    DrawLine(buf, 0, Ypos + 15, 127, Ypos + 15, 1);
    DrawLine(buf, 0, Ypos + 31, 127, Ypos + 31, 1);
    DrawLine(buf, 0, Ypos + 47, 127, Ypos + 47, 1);
    DrawLine(buf, 0, Ypos + 64, 127, Ypos + 64, 1);
}

int serial_monitor_YPos_End = 0;
int serial_monitor_YPos_Start = 0;

static bool serial_monitor_paused = false;
// 暂停状态下的静态数据缓存
static char paused_buffer[SERIAL_RX_BUFFER_SIZE];
static int paused_index = 0;
static bool paused_data_captured = false;

// 绘制串口监视器窗口（可偏移的绘制函数）
void draw_serial_monitor(int Ypos)
{
    static uint32_t paused_us = 0;

// 定义显示参数
#define SERIAL_SHOW_LINES 7
#define SERIAL_MAX_CHARS_PER_LINE 25
#define SERIAL_FONT_WIDTH 5
#define SERIAL_FONT_HEIGHT 8

    // 获取当前时间（微秒）
    uint32_t current_time_us = time_us_32();

    // 根据暂停状态选择数据源
    char *src_buf;
    int src_idx;

    if (serial_monitor_paused)
    {
        // 暂停状态：使用静态缓存数据
        if (!paused_data_captured)
        {
            // 第一次暂停，复制当前数据到静态缓存
            memcpy(paused_buffer, serial_rx_buffer, SERIAL_RX_BUFFER_SIZE);
            paused_index = serial_rx_index;
            paused_data_captured = true;
            paused_us = current_time_us; // 记录暂停时间
        }
        src_buf = paused_buffer;
        src_idx = paused_index;
    }
    else
    {
        // 非暂停状态：使用实时数据
        src_buf = serial_rx_buffer;
        src_idx = serial_rx_index;
        paused_data_captured = false; // 重置暂停数据标志
        paused_us = 0;
    }

    // --- 1. 文本处理部分（环形缓冲区处理）---
    char linear_buf[SERIAL_RX_BUFFER_SIZE + 1]; // +1 to ensure null-termination
    int total_len = 0;

    // 确保 src_idx 在合法范围内
    src_idx = src_idx % SERIAL_RX_BUFFER_SIZE;

    if (src_idx < SERIAL_RX_BUFFER_SIZE)
    {
        // 从环形缓冲区的当前位置开始重新排列数据
        // 先复制从当前位置到缓冲区末尾的数据
        int first_part_len = SERIAL_RX_BUFFER_SIZE - src_idx;
        memcpy(linear_buf, src_buf + src_idx, first_part_len);
        // 再复制从缓冲区开始到当前位置的数据
        memcpy(linear_buf + first_part_len, src_buf, src_idx);
        total_len = SERIAL_RX_BUFFER_SIZE;
    }

    // 确保缓冲区以 '\0' 结尾
    linear_buf[total_len] = '\0';

// --- 切分成行（正序处理，支持无限滚动），每行最多25字 ---
// 使用更大的缓冲区来存储更多行，实现真正的滚动
#define MAX_DISPLAY_LINES 50 // 最大存储行数
    static char all_lines[MAX_DISPLAY_LINES][SERIAL_MAX_CHARS_PER_LINE + 1];
    static int total_line_count = 0;

    // 清空所有行
    for (int l = 0; l < MAX_DISPLAY_LINES; l++)
    {
        all_lines[l][0] = '\0';
    }

    // 从最早的数据开始处理，按正序填充行
    int i = 0;
    int current_line = 0;
    int current_pos = 0;

    while (i < total_len && current_line < MAX_DISPLAY_LINES)
    {
        char ch = linear_buf[i];

        if (ch == '\n')
        {
            // 遇到换行符，结束当前行
            all_lines[current_line][current_pos] = '\0';
            current_line++;
            current_pos = 0;
            i++;
            continue;
        }

        // 检查是否为可显示字符
        if (ch >= 32 && ch <= 126)
        {
            if (current_pos < SERIAL_MAX_CHARS_PER_LINE)
            {
                all_lines[current_line][current_pos++] = ch;
            }
            else
            {
                // 当前行已满，换到下一行
                all_lines[current_line][current_pos] = '\0';
                current_line++;
                if (current_line < MAX_DISPLAY_LINES)
                {
                    current_pos = 0;
                    all_lines[current_line][current_pos++] = ch;
                }
            }
        }
        else if (ch == '\t')
        {
            // 制表符转为空格
            if (current_pos < SERIAL_MAX_CHARS_PER_LINE)
            {
                all_lines[current_line][current_pos++] = ' ';
            }
        }

        i++;
    }

    // 结束最后一行
    if (current_line < MAX_DISPLAY_LINES)
    {
        all_lines[current_line][current_pos] = '\0';
        total_line_count = current_line + 1;
    }
    else
    {
        total_line_count = MAX_DISPLAY_LINES;
    }

    // --- 2. 文本显示，y=ln*9，每行占8+1像素 ---
    // --- 2. 文本显示，最新行在最下面（第7行），实现自动上移滚动 ---
    // 计算显示起始位置：如果行数超过显示限制，只显示最后的SERIAL_SHOW_LINES行
    int display_start = (total_line_count > SERIAL_SHOW_LINES) ? total_line_count - SERIAL_SHOW_LINES : 0;

    for (int ln = 0; ln < SERIAL_SHOW_LINES; ln++)
    {
        int line_idx = display_start + ln;
        if (line_idx >= total_line_count)
            break;

        char *str = all_lines[line_idx];
        int len = strlen(str);

        int x_start = 1;
        int y_draw = Ypos + 64 + ln * (SERIAL_FONT_HEIGHT + 1);

        if (len > 0)
        {
            Paint_DrawString_EN(buf, x_start, y_draw, str, &Font8, 1);
        }

        // 在最后一行的末尾添加闪烁光标（仅在非暂停状态下）
        if (!serial_monitor_paused && line_idx == total_line_count - 1)
        {
            // 使用时间判断光标闪烁，每500ms闪烁一次
            if ((current_time_us / 500000) % 2 == 0)
            {
                int cursor_x = x_start + len * SERIAL_FONT_WIDTH;
                if (cursor_x < 125)
                {
                    Paint_DrawString_EN(buf, cursor_x, y_draw, "_", &Font8, 1);
                }
            }
        }
    }

    // --- 3. 进度条（第8行），使用统一的时间判断 ---
    int progress_y = Ypos + 64 + SERIAL_SHOW_LINES * (SERIAL_FONT_HEIGHT + 1);
    uint8_t progress;

    if (serial_monitor_paused)
    {
        // 暂停时，使用暂停时间计算进度条位置
        progress = (paused_us % 1000000) * 128 / 1000000;
    }
    else
    {
        // 非暂停时，使用当前时间计算进度条位置
        progress = (current_time_us % 1000000) * 128 / 1000000;
    }

    if (progress > 0)
    {
        int line_start = progress % SSD1306_WIDTH;
        int line_end = (progress + 20) % SSD1306_WIDTH;

        if (line_end > line_start)
        {
            // 正常情况，线条没有跨越屏幕边界
            DrawLine(buf, line_start, progress_y, line_end, progress_y, 1);
        }
        else
        {
            // 线条跨越了屏幕边界，需要分成两段绘制
            DrawLine(buf, line_start, progress_y, SSD1306_WIDTH - 1, progress_y, 1);
            DrawLine(buf, 0, progress_y, line_end, progress_y, 1);
        }
    }

    // 底部提示信息
}

bool serial_monitor_but_need_draw_main = 0;
// 动画和渲染函数
void anni_serialread()
{
    int xEnd, yEnd, widthEnd, heighEND;

    if (serialreadIsRunning == false)
    {
        // 根据选择项目确定高亮区域
        switch (nowselect_serialread_item)
        {
        case 0: // TX/RX交换
            xEnd = 1;
            yEnd = 17;
            widthEnd = 125;
            heighEND = 12;
            serial_monitor_YPos_End = 0;
            break;
        case 1: // 波特率选择器
            xEnd = 1;
            yEnd = 33;
            widthEnd = 125;
            heighEND = 12;
            serial_monitor_YPos_End = 0;
            break;
        case 2: // 串口监视器
            xEnd = 0;
            yEnd = 0;
            widthEnd = SCREEN_WIDTH - 1;
            heighEND = SCREEN_HEIGHT - 1;
            serial_monitor_YPos_End = -64;
            break;
        default:
            xEnd = 1;
            yEnd = 1;
            widthEnd = 126;
            heighEND = 12;
            serial_monitor_YPos_End = 0;
            break;
        }
    }
    else
    {
        // 运行状态下的窗口区域
        switch (nowselect_serialread_item)
        {
        case 1: // 波特率选择器
            heighEND = serial_item_height * 2;
            widthEnd = serial_ChangeVal_Width;
            xEnd = (SCREEN_WIDTH - widthEnd) / 2;
            yEnd = (SCREEN_HEIGHT - heighEND) / 2;
            serial_monitor_YPos_End = 0;
            break;
        default:
            heighEND = serial_item_height * 2;
            widthEnd = serial_ChangeVal_Width;
            xEnd = (SCREEN_WIDTH - widthEnd) / 2;
            yEnd = (SCREEN_HEIGHT - heighEND) / 2;
            serial_monitor_YPos_End = 0;
            break;
        }
    }

    if (serialread_infunction)
    {
        seedvalue = to_ms_since_boot(get_absolute_time());
    }

    // 判定如果xEnd,yEnd,widthEnd,heighEND与start相同则不进行动画
    bool if_no_animation = false;
    if (xEnd == xStart && yEnd == yStart && widthEnd == lengthStart && heighEND == heightStart)
    {
        if_no_animation = true;
    }

    if (!if_no_animation && footlength != 1 && !((serialreadIsRunning && !serialread_infunction) || (nowselect_serialread_item == 2 && !serial_monitor_but_need_draw_main)))
    {
        for (float i = 0; i <= 1; i += (footlength))
        {

            int x;
            int width;
            int heigh;

            {
                x = easeInOutQuad(xStart, xEnd, i);
                width = easeInOutQuad(lengthStart, widthEnd, i);
                heigh = easeInOutQuad(heightStart, heighEND, i);
            }

            int y = easeInOutQuad(yStart, yEnd, i);
            int serial_monitor_YPos = X2line_down(serial_monitor_YPos_Start, serial_monitor_YPos_End, i);

            if (serial_monitor_but_need_draw_main || (!serialreadIsRunning && nowselect_serialread_item != 2) || (serialreadIsRunning && serialread_infunction))
            {
                draw_serialread_main(serial_monitor_YPos);
            }

            if (serialreadIsRunning && !serialread_infunction)
            {
                ApplyBlurEffect(buf, 1); // 背景模糊层
            }
            else if (serialread_infunction && serialreadIsRunning)
            {
                ApplyBlurEffect(buf, X2line_up(0, 1, i)); // 背景模糊层
            }
            else if (serialread_outfunction)
            {
                ApplyBlurEffect(buf, X2line_down(0.8, 0, i)); // 背景模糊层
            }
            else if (!serialreadIsRunning && nowselect_serialread_item != 2)
            {
                // 无模糊 先画Sgate
                UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);
            }

            if (serialreadIsRunning)
            {

                UIDrawSgate(x, y, width, heigh, 8, 6, 1, 2);
            }
            else if (serialread_outfunction)
            {
                UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);
            }

            // if (serialreadIsRunning)
            // {
            //     if (nowselect_serialread_item == 1)
            //     {
            //         draw_baudrate_selector();
            //     }
            // }
            if (nowselect_serialread_item == 2)
            {
                draw_serial_monitor(serial_monitor_YPos);
                if (serial_monitor_but_need_draw_main)
                {
                    UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);
                }
            }

            render(buf, &frame_area);
            memset(buf, 0, SSD1306_BUF_LEN);
        }
    }

    lengthStart = widthEnd;
    heightStart = heighEND;
    yStart = yEnd;
    xStart = xEnd;
    serial_monitor_YPos_Start = serial_monitor_YPos_End;
    serialread_infunction = false;
    serialread_outfunction = false;
    serial_monitor_but_need_draw_main = false;

    // End静态绘制
    if (nowselect_serialread_item != 2)
    {
        draw_serialread_main(serial_monitor_YPos_End);
    }

    if (serialreadIsRunning)
    {

        ApplyBlurEffect(buf, 1); // 背景模糊层
        UIDrawSgate(xEnd, yEnd, widthEnd, heighEND, 8, 6, 1, 2);
    }
    if (!serialreadIsRunning && nowselect_serialread_item != 2)
    {
        // 无模糊 先画Sgate
        UIDrawSgate(xEnd, yEnd, widthEnd, heighEND, 8, 6, 1, 1);
    }

    // if (serialreadIsRunning)
    // {
    //     if (nowselect_serialread_item == 1)
    //     {
    //         draw_baudrate_selector();
    //     }
    // }

    if (nowselect_serialread_item == 2)
    {
        draw_serial_monitor(serial_monitor_YPos_End);
    }

    render(buf, &frame_area);
    memset(buf, 0, SSD1306_BUF_LEN);
}

int selsect_temp = 0;
// 主函数
void serialread()
{

    extern bool send_pc_flag;
    send_pc_flag = 1;

    if (nowselect_serialread_item == 2)
    {
        serial_monitor_YPos_Start = 0;
        serial_monitor_but_need_draw_main = 1;
    }
    ResetOpn();

    while (true)
    {

        if (opnEnter || opnExit || opnUp || opnDown)
        {
            if (nowselect_serialread_item == 2)
            {
                // 串口监视器逻辑
                if (opnUp && !serialreadIsRunning)
                {
                    nowselect_serialread_item--;
                    if (nowselect_serialread_item < 0)
                    {
                        nowselect_serialread_item = 0;
                    }
                }
                else if (opnDown && !serialreadIsRunning)
                {
                    nowselect_serialread_item++;
                    if (nowselect_serialread_item > 2)
                    {
                        nowselect_serialread_item = 2;
                    }
                }
                else if (opnExit)
                {
                    nowselect_serialread_item = selsect_temp;
                }
                else if (opnEnter)
                {
                    // 检查暂停/播放切换
                    serial_monitor_paused = !serial_monitor_paused;
                }

                // ResetOpn(); // 避免和后方冲突 提前清零
            }

            else
            {
                // 非串口监视器逻辑
                if (serialreadIsRunning)
                {
                    // 运行状态逻辑
                    if (opnExit)
                    {
                        serialreadIsRunning = false;
                        serialread_outfunction = 1;
                        ResetOpn(); // 避免和后方冲突 提前清零
                    }
                }
                else
                {
                    // 非运行状态逻辑
                    if (opnUp)
                    {
                        nowselect_serialread_item--;
                        if (nowselect_serialread_item < 0)
                        {
                            nowselect_serialread_item = 0;
                        }
                        if (nowselect_serialread_item == 2)
                        {
                            serial_monitor_but_need_draw_main = 1;
                        }
                    }
                    else if (opnDown)
                    {

                        nowselect_serialread_item++;
                        if (nowselect_serialread_item > 2)
                        {
                            nowselect_serialread_item = 2;
                        }

                        if (nowselect_serialread_item == 2)
                        {
                            serial_monitor_but_need_draw_main = 1;
                        }
                    }
                    else if (opnExit && nowselect_serialread_item != 2)
                    {

                        nowselect_serialread_item = 2;
                        serial_monitor_but_need_draw_main = 1;
                    }
                    else if (opnEnter && nowselect_serialread_item != 2)
                    {
                        // 根据当前选择项执行操作
                        switch (nowselect_serialread_item)
                        {
                        case 0:
                            // TX/RX交换逻辑
                            if (global_TA_serial_mode == 1)
                            {
                                global_TA_serial_mode = 0;
                                Selector_TA_TX_TB_RX();
                            }
                            else
                            {
                                global_TA_serial_mode = 1;
                                Selector_TA_RX_TB_TX();
                            }
                            extern bool send_pc_flag;
                            send_pc_flag = 1;

                            break;
                        case 1:
                            // 波特率选择器逻辑
                            serialreadIsRunning = true;

                            break;
                        default:
                            break;
                        }
                    }

                    if (nowselect_serialread_item != 2)
                    {
                        selsect_temp = nowselect_serialread_item;
                    }
                }
            }

            ResetOpn();
        }

        if (serialreadIsRunning && nowselect_serialread_item == 1)
        {
            uartconfig_infunction = 1;
            serialreadIsRunning = uartconfig();
        }

        extern bool opnPCchangemode;
        extern bool opnPCchangetool;
        if (opnLeft || opnRight ||opnPCchangemode || opnPCchangetool)
        {
            // 退出串口读取程序
            return;
        }
        // 在运行状态下持续更新显示
        anni_serialread();
    }
}

#endif
