# -*- coding: utf-8 -*-
"""화면에 나갈 모든 글자가 글꼴에 있는가. 하나라도 없으면 화면에 **공백**으로 나간다.
   글자를 고치고 tools/gen_font.py 를 안 돌리면 반드시 여기서 걸린다.
   실제로 그 사고가 네 번 났다 —
     「리쿠도렛카」→「리쿠도⎵카」, 「나도 그랬다」→「나도 그⎵다」,
     「114식 황물기」→「114식 ⎵물기」, 「(Kouryuken)」→「(ouryuken)」.

   뒤의 두 건은 이 검사기가 **못 걸렀다.** 구멍이 셋이었다:
     ① 볼 파일을 손으로 적어둬서 SVC 쪽(svcsp_moves.h 등)이 목록에 없었다.
        → 이제 디렉터리를 훑는다. 파일이 늘어도 저절로 따라간다.
     ② ord(ch) > 0x7F 라 로마자를 아예 안 봤다. 정작 F·I·J·K·M·Q·V·X·Z 가 없었다.
        → 공백 위(0x20 초과) 전부 본다.
     ③ 글꼴을 훑는 정규식이 글리프 항목과 **비트맵 행**을 구분 못 해,
        행 값이 우연히 글자 코드와 같으면 있는 것으로 쳤다.
        → 항목은 뒤에 '{' 가 오는 것으로 가려낸다."""
import io, re, sys, os, glob

D = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.abspath(__file__))

# ── 글꼴에 실제로 든 글자. 항목은 {0xCCCC, w,{...}} 또는 {0xCCCC,{...}} 꼴이라
#    코드 뒤에 (폭,) 이 오든 안 오든 반드시 '{' 가 따라온다. 비트맵 행은 안 그렇다.
ENTRY = re.compile(r"\{0x([0-9A-Fa-f]{4}),\s*(?:\d+,\s*)?\{")
font = None
for fn in ("ss2comm_font11.h", "ss2comm_font.h"):
    p = os.path.join(D, fn)
    if not os.path.exists(p): continue
    got = set(int(m, 16) for m in ENTRY.findall(io.open(p, encoding="utf-8").read()))
    font = got if font is None else (font & got)   # 두 글꼴 **모두** 있어야 안전하다
if not font:
    print("SKIP 글꼴 헤더를 못 찾음"); sys.exit(0)

def unescape(s):
    s = re.sub(r'\\[nrtv0abf]', '', s)
    return s.replace('\\"', '"').replace("\\'", "'").replace('\\\\', '\\')

# ── 화면에 나갈 수 있는 소스 전부. main() 이 있는 호스트 도구는 화면에 안 나오므로 뺀다.
need, nfile = {}, 0
for p in sorted(glob.glob(os.path.join(D, "*.c")) + glob.glob(os.path.join(D, "*.h"))):
    if os.path.basename(p).startswith("ss2comm_font"): continue
    t = io.open(p, encoding="utf-8", errors="replace").read()
    if re.search(r'^\s*int\s+main\s*\(', t, re.M): continue
    nfile += 1
    for s in re.findall(r'"((?:[^"\\]|\\.)*)"', t):
        for ch in unescape(s):
            if ord(ch) > 0x20: need.setdefault(ord(ch), (os.path.basename(p), s))

miss = sorted(c for c in need if c not in font)
if miss:
    print("FAIL 글꼴에 없는 글자 %d개 — 화면에 공백으로 나간다" % len(miss))
    for c in miss[:24]:
        f, s = need[c]
        print("      %s (U+%04X)  %-20s \"%s\"" % (chr(c), c, f, s[:36]))
    print("      고치는 법: python3 tools/gen_font.py")
    sys.exit(1)
print("PASS 글꼴 커버리지 — 파일 %d개 · 글자 %d종" % (nfile, len(need)))
