S={}
for t in 'NLR':
    S[t]=[open('svc_%s%02d.ram'%(t,i),'rb').read() for i in range(21)]
n=len(S['N'][0])
def ser(t,o): return [S[t][i][o] for i in range(21)]
def mono(v):
    return all(b>=a for a,b in zip(v,v[1:])), all(b<=a for a,b in zip(v,v[1:]))
out=[]
for o in range(n):
    R=ser('R',o); L=ser('L',o)
    ri,rd=mono(R); li,ld=mono(L)
    dR=R[-1]-R[0]; dL=L[-1]-L[0]
    if ri and ld and dR>=6 and dL<=-6:
        out.append((o,dR,dL,R,L,ser('N',o)))
print(len(out),'candidates')
for o,dR,dL,R,L,N in out:
    print('0x%04X dR=%+d dL=%+d'%(o,dR,dL))
    print('   R',R)
    print('   L',L)
    print('   N',N)
