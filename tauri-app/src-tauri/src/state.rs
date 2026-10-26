/// 全局共享状态 — AppState
///
/// 使用 Arc<Mutex<AppState>> 在控制线程和Tauri命令间共享

use std::sync::Arc;
use std::sync::Mutex;
use std::sync::atomic::{AtomicBool, Ordering};
use crate::kinematics::{KinematicsState, Point2D};
use crate::protocol::{MotorStatus, MOTOR_ADDRS, TORQUE_MIN_MA};

pub type SharedState = Arc<Mutex<AppState>>;

/// 控制器状态
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum MotionState {
    Idle,
    Moving,
    Holding,
    Grabbing,
    Error,
    EStop,
}

impl MotionState {
    pub fn as_str(&self) -> &'static str {
        match self {
            MotionState::Idle => "就绪",
            MotionState::Moving => "运动中",
            MotionState::Holding => "保持",
            MotionState::Grabbing => "抓取中",
            MotionState::Error => "通信错误",
            MotionState::EStop => "急停!",
        }
    }
}

pub struct AppState {
    /// 串口名称
    pub port_name: String,
    /// 是否已连接串口
    pub serial_connected: bool,
    /// 电机使能状态
    pub motors_enabled: bool,
    /// 急停标志
    pub estop: bool,
    /// 摇杆活动标志
    pub joystick_active: bool,
    /// 摇杆目标位置 (mm)
    pub joystick_target: Point2D,
    /// 当前位置 (mm)
    pub current_pos: Point2D,
    /// 目标位置 (mm)
    pub target_pos: Point2D,
    /// 运动学状态
    pub kinematics: KinematicsState,
    /// 各电机在线状态
    pub motor_online: [bool; 4],
    /// 各电机状态
    pub motor_status: [MotorStatus; 4],
    /// 上一周期编码器值
    pub last_encoders: crate::kinematics::EncoderValues,
    /// 通信错误计数
    pub comm_errors: u32,
    /// 控制循环计数
    pub loop_count: u32,
    /// 控制器状态
    pub motion_state: MotionState,
    /// 是否需要退出
    pub should_exit: Arc<AtomicBool>,
}

impl AppState {
    pub fn new(port_name: String) -> Self {
        let origin = Point2D { x: 0.0, y: 0.0 };
        let init_len = crate::kinematics::inverse_kinematics(&origin);
        AppState {
            port_name,
            serial_connected: false,
            motors_enabled: false,
            estop: false,
            joystick_active: false,
            joystick_target: origin,
            current_pos: origin,
            target_pos: origin,
            kinematics: KinematicsState::default(),
            motor_online: [false; 4],
            motor_status: (0..4)
                .map(|_| MotorStatus {
                    position: 0,
                    speed_rpm: 0,
                    current_ma: 0,
                    motor_state: 0,
                    online: false,
                })
                .collect::<Vec<_>>()
                .try_into()
                .unwrap(),
            last_encoders: crate::kinematics::EncoderValues { m1: 0, m2: 0, m3: 0, m4: 0 },
            comm_errors: 0,
            loop_count: 0,
            motion_state: MotionState::Idle,
            should_exit: Arc::new(AtomicBool::new(false)),
        }
    }
}

/// 构建默认状态 (离线模式)
pub fn create_shared_state(port_name: &str) -> SharedState {
    Arc::new(Mutex::new(AppState::new(port_name.to_string())))
}

/// 状态快照 (序列化为JSON发送到前端)
#[derive(Debug, Clone, serde::Serialize)]
pub struct StatusSnapshot {
    pub current_x: f32,
    pub current_y: f32,
    pub target_x: f32,
    pub target_y: f32,
    pub joystick_x: f32,
    pub joystick_y: f32,
    pub joystick_active: bool,
    pub motors_enabled: bool,
    pub motor_online: [bool; 4],
    pub motor_positions: [i32; 4],
    pub motor_currents: [i16; 4],
    pub estop: bool,
    pub comm_errors: u32,
    pub motion_state: String,
    pub is_zeroed: bool,
}

impl AppState {
    pub fn snapshot(&self) -> StatusSnapshot {
        StatusSnapshot {
            current_x: self.current_pos.x,
            current_y: self.current_pos.y,
            target_x: self.target_pos.x,
            target_y: self.target_pos.y,
            joystick_x: self.joystick_target.x,
            joystick_y: self.joystick_target.y,
            joystick_active: self.joystick_active,
            motors_enabled: self.motors_enabled,
            motor_online: self.motor_online,
            motor_positions: [
                self.motor_status[0].position,
                self.motor_status[1].position,
                self.motor_status[2].position,
                self.motor_status[3].position,
            ],
            motor_currents: [
                self.motor_status[0].current_ma,
                self.motor_status[1].current_ma,
                self.motor_status[2].current_ma,
                self.motor_status[3].current_ma,
            ],
            estop: self.estop,
            comm_errors: self.comm_errors,
            motion_state: self.motion_state.as_str().to_string(),
            is_zeroed: self.kinematics.is_zeroed,
        }
    }
}
