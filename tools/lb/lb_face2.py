# -*- coding: utf-8 -*-
"""M3 ① 이어서 — 후보 0x0386 이 «되돌아오는가».

KOF 의 사고가 정확히 여기서 났다: 0x0D4A 는 조건이 바뀔 때 «올라가기만» 하고
안 내려오는 플래그였는데, 스냅숏 두 장만 보고 반전이라 불렀다.
그러니 **넘어갔다가 다시 넘어와** 값이 0 → 1 → 0 으로 돌아오는지 본다.

같이 보는 것:
  0x0370 P1 act  ·  0x0386 후보  ·  0x03B0 (P1+0x40 = P2 act 인가)  ·  0x03C6 (후보+0x40)
  0x036A 가로 위치 — 교차 증인. 반전이 바뀌는 «그 순간»에 위치가 상대를 지나야 한다.
"""
import io, os, shutil, subprocess, sys, tempfile

RUN = "/mnt/c/Claude/KOF R2 한글/tools/ngprun"
CORE = os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM = os.path.expanduser("~/ss2/rom/pristine/Last Blade, The (UE) [!].ngc")
ST = os.path.expanduser("~/ss2/saves/lb/lb_train.st")

OFFS = [0x0370, 0x0386, 0x03B0, 0x03C6, 0x036A, 0x03EA]
NAMES = ["P1act", "후보386", "0x3B0", "후보3C6", "위치36A", "위치3EA"]


def run(script):
    env = dict(os.environ); env["NGP_OPTS"] = "ngp_ss2sp=enabled"
    t = tempfile.mkdtemp(); r = os.path.join(t, "r.ngc"); shutil.copy(ROM, r)
    io.open(os.path.join(t, "s.txt"), "w", encoding="utf-8", newline="\n").write(
        "\n".join(script) + "\n")
    subprocess.run([RUN, CORE, r, os.path.join(t, "s.txt"), os.path.join(t, "g")],
                   capture_output=True, text=True, env=env)
    p = os.path.join(t, "gt.csv")
    if not os.path.exists(p):
        return None
    return [l.split(",") for l in io.open(p).read().strip().splitlines()][1:]


def show(rows, title):
    print("\n== %s ==" % title)
    print("f      " + "".join("%-9s" % n for n in NAMES))
    prev = None
    shown = 0
    for r in rows:
        v = tuple(r[2:2 + len(OFFS)])
        if v != prev:
            print("%-6s " % r[0] + "".join("%-9s" % x for x in v))
            prev = v; shown += 1
        if shown > 45:
            print("  …(줄임)"); break


ph = int(sys.argv[1]) if len(sys.argv) > 1 else 0
offs = ",".join("%X" % a for a in OFFS)

# 넘어갔다 → 다시 넘어온다
sc = (["!load %s" % ST, "!w t %s" % offs, "%d -" % (10 + ph)]
      + ["60 R", "30 U R", "60 -"]        # 앞으로 뛰어넘는다
      + ["60 L", "30 U L", "60 -"]        # 뒤로 뛰어 되넘는다
      + ["!w off"])
rows = run(sc)
if rows is None:
    print("★ csv 없음"); sys.exit(1)
show(rows, "위상 %d · 넘어갔다 되넘기" % ph)

# 대조군 — 가만히 있으면 아무것도 안 바뀌어야 한다
sc2 = ["!load %s" % ST, "!w t %s" % offs, "%d -" % (10 + ph), "300 -", "!w off"]
rows2 = run(sc2)
show(rows2, "대조군 · 가만히 (바뀌면 안 된다)")
