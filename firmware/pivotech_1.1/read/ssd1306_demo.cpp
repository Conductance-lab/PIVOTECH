#ifndef _SSD1306_DEMO_CPP_
#define _SSD1306_DEMO_CPP_

#include "pico/stdlib.h"
#include "menu/ssd1306.hpp"
#include "menu/UI.hpp"
#include "menu/key.hpp"
#include "menu/core.hpp"
#include "menu/StateLED.hpp"
#include "CONFIG_FLO.hpp"
#include "read/hardware_core1.hpp"

#include <cstdio>
#include <cstring>

static const int ssd_demo_i2c_values[] = {100000, 400000, 1000000, 2000000};
static const char *ssd_demo_i2c_names[] = {"100KHz", "400KHz", "1MHz", "2MHz"};

int ssd_demo_i2c_sel = 1;
static int ssd_demo_i2c_backup = 1;
static bool ssd_demo_editing_freq = false;
static bool ssd_demo_infunction = false;
static bool ssd_demo_outfunction = false;

static int ssd_demo_highlight_y = 17;

extern bool device_init_success;
extern bool device_error_occurred;

extern void handle_pc_interface_commands();

enum SsdDemoStatus
{
    SSD_DEMO_STATUS_NORMAL = 0,
    SSD_DEMO_STATUS_NO_DEVICE,
    SSD_DEMO_STATUS_ERROR,
    SSD_DEMO_STATUS_TESTING
};

static SsdDemoStatus ssd_demo_get_status(int32_t rate)
{
    if (device_error_occurred || (device_init_success && rate == 0))
        return SSD_DEMO_STATUS_ERROR;
    if (device_init_success && rate == -1)
        return SSD_DEMO_STATUS_TESTING;
    if (!device_init_success)
        return SSD_DEMO_STATUS_NO_DEVICE;
    return SSD_DEMO_STATUS_NORMAL;
}

static void ssd_demo_sync_freq_from_global()
{
    int sel = 1;
    int val = (int)i2c_freq;
    for (int i = 0; i < 4; i++)
    {
        if (ssd_demo_i2c_values[i] >= val)
        {
            sel = i;
            break;
        }
    }
    ssd_demo_i2c_sel = sel;
}

static void ssd_demo_draw_main()
{
    DrawRectangle(buf, 0, 0, SSD1306_WIDTH, SSD1306_HEIGHT, 1, 0);

    DrawLine(buf, 0, 15, 127, 15, 1);
    DrawLine(buf, 0, 31, 127, 31, 1);

    Paint_DrawString_EN_CenterAtX(buf, 64, 3, (char *)"SSD1306/1315检测", &Font12, 1);

    char freq_line[20];
    snprintf(freq_line, sizeof(freq_line), "I2C:%s", ssd_demo_i2c_names[ssd_demo_i2c_sel]);
    Paint_DrawString_EN_CenterAtX(buf, 64, 19, freq_line, &Font12, 1);

    int32_t rate = get_screen_refresh_rate();
    SsdDemoStatus status = ssd_demo_get_status(rate);
    if (status == SSD_DEMO_STATUS_NORMAL)
    {
        DrawLine(buf, 0, 47, 127, 47, 1);

        char fps_line[20];
        snprintf(fps_line, sizeof(fps_line), "FPS:%ld", (long)rate);
        Paint_DrawString_EN_CenterAtX(buf, 64, 35, fps_line, &Font12, 1);
        Paint_DrawString_EN_CenterAtX(buf, 64, 51, (char *)"已接SSD1306 ->", &Font12, 1);
    }
    else
    {
        const char *msg_top = "无0x3C设备";
        const char *msg_bottom = "请检查设备";
        if (status == SSD_DEMO_STATUS_ERROR)
        {
            msg_top = "设备错误";
            msg_bottom = "请重新连接";
        }
        else if (status == SSD_DEMO_STATUS_TESTING)
        {
            msg_top = "测试中";
            msg_bottom = "初始化中";
        }

        Paint_DrawString_EN_CenterAtX(buf, 64, 38, (char *)msg_top, &Font12, 1);
        Paint_DrawString_EN_CenterAtX(buf, 64, 52, (char *)msg_bottom, &Font8, 1);
    }
}

static void ssd_demo_draw_freq_selector()
{
    int w = 84;
    int h = 30;
    int x = (SCREEN_WIDTH - w) / 2;
    int y = (SCREEN_HEIGHT - h) / 2;

    UIDisplayStr_font12(x + 3, y + 4, (char *)"I2C频率:", 12, 0);
    UIDisplayStr_font12(x + 3, y + 18, (char *)ssd_demo_i2c_names[ssd_demo_i2c_sel], 10, 0);
}

