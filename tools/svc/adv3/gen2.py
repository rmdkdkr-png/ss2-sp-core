D = '/home/dudu/ss2/repo/ss2-main/tools/svc/adv3/'
S = '/home/dudu/ss2/saves/svc/'
for name, sv in (('H', 'svc_fight'), ('I', 'svc_c0_0'), ('J', 'svc_c2_0')):
    L = ['!load %s%s.st' % (S, sv), '40 -']
    for i in range(60):
        L += ['!%s%02d' % (name, i), '20 -']
    open(D + 's%s.txt' % name, 'w').write('\n'.join(L) + '\n')
print('ok')
