import os, glob
saves = sorted(glob.glob("/home/dudu/ss2/saves/svc/*.st"))
f = open("sALL.txt", "w")
for s in saves:
    tag = os.path.basename(s)[:-3].replace("svc_", "")
    f.write("!load %s\n30 -\n!S_%s\n" % (s, tag))
f.close()
print(len(saves))
