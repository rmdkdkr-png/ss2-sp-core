# -*- coding: utf-8 -*-
"""버튼 쥔 길이가 «무엇을» 바꾸나 — 나가는 시점만인가, 기술 자체인가."""
import io,os,shutil,subprocess,tempfile
RUN="/mnt/c/Claude/KOF R2 한글/tools/ngprun"
CORE=os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM=os.path.expanduser("~/ss2/rom/pristine/Last Blade, The (UE) [!].ngc")
ST=os.path.expanduser("~/ss2/saves/lb/lb_train.st")
def run(btn,idle=20):
    env=dict(os.environ)
    env["NGP_OPTS"]="ngp_ss2sp=enabled,ngp_lbsp_engine=enabled"
    env["LBSP_RING"]="1"; env["LBSP_RING_F"]="2"; env["LBSP_RING_BTN"]=str(btn)
    sc=["!load %s"%ST,"!w t 370","%d -"%idle,"1 R1","110 -","!w off"]
    t=tempfile.mkdtemp(); r=os.path.join(t,"r.ngc"); shutil.copy(ROM,r)
    io.open(os.path.join(t,"s.txt"),"w",encoding="utf-8",newline="\n").write("\n".join(sc)+"\n")
    subprocess.run([RUN,CORE,r,os.path.join(t,"s.txt"),os.path.join(t,"g")],capture_output=True,text=True,env=env)
    p=os.path.join(t,"gt.csv")
    rows=[l.split(",") for l in io.open(p).read().strip().splitlines()][1:]
    out=[];prev=None
    for r in rows:
        v=int(r[2])
        if v!=prev: out.append((int(r[0])-(idle+1),v)); prev=v
    return out
for b in (2,3,4,6,10,16):
    q=run(b)
    print("버튼 %2d프레임 → %s"%(b," ".join("%d@+%d"%(v,f) for f,v in q[:8])))
