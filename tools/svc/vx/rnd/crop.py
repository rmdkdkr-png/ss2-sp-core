import sys
from ppm2png import readppm,png
w,h,rgb=readppm(sys.argv[1])
X0,X1,Y0,Y1=64,224,100,170
out=bytearray()
for y in range(Y0,Y1):
    out+=rgb[(y*w+X0)*3:(y*w+X1)*3]
png(X1-X0,Y1-Y0,bytes(out),sys.argv[2],5)
