#ifndef _SSD1306_C
#define _SSD1306_C
/*
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <string.h>
#include <stdlib.h>
#include <cstdio>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"

#include "Fonts/fonts.h"
#include "CONFIG_FLO.hpp"

#include "menu/ssd1306.hpp"

/*
    原作者的说明：

    Example code to talk to an SSD1306-based OLED display

   The SSD1306 is an OLED/PLED driver chip, capable of driving displays up to
   128x64 pixels.

   NOTE: Ensure the device is capable of being driven at 3.3v NOT 5v. The Pico
   GPIO (and therefore I2C) cannot be used at 5v.

   You will need to use a level shifter on the I2C lines if you want to run the
   board at 5v.

   Connections on Raspberry Pi Pico board, other boards may vary.

   GPIO PICO_DEFAULT_I2C_SDA_PIN (on Pico this is GP4 (pin 6)) -> SDA on display
   board
   GPIO PICO_DEFAULT_I2C_SCL_PIN (on Pico this is GP5 (pin 7)) -> SCL on
   display board
   3.3v (pin 36) -> VCC on display board
   GND (pin 38)  -> GND on display board
*/

// Define the size of the display we have attached. This can vary, make sure you
// have the right size defined or the output will look rather odd!
// Code has been tested on 128x32 and 128x64 OLED displays

/*
    最好去看一下我写的丐版使用手册啊哈哈哈因为我也感觉很抽象
    日志：
    把8x8的字符二进制扩充到SACII32~127
    增加了矩形，椭圆，线条的绘制方法
    修正了溢出边界的处理逻辑
    增加并再次优化了缓存区清屏
    解决了i2c0/i2c1的切换问题
    文字显示提供了增强模式
    提供了文本缩放功能
    提供了跨页面文字的分析重组显示功能
*/

// 定义了一个结构体render_area，用于描述渲染区域的起始列、结束列、起始页和结束页，以及缓冲区长度。
// struct render_area {
//     uint8_t start_col;
//     uint8_t end_col;
//     uint8_t start_page;
//     uint8_t end_page;

//     int buflen;
// };

// 计算渲染区域的扁平化缓冲区的长度
void calc_render_area_buflen(struct render_area *area)
{
    area->buflen = (area->end_col - area->start_col + 1) * (area->end_page - area->start_page + 1);
}

// 判断是否定义了i2c_default。

// 用于向SSD1306发送命令
void SSD1306_send_cmd(uint8_t cmd)
{
    // I2C write process expects a control byte followed by data
    // this "data" can be a command or data to follow up a command
    // Co = 1, D/C = 0 => the driver expects a command
    uint8_t buf[2] = {0x80, cmd};
    i2c_write_blocking(I2C_PORT, SSD1306_I2C_ADDR, buf, 2, false);
}
void SSD1306_send_cmd_list(uint8_t *buf, int num)
{
    for (int i = 0; i < num; i++)
        SSD1306_send_cmd(buf[i]);
}

// 用于向SSD1306发送数据缓冲区
void SSD1306_send_buf(uint8_t buf[], int buflen)
{
    // in horizontal addressing mode, the column address pointer auto-increments
    // and then wraps around to the next page, so we can send the entire frame
    // buffer in one gooooooo!

    // copy our frame buffer into a new buffer because we need to add the control byte
    // to the beginning

    uint8_t *temp_buf = (uint8_t *)malloc(buflen + 1);

    temp_buf[0] = 0x40;
    memcpy(temp_buf + 1, buf, buflen);

    i2c_write_blocking(I2C_PORT, SSD1306_I2C_ADDR, temp_buf, buflen + 1, false);

    free(temp_buf);
}

// 准备屏幕缓存区
uint8_t buf[SSD1306_BUF_LEN];
struct render_area frame_area = {
    start_col : 0,
    end_col : SSD1306_WIDTH - 1,
    start_page : 0,
    end_page : SSD1306_NUM_PAGES - 1
};

