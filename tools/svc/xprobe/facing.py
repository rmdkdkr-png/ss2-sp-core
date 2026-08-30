import glob
J = [open(t, "rb").read() for t in sorted(glob.glob("svc_J??.ram"))]
# J00..J04 = P1 left of P2 ; J05..J12,J50 = P1 right of P2
left = J[0:5]
right = J[5:]
N = len(J[0])
cand = []
for o in range(N):
    a = {x[o] for x in left}
    b = {x[o] for x in right}
    if len(a) == 1 and len(b) == 1 and a != b:
        cand.append((o, a.pop(), b.pop()))
print("bytes constant-but-different across the crossing:", len(cand))
# cross-check against the save-state survey: normal saves (P1 left) vs svc_flip (P1 right)
norm = open("svc_S_c0_0.ram", "rb").read()
norm2 = open("svc_S_fight.ram", "rb").read()
flip = open("svc_S_flip.ram", "rb").read()
K1 = open("svc_K1.ram", "rb").read()   # flip save, P1 right
F0 = open("svc_F00.ram", "rb").read()  # c0_0, P1 left
hits = []
for o, vl, vr in cand:
    if norm[o] == vl and norm2[o] == vl and F0[o] == vl and flip[o] == vr and K1[o] == vr:
        hits.append((o, vl, vr))
print("survives cross-check on independent saves:")
for o, vl, vr in hits:
    print("  %04X  P1-left=%3d  P1-right=%3d" % (o, vl, vr))
