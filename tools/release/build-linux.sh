#!/usr/bin/env bash
# build-linux.sh - build the Linux release package Homeworld-Infection-<version>-Linux-x86_64.tar.gz
# (docs/release-1.0.md; how it works, what is pinned and how CI tests it: docs/release-linux.md).
#
# usage: tools/release/build-linux.sh [--deps DIR] [--out DIR] [--work DIR] [--deps-only] [--cache-keys]
#   --deps DIR     where the third-party builds live (SDL2, FFmpeg); reused when their key matches
#                  (default build/linux-deps)
#   --out DIR      where the package goes (default dist/)
#   --work DIR     scratch space: the engine build, the AppDir, the tools (default build/linux-package)
#   --deps-only    build SDL2 and FFmpeg into --deps, then stop (CI saves them in its cache)
#   --cache-keys   print the CI cache keys of the third-party builds (sdl2=..., ffmpeg=...) and exit
#
# Steps: SDL2 from source (X11 and Wayland, every window-system and audio library dlopened); the minimal FFmpeg
# (tools/release/ffmpeg-min.sh --target linux); the engine (meson, the release flags of tools/build.sh --release,
# -Dbuild_version=<version>, movies on), RUNPATH $ORIGIN/../lib; the official innoextract Linux binary; the AppDir
# (AppRun, desktop file, icons from tools/release/icon/make_icon.py --sizes, AppStream data, licences);
# appimagetool with the static type-2 runtime; the tar.gz holding the AppImage, README-Linux.txt and the licences.
# Meant for the ubuntu-22.04 GitHub runner (.github/workflows/build-linux.yml installs the build packages). It
# downloads third-party code and runs it: every download is pinned by version and sha256.
# Needs: gcc, meson >= 1.1, ninja, cmake, pkg-config, flex, bison, patchelf, curl, xz, python3 with Pillow, and the
# SDL2 build packages (see the workflow).
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
VERSION=$(sed -n "s/^[[:space:]]*version: '\([0-9][0-9.]*\)',\$/\1/p" "$root/meson.build" | head -1)
[[ -n "$VERSION" ]] || { echo "build-linux.sh: no project version in meson.build" >&2; exit 1; }

# ---- pins (the sha256 of every download; a mismatch stops the build) ------------------------------------------
SDL2_VERSION=2.30.12
SDL2_URL="https://github.com/libsdl-org/SDL/releases/download/release-$SDL2_VERSION/SDL2-$SDL2_VERSION.tar.gz"
SDL2_SHA256=""
SDL2_RECIPE=1             # bump when build_sdl2's options change (it is part of the cache key)
INNOEXTRACT_VERSION=1.9
INNOEXTRACT_URL="https://github.com/dscharrer/innoextract/releases/download/$INNOEXTRACT_VERSION/innoextract-$INNOEXTRACT_VERSION-linux.tar.xz"
INNOEXTRACT_SHA256=""
APPIMAGETOOL_VERSION=1.9.1
APPIMAGETOOL_URL="https://github.com/AppImage/appimagetool/releases/download/$APPIMAGETOOL_VERSION/appimagetool-x86_64.AppImage"
APPIMAGETOOL_SHA256=ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0
RUNTIME_VERSION=20251108  # github.com/AppImage/type2-runtime: static, needs no libfuse2 on the player's machine
RUNTIME_URL="https://github.com/AppImage/type2-runtime/releases/download/$RUNTIME_VERSION/runtime-x86_64"
RUNTIME_SHA256=2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d
FFMPEG_VERSION=$(sed -n 's/^FFMPEG_VERSION=//p' "$root/tools/release/ffmpeg-min.sh")

deps="$root/build/linux-deps"; out="$root/dist"; work="$root/build/linux-package"; deps_only=0; keys_only=0
while [[ $# -gt 0 ]]; do
	case "$1" in
		--deps) deps=$2; shift ;;
		--out) out=$2; shift ;;
		--work) work=$2; shift ;;
		--deps-only) deps_only=1 ;;
		--cache-keys) keys_only=1 ;;
		-h|--help) sed -n '2,24p' "$0"; exit 0 ;;
		*) echo "build-linux.sh: unknown argument $1" >&2; exit 2 ;;
	esac
	shift
