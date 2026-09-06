import io,os,shutil,subprocess,tempfile
RUN=os.path.expanduser("~/ss2/repo/ss2-sp-core/tools/kof/ngprun")
CORE=os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM=os.path.expanduser("~/ss2/rom/svc.ngc"); ST=os.path.expanduser("~/ss2/saves/svc/svc_c0_0.st")
ACT=0x0968
ON="ngp_ss2sp=enabled,ngp_svcsp_engine=enabled"
OFF="ngp_ss2sp=enabled,ngp_svcsp_engine=disabled"
def run(keys,opts,ph=0,hold=8,lead=30):
    env=dict(os.environ); env["NGP_OPTS"]=opts
    sc=["!load %s"%ST,"!w t %X"%ACT,"%d -"%(lead+ph),
        ("%d %s"%(hold,keys)) if keys!="-" else ("%d -"%hold),"70 -","!w off"]
    t=tempfile.mkdtemp(); r=os.path.join(t,"r.ngc"); shutil.copy(ROM,r)
    io.open(os.path.join(t,"s.txt"),"w",encoding="utf-8",newline="\n").write("\n".join(sc)+"\n")
    subprocess.run([RUN,CORE,r,os.path.join(t,"s.txt"),os.path.join(t,"g")],capture_output=True,text=True,env=env)
    p=os.path.join(t,"gt.csv")
    if not os.path.exists(p): return None
    rows=[l.split(",") for l in io.open(p).read().strip().splitlines()][1:]
    o=[];prev=None
    for r2 in rows:
        f,a=int(r2[0]),int(r2[2])
        if f<lead: prev=a; continue
        if a!=prev: o.append(a); prev=a
    return o[:3]
print("SvC 버튼 계약 — 강약구분 폐기 뒤 (act 0x0968)\n")
for ph in (0,1):
  for nm,k in [("대조군 아무것도","-"),("NGP A+B 직접","A B"),("L1","L1"),("R1","R1"),("Y","Y"),("X","X")]:
    for lab,o in (("엔진 켬",ON),("엔진 끔",OFF)):
        v=run(k,o,ph)
        print("  위상%d %-16s %-7s %s"%(ph,nm,lab,v if v else "(변화 없음)"))
  print()
