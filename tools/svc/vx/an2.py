import glob,os,sys
def u16(b,o): return b[o]|(b[o+1]<<8)
pref=sys.argv[1]
print('tag   092E  0934  0AB4  19A6  |  0934-19A6  0AB4-19A6  hp1 hp2 anim')
for f in sorted(glob.glob('d2/svc_%s*.ram'%pref)):
    b=open(f,'rb').read(); t=os.path.basename(f)[4:-4]
    a,w,p2,cam=u16(b,0x092E),u16(b,0x0934),u16(b,0x0AB4),u16(b,0x19A6)
    print('%-5s %4d %5d %5d %5d  |  %6d %9d   %3d %3d %3d'%(t,a,w,p2,cam,w-cam,p2-cam,b[0x08AF],b[0x08CF],b[0x0C7E]))
