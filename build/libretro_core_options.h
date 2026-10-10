#ifndef LIBRETRO_CORE_OPTIONS_H__
#define LIBRETRO_CORE_OPTIONS_H__

#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <retro_inline.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 ********************************
 * Core Option Definitions
 ********************************
*/

/* RETRO_LANGUAGE_ENGLISH */

/* Default language:
 * - All other languages must include the same keys and values
 * - Will be used as a fallback in the event that frontend language
 *   is not available
 * - Will be used as a fallback for any missing entries in
 *   frontend language definition */

#define MAX_CORE_OPTIONS 32

struct retro_core_option_v2_category option_cats_us[] = {
   {"system",  "System",   NULL},
   {"video",   "Video",    NULL},
   //{"audio",   "Audio",    NULL},
   {"input",   "Input",    NULL},
   //{"advanced","Advanced", NULL},
   { NULL,     NULL,       NULL },
};

struct retro_core_option_v2_definition option_defs_us_v2[] = {
   {
      "ngp_language",
      "Language (*)",
      NULL,
      "Language games should display text in.\n(*) Core restart required.",
      NULL,
      "system",
      {
         { "english",  NULL },
         { "japanese",  NULL },
         { NULL, NULL},
      },
      "japanese",
   },
   {
      "ngp_ss2sp",
      "SS2 One-button Specials",
      NULL,
      "Samurai Shodown! 2 only. Maps unused pad buttons to the current character's special moves. The core reads facing direction from RAM and mirrors the command automatically.",
      NULL,
      "input",
      {
         { "enabled",  "Enabled" },
         { "disabled", "Disabled" },
         { NULL, NULL},
      },
      "enabled",
   },
   {
      "ngp_svcsp_engine",
      "SvC 원버튼 필살기",
      NULL,
      "기술 버튼 하나로 필살기가 나갑니다. 방향을 잡고 누르면 그 방향 기술. (SNK vs. Capcom MotM 전용)",
      NULL,
      "system",
      {
         { "enabled",  "켬" },
         { "disabled", "끔" },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "ngp_svcsp_toast",
      "SvC 기술명 표시",
      NULL,
      "원버튼으로 기술이 나갈 때 기술명과 커맨드(화살표+버튼)를 화면 위에 잠깐 띄웁니다. (SNK vs. Capcom MotM 전용)",
      NULL,
      "system",
      {
         { "enabled",  "켬" },
         { "disabled", "끔" },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "ngp_framegen",
      "프레임 생성 (120Hz)",
      NULL,
      "60fps 게임을 120Hz 화면에 맞춰 중간 프레임을 합성합니다. 픽셀을 섞지 않고 스프라이트·스크롤 위치를 보간해 같은 타일로 다시 그립니다. '자동'은 RetroArch 의 주사율 설정(Vertical Refresh Rate)이 120Hz(또는 짝수 배)일 때만 켭니다. 런어헤드(Run-Ahead)와는 같이 못 씁니다 — 런어헤드가 보이면 자동은 꺼집니다. 60Hz 화면에서 '켬'으로 두면 화면이 찢어집니다.",
      NULL,
      "video",
      {
         { "auto",     "자동 (120Hz 설정일 때)" },
         { "disabled", "끔" },
         { "enabled",  "켬" },
         { NULL, NULL },
      },
      "auto"
   },
   {
      "ngp_framegen_mode",
      "프레임 생성 방식",
      NULL,
      "예측: 다음 프레임을 미리 돌려 현재↔다음 사이를 그립니다 — 추가 지연 없음(런어헤드 원리, 입력이 바뀌는 순간만 반 프레임 어긋날 수 있음). 보간: 이전↔현재 사이를 먼저 보여 줘 표시가 반 프레임(8ms) 늦습니다.",
      NULL,
      "video",
      {
         { "predict", "예측 (지연 없음)" },
         { "interp",  "보간 (+8ms)" },
         { NULL, NULL },
      },
      "predict"
   },
   {
      "ngp_framegen_mult",
      "프레임 생성 배수",
      NULL,
      "4배: 사무쇼2 는 캐릭터·배경을 2 프레임에 한 번(초당 30번) 움직이므로, 다음에 바뀌는 프레임까지 미리 돌려 120Hz 네 장에 고르게 나눕니다. 예측 방식 = 두 프레임 앞까지 미리 돌림(지연 없음, 계산 약 1.5배). 보간 방식 = 한 프레임만 미리 돌리고 반 프레임 늦게 보여 줌(지연 +8ms 그대로, 계산은 예측 2배와 같음). 2배: 실제 프레임마다 반 프레임 지점 하나만 끼웁니다.",
      NULL,
      "video",
      {
         { "4", "4배 (게임 박자 맞춤)" },
         { "2", "2배" },
         { NULL, NULL },
      },
      "4"
   },
   {
      "ngp_runahead",
      "런어헤드 (입력 지연 줄이기)",
      NULL,
      "사무쇼2 는 버튼을 누르고 대개 2~3 프레임 뒤에 화면이 바뀝니다. 그만큼 미리 돌려 둔 그림을 보여 줘서 반응을 앞당깁니다. 2 = 33ms 빨라짐(입력을 바꾸는 순간 아주 가끔 한 프레임 이전 동작이 더 보임), 1 = 17ms(거의 정확). 계산이 늘고, 소리는 그림보다 그만큼 늦게 들립니다. 사무쇼2 롬에서만, 링크 플레이·2배 프레임 생성 중엔 꺼집니다.",
      NULL,
      "input",
      {
         { "2", "2 프레임 (33ms)" },
         { "1", "1 프레임 (17ms)" },
         { "0", "끔" },
         { NULL, NULL },
      },
      "2"
   },
   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },
};

struct retro_core_options_v2 options_us = {
   option_cats_us,
   option_defs_us_v2
};

struct retro_core_option_definition option_defs_us[] = {
   {
      "ngp_language",
      "Language (*)",
      "Language games should display text in.\n(*) Core restart required.",
      {
         { "english",  NULL },
         { "japanese",  NULL },
         { NULL, NULL},
      },
      "japanese",
   },
   {
      "ngp_ss2sp",
      "SS2 One-button Specials",
      "Samurai Shodown! 2 only. Maps unused pad buttons to the current character's special moves.",
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL},
      },
      "enabled",
   },
   {
      "ngp_framegen",
      "프레임 생성 (120Hz)",
      "60fps 게임을 120Hz 화면에 맞춰 중간 프레임을 합성합니다. '자동'은 RetroArch 주사율 설정이 120Hz 일 때만 켭니다.",
      {
         { "auto",     NULL },
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "auto",
   },
   {
      "ngp_framegen_mode",
      "프레임 생성 방식",
      "예측(지연 없음) 또는 보간(+8ms).",
      {
         { "predict", NULL },
         { "interp",  NULL },
         { NULL, NULL},
      },
      "predict",
   },
   {
      "ngp_framegen_mult",
      "프레임 생성 배수",
      "4배(게임 박자 맞춤) 또는 2배.",
      {
         { "4", NULL },
         { "2", NULL },
         { NULL, NULL},
      },
      "4",
   },
   {
      "ngp_runahead",
      "런어헤드 (입력 지연 줄이기)",
      "미리 돌려 둔 그림을 보여 줘서 반응을 앞당긴다(사무쇼2).",
      {
         { "2", NULL },
         { "1", NULL },
         { "0", NULL },
         { NULL, NULL},
      },
      "2",
   },
   { NULL, NULL, NULL, { NULL, NULL }, NULL },
};

