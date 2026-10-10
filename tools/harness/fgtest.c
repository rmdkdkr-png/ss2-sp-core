/* fgtest — 프레임 생성 렌더러(ss2fg) 검증. 롬 없이 돈다.
 *
 * mednafen gfx.c 의 원본 렌더러(ngpgfx_draw — 안에 ss2fg 캡처 훅이 들어 있다)로
 * K2GE 상태를 그리면 그 프레임이 ss2fg 링에 잡힌다. 그 뒤 ss2fg_render 로 합성해 비교한다.
 *   1. 동치: 무작위 상태 A,B 에서 render(A,B,256) == 원본(B), render(B,B,128) == 원본(B)
 *   2. 중점: 스프라이트 (10,20)→(20,20) 이면 t=128 에 (15,20), t=0 에 A 자리
 *   3. 스냅: 타일·팔레트·우선순위가 다르면 보간 없이 B 자리
 *   4. 체인: 부모가 움직이면 체인 자식도 같이 보간(보이지 않는 앵커도)
 *   5. 스크롤 랩: 250→4 는 +10 → 중점 255, 임계 초과는 B
 *   6. 임계·가장자리: 90px 점프는 B, 왼쪽 가장자리 넘기는 연속
 *   7. 더티: 표시 중 스프라이트표 쓰기 → 합성 거부(0), 흑백 모드 거부
 *   8. pop: 예측 프레임 무르기
 *   9. 래스터: 줄마다 다른 스크롤값도 줄별로 보간
 *
 * 빌드 (build/ 에서):
 *   gcc -O1 -std=gnu99 -I. -Imednafen -Imednafen/ngp -Ilibretro-common/include -DWANT_16BPP \
 *       -DFRONTEND_SUPPORTS_RGB565 -DMEDNAFEN_VERSION_NUMERIC=931 -D__LIBRETRO__ -DINLINE=inline \
 *       -o fgtest ../tools/harness/fgtest.c mednafen/ngp/gfx.c ss2fg.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <boolean.h>
#include "neopop.h"
#include "gfx.h"
#include "../video.h"
#include "../../ss2fg.h"

/* gfx.c 가 링크에 요구하는 것들 — 시험에서는 빈 껍데기 */
ngpgfx_t *NGPGfx;
uint32_t timer_hint;
void neopop_reset(void) {}
void TestIntHDMA(int bit, int vec) { (void)bit; (void)vec; }
int MDFNSS_StateAction(void *st, int load, int data_only, void *sf, const char *name, int optional)
{ (void)st; (void)load; (void)data_only; (void)sf; (void)name; (void)optional; return 1; }

static uint32_t rng = 12345;
static unsigned rnd(unsigned n) { rng = rng * 1103515245u + 12345u; return (rng >> 16) % n; }

typedef void (*line_cb)(ngpgfx_t *g, int y, void *ud);

/* 원본 렌더러로 한 프레임 → out. 줄 콜백으로 래스터 효과 흉내. dirty 면 40줄째에 표시 중 쓰기 신호.
   ngpgfx_draw 안의 훅이 이 프레임을 ss2fg 링에 넣는다. */
static void frame(ngpgfx_t *g, uint16_t *out, line_cb cb, void *ud, int dirty)
{
   MDFN_Surface s; int y;
   memset(&s, 0, sizeof s);
   s.pixels = out; s.pitch = SCREEN_WIDTH; s.depth = 16;
   g->raster_line = 0;
   for (y = 0; y < SCREEN_HEIGHT; y++)
   {
      if (cb) cb(g, y, ud);
      if (dirty && y == 40) ss2fg_capture_write(dirty);
      ngpgfx_draw(g, &s, 0);
   }
}
/* 참고용 렌더 — 링에 남기지 않는다 */
static void frame_ref(ngpgfx_t *g, uint16_t *out, line_cb cb, void *ud)
{
   frame(g, out, cb, ud, 0);
   ss2fg_capture_pop();
}

static void base_state(ngpgfx_t *g)
{
   int i;
   memset(g, 0, sizeof *g);
   g->winx = 0; g->winw = 0xFF; g->winy = 0; g->winh = 0xFF;
   g->layer_enable = 7;
   ngpgfx_set_pixel_format(g, 16);   /* ColorMap */
   g->SCREEN_PERIOD = 0xC6;          /* 0 이면 ngpgfx_draw 가 줄 0 다음에 래스터를 되감는다 */
   /* 타일 1·2·3 = 단색(색인 1·2·3), 타일 4 = 왼쪽 반만 색인 1, 타일 5 = 대각선 */
   for (i = 0; i < 8; i++)
   {
      ((uint16_t*)g->CharacterRAM)[1*8 + i] = 0x5555;
      ((uint16_t*)g->CharacterRAM)[2*8 + i] = 0xAAAA;
      ((uint16_t*)g->CharacterRAM)[3*8 + i] = 0xFFFF;
      ((uint16_t*)g->CharacterRAM)[4*8 + i] = 0x5500;
      ((uint16_t*)g->CharacterRAM)[5*8 + i] = (uint16_t)(3 << ((7 - i) * 2));
   }
   /* 팔레트: 스프라이트 0..15, 플레인1 (0x80), 플레인2 (0x100) — 각 4색, 색인 0 은 0 */
   for (i = 0; i < 16 * 4; i++)
   {
      ((uint16_t*)g->ColorPaletteRAM)[i]        = (uint16_t)((i & 3) ? 0x111 * (i & 3) + (i >> 2) * 0x010 : 0);
      ((uint16_t*)g->ColorPaletteRAM)[0x40 + i] = (uint16_t)((i & 3) ? 0x222 * (i & 3) + (i >> 2) * 0x001 : 0);
      ((uint16_t*)g->ColorPaletteRAM)[0x80 + i] = (uint16_t)((i & 3) ? 0x333 * (i & 3) + (i >> 2) * 0x100 : 0);
   }
   ((uint16_t*)g->ColorPaletteRAM)[0xF0] = 0x000;   /* 배경 */
   ((uint16_t*)g->ColorPaletteRAM)[0xF8] = 0x888;   /* 창밖 */
}

static void put_sprite(ngpgfx_t *g, int i, unsigned tile, int x, int y, unsigned prio, unsigned flags, unsigned pal)
{
   uint16_t d = (uint16_t)((tile & 0x1FF) | ((prio & 3) << 11) | flags);
   g->SpriteVRAM[i*4+0] = d & 0xFF; g->SpriteVRAM[i*4+1] = d >> 8;
   g->SpriteVRAM[i*4+2] = (uint8_t)x; g->SpriteVRAM[i*4+3] = (uint8_t)y;
   g->SpriteVRAMColor[i] = (uint8_t)pal;
}

static int fails = 0, tests = 0;
#define CHECK(cond, ...) do { tests++; if (!(cond)) { fails++; printf("  FAIL %s:%d ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int same(const uint16_t *a, const uint16_t *b) { return memcmp(a, b, SCREEN_WIDTH*SCREEN_HEIGHT*2) == 0; }
static int first_diff(const uint16_t *a, const uint16_t *b)
{ int i; for (i = 0; i < SCREEN_WIDTH*SCREEN_HEIGHT; i++) if (a[i] != b[i]) return i; return -1; }
static int lit(const uint16_t *f, int x, int y) { return f[y*SCREEN_WIDTH + x] != 0; }
static void dump_row(const uint16_t *f, int y)
{  int x, in = 0; printf("    row %d lit runs:", y);
   for (x = 0; x <= SCREEN_WIDTH; x++) { int l = x < SCREEN_WIDTH && lit(f, x, y); if (l && !in) { printf(" %d", x); in = 1; } else if (!l && in) { printf("..%d", x - 1); in = 0; } }
   printf("\n"); }

static uint16_t fa[SCREEN_WIDTH*SCREEN_HEIGHT], fb[SCREEN_WIDTH*SCREEN_HEIGHT], fm[SCREEN_WIDTH*SCREEN_HEIGHT];
static ngpgfx_t GA, GB, GM;
#define CM GB.ColorMap
#define RENDER(t) ss2fg_render(ss2fg_cur(), ss2fg_prev(), (t), fm, SCREEN_WIDTH, CM)   /* 내용 = 최신(cur), 이동 = prev 쪽 */

