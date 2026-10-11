/* ss2fg — 프레임 생성(중간 프레임 합성). 설명은 ss2fg.h.
 *
 * 렌더러는 mednafen gfx.c 의 draw_scanline_colour 를 그대로 옮기되
 *   · 스크롤값·스프라이트 오프셋은 스캔라인별로 a→b 보간한 값을 쓰고
 *   · 스프라이트는 체인을 푼 절대좌표를 슬롯별로 보간한다(같은 타일·팔레트·플립·
 *     우선순위일 때만 — 아니면 b 자리).
 * 타일·타일맵·팔레트는 b 의 것. 창(window)·배경색·반전·플레인 순서도 b 의 것. */
#include <string.h>
#include <stdlib.h>
#include "ss2fg.h"

/* ───────────── 캡처 ─────────────
   지난 프레임 몇 장을 쌓아 둔다(최근이 0번). 4배(게임 박자 맞춤) 예측은 N+1·N+2 를 미리 돌려 두 장을
   쌓았다가 두 번 무른다. */
#define FG_RING 10                 /* 런어헤드 2 + 미리 4(느린 몸 박자) + 실제·앞 프레임이 다 들어가게 */
static ss2fg_frame fg_slot[FG_RING];
static int fg_hist[FG_RING] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };   /* fg_hist[0] = 마지막(현재), [1] = 그 앞 … (-1 = 없음) */
static int fg_build = 0;
static uint8_t fg_build_dirty;

static int next_free(void)
{
   int i, k, used;
   for (i = 0; i < FG_RING; i++)
   {
      used = 0;
      for (k = 0; k < FG_RING - 1; k++) if (fg_hist[k] == i) used = 1;
      if (!used) return i;
   }
   return fg_hist[FG_RING - 1] >= 0 ? fg_hist[FG_RING - 1] : 0;
}

void ss2fg_reset(void)
{
   int k;
   memset(fg_slot, 0, sizeof fg_slot);
   for (k = 0; k < FG_RING; k++) fg_hist[k] = -1;
   fg_build = 0;
   fg_build_dirty = 0;
}

void ss2fg_capture_line(int y, const ss2fg_regs *r)
{
   if (y < 0 || y >= SS2FG_H) return;
   fg_slot[fg_build].line[y] = *r;
}

void ss2fg_capture_write(int dirty_bit)
{
   fg_build_dirty |= (uint8_t)dirty_bit;
}

/* 체인 비트를 풀어 절대좌표를 만든다 — 원본 렌더러처럼 보이지 않는 스프라이트도 앵커를 민다 */
static void resolve_chain(ss2fg_frame *f)
{
   int i, lx = 0, ly = 0;
   for (i = 0; i < 64; i++)
   {
      const uint8_t *s = f->spr + i * 4;
      uint16_t data = (uint16_t)(s[0] | (s[1] << 8));
      int x = s[2], y = s[3];
      if (data & 0x0400) x = lx + s[2];
      if (data & 0x0200) y = ly + s[3];
      lx = x; ly = y;
      f->ax[i] = (int16_t)x;
      f->ay[i] = (int16_t)y;
   }
}

/* 사무쇼2 RAM 의 물체 표 (PocketCore 방 리버싱 실측; 이쪽 롬 덤프 600프레임에서도 몸 근처 조각의 겉모습 벡터와 1867:19 일치)
   물체 k = 0x0E00 + 0x40·k : +0x36 종류(16비트, 0xFFFF = 빈 칸), +0x38 X, +0x3A Y(땅 = 128)
   k 0·1 = P1·P2, 2·3 = 그림자, 4~7 = 기술 이펙트·장풍. 카메라 X 0x176D, 대전 중 = 0x00A7 이 241 */
#define FG_RAM_MODE   0x00A7
#define FG_RAM_FIGHT  241
#define FG_RAM_CAMX   0x176D
#define FG_RAM_OBJ    0x0E00
static const uint8_t *fg_ram = 0;
void ss2fg_set_ram(const uint8_t *ram) { fg_ram = ram; }

static void capture_body(ss2fg_frame *f)
{
   const ss2fg_frame *pv = fg_hist[0] >= 0 ? &fg_slot[fg_hist[0]] : 0;   /* 직전 캡처 */
   int k;
   f->now_ok = (uint8_t)(fg_ram && fg_ram[FG_RAM_MODE] == FG_RAM_FIGHT);
   f->ob_now_act = 0;
   for (k = 0; k < SS2FG_OBJS; k++)
   {
      const uint8_t *o = f->now_ok ? fg_ram + FG_RAM_OBJ + 0x40 * k : 0;
      int act = o && !(o[0x36] == 0xFF && o[0x37] == 0xFF);
      f->ob_now_x[k] = act ? (uint8_t)(o[0x38] - fg_ram[FG_RAM_CAMX]) : 0;
      f->ob_now_y[k] = act ? o[0x3A] : 0;
      f->ob_now_t[k] = act ? o[0x36] : 0;
      if (act) f->ob_now_act |= (uint8_t)(1u << k);
   }
   f->body_ok = (uint8_t)(f->now_ok && pv && pv->valid && pv->now_ok);
   for (k = 0; k < SS2FG_OBJS; k++)
   {
      f->ob_x[k] = f->body_ok ? pv->ob_now_x[k] : 0;
      f->ob_y[k] = f->body_ok ? pv->ob_now_y[k] : 0;
      f->ob_t[k] = f->body_ok ? pv->ob_now_t[k] : 0;
   }
   f->ob_act = f->body_ok ? pv->ob_now_act : 0;
}

void ss2fg_capture_end(const uint8_t *scroll, const uint8_t *chr, const uint8_t *spr,
                       const uint8_t *sprcol, const uint8_t *pal, int mono, int layers)
{
   ss2fg_frame *f = &fg_slot[fg_build];
   int k;
   capture_body(f);
   memcpy(f->scroll, scroll, sizeof f->scroll);
   memcpy(f->chr,    chr,    sizeof f->chr);
   memcpy(f->spr,    spr,    sizeof f->spr);
   memcpy(f->sprcol, sprcol, sizeof f->sprcol);
   memcpy(f->pal,    pal,    sizeof f->pal);
   f->mono   = (uint8_t)(mono != 0);
   f->layers = layers;
   f->dirty  = fg_build_dirty;
   f->valid  = 1;
   resolve_chain(f);

   for (k = FG_RING - 1; k > 0; k--) fg_hist[k] = fg_hist[k - 1];
   fg_hist[0] = fg_build;
   fg_build = next_free();
   fg_build_dirty = 0;
   /* 다음 프레임의 줄 레지스터는 처음부터 다시 채운다 — 못 채운 줄이 남지 않게 지난 값으로 초기화 */
   memcpy(fg_slot[fg_build].line, f->line, sizeof f->line);
}

void ss2fg_capture_pop(void)
{
   int k;
   if (fg_hist[0] < 0) return;
   fg_build = fg_hist[0];
   for (k = 0; k < FG_RING - 1; k++) fg_hist[k] = fg_hist[k + 1];
   fg_hist[FG_RING - 1] = -1;
   fg_build_dirty = 0;
}

const ss2fg_frame *ss2fg_hist(int k)
{
   if (k < 0 || k >= FG_RING || fg_hist[k] < 0 || !fg_slot[fg_hist[k]].valid) return 0;
   return &fg_slot[fg_hist[k]];
}
const ss2fg_frame *ss2fg_prev(void) { return ss2fg_hist(1); }
const ss2fg_frame *ss2fg_cur(void)  { return ss2fg_hist(0); }

/* ───────────── 렌더러 ───────────── */
#define ZD_BACK_SPRITE   2
#define ZD_BG_SCROLL     3
#define ZD_FG_SCROLL     5

static const unsigned char mirrored[256] = {
    0x00, 0x40, 0x80, 0xc0, 0x10, 0x50, 0x90, 0xd0, 0x20, 0x60, 0xa0, 0xe0, 0x30, 0x70, 0xb0, 0xf0,
    0x04, 0x44, 0x84, 0xc4, 0x14, 0x54, 0x94, 0xd4, 0x24, 0x64, 0xa4, 0xe4, 0x34, 0x74, 0xb4, 0xf4,
    0x08, 0x48, 0x88, 0xc8, 0x18, 0x58, 0x98, 0xd8, 0x28, 0x68, 0xa8, 0xe8, 0x38, 0x78, 0xb8, 0xf8,
    0x0c, 0x4c, 0x8c, 0xcc, 0x1c, 0x5c, 0x9c, 0xdc, 0x2c, 0x6c, 0xac, 0xec, 0x3c, 0x7c, 0xbc, 0xfc,
    0x01, 0x41, 0x81, 0xc1, 0x11, 0x51, 0x91, 0xd1, 0x21, 0x61, 0xa1, 0xe1, 0x31, 0x71, 0xb1, 0xf1,
    0x05, 0x45, 0x85, 0xc5, 0x15, 0x55, 0x95, 0xd5, 0x25, 0x65, 0xa5, 0xe5, 0x35, 0x75, 0xb5, 0xf5,
    0x09, 0x49, 0x89, 0xc9, 0x19, 0x59, 0x99, 0xd9, 0x29, 0x69, 0xa9, 0xe9, 0x39, 0x79, 0xb9, 0xf9,
    0x0d, 0x4d, 0x8d, 0xcd, 0x1d, 0x5d, 0x9d, 0xdd, 0x2d, 0x6d, 0xad, 0xed, 0x3d, 0x7d, 0xbd, 0xfd,
    0x02, 0x42, 0x82, 0xc2, 0x12, 0x52, 0x92, 0xd2, 0x22, 0x62, 0xa2, 0xe2, 0x32, 0x72, 0xb2, 0xf2,
    0x06, 0x46, 0x86, 0xc6, 0x16, 0x56, 0x96, 0xd6, 0x26, 0x66, 0xa6, 0xe6, 0x36, 0x76, 0xb6, 0xf6,
    0x0a, 0x4a, 0x8a, 0xca, 0x1a, 0x5a, 0x9a, 0xda, 0x2a, 0x6a, 0xaa, 0xea, 0x3a, 0x7a, 0xba, 0xfa,
    0x0e, 0x4e, 0x8e, 0xce, 0x1e, 0x5e, 0x9e, 0xde, 0x2e, 0x6e, 0xae, 0xee, 0x3e, 0x7e, 0xbe, 0xfe,
    0x03, 0x43, 0x83, 0xc3, 0x13, 0x53, 0x93, 0xd3, 0x23, 0x63, 0xa3, 0xe3, 0x33, 0x73, 0xb3, 0xf3,
    0x07, 0x47, 0x87, 0xc7, 0x17, 0x57, 0x97, 0xd7, 0x27, 0x67, 0xa7, 0xe7, 0x37, 0x77, 0xb7, 0xf7,
    0x0b, 0x4b, 0x8b, 0xcb, 0x1b, 0x5b, 0x9b, 0xdb, 0x2b, 0x6b, 0xab, 0xeb, 0x3b, 0x7b, 0xbb, 0xfb,
    0x0f, 0x4f, 0x8f, 0xcf, 0x1f, 0x5f, 0x9f, 0xdf, 0x2f, 0x6f, 0xaf, 0xef, 0x3f, 0x7f, 0xbf, 0xff
};

static inline uint16_t ld16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline int imin(int a, int b) { return a < b ? a : b; }
static inline int imax(int a, int b) { return a > b ? a : b; }

/* a + d·t/256, 반올림(내림 나눗셈 기반 → .5 는 올림) */
static inline int lerp_i(int a, int d, int t)
{
   int v = d * t + 128;
   return a + (v >= 0 ? v / 256 : -((-v + 255) / 256));
}

/* 8비트 랩 최단경로 보간 — 임계 넘으면(장면 전환) 제자리(a = base) */
static inline uint8_t lerp_wrap8(uint8_t a, uint8_t b, int t)
{
   int d = (int8_t)(uint8_t)(b - a);
   if (d > SS2FG_SCROLL_MAX_STEP || d < -SS2FG_SCROLL_MAX_STEP) return a;
   return (uint8_t)lerp_i(a, d, t);
}

/* 원본의 세로 좌표 접기: 249..255 → -7..-1, 나머지는 하위 8비트(256 이상은 안 접힌다) */
static inline int wrapc(int v)
{
   if (v > 248 && v < 256) return v - 256;
   return v & 0xFF;
}
/* 가로는 한 번 더 접힌다 — 원본이 drawColourPattern 에 uint8 로 넘기고 그 안에서 >0xF8 이면 -256 */
static inline int wrapx(int v)
{
   v &= 0xFF;
   return v > 0xF8 ? v - 256 : v;
}

/* gfx.c drawColourPattern 과 같은 동작 — x 는 이미 접힌 좌표(-7..248) */
/* 서기 사이 그림(패치 100) — 몸 조각만 빼고 줄을 다시 그릴 때: 뺄 조각, 줄 끝 깊이(zbuf) 사본, 화소마다 차지한 스프라이트 슬롯 */
static const uint8_t *rl_skip = 0;
static uint8_t *rl_zout = 0;
static int16_t *rl_slot = 0;
static int rl_cur_slot = -1;

static void draw_pattern(const ss2fg_frame *f, const ss2fg_regs *r, uint16_t *scan, uint8_t *zbuf,
                         int x, unsigned tile, unsigned tiley, int mirror,
                         const uint8_t *palette, unsigned pal, uint8_t depth)
{
   int index, left, right, highmark, xx;
   if (x >= SS2FG_W) return;

   index = ld16(f->chr + (tile * 16) + (tiley * 2));
   if (mirror)
      index = mirrored[(index & 0xff00) >> 8] | (mirrored[index & 0xff] << 8);

   palette += pal << 3;                       /* 4색 × 2바이트 */
   left     = imax(imax(x, r->winx), 0);
   right    = x + 7;
   highmark = imin(r->winw + r->winx, SS2FG_W) - 1;
   if (right > highmark)
   {
      index >>= (right - highmark) * 2;
      right = highmark;
   }
   for (xx = right; xx >= left; --xx, index >>= 2)
   {
      uint16_t c;
      if (depth <= zbuf[xx] || (index & 3) == 0) continue;
      zbuf[xx] = depth;
      if (rl_slot) rl_slot[xx] = (int16_t)rl_cur_slot;
      c = ld16(palette + ((index & 3) << 1));
      if (r->neg) c = (uint16_t)~c;
      scan[xx] = c;
   }
}

/* 반투명 그리기 — K2GE 12비트 색(0BGR)을 채널마다 섞는다. 앞에 더 높은 우선순위가 있으면(zbuf) 건너뛴다. zbuf 는 안 바꾼다.
   (PocketCore 방의 60_ss2fg_effects.patch 에서 가져왔다) */
static uint16_t mix12(uint16_t a, uint16_t b, int t)
{
   int r = ((a & 15) * (256 - t) + (b & 15) * t + 128) >> 8;
   int g = (((a >> 4) & 15) * (256 - t) + ((b >> 4) & 15) * t + 128) >> 8;
   int bl = (((a >> 8) & 15) * (256 - t) + ((b >> 8) & 15) * t + 128) >> 8;
   return (uint16_t)(r | (g << 4) | (bl << 8));
}
static void draw_pattern_blend(const ss2fg_frame *f, const ss2fg_regs *r, uint16_t *scan, const uint8_t *zbuf,
                               int x, unsigned tile, unsigned tiley, int mirror,
                               const uint8_t *palette, unsigned pal, uint8_t depth, int alpha)
{
   int index, left, right, highmark, xx;
   if (x >= SS2FG_W || alpha <= 0) return;
   index = ld16(f->chr + (tile * 16) + (tiley * 2));
   if (mirror)
      index = mirrored[(index & 0xff00) >> 8] | (mirrored[index & 0xff] << 8);
   palette += pal << 3;
   left     = imax(imax(x, r->winx), 0);
   right    = x + 7;
   highmark = imin(r->winw + r->winx, SS2FG_W) - 1;
   if (right > highmark) { index >>= (right - highmark) * 2; right = highmark; }
   for (xx = right; xx >= left; --xx, index >>= 2)
   {
      uint16_t c;
      if (depth < zbuf[xx] || (index & 3) == 0) continue;
      c = ld16(palette + ((index & 3) << 1));
      if (r->neg) c = (uint16_t)~c;
      scan[xx] = alpha >= 256 ? c : mix12(scan[xx], c, alpha);
   }
}