done

distro=$(. /etc/os-release 2>/dev/null && echo "${ID:-linux}${VERSION_ID:-}")
sdl2_key="sdl2-$SDL2_VERSION-r$SDL2_RECIPE-$distro"
ffmpeg_key="ffmpeg-$FFMPEG_VERSION-$(sha256sum "$root/tools/release/ffmpeg-min.sh" | cut -c1-16)-$distro"
if [[ $keys_only -eq 1 ]]; then
	echo "sdl2=$sdl2_key"
	echo "ffmpeg=$ffmpeg_key"
	exit 0
fi

step() { printf '\n== %s\n' "$*"; }
die() { echo "build-linux.sh: $*" >&2; exit 1; }
for tool in gcc meson ninja cmake pkg-config flex bison patchelf curl xz python3 readelf; do
	command -v "$tool" >/dev/null 2>&1 || die "missing build tool: $tool"
done
[[ "$(uname -m)" == x86_64 ]] || die "this builds the x86-64 package on an x86-64 machine"
mkdir -p "$deps" "$out" "$work"
deps=$(cd "$deps" && pwd); out=$(cd "$out" && pwd); work=$(cd "$work" && pwd)

# fetch URL SHA256 FILE: download and check; an empty pin prints the sum to pin (CI log) and goes on
fetch() {
	local url=$1 want=$2 file=$3 got
	curl -fsSL --retry 3 --retry-delay 5 -o "$file" "$url"
	got=$(sha256sum "$file" | cut -d' ' -f1)
	echo "fetched $url"
	echo "  sha256 $got"
	if [[ -z "$want" ]]; then
		echo "  WARNING: not pinned yet: set its *_SHA256 in tools/release/build-linux.sh to $got"
		[[ -z "${GITHUB_ACTIONS:-}" ]] || echo "::warning title=unpinned download::$url sha256 $got"
		[[ -z "${HWI_REQUIRE_PINS:-}" ]] || die "HWI_REQUIRE_PINS is set and $url is not pinned"
	elif [[ "$got" != "$want" ]]; then
		die "checksum mismatch for $url: expected $want, got $got"
	fi
}
stamp_ok() { [[ -f "$1/.hwi-key" && "$(cat "$1/.hwi-key")" == "$2" ]]; }

# ---- SDL2 ------------------------------------------------------------------------------------------------------
# Every window-system, audio and helper library is dlopened (the *_SHARED options), so libSDL2 itself needs only
# glibc; on the player's machine it uses whatever of X11, Wayland (libdecor), PipeWire, PulseAudio and ALSA exists.
build_sdl2() {
	local prefix="$deps/sdl2" w="$work/sdl2-src"
	if stamp_ok "$prefix" "$sdl2_key"; then echo "SDL2 $SDL2_VERSION: cached ($sdl2_key)"; return; fi
	rm -rf -- "$prefix" "$w"; mkdir -p "$w"
	fetch "$SDL2_URL" "$SDL2_SHA256" "$w/sdl2.tar.gz"
	tar -xzf "$w/sdl2.tar.gz" -C "$w"
	cmake -S "$w/SDL2-$SDL2_VERSION" -B "$w/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib \
		-DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF -DSDL_TESTS=OFF -DSDL_RPATH=OFF -DSDL_INSTALL_TESTS=OFF \
		-DSDL_X11=ON -DSDL_X11_SHARED=ON \
		-DSDL_WAYLAND=ON -DSDL_WAYLAND_SHARED=ON -DSDL_WAYLAND_LIBDECOR=ON -DSDL_WAYLAND_LIBDECOR_SHARED=ON \
		-DSDL_PIPEWIRE=ON -DSDL_PIPEWIRE_SHARED=ON -DSDL_PULSEAUDIO=ON -DSDL_PULSEAUDIO_SHARED=ON \
		-DSDL_ALSA=ON -DSDL_ALSA_SHARED=ON -DSDL_LIBSAMPLERATE=ON -DSDL_LIBSAMPLERATE_SHARED=ON \
		-DSDL_JACK=OFF -DSDL_SNDIO=OFF -DSDL_NAS=OFF -DSDL_KMSDRM=OFF -DSDL_RPI=OFF -DSDL_VIVANTE=OFF \
		-DSDL_OPENGL=ON -DSDL_OPENGLES=ON -DSDL_VULKAN=ON
	cmake --build "$w/build"
	cmake --install "$w/build"
	mkdir -p "$prefix/share/licenses/SDL2"
	cp "$w/SDL2-$SDL2_VERSION/LICENSE.txt" "$prefix/share/licenses/SDL2/LICENSE.txt"
	printf 'SDL %s, built by tools/release/build-linux.sh (zlib licence).\nSource: %s\n' "$SDL2_VERSION" "$SDL2_URL" \
		> "$prefix/share/licenses/SDL2/SOURCE.txt"
	grep -rqE '^#define SDL_VIDEO_DRIVER_X11 ' "$prefix/include" || die "SDL2 was configured without X11"
	grep -rqE '^#define SDL_VIDEO_DRIVER_WAYLAND ' "$prefix/include" || die "SDL2 was configured without Wayland"
	echo "SDL2 dlopens:"; grep -rhE '^#define SDL_[A-Z0-9_]+_DYNAMIC[A-Z0-9_]* ' "$prefix/include" | sed 's/^#define /  /'
	echo "$sdl2_key" > "$prefix/.hwi-key"
	rm -rf -- "$w"
}

