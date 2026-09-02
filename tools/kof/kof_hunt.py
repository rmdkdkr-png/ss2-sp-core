#!/usr/bin/env python3
"""램 오프셋 사냥기 — 조건별 램을 떠서 **잡음을 뺀 뒤** 갈리는 바이트만 남긴다.

방법론(thinkbox methodology.md):
  * 같은 조건을 **두 번** 돌려 저절로 흔들리는 바이트를 먼저 제외한다.
    이 코어는 호스트 시각을 읽어서(rtc.c) 게임에 따라 램이 매 실행 흔들린다.
    그걸 안 빼면 잡음을 「신호」로 읽는다.
  * 남은 바이트 중 **조건마다 값이 갈리는 것**이 후보다.
  * 후보는 그 자체로 확정이 아니다 — 승격은 「독립 시나리오 2 + 위상 2종 + 교차 증인」.

조건 파일(TSV): `<이름>\\t<입력줄을 ; 로 이은 것>`
    서기<TAB>90
    앞걷기<TAB>40 R
    뒤걷기<TAB>40 L
빈 줄과 `#` 로 시작하는 줄은 무시한다(대본이 아니라 이 파일의 주석이다).

사용: kof_hunt.py <상태.st> <조건.tsv> [--settle 20] [--phase 0|1|both]
"""
import io
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
RUN = os.path.join(HERE, 'ngprun')
CORE = os.path.join(HERE, 'core.so')
ROM = os.path.expanduser('~/ss2/rom/kofr2.ngc')


def load_conds(path):
    out = []
    for ln in io.open(path, encoding='utf-8'):
        ln = ln.rstrip('\n')
        if not ln.strip() or ln.lstrip().startswith('#'):
            continue
        name, _, body = ln.partition('\t')
        if not body:
            sys.exit('조건 줄에 탭이 없다: %r' % ln)
        out.append((name.strip(), [s.strip() for s in body.split(';') if s.strip()]))
    return out


def run(state, conds, settle, pref):
    lines = []
    for name, body in conds:
        lines += ['!load %s' % state, '1', '%d' % settle]
        lines += body
        lines += ['!%s' % name]
    scr = pref + '.txt'
    io.open(scr, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
    subprocess.run([RUN, CORE, ROM, scr, pref + '_'],
                   capture_output=True, text=True)
    return {n: open('%s__%s.ram' % (pref, n), 'rb').read() for n, _ in conds}


def hunt(state, conds, settle, label):
    tmp = tempfile.mkdtemp()
    a = run(state, conds, settle, os.path.join(tmp, 'a'))
    b = run(state, conds, settle, os.path.join(tmp, 'b'))

    noise = set()
    for n, _ in conds:
        noise |= {i for i in range(len(a[n])) if a[n][i] != b[n][i]}

    names = [n for n, _ in conds]
    cand = []
    for i in range(len(a[names[0]])):
        if i in noise:
            continue
        vals = [a[n][i] for n in names]
        if len(set(vals)) > 1:
            cand.append((i, vals))

    print('[%s] 조건 %d개 · 잡음 %d바이트 · 갈리는 바이트 %d개'
          % (label, len(conds), len(noise), len(cand)))
    return cand, names


def main():
    state = sys.argv[1]
    conds = load_conds(sys.argv[2])
    settle = 20
    phase = 'both'
    if '--settle' in sys.argv:
        settle = int(sys.argv[sys.argv.index('--settle') + 1])
    if '--phase' in sys.argv:
        phase = sys.argv[sys.argv.index('--phase') + 1]

    if phase == 'both':
        c0, names = hunt(state, conds, settle, '위상0')
        c1, _ = hunt(state, conds, settle + 1, '위상1')
        m0, m1 = dict(c0), dict(c1)
        keep = [(i, m0[i], m1[i]) for i in sorted(set(m0) & set(m1))]
        print('\n★ **두 위상 모두**에서 갈리는 바이트 %d개 '
              '(위상 하나에만 나온 %d개는 버린다)'
              % (len(keep), len(set(m0) ^ set(m1))))
        # --expr 로 「이런 모양이어야 한다」를 걸 수 있다. v[i] = i번째 조건의 값.
        # 조건 길이를 같게 두고 술어로 거르는 것이 사냥의 핵심이다 —
        # 길이가 다르면 애니메이션이 통째로 달라져 수백 바이트가 다 걸린다.
        expr = None
        if '--expr' in sys.argv:
            expr = sys.argv[sys.argv.index('--expr') + 1]
            before = len(keep)
            keep = [k for k in keep
                    if eval(expr, {'__builtins__': {}}, {'v': k[1], 'w': k[2]})]
            print('   술어 %r 로 %d → %d 개' % (expr, before, len(keep)))

        print('\n%-8s %s' % ('오프셋', '  '.join('%-8s' % n for n in names)))
        for i, v0, v1 in keep[:80]:
            same = ' ' if v0 == v1 else '≠'
            print('%04X %s   %s' % (i, same, '  '.join('%-8s' % x for x in v0)))
        if len(keep) > 80:
            print('… 그 밖 %d개' % (len(keep) - 80))
        print('\n≠ 는 위상별로 값이 달랐다는 뜻 — 그런 바이트는 신원 잣대로 쓰지 마라.')
    else:
        c, names = hunt(state, conds, settle + int(phase), '위상%s' % phase)
        print('\n%-8s %s' % ('오프셋', '  '.join('%-8s' % n for n in names)))
        for i, v in c[:80]:
            print('%04X    %s' % (i, '  '.join('%-8s' % x for x in v)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
