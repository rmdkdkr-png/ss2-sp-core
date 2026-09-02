#!/usr/bin/env python3
"""P1 POW 보정: 값별 게이지 채움·Lv 표시 → 만땅 값 결정 → 쿄 초필 소모 시험"""
import subprocess, os, struct, zlib, io, sys, csv
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

CORE=os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM =os.path.expanduser('~/ss2/rom/svc.ngc')
ST='svc_c0_0.st'

def run(script,csvn='p3.csv'):
    open('t.txt','w').write('\n'.join(script)+'\n')
    env=dict(os.environ); env['PROBE_CSV']=csvn; env['SVCSP_NOGATE']='1'
    subprocess.run(['./svcrun',CORE,ROM,'t.txt'],capture_output=True,env=env,timeout=600)
def ram(tag): return open('svc_%s.ram'%tag,'rb').read()
def ppm(tag):
    d=open('svc_%s.ppm'%tag,'rb').read()
    h=d.split(b'\n',3); w,hh=map(int,h[1].split()); return w,hh,h[3]
def png(out,p):
    w,h,d=p
    raw=b''.join(b'\x00'+d[y*w*3:(y+1)*w*3] for y in range(h))
    def ck(t,dd): return struct.pack('>I',len(dd))+t+dd+struct.pack('>I',zlib.crc32(t+dd))
    open(out,'wb').write(b'\x89PNG\r\n\x1a\n'+ck(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+ck(b'IDAT',zlib.compress(raw))+ck(b'IEND',b''))

# 값별 채움 + 주변 바이트(Lv 표시 후보) 관찰
sc=['1 -']
for v in (30,60,120,180,240,255):
    sc+=['!load %s'%ST,'20 -','!poke 0963=%d'%v,'22 -','!pv_%d'%v]
run(sc)
strips=[]
for v in (30,60,120,180,240,255):
    p=ppm('pv_%d'%v); r=ram('pv_%d'%v)
    w,h,d=p
    print('poke %3d → 유지값 %3d, 주변 0x0960~0x0968: %s'%(v,r[0x0963],[r[o] for o in range(0x0960,0x0969)]))
    bot=[d[y*w*3:(y+1)*w*3] for y in range(h-20,h)]
    big=[]
    for row in bot:
        r3=b''.join(row[i:i+3]*3 for i in range(0,len(row),3))
        big+=[r3,r3,r3]
    strips+=big+[b'\xff\x00\x00'*(w*3)]*2
raw_h=len(strips)
def wr(out,w2,rows):
    raw=b''.join(b'\x00'+r for r in rows)
    def ck(t,dd): return struct.pack('>I',len(dd))+t+dd+struct.pack('>I',zlib.crc32(t+dd))
    open(out,'wb').write(b'\x89PNG\r\n\x1a\n'+ck(b'IHDR',struct.pack('>IIBBBBB',w2,len(rows),8,2,0,0,0))+ck(b'IDAT',zlib.compress(raw))+ck(b'IEND',b''))
wr('powcal.png',ppm('pv_30')[0]*3,strips)
print('powcal.png (30/60/120/180/240/255 순)')

# 쿄 초필 소모 시험: 게이지 만땅 → 236236P 주입 → 게이지·피해 추적
sc=['1 -','!load %s'%ST,'20 -','!poke 0963=240','10 -',
    '3 D','3 D R','3 R','3 D','3 D R','3 R B','3 R B']
for i in range(20): sc+=['4 -','!w sup@%d'%(i*4+4)]
run(sc)
rows={}
for r in csv.DictReader(open('p3.csv')):
    t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
d=rows.get('sup',{})
ks=sorted(d)
print('쿄 236236P (게이지 240에서): 게이지 흐름 %s'%','.join(d[k].get('pow1','?') for k in ks[:12]))
print('  주의: pow1 필드는 0x08EF(타이머) — 곧 svcrun 을 0x0963 으로 바꿔야 함')
print('  뱅크 %s'%','.join(d[k]['bank'] for k in ks[:12]))
print('  피해 %s'%(48-min(int(d[k]['hp2']) for k in ks)))
# 직접 램으로 게이지 확인
sc=['1 -','!load %s'%ST,'20 -','!poke 0963=240','10 -',
    '3 D','3 D R','3 R','3 D','3 D R','3 R B','40 -','!sup_after']
run(sc)
r=ram('sup_after')
print('  초필 후 0x0963=%d (240에서 줄었으면 게이지 소모 = 초필 발동 증거)'%r[0x0963])
