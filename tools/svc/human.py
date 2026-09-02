#!/usr/bin/env python3
"""사람 손 패턴 캔슬 재현 — ↓유지 롤링, 노멀 도중 모션 시작, 약공격, 다양한 속도.
   성공 판정: 뱅크 22(진짜 황물기) vs 115(지정기 누에) vs 무반응 + 콤보 피해"""
import subprocess, os, csv, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

CORE=os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM =os.path.expanduser('~/ss2/rom/svc.ngc')
ST='svc_f2.st'

def run(script,csvn):
    open('t.txt','w').write('\n'.join(script)+'\n')
    env=dict(os.environ); env['PROBE_CSV']=csvn; env['SVCSP_NOGATE']='1'
    subprocess.run(['./svcrun',CORE,ROM,'t.txt'],capture_output=True,env=env,timeout=900)

def trial(tag, seq):
    sc=['!load %s'%ST,'10 -']+seq
    for i in range(35): sc+=['2 -','!w %s@%d'%(tag,i*2+2)]
    return sc

# g = 사람 속도 (4f/방향)
CASES=[
 # 기준
 ('기준_앉강킥',      ['16 D A']),
 ('기준_황물기',      ['4 D','4 D R','4 R','3 R B']),
 # H1: 앉은 채 강킥 → ↓ 유지 롤링 236P (사람 캔슬의 전형)
 ('앉강킥_롤4f',      ['16 D A','4 D','4 D R','4 R','3 R B']),
 ('앉강킥_롤6f',      ['16 D A','6 D','6 D R','6 R','4 R B']),
 # H2: 킥 누른 직후(회복 초입) 모션 시작 — 모션이 후딜을 관통
 ('앉강킥8f_롤',      ['8 D A','4 D','4 D R','4 R','3 R B']),
 # H3: 약공격 캔슬 (탭)
 ('약킥탭_롤',        ['3 A','4 D','4 D R','4 R','3 R B']),
 ('앉약킥_롤',        ['3 D A','4 D','4 D R','4 R','3 R B']),
 ('약펀탭_롤',        ['3 B','4 D','4 D R','4 R','3 R B']),
 # H4: 모션을 킥 유지 중에 겹치기 — D 잡은 채 K 유지하면서 굴리기
 ('겹침_K유지롤',     ['6 D A','4 D R A','4 R A','3 R B']),
 # H5: 늦은 롤 (회복 끝물)
 ('앉강킥_늦롤20',    ['16 D A','20 -','4 D','4 D R','4 R','3 R B']),
 ('앉강킥_늦롤36',    ['16 D A','36 -','4 D','4 D R','4 R','3 R B']),
]
res={}
CH=6
for ci in range(0,len(CASES),CH):
    chunk=CASES[ci:ci+CH]
    sc=['1 -']
    for tag,seq in chunk: sc+=trial(tag,seq)
    run(sc,'hx.csv')
    rows={}
    for r in csv.DictReader(open('hx.csv')):
        t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
    for tag,_ in chunk:
        d=rows.get(tag,{})
        ks=sorted(d)
        b=[int(d[s]['bank']) for s in ks]
        hp=48-min(int(d[s]['hp2']) for s in ks) if ks else 0
        res[tag]=(hp,sorted(set(x for x in b if x not in(255,23))))
print('%-16s %4s %s'%('시행','피해','뱅크(23=킥 제외)'))
for tag,_ in CASES:
    hp,banks=res.get(tag,(0,[]))
    what = '황물기!' if 22 in banks else ('누에(지정기)' if 115 in banks else '—')
    print('%-16s %4d %-14s %s'%(tag,hp,banks,what))