static void draw_scroll(const ss2fg_frame *f, const ss2fg_regs *r, uint16_t *scan, uint8_t *zbuf,
                        int y, int plane /*1·2*/, uint8_t sx, uint8_t sy, uint8_t depth)
{
   unsigned i;
   uint8_t line = (uint8_t)(y + sy);
   uint8_t row  = line & 7;
   const uint8_t *map = f->scroll + (plane == 2 ? 0x0800 : 0);
   const uint8_t *pal = f->pal + (plane == 2 ? 0x0100 : 0x0080);
   for (i = 0; i < 32; i++)
   {
      uint16_t d = ld16(map + ((i + ((line >> 3) << 5)) << 1));
      int x = ((i << 3) - sx) & 0xFF;
      if (x > 0xF8) x -= 256;
      draw_pattern(f, r, scan, zbuf, x, d & 0x01FF, (d & 0x4000) ? (7 - row) : row,
                   d & 0x8000, pal, (d & 0x1E00) >> 9, depth);
   }
}

/* ── 스프라이트 이동량 표 (프레임 쌍마다 한 번) ──
   슬롯 i 가 base·to 에서 같은 속성어(타일·플립·우선순위·팔레트)이고 이동 ≤ 임계면 후보 이동량.
   단, 같은 프레임 안에서 둘 이상 쓰이는 타일(빈 타일·반복 무늬·대칭 조각)은 다른 조각이 같은 슬롯에
   들어온 것일 수 있다 — 그 번호를 쓰는 조각들이 모두 같은 이동량일 때만(몸이 통째로 옮겨감) 믿고,
   아니면(자리 바꿈·일부만 바뀜) 그 번호의 조각은 전부 버린다.
   또 타일 번호가 같아도 **그림(문자 RAM 16바이트)이 바뀌었으면** 다른 조각이다 — 사무쇼2는 포즈가 바뀔 때
   슬롯·타일 번호는 그대로 두고 그 번호에 새 그림을 올려 쓰므로(체인은 안 씀), 번호만 보면 포즈 교대 프레임마다
   옛 그림이 새 배치의 중간 자리에 찍혀 깨진다(실기 보고: 뉴트럴 숨쉬기에서 두드러짐).
   체인으로 묶인 그룹(메타스프라이트)은 **강체**다: 믿을 수 있는 조각들의 이동량 중 가장 많이 모인
   값(±1px) 하나를 그룹 벡터로 삼아 그룹의 모든 조각을 그 벡터로만 옮긴다(과반이 안 모이면 그룹 전체
   정지). 포즈가 바뀌어 조각이 재배치되는 프레임(숨쉬기·공격)에서 조각이 흩어지지 않는다 — 몸 전체의
   이동만 보간하고 포즈는 base 그대로. 체인 밖 단독 스프라이트는 믿을 수 있는 후보일 때만 움직인다. */
typedef struct { int16_t dx, dy; uint8_t has; } fg_move;

/* «같은 조각» — 둘 다 보이고, 플립·우선순위(0xD800)·팔레트가 같고, 타일 그림 16바이트가 같다. 슬롯 번호·타일 번호는
   안 본다(사무쇼2는 번호가 자리표일 뿐이다). */
static int same_piece(const ss2fg_frame *base, int k, const ss2fg_frame *to, int j)
{
   uint16_t dk = ld16(base->spr + k * 4), dj = ld16(to->spr + j * 4);
   if (!(dk & 0x1800) || !(dj & 0x1800) || ((dj ^ dk) & 0xD800) || to->sprcol[j] != base->sprcol[k]) return 0;
   return memcmp(base->chr + (dk & 0x1FF) * 16, to->chr + (dj & 0x1FF) * 16, 16) == 0;
}

static int blank_tile(const uint8_t *t)
{
   int i;
   for (i = 0; i < 16; i++) if (t[i]) return 0;
   return 1;
}

#define FG_VOTE 12                                             /* 조각 한 개의 표 — 1·2·3·4 로 나누어떨어진다 */

/* 무리 [gs,ge) 의 보이는 조각 가운데, 「자리 + v」(±1px) 에 같은 그림·플립·팔레트·우선순위의 조각이 to 에 있는 수.
   슬롯·타일 번호는 보지 않는다 — 머리 조각 하나가 슬롯 0 에 끼어들어 번호가 한 칸씩 밀리면 슬롯별 짝은 타일 간격(-8px)
   만큼의 가짜 이동량을 내지만, 자리로 보면 몸은 그대로다(PocketCore 방 c0e2f79 의 검증 규칙). */
static int overlap_count(const ss2fg_frame *base, const ss2fg_frame *to, int gs, int ge, int vx, int vy)
{
   const ss2fg_regs *rb = &base->line[0], *rt = &to->line[0];
   int k, j, n = 0;
   for (k = gs; k < ge; k++)
   {
      uint16_t dk = ld16(base->spr + k * 4);
      int bx, by;
      if (!(dk & 0x1800)) continue;
      bx = wrapx(base->ax[k] + rb->spx) + vx; by = wrapc(base->ay[k] + rb->spy) + vy;
      for (j = 0; j < 64; j++)
      {
         int tx, ty;
         if (!same_piece(base, k, to, j)) continue;
         tx = wrapx(to->ax[j] + rt->spx); ty = wrapc(to->ay[j] + rt->spy);
         if (tx - bx > 1 || bx - tx > 1 || ty - by > 1 || by - ty > 1) continue;
         n++; break;
      }
   }
   return n;
}

static int16_t round_div(int sum, int n)
{
   return (int16_t)(sum >= 0 ? (2 * sum + n) / (2 * n) : -((-2 * sum + n) / (2 * n)));
}

/* 몸 위치로 무리 벡터 정하기 — 사무쇼2는 포즈가 바뀔 때 조각 내용·배치를 통째로 새로 쓴다. 맞고 빙글빙글 날아갈 때는
   2프레임마다 회전 포즈가 새로 그려져 겉모습이 맞는 조각이 없어, 그 몸만 보간 없이 30Hz 로 끊겼다. 게임이 RAM 에 둔
   몸 위치(=실제 궤적)를 쓰면 지금 포즈를 그 궤적으로 옮길 수 있다(회전 그림을 만들어 내진 않는다). 몸이 그대로면
   0 = 제자리. 몸 팔레트(P1 = 0, P2 = 5)이고 무리가 그 몸 위치 근처일 때만 1. (PocketCore 방 30_ss2fg_body.patch) */
/* 몸 팔레트(P1 0 · P2 5)가 아닌데 «몸 그 자체»인 무리 — 감전(해골)·불탐처럼 맞은 몸을 다른 팔레트로 그리는 동안.
   그 캐릭터의 몸 팔레트 조각이 몸 자리 근처에 «하나도» 없고, 이 무리가 6조각 이상이며 몸 자리에 붙어 있을 때만.
   (PocketCore 방 70_ss2fg_body_cadence.patch fda75d8 — 나찰·카즈키 판 실측: 감전돼 날아가는 몸이 팔레트 14 로 23조각,
   예전엔 «이펙트»로 보고 제자리에 뒀다) */
static int body_alt_owner(const ss2fg_frame *base, int gstart, int g)
{
   const ss2fg_regs *rb = &base->line[0];
   int pal = base->sprcol[gstart] & 0x0F, p, k, j, n = 0, best = -1, bscore = 17;
   if (!base->body_ok || pal == 0 || pal == 5 || pal == 12) return -1;
   for (k = gstart; k < g; k++) if (ld16(base->spr + k * 4) & 0x1800) n++;
   if (n < 6) return -1;
   for (p = 0; p < 2; p++)
   {
      int bp = p ? 5 : 0, has = 0, l = 999, r = -999, t = 999, b = -999, dx, dy;
      if (!(base->ob_act & (1u << p))) continue;
      for (j = 0; j < 64 && !has; j++)
      {
         int sx, sy;
         if (!(ld16(base->spr + j * 4) & 0x1800) || (base->sprcol[j] & 0x0F) != bp) continue;
         sx = (int8_t)(uint8_t)(base->ax[j] + rb->spx - base->ob_x[p]);
         sy = (int8_t)(uint8_t)(base->ay[j] + rb->spy - base->ob_y[p]);
         if (sx >= -48 && sx <= 40 && sy >= -96 && sy <= 8) has = 1;
      }
      if (has) continue;
      for (k = gstart; k < g; k++)
      {
         int sx, sy;
         if (!(ld16(base->spr + k * 4) & 0x1800)) continue;
         sx = (int8_t)(uint8_t)(base->ax[k] + rb->spx - base->ob_x[p]);
         sy = (int8_t)(uint8_t)(base->ay[k] + rb->spy - base->ob_y[p]);
         if (sx < l) l = sx;
         if (sx + 8 > r) r = sx + 8;
         if (sy < t) t = sy;
         if (sy + 8 > b) b = sy + 8;
      }
      /* 몸 자리(발 가운데)가 무리 테두리 안(세로는 아래로 8px 여유)이면 0, 밖이면 그 거리 — 두 캐릭터 중 가까운 쪽(16px 안) */
      dx = l > 0 ? l : r < 0 ? -r : 0;
      dy = t > 0 ? t : b + 8 < 0 ? -(b + 8) : 0;
      if (dx + dy < bscore) { bscore = dx + dy; best = p; }
   }
   return best;
}

static int fg_last_owner;                                     /* body_vec 이 마지막으로 정한 몸 번호 */
static int8_t fg_body_of[64];                                 /* 조각 → 몸 번호(0·1), 아니면 -1 — sprite_moves 가 채운다 */
/* 몸 따로 박자 — 느린 박자(감전돼 날아갈 때 4프레임에 한 번)나 다른 조각과 바뀌는 때가 어긋난 몸은, 그 몸의 지난 바뀜·다음 바뀜
   사이로 따로 보간한 자리(1/256 px)를 libretro.c(앱은 Main.cc)가 넣어 준다. 켜진 몸의 조각은 무리 판정 이동량 대신 이 값으로 옮긴다.
   한 번 그리기용 — render2 가 그리고 나면 끈다. */
static int fg_ov_on[2], fg_ov_x[2], fg_ov_y[2];
void ss2fg_body_override(int p, int on, int x256, int y256)
{
   if (p < 0 || p > 1) return;
   fg_ov_on[p] = on; fg_ov_x[p] = x256; fg_ov_y[p] = y256;
}
static int body_vec(const ss2fg_frame *base, const ss2fg_frame *to, int gstart, int g, int *vx, int *vy)
{
   const ss2fg_regs *rb = &base->line[0];
   int pal = base->sprcol[gstart] & 0x0F, p, k, dx, dy;
   int l = 999, r = -999, t = 999, b = -999;
   fg_last_owner = -1;
   if (!base->body_ok || !to->body_ok) return 0;
   p = pal == 0 ? 0 : pal == 5 ? 1 : body_alt_owner(base, gstart, g);
   if (p < 0) return 0;
   if (!(base->ob_act & to->ob_act & (1u << p))) return 0;
   dx = (int8_t)(uint8_t)(to->ob_x[p] - base->ob_x[p]);
   dy = (int8_t)(uint8_t)(to->ob_y[p] - base->ob_y[p]);
   if (dx > SS2FG_SPR_MAX_STEP || dx < -SS2FG_SPR_MAX_STEP || dy > SS2FG_SPR_MAX_STEP || dy < -SS2FG_SPR_MAX_STEP) return 0;
   for (k = gstart; k < g; k++)                                /* 몸 위치 기준 상대 좌표(8비트 랩) 테두리 */
   {
      int sx, sy;
      if (!(ld16(base->spr + k * 4) & 0x1800)) continue;
      sx = (int8_t)(uint8_t)(base->ax[k] + rb->spx - base->ob_x[p]);
      sy = (int8_t)(uint8_t)(base->ay[k] + rb->spy - base->ob_y[p]);
      if (sx < l) l = sx;
      if (sx + 8 > r) r = sx + 8;
      if (sy < t) t = sy;
      if (sy + 8 > b) b = sy + 8;
   }
   if (l > r) return 0;
   /* 몸 위치(발 가운데)가 무리 테두리 옆 40px 안, 세로는 무리 위 16px ~ 아래 56px 안(머리만 떨어진 무리 포함) */
   if (l > 40 || r < -40 || t > 16 || b < -56) return 0;
   *vx = dx; *vy = dy;
   fg_last_owner = p;
   return 1;
}

/* 이펙트·그림자 무리 — 몸 팔레트가 아닌 무리가 겉모습으로 안 정해지면(기술 이펙트는 매 프레임 모양이 바뀐다),
   RAM 물체 표의 그림자(2·3)·이펙트(4~7) 칸 중 그 무리 테두리에 가장 가까운(16px 안) 물체의 이동량을 쓴다.
   앞뒤 프레임 모두 같은 종류로 쓰여 있는 칸만(칸이 다른 이펙트로 재사용되면 거리가 엉터리). 캐릭터 칸(0·1)은
   안 쓴다 — 캐릭터 근처 이펙트가 캐릭터를 따라가지 않는 일이 많다. (PocketCore 방 40_ss2fg_objects.patch) */
#define FG_OBJ_NEAR 16
static int obj_vec(const ss2fg_frame *base, const ss2fg_frame *to, int gstart, int g, int *vx, int *vy)
{
   const ss2fg_regs *rb = &base->line[0];
   int pal = base->sprcol[gstart] & 0x0F, o, k, best = -1, bd = 999, dx, dy;
   if (!base->body_ok || !to->body_ok) return 0;
   if (pal == 0 || pal == 5) return 0;                        /* 몸은 body_vec */
   for (o = 2; o < SS2FG_OBJS; o++)
   {
      int l = 999, r = -999, t = 999, b = -999, ex, ey, d;
      if (!(base->ob_act & to->ob_act & (1u << o)) || base->ob_t[o] != to->ob_t[o]) continue;
      for (k = gstart; k < g; k++)
      {
         int sx, sy;
         if (!(ld16(base->spr + k * 4) & 0x1800)) continue;
         sx = (int8_t)(uint8_t)(base->ax[k] + rb->spx - base->ob_x[o]);
         sy = (int8_t)(uint8_t)(base->ay[k] + rb->spy - base->ob_y[o]);
         if (sx < l) l = sx;
         if (sx + 8 > r) r = sx + 8;
         if (sy < t) t = sy;
         if (sy + 8 > b) b = sy + 8;
      }
      if (l > r) return 0;
      ex = l > 0 ? l : r < 0 ? -r : 0;                        /* 물체 자리에서 무리 테두리까지 */
      ey = t > 0 ? t : b < 0 ? -b : 0;
      d = ex > ey ? ex : ey;
      if (d < bd) { bd = d; best = o; }
   }
   if (best < 0 || bd > FG_OBJ_NEAR) return 0;
   dx = (int8_t)(uint8_t)(to->ob_x[best] - base->ob_x[best]);
   dy = (int8_t)(uint8_t)(to->ob_y[best] - base->ob_y[best]);
   if (dx > SS2FG_SPR_MAX_STEP || dx < -SS2FG_SPR_MAX_STEP || dy > SS2FG_SPR_MAX_STEP || dy < -SS2FG_SPR_MAX_STEP) return 0;
   /* 맞춰 보기 — 물체 자리가 그림의 기준점이 아닌 이펙트가 많다(폭발 그림 옆 불똥 물체가 +18 로 움직였는데 다음 그림은
      제자리 → 앞으로 갔다 되돌아오는 흔들림). to 의 같은 팔레트 조각 테두리 가운데가 물체와 같이(±2px) 움직였을 때만
      믿는다. 그림자는 거의 늘 맞고, 모양이 바뀌는 이펙트는 대부분 여기서 걸러진다(→ 제자리). */
   {
      const ss2fg_regs *rt = &to->line[0];
      uint16_t key = ld16(base->spr + gstart * 4) & 0x1800;
      int l = 999, r = -999, t = 999, b = -999, l2 = 999, r2 = -999, t2 = 999, b2 = -999, j, cx, cy;
      for (k = gstart; k < g; k++)
      {
         int sx, sy;
         if (!(ld16(base->spr + k * 4) & 0x1800)) continue;
         sx = wrapx(base->ax[k] + rb->spx); sy = wrapc(base->ay[k] + rb->spy);
         if (sx < l) l = sx;
         if (sx + 8 > r) r = sx + 8;
         if (sy < t) t = sy;
         if (sy + 8 > b) b = sy + 8;
      }
      for (j = 0; j < 64; j++)
      {
         uint16_t w = ld16(to->spr + j * 4);
         int sx, sy;
         if ((w & 0x1800) != key || to->sprcol[j] != base->sprcol[gstart]) continue;
         sx = wrapx(to->ax[j] + rt->spx); sy = wrapc(to->ay[j] + rt->spy);
         if (sx + 8 < l - 40 || sx > r + 40 || sy + 8 < t - 40 || sy > b + 40) continue;
         if (sx < l2) l2 = sx;
         if (sx + 8 > r2) r2 = sx + 8;
         if (sy < t2) t2 = sy;
         if (sy + 8 > b2) b2 = sy + 8;
      }
      if (l2 > r2) return 0;                                  /* 다음 프레임에 그 그림이 없다 — 맞춰 볼 수 없음 */
      cx = (l2 + r2) - (l + r); cy = (t2 + b2) - (t + b);     /* 가운데 이동 ×2 */
      if (cx - 2 * dx > 4 || 2 * dx - cx > 4 || cy - 2 * dy > 4 || 2 * dy - cy > 4) return 0;
   }
   *vx = dx; *vy = dy;
   return 1;
}

