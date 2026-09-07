#ifndef CONFIG_FLO_HPP
#define CONFIG_FLO_HPP

// Flexio unified configuration

#define DEBUG 1
#define count_of(a) (sizeof(a)/sizeof((a)[0]))

#define LICENSE_PLAINTEXT "Sc5ta6X8ufrWn18" // 设备授权校验明文

#define UBYTE   uint8_t
#define UWORD   uint16_t
#define WORD    int16_t
#define UDOUBLE uint32_t

// include pico headers commonly used
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"

// Board and peripheral definitions (moved from CONFIG_FLO.cpp)
// Baudrate options  28个
#define BAUD_NUMBERS 28

// 测试系统专用 I2C 配置
#define TEST_I2C_PORT i2c0
#define TEST_I2C_SDA_PIN RP_TD
#define TEST_I2C_SCL_PIN RP_RC

// 测试系统专用 SPI 配置
#define TEST_SPI_PORT spi0
#define TEST_SPI_SCK_PIN 18
#define TEST_SPI_TX_PIN 19
#define TEST_SPI_RX_PIN 4
#define TEST_SPI_CS_PIN 5

//系统端口
#define PULLUP_EN_PIN 28
#define SYS_MODE_PIN 29
#define SSD_RES 16

//正式端口
#define I2C_PORT i2c1
#define PICO_I2C_SDA_PIN 14
#define PICO_I2C_SCL_PIN 15

#define SELECTERPIN0 20
#define SELECTERPIN1 21

#define LEDRUNPIN 2

#define LEDTCPIN 10
#define LEDTDPIN 0
#define LEDSPIPIN 24
#define LEDTESTPIN 23
#define LEDDEBUGPIN 25
#define LEDTATBPIN 17

#define BUZZERPIN 7

#define UART_ID uart1

// Additional board pin defines (moved from CONFIG_FLO.cpp)
#define TAADCPIN 26
#define TBADCPIN 27
#define RP_TD 4
#define RP_RC 9
#define TAGENPIN RP_TD
#define TBGENPIN RP_RC
#define TCPWMOUT 12  //正常12
#define TDPWMOUT 1

//IN端口也使用GENPIN 的 CMOS 端口
#define TAINPIN TAGENPIN
#define TBINPIN TBGENPIN

#define KEYRIGHT 13
#define KEYLEFT 11
#define KEYUP 3
#define KEYDOWN 22
#define KEYOK 8
#define KEYCAN 6

// 其他全局定义
#define JOYSTICK_X_PIN    26   // ADC0
#define JOYSTICK_Y_PIN    27   // ADC1



// UART default pins (moved from CONFIG_FLO.cpp)
#define UART_TX_PIN     RP_TD
#define UART_RX_PIN     RP_RC

// UART 配置参数
#define UART_ID         uart1       // 使用UART0（可选uart0/uart1）
#define DATA_BITS       8           // 数据位（5-8）
#define STOP_BITS       1           // 停止位（1或2）
#define PARITY          UART_PARITY_NONE // 校验位（UART_PARITY_NONE/UART_PARITY_EVEN/UART_PARITY_ODD）

#define UART_TX_PIN     RP_TD          // GPIO0 (UART0 TX)
#define UART_RX_PIN     RP_RC           // GPIO1 (UART0 RX)

#define refresh_generalread 200000
#define refresh_generalread_ssd1306_draw 50000 // 在generalread中控制ssd1306的刷新频率，单位微秒
#define f_cal_num 30
#define f_highfrec_count_num 5

// safety voltage threshold
#define safe_V 3.5f

// ssd1306 OLED 显示器的高度和宽度
#define SSD1306_HEIGHT              64  //选择 32（4行） 或 64 （8行）
#define SSD1306_WIDTH               128

// Externs for commonly used globals
extern float footlengthDefault;
extern float footlength;
extern float select_voltage;
extern bool saveitem_insendmode;
extern bool menusetting_Dark;
extern bool menusetting_Light;

extern bool menusetting_ani_no;
extern bool menusetting_ani_fast;
extern bool menusetting_ani_normal;
extern bool menusetting_ani_slow;
extern bool menusetting_USB;
extern bool menusetting_UARTA;
extern bool menusetting_UARTB;

// Software and hardware versions
extern int PIVOsw;
extern int PIVOsw_sub;
extern int PIVOhw;

extern int PIVOsystem_mode_code;
extern bool is_device_licensed; // Added
extern bool keep_host_set;
extern int uart_baud_selection;
extern int uart_databits_selection;
extern int uart_parity_selection;
extern int uart_stopbits_selection;

// Expose some helpers implemented in CONFIG_FLO.cpp
#include <stdio.h>

#if DEBUG
#define Debug(__info,...) printf("Debug: " __info"\n",##__VA_ARGS__)
#else
#define Debug(__info,...)
#endif

// DEV helpers
UBYTE DEV_Digital_Read(UWORD Pin);
void DEV_Digital_Write(UWORD Pin, UBYTE Value);
void DEV_KEY_Config(UWORD Pin);

#endif // CONFIG_FLO_HPP
