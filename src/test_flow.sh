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

# ── 2회전 콜 회귀: 인트로에서 상대 미인식이어도 이어받아 발화 ──
cc -O1 -DSS2SP_RAM_POINTER -DSS2COMM_TEST -I. -o /tmp/ss2_round2 test_round2.c ss2comm.c
OUT=$(SS2COMM_DBGSEQ=1 /tmp/ss2_round2 2>&1 >/dev/null)
echo "$OUT" | grep -q "2회전!" || { echo "round2 콜 실패"; exit 1; }
echo "$OUT" | grep -qE "한 판!|완승!" || { echo "한 판!/완승! 훅 실패"; exit 1; }   # 무피격 승은 완승! 이 정답
echo "==== round2 call PASS ===="

# ── 플레이어 축 회귀: 뒤지는 판 응원(%m 치환·리터럴 유출 금지) ──
cc -O1 -DSS2SP_RAM_POINTER -DSS2COMM_TEST -I. -o /tmp/ss2_player test_player.c ss2comm.c
/tmp/ss2_player 2>/dev/null || { echo "player 축 실패"; exit 1; }
echo "==== player axis PASS ===="
