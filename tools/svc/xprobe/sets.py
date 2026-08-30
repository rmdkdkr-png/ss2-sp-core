def load(p): return [open("svc_%s.ram"%t,"rb").read() for t in p]
D=load(["D%d"%i for i in range(9)])
E=load(["E%d"%i for i in range(9)])
N=len(D[0])
chD={i for i in range(N) if len({x[i] for x in D})>1}
chE={i for i in range(N) if len({x[i] for x in E})>1}
only=sorted(chD-chE)
print("D-only (P1 moved, not while P1 idle):",len(only))
for i in only:
    print("%04X"%i,[x[i] for x in D])
