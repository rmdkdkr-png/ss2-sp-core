import sys,json,os
from pathlib import Path
from lbemu import *
p=Path(__file__).parent;spec=json.loads((p/'slots.json').read_text());ref=json.loads((p/'rings.json').read_text());report=[]
e=Emu(str(p/'allchars.dll'),os.environ.get('LB_ROM',str(p/'orig.ngc')))
# Keep RAM/logic/audio live; visual inspection is a separate recorded run.
cb=C.CFUNCTYPE(None,P,U,U,Z)(lambda *args:None);e.lib.retro_set_video_refresh(cb)
idle={4,12,20,24}
def trace(pad,n=160):
 a=[]
 for i in range(n):
  e.run(1,pad if i<6 else 0);v=e.ram()[0x370]
  if v not in idle and (not a or a[-1]!=v):a.append(v)
 return a
def expected(cid,slot,cmd):
 if slot==6:return {9:[112],10:[128],11:[152]}[cid]
 return [x for x in ref[str(cid)][cmd][0] if x not in idle][:2]
def matches(a,w):return a[:len(w)]==w
for cid in range(15):
 e.load(p/f'id{cid}.st');e.run(60,128);e.run(30,144);e.run(80);flip=e.ram()[0x386]
 e.save(p/f'flip{cid}.st')
 if flip!=1:raise RuntimeError(f'cross failed {cid}: {flip}')
 for side in (0,1):
  for s,cmd in enumerate(spec[cid]):
   if not cmd:continue
   w=expected(cid,s,cmd);counts=[];fail=[]
   for phase in (0,1):
    ok=0
    for trial in range(20):
     e.load(p/(f'flip{cid}.st' if side else f'id{cid}.st'));e.run(trial*2+phase)
     if s==6:e.run(8,16);e.run(2)
     f,b=(64,128) if side else (128,64)
     pad=[0,f,b,32,f|32,b|32,0][s]
     a=trace(pad|2048)
     if s==6:a=[x for x in a if x not in [44,48]]
     passed=matches(a,w);ok+=passed
     if not passed and len(fail)<2:fail.append([phase,trial,a,w])
    counts.append(ok)
   report.append({'id':cid,'slot':s,'side':side,'counts':counts,'fail':fail})
 # Repeated SP from one uninterrupted fight, no reload between attempts.
 e.load(p/f'id{cid}.st');cont=[]
 for _ in range(5):
  a=trace(2048,300);cont.append(matches(a,expected(cid,0,spec[cid][0])))
 (p/'stress.json').write_text(json.dumps({'rows':report,'last':cid,'continuous_last':cont},indent=2))
 print(cid,cont,[r for r in report if r['id']==cid and min(r['counts'])<18],flush=True)
e.close()
