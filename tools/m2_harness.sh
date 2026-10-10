#!/usr/bin/env bash
# m2_harness.sh - M2-R2 test harness: run build/debug/hwi-engine headless against an UNMODIFIED Cataclysm install.
#
# Usage:
#   tools/m2_harness.sh --setup                 (re)create the harness data dir if missing (--force: rebuild it)
#   tools/m2_harness.sh <label> [options] [-- extra engine args]
# Options:
#   --mode sp|fe|statics
#                    sp (default): /debugScreenshots, which starts single player at once (GoK SDL/utility.c, no input);
#                    fe: plain start, idles in the front end until the timeout;
#                    statics: /catShipStaticsTest, loads the .shp of all 145 ship types once at start-up (one
#                    "shipstatic:" log line per file, docs/m2-r2-shipstatics.md) and quits
#   --timeout N      seconds (default 60)
#   --driver D       offscreen (default; falls back to dummy if the offscreen driver cannot set a video mode) | dummy
#   --data DIR       HW_Data dir to use instead of the harness data dir. ONLY for isolation runs of one item while
#                    other stage-1 items are still missing; the summary prints it, and such a run is never the gate.
#   --bin PATH       engine binary (default build/debug/hwi-engine)
#
# What the harness is (docs/m2-r2-identifiers.md "Test harness"): $HARNESS/data holds ONLY what a real GOG Cataclysm
# install has, under the install's own names: symlinks to original/game/{cataclysm.big,update.big,Music4Cat.wxd,
# SectSpeech.vce,BeastSpeech.vce,...} (files listed in original/game/MANIFEST.sizes; no executables, no CET files,
# none of docs/m2-r1-data-deltas.md's 15 workaround stages, no mesh down-conversion, no renamed copies). The
# writable install dirs (SavedGames, ScreenShots, MultiPlayer) are real, empty directories so nothing is ever written
# into original/game. Each run gets a fresh HOME (template: $HARNESS/home-template, TutorialNeeded 0) and its own
# /settingspath, so runs do not influence each other.
#
# Output: $HARNESS/runs/<label>/{run.log, FileLoads.log, asan.*, ubsan.*, home/, settings/, summary.txt} and one line
# in $HARNESS/runs/runs.txt. Prints the furthest milestone reached and the first stop.
# Rules (docs/CONSTRAINTS.md): never runs the GOG game, never sends input, no gamescope.
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -d "$root/original/game" ]]; then main="$root"; else main="$(cd "$root/../../.." && pwd)"; fi
GAME="$main/original/game"
HARNESS="${M2_HARNESS:-$main/original/scratch-m2/m2r2-harness}"
DATA="$HARNESS/data"

setup() {
	local force=${1:-0}
	if [[ -d "$DATA" && $force -eq 0 ]]; then echo "harness data exists: $DATA"; return 0; fi
	[[ -f "$GAME/MANIFEST.sizes" ]] || { echo "m2_harness: $GAME/MANIFEST.sizes not found" >&2; return 1; }
	rm -rf -- "$DATA"; mkdir -p "$DATA"
	# top-level data entries of the GOG manifest; executables, DLLs, GOG/nGlide/3Dfx files and readmes are not data
	local e
	for e in cataclysm.big update.big Music4Cat.wxd SectSpeech.vce BeastSpeech.vce default_keys.cfg devstats.dat \
		credits feman Movies; do
		[[ -e "$GAME/$e" ]] && ln -s "$GAME/$e" "$DATA/$e"
	done
	# writable dirs: real empty dirs with the install's layout (the engine may write saves/screenshots)
	mkdir -p "$DATA/SavedGames/MultiPlayer" "$DATA/SavedGames/RecordedGames" "$DATA/SavedGames/SinglePlayer" \
		"$DATA/SavedGames/Training" "$DATA/ScreenShots" "$DATA/MultiPlayer"
	mkdir -p "$HARNESS/home-template/.homeworld" "$HARNESS/runs"
	printf '[single player]\n    \nTutorialNeeded    0\n\n[New HWSDL Options]\n    \nPlayIntros    0\n' \
		> "$HARNESS/home-template/.homeworld/Homeworld.cfg"
	echo "harness data created: $DATA"; ls -la "$DATA"
}

if [[ "${1:-}" == "--setup" ]]; then
	[[ "${2:-}" == "--force" ]] && setup 1 || setup 0
	exit $?
fi

label=${1:?usage: m2_harness.sh <label> [--mode sp|fe] [--timeout N] [--driver D] [--data DIR] [-- engine args]}
shift
mode=sp; tmo=60; drv=offscreen; data="$DATA"; bin="$root/build/debug/hwi-engine"; extra=()
while [[ $# -gt 0 ]]; do
	case "$1" in
		--mode) mode=$2; shift ;;
		--timeout) tmo=$2; shift ;;
		--driver) drv=$2; shift ;;
		--data) data=$2; shift ;;
		--bin) bin=$2; shift ;;
		--) shift; extra=("$@"); break ;;
		*) echo "m2_harness: unknown option $1" >&2; exit 2 ;;
	esac
	shift
done
[[ -d "$DATA" ]] || setup 0 || exit 1
[[ -x "$bin" ]] || { echo "m2_harness: no engine binary at $bin (tools/build.sh --debug)" >&2; exit 1; }

out="$HARNESS/runs/$label"
rm -rf -- "$out"; mkdir -p "$out/settings"
cp -r "$HARNESS/home-template" "$out/home"

