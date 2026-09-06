# -*- coding: utf-8 -*-
"""링 주입이 실제로 «먹는가», 그리고 지연이 얼마나 줄었나.

재는 것: 트리거를 누른 프레임부터 act 가 112(질풍)가 되는 프레임까지.
  · 링 끔  = 원래 18프레임 대본
  · 링 켬  = D·DF 를 박고 F+버튼만 진짜로 누른다 (F 프레임 수를 쓸어 본다)

⚠ 「빠르다」만 보면 안 된다. **발동률이 같이 100% 여야** 뜻이 있다.
   빨라졌는데 가끔 안 나가면 손해다.
"""
import io, os, shutil, subprocess, sys, tempfile

RUN = "/mnt/c/Claude/KOF R2 한글/tools/ngprun"
CORE = os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM = os.path.expanduser("~/ss2/rom/pristine/Last Blade, The (UE) [!].ngc")
ST = os.path.expanduser("~/ss2/saves/lb/lb_train.st")
ACT, WANT, BASIC = 0x0370, 112, 96
ON = "ngp_ss2sp=enabled,ngp_lbsp_engine=enabled"


def run(idle, env_extra, tail=80, cross=False):
    env = dict(os.environ)
    env["NGP_OPTS"] = ON
    env.update(env_extra)
    sc = ["!load %s" % ST, "!w t %X" % ACT, "%d -" % idle]
    if cross:
        sc += ["60 R", "30 U R", "90 -"]
    sc += ["1 R1", "%d -" % tail, "!w off"]
    t = tempfile.mkdtemp(); r = os.path.join(t, "r.ngc"); shutil.copy(ROM, r)
    io.open(os.path.join(t, "s.txt"), "w", encoding="utf-8", newline="\n").write(
        "\n".join(sc) + "\n")
    subprocess.run([RUN, CORE, r, os.path.join(t, "s.txt"), os.path.join(t, "g")],
                   capture_output=True, text=True, env=env)
    p = os.path.join(t, "gt.csv")
    if not os.path.exists(p):
        return None
    rows = [l.split(",") for l in io.open(p).read().strip().splitlines()][1:]
    # 트리거 프레임 = 마지막 «패드 0 이 아닌» 구간의 시작… 이 아니라
    # 대본상 idle 다음 프레임이다. csv 의 f 는 1부터라 idle+1.
    trig_f = idle + (180 if cross else 0) + 1
    for r in rows:
        if int(r[2]) == WANT:
            return int(r[0]) - trig_f
    for r in rows:
        if int(r[2]) == BASIC:
            return -1        # 평타가 나갔다
    return -2                # 아무것도


def sweep(label, env_extra, n=8, cross=False):
    lat, miss, basic = [], 0, 0
    for i in range(n):
        d = run(20 + 2 * i, env_extra, cross=cross)
        if d is None:
            miss += 1
        elif d == -1:
            basic += 1
        elif d == -2:
            miss += 1
        else:
            lat.append(d)
    ok = len(lat)
    avg = (sum(lat) / float(ok)) if ok else 0
    print("%-34s 발동 %d/%d  지연 %s  %s"
          % (label, ok, n,
             ("최소%d 평균%.1f 최대%d" % (min(lat), avg, max(lat))) if ok else "—",
             ("평타×%d " % basic if basic else "") + ("무반응×%d" % miss if miss else "")))
    return ok, (min(lat) if lat else None)


if __name__ == "__main__":
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 8
    print("링 끔이 기준선이다 — 그보다 빨라지고 발동률이 안 떨어져야 이득이다.\n")
    sweep("링 끔 (원래 18프레임 대본)", {"LBSP_RING": "0"}, n)
    for f in (6, 4, 2, 0):
        sweep("링 켬 · F %d프레임" % f, {"LBSP_RING": "1", "LBSP_RING_F": str(f)}, n)
    print()
    sweep("링 켬 · F 4 · 넘어간 뒤", {"LBSP_RING": "1", "LBSP_RING_F": "4"}, n, cross=True)
