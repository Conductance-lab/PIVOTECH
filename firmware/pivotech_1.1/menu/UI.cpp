
#include "menu/key.hpp"

#ifndef _UI_CPP_
#define _UI_CPP_

#include "CONFIG_FLO.hpp"
#include "menu/UI.hpp"
#include "menu/ssd1306.hpp"
#include "menu/send.hpp"
#include "menu/SGateSetting.hpp"
#include "menu/load.hpp"
#include "menu/StateLED.hpp"
#include "Fonts/fonts.h"
#include "menu/Curve.hpp"
#include "read/select.hpp"
#include "read/uartconfig.hpp"

#include <cstring>
#include <cstdlib>
#include <cmath>

#include "pico/stdio_usb.h"
#include "pico/stdio_uart.h"

extern char *backString;

// bool close_send = 0; // 用于软件禁用菜单的所有输出

bool menu_update_flag = 0; // 标记需要更新UI

EasyUIPage_t *pageHead = NULL, *pageTail = NULL;

uint8_t pageIndex[MAX_LAYER] = {0};
uint8_t itemIndex[MAX_LAYER] = {0};
uint8_t layer = 0;

bool AniBreak = 0; // 后续接入打断动画用(暂时没用)

extern bool opnEnter, opnExit, opnUp, opnDown, opnLeft, opnRight, opnCtrlUp, opnCtrlDown, opnCtrl;

bool function_to_menu = 0; // 记录是否从菜单页返回

uint16_t itemsYPosDefault = 16;       // 初始向下偏移
int16_t itemsYPos = itemsYPosDefault; // list中items的偏移
uint8_t safezone = 4;                 // 安全框设定

// 全局变量 存储位置状态
int16_t lengthStart, heightStart, yStart = itemsYPosDefault, scroll_yStart = SSD1306_HEIGHT + 1, itemsYPosStart = itemsYPos, xStart;
int16_t lastitemIndex = -1;
int16_t lastpageIndex = -1;

// 字体设置
sFONT *FontSelected = &Font12; // 8/12/16/20/24

// 行高设置
uint8_t ITEM_HEIGHT = (FontSelected->Height) * 1.2;
uint8_t FONT_HEIGHT = (FontSelected->Height);
uint8_t FONT_WIDTH = (FontSelected->Width);

// changevalue的width控制
uint16_t ChangeVal_Width = (FontSelected->Height) * 7;

// information的Y的状态值判定
int16_t information_pageheight = 0;
int16_t information_y = 0;

// 项目名称长度管理：

size_t Length_Item[ITEM_COUNT] = {
    15, // ITEM_PAGE_DESCRIPTION
    14, // ITEM_JUMP_PAGE
    11, // ITEM_SWITCH
    14, // ITEM_CHANGE_VALUE
    12, // ITEM_RADIO_BUTTON ITEM_CHECKBOX
    12, // ITEM_RADIO_BUTTON ITEM_CHECKBOX
    14, // ITEM_MESSAGE
    14, // ITEM_EVENT
    14, // ITEM_SEND
    14, // ITEM_INFOMATION
};

// 状态记录
bool functionIsRunning = false, listLoop = true;
bool infuction = false, outfuction = false, outsendfuction = false, if_send_flag = false;
uint8_t ifupdating_inchangevalue_Default = 10; // 每一帧减1 用于解释 发送提示符持续时间
uint8_t ifupdating_inchangevalue = ifupdating_inchangevalue_Default;

int seedvalue = 1314; // 随机值种子

/*!
 * @brief   Add item to page
 *
 * @param   page        EasyUI page struct
 * @param   item        EasyUI item struct
 * @param   _title      String of item title
 * @param   func        See EasyUIItem_e
 * @param   ...         ITEM_PAGE_DESCRIPTION: ignore this
 *                      ITEM_CALL_FUNCTION: fill with function
 *                      ITEM_JUMP_PAGE: fill with target page id
 *                      ITEM_CHECKBOX / ITEM_RADIO_BUTTON / ITEM_SWITCH: fill with bool value
 *                      ITEM_CHANGE_VALUE / ITEM_EVENT: fill with param that need to be changed and matched function
 *                      ITEM_MESSAGE: fill with message and matched function
 * @return  void
 *
 * @note    Do not modify
 *          ITEM_CHANGE_VALUE: the incoming param should always be paramType *,
 *          and cannot use casted variables(Don't know why)
 *          ITEM_EVENT: the incoming param should be 0 - 100
 *          If page type is PAGE_ICON, filled with icon array in the last variable
 */

/*
SGate 设定区域
*/

void UIUARTconfig()
{
    uartconfig();
}

void UISgateinformation()
{
    information_pageheight = 200;

    // 一个一个字符打出来，字符间距为14，X与前面字符间距为18，整体居中
    int total_width = 8 * 14;      // P-i-v-o-t 5个字符用14间距，X用18间距
    int start_x = 64 - total_width / 2 -2; // 居中起始位置

    Paint_DrawString_EN(buf, start_x, 2 - information_y, "P", &Font24, 1);
    Paint_DrawString_EN(buf, start_x + 14, 2 - information_y, "I", &Font24, 1);
    Paint_DrawString_EN(buf, start_x + 14 * 2, 2 - information_y, "V", &Font24, 1);
    Paint_DrawString_EN(buf, start_x + 14 * 3, 2 - information_y, "O", &Font24, 1);
    Paint_DrawString_EN(buf, start_x + 14 * 4, 2 - information_y, "T", &Font24, 1);
    Paint_DrawString_EN(buf, start_x + 14 * 5, 2 - information_y, "E", &Font24, 1);
    Paint_DrawString_EN(buf, start_x + 14 * 6, 2 - information_y, "C", &Font24, 1);
    Paint_DrawString_EN(buf, start_x + 14 * 7, 2 - information_y, "H", &Font24, 1);

    DrawRectangle(buf, 14, 23 - information_y, 100, 28, 1, 1);
    Paint_DrawString_EN_CenterAtX(buf, 64, 25 - information_y, "嵌入式通用快速", &Font12, 0);
    Paint_DrawString_EN_CenterAtX(buf, 64, 39 - information_y, "调试检测器", &Font12, 0);

    //int PIVOsw = 1;     // 软件主版本号
    // int PIVOsw_sub = 1; // 软件子版本号
    // int PIVOhw = 1;     // 硬件版本号
    extern int PIVOsw, PIVOsw_sub, PIVOhw;
    char text_info[256];
    snprintf(text_info, sizeof(text_info), "软件版本:%d.%d\r硬件版本:H.%d\rWeb APP 链接:\rconductance-lab.xyz/PIVOTECH\r扫描背面二维码获取使用帮助，更多内容关注B站@电导不是韩导，感谢您的使用和支持！", PIVOsw, PIVOsw_sub, PIVOhw);
    Paint_DrawString_Multiline(buf, 0, 55 - information_y, text_info, &Font12, 1);
}

void UIUpdateInformation()
{
    information_pageheight = 130;
    Paint_DrawString_Multiline(buf, 0, 2 - information_y, "如需升级到最新版，请访问:\rspace.bilibili.com/501542342\r获取最新的 SGate 系列资料与\rUF2 固件。", &Font12, 1);
}
void DrawQRCode(int start_x, int start_y);
void UIUpdateSponser()
{
    information_pageheight = 174;

    Paint_DrawString_Multiline(buf, 2, 5 - information_y, "如果你觉得 PivotX 很有用，感谢支持！\r我会继续维护并更新固件，这确实需要投入很多时间。\r~^v^~",
                               &Font12, 1);
    DrawRectangle(buf, 52, 107 - information_y, 70, 70, 1, 0);
    Paint_DrawString_EN(buf, 2, 127 - information_y, "微信", &Font12, 1);
    Paint_DrawString_EN(buf, 2, 139 - information_y, "扫码支持", &Font12, 1);
    DrawQRCode(54, 109 - information_y);
}

// void UILoadConfigUART(){
//     if(loadrequset(1)){
//         ClearScreen(buf, &frame_area);
//         DrawRectangle(buf,0,0,SSD1306_WIDTH+1,15,1,1);
//         Paint_DrawString_Multiline(buf, 8, 2,"Load Successful!",&Font12, 0);
//         Paint_DrawString_Multiline(buf, 0, 17,"Press the 'RESTART' button on the back to reboot the device.",&Font12, 1);
//         render(buf, &frame_area);
//         while (1)
//         {
//             sleep_ms(1000);//强制死机，等待重启
//         }

//     }
//     information_pageheight = SSD1306_HEIGHT;
//     Paint_DrawString_Multiline(buf, 22, 7,"Waiting for:",&Font12, 1);
//     Paint_DrawString_Multiline(buf, 5, 18,"UART config data.",&Font12, 1);
//     Paint_DrawString_Multiline(buf, 22, 40,"Please send!",&Font12, 1);
// }


