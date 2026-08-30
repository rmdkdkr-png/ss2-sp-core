import sys,zlib,struct
def rd(p):
    f=open(p,"rb"); f.readline(); w,h=map(int,f.readline().split()); f.readline()
    return w,h,f.read(w*h*3)
def png(name,w,h,d,scale=1):
    if scale>1:
        nd=bytearray()
        for y in range(h):
            row=bytearray()
            for x in range(w):
                i=(y*w+x)*3
                row+=d[i:i+3]*scale
            nd+=row*scale
        d=bytes(nd); w*=scale; h*=scale
    raw=b"".join(b"\x00"+d[y*w*3:(y+1)*w*3] for y in range(h))
    def ch(t,da):
        c=t+da; return struct.pack(">I",len(da))+c+struct.pack(">I",zlib.crc32(c))
    open(name,"wb").write(b"\x89PNG\r\n\x1a\n"+ch(b"IHDR",struct.pack(">IIBBBBB",w,h,8,2,0,0,0))+ch(b"IDAT",zlib.compress(raw))+ch(b"IEND",b""))
