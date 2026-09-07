// #include "status_display.h"

// #include "fonts.h"
// #include "hardware/gpio.h"
// #include "hardware/i2c.h"
// #include "hardware/watchdog.h"
// #include "pico/multicore.h"
// #include "pico/time.h"
// #include "hardware/pwm.h"

// #include <stdbool.h>
// #include <stdint.h>
// #include <stdatomic.h>
// #include <stddef.h>
// #include <string.h>
// #include <math.h>
// #include <stdlib.h>

// //////////////////////////////////////显示部分的引脚配置

// #define SYS_MODE_PIN 29
// #define SSD_RES 16
// #define LED_PIN 2 // 常亮LED
// #define LED_DEBUG  25

// #define I2C_PORT i2c1
// #define PICO_I2C_SDA_PIN 14
// #define PICO_I2C_SCL_PIN 15

// #define SELECTERPIN0 20
// #define SELECTERPIN1 21

// #define PULLUP_EN_PIN 0
// #define TEST_SPI_CS_PIN 5//复用端口  需要调为高阻

// //////////////////////////////////////显示相关的定义
// #define SSD1306_ADDRESS 0x3C
// #define SSD1306_COMMAND_CONTROL 0x00
// #define SSD1306_DATA_CONTROL 0x40
// #define DISPLAY_WIDTH 128
// #define DISPLAY_HEIGHT 64
// #define DISPLAY_PAGES (DISPLAY_HEIGHT / 8)
// #define STATUS_REFRESH_MS 200

// #define TABLE_TOP 32
// #define TABLE_LEFT 0
// #define TABLE_RIGHT (DISPLAY_WIDTH - 1)
// #define TABLE_ROW_PADDING 1
// #define SLIDE_WIDTH 30
// #define SLIDE_SPEED 10
// #define SLIDE_CAPACITY 8

// static uint8_t display_buffer[DISPLAY_WIDTH * DISPLAY_PAGES];
// static atomic_bool status_flags[STATUS_DISPLAY_COUNT];
// static atomic_int status_pending[STATUS_DISPLAY_COUNT];
// static bool display_started;
// static uint8_t page_out[DISPLAY_WIDTH + 1];
// static bool debug_force_usb_disconnected;
// static bool debug_force_dap_disconnected;
// static atomic_bool usb_status_hold_flag;
// static atomic_bool dap_status_hold_flag;
// static bool gpiosysmode_last_state;

// typedef struct
// {
//     bool active;
//     int32_t x;
//     int32_t delta;
// } slide_t;

// typedef struct
// {
//     int32_t start;
//     int32_t end;
// } interval_t;

// static slide_t tx_slides[SLIDE_CAPACITY];
// static slide_t rx_slides[SLIDE_CAPACITY];

// // LED run PWM
// #define PWM_MAX 80
// typedef enum {
//     LED_OFF = 0,
//     LED_ON = 1,
// } led_mode_t;

// static atomic_int led_mode;
// static uint16_t current_brightness;
// static struct repeating_timer led_timer;
// // flag set by display code when it detects activity; timer watches for edges
// static atomic_bool led_display_signal;

// static inline int text_width(const char *text, const sFONT *font)
// {
//     int width = 0;
//     while (*text++)
//     {
//         width += font->Width + 1;
//     }
//     if (width > 0)
//     {
//         width -= 1;
//     }
//     return width;
// }

// static inline void set_pixel(int x, int y, bool color)
// {
//     if ((unsigned)x >= DISPLAY_WIDTH || (unsigned)y >= DISPLAY_HEIGHT)
//         return;
//     const unsigned page = y >> 3;
//     const size_t index = page * DISPLAY_WIDTH + x;
//     if (color)
//         display_buffer[index] |= (1u << (y & 7));
//     else
//         display_buffer[index] &= ~(1u << (y & 7));
// }

// static void fill_rect(int x, int y, int width, int height, bool color)
// {
//     if (width <= 0 || height <= 0)
//         return;
//     for (int yy = y; yy < y + height; yy++)
//     {
//         for (int xx = x; xx < x + width; xx++)
//         {
//             set_pixel(xx, yy, color);
//         }
//     }
// }

// static void draw_char(int x, int y, char c, const sFONT *font, bool color)
// {
//     if (c < 32 || c > 126)
//         return;
//     const int bytes_per_row = (font->Width + 7) / 8;
//     const uint8_t *glyph = font->table + (size_t)(c - 32) * font->Height * bytes_per_row;
//     for (int row = 0; row < font->Height; row++)
//     {
//         for (int col = 0; col < font->Width; col++)
//         {
//             const int byte_index = (col / 8);
//             const int bit_index = 7 - (col & 7);
//             if (glyph[row * bytes_per_row + byte_index] & (1u << bit_index))
//             {
//                 set_pixel(x + col, y + row, color);
//             }
//         }
//     }
// }

