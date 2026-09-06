# -*- coding: utf-8 -*-
"""누른 «뒤»만 본다. 앞 구간을 세면 어느 설정에서나 같은 목록이 나온다 — 내가 그랬다."""
import io,os,shutil,subprocess,tempfile
RUN="/mnt/c/Claude/KOF R2 한글/tools/ngprun"
CORE=os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM=os.path.expanduser("~/ss2/rom/ss2.ngc")
ST=os.path.expanduser("~/ss2/saves/ss2/ss2_fight.st")
ACT=0x0E3E
ON="ngp_ss2sp=enabled"; OFF="ngp_ss2sp=disabled"
def run(keys,opts,lead,hold=8,tail=70):
    env=dict(os.environ); env["NGP_OPTS"]=opts
    sc=["!load %s"%ST,"!w t %X,%X"%(ACT,ACT+1),"%d -"%lead,
        ("%d %s"%(hold,keys)) if keys!="-" else ("%d -"%hold),"%d -"%tail,"!w off"]
    t=tempfile.mkdtemp(); r=os.path.join(t,"r.ngc"); shutil.copy(ROM,r)
    io.open(os.path.join(t,"s.txt"),"w",encoding="utf-8",newline="\n").write("\n".join(sc)+"\n")
    subprocess.run([RUN,CORE,r,os.path.join(t,"s.txt"),os.path.join(t,"g")],capture_output=True,text=True,env=env)
    p=os.path.join(t,"gt.csv")
    if not os.path.exists(p): return None
    rows=[l.split(",") for l in io.open(p).read().strip().splitlines()][1:]
    out=[];prev=None
    for r2 in rows:
        f=int(r2[0]); v=int(r2[2])|(int(r2[3])<<8)
        if f<lead: prev=v; continue        # 리드인은 세지 않는다
        if v!=prev: out.append(v); prev=v
    return out
for lead in (240,480):
    print("== 리드인 %d — 누른 뒤만 =="%lead)
    base=None
    for name,keys in [("대조군","-"),("A+B 직접","A B"),("A 단독","B"),("L1","L1"),("R1","R1"),("X","X"),("Y","Y")]:
        for lab,opts in (("켬",ON),("끔",OFF)):
            v=run(keys,opts,lead)
            s=" ".join(hex(x) for x in (v or [])[:6]) or "(안 바뀜)"
            if keys=="-" and lab=="켬": base=s
            print("   %-9s %-3s %s%s"%(name,lab,s," ← 대조군과 같음" if s==base and keys!="-" else ""))
    print()
