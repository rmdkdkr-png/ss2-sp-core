/* ss2sfx — 원샷 효과음 재생기. 설계 근거는 ss2sfx.h 머리말 참조.

   ss2voice 와 다른 점만 적는다:
     · 채널 8개, 역할 고정 없음 — 빈 자리에 그냥 얹는다.
     · 대기열이 **없다**. 순간이 지난 효과음은 의미가 없어서 줄을 세울 이유가 없다.
       꽉 차면 가장 오래 울린 채널을 빼앗는다 — 버리지 않는다.
     · 디코드는 로드 때 **전량 상주**. 해설처럼 그때그때 풀면(캐시 6칸) 연타에서
       그 자리 디코드가 오디오 생산 스레드를 잡아 끊긴다.
     · 같은 소리가 연달아 나면 기계처럼 들리므로 재생 속도를 ±5% 흔든다.
       흔들기는 결정적(고정 시드 LCG) — 하네스 검증이 재현돼야 한다.

   stb_vorbis 는 ss2voice.c 가 이미 번역단위에 포함하고 있다. 여기서 또 include 하면
   심볼이 겹치므로 **선언만** 끌어다 쓴다. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ss2sfx.h"

extern int stb_vorbis_decode_memory(const unsigned char *mem, int len,
                                    int *channels, int *sample_rate, short **output);

#define SFX_CH      8        /* 동시발음 — 연타·다단히트가 겹쳐도 씹히지 않을 만큼 */
#define SFX_MAXCLIP 64
#define SFX_RATE    44100
#define SFX_ATK     32       /* 시작 램프(샘플) — 빼앗긴 자리의 불연속을 눌러 준다 */
#define SFX_REL     64       /* 끝 램프 — 절단 클릭 제거 */

static int      sfx_ready;
static int      sfx_on_user = 1;
static int      sfx_vol = 50;   /* 기본 절반 — 클립이 -3dB 라 100% 면 게임을 덮는다(실측) */

static struct { unsigned h; short *pcm; int len; } clip[SFX_MAXCLIP];
static int      clip_n;

/* pos 는 16.16 고정소수 — 피치 흔들기 때문에 정수 인덱스로는 모자란다 */
static struct { const short *pcm; int len; unsigned pos, step, age; } ch[SFX_CH];
static unsigned sfx_tick;
static int      sfx_started;   /* 실제로 시작된 클립 수 — 검증용 계기 */

int  ss2sfx_started(void){ return sfx_started; }
void ss2sfx_started_reset(void){ sfx_started = 0; }

static unsigned fnv1a(const char *s){
  unsigned h = 2166136261u;
  while(*s){ h ^= (unsigned char)*s++; h *= 16777619u; }
  return h;
}

/* ── 팩 읽기 ── 'NGPV1\0' + u32 개수 + [u32 해시,u32 오프셋,u32 크기]×N + 블롭 */
static unsigned rd_u32(FILE *f){
  unsigned char b[4];
  if(fread(b, 1, 4, f) != 4) return 0;
  return (unsigned)b[0] | ((unsigned)b[1]<<8) | ((unsigned)b[2]<<16) | ((unsigned)b[3]<<24);
}

static short *decode_one(FILE *f, unsigned off, unsigned sz, int *out_len){
  unsigned char *raw; short *pcm = 0; int chn = 0, rate = 0, n = 0;
  raw = (unsigned char *)malloc(sz ? sz : 1);
  if(!raw) return 0;
  if(fseek(f, (long)off, SEEK_SET) || fread(raw, 1, sz, f) != sz){ free(raw); return 0; }
  if(sz > 4 && !memcmp(raw, "OggS", 4)){
    n = stb_vorbis_decode_memory(raw, (int)sz, &chn, &rate, &pcm);
    free(raw);
  }else{                                        /* 원시 s16 mono 44.1k */
    pcm = (short *)raw; n = (int)(sz / 2); chn = 1; rate = SFX_RATE;
  }
  if(n <= 0 || !pcm){ if(pcm) free(pcm); return 0; }
  if(chn > 1){ int k; for(k = 0; k < n; k++) pcm[k] = pcm[k*chn]; }
  if(rate != SFX_RATE && rate > 0){             /* 규약 밖 팩 대비 최근접 리샘플 */
    int m = (int)((long long)n * SFX_RATE / rate);
    short *r = (short *)malloc((size_t)m * 2);
    if(r){ int k; for(k = 0; k < m; k++) r[k] = pcm[(long long)k * rate / SFX_RATE];
           free(pcm); pcm = r; n = m; }
  }
  *out_len = n;
  return pcm;
}

void ss2sfx_reset(void){
  int i;
  for(i = 0; i < SFX_CH; i++){ ch[i].pcm = 0; ch[i].len = 0; ch[i].pos = 0; }
}

