/* KOF R-2 원버튼 필살기 엔진 — 프론트엔드용 선언.
   본체는 kofsp.c (순수 C). svcsp.h · ss2sp.h 와 같은 규약이다.

   ★ 지금은 **M1(배관만)** 단계다. 게임 상수가 하나도 안 측정됐고,
     kofsp_frame() 은 **순정 롬 폴드와 똑같이 동작한다** (Y=A · X=B · L·R=A+B).
     즉 이 파일이 들어와도 KOF 유저의 조작은 한 비트도 안 바뀐다.
     그게 M1 의 통과 조건이다 — 게임 넷(SvC·SS2·순정·KOF)의 출력이 전부 비트 동일. */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 매 프레임 한 번. pad = 지금까지 만들어진 NGP 패드 바이트,
   ret = 레트로패드 원본 비트마스크. 반환값이 실제로 게임에 들어간다. */
uint8_t kofsp_frame(uint8_t pad, uint16_t ret);

void kofsp_set_engine(int on);   /* 원버튼 엔진 토글 (기본 꺼짐) */
int  kofsp_engine_on(void);

/* 롬 로드·리셋·스테이트 로드 시 호출. 진행 중이던 커맨드를 버린다. */
void kofsp_reset(void);

/* 롬 로드 때 호출 — 헤더 0x24 의 "KOF R2" 로 판별해 둔다.
   ⚠ 한글패치본도 헤더가 같으므로 원본·한글판 모두에서 1 이 된다. */
void kofsp_set_rom(const void *rom, unsigned len);
int  kofsp_rom_ok(void);          /* 1 = 지금 롬이 KOF R-2 */

#ifdef SS2SP_RAM_POINTER
void kofsp_set_ram(void *ram);    /* 램 포인터를 밖에서 주는 빌드용 */
#endif

/* 아직 안 잰 것이 무엇인지 밖에서 물어볼 수 있게 — M1 회귀가 이걸 본다.
   0 이 되는 날이 오프셋 사냥이 끝난 날이다. */
int kofsp_unmeasured_count(void);

#ifdef __cplusplus
}
#endif
