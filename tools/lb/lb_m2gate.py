# -*- coding: utf-8 -*-
"""M2 관문 — 트리거 한 번에 236+A(질풍, act 112)가 나가는가.

관문(계획서):
  · 위상 2종 각각 **≥19/20**
  · **엔진 끔 대조군 0/n** — 꺼도 통과하는 지표는 아무것도 안 재고 있다는 뜻이다
  · **연속 발동** — 세이브를 «다시 안 불러오고» 한 상태에서 여러 번.
    KOF 는 M5(98/98)까지 전부 초록이었는데도 배포 사고가 났고, 원인이
    시행마다 세이브를 다시 부른 것이었다. 그 검사를 마지막이 아니라 여기서 시작한다.

⚠ 하네스 문자 R1 = 레트로패드 R = 우리 트리거.
⚠ NGP_OPTS 에 ngp_ss2sp=enabled 가 없으면 update_input 이 통째로 안 돈다 —
   그러면 「안 나간다」가 「엔진이 나쁘다」와 구별이 안 된다.
"""
import io, os, shutil, subprocess, sys, tempfile

RUN = "/mnt/c/Claude/KOF R2 한글/tools/ngprun"
CORE = os.environ.get(
    "LB_CORE",
    os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so"))
ROM = os.path.expanduser("~/ss2/rom/pristine/Last Blade, The (UE) [!].ngc")
ST = os.path.expanduser("~/ss2/saves/lb/lb_train.st")

ACT = 0x0370
ACT_REST = 4
WANT = 112          # 236+A — lb_moves.py 실측
BASIC = 96          # 서서 베기. 이게 나오면 «커맨드가 안 먹고 평타만» 나간 것이다


def run(script, opts):
    env = dict(os.environ)
    env["NGP_OPTS"] = opts
    t = tempfile.mkdtemp()
    r = os.path.join(t, "r.ngc")
    shutil.copy(ROM, r)
    io.open(os.path.join(t, "s.txt"), "w", encoding="utf-8", newline="\n").write(
        "\n".join(script) + "\n")
    subprocess.run([RUN, CORE, r, os.path.join(t, "s.txt"), os.path.join(t, "g")],
                   capture_output=True, text=True, env=env)
    p = os.path.join(t, "gt.csv")
    if not os.path.exists(p):
        return None          # ★ 빈 목록이 아니라 None — 「못 쟀다」와 「무반응」은 다른 말이다
    rows = [l.split(",") for l in io.open(p).read().strip().splitlines()][1:]
    return [int(r[2]) for r in rows]      # csv 는 frame,pad,<offset들> — 값은 r[2]부터


def one(idle, opts, tail=70):
    """세이브를 불러 idle 프레임 쉰 뒤 트리거를 한 프레임 누른다."""
    sc = ["!load %s" % ST, "!w t %X" % ACT,
          "%d -" % idle, "1 R1", "%d -" % tail, "!w off"]
    return run(sc, opts)


def verdict(vals):
    """무엇이 나갔나. 「안 나갔다」와 「평타가 나갔다」를 가른다.

    ★ **못 잰 것을 「무반응」이라 부르지 마라.** 한 번 그렇게 읽고 엔진을 의심했다.
      원인은 csv 이름을 틀린 것이었다 — 재는 쪽 흠이었다."""
    if not vals:
        return "못잼"
    s = set(vals)
    if WANT in s:
        return "기술"
    if BASIC in s:
        return "평타"
    if s - {ACT_REST}:
        return "다른것(%s)" % ",".join(str(v) for v in sorted(s - {ACT_REST}))[:24]
    return "무반응"


ON = "ngp_ss2sp=enabled,ngp_lbsp_engine=enabled"
OFF = "ngp_ss2sp=enabled,ngp_lbsp_engine=disabled"
N = int(sys.argv[1]) if len(sys.argv) > 1 else 20


UNMEASURED = []


def main():
    print("코어 %s" % CORE)
    print("롬   %s (순정 UE)" % os.path.basename(ROM))
    print()

    total = {}
    for ph in (0, 1):
        hits, other = 0, {}
        for i in range(N):
            idle = 20 + 2 * i + ph
            v = verdict(one(idle, ON))
            if v == "기술":
                hits += 1
            else:
                other[v] = other.get(v, 0) + 1
                if v == "못잼": UNMEASURED.append(1)
        total[ph] = hits
        extra = ("  (" + " ".join("%s×%d" % kv for kv in sorted(other.items())) + ")") if other else ""
        print("위상 %d · 엔진 켬 : %2d/%d%s" % (ph, hits, N, extra))

    # 대조군 — 꺼도 나가면 아무것도 안 재고 있는 것이다
    ctrl, cother = 0, {}
    for i in range(N):
        v = verdict(one(20 + 2 * i, OFF))
        if v == "기술":
            ctrl += 1
        else:
            cother[v] = cother.get(v, 0) + 1
            if v == "못잼": UNMEASURED.append(1)
    cextra = ("  (" + " ".join("%s×%d" % kv for kv in sorted(cother.items())) + ")") if cother else ""
    print("     엔진 끔 대조군: %2d/%d%s" % (ctrl, N, cextra))

    # 연속 발동 — 세이브를 «다시 안 불러온다»
    sc = ["!load %s" % ST, "!w t %X" % ACT, "30 -"]
    REP = 5
    for _ in range(REP):
        sc += ["1 R1", "70 -"]
    sc += ["!w off"]
    vals = run(sc, ON)
    unmeasured = len(UNMEASURED)
    # 트리거마다 창을 잘라 본다
    if vals is None:
        UNMEASURED.append(1); vals = []
    seq = []
    for k in range(REP):
        a = 30 + k * 71
        seq.append(verdict(vals[a:a + 71]))
    rep_ok = sum(1 for v in seq if v == "기술")
    print("연속 발동(세이브 안 불러옴) %d/%d : %s" % (rep_ok, REP, " ".join(seq)))

    print()
    if unmeasured:
        print("★ 못 잰 시행이 %d 번 있다 — 그 상태의 판정은 «모른다»다." % unmeasured)
    # 관문은 «19/20», 곧 95%. N 을 줄여 돌릴 때도 같은 잣대가 되게 비율로 잰다.
    need = lambda h: h * 20 >= 19 * N
    ok = (need(total[0]) and need(total[1]) and ctrl == 0
          and rep_ok == REP and unmeasured == 0)
    print("판정: %s" % ("PASS" if ok else "★FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
