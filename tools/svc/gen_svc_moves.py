#!/usr/bin/env python3
"""svcsp_moves.h 생성기 — 기술표 JSON(교차검증 산출물) → C 헤더.
   사용: gen_svc_moves.py moves.json > svcsp_moves.h
   SS2 의 gen_moves.js 와 같은 역할. 수동 수정 금지, JSON 을 고치고 재생성할 것."""
import json, re, sys

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
    if mv.get('kind') == 'super': f |= 32   # 초필 — 러시 마무리 픽커용
    return f

def emit(chars):
    out = []
    w = out.append
    w('/* 자동 생성 — gen_svc_moves.py. 수정하지 말 것. 원본: tools/svc/moves.json */')
    w('#ifndef SVCSP_MOVES_H')
    w('#define SVCSP_MOVES_H')
    w('')
    w('typedef struct { const char *name; const char *name_hold; const unsigned char *motion;')
    w('                 unsigned char len; unsigned char btn; unsigned char flags;')
    w('                 signed char next, next_hold, next_k; } svc_move;   /* 파생(렛카) — 표 인덱스, -1 없음.\n                    next=탭 갈래 next_hold=홀드 갈래 next_k=킥 갈래(125식 칠뢰)\n                    name_hold = 같은 커맨드의 **강판 이름**(쿄 황물기→독물기). 없으면 0.\n                    표를 병합해 둔 탓에 자막이 약·강을 구분 못 하던 것을 이걸로 가른다. */')
    w('/* flags: 1=근접 4=공중 8=미검증 16=모으기(첫 방향을 길게) 32=초필 */')
    w('')
    tables = {}
    for ch in chars:
        cid = ch['id']
        moves = []
        for k, mv in enumerate(ch.get('table', [])):
            _air_cn = mv.get('air') or str(mv.get('command','')).lower().startswith(('air','j'))
            if mv.get('kind') == 'command_normal' and not _air_cn:
                continue        # 지상 특수기 — 방향+버튼 한 번이라 원버튼 배치 대상이 아니다
                                # (공중 전용 특수기는 남긴다: 공중 X 한 방의 가치)
            try: seq, air, charge = parse_cmd(mv['command'])
            except Exception: continue
            if not seq: continue
            if mv.get('air'): air = True
            btn = BTN.get(mv.get('button','P'), 0x10)
            name = mv['name'].replace('"','')
            # 강판 이름. 구조화된 name_hold 가 있으면 그것, 없으면 notes 의
            # 「홀드=강판: <이름>」 를 읽는다 — 표를 병합할 때 자유문에만 적어 뒀다(§26).
            nh = mv.get('name_hold')
            if not nh:
                m_ = re.search(r'홀드\s*=\s*강판\s*[:：]\s*([^|/]+)', str(mv.get('notes','')))
                if m_: nh = m_.group(1).strip()
            nh = nh.replace('"','') if nh else None
            mo = 'mo_c%d_%d' % (cid, k)
            w('static const unsigned char %s[] = {%s};' % (mo, ','.join('0x%02X'%b for b in seq)))
            fl = flags_of(mv, charge)
            if air: fl |= 4
            moves.append((name, mo, len(seq), btn, fl, mv.get('next'), mv.get('next_hold'), mv.get('next_k'), nh))
        if moves:
            mnames=[m[0] for m in moves]
            def midx(nm):
                """이름 → 표 인덱스. **정확히 같은 것을 먼저** 찾는다.
                   부분일치를 먼저 보면 「아오이하나 2타」가 「아오이하나」에 걸려
                   자기 앞 기술을 가리킨다(실제로 한 번 그렇게 붙었다)."""
                if not nm: return -1
                for i3,n3 in enumerate(mnames):
                    if nm==n3: return i3
                for i3,n3 in enumerate(mnames):
                    if nm in n3 or n3 in nm: return i3
                return -1
            w('static const svc_move mv_c%d[] = {' % cid)
            for name, mo, ln, btn, fl, nx, nxh, nxk, nh in moves:
                w('  {"%s", %s, %s, %d, 0x%02X, %d, %d, %d, %d},'
                  % (name, ('"%s"' % nh) if nh else '0', mo, ln, btn, fl,
                     midx(nx), midx(nxh), midx(nxk)))
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
        # 초필 후순위: 슬롯에 초필이 지정됐는데 아직 안 쓴 일반 필살기가 남아 있으면 그것을 먼저.
        # 게이지가 없으면 초필은 안 나가서 그 방향이 죽은 슬롯이 된다 (제보).
        # 대체할 일반기가 없으면 그대로 둔다 — 빼면 슬롯이 비어 손해다.
        derived_names = set()
        for m2 in moves:
            for ni in (5, 6, 7):                   # next / next_hold / next_k — 이 시점엔 **이름 문자열**
                if m2[ni]: derived_names.add(m2[ni])
        def is_derived(nm):
            return any(nm == dn or nm in dn or dn in nm for dn in derived_names)
        for si in range(7):
            if ch.get('slots_literal'): break   # 유저가 명시한 배치는 그대로 (초필 후순위 규칙 생략)
            if slots[si] < 0 or not (moves[slots[si]][4] & 32): continue   # 32 = 초필
            for i2, m2 in enumerate(moves):
                if i2 in slots or is_derived(m2[0]): continue
                if m2[5] or m2[6] or m2[7]: continue                       # 파생을 갖는 기술 = 시동기·파생계열,
                                                                           # 단독 배치는 위험 (실측: 팔청을 쏘면 게임이 구상으로 받는다)
                if m2[4] & 32: continue                                    # 초필끼리 교체는 무의미
                if si == 6 and not (m2[4] & 4): continue                   # AIR 은 공중기만
                if si != 6 and (m2[4] & 4): continue                       # 지상 슬롯에 공중기 금지
                slots[si] = i2; break

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
