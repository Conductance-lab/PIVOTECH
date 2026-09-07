#ifndef _CORE_CPP_
#define _CORE_CPP_

#include "CONFIG_FLO.hpp"
#include "pico/stdlib.h"
#include "menu/flash.hpp"
#include "menu/StateLED.hpp"
#include "menu/license_crypto.hpp"

#include "read/hardware_core1.hpp"
#include "read/select.hpp"
#include "read/uartconfig.hpp"
#include "read/adcxy.hpp"
#include "read/ssd1306_demo.hpp"
#include "read/mpu6050_demo.hpp"
#include "read/hc05.hpp"
#define _FUNCTIONS_CPP_
#include "read/functions.hpp"
#include "read/mod_selection.hpp"
#include "read/aitest.hpp"
#include "menu/UI.hpp"
#include "pico/unique_id.h" // Added for pico_unique_board_id_t
#include "menu/key.hpp"

#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cmath>

#include "hardware/irq.h"
#include "hardware/watchdog.h"

pico_unique_board_id_t id; // 用于存储唯一 ID 的全局变量

// 外部频率计数相关变量/回调（定义于 read/generalread.cpp）
extern bool highspeed_ta_flag;
extern bool highspeed_tb_flag;
extern volatile float ta_freqs[];
extern volatile float tb_freqs[];
extern volatile float ta_dutys[];
extern volatile float tb_dutys[];
extern volatile uint32_t ta_highfrec_count[];
extern volatile uint32_t tb_highfrec_count[];
extern uint32_t ta_count_frec;
extern uint32_t tb_count_frec;
extern int32_t spi_freq;
extern int32_t i2c_freq;

static int map_baudrate_to_code(int baudrate)
{
    switch (baudrate)
    {
    case 8000000:
        return 0;
    case 7500000:
        return 1;
    case 6000000:
        return 2;
    case 5000000:
        return 3;
    case 4000000:
        return 4;
    case 3000000:
        return 5;
    case 2000000:
        return 6;
    case 1500000:
        return 7;
    case 1000000:
        return 8;
    case 921600:
        return 9;
    case 750000:
        return 10;
    case 512000:
        return 11;
    case 460800:
        return 12;
    case 256000:
        return 13;
    case 230400:
        return 14;
    case 128000:
        return 15;
    case 115200:
        return 16;
    case 57600:
        return 17;
    case 56000:
        return 18;
    case 38400:
        return 19;
    case 19200:
        return 20;
    case 14400:
        return 21;
    case 9600:
        return 22;
    case 4800:
        return 23;
    case 2400:
        return 24;
    case 1200:
        return 25;
    case 600:
        return 26;
    case 300:
        return 27;
    default:
        return -1;
    }
}

static int map_databits_to_code(uint data_bits)
{
    switch (data_bits)
    {
    case 5:
        return 0;
    case 6:
        return 1;
    case 7:
        return 2;
    case 8:
        return 3;
    default:
        return -1;
    }
}

static int map_parity_to_code(uart_parity_t parity)
{
    switch (parity)
    {
    case UART_PARITY_NONE:
        return 0;
    case UART_PARITY_ODD:
        return 1;
    case UART_PARITY_EVEN:
        return 2;
    default:
        return -1;
    }
}

static int map_stopbits_to_code(uint stop_bits)
{
    switch (stop_bits)
    {
    case 1:
        return 0;
    case 2:
        return 1;
    default:
        return -1;
    }
}

extern bool need_core1_irq_init;
void high_freq_isr_ta(uint gpio, uint32_t events);
void high_freq_isr_tb(uint gpio, uint32_t events);
void freq_isr(uint gpio, uint32_t events);
void high_freq_irq_tatb(uint gpio, uint32_t events);

extern bool Enable_PC_Interface;
extern bool menu_update_flag;
extern float TA_F;
extern float TB_F;
extern float TA_Duty;
extern float TB_Duty;

volatile bool send_pc_flag = false; // 标记是否有数据需要发送到上位机

// 与 generalread.cpp 的计数配置保持一致

char current_configdata[CONFIG_DATA_MAX_LEN] = {0};
size_t term_index = 0; // 终止符匹配进度
size_t data_index = 0; // 数据存储位置
bool if_receive_massage = 0;
// bool load_mode_1UART_0USB = 0;

bool opnPCchangemode = 0; // 标记是否正在进行上位机配置模式

bool if_need_send_menu_data = false;     // 标记是否需要发送菜单数据到上位机
bool if_need_send_menu_settings = false; // 标记是否需要发送配置数据到上位机

bool core1_usb_connect_uart = false;
bool core1_irq_count_mode = false;

const char TERMINATOR[] = "#END_CONFIG#";           // 终止符
constexpr size_t TERM_LEN = sizeof(TERMINATOR) - 1; // 终止符长度（自动计算）

// I2C监视器缓存
#define I2C_MONITOR_BUFFER_SIZE 512
char i2ctest_rx_buffer[I2C_MONITOR_BUFFER_SIZE];
int i2ctest_rx_index = 0;

// SPI监视器缓存
#define SPI_MONITOR_BUFFER_SIZE 32
uint8_t spitest_tx_buffer[SPI_MONITOR_BUFFER_SIZE];
uint8_t spitest_rx_buffer[SPI_MONITOR_BUFFER_SIZE];
int spitest_tx_index = 0;
int spitest_rx_index = 0;
bool spitest_buf_full = false;
bool spitext_ifupdate = false;

// 字符串写入函数
void core1_i2c_monitor_append(const char *str)
{
    while (*str)
    {
        i2ctest_rx_buffer[i2ctest_rx_index] = *str;
        i2ctest_rx_index = (i2ctest_rx_index + 1) % I2C_MONITOR_BUFFER_SIZE;
        str++;
    }
}

static inline void i2c_pc_print_begin()
{
    if (Enable_PC_Interface)
    {
        printf("#PIVOICIF#");
    }
}

static inline void i2c_pc_print_end()
{
    if (Enable_PC_Interface)
    {
        printf("#IFS#");
    }
}

// SPI 上位机包装：在 Enable_PC_Interface 为 true 时添加帧头/帧尾
static inline void spi_pc_print_begin()
{
    if (Enable_PC_Interface)
    {
        printf("#PIVOSPIF#");
    }
}

static inline void spi_pc_print_end()
{
    if (Enable_PC_Interface)
    {
        printf("#SFS#\r\n");
    }
}

// SPI 监视器写入函数 (废弃字符串写入，改为原始数据写入)
void core1_spi_monitor_append_tx(uint8_t data)
{
    spitest_tx_buffer[spitest_tx_index] = data;
    spitest_tx_index = (spitest_tx_index + 1);
    if (spitest_tx_index >= SPI_MONITOR_BUFFER_SIZE)
    {
        spitest_tx_index = 0;
        spitest_buf_full = true;
    }
}

void core1_spi_monitor_append_rx(uint8_t data)
{
    spitest_rx_buffer[spitest_rx_index] = data;
    spitest_rx_index = (spitest_rx_index + 1);
    if (spitest_rx_index >= SPI_MONITOR_BUFFER_SIZE)
    {
        spitest_rx_index = 0;
        spitest_buf_full = true;
    }
}

// 串口接收缓冲区
#define SERIAL_RX_BUFFER_SIZE 256 // 256
char serial_rx_buffer[SERIAL_RX_BUFFER_SIZE] = "";
int serial_rx_index = 0;
// bool serial_data_received = false;

// 分行策略：超时 + 定长
#define LINE_TIMEOUT_US 150000 // 超时分行阈值（微秒）
#define LINE_MAX_LEN 64        // 每行最大字符数（不含方向符号）

// 每个方向各自维护状态，互不影响
typedef struct
{
    bool expecting_lf;     // 是否刚接收了'\r'，期待'\n'
    int sentence_length;   // 当前行累计长度
    uint32_t last_tick_us; // 最近一次该方向收到字符的时间戳
} DirectionState;

static DirectionState dir_out_state = {false, 0, 0}; // '>' USB->UART
static DirectionState dir_in_state = {false, 0, 0};  // '<' UART->USB

bool core1_need_wait = 0;

// void Menu_connection();
void process_received_char(uint32_t now, DirectionState *st, DirectionState *other_st, bool direction, char received_char);

bool TXisReady = 0, RXisReady = 0;

// USB->UART non-blocking TX queue to avoid stalling core1 loop.
#define USB_UART_TX_QUEUE_SIZE 1024
static char usb_uart_tx_queue[USB_UART_TX_QUEUE_SIZE];
static uint16_t usb_uart_tx_head = 0;
static uint16_t usb_uart_tx_tail = 0;

static inline bool usb_uart_tx_queue_is_full()
{
    return (uint16_t)((usb_uart_tx_head + 1) % USB_UART_TX_QUEUE_SIZE) == usb_uart_tx_tail;
}

static inline bool usb_uart_tx_queue_is_empty()
{
    return usb_uart_tx_head == usb_uart_tx_tail;
}

static inline void usb_uart_tx_queue_push(char ch)
{
    if (usb_uart_tx_queue_is_full())
    {
        return;
    }
    usb_uart_tx_queue[usb_uart_tx_head] = ch;
    usb_uart_tx_head = (uint16_t)((usb_uart_tx_head + 1) % USB_UART_TX_QUEUE_SIZE);
}

static void usb_uart_tx_queue_push_buffer(const char *buf, size_t len)
{
    if (!buf)
    {
        return;
    }
    for (size_t i = 0; i < len; ++i)
    {
        usb_uart_tx_queue_push(buf[i]);
    }
}

static void usb_uart_tx_queue_drain(uint32_t now, DirectionState *st, DirectionState *other_st, uint16_t max_bytes)
{
    uint16_t sent = 0;
    while (!usb_uart_tx_queue_is_empty() && uart_is_writable(UART_ID) && sent < max_bytes)
    {
        char ch = usb_uart_tx_queue[usb_uart_tx_tail];
        usb_uart_tx_tail = (uint16_t)((usb_uart_tx_tail + 1) % USB_UART_TX_QUEUE_SIZE);
        uart_putc_raw(UART_ID, ch);
        TXisReady = 1;
        process_received_char(now, st, other_st, 0, ch);
        sent++;
    }
}

// 周期检查 need_core1_irq_init 的定时器（高优先级、200ms）
static struct repeating_timer irq_reinit_timer;
static bool irq_reinit_timer_started = false;

// send显示内容
char *backString = NULL;

char serial_anotherdir_buffer[SERIAL_RX_BUFFER_SIZE] = "";
int serial_anotherdir_index = 0;

bool now_read_direction = 0; // 0 in ;1 out

// 辅助：超时或定长时插入换行（只写入'\n'，下一次写入时会自动补方向符号）
static inline void flush_line(DirectionState *st)
{
    if (st->sentence_length > 0)
    {
        serial_rx_buffer[serial_rx_index] = '\n';
        serial_rx_index = (serial_rx_index + 1) % SERIAL_RX_BUFFER_SIZE;
        st->sentence_length = 0;
        st->expecting_lf = false;
    }
}

static inline void flush_line_another_buffer(DirectionState *st)
{
    if (st->sentence_length > 0)
    {
        serial_anotherdir_buffer[serial_anotherdir_index] = '\n';
        serial_anotherdir_index = (serial_anotherdir_index + 1) % SERIAL_RX_BUFFER_SIZE;
        st->sentence_length = 0;
        st->expecting_lf = false;
    }
}

// 更换now_read_direction并把另一个方向缓存写入主缓存
void switch_read_direction(DirectionState *st, DirectionState *other_st)
{

    // 主缓存区里如果当前不是换行 则加一个换行
    if (serial_rx_buffer[(serial_rx_index - 1 + SERIAL_RX_BUFFER_SIZE) % SERIAL_RX_BUFFER_SIZE] != '\n')
    {
        serial_rx_buffer[serial_rx_index] = '\n';
        serial_rx_index = (serial_rx_index + 1) % SERIAL_RX_BUFFER_SIZE;
    }

    // 先将另一个方向的缓存数据写入主缓存
    for (int i = 0; i < serial_anotherdir_index; i++)
    {
        char ch = serial_anotherdir_buffer[i];
        if (ch == '\0')
        {
            break; // 到达缓存末尾
        }
        serial_rx_buffer[serial_rx_index] = ch;
        serial_rx_index = (serial_rx_index + 1) % SERIAL_RX_BUFFER_SIZE;
    }

    st->sentence_length = 0;
    st->expecting_lf = false;
    // 清空另一个方向的缓存
    memset(serial_anotherdir_buffer, 0, SERIAL_RX_BUFFER_SIZE);
    serial_anotherdir_index = 0;

    // 切换当前读取方向
    now_read_direction = !now_read_direction;
}

// 处理接收到的字符（立即处理，无延时）
void process_received_char(uint32_t now, DirectionState *st, DirectionState *other_st, bool direction, char received_char)
{
    if (now_read_direction == direction)
    {
        // CR/LF 组合与单独换行处理
        if (received_char == '\r')
        {
            st->expecting_lf = true;
            st->last_tick_us = now;
            flush_line(st);
            return;
        }

        if (received_char == '\n')
        {
            if (st->expecting_lf)
            {
                st->expecting_lf = false;
                st->last_tick_us = now;
                return; // 已处理为CRLF组合，直接返回
            }
            // 来了换行，立即结束本方向当前行
            flush_line(st);
            st->last_tick_us = now;
            return;
        }
        else
        {
            st->expecting_lf = false;
        }
        // 如是新行，先写入方向标识
        if (st->sentence_length == 0)
        {
            serial_rx_buffer[serial_rx_index] = direction ? '<' : '>';
            serial_rx_index = (serial_rx_index + 1) % SERIAL_RX_BUFFER_SIZE;
        }

        // 写入字符
        serial_rx_buffer[serial_rx_index] = received_char;
        serial_rx_index = (serial_rx_index + 1) % SERIAL_RX_BUFFER_SIZE;
        st->sentence_length++;
        st->last_tick_us = now;
    }
    else if (now_read_direction != direction)
    {
        // CR/LF 组合与单独换行处理
        if (received_char == '\r')
        {
            other_st->expecting_lf = true;
            other_st->last_tick_us = now;
            flush_line_another_buffer(other_st);
            switch_read_direction(st, other_st);
            return;
        }
        if (received_char == '\n')
        {
            if (other_st->expecting_lf)
            {
                other_st->expecting_lf = false;
                other_st->last_tick_us = now;
                return; // 已处理为CRLF组合，直接返回
            }
            // 来了换行，立即结束本方向当前行
            flush_line_another_buffer(other_st);
            other_st->last_tick_us = now;
            switch_read_direction(st, other_st);
            return;
        }
        else
        {
            other_st->expecting_lf = false;
        }
        // 如是新行，先写入方向标识
        if (other_st->sentence_length == 0)
        {
            serial_anotherdir_buffer[serial_anotherdir_index] = direction ? '<' : '>';
            serial_anotherdir_index++;
        }
        // 写入字符
        serial_anotherdir_buffer[serial_anotherdir_index] = received_char;
        serial_anotherdir_index = (serial_anotherdir_index + 1) % SERIAL_RX_BUFFER_SIZE;
        other_st->sentence_length++;
        other_st->last_tick_us = now;
    }

    // // 定长触发换行
    // if (st->sentence_length >= LINE_MAX_LEN)
    // {
    //     flush_line(st);
    // }
}

// void Menu_connection()
// {
//     // connection
//     if (menusetting_USB && !menusetting_UARTA && !menusetting_UARTB)
//     {
//         // 启用USB，禁用UART
//         stdio_set_driver_enabled(&stdio_usb, true);
//         stdio_set_driver_enabled(&stdio_uart, true);
//         // 过滤到USB
//         stdio_filter_driver(&stdio_usb);
//     }
//     else if (!menusetting_USB && (menusetting_UARTA || menusetting_UARTB))
//     {
//         // 启用UART，禁用USB
//         stdio_set_driver_enabled(&stdio_usb, true);
//         stdio_set_driver_enabled(&stdio_uart, true);
//         // 过滤到UART
//         stdio_filter_driver(&stdio_uart);
//     }
//     else if (menusetting_USB && (menusetting_UARTA || menusetting_UARTB))
//     {
//         // 同时启用两个驱动
//         stdio_set_driver_enabled(&stdio_usb, true);
//         stdio_set_driver_enabled(&stdio_uart, true);
//         // 清除过滤，输出到所有接口
//         stdio_filter_driver(NULL);
//     }
//     else if (!menusetting_USB && !menusetting_UARTA && !menusetting_UARTB)
//     {
//         // 禁用所有驱动使能
//         stdio_set_driver_enabled(&stdio_usb, false);
//         stdio_set_driver_enabled(&stdio_uart, false);
//         // 设置空过滤
//         stdio_filter_driver(NULL);
//     }
// }

#include "pico/bootrom.h"
#include "pico/stdio_usb.h"
#include "pico/stdio_uart.h"
#include "hardware/i2c.h"
#include <stdio.h>
// 时间常量定义

