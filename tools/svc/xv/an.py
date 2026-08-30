import sys
def ram(t): return open("svc_%s.ram"%t,"rb").read()
tags=sys.argv[1:]
R=[ram(t) for t in tags]
# bytes that differ across tags
diff=[i for i in range(len(R[0])) if len(set(r[i] for r in R))>1]
print("changed bytes:",len(diff))
for i in diff:
    vals=[r[i] for r in R]
    print("%04X"%i, vals)
