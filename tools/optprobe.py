# -*- coding: utf-8 -*-
"""ngp_ss2sp 가 정말 먹나 — 코어 둘 × 옵션 둘로 램 덤프를 떠 견준다."""
import hashlib,io,os,shutil,subprocess,tempfile
RUN=os.path.expanduser("~/ss2/repo/ss2-sp-core/tools/kof/ngprun")
PRE="/tmp/core_pre.so"
CUR=os.path.expanduser("~/ss2/repo/ss2-sp-core/cores/mednafen_ngp_libretro.linux-x86_64.so")
ROM=os.path.expanduser("~/ss2/rom/pristine/Metal Slug - 1st Mission (JUE).ngc")
SC=os.path.expanduser("~/ss2/repo/ss2-sp-core/tools/kof/m1_reg.txt")
def go(core,opts):
    env=dict(os.environ); env["NGP_OPTS"]=opts
    t=tempfile.mkdtemp(); r=os.path.join(t,"r.ngc"); shutil.copy(ROM,r)
    subprocess.run([RUN,core,r,SC,os.path.join(t,"g")],capture_output=True,text=True,env=env)
    h={}
    for f in sorted(os.listdir(t)):
        if f.endswith(".ram"):
            h[f]=hashlib.md5(open(os.path.join(t,f),"rb").read()).hexdigest()[:10]
    return h
for core,nm in [(PRE,"pre77"),(CUR,"현재")]:
    for opts in ("ngp_ss2sp=enabled","ngp_ss2sp=disabled",""):
        h=go(core,opts)
        print("%-6s %-24s %s"%(nm,opts or "(빈 NGP_OPTS)"," ".join("%s=%s"%(k,v) for k,v in sorted(h.items()))))
