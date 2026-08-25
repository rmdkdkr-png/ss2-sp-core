/* 상황마다 해설이 실제로 뭐라고 하는지 그대로 찍는다.
 *
 *   cc -O1 -I. -o /tmp/render render_lines.c && /tmp/render
 *
 * ss2comm.c 의 emit_ex 와 같은 방식으로 고른다 — 널을 건너뛰며 압축한 뒤
 * vsel>=0 이면 cand[vsel < n ? vsel : n-1], vsel<0 이면 무작위.
 * 자리를 안 채우면 마지막 것으로 잘리는 성질까지 그대로 재현한다.
 *
 * 두 갈래로 나눠 찍는다:
 *   · 자리 뜻이 정해진 것 — ss2comm.c 가 vsel 을 0 이상으로 준다. 번호마다 뜻이 있다
 *   · 무작위로 도는 것     — vsel = -1. 변주가 다 살아 있으므로 전부 늘어놓는다
 */
#include <stdio.h>
#include <string.h>
#include "ss2comm_lines.h"

static const char *slot(int spk, int ev, int vsel){
    const char *cand[EVMAXV]; int n = 0, i;
    for(i = 0; i < EVMAXV; i++) if(LINES[spk][ev][i]) cand[n++] = LINES[spk][ev][i];
    if(!n) return "(없음)";
    return cand[vsel < n ? vsel : n-1];
}

static void fill(char *out, size_t cap, const char *fmt, const char *who, int n1, int n2){
    if(strstr(fmt, "%s"))        snprintf(out, cap, fmt, who ? who : "");
    else if(strstr(fmt, "%d")){
        const char *p = strstr(fmt, "%d");
        if(strstr(p+2, "%d"))    snprintf(out, cap, fmt, n1, n2);
        else                     snprintf(out, cap, fmt, n1);
    }
    else                         snprintf(out, cap, "%s", fmt);
}

/* 자리 번호가 뜻을 갖는 이벤트 */
static void fixed(const char *label, int spk, int ev, int vsel, const char *who, int n1, int n2){
    char out[512];
    fill(out, sizeof out, slot(spk, ev, vsel), who, n1, n2);
    printf("%s\t%s\n", label, out);
}

/* 무작위로 도는 이벤트 — 변주를 전부 늘어놓는다 */
static void all(const char *label, int spk, int ev, const char *who){
    char out[512]; int i, k = 0;
    for(i = 0; i < EVMAXV; i++){
        if(!LINES[spk][ev][i]) continue;
        fill(out, sizeof out, LINES[spk][ev][i], who, 0, 0);
        printf("%s %d\t%s\n", label, ++k, out);
    }
    if(!k) printf("%s\t(없음)\n", label);
}

int main(void){
    int s;
    for(s = 0; s < SS2COMM_SPK_N; s++){
        printf("\n[%s]\n", SPK_NAME[s]);
        /* ── 자리 뜻이 정해진 것 ── */
        fixed("오늘 이기고 있다", s, EV_RECORD, 0, 0, 10, 8);
        fixed("오늘 지고 있다",   s, EV_RECORD, 1, 0, 10, 2);
        fixed("생존 신기록",      s, EV_SURV,   0, 0, 14, 0);
        fixed("생존 10연승",      s, EV_SURV,   1, 0, 12, 0);
        fixed("생존 7연승",       s, EV_SURV,   2, 0,  8, 0);
        fixed("생존 3연승",       s, EV_SURV,   3, 0,  4, 0);
        fixed("스토리 8스테이지", s, EV_STAGE,  0, 0,  8, 0);
        fixed("스토리 5스테이지", s, EV_STAGE,  1, 0,  5, 0);
        fixed("5연승 중",         s, EV_STREAK, 0, 0,  6, 0);
        /* ── 무작위로 도는 것 ── */
        all("강타 적중",      s, EV_MOVEHIT,   "츠바메가에시");
        all("약타 적중",      s, EV_MOVEHITL,  "츠바메가에시");
        all("기술로 눕힘",    s, EV_MOVEDOWN,  "츠바메가에시");
        all("이름 없이 눕힘", s, EV_MOVEDOWNA, 0);
        all("기술로 KO",      s, EV_MOVEKO,    "츠바메가에시");
    }
    return 0;
}
