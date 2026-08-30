import sys
D = '/home/dudu/ss2/repo/ss2-main/tools/svc/adv3/'
def w16(x, o): return x[o] | (x[o + 1] << 8)
for n in sys.argv[1:]:
    print('== script %s' % n)
    for i in range(4):
        x = open(D + 'svc_%s%d.ram' % (n, i), 'rb').read()
        print('  step%d  P1wld=%4d P1scr=%4d P1y=%3d | P2wld=%4d P2scr=%4d P2y=%3d | cam=%3d' % (
            i, w16(x, 0x0934), w16(x, 0x092E), x[0x0930],
            w16(x, 0x0AB4), w16(x, 0x0AAE), x[0x0AB0], w16(x, 0x19A6)))
