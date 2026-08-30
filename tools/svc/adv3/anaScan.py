D = '/home/dudu/ss2/repo/ss2-main/tools/svc/adv3/'
def w16(x, o): return x[o] | (x[o + 1] << 8)
d = [open(D + 'svc_E%02d.ram' % i, 'rb').read() for i in range(61)]
n = len(d)
p1w = [w16(x, 0x0934) for x in d]
p1s = [w16(x, 0x092E) for x in d]
p2w = [w16(x, 0x0AB4) for x in d]
p2s = [w16(x, 0x0AAE) for x in d]
cam = [w16(x, 0x19A6) for x in d]
def find(series, label):
    hits = []
    for o in range(16383):
        if [w16(x, o) for x in d] == series:
            hits.append('%04X' % o)
    print('%s exact 16-bit mirrors: %s' % (label, hits))
find(p1w, 'P1 world X')
find(p1s, 'P1 screen X')
find(p2w, 'P2 world X')
find(p2s, 'P2 screen X')
find(cam, 'camera X')
# all (world,screen) pairs over the whole RAM
pairs = []
cand = [o for o in range(16383) if len(set(w16(x, o) for x in d)) >= 3
        and max(w16(x, o) for x in d) < 1024]
for a in cand:
    va = [w16(x, a) for x in d]
    for b in cand:
        if a == b: continue
        vb = [w16(x, b) for x in d]
        if all(va[i] - vb[i] == cam[i] for i in range(n)):
            pairs.append(('%04X' % a, '%04X' % b))
print('all (world,screen) pairs in full RAM:', pairs)
