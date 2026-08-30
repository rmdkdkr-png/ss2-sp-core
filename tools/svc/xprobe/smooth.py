import sys,glob
pre=sys.argv[1]; 
tags=sorted(glob.glob("svc_%s??.ram"%pre))
d=[open(t,"rb").read() for t in tags]
N=len(d[0])
def rep(v,lab,off):
    print("%04X %s"%(off,lab),v)
print("### %s  n=%d"%(pre,len(d)))
print("-- 8bit smooth monotone --")
for i in range(N):
    v=[x[i] for x in d]
    dl=[b-a for a,b in zip(v,v[1:])]
    if all(x>=0 for x in dl) and sum(dl)>=8 and max(dl)<=12: rep(v,"INC",i)
    elif all(x<=0 for x in dl) and sum(dl)<=-8 and min(dl)>=-12: rep(v,"DEC",i)
print("-- 16bit LE smooth monotone --")
for i in range(N-1):
    v=[x[i]|(x[i+1]<<8) for x in d]
    dl=[b-a for a,b in zip(v,v[1:])]
    if all(x>=0 for x in dl) and 8<=sum(dl)<=4000 and max(dl)<=600: rep(v,"INC",i)
    elif all(x<=0 for x in dl) and -4000<=sum(dl)<=-8 and min(dl)>=-600: rep(v,"DEC",i)
