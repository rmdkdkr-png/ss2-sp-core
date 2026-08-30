import sys
X0,X1,Y0,Y1=64,224,41,160
def readppm(p):
    f=open(p,'rb'); f.readline(); w,h=map(int,f.readline().split()); f.readline()
    return w,h,f.read(w*h*3)
def gray(p):
    w,h,d=readppm(p)
    g=[[ (d[((y*w)+x)*3]*3+d[((y*w)+x)*3+1]*6+d[((y*w)+x)*3+2])//10 for x in range(X0,X1)] for y in range(Y0,Y1)]
    return g
def best(p1,p2,y0,y1):
    A=gray(p1); B=gray(p2); W=X1-X0
    res=[]
    for s in range(-159,160):
        tot=0;n=0
        for y in range(y0,y1):
            ra=A[y]; rb=B[y]
            for x in range(W):
                xx=x+s
                if 0<=xx<W:
                    tot+=abs(ra[x]-rb[xx]); n+=1
        if n>W*(y1-y0)//3: res.append((tot/n,s))
    res.sort(); return res[:3]
pairs=[('d2/svc_C04.ppm','d2/svc_C09.ppm',80,80),('d2/svc_C09.ppm','d2/svc_C14.ppm',80,158),
       ('d2/svc_C09.ppm','d2/svc_C15.ppm',80,160),('d2/svc_A00.ppm','d2/svc_A20.ppm',80,62),
       ('d2/svc_B00.ppm','d2/svc_C15.ppm',0,160)]
for p1,p2,c1,c2 in pairs:
    r=best(p1,p2,0,60)   # 상단 60줄(배경)
    print('%s vs %s  cam %d->%d (delta %d)  top3 (score,shift)=%s'%(p1[-12:],p2[-12:],c1,c2,c2-c1,r))
