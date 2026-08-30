import sys
D = '/home/dudu/ss2/repo/ss2-main/tools/svc/adv3/'
def w16(x, o): return x[o] | (x[o + 1] << 8)
for name in sys.argv[1:]:
    d = [open(D + 'svc_%s%02d.ram' % (name, i), 'rb').read() for i in range(60)]
    p1w = [w16(x, 0x0934) for x in d]
    p2w = [w16(x, 0x0AB4) for x in d]
    p1s = [w16(x, 0x092E) for x in d]
    p2s = [w16(x, 0x0AAE) for x in d]
    cam = [w16(x, 0x19A6) for x in d]
    ident = all(p1w[i] - p1s[i] == cam[i] and p2w[i] - p2s[i] == cam[i] for i in range(60))
    print('== %s  (P1 gives NO input for 1200 frames)' % name)
    print('   P1 world distinct:', sorted(set(p1w)))
    print('   P2 world distinct:', sorted(set(p2w)))
    print('   cam distinct     :', sorted(set(cam)))
    print('   identity (world-screen==cam) for BOTH players, all 60 samples:', ident)
    ch = [i for i in range(1, 60) if p2w[i] != p2w[i - 1]]
    print('   samples where P2 world moved:', len(ch), '   P1 world moved:',
          len([i for i in range(1, 60) if p1w[i] != p1w[i - 1]]))
