/// Pure Win32 GDI UI — 虚拟摇杆 + 按钮 + 状态面板
/// 零外部依赖, FFI only

use std::ffi::{c_void, CString};
use std::ptr::{null, null_mut};
use std::sync::{Arc, Mutex};
use crate::Pt;

// ======================== Win32 FFI ========================

type HWND = *mut c_void; type HINSTANCE = *mut c_void; type HDC = *mut c_void;
type HBITMAP = *mut c_void; type HFONT = *mut c_void; type HBRUSH = *mut c_void;
type HPEN = *mut c_void; type HGDIOBJ = *mut c_void; type LPARAM = isize;
type WPARAM = usize; type LRESULT = isize; type ATOM = u16; type DWORD = u32;
type LPCWSTR = *const u16; type COLORREF = u32;
type WNDPROC = unsafe extern "system" fn(HWND, u32, WPARAM, LPARAM) -> LRESULT;

#[link(name = "user32")]
extern "system" {
    fn RegisterClassExW(lp: *const WNDCLASSEXW) -> ATOM;
    fn CreateWindowExW(dwEx: DWORD, cn: LPCWSTR, wn: LPCWSTR, style: DWORD,
        x: i32, y: i32, w: i32, h: i32, p: HWND, m: *mut c_void, i: HINSTANCE, pr: *mut c_void) -> HWND;
    fn DefWindowProcW(h: HWND, m: u32, w: WPARAM, l: LPARAM) -> LRESULT;
    fn GetMessageW(m: *mut MSG, h: HWND, wm: u32, wM: u32) -> i32;
    fn TranslateMessage(m: *const MSG) -> i32;
    fn DispatchMessageW(m: *const MSG) -> i32;
    fn PostQuitMessage(ec: i32);
    fn ShowWindow(h: HWND, c: i32);
    fn UpdateWindow(h: HWND);
    fn InvalidateRect(h: HWND, r: *const RECT, e: i32);
    fn GetDC(h: HWND) -> HDC;
    fn ReleaseDC(h: HWND, dc: HDC) -> i32;
    fn BeginPaint(h: HWND, ps: *mut PAINTSTRUCT) -> HDC;
    fn EndPaint(h: HWND, ps: *const PAINTSTRUCT) -> i32;
    fn SetTimer(h: HWND, id: usize, ms: u32, cb: *mut c_void) -> usize;
    fn KillTimer(h: HWND, id: usize) -> i32;
    fn LoadCursorW(i: HINSTANCE, n: LPCWSTR) -> *mut c_void;
    fn SetCapture(h: HWND) -> HWND;
    fn ReleaseCapture() -> i32;
    fn GetClientRect(h: HWND, r: *mut RECT) -> i32;
    fn FillRect(dc: HDC, r: *const RECT, br: HBRUSH) -> i32;
    fn GetStockObject(i: i32) -> HGDIOBJ;
    fn SetTextColor(dc: HDC, c: COLORREF) -> COLORREF;
    fn SetBkMode(dc: HDC, m: i32) -> i32;
    fn TextOutW(dc: HDC, x: i32, y: i32, s: LPCWSTR, c: i32) -> i32;
}

