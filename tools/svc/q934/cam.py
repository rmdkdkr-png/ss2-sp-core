import sys
def rd(t):
    f=open('svc_%s.ppm'%t,'rb'); f.readline(); w,h=map(int,f.readline().split()); f.readline()
    return w,h,f.read()
def band(t,y0,y1,x0,x1):
    w,h,D=rd(t)
    return [[D[((y*w+x)*3):((y*w+x)*3+3)] for x in range(x0,x1)] for y in range(y0,y1)]
def shift(a,b,y0=72,y1=112,x0=45,x1=245,rng=90):
    A=band(a,y0,y1,x0,x1); B=band(b,y0,y1,x0,x1)
    W=len(A[0]); best=(0,-1)
    for s in range(-rng,rng+1):
        tot=cnt=0
        for r in range(len(A)):
            Ar=A[r];Br=B[r]
            for x in range(W):
                xs=x+s
                if 0<=xs<W:
                    cnt+=1
                    if Ar[x]==Br[xs]: tot+=1
        sc=tot/max(cnt,1)
        if sc>best[1]: best=(s,sc)
    return best
def g(t,o):
    b=open('svc_%s.ram'%t,'rb').read(); return b[o]|(b[o+1]<<8)
tags=['T%02d'%i for i in range(int(sys.argv[1]))]
print('tag   x1   x2  x2-x1   camShiftFromPrev  match   dCam(pred=-d(x2-x1))')
prev=None
for t in tags:
    x1=g(t,0x092E); x2=g(t,0x0934); d=x2-x1
    if prev is None:
        print('%s %4d %4d %5d'%(t,x1,x2,d))
    else:
        s,m=shift(prev,t)
        print('%s %4d %4d %5d   shift=%+4d m=%.2f   d(x2-x1)=%+4d'%(t,x1,x2,d,s,m,d-pd))
    prev=t; pd=d
