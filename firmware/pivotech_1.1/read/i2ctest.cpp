#ifndef _I2CTEST_CPP_
#define _I2CTEST_CPP_

#include "pico/stdlib.h"
#include "menu/ssd1306.hpp"
#include "menu/UI.hpp"
#include "menu/key.hpp"
#include "hardware/i2c.h"
#include "CONFIG_FLO.hpp"
#include "menu/core.hpp"
#include "menu/StateLED.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

// PC 接口命令处理函数（在其他模块实现）
extern void handle_pc_interface_commands();

// 交互与窗口状态
bool i2ctestIsRunning = false;
bool i2ctest_infunction = false;
bool i2ctest_outfunction = false;

// 选项：0=SCL/SDA交换, 1=频率（左2/3），2=模式选择（右1/3），3=I2C Monitor
int nowselect_i2c_item = 0;
int i2ctest_select_temp = 0; // 记录非监视器选择项（避免与其他模块重名）

// 第一行 TA/TB 的 I2C 读写高亮指示（一次性闪烁）
// 与 hardwaretest 共享，避免重复定义
bool I2CWriteReady;
bool I2CReadReady;

// 提供外部设置的简易接口（若其他模块需要触发）
inline void i2c_set_write_ready() { I2CWriteReady = true; }
inline void i2c_set_read_ready() { I2CReadReady = true; }

// 第二行：SCL/SDA 左右位置（按下交换）
bool i2ctest_scl_on_left = true; // 与 hardwaretest 的同名变量区分

// 第三行：频率选择（预设），与 hardwaretest 共享
// 默认频率与历史一致
extern int32_t i2c_freq;
// SPI 频率供 Core1 测试信息使用（hardware_core1 需要）
extern int32_t spi_freq;
static const int i2ctest_preset_values[] = {100000, 400000, 1000000, 2000000};
static char *i2ctest_preset_names[] = {"100KHz", "400KHz", "1MHz", "2MHz"};
int i2ctest_global_i2c_freq_selection = 1; // 与默认频率同步（避免重名）
static int32_t i2c_freq_backup = 0;               // 进入窗口时备份（运行时同步）
static int32_t i2ctest_temp_i2c_freq = 0;         // 频率选择时的临时值（确认前不生效）

static void sync_i2c_freq_selection_from_value()
{
    int val = (int)i2c_freq;
    int sel = 1;
    for (int i = 0; i < 4; i++)
    {
        if (i2ctest_preset_values[i] >= val)
        {
            sel = i;
            break;
        }
    }
    i2ctest_global_i2c_freq_selection = sel;
}

// 第三行右：模式选择 (预留接口)
static void i2ctest_switch_mode()
{
    // 目前仅有主机模式 (M)，未来可在此扩展
    // StartStateLED(); 
}

// 外部窗口绘制接口（在此文件实现，避免链接缺失）
// Core1 测试 control interface (prototypes available in hardware_core1.hpp)
#include "read/hardware_core1.hpp"

// 第四行：I2C Monitor

// 供其他模块写入监视器文本（不可重入，简单环形缓冲）
static inline void i2ctest_monitor_append(const char *str)
{
    if (!str)
        return;
    for (const char *p = str; *p; ++p)
    {
        i2ctest_rx_buffer[i2ctest_rx_index++] = *p;
        if (i2ctest_rx_index >= I2C_MONITOR_BUFFER_SIZE)
            i2ctest_rx_index = 0;
    }
    // 行尾
    i2ctest_rx_buffer[i2ctest_rx_index++] = '\n';
    if (i2ctest_rx_index >= I2C_MONITOR_BUFFER_SIZE)
        i2ctest_rx_index = 0;
}

static bool i2c_monitor_paused = false; // 监视器暂停标志

// 监视器动画位移（同 serialread）
static int i2ctest_monitor_YPos_End = 0;
static int i2ctest_monitor_YPos_Start = 0;
static bool i2ctest_monitor_but_need_draw_main = false;

