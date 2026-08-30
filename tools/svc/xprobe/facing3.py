import glob
def show(pat):
    for t in sorted(glob.glob(pat)):
        x = open(t, "rb").read()
        p1 = x[0x0934] | (x[0x0935] << 8)
        p2 = x[0x0AB4] | (x[0x0AB5] << 8)
        f = 1 if p1 > p2 else 0
        b1, b2 = x[0x092C], x[0x0AAC]
        print("%-22s P1w=%4d P2w=%4d faceX=%d  092C=%3d(0x%02X flip=%d)  0AAC=%3d(flip=%d)  %s"
              % (t, p1, p2, f, b1, b1, (b1 >> 7) & 1, b2, (b2 >> 7) & 1,
                 "OK" if ((b1 >> 7) & 1) == f else "MISMATCH"))
show("svc_J??.ram")
print()
show("svc_S_*.ram")
