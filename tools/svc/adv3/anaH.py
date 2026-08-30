D = '/home/dudu/ss2/repo/ss2-main/tools/svc/adv3/'
def w16(x, o): return x[o] | (x[o + 1] << 8)
d = [open(D + 'svc_H%02d.ram' % i, 'rb').read() for i in range(60)]
print(' i | P1wld P1scr P1y | P2wld P2scr P2y | cam | note')
prev = None
onlyP2 = 0
for i in range(60):
    x = d[i]
    r = (w16(x, 0x0934), w16(x, 0x092E), x[0x0930], w16(x, 0x0AB4), w16(x, 0x0AAE), x[0x0AB0], w16(x, 0x19A6))
    note = ''
    if prev:
        if r[0] == prev[0] and r[3] != prev[3]:
            note = '<< P2 moved, P1 world UNCHANGED'
            onlyP2 += 1
        elif r[0] != prev[0] and r[3] == prev[3]:
            note = '   P1 moved, P2 unchanged'
    print('%2d | %5d %5d %4d | %5d %5d %4d | %3d | %s' % (i, r[0], r[1], r[2], r[3], r[4], r[5], r[6], note))
    prev = r
print('samples where ONLY P2 world moved:', onlyP2)
