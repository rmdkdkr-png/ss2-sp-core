#!/usr/bin/env python3
# 세이브 스테이트를 코어에 박을 헤더로 굽는다.
import os, io
S = os.path.expanduser('~/ss2/saves/svc/svc_ct_정지_0_0.st')
O = os.path.expanduser('~/ss2/repo/ss2-sp-core/src/svcsp_state.h')
b = open(S, 'rb').read()
w = io.StringIO()
w.write('/* 자동 생성 — tools/svc/mkstate.py. 쿄 vs 류 접촉·정지 더미 스파링 */\n')
w.write('#pragma once\n')
w.write('#define SVC_STATE_LEN %d\n' % len(b))
w.write('static const unsigned char svc_state_blob[SVC_STATE_LEN] = {\n')
for i in range(0, len(b), 16):
    w.write(' ' + ','.join('%d' % c for c in b[i:i+16]) + ',\n')
w.write('};\n')
io.open(O, 'w', encoding='utf-8', newline='\n').write(w.getvalue())
print('  %s  %d바이트' % (O, len(b)))
