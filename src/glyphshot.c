/* 문장을 그대로 띠에 올려 그림으로 뽑는다. 글꼴 빠진 글자를 눈으로 잡으려고. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
extern void ss2comm_set_ram(void*); extern void ss2comm_set_enabled(int);
extern void ss2comm_reset(void); extern const char *ss2comm_frame(void);
extern void ss2comm_draw_enable(int); extern void ss2comm_draw(uint16_t*,int,int,int);
extern void ss2comm_test_say(const char*,int);
const char *ss2sp_last_name=0; int ss2sp_last_ok=0;
#define W 160
#define H 152
#define BH 30
static uint16_t fb[W*(H+BH)]; static uint8_t ram[16384];
static void dump(const char*dir,int n){
    char p[512]; FILE*f; int x,y,sx,sy; const int S=3;
    snprintf(p,sizeof p,"%s/%03d.ppm",dir,n); f=fopen(p,"wb"); if(!f)return;
    fprintf(f,"P6\n%d %d\n255\n",W*S,BH*S);
    for(y=0;y<BH;y++)for(sy=0;sy<S;sy++)for(x=0;x<W;x++){
        uint16_t v=fb[y*W+x]; uint8_t r=((v>>11)&31)*255/31,g=((v>>5)&63)*255/63,b=(v&31)*255/31;
        for(sx=0;sx<S;sx++){fputc(r,f);fputc(g,f);fputc(b,f);} }
    fclose(f);
}
int main(int argc,char**argv){
    static const char*L[]={
      "셋째 판 — 정정당당히, 승부!",
      "벼랑에서 돌아왔구나. 나도 그랬다",
      "간다라는 내가 지었다. 시체 수천을 꿰매서",
      "암흑신이 설 터를 짓는 중이다",
      "흠 없는 그릇이군. 탐난다",
      "그릇은 남녀 한 쌍이라야 한다",
      "천천히 골라라. 나는 이십 년을 기다렸다",
      "쓴 술이 당기는 밤이군",
      0};
    int i,k; const char*dir=argc>1?argv[1]:"/tmp/gshots";
    ss2comm_set_ram(ram); ss2comm_set_enabled(1); ss2comm_draw_enable(4); ss2comm_reset();
    for(i=0;L[i];i++){
        ram[0x00A7]=0xF0; for(k=0;k<80;k++) ss2comm_frame();
        ss2comm_test_say(L[i],-2);              /* -2 = 심판칸(얼굴 없음) */
        for(k=0;k<60;k++) ss2comm_frame();
        memset(fb,0,sizeof fb); ss2comm_draw(fb,W,W,H); dump(dir,i);
        printf("%03d  %s\n",i,L[i]);
    }
    return 0;
}
