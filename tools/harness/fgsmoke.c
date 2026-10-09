/* fgsmoke — 프레임 생성 배관 스모크 시험. 롬 없이 돈다:
 * 헤더만 갖춘 합성 롬(엔트리에서 제자리 점프)을 코어에 물려 retro_run 을 돌리며
 *   · 목표 주사율 60 → 2배 출력이 켜지지 않는다 (fps 60.25 유지, 호출당 ~732 샘플)
 *   · 목표 주사율 120 → 30 프레임 뒤 SET_SYSTEM_AV_INFO(120.5) 한 번, 이후 호출당 ~366 샘플, 매 호출 비디오
 *   · 예측 모드: 실제 프레임 N회 뒤 상태가 2배 출력 끈 채 N회 돌린 상태와 바이트 단위로 같다 (복원 완전성)
 *   · 보간 모드: 돌아간다, 오디오 분배 같음
 *   · 런어헤드 흉내(저장 문맥=1 + 매 프레임 serialize): 자동 모드가 켜지지 않는다
 *   · 숨은 호출(비디오 꺼짐) 섞여도 죽지 않는다
 *
 * 빌드 (build/ 에서):  gcc -O1 -std=gnu99 -Ilibretro-common/include -o fgsmoke ../tools/harness/fgsmoke.c -ldl
 * 실행:                 ./fgsmoke ./mednafen_ngp_libretro.so <작업폴더>
 */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "libretro.h"

static const char *opt_framegen = "auto", *opt_mode = "predict";
static float target_hz = 60.0f;
static int   av_enable = 3, ss_ctx = 0, var_updated = 0, verbose = 0;
static double last_fps = 0; static int avinfo_calls = 0;
static int video_calls = 0; static unsigned last_w, last_h;
static size_t audio_last = 0, audio_total = 0; static int audio_calls = 0;
static char workdir[1024];

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
      case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: *(bool*)data = var_updated != 0; var_updated = 0; return true;
      case RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE: *(float*)data = target_hz; return true;
      case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: {
         const struct retro_system_av_info *a = (const struct retro_system_av_info*)data;
         last_fps = a->timing.fps; avinfo_calls++;
         if (verbose) printf("  [av_info] fps=%.3f %ux%u\n", a->timing.fps, a->geometry.base_width, a->geometry.base_height);
         return true; }
      case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
      case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: *(const char**)data = workdir; return true;
      case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE: *(int*)data = av_enable; return true;
      case RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT: *(int*)data = ss_ctx; return true;
      case RETRO_ENVIRONMENT_SET_MESSAGE: if (verbose) printf("  [msg] %s\n", ((const struct retro_message*)data)->msg); return true;
      case RETRO_ENVIRONMENT_GET_CAN_DUPE: *(bool*)data = true; return true;
      default: return false;
   }
}
static void video_cb(const void *data, unsigned w, unsigned h, size_t pitch) { (void)data; (void)pitch; video_calls++; last_w = w; last_h = h; }
static void audio_cb(int16_t l, int16_t r) { (void)l; (void)r; audio_total++; }
static size_t audio_batch_cb(const int16_t *d, size_t frames) { (void)d; audio_last = frames; audio_total += frames; audio_calls++; return frames; }
static void input_poll_cb(void) {}
static int16_t input_state_cb(unsigned port, unsigned dev, unsigned idx, unsigned id) { (void)port; (void)dev; (void)idx; (void)id; return 0; }

typedef void (*fv)(void);
static void *H;
static void (*p_set_environment)(retro_environment_t);
static void (*p_set_video)(retro_video_refresh_t);
static void (*p_set_audio)(retro_audio_sample_t);
static void (*p_set_audio_batch)(retro_audio_sample_batch_t);
static void (*p_set_input_poll)(retro_input_poll_t);
static void (*p_set_input_state)(retro_input_state_t);
static fv p_init, p_deinit, p_run, p_reset, p_unload;
static bool (*p_load)(const struct retro_game_info*);
static size_t (*p_ser_size)(void);
static bool (*p_ser)(void*, size_t);
static void (*p_fg_state)(int*, int*, unsigned*);
static void (*p_fg_set_clock)(double (*)(void));
static void (*p_get_av)(struct retro_system_av_info*);

#define SYM(v, n) do { v = dlsym(H, n); if (!v) { fprintf(stderr, "dlsym %s 실패\n", n); exit(2); } } while (0)

