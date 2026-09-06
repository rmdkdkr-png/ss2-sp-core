# -*- coding: utf-8 -*-
"""ngp_ss2sp 를 끄면 «다른 게임»의 A+B(L)까지 죽나 — 재서 가른다.

1-e8 의 읽기: ngp_ss2sp 는 SS2 에서만 보이는 항목인데 값은 한 벌로 저장된다.
그래서 SS2 에서 끄면 SvC·KOF·월화의 폴드까지 죽는다 — 유저는 그 게임 설정에서
그 항목을 보지도 되돌리지도 못한다. 맞으면 (나)는 선택이 아니라 «고침»이다.
"""
import io,os,shutil,subprocess,tempfile
RUN="/mnt/c/Claude/KOF R2 한글/tools/ngprun"
CORE=os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
G={
 "월화":(os.path.expanduser("~/ss2/rom/pristine/Last Blade, The (UE) [!].ngc"),
        os.path.expanduser("~/ss2/saves/lb/lb_train.st"),0x0370,240),
 "KOF":(os.path.expanduser("~/ss2/rom/kofr2.ngc"),
        os.path.expanduser("~/ss2/saves/kof/kof_c00.st"),0x0D3C,20),
}
def run(g,keys,opts,lead,hold=8):
    rom,st,act,_=G[g]
    env=dict(os.environ); env["NGP_OPTS"]=opts
    sc=["!load %s"%st,"!w t %X"%act,"%d -"%lead,
        ("%d %s"%(hold,keys)) if keys!="-" else ("%d -"%hold),"70 -","!w off"]
    t=tempfile.mkdtemp(); r=os.path.join(t,"r.ngc"); shutil.copy(rom,r)
    io.open(os.path.join(t,"s.txt"),"w",encoding="utf-8",newline="\n").write("\n".join(sc)+"\n")
    subprocess.run([RUN,CORE,r,os.path.join(t,"s.txt"),os.path.join(t,"g")],capture_output=True,text=True,env=env)
    p=os.path.join(t,"gt.csv")
    if not os.path.exists(p): return None
    rows=[l.split(",") for l in io.open(p).read().strip().splitlines()][1:]
    out=[];prev=None
    for r2 in rows:
        f=int(r2[0]); v=int(r2[2])
        if f<lead: prev=v; continue
        if v!=prev: out.append(v); prev=v
    return out
for g in ("월화","KOF"):
    _,_,_,lead=G[g]
    print("== %s (누른 뒤만) =="%g)
    for keys,nm in [("-","대조군"),("A B","NGP A+B 직접"),("L1","L1"),("R1","R1")]:
        for lab,opts in [("ss2sp 켬","ngp_ss2sp=enabled"),("ss2sp 끔","ngp_ss2sp=disabled")]:
            v=run(g,keys,opts,lead)
            print("   %-14s %-10s %s"%(nm,lab," ".join(str(x) for x in (v or [])[:4]) or "(안 바뀜)"))
    print()
