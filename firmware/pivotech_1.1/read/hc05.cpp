#ifndef _HC05_CPP_
#define _HC05_CPP_

#include "pico/stdlib.h"
#include "menu/ssd1306.hpp"
#include "menu/UI.hpp"
#include "menu/key.hpp"
#include "menu/core.hpp"
#include "CONFIG_FLO.hpp"
#include "read/uartconfig.hpp"
#include "menu/StateLED.hpp"
#include "hardware/uart.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

enum Hc05WorkMode
{
    HC05_MODE_S = 0,
    HC05_MODE_MS = 1
};

enum Hc05Modal
{
    HC05_MODAL_NONE = 0,
    HC05_MODAL_NAME,
    HC05_MODAL_PASS
};

enum Hc05MsLastAction
{
    HC05_MS_LAST_NONE = 0,
    HC05_MS_LAST_S,
    HC05_MS_LAST_M
};

enum Hc05MsPhase
{
    HC05_MS_PHASE_WAIT_S = 0,
    HC05_MS_PHASE_HAVE_S_ADDR
};

enum Hc05LineMsg
{
    HC05_LINE_MSG_NONE = 0,
    HC05_LINE_MSG_SUCCESS,
    HC05_LINE_MSG_FAIL,
    HC05_LINE_MSG_PROMPT
};

bool hc05IsRunning = false;
bool hc05_infunction = false;
bool hc05_outfunction = false;
bool hc05_is_active = false;

static int nowselect_hc05_item = 0;
static Hc05WorkMode hc05_work_mode = HC05_MODE_S;
static Hc05Modal hc05_modal = HC05_MODAL_NONE;

static int hc05_name_suffix = 1;
static int hc05_temp_name_suffix = 1;
static char hc05_custom_name[32] = {0};
static bool hc05_use_custom_name = false;

static constexpr int HC05_PASS_OPTION_COUNT = 7;
static constexpr int HC05_PASS_RANDOM4_INDEX = 3;
static constexpr int HC05_PASS_RANDOM6_INDEX = 6;

static int hc05_pass_selection = 0;
static int hc05_temp_pass_selection = 0;
static char hc05_random_pass4[5] = "1234";
static char hc05_random_pass6[7] = "123456";
static char hc05_custom_pass[16] = {0};
static bool hc05_use_custom_pass = false;

static int hc05_protocol_mode = 0;
static int hc05_protocol_action = 0;
static bool hc05_protocol_pending = false;

static bool hc05_ms_s_verified = false;
static bool hc05_ms_m_verified = false;
static uint16_t hc05_s_addr[3] = {0x0000, 0x0000, 0x0000};
static bool hc05_ms_random_generated = false;

static bool hc05_has_atmode_dev = false;
static uint64_t hc05_last_send_us = 0;
static bool hc05_rand_seeded = false;
static uint64_t hc05_last_probe_send_us = 0;
static bool hc05_last_cfg_ok = false;
static bool hc05_send_task_busy = false;
static uint64_t hc05_last_ok_us = 0;
static Hc05MsLastAction hc05_ms_last_action = HC05_MS_LAST_NONE;
static Hc05MsPhase hc05_ms_phase = HC05_MS_PHASE_WAIT_S;

static int hc05_YPos_Start = 0;
static int hc05_YPos_End = 0;

static constexpr uint64_t hc05_cfg_message_hold_us = 2000000ULL;

static Hc05LineMsg hc05_ms_line3_msg = HC05_LINE_MSG_NONE;
static uint64_t hc05_ms_line3_msg_us = 0;
static Hc05LineMsg hc05_ms_line4_msg = HC05_LINE_MSG_NONE;
static uint64_t hc05_ms_line4_msg_us = 0;
static bool hc05_ms_reset_pending = false;
static bool hc05_ms_need_master_swap = false;
static bool hc05_ms_seen_no_at_after_s = false;

static bool hc05_s_mode_result_ok = false;
static uint64_t hc05_s_mode_result_ok_us = 0;
static bool hc05_s_mode_result_fail = false;
static uint64_t hc05_s_mode_result_fail_us = 0;
static bool hc05_ms_s_result_ok = false;
static uint64_t hc05_ms_s_result_ok_us = 0;
static bool hc05_ms_s_result_fail = false;
static uint64_t hc05_ms_s_result_fail_us = 0;
static bool hc05_ms_m_result_ok = false;
static uint64_t hc05_ms_m_result_ok_us = 0;
static bool hc05_ms_m_result_fail = false;
static uint64_t hc05_ms_m_result_fail_us = 0;

static char hc05_addr_part1[5] = "0000";
static char hc05_addr_part2[3] = "00";
static char hc05_addr_part3[7] = "000000";

static const char *hc05_get_pass_text();
static const char *hc05_get_name_text();

static void hc05_refresh_result_flags_timeout(uint64_t now_us)
{
    bool changed = false;

    if (hc05_s_mode_result_ok && (now_us - hc05_s_mode_result_ok_us) >= hc05_cfg_message_hold_us)
    {
        hc05_s_mode_result_ok = false;
        changed = true;
    }
    if (hc05_s_mode_result_fail && (now_us - hc05_s_mode_result_fail_us) >= hc05_cfg_message_hold_us)
    {
        hc05_s_mode_result_fail = false;
        changed = true;
    }
    if (hc05_ms_s_result_ok && (now_us - hc05_ms_s_result_ok_us) >= hc05_cfg_message_hold_us)
    {
        hc05_ms_s_result_ok = false;
        changed = true;
    }
    if (hc05_ms_s_result_fail && (now_us - hc05_ms_s_result_fail_us) >= hc05_cfg_message_hold_us)
    {
        hc05_ms_s_result_fail = false;
        changed = true;
    }
    if (hc05_ms_m_result_ok && (now_us - hc05_ms_m_result_ok_us) >= hc05_cfg_message_hold_us)
    {
        hc05_ms_m_result_ok = false;
        changed = true;
    }
    if (hc05_ms_m_result_fail && (now_us - hc05_ms_m_result_fail_us) >= hc05_cfg_message_hold_us)
    {
        hc05_ms_m_result_fail = false;
        changed = true;
    }

    if (hc05_ms_line3_msg != HC05_LINE_MSG_NONE && (now_us - hc05_ms_line3_msg_us) >= hc05_cfg_message_hold_us)
    {
        hc05_ms_line3_msg = HC05_LINE_MSG_NONE;
        changed = true;
    }
    if (hc05_ms_line4_msg != HC05_LINE_MSG_NONE && (now_us - hc05_ms_line4_msg_us) >= hc05_cfg_message_hold_us)
    {
        hc05_ms_line4_msg = HC05_LINE_MSG_NONE;
        changed = true;
    }

    if (hc05_ms_reset_pending && (now_us - hc05_ms_line4_msg_us) >= hc05_cfg_message_hold_us)
    {
        hc05_ms_reset_pending = false;
        hc05_ms_phase = HC05_MS_PHASE_WAIT_S;
        hc05_ms_s_verified = false;
        hc05_ms_m_verified = false;
        hc05_ms_need_master_swap = false;
        hc05_ms_seen_no_at_after_s = false;
        hc05_ms_last_action = HC05_MS_LAST_NONE;
        strncpy(hc05_addr_part1, "0000", sizeof(hc05_addr_part1));
        strncpy(hc05_addr_part2, "00", sizeof(hc05_addr_part2));
        strncpy(hc05_addr_part3, "000000", sizeof(hc05_addr_part3));
        changed = true;
    }

    if (changed)
    {
        extern bool send_pc_flag;
        send_pc_flag = true; // 状态在2秒超时后恢复，通知上位机刷新
    }
}

