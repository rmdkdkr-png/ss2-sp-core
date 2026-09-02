#!/usr/bin/env python3
"""2차: 던지기 안 맞는 밀착 만들기 → 독물기 피해·렛카 후속타 실측.
   접근 길이를 스캔해서 「접촉했고 아직 안 던져진」 창을 찾은 뒤 본실험."""
import subprocess, os, csv

CORE=os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM =os.path.expanduser('~/ss2/rom/svc.ngc')

def run(script,csvn):
    open('t.txt','w').write('\n'.join(script)+'\n')
    env=dict(os.environ); env['PROBE_CSV']=csvn
    subprocess.run(['./svcrun',CORE,ROM,'t.txt'],capture_output=True,env=env)

# ── 접근 스캔: N 프레임 걷고 멈춘 시점의 거리와, 이후 60f 동안 던져지는지 ──
sc=['1 -']
for n in (110,125,140,155):
    sc+=['!load svc_c0_0.st','30 -','%d R'%n,'!w 접근%d@0'%n,'60 -','!w 접근%d@60'%n]
run(sc,'rk2.csv')
rows={}
for r in csv.DictReader(open('rk2.csv')):
    t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
best=None
print('접근 스캔 (거리 = p2x-p1x, 던져짐 = 60f 뒤 y≠128 또는 크게 밀림):')
for n in (110,125,140,155):
    d=rows.get('접근%d'%n,{})
    if 0 not in d or 60 not in d: continue
    gap0=int(d[0]['p2x'])-int(d[0]['p1x'])
    y60=int(d[60]['p1y']); gap60=int(d[60]['p2x'])-int(d[60]['p1x'])
    thrown = y60!=128 or gap60>gap0+20
    print('  %3df: 거리 %d → 60f 뒤 y=%d 거리 %d %s'%(n,gap0,y60,gap60,'던져짐' if thrown else '안전'))
    if not thrown and gap0<=64 and best is None: best=n
print('선택한 접근: %s프레임'%best)
if best is None: best=140

SAM=(4,10,16,24,32,40,50,62,76,92,110,130)
def probe(tag, prelines):
    sc=['!load svc_c0_0.st','30 -']+prelines
    prev=0
    for s in SAM:
        sc+=['%d -'%(s-prev),'!w %s@%d'%(tag,s)]; prev=s
    return sc

Q=['3 D','3 D R','3 R']
W=['%d R'%best]
CASES=[
 ('밀_탭236P',    W+Q+['3 R B']),
 ('밀_홀드236P',   W+Q+['16 R B']),
 ('렛_탭탭P',      W+Q+['3 R B','12 -','3 B']),
 ('렛_탭P_25',    W+Q+['3 R B','22 -','3 B']),
 ('렛_탭236P236P',W+Q+['3 R B','8 -']+Q+['3 R B']),
 ('렛_홀드후P',    W+Q+['16 R B','8 -','3 B']),
 ('렛_3연타',      W+Q+['3 R B','12 -','3 B','12 -','3 B']),
]
sc=['1 -']
for tag,pre in CASES: sc+=probe(tag,pre)
run(sc,'rk3.csv')
rows={}
for r in csv.DictReader(open('rk3.csv')):
    t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
print()
print('%-14s %4s %5s  %s'%('시행','피해','접촉?','애니 흐름 (재리셋 = 후속타)'))
for tag,_ in CASES:
    d=rows.get(tag,{})
    a=[int(d[s]['anim']) for s in SAM if s in d]
    hp=48-min(int(d[s]['hp2']) for s in SAM if s in d)
    g=[int(d[s]['p2x'])-int(d[s]['p1x']) for s in SAM if s in d]
    resets=sum(1 for i in range(1,len(a)) if a[i]<a[i-1])
    print('%-14s %4d %5d  %s  (리셋 %d회)'%(tag,hp,g[0] if g else -1,','.join(map(str,a)),1+resets))
