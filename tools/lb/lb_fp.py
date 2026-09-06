# -*- coding: utf-8 -*-
"""M3 ③ 준비 — 카에데 기술마다 «지문 쌍»을 실측한다.

★ act 하나로는 못 가른다. 오늘 확인했다: 평타를 8프레임 이상 쥐어도 act 112 가 나온다
  (강베기). 질풍과 갈리는 것은 **뒤따르는 144** 였다.
  그러니 슬롯 지문은 **(act, 그 다음 act)** 쌍으로 잡는다.

⚠ 근접 기술은 **붙여 놓고** 재야 한다 — 떨어져 있으면 41236+B 가 헛나가
  214+A 와 act 148 로 겹친다(MOVES.md). 그래서 붙는 판과 떨어진 판을 둘 다 잰다.
⚠ 버튼은 «확실한 약 구역»(≤6)에서 4프레임. 7 은 위상에 따라 강약이 갈리는 금지구역이다.
"""
import io, os, shutil, subprocess, sys, tempfile

RUN = os.path.expanduser("~/ss2/repo/ss2-sp-core/tools/kof/ngprun")
CORE = os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM = os.path.expanduser("~/ss2/rom/pristine/Last Blade, The (UE) [!].ngc")
ST = os.path.expanduser("~/ss2/saves/lb/lb_train.st")
ACT, FACE = 0x0370, 0x0386

SLASH, KICK = "B", "A"          # 하네스 문자 — NGP A(베기) / NGP B(킥)
STEP = 4
QCF = ["D", "D R", "R"]
QCB = ["D", "D L", "L"]
DP = ["R", "D", "D R"]          # 623
RDP = ["L", "D", "D L"]         # 421
HCF = ["L", "D L", "D", "D R", "R"]
HCB = ["R", "D R", "D", "D L", "L"]


def run(acts, lead=20, tail=110, close=False):
    env = dict(os.environ); env["NGP_OPTS"] = "ngp_ss2sp=enabled"
    sc = ["!load %s" % ST, "!w t %X,%X" % (ACT, FACE), "%d -" % lead]
    if close:
        sc += ["46 R", "10 -"]          # 붙는다 — 근접 기술용
    sc += acts + ["%d -" % tail, "!w off"]
    t = tempfile.mkdtemp(); r = os.path.join(t, "r.ngc"); shutil.copy(ROM, r)
    io.open(os.path.join(t, "s.txt"), "w", encoding="utf-8", newline="\n").write(
        "\n".join(sc) + "\n")
    subprocess.run([RUN, CORE, r, os.path.join(t, "s.txt"), os.path.join(t, "g")],
                   capture_output=True, text=True, env=env)
    p = os.path.join(t, "gt.csv")
    if not os.path.exists(p):
        return None
    rows = [l.split(",") for l in io.open(p).read().strip().splitlines()][1:]
    seq, prev = [], None
    for r2 in rows:
        v = int(r2[2])
        if v != prev:
            seq.append(v); prev = v
    return seq


def motion(steps, btn, step=STEP, hold=4):
    return ["%d %s" % (step, s) for s in steps] + ["%d %s" % (hold, btn)]


REST = {4, 12, 20, 24}          # 서기·웅크림·앞걷기·뒤걷기


def fp(seq):
    """쉼 값을 빼고 «앞의 둘»을 지문으로 삼는다."""
    if not seq:
        return "(못 쟀다)"
    core = [v for v in seq if v not in REST]
    if not core:
        return "(무반응)"
    return "→".join(str(v) for v in core[:2])


JOBS = [
    ("기준 · 서서 베기(4f)", ["4 %s" % SLASH], False),
    ("기준 · 킥(4f)", ["4 %s" % KICK], False),
    ("236+A 질풍", motion(QCF, SLASH), False),
    ("623+A 풍아", motion(DP, SLASH), False),
    ("214+A 참격", motion(QCB, SLASH), False),
    ("214+B 동풍", motion(QCB, KICK), False),
    ("421+A (표에 없음)", motion(RDP, SLASH), False),
    ("41236+B 폭풍(떨어져)", motion(HCF, KICK), False),
    ("41236+B 폭풍(붙어서)", motion(HCF, KICK), True),
    ("63214+B (표에 없음)", motion(HCB, KICK), False),
]

if __name__ == "__main__":
    print("카에데 지문 (쉼 값 뺀 앞 둘) · 버튼 4프레임(확실한 약 구역)\n")
    seen = {}
    for ph in (0, 1):
        print(" 위상 %d" % ph)
        for name, acts, close in JOBS:
            s = run(acts, lead=20 + ph, close=close)
            f = fp(s)
            print("   %-24s %-14s  전체 %s" % (name, f, " ".join(str(x) for x in (s or [])[:8])))
            seen.setdefault(f, []).append(name)
        print()
    print("★ 지문이 겹치는 것:")
    dup = {k: v for k, v in seen.items() if len(set(v)) > 1}
    for k, v in dup.items():
        print("   %-14s ← %s" % (k, " / ".join(sorted(set(v)))))
    if not dup:
        print("   없음")
