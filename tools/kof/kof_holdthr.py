#!/usr/bin/env python3
"""홀드 카운터 역검증 + **강약 문턱** 실측.

사냥으로 찾은 것은 「그렇게 보이는 바이트」일 뿐이다. 그것이 **게임이 강약을 정할 때
실제로 읽는 값**인지는 따로 물어야 한다. 방법: 카운터를 poke 로 고정해 놓고
**탭(2프레임)**을 넣는다. 탭인데 강이 나오면 그 바이트가 강약을 정하는 것이다.

`ngprun` 의 poke 는 core_run **앞**에 매 프레임 덮어쓰므로 값이 그 수에 고정된다.
poke 는 누적만 되고 안 지워져서 케이스마다 프로세스를 따로 띄운다.

기준(M0 실측, 쿄·스파링 정지무대): 탭 → act 57 · 홀드 → act 173.

사용: kof_holdthr.py [오프셋(16진, 기본 100C)] [버튼(기본 B)]
"""
import csv
import io
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
RUN = os.path.join(HERE, 'ngprun')
CORE = os.path.join(HERE, 'core.so')
ROM = os.path.expanduser('~/ss2/rom/kofr2.ngc')
ST = os.path.expanduser('~/ss2/saves/kof/kof_spar.st')
WEAK, STRONG = '57', '173'


def acts(csvpath):
    d = list(csv.reader(open(csvpath)))[1:]
    s, p = [], None
    for x in d:
        if x[2] != p:
            s.append(x[2]); p = x[2]
    return s


def one(tmp, off, btn, val, settle, tag):
    lines = ['!load %s' % ST, '1']
    if val is not None:
        lines.append('!poke %s=%d' % (off, val))
    lines += ['!w %s 0D3C,0D5Aw' % tag, str(settle), '2 %s' % btn, '110', '!w off']
    scr = os.path.join(tmp, tag + '.txt')
    io.open(scr, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
    pref = os.path.join(tmp, 'o_')
    subprocess.run([RUN, CORE, ROM, scr, pref], capture_output=True, text=True)
    return acts('%s%s.csv' % (pref, tag))


def main():
    off = sys.argv[1] if len(sys.argv) > 1 else '100C'
    btn = sys.argv[2] if len(sys.argv) > 2 else 'B'
    tmp = tempfile.mkdtemp()
    vals = [None, 0, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 30]

    print('오프셋 %s · 버튼 %s · 입력은 언제나 **탭(2프레임)**' % (off, btn))
    print('기준: 탭=%s(약) 홀드=%s(강)\n' % (WEAK, STRONG))
    print('%-8s %-8s %-8s %s' % ('poke값', '위상0', '위상1', 'act 흐름(위상0)'))
    for v in vals:
        r = {}
        for ph, settle in ((0, 20), (1, 21)):
            s = one(tmp, off, btn, v, settle, 'p%d_%s' % (ph, 'n' if v is None else v))
            core = [a for a in s if a not in ('213', '180', '143', '118')]
            r[ph] = ('강' if STRONG in core else ('약' if WEAK in core else
                     '딴것(%s)' % ','.join(core[:2]) if core else '무반응'))
            if ph == 0:
                flow = ' → '.join(s)
        print('%-8s %-8s %-8s %s' % ('없음' if v is None else v,
                                     r[0], r[1], flow[:44]))
    print('\n두 위상이 같은 답을 주는 가장 낮은 값이 문턱이다.')
    print('탭인데 강이 나오면 — 그 바이트가 강약을 정한다는 **직접 증거**다.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
