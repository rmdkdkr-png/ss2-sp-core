# -*- coding: utf-8 -*-
"""시험 감사 — 「돌지 않는 시험은 시험이 아니다」.

`test_kofsp.c` 가 CPUExRAM 미정의로 **링크조차 안 되고 있었다.** 그 파일 안에는
「전부 통과」라는 글자가 있었지만 아무도 그 글자를 본 적이 없었다.
같은 것이 더 있는지 훑는다.

C 시험: 파일 머리 주석의 빌드 줄을 찾아 그대로 해 보고, 없으면 흔한 꼴로 시도한다.
파이썬: 문법(compile)과 «불러오기»까지만 본다 — 돌리면 에뮬을 태우니까.
"""
import io, os, py_compile, re, subprocess, sys, tempfile

R = os.path.expanduser("~/ss2/repo/ss2-sp-core")

C_TESTS = ["src/reftest.c", "src/test_flow.c",
           "tools/kof/test_kofsp.c", "tools/lb/test_lbsp.c"]
PY = ["src/test_core_sp.py", "src/test_sp_mode.py",
      "tools/kof/kof_m2gate.py", "tools/kof/kof_m3gate.py", "tools/kof/kof_m5gate.py",
      "tools/lb/lb_m2gate.py", "tools/svc/gate.py", "tools/kof/m1_reg.py"]

print("== C 시험: 정말 «빌드되나» ==")
for rel in C_TESTS:
    p = os.path.join(R, rel)
    if not os.path.exists(p):
        print("  %-26s 파일 없음" % rel); continue
    head = io.open(p, encoding="utf-8", errors="replace").read(3000)
    m = re.search(r"(cc|gcc)\s+[^\n]*%s[^\n]*" % re.escape(os.path.basename(rel)), head)
    d = os.path.dirname(p)
    out = tempfile.mktemp()
    if m:
        cmd = m.group(0).strip()
        cmd = re.sub(r"-o\s+\S+", "-o " + out, cmd)
        cmd = cmd.split("&&")[0].strip()
    else:
        cmd = "cc -O1 -I%s/src -o %s %s" % (R, out, os.path.basename(rel))
    r = subprocess.run(["bash", "-c", cmd], cwd=d, capture_output=True, text=True)
    if r.returncode == 0:
        print("  %-26s ✔ 빌드됨   (%s)" % (rel, cmd[:52]))
    else:
        err = (r.stderr or r.stdout).strip().splitlines()
        key = [l for l in err if "error" in l or "undefined" in l][:2]
        print("  %-26s ★ 안 됨    %s" % (rel, " / ".join(key)[:110] or err[-1][:110] if err else ""))

print()
print("== 파이썬: 문법이 서나 (돌리지는 않는다 — 에뮬을 태운다) ==")
for rel in PY:
    p = os.path.join(R, rel)
    if not os.path.exists(p):
        print("  %-26s 파일 없음" % rel); continue
    try:
        py_compile.compile(p, cfile=tempfile.mktemp(), doraise=True)
        print("  %-26s ✔" % rel)
    except Exception as e:
        print("  %-26s ★ %s" % (rel, str(e)[:100]))
