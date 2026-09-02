#!/usr/bin/env python3
"""캔슬 3차 — 어떤 기본기가 맞는가 (피해 + P2 밀림 판정) → 맞는 놈으로 캔슬 그리드"""
import subprocess, os, csv, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

CORE=os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM =os.path.expanduser('~/ss2/rom/svc.ngc')
ST='svc_f2.st'
Q=['3 D','3 D R','3 R','3 R B']

def run(script,csvn):
    open('t.txt','w').write('\n'.join(script)+'\n')
    env=dict(os.environ); env['PROBE_CSV']=csvn; env['SVCSP_NOGATE']='1'
    subprocess.run(['./svcrun',CORE,ROM,'t.txt'],capture_output=True,env=env,timeout=900)

def trial(tag, seq, n=40):
    sc=['!load %s'%ST,'10 -']+seq
    for i in range(n): sc+=['2 -','!w %s@%d'%(tag,i*2+2)]
    return sc

ATK=[
 ('약펀',      ['3 B']), ('강펀(홀드)', ['16 B']),
 ('약킥',      ['3 A']), ('강킥(홀드)', ['16 A']),
 ('앉아약펀',   ['3 D B']), ('앉아약킥', ['3 D A']),
 ('앉아강펀',   ['16 D B']), ('앉아강킥', ['16 D A']),
 ('6K굉부양',  ['3 R A']), ('3K88식', ['3 D R A']),
 ('전진약펀',   ['14 R','3 B']), ('전진강펀', ['14 R','16 B']),
]
res={}
CH=8
for ci in range(0,len(ATK),CH):
    chunk=ATK[ci:ci+CH]
    sc=['1 -']
    for tag,seq in chunk: sc+=trial(tag,seq)
    run(sc,'c4.csv')
    rows={}
    for r in csv.DictReader(open('c4.csv')):
        t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
    for tag,_ in chunk:
        d2=rows.get(tag,{})
        ks=sorted(d2)
        hp=48-min(int(d2[s]['hp2']) for s in ks) if ks else 0
        p2=[int(d2[s]['p2x']) for s in ks]
        push=max(p2)-p2[0] if p2 else 0
        res[tag]=(hp,push)
print('%-12s %4s %6s'%('공격','피해','P2밀림'))
hit=None
for tag,seq in ATK:
    hp,push=res.get(tag,(0,0))
    mark='★' if hp>0 or push>=3 else ' '
    print('%s%-12s %4d %6d'%(mark,tag,hp,push))
    if (hp>0) and hit is None: hit=(tag,seq,hp)
print()
if not hit:
    print('피해 나는 기본기 없음 — 밀림만 있는 놈이라도 채택')
    for tag,seq in ATK:
        hp,push=res.get(tag,(0,0))
        if push>=3: hit=(tag,seq,hp); break
if not hit: raise SystemExit('전부 헛침')
tag0,seq0,hp0=hit
print('채택: %s (피해 %d) → 캔슬 그리드'%(tag0,hp0))

CASES=[('단독',seq0)]
for d in (0,2,4,6,8,12,16,22):
    CASES.append(('→236P_%d'%d, seq0+((['%d -'%d]) if d else [])+Q))
res2={}
for ci in range(0,len(CASES),CH):
    chunk=CASES[ci:ci+CH]
    sc=['1 -']
    for t2,seq in chunk: sc+=trial(t2,seq,45)
    run(sc,'c5.csv')
    rows={}
    for r in csv.DictReader(open('c5.csv')):
        t2,at=r['tag'].rsplit('@',1); rows.setdefault(t2,{})[int(at)]=r
    for t2,_ in chunk:
        d2=rows.get(t2,{})
        ks=sorted(d2)
        hp=48-min(int(d2[s]['hp2']) for s in ks) if ks else 0
        b=[int(d2[s]['bank']) for s in ks]
        res2[t2]=(hp,sorted(set(x for x in b if x!=255)))
base=res2['단독'][0]
print('%-12s %4s %s  (단독=%d)'%('시행','피해','뱅크',base))
for t2,_ in CASES:
    hp,banks=res2.get(t2,(0,[]))
    mark='★' if hp>base else ' '
    print('%s%-12s %4d %s'%(mark,t2,hp,banks))
