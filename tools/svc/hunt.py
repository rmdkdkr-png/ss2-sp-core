#!/usr/bin/env python3
"""검수 기반 사냥 ①: K 쪽 애니 카운터  ②: POW 게이지 오프셋"""
import subprocess, os, csv, glob, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

CORE=os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM =os.path.expanduser('~/ss2/rom/svc.ngc')

def run(script,csvn='h.csv'):
    open('t.txt','w').write('\n'.join(script)+'\n')
    env=dict(os.environ); env['PROBE_CSV']=csvn; env['SVCSP_NOGATE']='1'
    subprocess.run(['./svcrun',CORE,ROM,'t.txt'],capture_output=True,env=env)
def ram(tag): return open('svc_%s.ram'%tag,'rb').read()

# ── ① K 카운터 사냥: 아무것도 안 함 vs 펀치 vs 킥 — +8f 시점 전 램 비교 ──
sc=['1 -',
    '!load svc_c0_0.st','30 -','8 -','!k_idle',
    '!load svc_c0_0.st','30 -','3 B','5 -','!k_punch',    # B=펀치
    '!load svc_c0_0.st','30 -','3 A','5 -','!k_kick',     # A=킥
    '!load svc_c0_0.st','30 -','3 A','25 -','!k_kick30']
run(sc)
idle,pu,ki,ki30=ram('k_idle'),ram('k_punch'),ram('k_kick'),ram('k_kick30')
print('① 동작 카운터 후보 (idle과 다르고, 값이 작은(≤10) 바이트 = 방금 시작한 동작의 시계)')
print('   [펀치에서만]', end=' ')
pc=[o for o in range(len(idle)) if pu[o]!=idle[o] and pu[o]<=10 and ki[o]==idle[o]]
print(['0x%04X=%d'%(o,pu[o]) for o in pc if 0x0C00<=o<0x0D00] or '(0x0C대역 없음)')
print('   [킥에서만] ', end=' ')
kc=[o for o in range(len(idle)) if ki[o]!=idle[o] and ki[o]<=10 and pu[o]==idle[o]]
print(['0x%04X=%d'%(o,ki[o]) for o in kc if 0x0C00<=o<0x0D00] or '(0x0C대역 없음)')
print('   [둘 다]   ', end=' ')
bc=[o for o in range(len(idle)) if ki[o]!=idle[o] and pu[o]!=idle[o] and ki[o]<=10 and pu[o]<=10]
print(['0x%04X p=%d k=%d'%(o,pu[o],ki[o]) for o in bc][:12])
# 킥 카운터 후보의 시간 진행 확인
for o in (kc+bc)[:8]:
    print('   0x%04X: 킥+8f=%d 킥+28f=%d (증가하면 카운터)'%(o,ki[o],ki30[o]))

# ── ② POW 게이지: 전투 진행(ta 시리즈) 동안 단조 증가한 바이트 ──
tas=sorted(glob.glob('svc_ta_*.ram'))
print()
print('② POW 후보 — 전투(피격 포함) 14초 동안 단조 증가 (ta 덤프 %d장)'%len(tas))
if len(tas)>=6:
    ds=[open(f,'rb').read() for f in tas]
    cands=[]
    for o in range(len(ds[0])):
        s=[d[o] for d in ds]
        inc=sum(1 for i in range(1,len(s)) if s[i]>s[i-1])
        dec=sum(1 for i in range(1,len(s)) if s[i]<s[i-1])
        if inc>=3 and dec==0 and s[-1]-s[0]>=8 and s[-1]<=250:
            cands.append((o,s[0],s[-1]))
    for o,a,b in cands[:20]: print('   0x%04X: %d → %d'%(o,a,b))
    print('   (P1 블록 0x08A0~0x08BF / P2 블록 0x08C0~0x08DF 안쪽이 유력)')
else:
    print('   ta 덤프 없음 — 별도 생성 필요')
