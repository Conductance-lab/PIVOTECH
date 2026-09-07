#ifndef _MPU6050_DEMO_CPP_
#define _MPU6050_DEMO_CPP_

#include "pico/stdlib.h"
#include "menu/ssd1306.hpp"
#include "menu/UI.hpp"
#include "menu/key.hpp"
#include "menu/core.hpp"
#include "menu/StateLED.hpp"
#include "CONFIG_FLO.hpp"
#include "read/hardware_core1.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

// Axis mapping macros, change these definitions to tune orientation/order.
#define MPU_DEMO_PAIR0_H_AXIS 1
#define MPU_DEMO_PAIR0_V_AXIS 0
#define MPU_DEMO_PAIR0_H_SIGN 1
#define MPU_DEMO_PAIR0_V_SIGN 1

#define MPU_DEMO_PAIR1_H_AXIS 2
#define MPU_DEMO_PAIR1_V_AXIS 1
#define MPU_DEMO_PAIR1_H_SIGN -1
#define MPU_DEMO_PAIR1_V_SIGN 1

#define MPU_DEMO_PAIR2_H_AXIS 0
#define MPU_DEMO_PAIR2_V_AXIS 2
#define MPU_DEMO_PAIR2_H_SIGN -1
#define MPU_DEMO_PAIR2_V_SIGN 1

static const int mpu_demo_i2c_values[] = {100000, 400000, 1000000, 2000000};
static const char *mpu_demo_i2c_names[] = {"100kHz", "400kHz", "1MHz", "2MHz"};
static const char *mpu_demo_pair_names[] = {"X-Y", "Y-Z", "Z-X"};

static int mpu_demo_select = 0;
int mpu_demo_i2c_sel = 1;
static int mpu_demo_i2c_backup = 1;
static int mpu_demo_pair_sel = 1;

static absolute_time_t mpu_demo_last_t;
static bool mpu_demo_editing_freq = false;
static bool mpu_demo_infunction = false;
static bool mpu_demo_outfunction = false;

extern bool device_init_success;
extern bool device_error_occurred;

enum MpuDemoStatus
{
    MPU_DEMO_STATUS_NORMAL = 0,
    MPU_DEMO_STATUS_NO_DEVICE,
    MPU_DEMO_STATUS_ERROR,
    MPU_DEMO_STATUS_TESTING
};

static MpuDemoStatus mpu_demo_get_status(int32_t rate)
{
    if (device_error_occurred || (device_init_success && rate == 0))
        return MPU_DEMO_STATUS_ERROR;
    if (device_init_success && rate == -1)
        return MPU_DEMO_STATUS_TESTING;
    if (!device_init_success)
        return MPU_DEMO_STATUS_NO_DEVICE;
    return MPU_DEMO_STATUS_NORMAL;
}

static int mpu_demo_wrap_idx(int idx, int size)
{
    if (idx < 0)
        return size - 1;
    if (idx >= size)
        return 0;
    return idx;
}

static void mpu_demo_sync_i2c_from_global()
{
    int sel = 1;
    int val = (int)i2c_freq;
    for (int i = 0; i < 4; i++)
    {
        if (mpu_demo_i2c_values[i] >= val)
        {
            sel = i;
            break;
        }
    }
    mpu_demo_i2c_sel = sel;
}

static void mpu_demo_get_pair_map(int pair_idx, int *h_axis, int *v_axis, int *h_sign, int *v_sign)
{
    switch (pair_idx)
    {
    case 0:
        *h_axis = MPU_DEMO_PAIR0_H_AXIS;
        *v_axis = MPU_DEMO_PAIR0_V_AXIS;
        *h_sign = MPU_DEMO_PAIR0_H_SIGN;
        *v_sign = MPU_DEMO_PAIR0_V_SIGN;
        break;
    case 1:
        *h_axis = MPU_DEMO_PAIR1_H_AXIS;
        *v_axis = MPU_DEMO_PAIR1_V_AXIS;
        *h_sign = MPU_DEMO_PAIR1_H_SIGN;
        *v_sign = MPU_DEMO_PAIR1_V_SIGN;
        break;
    default:
        *h_axis = MPU_DEMO_PAIR2_H_AXIS;
        *v_axis = MPU_DEMO_PAIR2_V_AXIS;
        *h_sign = MPU_DEMO_PAIR2_H_SIGN;
        *v_sign = MPU_DEMO_PAIR2_V_SIGN;
        break;
    }
}

