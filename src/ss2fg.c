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
#define FG_RAM_SELECT 240         /* 대전 밖 — 캐릭터·유파 고르기 화면(물체 0 = 가운데 미리보기 몸 80,76), 판 시작 전 소개 */
#define FG_RAM_CAMX   0x176D
#define FG_RAM_OBJ    0x0E00
static const uint8_t *fg_ram = 0;
void ss2fg_set_ram(const uint8_t *ram) { fg_ram = ram; }
static int id_on;                 /* 서기 사이 그림(아래 «서기 자세») 켬 */

static void capture_body(ss2fg_frame *f)
{
   const ss2fg_frame *pv = fg_hist[0] >= 0 ? &fg_slot[fg_hist[0]] : 0;   /* 직전 캡처 */
   int k, sel = 0;
   f->now_ok = (uint8_t)(fg_ram && fg_ram[FG_RAM_MODE] == FG_RAM_FIGHT);
   /* 대전 밖(모드 240) — 고르기 화면 미리보기(물체 0 = 가운데 80,76, 유저 「여기서는 안 되는 거?」, 패치 96) +
      판 시작 전 소개(«いざ尋常に»·«一本目» 동안 두 캐릭터가 대전 자리에 서 있다 — 모드가 240 이라 사이 그림이 대전부터만 나왔다,
      유저 「대전 시작 전에도 좀 되게」, 패치 99). 서기 그리기가 켜졌을 때만, 몸(물체 0·1)만 — 그림자·이펙트 물체는 안 본다.
      끄면 예전과 같다(대전 밖은 몸 없음) */
   if (!f->now_ok && id_on && fg_ram && fg_ram[FG_RAM_MODE] == FG_RAM_SELECT)
   {
      const uint8_t *o = fg_ram + FG_RAM_OBJ;
      sel = !(o[0x36] == 0xFF && o[0x37] == 0xFF);
      f->now_ok = (uint8_t)sel;
   }
   f->ob_now_act = 0;
   for (k = 0; k < SS2FG_OBJS; k++)
   {
      const uint8_t *o = f->now_ok && (!sel || k <= 1) ? fg_ram + FG_RAM_OBJ + 0x40 * k : 0;
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
      if (right - highmark >= 8) return;      /* (main) 8 화소 모두 창 오른쪽 밖 — 그릴 것 없음. 그냥 두면 시프트가 32 이상(정의 안 된 동작, UBSan) */
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
   if (right > highmark) { if (right - highmark >= 8) return; index >>= (right - highmark) * 2; right = highmark; }   /* (main) 위와 같은 시프트 막기 */
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

/* 조각의 겉모습 지문 — 반전(H/V)까지 적용한 8×8 2bpp 8줄의 FNV-1a(빈 조각 = 0). 팔레트·우선순위는 안 본다.
   PocketCore 방 10_ss2fg_consensus.patch 의 piece_look 과 **바이트까지 같아야 한다**: body_sig(몸 포즈 지문)가 이걸로 만들어지고,
   모션 캐시(ss2_motion.bin, 패치 97)의 색인·순환 표 열쇠가 바로 그 지문이다. 예전 main 은 «반전·우선순위 비트 + 팔레트 + 타일 16바이트»
   를 해시해서(패치 80 이식 때 그쪽 piece_look 이 없어 대신 둔 것) 지문이 달라 진짜 캐시(44,438 바뀜)를 하나도 못 찾았다 */
static uint64_t piece_look(const ss2fg_frame *f, int k)
{
   uint16_t w = ld16(f->spr + k * 4);
   unsigned tile = w & 0x01FF, r;
   uint64_t h = 1469598103934665603ULL;
   int any = 0;
   if (!(w & 0x1800)) return 0;
   for (r = 0; r < 8; r++)
   {
      unsigned row = (w & 0x4000) ? 7 - r : r;
      uint16_t v = ld16(f->chr + tile * 16 + row * 2);
      if (w & 0x8000) v = (uint16_t)(mirrored[(v & 0xff00) >> 8] | (mirrored[v & 0xff] << 8));
      if (v) any = 1;
      h = (h ^ (v & 0xff)) * 1099511628211ULL;
      h = (h ^ (v >> 8)) * 1099511628211ULL;
   }
   return any ? (h | 1) : 0;                                   /* 빈 조각은 0 */
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
#define ID_NONE   0xFFFF        /* 빈 화소(그림 값은 팔레트<<2 | 색 번호 = 0x3F 까지) */
#define ID_BIGP   16            /* 99f: 안 맞는 덩어리가 이만큼(화소) 되면 «부분이 멀리 감» — 캐시 항목이 있으면 캐시로 */

typedef struct id_pose_s {
   uint64_t sig; unsigned used;
   int ox, oy, w, h;            /* 몸 자리(물체 X·Y = 발 가운데) 기준 그림 왼쪽 위, 크기 */
   uint8_t depth, first;        /* 몸 조각의 우선순위<<1, 가장 앞(가장 작은) 슬롯 */
   uint16_t c[ID_W * ID_H];     /* 팔레트<<2 | 색 번호(1~3), ID_NONE = 빈 화소. 색값이 아니라 번호로 — 같은 포즈라도 팔레트가 바뀐다
                                   (고르기 화면↔대전, 색 고르기, 화면이 어두워지는 때) — 기억한 색으로 그리면 사이 그림만 다른 색이 된다 */
   uint16_t mincc;              /* (main) 그림의 8-이웃 덩이 중 가장 작은 것의 화소 수 — 캐시 사이 그림의 떨어진 조각 지우기에 쓴다 */
} id_pose;

typedef struct {
   uint64_t sa, sb; unsigned used;
   int ox, oy, w, h;            /* 둘을 합친 틀(몸 자리 기준) */
   int aox, aoy, box, boy;      /* 만들 때의 A·B 자리(몸 자리 기준 왼쪽 위) — 지금 기억한 포즈와 다르면 다시 만든다(패치 99e) */
   uint8_t depth, first, small;  /* small = 작은 바뀜(옮긴 앞 그림이 뒤 그림 화소를 75% 넘게 맞춤 — 서기처럼) */
   uint8_t bigp;                 /* 99f: 이동표로 안 맞는 화소의 가장 큰 덩어리(8방향)가 ID_BIGP 넘음 — 칼처럼 가는 부분이 ±3px 밖으로 감 */
   uint16_t ca[ID_W * ID_H], cb[ID_W * ID_H];
   int8_t dax[ID_W * ID_H], day[ID_W * ID_H];   /* A 화소가 B 쪽으로 갈 때의 이동(A 격자) */
   int8_t dbx[ID_W * ID_H], dby[ID_W * ID_H];   /* B 화소가 A 의 어디서 왔나: B(p) = A(p - d) (B 격자) */
} id_pair;

typedef struct { uint64_t sa, sb; unsigned seen; uint16_t hold; uint8_t cnt; } id_learn_t;
struct mc_ent;
typedef struct { int on; id_pair *pr; int t256, ax, ay;
                 int kind;                    /* 0 = 서기(배운 바뀜, id_pair) · 1 = 모션 캐시 */
                 struct mc_ent *e; int ti; const struct id_pose_s *A, *B;
                 int gx, gy, gw, gh, depth, first; /* 4배 조각 틀: 몸 자리 기준 왼쪽 위·크기(1배), 깊이, 첫 슬롯 */
                 int sx4, sy4;                /* 캐시 사이 그림을 더 미는 양(4배 화소) — 포즈 자리 어긋남을 t 만큼 나눠 메운다(패치 99c) */
               } id_req_t;
typedef struct { int on, x0, y0, w, h; uint16_t px[(ID_W * ID_S) * (ID_H * ID_S)]; } id_patch_t;   /* px = 12비트 색(반전 적용) — 내보낼 때 색표를 거친다 */

static id_pose    id_pose_c[2][ID_POSES];
static id_pair    id_pair_c[2][ID_PAIRS];
static id_learn_t id_lt[2][ID_LEARN];
static unsigned   id_clock = 0;
static uint64_t   id_cur[2], id_prev[2];     /* 지금 포즈, 그 앞 포즈(제자리에서 바뀐 때만 — 왕복 동작의 다음 포즈 고르기) */
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
      memset(id_prev, 0, sizeof id_prev);
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
/* (main) 8-이웃 덩이 — 1배 칸 격자(occ[i] != 0 = 차 있음). lab 에 덩이 번호(1~), 반환 = 덩이 수, sz[번호] = 칸 수.
   캐시 사이 그림에서 A/B 를 칸마다 고르다 남은 «떨어진 조각»(옛 칼끝, 새 칼끝만 먼저 온 것, 한 칸 티끌)을 찾는 데 쓴다 */
static uint16_t id_cc_lab[ID_W * ID_H], id_cc_stk[ID_W * ID_H], id_cc_sz[ID_W * ID_H + 1];
static int id_cc_label(const uint8_t *occ, int w, int h)
{
   int i, n = 0;
   for (i = 0; i < w * h; i++) id_cc_lab[i] = 0;
   for (i = 0; i < w * h; i++)
   {
      int sp = 0, cnt = 0;
      if (!occ[i] || id_cc_lab[i]) continue;
      n++; id_cc_lab[i] = (uint16_t)n; id_cc_stk[sp++] = (uint16_t)i;
      while (sp)
      {
         int q = id_cc_stk[--sp], qx = q % w, qy = q / w, dx, dy;
         cnt++;
         for (dy = -1; dy <= 1; dy++)
            for (dx = -1; dx <= 1; dx++)
            {
               int nx = qx + dx, ny = qy + dy, r;
               if ((!dx && !dy) || nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
               r = ny * w + nx;
               if (occ[r] && !id_cc_lab[r]) { id_cc_lab[r] = (uint16_t)n; id_cc_stk[sp++] = (uint16_t)r; }
            }
      }
      id_cc_sz[n] = (uint16_t)(cnt > 65535 ? 65535 : cnt);
   }
   return n;
}
static uint16_t id_min_cc(const uint16_t *c, int w, int h)
{
   static uint8_t occ[ID_W * ID_H];
   int i, n, m = 65535;
   for (i = 0; i < w * h; i++) occ[i] = c[i] != ID_NONE;
   n = id_cc_label(occ, w, h);
   for (i = 1; i <= n; i++) if (id_cc_sz[i] < m) m = id_cc_sz[i];
   return (uint16_t)m;
}

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
            ps->c[o] = (uint16_t)(((f->sprcol[k] & 0xF) << 2) | idx);   /* 색은 그릴 때 그 프레임 팔레트로 */
         }
      }
   }
   ps->mincc = id_min_cc(ps->c, ps->w, ps->h);
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
         id_lt[p][i].seen = ++id_clock;
         id_stats[0]++;
         return;
      }
   /* 가장 오래 안 본 칸을 바꾼다. (예전: 횟수가 가장 적은 칸 — 표가 «2번 이상 본 바뀜»으로 다 차면 새 바뀜은 1번째에서
      서로 밀어내 영영 2번이 못 됐다 = 상대가 바뀌는 아케이드 몇 판 뒤·캐릭터를 바꾼 뒤 서기 그리기가 멈춤) */
   for (i = 1; i < ID_LEARN; i++) if (id_lt[p][i].seen < id_lt[p][lo].seen) lo = i;
   id_lt[p][lo].sa = sa; id_lt[p][lo].sb = sb; id_lt[p][lo].hold = (uint16_t)hold; id_lt[p][lo].cnt = 1;
   id_lt[p][lo].seen = ++id_clock;
   id_stats[0]++;
}
/* A 다음 포즈 — 같은 바뀜을 2번 이상 봤고 두 번째로 많은 것보다 두 배 이상일 때만. 그래도 못 정하면 방금 온 포즈(from)를 빼고
   다시 본다 — 왕복 동작의 가운데 포즈(우쿄 서기 57c3→f629→9621→f629→57c3: f629 다음은 온 쪽의 반대) */
