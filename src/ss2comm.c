/* ss2comm — 사쇼!2 캐릭터 해설 엔진 (C 이식본)
   브라우저판 runner.html 의 commStep() 을 그대로 옮긴 것. 상태 함수라 이식이 쉬웠다.
   좌표 규약: 모든 오프셋은 SYSTEM_RAM(CPUExRAM) 기준. CPU 주소 = 0x4000 + 오프셋.

   실측으로 확정한 신호 (인계 문서 참조):
     0x00A7 MODE   240 메뉴·연출 / 241 전투 / 197 캐릭터 대사 화면 / 199 엔딩·스태프롤
     0x01C0 SCR    0 타이틀·메뉴 / 2 캐릭터선택 / 4 유파선택 / 6 카드 / 8·10·12·14 전투
     0x1A46 P1 HP  0x1C46 P2 HP (max 128)
     0x0E3E P1 ACT (u16, 필살기 >= 0x180)   0x0E7E P2 ACT
     0x1B51 P1 블록(캐릭터·유파)  0x1D51 P2 블록
     0x180B 무한대전 연승 카운터 (스토리에서는 0)
     0x17FE 스토리 스테이지 (0-based, 무한대전에서는 0)
   기술명은 롬 버전 종속이라 코어/앱에서는 다루지 않는다 — 흥·썰 위주. */
#include <stdio.h>
#include <string.h>
#include "ss2comm.h"

#ifdef SS2SP_RAM_POINTER
static uint8_t *ss2c_ram = 0;
void ss2comm_set_ram(void *p) { ss2c_ram = (uint8_t *)p; }
#define RAM ss2c_ram
#else
extern uint8_t CPUExRAM[16384];
void ss2comm_set_ram(void *p) { (void)p; }
#define RAM CPUExRAM
#endif

#define OFF_MODE   0x00A7
#define OFF_SCR    0x01C0
#define OFF_HP1    0x1A46
#define OFF_HP2    0x1C46
#define OFF_ACT1   0x0E3E
#define OFF_ACT2   0x0E7E
#define OFF_BLK1   0x1B51
#define OFF_BLK2   0x1D51
#define OFF_SURV   0x180B
#define OFF_STAGE  0x17FE
#define OFF_PAD    0x2F82   /* 패드 레지스터 — 자동 전환(무입력 화면 넘김) 판별용 */
#define MD_BATTLE  241
#define MD_MENU    240
#define MD_QUOTE   197
#define MD_ENDING  199



#include "ss2comm_lines.h"
#include "ss2comm_duo.h"

static const char *CHARNAME[15] = {
  "카즈키","소게츠","하오마루","겐주로","나코루루","리무루루","한조","갈포드",
  "아수라","샤를로트","모로즈미","우쿄","쥬베이","시키","유가"
};

/* ── 쿨다운 키 — 브라우저판 commEmit 의 키를 그대로 옮겼다.
      같은 키를 쓰는 이벤트는 쿨다운을 나눠 쓴다(예: ko/koed/dko/moveKo = "ko"). */
enum { CK_ROUND, CK_ROUNDCTX, CK_KO, CK_MV, CK_HIT, CK_TK, CK_DN, CK_DN2, CK_OSP,
       CK_LOW, CK_LOW2X, CK_REV, CK_PFT, CK_CBK, CK_QKO, CK_RVG, CK_STK,
       CK_SURV, CK_SURV2, CK_STG, CK_QUOTE, CK_ENDING, CK_RES, CK_RES2, CK_REC,
       CK_VSQ, CK_STORY, CK_SCR0, CK_SCR2, CK_SCR4, CK_SCR6, CK_SELCHAT, CK_MIDLE,
       CK_FB, CK_IDLE, CK_LONG, CK_MUSE, CK_LORE,
       CK_FLOW, CK_ARC, CK_N };

/* {쿨다운 키, 쿨다운(프레임 · 60fps 기준)} — 브라우저판의 ms 값을 프레임으로 옮긴 것 */
static const struct { unsigned char key; unsigned short cool; } EVCD[EV_N] = {
  [EV_START      ] = { CK_ROUND, 150 },
  [EV_ROUND      ] = { CK_ROUND, 150 },
  [EV_KO         ] = { CK_KO, 240 },
  [EV_KOED       ] = { CK_KO, 240 },
  [EV_DKO        ] = { CK_KO, 240 },
  [EV_PERFECT    ] = { CK_PFT, 900 },
  [EV_COMEBACK   ] = { CK_CBK, 900 },
  [EV_QUICK      ] = { CK_QKO, 900 },
  [EV_LOW1       ] = { CK_LOW, 90 },
  [EV_LOW2       ] = { CK_LOW, 90 },
  [EV_REVERSAL   ] = { CK_REV, 360 },
  [EV_WINTALK    ] = { CK_RES2, 420 },
  [EV_LOSETALK   ] = { CK_RES2, 420 },
  [EV_SURV       ] = { CK_SURV, 480 },
  [EV_SURVEND    ] = { CK_SURV2, 600 },
  [EV_STAGE      ] = { CK_STG, 480 },
  [EV_QUOTE      ] = { CK_QUOTE, 540 },
  [EV_ENDING     ] = { CK_ENDING, 5400 },
  [EV_CHARSEL    ] = { CK_SCR2, 900 },
  [EV_STYLESEL   ] = { CK_SCR4, 720 },
  [EV_CARDSEL    ] = { CK_SCR6, 1200 },
  [EV_TITLE      ] = { CK_SCR0, 1800 },
  [EV_MUSE_B     ] = { CK_MUSE, 0 },
  [EV_MUSE_Q     ] = { CK_MUSE, 0 },
  [EV_MUSE_M     ] = { CK_MUSE, 0 },
  [EV_IDLE       ] = { CK_IDLE, 1500 },
  [EV_VSQ        ] = { CK_VSQ, 900 },
  [EV_STORYCHAT  ] = { CK_STORY, 900 },
  [EV_CHARSELCHAT] = { CK_SELCHAT, 300 },
  [EV_MENUIDLE   ] = { CK_MIDLE, 2100 },
  [EV_FIRSTBLOOD ] = { CK_FB, 1800 },
  [EV_ROUNDLEAD  ] = { CK_ROUNDCTX, 150 },
  [EV_ROUNDBEHIND] = { CK_ROUNDCTX, 150 },
  [EV_MATCHPOINT ] = { CK_ROUNDCTX, 150 },
  [EV_DOUBLELOW  ] = { CK_LOW2X, 480 },
  [EV_LONGFIGHT  ] = { CK_LONG, 3600 },
  [EV_HIT        ] = { CK_HIT, 48 },
  [EV_TAKEN      ] = { CK_TK, 54 },
  [EV_DOWN       ] = { CK_DN, 120 },
  [EV_DOWNED     ] = { CK_DN2, 120 },
  [EV_OPPSP      ] = { CK_OSP, 132 },
  [EV_MOVE       ] = { CK_MV, 66 },
  [EV_MOVEHIT    ] = { CK_MV, 66 },
  [EV_MOVEHITL   ] = { CK_MV, 66 },
  [EV_MOVEDOWN   ] = { CK_MV, 66 },
  [EV_MOVEDOWNA  ] = { CK_MV, 66 },
  [EV_MOVEKO     ] = { CK_KO, 240 },
  [EV_REVENGE    ] = { CK_RVG, 900 },
  [EV_STREAK     ] = { CK_STK, 300 },
  [EV_RECORD     ] = { CK_REC, 360 },
  [EV_WINSCR     ] = { CK_RES, 480 },
  [EV_LOSESCR    ] = { CK_RES, 480 },
  [EV_REL        ] = { CK_LORE, 300 },
  [EV_LORE       ] = { CK_LORE, 300 },
  [EV_FLOWSAME   ] = { CK_FLOW, 720 },
  [EV_FLOWTRADE  ] = { CK_FLOW, 720 },
  [EV_FLOWONE    ] = { CK_FLOW, 720 },
  [EV_FLOWCHASE  ] = { CK_FLOW, 720 },
  [EV_FLOWSP     ] = { CK_FLOW, 720 },
  [EV_ARCSWEEP   ] = { CK_ARC, 420 },
  [EV_ARCSWEPT   ] = { CK_ARC, 420 },
  [EV_ARCCOMEBACK] = { CK_ARC, 420 },
  [EV_ARCSWEAT   ] = { CK_ARC, 420 },
  [EV_ARCCHOKE   ] = { CK_ARC, 420 },
  [EV_ARCSLIP    ] = { CK_ARC, 420 },
};

