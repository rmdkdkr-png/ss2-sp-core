/* 2회전 콜 회귀 — 상대 개체가 인트로에서 미인식(0xFF)이어도 직전 라운드 값을
   이어받아 「2회전!」이 나가는지. SS2COMM_DBGSEQ 의 [REF] stderr 를 밖에서 grep. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

extern void        ss2comm_set_ram(void *p);
extern void        ss2comm_set_enabled(int on);
extern void        ss2comm_set_speaker(int idx);
extern void        ss2comm_reset(void);
extern const char *ss2comm_frame(void);

const char *ss2sp_last_name = 0;
int         ss2sp_last_ok   = 0;

static uint8_t ram[16384];
#define MODE   0x00A7
#define SCR    0x01C0
#define HP1    0x1A46
#define HP2    0x1C46
#define ACT1   0x0E3E
#define ACT2   0x0E7E
#define BLK1   0x1B51
#define SEQTXT 0x17D1
#define JING   0x0000
#define OPPID  0x17DF

static void w16(int off, int v){ ram[off] = v & 0xFF; ram[off+1] = (v>>8) & 0xFF; }
static void step(int mode, int scr, int h1, int h2)
{
    ram[MODE] = mode; ram[SCR] = scr; ram[HP1] = h1; ram[HP2] = h2;
    w16(ACT1, 8); w16(ACT2, 8);
    ss2comm_frame();
}
static void seqdown(int start)
{
    int v;
    for(v = start; v >= 0; v--){ ram[SEQTXT] = (uint8_t)v; step(0xF0, 8, 128, 128); }
}

int main(void)
{
    int i;
    ss2comm_set_ram(ram);
    ss2comm_set_enabled(1);
    ss2comm_set_speaker(0);
    ss2comm_reset();
    memset(ram, 0, sizeof(ram));
    ram[BLK1] = 8 * (2*2 + 0);            /* 나 = 하오마루 */
    ram[OPPID] = 3;                        /* 상대 = 정규 로스터 3 */

    for(i = 0; i < 30; i++) step(0xF0, 0, 128, 128);   /* 메뉴 */
    /* ── 1회전 인트로: 개체 유효 ── */
    step(0xF0, 8, 128, 128);              /* 진입 에지 */
    for(i = 0; i < 6; i++) step(0xF0, 8, 128, 128);
    seqdown(15);                           /* 자아 */
    seqdown(30);
    seqdown(33);                           /* → [REF] 1회전! 기대 */
    ram[JING] = 2; step(0xF0, 8, 128, 128); ram[JING] = 0;   /* 승부! */
    /* ── 전투 200f (lastFight·상대 인지 확립) ── */
    for(i = 0; i < 200; i++) step(0xF1, 8, 128, 100);
    /* ── 라운드 종료 연출 (전투 화면 유지, 상대 KO) ── */
    for(i = 0; i < 120; i++) step(0xF1, 8, 128, 0);
    /* ── 2회전 인트로: 개체 **미인식** ── */
    ram[OPPID] = 0xFF;
    step(0xF0, 8, 128, 128);              /* 진입 에지 */
    for(i = 0; i < 4; i++) step(0xF0, 8, 128, 128);
    seqdown(33);                           /* → [REF] 2회전! 이 나가야 한다 */
    for(i = 0; i < 30; i++) step(0xF0, 8, 128, 128);
    fprintf(stderr, "[DONE]\n");
    return 0;
}
