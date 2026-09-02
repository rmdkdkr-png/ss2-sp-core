#!/usr/bin/env python3
"""P1 POW 원본 수색 — 0x0700~0x0AE0 poke=60, 하단 왼쪽 게이지 감시"""
import subprocess, os, struct, zlib, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

CORE=os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM =os.path.expanduser('~/ss2/rom/svc.ngc')
ST='svc_c0_0.st'

def run(script):
    open('t.txt','w').write('\n'.join(script)+'\n')
    env=dict(os.environ); env['PROBE_CSV']='p2.csv'
    subprocess.run(['./svcrun',CORE,ROM,'t.txt'],capture_output=True,env=env,timeout=900)
def ram(tag): return open('svc_%s.ram'%tag,'rb').read()
def ppm(tag):
    d=open('svc_%s.ppm'%tag,'rb').read()
    h=d.split(b'\n',3); w,hh=map(int,h[1].split()); return w,hh,h[3]
def png(out,p):
    w,h,d=p
    raw=b''.join(b'\x00'+d[y*w*3:(y+1)*w*3] for y in range(h))
    def ck(t,dd): return struct.pack('>I',len(dd))+t+dd+struct.pack('>I',zlib.crc32(t+dd))
    open(out,'wb').write(b'\x89PNG\r\n\x1a\n'+ck(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+ck(b'IDAT',zlib.compress(raw))+ck(b'IEND',b''))

sc=['1 -','!load %s'%ST,'42 -','!bl_ref']
run(sc)
ref=ppm('bl_ref'); W,H,_=ref

def blb(a,b):
    """하단(3/4~) 왼쪽 절반 바뀐 픽셀"""
    wa,_,da=a; db=b[2]; n=0
    for y in range(H*3//4,H):
        for x in range(0,wa//2):
            i=(y*wa+x)*3
            if da[i:i+3]!=db[i:i+3]: n+=1
    return n

offs=list(range(0x0700,0x0AE0))
hits=[]
CH=48
for ci in range(0,len(offs),CH):
    chunk=offs[ci:ci+CH]
    sc=['1 -']
    for o in chunk:
        sc+=['!load %s'%ST,'20 -','!poke %04X=60'%o,'22 -','!s_%04X'%o]
    try: run(sc)
    except Exception as e: print('청크 0x%04X 실패'%chunk[0]); continue
    for o in chunk:
        try: p=ppm('s_%04X'%o); r=ram('s_%04X'%o)
        except: continue
        n=blb(ref,p)
        if n>=8:
            hits.append((o,n,r[o]))
            png('powL_%04X.png'%o,p)
    for o in chunk:
        for e2 in ('ram','ppm'):
            try: os.remove('svc_s_%04X.%s'%(o,e2))
            except: pass
print('하단 왼쪽 게이지 반응 오프셋:')
for o,n,v in hits: print('  0x%04X  %dpx, 22f후 값=%d %s'%(o,n,v,'★지속' if v==60 else '(복원)'))
if not hits: print('  없음 — 범위 밖')
