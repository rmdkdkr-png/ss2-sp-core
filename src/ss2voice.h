/* ss2voice — 해설 음성 팩 재생기.
   팩 폴더(manifest.tsv + *.ogg)가 없으면 모든 함수가 조용히 무시된다. */
#ifndef SS2VOICE_H
#define SS2VOICE_H

#include <stdint.h>

void ss2voice_init(const char *dir);              /* NULL 또는 없는 폴더 = 비활성 */
void ss2voice_say(const char *text, int prio);    /* prio 1=심판(끼어듦) 0=해설 */
void ss2voice_mix(int16_t *buf, int frames);      /* 44.1kHz 스테레오 s16 에 가산 */
int  ss2voice_on(void);
int  ss2voice_has_text(const char *text);

#endif