static float mpu_demo_axis_tilt_deg(int axis, float ax, float ay, float az)
{
    const float rad_to_deg = 57.2957795f;
    if (axis == 0)
        return atan2f(ax, sqrtf(ay * ay + az * az)) * rad_to_deg;
    if (axis == 1)
        return atan2f(ay, sqrtf(ax * ax + az * az)) * rad_to_deg;
    return atan2f(az, sqrtf(ax * ax + ay * ay)) * rad_to_deg;
}

static void mpu_demo_draw_level(MpuDemoStatus mpustatus)
{
    const int x0 = 0;
    const int y0 = 0;
    const int w = 64;
    const int h = 63;

    DrawRectangle(buf, x0, y0, w, h, 1, 0);
    UIDrawSgate(x0, y0, w, h, 6, 4, 1, 1);

    if (mpustatus != MPU_DEMO_STATUS_NORMAL)
    {
        const char *msg = "无0x68设备";
        if (mpustatus == MPU_DEMO_STATUS_ERROR)
            msg = "设备错误";
        else if (mpustatus == MPU_DEMO_STATUS_TESTING)
            msg = "测试中";

        Paint_DrawString_EN_CenterAtX(buf, 35, 27, (char *)msg, &Font12, 1);
        return;
    }

    struct MPU6050_Data *data = get_mpu6050_data();
    if (data == nullptr)
    {
        Paint_DrawString_EN_CenterAtX(buf, 35, 27, (char *)"无0x68设备", &Font12, 1);
        return;
    }

    absolute_time_t now_t = get_absolute_time();
    float dt = absolute_time_diff_us(mpu_demo_last_t, now_t) / 1000000.0f;
    if (dt < 0.001f)
        dt = 0.001f;
    if (dt > 0.05f)
        dt = 0.05f;
    mpu_demo_last_t = now_t;

    float acc[3] = {data->accel_x_g, data->accel_y_g, data->accel_z_g};
    float gyr[3] = {data->gyro_x_dps, data->gyro_y_dps, data->gyro_z_dps};

    int h_axis, v_axis, h_sign, v_sign;
    mpu_demo_get_pair_map(mpu_demo_pair_sel, &h_axis, &v_axis, &h_sign, &v_sign);

    float accel_h = mpu_demo_axis_tilt_deg(h_axis, acc[0], acc[1], acc[2]);
    float accel_v = mpu_demo_axis_tilt_deg(v_axis, acc[0], acc[1], acc[2]);

    // Keep level display tied to current frame angle result (no positional accumulation).
    float angle_h = 0.96f * accel_h + 0.04f * (gyr[h_axis] * dt);
    float angle_v = 0.96f * accel_v + 0.04f * (gyr[v_axis] * dt);

    float mapped_h = angle_h * (float)h_sign;
    float mapped_v = angle_v * (float)v_sign;

    const float max_deg = 90.0f;
    if (mapped_h > max_deg)
        mapped_h = max_deg;
    if (mapped_h < -max_deg)
        mapped_h = -max_deg;
    if (mapped_v > max_deg)
        mapped_v = max_deg;
    if (mapped_v < -max_deg)
        mapped_v = -max_deg;

    int cx = x0 + w / 2;
    int cy = y0 + h / 2;
    const float scale_x = (float)(w / 2 - 5) / max_deg;
    const float scale_y = (float)(h / 2 - 5) / max_deg;
    int xr = (int)(mapped_h * scale_x);
    int yr = (int)(mapped_v * scale_y);

    int bx = cx + xr;
    int by = cy - yr;

    if (bx < x0 + 5)
        bx = x0 + 5;
    if (bx > x0 + w - 5)
        bx = x0 + w - 5;
    if (by < y0 + 5)
        by = y0 + 5;
    if (by > y0 + h - 5)
        by = y0 + h - 5;

    DrawEllipse(buf, bx, by, 4, 4, 1, 1);
}

