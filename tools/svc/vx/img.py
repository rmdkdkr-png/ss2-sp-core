import sys
def readppm(p):
    f=open(p,'rb'); assert f.readline().strip()==b'P6'
    w,h=map(int,f.readline().split()); f.readline()
    d=f.read(w*h*3); return w,h,d
def col(d,w,h,x,y0,y1):
    return bytes(d[((y*w)+x)*3+c] for y in range(y0,y1) for c in range(3))
def bestshift(p1,p2,y0,y1):
    w,h,a=readppm(p1); _,_,b=readppm(p2)
    best=None
    for s in range(-200,201):
        sc=0;n=0
        for x in range(40,w-40):
            xx=x+s
            if 0<=xx<w:
                ca=col(a,w,h,x,y0,y1); cb=col(b,w,h,xx,y0,y1)
                sc+=sum(abs(ca[i]-cb[i]) for i in range(len(ca))); n+=len(ca)
        if n: 
            v=sc/n
            if best is None or v<best[1]: best=(s,v)
    return best,w,h
import itertools
pairs=[('d2/svc_C09.ppm','d2/svc_C14.ppm'),('d2/svc_A00.ppm','d2/svc_A03.ppm'),('d2/svc_C04.ppm','d2/svc_C09.ppm')]
for p1,p2 in pairs:
    b,w,h=bestshift(p1,p2,20,60)
    print(p1,p2,'size',w,h,'best shift',b)