static uint64_t id_next(int p, uint64_t sa, uint64_t from, unsigned *hold)
{
   int pass;
   for (pass = 0; pass < 2; pass++)
   {
      int i, b1 = -1, c2 = 0;
      if (pass && !from) break;
      for (i = 0; i < ID_LEARN; i++)
      {
         if (id_lt[p][i].sa != sa || !id_lt[p][i].cnt || (pass && id_lt[p][i].sb == from)) continue;
         if (b1 < 0 || id_lt[p][i].cnt > id_lt[p][b1].cnt) { if (b1 >= 0 && id_lt[p][b1].cnt > c2) c2 = id_lt[p][b1].cnt; b1 = i; }
         else if (id_lt[p][i].cnt > c2) c2 = id_lt[p][i].cnt;
      }
      if (b1 >= 0 && id_lt[p][b1].cnt >= 2 && id_lt[p][b1].cnt >= 2 * c2)
      {
         if (hold) *hold = id_lt[p][b1].hold;
         return id_lt[p][b1].sb;
      }
   }
   return 0;
}
/* A→B 가 되풀이 동작(서기처럼 같은 포즈들이 돈다)의 한 칸인가 — B 에서 배운 다음 포즈를 따라가 A 로 돌아오면 1.
   착지→서기·공격 끝 자세→서기처럼 한 번 지나가는 바뀜은 안 그린다 — 사이 그림은 반쯤에서 다음 그림으로 넘어가서 그 자세가
   끝나는 때가 앞당겨 보인다(서기는 돌기만 하니 상관없다). 하니스: 점프 착지 a944→3902(22 프레임)를 세 번째부터 그렸다 */
static int id_loop(int p, uint64_t sa, uint64_t sb)
{
   uint64_t from = sa, cur = sb;
   int n;
   for (n = 0; n < ID_POSES; n++)
   {
      uint64_t nx = id_next(p, cur, from, 0);
      if (!nx) return 0;
      if (nx == sa) return 1;
      from = cur; cur = nx;
   }
   return 0;
}
/* (main) 왕복 동작(P0→P1→P2→P3→P2→P1→P0, 고르기 화면 미리보기)의 가운데 포즈에서 «온 쪽으로 되돌아감» 예측을 믿지 않는 때 —
   id_next 의 «두 배» 규칙은 가운데 포즈의 두 갈래 횟수가 2:1 일 때 온 쪽을 골라, 미리 본 프레임으로 확인하기 전까지 반대 방향으로
   그리다가 되돌아왔다(검수: 고르기 화면 P3 다음 P2 에서 P3 쪽으로 80% 까지 갔다가 P1 로). 다른 갈래가 온 쪽의 반 이상이면 의심.
   끝 포즈(P0·P3)는 다른 장면에서 한 번 본 갈래가 있어도 온 쪽 횟수가 3 이 되면 다시 믿는다 */
