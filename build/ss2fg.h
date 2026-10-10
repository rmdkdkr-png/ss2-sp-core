/* ss2fg — 프레임 생성(중간 프레임 합성) 층
 *
 * 네오지오 포켓의 K2GE 는 스프라이트 64개(8×8, 체인 상대좌표)와 스크롤 플레인 2장을
 * 스캔라인 단위로 그린다. 게임 로직은 60fps 라 진짜 중간 상태는 없지만, 연속한 두
 * 프레임의 **스프라이트표·스크롤 레지스터**를 캡처해 두면 그 사이 위치를 보간해 같은
 * 타일·팔레트로 다시 그릴 수 있다 — 픽셀을 섞지 않으니 픽셀아트가 뭉개지지 않는다.
 *
 * 쓰는 쪽:
 *   gfx.c   — 스캔라인마다 ss2fg_capture_line(), V-int 에서 ss2fg_capture_end(),
 *             표시 중 VRAM 쓰기에 ss2fg_capture_write()
 *   libretro.c / 앱 — ss2fg_render(prev, cur, t, dst, …) 로 중간 프레임을 그린다
 *
 * mednafen 헤더에 기대지 않는다(코어 gfx.c 와 앱 gfx.cpp 가 같이 쓰도록). */
#ifndef SS2FG_H_INCLUDED
#define SS2FG_H_INCLUDED

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SS2FG_W 160
#define SS2FG_H 152

/* 스캔라인을 그리는 순간의 지연 레지스터(ngpgfx_delayed_settings 가 반영한 값) */
typedef struct ss2fg_regs {
   uint8_t winx, winw, winy, winh;
   uint8_t s1x, s1y, s2x, s2y;     /* 스크롤 플레인 1·2 */
   uint8_t spx, spy;               /* 스프라이트 오프셋 */
   uint8_t swap, bgc, oowc, neg;   /* 플레인 순서 교환, 배경색, 창밖색, 반전 */
} ss2fg_regs;

/* 표시 중 VRAM 쓰기 종류 (dirty 비트) */
#define SS2FG_DIRTY_SPR    1
#define SS2FG_DIRTY_SCROLL 2
#define SS2FG_DIRTY_CHR    4
#define SS2FG_DIRTY_PAL    8

typedef struct ss2fg_frame {
   ss2fg_regs line[SS2FG_H];
   uint8_t scroll[4096];           /* 9000-9fff 타일맵 2장 */
   uint8_t chr[8192];              /* a000-bfff 타일 */
   uint8_t spr[256];               /* 8800-88ff 스프라이트표 */
   uint8_t sprcol[64];             /* 8c00-8c3f 스프라이트 팔레트 번호 */
   uint8_t pal[512];               /* 8200-83ff 컬러 팔레트 */
   uint8_t mono;                   /* K2GE_MODE != 0 (흑백 호환 모드) — 합성 안 함 */
   uint8_t valid;
   uint8_t dirty;
   uint8_t pad;
   int     layers;                 /* layer_enable 마스크 */
   int16_t ax[64], ay[64];         /* 체인을 푼 절대 좌표(오프셋 더하기 전) */
   /* 사무쇼2 RAM 의 물체 표(0x0E00 + 0x40·k) — ss2fg_set_ram 이 있을 때만. 0 = P1, 1 = P2(몸), 2·3 = 그림자,
      4~7 = 기술 이펙트·장풍. 화면 X = 물체 X - 카메라, Y = 발 높이. 게임은 RAM 을 먼저 바꾸고 다음 프레임에
      그리므로, 이 프레임 화면의 위치 = 직전 캡처 순간의 RAM 값 */
#define SS2FG_OBJS 8
   uint8_t ob_now_x[SS2FG_OBJS], ob_now_y[SS2FG_OBJS], ob_now_t[SS2FG_OBJS];  /* 이 캡처 순간(다음 프레임에 나옴) */
   uint8_t ob_x[SS2FG_OBJS], ob_y[SS2FG_OBJS], ob_t[SS2FG_OBJS];              /* 이 프레임 화면의 위치·종류(하위 바이트) */
   uint8_t ob_now_act, ob_act;     /* 쓰는 칸 비트(종류 0xFFFF = 빈 칸) */
   uint8_t now_ok, body_ok;        /* 대전 중이라 값이 유효함 */
} ss2fg_frame;

