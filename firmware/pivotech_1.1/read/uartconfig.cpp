// UART configuration UI, aligned with serialread animation and controls
#ifndef _UARTCONFIG_CPP_
#define _UARTCONFIG_CPP_

#include "pico/stdlib.h"
#include "menu/ssd1306.hpp"
#include "menu/UI.hpp"
#include "menu/key.hpp"
#include "hardware/uart.h"
#include "CONFIG_FLO.hpp"
#include "menu/core.hpp"
#include "menu/StateLED.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string.h>

extern bool opnEnter, opnExit, opnUp, opnDown, opnLeft, opnRight, opnCtrlUp, opnCtrlDown, opnCtrl;
// Forward declarations for UI.cpp usage
bool uartconfig();
void apply_uart_settings();
void build_active_summary(char *out, size_t len);
void draw_strike_line(int y);
void draw_uartconfig_main(int Ypos);
void draw_uart_baud_selector();
void draw_uart_databits_selector();
void draw_uart_parity_selector();
void draw_uart_stopbits_selector();
void uart_handle_selector_operation(int *current_value, int max_value);
void anni_uartconfig();

// UI helpers consumed here (defined in UI.cpp and related modules)
void UIDrawSgate(int16_t x, int16_t y, int16_t width, int16_t height, uint8_t Xlength, uint8_t Ylength, bool on, int ignorerunning);
void UIDisplayStr_font12(int16_t x, int16_t y, char *pString, size_t keep_chars, bool Invert);
void ApplyBlurEffect(uint8_t *buf, float intensity);
struct render_area;
extern struct render_area frame_area;
void render(uint8_t *buf, struct render_area *area);
void ResetOpn();

extern int seedvalue;
extern int16_t lengthStart, heightStart, yStart, scroll_yStart, itemsYPosStart, xStart;

// State flags
bool uartconfigIsRunning = false;
bool uartconfig_infunction = false;
bool uartconfig_outfunction = false;

// Selection indices (0: keep host, 1: baud, 2: data bits, 3: parity, 4: stop bits)
int nowselect_uartconfig_item = 0;

// Host-set toggle
bool keep_host_set = false;

// Exit flag for main loop
bool exit_uartconfig_flag = false;
bool if_uartconfig_running = false;

// Host snapshot (used when keep_host_set is on)
static int host_baudrate_value = 115200;
static int host_databits_index = 3; // 8
static int host_parity_index = 0;   // NONE
static int host_stopbits_index = 0; // 1.5 (display), mapped to 2 stop bits for HW

// User selections
int uart_baud_selection = 16; // default 115200
int uart_databits_selection = 3;
int uart_parity_selection = 0;
int uart_stopbits_selection = 0; // 0:1, 1:1.5, 2:2

static const int uart_baudrate_values[] = {8000000, 7500000, 6000000, 5000000, 4000000, 3000000, 2000000, 1500000, 1000000, 921600, 750000, 512000, 460800, 256000, 230400, 128000, 115200, 57600, 56000, 38400, 19200, 14400, 9600, 4800, 2400, 1200, 600, 300};
static const char *uart_baudrate_names[] = {"8000000", "7500000", "6000000", "5000000", "4000000", "3000000", "2000000", "1500000", "1000000", "921600", "750000", "512000", "460800", "256000", "230400", "128000", "115200", "57600", "56000", "38400", "19200", "14400", "9600", "4800", "2400", "1200", "600", "300"};

// Data bits options
static const uint8_t uart_databits_values[] = {5, 6, 7, 8};
static const char *uart_databits_names[] = {"5", "6", "7", "8"};

// Parity options (MARK/SPACE mapped to closest supported hardware modes)
static const char *uart_parity_names[] = {"无", "奇", "偶", "无*", "无*"};
static const uart_parity_t uart_parity_hw_map[] = {UART_PARITY_NONE, UART_PARITY_ODD, UART_PARITY_EVEN, UART_PARITY_NONE, UART_PARITY_NONE};

// Stop bits options (1.5 is mapped to 2 stop bits for hardware)
static const char *uart_stopbits_names[] = {"1", "2*", "2"};
static const uint uart_stopbits_hw_map[] = {1, 2, 2};

// UI metrics (aligned with serialread)
uint16_t uart_item_height = 15;
uint16_t uart_font_height = 12;
uint16_t uart_font_width = 7;
uint16_t uart_change_width = 84;
static uint8_t uart_itemHeightOffset = (uart_item_height - uart_font_height) / 2 + 1;

