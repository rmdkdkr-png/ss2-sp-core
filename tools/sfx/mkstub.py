#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""엔진 검증용 **임시** 효과음을 합성한다. 진짜 소리는 AI 생성으로 따로 만든다.

   왜 임시음부터인가 — 좋은 소리를 구해 놔도 엔진이 연타에서 씹히면 소용이 없다.
   엔진의 위험(겹침·강탈·지연)을 먼저 걷어내고 소리를 얹는 순서가 맞다.
   또 합성음은 파형이 뚜렷해서 **캡처에서 온셋을 세기 좋다** — 검증에 유리하다.

   출력: <나갈곳>/*.raw (s16 mono 44.1k) + manifest.tsv
   팩으로 묶는 것은 tools/voice/mkpak.py 를 그대로 쓴다(포맷이 같다).
   쓰기: python3 tools/sfx/mkstub.py ~/ss2/sfxpack_stub
"""
import io, math, os, struct, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

RATE = 44100

def fnv1a(s):
    h = 2166136261
    for b in s.encode('utf-8'):
        h ^= b; h = (h * 16777619) & 0xFFFFFFFF
    return h

def noise(n, seed):
    """결정적 잡음 — 검증이 재현돼야 한다"""
    s = seed & 0xFFFFFFFF
    out = []
    for _ in range(n):
        s = (s * 1664525 + 1013904223) & 0xFFFFFFFF
        out.append(((s >> 16) & 0xFFFF) / 32768.0 - 1.0)
    return out

def lowpass(x, a):
    y = []; p = 0.0
    for v in x:
        p += a * (v - p); y.append(p)
    return y

def clip_bytes(samples):
    b = bytearray()
    for v in samples:
        i = int(max(-1.0, min(1.0, v)) * 30000)
        b += struct.pack('<h', i)
    return bytes(b)

def make(kind):
    """세기별로 길이·저역·감쇠를 다르게 — 캡처에서 눈으로도 구분되게"""
    if kind == 'l':   ms, lp, decay, thump = 60,  0.55, 45.0, 0.0
    elif kind == 'm': ms, lp, decay, thump = 110, 0.35, 26.0, 0.15
    else:             ms, lp, decay, thump = 190, 0.18, 14.0, 0.45
    n = int(RATE * ms / 1000)
    src = lowpass(noise(n, 0xC0FFEE + ord(kind)), lp)
    out = []
    for i in range(n):
        t = i / RATE
        env = math.exp(-decay * t)
        v = src[i] * env
        if thump:                      # 저역 쿵 — 강타에 무게를 준다
            v += thump * math.sin(2 * math.pi * 70 * t) * math.exp(-22 * t)
        out.append(v * 0.7)
    return clip_bytes(out)

if __name__ == '__main__':
    dst = os.path.expanduser(sys.argv[1] if len(sys.argv) > 1 else '~/ss2/sfxpack_stub')
    os.makedirs(dst, exist_ok=True)
    rows = []
    for kind in ('l', 'm', 'h'):
        key = 'sfx.hit.' + kind
        fn = key.replace('.', '_') + '.raw'
        data = make(kind)
        open(os.path.join(dst, fn), 'wb').write(data)
        rows.append('%08x\t%s' % (fnv1a(key), fn))
        print('  %-12s %6d샘플 %5.0fms  %s' % (key, len(data) // 2,
              len(data) / 2 / RATE * 1000, fn))
    io.open(os.path.join(dst, 'manifest.tsv'), 'w', encoding='utf-8',
            newline='\n').write('\n'.join(rows) + '\n')
    print('  manifest.tsv %d줄 → %s' % (len(rows), dst))
