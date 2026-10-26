// CPDR 抓娃娃机 v2.0 — 纯 Rust + Win32 FFI, 零外部依赖
#![windows_subsystem = "console"]

mod protocol;
mod kinematics;
mod controller;
mod serial;
mod ui;

use kinematics::*;
use controller::Ctrl;

fn main() {
    println!("==============================================================");
    println!("  CPDR 抓娃娃机 v2.0 — Rust + Win32 GDI (零外部依赖)");
    println!("==============================================================");
    println!();

    // 串口
    let port_name = std::env::args().nth(1).unwrap_or_else(|| "COM3".to_string());
    println!("[MAIN] 扫描串口...");
    let ports = serial::list_ports();
    println!("[MAIN] 可用串口: {:?}", ports);
    println!("[MAIN] 目标串口: {}", port_name);

    let port = serial::open(&port_name).ok();
    let online = port.is_some();
    if !online { println!("[MAIN] 离线模式 (无硬件)"); }

    // 控制器
    let mut ctrl = Ctrl::new();

    // UI
    println!("[MAIN] 初始化 UI...");
    ui::init().expect("UI init failed");

    println!("[MAIN] 系统启动 — 使能→归零→摇杆→抓取");
    println!("[MAIN] 关闭窗口退出");
    println!();

    // 主循环
    ui::run_loop(|| {
        // -- 事件 --
        let evt = ui::get_event();
        match evt {
            0 => { println!("[MAIN] 使能"); if online { ctrl.enable(port.unwrap()); } },
            1 => { println!("[MAIN] 失能"); if online { ctrl.disable(port.unwrap()); } },
            2 => { println!("[MAIN] 归零"); ctrl.zero(port.unwrap_or(std::ptr::null_mut())); },
            3 => { println!("[MAIN] 抓取"); ctrl.grab(port.unwrap_or(std::ptr::null_mut())); },
            4 => { println!("[MAIN] 急停"); ctrl.estop(port.unwrap_or(std::ptr::null_mut())); },
            5 => { println!("[MAIN] 退出"); return; },
            _ => {},
        }

        // -- 摇杆 --
        let (jx, jy, jactive) = ui::get_joy();
        if jactive {
            let mut t = Pt{x:jx, y:jy}; clamp(&mut t);
            ctrl.kin.tgt = t;
            ui::set_tgt(t.x, t.y);
        }

        // -- 控制 --
        if !ctrl.estop {
            ctrl.tick(port.unwrap_or(std::ptr::null_mut()));
        }

        // -- 更新 UI --
        ui::set_pos(ctrl.kin.cur.x, ctrl.kin.cur.y);
        ui::set_motors(&ctrl.motor_online);
        ui::set_errs(ctrl.comm_errs);

        let st = if ctrl.estop { "急停!" }
                 else if !ctrl.enabled { if online { "就绪" } else { "离线演示" } }
                 else { "运动中" };
        ui::set_status(st);
    });

    // 清理
    println!("[MAIN] 关闭中...");
    if ctrl.enabled && online { ctrl.disable(port.unwrap()); }
    if let Some(p) = port { serial::close(p); }
    println!("[MAIN] 已关闭");
}
