import sys
def rd(t):
    f=open('svc_%s.ppm'%t,'rb'); f.readline(); w,h=map(int,f.readline().split()); f.readline()
    return w,h,f.read()
def band(t,y0,y1,x0=40,x1=250):
    w,h,D=rd(t)
    return w,[[D[((y*w+x)*3):((y*w+x)*3+3)] for x in range(x0,x1)] for y in range(y0,y1)]
a,b=sys.argv[1],sys.argv[2]
y0,y1=(int(sys.argv[3]),int(sys.argv[4])) if len(sys.argv)>4 else (75,112)
w,A=band(a,y0,y1); _,B=band(b,y0,y1)
best=None
for s in range(-100,101):
    tot=0;cnt=0
    for r in range(len(A)):
        for x in range(len(A[0])):
            xs=x+s
            if 0<=xs<len(A[0]):
                cnt+=1
                if A[r][x]==B[r][xs]: tot+=1
    sc=tot/max(cnt,1)
    if best is None or sc>best[1]: best=(s,sc)
print('%s -> %s  best shift %+d  match %.3f'%(a,b,best[0],best[1]))
