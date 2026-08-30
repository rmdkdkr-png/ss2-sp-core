D = '/home/dudu/ss2/repo/ss2-main/tools/svc/adv3/'
def w16(x, o): return x[o] | (x[o + 1] << 8)
d = [open(D + 'svc_F%02d.ram' % i, 'rb').read() for i in range(40)]
print('TEST F: P1 holds RIGHT against the wall; CPU free')
print(' i | P1wld P1scr P1y | P2wld P2scr P2y | cam')
p1 = []
p2 = []
for i in range(40):
    x = d[i]
    a, b = w16(x, 0x0934), w16(x, 0x0AB4)
    p1.append(a); p2.append(b)
    print('%2d | %5d %5d %4d | %5d %5d %4d | %4d' % (
        i, a, w16(x, 0x092E), x[0x0930], b, w16(x, 0x0AAE), x[0x0AB0], w16(x, 0x19A6)))
print('P1 world distinct values:', sorted(set(p1)))
print('P2 world distinct values:', sorted(set(p2)))
