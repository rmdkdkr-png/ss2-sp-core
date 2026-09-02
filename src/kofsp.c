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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── 램 접근 — ss2sp.c · svcsp.c 와 같은 이중 경로 ───────────────── */
#ifdef SS2SP_RAM_POINTER
static uint8_t *kof_ram_ptr;
void kofsp_set_ram(void *p) { kof_ram_ptr = (uint8_t *)p; }
#define CPUExRAM kof_ram_ptr
#else
extern uint8_t CPUExRAM[16384];
#endif

static int kof_dbg(void)
{
   static int d = -1;
   if (d < 0) { const char *e = getenv("KOFSP_DEBUG"); d = (e && *e == '1'); }
   return d;
}

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

/* 좌우 반전 — **bit7** 이다(SVC 의 `0x092C` bit7 과 같은 꼴).

   ★★ 2026-09-03: **0x0D4A 였다. 틀렸다. 0x0D4C 다.**
   0x0D4A 는 반전이 아니라 **필살기를 한 번 쓰면 서고 안 내려오는 플래그**였다.
   그래서 배포된 엔진은 **한 라운드에 두 번째 발동부터 커맨드가 좌우로 뒤집혔다** —
   236 을 넣으라고 시켰는데 214 가 나갔다. 관문(M2 40/40 · M5 98/98)이 전부 통과한 것은
   **시행마다 세이브 스테이트를 다시 불러왔기 때문**이다. 매번 깨끗한 상태(플래그 0,
   시작 쪽)에서만 쟀으므로 「상태가 쌓여서 생기는 고장」이 구조적으로 안 보였다.
   → 회귀 얼개에 **한 상태에서 연속 2회 발동**을 반드시 넣을 것. 이게 이 사고의 값이다.

   무엇이 잘못됐었나: 옛 근거는 CPU 무대에서 「넘어간 프레임에 P1 0→128, 36프레임 뒤 원복」
   이었다. 36프레임은 **피격 경직 길이**와 같다 — 넘어간 것이 아니라 맞은 것을 본 것이다.
   성긴 관찰에 이야기를 붙였고, 「P2 는 갱신이 안 된다」는 그 이야기를 살리려고 덧댄 예외였다.

   새 근거 — **미지의 바이트에 안 기대는 심판을 먼저 세웠다**:
     걷기 act 가 반전을 말해 준다. R 을 눌렀을 때 **앞걷기 64 면 오른쪽, 뒤걷기 75 면 왼쪽**.
   그 심판으로 무대 넷을 세우고(위상 2종) 16KB 전량을 대조했다:
     A 기준(안 넘음)      R→64   |  B 앞점프로 넘음      R→75
     C 필살기만(안 넘음)  R→64   |  D 넘었다 되넘어옴    R→64
   조건 「B 에서 바뀌고 · C 에서 안 바뀌고 · D 에서 돌아온다」로 16,384 → **11개**.
   그중 0x0D4C 만 **P1 블록 안**이고 차이가 정확히 128(bit7)이다: 16 → 144.
   (나머지는 0x267E/0x268C 표시 사본과 0x28CA~CD 블록, 0x1021~1025 소형 카운터다.)

   ★ 교차 증인: **0x0E8C = 0x0D4C + 0x0140**(P2 스트라이드)가 176 → 48 로 **반대로** 뒤집힌다.
     「P1 과 P2 는 서로 마주본다」 불변식이 성립한다. 옛 주석이 이 불변식을 폐기한 것은
     **틀린 바이트를 보고 있었기 때문**이지 게임이 이상해서가 아니었다.
     정지 더미도 플래그가 살아 있다 — 예외를 만들 필요가 없다.

   ⚠ **좌표 비교로 반전을 판정하지 마라.** SVC 에서 6번 오판한 자리다. 그건 그대로 유효하다.
   ⚠ 이 무대에서 P1 하위 비트는 16, P2 는 48 로 다르다. **bit7 만 보고 나머지는 건드리지 마라.** */
#define OFF_FACE      0x0D4C
#define OFF_FACE2     (OFF_FACE + KOFSP_P2_STRIDE)   /* 교차 증인용 — 판정에는 안 쓴다 */
#define KOFSP_FACE_BIT 0x80

