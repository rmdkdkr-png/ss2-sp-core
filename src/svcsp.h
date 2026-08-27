/* SvC MotM 원버튼 필살기 엔진 — 프론트엔드용 선언.
   본체는 svcsp.c (순수 C). ss2sp.h 와 같은 규약이다. */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 매 프레임 한 번. pad = 현재 NGP 패드 바이트, trig = SP 트리거 비트(bit0).
   반환값이 실제로 게임에 들어갈 패드 바이트다. */
uint8_t svcsp_frame(uint8_t pad, uint16_t trig);

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
extern int         svcsp_last_ok;   /* -1 미판정 · 0 불발 · 1 발동 */

#ifdef __cplusplus
}
#endif
