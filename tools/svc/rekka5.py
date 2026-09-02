#!/usr/bin/env python3
"""파생 2라운드 — 3단 체인·벌읊기·K 마무리"""
import subprocess, os, csv, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

CORE=os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM =os.path.expanduser('~/ss2/rom/svc.ngc')
ST='svc_f2.st'

def run(script,csvn):
    open('t.txt','w').write('\n'.join(script)+'\n')
    env=dict(os.environ); env['PROBE_CSV']=csvn; env['SVCSP_NOGATE']='1'
    subprocess.run(['./svcrun',CORE,ROM,'t.txt'],capture_output=True,env=env,timeout=900)

T=['3 D','3 D R','3 R','3 R B']            # 236P 탭
H=['3 D','3 D R','3 R','16 R B']           # 독물기 홀드
HCB=['3 R','3 D R','3 D','3 D L','3 L','3 L B']
FP=['3 R','3 R B']                          # 6+P
K=['3 A']                                   # K 단독
Q236K=['3 D','3 D R','3 R','3 R A']         # 236K

def trial(tag, seq):
    sc=['!load %s'%ST,'10 -']+seq
    for i in range(45): sc+=['2 -','!w %s@%d'%(tag,i*2+2)]
    return sc

CASES=[
 # 기준
 ('기준_황',        T),
 ('기준_독',        H),
 ('기준_황구상',     T+['10 -']+T),
 ('기준_독죄',      H+['30 -']+HCB),
 # 3단: 황물기→구상→?
 ('3단_236P',      T+['10 -']+T+['10 -']+T),
 ('3단_236P_20',   T+['10 -']+T+['20 -']+T),
 ('3단_hcb',       T+['10 -']+T+['12 -']+HCB),
 ('3단_6P',        T+['10 -']+T+['12 -']+FP),
 ('3단_P',         T+['10 -']+T+['12 -',['3 B'][0]]),
 ('3단_K',         T+['10 -']+T+['12 -']+K),
 # 벌읊기: 독물기→죄읊기→6P / P / K
 ('벌_6P10',       H+['30 -']+HCB+['10 -']+FP),
 ('벌_6P20',       H+['30 -']+HCB+['20 -']+FP),
 ('벌_6P30',       H+['30 -']+HCB+['30 -']+FP),
 ('벌_P20',        H+['30 -']+HCB+['20 -',['3 B'][0]]),
 ('벌_K20',        H+['30 -']+HCB+['20 -']+K),
 # 75식 개: 236K 후 K 재입력
 ('개_기준',        Q236K),
 ('개_K10',        Q236K+['10 -']+K),
 ('개_K20',        Q236K+['20 -']+K),
 ('개_K30',        Q236K+['30 -']+K),
 # 황물기 파생을 hcb 로 (팔청 파생 후보 — 1라운드에서 156 떴던 것 재확인)
 ('황hcb10',       T+['10 -']+HCB),
]
res={}
CH=10
for ci in range(0,len(CASES),CH):
    chunk=CASES[ci:ci+CH]
    sc=['1 -']
    for tag,seq in chunk: sc+=trial(tag,seq)
    run(sc,'rk5.csv')
    rows={}
    for r in csv.DictReader(open('rk5.csv')):
        t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
    for tag,_ in chunk:
        d=rows.get(tag,{})
        ks=sorted(d)
        b=[int(d[s]['bank']) for s in ks]
        hp=[int(d[s]['hp2']) for s in ks]
        ka=[int(d[s]['kanim']) for s in ks]
        dmg=48-min(hp) if hp else 0
        banks=sorted(set(x for x in b if x!=255))
        kre=sum(1 for i in range(1,len(ka)) if ka[i]<ka[i-1])
        res[tag]=(dmg,banks,kre)
print('%-14s %4s %-22s %s'%('시행','피해','뱅크','K리셋'))
for tag,_ in CASES:
    dmg,banks,kre=res.get(tag,(0,[],0))
    print('%-14s %4d %-22s %d'%(tag,dmg,banks,kre))
