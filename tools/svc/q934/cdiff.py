import sys
def rd(t):
    f=open('svc_%s.ppm'%t,'rb'); f.readline(); w,h=map(int,f.readline().split()); f.readline()
    return w,h,f.read()
a,b=sys.argv[1],sys.argv[2]
w,h,A=rd(a); _,_,B=rd(b)
cols=[0]*w
rows=[0]*h
for y in range(h):
    for x in range(w):
        i=(y*w+x)*3
        if A[i:i+3]!=B[i:i+3]: cols[x]+=1; rows[y]+=1
print(a,'vs',b)
print('cols:', ''.join('#' if c>3 else ('.' if c else ' ') for c in cols))
print('rows:', ''.join('#' if c>3 else ('.' if c else ' ') for c in rows))
