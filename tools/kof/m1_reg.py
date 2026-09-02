#!/usr/bin/env python3
"""M1 회귀 — 게임별 출력이 kofsp 가지 도입 전후로 같은가.

★ 이 하네스는 그냥 두 번 돌리면 **결과가 흔들린다.** 코어가 호스트 시각을 읽기
  때문이다(`mednafen/ngp/rtc.c` 의 `time()`/`localtime()`). SVC 9바이트·SS2 3바이트가
  실행마다 달라진다. 그 상태로 「다르다」를 보고하면 **내 변경의 증거가 아니라
  측정기 잡음**이다 — 실제로 한 번 그렇게 오독할 뻔했다.

그래서 판정 전에 **측정기부터 잰다**:
    각 코어를 두 번 돌려 → 스스로 재현되지 않는 바이트 = 불안정
    비교는 **양쪽 모두에서 안정한 바이트**에 대해서만 한다.
불안정 바이트가 너무 많으면(기본 200) 판정을 포기한다 — 재고 있지 않다는 뜻이다.

사용: m1_reg.py <기준코어.so> <새코어.so> [대본]
"""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
RUN = os.path.join(HERE, 'ngprun')
HOME = os.path.expanduser('~')
KROM = os.path.join(os.path.dirname(HERE), 'rom')
TAGS = ('r1', 'r2', 'end')
NOISE_CAP = 200

GAMES = [
    ('svc',    HOME + '/ss2/rom/svc.ngc'),
    ('ss2',    HOME + '/ss2/rom/ss2.ngc'),
    ('mslug1', KROM + '/_mslug1.ngc'),
    ('mslug2', KROM + '/_mslug2.ngc'),
    ('ffury',  KROM + '/_fatalfury.ngc'),
    ('kofr2',  HOME + '/ss2/rom/kofr2.ngc'),
]


def run(core, rom, script, pref):
    env = dict(os.environ)
    # ★ 이걸 안 켜면 코어의 입력 분기 블록이 통째로 안 돈다 —
    #   「같다」가 「아무것도 안 쟀다」와 구별이 안 된다.
    env['NGP_OPTS'] = 'ngp_ss2sp=enabled'
    out = subprocess.run([RUN, core, rom, script, pref],
                         capture_output=True, text=True, env=env).stdout
    snd = [l for l in out.splitlines() if l.startswith('소리')]
    return snd[0] if snd else '(소리 줄 없음)'


def rd(pref, tag, ext):
    p = '%s_%s.%s' % (pref, tag, ext)
    return open(p, 'rb').read() if os.path.exists(p) else None


def unstable(p1, p2, tag, ext):
    a, b = rd(p1, tag, ext), rd(p2, tag, ext)
    if a is None or b is None or len(a) != len(b):
        return None
    return {i for i, (x, y) in enumerate(zip(a, b)) if x != y}


def main():
    base, new = sys.argv[1], sys.argv[2]
    script = sys.argv[3] if len(sys.argv) > 3 else os.path.join(HERE, 'm1_reg.txt')
    tmp = tempfile.mkdtemp()

    print('기준 %s' % base)
    print('새   %s' % new)
    print('대본 %s\n' % script)
    print('%-8s %-22s %-22s %s' % ('게임', '램(안정바이트 기준)', '화면', '소리'))

    # 「달라져야 정상」인 게임은 미리 선언한다 — 엔진이 졸업하면 그 게임은 달라진다.
    # 선언 안 한 게임이 달라지면 그게 회귀다.
    expect = set()
    if '--expect' in sys.argv:
        expect = set(sys.argv[sys.argv.index('--expect') + 1].split(','))

    allok = True
    unjudged = []
    for name, rom in GAMES:
        if not os.path.exists(rom):
            print('%-8s (롬 없음)' % name)
            continue
        p = {k: os.path.join(tmp, '%s_%s' % (name, k)) for k in ('a', 'b', 'c', 'd')}
        # ★ **엇갈려 돌린다** — a(기준) b(새것) c(기준) d(새것).
        #   잡음(a↔c, b↔d)이 비교(a↔b)와 **같은 시간 간격**을 건너야 뜻이 있다.
        #   나란히 돌리면(a,b 기준 → c,d 새것) 잡음은 인접 두 실행에서만 재지는데
        #   비교는 두 칸 떨어진 실행끼리 하게 되어, **시간에 따라 흔들리는 것을
        #   「변경 탓」으로 오독한다.** 실제로 SS2 를 그렇게 한 번 오독했다
        #   (같은 코어를 양쪽에 넣어도 같은 숫자가 나와서 들켰다).
        sa = run(base, rom, script, p['a'] + '_')
        sb = run(new, rom, script, p['b'] + '_')
        sc = run(base, rom, script, p['c'] + '_')
        run(new, rom, script, p['d'] + '_')

        ramline, scrline = [], []
        ok = True
        for tag in TAGS:
            for ext, acc in (('ram', ramline), ('ppm', scrline)):
                u1 = unstable(p['a'] + '_', p['c'] + '_', tag, ext)   # 기준 코어의 잡음
                u2 = unstable(p['b'] + '_', p['d'] + '_', tag, ext)   # 새 코어의 잡음
                if u1 is None or u2 is None:
                    acc.append('덤프없음'); ok = False; continue
                noise = u1 | u2
                if len(noise) > NOISE_CAP:
                    acc.append('판정불가(잡음%d)' % len(noise))
                    if name not in unjudged:
                        unjudged.append(name)
                    continue
                a, c = rd(p['a'] + '_', tag, ext), rd(p['b'] + '_', tag, ext)
                diff = [i for i in range(len(a))
                        if i not in noise and a[i] != c[i]]
                acc.append('%d' % len(diff) + ('' if not diff else '★'))
                if diff:
                    ok = False
        noisy = sum(len(unstable(p['a'] + '_', p['c'] + '_', t, 'ram') or ()) for t in TAGS)
        # 소리도 같은 규율 — 기준 코어가 스스로 재현 안 되면(sa≠sc) 판정하지 않는다.
        if sa != sc:
            snd = '잡음'
        elif sa == sb:
            snd = '같음'
        else:
            snd = '★다름'; ok = False
        mark = '  ← 달라져야 정상' if (name in expect) else ''
        print('%-8s %-22s %-22s %s   (잡음 %d바이트)%s'
              % (name, '/'.join(ramline), '/'.join(scrline), snd, noisy, mark))
        if name in expect:
            ok = True          # 선언된 게임의 차이는 회귀가 아니다
        allok = allok and ok

    print()
    if unjudged:
        print('판정불가: %s — 그 게임은 이 대본·간격에서 스스로 재현되지 않는다.'
              % ', '.join(unjudged))
        print('           **「같음」이 아니라 「모른다」다.** 대본을 결정적 구간으로 줄여야 한다.')
    print('판정: %s' % ('PASS — 선언 안 한 게임의 안정 바이트에서 차이 0'
                        if allok else '★FAIL — 선언 안 한 게임이 달라졌다'))
    return 0 if allok else 1


if __name__ == '__main__':
    sys.exit(main())
