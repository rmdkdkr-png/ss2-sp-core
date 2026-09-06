# -*- coding: utf-8 -*-
"""캐릭터마다 체공·정점이 다른가 — «프레임 고정 창»이 옳은 자인지 가른다."""
import glob,io,os,shutil,subprocess,tempfile
RUN=os.path.expanduser("~/ss2/repo/ss2-sp-core/tools/kof/ngprun")
CORE=os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM=os.path.expanduser("~/ss2/rom/svc.ngc")
Y1,CHAR=0x0930,0x08A0
def run(st):
    env=dict(os.environ); env["NGP_OPTS"]="ngp_ss2sp=enabled,ngp_svcsp_engine=enabled"
    sc=["!load %s"%st,"!w t %X,%X"%(Y1,CHAR),"30 -","4 U","90 -","!w off"]
    t=tempfile.mkdtemp(); r=os.path.join(t,"r.ngc"); shutil.copy(ROM,r)
    io.open(os.path.join(t,"s.txt"),"w",encoding="utf-8",newline="\n").write("\n".join(sc)+"\n")
    subprocess.run([RUN,CORE,r,os.path.join(t,"s.txt"),os.path.join(t,"g")],capture_output=True,text=True,env=env)
    p=os.path.join(t,"gt.csv")
    if not os.path.exists(p): return None
    rows=[l.split(",") for l in io.open(p).read().strip().splitlines()][1:]
    air=[(int(r[0]),int(r[2])) for r in rows if int(r[2])!=128]
    ch=int(rows[0][3])
    if not air: return (ch,None,None,None)
    t0=air[0][0]; tl=air[-1][0]+1; ap=min(air,key=lambda x:x[1])
    return (ch, tl-t0, ap[0]-t0, tl-ap[0])
print("캐릭 체공 정점(상대) 정점=착지−n")
seen={}
for st in sorted(glob.glob(os.path.expanduser("~/ss2/saves/svc/svc_c*_0.st")))[:10]:
    q=run(st)
    if not q: print("  %-14s 못 쟀다"%os.path.basename(st)); continue
    ch,air,ap,back=q
    if air is None: print("  %-14s 안 떴다"%os.path.basename(st)); continue
    print("  %-14s id%-3d %3d   %3d      %3d"%(os.path.basename(st),ch,air,ap,back))
    seen[ch]=(air,ap,back)
print()
if seen:
    airs=sorted(set(v[0] for v in seen.values())); backs=sorted(set(v[2] for v in seen.values()))
    print("체공 값들: %s"%airs)
    print("정점=착지−n 값들: %s"%backs)
