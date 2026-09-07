#ifndef _SPITEST_CPP_
#define _SPITEST_CPP_

#include "pico/stdlib.h"
#include "menu/ssd1306.hpp"
#include "menu/UI.hpp"
#include "menu/key.hpp"
#include "hardware/spi.h"
#include "CONFIG_FLO.hpp"
#include "menu/core.hpp"
#include "menu/StateLED.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

// 交互与窗口状态
bool spitestIsRunning = false;
bool spitest_infunction = false;
bool spitest_outfunction = false;

// 选项：0=频率（行2），1=Bits（行3左），2=CPOL（行3中），3=CPHA（行3右），4=Monitor（行4/全屏）
int nowselect_spi_item = 0;
int spitest_select_temp = 0; // 记录非监视器选择项

// 外部硬件参数（定义在 core.cpp）
extern int32_t spi_freq; 
extern uint8_t spi_bits_val;
extern uint8_t spi_cpol_val;
extern uint8_t spi_cpha_val;
extern bool spi_run_in_core1;
extern bool spi_master_mode;

// 频率预设
static const int spitest_preset_values[] = {100000, 400000, 1000000, 2000000};
static char *spitest_preset_names[] = {"100KHz", "400KHz", "1MHz", "2MHz"};
static int spitest_global_freq_selection = 1;
static int32_t spi_freq_backup = 0;
static int32_t spitest_temp_freq = 0;

static const int spitest_bits_values[] = {8, 9, 10, 11, 12, 13, 14, 15, 16};
static int spitest_global_bits_selection = 0;
static uint8_t spi_bits_backup = 8;
static uint8_t spitest_temp_bits = 8;

static void sync_spi_bits_selection_from_value()
{
    int val = spi_bits_val;
    spitest_global_bits_selection = 0;
    for (int i = 0; i < 9; i++)
    {
        if (spitest_bits_values[i] == val)
        {
            spitest_global_bits_selection = i;
            break;
        }
    }
}

static void sync_spi_freq_selection_from_value()
{
    int val = (int)spi_freq;
    int sel = 1;
    for (int i = 0; i < 4; i++)
    {
        if (spitest_preset_values[i] >= val)
        {
            sel = i;
            break;
        }
    }
    spitest_global_freq_selection = sel;
}

// 监视器缓冲写入
static inline void spitest_monitor_append(const char *str)
{
    if (!str)
        return;
    for (const char *p = str; *p; ++p)
    {
        spitest_rx_buffer[spitest_rx_index++] = *p;
        if (spitest_rx_index >= SPI_MONITOR_BUFFER_SIZE)
            spitest_rx_index = 0;
    }
    // 行尾
    spitest_rx_buffer[spitest_rx_index++] = '\n';
    if (spitest_rx_index >= SPI_MONITOR_BUFFER_SIZE)
        spitest_rx_index = 0;
}

static bool spi_monitor_paused = false;

// 监视器动画位移
static int spitest_monitor_YPos_End = 0;
static int spitest_monitor_YPos_Start = 0;
static bool spitest_monitor_but_need_draw_main = false;

// UI参数
static const uint16_t spitest_item_height = 15;
static const uint16_t spitest_font_height = 12;
static const uint16_t spitest_font_width = 7;
static const uint16_t spitest_ChangeVal_Width = 84;
static const uint8_t spitest_itemHeightOffset = (spitest_item_height - spitest_font_height) / 2 + 1;

