#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SP 콤보 실측 — 「누르면 나가나」가 아니라 **끝까지 이어지나**를 잰다.

   쓰기:
     python3 tools/svc/spcombo.py 쿄     캔슬 격자 전부 (노멀 10 × 배치 3 × 지연 × 좌우 × 정지/가드)
     python3 tools/svc/spcombo.py 전체   쿄에서 추린 조합으로 18캐릭터
     python3 tools/svc/spcombo.py 루트   정규 콤보 루트 (점프강 → 서서강 → 특수기 → 캔슬 → 파생)
     python3 tools/svc/spcombo.py 손     같은 루트를 엔진 끄고 직접 커맨드로 (대조군)

   ── 버튼 (이걸 몰라서 1152회를 버렸다) ───────────────────────────────
   대본 문자는 **레트로패드** 기준이고 코어가 NGP 로 바꾼다(`svcsp.c:589-604`):

       B = 약펀치      A = 약킥      Y = 강펀치      X = 강킥      R1 = 기술키

   앞 실측은 `2 A` 를 「약펀치」로 알고 눌렀다 — 전부 **약킥**이었다. 하필 쿄는
   18명 중 유일하게 `cancel_dud=1` 이고 그 플래그는 **마지막 노멀이 킥일 때만** 선다.
   킥으로 치면 엔진이 캔슬 창을 **일부러 피해** 평타 +40프레임 뒤에 쏜다
   (킥 캔슬창의 지정기가 누에잡기 = 비공격기라서). 그래서 쿄는 영영 안 붙었다.
   실측으로 확인한 간격 — 약펀 26f(연결) / 약킥 48f(끊김) / 88식 74f(끊김).

   또 「강」은 홀드가 아니다. 약 버튼은 6프레임에서 강제 해제된다. 강은 Y/X 다.

   ── 판정 ─────────────────────────────────────────────────────────
   **콤보 카운터(0x0B17)** 하나만 쓴다(화면 「N HITS!」 그 값, SVC_MEMO §12).
   단 다타 노멀(88식은 2타)이 있으므로 **노멀 단독 기준선보다 타수가 늘어야** 캔슬로 센다.
   기준선 없이 「콤보 ≥ 2」로 세면 88식·강킥이 거짓양성으로 잡힌다.

   ── 시험대 ───────────────────────────────────────────────────────
   스파링 「적동작 = 정지(허수아비) / 상단방어(가드)」. 아케이드 세이브는 CPU 가
   반격해서(무입력 320프레임에 체력 42→28) 못 쓴다. `mkspar.py` 가 접촉 상태까지 구워 둔다.
