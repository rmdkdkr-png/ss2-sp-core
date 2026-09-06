# -*- coding: utf-8 -*-
"""SvC — act 의 «성격»부터 잡는다. 판정 지문 없이 잰 숫자는 숫자가 아니다.

앞선 시도에서 대조군(아무것도 안 누름)까지 「나갔다」로 읽혔다.
공중에서 누르면 어차피 «공중 공격»이 나가고, 착지하면 «착지 동작»이 뜬다.
착지 선입력이 하는 일은 그 뒤에 **지상 공격을 하나 더** 내는 것이다.
그러니 먼저 지상공격·공중공격·착지 를 각각 재서 갈라 놓는다.
"""
import io, os, shutil, subprocess, tempfile

RUN = os.path.expanduser("~/ss2/repo/ss2-sp-core/tools/kof/ngprun")
CORE = os.path.expanduser(
    "~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM = os.path.expanduser("~/ss2/rom/svc.ngc")
ST = os.path.expanduser("~/ss2/saves/svc/svc_c0_0.st")
Y1, ACT = 0x0930, 0x0968
ON = "ngp_ss2sp=enabled,ngp_svcsp_engine=enabled,ngp_svcsp_land=enabled"
OFF = "ngp_ss2sp=enabled,ngp_svcsp_engine=enabled,ngp_svcsp_land=disabled"


def run(sc, opts, win=None):
    env = dict(os.environ); env["NGP_OPTS"] = opts
    if win is not None:
        env["SVCSP_LAND_WIN"] = str(win)
    t = tempfile.mkdtemp(); r = os.path.join(t, "r.ngc"); shutil.copy(ROM, r)
    io.open(os.path.join(t, "s.txt"), "w", encoding="utf-8", newline="\n").write(
        "\n".join(sc) + "\n")
    subprocess.run([RUN, CORE, r, os.path.join(t, "s.txt"), os.path.join(t, "g")],
                   capture_output=True, text=True, env=env)
    p = os.path.join(t, "gt.csv")
    if not os.path.exists(p):
        return None
    return [l.split(",") for l in io.open(p).read().strip().splitlines()][1:]


def tl(rows, base=0):
    """(상대프레임, Y, act) 중 act 가 바뀌는 자리만."""
    out, prev = [], None
    for r in rows:
        f, y, a = int(r[0]), int(r[2]), int(r[3])
        if a != prev:
            out.append((f - base, y, a)); prev = a
    return out


def show(name, rows, base=0):
    if not rows:
        print("  %-26s 못 쟀다" % name); return
    q = tl(rows, base)
    print("  %-26s %s" % (name, " ".join("%d@f%d%s" % (a, f, "^" if y != 128 else "")
                                          for f, y, a in q[:12])))


LEAD = 30
print("act 성격 (^ = 공중)\n")
show("지상 Y (강펀치)", run(["!load %s" % ST, "!w t %X,%X" % (Y1, ACT),
                          "%d -" % LEAD, "3 Y", "80 -", "!w off"], OFF))
show("지상 B (약펀치)", run(["!load %s" % ST, "!w t %X,%X" % (Y1, ACT),
                          "%d -" % LEAD, "3 B", "80 -", "!w off"], OFF))
show("점프만 (안 누름)", run(["!load %s" % ST, "!w t %X,%X" % (Y1, ACT),
                          "%d -" % LEAD, "4 U", "100 -", "!w off"], OFF))
show("점프 + 정점 Y · 옵션끔", run(["!load %s" % ST, "!w t %X,%X" % (Y1, ACT),
                              "%d -" % LEAD, "4 U", "22 -", "3 Y", "100 -", "!w off"], OFF))
show("점프 + 정점 Y · 옵션켬32", run(["!load %s" % ST, "!w t %X,%X" % (Y1, ACT),
                               "%d -" % LEAD, "4 U", "22 -", "3 Y", "100 -", "!w off"], ON, 32))
show("점프 + 뜨자마자 Y · 켬32", run(["!load %s" % ST, "!w t %X,%X" % (Y1, ACT),
                               "%d -" % LEAD, "4 U", "6 -", "3 Y", "100 -", "!w off"], ON, 32))
