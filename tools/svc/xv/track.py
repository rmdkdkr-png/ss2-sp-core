import sys
def rd(p):
    f=open(p,"rb"); f.readline(); w,h=map(int,f.readline().split()); f.readline()
    return w,h,f.read(w*h*3)
AX0,AX1=64,224   # active screen columns (NGPC 160 wide)
def band(d,w,y0,y1,x0,x1):
    return [[d[(y*w+x)*3:(y*w+x)*3+3] for x in range(x0,x1)] for y in range(y0,y1)]
def best_shift(A,B,maxs=24):
    # A,B lists of rows; find s minimizing mismatch of A[y][x] vs B[y][x+s]
    best=None;bs=0;h=len(A);wd=len(A[0])
    for s in range(-maxs,maxs+1):
        n=0;m=0
        for y in range(h):
            for x in range(wd):
                xx=x+s
                if 0<=xx<wd:
                    n+=1
                    if A[y][x]!=B[y][xx]: m+=1
        if n<wd*h*0.5: continue
        r=m/n
        if best is None or r<best: best=r;bs=s
    return bs,best