static void mpu_demo_draw_freq_selector()
{
    int w = 84;
    int h = 30;
    int x = (SCREEN_WIDTH - w) / 2;
    int y = (SCREEN_HEIGHT - h) / 2;

    UIDisplayStr_font12(x + 3, y + 4, (char *)"I2C频率:", 12, 0);
    UIDisplayStr_font12(x + 3, y + 18, (char *)mpu_demo_i2c_names[mpu_demo_i2c_sel], 10, 0);
}

MpuDemoStatus mpustatus;

static void mpu_demo_draw_ui()
{
    DrawRectangle(buf, 0, 0, SSD1306_WIDTH, SSD1306_HEIGHT, 1, 0);

    int32_t rate = get_screen_refresh_rate();
    mpustatus = mpu_demo_get_status(rate);

    mpu_demo_draw_level(mpustatus);

    Paint_DrawString_EN_CenterAtX(buf, 96, 3, (char *)"MPU6050", &Font12, 1);
    Paint_DrawString_EN_CenterAtX(buf, 96, 16, (char *)"检测", &Font12, 1);

    char line3[18];
    snprintf(line3, sizeof(line3), "%s", mpu_demo_i2c_names[mpu_demo_i2c_sel]);
    Paint_DrawString_EN_CenterAtX(buf, 96, 35, line3, &Font12, 1);

    char line4[18];
    snprintf(line4, sizeof(line4), "%s", mpu_demo_pair_names[mpu_demo_pair_sel]);
    Paint_DrawString_EN_CenterAtX(buf, 96, 51, line4, &Font12, 1);
}

static void mpu_demo_render_animated()
{
    int xEnd = (mpu_demo_select == 0) ? 70 : 70;
    int yEnd = (mpu_demo_select == 0) ? 33 : 49;
    int widthEnd = 52;
    int heighEnd = 12;

    if (mpu_demo_editing_freq)
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

            mpu_demo_draw_ui();

            if (mpu_demo_editing_freq || mpu_demo_infunction)
            {
                if (mpu_demo_infunction)
                    ApplyBlurEffect(buf, X2line_up(0, 1, i));
                else
                    ApplyBlurEffect(buf, 1);
                UIDrawSgate(x, y, width, heigh, 8, 6, 1, 2);
            }
            else if (mpu_demo_outfunction)
            {
                ApplyBlurEffect(buf, X2line_down(0.8, 0, i));
                UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);
            }
            else
            {
                UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);
            }

            if (mpu_demo_editing_freq)
                mpu_demo_draw_freq_selector();

            render(buf, &frame_area);
            memset(buf, 0, SSD1306_BUF_LEN);
        }
    }

    lengthStart = widthEnd;
    heightStart = heighEnd;
    yStart = yEnd;
    xStart = xEnd;
    mpu_demo_infunction = false;
    mpu_demo_outfunction = false;

    mpu_demo_draw_ui();

    if (mpu_demo_editing_freq)
    {
        ApplyBlurEffect(buf, 1);
        UIDrawSgate((SCREEN_WIDTH - 84) / 2, (SCREEN_HEIGHT - 30) / 2, 84, 30, 8, 6, 1, 2);
        mpu_demo_draw_freq_selector();
    }
    else
    {
        UIDrawSgate(70, (mpu_demo_select == 0) ? 33 : 49, 52, 12, 8, 6, 1, 1);
    }

    render(buf, &frame_area);
    memset(buf, 0, SSD1306_BUF_LEN);
}

extern void handle_pc_interface_commands();

