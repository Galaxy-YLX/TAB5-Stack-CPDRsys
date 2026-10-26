"""
电机力矩控制 — M5Stack Tab5
=============================
正点原子步进电机驱动器协议 V1.2 | RS485 (GPIO20/21/34)
按住按钮 = 1200mA | 松开 = 150mA | 方向: 反转 | 先使能后发力矩
▲电机1  ▼电机3  ◀电机2  ▶电机4
"""

import machine, time
import M5
from M5 import *

# ── 协议常量 ──────────────────────────────────────────
_FHEAD = 0xC5
_FTAIL = 0x5C
_FEN   = 0xFA    # 使能
_FTORQ = 0xF0    # 力矩
_HI    = 1200
_LO    = 150

# ── RS485 引脚 ────────────────────────────────────────
_UART = 1
_TX   = 20
_RX   = 21
_DIR  = 34

# ── 按键 → 电机 ───────────────────────────────────────
_MAP = {'up': 1, 'down': 3, 'left': 4, 'right': 2}

# ── 按键区域 (120×90) ─────────────────────────────────
_BW, _BH = 120, 90
_CX, _CY = 300, 300
_GAP     = 100

_ZONE = {
    'up':    (_CX - _BW // 2, _CY - _BH - _GAP),
    'down':  (_CX - _BW // 2, _CY + _GAP),
    'left':  (_CX - _BW - _GAP, _CY - _BH // 2),
    'right': (_CX + _GAP,       _CY - _BH // 2),
}

# ── 颜色 (美化版) ─────────────────────────────────────
_C_BG       = 0x1A1A2E   # 深色背景

# 按钮 — 释放态
_C_REL_TOP  = 0x454570   # 上部
_C_REL_BOT  = 0x2E2E50   # 下部
_C_REL_HI   = 0x6A6A95   # 高光边
_C_REL_SD   = 0x151530   # 阴影边
_C_REL_ARR  = 0xB0B0D0   # 箭头

# 按钮 — 按下态
_C_PRS_TOP  = 0xFF7B3D   # 上部
_C_PRS_BOT  = 0xE05020   # 下部
_C_PRS_HI   = 0xFFAA80   # 高光边
_C_PRS_SD   = 0x8A3010   # 阴影边
_C_PRS_ARR  = 0xFFFFFF   # 箭头

# ── 状态 ──────────────────────────────────────────────
_uart  = None
_dpin  = None
_cur   = None


# ═══════════════════════════════════════════════════════
#  RS485 / 协议
# ═══════════════════════════════════════════════════════

def _checksum(data):
    s = 0
    for b in data:
        s += b
    return s & 0xFF


def _send(data):
    _dpin.value(1)
    time.sleep_us(200)
    _uart.write(bytes(data))
    time.sleep_ms(max(1, len(data) * 10 * 1000 // 115200 + 10))
    _dpin.value(0)


def _motor_enable(addr, en):
    """使能/失能电机  0=使能, 1=失能"""
    b = bytearray(6)
    b[0] = _FHEAD
    b[1] = addr
    b[2] = _FEN
    b[3] = en
    b[4] = _checksum(b[:4])
    b[5] = _FTAIL
    _send(b)


def _motor(addr, ma):
    _motor_enable(addr, 0)           # 先使能
    b = bytearray(8)
    b[0] = _FHEAD
    b[1] = addr
    b[2] = _FTORQ
    b[3] = 0x01
    b[4] = (ma >> 8) & 0xFF
    b[5] = ma & 0xFF
    b[6] = _checksum(b[:6])
    b[7] = _FTAIL
    _send(b)
    print("M%d→%dmA" % (addr, ma))


# ═══════════════════════════════════════════════════════
#  触摸 / 按钮
# ═══════════════════════════════════════════════════════

def _hit(x, y):
    for nm, (zx, zy) in _ZONE.items():
        if zx <= x <= zx + _BW and zy <= y <= zy + _BH:
            return nm
    return None


def _switch(to):
    global _cur
    if to == _cur:
        return
    # 松开旧
    if _cur is not None:
        _motor(_MAP[_cur], _LO)
    # 按下新
    if to is not None:
        _motor(_MAP[to], _HI)
    _cur = to
    _draw(to)


# ═══════════════════════════════════════════════════════
#  按钮绘制
# ═══════════════════════════════════════════════════════

def _fill_btn_bg(x, y, w, h, r, c_top, c_bot):
    """圆角渐变背景: 上方 c_top → 下方 c_bot"""
    # 上半部 (3/5 高度 + 圆角)
    mid = y + h * 3 // 5
    top_h = mid - y + r
    # 中心 + 四边条 + 四角圆
    M5.Lcd.fillRect(x + r, y + r, w - 2 * r, top_h - 2 * r, c_top)
    M5.Lcd.fillRect(x + r, y, w - 2 * r, r, c_top)
    M5.Lcd.fillRect(x, y + r, w, top_h - r, c_top)
    M5.Lcd.fillCircle(x + r, y + r, r, c_top)
    M5.Lcd.fillCircle(x + w - r - 1, y + r, r, c_top)
    # 下半部 (直角底边)
    M5.Lcd.fillRect(x, mid, w, y + h - mid, c_bot)
    # 过渡融合线
    for i in range(4):
        t = (i + 1) / 5
        rr = int((c_top >> 16 & 0xFF) * (1 - t) + (c_bot >> 16 & 0xFF) * t)
        gg = int((c_top >> 8  & 0xFF) * (1 - t) + (c_bot >> 8  & 0xFF) * t)
        bb = int((c_top       & 0xFF) * (1 - t) + (c_bot       & 0xFF) * t)
        M5.Lcd.drawLine(x + r, mid - 2 + i, x + w - r - 1, mid - 2 + i,
                        rr << 16 | gg << 8 | bb)


def _draw_bevel(x, y, w, h, r, hi, sd):
    """3D 斜面边框 (2px 高光 + 2px 阴影)"""
    # 上方高光
    M5.Lcd.drawLine(x + r, y,     x + w - r - 1, y,     hi)
    M5.Lcd.drawLine(x + r, y + 1, x + w - r - 2, y + 1, hi)
    # 左侧高光
    M5.Lcd.drawLine(x,     y + r, x,     y + h - r - 1, hi)
    M5.Lcd.drawLine(x + 1, y + r, x + 1, y + h - r - 2, hi)
    # 下方阴影
    M5.Lcd.drawLine(x + r, y + h - 1, x + w - r - 1, y + h - 1, sd)
    M5.Lcd.drawLine(x + r, y + h - 2, x + w - r - 2, y + h - 2, sd)
    # 右侧阴影
    M5.Lcd.drawLine(x + w - 1, y + r, x + w - 1, y + h - r - 1, sd)
    M5.Lcd.drawLine(x + w - 2, y + r, x + w - 2, y + h - r - 2, sd)


def _draw_btn(nm, active):
    x, y = _ZONE[nm]
    pressed = (nm == active)
    R = 10

    # ── 选色 ──
    ct = _C_PRS_TOP if pressed else _C_REL_TOP
    cb = _C_PRS_BOT if pressed else _C_REL_BOT
    hi = _C_PRS_HI  if pressed else _C_REL_HI
    sd = _C_PRS_SD  if pressed else _C_REL_SD
    ca = _C_PRS_ARR if pressed else _C_REL_ARR

    # ── 1. 渐变背景 ──
    _fill_btn_bg(x, y, _BW, _BH, R, ct, cb)

    # ── 2. 3D 斜面边框 ──
    _draw_bevel(x, y, _BW, _BH, R, hi, sd)

    # ── 3. 方向箭头 (内置 fillTriangle) ──
    off = 3 if pressed else 1
    mx, my = x + _BW // 2, y + _BH // 2 + off
    s = 15
    tri = {
        'up':    (mx, my - s, mx - s, my + s, mx + s, my + s),
        'down':  (mx, my + s, mx - s, my - s, mx + s, my - s),
        'left':  (mx - s, my, mx + s, my - s, mx + s, my + s),
        'right': (mx + s, my, mx - s, my - s, mx - s, my + s),
    }[nm]
    # 阴影 (偏移 1px)
    M5.Lcd.fillTriangle(tri[0] + 1, tri[1] + 1, tri[2] + 1,
                        tri[3] + 1, tri[4] + 1, tri[5] + 1, sd)
    # 主体
    M5.Lcd.fillTriangle(*tri, ca)


def _draw(active):
    for nm in ('up', 'down', 'left', 'right'):
        _draw_btn(nm, active)


# ═══════════════════════════════════════════════════════
#  入口
# ═══════════════════════════════════════════════════════

def setup():
    global _uart, _dpin
    M5.begin()
    M5.Lcd.clear(_C_BG)
    _uart = machine.UART(_UART, tx=_TX, rx=_RX,
                         baudrate=115200, bits=8, parity=None, stop=1)
    _dpin = machine.Pin(_DIR, machine.Pin.OUT, value=0)

    print("init 150mA ...")
    for a in range(1, 5):
        _motor(a, _LO)
        time.sleep_ms(50)

    _draw(None)
    print("=== ready ===")


def loop():
    M5.update()
    if M5.Touch.getCount():
        _switch(_hit(M5.Touch.getX(), M5.Touch.getY()))
    else:
        _switch(None)
    time.sleep_ms(30)


if __name__ == "__main__":
    try:
        setup()
        while True:
            loop()
    except (Exception, KeyboardInterrupt) as e:
        try:
            from utility import print_error_msg
            print_error_msg(e)
        except ImportError:
            print("error:", e)
