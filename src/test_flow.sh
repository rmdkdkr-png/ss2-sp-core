#!/bin/bash
# 관전 기억(흐름·총평) 단위 시험 — 에뮬레이터도 롬도 없이 돈다.
# ss2comm.c 를 램 흉내와 함께 직접 링크해서 시나리오를 먹인다.
set -e
cd "$(dirname "$0")"
cc -O1 -DSS2SP_RAM_POINTER -I. -o /tmp/ss2_test_flow test_flow.c ss2comm.c
/tmp/ss2_test_flow