// static int draw_string(int x, int y, const char *text, const sFONT *font, bool color)
// {
//     int cursor = 0;
//     while (*text)
//     {
//         draw_char(x + cursor, y, *text, font, color);
//         cursor += font->Width + 1;
//         text++;
//     }
//     if (cursor > 0)
//         cursor -= 1;
//     return cursor;
// }

// static int char_spacing_adjust(char c)
// {
//     if (c == 'I' || c == '-')
//         return -2;
//     return 0;
// }

// static int compute_adjusted_width(const char *text, const sFONT *font)
// {
//     int width = 0;
//     bool first = true;
//     while (*text)
//     {
//         if (!first && char_spacing_adjust(*text) < 0)
//             width += char_spacing_adjust(*text);
//         width += font->Width + 1 + char_spacing_adjust(*text);
//         first = false;
//         text++;
//     }
//     if (width > 0)
//         width -= 1;
//     return width;
// }

// static int draw_string_adjusted(int x, int y, const char *text, const sFONT *font, bool color)
// {
//     int cursor = x;
//     bool first = true;
//     while (*text)
//     {
//         const char c = *text;
//         if (!first && char_spacing_adjust(c) < 0)
//             cursor += char_spacing_adjust(c);
//         draw_char(cursor, y, c, font, color);
//         cursor += font->Width + 1 + char_spacing_adjust(c);
//         first = false;
//         text++;
//     }
//     return cursor - x;
// }

// static inline int table_row_height(void)
// {
//     return Font12.Height + 3;
// }

// static inline int row1_y(void)
// {
//     return TABLE_TOP + TABLE_ROW_PADDING;
// }

// static inline int row2_y(void)
// {
//     return row1_y() + table_row_height() + TABLE_ROW_PADDING;
// }

// static inline int table_bottom(void)
// {
//     return row2_y() + table_row_height() - 1;
// }

// static inline void invert_rect(int x, int y, int width, int height)
// {
//     if (width <= 0 || height <= 0)
//         return;
//     for (int yy = y; yy < y + height; yy++)
//     {
//         if ((unsigned)yy >= DISPLAY_HEIGHT)
//             continue;
//         const unsigned page = yy >> 3;
//         const uint8_t bit_mask = 1u << (yy & 7);
//         for (int xx = x; xx < x + width; xx++)
//         {
//             if ((unsigned)xx >= DISPLAY_WIDTH)
//                 continue;
//             const size_t index = page * DISPLAY_WIDTH + xx;
//             display_buffer[index] ^= bit_mask;
//         }
//     }
// }

// static void queue_slide(slide_t *slides, int32_t start_x, int32_t delta)
// {
//     for (int i = 0; i < SLIDE_CAPACITY; i++)
//     {
//         if (!slides[i].active)
//         {
//             slides[i].active = true;
//             slides[i].x = start_x;
//             slides[i].delta = delta;
//             return;
//         }
//     }
// }

// static void slide_tick(slide_t *slides)
// {
//     for (int i = 0; i < SLIDE_CAPACITY; i++)
//     {
//         if (!slides[i].active)
//             continue;
//         slides[i].x += slides[i].delta;
//         if (slides[i].delta > 0 && slides[i].x > TABLE_RIGHT)
//             slides[i].active = false;
//         else if (slides[i].delta < 0 && slides[i].x + SLIDE_WIDTH < TABLE_LEFT)
//             slides[i].active = false;
//     }
// }

// static inline bool usb_display_connected(void)
// {
//     const bool connected = atomic_load_explicit(&status_flags[STATUS_DISPLAY_USB_CONNECTED], memory_order_relaxed);
//     return connected && !debug_force_usb_disconnected;
// }

// static inline bool dap_display_connected(void)
// {
//     const bool connected = atomic_load_explicit(&status_flags[STATUS_DISPLAY_DAP_CONNECTED], memory_order_relaxed);
//     return connected && !debug_force_dap_disconnected;
// }

// static void update_slide_state(void)
// {
//     const bool usb_connected = atomic_load_explicit(&status_flags[STATUS_DISPLAY_USB_CONNECTED], memory_order_relaxed);

//     if (usb_connected)
//     {
//         int tx_pending = atomic_exchange_explicit(&status_pending[STATUS_DISPLAY_UART_TX], 0, memory_order_relaxed);
//         int rx_pending = atomic_exchange_explicit(&status_pending[STATUS_DISPLAY_UART_RX], 0, memory_order_relaxed);
//         while (tx_pending-- > 0)
//             queue_slide(tx_slides, TABLE_LEFT - SLIDE_WIDTH, SLIDE_SPEED);
//         while (rx_pending-- > 0)
//             queue_slide(rx_slides, TABLE_RIGHT, -SLIDE_SPEED);
//     }
//     slide_tick(tx_slides);
//     slide_tick(rx_slides);
// }

