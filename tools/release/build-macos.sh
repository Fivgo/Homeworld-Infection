#!/usr/bin/env bash
# build-macos.sh - the macOS package of a release (docs/release-macos.md, docs/release-1.0.md):
# Homeworld-Infection-<version>-macOS.dmg holding "Homeworld Infection.app" (universal arm64 + x86_64, macOS 11+,
# ad-hoc signed), an Applications link, the player README and the licences. The app carries the engine, SDL2, the
# minimal LGPL FFmpeg (tools/release/ffmpeg-min.sh) and innoextract (beside the engine in Contents/MacOS).
#
# Meant for a macOS machine with Xcode's command line tools, cmake, meson, ninja and pkg-config: the GitHub macOS
# runners (.github/workflows/build-macos.yml). It downloads the pinned third-party sources (sha256-checked, below),
# builds them for both architectures and uses nothing from Homebrew or /usr/local.
#
# usage: tools/release/build-macos.sh [--version V] [--deps DIR] [--out DIR] [--jobs N] [--deps-only | --smoke-only]
#        tools/release/build-macos.sh --deps-key      print the third-party cache key and exit
#   --version V   the release version (default 1.0.0): -Dbuild_version, Info.plist, the dmg's name
#   --deps DIR    third-party builds (default build/macos-deps); each is rebuilt only when its recipe changes
#   --out DIR     engine builds, the app, the dmg and the logs (default build/macos-release)
#   --deps-only   stop after the third-party builds (CI caches them on their own)
#   --smoke-only  only the checks of an existing build (lipo, otool, codesign, a start without game data,
#                 innoextract --version); a normal run does them too
# Environment: MACOS_REQUIRE_PINS=1 fails on a source without a sha256 pin (otherwise a warning prints its sum).
#              MACOS_WERROR=false builds the engine without -Werror.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

# ---- pins (bump RECIPE to rebuild every dependency) -------------------------------------------------------------------
RECIPE=1
MACOS_MIN=11.0
ARCHS="arm64 x86_64"
SDL2_VERSION=2.30.12
SDL2_URL="https://github.com/libsdl-org/SDL/releases/download/release-$SDL2_VERSION/SDL2-$SDL2_VERSION.tar.gz"
SDL2_SHA256=ac356ea55e8b9dd0b2d1fa27da40ef7e238267ccf9324704850d5d47375b48ea   # Homebrew's sdl2 2.30.12 formula
FFMPEG_SHA256=3b624649725ecdc565c903ca6643d41f33bd49239922e45c9b1442c63dca4e38  # ffmpeg-6.1.2.tar.xz (ffmpeg-min.sh)
XZ_VERSION=5.8.4
XZ_URL="https://github.com/tukaani-project/xz/releases/download/v$XZ_VERSION/xz-$XZ_VERSION.tar.gz"
XZ_SHA256=0014c7886930454fe8bd4228665b51af55eeae560ea135c9c4cd33f55b2591d9     # GitHub's release asset digest
BOOST_VERSION=1.86.0   # innoextract's Boost.System component: the stub library is gone in 1.89
BOOST_URL="https://github.com/boostorg/boost/releases/download/boost-$BOOST_VERSION/boost-$BOOST_VERSION-b2-nodocs.tar.xz"
BOOST_SHA256=a4d99d032ab74c9c5e76eddcecc4489134282245fffa7e079c5804b92b45f51d  # Boost's own .tar.xz.txt
INNOEXTRACT_COMMIT=6e9e34ed0876014fdb46e684103ef8c3605e382e   # master 2025-02-06: Inno Setup 6.4, GOG Galaxy fixes
INNOEXTRACT_URL="https://github.com/dscharrer/innoextract/archive/$INNOEXTRACT_COMMIT.tar.gz"
INNOEXTRACT_SHA256=4d885b27e452dcb137a9d97736b19478447f172bcfed1aa98d80138e9a4c08ae   # the first CI download (2026-10-29)

