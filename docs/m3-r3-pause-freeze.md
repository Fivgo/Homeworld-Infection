<!-- done: a-exe, b-repro, c-freeze, d-camera-trails-nis, e-fences, f-docs -->
<!-- subsystems: a-exe, b-repro, c-freeze, d-camera-trails-nis, e-fences, f-docs -->
# M3-R3 producer 2: the pause freeze (key pause-freeze)

Base c8f97bb (this work is `git diff c8f97bb` in the m2 worktree). The user's item 5: "while paused, it seems like all
ships hold onto their travel vector as well as the camera. During a pause I notice that ships jitter in place and it
seems to be the other half of the issue to get smooth interpolation to work." Presentation only.

The answers file `cycle/answers/m3-r3-user.md` was read first and again before this report: unchanged (item 5 verbatim,
the orchestrator's hypothesis "freeze the render pose (alpha held, or the last rendered pose kept) whenever the universe
is paused, and the same for the camera"); no later answer changes this task.

**In one paragraph.** The user pauses with **P**, which in Cataclysm is not universePause but the frame-step flag
0x00a6ed74; univUpdate stops, the universe task does not, and M3-R1's lerp only knew universePause: every frame of a P
pause re-drew the last tick's motion (ships and the camera sliding forward one tick and snapping back 16 times a
second). HWI-FEATURE(pause-freeze) holds every object and the camera's focus at the pose the last frame drew, stops the
camera float and the engine glows' / trails' per-frame random jitter while paused, and on the unpause starts the next
tick's blend from the held pose. Render-side only: the Mission01 pair, the missiontest pair, the four default dumps
(and the opt-out and /meshImmediate twins) are identical to the previous audit's.

## (a) Which pause: the P key, the exe's flag 0x00a6ed74 (not universePause)

The user's logs answer it: `original/play-home/logs/play-20261022-155157.log` has `Pause ON` / `Pause OFF` lines (157,
159, 164, 173-179, 197-202, ...), the debug message of the key binding **Pause**, bound to **P** (`key  Pause  80` in the
install's `default_keys.cfg` line 110 and in the user's `play-home/.homeworld/keys.cfg` line 110).

The exe (disassembly `original/scratch-m2/m2r2-mesh-tools/exe.dis`; decompilation `tools/m2_fdec.py`):
- **The P key**, binding 0x69 `catKbPause` **0x00550640**: returns in a multiplayer game or a recording (0xa43c94,
  0xa75198 | 0xa7518c), sends the tutorial its "KB_Pause" message, refuses while the letterbox is in or full
  (0xa4cb38 != 0, 0xa4cb58), ends the pie-plate point mode on the way in, then **toggles 0x00a6ed74**
  (`sete al; mov ds:0xa6ed74,eax` at 0x5506c0-0x5506c5) and plays sound 0x108. The tree has it verbatim
  (KeyBindings.c `catKbPause`, `catUnivFrameStep`).
