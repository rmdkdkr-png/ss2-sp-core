import subprocess,glob,os,re
CORE="/home/dudu/ss2/repo/ss2-main/build/mednafen_ngp_libretro.so"
ROM="/home/dudu/ss2/rom/svc.ngc"; RUN="/home/dudu/ss2/repo/ss2-main/tools/svc/svcrun"
S="/home/dudu/ss2/saves/svc/"
def run(lines):
    for f in glob.glob("svc_N*.*"): os.remove(f)
    open("sN.txt","w").write("\n".join(lines)+"\n")
    subprocess.run([RUN,CORE,ROM,"sN.txt"],capture_output=True)
    fs=sorted(glob.glob("svc_N*.ram"),key=lambda p:int(re.search(r"N(\d+)\.ram",p).group(1)))
    return [open(f,"rb").read() for f in fs]
# 1) which byte responds to input, per save
print("--- input responsiveness (hold L 300f) ---")
for sv in ["svc_c0_0.st","svc_c1_0.st","svc_c2_0.st","svc_c3_0.st","svc_f2.st","svc_fight.st"]:
    L=["!load %s%s"%(S,sv),"60 -"]
    for i in range(30): L.append("10 L"); L.append("!N%03d"%i)
    d=run(L)
    a=[x[0x092E] for x in d]; b=[x[0x0AAE] for x in d]
    print("%-12s 092E %3d->%3d (min %d)   0AAE %3d->%3d (min %d)"%(sv,a[0],a[-1],min(a),b[0],b[-1],min(b)))
# 2) long mash run, invariant check incl. round resets
print("--- long mash 6000f, invariants ---")
L=["!load %ssvc_c0_0.st"%S,"60 -"]; k=0
seq=["A","B","X","Y","R A","L B","R","L","U","D","-"]
for i in range(300):
    L.append("20 %s"%seq[i%len(seq)]); k+=1; L.append("!N%04d"%k)
d=run(L)
w=lambda x,o: x[o]|(x[o+1]<<8)
bad2f=sum(1 for x in d if x[0x092F] or x[0x0AAF])
badcam=sum(1 for x in d if (w(x,0x0934)-x[0x092E])!=(w(x,0x0AB4)-x[0x0AAE]))
badord=sum(1 for x in d if ((x[0x092E]-x[0x0AAE])>0)!=((w(x,0x0934)-w(x,0x0AB4))>0))
hp=[x[0x08CF] for x in d]
resets=sum(1 for i in range(1,len(d)) if hp[i]>hp[i-1]+8)
eng=sum(1 for x in d if w(x,0x092E)>w(x,0x0934))
cor=sum(1 for x in d if w(x,0x0934)>w(x,0x0AB4))
print("n=%d  092F/0AAF nonzero:%d  cam mismatch:%d  order mismatch:%d  hp-resets(round/char change):%d"%(len(d),bad2f,badcam,badord,resets))
print("engine face=1 : %d/%d   corrected face=1 : %d/%d"%(eng,len(d),cor,len(d)))
print("hp2 seen:",sorted(set(hp))[:12],"... chr seen:",sorted(set(x[0x08A0] for x in d)))
