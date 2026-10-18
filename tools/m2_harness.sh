#!/usr/bin/env bash
# m2_harness.sh - M2-R2 test harness: run build/debug/hwi-engine headless against an UNMODIFIED Cataclysm install.
#
# Usage:
#   tools/m2_harness.sh --setup                 (re)create the harness data dir if missing (--force: rebuild it)
#   tools/m2_harness.sh <label> [options] [-- extra engine args]
# Options:
#   --mode sp|fe|statics|meshes|textures|oracle|etg|mission|dock|command|render|sound|hud|ui|fescreens|strings|keys|console|build|nisparse|motion|nis|toggle|replay|ai
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
#                    quits after <ticks> ticks with "oracle: mission run done" (M2-R6 nis-fow-camera: then, on stdout only,
#                    "oracle: mission fow players=.. curPlayer=.. gridN=set|NULL stealthUpdates=.. repairbots=.. droidUpdates=.."); needs --timeout of several minutes under
#                    ASan (about 10 ticks/s)
#                    dock: /catDockTest <ticks> (M2-R5 layout-dock, docs/m2-r5-layout-dock.md): the headless Mission01
#                    run of the mission mode with the scripted dock self-test (Dock.c catDockSelfTestTick): at tick 16 the
#                    player's acolytes, recons and ACVs dock for support (purpose 0), a Worker deposits a scripted hold
#                    (purpose 3), another harvests; M2-R6 managers: two more acolytes dock at an sCarrier (the carrier
#                    leg), a fourth Worker deposits at the sProcessor (the processor leg), and the ACVs are judged; once
#                    every fighter and ACV is inside its host (at the latest at 80% of the ticks) they are checked and
#                    launched (purpose 2); at the end they are checked outside; M2-R6 nis-fow-camera: the harvest lines
#                    log the Worker's mesh animation (mad=number/paused/at end) and the done line jackPausedTick= (the
#                    first tick it held the paused end of its jack animation); "docktest:"
#                    log lines, "docktest: done
#                    ... result=PASS|FAIL"; quits after <ticks> ticks (needs --timeout of several minutes under ASan)
#                    command: /catCommandTest <ticks> (M2-R5 command-tactics, docs/m2-r5-command-tactics.md): the
#                    headless Mission01 run of the mission mode with the scripted command self-test (CommandLayer.c
#                    catCommandSelfTestTick): at tick 16 a scripted fleet gets the link, mimic, research, point and
#                    force-field wall and sphere orders, the linked ACV is unlinked again, the end states are checked at
#                    the end; "commandtest:" log lines, "commandtest: done ... result=PASS|FAIL" (M2-R6 nis-fow-camera:
#                    then "commandtest: fow grid=... stealthUpdates=N", the mimics' stealth detection); quits after <ticks>
#                    ticks (needs --timeout of several minutes under ASan)
#                    render: /catRenderTest <frames> (M2-R5 render, docs/m2-r5-render.md): the headless Mission01 run of
#                    the mission mode drawn through the Cataclysm object-list renderer; at tick 16 the lens flare
#                    option goes on, the NIS light source is shown ahead of the camera and a fog line is set; M2-R6
#                    nis-fow-camera: the repair-droid option goes on and a ship with droids (a scripted player sProcessor
#                    while Mission01's Sect fleet is in the hyperspace ether) is damaged to half health with upgrade
#                    0x200000 and focused, so its droids launch, weld and are drawn ("rendertest: droids" line, "counts
#                    droids=N droidUpdates=N", droids > 0 is part of PASS); at the end
#                    "rendertest: counts" lines (objects, sorted objects, trails, lens flares, fog layers, ...) and
#                    "rendertest: done ... result=PASS|FAIL"; quits after <frames> ticks (one tick a frame)
#                    sound: /catSoundTest (M2-R5 sound, docs/m2-r5-sound.md): after the start-up, one stream of each audio
#                    bank (a Music4Cat.wxd track, a SectSpeech.vce and a BeastSpeech.vce sentence) is queued and decoded
#                    by the mixer through the fq codec into the audio device, which this script forces to SDL's dummy
#                    driver (nothing is audible); then the 88 battle-chatter events are looked up in both races' tables;
#                    "soundtest:" log lines, "soundtest: done ... result=PASS|FAIL"; quits (about 30 s on the wall clock)
#                    hud: /catHudTest <frames> (M2-R5 hud, docs/m2-r5-hud.md): the headless Mission01 run of the mission
#                    mode with the HUD on: at tick 16 the camera focuses the player's ships, at tick 64 they are
#                    band-boxed over the whole screen through the selection API, the ship info panel goes on, a message
#                    refers to the first ship and the movement pie plate goes on; every frame then draws the status
#                    bars, the info panel, the main overlay and the pie plate offscreen; M2-R6 managers: the last 120
#                    frames are the sensors manager's (instant transition, the selection as hot-key group 1), drawn
#                    through the Cataclysm scene (cat_sm*: "hudtest: sensors" counts); at the end "hudtest: counts"
#                    lines and "hudtest: done ... result=PASS|FAIL"; quits after <frames> ticks (one tick a frame)
#                    ui: /catUiTest <ticks> (M2-R6 mainrgn-popup, docs/m2-r6-mainrgn-popup.md): the headless Mission01
#                    run of the mission mode: at tick 16 the player's fleet is selected through the selection API, at
#                    tick 24 the right-click popup menu opens for it (and is drawn), at tick 40 every cat_ui* command
#                    runs once through the command wraps, the menu opens once for an enemy target (a resource while
#                    Mission01 has no enemy ship, as every r6 run: target=resource) [audit: said enemy only] and
#                    closes, at tick 48
#                    the game-stats screen begins (the universe pauses) and is drawn for 8 frames; "uitest:" log
#                    lines, "uitest: done ... result=PASS|FAIL", then the screen ends (to the main menu) and the engine
#                    quits; <ticks> is only the mission run's limit (default 400)
#                    fescreens: /catFeScreensTest <run>/fescreens.list (M2-R6 frontend, docs/m2-r6-frontend.md): once the
#                    front end is up at Main_game_screen, every .fib of both archives' TOCs (cataclysm.big and update.big,
#                    "archive<TAB>name" lines: 22 files, multiplayer_game.fib from each) is loaded from its archive, each of
#                    its screens is pushed with feScreenStart (its CallOnCreate callbacks run), its regions are drawn once
#                    and it is popped (feCurrentScreenDelete), then the file's screens are deleted; "fescreentest:" log
#                    lines, "fescreentest: done ... result=PASS|FAIL"; before the screens, the Cataclysm option set at the
#                    exe's defaults is written to the options file, scrambled, read back and compared ("optionstest:"
#                    lines, "optionstest: done ... result=PASS|FAIL"); quits. The summary counts the 'struct fescreen' /
#                    featom / fibfileheader UBSan reports in every mode (0 since M2-R6 frontend)
#                    strings: /catStringsTest (M2-R6 frontend): after the start-up, every one of the 1,705 string ids is
#                    checked against the language script read again line by line (its row maps back, a key resolves to the
#                    script's text, a missing key stays *NO_STRING* as in the exe) and the ship names are counted;
#                    "stringstest:" log lines, "stringstest: done ... result=PASS|FAIL"; quits
#                    keys: /catKeysTest (M2-R6 startup-keys, docs/m2-r6-startup-keys.md): after the start-up has loaded the
#                    key bindings (no keys.cfg in the run's settings dir yet, so cat_kbLoadKeyBindings makes it from the
#                    install's default_keys.cfg), default_keys.cfg is read again and checked row by row against the loaded
#                    bindings, keys.cfg is checked to be its copy, the bindings are saved (cat_kbSaveKeyBindings) and the
#                    written keys.cfg compared with default_keys.cfg's known rows, reloaded and reset (the table must not
#                    change), a new key is bound, saved, reloaded and reset again, and the key names, the commands of
#                    each key, the conflict test and the debug-key groups are checked; no key is pressed; "keystest:" log
#                    lines, "keystest: done ... result=PASS|FAIL"; quits. The written keys.cfg stays in <run>/settings
#                    console: /catConsoleTest <ticks> (M2-R6 startup-keys): the headless Mission01 run of the mission mode:
#                    at tick 16 the player's fleet is selected through the selection API, at tick 24 the developer console
#                    opens (cat_consoleOpen: the retail game has no opener; a single-player game pauses, so the rest runs
#                    from the console's draw callback); once it has scrolled in, every row of its command table is shown,
#                    set and reset, the command rows run (addResources, warpTo refused, alltech, upgradeships,
#                    experienceships), saveConfig / loadConfig write and read a test file in <run>/settings, lines are
#                    typed through its key handler (Enter, TAB, history, backspace, Escape; no OS input), then the tilde
#                    key scrolls it out; "consoletest:" log lines, "consoletest: done ... result=PASS|FAIL"; quits;
#                    <ticks> is only the mission run's limit (default 400)
#                    build: /catBuildTest <ticks> /oracleDump <run>/run.hwo (M2-R6 managers, docs/m2-r6-managers.md): the
#                    headless Mission01 run of the mission mode, writing the oracle stream: at tick 16 the local player
#                    gets a scripted sMothership (its factory) and 30,000 RUs, and three jobs are queued at it as the
#                    construction screen's build button does: the docking bay module (if it has none) and an sDestroyer,
#                    both built externally (the ship appears at health 0, clamped to the mothership, and is brought to
#                    full health by the construction manager), and an sAcolyte built inside; the checks run every tick;
#                    once every job is done the construction, research and launch screens open and draw 8 frames each
#                    (the research list must show the held technology, the launch list the docked acolyte, which Launch
#                    One then launches); "buildtest:" log lines, "buildtest: done ... result=PASS|FAIL" after the
#                    screens; quits after <ticks> ticks (two runs must give byte-identical run.hwo streams)
#                    nisparse: /catNisParseTest <run>/nis.list (M2-R6 nis-fow-camera, docs/m2-r6-nis-fow-camera.md): after the
#                    start-up, every .nis of the install (the cataclysm.big TOC: nis\*.nis) is loaded with the .script of
#                    the same name through nisLoad and the Cataclysm script table, and every animatic script
#                    (movies\*.script) through animLoadNISScript; the headers, object types, event opcodes and
#                    object references are checked, the events counted per opcode and the script keywords the exe's
#                    table does not know listed; "nisparsetest:" log lines, "nisparsetest: done ... result=PASS|FAIL";
#                    quits. (The NIS playback measurement of the motion-interpolation producer is mode nis.)
#                    motion: /catMotionTest <ticks> /catMotionDump <run>/motion.dump (M2-R6 motion-interp,
#                    docs/m2-r6-motion-interp.md; HWI-FEATURE(motion-interp) measurement): the headless Mission01 run with
#                    the virtual clock at --fps render frames a second (default 144: 9 frames a universe tick); at tick 16
#                    four scripted sProcessors appear far from Mission01's ships: A (the camera's focus, 300 u/s), B
#                    (crossing the view left to right in 6 s), C (receding along the line of sight below the centre) and D
#                    (at rest); from tick 64 to <ticks> (--motion-ticks, default 224) every rendered frame writes one dump
#                    line: the camera pose given to the main view's renderer, each ship's screen position (pixels), depth
#                    and on-screen flag as drawn, and a CRC-32 of the frame read back before the flip; "motiontest:" log
#                    lines, "motiontest: done ... result=PASS|FAIL"; quits. tools/nis_smoothness.py reads the dump
#                    nis: /catNisMotionTest <nis> /catMotionDump <run>/nis.dump (M2-R6 motion-interp): the same headless
#                    start, then at tick 16 the install's nis\<nis>.nis (--nis NAME[:FROM:TO], default n06:26:36) plays
#                    at the origin at --fps; the frames between NIS times FROM and TO are dumped (the camera pose and every
#                    NIS-driven object's screen position, as motion; with /noSpeech, since the speech subtitles follow
#                    the audio thread's wall clock); "motiontest:" lines; quits after TO
#                    toggle: /catToggleTest (M2-R6 motion-interp): after the start-up, in the front end (no input): the
#                    Smooth motion checkbox on Video_Options shows the flags at its first call and a click (its callback
#                    with the toggled state) sets both; Cancel's opRestoreSavedSettings puts them back; Accept's
#                    opOptionsSaveSettings writes Cataclysm.cfg with objInterpolate / nisInterpolate 0, then 1, each
#                    read back through utyOptionsList; the in-game Video screen carries the control; the console rows
#                    objInterpolate / nisInterpolate set each flag alone; "toggletest:" log lines, "toggletest: done ...
#                    result=PASS|FAIL"; quits. Pass "-- /noInterp" or "-- /noNISInterp" to see the switches' start flags
#                    replay: /oracleReplay <name> /oracleDump <run>/run.hwo (M2-R6 oracle-mode-a, docs/m2-r6-oracle-mode-a.md,
#                    oracle mode A): the recorded game --recording NAME (default oracle01: the user's reference recording)
#                    and NAME.pkts are copied from original/game/SavedGames/RecordedGames into the harness data dir's
#                    SavedGames/RecordedGames (where a real install keeps its recordings; refreshed when the copy differs,
#                    never written in original/game) and linked into the run's settings dir, where this engine reads saved
#                    games (/settingspath) and its relative paths (the cwd); once the start-up has put up the main screen
#                    the engine loads it as the front end's LoadRecordedGame does (VerifySaveFile, feScreenDisappear,
#                    recPackPlayInGameInit, utyLoadMultiPlayerGameGivenFilename, the recorded-game words), plays the
#                    packets on the lockstep branch with the virtual clock (one universe tick per frame, as mission), writes
#                    the stream (mode playback, 16 Hz) and quits at the end of the packets: "oracle: replay started ...",
#                    "oracle: replay tick N" every 128 packets, "oracle: replay done ticks=.. packets=.. ships=..
#                    randSyncErr=.. univSyncErr=.. blobSyncErr=.. pktSyncErr=.." and rc 0; the loader's own line
#                    "savecat32: loaded ..." counts the converted chunks. /oracleReplay turns sound and speech off
#                    (main.c OracleReplaySet): the sound stream RANDOM_SOUND_GAME_THREAD (11) is drawn when the mixer
#                    thread ends an ambience sound (SoundEvent.c soundover), on the wall clock even on SDL's dummy
#                    driver, which made rng_all differ run to run from about tick 476. Needs --timeout of several
#                    minutes under ASan
#                    ai: /catAiTest <ticks> /oracleDump <run>/run.hwo (M2-R7 ai-orders-moves, docs/m2-r7-ai-orders-moves.md):
#                    the headless Mission01 run of the mission mode, writing the oracle stream: at tick 16 the computer
#                    player (player 1, AI_ADVANCED) gets a scripted factory (its race's mothership, made its
#                    PlayerMothership) far from the level's ships, 30,000 RUs and a scripted fleet of its race handed to
#                    the AI as new ships, and fleet control is turned on (singlePlayerGameInfo.giveComputerFleetControl,
#                    as the missions' KAS call does); every tick the table of checks in src/Game/AISelfTest.c runs on the
#                    AI's own structures: teams formed (attack, guard and recon teams), attack / defence / recon orders
#                    (a move of that kind in a team's move list), moves issued (a team with ships past its get-ships
#                    move) and commands given (a team ship with a command-layer order), a build queued (a
#                    COMMAND_BUILDING_SHIP of player 1 or an external construction) and a Worker harvesting; once those
#                    hold the skirmish leg starts (M2-R7 ai-player-skirmish, docs/m2-r7-ai-player-skirmish.md): the
#                    single-player AI stops, the game becomes a one-human one-computer skirmish with research and
#                    harvesting on, every static of player 1's race is loaded, its unit cap lifted 200 above use, its
#                    Workers halted, and the test runs the skirmish AI (cat_aisPlay) on aiplayerUpdateAll's schedule;
#                    the skirmish rows hold on its first play, a new build, research started and a Worker it sent
#                    harvesting; "aitest: <check> tick=N ..." the first tick each holds, "aitest: skirmish tick=N ..."
#                    at the switch, "aitest: status" every 256 ticks, "aitest: done ... result=PASS|FAIL" at ticks-2;
#                    quits after <ticks> ticks (two runs must give byte-identical run.hwo streams)
#   --recording NAME replay mode: the recorded game to replay (default oracle01)
#   --oracle-ticks N oracle mode: universe ticks to run (default 512)
#   --mission-ticks N mission mode: universe ticks to run (default 3000)
#   --dock-ticks N   dock mode: universe ticks to run (default 5000; about 30 s under ASan on the virtual clock)
#   --command-ticks N command mode: universe ticks to run (default 3000)
#   --render-frames N render mode: frames (= universe ticks) to run (default 600)
#   --hud-frames N   hud mode: frames (= universe ticks) to run (default 600)
#   --ui-ticks N     ui mode: the mission run's tick limit (default 400; the test quits by itself at tick 48 + 8 frames)
#   --console-ticks N console mode: the mission run's tick limit (default 400; the test quits once the console is gone)
#   --build-ticks N  build mode: universe ticks to run (default 7000; the jobs end near tick 5,000)
#   --ai-ticks N     ai mode: universe ticks to run (default 400: the single-player checks hold by tick 65, the skirmish ones by 177; the skirmish AI plays to the end; docs/m2-r7-ai-player-skirmish.md)
#   --etg-ticks N    etg mode: ticks of the effect self-test (default 128; 0 = parse test only)
#   --motion-ticks N motion mode: the run's last universe tick (default 224: a 160-tick window from tick 64)
#   --nis SPEC       nis mode: NAME[:FROM:TO] (default n06:26:36: nis\n06.nis, NIS times 26 s to 36 s)
#   --fps N          motion / nis mode: the virtual frame rate (default 144)
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
# into original/game. Each run gets a fresh HOME (template: $HARNESS/home-template: .homeworld/Cataclysm.cfg, the
# options file since M2-R6 startup-keys, with TutorialNeeded 0 and PlayIntros 0; .homeworld/Homeworld.cfg, the same two
# settings for older binaries) and its own /settingspath (the options file, keys.cfg and the console's config file
# are written there), so runs do not influence each other.
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
	options_template
	echo "harness data created: $DATA"; ls -la "$DATA"
}

