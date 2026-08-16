#!/usr/bin/env bash
# SS2 원버튼 코어 빌드 — beetle-ngp-libretro 포크
#
#   ./build.sh                    호스트용 (리눅스 x86_64)
#   ./build.sh linux-aarch64      ARM 리눅스 휴대기기
#   ./build.sh windows-x86_64     윈도우 DLL
#   ./build.sh android-arm64      안드로이드 (ANDROID_NDK_ROOT 필요)
#
# 필요한 것:
#   linux-aarch64   apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
#   windows-x86_64  apt install gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64
#   android-*       Android NDK (r21 이상). ANDROID_NDK_ROOT 환경변수로 지정
set -euo pipefail
TARGET="${1:-host}"
UPSTREAM="https://github.com/libretro/beetle-ngp-libretro.git"
COMMIT="a50d5ac288a81f2104ddf43195a4efdd15c72227"   # 패치 기준 커밋
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${WORK:-$HERE/build}"
OUT="$HERE/cores"; mkdir -p "$OUT"

if [ ! -d "$WORK" ]; then
  git clone "$UPSTREAM" "$WORK"
  git -C "$WORK" checkout "$COMMIT"
fi
git -C "$WORK" checkout -- . 2>/dev/null || true
git -C "$WORK" apply --3way "$HERE/src/ss2sp.patch"
cp "$HERE/src/ss2sp.c" "$HERE/src/ss2sp_moves.h" "$WORK/"

cd "$WORK"; make clean >/dev/null 2>&1 || true
case "$TARGET" in
  host)            make -j"$(nproc)";                              cp mednafen_ngp_libretro.so  "$OUT/mednafen_ngp_libretro.linux-x86_64.so" ;;
  linux-aarch64)   make -j"$(nproc)" platform=unix CC=aarch64-linux-gnu-gcc CXX=aarch64-linux-gnu-g++
                                                                   cp mednafen_ngp_libretro.so  "$OUT/mednafen_ngp_libretro.linux-aarch64.so" ;;
  windows-x86_64)  make -j"$(nproc)" platform=win CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++
                                                                   cp mednafen_ngp_libretro.dll "$OUT/mednafen_ngp_libretro.windows-x86_64.dll" ;;
  android-arm64)   : "${ANDROID_NDK_ROOT:?ANDROID_NDK_ROOT 를 NDK 경로로 지정하세요}"
                   "$ANDROID_NDK_ROOT/ndk-build" -C jni -j"$(nproc)" APP_ABI=arm64-v8a
                   cp jni/../libs/arm64-v8a/libretro.so "$OUT/mednafen_ngp_libretro.android-arm64-v8a.so" ;;
  android-arm32)   : "${ANDROID_NDK_ROOT:?ANDROID_NDK_ROOT 를 NDK 경로로 지정하세요}"
                   "$ANDROID_NDK_ROOT/ndk-build" -C jni -j"$(nproc)" APP_ABI=armeabi-v7a
                   cp jni/../libs/armeabi-v7a/libretro.so "$OUT/mednafen_ngp_libretro.android-armeabi-v7a.so" ;;
  *) echo "알 수 없는 타깃: $TARGET"; exit 1 ;;
esac
echo "완료 → $OUT"
