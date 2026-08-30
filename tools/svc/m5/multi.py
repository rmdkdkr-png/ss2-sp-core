import subprocess,glob,os,re,sys
CORE="/home/dudu/ss2/repo/ss2-main/build/mednafen_ngp_libretro.so"
ROM="/home/dudu/ss2/rom/svc.ngc"
RUN="/home/dudu/ss2/repo/ss2-main/tools/svc/svcrun"
S="/home/dudu/ss2/saves/svc/"
saves=["svc_c0_0.st","svc_c1_0.st","svc_c2_0.st","svc_c3_0.st","svc_f2.st","svc_fight.st"]
pat=[("L",30),("R",60),("-",10),("U",10),("R",40),("A",6),("R",30),("U R",12),("B",6),
     ("L",50),("D",10),("L",40),("X",6),("-",20),("R",70),("Y",6),("L",25),("U",12),("R",55)]
for sv in saves:
    tag=sv.replace("svc_","").replace(".st","")
    for f in glob.glob("svc_M*.ram"): os.remove(f)
    for f in glob.glob("svc_M*.ppm"): os.remove(f)
    lines=["!load %s%s"%(S,sv),"60 -"]
    k=0
    for btn,rep in pat:
        for i in range(rep):
            lines.append("5 %s"%btn); k+=1; lines.append("!M%04d"%k)
    open("sM.txt","w").write("\n".join(lines)+"\n")
    subprocess.run([RUN,CORE,ROM,"sM.txt"],capture_output=True)
    fs=sorted(glob.glob("svc_M*.ram"),key=lambda p:int(re.search(r"M(\d+)\.ram",p).group(1)))
    d=[open(f,"rb").read() for f in fs]
    def w16(x,o): return x[o]|(x[o+1]<<8)
    p1s=[x[0x092E] for x in d]; p1h=[x[0x092F] for x in d]
    p2s=[x[0x0AAE] for x in d]; p2h=[x[0x0AAF] for x in d]
    p1w=[w16(x,0x0934) for x in d]; p2w=[w16(x,0x0AB4) for x in d]
    cam1=[a-b for a,b in zip(p1w,p1s)]; cam2=[a-b for a,b in zip(p2w,p2s)]
    bad=sum(1 for a,b in zip(cam1,cam2) if a!=b)
    ordmis=sum(1 for i in range(len(d)) if (p1s[i]-p2s[i]>0)!=(p1w[i]-p2w[i]>0))
    right=sum(1 for i in range(len(d)) if p1w[i]>p2w[i])
    print("%-10s n=%3d  092E[%3d..%3d] 092F max=%d | 0AAE[%3d..%3d] 0AAF max=%d | p1w[%d..%d] p2w[%d..%d] cam[%d..%d] cam1!=cam2:%d ordermismatch:%d  P1-right frames:%d"%(
        tag,len(d),min(p1s),max(p1s),max(p1h),min(p2s),max(p2s),max(p2h),min(p1w),max(p1w),min(p2w),max(p2w),min(cam1),max(cam1),bad,ordmis,right))
