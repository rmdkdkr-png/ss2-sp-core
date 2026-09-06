# -*- coding: utf-8 -*-
"""m1_reg 가 하는 그대로 흉내 내어 파일을 직접 견준다.

해시 검사(optprobe)는 「두 코어가 같다」고 하고 m1_reg 는 「199바이트 다르다」고 한다.
둘 다 맞을 수는 없다. **도구를 믿지 말고 파일을 봐라.**
"""
import hashlib, os, shutil, subprocess, sys, tempfile

RUN = "/mnt/c/Claude/KOF R2 한글/tools/ngprun"
PRE = "/tmp/core_pre.so"
CUR = os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM = os.path.expanduser("~/ss2/rom/pristine/Metal Slug - 1st Mission (JUE).ngc")
SC = os.path.expanduser("~/ss2/repo/ss2-sp-core/tools/kof/m1_reg.txt")


def run(core, pref):
    """m1_reg.run 과 «글자 그대로» 같게."""
    env = dict(os.environ)
    env["NGP_OPTS"] = "ngp_ss2sp=enabled"
    out = subprocess.run([RUN, core, ROM, SC, pref],
                         capture_output=True, text=True, env=env).stdout
    return out


tmp = tempfile.mkdtemp()
# ⚠ 롬을 «복사하지 않고» 원본 경로를 그대로 준다 — m1_reg 도 그렇게 한다.
pa = os.path.join(tmp, "a") + "_"
pb = os.path.join(tmp, "b") + "_"
pc = os.path.join(tmp, "c") + "_"
run(PRE, pa)
run(CUR, pb)
run(PRE, pc)

for tag in ("r1", "r2", "end"):
    for ext in ("ram", "ppm"):
        fa = "%s_%s.%s" % (pa, tag, ext)
        fb = "%s_%s.%s" % (pb, tag, ext)
        fc = "%s_%s.%s" % (pc, tag, ext)
        if not (os.path.exists(fa) and os.path.exists(fb)):
            print("  %-4s %-4s 파일 없음" % (tag, ext)); continue
        A, B, C = (open(f, "rb").read() for f in (fa, fb, fc))
        d_ab = sum(1 for x, y in zip(A, B) if x != y)
        d_ac = sum(1 for x, y in zip(A, C) if x != y)
        print("  %-4s %-4s  기준↔기준 %-6d 기준↔새것 %-6d  md5 %s / %s"
              % (tag, ext, d_ac, d_ab,
                 hashlib.md5(A).hexdigest()[:8], hashlib.md5(B).hexdigest()[:8]))
print("tmp:", tmp)
