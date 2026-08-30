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
#include <stdlib.h>   /* SS2COMM_DBGSEQ 진단용 getenv */
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
#define OFF_JING   0x0000   /* 징글 명령. 라운드 인트로에서 2 가 되는 프레임 = 게임 「승부!」 글이 서는 프레임.
                               세이브스테이트 두 주행(무입력 +494 · 연타 +190)에서 정확히 일치 */
#define OFF_OPPID  0x17DF   /* 상대 **개체 번호** — 정규 15명은 로스터 번호 그대로.
                               보스전에서 BLK2 는 잔존값이라 겐주로(0x30)·아수라(0x88)로
                               오인했다(실기 재현: 간다라전에 「겐주로냐…!」). 실측:
                               간다라=8, 그림자 보스=19, 유가=14 */
#define OFF_BOSS   0x17E3   /* 보스전 플래그 — 정규 스테이지 0, 간다라/그림자/유가전 1 */
#define OFF_SEQTXT 0x17D1   /* 인트로 글 연출 카운터. 「자아」(15부터)·「N회전」(33부터)이 서기 4~10프레임
                               전에 돌기 시작한다 — 연타로 인트로가 줄어도 그대로 따라간다 */
#define ACT_HEAVY(a) ((a) >= 0x60 && (a) <= 0x6F)   /* 강베기 액션대 — 실측 0x60/0x64/0x68, 피해 30 */
#define MD_BATTLE  241
#define MD_MENU    240
#define MD_QUOTE   197
#define MD_ENDING  199



/* 음성 팩(ss2voice.c) — 코어에서만 강한 구현이 링크된다. 하네스(audit/test_flow 등)는
   이 약한 무동작이 그대로 남아 링크 걱정이 없다. */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak)) void ss2voice_say(const char *t, int p){ (void)t; (void)p; }
__attribute__((weak)) int  ss2voice_on(void){ return 0; }
__attribute__((weak)) int  ss2voice_has_text(const char *t){ (void)t; return 0; }
__attribute__((weak)) void ss2voice_say_parts(const char *a, const char *b, const char *c, int p){ (void)a;(void)b;(void)c;(void)p; }
__attribute__((weak)) int  ss2voice_playing_prio(void){ return -1; }
#else
void ss2voice_say(const char *t, int p);
int  ss2voice_on(void);
int  ss2voice_has_text(const char *t);
void ss2voice_say_parts(const char *a, const char *b, const char *c, int p);
int  ss2voice_playing_prio(void);
#endif

#include "ss2comm_lines.h"
#include "ss2comm_icon.h"
#include "ss2comm_art.h"

static const char *CHARNAME[15] = {
  "카즈키","소게츠","하오마루","겐주로","나코루루","리무루루","한조","갈포드",
  "아수라","샤를로트","모로즈미","우쿄","쥬베이","시키","유가"
};
/* 게임 로스터 번호(위 순서) → 화자/아이콘 번호(하오마루 0…). 아이콘을 로스터
   번호로 그리면 카즈키 그림에 샤를로트 이름이 붙는다 — 미리보기 렌더로 잡은 버그. */
static const signed char ROST2SPK[15] = {9,10,0,5,1,4,2,3,11,7,13,6,8,12,14};

/* ── 쿨다운 키 — 브라우저판 commEmit 의 키를 그대로 옮겼다.
      같은 키를 쓰는 이벤트는 쿨다운을 나눠 쓴다(예: ko/koed/dko/moveKo = "ko"). */
enum { CK_ROUND, CK_ROUNDCTX, CK_KO, CK_MV, CK_HIT, CK_TK, CK_DN, CK_DN2, CK_OSP,
       CK_LOW, CK_LOW2X, CK_REV, CK_PFT, CK_CBK, CK_QKO, CK_RVG, CK_STK,
       CK_SURV, CK_SURV2, CK_STG, CK_QUOTE, CK_ENDING, CK_RES, CK_RES2, CK_REC,
       CK_VSQ, CK_STORY, CK_SCR0, CK_SCR2, CK_SCR4, CK_SCR6, CK_SELCHAT, CK_MIDLE,
       CK_FB, CK_IDLE, CK_LONG, CK_MUSE, CK_LORE, CK_REL,
       CK_FLOW, CK_ARC, CK_PCH, CK_PJD, CK_N };

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
  /* v3 플레이어 축 — 응원은 드물게(15초), 평가는 이따금(10초) */
  [EV_PCHEER     ] = { CK_PCH, 900 },
  [EV_PJUDGE     ] = { CK_PJD, 600 },
};

/* ── 상태 ── */
static int  cm_on = 1, cm_spk = 0;
static unsigned cm_f = 0;                 /* 프레임 카운터 */
static unsigned cd[CK_N];                 /* 쿨다운 만료 프레임 */
static int p_mode=-1, p_scr=-1, p_hp1=-1, p_hp2=-1, p_a1=0, p_a2=0, p_surv=0, p_stage=0;
static int p_jing=-1, p_seqtxt=-1;   /* 징글·인트로 글 카운터의 지난 값 — 마커 에지 검출용 */
static int st_ko, st_low1, st_low2, st_lead, st_rev;
static int st_won, st_lost, st_resultDone;
static int st_myR, st_opR, st_roundN, st_fb, st_longSaid, st_dblLow;
static int st_lastStage = -1;
static unsigned st_roundStart, st_offAt, st_actAt, st_menuAt, st_selChatAt, st_hitAt;
static int st_selChatN;
static int st_myChar = -1, st_oppChar = -1;
static unsigned lore_at;         /* 마지막 서사(관계·야사) 온에어 프레임 */
static unsigned loresaid_h[24];  /* 이번 매치에 이미 푼 서사 — 같은 썰 재탕 컷(제보: 반복 검수) */
static int      loresaid_n;
static unsigned fmtsaid_h[48];   /* 이번 매치에 이미 쓴 문형 — 변형이 남아 있으면 겹치지 않게 뽑는다 */
static int      fmtsaid_n;
static unsigned st_lastFightF;   /* 진짜 격투(F1+scr8)를 마지막으로 본 프레임 — 새 매치 판정용.
                                    문구·스토리 화면도 mode 는 F1 이라 st_offAt 으로는
                                    「방금 전투에서 나옴」과 「스토리 읽는 중」을 못 가른다 */
static unsigned last_line_f = 0;          /* 마지막 발화 프레임 */
static unsigned last_input_f = 0;
static unsigned hush_until = 0;           /* 결과 멘트 뒤 잡담을 잠그는 시점 */         /* 마지막 패드 입력 프레임 (자동 전환 판별용) */
/* 세션 기록 — 코어가 살아 있는 동안만 (저장 안 함) */
static int sess_wins, sess_games, sess_streak, sess_survBest, sess_lastLossChar = -1;
static int st_fHp1, st_fHp2;   /* KO 전 마지막으로 본 두 체력 — 타임오버 라운드 판정용 */
static int st_settled;         /* 이번 매치의 승패 정산(연승·전적)을 이미 했나 */
static int blk_boot = -1, blk_moved;   /* 부팅 뒤 BLK1 이 한 번이라도 움직였나 — 기본값 썰 도배 방지 */
static int st_oppGand;   /* 지금 상대가 간다라인가 — 이름은 「간다라」로 부른다(제보: 게임 명패는 수라라 떠도) */
static unsigned char st_shk[2];   /* 히트 충격 잔량(프레임) — 맞은 쪽 기둥이 흔들린다. [0]=나 [1]=상대 */
static int st_survSaid = -1, st_streakSaid = -1;   /* 이미 낭독한 연승 값 — 같은 값 반복 금지 */
static int surv_seen = -1, surv_live;   /* 무한대전 카운터가 이번 세션에 실제로 움직였나 —
                                           스토리는 이 값을 안 지워서(실기 검증) 잔존 15가 판마다 읽혔다 */
/* KO 없이 끝난 라운드(타임오버) 정산 — 체력바가 안 비면 KO 갈래가 라운드를 못 세고,
   그러면 판세 안내도 연승도 다 어긋난다 (제보: 「연승 카운트가 끊기지 않는다」).
   마지막으로 본 체력으로 그 판의 주인을 정한다. 스냅샷은 한 번 쓰면 비운다 —
   결과 화면과 다음 판 진입, 두 자리에서 불려도 두 번 세지 않게. */
static void tk_round(void){
  int h1 = st_fHp1, h2 = st_fHp2;
  st_fHp1 = st_fHp2 = 0;
  if(st_ko || h1 == h2) return;
  if(h1 > h2){
    st_myR++; st_won = 1; st_lost = 0;
    if(st_myR >= 2 && !st_settled){ sess_streak++; sess_wins++; sess_games++; st_settled = 1; }
  }else{
    st_opR++; st_lost = 1; st_won = 0;
    if(st_opR >= 2 && !st_settled){ sess_streak = 0; sess_lastLossChar = st_oppChar; sess_games++; st_settled = 1; }
  }
  st_ko = 1;
}
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
/* 큐에 실렸다가 밀려나거나 삭아 버린 흐름 라인은 「말했다」 깃발을 되돌린다 —
   임계는 여전히 넘어 있으니 다음 재시도 때 다시 실을 수 있게. (도배 방지는
   깃발+쿨다운이 계속 지키고, 이건 증발 방지만 맡는다) */
static void flow_unsay(int ev){
  if(ev==EV_FLOWTRADE) fl_said&=(unsigned char)~FLS_TRADE;
  else if(ev==EV_FLOWONE) fl_said&=(unsigned char)~FLS_ONE;
  else if(ev==EV_FLOWCHASE) fl_said&=(unsigned char)~FLS_CHASE;
}
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
typedef struct { char text[160]; short ev; short spk; unsigned at;
                 char vkn[56], vks[112];   /* 이어붙이기 조각키(합성) — 비면 통짜/자막 */
                 char vkh[96];             /* 머리 조각키 — 이름이 문중인 줄에만 */
               } ss2q;
static ss2q q[QN];
static int q_head, q_cnt;
static unsigned q_next;

/* 심판 전용 칸 — 해설 대기열과 따로 선다 */
static unsigned char anec_at[15], weap_at[15];  /* 썰·무기 소회를 어디까지 풀었나 */
static unsigned ref_next;                       /* 심판끼리의 최소 간격 */
static unsigned ref_shown;                      /* 아래 칸에 세운 시각 (0 = 아직) */
static char     ref_text[160];
static int ref_enabled = 1;       /* 심판 온오프 — 끄면 쿠로코가 아예 안 선다 (설정에서 토글) */
static unsigned char ref_flash;   /* 반짝이 — 「승부!」「한 판!」 같은 구령은 게임 연출처럼 깜빡인다 */
static int ref_ttl;               /* 이 줄의 수명 — 구령은 짧게 */
static unsigned char intro_pending;    /* 인트로 진입 때 상대 미탑재 — 인트로 동안 재확인한다
                                          (제보: 첫 대전(스토리)에서 쿠로코 구령·자막이 통째로 실종) */
static unsigned char intro_nw;         /* 그 인트로가 새 매치였나 — 지각 대진 콜용 */
static unsigned char intro_beat1_done; /* 「자아 — 정정당당히!」 (연출 카운터 시동값 15) */
static unsigned char intro_beat2_done; /* 「N회전!」 (시동값 33 — 2·3회전 인트로는 이것만 온다) */
static unsigned char intro_shout_done; /* 이 인트로에서 「승부!」를 이미 외쳤나 (징글 + F1 이중 발성 방지) */
static int      intro_roundN;     /* 그때 부를 판 번호 (게임 표기와 같은 N회전) */
static unsigned char intro_refok; /* 이 판에 심판이 서나 (인트로 진입 때 판정) */
static unsigned plate2_at;        /* 팻말 둘째 마디 「훌륭하오!」 시각 (이름…! 다음) */
static unsigned char ref_thump_pend;   /* 구령이 선 프레임 — 앱이 진동 한 번으로 받아 간다 */
static unsigned char anecv_used[15];   /* 주제별로 화자 목소리 썰 첫 마디를 냈나 */
static int chat_enabled = 1;      /* 캐릭터챗 온오프 — 쿠로코와 따로 끈다 (셋 다 조합 가능) */
static unsigned plate_at;         /* 승자 팻말 호명 시각 (0 = 없음) */
static int      plate_char;       /* 팻말에 오를 승자 */

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
/* 서사(관계·야사)는 30초 창으로 부족하다 — 한 매치가 몇 분이라 같은 썰이 또 풀린다.
   매치 단위로 기억한다. 24를 넘으면 그냥 안 적는다(한 판에 그만큼 나올 일 없음). */
