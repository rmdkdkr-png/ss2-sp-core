/* Last Blade UE SP: 14 profiles plus awakened Kaede (actor ID 0).
 * Measurements and reproducible gates: tools/lb/allchars/.
 * Only direction history is written; ROM, flash, HP and character IDs are untouched.
 */
#include "lbsp.h"
#include <stdlib.h>
#include <string.h>
#ifdef SS2SP_RAM_POINTER
static uint8_t *ram;
void lbsp_set_ram(void *p) { ram = (uint8_t *)p; }
#else
extern uint8_t CPUExRAM[16384];
#define ram CPUExRAM
#endif
#define ACT 0x370
#define CHAR 0x36e
#define FACE 0x386
#define HEAD 0x1312
#define RING 0x1313
#define F 64
#define B 128
/* pre is chronological, and HEAD points to the next ring slot. */
typedef struct { const char *label; uint8_t pre[44], n, live, btn; } LbMove;
static const LbMove moves[15][7] = {
 {{"236A", {2,66}, 2, 64, 16},{"623A", {64,2,66}, 3, 0, 16},{"214A", {2,130}, 2, 128, 16},{"214B", {2,130}, 2, 128, 32},{"41236B", {128,130,2,66}, 4, 64, 32},{0},{0}},
 {{"236A", {2,66}, 2, 64, 16},{"623A", {64,2,66}, 3, 0, 16},{"214A", {2,130}, 2, 128, 16},{"214B", {2,130}, 2, 128, 32},{"41236B", {128,130,2,66}, 4, 64, 32},{0},{0}},
 {{"236A", {2,66}, 2, 64, 16},{"623A", {64,2,66}, 3, 0, 16},{"214A", {2,130}, 2, 128, 16},{"214B", {2,130}, 2, 128, 32},{"41236B", {128,130,2,66}, 4, 64, 32},{0},{0}},
 {{"236A", {2,66}, 2, 64, 16},{"623A", {64,2,66}, 3, 0, 16},{"214A", {2,130}, 2, 128, 16},{"63214B", {64,66,2,130}, 4, 128, 32},{"41236B", {128,130,2,66}, 4, 64, 32},{0},{0}},
 {{"236A", {2,66}, 2, 64, 16},{"623A", {64,2,66}, 3, 0, 16},{"63214A", {64,66,2,130}, 4, 128, 16},{"236B", {2,66}, 2, 64, 32},{"63214B", {64,66,2,130}, 4, 128, 32},{"421A", {128,2,130}, 3, 0, 16},{0}},
 {{"236A", {2,66}, 2, 64, 16},{"623A", {64,2,66}, 3, 0, 16},{"421A", {128,2,130}, 3, 0, 16},{"28B", {2}, 1, 1, 32},{"63214A", {64,66,2,130}, 4, 128, 16},{"63214B", {64,66,2,130}, 4, 128, 32},{0}},
 {{"[4]6A", {128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128}, 40, 64, 16},{"[2]8A", {2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2}, 40, 1, 16},{"214A", {2,130}, 2, 128, 16},{"[4]6B", {128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128}, 40, 64, 32},{0},{0},{0}},
 {{"236A", {2,66}, 2, 64, 16},{"623A", {64,2,66}, 3, 0, 16},{"214A", {2,130}, 2, 128, 16},{"214B", {2,130}, 2, 128, 32},{0},{0},{0}},
 {{"616A", {64,130}, 2, 64, 16},{"623B", {64,2,66}, 3, 0, 32},{"214A", {2,130}, 2, 128, 16},{"63214B", {64,66,2,130}, 4, 128, 32},{"63214A", {64,66,2,130}, 4, 128, 16},{"41236B", {128,130,2,66}, 4, 64, 32},{0}},
 {{"214B", {2,130}, 2, 128, 32},{"[2]8A", {2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2}, 40, 1, 16},{"63214A", {64,66,2,130}, 4, 128, 16},{"AB", {0}, 0, 0, 48},{0},{0},{"236B", {2,66}, 2, 64, 32}},
 {{"236A", {2,66}, 2, 64, 16},{"623B", {64,2,66}, 3, 0, 32},{"214A", {2,130}, 2, 128, 16},{"421A", {128,2,130}, 3, 0, 16},{"63214B", {64,66,2,130}, 4, 128, 32},{"41236B", {128,130,2,66}, 4, 64, 32},{"236A", {2,66}, 2, 64, 16}},
 {{"236A", {2,66}, 2, 64, 16},{"623A", {64,2,66}, 3, 0, 16},{"63214A", {64,66,2,130}, 4, 128, 16},{"214B", {2,130}, 2, 128, 32},{"236B", {2,66}, 2, 64, 32},{0},{"2B", {0}, 0, 2, 32}},
 {{"236A", {2,66}, 2, 64, 16},{"623A", {64,2,66}, 3, 0, 16},{"214B", {2,130}, 2, 128, 32},{"623B", {64,2,66}, 3, 0, 32},{"41236B", {128,130,2,66}, 4, 64, 32},{"AB", {0}, 0, 0, 48},{0}},
 {{"236A", {2,66}, 2, 64, 16},{"623A", {64,2,66}, 3, 0, 16},{"214A", {2,130}, 2, 128, 16},{"623B", {64,2,66}, 3, 0, 32},{"63214B", {64,66,2,130}, 4, 128, 32},{0},{0}},
 {{"236A", {2,66}, 2, 64, 16},{0},{"214A", {2,130}, 2, 128, 16},{"214B", {2,130}, 2, 128, 32},{0},{"AB", {0}, 0, 0, 48},{0}}
};

