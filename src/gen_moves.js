/* runner.html의 CHARS 배열을 그대로 평가해 C 헤더로 변환 */
const fs=require('fs');
const src=fs.readFileSync('/home/claude/ejs/webroot/runner.html','utf8').split('\n');
const start=src.findIndex(l=>l.startsWith('const CHARS = ['));
let end=start; while(!/^\];/.test(src[end])) end++;
const CHARS=eval(src.slice(start,end+1).join('\n').replace(/^const CHARS = /,'')+'');
const DIR={'1':0x06,'2':0x02,'3':0x0A,'4':0x04,'5':0x00,'6':0x08,'7':0x05,'8':0x01,'9':0x09};
let out=[];
out.push('/* 자동 생성 — gen_moves.js. 수정하지 말 것. */');
out.push('#ifndef SS2SP_MOVES_H\n#define SS2SP_MOVES_H\n');
out.push('typedef struct { const char *name; const unsigned char *motion; unsigned char len;');
out.push('                unsigned char btn; unsigned char flags; } ss2_move;');
out.push('typedef struct { const char *id; const ss2_move *mv; unsigned char n; } ss2_style;');
out.push('/* flags: 1=near(근접) 2=card(카드필요) 4=air 8=unverified 16=grab */\n');
/* 내부 캐릭터 인덱스 = 0x1B51 >> 3 을 2로 나눈 값. runner.html의 CHAR_BY_IDX와 동일해야 한다. */
const CHAR_BY_IDX={0:"kazuki",1:"sogetsu",2:"haohmaru",3:"genjuro",4:"nakoruru",5:"rimururu",
                   6:"hanzo",7:"galford",8:"asura",9:"charlotte",10:"morozumi",11:"ukyo",
                   12:"jubei",13:"shiki",14:"yuga"};
const byId={}; CHARS.forEach(c=>byId[c.id]=c);
let styles=[];
const order=[];
for(let i=0;i<=14;i++){ const id=CHAR_BY_IDX[i]; order.push(byId[id]||{id:"unknown"+i,s:[],bst:[]}); }
order.forEach((c,ci)=>{
  ['s','bst'].forEach((k,si)=>{
    const list=c[k]||[];
    const tn=`mv_${c.id}_${k}`;
    list.forEach((m,mi)=>{
      const bytes=[...m.m].map(d=>DIR[d]);
      out.push(`static const unsigned char mo_${c.id}_${k}_${mi}[] = {${bytes.map(b=>'0x'+b.toString(16)).join(',')}};`);
    });
    out.push(`static const ss2_move ${tn}[] = {`);
    list.forEach((m,mi)=>{
      const fl=(m.near?1:0)|(m.card?2:0)|(m.air?4:0)|(m.unv?8:0)|(m.grab?16:0);
      out.push(`  {"${m.n}", mo_${c.id}_${k}_${mi}, ${m.m.length}, ${m.b==='A'?0x10:0x20}, ${fl}},`);
    });
    out.push('};');
    styles.push(`  {"${c.id}_${k}", ${tn}, ${list.length}},`);
  });
});
/* ── SP 슬롯 기본 배치 — runner.html의 deriveSpMap을 그대로 옮긴 것 ── */
function deriveSpMap(moves){
  const map={n:null,f:null,d:null,b:null,df:null,db:null,air:null};
  const take=(k,i)=>{ if(map[k]===null) map[k]=i; };
  moves.forEach((mv,i)=>{
    if(mv.unv||mv.card||mv.sup) return;
    if(mv.air){ take("air",i); return; }
    if(mv.near){ take("d",i); return; }
    if(mv.m==="623") take("f",i);
    else if(mv.m==="236") take("n",i);
    else if(["214","421","412"].includes(mv.m)) take("b",i);
    else take("f",i);
  });
  moves.forEach((mv,i)=>{
    if(mv.unv||mv.card||mv.sup||mv.air) return;
    if(!Object.values(map).includes(i)){
      for(const k of ["n","f","d","b"]) if(map[k]===null){ map[k]=i; break; }
    }
  });
  return map;
}
const SLOTS=["n","f","b","d","df","db","air"];
out.push('\n/* SP 슬롯 기본 배치 (deriveSpMap + 캐릭터 오버라이드). 순서: n f b d df db air, -1 = 없음 */');
out.push('static const signed char ss2_spmap[][7] = {');
order.forEach(c=>{
  ['s','bst'].forEach(k=>{
    const list=c[k]||[];
    const map=deriveSpMap(list);
    const ov=(k==='bst')?c.spB:c.spS;
    if(ov) for(const kk in ov) map[kk]=ov[kk];
    out.push('  {'+SLOTS.map(sl=>{const v=map[sl]; return (v===null||v===undefined||v>=list.length)?-1:v;}).join(',')+`},  /* ${c.id}_${k} */`);
  });
});
out.push('};');
out.push("#define SS2_SLOT_N 0\n#define SS2_SLOT_F 1\n#define SS2_SLOT_B 2\n#define SS2_SLOT_D 3\n#define SS2_SLOT_DF 4\n#define SS2_SLOT_DB 5\n#define SS2_SLOT_AIR 6");
out.push('\n/* 인덱스 = (0x1B51 >> 3). 내부 캐릭터 순서 = CHAR_BY_IDX (runner.html과 동일) */');
out.push('static const ss2_style ss2_styles[] = {');
out.push(...styles);
out.push('};');
out.push(`#define SS2_STYLE_COUNT ${styles.length}`);
out.push('\n#endif');
fs.writeFileSync('ss2sp_moves.h', out.join('\n')+'\n');
console.log('캐릭터', CHARS.length, '· 스타일', styles.length, '· 기술',
  CHARS.reduce((a,c)=>a+(c.s||[]).length+(c.bst||[]).length,0));
