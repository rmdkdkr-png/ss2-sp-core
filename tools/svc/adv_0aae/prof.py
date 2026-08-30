import sys
from ppm2png import readppm

# FB 288x184, NGPC screen 160x152 centred -> x0=64, y0=16
X0,Y0=64,16
def load(p):
    w,h,d=readppm(p); return w,h,d

def colmask(a,b,w,y1,y2,x1,x2):
    cols=[0]*w
    for y in range(y1,y2):
        base=y*w*3
        for x in range(x1,x2):
            i=base+x*3
            if a[i]!=b[i] or a[i+1]!=b[i+1] or a[i+2]!=b[i+2]:
                cols[x]+=1
    return cols

def blobs(cols,x1,x2,thr=1,gap=4):
    out=[];cur=None;lastx=None
    for x in range(x1,x2):
        if cols[x]>=thr:
            if cur is None or x-lastx>gap:
                if cur is not None: out.append(cur)
                cur=[x,x,0]
            cur[1]=x; cur[2]+=cols[x]; lastx=x
    if cur is not None: out.append(cur)
    return out

if __name__=='__main__':
    y1,y2=int(sys.argv[1]),int(sys.argv[2])
    ref=sys.argv[3]
    w,h,A=load(ref)
    for p in sys.argv[4:]:
        _,_,B=load(p)
        cols=colmask(A,B,w,y1,y2,X0,X0+160)
        bs=blobs(cols,X0,X0+160)
        s=" ".join("[%d..%d screen %d..%d w=%d]"%(b[0],b[1],b[0]-X0,b[1]-X0,b[2]) for b in bs)
        print(f"{ref} vs {p} rows{y1}-{y2}: {s}")