// static void draw_top_lines(void)
// {
//     const char *part_main = "CMSIS-DAP";
//     const char *part_tail = "v2";
//     const int main_width = compute_adjusted_width(part_main, &Font16);
//     const int tail_width = text_width(part_tail, &Font8);
//     const int total_width = main_width + tail_width;
//     const int base_x = (DISPLAY_WIDTH - total_width) / 2;
//     draw_string_adjusted(base_x, 4, part_main, &Font16, true);
//     const int tail_height = Font8.Height;
//     const int tail_y = 2 + Font16.Height - tail_height;
//     draw_string(base_x + main_width + 3, tail_y, part_tail, &Font12, true);

//     const char *segments[] = {"Debug", "Probe", "v2.2.3"};
//     const int segment_gap = (Font8.Width + 5) / 2;
//     int segment_widths[3];
//     int total_segments_width = 0;
//     for (int i = 0; i < 3; i++)
//     {
//         segment_widths[i] = text_width(segments[i], &Font8);
//         total_segments_width += segment_widths[i];
//     }
//     total_segments_width += segment_gap * 2;
//     int segment_x = (DISPLAY_WIDTH - total_segments_width) / 2;
//     const int line2_y = 23;
//     for (int i = 0; i < 3; i++)
//     {
//         draw_string(segment_x, line2_y, segments[i], &Font8, true);
//         segment_x += segment_widths[i];
//         if (i < 2)
//             segment_x += segment_gap;
//     }
// }

// static void draw_table_outline(void)
// {
//     const int left = TABLE_LEFT;
//     const int right = TABLE_RIGHT;
//     const int top = TABLE_TOP;
//     const int bottom = table_bottom();

//     fill_rect(left, top, right - left + 1, 1, true);
//     fill_rect(left, row2_y() - TABLE_ROW_PADDING, right - left + 1, 1, true);
//     fill_rect(left, bottom, right - left + 1, 1, true);
//     fill_rect(left, top, 1, bottom - top + 1, true);
//     fill_rect(right, top, 1, bottom - top + 1, true);
// }

// static void draw_row3_table_text(void)
// {
//     const int row_height = table_row_height();
//     const int row3_y = row1_y() + (row_height - Font12.Height) / 2 + 1;
//     const char *row3 = "USB:CDC <> UART";
//     const int row3_x = (DISPLAY_WIDTH - text_width(row3, &Font12)) / 2;
//     draw_string(row3_x, row3_y, row3, &Font12, true);
// }

// static void draw_row4_table_text(void)
// {
//     const int row_height = table_row_height();
//     const int row4_y = row2_y() + (row_height - Font12.Height) / 2 + 1;
//     const char *row4 = "USB:DAP <> SWD";
//     const int row4_x = (DISPLAY_WIDTH - text_width(row4, &Font12)) / 2;
//     draw_string(row4_x, row4_y, row4, &Font12, true);
// }

// static void draw_row_message(int y, const char *message)
// {
//     const int row_height = table_row_height();
//     // fill_rect(TABLE_LEFT + 1, y, TABLE_RIGHT - TABLE_LEFT - 1, row_height, false);
//     const int text_y = y + (row_height - Font12.Height) / 2 + 1;
//     const int text_x = (DISPLAY_WIDTH - text_width(message, &Font12)) / 2;
//     draw_string(text_x, text_y, message, &Font12, true);
// }

// static void add_rect_interval(interval_t *rects, int *count, int32_t start, int32_t end)
// {
//     if (*count >= SLIDE_CAPACITY * 2)
//         return;
//     rects[*count].start = start;
//     rects[*count].end = end;
//     (*count)++;
// }

// static int merge_intervals(interval_t *input, int count, interval_t *output)
// {
//     if (count == 0)
//         return 0;
//     for (int i = 0; i < count - 1; i++)
//     {
//         for (int j = i + 1; j < count; j++)
//         {
//             if (input[j].start < input[i].start)
//             {
//                 interval_t tmp = input[i];
//                 input[i] = input[j];
//                 input[j] = tmp;
//             }
//         }
//     }
//     int out_idx = 0;
//     int32_t current_start = input[0].start;
//     int32_t current_end = input[0].end;
//     for (int i = 1; i < count; i++)
//     {
//         if (input[i].start <= current_end)
//         {
//             if (input[i].end > current_end)
//                 current_end = input[i].end;
//         }
//         else
//         {
//             output[out_idx++] = (interval_t){.start = current_start, .end = current_end};
//             current_start = input[i].start;
//             current_end = input[i].end;
//         }
//     }
//     output[out_idx++] = (interval_t){.start = current_start, .end = current_end};
//     return out_idx;
// }