version=1.0.0
deps="$root/build/macos-deps"
out="$root/build/macos-release"
jobs=$(sysctl -n hw.ncpu 2>/dev/null || getconf _NPROCESSORS_ONLN)
mode=all
while [[ $# -gt 0 ]]; do
	case "$1" in
		--version) version=$2; shift ;;
		--deps) deps=$2; shift ;;
		--out) out=$2; shift ;;
		--jobs) jobs=$2; shift ;;
		--deps-only) mode=deps ;;
		--smoke-only) mode=smoke ;;
		--deps-key) mode=key ;;
		-h|--help) sed -n '2,24p' "$0"; exit 0 ;;
		*) echo "build-macos.sh: unknown argument $1" >&2; exit 2 ;;
	esac
	shift
done

ffmpeg_script="$root/tools/release/ffmpeg-min.sh"
sha256() { shasum -a 256 "$1" | cut -d' ' -f1; }
recipe_all() {
	printf 'r%s min%s sdl2 %s %s ffmpeg %s %s xz %s %s boost %s %s innoextract %s %s' "$RECIPE" "$MACOS_MIN" \
		"$SDL2_VERSION" "$SDL2_SHA256" "$(sha256 "$ffmpeg_script")" "$FFMPEG_SHA256" "$XZ_VERSION" "$XZ_SHA256" \
		"$BOOST_VERSION" "$BOOST_SHA256" "$INNOEXTRACT_COMMIT" "$INNOEXTRACT_SHA256"
}
if [[ $mode == key ]]; then
	printf 'macos-deps-%s\n' "$( (recipe_all; sha256 "$0") | shasum -a 256 | cut -c1-16)"
	exit 0
fi

[[ "$(uname -s)" == Darwin ]] || { echo "build-macos.sh: needs macOS (the CI runner)" >&2; exit 1; }
mkdir -p "$deps" "$out"
deps=$(cd "$deps" && pwd); out=$(cd "$out" && pwd)
logdir="$out/logs"; mkdir -p "$logdir"
export MACOSX_DEPLOYMENT_TARGET=$MACOS_MIN
# nothing from Homebrew or /usr/local: no pkg-config search path but ours, cmake told to ignore those prefixes
export PKG_CONFIG_LIBDIR="$deps/none"
ignore_prefixes="/opt/homebrew;/usr/local;/opt/local"

log() { printf '\n== %s\n' "$*"; }
die() { echo "build-macos.sh: $*" >&2; exit 1; }
triple() { case "$1" in arm64) echo aarch64-apple-darwin ;; x86_64) echo x86_64-apple-darwin ;; esac; }

# fetch NAME URL SHA256 FILE: download, print the sum ("pin:" lines), refuse a mismatch
fetch() {
	local name=$1 url=$2 want=$3 file=$4 got
	curl -fsSL --retry 4 --retry-delay 5 -o "$file" "$url"
	got=$(sha256 "$file")
	echo "pin: $name sha256 $got ($url)"
	if [[ -z "$want" ]]; then
		[[ "${MACOS_REQUIRE_PINS:-0}" == 1 ]] && die "$name has no sha256 pin"
		echo "::warning::$name has no sha256 pin yet: $got"
	elif [[ "$got" != "$want" ]]; then
		die "$name: sha256 $got, expected $want"
	fi
}
# a dependency's build is kept while its recipe line is the same
built() { [[ -f "$1/.recipe" && "$(cat "$1/.recipe")" == "$2" ]]; }
mark() { printf '%s' "$2" > "$1/.recipe"; }
# run a step with its output in logs/NAME.log (the tail on failure)
step() {
	local name=$1; shift
	if ! "$@" > "$logdir/$name.log" 2>&1; then
		tail -n 80 "$logdir/$name.log"
		die "$name failed (log: $logdir/$name.log)"
	fi
}
check_arch() {   # FILE ARCH: a thin Mach-O or archive of that architecture
	local info; info=$(lipo -info "$1" 2>&1) || die "lipo -info $1: $info"
	case "$info" in *"is architecture: $2"*) ;; *) die "$1 is not $2-only: $info" ;; esac
}

