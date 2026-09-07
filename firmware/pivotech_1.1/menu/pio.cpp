#ifndef _PIO_CPP_
#define _PIO_CPP_

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include <stdio.h>

#include "CONFIG_FLO.hpp"

#define COUNT_PIN_A TAINPIN
#define COUNT_PIN_B TBINPIN

// Persistent state for PIO high-speed frequency measurement
static PIO pio_inst = pio0;
static uint sm_a_idx = 0;
static uint sm_b_idx = 0;
static uint offset_a = 0;
static uint offset_b = 0;
static uint32_t edge_count_a = 0;
static uint32_t edge_count_b = 0;
static struct repeating_timer pio_timer;

extern uint32_t pio_ta_count_frec;
extern uint32_t pio_tb_count_frec;

// 定时器回调函数：每 200ms 触发一次
bool pio_timer_callback(struct repeating_timer *t)
{
	// 1. 暂时停止状态机，确保在提取 X 寄存器值时 X 不会发生变化
	pio_inst->ctrl = (pio_inst->ctrl & ~(1u << sm_a_idx)) & ~(1u << sm_b_idx);

	// 2. 通过执行指令将 X 寄存器的值推送到 FIFO
	pio_sm_exec(pio_inst, sm_a_idx, pio_encode_mov(pio_isr, pio_x));
	pio_sm_exec(pio_inst, sm_a_idx, pio_encode_push(false, false));
	
	pio_sm_exec(pio_inst, sm_b_idx, pio_encode_mov(pio_isr, pio_x));
	pio_sm_exec(pio_inst, sm_b_idx, pio_encode_push(false, false));

	// 3. 读取 FIFO 中的值
	uint32_t val_a = pio_sm_get(pio_inst, sm_a_idx);
	uint32_t val_b = pio_sm_get(pio_inst, sm_b_idx);

	uint32_t count_a = 0xFFFFFFFF - val_a;
	uint32_t count_b = 0xFFFFFFFF - val_b;

	// 4. 重置 X 寄存器并清理 FIFO
	pio_sm_exec(pio_inst, sm_a_idx, pio_encode_mov(pio_x, pio_null));
	pio_sm_exec(pio_inst, sm_a_idx, pio_encode_mov_not(pio_x, pio_x));
	pio_sm_clear_fifos(pio_inst, sm_a_idx);
	
	pio_sm_exec(pio_inst, sm_b_idx, pio_encode_mov(pio_x, pio_null));
	pio_sm_exec(pio_inst, sm_b_idx, pio_encode_mov_not(pio_x, pio_x));
	pio_sm_clear_fifos(pio_inst, sm_b_idx);

	// 5. 原子级同步启动两个状态机
	pio_inst->ctrl |= (1u << sm_a_idx) | (1u << sm_b_idx);

	// 6. 更新全局频率值
	pio_ta_count_frec = count_a;
	pio_tb_count_frec = count_b;

	return true; // 继续重复定时器
}

// PIO 程序：利用 X 寄存器进行 32 位计数。
// 程序执行逻辑：
// 1. wait 0 gpio pin -> 等待低电平
// 2. wait 1 gpio pin -> 等待高电平（捕捉上升沿）
// 3. jmp x-- 0 -> 递减 X 并跳转回第 1 步循环计数
// 注意：X 是递减的，读取到的值需要取反 (0xFFFFFFFF - X) 得到实际脉冲数。

// A 通道和 B 通道共享同一个 PIO 程序，因为逻辑是一样的。
// 只需要在 init 时分别为不同的 SM 指定不同的引脚即可。
static uint16_t pin_counter_program_instructions[3];

static struct pio_program pin_counter_program = {
	.instructions = pin_counter_program_instructions,
	.length = 3,
	.origin = -1,
};

static void fill_pin_counter_program(uint pin, uint16_t *inst_array)
{
	// 使用 .wrap_target 和 .wrap 逻辑的等效指令
	// 1. wait 0 pin 0 [1] : 等待低电平，并增加 1 周期延迟用于滤除可能的细微毛刺
	// 2. wait 1 pin 0     : 等待高电平（上升沿）
	// 3. jmp x-- 0        : 递减 X 并跳回指令 0
	inst_array[0] = pio_encode_wait_pin(false, 0) | pio_encode_delay(1); 
	inst_array[1] = pio_encode_wait_pin(true, 0);  
	inst_array[2] = pio_encode_jmp_x_dec(0);       
}

