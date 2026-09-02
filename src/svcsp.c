/* ═══════════════════════════════════════════════════════════════════
   svcsp.c — SNK vs. Capcom MotM (NGPC) 원버튼 필살기 엔진

   ss2sp.c 의 동생. 구조는 같지만 SvC 실측(tools/svc/SVC_MEMO.md)에 맞춰
   훨씬 단순하다:
     · 가로채기가 없다 — SS2 의 램 리셋 트릭(0x1DCD)이 필요 없다. 입력만 넣으면 된다
     · 강/약이 없다 — 버튼 유지 시간 판정이 없어서 sustain/hold 경로가 없다
     · 커맨드 창이 좁다 — 방향 간격 2~4프레임 (기본 3), 마지막 방향을
       버튼보다 **먼저** 잡고, 잡은 채로 버튼을 누른다 (실측 §7)

   좌표 규약: 모든 오프셋은 SYSTEM_RAM(CPUExRAM) 기준. CPU 주소 = 0x4000 + 오프셋.
   이 게임엔 간이입력이 원래 없어서 (BATTLE CONFIG 에 ABLE 없음, 실측 §6)
   이 엔진이 이 게임 최초의 간이입력이 된다.
   ═══════════════════════════════════════════════════════════════════ */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int svc_dbg(void){ static int d=-1; if(d<0){const char*e=getenv("SVCSP_DEBUG"); d=(e&&*e=='1');} return d; }

/* ── 램 접근 — ss2sp.c 와 같은 이중 경로 ─────────────────────────── */
#ifdef SS2SP_RAM_POINTER
static uint8_t *svc_ram_ptr;
void svcsp_set_ram(void *p) { svc_ram_ptr = (uint8_t *)p; }
#define CPUExRAM svc_ram_ptr
#else
extern uint8_t CPUExRAM[16384];
#endif

/* ── RAM 맵 (tools/svc 실측) ─────────────────────────────────────── */
#define OFF_CHAR1   0x08A0   /* P1 캐릭터 ID. 쿄=0 … 가일=17. 선택화면=255 */
#define OFF_CHAR2   0x08C0   /* P2 캐릭터 ID (P1 블록 +0x20) */
#define OFF_STYLE   0x08BE   /* P1 유파. 반격=0 균형=1 속공=2 */
#define OFF_HP1     0x08B3   /* P1 체력 (피격 이벤트 때 0x08AF 와 동시 갱신) */
#define OFF_HP2     0x08CF   /* P2 체력 (표시 사본 — 읽기엔 충분) */
#define OFF_TIMER   0x08EE   /* 라운드 타이머 (10진). 메뉴=0, GO 연출=60 */
#define OFF_FACE    0x092C   /* ★ P1 속성 — 비트 7(0x80) 이 좌우 반전 플래그다.
                                왼쪽에 서면 16, 상대를 넘어 오른쪽에 서면 144.
                                실측: 스파링 「적 동작=정지」로 더미를 세우고 앞점프로 넘었다
                                되넘어왔다 반복하며 램 5판을 떠서, 왼(3판)/오른(2판)이 정확히
                                갈리는 바이트만 남긴 것. 세이브 36개 전수 36/36.
                                좌표 비교는 6개에서 오판했다 — 적 좌표는 스테이지·상황에 따라
                                기준이 흔들려 쓸 수 없다. 옛 OFF_X1(0x092E)은 카메라 쪽이라
                                상대를 넘어가도 128 에서 멈춰 좌우가 영영 안 뒤집혔다. */
#define OFF_X1      0x0934   /* P1 X (16비트) — 거리 판정용 */
#define OFF_Y1      0x0930   /* P1 Y. 지상=128, 점프 정점=86 */
#define OFF_BANK    0x09AD   /* P1 애니 뱅크. 대기·평타=255, 필살기는 기술별 값(0 포함!) */
#define OFF_ACT     0x0968   /* P1 동작 ID. 뱅크가 같은 기술을 가른다 (황 157 / 독 168) */
#define OFF_ANIM    0x0C7E   /* P1 애니 카운터. 대기·걷기·앉기=127, 동작 중=카운트업 */

/* ── 패드 비트 (libretro.c input_buf 인코딩) ─────────────────────── */
#define PAD_UP    0x01
#define PAD_DOWN  0x02
#define PAD_LEFT  0x04
#define PAD_RIGHT 0x08
#define PAD_A     0x10          /* NGP A = PUNCH (게임 설정 화면 확인) */
#define PAD_B     0x20          /* NGP B = KICK */
#define PAD_DIR_MASK (PAD_UP|PAD_DOWN|PAD_LEFT|PAD_RIGHT)

/* ── 타이밍 (실측 §2·§7: 창 2~4프레임, 5 이상은 시간 초과) ────────── */
static int svc_step_frames(void){ static int v=-1; if(v<0){const char*e=getenv("SVCSP_STEP"); v=e?atoi(e):2;} return v; }
/* 마지막 **두 칸**을 뺀 앞 방향 칸들의 길이.
   ⚠️ 이 값을 한 번 1 → 2 로 **잘못 되돌린 적이 있다.** 그때 근거는
     「EARLY=1 은 뱅크 21, EARLY=2 는 뱅크 22 니까 다른 기술이 나간다」였다. 틀렸다.
     **뱅크(0x09AD)는 기술의 신원이 아니다.** 같은 입력이라도 게임의 2프레임 입력
     샘플링 위상이 한 프레임 밀리면 21 ↔ 22 로 바뀌고, 황물기와 독물기가 **둘 다 21**
     로 나오기도 한다(두 위상 검증표). 뱅크로 기술을 가르려던 것 자체가 잘못이었다.
   ★ 기술을 가르는 건 act(0x0968) 다. 피해로는 절대 가르지 마라 — 거리·타이밍에 흔들린다.
     (SS2 는 0x0E3E 가 16비트 액션ID 고 필살기 ≥ 0x180 이다. SvC 도 16비트일 수 있다 —
      황 0x9C · 독 0xA8 이 상위 바이트 1이면 0x19C · 0x1A8 로 그 규칙에 들어맞는다.
      8비트로도 황 156 / 독 168 이 갈려서 지금은 그대로 쓴다.)
   EARLY=1 이면 펀치가 커맨드 5프레임째에 들어가 **발동 7프레임**, 2 면 6프레임째라 9다.
   순정 실측(쿄 236+P): 펀치를 4·5 에 누르면 발동 7 │ 6·7 이면 9 │ 8 이면 커맨드 밖.

   ★ 그런데도 기본값은 **2** 다. 1 은 쿄 236+P(3칸)에서만 이득이고 **더 긴 커맨드를 깬다.**
     회귀(`tools/svc/sprun.py`) 대조 — EARLY=2 는 18/23, EARLY=1 은 14/23:
       오니야키(↓쥔 채)  피해 5 → 1
       료 아래 슬롯      즉시 → 36프레임 뒤에야 나옴
       류 중립          피해 7 → 0
     앞 칸을 줄이면 칸이 많은 모션은 게임이 못 받는다. 3칸에서 잰 값을 전부에 적용한 것이
     잘못이었다. 살리려면 **모션 길이별로** 갈라야 한다 — 3칸만 1, 나머지 2.
     그건 모션 길이마다 따로 재고 회귀를 통과시킨 뒤에 한다. */
static int svc_early_frames(void){ static int v=-1; if(v<0){const char*e=getenv("SVCSP_EARLY"); v=e?atoi(e):2;} return v; }
static int svc_hold_frames(void){ static int v=-1; if(v<0){const char*e=getenv("SVCSP_HOLD"); v=e?atoi(e):2;} return v; }
static int svc_tail_frames(void){ static int v=-1; if(v<0){const char*e=getenv("SVCSP_TAIL"); v=e?atoi(e):1;} return v; }

/* ── 버튼 레이아웃 (기본): 약은 원래 B·A, 강은 전용 버튼으로.
   실측 §30: 홀드 6f 이하 = 약, 8f 이상 = 강 — 12f 주입으로 여유.
   원버튼 모션 엔진은 기본 꺼짐: 순정 ABLE(アバレ) 모드가 그 역할을 대신한다(§29). */
#define SVC_HOLD_STRONG 12
#define SVC_TAP_MAX     5       /* (미사용 — 옛 추측 문턱) */
/* ═══ 문턱 파생 — 버튼 길이 경계는 **롬에서 읽은 문턱 하나**에서만 나온다 ═══
   게임 카운터는 2프레임당 1 오르고, ++ 결과가 문턱과 같아지는 순간 강을 래치한다.
     thr          = 롬 0x000D4E (원본 4 · 문턱패치 2). svcsp_set_rom 이 읽어 둔다
     강 자연 래치   = 버튼 2×thr 프레임      (원본 8 · 패치 4)
     약 안전 상한   = 버튼 2×thr − 2 프레임  (원본 6 · 패치 2. 그 사이 1프레임은 위상 회색)
   ⚠️ v3.29 사고의 교훈: 이 값들이 하드코딩 사본으로 5군데 흩어져 있다가 문턱 패치가
   게임 쪽 판정선을 옮기자 전부 어긋났다(약이 강으로 새고, 갈래가 뒤섞였다).
   사본 금지 — 반드시 아래 함수로만 얻는다. */
static int svc_rom_thr_read;   /* svcsp_set_rom 이 채운다. 0 = 아직 안 읽음 → 원본 4 */
static int svc_thr(void)      { return svc_rom_thr_read ? svc_rom_thr_read : 4; }
static int svc_btn_strong(void){ return 2 * svc_thr(); }          /* 이 길이부터 강 */
static int svc_btn_weak(void) { return 2 * svc_thr() - 2; }       /* 여기까지 약 */
/* 파생 홀드 갈래 판정 — 게임 문턱과 독립인 엔진 판단이라 따로 되올릴 수 있다.
   기본은 게임과 같은 경계. SP 4프레임 경계가 손에 빡빡하면 SVCSP_SPSTRONG 으로. */
static int svc_sp_strong(void)
{ static int v=-1; if(v<0){const char*e=getenv("SVCSP_SPSTRONG"); v=e?atoi(e):0;}
  return v > 0 ? v : svc_btn_strong(); }
#define SVC_STRONG_MIN  svc_btn_strong()
#define SVC_SP_STRONG   svc_sp_strong()
#define SVC_LAND_WIN    32      /* 착지 선입력 창 — 점프 정점(착지 −28f)에 눌러도 산다.
                                   18 이던 시절 정점 입력이 창 만료로 죽었다(실측) */
#define SVC_WEAK_MAX    svc_btn_weak()   /* 강판이 없는 기술의 버튼 상한 — 길게 잡으면 불발 */
#define SVC_HOLD_MIN    2       /* 1프레임은 위상에 따라 씹힌다 */
static int svc_engine = -1;                     /* 원버튼 엔진 — 메뉴에서만 켠다 */
static int svc_native_basics;                   /* 앱 모드 — 기본기는 순정 통과(탭 약/홀드 강) */
static int svc_basics_split = 1;                /* 옵션 — 약/강 4버튼 리맵. 끄면 순정 2버튼 */
void svcsp_set_basics(int on) { svc_basics_split = !!on; }
static int svc_engine_now(void)
{
   if (svc_engine < 0) { const char *e = getenv("SVCSP_FORCE"); svc_engine = (e && *e == '1'); }
   return svc_engine;
}
void svcsp_set_engine(int on) { svc_engine = !!on; }
int  svcsp_engine_on(void)    { return svc_engine_now(); }
#define STEP_FRAMES  svc_step_frames()
#define EARLY_FRAMES svc_early_frames()
/* 방향을 「스쳤다」고 볼 최대 유지 길이. 이보다 오래 잡고 있었으면 걷기·앉기로 보고
   이어받지 않는다. SVCSP_GRAZE 로 조절(0 이면 이어받기를 통째로 끈다). */
static int svc_graze(void){ static int v=-1; if(v<0){const char*e=getenv("SVCSP_GRAZE"); v=e?atoi(e):6;} return v; }
/* 파생(렛카) 재입력 창 — 매크로가 끝난 뒤 몇 프레임 받아 주는가.
   실측: 이 값이 창의 실제 한계였다. 34 일 때 1타→2타가 36프레임에서 끊겼다. */
static int svc_chain_win(void){ static int v=-1; if(v<0){const char*e=getenv("SVCSP_CHAIN"); v=e?atoi(e):34;} return v; }
/* 파생과 파생 **사이**의 최소 간격. 엔진이 창이 열리자마자 쏘면 게임이 안 받아 준다 —
   실측: 연타 간격 0~12 는 3타가 헛돌아 콤보 2, 16 이상이라야 콤보 3 이었다.
   엔진 로그상 벌읊기까지 컴파일은 되고 있었으니 판단이 아니라 **시각**의 문제다. */
static int svc_chain_gap(void){ static int v=-1; if(v<0){const char*e=getenv("SVCSP_CGAP"); v=e?atoi(e):8;} return v; }
#define DIR_GRAZE    svc_graze()
#define HOLD_FRAMES  svc_hold_frames() /* 탭 = 약. 마지막 방향 유지한 채 버튼 (기본 2 — 실측 최소) */
#define MAX_HOLD     20         /* 트리거를 잡고 있으면 여기까지 늘어난다 = 강.
                                   실측: 16f 홀드 = 독물기(지속 92f+, 전진 2.4배). 짧으면 황물기 */
#define TAIL_FRAMES  svc_tail_frames()
#define MAX_STEPS    16
#define PENDING_FRAMES 150       /* 착지·경직 대기 상한 — MotM 평타 회복 후 여유 */

/* ── 기술표 — 자동 생성 헤더 (tools/svc/gen_svc_moves.py ← moves.json) ──
   모션 바이트 = 패드 비트 (2=0x02 3=0x0A 6=0x08 1=0x06 4=0x04), 오른쪽 볼 때 기준.
   flags: 1=근접 4=공중 8=미검증 16=모으기(첫 방향을 CHARGE_FRAMES 유지) */
#include "svcsp_moves.h"

#define CHARGE_FRAMES 40        /* (옛값 — 엔진이 대신 모아 주던 시절의 주입 길이) */
/* 게임이 요구하는 **진짜** 모으기 길이 — 원거리 판정(탄이 날아가 늦게라도 맞는가)으로
   재실측(2026-09-02): 거리 고정 후 제자리 모음 n 스윕, 두 위상 —
     n28 이하 → 백너클(근접판 act 15~17 / 원거리판 37~38, 탄 없음)
     n30 이상 → 소닉붐(act 2 모션, 원거리 히트 +90)
   ⚠️ 옛 "6프레임" 실측은 37/38 을 붐으로 오인한 판정 오염이었다. 투사체 판별은
   반드시 원거리에서(근접은 몸타와 구분 불가). */
