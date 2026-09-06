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

/* kofsp.c 가 extern 으로 쓰는 램. 시험에서는 우리가 준다.
   (이게 없어 이 시험은 그동안 «링크조차 안 됐다» — 돌지 않는 시험은 시험이 아니다.) */
unsigned char CPUExRAM[16384];

static int fails;

static void ck(int cond, const char *what)
{
   if (!cond) { printf("  ★실패: %s\n", what); fails++; }
}

/* 기준 폴드 — 계약이 세 번 바뀌었다. 그때마다 시험을 «지우지 않고 고쳤다».
     M1: 순정 폴드와 완전히 같음(L·R 둘 다 A+B).
     M2: R 을 SP 트리거로 돌려 **엔진 켬·끔 어느 쪽에서도** 안 접힘.
     M3: 유저 지시 「svc처럼 해라」 — **엔진 끔이면 R 도 A+B**(`svcsp.c:1418` 과 같은 꼴).
         SvC·월화·KOF 셋이 같은 규칙이 됐다. 유저가 익힐 것이 하나다.

   ★ 지금 계약: **Y=A · X=B · L=A+B 는 언제나. R 은 켜면 SP, 끄면 A+B.**
   ⚠ 거는 조건은 «엔진 토글»이다. SvC 만 `ngp_svcsp_basics`(기본기 모드)에 걸려 있어
     조건까지 같지는 않다. */
static unsigned char ref_fold(unsigned char pad, unsigned ret, int engine)
{
   if (ret & (1u << RP_Y)) pad |= NGP_A;
   if (ret & (1u << RP_X)) pad |= NGP_B;
   if (ret & (1u << RP_L)) pad |= (unsigned char)(NGP_A | NGP_B);
   if (!engine && (ret & (1u << RP_R)))     /* 끔이면 R 도 A+B (SvC·월화와 같은 꼴) */
      pad |= (unsigned char)(NGP_A | NGP_B);
   return pad;
}

/* 롬 머리 흉내 — 0x24 에 표식을 박는다. 파일 없이 판별만 볼 때 쓴다. */
static void mkrom(unsigned char *r, const char *tag)
{
   memset(r, 0, 0x40);
   memcpy(r + 0x24, tag, strlen(tag));
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

   /* ── 버튼 계약 (월화와 같다) ───────────────────────────────
      Y=A · X=B · L=A+B 는 언제나. R 은 엔진 켤 때만 SP, 끄면 아무것도 안 한다.
      ⚠ 이 파일은 **원래 R 을 한 번도 안 접었다**(실측: `tools/kof/kof_ab.py` —
        끔 상태에서 R 은 화면에 아무 변화가 없었다). 그런데 코어 옵션 설명에는
        「끄면 R=A+B」라 적혀 있었다 — **설명이 코드가 안 하는 일을 적고 있었다.**
        유저가 「svc처럼 해라」로 정해, 이제 **코드를 설명에 맞췄다.** 둘이 처음으로 일치한다. */
   {
      unsigned char rr[0x40];
      mkrom(rr, "KOF R2");
      kofsp_set_rom(rr, sizeof rr);
      kofsp_set_engine(0); kofsp_reset();
      ck(kofsp_frame(0, (unsigned short)(1u << RP_R)) == (NGP_A | NGP_B),
         "엔진 끔: R 은 A+B 로 접힌다 (SvC 꼴)");
      ck(kofsp_frame(0, (unsigned short)((1u << RP_R) | (1u << RP_L)))
         == (NGP_A | NGP_B),
         "엔진 끔: R+L 을 같이 눌러도 A+B 하나뿐");
      ck(kofsp_frame(0, (unsigned short)(1u << RP_L)) == (NGP_A | NGP_B),
         "엔진 끔: L 한 프레임에 A+B 두 비트가 함께 선다");
      kofsp_set_engine(1); kofsp_reset();
      ck(kofsp_frame(0, (unsigned short)(1u << RP_L)) == (NGP_A | NGP_B),
         "엔진 켬: L 한 프레임에 A+B 두 비트가 함께 선다");
      kofsp_set_engine(0); kofsp_reset();
   }
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

   printf("\n3) kofsp_frame 이 계약대로 접는가 — 전수\n");
   {
      unsigned pad, i;
      long n = 0, bad = 0;
      int engine;
      unsigned char rr3[0x40];
      /* ★ 이 시험은 **자기가 쓰는 상태를 스스로 세운다.** 앞의 「널 방어」가 롬을
         널로 만들어 놓기 때문에, 물려받으면 엔진을 켜도 끔 경로를 탄다. */
      mkrom(rr3, "KOF R2");
      kofsp_set_rom(rr3, sizeof rr3);
      for (engine = 0; engine <= 1; engine++)
      {
         kofsp_set_engine(engine); kofsp_reset();
         for (pad = 0; pad < 256; pad++)
            for (i = 0; i < 16; i++)
            {
               unsigned ret = ((i & 1) ? (1u << RP_Y) : 0)
                            | ((i & 2) ? (1u << RP_X) : 0)
                            | ((i & 4) ? (1u << RP_L) : 0)
                            | ((i & 8) ? (1u << RP_R) : 0);
               unsigned char a, b;
               /* 엔진 켬 + R = 트리거다. 매크로가 돌기 시작해 폴드와 다른 값이
                  나오는 것이 정상이라 여기서 빼고 3b 에서 따로 본다. */
               if (engine && (i & 8)) { kofsp_reset(); continue; }
               a = kofsp_frame((unsigned char)pad, (unsigned short)ret);
               b = ref_fold((unsigned char)pad, ret, engine);
               n++;
               if (a != b) { if (bad < 4) printf("  ★ 엔진%d pad=%02X ret=%04X: %02X != %02X\n",
                                                 engine, pad, ret, a, b); bad++; }
            }
         kofsp_reset();
      }
      kofsp_set_engine(0);
      printf("  %ld 조합 중 어긋남 %ld\n", n, bad);
      if (bad) fails++;
   }

   printf("\n3b) **엔진 켬**에서 트리거(R)는 접히지 않는가\n");
   {
      /* R 만 눌렀을 때 아무 버튼도 안 들어가야 한다. 여기가 접히면 트리거를 누를 때마다
         A+B 가 같이 들어가 커맨드가 오염된다.
         ⚠ **엔진 켬**에서만 참이다 — 끄면 R 은 A+B 로 접힌다(SvC 꼴, 위 계약 시험). */
      unsigned char a;
      unsigned char rr2[0x40];
      /* ⚠ 앞 시험(널 방어)이 롬을 널로 만들어 놨다. 그대로 두면 `!kof_is_rom` 이라
         엔진을 켜도 «끔 경로»를 타서 R 이 A+B 로 접힌다 — 실제로 그렇게 헛짚었다.
         **앞 시험이 남긴 상태를 물려받지 마라.** */
      mkrom(rr2, "KOF R2");
      kofsp_set_rom(rr2, sizeof rr2);
      kofsp_set_engine(1); kofsp_reset();
      a = kofsp_frame(0, (unsigned short)(1u << RP_R));
      ck(a == 0, "R 단독 = 패드 0 (트리거는 게임 버튼이 아니다)");
      ck(kofsp_frame(0, (unsigned short)(1u << RP_L)) == (NGP_A | NGP_B),
         "L 은 여전히 A+B");
      printf("  R 단독 → %02X · L 단독 → %02X\n", a,
             kofsp_frame(0, (unsigned short)(1u << RP_L)));
      kofsp_set_engine(0);      /* ★ 켠 채 나가면 뒤의 「엔진 기본 꺼짐」이 깨진다 */
      kofsp_reset();
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
