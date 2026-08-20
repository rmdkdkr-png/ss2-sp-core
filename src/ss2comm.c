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
#define MD_BATTLE  241
#define MD_MENU    240
#define MD_QUOTE   197
#define MD_ENDING  199

enum {
  EV_START=0, EV_ROUND, EV_KO, EV_KOED, EV_DKO, EV_PERFECT, EV_COMEBACK, EV_QUICK,
  EV_LOW1, EV_LOW2, EV_REVERSAL, EV_WINTALK, EV_LOSETALK, EV_SURV, EV_SURVEND,
  EV_STAGE, EV_QUOTE, EV_ENDING, EV_CHARSEL, EV_STYLESEL, EV_CARDSEL, EV_TITLE,
  EV_MUSE_B, EV_MUSE_M, EV_IDLE, EV_N
};

/* ── 대사표: [화자][이벤트] 최대 3변형. %s = 매치업, %d = 숫자 ── */
#define V3(a,b,c) {a,b,c}
static const char *LINES[4][EV_N][3] = {
 { /* 0 하오마루 — 호쾌한 술꾼 검객 */
  V3("%s… 좋은 승부다!",0,0), V3("간다!!","자, 다시!",0),
  V3("핫핫하!! 승부 났다!!","훌륭한 마무리다!!",0), V3("…좋은 승부였다. 한잔 하러 가지",0,0),
  V3("둘 다 뻗었나! 하하!!",0,0), V3("완승이다!! 흠집 하나 없군, 핫핫하!!",0,0),
  V3("빈사에서 뒤집었다!! 이게 승부다!!",0,0), V3("순살이다!! 눈 깜짝할 새군!",0,0),
  V3("위험하다… 술이 확 깨는군!",0,0), V3("끝장을 내라!!",0,0),
  V3("오오, 판이 뒤집혔다!!",0,0),
  V3("좋은 검이었다. 다음 상대가 기다린다","이런 승부라면 몇 번이고 좋다","술이 달겠군. 자, 계속 가자!"),
  V3("진 판에서 배우는 게 더 많은 법이다","검이 무뎠나… 다시 잡아라","한 잔 걸치고 다시 하자"),
  V3("%d연승! 오늘 검이 좋군!","%d연승!! 대적할 자가 없느냐!!",0),
  V3("%d명 베고 여기까지… 훌륭한 기록이다!",0,0),
  V3("%d연전째다. 계속 가자!","%d연전째. 손이 완전히 풀렸겠군",0),
  V3("무슨 말을 하는 거냐, 저 녀석","한마디 하는군. 말보다 검이다",0),
  V3("끝까지 베고 올라갔군!! 오늘은 내가 술을 사마!!",0,0),
  V3("누구와 겨루려느냐!",0,0), V3("수라냐 나찰이냐… 고민되는군",0,0),
  V3("카드라… 나는 힘만 있으면 된다만",0,0), V3("사무라이의 혼이 부른다… 한판 하지!",0,0),
  V3("…검이 근질거리는군","호흡을 고르는 것도 검술이다",0),
  V3("…술이 떨어졌군","좋은 승부가 그립군","심심하면 한판 더 하자!"),
  V3("서로 노려보는군… 술이 식겠다","먼저 가라! 사양할 것 없다!",0),
 },
 { /* 1 나코루루 — 상냥한 자연의 수호자 */
  V3("%s… 좋은 시합이 되길",0,0), V3("힘내세요!","다시 한 번요!",0),
  V3("…승부가 났네요. 수고하셨어요",0,0), V3("아… 다음엔 꼭 이겨요",0,0),
  V3("둘 다…!? 이런 일도 있네요",0,0), V3("완승이에요! 하나도 안 다쳤어요!",0,0),
  V3("포기하지 않아서… 이겼어요!! 감동이에요!",0,0), V3("벌써 끝났어요…! 굉장해요!",0,0),
  V3("위험해요… 조심하세요!",0,0), V3("조금만 더예요!",0,0),
  V3("흐름이 바뀌었어요!",0,0),
  V3("다치지 않아서 다행이에요","좋은 승부였어요. 숨 좀 고르세요",0),
  V3("괜찮아요, 다음이 있잖아요","조금 쉬었다 해요…",0),
  V3("%d연승이에요! …무리는 하지 마세요?",0,0),
  V3("%d연승에서 멈췄어요… 정말 수고했어요!",0,0),
  V3("%d연전째예요! 힘내요","%d연전째예요. 지치진 않으셨어요?",0),
  V3("인사를 나누고 있어요","무슨 말을 할까요…?",0),
  V3("끝까지 해내셨어요…! 정말 축하해요!!",0,0),
  V3("누구랑 싸우실 건가요?",0,0), V3("수라와 나찰… 어느 쪽이든 응원할게요",0,0),
  V3("카드는 신중하게 고르세요",0,0), V3("어서 오세요! 오늘도 힘내요",0,0),
  V3("…바람이 조용해요","다치지 않게, 천천히요",0),
  V3("…오늘 날씨가 좋네요","리무루루는 잘 있을까요","자연의 소리가 들려요…"),
  V3("숨을 고르는 중이네요…","서로 거리를 재고 있어요",0),
 },
 { /* 2 한조 — 과묵한 닌자 두목 */
  V3("%s. 시작하지",0,0), V3("…간다","…다시",0),
  V3("…승부가 났다",0,0), V3("…패배도 수행이다",0,0),
  V3("동귀어진인가",0,0), V3("…완승. 흠잡을 데가 없다",0,0),
  V3("…사지에서 살아 돌아왔군. 훌륭하다",0,0), V3("…일섬. 이상적인 승부다",0,0),
  V3("물러설 곳은 없다",0,0), V3("끝내라",0,0),
  V3("…형세 역전",0,0),
  V3("…이겼다고 멈추지 마라","…다음 적은 더 강하다",0),
  V3("…패인을 새겨라. 그것이 수행이다","…다시 서라. 그뿐이다",0),
  V3("…%d연승. 인정하마",0,0),
  V3("…%d명 격파. 새겨 둘 기록이다",0,0),
  V3("…%d연전째. 간다","…%d연전. 몸이 익었을 때다",0),
  V3("…말은 검으로 하는 것이다","…각오를 말하는군",0),
  V3("…완주. 수행의 끝을 보았군. 훌륭하다",0,0),
  V3("상대를 골라라",0,0), V3("수라인가, 나찰인가",0,0),
  V3("…쓸 만한 카드인가",0,0), V3("…왔는가. 시작하지",0,0),
  V3("……","…호흡을 죽여라",0),
  V3("……","…그림자는 서두르지 않는다","…밤이 길다"),
  V3("…정적. 나쁘지 않다","기다리는 것도 병법이다",0),
 },
 { /* 3 갈포드 — 열혈 정의의 닌자 */
  V3("%s! 파이트다!",0,0), V3("고고!!","다시 가자!",0),
  V3("저스티스 이즈 윈!!",0,0), V3("이럴 수가…! 다음엔 이긴다!",0,0),
  V3("둘 다 쓰러졌다!?",0,0), V3("퍼펙트다!! 파피, 봤냐!!",0,0),
  V3("대역전!! 이게 히어로의 각본이다!!",0,0), V3("초스피드 피니시!! 번개 같았다!",0,0),
  V3("위기다! 하지만 포기는 없다!",0,0), V3("마무리다!!",0,0),
  V3("대역전이다아!!",0,0),
  V3("나이스 파이트! 파피도 꼬리 흔든다!","이게 정의의 승리다!",0),
  V3("히어로는 넘어져도 다시 선다!","다음 판에서 갚아주자!",0),
  V3("%d연승!! 연승 가도다!!",0,0),
  V3("%d연승 스트릭 종료…! 멋진 도전이었다!",0,0),
  V3("%d연전째다! 다음 도전자, 어서 와라!","벌써 %d연전! 엔진 다 데워졌다!",0),
  V3("명대사 타임이다!","뭐라고 하는 거냐! 두근두근!",0),
  V3("클리어다!! 오늘의 히어로는 너다!!",0,0),
  V3("누굴 고를 거냐! 두근두근이다!",0,0), V3("수라냐 나찰이냐!",0,0),
  V3("카드 타임! 뭐가 나올까!",0,0), V3("타이틀이다! 버튼을 눌러 시작이다!",0,0),
  V3("파피, 대기다!","슬슬 한 방 가자!",0),
  V3("파피, 앉아! …좋아, 착하지","심심한데 한판 더 어때!","오늘도 평화롭군!"),
  V3("정적이다… 폭풍 전의 고요인가!","파피도 하품하고 있다!",0),
 },
};