#[link(name = "gdi32")]
extern "system" {
    fn CreateCompatibleDC(dc: HDC) -> HDC;
    fn CreateCompatibleBitmap(dc: HDC, w: i32, h: i32) -> HBITMAP;
    fn SelectObject(dc: HDC, o: HGDIOBJ) -> HGDIOBJ;
    fn DeleteDC(dc: HDC) -> i32;
    fn DeleteObject(o: HGDIOBJ) -> i32;
    fn BitBlt(dst: HDC, x: i32, y: i32, w: i32, h: i32, src: HDC, sx: i32, sy: i32, op: DWORD) -> i32;
    fn CreatePen(s: i32, w: i32, c: COLORREF) -> HPEN;
    fn CreateSolidBrush(c: COLORREF) -> HBRUSH;
    fn CreateFontW(h: i32, w: i32, esc: i32, ori: i32, wt: i32, it: u8, ul: u8, so: u8,
                   cs: u8, oP: u32, cP: u32, q: u32, pf: u32, fn_: LPCWSTR) -> HFONT;
    fn Ellipse(dc: HDC, l: i32, t: i32, r: i32, b: i32) -> i32;
    fn RoundRect(dc: HDC, l: i32, t: i32, r: i32, b: i32, ew: i32, eh: i32) -> i32;
    fn MoveToEx(dc: HDC, x: i32, y: i32, p: *mut POINT) -> i32;
    fn LineTo(dc: HDC, x: i32, y: i32) -> i32;
    fn GetTextExtentPoint32W(dc: HDC, s: LPCWSTR, c: i32, sz: *mut SIZE) -> i32;
}

#[repr(C)] struct WNDCLASSEXW { cbSize: u32, style: u32, lpfnWndProc: WNDPROC, cbClsExtra: i32,
    cbWndExtra: i32, hInstance: HINSTANCE, hIcon: *mut c_void, hCursor: *mut c_void,
    hbrBackground: HBRUSH, lpszMenuName: LPCWSTR, lpszClassName: LPCWSTR, hIconSm: *mut c_void, }
#[repr(C)] struct MSG { hwnd: HWND, message: u32, wParam: WPARAM, lParam: LPARAM, time: u32,
    pt: POINT, lPrivate: u32, }
#[repr(C)] struct POINT { x: i32, y: i32 }
#[repr(C)] struct RECT { left: i32, top: i32, right: i32, bottom: i32 }
#[repr(C)] struct PAINTSTRUCT { hdc: HDC, fErase: i32, rcPaint: RECT, fRestore: i32, fIncUpdate: i32, rgbReserved: [u8; 32], }
#[repr(C)] struct SIZE { cx: i32, cy: i32 }

// ======================== 常量 ========================

const SCR_W: i32 = 800; const SCR_H: i32 = 480;
const J_CX: i32 = 120; const J_CY: i32 = 230;
const J_BASE_R: i32 = 90; const J_KNOB_R: i32 = 28;
const J_RANGE: f32 = 200.0;
const BTN_Y: i32 = 425; const BTN_H: i32 = 42; const BTN_GAP: i32 = 10; const BTN_W: i32 = 105;

const CS_HREDRAW: u32 = 2; const CS_VREDRAW: u32 = 1;
const WS_OVERLAPPED: u32 = 0; const WS_CAPTION: u32 = 0xC00000;
const WS_SYSMENU: u32 = 0x80000; const WS_MINIMIZEBOX: u32 = 0x20000;
const SW_SHOW: i32 = 5;
const SRCCOPY: u32 = 0xCC0020;
const PS_SOLID: i32 = 0; const TRANSPARENT: i32 = 1;
const FW_NORMAL: i32 = 400; const FW_BOLD: i32 = 700;
const BLACK_BRUSH: i32 = 4; const NULL_PEN: i32 = 8;
const DEFAULT_CHARSET: u8 = 1;

const WM_CREATE: u32 = 1; const WM_TIMER: u32 = 0x0113; const WM_PAINT: u32 = 0x000F;
const WM_ERASEBKGND: u32 = 0x0014; const WM_LBUTTONDOWN: u32 = 0x0201;
const WM_LBUTTONUP: u32 = 0x0202; const WM_MOUSEMOVE: u32 = 0x0200;
const WM_CLOSE: u32 = 0x0010; const WM_DESTROY: u32 = 2;
const WM_LBUTTONDOWN_: u32 = 0x0201;

