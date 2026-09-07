#ifndef _HARDWARE_CORE1_CPP_
#define _HARDWARE_CORE1_CPP_

/*
 * 独立的硬件测试系统 - Core1 专用
 *
 * 功能特点：
 * 1. 支持 I2C 和 SPI 两种通信模式
 * 2. 独立的引脚配置，避免与主系统冲突
 * 3. 可配置的通信频率
 * 4. 实时性能指标显示
 * 5. 3D/4D 动画演示
 *
 * 使用示例：
 * // 启动 I2C 模式测试
 * start_core1_ssd1306_test(TEST_MODE_I2C);
 *
 * // 启动 SPI 模式测试
 * start_core1_ssd1306_test(TEST_MODE_SPI);
 *
 * // 停止测试
 * stop_core1_ssd1306_test();
 */

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "../CONFIG_FLO.hpp"
#include "../Fonts/fonts.h"
#include "../menu/ssd1306.hpp"
#include "hardware_core1.hpp"
#include <cmath>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "menu/StateLED.hpp"

// ==================== 独立测试系统配置 ====================
// 测试系统专用工具宏
#define TEST_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// 测试系统专用 I2C 配置（避免与主系统冲突）

#define TEST_I2C_ADDR 0x3C

// 通信模式定义
#define TEST_MODE_I2C true
#define TEST_MODE_SPI false

// 测试系统专用 SSD1306 配置
#define TEST_SSD1306_WIDTH 128
#define TEST_SSD1306_HEIGHT 64
#define TEST_SSD1306_PAGE_HEIGHT 8
#define TEST_SSD1306_NUM_PAGES (TEST_SSD1306_HEIGHT / TEST_SSD1306_PAGE_HEIGHT)
#define TEST_SSD1306_BUF_LEN (TEST_SSD1306_NUM_PAGES * TEST_SSD1306_WIDTH)

// SSD1306 命令定义（测试系统专用前缀）
#define TEST_SSD1306_SET_MEM_MODE 0x20
#define TEST_SSD1306_SET_COL_ADDR 0x21
#define TEST_SSD1306_SET_PAGE_ADDR 0x22
#define TEST_SSD1306_SET_HORIZ_SCROLL 0x26
#define TEST_SSD1306_SET_SCROLL 0x2E
#define TEST_SSD1306_SET_DISP_START_LINE 0x40
#define TEST_SSD1306_SET_CONTRAST 0x81
#define TEST_SSD1306_SET_CHARGE_PUMP 0x8D
#define TEST_SSD1306_SET_SEG_REMAP 0xA0
#define TEST_SSD1306_SET_ENTIRE_ON 0xA4
#define TEST_SSD1306_SET_ALL_ON 0xA5
#define TEST_SSD1306_SET_NORM_DISP 0xA6
#define TEST_SSD1306_SET_INV_DISP 0xA7
#define TEST_SSD1306_SET_MUX_RATIO 0xA8
#define TEST_SSD1306_SET_DISP 0xAE
#define TEST_SSD1306_SET_COM_OUT_DIR 0xC0
#define TEST_SSD1306_SET_DISP_OFFSET 0xD3
#define TEST_SSD1306_SET_DISP_CLK_DIV 0xD5
#define TEST_SSD1306_SET_PRECHARGE 0xD9
#define TEST_SSD1306_SET_COM_PIN_CFG 0xDA
#define TEST_SSD1306_SET_VCOM_DESEL 0xDB

// 外部频率参数变量声明
int32_t i2c_freq = 400000;
int32_t spi_freq = 400000;

int i2c_result_return;

int i2c_result(int ret)
{
    i2c_result_return = ret;

    return i2c_result_return;
}

// 全局变量：屏幕刷新率（可以被外部访问）
int32_t screen_refresh_rate = 0;

// 全局变量：通信模式（true=I2C, false=SPI）
bool test_communication_mode = TEST_MODE_I2C;

// ==================== MPU6050配置和数据变量 ====================
#define TEST_MPU6050_ADDR 0x68
#define TEST_MPU6050_REG_PWR_MGMT_1 0x6B
#define TEST_MPU6050_REG_ACCEL_XOUT_H 0x3B
#define TEST_MPU6050_REG_GYRO_XOUT_H 0x43
#define TEST_MPU6050_REG_WHO_AM_I 0x75
#define TEST_MPU6050_REG_CONFIG 0x1A
#define TEST_MPU6050_REG_GYRO_CONFIG 0x1B
#define TEST_MPU6050_REG_ACCEL_CONFIG 0x1C

