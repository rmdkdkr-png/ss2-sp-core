import io,os,shutil,subprocess,tempfile
RUN=os.path.expanduser("~/ss2/repo/ss2-sp-core/tools/kof/ngprun")
CORE=os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM=os.path.expanduser("~/ss2/rom/svc.ngc"); Y1=0x0930
ON="ngp_ss2sp=enabled,ngp_svcsp_engine=enabled,ngp_svcsp_land=enabled"
def y(st,attack):
    env=dict(os.environ); env["NGP_OPTS"]=ON
    sc=["!load %s"%st,"!w t %X"%Y1,"30 -","4 U"]
    if attack: sc+=["2 -","3 Y"]
    sc+=["100 -","!w off"]
    t=tempfile.mkdtemp(); r=os.path.join(t,"r.ngc"); shutil.copy(ROM,r)
    io.open(os.path.join(t,"s.txt"),"w",encoding="utf-8",newline="\n").write("\n".join(sc)+"\n")
    subprocess.run([RUN,CORE,r,os.path.join(t,"s.txt"),os.path.join(t,"g")],capture_output=True,text=True,env=env)
    rows=[l.split(",") for l in io.open(os.path.join(t,"gt.csv")).read().strip().splitlines()][1:]
    air=[(int(r[0]),int(r[2])) for r in rows if int(r[2])!=128]
    if not air: return None,[]
    t0=air[0][0]
    return t0,[(f-t0,v) for f,v in air]
for name,st in [("id0","svc_c0_0.st"),("id10","svc_ct_반격_정지_10_0.st")]:
    for atk in (False,True):
        t0,q=y(os.path.expanduser("~/ss2/saves/svc/"+st),atk)
        lab="공중공격 있음" if atk else "그냥 점프"
        print("%s %s  체공 %d"%(name,lab,len(q)))
        print("   "+" ".join("%d:%d"%(f,v) for f,v in q))
    print()
