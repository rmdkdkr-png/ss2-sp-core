/* ═══════════════════════════════════════════════════════════════════
   svcsp.c — SNK vs. Capcom MotM (NGPC) 원버튼 필살기 엔진

   ss2sp.c 의 동생. 구조는 같지만 SvC 실측(tools/svc/SVC_MEMO.md)에 맞춰
   훨씬 단순하다:
     · 가로채기가 없다 — SS2 의 램 리셋 트릭(0x1DCD)이 필요 없다. 입력만 넣으면 된다
     · 강/약이 없다 — 버튼 유지 시간 판정이 없어서 sustain/hold 경로가 없다
     · 커맨드 창이 좁다 — 방향 간격 2~4프레임 (기본 3), 마지막 방향을
       버튼보다 **먼저** 잡고, 잡은 채로 버튼을 누른다 (실측 §7)

   좌표 규약: 모든 오프셋은 SYSTEM_RAM(CPUExRAM) 기준. CPU 주소 = 0x4000 + 오프셋.
   이 게임엔 간이입력이 원래 없어서 (BATTLE CONFIG 에 ABLE 없음, 실측 §6)
   이 엔진이 이 게임 최초의 간이입력이 된다.
   ═══════════════════════════════════════════════════════════════════ */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int svc_dbg(void){ static int d=-1; if(d<0){const char*e=getenv("SVCSP_DEBUG"); d=(e&&*e=='1');} return d; }

/* ── 램 접근 — ss2sp.c 와 같은 이중 경로 ─────────────────────────── */
#ifdef SS2SP_RAM_POINTER
static uint8_t *svc_ram_ptr;
void svcsp_set_ram(void *p) { svc_ram_ptr = (uint8_t *)p; }
#define CPUExRAM svc_ram_ptr
#else
extern uint8_t CPUExRAM[16384];
#endif

/* ── RAM 맵 (tools/svc 실측) ─────────────────────────────────────── */
#define OFF_CHAR1   0x08A0   /* P1 캐릭터 ID. 쿄=0 … 가일=17. 선택화면=255 */
#define OFF_CHAR2   0x08C0   /* P2 캐릭터 ID (P1 블록 +0x20) */
#define OFF_STYLE   0x08BE   /* P1 유파. 반격=0 균형=1 속공=2 */
#define OFF_HP1     0x08B3   /* P1 체력 (피격 이벤트 때 0x08AF 와 동시 갱신) */
#define OFF_HP2     0x08CF   /* P2 체력 (표시 사본 — 읽기엔 충분) */
#define OFF_TIMER   0x08EE   /* 라운드 타이머 (10진). 메뉴=0, GO 연출=60 */
#define OFF_X1      0x092E   /* P1 X — ★16비트 (하위 0x092E, 상위 0x092F). 하위만 비교하면
                                256px 경계에서 좌우 판정이 뒤집힌다 — 누에 오발의 진범 (실측 §27) */
#define OFF_X2      0x0934   /* P2 X — 16비트 (0x0934/0x0935) */
#define OFF_Y1      0x0930   /* P1 Y. 지상=128, 점프 정점=86 */
#define OFF_BANK    0x09AD   /* P1 애니 뱅크. 대기·평타=255, 필살기는 기술별 값(0 포함!) */
#define OFF_ANIM    0x0C7E   /* P1 애니 카운터. 대기·걷기·앉기=127, 동작 중=카운트업 */

/* ── 패드 비트 (libretro.c input_buf 인코딩) ─────────────────────── */
#define PAD_UP    0x01
#define PAD_DOWN  0x02
#define PAD_LEFT  0x04
#define PAD_RIGHT 0x08
#define PAD_A     0x10          /* NGP A = PUNCH (게임 설정 화면 확인) */
#define PAD_B     0x20          /* NGP B = KICK */
#define PAD_DIR_MASK (PAD_UP|PAD_DOWN|PAD_LEFT|PAD_RIGHT)

/* ── 타이밍 (실측 §2·§7: 창 2~4프레임, 5 이상은 시간 초과) ────────── */
static int svc_step_frames(void){ static int v=-1; if(v<0){const char*e=getenv("SVCSP_STEP"); v=e?atoi(e):2;} return v; }
static int svc_hold_frames(void){ static int v=-1; if(v<0){const char*e=getenv("SVCSP_HOLD"); v=e?atoi(e):2;} return v; }
static int svc_tail_frames(void){ static int v=-1; if(v<0){const char*e=getenv("SVCSP_TAIL"); v=e?atoi(e):1;} return v; }

/* ── 버튼 레이아웃 (기본): 약은 원래 B·A, 강은 전용 버튼으로.
   실측 §30: 홀드 6f 이하 = 약, 8f 이상 = 강 — 12f 주입으로 여유.
   원버튼 모션 엔진은 기본 꺼짐: 순정 ABLE(アバレ) 모드가 그 역할을 대신한다(§29). */
#define SVC_HOLD_STRONG 12
static int svc_engine = -1;                     /* 원버튼 엔진 — 메뉴에서만 켠다 */
static int svc_native_basics;                   /* 앱 모드 — 기본기는 순정 통과(탭 약/홀드 강) */
static int svc_engine_now(void)
{
   if (svc_engine < 0) { const char *e = getenv("SVCSP_FORCE"); svc_engine = (e && *e == '1'); }
   return svc_engine;
}
void svcsp_set_engine(int on) { svc_engine = !!on; }
int  svcsp_engine_on(void)    { return svc_engine_now(); }
#define STEP_FRAMES  svc_step_frames()
#define HOLD_FRAMES  svc_hold_frames() /* 탭 = 약. 마지막 방향 유지한 채 버튼 (기본 2 — 실측 최소) */
#define MAX_HOLD     20         /* 트리거를 잡고 있으면 여기까지 늘어난다 = 강.
                                   실측: 16f 홀드 = 독물기(지속 92f+, 전진 2.4배). 짧으면 황물기 */
