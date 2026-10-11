#!/usr/bin/env python3
"""motion_fake.py — 모션 캐시(코어 패치 97, ss2_motion.bin) 가짜 파일 생성기. 그리는 길(찾기·풀기·틀 맞춤·4배 조각)을 눌러 보는 용도이지
사이 그림의 품질과는 무관하다. 진짜 캐시는 PocketCore 쪽 tools/motion_cache.py(RIFE 흐름)가 만든다.

사용: motion_fake.py <FGROM_MOTION_LOG 파일> <출력 ss2_motion.bin> <identity|cut|empty>
  identity = 모든 t 에서 A 를 그대로(이동 0) → 출력이 캐시 없을 때와 바이트까지 같아야 한다(조각 안이 원래 A 와 같게 그려지는지)
  cut      = ¼ 는 A, ½·¾ 는 B(이동 0) → 바뀜 2 프레임 전에 B 가 보인다(틀·시간 계산 확인용, 흩어짐이 없어야)
  empty    = 바뀜 1개(sa=sb=0, 절대 안 맞음)만 든 파일 → «캐시는 열렸지만 아무 바뀜도 없음» (못 찾은 바뀜 기록용)

로그 줄: p sa sb | A ox oy w h depth first | B ox oy w h depth first   (fgrom FGROM_MOTION_LOG)

파일 형식(코어 ss2fg_motion_open/mc_get 이 읽는 대로):
  머리 16: "SS2M" ver=2 _ nt=3 t=64 128 192 n(u32 LE) _ _
  색인 n×24: sa(u64) sb(u64) off(u32) len(u32) — (sa, sb) 오름차순
  항목: rawlen(u32) + zlib(raw). raw = cx cy(int8) w h(u8) bdx bdy(int8) aw ah bw bh(u8), 그 뒤 t 마다
        n(u16 LE, 칸 수) · 칸 비트맵 ceil(w*h/8) · A 고르기 비트맵 ceil(n/8) · 이동 n×(dx,dy int8, 앞 칸과의 차 누적)
  틀(cx,cy,w,h)은 A 포즈 왼쪽 위 기준. B 는 bdx,bdy 만큼 옮겨 있다(B.ox-A.ox, B.oy-A.oy).
"""
import struct, sys, zlib


def bits(flags):
    out = bytearray((len(flags) + 7) >> 3)
    for i, f in enumerate(flags):
        if f:
            out[i >> 3] |= 1 << (i & 7)
    return bytes(out)


def entry(A, B, mode):
    aox, aoy, aw, ah = A[0], A[1], A[2], A[3]
    box, boy, bw, bh = B[0], B[1], B[2], B[3]
    bdx, bdy = box - aox, boy - aoy
    cx, cy = min(0, bdx), min(0, bdy)
    w, h = max(aw, bdx + bw) - cx, max(ah, bdy + bh) - cy
    if not (-128 <= cx <= 127 and -128 <= cy <= 127 and -128 <= bdx <= 127 and -128 <= bdy <= 127 and 0 < w < 256 and 0 < h < 256):
        return None
    raw = bytearray(struct.pack('<bbBBbbBBBB', cx, cy, w, h, bdx, bdy, aw, ah, bw, bh))
    for ti in range(3):
        cells = [True] * (w * h)
        n = w * h
        use_a = [mode == 'identity' or (mode == 'cut' and ti == 0)] * n
        raw += struct.pack('<H', n) + bits(cells) + bits(use_a) + bytes(2 * n)   # 이동 0
    z = zlib.compress(bytes(raw), 9)
    return struct.pack('<I', len(raw)) + z


def main():
    if len(sys.argv) != 4 or sys.argv[3] not in ('identity', 'cut', 'empty'):
        sys.exit(__doc__)
    logp, outp, mode = sys.argv[1:4]
    ents = {}
    if mode != 'empty':
        for line in open(logp, encoding='utf-8'):
            parts = line.split('|')
            if len(parts) != 3:
                continue
            p, sa, sb = parts[0].split()
            A = [int(v) for v in parts[1].split()]
            B = [int(v) for v in parts[2].split()]
            key = (int(sa, 16), int(sb, 16))
            if key in ents or A[4] != B[4]:        # 깊이가 다르면 코어도 안 그린다
                continue
            e = entry(A, B, mode)
            if e:
                ents[key] = e
    if not ents:
        ents[(0, 0)] = struct.pack('<I', 10) + zlib.compress(bytes(10))
    keys = sorted(ents)
    off = 16 + 24 * len(keys)
    idx = bytearray()
    body = bytearray()
    for k in keys:
        e = ents[k]
        idx += struct.pack('<QQII', k[0], k[1], off + len(body), len(e))
        body += e
    with open(outp, 'wb') as f:
        f.write(b'SS2M' + bytes([2, 0, 3, 64, 128, 192]) + struct.pack('<I', len(keys)) + b'\0\0')
        f.write(idx)
        f.write(body)
    print('%s: %d 바뀜, %d 바이트 (%s)' % (outp, len(keys), 16 + len(idx) + len(body), mode))


if __name__ == '__main__':
    main()
