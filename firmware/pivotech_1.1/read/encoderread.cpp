#ifndef _ENCODERREAD_CPP_
#define _ENCODERREAD_CPP_

#include "pico/stdlib.h"
#include "menu/ssd1306.hpp"
#include "menu/ssd1306.hpp"
#include "Fonts/fonts.h"
#include "menu/UI.hpp"
#include "menu/key.hpp"
#include "menu/Curve.hpp"
#include "menu/StateLED.hpp"
#include "read/adcread.hpp"
#include "CONFIG_FLO.hpp"
#include <cstring>
#include <cstdio>
#include "hardware/irq.h"
#include "hardware/gpio.h"

#define speed_group_num 100
#define update_gap_in_encoderread 200

int32_t count_encoder = 0;                        // 总count的判断
int32_t a_count_encoder = 0, b_count_encoder = 0; // 当前仅用于同频判断

bool if_aorb_is_wrong = 0;

int speed_encoder = 0;

// bool encoder_ta_update_flag = 0;
// bool encoder_tb_update_flag = 0;

uint32_t encoder_last_refresh_time = 0;
void encoder_global_isr(uint gpio, uint32_t events)
{
    uint32_t current_time = to_ms_since_boot(get_absolute_time());

    // 防止中断完全抢占cpu资源
    if (current_time - encoder_last_refresh_time >= update_gap_in_encoderread * 2)
    {
        const uint encoder_pins[] = {TAINPIN, TBINPIN};
        for (int i = 0; i < 2; ++i)
        {
            gpio_set_irq_enabled(encoder_pins[i], GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
            gpio_pull_up(encoder_pins[i]);
        }
        return;
    }

    // 编码器A的CHA处理
    if (gpio == TAINPIN)
    {
        // encoder_ta_update_flag = true;
        int b_state = gpio_get(TBINPIN);
        if (events & GPIO_IRQ_EDGE_RISE)
        {
            count_encoder += (b_state ? +1 : -1);
        }
        else
        {
            count_encoder += (b_state ? -1 : +1);
            a_count_encoder++;
        }
    }
    // 编码器A的CHB处理
    else if (gpio == TBINPIN)
    {
        // encoder_tb_update_flag = true;
        int a_state = gpio_get(TAINPIN);
        if (events & GPIO_IRQ_EDGE_RISE)
        {
            count_encoder += (a_state ? -1 : +1);
        }
        else
        {
            count_encoder += (a_state ? +1 : -1);
            b_count_encoder++;
        }
    }
}

int speed_data[5] = {0};
int32_t last_count_encoder = 0;
int32_t last_a_count_encoder = 0;
int32_t last_b_count_encoder = 0;
// 计算speed
void cal_encoder()
{

    static int valid_data_count = 0;

    // 计算当前原始速度
    int current_raw_speed = (count_encoder - last_count_encoder) * 1000 / update_gap_in_encoderread;
    last_count_encoder = count_encoder;

    // 持续0
    if (valid_data_count == 0 && current_raw_speed == 0)
    {
        speed_encoder = 0;
        return;
    }

    // 数据移位存储 (FIFO)
    for (int i = 4; i > 0; i--)
    {
        speed_data[i] = speed_data[i - 1];
    }
    speed_data[0] = current_raw_speed;

    // 记录有效数据个数 (0~5)
    if (valid_data_count < 5)
    {
        valid_data_count++;
    }

    // 统计有效数据中的 0 的个数并求和
    int zero_count = 0;
    long speed_sum = 0;
    for (int i = 0; i < valid_data_count; i++)
    {
        if (speed_data[i] == 0)
        {
            zero_count++;
        }
        speed_sum += speed_data[i];
    }

    // 若超过两个有效数据为 0，则清空记录和有效个数
    if (zero_count >= 2)
    {
        valid_data_count = 0;
        for (int i = 0; i < 5; i++)
        {
            speed_data[i] = 0;
        }
        speed_encoder = 0;
    }
    else
    {
        // 按照最多有效数据计算平均速度
        if (valid_data_count > 0)
        {
            speed_encoder = speed_sum / valid_data_count;
        }
        else
        {
            speed_encoder = 0;
        }
    }
    // 分别保存A、B通道的count值

    int ab_diff = (a_count_encoder - last_a_count_encoder) - (b_count_encoder - last_b_count_encoder);
    last_a_count_encoder = a_count_encoder;
    last_b_count_encoder = b_count_encoder;

    // 同频检查

    if (a_count_encoder + b_count_encoder > 5 &&
        ((float)(a_count_encoder - b_count_encoder) / (float)(a_count_encoder + b_count_encoder) > 0.02 || (float)(a_count_encoder - b_count_encoder) / (float)(a_count_encoder + b_count_encoder) < -0.02))
    {
        if_aorb_is_wrong = 1;
    }
    else
    {
        if_aorb_is_wrong = 0;
    }

    // printf("A_count: %ld, B_count: %ld, Diff: %d\n", a_count_encoder, b_count_encoder, ab_diff);
}

void encoder_init()
{

    // 重置复用引脚状态
    gpio_init(TEST_SPI_CS_PIN);
    gpio_set_dir(TEST_SPI_CS_PIN, GPIO_IN);
    gpio_disable_pulls(TEST_SPI_CS_PIN);

    // 初始化所有引脚
    const uint encoder_pins[] = {TAINPIN, TBINPIN};

    for (int i = 0; i < 2; i++)
    {
        gpio_init(encoder_pins[i]);
        gpio_set_dir(encoder_pins[i], GPIO_IN);

        gpio_set_irq_enabled_with_callback(encoder_pins[i],
                                           GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                           true,
                                           &encoder_global_isr);
        gpio_pull_up(encoder_pins[i]);
    }
}

void encoder_Deinit()
{
    // 初始化所有引脚
    const uint encoder_pins[] = {TAINPIN, TBINPIN};

    for (int i = 0; i < 2; i++)
    {
        gpio_set_irq_enabled_with_callback(encoder_pins[i],
                                           GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                           false, &encoder_global_isr);
    }
}

void draw_encoder_read()
{

    // 格式化 TA/TB 的显示内容
    char frec_a[16], frec_b[16];
    char speed[24], record[24];

    uint32_t current_time = to_ms_since_boot(get_absolute_time());

    if (current_time - encoder_last_refresh_time >= update_gap_in_encoderread)
    {
        encoder_last_refresh_time = current_time;
        cal_encoder();

        encoder_init();
    }

    snprintf(frec_a, sizeof(frec_a), "%ld p", a_count_encoder);
    snprintf(frec_b, sizeof(frec_b), "%ld p", b_count_encoder);
    if (if_aorb_is_wrong)
    {
        snprintf(speed, sizeof(speed), "A&B相位校验错误");
    }
    else
    {
        snprintf(speed, sizeof(speed), "%ld 脉冲/s", speed_encoder);
    }

    if (if_aorb_is_wrong)
    {
        snprintf(record, sizeof(record), "~%ld 脉冲", count_encoder);
    }
    else
    {
        snprintf(record, sizeof(record), "%ld 脉冲", count_encoder);
    }

    // 居中打印 TA/TB 的内容
    DrawRectangle(buf, 0, 0, 64, 16, 0, 1);
    DrawRectangle(buf, 64, 0, 64, 16, 0, 1);
    DrawLine(buf, 0, 31, 127, 31, 1);
    DrawLine(buf, 0, 47, 127, 47, 1);
    Paint_DrawString_EN_CenterAtX(buf, 32, 2, "TA", &Font16, 1);
    Paint_DrawString_EN_CenterAtX(buf, 96, 2, "TB", &Font16, 1);
    Paint_DrawString_EN_CenterAtX(buf, 32, 19, frec_a, &Font12, 1);
    Paint_DrawString_EN_CenterAtX(buf, 96, 19, frec_b, &Font12, 1);
    Paint_DrawString_EN_CenterAtX(buf, 64, 35, speed, &Font12, 1);
    Paint_DrawString_EN_CenterAtX(buf, 78, 51, record, &Font12, 1);

#include "read/adcread.hpp"
    testpin_state checkstate = checkpin();

    if (checkstate.ta_pinstate == PINSTATE_HIGH)
    {
        InvertRect(buf, 0, 0, 64, 16);
    }
    if (checkstate.tb_pinstate == PINSTATE_HIGH)
    {
        InvertRect(buf, 64, 0, 64, 16);
    }

    if (keyDown.isPressed)
    {
        DrawRectangle(buf, 1, 49, 31, 14, 1, 1);
        DrawRectangle(buf, 1, 49, 31, 14, 0, 0);
        Paint_DrawString_EN(buf, 3, 51, "复位", &Font12, 0);
    }
    else
    {
        DrawRectangle(buf, 1, 49, 31, 14, 1, 0);
        DrawRectangle(buf, 1, 49, 31, 14, 0, 1);
        Paint_DrawString_EN(buf, 3, 51, "复位", &Font12, 1);
    }
}

bool encoder_read()
{
    ResetOpn();

    int xEnd = -1;
    int yEnd = -1;
    int widthEnd = SSD1306_WIDTH + 2;
    int heighEND = SSD1306_HEIGHT + 2;
    if (footlength != 1)
    {
        for (float i = 0; i < 1; i += (footlength))
        {

            int x = X2line_up(xStart, xEnd, i);
            int y = X2line_up(yStart, yEnd, i);
            int width = X2line_up(lengthStart, widthEnd, i);
            int heigh = X2line_up(heightStart, heighEND, i);
            UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);
            draw_encoder_read();
            render(buf, &frame_area);
            memset(buf, 0, SSD1306_BUF_LEN);
        }
    }
    lengthStart = widthEnd;
    heightStart = heighEND;
    yStart = yEnd;
    xStart = xEnd;

    while (true)
    {
        extern bool opnPCchangemode;
        if (opnLeft || opnRight || opnPCchangemode)
        {
            // ResetOpn(); // 这里不需要在此处重置，放在function里
            return 0; // 退出，模式切换
        }
        extern bool opnPCchangetool;
        if (opnExit || opnPCchangetool)
        {
            return 1; // 返回重新选择模块
        }

        draw_encoder_read();
        render(buf, &frame_area);
        memset(buf, 0, SSD1306_BUF_LEN);

        if (opnDown)
        {
            int xEnd = 1;
            int yEnd = 50;
            int widthEnd = 24;
            int heighEND = 11;
            if (footlength != 1)
            {
                for (float i = 0; i < 1; i += (footlength))
                {

                    int x = easeInOutQuad(xStart, xEnd, i);
                    int y = easeInOutQuad(yStart, yEnd, i);
                    int width = easeInOutQuad(lengthStart, widthEnd, i);
                    int heigh = easeInOutQuad(heightStart, heighEND, i);
                    UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);
                    draw_encoder_read();
                    render(buf, &frame_area);
                    memset(buf, 0, SSD1306_BUF_LEN);
                }
            }

            while (keyDown.isPressed)
            {
                UIDrawSgate(xEnd, yEnd, widthEnd, heighEND, 8, 6, 1, 1);
                draw_encoder_read();
                render(buf, &frame_area);
                memset(buf, 0, SSD1306_BUF_LEN);
            }
            lengthStart = widthEnd;
            heightStart = heighEND;
            yStart = yEnd;
            xStart = xEnd;

            // clear values
            count_encoder = 0;
            a_count_encoder = 0;
            b_count_encoder = 0;
            if_aorb_is_wrong = 0;
            speed_encoder = 0;
            for (int i = 0; i < 5; i++)
            {
                speed_data[i] = 0;
            }
            last_count_encoder = 0;
            last_a_count_encoder = 0;
            last_b_count_encoder = 0;

            xEnd = -1;
            yEnd = -1;
            widthEnd = SSD1306_WIDTH + 2;
            heighEND = SSD1306_HEIGHT + 2;
            if (footlength != 1)
            {
                for (float i = 0; i < 1; i += (footlength))
                {

                    int x = X2line_up(xStart, xEnd, i);
                    int y = X2line_up(yStart, yEnd, i);
                    int width = X2line_up(lengthStart, widthEnd, i);
                    int heigh = X2line_up(heightStart, heighEND, i);
                    UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);
                    draw_encoder_read();
                    render(buf, &frame_area);
                    memset(buf, 0, SSD1306_BUF_LEN);
                }
            }
            lengthStart = widthEnd;
            heightStart = heighEND;
            yStart = yEnd;
            xStart = xEnd;
            ResetOpn();
        }
        if (opnUp || opnEnter)
        {

            if (footlength != 1)
            {
                for (float i = 0; i < 1; i += (footlength))
                {

                    int x = A_line(xStart, xEnd, 5, i);
                    int y = A_line(yStart, yEnd, 5, i);
                    int width = A_line(lengthStart, widthEnd, widthEnd - 10, i);
                    int heigh = A_line(heightStart, heighEND, heighEND - 10, i);

                    draw_encoder_read();
                    UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);
                    render(buf, &frame_area);
                    memset(buf, 0, SSD1306_BUF_LEN);
                }
            }
            lengthStart = widthEnd;
            heightStart = heighEND;
            yStart = yEnd;
            xStart = xEnd;

            ResetOpn();
        }

        extern bool send_pc_flag;
        send_pc_flag = true; // 触发向上位机更新数据
    }
}

#endif
