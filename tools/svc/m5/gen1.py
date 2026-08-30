import sys
S="/home/dudu/ss2/saves/svc/"
out=[]
out.append("!load %ssvc_c0_0.st"%S)
out.append("60 -")
out.append("!w idle0")
# 200 frames idle, sample every 4
for i in range(50):
    out.append("4 -"); out.append("!w idle")
# hold L 300 frames
for i in range(75):
    out.append("4 L"); out.append("!w L")
out.append("!DL")
# hold R 500 frames
for i in range(125):
    out.append("4 R"); out.append("!w R")
out.append("!DR")
open("s1.txt","w").write("\n".join(out)+"\n")
