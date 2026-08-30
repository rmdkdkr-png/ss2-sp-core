/* ss2sfx — 원샷 효과음 재생기 (기획: ~/.claude/plans 의 효과음 계획)

   해설 재생기(ss2voice)와 **따로 둔다.** 요구가 정반대라서다:
     해설  드물게 · 길게 · 자막과 동기 · 안 겹치게 · 늦으면 버려도 됨
     효과음 많이 · 짧게 · 즉시 · 겹쳐서 · 절대 안 버림
   ss2voice 의 채널 정책(3채널 고정역할·prio<0 폐기·1.2초 스테일 큐)은 실사용 제보로
   다듬은 값이라 건드리면 해설 v2.0 이 퇴행한다. 그래서 섞지 않고 같은 출력 버퍼에
   각자 가산한다.

   팩: <시스템폴더>/ss2_sfx.pak — 해설 팩과 같은 NGPV1 포맷(해시→오프셋).
       키는 문장이 아니라 짧은 이름("sfx.hit.m")이다. 팩이 없으면 완전 무해. */
#ifndef SS2SFX_H
#define SS2SFX_H

#include <stdint.h>

void ss2sfx_init(const char *pak_path);  /* 팩을 열고 전량 상주 디코드 */
int  ss2sfx_on(void);                    /* 팩이 실렸나 */
int  ss2sfx_count(void);                 /* 실린 클립 수 — 화면 확인용 */
void ss2sfx_set_enabled(int on);
void ss2sfx_set_volume(int pct);         /* 0~150 */
void ss2sfx_reset(void);                 /* 롬 로드·리셋 때 채널 비우기 */

void ss2sfx_hit(int dmg);                /* 타격 — 깎인 체력으로 세기를 고른다 */

/* 검증용 계기. 겹쳐 울리면 파형에서 온셋을 셀 수 없어(무음 구간이 없다) 밖에서
   「몇 발이 실제로 시작됐는가」를 물을 길이 필요하다. 버리는지 여부가 이 값으로 갈린다. */
int  ss2sfx_started(void);
void ss2sfx_started_reset(void);

void ss2sfx_mix(int16_t *buf, int frames); /* 게임 소리 위에 가산 */

#endif
