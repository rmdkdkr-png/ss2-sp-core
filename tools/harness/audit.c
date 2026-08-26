/* 장면 검수 — 실플레이 RAM 스냅샷을 엔진에 물려 어떤 대사가 나오는지 본다.
   사용: audit <ram파일> <프레임수>  →  stdout: 프레임@대사 목록 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ss2comm.h"

const char *ss2sp_last_name = 0;
int ss2sp_last_ok = 0;

static uint8_t ram[16384];

int main(int argc, char **argv){
  FILE *f; int n, i, cnt = 0;
  if(argc < 3) return 1;
  f = fopen(argv[1], "rb");
  if(!f){ printf("!open\n"); return 1; }
  fread(ram, 1, sizeof ram, f); fclose(f);
  n = atoi(argv[2]);
  ss2comm_set_ram(ram); ss2comm_set_enabled(1); ss2comm_draw_enable(4);
  ss2comm_set_speaker(0); ss2comm_reset();
  { const char *rp = getenv("SS2_ROM"); FILE *rf = rp ? fopen(rp, "rb") : 0;
    if(rf){ static unsigned char *rb; long sz; fseek(rf,0,SEEK_END); sz=ftell(rf); fseek(rf,0,SEEK_SET);
      rb = malloc(sz); fread(rb,1,sz,rf); fclose(rf); ss2comm_set_rom(rb,(unsigned)sz); } }
  printf("mode=%02X scr=%d opp=%d boss=%d hp1=%d hp2=%d\n",
         ram[0x00A7], ram[0x01C0], ram[0x17DF], ram[0x17E3], ram[0x1A46], ram[0x1C46]);
  for(i = 0; i < n; i++){
    const char *s = ss2comm_frame();
    if(s && *s){ printf("  f%-5d %s\n", i, s); if(++cnt >= 12){ printf("  ...\n"); break; } }
  }
  printf("  총 %d건\n", cnt);
  return 0;
}
