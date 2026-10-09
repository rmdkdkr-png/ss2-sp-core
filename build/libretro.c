#include <libretro.h>
#include <string.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>

#include "libretro_core_options.h"
#include "mednafen/git.h"
#include "mednafen/general.h"
#include "mednafen/file.h"
#include "mednafen/mednafen-types.h"
#include "mednafen/mempatcher.h"
#include "mednafen/settings.h"
#include "mednafen/state.h"
#include "mednafen/state_helpers.h"

#ifdef _MSC_VER
#include <compat/msvc.h>
#endif

/* Forward declarations */
void MDFN_LoadGameCheats(void);
void MDFN_FlushGameCheats(void);

/* core options */

static int RETRO_PIX_DEPTH   = 15;

static bool persistent_data  = false;

/* ==================================================== */

struct retro_perf_callback perf_cb;
retro_get_cpu_features_t perf_get_cpu_features_cb = NULL;
retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

static bool overscan;

static MDFN_Surface *surf;

static bool libretro_supports_bitmasks = false;

static char retro_base_directory[1024];
static char retro_base_name[1024];
static char retro_save_directory[1024];

/*---------------------------------------------------------------------------
 * NEOPOP : Emulator as in Dreamland
 *
 * Copyright (c) 2001-2002 by neopop_uk
 *---------------------------------------------------------------------------
 */

/*---------------------------------------------------------------------------
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version. See also the license.txt file for
 *	additional informations.
 *---------------------------------------------------------------------------
 */

#include "mednafen/ngp/neopop.h"
#include "mednafen/general.h"

#include "mednafen/ngp/TLCS-900h/TLCS900h_interpret.h"
#include "mednafen/ngp/TLCS-900h/TLCS900h_registers.h"
#include "mednafen/ngp/Z80_interface.h"
#include "mednafen/ngp/interrupt.h"
#include "mednafen/ngp/mem.h"
#include "mednafen/ngp/rom.h"
#include "mednafen/ngp/gfx.h"
#include "mednafen/ngp/sound.h"
#include "mednafen/ngp/dma.h"
#include "mednafen/ngp/bios.h"
#include "mednafen/ngp/flash.h"
#include "mednafen/ngp/system.h"

extern uint8 CPUExRAM[16384];

ngpgfx_t *NGPGfx;

COLOURMODE system_colour = COLOURMODE_AUTO;

static uint8 *chee;

static int32 z80_runtime;

/* 프레임 생성 예측용 가벼운 스냅샷 — StateAction 에서 건너뛸 섹션 (정의는 아래 ss2fg 배관 참고) */
#define FG_SKIP_SND   1
#define FG_SKIP_FLASH 2
static int fg_state_skip = 0;

extern int32_t ngpc_soundTS;

static void Emulate(EmulateSpecStruct *espec, int16_t *sound_buf)
{
   bool MeowMeow        = false;

   if(espec->VideoFormatChanged)
      ngpgfx_set_pixel_format(NGPGfx, espec->surface->depth);

   storeB(0x6F82, *chee);

   MDFNMP_ApplyPeriodicCheats();

   ngpc_soundTS         = 0;

   do
   {
      int32 timetime = (uint8)TLCS900h_interpret();
      MeowMeow |= updateTimers(espec->surface, timetime);
      z80_runtime += timetime;

      while(z80_runtime > 0)
      {
         int z80rantime = Z80_RunOP();

         if (z80rantime < 0) /* Z80 inactive, so take up all run time! */
         {
            z80_runtime = 0;
            break;
         }

         z80_runtime -= z80rantime << 1;

      }
   }while(!MeowMeow);

   espec->SoundBufSize = MDFNNGPCSOUND_Flush(sound_buf,
         espec->SoundBufMaxSize);
}

void neopop_reset(void)
{
   ngpgfx_power(NGPGfx);
   Z80_reset();
   reset_int();
   reset_timers();

   reset_memory();
   BIOSHLE_Reset();
   reset_registers();	/* TLCS900H registers */
   reset_dma();
}

static void extract_basename(char *buf, const char *path, size_t size)
{
   char *ext        = NULL;
   const char *base = strrchr(path, '/');
   if (!base)
      base = strrchr(path, '\\');
   if (!base)
      base = path;

   if (*base == '\\' || *base == '/')
      base++;

   strncpy(buf, base, size - 1);
   buf[size - 1] = '\0';

   ext = strrchr(buf, '.');
   if (ext)
      *ext = '\0';
}

static struct MDFNFILE *file_open(const char *path)
{
   int64_t size          = 0;
   const char        *ld = NULL;
   struct MDFNFILE *file = (struct MDFNFILE*)calloc(1, sizeof(*file));

   if (!file)
      return NULL;

   if (!filestream_read_file(path, (void**)&file->data, &size))
   {
      free(file);
      return NULL;
   }

   ld         = (const char*)strrchr(path, '.');
   file->size = size;
   file->ext  = strdup(ld ? ld + 1 : "");

   return file;
}

static int file_close(struct MDFNFILE *file)
{
   if (!file)
      return 0;

   if (file->ext)
      free(file->ext);
   file->ext = NULL;

   if (file->data)
      free(file->data);
   file->data = NULL;

   free(file);

   return 1;
}

static int Load(const char *path,
      const uint8_t *data, size_t size)
{
   struct retro_game_info_ext *info_ext = NULL;
   const char *rom_path = NULL;
   const uint8_t *rom_data = NULL;
   size_t rom_size = 0;

   /* Attempt to fetch extended game info */
   if (environ_cb(RETRO_ENVIRONMENT_GET_GAME_INFO_EXT, &info_ext))
   {
      rom_path = info_ext->full_path;
      rom_data = (const uint8_t *)info_ext->data;
      rom_size = info_ext->size;
      persistent_data = info_ext->persistent_data;
      /* Use canonical content name for save files */
      strlcpy(retro_base_name, info_ext->name, sizeof(retro_base_name));
   }
   else
   {
      rom_path = path;
      rom_data = data;
      rom_size = size;
      persistent_data = false;
      if (rom_path)
         extract_basename(retro_base_name, rom_path, sizeof(retro_base_name));
   }

   /* Use existing ROM data if available */
   if (rom_data && rom_size)
   {
      if (persistent_data)
      {
         ngpc_rom.orig_data = (uint8_t*)rom_data;
         ngpc_rom.length    = rom_size;
      }
      else
      {
         if (!(ngpc_rom.orig_data = (uint8 *)malloc(rom_size)))
            return 0;
         ngpc_rom.length = rom_size;
         memcpy(ngpc_rom.orig_data, rom_data, rom_size);
      }
   }
   /* Load ROM data from file */
   else
   {
      struct MDFNFILE *rom_file = NULL;

      if (rom_path)
         rom_file = file_open(rom_path);

      if (!rom_file)
         return 0;

      if (!(ngpc_rom.orig_data = (uint8 *)malloc(rom_file->size)))
      {
         file_close(rom_file);
         return 0;
      }

      ngpc_rom.length = rom_file->size;
      memcpy(ngpc_rom.orig_data, rom_file->data, rom_file->size);
      file_close(rom_file);
   }

   rom_loaded(ngpc_rom.orig_data, ngpc_rom.length);

   MDFNMP_Init(1024, 1024 * 1024 * 16 / 1024);

   NGPGfx = (ngpgfx_t*)calloc(1, sizeof(*NGPGfx));
   NGPGfx->layer_enable = 1 | 2 | 4;

   MDFNNGPCSOUND_Init();

   MDFNMP_AddRAM(16384, 0x4000, CPUExRAM);

   SetFRM(); /* Set up fast read memory mapping */

   bios_install();

   z80_runtime = 0;

   neopop_reset();

   return 1;
}

