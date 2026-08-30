/* 효과음 훅 단위 시험 — 에뮬레이터도 롬도 없이 램만 흉내 내서 돌린다.
   (test_flow.c 와 같은 방식. ss2comm.c 를 직접 링크해 체력만 깎는다.)

   묻는 것 둘:
     ① 체력이 깎이면 ss2sfx_hit 이 정말 불리는가 — 훅이 감지 원점에 제대로 붙었나.
     ② 세기 구분이 기둥 흔들림(st_shk)과 같은 경계로 갈리는가 — 화면과 소리가 어긋나면 어색하다.

   해설 쪽 억제(emit_ex 의 쿨다운)에 걸리지 않는지도 여기서 드러난다.
   연속 타격을 쿨다운보다 훨씬 촘촘히 넣어 **전부** 세어지는지 본다. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

extern void        ss2comm_set_ram(void *p);
extern void        ss2comm_set_enabled(int on);
extern void        ss2comm_reset(void);
extern const char *ss2comm_frame(void);

/* ss2sp 쪽 심볼 — 여기서는 안 쓴다 */
const char *ss2sp_last_name = 0;
int         ss2sp_last_ok   = 0;

/* ss2sfx 대신 계기만 두는 가짜 — 실제 재생기는 따로 시험한다(sfxtest).
   여기서 보는 것은 「불렸는가·어떤 세기로」다. */
static int hit_n, hit_l, hit_m, hit_h, last_dmg;
void ss2sfx_hit(int dmg){
  hit_n++; last_dmg = dmg;
  if(dmg >= 12) hit_h++;
  else if(dmg >= 4) hit_m++;
  else if(dmg > 0) hit_l++;
}

static uint8_t ram[16384];
#define MODE 0x00A7
#define SCR  0x01C0
#define HP1  0x1A46
#define HP2  0x1C46
#define ACT1 0x0E3E
#define ACT2 0x0E7E
#define BLK1 0x1B51
#define BLK2 0x1D51

static void w16(int off, int v){ ram[off] = v & 0xFF; ram[off+1] = (v>>8) & 0xFF; }

static void step(int hp1, int hp2){
  ram[MODE] = 0xF1; ram[SCR] = 8;
  ram[HP1] = hp1; ram[HP2] = hp2;
  w16(ACT1, 8); w16(ACT2, 8);
  ss2comm_frame();
}

static void begin(void){
  hit_n = hit_l = hit_m = hit_h = 0;
  ss2comm_reset();
  memset(ram, 0, sizeof ram);
  ram[BLK1] = 8 * (2*2 + 0);
  ram[BLK2] = 8 * (2*2 + 1);
  ss2comm_set_ram(ram);
  ss2comm_set_enabled(1);
  step(128, 128); step(128, 128);
}

static int fail;
static void ck(const char *what, int got, int want){
  if(got == want) printf("  OK   %-34s %d\n", what, got);
  else { printf("  FAIL %-34s 얻음 %d / 기대 %d\n", what, got, want); fail = 1; }
}

int main(void){
  int i, hp;

  /* ① 한 대 — 세기별로 한 번씩 */
  begin(); step(128, 126); ck("약(2) 한 대", hit_l, 1);
  begin(); step(128, 122); ck("중(6) 한 대", hit_m, 1);
  begin(); step(128, 114); ck("강(14) 한 대", hit_h, 1);

  /* ② 경계 — 기둥 흔들림과 같은 자리에서 갈려야 한다 (>=12 강 / >=4 중) */
  begin(); step(128, 125); ck("경계 3 = 약", hit_l, 1);
  begin(); step(128, 124); ck("경계 4 = 중", hit_m, 1);
  begin(); step(128, 117); ck("경계 11 = 중", hit_m, 1);
  begin(); step(128, 116); ck("경계 12 = 강", hit_h, 1);

  /* ③ 연타 — 해설 쿨다운(수십~백수십 프레임)보다 훨씬 촘촘히.
        emit_ex 를 탔다면 여기서 대부분 씹혀 개수가 모자란다. */
  begin();
  hp = 128;
  for(i = 0; i < 12; i++){ hp -= 5; step(128, hp); }
  ck("연타 12대 (매 프레임)", hit_n, 12);

  /* ④ 안 맞은 프레임은 안 울려야 한다 */
  begin();
  for(i = 0; i < 30; i++) step(128, 128);
  ck("체력 그대로면 무음", hit_n, 0);

  /* ⑤ 내가 맞아도 울린다 (양쪽 다) */
  begin(); step(120, 128); ck("내가 맞음(8)", hit_m, 1);

  printf("==== sfx hook %s ====\n", fail ? "FAIL" : "PASS");
  return fail;
}
