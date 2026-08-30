import re
s=open("svctrace.c",encoding="utf-8").read()

anchor='static int16_t PAD=0;'
assert anchor in s
i=s.index(anchor); j=s.index('\n',i)
add=r'''
/* -- added: per-frame trace of arbitrary offsets -- */
static uint8_t *G_ram; static size_t G_rlen;
static unsigned TRC[64]; static int TRC_W[64]; static int TRC_N=0;
static FILE *TRC_F=NULL; static char TRC_TAG[64]="-";
static void trace_init(void){
  const char*e=getenv("TRACE"); if(!e) return;
  char buf[1024]; snprintf(buf,sizeof buf,"%s",e);
  char*sp=buf,*tok;
  while((tok=strtok(sp,","))){ sp=NULL; unsigned o=0; int w=8;
    if(sscanf(tok,"%x:%d",&o,&w)<1) continue;
    if(!strchr(tok,':')) w=8;
    TRC[TRC_N]=o; TRC_W[TRC_N]=w; TRC_N++; if(TRC_N>=64) break; }
  const char*p=getenv("TRACE_CSV"); TRC_F=fopen(p?p:"trace.csv","w");
  fprintf(TRC_F,"tag,frame");
  for(int i=0;i<TRC_N;i++) fprintf(TRC_F,",x%04X%s",TRC[i],TRC_W[i]==16?"w":"");
  fprintf(TRC_F,"\n");
}
static void trace_row(long frame){
  if(!TRC_F) return;
  fprintf(TRC_F,"%s,%ld",TRC_TAG,frame);
  for(int i=0;i<TRC_N;i++){ unsigned o=TRC[i];
    int v = (TRC_W[i]==16) ? (G_ram[o]|(G_ram[o+1]<<8)) : G_ram[o];
    fprintf(TRC_F,",%d",v); }
  fprintf(TRC_F,"\n");
  fflush(TRC_F);
}
'''
s=s[:j+1]+add+s[j+1:]

a2='uint8_t*ram=getmem(2); size_t rlen=getsz(2);'
assert a2 in s
s=s.replace(a2, a2+"\n  G_ram=ram; G_rlen=rlen; trace_init();",1)

a3='      if(k>=2 && !strcmp(cmd,"poke")){'
assert a3 in s
ins='''      if(k>=2 && !strcmp(cmd,"tag")){ snprintf(TRC_TAG,sizeof TRC_TAG,"%s",arg); continue; }
      if(k>=2 && !strcmp(cmd,"poke16")){
        unsigned off; int val;
        if(sscanf(arg,"%x=%d",&off,&val)==2 && off+1<rlen){ ram[off]=(uint8_t)(val&255); ram[off+1]=(uint8_t)((val>>8)&255);
          printf("  [%ld] poke16 %04X=%d\\n",frame,off,val); }
        continue;
      }
'''
s=s.replace(a3, ins+a3,1)

a4='    for(int i=0;i<nf;i++){ retro_run(); frame++; }'
assert a4 in s
s=s.replace(a4,'    for(int i=0;i<nf;i++){ retro_run(); frame++; trace_row(frame); }',1)
open("svctrace.c","w",encoding="utf-8").write(s)
print("patched ok")
