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
#include "ss2comm_icon.h"

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
       CK_FB, CK_IDLE, CK_LONG, CK_MUSE, CK_LORE, CK_REL,
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
  /* 관계는 **썰과 열쇠를 나눠 쓰면 안 된다.** 같이 쓰면 매치 시작 직전에 나간
     썰 한 줄이 관계 대사를 통째로 막는다 (미러전·간다라 대사가 안 나오던 원인). */
  [EV_REL        ] = { CK_REL,  300 },
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
static unsigned rng = 2463534242u;

/* 발화 대기열.
   예전에는 네 칸에 쌓아 두고 1.6초마다 한 줄씩 밀어냈다. 그래서 한 판 44초에 24줄이 나오고
   6초부터 끝까지 간격이 전부 1.6초로 고정됐다 — 반응이 아니라 컨베이어였다.
   KO 를 봐도 그 말이 1.6초 뒤에 나오면 이미 그 순간이 아니다.

   그래서 셋을 건다:
     · 칸을 둘로 줄인다
     · 늦은 반응은 버린다 (Q_STALE 지나면 안 보여 준다 — 없는 것만 못하다)
     · 공방 중에는 GAP_BATTLE 만큼 벌린다. 말할 기회가 드물어야 고르게 된다 */
#define QN 2
/* 얼마나 묵으면 버리나 — 등급이 높을수록 오래 기다려 준다.
   **벽(GAP_BATTLE)보다 짧으면 안 된다.** 짧으면 말한 직후 들어온 것이
   다음 기회가 오기 전에 반드시 죽어서, 흐름 대사가 영영 못 나간다. */
#define Q_STALE      150  /* 2.5초 — 잔반응(맞았다/때렸다)은 그 순간이 지나면 의미가 없다 */
#define Q_STALE_MID  360  /* 6초 — 흐름·기록. 뽑을 때 최신을 고르게 한 뒤로 이 창은
                              「아주 묵은 것만 버리는」 안전장치일 뿐이다 */
#define Q_STALE_BIG  600  /* 10초 — 관계·세계관·KO·총평 */
#define GAP_BATTLE  270   /* 4.5초 — 공방 중 최소 간격 */
#define GAP_OTHER    96   /* 1.6초 — 화면 전환·메뉴에서는 촘촘해도 된다 */
#define GAP_RESULT  150   /* 결과 계열은 한 박자 더 */
typedef struct { char text[160]; short ev; short spk; unsigned at; } ss2q;
static ss2q q[QN];
static int q_head, q_cnt;
static unsigned q_next;

/* 심판 전용 칸 — 해설 대기열과 따로 선다 */
static unsigned char anec_at[15], weap_at[15];  /* 썰·무기 소회를 어디까지 풀었나 */
static unsigned ref_next;                       /* 심판끼리의 최소 간격 */
static unsigned char ref_pend_round;            /* 새 매치 구호 — 전투 화면이 설 때 낸다 */
static char pend_rel[160];                      /* 첫 관계 대사도 같이 미룬다 — VS 에서 소모되면 판에서는 안 보인다 */
static unsigned ref_shown;                      /* 아래 칸에 세운 시각 (0 = 아직) */
static char     ref_text[160];
static unsigned ref_at;
static int      ref_has;


/* 최근에 한 말은 다시 안 한다. 예전에는 44초 안에 같은 줄이 세 번 나왔다. */
#define RECENT_N   12
#define RECENT_F 1800     /* 30초 */
static unsigned recent_h[RECENT_N], recent_f[RECENT_N];
static int recent_i;
static unsigned line_hash(const char *s){
  unsigned h = 2166136261u;
  while(*s){ h ^= (unsigned char)*s++; h *= 16777619u; }
  return h;
}
static int said_recently(const char *s){
  unsigned h = line_hash(s); int i;
  for(i = 0; i < RECENT_N; i++) if(recent_h[i] == h && cm_f - recent_f[i] < RECENT_F) return 1;
  for(i = 0; i < q_cnt; i++) if(line_hash(q[(q_head+i)%QN].text) == h) return 1;  /* 대기 중인 것도 */
  return 0;
}
static void mark_said(const char *s){
  recent_h[recent_i] = line_hash(s); recent_f[recent_i] = cm_f;
  recent_i = (recent_i + 1) % RECENT_N;
}


/* ── 무엇을 먼저 말할까 ────────────────────────────────────────────
   말할 기회를 4.5초에 한 번으로 줄이면 **무엇을 버릴지**가 곧 성격이 된다.
   먼저 온 것부터 내보내면 「좋은 베기다!」 같은 잔반응이 자리를 차지하고
   관계 대사가 밀려난다 — 실제로 그랬다.
   그래서 자리를 다툴 때는 아래 등급이 이긴다.

     3  관계·세계관   — 상대가 누구인지 아는 말. 이 앱의 존재 이유다
     2  판을 가르는 순간 — KO·역전·총평
     1  흐름·기록
     0  잔반응        — 맞았다/때렸다. 없어도 아쉽지 않다 */
/* 우선도만으로는 안 갈린다. 축이 하나 더 필요하다 —
   **지금 아니면 의미 없는 말**(KO·총평·결과·흐름)과 **나중에 해도 되는 말**(관계·썰·사담).
   관계·썰은 빈 자리·메뉴·라운드마다 나갈 데가 많지만, KO 반응은 그 순간을 놓치면
   없는 것만 못하다. 그래서 자리가 없을 때는 **두어도 되는 말부터 밀어낸다.**
   (증상: 매치 시작에 들어온 관계 대사 둘이 두 칸을 차지한 채 심판이 게이트를 잡고
    있는 동안, 그 뒤 KO·퍼펙트·총평·결과가 통째로 버려졌다) */
static int ev_keep(int ev){
  switch(ev){
    case EV_REL: case EV_LORE: case EV_CHARSELCHAT: case EV_START:
    case EV_VSQ: case EV_STORYCHAT: case EV_MENUIDLE:
    case EV_MUSE_B: case EV_MUSE_Q: case EV_MUSE_M: case EV_IDLE:
    /* 라운드 맥락(「한 판 챙겼군」 「최종 라운드다」)도 후순위다. 심판과는 자리가
       갈렸지만 **KO·총평과는 여전히 다툰다** — 되돌려 보니 검사 셋이 다시 깨졌다.
       이쪽은 놓쳐도 다음 판에 또 올 말이고, KO 반응은 그 순간뿐이다. */
    case EV_ROUND: case EV_ROUNDLEAD: case EV_ROUNDBEHIND: case EV_MATCHPOINT:
      return 0;                 /* 두어도 되는 말 */
    default:
      return 1;                 /* 지금 아니면 의미 없는 말 */
  }
}
static unsigned char ev_prio(int ev){
  if(ev == -3) return 4;    /* 심판 구호 — **무엇보다 먼저.** 판을 여는 건 심판이다 */
  if(ev == -2) return 0;    /* 짝꿍이 받는 말 — 있으면 좋고 없어도 그만 */
  if(ev == -1) return 3;    /* ss2comm_notify — 사용자에게 꼭 보여야 하는 안내 */
  switch(ev){
    case EV_REL: case EV_LORE: case EV_CHARSELCHAT: case EV_START:
    case EV_VSQ: case EV_STORYCHAT:
      return 3;
    case EV_KO: case EV_KOED: case EV_DKO: case EV_MOVEKO:
    case EV_PERFECT: case EV_COMEBACK: case EV_QUICK: case EV_REVERSAL:
    case EV_ARCSWEEP: case EV_ARCSWEPT: case EV_ARCCOMEBACK:
    case EV_ARCSWEAT: case EV_ARCCHOKE: case EV_ARCSLIP:
    case EV_WINTALK: case EV_LOSETALK: case EV_ENDING:
      return 2;
    case EV_HIT: case EV_TAKEN: case EV_DOWN: case EV_DOWNED: case EV_OPPSP:
    case EV_MOVE: case EV_MOVEHIT: case EV_MOVEHITL:
    case EV_MOVEDOWN: case EV_MOVEDOWNA:
    case EV_MUSE_B: case EV_MUSE_Q: case EV_MUSE_M: case EV_IDLE:
    case EV_ROUND: case EV_ROUNDLEAD: case EV_ROUNDBEHIND: case EV_MATCHPOINT:
      return 0;
    default:
      return 1;
  }
}

