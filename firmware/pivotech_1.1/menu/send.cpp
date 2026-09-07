
#ifndef _SEND_CPP_
#define _SEND_CPP_


#define SEND_END '\n'

#include "CONFIG_FLO.hpp"
#include "menu/UI.hpp"
#include "menu/StateLED.hpp"
#include <string>
#include <cstring>
#include "menu/core.hpp"


extern char* backString;
extern bool core1_need_wait;


void update_backstring_SendOpention_OnlySelected(EasyUIPage_t *page)
{ // omly for selected flag tiem
    // 释放旧内存
    free(backString);
    backString = NULL;

    // 计算总长度（包含所有分号）
    size_t total_len = 1; // 终止符
    int valid_count = 0;

    for (EasyUIItem_t *item = page->itemHead; item != NULL; item = item->next)
    {

        if (item->funcType == ITEM_SWITCH ||
            item->funcType == ITEM_RADIO_BUTTON ||
            item->funcType == ITEM_CHECKBOX)
        {
            if (*item->flag && item->title)
            {
                total_len += strlen(item->title) + 1; // +1 分号
                valid_count++;
            }
        }
    }

    if (valid_count == 0)
        return; 

    // 分配内存（不再减少最后的分号空间）
    char *buffer = (char *)malloc(total_len);
    if (!buffer)
        return; 

    // 拼接字符串
    char *cursor = buffer;

    for (EasyUIItem_t *item = page->itemHead; item != NULL; item = item->next)
    {
        if (*item->flag && item->title)
        {
            size_t len = strlen(item->title);
            memcpy(cursor, item->title, len);
            cursor += len;
            *cursor++ = SEND_END; // 每个标题后强制添加分号
        }
    }
    *cursor = '\0'; // 正确终止字符串

    // 赋值全局变量
    backString = buffer;
    return;
}

void update_backstring_SendOpention_all(EasyUIPage_t *page, bool if_have_title = 0)
{
    // 释放旧内存
    free(backString);
    backString = NULL;

    // 计算总长度（包含所有分号）
    size_t total_len = 1; // 终止符
    int valid_count = 0;
    char Str[ARRAY_LEN];
    size_t len;

    for (EasyUIItem_t *item = page->itemHead; item != NULL; item = item->next)
    {

        if (item->funcType == ITEM_SWITCH ||
            item->funcType == ITEM_RADIO_BUTTON ||
            item->funcType == ITEM_CHECKBOX ||
            item->funcType == ITEM_CHANGE_VALUE)
        {
            if(item->funcType ==item->funcType == ITEM_SWITCH ||
                item->funcType == ITEM_RADIO_BUTTON ||
                item->funcType == ITEM_CHECKBOX ){
                    total_len += 1;
                }
            else if(item->funcType == ITEM_CHANGE_VALUE){

                if (item->ValueType == FLOAT_e || item->ValueType == UFLOAT_e)
                {
                    sprintf(Str, "%.*lf", 4, *item->param); // 4位小数
                    total_len += strlen(Str);
                }
                else if (item->ValueType == INT_e || item->ValueType == UINT_e)
                {
                    sprintf(Str, "%.*lf", 0, *item->param);
                    // 如果是0位小数，去除小数点
                    // 遍历并寻找小数点并去除
                    char *dot = strchr(Str, '.');
                    if (dot)
                    {
                        *dot = '\0'; // 将小数点替换为字符串结束符
                    }
                    total_len += strlen(Str);
                }
            }

            if(if_have_title){
                total_len += strlen(item->title); // +1 分号
            }
            total_len += 2;
            valid_count++;
        }
    }

    if (valid_count == 0)
        return; ///////////////////////////////////////////////

    // 分配内存（不再减少最后的分号空间）
    char *buffer = (char *)malloc(total_len);
    if (!buffer)
        return; ///////////////////////////////////////////////

    // 拼接字符串
    char *cursor = buffer;

    for (EasyUIItem_t *item = page->itemHead; item != NULL; item = item->next)
    {
        if (item->funcType == ITEM_SWITCH ||
            item->funcType == ITEM_RADIO_BUTTON ||
            item->funcType == ITEM_CHECKBOX ||
            item->funcType == ITEM_CHANGE_VALUE)
        {
            if(if_have_title){
                len = strlen(item->title);
                memcpy(cursor, item->title, len);
                cursor += len;
                *cursor++ = '='; // 每个标题后强制添加分号
            }

            if(item->funcType  == ITEM_SWITCH ||
                item->funcType == ITEM_RADIO_BUTTON ||
                item->funcType == ITEM_CHECKBOX ){
                    sprintf(Str, "%d", *item->flag);
                    len = 1;
                }
            else if(item->funcType == ITEM_CHANGE_VALUE){
                if (item->ValueType == FLOAT_e || item->ValueType == UFLOAT_e)
                {
                    sprintf(Str, "%.*lf", 4, *item->param); // 4位小数
                    len = strlen(Str);
                }
                else if (item->ValueType == INT_e || item->ValueType == UINT_e)
                {
                    sprintf(Str, "%.*lf", 0, *item->param);
                    // 如果是0位小数，去除小数点
                    // 遍历并寻找小数点并去除
                    char *dot = strchr(Str, '.');
                    if (dot)
                    {
                        *dot = '\0'; // 将小数点替换为字符串结束符
                    }
                    len = strlen(Str);
                }
            }
            memcpy(cursor, Str, len);
            cursor += len;            
            *cursor++ = SEND_END; // 每个标题后强制添加分号
        }
    }
    *cursor = '\0'; // 正确终止字符串

    // 赋值全局变量
    backString = buffer;
    return;
}

