#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SP 키 대량 검수 — 18캐릭터 × 7슬롯 × 탭/홀드 × 여러 번.

   기존 chartest 는 18×3 = 54번만 본다. 그 정도로는 「가끔 안 나가는」 것을 못 잡는다.
   여기서는 천 번 넘게 눌러 **어느 캐릭터의 어느 슬롯이 얼마나 자주 실패하는지**를 센다.

   판정은 세 가지를 함께 본다 — 하나만 보면 놓친다:
     P 카운터(0x0C7E)  펀치 계열이 시작되면 리셋된다
     K 카운터(0x0C7F)  **킥 계열은 P 카운터가 안 움직인다** (메모 §14 맹점)
     뱅크(0x09AD)      기술별 값. 255=대기, 공중은 4·5 도 대기 취급

   대조군을 같이 돌린다(SVCSP_FORCE=0). 엔진을 꺼도 같은 수치가 나오면 그 지표는 죽은 것이다.

   쓰기: python3 tools/svc/spstress.py [반복수]
"""
import collections, csv, io, os, re, subprocess, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

HERE = os.path.dirname(os.path.abspath(__file__))
CORE = os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM  = os.path.expanduser('~/ss2/rom/svc.ngc')
RUN  = os.path.expanduser('~/ss2/repo/ss2-main/tools/svc/svcrun')
NAME = {0:'쿄',1:'테리',2:'료',3:'마이',4:'레오나',5:'아테나',6:'이오리',7:'하오마루',
        8:'나코루루',9:'류',10:'춘리',11:'장기에프',12:'켄',13:'단',14:'사쿠라',
        15:'모리간',16:'펠리시아',17:'가일'}

# 슬롯 → 방향 입력. AIR 은 점프해서 정점 부근에 누른다.
SLOTS = [
 ('N',  ['']),
 ('F',  ['2 R']),
 ('B',  ['2 L']),
 ('D',  ['2 D']),
 ('DF', ['2 D R']),
 ('DB', ['2 D L']),
 ('AIR',['2 U', '16 -']),
]
SAM = (4, 10, 16, 24, 36, 50, 64)

def saves():
    st = {}
    p = os.path.join(HERE, 'chars.txt')
    for line in io.open(p, encoding='utf-8'):
        f = line.split()
        if len(f) == 2:
            q = os.path.join(HERE, 'svc_%s.st' % f[1])
            if not os.path.exists(q):
                q = os.path.expanduser('~/ss2/saves/svc/svc_%s.st' % f[1])
            if os.path.exists(q): st[int(f[0])] = q
    return st

def build(st, reps):
    sc = ['1 -']
    n = 0
    for rep in range(reps):
        hold = (rep % 2 == 1)          # 짝수 반복은 탭, 홀수는 홀드(강 갈래)
        for cid in sorted(st):
            for sl, pre in SLOTS:
                tag = 'r%dc%d%s%s' % (rep, cid, sl, 'H' if hold else 'T')
                sc.append('!load %s' % st[cid])
                sc.append('30 -')
                for p in pre:
                    if p: sc.append(p)
                d = pre[0].split(' ', 1)[1] if pre and pre[0] and not pre[0].endswith('-') else ''
                sc.append('%d %s' % (26 if hold else 2, ('%s R1' % d).strip()))
                prev = 0
                for s in SAM:
                    sc.append('%d -' % (s - prev)); sc.append('!w %s@%d' % (tag, s)); prev = s
                n += 1
    return sc, n

def run(sc, csvp, force, dbg=False):
    open('/tmp/sps.txt', 'w').write('\n'.join(sc) + '\n')
    env = dict(os.environ); env['PROBE_CSV'] = csvp; env['SVCSP_FORCE'] = force
    if dbg: env['SVCSP_DEBUG'] = '1'
    r = subprocess.run([RUN, CORE, ROM, '/tmp/sps.txt'], capture_output=True, env=env,
                       cwd=os.path.dirname(RUN))
    return r.stderr.decode('utf-8', 'replace')

def read(csvp):
    d = {}
    for r in csv.DictReader(open(csvp)):
        t, at = r['tag'].rsplit('@', 1)
        d.setdefault(t, {})[int(at)] = r
    return d

def sig(rows):
    """엔진이 그 시행에서 무엇을 바꿨는지 보려면 **결과 전체**를 비교해야 한다.

       뱅크만 추려서는 못 본다. 두 번 헛짚었다:
         · 「뭔가 났다」  — 엔진을 꺼도 R 이 A+B(기본기)로 나가서 100% 가 나온다.
         · 뱅크 집합     — 공중 동작은 점프와 **같은 뱅크(4·5)** 를 쓴다. 대기로 걸러 버리면
                          다 나가고 있는데도 「동작 없음」으로 찍힌다.
                          차이는 뱅크가 아니라 **궤적**에 있다
                          (레오나 X 칼리버: y 79→66→52 로 되올라간다).

       에뮬레이션은 같은 세이브·같은 대본이면 완전히 재현되므로, 표본 전체를 그대로
       비교해도 된다. 엔진 켬/끔이 **한 값이라도 다르면** 엔진이 뭔가를 한 것이다."""
    return tuple((rows[s]['bank'], rows[s]['anim'], rows[s]['kanim'], rows[s]['p1y'])
                 for s in SAM if s in rows)

def slots():
    """실패한 자리가 「표가 비었다」인지 「배정됐는데 안 나간다」인지 갈라 준다.
       둘은 고치는 법이 다르다 — 앞은 표를 채우는 일, 뒤는 엔진을 고치는 일이다."""
    import re
    p = os.path.expanduser('~/ss2/repo/ss2-sp-core/src/svcsp_moves.h')
    if not os.path.exists(p): return {}
    t = io.open(p, encoding='utf-8').read()
    SL = ['N','F','B','D','DF','DB','AIR']
    out = {}
    for cid, _n, _nm, sl in re.findall(
            r'\{\s*mv_c(\d+),\s*(\d+),\s*"([^"]*)",\s*\d+,\s*\{([^}]*)\}\s*\}', t):
        cid = int(cid); idxs = [int(x) for x in sl.split(',')]
        body = re.search(r'static const svc_move mv_c%d\[\] = \{(.*?)\n\};' % cid, t, re.S)
        mv = re.findall(r'\{"([^"]+)",[^,]+,\s*\d+,\s*(?:0x[0-9A-Fa-f]+),\s*(\d+)',
                        body.group(1)) if body else []
        for i, s in enumerate(SL):
            k = idxs[i] if i < len(idxs) else -1
            if k < 0:            out[(cid, s)] = ('— 빈 슬롯', '표를 채워야 한다')
            elif k < len(mv):
                fl = int(mv[k][1])
                why = '모으기 기술 — 선입력이 필요하다' if fl & 16 else ''
                out[(cid, s)] = (mv[k][0], why)
    return out

if __name__ == '__main__':
    reps = int(sys.argv[1]) if len(sys.argv) > 1 else 8
    st = saves()
    sc, n = build(st, reps)
    print('  캐릭터 %d명 · 슬롯 %d · 반복 %d → **%d회**' % (len(st), len(SLOTS), reps, n))
    err = run(sc, '/tmp/sps_on.csv', '1', dbg=True)
    run(sc, '/tmp/sps_off.csv', '0')
    on, off = read('/tmp/sps_on.csv'), read('/tmp/sps_off.csv')

    """판정 — 엔진의 자기 기록을 쓴다.

       화면 값만 봐서는 못 가른다. 두 번 헛짚었다: 뱅크만 보면 엔진을 꺼도 100%(R 이
       A+B 로 나가므로), 표본 전체를 비교하면 한 값만 달라도 100% 다. 둘 다 「배정된
       기술이 나갔나」에 답하지 못한다.

       엔진은 이미 답을 알고 있다. SVCSP_DEBUG 의 두 줄이면 된다:
         edge ... -> <이름>   그 입력에 **무엇을 고를지** 정한 자리. 「펀치」면 아무것도
                             못 고른 것이다(svc_basic — 표에 없거나 조건을 못 맞췄다).
         compile <이름>        고른 것을 **실제로 쏜** 자리.
       시행 순서가 대본과 같으므로 edge 줄을 차례로 짝지으면 시행마다 귀속된다."""
    edges, comp = [], []
    for l in err.split('\n'):
        if '[svcsp] edge' in l:
            m = re.search(r'chr=(\d+).*-> (.+)$', l)
            if m: edges.append((int(m.group(1)), m.group(2).strip()))
        elif '[svcsp] compile' in l:
            m = re.search(r'compile (.+?) steps=', l)
            if m: comp.append((len(edges), m.group(1).strip()))   # 직전 edge 번호에 붙인다

    order = [(cid, sl) for _ in range(reps) for cid in sorted(st) for sl, _p in SLOTS]
    if len(edges) != n or any(e[0] != c for e, (c, _s) in zip(edges, order)):
        print('  ** 로그 %d줄 / 시행 %d회 — 순서가 안 맞아 귀속이 불가하다' % (len(edges), n))
        sys.exit(1)
    ran = collections.Counter(i for i, _nm in comp)   # edge 번호별 compile 횟수

    slot = slots()
    tot = collections.Counter(); bad = collections.Counter()
    picked = {}
    for i, ((cid, sl), (_c, nm)) in enumerate(zip(order, edges)):
        tot[(cid, sl)] += 1
        picked.setdefault((cid, sl), nm)
        if nm == '펀치' or not ran[i + 1]:      # 못 골랐거나, 골라 놓고 못 쐈거나
            bad[(cid, sl)] += 1

    nb = sum(bad.values())
    print('  시행 %d — 배정된 기술이 나간 것 %d (%.1f%%) · 못 나간 것 %d'
          % (n, n - nb, 100.0 * (n - nb) / n, nb))
    print()
    if not bad:
        print('  모든 캐릭터·슬롯이 매번 제 기술을 냈다')
    else:
        print('  못 나간 자리 (실패/시도)')
        for k in sorted(bad, key=lambda k: (-bad[k], k)):
            nm, why = slot.get(k, ('?', ''))
            print('    %-8s %-4s  %2d/%-2d  %-30s %s'
                  % (NAME.get(k[0], k[0]), k[1], bad[k], tot[k], nm, why))
