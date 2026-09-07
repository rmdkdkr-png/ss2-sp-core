# -*- coding: utf-8 -*-
"""팀대전에서 «지금 싸우는 캐릭터» 자리를 잰다 (SvC, 2026-09-07).

방법: team_fight.st 를 불러 P1 을 가만 두면 CPU 가 KO 시키고 다음 캐릭터가 나온다.
      0x0880~0x08D0 을 120프레임마다 덤프하고, 명단 근방을 매 프레임 관찰한다.
      교대 앞뒤 화면(PPM)의 HUD 이름이 증인이다 — 바이트가 HUD 와 같이 움직여야 한다.

결과(실측):
    08A0 [00 06 03]  명단 — 안 변함           08A3 [00 06 03]  출전 순서 — 안 변함
    08A6  00→06→03   ★ KO 마다 바뀜(HUD: 쿄→IORI→MAI)   08A9  00→01→02  출전 순서 색인
    P2 도 같은 꼴(08C6 · 08C9). 단일 대전에서는 08A6 == 08A3 == 08A0.

★ 판정 규칙: «어느 바이트가 HUD 와 같이 움직이나»만 본다. 회전하는 사본(+3)의 머리를
  현재로 «가정»했다가 P1 에서 틀렸다 — P2 한 판만 보고 일반화한 것이 사고였다.
"""
import io, os, glob, shutil, subprocess, sys

RUN  = os.environ.get("NGPRUN") or "/mnt/c/Claude/KOF R2 한글/tools/ngprun"
CORE = os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM  = os.path.expanduser("~/ss2/rom/svc.ngc")
ST   = os.path.expanduser("~/ss2/saves/svc/team_fight.st")
W    = os.path.expanduser("~/ss2/work/team")
FRAMES, STEP = 6000, 120
CAND = [0x8A0, 0x8A1, 0x8A2, 0x8A3, 0x8A6, 0x8A9, 0x8C0, 0x8C3, 0x8C6, 0x8C9, 0x882, 0x8B3, 0x968]

def run():
    os.makedirs(W, exist_ok=True)
    r = os.path.join(W, "r.ngc"); shutil.copy(ROM, r)
    sc = ["!load " + ST, "!w t " + ",".join("%X" % c for c in CAND)]
    for i in range(FRAMES // STEP):
        sc += ["!d%02d" % i, "%d -" % STEP]
    sc.append("!w off")
    io.open(os.path.join(W, "s.txt"), "w", encoding="utf-8", newline="\n").write("\n".join(sc) + "\n")
    env = dict(os.environ); env["NGP_OPTS"] = "ngp_ss2sp=enabled,ngp_svcsp_engine=disabled"
    subprocess.run([RUN, CORE, r, os.path.join(W, "s.txt"), os.path.join(W, "g")],
                   capture_output=True, text=True, env=env)

def analyse():
    p = os.path.join(W, "gt.csv")
    if not os.path.exists(p):
        print("못잼 — CSV 가 없다 (하네스가 안 돌았다)"); return 2
    rows = [l.strip().split(",") for l in io.open(p).read().strip().splitlines()]
    hdr, rows = rows[0], rows[1:]
    print("매 프레임 관찰 — 값이 바뀐 프레임만:")
    prev = None
    for r in rows:
        v = tuple(r[2:])
        if v != prev:
            print("  f%5s  %s" % (r[0], " ".join("%s=%s" % (h, x) for h, x in zip(hdr[2:], v))))
            prev = v
    print("\n0x0880~0x08D0 덤프 간 차이:")
    base = None
    for d in sorted(glob.glob(os.path.join(W, "g_d*.ram"))):
        m = open(d, "rb").read()[0x880:0x8D0]
        if base is None: base = m; continue
        if m != base:
            diff = ["%04X:%02X->%02X" % (0x880 + i, base[i], m[i]) for i in range(len(m)) if base[i] != m[i]]
            print("  %s %s" % (os.path.basename(d), diff))
            base = m
    # 판정: 08A6 이 두 번 바뀌고, 08A3 은 한 번도 안 바뀌어야 한다
    a6 = [r[hdr.index("08A6")] for r in rows]; a3 = [r[hdr.index("08A3")] for r in rows]
    ch6 = sum(1 for i in range(1, len(a6)) if a6[i] != a6[i-1])
    ch3 = sum(1 for i in range(1, len(a3)) if a3[i] != a3[i-1])
    print("\n08A6 변화 %d회 · 08A3 변화 %d회 → %s" % (ch6, ch3, "PASS(08A6 이 현재)" if ch6 >= 2 and ch3 == 0 else "FAIL"))
    return 0 if ch6 >= 2 and ch3 == 0 else 1

if __name__ == "__main__":
    if "--no-run" not in sys.argv: run()
    sys.exit(analyse())