static void hc05_notify_success()
{
    StartStateLED();
    extern bool send_pc_flag;
    send_pc_flag = true; // 触发向上位机更新数据
}

static void hc05_mark_result_ok(bool &ok_flag, uint64_t &ok_us, bool &fail_flag)
{
    ok_flag = true;
    ok_us = time_us_64();
    fail_flag = false;
}

static void hc05_mark_result_fail(bool &fail_flag, uint64_t &fail_us, bool &ok_flag)
{
    fail_flag = true;
    fail_us = time_us_64();
    ok_flag = false;
}

static const char *hc05_get_name_text()
{
    if (hc05_use_custom_name && hc05_custom_name[0] != '\0')
    {
        return hc05_custom_name;
    }

    static char default_name[16];
    snprintf(default_name, sizeof(default_name), "PIVO%02d", hc05_name_suffix);
    return default_name;
}

static void hc05_uart_enter_at_mode_config()
{
    gpio_init(UART_TX_PIN);
    gpio_init(UART_RX_PIN);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    gpio_pull_up(UART_TX_PIN);
    gpio_pull_up(UART_RX_PIN);

    uart_init(UART_ID, 38400);
    uart_set_fifo_enabled(UART_ID, true);
    uart_set_baudrate(UART_ID, 38400);
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
}

static void hc05_uart_flush_rx()
{
    while (uart_is_readable(UART_ID))
    {
        (void)uart_getc(UART_ID);
    }
}

static bool hc05_wait_ok(uint32_t timeout_ms)
{
    uint64_t start_us = time_us_64();
    int ok_state = 0;

    while ((time_us_64() - start_us) < ((uint64_t)timeout_ms * 1000ULL))
    {
        while (uart_is_readable(UART_ID))
        {
            char c = (char)uart_getc(UART_ID);
            if (ok_state == 0)
            {
                ok_state = (c == 'O') ? 1 : 0;
            }
            else
            {
                if (c == 'K')
                {
                    hc05_last_ok_us = time_us_64();
                    hc05_has_atmode_dev = true;
                    hc05_notify_success();
                    return true;
                }
                ok_state = (c == 'O') ? 1 : 0;
            }
        }
        sleep_ms(2);
    }
    return false;
}

static bool hc05_send_cmd_wait_ok(const char *cmd, uint32_t timeout_ms)
{
    hc05_uart_flush_rx();
    uart_puts(UART_ID, cmd);
    return hc05_wait_ok(timeout_ms);
}

static bool hc05_send_cmd_wait_ok_retry(const char *cmd, uint32_t timeout_ms, int retries, uint32_t retry_gap_ms)
{
    if (retries < 1)
        retries = 1;

    for (int i = 0; i < retries; ++i)
    {
        if (hc05_send_cmd_wait_ok(cmd, timeout_ms))
        {
            return true;
        }
        if (i + 1 < retries)
        {
            sleep_ms(retry_gap_ms);
        }
    }

    return false;
}

static bool hc05_send_cmd_wait_ok_capture(const char *cmd, char *resp, size_t resp_len, uint32_t timeout_ms)
{
    if (!resp || resp_len == 0)
    {
        return false;
    }

    resp[0] = '\0';
    size_t idx = 0;
    int ok_state = 0;
    uint64_t start_us = time_us_64();

    hc05_uart_flush_rx();
    uart_puts(UART_ID, cmd);

    while ((time_us_64() - start_us) < ((uint64_t)timeout_ms * 1000ULL))
    {
        while (uart_is_readable(UART_ID))
        {
            char c = (char)uart_getc(UART_ID);

            if (idx + 1 < resp_len)
            {
                resp[idx++] = c;
                resp[idx] = '\0';
            }

            if (ok_state == 0)
            {
                ok_state = (c == 'O') ? 1 : 0;
            }
            else
            {
                if (c == 'K')
                {
                    hc05_last_ok_us = time_us_64();
                    hc05_has_atmode_dev = true;
                    hc05_notify_success();
                    return true;
                }
                ok_state = (c == 'O') ? 1 : 0;
            }
        }
        sleep_ms(2);
    }

    return false;
}

static bool hc05_send_cmd_wait_ok_capture_retry(const char *cmd, char *resp, size_t resp_len,
                                                uint32_t timeout_ms, int retries, uint32_t retry_gap_ms)
{
    if (retries < 1)
        retries = 1;

    for (int i = 0; i < retries; ++i)
    {
        if (hc05_send_cmd_wait_ok_capture(cmd, resp, resp_len, timeout_ms))
        {
            return true;
        }
        if (i + 1 < retries)
        {
            sleep_ms(retry_gap_ms);
        }
    }

    return false;
}

static bool hc05_ensure_at_ready(uint32_t budget_ms)
{
    uint64_t start_us = time_us_64();

    // 配置链开始前给模块一个静默窗口，避免和刚刚的探测AT应答相互干扰。
    sleep_ms(30);
    hc05_uart_flush_rx();

    while ((time_us_64() - start_us) < ((uint64_t)budget_ms * 1000ULL))
    {
        if (hc05_send_cmd_wait_ok_retry("AT\r\n", 220, 1, 0))
        {
            return true;
        }
        sleep_ms(20);
    }
    return false;
}

static bool hc05_send_step(const char *cmd, uint32_t timeout_ms)
{
    bool ok = hc05_send_cmd_wait_ok_retry(cmd, timeout_ms, 2, 20);
    // HC-05在部分固件上连续写命令过快会丢首包，增加小间隔提升首次成功率。
    sleep_ms(30);
    return ok;
}

static bool hc05_parse_addr_response(const char *resp)
{
    if (!resp)
    {
        return false;
    }

    const char *p = strstr(resp, "+ADDR:");
    if (!p)
    {
        return false;
    }
    p += 6;

    char part1[5] = {0};
    char part2[3] = {0};
    char part3[7] = {0};

    if (sscanf(p, "%4[0-9A-Fa-f]:%2[0-9A-Fa-f]:%6[0-9A-Fa-f]", part1, part2, part3) == 3)
    {
        strncpy(hc05_addr_part1, part1, sizeof(hc05_addr_part1) - 1);
        strncpy(hc05_addr_part2, part2, sizeof(hc05_addr_part2) - 1);
        strncpy(hc05_addr_part3, part3, sizeof(hc05_addr_part3) - 1);
        hc05_addr_part1[sizeof(hc05_addr_part1) - 1] = '\0';
        hc05_addr_part2[sizeof(hc05_addr_part2) - 1] = '\0';
        hc05_addr_part3[sizeof(hc05_addr_part3) - 1] = '\0';
        return true;
    }

    return false;
}

