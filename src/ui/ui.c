/**
 ******************************************************************************
 * @file        ui.c
 * @brief       纯Win32 GDI UI — 虚拟摇杆、按钮、状态面板
 *              零外部依赖, GDI双缓冲渲染
 ******************************************************************************
 */
#include "ui.h"
#include <stdio.h>
#include <math.h>

#define SCR_W      800
#define SCR_H      480
#define CLS_NAME   L"CPDR_Claw"

/* 摇杆几何 */
#define J_CX       120     /* 底座中心X */
#define J_CY       230     /* 底座中心Y (面板内) */
#define J_BASE_R   90      /* 底座半径 */
#define J_KNOB_R   28      /* 旋钮半径 */
#define J_RANGE    200.0f  /* mm映射范围 */

/* 按钮区域 */
#define BTN_Y      425
#define BTN_H      42
#define BTN_GAP    10

/* 颜色 */
#define C_BG       RGB(26,26,46)
#define C_CARD     RGB(22,33,62)
#define C_BORDER   RGB(15,52,96)
#define C_ACCENT   RGB(233,69,96)
#define C_GREEN    RGB(0,168,85)
#define C_YELLOW   RGB(255,204,0)
#define C_RED      RGB(255,50,50)
#define C_TEXT     0xCCCCCC
#define C_DIM      0x888888

/* ======================== 全局状态 ======================== */
static HINSTANCE  g_hinst;
static HWND       g_hwnd;
static ui_tick_cb_t g_tick_cb;

/* 双缓冲 */
static HDC        g_mem_dc;
static HBITMAP    g_mem_bmp;
static int        g_fb_w, g_fb_h;

/* 摇杆 */
static joystick_data_t g_joy;
static float      g_knob_dx, g_knob_dy;

/* 显示数据 */
static float      g_pos_x, g_pos_y;
static float      g_target_x, g_target_y;
static uint8_t    g_motors[4];
static uint32_t   g_errors;
static char       g_status[32] = "就绪";

/* 事件队列 */
static ui_event_t g_event;
static ui_event_t g_latched; /* 已消费的最后一个事件 */

/* 控件边界 */
static RECT g_btn_rects[6];
static const wchar_t *g_btn_labels[6] = {
    L"使能", L"失能", L"归零", L"抓取", L"急停", L"退出"
};
static ui_event_t g_btn_events[6] = {
    UI_EVENT_ENABLE, UI_EVENT_DISABLE, UI_EVENT_ZERO,
    UI_EVENT_GRAB,   UI_EVENT_ESTOP,   UI_EVENT_EXIT
};
static COLORREF g_btn_colors[6] = {
    0x00AA55, 0xAA5500, 0x3355CC, 0xE94560, 0xFF0000, 0x555555
};

/* ======================== 前向声明 ======================== */
static LRESULT CALLBACK wndproc(HWND, UINT, WPARAM, LPARAM);
static void draw_all(HDC hdc);
static void draw_joystick(HDC hdc);
static void draw_buttons(HDC hdc);
static void draw_info_panel(HDC hdc);
static void layout_buttons(void);
static int  hit_test_buttons(int mx, int my);
static int  hit_test_joystick(int mx, int my);

/* ======================== 公开 API ======================== */

bool ui_init(ui_tick_cb_t tick_cb, HINSTANCE hi)
{
    g_tick_cb = tick_cb;
    g_hinst   = hi ? hi : GetModuleHandle(NULL);

    /* 注册窗口类 */
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style  = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndproc;
    wc.hInstance   = g_hinst;
    wc.hCursor     = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = CLS_NAME;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;

    /* 计算窗口大小 */
    RECT rc = {0, 0, SCR_W, SCR_H};
    AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

    g_hwnd = CreateWindowExW(0, CLS_NAME, L"CPDR 抓娃娃机控制系统",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, g_hinst, NULL);
    if (!g_hwnd) return false;

    /* 双缓冲 */
    HDC hdc = GetDC(g_hwnd);
    g_mem_dc = CreateCompatibleDC(hdc);
    g_mem_bmp = CreateCompatibleBitmap(hdc, SCR_W, SCR_H);
    SelectObject(g_mem_dc, g_mem_bmp);
    g_fb_w = SCR_W; g_fb_h = SCR_H;
    ReleaseDC(g_hwnd, hdc);

    layout_buttons();

    /* 定时器: 20ms控制周期 */
    SetTimer(g_hwnd, 1, 20, NULL);

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    printf("[UI] Win32 GDI 初始化完成 (%dx%d)\r\n", SCR_W, SCR_H);
    return true;
}