#define SVC_CHARGE_MIN 30

/* 슬롯 인덱스: N F B D DF DB AIR */
enum { SL_N, SL_F, SL_B, SL_D, SL_DF, SL_DB, SL_AIR, SL_COUNT };

static int svc_in_battle(void);   /* 아래에 정의 — cur_char 가 먼저 쓴다 */

/* ── 런타임 슬롯 — 오버레이 메뉴에서 바꿀 수 있게 표를 복사해 둔다 ── */
static signed char svc_slot_run[SVC_CHAR_COUNT][7];
static int svc_slot_ready;
static int svc_slots_dirty;        /* 편집됨 — 프론트가 파일로 흘려보낸다 */
static void svc_slots_ensure(void)
{
   int c, k;
   if (svc_slot_ready) return;
   for (c = 0; c < SVC_CHAR_COUNT; c++)
      for (k = 0; k < 7; k++) svc_slot_run[c][k] = svc_chars[c].slots[k];
   svc_slot_ready = 1;
}

/* ── 오버레이 메뉴용 조회 API (ss2sp 의 style API 와 같은 역할) ── */
int svcsp_char_count(void) { return SVC_CHAR_COUNT; }
const char *svcsp_char_name(int c)
{ return (c >= 0 && c < SVC_CHAR_COUNT) ? svc_chars[c].name : ""; }
int svcsp_cur_char(void)
{
#ifdef SS2SP_RAM_POINTER
   if (!CPUExRAM) return -1;
#endif
   if (!svc_in_battle()) return -1;
   return CPUExRAM[OFF_CHAR1];
}
int svcsp_move_count(int c)
{ return (c >= 0 && c < SVC_CHAR_COUNT) ? svc_chars[c].n : 0; }
const char *svcsp_move_name(int c, int i)
{
   if (c < 0 || c >= SVC_CHAR_COUNT || i < 0 || i >= svc_chars[c].n) return "";
   return svc_chars[c].mv[i].name;
}
int svcsp_move_flags(int c, int i)
{
   if (c < 0 || c >= SVC_CHAR_COUNT || i < 0 || i >= svc_chars[c].n) return 0;
   return svc_chars[c].mv[i].flags;
}
/* 넘패드 표기 (236+P). 모으기는 첫 방향 두 번 (표시 규약 = 토스트와 동일) */
int svcsp_move_notation(int c, int i, char *out, int cap)
{
   static const struct { unsigned char pad; char ch; } tab[] = {
      {0x01,'8'},{0x02,'2'},{0x04,'4'},{0x08,'6'},
      {0x09,'9'},{0x05,'7'},{0x0A,'3'},{0x06,'1'},
   };
   const svc_move *m; int k, j, n = 0;
   if (!out || cap <= 0) return 0;
   out[0] = 0;
   if (c < 0 || c >= SVC_CHAR_COUNT || i < 0 || i >= svc_chars[c].n) return 0;
   m = &svc_chars[c].mv[i];
   for (k = 0; k < m->len && n < cap - 4; k++)
   {
      char ch2 = 0;
      for (j = 0; j < (int)(sizeof tab / sizeof tab[0]); j++)
         if (tab[j].pad == m->motion[k]) { ch2 = tab[j].ch; break; }
      if (!ch2) continue;
      out[n++] = ch2;
      /* 모으기: 「4모아6+P」 — 화살표 변환을 거치면 「←모아→+P」. (옛 표기 「첫 방향
         두 번」은 ←←→ 처럼 보여 커맨드가 이상하다는 제보) */
      if (k == 0 && (m->flags & 16) && n < cap - 10)
         { memcpy(out + n, "모아", 6); n += 6; }
   }
   if (n < cap - 3) { out[n++] = '+'; out[n++] = (m->btn & PAD_A) ? 'P' : 'K'; }
   out[n] = 0;
   return n;
}
int svcsp_get_slot(int c, int k)
{
   svc_slots_ensure();
   if (c < 0 || c >= SVC_CHAR_COUNT || k < 0 || k >= 7) return -1;
   return svc_slot_run[c][k];
}
void svcsp_set_slot(int c, int k, int mv)
{
   svc_slots_ensure();
   if (c < 0 || c >= SVC_CHAR_COUNT || k < 0 || k >= 7) return;
   if (mv >= svc_chars[c].n || mv < 0) mv = -1;
   svc_slot_run[c][k] = (signed char)mv;
   svc_slots_dirty = 1;
}
void svcsp_reset_slots(void) { svc_slot_ready = 0; svc_slots_ensure(); svc_slots_dirty = 1; }

/* ── 러시 마무리 픽커 — 대표 초필(flags 32)과, 게이지 불발 시 낼 필살기 ── */
static const svc_move *svc_pick_super(int chr)
{
   int i;
   if (chr < 0 || chr >= SVC_CHAR_COUNT || !svc_chars[chr].mv) return 0;
   for (i = 0; i < svc_chars[chr].n; i++)
   {
      const svc_move *m = &svc_chars[chr].mv[i];
      if ((m->flags & 32) && !(m->flags & (4 | 8))) return m;
   }
   return 0;
}
static const svc_move *svc_pick_fallback(int chr)
{
   int mi, i;
   if (chr < 0 || chr >= SVC_CHAR_COUNT || !svc_chars[chr].mv) return 0;
   svc_slots_ensure();
   mi = svc_slot_run[chr][SL_N];
   if (mi >= 0 && mi < svc_chars[chr].n && !(svc_chars[chr].mv[mi].flags & (4 | 32)))
      return &svc_chars[chr].mv[mi];
   for (i = 0; i < svc_chars[chr].n; i++)
   {
      const svc_move *m = &svc_chars[chr].mv[i];
      if (!(m->flags & (1 | 4 | 8 | 32))) return m;
   }
   return 0;
}

/* ── 슬롯 배치 내보내기/들여오기 — <system>/ngpsvc_slots.bin (파일 IO 는 프론트) ── */
int svcsp_slots_dirty(void) { int d = svc_slots_dirty; svc_slots_dirty = 0; return d; }
int svcsp_slots_export(unsigned char *buf, int cap)
{
   int c, k, n = 0;
   if (!buf || cap < 4 + SVC_CHAR_COUNT * 7) return 0;
   svc_slots_ensure();
   /* 판 'NSV2' — v3.31 에서 기본 배치를 정배(N=장풍·F=대공·B=용권)로 갈았다.
      옛 저장본(NSV1)을 그대로 불러오면 새 기본이 영영 안 보이므로 판을 올려 버린다. */
   buf[n++] = 'N'; buf[n++] = 'S'; buf[n++] = 'V'; buf[n++] = '2';
   for (c = 0; c < SVC_CHAR_COUNT; c++)
      for (k = 0; k < 7; k++) buf[n++] = (unsigned char)svc_slot_run[c][k];
   return n;
}
void svcsp_slots_import(const unsigned char *buf, int len)
{
   int c, k, n = 4;
   if (!buf || len < 4 + SVC_CHAR_COUNT * 7) return;
   if (buf[0] != 'N' || buf[1] != 'S' || buf[2] != 'V' || buf[3] != '2') return;
   svc_slots_ensure();
   for (c = 0; c < SVC_CHAR_COUNT; c++)
      for (k = 0; k < 7; k++)
      {
         signed char v = (signed char)buf[n++];
         if (v < -1 || v >= (signed char)svc_chars[c].n) v = -1;   /* 표가 줄었을 때 방어 */
         svc_slot_run[c][k] = v;
      }
}

/* 기술표가 없는 캐릭터 → 그냥 펀치 */
static const unsigned char mo_basic[] = {0x00};
static const svc_move svc_basic = { "펀치", 0, mo_basic, 1, PAD_A, 0, -1, -1, -1 };

/* ── 상태 ────────────────────────────────────────────────────────── */
typedef struct { uint8_t pad; uint8_t frames; uint8_t sustain; } svc_step;
static svc_step  q[MAX_STEPS];
static int       q_n, q_i, q_left;
static uint16_t  prev_trig;
static uint16_t  prev_pad_dir;
static const svc_move *pending;
static int       pending_left;
static int       pending_kind;     /* 0 일반(착지·회복 대기) 1 캔슬 선입력(히트 순간 발사) 2 파생 4 캔슬창 회피(dud) */
static int       warm;             /* 전투 게이트 연속 프레임 */
static int       verify_left;
static int       svc_is_rom;       /* 헤더 판별 결과 */
static int       hold_elapsed;     /* 버튼 스텝 유지 누적 (강약 판정) */
static const svc_move *chain_tbl;  /* 마지막 발동 기술의 소속 표 (파생 인덱스 해석용) */
static const svc_move *chain_mv;   /* 마지막 발동 기술 */
static int       chain_left;       /* 파생 입력 창 (매크로 끝난 뒤 프레임) — 실측 +2~36f */
static uint8_t   act_at_compile;   /* 컴파일 시점 act — 부모 발동 증거 판정 기준 */
static int       chain_saw_act;    /* 컴파일 후 「새 비중립 act」를 봤는가 = 부모가 실제 발동 */
static int       chain_queue;      /* 창 열리기 전에 겹쳐 누른 파생 입력 수 (연타 흡수) */
static uint32_t  frames;           /* 엔진 프레임 카운터 */
static uint32_t  my_attack_at;     /* 유저가 직접 평타(A/B)를 누른 프레임 — 킬캔슬 판정 */
static uint32_t  hit_at;           /* P2 체력이 깎인 프레임 — 히트 캔슬만 허용 (헛침 즉시주입은 leak) */
static uint32_t  move_started;     /* 마지막 기술 시작 프레임 */
static const svc_move *retry_mv;   /* 불발 재시도 대상 (파생 컴파일은 제외) */
static int       retry_cnt;
static uint32_t  retry_at;
static uint32_t  macro_end_at;
static int       compile_no_retry; /* 파생 등 재시도 금지 컴파일 표시 */
static uint8_t   bank_at_compile;  /* 컴파일 시점 뱅크 — 재시도 성공 판정은 뱅크 변화로
                                      (씹힌 평타도 카운터는 리셋해서 카운터만으론 속는다) */
static uint8_t   prev_pad_btn;
static int       trig_len;         /* 트리거를 이번에 몇 프레임 눌렀나 (강약 판정용) */
static uint8_t   charge_dir;       /* 모으기 기억 — 충분히(≥SVC_CHARGE_MIN) 잡았던 마지막 방향 */
static uint32_t  charge_len;       /* 그 방향을 잡았던 길이 */
static uint32_t  charge_end_at;    /* 놓은(바뀐) 프레임 — 유예 20f 안에서만 유효 */
static uint8_t   dir_latch;        /* 마지막으로 잡았던 방향 */
static uint32_t  dir_latch_at;     /* 그 프레임 */
static uint32_t  dir_hold_from;    /* 그 방향을 잡기 시작한 프레임 */
static uint32_t  dir_hold_len;     /* 그 방향을 **얼마나 잡고 있었나** — 뗀 뒤에도 남는다.
                                      뗄 때 지워 버리면 정작 필요한 순간에 값이 없다(한 번 밟음) */
static int       dir_held;         /* 지금 방향을 잡고 있는가 */
static int       attack_was_kick;  /* 마지막 자기 노멀이 킥(B)이었나 — 쿄 dud 는 킥 캔슬만 */
static uint8_t   prev_hp2v;
/* ── 모던 자동 콤보 상태 (러시/어시스트/기술키 판별) ── */
static uint16_t  prev_ret;         /* 물리 비트 이전 프레임 — 기본기 엣지 검출 */
static uint32_t  bas_last_at;      /* 마지막 기본기 눌림 (연타 창 판정) */
static int       rush_n;           /* 연타 단계. 0=없음 */
static uint8_t   rush_prev_btn;    /* 직전 연타 계열 (0x10 P / 0x20 K) — 교대용 */
static uint32_t  rush_hit0;        /* 러시 시작 시점의 hit_at — 이후 히트 여부 */
static const svc_move *rush_fb;    /* 마무리 초필 불발 시 갈아탈 필살기 */
static uint8_t   rush_conv;        /* 3타째 치환 버튼 (물리 버튼 잡는 동안 유지) */
static uint16_t  rush_conv_src;    /* 치환 대상 물리 비트 */

/* 러시(자동콤보) 스위치 — 유저 지시로 **끈다**.
   켜져 있으면 약 기본기 연타를 사다리로 받아 3타째에 버튼을 P↔K 로 갈아치우고
   4타째에 필살기를 대신 쏜다. 손으로 치는 콤보를 엔진이 덮어써 버려서
   「내가 누른 대로 안 나간다」가 된다. 되살리려면 이 값을 1 로. */
static int       svc_rush_on = 0;

static const svc_move *svc_disp_mv;   /* 자막에 쓴 기술 — 강이 확정되면 이름을 갈아 끼운다 */
static char      svc_disp_tail[48];   /* 자막의 꼬리(화살표+버튼) — 이름만 바꿔 붙이려고 */
static void      svc_name_short(const char *src, char *dst, int cap);

char svcsp_last_disp[64];  /* "황물기 \xe2\x86\x93\xe2\x86\x98\xe2\x86\x92+P" — 토스트용 */
int  svcsp_disp_seq;       /* 새 발동마다 +1. 프론트가 엣지 검출 */
const char *svcsp_last_name = 0;
int         svcsp_last_ok   = -1;
int         svcsp_last_strong = 0; /* 마지막 발동이 강(홀드)이었는지 — 표시용 */

static int svc_active(void) { return q_left > 0 || q_i < q_n; }

/* ── 롬 판별 — NGP 헤더 0x24~0x2F 12바이트 ──────────────────────── */
/* ★ 롬 문턱을 **코어에서** 고친다 — 원본 파일은 그대로 두고 올라온 바이트만 바꾼다.
   0x000D4E 는 강 문턱을 비교하는 상수다(`3F 04` = CP 4). 값이 곧 버튼 길이 문턱:
     04(원본)=8프레임  03=6  02=4  01=2
   IPS 를 따로 굽지 않아도 되고 설정으로 켜고 끌 수 있다. 0 이면 안 건드린다.
   ※ 실기·다른 에뮬에서 쓰려면 여전히 IPS 가 필요하다(그건 롬 파일을 고치는 것). */
#define OFF_ROM_STRONG 0x000D4E
static int svc_rom_thr_read;   /* 정의는 즉발 블록 옆 — 기본 4는 아래 초기화가 보장 못 하므로 set_rom 폴백에 의존 */
static int svc_rom_thr(void)
{ static int v=-1; if(v<0){const char*e=getenv("SVCSP_ROMTHR"); v=e?atoi(e):0;} return v; }

