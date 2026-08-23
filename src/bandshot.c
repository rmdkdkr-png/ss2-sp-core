/* 띠를 **그림으로** 뽑는다. 이 프로젝트가 만드는 건 픽셀인데 여태 표만 보고 있었다.
 *
 *   cc -O1 -DSS2SP_RAM_POINTER -I. -o /tmp/bandshot bandshot.c ss2comm.c && /tmp/bandshot /tmp/shots
 *
 * 상황을 하나씩 만들어 놓고, 새 대사가 뜰 때마다 160x30 띠를 PPM 으로 저장한다.
 * 글자가 잘렸는지, 글꼴에 없어서 빈칸으로 나갔는지, 얼굴이 붙었는지 —
 * 표를 아무리 들여다봐도 안 보이는 것들이 여기서는 그냥 보인다.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

extern void        ss2comm_set_ram(void *p);
extern void        ss2comm_set_enabled(int on);
extern void        ss2comm_set_speaker(int idx);
extern void        ss2comm_reset(void);
extern const char *ss2comm_frame(void);
extern void        ss2comm_draw_enable(int mode);
extern void        ss2comm_draw(uint16_t *fb, int pitch_px, int w, int h);
extern const char *ss2comm_current(int *age);

const char *ss2sp_last_name = 0;
int         ss2sp_last_ok   = 0;

#define W 160
#define H 152
#define BH 30
static uint16_t fb[W*(H+BH)];
static uint8_t  ram[16384];

#define MODE 0x00A7
#define SCR  0x01C0
#define HP1  0x1A46
#define HP2  0x1C46
#define ACT1 0x0E3E
#define ACT2 0x0E7E
#define BLK1 0x1B51
#define BLK2 0x1D51
static void w16(int o,int v){ ram[o]=v&0xFF; ram[o+1]=(v>>8)&0xFF; }

static const char *outdir;
static int shot_n;
static char shot_label[4096][192];

/* 띠만 3배로 키워 PPM 으로. 11px 글자를 눈으로 읽으려면 확대가 필요하다. */
static void dump(const char *label){
    char path[512]; FILE *f; int x,y,sx,sy; const int S=3;
    snprintf(path,sizeof path,"%s/%03d.ppm",outdir,shot_n);
    f=fopen(path,"wb"); if(!f) return;
    fprintf(f,"P6\n%d %d\n255\n",W*S,BH*S);
    for(y=0;y<BH;y++) for(sy=0;sy<S;sy++){
        if(sy) { fseek(f,0,SEEK_CUR); }
        for(x=0;x<W;x++){
            uint16_t v=fb[y*W+x];
            uint8_t r=((v>>11)&31)*255/31, g=((v>>5)&63)*255/63, b=(v&31)*255/31;
            for(sx=0;sx<S;sx++){ fputc(r,f); fputc(g,f); fputc(b,f); }
        }
    }
    fclose(f);
    snprintf(shot_label[shot_n],192,"%s",label);
    shot_n++;
}

static char last[192];
static const char *phase="";
static int frame;

static void step(int mode,int scr,int hp1,int hp2,int a1,int a2){
    const char *l;
    ram[MODE]=mode; ram[SCR]=scr; ram[HP1]=hp1; ram[HP2]=hp2;
    w16(ACT1,a1); w16(ACT2,a2);
    l = ss2comm_frame();
    if(l && strcmp(l,last)){
        char lab[192];
        snprintf(last,sizeof last,"%s",l);
        /* 대사가 바뀐 **직후** 프레임에 그린다 — 타자 연출이 다 끝난 뒤를 보려고
           몇 프레임 더 굴린 뒤 찍는다 (show = 2 + age*2). */
        { int k; for(k=0;k<60;k++){ ram[MODE]=mode; ss2comm_frame(); } }
        memset(fb,0,sizeof fb);
        ss2comm_draw(fb,W,W,H);
        snprintf(lab,sizeof lab,"[%s] %s",phase,l);
        dump(lab);
    }
    frame++;
}
static void hold(int n,int mode,int scr,int hp1,int hp2){ while(n-->0) step(mode,scr,hp1,hp2,8,8); }
static void hit(int scr,int *hp1,int *hp2,int who,int dmg){
    if(who==2){ *hp2-=dmg; if(*hp2<0)*hp2=0; } else { *hp1-=dmg; if(*hp1<0)*hp1=0; }
    step(0xF1,scr,*hp1,*hp2, who==1?0x200:8, who==2?0x200:8);
    step(0xF1,scr,*hp1,*hp2,8,8);
}

/* 한 판 — 화자와 두 캐릭터를 바꿔 가며 */
static void match(int spk,int me,int opp,const char *tag){
    int hp1=128,hp2=128,i,r;
    phase=tag;
    ss2comm_reset(); memset(ram,0,sizeof ram); last[0]=0;
    ss2comm_set_speaker(spk);
    ram[BLK1]=16*me; ram[BLK2]=16*opp;
    hold(150,0xF0,2,128,128);       /* 캐릭터 고르기 */
    hold(120,0xF0,6,128,128);       /* VS */
    for(r=0;r<2;r++){
        hp1=hp2=128;
        hold(90,0xF1,8,hp1,hp2);
        for(i=0;i<8;i++){ hit(8,&hp1,&hp2,(i%3==0)?1:2,16); hold(70,0xF1,8,hp1,hp2); if(hp2<=0)break; }
        hp2=0; hold(200,0xF1,8,hp1,hp2);
    }
    hold(30,0xF1,8,0,0); hold(240,0xF1,0,0,0);   /* 결과 */
    hold(400,0xF0,2,128,128);                     /* 메뉴 — 빈 시간 */
}

int main(int argc,char**argv){
    int i;
    outdir = argc>1?argv[1]:"/tmp/shots";
    ss2comm_set_ram(ram); ss2comm_set_enabled(1); 
    ss2comm_draw_enable(4);              /* 4 = 화면 위 띠 (앱과 같은 모드) */
    ss2comm_reset();

    match(14, 2, 3,  "유가해설 하오마루vs겐주로");   /* 유가가 해설 */
    match(14, 2, 2,  "유가해설 미러전");
    match(14,13,14,  "유가해설 시키vs유가");
    match( 0, 2, 3,  "하오마루해설");
    match( 5, 3, 2,  "겐주로해설");
    match(10,10, 4,  "모로즈미해설");

    printf("띠 %d장 → %s\n\n", shot_n, outdir);
    for(i=0;i<shot_n;i++) printf("%03d  %s\n", i, shot_label[i]);
    return 0;
}
