# AI 编码代理使用指南（Copilot Instructions）

## 项目概述
本代码库是为 Raspberry Pi Pico 设计的固件/软件项目，采用嵌入式开发模式。主要使用 C++ 和 C 语言，结构模块化，涵盖硬件抽象层、用户界面和设备专用逻辑。构建系统为 CMake，Pico SDK 通过 `pico_sdk_import.cmake` 集成。

## 架构与主要组件
- **主程序入口**：`main.cpp`
- **设备配置逻辑**：`CONFIG_FLO.cpp`
- **菜单/界面**：所有 UI 和菜单逻辑位于 `menu/` 文件夹（如 `menu.cpp`、`UI.cpp`、`ssd1306.cpp` 用于显示）
- **字体渲染**：字体相关文件和头文件在 `Fonts/` 文件夹
- **数据读写逻辑**：数据读取与选择逻辑在 `read/` 文件夹
- **构建产物**：所有编译输出在 `build/` 文件夹（如 `.elf`、`.uf2`、`.hex` 等）

## 开发者工作流
- **编译**：使用 VS Code 任务“Compile Project”（通过 Pico SDK 调用 Ninja），输出在 `build/` 文件夹
- **烧录/运行**：使用“Flash”或“Run Project”任务部署到硬件，自动解析设备配置，底层用 OpenOCD 和 Picotool
- **调试**：通过 OpenOCD 调试（具体配置见 `Flash` 任务）
- **无正式测试套件**：通常在硬件上手动验证

## 项目专用约定
- **模块化目录结构**：每个主要功能（UI、菜单、字体、读写逻辑）单独文件夹
- **CMake 构建**：所有构建逻辑在 `CMakeLists.txt` 和 `pico_sdk_import.cmake`，无顶层 `README.md` 或代理规则文件
- **字体处理**：字体为自定义 C 文件，非外部库
- **硬件抽象**：硬件访问通过 Pico SDK，底层代码见 `build/pico-sdk/src/`
- **无外部云服务集成**：所有逻辑本地运行

## 集成点
- **Pico SDK**：通过 CMake 集成，所有硬件访问均通过 `build/pico-sdk/src/` 头文件
- **OpenOCD & Picotool**：用于固件烧录和运行

## 典型模式示例
- **UI 更新**：参考 `menu/UI.cpp` 和 `menu/ssd1306.cpp` 的显示逻辑
- **数据读取**：参考 `read/read.cpp` 和 `read/functions.cpp` 的设备数据访问
- **字体渲染**：参考 `Fonts/font12.c`、`Fonts/fonts.h` 的自定义字体逻辑

## 快速上手建议
- 使用 VS Code 任务进行编译/烧录/运行（不要手动运行 Ninja 或 OpenOCD）
- 新增功能时遵循模块化目录结构
- 硬件访问请参考 Pico SDK 头文件
- UI 相关请修改 `menu/` 和 `Fonts/` 文件夹
- 设备逻辑请修改 `read/` 和 `CONFIG_FLO.cpp`

---
如有不清楚或遗漏的部分，请反馈或说明需要补充的工作流、模式或集成细节。
