#!/bin/bash
# 관전 기억(흐름·총평) 단위 시험 — 에뮬레이터도 롬도 없이 돈다.
# ss2comm.c 를 램 흉내와 함께 직접 링크해서 시나리오를 먹인다.
set -e
cd "$(dirname "$0")"
# ── 글꼴 커버리지 ─────────────────────────────────────────────
# 대사를 고치고 tools/gen_font.js 를 안 돌리면 글꼴에 없는 글자가 화면에
# **공백**으로 나간다. 빌드도 되고 테스트도 통과하니 사람 눈에만 걸린다.
# 실제로 두 번 났다 — 「리쿠도렛카」→「리쿠도⎵카」, 「나도 그랬다」→「나도 그⎵다」.
# 그래서 여기서 먼저 막는다. 이게 실패하면 나머지는 볼 것도 없다.
python3 check_glyph.py . || exit 1

cc -O1 -DSS2SP_RAM_POINTER -DSS2COMM_TEST -I. -o /tmp/ss2_test_flow test_flow.c ss2comm.c
/tmp/ss2_test_flow
