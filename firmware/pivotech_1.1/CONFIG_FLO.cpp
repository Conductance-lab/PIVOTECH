/**
 * Flexio统一配置文件
 */

#ifndef _DEV_CONFIG_CPP_
#define _DEV_CONFIG_CPP_

#include <stdint.h>

/**
 * DEBUG调试 实现方法见下
 */
#define DEBUG 1
#define count_of(a) (sizeof(a) / sizeof((a)[0]))
/**
 * data
 **/
#define UBYTE uint8_t
#define UWORD uint16_t
#define WORD int16_t
#define UDOUBLE uint32_t
#define gpio_pin_enum uint16_t // easyui部分的使用参数

/*菜单设置全局变量控制*/

bool Enable_PC_Interface = 0;

int PIVOsystem_mode_code = 0;
int PIVOsw = 1;     // 软件主版本号
int PIVOsw_sub = 1; // 软件子版本号
int PIVOhw = 0;     // 硬件版本号
float select_voltage = 0.0f; // 选择器电压

bool is_device_licensed = false;       // Flash License State
uint8_t stored_license_data[16] = {0}; // License Data Buffer
uint8_t stored_flash_id[8] = {0};      // Flash ID Buffer
bool license_data_loaded = false;      // License Data Load Flag

volatile bool menusetting_Dark = 1, menusetting_Light = 0;
volatile bool menusetting_ani_no = 0, menusetting_ani_fast = 0, menusetting_ani_normal = 1, menusetting_ani_slow = 0;
volatile bool menusetting_USB = 1, menusetting_UARTA = 0, menusetting_UARTB = 0;

volatile bool menusetting_LED = 1, menusetting_BUZ = 1;

volatile bool saveitem_insendmode = 0; // 在sendsetting模式下 但是是存储动作 需要动画展示
// 动画步长
float footlengthDefault = 0.05;
float footlength = footlengthDefault; // 长按时加速

int SettingBaudrate = 115200; // 后续删除

/**
 * 引用外部库的统一管理
 */
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "stdio.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include <stdlib.h> //itoa()
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <string.h> //memset()
#include <math.h>
#include "hardware/timer.h"
#include <stdbool.h>
#include <cstdarg>

/**
 * 字体库的统一管理
 */

/******************************************************************************
function:	以下为更改底层输入，方便后续迁移；DEBUG的打印实现
parameter:
Info:
******************************************************************************/

/*
 * DEBUG调试实现
 */
#if DEBUG
#define Debug(__info, ...) printf("Debug: " __info "\n", ##__VA_ARGS__)
#else
#define Debug(__info, ...)
#endif

uint slice_num;
/**
 * GPIO read and write
 **/
void DEV_Digital_Write(UWORD Pin, UBYTE Value)
{
    gpio_put(Pin, Value);
}

UBYTE DEV_Digital_Read(UWORD Pin)
{
    return gpio_get(Pin);
}

/**
 * SPI
 **/
// void DEV_SPI_WriteByte(uint8_t Value)
// {
//     spi_write_blocking(SPI_PORT, &Value, 1);
// }

// void DEV_SPI_Write_nByte(uint8_t pData[], uint32_t Len)
// {
//     spi_write_blocking(SPI_PORT, pData, Len);
// }

/**
 * I2C
 **/

// void DEV_I2C_Write(uint8_t addr, uint8_t reg, uint8_t Value)
// {
//     uint8_t data[2] = {reg, Value};
//     i2c_write_timeout_us(I2C_PORT, addr, data, 2, false, 1000);
// }

// void DEV_I2C_Write_nByte(uint8_t addr, uint8_t *pData, uint32_t Len)
// {
//     i2c_write_timeout_us(I2C_PORT, addr, pData, Len, false, 1000);
// }

// uint8_t DEV_I2C_ReadByte(uint8_t addr, uint8_t reg)
// {
//     uint8_t buf;
//     i2c_write_timeout_us(I2C_PORT,addr,&reg,1,true,1000);
//     i2c_read_timeout_us(I2C_PORT,addr,&buf,1,false,1000);
//     return buf;
// }

/**
 * GPIO Mode
 **/
// void DEV_GPIO_Mode(UWORD Pin, UWORD Mode)
// {
//     gpio_init(Pin);
//     if(Mode == 0 || Mode == GPIO_IN) {
//         gpio_set_dir(Pin, GPIO_IN);
//     } else {
//         gpio_set_dir(Pin, GPIO_OUT);
//     }
// }

/**
 * KEY Config
 **/
void DEV_KEY_Config(UWORD Pin)
{
    gpio_init(Pin);
    gpio_pull_up(Pin);
    gpio_set_dir(Pin, GPIO_IN);
}

/**
 * delay x ms
 **/
void DEV_Delay_ms(UDOUBLE xms)
{
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (to_ms_since_boot(get_absolute_time()) - start < xms)
        ;
}

void DEV_Delay_us(UDOUBLE xus)
{
    uint32_t start = to_us_since_boot(get_absolute_time());
    while (to_us_since_boot(get_absolute_time()) - start < xus)
        ;
}

// void DEV_GPIO_Init(void)
// {
//     DEV_GPIO_Mode(LCD_RST_PIN, 1);
//     DEV_GPIO_Mode(LCD_DC_PIN, 1);
//     DEV_GPIO_Mode(LCD_CS_PIN, 1);
//     DEV_GPIO_Mode(LCD_BL_PIN, 1);

//     DEV_GPIO_Mode(LCD_CS_PIN, 1);
//     DEV_GPIO_Mode(LCD_BL_PIN, 1);

//     DEV_Digital_Write(LCD_CS_PIN, 1);
//     DEV_Digital_Write(LCD_DC_PIN, 0);
//     DEV_Digital_Write(LCD_BL_PIN, 1);
// }
/******************************************************************************
function:	Module Initialize, the library and initialize the pins, SPI protocol
parameter:
Info:
******************************************************************************/
// UBYTE DEV_Module_Init(void)
// {
//     stdio_init_all();
//     // SPI Config
//     spi_init(SPI_PORT, 10000 * 1000);
//     gpio_set_function(LCD_CLK_PIN, GPIO_FUNC_SPI);
//     gpio_set_function(LCD_MOSI_PIN, GPIO_FUNC_SPI);

//     // GPIO Config
//     DEV_GPIO_Init();

//     // PWM Config
//     gpio_set_function(LCD_BL_PIN, GPIO_FUNC_PWM);
//     slice_num = pwm_gpio_to_slice_num(LCD_BL_PIN);
//     pwm_set_wrap(slice_num, 100);
//     pwm_set_chan_level(slice_num, PWM_CHAN_B, 1);
//     pwm_set_clkdiv(slice_num,50);
//     pwm_set_enabled(slice_num, true);

//     //I2C Config
//     i2c_init(I2C_PORT,300*1000);
//     gpio_set_function(LCD_SDA_PIN,GPIO_FUNC_I2C);
//     gpio_set_function(LCD_SCL_PIN,GPIO_FUNC_I2C);
//     gpio_pull_up(LCD_SDA_PIN);
//     gpio_pull_up(LCD_SCL_PIN);

//     Debug("DEV_Module_Init OK \r\n");
//     return 0;
// }

#endif