"""
import collections, csv, io, os, re, subprocess, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

E    = os.path.expanduser
CORE = E('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM  = E('~/ss2/rom/svc.ngc')
RUN  = E('~/ss2/repo/ss2-main/tools/svc/svcrun')
SAVE = E('~/ss2/saves/svc')
NAME = ['쿄','테리','료','마이','레오나','아테나','이오리','하오마루','나코루루','류',
        '춘리','장기에프','켄','단','사쿠라','모리간','펠리시아','가일']

SAM   = list(range(6, 166, 7))
DUMMY = ('정지', '상단방어')

# (이름, 방향, 버튼). 방향은 「앞」기준 — 왼쪽을 보면 물리 좌우가 뒤집힌다.
NORMALS = [('약펀', '',   'B'), ('강펀', '',   'Y'),
           ('약킥', '',   'A'), ('강킥', '',   'X'),
           ('앉약펀', 'D', 'B'), ('앉강펀', 'D', 'Y'),
           ('앉약킥', 'D', 'A'), ('앉강킥', 'D', 'X'),
           ('88식',  'DF', 'A'), ('굉부양', 'F',  'A')]
DIR = {0: {'': '', 'D': 'D', 'DF': 'D R', 'F': 'R', 'B': 'L'},
       1: {'': '', 'D': 'D', 'DF': 'D L', 'F': 'L', 'B': 'R'}}
SPS   = (('탭', '2 R1'), ('홀드', '20 R1'))

def press(nd, nb, face, frames=2):
    d = DIR[face][nd]
    return '%d %s' % (frames, ('%s %s' % (d, nb)).strip())

def ct(mode, cid, face):
    return '%s/svc_ct_%s_%d_%d.st' % (SAVE, mode, cid, face)

def cells():
    return sorted({(m, c, f) for m in DUMMY for c in range(18) for f in (0, 1)
                   if os.path.exists(ct(m, c, f))})

def run(sc, csvp, engine=True, dbg=False):
    open('/tmp/spc.txt', 'w').write('\n'.join(sc) + '\n')
    env = dict(os.environ); env['PROBE_CSV'] = csvp
    if engine: env['SVCSP_FORCE'] = '1'
    else:      env.pop('SVCSP_FORCE', None)
    if dbg: env['SVCSP_DEBUG'] = '1'
    r = subprocess.run([RUN, CORE, ROM, '/tmp/spc.txt'], capture_output=True, env=env,
                       cwd=os.path.dirname(RUN))
    return r.stderr.decode('utf-8', 'replace')

def read(csvp):
    d = collections.defaultdict(dict)
    for r in csv.DictReader(open(csvp)):
        t, at = r['tag'].rsplit('@', 1)
        d[t][int(at)] = r
    return d

def sample(sc, tag):
    prev = 0
    for s in SAM:
        sc.append('%d -' % (s - prev)); sc.append('!w %s@%d' % (tag, s)); prev = s

def peak(rows):
    if not rows: return 0, 0
    r = list(rows.values())
    h = [int(x['hp2']) for x in r]
    return max(int(x['combo']) for x in r), max(h) - min(h)

def span(ds, scale):
    if not ds: return '—'
    ds = sorted(ds); out = []; a = b = ds[0]
    for x in ds[1:]:
        if scale.index(x) == scale.index(b) + 1: b = x
        else: out.append((a, b)); a = b = x
    out.append((a, b))
    return ' '.join('%df' % s if s == e else '%d~%df' % (s, e) for s, e in out)

# ── 시퀀스 시행 ──────────────────────────────────────────────────────
def trial(sc, tag, save, steps):
    """steps = [(누를 것, 그 뒤 대기프레임), …]. 임의 길이의 콤보 루트를 그대로 쓴다."""
    sc += ['!load %s' % save, '20 -']
    for pr, dl in steps:
        sc.append(pr)
        if dl: sc.append('%d -' % dl)
    sample(sc, tag)

def grid(chars, delays, layouts, csvp):
    """2단 — 노멀 → SP. 배치 = SP 를 노멀보다 먼저/동시/나중에.
       킬캔슬 창이 좁고 주입에 6프레임이 더 붙으므로 먼저 누르는 쪽이 유효할 수 있다.
       앞 실측은 「나중」만 눌러 그 구간을 통째로 놓쳤다."""
    sc = ['1 -']; keys = []
    for (mode, cid, face) in chars:
        sv = ct(mode, cid, face)
        for ni, (nn, nd, nb) in enumerate(NORMALS):
            npr = press(nd, nb, face)
            t = 'b_%s_%d_%d_%d' % (mode, cid, face, ni)     # 기준선 — 노멀 단독
            keys.append((t, mode, cid, face, nn, None, None, None))
            trial(sc, t, sv, [(npr, 0)])
            for si, (sn, spr) in enumerate(SPS):
                for lay in layouts:
                    for dl in delays:
                        t = 'g_%s_%d_%d_%d_%d_%s_%d' % (mode, cid, face, ni, si, lay, dl)
                        keys.append((t, mode, cid, face, nn, sn, lay, dl))
                        if lay == 'SP먼저':   steps = [(spr, dl), (npr, 0)]
                        elif lay == '동시':   steps = [('2 %s R1' % (
                                                  '%s %s' % (DIR[face][nd], nb)).strip(), 0)]
                        else:                 steps = [(npr, dl), (spr, 0)]
                        trial(sc, t, sv, steps)
    run(sc, csvp)
    return keys, read(csvp)

def route3(chars, starters, mids, d1s, d2s, csvp):
    """3단 — 시동 기본기 → 특수기 → SP.

       유저 지적: 「88식은 **기본기 캔슬 후 발동일 때만** 이어서 필살기로 캔슬이 됐다」.
       특수기 단독에서 SP 를 누르면 안 되는 게 정상이다. 이 게임의 캔슬 계층은
       기본기 → 특수기 → 필살기이고, 앞 실측은 그 순서를 안 밟았다."""
    sc = ['1 -']; keys = []
    for (mode, cid, face) in chars:
        sv = ct(mode, cid, face)
        for sn_, sd, sb in starters:
            spr0 = press(sd, sb, face)
            for mn, md, mb in mids:
                mpr = press(md, mb, face)
                for d1 in d1s:
                    t = 'r0_%s_%d_%d_%s_%s_%d' % (mode, cid, face, sn_, mn, d1)
                    keys.append((t, mode, cid, face, sn_, mn, None, d1, None))
                    trial(sc, t, sv, [(spr0, d1), (mpr, 0)])      # 기준선 — SP 없이
                    for sp_n, sp_p in SPS:
                        for d2 in d2s:
                            t = 'r1_%s_%d_%d_%s_%s_%d_%s_%d' % (
                                mode, cid, face, sn_, mn, d1, sp_n, d2)
                            keys.append((t, mode, cid, face, sn_, mn, sp_n, d1, d2))
                            trial(sc, t, sv, [(spr0, d1), (mpr, d2), (sp_p, 0)])
    run(sc, csvp)
    return keys, read(csvp)

def tally(keys, data):
    base = {}; hit = collections.defaultdict(list)
    for k in keys:
        t, mode, cid, face, nn, sn, lay, dl = k
        if sn is None: base[(mode, cid, face, nn)] = peak(data.get(t, {}))
    for k in keys:
        t, mode, cid, face, nn, sn, lay, dl = k
        if sn is None: continue
        c, dm = peak(data.get(t, {}))
        b = base.get((mode, cid, face, nn), (0, 0))
        if c > b[0]: hit[(mode, cid, face, nn, sn, lay)].append((dl, c, dm))
    return base, hit

def tally3(keys, data):
    base = {}; hit = collections.defaultdict(list)
    for k in keys:
        t, mode, cid, face, sn_, mn, sp, d1, d2 = k
        if sp is None: base[(mode, cid, face, sn_, mn, d1)] = peak(data.get(t, {}))
    for k in keys:
        t, mode, cid, face, sn_, mn, sp, d1, d2 = k
        if sp is None: continue
        c, dm = peak(data.get(t, {}))
        b = base.get((mode, cid, face, sn_, mn, d1), (0, 0))
        if c > b[0]: hit[(mode, cid, face, sn_, mn, sp)].append((d1, d2, c, dm))
    return base, hit

if __name__ == '__main__':
    what = sys.argv[1] if len(sys.argv) > 1 else '쿄'
    have = cells()
    if not have:
        print('  접촉 세이브가 없다 — 먼저 `python3 tools/svc/mkspar.py 접촉`'); sys.exit(1)

    if what == '쿄':
        DL = [0, 2, 4, 6, 8, 12, 16, 20, 24]
        LAY = ['SP먼저', '동시', '노멀먼저']
        chars = [c for c in have if c[1] == 0]
        keys, data = grid(chars, DL, LAY, '/tmp/g_kyo.csv')
        base, hit = tally(keys, data)
        print('  쿄 — 캔슬 격자 %d회 (노멀 %d × 배치 %d × 지연 %d × 탭홀드 2 × %d칸)'
              % (len(keys), len(NORMALS), len(LAY), len(DL), len(chars)))
        for mode in DUMMY:
            print()
            print('  ── 상대 %s' % mode)
            print('  %-8s %-4s %-11s %s' % ('노멀', '방향', '단독', '캔슬이 붙은 배치·지연'))
            for nn, nd, nb in NORMALS:
                for face in (0, 1):
                    if (mode, 0, face) not in have: continue
                    b = base.get((mode, 0, face, nn), (0, 0))
                    got = []
                    for sn, _ in SPS:
                        for lay in LAY:
                            L = hit.get((mode, 0, face, nn, sn, lay))
                            if L: got.append('%s·%s %s' % (lay, sn, span([x[0] for x in L], DL)))
                    print('  %-8s %-4s %-11s %s'
                          % (nn, '→←'[face], '%d타/피해%d' % b, '  '.join(got) or '—'))
        print()
        good = sorted({(nn, sn, lay) for (m, c, f, nn, sn, lay) in hit if m == '정지'})
        print('  정지 상대에서 붙은 조합 %d종:' % len(good))
        for nn, sn, lay in good: print('    %-8s %-4s %s' % (nn, sn, lay))
        g2 = sorted({(nn, sn, lay) for (m, c, f, nn, sn, lay) in hit if m == '상단방어'})
        print('  가드 상대에서 붙은 조합 %d종%s' % (len(g2), ':' if g2 else ' (예상대로 0)'))
        for nn, sn, lay in g2: print('    %-8s %-4s %s' % (nn, sn, lay))

    elif what == '특수기':
        # 기본기 → 특수기 → SP. 유저 지적대로 특수기는 **캔슬로 나왔을 때만** 이어진다.
        ST = [('약펀', '', 'B'), ('강펀', '', 'Y'), ('앉강펀', 'D', 'Y'), ('앉강킥', 'D', 'X')]
        MD = [('88식', 'DF', 'A'), ('굉부양', 'F', 'A')]
        D1 = [2, 6, 10, 14, 18, 24]        # 시동 → 특수기 간격
        D2 = [0, 2, 4, 6, 8, 12, 16]       # 특수기 → SP 간격
        chars = [c for c in have if c[1] == 0]
        keys, data = route3(chars, ST, MD, D1, D2, '/tmp/r3_kyo.csv')
        base, hit = tally3(keys, data)
        print('  쿄 — 기본기 → 특수기 → SP  %d회' % len(keys))
        for mode in DUMMY:
            print()
            print('  ── 상대 %s' % mode)
            print('  %-8s %-8s %-4s %-14s %s'
                  % ('시동', '특수기', '방향', '시동+특수기', 'SP 까지 이어진 (시동간격,SP간격)'))
            for sn_, _a, _b in ST:
                for mn, _c, _d in MD:
                    for face in (0, 1):
                        if (mode, 0, face) not in have: continue
                        bs = [base.get((mode, 0, face, sn_, mn, d1), (0, 0)) for d1 in D1]
                        bmax = max(x[0] for x in bs) if bs else 0
                        got = []
                        for sp_n, _ in SPS:
                            L = hit.get((mode, 0, face, sn_, mn, sp_n))
                            if L:
                                best = max(L, key=lambda x: x[2])
                                got.append('%s %d타(%d,%d)' % (sp_n, best[2], best[0], best[1]))
                        print('  %-8s %-8s %-4s %-14s %s'
                              % (sn_, mn, '→←'[face], '최대 %d타' % bmax,
                                 '  '.join(got) or '—'))