void ui_run(void)
{
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (g_event == UI_EVENT_EXIT) break;
    }
}

void ui_post_quit(void) { PostMessage(g_hwnd, WM_CLOSE, 0, 0); }

const joystick_data_t* ui_get_joystick(void) { return &g_joy; }

ui_event_t ui_get_event(void)
{
    ui_event_t e = g_event;
    if (e != g_latched) { g_latched = e; return e; }
    return UI_EVENT_NONE;
}

void ui_set_status(const char *txt)      { strncpy(g_status, txt, sizeof(g_status)-1); }
void ui_set_position(float x, float y)   { g_pos_x = x; g_pos_y = y; }
void ui_set_target(float x, float y)     { g_target_x = x; g_target_y = y; }
void ui_set_motor_online(const uint8_t o[4]) { memcpy(g_motors, o, 4); }
void ui_set_comm_errors(uint32_t n)       { g_errors = n; }
HWND ui_get_hwnd(void)                    { return g_hwnd; }

/* ======================== 窗口过程 ======================== */

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        return 0;

    case WM_TIMER:
        if (wp == 1 && g_tick_cb) g_tick_cb();
        /* 触发重绘 */
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        draw_all(g_mem_dc);
        BitBlt(hdc, 0, 0, SCR_W, SCR_H, g_mem_dc, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND: return 1;

    case WM_LBUTTONDOWN: {
        int mx = LOWORD(lp), my = HIWORD(lp);
        int bi = hit_test_buttons(mx, my);
        if (bi >= 0) { g_event = g_btn_events[bi]; printf("[UI] 按钮: %d\r\n", (int)g_event); return 0; }
        if (hit_test_joystick(mx, my)) {
            g_joy.active = true;
            SetCapture(hwnd);
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (!g_joy.active) break;
        int mx = LOWORD(lp), my = HIWORD(lp);
        float dx = (float)(mx - J_CX), dy = (float)(my - J_CY);
        float dist = sqrtf(dx*dx + dy*dy);
        float max_r = (float)(J_BASE_R - J_KNOB_R);
        if (dist > max_r) { dx *= max_r/dist; dy *= max_r/dist; }
        g_knob_dx = dx; g_knob_dy = dy;
        g_joy.x =  (dx / max_r) * J_RANGE;
        g_joy.y = -(dy / max_r) * J_RANGE;
        return 0;
    }

    case WM_LBUTTONUP:
        if (g_joy.active) {
            ReleaseCapture();
            g_joy.active = false;
            g_joy.x = g_joy.y = 0;
            g_knob_dx = g_knob_dy = 0;
        }
        return 0;

    case WM_CLOSE:
        g_event = UI_EVENT_EXIT;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        if (g_mem_bmp) { DeleteObject(g_mem_bmp); g_mem_bmp = NULL; }
        if (g_mem_dc)  { DeleteDC(g_mem_dc); g_mem_dc = NULL; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ======================== 布局计算 ======================== */

static void layout_buttons(void)
{
    const int N = 6, BW = 105;
    int total_w = N * BW + (N - 1) * BTN_GAP;
    int x0 = (SCR_W - total_w) / 2;
    for (int i = 0; i < N; i++) {
        g_btn_rects[i].left   = x0 + i * (BW + BTN_GAP);
        g_btn_rects[i].top    = BTN_Y;
        g_btn_rects[i].right  = g_btn_rects[i].left + BW;
        g_btn_rects[i].bottom = BTN_Y + BTN_H;
    }
}

static int hit_test_buttons(int mx, int my)
{
    for (int i = 0; i < 6; i++)
        if (mx >= g_btn_rects[i].left && mx < g_btn_rects[i].right &&
            my >= g_btn_rects[i].top  && my < g_btn_rects[i].bottom)
            return i;
    return -1;
}

static int hit_test_joystick(int mx, int my)
{
    int dx = mx - J_CX, dy = my - J_CY;
    return (dx*dx + dy*dy) <= J_BASE_R * J_BASE_R;
}

/* ======================== 绘制 ======================== */

static void fill_rect(HDC hdc, int x, int y, int w, int h, COLORREF c)
{
    RECT rc = {x, y, x+w, y+h};
    HBRUSH br = CreateSolidBrush(c);
    FillRect(hdc, &rc, br);
    DeleteObject(br);
}

static void draw_text(HDC hdc, int x, int y, const wchar_t *txt, COLORREF c, int sz, bool bold)
{
    HFONT f = CreateFontW(sz, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
                           FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SetTextColor(hdc, c);
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, f);
    TextOutW(hdc, x, y, txt, (int)wcslen(txt));
    DeleteObject(f);
}

static void draw_text_ascii(HDC hdc, int x, int y, const char *txt, COLORREF c, int sz)
{
    HFONT f = CreateFontA(sz, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           DEFAULT_QUALITY, FIXED_PITCH, "Consolas");
    SetTextColor(hdc, c);
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, f);
    TextOutA(hdc, x, y, txt, (int)strlen(txt));
    DeleteObject(f);
}

static void draw_all(HDC hdc)
{
    /* 背景 */
    fill_rect(hdc, 0, 0, SCR_W, SCR_H, C_BG);

    /* 顶栏 */
    fill_rect(hdc, 0, 0, SCR_W, 36, C_CARD);
    draw_text(hdc, 12, 6, L"CPDR 抓娃娃机控制系统", C_ACCENT, 18, TRUE);

    /* 状态文字 */
    {
        wchar_t buf[32];
        int len = (int)strlen(g_status);
        for (int i = 0; i < len; i++) buf[i] = (wchar_t)(unsigned char)g_status[i];
        buf[len] = 0;
        draw_text(hdc, SCR_W - 140, 8, buf, C_BORDER, 14, TRUE);
    }

    draw_joystick(hdc);
    draw_info_panel(hdc);
    draw_buttons(hdc);
}

static void draw_joystick(HDC hdc)
{
    int cx = J_CX, cy = J_CY;
    RECT area = {cx - J_BASE_R - 10, cy - J_BASE_R - 30, cx + J_BASE_R + 10, cy + J_BASE_R + 10};
    fill_rect(hdc, area.left, area.top, area.right-area.left, area.bottom-area.top, C_BG);

    draw_text(hdc, cx - 40, area.top + 2, L"摇杆控制", C_DIM, 13, FALSE);

    /* 底座 */
    HPEN pen = CreatePen(PS_SOLID, 2, C_BORDER);
    SelectObject(hdc, pen);
    HBRUSH br = CreateSolidBrush(C_CARD);
    SelectObject(hdc, br);
    Ellipse(hdc, cx - J_BASE_R, cy - J_BASE_R, cx + J_BASE_R, cy + J_BASE_R);
    DeleteObject(br);
    DeleteObject(pen);

    /* 十字线 */
    pen = CreatePen(PS_SOLID, 1, RGB(50,50,60));
    SelectObject(hdc, pen);
    MoveToEx(hdc, cx - J_BASE_R + 15, cy, NULL); LineTo(hdc, cx + J_BASE_R - 15, cy);
    MoveToEx(hdc, cx, cy - J_BASE_R + 15, NULL); LineTo(hdc, cx, cy + J_BASE_R - 15);
    DeleteObject(pen);

    /* 旋钮 */
    int kx = cx + (int)g_knob_dx, ky = cy + (int)g_knob_dy;
    br = CreateSolidBrush(C_ACCENT);
    SelectObject(hdc, GetStockObject(NULL_PEN));
    SelectObject(hdc, br);
    Ellipse(hdc, kx - J_KNOB_R, ky - J_KNOB_R, kx + J_KNOB_R, ky + J_KNOB_R);
    DeleteObject(br);

    /* 数值 */
    char buf[64];
    snprintf(buf, sizeof(buf), "X: %6.1f mm  Y: %6.1f mm", g_joy.x, g_joy.y);
    draw_text_ascii(hdc, cx - 95, cy + J_BASE_R + 12, buf, C_DIM, 12);
}

static void draw_info_panel(HDC hdc)
{
    int px = 300, py = 55, pw = 480, ph = 340;

    /* 面板背景 */
    HPEN pen = CreatePen(PS_SOLID, 1, C_BORDER);
    SelectObject(hdc, pen);
    HBRUSH br = CreateSolidBrush(C_CARD);
    SelectObject(hdc, br);
    RoundRect(hdc, px, py, px+pw, py+ph, 10, 10);
    DeleteObject(br);
    DeleteObject(pen);

    int y = py + 12;

    /* 当前位置 */
    draw_text(hdc, px+14, y, L"当前位置", C_DIM, 12, FALSE);
    y += 20;
    char buf[64];
    snprintf(buf, sizeof(buf), "X: %6.1f mm  Y: %6.1f mm", g_pos_x, g_pos_y);
    draw_text_ascii(hdc, px+14, y, buf, C_GREEN, 16);
    y += 26;

    /* 目标位置 */
    draw_text(hdc, px+14, y, L"目标位置", C_DIM, 12, FALSE);
    y += 20;
    snprintf(buf, sizeof(buf), "X: %6.1f mm  Y: %6.1f mm", g_target_x, g_target_y);
    draw_text_ascii(hdc, px+14, y, buf, C_YELLOW, 16);
    y += 28;

    /* 电机状态 */
    draw_text(hdc, px+14, y, L"电机状态", C_DIM, 12, FALSE);
    y += 20;
    for (int i = 0; i < 4; i++) {
        COLORREF dot = g_motors[i] ? C_GREEN : C_RED;
        br = CreateSolidBrush(dot);
        SelectObject(hdc, br);
        SelectObject(hdc, GetStockObject(NULL_PEN));
        Ellipse(hdc, px+14+i*90, y+2, px+22+i*90, y+10);
        DeleteObject(br);
        wchar_t lbl[16];
        swprintf(lbl, 16, L"M%d %s", i+1, g_motors[i] ? L"在线" : L"离线");
        draw_text(hdc, px+28+i*90, y-2, lbl, g_motors[i] ? C_GREEN : C_RED, 11, FALSE);
    }
    y += 24;

    /* 通信 */
    snprintf(buf, sizeof(buf), "通信错误: %u", (unsigned)g_errors);
    draw_text_ascii(hdc, px+14, y, buf, C_RED, 12);
    y += 22;

    /* 状态 */
    wchar_t wstatus[32];
    int slen = (int)strlen(g_status);
    for (int i = 0; i < slen; i++) wstatus[i] = (wchar_t)(unsigned char)g_status[i];
    wstatus[slen] = 0;
    draw_text(hdc, px+14, y, wstatus, C_BORDER, 13, TRUE);
    y += 28;

    /* 分隔线 */
    pen = CreatePen(PS_SOLID, 1, C_BORDER);
    SelectObject(hdc, pen);
    MoveToEx(hdc, px+14, y, NULL); LineTo(hdc, px+pw-14, y);
    DeleteObject(pen);
    y += 10;

    /* 说明 */
    draw_text(hdc, px+14, y,   L"操作说明:", C_DIM, 11, FALSE);
    y += 16;
    draw_text(hdc, px+14, y,   L"• 拖拽 摇杆 → 移动夹爪 (X/Y 平面)", C_DIM, 11, FALSE);
    y += 16;
    draw_text(hdc, px+14, y,   L"• [归零] → 设置当前位置为坐标原点", C_DIM, 11, FALSE);
    y += 16;
    draw_text(hdc, px+14, y,   L"• [抓取] → 执行抓取动作", C_DIM, 11, FALSE);
    y += 16;
    draw_text(hdc, px+14, y,   L"• [急停] → 紧急停止, 关闭窗口退出", C_DIM, 11, FALSE);
    y += 16;
    draw_text(hdc, px+14, y,   L"纯 Win32 GDI — 零外部依赖, 仅需 MSVC 编译器", C_DIM, 11, FALSE);
}

static void draw_buttons(HDC hdc)
{
    for (int i = 0; i < 6; i++) {
        RECT r = g_btn_rects[i];
        HBRUSH br = CreateSolidBrush(g_btn_colors[i]);
        SelectObject(hdc, br);
        SelectObject(hdc, GetStockObject(NULL_PEN));
        RoundRect(hdc, r.left, r.top, r.right, r.bottom, 8, 8);
        DeleteObject(br);

        /* 文字居中 */
        SIZE sz;
        HFONT f = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
        SelectObject(hdc, f);
        GetTextExtentPoint32W(hdc, g_btn_labels[i], (int)wcslen(g_btn_labels[i]), &sz);
        int tx = r.left + (r.right - r.left - sz.cx) / 2;
        int ty = r.top  + (r.bottom - r.top - sz.cy) / 2;
        SetTextColor(hdc, RGB(255,255,255));
        SetBkMode(hdc, TRANSPARENT);
        TextOutW(hdc, tx, ty, g_btn_labels[i], (int)wcslen(g_btn_labels[i]));
        DeleteObject(f);
    }
}