// UI参数（与 serialread / hardwaretest 保持一致风格）——使用唯一前缀避免重定义
static const uint16_t i2ctest_item_height = 15;
static const uint16_t i2ctest_font_height = 12;
static const uint16_t i2ctest_font_width = 7;
static const uint16_t i2ctest_ChangeVal_Width = 84;
static const uint8_t i2ctest_itemHeightOffset = (i2ctest_item_height - i2ctest_font_height) / 2 + 1;

// 绘制主界面（可带 Y 偏移以实现上滑）
static void draw_i2ctest_main(int Ypos)
{
    // 清屏
    DrawRectangle(buf, 0, Ypos, 128, 64, 1, 0);

    // 第一行：TA / TB 标头
    DrawRectangle(buf, 0, Ypos + 0, 64, 16, 0, 1);
    DrawRectangle(buf, 64, Ypos + 0, 64, 16, 0, 1);
    Paint_DrawString_EN_CenterAtX(buf, 32, Ypos + 2, "TA", &Font16, 1);
    Paint_DrawString_EN_CenterAtX(buf, 96, Ypos + 2, "TB", &Font16, 1);

    // 第二行：SCL / SDA（按下交换；中间显示 <-> 提示）
    if (i2ctest_scl_on_left)
    {
        Paint_DrawString_EN_CenterAtX(buf, 32, Ypos + 19, "SCL", &Font12, 1);
        Paint_DrawString_EN_CenterAtX(buf, 96, Ypos + 19, "SDA", &Font12, 1);
    }
    else
    {
        Paint_DrawString_EN_CenterAtX(buf, 32, Ypos + 19, "SDA", &Font12, 1);
        Paint_DrawString_EN_CenterAtX(buf, 96, Ypos + 19, "SCL", &Font12, 1);
    }
    if (nowselect_i2c_item == 0)
    {
        Paint_DrawString_EN_CenterAtX(buf, 64, Ypos + 19, "<->", &Font12, 1);
    }

    // 第三行：左 2/3 频率，右 1/3 DEMO
    char freq_str[24];
    if (i2c_freq >= 1000000)
    {
        snprintf(freq_str, sizeof(freq_str), "频率:%.1fMHz", (float)i2c_freq / 1000000.0f);
    }
    else
    {
        snprintf(freq_str, sizeof(freq_str), "频率:%luKHz", (uint32_t)(i2c_freq / 1000));
    }
    Paint_DrawString_EN_CenterAtX(buf, 42, Ypos + 35, freq_str, &Font12, 1);
    Paint_DrawString_EN_CenterAtX(buf, 106, Ypos + 35, "M", &Font12, 1);

    // 第四行：I2C Monitor
    char monitor_str[32];
    uint8_t dev_num = 0;
    for (int addr = 0x03; addr <= 0x77; ++addr)
    {
        if (prev_present[addr])
        {
            dev_num++;
        }
    }
    snprintf(monitor_str, sizeof(monitor_str), "I2C监视(%d)", dev_num);
    Paint_DrawString_EN_CenterAtX(buf, 64, Ypos + 51, monitor_str, &Font12, 1);

    // 分割线
    DrawLine(buf, 0, Ypos + 15, 127, Ypos + 15, 1);
    DrawLine(buf, 0, Ypos + 31, 127, Ypos + 31, 1);
    DrawLine(buf, 0, Ypos + 47, 127, Ypos + 47, 1);
    DrawLine(buf, 0, Ypos + 64, 127, Ypos + 64, 1);
}

// 绘制频率预设选择窗口
static void draw_i2ctest_freq_selector()
{
    int heigh = i2ctest_item_height * 2;
    int width = i2ctest_ChangeVal_Width;
    int x = (SCREEN_WIDTH - width) / 2;
    int y = (SCREEN_HEIGHT - heigh) / 2 + 1;

    UIDisplayStr_font12(x + 3, y + i2ctest_itemHeightOffset, "I2C频率:", 10, 0);

    UIDisplayStr_font12(x + 3, y + i2ctest_item_height + i2ctest_itemHeightOffset, i2ctest_preset_names[i2ctest_global_i2c_freq_selection], 10, 0);
}