void svcsp_set_rom(const void *rom, unsigned len)
{
   svc_is_rom = rom && len >= 0x30 &&
                !memcmp((const unsigned char *)rom + 0x24, "SNKvsCAPCOM1", 12);
   if (svc_is_rom && len > OFF_ROM_STRONG)
   {  /* 즉발 주입값을 롬 문턱에 맞추려고 읽어 둔다 (패치 롬이면 2 나 1) */
      unsigned char t = ((const unsigned char *)rom)[OFF_ROM_STRONG];
      if (t >= 1 && t <= 8) svc_rom_thr_read = t;
   }
   if (svc_is_rom && svc_rom_thr() && len > OFF_ROM_STRONG)
   {
      unsigned char *p = (unsigned char *)rom;   /* const 를 벗긴다 — 의도적이다 */
      if (p[OFF_ROM_STRONG] == 0x04)             /* 원본 값일 때만 */
      {
         p[OFF_ROM_STRONG] = (unsigned char)svc_rom_thr();
         if (svc_dbg()) fprintf(stderr, "[svcsp] rom 문턱 %06X: 04 -> %02X\n",
                                OFF_ROM_STRONG, svc_rom_thr());
      }
   }
}
int svcsp_rom_ok(void) { return svc_is_rom; }

/* ── 전투중 판별 ─────────────────────────────────────────────────
   SvC 엔 SS2 의 MODE(0x00A7)=241 같은 단일 바이트가 없다 (BGM 상태 바이트는
   GO 연출에서 깨져 탈락 — 실측). 대신 네 조건의 합성으로 거른다.
   선택화면(char=255·timer=0)은 첫 조건에서 바로 떨어진다.
   Y 는 넣지 않는다 — CPU 던지기에 뜬 순간(y=44 실측)도 전투 중이다. */
static int svc_in_battle(void)
{
   if (CPUExRAM[OFF_CHAR1] >= 18) return 0;
   if (CPUExRAM[OFF_CHAR2] >= 26) return 0;
   if (CPUExRAM[OFF_STYLE] >= 3)  return 0;
   { unsigned t = CPUExRAM[OFF_TIMER];              /* 255 = 스파링 타임 무한 (실전 스테이트 실측) */
     /* ⚠️ 상한이 60 이던 탓에 **대전 시간을 90초로 두면 전투로 인식을 못 했다**(제보).
        게임 설정이 30/60/90/99 를 주므로 99 까지 받는다. 무한은 255. */
     if (t != 255 && (t < 1 || t > 99)) return 0; }
   return 1;
}

/* 행동 가능 판정.
   애니 카운터(0x0C7E)는 「마지막 동작 시작 후 경과」를 2프레임에 1씩 세다
   127에서 포화하는 시계다 (실측 — 대기 127은 포화값이었다).
   평타 회복 실측: +6f(카운터 3) 주입은 엉뚱한 기술이 새고, +12f(카운터 6)부터
   정상 발동. 그래서 문턱은 6. 피격·필살기 중의 이른 주입은 게임이 그냥 먹는다. */
static int svc_actable(void)
{
   unsigned a;
   static int nogate = -1;
   if (nogate < 0) { const char *e = getenv("SVCSP_NOGATE"); nogate = (e && *e == '1'); }
   if (nogate) return 1;
   if (warm < 4) return 0;
   a = CPUExRAM[OFF_ANIM];
   { unsigned k = CPUExRAM[OFF_ANIM + 1];          /* 0x0C7F = K(킥) 동작 카운터 */
     if (!((k == 127) || (k >= 6 && k < 127))) return 0; }
   return (a == 127) || (a >= 6 && a < 127);
}

static int svc_airborne(void) { return CPUExRAM[OFF_Y1] != 128; }

static unsigned svc_x16(unsigned off)
{ return (unsigned)CPUExRAM[off] | ((unsigned)CPUExRAM[off + 1] << 8); }
static int svc_facing_left(void)   /* P1 이 오른쪽에 있으면 왼쪽을 본다 */
{ return (CPUExRAM[OFF_FACE] & 0x80u) ? 1 : 0; }

static uint8_t svc_mirror(uint8_t pad)
{
   uint8_t lr = pad & (PAD_LEFT | PAD_RIGHT);
   pad &= (uint8_t)~(PAD_LEFT | PAD_RIGHT);
   if (lr == PAD_LEFT)  pad |= PAD_RIGHT;
   if (lr == PAD_RIGHT) pad |= PAD_LEFT;
   return pad;
}

/* 모으기 성립 판정 — 「잡은 채」 **또는** 「갓 뗀 기억(유예 20f)」.
   잡은-채 만 인정하면 슬롯 방향과 교착한다: 뒤를 잡으면 B슬롯으로 바뀌고, 놓으면
   dir_held=0 으로 탈락 — 가일 소닉붐을 낼 방법이 아예 없었다(실측 전 조합 무발동).
   대각(↙=0x06)은 뒤(0x04)·아래(0x02) 비트를 품으므로 비트 포함으로 본다 — ↙ 한 번으로
   소닉붐·서머솔트 둘 다 모인다. 문턱은 게임 실측 SVC_CHARGE_MIN(6) — 예전 40 은
   근거 없던 과잉이라 모으기 판정을 34프레임 늦게 만들었다. */
static int svc_charge_ok(const svc_move *m)
{
   uint8_t cd;
   if (!(m->flags & 16) || m->len <= 0) return 1;
   cd = m->motion[0];
   if (svc_facing_left()) cd = svc_mirror(cd);
   if (dir_held && (dir_latch & cd) == cd && dir_hold_len >= (uint32_t)SVC_CHARGE_MIN)
      return 1;
   /* 유예는 게임 실측에 맞춘다: 손입력 뗀 후 g10 까지 붐, g12 부터 백너클(두 위상).
      엔진 마무리가 +2f 늦게 닿으므로 8 — 예전 20 은 게임이 거부한 구간까지 쏴서
      가짜 기술(37/38)이 나갔다. */
   if ((charge_dir & cd) == cd && charge_len >= (uint32_t)SVC_CHARGE_MIN &&
       frames - charge_end_at <= 8)
      return 1;
   if (svc_dbg())
      fprintf(stderr, "[svcsp] charge chk cd=%02x held=%d latch=%02x hlen=%u mem=%02x mlen=%u age=%u\n",
              cd, dir_held, dir_latch, (unsigned)dir_hold_len,
              charge_dir, (unsigned)charge_len, (unsigned)(frames - charge_end_at));
   return 0;
}

/* 커맨드 → 프레임 큐. SS2 와 달리 리셋 트릭이 없다 — 그냥 순서대로 넣는다.
   ★ 마지막 방향을 STEP 프레임 잡은 **다음**, 잡은 채로 버튼을 HOLD 프레임.
     (같은 프레임에 방향+버튼이면 실패한다 — 실측 §7) */
static int svc_compile_cancel;   /* 이번 컴파일이 노멀 캔슬 경로인가 — 표시용 */
/* 방향 입력 이력 링 — 여기를 지우면 앞서 넣은 커맨드가 다음 것과 안 합쳐진다.
   실측: 초필 게이지가 찬 상태에서 SP 를 26프레임 간격으로 연타하면 236 이 두 번 겹쳐
   **최종결전오의 무식**(act 211~218, 콤보 6 피해 20)이 나갔다 — 제보 그대로다. */
#define OFF_DIRHIST  0x0CB8
#define DIRHIST_LEN  24
static int svc_clr_hist(void)
{ static int v=-1; if(v<0){const char*e=getenv("SVCSP_CLRHIST"); v=e?atoi(e):1;} return v; }
static int svc_hist_len(void)
{ static int v=-1; if(v<0){const char*e=getenv("SVCSP_HISTLEN"); v=e?atoi(e):DIRHIST_LEN;} return v; }

/* ═══ 링 주입 ═══════════════════════════════════════════════════════
   방향 이력 링에 커맨드를 **직접 써 넣고** 마지막만 실제로 입력한다.
   프레임마다 방향키를 넣던 것을 안 해도 되므로 발동이 9 → 5프레임이 된다.

   실측(18캐릭터 127기술, 손 커맨드 대조):
     주입이 손보다 못한 경우 **0건**. 92개는 피해가 같고 24개는 오히려 더 나갔다
     (손 대조군이 커맨드 창을 넘겨 실패한 것들). 나머지 11개는 양쪽 다 무발동.
   같은 시행을 세 번 돌려 전부 재현됐고, 황물기는 화면 픽셀로도 확인했다
   (같은 기술 0.4% 차이 / 다른 기술 10% 차이).

   ★ 규칙이 마지막 방향에 따라 **갈린다**:
       → 또는 ←  : 앞 칸만 박고 **마지막 방향+버튼은 실제 입력**
       대각(↘·↙) : **전부 박고 버튼만** 입력
     반대로 하면 안 나간다 — 623 계열을 앞 칸만 박으면 엉뚱한 기술이 나왔다.
   ★ 링 배치: 최신이 0x0CC8, 그 앞이 0x0CC6, 0x0CC4 … 2씩 내려간다. */
#define RING_TOP 0x0CC8
#define IS_DIAG(d) ((d)==0x0A || (d)==0x06 || (d)==0x05 || (d)==0x09)
/* 기본 **켜짐**. 발동 9 → 3프레임(위상1 은 2), 회귀 18/23 유지.
   ★ 대가: 강 경계가 SP 12 → **6** 으로 당겨진다(1~4 약 · 5 갈림 · 6+ 강).
     버튼 스텝이 프레임 0 부터 돌아 SP 길이가 곧 버튼 길이가 되는데 게임 문턱은
     버튼 8프레임 고정이라 그렇다. RING_HOLD_BIAS 로 12 까지 밀어 봤더니
     위상1 에서 강이 아예 안 나왔다 — 발동이 위상마다 3/2 로 갈려 버튼 길이가
     1프레임 어긋나기 때문. 그래서 보정 없이 6 으로 둔다.
   끄려면 SVCSP_RING=0 (그러면 발동 9 · 경계 12 로 돌아간다). */
/* 강 기본기 즉발 — 버튼 12프레임 대신 2프레임 + 홀드 카운터 주입.
   게임은 버튼을 누르는 동안 **2프레임에 1씩** 카운터를 올리고 놓으면 0 으로 되돌린다.
   값 4 = 버튼 8프레임 = 강 문턱. 그래서 3 을 박으면 다음 프레임에 4 가 되어 넘는다.
   4 이상을 박으면 게임이 되돌려 안 된다.
     펀치 0x0C76 · 킥 0x0C77  (나란히 있다. 대조군 없이 찾으면 시간 카운터에 묻힌다)

   실측 — 18캐릭터 × 강펀/강킥 × 두 위상 = **72/72 성공**. act·피해가 12프레임
   대조군과 완전히 같고(하오마루 강펀 16, 마이 강킥 4 까지), **발동이 6프레임 빠르다**
   (발동 8~9 → 2~3, 히트 16~25 → 10~19). 전 캐릭터·전 버튼·두 위상 모두 정확히 6.
   끄려면 SVCSP_FASTSTRONG=0. */
#define OFF_HOLDCNT_P 0x0C76
#define OFF_HOLDCNT_K 0x0C77
static int svc_fast_strong(void)
{ static int v=-1; if(v<0){const char*e=getenv("SVCSP_FASTSTRONG"); v=e?atoi(e):1;} return v; }
/* ★ 주입값은 **롬의 문턱 − 1** 이어야 한다. 게임은 「++ 한 결과가 문턱과 같아지는
   순간」 강을 래치한다 — 문턱을 건너뛴 값을 박으면 래치가 안 걸려 **약**이 나간다.
   실측: 문턱 2 로 패치한 롬에서 주입 3 → 강버튼이 약(81/90)으로 나갔다.
   문턱은 롬 0x000D4E 에서 읽는다(svcsp_set_rom 이 채움). 기본 4(원본). */
static int svc_inject_val(void)
{ int t = svc_rom_thr_read ? svc_rom_thr_read : 4;   /* 롬을 아직 안 읽었으면 원본값 4 */
  int v = t - 1; return v < 1 ? 1 : v; }
static int svc_ring_on(void)
{ static int v=-1; if(v<0){const char*e=getenv("SVCSP_RING"); v=e?atoi(e):1;} return v; }
static int  compiled_ring;   /* 이번 컴파일이 링 주입 경로였나 — 강약 보정에 쓴다 */
static uint8_t ring_pend[8]; /* 버튼 스텝 진입 프레임에 박을 링 내용 (컴파일 때 채움) */
static int     ring_pend_n;
/* 링 주입이면 방향 큐가 없어 버튼 스텝이 프레임 0 부터 돈다. 그래서 사용자가 SP 를
   쥔 시간이 그대로 버튼 길이가 되어 강 경계가 12 → 5~6 으로 당겨졌다.
   게임의 버튼 문턱은 링 주입과 무관하게 **8** 이다(실측: 버튼 2~6 약 / 8+ 강).
   사용자 규칙 「SP 12 부터 강」을 지키려면 SP 길이에서 이만큼 뺀다: 12-8 = 4. */
/* ⚠️ **0 이어야 한다.** 4 로 두면 위상0 은 경계가 7 로 밀리는데 위상1 은 독이 아예
   안 나온다 — 링 주입이면 발동이 3(위상0)/2(위상1)라 버튼 길이가 위상마다 1프레임
   어긋나는데 게임 문턱은 8 로 고정이라, 보정을 얹으면 한쪽이 문턱을 못 넘는다.
   그래서 링 주입에서는 **경계가 8** 이다(SP 8부터 강). 12 는 못 지킨다. */
#define RING_HOLD_BIAS 0
static void svc_ring_write(const uint8_t *dirs, int n)
{
   int i;
   for (i = 0; i < n; i++)
   {
      int a = RING_TOP - 2 * (n - 1 - i);
      if (a >= OFF_DIRHIST) CPUExRAM[a] = dirs[i];
   }
}

