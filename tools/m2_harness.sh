#!/usr/bin/env bash
# m2_harness.sh - M2-R2 test harness: run build/debug/hwi-engine headless against an UNMODIFIED Cataclysm install.
#
# Usage:
#   tools/m2_harness.sh --setup                 (re)create the harness data dir if missing (--force: rebuild it)
#   tools/m2_harness.sh <label> [options] [-- extra engine args]
# Options:
#   --mode sp|fe|statics|meshes|textures|oracle|etg|mission|dock|command|render|sound|hud|ui|fescreens|strings|keys|console|build|nisparse|motion|nis|toggle|uiscale|modequit|resize|replay|ai|skirmish|missiontest|loadgame|warpstart
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
#                    through the Cataclysm scene (cat_sm*: "hudtest: sensors" counts); M2-R7 r6-leftovers: 60 frames
#                    before those the waypoint mode starts (cat_mrWaypointsStart) with three waypoints, so the
#                    waypoint cursor 0x004e6e70 draws to the end ("hudtest: waypoints" draws and stream-0 draws, which
#                    must be 63 + 3 a draw, part of PASS); at the end "hudtest: counts"
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
#                    M3-R3 pause-freeze (HWI-TEST, docs/m3-r3-pause-freeze.md): motion, nis and --scene runs take
#                    "-- /catMotionPause <frame>:<frames>[:key|univ|nis]": after dump frame <frame> the game is paused
#                    (key: the P key as an SDL key event to HandleEvent, the exe's frame-step flag; univ: universePause,
#                    as the console / Escape menu; nis: nisPause) and <frames> dump frames later released; the dump
#                    lines then end with " pause=<universePause><P flag><nisPaused> crcv=<the CRC with the HUD's blinking
#                    SU value blanked>" ("motiontest: pause on|off" log lines); "-- /catMotionShot F1,F2,.." saves up
#                    to 9 frames
#                    toggle: /catToggleTest (M2-R6 motion-interp): after the start-up, in the front end (no input): the
#                    Smooth motion checkbox on Video_Options shows the flags at its first call and a click (its callback
#                    with the toggled state) sets both; Cancel's opRestoreSavedSettings puts them back; Accept's
#                    opOptionsSaveSettings writes Cataclysm.cfg with objInterpolate / nisInterpolate 0, then 1, each
#                    read back through utyOptionsList; the in-game Video screen carries the control; the console rows
#                    objInterpolate / nisInterpolate set each flag alone; "toggletest:" log lines, "toggletest: done ...
#                    result=PASS|FAIL"; quits. Pass "-- /noInterp" or "-- /noNISInterp" to see the switches' start flags
#                    (M2-R7 r6-leftovers: then the switch is checked run-only: the saves write the options file's
#                    values, not the switch's, and Cancel restores the override; "toggletest: run-only" lines)
#                    M3-R1 letterbox-front adds the letterboxBehind checks ("toggletest: letterbox ..."); M3-R1 full-detail
#                    adds FULL DETAIL's ("toggletest: fulldetail ..."): noLOD 1 by default (the start-up's save wrote the
#                    file's value), the Video_Options FULL DETAIL checkbox (OP_NoLOD) shows it and its clicks set 0 / 1,
#                    Accept writes and reads back noLOD 0 / 1, the console row noLOD, the frame-rate presets
#                    (cat_opSetAllDefaults: low 0, high 1), the switch run-only; pass "-- /lodBands" to check the switch
#                    uiscale: /catUiScaleTest (M3-R1 ui-scale, HWI-FEATURE(ui-scale), docs/m3-r1-ui-scale.md; src/Game/UiScaleTest.c):
#                    once the front end is up at Main_game_screen, with no OS input: the UI scale chosen for the window
#                    (auto: 200 at a physical height of 2160 or more, 150 at 1440 or more, 100 below; or the uiScale row /
#                    the /uiScale switch) and the logical size (physical / scale, rounded); the windowed mode's fit rule
#                    over a table of desktops (the user's 3840x2160 request against 3840x1960 usable bounds: fullscreen
#                    desktop; "window: fit .." lines); the mouse at the physical bottom row, the bottom-right corner and
#                    outside the window, handed to mouse.c's poll as SDL reports a window position (the test hook replaces
#                    SDL_GetMouseState), must land on the logical bottom row / corner, a region reaching the bottom edge
#                    under it; a click (SDL_MOUSEBUTTONDOWN / UP events handed to main.c HandleEvent, the regions processed
#                    as regProcessTask does) at the physical centre of Main_game_screen's OPTIONS button must open the
#                    options, one just outside it nothing; the Video screens' UI SCALE buttons (a scripted click on 150%,
#                    Cancel; 200%, Accept's order: applied at once, the main menu re-made at the new logical size, the
#                    saved screenWidth / Height the request); the options file's uiScale row read back with the window's
#                    logical size kept, /uiScale run-only, the console row; "uiscaletest:" lines, "uiscaletest: done ...
#                    result=PASS|FAIL"; quits. Pass --res WxH for the size, --cfg 'uiScale N' or "-- /uiScale N" for an
#                    override. Every mode's summary prints the engine's "window: requested .." and "uiscale: physical .."
#                    lines; "-- /testUsableBounds WxH[@DWxDH]" (HWI-TEST) forces the desktop's usable bounds (and display
#                    size) the fit uses for the run's own window (the offscreen driver has none; without it no fit)
#                    M3-R1 motion-jitter adds "toggletest: screen untouched / screen cancel" (the screens' own orders: the
#                    Smooth motion box keeps OP_NoLOD's FAF_CallOnDelete, so its callback runs at the screen's deletion;
#                    Cancel restores before the deletion, Accept saves after it): the flags, a switch's run-only state and
#                    the options file's rows must be as before the screen
#                    modequit: /catModeQuitTest <--mode-to WxH> (M3-R2 instrumentation, HWI-TEST, docs/m3-r2-instrumentation.md;
#                    src/Game/UiScaleTest.c): once the front end is up, the Video screen's render switch to that
#                    resolution (Options.c opOptionsAcceptHelper's order: soundEventShutdown, mainSaveRender, the new
#                    request, mainLoadGL, soundEventRestart; a real second window), then ONE quit (utyGameQuit);
#                    "modequittest:" lines; the summary's "modequit:" line judges the quit: rc 0 and "Quit game, baby!" once in
#                    the log (the debug build prints it) = PASS (M3-R1's finding: GoK's mainDestroyWindow cleared
#                    mainActuallyQuit, so that quit was swallowed and the run ended rc 1 without the shutdown)
#                    resize: /catResizeTest (M3-R4 window-modes, HWI-FEATURE(resolution-presets) / (window-resize),
#                    docs/m3-r4-window-modes.md; src/Game/UiScaleTest.c): once the front end is up: the Video screen's
#                    resolution list (rinit.c rinEnumeratePrimary: the 4:3 / 16:10 / 16:9 / 5:4 presets that fit the
#                    display, the display's own mode and the window's request, ascending, no duplicates, the aspect
#                    names), then scripted size changes (uiscale.c uiScaleWindowResizedTo: the drag path less SDL's
#                    event, the offscreen driver's SDL_SetWindowSize changes nothing) to 1600x900, 800x600, 1280x1024,
#                    2560x1440, 3840x2160 and 1234x777: the physical size, the scale chosen for the height, the logical
#                    size, the main region and the main menu's layout, the 3D aspect, the request, the mouse corner; the
#                    Video screen lists and selects the dragged size with no mode switch pending; the options file's
#                    screenWidth / screenHeight rows are the dragged size; then the start size again. "resizetest:"
#                    lines, "resizetest: done ... result=PASS|FAIL"; quits. --res WxH for the start size.
#                    --resize-at <tick>:<W>x<H> (any mode with a universe, HWI-TEST /catResizeAt): at that universe tick the
#                    window is treated as resized to WxH (the in-game path: the relayout waits while a screen is open;
#                    "resizetest: at tick .." and the engine's "window: resized .." lines in the summary). A --shot after
#                    the tick reads back the REAL drawable, which the offscreen driver never changes: a change DOWN from a
#                    larger start (--res 3840x2160 --resize-at T:1280x720) shows the new layout in the buffer's
#                    bottom-left corner, a change UP is clipped to the real buffer
#                    replay: /oracleReplay <name> /oracleDump <run>/run.hwo (M2-R6 oracle-mode-a, docs/m2-r6-oracle-mode-a.md,
#                    oracle mode A): the recorded game --recording NAME (default oracle01: the user's reference recording)
#                    and NAME.pkts are copied from original/game/SavedGames/RecordedGames into the harness data dir's
#                    SavedGames/RecordedGames (where a real install keeps its recordings; refreshed when the copy differs,
#                    never written in original/game) and linked into the run's settings dir, where this engine reads saved
#                    games (/settingspath; M2-R9 recorder-leftovers: the packets too, NetCheck.c opens them there) and its
#                    relative paths (the cwd); once the start-up has put up the main screen
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
#                    The permanent per-ship / per-packet dump (M2-R8 oracle-rung-physics,
#                    docs/m2-r8-oracle-rung-physics.md "dump"): pass "-- /oracleShipDump <ships>[@<from>-<to>]" to any mode
#                    that writes run.hwo (replay, mission, ai, build, skirmish, --hwo); <ships> is a ship number (the
#                    shipID the stream hashes), a comma-separated list or "all"; <run>/shipdump.txt gets, per universe
#                    tick, one 'S' line per dumped ship (position, velocity, rotation speed, coordsys, health as float
#                    bits and values, order, attributes, AI state, flags), 'A' / 'F' lines (each force / torque given and
#                    the sums the physics integrates), 'V' lines (the flight's target, avoidance terms and desired
#                    velocity) and one 'C' line per command of every sync packet; the stream is unchanged (read only).
#                    M2-R9 oracle-rung-physics adds: 'V <tick> <ship> fly' (the flight's destination, AISHIP_ flags,
#                    limitvel and the velocity it flies with), 'O' (each object the flight avoids: its type and ship
#                    number, collision position and coordsys, the distance, the normal and the three doubles of the
#                    collision distance as raw words) and, with "all", 'B' (every live bullet in BulletList order: the
#                    firing ship, its target, position, velocity)
#                    M2-R10 oracle-rung-physics adds, per dumped ship: 'T <tick> <ship> state=.. target=.. cmd=..
#                    [multi=..] madGun=..' (its attack state aistateattack, its attack target, the command's attack
#                    targets and, for a ship with multiple targets, each gun's target; a ship number, o<objtype> or -)
#                    and one 'G <tick> <ship> <gun> type=.. burst=.. last=.. heading=..' per gun (the gun type, its
#                    burst state, lasttimefired and heading in ship space); read only, the stream is unchanged
#                    tools/oracle/hwo_invert.py recovers a reference stream's per-ship words from an "all" dump
#                    The square root (M2-R9 oracle-rung-physics, decision 70): the engine's fsqrt / fmathSqrt / catFsqrtD
#                    are the exe's table square root (fsqrt 0x00472860, src/Game/FastMath.c) by default; pass
#                    "-- /oracleLibmSqrt" (HWI-EXPERIMENT) to any mode for libm's sqrtf instead, the A/B run of the rung
#                    (docs/m2-r9-oracle-rung-physics.md). The old switch /oracleTableFsqrt (M2-R7 / R8) is gone
#                    The animatics (M2-R10 render-animatics-sp, docs/m2-r10-render-animatics-sp.md): every caller of the
#                    exe's animBinkPlay 0x00421860 reaches animAviPlay (Animatic.c, its port): singlePlayerPostInit
#                    (Animatic01 before Mission01), singlePlayerGameUpdate's white-out (AnimaticNN+1 between missions N
#                    and N+1), kasfMissionCompleted (Mission17's end: Animatic18) and utyVideoPlay (no .fib binds it). A
#                    headless run plays none: every mode on the virtual clock (mission, missiontest, warpstart, loadgame,
#                    replay, ai, skirmish, build, oracle, ... : univRealTimeVirtual) and sp's /debugScreenshots loop skip
#                    the real-time playback after the exe's own time-compression reset and tutorial test, and log one
#                    line per animatic the engine would play: "animatic: skipped (headless) a=<from level> b=<to level>
#                    file=<AnimaticNN>"; the summary's "animatics:" line counts and lists them. An attended run
#                    (tools/play.sh) plays them (its log: "animatic: play a=.. b=.. file=Movies/AnimaticNN.bik")
#   --record NAME    skirmish mode (M2-R9 recorder-leftovers; any mode that plays a skirmish / multiplayer-type game):
#                    /oracleRecord NAME: the mode-B oracle's record side on this engine. At the first universe tick the
#                    game's own in-game recording starts (recPackInGameStartCB, as the GOG DLL's record mode calls it):
#                    universeUpdateTask saves the game (cataclysm.exe's save format, version 0x10ad0027) as
#                    <run>/settings/SavedGames/RecordedGames/NAME and from then on runs the lockstep with fake sends,
#                    appending every sync packet to NAME.pkts there; the stream gets the record-start / record-stop
#                    events; "oracle: record requested ..." and "oracle: record stopped ..." lines. Nothing is written
#                    in original/game or the harness data dir. Replay it with --mode replay --recording NAME
#                    --recording-src <run>/settings/SavedGames/RecordedGames --recording-as NAME2
#   --record-stop N  with --record: the universe tick the recording stops at (/oracleRecordStop; default 9600, the DLL's)
#   --recording NAME replay mode: the recorded game to replay (default oracle01)
#   --recording-src DIR replay mode: the directory NAME and NAME.pkts are copied from (default the install's
#                    SavedGames/RecordedGames). M2-R7 integrate: an archived recording (original/oracle/<session>/, where
#                    tools/oracle/run_oracle.sh keeps each GOG session's copy next to its .hwo streams) can be replayed
#                    after the install's copy was re-recorded, e.g. --recording oracle01 --recording-src
#                    original/oracle/oracle01-rec-20261017-140428 --recording-as oracle01-r6; the archive is only read
#   --recording-as NAME2 replay mode: the name the pair is copied to in the harness data dir and replayed under
#                    (default NAME), so two recordings of the same name can coexist there
#                    skirmish: /catSkirmishMap <map> /catSkirmishTest <ticks> /oracleDump <run>/run.hwo (skirmish-start,
#                    docs/skirmish-start-fix.md): once the start-up has put up the main screen, a 2-player skirmish is
#                    started through the front end's own button callbacks with no input (FE_Multiplayer_Game, MG_Skirmish:
#                    one computer opponent, the map --skirmish-map picked as the scenario picker's OK does, with its
#                    description's game type, the Sect as the colour screen's race, StartNewGame: gameStart and the
#                    level loader's MissionSphere readers); the universe then runs on the mission mode's virtual clock
#                    (one tick per frame), writing the oracle stream; at tick <ticks> the universe is judged (2 players,
#                    player 0 human and player 1 computer, both with a mothership, player 0's an sMothership and the
#                    Sect, the resource sphere's asteroids, every player alive, no ship of a player the map has a sphere
#                    for but the game does not have): "skirmishtest:" log lines, "skirmishtest: done ticks=N ships=N
#                    result=PASS|FAIL", then the mission run's quit (rc 0; two runs of one map must give byte-identical
#                    run.hwo streams). About 20 s under ASan for 600 ticks. The default map is not in the front end's
#                    list (multiPlayerMissions.script comments it out; the test adds its line); Armed8 is a listed one
#                    missiontest: /catMissionTest <ticks> /oracleDump <run>/run.hwo (M2-R8 kas-runtime-mission01,
#                    docs/m2-r8-kas-runtime-mission01.md "The mission self-test"): a new single-player game started as
#                    sp starts it, on the mission mode's virtual clock (one universe tick a frame) but with the exe's
#                    NIS-first start and Mission01's generated kas2c script, no sound and no speech; the opening NIS
#                    plays in its staging level, the fleet hyperspaces into Mission01, the KAS FSMs run, and the test
#                    plays the player's part with scripted events (commands through the command layer, no input);
#                    "missiontest: <objective> tick=N" per objective of Mission01's map (docs/mission-code-map.md 8),
#                    "missiontest: fsm ..." per KAS state change, "missiontest: done mission=1 ticks=N objectives=N/M
#                    result=PASS|FAIL" [audit: M2-R9 missions-02-09: the line gained mission=1], then the mission run's quit (rc 0; two runs must give byte-identical run.hwo
#                    streams). With --tutorial: /catMissionTestTutorial (Tutorial1 starts and its first prompt shows;
#                    M2-R8 singleplayer-tutorial: its Next / previous / quit buttons come up, its first three prompts
#                    show (each a new subtitle text, the third with the objective list's pointer), then the Next
#                    button is clicked through its region's own click callback (no input) and the Tutorial FSM must
#                    reach Lesson2: "missiontest: tutorial ..." lines)
#                    loadgame: /catLoadGameTest <ticks> /catLoadGameName <name> /oracleDump <run>/run.hwo (M2-R8
#                    singleplayer-tutorial, docs/m2-r8-singleplayer-tutorial.md): the single-player save --save NAME
#                    (default "01 - Hiigara", the user's) is copied from original/game/SavedGames/SinglePlayer into the
#                    run's settings dir's SavedGames/SinglePlayer (where the engine reads saved games; never into the
#                    data dir, whose SavedGames stay empty, nor written in original/game); once the start-up has put
#                    up the main screen the engine loads it as the front end's Load Game does (gpLoadGivenGame: the
#                    mode-A loader converts cataclysm.exe's chunks, "savecat32: loaded ..."), runs --load-ticks N
#                    universe ticks (default 400) on the mission run's virtual clock and quits: "loadgametest: loaded
#                    ...", "loadgametest: status ...", "loadgametest: done ... result=PASS|FAIL", rc 0. No sound, no
#                    speech
#   --mission-test-ticks N missiontest mode: the run's tick limit (default 20000)
#   --fail-jump      missiontest mode with --mission N (M2-R10 tests-leftovers): /catMissionTestFailJump, the deliberate-loss
#                    test of the failed jump. At the test's first hyperspace command the KAS function DisablePlayerHyperspace
#                    runs through the developer console's KAS call (cat_kasCallFunctionByName 0x0045b9a0: in cataclysm.exe
#                    the only route to singlePlayerGameInfo.hyperspaceFails; no mission script calls it), the jump fizzles
#                    and returns through singlePlayerGameUpdate's case HYPERSPACE_CATFAILEDRETURN (the exe's 10); at the
#                    return the test judges the state (no hyperspace, hyperspaceFails clear, the same level, the
#                    mothership outside, none of the player's ships hyperspacing or in the ether, none lost, docked ones
#                    counted): "missiontest: failed jump armed / begun / return ... ok|FAIL" lines; the run ends 32 ticks
#                    after the return (the exe's case 10 does not give playerCanHyperspace back, so there is no second
#                    jump): "missiontest: failed jump judged ... PASS|FAIL", and the done line's result also needs every
#                    objective reached as in the normal run and no next level. Needs a roll-call jump: not missions 2,
#                    5, 7, 15 (letterbox jumps ignore hyperspaceFails) nor 17 (no jump); the gate runs use --mission 3
#   --tutorial       missiontest mode: Tutorial1's first screens instead of Mission01
#                    (M2-R9 recorder-leftovers: the tutorial run's speech and mixer run on the virtual audio clock,
#                    /catVirtualAudio set by /catMissionTestTutorial: the speech's subtitles start and end at the same frame in
#                    every run, so two runs give byte-identical run.hwo streams)
#   --mission N      missiontest mode (M2-R9 missions-02-09): mission N (2 .. 17) instead of Mission01: /catMissionTest
#                    <ticks> /catMissionTestMission N. The mission starts through the exe's warp start (cat_spWarpToLevel
#                    0x0051b9c0 with Warp.script's level N record: the fleet, resources and technologies Relic set for
#                    it; HWI-TEST: WarpEnabled 1 / LevelToWarpTo N as a test copy of Warp.script would say), its KAS
#                    script runs, the test plays the player's part for each objective of the mission's table
#                    (MissionSelfTest.c catMtMissions, the order its Objectives FSM creates them), presses the
#                    hyperspace button, clicks the roll call's enabled button (Go Now / Emergency Hyperspace) if the
#                    jump has one, and ends 32 ticks after the next level (or its opening NIS's staging level) has
#                    loaded: "missiontest: next level ...", "missiontest: done mission=N ticks=N objectives=N/M
#                    result=PASS|FAIL" (two runs must give byte-identical run.hwo streams). Without --mission the
#                    run is Mission01's, unchanged
#                    M2-R9 missions-10-17: missions 10 .. 17 too (Mission17's run ends with the game: its script's
#                    MissionCompleted, the quit and the credits, "missiontest: game end ..."; no next level); a lost
#                    mission ends the run at once (result=FAIL). Pass "-- /catMissionTestTrace" for a diagnostic
#                    "missiontest: msg" line per new KAS message (not for gate runs)
#                    M3-R3 mission02 (HWI-TEST, docs/m3-r3-mission02.md): every mission run logs "speech: event=0x..
#                    tick=N" per KAS speech event and, after the first mission, "missiontest: resources ..." (the
#                    level's asteroids by type, meteors, the decorative Asteroid0 of the minor list, dust / gas clouds,
#                    nebulae), "missiontest: dustcloud type=.. pos=(..)" (up to 32) and "missiontest: sensors scenes=..
#                    dustpoints=.. resourcepoints=.." at the end; Mission02 judges the BUSHAN-RE hint ("missiontest:
#                    hint watch|speech ..", "missiontest: hint skill=.. expected=.. fired=.. result=PASS|FAIL", part of
#                    the done line's result: the exe's timer, 120 / 180 / 360 s on difficulty 0 / 1 / 2, none on 3 / 4,
#                    off once the BUSHAN-RE is found). Pass "-- /catMissionTestHoldScout <tick>" with --mission 2 to
#                    hold the FindDestroyer recon until <tick> (6400 lets the NORMAL timer run out: the speech at 5824);
#                    --cfg 'DifficultySetting N' for another difficulty (the self-test's player is built for NORMAL)
#                    M3-R3 mines (HWI-TEST, docs/m3-r3-mines.md), any mode that plays a game, e.g. --mission 3:
#                    "-- /catMeshTexLog <substring>" logs the texture wiring of the meshes whose file name has it
#                    ("meshtex: register load .. static= teamColor= handles=" per textured material at the mesh load;
#                    "meshtex: draw .. scheme= textures= bound= white= [..]" at the first draw per colour scheme; e.g.
#                    /MINE/RL); "-- /catMineView <tick>[:dropped][:<distance>]" closes the sensors manager and snaps the
#                    camera to the level mine (:dropped: a laid mine) nearest the player's mothership from that tick
#                    (with --shot: the mine captures; changes the view); "-- /catMineLog <ticks>" logs every mine state
#                    change with its reason ("minelog: change .. state=A->B why=..") and <ticks> ticks of state, target,
#                    distance and speeds for the first 8 mines that start an attack run; "-- /catMineChase
#                    <tick>[:<shiptype>]" orders up to 4 of player 0's ships of that type (default sRecon) past the
#                    nearest level mine through the command layer (a test scene: no stream reference)
#                    M3-R3 engine-trails (HWI-TEST, docs/m3-r3-engine-trails.md), any mode that plays a game (ai,
#                    skirmish, missiontest): "-- /catTrailView <tick>:<ShipType>[:d<distance>][:e<elevation>][:s<side>]
#                    [:r<ratio>][:m<move>]" closes the sensors manager from that tick and keeps the camera <distance>
#                    (500) behind the first ship of the type moving over <ratio> (0.4; 0: any) of its top speed, raised
#                    <elevation> (20) and turned <side> (30) degrees (:m: ordered <move> units ahead, command layer); per
#                    frame "trailview: tick=.. h<slot>=x,y n<slot>=x,y c<slot>=x,y rs=.." (each trail's head, split
#                    nozzle and engine circle on the physical screen; the 27 random streams' states). With --shot: the
#                    trail captures (changes the view: no stream reference). The feature's A/B: "-- /exeTrails"
#                    warpstart: /catMissionTest <ticks> /catWarpStartTest /oracleDump <run>/run.hwo (M2-R9 missions-10-17):
#                    the exe's warp start (cat_spWarpToLevel 0x0051b9c0) from a TEST COPY of the install's warp.script
#                    supplied to this run only, as loadgame supplies its save: the run gets its own data dir
#                    (<run>/data: a symlink to every entry of the harness data dir, nothing else changed) plus a loose
#                    warp.script read from the install's archive (tools/bigfile.py cat cataclysm.big warp.script) with
#                    "WarpEnabled 1" and "LevelToWarpTo <--warp-level>"; the engine takes the loose file over the
#                    archive's as the exe does (bigFilesystemCompare: a newer local file). Nothing is written in
#                    original/game or the harness data dir. The test checks the level's record (Resources, Ships, Tech)
#                    against player 0's fleet in the ether after the warp and out of it after the arrival, the level's
#                    load, its KAS start and the arrival: "warpstarttest: ..." lines, "warpstarttest: done level=N ...
#                    result=PASS|FAIL", rc 0, run.hwo (two runs must give byte-identical streams). No mission is played
#   --warp-level N   warpstart mode: the LevelToWarpTo of the test warp.script (default 10)
#   --save NAME      loadgame mode: the single-player save to load (default "01 - Hiigara")
#   --load-ticks N   loadgame mode: universe ticks to run after the load (default 400)
#   --save-src DIR   loadgame mode (M2-R9 missions-10-17): the directory the save is copied from instead of the install's
#                    SavedGames/SinglePlayer (read only), e.g. an earlier run's settings/SavedGames/Training, where the
#                    engine's own Tutorial1 run wrote its lesson saves
#   --training       loadgame mode (M2-R9 missions-10-17): the save is a tutorial lesson's: it is copied into the run's
#                    settings dir's SavedGames/Training and loaded as the front end's tutorial load does
#                    (/catLoadGameTraining: TutorialSavedGamesPath, gpLoadTutorial, tutorial 1); the judgement adds the
#                    tutorial's KAS script (Watch_Tutorial1) running
#   --skirmish-ticks N skirmish mode: universe ticks to run before the judgement (default 600)
#   --skirmish-map NAME skirmish mode: the map, a MultiPlayer directory name (default LiveFireExercise2; Armed8 is an
#                    8-player map, so its spheres 2..7 must be skipped)
#   --skirmish-bigotry N skirmish mode (M2-R10 tests-leftovers): /catSkirmishBigotry N (0 .. 2): before StartNewGame the
#                    test presses the skirmish screen's CPU-attacks radio button N (OP_CPU_Attacks on update.big's
#                    Skirmish_Basic, buttons pData 0 / 1 / 2) through its callback opCPUAttacks 0x004db440, which sets
#                    tpGameCreated.aiplayerBigotry ("skirmishtest: bigotry=N" line). Without the flag the front end's
#                    default stays (0: no alliance; every default skirmish stream is unchanged). The computers ally at the
#                    skirmish AI's first play (cat_aisSetupAlliances 0x004310a0): 1 pairs them by how many there are (3 or
#                    more computers; a 2-player test with one computer sends none), 2 sends ALLIANCE_FORM_NEW_ALLIANCE
#                    for every index pair from the first computer to 7 whether the player exists or not: 21 commands in
#                    a 2-player test (1..7), the 21 GOG's two skirmish sessions carry at frame 88. With --record the
#                    recording holds them. The skirmish test keeps sound and speech on (SDL's dummy driver, the wall
#                    clock): the speech's variation draws (stream 12) split two record runs' rng_all; pass
#                    "-- /catVirtualAudio" for a record run that pairs in every field (the .pkts pair either way)
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
#   --shot SPEC      any mode (M3-R1 letterbox-front): /catShot SPEC /catShotFile <run>/shot: frames saved as
#                    <run>/shot.<tick>.<frame>.ppm, the PHYSICAL framebuffer (SDL_GL_GetDrawableSize) read back before the
#                    flip after everything is drawn (bars, subtitles, cursor), binary P6, top row first; SPEC is a comma
#                    list of <tick>[:<frame>] or <from>-<to>[/<step>][:<frame>] (universe ticks; <frame> the n-th frame
#                    drawn in that tick, default 0; at most 4096 files a run); the spec applies to every level of the
#                    run: after a level load restarts the tick counter the files are <run>/shot.s<N>.<tick>.<frame>.ppm
#                    (N = 1, 2, .. the level's segment); one "catshot: tick=.. frame=.. segment=N size=WxH ui=WxH
#                    letterbox=full|fade=F|none letterboxBehind=0|1 file=.." log line per file; the summary counts them.
#                    Pixels: tools/ppm_region.py (info, stats, probe, diff, down2, bands). The nis mode's fade capture:
#                    "-- /catNisScissorOut <tick>:<seconds>" starts the letterbox fade-out at that tick as an NIS's
#                    ScissorOut event does (a measurement only: never a dump baseline)
#   --shot-letterbox with --shot: /catShotLetterbox, only the frames with the NIS / KAS letterbox up (full or fading)
#   --res WxH        any mode (M3-R1): appends "screenWidth W" / "screenHeight H" to the run's fresh home Cataclysm.cfg
#                    (never the template); without it every run is 1024x768
#   --cfg 'KEY VALUE' any mode (M3-R1), repeatable: appends the row to the run's fresh home Cataclysm.cfg (never the
#                    template), e.g. --cfg 'letterboxBehind 1' (the old letterbox look, HWI-FEATURE(letterbox-setting));
#                    the options file is read before the command line, so a switch after -- still wins
#   --player-colours BASE/STRIPE any mode (M3-R1 team-colours-warp): the player's colours as the front end's colour
#                    picker leaves them: appends PlayerBaseColor / PlayerStripeColor (the colour words, hex as the
#                    engine's teamcolour lines print them, e.g. ff1bca82/ffff5539 = the user's play-test colours;
#                    written as the options file's signed decimals), ColorsPicked 1 and PlayerRace 0 to the run's
#                    fresh home Cataclysm.cfg (as --cfg does); gameStart then gives player 0 these colours
#                    (teTeamColorsSet(0, utyBaseColor, utyStripeColor)), not the default scheme
#   --teamcolour     the single-player modes (missiontest, sp, loadgame; M3-R1 team-colours-warp): /catTeamColourTest
#                    (src/Game/TeamColourTest.c): "teamcolour: dump before|statics|after" lines at each level change
#                    (the schemes, player 0's Somtaaw ship types and their colour schemes, each level-0 texture's
#                    registry entry for scheme 0 and its palette), "teamcolour: check after|level|end ... result=" (every
#                    loaded Somtaaw static player 0 can own: each texture's scheme-0 handle loaded, nPalettes > 0, the
#                    palette index the one holding the player's colours, the colour info there the player's base /
#                    stripe, a paletted texture's palette built with its mesh-sort list's team flags; the Kuun-Lan on
#                    scheme 0; in a new game the colours the picker's) after each level's textures load, at each level's
#                    64th tick and at the end, "teamcolour: fail ..." per fault and "teamcolour: done checks=N failed=F
#                    ... result=PASS|FAIL" at the mission / load-game test's done line. In missiontest it also makes
#                    the run go on past its jump to the next MISSION level (Mission01: Mission02; mission N: past its
#                    next level's opening-NIS staging level to Mission N+1) and end 32 ticks after that level loaded
#                    ("-- /catTeamColourTicks N": N ticks, e.g. 2600 for captures of the next mission after the
#                    arrival). Read only: no stream field changes with it (the run is longer, so its stream has more
#                    ticks; compare with --to-tick). A debug build also logs "teamcolour: heap level=.. dupe=N" (the
#                    live "dupe trcolorinfo list" blocks of the memory analysis trRegistryRefresh writes)
#   --lodtest SPEC   any mode that draws the main view (M3-R1 full-detail, HWI-FEATURE(full-detail)): /catLodTest SPEC,
#                    SPEC = <tick>[:far][:fleet]: in the main view's first frame of universe tick <tick>, after the levels of
#                    detail are chosen, one "lodtest: obj type=.. name=.. id=.. band=B drawn=D levels=N radius=.. dist=.."
#                    line per visible object with a LOD table (B = currentLOD, the exe's level: the size-on-screen bands /
#                    the distance table / the frame-time passes; D = the level drawn: 0 under FULL DETAIL, the noLOD row,
#                    on by default; B with -- /lodBands), then "lodtest: frame ..." (counts, the camera distance, the
#                    vertices / polygons of the levels drawn) and "lodtest: done ... result=PASS|FAIL" (FULL DETAIL on:
#                    every D 0 and at least one object; off: every D = B and at least one B above 0). ":far" pulls the
#                    camera out to its farthest zoom from tick 16; ":fleet" closes the sensors manager the headless start
#                    opens at tick 16 and focuses the camera on every ship, so ai / skirmish / mission draw the main view
#                    (both change the view: such a run is no stream reference). The summary gets a "lodtest:" line
#   --lodstats SPEC  any mode (M3-R1 full-detail): /catLodStats SPEC, SPEC = <from>-<to>[:fleet]: per frame of those
#                    universe ticks "lodstats: tick=.. ms=.. objects=.. verts=.. polys=.. fullDetail=0|1" (ms: the render
#                    task from its top to after the flip, a glFinish before the clock is read; verts / polys: the levels
#                    drawn of the visible objects with a LOD table), then "lodstats: done frames=N median=.. p99=.. max=..
#                    verts_median=.. size=WxH". A GPU measurement (docs: idle GPU); use the release build (--bin) for
#                    frame times. The summary gets a "lodstats:" line
#   --scene SPEC     any mode that plays a game (M3-R1 motion-jitter, HWI-FEATURE(motion-interp) measurement; skirmish:
#                    the user's jitter recipe): /catMotionFps <--fps> /catMotionScene SPEC /catMotionDump <run>/scene.dump;
#                    SPEC = <kind>[:<distance>[:<move>[:<ticks>[:<start>]]]] [audit: <start> added]. At tick <start> (16) the scene picks player 0's Recon nearest its
#                    mothership (A) and the Recon nearest A (B), C (the nearest other free ship), D (the nearest
#                    asteroid), E (the nearest derelict, else a no-owner ship), F (a module clamped to the mothership),
#                    G (the wingman of three ships of one type, g0 / g2 the others), H (the nearest free ship and host
#                    ThisShipCanDockWith accepts: support, or a Worker's deposit), W (a Worker) and M (the mothership)
#                    [audit: H / W / M as MotionTest.c picks and orders them], O (the ore-can armour clamped to the mothership; kind eject,
#                    M3-R4 module-spin-interp: the camera on O, no Recons needed, at tick 48 O is jettisoned as a module launch does and
#                    M stays put; PASS = frames sampled and O still ejecting; with /catMadProbe: spinprobe: lines), focuses the camera on A (kind recon; follow: on B) at its zenith (declination -85, the
#                    eye straight above) and snaps it there (<distance> > 0: that camera distance; 0: the focus zoom); at
#                    tick 48 the command layer orders B <move> units (default 500) straight ahead, C 1,500 units across
#                    A, G's three ships into delta formation and 2,000 units along -x, H to dock at its host, W to harvest
#                    the asteroid nearest it and M 2,000 units ahead (no input); from tick 40 to 48 + <ticks> (default 160) every frame writes a motion-format
#                    line with alpha= (the alpha the frame was blended with, -1 none), raw= (taskPeriodFraction of the
#                    universe task), calls= (universe ticks since the previous frame), mi=blended/new/teleported and
#                    per object <name>m=1 when the interpolation wrote its transform that frame; "motiontest: scene ..."
#                    lines, "motiontest: scene done ... Bmoved=.. result=PASS|FAIL" (PASS: frames dumped and B moved
#                    over half its move); the summary gets a "scene:" line. The virtual clock runs --fps frames a tick;
#                    pass --skirmish-ticks over 48 + <ticks>. tools/nis_smoothness.py reads the dump (--span, --offscreen)
#   --real-clock FPS with --scene (M3-R1 motion-jitter): /catMotionRealClock FPS: the run on the wall clock instead of the
#                    virtual one (the offscreen driver, no vertical sync): each frame's dispatch waits for its slot of a
#                    1/FPS grid (SDL's performance counter), as a monitor's vertical sync paces the swap; the dump lines
#                    add ms= (the dispatch time), ft= (since the previous frame), tt= (the task ticks, 4 ms each) and
#                    sub= (taskSubTick), and carry crc=00000000 (no read-back). A GPU measurement: an idle GPU, the
#                    release build (--bin build/release/hwi-engine). Also in the nis mode (its lines get the same clock
#                    tokens; the mode's /noSpeech is left out then: a debug-build switch, and the real clock has no CRC)
#   --mode-to WxH    modequit mode: the resolution the render switch goes to (default 1280x720)
#   --resize-at T:WxH  any mode with a universe (M3-R4 window-modes, HWI-TEST): /catResizeAt T:WxH, the window treated as
#                    resized to WxH at universe tick T
#   --frame-log      any mode (M3-R2 instrumentation, HWI-TEST): /frameLog <run>/frame.log: one line a second (frame-time
#                    p50 / p95 / p99 / max, the frames over the refresh interval and by how many refreshes, the universe
#                    ticks and their cost, the main heap's use and growth, the 'dupe trcolorinfo list' blocks (debug
#                    build), the vertices drawn) and a 0.1 ms histogram line; the refresh is the --real-clock rate, else
#                    the display mode's, else 144 (the header says which). The summary's "framelog:" line is
#                    tools/frame_log_report.py --oneline (run it on the file for the whole report: the drift test, the
#                    heap's growth a minute, the seconds over budget). Frame times are a GPU measurement: the release
#                    build (--bin), an idle GPU
#                    Every run's log has the settings line (M3-R2 instrumentation, HWI-FEATURE(instrumentation)):
#                    "hwi-settings: objInterpolate=.. (source) nisInterpolate=.. letterboxBehind=.. noLOD=.. uiScale=..->..%
#                    overlay=.. fit=.. switches=.. [start]" once the window exists, again at every change ([console ..],
#                    [options accept], [key overlay], [frame], [window]); the summary prints the first and counts them.
#                    The overlay (HWI-FEATURE(instrumentation)) is off by default: "-- /overlay" turns it on for a run
#                    (with --shot to capture it), the console row "overlay" and the Menu key toggle it
#   --frame-sig      any mode (M3-R2 nis-flicker, HWI-TEST, docs/m3-r2-nis-flicker.md): /catFrameSig <run>/frame.sig: one
#                    line per rendered frame "F <n> tick=<t> seg=<level segment> dump=<dump frame|-> sig mv=<main views
#                    drawn> rl=<render list> vis=<visible objects> lb=<late letterbox bars: -|full|fade> fd=<nisBlackFade>
#                    nis=<NIS time|-> cut=<nisCameraCutTime> a=<the NIS task's period fraction|-> ft=<the frame's
#                    wall-clock period, ms>" and, on the virtual clock (not with --real-clock), a read-back's "bar=<non-black pixels in the bar rows> mid=<between them> lum=<mean luma>
#                    d1=<thumbnail difference to the previous frame> d2=<to the frame before>"; the motion / nis dump
#                    lines get the same " sig ..." after crc= (only with this flag: the default dumps are unchanged). The
#                    summary's "framesig:" line is tools/dump_outliers.py --oneline (the one-frame outliers: a frame that
#                    differs from both neighbours while they match, or that loses its objects or its bars for one frame;
#                    and the short runs: 1..12 frames without the bars or the objects between frames that have them, a
#                    state that lasts one universe tick being 9 frames at 144 Hz; full-black-fade frames excluded)
#   --virtual-fps N  any virtual-clock mode (M3-R2 nis-flicker, HWI-TEST): /catVirtualFps N: N/16 frames a universe tick
#                    instead of one (mission, missiontest, replay, ..), so the render-time interpolation runs between
#                    ticks as in play (with --frame-sig: the NIS-first start's cutscenes at 144 Hz); a scan only: the
#                    extra frames draw the shared random stream, so the run's .hwo is never a baseline
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
conticks=400; bticks=7000; aiticks=400; mtticks=20000; mttut=0; mtmission=1; savename="01 - Hiigara"; ldticks=400
motticks=224; nisspec=n06:26:36; vfps=144; recording=oracle01; recsrc=""; recas=""; skticks=600; skmap=LiveFireExercise2
warplevel=10; savesrc=""; training=0; recordname=""; recordstop=""; skbigotry=""; failjump=0
shotspec=""; shotlb=0; res=""; cfgrows=(); teamcolour=0; lodtest=""; lodstats=""; scene=""; realclock=""
modeto=1280x720; framelog=0; framesig=0; anyfps=""; resizeat=""
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
		--record) recordname=$2; shift ;;
		--record-stop) recordstop=$2; shift ;;
		--recording) recording=$2; shift ;;
		--recording-src) recsrc=$(realpath -- "$2"); shift ;;
		--recording-as) recas=$2; shift ;;
		--skirmish-ticks) skticks=$2; shift ;;
		--mission-test-ticks) mtticks=$2; shift ;;
		--tutorial) mttut=1 ;;
		--fail-jump) failjump=1 ;;
		--mission) mtmission=$2; shift ;;
		--save) savename=$2; shift ;;
		--warp-level) warplevel=$2; shift ;;
		--save-src) savesrc=$(realpath -- "$2"); shift ;;
		--training) training=1 ;;
		--load-ticks) ldticks=$2; shift ;;
		--skirmish-map) skmap=$2; shift ;;
		--skirmish-bigotry) skbigotry=$2; shift ;;
		--perturb) perturb=$2; shift ;;
		--hwo) hwo=1 ;;
		--shot) shotspec=$2; shift ;;
		--shot-letterbox) shotlb=1 ;;
		--res) res=$2; shift ;;
		--resize-at) resizeat=$2; shift ;;   # M3-R4 window-modes
		--cfg) cfgrows+=("$2"); shift ;;
		--player-colours)
			[[ "$2" =~ ^([0-9a-fA-F]{8})/([0-9a-fA-F]{8})$ ]] || { echo "m2_harness: --player-colours wants BASE/STRIPE (8 hex digits each)" >&2; exit 2; }
			pcb=$(( 16#${BASH_REMATCH[1]} )); pcs=$(( 16#${BASH_REMATCH[2]} ))
			(( pcb > 2147483647 )) && pcb=$(( pcb - 4294967296 )); (( pcs > 2147483647 )) && pcs=$(( pcs - 4294967296 ))
			cfgrows+=("PlayerBaseColor    $pcb" "PlayerStripeColor    $pcs" "ColorsPicked    1" "PlayerRace    0"); shift ;;
		--teamcolour) teamcolour=1 ;;
		--lodtest) lodtest=$2; shift ;;
		--lodstats) lodstats=$2; shift ;;
		--scene) scene=$2; shift ;;
		--real-clock) realclock=$2; shift ;;
		--mode-to) modeto=$2; shift ;;
		--frame-log) framelog=1 ;;
		--frame-sig) framesig=1 ;;
		--virtual-fps) anyfps=$2; shift ;;
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
# M3-R1: --res / --cfg rows appended to this run's own copy of the options file (the template stays as it is)
if [[ -n "$res" ]]; then
	[[ "$res" =~ ^([0-9]+)x([0-9]+)$ ]] || { echo "m2_harness: --res wants WxH" >&2; exit 2; }
	printf 'screenWidth    %s\nscreenHeight    %s\n' "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}" >> "$out/home/.homeworld/Cataclysm.cfg"
