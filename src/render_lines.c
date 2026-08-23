/* 고정 자리(vsel>=0)로 불리는 상황에서 화자가 실제로 뭐라고 하는지 그대로 찍는다.
   ss2comm.c 의 emit_ex 와 같은 방식으로 고른다 — 널을 건너뛰며 압축한 뒤
   cand[vsel < n ? vsel : n-1]. 그래야 「자리를 안 채우면 마지막 것으로 잘린다」가 그대로 재현된다. */
#include <stdio.h>
#include <string.h>
#include "ss2comm_lines.h"

static const char *pick(int spk, int ev, int vsel){
    const char *cand[EVMAXV]; int n = 0, i;
    for(i = 0; i < EVMAXV; i++) if(LINES[spk][ev][i]) cand[n++] = LINES[spk][ev][i];
    if(!n) return "(없음)";
    return cand[vsel < n ? vsel : n-1];
}

/* emit_ex 의 서식 처리와 같게 */
static void say(const char *label, int spk, int ev, int vsel, const char *who, int n1, int n2){
    const char *fmt = pick(spk, ev, vsel);
    char out[512];
    if(strstr(fmt, "%s"))        snprintf(out, sizeof out, fmt, who ? who : "");
    else if(strstr(fmt, "%d")){
        const char *p = strstr(fmt, "%d");
        if(strstr(p+2, "%d"))    snprintf(out, sizeof out, fmt, n1, n2);
        else                     snprintf(out, sizeof out, fmt, n1);
    }
    else                         snprintf(out, sizeof out, "%s", fmt);
    printf("    %-22s %s\n", label, out);
}

int main(void){
    int s;
    for(s = 0; s < SS2COMM_SPK_N; s++){
        printf("\n[%s]\n", SPK_NAME[s]);
        say("오늘 이기고 있다",   s, EV_RECORD,   0, 0, 10, 8);
        say("오늘 지고 있다",     s, EV_RECORD,   1, 0, 10, 2);
        say("생존 신기록",        s, EV_SURV,     0, 0, 14, 0);
        say("생존 10연승",        s, EV_SURV,     1, 0, 12, 0);
        say("생존 7연승",         s, EV_SURV,     2, 0,  8, 0);
        say("생존 3연승",         s, EV_SURV,     3, 0,  4, 0);
        say("스토리 8스테이지",   s, EV_STAGE,    0, 0,  8, 0);
        say("스토리 5스테이지",   s, EV_STAGE,    1, 0,  5, 0);
        say("5연승 중",           s, EV_STREAK,   0, 0,  6, 0);
        say("강타 적중",          s, EV_MOVEHIT,  0, "츠바메가에시", 0, 0);
        say("약타 적중",          s, EV_MOVEHIT,  2, "츠바메가에시", 0, 0);
        say("기술로 눕힘",        s, EV_MOVEDOWN, 0, "츠바메가에시", 0, 0);
        say("이름 없이 눕힘",     s, EV_MOVEDOWN, 1, 0, 0, 0);
        say("기술로 KO",          s, EV_MOVEKO,   0, "츠바메가에시", 0, 0);
    }
    return 0;
}