static void core_open(const char *so)
{
   H = dlopen(so, RTLD_NOW | RTLD_LOCAL);
   if (!H) { fprintf(stderr, "dlopen: %s\n", dlerror()); exit(2); }
   SYM(p_set_environment, "retro_set_environment"); SYM(p_set_video, "retro_set_video_refresh");
   SYM(p_set_audio, "retro_set_audio_sample"); SYM(p_set_audio_batch, "retro_set_audio_sample_batch");
   SYM(p_set_input_poll, "retro_set_input_poll"); SYM(p_set_input_state, "retro_set_input_state");
   SYM(p_init, "retro_init"); SYM(p_deinit, "retro_deinit"); SYM(p_run, "retro_run"); SYM(p_reset, "retro_reset");
   SYM(p_unload, "retro_unload_game"); SYM(p_load, "retro_load_game");
   SYM(p_ser_size, "retro_serialize_size"); SYM(p_ser, "retro_serialize"); SYM(p_fg_state, "retro_ngp_fg_state"); SYM(p_fg_set_clock, "retro_ngp_fg_set_clock"); SYM(p_get_av, "retro_get_system_av_info");
   p_set_environment(env_cb); p_set_video(video_cb); p_set_audio(audio_cb); p_set_audio_batch(audio_batch_cb);
   p_set_input_poll(input_poll_cb); p_set_input_state(input_state_cb);
}

/* 합성 롬: 64KB, 헤더 + 엔트리(0x200040)에서 제자리 점프(JR T,-2 = 68 FE) */
static void make_rom(const char *path)
{
   static uint8_t rom[65536];
   FILE *f;
   memset(rom, 0xFF, sizeof rom);
   memcpy(rom, "COPYRIGHT BY SNK CORPORATION", 28);
   rom[0x1C] = 0x40; rom[0x1D] = 0x00; rom[0x1E] = 0x20; rom[0x1F] = 0x00;   /* startPC = 0x00200040 */
   rom[0x20] = 0x34; rom[0x21] = 0x12; rom[0x22] = 0x01;                      /* catalog/sub */
   rom[0x23] = 0x10;                                                          /* NGPC 컬러 */
   memcpy(rom + 0x24, "FGSMOKE     ", 12);
   rom[0x40] = 0x68; rom[0x41] = 0xFE;
   f = fopen(path, "wb"); if (!f) { perror(path); exit(2); }
   fwrite(rom, 1, sizeof rom, f); fclose(f);
}

static int fails = 0, tests = 0;
#define CHECK(c, ...) do { tests++; if (!(c)) { fails++; printf("  FAIL %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void load_game(const char *rom)
{
   struct retro_game_info gi; memset(&gi, 0, sizeof gi);
   gi.path = rom;
   p_init();
   if (!p_load(&gi)) { fprintf(stderr, "retro_load_game 실패\n"); exit(2); }
   video_calls = 0; audio_calls = 0; audio_total = 0; avinfo_calls = 0; last_fps = 0;
}
static void unload_game(void) { p_unload(); p_deinit(); }

static int real_frames_run(int want_real)
{  /* 실제 프레임을 want_real 번 돌릴 때까지 호출 — 합성 호출은 세지 않는다 */
   int real = 0, calls = 0;
   while (real < want_real)
   {
      int act, ph; p_fg_state(&act, &ph, 0);
      int is_real = !(act && ph == 1);
      p_run(); calls++;
      if (is_real) real++;
      if (calls > want_real * 3 + 100) { printf("  (루프 보호) "); break; }
   }
   return calls;
}

static void test_60(const char *rom)
{
   int i, f0 = fails; size_t a[8];
   target_hz = 60; opt_framegen = "auto"; opt_mode = "predict";
   load_game(rom);
   for (i = 0; i < 200; i++) { p_run(); if (i >= 192) a[i - 192] = audio_last; }
   { int act, ph; p_fg_state(&act, &ph, 0);
     CHECK(act == 0, "60Hz 목표인데 2배 출력이 켜졌다");
     CHECK(avinfo_calls == 0, "60Hz 목표인데 SET_SYSTEM_AV_INFO 가 %d번 불렸다", avinfo_calls); }
   CHECK(video_calls == 200, "비디오 호출 %d (200 기대)", video_calls);
   for (i = 0; i < 8; i++) CHECK(a[i] >= 700 && a[i] <= 760, "호출당 오디오 %zu (약 732 기대)", a[i]);
   unload_game();
   printf("1 60Hz 목표 → 끔 유지: %s\n", fails == f0 ? "통과" : "실패");
}

