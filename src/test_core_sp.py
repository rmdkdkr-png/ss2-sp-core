#!/usr/bin/env python3
"""코어 내장 SP 엔진 검증.

트리거 버튼(X/Y/L/R/L2/R2)만 누르면 현재 캐릭터 필살기가 나가는지,
그리고 '뒤로 걷다가' 눌러도 하이재킹 없이 나가는지 확인한다.
SS2SP_NORESET=1 로 돌리면 커맨드 버퍼 리셋 없는 대조군.
"""
import os, sys, ctypes as C
sys.path.insert(0, '/home/claude/ngp/re')
from frontend import Emu, JOY
from comlib import hook_push

STATE = os.environ.get('SS2_STATE','/home/claude/ngp/re/states/nav_fight.bin')
TRIG = dict(SP1=JOY['X'], SP2=JOY['Y'], SP3=JOY['L'], SP4=JOY['R'], SP5=12, SP6=13)
ACT, Y, FACING, CHARSTYLE = 0x0E3E, 0x0E3A, 0x0E54, 0x1B51

def ram(e):
    p = e.lib.retro_get_memory_data(2)
    return C.string_at(p, 16384)

def act(e):
    r = ram(e); return r[ACT] | (r[ACT+1] << 8)

def fresh(e):
    e.load_state(open(STATE, 'rb').read())
    e.set_pad()
    e.run_frames(6)
    return e

def press(e, name, frames=3):
    e.pad[0] = {TRIG[name]: 1}
    e.run_frames(frames)
    e.pad[0] = {}

def walk_back(e, frames):
    """뒤로 걷기 — 입력 이력을 4로 오염시킨다 (facing에 따라 좌/우)"""
    back = JOY['RIGHT'] if ram(e)[FACING] == 1 else JOY['LEFT']
    e.pad[0] = {back: 1}
    e.run_frames(frames)
    e.pad[0] = {}

def trial(e, slot, prewalk=0, settle=70):
    fresh(e)
    if prewalk:
        walk_back(e, prewalk)
    a0 = act(e)
    press(e, slot)
    best = 0
    for _ in range(settle):
        e.run_frames(1)
        a = act(e)
        if a >= 0x180 and a != a0:
            best = a
            break
    return best

def main():
    e = Emu()
    hook_push(e)
    e._push_x = 0x180   # COM 중화 
    fresh(e)
    r = ram(e)
    print("캐릭터/유파 바이트 0x1B51 =", r[CHARSTYLE], "→ 스타일 인덱스", r[CHARSTYLE] >> 3)
    print("facing =", r[FACING], " Y =", r[Y])
    tag = "리셋 없음(대조군)" if os.environ.get('SS2SP_NORESET') == '1' else "리셋 적용"
    print("모드:", tag)
    print()
    for slot in ('SP1', 'SP2', 'SP3', 'SP4', 'SP5', 'SP6'):
        clean = trial(e, slot, prewalk=0)
        walked = [trial(e, slot, prewalk=w) for w in (12, 22, 30)]
        ok = sum(1 for a in walked if a == clean and a)
        print(f"  {slot}: 중립 발동 act={clean:#05x}   "
              f"뒤걷기 후 {[hex(a) for a in walked]}   일치 {ok}/3")
    e.deinit() if hasattr(e, 'deinit') else None

main()
