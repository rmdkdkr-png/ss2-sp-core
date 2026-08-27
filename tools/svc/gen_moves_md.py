#!/usr/bin/env python3
"""moves.json → MOVES.md (사람용 기술표 문서)"""
import json, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
d=json.load(open('moves.json',encoding='utf-8'))
STAT={'확정':'✅','실측만':'🔬','가이드만':'📖','불일치':'⚠️'}
print('''# SvC MotM 기술표 — 원버튼 SP 용

출처: 웹 가이드 3원천(GameFAQs·위키·기타) × 실측 SWEEP2 교차검증 (2026-08-27).
✅ 확정(가이드+실측 일치) · 🔬 실측만 · 📖 가이드만(미검증 플래그로 투입) · ⚠️ 불일치(판정 후 채택)

- 버튼: **P=A(펀치), K=B(킥)**. 탭=약, **홀드=강** (게임 공통).
- `[4]6` 은 모으기(뒤 40프레임 유지 후 앞). `air` 접두 = 공중 전용.
- 슬롯 = 원버튼 배치: **N**(SP만) **F**(앞+SP) **B**(뒤+SP) **D**(↓+SP) **DF**(↘+SP) **DB**(↙+SP) **AIR**(공중 SP)
- ⚠️ 주의: 애니 카운터(0x0C7E)는 K 기술에 리셋되지 않는다 — K 기술 실측은 뱅크·피해 기준.
  옛 SWEEP 의 「K 전멸」은 이 맹점의 오판이었다.
''')
for c in d['characters']:
    sug=c.get('slot_suggestion',{})
    inv={}
    for k,v in sug.items(): inv.setdefault(v,[]).append(k)
    print('## %d. %s'%(c['id'],c['name']))
    print()
    print('| 기술 | 커맨드 | 버튼 | 종류 | 검증 | 슬롯 | 비고 |')
    print('|---|---|---|---|---|---|---|')
    for m in c['table']:
        slot=','.join(inv.get(m['name'],[]))
        note=(m.get('notes') or '').replace('|','/')
        kind=m.get('kind','special')
        cmd=m['command']+('(홀드)' if m.get('hold') else '')
        if m.get('air'): cmd='air '+cmd
        if m.get('near'): cmd+=' 근접'
        print('| %s | `%s` | %s | %s | %s | %s | %s |'%(
            m['name'],cmd,m.get('button','P'),kind,STAT.get(m.get('status',''),'?'),slot,note[:80]))
    print()