// MPU6050 SPI特有配置
// #define TEST_MPU6050_SPI_READ_FLAG 0x80
// #define TEST_MPU6050_SPI_WRITE_FLAG 0x00

// 全局MPU6050数据结构 (struct definition moved to header)
struct MPU6050_Data mpu6050_data = {0};

// 测试类型枚举
enum TestDevice
{
    TEST_DEVICE_SSD1306 = 0,
    TEST_DEVICE_MPU6050 = 1
};

// 当前测试设备
static enum TestDevice current_test_device = TEST_DEVICE_SSD1306;

// 独立的SSD1306测试缓存区（避免与主程序UI混合）
static uint8_t test_buf[TEST_SSD1306_BUF_LEN];
static struct test_render_area
{
    uint8_t start_col;
    uint8_t end_col;
    uint8_t start_page;
    uint8_t end_page;
    int buflen;
} test_frame_area = {
    .start_col = 0,
    .end_col = TEST_SSD1306_WIDTH - 1,
    .start_page = 0,
    .end_page = TEST_SSD1306_NUM_PAGES - 1};

// 动画状态变量
static float rotation_angle = 0.0f;
bool i2cspi_test_running = false;

bool i2c_run_in_core1 = false;

// 帧率统计变量
static uint32_t fps_counter = 0;
static uint32_t last_fps_time = 0;
// screen_refresh_rate 已经在上方声明为全局变量

#include "menu/core.hpp"
// 测试设备状态变量
bool device_init_success = true;
bool device_error_occurred = false;

// 颜色翻转变量
static bool invert_display = false;
static uint32_t last_invert_time = 0;

// 优化：信息叠加缓存（仅每秒重绘一次文字，其余帧直接叠加）
static uint8_t info_overlay_buf[TEST_SSD1306_WIDTH * 2] = {0}; // 顶部16像素区域，两页
static uint32_t last_info_overlay_update_ms = 0;
static char info_str_cache[32] = {0};

// 清空信息叠加缓存
static inline void clear_info_overlay()
{
    memset(info_overlay_buf, 0, sizeof(info_overlay_buf));
}

// 仅在需要时更新信息叠加内容（1s更新一次）
// 获取系统时间（毫秒）
uint32_t get_time_ms()
{
    return to_ms_since_boot(get_absolute_time());
}

static void update_info_overlay_if_needed()
{
    uint32_t now_ms = get_time_ms();
    if (now_ms - last_info_overlay_update_ms < 1000)
        return;

    last_info_overlay_update_ms = now_ms;
    clear_info_overlay();

    // 格式化信息字符串（沿用原逻辑）
    char freq_str[16];
    int32_t freq = test_communication_mode ? i2c_freq : spi_freq;
    const char *mode = test_communication_mode ? "I2C" : "SPI";
    float f = (float)freq;
    if (f >= 1000000.0f)
        snprintf(freq_str, sizeof(freq_str), "%.1fM", f / 1000000.0f);
    else if (f >= 10000.0f)
        snprintf(freq_str, sizeof(freq_str), "%.0fK", f / 1000.0f);
    else if (f >= 1000.0f)
        snprintf(freq_str, sizeof(freq_str), "%.1fK", f / 1000.0f);
    else
        snprintf(freq_str, sizeof(freq_str), "%ld", freq);

    if (screen_refresh_rate == -1)
    {
        snprintf(info_str_cache, sizeof(info_str_cache), "%s:%sHz ...", mode, freq_str);
    }
    else
    {
        snprintf(info_str_cache, sizeof(info_str_cache), "%s:%sHz %luFPS", mode, freq_str, screen_refresh_rate);
    }

    // 将文字渲染到叠加缓冲（顶端，留2px边距）
    Paint_DrawString_EN(info_overlay_buf, 2, 2, info_str_cache, &Font12, true);
}

// 将叠加缓冲快速合成到当前帧缓冲（OR叠加）
static inline void blit_info_overlay(uint8_t *dst)
{
    // 顶部两页（16px），逐列OR叠加
    const int width = TEST_SSD1306_WIDTH;
    const int page_bytes = width; // 每页字节数
    const uint8_t *src_page0 = info_overlay_buf + 0 * page_bytes;
    const uint8_t *src_page1 = info_overlay_buf + 1 * page_bytes;
    uint8_t *dst_page0 = dst + 0 * page_bytes;
    uint8_t *dst_page1 = dst + 1 * page_bytes;
    for (int x = 0; x < width; ++x)
    {
        dst_page0[x] |= src_page0[x];
        dst_page1[x] |= src_page1[x];
    }
}

