#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""thumbtool — 런처 썸네일 지정 + NGPC 타일 디버거.

같은 기술이 세 군데에 쓰인다: 런처 썸네일(지금), 기둥 아트(추후), 한패 타일 조사.
코어는 안 건드린다 — 세이브 스테이트에 그래픽 램이 통째로 들어가는 것(GFX 섹션,
이름표 실측 확인)을 파싱한다.

용법 (WSL):
  thumbtool.py all <롬…|폴더>          초기 세트 최속 — 각 롬 900프레임 타이틀 캡처
  thumbtool.py set <롬> <프레임>        그 프레임을 지정 썸으로
  thumbtool.py sheet <롬> [s e step]   후보 프레임들을 candidates/<id>/ 에 (기본 300~1800/150)
  thumbtool.py tiles <롬> [프레임]      ★타일 디버거 — <id>_tiles.png(타일 시트 512개),
                                       <id>_plane1/2.png(스크롤 평면 256x256 전체)
  thumbtool.py crop <PNG> <x y w h> <id>   평면/시트에서 오려 지정 썸으로

산출: ~/ss2/release/design/thumbs/<id>.png (mkdesign.py 가 색인·배포)
검증 규율: tiles/plane 산출물은 같은 프레임의 실제 화면(PPM)과 눈대조가 가능하다 —
평면 렌더가 타이틀 화면을 재현하면 포맷 해석이 맞는 것이다.

포맷 (build/mednafen/ngp/gfx.c 실측):
  타일 = 16B, 행 = u16 LE, 픽셀0(왼쪽)=비트14-15 … 픽셀7=비트0-1 (2bpp)
  ScrollVRAM 엔트리 u16: tile=b0-8, pal=b9-12, vflip=b14, hflip=b15. 평면2는 +0x800
  ColorPaletteRAM(512B, u16×256): scroll1 베이스 +0x80B, scroll2 +0x100B, 팔레트당 4색
  색 u16: R=b0-3, G=b4-7, B=b8-11 (MonoPlot 의 <<1/<<5/<<9 자리에서 역산)
