D = '/home/dudu/ss2/repo/ss2-main/tools/svc/adv3/'
def w16(x, o): return x[o] | (x[o + 1] << 8)
saves = ['svc_f2', 'svc_fight', 'svc_c0_0', 'svc_c1_0', 'svc_c2_0', 'svc_c3_0',
         'svc_c1_3', 'svc_c2_5', 'svc_st2']
print('save        chr1 chr2 | ident | P1wld range      P2wld range      cam range   | P1 responded to R/L')
for s in saves:
    d = []
    for i in range(17):
        try:
            d.append(open(D + 'svc_G_%s_%d.ram' % (s, i), 'rb').read())
        except IOError:
            pass
    if not d:
        print(s, 'NO DUMPS'); continue
    ok = all(w16(x, 0x0934) - w16(x, 0x092E) == w16(x, 0x19A6) and
             w16(x, 0x0AB4) - w16(x, 0x0AAE) == w16(x, 0x19A6) for x in d)
    p1 = [w16(x, 0x0934) for x in d]
    p2 = [w16(x, 0x0AB4) for x in d]
    cam = [w16(x, 0x19A6) for x in d]
    # samples 1..8 = holding RIGHT, 9..16 = holding LEFT
    rightdelta = p1[8] - p1[0] if len(p1) > 8 else None
    leftdelta = p1[16] - p1[8] if len(p1) > 16 else None
    print('%-10s %3d %3d | %5s | %4d..%-4d (n=%2d)  %4d..%-4d (n=%2d)  %3d..%-3d | R:%+5s L:%+5s' % (
        s, d[0][0x08A0], d[0][0x08C0], ok,
        min(p1), max(p1), len(set(p1)), min(p2), max(p2), len(set(p2)),
        min(cam), max(cam), rightdelta, leftdelta))
