import sys
tags=sys.argv[1:]
d=[open("svc_%s.ram"%t,"rb").read() for t in tags]
N=len(d[0])
def seq8(i): return [x[i] for x in d]
def seq16(i): return [x[i]|(x[i+1]<<8) for x in d]
def mono(v):
    inc=all(b>=a for a,b in zip(v,v[1:])) and v[-1]>v[0]
    dec=all(b<=a for a,b in zip(v,v[1:])) and v[-1]<v[0]
    return inc,dec
print("== 8-bit monotone ==")
for i in range(N):
    v=seq8(i); inc,dec=mono(v)
    if inc or dec: print("%04X"%i,"INC" if inc else "DEC",v)
print("== 16-bit LE monotone ==")
for i in range(N-1):
    v=seq16(i); inc,dec=mono(v)
    if (inc or dec) and max(v)-min(v)>2: print("%04X"%i,"INC" if inc else "DEC",v)
