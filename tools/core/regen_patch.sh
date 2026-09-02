#!/bin/bash
# ss2sp.patch 재생성 — upstream 순정 3파일 vs 현재 build/ 3파일
set -e
V=~/ss2/repo/ss2-sp-core
COMMIT=a50d5ac288a81f2104ddf43195a4efdd15c72227
W=~/patchregen
if [ ! -d "$W/.git" ]; then
  rm -rf "$W"
  git clone --filter=blob:none https://github.com/libretro/beetle-ngp-libretro.git "$W"
fi
cd "$W"
git checkout -q "$COMMIT"
git reset --hard -q "$COMMIT"
for f in Makefile.common libretro.c libretro_core_options.h; do
  cp "$V/build/$f" "$W/$f"
done
git diff > /tmp/ss2sp.new.patch
# 검증: 순정으로 되돌리고 새 패치를 적용해 build/ 와 대조
git reset --hard -q "$COMMIT"
git apply /tmp/ss2sp.new.patch
ok=1
for f in Makefile.common libretro.c libretro_core_options.h; do
  cmp -s "$W/$f" "$V/build/$f" || { echo "불일치: $f"; ok=0; }
done
[ "$ok" = 1 ] && { cp /tmp/ss2sp.new.patch "$V/src/ss2sp.patch"; echo "ss2sp.patch 갱신 OK ($(wc -l < /tmp/ss2sp.new.patch) 줄)"; }