void ss2sfx_init(const char *pak_path){
  FILE *f; char magic[6]; int i, n;
  unsigned *hh, *oo, *ss;
  { int k; for(k = 0; k < clip_n; k++) if(clip[k].pcm) free(clip[k].pcm); }
  clip_n = 0; sfx_ready = 0;
  ss2sfx_reset();
  { const char *env = getenv("SS2SFX_PAK");     /* 하네스 우선 — 시스템 폴더 없이 검증 */
    if(env && *env) pak_path = env; }
  if(!pak_path || !*pak_path) return;
  f = fopen(pak_path, "rb");
  if(!f) return;                                 /* 팩 없음 = 조용히 비활성 */
  if(fread(magic, 1, 6, f) != 6 || memcmp(magic, "NGPV1", 6)){ fclose(f); return; }
  n = (int)rd_u32(f);
  if(n <= 0 || n > SFX_MAXCLIP * 64){ fclose(f); return; }
  hh = (unsigned *)malloc((size_t)n * 4);
  oo = (unsigned *)malloc((size_t)n * 4);
  ss = (unsigned *)malloc((size_t)n * 4);
  if(!hh || !oo || !ss){ free(hh); free(oo); free(ss); fclose(f); return; }
  for(i = 0; i < n; i++){ hh[i] = rd_u32(f); oo[i] = rd_u32(f); ss[i] = rd_u32(f); }
  /* 전량 상주 — 효과음 세트는 작다(P0 3클립 ≈ 80KB). 연타에서 디코드가 끼면 끊긴다. */
  for(i = 0; i < n && clip_n < SFX_MAXCLIP; i++){
    int len = 0;
    short *pcm = decode_one(f, oo[i], ss[i], &len);
    if(!pcm) continue;
    clip[clip_n].h = hh[i]; clip[clip_n].pcm = pcm; clip[clip_n].len = len; clip_n++;
  }
  free(hh); free(oo); free(ss); fclose(f);
  sfx_ready = clip_n > 0;
}

int  ss2sfx_on(void){ return sfx_ready && sfx_on_user; }
int  ss2sfx_count(void){ return clip_n; }
void ss2sfx_set_enabled(int on){ sfx_on_user = on ? 1 : 0; if(!sfx_on_user) ss2sfx_reset(); }
void ss2sfx_set_volume(int pct){
  if(pct < 0) pct = 0;
  if(pct > 150) pct = 150;
  sfx_vol = pct;
}

static const short *find_clip(const char *name, int *len){
  unsigned h = fnv1a(name);
  int i;
  for(i = 0; i < clip_n; i++) if(clip[i].h == h){ *len = clip[i].len; return clip[i].pcm; }
  return 0;
}

/* 결정적 난수 — 하네스 결과가 재현돼야 하므로 시계를 안 쓴다 */
static unsigned sfx_rnd(void){
  static unsigned s = 0x9E3779B9u;
  s = s * 1664525u + 1013904223u;
  return s >> 16;
}

/* 같은 세기에 여러 벌이 있으면 그중 하나를 고른다 — "sfx.hit.m" / "…m2" / "…m3".
   피치 흔들기만으로는 반복이 티가 난다. 벌이 하나뿐이면 그것만 쓴다(팩 호환). */
#define SFX_VAR 4
static const short *pick_variant(const char *base, int *len){
  const short *v[SFX_VAR]; int l[SFX_VAR], n = 0, i;
  char nm[48];
  for(i = 0; i < SFX_VAR; i++){
    const short *p;
    if(i == 0) snprintf(nm, sizeof nm, "%s", base);
    else       snprintf(nm, sizeof nm, "%s%d", base, i + 1);
    p = find_clip(nm, &l[n]);
    if(p){ v[n] = p; n++; }
  }
  if(!n) return 0;
  i = (int)(sfx_rnd() % (unsigned)n);
  *len = l[i];
  return v[i];
}

static void play(const char *name){
  int len = 0, i, pick = 0;
  const short *pcm;
  if(!ss2sfx_on()) return;
  pcm = pick_variant(name, &len);
  if(!pcm || len <= 0) return;
  for(i = 0; i < SFX_CH; i++) if(!ch[i].pcm){ pick = i; goto got; }
  /* 다 찼다 — 가장 오래 울린 자리를 **빼앗는다**. 버리면 그 타격은 소리가 없다. */
  for(i = 1; i < SFX_CH; i++) if(ch[i].age < ch[pick].age) pick = i;
got:
  ch[pick].pcm = pcm; ch[pick].len = len; ch[pick].pos = 0;
  /* 재생 속도 ±5% — 같은 소리가 연달아 나도 기계처럼 안 들리게 */
  ch[pick].step = 62259u + (sfx_rnd() % 6554u);      /* 0.95 ~ 1.05 (16.16) */
  ch[pick].age  = ++sfx_tick;
  sfx_started++;
}

void ss2sfx_hit(int dmg){
  /* 세기 경계는 기둥 흔들림(ss2comm 의 st_shk)이 쓰는 값과 **같게 둔다** —
     화면이 크게 흔들리는데 소리가 약하면 어색하다. */
  if(dmg >= 12)     play("sfx.hit.h");
  else if(dmg >= 4) play("sfx.hit.m");
  else if(dmg > 0)  play("sfx.hit.l");
}

void ss2sfx_mix(int16_t *buf, int frames){
  int i, c, any = 0;
  if(!ss2sfx_on()) return;
  for(c = 0; c < SFX_CH; c++) if(ch[c].pcm) any = 1;
  if(!any) return;
  for(i = 0; i < frames; i++){
    int l = buf[i*2], r = buf[i*2+1], v = 0;
    for(c = 0; c < SFX_CH; c++){
      int idx, s;
      if(!ch[c].pcm) continue;
      idx = (int)(ch[c].pos >> 16);
      if(idx >= ch[c].len){ ch[c].pcm = 0; continue; }
      s = ch[c].pcm[idx];
      if(idx < SFX_ATK)                s = s * (idx + 1) / SFX_ATK;          /* 시작 램프 */
      else if(ch[c].len - idx < SFX_REL) s = s * (ch[c].len - idx) / SFX_REL; /* 끝 램프 */
      v += s;
      ch[c].pos += ch[c].step;
    }
    v = v * sfx_vol / 100;
    l += v; r += v;                    /* 게임 소리는 그대로 — 효과음만 얹는다 */
    if(l >  32767) l =  32767;
    if(l < -32768) l = -32768;
    if(r >  32767) r =  32767;
    if(r < -32768) r = -32768;
    buf[i*2] = (int16_t)l; buf[i*2+1] = (int16_t)r;
  }
}
