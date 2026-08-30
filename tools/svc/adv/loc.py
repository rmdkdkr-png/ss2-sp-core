import sys
from collections import Counter
def rd(t):
    f=open('svc_%s.ppm'%t,'rb'); f.readline(); w,h=map(int,f.readline().split()); f.readline()
    return w,h,f.read()
def pix(D,w,x,y):
    i=(y*w+x)*3; return (D[i],D[i+1],D[i+2])
Y0,Y1=116,160
w,h,D=rd('W00')
c=Counter(pix(D,w,x,y) for y in range(Y0,Y1) for x in range(30,260))
bg=c.most_common(1)[0][0]
cols=[sum(1 for y in range(Y0,Y1) if pix(D,w,x,y)!=bg) for x in range(w)]
runs=[];st=None
for x in range(30,260):
    if cols[x]>2 and st is None: st=x
    elif cols[x]<=2 and st is not None:
        if x-st>8: runs.append((st,x))
        st=None
print('W00 bg',bg,'runs',runs)
C={}
for k,(a,b) in enumerate(runs):
    C[k]=Counter(pix(D,w,x,y) for y in range(Y0,Y1) for x in range(a,b) if pix(D,w,x,y)!=bg)
    print(k,(a,b),C[k].most_common(8))
