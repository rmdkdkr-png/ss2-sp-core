#!/usr/bin/env python3
"""SvC 기술 전수 실측기 — 커맨드 후보를 전부 넣어보고 무엇이 나갔는지 분류한다.

한 번의 프로세스 실행 안에서 상태를 계속 되돌려 가며 시행을 반복하고,
각 시행의 결과를 CSV 로 받아 지문(fingerprint)으로 묶는다.
같은 지문 = 같은 기술.  사용: probe.py <상태파일> [간격]
"""
import subprocess, sys, csv, collections

CORE='/home/user/ss2-sp-core/build/mednafen_ngp_libretro.so'
ROM ='/home/user/rom/svc_kr_v17.2.ngc'
DIR={'↑':'U','↗':'U R','→':'R','↘':'D R','↓':'D','↙':'D L','←':'L','↖':'U L'}
BTN={'P':'B','K':'A','PK':'B A'}          # P=NGP A=레트로 B / K=NGP B=레트로 A
SAMPLES=(4,10,16,24,36,48)                # 버튼 이후 관찰 시점

# ── 후보 커맨드 ──────────────────────────────────────────────
MOTIONS=[
 ("",        "그냥"),
 ("→",       "앞"),      ("↘","앞아래"), ("↓","아래"), ("↙","뒤아래"), ("←","뒤"),
 ("↑",       "위"),
 ("↓↘→",     "236"),     ("↓↙←","214"),  ("→↓↘","623"), ("←↓↙","421"),
 ("→↘↓↙←",   "41236역"), ("←↙↓↘→","41236"), ("→↘↓↙←→","반회전왕복"),
 ("↓↓",      "22"),      ("↓↘→↓↘→","236236"), ("↓↙←↓↙←","214214"),
 ("↓↘→↘↓↙←", "용호난무형"),
 ("←→",      "45"),      ("→←→","646"),
]
CHARGE=[  # (모으는 방향, 모으는 프레임, 마지막 방향)
 ("←",40,"→","뒤모아앞"), ("↓",40,"↑","아래모아위"), ("↙",40,"↗","뒤아래모아앞위"),
]

def emit(f, tag, state, lines):
    f.append("!load %s"%state)
    f.append("20 -")                       # 상태 안정화
    f += lines
    for i,s in enumerate(SAMPLES):
        gap = s if i==0 else s-SAMPLES[i-1]
        f.append("%d -"%gap)
        f.append("!w %s@%d"%(tag,s))

def build(state, gap):
    f=["1 -"]
    trials=[]
    for m,name in MOTIONS:
        for b in ("P","K"):
            tag="%s|%s"%(name,b); trials.append((tag,m,b))
            L=["%d %s"%(gap,DIR[d]) for d in m]
            last = DIR[m[-1]] if m else "-"
            L.append("3 %s %s"%(last,BTN[b]))
            emit(f,tag,state,L)
    for m,name in MOTIONS:                       # 공중판
        if name in ("위",): continue
        for b in ("P","K"):
            tag="공중%s|%s"%(name,b); trials.append((tag,m,b))
            L=["2 U","14 -"]
            L+=["%d %s"%(gap,DIR[d]) for d in m]
            last = DIR[m[-1]] if m else "-"
            L.append("3 %s %s"%(last,BTN[b]))
            emit(f,tag,state,L)
    for hold,fr,last,name in CHARGE:
        for b in ("P","K"):
            tag="%s|%s"%(name,b); trials.append((tag,hold+last,b))
            emit(f,tag,state,["%d %s"%(fr,DIR[hold]), "3 %s %s"%(DIR[last],BTN[b])])
    return "\n".join(f)+"\n", trials

def main():
    state=sys.argv[1] if len(sys.argv)>1 else 'svc_f2.st'
    gap=int(sys.argv[2]) if len(sys.argv)>2 else 3
    script,trials=build(state,gap)
    open('probe_script.txt','w').write(script)
    subprocess.run(['./svcrun',CORE,ROM,'probe_script.txt'],
                   capture_output=True, env={'PROBE_CSV':'probe.csv','PATH':'/usr/bin:/bin'})
    rows=collections.defaultdict(dict)
    for r in csv.DictReader(open('probe.csv')):
        tag,at=r['tag'].rsplit('@',1)
        rows[tag][int(at)]=r
    base=rows.get('그냥|P')
    print("상태 %s · 방향 간격 %d프레임\n"%(state,gap))
    sig=collections.defaultdict(list)
    for tag,_,_ in [(t[0],0,0) for t in trials]:
        d=rows.get(tag)
        if not d: continue
        banks=tuple(int(d[s]['bank']) for s in SAMPLES if s in d)
        hp=min(int(d[s]['hp2']) for s in SAMPLES if s in d)
        dx=int(d[SAMPLES[-1]]['p1x'])-int(d[SAMPLES[0]]['p1x'])
        # 새 동작이 시작되면 애니 카운터가 0으로 리셋된다 → 버튼 직후 값이 작다
        a4=int(d[SAMPLES[0]]['anim'])
        fired = a4 <= 6
        ys=tuple(int(d[s]['p1y']) for s in SAMPLES if s in d)
        air = min(ys) < 126
        sig[(banks,hp<48,fired,air)].append((tag,banks,48-hp,dx,fired,air))
    idle=None
    for k,v in sig.items():
        if any(t[0]=='그냥|P' for t in v): idle=k
    groups=[]; dead=[]
    for k,v in sig.items():
        banks,hurt,fired,air = k
        if not fired: dead += [t[0] for t in v]; continue
        dmg=max(t[2] for t in v); dx=v[0][3]
        groups.append((banks,dmg,dx,air,[t[0] for t in v]))
    groups.sort(key=lambda g:(g[3],-g[1],g[0]))
    print("%-4s %-24s %5s %6s  %s"%("","애니뱅크 흐름","피해","이동","이 커맨드들로 나온다"))
    print("-"*100)
    for banks,dmg,dx,air,tags in groups:
        flow=",".join(map(str,banks)); tagair="공중" if air else "지상"
        if set(banks)=={255}: tagair+="평타"   # 모션이 무효 → 그냥 평타가 나갔다
        print("%-4s %-24s %5s %6d  %s"%(tagair, flow, dmg or "-", dx, tags[0]))
        for t in tags[1:]: print("%-4s %-24s %5s %6s  %s"%("","","","",t))
    print("\n발동 안 됨 (%d): %s"%(len(dead), " · ".join(dead)))
main()