static void svc_compile(const svc_move *m)
{
   int i, mirror = svc_facing_left();
   int skip_charge = 0;
   uint8_t last = 0;
   q_n = q_i = 0;
   /* ★ **파생을 포함해 매번** 지운다. 「새 기술만 지운다」로 좁혀 봤더니 무식이 그대로
        나왔다 — 겹치는 커맨드가 파생 경로에서 나오기 때문이다.
        대가: 아주 빠른 연타(간격 4·8) 에서 위상에 따라 한 타를 잃는다. 황·독 단독과
        3연 파생은 두 위상 모두 그대로다(콤보 3 · 피해 9/11 실측). 그 값이면 남는 장사다. */
   if (svc_clr_hist() && !(m->flags & 16))
      /* 모으기 기술은 이력을 **지우지 않는다** — 사용자가 실제로 잡았던 방향 흔적이
         게임 차지 판정의 재료일 수 있다(손입력은 흔적 보존 상태에서 두 위상 모두 붐).
         모으기는 링 주입도 안 하므로 지울 이유가 없다. */
      memset(&CPUExRAM[OFF_DIRHIST], 0, (size_t)svc_hist_len());
   {  /* ★ 모으기 기술 — **사용자가 이미 모았으면 엔진이 또 모으지 않는다.**
        지금까지는 엔진이 첫 방향을 CHARGE_FRAMES(40) 동안 잡아 줬다. 나가긴 하지만
        그만큼 늦다(가일 소닉붐 실측: 손 커맨드 피해 7 로 정상 발동).
        사용자가 뒤를 잡은 채 SP 를 누르는 게 원래 이 기술의 치는 법이므로,
        잡고 있던 시간이 충분하면 첫 칸을 건너뛰고 나머지만 넣는다. */
      if ((m->flags & 16) && m->len > 0)
      {
         /* ★ 컴파일 시점 차지 **재검** — 보류(pending)를 거쳐 늦게 오면 발사 시점에
            차지가 소멸해 있을 수 있다. 옛날엔 이때 40f 자가충전 큐로 흘렀는데, 그러면
            한참 뒤 유령 발동(차지 무효면 마무리 8+K 가 점프킥으로 샘 — 연타 누수 제보의
            경로)이 된다. 불성립이면 **조용히 취소**한다. */
         if (!svc_charge_ok(m))
         {
            if (svc_dbg()) fprintf(stderr, "[svcsp] charge-expired at compile -> 취소\n");
            q_n = q_i = q_left = 0;
            return;
         }
         skip_charge = 1;
         /* ⚠️ x사본(0x0934) −10 「즉발」은 **가짜였다** — 나가던 것은 붐이 아니라
            앞+강 계열(act 37/38, 유저 포즈 판정 + 원거리 무히트 실측). 이분법의 성공
            판정 집합이 오염돼 있었다. 즉발 경로 전면 철회. 진짜 붐 = act 2 + 원거리 히트. */
      }
   }

   {  /* ★ 링 주입 — 방향을 프레임마다 넣는 대신 이력에 **직접 쓴다.**
        모으기 기술은 제외한다: 모으기는 「오래 잡고 있었다」는 사실이 필요한데
        이력에 값만 있다고 그게 성립하지는 않는다(미검증이라 손대지 않는다). */
      int nring = 0;
      compiled_ring = 0;
      if (svc_ring_on() && !(m->flags & 16) && m->len >= 2 && m->len <= 8)
      {
         /* ★ 링에는 **미러하지 않은 원본 모션**을 박는다. 게임 링은 물리 방향이 아니라
            **캐릭터 기준(앞/뒤)** 으로 저장한다 — 반전에서 물리 ←(214)를 손으로 쳐서
            성공한 순간의 링이 정방향 236 을 쳤을 때와 **동일**했다(02·0A·02, 실측).
            정방향에선 물리 = 캐릭터 기준이라 여태 구분이 안 됐고, 미러한 물리값을
            박는 바람에 반전에서 커맨드가 뒤집혀 보여 약펀만 나갔다(v3.24~26 버그).
            **실입력으로 나가는 마지막 방향만** 미러한다 — 패드는 물리 세계다. */
         nring = IS_DIAG(m->motion[m->len - 1]) ? m->len      /* 대각 마무리 — 전부 박는다 */
                                                : m->len - 1; /* →·← 마무리 — 마지막은 실제로 */
         /* ★ 링은 여기(컴파일)서 박지 않는다 — **버튼 스텝에 들어가는 프레임에** 박는다.
            링 항목 수명이 ~16프레임이라, 파생처럼 앞 기술 경직이 끝나길 기다렸다
            발사되는 경우 컴파일 시점에 박은 링은 발사 때 이미 만료돼 있었다
            (실측: 꾹 파생 — 죄읊기가 컴파일되는데 게임엔 안 들어감). */
         memcpy(ring_pend, m->motion, (size_t)nring);
         ring_pend_n = nring;
         /* ★ 마지막 방향은 **버튼과 같은 프레임에** 나가야 한다. 방향을 먼저 2프레임
              내보내고 버튼을 뒤에 붙이면 그 사이에 링이 밀려 안 나간다. */
         last = (nring < m->len)
              ? (mirror ? svc_mirror(m->motion[m->len - 1]) : m->motion[m->len - 1])
              : 0;                   /* 대각 마무리는 버튼만 — 방향을 얹으면 안 나간다 */
         compiled_ring = 1;
      }
      if (nring) goto tail;                  /* 방향 큐잉을 건너뛴다 */
   }

   i = 0;
   if ((m->flags & 16) && skip_charge)
   {  /* ★ 모음 기술은 손과 똑같이 **마무리(마지막 방향+버튼 동시)만** 넣는다.
         예비 스텝(4·6)을 앞에 붙이면 위상 절반에서 엉뚱한 기술이 나갔다(실측:
         엔진 경로 위상21 = act 48~50 · 원거리 무히트 = 장풍 아님). 손입력 전수
         (중립 0~3f × 앞+P 2~4f × 두 위상 12조합)는 전부 진짜 붐(act 2 + 원거리
         히트)이었다 — 게임은 마무리만 관대하게 받는다. */
      i = m->len;
      last = m->motion[m->len - 1];
      if (mirror) last = svc_mirror(last);
   }
   for (; i < m->len && q_n < MAX_STEPS - 3; i++)
   {
      uint8_t d = m->motion[i];
      if (mirror) d = svc_mirror(d);
      q[q_n].pad = d;
      /* 모으기 기술은 첫 방향을 길게 잡는다 ([4]6 의 4 부분).
         그 밖의 방향은 **마지막 한 칸만** STEP 프레임, 앞은 EARLY 프레임으로 짧게 간다.
         ⚠️ **앞 칸을 줄이는 시도는 실패했다.** 기본값은 전부 STEP(2)로 되돌렸다.
           · 손 커맨드로는 (…,1,2,2) 가 8캐릭터 전부 같은 히트 시각을 냈고,
             엔진에서도 14명이 2프레임 빨라지고 캔슬 연결이 5/18→7/18 로 늘었다.
           · 그런데 **나오는 기술이 달랐다** — 쿄 SP 단독이 뱅크 22(황물기)가 아니라
             21(88식 계열)로 나갔다. 피해가 둘 다 4 라 콤보·피해만 보던 판정이 놓쳤다.
           · (2,1) 처럼 뒤를 줄이면 시간부터 터진다 — 켄 62f · 펠리시아 54f · 료 74f.
         줄여도 기술이 빨라지지는 않는다 — 주입이 기술 발생에 흡수된다(쿄 16f 고정).
         다시 시도하려면 SVCSP_EARLY 로 하되 **뱅크를 반드시 대조**할 것. */
      /* 모으기 첫 칸: 사용자가 이미 모았으면(skip_charge) **2프레임 재확인**만 넣는다 —
         통째로 빼면 대각(↙) 모음이 위상 절반에서 백너클로 샜다(실측: 게임이 차지→6 전환을
         못 읽음). 손 커맨드 모양(뒤 잡은 채 → 앞+P)을 그대로 재현하는 것. 안 모았으면
         여기 오지 않는다(게이트가 무발동 처리). */
      q[q_n].frames = (uint8_t)((i == 0 && (m->flags & 16))
                              ? (skip_charge ? STEP_FRAMES : CHARGE_FRAMES)
                              : (i >= m->len - 2) ? STEP_FRAMES : EARLY_FRAMES);
      q[q_n].sustain = 0; q_n++;
      last = d;
   }
tail:
   /* 버튼 스텝 — 트리거를 잡고 있으면 늘어난다 (탭=약 황물기 / 홀드=강 독물기).
      링 주입으로 대각 마무리를 전부 박은 경우에는 방향을 얹지 않는다 — 버튼만 눌러야
      나간다(실측). 그 밖에는 마지막 방향을 잡은 채 버튼을 누른다. */
   q[q_n].pad = (uint8_t)(last | m->btn); q[q_n].frames = HOLD_FRAMES; q[q_n].sustain = 1; q_n++;
   q[q_n].pad = 0; q[q_n].frames = TAIL_FRAMES; q[q_n].sustain = 0; q_n++;

   q_left = q[0].frames;
   hold_elapsed = 0;
   verify_left = 60;
   /* ★ 모으기 기술은 재시도 금지 — 재전송 시점엔 모으기 유예(게임 실측 ~10f)가 이미
      끝나 있어 진짜 기술이 나갈 수 없고, 마무리 커맨드만 다시 들어가 가짜(가일 37/38)가
      나간다(실측 s8: 붐 성공 후 retry 가 백너클을 얹음 — 붐 act 2 가 걷기 2 와 겹쳐
      성공 증거로도 못 잡는 케이스가 있다). */
   if (compile_no_retry || (m->flags & 16)) { retry_mv = 0; }
   else { retry_mv = m; if (m == &svc_basic) retry_mv = 0; }
   retry_cnt = 0; retry_at = 0;
   bank_at_compile = CPUExRAM[OFF_BANK];
   act_at_compile  = CPUExRAM[OFF_ACT];
   chain_saw_act   = 0;               /* 부모 발동 증거는 act 로 다시 모은다 */
   chain_mv = m; chain_left = 0;      /* 창은 매크로가 끝날 때 연다 (step_out) */
   move_started = frames;
   svcsp_last_name = m->name;
   svc_disp_mv = m;                   /* 강이 확정되면 이름을 갈아 끼우려고 들고 있는다 */
   /* 표시 문자열: 이름(괄호 별칭은 잘라냄) + 화살표(표기 기준, 미러 안 함) + 버튼 */
   if (m != &svc_basic)
   {
      static const char *AR[16] = {0};
      const char *arrows[16]; int na=0, wi=0, ci;
      char nb[32];
      (void)AR;
      for (ci = 0; ci < m->len && na < 12; ci++)
      {
         const char *a2 = 0;
         switch (m->motion[ci]) {
            case 0x01: a2="\xe2\x86\x91"; break;   /* ↑ */
            case 0x09: a2="\xe2\x86\x97"; break;   /* ↗ */
            case 0x08: a2="\xe2\x86\x92"; break;   /* → */
            case 0x0A: a2="\xe2\x86\x98"; break;   /* ↘ */
            case 0x02: a2="\xe2\x86\x93"; break;   /* ↓ */
            case 0x06: a2="\xe2\x86\x99"; break;   /* ↙ */
            case 0x04: a2="\xe2\x86\x90"; break;   /* ← */
            case 0x05: a2="\xe2\x86\x96"; break;   /* ↖ */
         }
         if (!a2) continue;
         arrows[na++] = a2;
         if (ci == 0 && (m->flags & 16)) arrows[na++] = "모아";   /* 모으기: 「←모아→+P」 */
      }
      svc_name_short(m->name, nb, sizeof nb);
      {
         int n2 = snprintf(svcsp_last_disp, sizeof svcsp_last_disp, "%s ", nb);
         for (ci = 0; ci < na && n2 < (int)sizeof svcsp_last_disp - 8; ci++)
            n2 += snprintf(svcsp_last_disp + n2, sizeof svcsp_last_disp - n2, "%s", arrows[ci]);
         snprintf(svcsp_last_disp + n2, sizeof svcsp_last_disp - n2, "+%s%s",
                  (m->btn & PAD_A) ? "P" : "K",
                  svc_compile_cancel ? " 캔슬" : "");
      }
      { /* 강이 확정되면 갈아 끼울 수 있게 꼬리(화살표+버튼)를 기억해 둔다.
           ⚠️ 「첫 공백 이후」로 자르면 안 된다 — 「소닉 붐」처럼 이름에 공백이 있으면
           꼬리에 " 붐 …"이 딸려 들어가 강판 표시가 「소닉 붐 붐 …」이 된다(제보).
           이름 길이만큼 건너뛴 위치가 꼬리의 시작이다. */
        size_t nl = strlen(nb);
        snprintf(svc_disp_tail, sizeof svc_disp_tail, "%s", svcsp_last_disp + nl);
      }
      svcsp_disp_seq++;
   }
   svcsp_last_ok = -1;
   svcsp_last_strong = 0;
   if (svc_dbg()) fprintf(stderr, "[svcsp] compile %s steps=%d mirror=%d\n", m->name, q_n, mirror);
}

/* SP + 방향 → 슬롯. 공중이면 공중 슬롯만, 지상이면 지상 슬롯만.
   그 자리가 비면 가까운 자리 순서로 흐른다 (ss2sp SLOT_ORDER 축소판). */
static const unsigned char ORDER[6][6] = {
  /* N  */ {SL_N,  SL_N,  SL_N,  SL_N,  SL_N,  SL_N },
  /* F  */ {SL_F,  SL_DF, SL_N,  SL_D,  SL_B,  SL_DB},
  /* B  */ {SL_B,  SL_DB, SL_N,  SL_D,  SL_F,  SL_DF},
  /* D  */ {SL_D,  SL_DF, SL_DB, SL_N,  SL_F,  SL_B },
  /* DF */ {SL_DF, SL_D,  SL_F,  SL_N,  SL_DB, SL_B },
  /* DB */ {SL_DB, SL_D,  SL_B,  SL_N,  SL_DF, SL_F },
};

static const svc_move *svc_resolve(uint8_t held)
{
   const signed char *map;
   const svc_move *tbl;
   int ntbl;
   int chr = CPUExRAM[OFF_CHAR1];

   if (chr < SVC_CHAR_COUNT && svc_chars[chr].mv && svc_chars[chr].n)
      { svc_slots_ensure();
        map = svc_slot_run[chr]; tbl = svc_chars[chr].mv; ntbl = svc_chars[chr].n;
        chain_tbl = tbl; }
   else return &svc_basic;               /* 기술표 없는 캐릭터 */

   if (svc_airborne())
   {
      int mi = map[SL_AIR];
      /* ★ 비움 = **무반응**. 예전엔 평타(svc_basic)를 냈는데, 가일처럼 슬롯을 비운
         배치에서 연타할 때 점프킥·약손이 새어 나갔다(제보). 한 버튼 한 기술. */
      return (mi >= 0 && mi < ntbl && (tbl[mi].flags & 4)) ? &tbl[mi] : 0;
   }
   {
      int facing_left = svc_facing_left();
      int d = !!(held & PAD_DOWN);
      int l = !!(held & PAD_LEFT), r = !!(held & PAD_RIGHT);
      int fwd = facing_left ? l : r, back = facing_left ? r : l;
      int want = SL_N, k;
      if      (d && fwd)  want = SL_DF;
      else if (d && back) want = SL_DB;
      else if (d)         want = SL_D;
      else if (fwd)       want = SL_F;
      else if (back)      want = SL_B;
      for (k = 0; k < 6; k++)
      {
         int mi = map[ORDER[want][k]];
         if (mi < 0 || mi >= ntbl) continue;
         if (tbl[mi].flags & 4) continue;      /* 지상에서 공중기 배제 */
         return &tbl[mi];
      }
   }
   return 0;   /* ★ 비움 = 무반응 (예전 평타 폴백은 연타 때 약손이 새는 원인이었다) */
}