static void random_state(ngpgfx_t *g)
{
   int i;
   base_state(g);
   for (i = 0; i < 64; i++)
      put_sprite(g, i, 1 + rnd(5), rnd(256), rnd(256), rnd(4), (rnd(2) ? 0x8000 : 0) | (rnd(2) ? 0x4000 : 0) | (rnd(4) == 0 ? 0x0400 : 0) | (rnd(4) == 0 ? 0x0200 : 0), rnd(16));
   for (i = 0; i < 2048; i++)
   {
      uint16_t d = (uint16_t)((rnd(3) == 0 ? 1 + rnd(5) : 0) | (rnd(16) << 9) | (rnd(2) ? 0x8000 : 0) | (rnd(2) ? 0x4000 : 0));
      g->ScrollVRAM[i*2] = d & 0xFF; g->ScrollVRAM[i*2+1] = d >> 8;
   }
   g->scroll1x = rnd(256); g->scroll1y = rnd(256); g->scroll2x = rnd(256); g->scroll2y = rnd(256);
   g->scrollsprx = rnd(256); g->scrollspry = rnd(256);
   g->planeSwap = rnd(2) ? 0x80 : 0;
   g->negative  = rnd(4) == 0 ? 0x80 : 0;
   g->bgc = rnd(8); g->oowc = rnd(8);
   if (rnd(3) == 0) { g->winx = rnd(40); g->winw = 60 + rnd(200); g->winy = rnd(30); g->winh = 80 + rnd(180); }
}

static void t_equiv(void)
{
   int trial, k, f0 = fails;
   for (trial = 0; trial < 300; trial++)
   {
      ss2fg_reset();
      random_state(&GA); random_state(&GB);
      /* B 의 스프라이트 일부는 A 와 같은 물체로(보간 대상) */
      for (k = 0; k < 64; k++) if (rnd(2)) { memcpy(GB.SpriteVRAM + k*4, GA.SpriteVRAM + k*4, 2); GB.SpriteVRAMColor[k] = GA.SpriteVRAMColor[k];
         GB.SpriteVRAM[k*4+2] = (uint8_t)(GA.SpriteVRAM[k*4+2] + rnd(9) - 4); GB.SpriteVRAM[k*4+3] = (uint8_t)(GA.SpriteVRAM[k*4+3] + rnd(9) - 4); }
      frame(&GA, fa, 0, 0, 0);
      frame(&GB, fb, 0, 0, 0);
      CHECK(RENDER(0) == 1, "render(B,A,0) refused");
      { int d = first_diff(fm, fb);
        CHECK(d < 0, "trial %d: render(B,A,0) != mdfn(B) at x=%d y=%d mdfn=%04x fg=%04x | win x%d y%d w%d h%d neg%d swap%d sp=%d,%d", trial, d%160, d/160, d>=0?fb[d]:0, d>=0?fm[d]:0,
              GB.winx, GB.winy, GB.winw, GB.winh, !!GB.negative, !!GB.planeSwap, GB.scrollsprx, GB.scrollspry); }
      CHECK(ss2fg_render(ss2fg_cur(), ss2fg_cur(), 128, fm, SCREEN_WIDTH, CM) == 1 && same(fm, fb), "trial %d: render(B,B,128) != mdfn(B)", trial);
      CHECK(ss2fg_render(ss2fg_cur(), 0, 128, fm, SCREEN_WIDTH, CM) == 1 && same(fm, fb), "trial %d: render(B,NULL,128) != mdfn(B)", trial);
      /* 중간 프레임도 그려지기는 해야 한다(거부 없음) */
      CHECK(RENDER(128) == 1, "trial %d: mid refused", trial);
   }
   printf("1 동치 300회: %s\n", fails == f0 ? "통과" : "실패");
}

static void t_mid(void)
{
   int f0 = fails;
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 1, 10, 20, 3, 0, 1);
   base_state(&GB); put_sprite(&GB, 0, 1, 20, 20, 3, 0, 1);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   CHECK(RENDER(128) == 1, "refused");
   CHECK(lit(fm, 15, 20) && lit(fm, 22, 27) && !lit(fm, 14, 20) && !lit(fm, 23, 20), "t=128: sprite not at x=15..22");
   CHECK(RENDER(64) == 1, "refused");
   CHECK(lit(fm, 18, 20) && !lit(fm, 17, 20) && !lit(fm, 26, 20), "t=64: sprite not at x=18 (20-2.5→18)");
   CHECK(RENDER(0) == 1 && same(fm, fb), "t=0 != B (base)");
   CHECK(RENDER(256) == 1, "refused");
   CHECK(lit(fm, 10, 20) && !lit(fm, 18, 20), "t=256: sprite not at A position");
   /* 세로 이동 + 음수 방향 */
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 1, 50, 60, 3, 0, 1);
   base_state(&GB); put_sprite(&GB, 0, 1, 44, 50, 3, 0, 1);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   CHECK(lit(fm, 47, 55) && !lit(fm, 46, 55) && !lit(fm, 47, 54) && lit(fm, 54, 62) && !lit(fm, 55, 62), "mid of (50,60)->(44,50) not (47,55)");
   /* 스프라이트 오프셋 레지스터로 움직여도 같은 결과 */
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 1, 10, 20, 3, 0, 1); GA.scrollsprx = 0;
   base_state(&GB); put_sprite(&GB, 0, 1, 10, 20, 3, 0, 1); GB.scrollsprx = 10;
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   CHECK(lit(fm, 15, 20) && !lit(fm, 14, 20) && !lit(fm, 23, 20), "sprite offset register motion mid not x=15");
   printf("2 중점: %s\n", fails == f0 ? "통과" : "실패");
}

static void t_snap(void)
{
   int f0 = fails;
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 1, 10, 20, 3, 0, 1);
   base_state(&GB); put_sprite(&GB, 0, 2, 20, 20, 3, 0, 1);   /* 타일 다름 */
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128); CHECK(same(fm, fb), "different tile should snap to B");
   ss2fg_reset();
   base_state(&GB); put_sprite(&GB, 0, 1, 20, 20, 3, 0, 2);   /* 팔레트 다름 */
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128); CHECK(same(fm, fb), "different palette should snap to B");
   ss2fg_reset();
   base_state(&GB); put_sprite(&GB, 0, 1, 20, 20, 2, 0, 1);   /* 우선순위 다름 */
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128); CHECK(same(fm, fb), "different priority should snap to B");
   ss2fg_reset();
   base_state(&GB); put_sprite(&GB, 0, 1, 20, 20, 3, 0x8000, 1);   /* 플립 다름 */
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128); CHECK(same(fm, fb), "different flip should snap to B");
   printf("3 스냅: %s\n", fails == f0 ? "통과" : "실패");
}

