import glob, os
rows = []
for t in sorted(glob.glob("svc_S_*.ram")):
    x = open(t, "rb").read()
    p1 = x[0x0934] | (x[0x0935] << 8)
    p2 = x[0x0AB4] | (x[0x0AB5] << 8)
    s1 = x[0x092E] | (x[0x092F] << 8)
    s2 = x[0x0AAE] | (x[0x0AAF] << 8)
    c = x[0x19A6] | (x[0x19A7] << 8)
    c2 = x[0x19AE] | (x[0x19AF] << 8)
    ok = (p1 - c == s1) and (p2 - c == s2) and (c == c2)
    old = 1 if s1 > p1 else 0
    new = 1 if p1 > p2 else 0
    rows.append((os.path.basename(t), p1, p2, s1, s2, c, ok, old, new,
                 x[0x08A0], x[0x08C0]))
print("%-20s %5s %5s %5s %5s %5s %5s %4s %4s %4s %4s" %
      ("save", "P1w", "P2w", "P1s", "P2s", "cam", "ident", "old", "new", "ch1", "ch2"))
bad = 0
for r in rows:
    if not r[6]: bad += 1
    print("%-20s %5d %5d %5d %5d %5d %5s %4d %4d %4d %4d" %
          (r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8], r[9], r[10]))
print("identity failures:", bad, "of", len(rows))
print("old formula face=1 count:", sum(r[7] for r in rows))
print("new formula face=1 count:", sum(r[8] for r in rows))
