from pathlib import Path
import json,hashlib
from lbemu import *
p=Path(__file__).parent
roms={'ss2':p.parent/'ss2/SS2_Korean_stable101.ngc','svc':'C:/Claude/SVC 한글/rom/base.ngc','kof':'C:/Claude/KOF R2 한글/rom/orig_good.ngc','mslug1':'C:/Claude/메탈슬러그1 한글/rom/orig.ngc','mslug2':'C:/Claude/메탈슬러그2 한글/rom/orig.ngc','ffury':'C:/Claude/아랑전설 한글/rom/orig.ngc'}
out={}
for game,rom in roms.items():
 seed=Emu(str(p/'baseline.dll'),str(rom));seed.run(1);seed.save(p/(game+'_reg.st'));seed.close()
 runs=[]
 for core in ['baseline.dll','allchars.dll','baseline.dll']:
  e=Emu(str(p/core),str(rom));audio=hashlib.sha256();e.load(p/(game+'_reg.st'))
  def batch(ptr,n):audio.update(C.string_at(ptr,n*4));return n
  cb=C.CFUNCTYPE(Z,C.POINTER(C.c_int16),Z)(batch);e.lib.retro_set_audio_sample_batch(cb)
  for n,pad in [(300,0),(4,1),(96,0),(10,128),(10,2048),(100,0),(10,1024),(200,0)]:e.run(n,pad)
  runs.append((e.ram(),e.fb.tobytes(),audio.hexdigest()));e.close()
 runs=[runs[0],runs[2],runs[1]]
 stable=[i for i in range(len(runs[0][0])) if runs[0][0][i]==runs[1][0][i]]
 diff=sum(runs[0][0][i]!=runs[2][0][i] for i in stable)
 out[game]={'stable_ram_diff':diff,'noise_bytes':len(runs[0][0])-len(stable),'video_baseline_stable':runs[0][1]==runs[1][1],'video_equal':runs[0][1]==runs[2][1],'audio_baseline_stable':runs[0][2]==runs[1][2],'audio_equal':runs[0][2]==runs[2][2]}
 print(game,out[game],flush=True)
(p/'regression.json').write_text(json.dumps(out,indent=2))