# ---- SDL2 (one universal build) ------------------------------------------------------------------------------------------
build_sdl2() {
	local dst="$deps/sdl2" recipe="sdl2 $SDL2_VERSION $SDL2_SHA256 min$MACOS_MIN r$RECIPE" w
	if built "$dst" "$recipe"; then echo "SDL2 $SDL2_VERSION: cached"; return; fi
	log "SDL2 $SDL2_VERSION (arm64 + x86_64)"
	rm -rf "$dst"; w=$(mktemp -d)
	fetch SDL2 "$SDL2_URL" "$SDL2_SHA256" "$w/sdl2.tar.gz"
	tar -xzf "$w/sdl2.tar.gz" -C "$w"
	step sdl2-configure cmake -S "$w/SDL2-$SDL2_VERSION" -B "$w/build" -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=$MACOS_MIN -DCMAKE_INSTALL_PREFIX="$dst" \
		-DCMAKE_IGNORE_PREFIX_PATH="$ignore_prefixes" -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF \
		-DSDL_TESTS=OFF -DSDL_X11=OFF -DSDL_RPATH=OFF
	step sdl2-build cmake --build "$w/build" -j "$jobs"
	step sdl2-install cmake --install "$w/build"
	mkdir -p "$dst/share/licenses/SDL2"
	cp "$w/SDL2-$SDL2_VERSION/LICENSE.txt" "$dst/share/licenses/SDL2/"
	lipo -info "$dst/lib/libSDL2-2.0.0.dylib"
	mark "$dst" "$recipe"; rm -rf "$w"
}

# ---- FFmpeg (tools/release/ffmpeg-min.sh, one slice per architecture) -----------------------------------------------------
build_ffmpeg() {
	local arch=$1 dst="$deps/ffmpeg-$1" recipe
	recipe="ffmpeg-min $(sha256 "$ffmpeg_script") $FFMPEG_SHA256 $arch r$RECIPE"
	if built "$dst" "$recipe"; then echo "FFmpeg $arch: cached"; return; fi
	log "FFmpeg ($arch)"
	rm -rf "$dst"
	step "ffmpeg-$arch" env FFMPEG_SHA256="$FFMPEG_SHA256" "$ffmpeg_script" --prefix "$dst" --target "macos-$arch"
	grep "sha256" "$logdir/ffmpeg-$arch.log" | head -n 1 | sed 's/^/pin: /'
	check_arch "$dst/lib/libavcodec.dylib" "$arch"
	mark "$dst" "$recipe"
}

# ---- liblzma (xz), static, one slice per architecture ------------------------------------------------------------------------
build_xz() {
	local arch=$1 dst="$deps/xz-$1" recipe="xz $XZ_VERSION $XZ_SHA256 $1 min$MACOS_MIN r$RECIPE" w
	if built "$dst" "$recipe"; then echo "xz $arch: cached"; return; fi
	log "xz $XZ_VERSION ($arch)"
	rm -rf "$dst"; w=$(mktemp -d)
	fetch xz "$XZ_URL" "$XZ_SHA256" "$w/xz.tar.gz"
	tar -xzf "$w/xz.tar.gz" -C "$w"
	step "xz-$arch-configure" bash -c "cd '$w/xz-$XZ_VERSION' && ./configure --prefix='$dst' --host=$(triple "$arch") \
		--disable-shared --enable-static --disable-xz --disable-xzdec --disable-lzmadec --disable-lzmainfo \
		--disable-lzma-links --disable-scripts --disable-doc --disable-nls CC=clang \
		CFLAGS='-arch $arch -mmacosx-version-min=$MACOS_MIN -O2'"
	step "xz-$arch-build" make -C "$w/xz-$XZ_VERSION" -j "$jobs" install
	check_arch "$dst/lib/liblzma.a" "$arch"
	mkdir -p "$dst/share/licenses/xz"
	cp "$w/xz-$XZ_VERSION/COPYING" "$dst/share/licenses/xz/"
	[[ -f "$w/xz-$XZ_VERSION/COPYING.0BSD" ]] && cp "$w/xz-$XZ_VERSION/COPYING.0BSD" "$dst/share/licenses/xz/"
	mark "$dst" "$recipe"; rm -rf "$w"
}

