saves=["svc_c0_0","svc_c0_3","svc_c1_0","svc_c1_4","svc_c2_0","svc_c2_5","svc_c3_0","svc_c3_2","svc_f2","svc_fight","svc_st0","svc_st2"]
L=[]
for s in saves:
    p="/home/dudu/ss2/saves/svc/%s.st"%s
    L+= ["!tag %s_set"%s, "!load "+p, "90 -", "!save tmp_%s.st"%s,
         "!tag %s_A"%s, "!load tmp_%s.st"%s, "14 -", "!%s_A"%s,
         "!tag %s_B"%s, "!load tmp_%s.st"%s, "4 A", "10 -", "!%s_B"%s,
         "!tag %s_C"%s, "!load tmp_%s.st"%s, "4 B", "10 -", "!%s_C"%s]
open("e9.txt","w").write("\n".join(L)+"\n")
print("\n".join(saves))
