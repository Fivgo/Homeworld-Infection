#!/usr/bin/env bash
# build-windows.sh - build the Windows installer Homeworld-Infection-<version>-Windows-Setup.exe (docs/release-windows.md,
# docs/release-1.0.md). Runs on the GitHub Ubuntu runner (.github/workflows/build-windows.yml) and on a Linux machine.
#
# usage: tools/release/build-windows.sh [--deps DIR] [--out DIR] [--llvm-mingw DIR] [--sdl2 DIR] [--ffmpeg DIR]
#                                       [--innoextract DIR] [--no-installer]
#   --deps DIR         downloads, unpacked toolchains and the FFmpeg build (default <repo>/build/win-deps)
#   --out DIR          where the installer is written (default <repo>/dist)
#   --llvm-mingw DIR   an unpacked llvm-mingw to use instead of the pinned download
#   --sdl2 DIR         an SDL2 mingw tree (.../SDL2-2.30.12/x86_64-w64-mingw32) instead of the pinned download
#   --ffmpeg DIR       an FFmpeg prefix (bin/*.dll, lib/pkgconfig, share/licenses/ffmpeg) instead of building one
#   --innoextract DIR  a folder holding innoextract.exe and its licence files instead of the pinned download
#   --no-installer     stop after staging the files in build/win64-release/stage (no makensis needed)
# Every download is pinned by sha256: tools/release/windows/pins.env (llvm-mingw, SDL2, innoextract) and
# tools/release/ffmpeg-min.sh (the FFmpeg source, built here with --target mingw). Needs curl, tar, xz, unzip, make,
# python3, meson, ninja, pkg-config, binutils' strings and makensis (Ubuntu: apt-get install nsis).
# Build: build/win64-release, tools/build.sh --release's flags, -Dmovies=true, -Dbuild_version=<meson.build version>.
# Refuses an exe or DLL that imports a DLL the package does not carry and Windows does not have.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
here="$root/tools/release/windows"
# shellcheck source=windows/pins.env
. "$here/pins.env"
deps="$root/build/win-deps"; out="$root/dist"; llvm=""; sdl=""; ffmpeg=""; ie=""; installer=1
die() { echo "build-windows.sh: $*" >&2; exit 1; }
while [[ $# -gt 0 ]]; do
	case "$1" in
		--deps) deps=$2; shift ;;
		--out) out=$2; shift ;;
		--llvm-mingw) llvm=$2; shift ;;
		--sdl2) sdl=$2; shift ;;
		--ffmpeg) ffmpeg=$2; shift ;;
		--innoextract) ie=$2; shift ;;
		--no-installer) installer=0 ;;
		-h|--help) sed -n '2,19p' "$0"; exit 0 ;;
		*) die "unknown argument $1" ;;
	esac
	shift
done
mkdir -p "$deps" "$out"
deps=$(cd "$deps" && pwd); out=$(cd "$out" && pwd)
version=$(sed -n "s/^[[:space:]]*version:[[:space:]]*'\([0-9][0-9.]*\)'.*/\1/p" "$root/meson.build" | head -n1)
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || die "no x.y.z project version in meson.build"
commit=$(git -C "$root" rev-parse --short=7 HEAD 2>/dev/null || echo unknown)
git -C "$root" diff --quiet HEAD -- src meson.build meson_options.txt tools 2>/dev/null || commit="$commit+"
echo "Homeworld Infection $version for Windows, source $commit"

sha() { sha256sum "$1" | cut -d' ' -f1; }
# fetch URL SHA256 NAME: download into DEPS/dl/NAME once and check it against the pin
fetch() {
	local url=$1 want=$2 f="$deps/dl/$3" have
	mkdir -p "$deps/dl"
	if [[ -f "$f" && -n "$want" && "$(sha "$f")" == "$want" ]]; then echo "cached: $3 (sha256 $want)"; return; fi
	echo "download: $url"
	curl -fsSL --retry 3 -o "$f.part" "$url"
	have=$(sha "$f.part")
	echo "$3 sha256 $have"
	if [[ -z "$want" ]]; then
		echo "build-windows.sh: WARNING: $3 has no sha256 pin yet: set it in tools/release/windows/pins.env" >&2
	elif [[ "$have" != "$want" ]]; then
		rm -f -- "$f.part"; die "$3: sha256 $have, pinned $want"
	fi
	mv -f -- "$f.part" "$f"
}

# 1. third-party parts: the pinned downloads, unless a folder was given
if [[ -z "$llvm" ]]; then
	fetch "$LLVM_MINGW_URL" "$LLVM_MINGW_SHA256" "$LLVM_MINGW_NAME.tar.xz"
	llvm="$deps/$LLVM_MINGW_NAME"
	if [[ ! -x "$llvm/bin/x86_64-w64-mingw32-clang" ]]; then
		rm -rf -- "$llvm"; tar -xJf "$deps/dl/$LLVM_MINGW_NAME.tar.xz" -C "$deps"
	fi