/* RETRO_LANGUAGE_JAPANESE */

/* RETRO_LANGUAGE_FRENCH */

/* RETRO_LANGUAGE_SPANISH */

/* RETRO_LANGUAGE_GERMAN */

/* RETRO_LANGUAGE_ITALIAN */

/* RETRO_LANGUAGE_DUTCH */

/* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */

/* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */

/* RETRO_LANGUAGE_RUSSIAN */

/* RETRO_LANGUAGE_KOREAN */

/* RETRO_LANGUAGE_CHINESE_TRADITIONAL */

/* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */

/* RETRO_LANGUAGE_ESPERANTO */

/* RETRO_LANGUAGE_POLISH */

/* RETRO_LANGUAGE_VIETNAMESE */

/* RETRO_LANGUAGE_ARABIC */

/* RETRO_LANGUAGE_GREEK */

/* RETRO_LANGUAGE_TURKISH */

/*
 ********************************
 * Language Mapping
 ********************************
*/

struct retro_core_option_definition *option_defs_intl[RETRO_LANGUAGE_LAST] = {
   option_defs_us, /* RETRO_LANGUAGE_ENGLISH */
   NULL,           /* RETRO_LANGUAGE_JAPANESE */
   NULL,           /* RETRO_LANGUAGE_FRENCH */
   NULL,           /* RETRO_LANGUAGE_SPANISH */
   NULL,           /* RETRO_LANGUAGE_GERMAN */
   NULL,           /* RETRO_LANGUAGE_ITALIAN */
   NULL,           /* RETRO_LANGUAGE_DUTCH */
   NULL,           /* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */
   NULL,           /* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */
   NULL,           /* RETRO_LANGUAGE_RUSSIAN */
   NULL,           /* RETRO_LANGUAGE_KOREAN */
   NULL,           /* RETRO_LANGUAGE_CHINESE_TRADITIONAL */
   NULL,           /* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */
   NULL,           /* RETRO_LANGUAGE_ESPERANTO */
   NULL,           /* RETRO_LANGUAGE_POLISH */
   NULL,           /* RETRO_LANGUAGE_VIETNAMESE */
   NULL,           /* RETRO_LANGUAGE_ARABIC */
   NULL,           /* RETRO_LANGUAGE_GREEK */
   NULL,           /* RETRO_LANGUAGE_TURKISH */
};