static const char *CHARNAME[15] = {
  "카즈키","소게츠","하오마루","겐주로","나코루루","리무루루","한조","갈포드",
  "아수라","샤를로트","모로즈미","우쿄","쥬베이","시키","유가"
};

/* ── 상태 ── */
static int  cm_on = 1, cm_spk = 0, cm_ready = 0;
static unsigned cm_f = 0;                 /* 프레임 카운터 */
static unsigned cd[EV_N];                 /* 이벤트별 쿨다운 만료 프레임 */
static int p_mode=-1, p_scr=-1, p_hp1=-1, p_hp2=-1, p_surv=-1, p_stage=-1;
static int st_ko=0, st_low1=0, st_low2=0, st_lead=0, st_rev=0;
static int st_lastStage=-1, st_roundStart=0, st_won=0;
static unsigned last_line_f = 0;          /* 마지막 발화 프레임 */
static char  outbuf[160];
static char  curline[160];
static unsigned cur_f = 0;
static int cur_ev = -1;            /* 표정·강조를 고르려고 마지막 이벤트를 남긴다 */
static unsigned rng = 2463534242u;

static unsigned rnd(void){ rng ^= rng<<13; rng ^= rng>>17; rng ^= rng<<5; return rng; }
static int rd(int off){ return RAM ? RAM[off] : 0; }
static int rd16(int off){ return RAM ? (RAM[off] | (RAM[off+1]<<8)) : 0; }

