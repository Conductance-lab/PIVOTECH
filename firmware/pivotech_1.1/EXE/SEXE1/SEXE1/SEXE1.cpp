#include <Windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <io.h>
#include <fcntl.h>
#include <chrono>
#include <thread>

#pragma comment(lib, "setupapi.lib")

using namespace std;

// 设置控制台为UTF-8编码
void SetConsoleToUTF8() {
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

// 检查剪贴板内容是否以指定字符串开头
bool IsClipboardContentStartsWith(const wstring& prefix) {
    if (!OpenClipboard(nullptr)) {
        return false;
    }

    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData == nullptr) {
        CloseClipboard();
        return false;
    }

    wchar_t* pszText = static_cast<wchar_t*>(GlobalLock(hData));
    if (pszText == nullptr) {
        CloseClipboard();
        return false;
    }

    wstring clipboardText(pszText);
    GlobalUnlock(hData);
    CloseClipboard();

    return clipboardText.rfind(prefix, 0) == 0;
}

vector<pair<wstring, wstring>> GetAvailableComPorts() {
    vector<pair<wstring, wstring>> ports;

    HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVCLASS_PORTS, 0, 0, DIGCF_PRESENT);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        wcerr << L"[错误] 获取设备信息失败" << endl;
        return ports;
    }

    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &deviceInfoData); i++) {
        // 获取友好名称
        wchar_t friendlyName[256] = { 0 };
        if (!SetupDiGetDeviceRegistryPropertyW(hDevInfo, &deviceInfoData,
            SPDRP_FRIENDLYNAME, NULL,
            (PBYTE)friendlyName, sizeof(friendlyName) - 1,
            NULL)) {
            wcscpy_s(friendlyName, L"未知设备");
        }

        // 获取端口名称
        HKEY hDeviceKey = SetupDiOpenDevRegKey(hDevInfo, &deviceInfoData,
            DICS_FLAG_GLOBAL, 0,
            DIREG_DEV, KEY_READ);
        if (hDeviceKey != INVALID_HANDLE_VALUE) {
            wchar_t portName[256] = { 0 };
            DWORD size = sizeof(portName);
            DWORD type = 0;

            if (RegQueryValueExW(hDeviceKey, L"PortName", NULL, &type,
                (LPBYTE)portName, &size) == ERROR_SUCCESS &&
                type == REG_SZ) {
                // 过滤非COM端口
                if (wcsncmp(portName, L"COM", 3) == 0) {
                    ports.emplace_back(portName, friendlyName);
                }
            }
            RegCloseKey(hDeviceKey);
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);

    // 按COM号排序
    sort(ports.begin(), ports.end(), [](const auto& a, const auto& b) {
        return stoi(a.first.substr(3)) < stoi(b.first.substr(3));
        });

    return ports;
}

HANDLE ConfigureSerialPort(const wstring& portName) {
    wstring fullName = L"\\\\.\\" + portName;
    HANDLE hSerial = CreateFileW(fullName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if (hSerial == INVALID_HANDLE_VALUE) {
        wcerr << L"[错误] 打开端口失败 (错误代码: " << GetLastError() << L")" << endl;
        return nullptr;
    }

    DCB dcb = { 0 };
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(hSerial, &dcb)) {
        wcerr << L"[错误] 获取端口状态失败 (错误代码: " << GetLastError() << L")" << endl;
        CloseHandle(hSerial);
        return nullptr;
    }

    // 完整DCB配置
    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;

    if (!SetCommState(hSerial, &dcb)) {
        wcerr << L"[错误] 设置端口状态失败 (错误代码: " << GetLastError() << L")" << endl;
        CloseHandle(hSerial);
        return nullptr;
    }

    // 更合理的超时设置
    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 1000;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 1000;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(hSerial, &timeouts)) {
        wcerr << L"[错误] 设置端口超时失败 (错误代码: " << GetLastError() << L")" << endl;
        CloseHandle(hSerial);
        return nullptr;
    }

    // 清空缓冲区
    PurgeComm(hSerial, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return hSerial;
}

