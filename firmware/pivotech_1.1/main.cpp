/*
PIVOTECH发送：
设备状态包 #PIVOCOTX#id=...,hv=...,sv=...,lic=...#PCS#:
串口文本数据 #PIVOUSTX# ... #PTS#
视频推流数据 #PIVOSCAN# ... (分段数据) ... #PSM# ... (分段数据) ... #PTS#
PIVOTECH接收：
配置命令 #PIVOUSRX# ... #PRS#:
菜单导入 #PIVOCONF# ... #END_CONFIG#
e.g.#PIVOCONF# #Gen.v.1.0.0b#$MainPage;D,[3Example Menu];J,Radio_List;$Radio_List;R,choose1,NS,1;R,choose2,NS,0;R,choose3,NS,0;R,choose4,NS,0;M,Send Choosed,OT;$MainPage;S,myswtich,STV,0;C,mycheckbox,NS,1;R,Page1,NS,0;$Page1;J,Page2;$Page2;S,Here Page3,NS,123;$MainPage;J,Value_List;$Value_List;V,Float,SV,F,-12.2345;V,UFloat,RV,UF,0.0931;V,Int,SV,I,-100;V,Uint,RTV,UI,8;$MainPage;M,Hello,ST;X,0,5,1,1,1,-3,1,3,0;#END_CONFIG#

*/

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "menu/menu.hpp"
#include "CONFIG_FLO.hpp"
#include "menu/UI.hpp"
#include "menu/key.hpp"
#include "menu/core.hpp"
#include "menu/StateLED.hpp"
#include "menu/flash.hpp"          // Added for license check
#include "menu/license_crypto.hpp" // Added for encryption test
#include "tusb.h"                  // Include TinyUSB header for CDC functions
#include "menu/flash.hpp"

#include "read/serialread.hpp"
#include "read/functions.hpp"
#include "read/pwmoutput.hpp"
#include "pico/binary_info.h"
#include "read/uartconfig.hpp"
#include "read/select.hpp"
#include "read/encoderread.hpp"
#include "read/generalread.hpp"
#include "read/i2ctest.hpp"
#include "read/spitest.hpp"
#include "read/hardware_core1.hpp"
#include "pico/unique_id.h"
#include "read/mod_selection.hpp"
#include "read/aitest.hpp"
#include "read/select.hpp"
#include <stdint.h>
#include "pico/bootrom.h"

void process_hardware_adc_value(uint16_t adc_value) // 处理ADC值以更新PIVOhw
{
    extern int PIVOhw; // 引用外部变量
    if (adc_value <= 40)
        {
            PIVOhw = 1;
        }
}

char usb_serial[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];

// --- Custom USB Descriptors ---
// (We override the weak/default descriptors from pico_stdio_usb)
char const *string_desc_arr[] =
    {
        (const char[]){0x09, 0x04}, // 0: Supported language = English
        "PIVOTECH",                 // 1: Manufacturer
        "PIVOTECH",                 // 2: Product
        usb_serial,                 // 3: Serial Number
        "PIVOTECH",                 // 4: CDC UART Interface Name
};

tusb_desc_device_t const desc_device =
    {
        .bLength = sizeof(tusb_desc_device_t),
        .bDescriptorType = TUSB_DESC_DEVICE,
        .bcdUSB = 0x0200,
        .bDeviceClass = TUSB_CLASS_MISC,
        .bDeviceSubClass = MISC_SUBCLASS_COMMON,
        .bDeviceProtocol = MISC_PROTOCOL_IAD,
        .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

        .idVendor = 0x2E8A,  // Raspberry Pi VID
        .idProduct = 0x1F0F, // Pico SDK CDC PID
        .bcdDevice = 0x0100,

        .iManufacturer = 0x01, // string_desc_arr[1]
        .iProduct = 0x02,      // string_desc_arr[2]
        .iSerialNumber = 0x03, // string_desc_arr[3]

        .bNumConfigurations = 0x01};

extern "C" uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&desc_device;
}

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)

enum
{
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL
};

uint8_t const desc_configuration[] =
    {
        TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0, 100),
        TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, 0x81, 8, 0x02, 0x82, 64)};

extern "C" uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_configuration;
}

static uint16_t _desc_str[32];

extern "C" uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    uint8_t chr_count;

    if (index == 0)
    {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    }
    else
    {
        if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0])))
            return NULL;

        const char *str = string_desc_arr[index];

        chr_count = (uint8_t)strlen(str);
        if (chr_count > 31)
            chr_count = 31;

        for (uint8_t i = 0; i < chr_count; i++)
        {
            _desc_str[1 + i] = str[i];
        }
    }

    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);

    return _desc_str;
}
// --- End Custom USB Descriptors ---

