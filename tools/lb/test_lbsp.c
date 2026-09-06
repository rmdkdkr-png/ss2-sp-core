/* lbsp 단위 시험 — **에뮬레이터 없이** 돈다 (계획의 ①층).
 *
 * 여기서 잡는 것: 롬 판별 진리표, 그리고 M1 의 핵심 약속인
 * 「lbsp_frame 이 순정 폴드와 **글자 그대로** 같다」를 4,096조합 전수로 확인하는 것.
 * 에뮬을 띄우면 이 둘은 **출력이 같아서 구별이 안 된다** — 그래서 여기서 따로 잰다.
 *
 *   cc -O1 -I../../src -o test_lbsp test_lbsp.c ../../src/lbsp.c && ./test_lbsp
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lbsp.h"

#define RP_Y 1
#define RP_X 9
#define RP_L 10
#define RP_R 11
#define NGP_A (1 << 4)
#define NGP_B (1 << 5)

/* lbsp.c 가 extern 으로 쓰는 램. 시험에서는 우리가 준다.
   ⚠ 엔진은 act(0x0370)가 «쉼(4)»일 때만 매크로를 건다. 램을 0 으로 둔 채
     「매크로가 안 나간다」고 읽으면 **없는 병을 고치러 간다.** 시험대가 상태를 준다. */
unsigned char CPUExRAM[16384];
#define T_OFF_ACT   0x0370
#define T_ACT_REST  4
#define T_OFF_FACE  0x0386     /* 0 = 오른쪽 봄(앞=R) · 1 = 왼쪽 봄(앞=L) */
#define T_RING_HEAD 0x1312     /* «다음에 쓸» 칸의 색인 */
#define T_RING      0x1313     /* 128칸, 한 칸 2프레임 */

static void ram_rest(void) { CPUExRAM[T_OFF_ACT] = T_ACT_REST; }
static void ram_face(int left) { CPUExRAM[T_OFF_FACE] = (unsigned char)(left ? 1 : 0); }

static int fails;

static void ck(int cond, const char *what)
{
   if (!cond) { printf("  ★실패: %s\n", what); fails++; }
}

/* 기준 폴드 — 계약이 네 번 바뀌었다. 그때마다 시험을 «지우지 않고 고쳤다».
     M1: 엔진과 무관하게 순정 폴드와 동일.
     M2: 엔진을 켜면 R 이 트리거가 되어 안 접힘.
     M3: R 겸업을 뗐다(꺼도 A+B 로 안 접힘) — 「a+b는 a+b의 역할이고 SP는 SP다」를
         그렇게 읽었다.
     M4: **되돌렸다.** 유저 지시 「svc처럼 해라」. 그 말의 진짜 뜻은
         「버튼이 둘 다 화면에 있어야 한다」였고 그건 앱 쪽 일이었다.

   ★ 지금 계약: **Y=A · X=B · L=A+B 는 언제나.
     R 은 엔진 켤 때 SP, 끄면 A+B** (SvC `svcsp.c:1418` 과 같은 꼴).
   ★ A+B 를 쓰는 길은 «언제나» L 이다 — 아래 ①-2 가 그것을 이름 붙여 지킨다. */
static unsigned char ref_fold(unsigned char pad, unsigned ret, int engine)
{
   if (ret & (1u << RP_Y)) pad |= NGP_A;
   if (ret & (1u << RP_X)) pad |= NGP_B;
   if (ret & (1u << RP_L)) pad |= (unsigned char)(NGP_A | NGP_B);
   if (!engine && (ret & (1u << RP_R)))          /* 끔이면 R 도 A+B (SvC 와 같은 꼴) */
      pad |= (unsigned char)(NGP_A | NGP_B);
   return pad;
}

/* 롬 머리 흉내 — 0x24 에 표식을 박는다. */
static void mkrom(unsigned char *r, const char *tag)
{
   memset(r, 0, 0x40);
   memcpy(r + 0x24, tag, strlen(tag));
}

