# -*- coding: utf-8 -*-
"""월화 하네스 얇은 겉옷 — 대본을 돌리고 결과를 한곳에 모은다.

⚠ 롬은 **순정 UE** 를 쓴다. `~/ss2/rom/lastblade.ngc` 는 한글판이라 대조군이 못 된다
   (sha ca8cf4cf…, 순정 UE 는 3195add4…).
⚠ 대본에서 `!` 로 시작하는 줄은 주석이 아니다 — 첫 낱말을 태그로 먹는다.
⚠ 버튼 문자는 레트로패드 기준: **NGP A = 'B' · NGP B = 'A'**. ST=OPTION.
⚠ `ngp_ss2sp` 가 꺼지면 update_input 블록이 통째로 안 돌아 폴드가 죽는다.
"""
import io, os, shutil, subprocess, tempfile

RUN = "/mnt/c/Claude/KOF R2 한글/tools/ngprun"
CORE = "/home/dudu/m1/m31.so"
PRISTINE = os.path.expanduser("~/ss2/rom/pristine/Last Blade, The (UE) [!].ngc")
KOREAN = os.path.expanduser("/mnt/c/Claude/월화 한글/rom/kr_v0.24.ngc")
E = dict(os.environ)
E["NGP_OPTS"] = "ngp_ss2sp=enabled"

TITLE = 4500          # 「PRESS A BUTTON」이 떠 있는 프레임 (실측, 300간격 훑기 b14~b17)


def run(script, out, rom=PRISTINE, tag="g", keep=("ppm", "ram", "csv", "st")):
    os.makedirs(out, exist_ok=True)
    t = tempfile.mkdtemp(); r = os.path.join(t, "r.ngc"); shutil.copy(rom, r)
    io.open(os.path.join(t, "s.txt"), "w", encoding="utf-8", newline="\n").write("\n".join(script) + "\n")
    p = subprocess.run([RUN, CORE, r, os.path.join(t, "s.txt"), os.path.join(t, tag)],
                       capture_output=True, text=True, env=E)
    n = 0
    for f in sorted(os.listdir(t)):
        if f.startswith(tag) and f.rsplit(".", 1)[-1] in keep:
            shutil.copy(os.path.join(t, f), "%s/%s" % (out, f[len(tag):].lstrip("_"))); n += 1
    return p, n