// 命令行解析逻辑（待补充细节）
static bool parse_ulong_token(const char *token, unsigned long *out)
{
    if (!token || !out)
    {
        return false;
    }

    char *end = nullptr;
    unsigned long value = strtoul(token, &end, 10);
    if (end == token)
    {
        return false;
    }
    while (*end == ' ' || *end == '\t')
    {
        ++end;
    }
    if (*end != '\0')
    {
        return false;
    }

    *out = value;
    return true;
}

static bool parse_double_token(const char *token, double *out)
{
    if (!token || !out)
    {
        return false;
    }

    char *end = nullptr;
    double value = strtod(token, &end);
    if (end == token)
    {
        return false;
    }
    while (*end == ' ' || *end == '\t')
    {
        ++end;
    }
    if (*end != '\0')
    {
        return false;
    }

    *out = value;
    return true;
}

static bool parse_int_token(const char *token, int *out)
{
    if (!token || !out)
    {
        return false;
    }

    char *end = nullptr;
    long value = strtol(token, &end, 10);
    if (end == token)
    {
        return false;
    }
    while (*end == ' ' || *end == '\t')
    {
        ++end;
    }
    if (*end != '\0')
    {
        return false;
    }

    *out = (int)value;
    return true;
}

static int clamp_int_range(int value, int min_value, int max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

// SYS,C 专用上位机验证指令：回复硬件版本、软件版本、序列号、授权信息与串口参数，
// 并触发“重新进入当前模式”。供 core1 process_pc_command 与 AI 模式 aitest 接收机共用，
// 保证刷新/重连握手在 AI 独占串口（core1 让出读取）时仍可正常应答。
void Pc_ReplySystemInfo(void)
{
    pico_get_unique_board_id(&id);

    int active_baud = 0;
    uint active_data_bits = 0;
    uint active_stop_bits = 0;
    uart_parity_t active_parity = UART_PARITY_NONE;
    uartconfig_get_local_params(&active_baud, &active_data_bits, &active_stop_bits, &active_parity);

    printf("#PIVOCOTX#id=%02x%02x%02x%02x%02x%02x%02x%02x,hv=PIVO.H%d,sv=%d.%d,lic=%s,ucf=%d,%d,%d,%d,%d#PCS#\r\n", id.id[0], id.id[1], id.id[2], id.id[3], id.id[4], id.id[5], id.id[6], id.id[7], PIVOhw, PIVOsw, PIVOsw_sub, is_device_licensed ? "true" : "false", keep_host_set, map_baudrate_to_code(active_baud), map_databits_to_code(active_data_bits), map_stopbits_to_code(active_stop_bits), map_parity_to_code(active_parity));
    // 打开上位机推流
    send_pc_flag = true; // 让其他进程发送一次状态
    Enable_PC_Interface = true;
    Key_RequestSendSelectVoltage();
    Key_TrySendSelectVoltage();

    opnPCchangemode = 1; // 重新进入当前模式
}

void process_pc_command(const char *cmd_str)
{
    // 解析命令字符串并执行相应操作

    // 这部分格式 配置命令 #PIVOUSCO# ... #PRS# 的解析逻辑已在 handle_pivoconf_match 中实现，这里仅处理其他命令

    // SYS,0 看门狗重启
    // SYS，1 进入bootloader
    if (strcmp(cmd_str, "SYS,0") == 0)
    {
        watchdog_reboot(0, 0, 0); // 立即重启
    }
    else if (strcmp(cmd_str, "SYS,1") == 0)
    {
        reset_usb_boot(0, 0); // 进入bootloader
    }
    // SYS,C 专用上位机验证指令（应答逻辑抽到 Pc_ReplySystemInfo，AI 模式亦调用）
    else if (strcmp(cmd_str, "SYS,C") == 0)
    {
        Pc_ReplySystemInfo();
    }
    // SYS,M,x 切换系统模式
    else if (strncmp(cmd_str, "SYS,M,", 6) == 0)
    {
        char *mode_str = (char *)(cmd_str + 6);
        int mode = atoi(mode_str);
        nowselect_function = mode;
        opnPCchangemode = 1; // 标记正在进行上位机配置模式
    }
    // #PIVOUSCO#I2C,x#PRS# 更改 i2ctest_scl_on_left
    else if (strncmp(cmd_str, "I2C,", 4) == 0)
    {
        char *val_str = (char *)(cmd_str + 4);
        int val = atoi(val_str);
        extern bool i2ctest_scl_on_left;
        i2ctest_scl_on_left = (val != 0);
        send_pc_flag = true;
    }
    // STS,UARTHOST,0/1 切换是否使用主机串口参数
    else if (strncmp(cmd_str, "STS,UARTHOST,", 13) == 0)
    {
        char *value_str = (char *)(cmd_str + 13);
        int value = atoi(value_str);
        keep_host_set = (value != 0);
        // 更新串口配置以应用新的参数
        extern void apply_uart_settings();
        apply_uart_settings();
    }

    // 读取#PIVOUSCO#GOU,...四组数据和选项框位置#PRS#
    else if (strncmp(cmd_str, "GOU,", 4) == 0)
    {
        char parse_buf[512];
        strncpy(parse_buf, cmd_str + 4, sizeof(parse_buf) - 1);
        parse_buf[sizeof(parse_buf) - 1] = '\0';

        const int MAX_TOKENS = 32;
        char *tokens[MAX_TOKENS] = {0};
        int token_count = 0;

        char *ctx = nullptr;
        char *tok = strtok_r(parse_buf, ",", &ctx);
        while (tok && token_count < MAX_TOKENS)
        {
            tokens[token_count++] = tok;
            tok = strtok_r(nullptr, ",", &ctx);
        }

        bool updated = false;
        int parsed_select = -1;
        int changed_channels[4] = {-1, -1, -1, -1};
        int changed_count = 0;

        for (int i = 0; i < 4; ++i)
        {
            int base = i * 6;
            if (base >= token_count)
            {
                break;
            }

            bool channel_changed = false;
            bool open_changed = false;
            bool freq_changed = false;
            bool duty_changed = false;
            bool output_mode_changed = false;
            bool run_mode_changed = false;
            bool cfg_value_changed = false;

            bool old_open = PWMitem[i].open;
            double old_freq = *PWMitem[i].frec->param;
            double old_duty = *PWMitem[i].duty->param;
            uint8_t old_output_mode = PWMitem[i].cfg.output_mode;
            uint8_t old_run_mode = PWMitem[i].cfg.run_mode;
            uint16_t old_cfg_value = PWMitem[i].cfg.cfg_value;

            unsigned long u = 0;
            double d = 0.0;

            if (base < token_count && parse_ulong_token(tokens[base], &u))
            {
                bool new_open = (u != 0);
                if (new_open != old_open)
                {
                    open_changed = true;
                    channel_changed = true;
                }
                PWMitem[i].open = new_open;
            }

            if ((base + 1) < token_count && parse_double_token(tokens[base + 1], &d))
            {
                if (d < 10.0)
                    d = 10.0;
                if (d > 12500000.0)
                    d = 12500000.0;
                if (fabs(d - old_freq) > 1e-9)
                {
                    freq_changed = true;
                    channel_changed = true;
                }
                *PWMitem[i].frec->param = d;
            }

            if ((base + 2) < token_count && parse_double_token(tokens[base + 2], &d))
            {
                if (d < 0.0)
                    d = 0.0;
                if (d > 1.0)
                    d = 1.0;
                if (fabs(d - old_duty) > 1e-9)
                {
                    duty_changed = true;
                    channel_changed = true;
                }
                *PWMitem[i].duty->param = d;
            }

            if ((base + 3) < token_count && parse_ulong_token(tokens[base + 3], &u))
            {
                uint8_t new_output_mode = (uint8_t)clamp_int_range((int)u, 0, 2);
                if (new_output_mode != old_output_mode)
                {
                    output_mode_changed = true;
                    channel_changed = true;
                }
                PWMitem[i].cfg.output_mode = new_output_mode;
            }

            if ((base + 4) < token_count && parse_ulong_token(tokens[base + 4], &u))
            {
                uint8_t new_run_mode = (uint8_t)clamp_int_range((int)u, 0, 2);
                if (new_run_mode != old_run_mode)
                {
                    run_mode_changed = true;
                    channel_changed = true;
                }
                PWMitem[i].cfg.run_mode = new_run_mode;
            }

            if ((base + 5) < token_count && parse_ulong_token(tokens[base + 5], &u))
            {
                uint16_t new_cfg_value = (uint16_t)clamp_int_range((int)u, 0, 65535);
                if (new_cfg_value != old_cfg_value)
                {
                    cfg_value_changed = true;
                    channel_changed = true;
                }
                PWMitem[i].cfg.cfg_value = new_cfg_value;
            }

            if (channel_changed && changed_count < 4)
            {
                changed_channels[changed_count++] = i;
            }
        }

        if (token_count > 24)
        {
            unsigned long sel = 0;
            if (parse_ulong_token(tokens[24], &sel))
            {
                parsed_select = clamp_int_range((int)sel, 0, 11);
            }
        }

        if (changed_count > 0)
        {
            extern int nowselect_pwmoutput_item;
            extern void Update_pwmconfig();

            int original_select = nowselect_pwmoutput_item;

            for (int i = 0; i < changed_count; ++i)
            {
                int channel = changed_channels[i];
                nowselect_pwmoutput_item = channel * 3;
                Update_pwmconfig();
                pwmoutput_apply_channel_state((uint8_t)channel);
            }

            if (parsed_select >= 0)
            {
                nowselect_pwmoutput_item = parsed_select;
            }
            else
            {
                nowselect_pwmoutput_item = clamp_int_range(original_select, 0, 11);
            }

            if ((nowselect_pwmoutput_item % 3 == 2) && (PWMitem[nowselect_pwmoutput_item / 3].cfg.output_mode != 0))
            {
                nowselect_pwmoutput_item -= 1;
            }

            send_pc_flag = true;
        }
        else
        {
            // 仅更新选择项
            if (parsed_select >= 0)
            {
                nowselect_pwmoutput_item = parsed_select;
                send_pc_flag = true;
            }
        }
    }

    // #PIVOUSCO#SYS,TOL,x#PRS# 更改工具01234
    else if (strncmp(cmd_str, "SYS,TOL,", 8) == 0)
    {
        char *tol_str = (char *)(cmd_str + 8);
        int tol_value = atoi(tol_str);
        if (tol_value >= 0 && tol_value <= 4)
        {
            extern int nowselect_moditem;
            extern bool opnPCchangetool;
            extern bool exit_uartconfig_flag;
            nowselect_moditem = tol_value;
            opnPCchangetool = 1;         // 标记正在进行上位机工具配置
            exit_uartconfig_flag = true; // 退出串口配置界面（如果在的话）
        }
    }

    // 工具写入：#PIVOUSCO#TOL,tool,...#PRS#
    // 1=SSD1306 I2C频率，2=MPU6050 I2C频率+pair，3=HC-05 mode/name/password/action，4=ADC显示模式
    else if (strncmp(cmd_str, "TOL,", 4) == 0)
    {
        char parse_buf[256];
        strncpy(parse_buf, cmd_str + 4, sizeof(parse_buf) - 1);
        parse_buf[sizeof(parse_buf) - 1] = '\0';

        char *tokens[8] = {0};
        int token_count = 0;
        char *ctx = nullptr;
        char *tok = strtok_r(parse_buf, ",", &ctx);
        while (tok && token_count < 8)
        {
            tokens[token_count++] = tok;
            tok = strtok_r(nullptr, ",", &ctx);
        }

        int tool = -1;
        if (token_count > 0)
        {
            parse_int_token(tokens[0], &tool);
        }

        if (tool == 0)
        {
            int action = 0;
            if (token_count > 1 && parse_int_token(tokens[1], &action) && action == 1)
            {
                // Reset A/B count and total count for encoder tool
                extern volatile int32_t a_count_encoder;
                extern volatile int32_t b_count_encoder;
                extern volatile int32_t count_encoder;
                extern volatile bool if_aorb_is_wrong;
                extern volatile int speed_encoder;
                extern volatile int32_t last_count_encoder;
                extern volatile int32_t last_a_count_encoder;
                extern volatile int32_t last_b_count_encoder;
                a_count_encoder = 0;
                b_count_encoder = 0;
                count_encoder = 0;
                if_aorb_is_wrong = 0;
                last_count_encoder = 0;
                last_a_count_encoder = 0;
                last_b_count_encoder = 0;
                extern volatile int speed_data[5];
                for (int i = 0; i < 5; i++)
                {
                    speed_data[i] = 0;
                }
                speed_encoder = 0;
                send_pc_flag = true;
            }
        }
        else if (tool == 1)
        {
            int freq_idx = 0;
            if (token_count > 1 && parse_int_token(tokens[1], &freq_idx) && freq_idx >= 0 && freq_idx <= 3)
            {
                extern int ssd_demo_i2c_sel;
                ssd_demo_i2c_sel = freq_idx;
                const int preset_freqs[] = {100000, 400000, 1000000, 2000000};
                i2c_freq = preset_freqs[freq_idx];

                // 这部分一定是由core0调用
                StartStateLED();
                extern bool i2c_run_in_core1;
                i2c_run_in_core1 = false;
                sleep_ms(100);
                i2c_run_in_core1 = true;
                start_core1_ssd1306_test(true);

                send_pc_flag = true;
            }
        }
        else if (tool == 2)
        {
            int freq_idx = 0;
            int pair = 0;
            if (token_count > 1 && parse_int_token(tokens[1], &freq_idx) && freq_idx >= 0 && freq_idx <= 3)
            {
                extern int mpu_demo_i2c_sel;
                mpu_demo_i2c_sel = freq_idx;
                const int preset_freqs[] = {100000, 400000, 1000000, 2000000};
                i2c_freq = preset_freqs[freq_idx];
                // 这部分一定是由core0调用

                StartStateLED();
                extern bool i2c_run_in_core1;
                i2c_run_in_core1 = false;
                sleep_ms(100);
                i2c_run_in_core1 = true;
                start_core1_mpu6050_test(true);
            }
            if (token_count > 2 && parse_int_token(tokens[2], &pair))
            {
                mpu6050_demo_set_pair_sel_value(pair);
            }
            send_pc_flag = true;
        }
        else if (tool == 3)
        {
            int mode = 0;
            int action = 0;
            const char *name = (token_count > 2) ? tokens[2] : "";
            const char *password = (token_count > 3) ? tokens[3] : "";
            if (token_count > 1)
            {
                parse_int_token(tokens[1], &mode);
            }
            if (token_count > 4)
            {
                parse_int_token(tokens[4], &action);
            }
            // 最后一位配置exit_uartconfig_flag
            if (token_count > 5)
            {
                int exit_flag = 1;
                if (parse_int_token(tokens[5], &exit_flag) && exit_flag == 1)
                {
                    extern bool exit_uartconfig_flag;
                    exit_uartconfig_flag = true; // 退出串口配置界面（如果在的话）
                }
            }

            hc05_set_protocol_config(mode, name, password, action);
            send_pc_flag = true;
        }
        else if (tool == 4)
        {
            int display_mode = 0;
            if (token_count > 1 && parse_int_token(tokens[1], &display_mode))
            {
                adcxy_set_display_mode_code(display_mode);
                send_pc_flag = true;
            }
        }
    }

    // global_TA_serial_mode
    // #PIVOUSCO#UAR,x#PRS#
    else if (strncmp(cmd_str, "UAR,", 4) == 0)
    {
        char *uart_mode_str = (char *)(cmd_str + 4);
        int uart_mode = atoi(uart_mode_str);
        if (uart_mode == 0 || uart_mode == 1)
        {
            extern bool global_TA_serial_mode;
            global_TA_serial_mode = (uart_mode == 1);
            // 更新
            if (global_TA_serial_mode == 0)
            {
                Selector_TA_TX_TB_RX();
            }
            else if (global_TA_serial_mode == 1)
            {
                Selector_TA_RX_TB_TX();
            }
            send_pc_flag = true;
        }
    }

    // LIS证书验证
    else if (strncmp(cmd_str, "LIS,", 4) == 0)
    {
        // printf("[AUTH] Received License Data: %s\r\n", cmd_str + 4);
        const char *hex = cmd_str + 4;
        size_t len = strlen(hex);
        if (len != 32)
        {
            // printf("[AUTH] Invalid LIS length: %zu\r\n", len);
            return;
        }
        uint8_t new_license[18];
        bool hex_valid = true;
        for (int i = 0; i < 16; i++)
        {
            unsigned int byte_val;
            if (sscanf(hex + i * 2, "%02x", &byte_val) == 1)
            {
                new_license[i] = (uint8_t)byte_val;
            }
            else
            {
                hex_valid = false;
                break;
            }
        }
        if (!hex_valid)
        {
            // printf("[AUTH] Invalid LIS payload\r\n");
            return;
        }

        extern uint8_t stored_license_data[16]; // 引用全局存储缓冲区
        extern bool license_data_loaded;        // 引用全局加载标志

        // 复制到新stored_license_data
        memcpy(stored_license_data, new_license, 16);
        license_data_loaded = true;

        return;
    }

    // 目前仅作为占位符，后续可根据 "SYS,0" 或 "I2C,F,400000" 进行解析
    // printf("[PC_CMD] Received: %s\r\n", cmd_str);
}

static void handle_conf_frame_payload(const char *payload)
{
    if (!payload)
    {
        return;
    }

    menu_update_flag = 1;

    stdio_set_driver_enabled(&stdio_usb, true);
    stdio_set_driver_enabled(&stdio_uart, true);
    stdio_filter_driver(&stdio_usb);

    term_index = 0;
    data_index = 0;
    current_configdata[0] = '\0';

    strncpy(current_configdata, payload, CONFIG_DATA_MAX_LEN - 1);
    current_configdata[CONFIG_DATA_MAX_LEN - 1] = '\0';

    if (strncmp(current_configdata, "LIS,", 4) == 0)
    {
        process_pc_command(current_configdata);
        menu_update_flag = 0;
        return;
    }

    // printf("#PIVOUSTX#============ SGate MMC ============ \r\n");
    // printf("Configuration loaded successfully:\r\n");
    // printf("-----------------------------------\r\n#PTS#");
    // printf("#PIVOUSTX#%s\r\n#PTS#", current_configdata);
    // printf("#PIVOUSTX#-----------------------------------\r\n");
    // printf("Now, Press the 'RESTART' button on the back to reboot the device.\r\n");
    // printf("============ SGate MMC ============\r\n#PTS#");

    printf("#PIVOUSTX#配置写入成功，即将自动重启。#PTS#\r\n");

    if_receive_massage = 1;
    sleep_ms(2000);
    while (1)
    {
        tight_loop_contents();
    }
}

static void handle_usco_frame_payload(const char *payload)
{
    if (!payload)
    {
        return;
    }

    process_pc_command(payload);
}

static void handle_usrx_frame_payload(const char *payload, uint32_t now, DirectionState *st, DirectionState *other_st)
{
    if (!payload)
    {
        return;
    }

    (void)now;
    (void)st;
    (void)other_st;
    usb_uart_tx_queue_push_buffer(payload, strlen(payload));
}

// 处理 #PIVOCONF# 帧匹配的函数
// 返回值:
// -1: 确定不匹配（需重新发送缓冲区）
//  0: 正在匹配中（需缓存暂存）
//  1: 匹配成功（进入配置模式）
int handle_pivoconf_match(int c)
{
    if (c == PICO_ERROR_TIMEOUT)
        return 0;

    static const char *HEADER = "#PIVOCONF#";
    static size_t match_idx = 0;
    static uint32_t last_match_time = 0;
    uint32_t now = time_us_32();

    // 超时处理：150ms
    if (match_idx > 0 && (now - last_match_time) >= 150000)
    {
        match_idx = 0;
        return -1; // 超时强行不匹配
    }

    char ch = (char)c;
    if (ch == HEADER[match_idx])
    {
        match_idx++;
        last_match_time = now;
        if (HEADER[match_idx] == '\0')
        {
            match_idx = 0;
            return 1; // 成功
        }
        return 0; // 匹配中
    }
    else
    {
        match_idx = 0; // 只要有一个不匹配就重置
        return -1;     // 不匹配
    }
}

// 状态机核心处理逻辑：输入字符并处理帧匹配
// 返回值:
// -1: 确定不匹配（需重新发送缓冲区）
//  0: 正在匹配中（需缓存暂存）
//  1: 匹配成功（已在内部执行 process_pc_command）
int handle_pc_char_input(int c)
{
    if (c == PICO_ERROR_TIMEOUT)
        return 0;

    static enum {
        IDLE,
        MATCHING_HEADER,
        RECEIVING_DATA,
        MATCHING_FOOTER
    } state = IDLE;

    static char cmd_buffer[256];
    static size_t cmd_idx = 0;

    static const char *HEADER = "#PIVOUSCO#";
    static const char *FOOTER = "#PRS#";
    static size_t match_idx = 0;
    static uint32_t last_activity_time = 0;
    uint32_t now = time_us_32();

    // 协议B超时处理：150ms
    if (state != IDLE && (now - last_activity_time) >= 150000)
    {
        state = IDLE;
        match_idx = 0;
        cmd_idx = 0;
        return -1; // 超时触发回放
    }
    last_activity_time = now;

    char ch = (char)c;

    switch (state)
    {
    case IDLE:
    case MATCHING_HEADER:
        if (ch == HEADER[match_idx])
        {
            state = MATCHING_HEADER;
            match_idx++;
            if (HEADER[match_idx] == '\0')
            {
                state = RECEIVING_DATA;
                cmd_idx = 0;
                match_idx = 0;
            }
            return 0; // 正在匹配帧头
        }
        else
        {
            state = IDLE;
            match_idx = 0;
            return -1; // 帧头匹配失败
        }
        break;

    case RECEIVING_DATA:
        // 检测是否开始匹配帧尾
        if (ch == FOOTER[0])
        {
            state = MATCHING_FOOTER;
            match_idx = 1;
            return 0; // 可能是帧尾开始
        }
        else
        {
            if (cmd_idx < sizeof(cmd_buffer) - 1)
            {
                cmd_buffer[cmd_idx++] = ch;
            }
            return 0; // 正常接收数据
        }
        break;

    case MATCHING_FOOTER:
        if (ch == FOOTER[match_idx])
        {
            match_idx++;
            if (FOOTER[match_idx] == '\0')
            {
                // 帧尾匹配完成
                cmd_buffer[cmd_idx] = '\0';
                process_pc_command(cmd_buffer);
                state = IDLE;
                match_idx = 0;
                return 1; // 匹配成功并处理
            }
            return 0; // 正在匹配帧尾
        }
        else
        {
            // 如果在匹配帧尾过程中失败，说明之前的字符其实是数据
            // 退回到接收数据状态
            state = RECEIVING_DATA;
            // 注意：这里由于需要回放之前的 FOOTER[0...match_idx-1] 到 cmd_buffer，
            // 逻辑上比较复杂。为了简化并在外层统一回放原始流，
            // 我们直接返回 -1 让外层把从 #PIVOUSCO# 开始的所有缓存都吐出去。
            state = IDLE;
            match_idx = 0;
            return -1;
        }
        break;
    }
    return 0;
}

// 状态机核心处理逻辑：输入字符并处理帧匹配 #PIVOICSI# ... #PIS#
// 返回值:
// -1: 确定不匹配（需重新发送缓冲区）
//  0: 正在匹配中（需缓存暂存）
//  1: 匹配成功（已在内部执行 process_pc_command）
int handle_pivoicsi_char_input(int c)
{
    if (c == PICO_ERROR_TIMEOUT)
        return 0;

    static enum {
        IDLE,
        MATCHING_HEADER,
        RECEIVING_DATA,
        MATCHING_FOOTER
    } state = IDLE;

    static char cmd_buffer[256];
    static size_t cmd_idx = 0;

    static const char *HEADER = "#PIVOICSI#";
    static const char *FOOTER = "#PIS#";
    static size_t match_idx = 0;
    static uint32_t last_activity_time = 0;
    uint32_t now = time_us_32();

    // 协议B超时处理：150ms
    if (state != IDLE && (now - last_activity_time) >= 150000)
    {
        state = IDLE;
        match_idx = 0;
        cmd_idx = 0;
        return -1; // 超时触发回放
    }
    last_activity_time = now;

    char ch = (char)c;

    switch (state)
    {
    case IDLE:
    case MATCHING_HEADER:
        if (ch == HEADER[match_idx])
        {
            state = MATCHING_HEADER;
            match_idx++;
            if (HEADER[match_idx] == '\0')
            {
                state = RECEIVING_DATA;
                cmd_idx = 0;
                match_idx = 0;
            }
            return 0; // 正在匹配帧头
        }
        else
        {
            state = IDLE;
            match_idx = 0;
            return -1; // 帧头匹配失败
        }
        break;

    case RECEIVING_DATA:
        // 检测是否开始匹配帧尾
        if (ch == FOOTER[0])
        {
            state = MATCHING_FOOTER;
            match_idx = 1;
            return 0; // 可能是帧尾开始
        }
        else
        {
            if (cmd_idx < sizeof(cmd_buffer) - 1)
            {
                cmd_buffer[cmd_idx++] = ch;
            }
            return 0; // 正常接收数据
        }
        break;

    case MATCHING_FOOTER:
        if (ch == FOOTER[match_idx])
        {
            match_idx++;
            if (FOOTER[match_idx] == '\0')
            {
                // 帧尾匹配完成
                cmd_buffer[cmd_idx] = '\0';
                process_pc_command(cmd_buffer);
                state = IDLE;
                match_idx = 0;
                return 1; // 匹配成功并处理
            }
            return 0; // 正在匹配帧尾
        }
        else
        {
            // 如果在匹配帧尾过程中失败，说明之前的字符其实是数据
            // 退回到接收数据状态
            state = RECEIVING_DATA;
            state = IDLE;
            match_idx = 0;
            return -1;
        }
        break;
    }
    return 0;
}

void handle_pc_information() // 工具类SSD MPU由core0调用
{
    // 发送状态数据
    //  如果有发送标志，则发送当前状态数据
    // // 超过200ms发一次printf("#PIVOUSTX#123456789#PTS#\r\n");
    // static uint32_t last_send_time = 0;
    // uint32_t now = time_us_32();
    // if (send_pc_flag && (now - last_send_time) >= 200000)
    // {
    //     last_send_time = now;
    //     printf("#PIVOUSTX#123456789#PTS#\r\n");
    // }

    if (send_pc_flag && Enable_PC_Interface)
    {

        send_pc_flag = false; // 重置发送标志

        uint8_t pc_infor_buf[256];
        size_t offset = 0;

        const char *header = "#PIVOUSCO#";
        const char *footer = "#PTS#";
        memcpy(pc_infor_buf + offset, header, strlen(header));
        offset += strlen(header);

        // 帧头 #PIVOSTAT# + [状态数据] + #PTT#
        switch (nowselect_function)
        {
        case 0: // pwm/dc in
        {
            extern uint8_t ta_test_mode;
            extern uint8_t tb_test_mode;
            extern float ta_steady_voltage;
            extern float tb_steady_voltage;
            extern float ta_tb_diff_voltage;
            int written = snprintf(reinterpret_cast<char *>(pc_infor_buf + offset), sizeof(pc_infor_buf) - offset,
                                   "GEI,%u,%.2f,%.2f,%.1f,%u,%.2f,%.2f,%.1f,%.1f",
                                   (unsigned)ta_test_mode,
                                   (double)TA_F,
                                   (double)TA_Duty,
                                   (double)ta_steady_voltage,
                                   (unsigned)tb_test_mode,
                                   (double)TB_F,
                                   (double)TB_Duty,
                                   (double)tb_steady_voltage,
                                   (double)ta_tb_diff_voltage);
            if (written > 0)
            {
                offset += (size_t)written;
            }
            break;
        }

        case 1: // pwm/dc out
        {
            int written = snprintf(reinterpret_cast<char *>(pc_infor_buf + offset), sizeof(pc_infor_buf) - offset,
                                   "GEO");
            if (written > 0)
            {
                offset += (size_t)written;
            }

            for (int i = 0; i < 4; ++i)
            {
                if (offset >= sizeof(pc_infor_buf))
                {
                    break;
                }

                written = snprintf(reinterpret_cast<char *>(pc_infor_buf + offset), sizeof(pc_infor_buf) - offset,
                                   ",%u,%.2f,%.4f,%u,%u,%u",
                                   PWMitem[i].open ? 1u : 0u,
                                   (double)*PWMitem[i].frec->param,
                                   (double)*PWMitem[i].duty->param,
                                   (unsigned)PWMitem[i].cfg.output_mode,
                                   (unsigned)PWMitem[i].cfg.run_mode,
                                   (unsigned)PWMitem[i].cfg.cfg_value);
                if (written > 0)
                {
                    offset += (size_t)written;
                }
            }

            // 加入nowselect_pwmoutput_item的信息
            if (offset < sizeof(pc_infor_buf))
            {
                extern int nowselect_pwmoutput_item;
                written = snprintf(reinterpret_cast<char *>(pc_infor_buf + offset), sizeof(pc_infor_buf) - offset,
                                   ",%u",
                                   (unsigned)nowselect_pwmoutput_item);
                if (written > 0)
                {
                    offset += (size_t)written;
                }
            }
            break;
        }

        case 2: // mode tool
        {
            extern bool if_enter_tool;
            if (if_enter_tool)
            {
                switch (nowselect_moditem)
                {
                case 0:
                {
                    extern volatile int32_t a_count_encoder;
                    extern volatile int32_t b_count_encoder;
                    extern volatile int speed_encoder;
                    extern volatile int32_t count_encoder;
                    extern volatile bool if_aorb_is_wrong;
                    // MOD,0,计数A,计数B,速度,总计数
                    // 字段1: 0 = A/B phase encoder
                    // 字段2: a_count_encoder, A通道下降沿累计计数
                    // 字段3: b_count_encoder, B通道下降沿累计计数
                    // 字段4: speed_encoder, 当前速度估计值
                    // 字段5: count_encoder, 总脉冲计数
                    // 字段6: if_aorb_is_wrong, A/B相位是否错误，0=正常 1=错误,如果错误speed_encoder和count_encoder的值无效 但仍展示
                    int written = snprintf(reinterpret_cast<char *>(pc_infor_buf + offset), sizeof(pc_infor_buf) - offset,
                                           "MOD,0,%ld,%ld,%ld,%ld,%u",
                                           (long)a_count_encoder,
                                           (long)b_count_encoder,
                                           (long)speed_encoder,
                                           (long)count_encoder,
                                           (unsigned)if_aorb_is_wrong);
                    if (written > 0)
                    {
                        offset += (size_t)written;
                    }
                    break;
                }

                case 1:
                {
                    int written = snprintf(reinterpret_cast<char *>(pc_infor_buf + offset), sizeof(pc_infor_buf) - offset,
                                           "MOD,1,%d,%d,%d",
                                           ssd1306_demo_get_status_code(),
                                           ssd1306_demo_get_i2c_sel(),
                                           get_screen_refresh_rate());
                    if (written > 0)
                    {
                        offset += (size_t)written;
                    }
                    // 字段1: 1 = SSD1306/1315
                    // 字段2: SSD状态码, 0正常 1无设备 2错误 3测试中
                    // 字段3: I2C频率档位, 0=100k 1=400k 2=1M 3=2M
                    // 字段4: 屏幕刷新率, 单位fps
                    break;
                }

                case 2:
                {
                    int written = snprintf(reinterpret_cast<char *>(pc_infor_buf + offset), sizeof(pc_infor_buf) - offset,
                                           "MOD,2,%d,%d,%d,%d",
                                           mpu6050_demo_get_status_code(),
                                           mpu6050_demo_get_i2c_sel(),
                                           mpu6050_demo_get_pair_sel(),
                                           get_screen_refresh_rate());
                    if (written > 0)
                    {
                        offset += (size_t)written;
                    }
                    // 字段1: 2 = MPU6050
                    // 字段2: MPU状态码, 0正常 1无设备 2错误 3测试中
                    // 字段3: I2C频率档位, 0=100k 1=400k 2=1M 3=2M
                    // 字段4: 姿态配对档位, 0=X-Y 1=Y-Z 2=Z-X
                    // 字段5: 屏幕刷新率, 单位SPS
                    break;
                }

                case 3:
                {
                    int active_baud = 0;
                    uint active_data_bits = 0;
                    uint active_stop_bits = 0;
                    uart_parity_t active_parity = UART_PARITY_NONE;
                    uartconfig_get_local_params(&active_baud, &active_data_bits, &active_stop_bits, &active_parity);

                    extern bool if_uartconfig_running;
                    int written = snprintf(reinterpret_cast<char *>(pc_infor_buf + offset), sizeof(pc_infor_buf) - offset,
                                           "MOD,3,%d,%s,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                                           hc05_get_mode_code(),
                                           hc05_get_name_text_value(),
                                           hc05_get_pass_selection_value(),
                                           keep_host_set,
                                           map_baudrate_to_code(active_baud),
                                           map_databits_to_code(active_data_bits),
                                           map_parity_to_code(active_parity),
                                           map_stopbits_to_code(active_stop_bits),
                                           hc05_get_has_atmode_dev_value(),
                                           hc05_get_s_mode_result_ok_value(),
                                           hc05_get_s_mode_result_fail_value(),
                                           hc05_get_ms_s_result_ok_value(),
                                           hc05_get_ms_s_result_fail_value(),
                                           hc05_get_ms_m_result_ok_value(),
                                           hc05_get_ms_m_result_fail_value(),
                                           if_uartconfig_running);
                    if (written > 0)
                    {
                        offset += (size_t)written;
                    }
                    // 字段1: 3 = HC-05
                    // 字段2: 工作模式, 0=S 模式 1=M&S 模式
                    // 字段3: 名称字符串
                    // 字段4: 密码字符串
                    // 字段5: 是否保持主机设置, 0=否 1=是
                    // 字段6: 波特率档位码, 对应 core.cpp 的 map_baudrate_to_code
                    // 字段7: 数据位档位码, 0=5 1=6 2=7 3=8
                    // 字段8: 校验位档位码, 0=无 1=奇 2=偶
                    // 字段9: 停止位档位码, 0=1 1=2
                    // 字段10: 当前是否检测到 AT 模式设备, 0=否 1=是
                    // 字段11: S模式配置成功脉冲, 成功后2秒内=1，之后自动回0
                    // 字段12: S模式配置失败脉冲, 失败后2秒内=1，之后自动回0
                    // 字段13: M&S中从机S配置成功脉冲, 成功后2秒内=1，之后自动回0
                    // 字段14: M&S中从机S配置失败脉冲, 失败后2秒内=1，之后自动回0
                    // 字段15: M&S中主机M配置成功脉冲, 成功后2秒内=1，之后自动回0
                    // 字段16: M&S中主机M配置失败脉冲, 失败后2秒内=1，之后自动回0
                    break;
                }

                case 4:
                {
                    int active_baud = 0;
                    uint active_data_bits = 0;
                    uint active_stop_bits = 0;
                    uart_parity_t active_parity = UART_PARITY_NONE;
                    uartconfig_get_local_params(&active_baud, &active_data_bits, &active_stop_bits, &active_parity);

                    int written = snprintf(reinterpret_cast<char *>(pc_infor_buf + offset), sizeof(pc_infor_buf) - offset,
                                           "MOD,4,%ld,%ld,%d,%d,%d,%d,%d,%d",
                                           (long)adcxy_tx_x,
                                           (long)adcxy_tx_y,
                                           adcxy_get_display_mode_code(),
                                           keep_host_set,
                                           map_baudrate_to_code(active_baud),
                                           map_databits_to_code(active_data_bits),
                                           map_parity_to_code(active_parity),
                                           map_stopbits_to_code(active_stop_bits));

                    if (written > 0)
                    {
                        offset += (size_t)written;
                    }
                    // 字段1: 4 = ADC 摇杆
                    // 字段2: adcxy_tx_x, 当前发送的 X 坐标
                    // 字段3: adcxy_tx_y, 当前发送的 Y 坐标
                    // 字段4: 显示模式, 0=原始 1=归零
                    // 字段5: 是否保持主机设置, 0=否 1=是
                    // 字段6: 波特率档位码, 对应 core.cpp 的 map_baudrate_to_code
                    // 字段7: 数据位档位码, 0=5 1=6 2=7 3=8
                    // 字段8: 校验位档位码, 0=无 1=奇 2=偶
                    // 字段9: 停止位档位码, 0=1 1=2
                    break;
                }

                default:
                    break;
                }
            }
            else
            {
                // 未进入工具，仅提示当前选择的工具类型
                int written = snprintf(reinterpret_cast<char *>(pc_infor_buf + offset), sizeof(pc_infor_buf) - offset,
                                       "MOB,%d", // MOB区分与MOD，表示仅提示当前工具
                                       nowselect_moditem);
                if (written > 0)
                {
                    offset += (size_t)written;
                }
            }
            break;
        }
        case 3:
        { // 仅提示I2C
            int written_i2c = snprintf(reinterpret_cast<char *>(pc_infor_buf + offset), sizeof(pc_infor_buf) - offset,
                                       "I2C");
            if (written_i2c > 0)
            {
                offset += (size_t)written_i2c;
            }
            break;
        }
        case 4:
        { // SPI
            int written_spi = snprintf(reinterpret_cast<char *>(pc_infor_buf + offset), sizeof(pc_infor_buf) - offset,
                                       "SPI");
            if (written_spi > 0)
            {
                offset += (size_t)written_spi;
            }
            break;
        }
        case 5:
        { // UART

            send_pc_flag = false; // 重置发送标志
            int active_baud = 0;
            uint active_data_bits = 0;
            uint active_stop_bits = 0;
            uart_parity_t active_parity = UART_PARITY_NONE;
            uartconfig_get_local_params(&active_baud, &active_data_bits, &active_stop_bits, &active_parity);
            extern bool global_TA_serial_mode;

            int written_uart = snprintf(reinterpret_cast<char *>(pc_infor_buf + offset), sizeof(pc_infor_buf) - offset,
                                        "UAR,%u,%d,%d,%d,%d,%d",
                                        global_TA_serial_mode,
                                        keep_host_set,
                                        map_baudrate_to_code(active_baud),
                                        map_databits_to_code(active_data_bits),
                                        map_parity_to_code(active_parity),
                                        map_stopbits_to_code(active_stop_bits));
            if (written_uart > 0)
            {
                offset += (size_t)written_uart;
            }
            break;
        }
        case 6:
        { // AI 测试（AIT 周期状态，供上位机 AI 页回显）
            int written_ai = snprintf(reinterpret_cast<char *>(pc_infor_buf + offset), sizeof(pc_infor_buf) - offset,
                                      "AIT,%s,%u,%u",
                                      ai_state_str(),
                                      (unsigned)ai_code_len(),
                                      (unsigned)ai_run_line());
            if (written_ai > 0)
            {
                offset += (size_t)written_ai;
            }
            break;
        }
        case 7:
        { // MEU
            int written_meu = snprintf(reinterpret_cast<char *>(pc_infor_buf + offset), sizeof(pc_infor_buf) - offset,
                                       "MEU");
            if (written_meu > 0)
            {
                offset += (size_t)written_meu;
            }
            break;
        }
        default:
            break;
        }

        memcpy(pc_infor_buf + offset, footer, strlen(footer));
        offset += strlen(footer);

        // 发送到USB（如果启用）
        printf("%.*s\r\n", (int)offset, pc_infor_buf);
    }
}

void handle_pc_infotmation_ssddemo() // i2cdemo专用，仅由core1调用
{
    char ssddemo_buf[128];
    extern int i2ctest_global_i2c_freq_selection;
    int written = snprintf(ssddemo_buf, sizeof(ssddemo_buf), "#PIVOUSCO#I2C,1,%d,%d#PTS#",
                           ssd1306_demo_get_status_code(),
                           i2ctest_global_i2c_freq_selection);

    // 字段1: 1 = SSD1306/1315
    // 字段2: SSD状态码, 0正常 1无设备 2错误 3测试中
    // 字段3: I2C频率档位, 0=100k 1=400k 2=1M 3=2M
    printf("%s\r\n", ssddemo_buf);
}

void handle_pc_infotmation_mpudemo() // i2cdemo专用，仅由core1调用
{
    char mpudemo_buf[128];
    extern int i2ctest_global_i2c_freq_selection;
    int written = snprintf(mpudemo_buf, sizeof(mpudemo_buf), "#PIVOUSCO#I2C,2,%d,%d#PTS#",
                           mpu6050_demo_get_status_code(),
                           i2ctest_global_i2c_freq_selection);
    // 字段1: 2 = MPU6050
    // 字段2: MPU状态码, 0正常 1无设备 2错误 3测试中
    // 字段3: I2C频率档位, 0=100k 1=400k 2=1M 3=2M
    printf("%s\r\n", mpudemo_buf);
}

// 供外部周期性调用的非阻塞封装
void handle_pc_interface_commands()
{
    // 处理接收数据
    int c;
    // 持续读取 USB FIFO 中的所有可用数据并处理
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT)
    {
        handle_pc_char_input(c);
    }
}

