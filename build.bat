@echo off
REM ================================================================
REM  CPDR 抓娃娃机控制系统 — Windows构建脚本
REM
REM  前置要求 (仅需以下之一):
REM   - Visual Studio 2022 (Community版免费)
REM     下载: https://visualstudio.microsoft.com/zh-hans/downloads/
REM     安装时勾选 "使用C++的桌面开发" 工作负载
REM   - 或 Visual Studio Build Tools + VSCode C++扩展
REM   - CMake (VSCode CMake Tools扩展自带, 或手动安装)
REM
REM  零外部库依赖 — LVGL由CMake自动下载
REM  UI使用Win32 GDI (Windows内置, 无需安装任何SDK)
REM ================================================================

setlocal enabledelayedexpansion

echo ================================================================
echo   CPDR 抓娃娃机 — 构建系统 (Win32 GDI)
echo ================================================================
echo.

REM --- 检查CMake ---
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [提示] 未在PATH中找到cmake, 尝试通过VSCode CMake Tools扩展...
    echo   如果已安装VSCode + CMake Tools扩展, 请按 Ctrl+Shift+P
    echo   运行 "CMake: Build" 即可
    echo.
    echo   或安装独立CMake: https://cmake.org/download/
    pause
    exit /b 1
)

REM --- 检查编译器 ---
set USE_MSVC=0
where cl >nul 2>&1
if %errorlevel% equ 0 set USE_MSVC=1

set USE_MINGW=0
where gcc >nul 2>&1
if %errorlevel% equ 0 set USE_MINGW=1

if %USE_MSVC% equ 1 (
    echo [检测] Visual Studio C++ (MSVC^)
    set GENERATOR=Visual Studio 17 2022
    set BUILD_CMD=cmake --build build --config Release
) else if %USE_MINGW% equ 1 (
    echo [检测] MinGW-w64 GCC
    set GENERATOR=MinGW Makefiles
    set BUILD_CMD=cmake --build build
) else (
    echo.
    echo [错误] 未检测到编译器!
    echo.
    echo 请安装以下之一:
    echo   1. Visual Studio Build Tools (推荐^)
    echo      https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022
    echo      安装时勾选 "MSVC v143" 和 "Windows 10 SDK"
    echo.
    echo   2. 完整Visual Studio Community (免费^)
    echo      https://visualstudio.microsoft.com/zh-hans/downloads/
    echo.
    echo 安装后打开 "Developer Command Prompt for VS" 或
    echo "x64 Native Tools Command Prompt" 再运行此脚本
    pause
    exit /b 1
)

REM --- 清理 ---
if exist "build" (
    echo [清理] 删除旧构建...
    rmdir /s /q build
)

REM --- CMake配置 ---
echo.
echo [配置] CMake (LVGL将自动从GitHub下载)...
echo.

cmake -B build -G %GENERATOR%

if %errorlevel% neq 0 (
    echo.
    echo [错误] CMake配置失败!
    echo.
    echo 可能的原因:
    echo   1. 网络不可用 (CMake需要下载LVGL源码^)
    echo      → 手动下载LVGL到 lvgl\ 目录, 然后:
    echo        cmake -B build -G %GENERATOR% -DFETCH_LVGL=OFF
    echo.
    echo   2. 编译器问题
    echo      → 确认在正确的命令行环境中运行
    pause
    exit /b 1
)

REM --- 编译 ---
echo.
echo [编译] 正在编译...
%BUILD_CMD%

if %errorlevel% neq 0 (
    echo [错误] 编译失败!
    pause
    exit /b 1
)

echo.
echo ================================================================
echo   构建成功! 零外部依赖 — 仅Win32 API + LVGL
echo ================================================================
echo.
echo   输出文件:
if exist "build\Release\CPDR_ClawMachine.exe" (
    echo     build\Release\CPDR_ClawMachine.exe
    echo.
    echo 运行: build\Release\CPDR_ClawMachine.exe [COM口]
)
if exist "build\CPDR_ClawMachine.exe" (
    echo     build\CPDR_ClawMachine.exe
    echo.
    echo 运行: build\CPDR_ClawMachine.exe [COM口]
)
echo   例如: CPDR_ClawMachine.exe COM3

pause
endlocal
