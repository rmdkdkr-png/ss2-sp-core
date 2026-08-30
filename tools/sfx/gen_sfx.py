#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ElevenLabs 로 효과음을 뽑아 팩 재료를 만든다.

   ── 왜 PCM 으로 받나
   이 기계엔 ffmpeg 도 sox 도 없다. 그런데 API 가 output_format=pcm_44100 을 주므로
   **디코드가 아예 필요 없다** — 받은 것이 곧 우리 팩 규약(s16 mono 44.1k)이다.
   해설 팩처럼 22,050Hz 를 2배로 늘린 것이 아니라 처음부터 44.1k 라 고역이 산다.

   ── 왜 세기마다 여러 벌인가
   같은 소리가 반복되면 기계처럼 들린다. 엔진이 피치를 ±5% 흔들지만 그것만으론 모자라다.
   세기마다 여러 벌을 넣어 두면 엔진이 그중 하나를 골라 쓴다(sfx.hit.m / .m2 / .m3 …).

   ── 마스터링
   해설과 다르게 **loudnorm 이 아니라 피크 정규화**다. 타격음은 피크가 살아야 한다.
   앞뒤 무음은 잘라낸다 — 안 자르면 타격과 소리 사이가 뜬다.

   쓰기: python3 tools/sfx/gen_sfx.py <나갈곳> [벌수]
"""
import io, json, os, sys, time, urllib.request

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
HERE = os.path.dirname(os.path.abspath(__file__))
API  = 'https://api.elevenlabs.io/v1/sound-generation?output_format=pcm_44100'
RATE = 44100

def key():
    p = os.path.expanduser('~/.config/elevenlabs.key')
    return io.open(p, encoding='utf-8').read().strip()

def fnv1a(s):
    h = 2166136261
    for b in s.encode('utf-8'):
        h ^= b; h = (h * 16777619) & 0xFFFFFFFF
    return h

def rows():
    out = []
    for ln in io.open(HERE + '/sfx.tsv', encoding='utf-8'):
        ln = ln.rstrip('\n')
        if not ln or ln.startswith('#'): continue
        f = ln.split('\t')
        if len(f) >= 3: out.append((f[0], f[1], f[2]))
    return out

def gen(prompt, seconds, influence=0.5):
    body = json.dumps({'text': prompt,
                       'duration_seconds': seconds,
                       'prompt_influence': influence}).encode()
    req = urllib.request.Request(API, data=body, headers={
        'xi-api-key': key(), 'Content-Type': 'application/json'})
    with urllib.request.urlopen(req, timeout=180) as r:
        return r.read()

def master(pcm_bytes, cap_ms=250):
    """어택을 찾아 그 앞을 버리고, **세기별 길이로 잘라** 피크 -3dB 로 맞춘다.

       길이를 자르는 이유: 요청은 0.6초인데 API 가 1.2초를 준다(꼬리·잔향 포함).
       타격음이 1.2초면 연타에서 계속 겹쳐 뭉갠다. 원샷은 짧아야 한다.
       꼬리 판정을 무음 기준으로 하면 잔향까지 남으므로 아예 상한을 둔다."""
    import numpy as np
    a = np.frombuffer(pcm_bytes, dtype='<i2').astype(np.float32)
    if a.size == 0: return None
    amp = np.abs(a)
    pk = float(amp.max())
    if pk <= 0: return None
    # 창은 **피크를 기준**으로 잡는다. 처음엔 「피크의 25% 를 처음 넘는 곳」으로 잡았는데
    # 앞쪽 자잘한 소리에 걸려서 정작 타격 순간이 창 밖으로 밀려났다 —
    # 다듬기 전 크레스트 27.9 가 다듬은 뒤 9.1 로 떨어지던 것이 그 때문이었다.
    ipk = int(np.argmax(amp))
    s = max(0, ipk - int(RATE * 0.015))             # 어택 앞 15ms 를 남긴다
    e = min(a.size, s + int(RATE * cap_ms / 1000.0))
    a = a[s:e].copy()
    pk = float(np.abs(a).max())
    if pk > 0: a = a * (32767 * 0.708 / pk)         # -3dB (loudnorm 아님 — 피크가 살아야 한다)
    n = min(a.size, int(RATE * 0.025))              # 끝 25ms 페이드 — 잘린 자리의 클릭 제거
    if n > 1: a[-n:] *= np.linspace(1.0, 0.0, n)
    return np.clip(a, -32768, 32767).astype('<i2').tobytes()

if __name__ == '__main__':
    dst = os.path.expanduser(sys.argv[1] if len(sys.argv) > 1 else '~/ss2/sfxpack')
    nvar = int(sys.argv[2]) if len(sys.argv) > 2 else 3
    os.makedirs(dst, exist_ok=True)
    SEC = {'약': 0.5, '중': 0.6, '강': 1.0}
    # 세기별 길이 상한 — 원샷은 짧아야 겹쳐도 안 뭉갠다. 강타만 여운을 조금 준다.
    CAP = {'약': 130, '중': 210, '강': 330}
    man, spent = [], 0
    for base, tier, prompt in rows():
        for v in range(nvar):
            k = base if v == 0 else '%s%d' % (base, v + 1)
            fn = k.replace('.', '_') + '.raw'
            path = os.path.join(dst, fn)
            if os.path.exists(path):
                print('  %-14s 이미 있음' % k)
            else:
                raw = gen(prompt, SEC.get(tier, 0.6),
                          0.35 + 0.15 * v)          # 벌마다 프롬프트 반영도를 달리해 변화를 준다
                spent += 1
                m = master(raw, CAP.get(tier, 250))
                if not m: print('  %-14s ** 무음' % k); continue
                open(path, 'wb').write(m)
                print('  %-14s %5.0fms  (원본 %5.0fms)' % (
                    k, len(m)/2/RATE*1000, len(raw)/2/RATE*1000))
                time.sleep(0.4)
            man.append('%08x\t%s' % (fnv1a(k), fn))
    io.open(os.path.join(dst, 'manifest.tsv'), 'w', encoding='utf-8',
            newline='\n').write('\n'.join(man) + '\n')
    print('  manifest %d줄 · 이번에 생성 %d건 → %s' % (len(man), spent, dst))