# ---- Boost (what innoextract links), static, one slice per architecture ---------------------------------------------------
build_boost() {
	local arch w="" a b2arch dst recipe todo=""
	for arch in $ARCHS; do
		built "$deps/boost-$arch" "boost $BOOST_VERSION $BOOST_SHA256 $arch min$MACOS_MIN r$RECIPE" || todo="$todo $arch"
	done
	if [[ -z "$todo" ]]; then echo "Boost $BOOST_VERSION: cached"; return; fi
	log "Boost $BOOST_VERSION (${todo# })"
	w=$(mktemp -d)
	fetch Boost "$BOOST_URL" "$BOOST_SHA256" "$w/boost.tar.xz"
	tar -xJf "$w/boost.tar.xz" -C "$w"
	a=$(cd "$w" && ls -d boost-*/ | head -n 1); a="$w/${a%/}"
	step boost-bootstrap bash -c "cd '$a' && ./bootstrap.sh --with-libraries=iostreams,filesystem,date_time,system,program_options"
	for arch in $todo; do
		dst="$deps/boost-$arch"; recipe="boost $BOOST_VERSION $BOOST_SHA256 $arch min$MACOS_MIN r$RECIPE"
		case "$arch" in arm64) b2arch=arm ;; x86_64) b2arch=x86 ;; esac
		rm -rf "$dst"
		# iostreams uses the SDK's zlib and bzip2 (system libraries); no lzma/zstd filters
		step "boost-$arch" bash -c "cd '$a' && ./b2 -j$jobs -q --build-dir='$w/b2-$arch' --prefix='$dst' --layout=system \
			toolset=clang architecture=$b2arch address-model=64 link=static variant=release threading=multi \
			runtime-link=shared cflags='-arch $arch -mmacosx-version-min=$MACOS_MIN' \
			cxxflags='-arch $arch -mmacosx-version-min=$MACOS_MIN -std=c++17' \
			linkflags='-arch $arch -mmacosx-version-min=$MACOS_MIN' -sNO_LZMA=1 -sNO_ZSTD=1 install"
		check_arch "$dst/lib/libboost_iostreams.a" "$arch"
		mkdir -p "$dst/share/licenses/boost"
		cp "$a/LICENSE_1_0.txt" "$dst/share/licenses/boost/"
		mark "$dst" "$recipe"
	done
	rm -rf "$w"
}

# ---- innoextract (static Boost and liblzma; the SDK's libz, libbz2, libiconv, libc++), lipo'd ---------------------------
build_innoextract() {
	local dst="$deps/innoextract" recipe w arch slices=""
	recipe="innoextract $INNOEXTRACT_COMMIT $INNOEXTRACT_SHA256 boost $BOOST_VERSION xz $XZ_VERSION min$MACOS_MIN r$RECIPE"
	if built "$dst" "$recipe"; then echo "innoextract: cached"; return; fi
	log "innoextract ${INNOEXTRACT_COMMIT:0:12}"
	rm -rf "$dst"; w=$(mktemp -d)
	fetch innoextract "$INNOEXTRACT_URL" "$INNOEXTRACT_SHA256" "$w/ie.tar.gz"
	tar -xzf "$w/ie.tar.gz" -C "$w"
	for arch in $ARCHS; do
		step "innoextract-$arch-configure" env PKG_CONFIG_LIBDIR="$deps/xz-$arch/lib/pkgconfig" cmake \
			-S "$w/innoextract-$INNOEXTRACT_COMMIT" -B "$w/build-$arch" -DCMAKE_BUILD_TYPE=Release \
			-DCMAKE_OSX_ARCHITECTURES="$arch" -DCMAKE_OSX_DEPLOYMENT_TARGET=$MACOS_MIN \
			-DCMAKE_IGNORE_PREFIX_PATH="$ignore_prefixes" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
			-DUSE_STATIC_LIBS=ON -DBoost_USE_STATIC_LIBS=ON -DLZMA_USE_STATIC_LIBS=ON -DZLIB_USE_STATIC_LIBS=OFF \
			-DBZip2_USE_STATIC_LIBS=OFF -Diconv_USE_STATIC_LIBS=OFF \
			-DBUILD_TESTS=OFF -DUSE_LTO=OFF -DUSE_LD=default -DSET_WARNING_FLAGS=OFF \
			-DBOOST_ROOT="$deps/boost-$arch" -DBoost_NO_BOOST_CMAKE=ON -DBoost_NO_SYSTEM_PATHS=ON \
			-DLZMA_INCLUDE_DIR="$deps/xz-$arch/include" -DLZMA_LIBRARY="$deps/xz-$arch/lib/liblzma.a" \
			-DCMAKE_EXE_LINKER_FLAGS="-lz -lbz2"
		step "innoextract-$arch-build" cmake --build "$w/build-$arch" -j "$jobs"
		check_arch "$w/build-$arch/innoextract" "$arch"
		slices="$slices $w/build-$arch/innoextract"
	done
	mkdir -p "$dst/bin" "$dst/share/licenses/innoextract"
	# shellcheck disable=SC2086
	lipo -create $slices -output "$dst/bin/innoextract"
	cp "$w/innoextract-$INNOEXTRACT_COMMIT/LICENSE" "$dst/share/licenses/innoextract/"
	printf 'innoextract, built from commit %s (zlib licence).\nSource: https://github.com/dscharrer/innoextract/tree/%s\n' \
		"$INNOEXTRACT_COMMIT" "$INNOEXTRACT_COMMIT" > "$dst/share/licenses/innoextract/SOURCE.txt"
	mark "$dst" "$recipe"; rm -rf "$w"
}