int main(void)
{
   unsigned char rom[0x40];
   int pad, y, x, l, r, n = 0, bad = 0;
   int engine;

   ram_rest(); ram_face(0);
   printf("lbsp 단위 시험 (계약: L=A+B 언제나 · R 은 켜면 SP, 끄면 A+B (SvC 꼴))\n");

   /* ── ① 롬 판별 진리표 ───────────────────────────────────── */
   mkrom(rom, "LASTBLADE124");
   lbsp_set_rom(rom, sizeof rom);
   ck(lbsp_rom_ok() == 1, "LASTBLADE124 를 월화로 본다");

   mkrom(rom, "SAMURAI2");
   lbsp_set_rom(rom, sizeof rom);
   ck(lbsp_rom_ok() == 0, "SAMURAI2 를 월화로 보지 않는다");

   mkrom(rom, "KOF R2");
   lbsp_set_rom(rom, sizeof rom);
   ck(lbsp_rom_ok() == 0, "KOF R2 를 월화로 보지 않는다");

   mkrom(rom, "SNKvsCAPCOM1");
   lbsp_set_rom(rom, sizeof rom);
   ck(lbsp_rom_ok() == 0, "SNKvsCAPCOM1 를 월화로 보지 않는다");

   /* J판은 표식이 다르다 — 지원 범위 밖인 것이 **의도**다. */
   mkrom(rom, "GEKKA");
   lbsp_set_rom(rom, sizeof rom);
   ck(lbsp_rom_ok() == 0, "GEKKA(J판) 는 범위 밖 — 안 문다");

   /* 짧은 롬에 손대지 않는지 */
   lbsp_set_rom(rom, 4);
   ck(lbsp_rom_ok() == 0, "0x30 보다 짧은 롬은 안 문다");
   lbsp_set_rom(NULL, 0);
   ck(lbsp_rom_ok() == 0, "NULL 롬은 안 문다");

   /* ── ①-2 A+B 는 «엔진을 켜도» L 에 그대로 남는다 ─────────────
      유저 요구: 「A+B 도 있고 SP 도 필요하다」. L 이 그 자리다.
      ★ 두 버튼을 «동시에 누른 척»하는 것이 아니라 **한 프레임에 한 바이트로**
        두 비트를 함께 세운다 — 그래서 폴링 사이로 새는 함정이 원리상 없다. */
   mkrom(rom, "LASTBLADE124");
   lbsp_set_rom(rom, sizeof rom);
   ram_rest(); ram_face(0);
   lbsp_set_engine(0); lbsp_reset();
   ck(lbsp_frame(0, (unsigned short)(1u << RP_L)) == (NGP_A | NGP_B),
      "엔진 끔: L 한 프레임에 A+B 두 비트가 함께 선다");
   lbsp_set_engine(1); lbsp_reset();
   ck(lbsp_frame(0, (unsigned short)(1u << RP_L)) == (NGP_A | NGP_B),
      "엔진 켬: L 한 프레임에 A+B 두 비트가 함께 선다");
   ck(lbsp_frame(0, (unsigned short)((1u << RP_L) | (1u << RP_Y))) == (NGP_A | NGP_B),
      "엔진 켬: L 과 Y 를 같이 눌러도 A+B 가 산다");
   /* ★ 엔진을 끄면 R 도 A+B 로 접힌다 — SvC 와 같은 꼴.
      (한때 이 시험이 「R 은 아무것도 안 한다」였다. 계약이 바뀌어 뒤집었다.) */
   lbsp_set_engine(0); lbsp_reset();
   ck(lbsp_frame(0, (unsigned short)(1u << RP_R)) == (NGP_A | NGP_B),
      "엔진 끔: R 은 A+B 로 접힌다");
   ck(lbsp_frame(0, (unsigned short)((1u << RP_R) | (1u << RP_L))) == (NGP_A | NGP_B),
      "엔진 끔: R+L 을 같이 눌러도 A+B 하나뿐");
   lbsp_reset();

   /* ── ② 폴드 4,096조합 전수 — 엔진 켬/끔 둘 다 ───────────── */
   mkrom(rom, "LASTBLADE124");
   lbsp_set_rom(rom, sizeof rom);
   for (engine = 0; engine <= 1; engine++)
   {
      lbsp_set_engine(engine);
      for (pad = 0; pad < 256; pad++)
         for (y = 0; y < 2; y++)
            for (x = 0; x < 2; x++)
               for (l = 0; l < 2; l++)
                  for (r = 0; r < 2; r++)
                  {
                     unsigned ret = (unsigned)((y << RP_Y) | (x << RP_X)
                                             | (l << RP_L) | (r << RP_R));
                     unsigned char got, want;
                     /* ★ 엔진 켬 + R 누름 = 트리거다. 매크로가 돌기 시작해
                        폴드와 다른 값이 나오는 것이 **정상**이라 여기서 빼고
                        아래 ③에서 따로 본다. 매번 리셋해 상태를 안 끌고 간다. */
                     if (engine && r) { lbsp_reset(); continue; }
                     got = lbsp_frame((unsigned char)pad, (unsigned short)ret);
                     want = ref_fold((unsigned char)pad, ret, engine);
                     n++;
                     if (got != want)
                     {
                        if (bad < 5)
                           printf("  ★어긋남: 엔진%d pad=%02X ret=%04X → %02X (기대 %02X)\n",
                                  engine, pad, ret, got, want);
                        bad++;
                     }
                  }
      lbsp_reset();
   }
   ck(bad == 0, "폴드 전수 일치");
   printf("  폴드 %d조합 검사 · 어긋남 %d\n", n, bad);

   /* ── ③ 매크로 시간표 — 트리거 한 번에 236+A 가 «그대로» 나오는가 ──
      에뮬은 「기술이 나갔나」만 본다. 여기서는 **어느 프레임에 어떤 비트가
      나가는지**, 그리고 **링에 무엇이 박혔는지**를 본다 — 둘 다 에뮬로는 못 본다.

      ★ 링이 켜져 있으면 대본이 다르다(D·DF 는 박고 F·버튼만 실제로 누른다).
        그래서 시험도 «모드에 따라» 기대표를 바꾼다. 두 모드를 다 돌리려면
        LBSP_RING=0 으로 한 번 더 실행해라. */
   {
      static const unsigned char WANT_LONG[] = {
         0x02,0x02,0x02,0x02,              /* D    4프레임 */
         0x0A,0x0A,0x0A,0x0A,              /* D+R  4프레임 */
         0x08,0x08,0x08,0x08,              /* R    4프레임 */
         0x10,0x10,0x10,0x10,0x10,0x10     /* A    6프레임 */
      };
      static const unsigned char WANT_RING[] = {
         0x08,0x08,                        /* R  2프레임 (LBSP_RING_F 기본) */
         0x10,0x10,0x10,0x10               /* A  4프레임 (LBSP_RING_BTN 기본)
                                              ★ 4 인 이유: 7 은 «위상에 따라 강약이 갈리는»
                                                금지구역이라 확실한 약 구역(≤6)에 둔다. */
      };
      const char *e = getenv("LBSP_RING");
      int ring = !(e && *e == '0');
      const unsigned char *WANT = ring ? WANT_RING : WANT_LONG;
      int WN = ring ? (int)(sizeof WANT_RING) : (int)(sizeof WANT_LONG);
      int i, mis = 0;
      unsigned trigret = (unsigned)(1u << RP_R);

      printf("  모드: 링 %s\n", ring ? "켬" : "끔");
      lbsp_set_engine(1);
      lbsp_reset();
      ram_rest(); ram_face(0);
      CPUExRAM[T_RING_HEAD] = 40;          /* 아무 자리나 — 상대 위치가 본질이다 */
      for (i = 0; i < WN; i++)
      {
         /* 첫 프레임만 트리거를 누르고 그 뒤는 뗀다 — 엣지 발동을 확인한다. */
         unsigned char got = lbsp_frame(0, (unsigned short)(i == 0 ? trigret : 0));
         if (got != WANT[i])
         {
            if (mis < 4)
               printf("  ★시간표 어긋남: f%d → %02X (기대 %02X)\n", i, got, WANT[i]);
            mis++;
         }
      }
      ck(mis == 0, "매크로 시간표 일치");

      if (ring)
      {
         /* ★ 머리 40 이면 최근 칸은 39(DF), 그 앞이 38(D). 시간 순서가 뒤집히면
            게임이 «F,D» 로 읽어 아무것도 안 나간다. */
         ck(CPUExRAM[T_RING + 39] == (0x02 | 0x08), "링 머리-1 칸에 DF 가 박힌다");
         ck(CPUExRAM[T_RING + 38] == 0x02,          "링 머리-2 칸에 D 가 박힌다");
         ck(CPUExRAM[T_RING + 40] == 0,             "머리 «자신»은 안 건드린다");
      }

      /* 끝난 뒤에는 사람 입력이 그대로 통해야 한다 */
      ck(lbsp_frame(0x01, 0) == 0x01, "매크로가 끝나면 사람 입력이 그대로 통한다");

      /* ★ 누출 — 트리거를 «계속 쥐고» 있어도 되풀이 발동하면 안 된다 */
      lbsp_reset(); ram_rest();
      for (i = 0; i < WN; i++)
         lbsp_frame(0, (unsigned short)trigret);
      ck(lbsp_frame(0, (unsigned short)trigret) == 0,
         "트리거를 쥐고 있어도 두 번째가 저절로 안 나간다");

      /* ★★ 반대편을 볼 때 — 앞이 R 이 아니라 **L** 이어야 한다. */
      {
         int j, m2 = 0;
         lbsp_reset(); ram_rest(); ram_face(1);
         CPUExRAM[T_RING_HEAD] = 40;
         for (j = 0; j < WN; j++)
         {
            unsigned char g2 = lbsp_frame(0, (unsigned short)(j == 0 ? trigret : 0));
            unsigned char w2 = WANT[j];
            /* 기대표의 R(0x08) 을 L(0x04) 로 뒤집어 본다 */
            if (w2 & 0x08) w2 = (unsigned char)((w2 & ~0x08) | 0x04);
            if (g2 != w2)
            {
               if (m2 < 4)
                  printf("  ★반대편 시간표 어긋남: f%d → %02X (기대 %02X)\n", j, g2, w2);
               m2++;
            }
         }
         ck(m2 == 0, "반대편(왼쪽 봄)에서 앞이 L 로 뒤집힌다");
         if (ring)
            ck(CPUExRAM[T_RING + 39] == (0x02 | 0x04), "반대편에선 링에 DB 가 박힌다");
      }

      /* ★ 매크로가 «도는 중»에 반전이 바뀌어도 시작할 때의 앞을 끝까지 쓴다. */
      lbsp_reset(); ram_rest(); ram_face(0);
      lbsp_frame(0, (unsigned short)trigret);
      ram_face(1);
      ck((lbsp_frame(0, 0) & (0x04 | 0x08)) != 0x04,
         "도는 중에 반전이 바뀌어도 시작할 때의 앞을 쓴다");
      ram_face(0);

      /* ★ 쉬는 중이 아니면 안 걸린다 */
      lbsp_reset();
      CPUExRAM[T_OFF_ACT] = 96;      /* 서서 베기 중 */
      ck(lbsp_frame(0, (unsigned short)trigret) == 0,
         "기술이 나가는 중에는 트리거가 안 먹는다");
      ram_rest();

      lbsp_reset();
      lbsp_set_engine(0);
   }

   /* ── ④ 안 잰 상수가 몇 개인지 스스로 말하는가 ────────────── */
   printf("  미측정 상수 %d개 (0 이 되는 날이 오프셋 사냥이 끝난 날이다)\n",
          lbsp_unmeasured_count());
   ck(lbsp_unmeasured_count() > 0, "M1 이니 미측정이 남아 있는 것이 정상");

   printf(fails ? "\n★FAIL (%d)\n" : "\nPASS\n", fails);
   return fails ? 1 : 0;
}
