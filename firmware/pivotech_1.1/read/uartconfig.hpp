#ifndef READ_UARTCONFIG_HPP
#define READ_UARTCONFIG_HPP

#include "hardware/uart.h"


void UARTConfigInit(void);

// UI callback
void uartconfig_fromUI(void);

// apply settings helpers


// 根据选择设置来源（host或本地）应用当前UART配置到硬件
void apply_uart_settings(void);

// 仅应用host设置（当keep_host_set开启时）
void apply_uart_settings_from_host(void);

// Shared helpers used by serialread and other read modules
void build_active_summary(char *out, size_t len);
void build_active_summary_forhc05(char *out, size_t len);

// Get currently active UART parameters after host/local selection is resolved.
void uartconfig_get_active_params(int *baudrate, uint *data_bits, uint *stop_bits, uart_parity_t *parity);
void uartconfig_get_local_params(int *baudrate, uint *data_bits, uint *stop_bits, uart_parity_t *parity);

extern int seedvalue;
extern int16_t lengthStart, heightStart, yStart, scroll_yStart, itemsYPosStart, xStart;

extern bool uartconfig_infunction;

// Expose the modal function used by serialread to check if it should open
bool uartconfig(void);



#endif // READ_UARTCONFIG_HPP