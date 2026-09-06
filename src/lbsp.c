/* 월화의 검사 원버튼 필살기 엔진 — M2: **매크로 하나**.
 *
 * ─ 왜 svcsp.c 를 일반화하지 않고 파일을 하나 더 만드나
 *   저장소가 이미 ss2sp.c / svcsp.c / kofsp.c 로 갈라 놓았다. 넷째도 가른다.
 *   일반화 안은 kofsp.c 머리에서 한 번 기각됐다(「돌고 있는 코드를 대수술」).
 *   이식이 다 끝난 뒤에 판단할 일이다.
 *
 * ─ M1 이 일부러 아무것도 안 하는 이유
 *   게임 여러 개를 한 .so 에 담을 때 실제로 깨지는 자리는 **분기와 배관**이지 엔진이 아니다.
 *   메탈슬러그가 SS2 선처리에 삼켜져 무반응이던 사고가 그 증거다.
 *   그래서 M1 의 통과 조건은 **출력이 한 비트도 안 바뀌는 것** 하나다.
 *
 *   M1(배관만)은 통과했다 — 무회귀 일곱 게임 0바이트, 폴드 8,192조합 어긋남 0.
 *
 * ─ M2 에서 달라진 것: **R 이 SP 트리거다**
 *   ★ 단, **엔진을 끄면 R 은 예전처럼 A+B 로 접힌다.** 이건 kofsp 와 «일부러 다르다»
 *     (kofsp 는 R 을 무조건 뺀다). 이렇게 해야 「엔진 끔」이 **순정과 글자 그대로 같은**
 *     대조군이 된다 — 대조군이 조금이라도 다르면 그건 대조군이 아니다.
 *
 * ─ M2 는 링을 **안 쓴다**
 *   링은 M0 에서 찾아 뒀지만(0x1313~) M3 의 일이다. 여기서 링까지 같이 넣으면
 *   실패했을 때 **배관 탓인지 링 탓인지 못 가른다.** 먼저 패드로 걸어 넣어
 *   발동을 세우고, 그 다음에 링으로 빠르게 만든다.
 *
 * ─ 게임 상수는 잰 것만 넣는다
 *   안 잰 것은 전부 LBSP_UNMEASURED 로 둔다. 「아직 안 잰 것에 기대는 코드」가
 *   **구조적으로 못 생기게** 하려는 것이다. 실측 근거는 tools/lb/README.md · MOVES.md.
 */
#include "lbsp.h"

#include <stdlib.h>
#include <string.h>

/* ── 램 접근 — ss2sp.c · svcsp.c · kofsp.c 와 같은 이중 경로 ─────── */
#ifdef SS2SP_RAM_POINTER
static uint8_t *lb_ram_ptr;
void lbsp_set_ram(void *p) { lb_ram_ptr = (uint8_t *)p; }
#define CPUExRAM lb_ram_ptr
#else
extern uint8_t CPUExRAM[16384];
#endif

/* ── 게임 상수 ───────────────────────────────────────────────────
   램 오프셋은 CPUExRAM 기준(= CPU 주소 − 0x4000).
   ⚠ 겉모습으로 정하지 마라. 승격은 「독립 시나리오 2개 + 위상 2종 + 교차 증인」이다.
   ⚠ SvC·KOF 상수를 베껴 오지 마라 — KOF 는 강약 문턱이 6/4 로 SvC 의 12/5 와 달랐다. */
#define LBSP_UNMEASURED (-1)

/* ★ 승격됨 — 동작 ID. 쉴 때 4 · 서서베기 96 · 앉아베기 120 · 킥 128 ·
   236+A 112 · 623+A 116 · 214+B 168. 기술 여섯을 위상 2종으로 돌려 값이 전부 같았고,
   표에 없는 커맨드(421+A)는 평타만 나오는 대조군이 섰다. (tools/lb/MOVES.md) */
#define OFF_ACT        0x0370
#define LBSP_ACT_REST  4

