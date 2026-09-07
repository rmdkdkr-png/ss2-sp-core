import sys,json
from pathlib import Path
from lbemu import *
p=Path(__file__).parent;spec=json.loads((p/'slots.json').read_text());out={}
e=Emu(str(p/'allchars.dll'),os.environ.get('LB_ROM',str(p/'orig.ngc')))
for cid in range(int(sys.argv[1]),int(sys.argv[2])):
 out[cid]={}
 for s,cmd in enumerate(spec[cid]):
  if not cmd:continue
  cases=[]
  for phase in (0,1):
   e.load(p/f'id{cid}.st');e.run(phase)
   if s==6:e.run(8,16);e.run(2)
   a=[];pad=[0,128,64,32,160,96,0][s]
   for i in range(200):
    e.run(1,(pad|2048) if i<6 else 0);v=e.ram()[0x370]
    if not a or a[-1]!=v:a.append(v)
   cases.append(a)
  out[cid][cmd+('_air' if s==6 else '')]=cases
 print(cid,flush=True)
(p/f'gate{sys.argv[1]}.json').write_text(json.dumps(out,indent=2));print(e.queries);e.close()
