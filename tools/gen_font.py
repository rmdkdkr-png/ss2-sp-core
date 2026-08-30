#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ss2comm_font11.h / ss2comm_font.h 를 갈무리 BDF 에서 다시 뜬다.

   원래 있던 tools/gen_font.js 가 저장소에서 사라져 글꼴을 채울 수단이 없었다.
   그 사이 소스에 새 글자가 들어와도 아무도 못 채웠고, 실제로 화면에서 낱자가 비어 나왔다 —
   「114식 황물기」가 「114식 ⎵물기」, 「(Kouryuken)」이 「(ouryuken)」.
   기술 이름은 메뉴에 뜨는 글자다. 유저가 읽는 것이므로 비면 안 된다.

   ── 어떻게 맞다고 확신하는가
   BDF 에서 뜬 결과가 **지금 헤더에 든 글자와 한 픽셀도 안 틀리는지** 먼저 대조한다.
   11px 908자 · 8px 909자가 전부 일치할 때만 진행하고, 하나라도 어긋나면 멈춘다.
   음수 x 픽셀을 버리는 것까지 원본 생성기와 같게 맞췄다 — 그래야 「#」「j」가 일치한다.

   ── 무엇을 넣는가
   화면에 나갈 수 있는 소스의 **문자열 리터럴**에 쓰인 글자 전부.
   main() 이 있는 호스트 도구(reftest·simu·glyphshot …)는 화면에 안 나오므로 뺀다.
   기존 글자는 절대 빼지 않는다 — 훑기가 놓친 경로가 있어도 화면이 깨지지 않게.

   ── 글꼴 파일
   ~/ss2/font/ 에 둔다(SS2FONT_BDF_DIR 로 바꿀 수 있다). 저장소에는 넣지 않는다 —
   OFL 은 임베드·재배포를 허용하나 이 저장소는 글꼴 파일 자체를 재배포하지 않는다.
   받는 곳: https://github.com/quiple/galmuri/releases  (SIL Open Font License 1.1)
   11px 좁은꼴에 없는 한자는 Galmuri11(보통꼴)에서 뜬다 — 예: 「사치요 도리 (小夜千鳥)」.

   쓰기: python3 tools/gen_font.py            # 헤더 갱신
         python3 tools/gen_font.py --check    # 대조만, 쓰지 않음