// static int subtract_interval(interval_t base, interval_t *cutters, int cutter_count, interval_t *output, int max_output)
// {
//     int count = 0;
//     int32_t current_start = base.start;
//     const int32_t base_end = base.end;
//     for (int i = 0; i < cutter_count && current_start < base_end; i++)
//     {
//         if (cutters[i].end <= current_start)
//             continue;
//         if (cutters[i].start >= base_end)
//             break;
//         if (cutters[i].start > current_start)
//         {
//             if (count >= max_output)
//                 break;
//             output[count++] = (interval_t){.start = current_start, .end = cutters[i].start};
//         }
//         if (cutters[i].end > current_start)
//             current_start = cutters[i].end;
//     }
//     if (current_start < base_end && count < max_output)
//         output[count++] = (interval_t){.start = current_start, .end = base_end};
//     return count;
// }

// static void merge_and_invert(interval_t *rects, int count, int y, int height)
// {
//     if (count == 0)
//         return;
//     interval_t merged[SLIDE_CAPACITY * 4];
//     int merged_count = merge_intervals(rects, count, merged);
//     for (int i = 0; i < merged_count; i++)
//         invert_rect(merged[i].start, y, merged[i].end - merged[i].start, height);
// }

// static void apply_third_row_animation(void)
// {
//     const int height = table_row_height();
//     const int min_x = TABLE_LEFT + 1;
//     const int max_x = TABLE_RIGHT - 1;

//     interval_t tx_rects[SLIDE_CAPACITY * 2];
//     interval_t rx_rects[SLIDE_CAPACITY * 2];
//     int tx_count = 0;
//     int rx_count = 0;

//     for (int i = 0; i < SLIDE_CAPACITY; i++)
//     {
//         if (tx_slides[i].active)
//         {
//             int32_t start = tx_slides[i].x;
//             int32_t end = tx_slides[i].x + SLIDE_WIDTH;
//             if (end > min_x && start < max_x)
//             {
//                 if (start < min_x)
//                     start = min_x;
//                 if (end > max_x)
//                     end = max_x;
//                 add_rect_interval(tx_rects, &tx_count, start, end);
//             }
//         }
//         if (rx_slides[i].active)
//         {
//             int32_t start = rx_slides[i].x;
//             int32_t end = rx_slides[i].x + SLIDE_WIDTH;
//             if (end > min_x && start < max_x)
//             {
//                 if (start < min_x)
//                     start = min_x;
//                 if (end > max_x)
//                     end = max_x;
//                 add_rect_interval(rx_rects, &rx_count, start, end);
//             }
//         }
//     }

//     interval_t merged_tx[SLIDE_CAPACITY * 2];
//     interval_t merged_rx[SLIDE_CAPACITY * 2];
//     int merged_tx_count = merge_intervals(tx_rects, tx_count, merged_tx);
//     int merged_rx_count = merge_intervals(rx_rects, rx_count, merged_rx);

//     interval_t overlap_rects[SLIDE_CAPACITY * 2];
//     int overlap_count = 0;
//     for (int i = 0; i < merged_tx_count; i++)
//     {
//         for (int j = 0; j < merged_rx_count; j++)
//         {
//             int32_t start = merged_tx[i].start > merged_rx[j].start ? merged_tx[i].start : merged_rx[j].start;
//             int32_t end = merged_tx[i].end < merged_rx[j].end ? merged_tx[i].end : merged_rx[j].end;
//             if (start < end)
//                 add_rect_interval(overlap_rects, &overlap_count, start, end);
//         }
//     }
//     interval_t merged_overlap[SLIDE_CAPACITY * 2];
//     int merged_overlap_count = merge_intervals(overlap_rects, overlap_count, merged_overlap);

//     const int tx_height = (height * 2) / 3;
//     const int tx_y = row1_y();
//     const int rx_height = (height * 2) / 3;
//     const int rx_y = row1_y() + height - rx_height;
//     interval_t tx_non_overlap[SLIDE_CAPACITY * 2];
//     int tx_non_overlap_count = 0;
//     interval_t rx_non_overlap[SLIDE_CAPACITY * 2];
//     int rx_non_overlap_count = 0;
//     for (int i = 0; i < merged_tx_count; i++)
//     {
//         if (tx_non_overlap_count >= SLIDE_CAPACITY * 2)
//             break;
//         const int available = SLIDE_CAPACITY * 2 - tx_non_overlap_count;
//         int added = subtract_interval(merged_tx[i], merged_overlap, merged_overlap_count,
//                                       tx_non_overlap + tx_non_overlap_count, available);
//         tx_non_overlap_count += added;
//     }
//     for (int i = 0; i < merged_rx_count; i++)
//     {
//         if (rx_non_overlap_count >= SLIDE_CAPACITY * 2)
//             break;
//         const int available = SLIDE_CAPACITY * 2 - rx_non_overlap_count;
//         int added = subtract_interval(merged_rx[i], merged_overlap, merged_overlap_count,
//                                       rx_non_overlap + rx_non_overlap_count, available);
//         rx_non_overlap_count += added;
//     }

