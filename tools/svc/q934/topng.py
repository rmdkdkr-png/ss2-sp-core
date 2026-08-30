import sys,zlib,struct
def conv(src,dst):
    f=open(src,'rb'); assert f.readline().strip()==b'P6'
    l=f.readline()
    while l.startswith(b'#'): l=f.readline()
    w,h=map(int,l.split()); f.readline(); d=f.read()
    raw=b''.join(b'\x00'+d[y*w*3:(y+1)*w*3] for y in range(h))
    def ch(t,b): 
        c=t+b; return struct.pack('>I',len(b))+c+struct.pack('>I',zlib.crc32(c)&0xffffffff)
    png=b'\x89PNG\r\n\x1a\n'+ch(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+ch(b'IDAT',zlib.compress(raw))+ch(b'IEND',b'')
    open(dst,'wb').write(png)
for a in sys.argv[1:]:
    conv('svc_%s.ppm'%a,'v_%s.png'%a)
