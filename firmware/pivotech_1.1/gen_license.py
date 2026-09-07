import sys
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.backends import default_backend

def generate_license(flash_id_hex, plaintext="AuthorizedDat"):
    # 1. 解析 Flash ID
    flash_id_hex = flash_id_hex.replace(" ", "").replace(":", "").replace("0x", "")
    if len(flash_id_hex) < 16:
        print("错误: Flash ID 必须至少为 8 个字节 (16 个十六进制字符)。")
        return
    try:
        flash_id_bytes = bytes.fromhex(flash_id_hex[:16])
    except ValueError:
        print("错误: Flash ID 包含无效的十六进制字符。")
        return

    # 2. Master Key（与 C 代码一致）
    master_key = b'P!V0T3cH_M@sT3rK'

    # 3. 生成 AES 密钥
    aes_key = bytearray(16)
    for i in range(8):
        aes_key[i] = master_key[i] ^ flash_id_bytes[i]
    for i in range(8, 16):
        aes_key[i] = master_key[i] ^ 0xAA
    aes_key = bytes(aes_key)

    # 4. 准备明文：必须包含 C 字符串末尾的 '\0'
    #    C 代码中: char plaintext[16]; memset(plaintext,0,16); strcpy(plaintext, LICENSE_PLAINTEXT);
    #    所以实际明文 = "AuthorizedDat\0" + 3 个零填充。
    plaintext_bytes = plaintext.encode('utf-8') + b'\x00'   # 加上字符串终结符
    if len(plaintext_bytes) > 16:
        plaintext_bytes = plaintext_bytes[:16]  # 最多 16 字节
    # 填充至 16 字节（用 0x00）
    plaintext_padded = plaintext_bytes.ljust(16, b'\x00')

    # 5. AES-128-CBC 加密，IV=全零
    iv = b'\x00' * 16
    cipher = Cipher(algorithms.AES(aes_key), modes.CBC(iv), backend=default_backend())
    encryptor = cipher.encryptor()
    ciphertext = encryptor.update(plaintext_padded) + encryptor.finalize()

    # 6. 输出结果
    hex_continuous = ciphertext.hex().upper()
    hex_spaced = " ".join(f"{b:02X}" for b in ciphertext)

    print("\n--- 加密结果 (与 MbedTLS 完全对齐) ---")
    print(f"Flash ID:        {flash_id_bytes.hex().upper()}")
    print(f"AES 密钥:        {aes_key.hex().upper()}")
    print(f"明文 (含'\\0'):  {plaintext_padded.hex().upper()} -> {repr(plaintext_padded)}")
    print(f"密文:            {hex_spaced}")
    print("\n============ 请使用串口发送以下指令 ============")
    print(f"{hex_continuous}\n")

if __name__ == "__main__":
    print("=== PIVOTECH 设备授权密钥生成工具 ===")
    fid = input("请输入设备的 Flash ID (如 e664b49507748e37): ").strip()
    if not fid:
        sys.exit(1)
    pt = input("请输入验证明文 (回车默认 'Sc5ta6X8ufrWn18'): ").strip()
    if not pt:
        pt = "Sc5ta6X8ufrWn18"
    generate_license(fid, pt)