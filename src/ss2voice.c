/* ss2voice — 해설 음성 팩 재생기 (기획: tools/voice/PLAN.md)

   구조: 대사 문자열의 FNV-1a(32) 해시로 manifest.tsv 에서 OGG 를 찾아,
   에뮬 출력(44.1kHz 스테레오 s16)에 가산 믹싱한다. 게임 소리는 재생 중 덕킹.
   팩이 없으면 완전 무해 — 배포 코어에 넣어도 팩을 둔 사람만 소리가 난다.

   팩 규약(마스터링은 tools/voice/ 파이프라인이 보장):
     <dir>/manifest.tsv   한 줄 = "%08x\t파일명"  (해시 = 최종 표시 문자열의 UTF-8)
     <dir>/*.ogg          44.1kHz mono, 음량 정규화 완료 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ss2voice.h"

#define STB_VORBIS_NO_PUSHDATA_API
#include "stb_vorbis.c"

#define VC_MAXCLIP 32768
#define VC_CACHE   6
#define VC_RATE    44100

static char     vc_dir[512];
static int      vc_ready;
static unsigned vc_hash[VC_MAXCLIP];
static char    *vc_file[VC_MAXCLIP];
static int      vc_n;

static struct { unsigned h; short *pcm; int len; unsigned used; } cache[VC_CACHE];
static unsigned cache_tick;

static short *cur_pcm; static int cur_len, cur_pos, cur_prio;

static unsigned fnv1a(const char *s){
  unsigned h = 2166136261u;
  while(*s){ h ^= (unsigned char)*s++; h *= 16777619u; }
  return h;
}

/* ── 단일 팩 파일 (.pak) — 폰에서 압축 해제 없이 파일 하나로 설치 ──
   'NGPV1\0' + u32 개수 + [u32 해시,u32 오프셋,u32 크기] 해시 오름차순 + 블롭 */
static FILE     *pak_f;
static unsigned *pak_h, *pak_off, *pak_sz;
static int       pak_n;

static unsigned rd_u32(FILE *f){
  unsigned char b[4];
  if(fread(b, 1, 4, f) != 4) return 0;
  return (unsigned)b[0] | ((unsigned)b[1] << 8) | ((unsigned)b[2] << 16) | ((unsigned)b[3] << 24);
}
static int pak_open(const char *path){
  char magic[6]; int i;
  FILE *f = fopen(path, "rb");
  if(!f) return 0;
  if(fread(magic, 1, 6, f) != 6 || memcmp(magic, "NGPV1", 6)){ fclose(f); return 0; }
  pak_n = (int)rd_u32(f);
  if(pak_n <= 0 || pak_n > 200000){ fclose(f); pak_n = 0; return 0; }
  pak_h   = (unsigned *)malloc((size_t)pak_n * 4);
  pak_off = (unsigned *)malloc((size_t)pak_n * 4);
  pak_sz  = (unsigned *)malloc((size_t)pak_n * 4);
  if(!pak_h || !pak_off || !pak_sz){ fclose(f); pak_n = 0; return 0; }
  for(i = 0; i < pak_n; i++){ pak_h[i] = rd_u32(f); pak_off[i] = rd_u32(f); pak_sz[i] = rd_u32(f); }
  pak_f = f;                                     /* 열어 둔다 — 클립은 그때그때 읽는다 */
  return 1;
}
static int pak_find(unsigned h){
  int lo = 0, hi = pak_n - 1;
  while(lo <= hi){
    int mid = (lo + hi) / 2;
    if(pak_h[mid] == h) return mid;
    if(pak_h[mid] < h) lo = mid + 1; else hi = mid - 1;
  }
  return -1;
}

void ss2voice_init(const char *dir){
  char path[600]; FILE *f; char line[600];
  const char *env = getenv("SS2VOICE_DIR");
  if(env && *env) dir = env;                     /* 하네스 우선 */
  vc_ready = 0; vc_n = 0; cur_pcm = 0;
  if(pak_f){ fclose(pak_f); pak_f = 0; pak_n = 0; }
  if(!dir || !*dir) return;
  snprintf(vc_dir, sizeof vc_dir, "%s", dir);
  snprintf(path, sizeof path, "%s.pak", dir);    /* 1순위: <이름>.pak 단일 파일 */
  if(pak_open(path)){ vc_ready = 1; return; }
  snprintf(path, sizeof path, "%s/ngpvoice.pak", dir);
  if(pak_open(path)){ vc_ready = 1; return; }
  snprintf(path, sizeof path, "%s/manifest.tsv", dir);   /* 2순위: 폴더 팩 */
  f = fopen(path, "rb");
  if(!f) return;                                 /* 팩 없음 = 비활성 */
  while(vc_n < VC_MAXCLIP && fgets(line, sizeof line, f)){
    char *tab = strchr(line, '\t'); char *nl;
    if(!tab) continue;
    *tab = 0;
    nl = strpbrk(tab + 1, "\r\n"); if(nl) *nl = 0;
    if(!tab[1]) continue;
    vc_hash[vc_n] = (unsigned)strtoul(line, 0, 16);
    vc_file[vc_n] = strdup(tab + 1);
    vc_n++;
  }
  fclose(f);
  vc_ready = vc_n > 0;
}

