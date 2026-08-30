D = '/home/dudu/ss2/repo/ss2-main/tools/svc/adv3/'
def w16(x, o): return x[o] | (x[o + 1] << 8)
d = [open(D + 'svc_N%03d.ram' % i, 'rb').read() for i in range(100)]
bad = []
for i, x in enumerate(d):
    if w16(x, 0x0934) - w16(x, 0x092E) != w16(x, 0x19A6): bad.append(('P1', i))
    if w16(x, 0x0AB4) - w16(x, 0x0AAE) != w16(x, 0x19A6): bad.append(('P2', i))
print('long run: identity violations', len(bad), bad[:20])
print('chr1 values seen:', sorted(set(x[0x08A0] for x in d)))
print('chr2 values seen:', sorted(set(x[0x08C0] for x in d)))
print('hp1 (0x08B3) seen:', sorted(set(x[0x08B3] for x in d)))
print('hp2 (0x08CF) seen:', sorted(set(x[0x08CF] for x in d)))
print('timer (0x08EE) seen:', sorted(set(x[0x08EE] for x in d))[:20])
p1 = [w16(x, 0x0934) for x in d]; p2 = [w16(x, 0x0AB4) for x in d]
print('P1 world min/max', min(p1), max(p1), ' P2 world min/max', min(p2), max(p2))
print('cam min/max', min(w16(x, 0x19A6) for x in d), max(w16(x, 0x19A6) for x in d))
print('samples where P1world>P2world (P1 on the right):',
      sum(1 for i in range(100) if p1[i] > p2[i]), '/100')
o = [open(D + 'svc_O%02d.ram' % i, 'rb').read() for i in range(25)]
print()
print('JUMP TEST (P1 jumps forward):  i  P1y P1wld P1scr | P2y P2wld P2scr | cam | ident')
for i, x in enumerate(o):
    id1 = w16(x, 0x0934) - w16(x, 0x092E) == w16(x, 0x19A6)
    id2 = w16(x, 0x0AB4) - w16(x, 0x0AAE) == w16(x, 0x19A6)
    print('  %2d  %3d %5d %5d | %3d %5d %5d | %3d | %s %s' % (
        i, x[0x0930], w16(x, 0x0934), w16(x, 0x092E),
        x[0x0AB0], w16(x, 0x0AB4), w16(x, 0x0AAE), w16(x, 0x19A6), id1, id2))
