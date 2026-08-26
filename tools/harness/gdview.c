/* 기둥 대형 일러 검증 — 하오마루 vs 아수라 대전을 흉내내고 기둥을 찍는다 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ss2comm.h"

const char *ss2sp_last_name = 0;
int ss2sp_last_ok = 0;

#define SW 64
#define GW 160
#define GH 152
#define BH 32
#define TW (SW+GW+SW)
#define TH (BH+GH+32)
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
  /* 전투: 하오마루(roster2) vs 아수라(roster8, boss0) */
  {int c1=argc>2?atoi(argv[2]):2, c2=argc>3?atoi(argv[3]):8; int wp=argc>4?atoi(argv[4]):0; ram[BLK1]=8*(2*c1+wp); ram[BLK2]=8*(2*c2+wp); ram[0x17DF]=c2; ram[0x17E3]=(argc>5)?1:0;}
  hold(60,0xF0,2,128,128); hold(200,0xF1,8,128,100);
  fake_game();
  ss2comm_draw(fb+SW, TW, GW, GH);
  ss2comm_side(fb, TW, SW, TH, 0);
  ss2comm_side(fb+SW+GW, TW, SW, TH, 1);
  snap(argv[1]);
  return 0;
}