bool UILoadConfigUSB()
{
    if (loadrequset(0))
    {
        ClearScreen(buf, &frame_area);
        DrawRectangle(buf, 0, 0, SSD1306_WIDTH + 1, 15, 1, 1);
        Paint_DrawString_Multiline(buf, 8, 2, "写入配置成功！", &Font12, 0);
        Paint_DrawString_Multiline(buf, 0, 17, "即将自动重启\n若未重启请手动重启", &Font12, 1);
        render(buf, &frame_area);
        sleep_ms(1000);
        // 重新载入menu 清空menu数据并重置状态
        functionIsRunning  = 0; 

        return 0;
        
    }
    information_pageheight = SSD1306_HEIGHT;
    Paint_DrawString_Multiline(buf, 22, 7, "处理配置中:", &Font12, 1);
    Paint_DrawString_Multiline(buf, 19, 30, "请稍候！", &Font12, 1);
    // Paint_DrawString_Multiline(buf, 22, 40, "请稍候！", &Font12, 1);
    return 1;
}

/*
UI 常规设定区域
*/

/*UI项目生成*/

void UIAddItem(EasyUIPage_t *page, EasyUIItem_t *item, char *_title, EasyUIItem_e func, ...)
{
    //打印状态
    // printf("Adding item to page (Page ID: %d, Item Title: %s, Function Type: %d)\r\n", page->id, _title, func); ///////////////////////////////////
    *item->flag = false;
    item->flagDefault = false;
    *item->param = 0;
    item->paramDefault = 0;
    item->paramBackup = 0;
    item->pageId = 0;
    item->Event = NULL;

    va_list variableArg;
    va_start(variableArg, func);
    item->title = _title;
    item->funcType = func;

    switch (item->funcType)
    {
    case ITEM_JUMP_PAGE:
        item->pageId = va_arg(variableArg, int);
        break;
    case ITEM_CHECKBOX:
    case ITEM_RADIO_BUTTON:
    case ITEM_SWITCH:
        item->flag = va_arg(variableArg, bool *);
        item->flagDefault = *item->flag;
        item->SendMode = va_arg(variableArg, int);
        break;
    case ITEM_EVENT:
        item->Event = va_arg(variableArg, void (*)(EasyUIItem_t *));
        break;
    case ITEM_CHANGE_VALUE: // 加入自定义三类选项
        item->param = va_arg(variableArg, paramType *);
        item->paramBackup = *item->param;
        item->paramDefault = *item->param;
        item->ValueType = va_arg(variableArg, int); // 数据的类型
        if ((item->ValueType == UINT_e || item->ValueType == UFLOAT_e) && *item->param < 0)
        { // 解决初始化负值错误自动为零
            *item->param = 0;
            item->paramBackup = *item->param;
            item->paramDefault = *item->param;
        }

        item->SendMode = va_arg(variableArg, int);
        break;
    case ITEM_INFOMATION:
        item->Event = va_arg(variableArg, void (*)(EasyUIItem_t *));
        break;
    // case ITEM_MESSAGE:
    //     item->msg = va_arg(variableArg, char *);
    //     item->Event = va_arg(variableArg, void (*)(EasyUIItem_t *));
    case ITEM_SEND:
        item->SendMode = va_arg(variableArg, int);
        break;
    default:
        break;
    }

    va_end(variableArg);

    item->next = NULL;

    if (page->itemHead == NULL)
    {
        item->id = 0;
        page->itemHead = item;
        page->itemTail = item;
    }
    else
    {
        item->id = page->itemTail->id + 1;
        // printf("%d", item->id);
        page->itemTail->next = item;
        page->itemTail = page->itemTail->next;
    }

    item->lineId = item->id;
    item->posForCal = 0;
    item->step = 1;
    item->position = 0;
}

/*!
 * @brief   Add page to UI
 *
 * @param   page    EasyUI page struct
 * @param   func    See EasyUIPage_e
 * @param   ...     PAGE_LIST: ignore this
 *                  PAGE_CUSTOM: fill with certain function
 * @return  void
 *
 * @note    Do not modify, the first page should always be the fist one to be added.
 */

void UIAddPage(EasyUIPage_t *page, EasyUIPage_e func, ...)
{
    page->Event = NULL;

    va_list variableArg;
    va_start(variableArg, func);
    page->itemHead = NULL;
    page->itemTail = NULL;
    page->next = NULL;

    page->funcType = func;
    if (page->funcType == PAGE_CUSTOM)
        page->Event = va_arg(variableArg, void (*)(EasyUIPage_t *));
    va_end(variableArg);

    if (pageHead == NULL)
    {
        page->id = 0;
        pageHead = page;
        pageTail = page;
    }
    else
    {
        page->id = pageTail->id + 1;
        pageTail->next = page;
        pageTail = pageTail->next;
    }

    // printf("Page added (Page ID: %d, Function Type: %d)\r\n", page->id, func); ///////////////////////////////////
}

// Sgate标注框使用
void UIDrawSgate(int16_t x, int16_t y, int16_t width, int16_t height, uint8_t Xlength, uint8_t Ylength, bool on, int ignorerunning)
{

    int16_t heightbackground = ITEM_HEIGHT * 3 + 2;
    int16_t widthbackground = ChangeVal_Width;
    int16_t xbackground = (SCREEN_WIDTH - width) / 2;
    int16_t ybackground = (SCREEN_HEIGHT - height) / 2;

    // menu运行状态且没有被主动忽略（如外部menu）
    // function to menu 如果满足 functionIsRunning本次动画也需要底色清除
    if ((functionIsRunning && (!ignorerunning)) || (function_to_menu && functionIsRunning) || ignorerunning == 2) // 2时强制黑底
    {
        // 底色清除
        //  DrawRectangle(buf, xbackground - 1-2,ybackground - 1-2, widthbackground+2+4, heightbackground+2+4, 0,1);
        DrawRectangle(buf, x - 2, y - 2, width + 4, height + 4, 1, 0);
    }
    // LH
    DrawLine(buf, x, y, x + Xlength, y, 1);
    DrawLine(buf, x, y, x, y + Ylength, 1);
    // RH
    DrawLine(buf, x + width, y, x + width - Xlength, y, 1);
    DrawLine(buf, x + width, y, x + width, y + Ylength, 1);
    // LD
    DrawLine(buf, x, y + height, x + Xlength, y + height, 1);
    DrawLine(buf, x, y + height, x, y + height - Ylength, 1);
    // RD
    DrawLine(buf, x + width, y + height, x + width - Xlength, y + height, 1);
    DrawLine(buf, x + width, y + height, x + width, y + height - Ylength, 1);
}

// checkbox绘制
void UIDrawCheckbox(int16_t x, int16_t y, uint16_t size, uint8_t offset, bool boolValue, uint8_t r)
{
    DrawRectangle(buf, x, y, size, size, 0, 1);
    // Paint_DrawRectangle(x,y,x+size,y+size-1,BLACK,DOT_PIXEL_1X1,DRAW_FILL_EMPTY);
    if (boolValue)
        DrawRectangle(buf, x + offset, y + offset, size - 2 * offset, size - 2 * offset, 1, 1);
    // Paint_DrawRectangle(x+offset,y+offset,x+size-offset,y+size-offset,BLACK,DOT_PIXEL_1X1,DRAW_FILL_FULL);
}

// 用于item文本绘制中实现超出范围的省略符
char *copyFirstNChars(const char *pString, size_t bytes_to_copy)
{
    if (pString == NULL)
    {
        return NULL;
    }

    size_t original_len = strlen(pString);
    size_t copy_len = (bytes_to_copy > original_len) ? original_len : bytes_to_copy;
    char *new_str = (char *)malloc(copy_len + 1);
    if (new_str == NULL)
    {
        return NULL;
    }

    strncpy(new_str, pString, copy_len);
    new_str[copy_len] = '\0';
    return new_str;
}

static void UIDrawTruncatedText(int16_t x, int16_t y, const char *pString, size_t keep_chars,
                                sFONT *font, bool invert)
{
    if (pString == NULL || font == NULL)
    {
        return;
    }

    const uint16_t max_width = (uint16_t)(keep_chars * font->Width);
    const uint16_t full_width = SSD1306_TextWidth(pString, font);
    if (full_width <= max_width)
    {
        Paint_DrawString_EN(buf, x, y, pString, font, !invert);
        return;
    }

    const uint16_t ellipsis_width = SSD1306_TextWidth("...", font);
    const uint16_t text_limit = max_width > ellipsis_width ? (uint16_t)(max_width - ellipsis_width) : 0;
    const size_t fit_bytes = SSD1306_TextBytesForWidth(pString, text_limit, font);
    char *truncated = copyFirstNChars(pString, fit_bytes);
    if (truncated == NULL)
    {
        return;
    }

    Paint_DrawString_EN(buf, x, y, truncated, font, !invert);
    Paint_DrawString_EN(buf, x + SSD1306_TextWidth(truncated, font), y, "...", font, !invert);
    free(truncated);
}

