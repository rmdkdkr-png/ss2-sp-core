import zlib,struct,sys
def readppm(p):
    d=open(p,"rb").read()
    # P6\n<w> <h>\n255\n
    i=0; fields=[]
    while len(fields)<4:
        while d[i:i+1].isspace(): i+=1
        j=i
        while not d[j:j+1].isspace(): j+=1
        fields.append(d[i:j]); i=j
    i+=1
    w=int(fields[1]); h=int(fields[2])
    return w,h,bytearray(d[i:i+w*h*3])
def png(path,w,h,px,scale=2):
    rows=[]
    for y in range(h):
        for _ in range(scale):
            row=bytearray([0])
            for x in range(w):
                p=px[(y*w+x)*3:(y*w+x)*3+3]
                row+=p*scale
            rows.append(bytes(row))
    raw=b"".join(rows)
    def ch(t,d):
        return struct.pack(">I",len(d))+t+d+struct.pack(">I",zlib.crc32(t+d)&0xffffffff)
    out=b"\x89PNG\r\n\x1a\n"+ch(b"IHDR",struct.pack(">IIBBBBB",w*scale,h*scale,8,2,0,0,0))+ch(b"IDAT",zlib.compress(raw,9))+ch(b"IEND",b"")
    open(path,"wb").write(out)
def vline(w,h,px,x,col):
    if x<0 or x>=w: return
    for y in range(h):
        px[(y*w+x)*3:(y*w+x)*3+3]=bytes(col)
