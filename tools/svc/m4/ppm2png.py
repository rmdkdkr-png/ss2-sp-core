import sys,zlib,struct
def load(p):
    d=open(p,'rb').read()
    # parse P6 header
    i=0; toks=[]
    while len(toks)<4:
        while d[i:i+1].isspace(): i+=1
        if d[i:i+1]==b'#':
            while d[i:i+1] not in (b'\n',b''): i+=1
            continue
        j=i
        while not d[j:j+1].isspace(): j+=1
        toks.append(d[i:j]); i=j
    i+=1
    w=int(toks[1]); h=int(toks[2])
    return w,h,d[i:i+w*h*3]
def png(w,h,rgb,out,scale=1):
    raw=b''
    for y in range(h):
        row=rgb[y*w*3:(y+1)*w*3]
        if scale>1:
            r2=b''
            for x in range(w): r2+=row[x*3:x*3+3]*scale
            row=r2
        for _ in range(scale): raw+=b'\x00'+row
    W=w*scale; H=h*scale
    def ch(t,dd): 
        c=struct.pack('>I',len(dd))+t+dd
        return c+struct.pack('>I',zlib.crc32(t+dd)&0xffffffff)
    o=b'\x89PNG\r\n\x1a\n'+ch(b'IHDR',struct.pack('>IIBBBBB',W,H,8,2,0,0,0))+ch(b'IDAT',zlib.compress(raw,9))+ch(b'IEND',b'')
    open(out,'wb').write(o)
for p in sys.argv[1:]:
    w,h,rgb=load(p); png(w,h,rgb,p.replace('.ppm','.png'),2)
    print(p,w,h)
