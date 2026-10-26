"""
Tab5 Super Mario Gamepad v2 — 纯按键手柄
十字键(含长按连发) + A跳/B跑 + Start/Select + 按键日志
"""
import M5; from M5 import *; import m5ui; import lvgl as lv

BG=0x1A1A2E
g_last_lbl=None; g_last_msg=""; g_log=None; g_log_lines=[]; g_cnt=0

def _L(pg,t,x,y,f,c=0xFFFFFF):
    return m5ui.M5Label(text=t,x=x,y=y,text_c=c,bg_c=BG,bg_opa=0,font=f,parent=pg)

def _B(pg,t,x,y,w,h,f,c=0xFFFFFF,bc=0x2A2A3E):
    lb=m5ui.M5Label(text=t,x=x,y=y,text_c=c,bg_c=bc,bg_opa=255,font=f,parent=pg)
    lb.set_size(w,h); return lb

def _btn(pg,t,x,y,w,h,c,font,cb,cb_long=None):
    b=m5ui.M5Button(text=t,x=x,y=y,w=w,h=h,text_c=0xFFFFFF,bg_c=c,font=font,parent=pg)
    b.add_event_cb(cb,lv.EVENT.ALL,None)
    if cb_long: b.add_event_cb(cb_long,lv.EVENT.ALL,None)
    try: b.set_style_radius(10,0)
    except: pass
    return b

# ===== 日志 =====
def _log(msg):
    global g_log,g_log_lines,g_cnt,g_last_msg
    g_cnt+=1; g_last_msg=msg
    g_log_lines.insert(0,"[%d] %s"%(g_cnt,msg))
    if len(g_log_lines)>4: g_log_lines.pop()
    g_log.set_text("\n".join(g_log_lines))

# ===== 方向键回调 =====
def _L_cb(e):
    if e.code==lv.EVENT.CLICKED: _log("LEFT")
def _L_long(e):
    if e.code==lv.EVENT.LONG_PRESSED: _log("LEFT*")
def _R_cb(e):
    if e.code==lv.EVENT.CLICKED: _log("RIGHT")
def _R_long(e):
    if e.code==lv.EVENT.LONG_PRESSED: _log("RIGHT*")
def _U_cb(e):
    if e.code==lv.EVENT.CLICKED: _log("UP")
def _U_long(e):
    if e.code==lv.EVENT.LONG_PRESSED: _log("UP*")
def _D_cb(e):
    if e.code==lv.EVENT.CLICKED: _log("DOWN")
def _D_long(e):
    if e.code==lv.EVENT.LONG_PRESSED: _log("DOWN*")
def _A_cb(e):
    if e.code==lv.EVENT.CLICKED: _log("JUMP (A)")
def _B_cb(e):
    if e.code==lv.EVENT.CLICKED: _log("RUN (B)")
def _S_cb(e):
    if e.code==lv.EVENT.CLICKED: _log("START")
def _X_cb(e):
    if e.code==lv.EVENT.CLICKED: _log("SELECT")

# ===== Setup =====
def setup():
    global g_log
    M5.begin(); Widgets.setRotation(1); m5ui.init()
    pg=m5ui.M5Page(bg_c=BG)

    # 标题栏
    hdr=_B(pg,"SUPER MARIO GAMEPAD",0,0,1280,50,lv.font_montserrat_22,0xFFFFFF,0xCC0000)
    try: hdr.set_style_text_align(lv.TEXT_ALIGN.CENTER,0); hdr.set_style_radius(0,0)
    except: pass

    # ======== 左: D-Pad ========
    _L(pg,"D-Pad",190,75,lv.font_montserrat_18,0xAAAAAA)
    # 底板装饰
    bg1=_B(pg,"",80,110,240,230,lv.font_montserrat_14,0x000000,0x222240)
    try: bg1.set_style_radius(24,0)
    except: pass

    # UP
    _btn(pg,"^", 160,120, 80,62, 0x333366,lv.font_montserrat_24,_U_cb,_U_long)
    # LEFT
    _btn(pg,"<",  90,200, 70,62, 0x333366,lv.font_montserrat_24,_L_cb,_L_long)
    # CENTER (装饰)
    _btn(pg,"o", 170,200, 60,62, 0x1A1A30,lv.font_montserrat_16,None)
    # RIGHT
    _btn(pg,">", 240,200, 70,62, 0x333366,lv.font_montserrat_24,_R_cb,_R_long)
    # DOWN
    _btn(pg,"v", 160,280, 80,48, 0x333366,lv.font_montserrat_24,_D_cb,_D_long)

    # ======== 中: Start/Select + 状态 ========
    _L(pg,"Menu",530,75,lv.font_montserrat_18,0xAAAAAA)

    _btn(pg,"SELECT",480,120,140,44,0x555555,lv.font_montserrat_14,_X_cb)
    _btn(pg,"START", 480,180,140,44,0x555555,lv.font_montserrat_14,_S_cb)

    # 马里奥像素画
    art=("@  @  @@@@\n"
         "@@@@ @@@@@@\n"
         "@@@@ @@  @@\n"
         "@  @ @@@@@@\n"
         "     @@  @@\n"
         "    @@@  @@@")
    _L(pg,art,530,260,lv.font_montserrat_14,0xCC0000)

    # 当前按键指示
    global g_last_lbl
    g_last_lbl=_B(pg,"Press a button...",440,410,400,50,lv.font_montserrat_20,0x2196F3,0x2A2A3E)

    # ======== 右: A/B ========
    _L(pg,"Actions",1050,75,lv.font_montserrat_18,0xAAAAAA)
    bg2=_B(pg,"",940,110,240,210,lv.font_montserrat_14,0x000000,0x222240)
    try: bg2.set_style_radius(24,0)
    except: pass

    # B (红-小)
    _btn(pg,"B", 970,135, 90,85, 0xCC0000,lv.font_montserrat_30,_B_cb)
    # A (绿-大)
    _btn(pg,"A", 1050,215, 100,95, 0x3CB043,lv.font_montserrat_36,_A_cb)

    _L(pg,"B:Run",990,245,lv.font_montserrat_14,0xCC0000)
    _L(pg,"A:Jump",1080,130,lv.font_montserrat_14,0x3CB043)

    # ======== 底: 历史记录 ========
    _L(pg,"── Key History ──",500,480,lv.font_montserrat_16,0x666666)
    g_log=_L(pg,"",450,510,lv.font_montserrat_16,0xBBBBBB)
    g_log.set_size(450,140)

    # 提示
    hint=_B(pg,"Long-press D-Pad = Turbo  |  A=Jump  B=Run  SELECT/START",0,670,1280,40,lv.font_montserrat_14,0x777777,0x111122)
    try: hint.set_style_radius(0,0)
    except: pass

    pg.screen_load()
    print("===== Super Mario Gamepad v2 Ready! =====")

def loop():
    M5.update()
    global g_last_lbl,g_last_msg
    if g_last_lbl is not None and g_last_msg:
        try: g_last_lbl.set_text(g_last_msg)
        except: pass

if __name__=="__main__":
    setup()
    while True: loop()
