#!/usr/bin/env python3
import subprocess, os, csv, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
CORE=os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM =os.path.expanduser('~/ss2/rom/svc.ngc')
def run(script,csvn):
    open('t.txt','w').write('\n'.join(script)+'\n')
    env=dict(os.environ); env['PROBE_CSV']=csvn; env['SVCSP_NOGATE']='1'
    subprocess.run(['./svcrun',CORE,ROM,'t.txt'],capture_output=True,env=env,timeout=900)
def trial(st, tag, seq):
    sc=['!load %s'%st,'20 -','140 R']+seq   # 접근 후
    for i in range(35): sc+=['2 -','!w %s@%d'%(tag,i*2+2)]
    return sc
R236=['4 D','4 D R','4 R','3 R B']
# 류=c3_0, 켄=c2_0
CASES=[
 ('svc_c3_0.st','류_앉강킥',     ['16 D A']),
 ('svc_c3_0.st','류_킥롤4f',    ['16 D A','4 -']+R236),
 ('svc_c3_0.st','류_킥롤8f',    ['16 D A','8 -']+R236),
 ('svc_c3_0.st','류_파동권만',   R236),
 ('svc_c2_0.st','켄_킥롤4f',    ['16 D A','4 -']+R236),
]
res={}
sc=['1 -']
for st,tag,seq in CASES: sc+=trial(st,tag,seq)
run(sc,'ry.csv')
rows={}
for r in csv.DictReader(open('ry.csv')):
    t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
print('%-12s %4s %s'%('시행','피해','뱅크'))
for st,tag,seq in CASES:
    d=rows.get(tag,{})
    ks=sorted(d)
    b=[int(d[s]['bank']) for s in ks]
    hp=48-min(int(d[s]['hp2']) for s in ks) if ks else 0
    print('%-12s %4d %s'%(tag,hp,sorted(set(x for x in b if x!=255))))