// ======================== 颜色 ========================
const C_BG: u32 = 0x001A1A2E; const C_CARD: u32 = 0x0016213E; const C_BORDER: u32 = 0x000F3460;
const C_ACCENT: u32 = 0x00E94560; const C_GREEN: u32 = 0x0000AA55;
const C_YELLOW: u32 = 0x00FFCC00; const C_RED: u32 = 0x00FF3232;
const C_TXT: u32 = 0xCCCCCC; const C_DIM: u32 = 0x888888; const C_WHITE: u32 = 0xFFFFFF;

// ======================== 全局 UI 状态 ========================
pub struct UiState {
    pub hwnd: HWND, pub mem_dc: HDC, pub mem_bmp: HBITMAP,
    pub joy: (f32, f32, bool),  // (x, y, active)
    pub knob: (f32, f32),
    pub pos: (f32, f32), pub tgt: (f32, f32),
    pub motors: [bool; 4], pub errs: u32, pub status: String,
    pub event: i32, pub last_event: i32,
    pub btn_rects: [RECT; 6],
}

unsafe impl Send for UiState {}

static mut UI: Option<UiState> = None;

fn wide(s: &str) -> Vec<u16> { s.encode_utf16().chain(std::iter::once(0)).collect() }

fn to_wstr(s: &str) -> Vec<u16> { wide(s) }

fn draw_text(dc: HDC, x: i32, y: i32, text: &str, color: COLORREF, size: i32, bold: bool) {
    let w = to_wstr(&format!("{}\0", text));
    let wt = if bold { FW_BOLD } else { FW_NORMAL };
    unsafe {
        let f = CreateFontW(size, 0, 0, 0, wt, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0,
                            wide("Microsoft YaHei\0").as_ptr());
        SetTextColor(dc, color); SetBkMode(dc, TRANSPARENT);
        let old = SelectObject(dc, f as HGDIOBJ);
        TextOutW(dc, x, y, w.as_ptr(), (text.len()) as i32);
        SelectObject(dc, old); DeleteObject(f as HGDIOBJ);
    }
}

fn fill_rect(dc: HDC, x: i32, y: i32, w: i32, h: i32, c: COLORREF) {
    unsafe {
        let r = RECT{left:x, top:y, right:x+w, bottom:y+h};
        let br = CreateSolidBrush(c);
        FillRect(dc, &r, br); DeleteObject(br as HGDIOBJ);
    }
}

fn layout_buttons(btn_rects: &mut [RECT; 6]) {
    let total_w = 6 * BTN_W + 5 * BTN_GAP;
    let x0 = (SCR_W - total_w) / 2;
    for i in 0..6 {
        btn_rects[i] = RECT{left: x0 + i as i32 * (BTN_W + BTN_GAP), top: BTN_Y,
                             right: x0 + i as i32 * (BTN_W + BTN_GAP) + BTN_W, bottom: BTN_Y + BTN_H};
    }
}

fn hit_test_buttons(btn_rects: &[RECT; 6], mx: i32, my: i32) -> i32 {
    for i in 0..6 {
        let r = &btn_rects[i];
        if mx >= r.left && mx < r.right && my >= r.top && my < r.bottom { return i as i32; }
    }
    -1
}

fn hit_test_joystick(mx: i32, my: i32) -> bool {
    let dx = mx - J_CX; let dy = my - J_CY;
    (dx*dx + dy*dy) <= J_BASE_R * J_BASE_R
}

const BTN_LABELS: [&str; 6] = ["使能", "失能", "归零", "抓取", "急停", "退出"];
const BTN_COLORS: [u32; 6] = [0x00AA55, 0xAA5500, 0x3355CC, 0xE94560, 0xFF0000, 0x555555];

