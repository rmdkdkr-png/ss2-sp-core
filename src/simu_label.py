# -*- coding: utf-8 -*-
"""시뮬레이터가 뱉은 대사를 **이벤트 이름으로 되짚고**, 안 나온 이벤트를 찾는다.
   옆방 evindex.py 의 방식을 그대로 옮겼다 — 대사표에서 서식(%s/%d)을 뺀
   가장 긴 조각을 열쇠로 삼아 역인덱스를 만든다."""
import io, os, re, sys, collections
D   = os.path.dirname(os.path.abspath(__file__))
LOG = sys.argv[1] if len(sys.argv) > 1 else "/tmp/sim/log.tsv"
src = io.open(os.path.join(D, "ss2comm_lines.h"), encoding="utf-8").read()

ev  = re.search(r"enum \{(.*?)EV_N", src, re.S).group(1)
EV  = [x.strip() for x in re.sub(r"/\*.*?\*/", "", ev, flags=re.S).replace("\n", " ").split(",") if x.strip()]
m   = re.search(r"LINES\[SS2COMM_SPK_N\]\[EV_N\]\[EVMAXV\] = \{(.*)\n\};", src, re.S)
IDX = {}
for name, p in re.findall(r"/\* *([A-Z0-9_]+) *\*/ *\{(.*?)\},?\n", m.group(1)):
    for s in re.findall(r'"((?:[^"\\]|\\.)*)"', p):
        key = max(re.split(r"%[sd]", s), key=len).strip(" ,…·—-!?.")
        if len(key) >= 4: IDX.setdefault(key, "EV_" + name)
# 심판·썰·무기는 LINES 밖이다 — 따로 잡는다
REF  = [x for x in re.findall(r'"((?:[^"\\]|\\.)*)"', src) if "정정당당히" in x]
REL = set()
mr = re.search(r"static const char \*RELLINE\[SS2COMM_SPK_N\]\[\d+\] = \{(.*?)\n\};", src, re.S)
if mr:
    for s2 in re.findall(r'"((?:[^"\\]|\\.)*)"', mr.group(1)): REL.add(s2.strip())
ANEC = set()
for blk in ("ANEC", "WEAP"):
    mm = re.search(r"static const char \*%s\[\d+\]\[\d+\] = \{(.*?)\n\};" % blk, src, re.S)
    if mm:
        for s in re.findall(r'"((?:[^"\\]|\\.)*)"', mm.group(1)): ANEC.add(s.strip())
def label(t):
    t = t.strip()
    if "정정당당히" in t or t.endswith("훌륭하오!"): return "REFEREE"
    if t in ANEC: return "ANEC/WEAP"
    if t in REL:  return "EV_REL"
    for k, v in IDX.items():
        if k in t: return v
    return "?"

rows = [l.rstrip("\n").split("\t") for l in io.open(LOG, encoding="utf-8")][1:]
seen = collections.Counter(); byscen = collections.defaultdict(collections.Counter); unk = []
for r in rows:
    if len(r) < 4: continue
    e = label(r[3]); seen[e] += 1; byscen[r[1]][e] += 1
    if e == "?": unk.append((r[1], r[3]))

print("대사 %d줄 / 이벤트 %d종" % (len(rows), len([k for k in seen if k not in ("?",)])))
want = set(EV) | {"REFEREE", "ANEC/WEAP"}
missing = sorted(want - set(seen))
print("\n─ 한 번도 안 나온 이벤트 %d개 ─" % len(missing))
for e in missing: print("   ", e)
if unk:
    print("\n─ 대사표에서 못 찾은 줄 %d개 ─" % len(unk))
    for sc, t in unk[:12]: print("    [%s] %s" % (sc, t))
print("\n─ 시나리오별 ─")
for sc in byscen:
    if sc.startswith("화자"): continue
    print("  %-18s %s" % (sc, " ".join("%s×%d" % (k, v) if v > 1 else k for k, v in byscen[sc].most_common())))
