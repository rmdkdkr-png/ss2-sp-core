# -*- coding: utf-8 -*-
"""R 겸업을 뗀다 — 유저 지시 「a+b는 a+b의 역할이고 SP는 SP다」.

바뀌는 것:
    전     엔진 끔 → R = A+B  ·  엔진 켬 → R = SP     ← R 이 두 얼굴
    후     엔진 끔 → R 은 아무것도 안 함  ·  엔진 켬 → R = SP
           L = A+B 는 «양쪽 다, 언제나»

★ 잃는 것이 없다. NGPC 실기에는 A·B 두 버튼뿐이라 **R 을 A+B 로 접는 것 자체가 우리 규약**이었다.
  끔일 때도 Y=A · X=B · L=A+B 로 모든 입력을 낼 수 있다.
★ 「끔」을 순정 대조군으로 쓰려던 뜻은 L 이 그대로 이어받는다 —
  L 은 엔진과 무관하게 A+B 를 내고, 그것을 끔·켬 둘 다에서 실측했다(act 128).
⚠ KOF R-2 도 같은 겸업을 갖고 있다. **거기는 안 건드린다** — 유저에게 물어 정할 일이다.
"""
import io, os

# ── 엔진 ────────────────────────────────────────────────────────
p = os.path.expanduser("~/ss2/repo/ss2-sp-core/src/lbsp.c")
s = io.open(p, encoding="utf-8").read()

old = """   /* ★ 엔진이 꺼져 있으면 **순정 폴드 그대로**다 — R 까지 접는다.
      이것이 대조군의 정의다. 여기서 한 비트라도 다르면 대조군이 아니다. */
   if (!lb_engine_on || !lb_is_rom)
   {
      if (ret & (1u << RP_Y)) pad |= NGP_A;
      if (ret & (1u << RP_X)) pad |= NGP_B;
      if ((ret & (1u << RP_L)) || (ret & (1u << RP_R)))
         pad |= (uint8_t)(NGP_A | NGP_B);
      mac_step = -1;
      trig_prev = 0;
      return pad;
   }

   /* 엔진 켬 — R 은 트리거라 접지 않는다. L 은 그대로 A+B(사람의 동시입력 수단). */"""
new = """   /* ★ **R 은 겸업하지 않는다.** 엔진을 꺼도 R 은 A+B 로 안 접힌다.
      유저 지시: 「a+b는 a+b의 역할이고 SP는 SP다」.
      화면에 「SP」라 적힌 버튼이 때에 따라 A+B 를 내면 그건 거짓말이다.

      잃는 것은 없다 — NGPC 실기에는 A·B 두 버튼뿐이라 **R 을 A+B 로 접는 것 자체가
      우리가 만든 규약**이었고, 끔일 때도 Y=A · X=B · **L=A+B** 로 다 낼 수 있다.
      「끔이 순정 대조군」이라는 구실은 **L 이 그대로 이어받는다** — L 은 엔진과
      무관하게 A+B 를 내고 그것을 끔·켬 둘 다에서 실측했다(act 128, 위상 2종).

      ⚠ KOF R-2(`kofsp.c`)는 아직 겸업한다. 거기는 유저에게 물어 정할 일이라 안 건드렸다. */
   if (!lb_engine_on || !lb_is_rom)
   {
      if (ret & (1u << RP_Y)) pad |= NGP_A;
      if (ret & (1u << RP_X)) pad |= NGP_B;
      if (ret & (1u << RP_L)) pad |= (uint8_t)(NGP_A | NGP_B);
      mac_step = -1;
      trig_prev = 0;
      return pad;
   }

   /* 엔진 켬 — R 은 트리거. L 은 그대로 A+B(사람의 동시입력 수단). */"""
assert s.count(old) == 1
s = s.replace(old, new, 1)
io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("lbsp.c — R 겸업 뗌")

# ── 단위 시험의 계약 ────────────────────────────────────────────
p = os.path.expanduser("~/ss2/repo/ss2-sp-core/tools/lb/test_lbsp.c")
s = io.open(p, encoding="utf-8").read()
old = """/* 기준 폴드 — **M2 계약**. M1 때는 엔진 상태와 무관하게 순정과 같았는데,
   M2 에서 R 이 SP 트리거가 되며 «엔진 켤 때만» 달라진다. 시험을 지우지 않고 고쳤다.

   ★ 엔진 끔 = **순정 롬 폴드와 글자 그대로 동일**(R 도 접는다).
     이건 kofsp 와 일부러 다르다(kofsp 는 R 을 무조건 뺀다). 이래야 「엔진 끔」이
     **진짜 대조군**이 된다 — 대조군이 조금이라도 다르면 그건 대조군이 아니다.
   ★ 엔진 켬 = R 은 트리거라 안 접힌다. L 은 그대로 A+B. */
static unsigned char ref_fold(unsigned char pad, unsigned ret, int engine)
{
   if (ret & (1u << RP_Y)) pad |= NGP_A;
   if (ret & (1u << RP_X)) pad |= NGP_B;
   if (ret & (1u << RP_L)) pad |= (unsigned char)(NGP_A | NGP_B);
   if (!engine && (ret & (1u << RP_R)))
      pad |= (unsigned char)(NGP_A | NGP_B);
   return pad;
}"""
new = """/* 기준 폴드 — **M3 계약**. 계약이 두 번 바뀌었고 그때마다 시험을 «지우지 않고 고쳤다».
     M1: 엔진과 무관하게 순정 폴드와 동일.
     M2: 엔진을 켜면 R 이 트리거가 되어 안 접힘.
     M3: **R 은 아예 겸업하지 않는다** — 꺼도 A+B 로 안 접힌다.
         유저 지시 「a+b는 a+b의 역할이고 SP는 SP다」.

   ★ 지금 계약: **Y=A · X=B · L=A+B 는 언제나. R 은 엔진을 켤 때만 트리거이고,
     끄면 아무것도 안 한다.** 한 버튼이 두 얼굴을 갖지 않는다.
   ★ A+B 를 쓰는 길은 **L** 이다 — 엔진과 무관하게 한 프레임에 두 비트를 세운다.
     아래 ①-2 가 그것을 이름 붙여 지킨다. */
static unsigned char ref_fold(unsigned char pad, unsigned ret, int engine)
{
   (void)engine;
   if (ret & (1u << RP_Y)) pad |= NGP_A;
   if (ret & (1u << RP_X)) pad |= NGP_B;
   if (ret & (1u << RP_L)) pad |= (unsigned char)(NGP_A | NGP_B);
   return pad;               /* ⚠ R 은 어느 쪽에서도 안 접는다 */
}"""
assert s.count(old) == 1
s = s.replace(old, new, 1)

