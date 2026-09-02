/* ngprun — 한글패치 화면 검증용 libretro 하네스
 *
 * 기존 svcrun 과 다른 점
 *   - retro_set_controller_port_device 를 부른다. 이걸 빼면 코어가 패드를 안 붙여
 *     타이틀 화면 입력이 통째로 무시된다 (PocketCore native.c 를 보고 알아냄).
 *   - 픽셀 포맷을 협상하고 실제 포맷대로 덤프한다.
 *   - 코어 옵션 기본값을 넘겨준다 (RetroArch 없이 돌리므로 직접 시딩).
 *
 * 사용: ngprun <core.so> <rom> <script> [출력접두사]
* 스크립트 한 줄: "<프레임> <버튼…>"  또는  "!<태그>" / "!save f" / "!load f" / "!vram <태그>"
 * 버튼: U D L R A B X Y ST SE L1 R1   (NGP A = 레트로 B)
 */
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ENV_GET_CAN_DUPE        3
#define ENV_SET_PIXEL_FORMAT   10
#define ENV_GET_SYSTEM_DIR      9
#define ENV_GET_VARIABLE       15
#define ENV_SET_VARIABLES      16
#define ENV_GET_VARIABLE_UPDATE 17
#define ENV_GET_SAVE_DIR       31

#define RETRO_DEVICE_JOYPAD 1

struct retro_variable { const char *key; const char *value; };
struct retro_game_info { const char *path; const void *data; size_t size; const char *meta; };
struct retro_av_info {
  struct { unsigned bw, bh, mw, mh; float ar; } g;
  struct { double fps, sr; } t;
};

static unsigned  W, H;
static uint16_t  FB[512 * 512];
static int       PIXFMT = 1;          /* 0=0RGB1555 1=XRGB8888 2=RGB565 */
static uint32_t  FB32[512 * 512];
static int16_t   PAD;
static char      PREFIX[128] = "shot";

/* 코어 옵션 기본값 — RetroArch 없이 돌리므로 여기서 먹인다.
   ★ 월화 검수용: 손에 있는 NGP 코어들은 전부 SS2 더빙 빌드라 해설 오버레이가
   다른 게임에도 붙는다 (화면이 288x184 나 160x182 로 커지고 자막이 얹힌다).
   전부 꺼야 순정 160x152 화면이 나온다. */
static const struct retro_variable DEFAULTS[] = {
  /* NGPC 는 **본체 BIOS 의 언어 설정**을 게임이 읽는다. 게임 안에는 언어 항목이 없다.
     KOF R-2 는 이 값에 따라 일본어/영어 문자열과 폰트를 갈아 쓴다.
     NGP_LANG 환경변수로 바꾼다 (기본 japanese = 실기 초기값). */
  { "ngp_language",        "japanese" },
  { "ngp_svcsp_engine",    "disabled" },
  { "ngp_ss2sp",           "disabled" },
  { "ngp_ss2sp_comm",      "disabled" },
  { "ngp_ss2sp_comm_draw", "disabled" },
  { "ngp_ss2sp_comm_duo",  "disabled" },
  { "ngp_ss2sp_comm_spk",  "haohmaru" },
  { "ngp_ss2sp_dub",       "disabled" },
  { "ngp_svcsp_toast",     "disabled" },
  /* ★ 이 둘이 빠져 있어서 core_dbg.so 가 조용히 288x184 로 나왔다 (순정은 160x152).
     지오메트리를 바꾸는 옵션은 이 둘뿐이고, 둘 다 **안 주면 켜진 상태가 된다**:
       ngp_ss2sp_sides — 코어에서 `static bool ss2_sides = true`. 조회 실패면
                         그대로 참이라 폭이 SS2_WIDE_W(64*2+160=288)가 된다.
       ngp_svcsp_band  — 코어가 조회 실패 시 `int on = 1` 로 **켠다**. 다른 옵션은
                         전부 `if (…&& var.value)` 라 무해한데 이것만 다르다.
                         높이가 152+32=184 가 된다.
     PPM 을 바이트로 비교하는 검증 도구가 여기서 어긋난다. */
  { "ngp_ss2sp_sides",     "disabled" },
  { "ngp_svcsp_band",      "disabled" },
  { NULL, NULL }
};