static inline void pin_counter_program_init(PIO pio, uint sm, uint offset, uint pin)
{
	pio_gpio_init(pio, pin);
	pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, false);
	gpio_pull_down(pin);

	pio_sm_config c = pio_get_default_sm_config();
	// 设置 IN 引脚和 WAIT 引脚的基础 GPIO
	sm_config_set_in_pins(&c, pin);
	
	sm_config_set_in_shift(&c, true, false, 32);
	sm_config_set_clkdiv(&c, 1.0f);

	pio_sm_init(pio, sm, offset, &c);
	// 不要在这里启用启动，由 init 函数统一管理状态
	pio_sm_set_enabled(pio, sm, false);
}

int pio_highspeedfreq_init(void)
{
	// setup_pwm();

	// Configure PIO and claim two state machines
	pio_inst = pio0;
	sm_a_idx = pio_claim_unused_sm(pio_inst, true);
	sm_b_idx = pio_claim_unused_sm(pio_inst, true);

	// 填充程序逻辑（使用相对引脚 0）
	fill_pin_counter_program(0, pin_counter_program_instructions);

	// 将程序加载到 PIO 内存（两个 SM 可以共享同一个程序地址偏移）
	offset_a = pio_add_program(pio_inst, &pin_counter_program);
	offset_b = offset_a; // 共享同一个程序空间

	// Initialize the two state machines
	// 这里不再一初始化就启动 SM
	pin_counter_program_init(pio_inst, sm_a_idx, offset_a, COUNT_PIN_A);
	pin_counter_program_init(pio_inst, sm_b_idx, offset_b, COUNT_PIN_B);

	// 先清理 FIFO
	pio_sm_clear_fifos(pio_inst, sm_a_idx);
	pio_sm_clear_fifos(pio_inst, sm_b_idx);

	// 在 SM 停止的情况下，初始将 X 寄存器设置为 0xFFFFFFFF
	pio_sm_exec(pio_inst, sm_a_idx, pio_encode_mov(pio_x, pio_null));
	pio_sm_exec(pio_inst, sm_a_idx, pio_encode_mov_not(pio_x, pio_x));
	pio_sm_exec(pio_inst, sm_b_idx, pio_encode_mov(pio_x, pio_null));
	pio_sm_exec(pio_inst, sm_b_idx, pio_encode_mov_not(pio_x, pio_x));

	// 最后启动 SM
	pio_sm_set_enabled(pio_inst, sm_a_idx, true);
	pio_sm_set_enabled(pio_inst, sm_b_idx, true);

	// 设置 200ms 定时中断读取 PIO
	add_repeating_timer_ms(-200, pio_timer_callback, NULL, &pio_timer);

	// Reset counters and timing
	edge_count_a = 0;
	edge_count_b = 0;

	gpio_pull_up(COUNT_PIN_A);
	gpio_pull_up(COUNT_PIN_B);

	return 0;
}


void pio_highspeedfreq_deinit(void)
{
	// 取消定时器
	cancel_repeating_timer(&pio_timer);

	// Disable state machines and clear FIFOs
	pio_sm_set_enabled(pio_inst, sm_a_idx, false);
	pio_sm_set_enabled(pio_inst, sm_b_idx, false);
	pio_sm_clear_fifos(pio_inst, sm_a_idx);
	pio_sm_clear_fifos(pio_inst, sm_b_idx);

	// Unclaim SMs
	pio_sm_unclaim(pio_inst, sm_a_idx);
	pio_sm_unclaim(pio_inst, sm_b_idx);

	// Remove loaded programs
	pio_remove_program(pio_inst, &pin_counter_program, offset_a);

	// Reset counted GPIOs to SIO input and disable pulls
	// gpio_disable_pulls(COUNT_PIN_A);
	// gpio_disable_pulls(COUNT_PIN_B);
	gpio_set_function(COUNT_PIN_A, GPIO_FUNC_SIO);
	gpio_set_function(COUNT_PIN_B, GPIO_FUNC_SIO);
	gpio_set_dir(COUNT_PIN_A, false);
	gpio_set_dir(COUNT_PIN_B, false);

	// // Stop PWM and return pin to SIO input
	// uint slice = pwm_gpio_to_slice_num(PWM_PIN);
	// uint channel = pwm_gpio_to_channel(PWM_PIN);
	// pwm_set_enabled(slice, false);
	// pwm_set_chan_level(slice, channel, 0);
	// gpio_set_function(PWM_PIN, GPIO_FUNC_SIO);
	// gpio_set_dir(PWM_PIN, false);

	// Reset internal state
	edge_count_a = 0;
	edge_count_b = 0;
	sm_a_idx = 0;
	sm_b_idx = 0;
	offset_a = 0;
	offset_b = 0;
}

#endif