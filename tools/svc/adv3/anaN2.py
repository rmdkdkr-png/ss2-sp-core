D = '/home/dudu/ss2/repo/ss2-main/tools/svc/adv3/'
def w16(x, o): return x[o] | (x[o + 1] << 8)
d = [open(D + 'svc_N%03d.ram' % i, 'rb').read() for i in range(100)]
print('  i | P1wld P1scr P1y | P2wld P2scr P2y | cam | hp1 hp2 timer bank')
for i in range(84, 100):
    x = d[i]
    print('%3d | %5d %5d %4d | %5d %5d %4d | %3d | %3d %3d %5d %4d  d1=%d d2=%d' % (
        i, w16(x, 0x0934), w16(x, 0x092E), x[0x0930],
        w16(x, 0x0AB4), w16(x, 0x0AAE), x[0x0AB0], w16(x, 0x19A6),
        x[0x08B3], x[0x08CF], x[0x08EE], x[0x09AD],
        w16(x, 0x0934) - w16(x, 0x092E), w16(x, 0x0AB4) - w16(x, 0x0AAE)))
