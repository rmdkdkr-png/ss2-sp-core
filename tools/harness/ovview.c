/* 오버레이 두 페이지 미리보기 — 빠른 설정 / SP 배치 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ss2comm.h"

#define W 160
#define H 152
static uint16_t fb[W*H];
static uint8_t ram[16384];

static void snap(const char *p){
  FILE *f=fopen(p,"wb"); int x,y,sx,sy; const int S=3;
  fprintf(f,"P6\n%d %d\n255\n",W*S,H*S);
  for(y=0;y<H;y++)for(sy=0;sy<S;sy++)for(x=0;x<W;x++){
    uint16_t v=fb[y*W+x];
    uint8_t r=((v>>11)&31)*255/31,g=((v>>5)&63)*255/63,b=(v&31)*255/31;
    for(sx=0;sx<S;sx++){fputc(r,f);fputc(g,f);fputc(b,f);} }
  fclose(f);
}
static void checker(void){
  int x,y;
  for(y=0;y<H;y++)for(x=0;x<W;x++)
    fb[y*W+x]=(((x>>3)^(y>>3))&1)?0x4A69:0x2124;
}
int main(int argc,char**argv){
  static unsigned char chat=1,spk=0,ref=1,sides=1,vib=0,cap=0,sp=1;
  ss2comm_set_ram(ram); ss2comm_set_enabled(1); ss2comm_draw_enable(4);
  ss2comm_set_speaker(0); ss2comm_reset();
  ss2sp_set_ram(ram);
  ss2comm_overlay_bind(&chat,&spk,&ref,&sides,0,&vib,&cap,&sp);
  ss2comm_overlay_toggle();
  checker();
  ss2comm_overlay_draw(fb,W,W,H);
  snap(argv[1]);
  /* SP 배치 페이지: 위로 1번(마지막 줄) → 우측 */
  ss2comm_overlay_input(0);
  ss2comm_overlay_input(3);
  ss2comm_overlay_input(1);   /* 첫 슬롯 줄로 */
  checker();
  ss2comm_overlay_draw(fb,W,W,H);
  snap(argv[2]);
  if(argc>3){                 /* 기술 고르기 목록 */
    ss2comm_overlay_input(4);
    checker();
    ss2comm_overlay_draw(fb,W,W,H);
    snap(argv[3]);
    /* B 로 물러나기 검증: 목록→배치→설정→닫힘 */
    ss2comm_overlay_input(5);
    ss2comm_overlay_input(5);
    ss2comm_overlay_input(5);
    fprintf(stderr,"after 3x B: active=%d\n", ss2comm_overlay_active());
  }
  return 0;
}