static void t_chain(void)
{
   int f0 = fails;
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 1, 10, 10, 3, 0, 1); put_sprite(&GA, 1, 2, 8, 0, 3, 0x0400 | 0x0200, 1);
   base_state(&GB); put_sprite(&GB, 0, 1, 20, 10, 3, 0, 1); put_sprite(&GB, 1, 2, 8, 0, 3, 0x0400 | 0x0200, 1);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   CHECK(lit(fb, 28, 10) && lit(fb, 35, 10) && !lit(fb, 36, 10) && lit(fb, 20, 10) && !lit(fb, 19, 10), "sanity: mdfn(B) parent 20..27 child 28..35");
   RENDER(128);
   CHECK(lit(fm, 15, 10) && lit(fm, 23, 10) && lit(fm, 30, 10) && !lit(fm, 31, 10) && !lit(fm, 14, 10), "chained child not at parent+8 (x=23..30)");
   /* 보이지 않는 앵커(우선순위 0)도 체인을 민다 */
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 1, 10, 10, 0, 0, 1); put_sprite(&GA, 1, 2, 8, 0, 3, 0x0400 | 0x0200, 1);
   base_state(&GB); put_sprite(&GB, 0, 1, 20, 10, 0, 0, 1); put_sprite(&GB, 1, 2, 8, 0, 3, 0x0400 | 0x0200, 1);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   CHECK(lit(fb, 28, 10) && !lit(fb, 27, 10) && !lit(fb, 20, 10), "sanity: mdfn(B) child at x=28, anchor invisible");
   RENDER(128);
   CHECK(!lit(fm, 15, 10) && lit(fm, 23, 10) && !lit(fm, 22, 10), "invisible anchor chain: child not at x=23");
   /* 자식만 상대좌표가 바뀌는 경우: 부모 고정, 자식 +8 → +12 : 중점 +10 */
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 1, 10, 10, 3, 0, 1); put_sprite(&GA, 1, 2, 8, 0, 3, 0x0400 | 0x0200, 1);
   base_state(&GB); put_sprite(&GB, 0, 1, 10, 10, 3, 0, 1); put_sprite(&GB, 1, 2, 12, 0, 3, 0x0400 | 0x0200, 1);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   /* 강체 그룹: 부모(0)·자식(-4)이 갈려 과반이 없다 → 그룹 정지, 자식은 base(B) 자리 22..29 */
   CHECK(lit(fm, 22, 10) && lit(fm, 29, 10) && !lit(fm, 30, 10) && !lit(fm, 21, 10), "rigid group: split votes must leave child at base x=22..29");
   /* 그룹 벡터: 체인 그룹 3조각 중 2조각 일치(-10), 셋째는 타일이 바뀜 → 그룹 벡터로 같이 움직인다 */
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 1, 10, 10, 3, 0, 1); put_sprite(&GA, 1, 2, 8, 0, 3, 0x0400 | 0x0200, 1); put_sprite(&GA, 2, 4, 8, 0, 3, 0x0400 | 0x0200, 1);
   base_state(&GB); put_sprite(&GB, 0, 1, 20, 10, 3, 0, 1); put_sprite(&GB, 1, 2, 8, 0, 3, 0x0400 | 0x0200, 1); put_sprite(&GB, 2, 3, 8, 0, 3, 0x0400 | 0x0200, 1);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   CHECK(lit(fm, 15, 10) && lit(fm, 23, 10) && lit(fm, 31, 10) && lit(fm, 38, 10) && !lit(fm, 39, 10) && !lit(fm, 14, 10), "group vector: third piece (tile changed) not moved with group (x=31..38)");
   CHECK(fm[10*160+31] == fb[10*160+36], "group vector: third piece must keep base(B) tile 3 colour");
   /* 그룹 안 조각들이 제각각이면 그룹 벡터 없음 → 불일치 조각은 제자리 */
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 1, 10, 10, 3, 0, 1); put_sprite(&GA, 1, 2, 8, 0, 3, 0x0400 | 0x0200, 1); put_sprite(&GA, 2, 4, 8, 0, 3, 0x0400 | 0x0200, 1);
   base_state(&GB); put_sprite(&GB, 0, 1, 20, 10, 3, 0, 1); put_sprite(&GB, 1, 2, 4, 0, 3, 0x0400 | 0x0200, 1); put_sprite(&GB, 2, 3, 8, 0, 3, 0x0400 | 0x0200, 1);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   CHECK(lit(fm, 32, 10) && lit(fm, 39, 10) && !lit(fm, 40, 10), "spread group: third piece must stay at base x=32..39");
   CHECK(lit(fm, 20, 10) && !lit(fm, 15, 10), "spread group: parent must stay at base x=20 (rigid, no majority)");
   /* 공용 타일 포즈 교대(뉴트럴 숨쉬기 재현): 앵커 고정, 타일 2 조각 둘이 자리를 바꾼다 → 옛 코드는
      두 조각을 중간 자리(18,18)·(26,18)에 찍어 깨졌다. 공용 타일은 투표 못 하고 앵커(타일 1)만 0 이동 → 전부 base 자리 */
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 1, 10, 10, 3, 0, 1); put_sprite(&GA, 1, 2, 16, 0, 3, 0x0400 | 0x0200, 1); put_sprite(&GA, 2, 2, 0, 16, 3, 0x0400 | 0x0200, 1);
   base_state(&GB); put_sprite(&GB, 0, 1, 10, 10, 3, 0, 1); put_sprite(&GB, 1, 2, 0, 16, 3, 0x0400 | 0x0200, 1); put_sprite(&GB, 2, 2, 16, -16, 3, 0x0400 | 0x0200, 1);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   CHECK(lit(fb, 10, 26) && lit(fb, 26, 10) && !lit(fb, 18, 18), "sanity: mdfn(B) pieces at (10,26) and (26,10)");
   RENDER(128);
   CHECK(lit(fm, 10, 26) && lit(fm, 17, 26) && lit(fm, 26, 10) && lit(fm, 33, 10) && !lit(fm, 18, 18) && !lit(fm, 26, 18) && !lit(fm, 21, 21),
         "shared-tile pose swap: pieces must stay at base, not at midpoints");
   /* 같은 자리 바꿈 + 앵커가 10px 옮겨감: 공용 타일은 표를 못 내고 앵커(-10)만 → 그룹 전체 -5 로 base 포즈째 이동
      (공용 타일 제외가 없으면 표가 셋으로 갈려 그룹이 멈춘다) */
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 1, 10, 10, 3, 0, 1); put_sprite(&GA, 1, 2, 16, 0, 3, 0x0400 | 0x0200, 1); put_sprite(&GA, 2, 2, 0, 16, 3, 0x0400 | 0x0200, 1);
   base_state(&GB); put_sprite(&GB, 0, 1, 20, 10, 3, 0, 1); put_sprite(&GB, 1, 2, 0, 16, 3, 0x0400 | 0x0200, 1); put_sprite(&GB, 2, 2, 16, -16, 3, 0x0400 | 0x0200, 1);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   CHECK(lit(fm, 15, 10) && !lit(fm, 14, 10) && lit(fm, 15, 26) && lit(fm, 22, 26) && !lit(fm, 23, 26) && lit(fm, 31, 10) && lit(fm, 38, 10) && !lit(fm, 39, 10),
         "shared-tile swap with moving anchor: whole group at base pose shifted -5 (anchor votes alone)");
   /* 체인 밖 단독 스프라이트: 공용 타일 둘이 자리를 바꾸면(이동 ≤ 임계 안에서) 정지, 고유 타일은 보간 */
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 2, 10, 40, 3, 0, 1); put_sprite(&GA, 1, 2, 30, 40, 3, 0, 1); put_sprite(&GA, 2, 3, 100, 40, 3, 0, 1);
   base_state(&GB); put_sprite(&GB, 0, 2, 30, 40, 3, 0, 1); put_sprite(&GB, 1, 2, 10, 40, 3, 0, 1); put_sprite(&GB, 2, 3, 110, 40, 3, 0, 1);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   CHECK(lit(fm, 10, 40) && lit(fm, 17, 40) && lit(fm, 30, 40) && lit(fm, 37, 40) && !lit(fm, 18, 40) && !lit(fm, 20, 40) && !lit(fm, 27, 40) && !lit(fm, 29, 40),
         "standalone shared tile swap (|d|=20 < step): must stay at base, not midpoint x=20..27");
   CHECK(lit(fm, 105, 40) && lit(fm, 112, 40) && !lit(fm, 104, 40) && !lit(fm, 113, 40), "standalone unique tile: mid x=105..112");
   /* 공용 타일이라도 그 번호를 쓰는 조각들이 모두 같은 벡터로 움직이면(몸이 통째로) 믿는다 — 걷기에서 몸이 갈라지지 않게 */
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 1, 10, 50, 3, 0, 1); put_sprite(&GA, 1, 2, 18, 50, 3, 0, 1); put_sprite(&GA, 2, 2, 26, 50, 3, 0x8000, 1); put_sprite(&GA, 3, 3, 34, 50, 3, 0, 1);
   base_state(&GB); put_sprite(&GB, 0, 1, 18, 50, 3, 0, 1); put_sprite(&GB, 1, 2, 26, 50, 3, 0, 1); put_sprite(&GB, 2, 2, 34, 50, 3, 0x8000, 1); put_sprite(&GB, 3, 3, 42, 50, 3, 0, 1);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   CHECK(lit(fm, 14, 50) && lit(fm, 45, 50) && !lit(fm, 13, 50) && !lit(fm, 46, 50) && lit(fm, 22, 50) && lit(fm, 30, 50) && lit(fm, 38, 50),
         "shared tile moving as one body: all four pieces at mid (14..45 contiguous)");
   /* 공용 타일 중 하나가 빠지면(to 에서 사용 수가 다름) 그 번호의 조각은 전부 정지 */
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 2, 10, 56, 3, 0, 1); put_sprite(&GA, 1, 2, 60, 56, 3, 0, 1);
   base_state(&GB); put_sprite(&GB, 0, 2, 20, 56, 3, 0, 1); put_sprite(&GB, 1, 2, 70, 56, 3, 0, 1); put_sprite(&GB, 2, 2, 120, 56, 3, 0, 1);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   CHECK(lit(fm, 20, 56) && !lit(fm, 15, 56) && lit(fm, 70, 56) && !lit(fm, 65, 56), "shared tile count differs between frames: pieces stay at base");
   /* 강체 덮어쓰기: 3조각 중 둘은 -10, 셋째(고유 타일)는 포즈가 바뀌어 -14 → 과반 -10 으로 셋 다 -5 */
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 1, 10, 60, 3, 0, 1); put_sprite(&GA, 1, 2, 8, 0, 3, 0x0400 | 0x0200, 1); put_sprite(&GA, 2, 3, 8, 0, 3, 0x0400 | 0x0200, 1);
   base_state(&GB); put_sprite(&GB, 0, 1, 20, 60, 3, 0, 1); put_sprite(&GB, 1, 2, 8, 0, 3, 0x0400 | 0x0200, 1); put_sprite(&GB, 2, 3, 12, 0, 3, 0x0400 | 0x0200, 1);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   CHECK(lit(fb, 40, 60) && !lit(fb, 39, 60), "sanity: mdfn(B) third piece at x=40");
   RENDER(128);
   CHECK(lit(fm, 35, 60) && lit(fm, 42, 60) && !lit(fm, 43, 60) && !lit(fm, 34, 60), "rigid override: third piece must follow group vector (x=35..42), not its own -14");
   CHECK(lit(fm, 15, 60) && lit(fm, 23, 60) && !lit(fm, 14, 60), "rigid override: parent 15.., child 23..");
   /* 타일 그림 교체(사무쇼2 포즈 교대 재현): 같은 슬롯·같은 타일 번호·자리 이동, 그러나 문자 RAM 의 그림이 바뀜 →
      다른 조각이니 움직이지 않고 base(B) 자리에 base 그림. 같은 번호에 그림이 그대로인 다른 조각은 보간 */
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 1, 10, 80, 3, 0, 1); put_sprite(&GA, 1, 2, 60, 80, 3, 0, 1);
   base_state(&GB); put_sprite(&GB, 0, 1, 20, 80, 3, 0, 1); put_sprite(&GB, 1, 2, 70, 80, 3, 0, 1);
   { int i; for (i = 0; i < 8; i++) ((uint16_t*)GB.CharacterRAM)[1*8 + i] = 0xAAAA; }   /* B 의 타일 1 그림만 바꾼다 (A 는 0x5555) */
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   CHECK(lit(fb, 20, 80) && !lit(fb, 19, 80) && lit(fb, 70, 80), "sanity: mdfn(B) tile1 at 20, tile2 at 70");
   RENDER(128);
   CHECK(lit(fm, 20, 80) && lit(fm, 27, 80) && !lit(fm, 28, 80) && !lit(fm, 19, 80), "tile content changed: piece must stay at base x=20..27 (not mid 15)");
   CHECK(fm[80*160+20] == fb[80*160+20], "tile content changed: base(B) picture is drawn");
   CHECK(lit(fm, 65, 80) && lit(fm, 72, 80) && !lit(fm, 64, 80) && !lit(fm, 73, 80), "tile content same: piece interpolated to x=65..72");
   /* 고유성은 두 프레임 모두: base 에선 타일 2 가 하나뿐이지만 to 쪽에서 둘이면 그 조각은 믿지 않는다(정지) */
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 2, 10, 100, 3, 0, 1); put_sprite(&GA, 5, 2, 120, 100, 3, 0, 1);
   base_state(&GB); put_sprite(&GB, 0, 2, 20, 100, 3, 0, 1);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   CHECK(lit(fm, 20, 100) && !lit(fm, 15, 100) && !lit(fm, 19, 100), "uniqueness must hold in both frames: dup in A → static at base x=20");
   /* 과반 분모는 '믿을 수 있는 표'(보이는 조각 수가 아님): 보이는 5조각 중 2조각만 믿을 수 있고 둘이 같으면 그룹이 움직인다 */
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 1, 10, 120, 3, 0, 1); put_sprite(&GA, 1, 2, 8, 0, 3, 0x0400 | 0x0200, 1);
   put_sprite(&GA, 2, 3, 8, 0, 3, 0x0400 | 0x0200, 1); put_sprite(&GA, 3, 3, 8, 0, 3, 0x0400 | 0x0200, 1); put_sprite(&GA, 4, 3, 8, 0, 3, 0x0400 | 0x0200, 1);
   base_state(&GB); put_sprite(&GB, 0, 1, 20, 120, 3, 0, 1); put_sprite(&GB, 1, 2, 8, 0, 3, 0x0400 | 0x0200, 1);
   put_sprite(&GB, 2, 3, 8, 0, 3, 0x0400 | 0x0200, 1); put_sprite(&GB, 3, 3, 8, 0, 3, 0x0400 | 0x0200, 1); put_sprite(&GB, 4, 3, 8, 0, 3, 0x0400 | 0x0200, 1);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   CHECK(lit(fm, 15, 120) && !lit(fm, 14, 120) && lit(fm, 54, 120) && !lit(fm, 55, 120), "quorum over trusted voters: 2 of 5 visible (tile 3 shared) still moves the group by -5 (last piece 47..54)");
   /* 표가 ±1 안에서 갈릴 때 평균의 반올림: -10 과 -11 → -11(0 에서 먼 쪽) → 중점 -5 → x=15 */
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 1, 10, 140, 3, 0, 1); put_sprite(&GA, 1, 2, 7, 0, 3, 0x0400 | 0x0200, 1);
   base_state(&GB); put_sprite(&GB, 0, 1, 20, 140, 3, 0, 1); put_sprite(&GB, 1, 2, 8, 0, 3, 0x0400 | 0x0200, 1);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   CHECK(lit(fm, 15, 140) && !lit(fm, 14, 140), "cluster mean rounding: parent at x=15");
   CHECK(lit(fm, 30, 140) && !lit(fm, 31, 140), "cluster mean rounding: child (base 28, vector -11→mid -5) at x=23..30");
   printf("4 체인: %s\n", fails == f0 ? "통과" : "실패");
}