void ss2comm_set_enabled(int on){ cm_on = on ? 1 : 0; }
void ss2comm_set_speaker(int idx){ if(idx>=0 && idx<4) cm_spk = idx; }
void ss2comm_reset(void){
  int i; for(i=0;i<EV_N;i++) cd[i]=0;
  p_mode=p_scr=p_hp1=p_hp2=p_surv=p_stage=-1;
  st_ko=st_low1=st_low2=st_lead=st_rev=0; st_lastStage=-1; st_won=0;
  cm_ready=0; curline[0]=0; cur_f=0; cur_ev=-1; last_line_f=0;
}

/* 대사 한 줄 뽑기. n1/n2 는 %d, who 는 %s 자리에 들어간다. */
static const char *emit(int ev, unsigned cool, int num, const char *who){
  const char *cand[3]; int n=0, i; const char *fmt;
  if(!cm_on || ev<0 || ev>=EV_N) return 0;
  if(cd[ev] > cm_f) return 0;
  for(i=0;i<3;i++) if(LINES[cm_spk][ev][i]) cand[n++]=LINES[cm_spk][ev][i];
  if(!n) return 0;
  fmt = cand[rnd()%(unsigned)n];
  if(strstr(fmt,"%s") && who)      snprintf(outbuf,sizeof(outbuf),fmt,who);
  else if(strstr(fmt,"%d"))        snprintf(outbuf,sizeof(outbuf),fmt,num);
  else                             snprintf(outbuf,sizeof(outbuf),"%s",fmt);
  cd[ev] = cm_f + cool;
  last_line_f = cm_f;
  cur_ev = ev;
  snprintf(curline,sizeof(curline),"%s",outbuf);
  cur_f = cm_f;
  return outbuf;
}