- **univUpdate 0x0054c3f0** returns 0 at once while 0x00a6ed74 is set and no frame step is left (0x00a6ed78 < 1): no
  tick, no `universe.univUpdateCounter` increment. **universeUpdateTask 0x0053e460 keeps running every period**, so the
  task clock (the render alpha's `taskPeriodFraction`) keeps cycling through every tick of the pause.
- **universePause 0x00a6c5b4** is a different flag: the console (`cat_consoleOpen` 0x0045a460), the Escape menu
  (`cat_mrEscape` 0x0054f5f0, mainrgn.c, before `gpStartInGameEscapeMenu`), the game-stats screen, GamePick. universeUpdateTask skips univUpdate
  for it (the tree's Universe.c 5062, `cat_univRealTimeDelta`).
- **The NIS's own pause**: `nisPaused` (`nisPause` 0x004ce5f0, IDENTICAL: `_DAT_00a4cac8 = bPause`). nisUpdateTask
  0x004cc600 reads 0x00a6ed74 only to clear it while the letterbox comes in, never 0x00a6c5b4: universePause does not
  stop an NIS. The P key is refused while an NIS's letterbox is up. In a build with NIS_TIME_CONTROLS (the tree's debug
  build, the harness) nisUpdate then advances 1e-7 s a tick (NIS.c `NIS_SEEKABLE`); the exe has no NIS
  pause at all (nisPaused 0x00a4cac8 is only written: 0x4cc4e8, 0x4ce5f4; nothing reads it), and the tree's release build neither (the camera fly-in sets nisPaused, which nothing there reads).
- **Which render code reads the pause flags** (every reference to 0xa6c5b4 / 0xa6ed74 mapped to its function through
  `functions.csv`): `rndRenderTask` 0x0057a910 (the shameless plug while either is set: a static texture),
  `cat_moRender` (the "PAUSED" text: static), `rndMainViewRenderFunction` 0x005764e0 (the **camera float**: its clocks
  stop under universePause only), `cat_conTextDraw`, and simulation-side effect code (`cat_blastUpdate` 0x00516020,
  run by univUpdate per tick, and the dreadnought / moon-beam effects: not per frame). The exe's render path reads 0x00a6ed74 only for the plug and the "PAUSED" text (`cat_moRender` 0x004aafb0, 0x4ab373) [audit: 0x4aba7a is not cat_moRender's: functions.csv puts it in `cat_ioClick` 0x004ab840, the objectives' click ignored while paused (InfoOverlay.c catMoWideScreen): input, not a draw; the conclusion stands].

**So the exe, paused by P, draws the simulation's pose of every object (no tick moves it), and keeps animating:** the
camera float (`rndMainViewRenderFunction` 0x0057659a: its clocks advance unless 0xa6c5b4), the engine glows' random
size jitter (`trailDrawBillboardedSquareThingz` 0x005a5be0: stream 14 every RANDOM_MAX draws, scaled by the frozen
velocity) and the trail segments' random colour shimmer (`trailSurpriseColorAdjust` 0x005a6c30: streams 15-17 every
draw), the camera's own chase (`CameraChase` 0x0042d510, every frame, converging on a still focus) and the HUD's real-time
animations (the over-cap SU blink of `cat_moSupportUnitsDraw` 0x004a7fe0, the upgrade pulse, the ship view, a hit
flash's fade). Under universePause the float stops too.

## (b) The jitter reproduced headlessly, and its cause

**The new HWI-TEST switch `/catMotionPause <frame>:<frames>[:key|univ|nis]`** (MotionTest.c, main.c): in a motion / nis /
scene run, after dump frame `<frame>` the game is paused and `<frames>` dump frames later released: `key` (default) is
the user's P key, an SDL_KEYDOWN / SDL_KEYUP of SDL_SCANCODE_P handed to `main.c HandleEvent` as SDL's loop hands it (the
region callbacks and the key bindings take it from there: the log shows the binding's own `Pause ON` / `Pause OFF`; no OS
input); `univ` sets universePause (what the console / Escape menu set); `nis` calls `nisPause`. Every dump line then ends
with ` pause=UKN` (universePause, the P flag, nisPaused as the frame was drawn) and ` crcv=` (the frame's CRC with the
HUD's blinking over-cap SU value blanked: `InfoOverlay.c catHudSupportBlinkRect`, HWI-TEST, the rect the blink was drawn
in); without the switch the dumps are byte-for-byte unchanged. `/catMotionShot` takes a comma list of frames (HWI-TEST). [audit: in a `/catMotionPause` run the PPM is written after the crcv blanking, so a saved frame shows the over-cap SU value's rect black (MotionTest.c catMotionTestFrameEnd); the 3D view is untouched]

**Before the fix** (`original/scratch-m2/pf-prefix/build-debug/hwi-engine`: c8f97bb's MotionInterp.c / .h and render.c
with this round's test switch; motion mode = the default motion dump's Mission01 scene, 1024x768, 144 fps virtual):

| run | the paused frames (72, F501..F572) | the pause's edges |
|---|---|---|
| `m3r3-pf-pre-key-on` (P, flags on) | **71 of 71** pairs move: eye / look 2.083-2.084 units a frame and **16.67 back** at each tick of the pause (F503->F504), B 1.12 px a frame (8.97 back), C 0.42 (3.45), D 1.47 (11.74), every crc [audit: the C and D figures are the depth column (world units), not px: on screen C moves 0.18 px a frame (1.45 back) and D 0.19 (1.48), from the audit's reverted build m3r3a-pause-freeze-rev-key-on, columns equal to this run] | F500->F501 continues the motion |
| `m3r3-pf-pre-key-noi` (P, /noInterp) | 71 of 71 pairs move by the camera float: A..D up to 0.008 px, every crc [audit: up to 0.016 px on screen (C 0.0157, B 0.0153, D 0.0108, A 0.0016; depth up to 0.10), m3r3a-pause-freeze-rev-key-noi, columns equal to this run] | |
| `m3r3-pf-pre-univ-on` (universePause, flags on) | still (crc moves in 67 of 71 pairs: the SU blink) | entry: eye **+6.25** (F500 13436.24 -> F501 13442.49: the simulation's pose); exit: **-4.17 back** (F572 -> F573 13438.32: the stale lerp until the next tick) |
| `m3r3-pf-pre-univ-noi` | still (crc 67 of 71: the SU blink) | |
| `m3r3-pf-pre-nis-on` (nisPaused, flags on) | still | entry: eye **back** 4259.82 -> 4227.92 (the NIS tick's pose behind the drawn one); exit: +35.8 forward (F673) |
| `m3r3-pf-pre-scene-key` (skirmish LiveFireExercise2, `--scene follow:1500:500:200`, P at F700 for 72: the user's recipe) | **71 of 71**: eye 0.30 a frame, 2.42 back at the wrap; C 5.4 px a frame (43.5 back), A 0.09, H, G, g0, g2, W the same, every crc [audit: C's 5.4 / 43.5 are its depth column (world units: the crossing Recon moves along the view); on screen C moves 0.155 px a frame (1.29 back), A 0.107 (0.86), W 0.74 (6.22, off screen); m3r3a-pause-freeze-rev-scene-key, columns equal to this run] | |

**The cause, in the tree** (src/Game/MotionInterp.c): `miRenderBegin` tested `universePause` only. The P key sets
0x00a6ed74, univUpdate returns before `miCaptureTick` (UnivUpdate.c 9752-9761), so the side table keeps the capture of the
last tick, the objects keep their tick-end transforms, and `taskPeriodFraction(universeTaskHandle)` keeps cycling 0..1
every 62.5 ms because the universe task still runs: **every frame of the pause drew lerp(start of the last tick, end of
it, alpha), replaying the last tick's motion 16 times a second** (the user's "jitter in place" and "hold onto their
travel vector"); the camera's focus centre is computed from the drawn positions inside the draw (ccControl), so the camera
replayed it with them (CameraChase's velocity prediction adds the focus's per-frame move). On top, the exe's camera float
keeps running under P. Under universePause the early return drew the simulation's values: a step of up to a tick at the
pause, and after it the stale lerp until the next capture (a step back).

## (c) The fix: HWI-FEATURE(pause-freeze) in MotionInterp.c

- `miUniversePaused()` (new, MotionInterp.c / .h): `universePause || (catUnivFrameStep && catUnivFrameStepsLeft < 1)`,
  exactly the two conditions under which no univUpdate runs (universeUpdateTask's branch, univUpdate 0x0054c3f0's head).
- **The hold.** In the first paused frame `miRenderBegin` freezes: alpha is held at the last drawn frame's alpha
  (`miLastAlpha`, recorded every frame, 1 when the frame drew the simulation's values), or 0 when a tick's capture ran
  since that frame (the tick's start: what the next frame would have drawn, one frame's step). While frozen every
  object is blended with the held alpha; since no tick runs, the capture and the simulation's values do not change, so
  every frozen frame draws the same pose, the frame before the pause's pose (`F500 == F501`, no step). The missile-trail
  hiding, the clamped children in their host's blended frame and the repair droids use the same alpha.
- **The unpause.** The hold lasts past the unpause until the first tick's capture (`miCaptureTick`). That capture
  carries the held pose into the side table (`miCarryFrozen`: each entry the frozen frames blended gets
  lerp(frozen capture, the simulation's values, held alpha) for its position, its normalised coordinate-system blend,
  its collision and engine positions; new, NIS-driven or teleported objects take the simulation's values as before; the
  droids the same through `miCarryGen`), so the next tick's blend starts from the pose on screen: **no step back and no
  step forward**; the pose catches up with the simulation over that one tick (a speed-up of up to (1 - held alpha) of a
  tick's motion spread over the tick). Against M3-R1's capture rule (the capture records each object at the start of the
  tick, the lerp runs from there): only the first capture after a pause records the pose drawn instead of the
  simulation's; every later capture is M3-R1's.
- Nothing of the simulation changes: the hold and the carry write only the side table and the frame's own write-back
  (miRenderEnd restores every transform bit for bit). With both flags off (`/noInterp`) nothing here runs: the
  simulation's pose is drawn, as the exe draws it.

## (d) The camera float, the engine trails, the NIS

- **The camera float** (render.c `rndMainViewRenderFunction`, HWI-FEATURE(pause-freeze)): its two clocks (`floatTime0/1`)
  no longer advance under `miUniversePaused()` (the exe's tests at 0x0057659a read 0x00a6c5b4 only). The outer test is
  the exe's (the float stays "on" under P): the offsets are recomputed from the held clocks (the same values) and
  `lastFloat0/1` follow the real time, so on the unpause the clocks resume with one frame's step, as if the pause had
  not happened. Under universePause the exe's own rule stands (the float switches off, holds its offsets and restarts its
  clock at the resume frame).
- **The engine trails** (Trails.c, HWI-FEATURE(pause-freeze)): while paused, `trailDrawBillboardedSquareThingz` still
  draws stream 14 at the exe's cadence but keeps the glow size it was drawn with, and `trailSurpriseColorAdjust` still
  draws streams 15, 16 and 17 as the exe does but returns the segment's own colour (no shimmer). So the random streams
  are drawn exactly as the exe draws them; only the per-frame jitter of a frozen velocity is not shown. Every other
  trail state the draw writes (the capital glow's expansion state, trailZeroLength) is a function of the frozen
  simulation and settles in the first paused frame (trailUpdate 0x005a4e10, which counts the expansion down, runs per
  tick: compared with the exe, as the tree, no row).
- **The NIS's own pause** (`nisPaused`, a build with NIS_TIME_CONTROLS; MotionInterp.c miRenderNis): the render time the
  last frame drew (`miNisLastTime`) is held while the NIS is paused (the curves are evaluated at it, the camera too), and
  after the pause the hold lasts until the running NIS's render time passes it (never a step back; a forward step only if
  the unpause lands later in the NIS tick than the pause did). The camera fly-in's nisPaused is not held (it pauses the
  curves to fly the camera in). Before: the NIS's tick pose (a step back of alpha of a tick) while paused, then a jump.


## (e) The fences

All runs: the plain harness (`tools/m2_harness.sh`, offscreen, silent, a fresh home per run), the debug build of this
tree (ASan / UBSan), labels `m3r3-pf-*` in `original/scratch-m2/m2r2-harness/runs/`; the "before" runs used
`original/scratch-m2/pf-prefix/build-debug/hwi-engine` (c8f97bb's MotionInterp.c / .h and render.c with this round's test
switch, `--bin`). Every run below: **ASan errors 0**, no unsuppressed leak. Builds: `tools/build.sh --debug` and
`--release` green with 0 warnings under -Werror; the Windows cross build (llvm-mingw, the options of
tools/release_win64.sh, in `original/scratch-m2/pf-win64`, not build/win64: another agent's) compiles; no _WIN32 code
touched; `tools/m2_layout32.sh` OK.

### (e1) The paused dumps (after the fix)

`pause=` frames are the frames drawn with the pause on; "columns" = every position column of the dump line (eye, look,
up, each object's screen x, y, depth, on-screen flag); `crcv` = the frame's CRC with the HUD's over-cap SU blink blanked.

| run (`m3r3-pf-f-...`) | pause | paused frames | paused pairs with a column change | crcv changes | crc changes | the pause's entry | after the unpause (against the un-paused dump of the same run kind) |
|---|---|---|---|---|---|---|---|
| `pz-key-on` (motion, flags on) | P, after F500, 72 frames | 72 (F501..F572) | **0 of 71** | **0** | 67 (the SU blink) | F501 == F500 in every column | F573..F583 (11 frames) differ: the hold until the next tick (F573-F575, the pose held while the un-paused run moves on: eye 2.08, 4.17, 6.25 units behind), then the carry tick (F576-F583: 5.56 -> 0.69 behind, closing linearly); from F584 eye / look byte-equal through the end; A..D off by 0.0001-0.006 px in 191 later frames (44 above 0.0002 px) [audit: on screen; B's off-screen projection (flag 0, x > 1024) is off by up to 0.036 px in 3 frames (F1156 0.011, F1255 0.036, F1372 0.006)], as with /noInterp below: the camera float's float32 clock |
| `pz-key-noi` (/noInterp) | P, F500, 72 | 72 | **0 of 71** | **0** | 67 | equal | no frame off by more than 0.006 px on screen [audit: B's off-screen projection up to 0.037 px in 2 frames (F1156, F1255); 43 frames above 0.0002] (182 frames 0.0001-0.006 px, 44 above 0.0002: the float's clocks resume exactly, but `cat_taskRealTimeElapsed` is a float32 read 72 frames later, so its deltas round differently); eye / look byte-equal |
| `pz-univ-on` (universePause, flags on) | after F500, 72 | 72 | **0 of 71** | **0** | 67 | equal | F573..F583 as `pz-key-on` (the same hold and carry), then A..D 0.002-0.09 px off in every frame: the exe's own universePause rule for the camera float (it switches off and restarts its clock at the resume frame: one frame of float time lost, rndMainViewRenderFunction 0x0057659a, kept as the exe's); eye / look byte-equal from F584 |
| `pz-univ-noi` | universePause, F500, 72 | 72 | **0 of 71** | **0** | 67 | equal | the same float restart (0.002-0.09 px) only |
| `pz-nis-on` (nis n06 26-36, flags on) | nisPaused, after F600, 72 | 72 (F601..F672) | **0 of 71** | 1 | 1 | F601 == F600 (before: eye 4259.82 -> 4227.92, a step back) | F673 continues F600's motion by one frame (eye 4263.72): with the NIS time aligned (offset 72) **0 of 839 frames** with a column off; crc differs in 803 (the engine trails, below) |
| `pz-nis-noi` | nisPaused, F600, 72 | 72 | **0 of 71** | 1 | 1 | equal | 0 of 839 columns off; crc 811 |
| `pz-key-on-b` (motion, P after F503 for 41: the unpause in the middle of a tick) | P | 41 (F504..F544) | **0 of 40** | **0** | 37 | the pause took effect after a tick: alpha 0 (the tick's start), one frame's step | the hold (F545-F548, a full tick behind: held at the tick's start) and the carry tick (F549-F557, closing); from F557 eye / look byte-equal to the un-paused dump of the same tick and place in the tick; A..D up to 0.1 px: the float's clock is now 5 frames out of phase with the ticks (41 is not a multiple of 9), the expected consequence of a pause of real time |

The one crc change inside the NIS pause (F601 -> F602, columns equal) is the simulation's own: the NIS's first tick in the
pause runs nisUpdate with its 1e-7 s step (NIS_TIME_CONTROLS), which writes each NIS ship's velocity as its position
change over the step, 0, so `trailDraw` stops drawing their engine trails (mag < 0.001); the later frames are identical
again (F602 -> F603 `ppm_region.py diff`: 0 pixels). The trails regrow after the unpause, and the paused frames drew the
trail streams a different number of times (as the exe does), which is the crc difference after the unpause.

**The user's recipe** (skirmish LiveFireExercise2, `--skirmish-ticks 260 --scene follow:1500:500:200`: the camera on a
moving Recon at its zenith, the object kinds of M3-R1's scene; P after F700 for 72 frames): before (`m3r3-pf-pre-scene-key`)
71 of 71 paused pairs moved (eye 0.30 units a frame and 2.42 back at the tick, Recon C 5.4 [audit: depth units; 0.155 px on screen] a frame and 43.5 back, the
mothership's module, the formation, the docking ship, the Worker the same); after (`m3r3-pf-fix2-scene-key`, against
`m3r3-pf-fix-scene-base`): no object column moves [audit: no object moves in the world; its screen columns move with the camera in the same 24 pairs, at most 0.0014 px on screen (A, B), m3r3a-pause-freeze-scene-key]; the camera's eye alternates by one float32 ulp in 24 of 71 pairs
(z 5830.0693 / 5830.0688: CameraChase 0x0042d510, the exe's easing, runs 1 or 2 task ticks a frame at 144 Hz
(`utyNFrameTicks`) and its converged value differs in the last bit; 0.0005 units = at most 0.005 px on screen), crcv moves
only in those 24 (19 pixels, maxdiff 8, `ppm_region.py diff` F703 / F704), the engine glows and trails do not shimmer
(F702 / F703: 0 pixels; before the trail freeze, every paused frame differed by the glow and colour jitter).

### (e2) The default dumps, the self-tests, the streams

| fence | run | result |
|---|---|---|
| motion /noInterp | `m3r3-pf-f-motion-noi` | c503c869f0dc9cdb / 3016280e9e0dd3c8, 1,431 frames: **EQUAL** to `m3r3a-settings-and-keys-motion-noi` (and M3-R1's) |
| motion flags on | `m3r3-pf-f-motion-on` | c0e411662fa2f527 / 428ef41479eb5749, 1,431: **EQUAL** |
| nis /noInterp | `m3r3-pf-f-nis-noi` | 86cb155acffc180e / 196bab32d244f614, 1,440: **EQUAL** |
| nis flags on | `m3r3-pf-f-nis-on` | 0307f3be2044c29c / a5de13a44b8d264f, 1,440: **EQUAL** |
| opt-outs (`--cfg 'letterboxBehind 1' -- /lodBands /noInterp`) | `-motion-optout` / `-nis-optout` | 183681235a3fdc7b / 13ca70eea998e5de: byte-equal to R10's /noInterp dumps, **EQUAL** |
| /meshImmediate twins | `-motion-on-imm` / `-nis-on-imm` | c0e411662fa2f527 / 0307f3be2044c29c: byte-equal to the flags-on dumps |
| motion / nis self-tests | the four default runs | `motiontest: done ... result=PASS` (1,431 / 1,440 frames sampled) |
| toggle self-test | `-toggle`, `-toggle-noInterp`, `-toggle-noNISInterp`, `-toggle-letterboxBehind`, `-toggle-lodBands`, `-toggle-meshImmediate`, `-toggle-noFilter`, `-toggle-overlay`, `-toggle-sharpTextures` | `toggletest: done ... fails=0 result=PASS` in all nine (start 1/1, 0/0, 1/0, the rest 1/1) |
| headless Mission01 (`--mission-ticks 6000`) | `-mission-a`, `-mission-b` | f5064299e536486c, 1,476,370 B, both **IDENTICAL** in every field over ticks 0-6000 to `m3r1i-mission-a/run.hwo` (sha256 1ae66610..., 6,001 T records) and to each other (hwo_compare: "IDENTICAL: 6001 ticks") |
| missiontest Mission01 NIS-first | `-missiontest-a`, `-missiontest-b` | `done mission=1 ticks=11424 objectives=6/6 result=PASS`; both **IDENTICAL** to `m3r2i-e-missiontest-a` (11,426 ticks) |
| fe | `-fe` (--timeout 120) | milestone Main_game_screen, still running at the timeout (as the audit's), ASan 0 |
| sp | `-sp` (--timeout 1200) | reached **Mission12** (the screenshot loop, as the audit's), 23,289 file loads, still running at the timeout (rc 124, as every sp run), **ASan 0** |

**Streams and dumps moved: none.** No stream moved (presentation only; no simulation field, timer or flag is written by
the change: the freeze writes the side table and the frame's own write-back, the trail change only what is drawn, and
the random streams are drawn as the exe draws them). No default dump moved (no pause in their scenes). The new paused
dumps are new measurements, not baselines.

## Files changed (git diff c8f97bb)

- **src/Game/MotionInterp.c / .h** (HWI-FEATURE(pause-freeze)): `miUniversePaused()`; the hold (`miFrozen`,
  `miFrozenAlpha`, `miFrozenGen`, `miLastAlpha`, `miLastGen`) in `miRenderBegin`; the carry (`miCarryFrozen`,
  `miCarryGen`) in `miCaptureTick` and `miDroids`; the NIS hold (`miNisFrozen`, `miNisFrozenTime`, `miNisLastTime`,
  `miNisLast`, NIS_TIME_CONTROLS only) in `miRenderNis`; the file's header comment.
- **src/SDL/render.c** `rndMainViewRenderFunction` (HWI-FEATURE(pause-freeze)): the camera float's clocks hold under
  `miUniversePaused()`.
- **src/Game/Trails.c** `trailDrawBillboardedSquareThingz`, `trailSurpriseColorAdjust` (HWI-FEATURE(pause-freeze)): the
  glow size and the segment colours hold while paused; the streams are drawn as before.
- **src/Game/MotionTest.c / .h**, **src/SDL/main.c** (HWI-TEST): `/catMotionPause`, the ` pause=` / ` crcv=` tokens,
  `/catMotionShot` frame lists.
- **src/Game/InfoOverlay.c / .h** (HWI-TEST): `catHudSupportBlinkRect`, where `cat_moSupportUnitsDraw` drew the
  blinking SU value this frame (only read by the measurement).
- **docs/features/motion-interpolation.md**: item 7, the pause. **tools/m2_harness.sh**: header only (the motion / nis
  modes' `-- /catMotionPause` documented; no code change). **docs/m3-r3-pause-freeze.md**: this report.

Markers: 86 lines `HWI-FEATURE(pause-freeze)` (MotionInterp.c / .h, render.c, Trails.c), 35 added `HWI-TEST` lines [audit: 34 in src/ (36 with this report's and the harness header's mentions)]; the audit tagged 4 more Trails.c lines (90 lines). No
CAT line, no CAT-FIX, no CAT-TODO added or closed (the one CAT-TODO naming M3-R3, BTG.c:1151's star size, is the
integrate's).

**The delta map.** The exe functions edited (`rndMainViewRenderFunction` 0x005764e0, `trailDrawBillboardedSquareThingz`
0x005a5be0, `trailSurpriseColorAdjust` 0x005a6c30, `cat_moSupportUnitsDraw` 0x004a7fe0) are MODIFIED already; the
IDENTICAL bodies relied on were compared with the exe and found as the tree, so **no review row**: `nisPause` 0x004ce5f0
(`_DAT_00a4cac8 = bPause`), `trailUpdate` 0x005a4e10 (the per-tick expansion count, the grain counter),
`rndShamelessPlug` 0x00579ff0 (no time or random term: a still texture while paused), `ccControl` 0x0042e7c0 (M3-R1's
comparison stands). `catKbPause` 0x00550640 (no row of its own in functions.csv: read from the disassembly, as the
tree's KeyBindings.c).

## New and extended self-tests, switches, tools, harness flags

- **`/catMotionPause <frame>:<frames>[:key|univ|nis]`** (HWI-TEST, MotionTest.c, main.c): a scripted pause in a motion,
  nis or scene run (no new harness flag: pass it after `--`; documented in the harness header's motion and nis modes).
  The log gets `motiontest: pause on|off kind=.. frame=.. tick=..`; for `key` also the binding's own `Pause ON` / `Pause
  OFF`. Dump lines gain ` pause=UKN crcv=XXXXXXXX` only with the switch.
- **`/catMotionShot <frame>[,<frame>..]`** (HWI-TEST): up to 9 frames saved as PPM in one run (one number: as before).
- No new harness mode, setting, cfg row, console row, settings-line row, key or control: the pause freeze has no setting
  (the user wants a still pause; the exe's moving bits are not something the user asked to keep).

## THE USER'S CHECK (for the play-test brief)

`tools/play.sh` (it rebuilds the release binary when it is older than the newest src/ commit; Smooth motion on, the
default). Any mission with moving ships, e.g. **Mission02 or Mission03 with the fleet moving** (or a skirmish): focus the
camera on a moving ship (or a moving group), zoom in so the ships fill the view, then press **P** (the game's Pause key;
"PAUSED" appears). Look at the moving ships, their engine glows and trails, and the background while paused for a few
seconds; then press **P** again. Also open the **Escape** menu (universePause) over moving ships and close it.
Report: (1) while paused, does anything in the 3D view still move or shimmer (ships, the camera, engine glows, trails)?
(the HUD's blinking red SU number when over the unit cap, the selected ship's turning picture and the upgrade pulse
keep animating, as in the original); (2) on the unpause, do the ships jump, step back, or just resume (a brief catch-up
of under a tenth of a second is expected); (3) the same in the Escape menu. Compare, if wanted, with `tools/play.sh --
/noInterp` (Smooth motion off): the ships stand still there too, the camera float and the glows likewise.

## For STATE.md

- **Decision (proposed): HWI-FEATURE(pause-freeze)**, always on, no setting: while the universe is paused
  (`miUniversePaused()` = universePause 0x00a6c5b4, or the P key's flag 0x00a6ed74 with no frame step left) every object
  is drawn at the pose the last frame drew (the lerp's alpha held), the camera float's clocks hold (the exe stops them
  for universePause only), the engine glows keep their size and the trail segments their colour (the random streams
  drawn as the exe draws them), and the NIS's own pause (nisPaused, NIS_TIME_CONTROLS builds) holds the NIS render
  time; the first capture after a pause starts the next tick's blend from the held pose (no step at the pause or at the
  unpause). The HUD's real-time animations (the SU blink, the upgrade pulse, the ship view, a hit flash's fade) run on
  as in the exe. The exe's universePause float restart (one frame of float time) is kept.
- Names: `miUniversePaused`, `miFrozen`, `miFrozenAlpha`, `miFrozenGen`, `miLastAlpha`, `miLastGen`, `miCarryFrozen`,
  `miCarryGen`, `miNisFrozen`, `miNisFrozenTime`, `miNisLastTime`, `miNisLast`; HWI-TEST `/catMotionPause`, the ` pause=`
  and ` crcv=` dump tokens, `catHudSupportBlinkRect`, `/catMotionShot` frame lists.
- The user's pause is the P key (`Pause 80`), the exe's frame-step flag, not universePause; the Escape menu and the
  console set universePause; an NIS cannot be paused by the user (P is refused under the letterbox; universePause does
  not stop the NIS task).

## What is left (owners)

- The camera chase's 1-ulp alternation at a converged zenith camera (0.005 px, the exe's easing; seen only in the scene
  measurement): not a defect; no owner.
- A wall-clock (real-clock) capture of a pause was not run (the logic does not depend on the clock: the held alpha is
  whatever the last frame drew, above 1 included); if the user still sees motion while paused, the owner is the next fix
  round with `--real-clock 144` and `/catMotionPause`.
- USER question: the exe keeps the camera's gentle float and the engine glows' shimmer going while P pauses the game;
  the recreation now holds them still (the Escape menu already stopped the float in the exe). Keep the still pause (the
  default now), or bring the exe's look back behind a setting?