/* 체인 없는 무리(사무쇼2 방식): 같은 우선순위·팔레트의 인접 연속 슬롯 = 한 캐릭터, 강체 */
static void t_cluster(void)
{
   int f0 = fails, i;
   static const uint16_t pat48[5] = { 0x5A5A, 0xA5A5, 0xF5F5, 0x5F5F, 0xAFAF };   /* 빈 픽셀 없는 서로 다른 무늬 */
#define SOLID48(g) do { int t_, r_; for (t_ = 4; t_ <= 8; t_++) for (r_ = 0; r_ < 8; r_++) ((uint16_t*)(g)->CharacterRAM)[t_*8 + r_] = pat48[t_ - 4]; } while (0)   /* 타일 4..8 도 보이게 — 서로 다른 무늬라 겉모습 짝짓기가 섞이지 않는다 */
   /* 3x2 조각 몸이 +8 걷는 중 포즈 교대: 조각 둘(타일 3·6)은 그림이 바뀜 → 그래도 몸 벡터(-8 → 중점 -4)로 함께 */
   ss2fg_reset();
   base_state(&GA); base_state(&GB); SOLID48(&GA); SOLID48(&GB);
   for (i = 0; i < 6; i++) { put_sprite(&GA, i, 1 + i, 20 + (i % 3) * 8, 40 + (i / 3) * 8, 3, 0, 2); put_sprite(&GB, i, 1 + i, 28 + (i % 3) * 8, 40 + (i / 3) * 8, 3, 0, 2); }
   for (i = 0; i < 8; i++) { ((uint16_t*)GB.CharacterRAM)[3*8 + i] = 0xFAAF; ((uint16_t*)GB.CharacterRAM)[6*8 + i] = 0x5FA5; }   /* B 의 타일 3·6 그림 교체(다른 어떤 타일과도 다른 무늬) */
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   CHECK(lit(fm, 24, 40) && !lit(fm, 23, 40) && lit(fm, 47, 40) && !lit(fm, 48, 40) && lit(fm, 24, 48) && lit(fm, 47, 48) && !lit(fm, 48, 48),
         "cluster rigid: all six pieces (incl. 2 with changed tiles) at mid x=24..47 rows 40 and 48");
   CHECK(fm[40*160+40] == fb[40*160+44] && fm[48*160+40] == fb[48*160+44], "cluster rigid: changed pieces keep base(B) picture");
   /* 표가 둘로 갈리면(-8 둘, +8 둘) 무리 정지: base(B) 28,36,44,52 ← to(A) 20,28,52,60 */
   ss2fg_reset();
   base_state(&GA); base_state(&GB); SOLID48(&GA); SOLID48(&GB);
   for (i = 0; i < 4; i++) { put_sprite(&GA, i, 1 + i, (i < 2 ? 20 : 52) + (i % 2) * 8, 60, 3, 0, 2); put_sprite(&GB, i, 1 + i, 28 + i * 8, 60, 3, 0, 2); }
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   CHECK(lit(fm, 28, 60) && !lit(fm, 27, 60) && lit(fm, 59, 60) && !lit(fm, 60, 60), "cluster split votes 2:2: whole cluster static at base (B) 28..59");
   if (getenv("FGTEST_V")) { dump_row(fb, 60); dump_row(fm, 60); }
   /* 표가 하나뿐이면(여섯 중 하나만 그림 같음) 무리 정지 — 한 조각(관절)이 몸을 끌지 않는다 */
   ss2fg_reset();
   base_state(&GA); base_state(&GB); SOLID48(&GA); SOLID48(&GB);
   for (i = 0; i < 6; i++) { put_sprite(&GA, i, 1 + i, 20 + (i % 3) * 8, 80 + (i / 3) * 8, 3, 0, 2); put_sprite(&GB, i, 1 + i, 28 + (i % 3) * 8, 80 + (i / 3) * 8, 3, 0, 2); }
   { static const uint16_t np[5] = { 0x5AA5, 0xA55A, 0x5FF5, 0xF55F, 0xAFFA };   /* 타일 1 만 그대로 — 2..6 은 다른 어떤 타일과도 다른 무늬로(투명 없음) */
     for (i = 0; i < 8; i++) { int t; for (t = 2; t <= 6; t++) ((uint16_t*)GB.CharacterRAM)[t*8 + i] = np[t - 2]; } }
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   CHECK(lit(fm, 28, 80) && !lit(fm, 27, 80) && lit(fm, 51, 88) && !lit(fm, 52, 88), "cluster single voter: static at base x=28..51");
   /* 팔레트가 다른 인접 몸은 다른 무리 — 각자 움직인다 (pal 2 는 +8, pal 3 은 -8) */
   ss2fg_reset();
   base_state(&GA); base_state(&GB); SOLID48(&GA); SOLID48(&GB);
   for (i = 0; i < 2; i++) { put_sprite(&GA, i, 1 + i, 20 + i * 8, 100, 3, 0, 2); put_sprite(&GB, i, 1 + i, 28 + i * 8, 100, 3, 0, 2); }
   for (i = 2; i < 4; i++) { put_sprite(&GA, i, 1 + i, 60 + (i - 2) * 8, 100, 3, 0, 3); put_sprite(&GB, i, 1 + i, 52 + (i - 2) * 8, 100, 3, 0, 3); }
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   CHECK(lit(fm, 24, 100) && !lit(fm, 23, 100) && lit(fm, 39, 100) && !lit(fm, 40, 100) && lit(fm, 56, 100) && !lit(fm, 55, 100) && lit(fm, 71, 100) && !lit(fm, 72, 100),
         "two clusters by palette move independently: 24..39 and 56..71");
   if (getenv("FGTEST_V")) { dump_row(fb, 100); dump_row(fm, 100); }
   /* 16px 넘게 떨어진 같은 팔레트 조각은 다른 무리 (투사체): 혼자 보간 */
   ss2fg_reset();
   base_state(&GA); base_state(&GB); SOLID48(&GA); SOLID48(&GB);
   for (i = 0; i < 3; i++) { put_sprite(&GA, i, 1 + i, 20 + i * 8, 120, 3, 0, 2); put_sprite(&GB, i, 1 + i, 20 + i * 8, 120, 3, 0, 2); }
   put_sprite(&GA, 3, 4, 100, 120, 3, 0, 2); put_sprite(&GB, 3, 4, 116, 120, 3, 0, 2);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   CHECK(lit(fm, 108, 120) && lit(fm, 115, 120) && !lit(fm, 107, 120) && !lit(fm, 116, 120) && lit(fm, 20, 120) && !lit(fm, 19, 120),
         "far piece is its own cluster: projectile at mid 108..115, body static at 20");
   if (getenv("FGTEST_V")) { dump_row(fb, 120); dump_row(fm, 120); }
   /* 슬롯 밀림(유파선택 재현): to(A) 에서 머리 조각이 슬롯 0 에 끼어들어 나머지가 한 칸씩 밀린다. 슬롯별로 보면
      같은 그림(기모노 K)인 조각들이 -8 로 짝지어 표를 내지만 자리로 보면 몸은 그대로 → 무리 정지여야 한다.
      base(B): s0 H@20 s1 K@28 s2 K@36 s3 K@44 (타일 1·2·3·4; 2·3·4 는 같은 그림)
      to(A):   s0 X@12 s1 H@20 s2 K@28 s3 K@36 s4 K@44 (타일 1·2·3·4·5; 번호는 슬롯에 묶여 그림이 밀림) */
   ss2fg_reset();
   base_state(&GA); base_state(&GB); SOLID48(&GA); SOLID48(&GB);
   { int r;
     for (r = 0; r < 8; r++) { ((uint16_t*)GB.CharacterRAM)[1*8 + r] = 0x5555; ((uint16_t*)GB.CharacterRAM)[2*8 + r] = 0xAAAA; ((uint16_t*)GB.CharacterRAM)[3*8 + r] = 0xAAAA; ((uint16_t*)GB.CharacterRAM)[4*8 + r] = 0xAAAA; }
     for (r = 0; r < 8; r++) { ((uint16_t*)GA.CharacterRAM)[1*8 + r] = 0xFFFF; ((uint16_t*)GA.CharacterRAM)[2*8 + r] = 0x5555; ((uint16_t*)GA.CharacterRAM)[3*8 + r] = 0xAAAA; ((uint16_t*)GA.CharacterRAM)[4*8 + r] = 0xAAAA; ((uint16_t*)GA.CharacterRAM)[5*8 + r] = 0xAAAA; } }
   put_sprite(&GB, 0, 1, 20, 140, 3, 0, 2); put_sprite(&GB, 1, 2, 28, 140, 3, 0, 2); put_sprite(&GB, 2, 3, 36, 140, 3, 0, 2); put_sprite(&GB, 3, 4, 44, 140, 3, 0, 2);
   put_sprite(&GA, 0, 1, 12, 140, 3, 0, 2); put_sprite(&GA, 1, 2, 20, 140, 3, 0, 2); put_sprite(&GA, 2, 3, 28, 140, 3, 0, 2); put_sprite(&GA, 3, 4, 36, 140, 3, 0, 2); put_sprite(&GA, 4, 5, 44, 140, 3, 0, 2);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   CHECK(lit(fm, 20, 140) && !lit(fm, 19, 140) && !lit(fm, 16, 140) && lit(fm, 51, 140) && !lit(fm, 52, 140),
         "slot shift (head inserted at slot 0): body must stay at base 20..51, not jump -4");
   if (getenv("FGTEST_V")) { dump_row(fb, 140); dump_row(fm, 140); }
   /* 같은 배치에서 몸이 정말 -8 움직였으면(to 의 모든 조각이 8 왼쪽) 겹침 검증을 통과해 -4 로 움직여야 한다 */
   ss2fg_reset();
   base_state(&GA); base_state(&GB); SOLID48(&GA); SOLID48(&GB);
   { int r; for (r = 0; r < 8; r++) { ((uint16_t*)GB.CharacterRAM)[2*8 + r] = 0xAAAA; ((uint16_t*)GB.CharacterRAM)[3*8 + r] = 0xAAAA; ((uint16_t*)GB.CharacterRAM)[4*8 + r] = 0xAAAA;
                                       ((uint16_t*)GA.CharacterRAM)[2*8 + r] = 0xAAAA; ((uint16_t*)GA.CharacterRAM)[3*8 + r] = 0xAAAA; ((uint16_t*)GA.CharacterRAM)[4*8 + r] = 0xAAAA; } }
   put_sprite(&GB, 0, 1, 20, 148, 3, 0, 2); put_sprite(&GB, 1, 2, 28, 148, 3, 0, 2); put_sprite(&GB, 2, 3, 36, 148, 3, 0, 2); put_sprite(&GB, 3, 4, 44, 148, 3, 0, 2);
   put_sprite(&GA, 0, 1, 12, 148, 3, 0, 2); put_sprite(&GA, 1, 2, 20, 148, 3, 0, 2); put_sprite(&GA, 2, 3, 28, 148, 3, 0, 2); put_sprite(&GA, 3, 4, 36, 148, 3, 0, 2);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   CHECK(lit(fm, 16, 148) && !lit(fm, 15, 148) && lit(fm, 47, 148) && !lit(fm, 48, 148), "real -8 body move with repeated tiles: mid 16..47");
   if (getenv("FGTEST_V")) { dump_row(fb, 148); dump_row(fm, 148); }
   /* 겉모습 짝짓기(PocketCore 방 패치 3판에서 가져옴): 슬롯 밀림 + 몸 이동. to(A) 에서 조각 X 가 슬롯 0 에 끼어들어 번호가
      밀리고 몸은 8 왼쪽. 슬롯별로는 모든 슬롯의 그림이 바뀌어 표가 하나도 없지만(옛 규칙: 정지), 겉모습으로 짝지으면
      전부 -8 → 중점 -4 로 몸이 움직인다.  base(B): s0 H@20 s1 K1@28 s2 K2@36 s3 K3@44 (타일 1·2·3·4)
      to(A): s0 X@4 s1 H@12 s2 K1@20 s3 K2@28 s4 K3@36 (타일 1·2·3·4·5 — 그림이 한 칸씩 밀려 들어 있다) */
   ss2fg_reset();
   base_state(&GA); base_state(&GB); SOLID48(&GA); SOLID48(&GB);
   { int r; for (r = 0; r < 8; r++) { ((uint16_t*)GA.CharacterRAM)[1*8 + r] = 0xF5F5; ((uint16_t*)GA.CharacterRAM)[2*8 + r] = 0x5555; ((uint16_t*)GA.CharacterRAM)[3*8 + r] = 0xAAAA;
                                       ((uint16_t*)GA.CharacterRAM)[4*8 + r] = 0xFFFF; ((uint16_t*)GA.CharacterRAM)[5*8 + r] = 0x5A5A; } }
   put_sprite(&GB, 0, 1, 20, 20, 3, 0, 2); put_sprite(&GB, 1, 2, 28, 20, 3, 0, 2); put_sprite(&GB, 2, 3, 36, 20, 3, 0, 2); put_sprite(&GB, 3, 4, 44, 20, 3, 0, 2);
   put_sprite(&GA, 0, 1, 4, 20, 3, 0, 2); put_sprite(&GA, 1, 2, 12, 20, 3, 0, 2); put_sprite(&GA, 2, 3, 20, 20, 3, 0, 2); put_sprite(&GA, 3, 4, 28, 20, 3, 0, 2); put_sprite(&GA, 4, 5, 36, 20, 3, 0, 2);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   CHECK(lit(fa, 4, 20) && lit(fa, 43, 20) && !lit(fa, 44, 20) && lit(fb, 20, 20) && lit(fb, 51, 20), "sanity: mdfn(A) 4..43, mdfn(B) 20..51");
   RENDER(128);
   CHECK(lit(fm, 16, 20) && !lit(fm, 15, 20) && lit(fm, 47, 20) && !lit(fm, 48, 20), "look match: slot-shifted moving body at mid 16..47 (no slot agrees, looks do)");
   CHECK(fm[20*160+16] == fb[20*160+20] && fm[20*160+40] == fb[20*160+44], "look match: base(B) pictures drawn in base order");
   if (getenv("FGTEST_V")) { dump_row(fb, 20); dump_row(fm, 20); }
   /* 반복 무늬 줄(롬 실측: 가로 6조각 효과, 가운데 4조각이 같은 그림)이 -8 움직인다: 같은 그림 조각은 짝이 넷이라 표가
      ±8·±16 으로 흩어지지만 진짜 이동량에도 같이 주므로 1등은 뚜렷하다(과반 규칙이면 36/72 로 멈췄다) → 중점 -4 */
   ss2fg_reset();
   base_state(&GA); base_state(&GB); SOLID48(&GA); SOLID48(&GB);
   { int r; for (r = 0; r < 8; r++) { int t; for (t = 3; t <= 6; t++) { ((uint16_t*)GA.CharacterRAM)[t*8 + r] = 0xFFFF; ((uint16_t*)GB.CharacterRAM)[t*8 + r] = 0xFFFF; } } }
   put_sprite(&GB, 0, 1, 20, 30, 3, 0, 2); put_sprite(&GA, 0, 1, 12, 30, 3, 0, 2);
   for (i = 1; i <= 4; i++) { put_sprite(&GB, i, 2 + i, 20 + i * 8, 30, 3, 0, 2); put_sprite(&GA, i, 2 + i, 12 + i * 8, 30, 3, 0, 2); }
   put_sprite(&GB, 5, 2, 60, 30, 3, 0, 2); put_sprite(&GA, 5, 2, 52, 30, 3, 0, 2);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   CHECK(lit(fm, 16, 30) && !lit(fm, 15, 30) && lit(fm, 63, 30) && !lit(fm, 64, 30), "repeated-pattern row moving -8: all six at mid 16..63");
   if (getenv("FGTEST_V")) { dump_row(fb, 30); dump_row(fm, 30); }
   /* 같은 그림 두 조각뿐인 물체(투사체)가 -8: 짝이 둘이라 반 표씩이지만 두 조각이 같은 1등에 표를 주니 움직인다 → 16..31 */
   ss2fg_reset();
   base_state(&GA); base_state(&GB); SOLID48(&GA); SOLID48(&GB);
   put_sprite(&GB, 0, 3, 20, 10, 3, 0, 2); put_sprite(&GB, 1, 3, 28, 10, 3, 0, 2);
   put_sprite(&GA, 0, 3, 12, 10, 3, 0, 2); put_sprite(&GA, 1, 3, 20, 10, 3, 0, 2);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   CHECK(lit(fm, 16, 10) && !lit(fm, 15, 10) && lit(fm, 31, 10) && !lit(fm, 32, 10), "two identical pieces moving -8: mid 16..31");
   if (getenv("FGTEST_V")) { dump_row(fb, 10); dump_row(fm, 10); }
   printf("4b 무리(체인 없음): %s\n", fails == f0 ? "통과" : "실패");
#undef SOLID48
}

