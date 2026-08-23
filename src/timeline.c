/* 한 판을 흉내 내서 **언제 말하고 언제 조용한지** 초 단위로 찍는다.
 *
 *   cc -O1 -DSS2SP_RAM_POINTER -I. -o /tmp/tl timeline.c ss2comm.c && /tmp/tl
 *
 * 왜 필요한가: 대전이 빠르면 대사가 끼어들 틈이 적다. 어디가 비는지 눈으로 봐야
 * 대사를 어디에 둘지 정할 수 있다. 램을 흉내 내는 방식은 test_flow.c 와 같다.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

extern void        ss2comm_set_ram(void *p);
extern void        ss2comm_set_enabled(int on);
extern void        ss2comm_set_speaker(int idx);
extern void        ss2comm_reset(void);
extern const char *ss2comm_frame(void);
extern void        ss2comm_set_duo(int on);

const char *ss2sp_last_name = 0;
int         ss2sp_last_ok   = 0;

static uint8_t ram[16384];
#define MODE 0x00A7
#define SCR  0x01C0
#define HP1  0x1A46
#define HP2  0x1C46
#define ACT1 0x0E3E
#define ACT2 0x0E7E
#define BLK1 0x1B51
#define BLK2 0x1D51

static void w16(int off, int v){ ram[off]=v&0xFF; ram[off+1]=(v>>8)&0xFF; }

static int   frame;
static int   said_at[4096], said_n;
static char  said_tx[4096][160];

static void step(int mode,int scr,int hp1,int hp2,int a1,int a2){
    const char *l;
    ram[MODE]=mode; ram[SCR]=scr; ram[HP1]=hp1; ram[HP2]=hp2;
    w16(ACT1,a1); w16(ACT2,a2);
    l = ss2comm_frame();
    if(l && said_n < 4096){ said_at[said_n]=frame; snprintf(said_tx[said_n],160,"%s",l); said_n++; }
    frame++;
}
static void hold(int n,int mode,int scr,int hp1,int hp2){ while(n-- > 0) step(mode,scr,hp1,hp2,8,8); }
/* 한 대 주고받기 — 때린 쪽 동작값을 올렸다 내린다 */
static void hit(int scr,int *hp1,int *hp2,int who,int dmg){
    if(who==2){ *hp2 -= dmg; if(*hp2<0) *hp2=0; } else { *hp1 -= dmg; if(*hp1<0) *hp1=0; }
    step(0xF1,scr,*hp1,*hp2, who==1?0x200:8, who==2?0x200:8);
    step(0xF1,scr,*hp1,*hp2,8,8);
}

int main(void){
    int hp1=128, hp2=128, i, r;
    ss2comm_set_ram(ram); ss2comm_set_enabled(1); ss2comm_set_speaker(0); ss2comm_set_duo(1);
    ss2comm_reset(); memset(ram,0,sizeof ram);
    ram[BLK1]=8*(2*2+0);   /* 하오마루 */
    ram[BLK2]=8*(2*3+0);   /* 겐주로 */

    hold(180, 0xF0, 2, 128, 128);            /* 캐릭터 고르기 3초 */
    hold(180, 0xF0, 6, 128, 128);            /* VS 화면 3초 */

    for(r = 0; r < 2; r++){                  /* 두 라운드 */
        hp1 = hp2 = 128;
        /* 라운드 사이에는 **전투 모드를 잠깐 벗어난다**. 그 복귀가 「라운드 재개」이고,
           심판의 「N번째 판」 구호가 거기 걸린다. 3초 넘게 벗어나면 새 매치로 친다. */
        if(r) hold(60, 0xF0, 1, hp1, hp2);
        hold(90, 0xF1, 8, hp1, hp2);         /* 라운드 시작 1.5초 */
        for(i = 0; i < 9; i++){              /* 공방 — 1.2초에 한 번씩 주고받는다 */
            hit(8, &hp1, &hp2, (i%3==0)?1:2, 14);
            hold(70, 0xF1, 8, hp1, hp2);
            if(hp2<=0) break;
        }
        hp2 = 0;
        hold(240, 0xF1, 8, hp1, hp2);        /* KO 연출 4초 */
    }
    hold(300, 0xF0, 10, 0, 0);               /* 결과 화면 5초 */

    /* ── 결과 ── */
    printf("전체 %d프레임 (%.1f초), 대사 %d줄\n\n", frame, frame/60.0, said_n);
    {
        int prev = 0, gap, big = 0;
        for(i = 0; i < said_n; i++){
            gap = said_at[i] - prev;
            if(gap >= 420) { printf("  %6.1fs  ── 조용 %4.1f초 ──\n", prev/60.0, gap/60.0); big++; }
            printf("  %6.1fs  %s\n", said_at[i]/60.0, said_tx[i]);
            prev = said_at[i];
        }
        gap = frame - prev;
        if(gap >= 420){ printf("  %6.1fs  ── 조용 %4.1f초 ──\n", prev/60.0, gap/60.0); big++; }
        /* 4.5초는 **일부러 둔 최소 간격**이다(GAP_BATTLE). 그건 침묵이 아니다.
           진짜 빈 자리는 그보다 확실히 긴 7초 이상을 센다. */
        printf("\n7초 이상 빈 자리 %d군데\n", big);
        printf("말한 비율: %d줄 / %.0f초 = %.1f초에 한 줄\n",
               said_n, frame/60.0, said_n ? (frame/60.0)/said_n : 0.0);
    }
    return 0;
}
