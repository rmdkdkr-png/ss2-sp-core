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
/* [0] 심판 / [1] 해설 / [2] 오버플로(유동) — 밀린 말이 기다리다 버려지는 대신
   빈 버퍼 채널로 흘러 겹쳐 나온다(제보: 「밀리면 버퍼채널 유동적으로 — 호명에도」) */
#define VCN 3
static struct { short *pcm; int len, pos, prio; short *own; } vch[VCN];
#define VCH_OF(prio) ((prio) >= 1 ? 0 : 1)
#define VQD 2                        /* 채널당 대기 칸 — 씹힘 방지 버퍼(제보) */
static struct { short *own; int len; int prio; unsigned at; } vnq[VCN][VQD];
static int vnq_n[VCN];

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
  { int c; for(c = 0; c < VCN; c++){
      int i2;
      if(vch[c].own) free(vch[c].own);
      vch[c].pcm = vch[c].own = 0; vch[c].len = vch[c].pos = 0;
      for(i2 = 0; i2 < vnq_n[c]; i2++) if(vnq[c][i2].own) free(vnq[c][i2].own);
      vnq_n[c] = 0; } }
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
  for(c = 0; c < VCN; c++)
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
                      && cache[worst].pcm != vch[1].pcm
                      && cache[worst].pcm != vch[2].pcm) free(cache[worst].pcm);
  cache[worst].h = h; cache[worst].pcm = out; cache[worst].len = n; cache[worst].used = ++cache_tick;
  *len = n;
  return out;
}

/* ── 대기열·트림·체이닝 ─────────────────────────────────────────
   제보 3건을 여기서 푼다:
   「이름과 대사 사이 갭이 김」 — 생성 패드·잔향을 재생 직전에 잘라낸다(트림).
   「끝나기 전 다음 대사를 시작하면 이어질 듯, 끝을 인지해야」 — 끝 = 파일 끝이
     아니라 에너지 끝(트림된 끝). 거기서 다음 대기분을 같은 프레임에 잇는다.
   「겹칠 때 씹히지 않고 일단 나오게 추가 채널 버퍼」 — 채널마다 대기 2칸.
     구령은 구령을 절대 안 끊고 줄을 선다(자아→N회전→승부 3연 보존).
     해설은 예전처럼 교체하되, 끝물(0.4초 미만)이면 끊지 않고 잇는다. */

static unsigned vc_clock;   /* 믹스한 샘플 시계 — 대기 스테일(1.2초) 판정용 */

static int has_clip(unsigned h){
  int i;
  if(pak_f) return pak_find(h) >= 0;
  for(i = 0; i < vc_n; i++) if(vc_hash[i] == h) return 1;
  return 0;
}

/* 에너지 창 — 앞뒤 무음(|s|<=300)을 걷어낸 [s0,s1) */
static void trim_span(const short *p, int len, int *s0, int *s1){
  int i, a = -1, b = 0;
  for(i = 0; i < len; i++) if(p[i] > 300 || p[i] < -300){ a = i; break; }
  if(a < 0){ *s0 = 0; *s1 = 0; return; }
  for(i = len - 1; i >= 0; i--) if(p[i] > 300 || p[i] < -300){ b = i + 1; break; }
  *s0 = a; *s1 = b;
}

#define VQ_STALE  52920              /* 1.2초 — 순간이 지난 구령은 버린다 */

