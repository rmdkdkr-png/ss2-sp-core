# -*- coding: utf-8 -*-
"""회귀 도구에 «대본 문지기»를 단다.

없는 대본으로 돌리면 모든 게임이 「덤프없음」이 되는데, 그건 「달라졌다」가 아니라
**「못 쟀다」**다. --expect 를 대본 자리에 넣었다가 그 꼴을 봤다.
이 도구가 이미 지키는 규율(잡음이 많으면 판정 포기)과 같은 종류의 문지기다.
"""
import io, os

p = os.path.expanduser("~/ss2/repo/ss2-sp-core/tools/kof/m1_reg.py")
s = io.open(p, encoding="utf-8").read()
if "대본 파일이 없다" in s:
    print("이미 있음"); raise SystemExit

old = "    script = sys.argv[3] if len(sys.argv) > 3 else os.path.join(HERE, 'm1_reg.txt')"
new = """    script = (sys.argv[3] if (len(sys.argv) > 3 and not sys.argv[3].startswith('-'))
              else os.path.join(HERE, 'm1_reg.txt'))
    # ★ 대본이 없으면 «여기서 멈춘다». 없는 대본으로 돌리면 모든 게임이
    #   「덤프없음」이 되는데, 그건 「달라졌다」가 아니라 «못 쟀다»다.
    #   (--expect 를 대본 자리에 넣었다가 그 꼴을 봤다.)
    if not os.path.exists(script):
        print('★ 대본 파일이 없다: %s' % script)
        print('  — 이건 회귀가 아니라 «못 쟀다»다. 경로를 고쳐라.')
        return 2"""
assert s.count(old) == 1
s = s.replace(old, new, 1)
io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("대본 문지기 추가")
