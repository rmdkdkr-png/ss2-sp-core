/* fgrom — 실제 롬으로 프레임 생성 품질을 잰다 (롬은 저장소 밖에 둔다 — 절대 커밋하지 않는다).
 * 사용: fgrom <core.so> <rom> <workdir> <mode:off|interp|predict> <frames> <script> [dump_every] [metric_from]
 *   script: "s-e:BTN,..."  BTN 글자: U D L R A B S(option)  예) "200-205:S,400-405:A"
 *   매 합성 프레임: 앞 실제 N, 뒤 실제 N+1 과 비교해 3x3 이웃 어디에도 없는 색의 픽셀 수(=깨진 픽셀) 기록
 */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "libretro.h"
#define W 160
#define H 152
static const char *opt_framegen = "disabled", *opt_mode = "predict";
static float target_hz = 120.0f;
static char workdir[1024];
static uint16_t cur[W*H]; static int have_frame = 0;
static bool env_cb(unsigned cmd, void *data)
{
   switch (cmd)
   {
      case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: return *(enum retro_pixel_format*)data == RETRO_PIXEL_FORMAT_RGB565;
      case RETRO_ENVIRONMENT_GET_VARIABLE: {
         struct retro_variable *v = (struct retro_variable*)data;
         if (!strcmp(v->key, "ngp_framegen"))      { v->value = opt_framegen; return true; }
         if (!strcmp(v->key, "ngp_framegen_mode")) { v->value = opt_mode;     return true; }
         v->value = NULL; return false; }
      case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: *(bool*)data = false; return true;
      case RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE: *(float*)data = target_hz; return true;
      case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: return true;
      case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
      case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: *(const char**)data = workdir; return true;
      case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE: *(int*)data = 3; return true;
      case RETRO_ENVIRONMENT_GET_CAN_DUPE: *(bool*)data = true; return true;
      case RETRO_ENVIRONMENT_SET_MESSAGE: fprintf(stderr, "  [msg] %s\n", ((const struct retro_message*)data)->msg); return true;
      default: return false;
   }
}
static void video_cb(const void *data, unsigned w, unsigned h, size_t pitch)
{
   unsigned y; if (!data) return;
   for (y = 0; y < h && y < H; y++) memcpy(cur + y*W, (const uint8_t*)data + y*pitch, (w < W ? w : W) * 2);
   have_frame = 1;
}
static void audio_cb(int16_t l, int16_t r) { (void)l; (void)r; }
static size_t audio_batch_cb(const int16_t *d, size_t n) { (void)d; return n; }
static void input_poll_cb(void) {}
static unsigned btn_mask = 0; /* bit: 1<<id */
static int16_t input_state_cb(unsigned port, unsigned dev, unsigned idx, unsigned id)
{ (void)idx; if (port != 0 || dev != RETRO_DEVICE_JOYPAD) return 0; if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return (int16_t)btn_mask; if (id >= 16) return 0; return (btn_mask >> id) & 1; }

typedef void (*fv)(void);
static void *Hd;
static void (*p_set_environment)(retro_environment_t); static void (*p_set_video)(retro_video_refresh_t);
static void (*p_set_audio)(retro_audio_sample_t); static void (*p_set_audio_batch)(retro_audio_sample_batch_t);
static void (*p_set_input_poll)(retro_input_poll_t); static void (*p_set_input_state)(retro_input_state_t);
static fv p_init, p_deinit, p_run, p_unload; static bool (*p_load)(const struct retro_game_info*);
static void (*p_fg_state)(int*, int*, unsigned*);
static void *(*p_spriteram)(void); static void *(*p_spritecol)(void); static void *(*p_charram)(void); static FILE *oamf = NULL, *chrf = NULL, *csvf = NULL;
static size_t (*p_ser_size)(void); static bool (*p_ser)(void*, size_t); static bool (*p_unser)(const void*, size_t);
#define SYM(v, n) do { v = dlsym(Hd, n); if (!v) { fprintf(stderr, "dlsym %s\n", n); exit(2); } } while (0)

