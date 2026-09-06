# -*- coding: utf-8 -*-
"""월화 M0 ③ — 램 주소 사냥.

절차(MECH.md §1):
  ① **같은 상태를 두 번** 떠서 저절로 흔들리는 바이트를 먼저 뺀다.
     (코어가 호스트 시각을 읽어 실행마다 몇 바이트가 흔들린다 — SvC 9 · SS2 3)
  ② 조건 하나만 바꿔 diff 한다. 남는 것이 후보다.

⚠ 겉모습으로 정하지 마라. 승격은 「독립 시나리오 2개 + 위상 2종 + 교차 증인」이다.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lb_run as R

ST = os.path.expanduser("~/ss2/saves/lb/lb_train.st")
TMP = os.path.expanduser("~/ss2/tmp/lb/hunt")


def shoot(name, acts, wait=90, phase=0):
    """세이브를 불러 acts 를 넣고 wait 프레임 뒤 램을 뜬다. phase 는 위상(0/1)."""
    sc = ["!load %s" % ST]
    if phase: sc += ["%d -" % phase]
    sc += acts + ["%d -" % wait, "!%s" % name]
    R.run(sc, TMP, keep=("ram", "ppm"))
    return open("%s/%s.ram" % (TMP, name), "rb").read()


def diff(a, b):
    return {i for i in range(len(a)) if a[i] != b[i]}


if __name__ == "__main__":
    # ① 잡음 — 같은 대본 두 번
    n1 = shoot("rest1", ["60 -"])
    n2 = shoot("rest2", ["60 -"])
    noise = diff(n1, n2)
    print("잡음 바이트 %d개%s" % (len(noise), (" " + ", ".join("0x%04X" % i for i in sorted(noise)[:12])) if noise else ""))

    # ② 조건별
    jobs = [("walkF", ["90 R"]),          # 앞으로 걷기
            ("walkB", ["90 L"]),          # 뒤로 걷기
            ("crouch", ["90 D"]),         # 웅크림
            ("jump",  ["30 U", "60 -"]),  # 점프
            ("btnB",  ["6 B", "60 -"]),   # NGP A
            ("btnA",  ["6 A", "60 -"])]   # NGP B
    base = shoot("base", ["60 -"])
    for name, acts in jobs:
        d = diff(base, shoot(name, acts)) - noise
        print("%-7s 다른 바이트 %4d개  %s" % (name, len(d), ", ".join("0x%04X" % i for i in sorted(d)[:16])))
