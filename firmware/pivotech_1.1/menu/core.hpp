#ifndef MENU_CORE_HPP
#define MENU_CORE_HPP



/* Core functions exported from menu/core.cpp */
void core1_main();

/* SYS,C 握手应答（core1 process_pc_command 与 AI 模式 aitest 接收机共用） */
void Pc_ReplySystemInfo();

/* Serial buffer and flags (defined in menu/core.cpp) */
#define SERIAL_RX_BUFFER_SIZE 256
extern char serial_rx_buffer[SERIAL_RX_BUFFER_SIZE];
extern int serial_rx_index;
extern bool TXisReady;
extern bool RXisReady;

// I2C monitor buffer and device presence (defined in menu/core.cpp)
#define I2C_MONITOR_BUFFER_SIZE 512
extern char i2ctest_rx_buffer[I2C_MONITOR_BUFFER_SIZE];
extern int i2ctest_rx_index;
extern bool prev_present[128];

// SPI monitor buffer (defined in menu/core.cpp)
#define SPI_MONITOR_BUFFER_SIZE 32
extern uint8_t spitest_tx_buffer[SPI_MONITOR_BUFFER_SIZE];
extern uint8_t spitest_rx_buffer[SPI_MONITOR_BUFFER_SIZE];
extern int spitest_tx_index;
extern int spitest_rx_index;
extern bool spitest_buf_full;
extern bool spitext_ifupdate;
extern bool spi_master_mode; // SPI模式标志

extern bool core1_usb_connect_uart; // notify core1 to connect USB to UART


extern bool if_need_send_menu_data; // 进入Menu发送数据到上位机的标志

extern bool opnPCchangemode; // 触发向上位机发送数据的标志


// Core1 mode flags (defined in menu/core.cpp)
extern bool core1_irq_count_mode;

// SPI control flag for Core1 (used to let menu notify core1 to run SPI tests)
extern bool spi_run_in_core1;

/* Selector and control functions */
void ResetOpn(void);
void Selector_TA_TX_TB_RX(void);
void Selector_TA_RX_TB_TX(void);

int handle_pc_information(void); // 工具类SSD MPU由core0调用


#endif // MENU_CORE_HPP
