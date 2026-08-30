import sys,zlib,struct
def readppm(p):
    d=open(p,"rb").read()
    # parse header
    parts=[];i=0
    while len(parts)<4:
        while d[i:i+1].isspace(): i+=1
        j=i
        while not d[j:j+1].isspace(): j+=1
        parts.append(d[i:j]); i=j
    i+=1
    w=int(parts[1]);h=int(parts[2])
    return w,h,bytearray(d[i:i+w*h*3])
def png(w,h,buf,out):
    raw=b"".join(b"\x00"+bytes(buf[y*w*3:(y+1)*w*3]) for y in range(h))
    def ch(t,data):
        c=struct.pack(">I",len(data))+t+data
        return c+struct.pack(">I",zlib.crc32(t+data)&0xffffffff)
    o=b"\x89PNG\r\n\x1a\n"+ch(b"IHDR",struct.pack(">IIBBBBB",w,h,8,2,0,0,0))+ch(b"IDAT",zlib.compress(raw,9))+ch(b"IEND",b"")
    open(out,"wb").write(o)
def line(w,h,buf,x,col):
    if 0<=x<w:
        for y in range(h):
            o=(y*w+x)*3; buf[o],buf[o+1],buf[o+2]=col
