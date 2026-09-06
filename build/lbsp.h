/* 월화의 검사(The Last Blade) 원버튼 필살기 엔진 — 프론트엔드용 선언.
   본체는 lbsp.c (순수 C). ss2sp.h · svcsp.h · kofsp.h 와 같은 규약이다.

   ★ 지금은 **M1(배관만)** 단계다. 게임 상수가 거의 다 미측정이고,
     lbsp_frame() 은 **순정 롬 폴드와 글자 그대로 똑같이** 접는다
     (Y=A · X=B · L·R=A+B). 즉 이 파일이 들어와도 월화 유저의 조작은
     한 비트도 안 바뀐다. 그것이 M1 의 통과 조건이다.

   ⚠ 다음 단계(M2)에서 **R 이 트리거가 되면서 폴드에서 빠진다.** 그때가
     출력이 처음 달라지는 순간이다 — 무회귀 시험의 기준선을 그 전에 떠 둬라. */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 매 프레임 한 번. pad = 지금까지 만들어진 NGP 패드 바이트,
   ret = 레트로패드 원본 비트마스크. 반환값이 실제로 게임에 들어간다. */
uint8_t lbsp_frame(uint8_t pad, uint16_t ret);

void lbsp_set_engine(int on);   /* 원버튼 엔진 토글 (기본 꺼짐) */
int  lbsp_engine_on(void);

/* 롬 로드·리셋·스테이트 로드 시 호출. 진행 중이던 커맨드를 버린다. */
void lbsp_reset(void);

/* 롬 로드 때 호출 — 헤더 0x24 의 "LASTBLADE" 로 판별해 둔다.
   ⚠ 한글패치본도 헤더가 같으므로 원본·한글판 모두에서 1 이 된다.
   ⚠ J판은 표식이 "GEKKA" 라 여기에 안 걸린다 — 지원 범위 밖이다(의도한 것). */
void lbsp_set_rom(const void *rom, unsigned len);
int  lbsp_rom_ok(void);          /* 1 = 지금 롬이 월화(UE) */

#ifdef SS2SP_RAM_POINTER
void lbsp_set_ram(void *ram);    /* 램 포인터를 밖에서 주는 빌드용 */
#endif

/* 아직 안 잰 것이 몇 개인지 — M1 회귀가 이걸 본다.
   0 이 되는 날이 오프셋 사냥이 끝난 날이다. */
int lbsp_unmeasured_count(void);

/* ── 기술명 표시 (svcsp·kofsp 와 같은 규약) ───────────────────────
   버퍼를 **먼저** 채우고 그다음에 seq 를 올린다 — 프론트가 seq 엣지를 보고 버퍼를
   읽으므로 순서가 뒤바뀌면 옛 문자열을 읽는다. UTF-8. */
extern char lbsp_last_disp[64];
extern int  lbsp_disp_seq;

#ifdef __cplusplus
}
#endif
