import sys
# usage: mk4.py out.txt save prefix dir prelen ndump step
out,save,pref,d,pre,n,step = sys.argv[1],sys.argv[2],sys.argv[3],sys.argv[4],int(sys.argv[5]),int(sys.argv[6]),int(sys.argv[7])
L=["!tag "+pref, "!load "+save, "20 -", "%d %s"%(pre,d)]
for i in range(n):
    L.append("%d -"%step)
    L.append("!%s%03d"%(pref,i))
open(out,"a").write("\n".join(L)+"\n")
print(out,pref,"end frame",20+pre+n*step)
