#!/usr/bin/env python3
"""초필 정밀 재검증 — 자연 감쇠 보정: 무입력 대조군과 같은 프레임의 게이지 비교.
   판정: 어느 표본에서든 (대조군 - 시험군) >= 25 → 게이지 추가 소모 = 초필 발동"""
import subprocess, os, csv, json, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

CORE=os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM =os.path.expanduser('~/ss2/rom/svc.ngc')
D={'1':'D L','2':'D','3':'D R','4':'L','5':'-','6':'R','7':'U L','8':'U','9':'U R'}
BT={'P':'B','K':'A'}
SUPER_BANKS={35,27,43,7,21}   # 이번 실측에서 초필에만 나타난 뱅크 (쿄 무식=21 포함)

st_of={}
for line in open('chars.txt'):
    p=line.split()
    if len(p)==2: st_of[int(p[0])]='svc_%s.st'%p[1]
mv=json.load(open('moves.json',encoding='utf-8'))

def cmd_lines(cmd, btn):
    cmd=cmd.strip(); L=[]
    i=0; last='5'
    while i<len(cmd):
        c=cmd[i]
        if c=='[':
            j=cmd.index(']',i); d2=cmd[i+1:j]; L.append('40 %s'%D[d2]); last=d2; i=j+1
        elif c in D: L.append('3 %s'%D[c]); last=c; i+=1
        else: i+=1
    L.append(('3 %s %s'%(D[last] if last!='5' else '',BT[btn])).replace('  ',' ').strip())
    return L

SAM=(4,12,22,34,48,64,84,110)
def probe(tag, st, lines):
    sc=['!load %s'%st,'20 -','!poke 0963=240','10 -']+lines
    prev=0
    for s in SAM: sc+=['%d -'%(s-prev),'!w %s@%d'%(tag,s)]; prev=s
    return sc

def run(script,csvn):
    open('t.txt','w').write('\n'.join(script)+'\n')
    env=dict(os.environ); env['PROBE_CSV']=csvn
    subprocess.run(['./svcrun',CORE,ROM,'t.txt'],capture_output=True,env=env,timeout=900)

targets=[]
ctrl_needed=set()
for c in mv['characters']:
    st=st_of.get(c['id'])
    if not st or not os.path.exists(st): continue
    for k,m in enumerate(c['table']):
        if m.get('kind')!='super': continue
        targets.append((c['id'],c['name'],k,m))
        ctrl_needed.add(c['id'])
print('초필 %d건 / 대조군 %d개'%(len(targets),len(ctrl_needed)))

sc=['1 -']
for cid in sorted(ctrl_needed):
    # 대조군: 커맨드 길이만큼 중립 대기 후 같은 표본 (모션 프레임 근사 15f)
    sc+=probe('ctl%d'%cid, st_of[cid], ['15 -'])
for cid,cn,k,m in targets:
    sc+=probe('s%d_%d'%(cid,k), st_of[cid], cmd_lines(m['command'],m.get('button','P')))
run(sc,'sv.csv')

rows={}
for r in csv.DictReader(open('sv.csv')):
    t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r

print()
print('%-3s %-6s %-24s %-9s %-6s %s'%('ID','캐릭','초필','커맨드','판정','근거'))
ok_list=[]; ng_list=[]
for cid,cn,k,m in targets:
    d=rows.get('s%d_%d'%(cid,k),{}); ct=rows.get('ctl%d'%cid,{})
    if not d or not ct: print('%-3d %-6s %-24s 데이터없음'%(cid,cn,m['name'][:22])); continue
    diffs=[]
    for s in SAM:
        if s in d and s in ct:
            diffs.append(int(ct[s]['pow1'])-int(d[s]['pow1']))
    extra=max(diffs) if diffs else 0
    banks=set(int(d[s]['bank']) for s in SAM if s in d)-{255}
    sb=banks & SUPER_BANKS
    hp=48-min(int(d[s]['hp2']) for s in SAM if s in d)
    fired = extra>=25 or bool(sb)
    why='게이지+%d%s%s'%(extra,' 뱅크%s'%sorted(sb) if sb else '',' 피해%d'%hp if hp else '')
    print('%-3d %-6s %-24s %-9s %-6s %s'%(cid,cn[:6],m['name'][:22],m['command'][:9],
          '✅발동' if fired else '❌불발',why))
    (ok_list if fired else ng_list).append((cid,k))
print()
print('발동 %d / 불발 %d'%(len(ok_list),len(ng_list)))
json.dump({'ok':ok_list,'ng':ng_list}, open('super_results.json','w'))