/* held = 트리거(X/R)가 아직 눌려 있는가. 버튼 스텝에서 잡고 있으면
   프레임을 소비하지 않고 늘린다 → 게임이 홀드 = 강(독물기)으로 받는다. */
static int svc_chain_idx(void);
static int svc_chain_any(void);

/** 이름에서 괄호 별칭을 잘라낸다 — 「114식 황물기 (Aragami)」 → 「114식 황물기」. */
static void svc_name_short(const char *src, char *dst, int cap)
{
   int i = 0;
   for (; i < cap - 1 && src[i]; i++)
   {
      if (src[i] == '(' || (src[i] == ' ' && src[i+1] == '(')) break;
      dst[i] = src[i];
   }
   while (i > 0 && dst[i-1] == ' ') i--;
   dst[i] = 0;
}

/** 강판으로 굳었을 때 자막의 **이름만** 갈아 끼운다.
 *
 *  표가 병합돼 있어(같은 커맨드의 홀드 변형은 한 줄 — §26) 자막이 약·강을 구분 못 했다.
 *  「황물기」로 떠 놓고 실제로는 독물기가 나가던 것(유저 제보). 컴파일 시점에는 아직
 *  약인지 강인지 모르므로, 강이 확정되는 자리(hold_elapsed >= 9)에서 다시 쓴다. */
static void svc_disp_strong(void)
{
   char nb[32];
   if (!svc_disp_mv || !svc_disp_mv->name_hold) return;
   svc_name_short(svc_disp_mv->name_hold, nb, sizeof nb);
   snprintf(svcsp_last_disp, sizeof svcsp_last_disp, "%s%s", nb, svc_disp_tail);
   svcsp_last_name = svc_disp_mv->name_hold;
   svcsp_disp_seq++;
   svc_disp_mv = 0;                   /* 한 번만 */
}

static uint8_t svc_step_out(int held)
{
   uint8_t out = q[q_i].pad;
   if (q[q_i].sustain && hold_elapsed < MAX_HOLD)
   {  /* ★ 링은 버튼 스텝에 **들어오는 첫 프레임에** 박는다 — 컴파일 시점에 박으면
        파생처럼 발사가 늦는 경우 링 수명(~16f)이 다해 게임이 못 읽는다(실측). */
      if (compiled_ring && ring_pend_n && hold_elapsed == 0)
         { svc_ring_write(ring_pend, ring_pend_n); ring_pend_n = 0; }
      /* ★ 모으기 기술의 강판 — 강 래치 창이 기술 시작 전 ~7프레임뿐이라 자연 카운트
         (2f당 1)로는 문턱(4)에 못 닿는다(실측: 서머솔트, +6f에 카운터 3에서 리셋 → 약.
         물리 강킥이 강판이 되는 건 fast-strong 주입 덕: +2f에 4 도달). 같은 수법으로,
         강판이 표에 있고 기술키를 4프레임 이상 쥐고 있으면 카운터를 문턱−1로 박는다.
         탭(≤3f)은 약 그대로. */
      if (chain_mv && (chain_mv->flags & 16) && chain_mv->name_hold &&
          held && hold_elapsed >= 4 && svc_fast_strong())
      {
         int off = (out & 0x10) ? OFF_HOLDCNT_P : OFF_HOLDCNT_K;
         int iv  = svc_inject_val();
         if (CPUExRAM[off] < iv) CPUExRAM[off] = (uint8_t)iv;
      }
      /* 사용자가 누른 길이를 **게임 버튼 길이로 그대로 옮긴다.**

         순정 실측(SVCSP_OFF=1, 쿄 236+P). **쥔 길이**로 읽으면 위상을 안 탄다 —
         펀치가 커맨드 4·5·6·7 어디에 걸리든 아래가 그대로 성립한다:
             1f    → 위상에 따라 씹힌다. 못 쓴다
             2~6f  → 약  (유예 5프레임)
             7f    → 위상에 따라 약/강이 **갈린다. 회색지대다**
             8f 이상 → 강 (14 까지 확인)
         그러니 사용자 규칙이 하나로 선다 — **짧게 누르면 약, 8프레임 이상이면 강.**
         기술과 무관하다.

         ★ 7 은 **아예 내보내지 않는다.** 사람이 7프레임 쥐면 6 으로 내려 약으로 확정한다.
           이걸 안 하면 「같은 걸 눌렀는데 다른 게 나간다」가 그 한 칸에서 되살아난다.
           (예전에 `trig_len >= SVC_TAP_MAX(5)` 로 **추측**해 갈랐던 것이 같은 병이다.
            5 는 게임의 8 과 아무 상관 없는 값이라 사람의 탭 3~8f 가 약·강으로 갈렸다.)

         버튼 구간은 이미 HOLD_FRAMES 만큼 돌므로 그만큼 빼고 늘린다.

         ★ 강판이 **없는** 기술은 길게 잡으면 오히려 안 나간다 — 실측: 나코루루·단은
         8프레임 이상 잡으면 피해 7 → 0(불발). 그래서 강판이 있다고 표에 적힌 기술만
         문턱을 넘겨 잡고, 나머지는 약 상한(6f)에서 끊는다. */
      /* ★ 문턱을 두지 않는다. **SP 를 A 에 그대로 미러링**한다 —
           SP 를 누르는 순간 커맨드가 시작하고, A 는 잡은 채로 있다가
           **SP 를 떼면 A 도 같이 뗀다.** 약/강은 게임이 알아서 가른다.
         엔진이 몇 프레임에서 갈랐는지 사람이 알 필요가 없어진다.
         전에는 `trig_len` 을 버튼 길이로 **환산**해서 넣었는데, 그러면 엔진의
         환산값과 게임의 실제 경계가 어긋나 「같은 걸 눌렀는데 다른 게 나간다」가
         생긴다. 미러링은 그 어긋남이 원리적으로 없다. */
      /* 버튼은 커맨드 4프레임째에 들어간다. **뗄 프레임 = SP 길이**, 단 양끝을 자른다:
             SP 8 이하  → 8 에 뗀다 (커맨드가 8프레임이라 그 전엔 못 뗀다) = 약
             SP 9~11    → SP 를 뗄 때 같이 뗀다                            = 약
             SP 12 이상 → 12 에 뗀다. 더 쥐지 않는다                        = 강
         버튼 길이로 옮기면 **SP 길이 − 4, 하한 4 상한 8**.
         실측표로 검산 — 버튼을 4·5(위상) 에 눌렀을 때:
             4프레임 쥠 → 뗌 8·9  → 양쪽 위상 다 약
             8프레임 쥠 → 뗌 12·13 → 양쪽 위상 다 강
         양쪽 다 위상에 안 걸린다. 사람이 아는 규칙은 「12프레임부터 강」 하나다.
         ★ 강판이 **없는** 기술은 길게 잡으면 오히려 불발한다(나코루루·단: 피해 7→0).
           그런 기술은 상한도 4 로 묶어 항상 약으로 낸다. */
      int cap   = (svc_disp_mv && svc_disp_mv->name_hold) ? MAX_HOLD : SVC_WEAK_MAX;
      int least = SVC_HOLD_MIN - HOLD_FRAMES;      /* 1프레임은 위상에 따라 씹힌다 */
      cap -= HOLD_FRAMES;                          /* 버튼 스텝이 이미 도는 만큼 뺀다 */
      if (compiled_ring)
      {  /* 링 주입은 버튼 스텝이 프레임 0 부터 돌아 SP 길이가 곧 버튼 길이가 된다.
           그대로 두면 강 경계가 12 → 5~6 으로 당겨지므로, **앞 BIAS 프레임 동안은
           버튼을 늘리지 않는다.** 그만큼 경계가 뒤로 밀려 12 에 선다.
           환산식(want = trig_len - BIAS)으로 해 봤더니 초반에 want 가 0 이라
           버튼 스텝이 2프레임 만에 끝나 버렸다 — 미러링을 유지해야 한다.
           ★ cap 도 여기서 다시 잡는다: 위의 cap 은 강이 확정되는 순간
             svc_disp_strong() 이 svc_disp_mv 를 비워 SVC_WEAK_MAX 로 줄어든다. */
         cap = MAX_HOLD - HOLD_FRAMES;
         if ((int)trig_len <= RING_HOLD_BIAS) held = 0;
         if (hold_elapsed < cap && (held || hold_elapsed < least))
         {
            if (++hold_elapsed + HOLD_FRAMES >= SVC_STRONG_MIN)
               { svcsp_last_strong = 1; svc_disp_strong(); }
            return out;
         }
      }
      else if (hold_elapsed < cap && (held || hold_elapsed < least))
      {
         if (++hold_elapsed + HOLD_FRAMES >= SVC_STRONG_MIN)
            { svcsp_last_strong = 1; svc_disp_strong(); }
         return out;
      }
   }
   if (--q_left <= 0)
   {
      if (++q_i < q_n) q_left = q[q_i].frames;
      else
      {
         q_n = q_i = 0; q_left = 0;
         macro_end_at = frames;
         /* 파생이 있는 기술이면 재입력 창을 연다 (실측: 첫 기술 시작 +2~36f 수용) */
         if (svc_chain_any())
            chain_left = svc_chain_win();   /* 조건은 svc_chain_next 와 같은 규칙이어야 한다 —
                                  예전엔 여기만 next_hold 폴백을 빼먹어서, 홀드로 발동한
                                  next_hold=-1 기술(죄읊기·구상)은 창이 아예 안 열렸다 */
      }
   }
   return out;                    /* 매크로 중 사용자 입력은 무시 */
}

/* 파생 창에서 물리 킥이 잡혀 있는가 — 프레임마다 svcsp_frame 이 갱신한다.
   실측(§31 ③): 3타 자리에서 펀치면 외식 섬돌뚫기, 킥이면 125식 칠뢰다.
   뱅크는 둘 다 158 이라 뱅크로는 못 가른다 — P/K 카운터가 따로 리셋되는 것으로 갈렸다. */
static int chain_kick;

/* 지금 이어질 파생의 표 인덱스 (없으면 -1) — 창 개방과 실제 선택이 같은 답을 쓰게 */
static int svc_chain_idx(void)
{
   int idx, hold;
   if (!chain_mv || !chain_tbl) return -1;
   if (chain_kick && chain_mv->next_k >= 0) return chain_mv->next_k;
   /* ★ 갈래는 **지금 누르고 있는 길이**로 고른다. 예전엔 `svcsp_last_strong`(직전 기술이
      강이었나)만 봤는데, 그건 이미 지나간 정보라 죄읊기에서 홀드해도 탭 갈래(벌읊기)만
      나왔다. 지금 트리거가 강 문턱을 넘겼으면 홀드 갈래로 간다.
      직전이 강이었던 것도 폴백으로 남긴다 — 쥔 채 창이 열리는 경우가 있다. */
   hold = ((int)trig_len >= SVC_SP_STRONG) || svcsp_last_strong;
   idx = hold ? chain_mv->next_hold : chain_mv->next;
   if (idx < 0 && hold) idx = chain_mv->next;                /* 홀드 전용이 없으면 약 파생 */
   if (idx < 0) idx = chain_mv->next_k;                      /* P 갈래가 없으면 K 갈래라도 */
   return idx;
}

/* 갈래가 하나라도 있는가 — 창은 킥을 잡기 **전에** 열려야 하므로 따로 본다.
   svc_chain_idx 로 창을 열면, 손을 늦게 대는 킥 갈래가 영영 안 열린다. */
static int svc_chain_any(void)
{
   if (!chain_mv || !chain_tbl) return 0;
   return chain_mv->next >= 0 || chain_mv->next_hold >= 0 || chain_mv->next_k >= 0;
}

/* 지금 파생 창에서 X 를 누르면 나갈 기술 (없으면 0) */
static const svc_move *svc_chain_next(void)
{
   int idx = svc_chain_idx();
   if (idx < 0) return 0;
   return &chain_tbl[idx];
}

/* R(기술키) 한 번 = 기술 발동 하나. 상황별 경로(즉시/보류/캔슬)는 실측 규칙 그대로 */
static void svc_fire_sp(uint8_t held)
{
   const svc_move *m = svc_resolve(held);
   if (svc_dbg()) fprintf(stderr, "[svcsp] edge chr=%d air=%d anim=%d -> %s\n",
                          CPUExRAM[OFF_CHAR1], svc_airborne(), CPUExRAM[OFF_ANIM],
                          m ? m->name : "(null)");
   if (m && (m->flags & 16))
   {  /* ★ 모으기 갈래:
        · **뒤모음([4]…)은 즉발** — 게임의 뒤차지 판정은 시간이 아니라 「뒤로 물러난
          거리」다(실측: x사본 0x0934 를 −10 튕기면 모음 0 에서 진짜 붐, +10/−6 은 실패,
          문턱 7~10px). 매크로 동안 x사본을 뒤로 속여 모음 없이 낸다. 유저 선택 사양.
        · 아래모음([2]…)·대각([1]…)은 다른 상태를 세서 그 트릭이 안 통한다(실측) —
          기존 게이트(6f 모음 + 유예 20f) 유지. */
      if (!svc_charge_ok(m))
      {
         if (svc_dbg()) fprintf(stderr, "[svcsp] charge-not-ready -> 무발동\n");
         return;
      }
   }
   if (m)
   {
      int need_ground = !(m->flags & 4);
      /* 킬캔슬: 자기 평타 직후(24f)의 기술키는 회복을 기다리지 않는다 —
         캔슬창이 히트 후 0~8f 라서 기다리면 놓친다 (실측 §19).
         히트가 아니면 게임이 그냥 먹는다 (가드·헛침 캔슬 불가 실측). */
      /* 노멀 캔슬: 창 안의 모션+버튼은 게임이 캐릭터별 지정기로 낸다 (실측 §24·§25).
         류·켄 등은 지정기가 공격기라 손맛 그대로 콤보 — 캔슬을 살린다.
         쿄는 지정기가 누에잡기(비공격 카운터, dud) — 창을 회피해 깨끗하게 낸다. */
      int chr2 = CPUExRAM[OFF_CHAR1];
      int dud  = ((chr2 < SVC_CHAR_COUNT) ? svc_chars[chr2].cancel_dud : 0)
                 && attack_was_kick;   /* 쿄 실측: 킥 캔슬=누에(꽝), 펀치 캔슬=물기(콤보) */
      int own_atk = (frames - my_attack_at) <= (dud ? 40 : 24) && !svc_airborne();
      int kill_cancel = own_atk && !dud && (frames - hit_at) <= 14;
      if (need_ground && svc_airborne())
         { pending = m; pending_left = PENDING_FRAMES; pending_kind = 0; }
      else if (kill_cancel)
         { svc_compile_cancel = 1; svc_compile(m); svc_compile_cancel = 0; }
      else if (own_atk && dud)
         { pending = m; pending_left = PENDING_FRAMES; pending_kind = 4; }
      else if (own_atk)
         { pending = m; pending_left = 30; pending_kind = 1; }   /* 선입력 — 히트 순간 발사 */
      else if (!svc_actable())
         { pending = m; pending_left = PENDING_FRAMES; pending_kind = 0; }
      else
         svc_compile(m);
   }
}

