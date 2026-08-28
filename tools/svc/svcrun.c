/* SvC 정찰 하네스 — 입력 주입 + 램 덤프 + 화면 덤프
   사용: svcrun <core.so> <rom> <script>
   script 한 줄: <프레임수> <버튼들|->   예)  120 -   / 4 START / 60 -
   버튼: U D L R A B X Y ST SE L1 R1
   환경변수: DUMPAT="300,600,900"  각 프레임에서 ram/ppm 저장 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <dlfcn.h>

struct si{const char*n,*v,*e;char b,c;};
struct av{struct{unsigned bw,bh,mw,mh;float ar;}g;struct{double fps,sr;}t;};
struct gi{const char*p;const void*d;size_t s;const char*m;};

static unsigned W,H; static uint16_t FB[512*512];
static int16_t PAD=0;               /* 현재 프레임에 눌린 버튼 비트 */
static void vcb(const void*d,unsigned w,unsigned h,size_t p){
  unsigned y; W=w;H=h;
  for(y=0;y<h&&y<512;y++) memcpy(FB+y*512,(const char*)d+y*p,(w<512?w:512)*2);
}
static void acb(short a,short b){(void)a;(void)b;}
static size_t abcb(const short*d,size_t f){(void)d;return f;}
static FILE *aud_f;
static size_t abcb2(const short*d,size_t f){
  if(!aud_f) aud_f=fopen(getenv("AUDIO_RAW"),"wb");
  if(aud_f) fwrite(d,4,f,aud_f);
  return f;
}
static void poll(void){}
static short inp(unsigned port,unsigned dev,unsigned idx,unsigned id){
  (void)port;(void)dev;(void)idx;
  return (PAD>>id)&1;
}
static int envcb(unsigned cmd,void*data){
  if(cmd==3){*(int*)data=1;return 1;}
  if(cmd==9||cmd==31){*(const char**)data=".";return 1;}
  if(cmd==52||cmd==53||cmd==67) return 1;
  if(cmd==15){struct{const char*k;const char*v;}*v=data; v->v=NULL; return 0;}
  return 0;
}
static void dump(const char*tag,const uint8_t*ram,size_t rlen){
  char p[256]; FILE*f; unsigned x,y;
  snprintf(p,sizeof p,"svc_%s.ram",tag);
  f=fopen(p,"wb"); fwrite(ram,1,rlen,f); fclose(f);
  snprintf(p,sizeof p,"svc_%s.ppm",tag);
  f=fopen(p,"wb"); fprintf(f,"P6\n%u %u\n255\n",W,H);
  for(y=0;y<H;y++)for(x=0;x<W;x++){uint16_t v=FB[y*512+x];
    fputc(((v>>11)&31)*255/31,f);fputc(((v>>5)&63)*255/63,f);fputc((v&31)*255/31,f);}
  fclose(f);
}
/* libretro joypad id */
enum{ID_B=0,ID_Y=1,ID_SE=2,ID_ST=3,ID_U=4,ID_D=5,ID_L=6,ID_R=7,ID_A=8,ID_X=9,ID_L1=10,ID_R1=11};
static int btn(const char*s){
  if(!strcmp(s,"U"))return ID_U;   if(!strcmp(s,"D"))return ID_D;
  if(!strcmp(s,"L"))return ID_L;   if(!strcmp(s,"R"))return ID_R;
  if(!strcmp(s,"A"))return ID_A;   if(!strcmp(s,"B"))return ID_B;
  if(!strcmp(s,"X"))return ID_X;   if(!strcmp(s,"Y"))return ID_Y;
  if(!strcmp(s,"ST"))return ID_ST; if(!strcmp(s,"SE"))return ID_SE;
  if(!strcmp(s,"L1"))return ID_L1; if(!strcmp(s,"R1"))return ID_R1;
  return -1;
}
int main(int argc,char**argv){
  void*h=dlopen(argv[1],RTLD_NOW); if(!h){printf("dlopen: %s\n",dlerror());return 1;}
  typedef void (*fv)(void);
  typedef void (*fenv)(int(*)(unsigned,void*));
  typedef void (*fvid)(void(*)(const void*,unsigned,unsigned,size_t));
  typedef void (*fas)(void(*)(short,short));
  typedef void (*fab)(size_t(*)(const short*,size_t));
  typedef void (*fip)(void(*)(void));
  typedef void (*fis)(short(*)(unsigned,unsigned,unsigned,unsigned));
  typedef bool (*fload)(const struct gi*);
  typedef void (*fav)(struct av*);
  fenv  retro_set_environment    = (fenv) dlsym(h,"retro_set_environment");
  fvid  retro_set_video_refresh  = (fvid) dlsym(h,"retro_set_video_refresh");
  fas   retro_set_audio_sample   = (fas)  dlsym(h,"retro_set_audio_sample");
  fab   retro_set_audio_sample_batch=(fab)dlsym(h,"retro_set_audio_sample_batch");
  fip   retro_set_input_poll     = (fip)  dlsym(h,"retro_set_input_poll");
  fis   sis                      = (fis)  dlsym(h,"retro_set_input_state");
  fv    retro_init               = (fv)   dlsym(h,"retro_init");
  fload retro_load_game          = (fload)dlsym(h,"retro_load_game");
  fav   retro_get_system_av_info = (fav)  dlsym(h,"retro_get_system_av_info");
  fv    retro_run                = (fv)   dlsym(h,"retro_run");
  size_t(*ssize)(void)=(size_t(*)(void))dlsym(h,"retro_serialize_size");
  bool(*sser)(void*,size_t)=(bool(*)(void*,size_t))dlsym(h,"retro_serialize");
  bool(*sunser)(const void*,size_t)=(bool(*)(const void*,size_t))dlsym(h,"retro_unserialize");
  void*(*getmem)(unsigned) = (void*(*)(unsigned))dlsym(h,"retro_get_memory_data");
  size_t(*getsz)(unsigned) = (size_t(*)(unsigned))dlsym(h,"retro_get_memory_size");

  retro_set_environment(envcb); retro_set_video_refresh(vcb);
  retro_set_audio_sample(acb); retro_set_audio_sample_batch(getenv("AUDIO_RAW")?abcb2:abcb);
  retro_set_input_poll(poll); sis(inp); retro_init();

  FILE*rf=fopen(argv[2],"rb"); fseek(rf,0,SEEK_END); long n=ftell(rf); fseek(rf,0,SEEK_SET);
  void*rom=malloc(n); if(fread(rom,1,n,rf)!=(size_t)n){printf("rom read\n");return 1;} fclose(rf);
  struct gi g={argv[2],rom,(size_t)n,NULL};
  if(!retro_load_game(&g)){printf("LOAD FAIL\n");return 1;}
  struct av av; retro_get_system_av_info(&av);
  uint8_t*ram=getmem(2); size_t rlen=getsz(2);   /* RETRO_MEMORY_SYSTEM_RAM = 2 */
  printf("SYSTEM_RAM %p  %zu bytes\n",(void*)ram,rlen);

  FILE*sf=fopen(argv[3],"r"); char line[256]; long frame=0;
  while(sf && fgets(line,sizeof line,sf)){
    char*p=line;
    if(*p=='!'){
      char cmd[64], arg[128];
      int k=sscanf(p+1,"%63s %127s",cmd,arg);
      if(k>=2 && !strcmp(cmd,"save")){
        size_t sz=ssize(); void*buf=malloc(sz);
        if(sser(buf,sz)){ FILE*o=fopen(arg,"wb"); fwrite(buf,1,sz,o); fclose(o);
          printf("  [%ld] 상태 저장 %s (%zuB)\n",frame,arg,sz); }
        free(buf); continue;
      }
      if(k>=2 && !strcmp(cmd,"load")){
        FILE*o=fopen(arg,"rb");
        if(!o){ printf("  !! %s 없음\n",arg); continue; }
        fseek(o,0,SEEK_END); long sz=ftell(o); fseek(o,0,SEEK_SET);
        void*buf=malloc(sz); if(fread(buf,1,sz,o)!=(size_t)sz){} fclose(o);
        printf("  [%ld] 상태 복원 %s -> %s\n",frame,arg,sunser(buf,sz)?"OK":"실패");
        free(buf); continue;
      }
      if(k>=2 && !strcmp(cmd,"poke")){
        unsigned off; int val;
        if(sscanf(arg,"%x=%d",&off,&val)==2 && off<rlen){ ram[off]=(uint8_t)val;
          printf("  [%ld] poke %04X=%d\n",frame,off,val); }
        continue;
      }
      if(k>=2 && !strcmp(cmd,"w")){
        static FILE *csv = NULL;
        if(!csv){ const char*cp=getenv("PROBE_CSV"); csv=fopen(cp?cp:"probe.csv","w");
          fprintf(csv,"tag,frame,bank,hp2,p1x,p2x,anim,chr,style,p1y,kanim,pow1,pow2\n"); }
        fprintf(csv,"%s,%ld,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", arg, frame,
                ram[0x09AD], ram[0x08CF], ram[0x092E], ram[0x0934],
                ram[0x0C7E], ram[0x08A0], ram[0x08BE], ram[0x0930],
                ram[0x0C7F], ram[0x0963], ram[0x0AE3]);
        fflush(csv); continue;
      }
      if(k>=1){ dump(cmd,ram,rlen); printf("  [%ld] %s 덤프\n",frame,cmd); }
      continue;
    }
    int nf=strtol(p,&p,10); if(nf<=0) continue;
    PAD=0;
    char tok[16]; 
    while(sscanf(p,"%15s",tok)==1){
      p=strstr(p,tok)+strlen(tok);
      if(strcmp(tok,"-")){ int b=btn(tok); if(b>=0) PAD|=(int16_t)(1<<b); }
    }
    for(int i=0;i<nf;i++){ retro_run(); frame++; }
  }
  if(sf) fclose(sf);
  printf("총 %ld 프레임, 화면 %ux%u\n",frame,W,H);
  dump(getenv("TAG")?getenv("TAG"):"end", ram, rlen);
  return 0;
}