/* ★ 승격됨 — 방향이력 링. **SvC·KOF 와 구조가 다르다.**
   그 둘은 값이 «밀린다»(간격 2). 월화는 **값이 안 움직이고 머리만 나아간다**(간격 1).
   결과는 같지만 주입 코드가 다르다 — 밀기를 흉내 내면 안 된다.
   한 칸 = 2프레임 → 128칸으로 256프레임을 기억한다. 값은 NGP 패드 비트 그대로.
   중립도 기록된다(0 이 앉는다). 머리는 128 에서 되돌아온다. */
#define OFF_RING_HEAD  0x1312
#define OFF_RING       0x1313
#define LBSP_RING_N    128
#define LBSP_P2_DELTA  0x88    /* P2 는 머리 0x139A · 링 0x139B~ */

/* ── 아직 안 잰 것 ───────────────────────────────────────────────
   ⚠ **반전(좌우)이 제일 위험하다.** KOF 에서 0x0D4A 를 반전이라 잘못 잡았는데
     그건 「필살기를 한 번 쓰면 서고 안 내려오는 플래그」였고, 그 탓에 배포된 엔진이
     한 라운드 두 번째 발동부터 커맨드를 좌우로 뒤집었다. 이것을 못 확정하면 접는다. */
/* ★ 승격됨 — 좌우 반전. **0 = 오른쪽 봄(앞=R) · 1 = 왼쪽 봄(앞=L).**
   P2 는 +0x40 (P2 act 는 0x03B0, 쉼 4 로 확인). 승격 근거:
     · 독립 시나리오 둘 — ⓐ 뛰어넘어 자리를 바꾼다 ⓑ 넘어간 뒤 걷는 방향이 뒤집힌다
       (반전0: R=앞걷기20·L=뒤걷기24 / 반전1: R=24·L=20 — 네 칸 진리표가 딱 맞는다)
     · 위상 2종 완전 일치 · 가만히 있으면 안 바뀜(대조군)
     · 교차 증인 — P2(+0x40)가 «늘 반대 값»이다
   ★ **되돌아온다**: 0 → 1 → 0. 되넘어오면 0 으로 복귀한다.
     KOF 가 여기서 당했다 — 0x0D4A 는 한 번 서면 «안 내려오는» 플래그였는데
     스냅숏 두 장만 보고 반전이라 불렀고, 배포된 엔진이 한 라운드
     **두 번째 발동부터 커맨드를 좌우로 뒤집었다.** 그래서 복귀를 반드시 본다. */
#define OFF_FACE       0x0386
#define OFF_FACE2      (OFF_FACE + 0x40)  /* 교차 증인용 — 판정에는 안 쓴다 */
#define LBSP_FACE_LEFT 1
#define OFF_H1         LBSP_UNMEASURED   /* 지상/공중 */
#define OFF_HP2        LBSP_UNMEASURED   /* 상대 체력 — 교차 증인 */
#define OFF_COMBO      LBSP_UNMEASURED   /* 콤보 수 — 게임이 화면에도 띄운다 */
/* ★ 강약 문턱 — 버튼을 **8프레임 이상** 쥐면 강이 된다(질풍 지속 62 → 70).
   ⚠ **7 은 금지구역이다.** 위상 0 에서는 약, 위상 1 에서는 강으로 «갈린다».
     KOF 의 「5프레임은 위상에 따라 갈림 → 금지구역」과 똑같은 꼴이다.
     안전한 약 구역은 ≤6, 안전한 강 구역은 ≥8. 그래서 기본 버튼을 4 로 둔다.
   ⚠ 성질이 다른 증인으로도 확인했다 — **엔진을 끄고 손으로 베기를 쥐어도** 같은 문턱에서
     act 96(약) → 112(강) 로 넘어간다. */
#define LBSP_TH_STRONG 8
#define LBSP_TH_TAPMAX 6                 /* 여기까지는 확실히 약 */
#define LBSP_CMD_WIN   LBSP_UNMEASURED   /* 커맨드 창(프레임) */

static const int lb_consts[] = {
   OFF_H1, OFF_HP2, OFF_COMBO, LBSP_CMD_WIN
};

int lbsp_unmeasured_count(void)
{
   unsigned i; int n = 0;
   for (i = 0; i < sizeof(lb_consts) / sizeof(lb_consts[0]); i++)
      if (lb_consts[i] == LBSP_UNMEASURED) n++;
   return n;
}

