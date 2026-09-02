#!/usr/bin/env python3
import subprocess, os, csv, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
CORE=os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM =os.path.expanduser('~/ss2/rom/svc.ngc')
ST='/tmp/user_state.st'
def run(script,csvn):
    open('t.txt','w').write('\n'.join(script)+'\n')
    env=dict(os.environ); env['PROBE_CSV']=csvn; env['SVCSP_NOGATE']='1'
    subprocess.run(['./svcrun',CORE,ROM,'t.txt'],capture_output=True,env=env,timeout=900)
def trial(tag, seq):
    sc=['!load %s'%ST,'20 -','100 R']+seq
    for i in range(40): sc+=['2 -','!w %s@%d'%(tag,i*2+2)]
    return sc
RT=['4 D','4 D R','4 R','3 R B']      # 탭
RH=['4 D','4 D R','4 R','16 R B']     # 홀드
CASES=[
 ('강펀_탭',   ['16 B','4 -']+RT),
 ('강펀_홀드', ['16 B','4 -']+RH),
 ('약펀_탭',   ['3 B','4 -']+RT),
 ('강킥_탭',   ['16 A','4 -']+RT),
 ('강킥_홀드', ['16 A','4 -']+RH),
 ('앉강킥_탭', ['16 D A','4 -']+RT),
]
res={}
sc=['1 -']
for tag,seq in CASES: sc+=trial(tag,seq)
run(sc,'pk.csv')
rows={}
for r in csv.DictReader(open('pk.csv')):
    t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
print('%-10s %4s %s'%('시행','피해','뱅크'))
for tag,_ in CASES:
    d=rows.get(tag,{})
    ks=sorted(d)
    b=[int(d[s]['bank']) for s in ks]
    hp=45-min(int(d[s]['hp2']) for s in ks) if ks else 0
    print('%-10s %4d %s'%(tag,hp,sorted(set(x for x in b if x!=255))))