// 绘制主界面
static void draw_spitest_main(int Ypos)
{
    // 清屏
    DrawRectangle(buf, 0, Ypos, 128, 64, 1, 0);

    // 第一行：空（保留线）
    DrawRectangle(buf, 0, Ypos + 0, 128, 16, 0, 1);
    DrawLine(buf, 0, Ypos + 15, 127, Ypos + 15, 1);
    Paint_DrawString_EN_CenterAtX(buf, 64, Ypos + 3, "SPI(MSB优先)", &Font12, 1);

    if(spitext_ifupdate){
        spitext_ifupdate = false;
        //反转第一行
        InvertRect(buf, 0, Ypos + 0, 128, 16);
    }

    // 第二行：频率 + 模式
    char freq_str[24];
    if (spi_freq >= 1000000)
    {
        snprintf(freq_str, sizeof(freq_str), "频率:%.1fMHz", (float)spi_freq / 1000000.0f);
    }
    else
    {
        snprintf(freq_str, sizeof(freq_str), "频率:%luKHz", (uint32_t)(spi_freq / 1000));
    }
    Paint_DrawString_EN_CenterAtX(buf, 46, Ypos + 19, freq_str, &Font12, 1);
    
    // Mode
    Paint_DrawString_EN_CenterAtX(buf, 110, Ypos + 19, spi_master_mode ? "M" : "S", &Font12, 1);

    // Split Line
    DrawLine(buf, 0, Ypos + 31, 127, Ypos + 31, 1);

    // 第三行：Bits | CPOL | CPHA
    char bits_str[10], cpol_str[10], cpha_str[10];
    snprintf(bits_str, sizeof(bits_str), "%d位", spi_bits_val);
    snprintf(cpol_str, sizeof(cpol_str), "CPOL%d", spi_cpol_val);
    snprintf(cpha_str, sizeof(cpha_str), "CPHA%d", spi_cpha_val);

    // 分隔线位置：42, 85
    Paint_DrawString_EN_CenterAtX(buf, 21, Ypos + 35, bits_str, &Font12, 1);
    Paint_DrawString_EN_CenterAtX(buf, 64, Ypos + 35, cpol_str, &Font12, 1);
    Paint_DrawString_EN_CenterAtX(buf, 106, Ypos + 35, cpha_str, &Font12, 1);

    DrawLine(buf, 0, Ypos + 47, 127, Ypos + 47, 1);

    // 第四行：SPI Monitor
    Paint_DrawString_EN_CenterAtX(buf, 64, Ypos + 51, "SPI监视", &Font12, 1);
    DrawLine(buf, 0, Ypos + 64, 127, Ypos + 64, 1);
}

// 绘制频率选择器
static void draw_spitest_freq_selector()
{
    int heigh = spitest_item_height * 2;
    int width = spitest_ChangeVal_Width;
    int x = (SCREEN_WIDTH - width) / 2;
    int y = (SCREEN_HEIGHT - heigh) / 2 + 1;

    UIDisplayStr_font12(x + 3, y + spitest_itemHeightOffset, "SPI频率:", 10, 0);
    UIDisplayStr_font12(x + 3, y + spitest_item_height + spitest_itemHeightOffset, spitest_preset_names[spitest_global_freq_selection], 10, 0);
}

// 绘制Bits选择器
static void draw_spitest_bits_selector()
{
    int heigh = spitest_item_height * 2;
    int width = spitest_ChangeVal_Width;
    int x = (SCREEN_WIDTH - width) / 2;
    int y = (SCREEN_HEIGHT - heigh) / 2 + 1;

    UIDisplayStr_font12(x + 3, y + spitest_itemHeightOffset, "SPI位数:", 10, 0);
    char temp[16];
    snprintf(temp, sizeof(temp), "%d", spitest_bits_values[spitest_global_bits_selection]);
    UIDisplayStr_font12(x + 3, y + spitest_item_height + spitest_itemHeightOffset, temp, 10, 0);
}