/* ── 패드 비트 ───────────────────────────────────────────────────
   ⚠ 대본 글자와 헷갈리지 마라. 하네스 대본은 **레트로패드** 기준이라
     NGP A = 대본 `B` · NGP B = 대본 `A` 다. 여기 값은 **NGP 쪽**이다. */
#define NGP_U 0x01
#define NGP_D 0x02
#define NGP_L 0x04
#define NGP_R 0x08
#define NGP_A (1 << 4)
#define NGP_B (1 << 5)

#define RP_Y 1
#define RP_X 9
#define RP_L 10
#define RP_R 11

/* ── 매크로 ──────────────────────────────────────────────────────
   M2 는 **하나**만 있다: 236+A (카에데 질풍). act 112 가 나오면 성공이다.

   ⚠ 프레임 수는 lb_moves.py 의 실측 그대로다(방향 4프레임씩, 버튼 6프레임).
     링이 2프레임에 한 칸이니 4프레임이면 한 방향이 두 칸을 채운다.

   ★ 방향은 **`F`(앞)·`B`(뒤)로 적고** 나갈 때 반전을 보고 실제 비트로 바꾼다.
     못 박아 두면 반대편에서 딴 기술이 나간다. */
#define F 0x40                  /* 앞 — 실제 비트는 lb_fwd() 가 정한다 */
#define B 0x80                  /* 뒤 */

/* 아래 링 도우미들의 앞선언 — 대본 표가 먼저 와야 읽기 좋아서 여기 둔다. */
static int lb_env(const char *k, int dflt);
static int lb_ring_on(void);

typedef struct { int frames; unsigned char pad; } LbMacStep;

static const LbMacStep MAC_236A[] = {
   { 4, NGP_D },
   { 4, (unsigned char)(NGP_D | F) },
   { 4, F },
   { 6, NGP_A }                 /* 버튼은 방향을 놓고 나서 — 실측이 그랬다 */
};
#define MAC_236A_N ((int)(sizeof MAC_236A / sizeof MAC_236A[0]))

/* 링을 쓸 때의 짧은 꼬리 — D·DF 는 박고 **F 와 버튼만 실제로 누른다.**
   프레임 수는 lb_env() 로 흔들 수 있다(다시 안 굽고 쓸어 보려고). */
static LbMacStep mac_ring[2];
static int mac_ring_n;

static void lb_build_ring_macro(void)
{
   mac_ring[0].frames = lb_env("LBSP_RING_F", 2);
   mac_ring[0].pad = F;
   mac_ring[1].frames = lb_env("LBSP_RING_BTN", 4);
   mac_ring[1].pad = NGP_A;
   mac_ring_n = (mac_ring[0].frames > 0) ? 2 : 1;
   if (mac_ring[0].frames <= 0) { mac_ring[0] = mac_ring[1]; }
}

/* 지금 도는 대본이 무엇인지 — 링을 썼으면 짧은 꼬리, 아니면 원래 18프레임. */
static const LbMacStep *mac_cur;
static int mac_cur_n;

/* ── 상태 ────────────────────────────────────────────────────── */
static int  lb_engine_on;      /* 기본 꺼짐 */
static int  lb_is_rom;
static int  mac_step = -1;     /* -1 = 안 돎 */
static uint8_t mac_fwd;        /* ★ 시작할 때의 «앞». 도는 중엔 안 바꾼다 */
static int  mac_left;          /* 이번 칸에 남은 프레임 */
static int  trig_prev;

char lbsp_last_disp[64];
int  lbsp_disp_seq;

void lbsp_set_engine(int on) { lb_engine_on = on ? 1 : 0; }
int  lbsp_engine_on(void)    { return lb_engine_on; }

void lbsp_reset(void)
{
   mac_step = -1; mac_left = 0; trig_prev = 0; mac_fwd = NGP_R;
   mac_cur = MAC_236A; mac_cur_n = MAC_236A_N;
   lbsp_last_disp[0] = 0;
   /* seq 는 **안 되돌린다** — 프론트가 엣지로 보므로 되돌리면 옛 값과 같아져 한 번 놓친다. */
}