static void test_120(const char *rom, const char *mode)
{
   int i, f0 = fails; size_t a[8]; struct retro_system_av_info av;
   target_hz = 120; opt_framegen = "auto"; opt_mode = mode;
   load_game(rom);
   p_get_av(&av);   /* 로드 때 바로 판정 → 처음부터 120.5, 재초기화 없음 */
   CHECK(av.timing.fps > 120.4 && av.timing.fps < 120.6, "로드 직후 av_info fps %.3f (120.5 기대)", av.timing.fps);
   for (i = 0; i < 300; i++)
   {
      p_run();
      if (i >= 292) a[i - 292] = audio_last;
   }
   { int act, ph; p_fg_state(&act, &ph, 0);
     CHECK(act == 1, "120Hz 목표인데 2배 출력이 안 켜졌다");
     CHECK(avinfo_calls == 0, "SET_SYSTEM_AV_INFO %d번 (0 기대 — 로드 때 이미 120.5)", avinfo_calls); }
   CHECK(video_calls == 300, "비디오 호출 %d (300 기대 — 합성 호출도 매번)", video_calls);
   CHECK(last_w == 160 && last_h >= 152, "출력 크기 %ux%u", last_w, last_h);
   for (i = 0; i < 8; i++) CHECK(a[i] >= 340 && a[i] <= 400, "2배 출력 중 호출당 오디오 %zu (약 366 기대)", a[i]);
   /* 옵션을 끄면 다시 60.25 로 */
   opt_framegen = "disabled"; var_updated = 1;
   for (i = 0; i < 80; i++) p_run();   /* 판정은 실제 프레임마다(2호출당 1회) → 30 결정 = 60 호출 */
   { int act, ph; p_fg_state(&act, &ph, 0);
     CHECK(act == 0 && avinfo_calls == 1 && last_fps > 60 && last_fps < 61, "옵션 끔 뒤 복귀 실패 (act %d, av %d, fps %.2f)", act, avinfo_calls, last_fps); }
   unload_game();
   printf("2 120Hz 목표 → 켜짐·오디오 분배·복귀 (%s): %s\n", mode, fails == f0 ? "통과" : "실패");
}

static void test_determinism(const char *rom, const char *mode)
{
   int f0 = fails; size_t n0, n1; uint8_t *s0, *s1;
   /* A: 2배 출력 없이 400 실제 프레임 */
   target_hz = 60; opt_framegen = "disabled"; opt_mode = mode;
   load_game(rom);
   real_frames_run(400);
   n0 = p_ser_size(); s0 = malloc(n0); CHECK(p_ser(s0, n0), "serialize A");
   unload_game();
   /* B: 120Hz 자동(30 프레임 뒤 켜짐) 으로 400 실제 프레임 */
   target_hz = 120; opt_framegen = "auto"; opt_mode = mode;
   load_game(rom);
   { int calls = real_frames_run(400); int act, ph; p_fg_state(&act, &ph, 0);
     CHECK(act == 1, "B 에서 2배 출력이 안 켜졌다");
     CHECK(calls == 799, "B 호출 수 %d (실제 400 + 합성 399 = 799 기대 — 400번째 실제 프레임에서 멈춘다)", calls); }
   n1 = p_ser_size(); s1 = malloc(n1); CHECK(p_ser(s1, n1), "serialize B");
   unload_game();
   CHECK(n0 == n1, "상태 크기 다름 %zu vs %zu", n0, n1);
   if (n0 == n1)
   {
      size_t i, d = (size_t)-1;
      for (i = 0; i < n0; i++) if (s0[i] != s1[i]) { d = i; break; }
      CHECK(d == (size_t)-1, "상태가 다르다 — 첫 차이 오프셋 %zu (0x%zx): %02x vs %02x", d, d, s0[d], s1[d]);
   }
   free(s0); free(s1);
   printf("3 결정성 (%s: 400 실제 프레임 뒤 상태 동일): %s\n", mode, fails == f0 ? "통과" : "실패");
}

