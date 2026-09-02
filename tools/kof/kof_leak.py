#!/usr/bin/env python3
"""누출 시험 — 트리거를 **한 번도 안 누르면** 엔진이 아무 짓도 안 하는가.

「필살기가 몇 번 나갔나」로 세지 않는다. 무작위 입력이 우연히 236 모션을 만들면
**게임이** 기술을 낸다 — 그건 엔진 누출이 아니다. 그 둘을 act 집합으로 가르려 하면
경계가 흐려진다.

대신 정확한 질문을 던진다: **같은 대본에서 엔진 켬과 끔의 출력이 다른가.**
트리거를 안 눌렀으니 달라질 이유가 없다. 한 프레임이라도 다르면 누출이다.

⚠ 대조군도 같이 본다 — 트리거를 **누르는** 대본에서는 반드시 **달라야** 한다.
  안 달라지면 이 시험은 아무것도 안 재고 있는 것이다.

사용: kof_leak.py <코어.so> [판수] [판당 프레임]
"""
import csv
import io
import os
import random
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
RUN = os.path.join(HERE, 'ngprun')
ROM = os.path.expanduser('~/ss2/rom/kofr2.ngc')
ST = os.path.expanduser('~/ss2/saves/kof/kof_spar_dmg.st')
BTN = ['-', 'U', 'D', 'L', 'R', 'D L', 'D R', 'B', 'A', 'Y', 'X', 'L1',
       'R B', 'L B', 'D B', 'R A', 'L A', 'D A']


def gen(rounds, frames, seed, with_trigger):
    rnd = random.Random(seed)
    L = []
    for k in range(rounds):
        L += ['!load %s' % ST, '1', '!w r%03d 0D3C,0EC1' % k, '20']
        n = 0
        while n < frames:
            d = rnd.choice((2, 3, 4, 6, 8, 12))
            b = rnd.choice(BTN)
            if with_trigger and rnd.random() < 0.05:
                b = (b + ' R1') if b != '-' else 'R1'
            L.append('%d %s' % (d, b) if b != '-' else str(d))
            n += d
        L.append('!w off')
    return '\n'.join(L) + '\n'


def run(core, script, on, tmp, tag):
    scr = os.path.join(tmp, tag + '.txt')
    io.open(scr, 'w', encoding='utf-8', newline='\n').write(script)
    env = dict(os.environ)
    env['NGP_OPTS'] = 'ngp_ss2sp=enabled'
    if on:
        env['KOFSP_ON'] = '1'
    else:
        env.pop('KOFSP_ON', None)
    pref = os.path.join(tmp, tag + '_')
    subprocess.run([RUN, core, ROM, scr, pref], capture_output=True, text=True, env=env)
    return pref


def compare(p1, p2, rounds):
    diff = 0
    for k in range(rounds):
        a = os.path.join(os.path.dirname(p1), os.path.basename(p1) + 'r%03d.csv' % k)
        b = os.path.join(os.path.dirname(p2), os.path.basename(p2) + 'r%03d.csv' % k)
        if not (os.path.exists(a) and os.path.exists(b)):
            continue
        if open(a).read() != open(b).read():
            diff += 1
    return diff


def main():
    core = sys.argv[1]
    rounds = int(sys.argv[2]) if len(sys.argv) > 2 else 200
    frames = int(sys.argv[3]) if len(sys.argv) > 3 else 300
    tmp = tempfile.mkdtemp()

    print('코어 %s · %d판 × %d프레임\n' % (core, rounds, frames))

    s0 = gen(rounds, frames, 20260903, False)      # 트리거 안 누름
    a = run(core, s0, True, tmp, 'on')
    b = run(core, s0, False, tmp, 'off')
    leak = compare(a, b, rounds)

    s1 = gen(rounds, frames, 20260903, True)       # 트리거 누름 — 대조군
    c = run(core, s1, True, tmp, 'ton')
    d = run(core, s1, False, tmp, 'toff')
    ctrl = compare(c, d, rounds)

    print('트리거 안 누름 : 켬≠끔 인 판 %d/%d   (0 이어야 통과)' % (leak, rounds))
    print('트리거 누름    : 켬≠끔 인 판 %d/%d   (커야 정상 — 이 시험이 살아 있다는 증거)'
          % (ctrl, rounds))
    ok = leak == 0 and ctrl > rounds * 0.5
    if ctrl <= rounds * 0.5:
        print('\n★무효지표 — 트리거를 눌러도 안 달라진다. 아무것도 재고 있지 않다.')
    print('\n판정: %s' % ('PASS — 누출 0' if ok else '★FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