void SSD1306_init()
{
    // Some of these commands are not strictly necessary as the reset
    // process defaults to some of these but they are shown here
    // to demonstrate what the initialization sequence looks like
    // Some configuration values are recommended by the board manufacturer

    uint8_t cmds[] = {
        SSD1306_SET_DISP, // set display off
        /* memory mapping */
        SSD1306_SET_MEM_MODE, // set memory address mode 0 = horizontal, 1 = vertical, 2 = page
        0x00,                 // horizontal addressing mode
        /* resolution and layout */
        SSD1306_SET_DISP_START_LINE,    // set display start line to 0
        SSD1306_SET_SEG_REMAP | 0x01,   // set segment re-map, column address 127 is mapped to SEG0
        SSD1306_SET_MUX_RATIO,          // set multiplex ratio
        SSD1306_HEIGHT - 1,             // Display height - 1
        SSD1306_SET_COM_OUT_DIR | 0x08, // set COM (common) output scan direction. Scan from bottom up, COM[N-1] to COM0
        SSD1306_SET_DISP_OFFSET,        // set display offset
        0x00,                           // no offset
        SSD1306_SET_COM_PIN_CFG,        // set COM (common) pins hardware configuration. Board specific magic number.
                                        // 0x02 Works for 128x32, 0x12 Possibly works for 128x64. Other options 0x22, 0x32
#if ((SSD1306_WIDTH == 128) && (SSD1306_HEIGHT == 32))
        0x02,
#elif ((SSD1306_WIDTH == 128) && (SSD1306_HEIGHT == 64))
        0x12,
#else
        0x02,
#endif
        /* timing and driving scheme */
        SSD1306_SET_DISP_CLK_DIV, // set display clock divide ratio
        0x80,                     // div ratio of 1, standard freq
        SSD1306_SET_PRECHARGE,    // set pre-charge period
        0xF1,                     // Vcc internally generated on our board
        SSD1306_SET_VCOM_DESEL,   // set VCOMH deselect level
        0x30,                     // 0.83xVcc
        /* display */
        SSD1306_SET_CONTRAST, // set contrast control
        0xFF,
        SSD1306_SET_ENTIRE_ON,     // set entire display on to follow RAM content
        SSD1306_SET_NORM_DISP,     // set normal (not inverted) display
        SSD1306_SET_CHARGE_PUMP,   // set charge pump
        0x14,                      // Vcc internally generated on our board
        SSD1306_SET_SCROLL | 0x00, // deactivate horizontal scrolling if set. This is necessary as memory writes will corrupt if scrolling was enabled
        SSD1306_SET_DISP | 0x01,   // turn display on
    };

    SSD1306_send_cmd_list(cmds, count_of(cmds));

    calc_render_area_buflen(&frame_area);

    memset(buf, 0, SSD1306_BUF_LEN);
    render(buf, &frame_area);
}

void SSD1306_invert(bool invert)
{
    /*
    反转显示模式命令
    0xA6: 正常显示 (亮色像素点亮)
    0xA7: 反色显示 (暗色像素点亮)
    */
    uint8_t cmd = invert ? SSD1306_SET_INV_DISP : SSD1306_SET_NORM_DISP;
    SSD1306_send_cmd(cmd);
}

// // ==================== 上位机接口：图像发送 ====================
extern bool Enable_PC_Interface; // 在 functions.cpp 中定义
uint8_t streaming_buf[SSD1306_BUF_LEN]; // 二级推流缓冲区

/* 
   二级缓存区读取标志逻辑：
   true: 缓冲区为空或推流已结束，主程序可以写入新的一帧数据。
   false: 正在推流中，主程序禁止写入，待推流结束后会自动重置。
*/
volatile bool streaming_buf_ready_push = true; 

#include "tusb.h"