#define TAIL_FRAMES  svc_tail_frames()
#define MAX_STEPS    16
#define PENDING_FRAMES 150       /* 착지·경직 대기 상한 — MotM 평타 회복 후 여유 */

/* ── 기술표 — 자동 생성 헤더 (tools/svc/gen_svc_moves.py ← moves.json) ──
   모션 바이트 = 패드 비트 (2=0x02 3=0x0A 6=0x08 1=0x06 4=0x04), 오른쪽 볼 때 기준.
   flags: 1=근접 4=공중 8=미검증 16=모으기(첫 방향을 CHARGE_FRAMES 유지) */
#include "svcsp_moves.h"

#define CHARGE_FRAMES 40        /* 모으기 유지 (실측기 규격과 동일) */

/* 슬롯 인덱스: N F B D DF DB AIR */
enum { SL_N, SL_F, SL_B, SL_D, SL_DF, SL_DB, SL_AIR, SL_COUNT };

static int svc_in_battle(void);   /* 아래에 정의 — cur_char 가 먼저 쓴다 */

/* ── 런타임 슬롯 — 오버레이 메뉴에서 바꿀 수 있게 표를 복사해 둔다 ── */
static signed char svc_slot_run[SVC_CHAR_COUNT][7];
static int svc_slot_ready;
static int svc_slots_dirty;        /* 편집됨 — 프론트가 파일로 흘려보낸다 */
static void svc_slots_ensure(void)
{
   int c, k;
   if (svc_slot_ready) return;
   for (c = 0; c < SVC_CHAR_COUNT; c++)
      for (k = 0; k < 7; k++) svc_slot_run[c][k] = svc_chars[c].slots[k];
   svc_slot_ready = 1;
}

/* ── 오버레이 메뉴용 조회 API (ss2sp 의 style API 와 같은 역할) ── */
int svcsp_char_count(void) { return SVC_CHAR_COUNT; }
const char *svcsp_char_name(int c)
{ return (c >= 0 && c < SVC_CHAR_COUNT) ? svc_chars[c].name : ""; }
int svcsp_cur_char(void)
{
#ifdef SS2SP_RAM_POINTER
   if (!CPUExRAM) return -1;
#endif
   if (!svc_in_battle()) return -1;
   return CPUExRAM[OFF_CHAR1];
}
int svcsp_move_count(int c)
{ return (c >= 0 && c < SVC_CHAR_COUNT) ? svc_chars[c].n : 0; }
const char *svcsp_move_name(int c, int i)
{
   if (c < 0 || c >= SVC_CHAR_COUNT || i < 0 || i >= svc_chars[c].n) return "";
   return svc_chars[c].mv[i].name;
}
int svcsp_move_flags(int c, int i)
{
   if (c < 0 || c >= SVC_CHAR_COUNT || i < 0 || i >= svc_chars[c].n) return 0;
   return svc_chars[c].mv[i].flags;
}
/* 넘패드 표기 (236+P). 모으기는 첫 방향 두 번 (표시 규약 = 토스트와 동일) */
int svcsp_move_notation(int c, int i, char *out, int cap)
{
   static const struct { unsigned char pad; char ch; } tab[] = {
      {0x01,'8'},{0x02,'2'},{0x04,'4'},{0x08,'6'},
      {0x09,'9'},{0x05,'7'},{0x0A,'3'},{0x06,'1'},
   };
   const svc_move *m; int k, j, n = 0;
   if (!out || cap <= 0) return 0;
   out[0] = 0;
   if (c < 0 || c >= SVC_CHAR_COUNT || i < 0 || i >= svc_chars[c].n) return 0;
   m = &svc_chars[c].mv[i];
   for (k = 0; k < m->len && n < cap - 4; k++)
   {
      char ch2 = 0;
      for (j = 0; j < (int)(sizeof tab / sizeof tab[0]); j++)
         if (tab[j].pad == m->motion[k]) { ch2 = tab[j].ch; break; }
      if (!ch2) continue;
      out[n++] = ch2;
      if (k == 0 && (m->flags & 16) && n < cap - 4) out[n++] = ch2;
   }
   if (n < cap - 3) { out[n++] = '+'; out[n++] = (m->btn & PAD_A) ? 'P' : 'K'; }
   out[n] = 0;
   return n;
}
int svcsp_get_slot(int c, int k)
{
   svc_slots_ensure();
   if (c < 0 || c >= SVC_CHAR_COUNT || k < 0 || k >= 7) return -1;
   return svc_slot_run[c][k];
}
void svcsp_set_slot(int c, int k, int mv)
{
   svc_slots_ensure();
   if (c < 0 || c >= SVC_CHAR_COUNT || k < 0 || k >= 7) return;
   if (mv >= svc_chars[c].n || mv < 0) mv = -1;
   svc_slot_run[c][k] = (signed char)mv;
   svc_slots_dirty = 1;
}
void svcsp_reset_slots(void) { svc_slot_ready = 0; svc_slots_ensure(); svc_slots_dirty = 1; }

