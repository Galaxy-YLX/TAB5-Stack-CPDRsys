# CPDR 抓娃娃机控制系统

绳索驱动并联机器人 (Cable-Driven Parallel Robot) 抓娃娃机控制系统。

## 系统概览

4台电机通过绳索驱动末端执行器(夹爪)在XY平面内运动 (有效范围 ±200mm):

```
      M01(-315, 315)          M02(315, 315)
           ★───────────────────────★
           │                        │
           │      有效范围±200mm     │
           │     (夹爪移动区域)      │
           │                        │
           ★───────────────────────★
       M04(-315,-315)          M03(315,-315)
```

## 项目结构

```
CPDR抓娃娃机/
├── src/
│   ├── main.c                  ← 主程序 (单线程, Win32消息驱动)
│   ├── motor/
│   │   ├── protocol.h/c        ← 电机通信协议层
│   │   └── serial.h/c          ← Windows串口抽象层 (Win32 API)
│   ├── motion/
│   │   ├── kinematics.h/c      ← 正/逆运动学引擎
│   │   └── controller.h/c      ← 闭环运动控制器 (P控制)
│   └── ui/
│       ├── ui.h                ← LVGL UI接口
│       └── ui.c                ← LVGL + Win32 GDI显示后端
├── lv_conf.h                   ← LVGL v8.x 配置
├── CMakeLists.txt              ← CMake构建文件
├── build.bat                   ← Windows一键构建脚本
└── README.md
```

## 零外部依赖

| 组件 | 实现 | 说明 |
|------|------|------|
| **UI显示** | Win32 GDI (CreateDIBSection) | Windows内置, 无需安装 |
| **UI输入** | Win32 鼠标消息 | Windows内置 |
| **串口通信** | Win32 CreateFile/ReadFile/WriteFile | Windows内置 |
| **GUI框架** | LVGL v8.3 | CMake自动从GitHub下载 |
| **编译器** | MSVC (Visual Studio 2022) 或 MinGW-w64 | 用户已有 |

> **不需要**安装SDL2、vcpkg、MSYS2或任何第三方SDK。

## 硬件要求

- **电机**: 4个正点原子SMD智能电机 (地址 0x01~0x04)
- **通信**: RS485总线 + USB转RS485模块
- **协议**: 自定义帧协议, 115200bps, 8N1

### 电机帧格式

```
┌──────┬──────┬──────┬─────────┬──────┬──────┐
│ HEAD │ ADDR │ CMD  │  DATA   │ CHK  │ TAIL │
│ 0xC5 │ 1B   │ 1B   │ 0~8B    │ 1B   │ 0x5C │
└──────┴──────┴──────┴─────────┴──────┴──────┘
```

### 关键参数

| 参数 | 值 |
|------|-----|
| 编码器分辨率 | 51200 脉冲/圈 |
| 绳长变化 | 30mm/圈 |
| 力矩范围 | 150~1500 mA |
| 控制范围 | ±200mm (X/Y) |
| 控制周期 | 20ms (50Hz) |

## 构建方法

### 前置要求

- **编译器**: Visual Studio 2022 Community (免费) 或 Build Tools
  - 安装时勾选 "使用C++的桌面开发"
  - [下载地址](https://visualstudio.microsoft.com/zh-hans/downloads/)
- **CMake**: VSCode CMake Tools 扩展自带, 或[手动安装](https://cmake.org/download/)
- **网络**: 首次构建需要下载LVGL源码 (~2MB)

### VSCode一键构建 (推荐)

1. 安装 VSCode 扩展: **CMake Tools** + **C/C++**
2. 打开项目文件夹
3. 按 `Ctrl+Shift+P` → "CMake: Configure"
4. 按 `Ctrl+Shift+P` → "CMake: Build"
5. 按 `F5` 运行

### 命令行构建

```cmd
# 在 "Developer Command Prompt for VS" 或
# "x64 Native Tools Command Prompt" 中:

build.bat
```

或手动:

```cmd
cmake -B build
cmake --build build --config Release
```

## 使用方法

```cmd
# 连接硬件
CPDR_ClawMachine.exe COM3

# 离线演示模式 (不连接硬件)
CPDR_ClawMachine.exe
```

操作流程:
1. 点击 **[使能]** 激活所有电机
2. 点击 **[归零]** 将当前位置设为坐标原点 (0, 0)
3. **拖拽虚拟摇杆** 控制夹爪移动到目标位置
4. 点击 **[抓取]** 执行抓取动作
5. 点击 **[急停]** 紧急停止; 关闭窗口退出

## 控制原理

### 绳长计算 (逆运动学)

4根绳索均连接到同一个末端执行器(x,y):

```
L1 = sqrt((x+315)² + (y-315)²)    # M01 → 夹爪
L2 = sqrt((x-315)² + (y-315)²)    # M02 → 夹爪
L3 = sqrt((x-315)² + (y+315)²)    # M03 → 夹爪
L4 = sqrt((x+315)² + (y+315)²)    # M04 → 夹爪
```

### 正运动学 (解析解)

利用绳长平方差消元:
```
x = (L1² - L2²) / 1260 = (L4² - L3²) / 1260
y = (L4² - L1²) / 1260 = (L3² - L2²) / 1260
```
两组公式取平均提高精度。

### 闭环控制架构

```
摇杆输入(x,y) → 逆运动学 → 目标绳长L1~L4 → 对比当前绳长
                                                      ↓
当前位置(x,y) ← 正运动学 ← 编码器反馈 ← 读取4个电机
                                                      ↓
                                          误差 → P控制器 → 力矩 → 电机
```

## 移植到嵌入式

LVGL UI层与硬件控制层解耦:

- `src/ui/` → LVGL UI可直接移植到MCU (仅需更换显示驱动)
- `src/motor/protocol.c` → 协议层完全平台无关
- `src/motor/serial.c` → 替换为MCU串口驱动即可
- `src/motion/` → 纯算法, 完全平台无关