bool ReadInCore1_nonblock()
{

    // 清空UART缓存区
    while (uart_is_readable(UART_ID))
    {
        uart_getc(UART_ID);
    } // 清空接收缓存

    stdio_set_driver_enabled(&stdio_usb, true);
    stdio_set_driver_enabled(&stdio_uart, true);

    while (getchar_timeout_us(10) != PICO_ERROR_TIMEOUT)
        tight_loop_contents();

    sleep_ms(10); // 等待缓冲区数据稳定

    enum UsbFrameMode
    {
        USB_FRAME_IDLE = 0,
        USB_FRAME_CONF,
        USB_FRAME_USCO,
        USB_FRAME_USRX
    };

    static const char *USB_CONF_HEADER = "#PIVOCONF#";
    static const char *USB_CONF_FOOTER = "#END_CONFIG#";
    static const char *USB_USCO_HEADER = "#PIVOUSCO#";
    static const char *USB_USCO_FOOTER = "#PRS#";
    static const char *USB_USRX_HEADER = "#PIVOUSRX#";
    static const char *USB_USRX_FOOTER = "#RXS#";

    size_t conf_header_idx = 0;
    size_t usco_header_idx = 0;
    size_t usrx_header_idx = 0;
    size_t footer_idx = 0;
    UsbFrameMode usb_frame_mode = USB_FRAME_IDLE;
    char usb_frame_payload[CONFIG_DATA_MAX_LEN] = {0};
    size_t usb_frame_payload_idx = 0;

    auto reset_usb_frame = [&]()
    {
        usb_frame_mode = USB_FRAME_IDLE;
        footer_idx = 0;
        usb_frame_payload_idx = 0;
        usb_frame_payload[0] = '\0';
    };

    auto append_usb_payload = [&](char ch)
    {
        if (usb_frame_payload_idx + 1 < sizeof(usb_frame_payload))
        {
            usb_frame_payload[usb_frame_payload_idx++] = ch;
            usb_frame_payload[usb_frame_payload_idx] = '\0';
        }
    };

    auto update_header_match = [&](size_t &idx, const char *header, char ch) -> bool
    {
        if (ch == header[idx])
        {
            idx++;
            if (header[idx] == '\0')
            {
                idx = 0;
                return true;
            }
            return false;
        }

        idx = (ch == header[0]) ? 1 : 0;
        return false;
    };

    auto handle_usb_frame_char = [&](char ch, uint32_t now_ts, DirectionState *st, DirectionState *other_st)
    {
        if (usb_frame_mode == USB_FRAME_IDLE)
        {
            bool matched_conf = update_header_match(conf_header_idx, USB_CONF_HEADER, ch);
            bool matched_usco = update_header_match(usco_header_idx, USB_USCO_HEADER, ch);
            bool matched_usrx = update_header_match(usrx_header_idx, USB_USRX_HEADER, ch);

            if (matched_conf)
            {
                usb_frame_mode = USB_FRAME_CONF;
                usb_frame_payload_idx = 0;
                usb_frame_payload[0] = '\0';
                footer_idx = 0;
                usco_header_idx = 0;
                usrx_header_idx = 0;
                return true;
            }
            if (matched_usco)
            {
                usb_frame_mode = USB_FRAME_USCO;
                usb_frame_payload_idx = 0;
                usb_frame_payload[0] = '\0';
                footer_idx = 0;
                conf_header_idx = 0;
                usrx_header_idx = 0;
                return true;
            }
            if (matched_usrx)
            {
                usb_frame_mode = USB_FRAME_USRX;
                usb_frame_payload_idx = 0;
                usb_frame_payload[0] = '\0';
                footer_idx = 0;
                conf_header_idx = 0;
                usco_header_idx = 0;
                return true;
            }

            return false;
        }

        const char *footer = (usb_frame_mode == USB_FRAME_CONF) ? USB_CONF_FOOTER : (usb_frame_mode == USB_FRAME_USCO) ? USB_USCO_FOOTER
                                                                                                                       : USB_USRX_FOOTER;

        if (ch == footer[footer_idx])
        {
            footer_idx++;
            if (footer[footer_idx] == '\0')
            {
                usb_frame_payload[usb_frame_payload_idx] = '\0';
                switch (usb_frame_mode)
                {
                case USB_FRAME_CONF:
                    handle_conf_frame_payload(usb_frame_payload);
                    break;
                case USB_FRAME_USCO:
                    handle_usco_frame_payload(usb_frame_payload);
                    break;
                case USB_FRAME_USRX:
                    handle_usrx_frame_payload(usb_frame_payload, now_ts, st, other_st);
                    break;
                default:
                    break;
                }

                reset_usb_frame();
                return true;
            }
            return true;
        }

        if (footer_idx > 0)
        {
            for (size_t i = 0; i < footer_idx; ++i)
            {
                append_usb_payload(footer[i]);
            }
            footer_idx = 0;
        }

        append_usb_payload(ch);
        return true;
    };

    while (core1_usb_connect_uart) // I2Ctest会将通信关闭
    {

        // printf("Waiting for configuration data from USB or UART...\r\n"); ///////////////////////////////////

        stdio_filter_driver(&stdio_usb);

        // Allow core loops to attempt sending pending select voltage while draining
        Key_TrySendSelectVoltage();

        SSD1306_PrintBufRaw();

        // 在此实现UART USB的相互透传和数据记录到缓存区
        DirectionState *st = (now_read_direction == 0) ? &dir_out_state : &dir_in_state;
        DirectionState *other_st = (now_read_direction == 0) ? &dir_in_state : &dir_out_state;
        uint32_t now = time_us_32();

        // USB到UART透传（实时传输，无方向符号）
        int usb_char = getchar_timeout_us(0);

        if (usb_char != PICO_ERROR_TIMEOUT)
        {
            if (handle_usb_frame_char((char)usb_char, now, st, other_st))
            {
                StartStateLED();
            }
        }

        // Drain a bounded amount per loop to keep USB path responsive.
        usb_uart_tx_queue_drain(now, st, other_st, 64);

        stdio_filter_driver(&stdio_uart);

        // UART到USB透传（实时传输，无方向符号）
        bool uart_data_started = false;
        uint16_t burst_count = 0;
        while (uart_is_readable(UART_ID))
        {
            char uart_char = uart_getc(UART_ID);

            stdio_filter_driver(&stdio_usb);

            // 如果开启了上位机接口，在数据块开始处插入帧头
            if (Enable_PC_Interface && !uart_data_started)
            {
                printf("#PIVOUSTX#");
            }

            // 直接将UART接收的数据发送到USB（实时透传，无符号）
            putchar(uart_char);
            RXisReady = 1;
            uart_data_started = true;

            // 仅在缓冲区记录时添加方向符号（用于监视器显示）
            process_received_char(now, st, other_st, 1, uart_char);
            StartStateLED();

            // 如果连续超过1024字符则强制退出，防止阻塞核心1太久
            if (++burst_count >= 1024)
                break;
        }

        // 如果发生了数据传输，在数据块结束处插入帧尾并刷新
        if (Enable_PC_Interface && uart_data_started)
        {
            printf("#PTS#");
            fflush(stdout);
        }

        // 超时或达到定长触发方向切换

        if (other_st->sentence_length > 0)
        {
            // 超时分行
            if (now - st->last_tick_us >= LINE_TIMEOUT_US)
            {
                other_st->last_tick_us = now;
                switch_read_direction(st, other_st);
            }
            // 定长分行
            else if (other_st->sentence_length >= LINE_MAX_LEN)
            {
                other_st->last_tick_us = now;
                switch_read_direction(st, other_st);
            }
        }

        if (core1_need_wait)
        {
            // Menu_connection();       //;启用菜单控制方向
            stdio_set_driver_enabled(&stdio_usb, true);
            stdio_set_driver_enabled(&stdio_uart, true);
            if ((menusetting_UARTA || menusetting_UARTB))
            {
                stdio_filter_driver(&stdio_uart);

                // putchar发送到UART
                for (int i = 0; i < strlen(backString); i++)
                {
                    usb_uart_tx_queue_push(backString[i]);
                }
            }
            if (menusetting_USB)
            {
                stdio_filter_driver(&stdio_usb);
                if (Enable_PC_Interface) //;如果启用了上位机接口，发送特殊标志
                {
                    printf("#PIVOUSTX#"); //;发送特殊标志通知主循环已完成连接设置
                    printf("%s", backString);
                    printf("#PTS#");
                }
                else
                { // 普通模式下直接发送
                    printf("%s", backString);
                }
            }

            core1_need_wait = 0;
        }

        if (send_pc_flag && nowselect_function == 2 && nowselect_moditem == 4) // ADC摇杆模式在此发送数据
        {

            int16_t x = adcxy_tx_x;
            int16_t y = adcxy_tx_y;
            adcxy_tx_pending = false;

            stdio_set_driver_enabled(&stdio_usb, true);
            stdio_set_driver_enabled(&stdio_uart, true);

            stdio_filter_driver(&stdio_usb);

            if (Enable_PC_Interface)
            {
                int active_baud = 0;
                uint active_data_bits = 0;
                uint active_stop_bits = 0;
                uart_parity_t active_parity = UART_PARITY_NONE;
                char adcString[128];
                uartconfig_get_local_params(&active_baud, &active_data_bits, &active_stop_bits, &active_parity);

                int written = snprintf(adcString, sizeof(adcString),
                                       "#PIVOUSCO#MOD,4,%ld,%ld,%d,%d,%d,%d,%d,%d#PTS#", // /////////////////////////////////////
                                       (long)adcxy_tx_x,
                                       (long)adcxy_tx_y,
                                       adcxy_get_display_mode_code(),
                                       keep_host_set,
                                       map_baudrate_to_code(active_baud),
                                       map_databits_to_code(active_data_bits),
                                       map_parity_to_code(active_parity),
                                       map_stopbits_to_code(active_stop_bits));

                printf("%s", adcString);
                // 字段1: 4 = ADC 摇杆
                // 字段2: adcxy_tx_x, 当前发送的 X 坐标
                // 字段3: adcxy_tx_y, 当前发送的 Y 坐标
                // 字段4: 显示模式, 0=原始 1=归零
                // 字段5: 是否保持主机设置, 0=否 1=是
                // 字段6: 波特率档位码, 对应 core.cpp 的 map_baudrate_to_code
                // 字段7: 数据位档位码, 0=5 1=6 2=7 3=8
                // 字段8: 校验位档位码, 0=无 1=奇 2=偶
                // 字段9: 停止位档位码, 0=1 1=2
            }
            else
            {
                printf("x=%+d,y=%+d;\r\n", x, y);
            }

            stdio_filter_driver(&stdio_uart); // 发送 x= ,y= ;
            char uart_buf[64];
            int uart_written = snprintf(uart_buf, sizeof(uart_buf), "x=%+d,y=%+d;\r\n", x, y);
            for (int i = 0; i < uart_written; i++)
            {
                usb_uart_tx_queue_push(uart_buf[i]);
            }

            stdio_filter_driver(&stdio_usb); // 发送 x= ,y= ;

            send_pc_flag = false; // 重置发送标志
        }

        if (send_pc_flag && nowselect_function == 5 && Enable_PC_Interface)
        { // UART模式在此发送数据

            stdio_filter_driver(&stdio_usb);
            send_pc_flag = false; // 重置发送标志
            int active_baud = 0;
            uint active_data_bits = 0;
            uint active_stop_bits = 0;
            uart_parity_t active_parity = UART_PARITY_NONE;
            char uartString[128];
            uartconfig_get_local_params(&active_baud, &active_data_bits, &active_stop_bits, &active_parity);

            extern bool global_TA_serial_mode;
            int written = snprintf(uartString, sizeof(uartString),
                                   "#PIVOUSCO#UAR,%u,%d,%d,%d,%d,%d#PTS#", // //////////////////////////////////////
                                   global_TA_serial_mode,
                                   keep_host_set,
                                   map_baudrate_to_code(active_baud),
                                   map_databits_to_code(active_data_bits),
                                   map_parity_to_code(active_parity),
                                   map_stopbits_to_code(active_stop_bits));

            printf("%s", uartString);
        }
        else if (send_pc_flag && nowselect_function == 7 && Enable_PC_Interface) // 自定义菜单（原 6 顺延）
        {
            stdio_filter_driver(&stdio_usb);
            send_pc_flag = false; // 重置发送标志
            int active_baud = 0;
            uint active_data_bits = 0;
            uint active_stop_bits = 0;
            uart_parity_t active_parity = UART_PARITY_NONE;
            char uartString[128];
            uartconfig_get_local_params(&active_baud, &active_data_bits, &active_stop_bits, &active_parity);

            extern bool global_TA_serial_mode;
            int written = snprintf(uartString, sizeof(uartString),
                                   "#PIVOUSCO#MEU,%u,%d,%d,%d,%d,%d#PTS#", // //////////////////////////////////////
                                   global_TA_serial_mode,
                                   keep_host_set,
                                   map_baudrate_to_code(active_baud),
                                   map_databits_to_code(active_data_bits),
                                   map_parity_to_code(active_parity),
                                   map_stopbits_to_code(active_stop_bits));

            printf("%s", uartString);
        }

        // 后发送菜单数据
        if (Enable_PC_Interface && if_need_send_menu_data)
        {

            stdio_filter_driver(&stdio_usb);
            char send_buffer[CONFIG_DATA_MAX_LEN] = {0};
            int offset = 0;
            load_configdata_to_sram(send_buffer);

            // #PIVOCODA#data#PDS#
            printf("#PIVOCODA#");
            printf("%s", send_buffer);
            printf("#PDS#");
            if_need_send_menu_data = 0;
        }
        if (Enable_PC_Interface && if_need_send_menu_settings)
        {
            stdio_filter_driver(&stdio_usb);

            extern char current_configsettings[CONFIG_DATA_MAX_LEN];
            // #PIVOCODS#data#PDS#
            printf("#PIVOCOSE#");
            printf("%s", current_configsettings);
            printf("#PSS#");
            if_need_send_menu_settings = 0;
        }
        // stdio_filter_driver(&stdio_usb);
        // printf("Core1"); ////////////////////////////
    }

    // stdio_filter_driver(NULL);
    // stdio_set_driver_enabled(&stdio_usb, true);
    // stdio_set_driver_enabled(&stdio_uart, true);

    return 0;
}