build_deps() {
	local arch
	build_sdl2
	for arch in $ARCHS; do build_ffmpeg "$arch"; done
	for arch in $ARCHS; do build_xz "$arch"; done
	build_boost
	build_innoextract
	log "third-party builds ready in $deps (key $("$0" --deps-key))"
}

# ---- the engine: one meson build per architecture (cross files: kas2c is not built), lipo'd ---------------------------
build_engine() {
	local arch cf family bld werror=${MACOS_WERROR:-true}
	for arch in $ARCHS; do
		cf="$out/macos-$arch.ini"; bld="$out/engine-$arch"
		case "$arch" in arm64) family=aarch64 ;; x86_64) family=x86_64 ;; esac
		cat > "$cf" <<-EOF
		[binaries]
		c = ['clang', '-arch', '$arch']
		objc = ['clang', '-arch', '$arch']
		ar = 'ar'
		strip = 'strip'
		pkg-config = 'pkg-config'

		[built-in options]
		c_args = ['-mmacosx-version-min=$MACOS_MIN']
		c_link_args = ['-mmacosx-version-min=$MACOS_MIN', '-Wl,-headerpad_max_install_names']

		[properties]
		pkg_config_libdir = ['$deps/sdl2/lib/pkgconfig', '$deps/ffmpeg-$arch/lib/pkgconfig']

		[host_machine]
		system = 'darwin'
		cpu_family = '$family'
		cpu = '$arch'
		endian = 'little'
		EOF
		log "engine ($arch)"
		rm -rf "$bld"
		# tools/build.sh --release's flags (-O2, no sanitizers, HW_BUILD_FOR_DISTRIBUTION)
		step "engine-$arch-setup" meson setup "$bld" "$root" --cross-file "$cf" -Doptimization=2 -Ddebug=false \
			-Db_sanitize=none -Dbuild_version="$version" -Dwerror="$werror"
		step "engine-$arch-build" meson compile -C "$bld"
		echo "engine $arch: $(grep -c 'warning:' "$logdir/engine-$arch-build.log" || true) compiler warnings"
		grep 'warning:' "$logdir/engine-$arch-build.log" | sed 's|^.*src/|src/|' | sort -u | head -n 60 || true
		check_arch "$bld/hwi-engine" "$arch"
	done
}

# ---- the app bundle --------------------------------------------------------------------------------------------------------
app_name="Homeworld Infection"
app="$out/$app_name.app"
dmg="$out/Homeworld-Infection-$version-macOS.dmg"

