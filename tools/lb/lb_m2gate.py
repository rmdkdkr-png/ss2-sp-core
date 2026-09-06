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
WANT = 112
TAIL = 144
BASIC = 96          # 서서 베기(약). 이게 나오면 «커맨드가 안 먹고 평타만» 나간 것이다

# ★★ act 112 는 «질풍»만의 것이 아니다 — **평타를 8프레임 이상 쥐어도 112** 다(강베기).
#    갈리는 것은 **뒤따르는 144**:
#        질풍   112 → 144 → 4
#        강베기 112 →        4
#    그러니 「112 가 나왔다」로 판정하면 안 된다. **쌍으로 본다.**
#    지금 기본값(버튼 4)에서는 강베기가 안 나오지만, 버튼 프레임을 건드리는 순간
#    이 잣대는 조용히 거짓이 된다. 계획서가 「슬롯끼리 지문이 겹치면 경고」라 적어 둔 그것이다.


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


# 뛰어넘어 자리를 바꾸는 대본 — 반전이 1(왼쪽 봄)이 된다.
# 실측: f142 에 반전이 0→1 로 바뀌고, 되넘으면 f322 에 0 으로 «돌아온다».
CROSS = ["60 R", "30 U R", "90 -"]


def one(idle, opts, tail=70, cross=False):
    """세이브를 불러 idle 프레임 쉰 뒤 트리거를 한 프레임 누른다."""
    sc = ["!load %s" % ST, "!w t %X" % ACT, "%d -" % idle]
    if cross:
        sc += CROSS
    sc += ["1 R1", "%d -" % tail, "!w off"]
    return run(sc, opts)


def verdict(vals):
    """무엇이 나갔나. 「안 나갔다」와 「평타가 나갔다」를 가른다.

    ★ **못 잰 것을 「무반응」이라 부르지 마라.** 한 번 그렇게 읽고 엔진을 의심했다.
      원인은 csv 이름을 틀린 것이었다 — 재는 쪽 흠이었다."""
    if not vals:
        return "못잼"
    s = set(vals)
    if WANT in s:
        # 112 뒤에 144 가 «따라와야» 필살기다
        i = vals.index(WANT)
        if TAIL in vals[i:]:
            return "기술"
        return "강베기"          # 커맨드가 아니라 «오래 쥔 평타»가 나갔다
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

    # ★ 넘어간 뒤 — 앞이 뒤집힌 자리에서도 같은 기술이 나가야 한다.
    #   여기서 깨지면 반전을 잘못 잡은 것이다(KOF 가 배포까지 낸 그 사고).
    for ph in (0, 1):
        hits, other = 0, {}
        for i in range(N):
            v = verdict(one(20 + 2 * i + ph, ON, cross=True))
            if v == "기술":
                hits += 1
            else:
                other[v] = other.get(v, 0) + 1
                if v == "못잼": UNMEASURED.append(1)
        total["x%d" % ph] = hits
        extra = ("  (" + " ".join("%s×%d" % kv for kv in sorted(other.items())) + ")") if other else ""
        print("위상 %d · 넘어간 뒤: %2d/%d%s" % (ph, hits, N, extra))

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
    # ★ 간격을 «기술이 다 끝나고도 남게» 잡는다.
    #   전에는 70프레임이었는데, 링으로 발동이 빨라지자 기술(112 62프레임 + 144)이
    #   창을 넘겨 **다음 창이 앞 기술의 꼬리를 세고 있었다.** 그때는 「112 가 보이면 기술」이라
    #   느슨하게 재서 5/5 로 «통과»했다 — 지문을 쌍으로 바꾸자마자 드러났다.
    #   느슨한 잣대는 이렇게 실패를 통과로 바꾼다.
    GAP = 120
    sc = ["!load %s" % ST, "!w t %X" % ACT, "30 -"]
    REP = 5
    for _ in range(REP):
        sc += ["1 R1", "%d -" % GAP]
    sc += ["!w off"]
    vals = run(sc, ON)
    unmeasured = len(UNMEASURED)
    # 트리거마다 창을 잘라 본다
    if vals is None:
        UNMEASURED.append(1); vals = []
    seq = []
    for k in range(REP):
        a = 30 + k * (GAP + 1)
        seq.append(verdict(vals[a:a + GAP + 1]))
    rep_ok = sum(1 for v in seq if v == "기술")
    print("연속 발동(세이브 안 불러옴) %d/%d : %s" % (rep_ok, REP, " ".join(seq)))

    print()
    if unmeasured:
        print("★ 못 잰 시행이 %d 번 있다 — 그 상태의 판정은 «모른다»다." % unmeasured)
    # 관문은 «19/20», 곧 95%. N 을 줄여 돌릴 때도 같은 잣대가 되게 비율로 잰다.
    need = lambda h: h * 20 >= 19 * N
    ok = (need(total[0]) and need(total[1])
          and need(total["x0"]) and need(total["x1"]) and ctrl == 0
          and rep_ok == REP and unmeasured == 0)
    print("판정: %s" % ("PASS" if ok else "★FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