static void test_runahead_guard(const char *rom)
{
   int i, f0 = fails; size_t n; uint8_t *s;
   target_hz = 120; opt_framegen = "auto"; opt_mode = "predict"; ss_ctx = 1;
   load_game(rom);
   n = p_ser_size(); s = malloc(n);
   for (i = 0; i < 200; i++) { p_run(); p_ser(s, n); }   /* 런어헤드처럼 매 프레임 저장 */
   { int act, ph; p_fg_state(&act, &ph, 0);
     CHECK(act == 0, "런어헤드 감지인데 2배 출력이 안 꺼졌다 (act %d av %d)", act, avinfo_calls); }
   ss_ctx = 0; free(s);
   /* 런어헤드가 멈추면 2초 뒤 켜진다 */
   for (i = 0; i < 300; i++) p_run();
   { int act, ph; p_fg_state(&act, &ph, 0);
     CHECK(act == 1, "런어헤드 중단 뒤에도 안 켜진다"); }
   unload_game();
   printf("4 런어헤드 감지 → 자동 끔: %s\n", fails == f0 ? "통과" : "실패");
}

static void test_hidden(const char *rom)
{
   int i, f0 = fails;
   target_hz = 120; opt_framegen = "enabled"; opt_mode = "predict";
   load_game(rom);
   for (i = 0; i < 100; i++) p_run();
   { int act, ph; p_fg_state(&act, &ph, 0); CHECK(act == 1, "강제 켬인데 안 켜짐"); }
   for (i = 0; i < 100; i++) { av_enable = (i % 3 == 0) ? 0 : 3; p_run(); }
   av_enable = 3;
   for (i = 0; i < 20; i++) p_run();
   CHECK(video_calls == 220, "비디오 호출 %d", video_calls);
   unload_game();
   printf("5 숨은 호출 섞임: %s\n", fails == f0 ? "통과" : "실패");
}

#include <time.h>
static void nap_ms(int ms) { struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L }; nanosleep(&ts, 0); }

/* 자동 모드 + 숨은 호출(런어헤드 2-인스턴스처럼 저장은 안 오고 비디오만 꺼짐) → 바로 꺼지고, 게임은 2배속이 아니다 */
static void test_hidden_auto(const char *rom)
{
   int i, f0 = fails; int act, ph; unsigned fr0, fr1;
   target_hz = 120; opt_framegen = "auto"; opt_mode = "predict";
   load_game(rom);
   for (i = 0; i < 10; i++) p_run();
   p_fg_state(&act, &ph, 0); CHECK(act == 1, "시작 시 켜져 있어야");
   for (i = 0; i < 6; i++) { av_enable = 0; p_run(); }
   av_enable = 3;
   p_fg_state(&act, &ph, &fr0);
   CHECK(act == 0, "숨은 호출 뒤 자동 모드가 바로 꺼져야 (act %d)", act);
   CHECK(avinfo_calls == 1 && last_fps > 60 && last_fps < 61, "60.25 로 복귀 (av %d fps %.2f)", avinfo_calls, last_fps);
   /* 2초(240호출) 안엔 안 켜지고, 그 뒤 30 프레임 히스테리시스 지나 다시 켜진다 */
   for (i = 0; i < 200; i++) p_run();
   p_fg_state(&act, &ph, 0); CHECK(act == 0, "2초 안엔 다시 안 켜져야");
   for (i = 0; i < 120; i++) p_run();
   p_fg_state(&act, &ph, &fr1); CHECK(act == 1, "숨은 호출이 멈추면 다시 켜져야");
   unload_game();
   printf("6 숨은 호출 → 자동 끔·복귀: %s\n", fails == f0 ? "통과" : "실패");
}

/* 호출 속도 감시: 설정은 120 인데 프론트가 60/s 로만 부르면(패널이 60Hz) 2초 뒤 끄고, 옵션을 만지면 다시 판정 */
static void test_watchdog(const char *rom)
{
   int i, f0 = fails; int act, ph;
   target_hz = 120; opt_framegen = "auto"; opt_mode = "predict";
   load_game(rom);
   for (i = 0; i < 200; i++) { p_run(); nap_ms(16); }   /* 약 3.3초 동안 60/s */
   p_fg_state(&act, &ph, 0);
   CHECK(act == 0, "60/s 로 3초 돌았는데 안 꺼졌다 (act %d)", act);
   CHECK(last_fps > 60 && last_fps < 61, "복귀 fps %.2f", last_fps);
   for (i = 0; i < 100; i++) p_run();                  /* 빠르게 돌아도 판정은 유지 */
   p_fg_state(&act, &ph, 0); CHECK(act == 0, "판정은 옵션을 만질 때까지 유지돼야");
   var_updated = 1;                                     /* 옵션 재적용 → 판정 해제 → 30 프레임 뒤 켜짐 */
   for (i = 0; i < 60; i++) p_run();
   p_fg_state(&act, &ph, 0); CHECK(act == 1, "옵션을 만진 뒤 다시 켜져야 (act %d)", act);
   unload_game();
   printf("7 호출 속도 감시: %s\n", fails == f0 ? "통과" : "실패");
}

