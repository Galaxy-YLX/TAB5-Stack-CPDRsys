/// 电机通信协议 — 帧构建/解析
/// 帧格式: HEAD(0xC5) + ADDR(1B) + CMD(1B) + DATA(0~8B) + CHK(1B) + TAIL(0x5C)

pub const FRAME_HEAD: u8 = 0xC5;
pub const FRAME_TAIL: u8 = 0x5C;
pub const MOTOR_ADDRS: [u8; 4] = [0x01, 0x02, 0x03, 0x04];
pub const ENCODER_PPR: u32 = 51200;           // 脉冲/圈
pub const ROPE_MM_PER_REV: f32 = 30.0;         // mm/圈
pub const PULSES_PER_MM: f32 = ENCODER_PPR as f32 / ROPE_MM_PER_REV;
pub const TORQUE_MIN: u16 = 150;
pub const TORQUE_MAX: u16 = 1500;

pub mod fct { pub const R_POS: u8 = 0x2A; pub const TORQUE: u8 = 0xF0;
              pub const ENABLE: u8 = 0xFA; pub const CLEAR: u8 = 0xFB;
              pub const STOP: u8 = 0xFC; pub const ZERO: u8 = 0xF8; }

pub fn checksum(d: &[u8]) -> u8 { d.iter().fold(0u8, |a, &b| a.wrapping_add(b)) }

pub fn build_torque(addr: u8, dir: u8, cur: u16) -> Vec<u8> {
    let mut c = vec![FRAME_HEAD, addr, fct::TORQUE, dir, (cur>>8) as u8, cur as u8];
    c.push(checksum(&c)); c.push(FRAME_TAIL); c
}
pub fn build_read_pos(addr: u8) -> Vec<u8> {
    let mut c = vec![FRAME_HEAD, addr, fct::R_POS]; c.push(checksum(&c)); c.push(FRAME_TAIL); c
}
pub fn build_enable(addr: u8, en: u8) -> Vec<u8> {
    let mut c = vec![FRAME_HEAD, addr, fct::ENABLE, en]; c.push(checksum(&c)); c.push(FRAME_TAIL); c
}
pub fn build_stop(addr: u8) -> Vec<u8> {
    let mut c = vec![FRAME_HEAD, addr, fct::STOP]; c.push(checksum(&c)); c.push(FRAME_TAIL); c
}
pub fn build_angle_zero(addr: u8) -> Vec<u8> {
    let mut c = vec![FRAME_HEAD, addr, fct::ZERO]; c.push(checksum(&c)); c.push(FRAME_TAIL); c
}
pub fn build_clear(addr: u8) -> Vec<u8> {
    let mut c = vec![FRAME_HEAD, addr, fct::CLEAR]; c.push(checksum(&c)); c.push(FRAME_TAIL); c
}

/// 解析响应帧, 提取位置值
pub fn parse_pos_response(buf: &[u8], _addr: u8) -> Option<i32> {
    if buf.len() < 6 { return None; }
    if buf[0] != FRAME_HEAD || buf[buf.len()-1] != FRAME_TAIL { return None; }
    if checksum(&buf[..buf.len()-2]) != buf[buf.len()-2] { return None; }
    if buf[2] != fct::R_POS { return None; }
    if buf[3] != 0x01 { return None; } // error_code must be SUCCESS
    if buf.len() < 10 { return None; }
    Some(i32::from_be_bytes([buf[4],buf[5],buf[6],buf[7]]))
}