// ==================== 测试系统专用引脚配置函数 ====================

extern void core1_i2c_monitor_append(const char *str);

// Bus Recovery for I2C
void i2c_bus_recovery(uint scl_pin, uint sda_pin)
{
    // 先将管脚复位为GPIO模式，开漏输出
    gpio_set_function(scl_pin, GPIO_FUNC_SIO);
    gpio_set_function(sda_pin, GPIO_FUNC_SIO);

    gpio_pull_up(scl_pin);
    gpio_pull_up(sda_pin);
    gpio_set_dir(scl_pin, GPIO_OUT);
    gpio_set_dir(sda_pin, GPIO_OUT);

    gpio_put(sda_pin, 1); // SDA高电平
    for (int i = 0; i < 9; ++i)
    {
        gpio_put(scl_pin, 0);
        sleep_us(5);
        gpio_put(scl_pin, 1);
        sleep_us(5);
    }
    // 发 STOP (SCL高时让SDA从低拉到高)
    gpio_put(sda_pin, 0);
    sleep_us(5);
    gpio_put(scl_pin, 1);
    sleep_us(5);
    gpio_put(sda_pin, 1);
    sleep_us(5);

    // 恢复默认上拉
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
}

// 测试系统专用：配置 I2C 引脚
void test_configure_i2c_pins(int32_t frequency, bool print)
{
    // 执行I2C Bus Recovery（用GPIO模拟9次TEST_I2C_SDA_PIN TEST_I2C_SCL_PIN恢复）
    //  i2c_bus_recovery(TEST_I2C_SCL_PIN, TEST_I2C_SDA_PIN);
    //  初始化 I2C 端口
    i2c_init(TEST_I2C_PORT, frequency);

    // 配置 GPIO 引脚为 I2C 功能
    gpio_set_function(TEST_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(TEST_I2C_SCL_PIN, GPIO_FUNC_I2C);

    // 启用上拉电阻
    gpio_pull_up(TEST_I2C_SDA_PIN);
    gpio_pull_up(TEST_I2C_SCL_PIN);

    uint32_t actual = i2c_set_baudrate(TEST_I2C_PORT, frequency);

    // 已弃用 直接在core1核里检测并更新并打印

    // if (print)
    // {
    //     char temp_buf[256];
    //     snprintf(temp_buf, sizeof(temp_buf), "[INFO] I2C BAUD SET: REQ %u ACT %u\r\n", (unsigned)frequency, (unsigned)actual);
    //     core1_i2c_monitor_append(temp_buf);
    //     printf("%s", temp_buf);
    // }

    // stdio_set_driver_enabled(&stdio_usb, true);///////////////////////////
    // stdio_filter_driver(&stdio_usb);
}

// 测试系统专用：配置 SPI 引脚
// void test_configure_spi_pins(int32_t frequency) {
//     // 初始化 SPI 端口
//     spi_init(TEST_SPI_PORT, frequency);

//     // 配置 SPI 引脚功能
//     gpio_set_function(TEST_SPI_SCK_PIN, GPIO_FUNC_SPI);
//     gpio_set_function(TEST_SPI_MOSI_PIN, GPIO_FUNC_SPI);
//     gpio_set_function(TEST_SPI_MISO_PIN, GPIO_FUNC_SPI);

//     // CS 引脚作为普通 GPIO 控制
//     gpio_init(TEST_SPI_CS_PIN);
//     gpio_set_dir(TEST_SPI_CS_PIN, GPIO_OUT);
//     gpio_put(TEST_SPI_CS_PIN, 1);  // CS 默认高电平

//     // 设置 SPI 模式（可选）
//     spi_set_format(TEST_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
// }

// 测试系统专用：根据模式配置引脚

void test_configure_communication_i2c_init()
{
    if (test_communication_mode)
    {
        test_configure_i2c_pins(i2c_freq);
    }
}

// ==================== 测试系统专用 MPU6050 I2C 通信函数 ====================

// MPU6050 I2C写入寄存器
void test_mpu6050_i2c_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    i2c_result(i2c_write_timeout_us(TEST_I2C_PORT, TEST_MPU6050_ADDR, data, 2, false, 1000));
}

