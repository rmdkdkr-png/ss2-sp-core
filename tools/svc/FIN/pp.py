import sys
def rd(p):
    d=open(p,'rb').read()
    # P6\n<w> <h>\n255\n
    i=0; f=[]
    while len(f)<4:
        while d[i] in b' \t\r\n': i+=1
        j=i
        while d[j] not in b' \t\r\n': j+=1
        f.append(d[i:j]); i=j
    i+=1
    w=int(f[1]); h=int(f[2])
    return w,h,d[i:i+w*h*3]
def cols(a,b):
    w,h,A=rd(a); w2,h2,B=rd(b)
    assert (w,h)==(w2,h2)
    c=[0]*w
    for y in range(h):
        base=y*w*3
        for x in range(w):
            o=base+x*3
            if A[o]!=B[o] or A[o+1]!=B[o+1] or A[o+2]!=B[o+2]: c[x]+=1
    return w,h,c
if __name__=='__main__':
    w,h,c=cols(sys.argv[1],sys.argv[2])
    runs=[]; s=None
    for x in range(w):
        if c[x]>0 and s is None: s=x
        elif c[x]==0 and s is not None: runs.append((s,x-1)); s=None
    if s is not None: runs.append((s,w-1))
    print('size',w,h)
    print('changed column runs (fb x):',runs)
    print('  -> game x (fb-64):',[(a-64,b-64) for a,b in runs])
    print('  centers game x:',[round((a+b)/2-64,1) for a,b in runs])