static void hc05_probe_at_mode_task()
{
    if (hc05_send_task_busy)
    {
        return;
    }

    uint64_t now_us = time_us_64();
    if ((now_us - hc05_last_probe_send_us) >= 50000)
    {
        uart_puts(UART_ID, "AT\r\n");
        hc05_last_probe_send_us = now_us;
    }

    int ok_state = 0;
    while (uart_is_readable(UART_ID))
    {
        char c = (char)uart_getc(UART_ID);
        if (ok_state == 0)
        {
            ok_state = (c == 'O') ? 1 : 0;
        }
        else
        {
            if (c == 'K')
            {
                if (hc05_has_atmode_dev == false)
                {
                    hc05_notify_success();
                }
                hc05_has_atmode_dev = true;
                hc05_last_ok_us = now_us;

                return;
            }
            ok_state = (c == 'O') ? 1 : 0;
        }
    }

    // 在线心跳超时则判定离线，底部提示自动回到 no ATmode dev!
    if (hc05_has_atmode_dev && hc05_last_ok_us != 0 && (now_us - hc05_last_ok_us) > 200000)
    {
        hc05_has_atmode_dev = false;
        extern bool send_pc_flag;
        send_pc_flag = true; // 触发向上位机更新数据
        // hc05_last_cfg_ok = false;
        // hc05_ms_s_verified = false;
        // hc05_ms_m_verified = false;
    }
}

static int hc05_uart_parity_to_at(uart_parity_t parity)
{
    if (parity == UART_PARITY_ODD)
        return 1;
    if (parity == UART_PARITY_EVEN)
        return 2;
    return 0;
}

static bool hc05_apply_s_mode_config()
{
    hc05_send_task_busy = true;
    hc05_last_cfg_ok = false;

    if (!hc05_ensure_at_ready(650))
    {
        hc05_send_task_busy = false;
        hc05_has_atmode_dev = false;
        return false;
    }

    char name_cmd[32];
    char pswd_cmd[32];
    char role_cmd[24];
    char uart_cmd[40];

    int active_baud = 9600;
    uint active_data_bits = 8;
    uint active_stop_bits = 1;
    uart_parity_t active_parity = UART_PARITY_NONE;

    uartconfig_get_active_params(&active_baud, &active_data_bits, &active_stop_bits, &active_parity);

    // HC-05 AT+UART: stop parameter 0=1bit, 1=2bits.
    int at_stop = (active_stop_bits >= 2) ? 1 : 0;
    int at_parity = hc05_uart_parity_to_at(active_parity);

    snprintf(name_cmd, sizeof(name_cmd), "AT+NAME=%s\r\n", hc05_get_name_text());
    snprintf(pswd_cmd, sizeof(pswd_cmd), "AT+PSWD=\"%s\"\r\n", hc05_get_pass_text());
    snprintf(role_cmd, sizeof(role_cmd), "AT+ROLE=%d\r\n", 0);
    snprintf(uart_cmd, sizeof(uart_cmd), "AT+UART=%d,%d,%d\r\n", active_baud, at_stop, at_parity);

    bool ok = true;
    ok = ok && hc05_send_step("AT+ORGL\r\n", 900);
    ok = ok && hc05_send_step(name_cmd, 650);
    ok = ok && hc05_send_step(pswd_cmd, 650);
    ok = ok && hc05_send_step(role_cmd, 650);
    ok = ok && hc05_send_step("AT+CMODE=1\r\n", 650);
    ok = ok && hc05_send_step(uart_cmd, 700);

    hc05_send_task_busy = false;
    if (ok)
    {
        hc05_notify_success();
    }
    return ok;
}

static void hc05_seed_random_once()
{
    if (!hc05_rand_seeded)
    {
        srand((unsigned int)time_us_32());
        hc05_rand_seeded = true;
    }
}

static const char *hc05_get_pass_option_label(int selection)
{
    switch (selection)
    {
    case 0:
        return "0000";
    case 1:
        return "1234";
    case 2:
        return "1111";
    case 3:
        return "随机4位";
    case 4:
        return "000000";
    case 5:
        return "123456";
    case 6:
        return "随机6位";
    default:
        return "0000";
    }
}

static char *hc05_get_random_pass_buffer(int selection)
{
    return (selection == HC05_PASS_RANDOM6_INDEX) ? hc05_random_pass6 : hc05_random_pass4;
}

static size_t hc05_get_random_pass_length(int selection)
{
    return (selection == HC05_PASS_RANDOM6_INDEX) ? 6 : 4;
}

static void hc05_generate_random_pass(int selection)
{
    hc05_seed_random_once();
    char *target = hc05_get_random_pass_buffer(selection);
    size_t length = hc05_get_random_pass_length(selection);

    for (size_t i = 0; i < length; ++i)
    {
        target[i] = (char)('0' + (rand() % 10));
    }
    target[length] = '\0';
}

static const char *hc05_get_pass_text()
{
    if (hc05_use_custom_pass && hc05_custom_pass[0] != '\0')
    {
        return hc05_custom_pass;
    }

    switch (hc05_pass_selection)
    {
    case HC05_PASS_RANDOM4_INDEX:
        return hc05_random_pass4;
    case HC05_PASS_RANDOM6_INDEX:
        return hc05_random_pass6;
    case 0:
    case 1:
    case 2:
    case 4:
    case 5:
        return hc05_get_pass_option_label(hc05_pass_selection);
    default:
        return hc05_get_pass_option_label(0);
    }
}

static void hc05_refresh_dev_state()
{
    hc05_probe_at_mode_task();
}

static bool hc05_item_selectable(int item)
{
    if (item < 0 || item > 4)
    {
        return false;
    }

    if (hc05_work_mode == HC05_MODE_S)
    {
        // S 模式：0模式切换 1名称 2密码 3UART 4AT发送
        return true;
    }

    // S&M 模式：0模式切换 1UART 2S配置 3M配置
    if (item == 4)
    {
        return false;
    }
    if (item == 3 && hc05_ms_phase != HC05_MS_PHASE_HAVE_S_ADDR)
    {
        // 第四行在 S 阶段完成前不可选
        return false;
    }
    return true;
}

static void hc05_move_selection(int dir)
{
    int probe = nowselect_hc05_item;
    for (int i = 0; i < 5; ++i)
    {
        probe += dir;
        if (probe < 0)
            probe = 4;
        if (probe > 4)
            probe = 0;
        if (hc05_item_selectable(probe))
        {
            nowselect_hc05_item = probe;
            return;
        }
    }
}

