import sys
save=sys.argv[1] if len(sys.argv)>1 else "/home/dudu/ss2/saves/svc/svc_c0_0.st"
out=sys.argv[2] if len(sys.argv)>2 else "e3.txt"
pre=int(sys.argv[3]) if len(sys.argv)>3 else 120   # frames holding L to reach wall
n  =int(sys.argv[4]) if len(sys.argv)>4 else 40    # number of dumps
step=int(sys.argv[5]) if len(sys.argv)>5 else 6
L=[]
L.append("!tag wall")
L.append("!load "+save)
L.append("20 -")
L.append("%d L"%pre)
for i in range(n):
    L.append("%d -"%step)
    L.append("!s%03d"%i)
open(out,"w").write("\n".join(L)+"\n")
print(out,"frames",20+pre+n*step)
