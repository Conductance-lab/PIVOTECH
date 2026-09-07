/**
 * 基于 V. Hunter Adams 的 Bootloader 简化
 * 仅包含跳转逻辑
 */

#include "pico/stdlib.h"
#include "hardware/regs/m0plus.h" // 包含 M0PLUS_VTOR_OFFSET 定义
#include "hardware/structs/scb.h"

// 定义 GPIO 引脚号
#define SYS_MODE_PIN 29

// 应用程序在 Flash 中的偏移地址 (12 * 1024 bits = 12KB)
// 必须与目标应用程序 Linker Script 中的 Flash 起始地址一致
#define PROGRAM_OFFSET 12*1024

// 定义另一个程序的偏移地址 (1024 * 1024 bits = 1MB)
#define PROGRAM_OFFSET_ALT 1800*1024

// 清理环境并跳转的核心汇编函数
// 设置 VTOR 寄存器，设置堆栈指针 (MSP)，并跳转到应用程序的复位向量
static inline void handleBranch(uint32_t offset) {

    // 1. 屏蔽所有中断 (NVIC ICER, NVIC ICPR)
    // 在跳转前必须确保没有中断处于 pending 或 active 状态，否则会导致 HardFault
    hw_set_bits((io_rw_32 *)0xe000e180, 0xFFFFFFFF);
    hw_set_bits((io_rw_32 *)0xe000e280, 0xFFFFFFFF);
    
    // (可选) 如果启用了 SysTick，也应该在这里禁用
    // SysTick->CTRL &= ~1;

    // 2. 执行汇编跳转
    // r0 = 目标程序的起始地址 (XIP_BASE + OFFSET)
    // r1 = VTOR 寄存器的地址
    asm volatile (
    "mov r0, %[start]\n"            // 将新程序起始地址放入 r0
    "ldr r1, =%[vtable]\n"          // 将 VTOR 寄存器地址放入 r1
    "str r0, [r1]\n"                // 将 r0 的值写入 VTOR (重定向中断向量表)
    "ldmia r0, {r0, r1}\n"          // 从新程序起始位置加载前两个字：
                                    // 第1个字是栈顶地址 (MSP) -> 存入 r0
                                    // 第2个字是复位中断入口 (Reset Handler) -> 存入 r1
    "msr msp, r0\n"                 // 将 r0 的值设置为主堆栈指针 (MSP)
    "bx r1\n"                       // 跳转执行 r1 中的地址 (进入新程序)
    :
    : [start] "r" (XIP_BASE + offset), [vtable] "X" (PPB_BASE + M0PLUS_VTOR_OFFSET)
    :
    );
}

int main() {
    // 初始化 GPIO SYS_MODE_PIN
    gpio_init(SYS_MODE_PIN);
    gpio_set_dir(SYS_MODE_PIN, GPIO_IN);
    gpio_pull_up(SYS_MODE_PIN); // 启用上拉电阻

    sleep_ms(100); // 等待 GPIO 状态稳定

    // 读取 GPIO 0 的电平状态
    bool gpio_state = gpio_get(SYS_MODE_PIN); // SYS_MODE_PIN

    // 根据 GPIO 0 的状态跳转到不同的程序
    if (!gpio_state) {
                handleBranch(PROGRAM_OFFSET_ALT); // 跳转到 1MB

    } else {
                handleBranch(PROGRAM_OFFSET); // 跳转到 12KB

    }

    // 理论上程序永远不会运行到这里
    while (true) {
        tight_loop_contents();
    }
}