#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""스파링 세이브 굽기 — 18캐릭터 × 적동작(정지/상단방어).

   왜 필요한가: 기존 c*_*.st 는 **아케이드 전투**라 CPU 가 반격한다. 무입력 320프레임에
   내 체력이 42→28 로 깎이고 위치도 140→95→184 로 밀렸다(실측). 그 위에서 잰 콤보 수치는
   전부 무효다. 콤보를 재려면 상대가 가만히 있어야 한다.

   스파링 설정 화면에 그 스위치가 있다:
       시작 / 타임 / 무적 / 초필살 / 적레벨 / **적동작** / 배경 / 시간대
   커서를 5칸 내리면 적동작. → 로 돌리면 보통 → **정지** → **상단방어** → …
     정지     = 가만히 서 있는 허수아비 → 콤보·캔슬 측정용
     상단방어 = 서서 가드 → 가드 대조군

   부팅은 한 번만 한다. 캐릭터 선택 화면에서 상태를 떠 두고 그걸 18번 되불러
   커서만 다르게 움직인다(부팅 1500프레임 × 18 을 아낀다).

   쓰기: python3 tools/svc/mkspar.py [정지|상단방어|둘다]
"""
import collections, csv, io, os, shutil, subprocess, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

E    = os.path.expanduser
RUN  = E('~/ss2/repo/ss2-main/tools/svc/svcrun')
CORE = E('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM  = E('~/ss2/rom/svc.ngc')
SAVE = E('~/ss2/saves/svc')
CWD  = E('~/ss2/repo/ss2-main/tools/svc')
NAME = ['쿄','테리','료','마이','레오나','아테나','이오리','하오마루','나코루루','류',
        '춘리','장기에프','켄','단','사쿠라','모리간','펠리시아','가일']

DUMMY = {'보통': 0, '정지': 1, '상단방어': 2}   # 적동작에서 → 를 누르는 횟수

def spar(mode, cid):
    return '%s/svc_spar_%s_%d.st' % (SAVE, mode, cid)

def go(lines, csvp=None):
    open('/tmp/mkspar.txt', 'w').write('\n'.join(lines) + '\n')
    env = dict(os.environ)
    if csvp: env['PROBE_CSV'] = csvp
    env['SVCSP_FORCE'] = '0'
    return subprocess.run([RUN, CORE, ROM, '/tmp/mkspar.txt'],
                          capture_output=True, env=env, cwd=CWD)

def boot_to_select(dummy):
    """부팅 → 스파링 설정(적동작 지정) → 유파 → **캐릭터 선택 화면**까지."""
    sc = ['360 -']
    for _ in range(10): sc += ['3 ST', '47 -']
    sc += ['3 B', '60 -', '3 B', '60 -']        # 메인 메뉴
    sc += ['3 D', '30 -', '3 D', '30 -']        # 스파링
    sc += ['3 B', '97 -']                       # 설정 화면
    for _ in range(5): sc += ['3 D', '20 -']    # 적동작 줄
    for _ in range(DUMMY[dummy]): sc += ['3 R', '22 -']
    for _ in range(5): sc += ['3 U', '20 -']    # 시작 줄로 복귀
    sc += ['3 B', '97 -']                       # 유파 선택
    sc += ['3 B', '97 -']                       # 캐릭터 선택
    return sc

def pick(col, row):
    """선택 화면 좌상단에서 (열, 행) 으로 이동. 격자는 좌 2열 + 우 2열."""
    sc = []
    for _ in range(7): sc += ['2 U', '10 -']     # 좌상단으로 되돌린다 (넉넉히)
    for _ in range(4): sc += ['2 L', '10 -']
    for _ in range(col): sc += ['2 R', '12 -']
    for _ in range(row): sc += ['2 D', '12 -']
    return sc

def to_fight():
    """캐릭터 확정 → 상대(기본 커서) 확정 → 인트로 넘기고 GO 까지."""
    sc = ['3 B', '90 -']                         # 내 캐릭터
    sc += ['3 B', '90 -']                        # 상대 (기본 커서 그대로)
    for _ in range(6): sc += ['3 B', '80 -']     # 인트로 대사
    sc += ['150 -']
    return sc

def survey(dummy):
    """격자 24칸을 돌며 어느 칸이 어느 캐릭터인지 실측으로 적는다."""
    sc = boot_to_select(dummy) + ['!save /tmp/sel.st']
    for col in range(4):
        for row in range(6):
            t = 'g%d_%d' % (col, row)
            sc += ['!load /tmp/sel.st', '20 -'] + pick(col, row) + to_fight()
            sc += ['!w %s@0' % t, '!save /tmp/%s.st' % t]
    go(sc, '/tmp/mkspar.csv')
    out = {}
    for r in csv.DictReader(open('/tmp/mkspar.csv')):
        t = r['tag'].split('@')[0]
        c = int(r['chr'])
        if c < 18: out.setdefault(c, t)
    return out

def verify(paths, dummy):
    """구운 세이브가 쓸 만한지 — 조작이 되는가, 상대가 반격하지 않는가."""
    sc = ['1 -']
    for cid, p in sorted(paths.items()):
        t = 'v%d' % cid
        sc += ['!load %s' % p, '30 -', '!w %s@0' % t]
        sc += ['40 R', '!w %s@1' % t]            # 걸으면 움직이는가
        for i in range(2, 8): sc += ['40 -', '!w %s@%d' % (t, i)]
    go(sc, '/tmp/mkver.csv')
    d = collections.defaultdict(dict)
    for r in csv.DictReader(open('/tmp/mkver.csv')):
        t, at = r['tag'].split('@'); d[t][int(at)] = r
    print('  %-8s %-8s %-8s %s' % ('캐릭터', '걷기', '반격', '판정'))
    ok = {}
    for cid, p in sorted(paths.items()):
        rs = [d['v%d' % cid][i] for i in sorted(d['v%d' % cid])]
        moved = abs(int(rs[1]['p1x']) - int(rs[0]['p1x']))
        hurt  = int(rs[0]['hp1']) - min(int(r['hp1']) for r in rs)
        good = moved >= 4 and hurt == 0
        ok[cid] = good
        print('  %-8s %-8s %-8s %s' % (NAME[cid], '%dpx' % moved,
              '체력 -%d' % hurt, '쓸 만함' if good else '** 못 씀 **'))
    return ok

def contact(mode, chars, recipe=None):
    """접촉 상태 세이브를 굽는다 — 캐릭터 × 방향.

       poke 로 거리를 맞추려 했으나 안 된다: 0x0934 는 **표시 사본**이라 값은 박히는데
       판정에는 안 쓰인다(150~330 전 구간 무피해, 걸어가면 피해 3 — 실측). 걸어야 한다.
       방향 1 은 앞점프로 상대를 **넘어간** 뒤 되돌아 붙는다.

       캐릭터마다 걸음 속도가 달라(40프레임에 31~52px) 되걷기 길이를 몇 가지 시도하고
       실제로 때려서 닿는 것만 남긴다.

       가드 상대(상단방어)에서는 피해가 0 이 정상이다 — 그게 대조군의 뜻이다. 그래서
       거리를 피해로 고를 수 없다. 정지판에서 고른 **같은 걸음**(recipe)을 그대로 쓰고
       방향만 확인한다. 같은 지형·같은 걸음이니 붙는 자리도 같다."""
    BACK = (40, 46, 52, 58, 64)
    SAM = list(range(6, 96, 8))
    sc = ['1 -']; made = []
    for cid in chars:
        src = spar(mode, cid)
        # 방향 0 — 그냥 걸어가 붙는다
        t = 'f0_%d' % cid
        sc += ['!load %s' % src, '30 -', '90 R', '!save /tmp/%s.st' % t, '2 A']
        prev = 0
        for s in SAM: sc += ['%d -' % (s - prev), '!w %s@%d' % (t, s)]; prev = s
        made.append((t, cid, 0))
        # 방향 1 — 넘어갔다가 되돌아 붙는다
        for b in BACK:
            t = 'f1_%d_%d' % (cid, b)
            # 120프레임을 밀어 **구석에 붙인 뒤** 넘는다. 60프레임으로는 가드 상대가
            # 뒤로 물러나며 점프가 짧아져 못 넘는다(가드판 방향1 이 전부 실패했다).
            sc += ['!load %s' % src, '30 -', '120 R', '3 U R', '40 R', '20 -',
                   '%d L' % b, '!save /tmp/%s.st' % t, '2 A']
            prev = 0
            for s in SAM: sc += ['%d -' % (s - prev), '!w %s@%d' % (t, s)]; prev = s
            made.append((t, cid, 1))
    go(sc, '/tmp/mkcon.csv')
    d = collections.defaultdict(dict)
    for r in csv.DictReader(open('/tmp/mkcon.csv')):
        t, at = r['tag'].rsplit('@', 1); d[t][int(at)] = r
    best = {}
    for t, cid, face in made:
        rows = d.get(t)
        if not rows: continue
        rs = [rows[k] for k in sorted(rows)]
        if rs[0]['face'] != str(face): continue          # 방향이 안 맞으면 버린다
        if recipe is not None:
            # 정지판과 같은 걸음을 먼저 쓰되, 막는 상대는 밀어내는 힘이 달라 그 걸음으로는
            # 못 넘어가는 일이 있다. 그때는 방향만 맞으면 받는다.
            cur = best.get((cid, face))
            if recipe.get((cid, face)) == t:   best[(cid, face)] = (2, t)
            elif cur is None or cur[0] < 1:    best[(cid, face)] = (1, t)
            continue
        h = [int(r['hp2']) for r in rs]
        dmg = max(h) - min(h); cmb = max(int(r['combo']) for r in rs)
        if cmb >= 1 and dmg > 0 and dmg > best.get((cid, face), (0, None))[0]:
            best[(cid, face)] = (dmg, t)
    out = {}
    for (cid, face), (dmg, t) in sorted(best.items()):
        dst = '%s/svc_ct_%s_%d_%d.st' % (SAVE, mode, cid, face)
        shutil.copyfile('/tmp/%s.st' % t, dst)
        out[(cid, face)] = t
    print('  접촉 세이브 %d/%d칸' % (len(out), len(chars) * 2))
    for cid in chars:
        r = ['%s %s' % ('→←'[f],
                        ('피해%d' % best[(cid, f)][0] if recipe is None else '붙음')
                        if (cid, f) in best else '** 실패 **') for f in (0, 1)]
        print('    %-8s %s' % (NAME[cid], '   '.join(r)))
    return out

if __name__ == '__main__':
    want = sys.argv[1] if len(sys.argv) > 1 else '둘다'
    if want == '접촉':
        print('── 접촉 굽기 · 적동작 = 정지')
        rec = contact('정지', [c for c in range(18) if os.path.exists(spar('정지', c))])
        print()
        print('── 접촉 굽기 · 적동작 = 상단방어 (정지판과 같은 걸음을 쓴다)')
        contact('상단방어', [c for c in range(18) if os.path.exists(spar('상단방어', c))],
                recipe=rec)
        sys.exit(0)
    modes = ['정지', '상단방어'] if want == '둘다' else [want]
    for m in modes:
        print('── 적동작 = %s' % m)
        grid = survey(m)
        print('  격자에서 찾은 캐릭터 %d명' % len(grid))
        miss = [NAME[c] for c in range(18) if c not in grid]
        if miss: print('  ** 못 찾음: %s' % ' '.join(miss))
        for cid, g in sorted(grid.items()):
            print('    %-8s %s' % (NAME[cid], g))
        paths = {}
        for cid, g in sorted(grid.items()):
            dst = '%s/svc_spar_%s_%d.st' % (SAVE, m, cid)
            shutil.copyfile('/tmp/%s.st' % g, dst)     # /tmp 와 홈은 다른 장치다
            paths[cid] = dst
        verify(paths, m)
        print()