// Modal identification
enum UartConfigModal
{
    UART_MODAL_NONE = 0,
    UART_MODAL_BAUD,
    UART_MODAL_DATABITS,
    UART_MODAL_PARITY,
    UART_MODAL_STOPBITS
};

int uartconfig_modal = UART_MODAL_NONE;

// Temp selections inside modal (confirmed on Enter)
int temp_baud_selection = 4;
int temp_databits_selection = 3;
int temp_parity_selection = 0;
int temp_stopbits_selection = 1;

// Forward declarations
void apply_uart_settings();
void draw_uartconfig_main(int Ypos);
void draw_uart_baud_selector();
void draw_uart_databits_selector();
void draw_uart_parity_selector();
void draw_uart_stopbits_selector();
void uart_handle_selector_operation(int *current_value, int max_value);
void anni_uartconfig();

void apply_uart_settings_from_host()
{
    if (keep_host_set)
    {
        int active_baudrate = host_baudrate_value;
        uint data_bits = uart_databits_values[host_databits_index];
        uint stop_bits = uart_stopbits_hw_map[host_stopbits_index];
        uart_parity_t parity = uart_parity_hw_map[host_parity_index];

        extern bool hc05_is_active;
        if (hc05_is_active) {
            uart_set_baudrate(UART_ID, 38400);
            uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
        } else {
            uart_set_baudrate(UART_ID, active_baudrate);
            uart_set_format(UART_ID, data_bits, stop_bits, parity);
        }
        StartStateLED();
        extern bool send_pc_flag;
        send_pc_flag = true; // 触发向上位机更新数据

    }
};

// Apply the currently effective settings to hardware

// 上位机更新后 也执行该程序
void apply_uart_settings()
{
    int active_baud_idx = uart_baud_selection;
    int active_databits_idx = keep_host_set ? host_databits_index : uart_databits_selection;
    int active_parity_idx = keep_host_set ? host_parity_index : uart_parity_selection;
    int active_stop_idx = keep_host_set ? host_stopbits_index : uart_stopbits_selection;

    active_baud_idx = (active_baud_idx + BAUD_NUMBERS) % BAUD_NUMBERS;
    active_databits_idx = (active_databits_idx + 4) % 4;
    active_parity_idx = (active_parity_idx + 5) % 5;
    active_stop_idx = (active_stop_idx + 3) % 3;

    int active_baudrate = keep_host_set ? host_baudrate_value : uart_baudrate_values[active_baud_idx];
    uint data_bits = uart_databits_values[active_databits_idx];
    uint stop_bits = uart_stopbits_hw_map[active_stop_idx];
    uart_parity_t parity = uart_parity_hw_map[active_parity_idx];

    extern bool hc05_is_active;
    if (hc05_is_active) {
        uart_set_baudrate(UART_ID, 38400);
        uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    } else {
        uart_set_baudrate(UART_ID, active_baudrate);
        uart_set_format(UART_ID, data_bits, stop_bits, parity);
    }
    StartStateLED();
    extern bool send_pc_flag;
    send_pc_flag = true; // 触发向上位机更新数据
}

// Build summary strings based on the active or user selections
void build_active_summary(char *out, size_t len)
{
    int baud_idx = uart_baud_selection;
    int db_idx = keep_host_set ? host_databits_index : uart_databits_selection;
    int parity_idx = keep_host_set ? host_parity_index : uart_parity_selection;
    int stop_idx = keep_host_set ? host_stopbits_index : uart_stopbits_selection;

    baud_idx = (baud_idx + BAUD_NUMBERS) % BAUD_NUMBERS;
    db_idx = (db_idx + 4) % 4;
    parity_idx = (parity_idx + 5) % 5;
    stop_idx = (stop_idx + 3) % 3;

    // hostvalue转为文本
    if (keep_host_set)
    {
        snprintf(out, len, "%d;%s;%s;%s", host_baudrate_value, uart_databits_names[db_idx], uart_parity_names[parity_idx], uart_stopbits_names[stop_idx]);
        return;
    }
    else
    {
        snprintf(out, len, "%s;%s;%s;%s", uart_baudrate_names[baud_idx], uart_databits_names[db_idx], uart_parity_names[parity_idx], uart_stopbits_names[stop_idx]);
        return;
    }
}

