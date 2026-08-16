/* 자동 생성 — gen_moves.js. 수정하지 말 것. */
#ifndef SS2SP_MOVES_H
#define SS2SP_MOVES_H

typedef struct { const char *name; const unsigned char *motion; unsigned char len;
                unsigned char btn; unsigned char flags; } ss2_move;
typedef struct { const char *id; const ss2_move *mv; unsigned char n; } ss2_style;
/* flags: 1=near(근접) 2=card(카드필요) 4=air 8=unverified 16=grab */

static const unsigned char mo_kazuki_s_0[] = {0x2,0xa,0x8};
static const unsigned char mo_kazuki_s_1[] = {0x8,0x2,0xa};
static const unsigned char mo_kazuki_s_2[] = {0x2,0x6,0x4};
static const unsigned char mo_kazuki_s_3[] = {0x4,0x2,0x6};
static const unsigned char mo_kazuki_s_4[] = {0x8,0xa,0x2,0x6,0x4};
static const unsigned char mo_kazuki_s_5[] = {0x8,0x2,0xa};
static const ss2_move mv_kazuki_s[] = {
  {"부동격", mo_kazuki_s_0, 3, 16, 0},
  {"대폭살", mo_kazuki_s_1, 3, 16, 0},
  {"사이엔", mo_kazuki_s_2, 3, 32, 0},
  {"엔메츠", mo_kazuki_s_3, 3, 32, 0},
  {"치류엔", mo_kazuki_s_4, 5, 16, 10},
  {"추다엔진조", mo_kazuki_s_5, 3, 32, 11},
};
static const unsigned char mo_kazuki_bst_0[] = {0x4,0x2,0x6};
static const unsigned char mo_kazuki_bst_1[] = {0x8,0x2,0xa};
static const unsigned char mo_kazuki_bst_2[] = {0x2,0xa,0x8};
static const unsigned char mo_kazuki_bst_3[] = {0x4,0x2,0x6};
static const unsigned char mo_kazuki_bst_4[] = {0x8,0xa,0x2,0x6,0x4};
static const ss2_move mv_kazuki_bst[] = {
  {"화염격", mo_kazuki_bst_0, 3, 16, 0},
  {"엔네츠고쿠", mo_kazuki_bst_1, 3, 16, 0},
  {"리쿠도렛카", mo_kazuki_bst_2, 3, 16, 0},
  {"즈트쇼 (폭탄)", mo_kazuki_bst_3, 3, 32, 0},
  {"폭호참", mo_kazuki_bst_4, 5, 32, 0},
};
static const unsigned char mo_sogetsu_s_0[] = {0x8,0x2,0xa};
static const unsigned char mo_sogetsu_s_1[] = {0x2,0xa,0x8};
static const unsigned char mo_sogetsu_s_2[] = {0x2,0x6,0x4};
static const unsigned char mo_sogetsu_s_3[] = {0x2,0x6,0x4};
static const unsigned char mo_sogetsu_s_4[] = {0x2,0x6,0x4};
static const ss2_move mv_sogetsu_s[] = {
  {"월광", mo_sogetsu_s_0, 3, 16, 0},
  {"월륜파", mo_sogetsu_s_1, 3, 16, 0},
  {"소환", mo_sogetsu_s_2, 3, 32, 0},
  {"츠키가쿠레", mo_sogetsu_s_3, 3, 16, 0},
  {"시게츠", mo_sogetsu_s_4, 3, 32, 14},
};
static const unsigned char mo_sogetsu_bst_0[] = {0x8,0x2,0xa};
static const unsigned char mo_sogetsu_bst_1[] = {0x2,0x6,0x4};
static const unsigned char mo_sogetsu_bst_2[] = {0x2,0xa,0x8};
static const unsigned char mo_sogetsu_bst_3[] = {0x4,0x2,0x6};
static const ss2_move mv_sogetsu_bst[] = {
  {"쇼게츠", mo_sogetsu_bst_0, 3, 16, 0},
  {"스이게츠", mo_sogetsu_bst_1, 3, 16, 0},
  {"렌게스이부", mo_sogetsu_bst_2, 3, 16, 0},
  {"스이오텐신", mo_sogetsu_bst_3, 3, 32, 10},
};
static const unsigned char mo_haohmaru_s_0[] = {0x8,0x2,0xa};
static const unsigned char mo_haohmaru_s_1[] = {0x2,0xa,0x8};
static const unsigned char mo_haohmaru_s_2[] = {0x2,0x6,0x4};
static const unsigned char mo_haohmaru_s_3[] = {0x4,0x2,0x6};
static const unsigned char mo_haohmaru_s_4[] = {0x8,0x2,0xa};
static const unsigned char mo_haohmaru_s_5[] = {0x8,0xa,0x2,0x6,0x4};
static const ss2_move mv_haohmaru_s[] = {
  {"호월참", mo_haohmaru_s_0, 3, 16, 0},
  {"선풍열참", mo_haohmaru_s_1, 3, 16, 0},
  {"술병 치기", mo_haohmaru_s_2, 3, 32, 0},
  {"열진참", mo_haohmaru_s_3, 3, 16, 0},
  {"오월인", mo_haohmaru_s_4, 3, 32, 1},
  {"천파성왕참", mo_haohmaru_s_5, 5, 16, 10},
};
static const unsigned char mo_haohmaru_bst_0[] = {0x8,0x2,0xa};
static const unsigned char mo_haohmaru_bst_1[] = {0x2,0xa,0x8};
static const unsigned char mo_haohmaru_bst_2[] = {0x8,0x2,0xa};
static const unsigned char mo_haohmaru_bst_3[] = {0x4,0x2,0x6};
static const unsigned char mo_haohmaru_bst_4[] = {0x4,0x6,0x2,0xa,0x8};
static const ss2_move mv_haohmaru_bst[] = {
  {"호월참", mo_haohmaru_bst_0, 3, 16, 0},
  {"선풍열참", mo_haohmaru_bst_1, 3, 16, 0},
  {"강파", mo_haohmaru_bst_2, 3, 32, 0},
  {"열진참", mo_haohmaru_bst_3, 3, 16, 0},
  {"도도단", mo_haohmaru_bst_4, 5, 32, 10},
};
static const unsigned char mo_genjuro_s_0[] = {0x8,0x2,0xa};
static const unsigned char mo_genjuro_s_1[] = {0x2,0x6,0x4};
static const unsigned char mo_genjuro_s_2[] = {0x2,0xa,0x8};
static const unsigned char mo_genjuro_s_3[] = {0x8,0x2,0xa};
static const unsigned char mo_genjuro_s_4[] = {0x2,0xa,0x8};
static const ss2_move mv_genjuro_s[] = {
  {"광양인", mo_genjuro_s_0, 3, 16, 0},
  {"앵화참", mo_genjuro_s_1, 3, 16, 0},
  {"삼련살", mo_genjuro_s_2, 3, 16, 0},
  {"시즈쿠진", mo_genjuro_s_3, 3, 32, 1},
  {"키카즈키와리", mo_genjuro_s_4, 3, 32, 2},
};
static const unsigned char mo_genjuro_bst_0[] = {0x2,0xa,0x8};
static const unsigned char mo_genjuro_bst_1[] = {0x8,0xa,0x2,0x6,0x4};
static const unsigned char mo_genjuro_bst_2[] = {0x4,0x2,0x6};
static const unsigned char mo_genjuro_bst_3[] = {0x8,0x2,0xa};
static const ss2_move mv_genjuro_bst[] = {
  {"백귀살", mo_genjuro_bst_0, 3, 16, 0},
  {"가신토츠", mo_genjuro_bst_1, 5, 16, 0},
  {"시구레", mo_genjuro_bst_2, 3, 16, 0},
  {"히키즈리마와시", mo_genjuro_bst_3, 3, 32, 3},
};
static const unsigned char mo_nakoruru_s_0[] = {0x8,0x2,0xa};
static const unsigned char mo_nakoruru_s_1[] = {0x4,0x6,0x2};
static const unsigned char mo_nakoruru_s_2[] = {0x4,0x2,0x6};
static const unsigned char mo_nakoruru_s_3[] = {0x2,0xa,0x8};
static const unsigned char mo_nakoruru_s_4[] = {0x2,0x6,0x4};
static const unsigned char mo_nakoruru_s_5[] = {0x8,0x2,0xa};
static const ss2_move mv_nakoruru_s[] = {
  {"레라 무츠베", mo_nakoruru_s_0, 3, 16, 0},
  {"안누 무츠베", mo_nakoruru_s_1, 3, 16, 0},
  {"카무이 릿세", mo_nakoruru_s_2, 3, 16, 0},
  {"시치카프 아무", mo_nakoruru_s_3, 3, 16, 0},
  {"마마하하 타기", mo_nakoruru_s_4, 3, 32, 0},
  {"레라 오 치키리", mo_nakoruru_s_5, 3, 32, 1},
};
static const unsigned char mo_nakoruru_bst_0[] = {0x2,0xa,0x8};
static const unsigned char mo_nakoruru_bst_1[] = {0x2,0xa,0x8};
static const unsigned char mo_nakoruru_bst_2[] = {0x4,0x6,0x2};
static const unsigned char mo_nakoruru_bst_3[] = {0x8,0x2,0xa};
static const unsigned char mo_nakoruru_bst_4[] = {0x2,0x6,0x4};
static const unsigned char mo_nakoruru_bst_5[] = {0x8,0x2,0xa};
static const ss2_move mv_nakoruru_bst[] = {
  {"카무이 시키테", mo_nakoruru_bst_0, 3, 16, 0},
  {"싯소 루텐쿄게키하", mo_nakoruru_bst_1, 3, 32, 0},
  {"안누 무츠베", mo_nakoruru_bst_2, 3, 16, 0},
  {"레라 무츠베", mo_nakoruru_bst_3, 3, 16, 0},
  {"시크루 타기", mo_nakoruru_bst_4, 3, 32, 0},
  {"엔부 코캬쿠", mo_nakoruru_bst_5, 3, 32, 11},
};
static const unsigned char mo_rimururu_s_0[] = {0x2,0xa,0x8};
static const unsigned char mo_rimururu_s_1[] = {0x8,0x2,0xa};
static const unsigned char mo_rimururu_s_2[] = {0x2,0x6,0x4};
static const unsigned char mo_rimururu_s_3[] = {0x2};
static const unsigned char mo_rimururu_s_4[] = {0x8,0x2,0xa};
static const unsigned char mo_rimururu_s_5[] = {0x2,0xa,0x8};
static const ss2_move mv_rimururu_s[] = {
  {"루푸스 콰레", mo_rimururu_s_0, 3, 16, 0},
  {"콘루 논노", mo_rimururu_s_1, 3, 16, 0},
  {"우푼 오푸", mo_rimururu_s_2, 3, 16, 0},
  {"콘루 시라루", mo_rimururu_s_3, 1, 32, 12},
  {"에타이에 시노", mo_rimururu_s_4, 3, 32, 9},
  {"투케폰 온카미쿠루", mo_rimururu_s_5, 3, 32, 11},
};
static const unsigned char mo_rimururu_bst_0[] = {0x8,0xa,0x2,0x6,0x4};
static const unsigned char mo_rimururu_bst_1[] = {0x2,0x6,0x4};
static const unsigned char mo_rimururu_bst_2[] = {0x4,0x2,0x6};
static const unsigned char mo_rimururu_bst_3[] = {0x2};
static const unsigned char mo_rimururu_bst_4[] = {0x8,0x2,0xa};
static const unsigned char mo_rimururu_bst_5[] = {0x2,0xa,0x8};
static const unsigned char mo_rimururu_bst_6[] = {0x4,0x2,0x6};
static const ss2_move mv_rimururu_bst[] = {
  {"루푸 테쿠 누무", mo_rimururu_bst_0, 5, 32, 0},
  {"콘루 멤", mo_rimururu_bst_1, 3, 16, 0},
  {"카무이 시토우키", mo_rimururu_bst_2, 3, 16, 0},
  {"콘루 시라루", mo_rimururu_bst_3, 1, 32, 4},
  {"아르카 아르카", mo_rimururu_bst_4, 3, 32, 1},
  {"피리카노 온얀", mo_rimururu_bst_5, 3, 32, 11},
  {"루푸스 토우무", mo_rimururu_bst_6, 3, 32, 10},
};
static const unsigned char mo_hanzo_s_0[] = {0x2,0xa,0x8};
static const unsigned char mo_hanzo_s_1[] = {0x2,0x6,0x4};
static const unsigned char mo_hanzo_s_2[] = {0x2,0x6,0x4};
static const unsigned char mo_hanzo_s_3[] = {0x8,0x2,0xa};
static const unsigned char mo_hanzo_s_4[] = {0x8,0x4,0x6,0x2,0xa,0x8};
static const unsigned char mo_hanzo_s_5[] = {0x4,0x8,0xa,0x2,0x6,0x4};
static const unsigned char mo_hanzo_s_6[] = {0x4,0x8,0xa,0x2,0x6,0x4};
static const ss2_move mv_hanzo_s[] = {
  {"열풍수리검", mo_hanzo_s_0, 3, 16, 0},
  {"폭룡파", mo_hanzo_s_1, 3, 16, 0},
  {"폭염진", mo_hanzo_s_2, 3, 32, 0},
  {"폭염미진가쿠레", mo_hanzo_s_3, 3, 32, 0},
  {"그림자분신", mo_hanzo_s_4, 6, 16, 0},
  {"우츠세미 천무", mo_hanzo_s_5, 6, 16, 0},
  {"우츠세미 지참", mo_hanzo_s_6, 6, 32, 0},
};
static const unsigned char mo_hanzo_bst_0[] = {0x4,0x2,0x6};
static const unsigned char mo_hanzo_bst_1[] = {0x8,0x4,0x6,0x2,0xa,0x8};
static const unsigned char mo_hanzo_bst_2[] = {0x8,0x2,0xa};
static const unsigned char mo_hanzo_bst_3[] = {0x8,0x2,0xa};
static const unsigned char mo_hanzo_bst_4[] = {0x8,0x2,0xa};
static const unsigned char mo_hanzo_bst_5[] = {0x2,0xa,0x8};
static const unsigned char mo_hanzo_bst_6[] = {0x4,0x6,0x2,0xa,0x8};
static const unsigned char mo_hanzo_bst_7[] = {0x8,0xa,0x2,0x6,0x4};
static const ss2_move mv_hanzo_bst[] = {
  {"키루마이", mo_hanzo_bst_0, 3, 16, 0},
  {"그림자분신", mo_hanzo_bst_1, 6, 16, 0},
  {"모즈 떨구기", mo_hanzo_bst_2, 3, 32, 0},
  {"카스미", mo_hanzo_bst_3, 3, 16, 0},
  {"인 (은신·미검증)", mo_hanzo_bst_4, 3, 16, 8},
  {"거미달리기", mo_hanzo_bst_5, 3, 32, 0},
  {"바츠 (잡기)", mo_hanzo_bst_6, 5, 32, 17},
  {"라이 (잡기)", mo_hanzo_bst_7, 5, 32, 17},
};
static const unsigned char mo_galford_s_0[] = {0x2,0xa,0x8};
static const unsigned char mo_galford_s_1[] = {0x2,0x6,0x4};
static const unsigned char mo_galford_s_2[] = {0x2,0xa,0x8};
static const unsigned char mo_galford_s_3[] = {0x8,0x4,0x6,0x2,0xa,0x8};
static const unsigned char mo_galford_s_4[] = {0x4,0x8,0xa,0x2,0x6,0x4};
static const unsigned char mo_galford_s_5[] = {0x2,0x6,0x4};
static const ss2_move mv_galford_s[] = {
  {"러시 독", mo_galford_s_0, 3, 16, 0},
  {"스트라이크 독", mo_galford_s_1, 3, 16, 0},
  {"오버헤드 크래시", mo_galford_s_2, 3, 32, 0},
  {"그림자분신", mo_galford_s_3, 6, 16, 0},
  {"레플리카 어택", mo_galford_s_4, 6, 16, 0},
  {"머신건 독 (#59)", mo_galford_s_5, 3, 32, 2},
};
static const unsigned char mo_galford_bst_0[] = {0x2,0xa,0x8};
static const unsigned char mo_galford_bst_1[] = {0x8,0x2,0xa};
static const unsigned char mo_galford_bst_2[] = {0x8,0x2,0xa};
static const unsigned char mo_galford_bst_3[] = {0x2,0x6,0x4};
static const unsigned char mo_galford_bst_4[] = {0x8,0x4,0x6,0x2,0xa,0x8};
static const unsigned char mo_galford_bst_5[] = {0x4,0x8,0xa,0x2,0x6,0x4};
static const ss2_move mv_galford_bst[] = {
  {"플라즈마 블레이드", mo_galford_bst_0, 3, 16, 0},
  {"플라즈마 브레이크", mo_galford_bst_1, 3, 16, 0},
  {"스트라이크 헤즈", mo_galford_bst_2, 3, 32, 1},
  {"라이트닝 슬래시", mo_galford_bst_3, 3, 32, 0},
  {"그림자분신", mo_galford_bst_4, 6, 16, 0},
  {"레플리카 어택", mo_galford_bst_5, 6, 16, 0},
};
static const unsigned char mo_asura_s_0[] = {0x2,0xa,0x8};
static const unsigned char mo_asura_s_1[] = {0x8,0x2,0xa};
static const unsigned char mo_asura_s_2[] = {0x4,0x2,0x6};
static const unsigned char mo_asura_s_3[] = {0x4,0x2,0x6};
static const unsigned char mo_asura_s_4[] = {0x8,0xa,0x2,0x6,0x4};
static const unsigned char mo_asura_s_5[] = {0x2,0xa,0x8};
static const unsigned char mo_asura_s_6[] = {0x8,0x2,0xa};
static const ss2_move mv_asura_s[] = {
  {"셰드 무사", mo_asura_s_0, 3, 16, 0},
  {"로페 레프", mo_asura_s_1, 3, 16, 0},
  {"나바이베루", mo_asura_s_2, 3, 16, 0},
  {"노 아무", mo_asura_s_3, 3, 32, 0},
  {"레피 슐", mo_asura_s_4, 5, 16, 8},
  {"에레스 (#68)", mo_asura_s_5, 3, 32, 2},
  {"베엘제붑 (#67)", mo_asura_s_6, 3, 32, 3},
};
static const unsigned char mo_asura_bst_0[] = {0x2,0xa,0x8};
static const unsigned char mo_asura_bst_1[] = {0x8,0x2,0xa};
static const unsigned char mo_asura_bst_2[] = {0x8,0xa,0x2,0x6,0x4};
static const unsigned char mo_asura_bst_3[] = {0x2,0xa,0x8};
static const unsigned char mo_asura_bst_4[] = {0x4,0x2,0x6};
static const unsigned char mo_asura_bst_5[] = {0x8,0x2,0xa};
static const ss2_move mv_asura_bst[] = {
  {"르 이유 바케", mo_asura_bst_0, 3, 16, 0},
  {"레 말스", mo_asura_bst_1, 3, 16, 0},
  {"레 시마", mo_asura_bst_2, 5, 16, 0},
  {"르 태그 라구", mo_asura_bst_3, 3, 32, 0},
  {"노바 라템", mo_asura_bst_4, 3, 32, 10},
  {"르 커스", mo_asura_bst_5, 3, 32, 10},
};
static const unsigned char mo_charlotte_s_0[] = {0x8,0x2,0xa};
static const unsigned char mo_charlotte_s_1[] = {0x2,0xa,0x8};
static const unsigned char mo_charlotte_s_2[] = {0x2,0x6,0x4};
static const unsigned char mo_charlotte_s_3[] = {0x8,0x2,0xa};
static const ss2_move mv_charlotte_s[] = {
  {"파워 그라데이션", mo_charlotte_s_0, 3, 16, 0},
  {"트라이어드 슬래시", mo_charlotte_s_1, 3, 16, 0},
  {"스플래시 페인트", mo_charlotte_s_2, 3, 32, 10},
  {"세르주 랜스", mo_charlotte_s_3, 3, 32, 11},
};
static const unsigned char mo_charlotte_bst_0[] = {0x8,0x2,0xa};
static const unsigned char mo_charlotte_bst_1[] = {0x2,0xa,0x8};
static const unsigned char mo_charlotte_bst_2[] = {0x4,0x2,0x6};
static const unsigned char mo_charlotte_bst_3[] = {0x8,0x2,0xa};
static const ss2_move mv_charlotte_bst[] = {
  {"파워 그라데이션", mo_charlotte_bst_0, 3, 16, 0},
  {"바이오 넷 러시", mo_charlotte_bst_1, 3, 16, 0},
  {"오퍼드 로즈", mo_charlotte_bst_2, 3, 32, 10},
  {"리옹 랜스", mo_charlotte_bst_3, 3, 32, 11},
};
static const unsigned char mo_morozumi_s_0[] = {0x2,0xa,0x8};
static const unsigned char mo_morozumi_s_1[] = {0x8,0xa,0x2,0x6,0x4};
static const unsigned char mo_morozumi_s_2[] = {0x8,0x2,0xa};
static const unsigned char mo_morozumi_s_3[] = {0x4,0x2,0x6};
static const unsigned char mo_morozumi_s_4[] = {0x8,0x2,0xa};
static const ss2_move mv_morozumi_s[] = {
  {"화염", mo_morozumi_s_0, 3, 16, 0},
  {"바람", mo_morozumi_s_1, 5, 16, 0},
  {"낙뢰", mo_morozumi_s_2, 3, 16, 0},
  {"참격", mo_morozumi_s_3, 3, 16, 0},
  {"파괴", mo_morozumi_s_4, 3, 32, 10},
};
static const unsigned char mo_morozumi_bst_0[] = {0x2,0xa,0x8};
static const unsigned char mo_morozumi_bst_1[] = {0x8,0xa,0x2,0x6,0x4};
static const unsigned char mo_morozumi_bst_2[] = {0x2,0xa,0x8};
static const unsigned char mo_morozumi_bst_3[] = {0x8,0x2,0xa};
static const unsigned char mo_morozumi_bst_4[] = {0x4,0x2,0x6};
static const ss2_move mv_morozumi_bst[] = {
  {"화염", mo_morozumi_bst_0, 3, 16, 0},
  {"분화", mo_morozumi_bst_1, 5, 16, 0},
  {"봉인", mo_morozumi_bst_2, 3, 32, 0},
  {"재앙", mo_morozumi_bst_3, 3, 32, 10},
  {"응보", mo_morozumi_bst_4, 3, 32, 10},
};
static const unsigned char mo_ukyo_s_0[] = {0x2,0xa,0x8};
static const unsigned char mo_ukyo_s_1[] = {0x2,0x6,0x4};
static const unsigned char mo_ukyo_s_2[] = {0x8,0xa,0x2,0x6,0x4};
static const unsigned char mo_ukyo_s_3[] = {0x8,0x2,0xa};
static const unsigned char mo_ukyo_s_4[] = {0x8,0xa,0x2,0x6,0x4};
static const ss2_move mv_ukyo_s[] = {
  {"츠바메 가에시", mo_ukyo_s_0, 3, 16, 4},
  {"히켄 사사메유키", mo_ukyo_s_1, 3, 16, 0},
  {"천상의 자세", mo_ukyo_s_2, 5, 32, 0},
  {"나다레", mo_ukyo_s_3, 3, 32, 1},
  {"슌무니렌게", mo_ukyo_s_4, 5, 16, 10},
};
static const unsigned char mo_ukyo_bst_0[] = {0x2,0xa,0x8};
static const unsigned char mo_ukyo_bst_1[] = {0x4,0x2,0x6};
static const unsigned char mo_ukyo_bst_2[] = {0x4,0x2,0x6};
static const unsigned char mo_ukyo_bst_3[] = {0x2,0x6,0x4};
static const unsigned char mo_ukyo_bst_4[] = {0x2,0x6,0x4};
static const unsigned char mo_ukyo_bst_5[] = {0x2,0xa,0x8};
static const ss2_move mv_ukyo_bst[] = {
  {"츠바메 가에시", mo_ukyo_bst_0, 3, 16, 4},
  {"아사나기", mo_ukyo_bst_1, 3, 16, 0},
  {"유우나기", mo_ukyo_bst_2, 3, 32, 0},
  {"무소 카스미(상)", mo_ukyo_bst_3, 3, 16, 0},
  {"무소 카스미(하)", mo_ukyo_bst_4, 3, 32, 0},
  {"오보로가타나", mo_ukyo_bst_5, 3, 16, 10},
};
static const unsigned char mo_jubei_s_0[] = {0x2,0xa,0x8};
static const unsigned char mo_jubei_s_1[] = {0x8,0x2,0xa};
static const unsigned char mo_jubei_s_2[] = {0x2,0x6,0x4};
static const unsigned char mo_jubei_s_3[] = {0x8,0x2,0xa};
static const ss2_move mv_jubei_s[] = {
  {"간헐돌", mo_jubei_s_0, 3, 16, 0},
  {"츠나미 사브르", mo_jubei_s_1, 3, 16, 0},
  {"사브르 스러스트", mo_jubei_s_2, 3, 16, 0},
  {"비전 잡기", mo_jubei_s_3, 3, 32, 19},
};
static const unsigned char mo_jubei_bst_0[] = {0x2,0xa,0x8};
static const unsigned char mo_jubei_bst_1[] = {0x2,0x6,0x4};
static const unsigned char mo_jubei_bst_2[] = {0x2,0xa,0x8};
static const unsigned char mo_jubei_bst_3[] = {0x2,0x6,0x4};
static const unsigned char mo_jubei_bst_4[] = {0x8,0x2,0xa};
static const unsigned char mo_jubei_bst_5[] = {0x4,0x2,0x6};
static const ss2_move mv_jubei_bst[] = {
  {"간헐돌", mo_jubei_bst_0, 3, 16, 0},
  {"필멸 베기", mo_jubei_bst_1, 3, 16, 0},
  {"천멸 베기", mo_jubei_bst_2, 3, 32, 0},
  {"간헐 필살돌", mo_jubei_bst_3, 3, 32, 0},
  {"아아아", mo_jubei_bst_4, 3, 32, 2},
  {"롤리팝", mo_jubei_bst_5, 3, 32, 2},
};
static const unsigned char mo_shiki_s_0[] = {0x8,0x2,0xa};
static const unsigned char mo_shiki_s_1[] = {0x2,0xa,0x8};
static const unsigned char mo_shiki_s_2[] = {0x4,0x2,0x6};
static const unsigned char mo_shiki_s_3[] = {0x2,0xa,0x8};
static const unsigned char mo_shiki_s_4[] = {0x2,0x6,0x4};
static const unsigned char mo_shiki_s_5[] = {0x8,0x2,0xa};
static const unsigned char mo_shiki_s_6[] = {0x2,0x4,0x8};
static const ss2_move mv_shiki_s[] = {
  {"천봉린", mo_shiki_s_0, 3, 16, 0},
  {"츠유바라이", mo_shiki_s_1, 3, 16, 0},
  {"쿠비카타나", mo_shiki_s_2, 3, 32, 0},
  {"쿠게[아]", mo_shiki_s_3, 3, 32, 8},
  {"쿠게[운]", mo_shiki_s_4, 3, 32, 8},
  {"요미오토시", mo_shiki_s_5, 3, 32, 1},
  {"렌게마이", mo_shiki_s_6, 3, 32, 10},
};
static const unsigned char mo_shiki_bst_0[] = {0x2,0x6,0x4};
static const unsigned char mo_shiki_bst_1[] = {0x2,0xa,0x8};
static const unsigned char mo_shiki_bst_2[] = {0x2,0x4,0x8};
static const unsigned char mo_shiki_bst_3[] = {0x4,0x2,0x6};
static const unsigned char mo_shiki_bst_4[] = {0x4,0x2,0x6};
static const unsigned char mo_shiki_bst_5[] = {0x4,0x6,0x2};
static const unsigned char mo_shiki_bst_6[] = {0x8,0x4,0x6,0x2,0xa,0x8};
static const unsigned char mo_shiki_bst_7[] = {0x8,0x2,0xa};
static const ss2_move mv_shiki_bst[] = {
  {"세츠나", mo_shiki_bst_0, 3, 16, 0},
  {"텐보린", mo_shiki_bst_1, 3, 16, 0},
  {"무묘", mo_shiki_bst_2, 3, 32, 0},
  {"무쇼", mo_shiki_bst_3, 3, 16, 0},
  {"무슈카", mo_shiki_bst_4, 3, 32, 0},
  {"타쿠메츠", mo_shiki_bst_5, 3, 32, 0},
  {"지비", mo_shiki_bst_6, 6, 32, 10},
  {"소쿠진", mo_shiki_bst_7, 3, 32, 10},
};
static const unsigned char mo_yuga_s_0[] = {0x2,0xa,0x8};
static const unsigned char mo_yuga_s_1[] = {0x8,0x2,0xa};
static const unsigned char mo_yuga_s_2[] = {0x2,0x6,0x4};
static const unsigned char mo_yuga_s_3[] = {0x2,0x6,0x4};
static const unsigned char mo_yuga_s_4[] = {0x8,0x2,0xa};
static const ss2_move mv_yuga_s[] = {
  {"웨다가", mo_yuga_s_0, 3, 16, 0},
  {"루다", mo_yuga_s_1, 3, 16, 0},
  {"쿠샤나", mo_yuga_s_2, 3, 16, 0},
  {"다라니", mo_yuga_s_3, 3, 32, 0},
  {"나라카", mo_yuga_s_4, 3, 32, 10},
};
static const unsigned char mo_yuga_bst_0[] = {0x2,0xa,0x8};
static const unsigned char mo_yuga_bst_1[] = {0x8,0x2,0xa};
static const unsigned char mo_yuga_bst_2[] = {0x2,0x6,0x4};
static const unsigned char mo_yuga_bst_3[] = {0x2,0x6,0x4};
static const unsigned char mo_yuga_bst_4[] = {0x2,0xa,0x8};
static const unsigned char mo_yuga_bst_5[] = {0x8,0x2,0xa};
static const unsigned char mo_yuga_bst_6[] = {0x8,0x4,0x6,0x2,0xa,0x8};
static const ss2_move mv_yuga_bst[] = {
  {"부야우아하", mo_yuga_bst_0, 3, 16, 0},
  {"타부타쿤바", mo_yuga_bst_1, 3, 16, 0},
  {"마하아주아라", mo_yuga_bst_2, 3, 16, 0},
  {"아루가", mo_yuga_bst_3, 3, 32, 0},
  {"아시바토라우아나", mo_yuga_bst_4, 3, 16, 4},
  {"산단샤", mo_yuga_bst_5, 3, 32, 0},
  {"카루나", mo_yuga_bst_6, 6, 32, 10},
};

