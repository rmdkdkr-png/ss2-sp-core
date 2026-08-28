#!/usr/bin/env python3
"""18캐릭터 원버튼 자동 검증 — 캐릭터별 세이브스테이트에서 X (중립·아래·공중) 발동 확인.
   판정 = 애니 카운터 리셋(새 동작 시작). chars.txt 로 ID↔상태 대응."""
import subprocess, os, csv

CORE=os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM =os.path.expanduser('~/ss2/rom/svc.ngc')
NAME={0:'쿄',1:'테리',2:'료',3:'마이',4:'레오나',5:'아테나',6:'이오리',7:'하오마루',8:'나코루루',
      9:'류',10:'춘리',11:'장기에프',12:'켄',13:'단',14:'사쿠라',15:'모리간',16:'펠리시아',17:'가일'}

st_of={}
for line in open('chars.txt'):
    p=line.split()
    if len(p)==2: st_of[int(p[0])]='svc_%s.st'%p[1]

def run(script,csvn):
    open('t.txt','w').write('\n'.join(script)+'\n')
    env=dict(os.environ); env['PROBE_CSV']=csvn; env['SVCSP_FORCE']='1'
    subprocess.run(['./svcrun',CORE,ROM,'t.txt'],capture_output=True,env=env)

SAM=(4,10,16,24,36,48,60)
def probe(tag, st, prelines):
    sc=['!load %s'%st,'30 -']+prelines
    prev=0
    for s in SAM:
        sc+=['%d -'%(s-prev),'!w %s@%d'%(tag,s)]; prev=s
    return sc

sc=['1 -']
for cid in range(18):
    st=st_of.get(cid)
    if not st or not os.path.exists(st): continue
    sc+=probe('c%d_N'%cid,   st, ['1 X'])
    sc+=probe('c%d_D'%cid,   st, ['2 D','1 D X'])
    sc+=probe('c%d_AIR'%cid, st, ['2 U','14 -','1 X'])
run(sc,'ct.csv')

rows={}
for r in csv.DictReader(open('ct.csv')):
    t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r

def fired(d, air=False):
    """발동 = 애니 리셋(P계) 또는 뱅크 이탈(K계 — 0x0C7E 는 K 기술에 안 걸린다) 또는 피해"""
    a=[int(d[s]['anim']) for s in SAM if s in d]
    if not a: return False,'',0
    ok=a[0]<=8 or any(a[i]<a[i-1] for i in range(1,len(a)))
    b=[int(d[s]['bank']) for s in SAM if s in d]
    jump={255,4,5} if air else {255}
    nz=[x for x in b if x not in jump]
    hp=48-min(int(d[s]['hp2']) for s in SAM if s in d)
    ok = ok or bool(nz) or hp>0
    allb=[x for x in b if x!=255]
    return ok,(str(allb[0]) if allb else '-'),hp

print('%-4s %-6s  %-12s %-12s %-12s'%('ID','이름','중립X(뱅크,피해)','아래X','공중X'))
npass=0; total=0
for cid in range(18):
    if 'c%d_N'%cid not in rows: continue
    cells=[]
    okall=True
    for kind in ('N','D','AIR'):
        d=rows.get('c%d_%s'%(cid,kind),{})
        ok,bank,hp=fired(d, kind=='AIR')
        cells.append('%s b%s d%d'%('발동' if ok else '불발',bank,hp))
        if kind!='AIR' and not ok: okall=False   # 공중은 참고만 (기술 없을 수 있음)
    total+=1; npass+=okall
    print('%-4d %-6s  %-12s %-12s %-12s %s'%(cid,NAME[cid],cells[0],cells[1],cells[2],'' if okall else '←확인'))
print('== 지상 2슬롯 기준 %d/%d =='%(npass,total))