void SSD1306_PrintBufRaw()
{
    if (!Enable_PC_Interface) return;
    if (!streaming_buf_ready_push) return;

    printf("SEND SCANE\r\n"); // 帧头标识，便于上位机识别帧的开始

    // 帧格式：#PIVOSCAN# + [序列(uint8_t), 数据(uint8_t)] * N + #PSM#
    const uint8_t header[] = {'#','P','I','V','O','S','C','A','N','#'};
    const uint8_t footer[] = {'#','P','S','M','#'};

    const size_t frame_size = sizeof(header) + (SSD1306_BUF_LEN * 2) + sizeof(footer);

    uint8_t frame[sizeof(header) + (SSD1306_BUF_LEN * 2) + sizeof(footer)];
    size_t offset = 0;

    memcpy(frame + offset, header, sizeof(header));
    offset += sizeof(header);

    for (size_t i = 0; i < SSD1306_BUF_LEN; i++) {
        frame[offset++] = (uint8_t)(i & 0xFF);
        frame[offset++] = streaming_buf[i];
    }

    memcpy(frame + offset, footer, sizeof(footer));

    // 一次性写出完整帧，并刷新 C 库缓冲区，确保发送完成后再退出。
    fwrite(frame, 1, frame_size, stdout);
    fflush(stdout);

    streaming_buf_ready_push = false; 
}
void SSD1306_scroll(bool on)
{
    // configure horizontal scrolling
    uint8_t cmds[] = {
        SSD1306_SET_HORIZ_SCROLL | 0x00,
        0x00, // dummy byte
        0x00, // start page 0
        0x00, // time interval
        0x03, // end page 3 SSD1306_NUM_PAGES ??
        0x00, // dummy byte
        0xFF, // dummy byte

        SSD1306_SET_SCROLL | (uint8_t)(on ? 0x01 : 0) // 艹，这个地方警告就警告吧，应该没问题
        // Start/stop scrolling
    };

    SSD1306_send_cmd_list(cmds, count_of(cmds));
}

// 用于更新显示器上的一部分内容，通过给定的渲染区域和缓冲区数据
void render(uint8_t *buf, struct render_area *area)
{
    // 在实际渲染到硬件前，如果开启了上位机接口，将数据同步至二级缓冲区
    // 逻辑：只有当 streaming_buf_ready_push 为 false（即前一帧已推流完成）时才更新数据
    if (Enable_PC_Interface && !streaming_buf_ready_push)
    {
        memcpy(streaming_buf, buf, SSD1306_BUF_LEN);
        streaming_buf_ready_push = true; // 写入完成，标记有新数据等待推流
    }

    // 更新显示器的部分内容，使用一个渲染区域
    uint8_t cmds[] = {
        SSD1306_SET_COL_ADDR,
        area->start_col,
        area->end_col,
        SSD1306_SET_PAGE_ADDR,
        area->start_page,
        area->end_page};

    SSD1306_send_cmd_list(cmds, count_of(cmds));
    SSD1306_send_buf(buf, area->buflen);
}

// 用于设置显示缓冲区中指定像素（即点）的状态，即点亮或熄灭
void SetPixel(uint8_t *buf, int x, int y, bool on)
{
    if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT)
    {
        return;
    }

    // 根据所处的地址模式确定正确设置的位的计算依赖于我们所处的模式。此代码假设是水平模式。

    // SSD1306 上的视频 RAM 被分成了 8 行，每个像素一个比特。
    // 每行是 128 像素宽，8 像素高，每个字节垂直排列，因此字节 0 是 x=0，y=0->7，
    // 字节 1 是 x=1，y=0->7，依此类推。

    const int BytesPerRow = SSD1306_WIDTH; // x 像素，1bpp，但每行高度为 8 像素，因此 (x / 8) * 8

    int byte_idx = (y / 8) * BytesPerRow + x;
    uint8_t byte = buf[byte_idx];

    if (on)
        byte |= 1 << (y % 8);
    else
        byte &= ~(1 << (y % 8));

    buf[byte_idx] = byte;
}
// Basic Bresenhams.