# M2-R6 startup-keys: the engine's options file is Cataclysm.cfg (the exe's name; GoK read Homeworld.cfg), read from
# $HOME/.homeworld (utyOptionsFileRead) and written to the run's /settingspath. The template gets both: Cataclysm.cfg
# in the exe's format (no sections) for this engine, Homeworld.cfg (GoK's sections) for binaries built before M2-R6.
# Both set only TutorialNeeded 0 and PlayIntros 0 (GoK's intro switch). Created on every run when missing, so an
# existing harness data dir needs no --force.
options_template() {
	mkdir -p "$HARNESS/home-template/.homeworld"
	[[ -f "$HARNESS/home-template/.homeworld/Cataclysm.cfg" ]] || \
		printf 'TutorialNeeded    0\nPlayIntros    0\n' > "$HARNESS/home-template/.homeworld/Cataclysm.cfg"
}

if [[ "${1:-}" == "--setup" ]]; then
	[[ "${2:-}" == "--force" ]] && setup 1 || setup 0
	exit $?
fi

label=${1:?usage: m2_harness.sh <label> [--mode sp|fe] [--timeout N] [--driver D] [--data DIR] [-- engine args]}
shift
mode=sp; tmo=60; drv=offscreen; data="$DATA"; bin="$root/build/debug/hwi-engine"; extra=()
oticks=512; perturb=""; hwo=0; etgticks=128; mticks=3000; dticks=5000; cticks=3000; rframes=600; hframes=600; uticks=400
conticks=400; bticks=7000; aiticks=400
motticks=224; nisspec=n06:26:36; vfps=144; recording=oracle01
while [[ $# -gt 0 ]]; do
	case "$1" in
		--mode) mode=$2; shift ;;
		--timeout) tmo=$2; shift ;;
		--driver) drv=$2; shift ;;
		--data) data=$2; shift ;;
		--bin) bin=$(realpath -- "$2"); shift ;;
		--oracle-ticks) oticks=$2; shift ;;
		--mission-ticks) mticks=$2; shift ;;
		--dock-ticks) dticks=$2; shift ;;
		--command-ticks) cticks=$2; shift ;;
		--render-frames) rframes=$2; shift ;;
		--hud-frames) hframes=$2; shift ;;
		--ui-ticks) uticks=$2; shift ;;
		--console-ticks) conticks=$2; shift ;;
		--build-ticks) bticks=$2; shift ;;
		--ai-ticks) aiticks=$2; shift ;;
		--etg-ticks) etgticks=$2; shift ;;
		--motion-ticks) motticks=$2; shift ;;
		--nis) nisspec=$2; shift ;;
		--fps) vfps=$2; shift ;;
		--recording) recording=$2; shift ;;
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
options_template
cp -r "$HARNESS/home-template" "$out/home"

