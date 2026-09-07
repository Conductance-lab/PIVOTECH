
#ifndef _MENU_C_
#define _MENU_C_
#include "menu/menu.hpp"
#include "menu/flash.hpp"
#include "menu/UI.hpp"
#include "../CONFIG_FLO.hpp"
#include <map>
#include <string>
#include "read/uartconfig.hpp"
#include "menu/load.hpp"
#include <cstring>
#include "pico/multicore.h"
#include "menu/core.hpp"
std::map<std::string, int> title_to_id; // 名称到编号的映射

EasyUIPage_t Page[128];
bool ifpagehadJump[128];
EasyUIItem_t Item[256];
double ItemValue[256];
bool ItemFlag[256];

EasyUIItem_t no_import;

bool changesettingsfromconfig(char *config)
{

    char *ptr = config;
    int num0, num1, num2, num3, num4, num5, num6, num7, num8;
    int result = sscanf(ptr, "X,%d,%d,%d,%d,%d,%d,%d,%d,%d;", &num0, &num1, &num2, &num3, &num4, &num5, &num6, &num7, &num8);

    if (result != 9)
    {
        UIAddItem(&Page[0], &no_import, "[无导入项]", ITEM_PAGE_DESCRIPTION);
    }

    if(num0>5 || num0<0){
        num0 = 0; // 默认值
    }
    PIVOsystem_mode_code = num0;

  

    // Connection mode
    if (num1 == 0)
    {
        // 000
        menusetting_USB = 0;
        menusetting_UARTA = 0;
        menusetting_UARTB = 0;
    }
    else if (num1 == 1)
    {
        // 001
        menusetting_USB = 0;
        menusetting_UARTA = 0;
        menusetting_UARTB = 1;
    }
    else if (num1 == 2)
    {
        // 010
        menusetting_USB = 0;
        menusetting_UARTA = 1;
        menusetting_UARTB = 0;
    }
    else if (num1 == 4)
    {
        // 100
        menusetting_USB = 1;
        menusetting_UARTA = 0;
        menusetting_UARTB = 0;
    }
    else if (num1 == 5)
    {
        // 101
        menusetting_USB = 1;
        menusetting_UARTA = 0;
        menusetting_UARTB = 1;
    }
    else if (num1 == 6)
    {
        // 110
        menusetting_USB = 1;
        menusetting_UARTA = 1;
        menusetting_UARTB = 0;
    }
    else
    { // 不支持TATB同时打开 其他非法情况仅打开USB
        menusetting_USB = 1;
        menusetting_UARTA = 0;
        menusetting_UARTB = 0;
    }

    // Theme
    if (num2 == 0)
    {
        SSD1306_invert(0);
        menusetting_Dark = 1;
        menusetting_Light = 0;
    }
    else if (num2 == 1)
    {
        SSD1306_invert(1);
        menusetting_Dark = 0;
        menusetting_Light = 1;
    }

    // Font
    if (num3 == 0)
    {
    };

    // Animaton
    if (num4 == 0)
    {
        menusetting_ani_no = 1;
        menusetting_ani_fast = 0;
        menusetting_ani_normal = 0;
        menusetting_ani_slow = 0;
        footlength = footlengthDefault = 1;
    }
    else if (num4 == 3)
    {
        menusetting_ani_no = 0;
        menusetting_ani_fast = 0;
        menusetting_ani_normal = 0;
        menusetting_ani_slow = 1;
        footlength = footlengthDefault = 0.04;
    }
    else if (num4 == 2)
    {
        menusetting_ani_no = 0;
        menusetting_ani_fast = 0;
        menusetting_ani_normal = 1;
        menusetting_ani_slow = 0;
        footlength = footlengthDefault = 0.08;
    }
    else if (num4 == 1)
    {
        menusetting_ani_no = 0;
        menusetting_ani_fast = 1;
        menusetting_ani_normal = 0;
        menusetting_ani_slow = 0;
        footlength = footlengthDefault = 0.15;
    }

    // printf("Parsed animation setting: num4=%d, ani_no=%d, ani_fast=%d, ani_normal=%d, ani_slow=%d, footlength=%f\r\n", num4, menusetting_ani_no, menusetting_ani_fast, menusetting_ani_normal, menusetting_ani_slow, footlength); ///////////////////////////////////////////////

    if (num5 < 0)
    {
        keep_host_set = true;
        uart_baud_selection = (-num5) % BAUD_NUMBERS; // 第0个时 为了区分负数 使用28 因此在此处理
    }
    else
    {
        keep_host_set = false;
        uart_baud_selection = num5 % BAUD_NUMBERS;
    }

    uart_databits_selection = num6;
    uart_parity_selection = num7;
    uart_stopbits_selection = num8;

    MenuConfigInit(); // 刷新相关配置数据

    // printf("Parsed config: num0=%d, num1=%d, num2=%d, num3=%d, num4=%d, num5=%d, num6=%d, num7=%d, num8=%d\r\n", num0, num1, num2, num3, num4, num5, num6, num7, num8); ///////////////////////////////////////////////

    return 0;
}