// 用于在缓冲区中绘制一条直线
void DrawLine(uint8_t *buf, int x0, int y0, int x1, int y1, bool on)
{

    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int e2;

    // Bresenham算法实现找到直线
    while (true)
    {
        SetPixel(buf, x0, y0, on);
        if (x0 == x1 && y0 == y1)
            break;
        e2 = 2 * err;

        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

// 使用Bresenham算法生成矩形
void DrawRectangle(uint8_t *buf, int x0, int y0, int width, int height, bool solid, bool on)
{
    int x1 = x0 + width - 1;
    int y1 = y0 + height - 1;

    if (solid)
    {
        for (int y = y0; y <= y1; ++y)
        {
            DrawLine(buf, x0, y, x1, y, on);
        }
    }
    else
    {
        DrawLine(buf, x0, y0, x1, y0, on);
        DrawLine(buf, x0, y0, x0, y1, on);
        DrawLine(buf, x1, y0, x1, y1, on);
        DrawLine(buf, x0, y1, x1, y1, on);
    }
}

// 清屏
void ClearScreen_Run(uint8_t *buf, struct render_area *area)
{
    memset(buf, 0, SSD1306_BUF_LEN);
    render(buf, area);
}

void ClearScreen(uint8_t *buf, struct render_area *area)
{
    memset(buf, 0, SSD1306_BUF_LEN);
}

// 使用Bresenham算法生成椭圆形
void DrawEllipse(uint8_t *buf, int x0, int y0, int a, int b, bool solid, bool on)
{
    int x, y;
    double d;

    // 计算椭圆的上半部分
    x = 0;
    y = b;
    d = b * b - a * a * b + 0.25 * a * a;

    while (b * b * (x + 1) < a * a * (y - 0.5))
    {
        if (solid)
        {
            DrawLine(buf, x0 - x, y0 + y, x0 + x, y0 + y, on);
            DrawLine(buf, x0 - x, y0 - y, x0 + x, y0 - y, on);
        }
        else
        {
            SetPixel(buf, x0 - x, y0 + y, on);
            SetPixel(buf, x0 + x, y0 + y, on);
            SetPixel(buf, x0 - x, y0 - y, on);
            SetPixel(buf, x0 + x, y0 - y, on);
        }

        if (d < 0)
        {
            d += b * b * (2 * x + 3);
        }
        else
        {
            d += b * b * (2 * x + 3) + a * a * (-2 * y + 2);
            y--;
        }
        x++;
    }

    d = b * b * (x + 0.5) * (x + 0.5) + a * a * (y - 1) * (y - 1) - a * a * b * b;

    while (y >= 0)
    {
        if (solid)
        {
            DrawLine(buf, x0 - x, y0 + y, x0 + x, y0 + y, on);
            DrawLine(buf, x0 - x, y0 - y, x0 + x, y0 - y, on);
        }
        else
        {
            SetPixel(buf, x0 - x, y0 + y, on);
            SetPixel(buf, x0 + x, y0 + y, on);
            SetPixel(buf, x0 - x, y0 - y, on);
            SetPixel(buf, x0 + x, y0 - y, on);
        }

        if (d > 0)
        {
            d += a * a * (-2 * y + 3);
        }
        else
        {
            d += b * b * (2 * x + 2) + a * a * (-2 * y + 3);
            x++;
        }
        y--;
    }
}

// 用于获取字符在字体表中的索引
inline int GetFontIndex(uint8_t ch)
{
    if (ch >= 32 && ch <= 127)
    {
        return ch - ' ';
    }
    else
    {
        return 0; // Not got that char so space.
    }
}

static cFONT *GetChineseFontForHeight(uint16_t height)
{
    if (height <= 14)
    {
        return &Font12CN;
    }
    return &Font12CN;
}

static sFONT *GetAsciiFontForHeight(uint16_t height)
{
    if (height <= 8)
    {
        return &Font8;
    }
    if (height <= 14)
    {
        return &Font12;
    }
    if (height <= 16)
    {
        return &Font16;
    }
    if (height <= 20)
    {
        return &Font20;
    }
    return &Font24;
}

static int Utf8CharLen(const char *text)
{
    if (text == NULL || *text == '\0')
    {
        return 0;
    }

    const unsigned char lead = (unsigned char)text[0];
    if (lead < 0x80)
    {
        return 1;
    }
    if ((lead & 0xE0) == 0xC0)
    {
        return text[1] ? 2 : 1;
    }
    if ((lead & 0xF0) == 0xE0)
    {
        return (text[1] && text[2]) ? 3 : 1;
    }
    if ((lead & 0xF8) == 0xF0)
    {
        return (text[1] && text[2] && text[3]) ? 4 : 1;
    }
    return 1;
}

static const CH_CN *FindChineseGlyph(const char *text, cFONT *Font)
{
    if (Font == NULL || text == NULL || Utf8CharLen(text) != 3)
    {
        return NULL;
    }

    for (uint16_t i = 0; i < Font->size; i++)
    {
        if (text[0] == Font->table[i].index[0] &&
            text[1] == Font->table[i].index[1] &&
            text[2] == Font->table[i].index[2])
        {
            return &Font->table[i];
        }
    }
    return NULL;
}

static void DrawChineseGlyph(uint8_t *buf, int16_t Xpoint, int16_t Ypoint, const CH_CN *glyph,
                             cFONT *Font, bool on)
{
    if (glyph == NULL || Font == NULL)
    {
        return;
    }

    Ypoint = Ypoint -3; // 字体基线调整，具体值可能需要根据实际字体进行微调

    const unsigned char *ptr = (const unsigned char *)glyph->matrix;
    for (uint16_t Page = 0; Page < Font->Height; Page++)
    {
        for (uint16_t Column = 0; Column < Font->Width; Column++)
        {
            if (*ptr & (0x80 >> (Column % 8)))
            {
                SetPixel(buf, Xpoint + Column, Ypoint + Page, on);
            }
            if (Column % 8 == 7)
            {
                ptr++;
            }
        }
        if (Font->Width % 8 != 0)
        {
            ptr++;
        }
    }
}

static uint16_t GetGlyphWidthUTF8(const char *text, sFONT *AsciiFont, cFONT *CnFont)
{
    if (text == NULL || *text == '\0' || *text == '\r' || *text == '\n')
    {
        return 0;
    }

    if ((unsigned char)text[0] < 0x80)
    {
        if (AsciiFont)
        {
            return AsciiFont->Width;
        }
        return CnFont ? CnFont->ASCII_Width : 0;
    }

    if (CnFont)
    {
        return CnFont->Width;
    }
    return AsciiFont ? AsciiFont->Width : 0;
}

static uint16_t GetLineHeight(sFONT *AsciiFont, cFONT *CnFont)
{
    uint16_t asciiHeight = AsciiFont ? AsciiFont->Height : 0;
    uint16_t cnHeight = CnFont ? CnFont->Height : 0;
    return asciiHeight > cnHeight ? asciiHeight : cnHeight;
}

static void DrawGlyphUTF8(uint8_t *buf, int16_t Xpoint, int16_t Ypoint, const char *text,
                          sFONT *AsciiFont, cFONT *CnFont, bool on)
{
    if (text == NULL || *text == '\0')
    {
        return;
    }

    if ((unsigned char)text[0] < 0x80)
    {
        if (AsciiFont)
        {
            Paint_DrawChar(buf, Xpoint, Ypoint, text[0], AsciiFont, on);
        }
        return;
    }

    const CH_CN *glyph = FindChineseGlyph(text, CnFont);
    if (glyph)
    {
        DrawChineseGlyph(buf, Xpoint, Ypoint, glyph, CnFont, on);
        return;
    }

    if (AsciiFont)
    {
        Paint_DrawChar(buf, Xpoint, Ypoint, '?', AsciiFont, on);
    }
}

static uint16_t SSD1306_TextWidth_Impl(const char *text, sFONT *AsciiFont, cFONT *CnFont)
{
    if (text == NULL)
    {
        return 0;
    }

    uint16_t currentWidth = 0;
    uint16_t maxWidth = 0;
    while (*text != '\0')
    {
        if (*text == '\r' || *text == '\n')
        {
            if (currentWidth > maxWidth)
            {
                maxWidth = currentWidth;
            }
            currentWidth = 0;
            if (*text == '\r' && *(text + 1) == '\n')
            {
                text += 2;
            }
            else
            {
                text++;
            }
            continue;
        }

        currentWidth += GetGlyphWidthUTF8(text, AsciiFont, CnFont);
        text += Utf8CharLen(text);
    }

    return currentWidth > maxWidth ? currentWidth : maxWidth;
}

static size_t SSD1306_TextBytesForWidth_Impl(const char *text, uint16_t maxWidth,
                                             sFONT *AsciiFont, cFONT *CnFont)
{
    if (text == NULL)
    {
        return 0;
    }

    size_t bytes = 0;
    uint16_t currentWidth = 0;
    while (text[bytes] != '\0')
    {
        if (text[bytes] == '\r' || text[bytes] == '\n')
        {
            break;
        }

        uint16_t glyphWidth = GetGlyphWidthUTF8(text + bytes, AsciiFont, CnFont);
        if (currentWidth + glyphWidth > maxWidth)
        {
            break;
        }

        int advance = Utf8CharLen(text + bytes);
        if (advance <= 0)
        {
            break;
        }

        currentWidth += glyphWidth;
        bytes += (size_t)advance;
    }
    return bytes;
}

static void Paint_DrawString_UTF8_Impl(uint8_t *buf, WORD Xstart, WORD Ystart, const char *pString,
                                       sFONT *AsciiFont, cFONT *CnFont, bool on)
{
    WORD Xpoint = Xstart;
    WORD Ypoint = Ystart;

    while (pString && *pString != '\0')
    {
        if (*pString == '\r' || *pString == '\n')
        {
            Xpoint = Xstart;
            Ypoint += GetLineHeight(AsciiFont, CnFont);
            if (*pString == '\r' && *(pString + 1) == '\n')
            {
                pString += 2;
            }
            else
            {
                pString++;
            }
            continue;
        }

        DrawGlyphUTF8(buf, Xpoint, Ypoint, pString, AsciiFont, CnFont, on);
        Xpoint += GetGlyphWidthUTF8(pString, AsciiFont, CnFont);
        pString += Utf8CharLen(pString);
    }
}

static void Paint_DrawString_Multiline_UTF8_Impl(uint8_t *buf, WORD Xstart, WORD Ystart,
                                                 const char *pString, sFONT *AsciiFont,
                                                 cFONT *CnFont, bool on)
{
    WORD Xpoint = Xstart;
    WORD Ypoint = Ystart;
    const WORD lineHeight = GetLineHeight(AsciiFont, CnFont);

    while (pString && *pString != '\0')
    {
        if (*pString == '\r' || *pString == '\n')
        {
            Xpoint = Xstart;
            Ypoint += lineHeight;
            if (*pString == '\r' && *(pString + 1) == '\n')
            {
                pString += 2;
            }
            else
            {
                pString++;
            }
            continue;
        }

        const uint16_t glyphWidth = GetGlyphWidthUTF8(pString, AsciiFont, CnFont);
        if ((Xpoint + glyphWidth) > SSD1306_WIDTH)
        {
            Xpoint = Xstart;
            Ypoint += lineHeight;
        }

        DrawGlyphUTF8(buf, Xpoint, Ypoint, pString, AsciiFont, CnFont, on);
        Xpoint += glyphWidth;
        pString += Utf8CharLen(pString);
    }
}

// // 用于在缓冲区中写入一个字符
// static void WriteChar(uint8_t *buf, int16_t x, int16_t y, uint8_t ch, uint8_t XMultiples, uint8_t YMultiples) {
//     if (x > SSD1306_WIDTH  || y > SSD1306_HEIGHT)
//         return;

//     // 目前，仅在 Y 轴行边界上进行写入（每 8 个垂直像素）

//     int idx = GetFontIndex(ch);

//     for (int j = 0 ,idy = 0; j < 8; j++)
//     {

//         unsigned int value = static_cast<unsigned int>(font[idx*8+idy]);
//         for (int jm = 0; jm < XMultiples; jm++)
//         {
//             for (int i = 0; i < 8; i++) {
//                 for (int im = 0; im < YMultiples; im++)
//                 {
//                     if( (value >> i) & 1){
//                         SetPixel(buf, x + j * XMultiples + jm, y + i * YMultiples + im, true);
//                     }
//                 }
//             }

//         }
//         idy++;
//     }
// }

// // 用于在缓存区写入一串字符
// void WriteString(uint8_t *buf, int16_t x, int16_t y, char *str, uint8_t XMultiples, uint8_t YMultiples) {

//     while (*str) {
//         WriteChar(buf, x, y, *str++, XMultiples, YMultiples);
//         x+=(8*XMultiples);
//     }
// }

// void WriteString_Mid(uint8_t *buf, int16_t x, int16_t y, char *str, uint8_t XMultiples, uint8_t YMultiples) {
//     char *temp = str;
//     int16_t length = 0;
//     while (*str) {
//         length++;
//         str++;
//     }
//     int16_t movePixels = x - (length * 8 * XMultiples / 2);
//     while (*temp) {
//         WriteChar(buf, movePixels, y, *temp++, XMultiples, YMultiples);
//         movePixels+=(8*XMultiples);
//     }
// }

/*****************新版多字体输入********* */

void Paint_DrawChar(uint8_t *buf, int16_t Xpoint, int16_t Ypoint, const char Acsii_Char,
                    sFONT *Font, bool on)
{
    UWORD Page, Column;

    if (Xpoint > SSD1306_WIDTH || Ypoint > SSD1306_HEIGHT || Xpoint < 0 - Font->Width - 1 || Ypoint < 0 - Font->Height - 1)
    {
        return;
    }

    uint32_t Char_Offset = (Acsii_Char - ' ') * Font->Height * (Font->Width / 8 + (Font->Width % 8 ? 1 : 0));
    const unsigned char *ptr = &Font->table[Char_Offset];

    for (Page = 0; Page < Font->Height; Page++)
    {
        for (Column = 0; Column < Font->Width; Column++)
        {

            // To determine whether the font background color and screen background color is consistent
            if (*ptr & (0x80 >> (Column % 8)))
            {
                // 字体
                SetPixel(buf, Xpoint + Column, Ypoint + Page, on);
            }
            else
            {
                // SetPixel(buf,Xpoint + Column, Ypoint + Page, on);
            }
            // One pixel is 8 bits
            if (Column % 8 == 7)
                ptr++;
        } // Write a line
        if (Font->Width % 8 != 0)
            ptr++;
    } // Write all
}

void Paint_DrawString_Multiline(uint8_t *buf, WORD Xstart, WORD Ystart, const char *pString,
                                sFONT *Font, bool on)
{
    Paint_DrawString_Multiline_UTF8_Impl(buf, Xstart, Ystart, pString, Font,
                                         GetChineseFontForHeight(Font->Height), on);
}

void Paint_DrawString_Multiline_CN(uint8_t *buf, WORD Xstart, WORD Ystart, const char *pString,
                                   cFONT *Font, bool on)
{
    Paint_DrawString_Multiline_UTF8_Impl(buf, Xstart, Ystart, pString,
                                         GetAsciiFontForHeight(Font->Height), Font, on);
}

void Paint_DrawString_EN(uint8_t *buf, WORD Xstart, WORD Ystart, const char *pString,
                         sFONT *Font, bool on)
{
    Paint_DrawString_UTF8_Impl(buf, Xstart, Ystart, pString, Font,
                               GetChineseFontForHeight(Font->Height), on);
}

void Paint_DrawString_CN(uint8_t *buf, WORD Xstart, WORD Ystart, const char *pString,
                         cFONT *Font, bool on)
{
    Paint_DrawString_UTF8_Impl(buf, Xstart, Ystart, pString,
                               GetAsciiFontForHeight(Font->Height), Font, on);
}

void Paint_DrawString_EN_CenterAtX(uint8_t *buf, WORD CenterX, WORD Ystart, const char *pString,
                                   sFONT *Font, bool on)
{
    uint16_t totalWidth = SSD1306_TextWidth(pString, Font);
    WORD Xstart = CenterX - (totalWidth / 2);
    Paint_DrawString_EN(buf, Xstart, Ystart, pString, Font, on);
}

void Paint_DrawString_CN_CenterAtX(uint8_t *buf, WORD CenterX, WORD Ystart, const char *pString,
                                   cFONT *Font, bool on)
{
    uint16_t totalWidth = SSD1306_TextWidth_Impl(pString, GetAsciiFontForHeight(Font->Height), Font);
    WORD Xstart = CenterX - (totalWidth / 2);
    Paint_DrawString_CN(buf, Xstart, Ystart, pString, Font, on);
}

uint16_t SSD1306_TextWidth(const char *text, sFONT *Font)
{
    return SSD1306_TextWidth_Impl(text, Font, GetChineseFontForHeight(Font ? Font->Height : 12));
}

size_t SSD1306_TextBytesForWidth(const char *text, uint16_t maxWidth, sFONT *Font)
{
    return SSD1306_TextBytesForWidth_Impl(text, maxWidth, Font,
                                          GetChineseFontForHeight(Font ? Font->Height : 12));
}


// 显示小数点数字
#define ARRAY_LEN 15
void Paint_DrawNum(uint8_t *buf, WORD Xpoint, WORD Ypoint, double Nummber,
                   UWORD Digit, sFONT *Font, bool on)
{
    char Str[ARRAY_LEN];

    // 格式化数字为字符串
    sprintf(Str, "%.*lf", Digit, Nummber);

    // 如果是0位小数，去除小数点
    if (Digit == 0)
    {
        // 遍历并寻找小数点并去除
        char *dot = strchr(Str, '.');
        if (dot)
        {
            *dot = '\0'; // 将小数点替换为字符串结束符
        }
    }

    // 调用显示函数来显示该数字
    Paint_DrawString_EN(buf, Xpoint, Ypoint, Str, Font, on);
}

// 显示数字int
void Paint_DrawInt(uint8_t *buf, WORD Xpoint, WORD Ypoint, int Number,
                   sFONT *Font, bool on)
{
    char Str[ARRAY_LEN];

    // 格式化整数为字符串
    sprintf(Str, "%d", Number);

    // 调用显示函数来显示该整数
    Paint_DrawString_EN(buf, Xpoint, Ypoint, Str, Font, on);
}

void InvertRect(uint8_t *buf, int16_t x0, int16_t y0, int16_t width, int16_t height) {
    int16_t x1 = x0 + width - 1;
    int16_t y1 = y0 + height - 1;
    // 限定在屏幕范围内
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= SSD1306_WIDTH) x1 = SSD1306_WIDTH - 1;
    if (y1 >= SSD1306_HEIGHT) y1 = SSD1306_HEIGHT - 1;

    for (int16_t y = y0; y <= y1; y++) {
        for (int16_t x = x0; x <= x1; x++) {
            int16_t byte_idx = (y / 8) * SSD1306_WIDTH + x;
            uint8_t bit_mask = 1 << (y % 8);
            buf[byte_idx] ^= bit_mask; // 翻转该像素
        }
    }
}

#endif