build_ffmpeg() {
	local prefix="$deps/ffmpeg"
	if stamp_ok "$prefix" "$ffmpeg_key"; then echo "FFmpeg $FFMPEG_VERSION: cached ($ffmpeg_key)"; return; fi
	rm -rf -- "$prefix"
	"$root/tools/release/ffmpeg-min.sh" --prefix "$prefix" --target linux
	echo "$ffmpeg_key" > "$prefix/.hwi-key"
}

step "1/6 SDL2 $SDL2_VERSION"
build_sdl2
step "2/6 FFmpeg $FFMPEG_VERSION (tools/release/ffmpeg-min.sh)"
build_ffmpeg
[[ $deps_only -eq 0 ]] || { echo "third-party builds ready in $deps"; exit 0; }

# ---- the engine --------------------------------------------------------------------------------------------------
step "3/6 hwi-engine $VERSION"
b="$work/engine"
export PKG_CONFIG_PATH="$deps/sdl2/lib/pkgconfig:$deps/ffmpeg/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
# the release flags of tools/build.sh --release (-O2, no debug, no sanitizer); -Dwerror=false: a warning that only
# this runner's compiler gives must not stop a release (the warnings are counted below)
opts=(-Doptimization=2 -Ddebug=false -Db_sanitize=none -Dbuild_version="$VERSION" -Dmovies=true -Dwerror=false)
if [[ -f "$b/build.ninja" ]]; then meson setup --reconfigure "${opts[@]}" "$b" "$root"; else meson setup "${opts[@]}" "$b" "$root"; fi
meson compile -C "$b" 2>&1 | tee "$work/engine-build.log"
echo "compiler warnings: $(grep -c 'warning:' "$work/engine-build.log" || true)"

# ---- innoextract and the AppImage tools -------------------------------------------------------------------------
step "4/6 innoextract $INNOEXTRACT_VERSION, appimagetool $APPIMAGETOOL_VERSION, runtime $RUNTIME_VERSION"
tools="$work/tools"
rm -rf -- "$tools"; mkdir -p "$tools/ie" "$tools/ait"
fetch "$INNOEXTRACT_URL" "$INNOEXTRACT_SHA256" "$tools/innoextract.tar.xz"
tar -xJf "$tools/innoextract.tar.xz" -C "$tools/ie"
echo "innoextract archive:"; (cd "$tools/ie" && find . -type f | sort | sed 's/^/  /')
ie_bin=$(find "$tools/ie" -type f -name innoextract -path '*amd64*' | head -1)
[[ -n "$ie_bin" ]] || ie_bin=$(find "$tools/ie" -type f -name innoextract -path '*x86_64*' | head -1)
[[ -n "$ie_bin" ]] || die "no x86-64 innoextract in $INNOEXTRACT_URL"
file "$ie_bin" 2>/dev/null || true
"$ie_bin" --version
fetch "$APPIMAGETOOL_URL" "$APPIMAGETOOL_SHA256" "$tools/ait/appimagetool.AppImage"
fetch "$RUNTIME_URL" "$RUNTIME_SHA256" "$tools/runtime-x86_64"
chmod +x "$tools/ait/appimagetool.AppImage"
(cd "$tools/ait" && ./appimagetool.AppImage --appimage-extract >/dev/null)   # no FUSE on the runner
appimagetool="$tools/ait/squashfs-root/AppRun"