/* SP 슬롯 기본 배치 (deriveSpMap + 캐릭터 오버라이드). 순서: n f b d df db air, -1 = 없음 */
static const signed char ss2_spmap[][7] = {
  {0,1,2,3,-1,-1,-1},  /* kazuki_s */
  {2,1,4,0,-1,-1,-1},  /* kazuki_bst */
  {1,0,2,3,-1,-1,-1},  /* sogetsu_s */
  {2,0,1,-1,-1,-1,-1},  /* sogetsu_bst */
  {1,0,3,4,-1,-1,-1},  /* haohmaru_s */
  {1,0,3,2,-1,-1,-1},  /* haohmaru_bst */
  {2,0,1,3,-1,-1,-1},  /* genjuro_s */
  {0,1,2,-1,-1,-1,-1},  /* genjuro_bst */
  {3,0,1,5,-1,-1,-1},  /* nakoruru_s */
  {0,3,2,1,-1,-1,-1},  /* nakoruru_bst */
  {0,1,2,-1,-1,-1,-1},  /* rimururu_s */
  {2,0,1,4,-1,-1,3},  /* rimururu_bst */
  {0,3,1,2,-1,-1,-1},  /* hanzo_s */
  {5,1,0,6,-1,-1,-1},  /* hanzo_bst */
  {0,3,1,2,-1,-1,-1},  /* galford_s */
  {0,1,3,2,-1,-1,-1},  /* galford_bst */
  {0,1,2,3,-1,-1,-1},  /* asura_s */
  {0,1,3,2,-1,-1,-1},  /* asura_bst */
  {1,0,-1,-1,-1,-1,-1},  /* charlotte_s */
  {1,0,-1,-1,-1,-1,-1},  /* charlotte_bst */
  {0,1,3,2,-1,-1,-1},  /* morozumi_s */
  {0,1,-1,2,-1,-1,-1},  /* morozumi_bst */
  {1,2,1,3,-1,-1,0},  /* ukyo_s */
  {2,3,1,4,-1,-1,0},  /* ukyo_bst */
  {0,1,2,-1,-1,-1,-1},  /* jubei_s */
  {0,2,1,3,-1,-1,-1},  /* jubei_bst */
  {1,0,2,5,-1,-1,-1},  /* shiki_s */
  {1,2,0,3,-1,-1,-1},  /* shiki_bst */
  {0,1,2,3,-1,-1,-1},  /* yuga_s */
  {0,1,2,3,-1,-1,4},  /* yuga_bst */
};
#define SS2_SLOT_N 0
#define SS2_SLOT_F 1
#define SS2_SLOT_B 2
#define SS2_SLOT_D 3
#define SS2_SLOT_DF 4
#define SS2_SLOT_DB 5
#define SS2_SLOT_AIR 6

