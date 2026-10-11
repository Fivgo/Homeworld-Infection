#!/usr/bin/env bash
# m2_harness.sh - M2-R2 test harness: run build/debug/hwi-engine headless against an UNMODIFIED Cataclysm install.
#
# Usage:
#   tools/m2_harness.sh --setup                 (re)create the harness data dir if missing (--force: rebuild it)
#   tools/m2_harness.sh <label> [options] [-- extra engine args]
# Options:
#   --mode sp|fe|statics|meshes|textures|oracle|etg|mission
#                    sp (default): /debugScreenshots, which starts single player at once (GoK SDL/utility.c, no input);
#                    fe: plain start, idles in the front end until the timeout;
#                    statics: /catShipStaticsTest, loads the .shp of all 145 ship types once at start-up (one
#                    "shipstatic:" log line per file, docs/m2-r2-shipstatics.md) and quits;
#                    meshes: /catMeshLoadTest <run>/meshes.list, loads every .geo/.peo/.mad of the install (the
#                    cataclysm.big TOC, or the files of an /ignoreBigfiles --data tree) once at start-up, one
#                    "meshtest:" log line per file (docs/m2-r2-mesh.md), and quits;
#                    textures: /catTexturesTest, loads textures.ll and registers + loads every texture it lists in
#                    batches (docs/m2-r2-startup.md), "texturetest:" summary lines, and quits;
#                    oracle: /oracleSelfTest <ticks>, the checksum oracle's scripted lockstep start (2 players, no
#                    ships; docs/m2-r2-oracle.md), writes <run>/run.hwo and quits;
#                    etg: /catEtgParseTest <run>/etg.list (M2-R3, docs/m2-r3-etg-particles.md): starts the effects
#                    module (the exe's 8 etg\*.script files and every effect they load), then loads every other
#                    etg\*.script and etg\*.ebg of the cataclysm.big TOC, "etgtest:" log lines; with --etg-ticks N > 0
#                    also /catEtgEffectTest N (a sample of the loaded effects, created with the parameters the game
#                    passes, run for N universe ticks); quits
#                    mission: /oracleMission <ticks> /oracleDump <run>/run.hwo (M2-R4 rest-oracle,
#                    docs/m2-r4-rest-oracle.md): the headless Mission01 oracle run: single player starts at once (as sp),
#                    no animatic, the task clock advances one universe tick per frame (so the run does not depend on the
#                    wall clock), no /debugScreenshots next-level loop; "oracle: mission tick N" lines every 256 ticks,
#                    quits after <ticks> ticks with "oracle: mission run done"; needs --timeout of several minutes under
#                    ASan (about 10 ticks/s)
#   --oracle-ticks N oracle mode: universe ticks to run (default 512)
#   --mission-ticks N mission mode: universe ticks to run (default 3000)
#   --etg-ticks N    etg mode: ticks of the effect self-test (default 128; 0 = parse test only)
#   --perturb T      oracle or mission mode: one extra gamerand() before tick T (/oracleSelfTestPerturb; tests the compare tool)
#   --hwo            any other mode: also write the oracle stream to <run>/run.hwo (/oracleDump)
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
oticks=512; perturb=""; hwo=0; etgticks=128; mticks=3000
while [[ $# -gt 0 ]]; do
	case "$1" in
		--mode) mode=$2; shift ;;
		--timeout) tmo=$2; shift ;;
		--driver) drv=$2; shift ;;
		--data) data=$2; shift ;;
		--bin) bin=$(realpath -- "$2"); shift ;;
		--oracle-ticks) oticks=$2; shift ;;
		--mission-ticks) mticks=$2; shift ;;
		--etg-ticks) etgticks=$2; shift ;;
		--perturb) perturb=$2; shift ;;
		--hwo) hwo=1 ;;
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
# a release binary (tools/build.sh --release, the build oracle runs use) has no /logFileLoads switch and rejects it
grep -q -a "/logFileLoads" "$bin" || args=(/window /nodebugInt /settingspath "$out/settings")
[[ "$mode" == sp ]] && args+=(/debugScreenshots)
[[ "$mode" == statics ]] && args+=(/catShipStaticsTest)
[[ "$mode" == textures ]] && args+=(/catTexturesTest)
if [[ "$mode" == oracle ]]; then
	args+=(/oracleSelfTest "$oticks" /oracleDump "$out/run.hwo")
	[[ -n "$perturb" ]] && args+=(/oracleSelfTestPerturb "$perturb")
elif [[ "$mode" == mission ]]; then
	args+=(/oracleMission "$mticks" /oracleDump "$out/run.hwo")
	[[ -n "$perturb" ]] && args+=(/oracleSelfTestPerturb "$perturb")