void UIDisplayStr_font12(int16_t x, int16_t y, char *pString, size_t keep_chars, bool Invert)
{
    UIDrawTruncatedText(x, y, pString, keep_chars, &Font12, Invert);
}

void UIDisplayStr(int16_t x, int16_t y, char *pString, size_t keep_chars = 64, bool Invert = 0)
{
    UIDrawTruncatedText(x, y, pString, keep_chars, FontSelected, Invert);
}

static int32_t UIGetItemTextWidth(const char *title, size_t keep_chars, bool has_prefix, int32_t padding)
{
    if (title == NULL || FontSelected == NULL)
    {
        return padding;
    }

    const int32_t limit_width = (int32_t)(keep_chars * FontSelected->Width);
    int32_t actual_width = (int32_t)SSD1306_TextWidth(title, FontSelected);
    const bool truncated = actual_width > limit_width;
    if (truncated)
    {
        actual_width = limit_width;
    }

    int32_t width = actual_width + padding;
    if (has_prefix)
    {
        width += FontSelected->Width;
    }
    if (truncated)
    {
        width += FontSelected->Width;
    }
    return width;
}

void UIDisplayItem(EasyUIItem_t *item)
{
    switch (item->funcType)
    {
    case ITEM_JUMP_PAGE:

        UIDisplayStr(safezone, item->position, "+");
        UIDisplayStr(safezone + 3 + FONT_WIDTH, item->position, item->title, Length_Item[ITEM_JUMP_PAGE]);
        break;
    case ITEM_PAGE_DESCRIPTION:
        UIDisplayStr(safezone, item->position, item->title, Length_Item[ITEM_PAGE_DESCRIPTION]);
        break;
    case ITEM_CHECKBOX:
    case ITEM_RADIO_BUTTON:
        UIDisplayStr(safezone, item->position, "-");
        UIDisplayStr(safezone + 3 + FONT_WIDTH, item->position, item->title, Length_Item[ITEM_RADIO_BUTTON]);
        UIDrawCheckbox(SCREEN_WIDTH - safezone - SCROLL_BAR_WIDTH - ITEM_HEIGHT * 0.7, // checkbox位置定位
                       item->position + ITEM_HEIGHT * 0.1, FONT_HEIGHT * 0.8, CHECK_BOX_OFFSET,
                       *item->flag, 1);

        break;
    case ITEM_SWITCH:
        UIDisplayStr(safezone, item->position, "-");
        UIDisplayStr(safezone + 3 + FONT_WIDTH, item->position, item->title, Length_Item[ITEM_SWITCH]);
        if (*item->flag)
            UIDisplayStr(SCREEN_WIDTH - safezone - SSD1306_TextWidth("开", FontSelected) - SCROLL_BAR_WIDTH, item->position, "开");
        else
            UIDisplayStr(SCREEN_WIDTH - safezone - SSD1306_TextWidth("关", FontSelected) - SCROLL_BAR_WIDTH, item->position, "关");
        break;
    // case ITEM_EVENT:
    case ITEM_CHANGE_VALUE:
        UIDisplayStr(safezone, item->position, "-");

        if (item->ValueType == FLOAT_e || item->ValueType == UFLOAT_e)
        {                                                  // float显示
            int temparam = static_cast<int>(*item->param); // 转换为 int 类型
            int digitCount = (temparam == 0) ? 1 : static_cast<int>(log10(static_cast<int>(abs(*item->param)))) + 1;
            if (*item->param < 0)
            {
                digitCount++; // 负号的位置
            }
            UIDisplayStr(safezone + 3 + FONT_WIDTH, item->position, item->title, Length_Item[ITEM_CHANGE_VALUE] - 5 - digitCount - 1);
            Paint_DrawNum(buf, SCREEN_WIDTH - safezone - 2 - (digitCount + 5) * FONT_WIDTH - SCROLL_BAR_WIDTH, item->position, // 数字特殊渲染
                          *item->param, 4, FontSelected, 1);
        }
        else if (item->ValueType == UINT_e || item->ValueType == INT_e)
        { // uint 和 int

            int temparam = static_cast<int>(*item->param); // 转换为 int 类型
            int digitCount = (temparam == 0) ? 1 : static_cast<int>(log10(abs(*item->param))) + 1;
            if (*item->param < 0)
            {
                digitCount++; // 负号的位置
            }
            UIDisplayStr(safezone + 3 + FONT_WIDTH, item->position, item->title, Length_Item[ITEM_CHANGE_VALUE] - digitCount - 1);
            Paint_DrawInt(buf, SCREEN_WIDTH - safezone - 2 - digitCount * FONT_WIDTH - SCROLL_BAR_WIDTH, item->position, // 数字特殊渲染
                          *item->param, FontSelected, 1);
        }
        break;
    case ITEM_SEND:
        UIDisplayStr(safezone, item->position, ">");
        UIDisplayStr(safezone + 3 + FONT_WIDTH, item->position, item->title, Length_Item[ITEM_SEND]);
        break;
    case ITEM_INFOMATION:
    case ITEM_EVENT:
        UIDisplayStr(safezone, item->position, "#");
        UIDisplayStr(safezone + 3 + FONT_WIDTH, item->position, item->title, Length_Item[ITEM_EVENT]);
        break;
    default:
        UIDisplayStr(safezone, item->position, "?"); // item定义未找到
        UIDisplayStr(safezone + 3 + FONT_WIDTH, item->position, item->title);
        break;
    }
}

// 模糊动画
void ApplyBlurEffect(uint8_t *buf, float intensity)
{
    if (footlengthDefault == 1)
    {
        return;
    }
    srand(seedvalue);

    if (intensity >= 0.9)
    {
        for (int n = 0; n < 192; n += 2)
        {
            // 计算线条起点和终点
            int x0, y0, x1, y1;

            // 从左侧开始
            x0 = 0;
            y0 = n - 64;

            // 根据斜率计算终点
            x1 = 127;
            y1 = y0 + 64;

            DrawLine(buf, x0, y0, x1, y1, 1);
        }
    }

    int max_lines = (128 + 64) * 4; // 最大线条数
    int num_lines = static_cast<int>(max_lines * intensity);
    // 绘制线条
    for (int n = 0; n < num_lines; n++)
    {
        // 计算线条起点和终点
        int x0, y0, x1, y1;

        // 从左侧开始
        x0 = 0;
        y0 = 2 * (rand() % (128 + 64)) - 128;

        // 根据斜率计算终点
        x1 = 127;
        y1 = y0 + 64;

        DrawLine(buf, x0, y0, x1, y1, 1);
    }
}

// 提前声明
void DrawSendText(EasyUIItem_t *item, EasyUIPage_t *page);
void UIEventChangeValue(EasyUIItem_t *item, EasyUIPage_t *page, bool float1_int0, bool ifuint, bool ifonlyani);

void UIEventChangeFloat(EasyUIItem_t *item, EasyUIPage_t *page, bool ifuint, bool ifonlyani = 0)
{
    UIEventChangeValue(item, page, 1, ifuint, ifonlyani);
};
void UIEventChangeInt(EasyUIItem_t *item, EasyUIPage_t *page, bool ifuint, bool ifonlyani = 0)
{
    UIEventChangeValue(item, page, 0, ifuint, ifonlyani);
};