args=(/window /nodebugInt /logFileLoads /settingspath "$out/settings")
[[ "$mode" == sp ]] && args+=(/debugScreenshots)
[[ "$mode" == statics ]] && args+=(/catShipStaticsTest)
args+=("${extra[@]}")

run_once() {
	local d=$1
	export HOME="$out/home" HW_Data="$data" SDL_VIDEODRIVER="$d"
	export ASAN_OPTIONS="log_path=$out/asan:detect_leaks=1:verify_asan_link_order=0"
	export UBSAN_OPTIONS="print_stacktrace=1:log_path=$out/ubsan"
	local t0 t1
	t0=$(date +%s.%N)
	# cwd = settings dir: FileLoads.log and other engine-side files land in the run dir, never in the data dir
	(cd "$out/settings" && timeout -s TERM -k 5 "$tmo" stdbuf -oL -eL "$bin" "${args[@]}") > "$out/run.log" 2>&1
	rc=$?
	t1=$(date +%s.%N)
	wall=$(echo "$t1 - $t0" | bc)
}

run_once "$drv"
if [[ "$drv" == offscreen ]] && grep -q "Couldn't set video mode\|Couldn't initialize default rendering system" "$out/run.log"; then
	echo "offscreen driver could not set a video mode; retrying with dummy"
	mv "$out/run.log" "$out/run.offscreen.log"; drv=dummy; run_once dummy
fi
for f in "$out/settings"/FileLoads.log "$out/settings"/*/FileLoads.log "$data"/FileLoads.log; do
	[[ -f "$f" ]] && { mv "$f" "$out/FileLoads.log" 2>/dev/null || cp "$f" "$out/FileLoads.log"; break; }
done

# ---- milestones (furthest first) and first stop ----
L="$out/run.log"
milestone="none (no engine output)"
[[ -s "$L" ]] && milestone="process started"
grep -q "Homeworld.big TOC\|Cataclysm.big TOC\|bigOpen\|Opening .big\|TOC" "$L" && milestone="archives opened"
grep -q "Starting front end module" "$L" && milestone="front end module started"
grep -q "Startup engine trail module" "$L" && milestone="engine trail module started (meshes)"
grep -q "feScreenPush: pushed screen 1" "$L" && milestone="Main_game_screen"
grep -q "New single player game started" "$L" && milestone="single player started"
grep -qi "Mission01.mission\|LevelLoad\|levelLoad" "$out/FileLoads.log" "$L" 2>/dev/null && \
	grep -q "New single player game started" "$L" && milestone="level load (mission file read)"
grep -q "debugScreenshots univUpdateCounter" "$L" && milestone="universe running (univUpdateCounter > 0)"
grep -q "shipstatic: self-test done" "$L" && milestone="ship statics self-test done"

# first stop: the first fatal error / failed assertion / ASan report. UBSan runtime errors do not stop the engine; the
# first one is reported separately, except the misaligned-access reports on the packed sound-bank and front-end
# structs that every run prints (GoK 64-bit artefacts, docs/m2-r1-data-deltas.md 5.4)
stop=$(grep -h -E "Fatal error|Fatal Error|ERROR: AddressSanitizer|AddressSanitizer:DEADLYSIGNAL|Unable to open file|SUMMARY: AddressSanitizer|Assertion of" "$L" "$out"/asan.* 2>/dev/null | head -n 1)
ubsan1=$(grep -h "runtime error" "$L" "$out"/ubsan.* 2>/dev/null \
	| grep -v -E "misaligned address .* type '(struct PATCH|struct fescreen|struct featom|struct fibfileheader)'" | head -n 1)
if [[ -z "$stop" ]]; then
	if [[ $rc -eq 124 || $rc -eq 137 ]]; then stop="none: still running at the ${tmo}s timeout"; else stop="none found in the log (rc=$rc)"; fi
fi
asan_n=$(cat "$out"/asan.* 2>/dev/null | grep -c "ERROR: AddressSanitizer")
ubsan_kinds=$(cat "$L" "$out"/ubsan.* 2>/dev/null | grep "runtime error" | sed -E 's/^.*src\///; s/:[0-9]+:[0-9]+: runtime error:/: /; s/0x[0-9a-f]+/X/g' | sort | uniq -c | sort -rn | head -8)
nload=$(wc -l < "$out/FileLoads.log" 2>/dev/null || echo 0)

{
	echo "label=$label mode=$mode driver=$drv timeout=$tmo rc=$rc wall=${wall}s"
	echo "binary=$bin"
	echo "data=$data$([[ "$data" != "$DATA" ]] && echo '  (ISOLATION RUN: not the harness data dir)')"
	echo "args=${args[*]}"
	echo "milestone: $milestone"
	echo "first stop: $stop"
	echo "first non-benign UBSan report (non-fatal): ${ubsan1:-none}"
	echo "file loads: $nload; ASan errors: $asan_n"
	echo "ship statics: $(grep -c 'shipstatic: loaded' "$L") loaded, $(grep -c 'shipstatic: .* not found' "$L") not found (disabled), $(grep -c 'shipstatic: other' "$L") other .shp; $(grep -h 'shipstatic: self-test done' "$L" | sed 's/^.*shipstatic: //' | head -n 1)"
	echo "UBSan report kinds (count, site):"; echo "$ubsan_kinds"
	echo "log tail:"; grep -v '^    #\|^ *$' "$L" | tail -n 8
} | tee "$out/summary.txt"
echo "run=$label mode=$mode driver=$drv rc=$rc wall=${wall}s data=$data milestone=\"$milestone\" stop=\"$stop\"" >> "$HARNESS/runs/runs.txt"
