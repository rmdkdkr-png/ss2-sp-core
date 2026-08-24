/* ss2comm 관전 기억(flow/arc) 단위 시험 — 에뮬레이터 없이 램만 흉내 내서 돌린다.
   브라우저판 회귀 14.79~14.86 과 같은 시나리오를 C 엔진에 그대로 먹인다. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "ss2comm_lines.h"

extern void        ss2comm_set_ram(void *p);
extern void        ss2comm_set_enabled(int on);
extern void        ss2comm_set_speaker(int idx);
extern void        ss2comm_reset(void);
extern const char *ss2comm_frame(void);
extern int         ss2comm_speaker_count(void);
extern const char *ss2comm_speaker_name(int);

/* ss2sp 쪽 심볼 — 여기서는 안 쓴다 */
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
#define SURV 0x180B
#define STG  0x17FE

static char seen[64][160];
static int  nseen;

static void w16(int off, int v){ ram[off] = v & 0xFF; ram[off+1] = (v>>8) & 0xFF; }

/* 한 프레임 돌리고 나온 대사를 모은다 */
static void step(int mode, int scr, int hp1, int hp2, int a1, int a2)
{
    const char *l;
    ram[MODE] = mode; ram[SCR] = scr;
    ram[HP1] = hp1; ram[HP2] = hp2;
    w16(ACT1, a1); w16(ACT2, a2);
    l = ss2comm_frame();
    if(l && nseen < 64) snprintf(seen[nseen++], 160, "%s", l);
}
static void idle(int n){ int i; for(i=0;i<n;i++) step(0xF0, 0, 128, 128, 8, 8); }
/* 큐는 한 줄에 100~150프레임을 쓴다. 상태를 그대로 둔 채 넉넉히 돌려 다 빼낸다. */
static int  d_mode=0xF1, d_scr=8, d_h1=128, d_h2=128;
static void drain(int n){ int i; for(i=0;i<n;i++) step(d_mode, d_scr, d_h1, d_h2, 8, 8); }
static void dset(int mode,int scr,int h1,int h2){ d_mode=mode; d_scr=scr; d_h1=h1; d_h2=h2; }

static void begin(const char *what)
{
    (void)what;
    nseen = 0;
    ss2comm_reset();
    memset(ram, 0, sizeof(ram));
    ram[BLK1] = 8 * (2*2 + 0);      /* 하오마루 · 수라 */
    ram[BLK2] = 8 * (2*3 + 0);      /* 겐주로 · 수라 */
    ram[SURV] = 0; ram[STG] = 0;
}
/* 낱말이 아니라 **그 이벤트의 대사표**와 대조한다.
   브라우저 회귀에서 배운 것 — 문안으로 확인하면 대사를 고칠 때마다 검사가 깨진다.
   %s/%d 앞의 고정 부분만 비교하면 서식이 있는 줄도 정확히 잡힌다. */
/* 서식(%s·%d)을 뺀 **고정 조각들이 순서대로** 들어 있으면 그 이벤트의 줄로 본다.
   "%s에 벌써 세 번째군" 처럼 서식으로 시작하는 줄도 이렇게 하면 잡힌다. */
static int lineIsEv(const char *line, int ev, int spk)
{
    int i;
    for(i = 0; i < EVMAXV; i++){
        const char *f = LINES[spk][ev][i];
        const char *p, *cur;
        int ok = 1, chunks = 0;
        if(!f) continue;
        p = f; cur = line;
        while(*p){
            const char *pc = strchr(p, '%');
            size_t n = pc ? (size_t)(pc - p) : strlen(p);
            if(n){
                char buf[160]; const char *hit;
                if(n >= sizeof(buf)) n = sizeof(buf)-1;
                memcpy(buf, p, n); buf[n] = 0;
                hit = strstr(cur, buf);
                if(!hit){ ok = 0; break; }
                cur = hit + n; chunks++;
            }
            if(!pc) break;
            p = pc + 2;                       /* %s · %d 를 건너뛴다 */
        }
        if(ok && chunks) return 1;
    }
    return 0;
}
static int sawEv(int ev, int spk)
{
    int i; for(i = 0; i < nseen; i++) if(lineIsEv(seen[i], ev, spk)) return 1;
    return 0;
}
static int countEv(int ev, int spk)
{
    int i, n = 0; for(i = 0; i < nseen; i++) if(lineIsEv(seen[i], ev, spk)) n++;
    return n;
}
static void dump(void){ int i; for(i=0;i<nseen;i++) printf("      · %s\n", seen[i]); }

