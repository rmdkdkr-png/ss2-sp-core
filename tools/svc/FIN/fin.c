/* FIN — frozen probe harness (derived from svcrun.c, no shared state)
   usage: fin <core.so> <rom> <script>
   script: "<frames> <buttons|->", !load/!save/!poke/!<tag>, and:
     !p <tag>     -> append one row to $FINCSV with the raw offsets below
   env: FINCSV (default fin.csv), TRACE=1 -> row every frame (tag=frame no)
*/
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
static int16_t PAD=0, PAD1=0, PAD2=0; static int SPLIT=0;
static void vcb(const void*d,unsigned w,unsigned h,size_t p){
  unsigned y; W=w;H=h;
  for(y=0;y<h&&y<512;y++) memcpy(FB+y*512,(const char*)d+y*p,(w<512?w:512)*2);
}
static void acb(short a,short b){(void)a;(void)b;}
static size_t abcb(const short*d,size_t f){(void)d;return f;}
static void poll(void){}
static short inp(unsigned port,unsigned dev,unsigned idx,unsigned id){
  (void)dev;(void)idx;
  if(SPLIT) return ((port==0?PAD1:PAD2)>>id)&1;
  return (PAD>>id)&1;
}
static int envcb(unsigned cmd,void*data){
  if(cmd==3){*(int*)data=1;return 1;}
  if(cmd==9||cmd==31){*(const char**)data=".";return 1;}
  if(cmd==52||cmd==53||cmd==67) return 1;
  if(cmd==15){ struct{const char*k;const char*v;}*v=data;
    const char *ov=getenv(v->k); if(ov){ v->v=ov; return 1; } v->v=NULL; return 0;}
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
static FILE *csv=NULL;
#define U8(o)  ram[o]
#define U16(o) (ram[o]|(ram[(o)+1]<<8))
static void row(const char*tag,long frame,const uint8_t*ram){
  if(!csv){ const char*cp=getenv("FINCSV"); csv=fopen(cp?cp:"fin.csv","w");
    fprintf(csv,"tag,frame,chr1,chr2,hp1,hp2,timer,"
      "a92C,a92E,a92F,p1s,a930,p1w,a935,"
      "aAAC,aAAE,aAAF,p2s,aAB0,p2w,aAB5,"
      "cam19A6,a19A7,a0954,a0955,bank,anim,kanim\n"); }
  fprintf(csv,"%s,%ld,%d,%d,%d,%d,%d,"
    "%d,%d,%d,%d,%d,%d,%d,"
    "%d,%d,%d,%d,%d,%d,%d,"
    "%d,%d,%d,%d,%d,%d,%d\n",
    tag,frame,U8(0x08A0),U8(0x08C0),U8(0x0961),U8(0x08CF),U8(0x08BF),
    U8(0x092C),U8(0x092E),U8(0x092F),U16(0x092E),U8(0x0930),U16(0x0934),U8(0x0935),
    U8(0x0AAC),U8(0x0AAE),U8(0x0AAF),U16(0x0AAE),U8(0x0AB0),U16(0x0AB4),U8(0x0AB5),
    U16(0x19A6),U8(0x19A7),U8(0x0954),U8(0x0955),U8(0x09AD),U8(0x0C7E),U8(0x0C7F));
  fflush(csv);
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
  fenv  rse=(fenv)dlsym(h,"retro_set_environment");
  fvid  rsv=(fvid)dlsym(h,"retro_set_video_refresh");
  fas   rsa=(fas)dlsym(h,"retro_set_audio_sample");
  fab   rsab=(fab)dlsym(h,"retro_set_audio_sample_batch");
  fip   rsip=(fip)dlsym(h,"retro_set_input_poll");
  fis   sis=(fis)dlsym(h,"retro_set_input_state");
  fv    rinit=(fv)dlsym(h,"retro_init");
  fload rload=(fload)dlsym(h,"retro_load_game");
  fav   rav=(fav)dlsym(h,"retro_get_system_av_info");
  fv    rrun=(fv)dlsym(h,"retro_run");
  size_t(*ssize)(void)=(size_t(*)(void))dlsym(h,"retro_serialize_size");
  bool(*sser)(void*,size_t)=(bool(*)(void*,size_t))dlsym(h,"retro_serialize");
  bool(*sunser)(const void*,size_t)=(bool(*)(const void*,size_t))dlsym(h,"retro_unserialize");
  void*(*getmem)(unsigned)=(void*(*)(unsigned))dlsym(h,"retro_get_memory_data");
  size_t(*getsz)(unsigned)=(size_t(*)(unsigned))dlsym(h,"retro_get_memory_size");

  rse(envcb); rsv(vcb); rsa(acb); rsab(abcb); rsip(poll); sis(inp); rinit();
  { const char*sp=getenv("SPLITPAD"); SPLIT = sp && *sp=='1'; }

  FILE*rf=fopen(argv[2],"rb"); fseek(rf,0,SEEK_END); long n=ftell(rf); fseek(rf,0,SEEK_SET);
  void*rom=malloc(n); if(fread(rom,1,n,rf)!=(size_t)n){printf("rom read\n");return 1;} fclose(rf);
  struct gi g={argv[2],rom,(size_t)n,NULL};
  if(!rload(&g)){printf("LOAD FAIL\n");return 1;}
  struct av av; rav(&av);
  uint8_t*ram=getmem(2); size_t rlen=getsz(2);
  int trace = getenv("TRACE") && *getenv("TRACE")=='1';

  FILE*sf=fopen(argv[3],"r"); char line[256]; long frame=0;
  while(sf && fgets(line,sizeof line,sf)){
    char*p=line;
    if(*p=='#'||*p=='\n'||*p=='\r') continue;
    if(*p=='!'){
      char cmd[64], arg[128]; arg[0]=0;
      int k=sscanf(p+1,"%63s %127s",cmd,arg);
      if(k>=2 && !strcmp(cmd,"save")){
        size_t sz=ssize(); void*buf=malloc(sz);
        if(sser(buf,sz)){ FILE*o=fopen(arg,"wb"); fwrite(buf,1,sz,o); fclose(o);
          printf("  [%ld] save %s (%zuB)\n",frame,arg,sz); }
        free(buf); continue; }
      if(k>=2 && !strcmp(cmd,"load")){
        FILE*o=fopen(arg,"rb");
        if(!o){ printf("  !! no %s\n",arg); continue; }
        fseek(o,0,SEEK_END); long sz=ftell(o); fseek(o,0,SEEK_SET);
        void*buf=malloc(sz); if(fread(buf,1,sz,o)!=(size_t)sz){} fclose(o);
        printf("  [%ld] load %s -> %s\n",frame,arg,sunser(buf,sz)?"OK":"FAIL");
        free(buf); continue; }
      if(k>=2 && !strcmp(cmd,"poke")){
        unsigned off; int val;
        if(sscanf(arg,"%x=%d",&off,&val)==2 && off<rlen){ ram[off]=(uint8_t)val;
          printf("  [%ld] poke %04X=%d\n",frame,off,val); }
        continue; }
      if(k>=2 && !strcmp(cmd,"p")){ row(arg,frame,ram); continue; }
      if(k>=1){ dump(cmd,ram,rlen); printf("  [%ld] dump %s\n",frame,cmd); }
      continue;
    }
    int nf=strtol(p,&p,10); if(nf<=0) continue;
    PAD=0; PAD1=0; PAD2=0;
    char tok[16];
    while(sscanf(p,"%15s",tok)==1){
      p=strstr(p,tok)+strlen(tok);
      if(!strcmp(tok,"-")) continue;
      if(tok[0]=='1'&&tok[1]==':'){ int b=btn(tok+2); if(b>=0) PAD1|=(int16_t)(1<<b); continue; }
      if(tok[0]=='2'&&tok[1]==':'){ int b=btn(tok+2); if(b>=0) PAD2|=(int16_t)(1<<b); continue; }
      { int b=btn(tok); if(b>=0){ PAD|=(int16_t)(1<<b); PAD1|=(int16_t)(1<<b);} }
    }
    for(int i=0;i<nf;i++){ rrun(); frame++;
      if(trace){ char t[24]; snprintf(t,sizeof t,"f%ld",frame); row(t,frame,ram); } }
  }
  if(sf) fclose(sf);
  printf("frames=%ld  screen %ux%u\n",frame,W,H);
  return 0;
}
