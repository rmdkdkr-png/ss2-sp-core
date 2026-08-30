import sys
def rd(p):
    return open(p,"rb").read()
offs=[("092E16",0x092E,2),("0934_16",0x0934,2),("0AAE8",0x0AAE,1),("0AAF8",0x0AAF,1),("0AB4_16",0x0AB4,2),("19A6_16",0x19A6,2),("244C8",0x244C,1),("0930_Y1",0x0930,1),("0AB0_Y2",0x0AB0,1),("08A0chr1",0x08A0,1),("08C0chr2",0x08C0,1),("08B3hp1",0x08B3,1),("08CFhp2",0x08CF,1)]
print("file".ljust(14)+"".join(n.rjust(10) for n,_,_ in offs))
for f in sys.argv[1:]:
    d=rd(f); row=[]
    for n,o,w in offs:
        v=d[o] if w==1 else d[o]|(d[o+1]<<8)
        row.append(str(v).rjust(10))
    print(f.replace("svc_","").replace(".ram","").ljust(14)+"".join(row))
