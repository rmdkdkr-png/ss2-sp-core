/* 시뮬레이터 — 램을 흉내 내 **모든 상황**을 한 번에 돌리고, 무엇이 나오고
 * 무엇이 안 나오는지 표로 찍는다. 띠 그림도 같이 뽑는다.
 *
 *   cc -O1 -DSS2SP_RAM_POINTER -DSS2COMM_TEST -I. -o /tmp/simu simu.c ss2comm.c
 *   /tmp/simu /tmp/sim > /tmp/sim/log.tsv
 *
 * 램 주소는 옆방 scen.py 가 실기에서 확인한 것을 그대로 쓴다:
 *   0x0E7E/0x0E7F  P2 동작   (다운 = 0x013C, 필살기 = 0x0210)
 *   0x0E3E/0x0E3F  P1 동작
 *   0x0E54         바라보는 쪽
 * 출력은 탭 구분 — 시나리오 / 프레임 / 대사. 이벤트 이름 붙이기는 simu_label.py 가 한다.
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
extern const char *ss2comm_test_ref_take(void);   /* 아래 칸에 선 심판 구호 */

const char *ss2sp_last_name = 0;
int         ss2sp_last_ok   = 0;
/* 기술명은 ss2sp 엔진이 채워 주는 값이다. 시뮬레이터에서는 직접 세워야
   MOVEHIT/MOVEKO/MOVE 계열이 돈다 — 안 그러면 「안 나온다」로 잘못 보인다. */
static void mv(const char *n){ ss2sp_last_name = n; ss2sp_last_ok = 1; }

#define W 160
#define H 152
#define BH 32
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
#define SURV 0x180B
#define STG  0x17FE
static void w16(int o,int v){ ram[o]=v&0xFF; ram[o+1]=(v>>8)&0xFF; }

static const char *dir_out;
static const char *scen = "";
static int   shots, fno;
static char  last[192];

static void snap(const char *text){
    char p[512]; FILE *f; int x,y,sx,sy; const int S=3;
    snprintf(p,sizeof p,"%s/%04d.ppm",dir_out,shots);
    f=fopen(p,"wb"); if(f){
        fprintf(f,"P6\n%d %d\n255\n",W*S,BH*S);
        for(y=0;y<BH;y++)for(sy=0;sy<S;sy++)for(x=0;x<W;x++){
            uint16_t v=fb[y*W+x];
            uint8_t r=((v>>11)&31)*255/31,g=((v>>5)&63)*255/63,b=(v&31)*255/31;
            for(sx=0;sx<S;sx++){fputc(r,f);fputc(g,f);fputc(b,f);} }
        fclose(f);
    }
    printf("%04d\t%s\t%d\t%s\n", shots, scen, fno, text);
    shots++;
}
static void tick(void){
    const char *l = ss2comm_frame();
    const char *r;
    fno++;
    /* 심판은 화면 아래 제 칸에 선다 — 해설 흐름에 안 섞이므로 따로 걷는다 */
    r = ss2comm_test_ref_take();
    if(r){ char t[192]; snprintf(t,sizeof t,"%s",r); memset(fb,0,sizeof fb);
           ss2comm_draw(fb,W,W,H); snap(t); }
    if(l && strcmp(l,last)){
        int k;
        snprintf(last,sizeof last,"%s",l);
        for(k=0;k<50;k++) ss2comm_frame();      /* 타자 연출이 끝난 뒤를 찍는다 */
        memset(fb,0,sizeof fb); ss2comm_draw(fb,W,W,H);
        snap(l);
    }
}
static void set(int mode,int scr,int hp1,int hp2){ ram[MODE]=mode; ram[SCR]=scr; ram[HP1]=hp1; ram[HP2]=hp2; }
static void run(int n){ while(n-- > 0) tick(); }
static void hold(int n,int mode,int scr,int hp1,int hp2){ set(mode,scr,hp1,hp2); w16(ACT1,8); w16(ACT2,8); run(n); }
static void act1(int v,int n){ w16(ACT1,v); run(n); w16(ACT1,8); }
/* hold() 는 동작값을 8 로 되돌린다. 다운처럼 **값을 유지한 채** 시간을 보내야 하는
   상황에는 이걸 쓴다 — 안 그러면 다운이 한 프레임 만에 지워져 「안 나온다」로 보인다. */
