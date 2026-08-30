import sys
def rd(t):
    return open('svc_%s.ram'%t,'rb').read()
tags=['L0','L20','L40','L60','L90','L120']
d=[rd(t) for t in tags]
n=min(len(x) for x in d)
print('len',n)
# strictly decreasing L0>L20>L40>=L60, and L60==L90==L120 (wall saturation)
cands=[]
for o in range(n):
    v=[x[o] for x in d]
    if v[0]>v[1]>v[2]>=v[3] and v[3]==v[4]==v[5] and v[0]-v[3]>=8:
        cands.append((o,v))
for o,v in cands:
    print('0x%04X'%o, v)
