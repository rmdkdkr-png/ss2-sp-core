import glob
def load(tag): return open('svc_%s.ram'%tag,'rb').read()
S={}
for t in 'NLR':
    S[t]=[load('%s%02d'%(t,i)) for i in range(21)]
n=len(S['N'][0])
def ser(t,o): return [S[t][i][o] for i in range(21)]
def mono(v):
    inc=all(b>=a for a,b in zip(v,v[1:])); dec=all(b<=a for a,b in zip(v,v[1:]))
    return inc,dec
cands=[]
for o in range(n):
    R=ser('R',o); L=ser('L',o); N=ser('N',o)
    dR=R[-1]-R[0]; dL=L[-1]-L[0]; dN=max(N)-min(N)
    ri,rd=mono(R); li,ld=mono(L)
    if dR>=8 and dL<=-8 and ri and ld and dN<=2:
        cands.append((o,R[0],R[-1],L[-1],N[0],N[-1]))
print('P1-like (R up, L down, N flat):')
for c in cands: print('  0x%04X  R:%d->%d  L:->%d  N:%d->%d'%c)