static void draw_hc05_name_selector()
{
    int h = 30;
    int w = 88;
    int x = (SCREEN_WIDTH - w) / 2;
    int y = (SCREEN_HEIGHT - h) / 2;

    UIDisplayStr_font12(x + 4, y + 4, (char *)"名称后缀:", 24, 0);

    char suffix_text[12];
    snprintf(suffix_text, sizeof(suffix_text), "%02d", hc05_temp_name_suffix);
    UIDisplayStr_font12(x + 34, y + 18, suffix_text, 8, 0);
    InvertRect(buf, x + 30, y + 17, 28, 12);
}

static void draw_hc05_pass_selector()
{
    int h = 30;
    int w = 96;
    int x = (SCREEN_WIDTH - w) / 2;
    int y = (SCREEN_HEIGHT - h) / 2;

    UIDisplayStr_font12(x + 4, y + 4, (char *)"密码:", 8, 0);

    char pass_text[20];
    snprintf(pass_text, sizeof(pass_text), "%s", hc05_get_pass_option_label(hc05_temp_pass_selection));

    UIDisplayStr_font12(x + 4, y + 18, pass_text, 20, 0);
    InvertRect(buf, x + 2, y + 17, 70, 12);
}

static void hc05_fill_random_addr()
{
    hc05_seed_random_once();
    hc05_s_addr[0] = (uint16_t)(rand() & 0xFFFF);
    hc05_s_addr[1] = (uint16_t)(rand() & 0xFFFF);
    hc05_s_addr[2] = (uint16_t)(rand() & 0xFFFF);
}

static void hc05_draw_checkbox(int x, int y, bool checked)
{
    DrawRectangle(buf, x, y, 9, 9, 0, 1);
    if (checked)
    {
        DrawRectangle(buf, x + 2, y + 2, 5, 5, 1, 1);
    }
}

static void hc05_enter_ms_mode()
{
    hc05_work_mode = HC05_MODE_MS;
    hc05_ms_s_verified = false;
    hc05_ms_m_verified = false;
    hc05_ms_phase = HC05_MS_PHASE_WAIT_S;
    hc05_ms_line3_msg = HC05_LINE_MSG_NONE;
    hc05_ms_line4_msg = HC05_LINE_MSG_NONE;
    hc05_ms_reset_pending = false;
    hc05_ms_need_master_swap = false;
    hc05_ms_seen_no_at_after_s = false;
    hc05_ms_random_generated = true;

    if (!hc05_item_selectable(nowselect_hc05_item))
    {
        nowselect_hc05_item = 0;
    }
}

static void hc05_enter_s_mode()
{
    hc05_work_mode = HC05_MODE_S;
    hc05_ms_s_verified = false;
    hc05_ms_m_verified = false;
    hc05_ms_phase = HC05_MS_PHASE_WAIT_S;
    hc05_ms_line3_msg = HC05_LINE_MSG_NONE;
    hc05_ms_line4_msg = HC05_LINE_MSG_NONE;
    hc05_ms_reset_pending = false;
    hc05_ms_need_master_swap = false;
    hc05_ms_seen_no_at_after_s = false;
    hc05_ms_random_generated = false;
}

static void draw_hc05_main(int Ypos)
{
    uint64_t now_us = time_us_64();
    hc05_refresh_result_flags_timeout(now_us);

    DrawRectangle(buf, 0, Ypos, 128, 64, 1, 0);

    Paint_DrawString_EN(buf, 5, Ypos + 3, (char *)"HC-05 AT模式", &Font12, 1);
    Paint_DrawString_EN_CenterAtX(buf, 115, Ypos + 3,
                                  (char *)(hc05_work_mode == HC05_MODE_S ? "S" : "S&M"), &Font12, 1);

    if (hc05_work_mode == HC05_MODE_S)
    {
        char name_text[16];
        snprintf(name_text, sizeof(name_text), "N:%s", hc05_get_name_text());
        Paint_DrawString_EN_CenterAtX(buf, 32, Ypos + 19, name_text, &Font12, 1);

        char pass_text[16];
        snprintf(pass_text, sizeof(pass_text), "P:%s", hc05_get_pass_text());
        Paint_DrawString_EN_CenterAtX(buf, 96, Ypos + 19, pass_text, &Font12, 1);

        char uart_str[24];
        build_active_summary_forhc05(uart_str, sizeof(uart_str));
        Paint_DrawString_EN_CenterAtX(buf, 64, Ypos + 35, uart_str, &Font12, 1);

        const char *line4_text = hc05_has_atmode_dev ? "开始配置从机参数" : "未检测到AT设备";
        if (hc05_has_atmode_dev && (now_us - hc05_last_send_us) < hc05_cfg_message_hold_us)
        {
            line4_text = hc05_last_cfg_ok ? "从机参数配置成功" : "从机参数配置失败";
        }
        Paint_DrawString_EN_CenterAtX(buf, 64, Ypos + 51, (char *)line4_text, &Font12, 1);
    }
    else
    {
        // S&M 模式：第二行显示 UART，第三/四行显示任务进度
        char uart_str[24];
        build_active_summary_forhc05(uart_str, sizeof(uart_str));
        Paint_DrawString_EN_CenterAtX(buf, 64, Ypos + 19, uart_str, &Font12, 1);

        // 第三行复选框
        hc05_draw_checkbox(6, Ypos + 35, hc05_ms_s_verified);
        if (hc05_ms_line3_msg == HC05_LINE_MSG_SUCCESS)
        {
            Paint_DrawString_EN(buf, 18, Ypos + 35, (char *)"从机配置成功", &Font12, 1);
        }
        else if (hc05_ms_line3_msg == HC05_LINE_MSG_FAIL)
        {
            Paint_DrawString_EN(buf, 18, Ypos + 35, (char *)"从机配置失败", &Font12, 1);
        }
        else if (hc05_ms_phase == HC05_MS_PHASE_HAVE_S_ADDR)
        {
            char saddr_text[32];
            Paint_DrawString_EN(buf, 18, Ypos + 35, (char *)"地址:", &Font12, 1);
            snprintf(saddr_text, sizeof(saddr_text), "%s:%s:%s", hc05_addr_part1, hc05_addr_part2, hc05_addr_part3);
            Paint_DrawString_EN(buf, 18 + 42, Ypos + 37, saddr_text, &Font8, 1);
        }
        else
        {
            Paint_DrawString_EN(buf, 18, Ypos + 35,
                                (char *)(hc05_has_atmode_dev ? "开始配置从机参数" : "未检测到AT设备"),
                                &Font12, 1);
        }

        // 第四行复选框
        hc05_draw_checkbox(6, Ypos + 51, hc05_ms_m_verified);
        if (hc05_ms_line4_msg == HC05_LINE_MSG_SUCCESS)
        {
            Paint_DrawString_EN(buf, 18, Ypos + 51, (char *)"参数配置绑定成功", &Font12, 1);
        }
        else if (hc05_ms_line4_msg == HC05_LINE_MSG_FAIL)
        {
            Paint_DrawString_EN(buf, 18, Ypos + 51, (char *)"参数配置绑定失败", &Font12, 1);
        }
        else if (hc05_ms_line4_msg == HC05_LINE_MSG_PROMPT)
        {
            Paint_DrawString_EN(buf, 18, Ypos + 51, (char *)"请拔出从机设备", &Font12, 1);
        }
        else if (hc05_ms_phase != HC05_MS_PHASE_HAVE_S_ADDR)
        {
            Paint_DrawString_EN(buf, 18, Ypos + 51, (char *)"等待配置从机参数", &Font12, 1);
        }
        else if (hc05_ms_need_master_swap)
        {
            if (!hc05_has_atmode_dev)
            {
                Paint_DrawString_EN(buf, 18, Ypos + 51, (char *)"请插入主机设备", &Font12, 1);
            }
            else
            {
                Paint_DrawString_EN(buf, 18, Ypos + 51, (char *)"请拔出从机设备", &Font12, 1);
            }
        }
        else if (!hc05_has_atmode_dev)
        {
            Paint_DrawString_EN(buf, 18, Ypos + 51, (char *)"未检测到AT设备", &Font12, 1);
        }
        else
        {
            Paint_DrawString_EN(buf, 18, Ypos + 51, (char *)"开始配置主机参数", &Font12, 1);
        }
    }

    DrawLine(buf, 0, Ypos + 15, 127, Ypos + 15, 1);
    DrawLine(buf, 0, Ypos + 31, 127, Ypos + 31, 1);
    DrawLine(buf, 0, Ypos + 47, 127, Ypos + 47, 1);
}

