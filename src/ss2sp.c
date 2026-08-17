/* ═══════════════════════════════════════════════════════════════════
   ss2sp.c — 사무라이 스피리츠! 2 (NGPC) 원버튼 필살기 엔진, 코어 내장판

   HTML 실행기(사무라이스피리츠2_SP실행기.html)의 매크로 엔진을 libretro 코어
   안으로 옮긴 것. 브라우저판 대비 이점:
     · 프레임 단위 정확 — setTimeout/rAF 지터가 원리적으로 없다
     · RAM 접근이 공짜 — 힙 베이스 탐색·직렬화 폴백 불필요
     · 남는 패드 버튼을 그대로 쓴다 (NGP는 A/B/Option 셋뿐이라 X·Y·L·R·L2·R2가 논다)

   좌표 규약: 모든 오프셋은 SYSTEM_RAM(CPUExRAM) 기준. CPU 주소 = 0x4000 + 오프셋.
   ═══════════════════════════════════════════════════════════════════ */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
static int ss2_dbg(void){ static int d=-1; if(d<0){const char*e=getenv("SS2SP_DEBUG"); d=(e&&*e=='1');} return d; }
#include "ss2sp_moves.h"

/* ── 램 접근 ──────────────────────────────────────────────────────
   libretro(beetle-ngp)에서는 CPUExRAM 이 전역 C 심볼이라 그대로 extern 한다.
   NGP.emu(emu-ex-plus-alpha)에서는 같은 배열이 C++ 네임스페이스(MDFN_IEN_NGP)
   안에 있어 C에서 직접 못 본다. 그쪽은 SS2SP_RAM_POINTER 를 정의하고
   부팅 때 ss2sp_set_ram(&CPUExRAM[0]) 을 한 번 불러 주면 된다.
   본문 코드는 양쪽 모두 손대지 않는다. */
#ifdef SS2SP_RAM_POINTER
static uint8_t *ss2_ram_ptr;
void ss2sp_set_ram(void *p) { ss2_ram_ptr = (uint8_t *)p; }
#define CPUExRAM ss2_ram_ptr
#else
extern uint8_t CPUExRAM[16384];
#endif

/* ── RAM 맵 (리버싱 실측) ───────────────────────────────────────── */
#define OFF_ACT        0x0E3E   /* P1 액션ID (16bit). 필살기 ≥ 0x180 */
#define OFF_Y          0x0E3A   /* P1 Y. 지상 = 128 */
#define OFF_FACING     0x0E54   /* 1 = 왼쪽을 봄 */
#define OFF_CHARSTYLE  0x1B51   /* 8 * (2*캐릭터 + 유파) */
#define OFF_HITSTOP    0x1DC7
#define OFF_CMDRESET   0x1DCD   /* ★ 여기에 1을 쓰면 게임이 입력이력을 스스로 비운다 */
#define OFF_MODE       0x00A7   /* 메뉴/전투 판별 */

/* ── 패드 비트 (libretro.c의 input_buf 인코딩과 동일) ────────────── */
#define PAD_UP    0x01
#define PAD_DOWN  0x02
#define PAD_LEFT  0x04
#define PAD_RIGHT 0x08
#define PAD_A     0x10          /* NGP A = 약베기 */
#define PAD_B     0x20          /* NGP B = 강베기 */

/* ── 타이밍 (프레임) ────────────────────────────────────────────── */
static int ss2_step_frames(void){ static int v=-1; if(v<0){const char*e=getenv("SS2SP_STEP"); v=e?atoi(e):3;} return v; }
#define STEP_FRAMES   ss2_step_frames()   /* 방향 1개 유지 */
#define HOLD_FRAMES   6         /* 마지막 방향 + 버튼 동시 유지 (약) */
/* ── 강/약 ────────────────────────────────────────────────────────
   게임은 버튼을 얼마나 오래 잡고 있었는지로 강·약을 가른다.
   브라우저판은 사용자가 SP를 누르고 있는 동안 마지막 스텝을 늘려서
   8프레임을 넘기면 강으로 확정한다. 코어판에는 그게 없어서 **항상 약**이었다
   (실기 제보: "길게 눌러도 강이 안 나온다").
   → 트리거를 계속 잡고 있으면 버튼 유지 프레임을 늘린다. */
