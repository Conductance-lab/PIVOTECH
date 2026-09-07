#ifndef _LICENSE_CRYPTO_HPP_
#define _LICENSE_CRYPTO_HPP_

#include <stdint.h>

// 自动生成加密密文并写入 FLASH
void erase_license_data();
void encrypt_and_store_key();

// 验证授权状态
bool verify_authorization();

#endif // _LICENSE_CRYPTO_HPP_
