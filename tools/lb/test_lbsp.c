#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "lbsp.h"
uint8_t CPUExRAM[16384];
int main(void) {
 unsigned on,p,m,n=0; unsigned char rom[48]={0},before[16384];
 memcpy(rom+36,"LASTBLADE",9);lbsp_set_rom(rom,sizeof rom);
 assert(lbsp_rom_ok());
 for(on=0;on<2;on++) {
  lbsp_set_engine(on);
  for(p=0;p<256;p++)for(m=0;m<16;m++) {
   uint16_t ret=((m&1)?2:0)|((m&2)?512:0)|((m&4)?1024:0)|((m&8)?2048:0);
   unsigned expect=p|((m&1)?16:0)|((m&2)?32:0)|((m&4 || (!on&&(m&8)))?48:0);
   if(on&&(m&8))continue;
   assert(lbsp_frame(p,ret)==expect);n++;
  }
 }
 lbsp_set_engine(1);CPUExRAM[0x36e]=4;CPUExRAM[0x370]=4;
 for(m=0;m<128;m++) {
  unsigned i;int seq;
  CPUExRAM[0x1312]=m;lbsp_reset();memcpy(before,CPUExRAM,sizeof before);
  lbsp_frame(0,2048);
  for(i=0;i<sizeof before;i++)if(i<0x1313 || i>=0x1393)assert(before[i]==CPUExRAM[i]);
  seq=lbsp_disp_seq;
  for(i=0;i<300;i++)lbsp_frame(0,2048);
  assert(seq==lbsp_disp_seq);
 }
 for(m=57;m<256;m++) {
  CPUExRAM[0x36e]=m;lbsp_reset();memcpy(before,CPUExRAM,sizeof before);lbsp_frame(0,2048);
  assert(!memcmp(before,CPUExRAM,sizeof before));
 }
 CPUExRAM[0x36e]=36;CPUExRAM[0x370]=44;CPUExRAM[0x36c]=117;
 lbsp_reset();lbsp_frame(0,2048);lbsp_reset();CPUExRAM[0x36c]=80;
 {int seq=lbsp_disp_seq;for(m=0;m<40;m++)lbsp_frame(0,0);assert(seq==lbsp_disp_seq);}
 printf("PASS: %u fold cases; all 128 ring heads; held trigger; invalid actor; queued-air reset\n",n);
 return 0;
}