"""
import io, os, re, sys, glob

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
FD = os.environ.get('SS2FONT_BDF_DIR', os.path.expanduser('~/ss2/font'))
ROOTS = [os.path.expanduser('~/ss2/repo/ss2-sp-core'), os.path.expanduser('~/ss2/repo/ss2-main')]

# (헤더, 주 BDF, 보조 BDF, 행수, 맨윗줄 y, x=0 비트자리, 값 자릿수, 개수 매크로, 폭 기록)
FONTS = [
    ('ss2comm_font11.h', 'Galmuri11-Condensed.bdf', 'Galmuri11.bdf', 13, 10, 11, 4, 'SS2FONT11_N', True),
    ('ss2comm_font.h',   'Galmuri7.bdf',            None,             8,  6,  7, 2, 'SS2FONT_N',   False),
]

def parse_bdf(path):
    """{코드포인트: (폭, {(x, 베이스라인 기준 y)})}"""
    out = {}; cp = dw = bbx = None; bits = None; reading = False
    for line in io.open(path, encoding='latin-1'):
        t = line.split()
        if not t: continue
        if   t[0] == 'ENCODING': cp = int(t[1])
        elif t[0] == 'DWIDTH':   dw = int(t[1])
        elif t[0] == 'BBX':      bbx = tuple(int(x) for x in t[1:5])
        elif t[0] == 'BITMAP':   bits = []; reading = True
        elif t[0] == 'ENDCHAR':
            reading = False
            if cp is not None and cp >= 0 and bbx:
                bw, bh, xo, yo = bbx; px = set()
                for j, hx in enumerate(bits):
                    v = int(hx, 16); n = len(hx) * 4
                    for k in range(bw):
                        if (v >> (n - 1 - k)) & 1: px.add((xo + k, yo + bh - 1 - j))
                out[cp] = (dw, px)
            cp = dw = bbx = None; bits = None
        elif reading: bits.append(t[0])
    return out

def render(px, nrow, top, xsh):
    """원본 생성기와 같게 — x<0 픽셀은 버린다(앞 글자 자리를 침범하므로)."""
    rows = [0] * nrow
    for (x, y) in px:
        if x < 0: continue
        r, b = top - y, xsh - x
        if 0 <= r < nrow and 0 <= b < 16: rows[r] |= 1 << b
    return tuple(rows)

def read_header(path, nrow, has_w):
    s = io.open(path, encoding='utf-8').read(); d = {}
    if has_w:
        for c, w, b in re.findall(r'\{0x([0-9A-Fa-f]{4}),\s*(\d+),\s*\{([^}]*)\}\s*\}', s):
            d[int(c, 16)] = (int(w), tuple(int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]+)', b)))
    else:
        for c, b in re.findall(r'\{0x([0-9A-Fa-f]{4}),\s*\{([^}]*)\}\s*\}', s):
            r = tuple(int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]+)', b))
            if len(r) == nrow: d[int(c, 16)] = (None, r)
    return d

def unescape(s):
    """C 이스케이프를 푼다. \\n 을 안 풀면 역슬래시와 n 이 글자로 잡힌다."""
    s = re.sub(r'\\[nrtv0abf]', '', s)
    return s.replace('\\"', '"').replace("\\'", "'").replace('\\\\', '\\')

def scan():
    """화면에 나갈 수 있는 소스의 문자열 리터럴에 쓰인 글자."""
    need = set(); files = []
    for root in ROOTS:
        for sub in ('src', 'build'):
            for p in sorted(glob.glob(os.path.join(root, sub, '*.c')) +
                            glob.glob(os.path.join(root, sub, '*.h'))):
                if os.path.basename(p).startswith('ss2comm_font'): continue
                t = io.open(p, encoding='utf-8', errors='replace').read()
                if re.search(r'^\s*int\s+main\s*\(', t, re.M): continue   # 호스트 도구
                files.append(p)
                for lit in re.findall(r'"((?:[^"\\]|\\.)*)"', t):
                    need |= {c for c in unescape(lit) if ord(c) >= 0x20}
    return need, files

def emit(path, glyphs, hexw, nmacro, has_w):
    """글리프 배열 구간만 갈아끼운다 — 주석·typedef·#endif 는 원문 그대로."""
    lines = io.open(path, encoding='utf-8').read().split('\n')
    beg = next(i for i, l in enumerate(lines) if re.match(r'\s*\{0x[0-9A-Fa-f]{4},', l))
    end = next(i for i in range(beg, len(lines)) if lines[i].startswith('};'))
    body = []
    for cp in sorted(glyphs):
        w, rows = glyphs[cp]
        v = ','.join('0x%0*X' % (hexw, r) for r in rows)
        body.append('  {0x%04X, %d,{%s}},' % (cp, w, v) if has_w else '  {0x%04X,{%s}},' % (cp, v))
    out = '\n'.join(lines[:beg] + body + lines[end:])
    out = re.sub(r'(#define\s+%s\s+)\d+' % nmacro, r'\g<1>%d' % len(glyphs), out)
    io.open(path, 'w', encoding='utf-8', newline='\n').write(out)

def main():
    check_only = '--check' in sys.argv
    need, files = scan()
    print('훑은 파일 %d개 · 문자열에 쓰인 글자 %d종 (한글 %d종)' % (
        len(files), len(need), sum(1 for c in need if 0xAC00 <= ord(c) <= 0xD7A3)))

    for hname, mainb, subb, nrow, top, xsh, hexw, nmacro, has_w in FONTS:
        prim = parse_bdf(os.path.join(FD, mainb))
        secd = parse_bdf(os.path.join(FD, subb)) if subb else {}
        cur = read_header(os.path.join(ROOTS[0], 'src', hname), nrow, has_w)

        bad = [chr(c) for c in cur if c in prim and
               (render(prim[c][1], nrow, top, xsh) != cur[c][1] or
                (has_w and cur[c][0] != prim[c][0]))]
        print('\n[%s] 기존 %d자 대조 → 불일치 %d %s' % (hname, len(cur), len(bad), ''.join(bad[:10])))
        if bad: raise SystemExit('  기존 글자를 재현 못 한다. 글꼴 판이 어긋났다 — 멈춘다.')

        glyphs = {}; missing = []; fromsub = []
        for c in sorted(set(cur) | {ord(x) for x in need}):
            src = prim if c in prim else (secd if c in secd else None)
            if src is None:
                if c in cur: glyphs[c] = cur[c]
                else: missing.append(chr(c))
                continue
            if src is secd and c not in cur: fromsub.append(chr(c))
            glyphs[c] = (src[c][0] if has_w else None, render(src[c][1], nrow, top, xsh))
        added = sorted(set(glyphs) - set(cur))
        print('  더할 글자 %d종: %s' % (len(added), ''.join(chr(c) for c in added)))
        if fromsub: print('  보조 글꼴(%s)에서 뜬 것: %s' % (subb, ''.join(fromsub)))
        if missing: print('  ** 어느 BDF 에도 없어 못 넣음: %s' % ''.join(missing))

        if check_only: continue
        for root in ROOTS:
            for sub in ('src', 'build'):
                p = os.path.join(root, sub, hname)
                if os.path.exists(p):
                    emit(p, glyphs, hexw, nmacro, has_w); print('  썼다: %s' % p)

if __name__ == '__main__':
    main()