# ---- the AppDir --------------------------------------------------------------------------------------------------
step "5/6 the AppDir"
ad="$work/AppDir"
rm -rf -- "$ad"
mkdir -p "$ad/usr/bin" "$ad/usr/lib" "$ad/usr/share/applications" "$ad/usr/share/metainfo"
doc="$ad/usr/share/doc/homeworld-infection"; mkdir -p "$doc"
install -m 755 "$b/hwi-engine" "$ad/usr/bin/hwi-engine"
install -m 755 "$ie_bin" "$ad/usr/bin/innoextract"

# bundle every library the engine (and, in turn, a bundled library) loads from the third-party builds; the rest
# (glibc, libGL, libX11) comes from the player's system
bundle_from() {
	local f=$1 lib path
	while read -r lib path; do
		case "$path" in
			"$deps"/*)
				if [[ ! -e "$ad/usr/lib/$lib" ]]; then
					cp -L "$path" "$ad/usr/lib/$lib"; chmod 644 "$ad/usr/lib/$lib"
					echo "  bundled $lib  <- $path"
					bundle_from "$ad/usr/lib/$lib"
				fi ;;
		esac
	done < <(LD_LIBRARY_PATH="$deps/sdl2/lib:$deps/ffmpeg/lib" ldd "$f" | sed -n 's/^[[:space:]]*\([^ ]*\) => \(\/[^ ]*\) .*/\1 \2/p')
}
bundle_from "$ad/usr/bin/hwi-engine"
[[ -e "$ad/usr/lib/libSDL2-2.0.so.0" ]] || die "libSDL2 was not bundled: the engine did not link the SDL2 built here"
patchelf --set-rpath '$ORIGIN/../lib' "$ad/usr/bin/hwi-engine"
for so in "$ad"/usr/lib/*.so*; do patchelf --set-rpath '$ORIGIN' "$so"; done
echo "hwi-engine: $(readelf -d "$ad/usr/bin/hwi-engine" | grep -E 'RUNPATH|RPATH' | sed 's/.*\[\(.*\)\]/RUNPATH \1/')"
echo "hwi-engine needs: $(readelf -d "$ad/usr/bin/hwi-engine" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p' | tr '\n' ' ')"
unresolved=$(env -u LD_LIBRARY_PATH ldd "$ad/usr/bin/hwi-engine" | grep 'not found' || true)
[[ -z "$unresolved" ]] || die "unresolved libraries: $unresolved"

sed "s/@VERSION@/$VERSION/g" "$root/tools/release/linux/AppRun" > "$ad/AppRun"; chmod 755 "$ad/AppRun"
sed "s/@VERSION@/$VERSION/g" "$root/tools/release/linux/homeworld-infection.desktop" > "$ad/homeworld-infection.desktop"
cp "$ad/homeworld-infection.desktop" "$ad/usr/share/applications/homeworld-infection.desktop"
commit_date=$(git -C "$root" log -1 --format=%cs 2>/dev/null || date -u +%F)
sed -e "s/@VERSION@/$VERSION/g" -e "s/@DATE@/$commit_date/g" "$root/tools/release/linux/homeworld-infection.appdata.xml" \
	> "$ad/usr/share/metainfo/homeworld-infection.appdata.xml"

# icons: the original art (tools/release/icon), drawn at the standard sizes
python3 "$root/tools/release/icon/make_icon.py" "$work/icon-1024.png" --sizes "$work/icons"
for n in 16 32 48 64 128 256 512; do
	mkdir -p "$ad/usr/share/icons/hicolor/${n}x$n/apps"
	cp "$work/icons/homeworld-infection-$n.png" "$ad/usr/share/icons/hicolor/${n}x$n/apps/homeworld-infection.png"
done
cp "$work/icons/homeworld-infection-256.png" "$ad/homeworld-infection.png"
ln -sf homeworld-infection.png "$ad/.DirIcon"

# licences
cp "$root/src/COPYING.REULA" "$doc/LICENSE-REULA.txt"
cp "$deps/sdl2/share/licenses/SDL2/LICENSE.txt" "$doc/LICENSE-SDL2.txt"
cp "$deps/sdl2/share/licenses/SDL2/SOURCE.txt" "$doc/SOURCE-SDL2.txt"
cp "$deps/ffmpeg/share/licenses/ffmpeg/LICENSE.md" "$doc/LICENSE-FFmpeg.md"
cp "$deps/ffmpeg/share/licenses/ffmpeg/COPYING.LGPLv2.1" "$doc/COPYING-FFmpeg-LGPLv2.1.txt"
cp "$deps/ffmpeg/share/licenses/ffmpeg/SOURCE.txt" "$doc/SOURCE-FFmpeg.txt"
{
	echo "innoextract $INNOEXTRACT_VERSION (the official Linux binary, $INNOEXTRACT_URL)."
	echo "Copyright (C) Daniel Scharrer. zlib/libpng licence; its licence and the licences of the libraries built into"
	echo "the binary, as it prints them with --license:"
	echo
	"$ie_bin" --license || echo "(this binary has no --license: see the licence files beside this one)"
} > "$doc/LICENSE-innoextract.txt"
while IFS= read -r f; do
	cp "$f" "$doc/$(basename "$f" .txt)-innoextract.txt"
done < <(find "$tools/ie" -maxdepth 3 -type f \( -iname 'LICENSE*' -o -iname 'COPYING*' \) | sort)
commit=$(git -C "$root" rev-parse --short=7 HEAD 2>/dev/null || echo unknown)
sed -e "s/@VERSION@/$VERSION/g" -e "s/@DATE@/$commit_date/g" -e "s/@COMMIT@/$commit/g" \
	"$root/tools/release/README-Linux.txt" > "$doc/README-Linux.txt"
echo "AppDir:"; (cd "$ad" && find . \( -type f -o -type l \) -printf '  %p %s\n' | sort)

# ---- the AppImage and the tar.gz ---------------------------------------------------------------------------------
step "6/6 the AppImage and the package"
img_name="Homeworld-Infection-$VERSION-x86_64.AppImage"
pkg_dir="Homeworld-Infection-$VERSION"
pkg="$out/Homeworld-Infection-$VERSION-Linux-x86_64.tar.gz"
stage="$work/stage"
rm -rf -- "$stage"; mkdir -p "$stage/$pkg_dir/licenses"
# no update information (-g is not given) and no AppStream check here (CI validates the metadata on its own)
env -u GITHUB_ACTIONS ARCH=x86_64 VERSION="$VERSION" "$appimagetool" --no-appstream --runtime-file "$tools/runtime-x86_64" \
	"$ad" "$stage/$pkg_dir/$img_name"
chmod 755 "$stage/$pkg_dir/$img_name"
cp "$doc/README-Linux.txt" "$stage/$pkg_dir/README-Linux.txt"
cp "$doc"/LICENSE* "$doc"/COPYING* "$doc"/SOURCE* "$stage/$pkg_dir/licenses/"
epoch=$(git -C "$root" log -1 --format=%ct 2>/dev/null || date +%s)
rm -f -- "$pkg"
tar --sort=name --owner=0 --group=0 --numeric-owner --mtime="@$epoch" -C "$stage" -cf - "$pkg_dir" | gzip -9n > "$pkg"
echo "package: $pkg"
tar -tvzf "$pkg"
echo "sha256 $(sha256sum "$pkg" | cut -d' ' -f1)  $(basename "$pkg")"
echo "sha256 $(sha256sum "$stage/$pkg_dir/$img_name" | cut -d' ' -f1)  $img_name"
