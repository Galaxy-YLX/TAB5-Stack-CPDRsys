import M5; from M5 import *; import m5ui; import lvgl as lv; import time; import random
SW=1280; SH=720; HH=50; GW=1280; GH=670; TS=32
GRAVITY=0.55; JUMP_VEL=-10.5; MOVE_SPD=3; RUN_SPD=5.5; MAX_FALL=12; ENEMY_SPD=1.2
CSKY=0x6BADF7; CGROUND=0xC8621E; CGTOP=0x50C828; CBRICK=0xC8402E; CMORTAR=0x8B2010
CQMARK=0xE8A010; CQSYM=0x8B4513; CCOIN=0xFFD700; CCOIND=0xCC8800; CRED=0xE01020
CSKIN=0xFFC8A0; CBRN=0x8B4513; CBLUE=0x2040E0; CGOOMBA=0xA0522D; CPIPE=0x20B040
CFLAG=0x20D040; CFLAGP=0x808080; CWHITE=0xFFFFFF; CBLACK=0x000000; CASTLE=0x808080
LC=120; LR=22
EMPTY=0; GROUND=1; GROUND_TOP=2; BRICK=3; QMARK_COIN=4; QMARK_USED=5
PIPE_TL=6; PIPE_TR=7; PIPE_BL=8; PIPE_BR=9; FLAGPOLE=10; FLAG_TOP=11; COIN_FLT=12
ENEMY_SPAWN=13; CASTLE_BLK=14
CW=256; CH=134; CS=4
g_level=None; g_cc={}; g_canvas=None; g_buf=None; g_fskip=0
GAME_TITLE=0; GAME_PLAY=1; GAME_OVER=2; GAME_WIN=3
g_state=GAME_TITLE; g_cam_x=0.0; g_score=0; g_coins=0; g_lives=3; g_time=300
g_frame=0; g_anim=0; g_canim=0; g_inL=False; g_inR=False; g_inJ=False; g_inRun=False; g_jprs=False
g_cfx=[]; g_part=[]; g_enemies=[]; g_hl={}; g_mario=None; g_ftimer=0; g_last=0; g_page=None
def build_level():
    global g_level; g_level=[]
    for r in range(LR): g_level.append([EMPTY]*LC)
    for c in range(LC): g_level[LR-2][c]=GROUND; g_level[LR-1][c]=GROUND; g_level[LR-3][c]=GROUND_TOP
    for g in[(24,25),(53,54),(82,83)]:
        for c in range(g[0],g[1]+1):
            if c<LC:
                for r in range(LR-4,LR): g_level[r][c]=EMPTY
    for c in range(8,13): g_level[LR-7][c]=BRICK
    g_level[LR-7][10]=QMARK_COIN
    for c in range(16,20): g_level[LR-7][c]=QMARK_COIN if c==17 else BRICK
    for c in range(20,25): g_level[LR-11][c]=QMARK_COIN if c==22 else BRICK
    for c in range(30,36): g_level[LR-10][c]=BRICK
    g_level[LR-10][33]=QMARK_COIN
    for step in range(6):
        for j in range(step+1):
            rr=LR-8-step; cc=42+j
            if 0<=cc<LC and 0<=rr<LR: g_level[rr][cc]=BRICK
    for pc,ph in[(19,3),(38,4),(65,3)]:
        for rh in range(ph):
            r=LR-4-rh
            if r>=0 and r<LR:
                if rh==0: g_level[r][pc]=PIPE_TL; g_level[r][pc+1]=PIPE_TR
                else: g_level[r][pc]=PIPE_BL; g_level[r][pc+1]=PIPE_BR
    spots=[(12,LR-9),(14,LR-9),(35,LR-10),(50,LR-9),(70,LR-8),(72,LR-8)]
    for sc,sr in spots:
        if 0<=sc<LC and 0<=sr<LR: g_level[sr][sc]=COIN_FLT
    for c in[12,22,34,42,58,68,85]:
        if c<LC: g_level[LR-4][c]=ENEMY_SPAWN
    for rh in range(12):
        r=LR-4-rh
        if r>=0: g_level[r][95]=FLAGPOLE
    g_level[LR-15][95]=FLAG_TOP
    for cr in range(LR-8,LR-2):
        if cr>=0 and cr<LR:
            for cc in range(100,106):
                if cc<LC: g_level[cr][cc]=CASTLE_BLK
    if LR-6>=0: g_level[LR-6][101]=EMPTY
    if LR-5>=0: g_level[LR-5][101]=EMPTY
