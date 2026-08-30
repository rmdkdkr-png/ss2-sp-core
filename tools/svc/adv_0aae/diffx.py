import sys
from ppm2png import readppm

def colprofile(a,b,w,h):
    cols=[0]*w
    rows=[0]*h
    for y in range(h):
        base=y*w*3
        for x in range(w):
            i=base+x*3
            if a[i]!=b[i] or a[i+1]!=b[i+1] or a[i+2]!=b[i+2]:
                cols[x]+=1; rows[y]+=1
    return cols,rows

ctl=sys.argv[1]
w,h,A=readppm(ctl)
for p in sys.argv[2:]:
    w2,h2,B=readppm(p)
    cols,rows=colprofile(A,B,w,h)
    nz=[x for x,c in enumerate(cols) if c>0]
    nzr=[y for y,c in enumerate(rows) if c>0]
    tot=sum(cols)
    if not nz:
        print(f"{p}: identical")
        continue
    cen=sum(x*cols[x] for x in nz)/tot
    print(f"{p}: changed px={tot}  x=[{nz[0]}..{nz[-1]}] centroid={cen:.1f}  y=[{nzr[0]}..{nzr[-1]}]")
