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

static void ram_rest(void) { CPUExRAM[T_OFF_ACT] = T_ACT_REST; }

static int fails;

static void ck(int cond, const char *what)
{
   if (!cond) { printf("  ★실패: %s\n", what); fails++; }
}

/* 기준 폴드 — **M2 계약**. M1 때는 엔진 상태와 무관하게 순정과 같았는데,
   M2 에서 R 이 SP 트리거가 되며 «엔진 켤 때만» 달라진다. 시험을 지우지 않고 고쳤다.

   ★ 엔진 끔 = **순정 롬 폴드와 글자 그대로 동일**(R 도 접는다).
     이건 kofsp 와 일부러 다르다(kofsp 는 R 을 무조건 뺀다). 이래야 「엔진 끔」이
     **진짜 대조군**이 된다 — 대조군이 조금이라도 다르면 그건 대조군이 아니다.
   ★ 엔진 켬 = R 은 트리거라 안 접힌다. L 은 그대로 A+B. */
static unsigned char ref_fold(unsigned char pad, unsigned ret, int engine)
{
   if (ret & (1u << RP_Y)) pad |= NGP_A;
   if (ret & (1u << RP_X)) pad |= NGP_B;
   if (ret & (1u << RP_L)) pad |= (unsigned char)(NGP_A | NGP_B);
   if (!engine && (ret & (1u << RP_R)))
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

   ram_rest();
   printf("lbsp 단위 시험 (M2 계약: 엔진 끔=순정 폴드 · 엔진 켬=R 은 트리거)\n");

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
      나가는지**를 본다 — 에뮬로는 못 가르는 것이다. */
   {
      static const unsigned char WANT[] = {
         0x02,0x02,0x02,0x02,              /* D    4프레임 */
         0x0A,0x0A,0x0A,0x0A,              /* D+R  4프레임 */
         0x08,0x08,0x08,0x08,              /* R    4프레임 */
         0x10,0x10,0x10,0x10,0x10,0x10     /* A    6프레임 */
      };
      int i, mis = 0;
      unsigned trigret = (unsigned)(1u << RP_R);
      lbsp_set_engine(1);
      lbsp_reset();
      ram_rest();
      for (i = 0; i < (int)(sizeof WANT); i++)
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
      ck(mis == 0, "매크로 18프레임 시간표 일치");
      /* 끝난 뒤에는 사람 입력이 그대로 통해야 한다 */
      ck(lbsp_frame(0x01, 0) == 0x01, "매크로가 끝나면 사람 입력이 그대로 통한다");
      /* ★ 누출 — 트리거를 «계속 쥐고» 있어도 되풀이 발동하면 안 된다 */
      lbsp_reset();
      ram_rest();
      for (i = 0; i < (int)(sizeof WANT); i++)
         lbsp_frame(0, (unsigned short)trigret);
      ck(lbsp_frame(0, (unsigned short)trigret) == 0,
         "트리거를 쥐고 있어도 두 번째가 저절로 안 나간다");
      /* ★ 쉬는 중이 아니면 안 걸린다 — 이 시험이 없으면 조건이 사는지 모른다.
         (기술이 나가는 중에 또 꽂으면 커맨드가 이어 붙어 딴 게 나간다.) */
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