const char *ss2comm_current(int *age){
  if(age) *age = (int)(cm_f - cur_f);
  return curline[0] ? curline : 0;
}

const char *ss2comm_frame(void){
  int mode, scr, hp1, hp2, a1, a2, surv, stage;
  const char *out = 0;
  cm_f++;
  if(!cm_on || !RAM) return 0;

  mode  = rd(OFF_MODE);  scr  = rd(OFF_SCR);
  hp1   = rd(OFF_HP1);   hp2  = rd(OFF_HP2);
  a1    = rd16(OFF_ACT1);a2   = rd16(OFF_ACT2);
  surv  = rd(OFF_SURV);  stage= rd(OFF_STAGE);
  (void)a2;

  if(!cm_ready){ /* 첫 프레임은 기준만 잡는다 */
    p_mode=mode; p_scr=scr; p_hp1=hp1; p_hp2=hp2; p_surv=surv; p_stage=stage;
    cm_ready=1; return 0;
  }

  /* ── 화면 전환 ── */
  if(mode==MD_QUOTE && p_mode!=MD_QUOTE)      out = emit(EV_QUOTE, 540, 0, 0);
  else if(mode==MD_ENDING && p_mode!=MD_ENDING) out = emit(EV_ENDING, 5400, 0, 0);
  else if(mode!=MD_BATTLE && scr!=p_scr){
    if(scr==2)      out = emit(EV_CHARSEL, 900, 0, 0);
    else if(scr==4) out = emit(EV_STYLESEL, 720, 0, 0);
    else if(scr==6) out = emit(EV_CARDSEL, 1200, 0, 0);
    else if(scr==0 && p_scr>=0) out = emit(EV_TITLE, 1800, 0, 0);
  }

  /* ── 전투 진입 ── */
  if(!out && mode==MD_BATTLE && p_mode!=MD_BATTLE && hp1>0 && hp2>0){
    st_ko=0; st_low1=st_low2=0; st_lead=0; st_rev=0; st_roundStart=(int)cm_f; st_won=0;
    if(surv>=1) out = emit(EV_SURV, 480, surv, 0);
    else if(stage>=1 && stage<=14 && stage>st_lastStage){
      st_lastStage = stage;
      out = emit(EV_STAGE, 480, stage+1, 0);
    }else{
      int b1=rd(OFF_BLK1), b2=rd(OFF_BLK2);
      if(!(b1&7) && !(b2&7) && (b1>>4)<15 && (b2>>4)<15){
        static char who[64];
        snprintf(who,sizeof(who),"%s 대 %s", CHARNAME[b1>>4], CHARNAME[b2>>4]);
        out = emit(EV_START, 150, 0, who);
      }else out = emit(EV_ROUND, 150, 0, 0);
    }
    if(stage < st_lastStage) st_lastStage = stage;   /* 새 코스 */
  }

  /* ── 전투 중 ── */
  if(mode==MD_BATTLE && p_mode==MD_BATTLE && hp1<=p_hp1 && hp2<=p_hp2){
    int hit1 = hp1 < p_hp1, hit2 = hp2 < p_hp2;
    if(!st_ko && hit1 && hit2 && hp1<=0 && hp2<=0){ st_ko=1; if(!out) out=emit(EV_DKO,240,0,0); }
    else if(!st_ko && hit2 && hp2<=0){
      st_ko=1; st_won=1;
      if(!out) out = emit(EV_KO, 240, 0, 0);
      /* 후속 한마디 — 역전승 > 완승 > 순살 (하나만) */
      if(st_low1)                                    emit(EV_COMEBACK, 900, 0, 0);
      else if(hp1>=128)                              emit(EV_PERFECT, 900, 0, 0);
      else if((int)cm_f - st_roundStart < 600)       emit(EV_QUICK, 900, 0, 0);
    }
    else if(!st_ko && hit1 && hp1<=0){
      st_ko=1; st_won=0;
      if(!out) out = emit(EV_KOED, 240, 0, 0);
      if(surv>0) emit(EV_SURVEND, 600, surv, 0);
    }
    if(!st_ko){
      int lead = (hp1>hp2) - (hp1<hp2);
      if(lead && st_lead && lead!=st_lead && (hp1-hp2>=8 || hp2-hp1>=8) && st_rev<2){
        if(emit(EV_REVERSAL, 360, 0, 0)) st_rev++;
      }
      if(lead) st_lead = lead;
      if(hp1>0 && hp1<=32 && !st_low1){ st_low1=1; if(!out) out=emit(EV_LOW1,90,0,0); }
      if(hp2>0 && hp2<=32 && !st_low2){ st_low2=1; if(!out) out=emit(EV_LOW2,90,0,0); }
    }
  }

  /* ── 승패 연출로 넘어간 뒤 한마디 더 ── */
  if(mode==MD_BATTLE && st_ko && scr<8 && p_scr>=8)
    emit(st_won ? EV_WINTALK : EV_LOSETALK, 420, 0, 0);

  /* ── 쉼 채우기: 조용하면 화자다운 혼잣말 (전투 6초 / 그 외 3초) ── */
  if(!out){
    unsigned quiet = cm_f - last_line_f;
    unsigned need  = (mode==MD_BATTLE) ? 360u : 180u;
    if(quiet > need) out = emit(mode==MD_BATTLE ? EV_MUSE_B : EV_MUSE_M, need, 0, 0);
  }

  p_mode=mode; p_scr=scr; p_hp1=hp1; p_hp2=hp2; p_surv=surv; p_stage=stage;
  return out;
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
  0,0,1,2,0,1,1,1, 2,1,1,1,2,1,2, 1,0,1,0,0,0,0, 0,0,0
};
static const unsigned char EVHIT[EV_N] = {
  0,0,1,0,1,1,1,1, 0,0,1,0,0,1,0, 0,0,1,0,0,0,0, 0,0,0
};