/* 헤더 0x24 의 게임 표식으로 판별한다.
   ⚠ 판별 순서 사고를 피하려면 접두가 안 겹쳐야 한다. 확인했다:
     LASTBLADE124 는 SAMURAI2 · KOF R2 · SNKvsCAPCOM1 · GEKKA 어느 것과도 안 겹친다. */
void lbsp_set_rom(const void *rom, unsigned len)
{
   const char *p = (const char *)rom;
   lb_is_rom = 0;
   if (p && len >= 0x30 && !memcmp(p + 0x24, "LASTBLADE", 9))
      lb_is_rom = 1;
   lbsp_reset();
}

int lbsp_rom_ok(void) { return lb_is_rom; }

/* ── 링 주입 ─────────────────────────────────────────────────────
   기본 **켬**. 끄려면 LBSP_RING=0 — 대조군을 돌릴 길은 남겨 둔다. */
static int lb_ring_on(void)
{
   static int v = -1;
   if (v < 0) { const char *e = getenv("LBSP_RING"); v = !(e && *e == '0'); }
   return v;
}

static int lb_env(const char *k, int dflt)
{
   const char *e = getenv(k);
   if (!e || !*e) return dflt;
   return atoi(e);
}

/* 머리에서 back 칸 뒤에 값을 박는다. back=1 이 «가장 최근 칸»이다.
   ⚠ 머리(0x1312)는 «다음에 쓸 칸»의 색인이다 — 최근 칸은 머리−1 이다.
   ⚠ 한 칸만 붙잡아 두고 증명하려 하지 마라. KOF 에서 링을 정적으로 붙잡았더니
     게임이 읽는 시작점이 돌아 사실상 모든 회전을 시도하게 돼 증명이 안 됐다.
     **한 번만 쓰는 것**이 본질이다. */
static void lb_ring_put(int back, uint8_t v)
{
   int h = CPUExRAM[OFF_RING_HEAD];
   CPUExRAM[OFF_RING + ((h - back) & (LBSP_RING_N - 1))] = v;
}

/* 지금 «앞»이 어느 비트인가. 램을 못 읽으면 오른쪽 봄으로 둔다(트레이닝 기본). */
static uint8_t lb_fwd(void)
{
#ifdef SS2SP_RAM_POINTER
   if (!CPUExRAM) return NGP_R;
#endif
   return (CPUExRAM[OFF_FACE] == LBSP_FACE_LEFT) ? NGP_L : NGP_R;
}

/* 대본의 F/B 를 실제 방향 비트로 바꾼다. 한 프레임 안에서 값을 고정해 쓴다 —
   매크로가 도는 중에 반전이 바뀌어도 «시작할 때의 앞»을 끝까지 쓰기 위해서다. */
static uint8_t lb_bits(uint8_t p, uint8_t fwd)
{
   uint8_t back = (fwd == NGP_R) ? NGP_L : NGP_R;
   uint8_t o = (uint8_t)(p & (NGP_U | NGP_D | NGP_A | NGP_B));
   if (p & F) o |= fwd;
   if (p & B) o |= back;
   return o;
}

/* 기술명 — 프론트가 seq 엣지를 보고 버퍼를 읽는다. **버퍼 먼저, seq 나중.** */
static void lb_disp(const char *t)
{
   size_t n = strlen(t);
   if (n >= sizeof lbsp_last_disp) n = sizeof lbsp_last_disp - 1;
   memcpy(lbsp_last_disp, t, n);
   lbsp_last_disp[n] = 0;
   lbsp_disp_seq++;
}

/* 지금 매크로를 걸어도 되는 몸 상태인가.
   act 를 못 읽으면(램이 없으면) **막지 않는다** — M2 의 관심사는 배관이지 조건이 아니다.
   조건을 촘촘히 다는 것은 M3 이후, 그것도 «재고 나서» 할 일이다. */
static int lb_can_start(void)
{
#ifdef SS2SP_RAM_POINTER
   if (!CPUExRAM) return 1;      /* 포인터 빌드에서만 널일 수 있다 */
#endif
   return CPUExRAM[OFF_ACT] == LBSP_ACT_REST;
}

