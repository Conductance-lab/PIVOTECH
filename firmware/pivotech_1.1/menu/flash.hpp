#ifndef MENU_FLASH_HPP
#define MENU_FLASH_HPP

#include <stdint.h>
#include <stddef.h> // Fixed: Added for size_t

// Sizes exported from implementation for use by other modules
#define CONFIG_SETTINGS_MAX_LEN 32
#define CONFIG_DATA_MAX_LEN 10000 //////////////////////// 后续根据实际需求调整

#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024) 
#endif

// License related
#define FLASH_CONFIG_SETTINGS_OFFSET (PICO_FLASH_SIZE_BYTES - 32 * 1024 - 4 * 1024 - 4 * 1024) // -40KB
#define FLASH_CONFIG_DATA_OFFSET     (PICO_FLASH_SIZE_BYTES - 32 * 1024)                       // -32KB
#define FLASH_LICENSE_OFFSET         (PICO_FLASH_SIZE_BYTES - 32 * 1024 - 4 * 1024)            // -36KB

#define LICENSE_SIZE 256 // Matches a flash page, enough for RSA-2048 signature

// AI 测试代码分区（顶部，见 docs/AI测试模式设计方案.md v0.3 §5.7）
#define FLASH_AI_CODE_OFFSET (PICO_FLASH_SIZE_BYTES - 20 * 1024) // 2MB-20KB = 0x1FB000
#define AI_CODE_MAX_LEN      8192                                // 脚本长度上限（字符，含头部）
#define AI_CODE_REGION       (20 * 1024)                         // 顶部预留 20KB（5 扇区）
#define AI_CODE_SAVE_SIZE    (12 * 1024)                         // 实际擦写 12KB（3 扇区，含头 + ≤8KB 脚本）

void save_ai_code(const char* src);
void load_ai_code(char* dst);
void erase_ai_code(void);
bool has_ai_code(void);

void save_configsettings(const char* src);
void save_configdata(const char* src);
void load_configsettings_to_sram(char* dest_buffer);
void load_configdata_to_sram(char* dest_buffer);

// New licensing functions
// bool verify_license(void);
void save_license_data(const uint8_t* data, size_t len);
void load_license_data(uint8_t* dest, size_t len);

#endif // MENU_FLASH_HPP