args=(/window /nodebugInt /logFileLoads /settingspath "$out/settings")
# a release binary (tools/build.sh --release, the build oracle runs use) has no /logFileLoads switch and rejects it
grep -q -a "/logFileLoads" "$bin" || args=(/window /nodebugInt /settingspath "$out/settings")
[[ "$mode" == sp ]] && args+=(/debugScreenshots)
[[ "$mode" == statics ]] && args+=(/catShipStaticsTest)
[[ "$mode" == textures ]] && args+=(/catTexturesTest)
[[ "$mode" == sound ]] && args+=(/catSoundTest)
[[ "$mode" == strings ]] && args+=(/catStringsTest)
[[ "$mode" == keys ]] && args+=(/catKeysTest)
[[ "$mode" == toggle ]] && args+=(/catToggleTest)
if [[ "$mode" == oracle ]]; then
	args+=(/oracleSelfTest "$oticks" /oracleDump "$out/run.hwo")
	[[ -n "$perturb" ]] && args+=(/oracleSelfTestPerturb "$perturb")
elif [[ "$mode" == mission ]]; then
	args+=(/oracleMission "$mticks" /oracleDump "$out/run.hwo")
	[[ -n "$perturb" ]] && args+=(/oracleSelfTestPerturb "$perturb")
elif [[ "$mode" == dock ]]; then
	args+=(/catDockTest "$dticks")
