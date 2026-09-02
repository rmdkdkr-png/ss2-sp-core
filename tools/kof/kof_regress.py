#!/usr/bin/env python3
"""M4 — 회귀 **한 명령**. 세 층을 순서대로 돌고 숫자를 기준선 파일에 박는다.

  0층 문지기  코어 `.so` 가 소스보다 낡았으면 **거부하고 멈춘다**
  1층 단위    에뮬레이터 없이 kofsp.c 를 직접 컴파일해 돌린다 (롬 판별·폴드 계약)
  2층 무회귀  기준 코어와 여섯 게임 출력 대조 (잡음을 실측으로 빼고 본다)
  3층 발동률  14명 × 슬롯 7 × 위상 2
  4층 누출    트리거를 안 누르면 엔진 켬/끔 출력이 같은가
  5층 연속발동 **복원 없이** 한 상태에서 여러 번 발동 · 넘어간 뒤 발동

왜 5층이 따로 있나 — 3층은 **시행마다 세이브를 다시 불러온다.** 그래서
매번 깨끗한 상태에서만 재고, **상태가 쌓여서 생기는 고장이 구조적으로 안 보인다.**
실제로 반전 오프셋 오식별(0x0D4A)이 M2 40/40 · M5 98/98 을 통과하고 배포까지 나갔다
— 두 번째 발동부터 커맨드가 좌우로 뒤집혀 나가는 채로. 시행 수로는 못 잡는다.
**「같은 조건을 여러 번」이 아니라 「한 번의 삶에서 여러 번」**이 필요하다.

왜 0층이 먼저인가 — **낡은 바이너리로 잰 초록은 초록이 아니다.**
이 방에서 실제로 두 번 물렸다: 빌드 직후 회귀가 옛 `.so` 를 잡은 것,
그리고 다른 방의 러너가 `[ -x 파일 ] || gcc` 가드 탓에 **몇 주 동안 재컴파일을 안 해**
화면 색이 물빠진 채로 나간 것. 그래서 문지기를 맨 앞에 둔다.

사용:
  kof_regress.py <새코어.so> <기준코어.so> [--quick] [--trials N]
    --quick   3층을 쿄 하나로 줄인다 (빠른 확인용)
"""
import io
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.expanduser('~/ss2/repo/ss2-sp-core/src')
KOFDIR = os.path.expanduser('~/ss2/repo/ss2-sp-core/tools/kof')
BASELINE = os.path.join(HERE, 'regress_baseline.txt')


def sh(cmd, **kw):
    return subprocess.run(cmd, shell=isinstance(cmd, str), capture_output=True,
                          text=True, **kw)


def gate_binary_age(core):
    """코어가 소스보다 새 것인가. 아니면 여기서 멈춘다."""
    if not os.path.exists(core):
        return False, '코어가 없다: %s' % core
    ct = os.path.getmtime(core)
    newest, name = 0, ''
    for f in os.listdir(SRC):
        if f.endswith(('.c', '.h')):
            t = os.path.getmtime(os.path.join(SRC, f))
            if t > newest:
                newest, name = t, f
    if newest > ct:
        return False, ('%s 가 코어보다 새 것이다 (%s). **빌드부터 다시 해라** — '
                       '낡은 바이너리로 잰 초록은 초록이 아니다.'
                       % (name, time.strftime('%H:%M:%S', time.localtime(newest))))
    return True, '코어 %s · 소스 최신 %s' % (
        time.strftime('%H:%M:%S', time.localtime(ct)),
        time.strftime('%H:%M:%S', time.localtime(newest)))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    quick = '--quick' in sys.argv
    trials = int(sys.argv[sys.argv.index('--trials') + 1]) if '--trials' in sys.argv \
        else (20 if not quick else 10)
    new = args[0]
    base = args[1] if len(args) > 1 else None

    t0 = time.time()
    lines, allok = [], True

    print('══ 0층 · 문지기 ' + '─' * 40)
    ok, msg = gate_binary_age(new)
    print('   %s' % msg)
    if not ok:
        print('\n★ 여기서 멈춘다.')
        return 1

    print('\n══ 1층 · 단위 (에뮬레이터 없음) ' + '─' * 26)
    r = sh('cc -O1 -DSS2SP_RAM_POINTER -I%s -o /tmp/kof_unit %s/test_kofsp.c %s/kofsp.c'
           % (SRC, KOFDIR, SRC))
    if r.returncode:
        print(r.stderr[:400]); return 1
    r = sh('/tmp/kof_unit')
    tail = [l for l in r.stdout.splitlines() if '어긋남' in l or '미측정' in l
            or '통과' in l or '실패' in l]
    for l in tail:
        print('   ' + l.strip())
    u_ok = r.returncode == 0
    allok &= u_ok
    lines.append('1층 단위 %s' % ('PASS' if u_ok else 'FAIL'))

    if base:
        print('\n══ 2층 · 무회귀 (여섯 게임) ' + '─' * 30)
        r = sh(['python3', os.path.join(HERE, 'm1_reg.py'), base, new,
                os.path.join(HERE, 'm1_reg.txt'), '--expect', 'kofr2'])
        for l in r.stdout.splitlines()[4:]:
            if l.strip():
                print('   ' + l)
        allok &= (r.returncode == 0)
        lines.append('2층 무회귀 %s' % ('PASS' if r.returncode == 0 else 'FAIL'))

    print('\n══ 3층 · 발동률 ' + '─' * 42)
    tool = 'kof_m3gate.py' if quick else 'kof_m5gate.py'
    r = sh(['python3', os.path.join(HERE, tool), new, str(trials)])
    for l in r.stdout.splitlines():
        if '판정' in l or '합계' in l or '이상' in l:
            print('   ' + l)
    allok &= (r.returncode == 0)
    lines.append('3층 발동률 %s (%s, 시행 %d)'
                 % ('PASS' if r.returncode == 0 else 'FAIL', tool, trials))

    print('\n══ 4층 · 누출 ' + '─' * 44)
    r = sh(['python3', os.path.join(HERE, 'kof_leak.py'), new,
            '200' if not quick else '60', '300'])
    for l in r.stdout.splitlines():
        if '트리거' in l or '판정' in l:
            print('   ' + l)
    allok &= (r.returncode == 0)
    lines.append('4층 누출 %s' % ('PASS' if r.returncode == 0 else 'FAIL'))

    print('\n══ 5층 · 연속 발동 (복원 없이) ' + '─' * 27)
    r = sh(['python3', os.path.join(HERE, 'kof_repeat.py'), new])
    for l in r.stdout.splitlines():
        if '위상' in l or '판정' in l:
            print('   ' + l.strip())
    allok &= (r.returncode == 0)
    lines.append('5층 연속발동 %s' % ('PASS' if r.returncode == 0 else 'FAIL'))

    dt = time.time() - t0
    print('\n' + '═' * 56)
    print('총 %.1f분 · 판정: %s' % (dt / 60, '전부 초록' if allok else '★빨강 있음'))

    io.open(BASELINE, 'w', encoding='utf-8', newline='\n').write(
        '\n'.join(lines + ['소요 %.1f분' % (dt / 60)]) + '\n')
    print('기준선: %s' % BASELINE)
    return 0 if allok else 1


if __name__ == '__main__':
    sys.exit(main())
