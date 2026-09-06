# -*- coding: utf-8 -*-
"""공중 강이 흔들리나 — 새 규칙이 무장 시점을 늦추면 «버튼 삼키기»도 늦어진다.
   삼키기가 공중 강(홀드 판정)을 깨면 안 된다(주석: 4프레임에서 끊었더니 82로 떨어졌다)."""
import io,os,shutil,subprocess,tempfile
RUN=os.path.expanduser("~/ss2/repo/ss2-sp-core/tools/kof/ngprun")
CORE=os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM=os.path.expanduser("~/ss2/rom/svc.ngc"); Y1,ACT=0x0930,0x0968
ON="ngp_ss2sp=enabled,ngp_svcsp_engine=enabled,ngp_svcsp_land=enabled"
OFF="ngp_ss2sp=enabled,ngp_svcsp_engine=enabled,ngp_svcsp_land=disabled"
LEAD=30; TAKEOFF=10
def run(st,rel,opts,fall=None,btn="Y"):
    env=dict(os.environ); env["NGP_OPTS"]=opts
    if fall is not None: env["SVCSP_LAND_FALL"]=str(fall)
    pre=TAKEOFF+rel-4
    sc=["!load %s"%st,"!w t %X,%X"%(Y1,ACT),"%d -"%LEAD,"4 U","%d -"%max(pre,0),"3 %s"%btn,"100 -","!w off"]
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
    air=[a for f,y,a in out if y!=128 and a not in (3,4,5,6,0)]
    return air[0] if air else None
st=os.path.expanduser("~/ss2/saves/svc/svc_c0_0.st")
print("공중 공격 act — Y(강) 한 번만 누름\n")
print("이륙+  옵션끔  옛규칙  새규칙")
same=True
for rel in (2,6,10,14,18,22,26):
    a=run(st,rel,OFF); b=run(st,rel,ON,0); c=run(st,rel,ON,1)
    if b!=c: same=False
    print("%-6d %-7s %-7s %s%s"%(rel,a,b,c,"" if b==c else "   ★다름"))
print()
print("판정: %s"%("공중 강 안 흔들림 — 옛/새 규칙 결과가 같다" if same else "★ 흔들린다"))