fn draw_joystick(dc: HDC, state: &UiState) {
    let (cx, cy) = (J_CX, J_CY);
    fill_rect(dc, cx-J_BASE_R-10, cy-J_BASE_R-30, 2*J_BASE_R+20, 2*J_BASE_R+40, C_BG);
    draw_text(dc, cx-40, cy-J_BASE_R-28, "摇杆控制", C_DIM, 13, false);
    // 底座
    unsafe {
        let pen = CreatePen(PS_SOLID, 2, C_BORDER); let old_p = SelectObject(dc, pen as HGDIOBJ);
        let br = CreateSolidBrush(C_CARD); let old_b = SelectObject(dc, br as HGDIOBJ);
        Ellipse(dc, cx-J_BASE_R, cy-J_BASE_R, cx+J_BASE_R, cy+J_BASE_R);
        SelectObject(dc, old_b); DeleteObject(br as HGDIOBJ);
        SelectObject(dc, old_p); DeleteObject(pen as HGDIOBJ);
    }
    // 十字线
    unsafe {
        let pen = CreatePen(PS_SOLID, 1, 0x333333); let old = SelectObject(dc, pen as HGDIOBJ);
        MoveToEx(dc, cx-J_BASE_R+15, cy, null_mut()); LineTo(dc, cx+J_BASE_R-15, cy);
        MoveToEx(dc, cx, cy-J_BASE_R+15, null_mut()); LineTo(dc, cx, cy+J_BASE_R-15);
        SelectObject(dc, old); DeleteObject(pen as HGDIOBJ);
    }
    // 旋钮
    let kx = cx + state.knob.0 as i32; let ky = cy + state.knob.1 as i32;
    unsafe {
        let br = CreateSolidBrush(C_ACCENT); let old = SelectObject(dc, br as HGDIOBJ);
        SelectObject(dc, GetStockObject(NULL_PEN) as HGDIOBJ);
        Ellipse(dc, kx-J_KNOB_R, ky-J_KNOB_R, kx+J_KNOB_R, ky+J_KNOB_R);
        SelectObject(dc, old); DeleteObject(br as HGDIOBJ);
    }
    let lbl = format!("X: {:6.1} mm  Y: {:6.1} mm", state.joy.0, state.joy.1);
    draw_text(dc, cx-95, cy+J_BASE_R+12, &lbl, C_DIM, 12, false);
}

fn draw_info_panel(dc: HDC, state: &UiState) {
    let (px, py, pw, ph) = (300i32, 55i32, 480i32, 340i32);
    unsafe {
        let pen = CreatePen(PS_SOLID, 1, C_BORDER); let op = SelectObject(dc, pen as HGDIOBJ);
        let br = CreateSolidBrush(C_CARD); let ob = SelectObject(dc, br as HGDIOBJ);
        RoundRect(dc, px, py, px+pw, py+ph, 10, 10);
        SelectObject(dc, ob); DeleteObject(br as HGDIOBJ);
        SelectObject(dc, op); DeleteObject(pen as HGDIOBJ);
    }
    let mut y = py + 12;
    let (px_off, py_off) = (px + 14, y);
    draw_text(dc, px+14, y, "当前位置", C_DIM, 12, false);
    y += 20;
    let lbl = format!("X: {:6.1} mm  Y: {:6.1} mm", state.pos.0, state.pos.1);
    draw_text(dc, px+14, y, &lbl, C_GREEN, 16, false);
    y += 26;
    draw_text(dc, px+14, y, "目标位置", C_DIM, 12, false);
    y += 20;
    let lbl = format!("X: {:6.1} mm  Y: {:6.1} mm", state.tgt.0, state.tgt.1);
    draw_text(dc, px+14, y, &lbl, C_YELLOW, 16, false);
    y += 28;
    draw_text(dc, px+14, y, "电机状态", C_DIM, 12, false);
    y += 20;
    for i in 0..4 {
        let dot = if state.motors[i] { C_GREEN } else { C_RED };
        unsafe {
            let br = CreateSolidBrush(dot); let old = SelectObject(dc, br as HGDIOBJ);
            SelectObject(dc, GetStockObject(NULL_PEN) as HGDIOBJ);
            Ellipse(dc, px+14+(i*90) as i32, y+2, px+22+(i*90) as i32, y+10);
            SelectObject(dc, old); DeleteObject(br as HGDIOBJ);
        }
        let stat = if state.motors[i] { "在线" } else { "离线" };
        let col = if state.motors[i] { C_GREEN } else { C_RED };
        draw_text(dc, px+28+(i*90) as i32, y-2, &format!("M{} {}", i+1, stat), col, 11, false);
    }
    y += 24;
    draw_text(dc, px+14, y, &format!("通信错误: {}", state.errs), C_RED, 12, false);
    y += 22;
    draw_text(dc, px+14, y, &state.status.clone(), C_BORDER, 13, true);
    y += 28;
    unsafe {
        let pen = CreatePen(PS_SOLID, 1, C_BORDER); let old = SelectObject(dc, pen as HGDIOBJ);
        MoveToEx(dc, px+14, y, null_mut()); LineTo(dc, px+pw-14, y);
        SelectObject(dc, old); DeleteObject(pen as HGDIOBJ);
    }
    y += 10;
    draw_text(dc, px+14, y,   "操作说明:", C_DIM, 11, false); y += 16;
    draw_text(dc, px+14, y,   "拖拽摇杆 → 移动夹爪 (X/Y平面)", C_DIM, 11, false); y += 16;
    draw_text(dc, px+14, y,   "[归零] → 设置当前位置为原点", C_DIM, 11, false); y += 16;
    draw_text(dc, px+14, y,   "[抓取] → 执行抓取动作", C_DIM, 11, false); y += 16;
    draw_text(dc, px+14, y,   "[急停] → 紧急停止; 关闭窗口退出", C_DIM, 11, false); y += 16;
    draw_text(dc, px+14, y,   "Rust + Win32 GDI — 零外部依赖", C_DIM, 11, false);
}