static void hc05_line4_action()
{
    if (!hc05_has_atmode_dev)
    {
        hc05_last_cfg_ok = false;
        hc05_last_send_us = time_us_64();
        hc05_mark_result_fail(hc05_s_mode_result_fail, hc05_s_mode_result_fail_us, hc05_s_mode_result_ok);
        return;
    }

    hc05_last_cfg_ok = hc05_apply_s_mode_config();
    hc05_last_send_us = time_us_64();
    if (hc05_last_cfg_ok)
    {
        hc05_mark_result_ok(hc05_s_mode_result_ok, hc05_s_mode_result_ok_us, hc05_s_mode_result_fail);
    }
    else
    {
        hc05_mark_result_fail(hc05_s_mode_result_fail, hc05_s_mode_result_fail_us, hc05_s_mode_result_ok);
    }
}

static void hc05_ms_line3_action()
{
    hc05_send_task_busy = true;
    hc05_last_cfg_ok = false;
    hc05_ms_last_action = HC05_MS_LAST_S;
    hc05_ms_line3_msg = HC05_LINE_MSG_NONE;

    if (!hc05_ensure_at_ready(650))
    {
        hc05_send_task_busy = false;
        hc05_has_atmode_dev = false;
        hc05_ms_s_verified = false;
        hc05_last_send_us = time_us_64();
        hc05_ms_line3_msg = HC05_LINE_MSG_FAIL;
        hc05_ms_line3_msg_us = hc05_last_send_us;
        hc05_mark_result_fail(hc05_ms_s_result_fail, hc05_ms_s_result_fail_us, hc05_ms_s_result_ok);
        return;
    }

    int active_baud = 9600;
    uint active_data_bits = 8;
    uint active_stop_bits = 1;
    uart_parity_t active_parity = UART_PARITY_NONE;
    uartconfig_get_active_params(&active_baud, &active_data_bits, &active_stop_bits, &active_parity);
    int at_stop = (active_stop_bits >= 2) ? 1 : 0;
    int at_parity = hc05_uart_parity_to_at(active_parity);

    char uart_cmd[40];
    char addr_resp[80];
    snprintf(uart_cmd, sizeof(uart_cmd), "AT+UART=%d,%d,%d\r\n", active_baud, at_stop, at_parity);

    bool ok = true;
    ok = ok && hc05_send_step("AT+ORGL\r\n", 900);
    {
        char name_cmd[48];
        snprintf(name_cmd, sizeof(name_cmd), "AT+NAME=%s-Slaver\r\n", hc05_get_name_text());
        ok = ok && hc05_send_step(name_cmd, 650);
    }
    {
        char pass_cmd[32];
        snprintf(pass_cmd, sizeof(pass_cmd), "AT+PSWD=\"%s\"\r\n", hc05_get_pass_text());
        ok = ok && hc05_send_step(pass_cmd, 650);
    }
    ok = ok && hc05_send_step("AT+ROLE=0\r\n", 650);

    if (ok)
    {
        ok = hc05_send_cmd_wait_ok_capture_retry("AT+ADDR?\r\n", addr_resp, sizeof(addr_resp), 700, 2, 20);
    }
    if (ok)
    {
        ok = hc05_parse_addr_response(addr_resp);
    }

    ok = ok && hc05_send_step(uart_cmd, 700);

    hc05_ms_s_verified = ok;
    hc05_last_cfg_ok = ok;
    hc05_last_send_us = time_us_64();
    hc05_ms_line3_msg = ok ? HC05_LINE_MSG_SUCCESS : HC05_LINE_MSG_FAIL;
    hc05_ms_line3_msg_us = hc05_last_send_us;
    hc05_send_task_busy = false;
    if (ok)
    {
        hc05_notify_success();
        hc05_ms_phase = HC05_MS_PHASE_HAVE_S_ADDR;
        hc05_ms_need_master_swap = true;
        hc05_ms_seen_no_at_after_s = false;
        hc05_mark_result_ok(hc05_ms_s_result_ok, hc05_ms_s_result_ok_us, hc05_ms_s_result_fail);
        hc05_ms_line4_msg = HC05_LINE_MSG_PROMPT;
        hc05_ms_line4_msg_us = hc05_last_send_us;
    }
    else
    {
        hc05_mark_result_fail(hc05_ms_s_result_fail, hc05_ms_s_result_fail_us, hc05_ms_s_result_ok);
    }

    if (ok)
    {
        // S 完成后将焦点移动到第四行
        nowselect_hc05_item = 3;
    }
}

