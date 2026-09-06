# -*- coding: utf-8 -*-
"""월화 M0 ⑤ — 공략집이 적은 커맨드를 **에뮬레이터로 검산**한다.

공략집(GameFAQs, L.S.D, 2000)은 **가설이지 증명이 아니다.** 게다가 그 글쓴이는
일본판을 썼다고 적어 뒀고 우리 대조군은 UE 판이다. 그러니 하나씩 넣어 보고
**동작 ID(0x0370)가 기본기와 다른 값으로 가는지**로 가른다.

버튼: 공략집 A=SLASH · B=KICK. 하네스 문자는 레트로 기준이라 **NGP A = 'B' · NGP B = 'A'**.
P1 은 왼쪽이라 앞 = R, 뒤 = L.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lb_run as R

ST = os.path.expanduser("~/ss2/saves/lb/lb_train.st")
TMP = os.path.expanduser("~/ss2/tmp/lb/moves")
ACT, HIT, CUR_LO, CUR_HI = 0x0370, 0x037C, 0x0F60, 0x0F61
SLASH, KICK = "B", "A"          # 하네스 문자 (NGP A / NGP B)

# 방향 한 칸을 몇 프레임 잡을지 — 링이 2프레임에 한 칸이라 4면 두 칸이 찬다
STEP = 4
QCF = ["D", "D R", "R"]
QCB = ["D", "D L", "L"]
DP  = ["R", "D", "D R"]          # F,D,DF = 623
RDP = ["L", "D", "D L"]          # B,D,DB = 421
HCF = ["L", "D L", "D", "D R", "R"]
HCB = ["R", "D R", "D", "D L", "L"]


def motion(steps, btn, step=STEP):
    return ["%d %s" % (step, s) for s in steps] + ["6 %s" % btn]


def trial(name, acts, tail=70):
    offs = ",".join("%X" % a for a in (ACT, HIT, CUR_LO, CUR_HI))
    R.run(["!load %s" % ST, "!w t %s" % offs] + acts + ["%d -" % tail, "!w off"], TMP, keep=("csv",))
    rows = [l.split(",") for l in open("%s/t.csv" % TMP).read().strip().splitlines()][1:]
    seq, prev = [], None
    for r in rows:
        v = int(r[2])
        if v != prev: seq.append((int(r[0]), v)); prev = v
    hit = max(int(r[3]) for r in rows) if rows else 0
    return seq


if __name__ == "__main__":
    jobs = [("기준 · 서서 베기",        ["6 %s" % SLASH]),
            ("기준 · 킥",              ["6 %s" % KICK]),
            ("236+A 질풍",             motion(QCF, SLASH)),
            ("214+A 참격",             motion(QCB, SLASH)),
            ("214+B 동풍",             motion(QCB, KICK)),
            ("623+A 풍아",             motion(DP,  SLASH)),
            ("41236+B 폭풍",           motion(HCF, KICK)),
            ("421+A (표에 없는 것)",    motion(RDP, SLASH))]
    base = None
    for name, acts in jobs:
        seq = trial(name, acts)
        s = " → ".join("%d@f%d" % (v, f) for f, v in seq[:8])
        print("%-22s %s" % (name, s))