//     for (int i = 0; i < tx_non_overlap_count; i++)
//         invert_rect(tx_non_overlap[i].start, tx_y, tx_non_overlap[i].end - tx_non_overlap[i].start, tx_height);
//     for (int i = 0; i < rx_non_overlap_count; i++)
//         invert_rect(rx_non_overlap[i].start, rx_y, rx_non_overlap[i].end - rx_non_overlap[i].start, rx_height);

//     if (merged_overlap_count > 0)
//         merge_and_invert(merged_overlap, merged_overlap_count, row1_y(), height);
// }

// static void apply_fourth_row_highlight(void)
// {
//     const int y = row2_y();
//     const int height = table_row_height();
//     invert_rect(TABLE_LEFT + 1, y, TABLE_RIGHT - TABLE_LEFT - 1, height);
// }

// static void render_buffer(void)
// {
//     memset(display_buffer, 0, sizeof(display_buffer));
//     update_slide_state();
//     draw_top_lines();
//     const bool usb_connected = usb_display_connected();
//     const bool dap_connected = dap_display_connected();
//     const bool dap_running = atomic_load_explicit(&status_flags[STATUS_DISPLAY_DAP_RUNNING], memory_order_relaxed);
//     const bool dap_hold = atomic_load_explicit(&dap_status_hold_flag, memory_order_relaxed);
//     const int row_height = table_row_height();
//     const bool usb_hold = atomic_load_explicit(&usb_status_hold_flag, memory_order_relaxed);

//     // If display sees activity (slide animations or DAP running), set LED request (only set true)
//     {
//         bool activity = dap_running;
//         for (int i = 0; i < SLIDE_CAPACITY; i++)
//         {
//             if (tx_slides[i].active || rx_slides[i].active)
//             {
//                 activity = true;
//                 break;
//             }
//         }
//         if (activity)
//             atomic_store_explicit(&led_display_signal, true, memory_order_relaxed);
//     }

//     draw_table_outline();

//     if (usb_connected && usb_hold)
//     {
//         invert_rect(TABLE_LEFT + 1, row1_y(), TABLE_RIGHT - TABLE_LEFT - 1, row_height);
//         atomic_store_explicit(&usb_status_hold_flag, false, memory_order_relaxed);
//     }

//     if (usb_connected)
//         draw_row3_table_text();
//     else
//         draw_row_message(row1_y(), "USB Unconnected");
//     if (dap_connected)
//         draw_row4_table_text();
//     else
//         draw_row_message(row2_y(), "DAP Unconnected");
//     if (usb_connected)
//         apply_third_row_animation();
//     const bool dap_highlight = dap_connected && (dap_running || dap_hold);
//     if (dap_highlight)
//     {
//         apply_fourth_row_highlight();
//         if (dap_hold)
//             atomic_store_explicit(&dap_status_hold_flag, false, memory_order_relaxed);
//     }
// }

// static void ssd1306_write_command(uint8_t command)
// {
//     uint8_t packet[2] = {SSD1306_COMMAND_CONTROL, command};
//     (void)i2c_write_blocking(I2C_PORT, SSD1306_ADDRESS, packet, sizeof(packet), false);
// }

// static void ssd1306_write_page(uint8_t page)
// {
//     ssd1306_write_command(0xB0 | page);
//     ssd1306_write_command(0x00);
//     ssd1306_write_command(0x10);
//     page_out[0] = SSD1306_DATA_CONTROL;
//     memcpy(page_out + 1, &display_buffer[page * DISPLAY_WIDTH], DISPLAY_WIDTH);
//     (void)i2c_write_blocking(I2C_PORT, SSD1306_ADDRESS, page_out, DISPLAY_WIDTH + 1, false);
// }

// static void ssd1306_update(void)
// {
//     for (uint8_t page = 0; page < DISPLAY_PAGES; page++)
//     {
//         ssd1306_write_page(page);
//     }
// }

