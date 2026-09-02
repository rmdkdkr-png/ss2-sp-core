#!/usr/bin/env python3
"""체력 후보 중 **원본**을 가린다 — 표시 사본과 구별.

SVC 에서 「체력·게이지의 표시 사본은 poke 해도 매 프레임 복원된다」로 한 번 속았다.
줄어드는 것을 다 체력이라 부르면 안 된다. 그래서 **결과를 바꾸는지**로 묻는다:

    체력을 한 방보다 낮게 박아 놓고 때린다 → 라운드가 끝나면 그 바이트가 진짜다.

판정은 **화면**으로 한다 — KO 가 나면 연출이 들어가 화면이 크게 바뀐다.
대조군(poke 없음)과 픽셀이 같으면 그 바이트는 판정에 안 쓰이는 사본이다.

사용: kof_hp.py <오프셋16진…>
"""
import os
import subprocess
import sys
import tempfile
import io

HERE = os.path.dirname(os.path.abspath(__file__))
RUN = os.path.join(HERE, 'ngprun')
CORE = os.path.join(HERE, 'core.so')
ROM = os.path.expanduser('~/ss2/rom/kofr2.ngc')
ST = os.path.expanduser('~/ss2/saves/kof/kof_spar_dmg.st')


def one(tmp, off, tag):
    lines = ['!load %s' % ST, '1']
    if off:
        lines.append('!poke %s=5' % off)      # 강 한 방(7)보다 낮게
    lines += ['20', '140 R', '20 B', '150', '!%s' % tag]
    scr = os.path.join(tmp, tag + '.txt')
    io.open(scr, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
    pref = os.path.join(tmp, 'h_')
    subprocess.run([RUN, CORE, ROM, scr, pref], capture_output=True, text=True)
    return '%s_%s.ppm' % (pref, tag), '%s_%s.ram' % (pref, tag)


def main():
    offs = sys.argv[1:] or ['0D0D', '0D0E', '0EC1', '0EE3', '0EB8']
    tmp = tempfile.mkdtemp()
    base_p, base_r = one(tmp, None, 'base')
    b = open(base_p, 'rb').read()
    print('대조군(poke 없음) 화면 %d바이트\n' % len(b))
    print('%-8s %-12s %s' % ('오프셋', '화면차이', '판정'))
    for o in offs:
        p, r = one(tmp, o, 'x' + o)
        d = open(p, 'rb').read()
        diff = sum(1 for x, y in zip(b, d) if x != y)
        v = '★원본 후보 — 결과가 바뀐다' if diff > 2000 else (
            '조금 다름(%d)' % diff if diff else '사본 — 결과가 안 바뀐다')
        print('%-8s %-12d %s' % (o, diff, v))
    print('\n화면이 크게 바뀌면 KO 연출이 들어간 것 — 그 바이트가 판정에 쓰인다.')
    print('안 바뀌면 poke 가 먹히지 않았거나 표시 사본이다.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
