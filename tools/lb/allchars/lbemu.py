import ctypes as C, sys, json, hashlib, os, pathlib, time
import gzip
import numpy as np
from PIL import Image
U=C.c_uint; P=C.c_void_p; Z=C.c_size_t
class Var(C.Structure): _fields_=[('key',C.c_char_p),('value',C.c_char_p)]
class Game(C.Structure): _fields_=[('path',C.c_char_p),('data',P),('size',Z),('meta',C.c_char_p)]
class Emu:
 def __init__(self,core,rom):
  self.lib=C.CDLL(core); self.frame=0; self.pad=0; self.fb=None; self.fmt=0; self.audio_peak=0; self.queries={};self.outdir=str(pathlib.Path(__file__).parent).encode()
  self.options={b'ngp_language':b'japanese',b'ngp_ss2sp_comm_spk':b'haohmaru'}
  for k in ['ngp_ss2sp','ngp_svcsp_engine','ngp_ss2sp_comm','ngp_ss2sp_comm_draw','ngp_ss2sp_comm_duo','ngp_ss2sp_dub','ngp_svcsp_toast','ngp_ss2sp_sides','ngp_svcsp_band']:self.options[k.encode()]=b'disabled'
  self.options[b'ngp_ss2sp']=b'enabled'
  self.options[b'ngp_lbsp_engine']=os.environ.get('LB_TEST_ENGINE','enabled').encode()
  def env(cmd,data):
   if cmd==3:C.cast(data,C.POINTER(C.c_bool))[0]=True;return True
   if cmd in (9,31):C.cast(data,C.POINTER(C.c_char_p))[0]=self.outdir;return True
   if cmd==10:
    self.fmt=C.cast(data,C.POINTER(C.c_int))[0];return self.fmt in (0,1,2)
   if cmd==15:
    v=C.cast(data,C.POINTER(Var)).contents;v.value=self.options.get(v.key);self.queries[v.key.decode()]=(v.value or b'UNSET').decode();return bool(v.value)
   if cmd==17:C.cast(data,C.POINTER(C.c_bool))[0]=False;return True
   if cmd==16:return True
   return False
  def video(data,w,h,pitch):
   if not data:return
   n=4 if self.fmt==1 else 2
   raw=C.string_at(data,pitch*h);a=np.frombuffer(raw,dtype='<u4' if n==4 else '<u2').reshape(h,pitch//n)[:,:w].astype(np.uint32)
   if self.fmt==1:r=(a>>16)&255;g=(a>>8)&255;b=a&255
   elif self.fmt==2:r=((a>>11)&31)*255//31;g=((a>>5)&63)*255//63;b=(a&31)*255//31
   else:r=((a>>10)&31)*255//31;g=((a>>5)&31)*255//31;b=(a&31)*255//31
   self.fb=np.stack([r,g,b],2).astype(np.uint8)
  def aud(a,b):self.audio_peak=max(self.audio_peak,abs(a),abs(b))
  def batch(p,n):
   if n:self.audio_peak=max(self.audio_peak,int(np.abs(np.ctypeslib.as_array(p,shape=(n*2,)).astype(np.int32)).max()))
   return n
  callbacks=[('retro_set_environment',C.CFUNCTYPE(C.c_bool,U,P),env),('retro_set_video_refresh',C.CFUNCTYPE(None,P,U,U,Z),video),('retro_set_audio_sample',C.CFUNCTYPE(None,C.c_int16,C.c_int16),aud),('retro_set_audio_sample_batch',C.CFUNCTYPE(Z,C.POINTER(C.c_int16),Z),batch),('retro_set_input_poll',C.CFUNCTYPE(None),lambda:None),('retro_set_input_state',C.CFUNCTYPE(C.c_int16,U,U,U,U),lambda port,dev,idx,id: (self.pad>>id)&1 if port==0 and dev==1 and id<16 else 0)]
  self.callbacks=[]
  for name,typ,fn in callbacks:
   cb=typ(fn);self.callbacks.append(cb);fun=getattr(self.lib,name);fun.argtypes=[typ];fun(cb)
  self.lib.retro_init()
  self.romdata=pathlib.Path(rom).read_bytes();self.buf=C.create_string_buffer(self.romdata);g=Game(str(rom).encode(),C.cast(self.buf,P),len(self.romdata),None)
  self.lib.retro_load_game.argtypes=[C.POINTER(Game)];self.lib.retro_load_game.restype=C.c_bool
  assert self.lib.retro_load_game(C.byref(g)),'load failed'
  self.lib.retro_set_controller_port_device.argtypes=[U,U];self.lib.retro_set_controller_port_device(0,1)
  self.lib.retro_get_memory_data.argtypes=[U];self.lib.retro_get_memory_data.restype=P
  self.lib.retro_get_memory_size.argtypes=[U];self.lib.retro_get_memory_size.restype=Z
  self.lib.retro_serialize_size.restype=Z
  self.lib.retro_serialize.argtypes=[P,Z];self.lib.retro_serialize.restype=C.c_bool
  self.lib.retro_unserialize.argtypes=[P,Z];self.lib.retro_unserialize.restype=C.c_bool
 def run(self,n,pad=0):
  self.pad=pad
  for i in range(n):self.lib.retro_run();self.frame+=1
 def load(self,path):
  b=pathlib.Path(path).read_bytes()
  if b[:2]==bytes([31,139]):b=gzip.decompress(b)
  assert b[:8]==b'MDFNSVST', 'unknown state format'
  buf=C.create_string_buffer(b);assert self.lib.retro_unserialize(buf,len(b)),str(path)
 def save(self,path):
  n=self.lib.retro_serialize_size();b=C.create_string_buffer(n);assert self.lib.retro_serialize(b,n);pathlib.Path(path).write_bytes(b.raw)
 def ram(self):
  n=self.lib.retro_get_memory_size(2);return C.string_at(self.lib.retro_get_memory_data(2),n)
 def snap(self,path):
  assert self.fb is not None;Image.fromarray(self.fb).save(path)
 def close(self):self.lib.retro_unload_game();self.lib.retro_deinit()
if __name__=='__main__':
 core,rom,out=sys.argv[1:4];out=pathlib.Path(out);out.mkdir(parents=True,exist_ok=True);e=Emu(core,rom)
 if len(sys.argv)>4:
  e.load(sys.argv[4]);e.run(1);e.snap(out/'state.png')
 else:
  for j in range(12):e.run(100);e.snap(out/('boot_%04d.png'%e.frame))
 print(json.dumps({'frame':e.frame,'shape':None if e.fb is None else list(e.fb.shape),'pixel_format':e.fmt,'audio_peak':e.audio_peak,'options':e.queries,'rom_md5':hashlib.md5(e.romdata).hexdigest()}));e.close()
