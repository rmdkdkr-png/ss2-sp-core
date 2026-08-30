import subprocess,glob,os,re
CORE="/home/dudu/ss2/repo/ss2-main/build/mednafen_ngp_libretro.so"
ROM="/home/dudu/ss2/rom/svc.ngc"; RUN="/home/dudu/ss2/repo/ss2-main/tools/svc/svcrun"
S="/home/dudu/ss2/saves/svc/"
tot=0; engface=0; trueface=0
for sv in ["svc_c0_0.st","svc_c1_0.st","svc_c2_0.st","svc_c3_0.st","svc_f2.st","svc_fight.st"]:
    for f in glob.glob("svc_M*.*"): os.remove(f)
    lines=["!load %s%s"%(S,sv),"60 -"]; k=0
    for btn,rep in [("L",30),("R",60),("-",10),("U",10),("R",40),("A",6),("R",30),("U R",12),
                    ("B",6),("L",50),("D",10),("L",40),("X",6),("-",20),("R",70),("Y",6),("L",25),("U",12),("R",55)]:
        for i in range(rep):
            lines.append("5 %s"%btn); k+=1; lines.append("!M%04d"%k)
    open("sM.txt","w").write("\n".join(lines)+"\n")
    subprocess.run([RUN,CORE,ROM,"sM.txt"],capture_output=True)
    fs=sorted(glob.glob("svc_M*.ram"),key=lambda p:int(re.search(r"M(\d+)\.ram",p).group(1)))
    for f in fs:
        x=open(f,"rb").read(); tot+=1
        w=lambda o: x[o]|(x[o+1]<<8)
        if w(0x092E) > w(0x0934): engface+=1          # 현재 엔진 식
        if w(0x0934) > w(0x0AB4): trueface+=1          # 올바른 식 (P1 world > P2 world)
print("samples=%d  engine svc_facing_left()=1 : %d    correct P1w>P2w : %d"%(tot,engface,trueface))
