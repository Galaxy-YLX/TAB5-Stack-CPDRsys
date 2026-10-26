/// 闭环运动控制器 (P控制, 50Hz)
use crate::kinematics::*;
use crate::protocol::*;
use crate::serial::*;

const KP: f32 = 8.0;
const DEADZONE_MM: f32 = 0.5;

pub fn read_pos(port: PortHandle, addr: u8) -> Option<i32> {
    let cmd = build_read_pos(addr);
    if send(port, &cmd) < 0 { return None; }
    let mut buf = [0u8; 64];
    // Read with 50ms timeout, accumulate bytes
    let mut total = 0usize;
    use std::time::Instant;
    let deadline = Instant::now() + std::time::Duration::from_millis(50);
    while Instant::now() < deadline {
        match recv(port, &mut buf[total..], 10) {
            n if n > 0 => { total += n as usize; if total >= 6 { break; } }
            _ => std::thread::sleep(std::time::Duration::from_millis(1)),
        }
    }
    if total < 6 { return None; }
    parse_pos_response(&buf[..total], addr)
}

pub fn send_torque(port: PortHandle, addr: u8, dir: u8, cur: u16) -> bool {
    let cmd = build_torque(addr, dir, cur);
    send(port, &cmd) >= 0
}

fn compute_torque(delta_pulses: f32) -> (u8, u16) {
    let a = delta_pulses.abs();
    if a < DEADZONE_MM * PULSES_PER_MM { return (0, TORQUE_MIN); }
    let d = if delta_pulses > 0.0 { 1 } else { 0 };
    let c = ((KP * a) as u16).clamp(TORQUE_MIN, TORQUE_MAX);
    (d, c)
}

pub struct Ctrl {
    pub kin: KinState,
    pub last_enc: Encoders,
    pub motor_online: [bool; 4],
    pub comm_errs: u32,
    pub loop_cnt: u32,
    pub enabled: bool,
    pub estop: bool,
}

impl Ctrl {
    pub fn new() -> Self {
        Ctrl{kin:KinState::default(),last_enc:Encoders::default(),
             motor_online:[false;4],comm_errs:0,loop_cnt:0,enabled:false,estop:false}
    }

    pub fn tick(&mut self, port: PortHandle) {
        if self.estop { return; }
        self.loop_cnt += 1;
        let mut enc = self.last_enc;
        if port.is_null() { return; }
        for (i, &a) in MOTOR_ADDRS.iter().enumerate() {
            if let Some(p) = read_pos(port, a) {
                self.motor_online[i] = true;
                match i { 0=>enc.m1=p; 1=>enc.m2=p; 2=>enc.m3=p; 3=>enc.m4=p; _=>{} }
            } else { self.motor_online[i] = false; self.comm_errs += 1; }
        }
        self.last_enc = enc;
        if self.kin.zeroed {
            self.kin.cur = fwd(&self.kin, &enc);
            self.kin.cur_len = cur_len(&self.kin, &enc);
        }
        if self.kin.zeroed {
            let deltas = deltas(&self.kin, &self.kin.tgt);
            for (i, &a) in MOTOR_ADDRS.iter().enumerate() {
                if self.motor_online[i] && !port.is_null() {
                    let (d, c) = compute_torque(deltas[i] as f32);
                    send_torque(port, a, d, c);
                }
            }
        }
    }

    pub fn enable(&mut self, port: PortHandle) {
        if port.is_null() { return; }
        for &a in MOTOR_ADDRS.iter() {
            send(port, &build_clear(a));
            std::thread::sleep(std::time::Duration::from_millis(5));
            send(port, &build_enable(a, 0));
            std::thread::sleep(std::time::Duration::from_millis(5));
        }
        self.enabled = true; self.estop = false; self.comm_errs = 0;
    }

    pub fn disable(&mut self, port: PortHandle) {
        if port.is_null() { return; }
        for &a in MOTOR_ADDRS.iter() {
            send(port, &build_stop(a));
            std::thread::sleep(std::time::Duration::from_millis(5));
            send(port, &build_enable(a, 1));
            std::thread::sleep(std::time::Duration::from_millis(5));
        }
        self.enabled = false; self.kin.zeroed = false;
    }

    pub fn zero(&mut self, port: PortHandle) {
        set_zero(&mut self.kin, &self.last_enc);
        self.kin.cur = Pt{x:0.0,y:0.0}; self.kin.tgt = Pt{x:0.0,y:0.0};
        if !port.is_null() {
            for &a in MOTOR_ADDRS.iter() { send(port, &build_angle_zero(a)); }
            for &a in MOTOR_ADDRS.iter() { send(port, &build_torque(a, 0, 0)); }
        }
    }

    pub fn grab(&mut self, port: PortHandle) {
        if port.is_null() { return; }
        for &a in MOTOR_ADDRS.iter() { send(port, &build_stop(a)); }
        for &a in MOTOR_ADDRS.iter() { send(port, &build_torque(a, 0, 800)); }
    }

    pub fn estop(&mut self, port: PortHandle) {
        self.estop = true;
        if !port.is_null() {
            for &a in MOTOR_ADDRS.iter() { send(port, &build_stop(a)); }
        }
    }
}
