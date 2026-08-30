R = ["svc_J50.ram", "svc_K1.ram", "svc_K3.ram", "svc_S_flip.ram"]
L = ["svc_F00.ram", "svc_S_c0_0.ram", "svc_S_fight.ram", "svc_H00.ram",
     "svc_S_st0.ram", "svc_S_sparring.ram", "svc_H22.ram"]
r = [open(p, "rb").read() for p in R]
l = [open(p, "rb").read() for p in L]
N = len(r[0])
out = []
for o in range(N):
    a = {x[o] for x in r}
    b = {x[o] for x in l}
    if len(a) <= 2 and len(b) <= 2 and not (a & b):
        out.append((o, sorted(a), sorted(b)))
print("candidate facing flags:", len(out))
for o, a, b in out[:60]:
    print("  %04X  right=%s  left=%s" % (o, a, b))
