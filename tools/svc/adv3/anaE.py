import os
D = '/home/dudu/ss2/repo/ss2-main/tools/svc/adv3/'
tags = ['E%02d' % i for i in range(61)]
d = [open(D + 'svc_%s.ram' % t, 'rb').read() for t in tags]
def w16(x, o): return x[o] | (x[o + 1] << 8)
scr = [w16(x, 0x092E) for x in d]
wld = [w16(x, 0x0934) for x in d]
cam = [w16(x, 0x19A6) for x in d]
print('idx scr  wld  cam  wld-scr  diff==cam')
bad = 0
for i in range(61):
    ok = (wld[i] - scr[i]) == cam[i]
    if not ok: bad += 1
    if i % 4 == 0 or not ok:
        print('%2d  %4d %4d %4d  %5d  %s' % (i, scr[i], wld[i], cam[i], wld[i] - scr[i], ok))
print('identity violations:', bad, '/61')
print('scr range', min(scr), max(scr), 'wld range', min(wld), max(wld), 'cam range', min(cam), max(cam))
# bytes that changed during the final idle stretch (i=46..60) where wld is constant
lo, hi = 46, 60
ch = [o for o in range(16384) if len(set(d[i][o] for i in range(lo, hi + 1))) > 1]
print('bytes changing during final idle (P1 pinned at right wall):', len(ch))
print(['%04X' % o for o in ch][:80])