elif [[ $hwo -eq 1 ]]; then
	args+=(/oracleDump "$out/run.hwo")
fi
if [[ "$mode" == meshes ]]; then
	# every .geo/.peo the install has: the cataclysm.big TOC (tools/bigfile.py), or for an /ignoreBigfiles data tree
	# the files themselves; paths as the archive names them (backslashes)
	if [[ -e "$data/cataclysm.big" ]]; then
		python3 "$root/tools/bigfile.py" list "$data/cataclysm.big" | awk -F'\t' 'NR > 1 {print $5}' \
			| grep -i -E '\.(geo|peo|mad)$' | sort > "$out/meshes.list"
	else
		(cd "$data" && find -L . -type f \( -iname '*.geo' -o -iname '*.peo' -o -iname '*.mad' \)) | sed 's|^\./||; s|/|\\|g' \
			| sort > "$out/meshes.list"
	fi
	args+=(/catMeshLoadTest "$out/meshes.list")
fi
if [[ "$mode" == etg ]]; then
	# every etg\*.script and etg\*.ebg of the install (the cataclysm.big TOC; update.big has none), or of an
	# /ignoreBigfiles data tree; the engine loads the exe's 8 scripts first, then the rest of the list
	if [[ -e "$data/cataclysm.big" ]]; then
		python3 "$root/tools/bigfile.py" list "$data/cataclysm.big" | awk -F'\t' 'NR > 1 {print $5}' \
			| grep -i -E '^etg\\[^\\]*\.(script|ebg)$' | sort > "$out/etg.list"
	else
		(cd "$data" && find -L . -maxdepth 2 -type f \( -iname '*.script' -o -iname '*.ebg' \) -ipath './etg/*') \
			| sed 's|^\./||; s|/|\\|g' | sort > "$out/etg.list"
	fi
	args+=(/catEtgParseTest "$out/etg.list")
	[[ "$etgticks" -gt 0 ]] && args+=(/catEtgEffectTest "$etgticks")
fi
args+=("${extra[@]}")

