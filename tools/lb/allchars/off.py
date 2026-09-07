import os,json
from pathlib import Path
os.environ['LB_TEST_ENGINE']='disabled'
from lbemu import *
p=Path(__file__).parent;data=[]
for core in ['baseline.dll','allchars.dll']:
 e=Emu(str(p/core),os.environ.get('LB_ROM',str(p/'orig.ngc')));rows=[]
 for c in range(15):
  e.load(p/f'id{c}.st')
  for pad in [2048,128|2048,32|2048,1024,2,512]:e.run(6,pad);e.run(100)
  rows.append((e.ram(),e.fb.tobytes()))
 data.append(rows);e.close()
report=[{'id':i,'ram_diff':sum(a!=b for a,b in zip(data[0][i][0],data[1][i][0])),'video_equal':data[0][i][1]==data[1][i][1]} for i in range(15)]
(p/'off.json').write_text(json.dumps(report,indent=2));print(report)
assert all(r['ram_diff']==0 and r['video_equal'] for r in report)