static void vcb(const void *d, unsigned w, unsigned h, size_t pitch) {
  unsigned y;
  W = w; H = h;
  if (h > 512) h = 512;
  for (y = 0; y < h; y++) {
    if (PIXFMT == 1)
      memcpy(FB32 + y * 512, (const char *)d + y * pitch,
             (w < 512 ? w : 512) * 4);
    else
      memcpy(FB + y * 512, (const char *)d + y * pitch,
             (w < 512 ? w : 512) * 2);
  }
}

/* ── 소리 계측 ────────────────────────────────────────────────
   「소리가 안 난다」는 제보를 화면처럼 **숫자로** 가려야 한다. 코어가 넘겨주는
   샘플의 절대값 합과 최대 진폭을 프레임마다 모아 두고, 대본이 끝나면 찍는다.
   원본과 패치본을 같은 대본으로 돌려 이 숫자를 대면 「패치가 소리를 죽였나」가 가려진다. */
static long long AUD_SUM;      /* |샘플| 총합 */
static long long AUD_N;        /* 샘플 수 */
static int       AUD_PEAK;     /* 최대 진폭 */
static int       AUD_FRAMES;   /* 소리가 있었던 프레임 수 */
static int       AUD_CURPEAK;  /* 이번 프레임 최대 */

static void aud_take(const short *d, size_t frames) {
  size_t i;
  if (!d) return;
  for (i = 0; i < frames * 2; i++) {           /* 스테레오 */
    int v = d[i] < 0 ? -d[i] : d[i];
    AUD_SUM += v; AUD_N++;
    if (v > AUD_PEAK)    AUD_PEAK = v;
    if (v > AUD_CURPEAK) AUD_CURPEAK = v;
  }
}

static void acb(short a, short b) {
  short pair[2]; pair[0] = a; pair[1] = b;
  aud_take(pair, 1);
}
static size_t abcb(const short *d, size_t f) { aud_take(d, f); return f; }
static void poll(void) {}

/* 매 프레임 유지되는 램 덮어쓰기 — 체력 사본이 프레임마다 복원되므로 지속 적용해야 한다 */
#define MAXPOKE 16
static struct { unsigned off; int val; } POKES[MAXPOKE];
static int NPOKE;

/* ── 프레임별 램 관찰 (CSV) ──────────────────────────────────
   시점 덤프(`!<태그>`)만으로는 「몇 번째 프레임에 발동했나」를 못 잰다.
   판정을 사후 해석하면 그럴듯한 엉뚱한 결론에 수렴한다 — 그래서 원시 CSV 가
   필요하다. 발동=act 진입 프레임 / 히트=react 프레임 / 콤보=그 프레임 combo 값,
   이 셋을 **한 파일에서 프레임 번호로** 맞춰 봐야 판정이 선다.

     !w <태그> <오프셋[:w][,…]>   그 프레임부터 매 프레임 CSV 로 남긴다
     !w off                       멈춘다
   `:w` 를 붙이면 16비트 리틀엔디언으로 읽는다(좌표는 8비트로 보면 랩된다). */
#define MAXW 24
static struct { unsigned off; int wide; } WATCH[MAXW];
static int NWATCH;
static FILE *WCSV;

static short inp(unsigned port, unsigned dev, unsigned idx, unsigned id) {
  (void)idx;
  if (port != 0 || dev != RETRO_DEVICE_JOYPAD || id > 15) return 0;
  return (PAD >> id) & 1;
}

