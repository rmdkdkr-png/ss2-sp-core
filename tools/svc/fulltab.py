#!/usr/bin/env python3
"""쿄 노멀 8종 × 캔슬 정체 전수 (스파링 류) — ① 원생 롤 236P ② 엔진 X 경로"""
import subprocess, os, csv, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
CORE=os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM =os.path.expanduser('~/ss2/rom/svc.ngc')
ST='/tmp/user_state.st'
def run(script,csvn,nogate):
    open('t.txt','w').write('\n'.join(script)+'\n')
    env=dict(os.environ); env['PROBE_CSV']=csvn
    if nogate: env['SVCSP_NOGATE']='1'
    subprocess.run(['./svcrun',CORE,ROM,'t.txt'],capture_output=True,env=env,timeout=900)
def trial(tag, seq):
    sc=['!load %s'%ST,'20 -','100 R']+seq
    for i in range(40): sc+=['2 -','!w %s@%d'%(tag,i*2+2)]
    return sc
ROLL=['4 D','4 D R','4 R','3 R B']
NORM={
 '서서약펀':['3 B'],   '서서강펀':['16 B'],
 '앉아약펀':['3 D B'], '앉아강펀':['16 D B'],
 '서서약킥':['3 A'],   '서서강킥':['16 A'],
 '앉아약킥':['3 D A'], '앉아강킥':['16 D A'],
}
def batch(cases, csvn, nogate):
    res={}
    ks2=list(cases.items())
    for ci in range(0,len(ks2),5):
        chunk=ks2[ci:ci+5]
        sc=['1 -']
        for tag,seq in chunk: sc+=trial(tag,seq)
        run(sc,csvn,nogate)
        rows={}
        for r in csv.DictReader(open(csvn)):
            t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
        for tag,_ in chunk:
            d=rows.get(tag,{})
            ks=sorted(d)
            b=[int(d[s]['bank']) for s in ks]
            hp=45-min(int(d[s]['hp2']) for s in ks) if ks else 0
            res[tag]=(hp,sorted(set(x for x in b if x!=255)))
    return res
# ① 노멀 단독 + 원생 롤
solo=batch({k:v for k,v in NORM.items()}, 'ft1.csv', True)
roll=batch({k+'→롤': v+['4 -']+ROLL for k,v in NORM.items()}, 'ft2.csv', True)
# ② 엔진 X 경로
eng=batch({k+'→X': v+['2 -','1 X'] for k,v in NORM.items()}, 'ft3.csv', False)
def ident(banks):
    if 115 in banks: return '누에(꽝)'
    if 99 in banks: return '물기99'
    if 81 in banks: return '81'
    if 22 in banks: return '황물기22'
    return '-'
print('%-8s %6s | %6s %-10s | %6s %-10s'%('노멀','단독딜','롤 딜','롤 정체','X 딜','X 정체'))
for k in NORM:
    s=solo[k]; r=roll[k+'→롤']; e=eng[k+'→X']
    print('%-8s %6d | %6d %-10s | %6d %-10s  %s'%(k,s[0],r[0],ident(r[1]),e[0],ident(e[1]),
          '←X누에!' if 115 in e[1] else ''))
