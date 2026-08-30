import numpy as np
def ppm(p):
    d=open(p,'rb').read(); i=d.index(b'255\n')+4
    h=d[:i].split(); w,ht=int(h[1]),int(h[2])
    a=np.frombuffer(d[i:i+w*ht*3],dtype=np.uint8).reshape(ht,w,3).astype(np.int16)
    return a
def scroll(pa,pb,y0=66,y1=92,maxs=60):
    A=ppm(pa)[y0:y1]; B=ppm(pb)[y0:y1]
    w=A.shape[1]; best=None
    for s in range(-maxs,maxs+1):
        a=A[:,maxs:w-maxs]; b=B[:,maxs+s:w-maxs+s]
        v=np.abs(a-b).mean()
        if best is None or v<best[1]: best=(s,v)
    return best
