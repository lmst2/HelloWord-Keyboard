# 瀚文（HelloWord）智能键盘 —— 完整开发指南

> 本文档基于仓库源码深度分析整理，旨在为二次开发者提供从环境搭建到功能自定义的全链路指导。

---

## 目录

- [1. 项目总览](#1-项目总览)
  - [1.1 硬件架构](#11-硬件架构)
  - [1.2 仓库目录结构](#12-仓库目录结构)
  - [1.3 双固件工程说明](#13-双固件工程说明)
- [2. 开发环境搭建](#2-开发环境搭建)
  - [2.1 工具链安装](#21-工具链安装)
  - [2.2 IDE 配置（CLion / STM32CubeIDE）](#22-ide-配置clion--stm32cubeide)
  - [2.3 STM32CubeMX 工程生成](#23-stm32cubemx-工程生成)
  - [2.4 编译固件](#24-编译固件)
- [3. 连接键盘与烧录固件](#3-连接键盘与烧录固件)
  - [3.1 硬件连接](#31-硬件连接)
  - [3.2 驱动安装](#32-驱动安装)
  - [3.3 烧录方式](#33-烧录方式)
- [4. 自定义按键映射](#4-自定义按键映射)
  - [4.1 映射原理](#41-映射原理)
  - [4.2 Layer 0：物理布局到标准布局](#42-layer-0物理布局到标准布局)
  - [4.3 Layer 1-4：自定义键位层](#43-layer-1-4自定义键位层)
  - [4.4 Fn 键与层切换](#44-fn-键与层切换)
  - [4.5 组合键与宏](#45-组合键与宏)
  - [4.6 EEPROM 持久化配置](#46-eeprom-持久化配置)
- [5. 自定义灯光效果（RGB LED）](#5-自定义灯光效果rgb-led)
  - [5.1 硬件概况](#51-硬件概况)
  - [5.2 灯效编程接口](#52-灯效编程接口)
  - [5.3 内置 Demo 灯效解析](#53-内置-demo-灯效解析)
  - [5.4 自定义灯效示例](#54-自定义灯效示例)
  - [5.5 Fn 快捷键切换灯效](#55-fn-快捷键切换灯效)
  - [5.6 上位机 RGB 控制协议](#56-上位机-rgb-控制协议)
  - [5.7 SignalRGB 集成](#57-signalrgb-集成)
- [6. 墨水屏自定义](#6-墨水屏自定义)
  - [6.1 硬件参数](#61-硬件参数)
  - [6.2 固件驱动接口](#62-固件驱动接口)
  - [6.3 使用上位机工具修改图片](#63-使用上位机工具修改图片)
  - [6.4 USB 协议实现自定义刷新](#64-usb-协议实现自定义刷新)
  - [6.5 定时自动刷新方案](#65-定时自动刷新方案)
  - [6.6 开发上位机实时刷新软件](#66-开发上位机实时刷新软件)
- [7. OLED 显示自定义](#7-oled-显示自定义)
  - [7.1 硬件参数](#71-硬件参数)
  - [7.2 U8g2 图形库接口](#72-u8g2-图形库接口)
  - [7.3 自定义显示内容](#73-自定义显示内容)
  - [7.4 菜单系统与滚轮交互](#74-菜单系统与滚轮交互)
- [8. 滚轮（FOC 力反馈旋钮）自定义](#8-滚轮foc-力反馈旋钮自定义)
  - [8.1 硬件架构](#81-硬件架构)
  - [8.2 力反馈模式](#82-力反馈模式)
  - [8.3 自定义旋钮功能](#83-自定义旋钮功能)
  - [8.4 旋钮与 OLED 联动](#84-旋钮与-oled-联动)
- [9. 上位机软件开发](#9-上位机软件开发)
  - [9.1 HID 通信协议（键盘本体）](#91-hid-通信协议键盘本体)
  - [9.2 CDC/VCP 通信协议（Dynamic 模块）](#92-cdcvcp-通信协议dynamic-模块)
  - [9.3 Python 上位机示例](#93-python-上位机示例)
- [10. 关键源文件速查表](#10-关键源文件速查表)
- [11. 常见问题（FAQ）](#11-常见问题faq)
- [12. USB 固件刷写（免拆壳方案）](#12-usb-固件刷写免拆壳方案)

---

## 1. 项目总览

### 1.1 硬件架构

瀚文键盘由三大部分组成：

```
┌──────────────────────────────────────────────────────┐
│                    扩展坞底座                          │
│  (TypeC接口板 + USB-HUB + 电源管理 + FFC连接器)       │
│                                                       │
│  ┌─────────────────┐     ┌─────────────────────────┐ │
│  │  左侧Dynamic模块  │     │     键盘输入模块          │ │
│  │  STM32F405RGT6   │     │     STM32F103CBT6       │ │
│  │                  │     │                          │ │
│  │  ● 墨水屏 128×296│     │  ● 75键（82键位+触摸条） │ │
│  │  ● OLED 128×32   │     │  ● 104颗 WS2812B RGB   │ │
│  │  ● FOC力反馈旋钮  │     │  ● 11片 74HC165 扫描    │ │
│  │  ● 4颗 RGB LED   │     │  ● USB Custom HID       │ │
│  │  ● 2个按钮        │     │                          │ │
│  │  ● USB CDC/VCP   │     │                          │ │
│  └─────────────────┘     └─────────────────────────┘ │
└──────────────────────────────────────────────────────┘
```

| 模块 | MCU | 内核 | 主要功能 |
|------|-----|------|----------|
| 键盘本体 | STM32F103CBT6 | Cortex-M3 | 按键扫描、RGB、HID 键盘 |
| Dynamic 模块 | STM32F405RGT6 | Cortex-M4F | FOC旋钮、墨水屏、OLED、RGB |
| 磁编码器 | AS5047P | - | 无刷电机位置反馈 |
| 触摸条 | XW06A | - | 6键电容触摸 |

两个模块通过 **UART 串口**通信，各自独立通过 USB 连接电脑。

### 1.2 仓库目录结构

```
HelloWord-Keyboard/
├── 1.Hardware/                    # 硬件设计（立创EDA工程链接）
├── 2.Firmware/                    # 固件源码
│   ├── HelloWord-Keyboard-fw/     #   键盘固件（STM32F103）
│   └── HelloWord-Dynamic-fw/      #   Dynamic模块固件（STM32F405）
├── 3.Software/                    # 上位机工具
│   ├── HelloWord_plugin.js        #   SignalRGB 灯效插件
│   ├── 说明.md                    #   Zadig 驱动安装说明
│   └── 修改墨水屏图片/            #   墨水屏图片修改工具（exe）
├── 4.Tools/                       # 辅助工具（HID Descriptor Tool）
├── 5.Docs/                        # 文档与图片
├── LICENSE                        # GNU GPL v3
└── README.md                      # 项目说明
```

### 1.3 双固件工程说明

项目包含两个独立的固件工程，需要分别编译和烧录：

**HelloWord-Keyboard-fw（键盘固件）**
```
HelloWord-Keyboard-fw/
├── CMakeLists.txt                 # CMake 构建配置
├── HelloWord-Keyboard-fw.ioc      # STM32CubeMX 工程
├── STM32F103CBTx_FLASH.ld         # 链接脚本
├── Core/                          # HAL 初始化
├── Drivers/                       # CMSIS + STM32F1xx HAL
├── Middlewares/                    # USB Device Library (Custom HID)
├── USB_DEVICE/App/                # HID 报告描述符
├── HelloWord/                     # 键盘核心逻辑
│   ├── hw_keyboard.h              #   按键映射、扫描、RGB 定义
│   └── hw_keyboard.cpp            #   扫描、去抖、重映射、灯控实现
└── UserApp/
    ├── main.cpp                   #   主循环（灯效 + HID回调）
    └── configurations.h           #   EEPROM 配置结构
```

**HelloWord-Dynamic-fw（Dynamic 模块固件）**
```
HelloWord-Dynamic-fw/
├── CMakeLists.txt                 # CMake 构建配置
├── HelloWord-Dynamic-fw.ioc       # STM32CubeMX 工程
├── STM32F405RGTx_FLASH.ld         # 链接脚本
├── Core/                          # HAL 初始化 + FreeRTOS
├── Drivers/                       # CMSIS + STM32F4xx HAL + arm_math
├── Middlewares/                    # FreeRTOS + USB CDC
├── BSP/
│   ├── eink/                      #   墨水屏驱动（128×296 黑白）
│   ├── u8g2/                      #   OLED 图形库（SH1106 128×32）
│   └── fibre/                     #   USB 通信协议框架
├── Ctrl/
│   ├── Motor/                     #   FOC电机控制 + 旋钮模拟
│   ├── Driver/                    #   电机驱动（FD6288Q）
│   └── Sensor/Encoder/            #   AS5047P 磁编码器
├── Platform/
│   ├── Utils/rgb_light.*          #   Dynamic RGB LED 驱动
│   └── Communication/             #   USB/UART 通信接口
└── UserApp/
    ├── main.cpp                   #   FreeRTOS 任务入口
    └── protocols/usb_protocol.cpp #   USB Bulk 协议（墨水屏数据）
```

---

## 2. 开发环境搭建

### 2.1 工具链安装

#### 必需软件

| 软件 | 用途 | 下载地址 |
|------|------|----------|
| **arm-none-eabi-gcc** | ARM 交叉编译工具链 | [ARM Developer](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads) |
| **CMake** (≥3.22) | 构建系统 | [cmake.org](https://cmake.org/download/) |
| **STM32CubeMX** | 芯片引脚配置与代码生成 | [ST 官网](https://www.st.com/en/development-tools/stm32cubemx.html) |
| **Ninja** 或 **Make** | 构建后端 | 随 MinGW/MSYS2 安装 |

#### 推荐软件

| 软件 | 用途 |
|------|------|
| **CLion** | 推荐 IDE（作者使用），原生支持 CMake |
| **STM32CubeIDE** | ST 官方 IDE（免费，基于 Eclipse） |
| **ST-Link Utility** | 固件烧录工具 |
| **STM32CubeProgrammer** | 固件烧录工具（ST-Link Utility 的替代品） |
| **Zadig** | USB 驱动安装（墨水屏上位机需要） |
| **OpenOCD** | 开源调试/烧录工具 |

#### Windows 环境安装步骤

1. **安装 arm-none-eabi-gcc**
   - 下载安装包，安装时勾选 "Add path to environment variable"
   - 验证：
   ```bash
   arm-none-eabi-gcc --version
   ```

2. **安装 CMake**
   - 下载 MSI 安装包，安装时选择 "Add CMake to system PATH"
   - 验证：
   ```bash
   cmake --version
   ```

3. **安装 MinGW / Ninja（构建后端）**
   - 推荐通过 MSYS2 安装 `mingw-w64-x86_64-ninja`
   - 或单独下载 [Ninja](https://github.com/nicerobot/ninja/releases)

4. **安装 STM32CubeMX**
   - 安装后打开对应 `.ioc` 文件即可查看/修改芯片配置

### 2.2 IDE 配置（CLion / STM32CubeIDE）

#### 方案一：CLion（推荐）

参考作者教程：[配置CLion用于STM32开发【优雅の嵌入式开发】](https://zhuanlan.zhihu.com/p/145801160)

核心配置步骤：
1. 打开 CLion → File → Open → 选择 `2.Firmware/HelloWord-Keyboard-fw/` 目录
2. CLion 会自动检测 `CMakeLists.txt`
3. Settings → Build → Toolchains → 添加 "Embedded GCC" 工具链：
   - C Compiler: `arm-none-eabi-gcc`
   - C++ Compiler: `arm-none-eabi-g++`
4. Settings → Build → CMake → 选择 Debug/Release 配置
5. 配置 OpenOCD 用于下载调试（需安装 OpenOCD + STLink 插件）

#### 方案二：STM32CubeIDE

1. 打开 STM32CubeMX → 加载 `.ioc` 文件
2. Project Manager → Toolchain/IDE → 选择 "STM32CubeIDE"
3. 点击 "Generate Code" 生成 CubeIDE 工程
4. 用 STM32CubeIDE 打开生成的工程
5. 手动将 `HelloWord/`、`UserApp/` 等自定义代码目录添加到工程

### 2.3 STM32CubeMX 工程生成

两个固件都提供了 `.ioc` 文件，可以用来查看引脚分配和外设配置：

- `HelloWord-Keyboard-fw.ioc`：键盘固件的芯片配置
- `HelloWord-Dynamic-fw.ioc`：Dynamic 模块的芯片配置

**注意**：如果用 CubeMX 重新生成代码，自定义代码（位于 `UserApp/`、`HelloWord/`、`BSP/`、`Ctrl/`、`Platform/` 等目录）不会被覆盖，但 `Core/` 和 `USB_DEVICE/` 中的修改可能被重置。务必在 `/* USER CODE BEGIN */` 和 `/* USER CODE END */` 之间编写代码。

### 2.4 编译固件

#### 编译键盘固件

```bash
cd 2.Firmware/HelloWord-Keyboard-fw
mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja
```

成功后在 `build/` 目录下生成：
- `HelloWord-Keyboard-fw.elf`
- `HelloWord-Keyboard-fw.hex`
- `HelloWord-Keyboard-fw.bin`

#### 编译 Dynamic 模块固件

```bash
cd 2.Firmware/HelloWord-Dynamic-fw
mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja
```

成功后在 `build/` 目录下生成：
- `HelloWord-Dynamic-fw.elf`
- `HelloWord-Dynamic-fw.hex`
- `HelloWord-Dynamic-fw.bin`

---

## 3. 连接键盘与烧录固件

### 3.1 硬件连接

两个固件工程需要分别通过 SWD 调试器烧录。

**SWD 接线（通用）：**

| 调试器引脚 | 目标 PCB |
|-----------|----------|
| SWDIO | SWDIO (PA13) |
| SWCLK | SWCLK (PA14) |
| GND | GND |
| 3.3V | 3.3V（可选，USB 供电时不需要） |

**Dynamic 模块**：外壳上预留了 SWD 调试口开孔，可以直接连接调试器，无需拆壳。

**键盘本体**：外壳上 **没有** 外露的调试口。键盘 PCB 上的 STM32F103 芯片配置了 SWD 引脚（PA13/PA14），但需要 **拆开键盘外壳** 才能接触到 PCB 上的 SWD 焊盘/排针。具体操作：
1. 拆卸键盘外壳上盖（螺丝在底部）
2. 移除键帽和定位板（如需要）
3. 在键盘 PCB 上找到 SWD 接口位置（通常标注为 `SWDIO`、`SWCLK`、`GND` 的焊盘或排针孔）
4. 使用杜邦线或探针连接调试器

> ⚠️ README 中提到"我晚点也会放出一个 Bootloader，可以直接通过 USB 口进行固件升级"，但目前仓库中尚未提供该 Bootloader。如果不想每次都拆壳，可以考虑自行实现一个 USB DFU Bootloader，或在 PCB SWD 焊盘上焊接排针并引出线缆。

**注意**：烧录时需要通过 USB 给键盘供电（连接电脑 USB 口即可）。

### 3.2 驱动安装

#### ST-Link 驱动

- 安装 ST-Link Utility 或 STM32CubeProgrammer 时会自动安装 ST-Link 驱动
- 也可单独从 ST 官网下载 [ST-Link USB Driver](https://www.st.com/en/development-tools/stsw-link009.html)

#### Zadig（Dynamic 模块的 USB 驱动）

修改墨水屏图片的上位机工具需要 libusb 驱动：

1. 下载 [Zadig](https://zadig.akeo.ie/)
2. 将 Dynamic 模块通过 USB 连接电脑
3. 打开 Zadig → Options → List All Devices
4. 选择 Dynamic 模块对应的 USB 设备
5. Driver 列选择 **libusb-win32** 或 **WinUSB**
6. 点击 "Replace Driver" 或 "Install Driver"

> ⚠️ 安装 libusb 驱动后，该设备将不再作为 CDC 虚拟串口使用。如需恢复串口功能，需要在设备管理器中卸载 libusb 驱动。

### 3.3 烧录方式

#### 方式一：ST-Link Utility（推荐新手）

1. 打开 ST-Link Utility
2. Target → Connect（确保 ST-Link 已连接）
3. File → Open file → 选择编译好的 `.bin` 或 `.hex` 文件
4. Target → Program & Verify
5. 烧录地址（Start Address）：`0x08000000`

#### 方式二：STM32CubeProgrammer

1. 选择 ST-LINK 连接方式
2. 点击 Connect
3. 加载 `.bin` / `.hex` / `.elf` 文件
4. 点击 Download

#### 方式三：OpenOCD（命令行）

```bash
# 键盘固件（STM32F103）
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "program build/HelloWord-Keyboard-fw.bin verify reset exit 0x08000000"

# Dynamic 模块（STM32F405）
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "program build/HelloWord-Dynamic-fw.bin verify reset exit 0x08000000"
```

#### 方式四：CLion 集成下载

在 CLion 中配置 OpenOCD Run Configuration，可以一键编译+下载+调试。

---

## 4. 自定义按键映射

### 4.1 映射原理

键盘使用 **两级映射** 机制：

```
物理按键(74HC165扫描序号) → [Layer 0] → 标准键位位置 → [Layer N] → 最终HID键码
```

- **Layer 0**：将 PCB 走线的物理扫描顺序映射到标准键盘布局位置
- **Layer 1-4**：将标准键位映射到实际要发送的 HID 键码

这种设计使得 PCB 走线可以完全自由，不需要按照键盘物理位置排列。

### 4.2 Layer 0：物理布局到标准布局

文件：`2.Firmware/HelloWord-Keyboard-fw/HelloWord/hw_keyboard.h`

```cpp
int16_t keyMap[5][IO_NUMBER] = {
    // Layer 0: 物理扫描顺序 → 标准位置编号
    {67,61,60,58,59,52,55,51,50,49,48,47,46,3,
        80,81,64,57,62,63,53,54,45,44,40,31,26,18,2,
        19,70,71,66,65,56,36,37,38,39,43,42,41,28,1,
        15,74,73,72,68,69,29,30,35,34,33,32,24,0,
        14,76,77,78,79,16,20,21,22,23,27,25,17,4,
        13,12,8,75,9,10,7,11,6,5,
        86,84,82,87,85,83}, // 最后6个是触摸条
    ...
};
```

Layer 0 中的每个数字表示：对应物理扫描位置的按键应该映射到标准布局的第几个位置。

如果你修改了 PCB 布局或按键数量，只需要修改这一层。

### 4.3 Layer 1-4：自定义键位层

```cpp
    // Layer 1: 标准 QWERTY 布局
    {ESC,F1,F2,F3,F4,F5,F6,F7,F8,F9,F10,F11,F12,PAUSE,
        GRAVE_ACCENT,NUM_1,NUM_2,NUM_3,NUM_4,NUM_5,NUM_6,NUM_7,NUM_8,NUM_9,NUM_0,MINUS,EQUAL,BACKSPACE,INSERT,
        TAB,Q,W,E,R,T,Y,U,I,O,P,LEFT_U_BRACE,RIGHT_U_BRACE,BACKSLASH,DELETE,
        CAP_LOCK,A,S,D,F,G,H,J,K,L,SEMI_COLON,QUOTE,ENTER,PAGE_UP,
        LEFT_SHIFT,Z,X,C,V,B,N,M,COMMA,PERIOD,SLASH,RIGHT_SHIFT,UP_ARROW,PAGE_DOWN,
        LEFT_CTRL,LEFT_GUI,LEFT_ALT,SPACE,RIGHT_ALT,FN,RIGHT_CTRL,LEFT_ARROW,DOWN_ARROW,RIGHT_ARROW},

    // Layer 2: 可自定义（当前为示例布局）
    {ESC,F1,F2,F3,F4,F5,F6,F7,F8,F9,F10,F11,F12,PAUSE,
        ...},
```

可用键码定义在同一文件的 `KeyCode_t` 枚举中：

```cpp
enum KeyCode_t : int16_t {
    LEFT_CTRL = -8, LEFT_SHIFT = -7, LEFT_ALT = -6, LEFT_GUI = -5,
    RIGHT_CTRL = -4, RIGHT_SHIFT = -3, RIGHT_ALT = -2, RIGHT_GUI = -1,
    RESERVED = 0,
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    NUM_1, NUM_2, ... NUM_0,
    ENTER, ESC, BACKSPACE, TAB, SPACE,
    MINUS, EQUAL, LEFT_U_BRACE, RIGHT_U_BRACE, BACKSLASH,
    ...
    F1, F2, ... F24,
    MUTE, VOLUME_UP, VOLUME_DOWN,
    FN = 1000  // 特殊：Fn 键
};
```

### 4.4 Fn 键与层切换

Fn 键在固件中用于切换键位层。核心逻辑在 `UserApp/main.cpp` 的定时器回调中：

```cpp
extern "C" void OnTimerCallback() // 1000Hz callback
{
    keyboard.ScanKeyStates();
    keyboard.ApplyDebounceFilter(100);
    // Fn 按下时使用 Layer 2，否则使用 Layer 1
    keyboard.Remap(keyboard.FnPressed() ? 2 : 1);

    USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS,
                               keyboard.GetHidReportBuffer(1),
                               HWKeyboard::KEY_REPORT_SIZE);
}
```

**自定义更多层切换**：可以检测特定组合键来切换到不同层：

```cpp
// 示例：Fn+1 切换到 Layer 3，Fn+2 切换到 Layer 4
static uint8_t currentLayer = 1;

if (keyboard.FnPressed() && keyboard.KeyPressed(HWKeyboard::NUM_1))
    currentLayer = 3;
else if (keyboard.FnPressed() && keyboard.KeyPressed(HWKeyboard::NUM_2))
    currentLayer = 4;
else if (keyboard.FnPressed())
    currentLayer = 2;
else
    currentLayer = 1;

keyboard.Remap(currentLayer);
```

### 4.5 组合键与宏

可以在定时器回调中检测组合键并触发动作：

```cpp
// 示例：Ctrl+A 触发 Delete 键
if (keyboard.KeyPressed(HWKeyboard::LEFT_CTRL) &&
    keyboard.KeyPressed(HWKeyboard::A))
{
    keyboard.Press(HWKeyboard::DELETE);
}
```

相关 API：

| 方法 | 功能 |
|------|------|
| `keyboard.KeyPressed(key)` | 检测某键是否被按下 |
| `keyboard.Press(key)` | 在 HID 报告中注入一个按键 |
| `keyboard.Release(key)` | 从 HID 报告中释放一个按键 |
| `keyboard.FnPressed()` | 检测 Fn 键是否被按下 |
| `keyboard.GetTouchBarState(id)` | 获取触摸条状态 |

### 4.6 EEPROM 持久化配置

文件：`UserApp/configurations.h`

```cpp
typedef struct KeyboardConfig_t {
    configStatus_t configStatus;
    uint64_t serialNum;
    int8_t keyMap[128];  // 存储自定义键位映射
} KeyboardConfig_t;
```

在 `main.cpp` 中通过 EEPROM 类读写：

```cpp
EEPROM eeprom;
eeprom.Pull(0, config);  // 从 Flash 读取配置
// ... 修改 config ...
eeprom.Push(0, config);  // 写回 Flash
```

---

## 5. 自定义灯光效果（RGB LED）

### 5.1 硬件概况

| 项目 | 键盘本体 | Dynamic 模块 |
|------|----------|-------------|
| LED 数量 | 104 颗 | 4 颗 |
| LED 型号 | WS2812B | WS2812B |
| 驱动方式 | SPI2 + DMA | SPI3 + DMA |
| 数据格式 | GRB, 每色 8bit | GRB, 每色 8bit |

键盘的 104 颗 LED 包括：82 个按键灯 + 3 个前面板指示灯 + 19 个背部氛围灯。

### 5.2 灯效编程接口

文件：`HelloWord/hw_keyboard.h` / `hw_keyboard.cpp`

```cpp
struct Color_t {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

// 设置单个 LED 颜色
void SetRgbBufferByID(uint8_t _keyId, Color_t _color, float _brightness = 1);

// 将缓冲区数据通过 DMA 发送给 LED
void SyncLights();
```

LED 编号 `_keyId` 范围：`0` ~ `LED_NUMBER-1`（即 0~103）。

**LED 编号对应关系**（参考 SignalRGB 插件中的 `vKeys` 数组）：

- 0~13：第一行（ESC, F1-F12, Pause）
- 14~28：第二行
- 29~43：第三行
- 44~57：第四行
- 58~71：第五行
- 72~81：第六行
- 82~84：前面板指示灯
- 85~103：背部氛围灯（代码中被注释，可启用）

### 5.3 内置 Demo 灯效解析

当前固件中内置了一个简单的呼吸灯效果：

```cpp
while (true)
{
    static uint32_t t = 1;
    static bool fadeDir = true;

    fadeDir ? t++ : t--;
    if (t > 250) fadeDir = false;
    else if (t < 1) fadeDir = true;

    if (!isSoftWareControlColor)
    {
        for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
            keyboard.SetRgbBufferByID(i, HWKeyboard::Color_t{(uint8_t)t, 50, 20});
        keyboard.SyncLights();
    }
}
```

这个效果让所有 LED 的红色通道在 1~250 之间呼吸变化。

### 5.4 自定义灯效示例

以下是几种常见灯效的实现思路：

#### 彩虹流光效果

```cpp
static uint16_t hueOffset = 0;
hueOffset = (hueOffset + 1) % 360;

for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
{
    uint16_t hue = (hueOffset + i * 4) % 360;
    // HSV to RGB (simplified)
    uint8_t r, g, b;
    HsvToRgb(hue, 255, 200, &r, &g, &b);
    keyboard.SetRgbBufferByID(i, HWKeyboard::Color_t{r, g, b});
}
keyboard.SyncLights();
```

#### 按键响应灯效

在定时器回调中检测按键状态，并设置对应按键的 LED 颜色：

```cpp
for (uint8_t i = 0; i < HWKeyboard::KEY_NUMBER; i++)
{
    if (/* key i is pressed */)
        keyboard.SetRgbBufferByID(i, HWKeyboard::Color_t{255, 255, 255});
    else
        keyboard.SetRgbBufferByID(i, HWKeyboard::Color_t{0, 0, 30});
}
```

#### 静态单色

```cpp
for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
    keyboard.SetRgbBufferByID(i, HWKeyboard::Color_t{0, 100, 255});
keyboard.SyncLights();
```

### 5.5 Fn 快捷键切换灯效

在 `main.cpp` 中实现灯效切换逻辑。推荐方案：

```cpp
// 在 main.cpp 顶部定义
enum LightEffect_t { EFFECT_BREATHING, EFFECT_RAINBOW, EFFECT_STATIC, EFFECT_REACTIVE, EFFECT_COUNT };
static LightEffect_t currentEffect = EFFECT_BREATHING;

// 在 OnTimerCallback 中添加 Fn 组合键检测
extern "C" void OnTimerCallback()
{
    keyboard.ScanKeyStates();
    keyboard.ApplyDebounceFilter(100);
    keyboard.Remap(keyboard.FnPressed() ? 2 : 1);

    // Fn + F1~F4 切换灯效
    static bool fnF1LastState = false;
    bool fnF1Pressed = keyboard.FnPressed() && keyboard.KeyPressed(HWKeyboard::F1);
    if (fnF1Pressed && !fnF1LastState)
        currentEffect = static_cast<LightEffect_t>((currentEffect + 1) % EFFECT_COUNT);
    fnF1LastState = fnF1Pressed;

    // Fn + Up/Down 调节亮度
    // ...

    USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS,
                               keyboard.GetHidReportBuffer(1),
                               HWKeyboard::KEY_REPORT_SIZE);
}

// 在主循环中根据 currentEffect 渲染不同灯效
void Main()
{
    // ... 初始化 ...
    while (true)
    {
        if (!isSoftWareControlColor)
        {
            switch (currentEffect) {
                case EFFECT_BREATHING: RenderBreathing(); break;
                case EFFECT_RAINBOW:   RenderRainbow();   break;
                case EFFECT_STATIC:    RenderStatic();    break;
                case EFFECT_REACTIVE:  RenderReactive();  break;
            }
            keyboard.SyncLights();
        }
    }
}
```

### 5.6 上位机 RGB 控制协议

键盘通过 HID Report ID 2 接收上位机的 RGB 控制数据：

| 字节 | 含义 |
|------|------|
| 0 | Report ID = `0x02` |
| 1 | 命令：`0xAC`=上位机控制模式，`0xBD`=关闭上位机控制 |
| 2 | 包序号（0~11，每包 10 颗 LED，共约 110 颗） |
| 3~32 | RGB 数据（10 组 × 3 字节 = 30 字节） |

总包大小：33 字节。

固件接收端处理逻辑（`main.cpp`）：

```cpp
void HID_RxCpltCallback(uint8_t* _data)
{
    if (_data[1] == 0xbd)  isSoftWareControlColor = false;  // 退出上位机控制
    if (_data[1] == 0xac) {
        isSoftWareControlColor = true;  // 进入上位机控制
        uint8_t pageIndex = _data[2];
        for (uint8_t i = 0; i < 10; i++) {
            if (i + pageIndex * 10 >= HWKeyboard::LED_NUMBER) {
                isReceiveSuccess = true;
                break;
            }
            keyboard.SetRgbBufferByID(i + pageIndex * 10,
                HWKeyboard::Color_t{_data[3+i*3], _data[4+i*3], _data[5+i*3]});
        }
    }
}
```

### 5.7 SignalRGB 集成

仓库提供了 SignalRGB 插件：`3.Software/HelloWord_plugin.js`

使用方法：
1. 安装 [SignalRGB](https://www.signalrgb.com/)
2. 将 `HelloWord_plugin.js` 复制到 SignalRGB 插件目录
3. 在 SignalRGB 中选择键盘设备

USB 识别信息：VendorId = `0x1001`，ProductId = `0xF103`

---

## 6. 墨水屏自定义

### 6.1 硬件参数

| 参数 | 值 |
|------|------|
| 型号 | DEPG0290BxS75AFxX |
| 分辨率 | 128 × 296 像素 |
| 颜色 | 黑白（1bpp） |
| 接口 | SPI2 |
| 显存大小 | 128 × 296 / 8 = **4,736 字节** |
| 引脚 | CS=PA4, DC=PC0, BUSY=PC1, RST=PC2 |

### 6.2 固件驱动接口

文件：`BSP/eink/eink_290_bw.h` / `eink_290_bw.cpp`

```cpp
class Eink290BW {
public:
    void Init();                              // 初始化墨水屏
    void DrawBitmap(const unsigned char* datas); // 写入位图数据
    void Update();                            // 刷新屏幕显示
    void DeepSleep();                         // 进入深度睡眠

    static uint8_t buffer[EPD_HEIGHT * EPD_WIDTH / 8]; // 显示缓冲区
};
```

使用流程：
```cpp
eink.Init();
// 填充 buffer 数据...
memcpy(Eink290BW::buffer, imageData, sizeof(Eink290BW::buffer));
eink.DrawBitmap(Eink290BW::buffer);
eink.Update();  // 实际刷新屏幕（耗时约 2-3 秒）
```

### 6.3 使用上位机工具修改图片

仓库提供了现成工具：`3.Software/修改墨水屏图片/`

**使用步骤：**

1. 先用 Zadig 为 Dynamic 模块安装 libusb 驱动（见 3.2 节）
2. 准备一张图片（建议 296×128 分辨率，JPG 格式）
3. 将图片拖拽到 `将图片拖拽到这里.exe` 上
4. 工具会自动缩放、二值化、传输并刷新墨水屏

**注意**：该工具源码未开源，仅提供可执行文件。

### 6.4 USB 协议实现自定义刷新

文件：`UserApp/protocols/usb_protocol.cpp`

Dynamic 模块通过 USB CDC Bulk 端点接收墨水屏图像数据：

```cpp
uint8_t* OnBulkPacket(const uint8_t* _buffer, size_t _bufferLength,
                      uint32_t _packetWriteAddressOffset,
                      uint32_t _readDataLength)
{
    // 收到 3 字节触发刷新
    if (_bufferLength == 3)
    {
        eink.DrawBitmap(Eink290BW::buffer);
        eink.Update();
    }

    // 接收图片数据并写入 buffer
    if (_readDataLength == 0) // Host Write
    {
        memcpy(Eink290BW::buffer + _packetWriteAddressOffset, _buffer, _bufferLength);
    }
    return nullptr;
}
```

传输协议：
- 基于 fibre 协议框架
- 每包最大 64 字节（2 字节 header + 62 字节数据）
- 发送 4,736 字节的 1bpp 位图数据
- 最后发送 3 字节的提交包触发刷新

### 6.5 定时自动刷新方案

可以在 Dynamic 模块固件中实现基于 RTC 或系统时钟的定时刷新：

```cpp
// 在 ThreadOledUpdate 或新线程中
void ThreadEinkUpdate(void* argument)
{
    eink.Init();
    uint32_t lastRefreshTick = 0;
    const uint32_t REFRESH_INTERVAL_MS = 60000; // 每分钟刷新

    for (;;)
    {
        uint32_t now = HAL_GetTick();
        if (now - lastRefreshTick >= REFRESH_INTERVAL_MS)
        {
            // 更新 buffer 内容（如时间、状态等）
            UpdateEinkContent();
            eink.DrawBitmap(Eink290BW::buffer);
            eink.Update();
            lastRefreshTick = now;
        }
        osDelay(1000);
    }
}
```

### 6.6 开发上位机实时刷新软件

开发思路（Python 示例）：

1. 使用 `pyusb` 或 `libusb` 连接 Dynamic 模块
2. 将图片转换为 128×296 的 1bpp 位图
3. 按照协议分包发送
4. 发送提交包触发刷新

```python
import usb.core
import usb.util
from PIL import Image
import numpy as np

# 连接设备
dev = usb.core.find(idVendor=0xXXXX, idProduct=0xXXXX)  # 替换为实际 VID/PID

# 图片转换
img = Image.open("image.png").convert("L").resize((296, 128))
img = img.point(lambda x: 0 if x < 128 else 255, '1')
bitmap = np.packbits(np.array(img).flatten()).tobytes()

# 分包发送（每包 62 字节数据）
CHUNK_SIZE = 62
for offset in range(0, len(bitmap), CHUNK_SIZE):
    chunk = bitmap[offset:offset + CHUNK_SIZE]
    # 通过 USB bulk 端点发送（具体端点号需查看固件配置）
    dev.write(endpoint, header + chunk)

# 发送 3 字节提交包触发刷新
dev.write(endpoint, b'\x00\x00\x00')
```

> 注意：实际的 USB 端点号和 fibre 协议的包头格式需要参考 `BSP/fibre/` 目录下的协议实现来确定。

---

## 7. OLED 显示自定义

### 7.1 硬件参数

| 参数 | 值 |
|------|------|
| 驱动芯片 | SH1106 |
| 分辨率 | 128 × 32 像素 |
| 颜色 | 单色（白色像素） |
| 接口 | I2C1 |
| 图形库 | U8g2 |

### 7.2 U8g2 图形库接口

OLED 使用 U8g2 图形库，在 `BSP/u8g2/` 中提供了完整的库代码。

`SSD1306` 类继承自 `U8G2`，封装了初始化细节：

```cpp
SSD1306 oled(&hi2c1);  // I2C1
oled.Init();            // 自动初始化、设置中文字体
```

**常用绘图方法：**

| 方法 | 功能 |
|------|------|
| `oled.Clear()` | 清屏 |
| `oled.SetDrawColor(1)` | 设置绘制颜色（1=白/0=黑） |
| `oled.SetCursor(x, y)` | 设置光标位置 |
| `oled.DrawUTF8(x, y, "text")` | 绘制 UTF-8 文本（支持中文） |
| `oled.DrawStr(x, y, "text")` | 绘制 ASCII 文本 |
| `oled.DrawFrame(x, y, w, h)` | 绘制矩形边框 |
| `oled.DrawBox(x, y, w, h)` | 绘制填充矩形 |
| `oled.DrawXBM(x, y, w, h, bmp)` | 绘制 XBM 位图 |
| `oled.DrawHLine(x, y, w)` | 绘制水平线 |
| `oled.DrawVLine(x, y, h)` | 绘制垂直线 |
| `oled.drawLine(x1,y1,x2,y2)` | 绘制任意线 |
| `oled.drawCircle(x,y,r)` | 绘制圆 |
| `oled.SetFont(font)` | 设置字体 |
| `oled.SendBuffer()` | 将缓冲区发送到 OLED |

**字体列表**（部分）：
- `u8g2_font_wqy12_t_gb2312` - 文泉驿12px中文字体（默认）
- `u8g2_font_6x10_tf` - 6x10 ASCII
- `u8g2_font_ncenB14_tr` - 14px 英文
- 完整列表参考 [U8g2 字体列表](https://github.com/olikraus/u8g2/wiki/fntlistall)

### 7.3 自定义显示内容

当前 OLED 任务在 `UserApp/main.cpp` 的 `ThreadOledUpdate` 中：

```cpp
void ThreadOledUpdate(void* argument)
{
    oled.Init();
    eink.Init();

    for (;;)
    {
        oled.Clear();
        oled.SetDrawColor(1);
        oled.SetCursor(0, 0);
        oled.DrawUTF8(0, 10, "hello");
        oled.SendBuffer();

        // RGB 更新...
    }
}
```

**自定义示例 —— 显示旋钮位置和模式：**

```cpp
void ThreadOledUpdate(void* argument)
{
    oled.Init();
    char buf[32];

    for (;;)
    {
        oled.Clear();
        oled.SetDrawColor(1);

        // 显示当前旋钮模式
        const char* modeNames[] = {"OFF", "Inertia", "Encoder", "Spring", "Damped", "Spin"};
        snprintf(buf, sizeof(buf), "Mode: %s", modeNames[knob.GetCurrentMode()]);
        oled.DrawUTF8(0, 10, buf);

        // 显示旋钮位置
        snprintf(buf, sizeof(buf), "Pos: %.1f", knob.GetPosition());
        oled.DrawUTF8(0, 24, buf);

        // 绘制位置指示条
        int barWidth = (int)(knob.GetPosition() * 10) % 128;
        if (barWidth < 0) barWidth = -barWidth;
        oled.DrawBox(0, 28, barWidth, 4);

        oled.SendBuffer();
        osDelay(50);  // ~20fps
    }
}
```

### 7.4 菜单系统与滚轮交互

U8g2 库自带菜单功能，可以结合旋钮和按钮实现交互式菜单：

```cpp
// 使用 U8g2 的 UserInterface 菜单
// 需要先配置菜单事件回调

// 示例：通过旋钮和按钮实现选择菜单
void ShowMenu()
{
    const char* menuItems =
        "Knob Mode\n"
        "LED Effect\n"
        "Brightness\n"
        "E-Ink Refresh\n"
        "About";

    uint8_t selected = oled.userInterfaceSelectionList("Settings", 1, menuItems);

    switch (selected) {
        case 1: // 旋钮模式选择
            ShowKnobModeMenu();
            break;
        case 2: // LED 灯效选择
            ShowLedEffectMenu();
            break;
        // ...
    }
}
```

**旋钮-OLED 联动的完整方案**：

通过旋钮的编码器位置变化来导航菜单，按钮用于确认选择。在 `ThreadGpioUpdate` 和 `ThreadOledUpdate` 之间通过共享变量或队列通信：

```cpp
// 共享状态
volatile int menuCursor = 0;
volatile bool menuConfirm = false;

// ThreadGpioUpdate 中更新旋钮输入
void ThreadGpioUpdate(void* argument)
{
    for (;;)
    {
        // 旋钮位置变化 → 移动光标
        int encoderPos = knob.GetEncoderModePos();
        menuCursor = encoderPos;

        // 按钮按下 → 确认选择
        if (HAL_GPIO_ReadPin(KEY_A_GPIO_Port, KEY_A_Pin) == GPIO_PIN_RESET)
        {
            while (HAL_GPIO_ReadPin(KEY_A_GPIO_Port, KEY_A_Pin) == GPIO_PIN_RESET);
            menuConfirm = true;
        }
        osDelay(20);
    }
}

// ThreadOledUpdate 中渲染菜单
void ThreadOledUpdate(void* argument)
{
    oled.Init();
    const char* items[] = {"Inertia", "Encoder", "Spring", "Damped", "Spin"};
    int itemCount = 5;

    for (;;)
    {
        oled.Clear();
        int sel = ((menuCursor % itemCount) + itemCount) % itemCount;

        for (int i = 0; i < itemCount && i < 3; i++)
        {
            int idx = (sel - 1 + i + itemCount) % itemCount;
            if (i == 1)
            {
                oled.DrawBox(0, i * 11, 128, 11);
                oled.SetDrawColor(0);
            }
            oled.DrawUTF8(4, i * 11 + 9, items[idx]);
            oled.SetDrawColor(1);
        }

        if (menuConfirm)
        {
            knob.SetMode(static_cast<KnobSimulator::Mode_t>(sel));
            menuConfirm = false;
        }

        oled.SendBuffer();
        osDelay(50);
    }
}
```

---

## 8. 滚轮（FOC 力反馈旋钮）自定义

### 8.1 硬件架构

```
┌─────────────────────────────────────────┐
│         FOC 力反馈旋钮系统               │
│                                          │
│  AS5047P磁编码器 ──SPI1──► STM32F405    │
│                                          │
│  2204无刷电机 ◄──FD6288Q──► PWM(TIM1)   │
│                                          │
│  控制频率: 5000Hz (TIM7)                 │
│  FreeRTOS 优先级: Realtime               │
└─────────────────────────────────────────┘
```

### 8.2 力反馈模式

文件：`Ctrl/Motor/knob.h` / `knob.cpp`

| 模式 | 枚举值 | 描述 |
|------|--------|------|
| MODE_DISABLE | 0 | 禁用电机，自由转动 |
| MODE_INERTIA | 1 | 惯性模式，模拟飞轮效果 |
| MODE_ENCODER | 2 | 编码器模式，带棘齿感（12段） |
| MODE_SPRING | 3 | 弹簧模式，有中心回正力 |
| MODE_DAMPED | 4 | 阻尼模式，转动有阻力，带角度限位 |
| MODE_SPIN | 5 | 旋转模式，电机持续旋转 |

每种模式的 PID 参数和电机控制方式不同：

```cpp
case MODE_ENCODER:
    motor->SetTorqueLimit(1.5);
    motor->config.controlMode = Motor::ANGLE;
    motor->config.pidAngle.p = 100;
    motor->config.pidAngle.d = 3.5;
    // encoderDivides = 12: 一圈分为12格
    break;

case MODE_DAMPED:
    motor->SetTorqueLimit(1.5);
    motor->config.controlMode = Motor::VELOCITY;
    motor->target = 0;  // 目标速度为0 → 产生阻尼感
    // 支持角度限位
    break;
```

### 8.3 自定义旋钮功能

#### 修改编码器段数

```cpp
// knob.h 中 private 成员
int encoderDivides = 12;  // 修改此值改变棘齿数（如 24 = 更细腻）
```

#### 修改角度限位

```cpp
knob.SetLimitPos(3.3, 5.1);  // 设置 DAMPED 模式的最小/最大角度
```

#### 自定义力反馈参数

```cpp
// 增加弹簧硬度
motor->config.pidAngle.p = 150;  // 默认 100，增大 = 更硬

// 增加阻尼感
motor->config.pidVelocity.p = 0.2;  // 默认 0.1

// 限制最大力矩
motor->SetTorqueLimit(2.0);  // 默认 1.5V
```

#### 读取旋钮状态

```cpp
float position = knob.GetPosition();       // 当前角度（弧度）
float velocity = knob.GetVelocity();       // 当前角速度
int encoderPos = knob.GetEncoderModePos(); // 编码器模式的离散位置
```

### 8.4 旋钮与 OLED 联动

将旋钮状态反馈到 OLED 显示，实现交互式控制界面：

- **音量控制**：DAMPED 模式 + 位置映射到音量
- **菜单导航**：ENCODER 模式 + 编码器位置对应菜单项
- **参数调节**：SPRING 模式 + 偏移量映射到参数值

示例 — 旋钮控制音量：

```cpp
// 在控制循环中
int volume = (int)((knob.GetPosition() - limitMin) /
                   (limitMax - limitMin) * 100);
volume = constrain(volume, 0, 100);

// 在 OLED 线程中显示
char buf[16];
snprintf(buf, sizeof(buf), "Vol: %d%%", volume);
oled.DrawUTF8(0, 10, buf);
oled.DrawFrame(0, 20, 128, 8);
oled.DrawBox(0, 20, volume * 128 / 100, 8);
```

---

## 9. 上位机软件开发

### 9.1 HID 通信协议（键盘本体）

键盘本体通过 USB Custom HID 与电脑通信。

| 参数 | 值 |
|------|------|
| VID | 0x1001 |
| PID | 0xF103 |
| Report ID 1 | 键盘报文：8位修饰键 + 120位按键位图 |
| Report ID 2 | Raw HID：32字节输入 / 32字节输出 |
| Usage Page | 0xFFC0（Vendor Defined） |
| Usage | 0x0C00 |

**Report ID 2 用于上位机通信**：

发送方向（Host → Device）：
```
[0x02] [CMD] [DATA...]  // 33字节
CMD = 0xAC: 设置RGB
CMD = 0xBD: 退出上位机控制
```

可以利用 Report ID 2 自定义更多命令：
- 键位映射修改
- 固件参数调节
- 状态查询

### 9.2 CDC/VCP 通信协议（Dynamic 模块）

Dynamic 模块通过 USB CDC（虚拟串口）通信，同时提供 Bulk 端点用于大数据传输。

内置协议支持的命令（基于 fibre 协议框架）：

| 命令 | 功能 |
|------|------|
| `test_function` | 测试函数 |
| `config.can_node_id` | 读写 CAN 节点 ID |
| `save_configuration` | 保存配置 |
| `erase_configuration` | 擦除配置 |
| `get_temperature` | 读取芯片温度 |
| `reboot` | 重启模块 |

Bulk 端点用于传输墨水屏图片数据（见第 6 节）。

### 9.3 Python 上位机示例

#### RGB 控制（通过 HID）

```python
import hid

VID = 0x1001
PID = 0xF103

# 连接键盘
device = hid.device()
device.open(VID, PID)

def set_all_rgb(r, g, b):
    """设置所有 LED 为同一颜色"""
    for page in range(12):  # 110颗灯 / 10 = 11 包（取12保险）
        packet = [0x02, 0xAC, page]  # report_id, cmd, page_index
        for i in range(10):
            packet.extend([r, g, b])
        device.write(packet)

def disable_software_control():
    """退出上位机控制，恢复本地灯效"""
    packet = [0x02, 0xBD, 0x00, 0x01] + [0] * 29
    device.write(packet)

# 设置所有灯为蓝色
set_all_rgb(0, 0, 255)

# 恢复本地控制
# disable_software_control()

device.close()
```

#### 墨水屏更新（通过 USB）

```python
import usb.core
from PIL import Image
import numpy as np

def update_eink(image_path):
    """更新墨水屏图片"""
    # 1. 图片预处理
    img = Image.open(image_path).convert('L')
    img = img.resize((296, 128), Image.LANCZOS)
    # 二值化
    threshold = 128
    img = img.point(lambda p: 255 if p > threshold else 0)
    # 转换为 1bpp 位图
    pixels = np.array(img)
    bitmap = np.packbits(pixels // 255).tobytes()

    # 2. 连接设备（需要先装 libusb 驱动）
    dev = usb.core.find(idVendor=0xXXXX, idProduct=0xXXXX)
    if dev is None:
        raise ValueError("Device not found")
    dev.set_configuration()

    # 3. 分包发送 4736 字节
    CHUNK_SIZE = 62
    for offset in range(0, len(bitmap), CHUNK_SIZE):
        chunk = bitmap[offset:offset + CHUNK_SIZE]
        # 通过 bulk 端点写入（端点号需确认）
        dev.write(0x01, chunk)

    # 4. 发送 3 字节触发刷新
    dev.write(0x01, b'\x00\x00\x00')

update_eink("my_image.png")
```

> 注意：以上 USB 端点号和 VID/PID 需要根据实际固件配置调整。可以通过 `lsusb -v`（Linux）或设备管理器（Windows）查看。

---

## 10. 关键源文件速查表

| 功能 | 文件路径 |
|------|----------|
| **键盘按键映射 & HID 键码** | `2.Firmware/HelloWord-Keyboard-fw/HelloWord/hw_keyboard.h` |
| **键盘扫描 & 去抖 & RGB** | `2.Firmware/HelloWord-Keyboard-fw/HelloWord/hw_keyboard.cpp` |
| **键盘主循环 & 灯效 & HID回调** | `2.Firmware/HelloWord-Keyboard-fw/UserApp/main.cpp` |
| **键盘 EEPROM 配置结构** | `2.Firmware/HelloWord-Keyboard-fw/UserApp/configurations.h` |
| **键盘 HID 报告描述符** | `2.Firmware/HelloWord-Keyboard-fw/USB_DEVICE/App/usbd_custom_hid_if.c` |
| **键盘 CMake 构建** | `2.Firmware/HelloWord-Keyboard-fw/CMakeLists.txt` |
| **Dynamic 主程序（FreeRTOS 任务）** | `2.Firmware/HelloWord-Dynamic-fw/UserApp/main.cpp` |
| **墨水屏驱动** | `2.Firmware/HelloWord-Dynamic-fw/BSP/eink/eink_290_bw.h/.cpp` |
| **OLED 驱动 (U8g2)** | `2.Firmware/HelloWord-Dynamic-fw/BSP/u8g2/cpp/U8g2lib.hpp` |
| **旋钮力反馈控制** | `2.Firmware/HelloWord-Dynamic-fw/Ctrl/Motor/knob.h/.cpp` |
| **FOC 电机控制** | `2.Firmware/HelloWord-Dynamic-fw/Ctrl/Motor/motor.h/.cpp` |
| **磁编码器 (AS5047P)** | `2.Firmware/HelloWord-Dynamic-fw/Ctrl/Sensor/Encoder/Instances/encoder_as5047.cpp` |
| **Dynamic RGB LED** | `2.Firmware/HelloWord-Dynamic-fw/Platform/Utils/rgb_light.h/.cpp` |
| **USB Bulk 协议（墨水屏传输）** | `2.Firmware/HelloWord-Dynamic-fw/UserApp/protocols/usb_protocol.cpp` |
| **Dynamic CMake 构建** | `2.Firmware/HelloWord-Dynamic-fw/CMakeLists.txt` |
| **SignalRGB 插件** | `3.Software/HelloWord_plugin.js` |

---

## 11. 常见问题（FAQ）

### Q: 编译时找不到 arm-none-eabi-gcc？
确保安装了 ARM 工具链并添加到系统 PATH。验证：`arm-none-eabi-gcc --version`。

### Q: CLion 报错 CMake 配置失败？
检查 CLion 的工具链配置是否正确指向 arm-none-eabi 编译器。File → Settings → Build → Toolchains 中应配置为嵌入式工具链。

### Q: ST-Link 无法连接键盘？
- 确认 SWD 接线正确（SWDIO、SWCLK、GND 三根必须连接）
- 确认键盘已通过 USB 供电
- 尝试按住 Reset 按钮再连接
- 检查 ST-Link 驱动是否安装

### Q: 烧录后键盘无反应？
- 确认烧录地址为 `0x08000000`
- 确认编译目标芯片与实际芯片一致（F103 / F405）
- 尝试全片擦除后重新烧录

### Q: Dynamic 模块电机校准失败？
- FPC 排线不要太长，否则压降影响电机驱动
- 确认编码器（AS5047P）正常工作，可通过 Debug 读取编码器值
- 校准失败需要重新上电

### Q: 修改墨水屏图片工具报错？
- 确认已用 Zadig 安装 libusb 驱动
- 确认 Dynamic 模块已通过 USB 连接电脑
- 图片格式建议使用 JPG

### Q: 如何查看 Dynamic 模块的 USB VID/PID？
连接 Dynamic 模块后，在 Windows 设备管理器中查看 USB 设备属性，或使用 `USBDeview` 等工具。

### Q: 墨水屏刷新很慢（2-3秒），能加速吗？
这是墨水屏硬件的物理限制（电泳显示原理）。可以使用局部刷新（partial refresh）来加速，但需要修改驱动代码实现。全局刷新的延迟不可避免。

### Q: 键盘和 Dynamic 模块之间如何通信？
两者通过 UART 串口通信。可以在键盘固件的 `Protocols/interface_uart.cpp` 和 Dynamic 固件的 `Platform/Communication/` 中查看和修改通信协议。

---

## 12. USB 固件刷写（免拆壳方案）

本项目为两个模块分别实现了 USB 刷写方案，从此无需打开外壳连接 SWD 调试器。

### 12.1 方案概述

| 模块 | MCU | 方案 | 触发方式 |
|------|-----|------|----------|
| 键盘本体 | STM32F103 | 自定义 USB DFU Bootloader | HID 命令 `0xDF` / 无有效固件时自动进入 |
| Dynamic 模块 | STM32F405 | 内置系统 DFU Bootloader（芯片ROM） | 开机时同时按住 KEY_A + KEY_B |

### 12.2 键盘本体 USB DFU Bootloader

#### 原理

Bootloader 占据 Flash 前 16KB（`0x08000000` ~ `0x08003FFF`），键盘固件从 `0x08004000` 开始。上电后 Bootloader 先运行，检查是否需要进入 DFU 模式：

```
上电 → Bootloader 启动
        │
        ├─ RAM 中有魔术字(0xB00110AD) → 进入 USB DFU 模式
        ├─ 0x08004000 处无有效固件     → 进入 USB DFU 模式
        └─ 其他情况                    → 跳转到键盘固件
```

#### 首次烧录（需要 SWD，仅此一次）

```bash
# 1. 编译 Bootloader
cd 2.Firmware/HelloWord-Keyboard-bootloader
mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja

# 2. 编译键盘固件（已修改链接脚本，从 0x08004000 开始）
cd ../../HelloWord-Keyboard-fw
mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja

# 3. 通过 SWD 烧录 Bootloader（地址 0x08000000）
# 使用 ST-Link Utility / STM32CubeProgrammer / OpenOCD
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "program ../HelloWord-Keyboard-bootloader/build/HelloWord-Keyboard-bootloader.bin verify reset 0x08000000"

# 4. 通过 SWD 烧录键盘固件（地址 0x08004000）
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "program build/HelloWord-Keyboard-fw.bin verify reset 0x08004000"
```

#### 后续通过 USB 更新固件

安装 `dfu-util`：
- Windows: 下载 [dfu-util](http://dfu-util.sourceforge.net/releases/) 并加入 PATH
- macOS: `brew install dfu-util`
- Linux: `sudo apt install dfu-util`

**方法一：通过 HID 命令触发（推荐）**

发送 HID 报文触发键盘重启进入 DFU 模式：

```python
import hid
dev = hid.device()
dev.open(0x1001, 0xF103)
dev.write([0x02, 0xDF] + [0] * 31)  # Report ID 2, CMD 0xDF = enter DFU
dev.close()
```

键盘会立即重启进入 DFU 模式（USB 设备变为 VID:0483 PID:DF11）。

**方法二：无有效固件时自动进入**

如果 `0x08004000` 处没有有效固件（比如全片擦除后），Bootloader 自动进入 DFU 模式。

**刷写新固件：**

```bash
# 等待 DFU 设备出现后执行
dfu-util -a 0 -s 0x08004000:leave -D HelloWord-Keyboard-fw.bin
```

`-s 0x08004000:leave` 表示写入到 0x08004000 并在完成后自动重启。

#### Bootloader 项目文件

```
2.Firmware/HelloWord-Keyboard-bootloader/
├── CMakeLists.txt                      # 构建配置
├── STM32F103CBTx_BOOTLOADER.ld         # 链接脚本（16KB Flash）
├── bootloader.c                        # 入口：启动逻辑、时钟初始化、跳转
├── usb_dfu.h                           # DFU 协议定义
└── usb_dfu.c                           # 完整 USB DFU 实现（裸机寄存器级）
```

#### 键盘固件适配修改

1. **链接脚本** `STM32F103CBTx_FLASH.ld`：FLASH ORIGIN 改为 `0x08004000`，LENGTH 改为 `112K`
2. **向量表偏移** `system_stm32f1xx.c`：`VECT_TAB_OFFSET` 改为 `0x00004000`
3. **DFU 触发** `UserApp/main.cpp`：HID 收到 `0xDF` 命令时写魔术字并重启

### 12.3 Dynamic 模块 USB DFU

STM32F405 芯片内置了支持 USB DFU 的系统 Bootloader（位于 ROM 0x1FFF0000），无需自定义 Bootloader。

#### 进入 DFU 模式

**同时按住 Dynamic 模块上的 KEY_A 和 KEY_B 两个按钮，然后上电（或重新插入 USB）。**

固件在 `Main()` 函数最开始检测这两个按钮，如果同时按下则跳转到系统 Bootloader。

#### 刷写固件

```bash
# Dynamic 模块进入 DFU 后，USB 设备为 VID:0483 PID:DF11
dfu-util -a 0 -s 0x08000000:leave -D HelloWord-Dynamic-fw.bin
```

也可以使用 **STM32CubeProgrammer** 图形界面：
1. 选择 "USB" 连接方式
2. 点击 Connect（自动识别 DFU 设备）
3. 加载 .bin 文件，设置地址 `0x08000000`
4. 点击 Download

> 注意：Dynamic 模块的固件直接从 `0x08000000` 开始，不需要偏移。只有键盘本体（有自定义 Bootloader）才需要偏移到 `0x08004000`。

### 12.4 Windows 驱动注意事项

Windows 下首次使用 DFU 设备可能需要安装 WinUSB 驱动：

1. 设备进入 DFU 模式后，打开 [Zadig](https://zadig.akeo.ie/)
2. Options → List All Devices
3. 选择 "STM32 BOOTLOADER" 或 "DFU in FS Mode"
4. Driver 选择 **WinUSB**
5. 点击 Install Driver

安装后 `dfu-util -l` 应该能列出设备。

---

> 本文档基于 HelloWord-Keyboard 仓库源码分析，如有疑问可参考：
> - [B站视频](https://www.bilibili.com/video/BV19V4y1J7Hx)
> - [YouTube](https://www.youtube.com/watch?v=mGShD9ZER1c)
> - [CLion STM32 开发配置](https://zhuanlan.zhihu.com/p/145801160)
> - [U8g2 Wiki](https://github.com/olikraus/u8g2/wiki)
> - 项目 License: GNU GPL v3
