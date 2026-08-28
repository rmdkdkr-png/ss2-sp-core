#!/usr/bin/env python3
"""svcsp_moves.h 생성기 — 기술표 JSON(교차검증 산출물) → C 헤더.
   사용: gen_svc_moves.py moves.json > svcsp_moves.h
   SS2 의 gen_moves.js 와 같은 역할. 수동 수정 금지, JSON 을 고치고 재생성할 것."""
import json, sys

# 넘패드 → 패드 비트 (오른쪽 볼 때. 미러는 엔진이 한다)
NUM = {'1':0x06,'2':0x02,'3':0x0A,'4':0x04,'5':0x00,'6':0x08,'7':0x05,'8':0x01,'9':0x09}
BTN = {'P':0x10,'K':0x20,'PK':0x30}

def parse_cmd(cmd):
    """'236' '[4]6' 'air214' '632146' → (motion bytes, air, charge)"""
    cmd = cmd.strip()
    air = False; charge = False
    for pre in ('air','j','공중'):
        if cmd.lower().startswith(pre): air = True; cmd = cmd[len(pre):]; break
    seq = []
    i = 0
    while i < len(cmd):
        c = cmd[i]
        if c == '[':                        # [4]6 모으기 — 첫 방향을 길게
            j = cmd.index(']', i)
            seq.append(NUM[cmd[i+1:j]])
            charge = True
            i = j + 1
        elif c in NUM:
            seq.append(NUM[c]); i += 1
        else:
            i += 1                          # 구분자 등 무시
    return seq, air, charge

def flags_of(mv, charge):
    f = 0
    if mv.get('near'): f |= 1
    if mv.get('air'):  f |= 4
    if mv.get('status') in ('가이드만','불일치'): f |= 8   # 미검증
    if charge: f |= 16
    return f

def emit(chars):
    out = []
    w = out.append
    w('/* 자동 생성 — gen_svc_moves.py. 수정하지 말 것. 원본: tools/svc/moves.json */')
    w('#ifndef SVCSP_MOVES_H')
    w('#define SVCSP_MOVES_H')
    w('')
    w('typedef struct { const char *name; const unsigned char *motion; unsigned char len;')
    w('                 unsigned char btn; unsigned char flags;')
    w('                 signed char next, next_hold; } svc_move;   /* 파생(렛카) — 표 인덱스, -1 없음 */')
    w('/* flags: 1=근접 4=공중 8=미검증 16=모으기(첫 방향을 길게) */')
    w('')
    tables = {}
    for ch in chars:
        cid = ch['id']
        moves = []
        for k, mv in enumerate(ch.get('table', [])):
            if mv.get('kind') == 'command_normal':
                continue        # 특수기 — 방향+버튼 한 번이라 원버튼 배치 대상이 아니다
            try: seq, air, charge = parse_cmd(mv['command'])
            except Exception: continue
            if not seq: continue
            if mv.get('air'): air = True
            btn = BTN.get(mv.get('button','P'), 0x10)
            name = mv['name'].replace('"','')
            mo = 'mo_c%d_%d' % (cid, k)
            w('static const unsigned char %s[] = {%s};' % (mo, ','.join('0x%02X'%b for b in seq)))
            fl = flags_of(mv, charge)
            if air: fl |= 4
            moves.append((name, mo, len(seq), btn, fl, mv.get('next'), mv.get('next_hold')))
        if moves:
            mnames=[m[0] for m in moves]
            def midx(nm):
                if not nm: return -1
                for i3,n3 in enumerate(mnames):
                    if nm==n3 or nm in n3 or n3 in nm: return i3
                return -1
            w('static const svc_move mv_c%d[] = {' % cid)
            for name, mo, ln, btn, fl, nx, nxh in moves:
                w('  {"%s", %s, %d, 0x%02X, %d, %d, %d},' % (name, mo, ln, btn, fl, midx(nx), midx(nxh)))
            w('};')
        tables[cid] = (ch, moves)
    w('')
    w('/* 캐릭터별: 기술표 + 슬롯 7자리 (N F B D DF DB AIR — 값은 기술 인덱스, -1 없음) */')
    w('typedef struct { const svc_move *mv; unsigned char n; const char *name; unsigned char cancel_dud; signed char slots[7]; } svc_chartab;')
    w('#define SVC_CHAR_COUNT 18')
    w('static const svc_chartab svc_chars[SVC_CHAR_COUNT] = {')
    for cid in range(18):
        if cid not in tables or not tables[cid][1]:
            w('  { 0, 0, "", 0, {-1,-1,-1,-1,-1,-1,-1} },   /* %d: 기술표 없음 */' % cid)
            continue
        ch, moves = tables[cid]
        names = [m[0] for m in moves]
        sug = ch.get('slot_suggestion') or {}
        slots = []
        for key in ('N','F','B','D','DF','DB','AIR'):
            want = (sug.get(key) or '').replace('"','')
            idx = -1
            if want:
                for i2, n2 in enumerate(names):
                    if want == n2 or want in n2 or n2 in want: idx = i2; break
            slots.append(idx)
        # AIR 슬롯 자동 보정: 지정 없으면 첫 공중기
        if slots[6] < 0:
            for i2, m2 in enumerate(moves):
                if m2[4] & 4: slots[6] = i2; break
        w('  { mv_c%d, %d, "%s", %d, {%s} },' % (cid, len(moves), ch.get('name','?'), 1 if ch.get('cancel_dud') else 0, ','.join(map(str,slots))))
    w('};')
    w('')
    w('#endif')
    return '\n'.join(out) + '\n'

if __name__ == '__main__':
    data = json.load(open(sys.argv[1], encoding='utf-8'))
    chars = data['characters'] if isinstance(data, dict) else data
    sys.stdout.write(emit(chars))
