/* ss2fg — 프레임 생성(중간 프레임 합성). 설명은 ss2fg.h.
 *
 * 렌더러는 mednafen gfx.c 의 draw_scanline_colour 를 그대로 옮기되
 *   · 스크롤값·스프라이트 오프셋은 스캔라인별로 a→b 보간한 값을 쓰고
 *   · 스프라이트는 체인을 푼 절대좌표를 슬롯별로 보간한다(같은 타일·팔레트·플립·
 *     우선순위일 때만 — 아니면 b 자리).
 * 타일·타일맵·팔레트는 b 의 것. 창(window)·배경색·반전·플레인 순서도 b 의 것. */
#include <string.h>
#include "ss2fg.h"

/* ───────────── 캡처 ─────────────
   지난 프레임 몇 장을 쌓아 둔다(최근이 0번). 4배(게임 박자 맞춤) 예측은 N+1·N+2 를 미리 돌려 두 장을
   쌓았다가 두 번 무른다. */
#define FG_RING 6
static ss2fg_frame fg_slot[FG_RING];
static int fg_hist[FG_RING] = { -1, -1, -1, -1, -1, -1 };   /* fg_hist[0] = 마지막(현재), [1] = 그 앞 … (-1 = 없음) */
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

void ss2fg_capture_end(const uint8_t *scroll, const uint8_t *chr, const uint8_t *spr,
                       const uint8_t *sprcol, const uint8_t *pal, int mono, int layers)
{
   ss2fg_frame *f = &fg_slot[fg_build];
   int k;
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
      c = ld16(palette + ((index & 3) << 1));
      if (r->neg) c = (uint16_t)~c;
      scan[xx] = c;
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

static void sprite_moves(const ss2fg_frame *base, const ss2fg_frame *to, fg_move mv[64])
{
   const ss2fg_regs *rb = &base->line[0], *rt = &to->line[0];
   uint8_t cnt_b[512], cnt_t[512];
   fg_move cand[64];
   int i, g, gstart;
   memset(mv, 0, 64 * sizeof *mv);
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
      if (nvis < 2)
      {
         for (k = gstart; k < g; k++) if (cand[k].has) mv[k] = cand[k];   /* 단독 스프라이트 — 슬롯 규칙 */
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
      if (nvote == 0) continue;                                 /* 믿을 조각이 없다(포즈 전체 교체) → 무리 정지 */
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
      if (npc < 2 || bestw < 2 * second || bestw * 4 < nlook * FG_VOTE) continue;   /* 표 부족 → 무리 정지 */
      for (a = 0; a < nvote; a++)
      {
         int ddx = vdx[a] - vdx[best], ddy = vdy[a] - vdy[best];
         if (ddx >= -1 && ddx <= 1 && ddy >= -1 && ddy <= 1) { sumx += vdx[a] * vw[a]; sumy += vdy[a] * vw[a]; }
      }
      {  /* 자리 겹침 검증: 벡터만큼 옮긴 자리에 같은 조각이 있는 수가 제자리보다 많아야 몸이 움직인 것 */
         int vx = round_div(sumx, bestw), vy = round_div(sumy, bestw);
         if ((vx || vy) && overlap_count(base, to, gstart, g, 0, 0) >= overlap_count(base, to, gstart, g, vx, vy)) continue;
         for (k = gstart; k < g; k++)
            if (ld16(base->spr + k * 4) & 0x1800) { mv[k].dx = (int16_t)vx; mv[k].dy = (int16_t)vy; mv[k].has = 1; }
      }
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

static void render_line(const ss2fg_frame *base, const ss2fg_frame *to, const fg_move mv[64],
                        int t, int ts, int y, uint16_t *scan)
{
   const ss2fg_regs *rb = &base->line[y];
   uint8_t zbuf[256];
   uint16_t c;
   int x = 0, spr;
   int in_win = (y >= rb->winy) && (y < rb->winy + rb->winh);

   memset(zbuf, 0, sizeof zbuf);

   /* 창 밖 색 */
   c = ld16(base->pal + 0x01F0 + (rb->oowc << 1));
   if (rb->neg) c = (uint16_t)~c;
   if (in_win)
   {
      for (x = 0; x < imin(rb->winx, SS2FG_W); x++) scan[x] = c;
      x = imin(rb->winx + rb->winw, SS2FG_W);
   }
   for (; x < SS2FG_W; x++) scan[x] = c;

   if (!in_win) return;

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

   /* 스프라이트 — base 의 것을 이동량만큼 옮겨 그린다 */
   if (base->layers & 4)
   for (spr = 0; spr < 64; spr++)
   {
      uint16_t d = ld16(base->spr + spr * 4);
      unsigned priority = (d & 0x1800) >> 11;
      int sx, sy;
      if (priority == 0) continue;
      sx = wrapx(base->ax[spr] + rb->spx);
      sy = wrapc(base->ay[spr] + rb->spy);
      if (mv && mv[spr].has)
      {
         sx = lerp_i(sx, mv[spr].dx, ts);
         sy = lerp_i(sy, mv[spr].dy, ts);
      }
      if (y >= sy && y <= sy + 7)
      {
         unsigned row = (unsigned)(y - sy) & 7;
         draw_pattern(base, rb, scan, zbuf, sx, d & 0x01FF, (d & 0x4000) ? 7 - row : row,
                      d & 0x8000, base->pal, base->sprcol[spr] & 0xF, (uint8_t)(priority << 1));
      }
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
   if (!base || !base->valid || base->mono) return 0;
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
   if (to_spr) sprite_moves(base, to_spr, mv);

   for (y = 0; y < SS2FG_H; y++)
   {
      render_line(base, to_scr, to_spr ? mv : 0, t_scr, t_spr, y, scan);
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
