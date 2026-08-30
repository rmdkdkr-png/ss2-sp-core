#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""sprun — SvC 원버튼 회귀 러너.

  한 줄로 돌린다:   python3 sprun.py            (전부)
                    python3 sprun.py 방향       (이름에 '방향' 든 구간만)
                    python3 sprun.py --list     (구간 목록)

시나리오는 scenarios.tsv 한 곳에 모은다. 고치고 이 러너만 다시 돌리면
전 구간이 같은 기준으로 재검증된다 — 매번 스크립트를 새로 짜지 않는다.

★ 모든 케이스는 대조군(엔진 끔)을 자동으로 함께 돌린다.
  엔진을 꺼도 통과하는 케이스는 아무것도 재고 있지 않다는 뜻이라 FAIL 로 잡는다.
  (기존 chartest 가 'ok or nz or hp>0' 라 엔진 꺼짐에도 18/18 을 냈던 사고 방지)
"""
import subprocess, os, csv, sys, io, glob
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

HERE = os.path.dirname(os.path.abspath(__file__))
R    = os.path.expanduser('~/ss2/repo/ss2-main')
CORE = os.environ.get('SVC_CORE', R + '/build/mednafen_ngp_libretro.so')
ROM  = os.environ.get('SVC_ROM',  os.path.expanduser('~/ss2/rom/svc.ngc'))
SAVE = os.path.expanduser('~/ss2/saves/svc')
SAM  = (4, 10, 16, 24, 36, 48, 60, 80, 100)

def load_scenarios(path):
    """구간\t세이브\t사전입력(|구분)\t트리거\t판정식\t설명"""
    out = []
    for ln in open(path, encoding='utf-8'):
        ln = ln.rstrip('\n')
        if not ln.strip() or ln.lstrip().startswith('#'): continue
        p = ln.split('\t')
        assert len(p) >= 5, '시나리오 형식 오류: ' + ln
        rule = p[4].strip()
        exempt = rule.startswith('~')       # 대조군 검사 면제 — 엔진과 무관한 게 정상인 구간
        out.append(dict(name=p[0], save=p[1], pre=[x for x in p[2].split('|') if x],
                        trig=p[3], rule=rule.lstrip('~'), exempt=exempt,
                        note=p[5] if len(p) > 5 else ''))
    return out

def build_script(cases):
    sc = ['1 -']
    for c in cases:
        st = c['save'] if c['save'].endswith('.st') else 'svc_%s.st' % c['save']
        sc += ['!load %s/%s' % (SAVE, st), '30 -'] + c['pre'] + [c['trig']]
        prev = 0
        for s in SAM:
            sc += ['%d -' % (s - prev), '!w %s@%d' % (c['name'], s)]
            prev = s
    return sc

def run(cases, csv_path, engine_on):
    open('/tmp/sprun.txt', 'w', encoding='utf-8').write('\n'.join(build_script(cases)) + '\n')
    env = dict(os.environ)
    env['PROBE_CSV'] = csv_path
    env['SVCSP_FORCE'] = '1' if engine_on else '0'
    subprocess.run([R + '/tools/svc/svcrun', CORE, ROM, '/tmp/sprun.txt'],
                   capture_output=True, env=env, cwd=R + '/tools/svc')
    rows = {}
    if os.path.exists(csv_path):
        for r in csv.DictReader(open(csv_path)):
            t, at = r['tag'].rsplit('@', 1)
            rows.setdefault(t, {})[int(at)] = r
    return rows

def series(d):
    b  = [int(d[s]['bank'])  for s in SAM if s in d]
    a  = [int(d[s]['anim'])  for s in SAM if s in d]
    hp = 48 - min(int(d[s]['hp2']) for s in SAM if s in d) if d else 0
    fc = [int(d[s]['face'])  for s in SAM if s in d]
    x1 = [int(d[s]['p1x'])   for s in SAM if s in d]
    x2 = [int(d[s]['p2x'])   for s in SAM if s in d]
    return b, a, hp, fc, x1, x2

def judge(rule, d):
    if not d: return False
    b, a, hp, fc, x1, x2 = series(d)
    return bool(eval(rule, {'__builtins__': {}},
                     dict(b=b, a=a, hp=hp, face=fc, x1=x1, x2=x2,
                          min=min, max=max, any=any, all=all, set=set, len=len)))

def main():
    scen = os.path.join(HERE, 'scenarios.tsv')
    cases = load_scenarios(scen)
    args = [x for x in sys.argv[1:] if not x.startswith('-')]
    if '--list' in sys.argv:
        for c in cases: print('%-12s %-10s %s' % (c['name'], c['save'], c['note']))
        return 0
    if args:
        cases = [c for c in cases if any(k in c['name'] or k in c['note'] for k in args)]
        if not cases: print('해당 구간 없음'); return 1

    on  = run(cases, '/tmp/sprun_on.csv',  True)
    off = run(cases, '/tmp/sprun_off.csv', False)

    print('%-12s %-30s %5s %6s %s' % ('구간', '뱅크 흐름', '피해', '대조군', '판정'))
    print('-' * 78)
    npass = 0
    fails = []
    for c in cases:
        d  = on.get(c['name'], {})
        d0 = off.get(c['name'], {})
        ok  = judge(c['rule'], d)
        ok0 = judge(c['rule'], d0)          # 엔진 꺼도 통과하면 지표가 죽은 것
        b   = series(d)[0] if d else []
        hp  = series(d)[2] if d else 0
        if c['exempt']:
            verdict = 'PASS' if ok else 'FAIL'
        else:
            verdict = 'PASS' if (ok and not ok0) else ('무효지표' if (ok and ok0) else 'FAIL')
        if verdict == 'PASS': npass += 1
        else: fails.append((c, verdict))
        print('%-12s %-30s %5d %6s %s' % (
            c['name'], ','.join(map(str, b))[:30], hp, '통과' if ok0 else '-', verdict))
    print('-' * 78)
    print('== %d/%d 통과 ==' % (npass, len(cases)))
    if fails:
        print()
        for c, v in fails:
            print('  [%s] %-12s %s' % (v, c['name'], c['note']))
            print('       기대: %s' % c['rule'])
    return 0 if npass == len(cases) else 1

if __name__ == '__main__':
    sys.exit(main())
