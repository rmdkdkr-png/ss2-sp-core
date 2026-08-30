import sys
def rd(t):
    f=open('svc_%s.ppm'%t,'rb'); f.readline(); w,h=map(int,f.readline().split()); f.readline()
    return w,h,f.read()
def px(D,w,x,y):
    i=(y*w+x)*3; return D[i],D[i+1],D[i+2]
def measure(t, y0=110, y1=165):
    w,h,D=rd(t)
    # background = modal color in band
    from collections import Counter
    c=Counter()
    for y in range(y0,y1):
        for x in range(0,w): c[px(D,w,x,y)]+=1
    bg=c.most_common(1)[0][0]
    cols=[]
    for x in range(w):
        n=0
        for y in range(y0,y1):
            if px(D,w,x,y)!=bg: n+=1
        cols.append(n)
    return w,bg,cols
if __name__=='__main__':
    for t in sys.argv[1:]:
        w,bg,cols=measure(t)
        s=''.join('#' if c>6 else ('.' if c else ' ') for c in cols)
        print(t,'bg',bg)
        print('   ',s)