struct retro_core_options_v2 *options_intl[RETRO_LANGUAGE_LAST] = {
   &options_us, /* RETRO_LANGUAGE_ENGLISH */
   NULL,        /* RETRO_LANGUAGE_JAPANESE */
   NULL,        /* RETRO_LANGUAGE_FRENCH */
   NULL,        /* RETRO_LANGUAGE_SPANISH */
   NULL,        /* RETRO_LANGUAGE_GERMAN */
   NULL,        /* RETRO_LANGUAGE_ITALIAN */
   NULL,        /* RETRO_LANGUAGE_DUTCH */
   NULL,        /* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */
   NULL,        /* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */
   NULL,        /* RETRO_LANGUAGE_RUSSIAN */
   NULL,        /* RETRO_LANGUAGE_KOREAN */
   NULL,        /* RETRO_LANGUAGE_CHINESE_TRADITIONAL */
   NULL,        /* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */
   NULL,        /* RETRO_LANGUAGE_ESPERANTO */
   NULL,        /* RETRO_LANGUAGE_POLISH */
   NULL,        /* RETRO_LANGUAGE_VIETNAMESE */
   NULL,        /* RETRO_LANGUAGE_ARABIC */
   NULL,        /* RETRO_LANGUAGE_GREEK */
   NULL,        /* RETRO_LANGUAGE_TURKISH */
};

/*
 ********************************
 * Functions
 ********************************
*/

/* Handles configuration/setting of core options.
 * Should only be called inside retro_set_environment().
 * > We place the function body in the header to avoid the
 *   necessity of adding more .c files (i.e. want this to
 *   be as painless as possible for core devs)
 */

static INLINE void libretro_set_core_options(retro_environment_t environ_cb)
{
   unsigned version = 0;

   if (!environ_cb)
      return;

   if (!environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version))
      version = 0;

   if (version >= 2)
   {
      struct retro_core_options_v2_intl core_options_intl;
      unsigned language = 0;

      core_options_intl.us    = &options_us;
      core_options_intl.local = NULL;

      if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
          (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH))
         core_options_intl.local = options_intl[language];

      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL, &core_options_intl);
   }
   else if (version >= 1)
   {
      struct retro_core_options_intl core_options_intl;
      unsigned language = 0;

      core_options_intl.us    = option_defs_us;
      core_options_intl.local = NULL;

      if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
          (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH))
         core_options_intl.local = option_defs_intl[language];

      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL, &core_options_intl);
   }
   else
   {
      size_t i;
      size_t option_index              = 0;
      size_t num_options               = 0;
      struct retro_variable *variables = NULL;
      char **values_buf                = NULL;

      /* Determine number of options
       * > Note: We are going to skip a number of irrelevant
       *   core options when building the retro_variable array,
       *   but we'll allocate space for all of them. The difference
       *   in resource usage is negligible, and this allows us to
       *   keep the code 'cleaner' */
      while (true)
      {
         if (option_defs_us[num_options].key)
            num_options++;
         else
            break;
      }

      /* Allocate arrays */
      variables  = (struct retro_variable *)calloc(num_options + 1, sizeof(struct retro_variable));
      values_buf = (char **)calloc(num_options, sizeof(char *));

      if (!variables || !values_buf)
         goto error;

      /* Copy parameters from option_defs_us array */
      for (i = 0; i < num_options; i++)
      {
         const char *key                        = option_defs_us[i].key;
         const char *desc                       = option_defs_us[i].desc;
         const char *default_value              = option_defs_us[i].default_value;
         struct retro_core_option_value *values = option_defs_us[i].values;
         size_t buf_len                         = 3;
         size_t default_index                   = 0;

         values_buf[i] = NULL;

         /* Skip options that are irrelevant when using the
          * old style core options interface */
         if ((strcmp(key, "fceumm_advance_sound_options") == 0))
            continue;

         if (desc)
         {
            size_t num_values = 0;

            /* Determine number of values */
            while (true)
            {
               if (values[num_values].value)
               {
                  /* Check if this is the default value */
                  if (default_value)
                     if (strcmp(values[num_values].value, default_value) == 0)
                        default_index = num_values;

                  buf_len += strlen(values[num_values].value);
                  num_values++;
               }
               else
                  break;
            }

            /* Build values string */
            if (num_values > 1)
            {
               size_t j;

               buf_len += num_values - 1;
               buf_len += strlen(desc);

               values_buf[i] = (char *)calloc(buf_len, sizeof(char));
               if (!values_buf[i])
                  goto error;

               strcpy(values_buf[i], desc);
               strcat(values_buf[i], "; ");

               /* Default value goes first */
               strcat(values_buf[i], values[default_index].value);

               /* Add remaining values */
               for (j = 0; j < num_values; j++)
               {
                  if (j != default_index)
                  {
                     strcat(values_buf[i], "|");
                     strcat(values_buf[i], values[j].value);
                  }
               }
            }
         }

         variables[option_index].key   = key;
         variables[option_index].value = values_buf[i];
         option_index++;
      }

      /* Set variables */
      environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);

error:

      /* Clean up */
      if (values_buf)
      {
         for (i = 0; i < num_options; i++)
         {
            if (values_buf[i])
            {
               free(values_buf[i]);
               values_buf[i] = NULL;
            }
         }

         free(values_buf);
         values_buf = NULL;
      }

      if (variables)
      {
         free(variables);
         variables = NULL;
      }
   }
}

#ifdef __cplusplus
}
#endif

#endif