/* ── 방향 이력 링 — **없다** (실측, 2026-09-02) ──────────────────
   SVC 는 0x0CB8~ 에 24칸 링이 있어서 커맨드를 직접 써 넣어 발동을 9→3프레임으로 줄였다.
   KOF R-2 에는 **그런 것이 없다.** 프레임마다 램 16KB 를 전량 떠서(48프레임) 확인했다:
     · 옆 칸이 내 값을 늦게 받는 **시프트 관계가 한 쌍도 없다**
     · 시스템 대역(0x0C00~0x1200)에서 변하는 주소가 **12개뿐**이고
       그중 「입력 때만 몇 번」 변하는 원형 버퍼 후보가 **0개**
   → 링 주입은 못 쓴다. 대신 **찌꺼기 만료(KOFSP_HIST_CLEAR)를 지키는 쪽**으로 간다:
     매크로 전에 방향이 조용한 12프레임을 확보한다. 값은 +12프레임 지연이다. */

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

/* 캐릭터 ID — **로스터 색인 그대로다**(쿄 0 … 사이수 13, 루갈 14).
   쿄 무대 둘과 레오나 무대를 대서 잡았다: 쿄=0 / 레오나=4.
   교차 증인 — 레오나 무대에서 **P2 는 0(쿄) 그대로**였다(화면과 일치).
   P2 는 +0x140 이다. */
#define OFF_CHAR1 0x0D8B
#define OFF_CHAR2 (OFF_CHAR1 + KOFSP_P2_STRIDE)   /* 0x0ECB */

/* PLAYER SELECT 는 **행(팀) × 자리(3인)** 이다 — D 로 행, R 로 자리, B 로 확정.
   실측 지도(2026-09-03, tools/kof/kof_mkchars.py 가 이 표로 상태를 굽는다):
     쿠사나기      쿄0      사이수13  신고11
     초히로인      아테나8   유리9     카스미12
     신사우스타운   료2      마이3     테리1
     오로치        야시로5   셰르미6   크리스7
     에디트        레오나4   *랜덤*    이오리10
   ⚠ **루갈(14)은 이 화면에서 못 고른다.** 그래서 계획의 「15명」은 실제로 **14명**이다.
   ⚠ 에디트 자리1 은 `ランダム` 이라 상태를 구우면 안 된다 — 캐릭터가 실행마다 바뀐다. */

/* 미측정 — 이 값들이 채워져야 엔진이 돈다. 목록 자체가 사냥 목록이다. */
static const int OFF_STYLE     = KOFSP_UNMEASURED;
static const int OFF_TIMER     = KOFSP_UNMEASURED;
/* ── 전투중 판별 — SVC 식 4종은 못 좁혔다. 대신 **근사 하나**를 쓴다 ──
   전투/메뉴를 가르는 바이트가 3,500개나 나온다. 캐릭터를 바꾼 전투(레오나)까지 넣어도
   안 줄었다 — 표본이 전부 같은 무대·같은 순간이라 그렇다. 무대를 바꾼 상태가 더 있어야 한다.

   그동안 쓸 근사: **`OFF_H1`(지면 위 높이)가 0 이 아니면 전투 중**이다.
   실측 5/5 로 갈린다 — 전투(쿄)·전투(쿄,무적오프)·전투(레오나) 전부 24,
   PLAYER SELECT·모드선택 둘 다 0.
   ⚠ **근사다.** 무대가 바뀌면 지상값이 24 가 아닐 수 있고, 다른 메뉴에서 우연히
     0 이 아닐 수도 있다. 이걸로 안전을 논하지 마라 — 잘못 발동할 뿐 깨지지는 않는다.
   ⚠ 폐기한 안: 「P1·P2 의 face bit7 이 다르다」 — **정지 더미에서 거짓이 된다**(위 주석). */
#define KOFSP_INBATTLE_APPROX_OFF  OFF_H1