static void vq_clear(int c){
  int i;
  for(i = 0; i < vnq_n[c]; i++) if(vnq[c][i].own) free(vnq[c][i].own);
  vnq_n[c] = 0;
}
static void vq_push(int c, const short *pcm, int s0, int s1, int prio){
  int n = s1 - s0; short *b;
  if(n <= 0) return;
  if(vnq_n[c] >= VQD){                       /* 꽉 참 — 묵은 것부터 밀어낸다 */
    free(vnq[c][0].own);
    memmove(&vnq[c][0], &vnq[c][1], sizeof vnq[c][0] * (VQD - 1));
    vnq_n[c] = VQD - 1;
  }
  b = (short *)malloc((size_t)n * 2);
  if(!b) return;
  memcpy(b, pcm + s0, (size_t)n * 2);
  vnq[c][vnq_n[c]].own = b; vnq[c][vnq_n[c]].len = n;
  vnq[c][vnq_n[c]].prio = prio; vnq[c][vnq_n[c]].at = vc_clock;
  vnq_n[c]++;
}
static void vch_take(int c, short *pcm, short *own, int pos, int len, int prio){
  if(vch[c].own && vch[c].own != own) free(vch[c].own);
  vch[c].pcm = pcm; vch[c].own = own; vch[c].pos = pos; vch[c].len = len; vch[c].prio = prio;
}
static void vq_pop(int c){                   /* 대기분으로 즉시 전환(스테일은 걷어내며) */
  while(vnq_n[c]){
    short *own = vnq[c][0].own; int len = vnq[c][0].len, prio = vnq[c][0].prio;
    unsigned at = vnq[c][0].at;
    memmove(&vnq[c][0], &vnq[c][1], sizeof vnq[c][0] * (VQD - 1));
    vnq_n[c]--;
    if(vc_clock - at > VQ_STALE){ free(own); continue; }
    vch_take(c, own, own, 0, len, prio);
    return;
  }
}
static int vch_idle(int c){ return !(vch[c].pcm && vch[c].pos < vch[c].len); }
/* 채널이 바쁠 때의 정책 — 1 이면 처리 끝(대기열/오버플로로 갔다) */
static int vch_busy_enqueue(int c, const short *pcm, int s0, int s1, int prio){
  if(vch_idle(c) && !vnq_n[c]) return 0;     /* 한가하고 밀린 것도 없다 — 바로 재생 */
  if(c == 0){
    /* 심판이 겹치면(구령 중 호명 등) 빈 오버플로로 흘려 같이 낸다 — 순간을 놓치지 않는다 */
    if(vch_idle(2) && !vnq_n[2]){
      vch_take(2, 0, 0, 0, 0, prio);         /* 자리 선점 표시 */
      { short *b = (short *)malloc((size_t)(s1 - s0) * 2);
        if(!b){ vch[2].pcm = 0; vq_push(0, pcm, s0, s1, prio); return 1; }
        memcpy(b, pcm + s0, (size_t)(s1 - s0) * 2);
        vch_take(2, b, b, 0, s1 - s0, prio); }
      return 1;
    }
    vq_push(0, pcm, s0, s1, prio);           /* 오버플로도 참 — 줄 선다(구령 연쇄 보존) */
    return 1;
  }
  if(!vch_idle(1) && vch[1].len - vch[1].pos < 17640){  /* 해설 끝물 — 뭉개지 말고 잇는다 */
    vq_clear(1);                             /* 대기분은 최신 한 줄만 */
    vq_push(1, pcm, s0, s1, prio);
    return 1;
  }
  if(vch_idle(2) && !vnq_n[2]){              /* 해설 한복판 — 빈 버퍼 채널로 겹쳐 낸다 */
    short *b = (short *)malloc((size_t)(s1 - s0) * 2);
    if(b){ memcpy(b, pcm + s0, (size_t)(s1 - s0) * 2);
           vch_take(2, b, b, 0, s1 - s0, prio); return 1; }
  }
  return 0;                                  /* 버퍼도 참 — 예전처럼 교체(자막 동기) */
}

static void vc_start(unsigned h, int prio){
  int c = VCH_OF(prio);
  int len = 0, s0, s1; short *pcm = clip_get(h, &len);
  if(!pcm) return;
  trim_span(pcm, len, &s0, &s1);
  if(s1 <= s0) return;
  if(vch_busy_enqueue(c, pcm, s0, s1, prio)) return;
  vq_clear(c);                               /* 교체 — 물려 있던 대기분도 순간이 지났다 */
  vch_take(c, pcm, 0, s0, s1, prio);
}

/* 이어붙이기 — [머리][이름][꼬리] 조각을 트림해 12ms 틈으로 한 버퍼에 잇는다.
   조각은 합성키(제어문자 접두, 실대사 해시와 충돌 불가)로 팩에 들어 있다. */
