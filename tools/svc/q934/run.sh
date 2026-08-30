#!/bin/bash
cd /home/dudu/ss2/repo/ss2-main/tools/svc/q934
export SVCSP_OFF=1 SVCSP_FORCE=0 ngp_svcsp_engine=disabled
/home/dudu/ss2/repo/ss2-main/tools/svc/svcrun /home/dudu/ss2/repo/ss2-main/build/mednafen_ngp_libretro.so /home/dudu/ss2/rom/svc.ngc "$1"
