#ifndef _FLASH_CPP_
    #define _FLASH_CPP_

#include "hardware/flash.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "hardware/sync.h"
#include "CONFIG_FLO.hpp" // Added to update system state
#include <string.h>
#include <string>
#include <cstring>
#include "hardware/xip_cache.h"
#include "menu/flash.hpp"


#define XIP_BASE 0x10000000  // Flash 内存映射基地址

// Save configsettings（从 RAM 到 Flash）
void save_configsettings(const char* src) {
    uint8_t buffer[FLASH_SECTOR_SIZE] __attribute__((aligned(256)));
    memset(buffer, 0xFF, sizeof(buffer)); // 初始化为擦除状态
    const size_t src_len = strnlen(src, CONFIG_SETTINGS_MAX_LEN - 1);
    const size_t copy_len = src_len + 1; // 包含字符串终止符
    memcpy(buffer, src, copy_len);

    // 擦除并写入 Flash
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_CONFIG_SETTINGS_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_CONFIG_SETTINGS_OFFSET, buffer, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
}



// 保存 config（从 RAM 到 Flash，分段写入）
void save_configdata(const char* src) {
    const uint32_t sectors_needed = (CONFIG_DATA_MAX_LEN + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE;
    const size_t src_len = strnlen(src, CONFIG_DATA_MAX_LEN - 1);
    const size_t total_len = src_len + 1; // 包含字符串终止符
    uint8_t buffer[FLASH_SECTOR_SIZE] __attribute__((aligned(256)));

    for (uint32_t i = 0; i < sectors_needed; i++) {
        uint32_t offset = FLASH_CONFIG_DATA_OFFSET + i * FLASH_SECTOR_SIZE;
        uint32_t data_offset = i * FLASH_SECTOR_SIZE;

        memset(buffer, 0xFF, FLASH_SECTOR_SIZE); // 初始化为擦除状态
        if (data_offset < total_len) {
            size_t chunk = total_len - data_offset;
            if (chunk > FLASH_SECTOR_SIZE) {
                chunk = FLASH_SECTOR_SIZE;
            }
            memcpy(buffer, src + data_offset, chunk);
        }

        uint32_t ints = save_and_disable_interrupts();
        flash_range_erase(offset, FLASH_SECTOR_SIZE);
        flash_range_program(offset, buffer, FLASH_SECTOR_SIZE);
        restore_interrupts(ints);
    }
}



// 从 Flash 加载 configsettings 到 SRAM 缓冲区
void load_configsettings_to_sram(char* dest_buffer) {
    // 直接通过 XIP 地址访问 Flash
    const uint8_t* flash_src = (const uint8_t*)(XIP_BASE + FLASH_CONFIG_SETTINGS_OFFSET);
    // 复制数据到 SRAM
    memcpy(dest_buffer, flash_src, CONFIG_SETTINGS_MAX_LEN);
}


// 从 Flash 加载 config 到 SRAM 缓冲区
void load_configdata_to_sram(char* dest_buffer) {
    // 直接访问 Flash 地址
    const uint8_t* flash_src = (const uint8_t*)(XIP_BASE + FLASH_CONFIG_DATA_OFFSET);
    
    // 复制数据到 SRAM
    memcpy(dest_buffer, flash_src, CONFIG_DATA_MAX_LEN);

}


extern pico_unique_board_id_t id;
// 获取 Flash 唯一 ID
// 这里返回前 8 字节 ID，和 AES 密钥生成时使用的 8 字节长度一致。
// 如果设备返回的 ID 更长，则只截取前 8 字节；如果更短则不会发生，因为 pico_get_unique_board_id 保证 8 字节长度。
// void get_flash_unique_id(uint8_t id_out[8]) {
    
//     pico_get_unique_board_id(&id);
//     memcpy(id_out, id.id, 8);
// }

// 将 license 数据写入 Flash（数据长度应 ≤ FLASH_SECTOR_SIZE）
// 内部固定使用 4KB 扇区操作，与 save_configsettings 完全一致
void save_license_data(const uint8_t* src, size_t len) {
    if (len > FLASH_SECTOR_SIZE) {
        len = FLASH_SECTOR_SIZE;   // 截断，避免溢出
    }

    uint8_t buffer[FLASH_SECTOR_SIZE] __attribute__((aligned(256)));
    memset(buffer, 0xFF, sizeof(buffer));   // 初始化为擦除状态
    memcpy(buffer, src, len);               // 复制有效数据

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_LICENSE_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_LICENSE_OFFSET, buffer, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
}

// 从 Flash 加载 license 数据到 SRAM
void load_license_data(uint8_t* dest, size_t len) {
    if (len > FLASH_SECTOR_SIZE) {
        len = FLASH_SECTOR_SIZE;
    }
    const uint8_t* flash_src = (const uint8_t*)(XIP_BASE + FLASH_LICENSE_OFFSET);
    memcpy(dest, flash_src, len);
}

// ===================== AI 测试代码分区（顶部 2MB-20KB 起） =====================
// 布局: [0..3]='AIC1' 魔数, [4..5]=代码长度(u16 LE), [6..7]保留, [8..]=脚本 UTF-8

static const char AI_MAGIC[4] = {'A', 'I', 'C', '1'};

bool has_ai_code(void)
{
    const uint8_t* p = (const uint8_t*)(XIP_BASE + FLASH_AI_CODE_OFFSET);
    return (p[0] == (uint8_t)AI_MAGIC[0] && p[1] == (uint8_t)AI_MAGIC[1] &&
            p[2] == (uint8_t)AI_MAGIC[2] && p[3] == (uint8_t)AI_MAGIC[3] &&
            p[4] != 0xFF);
}

void save_ai_code(const char* src)
{
    if (!src) return;
    const size_t len = strnlen(src, AI_CODE_MAX_LEN);
    if (len == 0) return;

    static uint8_t buffer[AI_CODE_SAVE_SIZE] __attribute__((aligned(256)));
    memset(buffer, 0xFF, sizeof(buffer));
    memcpy(buffer, AI_MAGIC, 4);
    buffer[4] = (uint8_t)(len & 0xFF);
    buffer[5] = (uint8_t)((len >> 8) & 0xFF);
    memcpy(buffer + 8, src, len);

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_AI_CODE_OFFSET, AI_CODE_SAVE_SIZE);
    flash_range_program(FLASH_AI_CODE_OFFSET, buffer, AI_CODE_SAVE_SIZE);
    restore_interrupts(ints);
}

void load_ai_code(char* dst)
{
    if (!dst) return;
    const uint8_t* p = (const uint8_t*)(XIP_BASE + FLASH_AI_CODE_OFFSET);
    size_t len = (size_t)p[4] | ((size_t)p[5] << 8);
    if (len > AI_CODE_MAX_LEN) len = 0;
    memcpy(dst, p + 8, len);
    dst[len] = '\0';
}

void erase_ai_code(void)
{
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_AI_CODE_OFFSET, AI_CODE_SAVE_SIZE);
    restore_interrupts(ints);
}


#endif
