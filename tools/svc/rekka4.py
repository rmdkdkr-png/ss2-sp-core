#!/usr/bin/env python3
"""쿄 파생기 정밀 수색 — 황물기(탭)·독물기(홀드) 후속 입력 × 지연 그리드.
   무대: svc_f2 (밀착 62px, 수동 CPU). 판정: P카운터(0x0C7E) 2번째 리셋 / 추가 피해 / 뱅크 변화"""
import subprocess, os, csv, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

CORE=os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM =os.path.expanduser('~/ss2/rom/svc.ngc')
ST='svc_f2.st'

def run(script,csvn):
    open('t.txt','w').write('\n'.join(script)+'\n')
    env=dict(os.environ); env['PROBE_CSV']=csvn; env['SVCSP_NOGATE']='1'
    subprocess.run(['./svcrun',CORE,ROM,'t.txt'],capture_output=True,env=env,timeout=900)

Q236=['3 D','3 D R','3 R','3 R B']        # 황물기 탭
Q236H=['3 D','3 D R','3 R','16 R B']      # 독물기 홀드
HCB=['3 R','3 D R','3 D','3 D L','3 L','3 L B']   # 63214+P
Q236b=['3 D','3 D R','3 R','3 R B']

def trial(tag, first, delay, follow):
    sc=['!load %s'%ST,'10 -']+first+['%d -'%delay]+follow
    for i in range(40): sc+=['2 -','!w %s@%d'%(tag,i*2+2)]
    return sc

CASES=[]
# 황물기(탭) 후: P단독 / 236P / 6+P / hcb+P × 지연
for d in (2,6,10,14,18,22,26,30,36):
    CASES.append(('탭_P%d'%d,    Q236, d, ['3 B']))
    CASES.append(('탭_236P%d'%d, Q236, d, Q236b))
for d in (6,14,22,30):
    CASES.append(('탭_6P%d'%d,   Q236, d, ['3 R','3 R B']))
    CASES.append(('탭_hcb%d'%d,  Q236, d, HCB))
# 독물기(홀드) 후: hcb+P(죄읊기 후보) / P / 236P × 지연 (독물기는 길다 — 늦은 지연까지)
for d in (10,20,30,40,55,70):
    CASES.append(('홀_hcb%d'%d, Q236H, d, HCB))
    CASES.append(('홀_P%d'%d,   Q236H, d, ['3 B']))
print('시행 %d건'%len(CASES))

res={}
CH=12
for ci in range(0,len(CASES),CH):
    chunk=CASES[ci:ci+CH]
    sc=['1 -']
    for tag,f1,d,f2 in chunk: sc+=trial(tag,f1,d,f2)
    run(sc,'rk4.csv')
    rows={}
    for r in csv.DictReader(open('rk4.csv')):
        t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
    for tag,_,_,_ in chunk:
        d=rows.get(tag,{})
        ks=sorted(d)
        a=[int(d[s]['anim']) for s in ks]
        b=[int(d[s]['bank']) for s in ks]
        hp=[int(d[s]['hp2']) for s in ks]
        resets=sum(1 for i in range(1,len(a)) if a[i]<a[i-1])+(1 if a and a[0]<=6 else 0)
        dmg=48-min(hp) if hp else 0
        banks=sorted(set(x for x in b if x!=255))
        res[tag]=(resets,dmg,banks)

print()
print('%-12s %5s %4s %s'%('시행','리셋수','피해','뱅크'))
base=None
for tag,_,_,_ in CASES:
    r2,dmg,banks=res.get(tag,(0,0,[]))
    mark='★' if r2>=2 or dmg>=2 else ' '
    print('%s%-12s %5d %4d %s'%(mark,tag,r2,dmg,banks))