#define STRONG_FRAMES 9         /* 이 이상 유지되면 강 판정 */
#define MAX_HOLD      26        /* 안전 상한 — 무한정 잡고 있어도 여기서 끊는다 */
#define TAIL_FRAMES   2         /* 중립 복귀 */
#define MAX_STEPS     24

typedef struct { uint8_t pad; uint8_t frames; uint8_t sustain; } ss2_step;

static ss2_step  q[MAX_STEPS];
static uint16_t  hold_bit;        /* 매크로를 시작시킨 트리거 비트 */
int ss2sp_last_strong = 0;        /* 마지막 발동이 강이었는지 (표시용) */
static int       hold_elapsed;    /* 버튼 유지 프레임 누적 */
static int       q_n, q_i, q_left;
static uint16_t  prev_trig;       /* 트리거 버튼 엣지 검출 */
static int       started_act;     /* 발동 직전 액션ID — 결과 판정용 */
static int       verify_left;
static const ss2_move *pending;   /* 공중에서 지상기를 눌렀을 때 착지까지 보류 */
static int       pending_left;
static uint32_t  frame_no;        /* 엔진 프레임 카운터 */
static uint32_t  horiz_at;        /* 순수 좌/우(아래 없음)를 마지막으로 잡고 있던 프레임 */

/* 비오의 — 유파를 가리지 않고 ←→↓+A 하나다. 그래서 기술표에 넣지 않고 여기 둔다.
   분노 MAX 일 때만 나가는 기술이라 방향 조합에 끼워 두면 평소엔 죽은 자리가 된다.
   모션 바이트는 패드 비트다: 4=LEFT(0x4) 6=RIGHT(0x8) 2=DOWN(0x2). */
static const unsigned char mo_super[] = { 0x4, 0x8, 0x2 };
static const ss2_move ss2_super = { "비오의", mo_super, 3, 16, 0 };

/* 마지막 실행 결과 (프론트엔드 로그/디버그용) */
const char *ss2sp_last_name = 0;
int         ss2sp_last_ok   = -1;   /* -1 미판정 · 0 불발 · 1 발동 */

static int ss2_active(void) { return q_left > 0 || q_i < q_n; }

/* 배치 방식: 0 = 직결(9버튼 = 기술 1~9번) · 1 = SP(X = 방향으로 슬롯, 나머지 = 1~8번) */
static int ss2_layout_sp = 0;
void ss2sp_set_layout(int sp) { ss2_layout_sp = sp; }   /* 코어 옵션에서 호출 */
static int ss2_sp_layout(void)
{
   static int env = -1;                                  /* 실험용 강제 지정 */
   if (env < 0) { const char *e = getenv("SS2SP_LAYOUT"); env = e ? (*e == 's') : -2; }
   return (env >= 0) ? env : ss2_layout_sp;
}

/* 현재 캐릭터·유파의 기술 목록 */
static unsigned ss2_cur_idx;
static const ss2_style *ss2_cur_style(void)
{
   unsigned v = CPUExRAM[OFF_CHARSTYLE];
   if (v & 7) return 0;                 /* 8의 배수가 아니면 전투 중이 아님 */
   ss2_cur_idx = v >> 3;
   if (ss2_cur_idx >= SS2_STYLE_COUNT) return 0;
   return &ss2_styles[ss2_cur_idx];
}

static uint8_t ss2_mirror(uint8_t pad)
{
   uint8_t lr = pad & (PAD_LEFT | PAD_RIGHT);
   pad &= (uint8_t)~(PAD_LEFT | PAD_RIGHT);
   if (lr == PAD_LEFT)  pad |= PAD_RIGHT;
   if (lr == PAD_RIGHT) pad |= PAD_LEFT;
   return pad;
}

/* 커맨드를 프레임 큐로 컴파일한다.
   ★ 핵심: 시작 전에 OFF_CMDRESET에 1을 쓴다.
   게임은 방향 입력을 4프레임마다 128칸 링버퍼에 계속 기록하고, 매처는 뒤에서부터
   불일치를 건너뛰며 훑는다. 그래서 뒤로 걷다가 623을 넣으면 실제로는 4444…623이
   되어 엉뚱한 기술이 나가거나 불발한다. 리셋 플래그를 세우면 게임 자신의 리셋 경로가
   커서와 센티널을 정리한다 — 추가 지연 0프레임, 연속기도 안 끊긴다.
   (예전엔 약베기를 한 번 끼워 버퍼를 끊었는데 그건 3연참 같은 연속기를 파괴했다) */
