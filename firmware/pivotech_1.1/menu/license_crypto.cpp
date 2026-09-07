#include "menu/license_crypto.hpp"
#include "menu/flash.hpp"
#include "CONFIG_FLO.hpp"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "mbedtls/aes.h"

#define XIP_BASE_ADDR 0x10000000  // Flash 内存映射基地址

#include "hardware/flash.h"  // 提供 flash_get_unique_id
#include "pico/stdlib.h"

// 声明外部全局变量（在 CONFIG_FLO.cpp 中定义）
extern uint8_t stored_flash_id[8];
extern uint8_t stored_license_data[16];
extern bool license_data_loaded;
///////////////////////////////////////////////////////////////////////////////////////

// 自动生成加密密文并写入 FLASH (在 FLASH_LICENSE_OFFSET)
// 尽管正式程序不适用该逻辑 但仍保持更改以便后续设计自动授权程序
// 清除授权信息：将 FLASH 中授权区域写满 0xFF
void erase_license_data() {
    uint8_t erased_data[16];
    memset(erased_data, 0xFF, sizeof(erased_data));
    save_license_data(erased_data, sizeof(erased_data));
    printf("[ERASE] License data erased (all 0xFF).\r\n");
}

// 原有的加密写入函数，保持不变
void encrypt_and_store_key() {
    uint8_t flash_id[8];
    flash_get_unique_id(flash_id);
    memcpy(stored_flash_id, flash_id, 8);

    const uint8_t master_key[16] = {
        'P', '!', 'V', '0', 'T', '3', 'c', 'H',
        '_', 'M', '@', 's', 'T', '3', 'r', 'K'
    };

    uint8_t aes_key[16];
    for (int i = 0; i < 8; i++) {
        aes_key[i] = master_key[i] ^ flash_id[i];
    }
    for (int i = 8; i < 16; i++) {
        aes_key[i] = master_key[i] ^ 0xAA;
    }

    uint8_t plaintext[16];
    memset(plaintext, 0, sizeof(plaintext));
    strcpy((char*)plaintext, LICENSE_PLAINTEXT);

    uint8_t encrypted_data[16];
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, aes_key, 128);
    uint8_t iv[16] = {0};
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, sizeof(plaintext), iv, plaintext, encrypted_data);
    mbedtls_aes_free(&aes);

    save_license_data(encrypted_data, sizeof(encrypted_data));

    printf("[ENCRYPT] License encrypted and stored successfully.\r\n");
}
///////////////////////////////////////////////////////////////////////////////////////

extern bool is_device_licensed; // 从 CONFIG_FLO.cpp 引入授权状态变量
extern uint8_t stored_license_data[16]; // 从 CONFIG_FLO.cpp 引入授权数据缓冲区
extern bool license_data_loaded; // 从 CONFIG_FLO.cpp 引入授权数据加载标志
#include "pico/multicore.h"
#include "hardware/flash.h"
// 验证授权状态并返回结果

extern uint8_t stored_flash_id[8]; // 从 CONFIG_FLO.cpp 引入 Flash ID 缓冲区
bool verify_authorization() {
    uint8_t aes_key[16];
    uint8_t decrypted_data[16];

    // 如果 license_data_loaded 为 true，说明刚更新了内存中的授权数据，需重新写入 Flash
    if (license_data_loaded) {
        license_data_loaded = false;
        multicore_reset_core1();
        save_license_data(stored_license_data, 16);
        #include "menu/core.hpp"
        multicore_launch_core1(core1_main);
        // printf("[AUTH] License data re-saved to flash.\r\n");
    }

    // 调试：打印存储的密文（从 Flash 加载到全局变量 stored_license_data）
    // printf("[AUTH] Stored License Data (ciphertext): ");
    // for (int i = 0; i < 16; i++) {
    //     printf("%02x ", stored_license_data[i]);
    // }
    // printf("\r\n");

    // 混淆的 Master Key ('P!V0T3cH_M@sT3rK' XOR 0x9D)
    const uint8_t obfuscated_master_key[16] = {
        'P'^0x9D, '!'^0x9D, 'V'^0x9D, '0'^0x9D,
        'T'^0x9D, '3'^0x9D, 'c'^0x9D, 'H'^0x9D,
        '_'^0x9D, 'M'^0x9D, '@'^0x9D, 's'^0x9D,
        'T'^0x9D, '3'^0x9D, 'r'^0x9D, 'K'^0x9D
    };

    // 生成 AES 密钥：前 8 字节与 Flash ID 异或，后 8 字节与 0xAA 异或
    for (int i = 0; i < 8; i++) {
        aes_key[i] = (obfuscated_master_key[i] ^ 0x9D) ^ stored_flash_id[i];
    }
    for (int i = 8; i < 16; i++) {
        aes_key[i] = (obfuscated_master_key[i] ^ 0x9D) ^ 0xAA;
    }

    // 解密存储的密文（直接使用全局 stored_license_data）
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, aes_key, 128);
    uint8_t iv[16] = {0};  // 全零 IV
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, 16, iv, stored_license_data, decrypted_data);
    mbedtls_aes_free(&aes);

    // 构建期望的明文（与加密时完全一致）
    uint8_t expected_pt[16] = {0};
    strncpy((char*)expected_pt, LICENSE_PLAINTEXT, 15);  // LICENSE_PLAINTEXT = "AuthorizedDat"

    // 比较解密结果
    if (memcmp(decrypted_data, expected_pt, 16) == 0) {
        is_device_licensed = true;
        // printf("[AUTH] Authorization SUCCESS.\r\n");
        return true;
    } else {
        is_device_licensed = false;
        // printf("[AUTH] Authorization FAILED.\r\n");
        return false;
    }
}