# 「끔이면 R 이 죽는다」를 이름 붙여 지킨다
old = """   lbsp_set_engine(0); lbsp_reset();

   /* ── ② 폴드 4,096조합 전수"""
new = """   /* ★ 엔진을 끄면 R 은 «아무것도 안 한다» — 겸업 금지. */
   lbsp_set_engine(0); lbsp_reset();
   ck(lbsp_frame(0, (unsigned short)(1u << RP_R)) == 0,
      "엔진 끔: R 은 아무것도 안 한다(A+B 로 안 접힌다)");
   ck(lbsp_frame(0, (unsigned short)((1u << RP_R) | (1u << RP_L))) == (NGP_A | NGP_B),
      "엔진 끔: R+L 을 같이 눌러도 A+B 는 L 것만");
   lbsp_reset();

   /* ── ② 폴드 4,096조합 전수"""
assert s.count(old) == 1
s = s.replace(old, new, 1)
io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("test_lbsp.c — M3 계약")

# ── 코어 옵션 설명 ──────────────────────────────────────────────
p = os.path.expanduser("~/ss2/repo/ss2-sp-core/build/libretro_core_options.h")
s = io.open(p, encoding="utf-8").read()
old = '"월화의 검사(The Last Blade) 전용. 켜면 R가 기술키가 됩니다. 끄면 R=A+B (순정과 같음). ※ 아직 만드는 중입니다 - 지금은 카에데의 236+베기 하나만 나갑니다."'
new = '"월화의 검사(The Last Blade) 전용. 켜면 R가 기술키가 됩니다. 끄면 R은 아무 일도 하지 않습니다. A+B는 켜든 끄든 항상 L입니다. ※ 아직 만드는 중입니다 - 지금은 카에데의 앞굴리기+베기 하나만 나갑니다."'
assert s.count(old) == 1
io.open(p, "w", encoding="utf-8", newline="\n").write(s.replace(old, new, 1))
print("코어 옵션 문구 갱신")

# ── lbsp.h 머리 주석 ────────────────────────────────────────────
p = os.path.expanduser("~/ss2/repo/ss2-sp-core/src/lbsp.h")
s = io.open(p, encoding="utf-8").read()
old = """   ★ 지금은 **M1(배관만)** 단계다. 게임 상수가 거의 다 미측정이고,
     lbsp_frame() 은 **순정 롬 폴드와 글자 그대로 똑같이** 접는다
     (Y=A · X=B · L·R=A+B). 즉 이 파일이 들어와도 월화 유저의 조작은
     한 비트도 안 바뀐다. 그것이 M1 의 통과 조건이다.

   ⚠ 다음 단계(M2)에서 **R 이 트리거가 되면서 폴드에서 빠진다.** 그때가
     출력이 처음 달라지는 순간이다 — 무회귀 시험의 기준선을 그 전에 떠 둬라. */"""
new = """   버튼 계약(M3):
     **Y=A · X=B · L=A+B 는 언제나. R 은 엔진을 켤 때만 SP 트리거이고, 끄면 아무것도 안 한다.**
     한 버튼이 두 얼굴을 갖지 않는다 — 유저 지시 「a+b는 a+b의 역할이고 SP는 SP다」.
     A+B 가 필요하면 **L** 이다(엔진과 무관하게 한 프레임에 두 비트).

   ⚠ NGPC 실기에는 A·B 두 버튼뿐이다. Y/X/L/R 로 접는 것 자체가 우리 규약이고,
     R 을 안 접어도 끔 상태에서 낼 수 없는 입력은 없다.
   ⚠ KOF R-2 는 아직 R 을 겸업한다(끄면 A+B). 맞추는 것은 유저에게 물어 정할 일이다.

   단계 표시는 `tools/lb/README.md` 를 정본으로 본다 — 주석에 적어 두면 늙는다. */"""
assert s.count(old) == 1
io.open(p, "w", encoding="utf-8", newline="\n").write(s.replace(old, new, 1))
print("lbsp.h — 계약 갱신")