static void ss2_compile(const ss2_move *m)
{
   int i, mirror = (CPUExRAM[OFF_FACING] == 1);
   q_n = q_i = 0;

   /* 방향 오염 보정 — ⚠️ **조건부**로만 넣는다.
      좌/우로 시작하는 커맨드는 걷던 입력과 이어붙어 매처가 엉뚱하게 물린다.
      그래서 중립 프레임을 앞에 넣는데, 이걸 **항상** 넣으면 발동이 12프레임(0.2초) 느려진다.
      (실기 제보: "코어판은 발동이 느리다" — 원인이 정확히 이것이었다)
      브라우저판과 같은 규칙으로 좁힌다: **최근에 순수 좌/우를 잡고 있었을 때만.**
      아래·대각 유지는 무해하므로 제외한다(실측). */
   {
      static int pre = -1;
      /* 기본 0 — **RAM 리셋(0x1DCD)이 이미 링을 비우므로 프리앰블이 필요 없다.**
         실측: PRENEUTRAL 12 vs 0 결과 동일(12/12). 브라우저판도 리셋 성공 시 보정을 통째로 건너뛴다.
         12를 넣으면 발동만 0.2초 느려진다(실기 제보로 확인). 실험용으로만 남겨둔다. */
      if (pre < 0) { const char *e = getenv("SS2SP_PRENEUTRAL"); pre = e ? atoi(e) : 0; }
      if (pre > 0 && m->len && (m->motion[0] & (PAD_LEFT | PAD_RIGHT))
          && horiz_at && (frame_no - horiz_at) < 40)
      {
         q[q_n].pad = 0; q[q_n].frames = (uint8_t)pre; q_n++;
      }
   }

   for (i = 0; i < m->len && q_n < MAX_STEPS - 2; i++)
   {
      uint8_t d = m->motion[i];
      if (mirror) d = ss2_mirror(d);
      if (i == m->len - 1)
      {
         q[q_n].pad = (uint8_t)(d | m->btn);
         q[q_n].frames = HOLD_FRAMES;
         q[q_n].sustain = 1;         /* 트리거를 잡고 있으면 여기서 늘어난다 */
      }
      else
      {
         q[q_n].pad = d;
         q[q_n].frames = STEP_FRAMES;
         q[q_n].sustain = 0;
      }
      q_n++;
   }
   q[q_n].pad = 0; q[q_n].frames = TAIL_FRAMES; q[q_n].sustain = 0; q_n++;

   /* 실험용 스위치: SS2SP_NORESET=1 이면 리셋을 건너뛴다(대조군 측정용) */
   {
      static int noreset = -1;
      if (noreset < 0) { const char *e = getenv("SS2SP_NORESET"); noreset = (e && *e == '1'); }
      if (!noreset) CPUExRAM[OFF_CMDRESET] = 1;   /* ★ 입력 이력 리셋 */
   }

   q_left = q[0].frames;
   hold_elapsed = 0;
   ss2sp_last_strong = 0;
   started_act = CPUExRAM[OFF_ACT] | (CPUExRAM[OFF_ACT + 1] << 8);
   verify_left = 90;
   ss2sp_last_name = m->name;
   ss2sp_last_ok = -1;
}

/* ── SP 모드: 버튼 하나 + 방향으로 슬롯을 고른다 ─────────────────
   브라우저판 resolveSpSlot과 같은 우선순위:
     실제 공중 > ↑(공중기 의도) > ↘/↙ > 아래 > 앞/뒤 > 중립
   앞/뒤는 캐릭터가 보는 방향 기준으로 계산한다(좌우 자동 반전). */
static const signed char *ss2_slots_row(int style);