extern "C" void mpu6050_demo_main(void)
{
    ResetOpn();
    mpu_demo_sync_i2c_from_global();
    mpu_demo_select = 0;
    mpu_demo_pair_sel = 1;
    mpu_demo_editing_freq = false;
    mpu_demo_infunction = false;
    mpu_demo_outfunction = false;
    mpu_demo_last_t = get_absolute_time();

    start_core1_mpu6050_test(true);

    while (true)
    {
        extern bool opnPCchangemode;
        extern bool opnPCchangetool;
        if (opnPCchangetool || opnEnter || opnExit || opnUp || opnDown || opnLeft || opnRight ||opnPCchangemode)
        {
            if (opnUp)
            {
                if (mpu_demo_editing_freq)
                    mpu_demo_i2c_sel = mpu_demo_wrap_idx(mpu_demo_i2c_sel + 1, 4);
                else
                    mpu_demo_select = mpu_demo_wrap_idx(mpu_demo_select - 1, 2);
            }
            else if (opnDown)
            {
                if (mpu_demo_editing_freq)
                    mpu_demo_i2c_sel = mpu_demo_wrap_idx(mpu_demo_i2c_sel - 1, 4);
                else
                    mpu_demo_select = mpu_demo_wrap_idx(mpu_demo_select + 1, 2);
            }
            else if (opnEnter)
            {
                extern bool send_pc_flag;
                send_pc_flag = true; // 触发向上位机更新数据
                if (mpu_demo_editing_freq)
                {
                    i2c_freq = mpu_demo_i2c_values[mpu_demo_i2c_sel];
                    StartStateLED();
                    i2c_run_in_core1 = false; // 停止Core1的测试任务，准备重新启动以应用新频率
                    sleep_ms(100); //
                    i2c_run_in_core1 = true;  // 重新标记Core1的测试任务正在运行
                    start_core1_mpu6050_test(true);

                    mpu_demo_editing_freq = false;
                    mpu_demo_outfunction = true;
                    mpu_demo_last_t = get_absolute_time();

                    // 主动发送当前页数据
                    extern int handle_pc_information();
                    extern bool send_pc_flag;
                    send_pc_flag = true; // 触发向上位机更新数据
                    handle_pc_information();
                }
                else
                {
                    if (mpu_demo_select == 0)
                    {
                        mpu_demo_i2c_backup = mpu_demo_i2c_sel;
                        mpu_demo_editing_freq = true;
                        mpu_demo_infunction = true;
                    }
                    else
                    {
                        mpu_demo_pair_sel = mpu_demo_wrap_idx(mpu_demo_pair_sel + 1, 3);
                        StartStateLED();
                    }
                }
            }
            else if (opnExit || opnPCchangetool)
            {
                if (mpu_demo_editing_freq)
                {
                    mpu_demo_i2c_sel = mpu_demo_i2c_backup;
                    mpu_demo_editing_freq = false;
                    mpu_demo_outfunction = true;
                    ResetOpn();
                    continue;
                }
                stop_core1_test();
                ResetOpn();
                return;
            }
            else if (opnLeft || opnRight ||opnPCchangemode)
            {
                stop_core1_test();
                return;
            }

            ResetOpn();
        }
        mpu_demo_render_animated();

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

extern "C" int mpu6050_demo_get_status_code(void)
{
    int rate = get_screen_refresh_rate();
    MpuDemoStatus status = mpu_demo_get_status(rate);
    return (int)status;
}

extern "C" int mpu6050_demo_get_i2c_sel(void)
{
    return mpu_demo_i2c_sel;
}

extern "C" int mpu6050_demo_get_pair_sel(void)
{
    return mpu_demo_pair_sel;
}

extern "C" void mpu6050_demo_set_pair_sel_value(int pair_sel)
{
    mpu_demo_pair_sel = mpu_demo_wrap_idx(pair_sel, 3);
}

#endif // _MPU6050_DEMO_CPP_
