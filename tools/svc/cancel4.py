#!/usr/bin/env python3
"""뱅크 115 정체: 스크린샷 + 캔슬 후 구상 체인 + 다른 필살기 캔슬(623P·214P)"""
import subprocess, os, csv, struct, zlib, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

CORE=os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM =os.path.expanduser('~/ss2/rom/svc.ngc')
ST='svc_f2.st'
Q=['3 D','3 D R','3 R','3 R B']
Q623=['3 R','3 D','3 D R','3 D R B']
Q214=['3 D','3 D L','3 L','3 L B']
CK=['16 D A']   # 앉아강킥 (히트 확보)

def run(script,csvn):
    open('t.txt','w').write('\n'.join(script)+'\n')
    env=dict(os.environ); env['PROBE_CSV']=csvn; env['SVCSP_NOGATE']='1'
    subprocess.run(['./svcrun',CORE,ROM,'t.txt'],capture_output=True,env=env,timeout=900)
def png(tag):
    d=open('svc_%s.ppm'%tag,'rb').read()
    h=d.split(b'\n',3); w,hh=map(int,h[1].split()); px=h[3]
    raw=b''.join(b'\x00'+px[y*w*3:(y+1)*w*3] for y in range(hh))
    ck=lambda tp,dd: struct.pack('>I',len(dd))+tp+dd+struct.pack('>I',zlib.crc32(tp+dd))
    open('cx_%s.png'%tag,'wb').write(b'\x89PNG\r\n\x1a\n'+ck(b'IHDR',struct.pack('>IIBBBBB',w,hh,8,2,0,0,0))+ck(b'IDAT',zlib.compress(raw))+ck(b'IEND',b''))

# 스크린샷: 캔슬 순간
sc=['1 -','!load %s'%ST,'10 -']+CK+['4 -']+Q+['10 -','!cx1','14 -','!cx2']
run(sc,'x0.csv')
png('cx1'); png('cx2')

def trial(tag, seq, n=50):
    sc=['!load %s'%ST,'10 -']+seq
    for i in range(n): sc+=['2 -','!w %s@%d'%(tag,i*2+2)]
    return sc

CASES=[
 ('킥→236P→236P', CK+['4 -']+Q+['14 -']+Q),      # 캔슬 후 구상으로 이어지나
 ('킥→623P_4',    CK+['4 -']+Q623),               # 대공기도 캔슬되나
 ('킥→214P_4',    CK+['4 -']+Q214),               # 팔청도
 ('킥→236P홀드',   CK+['4 -']+Q[:3]+['16 R B']),   # 캔슬에서 강(독물기)판
 ('약킥→236P_4',   ['3 A','4 -']+Q),               # 가드당한 약킥에서도 캔슬 나가나 (가드캔슬?)
]
res={}
sc=['1 -']
for tag,seq in CASES: sc+=trial(tag,seq)
run(sc,'x1.csv')
rows={}
for r in csv.DictReader(open('x1.csv')):
    t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
print('%-14s %4s %s'%('시행','피해','뱅크'))
for tag,_ in CASES:
    d2=rows.get(tag,{})
    ks=sorted(d2)
    hp=48-min(int(d2[s]['hp2']) for s in ks) if ks else 0
    b=[int(d2[s]['bank']) for s in ks]
    print('%-14s %4d %s'%(tag,hp,sorted(set(x for x in b if x!=255))))