/* ── 상태 ── */
static int  cm_on = 1, cm_spk = 0;
static unsigned cm_f = 0;                 /* 프레임 카운터 */
static unsigned cd[CK_N];                 /* 쿨다운 만료 프레임 */
static int p_mode=-1, p_scr=-1, p_hp1=-1, p_hp2=-1, p_a1=0, p_a2=0, p_surv=0, p_stage=0;
static int st_ko, st_low1, st_low2, st_lead, st_rev;
static int st_won, st_lost, st_resultDone;
static int st_myR, st_opR, st_roundN, st_fb, st_longSaid, st_dblLow;
static int st_lastStage = -1;
static unsigned st_roundStart, st_offAt, st_actAt, st_menuAt, st_selChatAt, st_hitAt;
static int st_selChatN;
static int st_myChar = -1, st_oppChar = -1;
static unsigned last_line_f = 0;          /* 마지막 발화 프레임 */
static unsigned last_input_f = 0;
static unsigned hush_until = 0;           /* 결과 멘트 뒤 잡담을 잠그는 시점 */         /* 마지막 패드 입력 프레임 (자동 전환 판별용) */
/* 세션 기록 — 코어가 살아 있는 동안만 (저장 안 함) */
static int sess_wins, sess_games, sess_streak, sess_survBest, sess_lastLossChar = -1;
/* 기술 결합창 — 필살기가 나가면 450ms(27프레임) 기다렸다가 결과와 묶는다 */
static const char *pend_name; static int pend_sup, pend_left;

/* ── v0.7 관전 기억 ────────────────────────────────────────────────
   방금 일어난 일이 아니라 **쌓인 흐름**에 반응한다. 브라우저판 flowMem 과 같은 규칙:
   같은 기술 3번 · 주고받기 5회 · 6대 무피해 · 6대 맞고 무반격 · 상대 필살기 4번째.
   매치가 끝나면 라운드 결과 배열('w'/'l'/'d')을 읽어 판의 **모양**을 한 마디로 낸다. */
#define FLMV 6
static int fl_hit, fl_tak, fl_alt, fl_lastBy, fl_oppSp;
static unsigned char fl_said;             /* 라운드당 한 종류씩만 */
#define FLS_SAME  1
#define FLS_TRADE 2
#define FLS_ONE   4
#define FLS_CHASE 8
#define FLS_SP    16
static struct { const char *name; int n; } fl_mv[FLMV];
static char fl_rounds[10]; static int fl_nr;

static char  outbuf[160];
static char  curline[160];
static unsigned cur_f = 0;
static int cur_ev = -1;
static int cur_spk = 0;            /* 지금 줄을 말한 사람 — 짝꿍이 받으면 여기가 바뀐다 */
static int cm_duo = 1;             /* 짝꿍 켬/끔 */
static unsigned duo_at, duo_big_at;/* 시계 둘 — 잔반응 6초 · 승부 순간 2초 */
static unsigned rng = 2463534242u;

/* 발화 대기열 — 한 프레임에 두세 마디가 겹치면 순서대로 내보낸다(브라우저판 ANN 큐와 같은 역할) */
#define QN 4
static struct { char text[160]; short ev; short spk; } q[QN];
static int q_head, q_cnt;
static unsigned q_next;

static unsigned rnd(void){ rng ^= rng<<13; rng ^= rng>>17; rng ^= rng<<5; return rng; }
static int rd(int off){ return RAM[off]; }
static int rd16(int off){ return RAM[off] | (RAM[off+1]<<8); }

void ss2comm_set_enabled(int on){ cm_on = on ? 1 : 0; }
void ss2comm_set_speaker(int idx){ if(idx>=0 && idx<SS2COMM_SPK_N) cm_spk = idx; }
/* v0.7: 해설자가 15명이 되면서 프런트엔드가 이름·수를 알아야 한다.
   버튼 하나로 교대하는 길도 여기서 연다 — 코어·앱이 같은 순서를 쓴다. */
int ss2comm_speaker_count(void){ return SS2COMM_SPK_N; }
const char *ss2comm_speaker_name(int idx){
  return (idx>=0 && idx<SS2COMM_SPK_N) ? SPK_NAME[idx] : "";
}
int ss2comm_get_speaker(void){ return cm_spk; }
void ss2comm_set_duo(int on){ cm_duo = on ? 1 : 0; }
/* 다음 해설자로 넘기고 그 사람 번호를 돌려준다. 인사 한마디는 프런트엔드가 띄운다. */
int ss2comm_next_speaker(int step){
  int n = SS2COMM_SPK_N;
  cm_spk = ((cm_spk + (step?step:1)) % n + n) % n;
  cur_spk = cm_spk; duo_at = duo_big_at = 0;
  return cm_spk;
}
const char *ss2comm_speaker_hello(int idx){
  return (idx>=0 && idx<SS2COMM_SPK_N) ? HELLO[idx] : "";
}
void ss2comm_reset(void){
  int i; for(i=0;i<CK_N;i++) cd[i]=0;
  p_mode=p_scr=-1; p_hp1=p_hp2=-1; p_a1=p_a2=0; p_surv=p_stage=0;
  st_ko=st_low1=st_low2=st_lead=st_rev=0;
  st_won=st_lost=st_resultDone=0;
  st_myR=st_opR=0; st_roundN=1; st_fb=st_longSaid=st_dblLow=0;
  st_lastStage=-1; st_roundStart=st_offAt=st_actAt=st_menuAt=st_selChatAt=st_hitAt=0;
  st_selChatN=0; st_myChar=st_oppChar=-1;
  curline[0]=0; cur_f=0; cur_ev=-1; cur_spk=cm_spk; duo_at=duo_big_at=0;
  last_line_f=0; last_input_f=0; hush_until=0;
  pend_name=0; pend_sup=0; pend_left=0;
  { int k; fl_hit=fl_tak=fl_alt=fl_lastBy=fl_oppSp=0; fl_said=0; fl_nr=0; fl_rounds[0]=0;
    for(k=0;k<FLMV;k++){ fl_mv[k].name=0; fl_mv[k].n=0; } }
  q_head=q_cnt=0; q_next=0;
}

/* 대사 한 줄 만들어 대기열에 넣는다.
   vsel < 0 이면 변형 중 무작위, 0 이상이면 그 변형을 고른다.
   돌려주는 값: 1 = 받아들임(쿨다운 통과), 0 = 무시됨 → 호출부의 || 사슬이 다음 후보로 넘어간다 */
/* ── v0.7 짝꿍 — 큰 장면에서 다른 한 명이 짧게 받는다 ────────────
   한 명이 혼자 떠들면 심심하다. 받는 말은 길면 안 된다 — 한 호흡이어야 주고받는 맛이 난다.
   잔반응과 승부 순간은 **시계를 따로** 쓴다. 하나로 두면 시작 멘트에 한 번 받아친 것 때문에
   정작 KO 를 못 받는다(브라우저판에서 실제로 그랬다). */
