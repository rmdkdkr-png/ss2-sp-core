#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SP 입력 시험기 — 커맨드를 프레임 단위로 적어 넣고 무엇이 나오는지 본다.

   쓰기:
     python3 tools/svc/sptry.py "D:2 DR:2 R:4 +B@6:2"
     python3 tools/svc/sptry.py "D:1 DR:1 R:6 +B@6:8"        독물기 후보
     python3 tools/svc/sptry.py --char 6 "D:2 DL:2 L:4 +B@6:2"   이오리
     python3 tools/svc/sptry.py --engine "R1:2"                  엔진(원버튼)으로

   문법
     <방향>:<프레임>   방향을 그 프레임 수만큼. 방향은 U D L R UR DR DL UL, 중립은 N
     +<버튼>@<시작>:<길이>   버튼을 시작 프레임부터 그 길이만큼 (커맨드와 겹쳐도 된다)
     버튼: B=약펀 A=약킥 Y=강펀 X=강킥 R1=기술키(원버튼)

   판정 (하나씩 따로 본다 — 섞으면 틀린다)
     bank  0x09AD  기술이 나갔는가. 255=대기
     act   0x0968  **어느 갈래인가** — 뱅크가 같아도 이게 다르다 (쿄 황물기 157 / 독물기 168)
     발동  뱅크가 255 를 벗어난 프레임
     히트  상대 체력이 깎인 프레임 — 전진 거리에 좌우되므로 **발동과 다르다**
"""
import collections, csv, io, os, re, subprocess, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

E    = os.path.expanduser
RUN  = E('~/ss2/repo/ss2-main/tools/svc/svcrun')
CORE = E('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM  = E('~/ss2/rom/svc.ngc')
SAVE = E('~/ss2/saves/svc')
DIR  = {'U':'U','D':'D','L':'L','R':'R','UR':'U R','DR':'D R','DL':'D L','UL':'U L','N':''}
NAME = ['쿄','테리','료','마이','레오나','아테나','이오리','하오마루','나코루루','류',
        '춘리','장기에프','켄','단','사쿠라','모리간','펠리시아','가일']

def parse(spec):
    """→ (프레임별 방향 목록, [(버튼, 시작, 길이)…])"""
    dirs, btns = [], []
    for tok in spec.split():
        m = re.fullmatch(r'\+([A-Z0-9]+)@(\d+):(\d+)', tok)
        if m: btns.append((m.group(1), int(m.group(2)), int(m.group(3)))); continue
        m = re.fullmatch(r'([A-Z]+):(\d+)', tok)
        if not m: raise SystemExit('  못 읽는 토큰: %s' % tok)
        d = DIR.get(m.group(1))
        if d is None: raise SystemExit('  모르는 방향: %s' % m.group(1))
        dirs += [d] * int(m.group(2))
    return dirs, btns

def build(dirs, btns, total):
    ch = dirs + [''] * (total - len(dirs))
    out = []
    for i, d in enumerate(ch):
        on = [b for b, s, h in btns if s <= i < s + h]
        out.append((' '.join([d] + on)).strip() or '-')
    return out

def run(frames, save, engine, tail=60):
    sc = ['1 -', '!load %s' % save, '20 -']
    for i, f in enumerate(frames):
        sc += ['1 %s' % f, '!w t@%d' % i]
    n = len(frames)
    for _ in range(tail // 2):
        sc += ['2 -', '!w t@%d' % n]; n += 1
    open('/tmp/sptry.txt', 'w').write('\n'.join(sc) + '\n')
    env = dict(os.environ); env['PROBE_CSV'] = '/tmp/sptry.csv'
    env['SVCSP_FORCE' if engine else 'SVCSP_OFF'] = '1'
    env['SVCSP_DEBUG'] = '1'
    r = subprocess.run([RUN, CORE, ROM, '/tmp/sptry.txt'], capture_output=True, env=env,
                       cwd=os.path.dirname(RUN))
    rows = {}
    for x in csv.DictReader(open('/tmp/sptry.csv')):
        rows[int(x['tag'].split('@')[1])] = x
    return rows, r.stderr.decode('utf-8', 'replace')

if __name__ == '__main__':
    a = sys.argv[1:]
    cid, engine, mode = 0, False, '정지'
    while a and a[0].startswith('--'):
        if a[0] == '--char':   cid = int(a[1]); a = a[2:]
        elif a[0] == '--engine': engine = True; a = a[1:]
        elif a[0] == '--guard':  mode = '상단방어'; a = a[1:]
        else: raise SystemExit('  모르는 옵션: %s' % a[0])
    if not a: raise SystemExit(__doc__)
    spec = ' '.join(a)
    save = '%s/svc_ct_%s_%d_0.st' % (SAVE, mode, cid)
    if not os.path.exists(save): raise SystemExit('  세이브 없음: %s' % save)
    dirs, btns = parse(spec)
    end = max([len(dirs)] + [s + h for _, s, h in btns])
    frames = build(dirs, btns, end + 24)
    rows, err = run(frames, save, engine)

    print('  %s · %s · %s' % (NAME[cid], mode, '엔진' if engine else '순정'))
    print('  입력 %s' % spec)
    print('  입력 길이 %d프레임 (커맨드 %d · 버튼 끝 %d)'
          % (end, len(dirs), max([s + h for _, s, h in btns] or [0])))
    ks = sorted(rows)
    h0 = int(rows[ks[0]]['hp2'])
    fire = hit = None
    for k in ks:
        if fire is None and rows[k]['bank'] != '255': fire = k
        if hit is None and int(rows[k]['hp2']) < h0: hit = k
    print()
    if fire is None:
        print('  ** 안 나갔다 ** (뱅크가 255 에서 안 벗어남)')
    else:
        # act 는 발동 직후 몇 프레임 뒤에 정해진다 — 그 구간에서 읽는다
        acts = [rows[k]['act'] for k in ks if fire <= k <= fire + 12]
        act = collections.Counter(a for a in acts if a not in ('0',)).most_common(1)
        print('  발동 %d프레임 · 뱅크 %s · act %s'
              % (fire, rows[fire]['bank'], act[0][0] if act else '—'))
        print('  히트 %s · 피해 %d · 콤보 %d'
              % ('%d프레임' % hit if hit else '안 맞음',
                 h0 - min(int(rows[k]['hp2']) for k in ks),
                 max(int(rows[k]['combo']) for k in ks)))
    print('  뱅크 흐름 %s' % ' '.join(
        dict.fromkeys(rows[k]['bank'] for k in ks if rows[k]['bank'] != '255'))[:60])
    print('  act  흐름 %s' % ' '.join(
        dict.fromkeys(rows[k]['act'] for k in ks))[:60])
    if engine:
        for l in err.split('\n'):
            if 'svcsp' in l: print('    %s' % l.strip())