/* ── 러시 마무리 픽커 — 대표 초필(flags 32)과, 게이지 불발 시 낼 필살기 ── */
static const svc_move *svc_pick_super(int chr)
{
   int i;
   if (chr < 0 || chr >= SVC_CHAR_COUNT || !svc_chars[chr].mv) return 0;
   for (i = 0; i < svc_chars[chr].n; i++)
   {
      const svc_move *m = &svc_chars[chr].mv[i];
      if ((m->flags & 32) && !(m->flags & (4 | 8))) return m;
   }
   return 0;
}
static const svc_move *svc_pick_fallback(int chr)
{
   int mi, i;
   if (chr < 0 || chr >= SVC_CHAR_COUNT || !svc_chars[chr].mv) return 0;
   svc_slots_ensure();
   mi = svc_slot_run[chr][SL_N];
   if (mi >= 0 && mi < svc_chars[chr].n && !(svc_chars[chr].mv[mi].flags & (4 | 32)))
      return &svc_chars[chr].mv[mi];
   for (i = 0; i < svc_chars[chr].n; i++)
   {
      const svc_move *m = &svc_chars[chr].mv[i];
      if (!(m->flags & (1 | 4 | 8 | 32))) return m;
   }
   return 0;
}

/* ── 슬롯 배치 내보내기/들여오기 — <system>/ngpsvc_slots.bin (파일 IO 는 프론트) ── */
int svcsp_slots_dirty(void) { int d = svc_slots_dirty; svc_slots_dirty = 0; return d; }
int svcsp_slots_export(unsigned char *buf, int cap)
{
   int c, k, n = 0;
   if (!buf || cap < 4 + SVC_CHAR_COUNT * 7) return 0;
   svc_slots_ensure();
   buf[n++] = 'N'; buf[n++] = 'S'; buf[n++] = 'V'; buf[n++] = '1';
   for (c = 0; c < SVC_CHAR_COUNT; c++)
      for (k = 0; k < 7; k++) buf[n++] = (unsigned char)svc_slot_run[c][k];
   return n;
}
void svcsp_slots_import(const unsigned char *buf, int len)
{
   int c, k, n = 4;
   if (!buf || len < 4 + SVC_CHAR_COUNT * 7) return;
   if (buf[0] != 'N' || buf[1] != 'S' || buf[2] != 'V' || buf[3] != '1') return;
   svc_slots_ensure();
   for (c = 0; c < SVC_CHAR_COUNT; c++)
      for (k = 0; k < 7; k++)
      {
         signed char v = (signed char)buf[n++];
         if (v < -1 || v >= (signed char)svc_chars[c].n) v = -1;   /* 표가 줄었을 때 방어 */
         svc_slot_run[c][k] = v;
      }
}

/* 기술표가 없는 캐릭터 → 그냥 펀치 */
static const unsigned char mo_basic[] = {0x00};
static const svc_move svc_basic = { "펀치", mo_basic, 1, PAD_A, 0, -1, -1 };

/* ── 상태 ────────────────────────────────────────────────────────── */
typedef struct { uint8_t pad; uint8_t frames; uint8_t sustain; } svc_step;
static svc_step  q[MAX_STEPS];
static int       q_n, q_i, q_left;
static uint16_t  prev_trig;
static uint16_t  prev_pad_dir;
static const svc_move *pending;
static int       pending_left;
static int       pending_kind;     /* 0 일반 1 캔슬 선입력 2 파생(히트 확인) 3 창닫힘 후 재시전 */
static int       warm;             /* 전투 게이트 연속 프레임 */
static int       verify_left;
static int       svc_is_rom;       /* 헤더 판별 결과 */
static int       hold_elapsed;     /* 버튼 스텝 유지 누적 (강약 판정) */
static const svc_move *chain_tbl;  /* 마지막 발동 기술의 소속 표 (파생 인덱스 해석용) */
static const svc_move *chain_mv;   /* 마지막 발동 기술 */
static int       chain_left;       /* 파생 입력 창 (매크로 끝난 뒤 프레임) — 실측 +2~36f */
static uint32_t  frames;           /* 엔진 프레임 카운터 */
static uint32_t  my_attack_at;     /* 유저가 직접 평타(A/B)를 누른 프레임 — 킬캔슬 판정 */
static uint32_t  hit_at;           /* P2 체력이 깎인 프레임 — 히트 캔슬만 허용 (헛침 즉시주입은 leak) */
static uint32_t  move_started;     /* 마지막 기술 시작 프레임 */
static const svc_move *retry_mv;   /* 불발 재시도 대상 (파생 컴파일은 제외) */
static int       retry_cnt;
static uint32_t  retry_at;
static uint32_t  macro_end_at;
static int       compile_no_retry; /* 파생 등 재시도 금지 컴파일 표시 */
static uint8_t   bank_at_compile;  /* 컴파일 시점 뱅크 — 재시도 성공 판정은 뱅크 변화로
                                      (씹힌 평타도 카운터는 리셋해서 카운터만으론 속는다) */
static uint8_t   prev_pad_btn;
static int       attack_was_kick;  /* 마지막 자기 노멀이 킥(B)이었나 — 쿄 dud 는 킥 캔슬만 */
static uint8_t   prev_hp2v;
/* ── 모던 자동 콤보 상태 (러시/어시스트/기술키 판별) ── */
static uint16_t  prev_ret;         /* 물리 비트 이전 프레임 — 기본기 엣지 검출 */
static uint32_t  bas_last_at;      /* 마지막 기본기 눌림 (연타 창 판정) */
static int       rush_n;           /* 연타 단계. 0=없음 */
static uint8_t   rush_prev_btn;    /* 직전 연타 계열 (0x10 P / 0x20 K) — 교대용 */
static uint32_t  rush_hit0;        /* 러시 시작 시점의 hit_at — 이후 히트 여부 */
static const svc_move *rush_fb;    /* 마무리 초필 불발 시 갈아탈 필살기 */
static uint8_t   rush_conv;        /* 3타째 치환 버튼 (물리 버튼 잡는 동안 유지) */
static uint16_t  rush_conv_src;    /* 치환 대상 물리 비트 */