void UIListAni(EasyUIPage_t *page, uint8_t index) // 列表页面的渲染 包含每一个项目的渲染 动画部分
{
    float length, height, y, scroll_y, x;

    int32_t lengthTarget, heightTarget, yTarget, scroll_yTarget, xTarget;
    int16_t itemsYPosAni = 0, Yzoom = 0;

    if (page->funcType != PAGE_LIST)
        return;

    // Get target length and y
    for (EasyUIItem_t *itemTmp = page->itemHead; itemTmp != NULL; itemTmp = itemTmp->next)
    {
        if (index == itemTmp->id)
        {

            if (itemTmp->funcType == ITEM_PAGE_DESCRIPTION)
            { // DESCRIPTION 特殊设定 因为没有前面的+-项目符号
                lengthTarget = UIGetItemTextWidth(itemTmp->title, Length_Item[itemTmp->funcType], false, 5);
                yTarget = (itemTmp->lineId * ITEM_HEIGHT) + safezone + itemsYPos-1; // 在当前偏移下的实际位置
                xTarget = safezone - 3 + Yzoom;
                heightTarget = FONT_HEIGHT + 2 * Yzoom+1;
            }
            else if (itemTmp->funcType == ITEM_INFOMATION)
            {
                if (infuction || functionIsRunning)
                {
                    heightTarget = SSD1306_HEIGHT - 1;
                    lengthTarget = SSD1306_WIDTH - 1;
                    xTarget = 0;
                    yTarget = 0;
                }
                else
                {
                    lengthTarget = UIGetItemTextWidth(itemTmp->title, Length_Item[itemTmp->funcType], true, 8);
                    yTarget = (itemTmp->lineId * ITEM_HEIGHT) + safezone + itemsYPos-1; // 在当前偏移下的实际位置
                    xTarget = safezone - 3 + Yzoom;
                    heightTarget = FONT_HEIGHT + 2 * Yzoom+1;
                }
            }
            else if (itemTmp->funcType == ITEM_CHANGE_VALUE && !outsendfuction) // 若以send退出 则不按照changevalue逻辑退出
            {
                if (infuction || functionIsRunning)
                {

                    heightTarget = ITEM_HEIGHT * 3;
                    lengthTarget = ChangeVal_Width;
                    xTarget = (SCREEN_WIDTH - lengthTarget) / 2;
                    yTarget = (SCREEN_HEIGHT - heightTarget) / 2 + 1;
                }
                else
                {
                    lengthTarget = SSD1306_WIDTH - 2 * safezone;
                    yTarget = (itemTmp->lineId * ITEM_HEIGHT) + safezone + itemsYPos-1; // 在当前偏移下的实际位置
                    xTarget = safezone - 3 + Yzoom;
                    heightTarget = FONT_HEIGHT + 2 * Yzoom+1;
                }
            }

            else if ((itemTmp->funcType == ITEM_SEND) || outsendfuction || if_send_flag) // 以send退出时 渲染send
            {
                if (infuction)
                {

                    heightTarget = ITEM_HEIGHT * 2;
                    lengthTarget = ChangeVal_Width;
                    xTarget = (SCREEN_WIDTH - lengthTarget) / 2;
                    yTarget = (SCREEN_HEIGHT - heightTarget) / 2 + 1;
                }
                else
                {
                    lengthTarget = UIGetItemTextWidth(itemTmp->title, Length_Item[itemTmp->funcType], true, 8);
                    yTarget = (itemTmp->lineId * ITEM_HEIGHT) + safezone + itemsYPos-1; // 在当前偏移下的实际位置
                    xTarget = safezone - 3 + Yzoom;
                    heightTarget = FONT_HEIGHT + 2 * Yzoom+1;
                }
            }

            else
            {
                lengthTarget = UIGetItemTextWidth(itemTmp->title, Length_Item[itemTmp->funcType], true, 8);
                yTarget = (itemTmp->lineId * ITEM_HEIGHT) + safezone + itemsYPos-1; // 在当前偏移下的实际位置
                xTarget = safezone - 3 + Yzoom;
                heightTarget = FONT_HEIGHT + 2 * Yzoom+1;
            }

            // information下标条在UIinformation里绘制

            if ((itemTmp->funcType == ITEM_INFOMATION) && infuction)
            {
                scroll_yTarget = SSD1306_HEIGHT + 1;
            }
            else
            {
                scroll_yTarget = (itemTmp->lineId * SCREEN_HEIGHT) / ((int)(page->itemTail->id) + 1); // 避免因精度导致条底部露白
            }

            // 换页的重新偏移
            if (page->id != lastpageIndex)
            {
                itemsYPos += -(yTarget - (SCREEN_HEIGHT - safezone - ITEM_HEIGHT) / 2);
                yTarget = (SCREEN_HEIGHT - safezone - ITEM_HEIGHT) / 2;
                itemsYPosStart = itemsYPos - 30;
            }
            // 若不换页 超出行数自动偏移
            else if (!infuction)
            {
                if (yTarget + FONT_HEIGHT > SCREEN_HEIGHT - safezone)
                {
                    itemsYPos += -(yTarget + FONT_HEIGHT - (SCREEN_HEIGHT - safezone));
                    yTarget = SCREEN_HEIGHT - safezone - FONT_HEIGHT;
                }

                else if (yTarget < safezone)
                {
                    itemsYPos += safezone - yTarget;
                    yTarget = safezone;
                }
            }

            break;
        }
    }

    if (infuction)
    {
        // 在infuction时 设置随机种子
        seedvalue = to_ms_since_boot(get_absolute_time());
    }

    // 若没有发生变动则不渲染动画
    if (((footlength != 1) && (index != lastitemIndex || page->id != lastpageIndex || infuction || outfuction || outsendfuction)) || function_to_menu)
    {

        // 动画渲染
        for (float i = 0; i < 1; i += footlength)
        {
            length = easeInOutQuad(lengthStart, lengthTarget, i);
            if (functionIsRunning && outsendfuction && infuction)
            {
                height = X2line_down(heightStart, heightTarget, i); // 针对于 outsend 跳出模式时 进入sendUI的上下边框不同步问题 height和Y都改为X2Line_down 函数
            }
            else
            {
                height = easeInOutQuad(heightStart, heightTarget, i);
            }

            y = X2line_down(yStart, yTarget, i);
            x = X2line_down(xStart, xTarget, i);
            scroll_y = easeInOutQuad(scroll_yStart, scroll_yTarget, i);
            itemsYPosAni = easeInOutQuad(itemsYPosStart, itemsYPos, i);

            if (page->id != lastpageIndex && lastpageIndex != -1)
            { // 更换页面的动画
                Yzoom = A_line(0, 0, 10, i);
            }
            else
            {
                Yzoom = 0;
            }

            // draw items
            int16_t itemsYPos_cout = itemsYPosAni + safezone;
            for (EasyUIItem_t *item = page->itemHead; item != NULL; item = item->next)
            {
                item->position = itemsYPos_cout;
                itemsYPos_cout += ITEM_HEIGHT;
                UIDisplayItem(item);
            }

            if (infuction && outsendfuction) // 以send结束退出 infuction阶段背景不变
            {
                ApplyBlurEffect(buf, 1); // 背景模糊层
            }
            else if (infuction || (functionIsRunning && function_to_menu))
            {
                ApplyBlurEffect(buf, X2line_up(0, 1, i)); // 背景模糊层
            }
            else if (outfuction)
            {
                ApplyBlurEffect(buf, X2line_down(0.8, 0, i)); // 背景模糊层
            }

            // draw gate
            UIDrawSgate(x + Yzoom, y - 1 - Yzoom, length - Yzoom * 2, height + Yzoom * 2, 8, FONT_HEIGHT * 0.3, 1);
            DrawRectangle(buf, SCREEN_WIDTH - SCROLL_BAR_WIDTH, scroll_y, SCROLL_BAR_WIDTH, SCREEN_HEIGHT / (page->itemTail->id + 1) + 1, 1, 1);

            if (infuction || (function_to_menu && functionIsRunning)) // 在进入动画时 提前打字 增加流畅感
            {

                for (EasyUIItem_t *item = page->itemHead; item != NULL; item = item->next)
                {
                    if (item->id != index)
                    {
                        continue;
                    }

                    switch (item->funcType)
                    {
                    case ITEM_CHECKBOX:
                    case ITEM_RADIO_BUTTON:
                    case ITEM_SWITCH:
                        if (if_send_flag)
                        {
                            DrawSendText(item, page);
                        }
                        break;
                    case ITEM_CHANGE_VALUE:
                        if (outsendfuction)
                        {
                            DrawSendText(item, page);
                        }
                        else
                        {
                            switch (item->ValueType)
                            {
                            case INT_e:
                                UIEventChangeInt(item, page, false, true); // 仅动画 传入控制值
                                break;
                            case UINT_e:
                                UIEventChangeInt(item, page, true, true);
                                break;
                            case FLOAT_e:
                                UIEventChangeFloat(item, page, false, true);
                                break;
                            case UFLOAT_e:
                                UIEventChangeFloat(item, page, true, true);
                                break;

                            default:
                                break;
                            }

                            break;
                        }
                    case ITEM_EVENT:
                        // EasyUIDrawProgressBar(item);
                        // item->Event(item);
                        break;
                    case ITEM_SEND:
                        DrawSendText(item, page); // 仅动画
                        break;
                    case ITEM_INFOMATION:
                        item->Event(item); // 仅动画
                        break;
                    default:
                        // item->Event(item);
                        break;
                    }
                    break;
                }
            }

            render(buf, &frame_area);
            memset(buf, 0, SSD1306_BUF_LEN);
        }
        // here
    }

    infuction = 0; // 渲染动画结束
    outfuction = 0;
    outsendfuction = 0;
    function_to_menu = 0;

    // 保存为上一次状态
    lengthStart = lengthTarget, heightStart = heightTarget, yStart = yTarget, scroll_yStart = scroll_yTarget, itemsYPosStart = itemsYPos, xStart = xTarget;

    // 最后一步的动画
    length = lengthStart;
    height = heightStart;
    y = yStart;
    x = xStart;
    scroll_y = scroll_yStart;
    itemsYPosAni = itemsYPos;

    // 静态绘制

    // draw items
    int16_t itemsYPos_cout = itemsYPosAni + safezone;
    for (EasyUIItem_t *item = page->itemHead; item != NULL; item = item->next)
    {
        item->position = itemsYPos_cout;
        itemsYPos_cout += ITEM_HEIGHT;
        UIDisplayItem(item);
    }

    for (EasyUIItem_t *itemTmp = page->itemHead; itemTmp != NULL; itemTmp = itemTmp->next)
    {
        if (index == itemTmp->id)
        {
            if (functionIsRunning && (itemTmp->SendMode != SendtoSettings))
            { // 静止模糊层
                ApplyBlurEffect(buf, 1);
            }
        }
    }

    // draw gate
    //  UIDrawSgate(x,y -1,length, height,8,FONT_HEIGHT*0.3,1);
    UIDrawSgate(x, y - 1 - Yzoom, length - Yzoom * 2, height, 8, FONT_HEIGHT * 0.3, 1);
    // drwa 条
    DrawRectangle(buf, SCREEN_WIDTH - SCROLL_BAR_WIDTH, scroll_y, SCROLL_BAR_WIDTH, SCREEN_HEIGHT / (page->itemTail->id + 1) + 1, 1, 1);

    lastitemIndex = index;
    lastpageIndex = page->id;
    // 返回之后在UI_Update最终渲染
    return;
}

