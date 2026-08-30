import glob
def rd(p):
    f=open(p,"rb"); f.readline(); w,h=map(int,f.readline().split()); f.readline()
    return w,h,f.read()
T=sorted(glob.glob("svc_H??.ppm"))
imgs=[rd(t) for t in T]
w,h,_=imgs[0]
X0,X1=64,224   # play area
def band(im,y0,y1):
    w,h,d=im
    return [[d[((y*w+x)*3):((y*w+x)*3+3)] for x in range(X0,X1)] for y in range(y0,y1)]
ref=band(imgs[0],66,92)
print("shift of background band vs H00 (positive = content moved left = camera moved right):")
res=[]
for i,im in enumerate(imgs):
    cur=band(im,66,92)
    best=None
    for s in range(-140,141):
        tot=0;cnt=0
        for r in range(len(ref)):
            for x in range(len(ref[0])):
                xs=x+s
                if 0<=xs<len(ref[0]):
                    cnt+=1
                    if ref[r][x]==cur[r][xs]: tot+=1
        if cnt>200:
            sc=tot/cnt
            if best is None or sc>best[1]: best=(s,sc)
    res.append(best)
    print(T[i],best)