// 绘制 SPI 监视器 (针对双通道原始数据数组的 16 进制绘制)
static void draw_spitest_monitor(int Ypos)
{
    static uint8_t paused_tx_buffer[SPI_MONITOR_BUFFER_SIZE];
    static uint8_t paused_rx_buffer[SPI_MONITOR_BUFFER_SIZE];
    static int paused_tx_idx = 0;
    static int paused_rx_idx = 0;
    static bool paused_data_captured = false;
    static uint32_t paused_us = 0;

#define SPI_SHOW_LINES_PER_ZONE 3
#define SPI_CHARS_PER_HEX 3 // "XX "
#define SPI_HEX_PER_LINE 8  // 每行 8 个字节
#define SPI_FONT_WIDTH 5
#define SPI_FONT_HEIGHT 8

    uint32_t now_us = time_us_32();
    uint8_t *src_tx, *src_rx;
    int src_tx_idx, src_rx_idx;

    if (spi_monitor_paused)
    {
        if (!paused_data_captured)
        {
            memcpy(paused_tx_buffer, spitest_tx_buffer, SPI_MONITOR_BUFFER_SIZE);
            memcpy(paused_rx_buffer, spitest_rx_buffer, SPI_MONITOR_BUFFER_SIZE);
            paused_tx_idx = spitest_tx_index;
            paused_rx_idx = spitest_rx_index;
            paused_data_captured = true;
            paused_us = now_us;
        }
        src_tx = paused_tx_buffer;
        src_rx = paused_rx_buffer;
        src_tx_idx = paused_tx_idx;
        src_rx_idx = paused_rx_idx;
    }
    else
    {
        src_tx = spitest_tx_buffer;
        src_rx = spitest_rx_buffer;
        src_tx_idx = spitest_tx_index;
        src_rx_idx = spitest_rx_index;
        paused_data_captured = false;
        paused_us = 0;
    }

    auto render_zone = [&](uint8_t *data, int head_idx, const char *tag, int start_y)
    {
        const int show_rows = 3;
        const int total_rows = 4;
        
        // 核心渲染逻辑：始终显示物理缓冲区中的“最新” 3 行。
        // 如果缓冲区已满 (spitest_buf_full)，则数据形成环形流，
        // 当前写入位置 head_idx 所在的行应该是显示在最下方的。
        
        int current_row = head_idx / SPI_HEX_PER_LINE;
        int start_row;

        if (!spitest_buf_full)
        {
            // 第一轮写入：从第 0 行到第 2 行。如果写到第 3 行，则向下顺卷。
            start_row = (current_row >= show_rows) ? (current_row - show_rows + 1) : 0;
        }
        else
        {
            // 环形溢出后：为了保证历史连续感，我们将 head_idx 所在的行固定显示在第 3 行 (最后一行)
            // 这样视图会随着写入点的跨行而自动翻页。
            start_row = (current_row - show_rows + 1 + total_rows) % total_rows;
        }

        for (int r = 0; r < show_rows; r++)
        {
            int line_idx = (start_row + r) % total_rows;
            char line_str[64] = "";
            
            // 第一行显示标识
            if (r == 0) strcat(line_str, tag);
            else strcat(line_str, "  ");

            for (int i = 0; i < SPI_HEX_PER_LINE; i++)
            {
                int byte_pos = line_idx * SPI_HEX_PER_LINE + i;
                
                // 渲染判断逻辑：
                // 1. 在 buffer 未满的第一圈，只渲染 head_idx 之前的数据。
                // 2. 在环形模式下 (spitest_buf_full)，不仅要渲染数据，
                //    还必须确保不渲染“属于未来”的数据（即上一轮残留在 head_idx 之后的数据）。
                //    由于 head_idx 始终指向“下一发”要写入的位置，
                //    我们只渲染 [head_idx - 32, head_idx - 1] 范围内的数据。
                
                bool is_valid = false;
                if (!spitest_buf_full)
                {
                    is_valid = (byte_pos < head_idx);
                }
                else
                {
                    // 已满状态下的过滤逻辑：
                    // 1. 绝对不能显示 head_idx 位置（那是物理上的最旧数据，逻辑上的“未来”）。
                    // 2. 如果 head_idx 在行中，则从 head_idx 开始到该行结束（索引 7, 15, 23, 31）
                    //    都视为逻辑上的“旧轮次残留”或“已过期历史”，渲染为空白。
                    
                    int current_line_start = (head_idx / SPI_HEX_PER_LINE) * SPI_HEX_PER_LINE;
                    int current_line_end = current_line_start + SPI_HEX_PER_LINE - 1;

                    if (byte_pos >= head_idx && byte_pos <= current_line_end)
                    {
                        is_valid = false; // 处于当前写指针之后到行末的部分，强制为空
                    }
                    else
                    {
                        is_valid = (byte_pos != head_idx);
                    }
                }

                if (is_valid)
                {
                    char hex[4];
                    snprintf(hex, sizeof(hex), "%02X ", data[byte_pos]);
                    strcat(line_str, hex);
                }
                else
                {
                    // 不显示属于上一轮的残留数据，保持空白
                    strcat(line_str, "   ");
                }
            }

            int py = Ypos + 64 + start_y + r * (SPI_FONT_HEIGHT + 1) +2;
            Paint_DrawString_EN(buf, 1, py, line_str, &Font8, 1);

            // 光标逻辑：如果当前写入点就在这一行，则显示光标
            if (!spi_monitor_paused && line_idx == current_row)
            {
                if ((now_us / 500000) % 2 == 0)
                {
                    int col = head_idx % SPI_HEX_PER_LINE;
                    int cursor_x = 1 + (2 + col * 3) * SPI_FONT_WIDTH;
                    if (cursor_x < 125) Paint_DrawString_EN(buf, cursor_x, py, "_", &Font8, 1);
                }
            }
        }
    };

    // 绘制 TX 区域 (上方)
    render_zone(src_tx, src_tx_idx, "> ", 2);
    // 绘制 RX 区域 (下方)
    render_zone(src_rx, src_rx_idx, "< ", 32);

    // 绘制底部装饰线 (确保 Ypos 偏移正确)
    int monitor_base_y = Ypos + 64;

    // 底部进度条
    int progress_y = monitor_base_y + 63;
    uint8_t progress = spi_monitor_paused ? (paused_us % 1000000) * 128 / 1000000 : (now_us % 1000000) * 128 / 1000000;
    if (progress > 0)
    {
        int line_start = progress % 128;
        int line_end = (progress + 20) % 128;
        if (line_end > line_start) DrawLine(buf, line_start, progress_y, line_end, progress_y, 1);
        else { DrawLine(buf, line_start, progress_y, 127, progress_y, 1); DrawLine(buf, 0, progress_y, line_end, progress_y, 1); }
    }
}