static const ss2_move *ss2_resolve_sp(const ss2_style *st, unsigned idx, uint8_t held)
{
   const signed char *map = ss2_slots_row((int)idx);
   int facing_left = (CPUExRAM[OFF_FACING] == 1);
   int airborne = (CPUExRAM[OFF_Y] != 128);
   int u = !!(held & PAD_UP),  d = !!(held & PAD_DOWN);
   int l = !!(held & PAD_LEFT), r = !!(held & PAD_RIGHT);
   int fwd = facing_left ? l : r, back = facing_left ? r : l;
   int slot = SS2_SLOT_N;

   if      ((airborne || u) && map[SS2_SLOT_AIR] >= 0) slot = SS2_SLOT_AIR;
   else if (d && fwd  && map[SS2_SLOT_DF] >= 0)        slot = SS2_SLOT_DF;
   else if (d && back && map[SS2_SLOT_DB] >= 0)        slot = SS2_SLOT_DB;
   else if (d         && map[SS2_SLOT_D]  >= 0)        slot = SS2_SLOT_D;
   else if (fwd       && map[SS2_SLOT_F]  >= 0)        slot = SS2_SLOT_F;
   else if (back      && map[SS2_SLOT_B]  >= 0)        slot = SS2_SLOT_B;
   if (map[slot] < 0) slot = SS2_SLOT_N;
   if (map[slot] < 0 || map[slot] >= st->n) return 0;
   return &st->mv[map[slot]];
}

/* 큐에서 이번 프레임에 내보낼 패드 1개를 꺼낸다.
   held = 매크로를 시작시킨 트리거가 아직 눌려 있는가. 눌려 있으면 버튼 스텝을 늘린다(=강). */
static uint8_t ss2_step_out(int held)
{
   uint8_t out = q[q_i].pad;
   /* 히트스톱 중에는 프레임이 소비되지 않는다 — 카운트를 멈춘다.
      (브라우저판이 ms 타이머로 근사하던 부분. 코어에선 정확히 맞출 수 있다) */
   if (!CPUExRAM[OFF_HITSTOP])
   {
      /* 손가락을 떼지 않았고 상한에 안 닿았으면 이 프레임은 소비하지 않는다 */
      if (q[q_i].sustain && held && hold_elapsed < MAX_HOLD)
      {
         if (++hold_elapsed >= STRONG_FRAMES) ss2sp_last_strong = 1;
         return out;
      }
      if (--q_left <= 0)
      {
         if (++q_i < q_n) q_left = q[q_i].frames;
         else { q_n = q_i = 0; q_left = 0; }
      }
   }
   if (q[q_i].sustain && ++hold_elapsed >= STRONG_FRAMES) ss2sp_last_strong = 1;
   return out;                           /* 매크로 중 사용자 입력은 무시 */
}



/* 매 프레임 호출. pad는 libretro.c가 만든 input_buf.
   trig는 미사용 버튼 9개의 비트마스크(X,Y,L,R,L2,R2,L3,R3,SELECT 순).
   반환값이 실제로 게임에 들어갈 패드 바이트다. */
