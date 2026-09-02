#!/usr/bin/env python3
"""3차: svc_f2 (수동 CPU) 에서 탭/홀드 피해 + 렛카 후속타"""
import subprocess, os, csv

CORE=os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM =os.path.expanduser('~/ss2/rom/svc.ngc')
ST='svc_f2.st'

def run(script,csvn):
    open('t.txt','w').write('\n'.join(script)+'\n')
    env=dict(os.environ); env['PROBE_CSV']=csvn
    subprocess.run(['./svcrun',CORE,ROM,'t.txt'],capture_output=True,env=env)

# 무대 파악: 초기 간격, 수동성, 걷기 속도
sc=['1 -','!load %s'%ST,'10 -','!w f0@0','120 -','!w f0@120',
    '!load %s'%ST,'10 -','60 R','!w f1@0']
run(sc,'rf.csv')
rows={}
for r in csv.DictReader(open('rf.csv')):
    t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
g0=int(rows['f0'][0]['p2x'])-int(rows['f0'][0]['p1x'])
g120=int(rows['f0'][120]['p2x'])-int(rows['f0'][120]['p1x'])
gw=int(rows['f1'][0]['p2x'])-int(rows['f1'][0]['p1x'])
hp0=int(rows['f0'][0]['hp2'])
speed=(g0-gw)/60.0
print('f2 무대: 간격 %d, 120f 방치 후 %d (%s), 걷기 60f 후 %d (속도 %.2fpx/f), P2체력 %d'%(
    g0,g120,'수동' if abs(g120-g0)<10 else '움직임',gw,speed,hp0))
need=int(max(0,(g0-62)/max(speed,0.1)))
print('접촉(62px)까지 필요 걷기 ≈ %d프레임'%need)

SAM=(4,10,16,24,32,40,50,62,76,92,110,130,150)
def probe(tag, prelines):
    sc=['!load %s'%ST,'10 -']+prelines
    prev=0
    for s in SAM:
        sc+=['%d -'%(s-prev),'!w %s@%d'%(tag,s)]; prev=s
    return sc

Q=['3 D','3 D R','3 R']
W=['%d R'%need] if need>0 else []
CASES=[
 ('접촉확인',   W+['1 -']),
 ('탭236P',    W+Q+['3 R B']),
 ('홀드236P',   W+Q+['16 R B']),
 ('렛_탭P12',  W+Q+['3 R B','12 -','3 B']),
 ('렛_탭P20',  W+Q+['3 R B','20 -','3 B']),
 ('렛_탭P30',  W+Q+['3 R B','30 -','3 B']),
 ('렛_탭236x2',W+Q+['3 R B','8 -']+Q+['3 R B']),
 ('렛_홀드P',   W+Q+['16 R B','10 -','3 B']),
 ('평타2회',    W+['3 B','16 -','3 B']),
]
sc=['1 -']
for tag,pre in CASES: sc+=probe(tag,pre)
run(sc,'rf2.csv')
rows={}
for r in csv.DictReader(open('rf2.csv')):
    t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
print()
print('%-12s %4s %4s  %s'%('시행','피해','간격','애니 (리셋횟수)'))
for tag,_ in CASES:
    d=rows.get(tag,{})
    a=[int(d[s]['anim']) for s in SAM if s in d]
    hp=hp0-min(int(d[s]['hp2']) for s in SAM if s in d)
    g=[int(d[s]['p2x'])-int(d[s]['p1x']) for s in SAM if s in d]
    resets=1+sum(1 for i in range(1,len(a)) if a[i]<a[i-1])
    print('%-12s %4d %4d  %s (%d)'%(tag,hp,g[0] if g else -1,','.join(map(str,a)),resets))
