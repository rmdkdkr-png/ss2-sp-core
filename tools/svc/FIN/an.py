import csv,sys
def I(r,k): return int(r[k])
for path in sys.argv[1:]:
    rows=list(csv.DictReader(open(path)))
    live=[r for r in rows if I(r,'chr1')!=255]
    b1=b2=hi=0
    for r in live:
        if I(r,'p1w')-I(r,'p1s')!=I(r,'cam19A6'): b1+=1
        if I(r,'p2w')-I(r,'p2s')!=I(r,'cam19A6'): b2+=1
        if I(r,'a92F') or I(r,'aAAF'): hi+=1
    print(path,'rows',len(rows),'live',len(live))
    print('   viol p1w-p1s!=cam:',b1,' p2w-p2s!=cam:',b2,' hibyte 92F/AAF nonzero:',hi)
    def mm(k): return (min(I(r,k) for r in live), max(I(r,k) for r in live))
    print('   ranges:', {k:mm(k) for k in ('p1s','p1w','p2s','p2w','cam19A6','a935','aAB5')})
    print('   a92C set:', sorted(set(I(r,'a92C') for r in live)),
          ' aAAC set:', sorted(set(I(r,'aAAC') for r in live)),
          ' a0954 set:', sorted(set(I(r,'a0954') for r in live))[:8])
    ag=dis=cr=0; ex=[]
    for r in live:
        fl=1 if (I(r,'a92C')&0x80) else 0
        xw=1 if I(r,'p1w')>I(r,'p2w') else 0
        if xw: cr+=1
        if fl==xw: ag+=1
        else:
            dis+=1
            if len(ex)<12: ex.append((r['tag'],I(r,'p1w'),I(r,'p2w'),I(r,'a92C')))
    print('   face(92C.7) vs p1w>p2w: agree',ag,'disagree',dis,'| p1w>p2w frames:',cr)
    if ex: print('   disagree samples:',ex)
    # old engine formula
    old=sum(1 for r in live if I(r,'p1s')>I(r,'p1w'))
    print('   OLD formula x16(092E)>x16(0934) true count:',old)
    print()
