f = open("sJ.txt", "w")
f.write("!load /home/dudu/ss2/saves/svc/svc_c0_0.st\n30 -\n")
f.write("400 R\n20 -\n!J00\n")
# jump forward (up+right) over the opponent, sample the arc
f.write("4 U R\n")
n = 0
for i in range(1, 13):
    f.write("4 R\n!J%02d\n" % i)
f.write("40 -\n!J50\n")
f.close()