void build_active_summary_forhc05(char *out, size_t len)
{
    int baud_idx = uart_baud_selection;
    int db_idx = keep_host_set ? host_databits_index : uart_databits_selection;
    int parity_idx = keep_host_set ? host_parity_index : uart_parity_selection;
    int stop_idx = keep_host_set ? host_stopbits_index : uart_stopbits_selection;

    baud_idx = (baud_idx + BAUD_NUMBERS) % BAUD_NUMBERS;
    db_idx = (db_idx + 4) % 4;
    parity_idx = (parity_idx + 5) % 5;
    stop_idx = (stop_idx + 3) % 3;

    if (db_idx != 3)
    {
        // 提示只能设置为8数据位
        if (keep_host_set)
        {
            snprintf(out, len, "%d;8*;%s;%s", host_baudrate_value, uart_parity_names[parity_idx], uart_stopbits_names[stop_idx]);
            return;
        }
        else
        {
            snprintf(out, len, "%s;8*;%s;%s", uart_baudrate_names[baud_idx], uart_parity_names[parity_idx], uart_stopbits_names[stop_idx]);
            return;
        }
    }
    else
    {
        // hostvalue转为文本
        if (keep_host_set)
        {
            snprintf(out, len, "%d;%s;%s;%s", host_baudrate_value, uart_databits_names[db_idx], uart_parity_names[parity_idx], uart_stopbits_names[stop_idx]);
            return;
        }
        else
        {
            snprintf(out, len, "%s;%s;%s;%s", uart_baudrate_names[baud_idx], uart_databits_names[db_idx], uart_parity_names[parity_idx], uart_stopbits_names[stop_idx]);
            return;
        }
    }
}

void uartconfig_get_active_params(int *baudrate, uint *data_bits, uint *stop_bits, uart_parity_t *parity)
{
    int active_baud_idx = uart_baud_selection;
    int active_databits_idx = keep_host_set ? host_databits_index : uart_databits_selection;
    int active_parity_idx = keep_host_set ? host_parity_index : uart_parity_selection;
    int active_stop_idx = keep_host_set ? host_stopbits_index : uart_stopbits_selection;

    active_baud_idx = (active_baud_idx + BAUD_NUMBERS) % BAUD_NUMBERS;
    active_databits_idx = (active_databits_idx + 4) % 4;
    active_parity_idx = (active_parity_idx + 5) % 5;
    active_stop_idx = (active_stop_idx + 3) % 3;

    if (baudrate)
    {
        *baudrate = keep_host_set ? host_baudrate_value : uart_baudrate_values[active_baud_idx];
    }
    if (data_bits)
    {
        *data_bits = uart_databits_values[active_databits_idx];
    }
    if (stop_bits)
    {
        *stop_bits = uart_stopbits_hw_map[active_stop_idx];
    }
    if (parity)
    {
        *parity = uart_parity_hw_map[active_parity_idx];
    }
}

void uartconfig_get_local_params(int *baudrate, uint *data_bits, uint *stop_bits, uart_parity_t *parity)
{
    int active_baud_idx = uart_baud_selection;
    int active_databits_idx = uart_databits_selection;
    int active_parity_idx = uart_parity_selection;
    int active_stop_idx = uart_stopbits_selection;

    active_baud_idx = (active_baud_idx + BAUD_NUMBERS) % BAUD_NUMBERS;
    active_databits_idx = (active_databits_idx + 4) % 4;
    active_parity_idx = (active_parity_idx + 5) % 5;
    active_stop_idx = (active_stop_idx + 3) % 3;

    if (baudrate)
    {
        *baudrate = uart_baudrate_values[active_baud_idx];
    }
    if (data_bits)
    {
        *data_bits = uart_databits_values[active_databits_idx];
    }
    if (stop_bits)
    {
        *stop_bits = uart_stopbits_hw_map[active_stop_idx];
    }
    if (parity)
    {
        *parity = uart_parity_hw_map[active_parity_idx];
    }
}

void draw_strike_line(int y)
{
    DrawLine(buf, 4, y - 2, 123, y - 2, 1);
}

