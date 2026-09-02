#!/usr/bin/env python3
"""기본기 캔슬 실측 — 밀착 평타(히트) 직후 236P 를 프레임별로.
   판정: 총피해 (평타1 + 황물기1 = 2 이상이면 콤보), 뱅크 22 개시 시점.
   대조: 평타 단독 / 황물기 단독 / 늦은(회복 후) 236P"""
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

def trial(tag, seq):
    sc=['!load %s'%ST,'10 -']+seq
    for i in range(45): sc+=['2 -','!w %s@%d'%(tag,i*2+2)]
    return sc

CASES=[('평타단독',['3 B']),('황물기단독',Q)]
# 펀치 후 지연 d 프레임에 236P (d=0 은 펀치 떼자마자)
for d in (0,2,4,6,8,10,14,18,24,30):
    CASES.append(('펀치→236P_%d'%d, ['3 B']+((['%d -'%d]) if d else [])+Q))
# 킥 기본기 캔슬도 (킥 히트 후 236P)
for d in (2,6,10):
    CASES.append(('킥→236P_%d'%d, ['3 A','%d -'%d]+Q))

res={}
CH=8
for ci in range(0,len(CASES),CH):
    chunk=CASES[ci:ci+CH]
    sc=['1 -']
    for tag,seq in chunk: sc+=trial(tag,seq)
    run(sc,'cn.csv')
    rows={}
    for r in csv.DictReader(open('cn.csv')):
        t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
    for tag,_ in chunk:
        d2=rows.get(tag,{})
        ks=sorted(d2)
        b=[int(d2[s]['bank']) for s in ks]
        hp=48-min(int(d2[s]['hp2']) for s in ks) if ks else 0
        onset=next((ks[i] for i,x in enumerate(b) if x==22), -1)
        res[tag]=(hp,onset,sorted(set(x for x in b if x!=255)))
print('%-14s %4s %8s %s'%('시행','피해','22개시f','뱅크'))
for tag,_ in CASES:
    hp,onset,banks=res.get(tag,(0,-1,[]))
    mark='★' if hp>=2 else ' '
    print('%s%-14s %4d %8d %s'%(mark,tag,hp,onset,banks))