bool ParseConfig(char *config)
{

    const char *ptr = config;
    int currentPage = -1;
    int pagecount = -1;
    int itemcount = -1;

    while (*ptr)
    {
        // 跳过换行
        if (*ptr == '\n')
        {
            ptr++;
        }
        else if (*ptr == '\r')
        {
            ptr++;
        }
        else if (*ptr == ' ')
        {
            ptr++;
        }
        else if (*ptr == ';')
        {
            ptr++;
        }

        // 跳过 #注释(允许换行)#
        else if (*ptr == '#')
        {
            ptr++;
            while (*ptr != '\0' && *ptr != '#')
            {
                ptr++;
            };
            if (*ptr == '#')
            {
                ptr++;
            }
            if (*ptr == '\0')
            {
                return 1;
            };
            continue;
        }

        // 解析指令
        else if (*ptr == '$')
        {
            ptr++;

            char title[128] = {0};
            // 提取指令类型
            sscanf(ptr, "%127[^;]", title);
            ptr += strlen(title) + 1;

            std::string title_str(title); // 转为 string 方便操作

            // 查询是否已存在
            auto it = title_to_id.find(title_str);
            if (it != title_to_id.end())
            {
                // 已存在，获取对应编号
                currentPage = it->second;
            }
            else
            {
                // 不存在，分配新编号
                pagecount++; // page数量
                currentPage = pagecount;
                UIAddPage(&Page[currentPage], PAGE_LIST);
                int new_id = Page[currentPage].id; // 读取编号
                title_to_id[title_str] = new_id;
            }
        }

        else if (*ptr == 'D')
        {
            ptr++;

            if (*ptr != ',')
                return 1; // 检查分隔符
            ptr++;

            char temp_title[128] = {0}; // 临时存储读取的字符串
            sscanf(ptr, "%127[^;]", temp_title);
            ptr += strlen(temp_title) + 1; // 移动指针到分号后

            // 根据实际长度动态分配内存
            const size_t title_len = strlen(temp_title);
            char *const title = new char[title_len + 1]; // 堆内存分配
            strcpy(title, temp_title);                   // 复制内容

            itemcount++;
            UIAddItem(&Page[currentPage], &Item[itemcount], title, ITEM_PAGE_DESCRIPTION);
        }

        else if (*ptr == 'J')
        {
            ptr++;

            if (*ptr != ',')
                return 1; // 检查分隔符
            ptr++;

            char temp_title[128] = {0}; // 临时存储读取的字符串
            sscanf(ptr, "%127[^;]", temp_title);
            ptr += strlen(temp_title) + 1; // 移动指针到分号后

            // 根据实际长度动态分配内存
            const size_t title_len = strlen(temp_title);
            char *const title = new char[title_len + 1]; // 堆内存分配
            strcpy(title, temp_title);                   // 复制内容

            std::string title_str(title); // 转为 string 方便操作

            int TargetcurrentPage;

            // 查询是否已存在
            auto it = title_to_id.find(title_str);
            if (it != title_to_id.end())
            {
                // 已存在，获取对应编号
                TargetcurrentPage = it->second;
            }
            else
            {
                // 不存在，分配新编号
                pagecount++;
                TargetcurrentPage = pagecount;
                UIAddPage(&Page[TargetcurrentPage], PAGE_LIST);
                int new_id = Page[TargetcurrentPage].id; // 读取编号
                title_to_id[title_str] = new_id;
            }

            if (ifpagehadJump[TargetcurrentPage] == 0)
            {
                itemcount++;
                UIAddItem(&Page[currentPage], &Item[itemcount], title, ITEM_JUMP_PAGE, Page[TargetcurrentPage].id);
                ifpagehadJump[TargetcurrentPage] = 1;
            }
            else
            {
                return 1;
            }
        }
        else if (*ptr == 'S')
        {
            ptr++;

            if (*ptr != ',')
                return 1; // 检查分隔符
            ptr++;

            char temp_title[128] = {0}; // 临时存储读取的字符串
            sscanf(ptr, "%127[^,]", temp_title);
            ptr += strlen(temp_title) + 1; // 移动指针到分号后

            // 根据实际长度动态分配内存
            const size_t title_len = strlen(temp_title);
            char *const title = new char[title_len + 1]; // 堆内存分配
            strcpy(title, temp_title);                   // 复制内容

            char sendmode[4] = {0}; // 临时存储读取的字符串
            sscanf(ptr, "%3[^,]", sendmode);
            ptr += strlen(sendmode) + 1; // 移动指针到分号后

            SendMode_e SENDMODE = NoSend;
            if (strcmp(sendmode, "NS") == 0)
                SENDMODE = NoSend;
            else if (strcmp(sendmode, "ST") == 0)
                SENDMODE = Send_Title;
            else if (strcmp(sendmode, "SV") == 0)
                SENDMODE = Send_Value;
            else if (strcmp(sendmode, "STV") == 0)
                SENDMODE = Send_Title_Value;
            else if (strcmp(sendmode, "RV") == 0)
                SENDMODE = RealtimeSend_Value;
            else if (strcmp(sendmode, "RTV") == 0)
                SENDMODE = RealtimeSend_Title_Value;
            else if (strcmp(sendmode, "OT") == 0)
                SENDMODE = SendOption_Title;
            else if (strcmp(sendmode, "OV") == 0)
                SENDMODE = SendOption_Value;
            else if (strcmp(sendmode, "OTV") == 0)
                SENDMODE = SendOption_Title_Value;

            bool item_flag = 0; // 声明 double 变量
            int num_read = 0;   // 用于记录读取的字符数

            // 从 ptr 中提取数值（假设数值后跟分号 `;`）
            sscanf(ptr, "%d;%n", &item_flag, &num_read);

            ptr += num_read; // 移动指针跳过分号

            itemcount++;
            ItemFlag[itemcount] = item_flag;
            UIAddItem(&Page[currentPage], &Item[itemcount], title, ITEM_SWITCH, &ItemFlag[itemcount], SENDMODE);
        }

        else if (*ptr == 'V')
        {
            ptr++;

            if (*ptr != ',')
                return 1; // 检查分隔符
            ptr++;

            char temp_title[128] = {0}; // 临时存储读取的字符串
            sscanf(ptr, "%127[^,]", temp_title);
            ptr += strlen(temp_title) + 1; // 移动指针到分号后

            // 根据实际长度动态分配内存
            const size_t title_len = strlen(temp_title);
            char *const title = new char[title_len + 1]; // 堆内存分配
            strcpy(title, temp_title);                   // 复制内容

            char sendmode[4] = {0}; // 临时存储读取的字符串
            sscanf(ptr, "%3[^,]", sendmode);
            ptr += strlen(sendmode) + 1; // 移动指针到分号后

            SendMode_e SENDMODE = NoSend;
            if (strcmp(sendmode, "NS") == 0)
                SENDMODE = NoSend;
            else if (strcmp(sendmode, "ST") == 0)
                SENDMODE = Send_Title;
            else if (strcmp(sendmode, "SV") == 0)
                SENDMODE = Send_Value;
            else if (strcmp(sendmode, "STV") == 0)
                SENDMODE = Send_Title_Value;
            else if (strcmp(sendmode, "RV") == 0)
                SENDMODE = RealtimeSend_Value;
            else if (strcmp(sendmode, "RTV") == 0)
                SENDMODE = RealtimeSend_Title_Value;
            else if (strcmp(sendmode, "OT") == 0)
                SENDMODE = SendOption_Title;
            else if (strcmp(sendmode, "OV") == 0)
                SENDMODE = SendOption_Value;
            else if (strcmp(sendmode, "OTV") == 0)
                SENDMODE = SendOption_Title_Value;

            char Typemode[3] = {0}; // 临时存储读取的字符串
            sscanf(ptr, "%2[^,]", Typemode);
            ptr += strlen(Typemode) + 1; // 移动指针到分号后

            ValueType_e ValueType = INT_e;
            if (strcmp(Typemode, "I") == 0)
                ValueType = INT_e;
            else if (strcmp(Typemode, "UI") == 0)
                ValueType = UINT_e;
            else if (strcmp(Typemode, "F") == 0)
                ValueType = FLOAT_e;
            else if (strcmp(Typemode, "UF") == 0)
                ValueType = UFLOAT_e;

            double item_value = 0.0; // 声明 double 变量
            int num_read = 0;        // 用于记录读取的字符数

            // 从 ptr 中提取数值（假设数值后跟分号 `;`）
            sscanf(ptr, "%lf;%n", &item_value, &num_read);

            ptr += num_read; // 移动指针跳过分号

            itemcount++;
            ItemValue[itemcount] = item_value;
            UIAddItem(&Page[currentPage], &Item[itemcount], title, ITEM_CHANGE_VALUE, &ItemValue[itemcount], ValueType, SENDMODE);
        }

        else if (*ptr == 'C')
        {
            ptr++;

            if (*ptr != ',')
                return 1; // 检查分隔符
            ptr++;

            char temp_title[128] = {0}; // 临时存储读取的字符串
            sscanf(ptr, "%127[^,]", temp_title);
            ptr += strlen(temp_title) + 1; // 移动指针到分号后

            // 根据实际长度动态分配内存
            const size_t title_len = strlen(temp_title);
            char *const title = new char[title_len + 1]; // 堆内存分配
            strcpy(title, temp_title);                   // 复制内容

            char sendmode[4] = {0}; // 临时存储读取的字符串
            sscanf(ptr, "%3[^,]", sendmode);
            ptr += strlen(sendmode) + 1; // 移动指针到分号后

            SendMode_e SENDMODE = NoSend;
            if (strcmp(sendmode, "NS") == 0)
                SENDMODE = NoSend;
            else if (strcmp(sendmode, "ST") == 0)
                SENDMODE = Send_Title;
            else if (strcmp(sendmode, "SV") == 0)
                SENDMODE = Send_Value;
            else if (strcmp(sendmode, "STV") == 0)
                SENDMODE = Send_Title_Value;
            else if (strcmp(sendmode, "RV") == 0)
                SENDMODE = RealtimeSend_Value;
            else if (strcmp(sendmode, "RTV") == 0)
                SENDMODE = RealtimeSend_Title_Value;
            else if (strcmp(sendmode, "OT") == 0)
                SENDMODE = SendOption_Title;
            else if (strcmp(sendmode, "OV") == 0)
                SENDMODE = SendOption_Value;
            else if (strcmp(sendmode, "OTV") == 0)
                SENDMODE = SendOption_Title_Value;

            bool item_flag = 0; // 声明 double 变量
            int num_read = 0;   // 用于记录读取的字符数

            // 从 ptr 中提取数值（假设数值后跟分号 `;`）
            sscanf(ptr, "%d;%n", &item_flag, &num_read);

            ptr += num_read; // 移动指针跳过分号

            itemcount++;
            ItemFlag[itemcount] = item_flag;
            UIAddItem(&Page[currentPage], &Item[itemcount], title, ITEM_CHECKBOX, &ItemFlag[itemcount], SENDMODE);
        }

        else if (*ptr == 'R')
        {
            ptr++;

            if (*ptr != ',')
                return 1; // 检查分隔符
            ptr++;

            char temp_title[128] = {0}; // 临时存储读取的字符串
            sscanf(ptr, "%127[^,]", temp_title);
            ptr += strlen(temp_title) + 1; // 移动指针到分号后

            // 根据实际长度动态分配内存
            const size_t title_len = strlen(temp_title);
            char *const title = new char[title_len + 1]; // 堆内存分配
            strcpy(title, temp_title);                   // 复制内容

            char sendmode[4] = {0}; // 临时存储读取的字符串
            sscanf(ptr, "%3[^,]", sendmode);
            ptr += strlen(sendmode) + 1; // 移动指针到分号后

            SendMode_e SENDMODE = NoSend;
            if (strcmp(sendmode, "NS") == 0)
                SENDMODE = NoSend;
            else if (strcmp(sendmode, "ST") == 0)
                SENDMODE = Send_Title;
            else if (strcmp(sendmode, "SV") == 0)
                SENDMODE = Send_Value;
            else if (strcmp(sendmode, "STV") == 0)
                SENDMODE = Send_Title_Value;
            else if (strcmp(sendmode, "RV") == 0)
                SENDMODE = RealtimeSend_Value;
            else if (strcmp(sendmode, "RTV") == 0)
                SENDMODE = RealtimeSend_Title_Value;
            else if (strcmp(sendmode, "OT") == 0)
                SENDMODE = SendOption_Title;
            else if (strcmp(sendmode, "OV") == 0)
                SENDMODE = SendOption_Value;
            else if (strcmp(sendmode, "OTV") == 0)
                SENDMODE = SendOption_Title_Value;

            bool item_flag = 0; // 声明 double 变量
            int num_read = 0;   // 用于记录读取的字符数

            // 从 ptr 中提取数值（假设数值后跟分号 `;`）
            sscanf(ptr, "%d;%n", &item_flag, &num_read);

            ptr += num_read; // 移动指针跳过分号

            itemcount++;
            ItemFlag[itemcount] = item_flag;
            UIAddItem(&Page[currentPage], &Item[itemcount], title, ITEM_RADIO_BUTTON, &ItemFlag[itemcount], SENDMODE);
        }

        else if (*ptr == 'M')
        {
            ptr++;

            if (*ptr != ',')
                return 1; // 检查分隔符
            ptr++;

            char temp_title[128] = {0}; // 临时存储读取的字符串
            sscanf(ptr, "%127[^,]", temp_title);
            ptr += strlen(temp_title) + 1; // 移动指针到分号后

            // 根据实际长度动态分配内存
            const size_t title_len = strlen(temp_title);
            char *const title = new char[title_len + 1]; // 堆内存分配
            strcpy(title, temp_title);                   // 复制内容

            char sendmode[4] = {0}; // 临时存储读取的字符串
            sscanf(ptr, "%3[^;]", sendmode);
            ptr += strlen(sendmode) + 1; // 移动指针到分号后

            SendMode_e SENDMODE = NoSend;
            if (strcmp(sendmode, "NS") == 0)
                SENDMODE = NoSend;
            else if (strcmp(sendmode, "ST") == 0)
                SENDMODE = Send_Title;
            else if (strcmp(sendmode, "SV") == 0)
                SENDMODE = Send_Value;
            else if (strcmp(sendmode, "STV") == 0)
                SENDMODE = Send_Title_Value;
            else if (strcmp(sendmode, "RV") == 0)
                SENDMODE = RealtimeSend_Value;
            else if (strcmp(sendmode, "RTV") == 0)
                SENDMODE = RealtimeSend_Title_Value;
            else if (strcmp(sendmode, "OT") == 0)
                SENDMODE = SendOption_Title;
            else if (strcmp(sendmode, "OV") == 0)
                SENDMODE = SendOption_Value;
            else if (strcmp(sendmode, "OTV") == 0)
                SENDMODE = SendOption_Title_Value;

            itemcount++;
            UIAddItem(&Page[currentPage], &Item[itemcount], title, ITEM_SEND, SENDMODE);
        }

        else if (*ptr == 'X')
        {
            char TempMenuSettings_InConfig[CONFIG_SETTINGS_MAX_LEN] = {0};
            sscanf(ptr, "%32[^;]", TempMenuSettings_InConfig);
            ptr += strlen(TempMenuSettings_InConfig) + 1; // 移动指针到分号后
        }

        else
        {
            // ptr++;
            return 1;
        }
    }
    return 0;
}