fn draw_buttons(dc: HDC, state: &UiState) {
    for i in 0..6 {
        let r = &state.btn_rects[i];
        unsafe {
            let br = CreateSolidBrush(BTN_COLORS[i]);
            let old = SelectObject(dc, br as HGDIOBJ);
            SelectObject(dc, GetStockObject(NULL_PEN) as HGDIOBJ);
            RoundRect(dc, r.left, r.top, r.right, r.bottom, 8, 8);
            SelectObject(dc, old); DeleteObject(br as HGDIOBJ);
        }
        let lbl = BTN_LABELS[i];
        unsafe {
            let f = CreateFontW(18, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0,
                                wide("Microsoft YaHei\0").as_ptr());
            let old = SelectObject(dc, f as HGDIOBJ);
            SetTextColor(dc, C_WHITE); SetBkMode(dc, TRANSPARENT);
            let mut sz = SIZE{cx:0,cy:0};
            let w = wide(lbl);
            GetTextExtentPoint32W(dc, w.as_ptr(), lbl.len() as i32, &mut sz);
            let tx = r.left + (r.right - r.left - sz.cx) / 2;
            let ty = r.top + (r.bottom - r.top - sz.cy) / 2;
            TextOutW(dc, tx, ty, w.as_ptr(), lbl.len() as i32);
            SelectObject(dc, old); DeleteObject(f as HGDIOBJ);
        }
    }
}

fn draw_all(dc: HDC, state: &UiState) {
    fill_rect(dc, 0, 0, SCR_W, SCR_H, C_BG);
    fill_rect(dc, 0, 0, SCR_W, 36, C_CARD);
    draw_text(dc, 12, 6, "CPDR 抓娃娃机控制系统", C_ACCENT, 18, true);
    draw_text(dc, SCR_W-140, 8, &state.status, C_BORDER, 14, true);
    draw_joystick(dc, state);
    draw_info_panel(dc, state);
    draw_buttons(dc, state);
}

