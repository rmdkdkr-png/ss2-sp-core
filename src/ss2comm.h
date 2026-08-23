/* ss2comm — 사쇼!2 캐릭터 해설 엔진 (프런트엔드 공용)
   RAM만 읽어 이벤트를 잡고 대사 문자열(UTF-8)을 돌려준다. 표시는 프런트엔드가 한다.
   · libretro 코어 : ss2comm_frame() → RETRO_ENVIRONMENT_SET_MESSAGE 또는 ss2comm_draw()
   · NGP.emu (APK) : ss2comm_frame() → app.postMessage()
   브라우저판은 같은 로직의 JS 구현(runner.html)을 쓴다 — 이 파일은 그 이식본이다. */
#ifndef SS2COMM_H
#define SS2COMM_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

void        ss2comm_set_ram(void *p);        /* SS2SP_RAM_POINTER 빌드에서만 필요 */
void        ss2comm_set_enabled(int on);     /* 코어 옵션 */
void        ss2comm_set_rom(const void *rom, unsigned len); /* 초상(얼굴)을 사용자 롬에서 그린다 */
void        ss2comm_set_speaker(int idx);    /* 0 = 하오마루. 순서는 ss2comm_speaker_name() 으로 */
int         ss2comm_speaker_count(void);     /* 해설자 수 (v0.7: 15) */
const char *ss2comm_speaker_name(int idx);   /* 화자 이름 (UTF-8) */
const char *ss2comm_speaker_hello(int idx);  /* 화자 인사 한마디 */
int         ss2comm_get_speaker(void);
int         ss2comm_next_speaker(int step);  /* 다음(또는 이전) 해설자로. 새 번호를 돌려준다 */
void        ss2comm_reset(void);             /* 롬 로드/리셋 시 */
void        ss2comm_notify(const char *text);/* 해설 자리에 안내 한 줄 */
/* 카드가 없어 SP 발동을 거른 경우의 안내. 글리프 추출이 이 파일도 훑으므로 여기에 둔다. */
#define SS2COMM_MSG_NOCARD "카드가 없다 \xe2\x80\x94 그 기술은 못 낸다"
const char *ss2comm_frame(void);             /* 매 프레임 1회. 새 대사면 문자열, 아니면 NULL */
const char *ss2comm_current(int *age_frames);/* 현재 표시 중인 대사(자체 렌더용) */
void        ss2comm_draw_enable(int mode);   /* 0 끔 1 확장띠 2 상단겹침 3 하단겹침 */
int         ss2comm_band_h(void);            /* 화면 밖 띠 모드에서 늘려야 할 세로 픽셀 */
int         ss2comm_band_top(void);          /* 1이면 띠가 게임 화면 위에 붙는다 */
int         ss2comm_impact(void);            /* 지금 줄이 강조인가 — 진동·연출용 */
int         ss2comm_drawing(void);           /* 1이면 코어가 직접 그린다(알림 불필요) */
void        ss2comm_draw(uint16_t *fb, int pitch_px, int w, int h); /* 화면에 직접 그리기 */

#ifdef __cplusplus
}
#endif
#endif