# a thin Mach-O: dylib id @rpath/<name>; references to anything but the system @rpath/<name>; no absolute rpaths
fix_macho() {
	local f=$1 dep rp
	case "$f" in *.dylib) install_name_tool -id "@rpath/$(basename "$f")" "$f" ;; esac
	otool -L "$f" | tail -n +2 | awk '{print $1}' | while read -r dep; do
		case "$dep" in
			/usr/lib/*|/System/*|@rpath/*|@executable_path/*|@loader_path/*) ;;
			*) install_name_tool -change "$dep" "@rpath/$(basename "$dep")" "$f" ;;
		esac
	done
	otool -l "$f" | awk '/cmd LC_RPATH/{getline; getline; print $2}' | while read -r rp; do
		case "$rp" in @*) ;; *) install_name_tool -delete_rpath "$rp" "$f" ;; esac
	done
}

# universal DST from SRC (a path with @ARCH@ for per-architecture files, or one universal file): each slice is
# thinned, fixed (fix_macho, plus the rpath RPATH if given), checked and lipo'd again
universal() {
	local dst=$1 src=$2 rpath=${3:-} arch f s slices="" w
	w=$(mktemp -d)
	for arch in $ARCHS; do
		s=${src//@ARCH@/$arch}
		f="$w/$arch-$(basename "$dst")"
		if lipo -info "$s" | grep -q '^Non-fat'; then cp "$s" "$f"; else lipo -thin "$arch" "$s" -output "$f"; fi
		chmod u+w "$f"
		fix_macho "$f"
		if [[ -n "$rpath" ]]; then install_name_tool -add_rpath "$rpath" "$f"; fi
		check_arch "$f" "$arch"
		slices="$slices $f"
	done
	# shellcheck disable=SC2086
	lipo -create $slices -output "$dst"
	rm -rf "$w"
}

make_app() {
	local lib fw="$app/Contents/Frameworks" res="$app/Contents/Resources" commit lic="$out/Licenses"
	log "$app_name.app"
	rm -rf "$app" "$lic"
	mkdir -p "$app/Contents/MacOS" "$fw" "$res"
	universal "$app/Contents/MacOS/hwi-engine" "$out/engine-@ARCH@/hwi-engine" "@executable_path/../Frameworks"
	universal "$app/Contents/MacOS/innoextract" "$deps/innoextract/bin/innoextract"
	universal "$fw/libSDL2-2.0.0.dylib" "$deps/sdl2/lib/libSDL2-2.0.0.dylib"
	# FFmpeg: each library the engine links, under the versioned name it records (libavcodec.60.dylib, ...)
	for lib in $(cd "$deps/ffmpeg-arm64/lib" && ls | grep -E '^lib[a-z]+\.[0-9]+\.dylib$'); do
		universal "$fw/$lib" "$deps/ffmpeg-@ARCH@/lib/$lib"
	done
	chmod 755 "$app/Contents/MacOS/"*
	chmod 644 "$fw/"*
	cp "$root/tools/release/icon/homeworld-infection.icns" "$res/homeworld-infection.icns"
	cat > "$app/Contents/Info.plist" <<-EOF
	<?xml version="1.0" encoding="UTF-8"?>
	<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
	<plist version="1.0">
	<dict>
		<key>CFBundleDevelopmentRegion</key><string>en</string>
		<key>CFBundleDisplayName</key><string>$app_name</string>
		<key>CFBundleExecutable</key><string>hwi-engine</string>
		<key>CFBundleIconFile</key><string>homeworld-infection</string>
		<key>CFBundleIdentifier</key><string>io.github.fivgo.homeworld-infection</string>
		<key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
		<key>CFBundleName</key><string>$app_name</string>
		<key>CFBundlePackageType</key><string>APPL</string>
		<key>CFBundleShortVersionString</key><string>$version</string>
		<key>CFBundleVersion</key><string>$version</string>
		<key>LSApplicationCategoryType</key><string>public.app-category.strategy-games</string>
		<key>LSMinimumSystemVersion</key><string>$MACOS_MIN</string>
		<key>NSHighResolutionCapable</key><true/>
		<key>NSSupportsAutomaticGraphicsSwitching</key><true/>
		<key>NSHumanReadableCopyright</key><string>Fan recreation of the Homeworld: Cataclysm engine, based on Relic's Homeworld source (non-commercial, Relic EULA). Game data not included.</string>
	</dict>
	</plist>
	EOF
	plutil -lint "$app/Contents/Info.plist"
	printf 'APPL????' > "$app/Contents/PkgInfo"
	# licences: in the app (they travel with it) and beside it in the dmg
	mkdir -p "$lic/FFmpeg" "$lic/innoextract" "$lic/Boost" "$lic/xz"
	cp "$root/src/COPYING.REULA" "$lic/LICENSE-REULA.txt"
	cp "$deps/sdl2/share/licenses/SDL2/LICENSE.txt" "$lic/LICENSE-SDL2.txt"
	cp "$deps/ffmpeg-arm64/share/licenses/ffmpeg/"* "$lic/FFmpeg/"
	cp "$deps/innoextract/share/licenses/innoextract/"* "$lic/innoextract/"
	cp "$deps/boost-arm64/share/licenses/boost/"* "$lic/Boost/"
	cp "$deps/xz-arm64/share/licenses/xz/"* "$lic/xz/"
	cat > "$lic/README.txt" <<-EOF
	Licences of what Homeworld Infection $version for macOS carries:
	  LICENSE-REULA.txt  the engine (Relic Entertainment's Homeworld source code licence: non-commercial use only)
	  LICENSE-SDL2.txt   SDL2 $SDL2_VERSION (zlib licence), Contents/Frameworks/libSDL2-2.0.0.dylib
	  FFmpeg/            FFmpeg (LGPL-2.1-or-later), the libav*/libswscale dylibs in Contents/Frameworks; the source
	                     URL is in FFmpeg/SOURCE.txt, the build script is tools/release/ffmpeg-min.sh
	  innoextract/       innoextract (zlib licence), Contents/MacOS/innoextract, which links statically:
	  Boost/             Boost $BOOST_VERSION (Boost Software License 1.0)
	  xz/                liblzma from XZ Utils $XZ_VERSION (0BSD)
	innoextract also uses macOS's own libz, libbz2, libiconv and libc++.
	EOF
	cp -R "$lic" "$res/Licenses"
	commit=$(git -C "$root" rev-parse --short=12 HEAD 2>/dev/null || echo unknown)
	sed -e "s/@VERSION@/$version/g" -e "s/@COMMIT@/$commit/g" -e "s/@DATE@/$(date -u +%Y-%m-%d)/g" \
		"$root/tools/release/README-macOS.txt" > "$out/README.txt"
	# ad-hoc signature, inside out (no Developer ID: the player approves the app once, README)
	xattr -cr "$app"
	for lib in "$fw"/*.dylib "$app/Contents/MacOS/innoextract"; do
		codesign --force --sign - --timestamp=none "$lib"
	done
	codesign --force --sign - --timestamp=none --identifier io.github.fivgo.homeworld-infection "$app"
	codesign --verify --deep --strict --verbose=2 "$app"
}

make_dmg() {
	local stage="$out/dmg" try
	log "$(basename "$dmg")"
	rm -rf "$stage" "$dmg"; mkdir -p "$stage"
	cp -R "$app" "$stage/"
	ln -s /Applications "$stage/Applications"
	cp "$out/README.txt" "$stage/README.txt"
	cp -R "$out/Licenses" "$stage/Licenses"
	for try in 1 2 3 4 5; do
		if hdiutil create -volname "$app_name $version" -srcfolder "$stage" -fs HFS+ -format UDZO \
			-imagekey zlib-level=9 -ov "$dmg"; then
			break
		fi
		[[ $try == 5 ]] && die "hdiutil create failed"
		echo "hdiutil create failed (try $try), again in 10 s"; sleep 10
	done
	ls -la "$dmg"
	shasum -a 256 "$dmg"
}

# ---- smoke checks (CI never has game data) ---------------------------------------------------------------------------
# run CMD... with a time limit in seconds; exit 142 = killed by the alarm
with_timeout() { local t=$1; shift; perl -e 'alarm shift; exec @ARGV or die "exec: $!\n"' "$t" "$@"; }

smoke() {
	local f info bad=0 rc tmp eng="$app/Contents/MacOS/hwi-engine" arch out_log mnt
	[[ -d "$app" ]] || die "no app at $app"
	log "smoke: lipo -info"
	while IFS= read -r f; do
		info=$(lipo -info "$f")
		echo "$info"
		case "$info" in *"x86_64 arm64"*|*"arm64 x86_64"*) ;; *) echo "NOT UNIVERSAL: $f"; bad=1 ;; esac
	done < <(find "$app" -type f \( -perm -u+x -o -name '*.dylib' \) | sort)
	log "smoke: otool -L (only @rpath, /usr/lib and /System)"
	while IFS= read -r f; do
		for arch in $ARCHS; do
			echo "${f#"$out/"} ($arch):"
			otool -arch "$arch" -L "$f" | tail -n +2
			if otool -arch "$arch" -L "$f" | tail -n +2 | awk '{print $1}' | \
				grep -vqE '^(@rpath/|@executable_path/|@loader_path/|/usr/lib/|/System/Library/)'; then
				echo "BAD LIBRARY PATH in $f ($arch)"; bad=1
			fi
			if otool -arch "$arch" -L "$f" | grep -qE '/opt/homebrew|/usr/local'; then echo "HOMEBREW PATH in $f"; bad=1; fi
			otool -arch "$arch" -l "$f" | awk '/cmd LC_RPATH/{getline; getline; print "    rpath " $2}'
			otool -arch "$arch" -l "$f" | awk '/cmd LC_BUILD_VERSION/{for(i=0;i<5;i++){getline; if($1=="minos") print "    minos " $2}}'
		done
	done < <(find "$app" -type f \( -perm -u+x -o -name '*.dylib' \) | sort)
	log "smoke: codesign"
	codesign --verify --deep --strict --verbose=2 "$app" || bad=1
	codesign -dv "$app" 2>&1 | grep -E '^(Identifier|Format|Signature|TeamIdentifier|CodeDirectory)' || true
	spctl --assess --type execute -vv "$app" 2>&1 || echo "(spctl rejects an ad-hoc app: expected; the player approves it once)"
	plutil -p "$app/Contents/Info.plist"
	log "smoke: innoextract --version"
	with_timeout 30 "$app/Contents/MacOS/innoextract" --version || bad=1
	if arch -x86_64 /usr/bin/true 2>/dev/null; then
		with_timeout 60 arch -x86_64 "$app/Contents/MacOS/innoextract" --version || bad=1
	else
		echo "(no Rosetta: the x86_64 slice is not run)"
	fi
	log "smoke: the engine without game data (dummy video and audio, empty HOME, cwd an empty folder)"
	for arch in $ARCHS; do
		if [[ $arch == x86_64 && "$(uname -m)" == arm64 ]] && ! arch -x86_64 /usr/bin/true 2>/dev/null; then
			echo "($arch: no Rosetta, not run)"; continue
		fi
		tmp=$(mktemp -d); mkdir "$tmp/home"
		out_log="$logdir/smoke-engine-$arch.log"
		set +e
		(cd "$tmp" && env -i PATH=/usr/bin:/bin HOME="$tmp/home" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
			perl -e 'alarm shift; exec @ARGV or die "exec: $!\n"' 60 arch "-$arch" "$eng") > "$out_log" 2>&1
		rc=$?
		set -e
		echo "$arch: exit status $rc; log tail:"; tail -n 8 "$out_log" | sed 's/^/  /'
		if [[ $rc == 142 ]]; then echo "HANG: $arch did not exit within 60 s"; bad=1
		elif [[ $rc -ge 128 ]]; then echo "CRASH: $arch died of signal $((rc - 128))"; bad=1; fi
		find "$tmp/home" -mindepth 1 | sed "s|$tmp/home|  HOME|" | head -n 10
		rm -rf "$tmp"
	done
	if [[ -f "$dmg" ]]; then
		log "smoke: the dmg"
		mnt=$(mktemp -d)
		hdiutil attach -nobrowse -readonly -mountpoint "$mnt" "$dmg" > /dev/null
		ls -la "$mnt"
		codesign --verify --deep --strict "$mnt/$app_name.app" && echo "app on the dmg: signature valid" || bad=1
		hdiutil detach "$mnt" > /dev/null || hdiutil detach -force "$mnt" > /dev/null
	fi
	[[ $bad == 0 ]] || die "smoke checks failed"
	log "smoke: all checks passed"
}

case "$mode" in
	deps) build_deps ;;
	smoke) smoke ;;
	all)
		build_deps
		build_engine
		make_app
		make_dmg
		smoke
		;;
esac
