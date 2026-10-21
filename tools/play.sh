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
#             /noNISInterp (cutscene interpolation off only), /window, /fullscreen, /1600 (1600x1200), /noBorder,
#             /uiScale N (the UI scale for this run, see below)
# Logs: <home>/logs/play-<timestamp>.log (stdout+stderr). Exit code = the engine's.
#
# Smooth motion (docs/features/motion-interpolation.md): ON by default. It is the SMOOTH MOTION checkbox on
# Options -> Video (the front end's and the Escape menu's); Accept saves it as the Cataclysm.cfg rows
#   objInterpolate 0|1 / nisInterpolate 0|1   (both 1 = on; a file that says 0 keeps it OFF in every session)
# The switches -- /noInterp (all off) and -- /noNISInterp (cutscenes only) are this run only: the options file keeps
# its rows (since M2-R7; the M2-R6 builds of 2026-10-18 wrote a switch's 0 into the file, and until M3-R1 an options
# screen closed during a /noInterp run, or Cancel after a click, could save the 0 too). If Smooth motion seems off,
# open Options -> Video and tick SMOOTH MOTION, or set both rows to 1 in <home>/.homeworld/Cataclysm.cfg.
#
# Cutscene letterbox (M3-R1, HWI-FEATURE(letterbox-setting)): the black bars of an NIS and of a mission's letterbox
# are drawn IN FRONT of the scene by default (as cataclysm.exe draws them: a ship flying into a bar is hidden). The old
# look of the M2 builds (the bars behind ships close to the camera) is an opt-in setting:
#   Cataclysm.cfg row   letterboxBehind 0|1   (default 0 = bars in front; edit <home>/.homeworld/Cataclysm.cfg)
#   console row         letterboxBehind 0|1   (the developer console's; as in cataclysm.exe no key opens that console,
#                       so for play the Cataclysm.cfg row is the way; a value set there is saved with the options)
#   switch              -- /letterboxBehind   (this run only: the options file keeps its own letterboxBehind row)
#
# Full detail (M3-R1, HWI-FEATURE(full-detail)): ON by default: every ship, asteroid, derelict, dust cloud and nebula
# is drawn with its most detailed model (LOD 0) at any distance. It is the FULL DETAIL checkbox of Options -> Video
# (cataclysm.exe's own checkbox, which there only stopped the frame-time detail cuts; while it is ticked the detail
# slider is greyed out, as in the exe). The Options frame-rate preset "high" ticks it, "low" unticks it.
#   Cataclysm.cfg row   noLOD 0|1   (default 1 = full detail; 0 = the original game's levels of detail by size on screen)
#   console row         noLOD 0|1   (the developer console's; as in cataclysm.exe no key opens that console)
#   switch              -- /lodBands   (this run only: full detail off; the options file keeps its own noLOD row)
#
# Team colours (M3-R1 team-colours-warp): the colours picked in the front end's colour picker (Cataclysm.cfg rows
# PlayerBaseColor / PlayerStripeColor / ColorsPicked) stay on the Kuun-Lan and the fleet through every level change
# (the M2 builds lost them when the opening cutscene's level gave way to Mission01). No setting. To have the play log
# record them, add the test switch -- /catTeamColourTest: "teamcolour: check ... result=PASS|FAIL" lines after each
# level's load and at its 64th tick in <home>/logs/play-*.log (read only: it changes nothing in the game).
#
# UI scale (M3-R1, HWI-FEATURE(ui-scale)): everything drawn in 2D (the front end, the HUD and task bar, the fonts, the
# cursor, the subtitles, the letterbox bars, the managers, the menus, the console) is drawn at a UI scale; the 3D view
# keeps the window's full resolution. AUTO by default: 200% when the window is 2160 rows or taller (4K), 150% at 1440 or
# more (2K), 100% below; the front end's 640x480 art then shows at 2x (4K) or 1.5x (2K) instead of small in the middle.
#   Cataclysm.cfg row   uiScale 0|100|150|200   (0 = auto, the default; any 100 .. 400 works in the file)
#   Options -> Video    UI SCALE: AUTO / 100% / 150% / 200% (the front end's Video screen and the Escape menu's); Accept
#                       in the front end applies it at once; in a game it is saved and applies at the next start
#   console row         uiScale N   (the developer console's; as in cataclysm.exe no key opens that console)
#   switch              -- /uiScale N   (this run only: the options file keeps its own uiScale row)
#
# The window at 4K (M3-R1, HWI-FIX; the mouse that could not reach the bottom ~200 rows): a WINDOWED request as large as
# the desktop (Cataclysm.cfg fullScreen 0 with screenWidth / screenHeight the display's size, or /window) now opens as a
# borderless window covering the display (SDL fullscreen-desktop), so no title bar or panel hides its bottom rows; a
# windowed request larger than the desktop's usable area (less the panels) but smaller than the display is shrunk to fit
# it, aspect kept. A smaller window stays as requested. /fullscreen is unchanged (fullscreen-desktop), /noBorder makes
# the window borderless. The log's "window: requested WxH, usable WxH (display N: WxH), created WxH (...)" line and the
# "uiscale: physical .. logical .." line say what was made.
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
		-h|--help) sed -n '2,/^set -euo/p' "$0" | sed '$d'; exit 0 ;;
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
