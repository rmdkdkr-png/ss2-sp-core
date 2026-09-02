#!/usr/bin/env python3
"""검증 결과를 moves.json 에 반영"""
import json, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
mv=json.load(open('moves.json',encoding='utf-8'))
gr=json.load(open('grand_results.json',encoding='utf-8'))
sp=json.load(open('super_results.json',encoding='utf-8'))
ok={(a,b) for a,b in sp['ok']}; ng={(a,b) for a,b in sp['ng']}

up=dn=0
for c in mv['characters']:
    for k,m in enumerate(c['table']):
        key='%d_%d'%(c['id'],k); tup=(c['id'],k)
        if m.get('kind')=='super':
            if tup in ok and m.get('status')!='확정':
                m['status']='확정'; m['notes']=(m.get('notes','')+' | 실발동 확정(게이지 소모)').strip(' |'); up+=1
            elif tup in ng:
                m['status']='가이드만'
                m['notes']=(m.get('notes','')+' | 게이지 소모 0 — 커맨드 표기 의심 또는 해금 전용').strip(' |'); dn+=1
            continue
        r=gr.get(key)
        if not r: continue
        verdict,banks,hp,used=r
        if verdict=='확정' and m.get('status')!='확정':
            m['status']='확정'
            note='실측: 뱅크[%s]'%banks + (' 피해%d'%hp if hp else '')
            m['notes']=(m.get('notes','')+' | '+note).strip(' |'); up+=1
        elif verdict.startswith('동작만') and m.get('status')=='가이드만':
            m['status']='실측만'
            m['notes']=(m.get('notes','')+' | 동작 반응 확인(뱅크 무 — 공중 평타/이동기 추정)').strip(' |'); up+=1
        elif verdict.startswith('불발'):
            m['notes']=(m.get('notes','')+' | 원거리 불발 — 근접/조건 미검증').strip(' |')
json.dump(mv, open('moves.json','w',encoding='utf-8'), ensure_ascii=False, indent=1)

st={'확정':0,'실측만':0,'가이드만':0,'불일치':0}
tot=0
for c in mv['characters']:
    for m in c['table']:
        st[m.get('status','가이드만')]+=1; tot+=1
print('갱신: 승격 %d, 강등/유지 표시 %d'%(up,dn))
print('전체 %d건 — 확정 %d · 실측만 %d · 가이드만 %d · 불일치 %d'%(tot,st['확정'],st['실측만'],st['가이드만'],st['불일치']))
