import sys
save,btn,tag,steps,per = sys.argv[1],sys.argv[2],sys.argv[3],int(sys.argv[4]),int(sys.argv[5])
L=['!load /home/dudu/ss2/saves/svc/%s'%save]
L.append('!%s00'%tag)
for i in range(1,steps+1):
    L.append('%d %s'%(per,btn))
    L.append('!%s%02d'%(tag,i))
open('%s.txt'%tag,'w').write('\n'.join(L)+'\n')
