/* 홍보 캡처 — 실플레이 화면(ppm) + 실램(ram)으로 앱 화면(기둥+해설띠+오버레이)을 합성
   사용: promoshot <game.ppm> <ram.bin> <mode> <out.ppm>
   mode: fight | hit | low | over0 | over1 | over2 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ss2comm.h"
#include "ss2sp.h"

#define SW 64
#define GW 160
#define GH 152
#define TW (SW+GW+SW)
#define TH GH
static uint16_t fb[TW*TH];
static uint8_t ram[16384];
static uint8_t game[GW*GH*3];

static void putgame(void){
  int x,y;
  for(y=0;y<GH;y++)for(x=0;x<GW;x++){
    int r=game[(y*GW+x)*3],g=game[(y*GW+x)*3+1],b=game[(y*GW+x)*3+2];
    fb[y*TW+SW+x]=(uint16_t)(((r>>3)<<11)|((g>>2)<<5)|(b>>3));
  }
}
static void snap(const char*p){
  FILE*f=fopen(p,"wb"); int x,y;
  fprintf(f,"P6\n%d %d\n255\n",TW,TH);
  for(y=0;y<TH;y++)for(x=0;x<TW;x++){
    uint16_t v=fb[y*TW+x];
    fputc(((v>>11)&31)*255/31,f);fputc(((v>>5)&63)*255/63,f);fputc((v&31)*255/31,f);}
  fclose(f);
}
static void frames(int n){ while(n-->0) ss2comm_frame(); }
int main(int argc,char**argv){
  FILE*f; const char*mode=argv[3];
  static unsigned char chat=1,spk=0,ref=1,sides=1,vib=0,cap=0,sp=1;
  /* game ppm */
  { char l[128]; int w,h,mx;
    f=fopen(argv[1],"rb");
    fscanf(f,"%s %d %d %d",l,&w,&h,&mx); fgetc(f);
    fread(game,1,GW*GH*3,f); fclose(f); }
  f=fopen(argv[2],"rb"); fread(ram,1,sizeof ram,f); fclose(f);
  ss2comm_set_ram(ram); ss2comm_set_enabled(1); ss2comm_draw_enable(4);
  ss2comm_set_speaker(atoi(getenv("SPK")?getenv("SPK"):"0"));
  ss2comm_reset(); ss2sp_set_ram(ram);
  { const char *rp=getenv("SS2_ROM"); FILE*g=rp?fopen(rp,"rb"):0;
    if(g){ static unsigned char *rb; long n; fseek(g,0,SEEK_END); n=ftell(g); fseek(g,0,SEEK_SET);
      rb=malloc(n); fread(rb,1,n,g); fclose(g); ss2comm_set_rom(rb,(unsigned)n); } }
  ss2comm_overlay_bind(&chat,&spk,&ref,&sides,0,&vib,&cap,&sp);
  /* 전투 진입 흐름을 흉내 — 램은 실전 스냅샷 그대로, 모드만 잠깐 굴린다 */
  { uint8_t m=ram[0x00A7], s=ram[0x01C0];
    ram[0x00A7]=0xF0; ram[0x01C0]=2; frames(50);
    ram[0x00A7]=m; ram[0x01C0]=s; frames(200); }
  if(!strcmp(mode,"hit")){
    ram[0x1C46]=(uint8_t)(ram[0x1C46]>14?ram[0x1C46]-14:10); frames(1);
  }else if(!strcmp(mode,"low")){
    ram[0x1A46]=18; frames(30);
  }else if(!strncmp(mode,"over",4)){
    int page=mode[4]-'0';
    ss2comm_overlay_toggle();
    if(page>=1){ ss2comm_overlay_input(0); ss2comm_overlay_input(3); }
    if(page>=2){ ss2comm_overlay_input(1); ss2comm_overlay_input(1);
                 ss2comm_overlay_input(1); ss2comm_overlay_input(1);
                 ss2comm_overlay_input(1); ss2comm_overlay_input(4); }
  }
  putgame();
  ss2comm_draw(fb+SW, TW, GW, GH);
  ss2comm_side(fb, TW, SW, TH, 0);
  ss2comm_side(fb+SW+GW, TW, SW, TH, 1);
  if(!strncmp(mode,"over",4)) ss2comm_overlay_draw(fb+SW, TW, GW, GH);
  snap(argv[4]);
  return 0;
}