static int lore_said_match(const char *s){
  unsigned h = line_hash(s); int i;
  for(i = 0; i < loresaid_n; i++) if(loresaid_h[i] == h) return 1;
  return 0;
}
static void lore_mark_match(const char *s){
  if(loresaid_n < 24) loresaid_h[loresaid_n++] = line_hash(s);
}
static int fmt_said_match(const char *s){
  unsigned h = line_hash(s); int i;
  for(i = 0; i < fmtsaid_n; i++) if(fmtsaid_h[i] == h) return 1;
  return 0;
}
static void fmt_mark_match(const char *s){
  if(fmtsaid_n < 48) fmtsaid_h[fmtsaid_n++] = line_hash(s);
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
/* 결과 계열은 안무가 길어도 살린다 — 결과 화면에서 하는 말이라 늦은 게 아니다.
   순간 반응(KO·완승 따위)만 6초 컷에 걸린다. */
static int ev_resultish(int ev){
  switch(ev){
    case EV_WINSCR: case EV_LOSESCR: case EV_WINTALK: case EV_LOSETALK: case EV_RECORD:
    case EV_ARCSWEEP: case EV_ARCSWEPT: case EV_ARCCOMEBACK: case EV_ARCSWEAT:
    case EV_ARCCHOKE: case EV_ARCSLIP: case EV_REL: case EV_LORE:
      return 1;
    default: return 0;
  }
}
static int ev_keep(int ev){
  switch(ev){
    case EV_REL: case EV_LORE: case EV_CHARSELCHAT: case EV_START:
    case EV_VSQ: case EV_STORYCHAT: case EV_MENUIDLE:
    case EV_MUSE_B: case EV_MUSE_Q: case EV_MUSE_M: case EV_IDLE:
    /* 라운드 맥락(「한 판 챙겼군」 「최종 라운드다」)도 후순위다. 심판과는 자리가
       갈렸지만 **KO·총평과는 여전히 다툰다** — 되돌려 보니 검사 셋이 다시 깨졌다.
       이쪽은 놓쳐도 다음 판에 또 올 말이고, KO 반응은 그 순간뿐이다. */
    case EV_ROUND: case EV_ROUNDLEAD: case EV_ROUNDBEHIND: case EV_MATCHPOINT:
    case EV_PCHEER: case EV_PJUDGE:
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
  int n = SS2COMM_SPK_N, guard = 0;
  /* v4 로스터 11인 — 재캐스팅에서 빠진 화자는 순환에서 건너뛴다(대사표는 남겨둔다) */
  do {
    cm_spk = ((cm_spk + (step?step:1)) % n + n) % n;
  } while((cm_spk==7 || cm_spk==10 || cm_spk==13 || cm_spk==14) && ++guard < n);
  speaker_switched();
  return cm_spk;
}
const char *ss2comm_speaker_hello(int idx){
  return (idx>=0 && idx<SS2COMM_SPK_N) ? HELLO[idx] : "";
}
void ss2comm_reset(void){
  int i; for(i=0;i<CK_N;i++) cd[i]=0;
  /* 시드가 고정이면 켤 때마다 첫 마디가 똑같다(검수에서 확인) — 램 내용을 섞는다 */
  if(RAM){ unsigned s=0x9E3779B9u; for(i=0;i<4096;i+=7) s = s*33u + RAM[i]; rng ^= s | 1u; }
  p_mode=p_scr=-1; p_hp1=p_hp2=-1; p_a1=p_a2=0; p_surv=p_stage=0;
  st_ko=st_low1=st_low2=st_lead=st_rev=0;
  st_won=st_lost=st_resultDone=0;
  loresaid_n=0; fmtsaid_n=0;
  st_myR=st_opR=0; st_roundN=1; st_fb=st_longSaid=st_dblLow=0;
  st_fHp1=st_fHp2=0; st_settled=0;
  blk_boot=-1; blk_moved=0; st_survSaid=-1; st_streakSaid=-1; surv_seen=-1; surv_live=0; st_oppGand=0;
  st_shk[0]=st_shk[1]=0;
  memset(anec_at,0,sizeof anec_at); memset(weap_at,0,sizeof weap_at);
  memset(anecv_used, 0, sizeof anecv_used);
  ref_next=0; intro_pending=0; intro_nw=0; intro_beat1_done=intro_beat2_done=0; intro_shout_done=0; plate_at=0; plate2_at=0; plate_char=-1; ref_flash=0; ref_ttl=180; st_lastFightF=0; ref_thump_pend=0;
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

/* ── 심판 안무 예약 ──────────────────────────────────────────────
   진짜 게임 연출을 코어로 돌려 프레임 단위로 쟀다(ss2.ngc, 무입력):
     · 라운드 인트로 = mode F0 + scr 8. 새 매치 인트로는 518프레임 —
       「자아 정정당당히」가 인트로+326, 「N회전」+432, 「승부!」+494.
       2·3회전 인트로는 114프레임 — 「N회전」+28, 「승부!」+90.
     · mode 가 F1 로 서는 순간이 실제 조작 시작 = 게임의 「승부!」 직후.
     · KO 뒤 게임의 「한판!!」은 KO+151, 승자 팻말은 KO+391.
   그래서 호명은 인트로 진입에, 「정정당당히 — N회전!」은 게임의 자아/회전 글에,
   「승부!」는 전투가 서는 프레임에, 팻말 호명은 KO+390 에 맞춘다. */


/* 승부가 갈리면 승자의 **본명**을 부른다. 갈포드는 성이 없다. */
static const char *const CHARFULL[15] = {
  "카자마 카즈키", "카자마 소게츠", "하오마루", "키바가미 겐주로",
  "나코루루", "리무루루", "핫토리 한조", "갈포드",
  "아수라", "샤를로트 크리스틴 드 콜데", "모로즈미 타이잔", "타치바나 우쿄",
  "야규 쥬베이", "시키", "유가",
};

/* 심판은 **해설 대기열을 쓰지 않는다.** 다른 목소리니 줄도 따로 선다.
   같은 칸을 쓰게 했더니 승부가 갈리는 순간 구호가 총평을 밀어냈다 —
   둘 다 나와야 하는 자리다. 구호가 먼저, 해설이 그 뒤. */
static int dbgseq(void){ static int d=-1; if(d<0){const char*e=getenv("SS2COMM_DBGSEQ"); d=(e&&*e=='1');} return d; }
static void ref_say3(const char *text, int force, int flash, int ttl){
  if(!cm_on || !ref_enabled || !text || !*text) return;
  if(dbgseq()) fprintf(stderr, "[REF f=%u] %s\n", cm_f, text);
  if(!force && said_recently(text)) return;
  snprintf(ref_text, sizeof ref_text, "%s", text);
  ss2voice_say(text, 1);                         /* 심판은 해설을 끊는다 */
  ref_shown = 0;
  ref_at  = cm_f;
  ref_has = 1;
  ref_flash = (unsigned char)flash;
  ref_ttl = ttl;
  if(force) ref_next = cm_f;        /* 안무로 박은 줄은 심판 간격을 안 기다린다 */
}
static void ref_say(const char *text){ ref_say3(text, 0, 0, 180); }
/* 구령: 강제 + 반짝(금빛 버스트·흔들림·진동 한 번) + 1초.
   매 판 반복되는 말이라 「같은 말 금지」도 우회한다.
   제보: 「반짝 = 노랗게 되면서 쿵 하는 효과. 지금은 깜빡이고, 끝나고도 쓸데없이 더 깜빡임」
   — 점멸을 버리고 해설창의 강조 연출(금 배경 번쩍 + 좌우 흔들림)과 같은 문법으로 간다. */
static void ref_shout(const char *text){ ref_say3(text, 1, 1, 60); ref_thump_pend = 1; }
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
/* %m = 내 캐릭터 이름 (v3 플레이어 축). %m 을 %s 로 바꾼 사본을 만들어
   fill_name(조사 보정)을 재사용한다. 내 캐릭터를 모르면 「그대」로 부른다. */
static void fill_name(char *out, size_t cap, const char *fmt, const char *who);
static const char *my_name(void){
  if(st_myChar < 0 || st_myChar >= 15) return "그대";
  if(ROST2SPK[st_myChar] == cm_spk) return "그대";   /* 해설자 본인 캐릭터 — 제 이름을 3인칭으로 부르면 우습다 */
  return CHARNAME[st_myChar];
}
static int fill_me(char *out, size_t cap, const char *fmt){
  char tmp[176]; const char *ph = strstr(fmt, "%m");
  size_t pre;
  if(!ph){ snprintf(out, cap, "%s", fmt); return 1; }
  pre = (size_t)(ph - fmt);
  if(pre >= sizeof tmp - 3) return 0;
  memcpy(tmp, fmt, pre); tmp[pre] = '%'; tmp[pre + 1] = 's';
  snprintf(tmp + pre + 2, sizeof tmp - pre - 2, "%s", ph + 2);
  fill_name(out, cap, tmp, my_name());
  return 1;
}

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

static void fmt_one(const char *fmt, const char *who, int n1, int n2, char *out, size_t cap){
  char mb[176];
  if(strstr(fmt, "%m")){ if(fill_me(mb, sizeof mb, fmt)) fmt = mb; }
  if(strstr(fmt,"%s")) fill_name(out, cap, fmt, who);
  else if(strstr(fmt,"%d")){
    const char *p = strstr(fmt,"%d");
    if(strstr(p+2,"%d")) snprintf(out, cap, fmt, n1, n2);
    else                 snprintf(out, cap, fmt, n1);
  }
  else snprintf(out, cap, "%s", fmt);
}
/* 음성 팩 시대의 대체 이벤트 — 기술명 계열은 변형 전부가 기술명 채움이라 팩에 없을
   수 있다. 그 순간을 침묵으로 두지 말고 같은 뜻의 일반 대사로 갈아탄다(제보:
   「엔진 자체를 더빙에 맞춰라」). 팩이 없으면 이 경로는 아예 안 탄다. */
/* v3 이름 3톤 — 이벤트 성격이 이름 조각의 톤을 고른다.
   0 평서 / 1 외침(승부처) / 2 낮게(경계·위기). 톤 클립 키는 "\x01N<spk>/<T>\x01이름",
   없으면 무톤 키(현행 팩) → 통짜 → 자막 순으로 물러난다. */
static int ev_tone(int ev){
  switch(ev){
    case EV_KO: case EV_KOED: case EV_DKO: case EV_MOVEKO:
    case EV_REVERSAL: case EV_PERFECT: case EV_COMEBACK: case EV_QUICK:
    case EV_MOVEHITL: case EV_MOVEDOWNA:
      return 1;
    case EV_LOW1: case EV_LOW2: case EV_DOUBLELOW: case EV_OPPSP:
    case EV_TAKEN: case EV_DOWNED:
      return 2;
    default: return 0;
  }
}
static int ev_voicefb(int ev){
  switch(ev){
    case EV_MOVEHIT: case EV_MOVEHITL:   return EV_HIT;
    case EV_MOVEDOWN: case EV_MOVEDOWNA: return EV_DOWN;
    case EV_MOVEKO:                      return EV_KO;
    default: return -1;
  }
}
/* 꼬리 조각키: %s 뒤 문자열의 첫 조사를 이름 받침에 맞춰 확정해 합성키로 만든다.
   fill_name 과 같은 규칙 — 둘이 어긋나면 자막과 소리가 갈린다. */
static int splice_suffix(const char *rest, const char *who, char *out, size_t cap){
  int b = kr_batchim(who), i;
  const char *tail = rest; const char *josa = 0;
  if(b >= 0){
    for(i = 0; JOSA[i][0]; i++){
      size_t l0 = strlen(JOSA[i][0]), l1 = strlen(JOSA[i][1]);
      int noB = (b == 0) || (i == 0 && b == 8);
      if(!strncmp(rest, JOSA[i][0], l0)){ josa = JOSA[i][noB ? 0 : 1]; tail = rest + l0; break; }
      if(!strncmp(rest, JOSA[i][1], l1)){ josa = JOSA[i][noB ? 0 : 1]; tail = rest + l1; break; }
    }
  }
  return snprintf(out, cap, "\x01S%d\x01%s%s", cm_spk, josa ? josa : "", tail) < (int)cap;
}
/* 이름 조각키 — 톤 클립 우선, 없으면 무톤(v2 팩). 팩에 있는 키만 성공. */
static int spk_name_key(char *out, size_t cap, int ev, const char *who){
  if(!who || !*who) return 0;
  snprintf(out, cap, "\x01N%d/%d\x01%s", cm_spk, ev_tone(ev), who);
  if(ss2voice_has_text(out)) return 1;
  snprintf(out, cap, "\x01N%d\x01%s", cm_spk, who);
  return ss2voice_has_text(out) ? 1 : 0;
}
/* 머리 조각키 — 이름이 문중인 줄의 앞부분. 꼬리와 같은 S 계열 키(같은 말=같은 소리). */
static int head_key(char *out, size_t cap, const char *fmt, const char *ph){
  size_t hl = (size_t)(ph - fmt);
  if(hl == 0 || hl > 60) return 0;
  return snprintf(out, cap, "\x01S%d\x01%.*s", cm_spk, (int)hl, fmt) < (int)cap;
}
static int emit_ex(int ev, int vsel, int n1, int n2, const char *who){
  const char *cand[EVMAXV]; int n=0, i, key;
  const char *fmt;
  char spl_kn[56] = "", spl_ks[112] = "", spl_kh[96] = ""; int spl_on = 0;
  if(!cm_on || ev<0 || ev>=EV_N) return 0;
  key = EVCD[ev].key;
  if(cd[key] > cm_f) return 0;
  for(i=0;i<EVMAXV;i++) if(LINES[cm_spk][ev][i]) cand[n++]=LINES[cm_spk][ev][i];
  if(!n && (ev == EV_PCHEER || ev == EV_PJUDGE)){
    /* v3 골격 임시 대사 — 표(gen_lines)에 정식 편입 전까지의 공용분. 시대극 문어체. */
    static const char *const PCH[6] = {
      "%m! 물러서지 마라!", "%m, 아직이다 — 검을 다시 세워라!", "버텨라 %m! 승부는 지금부터다!",
      "%m, 검을 믿어라!", "아직이다, %m! 판은 살아 있다!", "%m! 그 정도에 꺾일 검이냐!" };
    static const char *const PJD[6] = {
      "%m의 검이 제대로 섰군!", "좋다 %m, 그 기세로다!", "방금 그 한 수… %m, 훌륭하다!",
      "%m! 지금 그 수, 실로 매섭다!", "보았는가! %m의 검이다!", "%m, 오늘 검이 울고 있구나!" };
    const char *const *tb = (ev == EV_PCHEER) ? PCH : PJD;
    for(i = 0; i < 6; i++) cand[n++] = tb[i];
  }
  if(!n) return 0;
  /* %s 채움용 상대 이름 — 음성 우대 검사에도 필요해서 미리 정한다.
     (이름 인자가 없는 호출(emit)의 %s 는 상대 이름. 표 밖 개체는 「상대」/「간다라」) */
  if(!who) who = (st_oppChar>=0 && st_oppChar<15 && ROST2SPK[st_oppChar]!=cm_spk)
             ? CHARNAME[st_oppChar]
             : (st_oppGand ? "간다라" : "상대");
  if(vsel >= 0) fmt = cand[vsel < n ? vsel : n-1];
  else {
    /* ① 팩이 있으면 「말할 수 있는」 후보 안에서만 고른다. 후보 전부가 무음인
       기술명 계열은 자매 이벤트로 통째 교체(한 번만). ② 그 안에서 기존 규칙 —
       이름 박힌 줄 8/10 우대 — 를 그대로 적용한다. */
    const char *vd[EVMAXV]; int nvd = 0;
    if(ss2voice_on()){
      char tb[160]; int pass;
      for(pass = 0; pass < 2; pass++){
        nvd = 0;
        for(i = 0; i < n; i++){
          fmt_one(cand[i], who, n1, n2, tb, sizeof tb);
          if(ss2voice_has_text(tb)) vd[nvd++] = cand[i];
        }
        if(nvd || pass) break;
        /* 통짜 전멸 — [머리][이름][꼬리] 조각 결합을 시도한다(제보: 「어색해도
           이어붙여라」 + 「문중도 지원해라」). 이름이 문중이면 머리 조각까지 세 조각.
           %m(내 캐릭터)도 같은 길 — 이름 조각을 재사용한다. 그래도 없으면 자매 이벤트. */
        if(who && *who){
          const char *sp2[EVMAXV]; int ns2 = 0;
          char kn_s[56] = "", kn_m[56] = "";
          const char *me2 = my_name();
          spk_name_key(kn_s, sizeof kn_s, ev, who);
          spk_name_key(kn_m, sizeof kn_m, ev, me2);
          for(i = 0; i < n; i++){
            const char *ph = strstr(cand[i], "%s"), *nm2 = who, *kn2 = kn_s;
            char hk[96];
            if(!ph){ ph = strstr(cand[i], "%m"); nm2 = me2; kn2 = kn_m; }
            if(!ph || !kn2[0]) continue;
            if(strchr(cand[i], '%') != ph) continue;      /* %d 혼합 줄은 조각 불가 */
            if(ph != cand[i]){                            /* 문중 — 머리 조각 필요 */
              if(!head_key(hk, sizeof hk, cand[i], ph)) continue;
              if(!ss2voice_has_text(hk)) continue;
            }
            if(!splice_suffix(ph + 2, nm2, tb, sizeof tb)) continue;
            if(ss2voice_has_text(tb)) sp2[ns2++] = cand[i];
          }
          if(ns2){
            const char *ph, *nm2;
            fmt = sp2[rnd()%(unsigned)ns2];
            ph = strstr(fmt, "%s"); nm2 = who;
            if(!ph){ ph = strstr(fmt, "%m"); nm2 = me2; }
            snprintf(spl_kn, sizeof spl_kn, "%s", nm2 == who ? kn_s : kn_m);
            if(ph != fmt) head_key(spl_kh, sizeof spl_kh, fmt, ph);
            splice_suffix(ph + 2, nm2, spl_ks, sizeof spl_ks);
            spl_on = 1;
            goto picked;
          }
        }
        { int fb = ev_voicefb(ev);
          if(fb < 0) break;
          ev = fb; key = EVCD[ev].key;
          n = 0;
          for(i = 0; i < EVMAXV; i++) if(LINES[cm_spk][ev][i]) cand[n++] = LINES[cm_spk][ev][i];
          if(!n) return 0;
        }
      }
    }
    { const char *const *cs0 = nvd ? vd : cand; int cn0 = nvd ? nvd : n;
      /* 이번 판에 이미 쓴 문형은 걸러서 뽑는다 — 변형이 6개라도 복원추출이면
         서너 번 만에 절반이 겹친다(시뮬 실증). 변형을 다 썼으면 그때는 허용. */
      const char *cs[EVMAXV]; int cn = 0;
      const char *nm[EVMAXV]; int nn = 0;
      for(i=0;i<cn0;i++) if(!fmt_said_match(cs0[i])) cs[cn++]=cs0[i];
      if(!cn){
        if(ev_prio(ev) == 0) return 0;  /* 잔반응은 변형이 다 떨어지면 침묵이 낫다(반복 검수) */
        for(i=0;i<cn0;i++) cs[cn++]=cs0[i];
      }
      for(i=0;i<cn;i++) if(strstr(cs[i],"%s")) nm[nn++]=cs[i];
      if(nn && nn<cn && (rnd()%10u)<4) fmt = nm[rnd()%(unsigned)nn];
      else                             fmt = cs[rnd()%(unsigned)cn];
    }
  }
picked:
  fmt_one(fmt, who, n1, n2, outbuf, sizeof(outbuf));
  if(said_recently(outbuf)) return 0;      /* 최근에 한 말은 다시 안 한다 */
  if(ev == EV_REL || ev == EV_LORE){
    if(lore_said_match(outbuf)) return 0;  /* 같은 썰은 같은 매치에서 한 번만 */
    lore_mark_match(outbuf);
  }
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
      flow_unsay(q[worst].ev);            /* 밀려나는 흐름 라인은 재시도 대상으로 되돌린다 */
      slot = worst;
    }
    snprintf(q[slot].text,sizeof(q[slot].text),"%s",outbuf);
    q[slot].at = cm_f;
    q[slot].ev = (short)ev;
    q[slot].spk = (short)cm_spk;
    q[slot].vkn[0] = q[slot].vks[0] = q[slot].vkh[0] = 0;
    if(spl_on){ snprintf(q[slot].vkn,sizeof q[slot].vkn,"%s",spl_kn);
                snprintf(q[slot].vks,sizeof q[slot].vks,"%s",spl_ks);
                snprintf(q[slot].vkh,sizeof q[slot].vkh,"%s",spl_kh); }
  }
  if(fmt) fmt_mark_match(fmt);      /* 실린 문형만 — 이번 판에는 같은 꼴을 다시 뽑지 않는다 */
  cd[key] = cm_f + EVCD[ev].cool;   /* 실린 다음에만 — 큐에서 밀려난 말이 쿨다운까지 먹으면 재시도가 막힌다 */
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
/* 설명 대사(썰·무기 소회)에 주어가 없으면 누구 얘기인지 모른다
   (제보: 「캐릭터 설명할 때도 누구 설명인지 명사를 넣어야지 툭 내뱉고 치운다」).
   문장에 그 캐릭터 이름이 이미 있으면 그대로, 없으면 「이름 — 」을 앞에 박는다. */
/* 뱅크 썰은 위키체(「…를 베었다」)라 화자 말투와 어긋난다(제보: 「말투 싱크가 안 맞는다」).
   「…다」로 끝나는 문장은 화자 말꼬리를 붙여 전언(들은 이야기)으로 바꾼다 —
   「베었다」→「베었다고 하더군요」. 사실은 그대로, 전달만 화자 것. */
static const char *const SPK_TAIL[SS2COMM_SPK_N] = {
  "고 하더군",      /* 하오마루 */  "고 해요",        /* 나코루루 */
  "고 들었다",      /* 한조 */      "고 들었다!",     /* 갈포드 */
  "고 해",          /* 리무루루 */  "고 하더군. 쿠쿡",/* 겐주로 */
  "고 하더군… 콜록",/* 우쿄 */      "고 하오",        /* 샬롯 */
  "고 하더군",      /* 쥬베이 */    "고 하더라",      /* 카즈키 */
  "고 들었지요",    /* 소게츠 */    "고 전해진다",    /* 아수라 */
  "고, 들었어",     /* 시키 */      "고 하더구먼",   /* 모로즈미 */
  "고 하더구나"     /* 유가 */
};
static int say_about2(int ch, const char *t, int voiced){
  char b[224]; size_t n;
  const char *pre = "", *dash = "", *tail = "";
  if(!t) return 0;
  if(ch >= 0 && ch < 15 && !strstr(t, CHARNAME[ch])){ pre = CHARNAME[ch]; dash = " — "; }
  n = strlen(t);
  if(!voiced && n >= 3 && !memcmp(t + n - 3, "\xEB\x8B\xA4", 3))   /* '다' 로 끝난다 */
    tail = SPK_TAIL[cm_spk];
  if(!*pre && !*tail) return emits(EV_LORE, t);
  snprintf(b, sizeof b, "%s%s%s%s", pre, dash, t, tail);
  return emits(EV_LORE, b);
}
static int say_about(int ch, const char *t){ return say_about2(ch, t, 1); }
static int say_anec(int ch){
  int i, n = 0;
  if(ch < 0 || ch >= 15) return 0;
  /* 첫 마디는 화자 목소리로 (제보: 「상대방 읊는 건 말투가 다 같네, 책 읽듯이」 —
     무기 소회와 같은 원리다). 그 뒤로는 썰 뱅크 회전으로 사실을 잇는다. */
  if(!anecv_used[ch] && ANECV[cm_spk][ch]){
    if(say_about(ch, ANECV[cm_spk][ch])){ anecv_used[ch] = 1; return 1; }
  }
  for(i = 0; i < SS2COMM_ANEC_N; i++) if(ANEC[ch][i]) n++;
  if(!n) return 0;
  for(i = 0; i < n; i++){
    const char *t = ANEC[ch][(anec_at[ch] + i) % n];
    if(!t) continue;
    anec_at[ch] = (unsigned char)((anec_at[ch] + i + 1) % n);
    return say_about2(ch, t, 0);
  }
  return 0;
}
static int say_weap(int ch){
  int i, n = 0;
  if(ch < 0 || ch >= 15) return 0;
  /* 화자 목소리로 쓴 줄이 먼저다. 사실은 같아도 느낌이 달라야 한다 —
     같은 「피 밴 검」이 겐주로에겐 당연하고, 나코루루에겐 무섭고, 유가에겐 취향이다.
     (예전에는 WEAP[상대] 하나뿐이라 누가 해설하든 같은 문장이 나왔다) */
  if(WEAPV[cm_spk][ch]) return say_about(ch, WEAPV[cm_spk][ch]);
  for(i = 0; i < SS2COMM_WEAP_N; i++) if(WEAP[ch][i]) n++;
  if(!n) return 0;
  { const char *t = WEAP[ch][weap_at[ch] % n];
    weap_at[ch] = (unsigned char)((weap_at[ch] + 1) % n);
    return t ? say_about2(ch, t, 0) : 0; }
}
/* 쌓인 모양이 임계에 닿으면 한 마디. 한 번에 한 종류만 낸다. */
static void flow_check(void){
  /* 큐가 꽉 차 못 실렸으면 깃발을 세우지 않는다 — 다음 타에 다시 시도.
     (관계 대사+선제타가 큐 두 칸을 다 차지한 채로 흐름 라인이 오면 통째로
      증발하던 것을 실측으로 잡았다. 성공했을 때만 「한 번 말했다」로 친다.) */
  if(fl_alt>=5 && !(fl_said&FLS_TRADE)){ if(emitn(EV_FLOWTRADE, fl_alt)) fl_said|=FLS_TRADE; return; }
  if(fl_hit>=6 && fl_tak==0 && !(fl_said&FLS_ONE)){ if(emitn(EV_FLOWONE, fl_hit)) fl_said|=FLS_ONE; return; }
  if(fl_tak>=6 && fl_hit==0 && !(fl_said&FLS_CHASE)){ if(emitn(EV_FLOWCHASE, fl_tak)) fl_said|=FLS_CHASE; }
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
  q[slot].vkn[0] = q[slot].vks[0] = q[slot].vkh[0] = 0;
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
/* 연승 장부를 밖에서 본다 — 「끊길 자리에서 끊기나」를 대사가 아니라 숫자로 확인 */
int ss2comm_test_streak(void){ return sess_streak; }
int ss2comm_test_games(void){ return sess_games; }
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
/* 상대 정체 판독 — BLK2 가 아니라 개체 번호(OFF_OPPID)로 본다.
   보스 플래그가 선 판은 유가(14)만 로스터로 인정하고 나머지(간다라 8·그림자 19)는
   표 밖 마물(-1)로 떨어뜨린다 — 심판이 안 서고 마물 관계대사가 나간다. */
/* 간다라 판별 — **개체 19 + 스테이지 7**. 보스기는 보지 않는다.
   실측 세이브 23장(스토리 주행 9장 + 유저 제보 14장) 대조:
     · 스테이지 7 에는 개체 19 밖에 안 온다. 개체 19 도 스테이지 7 에만 온다
     · 보스기는 **믿을 수 없다** — 같은 간다라전이 1 로도 0 으로도 온다
       (제보 세이브 14장 전부 보스기 0 인데 화면은 간다라 — 이것 때문에 못 알아봤다)
   옛 규칙에 있던 「8 + 보스기 = 간다라」는 오독이었다. 그 판은 **스테이지 6** 의
   직전 장 보스전이고(같은 자리에 13+보스기=시키도 온다), 간다라는 그 다음 장
   스테이지 7 에서 나온다. 로스터 번호 그대로 부르게 돌려놓는다. */
static int opp_is_gand(void){
  return rd(OFF_OPPID) == 19 && rd(OFF_STAGE) == 7;
}
static int opp_read(void){
  int id = rd(OFF_OPPID);
  /* 스토리 전체 강제 주행으로 전수 실측(아수라 스토리):
       정규전은 보스기 0 (아수라도 8/보스기0 으로 온다),
       고정 보스전은 **로스터 번호 그대로** + 보스기 1 (시키 13/보스기1 실측 —
       예전 규칙은 이걸 마물로 오인했다),
       개체 19 + 보스기 = 그림자 아수라 (제보: 「아수라한테 지고 리트라이했는데
       마물로 인지함」 — 유저에겐 그냥 아수라다),
       단 19 + 보스기 + **스테이지 7** = 진짜 간다라전 (ganda 세이브 실측). */
  if(opp_is_gand()) return -1;                /* 간다라 — 표 밖 마물 */
  if(id == 19) return 8;                      /* 그림자 아수라 — 아수라로 부른다 */
  if(id < 0 || id > 14) return -1;            /* 표 밖 개체 */
  return id;                                  /* 보스기가 서도 로스터면 그 사람(시키 보스전) */
}
/* 간다라는 로스터 밖 중간보스다. 해설자 15명에도 없다.
   따로 알아보게 해 둔다 — 유가만 제 물건이라 아는 척을 한다. */
#define SS2_CHAR_GANDHARA 15

/* 필살기 이름 — SP 엔진이 방금 낸 기술만 안다(손 커맨드는 이름 없음).
   기술표는 코어에 내장이라 롬 버전과 무관하다. */
extern const char *ss2sp_last_name;
extern int ss2sp_last_ok;
#include "ss2comm_major.h"
/* 주요 기술만 호명한다 — 잡기술 이름까지 부르면 시끄럽고, 조각 녹음량도 는다(제보). */
static int move_is_major(const char *nm){
  unsigned i;
  if(!nm) return 0;
  for(i = 0; i < MOVES_MAJOR_N; i++)
    if(!strcmp(nm, MOVES_MAJOR[i])) return 1;
  return 0;
}

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
  if(st_shk[0]) st_shk[0]--;
  if(st_shk[1]) st_shk[1]--;

  mode  = rd(OFF_MODE);  scr  = rd(OFF_SCR);
  hp1   = rd(OFF_HP1);   hp2  = rd(OFF_HP2);
  a1    = rd16(OFF_ACT1);a2   = rd16(OFF_ACT2);
  surv  = rd(OFF_SURV);  stage= rd(OFF_STAGE);
  if(surv_seen < 0) surv_seen = surv;
  else if(surv != surv_seen){ surv_live = 1; surv_seen = surv; }
  pad   = rd(OFF_PAD);
  if(pad) last_input_f = cm_f;
  if(pend_left > 0 && --pend_left == 0){        /* 결합창이 그냥 닫혔다 — 비오의면 한 마디 */
    int sup; const char *nm = pend_take(&sup);
    (void)nm; if(sup) emit(EV_MOVE);
  }

  if(p_mode < 0){                                /* 첫 프레임: 기준만 잡는다 */
    p_mode=mode; p_scr=scr; p_hp1=hp1; p_hp2=hp2; p_a1=a1; p_a2=a2;
    p_surv=surv; p_stage=stage; p_jing=rd(OFF_JING); p_seqtxt=rd(OFF_SEQTXT); goto out;
  }
  if(mode!=MD_BATTLE && p_mode==MD_BATTLE)
  {
    st_offAt = cm_f;
    /* v0.5.4b: 전투를 벗어나는 순간 메뉴 잡담 타이머를 다시 센다.
       결과 화면은 아직 MD_BATTLE 이라 화면 전환 리셋이 안 걸리고, 그 뒤 mode만 바뀌면서
       전투 전에 세워 둔 낡은 타이머가 이미 만료돼 있어 준비 화면마다 잡담이 터졌다. */
    st_menuAt = cm_f; st_selChatAt = cm_f; st_selChatN = 0;
  }

  if(dbgseq()){
    static int ls=-1, lj=-1, lm=-1, lsc=-1;
    int sq = rd(OFF_SEQTXT), jg = rd(OFF_JING);
    if(sq!=ls || jg!=lj || mode!=lm || scr!=lsc){
      fprintf(stderr, "[SEQ f=%u] mode=%02X scr=%d seq=%d jing=%d rN=%d myR=%d opR=%d lastF=%u b12=%d%d\n",
              cm_f, mode, scr, sq, jg, st_roundN, st_myR, st_opR, st_lastFightF,
              intro_beat1_done, intro_beat2_done);
      ls=sq; lj=jg; lm=mode; lsc=scr;
    }
  }
  if(mode==MD_QUOTE  && p_mode!=MD_QUOTE)  emit(EV_QUOTE);
  if(mode==MD_ENDING && p_mode!=MD_ENDING) emit(EV_ENDING);

  /* ── 심판 안무 — 게임 마커로 구동한다. 시계 예약은 연타로 인트로가 줄면 어긋난다
        (세이브스테이트 실측: 새 매치 인트로 518 → 연타 시 214 프레임) ── */
  if(mode==MD_MENU && scr>=8 && hp1>0 && hp2>0){
    /* 첫 대전 인트로는 진입 순간 상대 개체가 아직 없을 수 있다(스토리 프롤로그 직후).
       그대로 굳히면 구령 3연이 통째로 죽는다 — 개체가 실리는 즉시 승격한다.
       beat1(+354f)보다 훨씬 먼저 실리므로 구령은 제때 나간다. */
    if(intro_pending){
      int opl = opp_read();
      if(opl >= 0){
        int mel = blk_char(rd(OFF_BLK1));
        intro_pending = 0;
        intro_refok = (opl != 14);
        if(intro_nw && intro_refok && mel >= 0){   /* 지각 대진 콜 */
          char t[96];
          snprintf(t, sizeof t, "%s 대 %s!", CHARFULL[mel], CHARFULL[opl]);
          ref_say3(t, 1, 0, 180);
        }
        if(dbgseq()) fprintf(stderr, "[INTRO+ f=%u] late op=%d refok=%d\n", cm_f, opl, intro_refok);
      }
    }
    /* (hp 조건: 무한 대전의 승리 포즈도 F0/scr8 로 온다 — 체력이 둘 다 서 있어야 인트로다)
       인트로 글 연출 카운터의 **시동값**이 어느 글인지 알려준다(0→값 에지):
       15 = 「자아」, 30 = 「정정당당히」(자아와 한 박자로 묶는다), 33 = 「N회전」.
       라운드1 실측 +322(15)/+354(30)/+422(33), 2회전 인트로는 +18(33)뿐. */
    { int seqv = rd(OFF_SEQTXT);
      if(p_seqtxt == 0 && seqv >= 10 && intro_refok){
        if(seqv == 15 && !intro_beat1_done){
          intro_beat1_done = 1;
          ref_say3("자아 — 정정당당히!", 1, 0, 120);
        }else if(seqv == 33 && !intro_beat2_done){
          intro_beat2_done = 1;
          char t[32];
          snprintf(t, sizeof t, "%d회전!", intro_roundN);
          ref_say3(t, 1, 0, 120);
        }
      } }
    /* 징글 2 = 게임 「승부!」 글이 서는 그 프레임 — F1 보다 24프레임 빠르다 */
    if(!intro_shout_done && rd(OFF_JING)==2 && p_jing!=2){
      intro_shout_done = 1;
      if(intro_refok) ref_shout("승부!");
    }
  }
  /* 팻말 2연타: 「카자마 카즈키…!」 → 「훌륭하오!」 (제보: 각각 대사로 이어서) */
  if(plate_at && cm_f >= plate_at){
    plate_at = 0;
    if(plate_char >= 0 && plate_char < 15 && ref_stands()){
      char t[96];
      snprintf(t, sizeof t, "%s…!", CHARFULL[plate_char]);
      ref_say3(t, 1, 0, 150);
      plate2_at = cm_f + 210;      /* = KO+390, 게임 팻말이 서는 실측 시각 */
    }
  }
  if(plate2_at && cm_f >= plate2_at){
    plate2_at = 0;
    if(ref_stands()){
      /* 후속 멘트 4종 회전 — 매번 「훌륭하오!」는 단조롭다(제보) */
      static const char *const PL2[4] = { "훌륭하오!", "장한 승부였소!", "명승부로다!", "다음 상대가 기다리오!" };
      static unsigned char pl2_i;
      pl2_i = (unsigned char)((pl2_i + 1u + (rnd() % 3u)) & 3u);   /* 직전 것 회피 */
      ref_say3(PL2[pl2_i], 1, 0, 120);
    }
  }

  /* ── 라운드 인트로 진입: mode F0 + scr 8 — 선수들이 서고 「자아…승부!」가 도는 구간 ──
     여기서는 BLK 가 이미 유효하다(코어 추적으로 확인). 문구·스토리 화면(F1/scr0)은
     BLK 가 낡아서 「카즈키 대 카즈키」 같은 헛호명이 났다 — 호명은 이제 여기서만. */
  if(mode==MD_MENU && scr>=8 && hp1>0 && hp2>0 && !(p_mode==MD_MENU && p_scr>=8)){
    int me2 = blk_char(rd(OFF_BLK1)), op2 = opp_read();
    /* 2·3회전 인트로(114f)는 짧아서 상대 개체가 아직 안 실릴 때가 있다 — 직전 라운드
       값을 이어받는다 — 2·3회전 콜 실종의 진범: op2=-1 이면 ①refok=0 으로 구령이
       통째로 죽고 ②op2 != st_oppChar 가 참이 되어 새 매치로 오판(1회전! 재발화)까지
       겹쳤다. 이어받기는 「방금까지 싸우던 판」(10초 내)에만 — 셀렉트를 거친 새 매치는
       lastFightF 가 오래돼 타지 않는다. */
    { int fresh = st_lastFightF && cm_f - st_lastFightF <= 600;
      if(op2 < 0 && fresh && st_oppChar >= 0) op2 = st_oppChar;
      if(me2 < 0 && fresh && st_myChar  >= 0) me2 = st_myChar; }
    /* 새 매치 = 격투를 오래 안 봤거나 **상대가 바뀌었거나** — 무한 대전은 75프레임 만에
       다음 상대가 와서 시간만 보면 「라운드 재개」로 오판, 호명이 통째로 빠졌다 */
    int nw  = (!st_lastFightF || cm_f - st_lastFightF > 180
                || (op2 >= 0 && op2 != st_oppChar)
                || st_myR >= 2 || st_opR >= 2);   /* 결판난 매치는 재개일 수 없다 */
    intro_refok  = (op2 >= 0 && op2 != 14);
    intro_pending = (unsigned char)(op2 < 0);   /* 상대 늦탑재 — 아래 승격 루프가 이어받는다 */
    intro_nw      = (unsigned char)(nw ? 1 : 0);
    intro_roundN = nw ? 1 : st_roundN + 1;
    if(dbgseq()) fprintf(stderr, "[INTRO f=%u] me2=%d op2=%d nw=%d refok=%d introN=%d rN=%d lastF=%u oppChar=%d\n",
                         cm_f, me2, op2, nw, intro_refok, intro_roundN, st_roundN, st_lastFightF, st_oppChar);
    plate_at = 0; plate2_at = 0;         /* 못 낸 팻말 호명이 남아 있으면 여기서 접는다 */
    if(nw && intro_refok && me2 >= 0){
      char t[96];
      snprintf(t, sizeof t, "%s 대 %s!", CHARFULL[me2], CHARFULL[op2]);
      ref_say3(t, 1, 0, 180);
    }
    /* 다음 구호들은 시계가 아니라 게임 마커에 맞춘다 — 위 집행부 참조 */
    intro_beat1_done = intro_beat2_done = 0;
    intro_shout_done = 0;
  }

  /* ── 전투측 화면 전환: 승패 결과 이름 화면 · 스토리 사담 ── */
  if(mode==MD_BATTLE && scr!=p_scr){
    if(p_scr>=8 && (scr==0 || scr==2)){
      tk_round();   /* 매치를 가른 판이 타임오버였으면 여기서 정산해야 결과 멘트가 선다 */
      /* 매치가 실제로 끝났을 때만 결과 멘트를 낸다(2선승). 라운드 하나 이긴 것으로는 말하지 않는다.
         승패 화면에서는 **두 줄까지** — 결과 한 마디 + 한마디 더. 그 뒤 잡담은 잠시 잠근다.
         (제보: "대전 사이 승부 난 뒤 대사가 어색하다" — 여러 줄이 몰리고 잡담이 끼어들었다) */
      int done = (st_myR>=2 || st_opR>=2);
      if(!st_resultDone && done && (st_won || st_lost)){
        int wc = st_won ? st_myChar : st_oppChar;   /* 이긴 쪽 */
        st_resultDone = 1;
        (void)wc;   /* 승자 본명 호명은 팻말 시각(plate_at, KO+390)으로 옮겼다 */
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
      /* 문구(VS) 화면 — 스토리 프롤로그도 같은 상태(F1/scr0)로 온다.
         여기서는 BLK 가 낡아 있을 수 있다(제보: 「카즈키 대 카즈키」 헛호명 —
         프롤로그에서 상대 값이 아직 안 실렸다). 그래서 심판 호명은 라운드 인트로
         (F0/scr8, BLK 유효)로 옮겼고, 해설 대진 소개도 **서로 다른 두 캐릭터가
         읽힐 때만** 낸다 — 거울값(낡은 BLK)이면 이름 없는 한마디로 대신한다. */
      int me = blk_char(rd(OFF_BLK1)), op = blk_char(rd(OFF_BLK2));
      if(me>=0 && op>=0 && me!=op){
        /* 「A 대 B」 페어를 %s 에 넣으면 음성 사전생성이 조합 폭발이라, 대진 콜은
           심판 인트로(「A 대 B!」)에 맡기고 해설은 상대 이름만 부른다 — 기존 채움꼴 */
        if(!emits(EV_START, CHARNAME[op])) emit(EV_VSQ);
      }else emit(EV_VSQ);   /* 상대 미상 — 이름 채움("한판" 따위) 금지(제보: 「한판라…」) */
    }
    else if((scr==0||scr==2) && p_mode==MD_BATTLE && cm_f > hush_until) emit(EV_STORYCHAT);
  }

  /* ── 전투 진입 ── */
  if(mode==MD_BATTLE && p_mode!=MD_BATTLE && hp1>0 && hp2>0){
    tk_round();   /* 직전 판이 타임오버로 끝났으면 다음 판이 서기 전에 정산 */
    st_ko=0; st_low1=st_low2=0; st_lead=0; st_rev=0;
    st_won=st_lost=st_resultDone=0;
    st_actAt=cm_f; st_roundStart=cm_f;
    cd[CK_KO]=0; cd[CK_REV]=0;
    int opx = opp_read();
    /* 상대 개체가 아직/잠시 안 실렸으면 직전 판 값을 이어받는다 — 인트로(2·3회전 콜)와
       같은 병: -1 을 「상대 바뀜」으로 읽으면 매 라운드가 새 매치가 되어
       정산·관계대사가 재발화한다(시뮬 검수에서 실증). 이어받기는 10초 내 재개에만. */
    if(opx < 0 && st_lastFightF && cm_f - st_lastFightF <= 600 && st_oppChar >= 0)
      opx = st_oppChar;
    if(!st_lastFightF || cm_f-st_lastFightF > 180 || opx != st_oppChar
       || st_myR >= 2 || st_opR >= 2){   /* 새 매치 — 시간·상대 교체·직전 매치 결판 */
      const char *rel;
      /* 전판 정산 — 2선승 결말을 못 본 채 떠난 매치(무한대전 1라운드제·타임오버·중도 이탈).
         진 채로 떠났으면 연승은 여기서 끊는다 (제보: 「연승 카운트가 끊기거나 줄지 않는다」 —
         지금까지는 한 매치에서 KO 두 번 진 경우에만 0으로 돌아갔다). */
      if(!st_settled && (st_myR || st_opR)){
        if(st_opR > st_myR){ sess_streak=0; sess_lastLossChar=st_oppChar; sess_games++; }
        else if(st_myR > st_opR){ sess_streak++; sess_wins++; sess_games++; }
      }
      st_settled=0;
      loresaid_n=0; fmtsaid_n=0;          /* 새 매치 — 서사·문형 재탕 컷도 새로 센다 */
      if(dbgseq()) fprintf(stderr, "[NEWMATCH f=%u opx=%d oppChar=%d lastF=%u myR=%d opR=%d]\n", cm_f, opx, st_oppChar, st_lastFightF, st_myR, st_opR);
      st_myR=st_opR=0; st_roundN=1;
      st_fb=st_longSaid=st_dblLow=0;
      flow_reset(1);                              /* v0.7 관전 기억 — 매치 통째로 */
      st_myChar  = blk_char(rd(OFF_BLK1));
      st_oppChar = opx;
      st_oppGand = opp_is_gand();
      /* 판을 여는 건 **심판 하나**다. 예전에는 여기서 심판 구호 + EV_START(「하오마루 대
         겐주로…」) + 관계 대사가 한꺼번에 몰려, 두 칸짜리 대기열이 막히면서
         **흐름 대사가 통째로 버려졌다**(test_flow 6개 실패). 제보도 같았다 —
         「시작 메시지랑 캐릭터 첫 메시지랑 겹친다」.
         그래서 심판이 열고, 해설은 **관계 한 줄**로 받는다. 대진 소개는 심판이 이미 했다. */
      /* 「승부!」는 보통 징글 마커(인트로 +494/+190)에서 이미 나갔다.
         여기는 마커를 못 본 판(엔진을 중간에 켠 경우 등)의 예비다. */
      if(!intro_shout_done && ref_stands()) ref_shout("승부!");
      /* 2절 — 관계 대사. 순서가 중요하다:
           같은 캐릭터끼리면 미러 전용 (제보: 「본인이 상대인데도 대사가 좆같음」)
           상대를 못 알아보면 표 밖 개체 = 간다라 (제보: 「간다라 못 알아보는 유가」)
           그 밖에는 화자→상대, 없으면 화자→내 편 */
      if(st_myChar>=0 && st_myChar==st_oppChar) rel = RELSELF[cm_spk];
      else if(st_oppChar<0)                     rel = RELGAND[cm_spk];
      else rel = RELOPP[cm_spk][st_oppChar];          /* 225칸 — 빈칸 없음 */
      if(!rel && st_myChar>=0) rel = RELME[cm_spk][st_myChar];
      /* 관계 대사는 해설창(밴드) — 심판의 「승부!」와 칸이 달라 겹치지 않는다 */
      if(rel) emits(EV_REL, rel);
      else if(st_oppChar>=0){
        if(ANECV[cm_spk][st_oppChar]) say_about(st_oppChar, ANECV[cm_spk][st_oppChar]);
        else if(LORE[st_oppChar]){
          char t[160];
          snprintf(t,sizeof(t),"%s — %s",CHARNAME[st_oppChar],LORE[st_oppChar]);
          emits(EV_LORE, t);
        }
      }
    }else{                                        /* 라운드 재개 */
      st_roundN++; st_fb=st_longSaid=st_dblLow=0;
      flow_reset(0);                              /* v0.7 라운드 단위 관찰만 */
      if(!intro_shout_done && ref_stands()) ref_shout("승부!");   /* 징글 마커를 놓친 판의 예비 */
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
      if(surv_live && surv != st_survSaid &&
         emit_ex(EV_SURV, rec?0:(surv>=10?1:(surv>=7?2:(surv>=3?3:-1))), surv, 0, 0))
        st_survSaid = surv;
    }else if(stage>=1 && stage<=14 && stage > st_lastStage){
      st_lastStage = stage;
      emit_ex(EV_STAGE, (stage+1)>=8?0:((stage+1)>=5?1:-1), stage+1, 0, 0);
    }else if(stage > st_lastStage) st_lastStage = stage;
    goto store;
  }

  /* ── 전투가 아니거나 체력이 회복된 틱(=메뉴/연출) ── */
  if(mode!=MD_BATTLE || hp1>p_hp1 || hp2>p_hp2){
    if(scr!=p_scr && mode!=MD_BATTLE && scr<8){   /* scr>=8 은 라운드 인트로 — 심판의 시간이다 */
      int byClick = (cm_f - last_input_f) < 54;   /* 0.9초 안에 입력이 있었나 */
      st_menuAt = cm_f; st_selChatAt = cm_f; st_selChatN = 0;
      if(!byClick)               emit(EV_STORYCHAT);
      else if(scr==2)            emit(EV_CHARSEL);
      else if(scr==4)            emit(EV_STYLESEL);
      else if(scr==0)            emit(EV_TITLE);
      else if(scr==6)            emit(EV_CARDSEL);
    }
    if(mode!=MD_BATTLE){
      { int bv = rd(OFF_BLK1);                       /* 커서가 실제로 움직였는지 본다 */
        if(blk_boot < 0) blk_boot = bv;
        else if(bv != blk_boot) blk_moved = 1; }
      /* 메뉴에서 **7초마다** 한 마디. 예전에는 캐릭터 선택(scr 2)만 사담이 있었고
         나머지 화면은 15초짜리 「고민이 길구나」 하나뿐이었다. 그래서 카드 그림을
         한참 들여다보는 동안 통째로 조용했다 — 제보: 「카드 고를 때 왜 닥치고 있노」.
         고르는 화면(2 캐릭터 · 4 검질 · 6 카드)에서는 사담과 **썰**을 번갈아 낸다.
         카드 그림을 보고 있을 때 제 캐릭터 이야기를 듣는 게 제일 어울린다. */
      /* 다음 상대 화면(메뉴 scr>=8)에서는 BLK 가 벌써 새 대진을 안다 —
         기둥 아트를 여기서 미리 갈아 준다(제보: 「인지 가능하면 더 빨리 바뀌어야」). */
      if(mode==MD_MENU && scr>=8){
        int me2 = blk_char(rd(OFF_BLK1)), op2 = blk_char(rd(OFF_BLK2));
        if(me2>=0 && op2>=0){ st_myChar = me2; st_oppChar = op2; st_oppGand = 0; }
      }
      int picking = (mode==MD_MENU) && (scr==2 || scr==4);   /* 문구·카드 화면은 제외 — 카드 땐 쉰다(제보) */
      if(!st_selChatAt) st_selChatAt=cm_f;
      if(!st_menuAt)    st_menuAt=cm_f;
      if(picking){
        if(cm_f > hush_until && cm_f - st_selChatAt > 600){   /* 10초 — 말수 축소(제보) */
          int said;
          st_selChatAt = cm_f; st_selChatN++;
          said = (st_selChatN & 1) ? emit(EV_CHARSELCHAT) : 0;
          if(!said){
            int me = blk_char(rd(OFF_BLK1));
            /* 부팅 직후 BLK 는 아무도 안 고른 기본값이다(카즈키 자리) — 그걸 믿고
               오프닝 내내 카즈키 썰만 풀었다(제보). 값이 한 번이라도 움직였거나
               실전을 본 뒤에만 「지금 고른 캐릭터」로 친다. */
            if(!blk_moved && !st_lastFightF) me = -1;
            if(me < 0 || !say_anec(me)) emit(EV_CHARSELCHAT);
          }
        }
      }else if(mode==MD_MENU && (scr==8||scr==10||scr==12) && cm_f > hush_until && cm_f - st_menuAt > 600){
        st_menuAt = cm_f;
        if(!emit(EV_MENUIDLE)) emit(EV_MUSE_M);
      }
    }
    if(hp1>32) st_low1=0;
    if(hp2>32) st_low2=0;
    if(hp1>0 && hp2>0){
      /* 체력이 가득 돌아왔다 = 다음 판이 선다. mode 를 안 벗어나는 전환도 여기로 온다
         (체력이 오르는 틱은 mode 가 전투여도 이 갈래로 떨어진다 — 위 조건 참고). */
      st_ko=0;   /* 다음 판 구령은 라운드 인트로 안무가 맡는다 */
    }
    goto store;
  }

  /* ── 전투 중 ── */
  if(scr>=8){
    st_lastFightF = cm_f;   /* 진짜 격투 프레임 — 문구·스토리(F1/scr0)는 안 친다 */
    if(!st_ko){ st_fHp1 = hp1; st_fHp2 = hp2; }   /* 타임오버 판정용 마지막 체력 — KO 뒤엔 안 덮는다 */
  }
  if(scr>=8 && (st_myChar<0 || st_oppChar<0)){
    /* 세이브스테이트를 전투 중간에 불러오면 매치 진입 에지가 없어 캐릭터를 모른다 —
       그 매치 내내 심판이 통째로 침묵했다(스테이트 재현으로 확인). BLK 는 전투 중에도
       살아 있으니 여기서 주워 담는다. 간다라(표 밖)는 그대로 -1 로 남는다. */
    st_myChar  = blk_char(rd(OFF_BLK1));
    st_oppChar = opp_read();
    st_oppGand = opp_is_gand();
  }
  /* v0.5.6: 승부가 난 뒤의 승리 포즈도 액션ID가 0x180을 넘는다.
     그걸 필살기로 읽어 "온다! 비오의!" 를 뜬금없이 외치던 버그를 여기서 막는다.
     둘 다 살아 있고 KO 상태가 아닐 때만 기술 발동으로 본다. */
  if(hp1>=100 && hp2>=100) st_ko=0;   /* 라운드 재개 = KO 표식 해제 */
  { int live = (hp1>0 && hp2>0 && !st_ko);
  if(live && a1>=0x180 && p_a1<0x180){            /* 내 필살기 발동 → 결합창 열기 */
    pend_name = (ss2sp_last_name && ss2sp_last_ok!=0
                 && move_is_major(ss2sp_last_name)) ? ss2sp_last_name : 0;
    pend_sup  = (pend_name && !strcmp(pend_name,"비오의"));
    pend_left = 27;
    /* 비오의는 **나가는 순간 바로** 호들갑을 떤다. 결합창을 기다리면 적중 멘트에 먹힌다. */
    if(pend_sup) emit(EV_MOVE);
  }
  if(live && a2>=0x180 && p_a2<0x180){ flow_oppsp(); emit(EV_OPPSP); }
  }

  hit2 = hp2 < p_hp2; hit1 = hp1 < p_hp1;
  /* 기둥 충격 — 맞은 쪽 일러가 쿵 흔들리고 큰 타격은 첫 프레임 하얗게 번쩍 */
  if(hit2){ int d = p_hp2-hp2; st_shk[1] = (unsigned char)(d >= 12 ? 12 : (d >= 4 ? 8 : 5)); }
  if(hit1){ int d = p_hp1-hp1; st_shk[0] = (unsigned char)(d >= 12 ? 12 : (d >= 4 ? 8 : 5)); }
  /* v0.7 관전 기억 — 유효타만. pend_name 이 살아 있으면 그 기술로 친 것이다
     (아래 pend_take 가 가져가기 전에 세야 한다). */
  if(hit2 && (p_hp2-hp2)>=4) flow_hit(pend_name);
  if(hit1 && (p_hp1-hp1)>=4) flow_take();
  /* 큐가 붐벼서 못 실린 흐름 라인 재시도 — 임계는 이미 넘겼는데 깃발이 안 선 경우만.
     연타가 한숨에 몰리면 그 사이 큐가 안 비므로, 큐가 빠질 시간을 두고 이따금 돈다. */
  if((cm_f & 63)==0) flow_check();
  down2   = (a2>=0x13C && a2<=0x154) && !(p_a2>=0x13C && p_a2<=0x154);
  downed1 = (a1>=0x13C && a1<=0x154) && !(p_a1>=0x13C && p_a1<=0x154);

  if(!st_fb && (hit1||hit2)){
    st_fb = 1;
    if(hit2 && !hit1 && st_roundN==1 && hp2>0 && !pend_name && (p_hp2-hp2)>=4) (((rnd()&1) && say_weap(st_oppChar)) || emit(EV_FIRSTBLOOD));
  }

  if(hit1 && hit2 && hp1<=0 && hp2<=0 && !st_ko){ st_ko=1; st_fHp1=st_fHp2=0; pend_take(0); flow_round('d');
    if(ref_stands()) ref_shout("무승부!");
    emit(EV_DKO); }
  else{
    if(hit2 && hp2<=0 && !st_ko){
      int sup; const char *nm = pend_take(&sup);
      if(!nm && (ACT_HEAVY(a1) || ACT_HEAVY(p_a1))) nm = "강베기";   /* 시그니처 한 방 호명 */
      st_ko=1; st_fHp1=st_fHp2=0; st_won=1; st_lost=0;
      if(nm) emit_ex(EV_MOVEKO,-1,0,0,nm); else emit(EV_KO);
      st_myR++; flow_round('w');
      /* 체력바가 벌어지는 그 순간 — 심판이 먼저 찍는다. 매치가 갈렸으면 「승부 결정!」 */
      if(ref_stands()){
        /* 게임이 PERFECT 를 띄우는 판은 심판도 「완승!」 (제보: 「완승 이런 것도 있거든」) */
        ref_shout(hp1>=128 ? "완승!" : st_myR>=2 ? "승부 결정!" : "한 판!");
        plate_at = cm_f + 180; plate_char = st_myChar;   /* 본명은 3초 뒤 — 팻말(KO+390)엔 후속 멘트 */
      }
      { int mw = (st_myR>=2); int said;
        if(mw && !st_settled){ sess_streak++; sess_wins++; sess_games++; st_settled=1; }
        said =
        (st_low1 && emit(EV_COMEBACK)) ||
        (hp1>=128 && emit(EV_PERFECT)) ||
        ((cm_f-st_roundStart) < 600 && emit(EV_QUICK)) ||
        (mw && st_oppChar>=0 && st_oppChar==sess_lastLossChar && emit(EV_REVENGE)) ||
        (mw && sess_streak != st_streakSaid &&
              (sess_streak==2||sess_streak==3||sess_streak==5||sess_streak==7||
                (sess_streak>=10 && sess_streak%5==0)) &&
              emit_ex(EV_STREAK, sess_streak>=5?0:-1, sess_streak,0,0) &&
              (st_streakSaid = sess_streak));
        (void)said;
        if(mw && st_oppChar>=0 && st_oppChar==sess_lastLossChar) sess_lastLossChar=-1;
      }
    }
    else if(down2){
      int sup; const char *nm = pend_take(&sup);
      if(!nm && (ACT_HEAVY(a1) || ACT_HEAVY(p_a1))) nm = "강베기";
      if(nm) emit_ex(EV_MOVEDOWN,-1,0,0,nm);
      else if(pend_left>0) emit_ex(EV_MOVEDOWNA,-1,0,0,0);
      else if(cm_f - st_hitAt > 42) emit(EV_DOWN);
    }
    else if(hit2){
      int d = p_hp2 - hp2;
      if(d>=4){
        int sup; const char *nm = pend_take(&sup);
        if(!nm && (ACT_HEAVY(a1) || ACT_HEAVY(p_a1))) nm = "강베기";
        if(nm){ emit_ex(d>=12?EV_MOVEHIT:EV_MOVEHITL, -1, 0,0, nm); st_hitAt=cm_f; }
        else if(d>=12){ if(!((rnd()%10u)<2 && emit(EV_PJUDGE))) emit(EV_HIT); st_hitAt=cm_f; }   /* 2/10 은 평가(내 캐릭 호명) */
      }
    }
    if(hit1){
      int d = p_hp1 - hp1;
      if(hp1<=0 && !st_ko){
        st_ko=1; st_fHp1=st_fHp2=0; st_lost=1; st_won=0;
        emit(EV_KOED);
        if(surv>0 && surv_live) emitn(EV_SURVEND, surv);
        st_opR++; flow_round('l');
        if(ref_stands()){
          ref_shout(hp2>=128 ? "완승!" : st_opR>=2 ? "승부 결정!" : "한 판!");
          plate_at = cm_f + 180; plate_char = st_oppChar;
        }
        if(st_opR>=2 && !st_settled){ sess_streak=0; sess_lastLossChar=st_oppChar; sess_games++; st_settled=1; }
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
  p_surv=surv; p_stage=stage; p_jing=rd(OFF_JING); p_seqtxt=rd(OFF_SEQTXT);

out:
  if(!chat_enabled){                               /* 캐릭터챗 오프 — 쿠로코만 남는다 */
    q_head = q_cnt = 0; curline[0] = 0;
    if(ref_has && cm_f - ref_at > 90) ref_has = 0;
    return 0;                                      /* 혼잣말·팝도 전부 접는다 */
  }
  if(ref_has && cm_f - ref_at > 90) ref_has = 0;   /* 묵은 구호 — 1.5초 안에 못 세우면 버린다.
                                                      busy 검사보다 먼저 해야 교착이 없다 */
  /* 심판이 말하는 동안(서 있거나 곧 선다) 캐릭터챗은 후순위 — 팝도 혼잣말도 쉰다 */
  { int ref_busy = ref_enabled && (ref_has ||
        (ref_shown && cm_f - ref_shown <= (unsigned)(ref_ttl>0?ref_ttl:180)) ||
        plate_at || plate2_at ||                     /* 팻말 호명(이름→훌륭하오)이 남아 있다 */
        (mode==MD_MENU && scr>=8 && intro_refok));   /* 인트로 안무(호명~승부!) 화면 전체 */
    /* 제보: 「심판 있을 때는 심판 코멘트 사이에 끼지 마라」 — 안무가 도는 동안은
       통째로 심판의 칸이다. 그리고 그 사이 삭은 반응은 **버린다** —
       안무 끝나고 KO 반응을 뒤늦게 뱉었다(제보: 「밀린 대사는 치우는 걸로」). */
    while(q_cnt){
      unsigned pr  = ev_prio(q[q_head].ev);
      unsigned lim = pr >= 2 ? Q_STALE_BIG : pr >= 1 ? Q_STALE_MID : Q_STALE;
      if(ref_busy && !ev_resultish(q[q_head].ev) && lim > Q_STALE_MID)
        lim = Q_STALE_MID;   /* 점유 중엔 순간 반응은 6초 컷 — 결과 계열은 예외 */
      if(cm_f - q[q_head].at <= lim) break;
      flow_unsay(q[q_head].ev);
      q_head = (q_head+1)%QN; q_cnt--;
    }
    if(ref_busy) return 0; }
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
    unsigned anecN = (mode==MD_BATTLE) ? 360 : 420;   /* 6초 / 7초 — 비전투는 말수를 줄인다(제보) */
    unsigned need  = (mode==MD_BATTLE) ? 480 : 600;   /* 8초 / 10초 */
    /* 잡담 필러는 **진짜 메뉴(카드 화면 제외)와 공방 중에만** 돈다.
       문구·스토리·카드 화면에서 썰이 쏟아졌다(제보: 「스토리 볼 때 딴소리 오지게 함」
       「카드 획득 같은 중간 이벤트 때는 쉬라」). */
    int fillok = (mode==MD_MENU && (scr==2 || scr==4 || scr>=8)) || (mode==MD_BATTLE && scr >= 8);
    /* 타이틀·컬렉션·설정(s0)과 카드 화면(s6)은 제외 — 검수: 컬렉션 구경 내내 잔존 캐릭터 썰이 돌았다 */
    if(fillok && cm_f > 300 && quiet > anecN && cm_f > hush_until && !st_ko){
      /* 상대 이야기를 한 번 풀고, 다음 차례엔 내 편 이야기. 번갈아 간다. */
      static unsigned char turn;
      /* 메뉴에서는 아직 매치가 안 잡혀 st_myChar 가 비어 있다. 램에서 바로 읽는다 —
         안 그러면 메뉴에선 썰이 한 줄도 안 나가고 「…지루하구나」만 돈다. */
      int a1c = (mode==MD_BATTLE) ? st_oppChar : blk_char(rd(OFF_BLK2));
      int a2c = (mode==MD_BATTLE) ? st_myChar  : blk_char(rd(OFF_BLK1));
      /* 선택 화면(s2/s4)의 BLK 는 부팅 기본값일 수 있다 — 고른 적도 싸운 적도 없으면
         특정 캐릭터 썰 금지(검수: 켜자마자 카즈키 썰 도배). VS 화면(s8+)은 값이 진짜다. */
      if(mode==MD_MENU && scr<8 && !blk_moved && !st_lastFightF){ a1c = -1; a2c = -1; }
      int said;
      if(turn & 1){ int t = a1c; a1c = a2c; a2c = t; }
      /* 세 번에 한 번은 **혼잣말** 차례로 둔다. 안 그러면 썰이 빈 자리를 전부 먹어
         MUSE 계열 60줄이 통째로 죽는다(시뮬레이터가 「한 번도 안 나옴」으로 잡았다). */
      /* 빈 자리 차례: 썰 → 썰 → 관계 → 혼잣말 순으로 돈다.
         관계가 「매치 시작 한 번」에서 빠져나와 빈 자리에도 들어간다. */
      said = 0;
      /* 내가 뒤지는 판의 응원은 잡담 순번에 밀리지 않는다 — 쿨다운(15초)이 도배를 막는다 */
      if(!said && mode==MD_BATTLE && hp1 + 12 < hp2) said = emit(EV_PCHEER);
      if(!said && (turn % 4) == 2){
        const char *r3 = 0;
        if(a1c >= 0)      r3 = RELOPP[cm_spk][a1c];
        if(!r3 && a2c>=0) r3 = RELME [cm_spk][a2c];
        if(r3) said = emits(EV_REL, r3);
      }
      if(!said && (turn % 4) != 3) said = (say_anec(a1c) || say_anec(a2c));
      if(said) turn++;
      else if(quiet > need)
        { turn++;
          /* 내가 뒤지는 소강엔 응원이 먼저 — 잡담보다 판을 본다 (v3 플레이어 축) */
          if(!(mode==MD_BATTLE && hp1 + 12 < hp2 && emit(EV_PCHEER)))
            emit(mode==MD_BATTLE ? EV_MUSE_B : (mode==MD_QUOTE ? EV_MUSE_Q : EV_MUSE_M)); }
      else if((turn % 4) == 3) turn++;
    }
  }
  /* 묵은 반응은 보여 주지 않고 버린다 — 그 순간이 지난 말은 없는 것만 못하다.
     다만 관계·세계관 쪽은 더 기다려 준다. VS 화면이 짧아 2.5초로는 못 나간다. */
  while(q_cnt){
    unsigned pr  = ev_prio(q[q_head].ev);
    unsigned lim = pr >= 2 ? Q_STALE_BIG : pr >= 1 ? Q_STALE_MID : Q_STALE;
    if(cm_f - q[q_head].at <= lim) break;
    flow_unsay(q[q_head].ev);             /* 삭아 버려지는 흐름 라인도 재시도 대상 */
    q_head = (q_head+1)%QN; q_cnt--;
  }

  /* 심판 구호가 먼저 나간다. 해설 대기열은 건드리지 않으므로 바로 뒤에 총평이 붙는다. */
  /* 심판은 **제 차선**이다. 해설 간격(4.5초)을 같이 기다리면 판이 이미 시작된 뒤에
     「첫 판 —」이 뜬다(제보: 「늦고 밀린다」). 그래서 해설 게이트는 안 본다.
     다만 **제 간격은 지킨다.** 안 그러면 판이 몰릴 때 심판이 연달아 네 번 떠들면서
     KO·총평을 통째로 굶긴다. 몰린 구호는 ref_text 가 덮어써서 **최신 것 하나로 합쳐진다** —
     세 판이 순식간에 지나가면 「셋째 판」만 나오는 게 맞다. */

  /* 구령(심판)이 도는 동안은 해설을 안 뽑는다 — 구령은 1초 안짝이라 스테일 창을
     넘기지 않고, 끝나는 프레임에 해설이 바로 이어진다. 「구호가 먼저, 해설이 그 뒤」. */
  /* 구령 대기 게이트 제거 — 음성이 2채널이라 해설은 구령과 겹쳐 나온다(70% 덕킹).
     밀린 해설이 스테일로 버려지던 손실이 사라진다. */
  if(q_cnt && cm_f >= q_next){
    /* 등급이 높은 것부터. 같은 등급이면 **제일 최근 것**을 낸다.
       예전에는 먼저 들어온 쪽을 냈다. 그러면 말할 차례가 왔을 때 이미 지난 얘기를
       하게 되고, 그걸 막으려고 「몇 초 지나면 버린다」는 창을 캐릭터별로 매번
       손봐야 했다. 뽑을 때 최신을 고르면 애초에 상할 일이 없다 —
       상태는 매 프레임 보고 있고, 지켜야 하는 건 **말 사이 간격 하나**뿐이다. */
    int i, best = -1;
    ss2q chosen;
    for(i = 0; i < q_cnt; i++){
      int c = (q_head + i) % QN;
      int ev0 = q[c].ev;
      /* 판이 갈린 뒤 대전썰 금지 — 구령·총평 뒤에 뒤늦게 「대전 전 썰」이 풀리면
         그 순간이 지나 없느니만 못하다 (제보). 스테일 창이 알아서 걷어간다. */
      if(st_ko && (ev0==EV_REL || ev0==EV_LORE || ev0==EV_START
                   || ev0==EV_VSQ || ev0==EV_STORYCHAT)) continue;
      /* 서사 직후의 호격 완충 — 관객에게 회상을 들려주다 한 문장 만에 선수에게
         소리치면 어색하다(제보: 「그 미묘한 부분」). 8초는 저온끼리 잇는다. */
      if(lore_at && cm_f - lore_at < 560 &&
         (ev0==EV_PCHEER || ev0==EV_PJUDGE || ev0==EV_START)) continue;
      if(best < 0){ best = c; continue; }
      { int pc = ev_prio(ev0), pb = ev_prio(q[best].ev);
        if(pc > pb || (pc == pb && q[c].at > q[best].at)) best = c; }
    }
    if(best < 0) return 0;
    chosen = q[best];
    if(best != q_head) q[best] = q[q_head];
    q_head = (q_head+1)%QN; q_cnt--;
    snprintf(curline,sizeof(curline),"%s",chosen.text);
    cur_ev = chosen.ev; cur_spk = chosen.spk; cur_f = cm_f; last_line_f = cm_f;
    mark_said(curline);
    if(dbgseq()) fprintf(stderr, "[AIR f=%u ev=%d spk=%d] %s\n", cm_f, cur_ev, cur_spk, curline);
    if(cur_ev == EV_REL || cur_ev == EV_LORE || cur_ev == EV_STORYCHAT || cur_ev == EV_QUOTE)
      lore_at = cm_f;               /* 서사(회상 모드) 발화 시각 — 호격 완충용 */
    if(chosen.vkn[0]) ss2voice_say_parts(chosen.vkh[0] ? chosen.vkh : 0,
                                          chosen.vkn, chosen.vks, 0);      /* 이어붙이기 */
    else              ss2voice_say(curline, 0);  /* 온에어 = 음성도 이 순간 */
    /* 공방 중에는 넓게 벌린다. 말할 기회가 드물어야 아무 말이나 안 하게 된다.
       결과 계열(승패 화면·한마디 더·전적)은 한 박자 더. */
    { unsigned g = ((cur_ev==EV_WINSCR||cur_ev==EV_LOSESCR||cur_ev==EV_WINTALK||
                      cur_ev==EV_LOSETALK||cur_ev==EV_RECORD) ? GAP_RESULT
                    : (ev_prio(cur_ev) >= 3) ? GAP_OTHER      /* 관계·안내는 드무니 막지 않는다 */
                    : (mode==MD_BATTLE && !st_ko) ? GAP_BATTLE : GAP_OTHER);
      /* 팩 1.5배 완화는 폐지 — 3채널·체이닝이 실시간 겹침을 흡수한다(제보: 「빈도 늘어도 되겠다」) */
      q_next = cm_f + g; }
    return curline;
  }
  return 0;
}

/* ══ 자체 렌더 (D) ══════════════════════════════════════════════════
   RetroArch OSD 폰트는 환경마다 한글이 깨질 수 있어, 코어가 직접 그린다.
   화면 아래에 띠를 덧붙이고 초상 + 8x8 갈무리 글리프 + 연출을 찍는다. 폰트 의존 0. */
#include "ss2comm_font.h"

static int cm_draw = 4;   /* 기본: 화면 밖 위 띠 (아래는 어색하다는 제보로 상방 기본) */
void ss2comm_draw_enable(int mode){ cm_draw = mode; }
/* 0 끔 / 1 화면 밖 아래 띠 / 2 화면 안 위 / 3 화면 안 아래 / 4 화면 밖 위 띠 */

/* 자동 생성 글꼴에 없는 몇 자만 손으로 보강한다 (K·X — svcsp 기술 표기용) */
static const ss2glyph SS2FONT_X8[] = {
  {0x004B,{0x88,0x90,0xA0,0xC0,0xA0,0x90,0x88,0x00}},   /* K */
  {0x0058,{0x88,0x88,0x50,0x20,0x50,0x88,0x88,0x00}},   /* X */
};
static const ss2glyph *glyph_of(unsigned cp){
  int lo=0, hi=SS2FONT_N-1;
  while(lo<=hi){ int mid=(lo+hi)>>1;
    if(SS2FONT[mid].cp==cp) return &SS2FONT[mid];
    if(SS2FONT[mid].cp<cp) lo=mid+1; else hi=mid-1; }
  { unsigned i; for(i=0;i<sizeof SS2FONT_X8/sizeof SS2FONT_X8[0];i++)
      if(SS2FONT_X8[i].cp==cp) return &SS2FONT_X8[i]; }
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

#include "ss2comm_fix.h"
/* 한글패치 글자 깨짐 보정 — 로드된 롬 사본(메모리)에만 손댄다. 파일은 불변.
   검증합이 안 맞으면(다른 롬·이미 수정) 아무것도 하지 않는다. */
static void rom_fix(unsigned char *r, unsigned len){
  unsigned i, s;
  if(!r || len < 0x200000) return;
  for(s = 0, i = 0; i < SS2FIX_REC_LEN; i++) s = (s + r[SS2FIX_REC_OFF + i]) & 0xFFFF;
  if(s != SS2FIX_REC_SUM) return;
  for(i = 0; i < 240; i++) if(r[SS2FIX_FREE_OFF + i] != 0xFF) return;
  memcpy(r + SS2FIX_FREE_OFF,       SS2FIX_TILES, 208);        /* 글자 12 + 하단 민무늬 */
  memcpy(r + SS2FIX_FREE_OFF + 208, r + SS2FIX_BLANKT_OFF, 16); /* 상단 민무늬 */
  memcpy(r + SS2FIX_FREE_OFF + 224, r + SS2FIX_BLANKB_OFF, 16); /* 모서리 장식 */
  r[SS2FIX_BANK_OFF] = 0x3F;
  memcpy(r + SS2FIX_ADDR_OFF, SS2FIX_ADDRS, 30);
  memcpy(r + SS2FIX_LAYT_OFF, SS2FIX_LAYT, 8);
  memcpy(r + SS2FIX_LAYB_OFF, SS2FIX_LAYB, 8);
}

void ss2comm_set_rom(const void *rom, unsigned len){
  cm_rom = (const unsigned char *)rom; cm_romlen = len; face_built = 0;
}

/* 유저가 롬을 직접 패치하기로 함 — 자동 적용은 끈다. 필요하면 이 함수만 다시 호출. */
void ss2comm_rom_fix(void *rom, unsigned len){ rom_fix((unsigned char *)rom, len); (void)rom; (void)len; }

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
/* ── 빠른 설정 오버레이 — 게임 화면 위에 직접 띄우는 설정창 ─────────────
   앱이 변수 포인터를 묶어 주면(bind) 엔진이 그리기·조작을 다 맡는다.
   값은 앱 변수에 바로 쓰이므로 저장은 앱의 기존 설정 저장이 그대로 담당. */
static int draw_line11(uint16_t *fb,int pitch_px,int x0,int x1,const char *s,const char *end,
                       int ytop,int clipTop,int clipBot,int show,uint16_t col,int bold);
typedef struct { const char *name; unsigned char *v; unsigned char max; unsigned char kind; } ss2ovitem;
static int ov_svc = 0;      /* 1 = SvC — SP 배치 페이지가 svcsp 표를 쓴다 */
static ss2ovitem ov_it[8];
static int ov_n = 0;
static unsigned char ov_on = 0, ov_cur = 0;
void ss2comm_overlay_bind(unsigned char *chat, unsigned char *spk, unsigned char *ref,
                          unsigned char *sides, unsigned char *sbg, unsigned char *vib,
                          unsigned char *cap, unsigned char *sp){
  int n = 0;
  if(sp)   { ov_it[n].name="원버튼 필살기"; ov_it[n].v=sp; ov_it[n].max=1; ov_it[n].kind=0; n++; }
  if(chat) { ov_it[n].name="캐릭터 해설"; ov_it[n].v=chat;  ov_it[n].max=1; ov_it[n].kind=0; n++; }
  if(spk)  { ov_it[n].name="해설자";      ov_it[n].v=spk;   ov_it[n].max=SS2COMM_SPK_N-1; ov_it[n].kind=1; n++; }
  if(ref)  { ov_it[n].name="심판 쿠로코"; ov_it[n].v=ref;   ov_it[n].max=1; ov_it[n].kind=0; n++; }
  if(sides){ ov_it[n].name="기둥 아트";   ov_it[n].v=sides; ov_it[n].max=1; ov_it[n].kind=0; n++; }
  if(sbg)  { ov_it[n].name="기둥 배경";   ov_it[n].v=sbg;   ov_it[n].max=7; ov_it[n].kind=2; n++; }
  if(vib)  { ov_it[n].name="진동";        ov_it[n].v=vib;   ov_it[n].max=1; ov_it[n].kind=0; n++; }
  if(cap)  { ov_it[n].name="장면 수집";   ov_it[n].v=cap;   ov_it[n].max=1; ov_it[n].kind=0; n++; }
  ov_n = n;
}
/* ── 오버레이 2페이지: SP 기술 배치 ──
   ss2sp 층이 같이 링크되는 실제 빌드(코어·앱)에서만 켠다.
   단독 하네스(-DSS2COMM_TEST)는 ss2sp 없이 링크되므로 페이지가 빠진다
   (미리보기 하네스는 -DSS2COMM_SPEDIT 로 강제). 슬롯 표는 앱이 설정 저장 때
   ss2sp_slots_blob 을 그대로 직렬화하므로 여기서 바꾸면 저절로 저장된다. */
#if !defined(SS2COMM_TEST) || defined(SS2COMM_SPEDIT)
#define SS2OV_SP 1
extern int  ss2sp_style_count(void);
extern int  ss2sp_slot_count(void);
extern const char *ss2sp_style_id(int style);
extern int  ss2sp_cur_style(void);
extern int  ss2sp_move_count(int style);
extern const char *ss2sp_move_name(int style, int i);
extern int  ss2sp_move_notation(int style, int i, char *out, int cap);
extern int  ss2sp_get_slot(int style, int slot);
extern void ss2sp_set_slot(int style, int slot, int mv);
extern int  ss2sp_move_flags(int style, int i);
extern void ss2sp_reset_slots(void);
static unsigned char ov_page = 0;   /* 0=빠른 설정, 1=SP 배치, 2=기술 고르기 */
static int ov_spstyle = 0;          /* 마지막으로 본 유파 — 전투 밖에서 열었을 때 대비 */
static int ov_pickslot = 0;         /* 고르는 중인 슬롯 */
static const char *ov_slotname(int i){
  static const char *nm[7] = {"기본","→ 앞","← 뒤","↓ 아래","↘ 앞아래","↙ 뒤아래","공중"};
  return (i >= 0 && i < 7) ? nm[i] : "?";
}
/* 넘패드 표기(236+A)를 화살표(↓↘→+A)로 — 커맨드도 화살표로 보라는 제보 */
static void ov_nota_arrows(const char *nt, char *out, int cap){
  static const struct { char d; const char *a; } M[9] = {
    {'8',"↑"},{'2',"↓"},{'4',"←"},{'6',"→"},
    {'9',"↗"},{'7',"↖"},{'3',"↘"},{'1',"↙"},{'5',"·"}};
  int n = 0, i; const char *p;
  for(p = nt; *p && n < cap - 4; p++){
    const char *rep = 0;
    for(i = 0; i < 9; i++) if(M[i].d == *p){ rep = M[i].a; break; }
    if(rep){ int l = (int)strlen(rep); memcpy(out + n, rep, l); n += l; }
    else out[n++] = *p;
  }
  out[n] = 0;
}
static void ov_stylelabel(int st, char *out, int cap){
  static const struct { const char *id, *ko; } NM[15] = {
    {"kazuki","카즈키"},{"sogetsu","소게츠"},{"haohmaru","하오마루"},
    {"genjuro","겐주로"},{"nakoruru","나코루루"},{"rimururu","리무루루"},
    {"hanzo","한조"},{"galford","갈포드"},{"asura","아수라"},
    {"charlotte","샤를로트"},{"morozumi","모로즈미"},{"ukyo","우쿄"},
    {"jubei","쥬베이"},{"shiki","시키"},{"yuga","유가"}};
  const char *id = ss2sp_style_id(st); const char *ko = id; int i, bl;
  const char *cut = strrchr(id, '_');
  bl = cut ? (int)(cut - id) : (int)strlen(id);
  for(i = 0; i < 15; i++)
    if((int)strlen(NM[i].id) == bl && !strncmp(NM[i].id, id, bl)){ ko = NM[i].ko; break; }
  snprintf(out, cap, "%s·%s", ko, (cut && !strcmp(cut, "_bst")) ? "나찰" : "수라");
}
#if !defined(SS2COMM_TEST)
#define SS2OV_SVC 1
extern int  svcsp_char_count(void);
extern const char *svcsp_char_name(int c);
extern int  svcsp_cur_char(void);
extern int  svcsp_move_count(int c);
extern const char *svcsp_move_name(int c, int i);
extern int  svcsp_move_flags(int c, int i);
extern int  svcsp_move_notation(int c, int i, char *out, int cap);
extern int  svcsp_get_slot(int c, int k);
extern void svcsp_set_slot(int c, int k, int mv);
extern void svcsp_reset_slots(void);
#endif
/* 게임 분기 — SvC 면 svcsp 표, 아니면 ss2sp 표 */
static int sp_style_count(void){
#ifdef SS2OV_SVC
  if(ov_svc) return svcsp_char_count();
#endif
  return ss2sp_style_count(); }
static int sp_cur(void){
#ifdef SS2OV_SVC
  if(ov_svc) return svcsp_cur_char();
#endif
  return ss2sp_cur_style(); }
static int sp_move_count(int st){
#ifdef SS2OV_SVC
  if(ov_svc) return svcsp_move_count(st);
#endif
  return ss2sp_move_count(st); }
static const char *sp_move_name(int st,int i){
#ifdef SS2OV_SVC
  if(ov_svc) return svcsp_move_name(st,i);
#endif
  return ss2sp_move_name(st,i); }
static int sp_move_flags(int st,int i){
#ifdef SS2OV_SVC
  if(ov_svc) return svcsp_move_flags(st,i);
#endif
  return ss2sp_move_flags(st,i); }
static int sp_nota(int st,int i,char*o,int c){
#ifdef SS2OV_SVC
  if(ov_svc) return svcsp_move_notation(st,i,o,c);
#endif
  return ss2sp_move_notation(st,i,o,c); }
static int sp_get(int st,int k){
#ifdef SS2OV_SVC
  if(ov_svc) return svcsp_get_slot(st,k);
#endif
  return ss2sp_get_slot(st,k); }
static void sp_set(int st,int k,int mv){
#ifdef SS2OV_SVC
  if(ov_svc){ svcsp_set_slot(st,k,mv); return; }
#endif
  ss2sp_set_slot(st,k,mv); }
static void sp_resetslots(void){
#ifdef SS2OV_SVC
  if(ov_svc){ svcsp_reset_slots(); return; }
#endif
  ss2sp_reset_slots(); }
static void sp_label(int st, char *out, int cap){
#ifdef SS2OV_SVC
  if(ov_svc){ snprintf(out, cap, "%s", svcsp_char_name(st)); return; }
#endif
  ov_stylelabel(st, out, cap); }
static int ov_sp_rows(void){ return 3 + ss2sp_slot_count(); }  /* 캐릭터 + 슬롯들 + 초기화 + 돌아가기 */
#endif
void ss2comm_overlay_spmode(int svc){ ov_svc = !!svc; }
/* 페이지 0 에 토글 항목 하나를 덧붙인다 (SvC 기술명 표시 등) */
void ss2comm_overlay_bind_knob(const char *name, unsigned char *v, int max){
  if(ov_n >= 8 || !v) return;
  ov_it[ov_n].name = name; ov_it[ov_n].v = v;
  ov_it[ov_n].max = (unsigned char)max; ov_it[ov_n].kind = 2;
  ov_n++;
}
void ss2comm_overlay_bind_extra(const char *name, unsigned char *v){
  if(ov_n >= 8 || !v) return;
  ov_it[ov_n].name = name; ov_it[ov_n].v = v; ov_it[ov_n].max = 1; ov_it[ov_n].kind = 0;
  ov_n++;
}
int  ss2comm_overlay_active(void){ return ov_on; }
void ss2comm_overlay_toggle(void){
  ov_on = !ov_on;
  if(ov_on){ ov_cur = 0;
#ifdef SS2OV_SP
    ov_page = 0;
#endif
  }
}
int ss2comm_overlay_input(int k){
  ss2ovitem *it;
  if(!ov_on || !ov_n) return 0;
#ifdef SS2OV_SP
  /* B·옵션(k=5)은 한 겹씩 물러난다 — 기술 고르기→SP 배치→빠른 설정→닫기 */
  if(k == 5){
    if(ov_page == 2){ ov_page = 1; ov_cur = (unsigned char)(ov_pickslot + 1); return 1; }
    if(ov_page == 1){ ov_page = 0; ov_cur = 0; return 1; }
    ov_on = 0; return 1;
  }
  if(ov_page == 2){                                    /* 기술 고르기 목록 */
    int rows = sp_move_count(ov_spstyle) + 1;          /* 0 = 비움 */
    if(k == 0){ ov_cur = (unsigned char)((ov_cur + rows - 1) % rows); return 1; }
    if(k == 1){ ov_cur = (unsigned char)((ov_cur + 1) % rows); return 1; }
    if(k == 4){                                        /* A = 선택하고 돌아간다 */
      sp_set(ov_spstyle, ov_pickslot, (int)ov_cur - 1);
      ov_page = 1; ov_cur = (unsigned char)(ov_pickslot + 1);
    }
    return 1;
  }
  if(ov_page == 1){
    int rows = ov_sp_rows(), st = sp_cur();
    if(st >= 0) ov_spstyle = st;
    if(k == 0){ ov_cur = (unsigned char)((ov_cur + rows - 1) % rows); return 1; }
    if(k == 1){ ov_cur = (unsigned char)((ov_cur + 1) % rows); return 1; }
    if(k < 2 || k > 4) return 1;
    if(ov_cur == 0){                                   /* 캐릭터/유파 — 좌우 순환, A=다음 */
      int n = sp_style_count(); if(n <= 0) return 1;
      if(k == 2)             ov_spstyle = (ov_spstyle + n - 1) % n;
      if(k == 3 || k == 4)   ov_spstyle = (ov_spstyle + 1) % n;
    }else if(k != 4){                                  /* 진입/실행은 A 전용 — 방향키 오조작 방지 */
      return 1;
    }else if(ov_cur <= ss2sp_slot_count()){            /* 슬롯 = 목록 열기 */
      ov_pickslot = ov_cur - 1;
      ov_page = 2;
      ov_cur = (unsigned char)(sp_get(ov_spstyle, ov_pickslot) + 1);
    }else if(ov_cur == ss2sp_slot_count() + 1){        /* 기본 배치로 되돌리기 */
      sp_resetslots();
    }else{                                             /* ← 돌아가기 */
      ov_page = 0; ov_cur = 0;
    }
    return 1;
  }
  {
    int rows0 = ov_n + 1;                              /* 마지막 줄 = SP 기술 배치 */
    if(k == 0){ ov_cur = (unsigned char)((ov_cur + rows0 - 1) % rows0); return 1; }
    if(k == 1){ ov_cur = (unsigned char)((ov_cur + 1) % rows0); return 1; }
    if(ov_cur == ov_n){
      if(k == 4){                                      /* 진입은 A 전용 */
        int st = sp_cur(); if(st >= 0) ov_spstyle = st;
        ov_page = 1; ov_cur = 0;
      }
      return 1;
    }
  }
#else
  if(k == 5){ ov_on = 0; return 1; }
  if(k == 0){ ov_cur = (unsigned char)((ov_cur + ov_n - 1) % ov_n); return 1; }
  if(k == 1){ ov_cur = (unsigned char)((ov_cur + 1) % ov_n); return 1; }
#endif
  it = &ov_it[ov_cur];
  if(!it->v) return 1;
  if(it->max == 1){                     /* 켬끔 — A 전용. 십자 오조작으로 안 뒤집힌다(제보) */
    if(k == 4) *it->v ^= 1;
    return 1;
  }
  if(k == 2)            *it->v = (unsigned char)((*it->v + it->max) % (it->max + 1));
  if(k == 3 || k == 4)  *it->v = (unsigned char)((*it->v + 1) % (it->max + 1));
  return 1;
}
static const char *ov_bgname(int i){
  static const char *nm[8] = {"자동","구간 1","구간 2","구간 3","구간 4","구간 5","구간 6","격자"};
  return nm[i & 7];
}
void ss2comm_overlay_draw(uint16_t *fb, int pitch_px, int w, int h){
  int bw, bh, bx, by, x, y, i, rows;
  if(!ov_on || !ov_n || !fb) return;
  rows = ov_n;
#ifdef SS2OV_SP
  rows = (ov_page == 2) ? sp_move_count(ov_spstyle) + 1
       : (ov_page == 1) ? ov_sp_rows() : ov_n + 1;
#endif
  bw = w - 12; bh = 17 + rows*13 + 4;
  bx = 6; by = (h - bh) / 2; if(by < 0) by = 0;
  for(y = 0; y < bh && by + y < h; y++)
    for(x = 0; x < bw; x++){
      uint16_t *p = &fb[(by+y)*pitch_px + bx + x];
      if(y==0 || y==bh-1 || x==0 || x==bw-1) *p = COL_GOLD;
      else *p = (uint16_t)((*p >> 3) & 0x18E3);
    }
#ifdef SS2OV_SP
  if(ov_page == 2){
    char t[64], buf[96];
    snprintf(t, sizeof t, "%s — 기술 고르기 (A 선택)", ov_slotname(ov_pickslot));
    draw_line11(fb, pitch_px, bx, bx+bw, t, t+strlen(t), by+3, 0, h, 99, COL_GOLD, 0);
    for(i = 0; i < rows; i++){
      if(i == 0) snprintf(buf, sizeof buf, "— 비움 —");
      else{
        char nt[16], ar[48];
        sp_nota(ov_spstyle, i - 1, nt, sizeof nt);
        ov_nota_arrows(nt, ar, sizeof ar);
        snprintf(buf, sizeof buf, "%s %s%s", ar, sp_move_name(ov_spstyle, i - 1),
                 (sp_move_flags(ov_spstyle, i - 1) & 4) ? "(공중)" : "");
      }
      draw_line11(fb, pitch_px, bx, bx+bw, buf, buf+strlen(buf), by+17+i*13, 0, h, 99,
                  (i == ov_cur) ? COL_GOLD : 0xDEDB, 0);
    }
    return;
  }
  if(ov_page == 1){
    char t[64], buf[96];
    int st = sp_cur(); if(st >= 0) ov_spstyle = st;
    snprintf(t, sizeof t, "SP 배치  (B 돌아가기)");
    draw_line11(fb, pitch_px, bx, bx+bw, t, t+strlen(t), by+3, 0, h, 99, COL_GOLD, 0);
    for(i = 0; i < rows; i++){
      if(i == 0){
        char sl[40]; sp_label(ov_spstyle, sl, sizeof sl);
        snprintf(buf, sizeof buf, "캐릭터 : %s", sl);
      }else if(i <= ss2sp_slot_count()){
        int mv = sp_get(ov_spstyle, i - 1);
        if(mv < 0) snprintf(buf, sizeof buf, "%s : — 비움 —", ov_slotname(i - 1));
        else{
          char nt[16], ar[48];
          sp_nota(ov_spstyle, mv, nt, sizeof nt);
          ov_nota_arrows(nt, ar, sizeof ar);
          snprintf(buf, sizeof buf, "%s:%s %s%s", ov_slotname(i - 1), ar,
                   sp_move_name(ov_spstyle, mv),
                   (sp_move_flags(ov_spstyle, mv) & 4) ? "(공중)" : "");
        }
      }else if(i == ss2sp_slot_count() + 1) snprintf(buf, sizeof buf, "기본 배치로 되돌리기");
      else snprintf(buf, sizeof buf, "← 빠른 설정으로");
      draw_line11(fb, pitch_px, bx, bx+bw, buf, buf+strlen(buf), by+17+i*13, 0, h, 99,
                  (i == ov_cur) ? COL_GOLD : 0xDEDB, 0);
    }
    return;
  }
#endif
  { const char *t = "빠른 설정 v" SS2COMM_VERSION "  (B 닫기)";
    draw_line11(fb, pitch_px, bx, bx+bw, t, t+strlen(t), by+3, 0, h, 99, COL_GOLD, 0); }
  for(i = 0; i < ov_n; i++){
    char buf[72]; const char *vs;
    ss2ovitem *it = &ov_it[i];
    if(it->kind == 1)      vs = ss2comm_speaker_name(*it->v);
    else if(it->kind == 2) vs = ov_bgname(*it->v);
    else                   vs = *it->v ? "켬" : "끔";
    snprintf(buf, sizeof buf, "%s : %s", it->name, vs);
    draw_line11(fb, pitch_px, bx, bx+bw, buf, buf+strlen(buf), by+17+i*13, 0, h, 99,
                (i == ov_cur) ? COL_GOLD : 0xDEDB, 0);
  }
#ifdef SS2OV_SP
  { const char *t = "SP 기술 배치 →";
    draw_line11(fb, pitch_px, bx, bx+bw, t, t+strlen(t), by+17+ov_n*13, 0, h, 99,
                (ov_cur == ov_n) ? COL_GOLD : 0xDEDB, 0); }
#endif
}

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
static const ss2glyph11 SS2FONT11_X[] = {
  {0x004B,7,{0,0x8800,0x9000,0xA000,0xC000,0xC000,0xA000,0x9000,0x8800,0x8800,0,0,0}},  /* K */
  {0x0058,7,{0,0x8800,0x8800,0x5000,0x2000,0x2000,0x5000,0x8800,0x8800,0,0,0,0}},       /* X */
};
static const ss2glyph11 *glyph11_of(unsigned cp){
  int lo=0, hi=SS2FONT11_N-1;
  while(lo<=hi){ int mid=(lo+hi)>>1;
    if(SS2FONT11[mid].cp==cp) return &SS2FONT11[mid];
    if(SS2FONT11[mid].cp<cp) lo=mid+1; else hi=mid-1; }
  { unsigned i; for(i=0;i<sizeof SS2FONT11_X/sizeof SS2FONT11_X[0];i++)
      if(SS2FONT11_X[i].cp==cp) return &SS2FONT11_X[i]; }
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

/* ── 심판 ────────────────────────────────────────────────────────
   **해설창을 같이 쓴다** — 별도 칸도, 게임 위 오버레이도 없다. 심판이 말할 동안
   해설창은 쿠로코의 것이고, 캐릭터챗은 후순위로 기다린다(팝 정지 + 현재 줄 양보).
   (제보 이력: 별도 칸 → 오버레이 → 「둘 다 나오니 정신사납다. 쿠로코를 올리고
   온오프 하자. 쿠로코 온이면 캐릭터챗은 후순위」) */
static int ref_drawn_now;                        /* 이번 프레임에 실제로 그렸나 — 앱 32비트 경로가 묻는다 */
static void draw_ref_strip(uint16_t *fb, int pitch_px, int w, int h, int bandTop){
  const char *seg[BOX_MAXL+1], *t, *end;
  int x, y, top, bot, tx0, x1, maxw, nl, i, lh, ty, show;
  ref_drawn_now = 0;
  top = bandTop ? 0 : h;            /* 해설창 그 자리 — 심판이 말할 동안은 쿠로코의 창이다 */
  bot = top + SS2_REF_H;
  if(!ref_has && !ref_shown) return;
  if(ref_has){                                   /* 이번 프레임에 세운다 */
    if(cm_f < ref_next) return;
    ref_has = 0; ref_shown = cm_f; ref_next = cm_f + 150;
    curline[0] = 0;                              /* 캐릭터챗은 후순위 — 하던 말을 접는다 */
  }
  if(cm_f - ref_shown > (unsigned)(ref_ttl>0?ref_ttl:REF_TTL)){ ref_shown = 0; return; }
  t = ref_text; if(!*t) return;
  ref_drawn_now = 1;
  /* 구령의 「반짝」 = 해설창 강조와 같은 문법 — 첫 프레임 금빛으로 꽉 번쩍,
     몇 프레임에 걸쳐 식으며 좌우로 쿵 흔들린다. 점멸은 안 한다
     (제보: 「반짝 = 노랗게 되면서 쿵. 지금은 깜빡이고, 끝나고도 더 깜빡임」). */
  { unsigned age = cm_f - ref_shown;
    uint16_t bg = 0x0000;
    if(ref_flash){
      if     (age < 1) bg = COL_GOLD;
      else if(age < 2) bg = 0x8C40;
      else if(age < 5) bg = 0x3000;
    }
    for(y=top; y<bot; y++) for(x=0; x<w; x++) fb[y*pitch_px+x] = bg;
  }
  end = t + strlen(t);
  if(ref_ok){                                    /* 쿠로코 초상 32x32 */
    int fx=2, fy=top, a, b;
    if(ref_flash && cm_f-ref_shown < 6) fx += SS2_SHAKE[cm_f-ref_shown];
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
  if(ref_flash && cm_f-ref_shown < 6) tx0 += SS2_SHAKE[cm_f-ref_shown];  /* 쿵 — 글도 튄다 */
  nl   = wrap11(t, end, maxw, seg);
  lh   = BOX_LINE_H;
  if(nl > 2){ nl = wrap8(t, end, maxw, seg); lh = 9; }
  ty   = top + (SS2_REF_H - nl*lh)/2;
  show = ref_flash ? 999 : 2 + (int)(cm_f - ref_shown)*2;   /* 구령은 타자 없이 통째로 */
  { uint16_t rc = ref_flash ? COL_GOLD : COL_REF;           /* 구령은 금빛 */
  for(i=0;i<nl && show>0;i++){
    int drawn = (lh==9)
      ? draw_line  (fb,pitch_px,tx0,x1,seg[i],seg[i+1], ty + i*lh + 1, top, bot, show, rc, ref_flash)
      : draw_line11(fb,pitch_px,tx0,x1,seg[i],seg[i+1], ty + i*lh,     top, bot, show, rc, 0);
    show -= drawn;
  } }
}

/* 구령이 선 프레임 — 앱이 이걸 보고 40ms 진동 한 번(쿵)을 울린다. 읽으면 지워진다. */
int ss2comm_thump(void){ int t = ref_thump_pend; ref_thump_pend = 0; return t; }

/* 예전 게임-위-오버레이 시절의 조회 — 심판이 해설창으로 올라와서 이제 항상 0.
   (앱의 밴드 32줄 변환이 심판까지 그대로 덮는다.) ABI 는 남겨 둔다. */
int ss2comm_ref_overlay(void){ return 0; }
/* 심판 온오프 — 앱 설정에서 토글한다. 끄면 서 있던 줄도 접는다. */
void ss2comm_set_ref(int on){
  ref_enabled = !!on;
  if(!ref_enabled){ ref_has = 0; ref_shown = 0; }
}
/* 캐릭터챗 온오프 — 심판과 따로 끈다. 조합 넷: 둘 다 / 캐릭터만 / 쿠로코만 / 무음 */
void ss2comm_set_chat(int on){ chat_enabled = !!on; }

/* ── 기술명 토스트 — 해설층과 독립. svcsp 가 기술을 쏠 때 한 줄 띄운다 ── */
static char toast_txt[64];
static int  toast_left;
void ss2comm_toast(const char *t, int frames){
  if(!t || !*t) return;
  snprintf(toast_txt, sizeof toast_txt, "%s", t);
  toast_left = frames;
}
static void toast_render(uint16_t *fb, int pitch_px, int w, int h){
  int tw, x0, x1, x, y, y0, band;
  if(toast_left <= 0) return;
  toast_left--;
  tw = line_w11(toast_txt, toast_txt + sizeof toast_txt);
  if(tw <= 0 || tw > w) return;
  /* 위 띠 모드(4)면 게임 그림이 띠 높이만큼 내려가 있다 — 그 아래, HP 바 아래에 띄운다.
     (띠 안에 그리면 띠 지우기가 바로 덮는다 — 실측) */
  band = (cm_on && cm_draw==4) ? SS2_BAND_H : 0;
  y0 = band + 30;
  x0 = (w - tw)/2; x1 = x0 + tw;
  for(y=y0-2; y<y0+13 && y<h+band; y++)          /* 반투명 띠 (RGB565 절반 감광) */
    for(x=x0-5; x<x1+5; x++)
      if(x>=0 && x<w && y>=0) fb[y*pitch_px+x] = (uint16_t)((fb[y*pitch_px+x]>>1) & 0x7BEF);
  draw_line11(fb, pitch_px, 0, w, toast_txt, toast_txt + sizeof toast_txt,
              y0, 0, h+band, 999, 0xFFFF, 0);
}

void ss2comm_draw(uint16_t *fb, int pitch_px, int w, int h){
  const char *line, *end, *seg[BOX_MAXL+1];
  int age=0, x, y, top, bot, mood, hit, spk, tx0, x1, maxw, show, i, nl, boxh, ty, lh;
  int band, bandTop, small;
  uint16_t col;
  if(!fb) return;
  toast_render(fb, pitch_px, w, h);              /* 해설 온오프와 무관하게 그린다 */
  if(!cm_on || !cm_draw) return;
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
  if(band){
    draw_ref_strip(fb, pitch_px, w, h, bandTop);   /* 심판이 먼저 — 해설창을 같이 쓴다 */
    if(ref_drawn_now) return;                      /* 이 프레임의 창은 쿠로코의 것 */
    /* 안무(팻말 대기·인트로)가 도는 동안 캐릭터 잔상 밴드를 그리지 않는다 —
       구호 사이마다 캐릭터 얼굴이 들어왔다 나가며 껌뻑였다(제보). */
    if(ref_enabled && (ref_has || plate_at || plate2_at ||
                       (p_mode==MD_MENU && p_scr>=8 && intro_refok))) return;
  }
  if(!line || age > CM_TTL){
    /* 예전에는 여기서 창을 통째로 꺼 버렸다 — 화자는 그대로인데 2.5초마다
       껌뻑인다는 제보. 띠 모드는 **마지막 말을 흐린 글씨로 계속 걸어 두고**
       새 말이 오면 그때 갈아끼운다. 화면 안 상자는 게임을 가리므로 예전대로 진다. */
    if(!band) return;
    if(!line) line = "";                       /* 아직 아무 말도 없다 — 초상과 틀만 */
    age = CM_TTL + 1;
  }

  end  = line + strlen(line);
  mood = (cur_ev>=0 && cur_ev<EV_N) ? EVMOOD[cur_ev] : 0;
  hit  = (cur_ev>=0 && cur_ev<EV_N) ? EVHIT[cur_ev]  : 0;
  col  = hit ? COL_GOLD : COL_WHITE;
  if(age > CM_TTL){ col = 0x8410; hit = 0; mood = 0; }   /* 지난 말 — 흐린 회색으로 걸어만 둔다 */
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

/* ── 양옆 아트웍 기둥 — 넓은 화면에서 게임 좌우의 빈 공간을 채운다
   (제보: 「양쪽 아트웍 있으면 이쁘겠다」 — 실기 사진의 검은 기둥).
   그림은 전부 실행 중에 사용자 롬에서 굽는다(선택 아이콘·심판 초상) —
   배포물에는 주소와 팔레트뿐, 그림은 없다.
   전투 중: 왼쪽 = 내 캐릭터, 오른쪽 = 상대 (표 밖 개체는 「마물」 글자만).
   그 밖:   왼쪽 = 해설자, 오른쪽 = 심판.
   한 번에 기둥 하나를 그린다 — 32비트 화면은 기둥별로 변환해 얹기 좋게. */
/* 기둥 바탕감 — **스테이지 타일**로 굽는다 (제보: 「실시간 반영은 에바다, 타일을」).
   K1GE 스크롤2(배경면) 타일맵에서 **보이는 창 좌우 바깥 8칸씩**을 떼어
   실제 화면 밖 스테이지 그림을 정적으로 깐다. 매치가 설 때만 다시 굽는다.
   타일 형식은 tiletool 로 푼 그대로: 엔트리 = 타일9비트|팔레트4|V플립|H플립,
   패턴 2bpp 16바이트, 팔레트 RGB444 (SCR2 = 0x8300). 배포물엔 그림이 없다. */
/* 대형 일러(96x96) — 로스터별 롬 주소표에서 실행 중에 굽는다. 없으면 아이콘으로.
   (제보: 「롬 안에 주소를 서치해서 띄우자」 — 아이콘·쿠로코와 같은 방식의 확장) */
static const ss2artdef SS2ART[SS2COMM_ART_N] = SS2COMM_ART_INIT;
/* 슬롯별 유파 — 1=수라, 0=나찰 (컬렉션 목록 헤더 실측). 고른 유파의 일러만 돈다. */
static const unsigned char art_su[SS2COMM_ART_N] = {1,1,0,0,0,1,1,0,0,1,1,0,0,0,1,1,0,0,1,1,1,0,0,1,1,0,0,1,1,1,0,0,1,1,0,0,0,1,1,0,0,1,1,0,0,0,1,1,0,0,1,1,1,0,0,1,1,0,0,1};
/* 기둥 2칸 캐시 — 캐릭터/변형이 바뀔 때만 굽는다. 변형은 콜될 때 랜덤 1회, 유지. */
static uint16_t      art_px[2][96*96];
static signed char   art_ch[2]  = {-1,-1};   /* 칸에 구운 캐릭터 */
static signed char   art_sty[2] = {-1,-1};   /* 칸에 구운 유파 (수라0/나찰1 — BLK 무기비트) */
static unsigned char art_var[2], art_okc[2];
static int art_bake(int side, int ch, int var){
  const ss2artdef *A; int i, ry, rx; unsigned sum, a0;
  int slot = ch*4 + var;
  if(ch < 0 || ch > 15 || !cm_rom || slot >= SS2COMM_ART_N) return 0;
  A = &SS2ART[slot];
  if(!A->ok) return 0;
  /* 검증은 첫 **유의미** 타일로 — 빈 타일(주소 0 근처, 합 0)은 판별력이 없다 */
  a0 = 0;
  for(i = 0; i < 144 && !a0; i++) if(A->c[i].a >= 4096) a0 = A->c[i].a;
  if(!a0 || a0 + 16 > cm_romlen) return 0;
  for(sum = 0, i = 0; i < 16; i++) sum = (sum + cm_rom[a0 + i]) & 0xFFFF;
  if(sum != A->sum) return 0;                  /* 다른 롬 — 일러 생략 */
  for(i = 0; i < 144; i++){
    unsigned a = A->c[i].a; int pn = A->c[i].pf >> 2;
    int vf = (A->c[i].pf >> 1) & 1, hf = A->c[i].pf & 1;
    int ty = i / 12, tx = i % 12;
    for(ry = 0; ry < 8; ry++){
      int sy = vf ? 7 - ry : ry; unsigned w2 = 0;
      if(a && a + 16 <= cm_romlen) w2 = cm_rom[a + sy*2] | (cm_rom[a + sy*2 + 1] << 8);
      for(rx = 0; rx < 8; rx++){
        int sx = hf ? 7 - rx : rx;
        int ci = (w2 >> ((7 - sx) * 2)) & 3;
        /* 색인 0 은 K1GE 투명 — 카드 화면에선 흰 종이가 비친다 */
        art_px[side][(ty*8 + ry)*96 + tx*8 + rx] =
          ci ? pal12_to565(A->pal[pn][ci]) : (A->ok == 2 ? 0x0000 : 0xFFFF);
      }
    }
  }
  /* 간다라(슬롯 60)만 두 겹이다 — 몸은 스크롤면, 얼굴은 스프라이트라
     바탕을 다 깐 뒤 얼굴을 색인0 투명으로 덧그린다 (검은 상자 방지) */
  if(slot == 60){
    for(i = 0; i < 144; i++){
      unsigned a = SS2ART_GAND_OV[i].a; int pn = SS2ART_GAND_OV[i].pf >> 2;
      int vf = (SS2ART_GAND_OV[i].pf >> 1) & 1, hf = SS2ART_GAND_OV[i].pf & 1;
      int ty = i / 12, tx = i % 12;
      if(a < 4096 || a + 16 > cm_romlen) continue;
      for(ry = 0; ry < 8; ry++){
        int sy = vf ? 7 - ry : ry;
        unsigned w2 = cm_rom[a + sy*2] | (cm_rom[a + sy*2 + 1] << 8);
        for(rx = 0; rx < 8; rx++){
          int sx = hf ? 7 - rx : rx;
          int ci = (w2 >> ((7 - sx) * 2)) & 3;
          if(ci) art_px[side][(ty*8 + ry)*96 + tx*8 + rx] = pal12_to565(A->pal[pn][ci]);
        }
      }
    }
  }
  return 1;
}
static const uint16_t *art_get(int side, int ch, int *fx_out){
  int sty;
  if(ch < 0 || ch > 15) return 0;   /* 15 = 간다라 (슬롯 60, 유파 무관) */
  /* 이 캐릭터가 고른 유파 — BLK 무기비트 (0=수라 1=나찰, 유파선택 순서 기준) */
  sty = (rd(side ? OFF_BLK2 : OFF_BLK1) / 8) & 1;
  if(art_ch[side] != ch || art_sty[side] != (signed char)sty){
    int pool[4], np = 0, k, s0;
    for(k = 0; k < 4 && ch*4 + k < SS2COMM_ART_N; k++){
      int slot = ch*4 + k;
      if(SS2ART[slot].ok && (ch == 15 || art_su[slot] == (sty ? 0 : 1))) pool[np++] = k;
    }
    if(!np)   /* 그 유파에 쓸 그림이 없다 — 아무 유효 변형으로 */
      for(k = 0; k < 4 && ch*4 + k < SS2COMM_ART_N; k++) if(SS2ART[ch*4 + k].ok) pool[np++] = k;
    art_okc[side] = 0;
    if(np){
      s0 = (int)(rnd() % (unsigned)np);           /* 콜될 때 랜덤 1회 */
      for(k = 0; k < np && !art_okc[side]; k++){
        int v = pool[(s0 + k) % np];
        if(art_bake(side, ch, v)){ art_var[side] = (unsigned char)v; art_okc[side] = 1; }
      }
    }
    art_ch[side] = (signed char)ch; art_sty[side] = (signed char)sty;
  }
  if(!art_okc[side]) return 0;
  if(fx_out) *fx_out = SS2ART[ch*4 + art_var[side]].fx;
  return art_px[side];
}
void ss2comm_side(uint16_t *fb, int pitch_px, int w, int h, int right){
  int x, y, ch, battle;
  const char *nm;
  if(!fb || w <= 2 || h <= 0 || !cm_on) return;
  if(!face_built) build_faces();
  battle = (st_myChar >= 0 || st_oppChar >= 0);   /* 마지막 대진을 계속 — 메뉴에서도 안 갈아치운다 */
  {
    int rc;
    if(battle){
      rc = right ? st_oppChar : st_myChar;
      nm = (rc >= 0) ? CHARNAME[rc] : (right ? (st_oppGand ? "간다라" : "마물") : 0);
      ch = (rc >= 0 && rc < 15) ? ROST2SPK[rc]
         : (right && rc < 0 && st_oppGand) ? 15 : -1;   /* 간다라 = 슬롯 60 */
    }else{
      ch = -1; nm = 0;   /* 아직 아무 판도 못 봤다 — 무늬만. 해설자를 바꿔도 기둥은 안 바뀐다 */
    }
  }
  /* 바탕 — 어두운 마름모 무늬 (스테이지 구간 배경은 구리다는 제보로 폐지.
     매치 전 장식 아트가 정해지면 이 자리에 얹는다). */
  for(y = 0; y < h; y++)
    for(x = 0; x < w; x++){
      uint16_t v = 0x0841;
      if(((x + y) & 15) == 7 || ((x - y) & 15) == 7) v = 0x18E3;
      fb[y*pitch_px + x] = v;
    }
  /* 게임과 맞닿는 모서리에 금줄 */
  { int gx  = right ? 0 : w - 1;
    int gx2 = right ? 1 : w - 2;
    for(y = 0; y < h; y++){
      fb[y*pitch_px + gx]  = COL_GOLD;
      fb[y*pitch_px + gx2] = 0x4200;
    }
  }
  /* 초상 — 대형 일러(롬 주소표) 우선, 없으면 32x32 아이콘을 키운다 */
  { const uint16_t *px = 0; const unsigned char *al = 0;
    const uint16_t *art = 0;
    int size, ox, oy;
    int rc2 = -1, art_fc = 48;
    if(battle) rc2 = right ? (st_oppGand ? 15 : st_oppChar) : st_myChar;
    art = art_get(right ? 1 : 0, rc2, &art_fc);
    if(ch >= 0 && ch < SS2COMM_SPK_N && icon_ok[ch]){ px = icon_px[ch]; al = icon_a[ch]; }
    else if(ch == -2 && ref_ok){ px = ref_px; al = ref_a; }
    size = w - 8; if(size > 64) size = 64;
    ox = (w - size) / 2; oy = 14;
    if(art){
      /* 세로샷 — 2배 확대해 기둥을 위아래로 꽉 채우고 가로는 **얼굴 초점** 크롭
         (일괄 중앙이면 얼굴이 잘린다는 제보 — 그림마다 초점 x가 다르다) */
      int fc = art_fc;
      int sc = 2, aw = 96*sc, ah = 96*sc;
      int cx = fc*sc - w/2, cy = (ah - h) / 2;
      int pady = 0;
      /* 전황 연출 — 빈사 물들기·KO 명암 (판이 살아 있을 때만) */
      int live = (rd(OFF_MODE) == MD_BATTLE && rd(OFF_SCR) >= 8);
      int hpS  = right ? rd(OFF_HP2) : rd(OFF_HP1);
      int tt   = (live && !st_ko && hpS > 0 && hpS < 32) ? (32 - hpS) : 0;
      int gray = (live && st_ko && hpS == 0);   /* 진 쪽 = 체력 0 쪽 */
      /* 히트 충격 — 맞은 쪽 일러가 감쇠하며 좌우로 쿵, 큰 타격 첫 두 프레임은 백섬광 */
      int shk   = live ? st_shk[right ? 1 : 0] : 0;
      int flash = (shk >= 11);
      if(shk){
        int amp = (shk + 3) / 4;                       /* 3 → 2 → 1 감쇠 */
        cx += (cm_f & 2) ? amp : -amp;
        cy += (shk > 6 && (cm_f & 4)) ? 1 : 0;
      }
      if(cx > aw - w) cx = aw - w;
      if(cx < 0) cx = 0;
      if(cy > ah - h && ah > h) cy = ah - h;
      if(cy < 0){ pady = (h - ah) / 2; cy = 0; }
      for(y = 0; y < h; y++){
        int ay = y - pady;
        for(x = 0; x < w; x++){
          uint16_t v = 0x0841;
          if(ay >= 0 && ay < ah)
            v = art[((ay + cy)/sc)*96 + (x + cx)/sc];
          if(gray){
            int l5 = (((v >> 11) & 31)*2 + ((v >> 5) & 63) + (v & 31)*2) / 6;
            v = (uint16_t)((l5 << 11) | ((l5*2) << 5) | l5);
          }else if(tt){
            int g6 = (v >> 5) & 63, b5 = v & 31;
            g6 -= g6 * tt / 34; b5 -= b5 * tt / 34;
            v = (uint16_t)((v & 0xF800) | (g6 << 5) | b5);
          }
          if(flash) v = (uint16_t)(((v >> 1) & 0x7BEF) + 0x7BEF);   /* 백섬광 — 흰색 쪽 절반 */
          fb[y*pitch_px + x] = v;
        }
      }
      /* 금줄 다시 — 일러가 덮었다. (매치포인트 금테·집중선 장식은 제보로 폐지 —
         전황은 빈사 틴트·KO 흑백·히트 충격만 말한다) */
      { int gx  = right ? 0 : w - 1;
        int gx2 = right ? 1 : w - 2;
        for(y = 0; y < h; y++){
          fb[y*pitch_px + gx]  = COL_GOLD;
          fb[y*pitch_px + gx2] = 0x4200;
        }
      }
      /* 이름표 없음 — 일러만 (제보: 「이름 빼」) */
      return;
    }
    if(px){
      for(y = 0; y < size && oy + y < h; y++)
        for(x = 0; x < size; x++){
          int p = (y*32/size)*32 + (x*32/size);
          /* 카드는 불투명 — 투명 픽셀은 검정 바탕으로 (뒤 타일이 비쳤다는 제보) */
          fb[(oy + y)*pitch_px + ox + x] = al[p] ? px[p] : 0x0000;
        }
    }
    if(px || art){
      /* 얇은 틀 — 카드가 바탕에서 뜬다 */
      for(x = ox - 1; x <= ox + size; x++){
        if(x < 0 || x >= w) continue;
        if(oy - 1 >= 0)      fb[(oy - 1)*pitch_px + x]    = 0x8C40;
        if(oy + size < h)    fb[(oy + size)*pitch_px + x] = 0x8C40;
      }
      for(y = oy - 1; y <= oy + size && y < h; y++){
        if(y < 0) continue;
        if(ox - 1 >= 0)      fb[y*pitch_px + ox - 1]    = 0x8C40;
        if(ox + size < w)    fb[y*pitch_px + ox + size] = 0x8C40;
      }
    }
    /* 이름 — 초상(또는 그 자리) 아래 */
    if(nm && *nm)
      draw_line11(fb, pitch_px, 2, w - 2, nm, nm + strlen(nm),
                  oy + size + 6, 0, h, 99, COL_GOLD, 0);
  }
}