static void anni_spitest()
{
    int xEnd, yEnd, widthEnd, heighEND;

    if (!spitestIsRunning)
    {
        switch (nowselect_spi_item)
        {
        case 0: // Freq
            xEnd = 1; yEnd = 17; widthEnd = 92; heighEND = 12;
            spitest_monitor_YPos_End = 0;
            break;
        case 1: // Mode (M/S)
            xEnd = 94; yEnd = 17; widthEnd = 32; heighEND = 12;
            spitest_monitor_YPos_End = 0;
            break;
        case 2: // Bits
            xEnd = 1; yEnd = 33; widthEnd = 40; heighEND = 12;
            spitest_monitor_YPos_End = 0;
            break;
        case 3: // CPOL
            xEnd = 43; yEnd = 33; widthEnd = 41; heighEND = 12;
            spitest_monitor_YPos_End = 0;
            break;
        case 4: // CPHA
            xEnd = 86; yEnd = 33; widthEnd = 40; heighEND = 12;
            spitest_monitor_YPos_End = 0;
            break;
        case 5: // Monitor
            xEnd = 0; yEnd = 0; widthEnd = SCREEN_WIDTH - 1; heighEND = SCREEN_HEIGHT - 1;
            spitest_monitor_YPos_End = -64;
            break;
        default:
            xEnd = 1; yEnd = 1; widthEnd = 126; heighEND = 12;
            break;
        }
    }
    else
    {
        // 运行状态（窗口）
        if (nowselect_spi_item == 0) // Freq selection
        {
            heighEND = spitest_item_height * 2;
            widthEnd = spitest_ChangeVal_Width;
            xEnd = (SCREEN_WIDTH - widthEnd) / 2;
            yEnd = (SCREEN_HEIGHT - heighEND) / 2 + 1;
        }
        else if (nowselect_spi_item == 2) // Bits selection
        {
            heighEND = spitest_item_height * 2;
            widthEnd = spitest_ChangeVal_Width;
            xEnd = (SCREEN_WIDTH - widthEnd) / 2;
            yEnd = (SCREEN_HEIGHT - heighEND) / 2 + 1;
        }
        else
        {
             // 默认全屏或保持
            xEnd = 0; yEnd = 0; widthEnd = SCREEN_WIDTH - 1; heighEND = SCREEN_HEIGHT - 1;
        }
        spitest_monitor_YPos_End = 0;
    }

    if (spitest_infunction) seedvalue = to_ms_since_boot(get_absolute_time());

    bool if_no_animation = (xEnd == xStart && yEnd == yStart && widthEnd == lengthStart && heighEND == heightStart);

    if (!if_no_animation && footlength != 1)
    {
        for (float i = 0; i <= 1; i += footlength)
        {
            int x = easeInOutQuad(xStart, xEnd, i);
            int width = easeInOutQuad(lengthStart, widthEnd, i);
            int heigh = easeInOutQuad(heightStart, heighEND, i);
            int y = easeInOutQuad(yStart, yEnd, i);
            int monitor_YPos = X2line_down(spitest_monitor_YPos_Start, spitest_monitor_YPos_End, i);

            if (spitest_monitor_but_need_draw_main || (!spitestIsRunning && nowselect_spi_item != 5) || (spitestIsRunning && spitest_infunction))
            {
                draw_spitest_main(monitor_YPos);
            }

            if ((spitestIsRunning && !spitest_infunction)) ApplyBlurEffect(buf, 1);
            else if (spitest_infunction && spitestIsRunning) ApplyBlurEffect(buf, X2line_up(0, 1, i));
            else if (spitest_outfunction) ApplyBlurEffect(buf, X2line_down(0.8, 0, i));
            else if (!spitestIsRunning && nowselect_spi_item != 5) UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);

            if (spitestIsRunning) UIDrawSgate(x, y, width, heigh, 8, 6, 1, 2);
            else if (spitest_outfunction) UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);

            if (nowselect_spi_item == 5)
            {
                draw_spitest_monitor(monitor_YPos);
                if (spitest_monitor_but_need_draw_main) UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);
            }
            if (spitestIsRunning) 
            {
                if (nowselect_spi_item == 0) draw_spitest_freq_selector();
                if (nowselect_spi_item == 2) draw_spitest_bits_selector();
            }

            render(buf, &frame_area);
            memset(buf, 0, SSD1306_BUF_LEN);
        }
    }

    lengthStart = widthEnd; heightStart = heighEND; yStart = yEnd; xStart = xEnd;
    spitest_infunction = false; spitest_outfunction = false;
    spitest_monitor_but_need_draw_main = false;
    spitest_monitor_YPos_Start = spitest_monitor_YPos_End;

    if (nowselect_spi_item != 5) draw_spitest_main(spitest_monitor_YPos_End);

    if (spitestIsRunning)
    {
        ApplyBlurEffect(buf, 1);
        UIDrawSgate(xEnd, yEnd, widthEnd, heighEND, 8, 6, 1, 2);
        if (nowselect_spi_item == 0) draw_spitest_freq_selector();
        if (nowselect_spi_item == 2) draw_spitest_bits_selector();
    }
    if (!spitestIsRunning && nowselect_spi_item != 5) UIDrawSgate(xEnd, yEnd, widthEnd, heighEND, 8, 6, 1, 1);
    if (nowselect_spi_item == 5) draw_spitest_monitor(spitest_monitor_YPos_End);

    render(buf, &frame_area);
    memset(buf, 0, SSD1306_BUF_LEN);
}

