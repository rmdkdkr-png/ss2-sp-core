import sys, zlib, struct

def readppm(p):
    d=open(p,'rb').read()
    # P6\n<w> <h>\n255\n
    assert d[:2]==b'P6'
    i=2
    vals=[]
    while len(vals)<3:
        while d[i] in b' \t\r\n': i+=1
        if d[i:i+1]==b'#':
            while d[i] not in b'\n': i+=1
            continue
        j=i
        while d[j] not in b' \t\r\n': j+=1
        vals.append(int(d[i:j])); i=j
    i+=1
    w,h,mx=vals
    return w,h,d[i:i+w*h*3]

def png(w,h,rgb,out,scale=1):
    if scale>1:
        rows=[]
        for y in range(h):
            r=rgb[y*w*3:(y+1)*w*3]
            nr=b''.join(r[x*3:x*3+3]*scale for x in range(w))
            for _ in range(scale): rows.append(nr)
        w*=scale; h*=scale
        raw=b''.join(b'\x00'+r for r in rows)
    else:
        raw=b''.join(b'\x00'+rgb[y*w*3:(y+1)*w*3] for y in range(h))
    def chunk(t,data):
        c=struct.pack('>I',len(data))+t+data
        return c+struct.pack('>I',zlib.crc32(t+data)&0xffffffff)
    o=b'\x89PNG\r\n\x1a\n'
    o+=chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))
    o+=chunk(b'IDAT',zlib.compress(raw,9))
    o+=chunk(b'IEND',b'')
    open(out,'wb').write(o)

if __name__=='__main__':
    src=sys.argv[1]; dst=sys.argv[2]
    sc=int(sys.argv[3]) if len(sys.argv)>3 else 1
    w,h,rgb=readppm(src); png(w,h,rgb,dst,sc)
    print(src,w,h,'->',dst)
