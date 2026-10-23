#!/usr/bin/env bash
# Build raylib for Android and install its headers + per-ABI static archive
# into third_party/raylib-install-android/<abi>/{include,lib} (the paths the
# Makefile's `android` targets expect). raylib's own PLATFORM_ANDROID target
# drives the NDK's clang directly (no Gradle). Trimmed as the other platforms
# (raylib_flags.sh), with the family's EGL configuration patch
# (raylib-android-eglconfig.patch: fall back to 16-bit depth or 5/6/5 colour
# rather than fail with EGL_BAD_CONFIG, as the emulator's software EGL needs).
#
#   scripts/build_raylib_android.sh --ndk <ndk root> [--api 24] [--arch arm64]...
#
# The NDK comes from the command line, not the environment, as the Makefile's.
set -euo pipefail

RAYLIB_TAG="6.0"
RAYLIB_SRC_DIR="third_party/raylib"
INSTALL_DIR="third_party/raylib-install-android"
ANDROID_NDK=""
ANDROID_API="24"
ANDROID_ARCHES=""

usage() {
    echo "usage: $0 --ndk <dir> [--api <level>] [--arch <name>]..." >&2
    exit 2
}
while [ $# -gt 0 ]; do
    case "$1" in
        --ndk)  [ $# -ge 2 ] || usage; ANDROID_NDK="$2"; shift 2 ;;
        --api)  [ $# -ge 2 ] || usage; ANDROID_API="$2"; shift 2 ;;
        --arch) [ $# -ge 2 ] || usage; ANDROID_ARCHES="$ANDROID_ARCHES $2"; shift 2 ;;
        *) echo "[build_raylib_android] unknown argument: $1" >&2; usage ;;
    esac
done
[ -n "$ANDROID_NDK" ] || usage
[ -d "$ANDROID_NDK" ] || { echo "[build_raylib_android] no NDK at $ANDROID_NDK" >&2; exit 2; }
[ -n "$ANDROID_ARCHES" ] || ANDROID_ARCHES="arm64"

if [ ! -d "$RAYLIB_SRC_DIR" ]; then
    git clone --depth 1 --branch "$RAYLIB_TAG" \
        https://github.com/raysan5/raylib "$RAYLIB_SRC_DIR"
fi

abi_for_arch() {
    case "$1" in
        arm64)  echo "arm64-v8a" ;;
        arm)    echo "armeabi-v7a" ;;
        x86_64) echo "x86_64" ;;
        x86)    echo "x86" ;;
        *) echo "unknown-arch:$1" >&2; return 1 ;;
    esac
}

PATCH="$(cd "$(dirname "$0")" && pwd)/raylib-android-eglconfig.patch"
if git -C "$RAYLIB_SRC_DIR" apply --reverse --check "$PATCH" 2>/dev/null; then
    echo "[build_raylib_android] EGL config patch already applied"
else
    git -C "$RAYLIB_SRC_DIR" apply "$PATCH"
    echo "[build_raylib_android] applied EGL config patch"
fi

. "$(dirname "$0")/raylib_flags.sh"
for arch in $ANDROID_ARCHES; do
    abi="$(abi_for_arch "$arch")"
    echo "[build_raylib_android] building raylib $RAYLIB_TAG for $arch ($abi), API $ANDROID_API"
    make -C "$RAYLIB_SRC_DIR/src" clean
    make -C "$RAYLIB_SRC_DIR/src" \
        PLATFORM=PLATFORM_ANDROID \
        RAYLIB_LIBTYPE=STATIC \
        ANDROID_NDK="$ANDROID_NDK" \
        ANDROID_ARCH="$arch" \
        ANDROID_API_VERSION="$ANDROID_API" \
        CUSTOM_CFLAGS="-w $RAYLIB_TRIM_CFLAGS" \
        -j"$(nproc)"
    dest="$INSTALL_DIR/$abi"
    mkdir -p "$dest/include" "$dest/lib"
    cp "$RAYLIB_SRC_DIR/src/raylib.h" \
       "$RAYLIB_SRC_DIR/src/raymath.h" \
       "$RAYLIB_SRC_DIR/src/rlgl.h" "$dest/include/"
    cp "$RAYLIB_SRC_DIR/src/libraylib.a" "$dest/lib/"
    echo "[build_raylib_android] installed -> $dest"
done