char svcsp_last_disp[64];  /* "황물기 \xe2\x86\x93\xe2\x86\x98\xe2\x86\x92+P" — 토스트용 */
int  svcsp_disp_seq;       /* 새 발동마다 +1. 프론트가 엣지 검출 */
const char *svcsp_last_name = 0;
int         svcsp_last_ok   = -1;
int         svcsp_last_strong = 0; /* 마지막 발동이 강(홀드)이었는지 — 표시용 */

static int svc_active(void) { return q_left > 0 || q_i < q_n; }

/* ── 롬 판별 — NGP 헤더 0x24~0x2F 12바이트 ──────────────────────── */
void svcsp_set_rom(const void *rom, unsigned len)
{
   svc_is_rom = rom && len >= 0x30 &&
                !memcmp((const unsigned char *)rom + 0x24, "SNKvsCAPCOM1", 12);
}
int svcsp_rom_ok(void) { return svc_is_rom; }

/* ── 전투중 판별 ─────────────────────────────────────────────────
   SvC 엔 SS2 의 MODE(0x00A7)=241 같은 단일 바이트가 없다 (BGM 상태 바이트는
   GO 연출에서 깨져 탈락 — 실측). 대신 네 조건의 합성으로 거른다.
   선택화면(char=255·timer=0)은 첫 조건에서 바로 떨어진다.
   Y 는 넣지 않는다 — CPU 던지기에 뜬 순간(y=44 실측)도 전투 중이다. */
static int svc_in_battle(void)
{
   if (CPUExRAM[OFF_CHAR1] >= 18) return 0;
   if (CPUExRAM[OFF_CHAR2] >= 26) return 0;
   if (CPUExRAM[OFF_STYLE] >= 3)  return 0;
   { unsigned t = CPUExRAM[OFF_TIMER];              /* 255 = 스파링 타임 무한 (실전 스테이트 실측) */
     if (t != 255 && (t < 1 || t > 60)) return 0; }
   return 1;
}

/* 행동 가능 판정.
   애니 카운터(0x0C7E)는 「마지막 동작 시작 후 경과」를 2프레임에 1씩 세다
   127에서 포화하는 시계다 (실측 — 대기 127은 포화값이었다).
   평타 회복 실측: +6f(카운터 3) 주입은 엉뚱한 기술이 새고, +12f(카운터 6)부터
   정상 발동. 그래서 문턱은 6. 피격·필살기 중의 이른 주입은 게임이 그냥 먹는다. */
static int svc_actable(void)
{
   unsigned a;
   static int nogate = -1;
   if (nogate < 0) { const char *e = getenv("SVCSP_NOGATE"); nogate = (e && *e == '1'); }
   if (nogate) return 1;
   if (warm < 4) return 0;
   a = CPUExRAM[OFF_ANIM];
   { unsigned k = CPUExRAM[OFF_ANIM + 1];          /* 0x0C7F = K(킥) 동작 카운터 */
     if (!((k == 127) || (k >= 6 && k < 127))) return 0; }
   return (a == 127) || (a >= 6 && a < 127);
}

static int svc_airborne(void) { return CPUExRAM[OFF_Y1] != 128; }

static unsigned svc_x16(unsigned off)
{ return (unsigned)CPUExRAM[off] | ((unsigned)CPUExRAM[off + 1] << 8); }
static int svc_facing_left(void)   /* P1 이 오른쪽에 있으면 왼쪽을 본다 */
{ return svc_x16(OFF_X1) > svc_x16(OFF_X2); }

static uint8_t svc_mirror(uint8_t pad)
{
   uint8_t lr = pad & (PAD_LEFT | PAD_RIGHT);
   pad &= (uint8_t)~(PAD_LEFT | PAD_RIGHT);
   if (lr == PAD_LEFT)  pad |= PAD_RIGHT;
   if (lr == PAD_RIGHT) pad |= PAD_LEFT;
   return pad;
}

/* 커맨드 → 프레임 큐. SS2 와 달리 리셋 트릭이 없다 — 그냥 순서대로 넣는다.
   ★ 마지막 방향을 STEP 프레임 잡은 **다음**, 잡은 채로 버튼을 HOLD 프레임.
     (같은 프레임에 방향+버튼이면 실패한다 — 실측 §7) */
