/* KOF R-2 원버튼 필살기 엔진 — M1: **배관만**.
 *
 * ─ 왜 svcsp.c 를 일반화하지 않고 파일을 하나 더 만드나
 *   저장소가 이미 ss2sp.c / svcsp.c 로 갈라 놓았다. 세 번째를 만드는 것이 선례에 맞다.
 *   svcsp_frame 은 480줄이고 중립 act 집합·위상 처리가 본문에 박혀 있어서,
 *   서술자 구조체로 일반화하려면 **돌고 있는 코드를 대수술**해야 한다.
 *   이식이 끝난 뒤에 판단할 일이다.
 *
 * ─ M1 이 일부러 아무것도 안 하는 이유
 *   게임 여러 개를 한 .so 에 담을 때 실제로 깨지는 자리는 **분기와 배관**이지 엔진이 아니다.
 *   실제로 오늘(2026-09-02) 메탈슬러그가 SS2 선처리에 삼켜져 무반응이던 사고를 고쳤다.
 *   그래서 M1 은 **출력이 한 비트도 안 바뀌는 것**을 통과 조건으로 잡는다.
 *
 *   ⚠ 지금 KOF R-2 는 libretro.c 의 **순정 롬 폴드** 가지로 떨어진다(Y=A·X=B·L·R=A+B).
 *   그러므로 kofsp 가지를 그 앞에 끼우면서 **폴드와 똑같이 동작**해야 조작이 안 바뀐다.
 *   엔진 동작은 오프셋을 재고 나서 이 위에 얹는다.
 *
 * ─ 게임 상수는 하나도 안 잰 상태다
 *   전부 KOFSP_UNMEASURED 로 둔다. 잰 것만 값을 넣는다.
 *   「아직 안 잰 것에 기대는 코드」가 **구조적으로 못 생기게** 하려는 것이다.
 *   실측 절차와 근거는 계획서와 tools/kof/README.md 에 있다.
 */
#include "kofsp.h"

#include <string.h>

/* ── 게임 상수 ───────────────────────────────────────────────────
   램 오프셋은 CPUExRAM 기준(= CPU 주소 - 0x4000).
   ⚠ 겉모습으로 정하지 마라. 승격은 「독립 시나리오 2개 + 위상 2종 + 교차 증인」이다. */
#define KOFSP_UNMEASURED (-1)

/* 후보 — M0 에서 잰 것. 아직 **확정 아니다**.
   0x0D3C: 서있기 213 / 앞걷기 64 / 뒤걷기 75 / 공중 76,
           탭 57 · 홀드 173 · 236 계열 96 · 623 계열 127→7→63 (스파링 정지 무대, 쿄).
           네 시나리오에서 갈리고 vx 가 교차 증인이 되므로 사다리 1·3 은 채웠다.
           위상 2종 재확인이 남아 확정으로 안 올린다.
   0x0D5A/0x0D5C: 16비트 리틀엔디언 X/Y 속도. 앞 +6553 / 뒤 -6553 으로 부호가 뒤집힌다. */
#define OFF_ACT   0x0D3C
#define OFF_VX    0x0D5A   /* 16비트. 앞 +6553 / 뒤 −6553 (≈0.1px, 소수부다) */

/* ⚠ 1일차에 `0x0D5C` 를 「Y 속도(위로 갈 때 −5)」라고 적어 뒀는데
   **점프 시계열에서 재현되지 않았다** — 상승·정점·하강 내내 0 이었다.
   그때는 6프레임 간격 덤프였고 지금은 매 프레임이다. 그러니 그 이름표는 **취소**하고
   미측정으로 되돌린다. 다시 재기 전에는 쓰지 마라. */
#define OFF_VY_UNCONFIRMED 0x0D5C

/* 세로 위치 — 점프를 매 프레임 훑어 확인했다. 둘은 **서로 거울**이다(합이 152):
     0x0D50  화면 Y (지상 128, 정점 104 — 위로 갈수록 **감소**)
     0x0D58  지면 위 높이 (지상 24, 정점 48 — 위로 갈수록 **증가**)
   공중 판정은 지상값과 다른지로 한다. 점프 높이는 24.
   ⚠ 지상값 128/24 는 **이 무대(ちゅうきんとう)에서 잰 것**이다. 무대가 바뀌어도
     같은지 확인 안 했다 — 높이 쪽(0x0D58)이 무대에 덜 휘둘릴 것으로 보이나 미확인이다. */
#define OFF_Y1      0x0D50
#define OFF_H1      0x0D58
#define KOFSP_Y_GROUND 128
#define KOFSP_H_GROUND 24

/* 콤보 카운터 — **반증까지 통과**했다.
   두 히트의 간격을 벌려 가며 재니 ≤34프레임이면 2, **≥38프레임이면 둘 다 맞았는데도 0**.
   경계가 따로 잰 피격 경직 창(~36프레임)과 맞아떨어진다.
   ⚠ 첫 시도는 **무효 실험**이었다 — 3연타를 넣었는데 체력이 한 번만 줄었다(2·3타가 헛침).
     「콤보가 실제로 성립했는지」부터 확인하고서야 이 바이트가 나왔다. */