/* ═══════════════════════════════════════════════════════════════════
   시험대 (SVC_TESTBED 빌드 전용) — 손으로 눌러 보라고 만든 것

   엔진을 통째로 비켜 가서 **실측한 입력을 프레임 단위로 그대로** 밀어 넣는다.
   그래서 「엔진이 어떻게 해석했나」가 아니라 「게임이 이 입력에 뭘 내놓나」를
   맨눈으로 본다. 셋 다 커맨드는 ↓↓↘↘→→→→ 로 같고, **약펀치를 언제 잡고
   몇 프레임 쥐고 있느냐만** 다르다:

     Y(강펀 자리) = 황물기   A@6 · 2프레임   → 뱅크 22 · act 157
     X(강킥 자리) = 독물기   A@6 · 8프레임   → 뱅크 22 · act 168
     L          = 뱅크21    A@5 · 3프레임   → 뱅크 21 · act 156
     R(기술키)   = 세이브 되돌리기 (접촉 자세로 복구)

   부팅하면 스파링 접촉 세이브(쿄 vs 류·정지 더미)가 저절로 올라온다.
   발동 20프레임 뒤에 뱅크·act·피해를 자막으로 뱉는다.
   ═══════════════════════════════════════════════════════════════════ */
#include <stddef.h>
#include <stdbool.h>
#include "svcsp_state.h"
/* ★ 시험대 스위치 — 배포판은 0, 테스트 APK 는 빌드 스크립트가 1 로 바꾼다.
   런타임 스위치인 이유: 빌드 플래그는 ndk-build·make·svcrun 세 경로에
   각각 꽂아야 해서 하나만 빠뜨려도 조용히 꺼진 채 나간다. */
static int svc_tb_on = 0;
static int svc_tb(void)
{
   static int v = -1;
   if (v < 0) { const char *e = getenv("SVCSP_TEST"); v = svc_tb_on || (e && *e == '1'); }
   return v;
}
extern bool retro_unserialize(const void *data, size_t size);
extern void ss2comm_toast(const char *text, int frames);
void        svcsp_reset(void);

/* 커맨드 8프레임 — 오른쪽 볼 때 기준. svc_mirror 가 좌우를 맞춘다 */
static const uint8_t TB_CMD[8] = {
   PAD_DOWN, PAD_DOWN, PAD_DOWN|PAD_RIGHT, PAD_DOWN|PAD_RIGHT,
   PAD_RIGHT, PAD_RIGHT, PAD_RIGHT, PAD_RIGHT
};
/* 셋 다 커맨드는 ↓↓↘↘→→→→ 로 같다. 약펀치를 **언제 떼느냐**만 다르다.
   실측(순정, 스파링 접촉): A 를 4~5프레임에 잡으면 발동 7f, 6~7프레임이면 9f.
   황/독은 홀드 길이가 아니라 **뗀 프레임**이 가른다 — 발동+5 이상 쥐면 독.
   A@5 는 발동 7 이므로 12프레임에 떼면 독, 11 이면 황. 즉 홀드 6 과 7 이 경계다. */
static const struct { const char *nm; int at, hold; } TB_JOB[3] = {
   /* ★ 게임은 2프레임마다 입력을 본다. 누른 순간이 짝/홀 어느 위상에 걸리느냐로
      전체가 1프레임 밀린다 — 그래서 이론상 최소값(독 홀드 7)은 절반만 성공한다.
      양쪽 위상에서 다 되는 값으로 잡았다: 황 6 · 독 9. */
   { "황 최속 6f", 5, 6 },   /* 뗌 11~12 — 어느 위상이든 황 */
   { "독 최속 9f", 5, 9 },   /* 뗌 14~15 — 어느 위상이든 독 */
   { "커맨드밖",   8, 4 },   /* 8프레임 넘겨 누름 — 필살기가 안 나간다 */
};

static int      tb_boot;          /* 부팅 세이브를 올렸는가 */
static int      tb_job = -1;      /* 재생 중인 갈래 */
static int      tb_at;            /* 재생 프레임 */
static uint16_t tb_prev;          /* 버튼 엣지용 */
static int      tb_watch;         /* 결과를 읽기까지 남은 프레임 */
static int      tb_hp0;           /* 시작 체력 — 피해 계산 */
static uint8_t  tb_bank, tb_act;  /* 관측값 */

static void tb_load(void)
{
   retro_unserialize(svc_state_blob, (unsigned long)SVC_STATE_LEN);
   svcsp_reset();
   tb_job = -1; tb_watch = 0;
}

/* 1 = 이 프레임 패드를 시험대가 가져갔다 */
static int tb_step(uint8_t *pad, uint16_t ret)
{
   uint16_t now, edge;
   char msg[64];
   int i;

   if (!svc_tb()) return 0;
   if (!tb_boot) { tb_boot = 1; tb_load(); *pad = 0; return 1; }

   /* Y=0x0002 X=0x0200 L=0x0400 R=0x0800 */
   now  = (uint16_t)(ret & 0x0E02u);
   edge = (uint16_t)(now & ~tb_prev);
   tb_prev = now;

   if (edge & 0x0800u) { tb_load(); *pad = 0; return 1; }   /* R = 되돌리기 */

   if (tb_job < 0)
   {
      int pick = (edge & 0x0002u) ? 0 : (edge & 0x0200u) ? 1
               : (edge & 0x0400u) ? 2 : -1;
      if (pick < 0)
      {
         if (tb_watch && --tb_watch == 0)
         {  /* 결과 보고 — act 는 발동 직후가 아니라 몇 프레임 뒤에 앉는다 */
            int dmg = tb_hp0 - (int)CPUExRAM[OFF_HP2];
            snprintf(msg, sizeof msg, "뱅크 %u  act %u  피해 %d",
                     (unsigned)tb_bank, (unsigned)tb_act, dmg);
            ss2comm_toast(msg, 150);
         }
         return 0;                                    /* 평소엔 손 안 댄다 */
      }
      tb_job = pick; tb_at = 0;
      tb_hp0 = (int)CPUExRAM[OFF_HP2];
      tb_bank = 255; tb_act = 0;
      snprintf(msg, sizeof msg, "%s  A@%d x%d",
               TB_JOB[pick].nm, TB_JOB[pick].at, TB_JOB[pick].hold);
      ss2comm_toast(msg, 90);
   }

   i = tb_at++;
   *pad = 0;
   if (i < 8) *pad |= TB_CMD[i];
   if (i >= TB_JOB[tb_job].at && i < TB_JOB[tb_job].at + TB_JOB[tb_job].hold)
      *pad |= PAD_A;                                  /* 약펀치 */
   /* svc_mirror 는 **무조건 뒤집는** 함수다 — 좌우 판단은 부르는 쪽 몫 */
   if (svc_facing_left()) *pad = svc_mirror(*pad);

   /* 재생이 끝나면 20프레임 지켜보고 뱅크·act 를 집는다 */
   if (i >= 20)
   {
      uint8_t b = CPUExRAM[OFF_BANK];
      if (b != 255 && tb_bank == 255) tb_bank = b;
      if (tb_bank != 255 && !tb_act)  tb_act  = CPUExRAM[OFF_ACT];
   }
   if (tb_at >= 40) { tb_job = -1; tb_watch = 30; }
   return 1;
}