static int svc_compile_cancel;   /* 이번 컴파일이 노멀 캔슬 경로인가 — 표시용 */
static void svc_compile(const svc_move *m)
{
   int i, mirror = svc_facing_left();
   uint8_t last = 0;
   q_n = q_i = 0;

   for (i = 0; i < m->len && q_n < MAX_STEPS - 3; i++)
   {
      uint8_t d = m->motion[i];
      if (mirror) d = svc_mirror(d);
      q[q_n].pad = d;
      /* 모으기 기술은 첫 방향을 길게 잡는다 ([4]6 의 4 부분) */
      q[q_n].frames = (uint8_t)((i == 0 && (m->flags & 16)) ? CHARGE_FRAMES : STEP_FRAMES);
      q[q_n].sustain = 0; q_n++;
      last = d;
   }
   /* 버튼 스텝 — 트리거를 잡고 있으면 늘어난다 (탭=약 황물기 / 홀드=강 독물기) */
   q[q_n].pad = (uint8_t)(last | m->btn); q[q_n].frames = HOLD_FRAMES; q[q_n].sustain = 1; q_n++;
   q[q_n].pad = 0; q[q_n].frames = TAIL_FRAMES; q[q_n].sustain = 0; q_n++;

   q_left = q[0].frames;
   hold_elapsed = 0;
   verify_left = 60;
   if (compile_no_retry) { retry_mv = 0; }
   else { retry_mv = m; if (m == &svc_basic) retry_mv = 0; }
   retry_cnt = 0; retry_at = 0;
   bank_at_compile = CPUExRAM[OFF_BANK];
   chain_mv = m; chain_left = 0;      /* 창은 매크로가 끝날 때 연다 (step_out) */
   move_started = frames;
   svcsp_last_name = m->name;
   /* 표시 문자열: 이름(괄호 별칭은 잘라냄) + 화살표(표기 기준, 미러 안 함) + 버튼 */
   if (m != &svc_basic)
   {
      static const char *AR[16] = {0};
      const char *arrows[16]; int na=0, wi=0, ci;
      char nb[32];
      (void)AR;
      for (ci = 0; ci < m->len && na < 12; ci++)
      {
         const char *a2 = 0;
         switch (m->motion[ci]) {
            case 0x01: a2="\xe2\x86\x91"; break;   /* ↑ */
            case 0x09: a2="\xe2\x86\x97"; break;   /* ↗ */
            case 0x08: a2="\xe2\x86\x92"; break;   /* → */
            case 0x0A: a2="\xe2\x86\x98"; break;   /* ↘ */
            case 0x02: a2="\xe2\x86\x93"; break;   /* ↓ */
            case 0x06: a2="\xe2\x86\x99"; break;   /* ↙ */
            case 0x04: a2="\xe2\x86\x90"; break;   /* ← */
            case 0x05: a2="\xe2\x86\x96"; break;   /* ↖ */
         }
         if (!a2) continue;
         arrows[na++] = a2;
         if (ci == 0 && (m->flags & 16)) arrows[na++] = a2;   /* 모으기: 첫 방향 두 번 */
      }
      for (wi = 0; wi < (int)sizeof nb - 1 && m->name[wi]; wi++)
      {
         if (m->name[wi] == '(' || (m->name[wi]==' ' && m->name[wi+1]=='(')) break;
         nb[wi] = m->name[wi];
      }
      while (wi > 0 && nb[wi-1] == ' ') wi--;
      nb[wi] = 0;
      {
         int n2 = snprintf(svcsp_last_disp, sizeof svcsp_last_disp, "%s ", nb);
         for (ci = 0; ci < na && n2 < (int)sizeof svcsp_last_disp - 8; ci++)
            n2 += snprintf(svcsp_last_disp + n2, sizeof svcsp_last_disp - n2, "%s", arrows[ci]);
         snprintf(svcsp_last_disp + n2, sizeof svcsp_last_disp - n2, "+%s%s",
                  (m->btn & PAD_A) ? "P" : "K",
                  svc_compile_cancel ? " ìºì¬" : "");
      }
      svcsp_disp_seq++;
   }
   svcsp_last_ok = -1;
   svcsp_last_strong = 0;
   if (svc_dbg()) fprintf(stderr, "[svcsp] compile %s steps=%d mirror=%d\n", m->name, q_n, mirror);
}

/* SP + 방향 → 슬롯. 공중이면 공중 슬롯만, 지상이면 지상 슬롯만.
   그 자리가 비면 가까운 자리 순서로 흐른다 (ss2sp SLOT_ORDER 축소판). */
static const unsigned char ORDER[6][6] = {
  /* N  */ {SL_N,  SL_N,  SL_N,  SL_N,  SL_N,  SL_N },
  /* F  */ {SL_F,  SL_DF, SL_N,  SL_D,  SL_B,  SL_DB},
  /* B  */ {SL_B,  SL_DB, SL_N,  SL_D,  SL_F,  SL_DF},
  /* D  */ {SL_D,  SL_DF, SL_DB, SL_N,  SL_F,  SL_B },
  /* DF */ {SL_DF, SL_D,  SL_F,  SL_N,  SL_DB, SL_B },
  /* DB */ {SL_DB, SL_D,  SL_B,  SL_N,  SL_DF, SL_F },
};

static const svc_move *svc_resolve(uint8_t held)
{
   const signed char *map;
   const svc_move *tbl;
   int ntbl;
   int chr = CPUExRAM[OFF_CHAR1];

   if (chr < SVC_CHAR_COUNT && svc_chars[chr].mv && svc_chars[chr].n)
      { svc_slots_ensure();
        map = svc_slot_run[chr]; tbl = svc_chars[chr].mv; ntbl = svc_chars[chr].n;
        chain_tbl = tbl; }
   else return &svc_basic;               /* 기술표 없는 캐릭터 */

   if (svc_airborne())
   {
      int mi = map[SL_AIR];
      return (mi >= 0 && mi < ntbl && (tbl[mi].flags & 4)) ? &tbl[mi] : &svc_basic;
   }
   {
      int facing_left = svc_facing_left();
      int d = !!(held & PAD_DOWN);
      int l = !!(held & PAD_LEFT), r = !!(held & PAD_RIGHT);
      int fwd = facing_left ? l : r, back = facing_left ? r : l;
      int want = SL_N, k;
      if      (d && fwd)  want = SL_DF;
      else if (d && back) want = SL_DB;
      else if (d)         want = SL_D;
      else if (fwd)       want = SL_F;
      else if (back)      want = SL_B;
      for (k = 0; k < 6; k++)
      {
         int mi = map[ORDER[want][k]];
         if (mi < 0 || mi >= ntbl) continue;
         if (tbl[mi].flags & 4) continue;      /* 지상에서 공중기 배제 */
         return &tbl[mi];
      }
   }
   return &svc_basic;
}

/* held = 트리거(X/R)가 아직 눌려 있는가. 버튼 스텝에서 잡고 있으면
   프레임을 소비하지 않고 늘린다 → 게임이 홀드 = 강(독물기)으로 받는다. */
static uint8_t svc_step_out(int held)
{
   uint8_t out = q[q_i].pad;
   if (q[q_i].sustain && held && hold_elapsed < MAX_HOLD)
   {
      if (++hold_elapsed >= 9) svcsp_last_strong = 1;
      return out;
   }
   if (--q_left <= 0)
   {
      if (++q_i < q_n) q_left = q[q_i].frames;
      else
      {
         q_n = q_i = 0; q_left = 0;
         macro_end_at = frames;
         /* 파생이 있는 기술이면 재입력 창을 연다 (실측: 첫 기술 시작 +2~36f 수용) */
         if (chain_mv && chain_tbl &&
             ((svcsp_last_strong ? chain_mv->next_hold : chain_mv->next) >= 0))
            chain_left = 34;
      }
   }
   return out;                    /* 매크로 중 사용자 입력은 무시 */
}

