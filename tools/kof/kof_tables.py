#!/usr/bin/env python3
"""**오프셋 표 불변식** — 패치가 표를 망가뜨렸는지 롬 스스로 답하게 한다.

v0.2 의 최악의 사고는 `0x10BA1B` 였다. 거기는 17항목짜리 8비트 오프셋 표의
마지막 두 칸이고, `base + 오프셋` 이 각각 `.NOT DAMAGE` 와 `.EXIT` 라는 문자열의
**시작**을 가리키고 있었다. 패치가 그 두 값을 바꿔 엉뚱한 글자 한복판을 가리키게 만들었다.
그 메뉴를 여는 사람만 깨진 글자를 보게 되므로 순회 검증으로는 영원히 안 걸린다.

겉모습(가나 대역이다)은 「여기가 글자다」의 증거가 못 된다.
그래서 **롬이 스스로 답할 수 있는 것**을 묻는다:

    이 바이트열이 표라면, 모든 항목이 base+오프셋 에서 문자열의 시작을 가리켜야 한다.

원본에서 그 성질을 만족하는 표를 찾아 두고, 패치본에서 **같은 표가 여전히 성립하는지**
본다. 하나라도 깨지면 그 자리를 덮은 것이다.

사용:
  kof_tables.py find  <원본롬>              표 후보를 찾아 출력
  kof_tables.py check <원본롬> <패치롬>      원본에서 찾은 표가 패치본에서도 성립하는지
"""
import sys

MODE = (0x0A, 0x0B, 0x07)     # 문자열 앞에 붙는 모드 바이트 (실측: 0x07 도 쓰인다)
MINLEN = 6                    # 이보다 짧은 표는 우연히 성립하기 쉽다


def is_string_start(rom, a):
    """a 가 문자열의 시작인가 — 앞 바이트가 종단(0x00)이거나 표의 끝이고,
    자기 자리에 모드바이트가 오거나 뒤로 곧 0x00 이 나오면 문자열로 본다."""
    if a <= 0 or a >= len(rom):
        return False
    if rom[a] not in MODE:
        return False
    end = rom.find(b'\x00', a, a + 64)
    return end > a


def find_tables(rom, lo=0x000000, hi=0x200000):
    """8비트 오프셋 표 찾기 — 단조 증가하는 바이트열이고, 자기 시작을 base 로 삼았을 때
    모든 항목이 문자열 시작을 가리키는 구간."""
    out, i = [], lo
    while i < hi - MINLEN:
        # 단조 증가 구간을 잡는다
        j = i + 1
        while j < hi and rom[j] > rom[j - 1]:
            j += 1
        n = j - i
        if n >= MINLEN:
            hits = sum(1 for k in range(n) if is_string_start(rom, i + rom[i + k]))
            if hits == n:
                out.append((i, n))
                i = j
                continue
        i = j if j > i + 1 else i + 1
    return out


def check(rom, tables, label):
    bad = []
    for base, n in tables:
        miss = [k for k in range(n) if not is_string_start(rom, base + rom[base + k])]
        if miss:
            bad.append((base, n, miss))
    print('%s: 표 %d개 중 깨진 것 %d개' % (label, len(tables), len(bad)))
    for base, n, miss in bad:
        print('  ★ %06X (%d항목) — 항목 %s 가 문자열 시작이 아니다'
              % (base, n, ', '.join(str(m) for m in miss)))
        for m in miss:
            v = rom[base + m]
            a = base + v
            txt = rom[a:rom.find(b'\x00', a, a + 40)]
            pr = ''.join(chr(c) if 32 <= c < 127 else '.' for c in txt[:20])
            print('       [%2d] +%02X -> %06X  %r' % (m, v, a, pr))
    return bad


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    cmd = sys.argv[1]
    orig = open(sys.argv[2], 'rb').read()
    tables = find_tables(orig)

    if cmd == 'find':
        print('원본에서 찾은 오프셋 표 %d개' % len(tables))
        for base, n in tables:
            v = orig[base:base + n]
            a0 = base + v[0]
            t0 = orig[a0:orig.find(b'\x00', a0, a0 + 40)]
            pr = ''.join(chr(c) if 32 <= c < 127 else '.' for c in t0[:18])
            print('  %06X  %2d항목  +%02X..+%02X  첫 항목 → %r'
                  % (base, n, v[0], v[-1], pr))
        return 0

    if cmd == 'check':
        pat = open(sys.argv[3], 'rb').read()
        print('원본에서 표 %d개를 찾았다 (전 항목이 문자열 시작을 가리키는 것만)' % len(tables))
        bad = check(pat, tables, '패치본')
        if not bad:
            print('\n전부 성립 — 패치가 오프셋 표를 건드리지 않았다.')
        else:
            print('\n★ 위 표를 덮었다. 그 메뉴를 여는 사람만 깨진 글자를 본다 —')
            print('  화면 순회로는 안 걸리는 자리다.')
        return 1 if bad else 0

    print(__doc__)
    return 1


if __name__ == '__main__':
    sys.exit(main())
