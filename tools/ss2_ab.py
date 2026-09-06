# -*- coding: utf-8 -*-
"""SS2 — R·L·X·Y 가 SP 켬/끔에서 각각 무엇을 하나.

⚠ 첫 시도에서 **모든 줄이 글자 그대로 같았다.** 그건 「같다」가 아니라 «못 쟀다»다 —
  입력이 안 닿는 상태(연출 중 등)일 수 있다. 그래서:
    · **대조군(아무것도 안 누름)**을 넣는다. 그것과도 같으면 재고 있지 않은 것이다.
    · **화면을 같이 찍는다.** 내가 그 화면에 도착했는지부터 본다(traps ⑩).
"""
import io, os, shutil, subprocess, sys, tempfile

RUN = "/mnt/c/Claude/KOF R2 한글/tools/ngprun"
CORE = os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM = os.path.expanduser("~/ss2/rom/ss2.ngc")
ST = os.path.expanduser("~/ss2/saves/ss2/ss2_fight.st")
OUT = os.path.expanduser("~/ss2/tmp/ss2ab")
ACT = 0x0E3E                      # 16비트. 필살기 >= 0x180

ON = "ngp_ss2sp=enabled"
OFF = "ngp_ss2sp=disabled"


def run(keys, opts, ph=0, hold=8, lead=240, shot=None):
    env = dict(os.environ)
    env["NGP_OPTS"] = opts
    sc = ["!load %s" % ST, "!w t %X,%X" % (ACT, ACT + 1), "%d -" % (lead + ph)]
    if shot:
        sc += ["!s"]
    sc += (["%d %s" % (hold, keys)] if keys != "-" else ["%d -" % hold])
    sc += ["70 -", "!w off"]
    t = tempfile.mkdtemp()
    r = os.path.join(t, "r.ngc")
    shutil.copy(ROM, r)
    io.open(os.path.join(t, "s.txt"), "w", encoding="utf-8", newline="\n").write(
        "\n".join(sc) + "\n")
    subprocess.run([RUN, CORE, r, os.path.join(t, "s.txt"), os.path.join(t, "g")],
                   capture_output=True, text=True, env=env)
    if shot:
        os.makedirs(OUT, exist_ok=True)
        for f in os.listdir(t):
            if f.endswith(".ppm"):
                shutil.copy(os.path.join(t, f), os.path.join(OUT, shot + ".ppm"))
                break
    p = os.path.join(t, "gt.csv")
    if not os.path.exists(p):
        return None
    rows = [l.split(",") for l in io.open(p).read().strip().splitlines()][1:]
    out, prev = [], None
    for r2 in rows:
        v = int(r2[2]) | (int(r2[3]) << 8)
        if v != prev:
            out.append(v); prev = v
    return out


JOBS = [("★대조군: 아무것도", "-"),
        ("NGP A+B 직접(대본 A B)", "A B"),
        ("NGP A 단독(대본 B)", "B"),
        ("X", "X"), ("Y", "Y"), ("L1", "L1"), ("R1", "R1")]


def main():
    lead = int(sys.argv[1]) if len(sys.argv) > 1 else 240
    print("SS2 · ss2_fight.st · act 0x0E3E(16비트) · 필살기 >= 0x180 · 리드인 %d\n" % lead)
    base = None
    for ph in (0, 1):
        for name, keys in JOBS:
            for lab, opts in (("SP 켬", ON), ("SP 끔", OFF)):
                v = run(keys, opts, ph, lead=lead,
                        shot=("shot_%s" % lab.replace(" ", "")) if (ph == 0 and keys == "-") else None)
                if v is None:
                    print("  위상%d %-24s %-6s 못 쟀다(csv 없음)" % (ph, name, lab)); continue
                s = " ".join(hex(x) for x in v[:6])
                if base is None and keys == "-" and lab == "SP 켬":
                    base = s
                mark = "  ← 대조군과 같다(입력이 안 닿았다)" if (s == base and keys != "-") else ""
                print("  위상%d %-24s %-6s %s%s" % (ph, name, lab, s, mark))
        print()
    print("화면: %s" % OUT)


if __name__ == "__main__":
    main()