#define OFF_COMBO 0x10ED

/* ── 확정 (2026-09-02, 스파링 정지 무대·쿄) ─────────────────────
   전부 tools/kof/kof_hunt.py 로 잡았다. 요령은 **조건 길이를 똑같이 맞추는 것**이다 —
   길이가 다르면 애니메이션이 통째로 달라져 469바이트가 「갈린다」로 나온다.
   길이를 60프레임으로 통일하고 술어 하나를 걸면 328 → 1 개가 된다. */

/* 버튼 쥠 카운터 — 2프레임에 1씩 오르고 **놓으면 리셋**.
   술어: 무입력 == 놓음 < 늦게쥠 < 계속쥠. 펀치·킥이 이웃이다(SVC 의 0x0C76/77 배치와 같다). */
#define OFF_HOLDCNT_P 0x100C
#define OFF_HOLDCNT_K 0x100D

/* 강약 문턱 — **독립 두 방법이 일치**한다.
   ① poke 로 카운터를 고정하고 탭만 넣으면 **2 일 때만** 강이 나온다.
      (poke 는 매 프레임 core_run 앞에 덮으므로 게임은 늘 V 를 읽고 V+1 을 쓴다.
       그러니 비교가 `>=` 가 아니라 **`==`** 이고, 문턱은 3 이다.)
   ② poke 없이 홀드 길이를 훑으면 6프레임부터 강. 그때 카운터가 정확히 3이다.
   ⚠ SVC 는 12/5 였다. **재사용하면 사람의 탭 6~11프레임이 전부 약으로 죽는다.**
   ⚠ 5프레임은 **위상에 따라 갈린다** — 엔진이 쓰면 안 되는 값이다. */
#define KOFSP_HOLD_STRONG 6   /* 이 길이 이상 쥐면 강 (카운터 3) */
#define KOFSP_TAP_MAX     4   /* 이 길이 이하는 두 위상 모두 약 */

/* 동작 카운터 — 2프레임에 1씩 오르다 **127 에서 포화**. 동작이 시작되면 리셋된다.
   P/K 로 갈린다(펀치를 치면 P 만 리셋). 0x101C/0x101D 에 **같은 값의 짝**이 하나 더 있다.
   ⚠ **성공/실패 신호로 쓰지 마라.** SVC 에서 이 카운터는 **발동 없이도 리셋됐고**
     (킥 진행·히트 등) 착지 사이클·공중 소비 감지가 둘 다 그걸로 오탐했다.
     새 동작 판정은 **act 변화**로 한다. 이건 「행동 가능」 근사에만 쓴다. */
#define OFF_ANIM_P 0x1014
#define OFF_ANIM_K 0x1015

/* ── P2 블록은 P1 + 0x140 ────────────────────────────────────────
   P1 act 0x0D3C ↔ P2 act 0x0E7C 로 확인했다. 상대 쪽을 볼 때 이 간격을 쓴다. */
#define KOFSP_P2_STRIDE 0x0140
#define OFF_ACT2  (OFF_ACT + KOFSP_P2_STRIDE)   /* 0x0E7C */

/* 히트 판정 — **상대 act 가 피격 경직값이 되는 프레임**이다.
   SVC 는 별도 react 바이트(0x0AC4=255)를 썼는데, KOF 는 상대 act 로 충분하다.
   무적 오프 무대에서 체력 감소와 **같은 프레임**에 76 이 뜨고, 약 36프레임 뒤 213 으로 돌아온다.
   255 로 튀는 바이트도 다섯 개 있었지만 전부 P2 블록 **밖**이라 히트 섬광 효과로 보인다. */
#define KOFSP_ACT_HITSTUN 76

/* 상대 체력 — P2 블록 안(act+0x45). 히트한 **그 프레임에** 줄어든다.
   ⚠ 사본이 둘 더 있다: `0x0D0D` 는 같이 줄고, **`0x0D0E` 는 22프레임 늦게 따라온다**
     (체력바 애니메이션). 지연 사본으로 히트 프레임을 재면 판정이 통째로 밀린다.
   실측 피해(쿄, 근접): **약 3 · 강 7** — 이걸로 「홀드=강」 이름표가 확인됐다.
   ⚠ 무적이 기본 켜짐이라 피해를 재려면 스파링 설정에서 꺼야 한다(kof_spar_dmg.st). */
#define OFF_HP2 0x0EC1

/* 입력 이력 만료 — 찌꺼기 모션이 **다음 입력과 이어 붙는다**(막는 게 아니라).
   623 모션 뒤 236+B 를 넣으면 간격 11프레임까지 623 이 나가거나 엉뚱한 게 나간다.
   **12프레임부터 두 위상 모두 깨끗하다.**
   → 매크로를 쏘기 전에 이만큼 조용한 창을 보장하거나, 이력을 덮어써야 한다. */
