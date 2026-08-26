/* 양옆 아트웍 기둥 미리보기 — 앱 합성(288x216)을 그대로 흉내 낸다.
   cc -O1 -DSS2SP_RAM_POINTER -DSS2COMM_TEST -I. -o /tmp/sv sideview.c ss2comm.c */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
extern void ss2comm_set_ram(void*); extern void ss2comm_set_enabled(int);
extern void ss2comm_set_speaker(int); extern void ss2comm_reset(void);
extern const char *ss2comm_frame(void);
extern void ss2comm_draw_enable(int); extern void ss2comm_draw(uint16_t*,int,int,int);
extern void ss2comm_side(uint16_t*,int,int,int,int);
extern void ss2comm_side_feed(const uint16_t*,const uint16_t*,int);
extern void ss2comm_set_rom(const void*, unsigned);
const char *ss2sp_last_name=0; int ss2sp_last_ok=0;
#define SW 64
#define GW 160
#define GH 152
#define BH 32
#define TW (GW+2*SW)
#define TH (GH+BH)
static uint16_t fb[TW*TH];
static uint8_t ram[16384];
#define MODE 0x00A7
#define SCR 0x01C0
#define HP1 0x1A46
#define HP2 0x1C46
#define BLK1 0x1B51
#define BLK2 0x1D51
static void w16(int o,int v){ram[o]=v&0xFF;ram[o+1]=(v>>8)&0xFF;}
static void fake_game(void){
  int x,y;
  for(y=0;y<GH;y++) for(x=0;x<GW;x++){
    int r=(y>>3)&1, c=(x>>3)&1;
    fb[(BH+y)*TW+SW+x] = (r^c)?0x2965:0x18C3;
  }
}
static void snap(const char*p){
  FILE*f=fopen(p,"wb"); int x,y,sx,sy; const int S=2;
  fprintf(f,"P6\n%d %d\n255\n",TW*S,TH*S);
  for(y=0;y<TH;y++)for(sy=0;sy<S;sy++)for(x=0;x<TW;x++){
    uint16_t v=fb[y*TW+x];
    uint8_t r=((v>>11)&31)*255/31,g=((v>>5)&63)*255/63,b=(v&31)*255/31;
    for(sx=0;sx<S;sx++){fputc(r,f);fputc(g,f);fputc(b,f);} }
  fclose(f);
}
static void hold(int n,int m,int s,int a,int b){
  ram[MODE]=m;ram[SCR]=s;ram[HP1]=a;ram[HP2]=b;w16(0x0E3E,8);w16(0x0E7E,8);
  while(n-->0) ss2comm_frame();
}
int main(int argc,char**argv){
  ss2comm_set_ram(ram); ss2comm_set_enabled(1); ss2comm_draw_enable(4);
  ss2comm_set_speaker(0); ss2comm_reset(); memset(ram,0,sizeof ram);
  { const char *rp=getenv("SS2_ROM"); FILE*f=rp?fopen(rp,"rb"):0;
    if(f){ static unsigned char *rb; long n; fseek(f,0,SEEK_END); n=ftell(f); fseek(f,0,SEEK_SET);
      rb=malloc(n); fread(rb,1,n,f); fclose(f); ss2comm_set_rom(rb,(unsigned)n); } }
  /* 전투: 하오마루(화자) / 나 하오마루 vs 쥬베이 */
  ram[BLK1]=8*(2*0+0); ram[BLK2]=8*(2*9+0); ram[0x17DF]=9;
  hold(60,0xF0,2,128,128); hold(200,0xF1,8,128,100);
  fake_game();
  ss2comm_draw(fb+SW, TW, GW, GH);
  ss2comm_side_feed(fb+BH*TW+SW, fb+BH*TW+SW+GW-16, TW);
  ss2comm_side(fb, TW, SW, TH, 0);
  ss2comm_side(fb+SW+GW, TW, SW, TH, 1);
  snap(argv[1]);
  /* 메뉴: 해설자 + 심판 */
  hold(700,0xF0,4,128,128);
  memset(fb,0,sizeof fb); fake_game();
  ss2comm_draw(fb+SW, TW, GW, GH);
  ss2comm_side(fb, TW, SW, TH, 0);
  ss2comm_side(fb+SW+GW, TW, SW, TH, 1);
  snap(argv[2]);
  /* 다시 전투로 — 마지막 대사가 TTL(2.5초)을 넘겨도 흐리게 걸려 있는지 */
  hold(30,0xF1,8,128,100);
  hold(360,0xF1,8,128,100);   /* 6초 침묵 */
  fake_game();
  ss2comm_draw(fb+SW, TW, GW, GH);
  ss2comm_side(fb, TW, SW, TH, 0);
  ss2comm_side(fb+SW+GW, TW, SW, TH, 1);
  if(argc>3) snap(argv[3]);
  return 0;
}
