D = '/home/dudu/ss2/repo/ss2-main/tools/svc/adv3/'
d = [open(D + 'svc_E%02d.ram' % i, 'rb').read() for i in range(61)]
def w16(x, o): return x[o] | (x[o + 1] << 8)
print(' i | P1scr P1wld P1y | P2scr P2wld P2y |  cam | P1w-P2w')
for i in range(0, 61, 2):
    x = d[i]
    p1s, p1w, p1y = w16(x, 0x092E), w16(x, 0x0934), x[0x0930]
    p2s, p2w, p2y = w16(x, 0x0AAE), w16(x, 0x0AB4), x[0x0AB0]
    print('%2d | %5d %5d %4d | %5d %5d %4d | %4d | %+5d' % (
        i, p1s, p1w, p1y, p2s, p2w, p2y, w16(x, 0x19A6), p1w - p2w))
