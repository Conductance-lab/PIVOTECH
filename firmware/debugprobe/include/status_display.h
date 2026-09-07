#pragma once

#include <stdbool.h>

typedef enum status_display_id {
    STATUS_DISPLAY_USB_CONNECTED,
    STATUS_DISPLAY_DAP_CONNECTED,
    STATUS_DISPLAY_DAP_RUNNING,
    STATUS_DISPLAY_UART_RX,
    STATUS_DISPLAY_UART_TX,
    STATUS_DISPLAY_COUNT
} status_display_id_t;

void status_display_start(void);
void status_display_set(status_display_id_t id, bool active);
void status_display_request_dap_status_hold(void);
void status_display_request_usb_status_hold(void);
void status_display_force_usb_disconnected(bool force);
void status_display_force_dap_disconnected(bool force);
