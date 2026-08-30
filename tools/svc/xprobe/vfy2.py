import glob
def L(p): return [open(t,"rb").read() for t in sorted(glob.glob(p))]
sets = [("H stepwise", "svc_H??.ram"),
        ("F P1 walks R", "svc_F??.ram"),
        ("G P1 idle P2 walks", "svc_G??.ram"),
        ("E P1 idle long", "svc_E?.ram")]
for name, pat in sets:
    d = L(pat)
    if not d: continue
    print("==", name, "n=", len(d))
    print("  P1w 0934", [x[0x0934] for x in d])
    print("  P1s 092E", [x[0x092E] for x in d])
    print("  P2w 0AB4", [x[0x0AB4] for x in d])
    print("  P2s 0AAE", [x[0x0AAE] for x in d])
    print("  cam 19A6", [x[0x19A6] for x in d])
    print("  hi 0935 ", [x[0x0935] for x in d])
    print("  hi 0AB5 ", [x[0x0AB5] for x in d])
    print("  hi 19A7 ", [x[0x19A7] for x in d])
    print("  face P1w>P2w", [1 if x[0x0934] > x[0x0AB4] else 0 for x in d])