static void t_scrollwrap(void)
{
   int f0 = fails, i;
   base_state(&GA); base_state(&GB);
   /* 플레인1 타일맵: 열 0 에만 타일 3 (전체 행) */
   for (i = 0; i < 32; i++) { GA.ScrollVRAM[(i*32)*2] = 3; GB.ScrollVRAM[(i*32)*2] = 3; }
   ss2fg_reset(); GA.scroll1x = 250; GB.scroll1x = 4;
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   GM = GB; GM.scroll1x = 255; frame_ref(&GM, fa, 0, 0);
   CHECK(same(fm, fa), "scroll 250->4 mid should equal scroll 255 (diff at %d)", first_diff(fm, fa));
   /* 반대 방향 4→250 : -10 → 중점 255 */
   ss2fg_reset(); GA.scroll1x = 4; GB.scroll1x = 250;
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   GM = GB; GM.scroll1x = 255; frame_ref(&GM, fa, 0, 0);
   CHECK(same(fm, fa), "scroll 4->250 mid should equal scroll 255");
   /* 임계 초과(100 차이) → B */
   ss2fg_reset(); GA.scroll1x = 0; GB.scroll1x = 100;
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   CHECK(same(fm, fb), "scroll jump >32 should snap to B");
   /* 세로 스크롤 + 플레인2 */
   ss2fg_reset(); base_state(&GA); base_state(&GB);
   for (i = 0; i < 32; i++) { GA.ScrollVRAM[0x800 + (i)*2] = 3; GB.ScrollVRAM[0x800 + (i)*2] = 3; }   /* 플레인2 행 0 */
   GA.scroll2y = 0; GB.scroll2y = 6;
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   GM = GB; GM.scroll2y = 3; frame_ref(&GM, fa, 0, 0);
   CHECK(same(fm, fa), "plane2 vertical scroll mid (3) mismatch at %d", first_diff(fm, fa));
   printf("5 스크롤 랩·임계: %s\n", fails == f0 ? "통과" : "실패");
}