typedef struct { int s, e; unsigned mask; } seg;
static seg segs[256]; static int nseg = 0;
static unsigned btn_of(char c)
{
   switch (c) { case 'U': return 1u<<RETRO_DEVICE_ID_JOYPAD_UP; case 'D': return 1u<<RETRO_DEVICE_ID_JOYPAD_DOWN;
      case 'L': return 1u<<RETRO_DEVICE_ID_JOYPAD_LEFT; case 'R': return 1u<<RETRO_DEVICE_ID_JOYPAD_RIGHT;
      case 'A': return 1u<<RETRO_DEVICE_ID_JOYPAD_B; case 'B': return 1u<<RETRO_DEVICE_ID_JOYPAD_A;
      case 'S': return 1u<<RETRO_DEVICE_ID_JOYPAD_START; default: return 0; }
}
static void parse_script(const char *sc)
{
   char buf[4096]; char *tok, *save; strncpy(buf, sc, sizeof buf - 1); buf[sizeof buf - 1] = 0;
   for (tok = strtok_r(buf, ",", &save); tok && nseg < 256; tok = strtok_r(NULL, ",", &save))
   {
      int s, e; char b[16] = {0}; if (sscanf(tok, "%d-%d:%15s", &s, &e, b) != 3) continue;
      segs[nseg].s = s; segs[nseg].e = e; segs[nseg].mask = 0;
      { const char *c; for (c = b; *c; c++) segs[nseg].mask |= btn_of(*c); }
      nseg++;
   }
}
static unsigned mask_at(int f) { int i; unsigned m = 0; for (i = 0; i < nseg; i++) if (f >= segs[i].s && f <= segs[i].e) m |= segs[i].mask; return m; }

static void dump(const char *name, const uint16_t *fr)
{ char p[1200]; FILE *f; snprintf(p, sizeof p, "%s/%s.565", workdir, name); f = fopen(p, "wb"); if (f) { fwrite(fr, 2, W*H, f); fclose(f); } }

static int near_has(const uint16_t *fr, int x, int y, uint16_t c)
{  int dx, dy; for (dy = -1; dy <= 1; dy++) for (dx = -1; dx <= 1; dx++) { int xx = x+dx, yy = y+dy; if (xx < 0 || yy < 0 || xx >= W || yy >= H) continue; if (fr[yy*W+xx] == c) return 1; } return 0; }
/* 합성 S 를 앞 실제 A, 뒤 실제 B 와 비교: 어느 쪽 3x3 이웃에도 없는 색의 픽셀 수 */
static int artifacts(const uint16_t *S, const uint16_t *A, const uint16_t *B)
{  int x, y, n = 0; for (y = 0; y < H; y++) for (x = 0; x < W; x++) { uint16_t c = S[y*W+x]; if (c == A[y*W+x] || c == B[y*W+x]) continue; if (!near_has(A, x, y, c) && !near_has(B, x, y, c)) n++; } return n; }
static int differ(const uint16_t *A, const uint16_t *B) { int i, n = 0; for (i = 0; i < W*H; i++) if (A[i] != B[i]) n++; return n; }