/* ── 캡처 ── */
void ss2fg_capture_line(int y, const ss2fg_regs *r);
void ss2fg_capture_end(const uint8_t *scroll, const uint8_t *chr, const uint8_t *spr,
                       const uint8_t *sprcol, const uint8_t *pal, int mono, int layers);
void ss2fg_capture_write(int dirty_bit);   /* 표시 중(1..151줄) VRAM 쓰기 */
void ss2fg_capture_pop(void);              /* 마지막 capture_end 를 무른다(예측 프레임 되돌리기) */
void ss2fg_reset(void);
/* 사무쇼2 RAM(CPU 0x4000~, 16KB) — 주면 캡처마다 물체 표(두 캐릭터·그림자·이펙트)의 위치를 같이 적는다(NULL 이면 안 쓴다).
   포즈가 매번 통째로 바뀌는 동작(맞고 빙글빙글 날아가기 등)은 조각 겉모습으로 짝을 못 지으니 몸 위치로 옮기고,
   그림자·이펙트의 빈칸은 가장 가까운 물체로 채운다(PocketCore 방 30_ss2fg_body / 40_ss2fg_objects 패치의 규칙) */
void ss2fg_set_ram(const uint8_t *ram);

const ss2fg_frame *ss2fg_prev(void);       /* 직전 실제 프레임 (없으면 NULL) */
const ss2fg_frame *ss2fg_cur(void);        /* 마지막 실제 프레임 (없으면 NULL) */
const ss2fg_frame *ss2fg_hist(int k);      /* k 장 전 캡처 (0 = 마지막, 최대 5) */
/* a→b 사이 움직임: 비트0 스프라이트, 비트1 스크롤 */
int ss2fg_motion(const ss2fg_frame *a, const ss2fg_frame *b);

/* ── 합성 ──
 * base 의 그림(타일·팔레트·타일맵·창·배경색)을 그대로 쓰되, 스프라이트·스크롤 위치만 to 쪽으로
 * t/256 만큼 옮겨 dst(16bpp, pitch_px 픽셀 단위)에 그린다. colormap 은 K2GE 12비트 색 →
 * 출력 픽셀 표(4096칸, gfx->ColorMap). to 가 NULL 이면 base 를 그대로 그린다.
 *   예측 모드: base = 실제 N,  to = 예측 N+1  (없는 그림은 절대 안 나온다 — 위치만 예측)
 *   보간 모드: base = 실제 N,  to = 실제 N-1 (N 의 그림을 N-1 쪽으로 반만 되돌린 자리)
 * 반환 1 = 그렸다, 0 = 합성 불가(흑백 모드·표시 중 스프라이트/타일맵 쓰기·캡처 없음) —
 * 그때는 호출자가 실제 화면을 그대로 내보낸다. */
int ss2fg_render(const ss2fg_frame *base, const ss2fg_frame *to, int t256,
                 uint16_t *dst, int pitch_px, const uint32_t *colormap);
/* 픽셀 폭을 고르는 판 — bpp 2(16비트) 또는 4(32비트, 앱의 RGBA8888 표면). colormap 은 그 포맷의 것 */
int ss2fg_render_ex(const ss2fg_frame *base, const ss2fg_frame *to, int t256,
                    void *dst, int pitch_px, int bpp, const uint32_t *colormap);

/* 4배(게임 박자 맞춤) — 스프라이트·스크롤의 목표 프레임과 진행도(0..256)를 따로 */
int ss2fg_render2(const ss2fg_frame *base, const ss2fg_frame *to_spr, int t_spr,
                  const ss2fg_frame *to_scr, int t_scr,
                  void *dst, int pitch_px, int bpp, const uint32_t *colormap);

/* 보간 임계 — 이보다 멀리 뛴 스프라이트/스크롤은 보간하지 않고 b 자리에 둔다(순간이동·장면 전환) */
#define SS2FG_SPR_MAX_STEP    24
#define SS2FG_SCROLL_MAX_STEP 32

#ifdef __cplusplus
}
#endif
#endif