int kofsp_unmeasured_count(void)
{
   const int *v[] = { &OFF_STYLE, &OFF_TIMER };
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

/* 매크로 진행 상태 — 리셋·스테이트 로드 때 버려야 한다.
   SS2 는 스테이트 로드 뒤 남은 매크로 잔여가 유령 발동을 냈다. */
/* 매크로 한 스텝 — n 프레임 동안 bits 를 넣는다. bits 의 FWD/BAK 는 반전으로 푼다. */
typedef struct { unsigned char n, bits; } Step;

/* 마지막 스텝을 사람이 쥔 만큼 늘리되 이만큼까지만 — 무한정 끌면 다음 입력이 밀린다.
   강 문턱이 6프레임이므로 24 면 충분히 여유롭다. */
#define KOFSP_HOLD_CAP 24

static const Step *mac_tab;
static int mac_step = -1;   /* -1 = 쉬는 중 */
static int mac_left;
static int mac_hold;        /* 마지막 스텝을 몇 프레임 늘렸나 */
static int mac_trig;        /* 매크로가 도는 동안 트리거를 쥔 프레임 수 (약/강 판정) */
static int kof_quiet;       /* 사람이 방향을 안 누른 채 지난 프레임 수 */
static int mac_fwd;         /* 시작할 때 굳힌 「앞」 비트 */
static int trig_prev;

void kofsp_set_engine(int on) { kof_engine_on = on ? 1 : 0; }

/* 기본은 꺼짐. 다만 **`KOFSP_ON=1` 환경변수로도 켠다** —
   이렇게 두면 M2 검증에 `build/libretro.c` 를 손대지 않아도 된다
   (코어 옵션 `ngp_kofsp_engine` 은 배포 단계에서 붙인다).
   공유 트리를 만지는 구간이 짧을수록 동승 사고가 줄어든다. */
int kofsp_engine_on(void)
{
   static int env = -1;
   if (env < 0) { const char *e = getenv("KOFSP_ON"); env = (e && *e == '1'); }
   return kof_engine_on || env;
}

void kofsp_reset(void)
{
   mac_step = -1;
   mac_left = 0;
   mac_hold = mac_trig = 0;
   kof_quiet = 0;
   trig_prev = 0;
}

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

/* 방향 비트 — build/libretro.c 의 원시 지도 순서 그대로다
   (map[] = UP, DOWN, LEFT, RIGHT, B, A, START → 비트 0..6). */
#define NGP_U 0x01
#define NGP_D 0x02
#define NGP_L 0x04
#define NGP_R 0x08

/* ── M2: 하드코딩 매크로 하나 (쿄 236+P) ─────────────────────────
   손으로 넣어 발동을 확인한 그 시퀀스를 그대로 옮긴다: `4 D` → `4 D+앞` → `2 앞+A`.
   ★ 앞에 **조용한 12프레임**을 붙인다 — R1 실측에서 찌꺼기가 다음 입력과 이어 붙고
     12프레임이라야 두 위상 모두 깨끗했다. SVC 처럼 이력 링에 써 넣는 길은
     **이 게임엔 링이 없어서** 못 쓴다. 그래서 기다리는 쪽이 유일한 방법이다.
   ★ 「앞」은 반전에 따라 좌우가 바뀐다. 슬롯은 앞/뒤, 패드는 좌/우다 —
     안 뒤집으면 반전 무대에서 전부 오발로 찍힌다(SVC 에서 75건 허위 전과). */
/* 방향 자리표시 — 실제 좌/우는 반전을 보고 푼다.
   슬롯은 앞/뒤인데 패드는 좌/우다. 안 뒤집으면 반전 무대에서 전부 오발로 찍힌다. */
#define FWD 0x80
#define BAK 0x40

/* ── 기술표 (act 카탈로그 실측, 쿄·스파링 무대) ───────────────────
   ⚠ **짧고 모호하지 않은 커맨드만 쓴다.** 카탈로그에서 확인한 것:
     `41236` 은 `236` 과, `63214` 는 `214` 와 **act 지문이 같다** —
     긴 모션이 끝의 짧은 모션에 흡수된다. 그런 것을 슬롯에 태우면
     「안 나갔다」가 아니라 「다른 게 나갔다」가 되어 판정이 통째로 흐려진다.
   ⚠ 모든 매크로는 **조용한 12프레임**으로 시작한다(찌꺼기 만료). 링이 없으니 이 길뿐이다. */
#define Q { KOFSP_HIST_CLEAR, 0 }

static const Step M_236P[] = { Q, {4,NGP_D}, {4,NGP_D|FWD}, {2,FWD|NGP_A}, {0,0} };
static const Step M_236K[] = { Q, {4,NGP_D}, {4,NGP_D|FWD}, {2,FWD|NGP_B}, {0,0} };
static const Step M_214P[] = { Q, {4,NGP_D}, {4,NGP_D|BAK}, {2,BAK|NGP_A}, {0,0} };
static const Step M_623P[] = { Q, {4,FWD},   {4,NGP_D}, {4,NGP_D|FWD}, {2,NGP_A}, {0,0} };
static const Step M_421K[] = { Q, {4,BAK},   {4,NGP_D}, {4,NGP_D|BAK}, {2,NGP_B}, {0,0} };
/* 초필 — 카탈로그에서 **피해 19** 로 가장 컸고 버튼·강약에 무관했다(네 갈래가 같은 지문) */
static const Step M_SUPER[] = { Q, {4,NGP_D}, {4,NGP_D|BAK}, {4,BAK}, {4,NGP_D|BAK},
                                {4,NGP_D}, {4,NGP_D|FWD}, {2,FWD|NGP_A}, {0,0} };

/* 슬롯 — 트리거를 누를 때 **잡고 있던 방향**으로 고른다.
   비어 있으면 완전 무반응(0 을 넣지 않고 아예 매크로를 시작하지 않는다). */
enum { SLOT_N, SLOT_F, SLOT_B, SLOT_D, SLOT_DF, SLOT_DB, SLOT_AIR, SLOT_MAX };
static const Step *const SLOTS[SLOT_MAX] = {
   M_236P,    /* N   중립 — 장풍 자리 */
   M_623P,    /* F   앞  — 대공 (vx 222 로 전진하는 것을 확인) */
   M_214P,    /* B   뒤  */
   M_421K,    /* D   아래 — 피해 7 */
   M_SUPER,   /* DF  앞아래 — 초필 (피해 19) */
   M_236K,    /* DB  뒤아래 */
   M_236P,    /* AIR 공중 */
};

static int kof_forward_bit(void)
{
   /* 반전은 P1 의 것만 믿는다 — 「うごかない」 더미는 플래그가 갱신되지 않는다. */
   if (!CPUExRAM) return NGP_R;
   return (CPUExRAM[OFF_FACE] & KOFSP_FACE_BIT) ? NGP_L : NGP_R;
}

uint8_t kofsp_frame(uint8_t pad, uint16_t ret)
{
   int trig = (ret & (1u << RP_R)) ? 1 : 0;

   /* 방향이 조용히 지난 프레임 수 — 대기를 건너뛸지 정하는 값이다.
      ★ 매크로가 도는 동안은 **0 으로 눌러 둔다.** 얼려 두면 안 된다:
        매크로가 끝난 직후 게임 이력에는 **매크로 자신이 넣은 모션**이 그대로 남아 있는데,
        카운터가 매크로 전의 큰 값을 유지하면 **곧바로 또 누를 때 대기를 건너뛴다.**
        그러면 두 번째가 첫 번째 찌꺼기와 이어 붙어 엉뚱한 기술이 나간다.
        「엔진이 넣은 방향은 사람 것이 아니니 안 세도 된다」가 처음 생각이었는데,
        게임 입장에서는 **누가 넣었든 같은 이력**이다. */
   if (mac_step >= 0)
      kof_quiet = 0;
   else
      kof_quiet = (pad & (NGP_U | NGP_D | NGP_L | NGP_R)) ? 0 : kof_quiet + 1;

   /* 순정 폴드 — 트리거로 돌린 R 은 빼고 그대로 둔다.
      M1 에서 「출력 비트 동일」을 지키던 그 접기다. L 은 계속 A+B. */
   if (ret & (1u << RP_Y)) pad |= NGP_A;
   if (ret & (1u << RP_X)) pad |= NGP_B;
   if (ret & (1u << RP_L)) pad |= (uint8_t)(NGP_A | NGP_B);

   if (!kofsp_engine_on() || !kof_is_rom) { trig_prev = trig; return pad; }

   if (trig && !trig_prev && mac_step < 0)
   {  /* 엣지에서만 시작한다. 누르고 있는 동안 되풀이 발동하면 누출이 된다. */
      int fwd = kof_forward_bit();
      int back = (fwd == NGP_R) ? NGP_L : NGP_R;
      int held_f = (pad & fwd) != 0, held_b = (pad & back) != 0;
      int held_d = (pad & NGP_D) != 0;
      int air = CPUExRAM && CPUExRAM[OFF_H1] != KOFSP_H_GROUND;
      int slot;

      if (air)                     slot = SLOT_AIR;
      else if (held_d && held_f)   slot = SLOT_DF;
      else if (held_d && held_b)   slot = SLOT_DB;
      else if (held_d)             slot = SLOT_D;
      else if (held_f)             slot = SLOT_F;
      else if (held_b)             slot = SLOT_B;
      else                         slot = SLOT_N;

      if (SLOTS[slot])             /* 빈 슬롯 = 완전 무반응 */
      {
         mac_tab  = SLOTS[slot];
         mac_step = 0;
         mac_left = mac_tab[0].n;
         mac_hold = 0;
         mac_trig = 0;
         /* ── 조용했으면 기다리지 않는다 ────────────────────────────
            0번 스텝은 **찌꺼기가 만료되기를 기다리는** 칸이다. 그런데 사람이
            직전에 방향을 안 눌렀다면 만료될 찌꺼기가 애초에 없다.
            그때까지 기다리면 발동이 +23프레임인데, 건너뛰면 **+11** 이 된다.
            ⚠ 슬롯 자체가 방향을 요구하는 경우(F·B·D·DF·DB)는 지금 그 방향을 쥐고 있으므로
              조용할 수가 없다 — 그 슬롯들은 자연히 기다린다. 이득은 주로 N·AIR 에서 난다. */
         if (kof_quiet >= KOFSP_HIST_CLEAR)
            mac_left = 1;
         mac_fwd  = fwd;
         if (kof_dbg())
            fprintf(stderr, "[kofsp] 슬롯 %d 시작 (앞=%s%s)\n", slot,
                    fwd == NGP_R ? "R" : "L", air ? ", 공중" : "");
      }
   }
   trig_prev = trig;

   if (mac_step >= 0)
   {
      unsigned char b = mac_tab[mac_step].bits;
      int back = (mac_fwd == NGP_R) ? NGP_L : NGP_R;
      int last = (mac_tab[mac_step + 1].n == 0);
      /* 매크로가 도는 동안 사람 입력은 버린다 — 섞으면 커맨드가 오염된다 */
      pad = (uint8_t)((b & 0x3F)
                      | ((b & FWD) ? mac_fwd : 0)
                      | ((b & BAK) ? back : 0));

      /* ── 강약: **사람이 누른 길이를 그대로 게임 버튼 길이로 옮긴다** ──
         엔진이 「이쯤이면 강이겠지」로 채우면 사람의 탭이 약과 강으로 갈린다
         (SVC 가 근거 없는 5 로 그렇게 됐다).

         ★ 그런데 그냥 「트리거를 쥔 동안 버튼을 유지」로는 **모자란다.**
           매크로 앞머리(조용한 12 + 모션 8 = 20프레임) 동안 사람의 홀드가 다 소모되어,
           마지막 스텝에 남는 것이 몇 프레임뿐이다. 실측: 24프레임을 쥐어도
           **홀드 카운터가 2에서 멈췄다**(강은 3이 필요하다).
         → 그래서 **앞머리를 측정 창으로 쓴다.** 매크로가 도는 동안 트리거를 쥔 프레임을
           세고, 마지막 스텝에 들어갈 때 그 수로 약/강을 정해 **버튼 길이를 직접 준다.**
           문턱은 실측값(6프레임 = 카운터 3)을 그대로 쓴다. */
      if (trig) mac_trig++;
      if (last && trig && mac_left <= 1 && mac_hold < KOFSP_HOLD_CAP)
      {
         mac_hold++;
         return pad;          /* 트리거를 계속 쥐고 있으면 더 유지한다 */
      }
      if (--mac_left <= 0)
      {
         mac_step++;
         if (mac_tab[mac_step].n == 0) mac_step = -1;      /* 표 끝 */
         else
         {
            mac_left = mac_tab[mac_step].n;
            /* 마지막 스텝(버튼이 들어가는 칸)에 들어설 때 약/강을 정한다.
               앞머리에서 트리거를 문턱만큼 쥐고 있었으면 버튼을 길게 준다. */
            if (mac_tab[mac_step + 1].n == 0 && mac_trig >= KOFSP_HOLD_STRONG)
               mac_left = KOFSP_HOLD_STRONG + 4;   /* 카운터가 3을 넘기도록 여유 */
         }
      }
   }
   return pad;
}
