#!/usr/bin/env python3
"""점프강 → 착지 서서강이 이어지는가. **창의 폭을 프레임 단위로 잰다.**

사람이 말한 문제 그대로: 「점프강 서서강 이어지지 않는게 문제상황이고,
기본적으로 onset 이 늦는 문제가 있어서 링입력인지 뭐시기를 한거다」.

★ 판정은 **콤보 카운터를 믿지 않는다.**
  `0x10ED` 는 2타가 아직 안 맞았는데도 2 로 올라가는 것을 봤다(후속 입력 +3프레임).
  그래서 물리적 사실로 판정한다 — **1타와 2타 사이에 P2 가 평시(213)로 한 번도
  안 돌아왔는가.** 돌아왔으면 실전에선 막힌다. 콤보 카운터는 참고로만 찍는다.

  o = 콤보 성립 · x = 둘 다 맞지만 경직 풀린 뒤 · . = 2타 없음

두 팔을 나란히 돌린다:
  순정   — 그대로. 강은 홀드 20프레임.
  주입   — `0x100C`(펀치 홀드 카운터)를 문턱−1 로 눌러 놓고 **탭 2프레임**.
           본부가 SVC 에서 「착지엔 홀드 주입이 답이 아니었다」고 했다. KOF 는 어떤가.

무대: `kof_spar_dmg.st` 에서 **앞걸음 24프레임**(16이면 닿는다) 뒤 중립 점프.

사용: kof_combo.py <코어.so> [--probe]
"""
import csv
import io
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
RUN = os.path.join(HERE, 'ngprun')
ROM = os.path.expanduser('~/ss2/rom/kofr2.ngc')
ST = os.path.expanduser('~/ss2/saves/kof/kof_spar_dmg.st')

WATCH = '0D3C,0EC1,10ED,0E7C'          # 내act, P2체력, 콤보, P2act
POKE = '!poke 100C=2'                  # 문턱 3 − 1 (본부: 문턱을 건너뛴 값은 오동작)
ENV = dict(os.environ)
ENV['NGP_OPTS'] = 'ngp_ss2sp=enabled'
ENV['KOFSP_ON'] = '1'

# (이름, 공중타 입력 프레임(점프입력 기준), poke)
CFG = [('순정', 8, ''), ('순정', 10, ''),
       ('주입', 12, POKE), ('주입', 14, POKE)]
GS = list(range(2, 17))                # 공중타 놓은 뒤 후속까지의 간격
PHASES = (0, 1)


def build(tmp):
    lines, idx = [], {}
    for ph in PHASES:
        for ci, (nm, a, pk) in enumerate(CFG):
            for g in GS:
                i = len(idx)
                idx[(ph, ci, g)] = i
                hold = 2 if pk else 20          # 주입이면 탭으로도 강이 나온다
                lines += ['!load %s' % ST, str(1 + ph), '!unpoke']
                if pk:
                    lines.append(pk)
                lines += ['!w r%d %s' % (i, WATCH),
                          '24 R',               # 사거리 안으로
                          '2 U', str(a), '20 B',  # 점프 → 공중 강
                          str(g), '%d B' % hold,  # 착지 후 후속
                          '150', '!w off']
    return lines, idx


def verdict(tmp, i):
    p = os.path.join(tmp, 'r_r%d.csv' % i)
    d = list(csv.reader(open(p)))[1:]
    b = int(d[0][0])
    R = [(int(r[0]) - b, int(r[3]), int(r[4]), int(r[5])) for r in d]
    hits = [f for j, (f, hp, cb, p2) in enumerate(R) if j and hp < R[j - 1][1]]
    if len(hits) < 2:
        return '.'
    recovered = any(p2 == 213 for f, hp, cb, p2 in R if hits[0] < f < hits[1])
    return 'x' if recovered else 'o'


def main():
    core = sys.argv[1]
    tmp = tempfile.mkdtemp()
    lines, idx = build(tmp)
    scr = os.path.join(tmp, 's.txt')
    io.open(scr, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
    subprocess.run([RUN, core, ROM, scr, os.path.join(tmp, 'r_')],
                   capture_output=True, text=True, env=ENV)

    print('점프강 → 착지 서서강.  G = 공중타 놓은 뒤 후속까지의 간격\n')
    print('G=%s' % ''.join('%3d' % g for g in GS))
    widths = []
    for ci, (nm, a, pk) in enumerate(CFG):
        for ph in PHASES:
            s = ''.join('  ' + verdict(tmp, idx[(ph, ci, g)]) for g in GS)
            print('%s 공중+%-3d 위상%d%s' % (nm, a, ph, s))
        w = [g for g in GS
             if verdict(tmp, idx[(0, ci, g)]) == 'o'
             and verdict(tmp, idx[(1, ci, g)]) == 'o']
        widths.append(len(w))
        print('    → 두 위상 모두 성립하는 G: %s  (폭 %d프레임)\n'
              % (w if w else '없음', len(w)))
    print('o = 콤보 성립 · x = 둘 다 맞지만 경직 풀린 뒤 · . = 2타 없음')
    print('\n창이 3프레임이면 사람 손으로는 못 맞춘다 — 그게 「이어지지 않는다」의 정체다.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