static void hold_a(int n,int mode,int scr,int hp1,int hp2,int va1,int va2){
  set(mode,scr,hp1,hp2); w16(ACT1,va1); w16(ACT2,va2);
  while(n-- > 0){ w16(ACT1,va1); w16(ACT2,va2); tick(); }
}
static void act2(int v,int n){ w16(ACT2,v); run(n); w16(ACT2,8); }
static void dmg2(int *h2,int d){ *h2 -= d; if(*h2<0) *h2=0; ram[HP2]=*h2; act1(0x200,1); run(1); }
static void dmg1(int *h1,int d){ *h1 -= d; if(*h1<0) *h1=0; ram[HP1]=*h1; act2(0x200,1); run(1); }

static void begin(const char *tag,int spk,int me,int opp){
    scen = tag; last[0]=0; fno=0;
    ss2comm_reset(); memset(ram,0,sizeof ram);
    ss2comm_set_speaker(spk);
    ram[BLK1]=16*me; ram[BLK2]=16*opp; ram[0x17DF]=(unsigned char)(opp<0?19:opp);
}
/* 전투에 들어간 상태 만들기 */
static void enter(void){ hold(60,0xF0,2,128,128); hold(90,0xF1,8,128,128); }

int main(int argc,char**argv){
    int h1,h2,i,s;
    dir_out = argc>1?argv[1]:"/tmp/sim";
    ss2comm_set_ram(ram); ss2comm_set_enabled(1); 
    ss2comm_draw_enable(4); ss2comm_reset();
    printf("no\t시나리오\t프레임\t대사\n");

    /* ── 메뉴 계열: 화면 번호를 **전부 훑는다**. 어느 번호가 무슨 화면인지
          실기 값을 몰라서 추측했던 자리다 — 훑으면 무엇이 반응하는지 다 보인다. */
    for(s=0;s<16;s++){
        char tag[32]; snprintf(tag,sizeof tag,"메뉴 scr=%d",s);
        begin(tag,0,2,3);
        hold(120,0xF0,1,128,128);
        ram[0x2F82]=0x20;                   /* OFF_PAD — 버튼을 눌러 넘어온 셈 (byClick) */
        run(2); ram[0x2F82]=0;
        hold(600,0xF0,s,128,128);
    }
    /* 입력 없이 저절로 넘어가는 화면 — 스토리 연출 쪽 */
    for(s=0;s<16;s++){
        char tag[32]; snprintf(tag,sizeof tag,"무입력 scr=%d",s);
        begin(tag,0,2,3);
        hold(120,0xF0,1,128,128);
        hold(400,0xF0,s,128,128);
    }
    /* ── 전투 계열 ── */
    begin("라운드 시작",0,2,3); enter(); hold(400,0xF1,8,128,128);
    begin("주고받기",0,2,3); enter(); h1=h2=128;
      for(i=0;i<8;i++){ if(i&1) dmg1(&h1,10); else dmg2(&h2,10); hold(45,0xF1,8,h1,h2); }
    begin("일방적",0,2,3); enter(); h1=128;h2=128;
      for(i=0;i<8;i++){ dmg2(&h2,10); hold(45,0xF1,8,h1,h2); }
    begin("계속 쫓김",0,2,3); enter(); h1=128;h2=128;
      for(i=0;i<8;i++){ dmg1(&h1,10); hold(45,0xF1,8,h1,h2); }
    begin("상대 필살기 남발",0,2,3); enter(); h1=h2=128;
      for(i=0;i<5;i++){ act2(0x210,2); hold(80,0xF1,8,h1,h2); }
    begin("내 필살기 적중",0,2,3); enter(); h1=h2=128;
      mv("츠바메가에시"); act1(0x210,2); hold(10,0xF1,8,h1,h2); dmg2(&h2,18); hold(200,0xF1,8,h1,h2);
    begin("기술로 KO",0,2,3); enter(); h1=h2=128;
      hold(60,0xF1,8,h1,h2); mv("츠바메가에시"); act1(0x210,2); hold(8,0xF1,8,h1,h2);
      h2=0; ram[HP2]=0; hold(400,0xF1,8,h1,h2);
    begin("비오의",0,2,3); enter(); h1=h2=128;
      mv("비오의"); act1(0x210,2); hold(200,0xF1,8,h1,h2);
    begin("같은 기술 세 번",0,2,3); enter(); h1=h2=128;
      for(i=0;i<3;i++){ mv("앵화참"); act1(0x210,2); hold(8,0xF1,8,h1,h2); dmg2(&h2,14); hold(90,0xF1,8,h1,h2); }
    /* 문구(VS) 화면 — 대진 소개가 여기서 나와야 한다 */
    begin("문구 화면",0,2,3); begin("문구 화면",0,2,3);
      hold(60,0xF0,2,128,128); hold(400,0xF1,0,128,128); hold(200,0xF1,8,128,128);
    begin("상대 눕힘",0,2,3); enter(); h1=h2=128;
      act1(0x210,2); hold(8,0xF1,8,h1,h2); dmg2(&h2,18); w16(ACT2,0x013C); hold(200,0xF1,8,h1,h2);
    begin("내가 눕음",0,2,3); enter(); h1=h2=128;
      dmg1(&h1,18); w16(ACT1,0x013C); hold(200,0xF1,8,h1,h2);
    begin("상대 위험",0,2,3); enter(); hold(200,0xF1,8,128,12);
    begin("둘 다 위험",0,2,3); enter(); hold(150,0xF1,8,128,12); hold(200,0xF1,8,10,12);
    begin("KO 승",0,2,3); enter(); hold(60,0xF1,8,128,128); hold(400,0xF1,8,128,0);
    begin("KO 패",0,2,3); enter(); hold(60,0xF1,8,128,128); hold(400,0xF1,8,0,128);
    begin("더블 KO",0,2,3); enter(); hold(60,0xF1,8,128,128); hold(400,0xF1,8,0,0);
    begin("퍼펙트",0,2,3); enter(); hold(150,0xF1,8,128,128); hold(400,0xF1,8,128,0);
    begin("역전",0,2,3); enter(); hold(200,0xF1,8,7,128); hold(400,0xF1,8,7,0);
    begin("초살",0,2,3); enter(); hold(400,0xF1,8,128,0);
    begin("긴 싸움",0,2,3); enter(); hold(2000,0xF1,8,70,70);
    begin("방치",0,2,3); enter(); hold(1800,0xF1,8,128,128);
    /* 매치 전체 — 결과 화면까지 */
    begin("한 판 전체",0,2,3); enter();
      for(s=0;s<2;s++){ h1=h2=128;
        if(s) hold(90,0xF1,8,128,128);
        for(i=0;i<6;i++){ dmg2(&h2,22); hold(60,0xF1,8,h1,h2); if(h2<=0) break; }
        h2=0; hold(200,0xF1,8,h1,h2); }
      hold(30,0xF1,8,0,0); hold(400,0xF1,0,0,0);
    /* 서바이벌·스토리 */
    begin("서바이벌 연승",0,2,3); ram[SURV]=12; enter(); hold(200,0xF1,8,128,128);
    begin("스토리 8연전",0,2,3); ram[STG]=7; enter(); hold(200,0xF1,8,128,128);
    /* 미러전 · 표 밖 캐릭터(간다라) */
    begin("미러전",0,2,2); enter(); hold(300,0xF1,8,128,128);
    begin("간다라(표 밖)",0,2,15); enter(); hold(300,0xF1,8,128,128);
    /* 심판 — 세 판 */
    /* 세 판 — 판마다 **가득 찬 체력에서 시작**해야 한다. 처음부터 0 을 물리면
       전투 진입 갈래(hp1>0 && hp2>0)가 아예 안 타서 라운드 구호도 안 나온다. */
    begin("세 판 승부",0,2,3); enter();
      hold(120,0xF1,8,128,128); hold(300,0xF1,8,128,0);    /* 1판 내가 이김 */
      hold(60,0xF0,1,128,128);
      hold(120,0xF1,8,128,128); hold(300,0xF1,8,0,128);    /* 2판 내가 짐 */
      hold(60,0xF0,1,128,128);
      hold(120,0xF1,8,128,128); hold(300,0xF1,8,128,0);    /* 3판 */
      hold(30,0xF1,8,0,0); hold(300,0xF1,0,0,0);
    /* ── 아직 한 번도 안 나온다고 찍힌 것들을 겨냥한 시나리오 ── */
    begin("다운 주고받기",0,2,3); enter(); h1=h2=128;
      hold(120,0xF1,8,h1,h2);
      dmg2(&h2,14); hold_a(150,0xF1,8,h1,h2,8,0x013C); hold(120,0xF1,8,h1,h2);
      dmg1(&h1,14); hold_a(150,0xF1,8,h1,h2,0x013C,8); hold(120,0xF1,8,h1,h2);
    begin("기술로 눕힘",0,2,3); enter(); h1=h2=128;
      mv("앵화참"); act1(0x210,2); hold(6,0xF1,8,h1,h2);
      hold_a(200,0xF1,8,h1,h2,8,0x013C);
    /* 맞은 직후의 다운은 MOVEHIT 이 이미 말했으므로 침묵한다(st_hitAt +42프레임).
       **한참 뒤에 넘어지는** 경우라야 EV_DOWN 이 나온다 — 그 자리를 따로 만든다. */
    begin("한참 뒤 눕힘",0,2,3); enter(); h1=h2=128;
      dmg2(&h2,14); hold(180,0xF1,8,h1,h2); hold_a(150,0xF1,8,h1,h2,8,0x013C);
    begin("한참 뒤 내가 눕음",0,2,3); enter(); h1=h2=128;
      dmg1(&h1,14); hold(180,0xF1,8,h1,h2); hold_a(150,0xF1,8,h1,h2,0x013C,8);
    begin("이름 없이 눕힘",0,2,3); enter(); h1=h2=128;
      hold(120,0xF1,8,h1,h2); dmg2(&h2,16); hold(60,0xF1,8,h1,h2);
      hold_a(200,0xF1,8,h1,h2,8,0x013C);
    begin("약타 적중",0,2,3); enter(); h1=h2=128;
      mv("광양인"); act1(0x210,2); hold(6,0xF1,8,h1,h2); dmg2(&h2,4); hold(200,0xF1,8,h1,h2);
    begin("역전 패",0,2,3); enter(); hold(200,0xF1,8,128,7); hold(400,0xF1,8,0,7);
    begin("두 판 내리 짐",0,2,3); enter();
      hold(120,0xF1,8,128,128); hold(300,0xF1,8,0,128); hold(60,0xF0,1,128,128);
      hold(120,0xF1,8,128,128); hold(300,0xF1,8,0,128);
      hold(30,0xF1,8,0,0); hold(300,0xF1,0,0,0);
    begin("먼저 따고 뒤집힘",0,2,3); enter();
      hold(120,0xF1,8,128,128); hold(300,0xF1,8,128,0); hold(60,0xF0,1,128,128);
      hold(120,0xF1,8,128,128); hold(300,0xF1,8,0,128); hold(60,0xF0,1,128,128);
      hold(120,0xF1,8,128,128); hold(300,0xF1,8,0,128);
      hold(30,0xF1,8,0,0); hold(300,0xF1,0,0,0);
    begin("밀리다 뒤집음",0,2,3); enter();
      hold(120,0xF1,8,128,128); hold(300,0xF1,8,0,128); hold(60,0xF0,1,128,128);
      hold(120,0xF1,8,128,128); hold(300,0xF1,8,128,0); hold(60,0xF0,1,128,128);
      hold(120,0xF1,8,128,128); hold(300,0xF1,8,128,0);
      hold(30,0xF1,8,0,0); hold(300,0xF1,0,0,0);
    begin("서바이벌 끝",0,2,3); ram[SURV]=9; enter(); hold(120,0xF1,8,128,128);
      ram[SURV]=0; hold(300,0xF1,8,0,128); hold(30,0xF1,8,0,0); hold(300,0xF1,0,0,0);
    begin("엔딩",0,2,3); begin("엔딩",0,2,3); hold(120,0xF0,1,128,128); hold(400,0xC7,1,128,128);
    begin("긴 판",0,2,3); enter(); hold(3600,0xF1,8,64,64);
    /* 화자를 바꿔 가며 한 판씩 — 목소리가 다른지 */
    for(s=0;s<15;s++){
        char tag[32]; snprintf(tag,sizeof tag,"화자%02d",s);
        begin(tag,s,2,3); enter();
        hold(200,0xF1,8,128,40); hold(300,0xF1,8,128,0);
    }
    fprintf(stderr,"띠 %d장 → %s\n", shots, dir_out);
    return 0;
}
