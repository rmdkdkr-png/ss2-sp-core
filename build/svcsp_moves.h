/* 자동 생성 — gen_svc_moves.py. 수정하지 말 것. 원본: tools/svc/moves.json */
#ifndef SVCSP_MOVES_H
#define SVCSP_MOVES_H

typedef struct { const char *name; const unsigned char *motion; unsigned char len;
                 unsigned char btn; unsigned char flags;
                 signed char next, next_hold; } svc_move;   /* 파생(렛카) — 표 인덱스, -1 없음 */
/* flags: 1=근접 4=공중 8=미검증 16=모으기(첫 방향을 길게) 32=초필 */

static const unsigned char mo_c0_2[] = {0x02};
static const unsigned char mo_c0_3[] = {0x02,0x0A,0x08};
static const unsigned char mo_c0_4[] = {0x02,0x0A,0x08};
static const unsigned char mo_c0_5[] = {0x08,0x0A,0x02,0x06,0x04};
static const unsigned char mo_c0_6[] = {0x08,0x0A,0x02,0x06,0x04};
static const unsigned char mo_c0_7[] = {0x08,0x02,0x0A};
static const unsigned char mo_c0_8[] = {0x04,0x02,0x06};
static const unsigned char mo_c0_9[] = {0x02,0x0A,0x08};
static const unsigned char mo_c0_10[] = {0x08,0x0A,0x02,0x06,0x04};
static const unsigned char mo_c0_11[] = {0x02,0x06,0x04};
static const unsigned char mo_c0_12[] = {0x02,0x06,0x04,0x06,0x02,0x0A,0x08};
static const unsigned char mo_c0_13[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const unsigned char mo_c0_14[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const unsigned char mo_c0_15[] = {0x00};
static const unsigned char mo_c0_16[] = {0x00};
static const svc_move mv_c0[] = {
  {"외식 나락 떨구기 (Naraku Otoshi)", mo_c0_2, 1, 0x10, 4, -1, -1},
  {"114식 황물기 (Aragami)", mo_c0_3, 3, 0x10, 0, 2, 4},
  {"128식 구상 (Kono Kizu)", mo_c0_4, 3, 0x10, 0, 13, -1},
  {"127식 팔청 (Yano Sabi)", mo_c0_5, 5, 0x10, 0, -1, -1},
  {"401식 죄읊기 (Tsumi Yomi)", mo_c0_6, 5, 0x10, 0, -1, -1},
  {"100식 귀신태우기 (Oniyaki)", mo_c0_7, 3, 0x10, 0, -1, -1},
  {"R.E.D. 킥", mo_c0_8, 3, 0x20, 0, -1, -1},
  {"75식 개", mo_c0_9, 3, 0x20, 0, 14, -1},
  {"212식 금월양 (Kototsuki You)", mo_c0_10, 5, 0x20, 0, -1, -1},
  {"910식 누에잡기 (Nue Tsumi)", mo_c0_11, 3, 0x10, 0, -1, -1},
  {"이면 108식 대사치 (Orochinagi)", mo_c0_12, 7, 0x10, 32, -1, -1},
  {"최종결전오의 무식", mo_c0_13, 6, 0x10, 32, -1, -1},
  {"182식", mo_c0_14, 6, 0x20, 32, -1, -1},
  {"구상 추가타", mo_c0_15, 1, 0x10, 0, -1, -1},
  {"개 추가타", mo_c0_16, 1, 0x20, 0, -1, -1},
};
static const unsigned char mo_c1_2[] = {0x02,0x0A,0x08};
static const unsigned char mo_c1_3[] = {0x02,0x01};
static const unsigned char mo_c1_4[] = {0x02,0x06,0x04};
static const unsigned char mo_c1_5[] = {0x02,0x06,0x04};
static const unsigned char mo_c1_6[] = {0x08,0x02,0x0A};
static const unsigned char mo_c1_7[] = {0x04,0x06,0x02,0x0A,0x08};
static const unsigned char mo_c1_8[] = {0x02,0x06,0x04,0x06,0x02,0x0A,0x08};
static const unsigned char mo_c1_9[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const svc_move mv_c1[] = {
  {"파워 웨이브", mo_c1_2, 3, 0x10, 0, -1, -1},
  {"라이징 태클", mo_c1_3, 2, 0x10, 16, -1, -1},
  {"크랙 슛", mo_c1_4, 3, 0x20, 0, -1, -1},
  {"번 너클", mo_c1_5, 3, 0x10, 0, -1, -1},
  {"파워 덩크", mo_c1_6, 3, 0x20, 0, -1, -1},
  {"파이어 킥", mo_c1_7, 5, 0x20, 0, -1, -1},
  {"파워 가이저", mo_c1_8, 7, 0x10, 32, -1, -1},
  {"하이 앵글 가이저", mo_c1_9, 6, 0x20, 32, -1, -1},
};
static const unsigned char mo_c2_1[] = {0x02,0x0A,0x08};
static const unsigned char mo_c2_2[] = {0x02,0x0A,0x08};
static const unsigned char mo_c2_3[] = {0x08,0x02,0x0A};
static const unsigned char mo_c2_4[] = {0x08,0x0A,0x02,0x06,0x04};
static const unsigned char mo_c2_5[] = {0x08,0x04,0x08};
static const unsigned char mo_c2_6[] = {0x08,0x04,0x06,0x02,0x0A,0x08};
static const unsigned char mo_c2_7[] = {0x02,0x0A,0x08,0x0A,0x02,0x06,0x04};
static const unsigned char mo_c2_8[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const svc_move mv_c2[] = {
  {"호황권", mo_c2_1, 3, 0x10, 0, -1, -1},
  {"공중 호황권", mo_c2_2, 3, 0x10, 4, -1, -1},
  {"호포", mo_c2_3, 3, 0x10, 0, -1, -1},
  {"비연질풍각", mo_c2_4, 5, 0x20, 0, -1, -1},
  {"잔렬권", mo_c2_5, 3, 0x10, 0, -1, -1},
  {"패왕상후권", mo_c2_6, 6, 0x10, 32, -1, -1},
  {"용호난무", mo_c2_7, 7, 0x10, 32, -1, -1},
  {"천지패황권", mo_c2_8, 6, 0x10, 32, -1, -1},
};
static const unsigned char mo_c3_2[] = {0x02};
static const unsigned char mo_c3_3[] = {0x02,0x0A,0x08};
static const unsigned char mo_c3_4[] = {0x02,0x06,0x04};
static const unsigned char mo_c3_5[] = {0x02,0x06,0x04};
static const unsigned char mo_c3_6[] = {0x04,0x06,0x02,0x0A,0x08};
static const unsigned char mo_c3_7[] = {0x02,0x01};
static const unsigned char mo_c3_8[] = {0x02,0x06,0x04};
static const unsigned char mo_c3_9[] = {0x02,0x06,0x04,0x06,0x02,0x0A,0x08};
static const unsigned char mo_c3_10[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const unsigned char mo_c3_11[] = {0x02,0x06,0x04,0x02,0x06,0x04};
static const svc_move mv_c3[] = {
  {"대륜풍차떨구기", mo_c3_2, 1, 0x10, 4, -1, -1},
  {"화접선", mo_c3_3, 3, 0x10, 0, -1, -1},
  {"용염무", mo_c3_4, 3, 0x10, 0, -1, -1},
  {"사치요 도리 (小夜千鳥)", mo_c3_5, 3, 0x20, 0, -1, -1},
  {"필살인봉", mo_c3_6, 5, 0x20, 0, -1, -1},
  {"무사사비의 춤 (지상)", mo_c3_7, 2, 0x10, 16, -1, -1},
  {"무사사비의 춤 (공중)", mo_c3_8, 3, 0x10, 4, -1, -1},
  {"초필살인봉", mo_c3_9, 7, 0x20, 32, -1, -1},
  {"화람 (Hana Arashi)", mo_c3_10, 6, 0x10, 32, -1, -1},
  {"봉황의 춤", mo_c3_11, 6, 0x10, 32, -1, -1},
};
static const unsigned char mo_c4_1[] = {0x04,0x08};
static const unsigned char mo_c4_2[] = {0x04,0x08};
static const unsigned char mo_c4_3[] = {0x02,0x01};
static const unsigned char mo_c4_4[] = {0x02,0x06,0x04};
static const unsigned char mo_c4_5[] = {0x02,0x06,0x04};
static const unsigned char mo_c4_6[] = {0x02,0x06,0x04};
static const unsigned char mo_c4_7[] = {0x02,0x0A,0x08,0x0A,0x02,0x06,0x04};
static const unsigned char mo_c4_8[] = {0x02,0x06,0x04,0x06,0x02,0x0A,0x08};
static const unsigned char mo_c4_9[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const unsigned char mo_c4_10[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const svc_move mv_c4[] = {
  {"발틱 런처", mo_c4_1, 2, 0x10, 16, -1, -1},
  {"그라운드 세이버", mo_c4_2, 2, 0x20, 16, -1, -1},
  {"문 슬래셔", mo_c4_3, 2, 0x10, 16, -1, -1},
  {"아이 슬래셔", mo_c4_4, 3, 0x10, 0, -1, -1},
  {"X 칼리버", mo_c4_5, 3, 0x10, 4, -1, -1},
  {"이어링 폭탄", mo_c4_6, 3, 0x20, 0, -1, -1},
  {"V 슬래셔", mo_c4_7, 7, 0x10, 36, -1, -1},
  {"리벨 스파크", mo_c4_8, 7, 0x20, 32, -1, -1},
  {"그래비티 스톰", mo_c4_9, 6, 0x10, 32, -1, -1},
  {"그레이트풀 데드", mo_c4_10, 6, 0x20, 32, -1, -1},
};
static const unsigned char mo_c5_1[] = {0x02};
static const unsigned char mo_c5_2[] = {0x02,0x06,0x04};
static const unsigned char mo_c5_3[] = {0x08,0x02,0x0A};
static const unsigned char mo_c5_4[] = {0x02,0x06,0x04};
static const unsigned char mo_c5_5[] = {0x08,0x0A,0x02,0x06,0x04};
static const unsigned char mo_c5_6[] = {0x02,0x0A,0x08};
static const unsigned char mo_c5_7[] = {0x04,0x06,0x02,0x0A,0x08};
static const unsigned char mo_c5_8[] = {0x08,0x0A,0x02,0x06,0x04,0x08,0x0A,0x02,0x06,0x04};
static const unsigned char mo_c5_9[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const unsigned char mo_c5_10[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const svc_move mv_c5[] = {
  {"피닉스 밤", mo_c5_1, 1, 0x20, 4, -1, -1},
  {"사이코 볼", mo_c5_2, 3, 0x10, 0, -1, -1},
  {"사이코 소드", mo_c5_3, 3, 0x10, 0, -1, -1},
  {"피닉스 애로우", mo_c5_4, 3, 0x20, 4, -1, -1},
  {"사이코 리플렉터", mo_c5_5, 5, 0x20, 0, -1, -1},
  {"사이킥 텔레포트", mo_c5_6, 3, 0x20, 0, -1, -1},
  {"슈퍼 사이킥 스로우", mo_c5_7, 5, 0x10, 1, -1, -1},
  {"샤이닝 크리스탈 비트", mo_c5_8, 10, 0x10, 32, -1, -1},
  {"피닉스 팽 애로우", mo_c5_9, 6, 0x20, 36, -1, -1},
  {"원조! 불꽃의 검 (Flame Sword)", mo_c5_10, 6, 0x10, 32, -1, -1},
};
static const unsigned char mo_c6_2[] = {0x04};
static const unsigned char mo_c6_3[] = {0x02,0x0A,0x08};
static const unsigned char mo_c6_4[] = {0x08,0x02,0x0A};
static const unsigned char mo_c6_5[] = {0x02,0x06,0x04};
static const unsigned char mo_c6_6[] = {0x08,0x0A,0x02,0x06,0x04};
static const unsigned char mo_c6_7[] = {0x08,0x02,0x0A};
static const unsigned char mo_c6_8[] = {0x08,0x0A,0x02,0x06,0x04,0x08};
static const unsigned char mo_c6_9[] = {0x02,0x0A,0x08,0x0A,0x02,0x06,0x04};
static const unsigned char mo_c6_10[] = {0x02,0x06,0x04,0x06,0x02,0x0A,0x08};
static const unsigned char mo_c6_11[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const svc_move mv_c6[] = {
  {"외식 유리오리 (백합꺾기)", mo_c6_2, 1, 0x20, 4, -1, -1},
  {"108식 야미바라이 (어둠쫓기)", mo_c6_3, 3, 0x10, 0, -1, -1},
  {"100식 오니야키", mo_c6_4, 3, 0x10, 0, -1, -1},
  {"127식 아오이하나", mo_c6_5, 3, 0x10, 0, -1, -1},
  {"212식 코토츠키 인", mo_c6_6, 5, 0x20, 0, -1, -1},
  {"311식 츠마구시", mo_c6_7, 3, 0x20, 0, -1, -1},
  {"쿠즈카제", mo_c6_8, 6, 0x10, 1, -1, -1},
  {"금 1211식 야오토메 (팔치녀)", mo_c6_9, 7, 0x10, 32, -1, -1},
  {"이면 108식 야사카즈키", mo_c6_10, 7, 0x10, 32, -1, -1},
  {"이면 311식 사쿠 츠마구시", mo_c6_11, 6, 0x20, 32, -1, -1},
};
static const unsigned char mo_c7_0[] = {0x02,0x0A,0x08};
static const unsigned char mo_c7_1[] = {0x08,0x02,0x0A};
static const unsigned char mo_c7_2[] = {0x02,0x06,0x04};
static const unsigned char mo_c7_3[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const unsigned char mo_c7_4[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const svc_move mv_c7[] = {
  {"오의 선풍열참", mo_c7_0, 3, 0x10, 0, -1, -1},
  {"오의 호월참", mo_c7_1, 3, 0x10, 0, -1, -1},
  {"오의 열진참", mo_c7_2, 3, 0x10, 0, -1, -1},
  {"비오의 천파봉신참", mo_c7_3, 6, 0x10, 32, -1, -1},
  {"비오의 천파단공열참", mo_c7_4, 6, 0x20, 32, -1, -1},
};
static const unsigned char mo_c8_0[] = {0x02};
static const unsigned char mo_c8_1[] = {0x04,0x06,0x02};
static const unsigned char mo_c8_2[] = {0x02,0x0A,0x08};
static const unsigned char mo_c8_3[] = {0x04,0x02,0x06};
static const unsigned char mo_c8_4[] = {0x02,0x06,0x04};
static const unsigned char mo_c8_5[] = {0x02,0x06,0x04};
static const unsigned char mo_c8_6[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const unsigned char mo_c8_7[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const svc_move mv_c8[] = {
  {"카무이 훔 케습", mo_c8_0, 1, 0x20, 4, -1, -1},
  {"안누 무츠베", mo_c8_1, 3, 0x10, 0, -1, -1},
  {"레라 무츠베", mo_c8_2, 3, 0x10, 0, -1, -1},
  {"카무이 림세", mo_c8_3, 3, 0x10, 0, -1, -1},
  {"시치카푸 에투", mo_c8_4, 3, 0x10, 0, -1, -1},
  {"매에 매달리기 (타카니 츠카마루)", mo_c8_5, 3, 0x20, 0, -1, -1},
  {"엘레루시 카무이 림세", mo_c8_6, 6, 0x10, 32, -1, -1},
  {"이루스카 야토로 림세", mo_c8_7, 6, 0x20, 32, -1, -1},
};
static const unsigned char mo_c9_2[] = {0x02,0x0A,0x08};
static const unsigned char mo_c9_3[] = {0x04,0x06,0x02,0x0A,0x08};
static const unsigned char mo_c9_4[] = {0x08,0x02,0x0A};
static const unsigned char mo_c9_5[] = {0x02,0x06,0x04};
static const unsigned char mo_c9_6[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const unsigned char mo_c9_7[] = {0x02,0x06,0x04,0x02,0x06,0x04};
static const unsigned char mo_c9_8[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const svc_move mv_c9[] = {
  {"파동권(Hadouken)", mo_c9_2, 3, 0x10, 0, -1, -1},
  {"작열 파동권(Shakunetsu Hadouken)", mo_c9_3, 5, 0x10, 0, -1, -1},
  {"승룡권(Shoryuken)", mo_c9_4, 3, 0x10, 0, -1, -1},
  {"용권선풍각(Tatsumaki Senpukyaku)", mo_c9_5, 3, 0x20, 0, -1, -1},
  {"진공파동권(Shinku Hadouken)", mo_c9_6, 6, 0x10, 32, -1, -1},
  {"진공용권선풍각(Shinku Tatsumaki Senpukyaku)", mo_c9_7, 6, 0x20, 32, -1, -1},
  {"진 승룡권(Shin Shoryuken)", mo_c9_8, 6, 0x20, 32, -1, -1},
};
static const unsigned char mo_c10_2[] = {0x02};
static const unsigned char mo_c10_3[] = {0x04,0x06,0x02,0x0A,0x08};
static const unsigned char mo_c10_4[] = {0x02,0x01};
static const unsigned char mo_c10_5[] = {0x04,0x08};
static const unsigned char mo_c10_6[] = {0x08,0x0A,0x02,0x06,0x04};
static const unsigned char mo_c10_7[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const unsigned char mo_c10_8[] = {0x04,0x08,0x04,0x08};
static const unsigned char mo_c10_9[] = {0x06,0x0A,0x06,0x09};
static const unsigned char mo_c10_10[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const svc_move mv_c10[] = {
  {"응조각(Yousou Kyaku)", mo_c10_2, 1, 0x20, 4, -1, -1},
  {"기공권(Kikoken)", mo_c10_3, 5, 0x10, 0, -1, -1},
  {"천승각(Tenshokyaku)", mo_c10_4, 2, 0x20, 16, -1, -1},
  {"스피닝 버드 킥(Spinning Bird Kick)", mo_c10_5, 2, 0x20, 16, -1, -1},
  {"선원추(Sen'enshu)", mo_c10_6, 5, 0x20, 0, -1, -1},
  {"기공장(Kikosho)", mo_c10_7, 6, 0x10, 32, -1, -1},
  {"천렬각(Senretsukyaku)", mo_c10_8, 4, 0x20, 48, -1, -1},
  {"패산천승각(Hazan Tenshokyaku)", mo_c10_9, 4, 0x20, 48, -1, -1},
  {"칠성섬공각(Shichisei Senkukyaku)", mo_c10_10, 6, 0x20, 36, -1, -1},
};
static const unsigned char mo_c11_2[] = {0x02};
static const unsigned char mo_c11_3[] = {0x02};
static const unsigned char mo_c11_4[] = {0x01};
static const unsigned char mo_c11_5[] = {0x02,0x06,0x04};
static const unsigned char mo_c11_6[] = {0x02,0x06,0x04};
static const unsigned char mo_c11_7[] = {0x08,0x02,0x0A};
static const unsigned char mo_c11_8[] = {0x08,0x0A,0x02,0x06,0x04,0x08};
static const unsigned char mo_c11_9[] = {0x08,0x0A,0x02,0x06,0x04,0x08};
static const unsigned char mo_c11_10[] = {0x08,0x0A,0x02,0x06,0x04,0x08};
static const unsigned char mo_c11_11[] = {0x08,0x0A,0x02,0x06,0x04,0x08};
static const unsigned char mo_c11_12[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const unsigned char mo_c11_13[] = {0x02,0x06,0x04,0x02,0x06,0x04};
static const svc_move mv_c11[] = {
  {"플라잉 바디 프레스(Flying Body Press)", mo_c11_2, 1, 0x10, 4, -1, -1},
  {"더블 니 드롭(Double Knee Drop)", mo_c11_3, 1, 0x20, 4, -1, -1},
  {"공중 헤드벗(Midair Headbutt)", mo_c11_4, 1, 0x10, 4, -1, -1},
  {"더블 라리아트(Double Lariat)", mo_c11_5, 3, 0x10, 0, -1, -1},
  {"퀵 더블 라리아트(Quick Double Lariat)", mo_c11_6, 3, 0x20, 0, -1, -1},
  {"배니싱 플랫(Banishing Flat)", mo_c11_7, 3, 0x10, 0, -1, -1},
  {"스크류 파일 드라이버(Screw Piledriver)", mo_c11_8, 6, 0x10, 1, -1, -1},
  {"아토믹 수플렉스(Atomic Suplex)", mo_c11_9, 6, 0x20, 1, -1, -1},
  {"플라잉 파워밤(Flying Powerbomb)", mo_c11_10, 6, 0x20, 0, -1, -1},
  {"파이널 아토믹 버스터(Final Atomic Buster)", mo_c11_11, 6, 0x10, 33, -1, -1},
  {"에어리얼 러시안 슬램(Aerial Russian Slam)", mo_c11_12, 6, 0x20, 32, -1, -1},
  {"러시안 비트(Russian Beat)", mo_c11_13, 6, 0x20, 32, -1, -1},
};
static const unsigned char mo_c12_2[] = {0x02,0x06,0x04};
static const unsigned char mo_c12_3[] = {0x02,0x0A,0x08};
static const unsigned char mo_c12_4[] = {0x08,0x02,0x0A};
static const unsigned char mo_c12_5[] = {0x02,0x06,0x04};
static const unsigned char mo_c12_6[] = {0x02,0x0A,0x08,0x02,0x0A};
static const unsigned char mo_c12_7[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const unsigned char mo_c12_8[] = {0x02,0x06,0x04,0x02,0x06,0x04};
static const svc_move mv_c12[] = {
  {"전방전신(Zenpou Tenshin)", mo_c12_2, 3, 0x10, 0, -1, -1},
  {"파동권(Hadouken)", mo_c12_3, 3, 0x10, 0, -1, -1},
  {"승룡권(Shoryuken)", mo_c12_4, 3, 0x10, 0, -1, -1},
  {"용권선풍각(Tatsumaki Senpukyaku)", mo_c12_5, 3, 0x20, 0, -1, -1},
  {"승룡열파(Shoryu Reppa)", mo_c12_6, 5, 0x10, 32, -1, -1},
  {"신룡권(Shinryuken)", mo_c12_7, 6, 0x20, 32, -1, -1},
  {"질풍신뢰각(Shippu Jinraikyaku)", mo_c12_8, 6, 0x20, 32, -1, -1},
};
static const unsigned char mo_c13_0[] = {0x02,0x0A,0x08};
static const unsigned char mo_c13_1[] = {0x08,0x02,0x0A};
static const unsigned char mo_c13_2[] = {0x02,0x06,0x04};
static const unsigned char mo_c13_3[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const unsigned char mo_c13_4[] = {0x02,0x0A,0x08,0x02,0x0A};
static const unsigned char mo_c13_5[] = {0x02,0x06,0x04,0x02,0x06,0x04};
static const unsigned char mo_c13_6[] = {0x04,0x08,0x04,0x08};
static const svc_move mv_c13[] = {
  {"아도권(Gadouken)", mo_c13_0, 3, 0x10, 0, -1, -1},
  {"코류켄(Kouryuken)", mo_c13_1, 3, 0x10, 0, -1, -1},
  {"단공각(Dankukyaku)", mo_c13_2, 3, 0x20, 0, -1, -1},
  {"진공아도권(Shinku Gadouken)", mo_c13_3, 6, 0x10, 32, -1, -1},
  {"코류렉카(Kouryu Rekka)", mo_c13_4, 5, 0x20, 32, -1, -1},
  {"필승무뢰권(Hisshou Burai Ken)", mo_c13_5, 6, 0x20, 32, -1, -1},
  {"오토코미치(Otoko Michi)", mo_c13_6, 4, 0x10, 56, -1, -1},
};
static const unsigned char mo_c14_1[] = {0x02,0x0A,0x08};
static const unsigned char mo_c14_2[] = {0x08,0x02,0x0A};
static const unsigned char mo_c14_3[] = {0x02,0x06,0x04};
static const unsigned char mo_c14_4[] = {0x08,0x02,0x0A};
static const unsigned char mo_c14_5[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const unsigned char mo_c14_6[] = {0x02,0x0A,0x08,0x02,0x0A};
static const unsigned char mo_c14_7[] = {0x02,0x06,0x04,0x02,0x06,0x04};
static const unsigned char mo_c14_8[] = {0x04,0x08,0x04,0x08};
static const svc_move mv_c14[] = {
  {"파동권(Hadouken)", mo_c14_1, 3, 0x10, 0, -1, -1},
  {"쇼오켄(Shououken)", mo_c14_2, 3, 0x10, 0, -1, -1},
  {"춘풍각(Shunpukyaku)", mo_c14_3, 3, 0x20, 0, -1, -1},
  {"사쿠라 오토시(Sakura Otoshi)", mo_c14_4, 3, 0x20, 0, -1, -1},
  {"진공파동권(Shinku Hadouken)", mo_c14_5, 6, 0x10, 32, -1, -1},
  {"미다레자쿠라(Midare Zakura)", mo_c14_6, 5, 0x20, 32, -1, -1},
  {"하루이치방(Haru Ichiban)", mo_c14_7, 6, 0x20, 32, -1, -1},
  {"슌고쿠사츠(Shun Goku Satsu)", mo_c14_8, 4, 0x10, 56, -1, -1},
};
static const unsigned char mo_c15_0[] = {0x02};
static const unsigned char mo_c15_1[] = {0x02,0x0A,0x08};
static const unsigned char mo_c15_2[] = {0x08,0x02,0x0A};
static const unsigned char mo_c15_3[] = {0x08,0x0A,0x02,0x06,0x04};
static const unsigned char mo_c15_4[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const unsigned char mo_c15_5[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const unsigned char mo_c15_6[] = {0x08,0x0A,0x02,0x06,0x04};
static const unsigned char mo_c15_7[] = {0x02,0x06,0x04,0x02,0x06,0x04};
static const svc_move mv_c15[] = {
  {"셸 킥(Shell Kick)", mo_c15_0, 1, 0x20, 4, -1, -1},
  {"소울 피스트(Soul Fist)", mo_c15_1, 3, 0x10, 0, -1, -1},
  {"섀도 블레이드(Shadow Blade)", mo_c15_2, 3, 0x10, 0, -1, -1},
  {"벡터 드레인(Vector Drain)", mo_c15_3, 5, 0x10, 1, -1, -1},
  {"다크니스 일루전(Darkness Illusion)", mo_c15_4, 6, 0x10, 32, -1, -1},
  {"피니싱 샤워(Finishing Shower)", mo_c15_5, 6, 0x20, 32, -1, -1},
  {"발키리 턴(Valkyrie Turn)", mo_c15_6, 5, 0x20, 32, -1, -1},
  {"크립틱 니들(Cryptic Needle)", mo_c15_7, 6, 0x10, 40, -1, -1},
};
static const unsigned char mo_c16_0[] = {0x02,0x0A,0x08};
static const unsigned char mo_c16_1[] = {0x08,0x02,0x0A};
static const unsigned char mo_c16_2[] = {0x08,0x02,0x0A};
static const unsigned char mo_c16_3[] = {0x08,0x0A,0x02,0x06,0x04};
static const unsigned char mo_c16_5[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const unsigned char mo_c16_6[] = {0x02,0x06,0x04,0x02,0x06,0x04};
static const unsigned char mo_c16_7[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const svc_move mv_c16[] = {
  {"롤링 버클러(Rolling Buckler)", mo_c16_0, 3, 0x10, 0, -1, -1},
  {"캣 스파이크(Cat Spike)", mo_c16_1, 3, 0x10, 0, -1, -1},
  {"델타 킥(Delta Kick)", mo_c16_2, 3, 0x20, 0, -1, -1},
  {"헬 캣(Hell Cat)", mo_c16_3, 5, 0x20, 1, -1, -1},
  {"댄싱 플래시(Dancing Flash)", mo_c16_5, 6, 0x10, 32, -1, -1},
  {"플리즈 헬프 미(Please Help Me)", mo_c16_6, 6, 0x10, 32, -1, -1},
  {"ES 롤링 스크래치(ES Rolling Scratch)", mo_c16_7, 6, 0x20, 32, -1, -1},
};
static const unsigned char mo_c17_2[] = {0x04,0x08};
static const unsigned char mo_c17_3[] = {0x02,0x01};
static const unsigned char mo_c17_4[] = {0x04,0x08,0x04,0x08};
static const unsigned char mo_c17_5[] = {0x06,0x0A,0x06,0x09};
static const unsigned char mo_c17_6[] = {0x04,0x08,0x04,0x08};
static const svc_move mv_c17[] = {
  {"소닉 붐(Sonic Boom)", mo_c17_2, 2, 0x10, 16, -1, -1},
  {"서머솔트 킥(Somersault Kick)", mo_c17_3, 2, 0x20, 16, -1, -1},
  {"소닉 허리케인(Sonic Hurricane)", mo_c17_4, 4, 0x10, 56, -1, -1},
  {"서머솔트 스트라이크(Somersault Strike)", mo_c17_5, 4, 0x20, 48, -1, -1},
  {"크로스파이어 블리츠(Crossfire Blitz)", mo_c17_6, 4, 0x20, 56, -1, -1},
};

/* 캐릭터별: 기술표 + 슬롯 7자리 (N F B D DF DB AIR — 값은 기술 인덱스, -1 없음) */
typedef struct { const svc_move *mv; unsigned char n; const char *name; unsigned char cancel_dud; signed char slots[7]; } svc_chartab;
#define SVC_CHAR_COUNT 18
static const svc_chartab svc_chars[SVC_CHAR_COUNT] = {
  { mv_c0, 15, "쿄", 1, {1,11,1,5,6,8,0} },
  { mv_c1, 8, "테리", 0, {0,6,3,1,4,2,-1} },
  { mv_c2, 8, "료", 0, {0,6,4,2,3,5,1} },
  { mv_c3, 10, "마이", 0, {1,4,3,2,5,4,6} },
  { mv_c4, 10, "레오나", 0, {0,8,5,2,3,1,4} },
  { mv_c5, 10, "아테나", 0, {1,7,6,2,4,5,0} },
  { mv_c6, 10, "이오리", 0, {1,7,3,2,5,4,0} },
  { mv_c7, 5, "하오마루", 0, {0,3,0,1,-1,4,-1} },
  { mv_c8, 8, "나코루루", 0, {4,3,4,2,3,5,0} },
  { mv_c9, 7, "류", 0, {0,4,5,2,3,1,-1} },
  { mv_c10, 9, "춘리", 0, {1,5,-1,2,4,3,0} },
  { mv_c11, 12, "장기에프", 0, {5,9,6,3,10,4,0} },
  { mv_c12, 7, "켄", 0, {1,4,0,2,5,3,-1} },
  { mv_c13, 7, "단", 0, {0,3,2,1,4,5,-1} },
  { mv_c14, 8, "사쿠라", 0, {0,4,2,1,5,3,-1} },
  { mv_c15, 8, "모리간", 0, {1,4,3,2,5,6,0} },
  { mv_c16, 7, "펠리시아", 0, {0,4,3,2,1,5,-1} },
  { mv_c17, 5, "가일", 0, {0,2,-1,1,3,4,-1} },
};

#endif
