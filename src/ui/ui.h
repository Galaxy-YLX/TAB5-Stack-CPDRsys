/**
 ******************************************************************************
 * @file        ui.h
 * @brief       纯Win32 GDI用户界面 — 虚拟摇杆、按钮、状态显示
 * @details     零外部依赖, 仅需 Windows SDK + MSVC
 ******************************************************************************
 */
#ifndef __UI_H
#define __UI_H
#include <stdint.h>
#include <stdbool.h>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_EVENT_NONE,
    UI_EVENT_GRAB,
    UI_EVENT_ZERO,
    UI_EVENT_ESTOP,
    UI_EVENT_ENABLE,
    UI_EVENT_DISABLE,
    UI_EVENT_EXIT,
} ui_event_t;

typedef struct { float x; float y; bool active; } joystick_data_t;
typedef void (*ui_tick_cb_t)(void);

bool        ui_init(ui_tick_cb_t tick_cb, HINSTANCE hi);
void        ui_run(void);
void        ui_post_quit(void);

const joystick_data_t* ui_get_joystick(void);
ui_event_t  ui_get_event(void);

void ui_set_status(const char *txt);
void ui_set_position(float x, float y);
void ui_set_target(float x, float y);
void ui_set_motor_online(const uint8_t online[4]);
void ui_set_comm_errors(uint32_t n);

HWND ui_get_hwnd(void);

#ifdef __cplusplus
}
#endif
#endif
