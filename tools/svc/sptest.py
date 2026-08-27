#!/usr/bin/env python3
"""7차: 전체 회귀 — 원버튼 매트릭스 + 경직 보류 + 걷기 + 선택화면 + SS2 불간섭"""
import subprocess, os, csv

CORE=os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM =os.path.expanduser('~/ss2/rom/svc.ngc')
SS2 =os.path.expanduser('~/ss2/rom/ss2.ngc')

def run(script,csvn,rom=ROM):
    open('t.txt','w').write('\n'.join(script)+'\n')
    env=dict(os.environ); env['PROBE_CSV']=csvn
    subprocess.run(['./svcrun',CORE,rom,'t.txt'],capture_output=True,env=env)
def ppm(tag):
    d=open('svc_%s.ppm'%tag,'rb').read()
    h=d.split(b'\n',3); w,hh=map(int,h[1].split()); return w,hh,h[3]

SAM=(4,10,16,24,36,48,60,80,100)
def probe(tag, prelines):
    sc=['!load svc_c0_0.st','30 -']+prelines
    prev=0
    for s in SAM:
        sc+=['%d -'%(s-prev),'!w %s@%d'%(tag,s)]; prev=s
    return sc

CASES=[
 ('중립X',   ['1 X'],            lambda b,a,hp: 22 in b),
 ('아래X',   ['2 D','1 D X'],    lambda b,a,hp: 23 in b),
 ('뒤X',     ['2 L','1 L X'],    lambda b,a,hp: any(x==0 for x in b) and min(a)<=12),
 ('앞X',     ['2 R','1 R X'],    lambda b,a,hp: min(a)<=12),
 ('공중X',   ['2 U','14 -','1 X'], lambda b,a,hp: 4 in b),
 ('경직X',   ['3 B','4 -','1 X'], lambda b,a,hp: 22 in b),
 ('걷기40X', ['40 R','1 X'],     lambda b,a,hp: 22 in b),
 ('걷기100X',['100 R','1 X'],    lambda b,a,hp: 22 in b),
 ('밀착뒤X', ['170 R','2 L','1 L X'], lambda b,a,hp: hp>0 or min(a)<=12),  # 밀착 팔청 → 피해 기대
]
sc=['1 -']
for tag,pre,_ in CASES: sc+=probe(tag,pre)
run(sc,'spR.csv')
rows={}
for r in csv.DictReader(open('spR.csv')):
    t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
npass=0
print('%-10s %-32s %4s %s'%('시행','뱅크 흐름','피해','판정'))
for tag,_,chk in CASES:
    d=rows.get(tag,{})
    b=[int(d[s]['bank']) for s in SAM if s in d]
    a=[int(d[s]['anim']) for s in SAM if s in d]
    hp=48-min(int(d[s]['hp2']) for s in SAM if s in d)
    ok=chk(b,a,hp)
    npass+=ok
    print('%-10s %-32s %4d %s'%(tag,','.join(map(str,b)),hp,'PASS' if ok else 'FAIL'))

# 선택화면 (프레임 정렬)
sc=['1 -','!load svc_psel.st','60 -','!q_ref',
    '!load svc_psel.st','30 -','1 X','29 -','!q_x']
run(sc,'spS.csv')
a=ppm('q_ref'); b=ppm('q_x')
diff=sum(1 for i in range(0,len(a[2]),3) if a[2][i:i+3]!=b[2][i:i+3])
ok=diff==0; npass+=ok
print('%-10s 바뀐 픽셀 %d %s'%('선택화면X',diff,'PASS' if ok else 'FAIL'))

# SS2 롬 불간섭 — svcsp 가 자면 ss2sp 경로. 크래시 없이 X 눌러 프레임 진행되면 통과
ss2st=os.path.expanduser('~/ss2/saves/ss2/tk_base.st')
if os.path.exists(ss2st):
    import shutil; shutil.copy(ss2st,'ss2_tk.st')
    sc=['1 -','!load ss2_tk.st','30 -','1 X','60 -','!ss2ok']
    run(sc,'spT.csv',rom=SS2)
    ok=os.path.exists('svc_ss2ok.ram'); npass+=ok
    print('%-10s %s'%('SS2불간섭','PASS' if ok else 'FAIL'))
print('== %d 통과 =='%npass)