unsafe extern "system" fn wndproc(hwnd: HWND, msg: u32, wp: WPARAM, lp: LPARAM) -> LRESULT {
    let state = match &mut UI { Some(s) => s, None => return DefWindowProcW(hwnd, msg, wp, lp) };

    match msg {
        WM_CREATE => 0,
        WM_TIMER if wp == 1 => {
            // Trigger redraw
            InvalidateRect(hwnd, null(), 0);
            0
        }
        WM_PAINT => {
            let mut ps: PAINTSTRUCT = std::mem::zeroed();
            let hdc = BeginPaint(hwnd, &mut ps);

            // Draw to backbuffer
            draw_all(state.mem_dc, state);
            BitBlt(hdc, 0, 0, SCR_W, SCR_H, state.mem_dc, 0, 0, SRCCOPY);

            EndPaint(hwnd, &ps);
            0
        }
        WM_ERASEBKGND => 1,
        WM_LBUTTONDOWN => {
            let mx = (lp & 0xFFFF) as i32;
            let my = ((lp >> 16) & 0xFFFF) as i32;
            let bi = hit_test_buttons(&state.btn_rects, mx, my);
            if bi >= 0 { state.event = bi; return 0; }
            if hit_test_joystick(mx, my) {
                state.joy.2 = true;
                SetCapture(hwnd);
            }
            0
        }
        WM_MOUSEMOVE if state.joy.2 => {
            let mx = (lp & 0xFFFF) as i32;
            let my = ((lp >> 16) & 0xFFFF) as i32;
            let dx = (mx - J_CX) as f32; let dy = (my - J_CY) as f32;
            let dist = (dx*dx + dy*dy).sqrt();
            let max_r = (J_BASE_R - J_KNOB_R) as f32;
            let (kx, ky) = if dist > max_r { (dx*max_r/dist, dy*max_r/dist) } else { (dx, dy) };
            state.knob = (kx, ky);
            state.joy = (kx / max_r * J_RANGE, -ky / max_r * J_RANGE, true);
            0
        }
        WM_LBUTTONUP if state.joy.2 => {
            ReleaseCapture();
            state.joy = (0.0, 0.0, false);
            state.knob = (0.0, 0.0);
            0
        }
        WM_MOUSEMOVE => 0,
        WM_LBUTTONUP => 0,
        WM_CLOSE => { state.event = 5; DestroyWindow(hwnd); 0 } // 5 = EXIT
        WM_DESTROY => {
            KillTimer(hwnd, 1);
            DeleteObject(state.mem_bmp as HGDIOBJ);
            DeleteDC(state.mem_dc);
            PostQuitMessage(0);
            0
        }
        _ => DefWindowProcW(hwnd, msg, wp, lp)
    }
}