static void hc05_ms_line4_action()
{
    if (hc05_ms_phase != HC05_MS_PHASE_HAVE_S_ADDR)
    {
        return;
    }

    if (hc05_ms_need_master_swap)
    {
        return;
    }

    if (!hc05_has_atmode_dev)
    {
        hc05_ms_line4_msg = HC05_LINE_MSG_FAIL;
        hc05_ms_line4_msg_us = time_us_64();
        hc05_mark_result_fail(hc05_ms_m_result_fail, hc05_ms_m_result_fail_us, hc05_ms_m_result_ok);
        return;
    }

    hc05_send_task_busy = true;
    hc05_last_cfg_ok = false;
    hc05_ms_last_action = HC05_MS_LAST_M;
    hc05_ms_line4_msg = HC05_LINE_MSG_NONE;

    if (!hc05_ensure_at_ready(650))
    {
        hc05_send_task_busy = false;
        hc05_has_atmode_dev = false;
        hc05_ms_m_verified = false;
        hc05_last_send_us = time_us_64();
        hc05_ms_line4_msg = HC05_LINE_MSG_FAIL;
        hc05_ms_line4_msg_us = hc05_last_send_us;
        hc05_mark_result_fail(hc05_ms_m_result_fail, hc05_ms_m_result_fail_us, hc05_ms_m_result_ok);
        return;
    }

    int active_baud = 9600;
    uint active_data_bits = 8;
    uint active_stop_bits = 1;
    uart_parity_t active_parity = UART_PARITY_NONE;
    uartconfig_get_active_params(&active_baud, &active_data_bits, &active_stop_bits, &active_parity);
    int at_stop = (active_stop_bits >= 2) ? 1 : 0;
    int at_parity = hc05_uart_parity_to_at(active_parity);

    char uart_cmd[40];
    char bind_cmd[48];
    snprintf(uart_cmd, sizeof(uart_cmd), "AT+UART=%d,%d,%d\r\n", active_baud, at_stop, at_parity);
    snprintf(bind_cmd, sizeof(bind_cmd), "AT+BIND=%s,%s,%s\r\n", hc05_addr_part1, hc05_addr_part2, hc05_addr_part3);

    bool ok = true;
    ok = ok && hc05_send_step("AT+ORGL\r\n", 900);
    char name_cmd[48];
    snprintf(name_cmd, sizeof(name_cmd), "AT+NAME=%s-Master\r\n", hc05_get_name_text());
    ok = ok && hc05_send_step(name_cmd, 650);
    char pass_cmd[32];
    snprintf(pass_cmd, sizeof(pass_cmd), "AT+PSWD=\"%s\"\r\n", hc05_get_pass_text());
    ok = ok && hc05_send_step(pass_cmd, 650);
    ok = ok && hc05_send_step("AT+ROLE=1\r\n", 650);
    ok = ok && hc05_send_step("AT+CMODE=0\r\n", 650);
    ok = ok && hc05_send_step(bind_cmd, 700);
    ok = ok && hc05_send_step(uart_cmd, 700);

    hc05_ms_m_verified = ok;
    hc05_last_cfg_ok = ok;
    hc05_last_send_us = time_us_64();
    hc05_ms_line4_msg = ok ? HC05_LINE_MSG_SUCCESS : HC05_LINE_MSG_FAIL;
    hc05_ms_line4_msg_us = hc05_last_send_us;
    if (ok)
    {
        hc05_ms_reset_pending = true;
    }
    hc05_send_task_busy = false;
    if (ok)
    {
        hc05_notify_success();
        hc05_mark_result_ok(hc05_ms_m_result_ok, hc05_ms_m_result_ok_us, hc05_ms_m_result_fail);
    }
    else
    {
        hc05_mark_result_fail(hc05_ms_m_result_fail, hc05_ms_m_result_fail_us, hc05_ms_m_result_ok);
    }
}

static void anni_hc05()
{
    int xEnd, yEnd, widthEnd, heighEnd;

    if (!hc05IsRunning)
    {
        if (hc05_work_mode == HC05_MODE_S)
        {
            switch (nowselect_hc05_item)
            {
            case 0:
                xEnd = 115 - 12;
                yEnd = 1;
                widthEnd = 24;
                heighEnd = 12;
                break;
            case 1:
                xEnd = 1;
                yEnd = 17;
                widthEnd = 62;
                heighEnd = 12;
                break;
            case 2:
                xEnd = 65;
                yEnd = 17;
                widthEnd = 62;
                heighEnd = 12;
                break;
            case 3:
                xEnd = 0;
                yEnd = 33;
                widthEnd = 127;
                heighEnd = 12;
                break;
            case 4:
            default:
                xEnd = 0;
                yEnd = 49;
                widthEnd = 127;
                heighEnd = 12;
                break;
            }
        }
        else
        {
            switch (nowselect_hc05_item)
            {
            case 0:
                xEnd = 115 - 12;
                yEnd = 1;
                widthEnd = 24;
                heighEnd = 12;
                break;
            case 1:
                xEnd = 0;
                yEnd = 17;
                widthEnd = 127;
                heighEnd = 12;
                break;
            case 2:
                xEnd = 0;
                yEnd = 33;
                widthEnd = 127;
                heighEnd = 12;
                break;
            case 3:
            default:
                xEnd = 0;
                yEnd = 49;
                widthEnd = 127;
                heighEnd = 12;
                break;
            }
        }
        hc05_YPos_End = 0;
    }
    else
    {
        switch (hc05_modal)
        {
        case HC05_MODAL_NAME:
            heighEnd = 30;
            widthEnd = 88;
            break;
        case HC05_MODAL_PASS:
            heighEnd = 30;
            widthEnd = 96;
            break;
        case HC05_MODAL_NONE:
        default:
            heighEnd = 30;
            widthEnd = 84;
            break;
        }
        xEnd = (SCREEN_WIDTH - widthEnd) / 2;
        yEnd = (SCREEN_HEIGHT - heighEnd) / 2;
        hc05_YPos_End = 0;
    }

    if (hc05_infunction)
    {
        seedvalue = to_ms_since_boot(get_absolute_time());
    }

    bool if_no_animation = (xEnd == xStart && yEnd == yStart && widthEnd == lengthStart && heighEnd == heightStart);

    if (!if_no_animation && footlength != 1 && !(hc05IsRunning && !hc05_infunction))
    {
        for (float i = 0; i <= 1; i += footlength)
        {
            int x = easeInOutQuad(xStart, xEnd, i);
            int y = easeInOutQuad(yStart, yEnd, i);
            int width = easeInOutQuad(lengthStart, widthEnd, i);
            int heigh = easeInOutQuad(heightStart, heighEnd, i);
            int yPos = X2line_down(hc05_YPos_Start, hc05_YPos_End, i);

            draw_hc05_main(yPos);

            if (hc05IsRunning && !hc05_infunction)
            {
                ApplyBlurEffect(buf, 1);
            }
            else if (hc05_infunction && hc05IsRunning)
            {
                ApplyBlurEffect(buf, X2line_up(0, 1, i));
            }
            else if (hc05_outfunction)
            {
                ApplyBlurEffect(buf, X2line_down(0.8, 0, i));
            }
            else
            {
                UIDrawSgate(x, y, width, heigh, 8, 6, 1, 1);
            }

            if (hc05IsRunning)
            {
                UIDrawSgate(x, y, width, heigh, 8, 6, 1, 2);
                if (hc05_modal == HC05_MODAL_NAME)
                    draw_hc05_name_selector();
                else if (hc05_modal == HC05_MODAL_PASS)
                    draw_hc05_pass_selector();
            }

            render(buf, &frame_area);
            memset(buf, 0, SSD1306_BUF_LEN);
        }
    }

    lengthStart = widthEnd;
    heightStart = heighEnd;
    yStart = yEnd;
    xStart = xEnd;
    hc05_YPos_Start = hc05_YPos_End;
    hc05_infunction = false;
    hc05_outfunction = false;

    draw_hc05_main(hc05_YPos_End);

    if (hc05IsRunning)
    {
        ApplyBlurEffect(buf, 1);
        UIDrawSgate(xEnd, yEnd, widthEnd, heighEnd, 8, 6, 1, 2);
        if (hc05_modal == HC05_MODAL_NAME)
            draw_hc05_name_selector();
        else if (hc05_modal == HC05_MODAL_PASS)
            draw_hc05_pass_selector();
    }
    else
    {
        UIDrawSgate(xEnd, yEnd, widthEnd, heighEnd, 8, 6, 1, 1);
    }

    render(buf, &frame_area);
    memset(buf, 0, SSD1306_BUF_LEN);
}