void spitest()
{
    ResetOpn();
    sync_spi_freq_selection_from_value();
    spitest_monitor_YPos_Start = 0;
    
    
    
    anni_spitest();

    while (true)
    {
        if (opnEnter || opnExit || opnUp || opnDown || opnCtrl || opnCtrlUp || opnCtrlDown)
        {
            if (nowselect_spi_item == 5) // Monitor
            {
                if (opnUp && !spitestIsRunning) { nowselect_spi_item--; if (nowselect_spi_item < 0) nowselect_spi_item = 0; }
                else if (opnDown && !spitestIsRunning) { nowselect_spi_item++; if (nowselect_spi_item > 5) nowselect_spi_item = 5; }
                else if (opnExit) { nowselect_spi_item = spitest_select_temp; }
                else if (opnEnter) { spi_monitor_paused = !spi_monitor_paused; }
            }
            else // Configs
            {
                if (spitestIsRunning)
                {
                    if (nowselect_spi_item == 0) // Freq
                    {
                        if (opnDown) { spitest_global_freq_selection--; if (spitest_global_freq_selection < 0) spitest_global_freq_selection = 0; spitest_temp_freq = spitest_preset_values[spitest_global_freq_selection]; }
                        else if (opnUp) { spitest_global_freq_selection++; if (spitest_global_freq_selection > 3) spitest_global_freq_selection = 3; spitest_temp_freq = spitest_preset_values[spitest_global_freq_selection]; }
                        else if (opnEnter) { StartStateLED();spi_freq = spitest_temp_freq; spitestIsRunning = false; spitest_outfunction = 1; }
                        if (opnExit) { spitestIsRunning = false; spi_freq = spi_freq_backup; spitest_outfunction = 1; ResetOpn(); }
                    }
                    else if (nowselect_spi_item == 2) // Bits
                    {
                        if (opnDown) { spitest_global_bits_selection--; if (spitest_global_bits_selection < 0) spitest_global_bits_selection = 0; spitest_temp_bits = spitest_bits_values[spitest_global_bits_selection]; }
                        else if (opnUp) { spitest_global_bits_selection++; if (spitest_global_bits_selection > 8) spitest_global_bits_selection = 8; spitest_temp_bits = spitest_bits_values[spitest_global_bits_selection]; }
                        else if (opnEnter) { StartStateLED();spi_bits_val = spitest_temp_bits; spitestIsRunning = false; spitest_outfunction = 1; }
                        if (opnExit) { spitestIsRunning = false; spi_bits_val = spi_bits_backup; spitest_outfunction = 1; ResetOpn(); }
                    }
                }
                else
                {
                    if (opnUp) { nowselect_spi_item--; if (nowselect_spi_item < 0) nowselect_spi_item = 0; if (nowselect_spi_item == 5) spitest_monitor_but_need_draw_main = true; }
                    else if (opnDown) { nowselect_spi_item++; if (nowselect_spi_item > 5) nowselect_spi_item = 5; if (nowselect_spi_item == 5) spitest_monitor_but_need_draw_main = true; }
                    else if (opnExit && nowselect_spi_item != 5) { nowselect_spi_item = 5; spitest_monitor_but_need_draw_main = true; }
                    else if (opnEnter && nowselect_spi_item != 5)
                    {
                        switch (nowselect_spi_item)
                        {
                        case 0: // Freq
                            spi_freq_backup = spi_freq; sync_spi_freq_selection_from_value();
                            spitest_temp_freq = spitest_preset_values[spitest_global_freq_selection];
                            spitestIsRunning = true; spitest_infunction = 1;
                            break;
                        case 1: // Mode
                            spi_master_mode = !spi_master_mode;
                            break;
                        case 2: // Bits
                            spi_bits_backup = spi_bits_val; sync_spi_bits_selection_from_value();
                            spitest_temp_bits = spitest_bits_values[spitest_global_bits_selection];
                            spitestIsRunning = true; spitest_infunction = 1;
                            break;
                        case 3: // CPOL
                            spi_cpol_val = !spi_cpol_val;
                            // 一定会在CORE更改 一定会有LED提醒
                            break;
                        case 4: // CPHA
                            spi_cpha_val = !spi_cpha_val;
                            // 一定会在CORE更改 一定会有LED提醒
                            break;
                        }
                    }
                    if (nowselect_spi_item != 5) spitest_select_temp = nowselect_spi_item;
                }
            }
            ResetOpn();
        }
        anni_spitest();
        extern bool opnPCchangemode;
        if (opnLeft || opnRight || opnPCchangemode)
        {
            
            return;
        }
    }
}

#endif
