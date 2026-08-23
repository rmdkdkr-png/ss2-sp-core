/* 심판(쿠로코)만 집중 검사. 구호가 **언제** 서는지, **무엇을** 말하는지,
 * 안 서야 할 판에서 조용한지, 그림이 제대로 나오는지 한 번에 본다.
 *
 *   cc -O1 -DSS2SP_RAM_POINTER -DSS2COMM_TEST -I. -o /tmp/rt reftest.c ss2comm.c
 *   /tmp/rt /tmp/rt_shots
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
extern void ss2comm_set_ram(void*); extern void ss2comm_set_enabled(int);
extern void ss2comm_set_speaker(int); extern void ss2comm_reset(void);
extern const char *ss2comm_frame(void);
extern void ss2comm_draw_enable(int); extern void ss2comm_draw(uint16_t*,int,int,int);
extern const char *ss2comm_test_ref_take(void);
extern void ss2comm_test_ref_face(const uint16_t*,const unsigned char*);
const char *ss2sp_last_name=0; int ss2sp_last_ok=0;
#include "/tmp/ktest2.h"

#define W 160
#define H 152
#define BH 32
#define RH 32
static uint16_t fb[W*(H+BH+RH)];
static uint8_t  ram[16384];
#define MODE 0x00A7
#define SCR 0x01C0
#define HP1 0x1A46
#define HP2 0x1C46
#define ACT1 0x0E3E
#define ACT2 0x0E7E
#define BLK1 0x1B51
#define BLK2 0x1D51
static void w16(int o,int v){ram[o]=v&0xFF;ram[o+1]=(v>>8)&0xFF;}

static const char *dir; static int shots, fno, fails, checks;
static char lastref[192]; static int lastref_f;

/* 아래 칸만 잘라서 저장 — 심판 자리 그대로 */
static void snap(void){
  char p[512]; FILE*f; int x,y,sx,sy; const int S=3;
  snprintf(p,sizeof p,"%s/%03d.ppm",dir,shots++);
  f=fopen(p,"wb"); if(!f) return;
  fprintf(f,"P6\n%d %d\n255\n",W*S,RH*S);
  for(y=0;y<RH;y++)for(sy=0;sy<S;sy++)for(x=0;x<W;x++){
    uint16_t v=fb[(BH+H+y)*W+x];
    uint8_t r=((v>>11)&31)*255/31,g=((v>>5)&63)*255/63,b=(v&31)*255/31;
    for(sx=0;sx<S;sx++){fputc(r,f);fputc(g,f);fputc(b,f);} }
  fclose(f);
}
static void tick(void){
  const char *r;
  ss2comm_frame(); fno++;
  r = ss2comm_test_ref_take();
  if(r){ snprintf(lastref,sizeof lastref,"%s",r); lastref_f=fno;
         memset(fb,0,sizeof fb); ss2comm_draw(fb,W,W,H); snap(); }
}
static void hold(int n,int m,int s,int a,int b){
  ram[MODE]=m;ram[SCR]=s;ram[HP1]=a;ram[HP2]=b;w16(ACT1,8);w16(ACT2,8);
  while(n-->0) tick();
}
static void reset(int me,int opp){
  ss2comm_reset(); memset(ram,0,sizeof ram);
  ram[BLK1]=16*me; ram[BLK2]=16*opp;
  fno=0; lastref[0]=0; lastref_f=-1;
}
static void ck(const char *what,int ok,const char *got){
  checks++; if(!ok) fails++;
  printf("%s %s%s%s\n", ok?"PASS":"FAIL", what, got&&*got?"  → ":"", got?got:"");
}
/* 전투 진입 프레임을 기록해 두고, 구호가 그 프레임에 섰는지 본다 */
static int enter_at;
static void enter(int me,int opp){
  reset(me,opp);
  hold(60,0xF0,2,128,128);
  enter_at = fno + 1;
  hold(120,0xF1,8,128,128);
}
int main(int argc,char**argv){
  int i;
  dir = argc>1?argv[1]:"/tmp/rt_shots";
  ss2comm_set_ram(ram); ss2comm_set_enabled(1); ss2comm_draw_enable(4);
  ss2comm_set_speaker(0); ss2comm_reset();
  ss2comm_test_ref_face(KTEST2_PX,KTEST2_A);

  puts("── 구호가 판이 서는 프레임에 뜨는가 ──");
  enter(2,3);
  { char b[64]; snprintf(b,sizeof b,"진입 %d프레임 / 구호 %d프레임 · %s",enter_at,lastref_f,lastref);
    ck("첫 판 구호가 진입 프레임에", lastref_f==enter_at && strstr(lastref,"첫 판")!=0, b); }

  puts("\n── 세 판을 치면 첫·둘째·셋째가 차례로 ──");
  { const char *want[3]={"첫 판","둘째 판","셋째 판"}; int ok=1;
    enter(2,3);
    for(i=0;i<3;i++){
      char b[64];
      if(i){ hold(60,0xF0,1,128,128); hold(120,0xF1,8,128,128); }
      snprintf(b,sizeof b,"%s",lastref);
      if(!strstr(lastref,want[i])) ok=0;
      printf("      %d번째 판 → %s\n", i+1, lastref);
      hold(300,0xF1,8,128,0);        /* 이기고 다음 판으로 */
    }
    ck("첫 → 둘째 → 셋째", ok, 0); }

  puts("\n── 승자 본명 15명 ──");
  { int allok=1;
    static const char *want[15]={"카자마 카즈키","카자마 소게츠","하오마루","키바가미 겐주로",
      "나코루루","리무루루","핫토리 한조","갈포드","아수라","샤를로트 크리스틴 드 콜데",
      "모로즈미 타무리키","타치바나 우쿄","야규 쥬베이","시키","유가"};
    for(i=0;i<15;i++){
      int opp = (i==3)?2:3;                 /* 나와 다른 상대 */
      if(i==14) continue;                   /* 유가는 심판이 안 선다 — 아래에서 따로 */
      enter(i,opp);
      hold(120,0xF1,8,128,128); hold(300,0xF1,8,128,0);   /* 1판 승 */
      hold(60,0xF0,1,128,128); hold(120,0xF1,8,128,128);
      hold(300,0xF1,8,128,0);                              /* 2판 승 → 매치 종료 */
      hold(30,0xF1,8,0,0); hold(200,0xF1,0,0,0);           /* 결과 화면 */
      if(!strstr(lastref,want[i]) || !strstr(lastref,"훌륭하오")){ allok=0;
        printf("      %-22s → %s\n", want[i], lastref); }
      else printf("      %s\n", lastref);
    }
    ck("본명 14명 호명 (유가 제외)", allok, 0); }

  puts("\n── 심판이 안 서야 하는 판 ──");
  enter(2,15);                                  /* 표 밖 = 간다라 */
  ck("간다라전 — 구호 없음", lastref[0]==0, lastref);
  enter(2,14);                                  /* 유가 */
  ck("유가전 — 구호 없음", lastref[0]==0, lastref);
  enter(2,3);
  ck("보통 판 — 구호 있음", lastref[0]!=0, lastref);

  puts("\n── 늦게는 안 나온다 ──");
  { reset(2,3); hold(60,0xF0,2,128,128);
    /* 진입 직후 곧바로 또 진입 — 앞 구호가 간격에 막혀 밀린다 */
    hold(30,0xF1,8,128,128);
    { int first=lastref_f; char b[64];
      hold(20,0xF0,1,128,128); hold(200,0xF1,8,128,128);
      snprintf(b,sizeof b,"첫 구호 %d프레임, 마지막 구호 %d프레임",first,lastref_f);
      /* 밀린 구호는 버려야 한다 — 1.5초(90프레임)를 넘겨 뜨면 실패 */
      ck("밀린 구호는 버린다", lastref_f - first > 90 ? 0 : 1, b); } }

  printf("\n==== %d passed / %d failed ====\n", checks-fails, fails);
  printf("띠 그림 %d장 → %s\n", shots, dir);
  return fails?1:0;
}