/* 지금 파생 창에서 X 를 누르면 나갈 기술 (없으면 0) */
static const svc_move *svc_chain_next(void)
{
   int idx;
   if (!chain_mv || !chain_tbl) return 0;
   idx = svcsp_last_strong ? chain_mv->next_hold : chain_mv->next;
   if (idx < 0 && svcsp_last_strong) idx = chain_mv->next;   /* 홀드 전용이 없으면 약 파생 */
   if (idx < 0) return 0;
   return &chain_tbl[idx];
}

/* R(기술키) 한 번 = 기술 발동 하나. 상황별 경로(즉시/보류/캔슬)는 실측 규칙 그대로 */
static void svc_fire_sp(uint8_t held)
{
   const svc_move *m = svc_resolve(held);
   if (svc_dbg()) fprintf(stderr, "[svcsp] edge chr=%d air=%d anim=%d -> %s\n",
                          CPUExRAM[OFF_CHAR1], svc_airborne(), CPUExRAM[OFF_ANIM],
                          m ? m->name : "(null)");
   if (m)
   {
      int need_ground = !(m->flags & 4);
      /* 킬캔슬: 자기 평타 직후(24f)의 기술키는 회복을 기다리지 않는다 —
         캔슬창이 히트 후 0~8f 라서 기다리면 놓친다 (실측 §19).
         히트가 아니면 게임이 그냥 먹는다 (가드·헛침 캔슬 불가 실측). */
      /* 노멀 캔슬: 창 안의 모션+버튼은 게임이 캐릭터별 지정기로 낸다 (실측 §24·§25).
         류·켄 등은 지정기가 공격기라 손맛 그대로 콤보 — 캔슬을 살린다.
         쿄는 지정기가 누에잡기(비공격 카운터, dud) — 창을 회피해 깨끗하게 낸다. */
      int chr2 = CPUExRAM[OFF_CHAR1];
      int dud  = ((chr2 < SVC_CHAR_COUNT) ? svc_chars[chr2].cancel_dud : 0)
                 && attack_was_kick;   /* 쿄 실측: 킥 캔슬=누에(꽝), 펀치 캔슬=물기(콤보) */
      int own_atk = (frames - my_attack_at) <= (dud ? 40 : 24) && !svc_airborne();
      int kill_cancel = own_atk && !dud && (frames - hit_at) <= 14;
      if (need_ground && svc_airborne())
         { pending = m; pending_left = PENDING_FRAMES; pending_kind = 0; }
      else if (kill_cancel)
         { svc_compile_cancel = 1; svc_compile(m); svc_compile_cancel = 0; }
      else if (own_atk && dud)
         { pending = m; pending_left = PENDING_FRAMES; pending_kind = 4; }
      else if (own_atk)
         { pending = m; pending_left = 30; pending_kind = 1; }   /* 선입력 — 히트 순간 발사 */
      else if (!svc_actable())
         { pending = m; pending_left = PENDING_FRAMES; pending_kind = 0; }
      else
         svc_compile(m);
   }
}