static int envcb(unsigned cmd, void *data) {
  switch (cmd) {
  case ENV_GET_CAN_DUPE:
    *(bool *)data = true;
    return 1;
  case ENV_SET_PIXEL_FORMAT:
    PIXFMT = *(const int *)data;
    return 1;
  case ENV_GET_SYSTEM_DIR:
  case ENV_GET_SAVE_DIR:
    *(const char **)data = ".";
    return 1;
  case ENV_GET_VARIABLE: {
    struct retro_variable *v = data;
    if (!strcmp(v->key, "ngp_language")) {
      const char *e = getenv("NGP_LANG");
      v->value = e ? e : "japanese";
      return 1;
    }
    for (int i = 0; DEFAULTS[i].key; i++)
      if (!strcmp(v->key, DEFAULTS[i].key)) { v->value = DEFAULTS[i].value; return 1; }
    v->value = NULL;
    return 0;
  }
  case ENV_GET_VARIABLE_UPDATE:
    *(bool *)data = false;
    return 1;
  case ENV_SET_VARIABLES:
    return 1;
  default:
    return 0;
  }
}

static void dump(const char *tag, const uint8_t *ram, size_t rlen) {
  char p[256];
  FILE *f;
  unsigned x, y;
  snprintf(p, sizeof p, "%s_%s.ram", PREFIX, tag);
  if ((f = fopen(p, "wb"))) { fwrite(ram, 1, rlen, f); fclose(f); }
  snprintf(p, sizeof p, "%s_%s.ppm", PREFIX, tag);
  if (!(f = fopen(p, "wb"))) return;
  fprintf(f, "P6\n%u %u\n255\n", W, H);
  for (y = 0; y < H; y++)
    for (x = 0; x < W; x++) {
      if (PIXFMT == 1) {
        uint32_t v = FB32[y * 512 + x];
        fputc((v >> 16) & 255, f); fputc((v >> 8) & 255, f); fputc(v & 255, f);
      } else if (PIXFMT == 2) {
        uint16_t v = FB[y * 512 + x];
        fputc(((v >> 11) & 31) * 255 / 31, f);
        fputc(((v >> 5) & 63) * 255 / 63, f);
        fputc((v & 31) * 255 / 31, f);
      } else {
        uint16_t v = FB[y * 512 + x];
        fputc(((v >> 10) & 31) * 255 / 31, f);
        fputc(((v >> 5) & 31) * 255 / 31, f);
        fputc((v & 31) * 255 / 31, f);
      }
    }
  fclose(f);
}

enum { ID_B, ID_Y, ID_SE, ID_ST, ID_U, ID_D, ID_L, ID_R, ID_A, ID_X, ID_L1, ID_R1 };

static int btn(const char *s) {
  if (!strcmp(s, "U")) return ID_U;   if (!strcmp(s, "D"))  return ID_D;
  if (!strcmp(s, "L")) return ID_L;   if (!strcmp(s, "R"))  return ID_R;
  if (!strcmp(s, "A")) return ID_A;   if (!strcmp(s, "B"))  return ID_B;
  if (!strcmp(s, "X")) return ID_X;   if (!strcmp(s, "Y"))  return ID_Y;
  if (!strcmp(s, "ST")) return ID_ST; if (!strcmp(s, "SE")) return ID_SE;
  if (!strcmp(s, "L1")) return ID_L1; if (!strcmp(s, "R1")) return ID_R1;
  return -1;
}

