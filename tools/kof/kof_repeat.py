#!/usr/bin/env python3
"""5층 — **한 상태에서 연속 발동**. 그리고 **넘어간 뒤 발동**.

왜 이 층이 생겼나 (2026-09-03):
  배포된 엔진이 **한 라운드에 두 번째 발동부터 커맨드를 좌우로 뒤집어 넣고 있었다.**
  반전 오프셋을 `0x0D4A` 로 잘못 잡았는데, 그 바이트는 반전이 아니라
  **필살기를 한 번 쓰면 서고 안 내려오는 플래그**였다. 그래서 첫 발동은 멀쩡하고
  두 번째부터 236 대신 214 가 나갔다.

  ★ 관문 M2(40/40) · M5(98/98) 가 전부 초록이었다. **시행마다 세이브를 다시 불러왔기
    때문**이다 — 매번 깨끗한 상태, 시작 쪽, 플래그 0. 「상태가 쌓여서 생기는 고장」이
    구조적으로 안 보이는 얼개였다. 시행 수를 아무리 늘려도 못 잡는다.

  → 그래서 **복원 없이 이어서** 돌리는 층을 따로 만든다.
    「같은 조건을 여러 번」이 아니라 **「한 번의 삶에서 여러 번」**이다.

판정은 기술 번호를 박지 않는다 — **1회차의 act 흐름을 그대로 정답으로 삼고**
2·3회차가 같은 흐름을 내는지 본다. 그래야 캐릭터가 바뀌어도 그대로 쓴다.

사용: kof_repeat.py <코어.so>
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

SETTLE, GAP, WIN, TAIL = 20, 60, 46, 140
IDLE = {'213'}                       # 평시 서있기 — 흐름에서 뺀다

# (이름, 발동 전 사전입력, 발동 횟수)
CASES = [
    ('연속 3회',        [],                     3),
    ('넘어간 뒤 1회',   ['16 R', '30 U R', '40'], 1),
    ('넘어간 뒤 연속 2회', ['16 R', '30 U R', '40'], 2),
]


def script(tag, pre, n):
    L = ['!load %s' % ST, tag[0], '!w %s 0D3C,0D4C' % tag[1]] + list(pre) + [str(SETTLE)]
    for i in range(n):
        L += ['2 R1', str(GAP)]
    return L + [str(TAIL), '!w off']


def flow(rows, lo, hi):
    """구간 안의 act 흐름 — 평시를 뺀 중복 없는 순서열.

    ★ 창 머리에 **앞 기술의 회복 동작이 묻어 온다**(직전 발동의 꼬리). 그건 이번
      발동이 만든 것이 아니므로 뺀다 — 안 빼면 2회차부터 늘 「다르다」가 되어
      무효지표가 된다. 창이 시작하기 직전의 act 와 같은 머리만 잘라낸다."""
    carry = None
    for f, a in rows:
        if f < lo:
            carry = a
        else:
            break
    out, prev = [], None
    for f, a in rows:
        if lo <= f < hi and a != prev:
            if a not in IDLE:
                out.append(a)
            prev = a
    while out and carry is not None and out[0] == carry:
        out.pop(0)
    return out


def main():
    core = sys.argv[1]
    tmp = tempfile.mkdtemp()
    env = dict(os.environ)
    env['NGP_OPTS'] = 'ngp_ss2sp=enabled'
    env['KOFSP_ON'] = '1'

    L, meta = [], []
    for ph in (0, 1):
        for ci, (nm, pre, n) in enumerate(CASES):
            t = 'r%d%d' % (ph, ci)
            meta.append((ph, nm, pre, n, t))
            L += script((str(1 + ph), t), pre, n)
    scr = os.path.join(tmp, 's.txt')
    io.open(scr, 'w', encoding='utf-8', newline='\n').write('\n'.join(L) + '\n')
    subprocess.run([RUN, core, ROM, scr, os.path.join(tmp, 'p_')],
                   capture_output=True, text=True, env=env)

    allok = True
    for ph, nm, pre, n, t in meta:
        d = list(csv.reader(open(os.path.join(tmp, 'p_%s.csv' % t))))[1:]
        b = int(d[0][0])
        rows = [(int(r[0]) - b, r[2]) for r in d]
        pre_len = sum(int(x.split()[0]) for x in pre)
        flows = []
        for i in range(n):
            t0 = pre_len + SETTLE + i * (2 + GAP)
            # 창은 **다음 발동 직전까지**로 끊는다. 넉넉히 잡으면 다음 회차를
            # 물어 와서 「1회차가 2회차와 다르다」가 늘 참이 된다 — 무효지표다.
            flows.append(flow(rows, t0, t0 + 2 + GAP))
        ok = all(f == flows[0] for f in flows) and bool(flows[0])
        allok &= ok
        print('  위상%d %-18s %s' % (ph, nm, 'OK' if ok else '★불일치'))
        for i, f in enumerate(flows):
            mark = '' if i == 0 or f == flows[0] else '   ← 1회차와 다르다'
            print('        %d회차 %s%s' % (i + 1, '→'.join(f) if f else '(무발동)', mark))

    print('\n판정: %s' % ('전부 초록' if allok else '★빨강 — 두 번째 발동부터 다른 기술이 나간다'))
    return 0 if allok else 1


if __name__ == '__main__':
    sys.exit(main())