void ResetOpn()
{
    opnEnter = opnExit = opnUp = opnDown = opnCtrlDown = opnCtrlUp = opnRight = opnLeft = false;
}

void DrawSendText(EasyUIItem_t *item, EasyUIPage_t *page)
{
    static int16_t x, y;
    static uint16_t width, height;
    height = ITEM_HEIGHT * 2;
    width = ChangeVal_Width;
    x = (SCREEN_WIDTH - width) / 2;
    y = (SCREEN_HEIGHT - height) / 2 + 1;
    if (!saveitem_insendmode)
    {
        UIDisplayStr(x + 3, y, "发送 ->");
        UIDisplayStr(x + 3, y + ITEM_HEIGHT, backString, ChangeVal_Width / FONT_WIDTH - 2);
    }
    else
    {
        UIDisplayStr(x + 3 + FONT_WIDTH, y + ITEM_HEIGHT / 2, "已保存", ChangeVal_Width / FONT_WIDTH - 2 - 1);
    }
}

void UIinformation(EasyUIItem_t *item, EasyUIPage_t *page, uint8_t index)
{
    if (function_to_menu)
    { // 切回内容的过渡动画
        UIListAni(page, index);
    }

    if (opnEnter || opnExit)
    {
        functionIsRunning = false;
        outfuction = 1;

        ResetOpn();
        
        return;
    }
    
    else if (opnDown)
    {
        information_y += 2;
        if (information_y > information_pageheight - SSD1306_HEIGHT)
        {
            information_y = information_pageheight - SSD1306_HEIGHT;
        }
        ResetOpn();
    }
    
    else if(keyDown.isLongPress)
    {
        information_y += 1;
        if (information_y > information_pageheight - SSD1306_HEIGHT)
        {
            information_y = information_pageheight - SSD1306_HEIGHT;
        }
    }

    else if (opnUp)
    {
        information_y -= 2;
        if (information_y < 0)
        {
            information_y = 0;
        }
        ResetOpn();
    }
    else if(keyUp.isLongPress)
    {
        information_y -= 1;
        if (information_y < 0)
        {
            information_y = 0;
        }
    }

    UIDrawSgate(0, 0, SSD1306_WIDTH - 1, SSD1306_HEIGHT - 1, 8, FONT_HEIGHT * 0.3, 1);
    item->Event(item);
};
void UISend(EasyUIItem_t *item, EasyUIPage_t *page)
{

    uint8_t temlayer = layer + 1;

    SendMessage(item, page);

    infuction = 1;
    // 进入动画/跳转至send的进入动画
    if (outsendfuction)
    {
        footlength = footlengthDefault * 3;
    }
    UIListAni(page, itemIndex[temlayer]);
    sleep_ms(100);
    footlength = footlengthDefault;
    functionIsRunning = 0;
    if_send_flag = 0;
    outfuction = 1;

    saveitem_insendmode = 0;
    // 结束Update时 还会有一次动画 进行退出刷新
    ResetOpn();
}

// 在ani前已经声明 ifnolyani默认为0
void UIEventChangeValue(EasyUIItem_t *item, EasyUIPage_t *page, bool float1_int0, bool ifuint, bool ifonlyani)
{
    static int16_t x, y;
    static uint16_t width, height;

    double step = item->step;
    static uint8_t itemHeightOffset = (ITEM_HEIGHT - FONT_HEIGHT) / 2 + 1;
    uint8_t temlayer = layer + 1;

    if (!ifonlyani)
    {
        // Operation move reaction
        if (opnEnter)
        {
            item->paramBackup = *item->param;
            if (item->SendMode == Send_Value ||
                item->SendMode == Send_Title_Value ||
                item->SendMode == Send_Title)
            {
                outsendfuction = 1; // 标注 outsendfuction
                UISend(item, page); // 发送入口 里面会标注infuction and outfuction 第二部会取消标注 isrunningfuction 另外UIsend里有发送过程
                outsendfuction = 0;
            }
            else
            {
                functionIsRunning = false;
                outfuction = 1;
            }

            item->step = step;
            ResetOpn();
            // 结束Update时 还会有一次动画 进行退出刷新
            return;
        }
        if (opnExit)
        {
            *item->param = item->paramBackup;
            functionIsRunning = false;
            outfuction = 1;
            item->step = step;
            ResetOpn();
            // 结束Update时 还会有一次动画 进行退出刷新
            return;
        }

        // 根据数据类型进行分类
        if (float1_int0)
        {
            if (opnCtrlUp)
            {
                if (step == 0.0001)
                    step = 0.001;
                else if (step == 0.001)
                    step = 0.01;
                else if (step == 0.01)
                    step = 0.1;
                else if (step == 0.1)
                    step = 1;
                else if (step == 1)
                    step = 10;
                else if (step == 10)
                    step = 0.0001;
                else
                    step = 0.01; // 默认情况

                item->step = step;
            }

            if (opnCtrlDown)
            {
                if (step == 10)
                    step = 1;
                else if (step == 1)
                    step = 0.1;
                else if (step == 0.1)
                    step = 0.01;
                else if (step == 0.01)
                    step = 0.001;
                else if (step == 0.001)
                    step = 0.0001;
                else if (step == 0.0001)
                    step = 10;
                else
                    step = 0.01;

                item->step = step;
            }
        }
        else if (!float1_int0)
        {
            if (opnCtrlUp)
            {
                if (step == 1)
                    step = 10;
                else if (step == 10)
                    step = 100;
                else if (step == 100)
                    step = 1000;
                else if (step == 1000)
                    step = 10000;
                else if (step == 10000)
                    step = 1;
                else
                    step = 1; // 默认情况

                item->step = step;
            }
            if (opnCtrlDown)
            {
                if (step == 10000)
                    step = 1000;
                else if (step == 1000)
                    step = 100;
                else if (step == 100)
                    step = 10;
                else if (step == 10)
                    step = 1;
                else if (step == 1)
                    step = 10000;
                else
                    step = 1;

                item->step = step;
            }
        }

        if (opnUp)
        {
            *item->param += step;
            ifupdating_inchangevalue = ifupdating_inchangevalue_Default;
        }

        if (opnDown)
        {
            *item->param -= step;
            ifupdating_inchangevalue = ifupdating_inchangevalue_Default;

            if (ifuint)
            {
                if ((*item->param) < 0)
                {
                    *item->param = 0;
                }
            }
        }

        /************按键处理结束******** */

        // realtime发送模式 且刚刚完成ifupdating_inchangevalue的更新 进入发送入口
        if ((ifupdating_inchangevalue == ifupdating_inchangevalue_Default) && (item->SendMode == RealtimeSend_Title_Value || item->SendMode == RealtimeSend_Value))
        {
            // realtime发送入口
            SendMessage(item, page);
        }

        ResetOpn();
        // 根据key的判定重新渲染画面：
        UIListAni(page, itemIndex[temlayer]);
    }

    // 结束if:!ifonlyani 以下只渲染动画 且不经过UIListAni

    height = ITEM_HEIGHT * 3;
    width = ChangeVal_Width;
    x = (SCREEN_WIDTH - width) / 2;
    y = (SCREEN_HEIGHT - height) / 2;

    int temparam = static_cast<int>(*item->param);
    int digitCount;

    if (float1_int0)
    {
        digitCount = (abs(temparam) < 1) ? 6 : static_cast<int>(log10(abs(static_cast<int>(*item->param)))) + 6;
    }
    else if (!float1_int0)
    {
        digitCount = (temparam == 0) ? 1 : static_cast<int>(log10(abs(*item->param))) + 1;
    }

    if (*item->param < 0)
    {
        digitCount++; // 负号的位置
    }

    if (!opnCtrl)
        DrawRectangle(buf, x + 1, y + ITEM_HEIGHT, digitCount * FONT_WIDTH + 5, ITEM_HEIGHT, 1, !opnCtrl);
    if (opnCtrl)
        if (float1_int0)
        {
            DrawRectangle(buf, x + 1, y + 2 * ITEM_HEIGHT, ((static_cast<int>(step) >= 1) ? (log10(abs(static_cast<int>(step))) + 8) : (-log10(abs(step)) + 9)) * FONT_WIDTH + 2 - 4 * FONT_WIDTH, ITEM_HEIGHT, 1, opnCtrl);
        }
        else if (!float1_int0)
        {
            DrawRectangle(buf, x + 1, y + 2 * ITEM_HEIGHT, (log10(abs(static_cast<int>(step))) + 4) * FONT_WIDTH + 5, ITEM_HEIGHT, 1, opnCtrl);
        }

    // 根据是否realtimesend 进行闪烁发送提示符提醒
    if (ifupdating_inchangevalue && (item->SendMode == RealtimeSend_Value || item->SendMode == RealtimeSend_Title_Value))
    {
        UIDisplayStr(x + 3, y + itemHeightOffset, item->title, ChangeVal_Width / FONT_WIDTH - 2 - 3);
        UIDisplayStr(x + 3 + ChangeVal_Width - 3 * FONT_WIDTH, y + itemHeightOffset, "->");
        ifupdating_inchangevalue--; // 减到0为止
    }
    else
    {
        UIDisplayStr(x + 3, y + itemHeightOffset, item->title, ChangeVal_Width / FONT_WIDTH - 2);
    }

    if (float1_int0)
    {
        Paint_DrawNum(buf, x + 3, y + ITEM_HEIGHT + itemHeightOffset, *item->param, 4, FontSelected, opnCtrl); // 数字渲染
    }
    else if (!float1_int0)
    {
        Paint_DrawInt(buf, x + 3, y + ITEM_HEIGHT + itemHeightOffset, *item->param, FontSelected, opnCtrl); // 数字渲染
    }

    // Display step
    if (step == 0.0001)
        UIDisplayStr(x + 3 + FONT_WIDTH, y + 2 * ITEM_HEIGHT + itemHeightOffset, "+0.0001", 10, opnCtrl);
    else if (step == 0.001)
        UIDisplayStr(x + 3 + FONT_WIDTH, y + 2 * ITEM_HEIGHT + itemHeightOffset, "+0.001", 10, opnCtrl);
    else if (step == 0.01)
        UIDisplayStr(x + 3 + FONT_WIDTH, y + 2 * ITEM_HEIGHT + itemHeightOffset, "+0.01", 10, opnCtrl);
    else if (step == 0.1)
        UIDisplayStr(x + 3 + FONT_WIDTH, y + 2 * ITEM_HEIGHT + itemHeightOffset, "+0.1", 10, opnCtrl);
    else if (step == 1)
        UIDisplayStr(x + 3 + FONT_WIDTH, y + 2 * ITEM_HEIGHT + itemHeightOffset, "+1", 10, opnCtrl);
    else if (step == 10)
        UIDisplayStr(x + 3 + FONT_WIDTH, y + 2 * ITEM_HEIGHT + itemHeightOffset, "+10", 10, opnCtrl);
    else if (step == 100)
        UIDisplayStr(x + 3 + FONT_WIDTH, y + 2 * ITEM_HEIGHT + itemHeightOffset, "+100", 10, opnCtrl);
    else if (step == 1000)
        UIDisplayStr(x + 3 + FONT_WIDTH, y + 2 * ITEM_HEIGHT + itemHeightOffset, "+1000", 10, opnCtrl);
    else if (step == 10000)
        UIDisplayStr(x + 3 + FONT_WIDTH, y + 2 * ITEM_HEIGHT + itemHeightOffset, "+10000", 10, opnCtrl);

    UIDisplayStr(x + 3, y + 2 * ITEM_HEIGHT + itemHeightOffset, "(", 10, opnCtrl);
    if (float1_int0)
    {
        UIDisplayStr(x + 3 + ((static_cast<int>(step) >= 1) ? (log10(abs(static_cast<int>(step))) + 8) : (-log10(abs(step)) + 9)) * FONT_WIDTH + 2 - 5 * FONT_WIDTH, y + 2 * ITEM_HEIGHT + itemHeightOffset, ")", 10, opnCtrl);
    }
    else if (!float1_int0)
    {
        UIDisplayStr(x + 3 + (log10(abs(static_cast<int>(step))) + 3) * FONT_WIDTH, y + 2 * ITEM_HEIGHT + itemHeightOffset, ")", 10, opnCtrl);
    }

    return;
}

