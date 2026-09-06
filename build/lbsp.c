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
#define OFF_FACE       LBSP_UNMEASURED   /* 좌우 반전 */
#define OFF_H1         LBSP_UNMEASURED   /* 지상/공중 */
#define OFF_HP2        LBSP_UNMEASURED   /* 상대 체력 — 교차 증인 */
#define OFF_COMBO      LBSP_UNMEASURED   /* 콤보 수 — 게임이 화면에도 띄운다 */
#define LBSP_TH_STRONG LBSP_UNMEASURED   /* 강약 문턱(프레임) */
#define LBSP_CMD_WIN   LBSP_UNMEASURED   /* 커맨드 창(프레임) */

static const int lb_consts[] = {
   OFF_FACE, OFF_H1, OFF_HP2, OFF_COMBO, LBSP_TH_STRONG, LBSP_CMD_WIN
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

   ⚠ **앞 = NGP_R 로 못 박았다.** 반전(좌우) 주소가 아직 미측정이라
     「P1 이 왼쪽에서 오른쪽을 보는」 트레이닝 무대에서만 옳다.
     M3 로 가기 전에 반드시 반전을 재라 — KOF 는 반전을 잘못 잡아
     **한 라운드 두 번째 발동부터 커맨드가 좌우로 뒤집히는** 사고를 배포까지 냈다.

   ⚠ 프레임 수는 lb_moves.py 의 실측 그대로다(방향 4프레임씩, 버튼 6프레임).
     링이 2프레임에 한 칸이니 4프레임이면 한 방향이 두 칸을 채운다. */
typedef struct { int frames; unsigned char pad; } LbMacStep;

static const LbMacStep MAC_236A[] = {
   { 4, NGP_D },
   { 4, (unsigned char)(NGP_D | NGP_R) },
   { 4, NGP_R },
   { 6, NGP_A }                 /* 버튼은 방향을 놓고 나서 — 실측이 그랬다 */
};
#define MAC_236A_N ((int)(sizeof MAC_236A / sizeof MAC_236A[0]))

/* ── 상태 ────────────────────────────────────────────────────── */
static int  lb_engine_on;      /* 기본 꺼짐 */
static int  lb_is_rom;
static int  mac_step = -1;     /* -1 = 안 돎 */
static int  mac_left;          /* 이번 칸에 남은 프레임 */
static int  trig_prev;

char lbsp_last_disp[64];
int  lbsp_disp_seq;

void lbsp_set_engine(int on) { lb_engine_on = on ? 1 : 0; }
int  lbsp_engine_on(void)    { return lb_engine_on; }

void lbsp_reset(void)
{
   mac_step = -1; mac_left = 0; trig_prev = 0;
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

   /* ★ 엔진이 꺼져 있으면 **순정 폴드 그대로**다 — R 까지 접는다.
      이것이 대조군의 정의다. 여기서 한 비트라도 다르면 대조군이 아니다. */
   if (!lb_engine_on || !lb_is_rom)
   {
      if (ret & (1u << RP_Y)) pad |= NGP_A;
      if (ret & (1u << RP_X)) pad |= NGP_B;
      if ((ret & (1u << RP_L)) || (ret & (1u << RP_R)))
         pad |= (uint8_t)(NGP_A | NGP_B);
      mac_step = -1;
      trig_prev = 0;
      return pad;
   }

   /* 엔진 켬 — R 은 트리거라 접지 않는다. L 은 그대로 A+B(사람의 동시입력 수단). */
   trig = (ret & (1u << RP_R)) ? 1 : 0;
   if (ret & (1u << RP_Y)) pad |= NGP_A;
   if (ret & (1u << RP_X)) pad |= NGP_B;
   if (ret & (1u << RP_L)) pad |= (uint8_t)(NGP_A | NGP_B);

   /* 매크로가 도는 동안은 **사람 입력을 통째로 덮는다.**
      섞으면 사람이 잡고 있던 방향이 커맨드에 끼어들어 딴 기술이 나간다. */
   if (mac_step >= 0)
   {
      pad = MAC_236A[mac_step].pad;
      if (--mac_left <= 0)
      {
         mac_step++;
         if (mac_step >= MAC_236A_N) mac_step = -1;
         else mac_left = MAC_236A[mac_step].frames;
      }
      trig_prev = trig;
      return pad;
   }

   /* 엣지에서만 시작한다. 누르고 있는 동안 되풀이 발동하면 그게 누출이다. */
   if (trig && !trig_prev && lb_can_start())
   {
      mac_step = 0;
      mac_left = MAC_236A[0].frames;
      pad = MAC_236A[0].pad;
      if (--mac_left <= 0) { mac_step = 1; mac_left = MAC_236A[1].frames; }
      lb_disp("\u2193\u2198\u2192 + \ubca0\uae30");   /* ↓↘→ + 베기 */
      trig_prev = trig;
      return pad;
   }

   trig_prev = trig;
   return pad;
}
