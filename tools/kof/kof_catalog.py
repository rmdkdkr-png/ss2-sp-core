#!/usr/bin/env python3
"""act 카탈로그 — 커맨드를 **훑고 나서 이름을 붙인다**(옮겨 적지 않는다).

웹 무브리스트를 표에 옮겨 적으면 틀린 줄을 「커맨드가 안 먹는다」로 오독하게 된다.
신뢰 관계를 뒤집는다 — **측정이 진실이고 웹은 조회표다.**

방법
  * 고정 알파벳(기본기 + 모션 6종) × 버튼 2 × 강약 2 를 전수로 넣는다.
  * 지문 = (act 흐름, 지속, 최대 |vx|, 피해). 같은 지문 = 같은 기술.
  * 집계기가 경고 3종을 자동으로 찍는다:
      무반응 — act 가 평시값을 한 번도 안 벗어남
      충돌   — 다른 프로브가 같은 act (act 단독 신원 확정 불가)
      위상분화 — act 가 위상 0/1 에서 다름 (판정식은 집합으로)

⚠ 대본 버튼은 레트로패드다. **B = NGP A(펀치) · A = NGP B(킥)**.
  이걸 몰라 SVC 에서 실측 1,152회를 통째로 버린 전과가 있다.

사용: kof_catalog.py [상태.st] [세틀]
"""
import csv
import io
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
RUN = os.path.join(HERE, 'ngprun')
CORE = os.path.join(HERE, 'core.so')
ROM = os.path.expanduser('~/ss2/rom/kofr2.ngc')

# 반전은 앞=오른쪽으로 굳힌다(무대가 그렇다). 슬롯은 앞/뒤, 패드는 좌/우.
F, B, D = 'R', 'L', 'D'
DF, DB = 'D R', 'D L'
MOTION = {
    '무입력': [],
    '앞':     [F],
    '뒤':     [B],
    '236':    [D, DF, F],
    '214':    [D, DB, B],
    '623':    [F, D, DF],
    '421':    [B, D, DB],
    '41236':  [B, DB, D, DF, F],
    '63214':  [F, DF, D, DB, B],
    '236236': [D, DF, F, D, DF, F],
    '2141236': [D, DB, B, DB, D, DF, F],
}
BTN = {'P': 'B', 'K': 'A'}          # 레트로패드 문자
HOLD = {'약': 2, '강': 20}


def build(state, settle, probes):
    L = []
    for ph in (0, 1):
        for name, seq, btn, hold in probes:
            tag = 'p%d_%s' % (ph, name)
            L += ['!load %s' % state, '1',
                  '!w %s 0D3C,0D5Aw,0EC1' % tag, str(settle + ph)]
            for s in seq:
                L.append('4 %s' % s)
            last = seq[-1] if seq else None
            if last:
                L.append('%d %s %s' % (hold, last, btn))
            else:
                L.append('%d %s' % (hold, btn))
            L += ['110', '!w off']
    return '\n'.join(L) + '\n'


def analyse(path):
    d = list(csv.reader(open(path)))[1:]
    if not d:
        return None
    acts, prev = [], None
    for r in d:
        if r[2] != prev:
            acts.append((int(r[0]), r[2])); prev = r[2]
    base = acts[0][1]
    core = [v for _, v in acts[1:] if v != base]
    dur = (acts[-1][0] - acts[1][0]) if len(acts) > 1 else 0

    def sv(x):
        v = int(x); return v - 65536 if v > 32767 else v
    vx = max(abs(sv(r[3])) for r in d)
    hp = [int(r[4]) for r in d]
    dmg = max(hp) - min(hp)
    return {'acts': core, 'dur': dur, 'vx': vx, 'dmg': dmg}


def main():
    state = sys.argv[1] if len(sys.argv) > 1 else \
        os.path.expanduser('~/ss2/saves/kof/kof_spar_dmg.st')
    settle = int(sys.argv[2]) if len(sys.argv) > 2 else 20

    probes = []
    for mname, seq in MOTION.items():
        for bname, btn in BTN.items():
            for hname, hold in HOLD.items():
                probes.append(('%s_%s_%s' % (mname, bname, hname), seq, btn, hold))

    tmp = tempfile.mkdtemp()
    scr = os.path.join(tmp, 'cat.txt')
    io.open(scr, 'w', encoding='utf-8', newline='\n').write(build(state, settle, probes))
    subprocess.run([RUN, CORE, ROM, scr, os.path.join(tmp, 'c_')],
                   capture_output=True, text=True)

    rows = []
    for name, _, _, _ in probes:
        r0 = analyse(os.path.join(tmp, 'c_p0_%s.csv' % name))
        r1 = analyse(os.path.join(tmp, 'c_p1_%s.csv' % name))
        if not r0 or not r1:
            continue
        rows.append((name, r0, r1))

    print('프로브 %d개 × 위상 2 (상태 %s)\n' % (len(probes), os.path.basename(state)))
    print('%-18s %-26s %-6s %-6s %-5s %s' % ('프로브', 'act 흐름(위상0)', '지속', 'max|vx|', '피해', '경고'))
    sig = {}
    for name, a, b in rows:
        warn = []
        if not a['acts']:
            warn.append('무반응')
        if a['acts'] != b['acts']:
            warn.append('위상분화')
        key = tuple(a['acts'])
        if key and key in sig:
            warn.append('충돌:%s' % sig[key])
        elif key:
            sig[key] = name
        print('%-18s %-26s %-6d %-6d %-5d %s'
              % (name, '→'.join(a['acts'])[:26], a['dur'], a['vx'], a['dmg'],
                 ' '.join(warn)))
    print('\n서로 다른 act 지문 %d가지' % len(sig))
    return 0


if __name__ == '__main__':
    sys.exit(main())