// MPU6050 I2C读取寄存器
uint8_t test_mpu6050_i2c_read_reg(uint8_t reg)
{
    uint8_t value = 0;
    i2c_result(i2c_write_timeout_us(TEST_I2C_PORT, TEST_MPU6050_ADDR, &reg, 1, true, 1000));
    i2c_result(i2c_read_timeout_us(TEST_I2C_PORT, TEST_MPU6050_ADDR, &value, 1, false, 1000));
    return value;
}

// MPU6050 I2C读取多个寄存器
void test_mpu6050_i2c_read_regs(uint8_t reg, uint8_t *buf, uint16_t len)
{
    i2c_result(i2c_write_timeout_us(TEST_I2C_PORT, TEST_MPU6050_ADDR, &reg, 1, true, 1000));
    i2c_result(i2c_read_timeout_us(TEST_I2C_PORT, TEST_MPU6050_ADDR, buf, len, false, 1000));
}

// ==================== 测试系统专用 I2C 通信函数 ====================

// 计算渲染区域的缓存区长度
void test_calc_render_area_buflen(struct test_render_area *area)
{
    area->buflen = (area->end_col - area->start_col + 1) * (area->end_page - area->start_page + 1);
}

// 测试系统专用：发送命令到 SSD1306
void test_ssd1306_send_cmd(uint8_t cmd)
{
    if (test_communication_mode)
    {
        // I2C 模式
        uint8_t buf[2] = {0x80, cmd};
        i2c_result(i2c_write_timeout_us(TEST_I2C_PORT, TEST_I2C_ADDR, buf, 2, false, 1000));
    }
}

// 测试系统专用：发送命令列表
void test_ssd1306_send_cmd_list(uint8_t *buf, int num)
{
    if (test_communication_mode)
    {
        // I2C 模式
        for (int i = 0; i < num; i++)
        {
            test_ssd1306_send_cmd(buf[i]);
        }
    }
}

// 测试系统专用：发送数据缓存区
void test_ssd1306_send_buf(uint8_t buf[], int buflen)
{
    if (test_communication_mode)
    {
        // I2C 模式
        uint8_t *temp_buf = (uint8_t *)malloc(buflen + 1);
        if (temp_buf == NULL)
            return; // 内存分配失败

        temp_buf[0] = 0x40;
        memcpy(temp_buf + 1, buf, buflen);

        i2c_result(i2c_write_timeout_us(TEST_I2C_PORT, TEST_I2C_ADDR, temp_buf, buflen + 1, false, 100000));

        free(temp_buf);
    }
}

// 测试系统专用：渲染显示
void test_render(uint8_t *buf, struct test_render_area *area)
{
    uint8_t cmds[] = {
        TEST_SSD1306_SET_COL_ADDR,
        area->start_col,
        area->end_col,
        TEST_SSD1306_SET_PAGE_ADDR,
        area->start_page,
        area->end_page};

    test_ssd1306_send_cmd_list(cmds, TEST_COUNT_OF(cmds));
    test_ssd1306_send_buf(buf, area->buflen);
}

// 测试系统专用：SSD1306 初始化
void test_ssd1306_init()
{
    uint8_t cmds[] = {
        TEST_SSD1306_SET_DISP,               // 关闭显示
        TEST_SSD1306_SET_MEM_MODE,           // 设置内存地址模式
        0x00,                                // 水平地址模式
        TEST_SSD1306_SET_DISP_START_LINE,    // 设置显示起始行为 0
        TEST_SSD1306_SET_SEG_REMAP | 0x01,   // 设置段重新映射
        TEST_SSD1306_SET_MUX_RATIO,          // 设置复用比例
        TEST_SSD1306_HEIGHT - 1,             // 显示高度 - 1
        TEST_SSD1306_SET_COM_OUT_DIR | 0x08, // 设置 COM 输出扫描方向
        TEST_SSD1306_SET_DISP_OFFSET,        // 设置显示偏移
        0x00,                                // 无偏移
        TEST_SSD1306_SET_COM_PIN_CFG,        // 设置 COM 引脚配置
#if ((TEST_SSD1306_WIDTH == 128) && (TEST_SSD1306_HEIGHT == 32))
        0x02,
#elif ((TEST_SSD1306_WIDTH == 128) && (TEST_SSD1306_HEIGHT == 64))
        0x12,
#else
        0x02,
#endif
        TEST_SSD1306_SET_DISP_CLK_DIV, // 设置显示时钟分频比
        0x80,                          // 分频比为 1，标准频率
        TEST_SSD1306_SET_PRECHARGE,    // 设置预充电周期
        0xF1,                          // Vcc 内部生成
        TEST_SSD1306_SET_VCOM_DESEL,   // 设置 VCOMH 去选择级别
        0x30,                          // 0.83xVcc
        TEST_SSD1306_SET_CONTRAST,     // 设置对比度控制
        0xFF,
        TEST_SSD1306_SET_ENTIRE_ON,     // 设置整个显示开启以跟随 RAM 内容
        TEST_SSD1306_SET_NORM_DISP,     // 设置正常（非反色）显示
        TEST_SSD1306_SET_CHARGE_PUMP,   // 设置电荷泵
        0x14,                           // Vcc 内部生成
        TEST_SSD1306_SET_SCROLL | 0x00, // 去激活水平滚动
        TEST_SSD1306_SET_DISP | 0x01,   // 开启显示
    };

    test_ssd1306_send_cmd_list(cmds, TEST_COUNT_OF(cmds));

    // 计算帧区域缓存区长度
    test_calc_render_area_buflen(&test_frame_area);

    // 清空缓存区并渲染
    memset(test_buf, 0, TEST_SSD1306_BUF_LEN);
    test_render(test_buf, &test_frame_area);
}

