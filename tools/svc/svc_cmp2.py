# -*- coding: utf-8 -*-
"""새 규칙 vs 옛 규칙 — **이륙 기준** 상대 프레임으로 다시.

⚠ 앞 표는 원점이 틀렸다. U 를 누르고 «10프레임 뒤»에 뜬다. 그만큼 밀려 있었다.
  같은 병을 오늘만 세 번째 밟았다 — 「내가 어느 프레임을 세고 있나」부터 대라."""
import io,os,shutil,subprocess,tempfile
RUN=os.path.expanduser("~/ss2/repo/ss2-sp-core/tools/kof/ngprun")
CORE=os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM=os.path.expanduser("~/ss2/rom/svc.ngc")
Y1,ACT=0x0930,0x0968
ON="ngp_ss2sp=enabled,ngp_svcsp_engine=enabled,ngp_svcsp_land=enabled"
OFF="ngp_ss2sp=enabled,ngp_svcsp_engine=enabled,ngp_svcsp_land=disabled"
LEAD=30; TAKEOFF=10
def run(sc,opts,fall=None):
    env=dict(os.environ); env["NGP_OPTS"]=opts
    if fall is not None: env["SVCSP_LAND_FALL"]=str(fall)
    t=tempfile.mkdtemp(); r=os.path.join(t,"r.ngc"); shutil.copy(ROM,r)
    io.open(os.path.join(t,"s.txt"),"w",encoding="utf-8",newline="\n").write("\n".join(sc)+"\n")
    subprocess.run([RUN,CORE,r,os.path.join(t,"s.txt"),os.path.join(t,"g")],capture_output=True,text=True,env=env)
    p=os.path.join(t,"gt.csv")
    if not os.path.exists(p): return None
    rows=[l.split(",") for l in io.open(p).read().strip().splitlines()][1:]
    out=[];prev=None
    for r2 in rows:
        f,y,a=int(r2[0]),int(r2[2]),int(r2[3])
        if a!=prev: out.append((f,a)); prev=a
    return out
def g(q,land): return [a for f,a in (q or []) if f>land and a in (99,81)]
def sc2(st,rel):
    """rel = «이륙» 기준. 공중공격은 이륙+2 에, 두 번째 누름은 이륙+rel 에."""
    a1=TAKEOFF+2                 # 첫 누름 시작(스크립트 프레임, U 누른 순간 기준)
    a2=TAKEOFF+rel
    gap=a2-(a1+3)
    s=["!load %s"%st,"!w t %X,%X"%(Y1,ACT),"%d -"%LEAD,"4 U","%d -"%(a1-4),"3 Y"]
    if gap>0: s+=["%d -"%gap]
    return s+["3 Y","120 -","!w off"]
for name,st,air,ap,desc in [("id0  체공34 정점16 하강18","svc_c0_0.st",34,16,18),
                            ("id10 체공38 정점16 하강22","svc_ct_반격_정지_10_0.st",38,16,22)]:
    stp=os.path.expanduser("~/ss2/saves/svc/"+st); land=LEAD+TAKEOFF+air
    print("== %s =="%name)
    print("  이륙+  옵션끔  옛규칙(창32)  새규칙(하강)")
    for rel in (2,6,10,14,16,18,20,22,26,30):
        a=g(run(sc2(stp,rel),OFF),land)
        b=g(run(sc2(stp,rel),ON,0),land)
        c=g(run(sc2(stp,rel),ON,1),land)
        m=""
        if rel==ap: m="  ← 정점"
        elif rel==desc: m="  ← 하강 시작"
        print("  %-6d %-7s %-13s %s%s"%(rel,"O" if a else ".","O" if b else ".","O" if c else ".",m))
    print()