static int pass, fail;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if(ok) pass++; else { fail++; dump(); }
}

int main(void)
{
    int i;
    ss2comm_set_ram(ram);
    ss2comm_set_enabled(1);
    ss2comm_set_speaker(0);          /* 하오마루 */

    printf("해설자 %d명: ", ss2comm_speaker_count());
    for(i = 0; i < ss2comm_speaker_count(); i++) printf("%s ", ss2comm_speaker_name(i));
    printf("\n\n");

    /* ── 난타전: 서로 다섯 번 넘게 주고받는다 ── */
    {
        int a = 128, b = 128;
        begin("trade");
        idle(200); step(0xF1, 8, 128, 128, 8, 8);
        for(i = 0; i < 6; i++){
            b -= 6; step(0xF1, 8, a, b, 8, 8);
            a -= 6; step(0xF1, 8, a, b, 8, 8);
        }
        dset(0xF1,8,a,b); drain(900);
        check("흐름: 난타전", sawEv(EV_FLOWTRADE, 0));
    }

    /* ── 일방적: 여섯 대 넣는 동안 한 대도 안 맞음 ── */
    {
        int b = 128;
        begin("oneside");
        idle(200); step(0xF1, 8, 128, 128, 8, 8);
        for(i = 0; i < 8; i++){ b -= 6; step(0xF1, 8, 128, b, 8, 8); }
        dset(0xF1,8,128,b); drain(900);
        check("흐름: 일방적", sawEv(EV_FLOWONE, 0));
    }

    /* ── 쫓김: 여섯 대 맞는 동안 한 대도 못 때림 ── */
    {
        int a = 128;
        begin("chased");
        idle(200); step(0xF1, 8, 128, 128, 8, 8);
        for(i = 0; i < 8; i++){ a -= 6; step(0xF1, 8, a, 128, 8, 8); }
        dset(0xF1,8,a,128); drain(900);
        check("흐름: 계속 쫓김", sawEv(EV_FLOWCHASE, 0));
    }

    /* ── 상대가 필살기를 네 번째 꺼낸다 ── */
    {
        begin("spheavy");
        idle(200); step(0xF1, 8, 128, 128, 8, 8);
        for(i = 0; i < 5; i++){
            step(0xF1, 8, 128, 128, 8, 0x1B0);
            step(0xF1, 8, 128, 128, 8, 8);
        }
        dset(0xF1,8,128,128); drain(900);
        check("흐름: 상대 필살기 남발", sawEv(EV_FLOWSP, 0));
    }

    /* ── 같은 기술로 세 번 적중 ── */
    {
        int b = 128;
        begin("samemove");
        idle(200); step(0xF1, 8, 128, 128, 8, 8);
        ss2sp_last_ok = 1; ss2sp_last_name = "삼련살";
        for(i = 0; i < 3; i++){
            step(0xF1, 8, 128, b, 0x1B0, 8);     /* 발동 (결합창 열림) */
            b -= 10;
            step(0xF1, 8, 128, b, 0x1B0, 8);     /* 적중 */
            step(0xF1, 8, 128, b, 8, 8);         /* 원위치 */
        }
        ss2sp_last_name = 0; ss2sp_last_ok = 0;
        dset(0xF1,8,128,b); drain(900);
        check("흐름: 같은 기술 세 번", sawEv(EV_FLOWSAME, 0));
    }

    /* ── 총평: 첫 판 내주고 내리 두 판 (역전 매치) ── */
    {
        begin("arcComeback");
        idle(200);
        step(0xF1, 8, 128, 128, 8, 8); step(0xF1, 8, 90, 120, 8, 8); step(0xF1, 8, 0, 120, 8, 8);
        idle(10);
        step(0xF1, 8, 128, 128, 8, 8); step(0xF1, 8, 120, 90, 8, 8); step(0xF1, 8, 120, 0, 8, 8);
        idle(10);
        step(0xF1, 8, 128, 128, 8, 8); step(0xF1, 8, 110, 80, 8, 8); step(0xF1, 8, 110, 0, 8, 8);
        step(0xF1, 2, 110, 0, 8, 8);                     /* 결과 화면 */
        dset(0xF1,2,110,0); drain(1200);
        check("총평: 첫 판 내주고 뒤집은 판", sawEv(EV_ARCCOMEBACK, 0) && !sawEv(EV_WINTALK, 0));
    }

    /* ── 총평: 내리 두 판 (2-0) ── */
    {
        begin("arcSweep");
        idle(200);
        step(0xF1, 8, 128, 128, 8, 8); step(0xF1, 8, 120, 90, 8, 8); step(0xF1, 8, 120, 0, 8, 8);
        idle(10);
        step(0xF1, 8, 128, 128, 8, 8); step(0xF1, 8, 110, 80, 8, 8); step(0xF1, 8, 110, 0, 8, 8);
        step(0xF1, 2, 110, 0, 8, 8);
        dset(0xF1,2,110,0); drain(1200);
        check("총평: 내리 두 판", sawEv(EV_ARCSWEEP, 0));
    }

    /* ── 흐름 라인은 라운드당 한 번까지 (도배 금지) ── */
    {
        int b = 128, n = 0;
        begin("once");
        idle(200); step(0xF1, 8, 128, 128, 8, 8);
        for(i = 0; i < 18; i++){ b -= 6; if(b < 10) b = 10; step(0xF1, 8, 128, b, 8, 8); }
        dset(0xF1,8,128,b); drain(1200);
        n = countEv(EV_FLOWONE, 0);
        check("흐름 라인은 라운드당 한 번", n == 1);
    }

    /* ── 온오프 조합: 캐릭터챗과 심판을 따로 끈다 ── */
    {
        extern void ss2comm_set_chat(int);
        extern void ss2comm_set_ref(int);
        extern const char *ss2comm_test_ref_take(void);
        const char *rl;
        int refSeen, bandSeen;

        /* 캐릭터챗 오프 — 심판 구령은 서고 밴드는 침묵 */
        begin("chatOff"); ss2comm_set_chat(0);
        idle(200); step(0xF1, 8, 128, 128, 8, 8);
        refSeen = 0;
        { int i2; for(i2=0;i2<200;i2++){ step(0xF1,8,128,120,8,8);
            rl = ss2comm_test_ref_take(); if(rl && strstr(rl,"승부")) refSeen=1; } }
        check("캐릭터챗 오프 — 밴드 침묵", nseen == 0);
        check("캐릭터챗 오프 — 심판은 선다", refSeen == 1);
        ss2comm_set_chat(1);

        /* 심판 오프 — 구령 없음, 밴드는 산다 */
        begin("refOff"); ss2comm_set_ref(0);
        idle(200); step(0xF1, 8, 128, 128, 8, 8);
        refSeen = 0;
        { int i2; for(i2=0;i2<300;i2++){ step(0xF1,8,128,120,8,8);
            rl = ss2comm_test_ref_take(); if(rl) refSeen=1; } }
        bandSeen = nseen;
        check("심판 오프 — 구령 없음", refSeen == 0);
        check("심판 오프 — 캐릭터챗은 산다", bandSeen > 0);
        ss2comm_set_ref(1);
    }

    /* ── 열다섯 명이 서로 다른 말을 한다 (총평 기준) ── */
    {
        char lines[16][160];
        int n = 0, dup = 0, s;
        for(s = 0; s < ss2comm_speaker_count(); s++){
            ss2comm_set_speaker(s);
            begin("voices");
            idle(200);
            step(0xF1, 8, 128, 128, 8, 8); step(0xF1, 8, 120, 90, 8, 8); step(0xF1, 8, 120, 0, 8, 8);
            idle(10);
            step(0xF1, 8, 128, 128, 8, 8); step(0xF1, 8, 110, 80, 8, 8); step(0xF1, 8, 110, 0, 8, 8);
            step(0xF1, 2, 110, 0, 8, 8);
            dset(0xF1,2,110,0); drain(1200);
            if(nseen) snprintf(lines[n++], 160, "%s", seen[nseen-1]);
        }
        for(i = 0; i < n; i++){
            int j; for(j = i+1; j < n; j++) if(!strcmp(lines[i], lines[j])) dup++;
        }
        printf("      (화자 %d명 중 총평 문장 수집 %d개, 겹침 %d)\n", ss2comm_speaker_count(), n, dup);
        check("열다섯 목소리가 서로 다르다", n >= 15 && dup <= 2);
        ss2comm_set_speaker(0);
    }


    printf("\n==== %d passed / %d failed ====\n", pass, fail);
    return fail ? 1 : 0;
}