/* 외톨이 조각 하나 — to 에 같은 조각(플립·우선순위·팔레트·그림)이 임계 안에 «딱 하나» 있으면 그 거리 */
static int single_look(const ss2fg_frame *base, const ss2fg_frame *to, int k, int *vx, int *vy)
{
   const ss2fg_regs *rb = &base->line[0], *rt = &to->line[0];
   uint16_t dk = ld16(base->spr + k * 4);
   int j, c = 0, bx, by;
   if (blank_tile(base->chr + (dk & 0x1FF) * 16)) return 0;
   bx = wrapx(base->ax[k] + rb->spx); by = wrapc(base->ay[k] + rb->spy);
   for (j = 0; j < 64; j++)
   {
      int dx, dy;
      if (!same_piece(base, k, to, j)) continue;
      dx = wrapx(to->ax[j] + rt->spx) - bx; dy = wrapc(to->ay[j] + rt->spy) - by;
      if (dx > SS2FG_SPR_MAX_STEP || dx < -SS2FG_SPR_MAX_STEP || dy > SS2FG_SPR_MAX_STEP || dy < -SS2FG_SPR_MAX_STEP) continue;
      if (++c > 1) return 0;
      *vx = dx; *vy = dy;
   }
   return c == 1;
}

static void set_group(const ss2fg_frame *base, fg_move mv[64], int gstart, int g, int vx, int vy)
{
   int k;
   for (k = gstart; k < g; k++)
      if (ld16(base->spr + k * 4) & 0x1800) { mv[k].dx = (int16_t)vx; mv[k].dy = (int16_t)vy; mv[k].has = 1; }
}

/* ── 이펙트·장풍 (모양이 매 프레임 바뀌는 무리) — PocketCore 방의 60_ss2fg_effects.patch 에서 가져와 main 의 판정 구조에 맞췄다 ──
   겉모습·몸·물체 위치 어느 것으로도 못 정한 무리(몸 팔레트 0·5 제외)를 세 가지로 다시 본다. 순서는 ③ RAM 물체 등속 →
   ② 몸에 붙음 → ① 테두리 등속. 그래도 못 정하면 제자리(옮기기) 또는 다음 그림과 반투명으로 겹침(옮기기+섞기). */
static int same_sprites(const ss2fg_frame *a, const ss2fg_frame *b)
{
   return !memcmp(a->spr, b->spr, sizeof a->spr) && !memcmp(a->sprcol, b->sprcol, sizeof a->sprcol) &&
          !memcmp(a->ax, b->ax, sizeof a->ax) && !memcmp(a->ay, b->ay, sizeof a->ay) &&
          a->line[0].spx == b->line[0].spx && a->line[0].spy == b->line[0].spy;
}
/* base 보다 앞선 캡처 중 스프라이트가 base 와 다른 가장 최근 것 (없으면 0) */
static const ss2fg_frame *older_change(const ss2fg_frame *base)
{
   int i, j;
   for (i = 0; i < FG_RING; i++)
      if (fg_hist[i] >= 0 && &fg_slot[fg_hist[i]] == base) break;
   if (i >= FG_RING) return 0;
   for (j = i + 1; j < FG_RING; j++)
   {
      const ss2fg_frame *f;
      if (fg_hist[j] < 0) return 0;
      f = &fg_slot[fg_hist[j]];
      if (!f->valid || f->mono || (f->dirty & SS2FG_DIRTY_SPR)) return 0;
      if (!same_sprites(f, base)) return f;
   }
   return 0;
}
/* f 에서 우선순위 key·팔레트 pal 조각 무리의 테두리 — 창(w0..w1, h0..h1)에 걸치는 조각을 씨앗으로, 16px 안에
   붙는 같은 팔레트 조각을 넓혀 모은다(한 이펙트가 슬롯 여러 구간에 흩어져 있어도 하나로). 반환 = 조각 수 */
static int fx_box(const ss2fg_frame *f, uint16_t key, uint8_t pal, int w0, int w1, int h0, int h1,
                  int *x0, int *x1, int *y0, int *y1)
{
   const ss2fg_regs *r = &f->line[0];
   int j, c = 0, grew = 1;
   int px[64], py[64], in[64], np = 0;
   *x0 = 999; *x1 = -999; *y0 = 999; *y1 = -999;
   for (j = 0; j < 64; j++)
   {
      uint16_t w = ld16(f->spr + j * 4);
      if ((w & 0x1800) != key || f->sprcol[j] != pal) continue;
      px[np] = wrapx(f->ax[j] + r->spx); py[np] = wrapc(f->ay[j] + r->spy);
      in[np] = !(px[np] + 8 < w0 || px[np] > w1 || py[np] + 8 < h0 || py[np] > h1);
      np++;
   }
   while (grew)
   {
      grew = 0;
      for (j = 0; j < np; j++)
      {
         int i;
         if (in[j]) continue;
         for (i = 0; i < np; i++)
            if (in[i] && abs(px[i] - px[j]) <= 16 && abs(py[i] - py[j]) <= 16) { in[j] = 1; grew = 1; break; }
      }
   }
   for (j = 0; j < np; j++)
   {
      if (!in[j]) continue;
      c++;
      if (px[j] < *x0) *x0 = px[j];
      if (px[j] + 8 > *x1) *x1 = px[j] + 8;
      if (py[j] < *y0) *y0 = py[j];
      if (py[j] + 8 > *y1) *y1 = py[j] + 8;
   }
   return c;
}
/* 시험용 계기: [0] 살핀 무리 [1] 물체 등속으로 옮김 [2] 몸에 붙여 옮김 [3] 테두리 등속으로 옮김
                [4] 앞 캡처 없음 [5] 앞·다음에 같은 팔레트 조각 없음 [6] 등속 아님(두 걸음 차이·너무 큼) [7] 2px 미만·양 끝 방향 어긋남 */
static int fx_stats[8];
static int fx_in_render = 0;
/* 이펙트 처리: 0 끔(예전 그대로) · 1 옮기기 · 2 옮기기 + 못 정한 것은 다음 그림과 반투명 섞기 (코어 옵션 ngp_framegen_fx) */
static int fx_mode = 1;
void ss2fg_set_fx(int mode) { fx_mode = mode < 0 ? 0 : mode > 2 ? 2 : mode; }
int  ss2fg_get_fx(void) { return fx_mode; }
void ss2fg_fx_stats(int *out8) { int i; for (i = 0; i < 8; i++) out8[i] = fx_stats[i]; }
static uint8_t fx_fade_b[64];        /* render 중: base 조각이 «섞어 사라질» 이펙트인가 */
static uint8_t fx_fade_in[64];       /* render 중: to 조각 중 «섞어 나타날» 이펙트 */
static int fx_fade_on = 0, fx_fade_t = 0;
static const ss2fg_frame *fx_fade_to = 0;
/* base 무리(gstart..g)를 씨앗으로 같은 팔레트 이펙트 전체 테두리 */
static void fx_base_box(const ss2fg_frame *base, int gstart, int g, int *x0, int *x1, int *y0, int *y1)
{
   const ss2fg_regs *rb = &base->line[0];
   int k;
   *x0 = 999; *x1 = -999; *y0 = 999; *y1 = -999;
   for (k = gstart; k < g; k++)
   {
      uint16_t w = ld16(base->spr + k * 4);
      int sx, sy;
      if (!(w & 0x1800)) continue;
      sx = wrapx(base->ax[k] + rb->spx); sy = wrapc(base->ay[k] + rb->spy);
      if (sx < *x0) *x0 = sx;
      if (sx + 8 > *x1) *x1 = sx + 8;
      if (sy < *y0) *y0 = sy;
      if (sy + 8 > *y1) *y1 = sy + 8;
   }
   if (*x0 <= *x1)
      fx_box(base, ld16(base->spr + gstart * 4) & 0x1800, base->sprcol[gstart], *x0, *x1 - 1, *y0, *y1 - 1, x0, x1, y0, y1);
}
static int half_round(int v2) { return v2 >= 0 ? (v2 + 1) / 2 : -((-v2 + 1) / 2); }

/* ① 장풍(따로 날아가는 것) — «직전 바뀐 프레임 → base → to» 두 걸음의 테두리 가운데 이동이 2px 안으로 같고,
   진행 방향으로 테두리 양쪽 끝이 다 같은 쪽으로 움직였을 때(돌면서 날아가는 삼각 장풍처럼 모양·크기가 바뀌어도 됨).
   한쪽 끝이 고정된 «늘어나는» 이펙트(칼 궤적)는 가운데가 반만 움직여 등속처럼 보이므로 양쪽 끝 검사로 거른다. */
static int fx_vec(const ss2fg_frame *base, const ss2fg_frame *to, const ss2fg_frame *prev, int gstart, int g,
                  int *vx, int *vy)
{
   uint16_t key = ld16(base->spr + gstart * 4) & 0x1800;
   uint8_t pal = base->sprcol[gstart];
   int bx0, bx1, by0, by1, tx0, tx1, ty0, ty1, px0, px1, py0, py1;
   int c2bx, c2by, v2x, v2y, v1x, v1y;
   if (!prev) { fx_stats[4]++; return 0; }
   fx_base_box(base, gstart, g, &bx0, &bx1, &by0, &by1);
   if (bx0 > bx1) return 0;
   if (!fx_box(to, key, pal, bx0 - 24, bx1 + 24, by0 - 24, by1 + 24, &tx0, &tx1, &ty0, &ty1) ||
       !fx_box(prev, key, pal, bx0 - 24, bx1 + 24, by0 - 24, by1 + 24, &px0, &px1, &py0, &py1))
   { fx_stats[5]++; return 0; }
   c2bx = bx0 + bx1; c2by = by0 + by1;
   v2x = (tx0 + tx1) - c2bx; v2y = (ty0 + ty1) - c2by;
   v1x = c2bx - (px0 + px1); v1y = c2by - (py0 + py1);
   if (abs(v2x - v1x) > 4 || abs(v2y - v1y) > 4) { fx_stats[6]++; return 0; }
   if (abs(v2x) + abs(v2y) < 4) { fx_stats[7]++; return 0; }        /* 2px 미만 — 옮길 것 없음 */
   /* 진행 축(더 많이 움직인 축)에서 양쪽 끝이 두 걸음 모두 같은 방향으로 */
   if (abs(v2x) >= abs(v2y))
   {
      int sgn = v2x > 0 ? 1 : -1;
      if ((tx0 - bx0) * sgn <= 0 || (tx1 - bx1) * sgn <= 0 || (bx0 - px0) * sgn <= 0 || (bx1 - px1) * sgn <= 0)
      { fx_stats[7]++; return 0; }
   }
   else
   {
      int sgn = v2y > 0 ? 1 : -1;
      if ((ty0 - by0) * sgn <= 0 || (ty1 - by1) * sgn <= 0 || (by0 - py0) * sgn <= 0 || (by1 - py1) * sgn <= 0)
      { fx_stats[7]++; return 0; }
   }
   *vx = half_round(v2x); *vy = half_round(v2y);
   if (*vx > SS2FG_SPR_MAX_STEP || *vx < -SS2FG_SPR_MAX_STEP || *vy > SS2FG_SPR_MAX_STEP || *vy < -SS2FG_SPR_MAX_STEP)
   { fx_stats[6]++; return 0; }
   fx_stats[3]++;
   return 1;
}

static int fx_on_body(const ss2fg_frame *f, int o)
{
   int p;
   for (p = 0; p < 2; p++)
      if ((f->ob_act >> p) & 1 && abs((int)f->ob_x[o] - (int)f->ob_x[p]) <= 1)
         return 1;
   return 0;
}

/* ③ RAM 물체로 나는 장풍 — 돌면서 날아가는 삼각 장풍처럼 그림 테두리는 회전 때문에 들쭉날쭉해도, 게임의 물체 자리(RAM)는
   한 걸음에 -8px 씩 똑같이 간다(나찰 판 실측: 101→93→85→77…). 이펙트 근처(8px — 장풍은 제 물체 자리를 감싸고 그려진다)의
   물체 «모두»를 보고, 앞 걸음과 이번 걸음의 이동이 1px 안으로 같으면서 다음 그림 테두리 가운데가 그 이동과 가장 잘 맞는
   (8px 안) 물체를 고른다. 몸과 같은 x 에 붙어 다니는 물체(그림자·몸에 딸린 이펙트)는 몸 이동 그대로라 장풍 길잡이가 못 되므로
   뺀다(몸에 붙은 이펙트는 ② 가 맡는다). 가까운 물체 하나만 보면 옆을 지나는 캐릭터 그림자(+2~3px)를 잡아 장풍을 거꾸로
   끌고 가는 일이 생겼다(PocketCore 방 나찰 판 2255 실측, 패치 60 두 번째 판 0f42d23). */
static int fx_objpath(const ss2fg_frame *base, const ss2fg_frame *to, const ss2fg_frame *prev, int gstart, int g,
                      int *vx, int *vy)
{
   const ss2fg_regs *rb = &base->line[0];
   uint16_t key = ld16(base->spr + gstart * 4) & 0x1800;
   uint8_t pal = base->sprcol[gstart];
   int o, k, best = 999, bvx = 0, bvy = 0, bx0, bx1, by0, by1;
   if (!prev || !base->body_ok || !to->body_ok || !prev->body_ok) return 0;
   fx_base_box(base, gstart, g, &bx0, &bx1, &by0, &by1);
   if (bx0 > bx1) return 0;
   for (o = 2; o < SS2FG_OBJS; o++)
   {
      int l = 999, r = -999, t = 999, b = -999, ex, ey, d, d1x, d1y, d2x, d2y, tx0, tx1, ty0, ty1, err;
      if (!(base->ob_act & to->ob_act & prev->ob_act & (1u << o))) continue;
      if (base->ob_t[o] != to->ob_t[o] || base->ob_t[o] != prev->ob_t[o]) continue;
      if (fx_on_body(base, o) || fx_on_body(to, o)) continue;
      for (k = gstart; k < g; k++)
      {
         int sx, sy;
         if (!(ld16(base->spr + k * 4) & 0x1800)) continue;
         sx = (int8_t)(uint8_t)(base->ax[k] + rb->spx - base->ob_x[o]);
         sy = (int8_t)(uint8_t)(base->ay[k] + rb->spy - base->ob_y[o]);
         if (sx < l) l = sx;
         if (sx + 8 > r) r = sx + 8;
         if (sy < t) t = sy;
         if (sy + 8 > b) b = sy + 8;
      }
      if (l > r) continue;
      ex = l > 0 ? l : r < 0 ? -r : 0;
      ey = t > 0 ? t : b < 0 ? -b : 0;
      d = ex > ey ? ex : ey;
      if (d > 8) continue;                                     /* 장풍은 제 물체 자리를 감싸고(8px 안) 그려진다 */
      d2x = (int8_t)(uint8_t)(to->ob_x[o] - base->ob_x[o]);  d2y = (int8_t)(uint8_t)(to->ob_y[o] - base->ob_y[o]);
      d1x = (int8_t)(uint8_t)(base->ob_x[o] - prev->ob_x[o]); d1y = (int8_t)(uint8_t)(base->ob_y[o] - prev->ob_y[o]);
      if (abs(d2x - d1x) > 1 || abs(d2y - d1y) > 1) continue;
      if (abs(d2x) + abs(d2y) < 1) continue;
      if (d2x > SS2FG_SPR_MAX_STEP || d2x < -SS2FG_SPR_MAX_STEP || d2y > SS2FG_SPR_MAX_STEP || d2y < -SS2FG_SPR_MAX_STEP) continue;
      if (!fx_box(to, key, pal, bx0 - 24 + d2x, bx1 + 24 + d2x, by0 - 24 + d2y, by1 + 24 + d2y, &tx0, &tx1, &ty0, &ty1)) continue;
      ex = abs((tx0 + tx1) - (bx0 + bx1) - 2 * d2x); ey = abs((ty0 + ty1) - (by0 + by1) - 2 * d2y);
      err = ex > ey ? ex : ey;
      if (err <= 16 && err < best) { best = err; bvx = d2x; bvy = d2y; }
   }
   if (best == 999) return 0;
   *vx = bvx; *vy = bvy;
   fx_stats[1]++;
   return 1;
}

