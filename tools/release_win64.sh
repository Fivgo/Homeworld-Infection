#!/usr/bin/env bash
# release_win64.sh - build the Windows x86-64 hwi-engine.exe (llvm-mingw cross build) and zip the release
# (docs/m3-windows-build.md).
#
# Usage: tools/release_win64.sh [--engine ID] [--llvm-mingw DIR] [--sdl2 DIR] [--out DIR]
#   --engine ID     the engine id in the zip's name and README (default: HEAD's short hash)
#   --llvm-mingw    default ~/Applications/llvm-mingw/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64
#   --sdl2          default ~/Applications/SDL2-mingw/SDL2-2.30.12/x86_64-w64-mingw32 (SDL2-devel-2.30.12-mingw.tar.gz)
#   --out           default <main checkout>/dist
# Build: build/win64 (the release flags of tools/build.sh --release: -O2, debug off, no sanitizer; movies off: no
# FFmpeg for Windows here). Output: <out>/Homeworld-Infection-win64-<yyyymmdd>-<engine>.zip with hwi-engine.exe,
# SDL2.dll, README-Windows.txt (tools/release/README-Windows.txt filled in), LICENSE-REULA.txt (src/COPYING.REULA)
# and LICENSE-SDL2.txt. No game data. Refuses an exe that imports a DLL other than SDL2.dll and Windows' own.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -d "$root/original/game" ]]; then main="$root"; else main="$(cd "$root/../../.." && pwd)"; fi
llvm="$HOME/Applications/llvm-mingw/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64"
sdl="$HOME/Applications/SDL2-mingw/SDL2-2.30.12/x86_64-w64-mingw32"
out="$main/dist"; engine=""
while [[ $# -gt 0 ]]; do
	case "$1" in
		--engine) engine=$2; shift ;;
		--llvm-mingw) llvm=$2; shift ;;
		--sdl2) sdl=$2; shift ;;
		--out) out=$2; shift ;;
		-h|--help) sed -n '2,13p' "$0"; exit 0 ;;
		*) echo "release_win64.sh: unknown argument $1" >&2; exit 2 ;;
	esac
	shift
done
[[ -x "$llvm/bin/x86_64-w64-mingw32-clang" ]] || { echo "release_win64.sh: no llvm-mingw at $llvm" >&2; exit 1; }
[[ -f "$sdl/bin/SDL2.dll" ]] || { echo "release_win64.sh: no SDL2 mingw tree at $sdl" >&2; exit 1; }
commit=$(git -C "$root" rev-parse --short=7 HEAD)
git -C "$root" diff --quiet HEAD -- src meson.build meson_options.txt tools/cross || commit="$commit+"
[[ -n "$engine" ]] || engine=$commit
day=$(date +%Y%m%d)

b="$root/build/win64"
mkdir -p "$root/build"
printf "[constants]\nllvm_mingw = '%s'\nsdl2_mingw = '%s'\n" "$llvm" "$sdl" > "$root/build/win64-paths.ini"
opts=(--cross-file "$root/build/win64-paths.ini" --cross-file "$root/tools/cross/x86_64-w64-mingw32.ini"
	-Doptimization=2 -Ddebug=false -Db_sanitize=none -Dmovies=false)
if [[ -f "$b/build.ninja" ]]; then meson setup --reconfigure "${opts[@]}" "$b" "$root"; else meson setup "${opts[@]}" "$b" "$root"; fi
meson compile -C "$b"

dlls=$("$llvm/bin/llvm-objdump" -p "$b/hwi-engine.exe" | sed -n 's/^ *DLL Name: //p')
echo "imports: $(echo $dlls)"
bad=$(echo "$dlls" | grep -v -i -E '^(SDL2|OPENGL32|KERNEL32|USER32|GDI32|SHELL32|ADVAPI32|WINMM|OLE32|api-ms-win-crt-[a-z0-9-]+)\.dll$' || true)
[[ -z "$bad" ]] || { echo "release_win64.sh: the exe imports a DLL this package does not carry: $bad" >&2; exit 1; }

stage=$(mktemp -d); trap 'rm -rf -- "$stage"' EXIT
cp "$b/hwi-engine.exe" "$sdl/bin/SDL2.dll" "$stage/"
crlf() { sed 's/\r$//; s/$/\r/' "$1" > "$2"; }
sed -e "s/@DATE@/$(date +%Y-%m-%d)/" -e "s/@ENGINE@/$engine/" -e "s/@COMMIT@/$commit/" \
	"$root/tools/release/README-Windows.txt" > "$stage/README.tmp"
crlf "$stage/README.tmp" "$stage/README-Windows.txt"; rm -f "$stage/README.tmp"
crlf "$root/src/COPYING.REULA" "$stage/LICENSE-REULA.txt"
crlf "$sdl/../LICENSE.txt" "$stage/LICENSE-SDL2.txt"
mkdir -p "$out"
zipf="$out/Homeworld-Infection-win64-$day-$engine.zip"
rm -f -- "$zipf"
(cd "$stage" && zip -q -X "$zipf" hwi-engine.exe SDL2.dll README-Windows.txt LICENSE-REULA.txt LICENSE-SDL2.txt)
unzip -l "$zipf"
echo "zip: $zipf"
echo "sha256: $(sha256sum "$zipf" | cut -d' ' -f1)"
