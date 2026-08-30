import re,io,sys
p='/home/dudu/ss2/repo/ss2-main/tools/svc/vfin/svcrun2.c'
s=open(p,encoding='utf-8').read()

# 1) globals for tracing
s=s.replace('/* libretro joypad id */', '''
/* ---- per-frame trace ---- */
static unsigned TOFF[64]; static int NTOFF=0; static FILE*TF=NULL;
static char LAB[64]="-";
static void trace_init(void){
  const char*e=getenv("TRACEF"); if(!e) return;
  char buf[512]; snprintf(buf,sizeof buf,"%s",e);
  char*t=strtok(buf,",");
  while(t&&NTOFF<64){ TOFF[NTOFF++]=(unsigned)strtoul(t,NULL,16); t=strtok(NULL,","); }
  const char*o=getenv("TRACEOUT"); TF=fopen(o?o:"trace.csv","w");
  fprintf(TF,"lab,frame");
  for(int i=0;i<NTOFF;i++) fprintf(TF,",w%04X,b%04X,b%04X",TOFF[i],TOFF[i],TOFF[i]+1);
  fprintf(TF,"\\n");
}
static void trace_row(long frame,const uint8_t*ram,size_t rlen){
  if(!TF) return;
  fprintf(TF,"%s,%ld",LAB,frame);
  for(int i=0;i<NTOFF;i++){ unsigned o=TOFF[i];
    int lo=(o<rlen)?ram[o]:-1, hi=(o+1<rlen)?ram[o+1]:-1;
    fprintf(TF,",%d,%d,%d", (lo<0||hi<0)?-1:(lo|(hi<<8)), lo, hi); }
  fprintf(TF,"\\n");
}
/* libretro joypad id */''')

# 2) init after ram acquired
s=s.replace('printf("SYSTEM_RAM %p  %zu bytes\\n",(void*)ram,rlen);',
            'printf("SYSTEM_RAM %p  %zu bytes\\n",(void*)ram,rlen);\n  trace_init();')

# 3) label command + per-frame trace
s=s.replace('''      if(k>=1){ dump(cmd,ram,rlen); printf("  [%ld] %s 덤프\\n",frame,cmd); }''',
            '''      if(k>=2 && !strcmp(cmd,"lab")){ snprintf(LAB,sizeof LAB,"%s",arg); continue; }
      if(k>=2 && !strcmp(cmd,"dumpall")){ FILE*o=fopen(arg,"wb"); fwrite(ram,1,rlen,o); fclose(o); continue; }
      if(k>=1){ dump(cmd,ram,rlen); printf("  [%ld] %s 덤프\\n",frame,cmd); }''')

s=s.replace('    for(int i=0;i<nf;i++){ retro_run(); frame++; }',
            '    for(int i=0;i<nf;i++){ retro_run(); frame++; trace_row(frame,ram,rlen); }')

s=s.replace('  if(sf) fclose(sf);','  if(TF) fclose(TF);\n  if(sf) fclose(sf);')
open(p,'w',encoding='utf-8').write(s)
print("patched")