uint8_t svcsp_frame(uint8_t pad, uint16_t ret)   /* ret = 레트로패드 원본 비트 */
{
   uint16_t trig, edge;
   {  /* 순정 측정용 스위치 — 실기 배포에는 영향 없음(환경변수) */
      static int off = -1;
      if (off < 0) { const char *e = getenv("SVCSP_OFF"); off = (e && *e == '1'); }
      if (off) { prev_trig = 0; return pad; }
   }
   {  uint8_t tp = pad;
      int took = tb_step(&tp, ret);
      if (svc_dbg() && (took || pad))
         fprintf(stderr, "[pad] %s ret=%04x -> %02x\n", took ? "TB" : "HW",
                 (unsigned)ret, (unsigned)(took ? tp : pad));
      if (took) { prev_trig = 0; return tp; }
   }
   if (!svc_native_basics && !svc_basics_split)
   {  /* ★ 강약 구분 끔(옵션) — A·B 는 **순정 그대로**(탭=약/꾹=강, 약컷 없음).
         다만 강버튼(Y/X)까지 순정 홀드로 내리면 강기본기가 8프레임짜리가 된다
         (제보: 「빠른 강기본기 되겠나」) — 그래서 Y/X 는 즉발 주입을 유지한다.
         = 순정 2버튼 감각 + 원할 때만 쓰는 빠른 강버튼. */
      if (ret & (1u << 1))
      {
         pad |= 0x10;
         if (svc_fast_strong() && !svc_airborne())
         { int iv = svc_inject_val();
           if (CPUExRAM[OFF_HOLDCNT_P] < iv) CPUExRAM[OFF_HOLDCNT_P] = (uint8_t)iv; }
      }
      if (ret & (1u << 9))
      {
         pad |= 0x20;
         if (svc_fast_strong() && !svc_airborne())
         { int iv = svc_inject_val();
           if (CPUExRAM[OFF_HOLDCNT_K] < iv) CPUExRAM[OFF_HOLDCNT_K] = (uint8_t)iv; }
      }
      if (ret & (1u << 10)) pad |= 0x30;                  /* L = A+B */
   }
   else if (!svc_native_basics)
   {  /* 기본 레이아웃(양 모드 공통): A·B=약 고정, Y=강펀치(C) X=강킥(D), L=A+B.
         약 고정 = 물리 버튼을 6f(실측 약 상한)에서 강제 해제 — 꾹 눌러도 강이 안 된다.
         메뉴에서는 그냥 짧은 A 누름과 같아 부작용 없음.
         모던(엔진 켬)에서도 기본기 여섯 자리는 그대로다 — 「기본기는 콤보,
         기술키는 SP」(제보). 기술키는 R 하나만 넘어간다. 끔이면 R 도 A+B. */
      static int hold_p, hold_k, wk_p, wk_k;
      wk_p = (ret & (1u << 0)) ? wk_p + 1 : 0;            /* 물리 B버튼 = NGP A */
      wk_k = (ret & (1u << 8)) ? wk_k + 1 : 0;            /* 물리 A버튼 = NGP B */
      pad &= (uint8_t)~0x30;
      if (wk_p >= 1 && wk_p <= svc_btn_weak()) pad |= 0x10;   /* 약펀치 — 약 상한까지만 */
      if (wk_k >= 1 && wk_k <= svc_btn_weak()) pad |= 0x20;   /* 약킥   — (문턱 파생) */
      /* ★ 강 = **즉발**. 게임은 버튼 홀드 길이를 `0x0C76` 에 센다(누르는 동안 2프레임에
           1씩, 놓으면 0). 값 4 = 버튼 8프레임 = 강 문턱. 그래서 버튼을 12프레임 쥐는
           대신 **2프레임만 넣고 누른 다음 프레임에 3 을 박으면** 강이 그대로 나온다.
           실측(쿄·류·테리·켄 × 두 위상): 전부 강펀치 act 99 · 피해가 12프레임 대조군과 동일.
           박는 시점은 **버튼 누른 다음 프레임**이어야 한다 — 그보다 뒤면 안 나간다.
           4 이상을 박으면 게임이 되돌려 안 된다. 3 이어야 다음 프레임에 4 가 되어 넘는다. */
      if (svc_fast_strong() && !svc_airborne())
      {  /* ★ 즉발은 **지상 전용** — 공중은 홀드 카운터 판정이 달라 3f+주입으로는
            강이 안 나간다(제보: 공중 강PK 무발동). 공중은 아래 홀드 경로가 받는다. */
         static int pv, kv;
         int pn = (ret & (1u << 1)) ? 1 : 0, kn = (ret & (1u << 9)) ? 1 : 0;
         if (pn && !pv) hold_p = 3;                       /* 엣지에서 3프레임만 */
         if (kn && !kv) hold_k = 3;
         pv = pn; kv = kn;
         /* ★ **매 프레임 검사해서 3 미만일 때만 박는다.** 특정 프레임에 한 번만 박으면
              게임이 2프레임에 1씩 올리는 주기의 어느 쪽에 걸리느냐로 갈려 수율이 50%
              (캐릭터마다 한쪽 위상만 성공)였다. 이렇게 두면 위상을 안 탄다.
              이미 게임이 4 로 올렸으면 덮지 않는다 — 덮으면 도로 문턱 아래로 내려간다. */
         if (hold_p)
         { int iv = svc_inject_val(); pad |= 0x10; hold_p--;
           if (CPUExRAM[OFF_HOLDCNT_P] < iv) CPUExRAM[OFF_HOLDCNT_P] = (uint8_t)iv; }
         if (hold_k)
         { int iv = svc_inject_val(); pad |= 0x20; hold_k--;
           if (CPUExRAM[OFF_HOLDCNT_K] < iv) CPUExRAM[OFF_HOLDCNT_K] = (uint8_t)iv; }
      }
      else
      {
         if (ret & (1u << 1)) hold_p = SVC_HOLD_STRONG; else if (hold_p) hold_p--;
         if (ret & (1u << 9)) hold_k = SVC_HOLD_STRONG; else if (hold_k) hold_k--;
         if (hold_p) pad |= 0x10;                         /* NGP A 지속 = 강펀치 */
         if (hold_k) pad |= 0x20;                         /* NGP B 지속 = 강킥 */
      }
      if (ret & (1u << 10)) pad |= 0x30;                  /* L = A+B(백플립) */
   }
   {  /* ★ 착지 선입력 — 공중에서 누른 기본기를 기억했다가 **착지하는 순간** 낸다.
        실측(순정, 제자리 점프, 착지 43프레임): 공중에서 누른 기본기는 공중 동작으로
        소비되고 지상으로 안 넘어온다. 착지 뒤에 누른 것만 나가고, 그것도 누른 시점
        +8~20 프레임이라 「점프 기본기 → 착지 → 기본기」를 손으로 잇기가 어렵다.
        그래서 착지 직전 창 안에 누른 것을 엔진이 대신 들고 있다가 낸다. */
      static uint16_t prev_ret;
      static int  land_wait, land_cyc, land_air, land_str;
      static unsigned char land_a0;
      static uint8_t land_btn;
      uint16_t bedge = (uint16_t)(ret & ~prev_ret);
      int air = svc_airborne();
      prev_ret = ret;
      if (air)
      {  /* 공중에서 새로 누른 기본기를 기억한다. 나중 누름이 앞 누름을 덮어쓴다 */
         static int air_hold;
         static unsigned char air_acte;
         if      (bedge & (1u << 1)) { land_btn = 0x10; land_str = 1; land_wait = SVC_LAND_WIN; air_hold = 0; air_acte = CPUExRAM[OFF_ACT]; }
         else if (bedge & (1u << 9)) { land_btn = 0x20; land_str = 1; land_wait = SVC_LAND_WIN; air_hold = 0; air_acte = CPUExRAM[OFF_ACT]; }
         else if (bedge & (1u << 0)) { land_btn = 0x10; land_str = 0; land_wait = SVC_LAND_WIN; air_hold = 0; air_acte = CPUExRAM[OFF_ACT]; }
         else if (bedge & (1u << 8)) { land_btn = 0x20; land_str = 0; land_wait = SVC_LAND_WIN; air_hold = 0; air_acte = CPUExRAM[OFF_ACT]; }
         else if (land_wait > 0)     land_wait--;
         /* ★ 버튼 하나 = 기술 하나. 이 누름이 **공중 기술로 이미 소비**됐으면 착지 무장을
            푼다 — 빈 점프에서 Y 한 번에 공중강+착지강 두 방 나가는 오발 방지.
            소비 판정은 act 로 — 누른 시점의 act 도, 점프 이동 동작(제자리 3~6 / 앞 7~9 /
            뒤 11~14, 카탈로그 실측)도 아닌 값으로 바뀌면 새 공중 기술이 나간 것.
            (동작 카운터 리셋으로 가르면 오탐 — P 카운터는 킥 동작 중에도 리셋된다, 실측 65f)
            점프킥 뒤의 두 번째 누름은 공중 기술을 못 만들므로(점프당 공중 공격 1회)
            act 가 킥 그대로라 무장이 산다. */
         if (land_btn)
         {
            unsigned a = CPUExRAM[OFF_ACT];
            int freepass = (a == air_acte || a == 0 ||
                            (a >= 3 && a <= 9) || (a >= 11 && a <= 14));
            if (!freepass)
            {
               /* 귀속: 내 공중기술은 엣지 +4~8f 에 act 가 뜬다(카탈로그 실측 82/100).
                  엣지 +2f 이내에 뜬 act 는 **먼저 누른 다른 버튼의 기술**이 이제 막
                  시작된 것 — 내 누름은 안 소비됐으니 무장 유지 (e60 오귀속 실측). */
               if (air_hold <= 2) air_acte = (unsigned char)a;
               else               { land_btn = 0; land_wait = 0; }
            }
         }
         /* ★ 무장된 누름은 첫 12프레임은 게임에 보낸다 — 공중 강(홀드 판정) 성립용
            (4프레임에서 끊었더니 공중 강이 약 82 로 떨어졌다, 실측). 그 뒤로는
            **삼킨다** — 게임 눈엔 떼진 상태가 되어 착지 엣지가 깨끗해진다. */
         if (land_btn && land_wait > 0 && ++air_hold > 12)
            pad &= (uint8_t)~land_btn;
      }
      else
      {
         /* ★ 지상 회복 중 누름도 같은 사이클로 받는다 — 착지(67f)가 회복 끝(83f)보다
            훨씬 이르므로, 사람 손은 킥 히트(71f) 무렵 = 이미 지상에서 누르게 된다.
            그 누름은 게임 자체 버퍼(행동가능 3~4f 전)보다 이르면 그냥 죽던 것(실측
            e66~78 전멸). 행동불가 + 비중립 act 일 때만 무장 — 중립·걷기에서는 게임이
            직접 받으므로 끼어들지 않는다(이중발사 방지). */
         if ((bedge & 0x0303u) && svc_dbg())
            fprintf(stderr, "[land] g-edge cyc=%d actable=%d act=%u\n",
                    land_cyc, svc_actable(), (unsigned)CPUExRAM[OFF_ACT]);
         if (!land_cyc && (bedge & 0x0303u) && !svc_actable())
         {
            unsigned a = CPUExRAM[OFF_ACT];
            if (!(a == 0 || a == 2 || a == 21 || a == 22 || a == 23 || a == 255))
            {
               if      (bedge & (1u << 1)) { land_btn = 0x10; land_str = 1; }
               else if (bedge & (1u << 9)) { land_btn = 0x20; land_str = 1; }
               else if (bedge & (1u << 0)) { land_btn = 0x10; land_str = 0; }
               else                        { land_btn = 0x20; land_str = 0; }
               land_cyc = 36; land_wait = 0;
               land_a0  = (unsigned char)a;
            }
         }
         if (land_air && land_wait > 0 && land_btn)       /* 방금 내려앉았다 */
            /* ★ 엣지 사이클 시동. 착지 순간 한 번만 누르는 옛 방식은 「킥이 늦게 나가
               착지 후에도 킥 동작이 이어지는」 깊은 히트에서 회복 프레임에 먹혀 죽었다
               (실측: 착지 67f, 회복 끝 83f, 첫 발사는 그 사이에 만료).
               게임은 행동가능 3~4프레임 전까지의 **엣지**를 버퍼로 받아 주므로(실측
               p80→83 발동, p79 죽음), 3f 누름+2f 뗌을 반복해 엣지를 계속 만들면
               그중 하나가 반드시 버퍼 창에 걸린다 — 손입력 재현으로 콤보 성립 확인. */
            { land_cyc = 36; land_wait = 0;
              land_a0 = CPUExRAM[OFF_ACT]; }
         if (land_cyc > 0)
         {
            /* 성공 감지는 **act 로만** — act 가 무장 시점 값도, 중립(0·2·21~23·255)도,
               점프 이동(3~9·11~14)도 아닌 값으로 바뀌면 기술이 나간 것 → 즉시 중단.
               (동작 카운터 리셋으로 가르면 오탐 — 카운터는 킥 진행 중 74f 에도
               발동 없이 리셋된다, CSV 실측. 그 오탐이 사이클을 4프레임 만에 죽였다) */
            unsigned a = CPUExRAM[OFF_ACT];
            int neutral = (a == 0 || a == 2 || a == 21 || a == 22 || a == 23 || a == 255 ||
                           (a >= 3 && a <= 9) || (a >= 11 && a <= 14));
            int ph = (36 - land_cyc) % 5;
            land_cyc--;
            if (!neutral && a != land_a0)
               { land_cyc = 0; land_btn = 0; }
            else
            {
               if (ph < 3)
               {  /* 누름 3프레임 — **강 버튼이었을 때만** 즉발 카운터를 박는다. 물리
                     엣지는 공중에서 이미 소진돼 즉발 경로가 착지에서는 안 걸리기
                     때문(실측). 약 버튼 선입력은 주입 없이 3프레임 = 약으로 나간다. */
                  pad |= land_btn;
                  if (land_str && svc_fast_strong())
                  {
                     int off = (land_btn & 0x10) ? OFF_HOLDCNT_P : OFF_HOLDCNT_K;
                     int iv  = svc_inject_val();
                     if (CPUExRAM[off] < iv) CPUExRAM[off] = (uint8_t)iv;
                  }
               }
               else pad &= (uint8_t)~0x30;   /* 뗌 2프레임 — 다음 엣지 준비 */
               if (land_cyc == 0) land_btn = 0;
            }
         }
         else land_btn = 0;
      }
      land_air = air;
   }
   {  /* ★ 동작 번호 상시 표시 (SVCSP_ACTSHOW=1) — 검증용. 화면 구석에 act(0x0968)를
         매 프레임 그려서, 영상·스샷만으로 「실제 무엇이 발동했나」를 게임이 직접 말한다.
         사람 눈으로 강약을 가르는 판독(임팩트 컷으로는 원리적으로 불가, 실측 3/15)을 없앤다.
         앱은 코어 로드 전에 환경변수로 켠다 — 게임 중 토글 불가, 재시작 필요. */
      static int actshow = -1;
      if (actshow < 0) { const char *e = getenv("SVCSP_ACTSHOW"); actshow = (e && *e == '1'); }
      if (actshow)
      {
         extern void ss2comm_actshow(const char *t);
         static char at[24];
         /* 내 동작 ID | 상대 반응(0x0AC4: 0=평시, 255=피격경직 — svcrun p2react 와 동일) */
         snprintf(at, sizeof at, "%u|%u",
                  (unsigned)CPUExRAM[OFF_ACT], (unsigned)CPUExRAM[0x0AC4]);
         ss2comm_actshow(at);
      }
   }
   if (!svc_engine_now())
   {
      if (!svc_native_basics && (ret & (1u << 11))) pad |= 0x30;   /* 엔진 끔: R 도 A+B */
      prev_trig = 0;
      return pad;
   }
   trig = (ret & (1u << 11)) ? 1u : 0u;                   /* 엔진 켬: R = 기술키 */
   edge = (uint16_t)(trig & ~prev_trig);
   prev_trig = trig;
   frames++;
   chain_kick = (ret & ((1u << 8) | (1u << 9))) ? 1 : 0;   /* 물리 약킥·강킥 */
   if (chain_left > 0 && --chain_left == 0) chain_queue = 0;   /* 창이 닫히면 쌓인 것도 버린다 */
   if (pad & PAD_DIR_MASK)
   {
      uint8_t nd = (uint8_t)(pad & PAD_DIR_MASK);
      if (nd != dir_latch || !dir_held)
      {  /* 방향이 바뀐다 — 직전 방향이 모으기 성립 길이였다면 **기억**해 둔다.
            (모으기 게이트가 「잡은 채」만 인정하면 슬롯 방향과 교착: 뒤를 잡으면
            B슬롯이 되고 놓으면 게이트 탈락 — 소닉붐을 낼 방법이 없었다, 실측) */
         if (dir_held && dir_hold_len >= (uint32_t)SVC_CHARGE_MIN)
            { charge_dir = dir_latch; charge_len = dir_hold_len; charge_end_at = frames; }
         dir_hold_from = frames;  /* 잡기 시작 */
      }
      dir_latch = nd; dir_latch_at = frames; dir_held = 1;
      dir_hold_len = frames - dir_hold_from;
   }
   else
   {
      if (dir_held && dir_hold_len >= (uint32_t)SVC_CHARGE_MIN)
         { charge_dir = dir_latch; charge_len = dir_hold_len; charge_end_at = frames; }
      dir_held = 0;
   }
   if (edge & 1u) trig_len = 0;                       /* 새로 누를 때만 리셋 */
   if (trig & 1u) { if (trig_len < 60) trig_len++; }  /* 떼도 값은 남긴다 — sustain 은 나중에 온다 */
   /* ── 모던 자동 콤보: 「기본기는 콤보, 기술키는 SP」 ──────────────
      · 러시: 기본기 연타(24f 창). 3타째는 P/K 를 교대로 갈아 끼우고,
        히트가 확인된 4타째를 마무리(대표 초필)로 바꾼다. 초필이 게이지
        부족으로 불발이면 재시도 훅이 필살기로 갈아탄다.
      · 어시스트: R(기술키)을 잡은 채 기본기 연타 — 같은 사다리.
        R 엣지 쪽은 3f 판별 대기(sp_defer)로 N슬롯 오발을 막는다. */
   if (rush_conv && svc_rush_on)
   {  /* 3타째 교대 치환 — 물리 버튼을 잡고 있는 동안 유지 */
      if ((ret & rush_conv_src) && !svc_active())
         pad = (uint8_t)((pad & (uint8_t)~0x30) | rush_conv);
      else rush_conv = 0;
   }
   if (svc_rush_on && svc_in_battle() && !svc_active())
   {
      uint16_t bnew  = (uint16_t)(ret & (uint16_t)~prev_ret);
      /* 약 기본기만 센다 — 강 연타는 ABLE 수동 콤보의 몫이라 건드리면 안 된다
         (제보: 「ABLE 강펀치 독물기 콤보가 안 나간다」 — 3타째 치환이 범인이었다) */
      uint16_t bmask = (uint16_t)((1u << 0) | (1u << 8));
      if (bnew & bmask)
      {
         int in_window = (frames - bas_last_at) <= 24;
         if (in_window || (trig & 1u)) rush_n++; else rush_n = 1;
         bas_last_at = frames;
         if (rush_n == 1) rush_hit0 = hit_at;
         if (rush_n >= 4 && hit_at > rush_hit0 && (frames - hit_at) <= 40)
         {  /* 마무리 = 필살기 (제보: 「약펀치 콤보는 필살기로」 — 초필은 게이지·연출이
               무거워 연타 마무리로는 안 어울린다. 초필은 방향+기술키로 직접). */
            const svc_move *fin = svc_pick_fallback(CPUExRAM[OFF_CHAR1]);
            if (fin)
            {
               pending = 0; pending_kind = 0;
               svc_compile(fin);
            }
            rush_n = 0; rush_conv = 0;
         }
         else if (rush_n == 3)
         {
            uint8_t want = (rush_prev_btn == 0x10) ? 0x20 : 0x10;
            rush_conv = want; rush_conv_src = (uint16_t)(bnew & bmask);
            pad = (uint8_t)((pad & (uint8_t)~0x30) | want);
            rush_prev_btn = want;
         }
         else
            rush_prev_btn = (uint8_t)((pad & 0x10) ? 0x10 : ((pad & 0x20) ? 0x20 : rush_prev_btn));
      }
      else if (rush_n && (frames - bas_last_at) > 40) { rush_n = 0; rush_conv = 0; }
   }
   prev_ret = ret;
   /* 유저가 직접 누른 평타 감지 — 매크로 중이 아닐 때의 A/B 새 눌림 */
   if (!svc_active())
   {
      uint8_t btn = (uint8_t)(pad & (PAD_A | PAD_B));
      uint8_t nw  = (uint8_t)(btn & (uint8_t)~prev_pad_btn);
      if (nw) { my_attack_at = frames; attack_was_kick = !!(nw & PAD_B); }
      prev_pad_btn = btn;
   }
   /* 히트 감지 — P2 체력 감소 순간 (킬캔슬은 히트에서만 연다. 가드·헛침 캔슬 불가 실측) */
   {
      uint8_t h2 = CPUExRAM[OFF_HP2];
      if (h2 < prev_hp2v) hit_at = frames;
      prev_hp2v = h2;
   }

   if (svc_in_battle()) { if (warm < 1000) warm++; }
   else warm = 0;

   /* 발동 결과 판정 — 애니 카운터(경과시계)가 **줄어들면** 새 동작이 시작된 것.
      뱅크로는 안 된다: 황물기 꼬리가 뱅크 0 이라 팔청(0)과 구분이 안 된다 (실측). */
   {
      static uint8_t va_prev = 255, vk_prev = 255;
      uint8_t va = CPUExRAM[OFF_ANIM], vk = CPUExRAM[OFF_ANIM + 1];
      /* 부모 발동 증거 — 컴파일 후 act 가 「컴파일 시점 값도 중립·이동 집합도 아닌 값」으로
         바뀌었으면 부모가 실제로 나간 것. 파생은 이 증거가 있어야만 쏜다 (제보: 부모가
         씹혔는데 후속타만 나감 — 창이 시간만 보고 열려 있었다). 뱅크로는 못 가른다 —
         21↔22 위상 뒤집힘이 발동으로 보이고, 필살기 뱅크도 22 라 겹친다(실측). */
      if (!chain_saw_act)
      {
         unsigned a2 = CPUExRAM[OFF_ACT];
         /* 증거 집합은 **좁게** — 걷기·점프 번호(1~14)를 중립 취급했더니 가일 소닉붐
            (act 2!)이 증거로 안 잡혀 retry 가 붐 뒤에 또 쐈다(실측 s8: 2→37/38).
            act 는 캐릭터별이라 낮은 번호도 기술일 수 있다. 오탐(가짜 증거)은 재시도만
            멈추니 안전하고, 미탐(증거 놓침)은 이중발사를 만든다 — 좁게 가는 게 맞다. */
         if (a2 != act_at_compile &&
             !(a2 == 0 || a2 == 21 || a2 == 22 || a2 == 23 || a2 == 255))
            chain_saw_act = 1;
      }
      if (verify_left > 0)
      {
         if ((va < va_prev && va_prev != 255) || (vk < vk_prev && vk_prev != 255))
            { svcsp_last_ok = 1; verify_left = 0; }   /* retry 는 뱅크로만 접는다 — 평타 leak 도 카운터는 리셋한다 */
         else if (--verify_left == 0) svcsp_last_ok = 0;
      }
      va_prev = va; vk_prev = vk;
      /* 불발 자동 재시도 — 진짜 발동은 뱅크가 바뀐다 (평타 leak 은 카운터만 리셋).
         매크로 후 12f 안에 뱅크 변화가 없으면 14f 간격으로 다시 넣는다 (자기 후딜은
         무브마다 달라 문턱으로 못 덮는다 — 강킥은 +36f 에야 커맨드가 먹힌다, 실측). */
      {
         uint8_t bnow = CPUExRAM[OFF_BANK];
         /* ★ 성공 판정에 act 증거를 추가 — 뱅크만 보면 「컴파일 시점 뱅크=22, 필살기
            뱅크=22」인 위상 절반에서 정상 발동을 불발로 오판해 커맨드를 재전송하고,
            그걸 **게임 자체 렛카**가 파생으로 받아 유령 독물기가 나갔다(실측: N3,
            156 발동 뒤 retry → 158). */
         if (retry_mv && (chain_saw_act || (bnow != bank_at_compile && bnow != 255)))
            { retry_mv = 0; rush_fb = 0; }     /* 뱅크 변화 또는 act 증거 = 진짜 발동 */
         if (retry_mv && !svc_active() && !pending && frames - macro_end_at >= 12)
         {
            if (rush_fb)
            {  /* 러시 마무리 초필 불발(뱅크 그대로) = 게이지 부족 — 필살기로 갈아탄다 */
               const svc_move *fb = rush_fb;
               rush_fb = 0;
               svc_compile(fb);
            }
            else if (retry_cnt >= 3) retry_mv = 0;
            else if (!retry_at) retry_at = frames + 2;
            else if (frames >= retry_at)
            {
               const svc_move *rm = retry_mv;
               int rc = retry_cnt + 1;
               if (svc_dbg()) fprintf(stderr, "[svcsp] retry %d %s\n", rc, rm->name);
               svc_compile(rm);                /* compile 이 retry 상태를 초기화한다 — 복원 */
               retry_mv = rm; retry_cnt = rc; retry_at = frames + 14;
            }
         }
      }
   }

   /* 보류 관리 — 전투가 끝났거나 새 입력이 오면 버린다 (ss2sp v0.5.4 규칙) */
   if (pending)
   {
      if (!svc_in_battle() || CPUExRAM[OFF_HP1] == 0 || CPUExRAM[OFF_HP2] == 0)
         { pending = 0; pending_left = 0; }
      else if (edge || ((pad & ~prev_pad_dir) & PAD_DIR_MASK))
         { pending = 0; pending_left = 0; }
   }
   prev_pad_dir = (uint16_t)(pad & PAD_DIR_MASK);

   if (pending && !svc_active())
   {
      int need_ground = !(pending->flags & 4);
      int fire = 0;
      if (pending_kind == 2)
         fire = 1;                       /* 파생 — 헛쳐도 창 안이면 나가는 것이 원작 동작 (유저 확인) */
      else if (pending_kind == 1 && (frames - hit_at) <= 2)
         fire = 2;                       /* 캔슬 선입력 — 히트가 뜬 그 순간 발사 */
      else if (pending_kind == 4)
      {  /* 평타 후 — 캔슬 창(공격 시작 +40f)이 지나고 행동 가능해지면 */
         if ((frames - my_attack_at) >= 40 && svc_actable()) fire = 1;
         else if (--pending_left <= 0) { pending = 0; pending_kind = 0; }
      }
      else if ((!need_ground || !svc_airborne()) && svc_actable())
         fire = 1;                       /* 일반 — 착지·회복 대기 */
      else if (pending_kind != 2 && --pending_left <= 0) { pending = 0; pending_kind = 0; }
      if (fire && pending)
      {
         compile_no_retry = (pending_kind == 2);   /* 파생은 재시도 금지 */
         svc_compile(pending); compile_no_retry = 0;
         pending = 0; pending_kind = 0;
      }
   }

   if (svc_active())
   {
      /* 렛카: 매크로 진행 중 재입력 → 파생을 보류에 걸어 둔다 (창 안에서 자동 발동) */
      if ((edge & 1u))
      {
         if (pending && pending_kind == 2)
         {  /* ★ 이미 파생이 예약돼 있으면 **덮어쓰지 말고 쌓는다.**
              실측: 12프레임 이하로 연타하면 2·3타 입력이 둘 다 1타 매크로 안에
              떨어지는데, 보류가 하나뿐이라 뒤 입력이 앞을 덮어써 3타가 통째로
              사라졌다 — 간격 0~12 는 콤보 2, 16~28 만 콤보 3 이었다. */
            if (chain_queue < 3) chain_queue++;
         }
         else
         {
            const svc_move *nx = svc_chain_next();
            if (nx) { pending = nx; pending_left = 60; pending_kind = 2; }
         }
      }
      return svc_step_out(trig & 1u);
   }

   if ((edge & 1u) && !svc_in_battle() && svc_dbg())
      fprintf(stderr, "[svcsp] gate-fail chr=%d chr2=%d style=%d timer=%d y=%d\n",
              CPUExRAM[OFF_CHAR1], CPUExRAM[OFF_CHAR2], CPUExRAM[OFF_STYLE],
              CPUExRAM[OFF_TIMER], CPUExRAM[OFF_Y1]);
   /* ⚠️ 「갈래를 정할 수 있을 때까지 기다린다」를 넣어 봤다가 **뺐다.**
        창이 열리는 프레임의 trig_len(=1)으로 늘 탭 갈래가 나가는 건 사실이지만,
        기다리게 하면 파생이 늦어져 한쪽 위상에서 콤보가 3 → 2 로 줄었다.
        그리고 원래 노렸던 「죄읊기 → 섬돌뚫기 홀드 파생」은 **게임에 없다** —
        섬돌뚫기와 벌읊기의 커맨드가 6+P 로 같아서 죄읊기 뒤엔 벌읊기만 나온다.
        얻는 것이 없고 잃는 것만 있어서 되돌렸다. */
   if (((trig & 1u) || chain_queue > 0) && svc_in_battle() && chain_left > 0
       && chain_saw_act               /* ★ 부모 발동 증거 없이는 파생 금지 */
       && !(CPUExRAM[OFF_ACT] == 0 || CPUExRAM[OFF_ACT] == 2 ||
            CPUExRAM[OFF_ACT] == 21 || CPUExRAM[OFF_ACT] == 22 ||
            CPUExRAM[OFF_ACT] == 23 || CPUExRAM[OFF_ACT] == 255 ||
            (CPUExRAM[OFF_ACT] >= 3 && CPUExRAM[OFF_ACT] <= 9) ||
            (CPUExRAM[OFF_ACT] >= 11 && CPUExRAM[OFF_ACT] <= 14))
                                       /* ★ 후속타 입력은 **기술이 돌고 있는 동안만** 유효
                                          (사양: 「독물기 중인지 아닌지를 따라야 한다」).
                                          중립·걷기·점프면 파생이 아니라 새 기술로 떨어진다 */
       && !pending)
   {  /* 엣지가 아니라 **유지**로도 파생을 낸다 — 잡고 있으면 창이 열리는 프레임에
         엔진이 대신 쏜다. 사람이 34프레임 창을 맞추려고 손가락을 떼었다 누르는
         왕복이 오히려 창을 넘기던 것(제보: 「안 이어진다」).
         엣지든 유지든 같은 자리에서 처리하므로 탭으로 치던 방식도 그대로 산다. */
      const svc_move *nx = svc_chain_next();
      if (nx && nx != chain_mv               /* 자기 자신으로 되돌지 않게 */
          && (int)(frames - move_started) >= svc_chain_gap())
      {
         if (chain_queue > 0) chain_queue--;   /* 쌓아 둔 연타를 한 장 쓴다 */
         if (svc_actable())
         {  /* 행동 가능 — 바로 쏜다 */
            compile_no_retry = 1; svc_compile(nx); compile_no_retry = 0;
            chain_left = 0;              /* 이 창은 소비했다 — 다음 창은 새 매크로가 연다 */
            if (svc_active()) return svc_step_out(trig & 1u);
         }
         else
         {  /* ★ 기술 애니가 아직 도는 중 — **즉시 쏘면 게임이 무시한다**(실측:
              꾹 파생이 컴파일은 되는데 act 가 안 바뀌고 콤보 2 에서 끊겼다).
              연타 2타가 이어지는 이유는 pending 으로 갔다가 행동 가능해질 때
              발사되기 때문 — 꾹도 같은 길로 보낸다. */
            pending = nx; pending_left = 60; pending_kind = 2;
            chain_left = 0;
         }
      }
   }
   if ((edge & 1u) && svc_in_battle())
   {  /* 터치는 방향과 기술키가 1~2프레임 어긋나기 쉽다 — 엣지에 방향이 비면
         직전 4프레임의 방향을 이어받는다 (「기술키 누르면 좌우를 못 알아듣는다」 제보).

         ★ 단 **짧게 스친 방향만** 이어받는다. 그냥 「최근 4프레임」으로 두면
         걷다가 기술키를 누를 때마다 걷던 방향의 슬롯이 나간다 — 실측:
           뒤로 걷다 떼고 0~2프레임에 SP → 75식 개(뒤 슬롯)
           앞으로 걷다 → 누에잡기(앞 슬롯) / 앉았다 → 귀신태우기(아래 슬롯)
           4프레임을 넘기면 전부 정상(황물기)
         유저 제보 「SP 누르기 전 입력이 섞여 다른 기술이 나간다」의 정체가 이것이다.
         터치의 어긋남은 방향이 **잠깐** 닿았다 떨어지는 것이고, 걷기는 **오래 잡는다**.
         그래서 잡고 있던 길이로 가른다. */
      uint8_t held = pad;
      if (!(held & PAD_DIR_MASK) && frames - dir_latch_at <= 4
          && dir_hold_len <= (uint32_t)DIR_GRAZE)
         held |= dir_latch;
      svc_fire_sp(held);  /* 즉시 발동 — 미루면 방향-모션 인접이 깨져 판정이 달라진다(실측) */
   }

   if (svc_active()) return svc_step_out(trig & 1u);   /* 트리거 프레임에도 첫 스텝이 나간다 */
   return pad;
}