static int enabled, is_rom, step=-1, left, count, held;
static int pending=-1, pending_char, pending_frames;
static uint8_t direction, pads[48], times[48];
char lbsp_last_disp[64];
int lbsp_disp_seq;
int lbsp_unmeasured_count(void) { return 3; } /* HP, combo, command window */
void lbsp_set_engine(int on) { enabled=!!on; if (!enabled) {step=-1;held=0;pending=-1;} }
int lbsp_engine_on(void) {return enabled;}
void lbsp_reset(void) {step=-1;left=0;held=0;pending=-1;lbsp_last_disp[0]=0;}
void lbsp_set_rom(const void *p,unsigned n) {
 is_rom=p && n>=0x30 && !memcmp((const char *)p+0x24,"LASTBLADE",9);lbsp_reset();
}
int lbsp_rom_ok(void) {return is_rom;}
static uint8_t bits(uint8_t p) {
 return (p&63) | ((p&F)?direction:0) | ((p&B)?(direction==8?4:8):0);
}
static int character(void) {
 unsigned c=ram[CHAR];
 /* Actor dispatch index: 4*ID. Natural P1/P2 selections independently verified. */
 return (c%4==0 && c<=56)?(int)(c/4):-1;
}
static int slot(uint8_t pad) {
 unsigned f=pad&direction,b=pad&(direction==8?4:8),d=pad&2;
 return d?(f?4:b?5:3):f?1:b?2:0;
}
static int ground(int c,uint8_t a) {
 if(c==14) return a==4 || a==12 || a==16;
 return a==4 || a==12 || a==20 || a==24;
}
static void append(unsigned p,unsigned t) {pads[count]=(uint8_t)p;times[count++]=(uint8_t)t;}
static void begin(const LbMove *m) {
 unsigned i; const char *env=getenv("LBSP_RING");
 count=0;
 if(env && *env=='0') {
  for(i=0;i<m->n;i++) append(m->pre[i],m->n>5?2:4);
 } else {
  unsigned h=ram[HEAD];
  for(i=0;i<m->n;i++) ram[RING+((h-m->n+i)&127)]=bits(m->pre[i]);
 }
 if(m->live) append(m->live,2);
 /* Up must remain held with the button. Four up-only frames already jump. */
 append(m->btn | (m->live==1?1:0),4);
 step=0;left=times[0];
 strncpy(lbsp_last_disp,m->label,sizeof(lbsp_last_disp)-1);
 lbsp_last_disp[sizeof(lbsp_last_disp)-1]=0;lbsp_disp_seq++;
}
uint8_t lbsp_frame(uint8_t pad,uint16_t ret) {
 int trig=!!(ret&(1u<<11));
 if(ret&(1u<<1))pad|=16;
 if(ret&(1u<<9))pad|=32;
 if(ret&(1u<<10))pad|=48;
 if(!enabled || !is_rom) {if(trig)pad|=48;step=-1;held=0;return pad;}
#ifdef SS2SP_RAM_POINTER
 if(!ram) {step=-1;held=trig;return pad;}
#endif
 if(pending>=0) {
  int c=character();
  if(c!=pending_char || ++pending_frames>20 || (ram[ACT]!=44 && ram[ACT]!=48)) pending=-1;
  else if(ram[0x36c]<=90) {begin(&moves[c][6]);pending=-1;}
 }
 if(step<0 && pending<0 && trig && !held) {
  int c=character(),s; const LbMove *m=0;
  direction=ram[FACE]==1?4:8;
  if(c>=0) {
   s=slot(pad);
   if(ground(c,ram[ACT]))m=&moves[c][s];
   else if((ram[ACT]==44 || ram[ACT]==48) && moves[c][6].label) {
    if(ram[0x36c]<=90)m=&moves[c][6];
    else if(ram[ACT]==44){pending=6;pending_char=c;pending_frames=0;}
   }
   if(m && m->label)begin(m);
  }
 }
 held=trig;
 if(step>=0) {
  pad=bits(pads[step]);
  if(--left==0){if(++step==count)step=-1;else left=times[step];}
 }
 return pad;
}
