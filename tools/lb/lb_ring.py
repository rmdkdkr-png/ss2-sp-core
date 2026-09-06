# -*- coding: utf-8 -*-
"""월화 M0 ④ — 방향이력 링 확정.

첫 사냥에서 0x135B~0x1369 가 **NGP 패드 비트 그대로**를 담고 있었다
(중립 0 · 앞 8=R · 뒤 4=L · 아래 2=D · 위 1=U · NGP A 16 · NGP B 32).
버튼을 6프레임 잡았을 때 **세 칸**만 찼다 → 두 프레임에 한 칸.

여기서 확정할 것 넷:
  ① 링의 처음과 끝 (몇 칸인가)
  ② 미는 방향 (새것이 낮은 주소인가 높은 주소인가)
  ③ 한 칸이 몇 프레임인가
  ④ 값이 정말 패드 비트인가 (대각선을 넣어 본다)

⚠ 한 칸만 붙잡아 poke 로 증명하려 하지 마라 — KOF 에서 정적으로 붙잡으면 게임이 밀면서
  읽는 시작점이 돌아 사실상 모든 회전을 시도하게 돼 증명이 안 됐다.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lb_run as R

ST = os.path.expanduser("~/ss2/saves/lb/lb_train.st")
TMP = os.path.expanduser("~/ss2/tmp/lb/ring")
LO, HI = 0x1356, 0x1372          # ⚠ !w 의 오프셋 목록은 **159자에서 잘린다** — 30칸이 한계

def watch(name, acts, tail=40, lo=LO, hi=HI):
    """매 프레임 링 대역을 CSV 로 받는다. CSV 열은 frame,pad,<오프셋…> 이라 값은 r[2:] 다."""
    offs = ",".join("%X" % a for a in range(lo, hi))
    assert len(offs) < 159, "오프셋 목록이 159자를 넘는다 — 하네스가 자른다"
    sc = ["!load %s" % ST, "!w %s %s" % (name, offs)] + acts + ["%d -" % tail, "!w off"]
    R.run(sc, TMP, keep=("csv",))
    rows = [l.split(",") for l in open("%s/%s.csv" % (TMP, name)).read().strip().splitlines()]
    return rows[0], rows[1:]


def show(name, rows, lo=LO, hi=HI, last=None):
    print("== %s" % name)
    print("        pad  %s" % " ".join("%02X" % (a & 0xFF) for a in range(lo, hi)))
    for r in rows[:last]:
        vals = [int(v) for v in r[2:]]
        print("f%-4s %3s  %s" % (r[0], r[1], " ".join(("%2d" % v) if v else " ." for v in vals)))


if __name__ == "__main__":
    show("앞 4프레임", watch("tapR", ["4 R"], tail=24)[1], last=26)
