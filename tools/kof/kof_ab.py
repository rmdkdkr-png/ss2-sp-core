# -*- coding: utf-8 -*-
"""KOF R-2 — 「끔」에서 R 이 죽고 L 이 A+B 를 받는지 «화면으로» 확인한다.
이미 배포된 판이라 쓰던 사람 손이 바뀐다. 말이 아니라 표로 댄다."""
import io,os,shutil,subprocess,tempfile
RUN="/mnt/c/Claude/KOF R2 한글/tools/ngprun"
CORE=os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM=os.path.expanduser("~/ss2/rom/kofr2.ngc")
ST=os.path.expanduser("~/ss2/saves/kof/kof_c00.st")
ACT=0x0D3C
def run(keys,opts,ph=0,hold=8):
    env=dict(os.environ); env["NGP_OPTS"]=opts
    sc=["!load %s"%ST,"!w t %X"%ACT,"%d -"%(20+ph),"%d %s"%(hold,keys),"70 -","!w off"]
    t=tempfile.mkdtemp(); r=os.path.join(t,"r.ngc"); shutil.copy(ROM,r)
    io.open(os.path.join(t,"s.txt"),"w",encoding="utf-8",newline="\n").write("\n".join(sc)+"\n")
    subprocess.run([RUN,CORE,r,os.path.join(t,"s.txt"),os.path.join(t,"g")],capture_output=True,text=True,env=env)
    p=os.path.join(t,"gt.csv")
    if not os.path.exists(p): return None
    rows=[l.split(",") for l in io.open(p).read().strip().splitlines()][1:]
    out=[];prev=None
    for r in rows:
        v=int(r[2])
        if v!=prev: out.append(v); prev=v
    rest=out[0] if out else None
    return [v for v in out if v!=rest]
ON="ngp_ss2sp=enabled,ngp_kofsp_engine=enabled"
OFF="ngp_ss2sp=enabled,ngp_kofsp_engine=disabled"
print("KOF R-2 · 세이브 kof_c00 · act 0x0D3C (쉼 값은 뺀 목록)\n")
for ph in (0,1):
  for name,keys,opts in [
      ("기준: NGP A+B 직접(대본 A B)","A B",OFF),
      ("엔진 끔 · L1","L1",OFF),
      ("엔진 끔 · R1","R1",OFF),
      ("엔진 켬 · L1","L1",ON),
      ("엔진 켬 · R1(=SP)","R1",ON)]:
    v=run(keys,opts,ph)
    print("  위상%d %-28s act %s"%(ph,name,v if v else "(변화 없음)"))
