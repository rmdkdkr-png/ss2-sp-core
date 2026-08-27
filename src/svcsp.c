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
#define OFF_X1      0x092E   /* P1 화면 X */
#define OFF_X2      0x0934   /* P2 화면 X — 방향은 X1<X2 로만 판정 (실측 §3) */
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
static int svc_step_frames(void){ static int v=-1; if(v<0){const char*e=getenv("SVCSP_STEP"); v=e?atoi(e):3;} return v; }
#define STEP_FRAMES  svc_step_frames()
#define HOLD_FRAMES  3          /* 탭 = 약. 마지막 방향 유지한 채 버튼 3프레임 (실측기와 동일) */
#define MAX_HOLD     20         /* 트리거를 잡고 있으면 여기까지 늘어난다 = 강.
                                   실측: 16f 홀드 = 독물기(지속 92f+, 전진 2.4배). 3f = 황물기 */
#define TAIL_FRAMES  2
#define MAX_STEPS    16
#define PENDING_FRAMES 150       /* 착지·경직 대기 상한 — MotM 평타 회복 후 여유 */

/* ── 기술표 — 자동 생성 헤더 (tools/svc/gen_svc_moves.py ← moves.json) ──
   모션 바이트 = 패드 비트 (2=0x02 3=0x0A 6=0x08 1=0x06 4=0x04), 오른쪽 볼 때 기준.
   flags: 1=근접 4=공중 8=미검증 16=모으기(첫 방향을 CHARGE_FRAMES 유지) */
#include "svcsp_moves.h"

#define CHARGE_FRAMES 40        /* 모으기 유지 (실측기 규격과 동일) */

/* 슬롯 인덱스: N F B D DF DB AIR */
enum { SL_N, SL_F, SL_B, SL_D, SL_DF, SL_DB, SL_AIR, SL_COUNT };

/* 기술표가 없는 캐릭터 → 그냥 펀치 */
static const unsigned char mo_basic[] = {0x00};
static const svc_move svc_basic = { "펀치", mo_basic, 1, PAD_A, 0 };

/* ── 상태 ────────────────────────────────────────────────────────── */
typedef struct { uint8_t pad; uint8_t frames; uint8_t sustain; } svc_step;
static svc_step  q[MAX_STEPS];
static int       q_n, q_i, q_left;
static uint16_t  prev_trig;
static uint16_t  prev_pad_dir;
static const svc_move *pending;
static int       pending_left;
static int       warm;             /* 전투 게이트 연속 프레임 */
static int       verify_left;
static int       svc_is_rom;       /* 헤더 판별 결과 */
static int       hold_elapsed;     /* 버튼 스텝 유지 누적 (강약 판정) */
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
   { unsigned t = CPUExRAM[OFF_TIMER]; if (t < 1 || t > 60) return 0; }
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
   return (a == 127) || (a >= 6 && a < 127);
}

static int svc_airborne(void) { return CPUExRAM[OFF_Y1] != 128; }

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
static void svc_compile(const svc_move *m)
{
   int i, mirror = (CPUExRAM[OFF_X1] > CPUExRAM[OFF_X2]);   /* P1 이 오른쪽 → 왼쪽 봄 */
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
   svcsp_last_name = m->name;
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
      { map = svc_chars[chr].slots; tbl = svc_chars[chr].mv; ntbl = svc_chars[chr].n; }
   else return &svc_basic;               /* 기술표 없는 캐릭터 */

   if (svc_airborne())
   {
      int mi = map[SL_AIR];
      return (mi >= 0 && mi < ntbl && (tbl[mi].flags & 4)) ? &tbl[mi] : &svc_basic;
   }
   {
      int facing_left = (CPUExRAM[OFF_X1] > CPUExRAM[OFF_X2]);
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
      else { q_n = q_i = 0; q_left = 0; }
   }
   return out;                    /* 매크로 중 사용자 입력은 무시 */
}

uint8_t svcsp_frame(uint8_t pad, uint16_t trig)
{
   uint16_t edge = (uint16_t)(trig & ~prev_trig);
   prev_trig = trig;

   if (svc_in_battle()) { if (warm < 1000) warm++; }
   else warm = 0;

   /* 발동 결과 판정 — 애니 카운터(경과시계)가 **줄어들면** 새 동작이 시작된 것.
      뱅크로는 안 된다: 황물기 꼬리가 뱅크 0 이라 팔청(0)과 구분이 안 된다 (실측). */
   {
      static uint8_t va_prev = 255;
      uint8_t va = CPUExRAM[OFF_ANIM];
      if (verify_left > 0)
      {
         if (va < va_prev && va_prev != 255) { svcsp_last_ok = 1; verify_left = 0; }
         else if (--verify_left == 0)          svcsp_last_ok = 0;
      }
      va_prev = va;
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
      if ((!need_ground || !svc_airborne()) && svc_actable())
         { svc_compile(pending); pending = 0; }
      else if (--pending_left <= 0) pending = 0;
   }

   if (svc_active()) return svc_step_out(trig & 1u);

   if ((edge & 1u) && !svc_in_battle() && svc_dbg())
      fprintf(stderr, "[svcsp] gate-fail chr=%d chr2=%d style=%d timer=%d y=%d\n",
              CPUExRAM[OFF_CHAR1], CPUExRAM[OFF_CHAR2], CPUExRAM[OFF_STYLE],
              CPUExRAM[OFF_TIMER], CPUExRAM[OFF_Y1]);
   if ((edge & 1u) && svc_in_battle())
   {
      const svc_move *m = svc_resolve(pad);
      if (svc_dbg()) fprintf(stderr, "[svcsp] edge chr=%d air=%d anim=%d -> %s\n",
                             CPUExRAM[OFF_CHAR1], svc_airborne(), CPUExRAM[OFF_ANIM],
                             m ? m->name : "(null)");
      if (m)
      {
         int need_ground = !(m->flags & 4);
         if ((need_ground && svc_airborne()) || !svc_actable())
            { pending = m; pending_left = PENDING_FRAMES; }
         else
            svc_compile(m);
      }
   }

   if (svc_active()) return svc_step_out(trig & 1u);   /* 트리거 프레임에도 첫 스텝이 나간다 */
   return pad;
}

void svcsp_reset(void)
{
   q_n = q_i = q_left = 0;
   pending = 0; pending_left = 0;
   prev_trig = 0; prev_pad_dir = 0;
   warm = 0; verify_left = 0;
   hold_elapsed = 0; svcsp_last_strong = 0;
   svcsp_last_ok = -1; svcsp_last_name = 0;
}