/* ── 매 프레임 ─────────────────────────────────────────────────── */
uint8_t lbsp_frame(uint8_t pad, uint16_t ret)
{
   int trig;

   /* ★ **R 은 겸업하지 않는다.** 엔진을 꺼도 R 은 A+B 로 안 접힌다.
      유저 지시: 「a+b는 a+b의 역할이고 SP는 SP다」.
      화면에 「SP」라 적힌 버튼이 때에 따라 A+B 를 내면 그건 거짓말이다.

      잃는 것은 없다 — NGPC 실기에는 A·B 두 버튼뿐이라 **R 을 A+B 로 접는 것 자체가
      우리가 만든 규약**이었고, 끔일 때도 Y=A · X=B · **L=A+B** 로 다 낼 수 있다.
      「끔이 순정 대조군」이라는 구실은 **L 이 그대로 이어받는다** — L 은 엔진과
      무관하게 A+B 를 내고 그것을 끔·켬 둘 다에서 실측했다(act 128, 위상 2종).

      ⚠ KOF R-2(`kofsp.c`)는 아직 겸업한다. 거기는 유저에게 물어 정할 일이라 안 건드렸다. */
   if (!lb_engine_on || !lb_is_rom)
   {
      if (ret & (1u << RP_Y)) pad |= NGP_A;
      if (ret & (1u << RP_X)) pad |= NGP_B;
      if (ret & (1u << RP_L)) pad |= (uint8_t)(NGP_A | NGP_B);
      mac_step = -1;
      trig_prev = 0;
      return pad;
   }

   /* 엔진 켬 — R 은 트리거. L 은 그대로 A+B(사람의 동시입력 수단). */
   trig = (ret & (1u << RP_R)) ? 1 : 0;
   if (ret & (1u << RP_Y)) pad |= NGP_A;
   if (ret & (1u << RP_X)) pad |= NGP_B;
   if (ret & (1u << RP_L)) pad |= (uint8_t)(NGP_A | NGP_B);

   /* 매크로가 도는 동안은 **사람 입력을 통째로 덮는다.**
      섞으면 사람이 잡고 있던 방향이 커맨드에 끼어들어 딴 기술이 나간다. */
   if (mac_step >= 0)
   {
      pad = lb_bits(mac_cur[mac_step].pad, mac_fwd);
      if (--mac_left <= 0)
      {
         mac_step++;
         if (mac_step >= mac_cur_n) mac_step = -1;
         else mac_left = mac_cur[mac_step].frames;
      }
      trig_prev = trig;
      return pad;
   }

   /* 엣지에서만 시작한다. 누르고 있는 동안 되풀이 발동하면 그게 누출이다. */
   if (trig && !trig_prev && lb_can_start())
   {
      mac_fwd = lb_fwd();
      if (lb_ring_on())
      {
         /* ★ 마지막 F 는 카디널이니 **박지 않는다.** D·DF 만 박고 F 는 진짜로 누른다.
            최근 칸(머리−1)이 DF, 그 앞(머리−2)이 D 다 — 시간 순서가 뒤집히면 안 된다. */
         lb_ring_put(1, (uint8_t)(NGP_D | mac_fwd));
         lb_ring_put(2, NGP_D);
         lb_build_ring_macro();
         mac_cur = mac_ring; mac_cur_n = mac_ring_n;
      }
      else
      {
         mac_cur = MAC_236A; mac_cur_n = MAC_236A_N;
      }
      mac_step = 0;
      mac_left = mac_cur[0].frames;
      pad = lb_bits(mac_cur[0].pad, mac_fwd);
      if (--mac_left <= 0)
      {
         mac_step = 1;
         if (mac_step >= mac_cur_n) mac_step = -1;
         else mac_left = mac_cur[1].frames;
      }
      lb_disp("\u2193\u2198\u2192 + \ubca0\uae30");   /* ↓↘→ + 베기 */
      trig_prev = trig;
      return pad;
   }

   trig_prev = trig;
   return pad;
}
