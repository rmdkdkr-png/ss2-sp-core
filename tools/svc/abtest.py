#!/usr/bin/env python3
"""A+B 동시 기능 × 유파(반격 st0 / 균형 c0_0 / 속공 st2) 실측.
   지상 중립·밀착·가드중 A+B 를 각각 눌러 뱅크·카운터·이동·게이지 관찰 + 스샷"""
import subprocess, os, csv, struct, zlib, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
CORE=os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM =os.path.expanduser('~/ss2/rom/svc.ngc')
STYLES=[('반격','svc_st0.st'),('균형','svc_c0_0.st'),('속공','svc_st2.st')]

def run(script,csvn):
    open('t.txt','w').write('\n'.join(script)+'\n')
    env=dict(os.environ); env['PROBE_CSV']=csvn; env['SVCSP_NOGATE']='1'
    subprocess.run(['./svcrun',CORE,ROM,'t.txt'],capture_output=True,env=env,timeout=900)
def png(tag):
    d=open('svc_%s.ppm'%tag,'rb').read()
    h=d.split(b'\n',3); w,hh=map(int,h[1].split()); px=h[3]
    raw=b''.join(b'\x00'+px[y*w*3:(y+1)*w*3] for y in range(hh))
    ck=lambda tp,dd: struct.pack('>I',len(dd))+tp+dd+struct.pack('>I',zlib.crc32(tp+dd))
    open('ab_%s.png'%tag,'wb').write(b'\x89PNG\r\n\x1a\n'+ck(b'IHDR',struct.pack('>IIBBBBB',w,hh,8,2,0,0,0))+ck(b'IDAT',zlib.compress(raw))+ck(b'IEND',b''))

def trial(tag, st, pre):
    sc=['!load %s'%st,'30 -']+pre+['3 A B']
    for i in range(30): sc+=['2 -','!w %s@%d'%(tag,i*2+2)]
    sc+=['!%s_s'%tag]
    return sc

sc=['1 -']
for sname,st in STYLES:
    sc+=trial('%s_중립'%sname, st, [])
    sc+=trial('%s_밀착'%sname, st, ['140 R'])
run(sc,'ab.csv')
rows={}
for r in csv.DictReader(open('ab.csv')):
    t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
print('%-10s %-14s %5s %5s %6s %6s'%('시행','뱅크','P계','K계','P1x이동','게이지Δ'))
for sname,st in STYLES:
    for kind in ('중립','밀착'):
        tag='%s_%s'%(sname,kind)
        d=rows.get(tag,{})
        ks=sorted(d)
        if not ks: print(tag,'무데이터'); continue
        b=sorted(set(int(d[s]['bank']) for s in ks)-{255})
        a=[int(d[s]['anim']) for s in ks]; kn=[int(d[s]['kanim']) for s in ks]
        xs=[int(d[s]['p1x']) for s in ks]
        pw=[int(d[s]['pow1']) for s in ks]
        pres = 'R' if any(a[i]<a[i-1] for i in range(1,len(a))) or a[0]<=6 else '-'
        kres = 'R' if any(kn[i]<kn[i-1] for i in range(1,len(kn))) or kn[0]<=6 else '-'
        print('%-10s %-14s %5s %5s %6d %6d'%(tag,b,pres,kres,xs[-1]-xs[0],pw[-1]-pw[0]))
        png('%s_s'%tag)
print('스샷: ab_*_s.png')
