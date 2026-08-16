#!/usr/bin/env python3
"""SP 모드 검증 — 버튼 하나(X) + 방향으로 슬롯이 갈리는지.

SS2SP_LAYOUT=sp 로 돌려야 한다.
"""
import os, sys, ctypes as C
sys.path.insert(0, '/home/claude/ngp/re')
from frontend import Emu, JOY
from comlib import hook_push

STATE = os.environ.get('SS2_STATE', '/home/claude/ngp/re/states/nav_fight.bin')
ACT, Y, FACING = 0x0E3E, 0x0E3A, 0x0E54
SPBTN = JOY['X']

def ram(e): return C.string_at(e.lib.retro_get_memory_data(2), 16384)
def act(e):
    r = ram(e); return r[ACT] | (r[ACT+1] << 8)

def trial(e, dirs, prewalk=0):
    e.load_state(open(STATE, 'rb').read()); e.set_pad(); e.run_frames(6)
    if prewalk:
        back = JOY['RIGHT'] if ram(e)[FACING] == 1 else JOY['LEFT']
        e.pad[0] = {back: 1}; e.run_frames(prewalk); e.pad[0] = {}
    a0 = act(e)
    held = {JOY[d]: 1 for d in dirs}
    e.pad[0] = dict(held); e.pad[0][SPBTN] = 1
    e.run_frames(3)
    e.pad[0] = {}
    for _ in range(80):
        e.run_frames(1)
        a = act(e)
        if a >= 0x180 and a != a0:
            return a
    return 0

def main():
    e = Emu(); hook_push(e); e._push_x = 0x180
    print("배치:", "SP(방향+X)" if os.environ.get('SS2SP_LAYOUT') == 'sp' else "직결")
    print()
    cases = [("중립", []), ("앞(→)", ['RIGHT']), ("뒤(←)", ['LEFT']),
             ("아래(↓)", ['DOWN']), ("위(↑)", ['UP'])]
    for label, dirs in cases:
        clean = trial(e, dirs)
        walked = [trial(e, dirs, w) for w in (12, 22, 30)]
        ok = sum(1 for a in walked if a and a == clean)
        print(f"  {label:9s} → act={clean:#05x}   뒤걷기 후 {[hex(a) for a in walked]}   일치 {ok}/3")

main()
