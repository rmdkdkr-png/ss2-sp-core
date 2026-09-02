#!/usr/bin/env python3
"""18명 캔슬 지정기 전수 — 접근→앉강킥→롤236P. 킥 단독 대비 피해 증가 = 캔슬 유효"""
import subprocess, os, csv, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
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
    env=dict(os.environ); env['PROBE_CSV']=csvn; env['SVCSP_NOGATE']='1'
    subprocess.run(['./svcrun',CORE,ROM,'t.txt'],capture_output=True,env=env,timeout=900)
def trial(st, tag, seq):
    sc=['!load %s'%st,'20 -','140 R']+seq
    for i in range(35): sc+=['2 -','!w %s@%d'%(tag,i*2+2)]
    return sc
R236=['4 D','4 D R','4 R','3 R B']

res={}
ids=[i for i in range(18) if st_of.get(i) and os.path.exists(st_of[i])]
for ci in range(0,len(ids),4):
    chunk=ids[ci:ci+4]
    sc=['1 -']
    for cid in chunk:
        sc+=trial(st_of[cid],'k%d'%cid,['16 D A'])
        sc+=trial(st_of[cid],'c%d'%cid,['16 D A','4 -']+R236)
    run(sc,'c18.csv')
    rows={}
    for r in csv.DictReader(open('c18.csv')):
        t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
    for cid in chunk:
        out=[]
        for pre in ('k','c'):
            d=rows.get('%s%d'%(pre,cid),{})
            ks=sorted(d)
            b=sorted(set(int(d[s]['bank']) for s in ks)-{255})
            hp=48-min(int(d[s]['hp2']) for s in ks) if ks else 0
            out.append((hp,b))
        res[cid]=out
print('%-3s %-6s %6s %6s %-14s %s'%('ID','캐릭','킥단독','킥+캔슬','캔슬뱅크','판정'))
for cid in ids:
    (k,kb),(c,cb)=res[cid]
    gain=c-k
    verdict='공격기(활성)' if gain>0 else ('꽝/무반응(회피)' if k>0 else '킥헛침—판정불가')
    print('%-3d %-6s %6d %6d %-14s %s'%(cid,NAME[cid],k,c,cb[:4],verdict))
import json
json.dump({str(cid):{'kick':res[cid][0][0],'cancel':res[cid][1][0],'banks':res[cid][1][1]} for cid in ids},
          open('cancel18.json','w'))