// Draw the main four-line layout
void draw_uartconfig_main(int Ypos)
{
    DrawRectangle(buf, 0, Ypos, 128, 64, 1, 0);

    char summary[40] = {0};
    build_active_summary(summary, sizeof(summary));

    // Line separators
    DrawLine(buf, 0, Ypos + 15, 127, Ypos + 15, 1);
    DrawLine(buf, 0, Ypos + 31, 127, Ypos + 31, 1);
    DrawLine(buf, 0, Ypos + 47, 127, Ypos + 47, 1);

    // Line 1: boxed summary
    DrawRectangle(buf, 0, Ypos, 128, 16, 0, 1);
    Paint_DrawString_EN_CenterAtX(buf, 64, Ypos + 3, summary, &Font12, 1);

    // Line 2: keep host toggle
    char keep_str[24];
    snprintf(keep_str, sizeof(keep_str), "   使用主机配置");
    // 绘制复选框
    DrawRectangle(buf, 6, Ypos + 19, 9, 9, 0, 1);
    if (keep_host_set)
    {
        DrawRectangle(buf, 8, Ypos + 21, 5, 5, 1, 1);
    }
    Paint_DrawString_EN(buf, 4, Ypos + 19, keep_str, &Font12, 1);

    // Line 3: baudrate
    if (keep_host_set)
    {
        char baud_line[24];
        snprintf(baud_line, sizeof(baud_line), "波特率:%d", host_baudrate_value);
        Paint_DrawString_EN_CenterAtX(buf, 64, Ypos + 35, baud_line, &Font12, 1);
    }
    else
    {
        int baud_idx = uart_baud_selection;
        baud_idx = (baud_idx + BAUD_NUMBERS) % BAUD_NUMBERS;
        char baud_line[24];
        snprintf(baud_line, sizeof(baud_line), "波特率:%s", uart_baudrate_names[baud_idx]);
        Paint_DrawString_EN_CenterAtX(buf, 64, Ypos + 35, baud_line, &Font12, 1);
    }

    // Line 4: format D|P|S
    int db_idx = keep_host_set ? host_databits_index : uart_databits_selection;
    int parity_idx = keep_host_set ? host_parity_index : uart_parity_selection;
    int stop_idx = keep_host_set ? host_stopbits_index : uart_stopbits_selection;
    db_idx = (db_idx + 4) % 4;
    parity_idx = (parity_idx + 5) % 5;
    stop_idx = (stop_idx + 3) % 3;

    char data_str[8];
    char parity_str[12];
    char stop_str[8];
    snprintf(data_str, sizeof(data_str), "数:%s", uart_databits_names[db_idx]);
    snprintf(parity_str, sizeof(parity_str), "校:%s", uart_parity_names[parity_idx]);
    snprintf(stop_str, sizeof(stop_str), "停:%s", uart_stopbits_names[stop_idx]);

    Paint_DrawString_EN_CenterAtX(buf, 15, Ypos + 51, data_str, &Font12, 1);
    Paint_DrawString_EN_CenterAtX(buf, 57, Ypos + 51, parity_str, &Font12, 1);
    Paint_DrawString_EN_CenterAtX(buf, 106, Ypos + 51, stop_str, &Font12, 1);

    // Strikethrough when locked to host
    if (keep_host_set)
    {
        draw_strike_line(Ypos + 35 + uart_font_height / 2 + 1);
        draw_strike_line(Ypos + 51 + uart_font_height / 2 + 1);
    }
}

// Baudrate modal
void draw_uart_baud_selector()
{
    int height = uart_item_height * 2 + 4;
    int width = uart_change_width;
    int x = (SCREEN_WIDTH - width) / 2;
    int y = (SCREEN_HEIGHT - height) / 2;

    UIDisplayStr_font12(x + 3, y + uart_itemHeightOffset, "波特率:", 64, 0);
    {
        char line[16];
        snprintf(line, sizeof(line), "%s", uart_baudrate_names[temp_baud_selection]);
        UIDisplayStr_font12(x + 3 + 1, y + uart_item_height + uart_itemHeightOffset, line, 64, 0);
        // Highlight selected line by inverting its rectangle area
        int wv = (int)SSD1306_TextWidth(line, &Font12) + 6;
        if (wv > width - 2)
        {
            wv = width - 2;
        }
        InvertRect(buf, x + 1, y + uart_item_height, wv, uart_item_height);
    }
}

// Databits modal (2x2 layout, arrow marks current temp selection)
void draw_uart_databits_selector()
{
    // 3 lines total: title + 2 grid rows (2x2)
    int height = uart_item_height * 3 + 2;
    int width = uart_change_width;
    int x = (SCREEN_WIDTH - width) / 2;
    int y = (SCREEN_HEIGHT - height) / 2;

    UIDisplayStr_font12(x + 3, y + 5, "数据位:", width - 6, 0);
    int option_w = 34;
    int option_h = uart_item_height - 2;
    int start_y = y + uart_item_height + 2;
    for (int i = 0; i < 4; i++)
    {
        int col = i % 2;
        int row = i / 2;
        int opt_x = x + 9 + col * (option_w + 4);
        int opt_y = start_y + row * (option_h + 2);
        const char *label = uart_databits_names[i];
        char line[8];
        snprintf(line, sizeof(line), "%s", label);
        UIDisplayStr_font12(opt_x + 1, opt_y, line, 64, 0);
        if (temp_databits_selection == i)
        {
            // Invert the option's rectangle to indicate selection
            int wv = (int)SSD1306_TextWidth(line, &Font12) + 6;
            if (wv > option_w)
            {
                wv = option_w;
            }
            InvertRect(buf, opt_x - 2, opt_y - 1, wv, option_h);
        }
    }
}