/**************************** UI模块核心交互内容 ***********************/

void UIItemOperationResponse(EasyUIPage_t *page, EasyUIItem_t *item, uint8_t *index)
{
    uint8_t temlayer = layer + 1;
    switch (item->funcType)
    {
    case ITEM_JUMP_PAGE:
        if (layer == MAX_LAYER - 1)
        {
            break;
            Debug("UI:layer of page is MAX");
        }

        itemIndex[layer++] = *index;
        pageIndex[layer] = item->pageId;
        *index = 0;
        itemsYPos = itemsYPosDefault;
        break;
    case ITEM_CHECKBOX:
    case ITEM_SWITCH:
        *item->flag = !*item->flag;

        if (item->SendMode != NoSend)
        { // NOsend以外其他情况有显示UI
            functionIsRunning = true;
            itemIndex[temlayer] = *index;
            if_send_flag = 1;
        }
        break;
    case ITEM_RADIO_BUTTON:
        for (EasyUIItem_t *itemTmp = page->itemHead; itemTmp != NULL; itemTmp = itemTmp->next)
        {
            if (itemTmp->funcType == ITEM_RADIO_BUTTON && itemTmp->id != item->id)
                *itemTmp->flag = false;
        }
        *item->flag = !*item->flag;

        if (item->SendMode != NoSend)
        { // NOsend以外其他情况有显示UI
            functionIsRunning = true;
            itemIndex[temlayer] = *index;
            if_send_flag = 1;
        }
        break;
    case ITEM_EVENT:
        functionIsRunning = true;
        break;
    case ITEM_CHANGE_VALUE:
        infuction = 1;
        functionIsRunning = true;
        itemIndex[temlayer] = *index;
        break;
    case ITEM_MESSAGE:
        functionIsRunning = true;
        // EasyUIDrawMsgBox(item->msg);
        break;
    case ITEM_SEND:

        functionIsRunning = true;
        itemIndex[temlayer] = *index;

        break;
    case ITEM_INFOMATION:
        infuction = 1; // 类似changevalue 只能在进入第一次刷新
        information_y = 0;
        functionIsRunning = true;
        itemIndex[temlayer] = *index;
        break;
    default:
        break;
    }
}

