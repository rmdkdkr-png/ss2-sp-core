import sys,os
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from ppm import readppm,png,line
X0,Y0=64,16
tag=sys.argv[1]
ram=open("svc_%s.ram"%tag,"rb").read()
w,h,b=readppm("svc_%s.ppm"%tag)
p1=ram[0x092E]|(ram[0x092F]<<8)
p2=ram[0x0AAE]
line(w,h,b,X0+p1,(255,0,0))      # red = 0x092E
line(w,h,b,X0+p2,(0,255,0))      # green = 0x0AAE
png(w,h,b,"m_%s.png"%tag)
print(tag,"0x092E=",p1,"->px",X0+p1," 0x0AAE=",p2,"->px",X0+p2)
