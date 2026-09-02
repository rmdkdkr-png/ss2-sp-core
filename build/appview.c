/* 앱이 실제로 화면에 올리는 버퍼를 그대로 만들어 본다.
 *   [0,32)   해설창
 *   [32,64)  심판 칸
 *   [64,216) 게임 화면
 * 게임 자리는 가짜 무늬로 채운다 — 심판 칸이 게임에 덮이면 바로 보이라고.
 *
 *   cc -O1 -DSS2SP_RAM_POINTER -DSS2COMM_TEST -I. -o /tmp/av appview.c ss2comm.c && /tmp/av /tmp/av
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
extern void ss2comm_set_ram(void*); extern void ss2comm_set_enabled(int);
extern void ss2comm_set_speaker(int); extern void ss2comm_reset(void);
extern const char *ss2comm_frame(void);
extern void ss2comm_draw_enable(int); extern void ss2comm_draw(uint16_t*,int,int,int);
extern int  ss2comm_band_h(void); extern int ss2comm_ref_h(void);
extern const char *ss2comm_test_ref_take(void);
extern void ss2comm_test_ref_face(const uint16_t*,const unsigned char*);
extern void ss2comm_set_rom(const void*, unsigned);
#include <stdlib.h>
const char *ss2sp_last_name=0; int ss2sp_last_ok=0;
#include "/tmp/ktest2.h"

#define W 160
#define GH 152
#define BH 32
#define RH 32
#define TOT (BH+GH)   /* 심판은 오버레이 — 제 자리가 없다 */
static uint16_t fb[W*TOT];
static uint8_t  ram[16384];
#define MODE 0x00A7
#define SCR 0x01C0
#define HP1 0x1A46
#define HP2 0x1C46
#define BLK1 0x1B51
#define BLK2 0x1D51
static void w16(int o,int v){ram[o]=v&0xFF;ram[o+1]=(v>>8)&0xFF;}
#define ACT1 0x0E3E
#define ACT2 0x0E7E

/* 게임 자리를 가짜 화면으로 — 격자무늬 + 가운데 굵은 띠 */
static void fake_game(void){
  int x,y;
  for(y=0;y<GH;y++) for(x=0;x<W;x++){
    int r=(y>>3)&1, c=(x>>3)&1;
    uint16_t v = (r^c) ? 0x2965 : 0x18C3;
    if(y>60 && y<92) v = 0x6B4D;
    fb[(BH+y)*W+x] = v;
  }
}
static const char *dir; static int shots;
static void snap(const char *why){
  char p[512]; FILE*f; int x,y,sx,sy; const int S=2;
  snprintf(p,sizeof p,"%s/%02d.ppm",dir,shots);
  f=fopen(p,"wb"); if(!f) return;
  fprintf(f,"P6\n%d %d\n255\n",W*S,TOT*S);
  for(y=0;y<TOT;y++)for(sy=0;sy<S;sy++)for(x=0;x<W;x++){
    uint16_t v=fb[y*W+x];
    uint8_t r=((v>>11)&31)*255/31,g=((v>>5)&63)*255/63,b=(v&31)*255/31;
    for(sx=0;sx<S;sx++){fputc(r,f);fputc(g,f);fputc(b,f);} }
  fclose(f);
  printf("%02d  %s\n", shots, why); shots++;
}
static char lastref[192];
static void tick(void){
  const char *r;
  ss2comm_frame();
  r = ss2comm_test_ref_take();
  if(r) snprintf(lastref,sizeof lastref,"%s",r);
}
static void hold(int n,int m,int s,int a,int b){
  ram[MODE]=m;ram[SCR]=s;ram[HP1]=a;ram[HP2]=b;w16(ACT1,8);w16(ACT2,8);
  while(n-->0) tick();
}
static void shot(const char *why){ fake_game(); ss2comm_draw(fb,W,W,GH); snap(why); }

int main(int argc,char**argv){
  int i;
  dir = argc>1?argv[1]:"/tmp/av";
  ss2comm_set_ram(ram); ss2comm_set_enabled(1); ss2comm_draw_enable(4);
  ss2comm_set_speaker(0); ss2comm_reset(); memset(ram,0,sizeof ram);
  /* 진짜 롬을 물린다 — 초상이 롬에서 나온다 */
  { const char *rp = getenv("SS2_ROM");
    if(rp){ FILE *f=fopen(rp,"rb"); if(f){ static unsigned char *rb; long n;
      fseek(f,0,SEEK_END); n=ftell(f); fseek(f,0,SEEK_SET);
      rb=(unsigned char*)malloc(n); fread(rb,1,n,f); fclose(f);
      ss2comm_set_rom(rb,(unsigned)n); printf("롬 %ld바이트 물림\n", n); } }
    else ss2comm_test_ref_face(KTEST2_PX,KTEST2_A); }
  printf("band_h=%d ref_h=%d  버퍼 %dx%d\n", ss2comm_band_h(), ss2comm_ref_h(), W, TOT);

  ram[BLK1]=16*2; ram[BLK2]=16*3; ram[0x17DF]=3;
  hold(60,0xF0,2,128,128);
  hold(30,0xF1,0,128,128);   shot("문구(VS) 화면 — 심판 호명");
  hold(150,0xF1,0,128,128);
  hold(2,0xF1,8,128,128);    shot("판 시작 0초 — 구호가 바로 서나");
  hold(168,0xF1,8,128,128);  shot("2.8초 — 관계 대사는 아직 전");
  hold(15,0xF1,8,128,128);   shot("3초 — 관계 대사가 붙었나");
  hold(120,0xF1,8,128,90);   shot("공방 중 — 심판 칸이 게임에 안 덮이나");
  hold(300,0xF1,8,128,0);
  hold(60,0xF0,1,128,128);
  hold(20,0xF1,8,128,128);   shot("2판 진입 — 둘째 판 구호");
  hold(60,0xF1,8,128,128);   shot("2판 60프레임 뒤");
  hold(300,0xF1,8,128,0);
  hold(30,0xF1,8,0,0); hold(40,0xF1,0,0,0);  shot("매치 종료 — 승자 호명");
  /* 화자 15명 초상을 차례로 */
  for(i=0;i<15;i++){
    char b[64];
    ss2comm_reset(); memset(ram,0,sizeof ram);
    ram[BLK1]=16*2; ram[BLK2]=16*3; ram[0x17DF]=3;
    ss2comm_set_speaker(i);
    hold(60,0xF0,2,128,128); hold(90,0xF1,8,128,128); hold(60,0xF1,8,128,100);
    snprintf(b,sizeof b,"화자 %d", i); shot(b);
  }
  printf("\n마지막 구호: %s\n", lastref);
  (void)i;
  return 0;
}