// static void ssd1306_init(void)
// {
//     ssd1306_write_command(0xAE);
//     ssd1306_write_command(0x20);
//     ssd1306_write_command(0x00);
//     ssd1306_write_command(0xB0);
//     ssd1306_write_command(0xC8);
//     ssd1306_write_command(0x00);
//     ssd1306_write_command(0x10);
//     ssd1306_write_command(0x40);
//     ssd1306_write_command(0x81);
//     ssd1306_write_command(0xFF);
//     ssd1306_write_command(0xA1);
//     ssd1306_write_command(0xA6);
//     ssd1306_write_command(0xA8);
//     ssd1306_write_command(0x3F);
//     ssd1306_write_command(0xA4);
//     ssd1306_write_command(0xD3);
//     ssd1306_write_command(0x00);
//     ssd1306_write_command(0xD5);
//     ssd1306_write_command(0x80);
//     ssd1306_write_command(0xD9);
//     ssd1306_write_command(0xF1);
//     ssd1306_write_command(0xDA);
//     ssd1306_write_command(0x12);
//     ssd1306_write_command(0xDB);
//     ssd1306_write_command(0x40);
//     ssd1306_write_command(0x8D);
//     ssd1306_write_command(0x14);
//     ssd1306_write_command(0xAF);
// }

// float X2line_down(float start, float end, float lamda)
// {
//     if (lamda > 1.0)
//     {
//         lamda = 1.0;
//     }

//     return end + (start - end) * (1 - lamda) * (1 - lamda) * (1 - lamda);
// }

// float X2line_up(float start, float end, float lamda)
// {
//     if (lamda > 1.0)
//     {
//         lamda = 1.0;
//     }
//     return start + (end - start) * lamda * lamda;
// }

// // 缓入缓出曲线
// float easeInOutQuad(float start, float end, float lamda)
// {
//     float c = end - start; // 计算变化量
//     float t = lamda;       // 时间参数就是 lambda

//     t *= 2; // 将 t 的范围调整到 [0, 2]

//     if (t < 1)
//     {
//         return c / 2 * t * t + start; // 前半段使用简单的二次函数
//     }
//     else
//     {
//         t--;
//         return -c / 2 * (t * (t - 2) - 1) + start; // 后半段使用稍复杂的二次函数
//     }
// }

// // LED handling: update brightness on 50ms timer
// static void stateLED(void)
// {
//     int lm = atomic_load_explicit(&led_mode, memory_order_relaxed);
//     switch (lm)
//     {
//     case LED_ON:
//         if (current_brightness < PWM_MAX)
//         {
//             if (current_brightness > PWM_MAX - 20)
//                 current_brightness = PWM_MAX;
//             else
//                 current_brightness += 34;
//         }
//         break;
//     case LED_OFF:
//         if (current_brightness > 0)
//         {
//             if (current_brightness < 2)
//                 current_brightness = 0;
//             else
//                 current_brightness = (uint16_t)(current_brightness * 0.7f);
//         }
//         break;
//     default:
//         break;
//     }

//     if (current_brightness > PWM_MAX)
//         current_brightness = PWM_MAX;
//     // current_brightness is unsigned, no need to check < 0
//     pwm_set_gpio_level(LED_PIN, current_brightness);
// }

// static bool led_timer_callback(struct repeating_timer *rt)
// {
//     // led_timer only performs edge detection on `led_display_signal` and updates PWM

//     // Edge detection on display-provided signal: 0->1 quick on, 1->0 slow off
//     static bool prev_signal = false;
//     // Exchange: get whether display has requested activity since last tick and clear it
//     bool cur_signal = atomic_exchange_explicit(&led_display_signal, false, memory_order_relaxed);
//     if (!prev_signal && cur_signal)
//         atomic_store_explicit(&led_mode, LED_ON, memory_order_relaxed);
//     else if (prev_signal && !cur_signal)
//         atomic_store_explicit(&led_mode, LED_OFF, memory_order_relaxed);
//     prev_signal = cur_signal;

//     stateLED();
//     return true; // keep timer repeating
// }

// static int compact_text_width(const char *text, const sFONT *font, int gap)
// {
//     int width = 0;
//     while (*text++)
//     {
//         width += font->Width + gap;
//     }
//     return width;
// }

// static void draw_string_compact(int x, int y, const char *text, const sFONT *font, bool color, int gap)
// {
//     int cursor = 0;
//     while (*text)
//     {
//         draw_char(x + cursor, y, *text, font, color);
//         cursor += font->Width + gap;
//         text++;
//     }
// }

// static void show_splash_screen(void)
// {
//     const char *line1 = "Raspberry Pi";
//     const char *line2 = "Debug Probe";
//     const char *line3 = "Powered by";
//     const char *line4 = "PIVOTECH";

//     // Calculate Y positions for balanced spacing
//     const int y1 = 1 + 18;  // First line: Font12
//     const int y2 = 14 + 18; // Second line: Font16
//     const int y3 = 33 + 31; // Third line: Font12
//     const int y4 = 44 + 31; // Fourth line: Font24

//     // Clear display buffer
//     memset(display_buffer, 0, sizeof(display_buffer));

//     // Draw static text lines
//     int width1 = compact_text_width(line1, &Font16, -1);
//     int x1 = (DISPLAY_WIDTH - width1) / 2;
//     draw_string_compact(x1, y1, line1, &Font16, true, -1);