// 测试系统专用：设置像素
void test_set_pixel(uint8_t *buf, int x, int y, bool on)
{
    if (x < 0 || x >= TEST_SSD1306_WIDTH || y < 0 || y >= TEST_SSD1306_HEIGHT)
    {
        return;
    }

    const int BytesPerRow = TEST_SSD1306_WIDTH;
    int byte_idx = (y / 8) * BytesPerRow + x;
    uint8_t byte = buf[byte_idx];

    if (on)
    {
        byte |= 1 << (y % 8);
    }
    else
    {
        byte &= ~(1 << (y % 8));
    }

    buf[byte_idx] = byte;
}

// 测试系统专用：绘制直线
void test_draw_line(uint8_t *buf, int x0, int y0, int x1, int y1, bool on)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int e2;

    while (true)
    {
        test_set_pixel(buf, x0, y0, on);
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

// 3D点结构
struct Point3D
{
    float x, y, z;
};

// 2D点结构
struct Point2D
{
    int x, y;
};

// 立方体顶点（标准单位立方体）
static float cube_vertices[8][3] = {
    {-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f}, {-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f}};

// 立方体边连接定义
static int cube_edges[][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

// 3D到2D投影（简单透视投影）
Point2D project_to_2d(Point3D p)
{
    float distance = 4.0f; // 观察距离
    float scale = 20.0f;   // 缩放因子（稍微缩小以适应翻转动画）

    Point2D result;
    result.x = (int)(TEST_SSD1306_WIDTH / 2 + (p.x * scale * distance) / (distance + p.z));
    result.y = (int)(TEST_SSD1306_HEIGHT / 2 + (p.y * scale * distance) / (distance + p.z));

    return result;
}

// ==================== MPU6050初始化和数据读取函数 ====================

// MPU6050初始化（根据通信模式）
bool test_mpu6050_init()
{
    if (test_communication_mode)
    {
        // I2C模式初始化
        // 1. 唤醒MPU6050
        test_mpu6050_i2c_write_reg(TEST_MPU6050_REG_PWR_MGMT_1, 0x00);
        sleep_ms(100);

        // 3. 配置加速度计量程为±2g
        test_mpu6050_i2c_write_reg(TEST_MPU6050_REG_ACCEL_CONFIG, 0x00);

        // 4. 配置陀螺仪量程为±250°/s
        test_mpu6050_i2c_write_reg(TEST_MPU6050_REG_GYRO_CONFIG, 0x00);

        // 5. 设置数字低通滤波器
        test_mpu6050_i2c_write_reg(TEST_MPU6050_REG_CONFIG, 0x06);
    }

    sleep_ms(50);
    return true;
}

// 读取MPU6050原始数据
void test_mpu6050_read_raw_data()
{
    uint8_t data[14];

    if (test_communication_mode)
    {
        // I2C模式读取
        test_mpu6050_i2c_read_regs(TEST_MPU6050_REG_ACCEL_XOUT_H, data, 14);
    }
    // 解析数据（16位有符号整数，高字节在前）
    mpu6050_data.accel_x = (int16_t)((data[0] << 8) | data[1]);
    mpu6050_data.accel_y = (int16_t)((data[2] << 8) | data[3]);
    mpu6050_data.accel_z = (int16_t)((data[4] << 8) | data[5]);

    mpu6050_data.temp = (int16_t)((data[6] << 8) | data[7]);

    mpu6050_data.gyro_x = (int16_t)((data[8] << 8) | data[9]);
    mpu6050_data.gyro_y = (int16_t)((data[10] << 8) | data[11]);
    mpu6050_data.gyro_z = (int16_t)((data[12] << 8) | data[13]);

    // 转换为物理单位
    // 加速度计：±2g量程，16位ADC，LSB = 2g/32768 = 16384 LSB/g
    mpu6050_data.accel_x_g = (float)mpu6050_data.accel_x / 16384.0f;
    mpu6050_data.accel_y_g = (float)mpu6050_data.accel_y / 16384.0f;
    mpu6050_data.accel_z_g = (float)mpu6050_data.accel_z / 16384.0f;

    // 陀螺仪：±250°/s量程，16位ADC，LSB = 250°/s/32768 = 131 LSB/(°/s)
    mpu6050_data.gyro_x_dps = (float)mpu6050_data.gyro_x / 131.0f;
    mpu6050_data.gyro_y_dps = (float)mpu6050_data.gyro_y / 131.0f;
    mpu6050_data.gyro_z_dps = (float)mpu6050_data.gyro_z / 131.0f;

    // 温度：TEMP_degC = (TEMP_OUT - RoomTemp_Offset)/Temp_Sensitivity + 21
    // RoomTemp_Offset = -521, Temp_Sensitivity = 340
    mpu6050_data.temperature_c = ((float)mpu6050_data.temp + 521.0f) / 340.0f + 21.0f;
}

// 3D旋转矩阵变换
Point3D rotate_point(Point3D p, float angle_x, float angle_y, float angle_z)
{
    Point3D result = p;

    // 绕X轴旋转
    float cos_x = cosf(angle_x), sin_x = sinf(angle_x);
    float y = result.y, z = result.z;
    result.y = y * cos_x - z * sin_x;
    result.z = y * sin_x + z * cos_x;

    // 绕Y轴旋转
    float cos_y = cosf(angle_y), sin_y = sinf(angle_y);
    float x = result.x;
    z = result.z;
    result.x = x * cos_y + z * sin_y;
    result.z = -x * sin_y + z * cos_y;

    // 绕Z轴旋转
    float cos_z = cosf(angle_z), sin_z = sinf(angle_z);
    x = result.x;
    y = result.y;
    result.x = x * cos_z - y * sin_z;
    result.y = x * sin_z + y * cos_z;

    return result;
}

// 绘制动态立方体（随时间旋转）
void draw_cube(uint8_t *buf, float time)
{
    Point3D rotated_vertices[8];
    Point2D projected_vertices[8];

    float angle_x = time * 0.7f;
    float angle_y = time * 1.0f;
    float angle_z = time * 0.5f;

    // 处理所有顶点：3D旋转->2D投影
    for (int i = 0; i < 8; i++)
    {
        Point3D v = {cube_vertices[i][0], cube_vertices[i][1], cube_vertices[i][2]};
        rotated_vertices[i] = rotate_point(v, angle_x, angle_y, angle_z);
        projected_vertices[i] = project_to_2d(rotated_vertices[i]);
    }

    // 绘制所有边
    for (int i = 0; i < TEST_COUNT_OF(cube_edges); i++)
    {
        int v1 = cube_edges[i][0];
        int v2 = cube_edges[i][1];

        Point2D p1 = projected_vertices[v1];
        Point2D p2 = projected_vertices[v2];
        test_draw_line(buf, p1.x, p1.y, p2.x, p2.y, true);
    }
}

// 更新帧率统计
uint32_t update_fps()
{

    uint32_t current_time = get_time_ms();

    // 每秒更新一次帧率
    if (current_time - last_fps_time >= 1000)
    {
        screen_refresh_rate = fps_counter;
        fps_counter = 0;
        last_fps_time = current_time;
    }
    fps_counter++;
    return current_time;
}

// 检查是否需要翻转显示颜色
void check_display_invert()
{
    uint32_t current_time = get_time_ms();

    // 每5秒翻转一次显示颜色
    if (current_time - last_invert_time >= 5000)
    {
        invert_display = !invert_display;
        last_invert_time = current_time;
    }
}

// 显示测试信息（仅针对SSD1306设备）
void draw_test_info(uint8_t *buf)
{
    // 仅在SSD1306测试模式下显示信息
    if (current_test_device == TEST_DEVICE_SSD1306)
    {
        // 每秒更新一次叠加文字，其余帧直接复用缓存并快速合成
        update_info_overlay_if_needed();
        blit_info_overlay(buf);
    }
    // MPU6050测试模式下不进行屏幕绘制
}

void core1_test_internal()
{
    // 根据模式配置通信引脚和初始化
    screen_refresh_rate = -1;
    device_init_success = false;
    fps_counter = 0;
    // last_fps_time = get_time_ms();

    test_configure_communication_i2c_init();

    auto init_device = [&]()
    {
        if (current_test_device == TEST_DEVICE_SSD1306)
        {
            // 初始化测试缓存区
            test_calc_render_area_buflen(&test_frame_area);
            memset(test_buf, 0, TEST_SSD1306_BUF_LEN);

            // 初始化SSD1306显示器
            test_ssd1306_init();

            // 初始化时间变量
            last_fps_time = get_time_ms();
            last_invert_time = get_time_ms();
        }
        else if (current_test_device == TEST_DEVICE_MPU6050)
        {
            // 初始化MPU6050传感器
            test_mpu6050_init();
            last_fps_time = get_time_ms();
        }
        fps_counter = 0;
        screen_refresh_rate = -1;
    };

    if (current_test_device == TEST_DEVICE_SSD1306)
    {
        // init_device();
        char temp_char[64] = {0};
        snprintf(temp_char, sizeof(temp_char), "[SSD1306 Demo] I2C %ldHz Test Started\r\n", i2c_freq);
        core1_i2c_monitor_append(temp_char);
        printf("%s", temp_char);

        while (i2cspi_test_running && i2c_run_in_core1)
        {
            // SSD1306_PrintBufRaw();////////////////////////////////////////
            

            if (i2c_result_return == PICO_ERROR_GENERIC)
            {
                // no device found
                device_init_success = false;
                device_error_occurred = false;
                uint8_t tmp = 0;
                i2c_result(i2c_write_timeout_us(TEST_I2C_PORT, TEST_I2C_ADDR, &tmp, 1, false, 1000));
                if (i2c_result_return >= 0)
                {
                    snprintf(temp_char, sizeof(temp_char), "[SSD1306 Demo] Device Connected!\r\n");
                    core1_i2c_monitor_append(temp_char);
                    printf("%s", temp_char);

                    i2c_deinit(TEST_I2C_PORT);
                    test_configure_communication_i2c_init();
                    sleep_ms(250);
                    init_device();
                    sleep_ms(0);
                    continue;
                }
                Close_StateLED();
                continue;
            }
            else if (i2c_result_return == PICO_ERROR_TIMEOUT)
            {
                // device error occurred
                snprintf(temp_char, sizeof(temp_char), "[SSD1306 Demo] Device Error, Try Reconnect\r\n");
                core1_i2c_monitor_append(temp_char);
                printf("%s", temp_char);

                device_error_occurred = true;
                i2c_deinit(TEST_I2C_PORT);
                test_configure_communication_i2c_init();
                sleep_ms(250);
                init_device();
                sleep_ms(0);
                uint8_t tmp = 0;
                i2c_result(i2c_write_timeout_us(TEST_I2C_PORT, TEST_I2C_ADDR, &tmp, 1, false, 1000));
                if (i2c_result_return >= 0)
                {
                    snprintf(temp_char, sizeof(temp_char), "[SSD1306 Demo] Device Reconnected!\r\n");
                    core1_i2c_monitor_append(temp_char);
                    printf("%s", temp_char);
                }
                Close_StateLED();
                continue;
            }
            else
            {
                Open_StateLED();
                device_init_success = true;
                device_error_occurred = false;
            }

            // SSD1306测试：绘制动画和显示信息
            // 清空缓存区
            memset(test_buf, 0, TEST_SSD1306_BUF_LEN);

            // // 更新帧率统计
            float time = update_fps() * 0.001f;

            // // 绘制立方体动画
            draw_cube(test_buf, time);

            // 显示测试信息 +
            draw_test_info(test_buf);

            // 检查是否需要翻转显示颜色
            check_display_invert();

            // 如果需要翻转，对整个缓存区进行翻转
            if (invert_display)
            {
                for (int i = 0; i < TEST_SSD1306_BUF_LEN; i++)
                {
                    test_buf[i] = ~test_buf[i]; // 翻转所有位
                }
            }

            // 更新显示
            test_render(test_buf, &test_frame_area);
        }
        snprintf(temp_char, sizeof(temp_char), "[SSD1306 Demo] Test Stopped\r\n");
        core1_i2c_monitor_append(temp_char);
        printf("%s", temp_char);
        Close_StateLED();
        return;
    }
    else if (current_test_device == TEST_DEVICE_MPU6050)
    {
        // init_device();
        char temp_char[64] = {0};
        snprintf(temp_char, sizeof(temp_char), "[MPU6050 Demo] I2C %ldHz Test Started\r\n", i2c_freq);
        core1_i2c_monitor_append(temp_char);
        printf("%s", temp_char);

        while (i2cspi_test_running && i2c_run_in_core1)
        {
            // SSD1306_PrintBufRaw();////////////////////////////////////////
            

            if (i2c_result_return == PICO_ERROR_GENERIC)
            {
                // no device found
                device_init_success = false;
                device_error_occurred = false;
                uint8_t tmp = 0;
                i2c_result(i2c_write_timeout_us(TEST_I2C_PORT, TEST_MPU6050_ADDR, &tmp, 1, false, 1000));
                if (i2c_result_return >= 0)
                {
                    snprintf(temp_char, sizeof(temp_char), "[MPU6050 Demo] Device Connected!\r\n");
                    core1_i2c_monitor_append(temp_char);
                    printf("%s", temp_char);


                    i2c_deinit(TEST_I2C_PORT);
                    test_configure_communication_i2c_init();
                    sleep_ms(350);
                    init_device();
                    sleep_ms(0);
                    continue;
                }
                Close_StateLED();
                continue;
                
            }
            else if (i2c_result_return == PICO_ERROR_TIMEOUT)
            {
                snprintf(temp_char, sizeof(temp_char), "[MPU6050 Demo] Device Error, Try Reconnect\r\n");
                core1_i2c_monitor_append(temp_char);
                printf("%s", temp_char);

                // device error occurred
                device_error_occurred = true;
                i2c_deinit(TEST_I2C_PORT);
                test_configure_communication_i2c_init();
                sleep_ms(350);
                init_device();
                sleep_ms(0);
                uint8_t tmp = 0;
                i2c_result(i2c_write_timeout_us(TEST_I2C_PORT, TEST_MPU6050_ADDR, &tmp, 1, false, 1000));
                if (i2c_result_return >= 0)
                {
                    snprintf(temp_char, sizeof(temp_char), "[MPU6050 Demo] Device Reconnected!\r\n");
                    core1_i2c_monitor_append(temp_char);
                    printf("%s", temp_char);
                }
                Close_StateLED();
                continue;
            }
            else
            {
                    Open_StateLED();
                device_init_success = true;
                device_error_occurred = false;
            }

            // MPU6050测试：仅读取数据，不进行屏幕渲染
            update_fps();
            test_mpu6050_read_raw_data();

            // printf("%d", i2c_result_return);
        }
        snprintf(temp_char, sizeof(temp_char), "[MPU6050 Demo] Test Stopped\r\n");
        core1_i2c_monitor_append(temp_char);
        printf("%s", temp_char);
        Close_StateLED();
        
        return;
    }
}

// 启动Core1测试（支持选择I2C或SPI模式）
void start_core1_ssd1306_test(bool is_i2c_mode)
{
    // 设置通信模式
    test_communication_mode = is_i2c_mode;
    // 设置测试设备为SSD1306
    current_test_device = TEST_DEVICE_SSD1306;
    // I2C状态
    i2c_result_return = -1;
    // 启动多核任务
    i2cspi_test_running = true; 
}

// 启动Core1 MPU6050测试（支持选择I2C或SPI模式）
void start_core1_mpu6050_test(bool is_i2c_mode)
{

    // 设置通信模式
    test_communication_mode = is_i2c_mode;
    // 设置测试设备为MPU6050
    current_test_device = TEST_DEVICE_MPU6050;

    // 清空MPU6050数据
    memset(&mpu6050_data, 0, sizeof(mpu6050_data));
    // I2C状态
    i2c_result_return = -1;
    // 启动多核任务
    i2cspi_test_running = true; 
}

// 停止Core1测试
void stop_core1_test()
{
    i2cspi_test_running = false;
}

// ==================== 外部访问接口函数 ====================

// 获取屏幕刷新率（供外部访问）
int32_t get_screen_refresh_rate()
{
    return screen_refresh_rate;
}

// 获取MPU6050数据（供外部访问）
struct MPU6050_Data *get_mpu6050_data()
{
    return &mpu6050_data;
}

// 获取当前测试设备类型（供外部访问）
enum TestDevice get_current_test_device()
{
    return current_test_device;
}

// 获取当前通信模式（供外部访问）
bool get_current_communication_mode()
{
    return test_communication_mode;
}

#endif