void StateAction(StateMem *sm, int load, int data_only)
{
   SFORMAT StateRegs[] =
   {
      SFVARN(z80_runtime, "z80_runtime"),
      SFARRAY(CPUExRAM, 16384),
      SFVARN_BOOL(FlashStatusEnable, "FlashStatusEnable"),
      SFEND
   };

   SFORMAT TLCS_StateRegs[] =
   {
      { &pc, (uint32_t)sizeof(pc), MDFNSTATE_RLSB, "PC" },
      { &sr, (uint32_t)sizeof(sr), MDFNSTATE_RLSB, "SR" },
      { &f_dash, (uint32_t)sizeof(f_dash), MDFNSTATE_RLSB, "F_DASH" },
      { gpr, (uint32_t)(4 * sizeof(uint32_t)), MDFNSTATE_RLSB32, "GPR" },
      { gprBank[0], (uint32_t)(4 * sizeof(uint32_t)), MDFNSTATE_RLSB32, "GPRB0" },
      { gprBank[1], (uint32_t)(4 * sizeof(uint32_t)), MDFNSTATE_RLSB32, "GPRB1" },
      { gprBank[2], (uint32_t)(4 * sizeof(uint32_t)), MDFNSTATE_RLSB32, "GPRB2" },
      { gprBank[3], (uint32_t)(4 * sizeof(uint32_t)), MDFNSTATE_RLSB32, "GPRB3" },
      { 0, 0, 0, 0 }
   };

   MDFNSS_StateAction(sm, load, data_only, StateRegs, "MAIN", false);
   MDFNSS_StateAction(sm, load, data_only, TLCS_StateRegs, "TLCS", false);
   MDFNNGPCDMA_StateAction(sm, load, data_only);
   if (!(fg_state_skip & FG_SKIP_SND))     /* 예측 스냅샷: 소리는 뮤트라 안 바뀐다 — 로드가 Blip 버퍼를 비우는 것도 피한다 */
      MDFNNGPCSOUND_StateAction(sm, load, data_only);
   ngpgfx_StateAction(NGPGfx, sm, load, data_only);
   MDFNNGPCZ80_StateAction(sm, load, data_only);
   int_timer_StateAction(sm, load, data_only);
   BIOSHLE_StateAction(sm, load, data_only);
   if (!(fg_state_skip & FG_SKIP_FLASH))   /* 예측 스냅샷: 롬 쓰기를 막으므로 롬 전체 사본은 필요 없다 */
      FLASH_StateAction(sm, load, data_only);

   if(load)
   {
      RecacheFRM();
      changedSP();
   }
}

static bool update_video = false;

#define MEDNAFEN_CORE_NAME_MODULE "ngp"
#define MEDNAFEN_CORE_NAME "Beetle NeoPop (SS2 One-button)"
/* TODO/FIXME - only thing missing is flash/RTC refactors */
/* 실행기(HTML)와 같은 버전을 달아 둔다 — 레트로아크 코어 정보에서 확인할 수 있다 */
#define SS2SP_VERSION "0.6"
#define MEDNAFEN_CORE_VERSION "v1.29.0.0 · SS2 v" SS2SP_VERSION
#define MEDNAFEN_CORE_EXTENSIONS "ngp|ngc|ngpc|npc"
#define MEDNAFEN_CORE_TIMING_FPS 60.25
#define MEDNAFEN_CORE_GEOMETRY_BASE_W 160 
#define MEDNAFEN_CORE_GEOMETRY_BASE_H 152
#define MEDNAFEN_CORE_GEOMETRY_MAX_W 160
#define MEDNAFEN_CORE_GEOMETRY_MAX_H 152
#define MEDNAFEN_CORE_GEOMETRY_ASPECT_RATIO (20.0 / 19.0)
#define FB_WIDTH 160
#define FB_HEIGHT 152
#define FB_MAX_HEIGHT FB_HEIGHT

/* ── 프레임 생성(ss2fg) 배관 ─────────────────────────────────────────────
   켜지면 fps 를 2배(120.5)로 선언하고 retro_run 을 실제 프레임 / 합성 프레임으로 번갈아 쓴다.
     예측 모드: 실제 N 을 바로 내보내고, 다음 호출에서 상태를 저장한 채 N+1 을 미리 돌려
               N↔N+1 중간을 그린 뒤 상태를 되돌린다 — 추가 지연 0 (런어헤드와 같은 원리)
     보간 모드: N-1↔N 중간을 먼저, 다음 호출에 N 을 내보낸다 — 표시가 반 프레임(8.3ms) 늦다
   오디오는 실제 프레임의 샘플을 반씩 나눠 두 호출에 보낸다(RetroArch 의 호출당 기대 샘플 수에 맞춤).
   '자동'은 프론트엔드가 목표로 하는 주사율(GET_TARGET_REFRESH_RATE — RetroArch 에선 유저의
   Vertical Refresh Rate 설정값)이 60.25 의 짝수 배일 때만 켠다. 프론트엔드 런어헤드가 감지되면
   (저장 문맥이 런어헤드) 자동은 끈다 — 런어헤드는 'retro_run 1회 = 1프레임' 을 전제한다. */
#include "ss2fg.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif
static int      fg_opt    = 1;         /* 0 끔 · 1 자동 · 2 켬 */
static int      fg_mode   = 0;         /* 0 예측(지연 0) · 1 보간(+8ms) */
static int      fg_active = 0;         /* 지금 2배 fps 로 내보내는 중 */
static int      fg_phase  = 0;         /* 0 실제 프레임 · 1 합성 프레임 */
static int      fg_streak = 0;         /* 전환 히스테리시스 카운터 */
static unsigned fg_frames = 0;         /* retro_run 호출 수 */
static unsigned fg_runahead_seen = 0;  /* 런어헤드용 저장이 마지막으로 보인 호출 번호 */
static int16_t  fg_audio_tail[0x8000]; /* 실제 프레임 소리의 뒷 반 — 합성 호출에서 보낸다 */
static int      fg_audio_tail_n = 0;
static uint16_t fg_real[FB_WIDTH * FB_HEIGHT];   /* 마지막 실제 프레임(후처리 전) */
static int      fg_rate_block = 0;     /* 호출 속도 감시가 '화면이 120Hz 가 아님' 으로 판정 — 옵션을 다시 만질 때까지 끔 */
static double   fg_t_prev = 0, fg_win_sum = 0;
static int      fg_win_n = 0, fg_slow_secs = 0;
static uint8_t *fg_state = NULL;       /* 예측용 상태 버퍼 (MDFNSS 가 필요하면 늘린다) */
static uint32_t fg_state_cap = 0;
static int16_t  fg_scratch_snd[0x10000];
static void Emulate(EmulateSpecStruct *espec, int16_t *sound_buf);
extern int ngplink_active(void);
int ngp_fg_mute = 0;                      /* sound.cpp 가 본다 — 예측 프레임 중 소리 무시 */
extern int ngp_fg_predict;                /* mem.c — 예측 프레임 중 롬/플래시 쓰기 차단 */
extern void ngp_mem_fg_extras(uint8_t *buf, int load);
extern int iline;                         /* z80_ops.c — Z80 인터럽트선, 어떤 StateAction 에도 없다 */

static void fg_apply(int on);
static int fg_want(void)
{
   float hz = 0;
   int want = 0;
   if (fg_opt == 0) return 0;
   if (fg_opt == 2) return 1;
   if (environ_cb(RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE, &hz) && hz > 0)
   {
      int k = (int)(hz / MEDNAFEN_CORE_TIMING_FPS + 0.5f);        /* 60→1, 120→2, 240→4 */
      if (k >= 2 && (k & 1) == 0)
      {
         float skew = (float)MEDNAFEN_CORE_TIMING_FPS / (hz / (float)k);
         if (skew < 0) skew = -skew;
         want = (skew > 0.95f && skew < 1.05f);                   /* RetroArch 의 Maximum Timing Skew 기본 5% */
      }
   }
   if (fg_runahead_seen && fg_frames - fg_runahead_seen < 240)    /* 최근 2초 안에 런어헤드 저장/숨은 호출을 봤다 */
      want = 0;
   if (fg_rate_block)                                                /* 호출 속도가 60/s 에 머물렀다 — 패널이 120Hz 가 아니다 */
      want = 0;
   return want;
}