//     int width2 = compact_text_width(line2, &Font16, -1);
//     int x2 = (DISPLAY_WIDTH - width2) / 2;
//     draw_string_compact(x2, y2, line2, &Font16, true, -1);

//     int width3 = compact_text_width(line3, &Font12, 0);
//     int x3 = (DISPLAY_WIDTH - width3) / 2;
//     draw_string_compact(x3, y3, line3, &Font12, true, 0);

//     int width4 = compact_text_width(line4, &Font24, -3);
//     int x4 = (DISPLAY_WIDTH - width4) / 2;
//     draw_string_compact(x4, y4, line4, &Font24, true, -3);

//     // Update display
//     ssd1306_update();

//     for (int i = 0; i < 5; i++)
//     {
//         // Watchdog check
//         bool gpio_state = gpio_get(SYS_MODE_PIN);
//         if (gpio_state != gpiosysmode_last_state)
//             watchdog_reboot(0, 0, 0);
//         sleep_ms(100);
//     }
//     // Show splash with O animation for 2 seconds

//     int line12_y_add = 0;
//     int line34_y_add = 0;

//     const int left = TABLE_LEFT;
//     const int right = TABLE_RIGHT;
//     const int top = TABLE_TOP;
//     const int bottom = table_bottom();

//     for (float i = 0; i < 1.01; i = i + 0.04)
//     {
//         // Clear display buffer
//         memset(display_buffer, 0, sizeof(display_buffer));

//         line12_y_add = (int)(easeInOutQuad(0, -18, i));
//         line34_y_add = (int)(easeInOutQuad(0, -31, i));

//         // Draw static text lines
//         int width1 = compact_text_width(line1, &Font16, -1);
//         int x1 = (DISPLAY_WIDTH - width1) / 2;
//         draw_string_compact(x1, y1 + line12_y_add, line1, &Font16, true, -1);

//         int width2 = compact_text_width(line2, &Font16, -1);
//         int x2 = (DISPLAY_WIDTH - width2) / 2;
//         draw_string_compact(x2, y2 + line12_y_add, line2, &Font16, true, -1);

//         int width3 = compact_text_width(line3, &Font12, 0);
//         int x3 = (DISPLAY_WIDTH - width3) / 2;
//         draw_string_compact(x3, y3 + line34_y_add, line3, &Font12, true, 0);

//         int width4 = compact_text_width(line4, &Font24, -3);
//         int x4 = (DISPLAY_WIDTH - width4) / 2;
//         draw_string_compact(x4, y4 + line34_y_add, line4, &Font24, true, -3);

//         fill_rect(left, top + line34_y_add + 31, right - left + 1, 1, true);
//         // fill_rect(left, row2_y() - TABLE_ROW_PADDING+ line34_y_add, right - left + 1, 1, true);
//         fill_rect(left, bottom + line34_y_add + 31, right - left + 1, 1, true);
//         fill_rect(left, top + line34_y_add + 31, 1, bottom - top + 1, true);
//         fill_rect(right, top + line34_y_add + 31, 1, bottom - top + 1, true);

//         // Update display
//         ssd1306_update();

//         // Watchdog check
//         bool gpio_state = gpio_get(SYS_MODE_PIN);
//         if (gpio_state != gpiosysmode_last_state)
//             watchdog_reboot(0, 0, 0);
//     }

//     for (int i = 0; i < 20; i++)
//     {
//         // Watchdog check
//         bool gpio_state = gpio_get(SYS_MODE_PIN);
//         if (gpio_state != gpiosysmode_last_state)
//             watchdog_reboot(0, 0, 0);
//         sleep_ms(100);
//     }
// }

// static void status_display_core(void)
// {


//       gpio_init(LED_PIN);
//     // 具体调节在 定时中断中
//     gpio_set_function(LED_PIN, GPIO_FUNC_PWM);
//     uint led_run_slice = pwm_gpio_to_slice_num(LED_PIN);
//     pwm_config led_run_cfg = pwm_get_default_config();
//     pwm_config_set_clkdiv(&led_run_cfg, 125.0f); // 125MHz / 125 = 1MHz
//     pwm_config_set_wrap(&led_run_cfg, 1000);     // 1MHz / 1000 = 1kHz
//     pwm_init(led_run_slice, &led_run_cfg, true);
//     pwm_set_gpio_level(LED_PIN, 0); // 默认关闭

//     gpio_init(LED_DEBUG);
//     gpio_set_dir(LED_DEBUG, GPIO_OUT);
//     gpio_put(LED_DEBUG, 1);