static int duo_cat(int ev){
  switch(ev){
    case EV_KO: case EV_MOVEKO: case EV_KOED: case EV_DKO:
    case EV_PERFECT: case EV_QUICK:                       return DC_KO;
    case EV_MOVE:                                         return DC_SUP;
    case EV_LOW1: case EV_LOW2: case EV_DOUBLELOW:
    case EV_MATCHPOINT:                                   return DC_CRISIS;
    case EV_REVERSAL: case EV_COMEBACK:                   return DC_TURN;
    case EV_ARCSWEEP: case EV_ARCSWEPT: case EV_ARCCOMEBACK:
    case EV_ARCSWEAT: case EV_ARCCHOKE: case EV_ARCSLIP:  return DC_ARC;
    case EV_FLOWSAME: case EV_FLOWTRADE: case EV_FLOWONE:
    case EV_FLOWCHASE: case EV_FLOWSP:                    return DC_FLOW;
    case EV_START:                                        return DC_START;
    default:                                              return -1;
  }
}
static void duo_after(int ev){
  const char *cand[DUOMAXV];
  int cat, p, n = 0, i, big, slot;
  if(!cm_duo) return;
  cat = duo_cat(ev);            if(cat < 0) return;
  p   = DUO_PAIR_C[cm_spk];     if(p < 0)   return;   /* 받아칠 표가 없는 짝 — 조용히 */
  big = (cat == DC_KO || cat == DC_ARC);
  if(cm_f < (big ? duo_big_at : duo_at)) return;
  for(i = 0; i < DUOMAXV; i++) if(DUOLINE[p][cat][i]) cand[n++] = DUOLINE[p][cat][i];
  if(!n) return;
  if(big) duo_big_at = cm_f + 120; else duo_at = cm_f + 360;
  if(q_cnt < QN) slot = (q_head + q_cnt++) % QN;
  else { slot = q_head; q_head = (q_head+1)%QN; }
  snprintf(q[slot].text, sizeof(q[slot].text), "%s", cand[rnd()%(unsigned)n]);
  q[slot].ev  = -1;             /* 받는 말은 표정·강조 없이 담담하게 */
  q[slot].spk = (short)p;
}

static int emit_ex(int ev, int vsel, int n1, int n2, const char *who){
  const char *cand[EVMAXV]; int n=0, i, key;
  const char *fmt;
  if(!cm_on || ev<0 || ev>=EV_N) return 0;
  key = EVCD[ev].key;
  if(cd[key] > cm_f) return 0;
  for(i=0;i<EVMAXV;i++) if(LINES[cm_spk][ev][i]) cand[n++]=LINES[cm_spk][ev][i];
  if(!n) return 0;
  if(vsel >= 0) fmt = cand[vsel < n ? vsel : n-1];
  else          fmt = cand[rnd()%(unsigned)n];
  if(strstr(fmt,"%s"))                 snprintf(outbuf,sizeof(outbuf),fmt,who?who:"");
  else if(strstr(fmt,"%d")){
    const char *p = strstr(fmt,"%d");
    if(strstr(p+2,"%d"))               snprintf(outbuf,sizeof(outbuf),fmt,n1,n2);
    else                               snprintf(outbuf,sizeof(outbuf),fmt,n1);
  }
  else                                 snprintf(outbuf,sizeof(outbuf),"%s",fmt);
  cd[key] = cm_f + EVCD[ev].cool;
  { int slot;
    if(q_cnt < QN) slot = (q_head + q_cnt++) % QN;
    else { slot = q_head; q_head = (q_head+1)%QN; }   /* 꽉 차면 가장 오래된 것을 민다 */
    snprintf(q[slot].text,sizeof(q[slot].text),"%s",outbuf);
    q[slot].ev = (short)ev;
    q[slot].spk = (short)cm_spk;
  }
  duo_after(ev);
  return 1;
}
static int emit(int ev){ return emit_ex(ev,-1,0,0,0); }
static int emitn(int ev,int n){ return emit_ex(ev,-1,n,0,0); }
static int emits(int ev,const char *s){ return emit_ex(ev,-1,0,0,s); }

/* ── 관전 기억 ── */
static void flow_reset(int newMatch){
  int i;
  fl_hit=fl_tak=fl_alt=fl_lastBy=fl_oppSp=0; fl_said=0;
  for(i=0;i<FLMV;i++){ fl_mv[i].name=0; fl_mv[i].n=0; }
  if(newMatch){ fl_nr=0; fl_rounds[0]=0; }
}
/* 쌓인 모양이 임계에 닿으면 한 마디. 한 번에 한 종류만 낸다. */
static void flow_check(void){
  if(fl_alt>=5 && !(fl_said&FLS_TRADE)){ fl_said|=FLS_TRADE; emitn(EV_FLOWTRADE, fl_alt); return; }
  if(fl_hit>=6 && fl_tak==0 && !(fl_said&FLS_ONE)){ fl_said|=FLS_ONE; emitn(EV_FLOWONE, fl_hit); return; }
  if(fl_tak>=6 && fl_hit==0 && !(fl_said&FLS_CHASE)){ fl_said|=FLS_CHASE; emitn(EV_FLOWCHASE, fl_tak); }
}
static int flow_mv_bump(const char *name){
  int i, free_i = -1;
  for(i=0;i<FLMV;i++){
    if(fl_mv[i].name && !strcmp(fl_mv[i].name, name)) return ++fl_mv[i].n;
    if(!fl_mv[i].name && free_i<0) free_i = i;
  }
  if(free_i<0) free_i = 0;                 /* 자리가 없으면 첫 칸을 재사용 */
  fl_mv[free_i].name = name; fl_mv[free_i].n = 1;
  return 1;
}
static void flow_hit(const char *name){
  fl_hit++;
  if(fl_lastBy==2) fl_alt++;
  fl_lastBy = 1;
  if(name && *name && strcmp(name,"필살기")){
    if(flow_mv_bump(name)==3 && !(fl_said&FLS_SAME)){
      fl_said |= FLS_SAME; emits(EV_FLOWSAME, name); return;
    }
  }
  flow_check();
}
static void flow_take(void){
  fl_tak++;
  if(fl_lastBy==1) fl_alt++;
  fl_lastBy = 2;
  flow_check();
}
static void flow_oppsp(void){
  if(++fl_oppSp==4 && !(fl_said&FLS_SP)){ fl_said |= FLS_SP; emitn(EV_FLOWSP, 4); }
}
static void flow_round(char r){ if(fl_nr < (int)sizeof(fl_rounds)-1) fl_rounds[fl_nr++] = r; }
/* 매치의 모양 — 없으면 -1 (그러면 원래 승패 한마디로 떨어진다) */
static int flow_arc(void){
  fl_rounds[fl_nr] = 0;
  if(!strcmp(fl_rounds,"ww"))  return EV_ARCSWEEP;
  if(!strcmp(fl_rounds,"ll"))  return EV_ARCSWEPT;
  if(!strcmp(fl_rounds,"lww")) return EV_ARCCOMEBACK;
  if(!strcmp(fl_rounds,"wlw")) return EV_ARCSWEAT;
  if(!strcmp(fl_rounds,"wll")) return EV_ARCCHOKE;
  if(!strcmp(fl_rounds,"lwl")) return EV_ARCSLIP;
  return -1;
}

/* 엔진 밖에서 한 줄 띄우기 — 지금은 "카드가 없다" 안내에 쓴다.
   해설과 같은 자리에 같은 글꼴로 뜬다(해설을 꺼 두면 조용히 넘어간다). */
void ss2comm_notify(const char *text){
  int slot;
  if(!cm_on || !text || !*text) return;
  if(q_cnt < QN) slot = (q_head + q_cnt++) % QN;
  else { slot = q_head; q_head = (q_head+1)%QN; }
  snprintf(q[slot].text,sizeof(q[slot].text),"%s",text);
  q[slot].ev = -1;                       /* 표정·강조 없음 */
  q[slot].spk = (short)cm_spk;
}

const char *ss2comm_current(int *age){
  if(age) *age = (int)(cm_f - cur_f);
  return curline[0] ? curline : 0;
}

/* 블록값 → 캐릭터 번호. 0x98 은 쿠로코 수라와 샤를로트 나찰이 겹치는데,
   브라우저판과 같이 샤를로트로 해석한다(실기 제보 반영). */
