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

    allok = True
    for name, rom in GAMES:
        if not os.path.exists(rom):
            print('%-8s (롬 없음)' % name)
            continue
        p = {k: os.path.join(tmp, '%s_%s' % (name, k)) for k in ('a', 'b', 'c', 'd')}
        sa = run(base, rom, script, p['a'] + '_')
        sb = run(base, rom, script, p['b'] + '_')
        sc = run(new, rom, script, p['c'] + '_')
        run(new, rom, script, p['d'] + '_')

        ramline, scrline = [], []
        ok = True
        for tag in TAGS:
            for ext, acc in (('ram', ramline), ('ppm', scrline)):
                u1 = unstable(p['a'] + '_', p['b'] + '_', tag, ext)
                u2 = unstable(p['c'] + '_', p['d'] + '_', tag, ext)
                if u1 is None or u2 is None:
                    acc.append('덤프없음'); ok = False; continue
                noise = u1 | u2
                if len(noise) > NOISE_CAP:
                    acc.append('판정불가(잡음%d)' % len(noise)); ok = False; continue
                a, c = rd(p['a'] + '_', tag, ext), rd(p['c'] + '_', tag, ext)
                diff = [i for i in range(len(a))
                        if i not in noise and a[i] != c[i]]
                acc.append('%d' % len(diff) + ('' if not diff else '★'))
                if diff:
                    ok = False
        noisy = sum(len(unstable(p['a'] + '_', p['b'] + '_', t, 'ram') or ()) for t in TAGS)
        snd = '같음' if sa == sc else ('잡음' if sa != sb else '★다름')
        if sa != sc and sa == sb:
            ok = False
        print('%-8s %-22s %-22s %s   (잡음 %d바이트)'
              % (name, '/'.join(ramline), '/'.join(scrline), snd, noisy))
        allok = allok and ok

    print()
    print('판정: %s' % ('PASS — 안정 바이트에서 차이 0' if allok else '★FAIL'))
    return 0 if allok else 1


if __name__ == '__main__':
    sys.exit(main())
