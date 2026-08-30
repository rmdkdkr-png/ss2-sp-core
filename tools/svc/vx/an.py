import sys,os
def readppm(p):
    d=open(p,"rb").read(); assert d[:2]==b"P6"
    i=2; vals=[]
    while len(vals)<3:
        while d[i] in b" \t\r\n": i+=1
        j=i
        while d[j] not in b" \t\r\n": j+=1
        vals.append(int(d[i:j])); i=j
    i+=1; w,h,mx=vals
    return w,h,d[i:i+w*h*3]
tags=sys.argv[1:]
imgs={}
for t in tags:
    w,h,rgb=readppm("svc_%s.ppm"%t); imgs[t]=rgb
W,H=288,184
# background = per-pixel median over all frames
n=len(tags)
bg=bytearray(W*H*3)
for idx in range(W*H*3):
    vals=sorted(imgs[t][idx] for t in tags)
    bg[idx]=vals[n//2]
X0,X1=64,224; Y0,Y1=118,164   # fighter band (framebuffer coords)
print("%-6s %6s %6s %8s %8s %8s"%("tag","092E","0AAE","dxmin","dxmax","dxcent"))
res=[]
for t in tags:
    rgb=imgs[t]
    cols=[]
    for x in range(X0,X1):
        c=0
        for y in range(Y0,Y1):
            o=(y*W+x)*3
            if abs(rgb[o]-bg[o])+abs(rgb[o+1]-bg[o+1])+abs(rgb[o+2]-bg[o+2])>60: c+=1
        cols.append(c)
    d=open("svc_%s.ram"%t,"rb").read()
    p1=d[0x092E]|(d[0x092F]<<8); p2=d[0x0AAE]
    # right-side blob: ignore columns near P1 (framebuffer 64+p1 +-16)
    lo=64+p1+22
    xs=[x for x in range(X0,X1) if cols[x-X0]>=3 and x>lo]
    if xs:
        mn,mx=min(xs),max(xs); ct=(mn+mx)/2.0
        print("%-6s %6d %6d %8d %8d %8.1f"%(t,p1,p2,mn-64,mx-64,ct-64))
        res.append((t,p1,p2,ct-64))
    else:
        print("%-6s %6d %6d %8s"%(t,p1,p2,"-"))
import statistics
if res:
    errs=[r[3]-r[2] for r in res]
    print("N=%d  mean(meas-RAM)=%.2f  sd=%.2f  min=%.1f max=%.1f"%(len(errs),statistics.mean(errs),statistics.pstdev(errs),min(errs),max(errs)))
