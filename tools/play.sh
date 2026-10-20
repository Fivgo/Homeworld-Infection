#!/usr/bin/env bash
# play.sh - launch the recreation (hwi-engine) in a real window for play-testing. NOT a harness run: audio on,
# a real display, the user's own persistent settings.
#
# Usage: tools/play.sh [--debug] [--bin PATH] [--data DIR] [--home DIR] [--driver x11|wayland] [--rebuild] [-- <engine switches>]
#   build     release (build/release/hwi-engine; built with tools/build.sh --release if missing or --rebuild);
#             --debug uses build/debug/hwi-engine (ASan: much slower, prints sanitizer reports);
#             --bin PATH runs another engine binary (e.g. a fix branch's worktree build)
#   data      --data DIR, default original/scratch-m2/m2r2-harness/data (symlinks into original/game, the
#             unmodified GOG install; tools/m2_harness.sh --setup creates it)
#   home      --home DIR, default original/play-home: a private HOME so the recreation's Cataclysm.cfg, keys.cfg,
#             saved games, recordings and screenshots live there and never touch ~/ or the GOG install. Seeded on
#             first use with TutorialNeeded 0 and PlayIntros 0 (edit .homeworld/Cataclysm.cfg there to change).
#   driver    SDL video driver, default x11 (XWayland on COSMIC); --driver wayland to try SDL's native backend
#   switches  everything after -- goes to the engine, e.g. /noInterp (smooth motion off for an A/B run),
#             /noNISInterp (cutscene interpolation off only), /window, /fullscreen, /1600 (1600x1200), /noBorder
# Logs: <home>/logs/play-<timestamp>.log (stdout+stderr). Exit code = the engine's.
#
# Smooth motion (docs/features/motion-interpolation.md): ON by default. Toggle in game with the "Smooth motion"
# checkbox on Options -> Video (front end and the Escape menu), or with the console (tilde key): objInterpolate 0|1,
# nisInterpolate 0|1. Note: the switches /noInterp and /noNISInterp set the same flags, and saving options while
# they are off writes 0 into Cataclysm.cfg (the run-only override is an M2-R7 follow-up): re-enable with the checkbox.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
build=release; bin=""; data="$root/original/scratch-m2/m2r2-harness/data"; home="$root/original/play-home"; drv=x11; rebuild=0
while [[ $# -gt 0 ]]; do
	case "$1" in
		--debug) build=debug; shift ;;
		--bin) bin="$2"; shift 2 ;;
		--data) data="$2"; shift 2 ;;
		--home) home="$2"; shift 2 ;;
		--driver) drv="$2"; shift 2 ;;
		--rebuild) rebuild=1; shift ;;
		--) shift; break ;;
		-h|--help) sed -n '2,22p' "$0"; exit 0 ;;
		*) echo "play.sh: unknown option $1 (engine switches go after --)" >&2; exit 2 ;;
	esac
done
[[ -n "$bin" ]] || bin="$root/build/$build/hwi-engine"
if [[ $rebuild = 1 || ! -x "$bin" ]]; then
	echo "play.sh: building $build engine ..." >&2
	"$root/tools/build.sh" "--$build" >&2
fi
[[ -e "$data/cataclysm.big" ]] || { echo "play.sh: no Cataclysm data at $data (run tools/m2_harness.sh --setup, or pass --data)" >&2; exit 1; }
mkdir -p "$home/.homeworld" "$home/logs"
[[ -e "$home/.homeworld/Cataclysm.cfg" ]] || printf 'TutorialNeeded    0\nPlayIntros    0\n' > "$home/.homeworld/Cataclysm.cfg"
log="$home/logs/play-$(date +%Y%m%d-%H%M%S).log"
echo "play.sh: $bin  data=$data  home=$home  driver=$drv  switches: $*  log=$log" >&2
export HOME="$home" HW_Data="$data" SDL_VIDEODRIVER="$drv"
unset SDL_AUDIODRIVER   # audio on: this is a play session
cd "$data"
set +e
"$bin" "$@" 2>&1 | tee "$log"
rc=${PIPESTATUS[0]}
echo "play.sh: engine exit $rc (log $log)" >&2
exit "$rc"
