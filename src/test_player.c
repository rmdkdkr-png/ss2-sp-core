/* v3 플레이어 축 회귀 — 내가 뒤지는 소강에서 응원(PCHEER)이 나오고,
   %m 이 내 캐릭터 이름으로 채워지는지(리터럴 %m 유출 금지). */
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
#define OPPID  0x17DF

static char seen[128][160];
static int  nseen;
static void w16(int off, int v){ ram[off] = v & 0xFF; ram[off+1] = (v>>8) & 0xFF; }
static void step(int mode, int scr, int h1, int h2)
{
    const char *l;
    ram[MODE] = mode; ram[SCR] = scr; ram[HP1] = h1; ram[HP2] = h2;
    w16(ACT1, 8); w16(ACT2, 8);
    l = ss2comm_frame();
    if(l && nseen < 128) snprintf(seen[nseen++], 160, "%s", l);
}

int main(void)
{
    int i, ok_cheer = 0, ok_leak = 1, saw_name = 0;
    ss2comm_set_ram(ram);
    ss2comm_set_enabled(1);
    ss2comm_set_speaker(0);
    ss2comm_reset();
    memset(ram, 0, sizeof(ram));
    ram[BLK1] = 8 * (2*2 + 0);           /* 나 = 하오마루 */
    ram[OPPID] = 3;

    for(i = 0; i < 30; i++) step(0xF0, 0, 128, 128);
    /* 전투 — 내 체력만 깎인 채(뒤지는 판) 조용히 오래 흐른다 → 소강 응원 기대 */
    for(i = 0; i < 300; i++) step(0xF1, 8, 128, 128);
    for(i = 0; i < 40; i++)  step(0xF1, 8, 60, 128);   /* 내가 맞아 60까지 */
    for(i = 0; i < 3000; i++) step(0xF1, 8, 60, 128);

    for(i = 0; i < nseen; i++){
        if(strstr(seen[i], "%m")) ok_leak = 0;
        if(strstr(seen[i], "하오마루")) saw_name = 1;
        if(strstr(seen[i], "물러서지") || strstr(seen[i], "검을 다시 세워라")
           || strstr(seen[i], "승부는 지금부터")) ok_cheer = 1;
        fprintf(stderr, "[LINE] %s\n", seen[i]);
    }
    fprintf(stderr, "cheer=%d name=%d leak없음=%d\n", ok_cheer, saw_name, ok_leak);
    if(!ok_cheer || !ok_leak){ fprintf(stderr, "PLAYER FAIL\n"); return 1; }
    fprintf(stderr, "PLAYER PASS\n");
    return 0;
}
