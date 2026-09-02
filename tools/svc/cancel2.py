#!/usr/bin/env python3
"""캔슬 2차 — 먼저 평타가 '맞는' 전진량을 찾고, 그 조건에서 캔슬 그리드"""
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

def trial(tag, seq, n=45):
    sc=['!load %s'%ST,'10 -']+seq
    for i in range(n): sc+=['2 -','!w %s@%d'%(tag,i*2+2)]
    return sc

# 1) 평타가 맞는 전진량
sc=['1 -']
for w in (4,8,12,16,20,26):
    sc+=trial('걷%d펀치'%w, ['%d R'%w,'3 B'], 30)
run(sc,'c2.csv')
rows={}
for r in csv.DictReader(open('c2.csv')):
    t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
best=None
print('평타 히트 전진량:')
for w in (4,8,12,16,20,26):
    d=rows.get('걷%d펀치'%w,{})
    hp=48-min(int(d[s]['hp2']) for s in sorted(d)) if d else 0
    print('  %2df 전진: 평타 피해 %d'%(w,hp))
    if hp>0 and best is None: best=w
if best is None:
    print('평타가 안 맞음 — 킥으로 재시도')
    sc=['1 -']
    for w in (4,8,12,16,20,26):
        sc+=trial('걷%d킥'%w, ['%d R'%w,'3 A'], 30)
    run(sc,'c2b.csv')
    rows={}
    for r in csv.DictReader(open('c2b.csv')):
        t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
    for w in (4,8,12,16,20,26):
        d=rows.get('걷%d킥'%w,{})
        hp=48-min(int(d[s]['hp2']) for s in sorted(d)) if d else 0
        print('  %2df 전진: 킥 피해 %d'%(w,hp))
    raise SystemExit
W=['%d R'%best]
print('선택 전진: %df'%best)

# 2) 히트 캔슬 그리드
CASES=[('평타히트단독', W+['3 B']), ('황물기단독', W+Q)]
for d in (0,2,4,6,8,10,12,16,20,26):
    CASES.append(('히트→236P_%d'%d, W+['3 B']+((['%d -'%d]) if d else [])+Q))
res={}
CH=8
for ci in range(0,len(CASES),CH):
    chunk=CASES[ci:ci+CH]
    sc=['1 -']
    for tag,seq in chunk: sc+=trial(tag,seq)
    run(sc,'c3.csv')
    rows={}
    for r in csv.DictReader(open('c3.csv')):
        t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
    for tag,_ in chunk:
        d2=rows.get(tag,{})
        ks=sorted(d2)
        b=[int(d2[s]['bank']) for s in ks]
        hp=48-min(int(d2[s]['hp2']) for s in ks) if ks else 0
        onset=next((ks[i] for i,x in enumerate(b) if x==22), -1)
        res[tag]=(hp,onset,sorted(set(x for x in b if x!=255)))
base=res.get('평타히트단독',(0,-1,[]))[0]
print()
print('평타 단독 피해=%d — 이보다 크면 콤보'%base)
print('%-16s %4s %8s %s'%('시행','피해','22개시f','뱅크'))
for tag,_ in CASES:
    hp,onset,banks=res.get(tag,(0,-1,[]))
    mark='★' if hp>base and tag.startswith('히트') else ' '
    print('%s%-16s %4d %8d %s'%(mark,tag,hp,onset,banks))