uint8_t ss2sp_frame(uint8_t pad, uint16_t trig)
{
   uint16_t edge = (uint16_t)(trig & ~prev_trig);
   prev_trig = trig;

   /* 걷기 감지 — 순수 좌/우(아래 없음)만 오염원이다 */
   frame_no++;
   if ((pad & (PAD_LEFT | PAD_RIGHT)) && !(pad & PAD_DOWN)) horiz_at = frame_no;

   /* 발동 결과 판정 — 코어 안이라 폴링 비용이 사실상 0 */
   if (verify_left > 0 && !ss2_active())
   {
      int act = CPUExRAM[OFF_ACT] | (CPUExRAM[OFF_ACT + 1] << 8);
      if (act >= 0x180 && act != started_act) { ss2sp_last_ok = 1; verify_left = 0; }
      else if (--verify_left == 0)             ss2sp_last_ok = 0;
   }

   /* 착지 대기 — 공중에서 지상기를 누르면 씹힌다. 브라우저판과 동일하게 착지까지 들고 있다가 낸다. */
   if (pending && !ss2_active())
   {
      if (CPUExRAM[OFF_Y] == 128) { ss2_compile(pending); pending = 0; }
      else if (--pending_left <= 0) pending = 0;
   }

   if (ss2_active())
      return ss2_step_out(hold_bit && (trig & hold_bit));

   if (edge)
   {
      const ss2_style *st = ss2_cur_style();
      if (ss2_dbg()) fprintf(stderr, "[ss2sp] edge=%04x style=%s y=%d\n",
                             edge, st ? st->id : "(null)", CPUExRAM[OFF_Y]);
      if (st)
      {
         const ss2_move *m = 0;
         int slot = -1, b;
         for (b = 0; b < 9; b++) if (edge & (1u << b)) { slot = b; break; }
         hold_bit = (uint16_t)(slot >= 0 ? (1u << slot) : 0);   /* 이 버튼을 잡고 있으면 강 */

         /* 버튼 번호는 레이아웃과 무관하게 고정이다: SP1~SP7 = 기술 1~7번, SP8 = 비오의.
            SP 레이아웃일 때만 SP1 이 방향을 함께 읽는데, 방향을 안 잡으면 중립 자리가
            나오고 중립 자리는 항상 기술 1번이라 결과가 같다. 그래서 어긋나지 않는다.
            (예전에는 SP 레이아웃에서 번호가 한 칸씩 밀려 SP2 가 기술 1번이었다) */
         if (slot == 7)
            m = &ss2_super;
         else if (ss2_sp_layout() && slot == 0)
            m = ss2_resolve_sp(st, ss2_cur_idx, pad);   /* SP1 = SP + 방향 */
         else if (slot >= 0 && slot < st->n)
            m = &st->mv[slot];

         if (m)
         {
            /* 공중기가 아니면 착지까지 기다린다(지상기는 공중에서 씹힌다).
               브라우저판의 waitGround와 같은 판정. */
            if (!(m->flags & 4) && CPUExRAM[OFF_Y] != 128)
            {
               pending = m; pending_left = 90;      /* 최대 1.5초 대기 */
            }
            else
            {
               ss2_compile(m);
               if (ss2_dbg()) fprintf(stderr, "[ss2sp] compile %s len=%d steps=%d\n",
                                      m->name, m->len, q_n);
            }
         }
      }
   }

   /* ★ 트리거 프레임에도 매크로의 첫 스텝이 나가야 한다.
      예전엔 여기서 사용자 입력을 그대로 흘려서, ↑+SP를 누르면 점프가 먼저 튀어나갔다. */
   if (ss2_active())
      return ss2_step_out(hold_bit && (trig & hold_bit));

   return pad;
}


/* ═══════════════════════════════════════════════════════════════════
   기술 배치 커스텀 API
   ss2_spmap(자동 생성 기본값)을 통째로 복사해 두고, 사용자가 고친 값을 여기에 담는다.
   프론트엔드(NGP.emu 메뉴)가 읽고 쓴다. 저장은 blob 210바이트.
   ═══════════════════════════════════════════════════════════════════ */
#define SS2_SLOTS 7
static signed char ss2_slot_tbl[SS2_STYLE_COUNT][SS2_SLOTS];
static int         ss2_slot_ready;

static void ss2_slots_ensure(void)
{
   if (!ss2_slot_ready)
   {
      memcpy(ss2_slot_tbl, ss2_spmap, sizeof ss2_slot_tbl);
      ss2_slot_ready = 1;
   }
}

static const signed char *ss2_slots_row(int style)
{
   ss2_slots_ensure();
   if (style < 0 || style >= SS2_STYLE_COUNT) style = 0;
   return ss2_slot_tbl[style];
}

int ss2sp_style_count(void) { return SS2_STYLE_COUNT; }
int ss2sp_slot_count(void)  { return SS2_SLOTS; }
int ss2sp_slots_size(void)  { return SS2_STYLE_COUNT * SS2_SLOTS; }

const char *ss2sp_style_id(int style)
{
   if (style < 0 || style >= SS2_STYLE_COUNT) return "";
   return ss2_styles[style].id;
}

/* 현재 유파. 전투 중이 아니면 -1 */
int ss2sp_cur_style(void)
{
   unsigned v;
   if (!CPUExRAM) return -1;
   v = CPUExRAM[OFF_CHARSTYLE];
   if (v & 7) return -1;
   v >>= 3;
   return (v < SS2_STYLE_COUNT) ? (int)v : -1;
}

int ss2sp_move_count(int style)
{
   if (style < 0 || style >= SS2_STYLE_COUNT) return 0;
   return ss2_styles[style].n;
}

const char *ss2sp_move_name(int style, int i)
{
   if (style < 0 || style >= SS2_STYLE_COUNT) return "";
   if (i < 0 || i >= ss2_styles[style].n) return "";
   return ss2_styles[style].mv[i].name;
}