static unsigned rnd(void){ rng ^= rng<<13; rng ^= rng>>17; rng ^= rng<<5; return rng; }
static int rd(int off){ return RAM[off]; }
static int rd16(int off){ return RAM[off] | (RAM[off+1]<<8); }

void ss2comm_set_enabled(int on){ cm_on = on ? 1 : 0; }
/* 해설자를 바꾸면 **앞 사람 말을 지운다.**
   예전에는 cur_spk 만 새 사람으로 바꿔서, 얼굴은 바뀌었는데 글은 앞 사람 것이
   그대로 남았다 — 새 화자가 남의 말을 하는 것처럼 보인다.
   대기 중인 앞 사람 말도 같이 버린다. 바로 뒤에 소개 대사가 들어온다. */
static void speaker_switched(void){
  curline[0] = 0; cur_ev = -1; cur_f = cm_f;
  cur_spk = cm_spk;
  q_head = q_cnt = 0; q_next = 0; ref_has = 0;
}
void ss2comm_set_speaker(int idx){
  if(idx < 0 || idx >= SS2COMM_SPK_N || idx == cm_spk) return;
  cm_spk = idx;
  speaker_switched();
}
/* v0.7: 해설자가 15명이 되면서 프런트엔드가 이름·수를 알아야 한다.
   버튼 하나로 교대하는 길도 여기서 연다 — 코어·앱이 같은 순서를 쓴다. */
int ss2comm_speaker_count(void){ return SS2COMM_SPK_N; }
const char *ss2comm_speaker_name(int idx){
  return (idx>=0 && idx<SS2COMM_SPK_N) ? SPK_NAME[idx] : "";
}
int ss2comm_get_speaker(void){ return cm_spk; }
/* 짝꿍(두 명이 주고받기)은 폐기했다. 옛 호출부가 남아 있어도 조용히 무시한다. */
void ss2comm_set_duo(int on){ (void)on; }
/* 다음 해설자로 넘기고 그 사람 번호를 돌려준다. 인사 한마디는 프런트엔드가 띄운다. */
int ss2comm_next_speaker(int step){
  int n = SS2COMM_SPK_N;
  cm_spk = ((cm_spk + (step?step:1)) % n + n) % n;
  speaker_switched();
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
  memset(anec_at,0,sizeof anec_at); memset(weap_at,0,sizeof weap_at);
  ref_next=0; ref_pend_round=0; pend_rel[0]=0;
  st_lastStage=-1; st_roundStart=st_offAt=st_actAt=st_menuAt=st_selChatAt=st_hitAt=0;
  st_selChatN=0; st_myChar=st_oppChar=-1;
  curline[0]=0; cur_f=0; cur_ev=-1; cur_spk=cm_spk;
  last_line_f=0; last_input_f=0; hush_until=0;
  pend_name=0; pend_sup=0; pend_left=0;
  { int k; fl_hit=fl_tak=fl_alt=fl_lastBy=fl_oppSp=0; fl_said=0; fl_nr=0; fl_rounds[0]=0;
    for(k=0;k<FLMV;k++){ fl_mv[k].name=0; fl_mv[k].n=0; } }
  q_head=q_cnt=0; q_next=0; ref_has=0;
  memset(recent_h,0,sizeof recent_h); memset(recent_f,0,sizeof recent_f); recent_i=0;
}

/* 대사 한 줄 만들어 대기열에 넣는다.
   vsel < 0 이면 변형 중 무작위, 0 이상이면 그 변형을 고른다.
   돌려주는 값: 1 = 받아들임(쿨다운 통과), 0 = 무시됨 → 호출부의 || 사슬이 다음 후보로 넘어간다 */
/* ── v0.7 짝꿍 — 큰 장면에서 다른 한 명이 짧게 받는다 ────────────
   한 명이 혼자 떠들면 심심하다. 받는 말은 길면 안 된다 — 한 호흡이어야 주고받는 맛이 난다.
   잔반응과 승부 순간은 **시계를 따로** 쓴다. 하나로 두면 시작 멘트에 한 번 받아친 것 때문에
   정작 KO 를 못 받는다(브라우저판에서 실제로 그랬다). */

/* ── 심판 (쿠로코) ─────────────────────────────────────────────
   해설자와 **별개 목소리**다. 누구를 해설자로 골라도 심판은 늘 거기 있다 —
   그래서 「전통」이 된다. 라운드 시작과 승부가 갈리는 순간은 원래 아무도
   말하지 않는 자리라 해설 예산을 뺏지도 않는다.

   이 표는 실행기에서 나온 것이 아니라 C 쪽에서 새로 쓴 것이라
   생성기(ss2comm_lines.h)와 얽히지 않게 여기 둔다.

   본명은 이 게임이 음성도 없고 화면에 띄우지도 않는다. 그래서 여기서만 부른다. */
#define SS2_SPK_REF (-2)          /* 얼굴 없이, 심판 색으로 */

static const char *const REF_ROUND[3] = {
  "첫 판 — 정정당당히, 승부!",
  "둘째 판 — 정정당당히, 승부!",
  "셋째 판 — 정정당당히, 승부!",
};

/* 승부가 갈리면 승자의 **본명**을 부른다. 갈포드는 성이 없다. */
static const char *const CHARFULL[15] = {
  "카자마 카즈키", "카자마 소게츠", "하오마루", "키바가미 겐주로",
  "나코루루", "리무루루", "핫토리 한조", "갈포드",
  "아수라", "샤를로트 크리스틴 드 콜데", "모로즈미 타무리키", "타치바나 우쿄",
  "야규 쥬베이", "시키", "유가",
};

/* 심판은 **해설 대기열을 쓰지 않는다.** 다른 목소리니 줄도 따로 선다.
   같은 칸을 쓰게 했더니 승부가 갈리는 순간 구호가 총평을 밀어냈다 —
   둘 다 나와야 하는 자리다. 구호가 먼저, 해설이 그 뒤. */
static void ref_say(const char *text){
  if(!cm_on || !text || !*text) return;
  if(said_recently(text)) return;
  snprintf(ref_text, sizeof ref_text, "%s", text);
  ref_shown = 0;
  ref_at  = cm_f;
  ref_has = 1;
}
/* 몇 판째인지는 **따낸 판 수**로 센다. 화면에 라운드 번호가 없기 때문이다. */
/* 심판이 설 판인가. 쿠로코는 **사람끼리의 정식 승부**를 보는 사람이다.
   마계에서 온 것(유가)이나 시체를 꿰맨 인형(간다라)과의 싸움은 승부가 아니라 그냥
   싸움이라, 구호도 「오미고토」도 없다. 구호가 뚝 끊기는 것 자체가 「여기서부터는
   다른 판이다」라는 신호가 된다. 원작에서도 보스전에는 심판이 안 나온다. */
