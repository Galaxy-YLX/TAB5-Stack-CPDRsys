cmake_minimum_required(VERSION 3.16)
project(CPDR_ClawMachine VERSION 2.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ============================================================
# 源文件 — 纯 Win32 GDI, 零外部依赖
# 编译: cmake -B build && cmake --build build --config Release
# ============================================================
set(PROJECT_SOURCES
    src/main.c
    src/motor/protocol.c
    src/motor/serial.c
    src/motion/kinematics.c
    src/motion/controller.c
    src/ui/ui.c
)

add_executable(${PROJECT_NAME} WIN32 ${PROJECT_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Windows 系统库 (无需安装, OS 自带)
target_link_libraries(${PROJECT_NAME} PRIVATE gdi32 user32 setupapi)

# 保留控制台用于日志输出
set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE FALSE)

message(STATUS "CPDR v2.0 — 纯 Win32 GDI, 零外部依赖, 仅需 MSVC")