/* ── 앱(NGP.emu) 어댑터 — 레트로패드가 없는 프론트용 ────────────────
   기본기 A/B 는 순정 그대로(탭=약/홀드=강 — 강 전용 버튼이 없는 프론트),
   기술키는 trig bit0 하나(앱의 SP 키). 러시/어시스트/초필 폴백은 그대로 돈다. */
uint8_t svcsp_frame_app(uint8_t pad, uint16_t trig)
{
   uint16_t ret = 0;
   uint8_t out;
   if (pad & 0x10) ret |= (1u << 0);    /* NGP A(펀치) → 약P 엣지 자리 */
   if (pad & 0x20) ret |= (1u << 8);    /* NGP B(킥)  → 약K 엣지 자리 */
   if (trig & 1u)  ret |= (1u << 11);   /* SP 키 → 기술키 */
   svc_native_basics = 1;
   out = svcsp_frame(pad, ret);
   svc_native_basics = 0;
   return out;
}

void svcsp_reset(void)
{
   q_n = q_i = q_left = 0;
   pending = 0; pending_left = 0; pending_kind = 0; move_started = 0;
   retry_mv = 0; retry_cnt = 0; retry_at = 0; macro_end_at = 0; compile_no_retry = 0;
   prev_trig = 0; prev_pad_dir = 0;
   prev_pad_btn = 0; my_attack_at = 0; hit_at = 0; prev_hp2v = 255; frames = 100;
   attack_was_kick = 0;
   warm = 0; verify_left = 0;
   hold_elapsed = 0; svcsp_last_strong = 0;
   chain_mv = 0; chain_tbl = 0; chain_left = 0; chain_queue = 0;
   svcsp_last_ok = -1; svcsp_last_name = 0;
   dir_latch = 0; dir_latch_at = 0; dir_hold_from = 0; dir_hold_len = 0; dir_held = 0;
   charge_dir = 0; charge_len = 0; charge_end_at = 0;
   trig_len = 0;
   prev_ret = 0; bas_last_at = 0; rush_n = 0; rush_prev_btn = 0; rush_hit0 = 0;
   rush_fb = 0; rush_conv = 0; rush_conv_src = 0;
}
