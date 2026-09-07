#ifndef _FUNCTIONS_CPP_
#define _FUNCTIONS_CPP_
#include "pico/stdlib.h"
#include "menu/ssd1306.hpp"
#include "menu/ssd1306.hpp"
#include "Fonts/fonts.h"
#include "CONFIG_FLO.hpp"
#include "menu/UI.hpp"
#include "menu/key.hpp"
#include "menu/Curve.hpp"
#include "menu/StateLED.hpp"
#include <cstring>

// 强制在此处声明，不依赖头文件条件宏
struct ReadItem_t
{
    double *param;
    double paramBackup;
    double pwmitem_step;
    char *title;
};
struct PwmOutputConfig
{
    uint8_t output_mode;
    uint8_t run_mode;
    uint16_t cfg_value;
    double cfg_step;
};
struct PWMitem_t
{
    bool open;
    ReadItem_t *duty;
    ReadItem_t *frec;
    PwmOutputConfig cfg;
};
extern PWMitem_t PWMitem[4];
extern "C" void Update_PwmOutput();

#define functionsY 18
#define functionsGap 65
#define functionnum 8 // 7->8: 新增 AI测试（插在 UART 与 自定义 之间）

// 上位机接口启用标志位
extern bool Enable_PC_Interface;

// 解析状态定义
typedef enum
{
    WAIT_HEAD1,
    WAIT_HEAD2,
    RECEIVE_DATA,
    WAIT_TAIL1,
    WAIT_TAIL2
} ParseState_e;

static ParseState_e parser_state = WAIT_HEAD1;
static uint8_t parser_buffer[128];
static uint8_t parser_count = 0;
static bool protocol_updated = false;

// 返回协议载荷
const uint8_t *GetLastProtocolPayload()
{
    if (protocol_updated)
    {
        protocol_updated = false; // 读取后清除标记
        return parser_buffer;
    }
    return NULL;
}

/**
 * @brief 当前选中的功能模块 ID (nowselect_function)
 * 代表设备当前运行的模式，用于上位机同步和状态切换。
 */
uint16_t nowselect_function = 0;

int16_t functionsXPos;

// ==================== 上位机接口：数据接收 ====================
// 传入单个 uint8 的处理函数
int ParseProtocol_Single(uint8_t ch)
{
    if (!Enable_PC_Interface)
        return 0;
    int result = 0;
    switch (parser_state)
    {
    case WAIT_HEAD1:
        if (ch == 0xAA)
            parser_state = WAIT_HEAD2;
        break;
    case WAIT_HEAD2:
        if (ch == 0x55)
        {
            parser_state = RECEIVE_DATA;
            parser_count = 0;
        }
        else
        {
            parser_state = (ch == 0xAA) ? WAIT_HEAD2 : WAIT_HEAD1;
        }
        break;
    case RECEIVE_DATA:
        parser_buffer[parser_count++] = ch;
        if (parser_count >= 128)
        {
            parser_state = WAIT_TAIL1;
        }
        break;
    case WAIT_TAIL1:
        if (ch == 0x55)
            parser_state = WAIT_TAIL2;
        else
            parser_state = WAIT_HEAD1;
        break;
    case WAIT_TAIL2:
        if (ch == 0xAA)
        {
            // 成功解析到 128 个参数
            protocol_updated = true;

            // 规定：第一个字节 (uint8_t) 必须是 nowselect_function
            uint8_t received_func = parser_buffer[0];

            if (received_func != (uint8_t)nowselect_function)
            {
                result = 1; // 如果读取结果与当前实际不同，返回 1
                nowselect_function = received_func;

                // 选择框动画方向：如果新功能 ID 大于当前功能 ID，向右移；如果小于，向左移；如果相同，不动
                if (received_func > functionnum)
                {
                    functionsXPos = 10 + 2 * (received_func - functionnum);
                }
                else if (received_func < functionnum)
                {
                    functionsXPos = -10 - 2 * (functionnum - received_func);
                }
                else
                {
                    functionsXPos = 0;
                }
            }
        }
        parser_state = WAIT_HEAD1;
        break;
    }
    return result;
}



// ============================================================

// MENUUI中 SGate的保存参数

const char *function_list[functionnum * 2] = {
    "输入状态", "DC/PWM", "模块配置", "I2C", "SPI", "UART", "AI脚本", "自定义",
    "检测", "输出", "工具", "调试", "调试", "调试", "调试", "菜单"};

