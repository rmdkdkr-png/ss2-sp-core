#!/usr/bin/env python3
"""캐릭터별 스파링 전투 상태를 굽는다 — 14명.

PLAYER SELECT 는 **행(팀) × 자리(3인)** 이다. D 로 행, R 로 자리, B 로 확정.
실측 지도(2026-09-03):

  쿠사나기      쿄(0)      사이수(13)  신고(11)
  초히로인      아테나(8)   유리(9)     카스미(12)
  신사우스타운   료(2)      마이(3)     테리(1)
  오로치        야시로(5)   셰르미(6)   크리스(7)
  에디트        레오나(4)   *랜덤*      이오리(10)

⚠ **루갈(14)은 이 화면에서 못 고른다** — 보스다. 그래서 「15명」이 아니라 **14명**이다.
⚠ 에디트 자리1 은 `ランダム` 이라 굽지 않는다(셰르미는 오로치 자리1 로 잡는다).

바탕은 kof_psel.st(설정 완료·모드 선택까지 끝난 PLAYER SELECT).
굽고 나서 P1 char id(0x0D8B)를 읽어 **의도한 캐릭터가 맞는지 확인**한다 —
자리 지도가 틀리면 15명치 측정이 통째로 헛돈다.

사용: kof_mkchars.py [출력폴더]
"""
import io
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
RUN = os.path.join(HERE, 'ngprun')
CORE = os.path.join(HERE, 'core.so')
ROM = os.path.expanduser('~/ss2/rom/kofr2.ngc')
PS = os.path.expanduser('~/ss2/saves/kof/kof_psel.st')

NAME = ['쿄', '테리', '료', '마이', '레오나', '야시로', '셰르미', '크리스',
        '아테나', '유리', '이오리', '신고', '카스미', '사이수', '루갈']
# (행, 자리) → 기대 char id
GRID = [(0, 0, 0), (0, 1, 13), (0, 2, 11),
        (1, 0, 8), (1, 1, 9), (1, 2, 12),
        (2, 0, 2), (2, 1, 3), (2, 2, 1),
        (3, 0, 5), (3, 1, 6), (3, 2, 7),
        (4, 0, 4), (4, 2, 10)]


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser('~/ss2/saves/kof')
    tmpdir = os.path.expanduser('~/m0')
    L = []
    for row, mem, cid in GRID:
        L += ['!load %s' % PS, '1']
        L += ['20 D', '50'] * row
        L += ['12 R', '40'] * mem
        L += ['30 B', '360', '30 B', '360', '30 B', '360', '30 B', '420', '700',
              '!save %s/kof_c%02d.st' % (out, cid), '1', '!v%02d' % cid]
    scr = os.path.join(tmpdir, 'mkchars.txt')
    io.open(scr, 'w', encoding='utf-8', newline='\n').write('\n'.join(L) + '\n')
    subprocess.run([RUN, CORE, ROM, scr, os.path.join(tmpdir, 'mk_')],
                   capture_output=True, text=True)

    print('%-8s %-6s %-6s %s' % ('이름', '기대', '실제', '판정'))
    bad = 0
    for row, mem, cid in GRID:
        p = os.path.join(tmpdir, 'mk__v%02d.ram' % cid)
        if not os.path.exists(p):
            print('%-8s %-6d %-6s 덤프없음' % (NAME[cid], cid, '-')); bad += 1; continue
        r = open(p, 'rb').read()
        got, h = r[0x0D8B], r[0x0D58]
        ok = (got == cid and h != 0)
        if not ok:
            bad += 1
        print('%-8s %-6d %-6d %s' % (NAME[cid], cid, got,
                                     'OK' if ok else ('★불일치' if got != cid else '★전투아님')))
    print('\n%d/%d 성공' % (len(GRID) - bad, len(GRID)))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