/*待解决大问题*/
/*
1. UART校验位仅三个选项 无校验 偶校验 奇校验
*/

/*
待解决小问题
字体对所有的窗口都有影响
MPU6050窗口字体
I2C通信模拟
*/

/*长久规划*/
/*
通过PIO实现逻辑分析仪 提升识别频率
接入JI2C JSPI实现上位机调试I2C SPI

*/

void ALLInit()
{
    // USB init
    stdio_usb_init();

    // i2c初始化
    // bi_decl(bi_2pins_with_func(PICO_I2C_SDA_PIN, PICO_I2C_SCL_PIN, GPIO_FUNC_I2C));
    i2c_init(I2C_PORT, SSD1306_I2C_CLK * 1000);
    gpio_set_function(PICO_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_I2C_SDA_PIN);
    gpio_pull_up(PICO_I2C_SCL_PIN);

    // SSD INIT
    SSD1306_init();

    // KEY INIT
    DEV_KEY_Config(KEYOK);
    DEV_KEY_Config(KEYCAN);
    DEV_KEY_Config(KEYUP);
    DEV_KEY_Config(KEYDOWN);
    DEV_KEY_Config(KEYLEFT);
    DEV_KEY_Config(KEYRIGHT);

    // GENGERAL LED INIT

    gpio_init(LEDSPIPIN);
    gpio_set_function(LEDSPIPIN, GPIO_FUNC_PWM);
    uint led_i2c_slice = pwm_gpio_to_slice_num(LEDSPIPIN);
    pwm_config led_i2c_cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&led_i2c_cfg, 125.0f); // 125MHz / 125 = 1MHz
    pwm_config_set_wrap(&led_i2c_cfg, 1000);     // 1MHz / 1000 = 1kHz
    pwm_init(led_i2c_slice, &led_i2c_cfg, true);
    pwm_set_gpio_level(LEDSPIPIN, 0); // 默认关闭

    gpio_init(LEDTESTPIN);
    gpio_set_function(LEDTESTPIN, GPIO_FUNC_PWM);
    uint led_spi_slice = pwm_gpio_to_slice_num(LEDTESTPIN);
    pwm_config led_spi_cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&led_spi_cfg, 125.0f); // 125MHz / 125 = 1MHz
    pwm_config_set_wrap(&led_spi_cfg, 1000);     // 1MHz / 1000 = 1kHz
    pwm_init(led_spi_slice, &led_spi_cfg, true);
    pwm_set_gpio_level(LEDTESTPIN, 0); // 默认关闭

    gpio_init(LEDDEBUGPIN);
    gpio_set_function(LEDDEBUGPIN, GPIO_FUNC_PWM);
    uint led_debug_slice = pwm_gpio_to_slice_num(LEDDEBUGPIN);
    pwm_config led_debug_cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&led_debug_cfg, 125.0f); // 125MHz / 125 = 1MHz
    pwm_config_set_wrap(&led_debug_cfg, 1000);     // 1MHz / 1000 = 1kHz
    pwm_init(led_debug_slice, &led_debug_cfg, true);
    pwm_set_gpio_level(LEDDEBUGPIN, 0); // 默认关闭

    gpio_init(LEDTATBPIN);
    gpio_set_function(LEDTATBPIN, GPIO_FUNC_PWM);
    uint led_tatb_slice = pwm_gpio_to_slice_num(LEDTATBPIN);
    pwm_config led_tatb_cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&led_tatb_cfg, 125.0f); // 125MHz / 125 = 1MHz
    pwm_config_set_wrap(&led_tatb_cfg, 1000);     // 1MHz / 1000 = 1kHz
    pwm_init(led_tatb_slice, &led_tatb_cfg, true);
    pwm_set_gpio_level(LEDTATBPIN, 0); // 默认关闭

    // PWM LED INIT
    gpio_init(LEDRUNPIN);
    // 具体调节在 定时中断中
    gpio_set_function(LEDRUNPIN, GPIO_FUNC_PWM);
    uint led_run_slice = pwm_gpio_to_slice_num(LEDRUNPIN);
    pwm_config led_run_cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&led_run_cfg, 125.0f); // 125MHz / 125 = 1MHz
    pwm_config_set_wrap(&led_run_cfg, 10000);    // 为了视觉效果一样 对于RUNLED 特殊处理
    pwm_init(led_run_slice, &led_run_cfg, true);
    pwm_set_gpio_level(LEDRUNPIN, 0); // 默认关闭

    // TC 同步灯
    gpio_init(LEDTCPIN);
    gpio_set_function(LEDTCPIN, GPIO_FUNC_PWM);
    uint led_tc_slice = pwm_gpio_to_slice_num(LEDTCPIN);
    pwm_config led_tc_cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&led_tc_cfg, 125.0f); // 125MHz / 125 = 1MHz
    pwm_config_set_wrap(&led_tc_cfg, 1000);     // 1MHz / 1000 = 1kHz
    pwm_init(led_tc_slice, &led_tc_cfg, true);
    pwm_set_gpio_level(LEDTCPIN, 0); // 默认关闭


    // TD 同步灯
    gpio_init(LEDTDPIN);
    gpio_set_function(LEDTDPIN, GPIO_FUNC_PWM);
    uint led_td_slice = pwm_gpio_to_slice_num(LEDTDPIN);
    pwm_config led_td_cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&led_td_cfg, 125.0f); // 125MHz / 125 = 1MHz
    pwm_config_set_wrap(&led_td_cfg, 1000);     // 1MHz / 1000 = 1kHz
    pwm_init(led_td_slice, &led_td_cfg, true);
    pwm_set_gpio_level(LEDTDPIN, 0); // 默认关闭

    // PWM  TC
    gpio_init(TCPWMOUT);
    gpio_set_function(TCPWMOUT, GPIO_FUNC_PWM);
    uint tc_pwm_slice = pwm_gpio_to_slice_num(TCPWMOUT);
    pwm_config tc_pwm_cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&tc_pwm_cfg, 125.0f); // 125MHz / 125 = 1MHz
    pwm_config_set_wrap(&tc_pwm_cfg, 1000);     // 1MHz / 1000 = 1kHz
    pwm_init(tc_pwm_slice, &tc_pwm_cfg, true);
    pwm_set_gpio_level(TCPWMOUT, 0); // 默认关闭

    //将TC驱动能力改为12mA
    gpio_set_drive_strength(TCPWMOUT, GPIO_DRIVE_STRENGTH_12MA);

    // PWM TD
    gpio_init(LEDTDPIN);
    gpio_set_function(LEDTDPIN, GPIO_FUNC_PWM);
    uint td_pwm_slice = pwm_gpio_to_slice_num(LEDTDPIN);
    pwm_config td_pwm_cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&td_pwm_cfg, 125.0f); // 125MHz / 125 = 1MHz
    pwm_config_set_wrap(&td_pwm_cfg, 1000);     // 1MHz / 1000 = 1kHz
    pwm_init(td_pwm_slice, &td_pwm_cfg, true);
    pwm_set_gpio_level(LEDTDPIN, 0); // 默认关闭

    // selecter INIT
    gpio_init(SELECTERPIN0);
    gpio_set_dir(SELECTERPIN0, GPIO_OUT);
    gpio_init(SELECTERPIN1);
    gpio_set_dir(SELECTERPIN1, GPIO_OUT);

    // SPI复用线初始化为输入，避免干扰
    gpio_init(TEST_SPI_CS_PIN);
    gpio_set_dir(TEST_SPI_CS_PIN, GPIO_IN);
    gpio_disable_pulls(TEST_SPI_CS_PIN);

    // pullup enanle INIT
    gpio_init(PULLUP_EN_PIN);
    gpio_set_dir(PULLUP_EN_PIN, GPIO_OUT);

    // ADC init
    // ADC 初始化
    adc_init();
    adc_gpio_init(TAADCPIN);
    adc_gpio_init(TBADCPIN);
}

