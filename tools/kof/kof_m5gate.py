#!/usr/bin/env python3
"""M5 관문 — **14명 전부**에서 슬롯 7개가 되풀이 발동하는가.

쿄 하나로 맞춘 기술표(236/623/214/421/236K/초필)를 **그대로** 다른 13명에게 건다.
캐릭터마다 그 커맨드에 붙은 기술이 다르므로, 여기서 묻는 것은
「같은 기술이 나오나」가 아니라 **「누르면 같은 것이 되풀이 나오나」**다.

슬롯별 판정 (kof_m3gate.py 와 같은 규율)
  * 최빈 지문에 몇 번 맞나 — 위상 2종 각각 ≥18/20
  * 지문이 비면 **무반응** (그 캐릭터에 그 커맨드 기술이 없다는 뜻)
  * 엔진을 끄고 같은 지문이 나오면 **무효지표**

⚠ 루갈(14)은 PLAYER SELECT 에서 못 고른다 — 보스다. 그래서 15명이 아니라 **14명**이다.

사용: kof_m5gate.py <코어.so> [시행수]
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
SAVE = os.path.expanduser('~/ss2/saves/kof')

NAME = {0: '쿄', 1: '테리', 2: '료', 3: '마이', 4: '레오나', 5: '야시로', 6: '셰르미',
        7: '크리스', 8: '아테나', 9: '유리', 10: '이오리', 11: '신고', 12: '카스미',
        13: '사이수'}
SLOTS = [('N', ''), ('F', 'R'), ('B', 'L'), ('D', 'D'),
         ('DF', 'D R'), ('DB', 'D L'), ('AIR', None)]


def run(core, cid, on, n, tmp):
    st = '%s/kof_c%02d.st' % (SAVE, cid)
    L = []
    for ph in (0, 1):
        for sname, hold in SLOTS:
            for i in range(n):
                L += ['!load %s' % st, '1',
                      '!w %s_%d_%02d 0D3C' % (sname, ph, i), str(20 + ph + 2 * i)]
                if hold is None:
                    L += ['2 U', '18', '2 R1']
                else:
                    L += ['2 %s R1' % hold if hold else '2 R1']
                L += ['130', '!w off']
    scr = os.path.join(tmp, 'g%d_%d.txt' % (cid, on))
    io.open(scr, 'w', encoding='utf-8', newline='\n').write('\n'.join(L) + '\n')
    env = dict(os.environ)
    env['NGP_OPTS'] = 'ngp_ss2sp=enabled'
    if on:
        env['KOFSP_ON'] = '1'
    else:
        env.pop('KOFSP_ON', None)
    pref = os.path.join(tmp, 'c%d_%d_' % (cid, on))
    subprocess.run([RUN, core, ROM, scr, pref], capture_output=True, text=True, env=env)

    res = {}
    for sname, _ in SLOTS:
        for ph in (0, 1):
            sigs = []
            for i in range(n):
                p = '%s%s_%d_%02d.csv' % (pref, sname, ph, i)
                if not os.path.exists(p):
                    sigs.append(()); continue
                d = list(csv.reader(open(p)))[1:]
                s, prev = [], None
                for r in d:
                    if r[2] != prev:
                        s.append(r[2]); prev = r[2]
                base = s[0] if s else '?'
                sigs.append(tuple(v for v in s[1:] if v != base))
            res[(sname, ph)] = sigs
    return res


def main():
    core = sys.argv[1]
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 20
    tmp = tempfile.mkdtemp()
    print('코어 %s · 14명 × 슬롯 7 × 시행 %d × 위상 2\n' % (core, n))
    print('%-7s %s   %s' % ('캐릭터', ' '.join('%-5s' % s for s, _ in SLOTS), '통과'))
    total_ok = 0
    rows = []
    for cid in sorted(NAME):
        on = run(core, cid, True, n, tmp)
        off = run(core, cid, False, n, tmp)
        cells, ok_n, seen = [], 0, {}
        for sname, _ in SLOTS:
            c0 = Counter(on[(sname, 0)]); c1 = Counter(on[(sname, 1)])
            t0, n0 = c0.most_common(1)[0]
            t1, n1 = c1.most_common(1)[0]
            offbad = sum(1 for s in off[(sname, 0)] + off[(sname, 1)] if t0 and s == t0)
            if not t0:
                cells.append('무반응')
            elif offbad:
                cells.append('★끔')
            elif n0 >= n - 2 and n1 >= n - 2:
                dup = seen.get(t0)
                seen.setdefault(t0, sname)
                cells.append('%d/%d%s' % (min(n0, n1), n, '=' + dup if dup else ''))
                ok_n += 1
            else:
                cells.append('%d/%d★' % (min(n0, n1), n))
        rows.append((cid, cells, ok_n))
        total_ok += ok_n
        print('%-7s %s   %d/7' % (NAME[cid], ' '.join('%-5s' % c for c in cells), ok_n))
    print('\n슬롯 통과 합계 %d/%d' % (total_ok, len(NAME) * len(SLOTS)))
    five = sum(1 for _, _, k in rows if k >= 5)
    print('기술 5개 이상 서는 캐릭터 %d/%d' % (five, len(NAME)))
    print('판정: %s' % ('PASS' if five == len(NAME) else '★부분'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
