# -*- coding: utf-8 -*-
"""강약 문턱 — 위상 2종 + «성질이 다른» 독립 시나리오(엔진 없이 평타를 직접 쥐기)."""
import io,os,shutil,subprocess,tempfile
RUN="/mnt/c/Claude/KOF R2 한글/tools/ngprun"
CORE=os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM=os.path.expanduser("~/ss2/rom/pristine/Last Blade, The (UE) [!].ngc")
ST=os.path.expanduser("~/ss2/saves/lb/lb_train.st")
def go(sc,env_extra,idle):
    env=dict(os.environ); env["NGP_OPTS"]=env_extra.pop("OPTS"); env.update(env_extra)
    t=tempfile.mkdtemp(); r=os.path.join(t,"r.ngc"); shutil.copy(ROM,r)
    io.open(os.path.join(t,"s.txt"),"w",encoding="utf-8",newline="\n").write("\n".join(sc)+"\n")
    subprocess.run([RUN,CORE,r,os.path.join(t,"s.txt"),os.path.join(t,"g")],capture_output=True,text=True,env=env)
    rows=[l.split(",") for l in io.open(os.path.join(t,"gt.csv")).read().strip().splitlines()][1:]
    out=[];prev=None
    for r in rows:
        v=int(r[2])
        if v!=prev: out.append((int(r[0])-(idle+1),v)); prev=v
    return out
def sp(btn,ph):
    idle=20+ph
    return go(["!load %s"%ST,"!w t 370","%d -"%idle,"1 R1","110 -","!w off"],
              {"OPTS":"ngp_ss2sp=enabled,ngp_lbsp_engine=enabled","LBSP_RING":"1",
               "LBSP_RING_F":"2","LBSP_RING_BTN":str(btn)},idle)
def plain(hold,ph):
    idle=20+ph
    return go(["!load %s"%ST,"!w t 370","%d -"%idle,"%d B"%hold,"110 -","!w off"],
              {"OPTS":"ngp_ss2sp=enabled,ngp_lbsp_engine=disabled"},idle)
def dur(q,a,b):
    d={v:f for f,v in q}
    return (d.get(a,-99), d.get(b,-99)-d.get(a,0) if a in d and b in d else None)
print("① 필살기(236+A) — 엔진·링 켬")
for ph in (0,1):
    for b in (6,7,8,9):
        s,d=dur(sp(b,ph),112,144)
        print("   위상%d 버튼%2d → 112 시작 +%-3d 지속 %s"%(ph,b,s,d))
print("② 평타(그냥 베기) — 엔진 «끔», 손으로 쥐기  ← 성질이 다른 증인")
for ph in (0,1):
    for h in (6,7,8,9):
        q=plain(h,ph)
        vals=[v for f,v in q if v!=4]
        d={v:f for f,v in q}
        e=None
        if 96 in d:
            nxt=[f for f,v in q if f>d[96]]
            e=(nxt[0]-d[96]) if nxt else None
        print("   위상%d 쥠%2d → act %s (96 지속 %s)"%(ph,h,vals[:4],e))