// 读取串口返回数据（严格2秒超时）
wstring ReadSerialData(HANDLE hSerial, int timeoutMs = 2000) {
    wstring result;
    vector<char> buffer(1024);
    DWORD bytesRead = 0;

    auto startTime = chrono::steady_clock::now();

    while (true) {
        // 检查是否超时
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(
            chrono::steady_clock::now() - startTime).count();
        if (elapsed >= timeoutMs) {

            break;
        }

        // 尝试读取数据
        if (ReadFile(hSerial, buffer.data(), buffer.size(), &bytesRead, nullptr)) {
            if (bytesRead > 0) {
                // 将读取的数据转换为宽字符串
                int wideLen = MultiByteToWideChar(CP_UTF8, 0,
                    buffer.data(), bytesRead, nullptr, 0);
                wstring wideStr(wideLen, L'\0');
                MultiByteToWideChar(CP_UTF8, 0,
                    buffer.data(), bytesRead, &wideStr[0], wideLen);

                result += wideStr;

                // 重置超时计时器（可选，如果希望每次收到数据后重置超时）
                // startTime = chrono::steady_clock::now();
            }
        }
        else {
            DWORD err = GetLastError();
            if (err != ERROR_IO_PENDING) {
                wcerr << L"\n[错误] 读取串口数据失败 (错误代码: " << err << L")" << endl;
            }
            break;
        }

        // 短暂休眠避免CPU占用过高
        this_thread::sleep_for(chrono::milliseconds(10));
    }

    return result;
}

// 新增：安全发送数据函数（分包发送）
bool SafeSerialWrite(HANDLE hSerial, const vector<BYTE>& data) {
    const size_t PACKET_SIZE = 200;  // 每个包最大200字节
    size_t totalSent = 0;

    while (totalSent < data.size()) {
        size_t remaining = data.size() - totalSent;
        size_t toSend = min(PACKET_SIZE, remaining);

        DWORD bytesWritten = 0;
        if (!WriteFile(hSerial, &data[totalSent], toSend, &bytesWritten, nullptr)) {
            wcerr << L"\n[错误] 数据发送失败! 错误代码: " << GetLastError() << endl;
            return false;
        }

        if (bytesWritten != toSend) {
            wcerr << L"\n[警告] 部分数据发送不完整 ("
                << bytesWritten << L"/" << toSend << L" 字节)" << endl;
        }

        totalSent += bytesWritten;

        // 添加延迟避免堵塞
        if (totalSent < data.size()) {
            this_thread::sleep_for(chrono::milliseconds(50));
        }
    }
    return true;
}

