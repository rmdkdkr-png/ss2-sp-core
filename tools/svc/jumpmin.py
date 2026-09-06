# -*- coding: utf-8 -*-
"""점프가 나가는 «최소 누름 프레임» — 엔진 켬/끔으로 갈라 본다.

유저: 「점프버튼을 짧게 누르면 안 뛰거든. 너무 길게 눌러야 해서.」
가를 것: 게임이 원래 그런가, 아니면 우리 엔진이 위 방향을 삼키거나 늦추나."""
import io,os,shutil,subprocess,tempfile
RUN=os.path.expanduser("~/ss2/repo/ss2-sp-core/tools/kof/ngprun")
CORE=os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM=os.path.expanduser("~/ss2/rom/svc.ngc"); Y1=0x0930
CASES=[("엔진 끔",        "ngp_ss2sp=enabled,ngp_svcsp_engine=disabled"),
       ("엔진 켬",        "ngp_ss2sp=enabled,ngp_svcsp_engine=enabled"),
       ("엔진 켬+착지선입력","ngp_ss2sp=enabled,ngp_svcsp_engine=enabled,ngp_svcsp_land=enabled"),
       ("ss2sp 통째 끔",  "ngp_ss2sp=disabled")]
def jumped(st,n,opts):
    env=dict(os.environ); env["NGP_OPTS"]=opts
    sc=["!load %s"%st,"!w t %X"%Y1,"30 -","%d U"%n,"90 -","!w off"]
    t=tempfile.mkdtemp(); r=os.path.join(t,"r.ngc"); shutil.copy(ROM,r)
    io.open(os.path.join(t,"s.txt"),"w",encoding="utf-8",newline="\n").write("\n".join(sc)+"\n")
    subprocess.run([RUN,CORE,r,os.path.join(t,"s.txt"),os.path.join(t,"g")],capture_output=True,text=True,env=env)
    p=os.path.join(t,"gt.csv")
    if not os.path.exists(p): return None
    rows=[l.split(",") for l in io.open(p).read().strip().splitlines()][1:]
    return any(int(r[2])!=128 for r in rows)
for stn in ("svc_c0_0.st","svc_ct_반격_정지_10_0.st"):
    st=os.path.expanduser("~/ss2/saves/svc/"+stn)
    print("== %s =="%stn)
    print("  누름프레임  " + "".join("%-18s"%n for n,_ in CASES))
    for n in (1,2,3,4,5,6,8,10):
        row=[]
        for _,opts in CASES:
            r=jumped(st,n,opts)
            row.append("뜀" if r else ("못잼" if r is None else "안 뜀"))
        print("  %-10d "%n + "".join("%-18s"%c for c in row))
    print()
