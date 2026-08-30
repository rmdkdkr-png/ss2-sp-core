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
   //{"video",   "Video",    NULL},
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
      "ngp_ss2sp_comm",
      "SS2 Character Commentary",
      NULL,
      "Samurai Shodown! 2 only. One of 15 characters comments on the match: KO, perfect win, comeback, win streak, story progress and menu chatter. Press L2 during play to hand over to the next commentator. Korean text.",
      NULL,
      "video",
      {
         { "enabled",  "Enabled" },
         { "disabled", "Disabled" },
         { NULL, NULL},
      },
      "enabled",
   },
   {
      "ngp_ss2sp_comm_spk",
      "SS2 Commentary Speaker",
      NULL,
      "Who does the talking.",
      NULL,
      "video",
      {
         { "haohmaru",  "Haohmaru" },
         { "nakoruru",  "Nakoruru" },
         { "hanzo",     "Hanzo" },
         { "galford",   "Galford" },
         { "rimururu",  "Rimururu" },
         { "genjuro",   "Genjuro" },
         { "ukyo",      "Ukyo" },
         { "charlotte", "Charlotte" },
         { "jubei",     "Jubei" },
         { "kazuki",    "Kazuki" },
         { "sogetsu",   "Sogetsu" },
         { "asura",     "Asura" },
         { "shiki",     "Shiki" },
         { "morozumi",  "Morozumi" },
         { "yuga",      "Yuga" },
         { NULL, NULL},
      },
      "haohmaru",
   },
   {
      "ngp_ss2sp_comm_duo",
      "SS2 Commentary Partner",
      NULL,
      "In big moments a second character answers the commentator in one short line - KO, super move, comeback, danger, and the end-of-match verdict. Turn it off if one voice is enough.",
      NULL,
      "video",
      {
         { "enabled",  "Enabled" },
         { "disabled", "Disabled" },
         { NULL, NULL},
      },
      "enabled",
   },
   {
      "ngp_ss2sp_comm_draw",
      "SS2 Commentary Display",
      NULL,
      "Outside the screen (default): the core adds a 32px band above or below the game image and draws the line there with its own Korean pixel font (Galmuri 11px) plus the speaker's portrait - the game image is never covered, but the picture gets taller. Inside the screen: draws a dialogue box over the game image instead, so the console screen size and aspect stay exactly as they were. Frontend notification only: no drawing, uses RetroArch OSD (Korean may not render, depends on the frontend font).",
      NULL,
      "video",
      {
         { "above",         "Outside the screen, above" },
         { "enabled",       "Outside the screen, below" },
         { "inside_top",    "Inside the screen, top" },
         { "inside_bottom", "Inside the screen, bottom" },
         { "disabled",      "Frontend notification only" },
         { NULL, NULL},
      },
      "above",
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
      "ngp_ss2sp_sides",
      "SS2 Side Art Pillars",
      NULL,
      "Adds 64px pillars on both sides of the game image (288px wide output). Each pillar shows the fighter's large card illustration, rebuilt at runtime from your own ROM - the core ships only tile address tables, no artwork. The pillars react to the match: shake on hit, white flash on heavy damage, red tint when near death, grayscale on KO. Includes the Quick Settings overlay (hold Down + Option) with one-button special move assignment.",
      NULL,
      "video",
      {
         { "enabled",  "Enabled" },
         { "disabled", "Disabled" },
         { NULL, NULL},
      },
      "enabled",
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