static int ref_stands(void){
  if(st_oppChar < 0)  return 0;      /* 표 밖 개체 = 간다라 */
  if(st_oppChar == 14) return 0;     /* 유가 */
  return 1;
}
static void ref_round(void){
  int rn;
  if(!ref_stands()) return;
  rn = st_myR + st_opR; if(rn > 2) rn = 2;
  ref_say(REF_ROUND[rn]);
}



/* ── 조사 보정 ────────────────────────────────────────────────────
   대사표에는 「%s로 눕혔군」처럼 조사가 **박혀** 있다. 생성기가 받침 없는
   가짜 이름으로 훑어서 「로」가 굳은 것이다. 그러면 「부동격로」가 된다.
   그래서 이름을 끼워 넣을 때 뒤따르는 조사를 마지막 글자 받침에 맞춰 고친다.
   실행기(index.html)의 RO()/은는/이가 처리와 같은 규칙이다. */
static int kr_batchim(const char *s){
  /* 문자열 마지막 한글 음절의 종성 유무. 한글이 아니면 -1 */
  const unsigned char *p = (const unsigned char*)s;
  int last = -1;
  while(*p){
    if((p[0] & 0xF0) == 0xE0 && p[1] && p[2]){
      unsigned cp = ((p[0]&0x0F)<<12) | ((p[1]&0x3F)<<6) | (p[2]&0x3F);
      if(cp >= 0xAC00 && cp <= 0xD7A3) last = (int)((cp - 0xAC00) % 28);
      else last = -1;
      p += 3;
    }else if((p[0] & 0xE0) == 0xC0 && p[1]){ last = -1; p += 2; }
    else if((p[0] & 0xF8) == 0xF0 && p[1] && p[2] && p[3]){ last = -1; p += 4; }
    else { last = -1; p += 1; }
  }
  return last;   /* -1 한글 아님 / 0 받침 없음 / 8 = ㄹ / 그 밖 받침 있음 */
}
/* 조사 한 쌍. 앞이 「받침 없을 때」, 뒤가 「받침 있을 때」 */
static const char *const JOSA[][2] = {
  {"로","으로"},{"가","이"},{"를","을"},{"는","은"},{"와","과"},{"야","아"},{0,0}
};
/* fmt 안의 %s 를 who 로 바꾸되, 바로 뒤 조사를 받침에 맞춰 고쳐 쓴다 */
static void fill_name(char *out, size_t cap, const char *fmt, const char *who){
  const char *ph = strstr(fmt, "%s");
  size_t pre;
  int b, i;
  if(!ph || !who || !*who){ snprintf(out, cap, fmt, who ? who : ""); return; }
  pre = (size_t)(ph - fmt);
  if(pre >= cap) pre = cap - 1;
  memcpy(out, fmt, pre); out[pre] = 0;
  strncat(out, who, cap - strlen(out) - 1);
  b = kr_batchim(who);
  ph += 2;                                    /* %s 를 지나 */
  if(b >= 0){
    for(i = 0; JOSA[i][0]; i++){
      size_t l0 = strlen(JOSA[i][0]), l1 = strlen(JOSA[i][1]);
      /* 표에 박힌 쪽이 「받침 없음」형이든 「받침 있음」형이든 둘 다 받는다 */
      const char *hit = 0;
      if(!strncmp(ph, JOSA[i][0], l0)) { hit = ph + l0; }
      else if(!strncmp(ph, JOSA[i][1], l1)) { hit = ph + l1; }
      if(!hit) continue;
      /* 「로」는 ㄹ 받침(8)도 받침 없음 취급 */
      { int noB = (b == 0) || (i == 0 && b == 8);
        strncat(out, JOSA[i][noB ? 0 : 1], cap - strlen(out) - 1); }
      ph = hit;
      break;
    }
  }
  strncat(out, ph, cap - strlen(out) - 1);
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
  if(strstr(fmt,"%s"))                 fill_name(outbuf,sizeof(outbuf),fmt,who);
  else if(strstr(fmt,"%d")){
    const char *p = strstr(fmt,"%d");
    if(strstr(p+2,"%d"))               snprintf(outbuf,sizeof(outbuf),fmt,n1,n2);
    else                               snprintf(outbuf,sizeof(outbuf),fmt,n1);
  }
  else                                 snprintf(outbuf,sizeof(outbuf),"%s",fmt);
  if(said_recently(outbuf)) return 0;      /* 최근에 한 말은 다시 안 한다 */
  cd[key] = cm_f + EVCD[ev].cool;
  { int slot;
    if(q_cnt < QN) slot = (q_head + q_cnt++) % QN;
    else {
      /* 꽉 찼다. **지금 아니면 의미 없는 말이 들어오면, 두어도 되는 말부터 밀어낸다.**
         그 안에서는 등급이 낮은 쪽, 같으면 묵은 쪽. */
      int i, worst = -1;
      if(ev_keep(ev)){
        for(i = 0; i < q_cnt; i++){
          int c = (q_head + i) % QN;
          if(ev_keep(q[c].ev)) continue;
          if(worst < 0 || ev_prio(q[c].ev) < ev_prio(q[worst].ev)) worst = c;
        }
      }
      if(worst < 0){
        worst = q_head;
        for(i = 1; i < q_cnt; i++){
          int c = (q_head + i) % QN;
          if(ev_prio(q[c].ev) < ev_prio(q[worst].ev)) worst = c;
        }
        /* 새것이 **더 하찮을 때만** 버린다. 같은 등급이면 새것이 이긴다 —
           한꺼번에 몰려올 때 먼저 온 둘만 남으면, 마지막에 오는 총평이 늘 밀린다. */
        if(ev_prio(q[worst].ev) > ev_prio(ev)) return 0;
      }
      slot = worst;
    }
    snprintf(q[slot].text,sizeof(q[slot].text),"%s",outbuf);
    q[slot].at = cm_f;
    q[slot].ev = (short)ev;
    q[slot].spk = (short)cm_spk;
  }
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

/* ── 썰과 무기 소회 ───────────────────────────────────────────────
   LORE[] 는 한 줄짜리 이름표라 「썰」이 못 된다. ANEC[] 은 캐릭터마다 여섯 줄짜리
   일화·행적이고, WEAP[] 은 무기 갈래에 대한 소회다. 둘 다 **화자별이 아니라
   캐릭터별**이다 — 누가 해설하든 그 사람에 대한 이야기는 같다.
   캐릭터마다 어디까지 풀었는지 기억해 두고 차례로 낸다. 같은 판에서 같은 썰이
   두 번 나오면 썰이 아니라 소음이다. */
static int say_anec(int ch){
  int i, n = 0;
  if(ch < 0 || ch >= 15) return 0;
  for(i = 0; i < SS2COMM_ANEC_N; i++) if(ANEC[ch][i]) n++;
  if(!n) return 0;
  for(i = 0; i < n; i++){
    const char *t = ANEC[ch][(anec_at[ch] + i) % n];
    if(!t) continue;
    anec_at[ch] = (unsigned char)((anec_at[ch] + i + 1) % n);
    return emits(EV_LORE, t);
  }
  return 0;
}
static int say_weap(int ch){
  int i, n = 0;
  if(ch < 0 || ch >= 15) return 0;
  /* 화자 목소리로 쓴 줄이 먼저다. 사실은 같아도 느낌이 달라야 한다 —
     같은 「피 밴 검」이 겐주로에겐 당연하고, 나코루루에겐 무섭고, 유가에겐 취향이다.
     (예전에는 WEAP[상대] 하나뿐이라 누가 해설하든 같은 문장이 나왔다) */
  if(WEAPV[cm_spk][ch]) return emits(EV_LORE, WEAPV[cm_spk][ch]);
  for(i = 0; i < SS2COMM_WEAP_N; i++) if(WEAP[ch][i]) n++;
  if(!n) return 0;
  { const char *t = WEAP[ch][weap_at[ch] % n];
    weap_at[ch] = (unsigned char)((weap_at[ch] + 1) % n);
    return t ? emits(EV_LORE, t) : 0; }
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
  q[slot].at  = cm_f;                    /* 안내는 최근-중복 검사를 거치지 않는다 */
}

const char *ss2comm_current(int *age){
  if(age) *age = (int)(cm_f - cur_f);
  return curline[0] ? curline : 0;
}

/* 검사용 — 아무 문장이나 띠에 올려서 **그림으로** 확인한다.
   글꼴에 없는 글자·잘림·줄바꿈은 표를 들여다봐서는 안 보이고 그려 봐야 보인다.
   배포 빌드에는 안 들어간다 (bandshot.c 만 이 매크로를 켠다). */
#ifdef SS2COMM_TEST
/* 심판은 이제 대사 흐름(ss2comm_frame)에 안 들어간다 — 아래 칸에 따로 선다.
   그래서 시뮬레이터가 심판을 놓친다. 세워진 구호를 한 번만 돌려주는 창구를 둔다. */
const char *ss2comm_test_ref_take(void){
  static unsigned last;
  if(!ref_has || cm_f < ref_next) return 0;
  if(ref_at == last) return 0;
  last = ref_at;
  return ref_text;
}
/* 롬이 없는 방에서 심판 초상 **그리기 경로**만 확인하려고 합성 그림을 직접 밀어 넣는다.
   실제 그림은 언제나 사용자 롬에서 나온다 — 이 훅은 배포 빌드에 없다. */
void ss2comm_test_ref_face(const uint16_t *px, const unsigned char *al){
  extern uint16_t ref_px[]; extern unsigned char ref_a[]; extern unsigned char ref_ok;
  memcpy(ref_px, px, sizeof(uint16_t)*32*32);
  memcpy(ref_a, al, 32*32);
  ref_ok = 1;
}
void ss2comm_test_say(const char *t, int spk){
  snprintf(curline, sizeof curline, "%s", t);
  cur_ev = -1; cur_spk = spk; cur_f = cm_f; last_line_f = cm_f;
}
#endif

/* 블록값 → 캐릭터 번호. 0x98 은 쿠로코 수라와 샤를로트 나찰이 겹치는데,
   브라우저판과 같이 샤를로트로 해석한다(실기 제보 반영). */
/* 캐릭터 바이트는 (번호<<4 | 유파<<3) 꼴이라 **하위 3비트가 반드시 0**이다.
   실행기(readOpp)는 v%8!==0 이면 버리는데 C 에는 그 검사가 없어서,
   캐릭터 고르는 중의 과도값이나 쿠로코 같은 숨은 캐릭터(번호가 표에 없다)에서
   위 4비트만 우연히 0~14 에 들면 **엉뚱한 사람으로 읽혔다.**
   그 사람의 관계 대사가 그대로 나오니 「고르면 아무나로 바뀐다」로 보인다. */
static int blk_char(int blk){
  int c;
  if(blk < 0 || (blk & 7)) return -1;   /* 실행기와 같은 유효성 검사 */
  c = blk >> 4;
  if(c < 0 || c >= 15) return -1;
  /* 표에 없는 개체(중간보스 간다라 등)는 **-1 로 떨어뜨려 침묵**시킨다.
     예전에는 위 4비트만 우연히 범위에 들면 겐주로 등으로 **오인**해서
     엉뚱한 관계 대사가 나갔다. 실사용 제보: 「간다라 못 알아보는 유가」. */
  return c;
}
/* 간다라는 로스터 밖 중간보스다. 해설자 15명에도 없다.
   따로 알아보게 해 둔다 — 유가만 제 물건이라 아는 척을 한다. */
#define SS2_CHAR_GANDHARA 15

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
    if(scr>=8 && p_scr<8 && ref_pend_round){
      /* 판이 선 **그 프레임**에 구호(0초). 호명과의 심판 간격은 여기선 안 기다린다 —
         호명은 VS 화면에서 제 시간을 다 썼고, 판이 서면 판 이야기를 해야 한다. */
      ref_pend_round=0; ref_next=cm_f; ref_round();
      if(pend_rel[0]){
        emits(EV_REL, pend_rel); pend_rel[0]=0;
        q_next = cm_f + 180;                     /* 관계는 구호 3초 뒤 — 겹치지 않게 */
      }
    }
    if(p_scr>=8 && (scr==0 || scr==2)){
      /* 매치가 실제로 끝났을 때만 결과 멘트를 낸다(2선승). 라운드 하나 이긴 것으로는 말하지 않는다.
         승패 화면에서는 **두 줄까지** — 결과 한 마디 + 한마디 더. 그 뒤 잡담은 잠시 잠근다.
         (제보: "대전 사이 승부 난 뒤 대사가 어색하다" — 여러 줄이 몰리고 잡담이 끼어들었다) */
      int done = (st_myR>=2 || st_opR>=2);
      if(!st_resultDone && done && (st_won || st_lost)){
        int wc = st_won ? st_myChar : st_oppChar;   /* 이긴 쪽 */
        st_resultDone = 1;
        /* 승자의 본명을 부른다. 이 게임은 음성도 없고 본명을 띄우지도 않아서
           여기서만 불리는 이름이 된다. */
        if(wc >= 0 && wc < 15 && ref_stands()){
          char t[160];
          snprintf(t, sizeof t, "%s — 훌륭하오!", CHARFULL[wc]);
          ref_say(t);
        }
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
    else if(scr==0 && p_mode!=MD_BATTLE){
      /* 문구(VS) 화면. 심판이 먼저 **풀네임으로 대진을 호명한다** —
         「카자마 카즈키 대 모로즈미 타무리키!」. 이 게임은 본명을 화면에 안 띄우니
         여기서만 불리는 이름이 된다(승자 호명과 같은 원리). 심판이 안 서는 판
         (상대가 유가·간다라)에는 호명도 없다 — 구호와 같은 규칙. */
      { int me2 = blk_char(rd(OFF_BLK1)), op2 = blk_char(rd(OFF_BLK2));
        if(me2>=0 && op2>=0 && op2!=14){
          char t[96];
          snprintf(t,sizeof t,"%s 대 %s!",CHARFULL[me2],CHARFULL[op2]);
          ref_say(t);
        } }
      /* **대진 소개(해설 쪽)도 여기가 제 자리다.** 예전에는 전투가 시작되는
         순간에 심판 구호와 함께 나가서 서로 겹쳤다(제보: 「시작 메시지랑 캐릭터 첫
         메시지가 겹친다」). 여기로 옮기니 겹치지도 않고, EV_START 60줄이 살아난다. */
      int me = blk_char(rd(OFF_BLK1)), op = blk_char(rd(OFF_BLK2));
      if(me>=0 && op>=0){
        char who[64];
        snprintf(who,sizeof(who),"%s 대 %s",CHARNAME[me],CHARNAME[op]);
        if(!emits(EV_START, who)) emit(EV_VSQ);
      }else if(!emits(EV_START, "한판")) emit(EV_VSQ);
    }
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
      /* 판을 여는 건 **심판 하나**다. 예전에는 여기서 심판 구호 + EV_START(「하오마루 대
         겐주로…」) + 관계 대사가 한꺼번에 몰려, 두 칸짜리 대기열이 막히면서
         **흐름 대사가 통째로 버려졌다**(test_flow 6개 실패). 제보도 같았다 —
         「시작 메시지랑 캐릭터 첫 메시지랑 겹친다」.
         그래서 심판이 열고, 해설은 **관계 한 줄**로 받는다. 대진 소개는 심판이 이미 했다. */
      /* 진입 순간은 대개 아직 **문구(VS) 화면**이다(mode 는 이미 전투다).
         여기서 바로 구호를 내면 방금 선 풀네임 호명을 한 프레임 만에 덮는다 —
         appview 렌더로 잡았다. 구호는 실제 전투 화면(scr 8)이 설 때로 미룬다. */
      if(scr >= 8) ref_round(); else ref_pend_round = 1;
      /* 2절 — 관계 대사. 순서가 중요하다:
           같은 캐릭터끼리면 미러 전용 (제보: 「본인이 상대인데도 대사가 좆같음」)
           상대를 못 알아보면 표 밖 개체 = 간다라 (제보: 「간다라 못 알아보는 유가」)
           그 밖에는 화자→상대, 없으면 화자→내 편 */
      if(st_myChar>=0 && st_myChar==st_oppChar) rel = RELSELF[cm_spk];
      else if(st_oppChar<0)                     rel = RELGAND[cm_spk];
      else rel = RELOPP[cm_spk][st_oppChar];          /* 225칸 — 빈칸 없음 */
      if(!rel && st_myChar>=0) rel = RELME[cm_spk][st_myChar];
      /* 제보: 「초기 3초·6초 멘트를 0초·3초로」. 진입이 VS 화면이면 관계 대사도
         여기서 내지 않고 미룬다 — VS 에서 소모되면 정작 판에서는 안 보인다. */
      if(scr >= 8){
        if(rel) emits(EV_REL, rel);
        else if(st_oppChar>=0 && LORE[st_oppChar]){
          char t[160];
          snprintf(t,sizeof(t),"%s — %s",CHARNAME[st_oppChar],LORE[st_oppChar]);
          emits(EV_LORE, t);
        }
      }else{
        if(rel) snprintf(pend_rel,sizeof pend_rel,"%s",rel);
        else if(st_oppChar>=0 && LORE[st_oppChar])
          snprintf(pend_rel,sizeof pend_rel,"%s — %s",CHARNAME[st_oppChar],LORE[st_oppChar]);
      }
    }else{                                        /* 라운드 재개 */
      st_roundN++; st_fb=st_longSaid=st_dblLow=0;
      flow_reset(0);                              /* v0.7 라운드 단위 관찰만 */
      ref_round();                                /* 심판이 먼저 판을 연다 */
      /* 판이 설 때마다 관계를 한 줄씩 돌린다 — 맞은편 → 내 편 → 사람.
         제보: 「관계를 전혀 안 보여주노, 넘치게 넣어라」.
         예전에는 매치 진입 한 번뿐이라 한 판에 한 줄이 전부였다. */
      { static unsigned char rturn;
        const char *r2 = 0;
        switch(rturn++ % 3){
          case 0: if(st_oppChar>=0) r2 = RELOPP[cm_spk][st_oppChar]; break;
          case 1: if(st_myChar>=0)  r2 = RELME [cm_spk][st_myChar];  break;
          default:{ int n=0,i2; for(i2=0;i2<SS2COMM_RELYOU_N;i2++) if(RELYOU[cm_spk][i2]) n++;
                    if(n) r2 = RELYOU[cm_spk][rnd()%(unsigned)n]; } break;
        }
        if(r2) emits(EV_REL, r2); }
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
      /* 메뉴에서 **7초마다** 한 마디. 예전에는 캐릭터 선택(scr 2)만 사담이 있었고
         나머지 화면은 15초짜리 「고민이 길구나」 하나뿐이었다. 그래서 카드 그림을
         한참 들여다보는 동안 통째로 조용했다 — 제보: 「카드 고를 때 왜 닥치고 있노」.
         고르는 화면(2 캐릭터 · 4 검질 · 6 카드)에서는 사담과 **썰**을 번갈아 낸다.
         카드 그림을 보고 있을 때 제 캐릭터 이야기를 듣는 게 제일 어울린다. */
      int picking = (scr==2 || scr==4 || scr==6);
      if(!st_selChatAt) st_selChatAt=cm_f;
      if(!st_menuAt)    st_menuAt=cm_f;
      if(picking){
        if(cm_f > hush_until && cm_f - st_selChatAt > 420){
          int said;
          st_selChatAt = cm_f; st_selChatN++;
          said = (st_selChatN & 1) ? emit(EV_CHARSELCHAT) : 0;
          if(!said){
            int me = blk_char(rd(OFF_BLK1));
            if(!say_anec(me)) emit(EV_CHARSELCHAT);
          }
        }
      }else if(cm_f > hush_until && cm_f - st_menuAt > 420){
        st_menuAt = cm_f;
        if(!emit(EV_MENUIDLE)) emit(EV_MUSE_M);
      }
    }
    if(hp1>32) st_low1=0;
    if(hp2>32) st_low2=0;
    if(hp1>0 && hp2>0){
      /* 체력이 가득 돌아왔다 = 다음 판이 선다. mode 를 안 벗어나는 전환도 여기로 온다
         (체력이 오르는 틱은 mode 가 전투여도 이 갈래로 떨어진다 — 위 조건 참고). */
      if(st_ko && mode==MD_BATTLE && hp1>=100 && hp2>=100 && (st_myR+st_opR)>=1) ref_round();
      st_ko=0;
    }
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
    if(hit2 && !hit1 && st_roundN==1 && hp2>0 && !pend_name && (p_hp2-hp2)>=4) (((rnd()&1) && say_weap(st_oppChar)) || emit(EV_FIRSTBLOOD));
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
    /* 전투 15초 / 그 밖 5초. 예전에는 6초·3초였는데, 말할 기회를 4.5초로 조이고 나니
       그 기준이면 늘어난 침묵을 혼잣말이 전부 차지해 흐름 대사가 벽에 막혔다.
       잡담은 진짜로 오래 빌 때만 나와야 한다. */
    /* 전투 8초 / 그 밖 5초. 예전에 6초였던 것을 15초로 올려 놨더니
       「중간에 빌 때는 닥치고 있으니 아깝다」가 됐다. 8초로 되돌린다 —
       말할 기회 자체는 4.5초 간격 규칙이 막고 있어서 수다스러워지지 않는다. */
    /* 두 단계로 본다. **썰이 혼잣말보다 먼저** 나와야 한다 —
       6초쯤 비면 상대 이야기를 풀고, 그래도 8초를 넘기면 그때 혼잣말이다.
       (전에는 15초 하나뿐이라 실제로는 아무 말도 안 나왔다. 제보: 「빌 때 아깝다」) */
    unsigned anecN = (mode==MD_BATTLE) ? 360 : 240;   /* 6초 / 4초 */
    unsigned need  = (mode==MD_BATTLE) ? 480 : 300;   /* 8초 / 5초 */
    if(cm_f > 300 && quiet > anecN && cm_f > hush_until && !st_ko){ /* KO 연출 중엔 잡담 금지 */
      /* 상대 이야기를 한 번 풀고, 다음 차례엔 내 편 이야기. 번갈아 간다. */
      static unsigned char turn;
      /* 메뉴에서는 아직 매치가 안 잡혀 st_myChar 가 비어 있다. 램에서 바로 읽는다 —
         안 그러면 메뉴에선 썰이 한 줄도 안 나가고 「…지루하구나」만 돈다. */
      int a1c = (mode==MD_BATTLE) ? st_oppChar : blk_char(rd(OFF_BLK2));
      int a2c = (mode==MD_BATTLE) ? st_myChar  : blk_char(rd(OFF_BLK1));
      int said;
      if(turn & 1){ int t = a1c; a1c = a2c; a2c = t; }
      /* 세 번에 한 번은 **혼잣말** 차례로 둔다. 안 그러면 썰이 빈 자리를 전부 먹어
         MUSE 계열 60줄이 통째로 죽는다(시뮬레이터가 「한 번도 안 나옴」으로 잡았다). */
      /* 빈 자리 차례: 썰 → 썰 → 관계 → 혼잣말 순으로 돈다.
         관계가 「매치 시작 한 번」에서 빠져나와 빈 자리에도 들어간다. */
      said = 0;
      if((turn % 4) == 2){
        const char *r3 = 0;
        if(a1c >= 0)      r3 = RELOPP[cm_spk][a1c];
        if(!r3 && a2c>=0) r3 = RELME [cm_spk][a2c];
        if(r3) said = emits(EV_REL, r3);
      }
      if(!said && (turn % 4) != 3) said = (say_anec(a1c) || say_anec(a2c));
      if(said) turn++;
      else if(quiet > need)
        { turn++; emit(mode==MD_BATTLE ? EV_MUSE_B : (mode==MD_QUOTE ? EV_MUSE_Q : EV_MUSE_M)); }
      else if((turn % 4) == 3) turn++;
    }
  }
  /* 묵은 반응은 보여 주지 않고 버린다 — 그 순간이 지난 말은 없는 것만 못하다.
     다만 관계·세계관 쪽은 더 기다려 준다. VS 화면이 짧아 2.5초로는 못 나간다. */
  while(q_cnt){
    unsigned pr  = ev_prio(q[q_head].ev);
    unsigned lim = pr >= 2 ? Q_STALE_BIG : pr >= 1 ? Q_STALE_MID : Q_STALE;
    if(cm_f - q[q_head].at <= lim) break;
    q_head = (q_head+1)%QN; q_cnt--;
  }

  /* 심판 구호가 먼저 나간다. 해설 대기열은 건드리지 않으므로 바로 뒤에 총평이 붙는다. */
  /* 심판은 **제 차선**이다. 해설 간격(4.5초)을 같이 기다리면 판이 이미 시작된 뒤에
     「첫 판 —」이 뜬다(제보: 「늦고 밀린다」). 그래서 해설 게이트는 안 본다.
     다만 **제 간격은 지킨다.** 안 그러면 판이 몰릴 때 심판이 연달아 네 번 떠들면서
     KO·총평을 통째로 굶긴다. 몰린 구호는 ref_text 가 덮어써서 **최신 것 하나로 합쳐진다** —
     세 판이 순식간에 지나가면 「셋째 판」만 나오는 게 맞다. */
  /* 심판은 **화면 아래 레터박스**에 따로 선다(SS2_REF_H). 해설창과 자리를 나누므로
     대기열을 두고 다툴 일이 없다 — 구호가 뜨는 동안에도 위에서는 해설이 제 말을 한다.
     여기서는 묵은 구호만 지운다. 그리는 것은 ss2comm_draw 가 한다. */
  if(ref_has && cm_f - ref_at > 90) ref_has = 0;   /* 1.5초 안에 못 세우면 버린다 */

  if(q_cnt && cm_f >= q_next){
    /* 등급이 높은 것부터. 같은 등급이면 **제일 최근 것**을 낸다.
       예전에는 먼저 들어온 쪽을 냈다. 그러면 말할 차례가 왔을 때 이미 지난 얘기를
       하게 되고, 그걸 막으려고 「몇 초 지나면 버린다」는 창을 캐릭터별로 매번
       손봐야 했다. 뽑을 때 최신을 고르면 애초에 상할 일이 없다 —
       상태는 매 프레임 보고 있고, 지켜야 하는 건 **말 사이 간격 하나**뿐이다. */
    int i, best = q_head;
    ss2q chosen;
    for(i = 1; i < q_cnt; i++){
      int c = (q_head + i) % QN;
      int pc = ev_prio(q[c].ev), pb = ev_prio(q[best].ev);
      if(pc > pb || (pc == pb && q[c].at > q[best].at)) best = c;
    }
    chosen = q[best];
    if(best != q_head) q[best] = q[q_head];
    q_head = (q_head+1)%QN; q_cnt--;
    snprintf(curline,sizeof(curline),"%s",chosen.text);
    cur_ev = chosen.ev; cur_spk = chosen.spk; cur_f = cm_f; last_line_f = cm_f;
    mark_said(curline);
    /* 공방 중에는 넓게 벌린다. 말할 기회가 드물어야 아무 말이나 안 하게 된다.
       결과 계열(승패 화면·한마디 더·전적)은 한 박자 더. */
    q_next = cm_f + ((cur_ev==EV_WINSCR||cur_ev==EV_LOSESCR||cur_ev==EV_WINTALK||
                      cur_ev==EV_LOSETALK||cur_ev==EV_RECORD) ? GAP_RESULT
                    : (ev_prio(cur_ev) >= 3) ? GAP_OTHER      /* 관계·안내는 드무니 막지 않는다 */
                    : (mode==MD_BATTLE && !st_ko) ? GAP_BATTLE : GAP_OTHER);
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
/* ── 심판(쿠로코) 초상 ────────────────────────────────────────────
   선택 화면 아이콘 32x32. 스크롤2(밑그림) 위에 스크롤1(색 입히는 겹)을 얹어 그린다.
   윗겹은 타일 두 칸을 다른 그림과 **공유**해서 순서가 연속이 아니다.
   숫자는 실기 롬에서 CharacterRAM·ScrollVRAM·ColorPaletteRAM 을 떠서 뜬 것이다.
   그림은 배포물에 없다 — 사용자 롬에서 실행 중에 그린다. */
static const unsigned KUROKO_BG[16] = {          /* 밑겹: 394187 + 16k */
  394187,394203,394219,394235, 394251,394267,394283,394299,
  394315,394331,394347,394363, 394379,394395,394411,394427 };
static const unsigned KUROKO_FG[16] = {          /* 윗겹 (2번째·15번째가 공유 타일) */
  397883,394203,397899,397915, 397931,397947,397963,397979,
  397995,398011,398027,398043, 398059,398075,394411,398091 };
static const unsigned short KUROKO_PAL_FG[4] = {0x000,0x660,0xA95,0xFFF};
static const unsigned short KUROKO_PAL_BG[4] = {0x000,0x49D,0xADF,0xFFF};
#ifdef SS2COMM_TEST
#define REFSTATIC
#else
#define REFSTATIC static
#endif
REFSTATIC uint16_t      ref_px[32*32];
REFSTATIC unsigned char ref_a[32*32];
REFSTATIC unsigned char ref_ok;

/* 화자 초상을 **32x32 선택화면 아이콘**으로 올린다. HUD 얼굴(16x16)보다 훨씬 잘 보인다.
   띠 높이가 32라 딱 맞는다. 못 그리면(다른 롬·주소 안 맞음) 예전 16x16 으로 떨어진다. */
static const ss2icon ICON[SS2COMM_SPK_N] = SS2COMM_ICON_INIT;
static uint16_t      icon_px[SS2COMM_SPK_N][32*32];
static unsigned char icon_a[SS2COMM_SPK_N][32*32];
static unsigned char icon_ok[SS2COMM_SPK_N];

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
/* 32x32 두 겹을 한 장으로 굽는다. 밑겹은 색0까지 다 칠하고, 윗겹은 색0 을 비운다.
   따로 체크섬이 없어서 **HUD 얼굴 15개가 맞은 롬인지**로 대신 확인한다 —
   그게 맞으면 같은 빌드이고, 이 오프셋도 같이 맞는다. */
static void build_ref_face(void){
  int k, ty, tx, okAny = 0, i;
  ref_ok = 0;
  memset(ref_a, 0, sizeof ref_a);
  if(!cm_rom) return;
  for(i=0;i<SS2COMM_SPK_N;i++) if(face_ok[i]) okAny++;
  if(okAny < SS2COMM_SPK_N) return;          /* 다른 롬 — 심판 얼굴은 생략 */
  for(k=0;k<16;k++){
    unsigned ob = KUROKO_BG[k], of = KUROKO_FG[k];
    int ox = (k & 3) * 8, oy = (k >> 2) * 8;
    if(ob + 16 > cm_romlen || of + 16 > cm_romlen) return;
    for(ty=0; ty<8; ty++){
      unsigned wb = cm_rom[ob+ty*2] | (cm_rom[ob+ty*2+1]<<8);
      unsigned wf = cm_rom[of+ty*2] | (cm_rom[of+ty*2+1]<<8);
      for(tx=0; tx<8; tx++){
        int cb = (wb >> ((7-tx)*2)) & 3;
        int cf = (wf >> ((7-tx)*2)) & 3;
        int p  = (oy+ty)*32 + (ox+tx);
        if(cf){ ref_px[p] = pal12_to565(KUROKO_PAL_FG[cf]); ref_a[p] = 1; }
        /* 심판만은 흰 바탕을 뺀다(사용자 확정). 15명 초상은 일러라 카드째가 맞지만,
           쿠로코는 검은 심판 칸에 실루엣으로 앉는 쪽이 어울린다. */
        else if(cb && cb != 3){ ref_px[p] = pal12_to565(KUROKO_PAL_BG[cb]); ref_a[p] = 1; }
        else ref_a[p] = 0;
      }
    }
  }
  ref_ok = 1;
}

/* 아이콘 굽기 — 밑겹 16타일을 깔고, 배치표대로 윗겹을 얹는다(색0 은 투명).
   배치는 추측이 아니라 실기 렌더와 롬을 맞춰 240칸을 전부 풀어낸 것이다. */
static void build_icons(void){
  int i, k, ty, tx;
  memset(icon_ok, 0, sizeof icon_ok);
  if(!cm_rom) return;
  for(i=0;i<SS2COMM_SPK_N;i++){
    unsigned bg = ICON[i].bg;
    if(!bg || bg + 256 > cm_romlen) continue;

    for(k=0;k<16;k++){
      unsigned ob = bg + k*16;
      int ox = (k & 3)*8, oy = (k >> 2)*8;
      for(ty=0; ty<8; ty++){
        unsigned wb = cm_rom[ob+ty*2] | (cm_rom[ob+ty*2+1]<<8);
        for(tx=0; tx<8; tx++){
          int cb = (wb >> ((7-tx)*2)) & 3;
          int p  = (oy+ty)*32 + (ox+tx);
          /* 흰 바탕(색3)도 그대로 둔다. 원본 일러가 흰 카드 위에 그려진 그림이라,
             흰색을 빼면 얼굴 주변이 뻥 뚫려 어색하다(실사용 제보). 카드째 네모로 얹는다. */
          if(cb){ icon_px[i][p] = pal12_to565(ICON[i].palB[cb]); icon_a[i][p] = 1; }
          else icon_a[i][p] = 0;
        }
      }
    }
    for(k=0;k<16;k++){
      unsigned of = ICON[i].fg[k];
      int ox = (k & 3)*8, oy = (k >> 2)*8;
      if(!of || of + 16 > cm_romlen) continue;   /* 이 칸은 윗겹이 없다 */
      for(ty=0; ty<8; ty++){
        unsigned wf = cm_rom[of+ty*2] | (cm_rom[of+ty*2+1]<<8);
        for(tx=0; tx<8; tx++){
          int cf = (wf >> ((7-tx)*2)) & 3;
          int p  = (oy+ty)*32 + (ox+tx);
          if(cf){ icon_px[i][p] = pal12_to565(ICON[i].palF[cf]); icon_a[i][p] = 1; }
        }
      }
    }
    icon_ok[i] = 1;
  }
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
  build_ref_face();
  build_icons();
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
/* 32 인 이유: 심판 쿠로코 초상이 **32x32 선택화면 아이콘**뿐이기 때문이다.
   HUD 얼굴(16x16)은 388519~390375 에 64바이트 × 30칸 = 15캐릭터 × 2유파로 꽉 차 있고,
   유파가 없는 쿠로코는 그 배열에 자리가 아예 없다. 16x16 으로 줄이거나 잘라 봤으나
   후드 실루엣이 통째로 있어야 알아볼 수 있어 둘 다 못 쓴다(청록 덩어리가 된다). */
#define SS2_BAND_H 32
/* 심판은 **화면 아래**에 따로 선다. 해설창(위)과 자리를 나누면 서로 밀어내지 않는다.
   32 인 이유는 위 띠와 같다 — 쿠로코 초상이 32x32 뿐이라서. */
#define SS2_REF_H  32
#define REF_TTL    180                        /* 3초 */
#define CM_TTL     150
#define COL_WHITE  0xFFFF
#define COL_GOLD   0xFEA0
#define COL_REF    0x9E7F   /* 심판 — 해설자와 구분되는 찬 색 */
#define BOX_LINE_H 13
#define BOX_MAXL   3
int ss2comm_band_h(void){ return (cm_on && (cm_draw==1 || cm_draw==4)) ? SS2_BAND_H : 0; }
int ss2comm_ref_h(void){ return 0; }  /* 심판은 이제 게임 화면 위 오버레이 — 제 자리를 차지하지 않는다 */
int ss2comm_ref_overlay(void);        /* 아래에 — 이번 프레임에 오버레이가 실제로 그려졌으면 그 높이 */
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

/* 강조 순간의 흔들기 — 처음 여섯 프레임 좌우로 튄다.
   게임 화면은 건드리지 않는다. 띠 안의 글자와 얼굴만 흔든다. */
static const signed char SS2_SHAKE[6] = { 2, -2, 1, -1, 1, 0 };

/* 지금 보여 주는 줄이 강조인가 — 진동처럼 그리기 밖에서 쓰려고 열어 둔다 */
int ss2comm_impact(void){ return (cur_ev>=0 && cur_ev<EV_N) ? EVHIT[cur_ev] : 0; }

/* ── 심판 칸 ─────────────────────────────────────────────────────
   전용 자리를 차지하지 않는다. **게임 화면 맨 아래 32줄에 오버레이**로 얹는다 —
   대사가 서 있는 동안만 검은 상자가 뜨고, 끝나면 게임이 그대로 보인다.
   (예전에는 해설창 아래 별도 칸이었다. 제보: 「추가 대화 공간 날려줘」) */
static int ref_drawn_now;                        /* 이번 프레임에 실제로 그렸나 — 앱 32비트 경로가 묻는다 */
static void draw_ref_strip(uint16_t *fb, int pitch_px, int w, int h, int bandTop){
  const char *seg[BOX_MAXL+1], *t, *end;
  int x, y, top, bot, tx0, x1, maxw, nl, i, lh, ty, show;
  ref_drawn_now = 0;
  top = (bandTop ? SS2_BAND_H : 0) + h - SS2_REF_H;  /* 게임 자리의 마지막 32줄 */
  bot = top + SS2_REF_H;
  if(!ref_has && !ref_shown) return;
  if(ref_has){                                   /* 이번 프레임에 세운다 */
    if(cm_f < ref_next) return;
    ref_has = 0; ref_shown = cm_f; ref_next = cm_f + 150;
  }
  if(cm_f - ref_shown > REF_TTL){ ref_shown = 0; return; }
  t = ref_text; if(!*t) return;
  ref_drawn_now = 1;
  for(y=top; y<bot; y++) for(x=0; x<w; x++) fb[y*pitch_px+x] = 0x0000;
  end = t + strlen(t);
  if(ref_ok){                                    /* 쿠로코 초상 32x32 */
    int fx=2, fy=top, a, b;
    for(b=0;b<32;b++) for(a=0;a<32;a++){
      int px=fx+a, py=fy+b;
      if(!ref_a[b*32+a]) continue;
      if(px<0||px>=w||py<top||py>=bot) continue;
      fb[py*pitch_px+px] = ref_px[b*32+a];
    }
  }
  tx0  = ref_ok ? 37 : 4;
  x1   = w - 3;
  maxw = x1 - tx0 - 4;
  nl   = wrap11(t, end, maxw, seg);
  lh   = BOX_LINE_H;
  if(nl > 2){ nl = wrap8(t, end, maxw, seg); lh = 9; }
  ty   = top + (SS2_REF_H - nl*lh)/2;
  show = 2 + (int)(cm_f - ref_shown)*2;          /* 위 띠와 같은 타자 연출 */
  for(i=0;i<nl && show>0;i++){
    int drawn = (lh==9)
      ? draw_line  (fb,pitch_px,tx0,x1,seg[i],seg[i+1], ty + i*lh + 1, top, bot, show, COL_REF, 0)
      : draw_line11(fb,pitch_px,tx0,x1,seg[i],seg[i+1], ty + i*lh,     top, bot, show, COL_REF, 0);
    show -= drawn;
  }
}

/* 이번 프레임에 심판 오버레이가 그려졌으면 높이(32)를 돌려준다.
   32비트 화면 경로가 이걸 보고 게임 자리 맨 아래 32줄만 다시 변환한다. */
int ss2comm_ref_overlay(void){ return ref_drawn_now ? SS2_REF_H : 0; }

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
  /* 초상 굽기가 먼저다. 심판 칸을 먼저 그리면 **첫 프레임에 쿠로코 얼굴이 빈다** */
  if(!face_built) build_faces();
  if(band) draw_ref_strip(fb, pitch_px, w, h, bandTop);   /* 심판 오버레이 — 대사가 서 있을 때만 그린다 */
  if(!line || age > CM_TTL) return;            /* 2.5초만 표시 */

  end  = line + strlen(line);
  mood = (cur_ev>=0 && cur_ev<EV_N) ? EVMOOD[cur_ev] : 0;
  hit  = (cur_ev>=0 && cur_ev<EV_N) ? EVHIT[cur_ev]  : 0;
  col  = hit ? COL_GOLD : COL_WHITE;
  spk = (cur_spk >= 0 && cur_spk < SS2COMM_SPK_N) ? cur_spk : cm_spk;
  if(!face_built) build_faces();
  show = 2 + (int)age*2;                       /* 타자 연출: 프레임당 두 글자 */

  tx0  = icon_ok[spk] ? 37 : (face_ok[spk] ? 21 : 4);
  x1   = w - 3;
  /* 글자 상자는 12px 인데 자간(adv11)은 8px 다. 그래서 마지막 글자는 자기 자간보다
     **4px 더 오른쪽까지 그린다.** 폭을 자간으로만 재서 딱 맞추면 그 4px 이 잘려
     「…지은 것이다」가 「…것이ㄷ」로 나왔다. 그만큼을 미리 뺀다. */
  maxw = x1 - tx0 - 4;
  { int shx = (hit && age < 6) ? SS2_SHAKE[age] : 0;   /* 강조면 좌우로 튄다 */
    tx0 += shx; }

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

  /* 바탕 — 띠는 검정, 화면 안 상자는 게임 화면을 1/16 로 눌러 깐다.
     강조면 **첫 프레임에 금색으로 확 채우고** 두 프레임에 걸쳐 식힌다.
     16ms 짜리 번쩍임이라 글자를 가려도 상관없다 — 터지는 느낌이 목적이다. */
  for(y=top; y<bot; y++)
    for(x=0; x<w; x++){
      uint16_t c = fb[y*pitch_px+x];
      uint16_t base = band ? 0x0000 : (uint16_t)((c>>4)&0x0861);
      uint16_t v;
      if     (hit && age < 1) v = COL_GOLD;
      else if(hit && age < 2) v = (uint16_t)(0x8C40 | base);
      else if(hit && age < 5) v = (uint16_t)(0x3000 | base);
      else                    v = base;
      fb[y*pitch_px+x] = v;
    }
  /* 게임 화면과 맞닿는 쪽에 경계선 — 새 대사면 하얗게 튄다 */
  { uint16_t bc = (age<6) ? COL_WHITE : (hit ? COL_GOLD : (band ? 0x39E7 : 0x52AA));
    int by = (cm_draw==2 || cm_draw==4) ? (bot-1) : top;
    for(x=0; x<w; x++) fb[by*pitch_px+x] = bc;
  }
  /* 얼굴 — 32x32 아이콘이 구워졌으면 그걸, 아니면 예전 16x16 */
  if(icon_ok[spk]){
    int fx=2, fy=top, a, b;
    if(hit && age < 6) fx += SS2_SHAKE[age];
    for(b=0;b<32;b++) for(a=0;a<32;a++){
      int px=fx+a, py=fy+b;
      if(!icon_a[spk][b*32+a]) continue;
      if(px<0||px>=w||py<top||py>=bot) continue;
      fb[py*pitch_px+px] = tint(icon_px[spk][b*32+a], mood);
    }
  }
  else if(face_ok[spk]){
    int fx=3, fy=top + (boxh-16)/2, a, b;
    if(hit && age < 6) fx += SS2_SHAKE[age];          /* 얼굴도 같이 흔든다 */
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
