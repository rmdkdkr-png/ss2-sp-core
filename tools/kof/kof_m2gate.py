#!/usr/bin/env python3
"""M2 본 관문 — 트리거 한 번에 236+P 가 실제로 나가는가.

판정은 **act 진입**으로 한다(계획의 판정 사다리 1층):
  236 고유 act = 96 · 623 고유 act = 127/7/63 · 탭 57 · 홀드 173.
「나갔다」를 뱅크나 피해로 재지 않는다 — SVC 오판 전과.

통과 조건: **위상 2종 각각 ≥19/20.**
시행마다 세틀 프레임을 2씩 늘려(위상 홀짝은 유지) 타이밍 강건성까지 본다.

★ 대조군을 반드시 같이 돌린다:
  · 엔진 끔(KOFSP_ON 없음)에서 같은 트리거 → **하나도 안 나가야** 한다.
    둘 다 통과하면 아무것도 안 재고 있는 것이다(무효지표).

사용: kof_m2gate.py <코어.so> [시행수]
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
ST = os.path.expanduser('~/ss2/saves/kof/kof_spar.st')
QCF, DP = '96', ('127', '7', '63')


def run(core, engine_on, n):
    tmp = tempfile.mkdtemp()
    lines = []
    for ph in (0, 1):
        for i in range(n):
            lines += ['!load %s' % ST, '1', '!w p%d_%02d 0D3C' % (ph, i),
                      str(20 + ph + 2 * i), '2 R1', '90', '!w off']
    scr = os.path.join(tmp, 'g.txt')
    io.open(scr, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
    env = dict(os.environ)
    env['NGP_OPTS'] = 'ngp_ss2sp=enabled'
    if engine_on:
        env['KOFSP_ON'] = '1'
    else:
        env.pop('KOFSP_ON', None)
    subprocess.run([RUN, core, ROM, scr, os.path.join(tmp, 'o_')],
                   capture_output=True, text=True, env=env)
    out = {}
    for ph in (0, 1):
        ok = 0
        detail = []
        for i in range(n):
            p = os.path.join(tmp, 'o_p%d_%02d.csv' % (ph, i))
            if not os.path.exists(p):
                detail.append('?'); continue
            d = list(csv.reader(open(p)))[1:]
            s, prev = [], None
            for r in d:
                if r[2] != prev:
                    s.append(r[2]); prev = r[2]
            ss = set(s)
            if QCF in ss and not (set(DP) & ss):
                ok += 1; detail.append('o')
            elif set(DP) & ss:
                detail.append('D')          # 623 으로 나갔다
            elif len(ss) > 1:
                detail.append('x')          # 뭔가 나가긴 했다
            else:
                detail.append('.')          # 아무 일도 없었다
        out[ph] = (ok, ''.join(detail))
    return out


def main():
    core = sys.argv[1]
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 20
    print('코어 %s · 시행 %d × 위상 2\n' % (core, n))
    on = run(core, True, n)
    off = run(core, False, n)
    print('%-10s %-8s %s' % ('', '위상0', '위상1'))
    print('%-10s %-8s %s' % ('엔진 켬',
                             '%d/%d' % (on[0][0], n), '%d/%d' % (on[1][0], n)))
    print('%-10s %-8s %s' % ('엔진 끔',
                             '%d/%d' % (off[0][0], n), '%d/%d' % (off[1][0], n)))
    print('\n켬  위상0 %s\n켬  위상1 %s' % (on[0][1], on[1][1]))
    print('끔  위상0 %s\n끔  위상1 %s' % (off[0][1], off[1][1]))
    print('\n(o=236 나감  D=623 으로 나감  x=딴것  .=무반응)')

    good = on[0][0] >= n - 1 and on[1][0] >= n - 1
    ctrl = off[0][0] == 0 and off[1][0] == 0
    if not ctrl:
        print('\n★무효지표 — 엔진을 꺼도 나간다. 아무것도 재고 있지 않다.')
    elif good:
        print('\n판정: PASS — 위상 2종 모두 %d/%d 이상, 대조군 0' % (n - 1, n))
    else:
        print('\n판정: ★FAIL')
    return 0 if (good and ctrl) else 1


if __name__ == '__main__':
    sys.exit(main())
