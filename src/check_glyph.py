# -*- coding: utf-8 -*-
"""대사표의 모든 글자가 글꼴에 있는가. 하나라도 없으면 화면에 **공백**으로 나간다.
   대사를 고치고 tools/gen_font.js 를 안 돌리면 반드시 여기서 걸린다.
   실제로 그 사고가 두 번 났다 — 「리쿠도렛카」→「리쿠도⎵카」, 「나도 그랬다」→「나도 그⎵다」. """
import io, re, sys, os
D = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.abspath(__file__))
font = set()
for fn in ("ss2comm_font11.h", "ss2comm_font.h"):
    p = os.path.join(D, fn)
    if not os.path.exists(p): continue
    got = set(int(m, 16) for m in re.findall(r"\{0x([0-9A-Fa-f]{4}),", io.open(p, encoding="utf-8").read()))
    font = got if not font else (font & got)      # 두 글꼴 **모두** 있어야 안전하다
need = {}
for fn in ("ss2comm_lines.h", "ss2comm.c", "ss2comm.h", "ss2comm_duo.h",
           "ss2sp_moves.h", "ss2sp.c", "ss2sp.h"):
    p = os.path.join(D, fn)
    if not os.path.exists(p): continue
    t = io.open(p, encoding="utf-8").read()
    for s in re.findall(r'"((?:[^"\\]|\\.)*)"', t):
        for ch in s:
            if ord(ch) > 0x7F: need.setdefault(ord(ch), s)
miss = sorted(c for c in need if c not in font)
if miss:
    print("FAIL 글꼴에 없는 글자 %d개 — 화면에 공백으로 나간다" % len(miss))
    for c in miss[:20]:
        print("      %s (U+%04X)  ← \"%s\"" % (chr(c), c, need[c][:40]))
    print("      고치는 법: node tools/gen_font.js <이 디렉터리> <BDF디렉터리>")
    sys.exit(1)
print("PASS 글꼴 커버리지 (문자 %d종)" % len(need))