int main() {
    // 设置控制台编码为UTF-8
    SetConsoleToUTF8();

    // 程序标题
    wcout << L"\n================= SGate MMC =================";

    wcout << L"\n          SGate 配置工具 Cfg.v.1.0";
    wcout << L"\n                       电导不是韩导 @bilibili";

    while (true) {
        // 获取可用COM端口及设备信息
        wcout << L"\n---------------------------------------------";
        wcout << L"\n检测到以下可用端口:\n";
        auto comPorts = GetAvailableComPorts();
        for (const auto& port : comPorts) {
            wcout << L"  " << port.first << L" - " << port.second << L"\n";
        }
        if (comPorts.empty()) {
            wcerr << L"\n未检测到任何可用端口，请检查设备连接！" << endl;
            return 1;
        }
        wcout << L"\n通常SGate设备显示为'USB 串行设备'，若未找到可输入 0 重新扫描端口；";
        int comNumber;
        wstring textData;

        // 检查剪贴板是否包含#SGateConfig
        if (IsClipboardContentStartsWith(L"#SGateConfig")) {
            // 自动模式
            wcout << L"\n[成功] 检测到剪贴板包含SGate配置内容，已导入；";
            wcout << L"\n[输入] 请输入端口号，例如: 3 表示COM3:  ";
            wcin >> comNumber;

            // 如果输入0，则重新扫描端口
            if (comNumber == 0) {
                wcout << L"\n重新扫描端口和读取剪切板...\n";
                continue;
            }

            // 获取剪贴板内容
            if (!OpenClipboard(nullptr)) {
                wcerr << L"[错误] 无法访问剪贴板" << endl;
                return 1;
            }

            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData == nullptr) {
                CloseClipboard();
                wcerr << L"[错误] 无法读取剪贴板数据" << endl;
                return 1;
            }

            wchar_t* pszText = static_cast<wchar_t*>(GlobalLock(hData));
            if (pszText == nullptr) {
                CloseClipboard();
                wcerr << L"[错误] 无法锁定剪贴板数据" << endl;
                return 1;
            }

            textData = wstring(pszText);
            GlobalUnlock(hData);
            CloseClipboard();

            wcout << L"\n已自动加载剪贴板配置内容";
        }
        else {
            // 手动输入模式
            wcout << L"\n[提示] 未检测到剪贴板配置，需手动粘贴SGate配置内容，也可输入 0 尝试重新读取剪切板；";
            wcout << L"\n[输入] 请输入端口号，例如: 输入 3 表示COM3:  ";
            wcin >> comNumber;

            // 如果输入0，则重新扫描端口
            if (comNumber == 0) {
                wcout << L"\n重新扫描端口和读取剪切板...\n";
                continue;
            }

            wcin.ignore(); // 清除输入缓冲区
            wcout << L"[输入] 请输入SGate配置命令: ";
            getline(wcin, textData);
        }

        wstring comName = L"COM" + to_wstring(comNumber);

        // 验证端口存在
        bool validPort = any_of(comPorts.begin(), comPorts.end(),
            [&comName](const auto& port) { return port.first == comName; });

        if (!validPort) {
            wcerr << L"\n[错误] 无效的端口号，请重新输入！" << endl;
            continue;
        }

        // 转换数据
        vector<BYTE> sendData(textData.begin(), textData.end());
        if (sendData.empty()) {
            wcerr << L"\n[错误] 配置内容不能为空！" << endl;
            continue;
        }

        // 配置并打开端口
        wcout << L"\n正在尝试打开端口 " << comName << L" ...";
        HANDLE hSerial = ConfigureSerialPort(comName);
        if (!hSerial) {
            wcerr << L"\n端口初始化失败，请检查端口是否被占用！" << endl;
            continue;
        }

        // 发送数据
        wcout << L"\n正在发送配置数据(" << sendData.size() << L" 字节)...";
        if (SafeSerialWrite(hSerial, sendData)) {
            wcout << L"\n[成功] 所有配置数据已发送完成" << endl;

            // 实时读取响应数据
            wcout << L"\n正在等待设备响应（最多等待30秒）...\n";
            wstring response;
            const wstring endFlag = L"reboot the device.";
            bool receivedComplete = false;
            auto startTime = chrono::steady_clock::now();

            // 实时读取循环
            while (true) {
                // 检查超时
                auto elapsed = chrono::duration_cast<chrono::milliseconds>(
                    chrono::steady_clock::now() - startTime).count();
                if (elapsed >= 30000) { //超过十秒强制退出
                    break;
                }

                // 尝试读取数据
                char rawBuffer[65536];
                DWORD bytesRead = 0;
                if (ReadFile(hSerial, rawBuffer, sizeof(rawBuffer), &bytesRead, nullptr) && bytesRead > 0) {
                    // 转换为宽字符
                    int wideLen = MultiByteToWideChar(CP_UTF8, 0, rawBuffer, bytesRead, nullptr, 0);
                    wchar_t* wideBuffer = new wchar_t[wideLen + 1];
                    MultiByteToWideChar(CP_UTF8, 0, rawBuffer, bytesRead, wideBuffer, wideLen);
                    wideBuffer[wideLen] = L'\0';

                    // 实时输出到控制台
                    wcout << wideBuffer << flush;
                    response += wideBuffer;
                    delete[] wideBuffer;

                    // 检查结束标记
                    if (response.find(endFlag) != wstring::npos) {
                        receivedComplete = true;
                        break;
                    }
                }
                else if (GetLastError() != ERROR_IO_PENDING) {
                    break;
                }

                // 降低CPU占用
                this_thread::sleep_for(chrono::milliseconds(10));
            }

            // 处理最终结果
            if (receivedComplete) {
                wcout << L"\n[成功] 设备确认配置完成，SGateConfig导入成功 " << endl;
            }
            else if (!response.empty()) {
                wcout << L"\n[警告] 收到部分响应但未完成验证\n"
                    << L"接收内容：" << response << endl;
            }
            else {
                wcout << L"[提示] 未收到设备响应，请检查设备状态" << endl;
            }
        }

        CloseHandle(hSerial);
        break;
    }

    // 程序结束提示
    wcout << L"\n================= SGate MMC =================";
    wcout << L"\n按Enter键退出...";
    wcin.ignore();

    return 0;
}