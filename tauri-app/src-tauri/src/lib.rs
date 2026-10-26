/// CPDR 抓娃娃机 — Tauri 库入口
///
/// 注册所有 Tauri 命令, 启动控制循环线程

mod protocol;
mod kinematics;
mod controller;
mod state;
mod serial;

use std::sync::atomic::Ordering;
use std::sync::Arc;
use std::time::Duration;
use tauri::{AppHandle, Manager, State};
use state::*;

// ======================== Tauri 命令 ========================

#[tauri::command]
fn set_joystick_target(state: State<'_, SharedState>, x: f32, y: f32) -> Result<(), String> {
    let mut s = state.lock().map_err(|e| e.to_string())?;
    s.joystick_target = crate::kinematics::Point2D { x, y };
    crate::kinematics::clamp_to_range(&mut s.joystick_target);
    s.joystick_active = true;
    Ok(())
}

#[tauri::command]
fn release_joystick(state: State<'_, SharedState>) -> Result<(), String> {
    let mut s = state.lock().map_err(|e| e.to_string())?;
    s.joystick_active = false;
    Ok(())
}

#[tauri::command]
fn enable_motors(state: State<'_, SharedState>) -> Result<(), String> {
    let mut s = state.lock().map_err(|e| e.to_string())?;
    if !s.serial_connected {
        return Err("串口未连接".into());
    }
    let mut port = serial::get_serial_port(&s.port_name)?;
    controller::enable_motors(&mut s, &mut port);
    Ok(())
}

#[tauri::command]
fn disable_motors(state: State<'_, SharedState>) -> Result<(), String> {
    let mut s = state.lock().map_err(|e| e.to_string())?;
    if !s.serial_connected {
        return Err("串口未连接".into());
    }
    let mut port = serial::get_serial_port(&s.port_name)?;
    controller::disable_motors(&mut s, &mut port);
    Ok(())
}

#[tauri::command]
fn zero_position(state: State<'_, SharedState>) -> Result<(), String> {
    let mut s = state.lock().map_err(|e| e.to_string())?;
    if !s.serial_connected {
        // 离线模式: 仍可软件归零
        crate::kinematics::set_zero(
            &mut s.kinematics,
            &crate::kinematics::EncoderValues { m1: 0, m2: 0, m3: 0, m4: 0 },
        );
        s.current_pos = crate::kinematics::Point2D { x: 0.0, y: 0.0 };
        s.joystick_target = crate::kinematics::Point2D { x: 0.0, y: 0.0 };
        return Ok(());
    }
    let mut port = serial::get_serial_port(&s.port_name)?;
    controller::zero_position(&mut s, &mut port);
    Ok(())
}

#[tauri::command]
fn grab(state: State<'_, SharedState>) -> Result<(), String> {
    let mut s = state.lock().map_err(|e| e.to_string())?;
    if !s.serial_connected {
        return Err("串口未连接".into());
    }
    let mut port = serial::get_serial_port(&s.port_name)?;
    controller::grab(&mut s, &mut port);
    Ok(())
}

#[tauri::command]
fn emergency_stop(state: State<'_, SharedState>) -> Result<(), String> {
    let mut s = state.lock().map_err(|e| e.to_string())?;
    if !s.serial_connected {
        s.estop = true;
        return Ok(());
    }
    let mut port = serial::get_serial_port(&s.port_name)?;
    controller::emergency_stop(&mut s, &mut port);
    Ok(())
}

#[tauri::command]
fn get_status(state: State<'_, SharedState>) -> Result<StatusSnapshot, String> {
    let s = state.lock().map_err(|e| e.to_string())?;
    Ok(s.snapshot())
}

#[tauri::command]
fn list_ports() -> Result<Vec<String>, String> {
    serial::list_available_ports()
}

#[tauri::command]
fn connect_port(state: State<'_, SharedState>, port_name: String) -> Result<bool, String> {
    let mut s = state.lock().map_err(|e| e.to_string())?;
    match serial::open_port(&port_name) {
        Ok(_) => {
            s.serial_connected = true;
            s.port_name = port_name;
            Ok(true)
        }
        Err(e) => {
            s.serial_connected = false;
            Err(e)
        }
    }
}

// ======================== 控制循环线程 ========================

fn start_control_loop(app: AppHandle, state: SharedState) {
    std::thread::spawn(move || {
        log::info!("控制线程启动 (50Hz)");

        loop {
            // 检查退出信号
            {
                let s = state.lock().unwrap();
                if s.should_exit.load(Ordering::Relaxed) {
                    log::info!("控制线程退出");
                    break;
                }
            }

            std::thread::sleep(Duration::from_millis(20));

            // 打开串口 (每次循环尝试)
            let mut port: Option<Box<dyn serialport::SerialPort>> = {
                let s = state.lock().unwrap();
                if s.serial_connected {
                    serial::get_serial_port(&s.port_name).ok()
                } else {
                    None
                }
            };

            // 执行控制循环
            let snapshot = {
                let mut s = state.lock().unwrap();
                controller::control_tick(&mut s, &mut port);
                s.snapshot()
            };

            // 推送状态到前端
            let _ = app.emit("status-update", &snapshot);
        }
    });
}

// ======================== Tauri setup ========================

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    env_logger::init();

    let port_name = "COM3".to_string();
    let state = create_shared_state(&port_name);

    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .manage(state.clone())
        .setup(move |app| {
            let handle = app.handle().clone();
            start_control_loop(handle, state);
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            set_joystick_target,
            release_joystick,
            enable_motors,
            disable_motors,
            zero_position,
            grab,
            emergency_stop,
            get_status,
            list_ports,
            connect_port,
        ])
        .run(tauri::generate_context!())
        .expect("启动Tauri应用失败");
}
