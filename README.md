# PIVOTECH · 嵌入式通用快速调试检测器

> Universal Fast Debug Detector for Embedded Systems

PIVOTECH 把输入状态检测、DC/PWM 输出、模块配置工具、I2C / SPI / UART 调试、AI 脚本调试与自定义离线菜单放进同一套软硬件工作流：Web APP 负责可视化，硬件（RP2040）负责测量与执行，两者通过 USB 串口联动，也可离线独立工作。

---

## 这是什么 · 仓库状态

本仓库是 PIVOTECH 产品的**技术源码仓库**（固件 + 上位机），非官网营销仓库。仓库按“软件分层”组织：

| 路径 | 内容 | 说明 |
| --- | --- | --- |
| `index.html` | 上位机 Web APP（**发布副本**） | GitHub Pages 从仓库根发布，即开即用 |
| `webapp/` | 上位机 Web APP（**唯一源**） | 单文件 `index.html`，改动需同步到根 `index.html` |
| `firmware/pivotech_1.1/` | PIVOTECH 主固件源码 | 即安装包 `3.PIVOTECH.1.1.uf2`；源码目录内部版本号 `1.4.2` |
| `firmware/debugprobe/` | DebugProbe 模式固件 | Debug Probe V2.2.3（CMSIS-DAP），硬件另一模式使用 |
| `firmware/bootloader_t11/` | Bootloader 引导程序 | T11 引导区程序 |
| `安装文件/` | 出厂固件 `.uf2` + 烧录工具 | `0.loadlicense / 1.autoboot / 2.debugprobe / 3.PIVOTECH.1.1 / 4.bootloader` + `watch-uf2.py` |

> 仓库不含：`docs/`（本地设计文档）、`aitool/`（AI 模型配置参考页）、`电商资料/`——仅本地保留。

---

## 在线使用 Web APP

- 国内访问：<https://conductance-lab.xyz/PIVOTECH/>
- GitHub Pages：<https://conductance-lab.github.io/PIVOTECH/>

需要支持 **Web Serial API** 的 Chromium 内核浏览器（Chrome / Edge / Opera），通过 USB 连接 PIVOTECH 或 DebugProbe 设备。

---

## 目录与构建

### 上位机（webapp / 根 index.html）
- 纯前端单文件（Vue3 + Element Plus，内联依赖），浏览器直接打开即可；串口需 `https` 或 `localhost` 环境。
- **同步约定**：`webapp/index.html` 是唯一源；根目录 `index.html` 是发布副本。改动上位机后请将 `webapp/index.html` 覆盖到根 `index.html` 再提交，保持两者一致。

### 主固件（firmware/pivotech_1.1）
- RP2040 / Pico SDK 2.1.1，C++17 双核。
- 构建（参考，使用本地 Pico SDK 工具链）：

```powershell
$env:Path = '...\pico-sdk\cmake\...\bin;...ninja...;...toolchain...\bin;' + $env:Path
Set-Location firmware/pivotech_1.1/build
cmake --build . --target pivotech_1.1 -j 4
```

- 内部版本号见源码目录（当前源码版本目录为 `1.4.2`，产品固件为 `PIVOTECH 1.1`）。

### 出厂固件（安装文件）
`*.uf2` 为出厂烧录镜像，烧录顺序参考文件名序号（`0.loadlicense → 1.autoboot → 2.debugprobe → 3.PIVOTECH.1.1 → 4.bootloader`）。

---

## 说明
- 固件与上位机通过文本帧协议联动（详见各子目录源码与注释）。
- 若有疑问或反馈：电导实验室 <conductance-lab.xyz>。
