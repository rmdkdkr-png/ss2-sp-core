/* SS2 원버튼 필살기 엔진 — 프론트엔드용 선언.
   본체는 ss2sp.c (순수 C)이며 libretro 판과 동일한 파일이다.

   C++20 모듈(system.ccm)에서 쓰려면 이 헤더를 **global module fragment**
   (`module;` 과 `export module` 사이)에서 include 해야 한다.
   모듈 purview 안에 extern "C" 선언을 직접 쓰면 링키지가 꼬인다. */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 매 프레임 한 번. pad = 현재 NGP 패드 바이트, trig = SP1~SP6 눌림 비트.
   반환값을 그대로 0x6F82 에 쓰면 된다. */
uint8_t ss2sp_frame(uint8_t pad, uint16_t trig);
extern int ss2sp_card_block;   /* 카드가 없어 SP 발동을 거른 순간 1 (프런트엔드가 읽고 0으로) */

/* 롬 로드·리셋·스테이트 로드 시 호출. 진행 중이던 커맨드를 버린다. */
void ss2sp_reset(void);

/* 0 = 버튼 직결(SP1~6 = 필살기 1~6), 1 = SP + 방향으로 슬롯 선택 */
void ss2sp_set_layout(int sp);

/* 부팅 때 한 번. SYSTEM_RAM(16KB) 시작 주소를 넘긴다. */
void ss2sp_set_ram(void *ram);


/* ── 기술 배치 커스텀 ────────────────────────────────────────────
   유파 30개 × 슬롯 7개(중립·앞·뒤·아래·↘·↙·공중). 값은 기술 번호, -1 = 없음.
   저장은 ss2sp_slots_blob() 210바이트를 그대로 설정 파일에 넣으면 된다. */
int  ss2sp_style_count(void);
int  ss2sp_slot_count(void);
int  ss2sp_slots_size(void);
const char *ss2sp_style_id(int style);
int  ss2sp_cur_style(void);              /* 전투 중이 아니면 -1 */
int  ss2sp_move_count(int style);
const char *ss2sp_move_name(int style, int i);
int  ss2sp_move_btn(int style, int i);   /* 16 = A(약베기), 32 = B(강베기) */
int  ss2sp_move_flags(int style, int i); /* 1근접 2카드 4공중 8미검증 16잡기 */
int  ss2sp_move_notation(int style, int i, char *out, int cap);  /* "236+A" */
int  ss2sp_get_slot(int style, int slot);
void ss2sp_set_slot(int style, int slot, int mv);
void ss2sp_reset_slots(void);
void ss2sp_slots_blob(unsigned char *out);
void ss2sp_load_slots(const unsigned char *in);

#ifdef __cplusplus
}
#endif