static int blk_char(int blk){ int c = blk>>4; return (c>=0 && c<15) ? c : -1; }

/* 필살기 이름 — SP 엔진이 방금 낸 기술만 안다(손 커맨드는 이름 없음).
   기술표는 코어에 내장이라 롬 버전과 무관하다. */
extern const char *ss2sp_last_name;
extern int ss2sp_last_ok;

static const char *pend_take(int *sup){
  const char *n = pend_name;
  if(sup) *sup = pend_sup;
  pend_name = 0; pend_left = 0; pend_sup = 0;
  return n;
}

const char *ss2comm_frame(void){
  int mode, scr, hp1, hp2, a1, a2, surv, stage, pad;
  int hit1, hit2, down2, downed1;
  if(!cm_on) return 0;
  cm_f++;

  mode  = rd(OFF_MODE);  scr  = rd(OFF_SCR);
  hp1   = rd(OFF_HP1);   hp2  = rd(OFF_HP2);
  a1    = rd16(OFF_ACT1);a2   = rd16(OFF_ACT2);
  surv  = rd(OFF_SURV);  stage= rd(OFF_STAGE);
  pad   = rd(OFF_PAD);
  if(pad) last_input_f = cm_f;
  if(pend_left > 0 && --pend_left == 0){        /* 결합창이 그냥 닫혔다 — 비오의면 한 마디 */
    int sup; const char *nm = pend_take(&sup);
    (void)nm; if(sup) emit(EV_MOVE);
  }

  if(p_mode < 0){                                /* 첫 프레임: 기준만 잡는다 */
    p_mode=mode; p_scr=scr; p_hp1=hp1; p_hp2=hp2; p_a1=a1; p_a2=a2;
    p_surv=surv; p_stage=stage; goto out;
  }
  if(mode!=MD_BATTLE && p_mode==MD_BATTLE)
  {
    st_offAt = cm_f;
    /* v0.5.4b: 전투를 벗어나는 순간 메뉴 잡담 타이머를 다시 센다.
       결과 화면은 아직 MD_BATTLE 이라 화면 전환 리셋이 안 걸리고, 그 뒤 mode만 바뀌면서
       전투 전에 세워 둔 낡은 타이머가 이미 만료돼 있어 준비 화면마다 잡담이 터졌다. */
    st_menuAt = cm_f; st_selChatAt = cm_f; st_selChatN = 0;
  }

  if(mode==MD_QUOTE  && p_mode!=MD_QUOTE)  emit(EV_QUOTE);
  if(mode==MD_ENDING && p_mode!=MD_ENDING) emit(EV_ENDING);

  /* ── 전투측 화면 전환: 문구(VS) 화면 · 승패 결과 이름 화면 · 스토리 사담 ── */
  if(mode==MD_BATTLE && scr!=p_scr){
    if(p_scr>=8 && (scr==0 || scr==2)){
      /* 매치가 실제로 끝났을 때만 결과 멘트를 낸다(2선승). 라운드 하나 이긴 것으로는 말하지 않는다.
         승패 화면에서는 **두 줄까지** — 결과 한 마디 + 한마디 더. 그 뒤 잡담은 잠시 잠근다.
         (제보: "대전 사이 승부 난 뒤 대사가 어색하다" — 여러 줄이 몰리고 잡담이 끼어들었다) */
      int done = (st_myR>=2 || st_opR>=2);
      if(!st_resultDone && done && (st_won || st_lost)){
        st_resultDone = 1;
        emit(st_won ? EV_WINSCR  : EV_LOSESCR);
        /* v0.7: 판의 **모양**이 잡히면 승패 한마디 대신 총평을 낸다.
           "이겼다/졌다"를 두 번 말하지 않기 위해서다. */
        { int arc = flow_arc();
          if(arc < 0 || !emit(arc)) emit(st_won ? EV_WINTALK : EV_LOSETALK); }
        /* 오늘 전적은 세 판에 한 번만 — 매번 붙으면 결과가 늘어진다 */
        if(sess_games>=3 && sess_games%3==0)
          emit_ex(EV_RECORD, sess_wins*2>=sess_games?0:1, sess_games, sess_wins, 0);
        hush_until = cm_f + 600;                 /* 10초간 혼잣말·스토리 사담 금지 */
      }
    }
    else if(scr==0 && p_mode!=MD_BATTLE) emit(EV_VSQ);
    else if((scr==0||scr==2) && p_mode==MD_BATTLE && cm_f > hush_until) emit(EV_STORYCHAT);
  }

  /* ── 전투 진입 ── */
  if(mode==MD_BATTLE && p_mode!=MD_BATTLE && hp1>0 && hp2>0){
    st_ko=0; st_low1=st_low2=0; st_lead=0; st_rev=0;
    st_won=st_lost=st_resultDone=0;
    st_actAt=cm_f; st_roundStart=cm_f;
    cd[CK_KO]=0; cd[CK_REV]=0;
    if(!st_offAt || cm_f-st_offAt > 180){        /* 새 매치 (3초 넘게 전투를 벗어나 있었다) */
      const char *rel;
      st_myR=st_opR=0; st_roundN=1;
      st_fb=st_longSaid=st_dblLow=0;
      flow_reset(1);                              /* v0.7 관전 기억 — 매치 통째로 */
      st_myChar  = blk_char(rd(OFF_BLK1));
      st_oppChar = blk_char(rd(OFF_BLK2));
      if(st_myChar>=0 && st_oppChar>=0){
        char who[64];
        snprintf(who,sizeof(who),"%s 대 %s",CHARNAME[st_myChar],CHARNAME[st_oppChar]);
        emits(EV_START, who);
      }else emits(EV_START, "한판");
      /* 2절 — 화자와 상대의 관계 대사 우선, 없으면 캐릭터 설정 한 줄 */
      rel = (st_oppChar>=0 ? RELLINE[cm_spk][st_oppChar] : 0);
      if(!rel && st_myChar>=0) rel = RELLINE[cm_spk][st_myChar];
      if(rel) emits(EV_REL, rel);
      else if(st_oppChar>=0 && LORE[st_oppChar]){
        char t[160];
        snprintf(t,sizeof(t),"%s — %s",CHARNAME[st_oppChar],LORE[st_oppChar]);
        emits(EV_LORE, t);
      }
    }else{                                        /* 라운드 재개 */
      st_roundN++; st_fb=st_longSaid=st_dblLow=0;
      flow_reset(0);                              /* v0.7 라운드 단위 관찰만 */
      if(st_myR==1 && st_opR==1)      emit(EV_MATCHPOINT);
      else if(st_myR==1 && st_opR==0) emit(EV_ROUNDLEAD);
      else if(st_myR==0 && st_opR==1) emit(EV_ROUNDBEHIND);
      else                            emit(EV_ROUND);
    }
    if(stage < st_lastStage) st_lastStage = -1;   /* 새 주행 */
    if(surv >= 1){
      int rec = (surv > sess_survBest && surv >= 3);
      if(surv > sess_survBest) sess_survBest = surv;
      emit_ex(EV_SURV, rec?0:(surv>=10?1:(surv>=7?2:(surv>=3?3:-1))), surv, 0, 0);
    }else if(stage>=1 && stage<=14 && stage > st_lastStage){
      st_lastStage = stage;
      emit_ex(EV_STAGE, (stage+1)>=8?0:((stage+1)>=5?1:-1), stage+1, 0, 0);
    }else if(stage > st_lastStage) st_lastStage = stage;
    goto store;
  }

  /* ── 전투가 아니거나 체력이 회복된 틱(=메뉴/연출) ── */
  if(mode!=MD_BATTLE || hp1>p_hp1 || hp2>p_hp2){
    if(scr!=p_scr && mode!=MD_BATTLE){
      int byClick = (cm_f - last_input_f) < 54;   /* 0.9초 안에 입력이 있었나 */
      st_menuAt = cm_f; st_selChatAt = cm_f; st_selChatN = 0;
      if(!byClick)               emit(EV_STORYCHAT);
      else if(scr==2)            emit(EV_CHARSEL);
      else if(scr==4)            emit(EV_STYLESEL);
      else if(scr==0)            emit(EV_TITLE);
      else if(scr==6)            emit(EV_CARDSEL);
    }
    if(mode!=MD_BATTLE){
      if(scr==2){                                 /* 캐릭터 고르는 동안 사담 (한 방문에 3마디) */
        if(!st_selChatAt) st_selChatAt=cm_f;
        if(cm_f > hush_until && cm_f-st_selChatAt > 420 && st_selChatN < 3){
          st_selChatAt=cm_f; st_selChatN++; emit(EV_CHARSELCHAT);
        }
      }
      if(!st_menuAt) st_menuAt=cm_f;
      if(scr!=2 && cm_f > hush_until && cm_f-st_menuAt > 900){ st_menuAt=cm_f; emit(EV_MENUIDLE); }
    }
    if(hp1>32) st_low1=0;
    if(hp2>32) st_low2=0;
    if(hp1>0 && hp2>0) st_ko=0;
    goto store;
  }

  /* ── 전투 중 ── */
  /* v0.5.6: 승부가 난 뒤의 승리 포즈도 액션ID가 0x180을 넘는다.
     그걸 필살기로 읽어 "온다! 비오의!" 를 뜬금없이 외치던 버그를 여기서 막는다.
     둘 다 살아 있고 KO 상태가 아닐 때만 기술 발동으로 본다. */
  if(hp1>=100 && hp2>=100) st_ko=0;   /* 라운드 재개 = KO 표식 해제 */
  { int live = (hp1>0 && hp2>0 && !st_ko);
  if(live && a1>=0x180 && p_a1<0x180){            /* 내 필살기 발동 → 결합창 열기 */
    pend_name = (ss2sp_last_name && ss2sp_last_ok!=0) ? ss2sp_last_name : 0;
    pend_sup  = (pend_name && !strcmp(pend_name,"비오의"));
    pend_left = 27;
    /* 비오의는 **나가는 순간 바로** 호들갑을 떤다. 결합창을 기다리면 적중 멘트에 먹힌다. */
    if(pend_sup) emit(EV_MOVE);
  }
  if(live && a2>=0x180 && p_a2<0x180){ flow_oppsp(); emit(EV_OPPSP); }
  }

  hit2 = hp2 < p_hp2; hit1 = hp1 < p_hp1;
  /* v0.7 관전 기억 — 유효타만. pend_name 이 살아 있으면 그 기술로 친 것이다
     (아래 pend_take 가 가져가기 전에 세야 한다). */
  if(hit2 && (p_hp2-hp2)>=4) flow_hit(pend_name);
  if(hit1 && (p_hp1-hp1)>=4) flow_take();
  down2   = (a2>=0x13C && a2<=0x154) && !(p_a2>=0x13C && p_a2<=0x154);
  downed1 = (a1>=0x13C && a1<=0x154) && !(p_a1>=0x13C && p_a1<=0x154);

  if(!st_fb && (hit1||hit2)){
    st_fb = 1;
    if(hit2 && !hit1 && st_roundN==1 && hp2>0 && !pend_name && (p_hp2-hp2)>=4) emit(EV_FIRSTBLOOD);
  }

  if(hit1 && hit2 && hp1<=0 && hp2<=0 && !st_ko){ st_ko=1; pend_take(0); flow_round('d'); emit(EV_DKO); }
  else{
    if(hit2 && hp2<=0 && !st_ko){
      int sup; const char *nm = pend_take(&sup);
      st_ko=1; st_won=1; st_lost=0;
      if(nm) emit_ex(EV_MOVEKO,-1,0,0,nm); else emit(EV_KO);
      st_myR++; flow_round('w');
      { int mw = (st_myR>=2); int said;
        if(mw){ sess_streak++; sess_wins++; sess_games++; }
        said =
        (st_low1 && emit(EV_COMEBACK)) ||
        (hp1>=128 && emit(EV_PERFECT)) ||
        ((cm_f-st_roundStart) < 600 && emit(EV_QUICK)) ||
        (mw && st_oppChar>=0 && st_oppChar==sess_lastLossChar && emit(EV_REVENGE)) ||
        (mw && (sess_streak==2||sess_streak==3||sess_streak==5||sess_streak==7||
                (sess_streak>=10 && sess_streak%5==0)) &&
              emit_ex(EV_STREAK, sess_streak>=5?0:-1, sess_streak,0,0));
        (void)said;
        if(mw && st_oppChar>=0 && st_oppChar==sess_lastLossChar) sess_lastLossChar=-1;
      }
    }
    else if(down2){
      int sup; const char *nm = pend_take(&sup);
      if(nm) emit_ex(EV_MOVEDOWN,-1,0,0,nm);
      else if(pend_left>0) emit_ex(EV_MOVEDOWNA,-1,0,0,0);
      else if(cm_f - st_hitAt > 42) emit(EV_DOWN);
    }
    else if(hit2){
      int d = p_hp2 - hp2;
      if(d>=4){
        int sup; const char *nm = pend_take(&sup);
        if(nm){ emit_ex(d>=12?EV_MOVEHIT:EV_MOVEHITL, -1, 0,0, nm); st_hitAt=cm_f; }
        else if(d>=12){ emit(EV_HIT); st_hitAt=cm_f; }
      }
    }
    if(hit1){
      int d = p_hp1 - hp1;
      if(hp1<=0 && !st_ko){
        st_ko=1; st_lost=1; st_won=0;
        emit(EV_KOED);
        if(surv>0) emitn(EV_SURVEND, surv);
        st_opR++; flow_round('l');
        if(st_opR>=2){ sess_streak=0; sess_lastLossChar=st_oppChar; sess_games++; }
      }
      else if(d>=12) emit(EV_TAKEN);
    }
  }
  if(downed1) emit(EV_DOWNED);

  if(!st_ko){
    int lead = (hp1>hp2) - (hp1<hp2);
    int diff = hp1-hp2; if(diff<0) diff=-diff;
    if(lead!=0 && st_lead!=0 && lead!=st_lead && diff>=8 && st_rev<2){
      if(emit(EV_REVERSAL)) st_rev++;
    }
    if(lead!=0) st_lead=lead;
    if(hp1>0 && hp1<=32 && !st_low1){ st_low1=1; emit(EV_LOW1); }
    if(hp2>0 && hp2<=32 && !st_low2){ st_low2=1; emit(EV_LOW2); }
    if(st_low1 && st_low2 && !st_dblLow){ st_dblLow=1; emit(EV_DOUBLELOW); }
  }

  /* 공방이 끊기면 — 전투 잡담 12초, 장기전 60초 */
  if(hit1||hit2||down2||downed1||
     (hp1>0&&hp2>0&&!st_ko&&((a1>=0x180&&p_a1<0x180)||(a2>=0x180&&p_a2<0x180)))) st_actAt=cm_f;
  else if(!st_ko && hp1>0 && hp2>0 && scr>=8){
    if(!st_actAt) st_actAt=cm_f;
    if(cm_f-st_actAt > 720){ st_actAt=cm_f; emit(EV_IDLE); }
    if(!st_longSaid && st_roundStart && cm_f-st_roundStart > 3600){
      st_longSaid=1; emit(EV_LONGFIGHT);
    }
  }

store:
  p_mode=mode; p_scr=scr; p_hp1=hp1; p_hp2=hp2; p_a1=a1; p_a2=a2;
  p_surv=surv; p_stage=stage;

out:
  /* 아무도 말하지 않고 조용하면 화자가 혼잣말 — 전투 6초 / 그 밖 3초 */
  if(!q_cnt && cm_f >= q_next){
    unsigned quiet = cm_f - (last_line_f > cur_f ? last_line_f : cur_f);
    unsigned need  = (mode==MD_BATTLE) ? 360 : 180;
    if(cm_f > 300 && quiet > need && cm_f > hush_until && !st_ko)   /* KO 연출 중엔 잡담 금지 */
      emit(mode==MD_BATTLE ? EV_MUSE_B : (mode==MD_QUOTE ? EV_MUSE_Q : EV_MUSE_M));
  }
  /* 대기열에서 한 줄 꺼내 보여준다 — 겹칠 때도 순서대로 읽히게 1.6초 간격 */
  if(q_cnt && cm_f >= q_next){
    int slot = q_head;
    q_head = (q_head+1)%QN; q_cnt--;
    snprintf(curline,sizeof(curline),"%s",q[slot].text);
    cur_ev = q[slot].ev; cur_spk = q[slot].spk; cur_f = cm_f; last_line_f = cm_f;
    /* 결과 계열(승패 화면·한마디 더·전적)은 한 박자 더 벌려야 자연스럽다 */
    q_next = cm_f + ((cur_ev==EV_WINSCR||cur_ev==EV_LOSESCR||cur_ev==EV_WINTALK||
                      cur_ev==EV_LOSETALK||cur_ev==EV_RECORD) ? 150 : 100);
    return curline;
  }
  return 0;
}

