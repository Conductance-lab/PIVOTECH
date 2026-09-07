#ifndef _SELECT_CPP_
#define _SELECT_CPP_
#include "pico/stdlib.h"
#include "CONFIG_FLO.hpp"

// 00 全部TD TD

// void Selector_Unenable(){

//         // 避免复用线干扰
//         gpio_init(TEST_SPI_CS_PIN);
//         gpio_set_dir(TEST_SPI_CS_PIN, GPIO_IN);

//         gpio_put(PULLUP_EN_PIN, 1); //失能
//         gpio_put(SELECTERPIN0, 0);
//         gpio_put(SELECTERPIN1, 0);
// }

// //general read / encoder read
// void Selector_Read(){
//         // 避免复用线干扰
//         gpio_init(TEST_SPI_CS_PIN);
//         gpio_set_dir(TEST_SPI_CS_PIN, GPIO_IN);

//         gpio_put(PULLUP_EN_PIN, 0); //使能
//         gpio_put(SELECTERPIN0, 1);
//         gpio_put(SELECTERPIN1, 0);
// }

void Selector_TA_TX_TB_RX()
{
        // 避免复用线干扰
        gpio_init(TEST_SPI_CS_PIN);
        gpio_set_dir(TEST_SPI_CS_PIN, GPIO_IN);
        gpio_disable_pulls(TEST_SPI_CS_PIN);

        gpio_put(PULLUP_EN_PIN, 1); // 使能

        gpio_put(SELECTERPIN0, 0);
        gpio_put(SELECTERPIN1, 1);

        return;
}

void Selector_SPI()
{
        // 避免复用线干扰
        gpio_init(RP_RC);
        gpio_set_dir(RP_RC, GPIO_IN);
        gpio_disable_pulls(RP_RC);

        gpio_put(PULLUP_EN_PIN, 1); // 使能

        gpio_put(SELECTERPIN0, 0);
        gpio_put(SELECTERPIN1, 1);

        return;
}

void Selector_TA_RX_TB_TX()
{
        // 避免复用线干扰
        gpio_init(TEST_SPI_CS_PIN);
        gpio_set_dir(TEST_SPI_CS_PIN, GPIO_IN);
        gpio_disable_pulls(TEST_SPI_CS_PIN);

        gpio_put(PULLUP_EN_PIN, 1); // 使能

        gpio_put(SELECTERPIN0, 1);
        gpio_put(SELECTERPIN1, 0);
        return;
}

void Selector_Read_TATB_ADC()
{
        // 避免复用线干扰
        gpio_init(TEST_SPI_CS_PIN);
        gpio_set_dir(TEST_SPI_CS_PIN, GPIO_IN);
        gpio_disable_pulls(TEST_SPI_CS_PIN);

        gpio_put(PULLUP_EN_PIN, 1); // 使能

        gpio_put(SELECTERPIN0, 0);
        gpio_put(SELECTERPIN1, 0);
        return;
}

void Selector_Read_readSYS_ADC(){
        // 避免复用线干扰
        gpio_init(TEST_SPI_CS_PIN);
        gpio_set_dir(TEST_SPI_CS_PIN, GPIO_IN);
        gpio_disable_pulls(TEST_SPI_CS_PIN);

        gpio_put(PULLUP_EN_PIN, 1); // 使能

        gpio_put(SELECTERPIN0, 1);
        gpio_put(SELECTERPIN1, 0);
        return;
}
void Selector_Read_XY_ADC()
{
        // 避免复用线干扰
        gpio_init(TEST_SPI_CS_PIN);
        gpio_set_dir(TEST_SPI_CS_PIN, GPIO_IN);
        gpio_disable_pulls(TEST_SPI_CS_PIN);

        gpio_put(PULLUP_EN_PIN, 1); // 使能

        gpio_put(SELECTERPIN0, 1);
        gpio_put(SELECTERPIN1, 1);
        return;
}
// void Selector_I2C()
// {
//         // 避免复用线干扰
//         gpio_init(TEST_SPI_CS_PIN);
//         gpio_set_dir(TEST_SPI_CS_PIN, GPIO_IN);

//         gpio_put(PULLUP_EN_PIN, 0); //使能
//         gpio_put(SELECTERPIN0, 1);
//         gpio_put(SELECTERPIN1, 0);
//         return;
// }

// AI 测试模式：把前端摆到“脚本可直控 GPIO”的默认态。
// 已按硬件核验（2026-09-06）：AI 模式下选择器置 SEL=00（SEL0=0、SEL1=0）。
void Selector_AI_Free()
{
        gpio_init(TEST_SPI_CS_PIN);
        gpio_set_dir(TEST_SPI_CS_PIN, GPIO_IN);
        gpio_disable_pulls(TEST_SPI_CS_PIN);

        gpio_put(PULLUP_EN_PIN, 1); // 使能外部上拉

        gpio_put(SELECTERPIN0, 0); // SEL=00
        gpio_put(SELECTERPIN1, 0);
        return;
}

#endif