/* 호출 속도 감시 — RetroArch 설정은 120 인데 패널이 60Hz 에 묶여 있으면(삼성 게임 부스터 등) 프론트엔드가
   60 번/초만 부르고 게임은 반속이 된다. 100ms 넘는 공백(메뉴·일시정지)은 빼고 1초 창의 호출 속도를 재서
   2초 연속 90/s 미만이면 2배 출력을 끄고 알린다. 옵션을 다시 만지거나 게임을 다시 열면 풀린다. */
static double fg_now_sec(void)
{
#ifdef _WIN32
   LARGE_INTEGER f, c;                            /* mingw 엔 clock_gettime 링크가 없다 — 고해상도 카운터로 */
   if (!QueryPerformanceFrequency(&f) || !QueryPerformanceCounter(&c) || f.QuadPart == 0) return 0;
   return (double)c.QuadPart / (double)f.QuadPart;
#else
   struct timespec ts;
   if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
   return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#endif
}

static void fg_watch_rate(void)
{
   double now, dt;
   now = fg_now_sec();
   if (now <= 0) return;
   dt  = now - fg_t_prev;
   fg_t_prev = now;
   if (!fg_active) { fg_win_sum = 0; fg_win_n = 0; fg_slow_secs = 0; return; }
   if (dt <= 0 || dt > 0.1) return;
   fg_win_sum += dt; fg_win_n++;
   if (fg_win_sum < 1.0) return;
   {
      double rate = fg_win_n / fg_win_sum;
      fg_win_sum = 0; fg_win_n = 0;
      if (rate < 90.0) fg_slow_secs++; else fg_slow_secs = 0;
      if (fg_slow_secs >= 2)
      {
         struct retro_message msg;
         fg_slow_secs = 0;
         fg_rate_block = 1;
         fg_apply(0);
         msg.msg    = "화면이 120Hz 로 돌지 않습니다 — 프레임 생성을 껐습니다 (삼성 게임 부스터·주사율 설정 확인)";
         msg.frames = 300;
         environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
      }
   }
}

static void fg_apply(int on)
{
   struct retro_system_av_info av;
   struct retro_message msg;
   fg_active = on;
   fg_phase  = 0;
   fg_streak = 0;
   fg_audio_tail_n = 0;
   retro_get_system_av_info(&av);                 /* fps 가 fg_active 에 따라 60.25 / 120.5 */
   environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av);
   msg.msg    = on ? (fg_mode ? "프레임 생성 켬 — 120Hz 출력 (보간)" : "프레임 생성 켬 — 120Hz 출력 (예측)")
                   : "프레임 생성 끔 — 60Hz 출력";
   msg.frames = 150;
   environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
}

/* 예측 합성: 상태 저장 → 입력을 붙잡은 채 한 프레임 더 → 실제 N ↔ 예측 N+1 중간을 surf 에 → 상태 복원.
   반환 0 이면 그리지 못했다(호출자가 실제 N 을 다시 내보낸다). */
static int fg_predict(void)
{
   StateMem st;
   EmulateSpecStruct ps;
   uint8_t extras[8];
   int iline_save, ok;

   /* 가벼운 스냅샷: 소리·플래시 섹션은 뺀다(예측 중 바뀌지 않게 막는다) + StateAction 밖의 정적 변수들 */
   memset(&st, 0, sizeof st);
   st.data     = fg_state;
   st.malloced = fg_state_cap;
   fg_state_skip = FG_SKIP_SND | FG_SKIP_FLASH;
   ok = MDFNSS_SaveSM(&st, 0, 0, NULL, NULL, NULL);
   fg_state     = st.data;                        /* realloc 됐을 수 있다 */
   fg_state_cap = st.malloced;
   if (!ok) { fg_state_skip = 0; return 0; }
   ngp_mem_fg_extras(extras, 0);
   iline_save = iline;

   memset(&ps, 0, sizeof ps);
   ps.surface         = surf;
   ps.DisplayRect.w   = FB_WIDTH;
   ps.DisplayRect.h   = FB_HEIGHT;
   ps.SoundBufMaxSize = sizeof(fg_scratch_snd) / 2;
   ngp_fg_mute = 1; ngp_fg_predict = 1;
   Emulate(&ps, fg_scratch_snd);                  /* 입력은 마지막 실제 프레임 값 그대로(붙잡고 있다고 가정) */
   ngp_fg_mute = 0; ngp_fg_predict = 0;

   /* 그림은 실제 N(prev), 위치만 예측 N+1(cur) 쪽으로 반 — 없는 그림은 절대 안 나온다 */
   ok = ss2fg_render(ss2fg_prev(), ss2fg_cur(), 128, (uint16_t *)surf->pixels, FB_WIDTH, NGPGfx->ColorMap);

   st.loc = 0;
   MDFNSS_LoadSM(&st, 0, 0);
   fg_state_skip = 0;
   ngp_mem_fg_extras(extras, 1);
   iline = iline_save;
   ss2fg_capture_pop();                           /* 예측 프레임 캡처를 무른다 */
   return ok;
}

static void check_system_specs(void)
{
   unsigned level = 0;
   environ_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL, &level);
}

static void check_color_depth(void)
{
#if defined(FRONTEND_SUPPORTS_RGB565)
      enum retro_pixel_format rgb565 = RETRO_PIXEL_FORMAT_RGB565;

      if (environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rgb565))
      {
         if(log_cb) log_cb(RETRO_LOG_INFO, "Frontend supports RGB565 - will use that instead of 0RGB1555.\n");

         RETRO_PIX_DEPTH = 16;
      }
#else
      enum retro_pixel_format rgb555 = RETRO_PIXEL_FORMAT_0RGB1555;

      if (environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rgb555))
      {
         if(log_cb) log_cb(RETRO_LOG_INFO, "Using default 0RGB1555 pixel format.\n");

         RETRO_PIX_DEPTH = 15;
      }
#endif
}

/* ── SS2 원버튼 필살기 엔진 (ss2sp.c) ──────────────────────────────
   NGPC는 버튼이 A/B/Option 셋뿐이라 패드의 X·Y·L·R·L2·R2가 통째로 논다.
   그 6개를 현재 캐릭터의 필살기 1~6번에 직결한다. 캐릭터·유파는 RAM에서 자동 인식. */
extern uint8_t ss2sp_frame(uint8_t pad, uint16_t trig);
extern void    ss2sp_reset(void);
extern void    ss2sp_set_layout(int sp);
/* ── SvC MotM 원버튼 엔진 (svcsp.c) — 롬 헤더로 자동 판별, SvC 때만 이쪽이 받는다 ── */
extern uint8_t svcsp_frame(uint8_t pad, uint16_t ret);
extern void    ss2voice_init(const char *dir);
extern void    ss2voice_mix(int16_t *buf, int frames);
extern int     ss2voice_on(void);
extern void    svcsp_set_engine(int on);
extern int     svcsp_engine_on(void);
extern void    svcsp_reset(void);
extern void    svcsp_set_rom(const void *rom, unsigned len);
extern int     svcsp_rom_ok(void);
extern char    svcsp_last_disp[64];
extern int     svcsp_disp_seq;
static bool    svcsp_toast_on = true;
static unsigned char ov_toast = 1, ov_toast_p = 1;   /* 오버레이의 기술명 표시 토글 */
static unsigned char ov_vol = 10, ov_vol_p = 10;     /* 해설 볼륨 노브(x10%) */
static unsigned char ov_dub = 1, ov_dub_p = 1;       /* 더빙 온오프(오버레이) */
static unsigned char ov_band = 1, ov_band_p = 1;     /* 기술명 띠(화면 밖) 온오프 */
extern void ss2voice_set_dub(int on);
static bool cv_booted = false;   /* check_variables 런타임 재호출 — 화면 설정류는 부팅 때만
                                    (제보: 해설 바꿀 때마다 기둥아트가 옵션 파일값으로 롤백) */