/* ══ 자체 렌더 (D) ══════════════════════════════════════════════════
   RetroArch OSD 폰트는 환경마다 한글이 깨질 수 있어, 코어가 직접 그린다.
   화면 아래에 띠를 덧붙이고 초상 + 8x8 갈무리 글리프 + 연출을 찍는다. 폰트 의존 0. */
#include "ss2comm_font.h"

static int cm_draw = 1;
void ss2comm_draw_enable(int mode){ cm_draw = mode; }
/* 0 끔 / 1 화면 밖 아래 띠 / 2 화면 안 위 / 3 화면 안 아래 / 4 화면 밖 위 띠 */

static const ss2glyph *glyph_of(unsigned cp){
  int lo=0, hi=SS2FONT_N-1;
  while(lo<=hi){ int mid=(lo+hi)>>1;
    if(SS2FONT[mid].cp==cp) return &SS2FONT[mid];
    if(SS2FONT[mid].cp<cp) lo=mid+1; else hi=mid-1; }
  return 0;
}
/* UTF-8 한 글자 → 코드포인트 (BMP까지) */
static const char *utf8_next(const char *s, unsigned *cp){
  unsigned char c=(unsigned char)*s;
  if(c<0x80){ *cp=c; return s+1; }
  if((c&0xE0)==0xC0){ *cp=((c&0x1F)<<6)|((unsigned char)s[1]&0x3F); return s+2; }
  if((c&0xF0)==0xE0){ *cp=((c&0x0F)<<12)|(((unsigned char)s[1]&0x3F)<<6)|((unsigned char)s[2]&0x3F); return s+3; }
  *cp='?'; return s+1;
}