#define KOFSP_HIST_CLEAR 12

/* 미측정 — 이 값들이 채워져야 엔진이 돈다. 목록 자체가 사냥 목록이다. */
static const int OFF_CHAR1     = KOFSP_UNMEASURED;   /* 전투중 판별 4종 */
static const int OFF_CHAR2     = KOFSP_UNMEASURED;
static const int OFF_STYLE     = KOFSP_UNMEASURED;
static const int OFF_TIMER     = KOFSP_UNMEASURED;
static const int OFF_FACE      = KOFSP_UNMEASURED;   /* 좌우 반전. ⚠ 좌표 비교 금지 */
static const int OFF_DIRHIST   = KOFSP_UNMEASURED;   /* 링 주입 */
static const int OFF_RING_TOP  = KOFSP_UNMEASURED;
/* 아직 안 잰 것: 방향 이력 링과 그 최신 위치. 링 사냥은 `!w`(24칸 상한)로는 못 하고
   프레임마다 램 16KB 를 통째로 떠서 오프라인 diff 해야 한다. */

int kofsp_unmeasured_count(void)
{
   const int *v[] = { &OFF_CHAR1, &OFF_CHAR2, &OFF_STYLE, &OFF_TIMER, &OFF_FACE,
                      &OFF_DIRHIST, &OFF_RING_TOP };
   int i, n = 0;
   for (i = 0; i < (int)(sizeof(v) / sizeof(v[0])); i++)
      if (*v[i] == KOFSP_UNMEASURED) n++;
   return n;
}

/* ── 롬 판별 ─────────────────────────────────────────────────────
   헤더 0x24 의 12바이트 표식. KOF R-2 는 "KOF R2" 뒤가 공백이라 **앞 6바이트**만 본다
   (실측 'KOF R2      '). thinkbox knowledge/ecosystem.md 의 8게임 표와 교차 확인했다.
   ⚠ 앞부분 일치라 표식끼리 접두가 겹치면 순서가 의미를 갖는다
   (같은 표의 사고: SAMURAI2 를 SAMURAI 보다 먼저 봐야 한다).
   "KOF R2" 는 다른 표식의 접두가 아니므로 자리에 자유가 있다.
   ⚠ 한글패치본도 헤더가 같다 — 원본·한글판 모두에서 1 이 된다. 의도한 것이다. */
static int kof_is_rom;

void kofsp_set_rom(const void *rom, unsigned len)
{
   kof_is_rom = rom && len >= 0x30 &&
                !memcmp((const unsigned char *)rom + 0x24, "KOF R2", 6);
   kofsp_reset();
}

int kofsp_rom_ok(void) { return kof_is_rom; }

/* ── 엔진 토글 ───────────────────────────────────────────────────
   기본 꺼짐. 켜도 M1 에서는 하는 일이 없다 — 배관을 먼저 증명하고 얹는다. */
static int kof_engine_on;

void kofsp_set_engine(int on) { kof_engine_on = on ? 1 : 0; }
int  kofsp_engine_on(void)    { return kof_engine_on; }

void kofsp_reset(void)
{
   /* 진행 중이던 커맨드가 아직 없다. 상태가 생기면 여기서 버린다. */
}

#ifdef SS2SP_RAM_POINTER
static void *kof_ram;
void kofsp_set_ram(void *ram) { kof_ram = ram; (void)kof_ram; }
#endif

/* ── 매 프레임 ───────────────────────────────────────────────────
   M1: **순정 롬 폴드와 똑같이** 접는다. libretro.c 의 else 가지에 있던 것을
   그대로 옮겨 온 것이다 — 그래야 kofsp 가지를 그 앞에 끼워도 KOF 의 조작이 안 바뀐다.

   비트 자리는 libretro.c 의 원시 지도와 같다: NGP A = bit4, NGP B = bit5.
   (레트로패드로는 B→A, A→B 로 뒤집혀 있다. 여기서 쓰는 Y·X·L·R 은 그 위에 얹는 몫이다.) */
#define NGP_A (1 << 4)
#define NGP_B (1 << 5)

/* 레트로패드 비트 — libretro.h 를 끌어오지 않으려고 값만 적는다.
   순서: B A Y SELECT START UP DOWN LEFT RIGHT X L R … */
#define RP_Y 1
#define RP_X 9
#define RP_L 10
#define RP_R 11

uint8_t kofsp_frame(uint8_t pad, uint16_t ret)
{
   if (ret & (1u << RP_Y)) pad |= NGP_A;
   if (ret & (1u << RP_X)) pad |= NGP_B;
   if ((ret & (1u << RP_L)) || (ret & (1u << RP_R)))
      pad |= (uint8_t)(NGP_A | NGP_B);

   /* 엔진 본체가 들어올 자리. 지금은 아무것도 안 한다 —
      오프셋이 하나도 안 측정됐고, 안 잰 것에 기대는 코드를 만들지 않는 것이 규칙이다. */
   return pad;
}