extern void ss2voice_set_volume(int pct);
extern const char *ss2sp_last_name;
/* ── SS2 캐릭터 해설 엔진 (ss2comm.c) ── */
extern void        ss2comm_set_ram(void *p);
extern void        ss2comm_set_rom(const void *rom, unsigned len);
extern void        ss2comm_rom_fix(void *rom, unsigned len);
extern void        ss2comm_notify(const char *text);
#include "ss2comm.h"   /* SS2COMM_MSG_NOCARD */
extern int         ss2sp_card_block;
extern void        ss2comm_set_enabled(int on);
extern void        ss2comm_set_speaker(int idx);
extern void        ss2comm_set_duo(int on);
extern int         ss2comm_next_speaker(int step);
extern const char *ss2comm_speaker_hello(int idx);
extern void        ss2comm_reset(void);
extern const char *ss2comm_frame(void);
extern void        ss2comm_draw(uint16_t *fb, int pitch_px, int w, int h);
extern void        ss2comm_draw_enable(int mode);
extern int         ss2comm_band_h(void);
extern void        ss2comm_sp_band(int on);
extern int         ss2comm_rom_is_ss2(void);
extern int         ss2comm_band_top(void);
extern int         ss2comm_drawing(void);
#define SS2COMM_BAND_MAX 32   /* = 엔진 SS2_BAND_H (30으로 어긋나 있던 것 정정) */
extern int         ss2sp_last_ok;

/* SS2 원버튼 엔진 on/off (코어 옵션 ngp_ss2sp) */
static bool ss2sp_enable = true;

/* ── SS2 기둥 아트 + 빠른 설정 오버레이 — 앱판과 같은 층 ─────────────
   기둥: 좌우 64px 에 카드 대형 일러(주소표, 유저 롬에서 굽는다)·전황 연출.
   오버레이: 아래+옵션 콤보로 여닫고, 열려 있는 동안 게임·소리 완전 정지.
   SP 배치(기술 목록 선택·타이거니)는 엔진(ss2comm/ss2sp)이 다 들고 있다. */
#define SS2_SIDE_W 64
#define SS2_WIDE_W (SS2_SIDE_W*2 + FB_WIDTH)
/* ★ 기둥 아트 **폐기** (유저 2026-09-07). 옵션을 없앴으므로 이 값이 그대로 굳는다 —
   true 로 두면 «영영 켜진 채»가 된다. 배관은 남긴다(프레임 폭 계산이 본다). */
static bool     ss2_sides = false;
static int      comm_mode_opt = 4;                /* 해설창 옵션 원값 (0끔/2안위/3안아래/4위띠) */

/* 기둥이 켜져 있으면 위-띠(above)를 **화면 안 아래**로 강등한다 — 옆(288)이
   넓어졌는데 세로까지 +32 로 늘릴 이유가 없다는 제보. 기둥을 끄면 원값 복원. */
static void ss2_apply_comm_mode(void)
{
   extern void ss2comm_draw_enable(int mode);
   int m = comm_mode_opt;
   if (m == 4 && ss2_sides) m = 3;
   ss2comm_draw_enable(m);
}
static uint16_t ss2_wide[SS2_WIDE_W * (FB_HEIGHT + SS2COMM_BAND_MAX)];
static unsigned ss2_last_w = FB_WIDTH, ss2_last_h = FB_HEIGHT;
/* 오버레이가 직접 만지는 값 — 코어 옵션이 바뀌면 여기로도 동기한다 */
static unsigned char ov_sp = 1, ov_chat = 1, ov_spk = 0, ov_ref = 1, ov_sides = 1;
static unsigned char ov_sp_p = 1, ov_chat_p = 1, ov_spk_p = 0, ov_ref_p = 1, ov_sides_p = 1;

static void ss2_set_geometry(void)
{
   struct retro_game_geometry geom;
   int band = ss2comm_band_h();
   geom.base_width   = ss2_sides ? SS2_WIDE_W : FB_WIDTH;
   geom.base_height  = FB_HEIGHT + band;
   geom.max_width    = SS2_WIDE_W;
   geom.max_height   = FB_HEIGHT + SS2COMM_BAND_MAX;
   geom.aspect_ratio = (float)geom.base_width / (float)geom.base_height;
   if (environ_cb)
      environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &geom);
}

static void ss2_overlay_apply(void)
{
   if (ov_spk   != ov_spk_p)   { ss2comm_set_speaker(ov_spk);  ov_spk_p   = ov_spk; }
   if (ov_vol   != ov_vol_p)   { ss2voice_set_volume(ov_vol * 10); ov_vol_p = ov_vol; }
   if (ov_dub   != ov_dub_p)   { ss2voice_set_dub(ov_dub);         ov_dub_p = ov_dub; }
   if (ov_band  != ov_band_p)
   {  /* 띠는 화면 세로를 바꾼다 — 지오메트리를 다시 알려야 프론트가 안 깨진다 */
      if (!ss2comm_rom_is_ss2()) { ss2comm_sp_band(ov_band); ss2_set_geometry(); }
      ov_band_p = ov_band;
   }
   if (ov_chat  != ov_chat_p)  { ss2comm_set_enabled(ov_chat); ov_chat_p  = ov_chat; }
   if (ov_ref   != ov_ref_p)   { ss2comm_set_ref(ov_ref);      ov_ref_p   = ov_ref; }
   if (ov_sp    != ov_sp_p)
   {
      if (svcsp_rom_ok()) svcsp_set_engine(ov_sp != 0);   /* SvC: 원버튼 엔진만 토글 */
      else                ss2sp_enable = ov_sp != 0;
      ov_sp_p = ov_sp;
   }
   if (ov_toast != ov_toast_p) { svcsp_toast_on = ov_toast != 0; ov_toast_p = ov_toast; }
   if (ov_sides != ov_sides_p)
   {
      ss2_sides = ov_sides != 0;
      ss2_apply_comm_mode();       /* 기둥 상태에 따라 해설 위-띠 ↔ 화면 안 아래 */
      ss2_set_geometry();
      ov_sides_p = ov_sides;
   }
}

