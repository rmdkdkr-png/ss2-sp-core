/* 타일 배경 굽기 검증 — 진짜 스테이지 VRAM 을 먹여 기둥을 렌더한다 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
extern void ss2comm_set_ram(void*); extern void ss2comm_set_enabled(int);
extern void ss2comm_reset(void); extern void ss2comm_set_rom(const void*, unsigned);
extern void ss2comm_side(uint16_t*,int,int,int,int);
extern void ss2comm_side_tiles(const unsigned char*,const unsigned char*,const unsigned char*,int,int);
const char *ss2sp_last_name=0; int ss2sp_last_ok=0;
static uint8_t ram[16384];
static uint16_t fb[288*216];
static unsigned char b1[2048], b2[8192], b3[512];
static void load(const char*p, void*d, int n){ FILE*f=fopen(p,"rb"); fread(d,1,n,f); fclose(f); }
int main(int argc, char**argv){
  int sx, sy; FILE*f;
  ss2comm_set_ram(ram); ss2comm_set_enabled(1); ss2comm_reset();
  { const char *rp=getenv("SS2_ROM"); f=fopen(rp,"rb");
    if(f){ static unsigned char *rb; long n; fseek(f,0,SEEK_END); n=ftell(f); fseek(f,0,SEEK_SET);
      rb=malloc(n); fread(rb,1,n,f); fclose(f); ss2comm_set_rom(rb,(unsigned)n); } }
  load(argv[1],b1,2048); load(argv[2],b2,8192); load(argv[3],b3,512);
  sscanf(argv[4], "%d %d", &sx, &sy);
  ram[0x1B51]=0x80; ram[0x1D51]=0x90; ram[0x17DF]=9;   /* 아수라 vs 샤를로트 */
  { int i; for(i=0;i<400;i++){ ram[0x00A7]=0xF1; ram[0x01C0]=8; ram[0x1A46]=128; ram[0x1C46]=100;
      extern const char *ss2comm_frame(void); ss2comm_frame(); } }
  ss2comm_side_tiles(b1,b2,b3,sx,sy);
  { int x,y; for(y=0;y<216;y++) for(x=0;x<288;x++) fb[y*288+x] = (y>=64&&x>=64&&x<224)?0x39E7:0x0000; }
  ss2comm_side(fb, 288, 64, 216, 0);
  ss2comm_side(fb+224, 288, 64, 216, 1);
  f=fopen(argv[5],"wb");
  fprintf(f,"P6\n288 216\n255\n");
  { int i; for(i=0;i<288*216;i++){ uint16_t v=fb[i];
      fputc(((v>>11)&31)*255/31,f); fputc(((v>>5)&63)*255/63,f); fputc((v&31)*255/31,f); } }
  fclose(f);
  return 0;
}
