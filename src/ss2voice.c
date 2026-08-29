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

/* 2채널 — [0] 심판(prio>=1) / [1] 해설(prio 0). 한쪽이 도는 동안 다른 쪽이
   기다리다 밀리는 손실을 없앤다(제보: 「딜레이되는 시점에 2번째 채널로 이어서」).
   같은 채널 안에서는 예전처럼 새 말이 옛 말을 교체한다(자막과 동기). */
static struct { short *pcm; int len, pos, prio; short *own; } vch[2];
#define VCH_OF(prio) ((prio) >= 1 ? 0 : 1)

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
  vc_ready = 0; vc_n = 0;
  { int c; for(c = 0; c < 2; c++){
      if(vch[c].own) free(vch[c].own);
      vch[c].pcm = vch[c].own = 0; vch[c].len = vch[c].pos = 0; } }
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

int ss2voice_playing_prio(void){   /* 지금 도는 클립의 최고 우선순위. 무재생 = -1 */
  int c, best = -1;
  if(!vc_ready) return -1;
  for(c = 0; c < 2; c++)
    if(vch[c].pcm && vch[c].pos < vch[c].len && vch[c].prio > best) best = vch[c].prio;
  return best;
}
int ss2voice_count(void){ return pak_f ? pak_n : vc_n; }

static int has_clip(unsigned h);           /* 아래 정의 — 전방 선언 */
int ss2voice_has_text(const char *text){   /* 엔진의 대사 선택용 — 이 문장, 말할 수 있나 */
  if(!vc_ready || !text || !*text) return 0;
  return has_clip(fnv1a(text));
}

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
  if(cache[worst].pcm && cache[worst].pcm != vch[0].pcm
                      && cache[worst].pcm != vch[1].pcm) free(cache[worst].pcm);
  cache[worst].h = h; cache[worst].pcm = out; cache[worst].len = n; cache[worst].used = ++cache_tick;
  *len = n;
  return out;
}

/* 대기열 — 구호 연쇄(자아→N회전→승부!)와 KO 순간(그만!→이름 호명→해설)이
   서로 끊어먹지 않게 한다 (제보: 「인트로 구호 다 씹힌다」「이름 안 부른다」).
   규칙: 구호는 구호를 절대 안 끊고 줄을 선다. 해설은 해설을 끊고(자막과 동기),
   구호 재생 중엔 뒤에 줄 선다(해설 대기는 최신 한 줄만). */
#define VQ_N 4
static struct { unsigned h; int prio; } vq[VQ_N];
static int vq_n;

static int has_clip(unsigned h){
  int i;
  if(pak_f) return pak_find(h) >= 0;
  for(i = 0; i < vc_n; i++) if(vc_hash[i] == h) return 1;
  return 0;
}
static void vc_start(unsigned h, int prio){
  int c = VCH_OF(prio);
  int len = 0; short *pcm = clip_get(h, &len);
  if(!pcm){ vch[c].pcm = 0; return; }
  if(vch[c].own){ free(vch[c].own); vch[c].own = 0; }   /* 물러나는 결합 버퍼 정리 */
  vch[c].pcm = pcm; vch[c].len = len; vch[c].pos = 0; vch[c].prio = prio;
}
/* 이어붙이기 — 기술명처럼 조합이 폭발하는 대사는 [이름][꼬리] 조각을 즉석에서
   한 버퍼로 이어 재생한다 (제보: 「어색해도 그 방식으로」). 조각은 합성키
   (제어문자 접두, 실대사 해시와 충돌 불가)로 팩에 들어 있다. 틈 40ms. */
void ss2voice_say_parts(const char *k1, const char *k2, const char *k3, int prio){
  const char *ks[3]; short *pc[3]; int ln[3], nk = 0, i, tot = 0, gap = 1764, off = 0;
  short *buf;
  if(!vc_ready) return;
  /* 채널이 갈려 구령 보호가 필요 없다 — 해설은 해설 채널에서만 교체된다 */
  if(k1 && *k1) ks[nk++] = k1;
  if(k2 && *k2) ks[nk++] = k2;
  if(k3 && *k3) ks[nk++] = k3;
  if(!nk) return;
  for(i = 0; i < nk; i++){
    pc[i] = clip_get(fnv1a(ks[i]), &ln[i]);
    if(!pc[i]) return;                             /* 조각 하나라도 없으면 자막만 */
    tot += ln[i];
  }
  buf = (short *)malloc((size_t)(tot + gap * (nk - 1)) * 2);
  if(!buf) return;
  for(i = 0; i < nk; i++){
    memcpy(buf + off, pc[i], (size_t)ln[i] * 2); off += ln[i];
    if(i < nk - 1){ memset(buf + off, 0, (size_t)gap * 2); off += gap; }
  }
  { int c = VCH_OF(prio);
    if(vch[c].own) free(vch[c].own);
    vch[c].own = buf;
    vch[c].pcm = buf; vch[c].len = off; vch[c].pos = 0; vch[c].prio = prio; }
}

void ss2voice_say(const char *text, int prio){
  unsigned h;
  if(!vc_ready || !text || !*text) return;
  h = fnv1a(text);
  if(!has_clip(h)) return;                       /* 팩에 없는 대사 = 자막만 */
  /* 적시 시작 — 자막이 갈리는 그 순간 음성도 갈린다. 줄 세우지 않는다.
     구령과 해설은 채널이 갈려 서로 못 끊는다 — 겹치면 믹서가 해설을 낮춘다. */
  vc_start(h, prio);
}

void ss2voice_mix(int16_t *buf, int frames){
  int i, a0, a1;
  if(!vc_ready) return;
  a0 = vch[0].pcm && vch[0].pos < vch[0].len;
  a1 = vch[1].pcm && vch[1].pos < vch[1].len;
  if(!a0 && !a1) return;
  for(i = 0; i < frames; i++){
    int l = buf[i * 2], r = buf[i * 2 + 1], v = 0;
    if(vch[0].pcm && vch[0].pos < vch[0].len) v += vch[0].pcm[vch[0].pos++];
    if(vch[1].pcm && vch[1].pos < vch[1].len){
      int v1 = vch[1].pcm[vch[1].pos++];
      /* 구령과 겹치는 동안만 해설 70% — 폰 스피커에서 둘 다 들리게 */
      if(vch[0].pcm && vch[0].pos < vch[0].len) v1 = v1 * 7 / 10;
      v += v1;
    }
    l += v; r += v;   /* 게임 소리는 그대로, 음성만 가산 (제보: 「볼륨 줄이지 말기」) */
    if(l > 32767) l = 32767; if(l < -32768) l = -32768;
    if(r > 32767) r = 32767; if(r < -32768) r = -32768;
    buf[i * 2] = (int16_t)l; buf[i * 2 + 1] = (int16_t)r;
  }
  if(vch[0].pcm && vch[0].pos >= vch[0].len) vch[0].pcm = 0;
  if(vch[1].pcm && vch[1].pos >= vch[1].len) vch[1].pcm = 0;
}