static void check_variables(void)
{
   struct retro_variable var = {0};

   var.key   = "ngp_language";
   var.value = NULL;

   if (!cv_booted && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      /* user must manually restart core for change to happen */
      if (!strcmp(var.value, "japanese"))
         setting_ngp_language = 0;
      else if (!strcmp(var.value, "english"))
         setting_ngp_language = 1;
   }

   var.key   = "ngp_ss2sp";
   var.value = NULL;
   if (!cv_booted && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      ss2sp_enable = strcmp(var.value, "disabled") ? true : false;

   var.key   = "ngp_svcsp_engine";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      svcsp_set_engine(strcmp(var.value, "disabled") ? 1 : 0);
   if (svcsp_rom_ok()) ov_sp = ov_sp_p = svcsp_engine_on() ? 1 : 0;

   var.key   = "ngp_svcsp_toast";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      svcsp_toast_on = strcmp(var.value, "disabled") ? true : false;
   ov_toast = ov_toast_p = svcsp_toast_on ? 1 : 0;

   var.key   = "ngp_svcsp_band";
   var.value = NULL;
   {  /* 기술명 띠 — 게임 그림 위에 겹치지 않고 화면 밖 띠에 기술 이름을 띄운다.
         해설이 없는 롬(SVC 등)용. SS2 는 해설 띠가 이미 그 자리를 쓴다. */
      int on = 1;
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
         on = strcmp(var.value, "disabled") != 0;
      ov_band = ov_band_p = (unsigned char)on;   /* 오버레이 그림자값을 옵션과 맞춘다 */
      if (!ss2comm_rom_is_ss2())
      {
         int prev = ss2comm_band_h();
         ss2comm_sp_band(on);
         if (ss2comm_band_h() != prev) ss2_set_geometry();
      }
      else ss2comm_sp_band(0);
   }

   var.key   = "ngp_framegen";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      fg_opt = !strcmp(var.value, "disabled") ? 0 : !strcmp(var.value, "enabled") ? 2 : 1;

   var.key   = "ngp_framegen_mode";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      fg_mode = !strcmp(var.value, "interp") ? 1 : 0;
   fg_rate_block = 0;                     /* 옵션을 만졌다 — 속도 감시 판정을 다시 한다 */

   cv_booted = true;

   /* 오버레이 그림자값을 방금 적용한 옵션과 맞춘다 */
   ov_sp    = ov_sp_p    = svcsp_rom_ok() ? (svcsp_engine_on() ? 1 : 0)
                                          : (ss2sp_enable ? 1 : 0);
   ov_sides = ov_sides_p = ss2_sides ? 1 : 0;
}

void retro_init(void)
{
   struct retro_log_callback log;
   char *dir = NULL;

   {  /* 링크 케이블 — 여기서 잡아 둔다. 한 프로세스에 코어 두 벌을 올리는 하네스가
         인스턴스마다 다른 파이프를 주려면, 각자 초기화하는 그 순간의 환경변수를 봐야 한다. */
      extern void ngplink_init(void);
      ngplink_init();
   }

   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else 
      log_cb = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir)
      strcpy(retro_base_directory, dir);
   else
   {
      /* TODO: Add proper fallback */
      if (log_cb)
         log_cb(RETRO_LOG_WARN, "System directory is not defined. Fallback on using same dir as ROM for system directory later ...\n");
   }
   
   /* If save directory is defined use it, otherwise use system directory */
   if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &dir) && dir)
      strcpy(retro_save_directory, dir);
   else
   {
      /* TODO: Add proper fallback */
      if (log_cb)
         log_cb(RETRO_LOG_WARN, "Save directory is not defined. Fallback on using SYSTEM directory ...\n");
      strcpy(retro_save_directory, retro_base_directory);
   }      

   perf_get_cpu_features_cb = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_PERF_INTERFACE, &perf_cb))
      perf_get_cpu_features_cb = perf_cb.get_cpu_features;

   if (environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
      libretro_supports_bitmasks = true;

   check_system_specs();
}

void retro_reset(void)
{
   fg_phase = 0; fg_audio_tail_n = 0; fg_streak = 0;   /* 프레임 생성 위상도 처음부터 */
   ss2sp_reset();
   svcsp_reset();
   ss2comm_set_ram(&CPUExRAM[0]);
   ss2comm_set_rom(ngpc_rom.orig_data, (unsigned)ngpc_rom.length);
   svcsp_set_rom(ngpc_rom.orig_data, (unsigned)ngpc_rom.length);  /* 해설 초상은 사용자 롬에서 그린다 */
   ss2comm_overlay_spmode(svcsp_rom_ok());
   if (svcsp_rom_ok())
   {  /* SvC — 해설·심판·기둥은 SS2 전용이라 감춘다 */
      ss2comm_overlay_bind(0, 0, 0, 0, 0, 0, 0, &ov_sp);
      ss2comm_overlay_bind_extra("기술명 표시", &ov_toast);   /* 음성 항목은 SS2 전용 — SvC 는 더빙이 없다 */
      ss2comm_overlay_bind_extra("기술명 띠", &ov_band);      /* 화면 밖 띠에 낼지, 게임 위에 겹칠지 */
      ov_sp = ov_sp_p = svcsp_engine_on() ? 1 : 0;
   }
   else
   {
      /* ★ 해설·해설자·심판·기둥·음성 **폐기** (유저 2026-09-07 「게임내 기둥버튼 제거 /
         해설같은거 코어메뉴도 정리」). 옵션 표에서 뺐어도 **여기 오버레이에는 그대로 떴다** —
         「정리했다」고 말한 자리가 앱 설정·코어 옵션 표·이 오버레이로 **세 겹**이었다.
         남기는 것은 원버튼 필살기와 기술명 표시 둘뿐이다. */
      ss2comm_overlay_bind(0, 0, 0, 0, 0, 0, 0, &ov_sp);
      ss2comm_overlay_bind_extra("기술명 표시", &ov_toast);
   }
   if (svcsp_rom_ok())
   {  /* 어떤 빌드가 도는지 화면으로 — "지원 문의: 옛 코어가 로드되는 사고" 방지 */
      static char ver_toast[48];
      snprintf(ver_toast, sizeof ver_toast, "SP %s", GIT_VERSION);
      ss2comm_toast(ver_toast, 180);
   }
   ss2comm_reset();
   neopop_reset();
}

bool retro_load_game_special(unsigned a, const struct retro_game_info *b, size_t c)
{
   return false;
}

#define MAX_PLAYERS 1
#define MAX_BUTTONS 7
static uint8_t input_buf;


bool retro_load_game(const struct retro_game_info *info)
{
   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "A" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "B" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Option" },
      /* v0.5: 버튼은 넷뿐이다 — A · B · A+B · SP.
         기술은 SP + 방향으로 나가고, 비오의는 뒤 + A+B 다. */
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,  "A + B (back + A+B = Super)" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,  "SP (with a direction)" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,  "SP (same as X)" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,  "A + B (same as Y)" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "Next commentator" },

      { 0 },
   };

   if (!info)
      return false;

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   overscan = false;
   environ_cb(RETRO_ENVIRONMENT_GET_OVERSCAN, &overscan);

   check_variables();
   check_color_depth();

   /* 프레임 생성 배관 초기화 — 지난 게임의 상태가 남지 않게. 목표 주사율은 지금 바로 판정해
      retro_get_system_av_info 가 처음부터 맞는 fps 를 돌려주게 한다(재초기화 한 번 절약) */
   fg_phase = 0; fg_streak = 0; fg_frames = 0; fg_runahead_seen = 0; fg_audio_tail_n = 0;
   fg_rate_block = 0; fg_win_sum = 0; fg_win_n = 0; fg_slow_secs = 0; fg_t_prev = 0;
   ss2fg_reset();
   fg_active = fg_want();

   if (Load(info->path, (const uint8_t*)info->data, info->size) <= 0)
      return false;

   MDFN_LoadGameCheats();
   MDFNMP_InstallReadPatches();

   /* 해설 엔진: 램·롬을 물리고 상태를 비운다 (retro_reset() 에도 같은 줄이 있다) */
   ss2comm_set_ram(&CPUExRAM[0]);
   ss2comm_set_rom(ngpc_rom.orig_data, (unsigned)ngpc_rom.length);
   svcsp_set_rom(ngpc_rom.orig_data, (unsigned)ngpc_rom.length);
   ss2comm_overlay_spmode(svcsp_rom_ok());
   if (svcsp_rom_ok())
   {  /* SvC — 해설·심판·기둥은 SS2 전용이라 감춘다 */
      ss2comm_overlay_bind(0, 0, 0, 0, 0, 0, 0, &ov_sp);
      ss2comm_overlay_bind_extra("기술명 표시", &ov_toast);   /* 음성 항목은 SS2 전용 — SvC 는 더빙이 없다 */
      ov_sp = ov_sp_p = svcsp_engine_on() ? 1 : 0;
   }
   else
   {
      /* ★ 해설·해설자·심판·기둥·음성 **폐기** (유저 2026-09-07 「게임내 기둥버튼 제거 /
         해설같은거 코어메뉴도 정리」). 옵션 표에서 뺐어도 **여기 오버레이에는 그대로 떴다** —
         「정리했다」고 말한 자리가 앱 설정·코어 옵션 표·이 오버레이로 **세 겹**이었다.
         남기는 것은 원버튼 필살기와 기술명 표시 둘뿐이다. */
      ss2comm_overlay_bind(0, 0, 0, 0, 0, 0, 0, &ov_sp);
      ss2comm_overlay_bind_extra("기술명 표시", &ov_toast);
   }
   if (svcsp_rom_ok())
   {  /* 어떤 빌드가 도는지 화면으로 — "지원 문의: 옛 코어가 로드되는 사고" 방지 */
      static char ver_toast[48];
      snprintf(ver_toast, sizeof ver_toast, "SP %s", GIT_VERSION);
      ss2comm_toast(ver_toast, 180);
   }
   ss2comm_reset();
   {  /* 해설 음성 팩 — <시스템 폴더>/ss2_voice_<언어>.pak
         해설 **텍스트표와 음성팩은 한 덩이**다. 재생 키가 문장 해시(fnv1a)라서
         표와 팩의 판이 어긋나면 조회가 전부 빗나가 그냥 조용해진다. 그래서 언어를
         파일 이름에 박아 섞이지 않게 한다. 옛 이름(ngpvoice.pak)도 계속 받는다. */
      const char *sd = NULL;
      static char vdir[560];
      if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &sd) && sd && *sd)
      {
         {  /* 언어는 여기서 직접 읽는다 — check_variables 가 언제 도는지에 기대지 않게 */
            struct retro_variable lv;
            lv.key = "ngp_ss2sp_comm_lang"; lv.value = NULL;
            if (!(environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &lv) && lv.value && *lv.value))
               lv.value = "ko";
            snprintf(vdir, sizeof vdir, "%s/ss2_voice_%s", sd, lv.value);
         }
         ss2voice_init(vdir);
         if (!ss2voice_on())                    /* 옛 배포본 하위호환 */
         {
            snprintf(vdir, sizeof vdir, "%s/ngpvoice", sd);
            ss2voice_init(vdir);
         }
      }
      else
         ss2voice_init(NULL);   /* SS2VOICE_DIR 환경변수는 init 안에서 우선 적용 */
      if (ss2voice_on())
      {  /* 어느 팩이 실렸는지 육안 확인 — 클립 수가 곧 팩 버전이다 */
         extern int ss2voice_count(void);
         static char vt[32];
         snprintf(vt, sizeof vt, "VOICE %d", ss2voice_count());
         ss2comm_toast(vt, 150);
      }
   }

   surf = (MDFN_Surface*)calloc(1, sizeof(*surf));

   if (!surf)
      return false;

   surf->width  = FB_WIDTH;
   surf->height = FB_HEIGHT;
   surf->pitch  = FB_WIDTH;
   surf->depth  = RETRO_PIX_DEPTH;

   surf->pixels = (uint16_t*)calloc(1, FB_WIDTH * FB_HEIGHT * sizeof(uint32_t));

   if (!surf->pixels)
   {
      free(surf);
      return false;
   }

   chee = (uint8 *)&input_buf;

   ngpgfx_set_pixel_format(NGPGfx, RETRO_PIX_DEPTH);
   MDFNNGPC_SetSoundRate();

   update_video = false;

   return true;
}