fi
[[ -x "$llvm/bin/x86_64-w64-mingw32-clang" ]] || die "no llvm-mingw at $llvm"
llvm=$(cd "$llvm" && pwd)
if [[ -z "$sdl" ]]; then
	fetch "$SDL2_URL" "$SDL2_SHA256" "SDL2-devel-$SDL2_VERSION-mingw.tar.gz"
	sdl="$deps/SDL2-$SDL2_VERSION/x86_64-w64-mingw32"
	[[ -f "$sdl/bin/SDL2.dll" ]] || tar -xzf "$deps/dl/SDL2-devel-$SDL2_VERSION-mingw.tar.gz" -C "$deps"
fi
[[ -f "$sdl/bin/SDL2.dll" && -f "$sdl/../LICENSE.txt" ]] || die "no SDL2 mingw tree (bin/SDL2.dll, ../LICENSE.txt) at $sdl"
sdl=$(cd "$sdl" && pwd)
if [[ -z "$ie" ]]; then
	fetch "$INNOEXTRACT_URL" "$INNOEXTRACT_SHA256" "innoextract-$INNOEXTRACT_VERSION-windows.zip"
	ie="$deps/innoextract-$INNOEXTRACT_VERSION-windows"
	rm -rf -- "$ie"; mkdir -p "$ie"
	unzip -q "$deps/dl/innoextract-$INNOEXTRACT_VERSION-windows.zip" -d "$ie"
	echo "innoextract-$INNOEXTRACT_VERSION-windows.zip holds:"; (cd "$ie" && find . -type f -printf '  %p %s\n' | sort)
fi
iexe=$(find "$ie" -type f -iname innoextract.exe | head -n1)
[[ -n "$iexe" ]] || die "no innoextract.exe under $ie"
if [[ -z "$ffmpeg" ]]; then
	# the prefix is rebuilt when ffmpeg-min.sh or the compiler changes (the CI cache may hold an older one)
	ffmpeg="$deps/ffmpeg-mingw"
	stamp="$(sha "$root/tools/release/ffmpeg-min.sh") $(basename "$llvm")"
	if [[ "$(cat "$ffmpeg/.hwi-stamp" 2>/dev/null || true)" != "$stamp" ]]; then
		rm -rf -- "$ffmpeg"
		PATH="$llvm/bin:$PATH" "$root/tools/release/ffmpeg-min.sh" --prefix "$ffmpeg" --target mingw \
			--cross-prefix x86_64-w64-mingw32- --cc x86_64-w64-mingw32-clang
		echo "$stamp" > "$ffmpeg/.hwi-stamp"
	else
		echo "cached: FFmpeg in $ffmpeg"
	fi
fi
[[ -f "$ffmpeg/lib/pkgconfig/libavcodec.pc" ]] || die "no FFmpeg prefix (lib/pkgconfig/libavcodec.pc) at $ffmpeg"
ffmpeg=$(cd "$ffmpeg" && pwd)

# 2. the engine: tools/release_win64.sh's cross build with movies on; a third machine file adds FFmpeg's .pc files
b="$root/build/win64-release"
mkdir -p "$root/build"
printf "[constants]\nllvm_mingw = '%s'\nsdl2_mingw = '%s'\n" "$llvm" "$sdl" > "$root/build/win64-release-paths.ini"
printf "[properties]\npkg_config_libdir = ['%s', '%s']\n" "$sdl/lib/pkgconfig" "$ffmpeg/lib/pkgconfig" \
	> "$root/build/win64-release-ffmpeg.ini"
opts=(--cross-file "$root/build/win64-release-paths.ini" --cross-file "$root/tools/cross/x86_64-w64-mingw32.ini"
	--cross-file "$root/build/win64-release-ffmpeg.ini"
	-Doptimization=2 -Ddebug=false -Db_sanitize=none -Dmovies=true "-Dbuild_version=$version")
if [[ -f "$b/build.ninja" ]]; then meson setup --reconfigure "${opts[@]}" "$b" "$root"; else meson setup "${opts[@]}" "$b" "$root"; fi
meson compile -C "$b"
res=$("$llvm/bin/llvm-readobj" --coff-resources "$b/hwi-engine.exe")
grep -q 'Type: GROUP_ICON' <<<"$res" || die "hwi-engine.exe has no icon resource"
grep -q 'Type: VERSIONINFO' <<<"$res" || die "hwi-engine.exe has no VERSIONINFO resource"