EasyUIPage_t pageMenu_Setting, pageTools, pageTheme, pageConnectionMode, pageAnimations, pageFont; // Tools下个大版本更新

EasyUIItem_t item_menusetting_sercption;

EasyUIItem_t itemjumptoMenu_Setting, itemUpdate, itemAbout, itemSponsor;

EasyUIItem_t itemmenuseting_loadfromUSB;

EasyUIItem_t itemjumptotheme, itemmenusetting_Dark, itemmenusetting_Light;

EasyUIItem_t itemjumptoanimations, itemmenusetting_ani_no, itemmenusetting_ani_fast, itemmenusetting_ani_normal, itemmenusetting_ani_slow;

EasyUIItem_t itemjunptoconnectionmode, itemmenuserring_sendto, itemmenusetting_USB, itemmenuseting_UARTA, itemmenuseting_UARTB;

EasyUIItem_t itemtobuadrate, itemmenusetting_host, itemmenusetting_9600, itemmenusetting_19200, itemmenusetting_38400, itemmenusetting_57600, itemmenusetting_115200;

EasyUIItem_t itemLEDswitch, itemBUZswitch, itemsavesettings, itemtestpage, iteminformation, itemupdate, itemsponser;

void menuAddsettings()
{

    UIAddPage(&pageMenu_Setting, PAGE_LIST);
    UIAddPage(&pageTheme, PAGE_LIST);
    UIAddPage(&pageFont, PAGE_LIST);
    UIAddPage(&pageAnimations, PAGE_LIST);
    UIAddPage(&pageConnectionMode, PAGE_LIST);

    UIAddItem(&Page[0], &itemjumptoMenu_Setting, "设置", ITEM_JUMP_PAGE, pageMenu_Setting.id);
    UIAddItem(&pageMenu_Setting, &item_menusetting_sercption, "[设置]", ITEM_PAGE_DESCRIPTION);

    // UIAddItem(&pageMenu_Setting, &itemmenuseting_loadfromUSB, "Load Config", ITEM_INFOMATION, UILoadConfigUSB);

    UIAddItem(&pageMenu_Setting, &itemjunptoconnectionmode, "参数发送目标", ITEM_JUMP_PAGE, pageConnectionMode.id);
    UIAddItem(&pageConnectionMode, &itemmenuserring_sendto, "参数发送至:", ITEM_PAGE_DESCRIPTION);
    UIAddItem(&pageConnectionMode, &itemmenusetting_USB, "USB(CDC)", ITEM_CHECKBOX, &menusetting_USB, SendtoSettings);

    UIAddItem(&pageConnectionMode, &itemmenuseting_UARTA, "TA(UART)", ITEM_CHECKBOX, &menusetting_UARTA, SendtoSettings);
    UIAddItem(&pageConnectionMode, &itemmenuseting_UARTB, "TB(UART)", ITEM_CHECKBOX, &menusetting_UARTB, SendtoSettings);
    UIAddItem(&pageConnectionMode, &itemtobuadrate, "UART配置", ITEM_EVENT, uartconfig_fromUI);

    UIAddItem(&pageMenu_Setting, &itemjumptotheme, "显示主题", ITEM_JUMP_PAGE, pageTheme.id);
    UIAddItem(&pageTheme, &itemmenusetting_Dark, "深色主题", ITEM_RADIO_BUTTON, &menusetting_Dark, SendtoSettings);
    UIAddItem(&pageTheme, &itemmenusetting_Light, "浅色主题", ITEM_RADIO_BUTTON, &menusetting_Light, SendtoSettings);

    UIAddItem(&pageMenu_Setting, &itemjumptoanimations, "动画速度", ITEM_JUMP_PAGE, pageAnimations.id);
    UIAddItem(&pageAnimations, &itemmenusetting_ani_no, "关闭动画", ITEM_RADIO_BUTTON, &menusetting_ani_no, SendtoSettings);
    UIAddItem(&pageAnimations, &itemmenusetting_ani_fast, "快速动画", ITEM_RADIO_BUTTON, &menusetting_ani_fast, SendtoSettings);
    UIAddItem(&pageAnimations, &itemmenusetting_ani_normal, "标准动画", ITEM_RADIO_BUTTON, &menusetting_ani_normal, SendtoSettings);
    UIAddItem(&pageAnimations, &itemmenusetting_ani_slow, "缓慢动画", ITEM_RADIO_BUTTON, &menusetting_ani_slow, SendtoSettings);

    // v2.0 取消LED蜂鸣器
    // UIAddItem(&pageMenu_Setting, &itemLEDswitch, "LED", ITEM_SWITCH ,&menusetting_LED,SendtoSettings);
    // UIAddItem(&pageMenu_Setting, &itemBUZswitch, "Buzzer", ITEM_SWITCH ,&menusetting_BUZ,SendtoSettings);

    // UIAddItem(&pageMenu_Setting, &itemtestpage, "按键测试", ITEM_EVENT, testpage);

    UIAddItem(&pageMenu_Setting, &itemsavesettings, "保存设置", ITEM_SEND, Send_SaveSettings);

    UIAddItem(&pageMenu_Setting, &iteminformation, "关于", ITEM_INFOMATION, UISgateinformation);
    // UIAddItem(&pageMenu_Setting, &itemUpdate, "更新", ITEM_INFOMATION, UIUpdateInformation);
    // UIAddItem(&pageMenu_Setting, &itemSponsor, "致谢", ITEM_INFOMATION, UIUpdateSponser);
}

