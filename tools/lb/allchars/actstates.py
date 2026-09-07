import sys,json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parents[1]/'ss2'));from lbemu import *
p=Path(__file__).parent;e=Emu(str(p/'baseline.dll'),os.environ.get('LB_ROM',str(p/'orig.ngc')));out={}
for cid in range(15):
 out[cid]={}
 for name,pad in [('rest',0),('down',32),('forward',128),('back',64),('jump',16)]:
  e.load(p/f'id{cid}.st');a=[]
  for i in range(40):
   e.run(1,pad if i<10 or name!='jump' else 0);v=e.ram()[0x370]
   if not a or a[-1]!=v:a.append(v)
  out[cid][name]=a
 print(cid,out[cid])
(p/'states.json').write_text(json.dumps(out,indent=2));e.close()