static void t_threshold(void)
{
   int f0 = fails;
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 1, 10, 20, 3, 0, 1);
   base_state(&GB); put_sprite(&GB, 0, 1, 100, 20, 3, 0, 1);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128); CHECK(same(fm, fb), "sprite jump 90px should snap to B");
   /* 왼쪽 가장자리 넘기: 2 → 254(-2) 는 -4 이동 → 중점 0 */
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 1, 2, 20, 3, 0, 1);
   base_state(&GB); put_sprite(&GB, 0, 1, 254, 20, 3, 0, 1);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128);
   CHECK(lit(fm, 0, 20) && lit(fm, 7, 20) && !lit(fm, 8, 20), "edge crossing 2->-2 mid should be x=0");
   /* 정확히 임계(24) 는 보간, 25 는 스냅 */
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 1, 10, 20, 3, 0, 1);
   base_state(&GB); put_sprite(&GB, 0, 1, 34, 20, 3, 0, 1);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128); CHECK(lit(fm, 22, 20) && !lit(fm, 21, 20), "24px step should interpolate (x=22)");
   ss2fg_reset();
   base_state(&GB); put_sprite(&GB, 0, 1, 35, 20, 3, 0, 1);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   RENDER(128); CHECK(same(fm, fb), "25px step should snap");
   printf("6 임계·가장자리: %s\n", fails == f0 ? "통과" : "실패");
}