//     atomic_store_explicit(&led_mode, LED_OFF, memory_order_relaxed);
//     atomic_store_explicit(&led_display_signal, false, memory_order_relaxed);
//     current_brightness = 0;
//     // start 50ms repeating timer to update LED brightness
//     add_repeating_timer_ms(50, led_timer_callback, NULL, &led_timer);

//     gpio_init(SYS_MODE_PIN);
//     gpio_set_dir(SYS_MODE_PIN, GPIO_IN);
//     gpio_pull_up(SYS_MODE_PIN);
//     gpiosysmode_last_state = gpio_get(SYS_MODE_PIN);

//     // RES SSD1306
//     gpio_init(SSD_RES);
//     gpio_set_dir(SSD_RES, GPIO_OUT);
//     gpio_put(SSD_RES, 0);
//     sleep_ms(1);
//     gpio_put(SSD_RES, 1);
//     sleep_ms(1);

//     i2c_init(I2C_PORT, 1000000);
//     gpio_set_function(PICO_I2C_SDA_PIN, GPIO_FUNC_I2C);
//     gpio_set_function(PICO_I2C_SCL_PIN, GPIO_FUNC_I2C);
//     gpio_pull_up(PICO_I2C_SDA_PIN);
//     gpio_pull_up(PICO_I2C_SCL_PIN);

//     ssd1306_init();

//     // Show splash screen
//     show_splash_screen();

//     while (1)
//     {
//         bool gpio0_state = gpio_get(SYS_MODE_PIN);
//         if (gpio0_state != gpiosysmode_last_state)
//             watchdog_reboot(0, 0, 0);

//         render_buffer();
//         ssd1306_update();
//         // sleep_ms(STATUS_REFRESH_MS);
//     }
// }

// #include "board_pico_config.h"

// void status_display_start(void)
// {
//     gpio_pull_up(PROBE_UART_TX);
//     gpio_pull_up(PROBE_UART_RX);

//     gpio_init(PULLUP_EN_PIN);
//     gpio_set_dir(PULLUP_EN_PIN, GPIO_OUT);
//     gpio_put(PULLUP_EN_PIN, 1);

//     gpio_init(SELECTERPIN0);
//     gpio_set_dir(SELECTERPIN0, GPIO_OUT);
//     gpio_put(SELECTERPIN0, 1);

//     gpio_init(SELECTERPIN1);
//     gpio_set_dir(SELECTERPIN1, GPIO_OUT);
//     gpio_put(SELECTERPIN1, 0);

//     gpio_init(TEST_SPI_CS_PIN);
//     gpio_set_dir(TEST_SPI_CS_PIN, GPIO_IN);
//     gpio_disable_pulls(TEST_SPI_CS_PIN);

//     if (display_started)
//         return;
//     for (int i = 0; i < STATUS_DISPLAY_COUNT; i++)
//     {
//         atomic_store_explicit(&status_flags[i], false, memory_order_relaxed);
//         atomic_store_explicit(&status_pending[i], 0, memory_order_relaxed);
//     }
//     for (int i = 0; i < SLIDE_CAPACITY; i++)
//     {
//         tx_slides[i].active = false;
//         rx_slides[i].active = false;
//     }
//     debug_force_usb_disconnected = false;
//     debug_force_dap_disconnected = false;
//     atomic_store_explicit(&usb_status_hold_flag, false, memory_order_relaxed);
//     atomic_store_explicit(&dap_status_hold_flag, false, memory_order_relaxed);
//     display_started = true;
//     multicore_launch_core1(status_display_core);
// }

// void status_display_set(status_display_id_t id, bool active)
// {
//     if (id >= STATUS_DISPLAY_COUNT)
//         return;
//     atomic_store_explicit(&status_flags[id], active, memory_order_relaxed);
//     if (id == STATUS_DISPLAY_USB_CONNECTED && active)
//         status_display_request_usb_status_hold();
//     if (id == STATUS_DISPLAY_DAP_RUNNING && active)
//     {
//         status_display_request_dap_status_hold();
//         // also signal LED via display flag so timer sees the event
//         atomic_store_explicit(&led_display_signal, true, memory_order_relaxed);
//     }
//     if (active && (id == STATUS_DISPLAY_UART_RX || id == STATUS_DISPLAY_UART_TX))
//     {
//         atomic_fetch_add_explicit(&status_pending[id], 1, memory_order_relaxed);
//     }
// }

// void status_display_request_dap_status_hold(void)
// {
//     atomic_store_explicit(&dap_status_hold_flag, true, memory_order_relaxed);
// }

// void status_display_request_usb_status_hold(void)
// {
//     atomic_store_explicit(&usb_status_hold_flag, true, memory_order_relaxed);
// }

// void status_display_force_usb_disconnected(bool force)
// {
//     debug_force_usb_disconnected = force;
// }

// void status_display_force_dap_disconnected(bool force)
// {
//     debug_force_dap_disconnected = force;
// }
