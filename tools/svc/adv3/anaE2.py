D = '/home/dudu/ss2/repo/ss2-main/tools/svc/adv3/'
d = [open(D + 'svc_E%02d.ram' % i, 'rb').read() for i in range(61)]
def w16(x, o): return x[o] | (x[o + 1] << 8)
print('idx  scr0x092E wld0x0934 cam0x19A6 | 0x094E 0x0950 0x0954 0x0956 0x095A | 0x0954w')
for i in range(0, 61, 2):
    x = d[i]
    print('%2d   %5d %6d %6d | %4d %4d %4d %4d %4d | %5d' % (
        i, w16(x, 0x092E), w16(x, 0x0934), w16(x, 0x19A6),
        x[0x094E], x[0x0950], x[0x0954], x[0x0956], x[0x095A], w16(x, 0x0954)))
# scan for byte pairs that satisfy  X - 0x092E-ish pattern: find any 16-bit loc L with
# L - (some other 16-bit loc) == cam for all samples, i.e. second entity pair
cam = [w16(x, 0x19A6) for x in d]
pairs = []
for a in range(0x0800, 0x1000):
    va = [w16(x, a) for x in d]
    if len(set(va)) < 3: continue
    for b in range(0x0800, 0x1000):
        if b == a: continue
        vb = [w16(x, b) for x in d]
        if all(va[i] - vb[i] == cam[i] for i in range(61)):
            pairs.append(('%04X' % a, '%04X' % b))
print('world/screen pairs (world,screen) with diff==cam:', pairs)
