import struct,zlib,sys
def topng(src,dst):
    f=open(src,"rb"); assert f.readline().strip()==b"P6"
    w,h=map(int,f.readline().split()); f.readline(); d=f.read()
    raw=b"".join(b"\x00"+d[y*w*3:(y+1)*w*3] for y in range(h))
    def ch(t,x):
        c=t+x; return struct.pack(">I",len(x))+c+struct.pack(">I",zlib.crc32(c))
    open(dst,"wb").write(b"\x89PNG\r\n\x1a\n"+ch(b"IHDR",struct.pack(">IIBBBBB",w,h,8,2,0,0,0))+ch(b"IDAT",zlib.compress(raw))+ch(b"IEND",b""))
for t in sys.argv[1:]: topng("svc_%s.ppm"%t,"svc_%s.png"%t)
