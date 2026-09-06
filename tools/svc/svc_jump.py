# -*- coding: utf-8 -*-
"""SvC ① — 점프 한 판의 프레임 지도.

유저 지적: 「점프 유예 타이밍이 너무 길다」.
짚이는 자리는 `SVC_LAND_WIN 32` — **누른 뒤 «공중에서» 32프레임을 세는 카운트다운**이다.
곧 「착지 −32프레임 안에 누른 것」이 살아남는다.

주석은 「정점 = 착지 −28f」라 한다. 그 값이 맞는지부터 잰다 —
맞으면 창을 28 미만으로 줄이는 순간 **정점 입력이 죽는다.** 그러면 창 길이로는 못 풀고
기준을 바꿔야 한다(예: «하강 중일 때만 무장»).

⚠ 옵션 `ngp_svcsp_land` 는 기본 끔이다. 유저는 켜 놓고 쓰는 중이다.
"""
import io, os, shutil, subprocess, sys, tempfile

RUN = os.path.expanduser("~/ss2/repo/ss2-sp-core/tools/kof/ngprun")
CORE = os.path.expanduser(
    "~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM = os.path.expanduser("~/ss2/rom/svc.ngc")
ST = os.path.expanduser("~/ss2/saves/svc/svc_c0_0.st")
Y1, ACT, ANIM = 0x0930, 0x0968, 0x0C7E

BASE = "ngp_ss2sp=enabled,ngp_svcsp_engine=enabled"


def run(sc, opts=BASE):
    env = dict(os.environ); env["NGP_OPTS"] = opts
    t = tempfile.mkdtemp(); r = os.path.join(t, "r.ngc"); shutil.copy(ROM, r)
    io.open(os.path.join(t, "s.txt"), "w", encoding="utf-8", newline="\n").write(
        "\n".join(sc) + "\n")
    subprocess.run([RUN, CORE, r, os.path.join(t, "s.txt"), os.path.join(t, "g")],
                   capture_output=True, text=True, env=env)
    p = os.path.join(t, "gt.csv")
    if not os.path.exists(p):
        return None
    return [l.split(",") for l in io.open(p).read().strip().splitlines()][1:]


def main():
    lead = int(sys.argv[1]) if len(sys.argv) > 1 else 30
    sc = ["!load %s" % ST, "!w t %X,%X,%X" % (Y1, ACT, ANIM),
          "%d -" % lead, "4 U", "80 -", "!w off"]
    rows = run(sc)
    if not rows:
        print("★ 못 쟀다 (csv 없음)"); return 1
    t0 = None
    ys = []
    for r in rows:
        f, y, a = int(r[0]), int(r[2]), int(r[3])
        ys.append((f, y, a))
    # 뜨는 순간 = Y 가 128 을 처음 벗어나는 프레임
    air = [(f, y, a) for f, y, a in ys if y != 128]
    if not air:
        print("★ 안 떴다 — 이 세이브에서 점프가 안 나간다(대조군 실패)"); return 1
    t0 = air[0][0]
    tland = air[-1][0] + 1
    apex = min(air, key=lambda t: t[1])
    print("점프 프레임 지도 (Y: 지상 128, 위로 갈수록 작다)")
    print("  뜨는 순간   f%-5d (상대 0)" % t0)
    print("  정점        f%-5d (상대 %d)   Y=%d" % (apex[0], apex[0] - t0, apex[1]))
    print("  착지        f%-5d (상대 %d)" % (tland, tland - t0))
    print("  체공        %d프레임" % (tland - t0))
    print("  ★ 정점은 착지 −%d 프레임" % (tland - apex[0]))
    print()
    print("  Y 시계열(상대프레임:Y) —")
    line = []
    for f, y, a in air:
        line.append("%d:%d" % (f - t0, y))
    for i in range(0, len(line), 12):
        print("    " + " ".join(line[i:i + 12]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
