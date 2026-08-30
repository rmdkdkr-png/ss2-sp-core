import sys,glob,os
offs=[("092E16",0x092E,2),("0930",0x0930,1),("0934_16",0x0934,2),("0936",0x0936,1),
      ("0954",0x0954,1),("0955",0x0955,1),("0AAE",0x0AAE,1),("0AAF",0x0AAF,1),
      ("0AB0",0x0AB0,1),("0AB4",0x0AB4,1),("08CF_hp2",0x08CF,1),("08AF_hp1",0x08AF,1)]
files=sys.argv[1:]
print("%-14s"%"tag"+"".join("%9s"%n for n,_,_ in offs))
for f in files:
    d=open(f,"rb").read()
    tag=os.path.basename(f)[4:-4]
    row=[]
    for n,o,w in offs:
        v=d[o] if w==1 else d[o]|(d[o+1]<<8)
        row.append(v)
    print("%-14s"%tag+"".join("%9d"%v for v in row))
