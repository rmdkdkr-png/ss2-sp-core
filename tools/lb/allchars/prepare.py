import os,sys
from pathlib import Path
from lbemu import *
p=Path(__file__).parent
rom=os.environ.get('LB_ROM',str(p/'orig.ngc'))
e=Emu(str(p/'baseline.dll'),rom)
e.run(4500)
for pad in [1,1,32,32,32,1,1,1,1]:e.run(4,pad);e.run(90)
e.save(p/'selection.st')
for cid in range(15):
 e.load(p/'selection.st');mem=e.lib.retro_get_memory_data(2)
 # TEST ONLY: ask the normal match initializer to load a selected actor.
 C.c_uint8.from_address(mem+0xc8a).value=cid
 C.c_uint8.from_address(mem+0xd21).value=cid
 for _ in range(2):e.run(4,1);e.run(90)
 e.run(120);assert e.ram()[0x36e]==cid*4
 e.save(p/f'id{cid}.st');e.snap(p/f'id{cid}.png')
e.close()