static int id_back_doubt(int p, uint64_t sa, uint64_t from, uint64_t nb)
{
   int i, cf = 0, ca = 0;
   if (!from || nb != from) return 0;
   for (i = 0; i < ID_LEARN; i++)
      if (id_lt[p][i].sa == sa && id_lt[p][i].cnt)
      {
         if (id_lt[p][i].sb == from) cf = id_lt[p][i].cnt;
         else if (id_lt[p][i].cnt > ca) ca = id_lt[p][i].cnt;
      }
   return ca && 2 * ca >= cf;
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
   A = id_find_pose(p, sa); B = id_find_pose(p, sb);
   for (i = 0; i < ID_PAIRS; i++)
      if (id_pair_c[p][i].sa == sa && id_pair_c[p][i].sb == sb)
      {
         pr = &id_pair_c[p][i];
         /* 99e: 포즈가 밀려났다 다시 잡혔거나 장면이 바뀌어 자리·우선순위가 달라졌으면 옛 틀로 그리면 옆으로 밀린다 — 이 칸에 다시 만든다 */
         if (A && B && pr->aox == A->ox && pr->aoy == A->oy && pr->box == B->ox && pr->boy == B->oy && pr->depth == A->depth
             && B->depth == A->depth) { pr->used = ++id_clock; return pr; }
         pr->sa = pr->sb = 0; oldest = i; break;
      }
   if (!A || !B || A->depth != B->depth) return 0;
   if (i == ID_PAIRS)
      for (i = 1; i < ID_PAIRS; i++) if (id_pair_c[p][i].used < id_pair_c[p][oldest].used) oldest = i;
   pr = &id_pair_c[p][oldest];
   {
      int x0 = A->ox < B->ox ? A->ox : B->ox, y0 = A->oy < B->oy ? A->oy : B->oy;
      int x1 = A->ox + A->w > B->ox + B->w ? A->ox + A->w : B->ox + B->w;
      int y1 = A->oy + A->h > B->oy + B->h ? A->oy + A->h : B->oy + B->h;
      if (x1 - x0 > ID_W || y1 - y0 > ID_H) return 0;
      pr->ox = x0; pr->oy = y0; pr->w = x1 - x0; pr->h = y1 - y0;
      pr->aox = A->ox; pr->aoy = A->oy; pr->box = B->ox; pr->boy = B->oy;
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
   {  /* 작은 바뀜인가 — 이동표로 옮긴 앞 그림이 뒤 그림 화소를(거꾸로도) 75% 넘게 맞추면. 하니스 실측(하오마루 판):
         서기 바뀜 80~94%(칼이 크게 도는 것 포함), 착지→서기 65·59%, 공격 끝 자세→서기 60·54%, 공격 중 46~70% */
      int na = 0, ga = 0, nb = 0, gb = 0, sx, sy, q;
      for (y = 0; y < pr->h; y++)
         for (x = 0; x < pr->w; x++)
         {
            q = y * pr->w + x;
            if (pr->cb[q] != ID_NONE)
            {
               nb++; sx = x - pr->dbx[q]; sy = y - pr->dby[q];
               if (sx >= 0 && sy >= 0 && sx < pr->w && sy < pr->h && pr->ca[sy * pr->w + sx] == pr->cb[q]) gb++;
            }
            if (pr->ca[q] != ID_NONE)
            {
               na++; sx = x + pr->dax[q]; sy = y + pr->day[q];
               if (sx >= 0 && sy >= 0 && sx < pr->w && sy < pr->h && pr->cb[sy * pr->w + sx] == pr->ca[q]) ga++;
            }
         }
      pr->small = (uint8_t)(na && nb && ga * 4 >= na * 3 && gb * 4 >= nb * 3);
   }
   {  /* 99f: 작은 바뀜이라도(서기 80~94% 맞음) 칼·창처럼 가는 부분이 ±3px 밖으로 옮겨 가면 그 부분만 이동표가 못 따라가
         사이 그림에서 칼 가운데가 끊기거나 조각이 흩어진다(유저 2026-10-12 「하오마루 뉴트럴 칼 빈틈」 — 서기 8장 중 5바뀜, 덩어리 22~78).
         숨쉬기처럼 1~2px 움직임은 안 맞는 화소가 흩어져 덩어리가 작다(대전 제자리 바뀜 1,045개: 가운데 14, 칼 끊김 보인 것 19 이상).
         양쪽(B→A·A→B) 안 맞는 화소의 가장 큰 덩어리를 재어 둔다 — id_plan_learned 가 캐시 항목이 있으면 캐시에 맡긴다 */
      static uint8_t um[ID_W * ID_H];
      static int stk[ID_W * ID_H];
      int side, best = 0, n = pr->w * pr->h, q;
      for (side = 0; side < 2; side++)
      {
         for (q = 0; q < n; q++)
         {
            int xq = q % pr->w, yq = q / pr->w, sx, sy, ok;
            um[q] = 0;
            if (side == 0)
            {
               if (pr->cb[q] == ID_NONE) continue;
               sx = xq - pr->dbx[q]; sy = yq - pr->dby[q];
               ok = sx >= 0 && sy >= 0 && sx < pr->w && sy < pr->h && pr->ca[sy * pr->w + sx] == pr->cb[q];
            }
            else
            {
               if (pr->ca[q] == ID_NONE) continue;
               sx = xq + pr->dax[q]; sy = yq + pr->day[q];
               ok = sx >= 0 && sy >= 0 && sx < pr->w && sy < pr->h && pr->cb[sy * pr->w + sx] == pr->ca[q];
            }
            um[q] = (uint8_t)!ok;
         }
         for (q = 0; q < n; q++)
            if (um[q])
            {
               int sp = 0, cnt = 0;
               um[q] = 0; stk[sp++] = q;
               while (sp)
               {
                  int c = stk[--sp], cx = c % pr->w, cy = c / pr->w, a, b;
                  cnt++;
                  for (b = -1; b <= 1; b++)
                     for (a = -1; a <= 1; a++)
                     {
                        int nx = cx + a, ny = cy + b, j;
                        if (nx < 0 || ny < 0 || nx >= pr->w || ny >= pr->h) continue;
                        j = ny * pr->w + nx;
                        if (um[j]) { um[j] = 0; stk[sp++] = j; }
                     }
               }
               if (cnt > best) best = cnt;
            }
      }
      pr->bigp = (uint8_t)(best >= ID_BIGP);
   }
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

/* ── 모션 캐시(코어 패치 97) — 공격 등 모든 포즈 바뀜의 사이 그림 ─────────────────────────────────────────────
   유저 2026-10-11 「전 모션 … 바로바로 어려우면 캐시 만들어서」·「어떤 방식으로 수행이 어려우면 캐시를 만들어서 업로드」.
   사이 그림은 미리(PC) AI 흐름(RIFE v4.6)으로 계산해 두고 여기서는 꺼내 붙이기만 한다. 파일(시스템 폴더 ss2_motion.bin)엔
   그림이 없다 — 바뀜(A→B)마다 t = ¼·½·¾ 의 «1배 칸마다 A 에서 가져올지 B 에서 가져올지 + 그 이동»뿐. 색은 지금 프레임의 그림·팔레트.
   B 는 미리 돌린 D+1·D+2 에서 안다 → 바뀌기 전 마지막 2 프레임(4 출력)에 ¼·½·¾ 를 그린다(길게 서 있는 자세는 끝에만 — 끝나는 때가 안 당겨짐).
   제자리 바뀜만(몸 자리가 그대로). 캐시에 없으면 예전처럼 툭. 형식은 tools/motion_cache.py 머리글. */
#include <stdio.h>
#include <zlib.h>
typedef struct { uint64_t sa, sb; uint32_t off, len; } mc_idx_t;
typedef struct mc_ent {
   uint64_t sa, sb; unsigned used;
   int8_t cx, cy, bdx, bdy; uint8_t w, h, aw, ah, bw, bh;
   uint32_t ro[3], mo[3], fo[3];        /* t 마다 칸 비트맵·A 고르기 비트맵·이동의 자리 */
   uint8_t *buf; uint32_t blen;
} mc_ent;
static FILE    *mc_f = 0;
static mc_idx_t *mc_ix = 0;
static uint32_t mc_n = 0;
/* 순환 표(패치 99b) — 모아 둔 판들에서 본 되풀이 동작(걷기·서기 등)의 «A 다음 B, 길이». 파일 끝 12바이트 'SS2L' u32 개수 u32 자리,
   항목 20바이트(u64 A, u64 B, u16 길이, u16 0), A 오름차순. 이걸로 배우기 전(그 판 처음 몇 걸음)부터 한 걸음 내내 편다 */
typedef struct { uint64_t sa, sb; uint16_t hold; } mc_loop_t;
static mc_loop_t *mc_lp = 0;
static uint32_t mc_ln = 0;
static int      mc_on = 0;
static mc_ent   mc_c[8];
static int      mc_stats[4];               /* [0] 찾음 [1] 있음 [2] 그림 [3] 틀 안 맞음 */
static int      mc_bunk, mc_loopstop, mc_speck;   /* (main 계기) 틀 안 맞음 중 B 그림 없음 / 순환 예측 그만 / 지운 조각 칸 */
void ss2fg_motion_stats2(int *out3) { out3[0] = mc_bunk; out3[1] = mc_loopstop; out3[2] = mc_speck; }
void ss2fg_set_motion(int on) { mc_on = on ? 1 : 0; }
/* 계기(main) — 캐시에 없던 바뀜(p, A→B)을 32개까지 쌓아 두고 하네스(tools/harness/fgrom.c FGROM_MOTION_LOG)가 꺼내 간다.
   그 포즈 틀(ss2fg_idle_pose_info)과 함께 가짜 캐시(tools/harness/motion_fake.py)를 만들어 그리는 길을 눌러 보는 용도. 출력엔 영향 없음 */
static struct { uint64_t sa, sb; int p; } mc_miss[32];
static unsigned mc_miss_w = 0, mc_miss_r = 0;
static void mc_miss_push(int p, uint64_t sa, uint64_t sb)
{
   unsigned i;
   for (i = mc_miss_r; i != mc_miss_w; i++)
      if (mc_miss[i & 31].sa == sa && mc_miss[i & 31].sb == sb && mc_miss[i & 31].p == p) return;
   if (mc_miss_w - mc_miss_r == 32) mc_miss_r++;
   i = mc_miss_w++ & 31; mc_miss[i].sa = sa; mc_miss[i].sb = sb; mc_miss[i].p = p;
}
int ss2fg_motion_miss(uint64_t *sa, uint64_t *sb, int *p)
{
   unsigned i;
   if (mc_miss_r == mc_miss_w) return 0;
   i = mc_miss_r++ & 31; *sa = mc_miss[i].sa; *sb = mc_miss[i].sb; *p = mc_miss[i].p;
   return 1;
}
int ss2fg_idle_pose_info(int p, uint64_t sig, int *out6)
{
   int i;
   if (p < 0 || p > 1 || !sig) return 0;
   for (i = 0; i < ID_POSES; i++)
      if (id_pose_c[p][i].sig == sig)
      {
         const id_pose *q = &id_pose_c[p][i];
         out6[0] = q->ox; out6[1] = q->oy; out6[2] = q->w; out6[3] = q->h; out6[4] = q->depth; out6[5] = q->first;
         return 1;
      }
   return 0;
}
int  ss2fg_motion_count(void) { return mc_f ? (int)mc_n : -1; }
void ss2fg_motion_stats(int *out4) { int i; for (i = 0; i < 4; i++) out4[i] = mc_stats[i]; }
static uint32_t mc_rd32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint64_t mc_rd64(const uint8_t *p) { return (uint64_t)mc_rd32(p) | ((uint64_t)mc_rd32(p + 4) << 32); }
void ss2fg_motion_close(void)
{
   int i;
   if (mc_f) fclose(mc_f);
   mc_f = 0; free(mc_ix); mc_ix = 0; mc_n = 0;
   free(mc_lp); mc_lp = 0; mc_ln = 0;
   for (i = 0; i < 8; i++) { free(mc_c[i].buf); memset(&mc_c[i], 0, sizeof mc_c[i]); }
}
/* 반환: 바뀜 수(0 이상), 없거나 틀리면 -1 */
int ss2fg_motion_open(const char *path)
{
   uint8_t hd[16], *ib = 0;
   uint32_t n, i;
   ss2fg_motion_close();
   if (!path || !(mc_f = fopen(path, "rb"))) return -1;
   if (fread(hd, 1, 16, mc_f) != 16 || memcmp(hd, "SS2M", 4) || hd[4] != 2 || hd[6] != 3 || hd[7] != 64 || hd[8] != 128 || hd[9] != 192)
   { ss2fg_motion_close(); return -1; }
   n = mc_rd32(hd + 10);
   if (!n || n > 4000000u || !(ib = (uint8_t *)malloc((size_t)n * 24)) || !(mc_ix = (mc_idx_t *)malloc((size_t)n * sizeof *mc_ix)))
   { free(ib); ss2fg_motion_close(); return -1; }            /* ib 는 0 으로 시작 — n 이 터무니없으면 malloc 전에 빠져나와 ib 가 안 잡힌 채였다(그쪽 패치 97: 미초기화 free) */
   if (fread(ib, 24, n, mc_f) != n) { free(ib); ss2fg_motion_close(); return -1; }
   for (i = 0; i < n; i++)
   {
      mc_ix[i].sa = mc_rd64(ib + i * 24); mc_ix[i].sb = mc_rd64(ib + i * 24 + 8);
      mc_ix[i].off = mc_rd32(ib + i * 24 + 16); mc_ix[i].len = mc_rd32(ib + i * 24 + 20);
   }
   free(ib);
   mc_n = n;
   {  /* 순환 표(있으면) */
      uint8_t tr[12];
      if (!fseek(mc_f, -12, SEEK_END) && fread(tr, 1, 12, mc_f) == 12 && !memcmp(tr, "SS2L", 4))
      {
         uint32_t ln = mc_rd32(tr + 4), lo = mc_rd32(tr + 8);
         uint8_t *lb;
         if (ln && ln < 1000000u && (lb = (uint8_t *)malloc((size_t)ln * 20)) != 0)
         {
            if (!fseek(mc_f, (long)lo, SEEK_SET) && fread(lb, 20, ln, mc_f) == ln && (mc_lp = (mc_loop_t *)malloc((size_t)ln * sizeof *mc_lp)) != 0)
            {
               for (i = 0; i < ln; i++)
               {
                  mc_lp[i].sa = mc_rd64(lb + i * 20); mc_lp[i].sb = mc_rd64(lb + i * 20 + 8);
                  mc_lp[i].hold = (uint16_t)(lb[i * 20 + 16] | (lb[i * 20 + 17] << 8));
               }
               mc_ln = ln;
            }
            free(lb);
         }
      }
   }
   return (int)n;
}
/* 순환 표에서 A 다음 — 없으면 0 */
static uint64_t mc_loop_next(uint64_t sa, unsigned *hold)
{
   int lo = 0, hi = (int)mc_ln - 1;
   while (lo <= hi)
   {
      int mid = (lo + hi) >> 1;
      if (mc_lp[mid].sa == sa) { *hold = mc_lp[mid].hold; return mc_lp[mid].sb; }
      if (mc_lp[mid].sa < sa) lo = mid + 1; else hi = mid - 1;
   }
   return 0;
}
/* 99f: 캐시에 (sa, sb) 항목이 있나 — 색인만 본다(읽지 않음) */
static int mc_has(uint64_t sa, uint64_t sb)
{
   int lo = 0, hi = (int)mc_n - 1;
   if (!mc_on || !mc_f || !mc_ix) return 0;
   while (lo <= hi)
   {
      int mid = (lo + hi) >> 1;
      const mc_idx_t *x = &mc_ix[mid];
      if (x->sa == sa && x->sb == sb) return 1;
      if (x->sa < sa || (x->sa == sa && x->sb < sb)) lo = mid + 1; else hi = mid - 1;
   }
   return 0;
}
static mc_ent *mc_get(uint64_t sa, uint64_t sb)
{
   int lo = 0, hi = (int)mc_n - 1, i, old = 0;
   uint8_t *z = 0, *raw = 0;
   uLongf rl;
   uint32_t rawlen, at, t;
   mc_ent *e;
   if (!mc_f || !mc_ix) return 0;
   mc_stats[0]++;
   for (i = 0; i < 8; i++)
      if (mc_c[i].buf && mc_c[i].sa == sa && mc_c[i].sb == sb) { mc_c[i].used = ++id_clock; return &mc_c[i]; }
   while (lo <= hi)
   {
      int mid = (lo + hi) >> 1;
      const mc_idx_t *x = &mc_ix[mid];
      if (x->sa == sa && x->sb == sb) { lo = mid; break; }
      if (x->sa < sa || (x->sa == sa && x->sb < sb)) lo = mid + 1; else hi = mid - 1;
   }
   if (lo > hi || lo >= (int)mc_n || mc_ix[lo].sa != sa || mc_ix[lo].sb != sb) return 0;
   mc_stats[1]++;
   if (mc_ix[lo].len < 8 || mc_ix[lo].len > (1u << 22) || fseek(mc_f, (long)mc_ix[lo].off, SEEK_SET)) return 0;
   z = (uint8_t *)malloc(mc_ix[lo].len);
   if (!z || fread(z, 1, mc_ix[lo].len, mc_f) != mc_ix[lo].len) { free(z); return 0; }
   rawlen = mc_rd32(z);
   if (rawlen < 10 || rawlen > (1u << 22) || !(raw = (uint8_t *)malloc(rawlen))) { free(z); return 0; }
   rl = rawlen;
   if (uncompress(raw, &rl, z + 4, mc_ix[lo].len - 4) != Z_OK || rl != rawlen) { free(z); free(raw); return 0; }
   free(z);
   for (i = 1; i < 8; i++) if (mc_c[i].used < mc_c[old].used) old = i;
   e = &mc_c[old];
   free(e->buf);
   memset(e, 0, sizeof *e);
   e->sa = sa; e->sb = sb; e->used = ++id_clock;
   e->cx = (int8_t)raw[0]; e->cy = (int8_t)raw[1]; e->w = raw[2]; e->h = raw[3];
   e->bdx = (int8_t)raw[4]; e->bdy = (int8_t)raw[5]; e->aw = raw[6]; e->ah = raw[7]; e->bw = raw[8]; e->bh = raw[9];
   at = 10;
   for (t = 0; t < 3; t++)
   {
      uint32_t n, rb = ((uint32_t)e->w * e->h + 7) >> 3;
      if (at + 2 > rawlen) { free(raw); e->buf = 0; return 0; }
      n = (uint32_t)raw[at] | ((uint32_t)raw[at + 1] << 8);
      e->ro[t] = at + 2; e->mo[t] = e->ro[t] + rb; e->fo[t] = e->mo[t] + ((n + 7) >> 3);
      at = e->fo[t] + n * 2;
      if (at > rawlen) { free(raw); e->buf = 0; return 0; }
      {  /* 칸 비트 수 = n 이어야(패치 99c) — 아니면 mc_draw 가 이동값 뒤를 넘어 읽는다(일부러 만든 파일만 — zlib 검사합이 우연한 깨짐은 막는다) */
         uint32_t c, cnt = 0, nb = (uint32_t)e->w * e->h;
         for (c = 0; c < nb; c++) cnt += (raw[e->ro[t] + (c >> 3)] >> (c & 7)) & 1;
         if (cnt != n) { free(raw); e->buf = 0; return 0; }
      }
   }
   e->buf = raw; e->blen = rawlen;
   return e;
}
static int mc_fdiv(int a) { return a >= 0 ? a / ID_S : -((-a + ID_S - 1) / ID_S); }
/* 캐시 사이 그림(4배 틀) — 칸마다 저장된 이동으로 A 나 B 의 그림 화소(팔레트·색 번호)를 그대로 가져온다 */
static void mc_draw(const mc_ent *e, int ti, const id_pose *A, const id_pose *B, uint16_t *out)
{
   int W4 = e->w * ID_S, x, y, a, b, idx = 0;
   uint8_t cfx = 0, cfy = 0;                       /* 이동은 앞 칸과의 차로 저장 — 누적(mod 256) */
   const uint8_t *reg = e->buf + e->ro[ti], *msk = e->buf + e->mo[ti];
   const uint8_t *fl = e->buf + e->fo[ti];
   for (y = 0; y < e->h; y++)
      for (x = 0; x < e->w; x++)
      {
         int c = y * e->w + x, i, useA, fx, fy, ox, oy;
         const id_pose *P;
         if (!((reg[c >> 3] >> (c & 7)) & 1))
         {
            for (b = 0; b < ID_S; b++) for (a = 0; a < ID_S; a++) out[(y * ID_S + b) * W4 + x * ID_S + a] = ID_NONE;
            continue;
         }
         i = idx++;
         useA = (msk[i >> 3] >> (i & 7)) & 1;
         cfx = (uint8_t)(cfx + fl[i * 2]); cfy = (uint8_t)(cfy + fl[i * 2 + 1]);
         fx = (int8_t)cfx; fy = (int8_t)cfy;
         P = useA ? A : B;
         ox = useA ? e->cx : e->cx - e->bdx; oy = useA ? e->cy : e->cy - e->bdy;     /* 틀 → P 그림 좌표 */
         for (b = 0; b < ID_S; b++)
            for (a = 0; a < ID_S; a++)
            {
               int sx = mc_fdiv(x * ID_S + a + fx) + ox, sy = mc_fdiv(y * ID_S + b + fy) + oy;
               uint16_t v = ID_NONE;
               if (sx >= 0 && sy >= 0 && sx < P->w && sy < P->h) v = P->c[sy * P->w + sx];
               out[(y * ID_S + b) * W4 + x * ID_S + a] = v;
            }
      }
}
/* (main) 캐시 사이 그림의 떨어진 작은 조각 지우기. 칸마다 A/B 를 고르는 캐시라, 한쪽에만 있는 칼·팔이 이웃 칸과 다른 쪽을 골라
   몸에서 떨어진 채 남는다(검수: 옛 칼끝·새 칼끝만 먼저 온 것·한 칸 티끌·떠 있는 팔뚝 — 148 띠 중 14). A·B 둘 다 그런 작은 덩이가 없는
   그림일 때만(속도선·떨어진 이펙트가 원래 그림에 있으면 손대지 않는다), MC_SPECK 칸 미만이고 가장 큰 덩이의 절반 미만인 덩이를 비운다.
   비운 자리는 몸 뺀 줄(배경·다른 스프라이트)이 보인다. 반환 = 지운 칸 수 */
#define MC_SPECK 16
/* (main) 캐시 사이 그림(¼·½·¾ 석 장)은 머무는 길이가 8 프레임을 넘으면 마지막 8 프레임에만 편다 — 고르기 화면 미리보기의 34·14 프레임
   서기에 석 장을 펴면 한 장이 6~19 프레임 머물러 게임에 없는 자세로 보였다(검수). 걷기 한 걸음(6)은 그대로. 길이를 여기서 잰 것이거나
   미리 본 프레임으로 바뀜이 보일 때만 — 캐시 순환 표의 길이만 있을 때는(같은 걷기가 14·18, 실제 6) 24 프레임 이상일 때만 */
#define MC_WIN 8
#define MC_LONG 24
static int mc_despeckle(uint16_t *out, int w, int h, const id_pose *A, const id_pose *B)
{
   static uint8_t occ[ID_W * ID_H];
   int W4 = w * ID_S, x, y, a, b, n, i, big = 0, gone = 0;
   if (A->mincc < MC_SPECK || B->mincc < MC_SPECK) return 0;
   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
      {
         int any = 0;
         for (b = 0; b < ID_S && !any; b++) for (a = 0; a < ID_S; a++) if (out[(y * ID_S + b) * W4 + x * ID_S + a] != ID_NONE) { any = 1; break; }
         occ[y * w + x] = (uint8_t)any;
      }
   n = id_cc_label(occ, w, h);
   if (n < 2) return 0;
   for (i = 1; i <= n; i++) if (id_cc_sz[i] > big) big = id_cc_sz[i];
   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
      {
         int l = id_cc_lab[y * w + x];
         if (!l || id_cc_sz[l] >= MC_SPECK || id_cc_sz[l] * 2 >= big) continue;
         for (b = 0; b < ID_S; b++) for (a = 0; a < ID_S; a++) out[(y * ID_S + b) * W4 + x * ID_S + a] = ID_NONE;
         gone++;
      }
   return gone;
}

/* 몸 p 의 그림 왼쪽 위(몸 조각 최소 − 물체 자리) — 포즈 그림(id_capture)과 같은 셈. 몸이 없으면 0 */
static int mc_top_left(const ss2fg_frame *f, int p, int *x, int *y)
{
   uint8_t m[64];
   int k, x0 = 9999, y0 = 9999;
   if (!f || !f->body_ok || !body_mask(f, p, m)) return 0;
   for (k = 0; k < 64; k++)
      if (m[k])
      {
         int sx = wrapx(f->ax[k] + f->line[0].spx) - f->ob_x[p], sy = wrapc(f->ay[k] + f->line[0].spy) - f->ob_y[p];
         if (sx < x0) x0 = sx;
         if (sy < y0) y0 = sy;
      }
   if (x0 == 9999) return 0;
   *x = x0; *y = y0;
   return 1;
}
/* 출력 칸 slot 의 몸 p — D+1·D+2 에서 포즈가 바뀌면(제자리) 캐시로. 1 = 계획함 */
static void id_realign(const ss2fg_frame *f, int p, uint64_t sg);
static int mc_plan(const ss2fg_frame *f0, const ss2fg_frame *f1, const ss2fg_frame *f2, unsigned realn, int h, int slot, int p)
{
   uint8_t mm[64];
   uint64_t sb = 0;
   int k, K = 0, L, num, ti;
   const ss2fg_frame *fk = 0;
   mc_ent *e;
   id_pose *A, *B;
   id_req_t *rq = &id_rq[slot][p];
   if (!mc_on || !mc_f || !id_cur[p]) return 0;
   for (k = 1; k <= 2; k++)
   {
      const ss2fg_frame *g = k == 1 ? f1 : f2;
      uint64_t sg;
      if (!g || !g->valid || g->mono || !g->body_ok || !body_mask(g, p, mm)) return 0;
      sg = body_sig(g, mm);
      if (sg != id_cur[p]) { sb = sg; K = k; fk = g; break; }
   }
   /* 되풀이 동작(배운 순환 — 걷기 등): 다음 포즈를 미리 알면 사이 그림을 한 걸음 내내 고르게 편다(패치 99).
      바뀌기 직전 2 프레임에만 몰면, 걷기는 몸은 120Hz 로 매끄럽게 가는데 다리는 6 프레임 멈춰 미끄러지듯 보였다
      (유저 「앞으로 올 때 슬라이딩하는 것처럼 보일 때가 있다」). t = 지난 시간/길이 → ¼·½·¾ 중 가까운 칸(⅛ 전엔 A 그대로).
      미리 돌린 D+1·D+2 가 다른 포즈면 순환이 아닌 것으로 — 아래 «마지막 2 프레임» 으로 그 포즈를 향한다 */
   {
      unsigned hold = 0;
      uint64_t nb = id_next(p, id_cur[p], id_prev[p], &hold);
      int loop = nb && hold >= 3 && hold <= 60 && id_loop(p, id_cur[p], nb) && (!K || sb == nb) && !id_back_doubt(p, id_cur[p], id_prev[p], nb);
      int measured = loop;                /* (main) 길이를 믿을 만함 = 배운 순환(id_loop)으로 확인됨. 캐시 표의 길이(같은 걷기가 14·18, 실제 6)나
                                             순환 확인 없이 배운 길이(다른 장면에서 그 바뀜을 본 길이 — 걷기 첫 걸음에 18)는 못 믿는다 */
      if (!loop && mc_ln)
      {  /* 아직 못 배웠으면 캐시의 순환 표로(패치 99b). (main) 표는 포즈마다 다음 하나뿐이라 왕복 동작의 가운데 포즈에선
            방향을 모른다 — 온 쪽으로 되돌아가는 예측은 펴지 않는다(미리 본 프레임에 보이면 아래 «마지막 2 프레임») */
         unsigned h2 = 0;
         uint64_t cb = mc_loop_next(id_cur[p], &h2);
         if (cb && cb == nb && hold >= 3) h2 = hold;   /* (main) 여기서 잰 길이가 있으면 그것 — 표의 길이는 같은 걷기도 14·18 (실제 6) */
         if (cb && h2 >= 3 && h2 <= 60 && (!K || sb == cb) && cb != id_prev[p]) { nb = cb; hold = h2; loop = 1; }
      }
      if (loop && (K ? (int)(realn + (unsigned)K - id_start[p]) > (int)hold + 2 : 2 * (int)(realn - id_start[p]) + h + 6 > 2 * (int)hold + 4))
      {  /* 99e: 배운 길이보다 오래 머묾 — 순환이 아니라 붙잡은 자세. 바뀌기 직전 2 프레임만(아래).
            (main) 미리 본 프레임에 바뀜이 보이면 실제 길이(지난 + K)로 판단 — 99e 식은 길이가 배운 것과 똑같아도 마지막 반 프레임에
            걸려 «2 프레임» 쪽으로 넘어가며 ¾ 에서 ¼ 로 되돌아가기도 했다 */
         loop = 0;
         if (!K) mc_loopstop++;        /* (main 계기) 확인 안 된 순환 예측을 그만둔 수 */
      }
      if (!K && !loop) return 0;
      if (loop)
      {
         int e2 = 2 * (int)(realn - id_start[p]) + h, tot2;
         if (!K) sb = nb;
         tot2 = K ? 2 * (int)(realn + (unsigned)K - id_start[p]) : 2 * (int)hold;
         if (!K && tot2 < e2 + 6) tot2 = e2 + 6;                                 /* D+2 까진 안 바뀜 — 적어도 3 프레임 남음 */
         if (tot2 <= 0 || e2 < 0) return 0;
         if (tot2 > 2 * MC_WIN && (K || measured || tot2 >= 2 * MC_LONG)) { e2 -= tot2 - 2 * MC_WIN; tot2 = 2 * MC_WIN; if (e2 <= 0) return 0; }
         ti = (8 * e2 + tot2) / (2 * tot2) - 1;                                   /* round(4t) − 1 */
         if (ti < 0) return 0;
         if (ti > 2) ti = 2;
         if (!K && ti > 1) ti = 1;   /* (main) 아직 미리 본 프레임으로 확인 못 한 예측은 ½ 까지만 — 틀리면(다른 갈래로 감) 반쯤 간 자세에서 돌아온다
                                        (검수 s024~s026: 오지 않을 포즈로 ¾ 까지 갔다가 한 번에 되돌아와 팔이 내려-올라-내려). 걷기(한 걸음 6 프레임)는
                                        확인 전 구간이 원래 ½ 를 안 넘어 그대로 */
      }
      else
      {
         L = (int)(realn + (unsigned)K - id_start[p]);                            /* A 가 보이는 프레임 수(바뀔 때까지) */
         if (L > 2) L = 2;
         if (L < 1) return 0;
         num = 2 * L - (2 * K - h);                                               /* t = num / (2L) */
         if (num <= 0) return 0;
         ti = L == 2 ? num - 1 : 1;                                               /* ¼·½·¾ 칸 */
         if (ti < 0 || ti > 2) return 0;
      }
   }
   e = mc_get(id_cur[p], sb);
   if (!e) { mc_miss_push(p, id_cur[p], sb); return 0; }
   A = id_find_pose(p, id_cur[p]);
   B = id_find_pose(p, sb);
   if (!B && fk) { id_capture(fk, p, mm, sb); B = id_find_pose(p, sb); }
   else if (B && fk) id_realign(fk, p, sb);                    /* 뒤 포즈가 다른 장면에서 잡힌 것이면 지금 것으로(99e) */
   else if (A && B && B->depth != A->depth) B->depth = A->depth; /* 아직 안 보인 뒤 포즈의 우선순위는 지금 장면(A) 것으로(99e) */
   if (A && !B && !fk) { mc_bunk++; mc_stats[3]++; return 0; }   /* (main 계기) 되풀이 길의 다음 포즈를 아직 못 봤거나 잊음 — 캐시엔 그림이 없다 */
   if (!A || !B || A->depth != B->depth || A->w != e->aw || A->h != e->ah || B->w != e->bw || B->h != e->bh) { mc_stats[3]++; return 0; }
   {  /* 자리(패치 99c) — 전엔 기억한 두 포즈의 «물체 자리 기준 왼쪽 위» 차가 캐시의 B 자리(bdx·bdy)와 한 픽셀이라도 다르면 버렸다.
         물체 자리(RAM)와 스프라이트가 움직일 때 한 박자 어긋나 같은 포즈라도 잡힌 때마다 1~3 픽셀 달라, 바뀜의 10% 가 사이 그림 없이 툭 바뀌었다(검수 2026-10-11).
         이제 A 는 지금 D 의 실제 자리에 놓고, B 는 D+K 의 실제 자리(K 를 모르는 되풀이 동작은 기억한 자리)와 캐시 자리의 차를 t 만큼 나눠 밀어 메운다.
         차가 4 픽셀을 넘으면 다른 상황으로 보고 예전처럼 툭 */
      int ax0, ay0, bx0, by0, rbx, rby, dx, dy;
      if (!mc_top_left(f0, p, &ax0, &ay0)) { ax0 = A->ox; ay0 = A->oy; }
      if (fk && mc_top_left(fk, p, &bx0, &by0)) { rbx = bx0 - ax0; rby = by0 - ay0; }
      else { rbx = B->ox - A->ox; rby = B->oy - A->oy; }
      dx = rbx - e->bdx; dy = rby - e->bdy;
      if (dx < -4 || dx > 4 || dy < -4 || dy > 4) { mc_stats[3]++; return 0; }
      rq->gx = ax0 + e->cx; rq->gy = ay0 + e->cy;
      rq->sx4 = (ti + 1) * dx; rq->sy4 = (ti + 1) * dy;   /* t = (ti+1)/4 → 4배 화소로 t·차·4 */
   }
   rq->on = 1; rq->kind = 1; rq->pr = 0; rq->t256 = 64 * (ti + 1);
   rq->e = e; rq->ti = ti; rq->A = A; rq->B = B;
   rq->ax = f0->ob_x[p]; rq->ay = f0->ob_y[p];              /* 몸 자리는 보여 줄 D 의 것 — 움직이는 몸은 그리기에서 몸 조각의 옮김만큼 더 민다(패치 98) */
   rq->gw = e->w; rq->gh = e->h;
   rq->depth = A->depth; rq->first = A->first < B->first ? A->first : B->first;
   return 1;
}

/* 실제 프레임마다(보여 줄 D) — 포즈 그림 기억 + 제자리 바뀜 배우기 */
/* 포즈 맞춤(패치 99e) — 같은 포즈(지문)라도 장면·동작마다 물체 자리(RAM X·Y)와 그림 사이(왼쪽 위)와 스프라이트 우선순위가 다르다
   (고르기 화면 미리보기·판 시작 «승부»·대전·공격 끝 자세 등). 기억한 포즈는 처음 잡은 장면 것을 계속 써서
   ① 다른 장면에서 배운 서기 사이 그림이 그만큼 옆으로 그려졌다 — 조각 틀 밖에 남은 진짜 몸과 겹쳐 «몸이 둘»
   ② 앞·뒤 포즈의 우선순위가 달라 보여(한쪽은 이 장면, 한쪽은 다른 장면 것) 캐시·배운 바뀜 모두 «안 맞음»으로 건너뜀 — 고르기 화면에서 되다 안 되다
   (유저 2026-10-12 「뉴트럴에서 … 이러기도 함」·「캐릭 선택창에는 적용이 되다 안 되다」). 지금 몸 조각에서 다시 재 기억을 고친다 —
   그 포즈가 든 배운 바뀜은 id_get_pair 가 자리·우선순위가 다른 걸 보고 다시 만든다. */
static void id_realign(const ss2fg_frame *f, int p, uint64_t sg)
{
   id_pose *P = id_find_pose(p, sg);
   uint8_t m[64];
   int x, y, k, depth = -1;
   if (!P || !body_mask(f, p, m) || !mc_top_left(f, p, &x, &y)) return;
   for (k = 0; k < 64; k++)
      if (m[k])
      {
         int pr = (ld16(f->spr + k * 4) & 0x1800) >> 11;
         if (depth < 0) depth = pr << 1;
         else if ((pr << 1) != depth) return;                    /* 우선순위가 섞인 몸 — 그대로 둔다 */
      }
   /* 가장 앞 슬롯(first)은 프레임마다 바뀌어(스프라이트 칸 배정) 맞추지 않는다 — 자리·우선순위만 */
   if (depth <= 0 || (x == P->ox && y == P->oy && depth == P->depth)) return;
   P->ox = x; P->oy = y; P->depth = (uint8_t)depth;
}

void ss2fg_idle_observe(const ss2fg_frame *f, unsigned realn)
{
   int p;
   if (!id_on || !f || !f->valid || f->mono) return;
   for (p = 0; p < 2; p++)
   {
      uint8_t m[64];
      uint64_t sg;
      int ax, ay;
      if (!body_mask(f, p, m)) { id_cur[p] = id_prev[p] = 0; continue; }
      sg = body_sig(f, m);
      ax = f->ob_x[p]; ay = f->ob_y[p];
      if (!id_find_pose(p, sg)) id_capture(f, p, m, sg);
      else id_realign(f, p, sg);
      if (sg == id_cur[p]) { if (ax != id_ax[p] || ay != id_ay[p]) id_still[p] = 0; continue; }
      if (id_cur[p])
      {  /* 움직이는 바뀜도 배운다(패치 99) — 걷기 순환을 알아야 사이 그림을 한 걸음 내내 펼 수 있다. 서기 그리기(±3)는 여전히 제자리만 */
         unsigned hold = realn - id_start[p];
         if (hold >= 3 && hold <= 60) id_learn_add(p, id_cur[p], sg, hold);
         id_prev[p] = id_cur[p];
      }
      else id_prev[p] = 0;
      id_cur[p] = sg; id_start[p] = realn; id_ax[p] = ax; id_ay[p] = ay; id_still[p] = 1;
   }
}

static int id_plan_learned(const ss2fg_frame *f0, const ss2fg_frame *f1, const ss2fg_frame *f2, unsigned realn, int h, int slot, int p);
/* 출력 칸 slot(0 = τD, 1 = τD+½, h = 반 프레임) 의 사이 그림 계획. f1·f2 = 미리 돌린 D+1·D+2. 반환 1 = 그릴 몸이 있음 */
int ss2fg_idle_plan(const ss2fg_frame *f0, const ss2fg_frame *f1, const ss2fg_frame *f2, unsigned realn, int h, int slot)
{
   int p, any = 0;
   if (slot < 0 || slot > 1) return 0;
   for (p = 0; p < 2; p++) { id_rq[slot][p].on = 0; id_pt[slot][p].on = 0; }
   if (!id_on || !f0 || !f0->body_ok) return 0;
   for (p = 0; p < 2; p++)
   {
      if (id_plan_learned(f0, f1, f2, realn, h, slot, p) || mc_plan(f0, f1, f2, realn, h, slot, p)) any = 1;
   }
   return any;
}

/* 서기(배운 되풀이 바뀜) — 지난 시간/길이로 t, ±3px 이동표(id_pair) */
static int id_plan_learned(const ss2fg_frame *f0, const ss2fg_frame *f1, const ss2fg_frame *f2, unsigned realn, int h, int slot, int p)
{
   {
      unsigned hold = 0;
      uint64_t sb;
      int k, K = 0, bad = 0, e2, tot2, t256, doubt;
      id_pair *pr;
      id_req_t *rq = &id_rq[slot][p];
      if (!id_cur[p] || !id_still[p] || f0->ob_x[p] != id_ax[p] || f0->ob_y[p] != id_ay[p]) return 0;
      sb = id_next(p, id_cur[p], id_prev[p], &hold);
      if (!sb || !id_find_pose(p, sb)) return 0;
      doubt = id_back_doubt(p, id_cur[p], id_prev[p], sb);
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
      if (bad) { id_stats[3]++; return 0; }
      if (doubt && !K) return 0;                        /* (main) 온 쪽으로 되돌아감 예측 — 미리 본 프레임으로 확인될 때만(그때는 마지막 2 프레임) */
      if (K) id_realign(K == 1 ? f1 : f2, p, sb);      /* 뒤 포즈도 지금 장면 것으로(99e) */
      else
      {  /* 아직 안 보인 뒤 포즈 — 우선순위는 지금 장면(A) 것으로 본다(다른 장면에서 잡힌 값이면 바뀜을 못 만든다). 보이면 id_realign 이 고친다 */
         id_pose *QA = id_find_pose(p, id_cur[p]), *QB = id_find_pose(p, sb);
         if (QA && QB && QB->depth != QA->depth) QB->depth = QA->depth;
      }
      e2 = 2 * (int)(realn - id_start[p]) + h;
      /* 99e: 배운 길이보다 2 프레임 넘게 머물면 붙잡은 자세(앉아 방어·서서 막기 등) — 예전엔 «적어도 3 프레임 남음»으로 길이를 늘려
         t 가 1 쪽으로 다가가, 자세를 쥐고 있는 내내 다음 포즈 쪽으로 ¾ 쯤 바뀐 그림이 나왔다(유저 2026-10-12 「앉아 방어 자세에서 글리치」).
         그동안은 그리지 않고, 실제로 바뀔 때는(미리 돌린 D+1·D+2 에 보이면) 바뀌기 직전 2 프레임에만 */
      if (doubt || (K ? (int)(realn + (unsigned)K - id_start[p]) > (int)hold + 2 : e2 + 6 > 2 * (int)hold + 4))
      {  /* (main) 바뀜이 보이면 실제 길이로 — 99e 식은 길이가 배운 것과 같아도 마지막 반 프레임에 걸려 t 가 0.94 → 0.75 로 되돌아갔다 */
         if (!K) return 0;
         tot2 = 2 * (int)(realn + K - id_start[p]);
         e2 -= tot2 - 4; tot2 = 4;
         if (e2 <= 0) return 0;
      }
      else if (K) tot2 = 2 * (int)(realn + K - id_start[p]);
      else { tot2 = 2 * (int)hold; if (tot2 < e2 + 6) tot2 = e2 + 6; }      /* D+2 까진 안 바뀜 — 적어도 3 프레임 남음 */
      if (tot2 <= 0) return 0;
      t256 = 256 * e2 / tot2;
      if (t256 <= 0) return 0;
      if (t256 > 255) t256 = 255;
      pr = id_get_pair(p, id_cur[p], sb);
      if (!pr) return 0;
      /* 큰 바뀜(사이 그림이 반쯤에서 뒤 그림으로 넘어감)은 되풀이 동작(서기)일 때만 — 착지→서기·공격 끝 자세→서기처럼 한 번 지나가는
         바뀜이면 그 자세가 끝나는 때가 앞당겨 보인다 */
      if (!pr->small && !id_loop(p, id_cur[p], sb)) return 0;
      if (pr->bigp && mc_has(id_cur[p], sb)) return 0;           /* 99f: 가는 부분이 멀리 가는 바뀜 — 캐시 사이 그림(칼이 이어짐)으로 */
      rq->on = 1; rq->kind = 0; rq->pr = pr; rq->t256 = t256; rq->sx4 = rq->sy4 = 0;
      rq->ax = id_ax[p]; rq->ay = id_ay[p];
      rq->gx = pr->ox; rq->gy = pr->oy; rq->gw = pr->w; rq->gh = pr->h; rq->depth = pr->depth; rq->first = pr->first;
      id_stats[1]++;
      return 1;
   }
}

/* ss2fg_render2 끝에서 — 이 출력 칸의 몸마다 «몸 뺀 줄 + 사이 그림» 4배 조각 */
static void id_build(const ss2fg_frame *base, const ss2fg_frame *to_scr, int t_scr,
                     const int16_t *offx, const int16_t *offy)
{
   int s = id_slot, p;
   if (s < 0 || s > 1 || !id_on) return;
   for (p = 0; p < 2; p++)
   {
      id_req_t *rq = &id_rq[s][p];
      id_patch_t *pt = &id_pt[s][p];
      uint8_t m[64], zrow[256];
      int16_t srow[256], brow[256];
      uint16_t scan[256], bscan[256];
      uint8_t minv[64], bz[256];
      int k, ux0, uy0, x0, y0, x1, y1, y, x, sx, sy, W4, mvx, mvy, sh4x = 0, sh4y = 0;
      pt->on = 0;
      if (!rq->on || pp_on || fx_fade_on) continue;           /* 포즈·이펙트 섞기 중엔 몸 뺀 줄을 같게 못 만든다 */
      if (!body_mask(base, p, m)) continue;
      {  /* 몸 조각이 옮겨 그려지는 만큼(보간 자리) — 모두 같아야. 서기(배운 바뀜)는 제자리만, 캐시는 움직이는 몸도(걷기·점프 등, 패치 98) */
         int seen = 0, same = 1;
         mvx = mvy = 0;
         for (k = 0; k < 64; k++)
            if (m[k])
            {
               int dx = offx ? offx[k] : 0, dy = offy ? offy[k] : 0;
               if (!seen) { mvx = dx; mvy = dy; seen = 1; }
               else if (dx != mvx || dy != mvy) { same = 0; break; }
            }
         if (!same || (rq->kind == 0 && (mvx || mvy))) continue;
      }
      if (rq->gw * ID_S * rq->gh * ID_S > (int)(sizeof id_tc / sizeof id_tc[0])) continue;
      if (rq->kind == 1) { mc_draw(rq->e, rq->ti, rq->A, rq->B, id_tc); mc_stats[2]++; mc_speck += mc_despeckle(id_tc, rq->gw, rq->gh, rq->A, rq->B); }
      else id_draw(rq->pr, rq->t256, id_tc);
      W4 = rq->gw * ID_S;
      ux0 = rq->ax + rq->gx + mvx; uy0 = rq->ay + rq->gy + mvy;
      {  /* 캐시 사이 그림의 4배 밀기(패치 99c) — 틀이 그만큼 한두 픽셀 넓어진다. 조각 버퍼를 넘으면 밀지 않는다 */
         int s4x = rq->sx4, s4y = rq->sy4;
         int ex0 = s4x < 0 ? -((-s4x + ID_S - 1) / ID_S) : 0, ex1 = s4x > 0 ? (s4x + ID_S - 1) / ID_S : 0;
         int ey0 = s4y < 0 ? -((-s4y + ID_S - 1) / ID_S) : 0, ey1 = s4y > 0 ? (s4y + ID_S - 1) / ID_S : 0;
         if ((rq->gw + ex1 - ex0) * (rq->gh + ey1 - ey0) > ID_W * ID_H) { s4x = s4y = 0; ex0 = ex1 = ey0 = ey1 = 0; }
         sh4x = s4x; sh4y = s4y;
         x0 = ux0 + ex0 < 0 ? 0 : ux0 + ex0; y0 = uy0 + ey0 < 0 ? 0 : uy0 + ey0;
         x1 = ux0 + rq->gw + ex1 > SS2FG_W ? SS2FG_W : ux0 + rq->gw + ex1;
         y1 = uy0 + rq->gh + ey1 > SS2FG_H ? SS2FG_H : uy0 + rq->gh + ey1;
      }
      if (x1 <= x0 || y1 <= y0) continue;
      for (k = 0; k < 64; k++) minv[k] = (uint8_t)!m[k];
      for (y = y0; y < y1; y++)
      {
         const ss2fg_regs *rb = &base->line[y];
         int in_win = (y >= rb->winy) && (y < rb->winy + rb->winh);
         int wx0 = rb->winx, wx1 = imin(rb->winx + rb->winw, SS2FG_W);
         if (rq->kind == 1)
         {  /* (main) 지금 프레임(D)에서 몸 조각만 그린 줄 — 화소마다 그 자리를 칠한 몸 조각의 슬롯(brow, 없으면 −1) */
            rl_skip = minv; rl_zout = bz; rl_slot = brow;
            render_line(base, to_scr, offx, offy, t_scr, y, bscan);
         }
         rl_skip = m; rl_zout = zrow; rl_slot = srow;
         render_line(base, to_scr, offx, offy, t_scr, y, scan);
         for (sy = 0; sy < ID_S; sy++)
         {
            uint16_t *dst = pt->px + ((y - y0) * ID_S + sy) * ((x1 - x0) * ID_S);
            int ty = (y - uy0) * ID_S + sy - sh4y;
            const uint16_t *tw = (ty >= 0 && ty < rq->gh * ID_S) ? id_tc + ty * W4 : 0;
            for (x = x0; x < x1; x++)
               for (sx = 0; sx < ID_S; sx++)
               {
                  int tx = (x - ux0) * ID_S + sx - sh4x;
                  uint16_t c = scan[x], v = (tw && tx >= 0 && tx < W4) ? tw[tx] : ID_NONE;
                  /* (main) 캐시 그림은 지금 프레임(D)에서 그 화소를 칠한 몸 조각의 슬롯으로 겹친다 — 몸 단위 first(가장 앞 슬롯)로 보면 그보다
                     뒤지만 그 자리 몸 조각보다는 앞인 상대 발·창·타격 불꽃을 덮었다(검수 s055·s105·s108). D 의 몸이 없는 자리(B 에서 새로 생긴
                     부분)는 예전대로 first. 포즈를 잡은 옛 프레임의 슬롯은 쓰지 않는다 — 슬롯 순서가 프레임마다 달라 발바닥이 그림자 뒤로 가는 등
                     오히려 틀렸다(2차 검수 22곳). 서기 그림(kind 0)은 예전대로 first */
                  int vs = (rq->kind == 1 && brow[x] >= 0) ? brow[x] : rq->first;
                  if (v != ID_NONE && in_win && x >= wx0 && x < wx1
                      && (rq->depth > zrow[x] || (rq->depth == zrow[x] && srow[x] > vs)))
                  {
                     uint16_t col = ld16(base->pal + ((v >> 2) << 3) + ((v & 3) << 1)) & 0x0FFF;   /* 번호 → 그 프레임 팔레트(패치 96) */
                     c = rb->neg ? (uint16_t)~col : col;
                  }
                  *dst++ = (uint16_t)(c & 4095);                   /* 12비트 그대로 — compose 가 화면 형식(16/32비트)의 색표로 바꾼다 */
               }
         }
      }
      rl_skip = 0; rl_zout = 0; rl_slot = 0;
      pt->x0 = x0; pt->y0 = y0; pt->w = x1 - x0; pt->h = y1 - y0; pt->on = 1;
      id_stats[2]++;
   }
}

/* 내보내기 — 원래 그림(화면 형식 그대로, bpp 2 또는 4)을 4배로 늘리고 출력 칸 slot 의 조각을 색표를 거쳐 붙인다.
   dpitch·spitch 는 화소 단위. 코어판은 16비트, 앱은 화면에 따라 16/32비트라 둘 다 받는다 */
#define ID_COMPOSE(T)                                                                                     \
   {                                                                                                      \
      T *dst = (T *)dstv; const T *src = (const T *)srcv;                                                 \
      for (y = 0; y < h; y++)                                                                             \
      {                                                                                                   \
         T *row = dst + (size_t)(y * ID_S) * dpitch;                                                      \
         const T *s = src + (size_t)y * spitch;                                                           \
         for (x = 0; x < w; x++)                                                                          \
         {                                                                                                \
            T v = s[x];                                                                                   \
            row[x * 4] = row[x * 4 + 1] = row[x * 4 + 2] = row[x * 4 + 3] = v;                            \
         }                                                                                                \
         for (sy = 1; sy < ID_S; sy++) memcpy(row + (size_t)sy * dpitch, row, (size_t)w * ID_S * sizeof(T)); \
      }                                                                                                   \
      if (slot >= 0 && slot <= 1)                                                                         \
         for (p = 0; p < 2; p++)                                                                          \
         {                                                                                                \
            const id_patch_t *pt = &id_pt[slot][p];                                                       \
            int pw = pt->w * ID_S, r, c;                                                                  \
            if (!pt->on || pt->x0 + pt->w > w || pt->y0 + pt->h > h) continue;                            \
            for (r = 0; r < pt->h * ID_S; r++)                                                            \
            {                                                                                             \
               T *o = dst + (size_t)(pt->y0 * ID_S + r) * dpitch + pt->x0 * ID_S;                         \
               const uint16_t *q = pt->px + (size_t)r * pw;                                               \
               for (c = 0; c < pw; c++) o[c] = (T)colormap[q[c] & 4095];                                  \
            }                                                                                             \
         }                                                                                                \
   }
void ss2fg_idle_compose(void *dstv, int dpitch, const void *srcv, int spitch, int w, int h, int bpp, const uint32_t *colormap, int slot)
{
   int y, x, sy, p;
   if (bpp == 4) ID_COMPOSE(uint32_t)
   else          ID_COMPOSE(uint16_t)
}
#undef ID_COMPOSE

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
   id_build(base, to_scr, t_scr, have_off ? offx : 0, offy);   /* 서기 사이 그림 조각(패치 95) — 12비트 색, 16/32비트 화면 모두 */
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