uint8_t svcsp_frame(uint8_t pad, uint16_t ret)   /* ret = 레트로패드 원본 비트 */
{
   uint16_t trig, edge;
   {  /* 순정 측정용 스위치 — 실기 배포에는 영향 없음(환경변수) */
      static int off = -1;
      if (off < 0) { const char *e = getenv("SVCSP_OFF"); off = (e && *e == '1'); }
      if (off) { prev_trig = 0; return pad; }
   }
   if (!svc_native_basics)
   {  /* 기본 레이아웃(양 모드 공통): A·B=약 고정, Y=강펀치(C) X=강킥(D), L=A+B.
         약 고정 = 물리 버튼을 6f(실측 약 상한)에서 강제 해제 — 꾹 눌러도 강이 안 된다.
         메뉴에서는 그냥 짧은 A 누름과 같아 부작용 없음.
         모던(엔진 켬)에서도 기본기 여섯 자리는 그대로다 — 「기본기는 콤보,
         기술키는 SP」(제보). 기술키는 R 하나만 넘어간다. 끔이면 R 도 A+B. */
      static int hold_p, hold_k, wk_p, wk_k;
      wk_p = (ret & (1u << 0)) ? wk_p + 1 : 0;            /* 물리 B버튼 = NGP A */
      wk_k = (ret & (1u << 8)) ? wk_k + 1 : 0;            /* 물리 A버튼 = NGP B */
      pad &= (uint8_t)~0x30;
      if (wk_p >= 1 && wk_p <= 6) pad |= 0x10;            /* 약펀치 — 6f 까지만 */
      if (wk_k >= 1 && wk_k <= 6) pad |= 0x20;            /* 약킥   — 6f 까지만 */
      if (ret & (1u << 1)) hold_p = SVC_HOLD_STRONG; else if (hold_p) hold_p--;
      if (ret & (1u << 9)) hold_k = SVC_HOLD_STRONG; else if (hold_k) hold_k--;
      if (hold_p) pad |= 0x10;                            /* NGP A 지속 = 강펀치 */
      if (hold_k) pad |= 0x20;                            /* NGP B 지속 = 강킥 */
      if (ret & (1u << 10)) pad |= 0x30;                  /* L = A+B(백플립) */
   }
   if (!svc_engine_now())
   {
      if (!svc_native_basics && (ret & (1u << 11))) pad |= 0x30;   /* 엔진 끔: R 도 A+B */
      prev_trig = 0;
      return pad;
   }
   trig = (ret & (1u << 11)) ? 1u : 0u;                   /* 엔진 켬: R = 기술키 */
   edge = (uint16_t)(trig & ~prev_trig);
   prev_trig = trig;
   frames++;
   if (chain_left > 0) chain_left--;
   /* ── 모던 자동 콤보: 「기본기는 콤보, 기술키는 SP」 ──────────────
      · 러시: 기본기 연타(24f 창). 3타째는 P/K 를 교대로 갈아 끼우고,
        히트가 확인된 4타째를 마무리(대표 초필)로 바꾼다. 초필이 게이지
        부족으로 불발이면 재시도 훅이 필살기로 갈아탄다.
      · 어시스트: R(기술키)을 잡은 채 기본기 연타 — 같은 사다리.
        R 엣지 쪽은 3f 판별 대기(sp_defer)로 N슬롯 오발을 막는다. */
   if (rush_conv)
   {  /* 3타째 교대 치환 — 물리 버튼을 잡고 있는 동안 유지 */
      if ((ret & rush_conv_src) && !svc_active())
         pad = (uint8_t)((pad & (uint8_t)~0x30) | rush_conv);
      else rush_conv = 0;
   }
   if (svc_in_battle() && !svc_active())
   {
      uint16_t bnew  = (uint16_t)(ret & (uint16_t)~prev_ret);
      uint16_t bmask = (uint16_t)((1u << 0) | (1u << 8) | (1u << 1) | (1u << 9));
      if (bnew & bmask)
      {
         int in_window = (frames - bas_last_at) <= 24;
         if (in_window || (trig & 1u)) rush_n++; else rush_n = 1;
         bas_last_at = frames;
         if (rush_n == 1) rush_hit0 = hit_at;
         if (rush_n >= 4 && hit_at > rush_hit0 && (frames - hit_at) <= 40)
         {
            const svc_move *sup = svc_pick_super(CPUExRAM[OFF_CHAR1]);
            if (sup)
            {
               rush_fb = svc_pick_fallback(CPUExRAM[OFF_CHAR1]);
               pending = 0; pending_kind = 0;
               svc_compile(sup);
            }
            rush_n = 0; rush_conv = 0;
         }
         else if (rush_n == 3)
         {
            uint8_t want = (rush_prev_btn == 0x10) ? 0x20 : 0x10;
            rush_conv = want; rush_conv_src = (uint16_t)(bnew & bmask);
            pad = (uint8_t)((pad & (uint8_t)~0x30) | want);
            rush_prev_btn = want;
         }
         else
            rush_prev_btn = (uint8_t)((pad & 0x10) ? 0x10 : ((pad & 0x20) ? 0x20 : rush_prev_btn));
      }
      else if (rush_n && (frames - bas_last_at) > 40) { rush_n = 0; rush_conv = 0; }
   }
   prev_ret = ret;
   /* 유저가 직접 누른 평타 감지 — 매크로 중이 아닐 때의 A/B 새 눌림 */
   if (!svc_active())
   {
      uint8_t btn = (uint8_t)(pad & (PAD_A | PAD_B));
      uint8_t nw  = (uint8_t)(btn & (uint8_t)~prev_pad_btn);
      if (nw) { my_attack_at = frames; attack_was_kick = !!(nw & PAD_B); }
      prev_pad_btn = btn;
   }
   /* 히트 감지 — P2 체력 감소 순간 (킬캔슬은 히트에서만 연다. 가드·헛침 캔슬 불가 실측) */
   {
      uint8_t h2 = CPUExRAM[OFF_HP2];
      if (h2 < prev_hp2v) hit_at = frames;
      prev_hp2v = h2;
   }

   if (svc_in_battle()) { if (warm < 1000) warm++; }
   else warm = 0;

   /* 발동 결과 판정 — 애니 카운터(경과시계)가 **줄어들면** 새 동작이 시작된 것.
      뱅크로는 안 된다: 황물기 꼬리가 뱅크 0 이라 팔청(0)과 구분이 안 된다 (실측). */
   {
      static uint8_t va_prev = 255, vk_prev = 255;
      uint8_t va = CPUExRAM[OFF_ANIM], vk = CPUExRAM[OFF_ANIM + 1];
      if (verify_left > 0)
      {
         if ((va < va_prev && va_prev != 255) || (vk < vk_prev && vk_prev != 255))
            { svcsp_last_ok = 1; verify_left = 0; }   /* retry 는 뱅크로만 접는다 — 평타 leak 도 카운터는 리셋한다 */
         else if (--verify_left == 0) svcsp_last_ok = 0;
      }
      va_prev = va; vk_prev = vk;
      /* 불발 자동 재시도 — 진짜 발동은 뱅크가 바뀐다 (평타 leak 은 카운터만 리셋).
         매크로 후 12f 안에 뱅크 변화가 없으면 14f 간격으로 다시 넣는다 (자기 후딜은
         무브마다 달라 문턱으로 못 덮는다 — 강킥은 +36f 에야 커맨드가 먹힌다, 실측). */
      {
         uint8_t bnow = CPUExRAM[OFF_BANK];
         if (retry_mv && bnow != bank_at_compile && bnow != 255)
            { retry_mv = 0; rush_fb = 0; }     /* 뱅크 변화 = 진짜 발동 */
         if (retry_mv && !svc_active() && !pending && frames - macro_end_at >= 12)
         {
            if (rush_fb)
            {  /* 러시 마무리 초필 불발(뱅크 그대로) = 게이지 부족 — 필살기로 갈아탄다 */
               const svc_move *fb = rush_fb;
               rush_fb = 0;
               svc_compile(fb);
            }
            else if (retry_cnt >= 3) retry_mv = 0;
            else if (!retry_at) retry_at = frames + 2;
            else if (frames >= retry_at)
            {
               const svc_move *rm = retry_mv;
               int rc = retry_cnt + 1;
               if (svc_dbg()) fprintf(stderr, "[svcsp] retry %d %s\n", rc, rm->name);
               svc_compile(rm);                /* compile 이 retry 상태를 초기화한다 — 복원 */
               retry_mv = rm; retry_cnt = rc; retry_at = frames + 14;
            }
         }
      }
   }

   /* 보류 관리 — 전투가 끝났거나 새 입력이 오면 버린다 (ss2sp v0.5.4 규칙) */
   if (pending)
   {
      if (!svc_in_battle() || CPUExRAM[OFF_HP1] == 0 || CPUExRAM[OFF_HP2] == 0)
         { pending = 0; pending_left = 0; }
      else if (edge || ((pad & ~prev_pad_dir) & PAD_DIR_MASK))
         { pending = 0; pending_left = 0; }
   }
   prev_pad_dir = (uint16_t)(pad & PAD_DIR_MASK);

   if (pending && !svc_active())
   {
      int need_ground = !(pending->flags & 4);
      int fire = 0;
      if (pending_kind == 2)
         fire = 1;                       /* 파생 — 헛쳐도 창 안이면 나가는 것이 원작 동작 (유저 확인) */
      else if (pending_kind == 1 && (frames - hit_at) <= 2)
         fire = 2;                       /* 캔슬 선입력 — 히트가 뜬 그 순간 발사 */
      else if (pending_kind == 4)
      {  /* 평타 후 — 캔슬 창(공격 시작 +40f)이 지나고 행동 가능해지면 */
         if ((frames - my_attack_at) >= 40 && svc_actable()) fire = 1;
         else if (--pending_left <= 0) { pending = 0; pending_kind = 0; }
      }
      else if ((!need_ground || !svc_airborne()) && svc_actable())
         fire = 1;                       /* 일반 — 착지·회복 대기 */
      else if (pending_kind != 2 && --pending_left <= 0) { pending = 0; pending_kind = 0; }
      if (fire && pending)
      {
         compile_no_retry = (pending_kind == 2);   /* 파생은 재시도 금지 */
         svc_compile(pending); compile_no_retry = 0;
         pending = 0; pending_kind = 0;
      }
   }

   if (svc_active())
   {
      /* 렛카: 매크로 진행 중 X 재입력 → 파생을 보류에 걸어 둔다 (창 안에서 자동 발동) */
      if ((edge & 1u))
      {
         const svc_move *nx = svc_chain_next();
         if (nx) { pending = nx; pending_left = 60; pending_kind = 2; }
      }
      return svc_step_out(trig & 1u);
   }

   if ((edge & 1u) && !svc_in_battle() && svc_dbg())
      fprintf(stderr, "[svcsp] gate-fail chr=%d chr2=%d style=%d timer=%d y=%d\n",
              CPUExRAM[OFF_CHAR1], CPUExRAM[OFF_CHAR2], CPUExRAM[OFF_STYLE],
              CPUExRAM[OFF_TIMER], CPUExRAM[OFF_Y1]);
   if ((edge & 1u) && svc_in_battle() && chain_left > 0)
   {
      const svc_move *nx = svc_chain_next();
      if (nx)
      {
         compile_no_retry = 1; svc_compile(nx); compile_no_retry = 0;
         if (svc_active()) return svc_step_out(trig & 1u);
      }
   }
   if ((edge & 1u) && svc_in_battle())
      svc_fire_sp(pad);   /* 즉시 발동 — 미루면 방향-모션 인접이 깨져 판정이 달라진다(실측) */

   if (svc_active()) return svc_step_out(trig & 1u);   /* 트리거 프레임에도 첫 스텝이 나간다 */
   return pad;
}

