# -*- coding: utf-8 -*-
"""짧은 누름 길이별 점프 성공률 — 걸쇠(최소 3프레임)가 값을 하나.

1-e8 이 앱에서 「방향 비트가 새로 서면 최소 3프레임 살린다」로 고쳤다.
그게 실제로 낫는지는 **코어가 N프레임 누름을 얼마나 받아 주나**로 갈린다.
캐릭터와 «위상»(누름이 어느 프레임에 떨어지나)을 흔들어 전수로 센다."""
import io,os,shutil,subprocess,tempfile
RUN=os.path.expanduser("~/ss2/repo/ss2-sp-core/tools/kof/ngprun")
CORE=os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM=os.path.expanduser("~/ss2/rom/svc.ngc"); Y1=0x0930
OPTS="ngp_ss2sp=enabled,ngp_svcsp_engine=enabled"
def jumped(st,n,ph):
    env=dict(os.environ); env["NGP_OPTS"]=OPTS
    sc=["!load %s"%st,"!w t %X"%Y1,"%d -"%(30+ph),"%d U"%n,"90 -","!w off"]
    t=tempfile.mkdtemp(); r=os.path.join(t,"r.ngc"); shutil.copy(ROM,r)
    io.open(os.path.join(t,"s.txt"),"w",encoding="utf-8",newline="\n").write("\n".join(sc)+"\n")
    subprocess.run([RUN,CORE,r,os.path.join(t,"s.txt"),os.path.join(t,"g")],capture_output=True,text=True,env=env)
    p=os.path.join(t,"gt.csv")
    if not os.path.exists(p): return None
    rows=[l.split(",") for l in io.open(p).read().strip().splitlines()][1:]
    return any(int(r[2])!=128 for r in rows)
STS=[("id0","svc_c0_0.st"),("id1","svc_c1_0.st"),("id9","svc_c3_0.st"),
     ("id10","svc_ct_반격_정지_10_0.st"),("id11","svc_ct_반격_정지_11_0.st"),
     ("id13","svc_ct_반격_정지_13_0.st")]
PH=range(6)   # 누름이 떨어지는 «위상» 여섯
print("누름 길이별 점프 성공 (캐릭 6 x 위상 6 = 36회)\n")
print("프레임  성공/36   실패한 캐릭")
for n in (1,2,3,4):
    ok=0; bad={}
    for nm,st in STS:
        stp=os.path.expanduser("~/ss2/saves/svc/"+st)
        for ph in PH:
            r=jumped(stp,n,ph)
            if r: ok+=1
            else: bad[nm]=bad.get(nm,0)+1
    print("  %d     %2d/36     %s"%(n,ok," ".join("%s x%d"%kv for kv in sorted(bad.items())) or "—"))