void update_backstring_ChangeValue(EasyUIItem_t *item, EasyUIPage_t *page)
{

    // 释放旧内存
    free(backString);
    backString = NULL;

    // 格式化数值为字符串
    char Str[ARRAY_LEN];
    size_t value_len = 0;

    if (item->ValueType == FLOAT_e || item->ValueType == UFLOAT_e)
    {
        sprintf(Str, "%.*lf", 4, *item->param); // 4位小数
        value_len = strlen(Str);
    }
    else if (item->ValueType == INT_e || item->ValueType == UINT_e)
    {
        sprintf(Str, "%.*lf", 0, *item->param);
        // 如果是0位小数，去除小数点
        // 遍历并寻找小数点并去除
        char *dot = strchr(Str, '.');
        if (dot)
        {
            *dot = '\0'; // 将小数点替换为字符串结束符
        }
        value_len = strlen(Str);
    }
    else
    {
        return; // 不支持的ValueType
    }

    size_t title_len = strlen(item->title);
    size_t total_len = 0;

    // 根据SendMode计算总长度
    if (item->SendMode == RealtimeSend_Title_Value || item->SendMode == Send_Title_Value)
    {
        total_len = title_len + value_len + 3; // "title=value;"
    }
    else if (item->SendMode == RealtimeSend_Value || item->SendMode == Send_Value)
    {
        total_len = value_len + 2; // "value;"
    }
    else
    {
        return; // 不支持的SendMode
    }

    // 分配内存
    char *buffer = (char *)malloc(total_len);
    if (!buffer)
        return;

    // 生成格式化字符串
    if (item->SendMode == RealtimeSend_Title_Value || item->SendMode == Send_Title_Value)
    {
        snprintf(buffer, total_len, "%s=%s%c", item->title, Str, SEND_END);
    }
    else if (item->SendMode == RealtimeSend_Value || item->SendMode == Send_Value)
    {
        snprintf(buffer, total_len, "%s%c", Str, SEND_END);
    }

    // 更新全局变量
    backString = buffer;
}

void update_backstring_Flag(EasyUIItem_t *item, EasyUIPage_t *page)
{

    // 释放旧内存
    free(backString);
    backString = NULL;

    size_t title_len = strlen(item->title);
    size_t total_len = 0;

    // 根据SendMode计算总长度
    if (item->SendMode == RealtimeSend_Title_Value || item->SendMode == Send_Title_Value)
    {
        total_len = title_len + 4; // "title=0;"
    }
    else if (item->SendMode == RealtimeSend_Value || item->SendMode == Send_Value)
    {
        total_len = 3; // "0;"
    }
    else
    {
        return; // 不支持的SendMode
    }

    // 分配内存
    char *buffer = (char *)malloc(total_len);
    if (!buffer)
        return;

    // 生成格式化字符串
    if (item->SendMode == RealtimeSend_Title_Value || item->SendMode == Send_Title_Value)
    {
        snprintf(buffer, total_len, "%s=%d%c", item->title, *item->flag, SEND_END);
    }
    else if (item->SendMode == RealtimeSend_Value || item->SendMode == Send_Value)
    {
        snprintf(buffer, total_len, "%d%c", *item->flag, SEND_END);
    }

    // 更新全局变量
    backString = buffer;
}

void update_backstring_sendtitle(EasyUIItem_t *item, EasyUIPage_t *page)
{

    // 释放旧内存
    free(backString);
    backString = NULL;

    size_t total_len = strlen(item->title) + 2;

    // 分配内存
    char *buffer = (char *)malloc(total_len);
    if (!buffer)
        return;

    // 生成格式化字符串
    snprintf(buffer, total_len, "%s%c", item->title, SEND_END);

    // 更新全局变量
    backString = buffer;
}

void SendMessage(EasyUIItem_t *item, EasyUIPage_t *page)
{

    switch (item->SendMode)
    {
    case NoSend:
        free(backString);
        backString = NULL;
        break;

    case RealtimeSend_Title_Value:
    case RealtimeSend_Value:
        if (item->funcType == ITEM_SWITCH ||
            item->funcType == ITEM_RADIO_BUTTON ||
            item->funcType == ITEM_CHECKBOX)
        {
            break;
        }
    case Send_Title_Value:
    case Send_Value:

        if (item->funcType == ITEM_CHANGE_VALUE)
        {
            update_backstring_ChangeValue(item, page);
            break;
        }

        if (item->funcType == ITEM_SWITCH ||
            item->funcType == ITEM_RADIO_BUTTON ||
            item->funcType == ITEM_CHECKBOX)
        {
            update_backstring_Flag(item, page);
            break;
        }

        break;

    case Send_Title:
        if (item->funcType != ITEM_SEND)
        {
            break;
        }
        update_backstring_sendtitle(item, page);
        break;

    case SendOption_Title:
        if (item->funcType != ITEM_SEND)
        {
            break;
        }
        update_backstring_SendOpention_OnlySelected(page);
        break;

    case SendOption_Value:
        if (item->funcType != ITEM_SEND)
        {
            break;
        }
        update_backstring_SendOpention_all(page);
        break;

    case SendOption_Title_Value:
        if (item->funcType != ITEM_SEND)
        {
            break;
        }
        update_backstring_SendOpention_all(page, 1);
        break;

        break;

    default:
        break;
    }


    core1_need_wait = 1;


    
    // if(menusetting_USB){
    //     stdio_filter_driver(&stdio_usb); // 确保只输出到 USB
    //     for(const char *p = backString; *p; p++) {
    //     putchar_raw(*p);  // 输出到USB
    // }
    // }
    // if(menusetting_UART){   
    //     stdio_filter_driver(&stdio_uart);
    //     uart_puts(UART_ID, backString);
    // }
    
    StartStateLED();


    return;
};

#endif