/* ── 표정·강조 표 (브라우저판 MOOD 맵과 같은 결) ──
   0 담담 / 1 신남 / 2 걱정.  강조(impact)는 굵게 + 금색 + 띠 번쩍. */

static const unsigned char EVMOOD[EV_N] = {
  [EV_START      ] = 0,
  [EV_ROUND      ] = 0,
  [EV_KO         ] = 1,
  [EV_KOED       ] = 2,
  [EV_DKO        ] = 0,
  [EV_PERFECT    ] = 1,
  [EV_COMEBACK   ] = 1,
  [EV_QUICK      ] = 1,
  [EV_LOW1       ] = 2,
  [EV_LOW2       ] = 1,
  [EV_REVERSAL   ] = 1,
  [EV_WINTALK    ] = 1,
  [EV_LOSETALK   ] = 2,
  [EV_SURV       ] = 1,
  [EV_SURVEND    ] = 2,
  [EV_STAGE      ] = 1,
  [EV_QUOTE      ] = 0,
  [EV_ENDING     ] = 1,
  [EV_CHARSEL    ] = 0,
  [EV_STYLESEL   ] = 0,
  [EV_CARDSEL    ] = 0,
  [EV_TITLE      ] = 0,
  [EV_MUSE_B     ] = 0,
  [EV_MUSE_Q     ] = 0,
  [EV_MUSE_M     ] = 0,
  [EV_IDLE       ] = 0,
  [EV_VSQ        ] = 0,
  [EV_STORYCHAT  ] = 0,
  [EV_CHARSELCHAT] = 0,
  [EV_MENUIDLE   ] = 0,
  [EV_FIRSTBLOOD ] = 1,
  [EV_ROUNDLEAD  ] = 1,
  [EV_ROUNDBEHIND] = 2,
  [EV_MATCHPOINT ] = 1,
  [EV_DOUBLELOW  ] = 1,
  [EV_LONGFIGHT  ] = 0,
  [EV_HIT        ] = 1,
  [EV_TAKEN      ] = 2,
  [EV_DOWN       ] = 1,
  [EV_DOWNED     ] = 2,
  [EV_OPPSP      ] = 2,
  [EV_MOVE       ] = 1,
  [EV_MOVEHIT    ] = 1,
  [EV_MOVEHITL   ] = 1,
  [EV_MOVEDOWN   ] = 1,
  [EV_MOVEDOWNA  ] = 1,
  [EV_MOVEKO     ] = 1,
  [EV_REVENGE    ] = 1,
  [EV_STREAK     ] = 1,
  [EV_RECORD     ] = 0,
  [EV_WINSCR     ] = 1,
  [EV_LOSESCR    ] = 2,
  [EV_REL        ] = 0,
  [EV_LORE       ] = 0,
  [EV_FLOWSAME   ] = 0,
  [EV_FLOWTRADE  ] = 1,
  [EV_FLOWONE    ] = 1,
  [EV_FLOWCHASE  ] = 2,
  [EV_FLOWSP     ] = 0,
  [EV_ARCSWEEP   ] = 1,
  [EV_ARCSWEPT   ] = 2,
  [EV_ARCCOMEBACK] = 1,
  [EV_ARCSWEAT   ] = 1,
  [EV_ARCCHOKE   ] = 2,
  [EV_ARCSLIP    ] = 2,
};

static const unsigned char EVHIT[EV_N] = {
  [EV_START      ] = 0,
  [EV_ROUND      ] = 0,
  [EV_KO         ] = 1,
  [EV_KOED       ] = 0,
  [EV_DKO        ] = 1,
  [EV_PERFECT    ] = 1,
  [EV_COMEBACK   ] = 1,
  [EV_QUICK      ] = 1,
  [EV_LOW1       ] = 0,
  [EV_LOW2       ] = 0,
  [EV_REVERSAL   ] = 1,
  [EV_WINTALK    ] = 0,
  [EV_LOSETALK   ] = 0,
  [EV_SURV       ] = 1,
  [EV_SURVEND    ] = 0,
  [EV_STAGE      ] = 1,
  [EV_QUOTE      ] = 0,
  [EV_ENDING     ] = 1,
  [EV_CHARSEL    ] = 0,
  [EV_STYLESEL   ] = 0,
  [EV_CARDSEL    ] = 0,
  [EV_TITLE      ] = 0,
  [EV_MUSE_B     ] = 0,
  [EV_MUSE_Q     ] = 0,
  [EV_MUSE_M     ] = 0,
  [EV_IDLE       ] = 0,
  [EV_VSQ        ] = 0,
  [EV_STORYCHAT  ] = 0,
  [EV_CHARSELCHAT] = 0,
  [EV_MENUIDLE   ] = 0,
  [EV_FIRSTBLOOD ] = 0,
  [EV_ROUNDLEAD  ] = 0,
  [EV_ROUNDBEHIND] = 0,
  [EV_MATCHPOINT ] = 1,
  [EV_DOUBLELOW  ] = 1,
  [EV_LONGFIGHT  ] = 0,
  [EV_HIT        ] = 0,
  [EV_TAKEN      ] = 0,
  [EV_DOWN       ] = 0,
  [EV_DOWNED     ] = 0,
  [EV_OPPSP      ] = 0,
  [EV_MOVE       ] = 1,
  [EV_MOVEHIT    ] = 0,
  [EV_MOVEHITL   ] = 0,
  [EV_MOVEDOWN   ] = 0,
  [EV_MOVEDOWNA  ] = 0,
  [EV_MOVEKO     ] = 1,
  [EV_REVENGE    ] = 1,
  [EV_STREAK     ] = 1,
  [EV_RECORD     ] = 0,
  [EV_WINSCR     ] = 0,
  [EV_LOSESCR    ] = 0,
  [EV_REL        ] = 0,
  [EV_LORE       ] = 0,
  [EV_FLOWSAME   ] = 0,
  [EV_FLOWTRADE  ] = 0,
  [EV_FLOWONE    ] = 0,
  [EV_FLOWCHASE  ] = 0,
  [EV_FLOWSP     ] = 0,
  [EV_ARCSWEEP   ] = 1,
  [EV_ARCSWEPT   ] = 0,
  [EV_ARCCOMEBACK] = 1,
  [EV_ARCSWEAT   ] = 1,
  [EV_ARCCHOKE   ] = 0,
  [EV_ARCSLIP    ] = 0,
};