void LED_general_init()
{
    // GENGERAL LED INIT

    gpio_init(LEDSPIPIN);
    gpio_set_dir(LEDSPIPIN, GPIO_OUT);

    gpio_init(LEDTESTPIN);
    gpio_set_dir(LEDTESTPIN, GPIO_OUT);

    gpio_init(LEDDEBUGPIN);
    gpio_set_dir(LEDDEBUGPIN, GPIO_OUT);

    gpio_init(LEDTATBPIN);
    gpio_set_dir(LEDTATBPIN, GPIO_OUT);

    // PWM LED INIT
    gpio_init(LEDRUNPIN);
    // 具体调节在 定时中断中
    gpio_set_function(LEDRUNPIN, GPIO_FUNC_PWM);
    uint led_run_slice = pwm_gpio_to_slice_num(LEDRUNPIN);
    pwm_config led_run_cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&led_run_cfg, 125.0f); // 125MHz / 125 = 1MHz
    pwm_config_set_wrap(&led_run_cfg, 1000);     // 1MHz / 1000 = 1kHz
    pwm_init(led_run_slice, &led_run_cfg, true);
    pwm_set_gpio_level(LEDRUNPIN, 0); // 默认关闭

    // TD 同步灯
    gpio_init(LEDTDPIN);
    gpio_set_function(LEDTDPIN, GPIO_FUNC_PWM);
    uint led_td_slice = pwm_gpio_to_slice_num(LEDTDPIN);
    pwm_config led_td_cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&led_td_cfg, 125.0f); // 125MHz / 125 = 1MHz
    pwm_config_set_wrap(&led_td_cfg, 1000);     // 1MHz / 1000 = 1kHz
    pwm_init(led_td_slice, &led_td_cfg, true);
    pwm_set_gpio_level(LEDTDPIN, 0); // 默认关闭

    // TC 同步灯
    gpio_init(LEDTCPIN);
    gpio_set_function(LEDTCPIN, GPIO_FUNC_PWM);
    uint led_tc_slice = pwm_gpio_to_slice_num(LEDTCPIN);
    pwm_config led_tc_cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&led_tc_cfg, 125.0f); // 125MHz / 125 = 1MHz
    pwm_config_set_wrap(&led_tc_cfg, 1000);     // 1MHz / 1000 = 1kHz
    pwm_init(led_tc_slice, &led_tc_cfg, true);
    pwm_set_gpio_level(LEDTCPIN, 0); // 默认关闭

    // buzzer init 不能提前初始化，否则会和I2CLED的PWM冲突
    gpio_init(BUZZERPIN);
    // 具体调节在 定时中断中
    gpio_set_function(BUZZERPIN, GPIO_FUNC_PWM);
    uint buzzer_slice = pwm_gpio_to_slice_num(BUZZERPIN);
    pwm_config buzzer_cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&buzzer_cfg, 125.0f); // 125MHz / 125 = 1MHz
    pwm_config_set_wrap(&buzzer_cfg, 1000);     // 1MHz / 1000 = 1kHz
    pwm_init(buzzer_slice, &buzzer_cfg, true);
    pwm_set_gpio_level(BUZZERPIN, 0); // 默认静音
}