/* ── 초상 (얼굴) ──────────────────────────────────────────────────
   전투 HUD 초상 타일(16×16 = 4타일 × 16B, 2bpp)의 **롬 파일 오프셋**이다.
   브라우저판에서 리버싱해 둔 것과 같은 표 — 배포물에 들어가는 건 숫자뿐이고
   그림은 사용자 롬에서 그 자리에서 그린다(게임 그림은 어디에도 넣지 않는다).
   sum = 64바이트 단순합. 다른 버전 롬이면 초상은 조용히 생략한다. */
typedef struct { unsigned off; unsigned short pal[4]; unsigned short sum; } ss2face;
static const ss2face FACE_ROM[4] = {          /* 화자 순서: 하오마루·나코루루·한조·갈포드 */
  { 388903, {0x0000,0x0BDF,0x0865,0x0024}, 12253 },
  { 389671, {0x0000,0x0CDF,0x043F,0x0224}, 13246 },
  { 389543, {0x0000,0x00AF,0x0653,0x0210}, 11994 },
  { 388711, {0x0000,0x0BDF,0x00FF,0x0410}, 11724 }
};
static const unsigned char *cm_rom = 0;
static unsigned cm_romlen = 0;
static uint16_t face_px[4][256];
static unsigned char face_a[4][256];          /* 0 = 투명(색인 0) */
static unsigned char face_ok[4];
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
  for(i=0;i<4;i++){
    unsigned off = FACE_ROM[i].off, sum = 0; int j;
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
  spk  = cm_spk;
  if(!face_built) build_faces();
  show = 2 + (int)age*2;                       /* 타자 연출: 프레임당 두 글자 */

  tx0  = face_ok[spk] ? 21 : 4;
  x1   = w - 3;
  maxw = x1 - tx0 - (hit ? 2 : 0);

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
      : draw_line11(fb,pitch_px,tx0,x1,seg[i],seg[i+1], ty + i*lh,     top, bot, show, col, hit);
    show -= drawn;
  }
}
