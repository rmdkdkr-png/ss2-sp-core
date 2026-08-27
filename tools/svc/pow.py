#!/usr/bin/env python3
"""POW 게이지 확정: 후보 poke → 화면 하단 게이지 바 변화 + 지속성"""
import subprocess, os, struct, zlib, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

CORE=os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM =os.path.expanduser('~/ss2/rom/svc.ngc')
ST='svc_c0_0.st'

def run(script):
    open('t.txt','w').write('\n'.join(script)+'\n')
    env=dict(os.environ); env['PROBE_CSV']='p.csv'
    subprocess.run(['./svcrun',CORE,ROM,'t.txt'],capture_output=True,env=env)
def ram(tag): return open('svc_%s.ram'%tag,'rb').read()
def ppm(tag):
    d=open('svc_%s.ppm'%tag,'rb').read()
    h=d.split(b'\n',3); w,hh=map(int,h[1].split()); return w,hh,h[3]
def png(out,p):
    w,h,d=p
    raw=b''.join(b'\x00'+d[y*w*3:(y+1)*w*3] for y in range(h))
    def ck(t,dd): return struct.pack('>I',len(dd))+t+dd+struct.pack('>I',zlib.crc32(t+dd))
    open(out,'wb').write(b'\x89PNG\r\n\x1a\n'+ck(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+ck(b'IDAT',zlib.compress(raw))+ck(b'IEND',b''))

def rowdiff(a,b,y0,y1):
    """y0~y1 행에서 바뀐 픽셀 (x구간별: 왼쪽/오른쪽 절반)"""
    wa,_,da=a; db=b[2]; L=R=0
    for y in range(y0,y1):
        for x in range(wa):
            i=(y*wa+x)*3
            if da[i:i+3]!=db[i:i+3]:
                if x<wa//2: L+=1
                else: R+=1
    return L,R

CANDS=[('08EF',0x08EF),('08D6',0x08D6),('0AE3',0x0AE3),('0AE5',0x0AE5),('08B6',0x08B6)]
sc=['1 -','!load %s'%ST,'42 -','!pw_ref']
for name,off in CANDS:
    sc+=['!load %s'%ST,'20 -','!poke %s=60'%name,'22 -','!pw_%s'%name]
run(sc)
ref=ppm('pw_ref')
H=ref[1]
print('화면 %dx%d — 하단 1/4 (%d~%d행) 게이지 구역 감시'%(ref[0],H,H*3//4,H))
for name,off in CANDS:
    p=ppm('pw_%s'%name); r=ram('pw_%s'%name)
    L,R=rowdiff(ref,p,H*3//4,H)
    Lt,Rt=rowdiff(ref,p,0,H*3//4)
    print('  0x%s=60 → 하단 좌%d/우%d px, 상단 좌%d/우%d, 22f 후 값=%d %s'%(
        name,L,R,Lt,Rt,r[off],'★지속' if r[off]==60 else '(복원)'))
    png('pow_%s.png'%name,p)
png('pow_ref.png',ref)