void ss2voice_say_parts(const char *k1, const char *k2, const char *k3, int prio){
  const char *ks[3]; short *pc[3]; int ln[3], a0[3], a1[3];
  int nk = 0, i, tot = 0, gap = 530, off = 0, c = VCH_OF(prio);
  short *buf;
  if(!vc_ready) return;
  if(k1 && *k1) ks[nk++] = k1;
  if(k2 && *k2) ks[nk++] = k2;
  if(k3 && *k3) ks[nk++] = k3;
  if(!nk) return;
  for(i = 0; i < nk; i++){
    pc[i] = clip_get(fnv1a(ks[i]), &ln[i]);
    if(!pc[i]) return;                       /* 조각 하나라도 없으면 자막만 */
    trim_span(pc[i], ln[i], &a0[i], &a1[i]);
    tot += a1[i] - a0[i];
  }
  if(tot <= 0) return;
  buf = (short *)malloc((size_t)(tot + gap * (nk - 1)) * 2);
  if(!buf) return;
  for(i = 0; i < nk; i++){
    int n = a1[i] - a0[i];
    memcpy(buf + off, pc[i] + a0[i], (size_t)n * 2); off += n;
    if(i < nk - 1){ memset(buf + off, 0, (size_t)gap * 2); off += gap; }
  }
  if(vch_busy_enqueue(c, buf, 0, off, prio)){ free(buf); return; }
  vq_clear(c);
  vch_take(c, buf, buf, 0, off, prio);
}

void ss2voice_say(const char *text, int prio){
  unsigned h;
  if(!vc_ready || !text || !*text) return;
  h = fnv1a(text);
  if(!has_clip(h)) return;                   /* 팩에 없는 대사 = 자막만 */
  /* 적시 시작 — 자막이 갈리는 그 순간 음성도 갈린다. 구령·해설은 채널이 갈려
     서로 못 끊고, 같은 채널의 순서 다툼은 vch_busy_enqueue 가 정리한다. */
  vc_start(h, prio);
}

void ss2voice_mix(int16_t *buf, int frames){
  int i, c, a0v, a1v;
  if(!vc_ready) return;
  for(c = 0; c < VCN; c++)
    if((!vch[c].pcm || vch[c].pos >= vch[c].len) && vnq_n[c]) vq_pop(c);
  a0v = 0; a1v = 0;
  for(c = 0; c < VCN; c++) if(!vch_idle(c)) a0v = 1;
  if(!a0v){ vc_clock += (unsigned)frames; return; }
  (void)a1v;
  for(i = 0; i < frames; i++){
    int l = buf[i * 2], r = buf[i * 2 + 1], v = 0;
    /* 심판(prio>=1)이 어느 채널에서든 살아 있으면 해설류는 70% */
    int refAct = 0;
    for(c = 0; c < VCN; c++)
      if(!vch_idle(c) && vch[c].prio >= 1) refAct = 1;
    for(c = 0; c < VCN; c++){
      int vc2;
      if(vch_idle(c)) continue;
      vc2 = vch[c].pcm[vch[c].pos++];
      if(vch[c].prio < 1){
        if(refAct) vc2 = vc2 * 7 / 10;              /* 구령 아래서는 낮게 */
        else if(c == 1 && !vch_idle(2) && vch[2].prio < 1)
          vc2 = vc2 * 7 / 10;                        /* 해설 겹침 — 먼저 말하던 쪽을 낮게 */
      }
      v += vc2;
      if(vch[c].pos >= vch[c].len) vq_pop(c);        /* 에너지 끝 — 같은 프레임에 잇는다 */
    }
    l += v; r += v;   /* 게임 소리는 그대로, 음성만 가산 (제보: 「볼륨 줄이지 말기」) */
    if(l > 32767) l = 32767; if(l < -32768) l = -32768;
    if(r > 32767) r = 32767; if(r < -32768) r = -32768;
    buf[i * 2] = (int16_t)l; buf[i * 2 + 1] = (int16_t)r;
  }
  vc_clock += (unsigned)frames;
  for(c = 0; c < VCN; c++)
    if(vch[c].pcm && vch[c].pos >= vch[c].len){
      if(vch[c].own){ free(vch[c].own); vch[c].own = 0; }
      vch[c].pcm = 0;
    }
}
