/* SvC MotM 원버튼 필살기 엔진 — 프론트엔드용 선언.
   본체는 svcsp.c (순수 C). ss2sp.h 와 같은 규약이다. */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 매 프레임 한 번. pad = 현재 NGP 패드 바이트, trig = SP 트리거 비트(bit0).
   반환값이 실제로 게임에 들어갈 패드 바이트다. */
uint8_t svcsp_frame(uint8_t pad, uint16_t ret);
uint8_t svcsp_frame_app(uint8_t pad, uint16_t trig);   /* 앱용: trig bit0 = 기술키 */
void    svcsp_set_engine(int on);   /* 원버튼 엔진 토글 (기본 꺼짐) */
int     svcsp_engine_on(void);

/* 롬 로드·리셋·스테이트 로드 시 호출. 진행 중이던 커맨드를 버린다. */
void svcsp_reset(void);

/* 롬 로드 때 호출 — 헤더(0x24 "SNKvsCAPCOM1")로 SvC 인지 판별해 둔다. */
void svcsp_set_rom(const void *rom, unsigned len);
int  svcsp_rom_ok(void);          /* 1 = 지금 롬이 SvC MotM */

#ifdef SS2SP_RAM_POINTER
void svcsp_set_ram(void *ram);    /* NGP.emu 쪽에서 부팅 때 한 번 */
#endif

/* 디버그/표시용 */
extern const char *svcsp_last_name;
extern int         svcsp_last_ok;     /* -1 미판정 · 0 불발 · 1 발동 */
extern int         svcsp_last_strong; /* 마지막 발동이 강(홀드)이었는지 */
extern char        svcsp_last_disp[64]; /* "황물기 ↓↘→+P" — 표시용 */
extern int         svcsp_disp_seq;      /* 새 발동마다 +1 */

/* 오버레이 메뉴용 (ss2sp 의 style API 대응) */
int  svcsp_char_count(void);
const char *svcsp_char_name(int c);
int  svcsp_cur_char(void);            /* 전투 중이 아니면 -1 */
int  svcsp_move_count(int c);
const char *svcsp_move_name(int c, int i);
int  svcsp_move_flags(int c, int i);
int  svcsp_move_notation(int c, int i, char *out, int cap);
int  svcsp_get_slot(int c, int k);
void svcsp_set_slot(int c, int k, int mv);
void svcsp_reset_slots(void);

/* 슬롯 배치 저장/복원 — 파일 IO 는 프론트 몫 (<system>/ngpsvc_slots.bin) */
int  svcsp_slots_dirty(void);                              /* 읽으면 플래그가 접힌다 */
int  svcsp_slots_export(unsigned char *buf, int cap);
void svcsp_slots_import(const unsigned char *buf, int len);

#ifdef __cplusplus
}
#endif