int ss2voice_on(void){ return vc_ready; }

static short *clip_get(unsigned h, int *len){
  int i, worst = 0; char path[900];
  int chan = 0, rate = 0; short *out = 0; int n;
  for(i = 0; i < VC_CACHE; i++)
    if(cache[i].pcm && cache[i].h == h){ cache[i].used = ++cache_tick; *len = cache[i].len; return cache[i].pcm; }
  if(pak_f){                                     /* 단일 팩 — 오프셋 읽기 + 메모리 디코드 */
    unsigned char *buf; int k = pak_find(h);
    if(k < 0) return 0;
    buf = (unsigned char *)malloc(pak_sz[k]);
    if(!buf) return 0;
    if(fseek(pak_f, (long)pak_off[k], SEEK_SET) ||
       fread(buf, 1, pak_sz[k], pak_f) != pak_sz[k]){ free(buf); return 0; }
    if(pak_sz[k] > 4 && !memcmp(buf, "OggS", 4))
      n = stb_vorbis_decode_memory(buf, (int)pak_sz[k], &chan, &rate, &out);
    else {                                       /* 원시 s16 mono 44.1k */
      out = (short *)buf; n = (int)(pak_sz[k] / 2); chan = 1; rate = VC_RATE; buf = 0;
    }
    free(buf);
    goto got;
  }
  for(i = 0; i < vc_n; i++) if(vc_hash[i] == h) break;
  if(i >= vc_n) return 0;
  snprintf(path, sizeof path, "%s/%s", vc_dir, vc_file[i]);
  { size_t fl = strlen(path);                     /* .raw = s16 mono 44.1k 그대로 */
    if(fl > 4 && !strcmp(path + fl - 4, ".raw")){
      FILE *rf = fopen(path, "rb"); long sz;
      if(!rf) return 0;
      fseek(rf, 0, SEEK_END); sz = ftell(rf); fseek(rf, 0, SEEK_SET);
      out = (short *)malloc((size_t)sz);
      if(!out){ fclose(rf); return 0; }
      if(fread(out, 1, (size_t)sz, rf) != (size_t)sz){ free(out); fclose(rf); return 0; }
      fclose(rf);
      n = (int)(sz / 2); chan = 1; rate = VC_RATE;
    }else
      n = stb_vorbis_decode_filename(path, &chan, &rate, &out);
  }
got:
  if(n <= 0 || !out) return 0;
  if(chan > 1){                                   /* 모노로 접는다 */
    int k; for(k = 0; k < n; k++) out[k] = out[k * chan];
  }
  if(rate != VC_RATE && rate > 0){                /* 규약 밖 팩 대비 최근접 리샘플 */
    int m = (int)((long long)n * VC_RATE / rate);
    short *r = (short *)malloc((size_t)m * 2);
    if(r){ int k; for(k = 0; k < m; k++) r[k] = out[(long long)k * rate / VC_RATE];
           free(out); out = r; n = m; }
  }
  for(i = 1; i < VC_CACHE; i++) if(!cache[i].pcm || cache[i].used < cache[worst].used) worst = i;
  if(cache[worst].pcm && cache[worst].pcm != cur_pcm) free(cache[worst].pcm);
  cache[worst].h = h; cache[worst].pcm = out; cache[worst].len = n; cache[worst].used = ++cache_tick;
  *len = n;
  return out;
}

void ss2voice_say(const char *text, int prio){
  unsigned h; short *pcm; int len = 0;
  if(!vc_ready || !text || !*text) return;
  if(cur_pcm && cur_pos < cur_len){
    /* 제보 「대사가 많이 씹힌다」 — 말은 끝까지 한다. 재생 중에 온 해설은
       음성만 양보하고 자막으로 나간다. 쿠로코 구호(prio 1)만 끼어든다. */
    if(prio == 0) return;
  }
  h = fnv1a(text);
  pcm = clip_get(h, &len);
  if(!pcm) return;                                /* 팩에 없는 대사 = 자막만 */
  cur_pcm = pcm; cur_len = len; cur_pos = 0; cur_prio = prio;
}

void ss2voice_mix(int16_t *buf, int frames){
  int i;
  if(!vc_ready || !cur_pcm || cur_pos >= cur_len) return;
  for(i = 0; i < frames; i++){
    int l = buf[i * 2], r = buf[i * 2 + 1], v = 0;
    if(cur_pos < cur_len) v = cur_pcm[cur_pos++];
    l = (l * 7) / 16 + v;   r = (r * 7) / 16 + v;   /* 덕킹 0.44 + 음성 가산 */
    if(l > 32767) l = 32767; if(l < -32768) l = -32768;
    if(r > 32767) r = 32767; if(r < -32768) r = -32768;
    buf[i * 2] = (int16_t)l; buf[i * 2 + 1] = (int16_t)r;
  }
  if(cur_pos >= cur_len) cur_pcm = 0;
}
