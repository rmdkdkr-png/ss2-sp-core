/* 링크 대전 하네스 — 코어 **두 벌**을 한 프로세스에서 번갈아 한 프레임씩 돌린다.

   왜 한 프로세스인가: 프로세스를 둘로 띄우면 각자 최대 속도로 달려서 보조가 안 맞는다.
   실제로 그렇게 해 보니 「2 인 대전」을 고르는 순간 악수에 실패해 메뉴로 튕겨 나왔다.
   링크는 두 기기가 같은 박자로 돌아야 성립한다 — 번갈아 돌리면 그 박자가 보장된다.

   코어 두 벌은 **파일을 두 벌 복사해서** dlopen 한다. 같은 경로를 두 번 열면
   같은 인스턴스가 돌아와 정적 상태를 공유한다(램도 링크 상태도 하나가 된다).

   바이트 전달은 이름있는 파이프 두 개로 엇갈리게 잇는다. 각 코어는 retro_init 때
   자기 파이프를 잡는다 — 그래서 dlopen·init 사이에 환경변수를 바꿔 준다.

   쓰기: linkrun <core.so> <rom> <script>
   script 한 줄:  <프레임수> <1P버튼|-> <2P버튼|->      버튼 여러 개는 +로 잇는다
                  !a <tag> / !b <tag>   그 시점 화면을 저장
   예)  120 - -
        6 B -
        30 - -
        6 - B
        !a menu */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

struct av{struct{unsigned bw,bh,mw,mh;float ar;}g;struct{double fps,sr;}t;};
struct gi{const char*p;const void*d;size_t s;const char*m;};

enum{ID_B=0,ID_Y=1,ID_SE=2,ID_ST=3,ID_U=4,ID_D=5,ID_L=6,ID_R=7,ID_A=8,ID_X=9,ID_L1=10,ID_R1=11};
static int btn1(const char*s){
  if(!strcmp(s,"U"))return ID_U;   if(!strcmp(s,"D"))return ID_D;
  if(!strcmp(s,"L"))return ID_L;   if(!strcmp(s,"R"))return ID_R;
  if(!strcmp(s,"A"))return ID_A;   if(!strcmp(s,"B"))return ID_B;
  if(!strcmp(s,"X"))return ID_X;   if(!strcmp(s,"Y"))return ID_Y;
  if(!strcmp(s,"ST"))return ID_ST; if(!strcmp(s,"SE"))return ID_SE;
  if(!strcmp(s,"L1"))return ID_L1; if(!strcmp(s,"R1"))return ID_R1;
  return -1;
}
/* "B+D" 같은 묶음을 비트로 */
static int16_t btns(const char*s){
  char tmp[64], *p, *q; int16_t m = 0;
  if(!s || !strcmp(s,"-")) return 0;
  snprintf(tmp,sizeof tmp,"%s",s);
  for(p = tmp; (q = strsep(&p, "+")); ){ int b = btn1(q); if(b>=0) m |= (int16_t)(1<<b); }
  return m;
}

#define NC 2
static int cur;                          /* 지금 도는 코어 — 콜백이 누구 것인지 가른다 */
static unsigned W[NC], H[NC];
static uint16_t FB[NC][512*512];
static int16_t  PAD[NC];

static void vcb(const void*d,unsigned w,unsigned h,size_t p){
  unsigned y; W[cur]=w; H[cur]=h;
  for(y=0;y<h&&y<512;y++) memcpy(FB[cur]+y*512,(const char*)d+y*p,(w<512?w:512)*2);
}
static void acb(short a,short b){(void)a;(void)b;}
static size_t abcb(const short*d,size_t f){(void)d;return f;}
static void ipoll(void){}
static short inp(unsigned port,unsigned dev,unsigned idx,unsigned id){
  (void)port;(void)dev;(void)idx;
  return (PAD[cur]>>id)&1;
}
static int envcb(unsigned cmd,void*data){
  if(cmd==3){*(int*)data=1;return 1;}
  if(cmd==9||cmd==31){*(const char**)data=".";return 1;}
  if(cmd==52||cmd==53||cmd==67) return 1;
  if(cmd==15){ struct{const char*k;const char*v;}*v=data;
    const char *ov=getenv(v->k);
    if(ov){ v->v=ov; return 1; }
    v->v=NULL; return 0; }
  return 0;
}
static void shot(int c,const char*tag){
  char p[256]; FILE*f; unsigned x,y;
  snprintf(p,sizeof p,"svc_%s.ppm",tag);
  f=fopen(p,"wb"); if(!f) return;
  fprintf(f,"P6\n%u %u\n255\n",W[c],H[c]);
  for(y=0;y<H[c];y++)for(x=0;x<W[c];x++){uint16_t v=FB[c][y*512+x];
    fputc(((v>>11)&31)*255/31,f);fputc(((v>>5)&63)*255/63,f);fputc((v&31)*255/31,f);}
  fclose(f);
  printf("  [%s] %ux%u\n",tag,W[c],H[c]);
}

