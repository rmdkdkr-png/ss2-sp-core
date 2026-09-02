#!/usr/bin/env python3
"""M3 관문 — 슬롯 7개가 각각 **같은 기술을 되풀이해서** 내는가.

기대 act 를 손으로 박지 않는다. 박으면 내가 틀렸을 때 「엔진이 틀렸다」로 읽힌다.
대신 **시행들의 최빈 지문**을 잡고 **그것에 몇 번 맞는지**를 센다 — 그게 발동률이다.
따로 두 조건을 건다:
  * 지문이 **무반응이 아니어야** 한다(뭔가 나가야 한다)
  * 슬롯끼리 지문이 **겹치면 경고** — 겹치면 슬롯이 실제로는 하나다

대조군: 엔진을 끄면 **전부 무반응**이어야 한다. 아니면 무효지표.

통과: 슬롯마다 위상 2종 각각 **≥18/20**.

사용: kof_m3gate.py <코어.so> [시행수]
"""
import csv
import io
import os
import subprocess
import sys
import tempfile
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
RUN = os.path.join(HERE, 'ngprun')
ROM = os.path.expanduser('~/ss2/rom/kofr2.ngc')
ST = os.path.expanduser('~/ss2/saves/kof/kof_spar_dmg.st')

# (이름, 트리거 직전에 잡고 있을 것, 앞선 준비입력)
SLOTS = [
    ('N',   '',      []),
    ('F',   'R',     []),
    ('B',   'L',     []),
    ('D',   'D',     []),
    ('DF',  'D R',   []),
    ('DB',  'D L',   []),
    ('AIR', '',      ['2 U', '18']),
]


def run(core, on, n):
    tmp = tempfile.mkdtemp()
    L = []
    for ph in (0, 1):
        for name, hold, pre in SLOTS:
            for i in range(n):
                L += ['!load %s' % ST, '1', '!w %s_%d_%02d 0D3C,0EC1' % (name, ph, i),
                      str(20 + ph + 2 * i)]
                L += pre
                L += ['2 %s R1' % hold if hold else '2 R1', '130', '!w off']
    scr = os.path.join(tmp, 'g.txt')
    io.open(scr, 'w', encoding='utf-8', newline='\n').write('\n'.join(L) + '\n')
    env = dict(os.environ)
    env['NGP_OPTS'] = 'ngp_ss2sp=enabled'
    if on:
        env['KOFSP_ON'] = '1'
    else:
        env.pop('KOFSP_ON', None)
    subprocess.run([RUN, core, ROM, scr, os.path.join(tmp, 'o_')],
                   capture_output=True, text=True, env=env)

    res = {}
    for name, _, _ in SLOTS:
        for ph in (0, 1):
            sigs = []
            for i in range(n):
                p = os.path.join(tmp, 'o_%s_%d_%02d.csv' % (name, ph, i))
                if not os.path.exists(p):
                    sigs.append(()); continue
                d = list(csv.reader(open(p)))[1:]
                s, prev = [], None
                for r in d:
                    if r[2] != prev:
                        s.append(r[2]); prev = r[2]
                base = s[0] if s else '?'
                sigs.append(tuple(v for v in s[1:] if v != base))
            res[(name, ph)] = sigs
    return res


def main():
    core = sys.argv[1]
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 20
    on = run(core, True, n)
    off = run(core, False, n)

    print('코어 %s · 슬롯 %d × 시행 %d × 위상 2\n' % (core, len(SLOTS), n))
    print('%-5s %-8s %-8s %-8s %s' % ('슬롯', '위상0', '위상1', '끔', '최빈 지문'))
    seen, allok, ctrlok = {}, True, True
    for name, _, _ in SLOTS:
        rates, sig = [], None
        for ph in (0, 1):
            c = Counter(on[(name, ph)])
            top, cnt = c.most_common(1)[0]
            if ph == 0:
                sig = top
            rates.append(cnt if top else 0)      # 빈 지문(무반응)은 0점
        # ★ 대조군은 「뭐라도 변했나」가 아니라 **「같은 기술이 나왔나」**를 묻는다.
        #   방향을 잡으면 엔진이 없어도 걷기 act(64/75)가 뜬다 — 그걸 발동으로 세면
        #   모든 방향 슬롯이 거짓 실패로 찍힌다(실제로 한 번 그렇게 나왔다).
        offcnt = sum(1 for s in off[(name, 0)] + off[(name, 1)] if sig and s == sig)
        warn = []
        if not sig:
            warn.append('★무반응')
        if sig and sig in seen:
            warn.append('★겹침:%s' % seen[sig])
        elif sig:
            seen[sig] = name
        if offcnt:
            warn.append('★끔에도나감(%d)' % offcnt); ctrlok = False
        ok = rates[0] >= n - 2 and rates[1] >= n - 2 and sig
        if not ok:
            allok = False
        print('%-5s %-8s %-8s %-8s %s %s'
              % (name, '%d/%d' % (rates[0], n), '%d/%d' % (rates[1], n),
                 '%d/%d' % (offcnt, 2 * n), '→'.join(sig)[:30] if sig else '-',
                 ' '.join(warn)))
    print()
    if not ctrlok:
        print('★무효지표 — 엔진을 꺼도 나간다.')
    print('판정: %s' % ('PASS — 슬롯 전부 위상 2종 ≥%d/%d, 대조군 0'
                        % (n - 2, n) if (allok and ctrlok) else '★FAIL'))
    return 0 if (allok and ctrlok) else 1


if __name__ == '__main__':
    sys.exit(main())
