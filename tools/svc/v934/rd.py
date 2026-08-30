import sys,glob,os
offs=[0x092E,0x092F,0x0934,0x0935,0x0954,0x0955,0x19A6,0x19A7,0x0930]
def show(files):
    print('tag      ' + ' '.join('%6s'%('%04X'%o) for o in offs) + '   | 934_16  92E_16  92E+19A6')
    for f in sorted(files):
        d=open(f,'rb').read()
        t=os.path.basename(f)[4:-4]
        v=[d[o] for o in offs]
        x934=d[0x0934]|(d[0x0935]<<8); x92e=d[0x092E]|(d[0x092F]<<8)
        s=d[0x092E]+d[0x19A6]
        print('%-8s'%t + ' '.join('%6d'%x for x in v) + '   | %5d %6d %8d'%(x934,x92e,s))
show(glob.glob(sys.argv[1]))