/* ── 초상 (얼굴) ──────────────────────────────────────────────────
   전투 HUD 초상 타일(16×16 = 4타일 × 16B, 2bpp)의 **롬 파일 오프셋**이다.
   브라우저판에서 리버싱해 둔 것과 같은 표 — 배포물에 들어가는 건 숫자뿐이고
   그림은 사용자 롬에서 그 자리에서 그린다(게임 그림은 어디에도 넣지 않는다).
   sum = 64바이트 단순합. 다른 버전 롬이면 초상은 조용히 생략한다. */
typedef struct { unsigned off; unsigned short pal[4]; unsigned short sum; } ss2face;
/* 표는 ss2comm_lines.h 가 만든다 (브라우저판 FACE_ROM 을 화자 순서로 옮긴 것) */
static const ss2face FACE_ROM[SS2COMM_SPK_N] = SS2COMM_FACE_ROM_INIT;
static const unsigned char *cm_rom = 0;
static unsigned cm_romlen = 0;
static uint16_t face_px[SS2COMM_SPK_N][256];
static unsigned char face_a[SS2COMM_SPK_N][256];   /* 0 = 투명(색인 0) */
static unsigned char face_ok[SS2COMM_SPK_N];
static int face_built = 0;

void ss2comm_set_rom(const void *rom, unsigned len){
  cm_rom = (const unsigned char *)rom; cm_romlen = len; face_built = 0;
}

static uint16_t pal12_to565(unsigned short v){
  int r=(v&0xF)*17, g=((v>>4)&0xF)*17, b=((v>>8)&0xF)*17;   /* RGB444, R이 하위 니블 */
  return (uint16_t)(((r>>3)<<11) | ((g>>2)<<5) | (b>>3));
}
static void build_faces(void){
  int i, k, ty, tx;
  face_built = 1;
  memset(face_ok, 0, sizeof(face_ok));
  if(!cm_rom) return;
  for(i=0;i<SS2COMM_SPK_N;i++){
    unsigned off = FACE_ROM[i].off, sum = 0; int j;
    if(!off) continue;                        /* 오프셋을 아직 못 뜬 화자 — 얼굴 없이 글자만 */
    if(off + 64 > cm_romlen) continue;
    for(j=0;j<64;j++) sum = (sum + cm_rom[off+j]) & 0xFFFF;
    if(sum != FACE_ROM[i].sum) continue;      /* 다른 롬 — 이 얼굴은 생략 */
    for(k=0;k<4;k++){
      unsigned o = off + k*16; int ox=(k&1)*8, oy=(k>>1)*8;
      for(ty=0; ty<8; ty++){
        unsigned w = cm_rom[o+ty*2] | (cm_rom[o+ty*2+1]<<8);
        for(tx=0; tx<8; tx++){
          int ci = (w >> ((7-tx)*2)) & 3;
          int p  = (oy+ty)*16 + (ox+tx);
          face_a[i][p]  = ci ? 1 : 0;
          face_px[i][p] = ci ? pal12_to565(FACE_ROM[i].pal[ci]) : 0;
        }
      }
    }
    face_ok[i] = 1;
  }
}
/* 표정: 신남이면 밝게, 걱정이면 어둡고 푸르게 */
static uint16_t tint(uint16_t c, int mood){
  int r=(c>>11)&31, g=(c>>5)&63, b=c&31;
  if(mood==1){ r+=r>>2; g+=g>>2; b+=b>>3; }
  else if(mood==2){ r-=r>>2; g-=g>>3; b+=(31-b)>>3; }
  if(r>31) r=31;
  if(g>63) g=63;
  if(b>31) b=31;
  if(r<0) r=0;
  if(g<0) g=0;
  if(b<0) b=0;
  return (uint16_t)((r<<11)|(g<<5)|b);
}

/* 표시 모드: 0 끔(프론트엔드 알림) / 1 화면 밖 아래 띠 / 2 화면 안 위 / 3 화면 안 아래 / 4 화면 밖 위 띠
   화면 밖 띠는 게임 화면을 하나도 가리지 않는다. 큰 글씨 두 줄이 들어가게 30px 로 잡았다. */
#define SS2_BAND_H 30
#define CM_TTL     150
#define COL_WHITE  0xFFFF
#define COL_GOLD   0xFEA0
#define BOX_LINE_H 13
#define BOX_MAXL   3
int ss2comm_band_h(void){ return (cm_on && (cm_draw==1 || cm_draw==4)) ? SS2_BAND_H : 0; }
int ss2comm_band_top(void){ return (cm_on && cm_draw==4) ? 1 : 0; }
int ss2comm_drawing(void){ return (cm_on && cm_draw) ? 1 : 0; }

/* ── 작은 글씨(8×8) — 덧띠 전용 ── */
static int line_w(const char *s, const char *end){
  int tw=0; const char *p=s;
  while(p<end && *p){ unsigned cp; p=utf8_next(p,&cp); tw += (cp<0x80)?4:8; }
  return tw;
}
static int draw_line(uint16_t *fb,int pitch_px,int x0,int x1,const char *s,const char *end,
                     int ytop,int clipTop,int clipBot,int show,uint16_t col,int bold){
  const char *p=s; int x = x0 + ((x1-x0) - line_w(s,end))/2, drawn=0;
  if(x<x0) x=x0;
  while(p<end && *p && drawn<show){
    unsigned cp; const ss2glyph *g;
    p = utf8_next(p,&cp);
    g = glyph_of(cp);
    if(g){
      int gy,gx,b;
      for(gy=0; gy<8; gy++){
        uint8_t bits=g->b[gy];
        for(gx=0; gx<8; gx++)
          if(bits & (0x80>>gx))
            for(b=0; b<=bold; b++){
              int px=x+gx+b, py=ytop+gy;
              if(px>=0 && px<x1 && py>=clipTop && py<clipBot) fb[py*pitch_px+px]=col;
            }
      }
    }
    x += (cp<0x80)?4:8; drawn++;
    if(x > x1-4) break;
  }
  return drawn;
}

