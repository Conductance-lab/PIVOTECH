#ifndef _SGATESETTING_CPP_
#define _SGATESETTING_CPP_

#include "CONFIG_FLO.hpp"
#include "menu/SGateSetting.hpp"
#include "menu/UI.hpp"
#include "menu/flash.hpp"
#include "menu/core.hpp"
#include "menu/StateLED.hpp"
#include "pico/multicore.h"

extern bool menusetting_USB, menusetting_UARTA, menusetting_UARTB;
extern bool menusetting_Dark, menusetting_Light;
extern bool menusetting_ani_no, menusetting_ani_fast, menusetting_ani_normal, menusetting_ani_slow;
extern sFONT *FontSelected;
extern uint16_t ChangeVal_Width;
extern uint8_t ITEM_HEIGHT;
extern uint8_t FONT_HEIGHT;
extern uint8_t FONT_WIDTH;

extern bool keep_host_set;
extern int uart_baud_selection;
extern int uart_databits_selection;
extern int uart_parity_selection;
extern int uart_stopbits_selection;
extern bool core1_need_wait;

void apply_uart_settings_from_host();
void apply_uart_settings();
void apply_uart_settings();

// 0:FONT8 1:FONT12 2:FONT16
void MenuConfigInit()
{

    FontSelected = &Font12;

    ITEM_HEIGHT = (FontSelected->Height) * 1.2;
    FONT_HEIGHT = (FontSelected->Height);
    FONT_WIDTH = (FontSelected->Width);

    // changevalue的width控制
    ChangeVal_Width = (FontSelected->Height) * 7;

    if (menusetting_UARTA && !menusetting_UARTB)
    {
        Selector_TA_TX_TB_RX();
    }
    else if (!menusetting_UARTA && menusetting_UARTB)
    {
        Selector_TA_RX_TB_TX();
    }

    if (keep_host_set)
    { // 在此应用UART参数
        apply_uart_settings_from_host();
    }
    else
    {
        apply_uart_settings();
    }
    /// 这里应该有波特率的更新///////////////////////////////////////////////////////////
}

bool changesettings(EasyUIItem_t *item)
{
    switch (item->funcType)
    {
    case ITEM_CHECKBOX:
        // Connection mode
        if (item->flag == &menusetting_USB)
        {
            StartStateLED();
            return 1; // 返回需要重新刷新
        }
        else if (item->flag == &menusetting_UARTA)
        {
            StartStateLED();
            menusetting_UARTB = 0; // 强制TB关闭，保持TA/TB互斥
            
            return 1;              // 返回需要重新刷新
        }
        else if (item->flag == &menusetting_UARTB)
        {
            StartStateLED();
            menusetting_UARTA = 0; // 强制TA关闭，保持TA/TB互斥
            return 1;              // 返回需要重新刷新
        }
        break;
    case ITEM_RADIO_BUTTON:
        // Theme
        if (item->flag == &menusetting_Dark)
        {
            SSD1306_invert(0);
            menusetting_Dark = 1;
            menusetting_Light = 0;
        }
        else if (item->flag == &menusetting_Light)
        {
            SSD1306_invert(1);
            menusetting_Dark = 0;
            menusetting_Light = 1;
        }

        // Font

        // Animaton
        else if (item->flag == &menusetting_ani_no)
        {
            menusetting_ani_no = 1;
            menusetting_ani_fast = 0;
            menusetting_ani_normal = 0;
            menusetting_ani_slow = 0;
            footlength = footlengthDefault = 1;
        }
        else if (item->flag == &menusetting_ani_slow)
        {
            menusetting_ani_no = 0;
            menusetting_ani_fast = 0;
            menusetting_ani_normal = 0;
            menusetting_ani_slow = 1;
            footlength = footlengthDefault = 0.04;
        }
        else if (item->flag == &menusetting_ani_normal)
        {
            menusetting_ani_no = 0;
            menusetting_ani_fast = 0;
            menusetting_ani_normal = 1;
            menusetting_ani_slow = 0;
            footlength = footlengthDefault = 0.08;
        }
        else if (item->flag == &menusetting_ani_fast)
        {
            menusetting_ani_no = 0;
            menusetting_ani_fast = 1;
            menusetting_ani_normal = 0;
            menusetting_ani_slow = 0;
            footlength = footlengthDefault = 0.15;
        }

        // // Buadrate
        // else if (item->flag == &menusetting_9600)
        // {
        //     menusetting_9600 = 1;
        //     menusetting_19200 = 0;
        //     menusetting_38400 = 0;
        //     menusetting_57600 = 0;
        //     menusetting_115200 = 0;
        //     SettingBaudrate = 9600;
        //     return 1; // 返回需要重新刷新
        // }
        // else if (item->flag == &menusetting_19200)
        // {
        //     menusetting_9600 = 0;
        //     menusetting_19200 = 1;
        //     menusetting_38400 = 0;
        //     menusetting_57600 = 0;
        //     menusetting_115200 = 0;
        //     SettingBaudrate = 19200;
        //     return 1; // 返回需要重新刷新
        // }
        // else if (item->flag == &menusetting_38400)
        // {
        //     menusetting_9600 = 0;
        //     menusetting_19200 = 0;
        //     menusetting_38400 = 1;
        //     menusetting_57600 = 0;
        //     menusetting_115200 = 0;
        //     SettingBaudrate = 38400;
        //     return 1; // 返回需要重新刷新
        // }
        // else if (item->flag == &menusetting_57600)
        // {
        //     menusetting_9600 = 0;
        //     menusetting_19200 = 0;
        //     menusetting_38400 = 0;
        //     menusetting_57600 = 1;
        //     menusetting_115200 = 0;
        //     SettingBaudrate = 57600;
        //     return 1; // 返回需要重新刷新
        // }
        // else if (item->flag == &menusetting_115200)
        // {
        //     menusetting_9600 = 0;
        //     menusetting_19200 = 0;
        //     menusetting_38400 = 0;
        //     menusetting_57600 = 0;
        //     menusetting_115200 = 1;
        //     SettingBaudrate = 115200;
        //     return 1; // 返回需要重新刷新
        // }

        break;
    default:
        break;
    }

    return 0;
}

