import sys,zlib,struct
def rd(p):
    d=open(p,'rb').read(); i=0; f=[]
    while len(f)<4:
        while d[i] in b' \t\r\n': i+=1
        j=i
        while d[j] not in b' \t\r\n': j+=1
        f.append(d[i:j]); i=j
    i+=1; w=int(f[1]); h=int(f[2]); return w,h,d[i:i+w*h*3]
def png(path,w,h,rows):
    raw=b''.join(b'\x00'+r for r in rows)
    def ck(t,d):
        c=t+d; return struct.pack('>I',len(d))+c+struct.pack('>I',zlib.crc32(c)&0xffffffff)
    open(path,'wb').write(b'\x89PNG\r\n\x1a\n'+ck(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))
        +ck(b'IDAT',zlib.compress(raw,9))+ck(b'IEND',b''))
# crop.py in.ppm out.png x0 y0 x1 y1 [scale]
src,dst=sys.argv[1],sys.argv[2]
x0,y0,x1,y1=[int(v) for v in sys.argv[3:7]]
sc=int(sys.argv[7]) if len(sys.argv)>7 else 1
w,h,D=rd(src)
rows=[]
for y in range(y0,y1):
    r=bytearray()
    for x in range(x0,x1):
        o=(y*w+x)*3
        r+=D[o:o+3]*sc
    for _ in range(sc): rows.append(bytes(r))
png(dst,(x1-x0)*sc,(y1-y0)*sc,rows)
print(dst,(x1-x0)*sc,(y1-y0)*sc)
