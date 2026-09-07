#ifndef _ADC_JOYSTICK_HPP_
#define _ADC_JOYSTICK_HPP_

#ifdef __cplusplus

#endif

/**
 * @brief 启动二维摇杆ADC采集与菜单交互界面
 * 
 * 该函数会进入主循环，实时采集摇杆X/Y通道的ADC值，
 * 并在OLED上绘制左侧64x64区域的十字线加实心圆摇杆位置图，
 * 右侧显示坐标、归零矫正、静态文本"->UART/USB"以及Config菜单项。
 * 支持上下键选择（可选Zero Calib和Config），Enter执行功能，
 * Exit退出整个摇杆程序。
 */
void adcxy_set_display_mode_code(int mode);
void adc_joystick_main(void);
int adcxy_get_display_mode_code(void);

extern volatile bool adcxy_tx_enable;
extern volatile bool adcxy_tx_pending;
extern volatile int16_t adcxy_tx_x;
extern volatile int16_t adcxy_tx_y;

#ifdef __cplusplus

#endif

#endif // _ADC_JOYSTICK_HPP_