void retro_unload_game(void)
{
   MDFN_FlushGameCheats();

   rom_unload(persistent_data);
   if (NGPGfx)
      free(NGPGfx);
   NGPGfx = NULL;

   MDFNMP_Kill();

   if (surf)
   {
      if (surf->pixels)
         free(surf->pixels);
      free(surf);
   }
   surf            = NULL;

   persistent_data = false;
}

static void update_input(void)
{
   static unsigned map[] = {
      RETRO_DEVICE_ID_JOYPAD_UP,    /* X Cursor horizontal-layout games */
      RETRO_DEVICE_ID_JOYPAD_DOWN,  /* X Cursor horizontal-layout games */
      RETRO_DEVICE_ID_JOYPAD_LEFT,  /* X Cursor horizontal-layout games */
      RETRO_DEVICE_ID_JOYPAD_RIGHT, /* X Cursor horizontal-layout games */
      RETRO_DEVICE_ID_JOYPAD_B,
      RETRO_DEVICE_ID_JOYPAD_A,
      RETRO_DEVICE_ID_JOYPAD_START,
   };
   unsigned i, j;
   int16_t ret = 0;
   input_buf   = 0;

   if (libretro_supports_bitmasks)
      ret = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
   else
   {
      for (j = 0; j < (RETRO_DEVICE_ID_JOYPAD_R3+1); j++)
         if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, j))
            ret |= (1 << j);
   }

   for (i = 0; i < MAX_BUTTONS; i++)
      if ((map[i] != -1u) && (ret & (1 << map[i])))
         input_buf |= (1 << i);

   /* ── 빠른 설정 오버레이 — 아래+옵션 콤보로 여닫는다 (앱판과 동일).
      열려 있으면 방향·A·B·옵션은 오버레이 몫이고 게임엔 아무것도 안 간다. */
   {
      static int     ss2_combo_latch = 0;
      static int16_t ss2_ov_prev = 0;
      int toggled = 0;
      int down = (ret >> RETRO_DEVICE_ID_JOYPAD_DOWN) & 1;
      int opt  = (ret >> RETRO_DEVICE_ID_JOYPAD_START) & 1;
      if (down && opt)
      {
         if (!ss2_combo_latch)
         {
            ss2_combo_latch = 1;
            ss2comm_overlay_toggle();
            toggled = 1;
         }
      }
      else if (!down && !opt)
         ss2_combo_latch = 0;
      if (ss2comm_overlay_active())
      {
         if (!toggled)
         {
            int16_t edge = ret & ~ss2_ov_prev;
            if (edge & (1 << RETRO_DEVICE_ID_JOYPAD_UP))    ss2comm_overlay_input(0);
            if (edge & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))  ss2comm_overlay_input(1);
            if (edge & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))  ss2comm_overlay_input(2);
            if (edge & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)) ss2comm_overlay_input(3);
            if (edge & (1 << RETRO_DEVICE_ID_JOYPAD_B))     ss2comm_overlay_input(4); /* NGP A = 선택 */
            if (edge & (1 << RETRO_DEVICE_ID_JOYPAD_A))     ss2comm_overlay_input(5); /* NGP B = 뒤로 */
            if (edge & (1 << RETRO_DEVICE_ID_JOYPAD_START)) ss2comm_overlay_input(5);
         }
         ss2_ov_prev = ret;
         ss2_overlay_apply();
         input_buf = 0;
         return;
      }
      ss2_ov_prev = ret;
   }

   if (ss2sp_enable || svcsp_rom_ok())   /* SvC 입력은 SS2 전용 옵션에 매이지 않는다 */
   {
      /* v0.5: 트리거는 SP 하나뿐(X, 그리고 손이 편한 쪽을 위해 R 도 같은 자리).
         A+B 버튼(Y·L)은 패드 바이트에 A·B 를 한꺼번에 세워 준다 —
         뒤를 잡고 누르면 엔진이 비오의로 받는다. */
      uint16_t trig = 0;
      if (!svcsp_rom_ok())
      {  /* SS2 전용 선처리 — SvC 는 svcsp_frame 이 레트로패드 원본을 직접 해석한다 */
         if ((ret & (1 << RETRO_DEVICE_ID_JOYPAD_X)) ||
             (ret & (1 << RETRO_DEVICE_ID_JOYPAD_R)))
            trig |= 1u;
         if ((ret & (1 << RETRO_DEVICE_ID_JOYPAD_Y)) ||
             (ret & (1 << RETRO_DEVICE_ID_JOYPAD_L)))
            input_buf |= (uint8_t)((1 << 4) | (1 << 5));   /* A + B 동시 */
      }
      /* v0.7: L2 = 해설자 교대. 설정을 열지 않고 판 중에 다음 사람에게 넘긴다.
         누른 순간에만 한 번 — 누르고 있는 동안 계속 넘어가면 안 된다. */
      {
         static int comm_prev = 0;
         int now = (ret & (1 << RETRO_DEVICE_ID_JOYPAD_L2)) ? 1 : 0;
         if (now && !comm_prev)
         {
            int sp = ss2comm_next_speaker(1);
            ss2comm_notify(ss2comm_speaker_hello(sp));
         }
         comm_prev = now;
      }
      if (svcsp_rom_ok())
      {
         input_buf = svcsp_frame(input_buf, (uint16_t)ret);   /* SvC — 원본 비트를 직접 */
         {                                           /* 기술명 토스트 */
            static int disp_seen;
            if (svcsp_disp_seq != disp_seen)
            {
               disp_seen = svcsp_disp_seq;
               if (svcsp_toast_on && svcsp_last_disp[0])
                  ss2comm_toast(svcsp_last_disp, 80);
            }
         }
      }
      else
         input_buf = ss2sp_frame(input_buf, trig);
      if (ss2sp_card_block)      /* 카드가 없어 걸러진 순간 — 왜 안 나갔는지 알려 준다 */
      {
         ss2sp_card_block = 0;
         ss2comm_notify(SS2COMM_MSG_NOCARD);
      }
   }
}

