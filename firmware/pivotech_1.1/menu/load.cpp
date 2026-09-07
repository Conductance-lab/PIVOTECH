#ifndef _LOAD_CPP_
#define _LOAD_CPP_

#include "CONFIG_FLO.hpp"
#include "menu/flash.hpp"
#include "menu/core.hpp"
// #include "menu/UI.hpp"
#include "menu/StateLED.hpp"
#include "menu/SGateSetting.hpp"

extern bool if_receive_massage;
extern char current_configdata[CONFIG_DATA_MAX_LEN];

bool loadrequset(bool source_1UART_0USB)
{



    // load_mode_1UART_0USB = source_1UART_0USB;
    // core 收到标志位后进入读取状态

    if (!if_receive_massage)
    { // core 1未收到指定内容
        return 0;
    }

    // 找到X配置项 并存储到flash里
    const char *ptr = current_configdata;
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
            else if (*ptr == '\0')
            {
            };
            continue;
        }

        // 解析指令
        else if (*ptr == '$')
        {
            ptr++;
            while (*ptr != '\0' && *ptr != ';')
            {
                ptr++;
            };
            ptr++;
        }

        else if (*ptr == 'D')
        {
            ptr++;
            while (*ptr != '\0' && *ptr != ';')
            {
                ptr++;
            };
            ptr++;
        }

        else if (*ptr == 'J')
        {
            ptr++;
            while (*ptr != '\0' && *ptr != ';')
            {
                ptr++;
            };
            ptr++;
        }
        else if (*ptr == 'S')
        {
            ptr++;
            while (*ptr != '\0' && *ptr != ';')
            {
                ptr++;
            };
            ptr++;
        }

        else if (*ptr == 'V')
        {
            ptr++;
            while (*ptr != '\0' && *ptr != ';')
            {
                ptr++;
            };
            ptr++;
        }

        else if (*ptr == 'C')
        {
            ptr++;
            while (*ptr != '\0' && *ptr != ';')
            {
                ptr++;
            };
            ptr++;
        }

        else if (*ptr == 'R')
        {
            ptr++;
            while (*ptr != '\0' && *ptr != ';')
            {
                ptr++;
            };
            ptr++;
        }

        else if (*ptr == 'M')
        {
            ptr++;
            while (*ptr != '\0' && *ptr != ';')
            {
                ptr++;
            };
            ptr++;
        }

        else if (*ptr == 'X')
        {
            char TempMenuSettings_InConfig[CONFIG_SETTINGS_MAX_LEN] = {0};
            sscanf(ptr, "%32[^;]", TempMenuSettings_InConfig);
            char saved_configsettings[CONFIG_SETTINGS_MAX_LEN];
            load_configsettings_to_sram(saved_configsettings);
            if (configsettingcmp(saved_configsettings, TempMenuSettings_InConfig, CONFIG_SETTINGS_MAX_LEN) != 0)
            {
                Open_StateLED();
                save_configsettings(TempMenuSettings_InConfig);
            }
            // printf("1:%s\r\n", saved_configsettings);///////////////////////////////
            // printf("2:%s\r\n", TempMenuSettings_InConfig);///////////////////////////////
            break;
        }
        else
        {
            ptr++;
        }
    }

    save_configdata(current_configdata);
    if_receive_massage = 0;
    // printf("3:%s\r\n", current_configdata);////////////////////////////////

    Open_StateLED();
    MenuConfigInit(); // 恢复连接驱动
    return 1;         // 1表示读取成功
}

#endif