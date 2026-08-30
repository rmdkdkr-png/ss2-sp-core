import numpy as np
def img(p):
    f=open(p,"rb"); f.readline(); w,h=map(int,f.readline().split()); f.readline()
    a=np.frombuffer(f.read(w*h*3),dtype=np.uint8).reshape(h,w,3)
    return a
def shift(A,B,y0,y1,x0,x1,maxs=40):
    a=A[y0:y1,x0:x1].astype(np.int16)
    best=(1e9,0)
    for s in range(-maxs,maxs+1):
        bx0,bx1=x0+s,x1+s
        if bx0<0 or bx1>B.shape[1]: continue
        b=B[y0:y1,bx0:bx1].astype(np.int16)
        d=float(np.mean(np.abs(a-b)))
        if d<best[0]: best=(d,s)
    return best[1],best[0]