static void t_dirty(void)
{
   int f0 = fails;
   base_state(&GA); put_sprite(&GA, 0, 1, 10, 20, 3, 0, 1);
   base_state(&GB); put_sprite(&GB, 0, 1, 20, 20, 3, 0, 1);
   ss2fg_reset(); frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, SS2FG_DIRTY_SPR);
   CHECK(RENDER(128) == 0, "dirty sprite table must refuse");
   ss2fg_reset(); frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, SS2FG_DIRTY_SCROLL);
   CHECK(RENDER(128) == 0, "dirty tilemap must refuse");
   ss2fg_reset(); frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, SS2FG_DIRTY_CHR);
   CHECK(RENDER(128) == 1, "dirty char RAM alone is tolerated");
   /* 앞 프레임만 더티 → a 없이(보간 없이) 그린다 */
   ss2fg_reset(); frame(&GA, fa, 0, 0, SS2FG_DIRTY_SCROLL); frame(&GB, fb, 0, 0, 0);
   CHECK(RENDER(128) == 1 && same(fm, fb), "dirty A → draw B as is");
   /* 흑백 모드 거부 */
   ss2fg_reset(); GB.K2GE_MODE = 1; frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0); GB.K2GE_MODE = 0;
   CHECK(RENDER(128) == 0, "mono must refuse");
   /* 캡처가 하나뿐이면 보간 없이 그린다 */
   ss2fg_reset(); frame(&GB, fb, 0, 0, 0);
   CHECK(ss2fg_prev() == 0 && RENDER(128) == 1 && same(fm, fb), "single capture → B as is");
   /* 캡처 없음 → 거부 */
   ss2fg_reset();
   CHECK(RENDER(128) == 0, "no capture must refuse");
   printf("7 더티·흑백: %s\n", fails == f0 ? "통과" : "실패");
}

static void t_pop(void)
{
   int f0 = fails;
   const ss2fg_frame *c, *p;
   ss2fg_reset();
   base_state(&GA); put_sprite(&GA, 0, 1, 10, 20, 3, 0, 1);
   base_state(&GB); put_sprite(&GB, 0, 1, 20, 20, 3, 0, 1);
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   c = ss2fg_cur(); p = ss2fg_prev();
   CHECK(c && p && c->spr[2] == 20 && p->spr[2] == 10, "cur/prev after two captures");
   /* 예측 프레임 P 를 넣었다가 무른다 → cur=B, prev=A 로 복귀 */
   base_state(&GM); put_sprite(&GM, 0, 1, 30, 20, 3, 0, 1);
   frame(&GM, fm, 0, 0, 0);
   CHECK(ss2fg_cur()->spr[2] == 30 && ss2fg_prev()->spr[2] == 20, "after predicted capture cur=P prev=B");
   RENDER(128); CHECK(lit(fm, 25, 20) && !lit(fm, 24, 20), "mid(B,P) at x=25");
   ss2fg_capture_pop();
   CHECK(ss2fg_cur() && ss2fg_cur()->spr[2] == 20, "after pop cur=B");
   CHECK(ss2fg_prev() && ss2fg_prev()->spr[2] == 10, "after pop prev=A");
   /* 그 다음 실제 프레임 → cur=C prev=B, 무른 슬롯이 재사용돼도 B 가 멀쩡해야 한다 */
   base_state(&GM); put_sprite(&GM, 0, 1, 25, 20, 3, 0, 1);
   frame(&GM, fm, 0, 0, 0);
   CHECK(ss2fg_cur()->spr[2] == 25 && ss2fg_prev()->spr[2] == 20, "after next real capture cur=C prev=B");
   RENDER(128); CHECK(lit(fm, 23, 20) && !lit(fm, 22, 20) && !lit(fm, 31, 20), "mid(B,C) at x=23 (20+2.5)");
   /* 두 번 연속 pop 도 안전 */
   ss2fg_capture_pop(); ss2fg_capture_pop(); ss2fg_capture_pop();
   CHECK(ss2fg_cur() == 0 || 1, "pop chain does not crash");
   printf("8 pop(예측 무르기): %s\n", fails == f0 ? "통과" : "실패");
}

/* 래스터: 줄 0..75 와 76..151 의 스크롤이 다르다 */
static void raster_cb(ngpgfx_t *g, int y, void *ud)
{
   const int *v = (const int *)ud;
   g->scroll1x = (uint8_t)(y < 76 ? v[0] : v[1]);
}

