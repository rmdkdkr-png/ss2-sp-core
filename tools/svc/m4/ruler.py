import sys,zlib,struct
def load(p):
    d=open(p,'rb').read(); i=0;toks=[]
    while len(toks)<4:
        while d[i:i+1].isspace(): i+=1
        j=i
        while not d[j:j+1].isspace(): j+=1
        toks.append(d[i:j]); i=j
    i+=1; w=int(toks[1]);h=int(toks[2])
    return w,h,bytearray(d[i:i+w*h*3])
def png(w,h,rgb,out,scale):
    raw=b''
    for y in range(h):
        row=bytes(rgb[y*w*3:(y+1)*w*3])
        r2=b''
        for x in range(w): r2+=row[x*3:x*3+3]*scale
        for _ in range(scale): raw+=b'\x00'+r2
    W=w*scale;H=h*scale
    def ch(t,dd):
        return struct.pack('>I',len(dd))+t+dd+struct.pack('>I',zlib.crc32(t+dd)&0xffffffff)
    open(out,'wb').write(b'\x89PNG\r\n\x1a\n'+ch(b'IHDR',struct.pack('>IIBBBBB',W,H,8,2,0,0,0))+ch(b'IDAT',zlib.compress(raw,9))+ch(b'IEND',b''))
X0=64
for p in sys.argv[1:]:
    w,h,rgb=load(p)
    for gx in range(0,160):
        if gx%16: continue
        col=(255,0,0) if gx%64 else (0,255,0)
        x=X0+gx
        for y in range(h):
            if y%3==0 or gx%64==0:
                o=(y*w+x)*3; rgb[o],rgb[o+1],rgb[o+2]=col
    # crop to play area x 60..228
    cw=170; crop=bytearray()
    for y in range(h):
        crop+=rgb[(y*w+60)*3:(y*w+60+cw)*3]
    png(cw,h,crop,p.replace('.ppm','_r.png'),4)
    print(p,'-> game x = (cropx/4) + 60 - 64')