#include "hardware/uart.h"
void UART_Init()
{

    // 重置复用引脚状态
    gpio_init(TEST_SPI_CS_PIN);
    gpio_set_dir(TEST_SPI_CS_PIN, GPIO_IN);
    gpio_disable_pulls(TEST_SPI_CS_PIN);

    // UART init
    gpio_init(UART_TX_PIN);
    gpio_init(UART_RX_PIN);

    uart_init(UART_ID, 115200); // 临时初始化波特率115200
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    gpio_pull_up(UART_TX_PIN);
    gpio_pull_up(UART_RX_PIN);
    uart_set_fifo_enabled(UART_ID, true);
    if (keep_host_set)
    { // 在此应用UART参数
        apply_uart_settings_from_host();
    }
    else
    {
        apply_uart_settings();
    }
}

static bool show_license_screen()
{
    extern uint8_t buf[SSD1306_BUF_LEN];
    extern struct render_area frame_area;

    // 使用状态 LED API 控制 LEDRUNPIN
    uint32_t last_state_change = time_us_32();
    bool state_on = true;
    bool screen_dirty = true;

    while (true)
    {
        if (verify_authorization())
        {
            ClearScreen(buf, &frame_area);
            Paint_DrawString_EN_CenterAtX(buf, 64, 24, "授权成功！", &Font12, true);
            render(buf, &frame_area);
            sleep_ms(1500);
            return true;
        }

        uint32_t now = time_us_32();
        if (now - last_state_change >= 1000000)
        {
            last_state_change += 1000000;
            if (state_on)
            {
                Close_StateLED();
            }
            else
            {
                Open_StateLED();
            }
            state_on = !state_on;
        }

        if (screen_dirty)
        {
            ClearScreen(buf, &frame_area);
            Paint_DrawString_EN_CenterAtX(buf, 64, 16, "授权无效！", &Font12, true);
            Paint_DrawString_EN_CenterAtX(buf, 64, 40, "功能受限", &Font12, true);
            render(buf, &frame_area);
            screen_dirty = false;
        }

        if (opnEnter || opnExit || opnUp || opnDown || opnLeft || opnRight)
        {
            ResetOpn();
            return false;
        }

        sleep_ms(100);
    }
}