extern int32_t i2c_freq;
static bool i2ctest_master_mode = true;             // Enter 切换 1Master/0Monitor
constexpr uint32_t I2C_CMD_IDLE_TIMEOUT_US = 10000; // 空闲超时自动解析

// SPI 参数
bool spi_run_in_core1 = false;
bool spi_master_mode = true;
uint8_t spi_bits_val = 8;
uint8_t spi_cpol_val = 0;
uint8_t spi_cpha_val = 0;

// void update_i2c_init()
// {
//     if (i2ctest_master_mode)
//     {
//         test_configure_i2c_pins(i2c_freq);
//     }
//     else
//     {
//         // 监听模式初始化
//     }
// }

// Core1 I2C测试主循环
bool prev_present[128] = {false}; // I2C设备状态 地址0x03~0x77

void core1_i2c_connect_USB()
{
    // 初始化USB与I2C
    if (test_communication_mode)
    {
        test_configure_communication_i2c_init();
    }

    stdio_set_driver_enabled(&stdio_usb, true);
    stdio_set_driver_enabled(&stdio_uart, true);

    while (getchar_timeout_us(10) != PICO_ERROR_TIMEOUT) // Use a small timeout to allow proper clearing
    {
        tight_loop_contents(); // Optional: Add this to prevent the watchdog timer from triggering
    }

    static char line[512];
    static int li = 0;
    uint32_t last_rx_tick = time_us_32();
    bool first_entry = true;
    static enum {
        ICSI_IDLE,
        ICSI_MATCHING_HEADER,
        ICSI_RECEIVING_DATA,
        ICSI_MATCHING_FOOTER
    } icsi_state = ICSI_IDLE;
    static enum {
        FRAME_NONE,
        FRAME_USCO,
        FRAME_ICSI,
        FRAME_BOTH
    } frame_mode = FRAME_NONE;
    static char icsi_buffer[256];
    static size_t icsi_idx = 0;
    static size_t icsi_match_idx = 0;
    static uint32_t icsi_last_time = 0;

    auto parse_number = [](const char *s, uint32_t *out, int base = 16) -> bool
    {
        if (!s || !*s)
            return false;
        char *end = nullptr;
        unsigned long v = strtoul(s, &end, base);
        if (end == s)
            return false;
        *out = (uint32_t)v;
        return true;
    };

    auto to_upper = [](char *s)
    { for (; *s; ++s) { if (*s >= 'a' && *s <= 'z') *s -= 32; } };

    auto tokenize = [](char *s, char *argv[], int max) -> int
    {
        int n = 0;
        char *p = s;
        while (*p && n < max)
        {
            while (*p == ' ' || *p == '\t')
                ++p;
            if (!*p)
                break;
            argv[n++] = p;
            while (*p && *p != ' ' && *p != '\t')
                ++p;
            if (!*p)
                break;
            *p++ = '\0';
        }
        return n;
    };

    auto handle_icsi_char = [&](int c) -> int
    {
        if (c == PICO_ERROR_TIMEOUT)
            return 0;

        static const char *HEADER = "#PIVOICSI#";
        static const char *FOOTER = "#PIS#";
        uint32_t now = time_us_32();

        if (icsi_state != ICSI_IDLE && (now - icsi_last_time) >= 150000)
        {
            icsi_state = ICSI_IDLE;
            icsi_match_idx = 0;
            icsi_idx = 0;
            return -1;
        }
        icsi_last_time = now;

        char ch = (char)c;

        switch (icsi_state)
        {
        case ICSI_IDLE:
        case ICSI_MATCHING_HEADER:
            if (ch == HEADER[icsi_match_idx])
            {
                icsi_state = ICSI_MATCHING_HEADER;
                icsi_match_idx++;
                if (HEADER[icsi_match_idx] == '\0')
                {
                    icsi_state = ICSI_RECEIVING_DATA;
                    icsi_idx = 0;
                    icsi_match_idx = 0;
                }
                return 0;
            }
            icsi_state = ICSI_IDLE;
            icsi_match_idx = 0;
            return -1;

        case ICSI_RECEIVING_DATA:
            if (ch == FOOTER[0])
            {
                icsi_state = ICSI_MATCHING_FOOTER;
                icsi_match_idx = 1;
                return 0;
            }
            if (icsi_idx < sizeof(icsi_buffer) - 1)
            {
                icsi_buffer[icsi_idx++] = ch;
                icsi_buffer[icsi_idx] = '\0';
            }
            return 0;

        case ICSI_MATCHING_FOOTER:
            if (ch == FOOTER[icsi_match_idx])
            {
                icsi_match_idx++;
                if (FOOTER[icsi_match_idx] == '\0')
                {
                    icsi_buffer[icsi_idx] = '\0';
                    icsi_state = ICSI_IDLE;
                    icsi_match_idx = 0;
                    return 1;
                }
                return 0;
            }
            icsi_state = ICSI_IDLE;
            icsi_match_idx = 0;
            return -1;
        }
        return 0;
    };

    auto handle_command = [&]()
    {
        if (li == 0)
            return;

        line[li] = '\0';
        char *argv[32];
        char line_copy[512];
        strcpy(line_copy, line);
        int argc = tokenize(line_copy, argv, 32);
        li = 0;

        if (argc > 0)
        {
            // Helper: detect tokens that start a new command
            auto is_command_token = [](const char *t) -> bool
            {
                if (!t || !*t)
                    return false;
                // Known commands (uppercase expected)
                return (strcmp(t, "L") == 0) || (strcmp(t, "LIST") == 0) ||
                       (strcmp(t, "R") == 0) || (strcmp(t, "READ") == 0) || (strcmp(t, "RD") == 0) ||
                       (strcmp(t, "W") == 0) || (strcmp(t, "WRITE") == 0) || (strcmp(t, "WR") == 0) ||
                       (strcmp(t, "BAUD") == 0) || (strcmp(t, "F") == 0) ||
                       (strcmp(t, "FREQ") == 0);
            };

            // Process tokens sequentially so a line can contain multiple commands:
            for (int idx = 0; idx < argc;)
            {
                to_upper(argv[idx]); // normalize command token
                char *cmd = argv[idx++];

                // sniffer模式切换 暂时转移至专用模式
                // if (strcmp(cmd, "M") == 0)
                // {
                //     i2ctest_master_mode = true;
                //     printf("MODE: MASTER\r\n");
                //     continue;
                // }
                // else if (strcmp(cmd, "S") == 0)
                // {
                //     i2ctest_master_mode = false;
                //     printf("MODE: SLAVE (monitor)\r\n");
                //     continue;
                // }

                // If not master and command is not mode switch -> report monitor active and stop processing further commands
                // if (!i2ctest_master_mode)
                // {
                //     char temp_buf[64];
                //     snprintf(temp_buf, sizeof(temp_buf), "SLAVE MONITOR ACTIVE\r\n");
                //     core1_i2c_monitor_append(temp_buf);
                //     printf("%s", temp_buf);
                //     break;
                // }

                if (strcmp(cmd, "L") == 0 || strcmp(cmd, "LIST") == 0)
                {
                    // existing LIST behavior (reads prev_present snapshot)
                    bool any_found = false;
                    char temp_buf0[256];
                    uint8_t dev_num = 0;
                    for (int addr = 0x03; addr <= 0x77; ++addr)
                    {
                        if (prev_present[addr])
                        {
                            any_found = true;
                            dev_num++;
                        }
                    }
                    if (!any_found)
                    {
                        snprintf(temp_buf0, sizeof(temp_buf0), "[DONE] NO I2C DEV\r\n");
                        core1_i2c_monitor_append(temp_buf0);
                        i2c_pc_print_begin();
                        printf("%s", temp_buf0);
                        i2c_pc_print_end();
                    }
                    else
                    {
                        snprintf(temp_buf0, sizeof(temp_buf0), "[DONE] I2C DEV(%d): ", dev_num);
                        core1_i2c_monitor_append(temp_buf0);
                        i2c_pc_print_begin();
                        printf("%s", temp_buf0);
                        for (int addr = 0x03; addr <= 0x77; ++addr)
                        {
                            if (prev_present[addr])
                            {
                                char temp_buf[64];
                                snprintf(temp_buf, sizeof(temp_buf), "0x%02X ", addr);
                                core1_i2c_monitor_append(temp_buf);
                                printf("%s", temp_buf);
                            }
                        }
                        char temp_buf2[16] = "\r\n";
                        core1_i2c_monitor_append(temp_buf2);
                        printf("%s", temp_buf2);
                        i2c_pc_print_end();
                    }
                    continue;
                }
                else if (strcmp(cmd, "R") == 0 || strcmp(cmd, "READ") == 0 || strcmp(cmd, "RD") == 0)
                {
                    // Support both: R DEV REG LEN  (read from register)
                    // and: R DEV LEN  (direct read from device without register)
                    int remaining = argc - idx;
                    bool has_next_command = (remaining > 2) && is_command_token(argv[idx + 2]);

                    if (remaining >= 3 && !has_next_command)
                    {
                        uint32_t dev = 0, reg = 0, len = 0;
                        if (parse_number(argv[idx++], &dev) && parse_number(argv[idx++], &reg) && parse_number(argv[idx++], &len, 10))
                        {
                            uint8_t r = (uint8_t)reg;
                            int wr = i2c_write_timeout_us(TEST_I2C_PORT, (uint8_t)dev, &r, 1, true, 1000);
                            if (wr == PICO_ERROR_TIMEOUT)
                            {
                                i2c_deinit(TEST_I2C_PORT);
                                test_configure_i2c_pins(i2c_freq);
                                wr = i2c_write_timeout_us(TEST_I2C_PORT, (uint8_t)dev, &r, 1, true, 1000);
                            }
                            if (wr >= 0)
                            {
                                uint8_t buf[64];
                                if (len > sizeof(buf))
                                    len = sizeof(buf);
                                int rr = i2c_read_timeout_us(TEST_I2C_PORT, (uint8_t)dev, buf, (size_t)len, false, 1000);
                                if (rr == PICO_ERROR_TIMEOUT)
                                {
                                    i2c_deinit(TEST_I2C_PORT);
                                    test_configure_i2c_pins(i2c_freq);
                                    rr = i2c_read_timeout_us(TEST_I2C_PORT, (uint8_t)dev, buf, (size_t)len, false, 1000);
                                }
                                if (rr >= 0)
                                {
                                    char temp_buf[256];
                                    snprintf(temp_buf, sizeof(temp_buf), "[SUCCESS] READ %u BYTES FROM 0x%02X REG 0x%02X: ", (unsigned)len, (unsigned)dev, (unsigned)reg);
                                    core1_i2c_monitor_append(temp_buf);
                                    i2c_pc_print_begin();
                                    printf("%s", temp_buf);
                                    for (uint32_t i = 0; i < len; ++i)
                                    {
                                        snprintf(temp_buf, sizeof(temp_buf), "0x%02X ", buf[i]);
                                        core1_i2c_monitor_append(temp_buf);
                                        printf("%s", temp_buf);
                                    }
                                    char temp_buf2[16] = "\r\n";
                                    core1_i2c_monitor_append(temp_buf2);
                                    printf("%s", temp_buf2);
                                    i2c_pc_print_end();
                                    StartStateLED();
                                }
                                else
                                {
                                    char temp_buf[256];
                                    snprintf(temp_buf, sizeof(temp_buf), "[ERROR] READ FAIL < %s\r\n", line);
                                    core1_i2c_monitor_append(temp_buf);
                                    i2c_pc_print_begin();
                                    printf("%s", temp_buf);
                                    i2c_pc_print_end();
                                }
                            }
                            else
                            {
                                char temp_buf[256];
                                snprintf(temp_buf, sizeof(temp_buf), "[ERROR] WRITE REG FAIL < %s\r\n", line);
                                core1_i2c_monitor_append(temp_buf);
                                i2c_pc_print_begin();
                                printf("%s", temp_buf);
                                i2c_pc_print_end();
                            }
                        }
                        else
                        {
                            char temp_buf[256];
                            snprintf(temp_buf, sizeof(temp_buf), "[WRONG] USAGE: R [DEV(HEX)] [REG(HEX)] [LEN(DEC)] or R [DEV(HEX)] [LEN(DEC)] < %s\r\n", line);
                            core1_i2c_monitor_append(temp_buf);
                            i2c_pc_print_begin();
                            printf("%s", temp_buf);
                            i2c_pc_print_end();
                        }
                    }
                    else if (remaining >= 2)
                    {
                        uint32_t dev = 0, len = 0;
                        if (parse_number(argv[idx++], &dev) && parse_number(argv[idx++], &len, 10))
                        {
                            uint8_t buf[64];
                            if (len > sizeof(buf))
                                len = sizeof(buf);
                            int rr = i2c_read_timeout_us(TEST_I2C_PORT, (uint8_t)dev, buf, (size_t)len, false, 1000);
                            if (rr == PICO_ERROR_TIMEOUT)
                            {
                                i2c_deinit(TEST_I2C_PORT);
                                test_configure_i2c_pins(i2c_freq);
                                rr = i2c_read_timeout_us(TEST_I2C_PORT, (uint8_t)dev, buf, (size_t)len, false, 1000);
                            }
                            if (rr >= 0)
                            {
                                char temp_buf[256];
                                snprintf(temp_buf, sizeof(temp_buf), "[SUCCESS] READ %u BYTES FROM 0x%02X: ", (unsigned)len, (unsigned)dev);
                                core1_i2c_monitor_append(temp_buf);
                                i2c_pc_print_begin();
                                printf("%s", temp_buf);
                                for (uint32_t i = 0; i < len; ++i)
                                {
                                    snprintf(temp_buf, sizeof(temp_buf), "0x%02X ", buf[i]);
                                    core1_i2c_monitor_append(temp_buf);
                                    printf("%s", temp_buf);
                                }
                                char temp_buf2[16] = "\r\n";
                                core1_i2c_monitor_append(temp_buf2);
                                printf("%s", temp_buf2);
                                i2c_pc_print_end();
                                StartStateLED();
                            }
                            else
                            {
                                char temp_buf[256];
                                snprintf(temp_buf, sizeof(temp_buf), "[ERROR] READ FAIL < %s\r\n", line);
                                core1_i2c_monitor_append(temp_buf);
                                i2c_pc_print_begin();
                                printf("%s", temp_buf);
                                i2c_pc_print_end();
                            }
                        }
                        else
                        {
                            char temp_buf[256];
                            snprintf(temp_buf, sizeof(temp_buf), "[WRONG] USAGE: R [DEV(HEX)] [LEN(DEC)] < %s\r\n", line);
                            core1_i2c_monitor_append(temp_buf);
                            i2c_pc_print_begin();
                            printf("%s", temp_buf);
                            i2c_pc_print_end();
                        }
                    }
                    else
                    {
                        char temp_buf[256];
                        snprintf(temp_buf, sizeof(temp_buf), "[WRONG] USAGE: R [DEV(HEX)] [REG(HEX)] [LEN(DEC)] or R [DEV(HEX)] [LEN(DEC)] < %s\r\n", line);
                        core1_i2c_monitor_append(temp_buf);
                        i2c_pc_print_begin();
                        printf("%s", temp_buf);
                        i2c_pc_print_end();
                    }
                    continue;
                }
                else if (strcmp(cmd, "W") == 0 || strcmp(cmd, "WRITE") == 0 || strcmp(cmd, "WR") == 0)
                {
                    // Support both: W DEV REG DATA...  (write with register)
                    // and: W DEV DATA...  (direct write to device without register)
                    if (idx < argc)
                    {
                        uint32_t dev = 0;
                        if (!parse_number(argv[idx++], &dev))
                        {
                            char temp_buf[256];
                            snprintf(temp_buf, sizeof(temp_buf), "[WRONG] USAGE: W [DEV(HEX)] [REG(HEX)] [DATA(HEX)...] or W [DEV(HEX)] [DATA(HEX)...] < %s\r\n", line);
                            core1_i2c_monitor_append(temp_buf);
                            i2c_pc_print_begin();
                            printf("%s", temp_buf);
                            i2c_pc_print_end();
                            continue;
                        }

                        uint8_t tx[64];
                        size_t n = 0;

                        // If there are at least 2 tokens left, treat first as REG and the rest as DATA (legacy behavior)
                        if ((argc - idx) >= 2)
                        {
                            uint32_t reg = 0;
                            if (!parse_number(argv[idx++], &reg))
                            {
                                char temp_buf[256];
                                snprintf(temp_buf, sizeof(temp_buf), "[WRONG] USAGE: W [DEV(HEX)] [REG(HEX)] [DATA(HEX)...] < %s\r\n", line);
                                core1_i2c_monitor_append(temp_buf);
                                i2c_pc_print_begin();
                                printf("%s", temp_buf);
                                i2c_pc_print_end();
                                continue;
                            }
                            tx[n++] = (uint8_t)reg;
                            // collect data tokens until next command token
                            while (idx < argc && !is_command_token(argv[idx]))
                            {
                                uint32_t v;
                                if (!parse_number(argv[idx++], &v))
                                {
                                    char temp_buf[256];
                                    snprintf(temp_buf, sizeof(temp_buf), "[WRONG] USAGE: W [DEV(HEX)] [REG(HEX)] [DATA(HEX)...] < %s\r\n", line);
                                    core1_i2c_monitor_append(temp_buf);
                                    i2c_pc_print_begin();
                                    printf("%s", temp_buf);
                                    i2c_pc_print_end();
                                    n = 0;
                                    break;
                                }
                                if (n < sizeof(tx))
                                    tx[n++] = (uint8_t)v;
                            }
                            if (n > 0)
                            {
                                int ret = i2c_write_timeout_us(TEST_I2C_PORT, (uint8_t)dev, tx, n, false, 1000);
                                if (ret == PICO_ERROR_TIMEOUT)
                                {
                                    i2c_deinit(TEST_I2C_PORT);
                                    test_configure_i2c_pins(i2c_freq);
                                    ret = i2c_write_timeout_us(TEST_I2C_PORT, (uint8_t)dev, tx, n, false, 1000);
                                }
                                if (ret >= 0)
                                {
                                    char temp_buf[256];
                                    snprintf(temp_buf, sizeof(temp_buf), "[SUCCESS] WRITE %u BYTES TO 0x%02X REG 0x%02X\r\n", (unsigned)n - 1, (unsigned)dev, (unsigned)tx[0]);
                                    core1_i2c_monitor_append(temp_buf);
                                    i2c_pc_print_begin();
                                    printf("%s", temp_buf);
                                    i2c_pc_print_end();
                                    StartStateLED();
                                }
                                else
                                {
                                    char temp_buf[256];
                                    snprintf(temp_buf, sizeof(temp_buf), "[ERROR] WRITE FAIL < %s\r\n", line);
                                    core1_i2c_monitor_append(temp_buf);
                                    i2c_pc_print_begin();
                                    printf("%s", temp_buf);
                                    i2c_pc_print_end();
                                }
                            }
                        }
                        else
                        {
                            // Treat remaining tokens as direct data bytes to write (no register)
                            while (idx < argc && !is_command_token(argv[idx]))
                            {
                                uint32_t v;
                                if (!parse_number(argv[idx++], &v))
                                {
                                    char temp_buf[256];
                                    snprintf(temp_buf, sizeof(temp_buf), "[WRONG] USAGE: W [DEV(HEX)] [DATA(HEX)...] < %s\r\n", line);
                                    core1_i2c_monitor_append(temp_buf);
                                    i2c_pc_print_begin();
                                    printf("%s", temp_buf);
                                    i2c_pc_print_end();
                                    n = 0;
                                    break;
                                }
                                if (n < sizeof(tx))
                                    tx[n++] = (uint8_t)v;
                            }
                            if (n > 0)
                            {
                                int ret = i2c_write_timeout_us(TEST_I2C_PORT, (uint8_t)dev, tx, n, false, 1000);
                                if (ret == PICO_ERROR_TIMEOUT)
                                {
                                    i2c_deinit(TEST_I2C_PORT);
                                    test_configure_i2c_pins(i2c_freq);
                                    ret = i2c_write_timeout_us(TEST_I2C_PORT, (uint8_t)dev, tx, n, false, 1000);
                                }
                                if (ret >= 0)
                                {
                                    char temp_buf[256];
                                    snprintf(temp_buf, sizeof(temp_buf), "[SUCCESS] WRITE %u BYTES TO 0x%02X (no reg)\r\n", (unsigned)n, (unsigned)dev);
                                    core1_i2c_monitor_append(temp_buf);
                                    i2c_pc_print_begin();
                                    printf("%s", temp_buf);
                                    i2c_pc_print_end();
                                    StartStateLED();
                                }
                                else
                                {
                                    char temp_buf[256];
                                    snprintf(temp_buf, sizeof(temp_buf), "[ERROR] WRITE FAIL < %s\r\n", line);
                                    core1_i2c_monitor_append(temp_buf);
                                    i2c_pc_print_begin();
                                    printf("%s", temp_buf);
                                    i2c_pc_print_end();
                                }
                            }
                        }
                    }
                    else
                    {
                        char temp_buf[256];
                        snprintf(temp_buf, sizeof(temp_buf), "[WRONG] USAGE: W [DEV(HEX)] [REG(HEX)] [DATA(HEX)...] or W [DEV(HEX)] [DATA(HEX)...] < %s\r\n", line);
                        core1_i2c_monitor_append(temp_buf);
                        i2c_pc_print_begin();
                        printf("%s", temp_buf);
                        i2c_pc_print_end();
                    }
                    continue;
                }
                else if (strcmp(cmd, "BAUD") == 0 || strcmp(cmd, "F") == 0 || strcmp(cmd, "FREQ") == 0)
                {
                    if (idx < argc)
                    {
                        uint32_t hz;
                        if (parse_number(argv[idx++], &hz, 10))
                        {
                            i2c_freq = (int32_t)hz;
                            char temp_buf[256];
                            snprintf(temp_buf, sizeof(temp_buf), "[SUCCESS] BAUD SET\r\n");
                            core1_i2c_monitor_append(temp_buf);
                            i2c_pc_print_begin();
                            printf("%s", temp_buf);
                            i2c_pc_print_end();
                            first_entry = true; // trigger reconfiguration
                        }
                        else
                        {
                            char temp_buf[256];
                            snprintf(temp_buf, sizeof(temp_buf), "[WRONG] USAGE: BAUD [HZ] < %s\r\n", line);
                            core1_i2c_monitor_append(temp_buf);
                            i2c_pc_print_begin();
                            printf("%s", temp_buf);
                            i2c_pc_print_end();
                        }
                    }
                    else
                    {
                        char temp_buf[256];
                        snprintf(temp_buf, sizeof(temp_buf), "[WRONG] USAGE: BAUD [HZ] < %s\r\n", line);
                        core1_i2c_monitor_append(temp_buf);
                        i2c_pc_print_begin();
                        printf("%s", temp_buf);
                        i2c_pc_print_end();
                    }
                    continue;
                }
                else
                {
                    // 合并连续的未知 token 为一条消息输出
                    char combined[256] = {0};
                    // cmd 已为 argv[idx-1]
                    snprintf(combined, sizeof(combined), "%s", cmd);
                    // 将后续连续非命令 token 一并合并
                    while (idx < argc && !is_command_token(argv[idx]))
                    {
                        strncat(combined, " ", sizeof(combined) - strlen(combined) - 1);
                        strncat(combined, argv[idx], sizeof(combined) - strlen(combined) - 1);
                        idx++;
                    }
                    char out_buf[512];
                    snprintf(out_buf, sizeof(out_buf), "[WRONG] UNKNOWN CMD: %s\r\n", combined);
                    core1_i2c_monitor_append(out_buf);
                    i2c_pc_print_begin();
                    printf("%s", out_buf);
                    i2c_pc_print_end();
                    continue;
                }
            } // end for token loop
        }
    };
    bool prev_valid = false;

    while (i2c_run_in_core1 && !i2cspi_test_running)
    {
        stdio_filter_driver(&stdio_usb); ////////////////////////////////////////////
                                         // Periodically allow core to send pending select-voltage messages
        Key_TrySendSelectVoltage();
        SSD1306_PrintBufRaw();
        // handle_pc_interface_commands();//在I2C指令处理中 引入全局指令读取
        stdio_filter_driver(NULL);
        // 动态更新配置
        static int32_t last_i2c_freq = 0;
        static bool last_i2c_master_mode = true;

        if (first_entry || last_i2c_freq != i2c_freq || last_i2c_master_mode != i2ctest_master_mode)
        {
            uint actual = i2c_set_baudrate(TEST_I2C_PORT, (uint)i2c_freq);
            // i2c_set_slave_mode(TEST_I2C_PORT, !i2ctest_master_mode, 0x55);
            last_i2c_freq = i2c_freq;
            last_i2c_master_mode = i2ctest_master_mode;
            first_entry = false;

            char tmp[128];
            snprintf(tmp, sizeof(tmp), "[INFO] I2C BAUD SET: REQ %u ACT %u\r\n", (unsigned)i2c_freq, actual);
            core1_i2c_monitor_append(tmp);
            i2c_pc_print_begin();
            printf("%s", tmp);
            i2c_pc_print_end();
            StartStateLED();

            // 控制协议////////////////////////////////////////////////////////////////////////////////////////////////
            if (Enable_PC_Interface)
            {
                extern bool i2ctest_scl_on_left;
                char temp_buf[256];
                snprintf(temp_buf, sizeof(temp_buf), "#PIVOICSI#I2C,BAU,%u,%u,%d#PIS#\r\n", (unsigned)i2c_freq, actual, i2ctest_scl_on_left ? 1 : 0);
                printf("%s", temp_buf);
            }
        }

        uint32_t now = time_us_32();
        // 1s定时I2C设备扫描（仅变化时输出）
        {
            static uint32_t last_scan = 0;
            ;

            if ((uint32_t)(now - last_scan) >= 200000u)
            {

                // printf("I2C SCANNING...\r\n");
                bool present[128] = {false};
                for (uint32_t addr = 0x03; addr <= 0x77; ++addr)
                {
                    int retry = 2; // 超时最多重试3次
                    while (retry--)
                    {
                        uint8_t tmp = 0;
                        int ret = i2c_write_timeout_us(TEST_I2C_PORT, (uint8_t)addr, &tmp, 1, true, 1000);
                        if (ret >= 0)
                        {
                            present[addr] = true;
                            break;
                        }
                        else if (ret == PICO_ERROR_TIMEOUT)
                        {
                            i2c_deinit(TEST_I2C_PORT);
                            test_configure_i2c_pins(i2c_freq);
                            // retry count减1后再进下一轮
                        }
                        else
                        {
                            break;
                        }
                    }
                }
                bool changed = false;
                if (prev_valid)
                {
                    for (uint32_t addr = 0; addr < 128; ++addr)
                    {
                        if (present[addr] != prev_present[addr])
                        {
                            changed = true;
                            break;
                        }
                    }
                }
                else
                {
                    changed = true;
                }
                // 更新快照
                for (uint32_t addr = 0; addr < 128; ++addr)
                    prev_present[addr] = present[addr];
                prev_valid = true;

                if (changed)
                {
                    bool any_present = false;
                    char temp_buf[64];
                    snprintf(temp_buf, sizeof(temp_buf), "[INFO] I2C DEV: ");
                    core1_i2c_monitor_append(temp_buf);
                    i2c_pc_print_begin();
                    printf("%s", temp_buf);
                    StartStateLED();
                    for (uint32_t addr = 0x03; addr <= 0x77; ++addr)
                    {
                        if (present[addr])
                        {
                            char temp_buf2[8];
                            snprintf(temp_buf2, sizeof(temp_buf2), "0x%02X ", (unsigned)addr);
                            core1_i2c_monitor_append(temp_buf2);
                            printf("%s", temp_buf2);
                            any_present = true;
                        }
                    }
                    if (!any_present)
                    {
                        char temp_buf2[16];
                        snprintf(temp_buf2, sizeof(temp_buf2), "NONE ");
                        core1_i2c_monitor_append(temp_buf2);
                        printf("%s", temp_buf2);
                    }
                    char temp_buf2[16];
                    snprintf(temp_buf2, sizeof(temp_buf2), "\r\n");
                    core1_i2c_monitor_append(temp_buf2);
                    printf("%s", temp_buf2);
                    i2c_pc_print_end();
                    StartStateLED();

                    // 控制协议////////////////////////////////////////////////////////////////////////////////////////////////
                    if (Enable_PC_Interface)
                    {
                        char temp_buf[256];
                        snprintf(temp_buf, sizeof(temp_buf), "#PIVOICSI#I2C,DEV");
                        printf("%s", temp_buf);
                        for (uint32_t addr = 0x03; addr <= 0x77; ++addr)
                        {
                            if (present[addr])
                            {
                                char temp_buf2[8];
                                snprintf(temp_buf2, sizeof(temp_buf2), ",0x%02X", (unsigned)addr);
                                printf("%s", temp_buf2);
                            }
                        }
                        if (!any_present)
                        {
                            char temp_buf2[16];
                            snprintf(temp_buf2, sizeof(temp_buf2), ",NONE");
                            printf("%s", temp_buf2);
                        }
                        char temp_buf2[8];
                        snprintf(temp_buf2, sizeof(temp_buf2), "#PIS#\r\n");
                        printf("%s", temp_buf2);
                    }
                }

                last_scan = now;
            }
        }
        int ch = getchar_timeout_us(0);
        if (ch == PICO_ERROR_TIMEOUT)
        {
            if (li > 0 && (uint32_t)(now - last_rx_tick) >= I2C_CMD_IDLE_TIMEOUT_US)
            {
                handle_command();
            }
            else
            {
                tight_loop_contents();
            }
        }
        else
        {
            last_rx_tick = now;

            if (ch == '\r' || ch == '\n')
            {
                continue; // 依靠空闲超时分隔命令
            }

            // 仅处理协议包裹命令，裸数据不进入命令解析
            if (frame_mode == FRAME_USCO)
            {
                int res_usco = handle_pc_char_input(ch);
                if (res_usco == 1)
                {
                    li = 0;
                    frame_mode = FRAME_NONE;
                    continue; // 由 process_pc_command 处理
                }
                if (res_usco == -1)
                {
                    frame_mode = FRAME_NONE;
                }
                continue;
            }
            if (frame_mode == FRAME_ICSI)
            {
                int res_icsi = handle_icsi_char(ch);
                if (res_icsi == 1)
                {
                    strncpy(line, icsi_buffer, sizeof(line) - 1);
                    line[sizeof(line) - 1] = '\0';
                    li = (int)strlen(line);
                    handle_command();
                    frame_mode = FRAME_NONE;
                }
                else if (res_icsi == -1)
                {
                    frame_mode = FRAME_NONE;
                }
                continue;
            }

            int res_usco = handle_pc_char_input(ch);
            int res_icsi = handle_icsi_char(ch);

            if (res_icsi == 1)
            {
                strncpy(line, icsi_buffer, sizeof(line) - 1);
                line[sizeof(line) - 1] = '\0';
                li = (int)strlen(line);
                handle_command();
                frame_mode = FRAME_NONE;
                continue;
            }
            if (res_usco == 1)
            {
                li = 0;
                frame_mode = FRAME_NONE;
                continue; // 由 process_pc_command 处理
            }

            if (res_usco == 0 && res_icsi == 0)
            {
                frame_mode = FRAME_BOTH;
                continue;
            }
            if (res_usco == 0 && res_icsi == -1)
            {
                frame_mode = FRAME_USCO;
                continue;
            }
            if (res_icsi == 0 && res_usco == -1)
            {
                frame_mode = FRAME_ICSI;
                continue;
            }
            if (frame_mode == FRAME_BOTH)
            {
                if (res_usco == 0 && res_icsi == -1)
                {
                    frame_mode = FRAME_USCO;
                }
                else if (res_icsi == 0 && res_usco == -1)
                {
                    frame_mode = FRAME_ICSI;
                }
                else if (res_usco == -1 && res_icsi == -1)
                {
                    frame_mode = FRAME_NONE;
                }
                continue;
            }
        }
    }
}