def engine_init(parent):
    global g_canvas,g_buf
    g_canvas=lv.canvas(parent)
    g_canvas.set_size(GW,GH)
    g_canvas.set_pos(0,HH)
    g_buf=bytearray(CW*CH*2)
    g_canvas.set_buffer(g_buf,CW,CH,7)
def fill(x,y,w,h,c):
    if x<0: w=w+x; x=0
    if y<0: h=h+y; y=0
    if x+w>CW: w=CW-x
    if y+h>CH: h=CH-y
    if w<=0 or h<=0: return
    lo=c&0xFF; hi=(c>>8)&0xFF
    row_b=bytearray([lo,hi]*w)
    stride=CW*2
    for row in range(y,y+h):
        base=(row*stride)+(x*2)
        g_buf[base:base+w*2]=row_b
def cls(color): fill(0,0,CW,CH,color)
def flush():
    global g_fskip
    if g_canvas is None: return
    g_fskip=g_fskip+1
    if g_fskip<12: return
    g_fskip=0
    g_canvas.set_buffer(g_buf,CW,CH,7)
def dr_tile(col,row,sx,sy,t):
    if t==GROUND: fill(sx,sy,CS,CS,CGROUND)
    elif t==GROUND_TOP: fill(sx,sy,CS,CS,CGROUND); fill(sx,sy,CS,1,CGTOP)
    elif t==BRICK: fill(sx,sy,CS,CS,CBRICK); fill(sx,sy+2,CS,1,CMORTAR)
    elif t==QMARK_COIN: fill(sx,sy,CS,CS,CQMARK); fill(sx+1,sy+1,2,2,CQSYM)
    elif t==QMARK_USED: fill(sx,sy,CS,CS,0x6B5A3E)
    elif t in(PIPE_TL,PIPE_TR,PIPE_BL,PIPE_BR): fill(sx,sy,CS,CS,CPIPE)
    elif t==COIN_FLT:
        key=(col,row)
        if key not in g_cc: fill(sx+1,sy+1,2,2,CCOIN)
    elif t==FLAGPOLE: fill(sx+2,sy,1,CS,CFLAGP)
    elif t==FLAG_TOP: fill(sx+1,sy,2,CS,CFLAG)
    elif t==CASTLE_BLK: fill(sx,sy,CS,CS,CASTLE)