void retro_run(void)
{
   int total = 0;
   unsigned width, height;
   static int16_t sound_buf[0x10000];
   EmulateSpecStruct spec;
   bool updated = false;
   int synth = 0, hidden = 0, ss2_paused;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      check_variables();

   fg_frames++;
   {  /* 프론트엔드 런어헤드/프리엠프티브의 숨은 호출(비디오 꺼짐) — 그건 'retro_run 1회 = 1프레임' 을
         전제하므로 2배 출력과 맞지 않는다. 자동 모드는 물러나고, 그동안도 위상은 그대로 번갈아
         (숨은 호출의 합성 차례엔 아무것도 돌리지 않는다) — 게임이 2배속이 되지는 않는다. */
      int av_en = 3;
      if (environ_cb(RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE, &av_en) && !(av_en & 1))
      {
         hidden = 1;
         if (fg_active) fg_runahead_seen = fg_frames;
      }
   }

   input_poll_cb();
   if (!hidden) fg_watch_rate();

   if (fg_active && fg_phase == 1)
      synth = 1;              /* 합성 호출 — 입력 변환(원버튼 매크로)은 실제 프레임마다 한 번만 돈다 */
   else
      update_input();

   spec.surface            = surf;
   spec.VideoFormatChanged = update_video;
   spec.DisplayRect.w      = 160;
   spec.DisplayRect.h      = 152;
   spec.SoundBufMaxSize    = sizeof(sound_buf) / 2;
   spec.SoundBufSize       = 0;

   if (update_video)
   {
      struct retro_system_av_info system_av_info;
      retro_get_system_av_info(&system_av_info);
      environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &system_av_info);
      surf->depth = RETRO_PIX_DEPTH;
      update_video = false;
   }

   if (!synth)
   {  /* 2배 출력 켜기/끄기 — 켤 때는 같은 결정이 30 실제 프레임 이어져야(AV 재초기화 비용),
         런어헤드가 보여 끌 때는 바로 */
      int want = fg_want();
      if (want != fg_active)
      {
         if (!want && fg_runahead_seen && fg_frames - fg_runahead_seen < 240) fg_apply(0);
         else if (++fg_streak >= 30) fg_apply(want);
      }
      else fg_streak = 0;
   }

   ss2_paused = ss2comm_overlay_active();

   if (!ss2_paused && !synth)
   {
      Emulate(&spec, sound_buf);

      width  = spec.DisplayRect.w;
      height = spec.DisplayRect.h;

      if (fg_active)
      {
         memcpy(fg_real, surf->pixels, sizeof fg_real);           /* 후처리 전 원본 — 합성 실패·보간 모드용 */
         if (fg_mode == 1)                                         /* 보간: N 의 그림을 N-1 쪽으로 반 되돌린 자리에 지금, N 은 다음 호출에 */
            ss2fg_render(ss2fg_cur(), ss2fg_prev(), 128, (uint16_t *)surf->pixels, FB_WIDTH, NGPGfx->ColorMap);
      }

      {
         const char *cline = ss2comm_frame();
         int band = ss2comm_band_h();
         uint16_t *fbuf = (uint16_t *)surf->pixels;
         if (cline && !ss2comm_drawing())   /* 코어가 직접 그릴 때는 알림을 겹쳐 띄우지 않는다 */
         {
            struct retro_message msg;
            msg.msg    = cline;
            msg.frames = 150;                 /* 약 2.5초 */
            environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
         }
         /* 띠를 화면 위에 붙이는 모드에서는 게임 그림을 띠 높이만큼 아래로 민다.
            (같은 버퍼 안에서 옮긴다 — surf->pixels 는 16bpp 기준 두 배로 잡혀 있다) */
         if (band && ss2comm_band_top())
            memmove(fbuf + band * FB_WIDTH, fbuf, (size_t)height * FB_WIDTH * sizeof(uint16_t));
         ss2comm_draw(fbuf, FB_WIDTH, (int)width, (int)height);
         height += band;
      }
      ss2_last_w = width;
      ss2_last_h = height;
   }
   else if (!ss2_paused)
   {
      /* 합성 프레임 — 게임은 돌리지 않는다. 띠·기둥 후처리는 실제 프레임과 같은 경로 */
      int band = ss2comm_band_h();
      uint16_t *fbuf = (uint16_t *)surf->pixels;
      width  = FB_WIDTH;
      height = FB_HEIGHT;
      if (hidden)
         memcpy(surf->pixels, fg_real, sizeof fg_real);            /* 숨은 호출: 아무것도 안 돌리고 지난 화면 */
      else if (fg_mode == 0 && !ngplink_active())                   /* 링크 플레이 중엔 예측이 통신을 먹는다 → 합성 없이 지난 화면 */
      {
         if (!fg_predict())                                        /* 합성 못 하면 실제 N 을 한 번 더 */
            memcpy(surf->pixels, fg_real, sizeof fg_real);
      }
      else
         memcpy(surf->pixels, fg_real, sizeof fg_real);            /* 보간 모드: 이제 실제 N */
      if (band && ss2comm_band_top())
         memmove(fbuf + band * FB_WIDTH, fbuf, (size_t)height * FB_WIDTH * sizeof(uint16_t));
      ss2comm_draw_hold(1);                                        /* 토스트 수명은 실제 프레임에서만 줄인다 */
      ss2comm_draw(fbuf, FB_WIDTH, (int)width, (int)height);
      ss2comm_draw_hold(0);
      height += band;
      spec.SoundBufSize = 0;
   }
   else
   {
      /* 빠른 설정이 열려 있다 — 앱판처럼 완전 일시정지: 에뮬·소리를 돌리지 않고
         마지막 화면 위에 설정창만 그린다 */
      width  = ss2_last_w;
      height = ss2_last_h;
      spec.SoundBufSize = 0;
      fg_audio_tail_n = 0;
   }

   if (ss2_sides)
   {
      /* 좌우 기둥 — 앱판과 같은 층: 대형 일러(주소표)·전황 연출.
         게임 그림을 가운데로 옮겨 넓은 캔버스(288px)로 내보낸다 */
      const uint16_t *fbuf = (const uint16_t *)surf->pixels;
      unsigned y;
      for (y = 0; y < height; y++)
         memcpy(ss2_wide + y * SS2_WIDE_W + SS2_SIDE_W, fbuf + y * FB_WIDTH,
                width * sizeof(uint16_t));
      ss2comm_side(ss2_wide, SS2_WIDE_W, SS2_SIDE_W, (int)height, 0);
      ss2comm_side(ss2_wide + SS2_SIDE_W + width, SS2_WIDE_W, SS2_SIDE_W, (int)height, 1);
      if (ss2_paused)
         ss2comm_overlay_draw(ss2_wide + SS2_SIDE_W, SS2_WIDE_W, (int)width, (int)height);
      video_cb(ss2_wide, SS2_SIDE_W * 2 + width, height, SS2_WIDE_W * 2);
   }
   else
   {
      if (ss2_paused)
         ss2comm_overlay_draw((uint16_t *)surf->pixels, FB_WIDTH, (int)width, (int)height);
      video_cb(surf->pixels, width, height, FB_WIDTH * 2);
   }

   /* 소리 — 2배 출력 중엔 실제 프레임의 샘플을 반씩 두 호출에 나눈다 */
   if (fg_active && !ss2_paused)
   {
      if (!synth)
      {
         int n = spec.SoundBufSize, half = n / 2;
         ss2voice_mix(sound_buf, n);                               /* 해설 음성 — 게임 소리 위에 */
         for (total = 0; total < half; )
            total += audio_batch_cb(sound_buf + total*2, half - total);
         fg_audio_tail_n = n - half;
         if (fg_audio_tail_n > (int)(sizeof(fg_audio_tail) / 4)) fg_audio_tail_n = sizeof(fg_audio_tail) / 4;
         memcpy(fg_audio_tail, sound_buf + half*2, (size_t)fg_audio_tail_n * 2 * sizeof(int16_t));
      }
      else
      {
         for (total = 0; total < fg_audio_tail_n; )
            total += audio_batch_cb(fg_audio_tail + total*2, fg_audio_tail_n - total);
         fg_audio_tail_n = 0;
      }
      fg_phase ^= 1;
   }
   else
   {
      fg_audio_tail_n = 0;
      fg_phase = 0;
      ss2voice_mix(sound_buf, spec.SoundBufSize);   /* 해설 음성 — 게임 소리 위에 */
      for (total = 0; total < spec.SoundBufSize; )
         total += audio_batch_cb(sound_buf + total*2, spec.SoundBufSize - total);
   }
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = MEDNAFEN_CORE_NAME;
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif

   info->need_fullpath    = true;
   info->library_version  = MEDNAFEN_CORE_VERSION GIT_VERSION;
   info->valid_extensions = MEDNAFEN_CORE_EXTENSIONS;
   info->block_extract    = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->timing.fps            = fg_active ? MEDNAFEN_CORE_TIMING_FPS * 2.0 : MEDNAFEN_CORE_TIMING_FPS;
   info->timing.sample_rate    = 44100;
   {
      int band = ss2comm_band_h();                   /* 해설 확장 띠(20px) 사용 시에만 > 0 */
      info->geometry.base_width   = ss2_sides ? SS2_WIDE_W : MEDNAFEN_CORE_GEOMETRY_BASE_W;
      info->geometry.base_height  = MEDNAFEN_CORE_GEOMETRY_BASE_H + band;
      info->geometry.max_width    = SS2_WIDE_W;
      info->geometry.max_height   = MEDNAFEN_CORE_GEOMETRY_MAX_H + SS2COMM_BAND_MAX;
      info->geometry.aspect_ratio = (float)info->geometry.base_width /
                                    (float)(MEDNAFEN_CORE_GEOMETRY_BASE_H + band);
   }

   check_color_depth();
}

