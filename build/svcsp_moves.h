/* 자동 생성 — gen_svc_moves.py. 수정하지 말 것. 원본: tools/svc/moves.json */
#ifndef SVCSP_MOVES_H
#define SVCSP_MOVES_H

typedef struct { const char *name; const char *name_hold; const unsigned char *motion;
                 unsigned char len; unsigned char btn; unsigned char flags;
                 signed char next, next_hold, next_k; } svc_move;   /* 파생(렛카) — 표 인덱스, -1 없음.
                    next=탭 갈래 next_hold=홀드 갈래 next_k=킥 갈래(125식 칠뢰)
                    name_hold = 같은 커맨드의 **강판 이름**(쿄 황물기→독물기). 없으면 0.
                    표를 병합해 둔 탓에 자막이 약·강을 구분 못 하던 것을 이걸로 가른다. */
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
static const unsigned char mo_c0_15[] = {0x08};
static const unsigned char mo_c0_16[] = {0x00};
static const unsigned char mo_c0_17[] = {0x00};
static const unsigned char mo_c0_18[] = {0x08};
static const svc_move mv_c0[] = {
  {"외식 나락 떨구기 (Naraku Otoshi)", 0, mo_c0_2, 1, 0x10, 4, -1, -1, -1},
  {"114식 황물기 (Aragami)", "115식 독물기 (Dokugami)", mo_c0_3, 3, 0x10, 0, 2, 4, -1},
  {"128식 구상 (Kono Kizu)", 0, mo_c0_4, 3, 0x10, 0, 13, -1, 14},
  {"127식 팔청 (Yano Sabi)", 0, mo_c0_5, 5, 0x10, 0, 13, -1, 14},
  {"401식 죄읊기 (Tsumi Yomi)", 0, mo_c0_6, 5, 0x10, 0, 16, -1, -1},
  {"100식 귀신태우기 (Oniyaki)", 0, mo_c0_7, 3, 0x10, 0, -1, -1, -1},
  {"R.E.D. 킥", 0, mo_c0_8, 3, 0x20, 0, -1, -1, -1},
  {"75식 개", 0, mo_c0_9, 3, 0x20, 0, 15, 15, -1},
  {"212식 금월양 (Kototsuki You)", 0, mo_c0_10, 5, 0x20, 0, -1, -1, -1},
  {"910식 누에잡기 (Nue Tsumi)", 0, mo_c0_11, 3, 0x10, 0, -1, -1, -1},
  {"이면 108식 대사치 (Orochinagi)", 0, mo_c0_12, 7, 0x10, 32, -1, -1, -1},
  {"최종결전오의 무식", 0, mo_c0_13, 6, 0x10, 32, -1, -1, -1},
  {"182식", 0, mo_c0_14, 6, 0x20, 32, -1, -1, -1},
  {"외식 섬돌뚫기 (Migiri Ugachi)", 0, mo_c0_15, 1, 0x10, 0, -1, -1, -1},
  {"125식 칠뢰 (Nanase)", 0, mo_c0_16, 1, 0x20, 0, -1, -1, -1},
  {"개 추가타", 0, mo_c0_17, 1, 0x20, 0, -1, -1, -1},
  {"벌읊기 (Batsu Yomi)", 0, mo_c0_18, 1, 0x10, 0, -1, -1, -1},
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
  {"파워 웨이브", 0, mo_c1_2, 3, 0x10, 0, -1, -1, -1},
  {"라이징 태클", "라이징 태클(강)", mo_c1_3, 2, 0x10, 16, -1, -1, -1},
  {"크랙 슛", 0, mo_c1_4, 3, 0x20, 0, -1, -1, -1},
  {"번 너클", 0, mo_c1_5, 3, 0x10, 0, -1, -1, -1},
  {"파워 덩크", 0, mo_c1_6, 3, 0x20, 0, -1, -1, -1},
  {"파이어 킥", 0, mo_c1_7, 5, 0x20, 0, -1, -1, -1},
  {"파워 가이저", 0, mo_c1_8, 7, 0x10, 32, -1, -1, -1},
  {"하이 앵글 가이저", 0, mo_c1_9, 6, 0x20, 32, -1, -1, -1},
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
  {"호황권", 0, mo_c2_1, 3, 0x10, 0, -1, -1, -1},
  {"공중 호황권", 0, mo_c2_2, 3, 0x10, 4, -1, -1, -1},
  {"호포", 0, mo_c2_3, 3, 0x10, 0, -1, -1, -1},
  {"비연질풍각", 0, mo_c2_4, 5, 0x20, 0, -1, -1, -1},
  {"잔렬권", 0, mo_c2_5, 3, 0x10, 0, -1, -1, -1},
  {"패왕상후권", 0, mo_c2_6, 6, 0x10, 32, -1, -1, -1},
  {"용호난무", 0, mo_c2_7, 7, 0x10, 32, -1, -1, -1},
  {"천지패황권", 0, mo_c2_8, 6, 0x10, 32, -1, -1, -1},
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
  {"대륜풍차떨구기", 0, mo_c3_2, 1, 0x10, 4, -1, -1, -1},
  {"화접선", 0, mo_c3_3, 3, 0x10, 0, -1, -1, -1},
  {"용염무", 0, mo_c3_4, 3, 0x10, 0, -1, -1, -1},
  {"사치요 도리 (小夜千鳥)", 0, mo_c3_5, 3, 0x20, 0, 2, -1, -1},
  {"필살인봉", 0, mo_c3_6, 5, 0x20, 0, -1, -1, -1},
  {"무사사비의 춤 (지상)", 0, mo_c3_7, 2, 0x10, 16, -1, -1, -1},
  {"무사사비의 춤 (공중)", 0, mo_c3_8, 3, 0x10, 4, -1, -1, -1},
  {"초필살인봉", 0, mo_c3_9, 7, 0x20, 32, -1, -1, -1},
  {"화람 (Hana Arashi)", 0, mo_c3_10, 6, 0x10, 32, -1, -1, -1},
  {"봉황의 춤", 0, mo_c3_11, 6, 0x10, 32, -1, -1, -1},
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
  {"발틱 런처", 0, mo_c4_1, 2, 0x10, 16, -1, -1, -1},
  {"그라운드 세이버", 0, mo_c4_2, 2, 0x20, 16, -1, -1, -1},
  {"문 슬래셔", 0, mo_c4_3, 2, 0x10, 16, -1, -1, -1},
  {"아이 슬래셔", 0, mo_c4_4, 3, 0x10, 0, -1, -1, -1},
  {"X 칼리버", 0, mo_c4_5, 3, 0x10, 4, -1, -1, -1},
  {"이어링 폭탄", 0, mo_c4_6, 3, 0x20, 0, -1, -1, -1},
  {"V 슬래셔", 0, mo_c4_7, 7, 0x10, 36, -1, -1, -1},
  {"리벨 스파크", 0, mo_c4_8, 7, 0x20, 32, -1, -1, -1},
  {"그래비티 스톰", 0, mo_c4_9, 6, 0x10, 32, -1, -1, -1},
  {"그레이트풀 데드", 0, mo_c4_10, 6, 0x20, 32, -1, -1, -1},
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
  {"피닉스 밤", 0, mo_c5_1, 1, 0x20, 4, -1, -1, -1},
  {"사이코 볼", 0, mo_c5_2, 3, 0x10, 0, -1, -1, -1},
  {"사이코 소드", 0, mo_c5_3, 3, 0x10, 0, -1, -1, -1},
  {"피닉스 애로우", 0, mo_c5_4, 3, 0x20, 4, -1, -1, -1},
  {"사이코 리플렉터", "뉴 사이코 리플렉터", mo_c5_5, 5, 0x20, 0, -1, -1, -1},
  {"사이킥 텔레포트", 0, mo_c5_6, 3, 0x20, 0, -1, -1, -1},
  {"슈퍼 사이킥 스로우", 0, mo_c5_7, 5, 0x10, 1, -1, -1, -1},
  {"샤이닝 크리스탈 비트", 0, mo_c5_8, 10, 0x10, 32, -1, -1, -1},
  {"피닉스 팽 애로우", 0, mo_c5_9, 6, 0x20, 36, -1, -1, -1},
  {"원조! 불꽃의 검 (Flame Sword)", 0, mo_c5_10, 6, 0x10, 32, -1, -1, -1},
};
static const unsigned char mo_c6_2[] = {0x04};
static const unsigned char mo_c6_3[] = {0x02,0x0A,0x08};
static const unsigned char mo_c6_4[] = {0x08,0x02,0x0A};
static const unsigned char mo_c6_5[] = {0x02,0x06,0x04};
static const unsigned char mo_c6_6[] = {0x02,0x06,0x04};
static const unsigned char mo_c6_7[] = {0x02,0x06,0x04};
static const unsigned char mo_c6_8[] = {0x08,0x0A,0x02,0x06,0x04};
static const unsigned char mo_c6_9[] = {0x08,0x02,0x0A};
static const unsigned char mo_c6_10[] = {0x08,0x0A,0x02,0x06,0x04,0x08};
static const unsigned char mo_c6_11[] = {0x02,0x0A,0x08,0x0A,0x02,0x06,0x04};
static const unsigned char mo_c6_12[] = {0x02,0x06,0x04,0x06,0x02,0x0A,0x08};
static const unsigned char mo_c6_13[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const svc_move mv_c6[] = {
  {"외식 유리오리 (백합꺾기)", 0, mo_c6_2, 1, 0x20, 4, -1, -1, -1},
  {"108식 야미바라이 (어둠쫓기)", 0, mo_c6_3, 3, 0x10, 0, -1, -1, -1},
  {"100식 오니야키", 0, mo_c6_4, 3, 0x10, 0, -1, -1, -1},
  {"127식 아오이하나", 0, mo_c6_5, 3, 0x10, 0, 4, -1, -1},
  {"127식 아오이하나 2타", 0, mo_c6_6, 3, 0x10, 0, 5, -1, -1},
  {"127식 아오이하나 3타", 0, mo_c6_7, 3, 0x10, 0, -1, -1, -1},
  {"212식 코토츠키 인", 0, mo_c6_8, 5, 0x20, 0, -1, -1, -1},
  {"311식 츠마구시", 0, mo_c6_9, 3, 0x20, 0, -1, -1, -1},
  {"쿠즈카제", 0, mo_c6_10, 6, 0x10, 1, -1, -1, -1},
  {"금 1211식 야오토메 (팔치녀)", 0, mo_c6_11, 7, 0x10, 32, -1, -1, -1},
  {"이면 108식 야사카즈키", 0, mo_c6_12, 7, 0x10, 32, -1, -1, -1},
  {"이면 311식 사쿠 츠마구시", 0, mo_c6_13, 6, 0x20, 32, -1, -1, -1},
};
static const unsigned char mo_c7_0[] = {0x02,0x0A,0x08};
static const unsigned char mo_c7_1[] = {0x08,0x02,0x0A};
static const unsigned char mo_c7_2[] = {0x02,0x06,0x04};
static const unsigned char mo_c7_3[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const unsigned char mo_c7_4[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const svc_move mv_c7[] = {
  {"오의 선풍열참", 0, mo_c7_0, 3, 0x10, 0, -1, -1, -1},
  {"오의 호월참", 0, mo_c7_1, 3, 0x10, 0, -1, -1, -1},
  {"오의 열진참", 0, mo_c7_2, 3, 0x10, 0, -1, -1, -1},
  {"비오의 천파봉신참", 0, mo_c7_3, 6, 0x10, 32, -1, -1, -1},
  {"비오의 천파단공열참", 0, mo_c7_4, 6, 0x20, 32, -1, -1, -1},
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
  {"카무이 훔 케습", 0, mo_c8_0, 1, 0x20, 4, -1, -1, -1},
  {"안누 무츠베", 0, mo_c8_1, 3, 0x10, 0, -1, -1, -1},
  {"레라 무츠베", 0, mo_c8_2, 3, 0x10, 0, -1, -1, -1},
  {"카무이 림세", 0, mo_c8_3, 3, 0x10, 0, -1, -1, -1},
  {"시치카푸 에투", 0, mo_c8_4, 3, 0x10, 0, -1, -1, -1},
  {"매에 매달리기 (타카니 츠카마루)", 0, mo_c8_5, 3, 0x20, 0, -1, -1, -1},
  {"엘레루시 카무이 림세", 0, mo_c8_6, 6, 0x10, 32, -1, -1, -1},
  {"이루스카 야토로 림세", 0, mo_c8_7, 6, 0x20, 32, -1, -1, -1},
};
static const unsigned char mo_c9_2[] = {0x02,0x0A,0x08};
static const unsigned char mo_c9_3[] = {0x04,0x06,0x02,0x0A,0x08};
static const unsigned char mo_c9_4[] = {0x08,0x02,0x0A};
static const unsigned char mo_c9_5[] = {0x02,0x06,0x04};
static const unsigned char mo_c9_6[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const unsigned char mo_c9_7[] = {0x02,0x06,0x04,0x02,0x06,0x04};
static const unsigned char mo_c9_8[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const svc_move mv_c9[] = {
  {"파동권(Hadouken)", 0, mo_c9_2, 3, 0x10, 0, -1, -1, -1},
  {"작열 파동권(Shakunetsu Hadouken)", 0, mo_c9_3, 5, 0x10, 0, -1, -1, -1},
  {"승룡권(Shoryuken)", 0, mo_c9_4, 3, 0x10, 0, -1, -1, -1},
  {"용권선풍각(Tatsumaki Senpukyaku)", 0, mo_c9_5, 3, 0x20, 0, -1, -1, -1},
  {"진공파동권(Shinku Hadouken)", 0, mo_c9_6, 6, 0x10, 32, -1, -1, -1},
  {"진공용권선풍각(Shinku Tatsumaki Senpukyaku)", 0, mo_c9_7, 6, 0x20, 32, -1, -1, -1},
  {"진 승룡권(Shin Shoryuken)", 0, mo_c9_8, 6, 0x20, 32, -1, -1, -1},
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
  {"응조각(Yousou Kyaku)", 0, mo_c10_2, 1, 0x20, 4, -1, -1, -1},
  {"기공권(Kikoken)", 0, mo_c10_3, 5, 0x10, 0, -1, -1, -1},
  {"천승각(Tenshokyaku)", 0, mo_c10_4, 2, 0x20, 16, -1, -1, -1},
  {"스피닝 버드 킥(Spinning Bird Kick)", 0, mo_c10_5, 2, 0x20, 16, -1, -1, -1},
  {"선원추(Sen'enshu)", 0, mo_c10_6, 5, 0x20, 0, -1, -1, -1},
  {"기공장(Kikosho)", 0, mo_c10_7, 6, 0x10, 32, -1, -1, -1},
  {"천렬각(Senretsukyaku)", 0, mo_c10_8, 4, 0x20, 48, -1, -1, -1},
  {"패산천승각(Hazan Tenshokyaku)", 0, mo_c10_9, 4, 0x20, 48, -1, -1, -1},
  {"칠성섬공각(Shichisei Senkukyaku)", 0, mo_c10_10, 6, 0x20, 36, -1, -1, -1},
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
  {"플라잉 바디 프레스(Flying Body Press)", 0, mo_c11_2, 1, 0x10, 4, -1, -1, -1},
  {"더블 니 드롭(Double Knee Drop)", 0, mo_c11_3, 1, 0x20, 4, -1, -1, -1},
  {"공중 헤드벗(Midair Headbutt)", 0, mo_c11_4, 1, 0x10, 4, -1, -1, -1},
  {"더블 라리아트(Double Lariat)", 0, mo_c11_5, 3, 0x10, 0, -1, -1, -1},
  {"퀵 더블 라리아트(Quick Double Lariat)", 0, mo_c11_6, 3, 0x20, 0, -1, -1, -1},
  {"배니싱 플랫(Banishing Flat)", 0, mo_c11_7, 3, 0x10, 0, -1, -1, -1},
  {"스크류 파일 드라이버(Screw Piledriver)", 0, mo_c11_8, 6, 0x10, 1, -1, -1, -1},
  {"아토믹 수플렉스(Atomic Suplex)", 0, mo_c11_9, 6, 0x20, 1, -1, -1, -1},
  {"플라잉 파워밤(Flying Powerbomb)", 0, mo_c11_10, 6, 0x20, 0, -1, -1, -1},
  {"파이널 아토믹 버스터(Final Atomic Buster)", 0, mo_c11_11, 6, 0x10, 33, -1, -1, -1},
  {"에어리얼 러시안 슬램(Aerial Russian Slam)", 0, mo_c11_12, 6, 0x20, 32, -1, -1, -1},
  {"러시안 비트(Russian Beat)", 0, mo_c11_13, 6, 0x20, 32, -1, -1, -1},
};
static const unsigned char mo_c12_2[] = {0x02,0x06,0x04};
static const unsigned char mo_c12_3[] = {0x02,0x0A,0x08};
static const unsigned char mo_c12_4[] = {0x08,0x02,0x0A};
static const unsigned char mo_c12_5[] = {0x02,0x06,0x04};
static const unsigned char mo_c12_6[] = {0x02,0x0A,0x08,0x02,0x0A};
static const unsigned char mo_c12_7[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const unsigned char mo_c12_8[] = {0x02,0x06,0x04,0x02,0x06,0x04};
static const svc_move mv_c12[] = {
  {"전방전신(Zenpou Tenshin)", 0, mo_c12_2, 3, 0x10, 0, -1, -1, -1},
  {"파동권(Hadouken)", 0, mo_c12_3, 3, 0x10, 0, -1, -1, -1},
  {"승룡권(Shoryuken)", 0, mo_c12_4, 3, 0x10, 0, -1, -1, -1},
  {"용권선풍각(Tatsumaki Senpukyaku)", 0, mo_c12_5, 3, 0x20, 0, -1, -1, -1},
  {"승룡열파(Shoryu Reppa)", 0, mo_c12_6, 5, 0x10, 32, -1, -1, -1},
  {"신룡권(Shinryuken)", 0, mo_c12_7, 6, 0x20, 32, -1, -1, -1},
  {"질풍신뢰각(Shippu Jinraikyaku)", 0, mo_c12_8, 6, 0x20, 32, -1, -1, -1},
};
static const unsigned char mo_c13_0[] = {0x02,0x0A,0x08};
static const unsigned char mo_c13_1[] = {0x08,0x02,0x0A};
static const unsigned char mo_c13_2[] = {0x02,0x06,0x04};
static const unsigned char mo_c13_3[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const unsigned char mo_c13_4[] = {0x02,0x0A,0x08,0x02,0x0A};
static const unsigned char mo_c13_5[] = {0x02,0x06,0x04,0x02,0x06,0x04};
static const unsigned char mo_c13_6[] = {0x04,0x08,0x04,0x08};
static const svc_move mv_c13[] = {
  {"아도권(Gadouken)", 0, mo_c13_0, 3, 0x10, 0, -1, -1, -1},
  {"코류켄(Kouryuken)", 0, mo_c13_1, 3, 0x10, 0, -1, -1, -1},
  {"단공각(Dankukyaku)", 0, mo_c13_2, 3, 0x20, 0, -1, -1, -1},
  {"진공아도권(Shinku Gadouken)", 0, mo_c13_3, 6, 0x10, 32, -1, -1, -1},
  {"코류렉카(Kouryu Rekka)", 0, mo_c13_4, 5, 0x20, 32, -1, -1, -1},
  {"필승무뢰권(Hisshou Burai Ken)", 0, mo_c13_5, 6, 0x20, 32, -1, -1, -1},
  {"오토코미치(Otoko Michi)", 0, mo_c13_6, 4, 0x10, 56, -1, -1, -1},
};
static const unsigned char mo_c14_1[] = {0x02,0x0A,0x08};
static const unsigned char mo_c14_2[] = {0x08,0x02,0x0A};
static const unsigned char mo_c14_3[] = {0x02,0x06,0x04};
static const unsigned char mo_c14_4[] = {0x08,0x02,0x0A};
static const unsigned char mo_c14_5[] = {0x00};
static const unsigned char mo_c14_6[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const unsigned char mo_c14_7[] = {0x02,0x0A,0x08,0x02,0x0A};
static const unsigned char mo_c14_8[] = {0x02,0x06,0x04,0x02,0x06,0x04};
static const unsigned char mo_c14_9[] = {0x04,0x08,0x04,0x08};
static const svc_move mv_c14[] = {
  {"파동권(Hadouken)", 0, mo_c14_1, 3, 0x10, 0, -1, -1, -1},
  {"쇼오켄(Shououken)", 0, mo_c14_2, 3, 0x10, 0, -1, -1, -1},
  {"춘풍각(Shunpukyaku)", 0, mo_c14_3, 3, 0x20, 0, -1, -1, -1},
  {"사쿠라 오토시(Sakura Otoshi)", 0, mo_c14_4, 3, 0x20, 0, 4, -1, -1},
  {"사쿠라 오토시 추가타", 0, mo_c14_5, 1, 0x10, 0, -1, -1, -1},
  {"진공파동권(Shinku Hadouken)", 0, mo_c14_6, 6, 0x10, 32, -1, -1, -1},
  {"미다레자쿠라(Midare Zakura)", 0, mo_c14_7, 5, 0x20, 32, -1, -1, -1},
  {"하루이치방(Haru Ichiban)", 0, mo_c14_8, 6, 0x20, 32, -1, -1, -1},
  {"슌고쿠사츠(Shun Goku Satsu)", 0, mo_c14_9, 4, 0x10, 56, -1, -1, -1},
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
  {"셸 킥(Shell Kick)", 0, mo_c15_0, 1, 0x20, 4, -1, -1, -1},
  {"소울 피스트(Soul Fist)", 0, mo_c15_1, 3, 0x10, 0, -1, -1, -1},
  {"섀도 블레이드(Shadow Blade)", 0, mo_c15_2, 3, 0x10, 0, -1, -1, -1},
  {"벡터 드레인(Vector Drain)", 0, mo_c15_3, 5, 0x10, 1, -1, -1, -1},
  {"다크니스 일루전(Darkness Illusion)", 0, mo_c15_4, 6, 0x10, 32, -1, -1, -1},
  {"피니싱 샤워(Finishing Shower)", 0, mo_c15_5, 6, 0x20, 32, -1, -1, -1},
  {"발키리 턴(Valkyrie Turn)", 0, mo_c15_6, 5, 0x20, 32, -1, -1, -1},
  {"크립틱 니들(Cryptic Needle)", 0, mo_c15_7, 6, 0x10, 40, -1, -1, -1},
};
static const unsigned char mo_c16_0[] = {0x02,0x0A,0x08};
static const unsigned char mo_c16_1[] = {0x00};
static const unsigned char mo_c16_2[] = {0x08,0x02,0x0A};
static const unsigned char mo_c16_3[] = {0x08,0x02,0x0A};
static const unsigned char mo_c16_4[] = {0x08,0x0A,0x02,0x06,0x04};
static const unsigned char mo_c16_6[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const unsigned char mo_c16_7[] = {0x02,0x06,0x04,0x02,0x06,0x04};
static const unsigned char mo_c16_8[] = {0x02,0x0A,0x08,0x02,0x0A,0x08};
static const svc_move mv_c16[] = {
  {"롤링 버클러(Rolling Buckler)", 0, mo_c16_0, 3, 0x10, 0, 1, -1, -1},
  {"롤링 어퍼", 0, mo_c16_1, 1, 0x10, 0, -1, -1, -1},
  {"캣 스파이크(Cat Spike)", 0, mo_c16_2, 3, 0x10, 0, -1, -1, -1},
  {"델타 킥(Delta Kick)", 0, mo_c16_3, 3, 0x20, 0, -1, -1, -1},
  {"헬 캣(Hell Cat)", 0, mo_c16_4, 5, 0x20, 1, -1, -1, -1},
  {"댄싱 플래시(Dancing Flash)", 0, mo_c16_6, 6, 0x10, 32, -1, -1, -1},
  {"플리즈 헬프 미(Please Help Me)", 0, mo_c16_7, 6, 0x10, 32, -1, -1, -1},
  {"ES 롤링 스크래치(ES Rolling Scratch)", 0, mo_c16_8, 6, 0x20, 32, -1, -1, -1},
};
static const unsigned char mo_c17_2[] = {0x04,0x08};
static const unsigned char mo_c17_3[] = {0x02,0x01};
static const unsigned char mo_c17_4[] = {0x04,0x08,0x04,0x08};
static const unsigned char mo_c17_5[] = {0x06,0x0A,0x06,0x09};
static const unsigned char mo_c17_6[] = {0x04,0x08,0x04,0x08};
static const svc_move mv_c17[] = {
  {"소닉 붐(Sonic Boom)", "소닉 붐(강)", mo_c17_2, 2, 0x10, 16, -1, -1, -1},
  {"서머솔트 킥(Somersault Kick)", "서머솔트 킥(강)", mo_c17_3, 2, 0x20, 16, -1, -1, -1},
  {"소닉 허리케인(Sonic Hurricane)", 0, mo_c17_4, 4, 0x10, 56, -1, -1, -1},
  {"서머솔트 스트라이크(Somersault Strike)", 0, mo_c17_5, 4, 0x20, 48, -1, -1, -1},
  {"크로스파이어 블리츠(Crossfire Blitz)", 0, mo_c17_6, 4, 0x20, 56, -1, -1, -1},
};

/* 캐릭터별: 기술표 + 슬롯 7자리 (N F B D DF DB AIR — 값은 기술 인덱스, -1 없음) */
typedef struct { const svc_move *mv; unsigned char n; const char *name; unsigned char cancel_dud; signed char slots[7]; } svc_chartab;
#define SVC_CHAR_COUNT 18
static const svc_chartab svc_chars[SVC_CHAR_COUNT] = {
  { mv_c0, 17, "쿄", 1, {1,5,6,9,7,8,0} },
  { mv_c1, 8, "테리", 0, {0,4,3,1,5,2,-1} },
  { mv_c2, 8, "료", 0, {0,2,3,4,5,6,1} },
  { mv_c3, 10, "마이", 0, {1,2,3,5,4,4,6} },
  { mv_c4, 10, "레오나", 0, {5,3,1,2,8,0,4} },
  { mv_c5, 10, "아테나", 0, {1,2,4,6,5,7,0} },
  { mv_c6, 12, "이오리", 0, {1,2,3,7,6,8,0} },
  { mv_c7, 5, "하오마루", 0, {0,1,2,2,3,4,-1} },
  { mv_c8, 8, "나코루루", 0, {4,2,3,5,3,4,0} },
  { mv_c9, 7, "류", 0, {0,2,3,1,4,5,-1} },
  { mv_c10, 9, "춘리", 0, {1,4,3,2,5,5,0} },
  { mv_c11, 12, "장기에프", 0, {5,3,6,4,7,8,0} },
  { mv_c12, 7, "켄", 0, {1,2,3,0,4,5,-1} },
  { mv_c13, 7, "단", 0, {0,1,2,4,3,5,-1} },
  { mv_c14, 9, "사쿠라", 0, {0,1,2,3,5,6,-1} },
  { mv_c15, 8, "모리간", 0, {1,2,3,6,5,4,0} },
  { mv_c16, 8, "펠리시아", 0, {0,2,4,3,5,6,-1} },
  { mv_c17, 5, "가일", 0, {-1,-1,0,1,-1,-1,-1} },
};

#endif