# 3. the installed files
stage="$b/stage"
rm -rf -- "$stage"
mkdir -p "$stage/licenses/FFmpeg" "$stage/licenses/innoextract"
cp -- "$b/hwi-engine.exe" "$sdl/bin/SDL2.dll" "$iexe" "$stage/"
cp -- "$ffmpeg"/bin/*.dll "$stage/"
crlf() { sed 's/\r$//; s/$/\r/' "$1" > "$2"; }
sed -e "s/@VERSION@/$version/g" -e "s/@DATE@/$(date -u +%Y-%m-%d)/g" -e "s/@COMMIT@/$commit/g" \
	"$root/tools/release/README-Windows.txt" > "$b/README.tmp"
crlf "$b/README.tmp" "$stage/README.txt"
crlf "$root/src/COPYING.REULA" "$stage/licenses/REULA.txt"
crlf "$sdl/../LICENSE.txt" "$stage/licenses/SDL2.txt"
for f in "$ffmpeg/share/licenses/ffmpeg/"*; do crlf "$f" "$stage/licenses/FFmpeg/$(basename "$f" .txt).txt"; done
# LGPL: how the DLLs were configured (FFmpeg keeps its configure line in avutil, avutil_configuration())
cfg=$(strings -a "$ffmpeg"/bin/avutil-*.dll | grep -m1 -e '--enable-shared' || true)
printf '\nThe DLLs are unmodified FFmpeg, configured with:\n%s\n' "${cfg:-(not found)}" >> "$stage/licenses/FFmpeg/SOURCE.txt"
crlf "$stage/licenses/FFmpeg/SOURCE.txt" "$b/SOURCE.tmp"; mv -f -- "$b/SOURCE.tmp" "$stage/licenses/FFmpeg/SOURCE.txt"
while IFS= read -r -d '' f; do
	n=$(basename "$f"); [[ "$n" == *.txt ]] || n="$n.txt"
	crlf "$f" "$stage/licenses/innoextract/$n"
done < <(find "$(dirname "$iexe")" -maxdepth 1 -type f ! -iname '*.exe' -print0)
[[ -n "$(ls -A "$stage/licenses/innoextract")" ]] || die "no licence file beside innoextract.exe in $ie"

# 4. every exe and DLL may import only what the package carries and Windows' own DLLs
sysdlls='kernel32|user32|gdi32|shell32|advapi32|ole32|oleaut32|comdlg32|winmm|opengl32|bcrypt|imm32|version|setupapi'
sysdlls="$sysdlls|shlwapi|ws2_32|msvcrt|api-ms-win-crt-[a-z0-9-]+"
shipped=$(cd "$stage" && ls -- *.dll | tr '[:upper:]' '[:lower:]')
for pe in "$stage"/*.exe "$stage"/*.dll; do
	imps=$("$llvm/bin/llvm-objdump" -p "$pe" | sed -n 's/^ *DLL Name: //p')
	echo "$(basename "$pe") ($("$llvm/bin/llvm-objdump" -f "$pe" | sed -n 's/.*file format //p')) imports: $(echo $imps)"
	for d in $imps; do
		lc=$(tr '[:upper:]' '[:lower:]' <<<"$d")
		[[ "$lc" =~ ^($sysdlls)\.dll$ ]] && continue
		grep -qx -- "$lc" <<<"$shipped" && continue
		die "$(basename "$pe") imports $d, which neither the package nor Windows has"
	done
done
echo "staged:"; (cd "$stage" && find . -type f -printf '  %p %s\n' | sort)
[[ $installer -eq 1 ]] || { echo "build-windows.sh: staged in $stage (--no-installer)"; exit 0; }

# 5. the installer (tools/release/windows/installer.nsi); the file lists come from the stage
python3 - "$stage" "$b/files-install.nsh" "$b/files-uninstall.nsh" <<'PY'
import os, sys
stage, inst, uninst = sys.argv[1:]
files, dirs = [], []
for d, ds, fs in os.walk(stage):
    ds.sort()
    rel = os.path.relpath(d, stage)
    rel = "" if rel == "." else rel
    if rel:
        dirs.append(rel)
    files += [(rel, f) for f in sorted(fs)]
win = lambda p: p.replace("/", "\\")
with open(inst, "w") as o:
    cur = None
    for rel, f in files:
        if rel != cur:
            o.write('SetOutPath "$INSTDIR%s"\n' % ("\\" + win(rel) if rel else ""))
            cur = rel
        o.write('File "%s"\n' % os.path.join(stage, rel, f))
    o.write('SetOutPath "$INSTDIR"\n')
with open(uninst, "w") as o:
    for rel, f in files:
        o.write('Delete "$INSTDIR\\%s"\n' % win(os.path.join(rel, f)))
    for rel in sorted(dirs, key=lambda r: -r.count("/")):
        o.write('RMDir "$INSTDIR\\%s"\n' % win(rel))
PY
command -v makensis >/dev/null || die "makensis is missing (Ubuntu: apt-get install nsis)"
setup="$out/Homeworld-Infection-$version-Windows-Setup.exe"
rm -f -- "$setup"
makensis -V3 -INPUTCHARSET UTF8 "-DVERSION=$version" "-DSTAGE=$stage" \
	"-DICON=$root/tools/release/icon/homeworld-infection.ico" \
	"-DFILES_INSTALL=$b/files-install.nsh" "-DFILES_UNINSTALL=$b/files-uninstall.nsh" \
	"-DOUTFILE=$setup" "$here/installer.nsi"
echo "installer: $setup ($(stat -c %s "$setup") bytes)"
echo "sha256: $(sha "$setup")"
