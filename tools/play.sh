#!/usr/bin/env bash
# play.sh - launch the recreation (hwi-engine) in a real window for play-testing. NOT a harness run: audio on,
# a real display, the user's own persistent settings.
#
# Usage: tools/play.sh [--debug] [--bin PATH] [--data DIR] [--home DIR] [--driver x11|wayland] [--rebuild] [-- <engine switches>]
#   build     release (build/release/hwi-engine; built with tools/build.sh --release if missing or --rebuild);
#             (also rebuilt when older than the newest src/ commit or the tree has uncommitted src/ changes)
#             --debug uses build/debug/hwi-engine (ASan: much slower, prints sanitizer reports);
#             --bin PATH runs another engine binary (e.g. a fix branch's worktree build)
#   data      --data DIR, default original/scratch-m2/m2r2-harness/data (symlinks into original/game, the
#             unmodified GOG install; tools/m2_harness.sh --setup creates it). Run from the MAIN checkout
#             (~/Documents/GameDev/Homeworld-Infection) or from a worktree: a worktree's original/ has no data, so the
#             script then uses the main checkout's original/ for the data and the home (the engine binary stays the
#             worktree's own build)
#   home      --home DIR, default original/play-home: a private HOME so the recreation's Cataclysm.cfg, keys.cfg,
#             saved games, recordings and screenshots live there and never touch ~/ or the GOG install. Seeded on
#             first use with TutorialNeeded 0 (edit .homeworld/Cataclysm.cfg there to change). [M3-R4 intro-movies:
#             no longer PlayIntros 0: the intro logos play, see below]
# The shortcut: tools/launch.sh runs this script with its defaults from the "Homeworld Infection" shortcut (./setup.sh
#   installs it); with HWI_LAUNCHER=1 in the environment a build sends a desktop notification first.
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
# The ship meshes' submission (M3-R2, HWI-FEATURE(mesh-batching)): FULL DETAIL's cost at 4K was the old immediate-mode
# mesh path (one GL call per vertex corner); the ships are now drawn in vertex arrays, one draw per material run, with
# the same lighting, colours and order (the pixels are byte-identical in every harness capture). ON; no options-file row.
#   switch              -- /meshImmediate   (this run only: the old one-call-per-vertex path, for an A/B of the frame time)
#   console row         meshImmediate 0|1   (the developer console's; as in cataclysm.exe no key opens that console)
#   the settings line   "meshImmediate=0 (default)" (1 (switch) with -- /meshImmediate)
#
# Texture filtering (M3-R3, HWI-FEATURE(sharp-textures)): cataclysm.exe's own SURFACE FILTERING box. Unticked, the ship,
# derelict and asteroid textures are magnified as square texels (GL_NEAREST) instead of the original game's smooth
# (linear) look; ticked (the default) is the original look. The effect sprites and the trails' billboard sprites stay
# smooth either way, as in cataclysm.exe (its particle drawing always filters them). (The mod's own SHARP TEXTURES
# checkbox did the same thing the other way round and was removed on 2026-10-31.)
#   Options -> Video -> CUSTOM EFFECTS   SURFACE FILTERING (in game: the Escape menu's Video -> CUSTOM EFFECTS): the
#                       textures change at once (a live preview); that screen's ACCEPT makes the EFFECTS level CUSTOM,
#                       because the HIGH effects preset means filtering on and is re-applied at every start; the Video
#                       screen's Accept saves it, Cancel puts it back
#   Cataclysm.cfg row   texLinearFiltering 0|1   (1 = the original look; 0 = sharp. NOTE: by hand it only holds with
#                       effectsLevel 2 (CUSTOM): with effectsLevel 0 (HIGH) the game turns it back to 1 at every start,
#                       exactly as cataclysm.exe does; the log then says "texLinearFiltering=1 (preset) [effects preset]")
#   switch              -- /noFilter   (this run only, over any effects level; the file keeps its row)
#   console row         texLinearFiltering 0|1   (the developer console's; as in cataclysm.exe no key opens that console)
#   the log             "texfilter: filter=sharp|linear textures=N globjects=M wrong=0" at every change (wrong must be 0)
#
# Engine trails (M3-R3, HWI-FEATURE(engine-trails)): the missile corvettes (the Beast's bMissileCorvette and the raiders'
# P1MissileCorvette, Mission03 onward) draw FOUR jet streams, one from the centre of each of the four glowing engine
# circles on their rear. ON by default. cataclysm.exe draws TWO (one per engine nozzle in the model's data, each wide
# enough to cover an upper and a lower circle); every other ship keeps the exe's trails. The engine glow is the exe's.
#   switch              -- /exeTrails   (this run only: the exe's two ribbons, for an A/B)
#   console row         engineTrails 0|1   (the developer console's; as in cataclysm.exe no key opens that console)
#   the settings line   "engineTrails=1 (default)" (0 (switch) with -- /exeTrails)
#
# Sensors points (M3-R4, HWI-FEATURE(sensors-points)): in the sensors manager (Space) the "Dust" (the faint tan-grey dots
# of the decorative asteroid belts, e.g. Mission02's arcs; the sensors menu's "Dust" option) are 2 pixels instead of
# cataclysm.exe's 1, and the resource, meteor, mine and crate markers 3 pixels instead of the exe's 2 (1 for the smallest
# ones), so every marker is bigger than the Dust. ON by default. Fixed screen pixels (the UI scale does not change them);
# the ship blips keep the exe's 2 (a mimic hidden as an asteroid is drawn at the markers' 3, as the exe draws it at a
# big asteroid's size).
#   switch              -- /exePoints   (this run only: the exe's 1 / 2 pixel points, for an A/B)
#   console row         sensorsPoints 0|1   (the developer console's; as in cataclysm.exe no key opens that console)
#   the settings line   "sensorsPoints=1 (default)" (0 (switch) with -- /exePoints)
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
# Menu backgrounds (HWI-FEATURE(screen-background), docs/features/screen-background.md): a picture of your own behind a
# menu screen: <home>/.homeworld/Assets/feman/backgrounds/<screen>.png (Video_Options = Options -> Video in the main
# menu, Video = the Escape menu's Video screen), stretched over the screen's 640x480 layout (1280x960 is sharp up to
# 200%), with alpha; the game's panels, buttons and text are drawn over it. The folder's README.txt and .ora files (GIMP,
# with layers: the game's art and an empty layer to paint on) say how. Delete the PNG for the original look.
#
# Hyperspace of a modular ship (M3-R2, the Kuun-Lan): the gate is sized as cataclysm.exe sizes it (the .shp's
# hyperspaceBoxScale: 6x the spine for the Kuun-Lan, so it spans the whole ship) and the modules are never cut away
# after the jump (the M3-R1 builds cut the back modules for ~1 s at the end of an arrival and while a departure began).
# On top (HWI-FEATURE(hyperspace-assembly)), the gate and the reveal cover every module, and a module still waiting to
# appear is drawn under the hull's gate from its first frame. ON; no options-file row. The log's "hsgate:" /
# "hsarrive:" / "hsassembly:" / "hsfinish:" lines record each modular ship's jump (read only).
#   switch              -- /noHsAssembly   (this run only: the exe's look, the modules shown at their own exits)
#
# The window at 4K (M3-R1, HWI-FIX; the mouse that could not reach the bottom ~200 rows): a WINDOWED request as large as
# the desktop (Cataclysm.cfg fullScreen 0 with screenWidth / screenHeight the display's size, or /window) now opens as a
# borderless window covering the display (SDL fullscreen-desktop), so no title bar or panel hides its bottom rows; a
# windowed request larger than the desktop's usable area (less the panels) but smaller than the display is shrunk to fit
# it, aspect kept. A smaller window stays as requested. /fullscreen is unchanged (fullscreen-desktop), /noBorder makes
# the window borderless. The log's "window: requested WxH, usable WxH (display N: WxH), created WxH (...)" line and the
# "uiscale: physical .. logical .." line say what was made.
#
# The resolution list and the resizable window (M3-R4 window-modes, HWI-FEATURE(resolution-presets) / (window-resize)):
# the Video screen's list holds the typical 4:3, 16:10 and 16:9 sizes that fit a connected display (640x480 .. 2048x1536,
# 1280x800 .. 2560x1600, 1280x720 .. 3840x2160, the exe's 5:4 1280x1024), each display's own mode and the size in use,
# labelled by aspect (cataclysm.exe listed the driver's modes; the SDL port listed the desktop and its halves, thirds and
# quarters only). A windowed mode can be dragged to any size from 640x480 up: the UI scale is chosen again for the new
# height, the layout, the 3D view, the mouse and the HUD follow at once (in a game an open manager or menu keeps its
# layout until it closes), and the dragged size is saved as the window's size (screenWidth / screenHeight) at an Accept
# and at exit, so the next start opens at it and the Video screen lists it. A windowed request the size of the display
# still opens as a borderless fullscreen-desktop window (no frame to drag): pick a smaller size for a framed window.
# The log's "window: resized to WxH (..) scale P% .. logical WxH" line records each change.
#
# The intro logos (M3-R4 intro-movies, HWI-FEATURE(intro-movies)): the Sierra logo (16 s, its music, then 2 s on its
# last frame) and the Barking Dog logo (27 s, its music) play at every start, as cataclysm.exe plays them; a key skips
# the one playing. They keep their 4:3 shape: fitted to the window's height (or width) with black bars, never
# stretched; the mission movies (the animatics) likewise keep their 5:3 picture whole. The log's "movie:" lines say where
# each was drawn and how it ended.
#   Cataclysm.cfg row   PlayIntros 0|1   (default 1; 0 = no intro logos)
#   switch              -- /disableAVI   (this run only: no intro logos)
#
# The settings line (M3-R2, HWI-FEATURE(instrumentation)): the log's "hwi-settings:" line names every recreation setting
# with its value and where it came from, once at start-up and again whenever one changes:
#   hwi-settings: objInterpolate=1 (cfg) nisInterpolate=1 (cfg) letterboxBehind=0 (cfg) noLOD=1 (cfg)
#                 uiScale=0->200% (cfg, auto) overlay=0 (default) meshImmediate=0 (default) texLinearFiltering=1 (cfg)
#                 fit=fullscreen-desktop (...) 3840x2160 switches=none [start]
#   the source: default (Cataclysm.cfg has no row), cfg (its row, or the row Options -> Accept saved), switch (a -- switch
#   overrides the row for this run), console (a developer-console row), options (an Options control, not yet accepted),
#   key (the overlay key), preset (M3-R3: the EFFECTS preset HIGH / LOW set it, also at every start); [why]: start,
#   options accept, console <row>, key overlay, effects preset, window (a mode switch), frame.
#   A console row under a switch keeps the label "switch" (the switch still keeps the options file's row) except for
#   the overlay and meshImmediate, which have no row: there the console row ends the switch.
#   So "objInterpolate=0 (cfg)" means Smooth motion is OFF because the options file says so.
# The overlay (M3-R2, HWI-FEATURE(instrumentation)): OFF by default; a small panel in the top-left corner, drawn at the
# UI scale over everything: FRAME (the last frame's period in ms, the last second's median and 99th percentile; at
# 144 Hz a frame within budget is 6.94 ms or less), TICK (the universe tick's cost in the last frame that ran one, and
# how many ran), ALPHA (the motion-interpolation blend the frame was drawn with, -1 = none), the flags (OBJ / NIS
# interpolation, LBB letterboxBehind, NOLOD = FULL DETAIL) and the switches, VERTS / OBJECTS (the vertices and objects
# of the levels of detail drawn in the main view), FULL DETAIL and the UI scale.
#   key                 F1 (M3-R3, the user's choice of an F key): toggles it. F1 is the one F key cataclysm.exe leaves
#                       free in the game view (its default_keys.cfg binds F2..F12 to the tactics and formations, F12 =
#                       Formation_Parade, and F1 only with Shift / Ctrl). F1 yields: with Shift, Ctrl or Alt held it is
#                       the game's (Shift+F1 Leech_Spy, Ctrl+F1 the sensors' areas visited, Shift+Ctrl+F1 the LOD
#                       drawing), and if you bind plain F1 to a command on the KEY BINDINGS screen it is that command's
#   key                 the keyboard's Menu key (the "application" key, usually right of the right Windows key): toggles
#                       it too; no default key binding of cataclysm.exe uses it and this engine's keys.cfg cannot bind it
#                       [audit: said "cataclysm.exe has no use for that key"]
#   console row         overlay 0|1
#   switch              -- /overlay   (on for this run; nothing is saved: the overlay has no Cataclysm.cfg row)
# The frame log (M3-R2, HWI-TEST; docs/features/high-refresh-4k.md "The soak's instrument") [audit: added]:
#   switch              -- /frameLog <file>   (this run only: one line a second to <file>: frame-time p50 / p95 / p99 /
#                       max, the frames over the refresh interval, the universe ticks and their cost, the heap, the
#                       vertices drawn; give a full path, e.g. -- /frameLog ~/hwi-frame-$(date +%H%M).log)
#   report              python3 -I tools/frame_log_report.py <file>   (the whole session's percentiles, the drift test,
#                       the heap's growth a minute, the seconds over budget)
# The per-frame signal (M3-R2 nis-flicker, HWI-TEST; docs/m3-r2-nis-flicker.md) [the cutscene one-frame flicker]:
#   switch              -- /catFrameSig <file>   (this run only: ONE line per rendered frame to <file>: the main view
#                       drawn (mv) and its objects (rl render list, vis visible), the letterbox bars drawn late (lb: -
#                       none, full, or the fade), the NIS black fade (fd), the NIS time (nis) and blend (a), the frame's
#                       wall-clock period in ms (ft); no pixel read-back in a play-test, so it costs next to nothing;
#                       give a full path, e.g. -- /catFrameSig ~/hwi-sig-$(date +%H%M).txt; about 15 MB an hour at 144 Hz)
#   report              python3 -I tools/dump_outliers.py <file>   (the frames that lost their objects or their bars for
#                       up to 12 frames between frames that had them; "short runs=0" = the engine drew the bars and the
#                       objects on every frame of the session)
# The mines (M3-R3 mines, docs/m3-r3-mines.md) [audit: added]: the mines' and missiles' textures are now loaded for the
# colour scheme they are drawn with (an x86-64 fault, HWI-FIX; no setting); the mine AI is cataclysm.exe's, unchanged.
#   switch              -- /catMineLog <ticks>   (HWI-TEST, read only: "minelog: change ... state=A->B why=..." for every
#                       mine state change, why = acquired / faster-away / range / cloaked / disabled / cleared, and
#                       <ticks> ticks of distance and speeds for the first 8 mines that start a chase, e.g. 480)
#   switch              -- /catMeshTexLog MINE   (HWI-TEST, read only: "meshtex: draw ... bound= white=" at the first
#                       draw of each mine mesh per colour scheme; white=0 = every textured material bound)
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
# The game data and the play home live under the MAIN checkout's original/ (gitignored; a worktree such as
# .claude/worktrees/m2 has an original/ of its own with neither). From a worktree, fall back to the main checkout's
# (2026-10-26: a launch from the m2 worktree rebuilt the engine, then stopped at "no Cataclysm data").
orig="$root/original"
if [[ ! -e "$orig/scratch-m2/m2r2-harness/data/cataclysm.big" ]]; then
	common=$(git -C "$root" rev-parse --path-format=absolute --git-common-dir 2>/dev/null || true)
	[[ -n "$common" && -e "${common%/.git}/original/scratch-m2/m2r2-harness/data/cataclysm.big" ]] && orig="${common%/.git}/original"