extern "C" void hc05_at_mode(void)
{
    ResetOpn();

    extern bool send_pc_flag;
    send_pc_flag = true; // 触发向上位机更新数据

    // 每次进入页面都重置本页配置状态，避免继承上次流程结果。
    hc05IsRunning = false;
    hc05_infunction = false;
    hc05_outfunction = false;
    hc05_modal = HC05_MODAL_NONE;
    nowselect_hc05_item = 0;

    hc05_ms_s_verified = false;
    hc05_ms_m_verified = false;
    hc05_ms_phase = HC05_MS_PHASE_WAIT_S;
    hc05_ms_line3_msg = HC05_LINE_MSG_NONE;
    hc05_ms_line4_msg = HC05_LINE_MSG_NONE;
    hc05_ms_reset_pending = false;
    hc05_ms_need_master_swap = false;
    hc05_ms_seen_no_at_after_s = false;
    hc05_ms_random_generated = false;
    hc05_last_cfg_ok = false;
    hc05_ms_last_action = HC05_MS_LAST_NONE;
    hc05_last_send_us = 0;

    // Set active flag so UART config keeps AT mode
    hc05_is_active = true;

    strncpy(hc05_addr_part1, "0000", sizeof(hc05_addr_part1));
    strncpy(hc05_addr_part2, "00", sizeof(hc05_addr_part2));
    strncpy(hc05_addr_part3, "000000", sizeof(hc05_addr_part3));

    hc05_uart_enter_at_mode_config();
    hc05_uart_flush_rx();
    hc05_has_atmode_dev = false;
    hc05_last_probe_send_us = 0;
    hc05_last_ok_us = 0;
    hc05_send_task_busy = false;
    hc05_refresh_dev_state();

    if (hc05_work_mode == HC05_MODE_MS && !hc05_ms_random_generated)
    {
        hc05_generate_random_pass(HC05_PASS_RANDOM4_INDEX);
        hc05_pass_selection = HC05_PASS_RANDOM4_INDEX;
        hc05_ms_random_generated = true;
    }

    if (!hc05_item_selectable(nowselect_hc05_item))
    {
        nowselect_hc05_item = 0;
    }

    anni_hc05();

    if (hc05_protocol_pending)
    {
        hc05_protocol_pending = false;

        if (hc05_protocol_mode == HC05_MODE_S && hc05_work_mode != HC05_MODE_S)
        {
            hc05_enter_s_mode();
        }
        else if (hc05_protocol_mode == HC05_MODE_MS && hc05_work_mode != HC05_MODE_MS)
        {
            hc05_enter_ms_mode();
        }

        if (hc05_protocol_action == 1)
        {
            hc05_line4_action();
        }
        else if (hc05_protocol_action == 2)
        {
            hc05_ms_line3_action();
        }
        else if (hc05_protocol_action == 3)
        {
            hc05_ms_line4_action();
        }

        extern bool send_pc_flag;
        send_pc_flag = true;
    }

    while (true)
    {
        hc05_refresh_result_flags_timeout(time_us_64());
        hc05_refresh_dev_state();

        if (hc05_protocol_pending)
        {
            hc05_protocol_pending = false;

            if (hc05_protocol_mode == HC05_MODE_S && hc05_work_mode != HC05_MODE_S)
            {
                hc05_enter_s_mode();
            }
            else if (hc05_protocol_mode == HC05_MODE_MS && hc05_work_mode != HC05_MODE_MS)
            {
                hc05_enter_ms_mode();
            }

            if (hc05_protocol_action == 1)
            {
                hc05_line4_action();
            }
            else if (hc05_protocol_action == 2)
            {
                hc05_ms_line3_action();
            }
            else if (hc05_protocol_action == 3)
            {
                hc05_ms_line4_action();
            }

            extern bool send_pc_flag;
            send_pc_flag = true;
        }

        if (hc05_work_mode == HC05_MODE_MS && hc05_ms_phase == HC05_MS_PHASE_HAVE_S_ADDR && hc05_ms_need_master_swap)
        {
            if (!hc05_has_atmode_dev)
            {
                if (!hc05_ms_seen_no_at_after_s)
                {
                    hc05_ms_seen_no_at_after_s = true;
                    extern bool send_pc_flag;
                    send_pc_flag = true;
                }
            }
            else if (hc05_ms_seen_no_at_after_s)
            {
                hc05_ms_need_master_swap = false;
                extern bool send_pc_flag;
                send_pc_flag = true;
            }
        }

        if (opnEnter || opnExit || opnUp || opnDown)
        {
            if (hc05IsRunning)
            {
                if (hc05_modal == HC05_MODAL_NAME)
                {
                    if (opnUp)
                    {
                        hc05_temp_name_suffix++;
                        if (hc05_temp_name_suffix > 99)
                            hc05_temp_name_suffix = 1;
                    }
                    else if (opnDown)
                    {

                        hc05_temp_name_suffix--;
                        if (hc05_temp_name_suffix < 1)
                            hc05_temp_name_suffix = 99;
                    }
                    else if (opnEnter)
                    {
                        hc05_name_suffix = hc05_temp_name_suffix;
                        hc05_use_custom_name = false; // Local selection overrides PC custom name
                        hc05IsRunning = false;
                        hc05_outfunction = true;
                        hc05_modal = HC05_MODAL_NONE;
                        extern bool send_pc_flag;
                        send_pc_flag = true; // 触发向上位机更新数据
                    }
                    else if (opnExit)
                    {
                        hc05IsRunning = false;
                        hc05_outfunction = true;
                        hc05_modal = HC05_MODAL_NONE;
                    }
                }
                else if (hc05_modal == HC05_MODAL_PASS)
                {
                    if (opnUp)
                    {
                        hc05_temp_pass_selection = (hc05_temp_pass_selection + HC05_PASS_OPTION_COUNT - 1) % HC05_PASS_OPTION_COUNT;
                    }
                    else if (opnDown)
                    {
                        hc05_temp_pass_selection = (hc05_temp_pass_selection + 1) % HC05_PASS_OPTION_COUNT;
                    }
                    else if (opnEnter)
                    {
                        hc05_pass_selection = hc05_temp_pass_selection;
                        if (hc05_pass_selection == HC05_PASS_RANDOM4_INDEX || hc05_pass_selection == HC05_PASS_RANDOM6_INDEX)
                        {
                            hc05_generate_random_pass(hc05_pass_selection);
                        }
                        hc05_use_custom_pass = false; // Local selection overrides PC custom pass
                        hc05IsRunning = false;
                        hc05_outfunction = true;
                        hc05_modal = HC05_MODAL_NONE;
                        extern bool send_pc_flag;
                        send_pc_flag = true; // 触发向上位机更新数据
                    }
                    else if (opnExit)
                    {
                        hc05IsRunning = false;
                        hc05_outfunction = true;
                        hc05_modal = HC05_MODAL_NONE;
                    }
                }
                else if (opnExit)
                {
                    hc05IsRunning = false;
                    hc05_outfunction = true;
                }
            }
            else
            {
                if (opnUp)
                {
                    hc05_move_selection(-1);
                }
                else if (opnDown)
                {
                    hc05_move_selection(1);
                }
                else if (opnEnter)
                {

                    switch (nowselect_hc05_item)
                    {
                    case 0:

                        if (hc05_work_mode == HC05_MODE_S)
                        {
                            hc05_last_cfg_ok = false;
                            hc05_enter_ms_mode();
                        }
                        else
                        {
                            hc05_last_cfg_ok = false;
                            hc05_enter_s_mode();
                        }
                        if (!hc05_item_selectable(nowselect_hc05_item))
                        {
                            hc05_move_selection(1);
                        }
                        extern bool send_pc_flag;
                        send_pc_flag = true; // 触发向上位机更新数据

                        break;
                    case 1:
                        if (hc05_work_mode == HC05_MODE_S)
                        {
                            hc05_temp_name_suffix = hc05_name_suffix;
                            hc05_modal = HC05_MODAL_NAME;
                            hc05IsRunning = true;
                            hc05_infunction = true;
                        }
                        else
                        {
                            // S&M 第二行进入 UART 配置
                            hc05_modal = HC05_MODAL_NONE;
                            hc05IsRunning = true;
                            hc05_infunction = true;
                        }
                        break;
                    case 2:
                        if (hc05_work_mode == HC05_MODE_S)
                        {
                            hc05_temp_pass_selection = hc05_pass_selection;
                            hc05_modal = HC05_MODAL_PASS;
                            hc05IsRunning = true;
                            hc05_infunction = true;
                        }
                        else
                        {
                            hc05_ms_line3_action();
                        }
                        break;
                    case 3:
                        if (hc05_work_mode == HC05_MODE_S)
                        {
                            hc05_modal = HC05_MODAL_NONE;
                            hc05IsRunning = true;
                            hc05_infunction = true;
                        }
                        else
                        {
                            hc05_ms_line4_action();
                        }
                        break;
                    case 4:
                        if (hc05_work_mode == HC05_MODE_S)
                        {
                            hc05_line4_action();
                        }
                        break;
                    default:
                        break;
                    }
                }
                else if (opnExit)
                {
                    ResetOpn();
                    return;
                }
            }

            ResetOpn();
        }

        if (hc05IsRunning && hc05_modal == HC05_MODAL_NONE &&
            ((hc05_work_mode == HC05_MODE_S && nowselect_hc05_item == 3) ||
             (hc05_work_mode == HC05_MODE_MS && nowselect_hc05_item == 1)))
        {
            uartconfig_infunction = 1;

            hc05IsRunning = uartconfig();

            // 从 UART 配置页返回后，强制恢复 HC-05 的 AT 串口参数（38400/8N1）。
            hc05_uart_enter_at_mode_config();
            extern bool send_pc_flag;
            send_pc_flag = true; // 触发向上位机更新数据
            hc05_uart_flush_rx();

            // 触发重新探测，确保底部在线状态及时刷新。
            hc05_has_atmode_dev = false;
            hc05_last_probe_send_us = 0;
        }

        extern bool opnPCchangemode;
        extern bool opnPCchangetool;
        if (opnLeft || opnRight || opnPCchangemode || opnPCchangetool)
        {
            hc05_is_active = false;
            return;
        }

        anni_hc05();
    }
}

