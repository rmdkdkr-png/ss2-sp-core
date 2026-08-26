/* 전황 연출 검증 — 매치포인트 숨쉬는 금테, 히트 흔들림·섬광을 컷으로 찍는다
   사용: effview <out접두> — out_mp1/mp2(금테 위상), out_hit0/1/2(충격) .ppm */
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
#define TH GH
static uint16_t fb[(SW*2+GW)*TH];
static uint8_t ram[16384];
#define TW (SW*2+GW)
static void w16(int o,int v){ram[o]=v&0xFF;ram[o+1]=(v>>8)&0xFF;}
static void snap(const char*p){
  FILE*f=fopen(p,"wb"); int x,y;
  fprintf(f,"P6\n%d %d\n255\n",TW,TH);
  for(y=0;y<TH;y++)for(x=0;x<TW;x++){
    uint16_t v=fb[y*TW+x];
    fputc(((v>>11)&31)*255/31,f);fputc(((v>>5)&63)*255/63,f);fputc((v&31)*255/31,f);}
  fclose(f);
}
static void frames(int n){ while(n-->0) ss2comm_frame(); }
static void draw(void){
  int x,y;
  for(y=0;y<GH;y++)for(x=0;x<GW;x++)
    fb[y*TW+SW+x]=(((x>>3)^(y>>3))&1)?0x2965:0x18C3;
  ss2comm_side(fb,TW,SW,TH,0);
  ss2comm_side(fb+SW+GW,TW,SW,TH,1);
}
int main(int argc,char**argv){
  char path[256];
  ss2comm_set_ram(ram); ss2comm_set_enabled(1); ss2comm_draw_enable(4);
  ss2comm_set_speaker(0); ss2comm_reset(); memset(ram,0,sizeof ram);
  { const char *rp=getenv("SS2_ROM"); FILE*f=rp?fopen(rp,"rb"):0;
    if(f){ static unsigned char *rb; long n; fseek(f,0,SEEK_END); n=ftell(f); fseek(f,0,SEEK_SET);
      rb=malloc(n); fread(rb,1,n,f); fclose(f); ss2comm_set_rom(rb,(unsigned)n); } }
  /* 하오마루 vs 겐주로, 전투 */
  ram[0x1B51]=8*(2*2+0); ram[0x1D51]=8*(2*3+0); ram[0x17DF]=3; ram[0x17E3]=0;
  ram[0x00A7]=0xF0; ram[0x01C0]=2; ram[0x1A46]=128; ram[0x1C46]=128;
  w16(0x0E3E,8); w16(0x0E7E,8);
  frames(60);
  ram[0x00A7]=0xF1; ram[0x01C0]=8; frames(120);
  /* 1라운드 승리 → 매치포인트 */
  ram[0x1C46]=60; frames(4); ram[0x1C46]=0; frames(40);
  ram[0x1A46]=128; ram[0x1C46]=128; frames(80);
  frames(9);  draw(); snprintf(path,256,"%s_mp1.ppm",argv[1]); snap(path);  /* 위상 어둡 */
  frames(15); draw(); snprintf(path,256,"%s_mp2.ppm",argv[1]); snap(path);  /* 위상 밝음 */
  /* 상대에게 큰 타격 → 우측 흔들림+섬광 */
  ram[0x1C46]=128-14; ss2comm_frame();
  draw(); snprintf(path,256,"%s_hit0.ppm",argv[1]); snap(path);   /* 섬광 프레임 */
  frames(3); draw(); snprintf(path,256,"%s_hit1.ppm",argv[1]); snap(path);
  frames(4); draw(); snprintf(path,256,"%s_hit2.ppm",argv[1]); snap(path);
  return 0;
}
