#!/usr/bin/env bash
# ffmpeg-min.sh - build the minimal LGPL FFmpeg the release packages carry (docs/release-1.0.md contract 5): shared
# libraries with only what the movie player (src/SDL/avi.c) uses: the bink demuxer, the bink video decoder (and the
# binkaudio decoders, so stream probing does not complain), the file protocol, swscale. No programs, no network,
# no external libraries, no GPL parts. Meant for the CI runners (it downloads the FFmpeg source).
#
# usage: tools/release/ffmpeg-min.sh --prefix DIR [--target linux|mingw|macos-arm64|macos-x86_64] [--cross-prefix P] [--cc CC]
#   linux          native build (the Linux runner)
#   mingw          Windows x86-64 cross build; --cross-prefix x86_64-w64-mingw32- (llvm-mingw: also --cc x86_64-w64-mingw32-clang)
#   macos-<arch>   one macOS slice (min macOS 11.0); the caller lipo's two slices into universal dylibs
# Output: DIR/{include,lib,lib/pkgconfig} (+ DIR/bin/*.dll on Windows). The source tarball is checked against
# FFMPEG_SHA256 below; LICENSE.md and COPYING.LGPLv2.1 are copied to DIR/share/licenses/ffmpeg/.
set -euo pipefail
FFMPEG_VERSION=6.1.2
FFMPEG_URL="https://ffmpeg.org/releases/ffmpeg-$FFMPEG_VERSION.tar.xz"
FFMPEG_SHA256="${FFMPEG_SHA256:-3b624649725ecdc565c903ca6643d41f33bd49239922e45c9b1442c63dca4e38}"   # sha256 of ffmpeg-6.1.2.tar.xz (Homebrew ffmpeg@6 formula; the CI logs print the same)
prefix=""; target=linux; cross=""; cc=""
while [[ $# -gt 0 ]]; do
	case "$1" in
		--prefix) prefix=$2; shift ;;
		--target) target=$2; shift ;;
		--cross-prefix) cross=$2; shift ;;
		--cc) cc=$2; shift ;;
		-h|--help) sed -n '2,15p' "$0"; exit 0 ;;
		*) echo "ffmpeg-min.sh: unknown argument $1" >&2; exit 2 ;;
	esac
	shift
done
[[ -n "$prefix" ]] || { echo "ffmpeg-min.sh: --prefix is required" >&2; exit 2; }
mkdir -p "$prefix"; prefix=$(cd "$prefix" && pwd)
work=$(mktemp -d); trap 'rm -rf -- "$work"' EXIT
curl -fsSL --retry 3 -o "$work/ffmpeg.tar.xz" "$FFMPEG_URL"
sum=$(sha256sum "$work/ffmpeg.tar.xz" 2>/dev/null | cut -d' ' -f1 || shasum -a 256 "$work/ffmpeg.tar.xz" | cut -d' ' -f1)
echo "ffmpeg-$FFMPEG_VERSION.tar.xz sha256 $sum"
if [[ -n "$FFMPEG_SHA256" && "$sum" != "$FFMPEG_SHA256" ]]; then echo "ffmpeg-min.sh: checksum mismatch" >&2; exit 1; fi
tar -xJf "$work/ffmpeg.tar.xz" -C "$work"
src="$work/ffmpeg-$FFMPEG_VERSION"

opts=(--prefix="$prefix" --enable-shared --disable-static --enable-pic
	--disable-everything --disable-autodetect --disable-programs --disable-doc --disable-network
	--disable-avdevice --disable-avfilter --disable-swresample --disable-postproc --disable-x86asm
	--enable-avcodec --enable-avformat --enable-avutil --enable-swscale
	--enable-decoder=bink,binkaudio_dct,binkaudio_rdft --enable-demuxer=bink --enable-protocol=file)
case "$target" in
	linux) ;;
	mingw) opts+=(--enable-cross-compile --target-os=mingw32 --arch=x86_64 --cross-prefix="${cross:-x86_64-w64-mingw32-}") ;;
	macos-arm64|macos-x86_64)
		arch=${target#macos-}
		opts+=(--enable-cross-compile --target-os=darwin --arch="$arch"
			--extra-cflags="-arch $arch -mmacosx-version-min=11.0" --extra-ldflags="-arch $arch -mmacosx-version-min=11.0") ;;
	*) echo "ffmpeg-min.sh: unknown target $target" >&2; exit 2 ;;
esac
[[ -n "$cc" ]] && opts+=(--cc="$cc")
(cd "$src" && ./configure "${opts[@]}" && make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu)" && make install)
mkdir -p "$prefix/share/licenses/ffmpeg"
cp "$src/LICENSE.md" "$src/COPYING.LGPLv2.1" "$prefix/share/licenses/ffmpeg/"
printf 'FFmpeg %s, built by tools/release/ffmpeg-min.sh (LGPL-2.1-or-later).\nSource: %s\n' "$FFMPEG_VERSION" "$FFMPEG_URL" > "$prefix/share/licenses/ffmpeg/SOURCE.txt"
echo "ffmpeg-min.sh: installed into $prefix"