int ss2sp_move_btn(int style, int i)
{
   if (style < 0 || style >= SS2_STYLE_COUNT) return 0;
   if (i < 0 || i >= ss2_styles[style].n) return 0;
   return ss2_styles[style].mv[i].btn;
}

int ss2sp_move_flags(int style, int i)
{
   if (style < 0 || style >= SS2_STYLE_COUNT) return 0;
   if (i < 0 || i >= ss2_styles[style].n) return 0;
   return ss2_styles[style].mv[i].flags;
}

/* 커맨드를 넘패드 표기(236 / 623 / 41236 …)로 써 준다. 반환값 = 쓴 글자 수 */
int ss2sp_move_notation(int style, int i, char *out, int cap)
{
   static const struct { unsigned char pad; char ch; } tab[] = {
      {PAD_UP,'8'}, {PAD_DOWN,'2'}, {PAD_LEFT,'4'}, {PAD_RIGHT,'6'},
      {PAD_UP|PAD_RIGHT,'9'}, {PAD_UP|PAD_LEFT,'7'},
      {PAD_DOWN|PAD_RIGHT,'3'}, {PAD_DOWN|PAD_LEFT,'1'},
   };
   int k, j, n = 0;
   const ss2_move *m;
   if (!out || cap <= 0) return 0;
   out[0] = 0;
   if (style < 0 || style >= SS2_STYLE_COUNT) return 0;
   if (i < 0 || i >= ss2_styles[style].n) return 0;
   m = &ss2_styles[style].mv[i];
   for (k = 0; k < m->len && n < cap - 3; k++)
   {
      char c = '5';
      for (j = 0; j < (int)(sizeof tab / sizeof tab[0]); j++)
         if (tab[j].pad == m->motion[k]) { c = tab[j].ch; break; }
      out[n++] = c;
   }
   if (n < cap - 2) { out[n++] = '+'; out[n++] = (m->btn == 32) ? 'B' : 'A'; }
   out[n] = 0;
   return n;
}

int ss2sp_get_slot(int style, int slot)
{
   ss2_slots_ensure();
   if (style < 0 || style >= SS2_STYLE_COUNT) return -1;
   if (slot  < 0 || slot  >= SS2_SLOTS)       return -1;
   return ss2_slot_tbl[style][slot];
}

void ss2sp_set_slot(int style, int slot, int mv)
{
   ss2_slots_ensure();
   if (style < 0 || style >= SS2_STYLE_COUNT) return;
   if (slot  < 0 || slot  >= SS2_SLOTS)       return;
   if (mv >= ss2_styles[style].n) mv = -1;
   if (mv < 0) mv = -1;
   ss2_slot_tbl[style][slot] = (signed char)mv;
}

void ss2sp_reset_slots(void)
{
   memcpy(ss2_slot_tbl, ss2_spmap, sizeof ss2_slot_tbl);
   ss2_slot_ready = 1;
}

/* 저장/복원 — 0xFF = 없음(-1) */
void ss2sp_slots_blob(unsigned char *out)
{
   int s, k;
   ss2_slots_ensure();
   if (!out) return;
   for (s = 0; s < SS2_STYLE_COUNT; s++)
      for (k = 0; k < SS2_SLOTS; k++)
         *out++ = (unsigned char)(ss2_slot_tbl[s][k] < 0 ? 0xFF : ss2_slot_tbl[s][k]);
}

void ss2sp_load_slots(const unsigned char *in)
{
   int s, k;
   if (!in) return;
   ss2_slots_ensure();
   for (s = 0; s < SS2_STYLE_COUNT; s++)
      for (k = 0; k < SS2_SLOTS; k++)
      {
         unsigned char v = *in++;
         int mv = (v == 0xFF) ? -1 : (int)v;
         if (mv >= ss2_styles[s].n) mv = -1;
         ss2_slot_tbl[s][k] = (signed char)mv;
      }
}

void ss2sp_reset(void)
{
   q_n = q_i = q_left = 0;
   pending = 0; pending_left = 0;
   horiz_at = 0;
   prev_trig = 0;
   hold_bit = 0; hold_elapsed = 0; ss2sp_last_strong = 0;
   verify_left = 0;
   ss2sp_last_ok = -1;
   ss2sp_last_name = 0;
}
