import json
from pathlib import Path
from lbemu import *
p=Path(__file__).parent;e=Emu(str(p/'baseline.dll'),os.environ.get('LB_ROM',str(p/'orig.ngc')));out={}
for cid in [0,9,10,11]:
 out[cid]={}
 for mode in ['A','B','236A','236B','2B','236Ahold','2Bhold']:
  vals=[]
  for delay in [6,10,14]:
   e.load(p/f'id{cid}.st');e.run(4,16);e.run(delay-4);seq=[]
   if mode.startswith('236'):seq=[(2,32),(2,160),(2,128)]
   elif mode.startswith('2'):seq=[(2,32)]
   btn=1 if 'A' in mode else 256
   seq.append((4,btn | ((128 if mode.startswith('236') else 32) if 'hold' in mode else 0)));seq.append((80,0));a=[]
   for n,pad in seq:
    for _ in range(n):
     e.run(1,pad);v=e.ram()[0x370]
     if not a or a[-1]!=v:a.append(v)
   vals.append(a)
  out[cid][mode]=vals
 print(cid,out[cid])
(p/'air.json').write_text(json.dumps(out,indent=2));e.close()
