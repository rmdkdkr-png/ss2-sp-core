import sys
def rd(p):
    f=open(p,"rb"); assert f.readline().strip()==b"P6"
    w,h=map(int,f.readline().split()); f.readline(); return w,h,f.read()
a=rd(sys.argv[1]); b=rd(sys.argv[2])
w,h=a[0],a[1]
y0,y1=int(sys.argv[3]),int(sys.argv[4])
cols=[]
for x in range(w):
    d=0
    for y in range(y0,y1):
        i=(y*w+x)*3
        if a[2][i:i+3]!=b[2][i:i+3]: d+=1
    cols.append(d)
run=[]
for x,c in enumerate(cols):
    if c>0: run.append(x)
# print contiguous ranges
out=[];s=None;p=None
for x in run:
    if s is None: s=x
    elif x!=p+1: out.append((s,p)); s=x
    p=x
if s is not None: out.append((s,p))
print("changed column ranges:",out)