// 移除 SSD1306/MPU6050 演示相关逻辑

// 绘制 I2C 监视器（支持上滑偏移、右上角模式标注、底部进度条）
static void draw_i2ctest_monitor(int Ypos)
{
    static char paused_buffer[I2C_MONITOR_BUFFER_SIZE];
    static int paused_index = 0;
    static bool paused_data_captured = false;
    static uint32_t paused_us = 0;

// 行处理参数
#define I2C_SHOW_LINES 7
#define I2C_MAX_CHARS_PER_LINE 25
#define I2C_FONT_WIDTH 5
#define I2C_FONT_HEIGHT 8

    uint32_t now_us = time_us_32();

    // 根据暂停状态选择数据源
    char *src_buf;
    int src_idx;

    if (i2c_monitor_paused)
    {
        // 暂停状态：使用静态缓存数据
        if (!paused_data_captured)
        {
            // 第一次暂停，复制当前数据到静态缓存
            memcpy(paused_buffer, i2ctest_rx_buffer, I2C_MONITOR_BUFFER_SIZE);
            paused_index = i2ctest_rx_index;
            paused_data_captured = true;
            paused_us = now_us; // 记录暂停时间
        }
        src_buf = paused_buffer;
        src_idx = paused_index;
    }
    else
    {
        // 非暂停状态：使用实时数据
        src_buf = i2ctest_rx_buffer;
        src_idx = i2ctest_rx_index;
        paused_data_captured = false; // 重置暂停数据标志
        paused_us = 0;
    }

    // 环形缓冲线性化
    char linear_buf[I2C_MONITOR_BUFFER_SIZE + 1];
    int total_len = 0;
    int idx = src_idx % I2C_MONITOR_BUFFER_SIZE;
    int first_part_len = I2C_MONITOR_BUFFER_SIZE - idx;
    memcpy(linear_buf, src_buf + idx, first_part_len);
    memcpy(linear_buf + first_part_len, src_buf, idx);
    total_len = I2C_MONITOR_BUFFER_SIZE;
    linear_buf[total_len] = '\0';

// 切分为行（正序）
#define I2C_MAX_DISPLAY_LINES 50
    static char all_lines[I2C_MAX_DISPLAY_LINES][I2C_MAX_CHARS_PER_LINE + 1];
    for (int l = 0; l < I2C_MAX_DISPLAY_LINES; l++)
        all_lines[l][0] = '\0';
    int current_line = 0, current_pos = 0;
    for (int i = 0; i < total_len && current_line < I2C_MAX_DISPLAY_LINES; i++)
    {
        char ch = linear_buf[i];
        if (ch == '\r' || ch == '\n')
        {
            // 如果是 CRLF，避免将 CR 与 LF 分别作为两行（跳过紧随 CR 的 LF）
            if (ch == '\n' && i > 0 && linear_buf[i - 1] == '\r')
                continue;
            all_lines[current_line][current_pos] = '\0';
            current_line++;
            current_pos = 0;
            continue;
        }
        if (ch >= 32 && ch <= 126)
        {
            if (current_pos < I2C_MAX_CHARS_PER_LINE)
            {
                all_lines[current_line][current_pos++] = ch;
            }
            else
            {
                all_lines[current_line][current_pos] = '\0';
                current_line++;
                if (current_line < I2C_MAX_DISPLAY_LINES)
                {
                    current_pos = 0;
                    all_lines[current_line][current_pos++] = ch;
                }
            }
        }
        else if (ch == '\t')
        {
            if (current_pos < I2C_MAX_CHARS_PER_LINE)
                all_lines[current_line][current_pos++] = ' ';
        }
    }
    if (current_line < I2C_MAX_DISPLAY_LINES)
        all_lines[current_line][current_pos] = '\0';
    int total_line_count = current_line + (current_pos ? 1 : 0);

    int display_start = (total_line_count > I2C_SHOW_LINES) ? total_line_count - I2C_SHOW_LINES : 0;

    for (int ln = 0; ln < I2C_SHOW_LINES; ln++)
    {
        int line_idx = display_start + ln;
        if (line_idx >= total_line_count)
            break;

        char *str = all_lines[line_idx];
        int len = strlen(str);

        int x_start = 1;
        int y_draw = Ypos + 64 + ln * (I2C_FONT_HEIGHT + 1);

        if (len > 0)
        {
            Paint_DrawString_EN(buf, x_start, y_draw, str, &Font8, 1);
        }

        // 在最后一行的末尾添加闪烁光标（仅在非暂停状态下）
        if (!i2c_monitor_paused && line_idx == total_line_count - 1)
        {
            // 使用时间判断光标闪烁，每500ms闪烁一次
            if ((now_us / 500000) % 2 == 0)
            {
                int cursor_x = x_start + len * I2C_FONT_WIDTH;
                if (cursor_x < 125)
                {
                    Paint_DrawString_EN(buf, cursor_x, y_draw, "_", &Font8, 1);
                }
            }
        }
    }

    // 底部进度条（右滑动）
    int progress_y = Ypos + 64 + I2C_SHOW_LINES * (I2C_FONT_HEIGHT + 1);
    uint8_t progress;

    if (i2c_monitor_paused)
    {
        progress = (paused_us % 1000000) * 128 / 1000000;
    }
    else
    {
        progress = (now_us % 1000000) * 128 / 1000000;
    }

    if (progress > 0)
    {
        int line_start = progress % SSD1306_WIDTH;
        int line_end = (progress + 20) % SSD1306_WIDTH;
        if (line_end > line_start)
        {
            DrawLine(buf, line_start, progress_y, line_end, progress_y, 1);
        }
        else
        {
            DrawLine(buf, line_start, progress_y, SSD1306_WIDTH - 1, progress_y, 1);
            DrawLine(buf, 0, progress_y, line_end, progress_y, 1);
        }
    }
}