int main()
{

    // 获取设备唯一ID
    pico_get_unique_board_id_string(usb_serial, sizeof(usb_serial));

    stdio_init_all();

    // // 擦除授权
    // erase_license_data(); // 在正式发布版本中应当移除这一行，以免每次重启都清除授权数据
    // while (1)
    // {
    //     /* code */
    // }

    // 主动授权注册板子分支

    // encrypt_and_store_key(); // 在正式发布版本中，这一行应当放在特权设置模式内，或通过特定按键组合触发，以避免每次重启都重新生成授权数据

    // const char buffer[] = ";;#PIVOCONF# #Sample Menu#$MainPage;D,[Example Menu];J,Radio_List;S,myswtich,STV,0;C,mycheckbox,NS,1;R,Page1,NS,0;J,Value_List;J,Page1;M,Hello,ST;$Radio_List;R,choose1,NS,1;R,choose2,NS,0;R,choose3,NS,0;R,choose4,NS,0;M,Send Choosed,OT;$Value_List;V,Float,SV,F,-12.2345;V,UFloat,RV,UF,0.0931;V,Int,SV,I,-100;V,Uint,RTV,UI,8;$Page1;J,Page2;$Page2;S,Here Page3,NS,0;X,0,6,0,0,2,-16,3,0,0;#END_CONFIG#";
    // char preload_configdata[CONFIG_DATA_MAX_LEN] = {0};
    // strncpy(preload_configdata, buffer, CONFIG_DATA_MAX_LEN - 1);
    // save_configdata(preload_configdata); // 预先加载配置数据以供测试使用

    // const char settings_buffer[] = "X,0,6,0,0,2,-16,3,0,0;"; // 预设UART设置字符串
    // char preload_settings[CONFIG_SETTINGS_MAX_LEN] = {0};
    // strncpy(preload_settings, settings_buffer, CONFIG_SETTINGS_MAX_LEN - 1);
    // save_configsettings(preload_settings); // 预先加载UART设置数据以供测试使用

    // // // 重启进入bootloader模式以测试授权数据的持久化
    // reset_usb_boot(0, 0); // 进入bootloader

    // return 0;

    // 结束 主动授权

    // 优先初始化 I2C 和 OLED 以便支持错误信息的屏幕显示
    // RES SSD1306
    gpio_init(SSD_RES);
    gpio_set_dir(SSD_RES, GPIO_OUT);
    gpio_put(SSD_RES, 0);
    sleep_ms(1);
    gpio_put(SSD_RES, 1);
    sleep_ms(1);

    ALLInit(); // UI : I2C SSD KEYS UI UART初始化 等


    // 读取设备验证数据 (仅作解密验证，不再直接走强制加密写入)
    // encrypt_and_store_key(); 这一行现在应当移除或放在特权设置模式内

    MenuInit(); // 菜单输入

    Timer_Key_Init(); // 中断 按键触发 LED触发

    extern uint8_t stored_license_data[16]; // 从 CONFIG_FLO.cpp 引入授权数据缓冲区
    extern uint8_t stored_flash_id[8];      // 从 CONFIG_FLO.cpp 引入 Flash ID 缓冲区

    // 在core1启动之前保存lic 到 stored_license_data
    load_license_data(stored_license_data, 16); // 读取验证
    pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);
    for (size_t i = 0; i < sizeof(stored_flash_id); ++i)
    {
        stored_flash_id[i] = board_id.id[i];
    }
    // printf("[TEST] License Data Read Back: ");
    // for (int i = 0; i < 16; i++)
    // {
    //     printf("%02x ", stored_data[i]);
    // }

    multicore_launch_core1(core1_main);

    if (!verify_authorization())
    {
        show_license_screen();
    }

    // ADC首次读取
    Selector_Read_readSYS_ADC(); // 预先读取一次ADC数据，确保后续功能切换时有最新的ADC值可用
    {
        adc_select_input(TBADCPIN - 26);
        uint16_t hardware_adc_raw = (uint16_t)adc_read();
        process_hardware_adc_value(hardware_adc_raw); // 处理ADC值以更新select_voltage
        
    }

    UI_START(); // 启动动画

    if_led_initmode = 0; // LED初始模式关闭

    LED_general_init(); // 通用LED灯初始化

    StartStateLED(); // 初始完成LED灯告示

    Pwm_Output_Init(); // 复位并初始化四路PWM输出

    ///////////////////////////////////////////////////////////
    // test 调试入口

    ////////////////////////////////////////////////////////

    while (true)
    {

        Selector_Read_readSYS_ADC();
        int select = functionmenu(); // 功能切换渲染入口

        extern int functionstartx;
        if (!verify_authorization() && select != 0)
        { // 如果未授权 仅可使用功能0
            select = 0;
            nowselect_function = 0;
            functionstartx = SSD1306_WIDTH / 2 - 30;
            show_license_screen();
        }

        ResetOpn();

        switch (select)
        {
        case 0:

            // 现支持2X10Mhz 高频采集 完全不冲突 基于复杂的中断管理 复用引脚功能 双核计算 pio可编程IO 实现高频率采集
            // 待解决：在F<0.1 时 切换至下拉检测电路来判定 高电平/低电平/悬空 只有双浮空的时候进行短路测试
            core1_irq_count_mode = 1;

            Selector_Read_TATB_ADC();
            // Selector_TA_TX_TB_RX();

            Frequency_Read_Init();

            // gpio_put(0, 1); // 关闭上拉

            general_read();
            // 退出
            Frequency_Read_Deinit();
            core1_irq_count_mode = 0;
            break;
        case 1: // PWM output 10~100kHz输出
            // 基本完成
            // 待解决：VREF改为高阻VREF 输出
            Selector_TA_RX_TB_TX();
            pwmoutitem_infunction = 1;
            Pwm_Output_Init();

            pwmoutput();
            break;
        case 2:
            // 基本完成 待测试
            // 后续计划所有通信内容进行合并 ENCODER I2C SPI UART

            main_mod_selection(); // 进入选择器界面

            break;
        case 3:
            // 待完成 I2C sniffer pio模式
            i2c_run_in_core1 = true; // 通知core1停止I2C通信
                                     // 初始化选择器
            extern bool i2ctest_scl_on_left;
            if (i2ctest_scl_on_left)
                Selector_TA_TX_TB_RX();
            else
                Selector_TA_RX_TB_TX();
            i2ctest_infunction = 1;
            i2ctest();
            i2c_run_in_core1 = false; // 通知core1恢复I2C通信
            break;
        case 4:
            // SPI通信测试 待完成
            // 打开SPI后台任务
            Selector_SPI();
            spi_run_in_core1 = true;
            spitest_infunction = 1;
            spitest();
            spi_run_in_core1 = false;

            break;

        case 5:
            // UART校验位只有三种
            UART_Init(); // 启用UART探针
            if (global_TA_serial_mode == 0 )
            {
                Selector_TA_TX_TB_RX();
            }
            else if (global_TA_serial_mode == 1 )
            {
                Selector_TA_RX_TB_TX();
            }
            core1_usb_connect_uart = true; // 通知core1启用USB转UART功能
            serialread_infunction = 1;
            // stdio_set_driver_enabled(&stdio_usb, true);
            // stdio_set_driver_enabled(&stdio_uart, true);
            serialread();
            core1_usb_connect_uart = false; // 通知core1关闭USB转UART功能
            break;

        case 6:
            // AI 测试：core0 接管串口接收（#PIVOAIUP# 大包 / AIC 命令 / SYS,M 切走）
            ai_infunction = 1;
            Selector_AI_Free();
            aitest_main(); // 阻塞直至左右键 / 上位机 SYS,M 切走
            ai_infunction = 0;
            break;

        case 7:
            //  基本完成
            // 发送当前配置文本到上位机 供上位机解析显示在菜单配置界面
            extern bool if_need_send_menu_data;
            if_need_send_menu_data = true;
            extern void save_settings();
            save_settings(); // 刷新配置数据并标记需要发送到上位机 内部包含 if_need_send_menu_settings = 1; 标记需要发送菜单设置数据到上位机

            // custome menu
            UART_Init();                   // 启用UART探针
            core1_usb_connect_uart = true; // 通知core1启用USB转UART功能
            MenuConfigInit();              // 刷新相关配置数据
            // 在MENUCONFIGINIT中已刷新SELECTER配置
            function_to_menu = 1;
            UI_Update();
            while (true)
            {
                extern bool opnPCchangemode;
                if (opnRight || opnLeft || opnPCchangemode)
                { // 改为模式切换功能
                    break;
                }
                UI_Update();
                // ResetOpn();// 这里opn不需要在此处重置，放在function里
            }
            core1_usb_connect_uart = false; // 通知core1关闭USB转UART功能
            break;

        default:
            break;
        }
    }

    return 0;
}