"""
import os, struct, subprocess, sys, zlib

HOME = os.path.expanduser('~')
CORE = HOME + '/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so'
SVCRUN_SRC = HOME + '/ss2/repo/ss2-main/tools/svc/svcrun.c'
WORK = HOME + '/thumbwork'
OUT = HOME + '/ss2/release/design/thumbs'

TAGMAP = [  # 헤더 0x24, 앞부분 일치 — 구체적인 것 먼저 (SAMURAI2 > SAMURAI)
    ('SAMURAI2', 'ss2'), ('SNKvsCAPCOM1', 'svc'), ('SAMURAI', 'ss1'),
    ('LASTBLADE', 'lb'), ('KOF R2', 'kofr2'), ('RB_F_CONTACT', 'ffc'),
    ('METALSLUG1ST', 'ms1'), ('METALSLUG2ND', 'ms2'),
]

def id_of(rom):
    with open(rom, 'rb') as f:
        f.seek(0x24)
        tag = f.read(12).decode('ascii', 'replace')
    for t, gid in TAGMAP:
        if tag.startswith(t):
            return gid
    return os.path.splitext(os.path.basename(rom))[0]

def ensure_runner():
    os.makedirs(WORK, exist_ok=True)
    exe = os.path.join(WORK, 'svcrun')
    if not os.path.exists(exe):
        subprocess.check_call(['cc', '-O2', '-o', exe, SVCRUN_SRC, '-ldl'])
    return exe

def run_script(rom, script):
    exe = ensure_runner()
    sp = os.path.join(WORK, 'run.txt')
    open(sp, 'w').write(script)
    env = dict(os.environ, ngp_language='japanese',
               ngp_svcsp_engine='disabled', ngp_svcsp_band='disabled',
               ngp_ss2sp_sides='disabled', ngp_ss2sp_comm_draw='disabled',
               ngp_ss2sp_comm='disabled')
    subprocess.run([exe, CORE, os.path.abspath(rom), sp],
                   cwd=WORK, env=env, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)

def png_write(path, w, h, rgb):
    def ch(t, b):
        c = t + b
        return struct.pack('>I', len(b)) + c + struct.pack('>I', zlib.crc32(c))
    raw = b''.join(b'\x00' + rgb[y*w*3:(y+1)*w*3] for y in range(h))
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n'
                + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                + ch(b'IDAT', zlib.compress(raw, 6)) + ch(b'IEND', b''))

def ppm_to_png(src, dst):
    d = open(src, 'rb').read()
    parts = d.split(b'\n', 3)
    w, h = map(int, parts[1].split())
    png_write(dst, w, h, parts[3][:w*h*3])
    return w, h

def capture(rom, frame, dst):
    run_script(rom, '%d -\n!cap\n' % frame)
    ppm = os.path.join(WORK, 'svc_cap.ppm')
    if not os.path.exists(ppm):
        raise SystemExit('캡처 실패 — svc_cap.ppm 없음')
    w, h = ppm_to_png(ppm, dst)
    print('캡처:', dst, '%dx%d @f%d' % (w, h, frame))

# ── 스테이트 파서 — 이름표(TLV)로 변수 위치를 찾는다 ────────────────
def st_var(data, name, size):
    nb = name.encode()
    i = -1
    while True:
        i = data.find(nb, i + 1)
        if i < 0:
            raise SystemExit('스테이트에 %s 없음' % name)
        # 형식: [len:1][name][size:4 LE][data] — len·size 가 맞는 자리만 진짜
        if i >= 1 and data[i-1] == len(nb):
            sz = struct.unpack_from('<I', data, i + len(nb))[0]
            if sz == size:
                off = i + len(nb) + 4
                return data[off:off+size]

def save_state(rom, frame):
    stp = os.path.join(WORK, 'tt.st')
    if os.path.exists(stp):
        os.remove(stp)
    run_script(rom, '%d -\n!save tt.st\n5 -\n' % frame)
    if not os.path.exists(stp):
        raise SystemExit('스테이트 저장 실패')
    return open(stp, 'rb').read()

def pal_rgb(v):  # u16 → (r,g,b) 8비트
    r = (v & 0xF) * 17
    g = ((v >> 4) & 0xF) * 17
    b = ((v >> 8) & 0xF) * 17
    return r, g, b

def tile_pixels(cram, tile):
    px = []
    for row in range(8):
        d = struct.unpack_from('<H', cram, tile*16 + row*2)[0]
        px.append([(d >> (14 - 2*x)) & 3 for x in range(8)])
    return px

def render_tiles(st, out, plane=1):
    cram = st_var(st, 'CharacterRAM', 8192)
    cpal = st_var(st, 'ColorPaletteRAM', 0x200)
    base = 0x80 if plane == 1 else 0x100
    cols, rows = 32, 16                     # 512타일 = 32x16
    w, h = cols*8, rows*8
    buf = bytearray(w*h*3)
    for t in range(512):
        px = tile_pixels(cram, t)
        tx, ty = (t % cols)*8, (t // cols)*8
        for y in range(8):
            for x in range(8):
                v = struct.unpack_from('<H', cpal, base + (px[y][x])*2)[0]
                r, g, b = pal_rgb(v) if px[y][x] else (16, 16, 20)
                o = ((ty+y)*w + tx+x)*3
                buf[o:o+3] = bytes((r, g, b))
    png_write(out, w, h, bytes(buf))
    print('타일 시트:', out, '(512타일, 팔레트0 기준)')

def render_plane(st, out, plane):
    cram = st_var(st, 'CharacterRAM', 8192)
    svram = st_var(st, 'ScrollVRAM', 4096)
    cpal = st_var(st, 'ColorPaletteRAM', 0x200)
    base = 0x80 if plane == 1 else 0x100
    off = 0 if plane == 1 else 0x800
    w = h = 256
    buf = bytearray(w*h*3)
    for ty in range(32):
        for tx in range(32):
            e = struct.unpack_from('<H', svram, off + (ty*32 + tx)*2)[0]
            tile, pal = e & 0x1FF, (e >> 9) & 0xF
            vf, hf = e & 0x4000, e & 0x8000
            px = tile_pixels(cram, tile)
            for y in range(8):
                sy = 7-y if vf else y
                for x in range(8):
                    sx = 7-x if hf else x
                    c = px[sy][sx]
                    v = struct.unpack_from('<H', cpal, base + (pal*4 + c)*2)[0]
                    r, g, b = pal_rgb(v) if c else (16, 16, 20)
                    o = ((ty*8+y)*w + tx*8+x)*3
                    buf[o:o+3] = bytes((r, g, b))
    png_write(out, w, h, bytes(buf))
    print('평면%d:' % plane, out, '(256x256 전체 — 화면 밖 포함)')

def png_read(path):
    d = open(path, 'rb').read()
    assert d[:8] == b'\x89PNG\r\n\x1a\n'
    pos, w, h, idat = 8, 0, 0, b''
    while pos < len(d):
        ln = struct.unpack_from('>I', d, pos)[0]
        typ = d[pos+4:pos+8]
        if typ == b'IHDR':
            w, h = struct.unpack_from('>II', d, pos+8)[:2]
        elif typ == b'IDAT':
            idat += d[pos+8:pos+8+ln]
        pos += 12 + ln
    raw = zlib.decompress(idat)
    stride = w*3 + 1
    rows = []
    prev = bytearray(w*3)
    for y in range(h):
        ft = raw[y*stride]
        line = bytearray(raw[y*stride+1:(y+1)*stride])
        if ft == 1:
            for i in range(3, len(line)):
                line[i] = (line[i] + line[i-3]) & 0xFF
        elif ft == 2:
            for i in range(len(line)):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ft != 0:
            raise SystemExit('crop: 필터 %d 미지원 — thumbtool 산출 PNG 만 지원' % ft)
        rows.append(bytes(line))
        prev = line
    return w, h, b''.join(rows)

def main():
    a = sys.argv[1:]
    if not a:
        print(__doc__)
        return
    os.makedirs(OUT, exist_ok=True)
    mode = a[0]
    if mode == 'all':
        roms = []
        for p in a[1:]:
            if os.path.isdir(p):
                roms += [os.path.join(p, f) for f in sorted(os.listdir(p))
                         if f.lower().endswith(('.ngc', '.ngp'))]
            else:
                roms.append(p)
        for r in roms:
            gid = id_of(r)
            capture(r, 900, os.path.join(OUT, gid + '.png'))
    elif mode == 'set':
        capture(a[1], int(a[2]), os.path.join(OUT, id_of(a[1]) + '.png'))
    elif mode == 'sheet':
        s, e, step = (int(a[2]), int(a[3]), int(a[4])) if len(a) > 4 else (300, 1800, 150)
        gid = id_of(a[1])
        cd = os.path.join(OUT, 'candidates', gid)
        os.makedirs(cd, exist_ok=True)
        for f in range(s, e + 1, step):
            capture(a[1], f, os.path.join(cd, 'f%04d.png' % f))
        print('후보 폴더:', cd, '— 고른 프레임으로 `set` 하라')
    elif mode == 'tiles':
        frame = int(a[2]) if len(a) > 2 else 900
        gid = id_of(a[1])
        st = save_state(a[1], frame)
        render_tiles(st, os.path.join(OUT, gid + '_tiles.png'))
        render_plane(st, os.path.join(OUT, gid + '_plane1.png'), 1)
        render_plane(st, os.path.join(OUT, gid + '_plane2.png'), 2)
    elif mode == 'crop':
        w, h, rgb = png_read(a[1])
        x, y, cw, chh = map(int, a[2:6])
        gid = a[6]
        out = bytearray()
        for yy in range(y, y + chh):
            out += rgb[(yy*w + x)*3:(yy*w + x + cw)*3]
        png_write(os.path.join(OUT, gid + '.png'), cw, chh, bytes(out))
        print('오림:', os.path.join(OUT, gid + '.png'))
    else:
        print(__doc__)

if __name__ == '__main__':
    main()
