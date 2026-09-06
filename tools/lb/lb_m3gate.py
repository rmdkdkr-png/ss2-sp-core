# -*- coding: utf-8 -*-
"""M3 ③ 관문 — 슬롯 일곱.

관문(계획서):
  · 슬롯마다 **최빈 지문**에 위상 2종 각각 ≥18/20
  · 지문이 **무반응이 아님**
  · **슬롯끼리 지문이 겹치면 경고**
  · **엔진 끔이면 전부 무반응**

★ 지문은 «쌍»으로 잡는다 — act 148 이 참격(148→28)과 폭풍(148→140) 둘에 걸린다.
  하나만 보면 검사기가 통과를 남발한다(오늘 실제로 그랬다).
★ 빈 슬롯(뒤아래·공중)은 **아무것도 안 나가는 것이 정답**이다. 그렇게 검사한다.
"""
import collections, io, os, shutil, subprocess, sys, tempfile

RUN = os.path.expanduser("~/ss2/repo/ss2-sp-core/tools/kof/ngprun")
CORE = os.environ.get("LB_CORE", os.path.expanduser(
    "~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so"))
ROM = os.path.expanduser("~/ss2/rom/pristine/Last Blade, The (UE) [!].ngc")
ST = os.path.expanduser("~/ss2/saves/lb/lb_train.st")
ACT = 0x0370
REST = {4, 12, 20, 24}

ON = "ngp_ss2sp=enabled,ngp_lbsp_engine=enabled"
OFF = "ngp_ss2sp=enabled,ngp_lbsp_engine=disabled"

# 슬롯 → (방향 대본 문자, 기대 지문, 이름)
#   ⚠ P1 이 왼쪽을 보는 트레이닝 무대 기준이라 앞 = R.
SLOTS = [
    ("N",   "",     "112→144", "236+A 질풍"),
    ("F",   "R",    "116→48",  "623+A 풍아"),
    ("B",   "L",    "148→28",  "214+A 참격"),
    ("D",   "D",    "168",     "214+B 동풍"),
    ("DF",  "D R",  "148→140", "41236+B 폭풍"),
    ("DB",  "D L",  "(무반응)", "— 없음(정답)"),
    ("AIR", None,   "(건너뜀)", "— 미측정(OFF_H1)"),
]


def run(dirs, idle, opts, tail=110):
    env = dict(os.environ); env["NGP_OPTS"] = opts
    press = ("1 R1 " + dirs).strip() if dirs else "1 R1"
    hold = ("6 " + dirs) if dirs else "6 -"
    sc = ["!load %s" % ST, "!w t %X" % ACT, "%d -" % idle,
          hold,                   # 방향을 먼저 잡는다 — 슬롯은 «잡은 방향»으로 고른다
          press, "%d -" % tail, "!w off"]
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
        f = int(r2[0]); v = int(r2[2])
        if f < idle: prev = v; continue
        if v != prev: seq.append(v); prev = v
    core = [v for v in seq if v not in REST]
    if not core:
        return "(무반응)"
    return "→".join(str(v) for v in core[:2])


def main():
    N = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    print("코어 %s\n" % CORE)
    ok = True
    fps = {}
    for name, dirs, want, label in SLOTS:
        if dirs is None:
            print("  %-4s %-16s 건너뜀 — %s" % (name, label, want)); continue
        line = []
        for ph in (0, 1):
            c = collections.Counter()
            for i in range(N):
                v = run(dirs, 20 + 2 * i + ph, ON)
                c[v if v is not None else "(못잼)"] += 1
            top, cnt = c.most_common(1)[0]
            line.append((top, cnt))
        # 판정
        good = all(t == want and n * 20 >= 18 * N for t, n in line)
        if not good: ok = False
        fps.setdefault(line[0][0], []).append(name)
        print("  %-4s %-16s 위상0 %-10s %2d/%d   위상1 %-10s %2d/%d  %s"
              % (name, label, line[0][0], line[0][1], N,
                 line[1][0], line[1][1], N, "" if good else "★"))
    # 대조군 — 엔진을 끄면 전부 무반응이어야 한다
    print()
    bad_ctrl = []
    for name, dirs, want, label in SLOTS:
        if dirs is None: continue
        v = run(dirs, 40, OFF)
        if v != "(무반응)": bad_ctrl.append("%s=%s" % (name, v))
    print("  엔진 끔 대조군: %s" % ("전부 무반응" if not bad_ctrl else "★ " + " ".join(bad_ctrl)))
    if bad_ctrl: ok = False
    # 지문 겹침
    dup = {k: v for k, v in fps.items() if len(v) > 1 and k != "(무반응)"}
    if dup:
        ok = False
        for k, v in dup.items():
            print("  ★ 지문 겹침: %s ← %s" % (k, ", ".join(v)))
    print("\n판정: %s" % ("PASS" if ok else "★FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