// 动画与渲染
static void anni_i2ctest()
{
    int xEnd, yEnd, widthEnd, heighEND;

    if (!i2ctestIsRunning)
    {
        // 非运行状态下，根据选择项目确定高亮区域
        switch (nowselect_i2c_item)
        {
        case 0: // SCL/SDA 交换
            xEnd = 1;
            yEnd = 17;
            widthEnd = 125;
            heighEND = 12;
            i2ctest_monitor_YPos_End = 0;
            break;
        case 1: // 频率 左 2/3
            xEnd = 1;
            yEnd = 33;
            widthEnd = 85;
            heighEND = 12;
            i2ctest_monitor_YPos_End = 0;
            break;
        case 2: // M 右 1/3
            xEnd = 87;
            yEnd = 33;
            widthEnd = 39;
            heighEND = 12;
            i2ctest_monitor_YPos_End = 0;
            break;
        case 3: // I2C Monitor 整页
            xEnd = 0;
            yEnd = 0;
            widthEnd = SCREEN_WIDTH - 1;
            heighEND = SCREEN_HEIGHT - 1;
            i2ctest_monitor_YPos_End = -64;
            break;
        default:
            xEnd = 1;
            yEnd = 1;
            widthEnd = 126;
            heighEND = 12;
            i2ctest_monitor_YPos_End = 0;
            break;
        }
    }
    else
    {
        // 运行状态下的窗口区域
        switch (nowselect_i2c_item)
        {
        case 1: // 频率选择窗口
            heighEND = i2ctest_item_height * 2;
            widthEnd = i2ctest_ChangeVal_Width;
            xEnd = (SCREEN_WIDTH - widthEnd) / 2;
            yEnd = (SCREEN_HEIGHT - heighEND) / 2 + 1;
            i2ctest_monitor_YPos_End = 0;
            break;
        default:
            heighEND = i2ctest_item_height * 2;
            widthEnd = i2ctest_ChangeVal_Width;
            xEnd = (SCREEN_WIDTH - widthEnd) / 2;
            yEnd = (SCREEN_HEIGHT - heighEND) / 2;
            i2ctest_monitor_YPos_End = 0;
            break;
        }
    }

    if (i2ctest_infunction)
    {
        seedvalue = to_ms_since_boot(get_absolute_time());
    }

    bool if_no_animation = false;
    if (xEnd == xStart && yEnd == yStart && widthEnd == lengthStart && heightStart == heighEND)
    {
        if_no_animation = true;
    }

    if (!if_no_animation && footlength != 1)
    {
        float footlengthtemp = footlength;
        for (float i = 0; i <= 1; i += (footlengthtemp))
        {
            int x = easeInOutQuad(xStart, xEnd, i);
            int width = easeInOutQuad(lengthStart, widthEnd, i);
            int heigh = easeInOutQuad(heightStart, heighEND, i);
            int y = easeInOutQuad(yStart, yEnd, i);
            int monitor_YPos = X2line_down(i2ctest_monitor_YPos_Start, i2ctest_monitor_YPos_End, i);

            if (i2ctest_monitor_but_need_draw_main || (!i2ctestIsRunning && nowselect_i2c_item != 3) || (i2ctestIsRunning && i2ctest_infunction))
            {
                draw_i2ctest_main(monitor_YPos);
            }

            if ((i2ctestIsRunning && !i2ctest_infunction))
            {
                ApplyBlurEffect(buf, 1);
            }
            else if (i2ctest_infunction && i2ctestIsRunning)
            {
                ApplyBlurEffect(buf, X2line_up(0, 1, i));
            }
            else if (i2ctest_outfunction)
            {
                ApplyBlurEffect(buf, X2line_down(0.8, 0, i));
            }
            else if (!i2ctestIsRunning && nowselect_i2c_item != 3)
            {
                UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);
            }

            if (i2ctestIsRunning)
            {
                UIDrawSgate(x, y, width, heigh, 8, 6, 1, 2);
            }
            else if (i2ctest_outfunction)
            {
                UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);
            }

            if (nowselect_i2c_item == 3)
            {
                draw_i2ctest_monitor(monitor_YPos);
                if (i2ctest_monitor_but_need_draw_main)
                {
                    UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);
                }
            }

            if (i2ctestIsRunning)
            {
                switch (nowselect_i2c_item)
                {
                case 1:
                    draw_i2ctest_freq_selector();
                    break;
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
    i2ctest_infunction = false;
    i2ctest_outfunction = false;
    i2ctest_monitor_but_need_draw_main = false;
    i2ctest_monitor_YPos_Start = i2ctest_monitor_YPos_End;

    // 静态收尾绘制
    if (nowselect_i2c_item != 3)
    {
        draw_i2ctest_main(i2ctest_monitor_YPos_End);
    }

    if (i2ctestIsRunning)
    {
        ApplyBlurEffect(buf, 1);
        UIDrawSgate(xEnd, yEnd, widthEnd, heighEND, 8, 6, 1, 2);
    }
    if (!i2ctestIsRunning && nowselect_i2c_item != 3)
    {
        UIDrawSgate(xEnd, yEnd, widthEnd, heighEND, 8, 6, 1, 1);
    }

    if (i2ctestIsRunning)
    {
        UIDrawSgate(xEnd, yEnd, widthEnd, heighEND, 8, 6, 1, 2);
        switch (nowselect_i2c_item)
        {
        case 1:
            draw_i2ctest_freq_selector();
            break;
        }
    }

    if (nowselect_i2c_item == 3)
    {
        draw_i2ctest_monitor(i2ctest_monitor_YPos_End);
    }

    render(buf, &frame_area);
    memset(buf, 0, SSD1306_BUF_LEN);
}

// 主交互
void i2ctest()
{
    ResetOpn();
    sync_i2c_freq_selection_from_value();
    i2ctest_monitor_YPos_Start = 0;
    anni_i2ctest();

    while (true)
    {
        if (opnEnter || opnExit || opnUp || opnDown || opnCtrl || opnCtrlUp || opnCtrlDown)
        {
            if (nowselect_i2c_item == 3)
            {
                // 监视器逻辑
                if (opnUp && !i2ctestIsRunning)
                {
                    nowselect_i2c_item--; 
                    if (nowselect_i2c_item < 0) nowselect_i2c_item = 0;
                }
                else if (opnDown && !i2ctestIsRunning)
                {
                    nowselect_i2c_item++; 
                    if (nowselect_i2c_item > 3) nowselect_i2c_item = 3;
                }
                else if (opnExit)
                {
                    nowselect_i2c_item = i2ctest_select_temp;
                }
                else if (opnEnter)
                {
                    i2c_monitor_paused = !i2c_monitor_paused;
                }
            }
            else
            {
                // 非监视器逻辑
                if (i2ctestIsRunning)
                {
                    // 窗口内交互
                    if (nowselect_i2c_item == 1)
                    {
                        if (opnDown)
                        {
                            i2ctest_global_i2c_freq_selection--;
                            if (i2ctest_global_i2c_freq_selection < 0) i2ctest_global_i2c_freq_selection = 0;
                            i2ctest_temp_i2c_freq = i2ctest_preset_values[i2ctest_global_i2c_freq_selection];
                        }
                        else if (opnUp)
                        {
                            i2ctest_global_i2c_freq_selection++;
                            if (i2ctest_global_i2c_freq_selection > 3) i2ctest_global_i2c_freq_selection = 3;
                            i2ctest_temp_i2c_freq = i2ctest_preset_values[i2ctest_global_i2c_freq_selection];
                        }
                        else if (opnEnter)
                        {
                            StartStateLED();
                            i2c_freq = i2ctest_temp_i2c_freq;
                            i2ctestIsRunning = false;
                            i2ctest_outfunction = 1;
                        }
                        else if (opnExit)
                        {
                            i2ctestIsRunning = false;
                            i2c_freq = i2c_freq_backup;
                            i2ctest_outfunction = 1;
                        }
                    }
                }
                else
                {
                    // 主界面交互
                    if (opnUp)
                    {
                        nowselect_i2c_item--;
                        if (nowselect_i2c_item < 0) nowselect_i2c_item = 0;
                    }
                    else if (opnDown)
                    {
                        nowselect_i2c_item++;
                        if (nowselect_i2c_item > 3) nowselect_i2c_item = 3;
                        if (nowselect_i2c_item == 3) i2ctest_monitor_but_need_draw_main = true;
                    }
                    else if (opnExit)
                    {
                        nowselect_i2c_item = 3;
                        i2ctest_monitor_but_need_draw_main = true;
                    }
                    else if (opnEnter)
                    {
                        switch (nowselect_i2c_item)
                        {
                        case 0:
                            i2ctest_scl_on_left = !i2ctest_scl_on_left;
                            StartStateLED();
                            if (i2ctest_scl_on_left) Selector_TA_TX_TB_RX();
                            else Selector_TA_RX_TB_TX();
                            break;
                        case 1:
                            i2c_freq_backup = i2c_freq;
                            sync_i2c_freq_selection_from_value();
                            i2ctest_temp_i2c_freq = i2ctest_preset_values[i2ctest_global_i2c_freq_selection];
                            i2ctestIsRunning = true;
                            i2ctest_infunction = 1;
                            break;
                        case 2:
                            i2ctest_switch_mode();
                            break;
                        }
                    }
                }
                if (nowselect_i2c_item != 3) i2ctest_select_temp = nowselect_i2c_item;
            }
            ResetOpn();
        }
        anni_i2ctest();

        extern bool opnPCchangemode;
        if (opnLeft || opnRight || opnPCchangemode) return;
        if (i2cspi_test_running) { }
    }
}

#endif