fi
build=release; bin=""; data="$orig/scratch-m2/m2r2-harness/data"; home="$orig/play-home"; drv=x11; rebuild=0
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
bin_given="$bin"
[[ -n "$bin" ]] || bin="$root/build/$build/hwi-engine"
# Build when asked (--rebuild), when the binary is missing, when it is older than the newest commit that touched src/,
# or when the tree has uncommitted src/ changes. (2026-10-25: a play-test ran a four-day-old binary because --rebuild
# was not given; the newest play log had no hwi-settings line.) A binary given with --bin is used as it is.
why=""
if [[ -n "$bin_given" ]]; then
	[[ $rebuild = 1 ]] && why="--rebuild (note: --bin given, building the default binary, not $bin)"
elif [[ $rebuild = 1 ]]; then why="--rebuild"
elif [[ ! -x "$bin" ]]; then why="no binary yet"
else
	newest_src=$(git -C "$root" log -1 --format=%ct -- src/ 2>/dev/null || echo 0)
	if [[ $(stat -c %Y "$bin") -lt ${newest_src:-0} ]]; then
		why="binary built $(date -r "$bin" +%F\ %H:%M), newest src/ commit $(date -d @"$newest_src" +%F\ %H:%M)"
	elif [[ -n $(git -C "$root" status --porcelain -- src/ 2>/dev/null) ]]; then
		why="uncommitted src/ changes"
	fi
fi
if [[ -n "$why" ]]; then
	echo "play.sh: building $build engine ($why) ..." >&2
	if [[ -n "${HWI_LAUNCHER:-}" ]] && command -v notify-send >/dev/null 2>&1; then   # the shortcut (tools/launch.sh)
		notify-send -a "Homeworld Infection" "Homeworld Infection" "Preparing the game: building the engine (about a minute) ..." || true
	fi
	"$root/tools/build.sh" "--$build" >&2
else
	echo "play.sh: $build engine up to date (built $(date -r "$bin" +%F\ %H:%M))" >&2
fi
[[ -e "$data/cataclysm.big" ]] || { echo "play.sh: no Cataclysm data at $data (run tools/m2_harness.sh --setup, or pass --data)" >&2; exit 1; }
mkdir -p "$home/.homeworld" "$home/logs"
[[ -e "$home/.homeworld/Cataclysm.cfg" ]] || printf 'TutorialNeeded    0\n' > "$home/.homeworld/Cataclysm.cfg"
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