fi
for row in "${cfgrows[@]}"; do printf '%s\n' "$row" >> "$out/home/.homeworld/Cataclysm.cfg"; done

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
[[ "$mode" == uiscale ]] && args+=(/catUiScaleTest)
[[ "$mode" == modequit ]] && args+=(/catModeQuitTest "$modeto")   # M3-R2 instrumentation
[[ "$mode" == resize ]] && args+=(/catResizeTest)                 # M3-R4 window-modes
[[ -n "$resizeat" ]] && args+=(/catResizeAt "$resizeat")           # M3-R4 window-modes
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
elif [[ "$mode" == missiontest ]]; then
	if [[ $mttut -eq 1 ]]; then
		args+=(/catMissionTestTutorial "$mtticks" /oracleDump "$out/run.hwo")
	else
		args+=(/catMissionTest "$mtticks" /oracleDump "$out/run.hwo")
		if [[ "$mtmission" -ne 1 ]]; then args+=(/catMissionTestMission "$mtmission"); fi
		[[ $failjump -eq 1 ]] && args+=(/catMissionTestFailJump)
	fi
elif [[ "$mode" == warpstart ]]; then
	# M2-R9 missions-10-17: the run's own data dir: the harness data dir's entries as symlinks plus the test warp.script
	# (the install's, from its archive, with the two switches set); HW_Data points there for this run only
	wdata="$out/data"; mkdir -p "$wdata"
	for e in "$DATA"/*; do ln -s -- "$e" "$wdata/$(basename -- "$e")"; done
	python3 -I "$root/tools/bigfile.py" cat "$DATA/cataclysm.big" warp.script > "$out/warp.script.install" || \
		{ echo "m2_harness: warp.script not readable from $DATA/cataclysm.big" >&2; exit 1; }
	sed -E "s/^(WarpEnabled[[:space:]]+)[0-9]+/\11/; s/^(LevelToWarpTo[[:space:]]+)[0-9]+/\1$warplevel/" \
		"$out/warp.script.install" > "$wdata/warp.script"
	grep -q "^WarpEnabled[[:space:]]*1" "$wdata/warp.script" && grep -q "^LevelToWarpTo[[:space:]]*$warplevel\b" "$wdata/warp.script" || \
		{ echo "m2_harness: the test warp.script did not take WarpEnabled 1 / LevelToWarpTo $warplevel" >&2; exit 1; }
	data="$wdata"
	args+=(/catMissionTest "$mtticks" /catWarpStartTest /oracleDump "$out/run.hwo")
elif [[ "$mode" == loadgame ]]; then
	# M2-R8 singleplayer-tutorial: the save is copied into the run's own settings dir (where the engine reads and writes
	# saved games: /settingspath), never into the data dir: the engine writes a KAS level save of the same name over an
	# existing file wherever it finds it, and the data dir's SavedGames must stay empty (only what a real install has)
	ssrc="${savesrc:-$GAME/SavedGames/SinglePlayer}"
	sdir=SinglePlayer; [[ $training -eq 1 ]] && sdir=Training
	[[ -f "$ssrc/$savename" ]] || { echo "m2_harness: no save $ssrc/$savename" >&2; exit 1; }
	mkdir -p "$out/settings/SavedGames/$sdir"
	cp -f -- "$ssrc/$savename" "$out/settings/SavedGames/$sdir/$savename"
	args+=(/catLoadGameTest "$ldticks" /catLoadGameName "$savename" /oracleDump "$out/run.hwo")
	[[ $training -eq 1 ]] && args+=(/catLoadGameTraining)
elif [[ "$mode" == skirmish ]]; then
	args+=(/catSkirmishMap "$skmap" /catSkirmishTest "$skticks" /oracleDump "$out/run.hwo")
	[[ -n "$skbigotry" ]] && args+=(/catSkirmishBigotry "$skbigotry")
elif [[ "$mode" == motion ]]; then
	args+=(/catMotionFps "$vfps" /catMotionTest "$motticks" /catMotionDump "$out/motion.dump")
elif [[ "$mode" == replay ]]; then
	# M2-R6 oracle-mode-a: the recording into the data dir's SavedGames/RecordedGames (as a real install keeps it),
	# then linked into the run's settings dir, where the engine looks (saved games and, since M2-R9, the packets: /settingspath)
	# M2-R7 integrate: --recording-src DIR takes the pair from an archived session directory instead (read only) and
	# --recording-as NAME2 copies and replays it under another name, so the install's current recording of the same
	# name is not replaced in the data dir (the engine names the packets <name>.pkts, OracleDump.c oracleStartReplay)
	src="${recsrc:-$GAME/SavedGames/RecordedGames}"
	[[ -f "$src/$recording" && -f "$src/$recording.pkts" ]] || { echo "m2_harness: no recording $src/$recording(.pkts)" >&2; exit 1; }
	mkdir -p "$DATA/SavedGames/RecordedGames" "$out/settings/SavedGames/RecordedGames"
	play="${recas:-$recording}"
	for ext in "" ".pkts"; do
		cmp -s "$src/$recording$ext" "$DATA/SavedGames/RecordedGames/$play$ext" || cp -f -- "$src/$recording$ext" "$DATA/SavedGames/RecordedGames/$play$ext"
		ln -sf -- "$DATA/SavedGames/RecordedGames/$play$ext" "$out/settings/SavedGames/RecordedGames/$play$ext"
	done
	args+=(/oracleReplay "$play" /oracleDump "$out/run.hwo")
elif [[ "$mode" == nis ]]; then
	# /noSpeech: the NIS's speech subtitles start and end with the audio thread's progress through the speech (wall
	# clock, even on SDL's dummy driver), which made about 40 frame CRCs of n06's window differ run to run
	# M3-R1 motion-jitter: not with --real-clock (/noSpeech is a debug-build switch; a real-clock run has no CRC to keep)
	[[ -z "$realclock" ]] && args+=(/noSpeech)
	args+=(/catMotionFps "$vfps" /catNisMotionTest "$nisspec" /catMotionDump "$out/nis.dump")
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
if [[ -n "$recordname" ]]; then
	# M2-R9 recorder-leftovers: the recording lands in the run's settings dir (saves and packets: /settingspath [audit: was "packets: the cwd"; NetCheck.c opens them in the settings path])
	mkdir -p "$out/settings/SavedGames/RecordedGames"
	args+=(/oracleRecord "$recordname")
	[[ -n "$recordstop" ]] && args+=(/oracleRecordStop "$recordstop")
fi
[[ $teamcolour -eq 1 ]] && args+=(/catTeamColourTest)     # M3-R1 team-colours-warp
[[ -n "$lodtest" ]] && args+=(/catLodTest "$lodtest")      # M3-R1 full-detail
[[ -n "$lodstats" ]] && args+=(/catLodStats "$lodstats")   # M3-R1 full-detail
[[ -n "$scene" ]] && args+=(/catMotionFps "$vfps" /catMotionScene "$scene" /catMotionDump "$out/scene.dump")   # M3-R1 motion-jitter
[[ -n "$realclock" ]] && args+=(/catMotionRealClock "$realclock")   # M3-R1 motion-jitter
[[ $framelog -eq 1 ]] && args+=(/frameLog "$out/frame.log")   # M3-R2 instrumentation
[[ $framesig -eq 1 ]] && args+=(/catFrameSig "$out/frame.sig")   # M3-R2 nis-flicker
[[ -n "$anyfps" ]] && args+=(/catVirtualFps "$anyfps")   # M3-R2 nis-flicker
if [[ -n "$shotspec" ]]; then
	args+=(/catShot "$shotspec" /catShotFile "$out/shot")
	[[ $shotlb -eq 1 ]] && args+=(/catShotLetterbox)
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
grep -q "motiontest: scene done" "$L" && milestone="motion scene done"
grep -q "toggletest: done" "$L" && milestone="toggle self-test done"
grep -q "uiscaletest: done" "$L" && milestone="uiscale self-test done"
grep -q "modequittest: done" "$L" && milestone="modequit self-test done"
grep -q "resizetest: done" "$L" && milestone="resize self-test done"
grep -q "skirmishtest: done" "$L" && milestone="skirmish self-test done"
grep -q "missiontest: done" "$L" && milestone="mission self-test done"
grep -q "loadgametest: loaded" "$L" && milestone="save loaded"
grep -q "loadgametest: done" "$L" && milestone="save-load self-test done"
grep -q "missiontest: game end" "$L" && milestone="mission self-test: the game's end (credits)"
grep -q "warpstarttest: done" "$L" && milestone="warp-start self-test done"

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
	if [[ "$mode" == warpstart ]]; then
		echo "data=$data  (warpstart: the harness data dir's entries + the test warp.script: $(grep -h '^WarpEnabled\|^LevelToWarpTo' "$data/warp.script" | tr -s '\t ' ' ' | paste -sd';'))"
	else
		echo "data=$data$([[ "$data" != "$DATA" ]] && echo '  (ISOLATION RUN: not the harness data dir)')"
	fi
	echo "args=${args[*]}"
	echo "milestone: $milestone"
	echo "first stop: $stop"
	echo "first non-benign UBSan report (non-fatal): ${ubsan1:-none}"
	echo "fescreen UBSan reports (struct fescreen / featom / fibfileheader): $fescreen_n"
	echo "file loads: $nload; ASan errors: $asan_n; LeakSanitizer at exit: ${leak_sum:-no unsuppressed leak}"
	echo "ship statics: $(grep -c 'shipstatic: loaded' "$L") loaded, $(grep -c 'shipstatic: .* not found' "$L") not found (disabled), $(grep -c 'shipstatic: other' "$L") other .shp; $(grep -h 'shipstatic: self-test done' "$L" | sed 's/^.*shipstatic: //' | head -n 1)"
	[[ "$mode" == meshes ]] && echo "meshes: $(grep -c . "$out/meshes.list" 2>/dev/null) listed, $(grep -c 'meshtest: loaded' "$L") .geo/.peo loaded, $(grep -c 'meshtest: mad loaded' "$L") .mad loaded, $(grep 'meshtest: .*loaded' "$L" | grep -c -v ' bad=0$') with bad=; $(grep -h 'meshtest: self-test done' "$L" | sed 's/^.*meshtest: //' | head -n 1)"
	[[ -f "$out/run.hwo" ]] && echo "oracle: $(grep -h 'oracle: self-test done\|oracle: mission run done\|oracle: replay done' "$L" | sed 's/^.*oracle: //' | head -n 1); run.hwo $(stat -c %s "$out/run.hwo") bytes, sha256 $(sha256sum "$out/run.hwo" | cut -c1-16)"
	[[ -n "$recordname" ]] && echo "record: $(grep -h 'oracle: record ' "$L" | sed 's/^.*oracle: //' | paste -sd';'); files: $(cd "$out/settings/SavedGames/RecordedGames" 2>/dev/null && for f in "$recordname" "$recordname.pkts"; do [[ -f "$f" ]] && echo -n "$f $(stat -c %s "$f") B sha256 $(sha256sum "$f" | cut -c1-16); "; done)"
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
	[[ "$mode" == loadgame ]] && echo "loadgame: $(grep -h 'savecat32: loaded\|loadgametest: load\|loadgametest: done' "$L" | sed 's/^.*\(savecat32\|loadgametest\): //' | paste -sd';')"
	[[ "$mode" == missiontest ]] && echo "missiontest: $(grep -h 'missiontest: ' "$L" | grep -v 'missiontest: status\|missiontest: fsm\|missiontest: FAIL' | sed 's/^.*missiontest: //' | paste -sd';'); FAIL lines: $(grep -c 'missiontest: FAIL' "$L")"
	[[ "$mode" == warpstart ]] && echo "warpstarttest: $(grep -h 'warpstarttest: ' "$L" | sed 's/^.*warpstarttest: //' | paste -sd';')"
	[[ "$mode" == skirmish ]] && echo "skirmishtest: $(grep -h 'skirmishtest: ' "$L" | grep -v 'skirmishtest: FAIL' | sed 's/^.*skirmishtest: //' | paste -sd';'); FAIL lines: $(grep -c 'skirmishtest: FAIL' "$L")"
	[[ "$mode" == toggle ]] && echo "toggletest: $(grep -h 'toggletest: ' "$L" | grep -v 'toggletest: FAIL' | sed 's/^.*toggletest: //' | paste -sd';'); FAIL lines: $(grep -c 'toggletest: FAIL' "$L")"
	[[ "$mode" == uiscale ]] && echo "uiscaletest: $(grep -h 'uiscaletest: ' "$L" | grep -v 'uiscaletest: FAIL' | sed 's/^.*uiscaletest: //' | paste -sd';'); FAIL lines: $(grep -c 'uiscaletest: FAIL' "$L")"
	[[ "$mode" == resize ]] && echo "resizetest: $(grep -h 'resizetest: ' "$L" | grep -v 'resizetest: FAIL\|resizetest: list ' | sed 's/^.*resizetest: //' | paste -sd';'); list: $(grep -c 'resizetest: list .*bit' "$L") modes; FAIL lines: $(grep -c 'resizetest: FAIL' "$L")"   # M3-R4 window-modes
	[[ -n "$resizeat" ]] && echo "resize-at: $(grep -h 'resizetest: at tick\|^window: resized\|^window: the owed' "$L" | paste -sd';')"   # M3-R4 window-modes
	echo "$(grep -h '^window: requested\|^window: borders' "$L" | head -n 2 | paste -sd';'); $(grep -h '^uiscale: physical' "$L" | head -n 1)"
	echo "settings: $(grep -c '^hwi-settings:' "$L") lines; first: $(grep -h '^hwi-settings:' "$L" | head -n 1 | sed 's/^hwi-settings: //')"
	if [[ "$mode" == modequit ]]; then
		nq=$(grep -c "Quit game, baby" "$L")
		mq=$(grep -h 'modequittest: done' "$L" | sed 's/^.*modequittest: //')
		echo "modequit: $(grep -h 'modequittest: ' "$L" | grep -v 'modequittest: done' | sed 's/^.*modequittest: //' | paste -sd';'); $mq; rc=$rc; \"Quit game\" lines: $nq; quit: $([[ $rc -eq 0 && $nq -eq 1 && "$mq" == *result=PASS* ]] && echo PASS || echo FAIL)"
	fi
	[[ $framesig -eq 1 ]] && echo "framesig: $(python3 -I "$root/tools/dump_outliers.py" "$out/frame.sig" --oneline 2>&1 | head -n 1)"
	[[ $framelog -eq 1 ]] && echo "framelog: $(python3 -I "$root/tools/frame_log_report.py" "$out/frame.log" --oneline 2>&1 | head -n 1)"
	[[ "$mode" == keys ]] && echo "keystest: $(grep -h 'keystest: ' "$L" | grep -v 'keystest: FAIL' | sed 's/^.*keystest: //' | paste -sd';'); FAIL lines: $(grep -c 'keystest: FAIL' "$L")"
	[[ "$mode" == sound ]] && echo "soundtest: $(grep -h 'soundtest: ' "$L" | sed 's/^.*soundtest: //' | paste -sd';'); UBSan reports in fquant.c/fqcodec.c: $(cat "$L" "$out"/ubsan.* 2>/dev/null | grep 'runtime error' | grep -c 'fquant\.c\|fqcodec\.c'); PATCH reports: $(cat "$L" "$out"/ubsan.* 2>/dev/null | grep 'runtime error' | grep -c "struct PATCH")"
	[[ "$mode" == etg ]] && echo "etg: $(grep -c . "$out/etg.list" 2>/dev/null) listed; $(grep -h 'etgtest: etgStartup done\|etgtest: self-test done\|etgtest: created\|etgtest: effect self-test done' "$L" | sed 's/^.*etgtest: //' | paste -sd';')"
	[[ -n "$shotspec" ]] && echo "catshot: $(grep -c 'catshot: tick=' "$L") files ($(grep -h 'catshot: tick=' "$L" | head -n 1 | sed 's/^.*catshot: //; s/ file=.*//') ..); FAIL lines: $(grep -c 'catshot: FAIL' "$L")"
	[[ -n "$lodtest" ]] && echo "lodtest: $(grep -c 'lodtest: obj ' "$L") objects; $(grep -h 'lodtest: frame' "$L" | sed 's/^.*lodtest: //' | cut -c1-300); $(grep -h 'lodtest: done' "$L" | sed 's/^.*lodtest: //' | cut -c1-300)"
	[[ -n "$lodstats" ]] && echo "lodstats: $(grep -c 'lodstats: tick=' "$L") frames; $(grep -h 'lodstats: done' "$L" | sed 's/^.*lodstats: //' | cut -c1-300)"
	[[ -n "$scene" ]] && echo "scene: $(grep -h 'motiontest: scene done' "$L" | sed 's/^.*motiontest: //' | cut -c1-300)"
	[[ $teamcolour -eq 1 ]] && echo "teamcolour: $(grep -c 'teamcolour: check ' "$L") checks, $(grep 'teamcolour: check ' "$L" | grep -c 'result=FAIL') FAIL, $(grep -c 'teamcolour: fail ' "$L") fail lines; $(grep -h 'teamcolour: done' "$L" | sed 's/^.*teamcolour: //' | cut -c1-400)"
	[[ -n "$res$(printf '%s' "${cfgrows[@]}")" ]] && echo "options rows added: $(tail -n $(( ${#cfgrows[@]} + $([[ -n "$res" ]] && echo 2 || echo 0) )) "$out/home/.homeworld/Cataclysm.cfg" | paste -sd';')"
	echo "animatics: $(grep -c 'animatic: skipped (headless)' "$L") skipped (headless): $(grep -h 'animatic: skipped (headless)' "$L" | sed 's/^.*file=//' | paste -sd',')"
	echo "engine glows:$(grep -c -i 'engineglows' "$out/FileLoads.log" 2>/dev/null) mesh/mex loads from misc\\EngineGlows in FileLoads.log"
	echo "UBSan report kinds (count, site):"; echo "$ubsan_kinds"
	echo "log tail:"; grep -v '^    #\|^ *$' "$L" | tail -n 8
} | tee "$out/summary.txt"
echo "run=$label mode=$mode driver=$drv rc=$rc wall=${wall}s data=$data milestone=\"$milestone\" stop=\"$stop\"" >> "$HARNESS/runs/runs.txt"