static void t_raster(void)
{
   int f0 = fails, i;
   int va[2] = { 0, 8 }, vb[2] = { 4, 12 }, vm[2] = { 2, 10 };
   base_state(&GA); base_state(&GB);
   for (i = 0; i < 32; i++) { GA.ScrollVRAM[(i*32)*2] = 3; GB.ScrollVRAM[(i*32)*2] = 3; GA.ScrollVRAM[(i*32+5)*2] = 5; GB.ScrollVRAM[(i*32+5)*2] = 5; }
   ss2fg_reset();
   frame(&GA, fa, raster_cb, va, 0); frame(&GB, fb, raster_cb, vb, 0);
   RENDER(128);
   GM = GB; frame_ref(&GM, fa, raster_cb, vm);
   CHECK(same(fm, fa), "per-line scroll mid (2/10) mismatch at %d", first_diff(fm, fa));
   /* 동치도 래스터에서: render(B,A,0) == mdfn(B) */
   RENDER(0);
   CHECK(same(fm, fb), "raster t=0 != mdfn(B) at %d", first_diff(fm, fb));
   /* 줄별 스프라이트 오프셋 래스터도 */
   printf("9 래스터: %s\n", fails == f0 ? "통과" : "실패");
}


/* 4배(게임 박자 맞춤)용 — 스프라이트·스크롤 목표를 따로 주는 render2, 움직임 판정 ss2fg_motion, 캡처 링 ss2fg_hist */
static void t_four(void)
{
   int f0 = fails, i;
   static uint16_t fr[SCREEN_WIDTH*SCREEN_HEIGHT];
   /* A: 스프라이트 x=10, 플레인1 스크롤 0 · B: x=20, 스크롤 8. 플레인1 타일맵 열 0·5 에 타일 3·5 */
   ss2fg_reset();
   base_state(&GA); base_state(&GB);
   for (i = 0; i < 32; i++) { GA.ScrollVRAM[(i*32)*2] = 3; GB.ScrollVRAM[(i*32)*2] = 3; GA.ScrollVRAM[(i*32+5)*2] = 5; GB.ScrollVRAM[(i*32+5)*2] = 5; }
   put_sprite(&GA, 0, 1, 10, 60, 3, 0, 1); put_sprite(&GB, 0, 1, 20, 60, 3, 0, 1);
   GA.scroll1x = 0; GB.scroll1x = 8;
   frame(&GA, fa, 0, 0, 0); frame(&GB, fb, 0, 0, 0);
   /* 둘 다 128 = 기존 render 와 같은 그림 */
   RENDER(128);
   CHECK(ss2fg_render2(ss2fg_cur(), ss2fg_prev(), 128, ss2fg_prev(), 128, fr, SCREEN_WIDTH, 2, CM) && memcmp(fr, fm, sizeof fr) == 0,
         "render2(spr 128, scr 128) == render(128)");
   /* 스프라이트만 반: 스프라이트 x=15, 스크롤은 base(B)=8 그대로 → 스프라이트 줄 밖은 B 와 같다 */
   CHECK(ss2fg_render2(ss2fg_cur(), ss2fg_prev(), 128, 0, 0, fr, SCREEN_WIDTH, 2, CM), "render2 spr-only ok");
   CHECK(lit(fr, 15, 60) && lit(fr, 22, 60) && !lit(fr, 14, 60) && !lit(fr, 23, 60), "render2 spr-only: sprite at mid x=15..22");
   CHECK(memcmp(fr + 100*SCREEN_WIDTH, fb + 100*SCREEN_WIDTH, 20 * SCREEN_WIDTH * 2) == 0, "render2 spr-only: rows 100..119 (no sprite) identical to base(B) scroll");
   /* 스크롤만 반: 스프라이트는 base(B) x=20, 스크롤은 A·B 중간(4) — 참고 렌더(B 상태에 scroll1x=4)와 스프라이트 없는 줄이 같다 */
   CHECK(ss2fg_render2(ss2fg_cur(), 0, 0, ss2fg_prev(), 128, fr, SCREEN_WIDTH, 2, CM), "render2 scr-only ok");
   CHECK(lit(fr, 20, 60) && lit(fr, 27, 60) && !lit(fr, 19, 60) && !lit(fr, 28, 60), "render2 scr-only: sprite stays at base x=20..27");
   GM = GB; GM.scroll1x = 4; frame_ref(&GM, fa, 0, 0);
   CHECK(memcmp(fr + 100*SCREEN_WIDTH, fa + 100*SCREEN_WIDTH, 20 * SCREEN_WIDTH * 2) == 0, "render2 scr-only: rows 100..119 equal reference with scroll1x=4");
   /* ss2fg_motion: 스프라이트 이동 → 비트0, 스크롤 변화 → 비트1 */
   CHECK(ss2fg_motion(ss2fg_prev(), ss2fg_cur()) == 3, "motion(A,B) == 3 (sprite + scroll)");
   CHECK(ss2fg_motion(ss2fg_cur(), ss2fg_cur()) == 0, "motion(B,B) == 0");
   base_state(&GM); for (i = 0; i < 32; i++) { GM.ScrollVRAM[(i*32)*2] = 3; GM.ScrollVRAM[(i*32+5)*2] = 5; }
   put_sprite(&GM, 0, 1, 20, 60, 3, 0, 1); GM.scroll1x = 12;             /* C: 스프라이트 그대로, 스크롤만 */
   frame(&GM, fm, 0, 0, 0);
   CHECK(ss2fg_motion(ss2fg_prev(), ss2fg_cur()) == 2, "motion(B,C) == 2 (scroll only)");
   GM.scroll1x = 12; put_sprite(&GM, 0, 1, 28, 60, 3, 0, 1);              /* D: 스프라이트만 */
   frame(&GM, fm, 0, 0, 0);
   CHECK(ss2fg_motion(ss2fg_prev(), ss2fg_cur()) == 1, "motion(C,D) == 1 (sprite only)");
   /* 캡처 링: A B C D 가 쌓였다 → hist(0)=D … hist(3)=A, hist(4)=없음. 두 번 무르면 hist(0)=B */
   CHECK(ss2fg_hist(0) && ss2fg_hist(0)->spr[2] == 28 && ss2fg_hist(1) && ss2fg_hist(1)->spr[2] == 20 && ss2fg_hist(1)->line[0].s1x == 12 &&
         ss2fg_hist(2) && ss2fg_hist(2)->spr[2] == 20 && ss2fg_hist(2)->line[0].s1x == 8 && ss2fg_hist(3) && ss2fg_hist(3)->spr[2] == 10,
         "hist(0..3) = D C B A");
   CHECK(ss2fg_hist(4) == 0 && ss2fg_hist(-1) == 0 && ss2fg_hist(9) == 0, "hist(4) empty, bad index NULL");
   ss2fg_capture_pop(); ss2fg_capture_pop();
   CHECK(ss2fg_cur() && ss2fg_cur()->spr[2] == 20 && ss2fg_cur()->line[0].s1x == 8 && ss2fg_prev() && ss2fg_prev()->spr[2] == 10,
         "after two pops cur=B prev=A");
   /* 그 뒤 새 캡처가 무른 슬롯을 재사용해도 B·A 가 멀쩡하고, 여섯 장 넘게 쌓으면 가장 오래된 것부터 밀려난다 */
   for (i = 0; i < 7; i++) { put_sprite(&GM, 0, 1, (uint8_t)(40 + i * 8), 60, 3, 0, 1); frame(&GM, fm, 0, 0, 0); }
   CHECK(ss2fg_hist(0)->spr[2] == 88 && ss2fg_hist(5) && ss2fg_hist(5)->spr[2] == 48 && ss2fg_hist(6) == 0, "ring keeps 6 most recent (hist(5) = 48, hist(6) empty)");
   printf("10 4배 — render2·motion·hist: %s\n", fails == f0 ? "통과" : "실패");
}

int main(void)
{
   t_equiv(); t_mid(); t_snap(); t_chain(); t_cluster(); t_scrollwrap(); t_threshold(); t_dirty(); t_pop(); t_raster(); t_four();
   printf("%d 검사 중 %d 실패\n", tests, fails);
   return fails ? 1 : 0;
}
