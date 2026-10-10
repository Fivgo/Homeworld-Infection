#!/usr/bin/env bash
# Build hwi-engine (the Homeworld 1 engine tree under src/, REULA-licensed).
#
# Usage: tools/build.sh [--debug|--release] [--clean] [-- extra meson setup args]
#   --debug    (default) debug build with ASan/UBSan, HW_BUILD_FOR_DEBUGGING -> build/debug/
#   --release  optimised build (-O2, the build oracle runs use), no sanitizers, HW_BUILD_FOR_DISTRIBUTION -> build/release/
#   --clean    delete that build directory first
# Output: build/<type>/hwi-engine. build/ is gitignored.
# Prerequisites (Ubuntu/Pop!_OS 24.04 package names): see docs/m2-r1-bootstrap.md.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
type=debug
clean=0
extra=()
while [[ $# -gt 0 ]]; do
	case "$1" in
		--debug) type=debug ;;
		--release) type=release ;;
		--clean) clean=1 ;;
		-h|--help) sed -n '2,9p' "$0"; exit 0 ;;
		--) shift; extra=("$@"); break ;;
		*) echo "build.sh: unknown argument: $1" >&2; exit 2 ;;
	esac
	shift
done

builddir="$root/build/$type"
if [[ $clean -eq 1 && -d "$builddir" ]]; then
	rm -rf -- "$builddir"
fi

case "$type" in
	debug)   setup_args=(--buildtype=debug -Db_sanitize=address,undefined) ;;
	# release: -O2 pinned for oracle runs (docs/m2-r2-oracle.md "flags"). Not --buildtype=release, which forces -O3 over
	# -Doptimization; meson then labels the buildtype "debug" or "custom", but debug=false and optimization=2 are what count.
	release) setup_args=(-Doptimization=2 -Ddebug=false -Db_sanitize=none) ;;
esac

if [[ -f "$builddir/build.ninja" ]]; then
	meson setup --reconfigure "${setup_args[@]}" "${extra[@]}" "$builddir" "$root"
else
	meson setup "${setup_args[@]}" "${extra[@]}" "$builddir" "$root"
fi
meson compile -C "$builddir"
echo "built: $builddir/hwi-engine"
