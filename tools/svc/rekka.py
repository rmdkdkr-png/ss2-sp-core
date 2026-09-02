#!/usr/bin/env python3
"""원판 실측: ① 버튼 홀드 = 강(독물기)인가  ② 황물기 렛카 후속타가 있는가.
   svcsp 를 거치지 않고 방향을 직접 주입한다 (밀착 = 170R 선행)."""
import subprocess, os, csv

CORE=os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM =os.path.expanduser('~/ss2/rom/svc.ngc')

def run(script,csvn):
    open('t.txt','w').write('\n'.join(script)+'\n')
    env=dict(os.environ); env['PROBE_CSV']=csvn
    subprocess.run(['./svcrun',CORE,ROM,'t.txt'],capture_output=True,env=env)

SAM=(4,10,16,24,32,40,50,62,76,92)
def probe(tag, prelines):
    sc=['!load svc_c0_0.st','30 -']+prelines
    prev=0
    for s in SAM:
        sc+=['%d -'%(s-prev),'!w %s@%d'%(tag,s)]; prev=s
    return sc

Q=['3 D','3 D R','3 R']              # 236, 마지막 → 유지 규격
CASES=[
 # ① 홀드 강약 — 원거리 (이동거리·모션 차이 보려고)
 ('원_탭236P',   Q+['3 R B']),
 ('원_홀드236P',  Q+['16 R B']),
 ('원_긴홀드236P',Q+['30 R B']),
 # ① 홀드 강약 — 밀착 (피해 차이)
 ('밀_탭236P',   ['170 R']+Q+['3 R B']),
 ('밀_홀드236P',  ['170 R']+Q+['16 R B']),
 # ② 렛카 — 황물기 (밀착 히트) 후 재입력
 ('렛_P15',     ['170 R']+Q+['3 R B','12 -','3 B']),
 ('렛_P25',     ['170 R']+Q+['3 R B','22 -','3 B']),
 ('렛_P35',     ['170 R']+Q+['3 R B','32 -','3 B']),
 ('렛_236P20',  ['170 R']+Q+['3 R B','8 -']+Q+['3 R B']),
 ('렛_3연타',    ['170 R']+Q+['3 R B','12 -','3 B','12 -','3 B']),
 # 대조: 밀착 평타 두 방 (렛카가 아니라 그냥 평타 연타인지 구분용)
 ('대_평타2',    ['170 R','3 B','12 -','3 B']),
]
sc=['1 -']
for tag,pre in CASES: sc+=probe(tag,pre)
run(sc,'rk.csv')

rows={}
for r in csv.DictReader(open('rk.csv')):
    t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
print('%-12s %-34s %4s %5s  %s'%('시행','뱅크 흐름','피해','이동','애니 흐름'))
for tag,_ in CASES:
    d=rows.get(tag,{})
    b=[int(d[s]['bank']) for s in SAM if s in d]
    a=[int(d[s]['anim']) for s in SAM if s in d]
    hp=48-min(int(d[s]['hp2']) for s in SAM if s in d)
    xs=[int(d[s]['p1x']) for s in SAM if s in d]
    dx=xs[-1]-xs[0] if xs else 0
    print('%-12s %-34s %4d %5d  %s'%(tag,','.join(map(str,b)),hp,dx,','.join(map(str,a))))