/* ② 몸에 붙은 이펙트(칼 궤적·기 모으기 등) — 몸이 움직이는데 이펙트만 제자리면 중간 그림에서 몸에서 떨어져 보인다.
   이펙트가 몸 근처이고, «앞 걸음과 이번 걸음 둘 다» 같은 팔레트 이펙트 테두리 가운데가 몸 이동과 6px 안으로 같이 움직였으면
   몸 이동량(RAM — 정확)으로 옮긴다. 한 걸음만 보면 몸 옆을 지나 날아가는 장풍이 우연히 맞아 몸에 붙어 끌려갔다(나찰 판 2255).
   맞은 자리에 고정된 불똥은 몸만 밀려나고 불똥은 제자리라 걸리지 않는다. */
static int fx_attach(const ss2fg_frame *base, const ss2fg_frame *to, const ss2fg_frame *prev, int gstart, int g,
                     int *vx, int *vy)
{
   uint16_t key = ld16(base->spr + gstart * 4) & 0x1800;
   uint8_t pal = base->sprcol[gstart];
   int bx0, bx1, by0, by1, tx0, tx1, ty0, ty1, px0, px1, py0, py1, p, best = 99, bvx = 0, bvy = 0;
   if (!prev || !base->body_ok || !to->body_ok || !prev->body_ok) return 0;
   fx_base_box(base, gstart, g, &bx0, &bx1, &by0, &by1);
   if (bx0 > bx1) return 0;
   for (p = 0; p < 2; p++)
   {
      int dx, dy, d1x, d1y, ex, ey, rx0, rx1, ry0, ry1, err, err1;
      if (!(prev->ob_act & base->ob_act & to->ob_act & (1u << p))) continue;
      dx = (int8_t)(uint8_t)(to->ob_x[p] - base->ob_x[p]);
      dy = (int8_t)(uint8_t)(to->ob_y[p] - base->ob_y[p]);
      d1x = (int8_t)(uint8_t)(base->ob_x[p] - prev->ob_x[p]);
      d1y = (int8_t)(uint8_t)(base->ob_y[p] - prev->ob_y[p]);
      if (abs(dx) + abs(dy) < 2) continue;                       /* 몸이 안 움직이면 붙일 이유 없음 */
      if (dx > SS2FG_SPR_MAX_STEP || dx < -SS2FG_SPR_MAX_STEP || dy > SS2FG_SPR_MAX_STEP || dy < -SS2FG_SPR_MAX_STEP) continue;
      /* 몸(발 가운데) 기준 상대 테두리 — 옆 56px, 위 80px ~ 아래 24px 안에 걸쳐야 «몸 근처» */
      rx0 = bx0 - (int)base->ob_x[p]; rx1 = bx1 - (int)base->ob_x[p];
      ry0 = by0 - (int)base->ob_y[p]; ry1 = by1 - (int)base->ob_y[p];
      if (rx0 > 56 || rx1 < -56 || ry0 > 24 || ry1 < -80) continue;
      if (!fx_box(to, key, pal, bx0 - 24 + dx, bx1 + 24 + dx, by0 - 24 + dy, by1 + 24 + dy, &tx0, &tx1, &ty0, &ty1)) continue;
      if (!fx_box(prev, key, pal, bx0 - 24 - d1x, bx1 + 24 - d1x, by0 - 24 - d1y, by1 + 24 - d1y, &px0, &px1, &py0, &py1)) continue;
      ex = (tx0 + tx1) - (bx0 + bx1) - 2 * dx; ey = (ty0 + ty1) - (by0 + by1) - 2 * dy;
      err = abs(ex) > abs(ey) ? abs(ex) : abs(ey);
      ex = (bx0 + bx1) - (px0 + px1) - 2 * d1x; ey = (by0 + by1) - (py0 + py1) - 2 * d1y;
      err1 = abs(ex) > abs(ey) ? abs(ex) : abs(ey);
      if (err1 > err) err = err1;
      if (err <= 12 && err < best) { best = err; bvx = dx; bvy = dy; }
   }
   if (best == 99) return 0;
   *vx = bvx; *vy = bvy;
   fx_stats[2]++;
   return 1;
}

static void sprite_moves(const ss2fg_frame *base, const ss2fg_frame *to, fg_move mv[64])
{
   const ss2fg_regs *rb = &base->line[0], *rt = &to->line[0];
   uint8_t cnt_b[512], cnt_t[512];
   fg_move cand[64];
   uint8_t und[64];                 /* und[gstart] = 그 무리 끝(g) — 아무것으로도 못 정한 비체인 무리 (0 = 아님) */
   int i, g, gstart;
   memset(mv, 0, 64 * sizeof *mv);
   memset(und, 0, sizeof und);
   memset(fg_body_of, 0xFF, sizeof fg_body_of);
   memset(cand, 0, sizeof cand);
   memset(cnt_b, 0, sizeof cnt_b);
   memset(cnt_t, 0, sizeof cnt_t);

   for (i = 0; i < 64; i++)
   {
      uint16_t db = ld16(base->spr + i * 4), dt = ld16(to->spr + i * 4);
      if ((db & 0x1800) && cnt_b[db & 0x1FF] < 255) cnt_b[db & 0x1FF]++;
      if ((dt & 0x1800) && cnt_t[dt & 0x1FF] < 255) cnt_t[dt & 0x1FF]++;
   }

   for (i = 0; i < 64; i++)
   {
      uint16_t db = ld16(base->spr + i * 4), dt = ld16(to->spr + i * 4);
      int bx, by, tx, ty, dx, dy;
      if (!(db & 0x1800) || !(dt & 0x1800)) continue;          /* 둘 다 보여야 */
      if (db != dt || base->sprcol[i] != to->sprcol[i]) continue;
      if (memcmp(base->chr + (db & 0x1FF) * 16, to->chr + (db & 0x1FF) * 16, 16) != 0)
         continue;                                                 /* 타일 그림이 바뀌었다(포즈 교대: 같은 번호에 새 그림을 올려 씀) — 다른 조각이다 */
      bx = wrapx(base->ax[i] + rb->spx); by = wrapc(base->ay[i] + rb->spy);
      tx = wrapx(to->ax[i]   + rt->spx); ty = wrapc(to->ay[i]   + rt->spy);
      dx = tx - bx; dy = ty - by;
      if (dx > SS2FG_SPR_MAX_STEP || dx < -SS2FG_SPR_MAX_STEP ||
          dy > SS2FG_SPR_MAX_STEP || dy < -SS2FG_SPR_MAX_STEP) continue;
      cand[i].dx = (int16_t)dx; cand[i].dy = (int16_t)dy; cand[i].has = 1;
   }

   /* 공용 타일(같은 프레임에서 둘 이상 쓰이는 번호)은 다른 조각이 같은 슬롯에 들어온 것일 수 있다. 그 번호를 쓰는
      보이는 조각들이 **모두** 후보이고 **같은 이동량**이면(몸이 통째로 옮겨감) 믿고, 하나라도 다르거나 빠지면
      (자리 바꿈·일부만 바뀜) 그 번호의 조각은 전부 버린다. to 쪽 사용 수가 다르면 조각이 생기거나 사라진 것 — 버린다 */
   for (i = 0; i < 64; i++)
   {
      uint16_t db; int t, j, ok;
      if (!cand[i].has) continue;
      db = ld16(base->spr + i * 4); t = db & 0x1FF;
      if (cnt_b[t] == 1 && cnt_t[t] == 1) continue;
      ok = (cnt_b[t] == cnt_t[t]);
      for (j = 0; ok && j < 64; j++)
      {
         uint16_t dj = ld16(base->spr + j * 4);
         if (j == i || !(dj & 0x1800) || (dj & 0x1FF) != t) continue;
         if (!cand[j].has || cand[j].dx != cand[i].dx || cand[j].dy != cand[i].dy) ok = 0;
      }
      if (!ok)
      {  /* 이 번호의 조각 전부를 버린다(i 뒤의 조각도 같은 판정이 나오지만 앞의 조각은 이미 지나갔으니 여기서 지운다) */
         for (j = 0; j < 64; j++)
         {
            uint16_t dj = ld16(base->spr + j * 4);
            if ((dj & 0x1800) && (dj & 0x1FF) == t) cand[j].has = 0;
         }
      }
   }

   /* 무리(체인 비트 없는 메타스프라이트 — 사무쇼2 방식): 우선순위·팔레트가 같고 지금까지의 무리 테두리에서
      16px 안에 붙는 연속 슬롯을 한 캐릭터로 본다(PocketCore 방의 ss2fg_consensus.patch 에서 가져온 묶음 규칙).
      무리는 강체다: 조각마다 to 전체에서 «같은 조각»(플립·우선순위·팔레트·타일 그림이 같은 것 — 슬롯·타일 번호는
      안 본다. 사무쇼2는 조각이 끼어들면 뒤 번호가 밀리고, 포즈가 바뀌면 같은 번호에 새 그림을 올려 쓴다)을 찾아
      그 거리에 표를 던지고, 최빈 이동량 하나로 **포즈가 바뀐 조각까지** 함께 옮긴다 — 걷는 중 포즈 교대 프레임에도
      몸은 계속 움직이고 조각은 흩어지지 않는다. 짝이 여럿이면 표를 나누고(FG_VOTE/m) 넷을 넘으면(흔한 무늬) 기권,
      빈 조각도 기권. 1등에 표를 준 조각이 둘 미만이거나, 2등의 두 배가 안 되거나, 그림 있는 조각의 1/4 에 못 미치면
      무리 전체 정지.
      (겉모습 짝짓기는 PocketCore 방의 패치 3판에서 가져왔다.) 체인 비트가 섞인 구간은 아래 체인 규칙에 맡긴다. */
   for (gstart = 0; gstart < 64; gstart = g)
   {
      uint16_t w0 = ld16(base->spr + gstart * 4);
      int vdx[256], vdy[256], vw[256], pm[64], pdx[64][4], pdy[64][4], nvote = 0, nvis = 0, nlook = 0, npc = 0, k, a, best = -1, bestw = 0, second = 0, sumx = 0, sumy = 0;
      int body, bvx = 0, bvy = 0;
      int bx0 = base->ax[gstart], bx1 = base->ax[gstart] + 8, by0 = base->ay[gstart], by1 = base->ay[gstart] + 8;
      if (w0 & 0x0600) { g = gstart + 1; continue; }             /* 체인 비트가 달린 슬롯은 체인 규칙 몫 */
      if (gstart + 1 < 64 && (ld16(base->spr + (gstart + 1) * 4) & 0x0600))
      {  /* 다음 슬롯이 체인 → 이 슬롯은 체인 그룹의 앵커다. 앵커와 그 체인은 아래 체인 규칙이 다룬다 */
         for (g = gstart + 1; g < 64 && (ld16(base->spr + g * 4) & 0x0600); g++) ;
         continue;
      }
      for (g = gstart + 1; g < 64; g++)
      {
         uint16_t w = ld16(base->spr + g * 4);
         int sx = base->ax[g], sy = base->ay[g];
         if (w & 0x0600) break;                                  /* 체인 시작(앞 슬롯 g-1 이 앵커) — 아래에서 떼어 낸다 */
         if ((w & 0x1800) != (w0 & 0x1800) || base->sprcol[g] != base->sprcol[gstart]) break;
         if (sx + 8 < bx0 - 16 || sx > bx1 + 16 || sy + 8 < by0 - 16 || sy > by1 + 16) break;
         if (sx < bx0) bx0 = sx;
         if (sx + 8 > bx1) bx1 = sx + 8;
         if (sy < by0) by0 = sy;
         if (sy + 8 > by1) by1 = sy + 8;
      }
      if (g < 64 && (ld16(base->spr + g * 4) & 0x0600) && g - 1 > gstart)
         g--;                                                    /* 체인 앵커(g-1)는 무리에서 뺀다 — 다음 반복이 앵커로 시작해 위에서 건너뛴다 */
      for (k = gstart; k < g; k++)
         if (ld16(base->spr + k * 4) & 0x1800) nvis++;
      body = body_vec(base, to, gstart, g, &bvx, &bvy);
      if (body) for (k = gstart; k < g; k++) fg_body_of[k] = (int8_t)fg_last_owner;   /* 몸 박자 따로 보간에 쓴다 */
      if (!body && obj_vec(base, to, gstart, g, &bvx, &bvy)) body = 2;   /* 그림자·이펙트: 가장 가까운 물체(맞춰 본 것만) */
      if (nvis < 2)
      {
         if (body && nvis == 1)
         {  /* 외톨이 조각(돌며 날아갈 때 몸에서 떨어져 나온 조각 등) — 몸 위치를 알면 슬롯 번호 짝(엉뚱한 +22px 따위) 대신,
               같은 조각이 딱 하나면 그 거리, 아니면 몸 위치로 */
            for (k = gstart; k < g; k++)
               if (ld16(base->spr + k * 4) & 0x1800)
               {
                  int vx = bvx, vy = bvy;
                  single_look(base, to, k, &vx, &vy);
                  mv[k].dx = (int16_t)vx; mv[k].dy = (int16_t)vy; mv[k].has = 1;
               }
         }
         else
         {
            int dec = 0;
            for (k = gstart; k < g; k++) if (cand[k].has) { mv[k] = cand[k]; dec = 1; }   /* 단독 스프라이트 — 슬롯 규칙 */
            if (!dec && nvis == 1) und[gstart] = (uint8_t)g;             /* 아무 짝도 없는 외톨이 — 아래 이펙트 규칙 후보 */
         }
         continue;
      }
      for (k = gstart; k < g; k++)
      {
         uint16_t dk = ld16(base->spr + k * 4);
         int *mdx = pdx[k], *mdy = pdy[k], m = 0, bx, by, j;
         pm[k] = 0;
         if (!(dk & 0x1800) || blank_tile(base->chr + (dk & 0x1FF) * 16)) continue;
         nlook++;
         bx = wrapx(base->ax[k] + rb->spx); by = wrapc(base->ay[k] + rb->spy);
         for (j = 0; j < 64 && m <= 4; j++)
         {
            int dx, dy;
            if (!same_piece(base, k, to, j)) continue;
            dx = wrapx(to->ax[j] + rt->spx) - bx; dy = wrapc(to->ay[j] + rt->spy) - by;
            if (dx > SS2FG_SPR_MAX_STEP || dx < -SS2FG_SPR_MAX_STEP ||
                dy > SS2FG_SPR_MAX_STEP || dy < -SS2FG_SPR_MAX_STEP) continue;
            if (m < 4) { mdx[m] = dx; mdy[m] = dy; }
            m++;
         }
         if (m == 0 || m > 4) continue;                          /* 바뀐 조각 / 너무 흔한 조각 */
         pm[k] = m;
         for (j = 0; j < m; j++)
         {
            for (a = 0; a < nvote; a++) if (vdx[a] == mdx[j] && vdy[a] == mdy[j]) break;
            if (a == nvote) { if (nvote == 256) continue; vdx[nvote] = mdx[j]; vdy[nvote] = mdy[j]; vw[nvote] = 0; nvote++; }
            vw[a] += FG_VOTE / m;
         }
      }
      if (nvote == 0) goto body_or_stop;                        /* 믿을 조각이 없다(포즈 전체 교체) → 몸 위치가 있으면 그것으로, 없으면 무리 정지 */
      for (a = 0; a < nvote; a++)
      {
         int w = 0, b;
         for (b = 0; b < nvote; b++)
         {
            int ddx = vdx[b] - vdx[a], ddy = vdy[b] - vdy[a];
            if (ddx >= -1 && ddx <= 1 && ddy >= -1 && ddy <= 1) w += vw[b];
         }
         if (w > bestw) { bestw = w; best = a; }
      }
      /* 2등(1등의 ±1px 창 밖에서 가장 많이 모인 이동량)의 두 배는 되어야 뚜렷한 1등이다 — 반복 무늬(같은 그림이
         줄지어 선 효과)는 짝이 여럿이라 표가 ±8·±16 으로 대칭으로 흩어지는데, 그 조각들도 진짜 이동량에는 같이
         표를 주므로 1등은 뚜렷하다. 과반 규칙은 그런 무리를 괜히 세웠다(롬 실측: 가로 6조각 효과 줄, 4조각이 같은 그림). */
      for (a = 0; a < nvote; a++)
      {
         int w = 0, b, ddx = vdx[a] - vdx[best], ddy = vdy[a] - vdy[best];
         if (ddx >= -1 && ddx <= 1 && ddy >= -1 && ddy <= 1) continue;
         for (b = 0; b < nvote; b++)
         {
            int ex = vdx[b] - vdx[a], ey = vdy[b] - vdy[a], fx = vdx[b] - vdx[best], fy = vdy[b] - vdy[best];
            if (fx >= -1 && fx <= 1 && fy >= -1 && fy <= 1) continue;             /* 1등 창의 표는 안 센다 */
            if (ex >= -1 && ex <= 1 && ey >= -1 && ey <= 1) w += vw[b];
         }
         if (w > second) second = w;
      }
      for (k = gstart; k < g; k++)                              /* 1등 창에 표를 준 조각 수 — 한 조각(관절)이 몸을 끌지 않게 둘 이상 */
      {
         int j;
         for (j = 0; j < pm[k]; j++)
         {
            int ddx = pdx[k][j] - vdx[best], ddy = pdy[k][j] - vdy[best];
            if (ddx >= -1 && ddx <= 1 && ddy >= -1 && ddy <= 1) { npc++; break; }
         }
      }
      if (npc < 2 || bestw < 2 * second || bestw * 4 < nlook * FG_VOTE) goto body_or_stop;   /* 표 부족 */
      for (a = 0; a < nvote; a++)
      {
         int ddx = vdx[a] - vdx[best], ddy = vdy[a] - vdy[best];
         if (ddx >= -1 && ddx <= 1 && ddy >= -1 && ddy <= 1) { sumx += vdx[a] * vw[a]; sumy += vdy[a] * vw[a]; }
      }
      {  /* 자리 겹침 검증: 벡터만큼 옮긴 자리에 같은 조각이 있는 수가 제자리보다 많아야 몸이 움직인 것 */
         int vx = round_div(sumx, bestw), vy = round_div(sumy, bestw);
         if ((vx || vy) && overlap_count(base, to, gstart, g, 0, 0) >= overlap_count(base, to, gstart, g, vx, vy)) goto body_or_stop;
         /* 몸이 움직였는데 겉모습 표가 딴 데(4px 넘게)를 가리키면 우연한 짝 — 몸 위치를 따른다(물체 칸은 겉모습이 우선) */
         if (body == 1 && (bvx || bvy) && (vx - bvx > 4 || bvx - vx > 4 || vy - bvy > 4 || bvy - vy > 4)) goto body_or_stop;
         for (k = gstart; k < g; k++)
            if (ld16(base->spr + k * 4) & 0x1800) { mv[k].dx = (int16_t)vx; mv[k].dy = (int16_t)vy; mv[k].has = 1; }
         continue;
      }
body_or_stop:
      if (!body) { und[gstart] = (uint8_t)g; continue; }        /* 몸·물체 위치를 모른다 → 무리 정지(이펙트면 아래 이펙트 규칙으로) */
      for (k = gstart; k < g; k++)                              /* 2순위: 몸·물체 위치 — 포즈가 통째로 바뀌어도 궤적대로 */
         if (ld16(base->spr + k * 4) & 0x1800) { mv[k].dx = (int16_t)bvx; mv[k].dy = (int16_t)bvy; mv[k].has = 1; }
   }

   /* 못 정한 무리 중 몸 팔레트(0·5)가 아닌 것 = 이펙트·장풍 — ③ RAM 물체 등속 → ② 몸에 붙음 → ① 테두리 등속 순서로.
      정해지면 und 를 지운다(남은 und = 옮기기+섞기에서 반투명으로 겹칠 후보) */
   if (fx_mode)
   {
      const ss2fg_frame *prev = 0;
      int prev_done = 0;
      for (gstart = 0; gstart < 64; gstart++)
      {
         int vx, vy;
         uint8_t pal;
         if (!und[gstart]) continue;
         pal = base->sprcol[gstart];
         if (pal == 0 || pal == 5) continue;
         if (!prev_done) { prev = older_change(base); prev_done = 1; }
         fx_stats[0]++;
         if (fx_objpath(base, to, prev, gstart, und[gstart], &vx, &vy) ||
             fx_attach(base, to, prev, gstart, und[gstart], &vx, &vy) ||
             fx_vec(base, to, prev, gstart, und[gstart], &vx, &vy))
         { set_group(base, mv, gstart, und[gstart], vx, vy); und[gstart] = 0; }
      }
   }
   if (fx_in_render)
   {
      memset(fx_fade_b, 0, sizeof fx_fade_b);
      if (fx_mode == 2)
         for (gstart = 0; gstart < 64; gstart++)
            if (und[gstart] && base->sprcol[gstart] != 0 && base->sprcol[gstart] != 5 && base->sprcol[gstart] != 12)
               memset(fx_fade_b + gstart, 1, und[gstart] - gstart);
   }
   /* 체인 그룹: 슬롯 k 에 체인 비트(0x0600)가 있으면 k-1 과 같은 그룹 */
   for (gstart = 0; gstart < 64; gstart = g)
   {
      int voters[64], nv = 0, k, a, best = -1, bestn = 0, sumx = 0, sumy = 0;
      for (g = gstart + 1; g < 64; g++)
         if (!(ld16(base->spr + g * 4) & 0x0600)) break;
      if (g - gstart < 2) continue;                              /* 단독 슬롯은 위 무리 규칙이 처리했다 */
      for (k = gstart; k < g; k++)
         if (cand[k].has) voters[nv++] = k;
      if (nv == 0) continue;                                    /* 믿을 조각이 없다 → 그룹 정지 */
      for (a = 0; a < nv; a++)
      {
         int n = 0, b;
         for (b = 0; b < nv; b++)
         {
            int ddx = cand[voters[b]].dx - cand[voters[a]].dx, ddy = cand[voters[b]].dy - cand[voters[a]].dy;
            if (ddx >= -1 && ddx <= 1 && ddy >= -1 && ddy <= 1) n++;
         }
         if (n > bestn) { bestn = n; best = a; }
      }
      if (nv >= 2 && bestn * 2 <= nv) continue;                 /* 과반이 안 모인다 → 그룹 정지 */
      for (a = 0; a < nv; a++)
      {
         int ddx = cand[voters[a]].dx - cand[voters[best]].dx, ddy = cand[voters[a]].dy - cand[voters[best]].dy;
         if (ddx >= -1 && ddx <= 1 && ddy >= -1 && ddy <= 1) { sumx += cand[voters[a]].dx; sumy += cand[voters[a]].dy; }
      }
      {
         int vx = round_div(sumx, bestn), vy = round_div(sumy, bestn);
         if ((vx || vy) && overlap_count(base, to, gstart, g, 0, 0) >= overlap_count(base, to, gstart, g, vx, vy)) continue;
         for (k = gstart; k < g; k++)
            if (ld16(base->spr + k * 4) & 0x1800) { mv[k].dx = (int16_t)vx; mv[k].dy = (int16_t)vy; mv[k].has = 1; }
      }
   }
}