/* 인덱스 = (0x1B51 >> 3). 내부 캐릭터 순서 = CHAR_BY_IDX (runner.html과 동일) */
static const ss2_style ss2_styles[] = {
  {"kazuki_s", mv_kazuki_s, 6},
  {"kazuki_bst", mv_kazuki_bst, 5},
  {"sogetsu_s", mv_sogetsu_s, 5},
  {"sogetsu_bst", mv_sogetsu_bst, 4},
  {"haohmaru_s", mv_haohmaru_s, 6},
  {"haohmaru_bst", mv_haohmaru_bst, 5},
  {"genjuro_s", mv_genjuro_s, 5},
  {"genjuro_bst", mv_genjuro_bst, 4},
  {"nakoruru_s", mv_nakoruru_s, 6},
  {"nakoruru_bst", mv_nakoruru_bst, 6},
  {"rimururu_s", mv_rimururu_s, 6},
  {"rimururu_bst", mv_rimururu_bst, 7},
  {"hanzo_s", mv_hanzo_s, 7},
  {"hanzo_bst", mv_hanzo_bst, 8},
  {"galford_s", mv_galford_s, 6},
  {"galford_bst", mv_galford_bst, 6},
  {"asura_s", mv_asura_s, 7},
  {"asura_bst", mv_asura_bst, 6},
  {"charlotte_s", mv_charlotte_s, 4},
  {"charlotte_bst", mv_charlotte_bst, 4},
  {"morozumi_s", mv_morozumi_s, 5},
  {"morozumi_bst", mv_morozumi_bst, 5},
  {"ukyo_s", mv_ukyo_s, 5},
  {"ukyo_bst", mv_ukyo_bst, 6},
  {"jubei_s", mv_jubei_s, 4},
  {"jubei_bst", mv_jubei_bst, 6},
  {"shiki_s", mv_shiki_s, 7},
  {"shiki_bst", mv_shiki_bst, 8},
  {"yuga_s", mv_yuga_s, 5},
  {"yuga_bst", mv_yuga_bst, 7},
};
#define SS2_STYLE_COUNT 30

#endif
