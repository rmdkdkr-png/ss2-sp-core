D = '/home/dudu/ss2/repo/ss2-main/tools/svc/adv3/'
S = '/home/dudu/ss2/saves/svc/'
# N: very long run (round transitions / KO / char change) - P1 mostly idle, occasional attack
L = ['!load %ssvc_fight.st' % S, '40 -']
for i in range(100):
    L += ['!N%03d' % i, '30 -', '6 A', '14 -']
open(D + 'sN.txt', 'w').write('\n'.join(L) + '\n')
# O: P1 jumps forward
L = ['!load %ssvc_fight.st' % S, '40 -', '!O00', '4 U R']
for i in range(1, 25):
    L += ['!O%02d' % i, '3 R']
open(D + 'sO.txt', 'w').write('\n'.join(L) + '\n')
print('ok')
