/* ss2fg — 프레임 생성(중간 프레임 합성). 설명은 ss2fg.h.
 *
 * 렌더러는 mednafen gfx.c 의 draw_scanline_colour 를 그대로 옮기되
 *   · 스크롤값·스프라이트 오프셋은 스캔라인별로 a→b 보간한 값을 쓰고
 *   · 스프라이트는 체인을 푼 절대좌표를 슬롯별로 보간한다(같은 타일·팔레트·플립·
 *     우선순위일 때만 — 아니면 b 자리).
 * 타일·타일맵·팔레트는 b 의 것. 창(window)·배경색·반전·플레인 순서도 b 의 것. */
#include <string.h>
#include "ss2fg.h"

/* ───────────── 캡처 ───────────── */
static ss2fg_frame fg_slot[3];
static int fg_cur = -1, fg_prev = -1, fg_old = -1, fg_build = 0;
static uint8_t fg_build_dirty;

static int next_free(void)
{
   int i;
   for (i = 0; i < 3; i++)
      if (i != fg_cur && i != fg_prev) return i;
   return 0;
}

void ss2fg_reset(void)
{
   memset(fg_slot, 0, sizeof fg_slot);
   fg_cur = fg_prev = fg_old = -1;
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

   fg_old  = fg_prev;
   fg_prev = fg_cur;
   fg_cur  = fg_build;
   fg_build = next_free();
   fg_build_dirty = 0;
   /* 다음 프레임의 줄 레지스터는 처음부터 다시 채운다 — 못 채운 줄이 남지 않게 지난 값으로 초기화 */
   memcpy(fg_slot[fg_build].line, f->line, sizeof f->line);
}

void ss2fg_capture_pop(void)
{
   if (fg_cur < 0) return;
   fg_build = fg_cur;
   fg_cur   = fg_prev;
   fg_prev  = fg_old;
   fg_old   = -1;
   fg_build_dirty = 0;
}

const ss2fg_frame *ss2fg_prev(void) { return fg_prev >= 0 && fg_slot[fg_prev].valid ? &fg_slot[fg_prev] : 0; }
const ss2fg_frame *ss2fg_cur(void)  { return fg_cur  >= 0 && fg_slot[fg_cur].valid  ? &fg_slot[fg_cur]  : 0; }

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
   base 의 슬롯 i 가 to 에서 같은 물체(타일·플립·우선순위·팔레트 동일, 이동 ≤ 임계)면 그 이동량.
   체인으로 묶인 그룹(메타스프라이트)은 일치한 조각들의 이동량이 서로 1px 안에 모이고 절반 이상
   일치하면 그 값을 그룹 벡터로 삼아, 타일이 바뀐(애니메이션) 조각도 같이 움직인다. */
typedef struct { int16_t dx, dy; uint8_t has; } fg_move;