/* ── 앱(NGP.emu) 어댑터 — 레트로패드가 없는 프론트용 ────────────────
   기본기 A/B 는 순정 그대로(탭=약/홀드=강 — 강 전용 버튼이 없는 프론트),
   기술키는 trig bit0 하나(앱의 SP 키). 러시/어시스트/초필 폴백은 그대로 돈다. */
uint8_t svcsp_frame_app(uint8_t pad, uint16_t trig)
{
   uint16_t ret = 0;
   uint8_t out;
   if (pad & 0x10) ret |= (1u << 0);    /* NGP A(펀치) → 약P 엣지 자리 */
   if (pad & 0x20) ret |= (1u << 8);    /* NGP B(킥)  → 약K 엣지 자리 */
   if (trig & 1u)  ret |= (1u << 11);   /* SP 키 → 기술키 */
   svc_native_basics = 1;
   out = svcsp_frame(pad, ret);
   svc_native_basics = 0;
   return out;
}

void svcsp_reset(void)
{
   q_n = q_i = q_left = 0;
   pending = 0; pending_left = 0; pending_kind = 0; move_started = 0;
   retry_mv = 0; retry_cnt = 0; retry_at = 0; macro_end_at = 0; compile_no_retry = 0;
   prev_trig = 0; prev_pad_dir = 0;
   prev_pad_btn = 0; my_attack_at = 0; hit_at = 0; prev_hp2v = 255; frames = 100;
   attack_was_kick = 0;
   warm = 0; verify_left = 0;
   hold_elapsed = 0; svcsp_last_strong = 0;
   chain_mv = 0; chain_tbl = 0; chain_left = 0;
   svcsp_last_ok = -1; svcsp_last_name = 0;
   prev_ret = 0; bas_last_at = 0; rush_n = 0; rush_prev_btn = 0; rush_hit0 = 0;
   rush_fb = 0; rush_conv = 0; rush_conv_src = 0;
}