/* ───────────── 포즈 섞기 (PocketCore 방 80_ss2fg_pose_blend.patch b4b3b3c) ─────────────
   맞고 빙글빙글 날아가는 동안 사무쇼2 는 몸 포즈 두 장(쭉 뻗음·웅크림)을 실제 2 프레임(1/30초)마다 번갈아 그린다 — 60Hz 원판에도
   그대로라 «깜빡이며 날아가는» 느낌이 난다(유저 2026-10-10 「다 깜빡이는 거마냥 날아가는데 원래 그런가 보네 — 연구 좀 해 봐」).
   포즈는 그림이 통째로 바뀌어 조각을 옮겨 이어 줄 수 없으니, «번갈아 바뀌는 몸»만 중간 그림에서 지금 포즈(1−t)와 다음 포즈(t)를
   반투명으로 겹친다(잔상). 그 몸을 지금 포즈로 그린 줄과 다음 포즈로 그린 줄을 따로 만들어 화소마다 섞으므로, 앞에 있는
   다른 조각·배경과의 앞뒤는 원래 그리기 순서 그대로다. 다음 포즈는 몸 이동(RAM)만큼 되돌린 자리에서 출발해 t 와 함께 제자리로.
   켜지는 때: 몸 포즈가 base→to 에서 바뀌고, 포즈가 3 프레임 이하씩 돌아가는 중이고(to 포즈가 최근에 나왔던 것 = 순환),
   몸이 공중에서 움직이는 중(pose_alternates). 몸 따로 박자가 걸린 몸은 안 섞는다. 코어 옵션 ngp_framegen_pose(blend·off). */
static int pose_mode = 1;
static int pose_stats[4];          /* [0] 몸 포즈가 바뀐 그림 [1] 번갈음 아님 [2] 섞음 [3] 안 움직임 */
void ss2fg_set_pose(int on) { pose_mode = on ? 1 : 0; }
int  ss2fg_get_pose(void) { return pose_mode; }
void ss2fg_pose_stats(int *out4) { int i; for (i = 0; i < 4; i++) out4[i] = pose_stats[i]; }
static int pp_on = 0;
static uint8_t pp_skip[64], pp_add[64];
static int16_t pp_dx[64], pp_dy[64];
static const ss2fg_frame *pp_to = 0;
static int pp_ymin, pp_ymax;

/* 조각의 겉모습 지문 — 플립·우선순위 비트, 팔레트, 타일 그림 16바이트 (same_piece 와 같은 것을 본다) */
static uint64_t piece_look(const ss2fg_frame *f, int k)
{
   uint16_t w = ld16(f->spr + k * 4);
   const uint8_t *t = f->chr + (w & 0x1FF) * 16;
   uint64_t h = 1469598103934665603ULL;
   int i;
   h = (h ^ (uint64_t)(w & 0xD800)) * 1099511628211ULL;
   h = (h ^ (uint64_t)f->sprcol[k]) * 1099511628211ULL;
   for (i = 0; i < 16; i++) h = (h ^ (uint64_t)t[i]) * 1099511628211ULL;
   return h;
}

/* f 의 몸 p 조각 — 몸 팔레트(P1 0 · P2 5) 무리(sprite_moves 와 같은 무리 나누기) 중 테두리가 몸 자리 근처(body_vec 과 같은 창) */
static int body_mask(const ss2fg_frame *f, int p, uint8_t *mask)
{
   const ss2fg_regs *r = &f->line[0];
   int gstart, g, n = 0, bp = p ? 5 : 0;
   memset(mask, 0, 64);
   if (!f->valid || f->mono || !f->body_ok || !(f->ob_act & (1u << p))) return 0;
   for (gstart = 0; gstart < 64; gstart = g)
   {
      uint16_t key = ld16(f->spr + gstart * 4) & 0x1800;
      uint8_t pal = f->sprcol[gstart];
      int bx0 = f->ax[gstart], bx1 = f->ax[gstart] + 8, by0 = f->ay[gstart], by1 = f->ay[gstart] + 8, k;
      int l = 999, rr = -999, t = 999, b = -999;
      for (g = gstart + 1; g < 64; g++)
      {
         uint16_t w = ld16(f->spr + g * 4);
         int sx = f->ax[g], sy = f->ay[g];
         if ((w & 0x1800) != key || f->sprcol[g] != pal) break;
         if (sx + 8 < bx0 - 16 || sx > bx1 + 16 || sy + 8 < by0 - 16 || sy > by1 + 16) break;
         if (sx < bx0) bx0 = sx;
         if (sx + 8 > bx1) bx1 = sx + 8;
         if (sy < by0) by0 = sy;
         if (sy + 8 > by1) by1 = sy + 8;
      }
      if (!key || (pal & 0x0F) != bp) continue;
      for (k = gstart; k < g; k++)
      {
         int sx, sy;
         if (!(ld16(f->spr + k * 4) & 0x1800)) continue;
         sx = (int8_t)(uint8_t)(f->ax[k] + r->spx - f->ob_x[p]);
         sy = (int8_t)(uint8_t)(f->ay[k] + r->spy - f->ob_y[p]);
         if (sx < l) l = sx;
         if (sx + 8 > rr) rr = sx + 8;
         if (sy < t) t = sy;
         if (sy + 8 > b) b = sy + 8;
      }
      if (l > rr || l > 40 || rr < -40 || t > 16 || b < -56) continue;
      for (k = gstart; k < g; k++)
         if (ld16(f->spr + k * 4) & 0x1800) { mask[k] = 1; n++; }
   }
   return n;
}

/* 몸 포즈의 지문 — 조각마다 (겉모습, 몸 테두리 왼쪽 위 기준 자리)를 줄·칸 순으로 늘어놓은 해시. 자리(몸 이동)와 슬롯 번호에 무관 */
static uint64_t body_sig(const ss2fg_frame *f, const uint8_t *mask)
{
   const ss2fg_regs *r = &f->line[0];
   int k, n = 0, i, j, x0 = 9999, y0 = 9999;
   int px[64], py[64];
   uint64_t lk[64], h = 1469598103934665603ULL;
   for (k = 0; k < 64; k++)
   {
      if (!mask[k]) continue;
      px[n] = wrapx(f->ax[k] + r->spx); py[n] = wrapc(f->ay[k] + r->spy); lk[n] = piece_look(f, k);
      if (px[n] < x0) x0 = px[n];
      if (py[n] < y0) y0 = py[n];
      n++;
   }
   if (!n) return 0;
   for (i = 1; i < n; i++)                                       /* 줄(y)·칸(x)·겉모습 순으로 */
      for (j = i; j > 0; j--)
      {
         int sw = py[j] < py[j - 1] || (py[j] == py[j - 1] && (px[j] < px[j - 1] || (px[j] == px[j - 1] && lk[j] < lk[j - 1])));
         int tx, ty; uint64_t tl;
         if (!sw) break;
         tx = px[j]; px[j] = px[j - 1]; px[j - 1] = tx;
         ty = py[j]; py[j] = py[j - 1]; py[j - 1] = ty;
         tl = lk[j]; lk[j] = lk[j - 1]; lk[j - 1] = tl;
      }
   for (i = 0; i < n; i++)
   {
      h = (h ^ (uint64_t)(uint8_t)(px[i] - x0)) * 1099511628211ULL;
      h = (h ^ (uint64_t)(uint8_t)(py[i] - y0)) * 1099511628211ULL;
      h = (h ^ lk[i]) * 1099511628211ULL;
   }
   return h | 1;
}

static int ring_index(const ss2fg_frame *f)
{
   int i;
   for (i = 0; i < FG_RING; i++) if (fg_hist[i] >= 0 && &fg_slot[fg_hist[i]] == f) return i;
   return -1;
}