bool configsettingcmp(char *a, char *b, int length)
{
    int num0, num1, num2, num3, num4, num5, num6, num7, num8;
    int result = sscanf(a, "X,%d,%d,%d,%d,%d,%d,%d,%d,%d;", &num0, &num1, &num2, &num3, &num4, &num5, &num6, &num7, &num8);
    int num0c, num1c, num2c, num3c, num4c, num5c, num6c, num7c, num8c;
    int resultc = sscanf(b, "X,%d,%d,%d,%d,%d,%d,%d,%d,%d;", &num0c, &num1c, &num2c, &num3c, &num4c, &num5c, &num6c, &num7c, &num8c);
    if ((num0 == num0c) &&
        (num1 == num1c) &&
        (num2 == num2c) &&
        (num3 == num3c) &&
        (num4 == num4c) &&
        (num5 == num5c) &&
        (num6 == num6c) &&
        (num7 == num7c) &&
        (num8 == num8c))
    {
        return 0;
    }
    return 1;
}


char current_configsettings[CONFIG_SETTINGS_MAX_LEN];

void save_settings()
{
    

    // sleep_ms(1000); // 确保上位机有时间准备接收数据
    
    int num[9] = {0};

    num[0] = PIVOsystem_mode_code; // 预留位，暂不使用
    if (num[0] > 5 || num[0] < 0)
    {
        num[0] = 0; // 默认值
    }

    // // 解析波特率设置 (num0)
    // if (menusetting_9600)
    //     num[0] = 0;
    // else if (menusetting_19200)
    //     num[0] = 1;
    // else if (menusetting_38400)
    //     num[0] = 2;
    // else if (menusetting_57600)
    //     num[0] = 3;
    // else if (menusetting_115200)
    //     num[0] = 4;
    // else if( menusetting_host)
    //     num[0] = 9;

    // 解析连接模式 (num1)，严格遵循二进制逻辑
    // 二进制位定义：USB(高位)、TA(中位)、TB(低位)
    if (menusetting_USB && !menusetting_UARTA && !menusetting_UARTB)
    {
        num[1] = 4; // 100 - USB(Serial)
    }
    else if (!menusetting_USB && menusetting_UARTA && !menusetting_UARTB)
    {
        num[1] = 2; // 010 - TA(UART)
    }
    else if (!menusetting_USB && !menusetting_UARTA && menusetting_UARTB)
    {
        num[1] = 1; // 001 - TB(UART)
    }
    else if (menusetting_USB && menusetting_UARTA && !menusetting_UARTB)
    {
        num[1] = 6; // 110 - USB&TA
    }
    else if (menusetting_USB && !menusetting_UARTA && menusetting_UARTB)
    {
        num[1] = 5; // 101 - USB&TB
    }
    else
    {
        num[1] = 0; // 100 - None or unsupported combination, default to USB only
    }

    // 解析主题设置 (num2)
    num[2] = menusetting_Light ? 1 : 0;

    // 解析字体大小 (num3)
    // if (menusetting_FontL)
    //     num[3] = 0;
    // else if (menusetting_FontM)
    //     num[3] = 1;
    // else if (menusetting_FontS)
    //     num[3] = 2;

    num[3] = 0;

    // 解析动画速度 (num4)
    if (menusetting_ani_no)
        num[4] = 0;
    else if (menusetting_ani_fast)
        num[4] = 1;
    else if (menusetting_ani_normal)
        num[4] = 2;
    else if (menusetting_ani_slow)
        num[4] = 3;

    // 直接读取LED和BUZ设置
    int tempnum5;
    if (uart_baud_selection == 0)
    {
        tempnum5 = BAUD_NUMBERS; // 使用特殊值表示“保持主机设置”
    }
    else
    {
        tempnum5 = uart_baud_selection;
    }

    num[5] = keep_host_set ? ((-1) * tempnum5) : tempnum5;
    num[6] = uart_databits_selection;
    num[7] = uart_parity_selection;
    num[8] = uart_stopbits_selection;

    // 生成配置字符串
    snprintf(current_configsettings, 32, "X,%d,%d,%d,%d,%d,%d,%d,%d,%d;",
             num[0], num[1], num[2], num[3], num[4], num[5], num[6], num[7], num[8]);
    
    extern bool if_need_send_menu_settings;
    if_need_send_menu_settings = 1; // 标记需要发送菜单设置到上位机/////////////////////

    

    sleep_ms(10); // 确保数据准备好发送


    multicore_reset_core1();

    sleep_ms(10); // 确保core1已重置

    char saved_configsettings[CONFIG_SETTINGS_MAX_LEN];
    load_configsettings_to_sram(saved_configsettings);
    // printf("Current Config Settings: %s\r\n", current_configsettings); /////////////////////////////////

    if (configsettingcmp(saved_configsettings, current_configsettings, CONFIG_SETTINGS_MAX_LEN) != 0)
    {

        save_configsettings(current_configsettings);
        
    }


    // printf("Saved Config Settings: %s\r\n", current_configsettings); /////////////////////////////////

    // 先重置 core1（确保它处于清理状态），然后重新启动 

    multicore_launch_core1(core1_main);
}

#endif
