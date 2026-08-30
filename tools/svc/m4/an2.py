def rd(t): return open('svc_%s.ram'%t,'rb').read()
L=['L0','L20','L40','L60']
R=['L0','R20','R40','R120','R180']
dl=[rd(t) for t in L]; dr=[rd(t) for t in R]
n=16384
out=[]
for o in range(n):
    a=[x[o] for x in dl]; b=[x[o] for x in dr]
    dec = all(a[i]>=a[i+1] for i in range(len(a)-1)) and a[0]-a[-1]>=10
    inc = all(b[i]<=b[i+1] for i in range(len(b)-1)) and b[-1]-b[0]>=10
    if dec and inc: out.append((o,a,b))
print('both-direction candidates:',len(out))
for o,a,b in out: print('0x%04X'%o,'L:',a,'R:',b)