run_once() {
	local d=$1
	export HOME="$out/home" HW_Data="$data" SDL_VIDEODRIVER="$d"
	# Automated runs are SILENT: the engine still initialises and "plays" audio, but through SDL's dummy
	# backend, so nothing reaches the speakers (the user heard the intro and front-end music on every
	# self-test, 2026-10-11). Override only for a deliberate, attended listen: HWI_AUDIBLE=1.
	[[ "${HWI_AUDIBLE:-0}" = 1 ]] || export SDL_AUDIODRIVER=dummy
	export ASAN_OPTIONS="log_path=$out/asan:detect_leaks=1:verify_asan_link_order=0"
	export UBSAN_OPTIONS="print_stacktrace=1:log_path=$out/ubsan"
	# LeakSanitizer runs at a normal exit (M2-R3: quits no longer stop at the munmap assertion); its report lands in the
	# ubsan.* log (UBSan's log_path wins), and known start-up leaks of GoK and system libraries are suppressed
	export LSAN_OPTIONS="suppressions=$root/tools/lsan.supp:print_suppressions=0"
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
grep -q "debugScreenshots univUpdateCounter\|oracle: mission tick" "$L" && milestone="universe running (univUpdateCounter > 0)"
grep -q "shipstatic: self-test done" "$L" && milestone="ship statics self-test done"
grep -q "meshtest: self-test done" "$L" && milestone="mesh self-test done"
grep -q "texturetest: self-test done" "$L" && milestone="texture self-test done"
grep -q "oracle: self-test done" "$L" && milestone="oracle self-test done"
grep -q "oracle: mission run done" "$L" && milestone="oracle mission run done"
grep -q "etgtest: self-test done" "$L" && milestone="etg parse self-test done"
grep -q "etgtest: effect self-test done" "$L" && milestone="etg effect self-test done"

# first stop: the first fatal error / failed assertion / ASan report. UBSan runtime errors do not stop the engine; the
# first one is reported separately, except the misaligned-access reports on the packed sound-bank and front-end
# structs that every run prints (GoK 64-bit artefacts, docs/m2-r1-data-deltas.md 5.4)
# [M2-R3 etg-particles: an ASan report can land in the ubsan.* log (UBSan's log_path wins when both sanitizers write,
# the same as the LeakSanitizer report below); the stop and the ASan count read both logs]
stop=$(grep -h -E "Fatal error|Fatal Error|ERROR: AddressSanitizer|AddressSanitizer:DEADLYSIGNAL|Unable to open file|SUMMARY: AddressSanitizer: [a-z-]+ |Assertion of" "$L" "$out"/asan.* "$out"/ubsan.* 2>/dev/null | grep -v "SUMMARY: AddressSanitizer: .* leaked" | head -n 1)
ubsan1=$(grep -h "runtime error" "$L" "$out"/ubsan.* 2>/dev/null \
	| grep -v -E "misaligned address .* type '(struct PATCH|struct fescreen|struct featom|struct fibfileheader)'" | head -n 1)
if [[ -z "$stop" ]]; then
	if [[ $rc -eq 124 || $rc -eq 137 ]]; then stop="none: still running at the ${tmo}s timeout"; else stop="none found in the log (rc=$rc)"; fi
fi
asan_n=$(cat "$out"/asan.* "$out"/ubsan.* 2>/dev/null | grep -c "ERROR: AddressSanitizer")
leak_sum=$(cat "$out"/asan.* "$out"/ubsan.* 2>/dev/null | grep -h "SUMMARY: AddressSanitizer: .* leaked" | head -n 1)
ubsan_kinds=$(cat "$L" "$out"/ubsan.* 2>/dev/null | grep "runtime error" | sed -E 's/^.*src\///; s/:[0-9]+:[0-9]+: runtime error:/: /; s/0x[0-9a-f]+/X/g' | sort | uniq -c | sort -rn | head -8)
nload=$(wc -l 2>/dev/null < "$out/FileLoads.log" || echo 0)   # [audit: 2>/dev/null first: a release run has no FileLoads.log]

{
	echo "label=$label mode=$mode driver=$drv timeout=$tmo rc=$rc wall=${wall}s"
	echo "binary=$bin"
	echo "data=$data$([[ "$data" != "$DATA" ]] && echo '  (ISOLATION RUN: not the harness data dir)')"
	echo "args=${args[*]}"
	echo "milestone: $milestone"
	echo "first stop: $stop"
	echo "first non-benign UBSan report (non-fatal): ${ubsan1:-none}"
	echo "file loads: $nload; ASan errors: $asan_n; LeakSanitizer at exit: ${leak_sum:-no unsuppressed leak}"
	echo "ship statics: $(grep -c 'shipstatic: loaded' "$L") loaded, $(grep -c 'shipstatic: .* not found' "$L") not found (disabled), $(grep -c 'shipstatic: other' "$L") other .shp; $(grep -h 'shipstatic: self-test done' "$L" | sed 's/^.*shipstatic: //' | head -n 1)"
	[[ "$mode" == meshes ]] && echo "meshes: $(grep -c . "$out/meshes.list" 2>/dev/null) listed, $(grep -c 'meshtest: loaded' "$L") .geo/.peo loaded, $(grep -c 'meshtest: mad loaded' "$L") .mad loaded, $(grep 'meshtest: .*loaded' "$L" | grep -c -v ' bad=0$') with bad=; $(grep -h 'meshtest: self-test done' "$L" | sed 's/^.*meshtest: //' | head -n 1)"
	[[ -f "$out/run.hwo" ]] && echo "oracle: $(grep -h 'oracle: self-test done\|oracle: mission run done' "$L" | sed 's/^.*oracle: //' | head -n 1); run.hwo $(stat -c %s "$out/run.hwo") bytes, sha256 $(sha256sum "$out/run.hwo" | cut -c1-16)"
	[[ "$mode" == mission ]] && echo "mission: last tick line: $(grep -h 'oracle: mission tick' "$L" | tail -n 1 | sed 's/^.*oracle: //'); ships loaded $(grep -c 'shipstatic: loaded' "$L")"
	[[ "$mode" == textures ]] && echo "textures: $(grep -h 'texturetest: listing lookups\|texturetest: self-test done' "$L" | sed 's/^.*texturetest: //' | paste -sd';')"
	[[ "$mode" == etg ]] && echo "etg: $(grep -c . "$out/etg.list" 2>/dev/null) listed; $(grep -h 'etgtest: etgStartup done\|etgtest: self-test done\|etgtest: created\|etgtest: effect self-test done' "$L" | sed 's/^.*etgtest: //' | paste -sd';')"
	echo "engine glows:$(grep -c -i 'engineglows' "$out/FileLoads.log" 2>/dev/null) mesh/mex loads from misc\\EngineGlows in FileLoads.log"
	echo "UBSan report kinds (count, site):"; echo "$ubsan_kinds"
	echo "log tail:"; grep -v '^    #\|^ *$' "$L" | tail -n 8
} | tee "$out/summary.txt"
echo "run=$label mode=$mode driver=$drv rc=$rc wall=${wall}s data=$data milestone=\"$milestone\" stop=\"$stop\"" >> "$HARNESS/runs/runs.txt"
