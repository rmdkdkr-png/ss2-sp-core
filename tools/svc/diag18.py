#!/usr/bin/env python3
"""불발 9건 정밀 진단 — 디버그 로그 + 2프레임 간격 애니/뱅크 추적"""
import subprocess, os, csv, json, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

CORE=os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM =os.path.expanduser('~/ss2/rom/svc.ngc')

st_of={}
for line in open('chars.txt'):
    p=line.split()
    if len(p)==2: st_of[int(p[0])]='svc_%s.st'%p[1]

# 슬롯 배치 확인용
mv=json.load(open('moves.json',encoding='utf-8'))['characters']
by_id={c['id']:c for c in mv}
def slotinfo(cid):
    c=by_id[cid]; sug=c['slot_suggestion']
    cmds={m['name']:(m['command'],m.get('button'),m.get('air'),m.get('status')) for m in c['table']}
    out=[]
    for k in ('N','D','DF','AIR'):
        n=sug.get(k)
        out.append('%s=%s%s'%(k,n,cmds.get(n,'')))
    return ' | '.join(out)

def run(script,csvn):
    open('t.txt','w').write('\n'.join(script)+'\n')
    env=dict(os.environ); env['PROBE_CSV']=csvn; env['SVCSP_DEBUG']='1'
    r=subprocess.run(['./svcrun',CORE,ROM,'t.txt'],capture_output=True,env=env,text=True)
    return r.stderr

FAILS=[(10,'D'),(12,'D'),(13,'D'),(14,'D'),(15,'D'),(16,'D'),(17,'D'),(6,'AIR'),(8,'AIR')]
for cid,kind in FAILS:
    st=st_of[cid]
    pre = ['2 D','1 D X'] if kind=='D' else ['2 U','14 -','1 X']
    sc=['1 -','!load %s'%st,'30 -']+pre
    for i in range(45): sc+=['2 -','!w t@%d'%(i*2+2)]
    err=run(sc,'dg.csv')
    rows={}
    for r in csv.DictReader(open('dg.csv')):
        t,at=r['tag'].rsplit('@',1); rows[int(at)]=r
    a=[int(rows[s]['anim']) for s in sorted(rows)]
    b=[int(rows[s]['bank']) for s in sorted(rows)]
    # 요약: 애니 리셋 시점, 뱅크 비255 구간
    reset=[i*2+2 for i,x in enumerate(a) if i and a[i]<a[i-1]]
    nz=sorted(set(x for x in b if x!=255))
    print('[%d %s] 리셋@%s 뱅크%s'%(cid,kind,reset[:3],nz))
    print('   слот: %s'%slotinfo(cid))
    for L in err.strip().split('\n')[-3:]:
        if L.strip(): print('   %s'%L)
    print('   anim: %s'%','.join(map(str,a[:30])))
    print()
