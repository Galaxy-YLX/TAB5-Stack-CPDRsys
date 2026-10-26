"""
Tab5 m5ui 最小测试 — 验证基础 API
粘贴后 Ctrl-D 运行，看屏幕是否显示按钮和文字。
"""
import M5
from M5 import *
import m5ui
import lvgl as lv

g_cnt = 0
g_lbl = None

def btn_clicked(e):
    global g_cnt, g_lbl
    g_cnt += 1
    g_lbl.set_text("Count: %d" % g_cnt)

def btn_handler(e):
    if e.code == lv.EVENT.CLICKED:
        btn_clicked(e)

def setup():
    global g_lbl
    M5.begin()
    Widgets.setRotation(1)
    m5ui.init()

    page = m5ui.M5Page(bg_c=0x1A1A2E)

    # 标题
    m5ui.M5Label(
        text="Tab5 m5ui Test",
        x=0, y=50,
        text_c=0xFFFFFF, bg_c=0x1A1A2E, bg_opa=0,
        font=lv.font_montserrat_36,
        parent=page,
    )

    # 计数器
    g_lbl = m5ui.M5Label(
        text="Count: 0",
        x=440, y=200,
        text_c=0xFFFFFF, bg_c=0x2A2A3E, bg_opa=255,
        font=lv.font_montserrat_36,
        parent=page,
    )
    g_lbl.set_size(400, 70)

    # 按钮
    btn = m5ui.M5Button(
        text="Click Me",
        x=490, y=350, w=300, h=80,
        text_c=0xFFFFFF, bg_c=0x2196F3,
        font=lv.font_montserrat_24,
        parent=page,
    )
    btn.add_event_cb(btn_handler, lv.EVENT.ALL, None)

    page.screen_load()
    print("OK - setup done")

def loop():
    M5.update()

setup()
while True:
    loop()
