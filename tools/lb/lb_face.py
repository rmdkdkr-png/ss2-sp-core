# -*- coding: utf-8 -*-
"""M3 ① — 반전(좌우) 주소 사냥.

**이걸 틀리면 배포 사고가 난다.** KOF 는 0x0D4A 를 반전이라 잘못 잡았는데 그건
「필살기를 한 번 쓰면 서고 안 내려오는 플래그」였고, 그 탓에 배포된 엔진이
한 라운드 **두 번째 발동부터 커맨드를 좌우로 뒤집었다.**

그래서 겉모습으로 안 정한다. 승격 잣대:
  · 독립 시나리오 **2개** (오른쪽 봄 / 넘어가서 왼쪽 봄)
  · **위상 2종** (프레임 한 칸 밀어도 같은 값)
  · **교차 증인** (P2 쪽에서 반대 값이 나오나)

먼저 «넘어갔는지»를 화면으로 확인한다 — 안 넘어갔으면 시나리오가 하나뿐이라
무엇을 재든 뜻이 없다.
"""
import io, os, shutil, subprocess, sys, tempfile

RUN = "/mnt/c/Claude/KOF R2 한글/tools/ngprun"
CORE = os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM = os.path.expanduser("~/ss2/rom/pristine/Last Blade, The (UE) [!].ngc")
ST = os.path.expanduser("~/ss2/saves/lb/lb_train.st")
OUT = os.path.expanduser("~/ss2/tmp/lb/face")


def run(script, tag):
    env = dict(os.environ); env["NGP_OPTS"] = "ngp_ss2sp=enabled"
    t = tempfile.mkdtemp(); r = os.path.join(t, "r.ngc"); shutil.copy(ROM, r)
    io.open(os.path.join(t, "s.txt"), "w", encoding="utf-8", newline="\n").write(
        "\n".join(script) + "\n")
    subprocess.run([RUN, CORE, r, os.path.join(t, "s.txt"), os.path.join(t, "g")],
                   capture_output=True, text=True, env=env)
    os.makedirs(OUT, exist_ok=True)
    got = {}
    for f in sorted(os.listdir(t)):
        if not f.startswith("g"):
            continue
        ext = f.rsplit(".", 1)[-1]
        if ext in ("ram", "ppm"):
            d = os.path.join(OUT, "%s_%s" % (tag, f[1:].lstrip("_")))
            shutil.copy(os.path.join(t, f), d)
            got[ext] = d
    return got


# 시나리오 — 위상은 앞에 붙이는 여분 프레임으로 만든다
def sc_right(ph):
    return ["!load %s" % ST, "%d -" % (40 + ph), "!s"]


def sc_cross(ph):
    """앞으로 걸어가 붙은 뒤 **뛰어넘는다.** 넘어가면 P1 이 오른쪽에 서서 왼쪽을 본다."""
    return (["!load %s" % ST, "%d -" % (10 + ph)]
            + ["60 R"]              # 붙는다
            + ["30 U R"]            # 앞으로 뛴다
            + ["50 -"]              # 착지·정지
            + ["!s"])


def ram(p):
    return open(p, "rb").read()


def main():
    sh = {}
    for name, mk in (("right", sc_right), ("cross", sc_cross)):
        for ph in (0, 1):
            tag = "%s%d" % (name, ph)
            g = run(mk(ph), tag)
            sh[tag] = g
            print("%-8s ram=%s ppm=%s" % (tag, "ok" if "ram" in g else "없음",
                                          "ok" if "ppm" in g else "없음"))
    if not all("ram" in v for v in sh.values()):
        print("★ 덤프가 없다 — 여기서 멈춘다."); return 1

    R0, R1 = ram(sh["right0"]["ram"]), ram(sh["right1"]["ram"])
    C0, C1 = ram(sh["cross0"]["ram"]), ram(sh["cross1"]["ram"])
    n = min(len(R0), len(R1), len(C0), len(C1))

    # 위상 사이에 흔들리는 바이트는 «못 재는 것»이니 먼저 뺀다
    noisy = {i for i in range(n) if R0[i] != R1[i] or C0[i] != C1[i]}
    cand = [i for i in range(n) if i not in noisy and R0[i] != C0[i]]
    print()
    print("위상 잡음 %d바이트 · 시나리오 차이(잡음 뺀) **%d바이트**" % (len(noisy), len(cand)))

    # 반전다운 값: 두 시나리오에서 각각 «한 값»이고, 흔한 꼴(0/1, 0/0x80, 0/0xFF)
    print()
    print("주소     오른쪽봄 → 넘어간뒤   비고")
    shown = 0
    for i in cand:
        a, b = R0[i], C0[i]
        note = ""
        if {a, b} <= {0, 1}: note = "0/1"
        elif {a & 0x80, b & 0x80} == {0, 0x80}: note = "bit7 뒤집힘"
        elif {a, b} == {0, 0xFF}: note = "0/FF"
        if note:
            print("0x%04X   %3d → %3d          %s" % (i, a, b, note))
            shown += 1
        if shown > 40: break
    if not shown:
        print("(흔한 꼴이 하나도 없다 — 아래 전량을 봐라)")
        for i in cand[:40]:
            print("0x%04X   %3d → %3d" % (i, R0[i], C0[i]))
    print()
    print("화면: %s  /  %s" % (sh["right0"].get("ppm"), sh["cross0"].get("ppm")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
