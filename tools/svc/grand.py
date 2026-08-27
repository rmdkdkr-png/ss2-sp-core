#!/usr/bin/env python3
"""미검증 전량 일괄 검증 — 18명 × 가이드만/불일치 항목 전부.
   게이지 240 선주입 후 정확한 커맨드 주입, 4중 판정:
   P카운터(anim)·K카운터(kanim) 리셋 / 뱅크 이탈 / 피해 / 게이지 소모(초필)"""
import subprocess, os, csv, json, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

CORE=os.path.expanduser('~/ss2/repo/ss2-sp-core/build/mednafen_ngp_libretro.so')
ROM =os.path.expanduser('~/ss2/rom/svc.ngc')
D={'1':'D L','2':'D','3':'D R','4':'L','5':'-','6':'R','7':'U L','8':'U','9':'U R'}
BT={'P':'B','K':'A'}

st_of={}
for line in open('chars.txt'):
    p=line.split()
    if len(p)==2: st_of[int(p[0])]='svc_%s.st'%p[1]

mv=json.load(open('moves.json',encoding='utf-8'))

def cmd_lines(cmd, btn):
    cmd=cmd.strip(); air=False
    for pre in ('air','j.','j','공중'):
        if cmd.lower().startswith(pre): air=True; cmd=cmd[len(pre):]; break
    L=[]; last='5'
    i=0
    while i<len(cmd):
        c=cmd[i]
        if c=='[':
            j=cmd.index(']',i); d=cmd[i+1:j]
            L.append('40 %s'%D[d]); last=d; i=j+1
        elif c in D:
            L.append('3 %s'%D[c]); last=c; i+=1
        else: i+=1
    hold = '3 %s %s' if last!='5' else '3 %s%s'
    ld = D[last] if last!='5' else ''
    L.append(('3 %s %s'%(ld,BT[btn])).replace('  ',' ').strip())
    return L, air

SAM=(4,10,16,24,36,48,64,84)
def probe(tag, st, lines, air):
    sc=['!load %s'%st,'20 -','!poke 0963=240','10 -']
    if air: sc+=['2 U','12 -']
    sc+=lines
    prev=0
    for s in SAM: sc+=['%d -'%(s-prev),'!w %s@%d'%(tag,s)]; prev=s
    return sc

def run(script,csvn):
    open('t.txt','w').write('\n'.join(script)+'\n')
    env=dict(os.environ); env['PROBE_CSV']=csvn
    subprocess.run(['./svcrun',CORE,ROM,'t.txt'],capture_output=True,env=env,timeout=900)

# 대상: 확정이 아닌 모든 항목
targets=[]
for c in mv['characters']:
    st=st_of.get(c['id'])
    if not st or not os.path.exists(st): continue
    for k,m in enumerate(c['table']):
        if m.get('status')=='확정': continue
        targets.append((c['id'],c['name'],k,m))
print('검증 대상 %d건'%len(targets))

results={}
CH=14
for ci in range(0,len(targets),CH):
    chunk=targets[ci:ci+CH]
    sc=['1 -']
    for cid,cn,k,m in chunk:
        lines,air=cmd_lines(m['command'], m.get('button','P'))
        air = air or bool(m.get('air'))
        sc+=probe('t%d_%d'%(cid,k), st_of[cid], lines, air)
    run(sc,'gv.csv')
    rows={}
    for r in csv.DictReader(open('gv.csv')):
        t,at=r['tag'].rsplit('@',1); rows.setdefault(t,{})[int(at)]=r
    for cid,cn,k,m in chunk:
        d=rows.get('t%d_%d'%(cid,k),{})
        if not d: results[(cid,k)]=('무데이터','',0,0); continue
        ks=sorted(d)
        anim=[int(d[s]['anim']) for s in ks]; kan=[int(d[s]['kanim']) for s in ks]
        bank=[int(d[s]['bank']) for s in ks]
        air = bool(m.get('air'))
        jb={255,4,5} if air else {255}
        nz=[x for x in bank if x not in jb]
        hp=48-min(int(d[s]['hp2']) for s in ks)
        pw=min(int(d[s]['pow1']) for s in ks)
        used=240-pw
        reset = any(a[i]<a[i-1] for a in (anim,kan) for i in range(1,len(a))) or anim[0]<=8 or kan[0]<=8
        is_super = m.get('kind')=='super'
        if is_super:
            ok = used>=20
            verdict='확정' if ok else ('발동체계외' if (nz or hp>0) else '불발(게이지불소모)')
        else:
            ok = bool(nz) or hp>0
            verdict='확정' if ok else ('동작만(뱅크무)' if reset else '불발')
        results[(cid,k)]=(verdict, ','.join(map(str,sorted(set(nz))[:3])), hp, used)

print()
print('%-3s %-6s %-22s %-10s %-8s %4s %4s'%('ID','캐릭','기술','커맨드','판정','피해','겍소모'))
up=0
for cid,cn,k,m in targets:
    v,b,hp,used=results.get((cid,k),('?','',0,0))
    mark='✅' if v=='확정' else ('❓' if v.startswith('동작만') else '❌')
    print('%-3d %-6s %-22s %-10s %s%-8s %4d %4d  뱅크[%s]'%(cid,cn[:6],m['name'][:20],m['command'][:10],mark,v,hp,used,b))
    if v=='확정': up+=1
print()
print('== 확정 승격 %d / %d =='%(up,len(targets)))
json.dump({str(k):v for k,v in {('%d_%d'%kk):vv for kk,vv in results.items()}.items()},
          open('grand_results.json','w',encoding='utf-8'), ensure_ascii=False)
