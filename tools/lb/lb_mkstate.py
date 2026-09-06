# -*- coding: utf-8 -*-
"""월화 M0 ② — 트레이닝 무대 세이브를 굽는다.

길(실측): 제목(4500f) → A → 1P 메뉴 → A → 모드 선택 → **아래 3칸** → TRAINING
        → A → Player Select → A(카에데) → Ability Select → A → 무대

세이브 둘을 뜬다:
  lb_psel.st   Player Select 바탕 — 나중에 전 캐릭을 여기서 굽는다(부팅 한 번으로)
  lb_train.st  싸움이 선 뒤 — 모든 측정은 여기서 시작한다

⚠ 아케이드 세이브는 못 쓴다(CPU 가 반격한다). 트레이닝은 게임이 허수아비를 준다.
"""
import os
import lb_run as R

SAVE = os.path.expanduser("~/ss2/saves/lb")
os.makedirs(SAVE, exist_ok=True)

TO_MODE = ["%d -" % R.TITLE, "8 B", "60 -", "8 B", "60 -"]          # 제목 → 모드 선택
TO_TRAIN = TO_MODE + ["8 D", "20 -"] * 3 + ["8 B", "90 -"]          # 모드 선택 → TRAINING
TO_PSEL = TO_TRAIN                                                   # 여기가 Player Select

if __name__ == "__main__":
    # ⚠ Player Select 에서 **다섯 번** 눌러야 무대다(한 칸씩 찍어 확인했다):
    #   A(P1 캐릭) → A(확정) → **Ability Select** → A(P2 캐릭) → A(확정) → 무대
    #   두 번만 누르고 「무대」라고 이름 붙였다가 Ability Select 를 찍었다.
    sc = (TO_PSEL + ["!save %s/lb_psel.st" % SAVE, "!psel"]
          + ["8 B", "110 -", "!s1"]
          + ["8 B", "110 -", "!abil"]
          + ["8 B", "110 -", "!p2"]
          + ["8 B", "110 -", "!s4"]
          + ["8 B", "180 -", "!go"]
          + ["240 -", "!train", "!save %s/lb_train.st" % SAVE])
    p, n = R.run(sc, "/home/dudu/ss2/tmp/lb/state")
    for ln in p.stdout.strip().splitlines():
        if "저장" in ln: print(ln)
    for f in sorted(os.listdir(SAVE)):
        print("  ", f, os.path.getsize(os.path.join(SAVE, f)), "바이트")