/* 몸 p 를 섞을지 — 맞고 날아가는 몸은 포즈를 1~3 프레임마다 돌려 가며 그린다(그쪽 하니스 실측 나코루루: 웅크림 14조각 ·
   뻗음 20조각 두 가지가 «뻗음1·웅크림·뻗음2·웅크림…» 으로 2 프레임씩). 그래서 «A·B·A» 하나만 보지 않고:
   base 포즈가 3 프레임 이하로 이어졌고, 그 앞으로도 3 프레임 이하짜리 포즈가 둘 이상 이어졌고(짧게 돌아가는 중),
   다음 포즈(st)가 최근에 이미 나왔고(돌아오는 포즈 = 순환), 몸이 공중(발 높이 < 땅 128 − 4)에서 움직이는 중일 때.
   걷기·대시(한 포즈 4 프레임 넘게, 또는 땅 위)와 제자리 연타(안 움직임)는 안 걸린다.
   몸 조각을 못 찾는 프레임(맞는 순간 번쩍임 — 다른 팔레트)을 만나면 거기서 멈춘다. */
#define FG_GROUND_Y 128
static int pose_alternates(const ss2fg_frame *base, const ss2fg_frame *to, int p, uint64_t sb, uint64_t st)
{
   int i = ring_index(base), j, cur_len = 1, changes = 0, seen_st = 0, moved = 0;
   uint64_t cur = sb;
   uint8_t m[64];
   if (i < 0) return 0;
   if (base->ob_y[p] > FG_GROUND_Y - 4 && to->ob_y[p] > FG_GROUND_Y - 4) return 0;      /* 땅 위 */
   if (to->ob_x[p] != base->ob_x[p] || to->ob_y[p] != base->ob_y[p]) moved = 1;
   for (j = i + 1; j < FG_RING; j++)
   {
      const ss2fg_frame *f = fg_hist[j] >= 0 ? &fg_slot[fg_hist[j]] : 0;
      uint64_t sg;
      if (!f || !body_mask(f, p, m)) break;
      sg = body_sig(f, m);
      if (f->ob_x[p] != base->ob_x[p] || f->ob_y[p] != base->ob_y[p]) moved = 1;
      if (sg == cur)
      {
         if (++cur_len > 3) { if (!changes) return 0; break; }  /* base 포즈가 오래 머묾 → 아님 / 앞쪽 긴 포즈 → 거기까지 */
         continue;
      }
      changes++;                                                 /* cur 포즈가 3 프레임 이하로 끝남 */
      cur = sg; cur_len = 1;
      if (sg == st) seen_st = 1;
   }
   if (changes < 2 || !seen_st) return 0;
   if (!moved) { pose_stats[3]++; return 0; }
   return 1;
}

static void render_line(const ss2fg_frame *base, const ss2fg_frame *to, const int16_t *offx, const int16_t *offy,
                        int t, int y, uint16_t *scan)
{
   const ss2fg_regs *rb = &base->line[y];
   uint8_t zbuf[256];
   uint16_t c;
   int x = 0, spr;
   int in_win = (y >= rb->winy) && (y < rb->winy + rb->winh);

   memset(zbuf, 0, sizeof zbuf);
   rl_cur_slot = -1;
   if (rl_slot) { int i; for (i = 0; i < 256; i++) rl_slot[i] = -1; }

   /* 창 밖 색 */
   c = ld16(base->pal + 0x01F0 + (rb->oowc << 1));
   if (rb->neg) c = (uint16_t)~c;
   if (in_win)
   {
      for (x = 0; x < imin(rb->winx, SS2FG_W); x++) scan[x] = c;
      x = imin(rb->winx + rb->winw, SS2FG_W);
   }
   for (; x < SS2FG_W; x++) scan[x] = c;

   if (!in_win) { if (rl_zout) memcpy(rl_zout, zbuf, sizeof zbuf); return; }

   /* 배경색 */
   c = ld16(base->pal + 0x01E0 + ((rb->bgc & 7) << 1));
   if (rb->neg) c = (uint16_t)~c;
   for (x = rb->winx; x < imin(rb->winx + rb->winw, SS2FG_W); x++) scan[x] = c;

   /* 스크롤 플레인 — 값만 base→to 보간, 내용·순서는 base */
   {
      uint8_t s1x = rb->s1x, s1y = rb->s1y, s2x = rb->s2x, s2y = rb->s2y;
      if (to)
      {
         const ss2fg_regs *rt = &to->line[y];
         s1x = lerp_wrap8(rb->s1x, rt->s1x, t); s1y = lerp_wrap8(rb->s1y, rt->s1y, t);
         s2x = lerp_wrap8(rb->s2x, rt->s2x, t); s2y = lerp_wrap8(rb->s2y, rt->s2y, t);
      }
      if (rb->swap)
      {
         if (base->layers & 1) draw_scroll(base, rb, scan, zbuf, y, 1, s1x, s1y, ZD_BG_SCROLL);
         if (base->layers & 2) draw_scroll(base, rb, scan, zbuf, y, 2, s2x, s2y, ZD_FG_SCROLL);
      }
      else
      {
         if (base->layers & 1) draw_scroll(base, rb, scan, zbuf, y, 2, s2x, s2y, ZD_BG_SCROLL);
         if (base->layers & 2) draw_scroll(base, rb, scan, zbuf, y, 1, s1x, s1y, ZD_FG_SCROLL);
      }
   }

   /* 스프라이트 — base 의 것을 이동량만큼 옮겨 그린다.
      포즈 섞기 둘째 판(pp_on == 2)에서는 섞을 몸의 base 조각을 빼고, 그 자리 순서(슬롯 번호)에 다음 포즈(pp_to) 조각을 넣는다 */
   if (base->layers & 4)
   for (spr = 0; spr < 64; spr++)
   {
      uint16_t d = ld16(base->spr + spr * 4);
      unsigned priority = (d & 0x1800) >> 11;
      int sx, sy;
      if (priority != 0 && !(pp_on == 2 && pp_skip[spr]) && !(fx_fade_on && fx_fade_b[spr])   /* 섞어 그릴 이펙트 — 아래 반투명 단계에서 */
          && !(rl_skip && rl_skip[spr]))                                                       /* 서기 사이 그림: 몸 뺀 줄 */
      {
         rl_cur_slot = spr;
         sx = wrapx(base->ax[spr] + rb->spx);
         sy = wrapc(base->ay[spr] + rb->spy);
         if (offx) { sx += offx[spr]; sy += offy[spr]; }
         if (y >= sy && y <= sy + 7)
         {
            unsigned row = (unsigned)(y - sy) & 7;
            draw_pattern(base, rb, scan, zbuf, sx, d & 0x01FF, (d & 0x4000) ? 7 - row : row,
                         d & 0x8000, base->pal, base->sprcol[spr] & 0xF, (uint8_t)(priority << 1));
         }
      }
      if (pp_on == 2 && pp_add[spr])
      {
         const ss2fg_regs *rt = &pp_to->line[y];
         uint16_t dt = ld16(pp_to->spr + spr * 4);
         unsigned pt = (dt & 0x1800) >> 11;
         sx = wrapx(pp_to->ax[spr] + rt->spx) + pp_dx[spr];
         sy = wrapc(pp_to->ay[spr] + rt->spy) + pp_dy[spr];
         if (pt && y >= sy && y <= sy + 7)
         {
            unsigned row = (unsigned)(y - sy) & 7;
            draw_pattern(pp_to, rb, scan, zbuf, sx, dt & 0x01FF, (dt & 0x4000) ? 7 - row : row,
                         dt & 0x8000, pp_to->pal, pp_to->sprcol[spr] & 0xF, (uint8_t)(pt << 1));
         }
      }
   }
   if (fx_fade_on && (base->layers & 4))
   {
      /* 반투명 단계: 사라질 base 이펙트(1-t) 위에 다음 그림의 이펙트(t) */
      for (spr = 0; spr < 64; spr++)
      {
         uint16_t d = ld16(base->spr + spr * 4);
         unsigned priority = (d & 0x1800) >> 11;
         int sx, sy;
         if (!priority || !fx_fade_b[spr]) continue;
         sx = wrapx(base->ax[spr] + rb->spx); sy = wrapc(base->ay[spr] + rb->spy);
         if (y >= sy && y <= sy + 7)
         {
            unsigned row = (unsigned)(y - sy) & 7;
            draw_pattern_blend(base, rb, scan, zbuf, sx, d & 0x01FF, (d & 0x4000) ? 7 - row : row,
                               d & 0x8000, base->pal, base->sprcol[spr] & 0xF, (uint8_t)(priority << 1), 256 - fx_fade_t);
         }
      }
      if (fx_fade_to)
      {
         const ss2fg_regs *rt = &fx_fade_to->line[y];
         for (spr = 0; spr < 64; spr++)
         {
            uint16_t d = ld16(fx_fade_to->spr + spr * 4);
            unsigned priority = (d & 0x1800) >> 11;
            int sx, sy;
            if (!priority || !fx_fade_in[spr]) continue;
            sx = wrapx(fx_fade_to->ax[spr] + rt->spx); sy = wrapc(fx_fade_to->ay[spr] + rt->spy);
            if (y >= sy && y <= sy + 7)
            {
               unsigned row = (unsigned)(y - sy) & 7;
               draw_pattern_blend(fx_fade_to, rb, scan, zbuf, sx, d & 0x01FF, (d & 0x4000) ? 7 - row : row,
                                  d & 0x8000, fx_fade_to->pal, fx_fade_to->sprcol[spr] & 0xF, (uint8_t)(priority << 1), fx_fade_t);
            }
         }
      }
   }
   if (rl_zout) memcpy(rl_zout, zbuf, sizeof zbuf);
}

/* ══ 서기 사이 그림 (코어 패치 100) ═══════════════════════════════════════════════════════════
   유저 2026-10-11 「뉴트럴 포즈를 기존 방식대로 일단 해 줘 봐, 그리는 거랑도」 → 비교 영상 → 「ㄱ」.
   서 있는 몸은 포즈 6~8장을 6~18 프레임씩 돌린다(하오마루 8장×8f, 갈포드 8장×14f …). 장마다 윗몸이 1~2픽셀
   옮겨진 그림이라 원래 해상도(160칸)에선 그 «사이»가 없다 — 4배 격자(640칸)에 앞 장·뒤 장의 화소를 움직인 만큼의
   t 씩 ¼픽셀로 옮겨 찍어 사이 그림을 만든다. 섞지 않는다(도트 그대로), 반이 넘으면 뒤 장이 위로.
   ① 관찰(실제 프레임마다): 몸 포즈 그림을 지문별로 기억하고, 제자리에서 A→B 로 바뀐 것과 A 의 길이를 배운다.
   ② 계획(출력마다): 지금 포즈 A 다음이 B 로 굳었고(같은 바뀜 2번 이상, 다른 것보다 두 배 이상) B 그림을 기억하면
      t = A 시작 뒤 지난 시간 / 길이. 미리 돌린 D+1·D+2 에서 B 로 바뀌면 그때 t=1 이 되게 맞추고, 다른 포즈로 바뀌거나
      몸이 움직이면(기술·걷기) 안 한다.
   ③ 그리기: 몸 조각만 뺀 줄을 다시 그려(깊이·슬롯 그대로) 4배로 늘리고 그 위에 사이 그림을 원래 앞뒤 규칙대로 얹은
      «조각»을 만든다. 내보낼 때 화면 전체를 4배로 늘리고 조각을 붙인다(ngp_framegen_idle=draw 일 때만 4배 출력). */

#define ID_W      80            /* 몸 그림 틀 최대 폭·높이(원래 화소) */
#define ID_H      96
#define ID_POSES  24            /* 몸마다 기억할 포즈 그림 */
#define ID_PAIRS  12            /* 몸마다 기억할 (A,B) 이동표 */
#define ID_LEARN  48            /* 몸마다 배운 바뀜 */
#define ID_R      3             /* 이동 찾기 범위 ±px */
#define ID_S      4             /* 4배 */
#define ID_NONE   0xFFFF        /* 빈 화소(12비트 색은 0x0FFF 까지) */

typedef struct {
   uint64_t sig; unsigned used;
   int ox, oy, w, h;            /* 몸 자리(물체 X·Y = 발 가운데) 기준 그림 왼쪽 위, 크기 */
   uint8_t depth, first;        /* 몸 조각의 우선순위<<1, 가장 앞(가장 작은) 슬롯 */
   uint16_t c[ID_W * ID_H];     /* 12비트 색, ID_NONE = 빈 화소 */
} id_pose;

typedef struct {
   uint64_t sa, sb; unsigned used;
   int ox, oy, w, h;            /* 둘을 합친 틀(몸 자리 기준) */
   uint8_t depth, first;
   uint16_t ca[ID_W * ID_H], cb[ID_W * ID_H];
   int8_t dax[ID_W * ID_H], day[ID_W * ID_H];   /* A 화소가 B 쪽으로 갈 때의 이동(A 격자) */
   int8_t dbx[ID_W * ID_H], dby[ID_W * ID_H];   /* B 화소가 A 의 어디서 왔나: B(p) = A(p - d) (B 격자) */
} id_pair;

typedef struct { uint64_t sa, sb; uint16_t hold; uint8_t cnt; } id_learn_t;
typedef struct { int on; id_pair *pr; int t256, ax, ay; } id_req_t;
typedef struct { int on, x0, y0, w, h; uint16_t px[(ID_W * ID_S) * (ID_H * ID_S)]; } id_patch_t;

static int        id_on = 0;
static id_pose    id_pose_c[2][ID_POSES];
static id_pair    id_pair_c[2][ID_PAIRS];
static id_learn_t id_lt[2][ID_LEARN];
static unsigned   id_clock = 0;
static uint64_t   id_cur[2];
static unsigned   id_start[2];
static int        id_ax[2], id_ay[2], id_still[2];
static id_req_t   id_rq[2][2];               /* [출력 칸: 0 = τD, 1 = τD+½][몸] */
static id_patch_t id_pt[2][2];
static int        id_slot = -1;              /* 지금 그리는 출력 칸(ss2fg_render2 가 본다) */
static uint16_t   id_tc[(ID_W * ID_S) * (ID_H * ID_S)];
static uint16_t   id_la[(ID_W * ID_S) * (ID_H * ID_S)], id_lb[(ID_W * ID_S) * (ID_H * ID_S)];
static int        id_stats[6];               /* 계기: [0] 배운 바뀜 [1] 계획 [2] 패치 [3] 미리 본 다른 포즈 [4] 이동표 [5] 그림 */

void ss2fg_set_idle(int on)
{
   if (!on && id_on)
   {
      memset(id_cur, 0, sizeof id_cur);
      memset(id_rq, 0, sizeof id_rq);
      id_pt[0][0].on = id_pt[0][1].on = id_pt[1][0].on = id_pt[1][1].on = 0;
   }
   id_on = on ? 1 : 0;
}
int ss2fg_idle_on(void) { return id_on; }
void ss2fg_idle_slot(int s) { id_slot = s; }
void ss2fg_idle_stats(int *out6) { int i; for (i = 0; i < 6; i++) out6[i] = id_stats[i]; }

static id_pose *id_find_pose(int p, uint64_t sg)
{
   int i;
   for (i = 0; i < ID_POSES; i++) if (id_pose_c[p][i].sig == sg) { id_pose_c[p][i].used = ++id_clock; return &id_pose_c[p][i]; }
   return 0;
}

