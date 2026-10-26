/**
 ******************************************************************************
 * @file        main.c
 * @brief       CPDR 抓娃娃机 — Win32 主程序 (零外部依赖)
 * @details     单线程 Win32 消息驱动, 纯 GDI UI, 仅需 MSVC + Windows SDK
 *     使用: CPDR_ClawMachine.exe [COM3]
 ******************************************************************************
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <windows.h>
#include "motor/protocol.h"
#include "motor/serial.h"
#include "motion/kinematics.h"
#include "motion/controller.h"
#include "ui/ui.h"

static motion_controller_t g_ctrl;
static serial_handle_t     g_serial    = NULL;
static bool                g_estop     = false;
static bool                g_motors_on = false;

/* 控制回调 (UI WM_TIMER 每20ms调用) */
static void control_tick(void)
{
    const joystick_data_t *joy = ui_get_joystick();

    /* 处理按钮事件 */
    ui_event_t evt = ui_get_event();
    switch (evt) {
    case UI_EVENT_ENABLE:
        printf("[MAIN] 使能\r\n");
        if (g_serial) motion_ctrl_enable_motors(&g_ctrl);
        g_motors_on = true; g_estop = false;
        ui_set_status("已使能");
        break;
    case UI_EVENT_DISABLE:
        printf("[MAIN] 失能\r\n");
        if (g_serial) motion_ctrl_disable_motors(&g_ctrl);
        g_motors_on = false;
        ui_set_status("已失能");
        break;
    case UI_EVENT_ZERO:
        printf("[MAIN] 归零\r\n");
        motion_ctrl_trigger_zero(&g_ctrl);
        ui_set_status("已归零");
        break;
    case UI_EVENT_GRAB:
        printf("[MAIN] 抓取\r\n");
        if (g_serial) motion_ctrl_trigger_grab(&g_ctrl);
        ui_set_status("抓取完成");
        break;
    case UI_EVENT_ESTOP:
        printf("[MAIN] 急停\r\n");
        if (g_serial) motion_ctrl_emergency_stop(&g_ctrl);
        g_estop = true; g_motors_on = false;
        ui_set_status("急停!");
        break;
    case UI_EVENT_EXIT:
        printf("[MAIN] 退出\r\n");
        ui_post_quit();
        return;
    default: break;
    }

    if (g_estop) { ui_set_status("急停!"); return; }

    /* 摇杆 → 运动目标 */
    if (joy->active) {
        motion_ctrl_set_target(&g_ctrl, joy->x, joy->y);
        ui_set_target(joy->x, joy->y);
    } else {
        float cx, cy;
        motion_ctrl_get_position(&g_ctrl, &cx, &cy);
        ui_set_target(cx, cy);
    }

    /* 控制循环 */
    motion_state_t st = motion_ctrl_tick(&g_ctrl);

    /* 更新显示 */
    float cx, cy;
    motion_ctrl_get_position(&g_ctrl, &cx, &cy);
    ui_set_position(cx, cy);

    uint8_t online[4];
    motion_ctrl_get_motor_online(&g_ctrl, online);
    ui_set_motor_online(online);
    ui_set_comm_errors(motion_ctrl_get_comm_errors(&g_ctrl));

    switch (st) {
    case MOTION_STATE_IDLE:    ui_set_status(g_motors_on ? "就绪" : "离线演示"); break;
    case MOTION_STATE_MOVING:  ui_set_status(g_motors_on ? "运动中" : "离线演示"); break;
    case MOTION_STATE_HOLDING: ui_set_status("保持"); break;
    case MOTION_STATE_GRABBING:ui_set_status("抓取中"); break;
    case MOTION_STATE_ERROR:   ui_set_status("通信错误"); break;
    case MOTION_STATE_ESTOP:   ui_set_status("急停!"); break;
    }
}

int main(int argc, char *argv[])
{
    /* 控制台 */
    AllocConsole();
    FILE *f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);

    printf("==============================================================\r\n");
    printf("  CPDR 抓娃娃机 v2.0 — Win32 GDI (零外部依赖)\r\n");
    printf("==============================================================\r\n\r\n");

    const char *port = SERIAL_DEFAULT_PORT;
    if (argc > 1 && argv[1][0]) port = argv[1];

    serial_list_ports();
    printf("\r\n[MAIN] 串口: %s @ %d bps\r\n", port, SERIAL_DEFAULT_BAUD);

    g_serial = serial_open(port, SERIAL_DEFAULT_BAUD);
    if (!g_serial) printf("[MAIN] 离线模式 (无硬件)\r\n");

    motion_ctrl_init(&g_ctrl, g_serial);

    if (!ui_init(control_tick, GetModuleHandle(NULL))) {
        printf("[MAIN] UI 初始化失败!\r\n");
        if (g_serial) serial_close(g_serial);
        FreeConsole();
        return 1;
    }

    printf("[MAIN] 启动完成 — 使能→归零→摇杆→抓取\r\n");
    printf("[MAIN] 关闭窗口退出\r\n\r\n");

    /* Win32 消息循环 (阻塞) */
    ui_run();

    /* 清理 */
    printf("\r\n[MAIN] 关闭中...\r\n");
    if (g_motors_on && g_serial) motion_ctrl_disable_motors(&g_ctrl);
    if (g_serial) serial_close(g_serial);
    printf("[MAIN] 已关闭\r\n");
    FreeConsole();
    return 0;
}
