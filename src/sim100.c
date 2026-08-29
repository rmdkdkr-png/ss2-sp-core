/* 100판 시뮬레이터 — 합성 램으로 매치를 돌려 해설 대사를 전량 수집한다.
   stderr: [MATCH]/[KO]/[END] 마커 + 엔진 [AIR]/[REF] 진단(SS2COMM_DBGSEQ=1). */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

extern void        ss2comm_set_ram(void *p);
extern void        ss2comm_set_enabled(int on);
extern void        ss2comm_set_speaker(int idx);
extern void        ss2comm_reset(void);
extern const char *ss2comm_frame(void);
extern const char *ss2comm_speaker_name(int);

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
#define BLK2   0x1D51
#define SEQTXT 0x17D1
#define JING   0x0000
#define OPPID  0x17DF

static int h1, h2;   /* 시뮬 체력 */
static int late_op = -1;   /* 늦탑재 시나리오 — 인트로 초입 6프레임 뒤 상대 개체가 실린다 */
static void w16(int off, int v){ ram[off] = v & 0xFF; ram[off+1] = (v>>8) & 0xFF; }
static void stepf(int mode, int scr)
{
    ram[MODE] = mode; ram[SCR] = scr;
    ram[HP1] = (uint8_t)(h1 < 0 ? 0 : h1); ram[HP2] = (uint8_t)(h2 < 0 ? 0 : h2);
    w16(ACT1, 8); w16(ACT2, 8);
    ss2comm_frame();
}
static void battle(int n){ int i; for(i = 0; i < n; i++) stepf(0xF1, 8); }
static void seqdown(int start){ int v; for(v = start; v >= 0; v--){ ram[SEQTXT] = (uint8_t)v; stepf(0xF0, 8); } }

static void intro(int first)
{
    int i;
    stepf(0xF0, 8);
    for(i = 0; i < 5; i++) stepf(0xF0, 8);
    if(late_op >= 0){ ram[OPPID] = (uint8_t)late_op; late_op = -1; }
    if(first){ seqdown(15); seqdown(30); }
    seqdown(33);
    ram[JING] = 2; stepf(0xF0, 8); ram[JING] = 0;
    for(i = 0; i < 10; i++) stepf(0xF0, 8);
}
static void myhit(int d){ h2 -= d; battle(2); }     /* 상대가 맞음 */
static void taken(int d){ h1 -= d; battle(2); }

/* 한 라운드: win=1 이면 내가 이긴다. pace: 0 빠름 / 1 보통 / 2 그라인드(소강 김) */
static void round_play(int win, int pace)
{
    int gap = pace == 0 ? 90 : pace == 1 ? 200 : 420;
    int quiet = pace == 2 ? 900 : 300;
    int i;
    for(i = 0; i < 4; i++){
        if(win){ myhit(10); battle(gap); if(i == 1){ taken(14); battle(gap); } }
        else   { taken(12); battle(gap); if(i == 1){ myhit(8);  battle(gap); } }
    }
    battle(quiet);                                   /* 소강 — 썰·응원 자리 */
    if(win){ myhit(12); battle(120); h2 = 0; }
    else   { taken(12); battle(120); h1 = 0; }
    battle(110);                                     /* KO 연출 */
    fprintf(stderr, "[KO win=%d]\n", win);
    battle(160);                                     /* 팻말 시간대 */
    h1 = 128; h2 = 128;
}

int main(int argc, char **argv)
{
    int N = argc > 1 ? atoi(argv[1]) : 100;
    int S = argc > 2 ? atoi(argv[2]) : 0;   /* 변주 시드 — 매핑 오프셋 */
    int m;
    ss2comm_set_ram(ram);
    ss2comm_set_enabled(1);
    for(m = 0; m < N; m++){
        int spk = (m + S) % 15;
        int me  = (m * 7 + 2 + S * 5) % 15;
        int op  = (m * 3 + 1 + S * 11) % 14;        /* 유가(14) 제외 */
        int flavor = (m + S * 2) % 5;
        int i;
        if(op == me) op = (op + 1) % 14;
        ss2comm_set_speaker(spk);
        ss2comm_reset();
        memset(ram, 0, sizeof(ram));
        ram[BLK1] = (uint8_t)(8 * (me * 2));
        ram[BLK2] = (uint8_t)(8 * (op * 2));   /* 실기: 인트로에도 상대 블록이 실려 있다 */
        ram[OPPID] = (uint8_t)op;
        if(m % 10 == 0){ ram[OPPID] = 0xFF; late_op = op; }   /* 첫 대전형 늦탑재 재현 */
        h1 = 128; h2 = 128;
        fprintf(stderr, "[MATCH %d spk=%s me=%d op=%d flavor=%d]\n",
                m, ss2comm_speaker_name(spk), me, op, flavor);
        for(i = 0; i < (m % 10 == 0 ? 700 : 30); i++) stepf(0xF0, 0);   /* 늦탑재 매치는 실기처럼 공백을 길게 — 이어받기 창(600f) 밖 */
        intro(1);
        /* 인트로 초입은 실기처럼 0xFF, 전투에 들어가면 개체가 실린다(OPPID 복원) —
           내내 0xFF 로 두면 전투 진입부가 매 라운드를 새 매치로 오판한다(검수 실증) */
        switch(flavor){
        case 0: round_play(1, 1); ram[OPPID] = 0xFF; intro(0); ram[OPPID] = (uint8_t)op; round_play(1, 1); break;
        case 1: round_play(0, 1); ram[OPPID] = 0xFF; intro(0); ram[OPPID] = (uint8_t)op; round_play(0, 0); break;
        case 2: round_play(1, 0); ram[OPPID] = 0xFF; intro(0); ram[OPPID] = (uint8_t)op; round_play(0, 1);
                ram[OPPID] = 0xFF; intro(0); ram[OPPID] = (uint8_t)op; round_play(1, 2); break;
        case 3: round_play(1, 0); ram[OPPID] = 0xFF; intro(0); ram[OPPID] = (uint8_t)op; round_play(1, 0); break;
        case 4: round_play(0, 2); ram[OPPID] = 0xFF; intro(0); ram[OPPID] = (uint8_t)op; round_play(0, 2); break;
        }
        /* 결과 화면 */
        stepf(0xF1, 2); stepf(0xF1, 2); stepf(0xF1, 2); stepf(0xF1, 2); stepf(0xF1, 2); stepf(0xF1, 2);
        for(i = 0; i < 150; i++) stepf(0xF0, 0);
        fprintf(stderr, "[END %d]\n", m);
    }
    return 0;
}
