import sys,glob,re,os
def ram(p): return open(p,'rb').read()
def ppm(p):
    d=open(p,'rb').read()
    # P6\n W H\n255\n
    i=d.index(b'255\n')+4
    hdr=d[:i].split()
    w,h=int(hdr[1]),int(hdr[2])
    return w,h,d[i:]
def band(p,y0,y1):
    w,h,px=ppm(p)
    rows=[]
    for y in range(y0,y1):
        rows.append(px[(y*w)*3:((y+1)*w)*3])
    return w,rows
def scroll(pa,pb,y0=66,y1=92,maxs=40):
    w,ra=band(pa,y0,y1); _,rb=band(pb,y0,y1)
    best=None
    for s in range(-maxs,maxs+1):
        tot=0;cnt=0
        for A,B in zip(ra,rb):
            for x in range(maxs,w-maxs):
                xs=x+s
                ia=x*3; ib=xs*3
                tot+=abs(A[ia]-B[ib])+abs(A[ia+1]-B[ib+1])+abs(A[ia+2]-B[ib+2])
                cnt+=1
        v=tot/cnt
        if best is None or v<best[1]: best=(s,v)
    return best
