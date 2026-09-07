#ifndef _SSD1306_H
#define _SSD1306_H

// 显示配置宏（根据实际屏幕尺寸选择）
#define SSD1306_WIDTH 128
#if !defined(SSD1306_HEIGHT)
#define SSD1306_HEIGHT 64  // 默认64像素高度
#endif

// I2C 地址及参数
#define SSD1306_I2C_ADDR 0x3C


// I2C 时钟速率，默认为 400，但通常可以超频以提高显示器响应速度
// 在 32 和 84 像素高度的设备上测试 1000 时，工作正常
#define SSD1306_I2C_CLK 1000

// SSD1306 命令定义（参考数据手册）
#define SSD1306_SET_MEM_MODE 0x20
#define SSD1306_SET_COL_ADDR 0x21
#define SSD1306_SET_PAGE_ADDR 0x22
#define SSD1306_SET_HORIZ_SCROLL 0x26
#define SSD1306_SET_SCROLL 0x2E
#define SSD1306_SET_DISP_START_LINE 0x40
#define SSD1306_SET_CONTRAST 0x81
#define SSD1306_SET_CHARGE_PUMP 0x8D
#define SSD1306_SET_SEG_REMAP 0xA0
#define SSD1306_SET_ENTIRE_ON 0xA4
#define SSD1306_SET_ALL_ON 0xA5
#define SSD1306_SET_NORM_DISP 0xA6
#define SSD1306_SET_INV_DISP 0xA7
#define SSD1306_SET_MUX_RATIO 0xA8
#define SSD1306_SET_DISP 0xAE
#define SSD1306_SET_COM_OUT_DIR 0xC0
#define SSD1306_SET_DISP_OFFSET 0xD3
#define SSD1306_SET_DISP_CLK_DIV 0xD5
#define SSD1306_SET_PRECHARGE 0xD9
#define SSD1306_SET_COM_PIN_CFG 0xDA
#define SSD1306_SET_VCOM_DESEL 0xDB

// 显示缓存参数
#define SSD1306_PAGE_HEIGHT 8
#define SSD1306_NUM_PAGES (SSD1306_HEIGHT / SSD1306_PAGE_HEIGHT)
#define SSD1306_BUF_LEN (SSD1306_NUM_PAGES * SSD1306_WIDTH)




#define ARRAY_LEN 15

#define SSD1306_WRITE_MODE         _u(0xFE)
#define SSD1306_READ_MODE          _u(0xFF)

// 渲染区域结构体（对应网页4/7的显示缓存管理）
struct render_area {
    uint8_t start_col;
    uint8_t end_col;
    uint8_t start_page;
    uint8_t end_page;
    int buflen;
};

#include "Fonts/fonts.h"

// 核心功能声明
void SSD1306_send_cmd(uint8_t cmd);
void SSD1306_send_cmd_list(uint8_t *buf, int num);
void SSD1306_send_buf(uint8_t buf[], int buflen);
void SSD1306_init();

// Expose global render area from ssd1306.cpp
extern struct render_area frame_area;
void SSD1306_scroll(bool on);
void SSD1306_invert(bool invert);
void render(uint8_t *buf, struct render_area *area);
void calc_render_area_buflen(struct render_area *area);

// 图形绘制API
void SetPixel(uint8_t *buf, int x,int y, bool on) ;
void DrawLine(uint8_t *buf, int x0, int y0, int x1, int y1, bool on);
void DrawRectangle(uint8_t *buf, int x0, int y0, int width, int height, bool solid, bool on);
void DrawEllipse(uint8_t *buf, int x0, int y0, int a, int b, bool solid, bool on);
void ClearScreen_Run(uint8_t *buf, struct render_area *area);
void ClearScreen(uint8_t *buf, struct render_area *area);

// Global display buffer declared in implementation
extern uint8_t buf[SSD1306_BUF_LEN];

// 旧版本 已弃用 文本显示功能
// void WriteString(uint8_t *buf, int16_t x, int16_t y, char *str, uint8_t XMultiples, uint8_t YMultiples);
// void WriteString_Mid(uint8_t *buf, int16_t x, int16_t y, char *str, uint8_t XMultiples, uint8_t YMultiples);

// 绘制/文本辅助函数
void Paint_DrawString_EN(uint8_t *buf, int16_t Xstart, int16_t Ystart, const char *pString, sFONT *Font, bool on);
void Paint_DrawString_CN(uint8_t *buf, int16_t Xstart, int16_t Ystart, const char *pString, cFONT *Font, bool on);
void Paint_DrawString_EN_CenterAtX(uint8_t *buf, int16_t centerX, int16_t y, const char *pString, sFONT *Font, bool on);
void Paint_DrawString_CN_CenterAtX(uint8_t *buf, int16_t centerX, int16_t y, const char *pString, cFONT *Font, bool on);
void InvertRect(uint8_t *buf, int16_t x0, int16_t y0, int16_t width, int16_t height);

// Extended drawing functions implemented in ssd1306.cpp
void Paint_DrawChar(uint8_t *buf, int16_t Xpoint, int16_t Ypoint, const char Acsii_Char, sFONT *Font, bool on);
void Paint_DrawString_Multiline(uint8_t *buf, int16_t Xstart, int16_t Ystart, const char *pString, sFONT *Font, bool on);
void Paint_DrawString_Multiline_CN(uint8_t *buf, int16_t Xstart, int16_t Ystart, const char *pString, cFONT *Font, bool on);
void Paint_DrawNum(uint8_t *buf, int16_t Xpoint, int16_t Ypoint, double Nummber, uint16_t Digit, sFONT *Font, bool on);
void Paint_DrawInt(uint8_t *buf, int16_t Xpoint, int16_t Ypoint, int Number, sFONT *Font, bool on);
uint16_t SSD1306_TextWidth(const char *text, sFONT *Font);
size_t SSD1306_TextBytesForWidth(const char *text, uint16_t maxWidth, sFONT *Font);

// print raw buffer
void SSD1306_PrintBufRaw();
bool SSD1306_IsStreamingActive(); 

#endif // _SSD1306_H