/* ── 큰 글씨(갈무리11) — 화면 안 말풍선 전용. 8×8은 작아서 읽기 힘들다는 지적 반영 ── */
#include "ss2comm_font11.h"
static const ss2glyph11 *glyph11_of(unsigned cp){
  int lo=0, hi=SS2FONT11_N-1;
  while(lo<=hi){ int mid=(lo+hi)>>1;
    if(SS2FONT11[mid].cp==cp) return &SS2FONT11[mid];
    if(SS2FONT11[mid].cp<cp) lo=mid+1; else hi=mid-1; }
  return 0;
}
static int adv11(unsigned cp){
  const ss2glyph11 *g = glyph11_of(cp);
  if(g) return g->w;
  return (cp<0x80) ? 6 : 11;
}
static int line_w11(const char *s, const char *end){
  int tw=0; const char *p=s;
  while(p<end && *p){ unsigned cp; p=utf8_next(p,&cp); tw += adv11(cp); }
  return tw;
}
/* 한 줄 그리기(가운데 정렬). 그림자 한 겹을 깔아 게임 화면 위에서도 글자가 뜬다. */
static int draw_line11(uint16_t *fb,int pitch_px,int x0,int x1,const char *s,const char *end,
                       int ytop,int clipTop,int clipBot,int show,uint16_t col,int bold){
  const char *p=s; int x = x0 + ((x1-x0) - line_w11(s,end))/2, drawn=0;
  if(x<x0) x=x0;
  while(p<end && *p && drawn<show){
    unsigned cp; const ss2glyph11 *g;
    p = utf8_next(p,&cp);
    g = glyph11_of(cp);
    if(g){
      int gy,gx,b,pass;
      for(pass=0; pass<2; pass++){               /* 0 = 그림자, 1 = 본체 */
        uint16_t c = pass ? col : 0x0000;
        int dx = pass ? 0 : 1, dy = pass ? 0 : 1;
        for(gy=0; gy<SS2FONT11_H; gy++){
          uint16_t bits = g->b[gy];
          for(gx=0; gx<12; gx++)
            if(bits & (0x8000>>gx))
              for(b=0; b<=bold; b++){
                int px=x+gx+b+dx, py=ytop+gy+dy;
                if(px>=0 && px<x1 && py>=clipTop && py<clipBot) fb[py*pitch_px+px]=c;
              }
        }
      }
    }
    x += adv11(cp); drawn++;
    if(x > x1-4) break;
  }
  return drawn;
}
/* 최대 세 줄로 접는다. 공백이 있으면 공백에서, 없으면 넘치기 직전에서. */
static int wrap11(const char *line, const char *end, int maxw, const char *seg[BOX_MAXL+1]){
  int n=0; const char *p=line;
  seg[0]=line;
  while(p<end && n<BOX_MAXL){
    const char *lastsp=0, *q=p; int tw=0;
    while(q<end && *q){
      const char *r; unsigned cp; r=utf8_next(q,&cp);
      tw += adv11(cp);
      if(tw > maxw) break;
      if(cp==' ') lastsp=r;
      q=r;
    }
    if(q>=end || !*q){ n++; seg[n]=end; break; }
    p = (lastsp && lastsp>p) ? lastsp : q;
    n++; seg[n]=p;
  }
  if(n==0){ n=1; seg[1]=end; }
  return n;
}

/* 작은 글씨(8×8) 로 두 줄 접기 — 띠에서 큰 글씨가 두 줄을 넘칠 때만 쓴다 */
static int wrap8(const char *line, const char *end, int maxw, const char *seg[BOX_MAXL+1]){
  int n=0; const char *p=line;
  seg[0]=line;
  while(p<end && n<2){
    const char *lastsp=0, *q=p; int tw=0;
    while(q<end && *q){
      const char *r; unsigned cp; r=utf8_next(q,&cp);
      tw += (cp<0x80)?4:8;
      if(tw > maxw) break;
      if(cp==' ') lastsp=r;
      q=r;
    }
    if(q>=end || !*q){ n++; seg[n]=end; break; }
    p = (lastsp && lastsp>p) ? lastsp : q;
    n++; seg[n]=p;
  }
  if(n==0){ n=1; seg[1]=end; }
  return n;
}

void ss2comm_draw(uint16_t *fb, int pitch_px, int w, int h){
  const char *line, *end, *seg[BOX_MAXL+1];
  int age=0, x, y, top, bot, mood, hit, spk, tx0, x1, maxw, show, i, nl, boxh, ty, lh;
  int band, bandTop, small;
  uint16_t col;
  if(!cm_on || !cm_draw || !fb) return;
  band    = (cm_draw==1 || cm_draw==4);
  bandTop = (cm_draw==4);
  line    = ss2comm_current(&age);

  /* 화면 밖 띠는 대사가 없어도 매 프레임 지운다 (위 띠일 때 게임 화면은 이미 아래로 밀려 있다) */
  if(band){
    int b0 = bandTop ? 0 : h, b1 = bandTop ? SS2_BAND_H : h + SS2_BAND_H;
    for(y=b0; y<b1; y++)
      for(x=0; x<w; x++) fb[y*pitch_px+x] = 0x0000;
  }
  if(!line || age > CM_TTL) return;            /* 2.5초만 표시 */

  end  = line + strlen(line);
  mood = (cur_ev>=0 && cur_ev<EV_N) ? EVMOOD[cur_ev] : 0;
  hit  = (cur_ev>=0 && cur_ev<EV_N) ? EVHIT[cur_ev]  : 0;
  col  = hit ? COL_GOLD : COL_WHITE;
  spk  = (cur_spk >= 0 && cur_spk < SS2COMM_SPK_N) ? cur_spk : cm_spk;
  if(!face_built) build_faces();
  show = 2 + (int)age*2;                       /* 타자 연출: 프레임당 두 글자 */

  tx0  = face_ok[spk] ? 21 : 4;
  x1   = w - 3;
  maxw = x1 - tx0;

  /* 줄 나누기 — 띠는 높이가 고정이라 두 줄까지. 큰 글씨로 안 들어가면 작은 글씨로 접는다. */
  small = 0;
  nl    = wrap11(line, end, maxw, seg);
  if(band && nl > 2){ small = 1; nl = wrap8(line, end, maxw, seg); }
  lh    = small ? 9 : BOX_LINE_H;

  if(band){
    boxh = SS2_BAND_H;
    top  = bandTop ? 0 : h;
  }else{
    boxh = 4 + nl*lh;
    if(face_ok[spk] && boxh < 22) boxh = 22;
    if(boxh > h) boxh = h;
    top  = (cm_draw==3) ? h - boxh : 0;
  }
  bot = top + boxh;

  /* 바탕 — 띠는 검정, 화면 안 상자는 게임 화면을 1/16 로 눌러 깐다. 강조면 첫 네 프레임 붉게. */
  for(y=top; y<bot; y++)
    for(x=0; x<w; x++){
      uint16_t c = fb[y*pitch_px+x];
      uint16_t base = band ? 0x0000 : (uint16_t)((c>>4)&0x0861);
      fb[y*pitch_px+x] = (hit && age<4) ? (uint16_t)(0x3000 | base) : base;
    }
  /* 게임 화면과 맞닿는 쪽에 경계선 — 새 대사면 하얗게 튄다 */
  { uint16_t bc = (age<6) ? COL_WHITE : (hit ? COL_GOLD : (band ? 0x39E7 : 0x52AA));
    int by = (cm_draw==2 || cm_draw==4) ? (bot-1) : top;
    for(x=0; x<w; x++) fb[by*pitch_px+x] = bc;
  }
  /* 얼굴 */
  if(face_ok[spk]){
    int fx=3, fy=top + (boxh-16)/2, a, b;
    if(age < 4) fy -= 1;
    else if(mood==1 && age < 28 && (age % 12) < 6) fy -= 1;
    if(mood==2 && age < 20) fx += ((age>>1)&1) ? 1 : -1;
    for(b=0;b<16;b++) for(a=0;a<16;a++){
      int px=fx+a, py=fy+b;
      if(!face_a[spk][b*16+a]) continue;
      if(px<0||px>=w||py<top||py>=bot) continue;
      fb[py*pitch_px+px] = tint(face_px[spk][b*16+a], mood);
    }
  }
  /* 글자 */
  ty = top + (boxh - nl*lh)/2;
  for(i=0;i<nl && show>0;i++){
    int drawn = small
      ? draw_line  (fb,pitch_px,tx0,x1,seg[i],seg[i+1], ty + i*lh + 1, top, bot, show, col, hit)
      : draw_line11(fb,pitch_px,tx0,x1,seg[i],seg[i+1], ty + i*lh,     top, bot, show, col, 0);
    show -= drawn;
  }
}
