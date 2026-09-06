# -*- coding: utf-8 -*-
"""월화 M0 ① — 부팅부터 메뉴까지 훑어 찍는다.

무엇을 찾나: **트레이닝 모드로 가는 길**. 게임 문자열에 TRAINING 이 있는 것은 확인했으니
(dump/strings_out.tsv 30·31·32행) 화면 어디서 고르는지만 찾으면 된다.

⚠ 대본에서 `!` 로 시작하는 줄은 주석이 아니다 — 첫 낱말을 태그로 먹는다.
⚠ NGP A = 레트로 B · NGP B = 레트로 A (하네스 문자는 레트로패드 기준).
"""
import io, os, shutil, subprocess, sys, tempfile

RUN = "/mnt/c/Claude/KOF R2 한글/tools/ngprun"
CORE = "/home/dudu/m1/m31.so"
ROM = os.path.expanduser("~/ss2/rom/lastblade.ngc")
OUT = os.path.expanduser("~/ss2/tmp/lb/boot")
E = dict(os.environ)
E["NGP_OPTS"] = "ngp_ss2sp=enabled"      # 이게 꺼지면 폴드가 통째로 죽는다


def run(script, out, tag="g"):
    os.makedirs(out, exist_ok=True)
    t = tempfile.mkdtemp(); r = os.path.join(t, "r.ngc"); shutil.copy(ROM, r)
    io.open(os.path.join(t, "s.txt"), "w", encoding="utf-8", newline="\n").write("\n".join(script) + "\n")
    p = subprocess.run([RUN, CORE, r, os.path.join(t, "s.txt"), os.path.join(t, tag)],
                       capture_output=True, text=True, env=E)
    n = 0
    for f in sorted(os.listdir(t)):
        if f.startswith(tag):
            shutil.copy(os.path.join(t, f), "%s/%s" % (out, f[len(tag):].lstrip("_"))); n += 1
    return p.stdout.strip().splitlines()[-3:], n


if __name__ == "__main__":
    step = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 20
    sc = []
    for i in range(n):
        sc += ["%d -" % step, "!b%02d" % i]
    tail, k = run(sc, OUT)
    print("\n".join(tail)); print("파일 %d개 → %s (간격 %d, %d장)" % (k, OUT, step, n))
