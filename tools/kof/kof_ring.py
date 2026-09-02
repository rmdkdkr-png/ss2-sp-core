#!/usr/bin/env python3
"""방향 이력 링 사냥 — 「값」이 아니라 **「쓰이는 자리가 옮겨가는 규칙」**을 찾는다.

`!w` 로는 못 한다(24칸 상한이고, 어디를 볼지 모른다). 그래서 프레임마다 램 16KB 를
통째로 떠서 오프라인으로 본다.

찾는 지문:
  * 링 구성원은 **서로 일정한 간격(stride)** 으로 늘어서 있다.
  * 방향을 누를 때마다 **다음 칸**이 쓰인다 — 즉 변한 주소가 stride 만큼씩 옮겨간다.
  * 그러다 **되돌아온다**(원형).

사용: kof_ring.py <덤프접두사> [프레임수]
"""
import os
import sys
from collections import defaultdict


def main():
    pref = sys.argv[1]
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 48
    frames = []
    for i in range(n):
        p = '%s_f%02d.ram' % (pref, i)
        if not os.path.exists(p):
            break
        frames.append(open(p, 'rb').read())
    print('프레임 %d개' % len(frames))

    # 주소별 「변한 프레임 목록」
    chg = defaultdict(list)
    for i in range(1, len(frames)):
        a, b = frames[i - 1], frames[i]
        for k in range(len(a)):
            if a[k] != b[k]:
                chg[k].append(i)
    print('한 번이라도 변한 주소 %d개' % len(chg))

    # 방향을 누른 프레임에만 변하는 주소 = 입력 이력 후보
    press = [i for i in range(len(frames)) if i % 4 == 1]
    cand = {k: v for k, v in chg.items()
            if v and len(v) <= 14 and all(f in press or f - 1 in press for f in v)}
    print('입력 프레임에만 변한 주소 %d개\n' % len(cand))

    # 같은 stride 로 늘어선 무리를 찾는다
    ks = sorted(cand)
    best = []
    for stride in (1, 2, 3, 4, 6, 8):
        run, cur = [], []
        for k in ks:
            if cur and k - cur[-1] == stride:
                cur.append(k)
            else:
                if len(cur) >= 6:
                    run.append(list(cur))
                cur = [k]
        if len(cur) >= 6:
            run.append(cur)
        for r in run:
            best.append((len(r), stride, r))
    best.sort(reverse=True)

    if not best:
        print('일정 간격으로 늘어선 무리가 없다 — 링이 없거나 구조가 다르다.')
        for k in ks[:30]:
            print('  %04X  변한 프레임 %s' % (k, cand[k]))
        return 1

    for cnt, stride, r in best[:4]:
        print('★ 간격 %d 로 %d칸: %04X ~ %04X' % (stride, cnt, r[0], r[-1]))
        for k in r[:16]:
            print('    %04X  변한 프레임 %s  값 %s'
                  % (k, cand[k], [frames[i][k] for i in range(0, len(frames), 4)][:10]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
