# -*- coding: utf-8 -*-
"""띠 PPM 여러 장을 세로로 붙여 PNG 한 장으로. 내가 눈으로 보려고 만든 것."""
import sys, os, zlib, struct
def readppm(p):
    d=open(p,'rb').read()
    # P6\n<w> <h>\n255\n
    parts=d.split(b'\n',3)
    w,h=[int(x) for x in parts[1].split()]
    return w,h,parts[3]
def png(path,w,h,rgb):
    raw=b''.join(b'\x00'+rgb[y*w*3:(y+1)*w*3] for y in range(h))
    def ch(t,d):
        c=struct.pack('>I',len(d))+t+d
        return c+struct.pack('>I',zlib.crc32(t+d)&0xffffffff)
    open(path,'wb').write(b'\x89PNG\r\n\x1a\n'
        +ch(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))
        +ch(b'IDAT',zlib.compress(raw,9))+ch(b'IEND',b''))
src=sys.argv[1]; out=sys.argv[2]
idx=[int(x) for x in sys.argv[3].split(',')] if len(sys.argv)>3 else None
files=sorted(f for f in os.listdir(src) if f.endswith('.ppm'))
if idx is not None: files=[ '%03d.ppm'%i for i in idx ]
GAP=4
W=None; rows=[]
for f in files:
    w,h,d=readppm(os.path.join(src,f))
    W=w; rows.append((h,d))
H=sum(h for h,_ in rows)+GAP*(len(rows)-1)
buf=bytearray(W*H*3)
y0=0
for h,d in rows:
    buf[y0*W*3:(y0+h)*W*3]=d
    y0+=h+GAP
    if y0<H:
        for y in range(y0-GAP,y0):
            for x in range(W):
                buf[(y*W+x)*3+0]=40; buf[(y*W+x)*3+1]=40; buf[(y*W+x)*3+2]=60
png(out,W,H,bytes(buf))
print(out, W, H, len(files),"장")
