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
# ★ 하네스는 «옵션을 넘기는» 것이어야 한다. 저장소 안의 tools/kof/ngprun 은
#   한동안 NGP_OPTS 를 통째로 무시하는 낡은 바이너리였고, 그 탓에 이 도구가
#   **입력 분기 블록이 꺼진 채**로 돌면서 「0바이트」를 남발했다.
#   NGPRUN 환경변수로 덮을 수 있다.
RUN = os.environ.get('NGPRUN') or os.path.join(HERE, 'ngprun')
HOME = os.path.expanduser('~')
KROM = os.path.join(os.path.dirname(HERE), 'rom')
TAGS = ('r1', 'r2', 'end')
NOISE_CAP = 200

GAMES = [
    ('svc',    HOME + '/ss2/rom/svc.ngc'),
    ('ss2',    HOME + '/ss2/rom/ss2.ngc'),
    ('mslug1', HOME + '/ss2/rom/pristine/Metal Slug - 1st Mission (JUE).ngc'),
    ('mslug2', HOME + '/ss2/rom/pristine/Metal Slug - 2nd Mission (JUE) [!].ngc'),
    ('ffury',  HOME + '/ss2/rom/pristine/Fatal Fury F-Contact (JUE) [!].ngc'),
    ('kofr2',  HOME + '/ss2/rom/kofr2.ngc'),
    # 월화의 검사 — 대조군은 **순정**이다(한글판 lastblade.ngc 아님)
    ('lb',     HOME + '/ss2/rom/pristine/Last Blade, The (UE) [!].ngc'),
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


def optcheck(core):
    """하네스가 NGP_OPTS 를 코어에 넘기나 — 넘기면 켬/끔이 «달라야» 한다.

    ⚠ 이 검사는 **주어진 코어가 그 옵션에 반응할 때만** 뜻이 있다.
      SS2 의 마스터 스위치를 가른 뒤로는 새 코어가 옵션과 무관하게 폴드를 돌리므로,
      **기준 코어(옛것)** 로 재야 한다. 그래도 「모른다」가 나올 수 있으니
      결과를 세 갈래(넘긴다 / 안 넘긴다 / 모른다)로 낸다.
    """
    import hashlib
    rom = None
    for _, r in GAMES:
        if os.path.exists(r): rom = r; break
    if not rom:
        return '모른다(롬 없음)'
    t = tempfile.mkdtemp()
    h = []
    for v in ('enabled', 'disabled'):
        env = dict(os.environ); env['NGP_OPTS'] = 'ngp_ss2sp=' + v
        pref = os.path.join(t, v) + '_'
        subprocess.run([RUN, core, rom, os.path.join(HERE, 'm1_reg.txt'), pref],
                       capture_output=True, text=True, env=env)
        f = '%s_r1.ram' % pref
        h.append(hashlib.md5(open(f, 'rb').read()).hexdigest() if os.path.exists(f) else None)
    if None in h:
        return '모른다(덤프 없음)'
    return '넘긴다' if h[0] != h[1] else '안 넘긴다(또는 코어가 그 옵션에 무관)'


def main():
    base, new = sys.argv[1], sys.argv[2]
    script = (sys.argv[3] if (len(sys.argv) > 3 and not sys.argv[3].startswith('-'))
              else os.path.join(HERE, 'm1_reg.txt'))
    # ★ 대본이 없으면 «여기서 멈춘다». 없는 대본으로 돌리면 모든 게임이
    #   「덤프없음」이 되는데, 그건 「달라졌다」가 아니라 «못 쟀다»다.
    #   (--expect 를 대본 자리에 넣었다가 그 꼴을 봤다.)
    if not os.path.exists(script):
        print('★ 대본 파일이 없다: %s' % script)
        print('  — 이건 회귀가 아니라 «못 쟀다»다. 경로를 고쳐라.')
        return 2
    tmp = tempfile.mkdtemp()

    oc = optcheck(base)
    print('하네스 %s' % RUN)
    print('옵션 전달 검사(기준 코어) — **%s**' % oc)
    if oc.startswith('안 넘긴다'):
        print('  ⚠ 하네스가 NGP_OPTS 를 코어에 못 넘기면 «입력 분기 블록이 꺼진 채»로 돈다.')
        print('    그 상태의 「0바이트」는 «같다»가 아니라 «아무것도 안 쟀다»다.')
        print('    NGPRUN=<옵션을 넘기는 ngprun> 로 다시 돌려라.')
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