extern "C" int hc05_get_mode_code(void)
{
    return (int)hc05_work_mode;
}

extern "C" int hc05_get_modal_code(void)
{
    return (int)hc05_modal;
}

extern "C" int hc05_get_name_suffix_value(void)
{
    return hc05_name_suffix;
}

extern "C" const char *hc05_get_name_text_value(void)
{
    return hc05_get_name_text();
}

extern "C" const char *hc05_get_pass_selection_value(void)
{
    return hc05_get_pass_text();
}

extern "C" void hc05_set_protocol_config(int mode, const char *name, const char *password, int action)
{

    hc05_protocol_mode = (mode == 1) ? HC05_MODE_MS : HC05_MODE_S;
    hc05_protocol_action = action;

    hc05_protocol_pending = true;

    if (name && *name)
    {
        strncpy(hc05_custom_name, name, sizeof(hc05_custom_name) - 1);
        hc05_custom_name[sizeof(hc05_custom_name) - 1] = '\0';
        hc05_use_custom_name = true;
    }
    else
    {
        hc05_custom_name[0] = '\0';
        hc05_use_custom_name = false;
    }

    if (password && *password)
    {
        strncpy(hc05_custom_pass, password, sizeof(hc05_custom_pass) - 1);
        hc05_custom_pass[sizeof(hc05_custom_pass) - 1] = '\0';
        hc05_use_custom_pass = true;
    }
    else
    {
        hc05_custom_pass[0] = '\0';
        hc05_use_custom_pass = false;
    }
}

extern "C" int hc05_get_s_mode_result_ok_value(void)
{
    hc05_refresh_result_flags_timeout(time_us_64());
    return hc05_s_mode_result_ok ? 1 : 0;
}

extern "C" int hc05_get_s_mode_result_fail_value(void)
{
    hc05_refresh_result_flags_timeout(time_us_64());
    return hc05_s_mode_result_fail ? 1 : 0;
}

extern "C" int hc05_get_ms_s_result_ok_value(void)
{
    hc05_refresh_result_flags_timeout(time_us_64());
    return hc05_ms_s_result_ok ? 1 : 0;
}

extern "C" int hc05_get_ms_s_result_fail_value(void)
{
    hc05_refresh_result_flags_timeout(time_us_64());
    return hc05_ms_s_result_fail ? 1 : 0;
}

extern "C" int hc05_get_ms_m_result_ok_value(void)
{
    hc05_refresh_result_flags_timeout(time_us_64());
    return hc05_ms_m_result_ok ? 1 : 0;
}

extern "C" int hc05_get_ms_m_result_fail_value(void)
{
    hc05_refresh_result_flags_timeout(time_us_64());
    return hc05_ms_m_result_fail ? 1 : 0;
}

extern "C" int hc05_get_has_atmode_dev_value(void)
{
    return hc05_has_atmode_dev ? 1 : 0;
}

extern "C" int hc05_get_last_cfg_ok_value(void)
{
    return hc05_last_cfg_ok ? 1 : 0;
}

extern "C" int hc05_get_ms_s_verified_value(void)
{
    return hc05_ms_s_verified ? 1 : 0;
}

extern "C" int hc05_get_ms_m_verified_value(void)
{
    return hc05_ms_m_verified ? 1 : 0;
}

#endif // _HC05_CPP_