/* 차단 재시도 — 시계를 주입해 결정적으로: 1·2·3 차 차단 뒤 10·20·40 초에 재시도, 4 차는 옵션/재로드까지,
   120/s 가 30 초 이어지면 횟수 복원, 목표가 60 인 동안엔 재시도해도 안 켜지고 돌아오면 켜진다 */
static double fake_t = 1000.0;
static double fake_clock(void) { return fake_t; }
static void run_at(int n, double dt) { while (n-- > 0) { fake_t += dt; p_run(); } }
static double run_until(int want, double dt, double maxsec)
{
   double t0 = fake_t; int act, ph;
   for (;;)
   {
      fake_t += dt; p_run(); p_fg_state(&act, &ph, 0);
      if (act == want) return fake_t - t0;
      if (fake_t - t0 > maxsec) return -1;
   }
}
static void test_retry(const char *rom)
{
   int f0 = fails; int act, ph; double e, tb;
   const double SLOW = 1.0 / 60, FAST = 1.0 / 120;
   target_hz = 120; opt_framegen = "auto"; opt_mode = "predict";
   load_game(rom);
   p_fg_set_clock(fake_clock);
   p_fg_state(&act, &ph, 0); CHECK(act == 1, "시작 시 켜져 있어야");
   e = run_until(0, SLOW, 5); CHECK(e > 1.5 && e < 3.5, "1차 차단 %.2fs", e); tb = fake_t;
   run_at((int)(9.0 / SLOW), SLOW); p_fg_state(&act, &ph, 0); CHECK(act == 0, "10초 전엔 재시도 없어야");
   e = run_until(1, SLOW, 3); CHECK(e >= 0 && fake_t - tb > 10 && fake_t - tb < 12.5, "1차 재시도 복귀 (차단 뒤 %.2fs)", fake_t - tb);
   e = run_until(0, SLOW, 5); CHECK(e > 1.5 && e < 3.5, "2차 차단 %.2fs", e); tb = fake_t;
   run_at((int)(19.0 / SLOW), SLOW); p_fg_state(&act, &ph, 0); CHECK(act == 0, "20초 전엔 재시도 없어야");
   e = run_until(1, SLOW, 3); CHECK(e >= 0 && fake_t - tb > 20 && fake_t - tb < 22.5, "2차 재시도 복귀 (%.2fs)", fake_t - tb);
   e = run_until(0, SLOW, 5); CHECK(e > 1.5 && e < 3.5, "3차 차단 %.2fs", e); tb = fake_t;
   run_at((int)(39.0 / SLOW), SLOW); p_fg_state(&act, &ph, 0); CHECK(act == 0, "40초 전엔 재시도 없어야");
   e = run_until(1, SLOW, 3); CHECK(e >= 0 && fake_t - tb > 40 && fake_t - tb < 42.5, "3차 재시도 복귀 (%.2fs)", fake_t - tb);
   e = run_until(0, SLOW, 5); CHECK(e > 1.5 && e < 3.5, "4차 차단 %.2fs", e); tb = fake_t;
   { int av0 = avinfo_calls;                             /* 한 점 표본이 아니라 200초 내내 꺼져 있어야(80초 뒤 재시도하는 변종도 잡는다) */
     e = run_until(1, SLOW, 200.0);
     CHECK(e < 0 && avinfo_calls == av0, "4차 뒤엔 200초 내내 꺼져 있어야 (켜짐 %.1fs, av %d→%d)", e, av0, avinfo_calls); }
   var_updated = 1; e = run_until(1, FAST, 2); CHECK(e >= 0, "옵션 재적용 뒤 복귀");
   run_at((int)(31.0 / FAST), FAST); p_fg_state(&act, &ph, 0); CHECK(act == 1, "120/s 31초 동안 켜져 있어야");
   e = run_until(0, SLOW, 5); CHECK(e > 1.5 && e < 3.5, "복원 뒤 차단 %.2fs", e); tb = fake_t;
   e = run_until(1, SLOW, 13); CHECK(e >= 0 && fake_t - tb > 10 && fake_t - tb < 12.5, "복원 뒤 다시 10초 재시도 (%.2fs)", fake_t - tb);
   run_at((int)(31.0 / FAST), FAST); p_fg_state(&act, &ph, 0); CHECK(act == 1, "다시 31초 건강");
   e = run_until(0, SLOW, 5); CHECK(e > 1.5 && e < 3.5, "차단 %.2fs", e); tb = fake_t;
   target_hz = 60; run_at((int)(30.0 / SLOW), SLOW); p_fg_state(&act, &ph, 0); CHECK(act == 0, "목표 60 이면 재시도해도 안 켜져야");
   target_hz = 120; e = run_until(1, SLOW, 3); CHECK(e >= 0, "목표가 120 으로 돌아오면 켜져야 (%.2f)", e);
   e = run_until(0, SLOW, 5); CHECK(e > 1.5 && e < 3.5, "차단 %.2fs", e); tb = fake_t;
   run_at((int)(19.0 / SLOW), SLOW); p_fg_state(&act, &ph, 0); CHECK(act == 0, "2차니 20초 전엔 재시도 없어야");
   e = run_until(1, SLOW, 3); CHECK(e >= 0 && fake_t - tb > 20 && fake_t - tb < 22.5, "2차 20초 복귀 (%.2fs)", fake_t - tb);
   p_fg_set_clock(NULL);
   unload_game();
   printf("8 차단 재시도(10·20·40초, 4차 영구, 30초 건강 복원): %s\n", fails == f0 ? "통과" : "실패");
}