elif [[ "$mode" == command ]]; then
	args+=(/catCommandTest "$cticks")
elif [[ "$mode" == render ]]; then
	args+=(/catRenderTest "$rframes")
elif [[ "$mode" == hud ]]; then
	args+=(/catHudTest "$hframes")
elif [[ "$mode" == ui ]]; then
	args+=(/catUiTest "$uticks")
elif [[ "$mode" == console ]]; then
	args+=(/catConsoleTest "$conticks")
elif [[ "$mode" == build ]]; then
	args+=(/catBuildTest "$bticks" /oracleDump "$out/run.hwo")
elif [[ "$mode" == ai ]]; then
	args+=(/catAiTest "$aiticks" /oracleDump "$out/run.hwo")
elif [[ "$mode" == motion ]]; then
	args+=(/catMotionFps "$vfps" /catMotionTest "$motticks" /catMotionDump "$out/motion.dump")
elif [[ "$mode" == replay ]]; then
	# M2-R6 oracle-mode-a: the recording into the data dir's SavedGames/RecordedGames (as a real install keeps it),
	# then linked into the run's settings dir, where the engine looks (saved games: /settingspath; the packets: the cwd)
	src="$GAME/SavedGames/RecordedGames"
	[[ -f "$src/$recording" && -f "$src/$recording.pkts" ]] || { echo "m2_harness: no recording $src/$recording(.pkts)" >&2; exit 1; }
	mkdir -p "$DATA/SavedGames/RecordedGames" "$out/settings/SavedGames/RecordedGames"
	for f in "$recording" "$recording.pkts"; do
		cmp -s "$src/$f" "$DATA/SavedGames/RecordedGames/$f" || cp -f -- "$src/$f" "$DATA/SavedGames/RecordedGames/$f"
		ln -sf -- "$DATA/SavedGames/RecordedGames/$f" "$out/settings/SavedGames/RecordedGames/$f"
	done
	args+=(/oracleReplay "$recording" /oracleDump "$out/run.hwo")