void retro_deinit(void)
{
   if (surf)
   {
      if (surf->pixels)
         free(surf->pixels);
      free(surf);
   }
   surf = NULL;

   libretro_supports_bitmasks = false;
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned in_port, unsigned device)
{
}

void retro_set_environment(retro_environment_t cb)
{
   struct retro_vfs_interface_info vfs_iface_info;
   static const struct retro_system_content_info_override content_overrides[] = {
      {
         MEDNAFEN_CORE_EXTENSIONS, /* extensions */
#ifdef LOAD_FROM_MEMORY
         false, /* need_fullpath */
         true   /* persistent_data */
#else
         true,  /* need_fullpath */
         false  /* persistent_data */
#endif
      },
      { NULL, false, false }
   };
   environ_cb = cb;

   libretro_set_core_options(environ_cb);

   vfs_iface_info.required_interface_version = 1;
   vfs_iface_info.iface                      = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_iface_info))
	   filestream_vfs_init(&vfs_iface_info);
   /* Request a persistent content data buffer */
   environ_cb(RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE,
         (void*)content_overrides);
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

size_t retro_serialize_size(void)
{
   StateMem st;

   st.data           = NULL;
   st.loc            = 0;
   st.len            = 0;
   st.malloced       = 0;
   st.initial_malloc = 0;

   if (!MDFNSS_SaveSM(&st, 0, 0, NULL, NULL, NULL))
      return 0;

   free(st.data);

   return st.len;
}

bool retro_serialize(void *data, size_t size)
{
   StateMem st;
   bool ret          = false;
   uint8_t *_dat     = (uint8_t*)malloc(size);

   {  /* 프론트엔드 런어헤드/롤백용 저장인가 — 그러면 2배 출력 자동 모드는 물러난다 */
      int ctx = 0;
      if (environ_cb(RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT, &ctx) && ctx >= 1 && ctx <= 3)
         fg_runahead_seen = fg_frames ? fg_frames : 1;
   }

   if (!_dat)
      return false;

   /* Mednafen can realloc the buffer so we need to ensure this is safe. */
   st.data           = _dat;
   st.loc            = 0;
   st.len            = 0;
   st.malloced       = size;
   st.initial_malloc = 0;

   ret = MDFNSS_SaveSM(&st, 0, 0, NULL, NULL, NULL);

   memcpy(data, st.data, size);
   free(st.data);

   return ret;
}

bool retro_unserialize(const void *data, size_t size)
{
   ss2sp_reset();
   svcsp_reset();   /* 세이브스테이트 로드 시 매크로 잔여 상태 제거 */
   ss2fg_reset();   /* 이전 프레임 캡처도 버린다 — 불러온 상태와 보간하지 않게 */
   fg_audio_tail_n = 0;
   StateMem st;

   st.data           = (uint8_t*)data;
   st.loc            = 0;
   st.len            = size;
   st.malloced       = 0;
   st.initial_malloc = 0;

   MDFNSS_LoadSM(&st, 0, 0);

   return true;
}

void *retro_get_memory_data(unsigned type)
{
   if(type == RETRO_MEMORY_SYSTEM_RAM)
      return CPUExRAM;
   return NULL;
}

/* debug export: 프레임 생성 배관 상태 (스모크 하네스용) */
void retro_ngp_fg_state(int *active, int *phase, unsigned *frames)
{
   if (active) *active = fg_active;
   if (phase)  *phase  = fg_phase;
   if (frames) *frames = fg_frames;
}

/* debug export: K1GE VRAM pointers (portrait tile reversing) */
void *retro_ngp_charram(void)
{
   return NGPGfx ? NGPGfx->CharacterRAM : NULL;
}
void *retro_ngp_scrollram(void)
{
   return NGPGfx ? NGPGfx->ScrollVRAM : NULL;
}
void *retro_ngp_spriteram(void)
{
   return NGPGfx ? NGPGfx->SpriteVRAM : NULL;
}
void *retro_ngp_spritecol(void)
{
   return NGPGfx ? NGPGfx->SpriteVRAMColor : NULL;
}
void *retro_ngp_palram(void)
{
   return NGPGfx ? NGPGfx->ColorPaletteRAM : NULL;
}

size_t retro_get_memory_size(unsigned type)
{
   if(type == RETRO_MEMORY_SYSTEM_RAM)
      return 16384;
   return 0;
}

void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned a, bool b, const char *c) { }

/* Use a simpler approach to make sure that things go right for libretro. */
void MDFN_MakeFName(uint8_t type, char *s, size_t len,
      int id1, const char *cd1)
{
#ifdef _WIN32
   char slash = '\\';
#else
   char slash = '/';
#endif
   switch (type)
   {
      case MDFNMKF_SAV:
         snprintf(s, len, "%s%c%s%s%s", 
               retro_save_directory, slash, retro_base_name, ".",
               cd1);
	 if (log_cb)
	       log_cb(RETRO_LOG_INFO, "MDFN_MakeFName: %s\n", s);
         break;
      default:	  
         break;
   }
}