int main(int argc, char **argv) {
  if (argc < 4) { printf("사용: ngprun <core.so> <rom> <script> [접두사]\n"); return 2; }
  if (argc > 4) snprintf(PREFIX, sizeof PREFIX, "%s", argv[4]);

  void *h = dlopen(argv[1], RTLD_NOW);
  if (!h) { printf("dlopen: %s\n", dlerror()); return 1; }

#define SYM(v, n) do { *(void **)&(v) = dlsym(h, n); \
    if (!(v)) { printf("심볼 없음: %s\n", n); return 1; } } while (0)

  void (*set_env)(int (*)(unsigned, void *));
  void (*set_video)(void (*)(const void *, unsigned, unsigned, size_t));
  void (*set_audio)(void (*)(short, short));
  void (*set_audio_batch)(size_t (*)(const short *, size_t));
  void (*set_input_poll)(void (*)(void));
  void (*set_input_state)(short (*)(unsigned, unsigned, unsigned, unsigned));
  void (*set_port_device)(unsigned, unsigned);
  void (*core_init)(void);
  bool (*load_game)(const struct retro_game_info *);
  void (*get_av)(struct retro_av_info *);
  void (*core_run)(void);
  size_t (*ser_size)(void);
  bool (*ser)(void *, size_t);
  bool (*unser)(const void *, size_t);
  void *(*getmem)(unsigned);
  size_t (*getsz)(unsigned);
  /* 코어가 내보내는 VRAM 접근자. 롬 안에서 못 찾는 그래픽(압축된 폰트 등)을
     실행 중 화면 데이터로 직접 잡을 때 쓴다. 없는 코어면 조용히 건너뛴다. */
  void *(*ngp_charram)(void)  = 0;   /* 8192B  a000-bfff  타일 512개 */
  void *(*ngp_scrollram)(void) = 0;  /* 4096B  9000-9fff  타일맵 */
  void *(*ngp_spriteram)(void) = 0;  /* 256B   8800-88ff */
  void *(*ngp_spritecol)(void) = 0;  /* 64B    8c00-8c3f */
  void *(*ngp_palram)(void)   = 0;   /* 512B   8200-83ff */
  /* 계측 빌드 전용 — CharacterRAM 에 쓴 코드 주소(PC) 목록 */
  int (*ngp_gfxlog)(uint32_t **, uint32_t **, uint32_t **) = 0;
  void *(*ngp_rdmap)(void) = 0;          /* 롬 읽기 지도 8192칸(256B 단위) */
  void (*ngp_rdmap_clear)(void) = 0;

  SYM(set_env,         "retro_set_environment");
  SYM(set_video,       "retro_set_video_refresh");
  SYM(set_audio,       "retro_set_audio_sample");
  SYM(set_audio_batch, "retro_set_audio_sample_batch");
  SYM(set_input_poll,  "retro_set_input_poll");
  SYM(set_input_state, "retro_set_input_state");
  SYM(set_port_device, "retro_set_controller_port_device");
  SYM(core_init,       "retro_init");
  SYM(load_game,       "retro_load_game");
  SYM(get_av,          "retro_get_system_av_info");
  SYM(core_run,        "retro_run");
  SYM(ser_size,        "retro_serialize_size");
  SYM(ser,             "retro_serialize");
  SYM(unser,           "retro_unserialize");
  SYM(getmem,          "retro_get_memory_data");
  SYM(getsz,           "retro_get_memory_size");
  /* 여기는 SYM 을 안 쓴다 — 없어도 하네스는 굴러가야 한다 */
  *(void **)&ngp_charram   = dlsym(h, "retro_ngp_charram");
  *(void **)&ngp_scrollram = dlsym(h, "retro_ngp_scrollram");
  *(void **)&ngp_spriteram = dlsym(h, "retro_ngp_spriteram");
  *(void **)&ngp_spritecol = dlsym(h, "retro_ngp_spritecol");
  *(void **)&ngp_palram    = dlsym(h, "retro_ngp_palram");
  *(void **)&ngp_gfxlog    = dlsym(h, "retro_ngp_gfxlog");
  *(void **)&ngp_rdmap     = dlsym(h, "retro_ngp_rdmap");
  *(void **)&ngp_rdmap_clear = dlsym(h, "retro_ngp_rdmap_clear");
  if (!ngp_charram) printf("  (VRAM 접근자 없는 코어 — !vram 은 못 쓴다)\n");

  set_env(envcb);
  set_video(vcb);
  set_audio(acb);
  set_audio_batch(abcb);
  set_input_poll(poll);
  set_input_state(inp);
  core_init();

  FILE *rf = fopen(argv[2], "rb");
  if (!rf) { printf("롬 못 엶\n"); return 1; }
  fseek(rf, 0, SEEK_END); long n = ftell(rf); fseek(rf, 0, SEEK_SET);
  void *rom = malloc(n);
  if (fread(rom, 1, n, rf) != (size_t)n) { printf("롬 읽기 실패\n"); return 1; }
  fclose(rf);

  struct retro_game_info g = { argv[2], rom, (size_t)n, NULL };
  if (!load_game(&g)) { printf("LOAD 실패\n"); return 1; }

  /* ★ 이 한 줄이 없으면 코어가 패드를 안 붙인다 */
  set_port_device(0, RETRO_DEVICE_JOYPAD);

  struct retro_av_info av;
  get_av(&av);
  uint8_t *ram = getmem(2);
  size_t rlen = getsz(2);
  printf("SYSTEM_RAM %zu바이트, 픽셀포맷 %d\n", rlen, PIXFMT);

  FILE *sf = fopen(argv[3], "r");
  char line[256];
  long frame = 0;
  while (sf && fgets(line, sizeof line, sf)) {
    char *p = line;
    if (*p == '!') {
      char cmd[64], arg[128];
      int k = sscanf(p + 1, "%63s %127s", cmd, arg);
      if (k >= 2 && !strcmp(cmd, "save")) {
        size_t sz = ser_size(); void *buf = malloc(sz);
        if (ser(buf, sz)) { FILE *o = fopen(arg, "wb"); fwrite(buf, 1, sz, o); fclose(o);
          printf("  [%ld] 저장 %s\n", frame, arg); }
        free(buf); continue;
      }
      if (k >= 2 && !strcmp(cmd, "load")) {
        FILE *o = fopen(arg, "rb");
        if (!o) { printf("  !! %s 없음\n", arg); continue; }
        fseek(o, 0, SEEK_END); long sz = ftell(o); fseek(o, 0, SEEK_SET);
        void *buf = malloc(sz);
        if (fread(buf, 1, sz, o) != (size_t)sz) {}
        fclose(o);
        printf("  [%ld] 복원 %s -> %s\n", frame, arg, unser(buf, sz) ? "OK" : "실패");
        free(buf); continue;
      }
      if (!strcmp(cmd, "w")) {
        /* !w off  /  !w <태그> <오프셋[:w][,…]> */
        if (k >= 2 && !strcmp(arg, "off")) {
          if (WCSV) { fclose(WCSV); WCSV = 0; }
          NWATCH = 0;
          printf("  [%ld] 관찰 멈춤\n", frame);
          continue;
        }
        char tag[64], list[160];
        if (sscanf(p + 1, "%*s %63s %159s", tag, list) != 2) {
          printf("  !w <태그> <오프셋[:w][,…]>  또는  !w off\n");
          continue;
        }
        if (WCSV) fclose(WCSV);
        NWATCH = 0;
        for (char *q = list; q && *q && NWATCH < MAXW; ) {
          unsigned off; char sfx[4] = "";
          if (sscanf(q, "%x:%1s", &off, sfx) >= 1 && off < rlen) {
            WATCH[NWATCH].off = off;
            WATCH[NWATCH].wide = (sfx[0] == 'w' || sfx[0] == 'W');
            NWATCH++;
          }
          q = strchr(q, ',');
          if (q) q++;
        }
        char path[512];
        snprintf(path, sizeof path, "%s%s.csv", PREFIX, tag);
        WCSV = fopen(path, "w");
        if (WCSV) {
          fprintf(WCSV, "frame,pad");
          for (int i = 0; i < NWATCH; i++)
            fprintf(WCSV, ",%04X%s", WATCH[i].off, WATCH[i].wide ? "w" : "");
          fprintf(WCSV, "\n");
        }
        printf("  [%ld] 관찰 %d칸 → %s\n", frame, NWATCH, path);
        continue;
      }
      if (k >= 2 && !strcmp(cmd, "poke")) {
        unsigned off; int val;
        if (sscanf(arg, "%x=%d", &off, &val) == 2 && off < rlen && NPOKE < MAXPOKE) {
          POKES[NPOKE].off = off; POKES[NPOKE].val = val; NPOKE++;
          printf("  [%ld] poke %04X=%d (지속)\n", frame, off, val);
        }
        continue;
      }
      if (!strcmp(cmd, "unpoke")) { NPOKE = 0; printf("  [%ld] poke 해제\n", frame); continue; }
      if (k >= 2 && !strcmp(cmd, "peek")) {
        unsigned off;
        if (sscanf(arg, "%x", &off) == 1 && off < rlen)
          printf("  [%ld] peek %04X = %d\n", frame, off, ram[off]);
        continue;
      }
      if (!strcmp(cmd, "vram")) {
        /* 화면에 지금 올라와 있는 타일·타일맵·팔레트를 그대로 떠낸다.
           롬에서 못 찾는 그래픽을 잡는 유일한 확실한 경로다. */
        static const struct { const char *n; size_t sz; } V[5] =
          { { "char", 8192 }, { "scroll", 4096 }, { "sprite", 256 },
            { "spritecol", 64 }, { "pal", 512 } };
        void *(*fn[5])(void) = { ngp_charram, ngp_scrollram, ngp_spriteram,
                                 ngp_spritecol, ngp_palram };
        const char *tag = (k >= 2) ? arg : "vram";
        int i;
        for (i = 0; i < 5; i++) {
          void *p = fn[i] ? fn[i]() : 0;
          char path[512];
          FILE *o;
          if (!p) continue;
          snprintf(path, sizeof path, "%s%s.%s", PREFIX, tag, V[i].n);
          o = fopen(path, "wb");
          if (!o) continue;
          fwrite(p, 1, V[i].sz, o);
          fclose(o);
        }
        printf("  [%ld] %s VRAM 덤프\n", frame, tag);
        continue;
      }
      if (!strcmp(cmd, "pairs")) {
        /* VRAM 쓰기와 직전 카트 읽기의 짝을 파일로. 원본 주소가 연속이면 단순 복사,
           들쭉날쭉하면 압축 해제다. */
        int (*fn)(uint32_t **, uint16_t **) = dlsym(h, "retro_ngp_gfxpair");
        void (*clr)(void) = dlsym(h, "retro_ngp_gfxpair_clear");
        uint32_t *src = 0; uint16_t *dst = 0;
        if (!strcmp(arg, "clear")) {
          if (clr) { clr(); printf("  [%ld] 짝 기록 초기화\n", frame); }
          continue;
        }
        int (*fn2)(uint32_t **, uint16_t **, uint32_t **, uint16_t **) =
            dlsym(h, "retro_ngp_gfxpair2");
        int (*fnv)(uint8_t **) = dlsym(h, "retro_ngp_gfxval");
        uint8_t *vals = 0;
        if (fnv) fnv(&vals);
        if (fn2) {
          uint16_t *ram = 0; uint32_t *pcs = 0;
          int n = fn2(&src, &ram, &pcs, &dst), i;
          char path[512];
          FILE *o;
          snprintf(path, sizeof path, "%s%s.pairs", PREFIX, (k >= 2) ? arg : "p");
          if ((o = fopen(path, "w")))
            { for (i = 0; i < n; i++)
                fprintf(o, "%06X %04X %06X %04X %02X\n", src[i], ram[i], pcs[i],
                        dst[i], vals ? vals[i] : 0);
              fclose(o); }
          printf("  [%ld] 짝 %d개 저장 (롬·RAM·PC·VRAM·값)\n", frame, n);
        } else if (fn) {
          int n = fn(&src, &dst), i;
          char path[512];
          FILE *o;
          snprintf(path, sizeof path, "%s%s.pairs", PREFIX, (k >= 2) ? arg : "p");
          if ((o = fopen(path, "w")))
            { for (i = 0; i < n; i++) fprintf(o, "%06X %04X\n", src[i], dst[i]); fclose(o); }
          printf("  [%ld] 짝 %d개 저장\n", frame, n);
        } else printf("  (계측 코어가 아니다)\n");
        continue;
      }
      if (!strcmp(cmd, "ramsnap")) {
        /* NGP_SNAPPC=24B300 로 지정한 코드가 0 아닌 값을 처음 쓰는 순간의 CPU RAM.
           스크래치에 잠깐 풀렸다 사라지는 폰트를 잡는 유일한 방법이다. */
        void *(*fn)(int *) = dlsym(h, "retro_ngp_ramsnap");
        int done = 0;
        void *m = fn ? fn(&done) : 0;
        char path[512];
        FILE *o;
        if (!m) { printf("  (계측 코어가 아니다)\n"); continue; }
        snprintf(path, sizeof path, "%s%s.ramsnap", PREFIX, (k >= 2) ? arg : "s");
        if ((o = fopen(path, "wb"))) { fwrite(m, 1, 16384, o); fclose(o); }
        printf("  [%ld] RAM 스냅샷 %s\n", frame, done ? "저장" : "(아직 안 찍힘)");
        continue;
      }
      if (!strcmp(cmd, "itrace")) {
        /* NGP_ITRACE=lo-hi 구간의 실행 순서와 레지스터. 루틴을 읽는 데 쓴다. */
        int (*fn)(uint32_t **, uint32_t **, uint32_t **, uint32_t **, uint32_t **) =
            dlsym(h, "retro_ngp_itrace");
        void (*clr)(void) = dlsym(h, "retro_ngp_itrace_clear");
        uint32_t *p = 0, *wa = 0, *bc = 0, *de = 0, *hl = 0;
        if (k >= 2 && !strcmp(arg, "clear")) {
          if (clr) { clr(); printf("  [%ld] 트레이스 초기화\n", frame); }
          continue;
        }
        if (fn) {
          /* IX 는 따로 내보내는 코어에서만 온다. 없으면 0 으로 찍는다 —
             옛 코어(core.so)로도 그대로 돌아야 한다. */
          uint32_t *(*fix)(void) = dlsym(h, "retro_ngp_itrace_ix");
          uint32_t *ix = fix ? fix() : 0;
          int n = fn(&p, &wa, &bc, &de, &hl), i;
          char path[512];
          FILE *o;
          snprintf(path, sizeof path, "%s%s.itr", PREFIX, (k >= 2) ? arg : "t");
          if ((o = fopen(path, "w")))
            { for (i = 0; i < n; i++)
                fprintf(o, "%06X %08X %08X %08X %08X %08X\n",
                        p[i], wa[i], bc[i], de[i], hl[i], ix ? ix[i] : 0);
              fclose(o); }
          printf("  [%ld] 명령 %d개 기록\n", frame, n);
        } else printf("  (계측 코어가 아니다)\n");
        continue;
      }
      if (!strcmp(cmd, "ramw")) {
        /* NGP_RAMW=lo-hi 로 지정한 RAM 구간에 쓴 코드 주소 목록.
           폰트가 RAM 버퍼를 거칠 때 그 버퍼를 채우는 코드를 찾는다. */
        int (*fn)(uint32_t **, uint32_t **, uint32_t **) = dlsym(h, "retro_ngp_ramw");
        uint32_t *pcs = 0, *srcs = 0, *hits = 0;
        int n = fn ? fn(&pcs, &srcs, &hits) : -1, i, j;
        if (n < 0) { printf("  (계측 코어가 아니다)\n"); continue; }
        printf("  [%ld] 그 RAM 구간에 쓴 PC %d종\n", frame, n);
        for (i = 0; i < n; i++) {
          int best = i;
          for (j = i + 1; j < n; j++) if (hits[j] > hits[best]) best = j;
          if (best != i) {
            uint32_t t;
            t = pcs[i];  pcs[i]  = pcs[best];  pcs[best]  = t;
            t = srcs[i]; srcs[i] = srcs[best]; srcs[best] = t;
            t = hits[i]; hits[i] = hits[best]; hits[best] = t;
          }
          if (i < 12)
            printf("    PC %06X (롬 %06X)  %6u회   그때 카트읽기 %06X (롬 %06X)\n",
                   pcs[i], pcs[i] - 0x200000, hits[i], srcs[i], srcs[i] - 0x200000);
        }
        continue;
      }
      if (!strcmp(cmd, "rdclear")) {
        if (ngp_rdmap_clear) { ngp_rdmap_clear(); printf("  [%ld] 읽기지도 초기화\n", frame); }
        else printf("  (계측 코어가 아니다)\n");
        continue;
      }
      if (!strcmp(cmd, "rdmap")) {
        /* 그동안 읽은 롬 구간을 파일로. 화면별로 떠서 차분을 내면
           그 화면 전용 데이터(압축 폰트 등)가 드러난다. */
        void *m = ngp_rdmap ? ngp_rdmap() : 0;
        char path[512];
        FILE *o;
        if (!m) { printf("  (계측 코어가 아니다)\n"); continue; }
        snprintf(path, sizeof path, "%s%s.rdmap", PREFIX, (k >= 2) ? arg : "rd");
        if ((o = fopen(path, "wb"))) { fwrite(m, 1, 8192, o); fclose(o); }
        printf("  [%ld] 읽기지도 저장\n", frame);
        continue;
      }
      if (!strcmp(cmd, "gfxlog")) {
        /* 글자 타일을 쓴 코드 주소를 많이 쓴 순으로 뱉는다.
           NGP_GFXLOG=1 로 켠 계측 코어에서만 값이 찬다. */
        uint32_t *pcs = 0, *hits = 0, *firsts = 0;
        int n = ngp_gfxlog ? ngp_gfxlog(&pcs, &hits, &firsts) : -1;
        int i, j;
        if (n < 0) { printf("  (계측 코어가 아니다)\n"); continue; }
        printf("  [%ld] VRAM 타일을 쓴 PC %d종\n", frame, n);
        for (i = 0; i < n; i++) {
          int best = i;
          for (j = i + 1; j < n; j++) if (hits[j] > hits[best]) best = j;
          if (best != i) {
            uint32_t t;
            t = pcs[i]; pcs[i] = pcs[best]; pcs[best] = t;
            t = hits[i]; hits[i] = hits[best]; hits[best] = t;
            t = firsts[i]; firsts[i] = firsts[best]; firsts[best] = t;
          }
          if (i < 16)
            printf("    PC %06X  쓰기 %6u회  처음 VRAM %04X  (롬 %06X)\n",
                   pcs[i], hits[i], firsts[i], pcs[i] - 0x200000);
        }
        continue;
      }
      if (k >= 1) { dump(cmd, ram, rlen); printf("  [%ld] %s 덤프\n", frame, cmd); }
      continue;
    }
    int nf = strtol(p, &p, 10);
    if (nf <= 0) continue;
    PAD = 0;
    char tok[16];
    while (sscanf(p, "%15s", tok) == 1) {
      p = strstr(p, tok) + strlen(tok);
      if (strcmp(tok, "-")) { int b = btn(tok); if (b >= 0) PAD |= (int16_t)(1 << b); }
    }
    for (int i = 0; i < nf; i++) {
      for (int k = 0; k < NPOKE; k++) ram[POKES[k].off] = (uint8_t)POKES[k].val;
      AUD_CURPEAK = 0;
      core_run();
      if (AUD_CURPEAK > 64) AUD_FRAMES++;   /* 잡음 바닥 위로 올라온 프레임만 센다 */
      frame++;
      /* **코어를 돌린 뒤에** 읽는다 — 그 프레임의 결과를 남겨야 발동 프레임이 맞는다 */
      if (WCSV) {
        fprintf(WCSV, "%ld,%04X", frame, (unsigned)(uint16_t)PAD);
        for (int j = 0; j < NWATCH; j++) {
          unsigned o = WATCH[j].off;
          fprintf(WCSV, ",%d", WATCH[j].wide
                  ? (int)(ram[o] | (ram[o + 1] << 8)) : (int)ram[o]);
        }
        fprintf(WCSV, "\n");
      }
    }
  }
  if (sf) fclose(sf);
  if (WCSV) fclose(WCSV);
  printf("총 %ld 프레임, 화면 %ux%u\n", frame, W, H);
  printf("소리: 샘플 %lld, 평균|진폭| %.1f, 최대 %d, 소리난 프레임 %d/%ld\n",
         AUD_N, AUD_N ? (double)AUD_SUM / (double)AUD_N : 0.0,
         AUD_PEAK, AUD_FRAMES, frame);
  dump("end", ram, rlen);
  return 0;
}
