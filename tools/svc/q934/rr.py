import sys
def g(t,o,w=2):
    b=open('svc_%s.ram'%t,'rb').read()
    return b[o]|(b[o+1]<<8) if w==2 else b[o]
for t in sys.argv[1:]:
    print(t,'x1=%d x2=%d y1=%d hp1=%d hp2=%d'%(g(t,0x092E),g(t,0x0934),g(t,0x0930,1),g(t,0x08B3,1),g(t,0x08CF,1)))
