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

/* lbsp.c 가 extern 으로 쓰는 램. 시험에서는 우리가 준다. */
unsigned char CPUExRAM[16384];

static int fails;

static void ck(int cond, const char *what)
{
   if (!cond) { printf("  ★실패: %s\n", what); fails++; }
}

/* 기준 폴드 — **M1 계약**: 순정 롬 폴드와 완전히 같다.
   ⚠ M2 에서 R 이 SP 트리거가 되면 이 계약이 «의도적으로» 깨진다.
     그때 이 시험을 지우지 말고 **새 계약으로 고쳐라** (kofsp 가 그렇게 했다). */
static unsigned char ref_fold(unsigned char pad, unsigned ret)
{
   if (ret & (1u << RP_Y)) pad |= NGP_A;
   if (ret & (1u << RP_X)) pad |= NGP_B;
   if ((ret & (1u << RP_L)) || (ret & (1u << RP_R)))
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

   printf("lbsp 단위 시험 (M1 계약: 순정 폴드와 동일)\n");

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
                     unsigned char got = lbsp_frame((unsigned char)pad, (unsigned short)ret);
                     unsigned char want = ref_fold((unsigned char)pad, ret);
                     n++;
                     if (got != want)
                     {
                        if (bad < 5)
                           printf("  ★어긋남: 엔진%d pad=%02X ret=%04X → %02X (기대 %02X)\n",
                                  engine, pad, ret, got, want);
                        bad++;
                     }
                  }
   }
   ck(bad == 0, "폴드 전수 일치");
   printf("  폴드 %d조합 검사 · 어긋남 %d\n", n, bad);

   /* ── ③ 안 잰 상수가 몇 개인지 스스로 말하는가 ────────────── */
   printf("  미측정 상수 %d개 (0 이 되는 날이 오프셋 사냥이 끝난 날이다)\n",
          lbsp_unmeasured_count());
   ck(lbsp_unmeasured_count() > 0, "M1 이니 미측정이 남아 있는 것이 정상");

   printf(fails ? "\n★FAIL (%d)\n" : "\nPASS\n", fails);
   return fails ? 1 : 0;
}
