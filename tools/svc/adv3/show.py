import sys
for t in sys.argv[1:]:
    d = open('/home/dudu/ss2/repo/ss2-main/tools/svc/adv3/svc_%s.ram' % t, 'rb').read()
    print(t, ' '.join('%02X' % b for b in d[0x092C:0x0938]),
          '| cam', d[0x19A6], d[0x19A7], '| len', len(d))
