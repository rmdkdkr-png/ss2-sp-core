import sys, csv
f=sys.argv[1]
step=int(sys.argv[2]) if len(sys.argv)>2 else 10
rows=list(csv.DictReader(open(f)))
hdr=[k for k in rows[0].keys() if k.startswith('w')]
print('lab      frame  ' + '  '.join(h.ljust(7) for h in hdr))
prev=None
for i,r in enumerate(rows):
    cur=tuple(r[h] for h in hdr)
    show = (i%step==0) or (i==len(rows)-1) or (prev is not None and cur!=prev and step<0)
    if show:
        print(r['lab'][:8].ljust(8), r['frame'].rjust(5), '  ' + '  '.join(r[h].rjust(7) for h in hdr))
    prev=cur
# summary per label: min/max
print('--- per-label ranges ---')
labs=[]
for r in rows:
    if not labs or labs[-1][0]!=r['lab']: labs.append((r['lab'],[]))
    labs[-1][1].append(r)
for lab,rs in labs:
    out=[]
    for h in hdr:
        v=[int(x[h]) for x in rs]
        out.append(f"{h}:{min(v)}..{max(v)}({v[0]}->{v[-1]})")
    print(lab.ljust(8), ' '.join(out))