int main(int argc, char **argv)
{
   const char *mode; int frames, dump_every = 0, metric_from = 0, real = 0, calls = 0;
   static uint16_t R0[W*H], R1[W*H], Sy[W*H]; int haveS = 0, haveR0 = 0, nS = 0; long sumA = 0; int maxA = 0, maxAt = -1, bad = 0; long sumD = 0;
   struct retro_game_info gi;
   if (argc < 7) { fprintf(stderr, "사용: fgrom <core.so> <rom> <workdir> <off|interp|predict> <frames> <script> [dump_every] [metric_from]\n"); return 2; }
   snprintf(workdir, sizeof workdir, "%s", argv[3]);
   mode = argv[4]; frames = atoi(argv[5]); parse_script(argv[6]);
   if (argc > 7) dump_every = atoi(argv[7]); if (argc > 8) metric_from = atoi(argv[8]);
   if (!strcmp(mode, "off")) opt_framegen = "disabled"; else { opt_framegen = "enabled"; opt_mode = mode; }
   Hd = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL); if (!Hd) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 2; }
   SYM(p_set_environment, "retro_set_environment"); SYM(p_set_video, "retro_set_video_refresh");
   SYM(p_set_audio, "retro_set_audio_sample"); SYM(p_set_audio_batch, "retro_set_audio_sample_batch");
   SYM(p_set_input_poll, "retro_set_input_poll"); SYM(p_set_input_state, "retro_set_input_state");
   SYM(p_init, "retro_init"); SYM(p_deinit, "retro_deinit"); SYM(p_run, "retro_run"); SYM(p_unload, "retro_unload_game"); SYM(p_load, "retro_load_game");
   SYM(p_fg_state, "retro_ngp_fg_state"); SYM(p_spriteram, "retro_ngp_spriteram"); SYM(p_spritecol, "retro_ngp_spritecol"); SYM(p_charram, "retro_ngp_charram"); SYM(p_ser_size, "retro_serialize_size"); SYM(p_ser, "retro_serialize"); SYM(p_unser, "retro_unserialize");
   p_set_environment(env_cb); p_set_video(video_cb); p_set_audio(audio_cb); p_set_audio_batch(audio_batch_cb);
   p_set_input_poll(input_poll_cb); p_set_input_state(input_state_cb);
   memset(&gi, 0, sizeof gi); gi.path = argv[2];
   p_init(); if (!p_load(&gi)) { fprintf(stderr, "load 실패\n"); return 2; }
   { const char *o = getenv("FGROM_OAM"); if (o) oamf = fopen(o, "wb"); o = getenv("FGROM_CHR"); if (o) chrf = fopen(o, "wb"); o = getenv("FGROM_CSV"); if (o) csvf = fopen(o, "w"); }
   { const char *st = getenv("FGROM_LOAD"); if (st) { FILE *f = fopen(st, "rb"); if (f) { size_t n = p_ser_size(); void *b = malloc(n); size_t got = fread(b, 1, n, f); fclose(f); if (!p_unser(b, got)) fprintf(stderr, "unserialize 실패\n"); else fprintf(stderr, "상태 불러옴 %s (%zu)\n", st, got); free(b); } } }
   while (real < frames)
   {
      int act, ph, is_synth;
      btn_mask = mask_at(real);
      /* 위상 0 = 에뮬레이션이 도는 호출, 1 = 안 도는 호출. 예측 모드는 0 에 실제 N·1 에 예측 N+½ 을 내지만,
         보간 모드는 0 에 중간(N-1→N)·1 에 실제 N 을 낸다(N 을 한 호출 미룸) — 합성 프레임 판정을 모드에 맞춘다 */
      p_fg_state(&act, &ph, 0); is_synth = act && ph == (!strcmp(mode, "interp") ? 0 : 1);
      have_frame = 0; p_run(); calls++;
      if (!have_frame) continue;
      if (is_synth) { memcpy(Sy, cur, sizeof Sy); haveS = 1; continue; }
      /* 실제 프레임 */
      if (haveR0) { memcpy(R1, cur, sizeof R1);
         if (haveS && real >= metric_from) { int a = artifacts(Sy, R0, R1); int d = differ(R0, R1); nS++; if (csvf) fprintf(csvf, "%d,%d,%d\n", real, a, d); { const char *pk = getenv("FGROM_PICK"); if (pk && atoi(pk) == real) { dump("pick_S", Sy); dump("pick_A", R0); dump("pick_B", R1); } } sumA += a; sumD += d; if (a > maxA) { maxA = a; maxAt = real; dump("worst_S", Sy); dump("worst_A", R0); dump("worst_B", R1); } if (a > 40) bad++; }
         memcpy(R0, R1, sizeof R0); }
      else { memcpy(R0, cur, sizeof R0); haveR0 = 1; }
      haveS = 0;
      if (oamf) { fwrite(p_spriteram(), 1, 256, oamf); fwrite(p_spritecol(), 1, 64, oamf); }
      if (chrf) fwrite(p_charram(), 1, 8192, chrf);
      if (dump_every && real % dump_every == 0) { char nm[64]; snprintf(nm, sizeof nm, "f%06d", real); dump(nm, cur); }
      real++;
   }
   { const char *st = getenv("FGROM_SAVE"); if (st) { FILE *f = fopen(st, "wb"); size_t n = p_ser_size(); void *b = malloc(n); if (f && p_ser(b, n)) { fwrite(b, 1, n, f); fprintf(stderr, "상태 저장 %s (%zu)\n", st, n); } if (f) fclose(f); free(b); } }
   printf("mode=%s real=%d calls=%d synth_measured=%d artifacts: mean=%.2f max=%d (at real %d) frames>40px=%d  mean|A-B|=%.1f\n",
          mode, real, calls, nS, nS ? (double)sumA / nS : 0.0, maxA, maxAt, bad, nS ? (double)sumD / nS : 0.0);
   if (oamf) fclose(oamf); if (chrf) fclose(chrf); if (csvf) fclose(csvf);
   p_unload(); p_deinit();
   return 0;
}
