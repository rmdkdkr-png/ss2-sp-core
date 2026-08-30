import os
D = '/home/dudu/ss2/repo/ss2-main/tools/svc/adv3/'
S = '/home/dudu/ss2/saves/svc/'

# TEST F: P1 pinned in right corner, CPU free to move
L = ['!load %ssvc_fight.st' % S, '40 -']
L += ['300 R']            # walk to right wall
for i in range(40):
    L += ['!F%02d' % i, '20 R']   # keep holding R: P1 stays pinned
open(D + 'sF.txt', 'w').write('\n'.join(L) + '\n')

# TEST G: multi-save snapshot + short walk, per save
saves = ['svc_f2', 'svc_fight', 'svc_c0_0', 'svc_c1_0', 'svc_c2_0', 'svc_c3_0',
         'svc_c1_3', 'svc_c2_5', 'svc_st2']
for s in saves:
    L = ['!load %s%s.st' % (S, s), '60 -', '!G_%s_0' % s]
    for i in range(1, 9):
        L += ['15 R', '!G_%s_%d' % (s, i)]
    for i in range(9, 17):
        L += ['15 L', '!G_%s_%d' % (s, i)]
    open(D + 'sG_%s.txt' % s, 'w').write('\n'.join(L) + '\n')
print(' '.join(saves))