void UI_START()
{
    int16_t x, y, width, height, Gate_Ylength, Gate_Xlength;
    int16_t x_START, y_START, width_START, height_START, Gate_Ylength_START, Gate_Xlength_START;
    int16_t x_TARGET, y_TARGET, width_TARGET, height_TARGET, Gate_Ylength_TARGET, Gate_Xlength_TARGET;

    x_START = 0;
    y_START = 0;
    width_START = SSD1306_WIDTH - 1;
    height_START = SSD1306_HEIGHT - 1;

    width_TARGET = 40;
    height_TARGET = 40;
    x_TARGET = (SSD1306_WIDTH - width_TARGET) / 2;
    y_TARGET = (SSD1306_HEIGHT - height_TARGET) / 2;
    Gate_Ylength_START = 10;
    Gate_Xlength_START = 10;
    Gate_Xlength_TARGET = 5;
    Gate_Ylength_TARGET = 5;

    for (float i = 0; i < 1; i = i + 0.03)
    {
        x = X2line_down(x_START, x_TARGET, i);
        y = X2line_down(y_START, y_TARGET, i);
        width = X2line_down(width_START, width_TARGET, i);
        height = X2line_down(height_START, height_TARGET, i);
        Gate_Ylength = easeInOutQuad(Gate_Ylength_START, Gate_Ylength_TARGET, i);
        Gate_Xlength = easeInOutQuad(Gate_Xlength_START, Gate_Xlength_TARGET, i);
        UIDrawSgate(x, y, width, height, Gate_Xlength, Gate_Ylength, 1);
        // Paint_DrawString_EN(buf, 64-8, 32-10, "X",
        //     &Font24, 1);
        float j = i * 2;
        if (j > 1)
            j = 1;
        DrawEllipse(buf, 63, 31, X2line_up(71, 15, j), X2line_up(71, 15, j), 1, 1);
        DrawEllipse(buf, 63, 31, easeInOutQuad(72, 13, j), easeInOutQuad(72, 13, j), 1, 0);
        render(buf, &frame_area);

        memset(buf, 0, SSD1306_BUF_LEN);
    }

    x_START = x_TARGET;
    y_START = y_TARGET;
    width_START = width_TARGET;
    height_START = height_TARGET;
    Gate_Ylength_START = Gate_Ylength_TARGET;
    width_TARGET = 126;
    height_TARGET = 19;
    int String_TARGER = 35;
    x_TARGET = (SSD1306_WIDTH - width_TARGET) / 2;
    y_TARGET = (SSD1306_HEIGHT - height_TARGET) / 2 + 1;
    Gate_Ylength_TARGET = 4;
    for (float i = 0; i < 1; i = i + 0.04)
    {
        x = easeInOutQuad(x_START, x_TARGET, i);
        y = easeInOutQuad(y_START, y_TARGET, i);
        width = easeInOutQuad(width_START, width_TARGET, i);
        height = easeInOutQuad(height_START, height_TARGET, i);
        Gate_Ylength = easeInOutQuad(Gate_Ylength_START, Gate_Ylength_TARGET, i);
        int Sring_X = easeInOutQuad(x_START, String_TARGER, i);
        int Sring_X_A = easeInOutQuad(85, String_TARGER, i);
        int Sring_X_B = easeInOutQuad(-8, String_TARGER, i);

        // 创建临时缓冲层用于处理 TECH (VIP 直接画在 buf)
        uint8_t buf_tech[SSD1306_BUF_LEN];
        memset(buf_tech, 0, SSD1306_BUF_LEN);

        // 1. 在主缓冲 buf 绘制 VIP
        Paint_DrawString_EN(buf, Sring_X_A - 5, 32 - 10, "V", &Font24, 1);
        Paint_DrawString_EN(buf, Sring_X_A - 5 - 14 * 1, 32 - 10, "I", &Font24, 1);
        Paint_DrawString_EN(buf, Sring_X_A - 5 - 14 * 2, 32 - 10, "P", &Font24, 1);

        // 2. 在临时缓冲 buf_tech 绘制 TECH
        Paint_DrawString_EN(buf_tech, Sring_X_B - 5 + 14 * 2 + 10, 32 - 10, "T", &Font24, 1);
        Paint_DrawString_EN(buf_tech, Sring_X_B - 5 + 14 * 3 + 10, 32 - 10, "E", &Font24, 1);
        Paint_DrawString_EN(buf_tech, Sring_X_B - 5 + 14 * 4 + 10, 32 - 10, "C", &Font24, 1);
        Paint_DrawString_EN(buf_tech, Sring_X_B - 5 + 14 * 5 + 10, 32 - 10, "H", &Font24, 1);

        // 3. 混合缓冲：以 Sring_X + 20 为界
        // 左侧保留 VIP (buf)，右侧覆盖为 TECH (buf_tech)
        int split_line = Sring_X + 20;
        for (int k = 0; k < SSD1306_BUF_LEN; k++)
        {
            if ((k % SSD1306_WIDTH) >= split_line)
            {
                buf[k] = buf_tech[k];
            }
        }

        DrawEllipse(buf, Sring_X + 19, 31, easeInOutQuad(15, 7, i), easeInOutQuad(15, 7, i), 1, 1);
        DrawEllipse(buf, Sring_X + 19, 31, easeInOutQuad(13, 5, i), easeInOutQuad(13, 5, i), 1, 0);
        DrawRectangle(buf, x + width, 0, 80, 80, 1, 0);
        UIDrawSgate(x, y, width, height, 5, Gate_Ylength, 1);
        render(buf, &frame_area);
        memset(buf, 0, SSD1306_BUF_LEN);
    }
    // sleep_ms(1000);
    sleep_ms(800);

    xStart = x_TARGET;
    yStart = y_TARGET;
    lengthStart = width_TARGET;
    heightStart = height_TARGET;
}

void UI_Update()
{
    // printf("UI Update Start\r\n");

    static uint8_t index = 0, itemSum = 0;
    // Get current page by id
    EasyUIPage_t *page = pageHead;

    // 软件重置
    if(menu_update_flag){
        menu_update_flag = 0;
        // printf("Menu Update\r\n");
        while (UILoadConfigUSB())
        {
            tight_loop_contents();
            // printf("Load Menu Config Failed, Please Check Your USB Connection and File Format\r\n");
            /* code */
        }
        MenuReset();
        itemsYPos = 16;
        index = 0;
        itemSum = 0;
    }

    

    
    while (page->id != pageIndex[layer])
    {
        page = page->next;
    }

    // 打印当前页面位置 项目情况
    // printf("Current Page: %d\r\n", page->id);
    // printf("Item Sum: %d\r\n", itemSum);

    /*先根据键位执行变换*/

    if (!functionIsRunning) // 只有非running状态进行菜单item控制
    {
        // Operation move reaction
        itemSum = page->itemTail->id;
        if (opnDown)
        {
            if (index < itemSum)
                index++;
            else if (listLoop)
                index = 0;
        }
        if (opnUp)
        {
            if (index > 0)
                index--;
            else if (listLoop)
                index = itemSum;
        }

        if (opnEnter)
        {

            for (EasyUIItem_t *item = page->itemHead; item != NULL; item = item->next)
            {
                if (item->id != index)
                {
                    continue;
                }

                UIItemOperationResponse(page, item, &index);

                break;
            }
            // 更新page
            while (page->id != pageIndex[layer])
            {
                page = page->next;
            }
        }

        if (opnExit)
        {
            if (layer == 0) // 在首页回退
            {
                ResetOpn();
                return;
            }

            pageIndex[layer] = 0;
            itemIndex[layer--] = 0;
            index = itemIndex[layer];

            // 更新page，渲染新的页面
            page = pageHead;

            while (page->id != pageIndex[layer])
            {
                page = page->next;
            }
        }
        // // -------------------------------------------------------------------------------------------

        // 重置键位状态
        ResetOpn();

        /*按键控件更新结束,开始渲染画面*/

        // Custom page--------------------------------------------------------------------------------
        if (page->funcType == PAGE_CUSTOM)
        {
            // page->Event(page);

            // // Clear the states of key to monitor next key action
            // opnForward = opnBackward = ResetOpn();

            // if (layer == 0)
            // {
            //     opnExit = false;

            //     //最外层返回键
            //     EasyUISendBuffer();
            //     return;
            // }

            // if (opnExit)
            // {
            //     opnExit = false;
            //     pageIndex[layer] = 0;
            //     itemIndex[layer--] = 0;
            //     index = itemIndex[layer];
            //     // EasyUITransitionAnim();
            //     // EasyUIDrawIndicator(page, index, timer, 1);
            // }

            // EasyUISendBuffer();
            return;
        }

        // List Page ---------------------------------------------------------------------------------
        UIListAni(page, index); // 除了动画过程中有渲染 其他变换过程没有渲染 需要在UIUpdate结尾渲染
    }

    /************UI上方的运行层******************* */
    // Running Item
    if (functionIsRunning)
    {
        for (EasyUIItem_t *item = page->itemHead; item != NULL; item = item->next)
        {
            if (item->id != index)
            {
                continue;
            }

            switch (item->funcType)
            {
            case ITEM_CHANGE_VALUE:
                switch (item->ValueType)
                {
                case INT_e:
                    UIEventChangeInt(item, page, false);
                    break;
                case UINT_e:
                    UIEventChangeInt(item, page, true);
                    break;
                case FLOAT_e:
                    UIEventChangeFloat(item, page, false);
                    break;
                case UFLOAT_e:
                    UIEventChangeFloat(item, page, true);
                    break;

                default:
                    break;
                }
                break;
            case ITEM_EVENT:
                function_to_menu = 0;      // 计时消除标记 否则内部SGate框的底色判定会出问题
                uartconfig_infunction = 1; // 进入uartconfig 模式 进入动画标记
                item->Event(item);
                if (functionIsRunning)
                {
                    return;
                }
                function_to_menu = 1; // 返回menu时的动画
                UIListAni(page, index);

                break;
            case ITEM_SEND:
                if (item->SendMode == Send_SaveSettings)
                {

                    saveitem_insendmode = 1;
                    save_settings();
                }
            case ITEM_CHECKBOX: // 仅在sendmode 非 nosend 时可进入functionisrunning 的这里的判定
            case ITEM_RADIO_BUTTON:
            case ITEM_SWITCH:
                if (item->SendMode == SendtoSettings)
                {
                    functionIsRunning = 0;
                    if (changesettings(item))
                    {
                        MenuConfigInit();
                    };
                    break;
                }
                else
                {
                    UISend(item, page); // save_settings也进入这里进行显示
                    break;
                }
                break;

            case ITEM_INFOMATION:
                UIinformation(item, page, index);
                break;
            default:
                // item->Event(item);
                break;
            }
            break;
        }
    }

    // 最终渲染
    render(buf, &frame_area);
    memset(buf, 0, SSD1306_BUF_LEN);
    // ResetOpn();
    return;
}

