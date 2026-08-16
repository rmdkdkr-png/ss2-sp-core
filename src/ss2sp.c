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

extern uint8_t CPUExRAM[16384];

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
#define HOLD_FRAMES   6         /* 마지막 방향 + 버튼 동시 유지 */
#define TAIL_FRAMES   2         /* 중립 복귀 */
#define MAX_STEPS     24

typedef struct { uint8_t pad; uint8_t frames; } ss2_step;

static ss2_step  q[MAX_STEPS];
static int       q_n, q_i, q_left;
static uint16_t  prev_trig;       /* 트리거 버튼 엣지 검출 */
static int       started_act;     /* 발동 직전 액션ID — 결과 판정용 */
static int       verify_left;
static const ss2_move *pending;   /* 공중에서 지상기를 눌렀을 때 착지까지 보류 */
static int       pending_left;
static uint32_t  frame_no;        /* 엔진 프레임 카운터 */
static uint32_t  horiz_at;        /* 순수 좌/우(아래 없음)를 마지막으로 잡고 있던 프레임 */

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
      }
      else
      {
         q[q_n].pad = d;
         q[q_n].frames = STEP_FRAMES;
      }
      q_n++;
   }
   q[q_n].pad = 0; q[q_n].frames = TAIL_FRAMES; q_n++;

   /* 실험용 스위치: SS2SP_NORESET=1 이면 리셋을 건너뛴다(대조군 측정용) */
   {
      static int noreset = -1;
      if (noreset < 0) { const char *e = getenv("SS2SP_NORESET"); noreset = (e && *e == '1'); }
      if (!noreset) CPUExRAM[OFF_CMDRESET] = 1;   /* ★ 입력 이력 리셋 */
   }

   q_left = q[0].frames;
   started_act = CPUExRAM[OFF_ACT] | (CPUExRAM[OFF_ACT + 1] << 8);
   verify_left = 90;
   ss2sp_last_name = m->name;
   ss2sp_last_ok = -1;
}

/* ── SP 모드: 버튼 하나 + 방향으로 슬롯을 고른다 ─────────────────
   브라우저판 resolveSpSlot과 같은 우선순위:
     실제 공중 > ↑(공중기 의도) > ↘/↙ > 아래 > 앞/뒤 > 중립
   앞/뒤는 캐릭터가 보는 방향 기준으로 계산한다(좌우 자동 반전). */
static const ss2_move *ss2_resolve_sp(const ss2_style *st, unsigned idx, uint8_t held)
{
   const signed char *map = ss2_spmap[idx];
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

/* 큐에서 이번 프레임에 내보낼 패드 1개를 꺼낸다. */
static uint8_t ss2_step_out(void)
{
   uint8_t out = q[q_i].pad;
   /* 히트스톱 중에는 프레임이 소비되지 않는다 — 카운트를 멈춘다.
      (브라우저판이 ms 타이머로 근사하던 부분. 코어에선 정확히 맞출 수 있다) */
   if (!CPUExRAM[OFF_HITSTOP] && --q_left <= 0)
   {
      if (++q_i < q_n) q_left = q[q_i].frames;
      else { q_n = q_i = 0; q_left = 0; }
   }
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
      return ss2_step_out();

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

         if (ss2_sp_layout() && slot == 0)
            m = ss2_resolve_sp(st, ss2_cur_idx, pad);   /* X = SP 버튼 */
         else
         {
            if (ss2_sp_layout()) slot--;                /* Y·L·R·L2·R2 = 1~5번 */
            if (slot >= 0 && slot < st->n) m = &st->mv[slot];
         }

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
      return ss2_step_out();

   return pad;
}

void ss2sp_reset(void)
{
   q_n = q_i = q_left = 0;
   pending = 0; pending_left = 0;
   horiz_at = 0;
   prev_trig = 0;
   verify_left = 0;
   ss2sp_last_ok = -1;
   ss2sp_last_name = 0;
}