typedef void (*fv)(void);
struct core{ void*h; fv init, run; bool(*load)(const struct gi*); void*(*getmem)(unsigned); };

int main(int argc,char**argv){
  struct core C[NC];
  const char *fifo[NC][2] = {{"/tmp/ngp_b2a","/tmp/ngp_a2b"},   /* [0]=1P: in,out */
                             {"/tmp/ngp_a2b","/tmp/ngp_b2a"}};  /* [1]=2P */
  char copy[NC][256];
  int i; long frame=0;
  void *rom; long rn;

  if(argc<4){ printf("쓰기: linkrun <core.so> <rom> <script>\n"); return 1; }
  unlink("/tmp/ngp_a2b"); unlink("/tmp/ngp_b2a");
  if(mkfifo("/tmp/ngp_a2b",0600) || mkfifo("/tmp/ngp_b2a",0600)){ perror("mkfifo"); return 1; }

  { FILE*rf=fopen(argv[2],"rb"); if(!rf){printf("rom 없음\n");return 1;}
    fseek(rf,0,SEEK_END); rn=ftell(rf); fseek(rf,0,SEEK_SET);
    rom=malloc(rn); if(fread(rom,1,rn,rf)!=(size_t)rn){printf("rom 읽기\n");return 1;} fclose(rf); }

  for(i=0;i<NC;i++){
    char cmd[600];
    snprintf(copy[i],sizeof copy[i],"/tmp/linkcore_%d.so",i);
    snprintf(cmd,sizeof cmd,"cp -f '%s' '%s'",argv[1],copy[i]);
    if(system(cmd)){ printf("코어 복사 실패\n"); return 1; }
    /* 같은 경로를 두 번 dlopen 하면 **같은 인스턴스**가 온다 — 파일을 갈라야 정적 상태가 갈린다 */
    C[i].h = dlopen(copy[i], RTLD_NOW | RTLD_LOCAL);
    if(!C[i].h){ printf("dlopen: %s\n", dlerror()); return 1; }
    #define SYM(t,n) ((t)dlsym(C[i].h,n))
    ((void(*)(int(*)(unsigned,void*)))SYM(void*,"retro_set_environment"))(envcb);
    ((void(*)(void(*)(const void*,unsigned,unsigned,size_t)))SYM(void*,"retro_set_video_refresh"))(vcb);
    ((void(*)(void(*)(short,short)))SYM(void*,"retro_set_audio_sample"))(acb);
    ((void(*)(size_t(*)(const short*,size_t)))SYM(void*,"retro_set_audio_sample_batch"))(abcb);
    ((void(*)(void(*)(void)))SYM(void*,"retro_set_input_poll"))(ipoll);
    ((void(*)(short(*)(unsigned,unsigned,unsigned,unsigned)))SYM(void*,"retro_set_input_state"))(inp);
    C[i].init  = SYM(fv,"retro_init");
    C[i].run   = SYM(fv,"retro_run");
    C[i].load  = (bool(*)(const struct gi*))dlsym(C[i].h,"retro_load_game");
    C[i].getmem= (void*(*)(unsigned))dlsym(C[i].h,"retro_get_memory_data");
    /* 이 인스턴스가 쥘 파이프를 지금 정한다 — init 안에서 잡는다 */
    setenv("NGP_LINK_IN",  fifo[i][0], 1);
    setenv("NGP_LINK_OUT", fifo[i][1], 1);
    cur = i;
    C[i].init();
    { struct gi g={argv[2],rom,(size_t)rn,NULL};
      if(!C[i].load(&g)){ printf("%d번 코어 LOAD 실패\n",i); return 1; } }
    printf("  %d번 코어 준비 (in=%s out=%s)\n", i+1, fifo[i][0], fifo[i][1]);
  }

  { FILE*sf=fopen(argv[3],"r"); char line[256];
    if(!sf){ printf("script 없음\n"); return 1; }
    while(fgets(line,sizeof line,sf)){
      char a[64]="-", b[64]="-", tag[64]; long n=0;
      if(line[0]=='!'){
        char c[16];
        if(sscanf(line+1,"%15s %63s",c,tag)==2){
          if(!strcmp(c,"a")) shot(0,tag);
          else if(!strcmp(c,"b")) shot(1,tag);
        }
        continue;
      }
      if(sscanf(line,"%ld %63s %63s",&n,a,b)<1) continue;
      PAD[0]=btns(a); PAD[1]=btns(b);
      while(n-- > 0){
        cur=0; C[0].run();
        cur=1; C[1].run();
        frame++;
      }
    }
    fclose(sf);
  }
  printf("  총 %ld 프레임\n",frame);
  return 0;
}