elif [[ "$mode" == nis ]]; then
	# /noSpeech: the NIS's speech subtitles start and end with the audio thread's progress through the speech (wall
	# clock, even on SDL's dummy driver), which made about 40 frame CRCs of n06's window differ run to run
	args+=(/noSpeech /catMotionFps "$vfps" /catNisMotionTest "$nisspec" /catMotionDump "$out/nis.dump")
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
if [[ "$mode" == fescreens ]]; then
	# every .fib of both archives' TOCs, as "archive<TAB>name" (the engine loads each from that archive); an
	# /ignoreBigfiles data tree has no archives, so this mode needs the harness data dir
	for arc in Cataclysm.big Update.big; do
		f="$data/$(echo "$arc" | tr 'A-Z' 'a-z')"
		[[ -e "$f" ]] || continue
		python3 "$root/tools/bigfile.py" list "$f" | awk -F'\t' -v a="$arc" 'NR > 1 && tolower($5) ~ /\.fib$/ {print a "\t" $5}'
	done | sort -t$'\t' -k2,2 -k1,1 > "$out/fescreens.list"
	args+=(/catFeScreensTest "$out/fescreens.list")
fi
if [[ "$mode" == nisparse ]]; then
	# every nis\*.nis and nis\*.script and every movies\*.script of the install (the cataclysm.big TOC; update.big has
	# none), or of an /ignoreBigfiles data tree; the engine pairs each .nis with the .script of the same name
	if [[ -e "$data/cataclysm.big" ]]; then
		python3 "$root/tools/bigfile.py" list "$data/cataclysm.big" | awk -F'\t' 'NR > 1 {print $5}' \
			| grep -i -E '^(nis\\[^\\]*\.(nis|script)|movies\\[^\\]*\.script)$' | sort > "$out/nis.list"
	else
		(cd "$data" && find -L . -maxdepth 2 -type f \( -ipath './nis/*.nis' -o -ipath './nis/*.script' -o -ipath './movies/*.script' \)) \
			| sed 's|^\./||; s|/|\\|g' | sort > "$out/nis.list"
	fi
	args+=(/catNisParseTest "$out/nis.list")
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
grep -q "oracle: replay started" "$L" && milestone="replay loaded"
grep -q "oracle: replay tick" "$L" && milestone="replay running"
grep -q "oracle: replay done" "$L" && milestone="oracle replay done"
grep -q "etgtest: self-test done" "$L" && milestone="etg parse self-test done"
grep -q "etgtest: effect self-test done" "$L" && milestone="etg effect self-test done"
grep -q "docktest: done" "$L" && milestone="dock self-test done"
grep -q "commandtest: done" "$L" && milestone="command self-test done"
grep -q "rendertest: done" "$L" && milestone="render self-test done"
grep -q "soundtest: done" "$L" && milestone="sound self-test done"
grep -q "hudtest: done" "$L" && milestone="hud self-test done"
grep -q "uitest: done" "$L" && milestone="ui self-test done"
grep -q "optionstest: done" "$L" && milestone="options round trip done"
grep -q "fescreentest: done" "$L" && milestone="fescreens self-test done"
grep -q "stringstest: done" "$L" && milestone="strings self-test done"
grep -q "keystest: done" "$L" && milestone="keys self-test done"
grep -q "consoletest: done" "$L" && milestone="console self-test done"
grep -q "buildtest: done" "$L" && milestone="build self-test done"
grep -q "aitest: done" "$L" && milestone="ai self-test done"
grep -q "nisparsetest: done" "$L" && milestone="nisparse self-test done"
grep -q "motiontest: done" "$L" && milestone="motion self-test done"
grep -q "toggletest: done" "$L" && milestone="toggle self-test done"

