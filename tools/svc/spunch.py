#!/usr/bin/env python3
import subprocess, os, csv, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
CORE=os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM =os.path.expanduser('~/ss2/rom/svc.ngc')
def run(script,csvn,dbg=False):
    open('t.txt','w').write('\n'.join(script)+'\n')
    env=dict(os.environ); env['PROBE_CSV']=csvn
    if not dbg: env['SVCSP_NOGATE']='1'
    r=subprocess.run(['./svcrun',CORE,ROM,'t.txt'],capture_output=True,env=env,text=True)
    return r.stderr
def trial(st, tag, seq, app):
    sc=['!load %s'%st,'20 -']
    if app: sc.append('%d R'%app)
    sc+=seq
    for i in range(45): sc+=['2 -','!w %s@%d'%(tag,i*2+2)]
    return sc
ROLLH=['4 D','4 D R','4 R','16 R B']    # 사람 롤 + 홀드(강) 버튼
ST='/tmp/user_state.st'
# 접근량 스캔: 강펀치가 맞는 거리 (스파링 류 — 가드 안 함)
CASES=[('강펀%d'%a, ['16 B'], a) for a in (100,120,140,155)]
res={}
sc=['1 -']
for tag,seq,app in CASES: sc+=trial(ST,tag,seq,app)
run(sc,'pp.csv')
rows={}
for r in csv.DictReader(open('pp.csv')):
    t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
best=None
for tag,seq,app in CASES:
    d=rows.get(tag,{})
    hp=45-min(int(d[s]['hp2']) for s in sorted(d)) if d else 0
    print('%s: 피해 %d'%(tag,hp))
    if hp>0 and best is None: best=app
print('강펀 히트 접근 =',best)
if best is None: raise SystemExit('강펀이 안 맞음 — 다른 무대 필요')
# 본실험
C2=[
 ('A_강펀단독',    ['16 B'], best),
 ('B_강펀_롤홀드',  ['16 B','4 -']+ROLLH, best),          # 사람 손: 캔슬창에 236+P홀드
 ('C_독물기단독',   ['4 D','4 D R','4 R','16 R B'], best),
]
sc=['1 -']
for tag,seq,app in C2: sc+=trial(ST,tag,seq,app)
run(sc,'pp2.csv')
rows={}
for r in csv.DictReader(open('pp2.csv')):
    t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
for tag,seq,app in C2:
    d=rows.get(tag,{})
    ks=sorted(d)
    b=[int(d[s]['bank']) for s in ks]
    hp=45-min(int(d[s]['hp2']) for s in ks) if ks else 0
    print('%-14s 피해 %d 뱅크 %s'%(tag,hp,sorted(set(x for x in b if x!=255))))