static void sprite_moves(const ss2fg_frame *base, const ss2fg_frame *to, fg_move mv[64])
{
   const ss2fg_regs *rb = &base->line[0], *rt = &to->line[0];
   int i, g, gstart;
   memset(mv, 0, 64 * sizeof *mv);

   for (i = 0; i < 64; i++)
   {
      uint16_t db = ld16(base->spr + i * 4), dt = ld16(to->spr + i * 4);
      int bx, by, tx, ty, dx, dy;
      if (!(db & 0x1800) || !(dt & 0x1800)) continue;          /* 둘 다 보여야 */
      if (db != dt || base->sprcol[i] != to->sprcol[i]) continue;
      bx = wrapx(base->ax[i] + rb->spx); by = wrapc(base->ay[i] + rb->spy);
      tx = wrapx(to->ax[i]   + rt->spx); ty = wrapc(to->ay[i]   + rt->spy);
      dx = tx - bx; dy = ty - by;
      if (dx > SS2FG_SPR_MAX_STEP || dx < -SS2FG_SPR_MAX_STEP ||
          dy > SS2FG_SPR_MAX_STEP || dy < -SS2FG_SPR_MAX_STEP) continue;
      mv[i].dx = (int16_t)dx; mv[i].dy = (int16_t)dy; mv[i].has = 1;
   }

   /* 체인 그룹: 슬롯 k 에 체인 비트(0x0600)가 있으면 k-1 과 같은 그룹 */
   for (gstart = 0; gstart < 64; gstart = g)
   {
      int n = 0, m = 0, k;
      int minx = 999, maxx = -999, miny = 999, maxy = -999;
      for (g = gstart + 1; g < 64; g++)
         if (!(ld16(base->spr + g * 4) & 0x0600)) break;
      if (g - gstart < 2) continue;
      for (k = gstart; k < g; k++)
      {
         if (!(ld16(base->spr + k * 4) & 0x1800)) continue;
         n++;
         if (!mv[k].has) continue;
         m++;
         if (mv[k].dx < minx) minx = mv[k].dx;
         if (mv[k].dx > maxx) maxx = mv[k].dx;
         if (mv[k].dy < miny) miny = mv[k].dy;
         if (mv[k].dy > maxy) maxy = mv[k].dy;
      }
      if (m == 0 || m * 2 < n) continue;                        /* 절반 미만 일치 → 믿지 않는다 */
      if (maxx - minx > 1 || maxy - miny > 1) continue;         /* 조각이 제각각 → 그룹 벡터 없음 */
      for (k = gstart; k < g; k++)
         if (!mv[k].has && (ld16(base->spr + k * 4) & 0x1800))
         {
            mv[k].dx = (int16_t)((minx + maxx) >= 0 ? (minx + maxx + 1) / 2 : -((-(minx + maxx) + 1) / 2));
            mv[k].dy = (int16_t)((miny + maxy) >= 0 ? (miny + maxy + 1) / 2 : -((-(miny + maxy) + 1) / 2));
            mv[k].has = 1;
         }
   }
}

static void render_line(const ss2fg_frame *base, const ss2fg_frame *to, const fg_move mv[64],
                        int t, int y, uint16_t *scan)
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
         sx = lerp_i(sx, mv[spr].dx, t);
         sy = lerp_i(sy, mv[spr].dy, t);
      }
      if (y >= sy && y <= sy + 7)
      {
         unsigned row = (unsigned)(y - sy) & 7;
         draw_pattern(base, rb, scan, zbuf, sx, d & 0x01FF, (d & 0x4000) ? 7 - row : row,
                      d & 0x8000, base->pal, base->sprcol[spr] & 0xF, (uint8_t)(priority << 1));
      }
   }
}

int ss2fg_render_ex(const ss2fg_frame *base, const ss2fg_frame *to, int t256,
                    void *dst, int pitch_px, int bpp, const uint32_t *colormap)
{
   int y, x;
   uint16_t scan[256];
   fg_move mv[64];
   if (!base || !base->valid || base->mono) return 0;
   if (bpp != 2 && bpp != 4) return 0;
   /* 표시 도중 스프라이트표/타일맵이 바뀐 프레임은 끝 시점 사본이 위쪽 줄과 안 맞는다 — 합성 포기 */
   if (base->dirty & (SS2FG_DIRTY_SPR | SS2FG_DIRTY_SCROLL)) return 0;
   if (to && (!to->valid || to->mono || (to->dirty & (SS2FG_DIRTY_SPR | SS2FG_DIRTY_SCROLL)))) to = 0;
   if (t256 < 0) t256 = 0;
   if (t256 > 256) t256 = 256;
   if (to && t256 == 0) to = 0;
   if (to) sprite_moves(base, to, mv);

   for (y = 0; y < SS2FG_H; y++)
   {
      render_line(base, to, to ? mv : 0, t256, y, scan);
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

int ss2fg_render(const ss2fg_frame *base, const ss2fg_frame *to, int t256,
                 uint16_t *dst, int pitch_px, const uint32_t *colormap)
{
   return ss2fg_render_ex(base, to, t256, dst, pitch_px, 2, colormap);
}