/* 몸 p 의 조각(마스크 m)을 몸 자리 기준 그림으로 — 슬롯 번호가 작은 조각이 앞(render_line 의 zbuf 규칙과 같음) */
static void id_capture(const ss2fg_frame *f, int p, const uint8_t *m, uint64_t sg)
{
   const ss2fg_regs *r = &f->line[0];
   int k, x0 = 9999, y0 = 9999, x1 = -9999, y1 = -9999, depth = -1, first = -1, i, oldest = 0;
   id_pose *ps;
   for (k = 0; k < 64; k++)
   {
      uint16_t w;
      int sx, sy, pr;
      if (!m[k]) continue;
      w = ld16(f->spr + k * 4);
      pr = (w & 0x1800) >> 11;
      if (depth < 0) { depth = pr << 1; first = k; }
      else if ((pr << 1) != depth) return;                    /* 우선순위가 섞인 몸 — 안 다룬다 */
      sx = wrapx(f->ax[k] + r->spx) - f->ob_x[p];
      sy = wrapc(f->ay[k] + r->spy) - f->ob_y[p];
      if (sx < x0) x0 = sx;
      if (sy < y0) y0 = sy;
      if (sx + 8 > x1) x1 = sx + 8;
      if (sy + 8 > y1) y1 = sy + 8;
   }
   if (depth <= 0 || x1 - x0 > ID_W || y1 - y0 > ID_H) return;
   for (i = 1; i < ID_POSES; i++) if (id_pose_c[p][i].used < id_pose_c[p][oldest].used) oldest = i;
   ps = &id_pose_c[p][oldest];
   ps->sig = sg; ps->used = ++id_clock;
   ps->ox = x0; ps->oy = y0; ps->w = x1 - x0; ps->h = y1 - y0;
   ps->depth = (uint8_t)depth; ps->first = (uint8_t)first;
   for (i = 0; i < ps->w * ps->h; i++) ps->c[i] = ID_NONE;
   for (k = 0; k < 64; k++)
   {
      uint16_t w;
      int sx, sy, row, col;
      unsigned tile;
      if (!m[k]) continue;
      w = ld16(f->spr + k * 4);
      tile = w & 0x01FF;
      sx = wrapx(f->ax[k] + r->spx) - f->ob_x[p] - x0;
      sy = wrapc(f->ay[k] + r->spy) - f->ob_y[p] - y0;
      for (row = 0; row < 8; row++)
      {
         unsigned rr = (w & 0x4000) ? 7 - row : row;
         int v = ld16(f->chr + tile * 16 + rr * 2);
         if (w & 0x8000) v = mirrored[(v & 0xff00) >> 8] | (mirrored[v & 0xff] << 8);
         for (col = 0; col < 8; col++)
         {
            int idx = (v >> (2 * (7 - col))) & 3, o;
            if (!idx) continue;
            o = (sy + row) * ps->w + sx + col;
            if (ps->c[o] != ID_NONE) continue;                 /* 앞 슬롯이 이미 칠함 */
            ps->c[o] = ld16(f->pal + ((f->sprcol[k] & 0xF) << 3) + (idx << 1)) & 0x0FFF;
         }
      }
   }
   id_stats[5]++;
}

static void id_learn_add(int p, uint64_t sa, uint64_t sb, unsigned hold)
{
   int i, lo = 0;
   for (i = 0; i < ID_LEARN; i++)
      if (id_lt[p][i].sa == sa && id_lt[p][i].sb == sb)
      {
         if (id_lt[p][i].cnt < 250) id_lt[p][i].cnt++;
         id_lt[p][i].hold = (uint16_t)hold;
         id_stats[0]++;
         return;
      }
   for (i = 1; i < ID_LEARN; i++) if (id_lt[p][i].cnt < id_lt[p][lo].cnt) lo = i;
   id_lt[p][lo].sa = sa; id_lt[p][lo].sb = sb; id_lt[p][lo].hold = (uint16_t)hold; id_lt[p][lo].cnt = 1;
   id_stats[0]++;
}
/* A 다음 포즈 — 같은 바뀜을 2번 이상 봤고 두 번째로 많은 것보다 두 배 이상일 때만 */
static uint64_t id_next(int p, uint64_t sa, unsigned *hold)
{
   int i, b1 = -1, c2 = 0;
   for (i = 0; i < ID_LEARN; i++)
   {
      if (id_lt[p][i].sa != sa || !id_lt[p][i].cnt) continue;
      if (b1 < 0 || id_lt[p][i].cnt > id_lt[p][b1].cnt) { if (b1 >= 0 && id_lt[p][b1].cnt > c2) c2 = id_lt[p][b1].cnt; b1 = i; }
      else if (id_lt[p][i].cnt > c2) c2 = id_lt[p][i].cnt;
   }
   if (b1 < 0 || id_lt[p][b1].cnt < 2 || id_lt[p][b1].cnt < 2 * c2) return 0;
   *hold = id_lt[p][b1].hold;
   return id_lt[p][b1].sb;
}

/* B 의 칠한 화소마다 A 에서 온 이동 d — 5×5 창에서 화소(빈칸 포함)가 다른 수 × 16 + 덜 움직인 쪽을 조금 선호 */
static void id_match(const uint16_t *ca, const uint16_t *cb, int w, int h, int8_t *dx, int8_t *dy)
{
   int x, y;
   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
      {
         int best = 1 << 30, bx = 0, by = 0, ddx, ddy, i = y * w + x;
         dx[i] = dy[i] = 0;
         if (cb[i] == ID_NONE) continue;
         for (ddy = -ID_R; ddy <= ID_R; ddy++)
            for (ddx = -ID_R; ddx <= ID_R; ddx++)
            {
               int cost = (ddx < 0 ? -ddx : ddx) + (ddy < 0 ? -ddy : ddy), wx, wy;
               for (wy = -2; wy <= 2 && cost < best; wy++)
                  for (wx = -2; wx <= 2; wx++)
                  {
                     int qx = x + wx, qy = y + wy, ax2 = qx - ddx, ay2 = qy - ddy;
                     uint16_t vb = (qx >= 0 && qy >= 0 && qx < w && qy < h) ? cb[qy * w + qx] : ID_NONE;
                     uint16_t va = (ax2 >= 0 && ay2 >= 0 && ax2 < w && ay2 < h) ? ca[ay2 * w + ax2] : ID_NONE;
                     if (va != vb) cost += 16;
                  }
               if (cost < best) { best = cost; bx = ddx; by = ddy; }
            }
         dx[i] = (int8_t)bx; dy[i] = (int8_t)by;
      }
}
static int id_med(int *v, int n)
{
   int i, j, t;
   for (i = 1; i < n; i++) for (j = i; j > 0 && v[j] < v[j - 1]; j--) { t = v[j]; v[j] = v[j - 1]; v[j - 1] = t; }
   return v[n / 2];
}
/* 칠한 화소끼리 3×3 중앙값으로 두 번 다듬는다(외톨이 엉터리 이동 지우기) */
static void id_smooth(const uint16_t *c, int w, int h, int8_t *dx, int8_t *dy)
{
   static int8_t tx[ID_W * ID_H], ty[ID_W * ID_H];
   int it, x, y;
   for (it = 0; it < 2; it++)
   {
      for (y = 0; y < h; y++)
         for (x = 0; x < w; x++)
         {
            int vx[9], vy[9], n = 0, i = y * w + x, a, b;
            tx[i] = dx[i]; ty[i] = dy[i];
            if (c[i] == ID_NONE) continue;
            for (b = -1; b <= 1; b++)
               for (a = -1; a <= 1; a++)
               {
                  int qx = x + a, qy = y + b, j;
                  if (qx < 0 || qy < 0 || qx >= w || qy >= h) continue;
                  j = qy * w + qx;
                  if (c[j] == ID_NONE) continue;
                  vx[n] = dx[j]; vy[n] = dy[j]; n++;
               }
            tx[i] = (int8_t)id_med(vx, n); ty[i] = (int8_t)id_med(vy, n);
         }
      memcpy(dx, tx, (size_t)w * h); memcpy(dy, ty, (size_t)w * h);
   }
}

static id_pair *id_get_pair(int p, uint64_t sa, uint64_t sb)
{
   int i, oldest = 0, x, y;
   id_pose *A, *B;
   id_pair *pr;
   for (i = 0; i < ID_PAIRS; i++)
      if (id_pair_c[p][i].sa == sa && id_pair_c[p][i].sb == sb) { id_pair_c[p][i].used = ++id_clock; return &id_pair_c[p][i]; }
   A = id_find_pose(p, sa); B = id_find_pose(p, sb);
   if (!A || !B || A->depth != B->depth) return 0;
   for (i = 1; i < ID_PAIRS; i++) if (id_pair_c[p][i].used < id_pair_c[p][oldest].used) oldest = i;
   pr = &id_pair_c[p][oldest];
   {
      int x0 = A->ox < B->ox ? A->ox : B->ox, y0 = A->oy < B->oy ? A->oy : B->oy;
      int x1 = A->ox + A->w > B->ox + B->w ? A->ox + A->w : B->ox + B->w;
      int y1 = A->oy + A->h > B->oy + B->h ? A->oy + A->h : B->oy + B->h;
      if (x1 - x0 > ID_W || y1 - y0 > ID_H) return 0;
      pr->ox = x0; pr->oy = y0; pr->w = x1 - x0; pr->h = y1 - y0;
   }
   pr->sa = sa; pr->sb = sb; pr->used = ++id_clock;
   pr->depth = A->depth; pr->first = A->first < B->first ? A->first : B->first;
   for (i = 0; i < pr->w * pr->h; i++) pr->ca[i] = pr->cb[i] = ID_NONE;
   for (y = 0; y < A->h; y++) for (x = 0; x < A->w; x++)
      pr->ca[(y + A->oy - pr->oy) * pr->w + x + A->ox - pr->ox] = A->c[y * A->w + x];
   for (y = 0; y < B->h; y++) for (x = 0; x < B->w; x++)
      pr->cb[(y + B->oy - pr->oy) * pr->w + x + B->ox - pr->ox] = B->c[y * B->w + x];
   id_match(pr->ca, pr->cb, pr->w, pr->h, pr->dbx, pr->dby);          /* B(p) = A(p - d) */
   id_smooth(pr->cb, pr->w, pr->h, pr->dbx, pr->dby);
   id_match(pr->cb, pr->ca, pr->w, pr->h, pr->dax, pr->day);          /* A(q) = B(q - e) → A 에서 B 로는 -e */
   id_smooth(pr->ca, pr->w, pr->h, pr->dax, pr->day);
   for (i = 0; i < pr->w * pr->h; i++) { pr->dax[i] = (int8_t)-pr->dax[i]; pr->day[i] = (int8_t)-pr->day[i]; }
   id_stats[4]++;
   return pr;
}

static int id_rnd(int num)                     /* num / 256 반올림(음수 대칭) */
{
   return num >= 0 ? (num + 128) >> 8 : -((-num + 128) >> 8);
}
/* 사이 그림(4배 격자) — A 화소는 t·(A→B), B 화소는 (1−t)·(B→A) 만큼 옮겨 찍고, 반 넘으면 B 를 위로.
   위 그림의 «틈»(좌우나 위아래 양쪽 한 도트 안에 위 그림이 있는 빈칸)만 아래 그림으로 메운다 — 새 자리로 간 큰 부분은
   안 메워 두 번 보이지 않게 */
static void id_draw(const id_pair *pr, int t256, uint16_t *out)
{
   int W4 = pr->w * ID_S, H4 = pr->h * ID_S, x, y, n = W4 * H4, i;
   uint16_t *top, *und;
   for (i = 0; i < n; i++) id_la[i] = id_lb[i] = ID_NONE;
   for (y = 0; y < pr->h; y++)
      for (x = 0; x < pr->w; x++)
      {
         int q = y * pr->w + x, X, Y, a, b;
         if (pr->ca[q] != ID_NONE)
         {
            X = x * ID_S + id_rnd(ID_S * pr->dax[q] * t256); Y = y * ID_S + id_rnd(ID_S * pr->day[q] * t256);
            for (b = 0; b < ID_S; b++) for (a = 0; a < ID_S; a++)
               if (X + a >= 0 && Y + b >= 0 && X + a < W4 && Y + b < H4) id_la[(Y + b) * W4 + X + a] = pr->ca[q];
         }
         if (pr->cb[q] != ID_NONE)
         {
            X = x * ID_S - id_rnd(ID_S * pr->dbx[q] * (256 - t256)); Y = y * ID_S - id_rnd(ID_S * pr->dby[q] * (256 - t256));
            for (b = 0; b < ID_S; b++) for (a = 0; a < ID_S; a++)
               if (X + a >= 0 && Y + b >= 0 && X + a < W4 && Y + b < H4) id_lb[(Y + b) * W4 + X + a] = pr->cb[q];
         }
      }
   top = t256 < 128 ? id_la : id_lb; und = t256 < 128 ? id_lb : id_la;
   for (y = 0; y < H4; y++)
      for (x = 0; x < W4; x++)
      {
         int o = y * W4 + x, k, l = 0, r = 0, u = 0, d = 0;
         out[o] = top[o];
         if (top[o] != ID_NONE || und[o] == ID_NONE) continue;
         for (k = 1; k <= ID_S; k++)
         {
            if (x - k >= 0 && top[o - k] != ID_NONE) l = 1;
            if (x + k < W4 && top[o + k] != ID_NONE) r = 1;
            if (y - k >= 0 && top[o - k * W4] != ID_NONE) u = 1;
            if (y + k < H4 && top[o + k * W4] != ID_NONE) d = 1;
         }
         if ((l && r) || (u && d)) out[o] = und[o];
      }
}

/* 실제 프레임마다(보여 줄 D) — 포즈 그림 기억 + 제자리 바뀜 배우기 */
void ss2fg_idle_observe(const ss2fg_frame *f, unsigned realn)
{
   int p;
   if (!id_on || !f || !f->valid || f->mono) return;
   for (p = 0; p < 2; p++)
   {
      uint8_t m[64];
      uint64_t sg;
      int ax, ay;
      if (!body_mask(f, p, m)) { id_cur[p] = 0; continue; }
      sg = body_sig(f, m);
      ax = f->ob_x[p]; ay = f->ob_y[p];
      if (!id_find_pose(p, sg)) id_capture(f, p, m, sg);
      if (sg == id_cur[p]) { if (ax != id_ax[p] || ay != id_ay[p]) id_still[p] = 0; continue; }
      if (id_cur[p] && id_still[p] && ax == id_ax[p] && ay == id_ay[p])
      {
         unsigned hold = realn - id_start[p];
         if (hold >= 3 && hold <= 60) id_learn_add(p, id_cur[p], sg, hold);
      }
      id_cur[p] = sg; id_start[p] = realn; id_ax[p] = ax; id_ay[p] = ay; id_still[p] = 1;
   }
}

/* 출력 칸 slot(0 = τD, 1 = τD+½, h = 반 프레임) 의 사이 그림 계획. f1·f2 = 미리 돌린 D+1·D+2. 반환 1 = 그릴 몸이 있음 */
int ss2fg_idle_plan(const ss2fg_frame *f0, const ss2fg_frame *f1, const ss2fg_frame *f2, unsigned realn, int h, int slot)
{
   int p, any = 0;
   if (slot < 0 || slot > 1) return 0;
   for (p = 0; p < 2; p++) { id_rq[slot][p].on = 0; id_pt[slot][p].on = 0; }
   if (!id_on || !f0 || !f0->body_ok) return 0;
   for (p = 0; p < 2; p++)
   {
      unsigned hold = 0;
      uint64_t sb;
      int k, K = 0, bad = 0, e2, tot2, t256;
      id_pair *pr;
      if (!id_cur[p] || !id_still[p] || f0->ob_x[p] != id_ax[p] || f0->ob_y[p] != id_ay[p]) continue;
      sb = id_next(p, id_cur[p], &hold);
      if (!sb || !id_find_pose(p, sb)) continue;
      for (k = 1; k <= 2 && !K && !bad; k++)
      {
         const ss2fg_frame *fk = k == 1 ? f1 : f2;
         uint8_t mm[64];
         uint64_t s;
         if (!fk || !fk->body_ok || !body_mask(fk, p, mm)) { bad = 1; break; }
         if (fk->ob_x[p] != id_ax[p] || fk->ob_y[p] != id_ay[p]) { bad = 1; break; }   /* 움직이기 시작 */
         s = body_sig(fk, mm);
         if (s == id_cur[p]) continue;
         if (s == sb) K = k; else bad = 1;
      }
      if (bad) { id_stats[3]++; continue; }
      e2 = 2 * (int)(realn - id_start[p]) + h;
      if (K) tot2 = 2 * (int)(realn + K - id_start[p]);
      else { tot2 = 2 * (int)hold; if (tot2 < e2 + 6) tot2 = e2 + 6; }      /* D+2 까진 안 바뀜 — 적어도 3 프레임 남음 */
      if (tot2 <= 0) continue;
      t256 = 256 * e2 / tot2;
      if (t256 <= 0) continue;
      if (t256 > 255) t256 = 255;
      pr = id_get_pair(p, id_cur[p], sb);
      if (!pr) continue;
      id_rq[slot][p].on = 1; id_rq[slot][p].pr = pr; id_rq[slot][p].t256 = t256;
      id_rq[slot][p].ax = id_ax[p]; id_rq[slot][p].ay = id_ay[p];
      id_stats[1]++;
      any = 1;
   }
   return any;
}

