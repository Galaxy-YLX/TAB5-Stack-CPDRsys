/// 绳索驱动并联机器人运动学
/// L1=√((x+315)²+(y-315)²)  L2=√((x-315)²+(y-315)²)
/// L3=√((x-315)²+(y+315)²)  L4=√((x+315)²+(y+315)²)

use crate::protocol::PULSES_PER_MM;

pub const HALF: f32 = 315.0;
pub const RANGE: f32 = 200.0;

#[derive(Debug, Clone, Copy, Default)]
pub struct Pt { pub x: f32, pub y: f32 }

#[derive(Debug, Clone, Copy)]
pub struct Ropes { pub l1: f32, pub l2: f32, pub l3: f32, pub l4: f32 }

#[derive(Debug, Clone, Copy, Default)]
pub struct Encoders { pub m1: i32, pub m2: i32, pub m3: i32, pub m4: i32 }

#[derive(Debug, Clone)]
pub struct KinState {
    pub cur: Pt, pub tgt: Pt,
    pub cur_len: Ropes, pub init_len: Ropes,
    pub zero_enc: Encoders, pub zeroed: bool,
}

impl Default for KinState {
    fn default() -> Self {
        let o = Pt{x:0.0,y:0.0}; let l = inv(&o);
        KinState{cur:o,tgt:o,cur_len:l,init_len:l,zero_enc:Encoders::default(),zeroed:false}
    }
}

/// 逆运动学
pub fn inv(p: &Pt) -> Ropes { let x=p.x; let y=p.y; let h=HALF;
    Ropes{l1:((x+h)*(x+h)+(y-h)*(y-h)).sqrt(), l2:((x-h)*(x-h)+(y-h)*(y-h)).sqrt(),
          l3:((x-h)*(x-h)+(y+h)*(y+h)).sqrt(), l4:((x+h)*(x+h)+(y+h)*(y+h)).sqrt()}
}

fn pl(d: i32) -> f32 { d as f32 / PULSES_PER_MM }
pub fn lp(d: f32) -> i32 { (d * PULSES_PER_MM).round() as i32 }

/// 编码器→当前绳长
pub fn cur_len(st: &KinState, e: &Encoders) -> Ropes {
    Ropes{l1:st.init_len.l1+pl(e.m1-st.zero_enc.m1), l2:st.init_len.l2+pl(e.m2-st.zero_enc.m2),
          l3:st.init_len.l3+pl(e.m3-st.zero_enc.m3), l4:st.init_len.l4+pl(e.m4-st.zero_enc.m4)}
}

/// 正运动学解析解: x=(L1²-L2²)/1260, y=(L4²-L1²)/1260
pub fn fwd(st: &KinState, e: &Encoders) -> Pt {
    let l = cur_len(st, e); let d=4.0*HALF;
    let l1=l.l1*l.l1; let l2=l.l2*l.l2; let l3=l.l3*l.l3; let l4=l.l4*l.l4;
    Pt{x:((l1-l2)/d+(l4-l3)/d)*0.5, y:((l4-l1)/d+(l3-l2)/d)*0.5}
}

/// 绳长脉冲差值
pub fn deltas(st: &KinState, tgt: &Pt) -> [i32; 4] {
    let tl = inv(tgt);
    [lp(tl.l1-st.cur_len.l1), lp(tl.l2-st.cur_len.l2), lp(tl.l3-st.cur_len.l3), lp(tl.l4-st.cur_len.l4)]
}

/// 归零
pub fn set_zero(st: &mut KinState, e: &Encoders) {
    st.zero_enc = *e; st.cur = Pt{x:0.0,y:0.0}; st.tgt = Pt{x:0.0,y:0.0};
    st.init_len = inv(&Pt{x:0.0,y:0.0}); st.cur_len = st.init_len; st.zeroed = true;
}

pub fn clamp(p: &mut Pt) { p.x=p.x.clamp(-RANGE,RANGE); p.y=p.y.clamp(-RANGE,RANGE); }
