import sys,json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parents[1]/'ss2'));from lbemu import *
p=Path(__file__).parent;DIR={'1':6,'2':2,'3':10,'4':4,'5':0,'6':8,'7':5,'8':1,'9':9}
def retro(v):return (16 if v&1 else 0)|(32 if v&2 else 0)|(64 if v&4 else 0)|(128 if v&8 else 0)|(1 if v&16 else 0)|(256 if v&32 else 0)
e=Emu(str(p/'baseline.dll'),os.environ.get('LB_ROM',str(p/'orig.ngc')));spec=json.loads((p/'slots.json').read_text());out=json.loads((p/'rings.json').read_text()) if (p/'rings.json').exists() else {}
for cid in range(int(sys.argv[1]),int(sys.argv[2])):
 out[str(cid)]={}
 for cmd in spec[cid][:6]:
  if not cmd:continue
  ans=[]
  for mode in ['manual','ring']:
   e.load(p/f'id{cid}.st');mem=e.lib.retro_get_memory_data(2);rest=cmd;pre=[];seq=[]
   if cmd.startswith('['):pre=[DIR[cmd[1]]]*40;rest=cmd[3:]
   dirs=[DIR[d] for d in rest.rstrip('AB')];btn=48 if rest=='AB' else 16 if rest.endswith('A') else 32
   if mode=='manual':
    seq=[(2,d) for d in pre]+[(2 if d==1 else 4,d) for d in dirs]
    seq.append((4,btn| (dirs[-1] if dirs and dirs[-1]==1 else 0)))
   else:
    live=dirs[-1] if dirs and dirs[-1] in [1,2,4,8] else 0
    hist=pre+(dirs[:-1] if live else dirs);h=e.ram()[0x1312]
    for j,d in enumerate(hist):C.c_uint8.from_address(mem+0x1313+((h-len(hist)+j)&127)).value=d
    if live:seq.append((2,live))
    seq.append((4,btn|(live if live==1 else 0)))
   seq.append((190,0));acts=[]
   for frames,v in seq:
    for _ in range(frames):
     e.run(1,retro(v));a=e.ram()[0x370]
     if not acts or acts[-1]!=a:acts.append(a)
   ans.append(acts)
  out[str(cid)][cmd]=ans
 (p/'rings.json').write_text(json.dumps(out,indent=2));print(cid,flush=True)
e.close()