// SPI连接与测试循环
#define SPI_SLAVE_TIMEOUT_US (30 * 1000 * 1000)

void core1_spi_connect_USB()
{
    // 初始化SPI硬件
#ifndef TEST_SPI_PORT
#define TEST_SPI_PORT spi0
#endif

    // 配置GPIO并上拉
    gpio_init(TEST_SPI_SCK_PIN);
    gpio_set_function(TEST_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_pull_up(TEST_SPI_SCK_PIN);

    gpio_init(TEST_SPI_TX_PIN);
    gpio_set_function(TEST_SPI_TX_PIN, GPIO_FUNC_SPI);
    gpio_pull_up(TEST_SPI_TX_PIN);

    gpio_init(TEST_SPI_RX_PIN);
    gpio_set_function(TEST_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_pull_up(TEST_SPI_RX_PIN);

    gpio_init(TEST_SPI_CS_PIN);
    gpio_set_function(TEST_SPI_CS_PIN, GPIO_FUNC_SPI);
    gpio_pull_up(TEST_SPI_CS_PIN);

    spi_init(TEST_SPI_PORT, (uint)i2c_freq);
    spi_set_format(TEST_SPI_PORT, spi_bits_val, (spi_cpol_val ? SPI_CPOL_1 : SPI_CPOL_0), (spi_cpha_val ? SPI_CPHA_1 : SPI_CPHA_0), SPI_MSB_FIRST);
    spi_set_slave(TEST_SPI_PORT, !spi_master_mode);

    stdio_set_driver_enabled(&stdio_usb, true);
    stdio_set_driver_enabled(&stdio_uart, true);

    while (getchar_timeout_us(10) != PICO_ERROR_TIMEOUT)
        tight_loop_contents();

    static char line[512];
    static int li = 0;
    uint32_t last_rx_tick = time_us_32();
    bool first_entey = true;
    static enum {
        ICSI_IDLE,
        ICSI_MATCHING_HEADER,
        ICSI_RECEIVING_DATA,
        ICSI_MATCHING_FOOTER
    } icsi_state = ICSI_IDLE;
    static enum {
        FRAME_NONE,
        FRAME_USCO,
        FRAME_ICSI,
        FRAME_BOTH
    } frame_mode = FRAME_NONE;
    static char icsi_buffer[256];
    static size_t icsi_idx = 0;
    static size_t icsi_match_idx = 0;
    static uint32_t icsi_last_time = 0;

    auto parse_number = [](const char *s, uint32_t *out, int base = 16) -> bool
    {
        if (!s || !*s)
            return false;
        char *end = nullptr;
        unsigned long v = strtoul(s, &end, base);
        if (end == s)
            return false;
        *out = (uint32_t)v;
        return true;
    };

    auto to_upper = [](char *s)
    {
        for (; *s; ++s)
        {
            if (*s >= 'a' && *s <= 'z')
                *s -= 32;
        }
    };

    auto tokenize = [](char *s, char *argv[], int max) -> int
    {
        int n = 0;
        char *p = s;
        while (*p && n < max)
        {
            while (*p == ' ' || *p == '\t')
                ++p;
            if (!*p)
                break;
            argv[n++] = p;
            while (*p && *p != ' ' && *p != '\t')
                ++p;
            if (!*p)
                break;
            *p++ = '\0';
        }
        return n;
    };

    auto handle_icsi_char = [&](int c) -> int
    {
        if (c == PICO_ERROR_TIMEOUT)
            return 0;

        static const char *HEADER = "#PIVOICSI#";
        static const char *FOOTER = "#PIS#";
        uint32_t now = time_us_32();

        if (icsi_state != ICSI_IDLE && (now - icsi_last_time) >= 150000)
        {
            icsi_state = ICSI_IDLE;
            icsi_match_idx = 0;
            icsi_idx = 0;
            return -1;
        }
        icsi_last_time = now;

        char ch = (char)c;

        switch (icsi_state)
        {
        case ICSI_IDLE:
        case ICSI_MATCHING_HEADER:
            if (ch == HEADER[icsi_match_idx])
            {
                icsi_state = ICSI_MATCHING_HEADER;
                icsi_match_idx++;
                if (HEADER[icsi_match_idx] == '\0')
                {
                    icsi_state = ICSI_RECEIVING_DATA;
                    icsi_idx = 0;
                    icsi_match_idx = 0;
                }
                return 0;
            }
            icsi_state = ICSI_IDLE;
            icsi_match_idx = 0;
            return -1;

        case ICSI_RECEIVING_DATA:
            if (ch == FOOTER[0])
            {
                icsi_state = ICSI_MATCHING_FOOTER;
                icsi_match_idx = 1;
                return 0;
            }
            if (icsi_idx < sizeof(icsi_buffer) - 1)
            {
                icsi_buffer[icsi_idx++] = ch;
                icsi_buffer[icsi_idx] = '\0';
            }
            return 0;

        case ICSI_MATCHING_FOOTER:
            if (ch == FOOTER[icsi_match_idx])
            {
                icsi_match_idx++;
                if (FOOTER[icsi_match_idx] == '\0')
                {
                    icsi_buffer[icsi_idx] = '\0';
                    icsi_state = ICSI_IDLE;
                    icsi_match_idx = 0;
                    return 1;
                }
                return 0;
            }
            icsi_state = ICSI_IDLE;
            icsi_match_idx = 0;
            return -1;
        }
        return 0;
    };

    static uint8_t g_slave_tx[256];
    static size_t g_slave_len = 0;
    static size_t g_slave_pos = 0;
    static size_t g_slave_tx_pos = 0;

    auto slave_load_tx = [&](const uint8_t *data, size_t len)
    {
        g_slave_len = (len > 256) ? 256 : len;
        memcpy(g_slave_tx, data, g_slave_len);
        g_slave_pos = 0;
        g_slave_tx_pos = 0;

        // 解决方案：使用 SDK 的 deinit+init 对 SPI 外设执行完整的硬件底层软复位。
        spi_deinit(TEST_SPI_PORT);
        spi_init(TEST_SPI_PORT, (uint)spi_freq);
        spi_set_format(TEST_SPI_PORT, spi_bits_val,
                       (spi_cpol_val ? SPI_CPOL_1 : SPI_CPOL_0),
                       (spi_cpha_val ? SPI_CPHA_1 : SPI_CPHA_0),
                       SPI_MSB_FIRST);
        spi_set_slave(TEST_SPI_PORT, !spi_master_mode);

        // 【预加载正确数据】
        // 硬件完全复位后，Shift Register 为干干净净的空状态
        if (g_slave_len > 0)
        {
            while (g_slave_tx_pos < g_slave_len && spi_is_writable(TEST_SPI_PORT))
            {
                uint8_t txb = g_slave_tx[g_slave_tx_pos++];
                spi_get_hw(TEST_SPI_PORT)->dr = (uint32_t)txb;
                // core1_spi_monitor_append_tx(txb); // 预加载时不记录监视器，等真正移位时再记录以确保对齐
            }
        }
    };

    auto slave_try_transfer = [&](uint8_t *rx_out) -> size_t
    {
        size_t count = 0;

        // 1. 发送维护：趁着主机还没跑完当前的字节，尽早就填满 TX FIFO 余下的空间
        if (g_slave_len > 0)
        {
            // 预设任务模式，只要硬件 FIFO 还有空间我们就塞
            while (g_slave_tx_pos < g_slave_len && spi_is_writable(TEST_SPI_PORT))
            {
                uint8_t txb = g_slave_tx[g_slave_tx_pos++];
                spi_get_hw(TEST_SPI_PORT)->dr = (uint32_t)txb;
                // 原本在这里的 append_tx 移动到了下方 RX 可读时，以确保 TX/RX 索引同步
            }
        }
        else
        {
        }

        // 2. 接收维护：专门从 RX FIFO 中读取回收，与上面的 TX 完成彻底解耦（绝不等收了才发）
        while (spi_is_readable(TEST_SPI_PORT))
        {
            uint8_t rxb = (uint8_t)spi_get_hw(TEST_SPI_PORT)->dr;

            // 重要：由于 SPI 是全双工，每一个接收到的字节都代表一个完整的移位周期。
            // 此时从硬件 FIFO 读取出的 RX 数据代表当前移位周期的结束。
            core1_spi_monitor_append_rx(rxb); // 实时存入 RX 监视器

            // 记录 TX 监视器：
            if (g_slave_len > 0)
            {
                // 预设任务模式 (W/R)：从预置的 TX 数组中取值
                if (g_slave_pos < g_slave_len)
                {
                    core1_spi_monitor_append_tx(g_slave_tx[g_slave_pos]);
                }
                StartStateLED();
            }
            else
            {
                // 纯监听模式 (Monitor)：
                // 在没有预设任务时，硬件 FIFO 实际上发送的是当前移位寄存器里残存的内容
                // 但为了监视器显示的一致性，我们记录 0x00 或从寄存器读取的实际发出值（如果有的话）
                // 在 RP2040 这种架构，被动 FIFO 为空时通常会重复最后一次的值或由拉高电平决定
                core1_spi_monitor_append_tx(0x00);
            }

            // 通知 UI 更新
            spitext_ifupdate = true;

            if (g_slave_len > 0)
            {
                // 预设任务模式
                if (g_slave_pos < g_slave_len)
                {
                    rx_out[g_slave_pos] = rxb;
                    g_slave_pos++;
                    count++;
                }
            }
            else
            {
                // 被动监听模式
                if (g_slave_pos < 256)
                {
                    rx_out[g_slave_pos] = rxb;
                    g_slave_pos++;
                    count++;
                }
            }
        }
        return count;
    };

    auto handle_spi_command = [&]()
    {
        if (li == 0)
            return;
        line[li] = '\0';
        char *argv[32];
        char line_copy[512];
        strcpy(line_copy, line);
        int argc = tokenize(line_copy, argv, 32);
        li = 0;

        if (argc > 0)
        {
            auto is_command_token = [](const char *t) -> bool
            {
                if (!t || !*t)
                    return false;
                return (strcmp(t, "W") == 0) || (strcmp(t, "WRITE") == 0) || (strcmp(t, "WR") == 0) ||
                       (strcmp(t, "R") == 0) || (strcmp(t, "READ") == 0) || (strcmp(t, "RD") == 0) ||
                       (strcmp(t, "WR") == 0) || (strcmp(t, "RD") == 0) ||
                       (strcmp(t, "BAUD") == 0) || (strcmp(t, "F") == 0) || (strcmp(t, "FREQ") == 0) ||
                       (strcmp(t, "S") == 0) || (strcmp(t, "M") == 0);
            };

            for (int idx = 0; idx < argc;)
            {
                to_upper(argv[idx]);
                char *cmd = argv[idx++];

                if (strcmp(cmd, "W") == 0 || strcmp(cmd, "WRITE") == 0 || strcmp(cmd, "WR") == 0)
                {
                    uint8_t tx[256];
                    size_t n = 0;
                    while (idx < argc && !is_command_token(argv[idx]))
                    {
                        uint32_t v;
                        if (parse_number(argv[idx++], &v))
                            tx[n++] = (uint8_t)v;
                        else
                        {
                            n = 0;
                            break;
                        }
                    }
                    if (n > 0)
                    {
                        if (!spi_master_mode)
                        {
                            // 清空 SPI FIFO
                            while (spi_is_readable(TEST_SPI_PORT))
                                (void)spi_get_hw(TEST_SPI_PORT)->dr;

                            slave_load_tx(tx, n);

                            char inf[128];
                            snprintf(inf, sizeof(inf), "[INFO] SLAVE W-TASK LOADED: %u BYTES, WAITING FOR MASTER SPI...\r\n", (unsigned)n);
                            spi_pc_print_begin();
                            printf("%s", inf);
                            spi_pc_print_end();
                        }
                        else
                        {
                            // 清空 SPI FIFO
                            while (spi_is_readable(TEST_SPI_PORT))
                                (void)spi_get_hw(TEST_SPI_PORT)->dr;

                            uint8_t rx[256] = {0};
                            spi_write_read_blocking(TEST_SPI_PORT, tx, rx, n);

                            // 以环形方式记录到监视器缓存
                            for (size_t i = 0; i < n; i++)
                            {
                                core1_spi_monitor_append_tx(tx[i]);
                                core1_spi_monitor_append_rx(rx[i]);
                            }
                            spitext_ifupdate = true;

                            char res[512];
                            snprintf(res, sizeof(res), "[DONE] MASTER RX: ");
                            spi_pc_print_begin();
                            printf("%s", res);
                            for (size_t i = 0; i < n; i++)
                            {
                                printf("%02X ", rx[i]);
                            }
                            printf("| TX: ");
                            for (size_t i = 0; i < n; i++)
                            {
                                printf("%02X ", tx[i]);
                            }
                            spi_pc_print_end();
                            StartStateLED();
                        }
                    }
                }
                else if (strcmp(cmd, "R") == 0 || strcmp(cmd, "READ") == 0 || strcmp(cmd, "RD") == 0)
                {
                    uint32_t len = 0;
                    if (idx < argc && parse_number(argv[idx++], &len, 10))
                    {
                        if (!spi_master_mode)
                        {
                            // 清空 SPI FIFO
                            while (spi_is_readable(TEST_SPI_PORT))
                                (void)spi_get_hw(TEST_SPI_PORT)->dr;

                            uint8_t tx_dummy[256];
                            memset(tx_dummy, 0xFF, 256);
                            size_t actual_len = (len > 256) ? 256 : len;
                            slave_load_tx(tx_dummy, actual_len);

                            char inf[128];
                            snprintf(inf, sizeof(inf), "[INFO] SLAVE R-TASK LOADED: %u BYTES (TX:0xFF), WAITING FOR MASTER SPI...\r\n", (unsigned)actual_len);
                            spi_pc_print_begin();
                            printf("%s", inf);
                            spi_pc_print_end();
                        }
                        else
                        {
                            // 清空 SPI FIFO
                            while (spi_is_readable(TEST_SPI_PORT))
                                (void)spi_get_hw(TEST_SPI_PORT)->dr;

                            uint8_t tx_dummy[256], rx[256];
                            memset(tx_dummy, 0xFF, 256);
                            if (len > 256)
                                len = 256;
                            spi_write_read_blocking(TEST_SPI_PORT, tx_dummy, rx, len);

                            // 以环形方式记录到监视器缓存
                            for (size_t i = 0; i < len; i++)
                            {
                                core1_spi_monitor_append_tx(0xFF);
                                core1_spi_monitor_append_rx(rx[i]);
                            }
                            spitext_ifupdate = true;

                            char res[512];
                            snprintf(res, sizeof(res), "[DONE] MASTER RX: ");
                            spi_pc_print_begin();
                            printf("%s", res);
                            for (size_t i = 0; i < len; i++)
                            {
                                printf("%02X ", rx[i]);
                            }
                            printf("| TX: ");
                            for (size_t i = 0; i < len; i++)
                            {
                                printf("%02X ", 0xFF);
                            }
                            spi_pc_print_end();
                        }
                    }
                }
                else if (strcmp(cmd, "BAUD") == 0 || strcmp(cmd, "F") == 0 || strcmp(cmd, "FREQ") == 0)
                {
                    uint32_t hz;
                    if (idx < argc && parse_number(argv[idx++], &hz, 10))
                    {
                        spi_freq = (int32_t)hz;
                        uint actual = spi_set_baudrate(TEST_SPI_PORT, (uint)spi_freq);
                        char temp_buf[256];
                        snprintf(temp_buf, sizeof(temp_buf), "[SUCCESS] BAUD SET\r\n");
                        spi_pc_print_begin();
                        printf("%s", temp_buf);
                        spi_pc_print_end();
                        first_entey = true; // trigger reconfiguration
                    }
                    else
                    {
                        char temp_buf[256];
                        snprintf(temp_buf, sizeof(temp_buf), "[WRONG] USAGE: BAUD [HZ] < %s\r\n", line);
                        spi_pc_print_begin();
                        printf("%s", temp_buf);
                        spi_pc_print_end();
                    }
                }
                else if (strcmp(cmd, "SET") == 0 || strcmp(cmd, "CONFIG") == 0 || strcmp(cmd, "C") == 0 || strcmp(cmd, "CFG") == 0)
                {
                    if (idx + 5 <= argc)
                    {
                        char *mode_str = argv[idx++];
                        char *baud_str = argv[idx++];
                        char *cpol_str = argv[idx++];
                        char *cpha_str = argv[idx++];
                        char *bits_str = argv[idx++];

                        to_upper(mode_str);
                        bool new_master = (strcmp(mode_str, "M") == 0 || strcmp(mode_str, "MASTER") == 0);

                        uint32_t new_baud = 0;
                        uint32_t new_cpol = 0;
                        uint32_t new_cpha = 0;
                        uint32_t new_bits = 0;

                        if (parse_number(baud_str, &new_baud, 10) && parse_number(cpol_str, &new_cpol) && parse_number(cpha_str, &new_cpha) && parse_number(bits_str, &new_bits, 10))
                        {
                            spi_master_mode = new_master;
                            spi_freq = (int32_t)new_baud;
                            spi_cpol_val = (uint8_t)new_cpol;
                            spi_cpha_val = (uint8_t)new_cpha;
                            if (new_bits < 8)
                                new_bits = 8;
                            if (new_bits > 16)
                                new_bits = 16;
                            spi_bits_val = (uint8_t)new_bits;

                            // Clear Buffer on mode switch
                            g_slave_len = 0;
                            g_slave_pos = 0;

                            char temp_buf[256];
                            snprintf(temp_buf, sizeof(temp_buf), "[SUCCESS] CFG SET\r\n");
                            spi_pc_print_begin();
                            printf("%s", temp_buf);
                            spi_pc_print_end();
                            first_entey = true; // trigger reconfiguration
                        }
                        else
                        {
                            char temp_buf[256];
                            snprintf(temp_buf, sizeof(temp_buf), "[WRONG] USAGE: SET [M/S] [BAUD] [CPOL] [CPHA] [BITS] < %s\r\n", line);
                            spi_pc_print_begin();
                            printf("%s", temp_buf);
                            spi_pc_print_end();
                        }
                    }
                    else
                    {
                        char temp_buf[256];
                        snprintf(temp_buf, sizeof(temp_buf), "[WRONG] USAGE: SET [M/S] [BAUD] [CPOL] [CPHA] [BITS] < %s\r\n", line);
                        spi_pc_print_begin();
                        printf("%s", temp_buf);
                        spi_pc_print_end();
                    }
                }
                else if (strcmp(cmd, "M") == 0)
                {
                    spi_master_mode = true;
                    char temp_buf[256];
                    snprintf(temp_buf, sizeof(temp_buf), "[SUCCESS] MODE: MASTER\r\n");
                    spi_pc_print_begin();
                    printf("%s", temp_buf);
                    spi_pc_print_end();
                    first_entey = true; // trigger reconfiguration
                }
                else if (strcmp(cmd, "S") == 0)
                {
                    spi_master_mode = false;
                    char temp_buf[256];
                    snprintf(temp_buf, sizeof(temp_buf), "[SUCCESS] MODE: SLAVE\r\n");
                    spi_pc_print_begin();
                    printf("%s", temp_buf);
                    spi_pc_print_end();
                    first_entey = true; // trigger reconfiguration
                }
                else
                {
                    char temp_buf[256];
                    snprintf(temp_buf, sizeof(temp_buf), "[WRONG] UNKNOWN CMD: %s\r\n", line);
                    spi_pc_print_begin();
                    printf("%s", temp_buf);
                    spi_pc_print_end();
                }
            }
        }
    };

    // 动态更新配置
    static uint8_t last_bits = 8;
    static uint8_t last_cpol = 0;
    static uint8_t last_cpha = 0;
    static int32_t last_freq = 0;
    static bool last_master_mode = true;

    static uint8_t rx_buf[256];
    static uint32_t last_print_ms = 0;
    static uint32_t slave_start_ms = 0;

    while (spi_run_in_core1)
    {
        stdio_filter_driver(&stdio_usb); /////////////////////////////////////
        // Try to send pending select-voltage from core SPI loop
        Key_TrySendSelectVoltage();
        SSD1306_PrintBufRaw();
        // handle_pc_interface_commands();  //在SPI指令处理中 引入全局指令读取
        stdio_filter_driver(NULL);

        if (first_entey || last_bits != spi_bits_val || last_cpol != spi_cpol_val || last_cpha != spi_cpha_val || last_freq != spi_freq || last_master_mode != spi_master_mode)
        {
            // 解决方案：使用 SDK 的 deinit+init 对 SPI 外设执行完整的硬件底层软复位。
            spi_deinit(TEST_SPI_PORT);
            uint actual = spi_init(TEST_SPI_PORT, (uint)spi_freq);
            spi_set_format(TEST_SPI_PORT, spi_bits_val, (spi_cpol_val ? SPI_CPOL_1 : SPI_CPOL_0), (spi_cpha_val ? SPI_CPHA_1 : SPI_CPHA_0), SPI_MSB_FIRST);
            spi_set_slave(TEST_SPI_PORT, !spi_master_mode);
            last_bits = spi_bits_val;
            last_cpol = spi_cpol_val;
            last_cpha = spi_cpha_val;
            last_freq = spi_freq;
            last_master_mode = spi_master_mode;
            first_entey = false;

            // Clear Software Buffer on mode update
            g_slave_len = 0;
            g_slave_pos = 0;

            char tmp[128];
            snprintf(tmp, sizeof(tmp), "[INFO] SPI CFG: %s Set:%uHz Act:%uHz CPHA:%u CPOL:%u Bits:%u\r\n", spi_master_mode ? "M" : "S", (unsigned)spi_freq, actual, spi_cpha_val, spi_cpol_val, spi_bits_val);
            spi_pc_print_begin();
            printf("%s", tmp);
            spi_pc_print_end();
            StartStateLED();

            // 控制协议////////////////////////////////////////////////////////////////////////////////////////////////
            if (Enable_PC_Interface)
            {
                char temp_buf[256];
                snprintf(temp_buf, sizeof(temp_buf), "#PIVOICSI#SPI,CFG,%s,%u,%u,%u,%u,%u#PIS#\r\n", spi_master_mode ? "M" : "S", (unsigned)spi_freq, actual, spi_cpha_val, spi_cpol_val, spi_bits_val);
                printf("%s", temp_buf);
            }
        }

        uint32_t now = time_us_32();
        if (spi_master_mode == false)
        {
            // 始终运行传输检查，不仅限于 g_slave_len > 0
            size_t batch = slave_try_transfer(rx_buf);

            uint32_t now_ms = to_ms_since_boot(get_absolute_time());
            // 对于 W/R 预设任务模式：
            if (g_slave_len > 0)
            {
                if (slave_start_ms == 0)
                    slave_start_ms = now_ms; // 记录任务开始时间

                // 重点修复：如果发现传送完成，无论是否到 1s，立即打印结果
                if (g_slave_pos >= g_slave_len)
                {
                    char result[512];
                    snprintf(result, sizeof(result), "[DONE] SLAVE RX: ");
                    spi_pc_print_begin();
                    printf("%s", result);
                    for (int i = 0; i < (int)g_slave_len; i++)
                    {
                        char hex[8];
                        snprintf(hex, sizeof(hex), "%02X ", rx_buf[i]);
                        printf("%s", hex);
                    }
                    snprintf(result, sizeof(result), "| TX: ");
                    printf("%s", result);
                    for (int i = 0; i < (int)g_slave_len; i++)
                    {
                        char hex[8];
                        snprintf(hex, sizeof(hex), "%02X ", g_slave_tx[i]);
                        printf("%s", hex);
                    }
                    spi_pc_print_end();
                    g_slave_pos = 0;
                    g_slave_len = 0;        // 完成后清除任务
                    slave_start_ms = 0;     // 重置开始时间
                    last_print_ms = now_ms; // 同步打印时间戳
                }
                else if (now_ms - last_print_ms >= 1000)
                {
                    // 尚未完成时，每秒打印一次 Progress
                    char status[128];
                    uint32_t wait_sec = (now_ms - slave_start_ms) / 1000;
                    snprintf(status, sizeof(status), "[INFO] PROGRESS: %u/%u (WAIT: %us)\r\n", (unsigned)g_slave_pos, (unsigned)g_slave_len, (unsigned)wait_sec);
                    spi_pc_print_begin();
                    printf("%s", status);
                    spi_pc_print_end();
                    last_print_ms = now_ms;
                }
            }
            else
            {
                slave_start_ms = 0; // 退出任务模式时重置
                // 对于被动监听模式：只要有数据接收过 (g_slave_pos > 0)
                // 100ms 无数据 (idle) 或 持续时间达到 1s
                static uint32_t first_byte_ms = 0;
                static uint32_t last_byte_ms = 0;

                if (batch > 0)
                {
                    if (g_slave_pos == batch)
                        first_byte_ms = now_ms;
                    last_byte_ms = now_ms;
                }

                if (g_slave_pos > 0)
                {
                    bool timeout = (now_ms - last_byte_ms >= 100);
                    bool long_duration = (now_ms - first_byte_ms >= 1000);

                    if (timeout || long_duration)
                    {
                        char result[512];
                        StartStateLED(); // 从机意外被动接收时点亮 LED
                        snprintf(result, sizeof(result), "[INFO] MONITOR RX (%u BYTES): ", (unsigned)g_slave_pos);
                        spi_pc_print_begin();
                        printf("%s", result);
                        for (int i = 0; i < (int)g_slave_pos; i++)
                        {
                            char hex[8];
                            snprintf(hex, sizeof(hex), "%02X ", rx_buf[i]);
                            printf("%s", hex);
                        }
                        spi_pc_print_end();
                        g_slave_pos = 0; // 重置计数以等待下一波
                        last_print_ms = now_ms;
                    }
                }
                else
                {
                    last_print_ms = now_ms;
                }
            }
        }

        int ch = getchar_timeout_us(0);
        if (ch == PICO_ERROR_TIMEOUT)
        {
            if (li > 0 && (uint32_t)(now - last_rx_tick) >= I2C_CMD_IDLE_TIMEOUT_US)
                handle_spi_command();
            else
                tight_loop_contents();
        }
        else
        {
            last_rx_tick = now;
            if (ch == '\r' || ch == '\n')
                continue;

            // 仅处理协议包裹命令，裸数据不进入命令解析
            if (frame_mode == FRAME_USCO)
            {
                int res_usco = handle_pc_char_input(ch);
                if (res_usco == 1)
                {
                    li = 0;
                    frame_mode = FRAME_NONE;
                    continue; // 由 process_pc_command 处理
                }
                if (res_usco == -1)
                {
                    frame_mode = FRAME_NONE;
                }
                continue;
            }
            if (frame_mode == FRAME_ICSI)
            {
                int res_icsi = handle_icsi_char(ch);
                if (res_icsi == 1)
                {
                    strncpy(line, icsi_buffer, sizeof(line) - 1);
                    line[sizeof(line) - 1] = '\0';
                    li = (int)strlen(line);
                    handle_spi_command();
                    frame_mode = FRAME_NONE;
                }
                else if (res_icsi == -1)
                {
                    frame_mode = FRAME_NONE;
                }
                continue;
            }

            int res_usco = handle_pc_char_input(ch);
            int res_icsi = handle_icsi_char(ch);

            if (res_icsi == 1)
            {
                strncpy(line, icsi_buffer, sizeof(line) - 1);
                line[sizeof(line) - 1] = '\0';
                li = (int)strlen(line);
                handle_spi_command();
                frame_mode = FRAME_NONE;
                continue;
            }
            if (res_usco == 1)
            {
                li = 0;
                frame_mode = FRAME_NONE;
                continue; // 由 process_pc_command 处理
            }

            if (res_usco == 0 && res_icsi == 0)
            {
                frame_mode = FRAME_BOTH;
                continue;
            }
            if (res_usco == 0 && res_icsi == -1)
            {
                frame_mode = FRAME_USCO;
                continue;
            }
            if (res_icsi == 0 && res_usco == -1)
            {
                frame_mode = FRAME_ICSI;
                continue;
            }
            if (frame_mode == FRAME_BOTH)
            {
                if (res_usco == 0 && res_icsi == -1)
                {
                    frame_mode = FRAME_USCO;
                }
                else if (res_icsi == 0 && res_usco == -1)
                {
                    frame_mode = FRAME_ICSI;
                }
                else if (res_usco == -1 && res_icsi == -1)
                {
                    frame_mode = FRAME_NONE;
                }
                continue;
            }
        }
    }
}

void encoder_init();
void encoder_Deinit();

#include "menu/pio.hpp"
void core1_main()
{
    core1_need_wait = 0; // 确保不进入发送状态

    while (1)
    {

        if (core1_usb_connect_uart)
        {
            // 其实在core0 UART初始化时已经重置
            // 重置复用引脚状态
            gpio_init(TEST_SPI_CS_PIN);
            gpio_set_dir(TEST_SPI_CS_PIN, GPIO_IN);
            gpio_disable_pulls(TEST_SPI_CS_PIN);
            ReadInCore1_nonblock();
        }
        if (i2c_run_in_core1)
        {
            // 重置复用引脚状态
            gpio_init(TEST_SPI_CS_PIN);
            gpio_set_dir(TEST_SPI_CS_PIN, GPIO_IN);
            gpio_disable_pulls(TEST_SPI_CS_PIN);
            if (i2cspi_test_running)
            {

                core1_test_internal();
            }
            else
            {

                core1_i2c_connect_USB();
            }
        }
        if (spi_run_in_core1)
        {
            // 重置复用引脚状态
            gpio_init(TBINPIN);
            gpio_set_dir(TBINPIN, GPIO_IN);
            gpio_disable_pulls(TBINPIN);

            core1_spi_connect_USB();
        }

        if (core1_irq_count_mode)
        {
            // 其实在core0 已经重置
            // 重置复用引脚状态
            gpio_init(TEST_SPI_CS_PIN);
            gpio_set_dir(TEST_SPI_CS_PIN, GPIO_IN);
            gpio_disable_pulls(TEST_SPI_CS_PIN);

            pio_highspeedfreq_init();
            stdio_filter_driver(&stdio_usb);
            while (core1_irq_count_mode)
            {

                SSD1306_PrintBufRaw();
                handle_pc_interface_commands();
                handle_pc_information();
            }
            stdio_filter_driver(NULL);
            // cancel_repeating_timer(&stream_timer);
            pio_highspeedfreq_deinit();
        }

        stdio_filter_driver(&stdio_usb);
        // Periodically allow core to send pending select-voltage messages
        Key_TrySendSelectVoltage();
        SSD1306_PrintBufRaw();
        // AI 模式下 core0 接管 stdin 读取，core1 让出，避免双核抢读
        if (!ai_owns_stdin())
        {
            handle_pc_information();
            handle_pc_interface_commands();
        }
        stdio_filter_driver(NULL);

        // printf("Core1 idle loop\r\n");
    }
}

#endif