# first stop: the first fatal error / failed assertion / ASan report. UBSan runtime errors do not stop the engine; the
# first one is reported separately (the misaligned-access reports on the packed front-end structs, GoK 64-bit artefacts
# docs/m2-r1-data-deltas.md 5.4, are fixed since M2-R6 frontend and count again, as the sound-bank 'struct PATCH' ones
# since M2-R5 sound)
# [M2-R3 etg-particles: an ASan report can land in the ubsan.* log (UBSan's log_path wins when both sanitizers write,
# the same as the LeakSanitizer report below); the stop and the ASan count read both logs]
stop=$(grep -h -E "Fatal error|Fatal Error|ERROR: AddressSanitizer|AddressSanitizer:DEADLYSIGNAL|Unable to open file|SUMMARY: AddressSanitizer: [a-z-]+ |Assertion of" "$L" "$out"/asan.* "$out"/ubsan.* 2>/dev/null | grep -v "SUMMARY: AddressSanitizer: .* leaked" | head -n 1)
ubsan1=$(grep -h "runtime error" "$L" "$out"/ubsan.* 2>/dev/null | head -n 1)
fescreen_n=$(cat "$L" "$out"/ubsan.* 2>/dev/null | grep "runtime error" | grep -c -E "type '(struct fescreen|struct featom|struct fibfileheader)'")
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
	echo "fescreen UBSan reports (struct fescreen / featom / fibfileheader): $fescreen_n"
	echo "file loads: $nload; ASan errors: $asan_n; LeakSanitizer at exit: ${leak_sum:-no unsuppressed leak}"
	echo "ship statics: $(grep -c 'shipstatic: loaded' "$L") loaded, $(grep -c 'shipstatic: .* not found' "$L") not found (disabled), $(grep -c 'shipstatic: other' "$L") other .shp; $(grep -h 'shipstatic: self-test done' "$L" | sed 's/^.*shipstatic: //' | head -n 1)"
	[[ "$mode" == meshes ]] && echo "meshes: $(grep -c . "$out/meshes.list" 2>/dev/null) listed, $(grep -c 'meshtest: loaded' "$L") .geo/.peo loaded, $(grep -c 'meshtest: mad loaded' "$L") .mad loaded, $(grep 'meshtest: .*loaded' "$L" | grep -c -v ' bad=0$') with bad=; $(grep -h 'meshtest: self-test done' "$L" | sed 's/^.*meshtest: //' | head -n 1)"
	[[ -f "$out/run.hwo" ]] && echo "oracle: $(grep -h 'oracle: self-test done\|oracle: mission run done\|oracle: replay done' "$L" | sed 's/^.*oracle: //' | head -n 1); run.hwo $(stat -c %s "$out/run.hwo") bytes, sha256 $(sha256sum "$out/run.hwo" | cut -c1-16)"
	[[ "$mode" == replay ]] && echo "replay: $(grep -h 'oracle: replay started\|savecat32: loaded\|oracle: replay load failed' "$L" | sed 's/^.*\(oracle\|savecat32\): //' | paste -sd';'); last tick line: $(grep -h 'oracle: replay tick' "$L" | tail -n 1 | sed 's/^.*oracle: //')"
	[[ "$mode" == mission ]] && echo "mission: last tick line: $(grep -h 'oracle: mission tick' "$L" | tail -n 1 | sed 's/^.*oracle: //'); ships loaded $(grep -c 'shipstatic: loaded' "$L")"
	[[ "$mode" == textures ]] && echo "textures: $(grep -h 'texturetest: listing lookups\|texturetest: self-test done' "$L" | sed 's/^.*texturetest: //' | paste -sd';')"
	[[ "$mode" == dock ]] && echo "docktest: $(grep -h 'docktest: orders\|docktest: clDock\|docktest: A inside\|docktest: done' "$L" | sed 's/^.*docktest: //' | paste -sd';'); FAIL lines: $(grep -c 'docktest: FAIL' "$L")"
	[[ "$mode" == command ]] && echo "commandtest: $(grep -h 'commandtest: link ordered\|commandtest: point ordered\|commandtest: link done\|commandtest: unlink done\|commandtest: done' "$L" | sed 's/^.*commandtest: //' | paste -sd';'); FAIL lines: $(grep -c 'commandtest: FAIL' "$L")"
	[[ "$mode" == render ]] && echo "rendertest: $(grep -h 'rendertest: counts\|rendertest: done' "$L" | sed 's/^.*rendertest: //' | paste -sd';')"
	[[ "$mode" == hud ]] && echo "hudtest: $(grep -h 'hudtest: selected\|hudtest: counts\|hudtest: sensors\|hudtest: done' "$L" | sed 's/^.*hudtest: //' | paste -sd';')"
	[[ "$mode" == ui ]] && echo "uitest: $(grep -h 'uitest: ' "$L" | sed 's/^.*uitest: //' | paste -sd';')"
	[[ "$mode" == fescreens ]] && echo "fescreentest: $(grep -c . "$out/fescreens.list" 2>/dev/null) files listed; $(grep -h 'fescreentest: start\|fescreentest: done' "$L" | sed 's/^.*fescreentest: //' | paste -sd';'); FAIL lines: $(grep -c 'fescreentest: FAIL' "$L")"
	[[ "$mode" == fescreens ]] && echo "optionstest: $(grep -h 'optionstest: ' "$L" | sed 's/^.*optionstest: //' | paste -sd';')"
	[[ "$mode" == strings ]] && echo "stringstest: $(grep -h 'stringstest: ' "$L" | grep -v 'stringstest: FAIL' | sed 's/^.*stringstest: //' | paste -sd';'); FAIL lines: $(grep -c 'stringstest: FAIL' "$L")"
	[[ "$mode" == console ]] && echo "consoletest: $(grep -h 'consoletest: ' "$L" | grep -v 'consoletest: FAIL' | sed 's/^.*consoletest: //' | paste -sd';'); FAIL lines: $(grep -c 'consoletest: FAIL' "$L")"
	[[ "$mode" == build ]] && echo "buildtest: $(grep -h 'buildtest: orders\|buildtest: queued\|buildtest: appeared\|buildtest: attached\|buildtest: finished\|buildtest: job\|buildtest: done' "$L" | sed 's/^.*buildtest: //' | paste -sd';'); FAIL lines: $(grep -c 'buildtest: FAIL' "$L")"
	[[ "$mode" == ai ]] && echo "aitest: $(grep -h 'aitest: ' "$L" | grep -v 'aitest: status\|aitest: FAIL\|aitest: loaded' | sed 's/^.*aitest: //' | paste -sd';'); FAIL lines: $(grep -c 'aitest: FAIL' "$L")"
	[[ "$mode" == nisparse ]] && echo "nisparsetest: $(grep -c . "$out/nis.list" 2>/dev/null) files listed; $(grep -h 'nisparsetest: done' "$L" | sed 's/^.*nisparsetest: //' | head -n 1); FAIL lines: $(grep -c 'nisparsetest: FAIL' "$L"); ignored keywords: $(grep -h 'nisparsetest: keyword' "$L" | sed "s/^.*: '//; s/' / /" | paste -sd',')"
	[[ "$mode" == motion || "$mode" == nis ]] && echo "motiontest: $(grep -h 'motiontest: ' "$L" | sed 's/^.*motiontest: //' | paste -sd';'); dump $(ls "$out"/*.dump 2>/dev/null | head -n 1 | xargs -r -I{} sh -c 'echo "$(basename {}) $(grep -c "^F " {}) frames, sha256 $(sha256sum {} | cut -c1-16)"')"
	[[ "$mode" == toggle ]] && echo "toggletest: $(grep -h 'toggletest: ' "$L" | grep -v 'toggletest: FAIL' | sed 's/^.*toggletest: //' | paste -sd';'); FAIL lines: $(grep -c 'toggletest: FAIL' "$L")"
	[[ "$mode" == keys ]] && echo "keystest: $(grep -h 'keystest: ' "$L" | grep -v 'keystest: FAIL' | sed 's/^.*keystest: //' | paste -sd';'); FAIL lines: $(grep -c 'keystest: FAIL' "$L")"
	[[ "$mode" == sound ]] && echo "soundtest: $(grep -h 'soundtest: ' "$L" | sed 's/^.*soundtest: //' | paste -sd';'); UBSan reports in fquant.c/fqcodec.c: $(cat "$L" "$out"/ubsan.* 2>/dev/null | grep 'runtime error' | grep -c 'fquant\.c\|fqcodec\.c'); PATCH reports: $(cat "$L" "$out"/ubsan.* 2>/dev/null | grep 'runtime error' | grep -c "struct PATCH")"
	[[ "$mode" == etg ]] && echo "etg: $(grep -c . "$out/etg.list" 2>/dev/null) listed; $(grep -h 'etgtest: etgStartup done\|etgtest: self-test done\|etgtest: created\|etgtest: effect self-test done' "$L" | sed 's/^.*etgtest: //' | paste -sd';')"
	echo "engine glows:$(grep -c -i 'engineglows' "$out/FileLoads.log" 2>/dev/null) mesh/mex loads from misc\\EngineGlows in FileLoads.log"
	echo "UBSan report kinds (count, site):"; echo "$ubsan_kinds"
	echo "log tail:"; grep -v '^    #\|^ *$' "$L" | tail -n 8
} | tee "$out/summary.txt"
echo "run=$label mode=$mode driver=$drv rc=$rc wall=${wall}s data=$data milestone=\"$milestone\" stop=\"$stop\"" >> "$HARNESS/runs/runs.txt"