/* '켬'(강제)도 차단을 따른다 — 안 그러면 2.5초마다 껐다 켰다 한다. 재시도 일정은 같다 */
static void test_retry_forced(const char *rom)
{
   int f0 = fails; int act, ph; double e, tb; int av0;
   const double SLOW = 1.0 / 60;
   target_hz = 60; opt_framegen = "enabled"; opt_mode = "predict";
   load_game(rom);
   p_fg_set_clock(fake_clock);
   p_fg_state(&act, &ph, 0); CHECK(act == 1, "강제 켬은 목표 60 이어도 켜져야");
   e = run_until(0, SLOW, 5); CHECK(e > 1.5 && e < 3.5, "강제 켬 1차 차단 %.2fs", e); tb = fake_t; av0 = avinfo_calls;
   run_at((int)(9.0 / SLOW), SLOW); p_fg_state(&act, &ph, 0);
   CHECK(act == 0 && avinfo_calls == av0, "강제 켬이 차단 중 다시 켜지면 안 된다 (act %d, av %d→%d)", act, av0, avinfo_calls);
   e = run_until(1, SLOW, 3); CHECK(e >= 0 && fake_t - tb > 10 && fake_t - tb < 12.5, "강제 켬 10초 재시도 (%.2fs)", fake_t - tb);
   p_fg_set_clock(NULL);
   unload_game();
   printf("9 강제 켬도 차단·재시도를 따름: %s\n", fails == f0 ? "통과" : "실패");
}

int main(int argc, char **argv)
{
   char rom[1200];
   if (argc < 3) { fprintf(stderr, "사용: fgsmoke <core.so> <workdir>\n"); return 2; }
   verbose = getenv("FGSMOKE_V") != NULL;
   snprintf(workdir, sizeof workdir, "%s", argv[2]);
   snprintf(rom, sizeof rom, "%s/fgsmoke.ngc", workdir);
   make_rom(rom);
   core_open(argv[1]);
   test_60(rom);
   test_120(rom, "predict");
   test_120(rom, "interp");
   test_determinism(rom, "predict");
   test_determinism(rom, "interp");
   test_runahead_guard(rom);
   test_hidden(rom);
   test_hidden_auto(rom);
   test_watchdog(rom);
   test_retry(rom);
   test_retry_forced(rom);
   printf("%d 검사 중 %d 실패\n", tests, fails);
   return fails ? 1 : 0;
}
