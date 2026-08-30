import os,glob,subprocess
CORE='/home/dudu/ss2/repo/ss2-main/build/mednafen_ngp_libretro.so'
ROM='/home/dudu/ss2/rom/svc.ngc'
SAVES=sorted(glob.glob('/home/dudu/ss2/saves/svc/*.st'))
os.makedirs('d1',exist_ok=True)
seq=[('i','1 -'),('r1','20 R'),('r2','20 R'),('r3','20 R'),('r4','20 R'),
     ('l1','20 L'),('l2','20 L'),('l3','20 L'),('l4','20 L'),
     ('jm','10 U'),('jm2','10 U'),('at','12 A'),('at2','12 B'),
     ('wait','40 -'),('wait2','40 -'),('dn','15 D'),('rr','30 R'),('ll','30 L')]
for s in SAVES:
    nm=os.path.basename(s)[:-3]
    lines=['!load '+s]
    for tag,cmd in seq:
        lines.append(cmd)
        lines.append('!%s__%s'%(nm,tag))
    open('d1/sc.txt','w').write('\n'.join(lines)+'\n')
    r=subprocess.run(['/home/dudu/ss2/repo/ss2-main/tools/svc/svcrun',CORE,ROM,'sc.txt'],cwd='d1',capture_output=True,text=True)
    print(nm, 'ok' if r.returncode==0 else r.stdout[-200:])