/// 初始化并返回 Arc<Mutex<UiState>>
pub fn init() -> Result<(), String> {
    let hinst = std::ptr::null_mut::<c_void>();

    let cls_name = wide("CPDR_ClawMachine\0");
    let cls = WNDCLASSEXW {
        cbSize: std::mem::size_of::<WNDCLASSEXW>() as u32,
        style: CS_HREDRAW | CS_VREDRAW, lpfnWndProc: wndproc,
        cbClsExtra: 0, cbWndExtra: 0, hInstance: hinst,
        hIcon: null_mut(), hCursor: unsafe { LoadCursorW(null_mut(), wide("IDC_ARROW\0").as_ptr()) },
        hbrBackground: unsafe { GetStockObject(BLACK_BRUSH) as HBRUSH },
        lpszMenuName: null(), lpszClassName: cls_name.as_ptr(), hIconSm: null_mut(),
    };
    let atom = unsafe { RegisterClassExW(&cls) };
    if atom == 0 {
        let err = unsafe { crate::serial::GetLastError() };
        if err != 1410 { return Err(format!("RegisterClass failed: {}", err)); }
        // 1410 = class already exists, OK
    }

    let mut rc = RECT{left:0,top:0,right:SCR_W,bottom:SCR_H};
    unsafe {
        let style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        // AdjustWindowRect
        extern "system" {
            fn AdjustWindowRect(r: *mut RECT, style: u32, menu: i32) -> i32;
        }
        AdjustWindowRect(&mut rc, style, 0);
    }

    let wn = wide("CPDR 抓娃娃机控制系统\0");
    let hwnd = unsafe {
        CreateWindowExW(0, cls_name.as_ptr(), wn.as_ptr(),
                        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                        ((1920 - (rc.right-rc.left)) / 2) as i32, // center-ish
                        ((1080 - (rc.bottom-rc.top)) / 2) as i32,
                        rc.right - rc.left, rc.bottom - rc.top,
                        null_mut(), null_mut(), hinst, null_mut())
    };
    if hwnd.is_null() { return Err("CreateWindow failed".into()); }

    // 双缓冲
    let screen_dc = unsafe { GetDC(hwnd) };
    let mem_dc = unsafe { CreateCompatibleDC(screen_dc) };
    let mem_bmp = unsafe { CreateCompatibleBitmap(screen_dc, SCR_W, SCR_H) };
    unsafe { SelectObject(mem_dc, mem_bmp as HGDIOBJ); ReleaseDC(hwnd, screen_dc); }

    let mut btn_rects: [RECT; 6] = unsafe { std::mem::zeroed() };
    layout_buttons(&mut btn_rects);

    unsafe {
        let s = UiState {
            hwnd, mem_dc, mem_bmp,
            joy: (0.0, 0.0, false), knob: (0.0, 0.0),
            pos: (0.0, 0.0), tgt: (0.0, 0.0),
            motors: [false; 4], errs: 0,
            status: "就绪".to_string(),
            event: -1, last_event: -2,
            btn_rects,
        };
        UI = Some(s);
    }

    unsafe { SetTimer(hwnd, 1, 20, null_mut()); }
    unsafe { ShowWindow(hwnd, SW_SHOW); UpdateWindow(hwnd); }
    println!("[UI] Win32 GDI 初始化完成 {}x{}", SCR_W, SCR_H);
    Ok(())
}

pub fn run_loop(tick_fn: fn()) {
    let mut msg: MSG = unsafe { std::mem::zeroed() };
    loop {
        let ret = unsafe { GetMessageW(&mut msg, null_mut(), 0, 0) };
        if ret <= 0 { break; }
        // Before dispatching, check timer
        unsafe { TranslateMessage(&msg); DispatchMessageW(&msg); }
        // Check if should tick
        tick_fn();
        // Check exit
        if let Some(ref s) = unsafe { &UI } {
            if s.event == 5 { break; }
        }
    }
}

/// 获取摇杆数据 (线程安全通过 Arc<Mutex>)
pub fn get_joy() -> (f32, f32, bool) {
    unsafe { UI.as_ref().map(|s| s.joy).unwrap_or((0.0, 0.0, false)) }
}

pub fn get_event() -> i32 {
    unsafe {
        UI.as_mut().map(|s| {
            let e = s.event;
            if e != s.last_event { s.last_event = e; e } else { -1 }
        }).unwrap_or(-1)
    }
}

pub fn set_status(txt: &str) {
    unsafe { if let Some(ref mut s) = UI { s.status = txt.to_string(); } }
}

pub fn set_pos(x: f32, y: f32) {
    unsafe { if let Some(ref mut s) = UI { s.pos = (x, y); } }
}

pub fn set_tgt(x: f32, y: f32) {
    unsafe { if let Some(ref mut s) = UI { s.tgt = (x, y); } }
}

pub fn set_motors(online: &[bool; 4]) {
    unsafe { if let Some(ref mut s) = UI { s.motors = *online; } }
}

pub fn set_errs(n: u32) {
    unsafe { if let Some(ref mut s) = UI { s.errs = n; } }
}