// Parity modal (2x3 layout)
void draw_uart_parity_selector()
{
    // 3 lines total: title + 3 grid rows (2x3)
    int height = uart_item_height * 3 + 2;
    int width = uart_change_width + 12;
    int x = (SCREEN_WIDTH - width) / 2;
    int y = (SCREEN_HEIGHT - height) / 2;

    UIDisplayStr_font12(x + 3, y + 5, "校验位:", width - 6, 0);
    int option_w = 40;
    int option_h = uart_item_height - 2;
    int start_y = y + uart_item_height + 4;
    for (int i = 0; i < 3; i++)
    {
        int col = i % 2;
        int row = i / 2; // distributes across up to 3 rows
        int opt_x = x + 6 + col * (option_w + 8);
        int opt_y = start_y + row * (option_h);
        char line[12];
        snprintf(line, sizeof(line), "%s", uart_parity_names[i]);
        UIDisplayStr_font12(opt_x + 1, opt_y, line, 64, 0);
        if (temp_parity_selection == i)
        {
            // Invert the option's rectangle to indicate selection
            int wv = (int)SSD1306_TextWidth(line, &Font12) + 6;
            if (wv > option_w)
            {
                wv = option_w;
            }
            InvertRect(buf, opt_x - 2, opt_y - 1, wv, option_h);
        }
    }
}

// Stop bits modal (two options: 1 and 2; 1.5 remains display-only via host)
void draw_uart_stopbits_selector()
{
    // 2 lines total: title + 1 row (1 and 2 horizontally)
    int height = uart_item_height * 2 + 2;
    int width = uart_change_width;
    int x = (SCREEN_WIDTH - width) / 2;
    int y = (SCREEN_HEIGHT - height) / 2 - 2;

    UIDisplayStr_font12(x + 3, y + 5, "停止位:", width - 6, 0);

    int option_w = (width - 12) / 2 - 8; // Divide width for two horizontal options
    int option_h = uart_item_height - 2;
    int start_y = y + uart_item_height + 2;

    char line1[10];
    char line2[10];
    // Only indices 0 ("1") and 2 ("2") are selectable
    snprintf(line1, sizeof(line1), "1");
    snprintf(line2, sizeof(line2), "2");

    int opt1_x = x + 6;
    int opt2_x = x + 6 + option_w + 10;
    int opt_y = start_y + uart_itemHeightOffset - 1;

    UIDisplayStr_font12(opt1_x + 1, opt_y, line1, 64, 0);
    UIDisplayStr_font12(opt2_x + 1, opt_y, line2, 64, 0);

    // Invert the selected option rectangle
    int opt1_wv = (int)SSD1306_TextWidth(line1, &Font12) + 6;
    int opt2_wv = (int)SSD1306_TextWidth(line2, &Font12) + 6;
    if (opt1_wv > option_w)
    {
        opt1_wv = option_w;
    }
    if (opt2_wv > option_w)
    {
        opt2_wv = option_w;
    }
    if (temp_stopbits_selection == 0)
    {
        InvertRect(buf, opt1_x - 2, start_y, opt1_wv, option_h);
    }
    else if (temp_stopbits_selection == 2)
    {
        InvertRect(buf, opt2_x - 2, start_y, opt2_wv, option_h);
    }
}

// Generic selector handler (wraps)
void uart_handle_selector_operation(int *current_value, int max_value)
{
    // Corrected: Up moves to previous (decrement), Down moves to next (increment)
    if (opnUp)
    {
        if (*current_value == 0)
        {
            *current_value = max_value - 1;
        }
        else
        {
            (*current_value)--;
        }
    }
    if (opnDown)
    {
        if (*current_value == max_value - 1)
        {
            *current_value = 0;
        }
        else
        {
            (*current_value)++;
        }
    }
}