static void ssd_demo_render_animated()
{
    int xEnd = 1;
    int yEnd = ssd_demo_highlight_y;
    int widthEnd = 126;
    int heighEnd = 12;

    if (ssd_demo_editing_freq)
    {
        widthEnd = 84;
        heighEnd = 30;
        xEnd = (SCREEN_WIDTH - widthEnd) / 2;
        yEnd = (SCREEN_HEIGHT - heighEnd) / 2;
    }

    bool if_no_animation = (xEnd == xStart && yEnd == yStart && widthEnd == lengthStart && heighEnd == heightStart);
    if (!if_no_animation && footlength != 1)
    {
        for (float i = 0; i <= 1; i += footlength)
        {
            int x = easeInOutQuad(xStart, xEnd, i);
            int y = easeInOutQuad(yStart, yEnd, i);
            int width = easeInOutQuad(lengthStart, widthEnd, i);
            int heigh = easeInOutQuad(heightStart, heighEnd, i);

            ssd_demo_draw_main();

            if (ssd_demo_editing_freq || ssd_demo_infunction)
            {
                if (ssd_demo_infunction)
                    ApplyBlurEffect(buf, X2line_up(0, 1, i));
                else
                    ApplyBlurEffect(buf, 1);
                UIDrawSgate(x, y, width, heigh, 8, 6, 1, 2);
            }
            else if (ssd_demo_outfunction)
            {
                ApplyBlurEffect(buf, X2line_down(0.8, 0, i));
                UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);
            }
            else
            {
                UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);
            }

            if (ssd_demo_editing_freq)
                ssd_demo_draw_freq_selector();

            render(buf, &frame_area);
            memset(buf, 0, SSD1306_BUF_LEN);
        }
    }

    lengthStart = widthEnd;
    heightStart = heighEnd;
    yStart = yEnd;
    xStart = xEnd;
    ssd_demo_infunction = false;
    ssd_demo_outfunction = false;

    ssd_demo_draw_main();

    if (ssd_demo_editing_freq)
    {
        ApplyBlurEffect(buf, 1);
        UIDrawSgate((SCREEN_WIDTH - 84) / 2, (SCREEN_HEIGHT - 30) / 2, 84, 30, 8, 6, 1, 2);
        ssd_demo_draw_freq_selector();
    }
    else
    {
        UIDrawSgate(1, ssd_demo_highlight_y, 126, 12, 8, 6, 1, 1);
    }

    render(buf, &frame_area);
    memset(buf, 0, SSD1306_BUF_LEN);
}

extern "C" void ssd1306_demo_main(void)
{
    ResetOpn();
    ssd_demo_sync_freq_from_global();
    ssd_demo_highlight_y = 17;
    ssd_demo_infunction = false;
    ssd_demo_outfunction = false;

    start_core1_ssd1306_test(true);

    while (true)
    {
        extern bool opnPCchangemode;
        extern bool opnPCchangetool;
        if (opnPCchangetool || opnEnter || opnExit || opnUp || opnDown || opnLeft || opnRight ||opnPCchangemode)
        {
            if (ssd_demo_editing_freq)
            {
                if (opnDown)
                {
                    ssd_demo_i2c_sel = (ssd_demo_i2c_sel + 3) % 4;
                }
                else if (opnUp)
                {
                    ssd_demo_i2c_sel = (ssd_demo_i2c_sel + 1) % 4;
                }
                else if (opnEnter)
                {

                    i2c_freq = ssd_demo_i2c_values[ssd_demo_i2c_sel];
                    StartStateLED();
                    // stop_core1_test();
                    i2c_run_in_core1 = false; // 停止Core1的测试任务，准备重新启动以应用新频率
                    sleep_ms(100); //
                    i2c_run_in_core1 = true;  // 重新标记Core1的测试任务正在运行
                    start_core1_ssd1306_test(true);
                    
                    ssd_demo_editing_freq = false;
                    ssd_demo_outfunction = true;

                    // 主动发送当前页数据
                    extern int handle_pc_information();
                    extern bool send_pc_flag;
                    send_pc_flag = true; // 触发向上位机更新数据
                    handle_pc_information();
                }
                else if (opnExit)
                {
                    ssd_demo_i2c_sel = ssd_demo_i2c_backup;
                    ssd_demo_editing_freq = false;
                    ssd_demo_outfunction = true;
                }
            }
            else
            {
                if (opnEnter)
                {
                    ssd_demo_i2c_backup = ssd_demo_i2c_sel;
                    ssd_demo_editing_freq = true;
                    ssd_demo_infunction = true;

                    // 主动发送当前页数据
                    extern int handle_pc_information();
                    extern bool send_pc_flag;
                    send_pc_flag = true; // 触发向上位机更新数据
                    handle_pc_information();
                }
                
                else if (opnExit ||opnPCchangetool)
                {
                    stop_core1_test();
                    ResetOpn();
                    return;
                }
            }

            if (opnLeft || opnRight ||opnPCchangemode)
            {
                stop_core1_test();
                return;
            }

            ResetOpn();
        }

        ssd_demo_render_animated();

        // 200ms执行以下
        static uint32_t last_time = 0;
        uint32_t current_time = to_ms_since_boot(get_absolute_time());
        if (current_time - last_time >= 200)
        {
            last_time = current_time;
            // 主动发送当前页数据
            extern int handle_pc_information();
            extern bool send_pc_flag;
            send_pc_flag = true; // 触发向上位机更新数据
            handle_pc_information();
        }

        stdio_filter_driver(&stdio_usb);
        Key_TrySendSelectVoltage();
        SSD1306_PrintBufRaw();
        handle_pc_interface_commands();
        stdio_filter_driver(NULL);
    }
}

extern "C" int ssd1306_demo_get_status_code(void)
{
    int32_t rate = get_screen_refresh_rate();
    return (int)ssd_demo_get_status(rate);
}

extern "C" int ssd1306_demo_get_i2c_sel(void)
{
    return ssd_demo_i2c_sel;
}

#endif // _SSD1306_DEMO_CPP_