void testpage()
{
    ClearScreen(buf, &frame_area);
    uint16_t valuereturn = 0;
    while (true)
    {

        DrawRectangle(buf, -1, 0, valuereturn, 65, 1, 1);

        if (DEV_Digital_Read(KEYLEFT) == 0)
        {
            DrawRectangle(buf, 0, 0, 12, 12, 1, 1);
        }
        else
        {
            DrawRectangle(buf, 0, 0, 12, 12, 0, 1);
        }

        if (DEV_Digital_Read(KEYRIGHT) == 0)
        {
            DrawRectangle(buf, 116, 0, 12, 12, 1, 1);
        }
        else
        {
            DrawRectangle(buf, 116, 0, 12, 12, 0, 1);
        }

        if (DEV_Digital_Read(KEYUP) == 0)
        {
            DrawRectangle(buf, 0, 35, 12, 12, 1, 1);
        }
        else
        {
            DrawRectangle(buf, 0, 35, 12, 12, 0, 1);
        }

        if (DEV_Digital_Read(KEYDOWN) == 0)
        {
            DrawRectangle(buf, 0, 52, 12, 12, 1, 1);
        }
        else
        {
            DrawRectangle(buf, 0, 52, 12, 12, 0, 1);
        }

        if (DEV_Digital_Read(KEYOK) == 0)
        {
            DrawRectangle(buf, 116, 35, 12, 12, 1, 1);
        }
        else
        {
            DrawRectangle(buf, 116, 35, 12, 12, 0, 1);
        }

        if (DEV_Digital_Read(KEYCAN) == 0)
        {
            DrawRectangle(buf, 116, 52, 12, 12, 1, 1);
        }
        else
        {
            DrawRectangle(buf, 116, 52, 12, 12, 0, 1);
        }

        // if (DEV_Digital_Read(KEYOK) == 0 & DEV_Digital_Read(KEYCAN) == 0)
        // {
        //     uint wrap = map_tone_to_wrap(50);
        //     pwm_set_wrap(slice_num, wrap);
        //     pwm_set_gpio_level(BUZZER, 30000);
        //     DrawRectangle(buf,85,18,42,12,1,1);
        //     Paint_DrawString_EN(buf,85 , 18, "Buzzer",
        //         &Font12, 0);
        // }
        // else
        // {
        //     pwm_set_gpio_level(BUZZER, 0); // 静音
        //     Paint_DrawString_EN(buf,85 , 18, "Buzzer",
        //         &Font12, 1);

        // }

        if (DEV_Digital_Read(KEYDOWN) == 0 & DEV_Digital_Read(KEYCAN) == 0)
        {
            Open_StateLED();
            DrawRectangle(buf, 33, 54, 63, 12, 1, 1);
            Paint_DrawString_EN(buf, 33, 54, "状态LED",
                                &Font12, 0);
        }
        else
        {
            Close_StateLED();
            Paint_DrawString_EN(buf, 33, 54, "状态LED",
                                &Font12, 1);
        }

        // if (DEV_Digital_Read(KEYUP) == 0 & DEV_Digital_Read(KEYDOWN) == 0)
        // {
        //     pwm_set_gpio_level(LEDBPIN, 60000);
        //     DrawRectangle(buf,10,18,21,12,1,1);
        //     Paint_DrawString_EN(buf,10 , 18, "LED",
        //         &Font12, 0);

        // }
        // else
        // {
        //     pwm_set_gpio_level(LEDBPIN, 0);
        //     Paint_DrawString_EN(buf,10 , 18, "LED",
        //         &Font12, 1);

        // }

        if (DEV_Digital_Read(KEYUP) == 0 & DEV_Digital_Read(KEYOK) == 0)
        {
            valuereturn += 3;
            DrawRectangle(buf, 44, 37, 42, 12, 1, 1);
            Paint_DrawString_EN(buf, 51, 37, "退出",
                                &Font12, 0);
            if (valuereturn >= SSD1306_WIDTH)
            {
                ClearScreen(buf, &frame_area);
                DrawRectangle(buf, -1, 0, 129, 65, 1, 1);
                Paint_DrawString_EN(buf, 10, 30, "请松开全部按键",
                                    &Font12, 0);
                render(buf, &frame_area);
                // BUZcontrol(50, 0);
                // LEDcontrol(0);
                Close_StateLED();
                while (DEV_Digital_Read(KEYUP) == 0 || DEV_Digital_Read(KEYDOWN) == 0 || DEV_Digital_Read(KEYOK) == 0 || DEV_Digital_Read(KEYCAN) == 0)
                {
                    sleep_ms(30);
                };
                ResetOpn();
                functionIsRunning = false;
                return;
            }
        }
        else
        {
            valuereturn = 0;
            Paint_DrawString_EN(buf, 51, 37, "退出",
                                &Font12, 1);
        }

        DrawLine(buf, 12, 58, 30, 58, 1);
        DrawLine(buf, 116, 58, 98, 58, 1);

        DrawLine(buf, 12, 41, 47, 41, 1);
        DrawLine(buf, 116, 41, 81, 41, 1);

        // DrawLine(buf,20,32,20,58,1);
        // DrawLine(buf,108,32,108,58,1);

        render(buf, &frame_area);
        ClearScreen(buf, &frame_area);
        sleep_ms(10);
    }
}

const unsigned char gImage_QR[165] = {
    /* 0X10,0X01,0X00,0X21,0X00,0X21, */
    0XFE,
    0X19,
    0XB7,
    0XBF,
    0X80,
    0X82,
    0X17,
    0X53,
    0X20,
    0X80,
    0XBA,
    0XA9,
    0X89,
    0X2E,
    0X80,
    0XBA,
    0X59,
    0X8A,
    0X2E,
    0X80,
    0XBA,
    0X79,
    0X99,
    0X2E,
    0X80,
    0X82,
    0X33,
    0X51,
    0XA0,
    0X80,
    0XFE,
    0XAA,
    0XAA,
    0XBF,
    0X80,
    0X00,
    0X94,
    0XC6,
    0X00,
    0X00,
    0XEF,
    0X84,
    0XEC,
    0XE2,
    0X00,
    0X90,
    0X44,
    0XE8,
    0XA4,
    0X80,
    0X12,
    0X20,
    0XAE,
    0XE5,
    0X80,
    0X35,
    0X6E,
    0X64,
    0XAD,
    0X00,
    0X06,
    0XB4,
    0X6D,
    0XE4,
    0X80,
    0X78,
    0XBC,
    0X6E,
    0X76,
    0X80,
    0X46,
    0X4E,
    0X20,
    0X69,
    0X80,
    0X70,
    0XBD,
    0X4D,
    0X15,
    0X00,
    0X26,
    0XA6,
    0XD7,
    0X71,
    0X80,
    0X14,
    0X82,
    0XD8,
    0XE2,
    0X80,
    0X9F,
    0XAA,
    0X64,
    0X2D,
    0X80,
    0X15,
    0X0C,
    0X47,
    0X35,
    0X00,
    0XA3,
    0XAE,
    0X7C,
    0XF1,
    0X80,
    0X1C,
    0X2E,
    0X2E,
    0XE2,
    0X80,
    0X8F,
    0X4A,
    0X2A,
    0X9D,
    0X80,
    0X64,
    0X9D,
    0XCF,
    0X51,
    0X00,
    0X87,
    0X04,
    0X7C,
    0XFC,
    0X00,
    0X00,
    0X84,
    0X6E,
    0X8D,
    0X80,
    0XFE,
    0XE2,
    0XE7,
    0XAB,
    0X80,
    0X82,
    0XA7,
    0XF4,
    0X89,
    0X00,
    0XBA,
    0XAE,
    0XDE,
    0XF9,
    0X00,
    0XBA,
    0X6E,
    0XC9,
    0X1D,
    0X00,
    0XBA,
    0XA0,
    0X0D,
    0X9F,
    0X80,
    0X82,
    0X84,
    0XFE,
    0XC1,
    0X00,
    0XFE,
    0XE6,
    0X7D,
    0XE5,
    0X80,
};

void DrawQRCode(int start_x, int start_y)
{
    // 遍历原始二维码的每个像素
    for (int y = 0; y < 33; y++)
    {
        int row_offset = y * 5;

        for (int byte_idx = 0; byte_idx < 5; byte_idx++)
        {
            uint8_t byte = gImage_QR[row_offset + byte_idx];

            for (int bit = 7; bit >= 0; bit--)
            {
                int x = byte_idx * 8 + (7 - bit);
                if (x >= 33)
                    break;

                // 获取像素状态
                bool pixel_on = (byte >> bit) & 0x01;

                // 计算放大后的基准坐标
                int base_x = start_x + x * 2;
                int base_y = start_y + y * 2;

                // 绘制2x2像素块
                for (int dy = 0; dy < 2; dy++)
                {
                    for (int dx = 0; dx < 2; dx++)
                    {
                        SetPixel(buf, base_x + dx, base_y + dy, pixel_on);
                    }
                }
            }
        }
    }
}
void TEST()
{

    while (1)
    {
        /* code */
    }
}
#endif
