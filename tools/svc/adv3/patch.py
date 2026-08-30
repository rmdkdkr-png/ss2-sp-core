s = open('/home/dudu/ss2/repo/ss2-main/tools/svc/svcrun.c').read()
old = '        fflush(csv); continue;'
new = ('        fflush(csv);\n'
       '        { int q; int y1 = ram[0x092E] | (ram[0x092F]<<8);\n'
       '          int y2 = ram[0x0934] | (ram[0x0935]<<8);\n'
       '          printf("WDBG %s: y1=%d y2=%d raw:", arg, y1, y2);\n'
       '          for(q=0x092C;q<0x0938;q++) printf(" %02X", ram[q]);\n'
       '          printf(" | cam %02X %02X | ptr %p | live %p\\n", ram[0x19A6], ram[0x19A7], (void*)ram, (void*)getmem(2)); }\n'
       '        continue;')
assert old in s
open('/home/dudu/ss2/repo/ss2-main/tools/svc/adv3/svcrun3.c', 'w').write(s.replace(old, new))
print('ok')