/* ss2fg_render2 끝에서 — 이 출력 칸의 몸마다 «몸 뺀 줄 + 사이 그림» 4배 조각 */
static void id_build(const ss2fg_frame *base, const ss2fg_frame *to_scr, int t_scr,
                     const int16_t *offx, const int16_t *offy, const uint32_t *colormap)
{
   int s = id_slot, p;
   if (s < 0 || s > 1 || !id_on) return;
   for (p = 0; p < 2; p++)
   {
      id_req_t *rq = &id_rq[s][p];
      id_patch_t *pt = &id_pt[s][p];
      uint8_t m[64], zrow[256];
      int16_t srow[256];
      uint16_t scan[256];
      int k, ux0, uy0, x0, y0, x1, y1, y, x, sx, sy, W4;
      const id_pair *pr = rq->pr;
      pt->on = 0;
      if (!rq->on || pp_on || fx_fade_on) continue;           /* 포즈·이펙트 섞기 중엔 몸 뺀 줄을 같게 못 만든다 */
      if (!body_mask(base, p, m)) continue;
      for (k = 0; k < 64; k++) if (m[k] && offx && (offx[k] || offy[k])) break;
      if (k < 64) continue;                                     /* 서 있는 몸이 옮겨 그려지는 중 — 안 함 */
      id_draw(pr, rq->t256, id_tc);
      W4 = pr->w * ID_S;
      ux0 = rq->ax + pr->ox; uy0 = rq->ay + pr->oy;
      x0 = ux0 < 0 ? 0 : ux0; y0 = uy0 < 0 ? 0 : uy0;
      x1 = ux0 + pr->w > SS2FG_W ? SS2FG_W : ux0 + pr->w;
      y1 = uy0 + pr->h > SS2FG_H ? SS2FG_H : uy0 + pr->h;
      if (x1 <= x0 || y1 <= y0) continue;
      rl_skip = m; rl_zout = zrow; rl_slot = srow;
      for (y = y0; y < y1; y++)
      {
         const ss2fg_regs *rb = &base->line[y];
         int in_win = (y >= rb->winy) && (y < rb->winy + rb->winh);
         int wx0 = rb->winx, wx1 = imin(rb->winx + rb->winw, SS2FG_W);
         render_line(base, to_scr, offx, offy, t_scr, y, scan);
         for (sy = 0; sy < ID_S; sy++)
         {
            uint16_t *dst = pt->px + ((y - y0) * ID_S + sy) * ((x1 - x0) * ID_S);
            const uint16_t *tw = id_tc + ((y - uy0) * ID_S + sy) * W4;
            for (x = x0; x < x1; x++)
               for (sx = 0; sx < ID_S; sx++)
               {
                  uint16_t c = scan[x], v = tw[(x - ux0) * ID_S + sx];
                  if (v != ID_NONE && in_win && x >= wx0 && x < wx1
                      && (pr->depth > zrow[x] || (pr->depth == zrow[x] && srow[x] > (int)pr->first)))
                     c = rb->neg ? (uint16_t)~v : v;
                  *dst++ = (uint16_t)colormap[c & 4095];
               }
         }
      }
      rl_skip = 0; rl_zout = 0; rl_slot = 0;
      pt->x0 = x0; pt->y0 = y0; pt->w = x1 - x0; pt->h = y1 - y0; pt->on = 1;
      id_stats[2]++;
   }
}

/* 내보내기 — 원래 그림(16bpp)을 4배로 늘리고 출력 칸 slot 의 조각을 붙인다 */
void ss2fg_idle_compose(uint16_t *dst, int dpitch, const uint16_t *src, int spitch, int w, int h, int slot)
{
   int y, x, sy, p;
   for (y = 0; y < h; y++)
   {
      uint16_t *row = dst + (size_t)(y * ID_S) * dpitch;
      const uint16_t *s = src + (size_t)y * spitch;
      for (x = 0; x < w; x++)
      {
         uint16_t v = s[x];
         row[x * 4] = row[x * 4 + 1] = row[x * 4 + 2] = row[x * 4 + 3] = v;
      }
      for (sy = 1; sy < ID_S; sy++) memcpy(row + (size_t)sy * dpitch, row, (size_t)w * ID_S * sizeof(uint16_t));
   }
   if (slot < 0 || slot > 1) return;
   for (p = 0; p < 2; p++)
   {
      const id_patch_t *pt = &id_pt[slot][p];
      int pw = pt->w * ID_S, r;
      if (!pt->on || pt->x0 + pt->w > w || pt->y0 + pt->h > h) continue;
      for (r = 0; r < pt->h * ID_S; r++)
         memcpy(dst + (size_t)(pt->y0 * ID_S + r) * dpitch + pt->x0 * ID_S, pt->px + (size_t)r * pw, (size_t)pw * sizeof(uint16_t));
   }
}

/* 4배(게임 박자 맞춤)용 — 스프라이트와 스크롤의 목표 프레임·진행도를 따로 준다.
   사무쇼2 는 캐릭터를 짝수 프레임에만, 배경 스크롤을 홀수 프레임에만 움직인다(실측) — 둘의 «다음 바뀌는 때»가
   서로 다르다. to_spr/to_scr 가 0 이거나 t 가 0 이면 그쪽은 base 그대로. */
int ss2fg_render2(const ss2fg_frame *base, const ss2fg_frame *to_spr, int t_spr,
                  const ss2fg_frame *to_scr, int t_scr,
                  void *dst, int pitch_px, int bpp, const uint32_t *colormap)
{
   int y, x;
   uint16_t scan[256];
   fg_move mv[64];
   int16_t offx[64], offy[64];
   int have_off = 0;
   if (!base || !base->valid || base->mono) { fg_ov_on[0] = fg_ov_on[1] = 0; return 0; }
   if (bpp != 2 && bpp != 4) return 0;
   /* 표시 도중 스프라이트표/타일맵이 바뀐 프레임은 끝 시점 사본이 위쪽 줄과 안 맞는다 — 합성 포기 */
   if (base->dirty & (SS2FG_DIRTY_SPR | SS2FG_DIRTY_SCROLL)) return 0;
   if (to_spr && (!to_spr->valid || to_spr->mono || (to_spr->dirty & (SS2FG_DIRTY_SPR | SS2FG_DIRTY_SCROLL)))) to_spr = 0;
   if (to_scr && (!to_scr->valid || to_scr->mono || (to_scr->dirty & (SS2FG_DIRTY_SPR | SS2FG_DIRTY_SCROLL)))) to_scr = 0;
   if (t_spr < 0) t_spr = 0;
   if (t_spr > 256) t_spr = 256;
   if (t_scr < 0) t_scr = 0;
   if (t_scr > 256) t_scr = 256;
   if (to_spr && t_spr == 0) to_spr = 0;
   if (to_scr && t_scr == 0) to_scr = 0;
   fx_fade_on = 0; fx_fade_to = 0;
   if (to_spr) { fx_in_render = 1; sprite_moves(base, to_spr, mv); fx_in_render = 0; }
   else if ((fg_ov_on[0] || fg_ov_on[1]) && base->body_ok) sprite_moves(base, base, mv);   /* 몸 따로 박자만 — 조각 → 몸 표가 필요 */
   else memset(fg_body_of, 0xFF, sizeof fg_body_of);
   {  /* 조각마다 이번 그림의 이동량(px): 몸 따로 박자가 켜진 몸의 조각은 그 값, 아니면 무리 판정 이동량의 진행도만큼 */
      int k2;
      for (k2 = 0; k2 < 64; k2++)
      {
         int p2 = fg_body_of[k2];
         offx[k2] = offy[k2] = 0;
         if (p2 >= 0 && fg_ov_on[p2])
         {
            int vx = fg_ov_x[p2], vy = fg_ov_y[p2];
            offx[k2] = (int16_t)(vx >= 0 ? (vx + 128) >> 8 : -((-vx + 127) >> 8));
            offy[k2] = (int16_t)(vy >= 0 ? (vy + 128) >> 8 : -((-vy + 127) >> 8));
            have_off = 1;
         }
         else if (to_spr && mv[k2].has)
         {
            offx[k2] = (int16_t)lerp_i(0, mv[k2].dx, t_spr);
            offy[k2] = (int16_t)lerp_i(0, mv[k2].dy, t_spr);
            have_off = 1;
         }
      }
   }
   if (to_spr && fx_mode == 2)
   {
      /* 나타날 to 이펙트 = 몸·그림자 팔레트가 아니고, base 의 «정해진(옮겨 그려지는)» 이펙트 조각이 도착할 자리(8px)에
         있지 않은 조각. 정해진 무리는 base 그림을 옮겨 그리므로 to 쪽을 또 그리면 겹친다 */
      const ss2fg_regs *rb0 = &base->line[0], *rt0 = &to_spr->line[0];
      int i, j, any = 0;
      memset(fx_fade_in, 0, sizeof fx_fade_in);
      for (j = 0; j < 64; j++)
      {
         uint16_t wt = ld16(to_spr->spr + j * 4);
         int tx, ty, cov = 0;
         uint8_t pal = to_spr->sprcol[j];
         if (!(wt & 0x1800) || pal == 0 || pal == 5 || pal == 12) continue;
         tx = wrapx(to_spr->ax[j] + rt0->spx); ty = wrapc(to_spr->ay[j] + rt0->spy);
         for (i = 0; i < 64 && !cov; i++)
         {
            uint16_t wb = ld16(base->spr + i * 4);
            int bx, by;
            if (!(wb & 0x1800) || base->sprcol[i] != pal || fx_fade_b[i]) continue;
            bx = wrapx(base->ax[i] + rb0->spx) + (mv[i].has ? mv[i].dx : 0);
            by = wrapc(base->ay[i] + rb0->spy) + (mv[i].has ? mv[i].dy : 0);
            if (abs(bx - tx) <= 8 && abs(by - ty) <= 8) cov = 1;
         }
         if (!cov) { fx_fade_in[j] = 1; any = 1; }
      }
      for (i = 0; i < 64; i++) if (fx_fade_b[i]) any = 1;
      if (any) { fx_fade_on = 1; fx_fade_to = to_spr; fx_fade_t = t_spr; }
   }

   /* 포즈 섞기 — 번갈아 바뀌는 몸(들)을 고른다 */
   pp_on = 0;
   if (pose_mode && to_spr && t_spr > 0 && t_spr < 256 && base->body_ok && to_spr->body_ok)
   {
      int p, ymin = 999, ymax = -999;
      uint8_t mb[64], mt[64];
      memset(pp_skip, 0, sizeof pp_skip); memset(pp_add, 0, sizeof pp_add);
      for (p = 0; p < 2; p++)
      {
         uint64_t sb, st;
         int k, vx, vy, ox, oy;
         if (fg_ov_on[p]) continue;                              /* 몸 따로 박자 중 — 박자가 달라 안 섞는다 */
         if (!body_mask(base, p, mb) || !body_mask(to_spr, p, mt)) continue;
         sb = body_sig(base, mb); st = body_sig(to_spr, mt);
         if (sb == st) continue;
         pose_stats[0]++;
         if (!pose_alternates(base, to_spr, p, sb, st)) { pose_stats[1]++; continue; }
         pose_stats[2]++;
         vx = (int8_t)(uint8_t)(to_spr->ob_x[p] - base->ob_x[p]);
         vy = (int8_t)(uint8_t)(to_spr->ob_y[p] - base->ob_y[p]);
         if (vx > SS2FG_SPR_MAX_STEP || vx < -SS2FG_SPR_MAX_STEP || vy > SS2FG_SPR_MAX_STEP || vy < -SS2FG_SPR_MAX_STEP) vx = vy = 0;
         ox = lerp_i(0, vx, t_spr) - vx; oy = lerp_i(0, vy, t_spr) - vy;   /* 다음 포즈: 몸 이동만큼 뒤에서 출발 */
         for (k = 0; k < 64; k++)
         {
            int sy;
            if (mb[k])
            {
               pp_skip[k] = 1;
               sy = wrapc(base->ay[k] + base->line[0].spy) + (have_off ? offy[k] : 0);
               if (sy < ymin) ymin = sy;
               if (sy + 7 > ymax) ymax = sy + 7;
            }
            if (mt[k])
            {
               pp_add[k] = 1; pp_dx[k] = (int16_t)ox; pp_dy[k] = (int16_t)oy;
               sy = wrapc(to_spr->ay[k] + to_spr->line[0].spy) + oy;
               if (sy < ymin) ymin = sy;
               if (sy + 7 > ymax) ymax = sy + 7;
            }
         }
         pp_on = 1;
      }
      pp_to = to_spr;
      pp_ymin = ymin; pp_ymax = ymax;
   }

   for (y = 0; y < SS2FG_H; y++)
   {
      render_line(base, to_scr, have_off ? offx : 0, offy, t_scr, y, scan);
      if (pp_on && y >= pp_ymin && y <= pp_ymax)
      {
         uint16_t scan2[256];
         int a = t_spr;
         pp_on = 2;                                             /* 둘째 판: 섞을 몸을 다음 포즈로 */
         render_line(base, to_scr, have_off ? offx : 0, offy, t_scr, y, scan2);
         pp_on = 1;
         for (x = 0; x < SS2FG_W; x++)
            if (scan2[x] != scan[x]) scan[x] = mix12(scan[x], scan2[x], a);
      }
      if (bpp == 4)
      {
         uint32_t *row = (uint32_t *)dst + (size_t)y * pitch_px;
         for (x = 0; x < SS2FG_W; x++)
            row[x] = colormap[scan[x] & 4095];
      }
      else
      {
         uint16_t *row = (uint16_t *)dst + (size_t)y * pitch_px;
         for (x = 0; x < SS2FG_W; x++)
            row[x] = (uint16_t)colormap[scan[x] & 4095];
      }
   }
   if (bpp == 2) id_build(base, to_scr, t_scr, have_off ? offx : 0, offy, colormap);   /* 서기 사이 그림 조각(패치 100) */
   fg_ov_on[0] = fg_ov_on[1] = 0;                              /* 몸 따로 박자는 한 번 그리기용 */
   pp_on = 0;
   return 1;
}

int ss2fg_render_ex(const ss2fg_frame *base, const ss2fg_frame *to, int t256,
                    void *dst, int pitch_px, int bpp, const uint32_t *colormap)
{
   return ss2fg_render2(base, to, t256, to, t256, dst, pitch_px, bpp, colormap);
}

int ss2fg_render(const ss2fg_frame *base, const ss2fg_frame *to, int t256,
                 uint16_t *dst, int pitch_px, const uint32_t *colormap)
{
   return ss2fg_render_ex(base, to, t256, dst, pitch_px, 2, colormap);
}

/* a→b 사이에 무엇이 움직였나 — 비트0 스프라이트(무리 합의·겉모습 짝짓기를 거친 최종 이동량이 0 이 아닌 조각),
   비트1 스크롤(어느 줄이든 플레인 스크롤값이 다름). 4배 모드가 «다음에 바뀌는 프레임»을 찾을 때 쓴다. */
int ss2fg_motion(const ss2fg_frame *a, const ss2fg_frame *b)
{
   int f = 0, i, y;
   fg_move mv[64];
   if (!a || !b || !a->valid || !b->valid) return 0;
   sprite_moves(a, b, mv);
   for (i = 0; i < 64; i++)
      if (mv[i].has && (mv[i].dx || mv[i].dy)) { f |= 1; break; }
   for (y = 0; y < SS2FG_H; y++)
   {
      const ss2fg_regs *ra = &a->line[y], *rb = &b->line[y];
      if (ra->s1x != rb->s1x || ra->s1y != rb->s1y || ra->s2x != rb->s2x || ra->s2y != rb->s2y) { f |= 2; break; }
   }
   return f;
}
