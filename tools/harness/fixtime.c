/* 롬이 RTC(time/localtime)를 읽어 실행마다 갈라지지 않게 시각을 고정한다 — LD_PRELOAD 용.
   코어의 «120Hz 안 나옴» 감시(clock_gettime CLOCK_MONOTONIC 으로 호출 속도를 잼)가 덤프로 느려진 실행에서 걸려 프레임 생성을 끄고
   판이 갈라지는 일이 있어, 단조 시계도 «부를 때마다 4ms» 로 고정한다(≈240/s — 늘 120Hz 로 보인다). 다른 시계는 그대로. */
#include <time.h>
#include <stddef.h>
#include <stdint.h>
time_t time(time_t *t) { time_t v = 1700000000; if (t) *t = v; return v; }
int clock_gettime(clockid_t id, struct timespec *ts)
{
   static uint64_t ns = 1000000000ULL;
   (void)id;
   ns += 4000000ULL;
   if (ts) { ts->tv_sec = (time_t)(ns / 1000000000ULL); ts->tv_nsec = (long)(ns % 1000000000ULL); }
   return 0;
}
