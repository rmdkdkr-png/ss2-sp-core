import sys
def rd(t): return open("svc_%s.ram"%t,"rb").read()
tags=sys.argv[1:]
d=[rd(t) for t in tags]
n=0
for i in range(len(d[0])):
    vals=[x[i] for x in d]
    if len(set(vals))>1:
        n+=1
        if n<=400: print("%04X"%i, vals)
print("changed bytes:",n)