// Animation and render pipeline (mirrors serialread)
void anni_uartconfig()
{
    int xEnd, yEnd, widthEnd, heighEND;

    if (uartconfigIsRunning == false)
    {
        switch (nowselect_uartconfig_item)
        {
        case 0: // Keep host line
            xEnd = 1;
            yEnd = 17;
            widthEnd = 126;
            heighEND = 12;
            break;
        case 1: // Baud line
            xEnd = 1;
            yEnd = 33;
            widthEnd = 126;
            heighEND = 12;
            break;
        case 2: // Data item
            xEnd = 1;
            yEnd = 49;
            widthEnd = 27;
            heighEND = 13;
            break;
        case 3: // Parity item
            xEnd = 30;
            yEnd = 49;
            widthEnd = 54;
            heighEND = 13;
            break;
        case 4: // Stop item
            xEnd = 86;
            yEnd = 49;
            widthEnd = 40;
            heighEND = 13;
            break;
        default:
            xEnd = 1;
            yEnd = 17;
            widthEnd = 126;
            heighEND = 12;
            break;
        }
    }
    else
    {
        switch (nowselect_uartconfig_item)
        {
        case 0: // Keep host line
            heighEND = uart_item_height * 2 + 5;
            widthEnd = uart_change_width;
            xEnd = (SCREEN_WIDTH - widthEnd) / 2;
            yEnd = (SCREEN_HEIGHT - heighEND) / 2;
            break;
        case 1: // Baud modal
            heighEND = uart_item_height * 2 + 5;
            widthEnd = uart_change_width + 4;
            xEnd = (SCREEN_WIDTH - widthEnd) / 2;
            yEnd = (SCREEN_HEIGHT - heighEND) / 2;
            break;
        case 2: // Data bits modal
            heighEND = uart_item_height * 3;
            widthEnd = uart_change_width;
            xEnd = (SCREEN_WIDTH - widthEnd) / 2;
            yEnd = (SCREEN_HEIGHT - heighEND) / 2;
            break;
        case 3: // Parity modal
            heighEND = uart_item_height * 3;
            widthEnd = uart_change_width + 12;
            xEnd = (SCREEN_WIDTH - widthEnd) / 2;
            yEnd = (SCREEN_HEIGHT - heighEND) / 2;
            break;
        case 4: // Stop bits modal
            heighEND = uart_item_height * 2 + 5;
            widthEnd = uart_change_width - 2 + 6;
            xEnd = (SCREEN_WIDTH - widthEnd) / 2;
            yEnd = (SCREEN_HEIGHT - heighEND) / 2;
            break;

        default:
            heighEND = uart_item_height * 2;
            widthEnd = uart_change_width;
            xEnd = (SCREEN_WIDTH - widthEnd) / 2;
            yEnd = (SCREEN_HEIGHT - heighEND) / 2;
            break;
        }
    }

    if (uartconfig_infunction)
    {
        seedvalue = to_ms_since_boot(get_absolute_time());
    }

    // 判定如果xEnd,yEnd,widthEnd,heighEND与start相同则不进行动画
    bool if_no_animation = false;
    if (xEnd == xStart && yEnd == yStart && widthEnd == lengthStart && heighEND == heightStart)
    {
        if_no_animation = true;
    }
    if (!if_no_animation && footlength != 1 && !(uartconfigIsRunning && !uartconfig_infunction))
    {
        for (float i = 0; i <= 1; i += (footlength))
        {
            int x;
            int y;
            int width;
            int heigh;
            int addtion;
            if (uartconfig_infunction)
            {
                addtion = 0;
                // int addtion = A_line(0, 0, 8, i); // 用于infuction的弹簧效果
                x = easeInOutQuad(xStart, xEnd, i) + addtion;
                y = easeInOutQuad(yStart, yEnd, i) - addtion;
                width = easeInOutQuad(lengthStart, widthEnd, i) - addtion * 2;
                heigh = easeInOutQuad(heightStart, heighEND, i) + addtion * 2; // 这个动画还挺好考虑后续加 /////////////////////////
            }
            else
            {
                x = easeInOutQuad(xStart, xEnd, i);
                y = easeInOutQuad(yStart, yEnd, i);
                width = easeInOutQuad(lengthStart, widthEnd, i);
                heigh = easeInOutQuad(heightStart, heighEND, i);
            }

            draw_uartconfig_main(0);

            if (uartconfigIsRunning && !uartconfig_infunction)
            {
                ApplyBlurEffect(buf, 1); // 背景模糊层
            }
            else if (uartconfig_infunction && uartconfigIsRunning)
            {
                ApplyBlurEffect(buf, X2line_up(0, 1, i)); // 背景模糊层
            }
            else if (uartconfig_outfunction)
            {
                ApplyBlurEffect(buf, X2line_down(0.8, 0, i)); // 背景模糊层
            }
            else if (!uartconfigIsRunning)
            {
                // 无模糊 先画Sgate
                UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);
            }

            if (uartconfigIsRunning)
            {
                UIDrawSgate(x, y, width, heigh, 8, 6, 1, 2);
            }
            else if (uartconfig_outfunction)
            {
                UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);
            }

            if (uartconfigIsRunning)
            {
                switch (uartconfig_modal)
                {
                case UART_MODAL_BAUD:
                    draw_uart_baud_selector();
                    break;
                case UART_MODAL_DATABITS:
                    draw_uart_databits_selector();
                    break;
                case UART_MODAL_PARITY:
                    draw_uart_parity_selector();
                    break;
                case UART_MODAL_STOPBITS:
                    draw_uart_stopbits_selector();
                    break;
                default:
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
    uartconfig_infunction = false;
    uartconfig_outfunction = false;

    // End frame
    draw_uartconfig_main(0);
    if (uartconfigIsRunning)
    {
        ApplyBlurEffect(buf, 1);
        UIDrawSgate(xEnd, yEnd, widthEnd, heighEND, 8, 6, 1, 2);
        switch (uartconfig_modal)
        {
        case UART_MODAL_BAUD:
            draw_uart_baud_selector();
            break;
        case UART_MODAL_DATABITS:
            draw_uart_databits_selector();
            break;
        case UART_MODAL_PARITY:
            draw_uart_parity_selector();
            break;
        case UART_MODAL_STOPBITS:
            draw_uart_stopbits_selector();
            break;
        default:
            break;
        }
    }
    else
    {
        UIDrawSgate(xEnd, yEnd, widthEnd, heighEND, 8, 6, 1, 1);
    }

    render(buf, &frame_area);
    memset(buf, 0, SSD1306_BUF_LEN);
}

// Entry point
bool uartconfig()
{
    ResetOpn();
    exit_uartconfig_flag = false; // 确保每次进入都重置退出标志
    if_uartconfig_running = true; // 标记函数正在运行
    // apply_uart_settings();  //首次进入无需应用，保持现有设置
    extern bool send_pc_flag;
    send_pc_flag = true; // 进入时向上位机报告当前设置
    anni_uartconfig();

    while (true)
    {
        if (opnEnter || opnExit || opnUp || opnDown || exit_uartconfig_flag)
        {
            if (uartconfigIsRunning)
            {
                switch (uartconfig_modal)
                {
                case UART_MODAL_BAUD:
                    uart_handle_selector_operation(&temp_baud_selection, BAUD_NUMBERS);
                    if (opnEnter)
                    {
                        uart_baud_selection = temp_baud_selection;
                        apply_uart_settings();
                        uartconfigIsRunning = false;
                        uartconfig_outfunction = 1;
                    }
                    else if (opnExit)
                    {
                        uartconfigIsRunning = false;
                        uartconfig_outfunction = 1;
                    }
                    break;
                case UART_MODAL_DATABITS:
                    uart_handle_selector_operation(&temp_databits_selection, 4);
                    if (opnEnter)
                    {
                        uart_databits_selection = temp_databits_selection;
                        apply_uart_settings();
                        uartconfigIsRunning = false;
                        uartconfig_outfunction = 1;
                    }
                    else if (opnExit)
                    {
                        uartconfigIsRunning = false;
                        uartconfig_outfunction = 1;
                    }
                    break;
                case UART_MODAL_PARITY:
                    uart_handle_selector_operation(&temp_parity_selection, 3);
                    if (opnEnter)
                    {
                        uart_parity_selection = temp_parity_selection;
                        apply_uart_settings();
                        uartconfigIsRunning = false;
                        uartconfig_outfunction = 1;
                    }
                    else if (opnExit)
                    {
                        uartconfigIsRunning = false;
                        uartconfig_outfunction = 1;
                    }
                    break;
                case UART_MODAL_STOPBITS:
                    // Toggle only between indices 0 ("1") and 2 ("2")
                    if (opnUp || opnDown)
                    {
                        temp_stopbits_selection = (temp_stopbits_selection == 0) ? 2 : 0;
                    }
                    if (opnEnter)
                    {
                        uart_stopbits_selection = temp_stopbits_selection;
                        apply_uart_settings();
                        uartconfigIsRunning = false;
                        uartconfig_outfunction = 1;
                    }
                    else if (opnExit)
                    {
                        uartconfigIsRunning = false;
                        uartconfig_outfunction = 1;
                    }
                    break;
                default:
                    break;
                }
            }
            else
            {
                if (opnUp)
                {
                    nowselect_uartconfig_item = (nowselect_uartconfig_item + 4) % 5;
                }
                else if (opnDown)
                {
                    nowselect_uartconfig_item = (nowselect_uartconfig_item + 1) % 5;
                }

                if (opnEnter)
                {
                    switch (nowselect_uartconfig_item)
                    {
                    case 0: // keep host
                        keep_host_set = !keep_host_set;
                        if (keep_host_set)
                        {
                            apply_uart_settings_from_host();
                        }
                        else
                        {
                            apply_uart_settings();
                        }
                        break;
                    case 1: // baud modal
                        if (!keep_host_set)
                        {
                            temp_baud_selection = uart_baud_selection;
                            uartconfig_modal = UART_MODAL_BAUD;
                            uartconfigIsRunning = true;
                            uartconfig_infunction = 1;
                        }
                        break;
                    case 2: // data modal
                        if (!keep_host_set)
                        {
                            temp_databits_selection = uart_databits_selection;
                            uartconfig_modal = UART_MODAL_DATABITS;
                            uartconfigIsRunning = true;
                            uartconfig_infunction = 1;
                        }
                        break;
                    case 3: // parity modal
                        if (!keep_host_set)
                        {
                            temp_parity_selection = uart_parity_selection;
                            uartconfig_modal = UART_MODAL_PARITY;
                            uartconfigIsRunning = true;
                            uartconfig_infunction = 1;
                        }
                        break;
                    case 4: // stop modal
                        if (!keep_host_set)
                        {
                            temp_stopbits_selection = (uart_stopbits_selection == 1) ? 0 : uart_stopbits_selection;
                            uartconfig_modal = UART_MODAL_STOPBITS;
                            uartconfigIsRunning = true;
                            uartconfig_infunction = 1;
                        }
                        break;
                    default:
                        break;
                    }
                }

                if (opnExit || exit_uartconfig_flag)
                {
                    if_uartconfig_running = false; // Mark function as no longer running
                    exit_uartconfig_flag = false; // Reset the flag for next time
                    ResetOpn();
                    return false;
                }
            }

            ResetOpn();
            anni_uartconfig();
        }

        // Continuous repaint for animations/background
        anni_uartconfig();

        extern bool opnPCchangemode;
        if (opnLeft || opnRight || opnPCchangemode)
        {
            if_uartconfig_running = false; // Mark function as no longer running
            return true;
        }
    }

    if_uartconfig_running = false; // Mark function as no longer running
    return false; // Explicit return at exit path
}

extern bool functionIsRunning;
void uartconfig_fromUI()
{
    functionIsRunning = uartconfig();
}

// RP2040 TinyUSB CDC: 定期报告主机的线路编码（波特率、数据位、校验位、停止位）
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "tusb.h"

static volatile bool g_has_line_coding = false;
static cdc_line_coding_t g_line_coding;
static volatile bool g_dtr = false;
static volatile bool g_rts = false;

static const char *parity_to_str(uint8_t parity)
{
    switch (parity)
    {
    case 0:
        return "无";
    case 1:
        return "奇";
    case 2:
        return "偶";
    case 3:
        return "标记";
    case 4:
        return "空格";
    default:
        return "未知";
    }
}

static const char *stopbits_to_str(uint8_t stop)
{
    switch (stop)
    {
    case 0:
        return "1";
    case 1:
        return "1.5";
    case 2:
        return "2";
    default:
        return "?";
    }
}

bool usbhost_config_changed()
{
    printf("CDC Line Coding: Baud=%lu, Data=%u, Parity=%s, Stop=%s, DTR=%u, RTS=%u\r\n",
           (unsigned long)g_line_coding.bit_rate,
           (unsigned)g_line_coding.data_bits,
           parity_to_str(g_line_coding.parity),
           stopbits_to_str(g_line_coding.stop_bits),
           g_dtr ? 1u : 0u,
           g_rts ? 1u : 0u);
}

// // TinyUSB CDC 回调：线路状态（DTR/RTS）
// void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
//     (void)itf;
//     g_dtr = dtr;
//     g_rts = rts;
//     StartStateLED();
//     // printf("DTR=%d, RTS=%d\r\n", (int)dtr, (int)rts);
// }

// TinyUSB CDC 回调：线路编码（波特率、数据位、校验位、停止位）
void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *p_line_coding)
{
    (void)itf;
    g_line_coding = *p_line_coding;
    g_has_line_coding = true;
    host_baudrate_value = g_line_coding.bit_rate;
    host_databits_index = g_line_coding.data_bits - 5; // 5-8 映射到 0-3
    host_parity_index = g_line_coding.parity;          // 0-4 直接映射
    host_stopbits_index = g_line_coding.stop_bits;     // 0-2 直接映射

    apply_uart_settings_from_host();
}

#endif
