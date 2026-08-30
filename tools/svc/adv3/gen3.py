D = '/home/dudu/ss2/repo/ss2-main/tools/svc/adv3/'
S = '/home/dudu/ss2/saves/svc/'
F = '!load %ssvc_fight.st' % S

# K: poke P1 world X (0x0934/35) -> 232
open(D + 'sK.txt', 'w').write('\n'.join([
    F, '40 -', '!K0',
    '!poke 0934=232', '!poke 0935=0', '!K1', '2 -', '!K2', '10 -', '!K3']) + '\n')
# L: poke P2 world X (0x0AB4/B5) -> 100
open(D + 'sL.txt', 'w').write('\n'.join([
    F, '40 -', '!L0',
    '!poke 0AB4=100', '!poke 0AB5=0', '!L1', '2 -', '!L2', '10 -', '!L3']) + '\n')
# M: poke P1 SCREEN X (0x092E/2F) -> 152, see whether world follows or it is overwritten
open(D + 'sM.txt', 'w').write('\n'.join([
    F, '40 -', '!M0',
    '!poke 092E=152', '!poke 092F=0', '!M1', '2 -', '!M2', '10 -', '!M3']) + '\n')
print('ok')
