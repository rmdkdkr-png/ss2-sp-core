#!/usr/bin/env python3
"""폴더 팩 → 단일 ngpvoice.pak — 'NGPV1\\0' u32 개수 [해시,오프셋,크기]* 블롭"""
import os, struct, sys, glob, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
src=os.path.expanduser(sys.argv[1] if len(sys.argv)>1 else '~/ss2/voicepack_v3')
out=os.path.expanduser(sys.argv[2] if len(sys.argv)>2 else src+'.pak')
rows=[l.split('\t') for l in open(src+'/manifest.tsv',encoding='utf-8').read().splitlines() if '\t' in l]
ent=[]
for h,fn in rows:
    p=src+'/'+fn
    if os.path.exists(p): ent.append((int(h,16), p, os.path.getsize(p)))
ent.sort()
hdr=6+4+len(ent)*12
with open(out,'wb') as f:
    f.write(b'NGPV1\x00'+struct.pack('<I',len(ent)))
    off=hdr
    for h,p,sz in ent:
        f.write(struct.pack('<III',h,off,sz)); off+=sz
    for h,p,sz in ent:
        f.write(open(p,'rb').read())
print('팩: %s — %d클립, %.1fMB'%(out,len(ent),os.path.getsize(out)/1e6))
