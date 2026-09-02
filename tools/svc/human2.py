#!/usr/bin/env python3
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
R236=['4 D','4 D R','4 R','3 R B']
CASES=[
 ('기준_88식',      ['3 D R A']),
 ('기준_굉부양',    ['3 R A']),
 ('88식_롤즉시',    ['3 D R A']+R236),
 ('88식_롤8f',     ['3 D R A','8 -']+R236),
 ('88식_롤16f',    ['3 D R A','16 -']+R236),
 ('굉부양_롤즉시',  ['3 R A']+R236),
 ('굉부양_롤12f',  ['3 R A','12 -']+R236),
 ('굉부양_롤24f',  ['3 R A','24 -']+R236),
]
res={}
for ci in range(0,len(CASES),6):
    chunk=CASES[ci:ci+6]
    sc=['1 -']
    for tag,seq in chunk: sc+=trial(tag,seq)
    run(sc,'h2.csv')
    rows={}
    for r in csv.DictReader(open('h2.csv')):
        t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
    for tag,_ in chunk:
        d=rows.get(tag,{})
        ks=sorted(d)
        b=[int(d[s]['bank']) for s in ks]
        hp=48-min(int(d[s]['hp2']) for s in ks) if ks else 0
        res[tag]=(hp,sorted(set(x for x in b if x!=255)))
print('%-14s %4s %s'%('시행','피해','뱅크'))
for tag,_ in CASES:
    hp,banks=res.get(tag,(0,[]))
    what='황물기!' if 22 in banks else ('누에' if 115 in banks else '')
    print('%-14s %4d %-16s %s'%(tag,hp,banks,what))
