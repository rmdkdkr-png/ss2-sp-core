# -*- coding: utf-8 -*-
"""착지 선입력이 «실제로 발동하는» 상황을 찾는다.
한 누름은 한 기술이라, 공중공격에 쓰인 누름은 무장이 풀린다(코드 주석).
그러니 «두 번째 누름»이 본무대다: 점프 → 공중공격 → 또 누름 → 착지에 지상공격(99/81)."""
import io,os,shutil,subprocess,tempfile
RUN=os.path.expanduser("~/ss2/repo/ss2-sp-core/tools/kof/ngprun")
CORE=os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM=os.path.expanduser("~/ss2/rom/svc.ngc"); ST=os.path.expanduser("~/ss2/saves/svc/svc_c0_0.st")
Y1,ACT=0x0930,0x0968
ON="ngp_ss2sp=enabled,ngp_svcsp_engine=enabled,ngp_svcsp_land=enabled"
OFF="ngp_ss2sp=enabled,ngp_svcsp_engine=enabled,ngp_svcsp_land=disabled"
LEAD=30
def run(sc,opts,win=None):
    env=dict(os.environ); env["NGP_OPTS"]=opts
    if win is not None: env["SVCSP_LAND_WIN"]=str(win)
    t=tempfile.mkdtemp(); r=os.path.join(t,"r.ngc"); shutil.copy(ROM,r)
    io.open(os.path.join(t,"s.txt"),"w",encoding="utf-8",newline="\n").write("\n".join(sc)+"\n")
    subprocess.run([RUN,CORE,r,os.path.join(t,"s.txt"),os.path.join(t,"g")],capture_output=True,text=True,env=env)
    p=os.path.join(t,"gt.csv")
    if not os.path.exists(p): return None
    rows=[l.split(",") for l in io.open(p).read().strip().splitlines()][1:]
    out=[];prev=None
    for r2 in rows:
        f,y,a=int(r2[0]),int(r2[2]),int(r2[3])
        if a!=prev: out.append((f,y,a)); prev=a
    return out
def has_ground_atk(q,after=74):
    return [a for f,y,a in (q or []) if f>after and a in (99,81)]
def sc2(t2):
    """공중공격은 뜨자마자(상대 6), 두 번째 누름은 상대 t2."""
    gap=t2-6-3
    s=["!load %s"%ST,"!w t %X,%X"%(Y1,ACT),"%d -"%LEAD,"4 U","2 -","3 Y"]
    if gap>0: s+=["%d -"%gap]
    s+=["3 Y","100 -","!w off"]
    return s
print("두 번째 누름 시점별 — 착지 뒤 지상공격(99/81)이 나오나\n")
print("상대  옵션끔        창32        창20        창12")
for t2 in (12,16,20,24,28,32):
    row=[]
    for opts,w in ((OFF,None),(ON,32),(ON,20),(ON,12)):
        q=run(sc2(t2),opts,w)
        g=has_ground_atk(q)
        row.append(("O%s"%g[0]) if g else ".")
    print("%-5d %-12s %-11s %-11s %s"%(t2,row[0],row[1],row[2],row[3]))
