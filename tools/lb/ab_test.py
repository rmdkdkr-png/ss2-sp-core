# -*- coding: utf-8 -*-
"""SP 를 켠 상태에서도 A+B 가 되나 — 재서 답한다."""
import io,os,shutil,subprocess,tempfile
RUN="/mnt/c/Claude/KOF R2 한글/tools/ngprun"
CORE=os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM=os.path.expanduser("~/ss2/rom/pristine/Last Blade, The (UE) [!].ngc")
ST=os.path.expanduser("~/ss2/saves/lb/lb_train.st")
def run(keys,opts,ph=0,hold=8):
    env=dict(os.environ); env["NGP_OPTS"]=opts
    sc=["!load %s"%ST,"!w t 370,386","%d -"%(20+ph),"%d %s"%(hold,keys),"70 -","!w off"]
    t=tempfile.mkdtemp(); r=os.path.join(t,"r.ngc"); shutil.copy(ROM,r)
    io.open(os.path.join(t,"s.txt"),"w",encoding="utf-8",newline="\n").write("\n".join(sc)+"\n")
    subprocess.run([RUN,CORE,r,os.path.join(t,"s.txt"),os.path.join(t,"g")],capture_output=True,text=True,env=env)
    p=os.path.join(t,"gt.csv")
    if not os.path.exists(p): return None
    rows=[l.split(",") for l in io.open(p).read().strip().splitlines()][1:]
    seq=[]; prev=None
    for r in rows:
        v=int(r[2])
        if v!=prev: seq.append(v); prev=v
    return [v for v in seq if v!=4]
ON="ngp_ss2sp=enabled,ngp_lbsp_engine=enabled"
OFF="ngp_ss2sp=enabled,ngp_lbsp_engine=disabled"
print("A = NGP B(킥) · B = NGP A(베기) · L1 = 레트로 L · R1 = 레트로 R\n")
for ph in (0,1):
  for name,keys,opts in [
      ("기준: NGP A+B 를 직접(대본 A B)", "A B", OFF),
      ("엔진 끔 · L1",                    "L1",  OFF),
      ("엔진 끔 · R1",                    "R1",  OFF),
      ("엔진 켬 · L1",                    "L1",  ON),
      ("엔진 켬 · R1(=SP)",               "R1",  ON),
      ("엔진 켬 · NGP A+B 직접",          "A B", ON)]:
    v=run(keys,opts,ph)
    print("위상%d %-30s act %s"%(ph,name,v if v else "(변화 없음)"))
