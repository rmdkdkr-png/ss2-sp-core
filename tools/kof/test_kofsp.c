/* kofsp 단위 시험 — **에뮬레이터 없이** 돈다 (계획의 ①층).
 *
 * 여기서 잡는 것: 롬 판별 진리표, 그리고 M1 의 핵심 약속인
 * 「kofsp_frame 이 순정 폴드와 **완전히** 같다」를 전수로 확인하는 것.
 * 에뮬을 띄우면 이 둘은 **출력이 같아서 구별이 안 된다** — 그래서 여기서 따로 잰다.
 *
 *   cc -O1 -I../../src -o test_kofsp test_kofsp.c ../../src/kofsp.c && ./test_kofsp
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kofsp.h"

#define RP_Y 1
#define RP_X 9
#define RP_L 10
#define RP_R 11
#define NGP_A (1 << 4)
#define NGP_B (1 << 5)

static int fails;

static void ck(int cond, const char *what)
{
   if (!cond) { printf("  ★실패: %s\n", what); fails++; }
}

/* 기준 폴드 — **M2 에서 계약이 바뀌었다.**
   M1 때는 순정 폴드와 완전히 같아야 했다(L·R 둘 다 A+B).
   M2 에서 **레트로패드 R 을 SP 트리거로 돌렸으므로 R 은 더 이상 접히지 않는다.**
   그건 의도한 졸업이다 — 시험을 지우지 말고 **새 계약으로 고친다.**
   L 은 그대로 A+B 로 남는다(사람의 A+B 동시입력 수단을 뺏지 않는다). */
static unsigned char ref_fold(unsigned char pad, unsigned ret)
{
   if (ret & (1u << RP_Y)) pad |= NGP_A;
   if (ret & (1u << RP_X)) pad |= NGP_B;
   if (ret & (1u << RP_L)) pad |= (unsigned char)(NGP_A | NGP_B);
   return pad;   /* ⚠ R 은 일부러 뺐다 — 트리거다 */
}

static int rom_says(const char *path, int want)
{
   unsigned char *buf;
   long n;
   FILE *f = fopen(path, "rb");
   if (!f) { printf("  (건너뜀 — 롬 없음: %s)\n", path); return 1; }
   fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
   buf = (unsigned char *)malloc(n);
   if (fread(buf, 1, n, f) != (size_t)n) { }
   fclose(f);
   kofsp_set_rom(buf, (unsigned)n);
   {
      int got = kofsp_rom_ok();
      printf("  %-46s rom_ok=%d (기대 %d) %s\n", path, got, want,
             got == want ? "" : "★");
      if (got != want) fails++;
      free(buf);
      return got == want;
   }
}

int main(void)
{
   const char *home = getenv("HOME");
   char p[512];

   printf("1) 롬 판별 진리표 — 헤더 0x24 의 \"KOF R2\"\n");
   snprintf(p, sizeof p, "%s/ss2/rom/kofr2.ngc", home);        rom_says(p, 1);
   snprintf(p, sizeof p, "%s/ss2/rom/svc.ngc", home);          rom_says(p, 0);
   snprintf(p, sizeof p, "%s/ss2/rom/ss2.ngc", home);          rom_says(p, 0);
   snprintf(p, sizeof p, "%s/ss2/rom/lastblade.ngc", home);    rom_says(p, 0);
   rom_says("/mnt/c/Claude/KOF R2 한글/rom/_mslug1.ngc", 0);
   rom_says("/mnt/c/Claude/KOF R2 한글/rom/_mslug2.ngc", 0);
   rom_says("/mnt/c/Claude/KOF R2 한글/rom/_fatalfury.ngc", 0);
   /* 한글패치본도 헤더가 같으므로 1 이어야 한다 — 의도한 것이다 */
   rom_says("/mnt/c/Claude/KOF R2 한글/rom/v021_final.ngc", 1);

   printf("\n2) 짧은 입력·널 방어\n");
   kofsp_set_rom(NULL, 0);            ck(!kofsp_rom_ok(), "NULL 롬은 0");
   kofsp_set_rom("KOF R2", 6);        ck(!kofsp_rom_ok(), "0x30 미만 길이는 0");

   printf("\n3) kofsp_frame 이 순정 폴드와 같은가 — 전수\n");
   {
      unsigned pad, i;
      long n = 0, bad = 0;
      /* 관련 비트만 훑는다: pad 8비트 × (Y,X,L,R) 조합 16가지 */
      for (pad = 0; pad < 256; pad++)
         for (i = 0; i < 16; i++)
         {
            unsigned ret = ((i & 1) ? (1u << RP_Y) : 0)
                         | ((i & 2) ? (1u << RP_X) : 0)
                         | ((i & 4) ? (1u << RP_L) : 0)
                         | ((i & 8) ? (1u << RP_R) : 0);
            unsigned char a = kofsp_frame((unsigned char)pad, (unsigned short)ret);
            unsigned char b = ref_fold((unsigned char)pad, ret);
            n++;
            if (a != b) { if (bad < 4) printf("  ★ pad=%02X ret=%04X: %02X != %02X\n",
                                              pad, ret, a, b); bad++; }
         }
      printf("  %ld 조합 중 어긋남 %ld\n", n, bad);
      if (bad) fails++;
   }

   printf("\n3b) 트리거(R)는 접히지 않는가 — M2 의 계약\n");
   {
      /* R 만 눌렀을 때 아무 버튼도 안 들어가야 한다. 여기가 접히면 트리거를 누를 때마다
         A+B 가 같이 들어가 커맨드가 오염된다. */
      unsigned char a = kofsp_frame(0, (unsigned short)(1u << RP_R));
      ck(a == 0, "R 단독 = 패드 0 (트리거는 게임 버튼이 아니다)");
      ck(kofsp_frame(0, (unsigned short)(1u << RP_L)) == (NGP_A | NGP_B),
         "L 은 여전히 A+B");
      printf("  R 단독 → %02X · L 단독 → %02X\n", a,
             kofsp_frame(0, (unsigned short)(1u << RP_L)));
   }

   printf("\n4) 엔진 토글·미측정 개수\n");
   ck(kofsp_engine_on() == 0, "엔진 기본 꺼짐");
   kofsp_set_engine(1); ck(kofsp_engine_on() == 1, "켜짐 반영");
   kofsp_set_engine(0); ck(kofsp_engine_on() == 0, "꺼짐 반영");
   printf("  미측정 상수 %d개 (0 이 되는 날이 사냥 끝)\n", kofsp_unmeasured_count());
   ck(kofsp_unmeasured_count() > 0, "M1 은 아직 하나도 안 쟀어야 한다");

   printf("\n%s\n", fails ? "★ 실패 있음" : "전부 통과");
   return fails ? 1 : 0;
}