void drawfuctionsmenu(int16_t x_start)
{
    for (int i = 0; i < functionnum; i++)
    {
        Paint_DrawString_EN_CenterAtX(buf, x_start + i * functionsGap, functionsY, function_list[i], &Font12, 1);
        Paint_DrawString_EN_CenterAtX(buf, x_start + i * functionsGap, functionsY + 15, function_list[functionnum + i], &Font12, 1);
    }
}
int functionstartx = SSD1306_WIDTH / 2 - 30;

void functionupdate(bool if_animate = true)
{

    int functionendx = SSD1306_WIDTH / 2 - (nowselect_function)*functionsGap+2;

    int xEnd = SSD1306_WIDTH / 2 - 32;
    int yEnd = functionsY - 3;
    int widthEnd = 64;
    int heighEND = 30;
    if (footlength != 1 && if_animate)
    {
        for (float i = 0; i < 1; i += (footlength * 0.6))
        {

            int functionx = X2line_down(functionstartx, functionendx, i);
            drawfuctionsmenu(functionx);

            int x;
            if (functionsXPos == 0)
            {
                x = easeInOutQuad(xStart, xEnd, i);
            }
            else
            {
                x = A_line(xStart, xEnd, xEnd + functionsXPos, i);
            }
            int y = easeInOutQuad(yStart, yEnd, i);
            int width = easeInOutQuad(lengthStart, widthEnd, i);
            int heigh = easeInOutQuad(heightStart, heighEND, i);
            UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);
            render(buf, &frame_area);
            memset(buf, 0, SSD1306_BUF_LEN);
        }
    }
    functionsXPos = 0;
    drawfuctionsmenu(functionendx);
    UIDrawSgate(xEnd, yEnd, widthEnd, heighEND, 8, 6, 1, 1);
    render(buf, &frame_area);
    memset(buf, 0, SSD1306_BUF_LEN);

    functionstartx = functionendx;
    lengthStart = widthEnd;
    heightStart = heighEND;
    yStart = yEnd;
    xStart = xEnd;
}
bool if_enter_function = false;
int functionmenu()
{
    Open_StateLED();
    if_enter_function = true;

    // 记录最后一次操作的时间（毫秒）
    uint32_t lastOperationTime = to_ms_since_boot(get_absolute_time());
    const uint32_t TIMEOUT_MS = 1000; // 1秒超时时间

    if (!opnRight && !opnLeft )
    { // 并非通过L R按键触发 软件触发直接渲染内容
        extern bool opnPCchangemode;
        while (opnPCchangemode)
        {
            opnPCchangemode = 0; // 标记非上位机配置模式
            functionupdate();
            sleep_ms(200); // 等待一下  
        }
        
        if_enter_function = false;
        Close_StateLED();          // 自动关闭状态LED
        return nowselect_function;
    }

    while (true)
    {
        // 获取当前时间
        uint32_t currentTime = to_ms_since_boot(get_absolute_time());

        // 检查是否超时（1秒内无左右按键操作，且不是刚进入菜单）
        if (currentTime - lastOperationTime > TIMEOUT_MS)
        {
            Close_StateLED();          // 超时自动关闭状态LED
            if_enter_function = false;
            return nowselect_function; // 超时自动退出
        }

        if (opnRight || keyRight.isPressed)
        {
            lastOperationTime = currentTime; // 重置计时器
            if (nowselect_function < (functionnum - 1))
            {
                nowselect_function++;
                functionsXPos = -10;
            }
            else
            {
                nowselect_function = 0;
                functionsXPos = 10;
            }
            extern bool send_pc_flag;
            send_pc_flag = true; // 触发向上位机更新数据

            functionupdate();
        }
        if (opnLeft || keyLeft.isPressed)
        {
            lastOperationTime = currentTime; // 重置计时器
            if (nowselect_function > 0)
            {
                nowselect_function--;
                functionsXPos = 10;
            }
            else
            {
                nowselect_function = functionnum - 1;
                functionsXPos = -10;
            }
            extern bool send_pc_flag;
            send_pc_flag = true; // 触发向上位机更新数据
            functionupdate();
        }
        ResetOpn();            // 每轮循环结束重置按键状态，等待下一次输入
        functionupdate(false); // 直接更新显示，无动画
    }
}

#endif