def dr_world():
    cx=int(g_cam_x/8); sc=max(0,cx//CS-1); ec=min(LC,sc+70)
    for col in range(sc,ec):
        sx=col*CS-cx
        for row in range(LR):
            t=g_level[row][col]
            if t!=EMPTY: dr_tile(col,row,sx,row*CS,t)
def dr_mario(m):
    mx=int((m.x-g_cam_x)/8); my=int(m.y/8)
    if m.dead: fill(mx,my,3,4,CRED); return
    fill(mx+1,my,2,3,CRED)
    if not m.on_g: fill(mx,my-1,3,1,CWHITE)
    fill(mx+1,my+3,1,1,CBRN)
def dr_enemies():
    for e in g_enemies:
        if not e.alive: continue
        ex=int((e.x-g_cam_x)/8); ey=int(e.y/8)
        if e.squished: fill(ex,ey+3,3,1,CGOOMBA)
        else: fill(ex,ey,3,3,CGOOMBA)
def dr_effects():
    for ce in g_cfx: fill(int((ce.x-g_cam_x)/8),int(ce.y/8),1,1,CCOIN)
    for p in g_part: fill(int((p.x-g_cam_x)/8),int(p.y/8),1,1,p.c)
def reset_game():
    global g_score,g_coins,g_lives,g_time,g_frame,g_anim,g_canim,g_cam_x,g_enemies,g_cfx,g_part,g_state
    g_score=0; g_coins=0; g_lives=3; g_time=300
    g_frame=0; g_anim=0; g_canim=0; g_cam_x=0.0
    g_enemies.clear(); g_cfx.clear(); g_part.clear()
    g_state=GAME_PLAY; build_level()
class Mario:
    def __init__(s): s.x=80.0; s.y=300.0; s.vx=0.0; s.vy=0.0; s.w=28; s.h=32; s.on_g=False; s.fr=True; s.dead=False; s.dt=0; s.wt=0; s.won=False
    def rect(s): return(int(s.x),int(s.y),s.w,s.h)
    def frct(s): return(int(s.x)+4,int(s.y)+28,s.w-8,8)
    def rst(s): s.x=80.0; s.y=300.0; s.vx=0.0; s.vy=0.0; s.on_g=False; s.fr=True; s.dead=False; s.dt=0; s.wt=0; s.won=False
class Enemy:
    def __init__(s,x,y): s.x=float(x); s.y=float(y); s.vx=-ENEMY_SPD; s.vy=0.0; s.w=28; s.h=28; s.alive=True; s.squished=False; s.st=0
    def rect(s): return(int(s.x),int(s.y),s.w,s.h)
class CoinFx:
    def __init__(s,x,y): s.x=x; s.y=y; s.vy=-6.0; s.life=30
class Particle:
    def __init__(s,x,y,vx,vy,c): s.x=x; s.y=y; s.vx=vx; s.vy=vy; s.life=20; s.c=c
def rnd(a,b): return a+random.random()*(b-a)
def spawn_coin(x,y): g_cfx.append(CoinFx(x,y))
def spawn_enemies():
    global g_enemies
    cx=int(g_cam_x); sc=max(0,cx//TS-2); ec=min(LC,sc+42)
    sp=[]
    for e in g_enemies: sp.append((int(e.x//TS),int(e.y//TS)))
    row=LR-4
    for col in range(sc,ec):
        if g_level[row][col]==ENEMY_SPAWN:
            key=(col,row); ok=1
            for s in sp:
                if s[0]==key[0] and s[1]==key[1]: ok=0; break
            if ok: g_enemies.append(Enemy(col*TS,row*TS)); sp.append(key)
def overlap(a,b):
    return not(a[0]+a[2]<=b[0] or b[0]+b[2]<=a[0] or a[1]+a[3]<=b[1] or b[1]+b[3]<=a[1])
def gtile(c,r):
    if 0<=c<LC and 0<=r<LR: return g_level[r][c]
    if r>=LR: return GROUND
    return EMPTY
def tsolid(t):
    if t==GROUND or t==GROUND_TOP or t==BRICK or t==QMARK_COIN or t==QMARK_USED: return True
    if t==PIPE_TL or t==PIPE_TR or t==PIPE_BL or t==PIPE_BR: return True
    if t==CASTLE_BLK or t==FLAGPOLE: return True
    return False
def tile_rect(c,r): return(c*TS,r*TS,TS,TS)
def col_x(ent):
    rx=ent.rect()
    for dx,dy in[(0,2),(0,rx[3]-2),(rx[2],2),(rx[2],rx[3]-2)]:
        tc=rx[0]+dx; tr=rx[1]+dy; tc=tc//TS; tr=tr//TS
        if tsolid(gtile(tc,tr)):
            if ent.vx>0: ent.x=tc*TS-rx[2]
            elif ent.vx<0: ent.x=(tc+1)*TS
            ent.vx=0; return True
    return False
def col_y(ent):
    global g_coins,g_score
    rx=ent.rect(); og=False
    ft=(rx[0]+2,rx[1]+rx[3]); ft2=(rx[0]+rx[2]-2,rx[1]+rx[3])
    for px,py in[ft,ft2]:
        tc=px//TS; tr=py//TS
        if tsolid(gtile(tc,tr)): ent.y=tr*TS-rx[3]; ent.vy=0; og=True; break
    if not og:
        hd=(rx[0]+2,rx[1]); hd2=(rx[0]+rx[2]-2,rx[1])
        for px,py in[hd,hd2]:
            tc=px//TS; tr=py//TS; t=gtile(tc,tr)
            if tsolid(t):
                ent.y=(tr+1)*TS; ent.vy=0
                qt=g_level[tr][tc]
                if t==QMARK_COIN and qt==QMARK_COIN:
                    g_level[tr][tc]=QMARK_USED
                    spawn_coin(tc*TS+TS//2,tr*TS-16)
                    g_coins=g_coins+1; g_score=g_score+200
                if t==BRICK: g_level[tr][tc]=EMPTY; g_score=g_score+50
                break
    return og
def up_phys(m):
    global g_cam_x,g_jprs,g_state,g_score,g_lives,g_enemies,g_cfx,g_part
    if m.won:
        m.wt=m.wt+1
        if m.wt>120: g_score=g_score+g_time*10; g_state=GAME_WIN
        return
    if m.dead:
        m.dt=m.dt+1; m.vy=m.vy+GRAVITY; m.y=m.y+m.vy
        if m.dt>90:
            g_lives=g_lives-1
            if g_lives<=0: g_state=GAME_OVER
            else: m.rst(); g_cam_x=0.0; g_enemies.clear(); g_cfx.clear(); g_part.clear()
        return
    spd=RUN_SPD if g_inRun else MOVE_SPD
    if g_inL: m.vx=-spd; m.fr=False
    elif g_inR: m.vx=spd; m.fr=True
    else:
        m.vx=m.vx*0.7
        if abs(m.vx)<0.3: m.vx=0
    if g_jprs and m.on_g: m.vy=JUMP_VEL; m.on_g=False; g_jprs=False
    else: g_jprs=False
    if not g_inJ and m.vy<-3: m.vy=m.vy*0.85
    m.vy=m.vy+GRAVITY
    if m.vy>MAX_FALL: m.vy=MAX_FALL
    m.x=m.x+m.vx; col_x(m); m.y=m.y+m.vy; m.on_g=col_y(m)
    if m.x<0: m.x=0
    if m.x>LC*TS-m.w: m.x=LC*TS-m.w
    if m.y>GH+64: m.dead=True; m.vy=JUMP_VEL*0.6; m.vx=0
    tgt=m.x-GW//3; g_cam_x=g_cam_x+(tgt-g_cam_x)*0.1
    if g_cam_x<0: g_cam_x=0
    mcam=LC*TS-GW
    if mcam<0: mcam=0
    if g_cam_x>mcam: g_cam_x=float(mcam)
    mr=m.rect()
    for rh in range(12):
        r=LR-4-rh
        if 0<=r<LR:
            tr=tile_rect(95,r)
            if overlap(mr,tr): m.won=True; m.vx=0; m.vy=0; g_score=g_score+2000; return
def up_enemies():
    for e in g_enemies:
        if not e.alive: continue
        if e.squished: e.st=e.st+1
        if e.st>30: e.alive=False; continue
        if e.squished: continue
        e.vy=e.vy+GRAVITY
        if e.vy>MAX_FALL: e.vy=MAX_FALL
        e.y=e.y+e.vy; er=e.rect()
        fy=er[1]+er[3]; tc=(er[0]+er[2]//2)//TS; tr=fy//TS
        if tsolid(gtile(tc,tr)): e.y=tr*TS-er[3]; e.vy=0
        elif fy>GH: e.alive=False; continue
        e.x=e.x+e.vx; er=e.rect()
        if e.vx<0: cx=er[0]
        else: cx=er[0]+er[2]
        tc=cx//TS; tr=(er[1]+er[3]-2)//TS
        if tsolid(gtile(tc,tr)): e.vx=-e.vx; e.x=e.x+e.vx*2
        if e.vx>0: ex=er[0]+er[2]
        else: ex=er[0]
        btc=ex//TS; btr=(er[1]+er[3]+2)//TS
        if not tsolid(gtile(btc,btr)): e.vx=-e.vx
def chk_mario_enemy(m):
    global g_score
    mr=m.rect(); fr=m.frct()
    for e in g_enemies:
        if not e.alive or e.squished: continue
        er=e.rect()
        if overlap(mr,er):
            if m.vy>0 and overlap(fr,er): e.squished=True; e.st=0; e.vx=0; m.vy=-7; g_score=g_score+100
            elif not m.dead and not m.won: m.dead=True; m.vy=JUMP_VEL*0.7; m.vx=0
def chk_coins(m):
    global g_coins,g_score
    mr=m.rect()
    cs=max(0,int(m.x//TS)-1); ce=min(LC,int((m.x+m.w)//TS)+2)
    rs=max(0,int(m.y//TS)-1); re=min(LR,int((m.y+m.h)//TS)+2)
    for row in range(rs,re):
        for col in range(cs,ce):
            if g_level[row][col]==COIN_FLT:
                key=(col,row)
                if key in g_cc: continue
                tr=tile_rect(col,row)
                tr2=(tr[0]+4,tr[1]+4,tr[2]-8,tr[3]-8)
                if overlap(mr,tr2): g_cc[key]=True; g_coins=g_coins+1; g_score=g_score+100; spawn_coin(tr[0]+TS//2,tr[1]+TS//2)
def up_effects():
    for ce in g_cfx[:]:
        ce.y=ce.y+ce.vy; ce.vy=ce.vy+0.3; ce.life=ce.life-1
        if ce.life<=0: g_cfx.remove(ce)
    for p in g_part[:]:
        p.x=p.x+p.vx; p.y=p.y+p.vy; p.vy=p.vy+GRAVITY*0.8; p.life=p.life-1
        if p.life<=0: g_part.remove(p)
def _cb_L(e):
    global g_inL
    if e.code==lv.EVENT.PRESSED: g_inL=True
    elif e.code==lv.EVENT.RELEASED: g_inL=False
def _cb_R(e):
    global g_inR
    if e.code==lv.EVENT.PRESSED: g_inR=True
    elif e.code==lv.EVENT.RELEASED: g_inR=False
def _cb_J(e):
    global g_inJ,g_jprs
    if e.code==lv.EVENT.PRESSED: g_inJ=True; g_jprs=True
    elif e.code==lv.EVENT.RELEASED: g_inJ=False
def _cb_Run(e):
    global g_inRun
    if e.code==lv.EVENT.PRESSED: g_inRun=True
    elif e.code==lv.EVENT.RELEASED: g_inRun=False
def _btn(pg,t,x,y,w,h,c,font,cb):
    b=m5ui.M5Button(text=t,x=x,y=y,w=w,h=h,text_c=0xFFFFFF,bg_c=c,font=font,parent=pg)
    try: b.set_style_radius(10,0)
    except: pass
    b.add_event_cb(cb,lv.EVENT.ALL,None)
    return b
def hud_init(parent):
    F=lv.font_montserrat_16; BG=0x1A1A2E
    bar=m5ui.M5Label(text="",x=0,y=0,text_c=CWHITE,bg_c=BG,bg_opa=255,font=F,parent=parent)
    bar.set_size(SW,HH)
    g_hl['sc']=m5ui.M5Label(text="000000",x=10,y=5,text_c=CWHITE,bg_c=BG,bg_opa=0,font=F,parent=parent)
    g_hl['tm']=m5ui.M5Label(text="TIME 300",x=500,y=5,text_c=CWHITE,bg_c=BG,bg_opa=0,font=F,parent=parent)
    g_hl['lv']=m5ui.M5Label(text="x3",x=750,y=5,text_c=0xE01020,bg_c=BG,bg_opa=0,font=F,parent=parent)
def hud_upd():
    g_hl['sc'].set_text("%06d" % g_score)
    g_hl['tm'].set_text("TIME %d" % g_time)
    g_hl['lv'].set_text("x%d" % g_lives)
def setup():
    global g_page,g_mario
    M5.begin(); Widgets.setRotation(1); m5ui.init()
    g_page=m5ui.M5Page(bg_c=0x1A1A2E)
    engine_init(g_page)
    hud_init(g_page)
    F=lv.font_montserrat_36
    _btn(g_page,"<",40,SH-210,100,100,0x333366,F,_cb_L)
    _btn(g_page,">",240,SH-210,100,100,0x333366,F,_cb_R)
    _btn(g_page,"B",SW-320,SH-220,110,110,0xCC0000,F,_cb_Run)
    _btn(g_page,"A",SW-180,SH-290,130,130,0x3CB043,F,_cb_J)
    g_page.screen_load()
    build_level(); g_mario=Mario()
    print("=== Mario Ready! ===")
def game_loop():
    global g_frame,g_anim,g_canim,g_time,g_ftimer,g_last,g_state,g_jprs
    now=time.ticks_ms()
    dt=time.ticks_diff(now,g_last)
    if dt<150: return
    g_last=now
    if g_state==GAME_TITLE:
        if g_jprs: g_jprs=False; reset_game(); g_mario.rst(); hud_upd()
        return
    if g_state in(GAME_OVER,GAME_WIN):
        if g_jprs: g_jprs=False; reset_game(); g_mario.rst(); hud_upd()
        return
    g_frame=g_frame+1; g_anim=g_anim+1; g_canim=g_canim+1; g_ftimer=g_ftimer+1
    if g_ftimer>=8:
        g_ftimer=0; g_time=g_time-1
        if g_time<=0: g_mario.dead=True; g_mario.vy=JUMP_VEL*0.7; g_mario.vx=0; g_time=0
    spawn_enemies(); up_phys(g_mario); up_enemies(); chk_mario_enemy(g_mario)
    chk_coins(g_mario); up_effects()
    cls(CSKY); dr_world(); dr_enemies(); dr_effects(); dr_mario(g_mario)
    flush(); hud_upd()
def loop():
    M5.update(); game_loop()
setup()
while True: loop()