void MenuInit()
{

    // 重新初始化usb

    char configsettings[CONFIG_SETTINGS_MAX_LEN] = ""; // Baud Rate //Connect Mode // Theme // Font // Animations // LED // BUZ

    // allocate a global buffer for load operation to avoid large BSS allocation
    current_configdata = (char *)malloc(CONFIG_DATA_MAX_LEN);
    if (current_configdata)
    {
        memset(current_configdata, 0, CONFIG_DATA_MAX_LEN);
    }

    char *configdata = (char *)malloc(CONFIG_DATA_MAX_LEN);

    // 从 Flash 加载数据到 SRAM
    load_configsettings_to_sram(configsettings);
    load_configdata_to_sram(configdata);

    // sleep_ms(1000);
    // 打印两组数据 ///////////////////////////////////////////////////////////////////////////////
    // printf("Config Settings: %s\n", configsettings);
    // printf("Config Data: %s\n", configdata);

    // 一用就卡死，怀疑是内存问题，暂时注释
    // 复制到current_configsettings
    // extern char current_configsettings[CONFIG_SETTINGS_MAX_LEN];
    // strncpy(current_configsettings, configsettings, CONFIG_DATA_MAX_LEN - 1);

    // 应用flash里的数据
    ParseConfig(configdata);

    free(configdata);
    
    menuAddsettings();
    changesettingsfromconfig(configsettings);
}


extern size_t term_index;        // 终止符匹配进度
extern size_t data_index;        // 数据存储位置
extern char *current_configdata; // 当前配置数据

void MenuReset()
{
    
    // 重置状态
    layer = 0;
    memset(pageIndex, 0, sizeof(pageIndex));
    memset(itemIndex, 0, sizeof(itemIndex));

    // 清空 Page, Item 链表头尾
    pageHead = NULL;
    pageTail = NULL;

    // 清空 title_to_id 映射
    title_to_id.clear();

    // 清空数据结构
    memset(Page, 0, sizeof(Page));
    memset(ifpagehadJump, 0, sizeof(ifpagehadJump));
    memset(Item, 0, sizeof(Item));
    memset(ItemValue, 0, sizeof(ItemValue));
    memset(ItemFlag, 0, sizeof(ItemFlag));

    // 重新初始化
    MenuInit();

    //
    term_index = 0;                   // 终止符匹配进度
    data_index = 0;                   // 数据存储位置
    strcpy(current_configdata, ";;"); // 首位安全设定

    // 先重置 core1（确保它处于清理状态），然后重新启动
    multicore_reset_core1();
    multicore_launch_core1(core1_main);
}

#endif
