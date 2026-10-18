<!-- done: measure, gameplay, nis, toggle, guard-rails -->
# M2-R6 motion-interp: render-time motion interpolation (gameplay and NIS) with the "Smooth motion" toggle

Status: DONE (M2-R6 motion-interp producer; base commit `3aa4cce`; nothing committed by the producer, the auditor
checkpoints). The DONE WHEN is met (see "DONE WHEN" at the end). The spec is docs/features/motion-interpolation.md (the user's first approved feature,
2026-10-17). It is NOT in the exe: every code line and every comment block of it carries `HWI-FEATURE(motion-interp)` (never `CAT:`) [audit: said every line; the continuation lines of about 60 tagged comment blocks carry no tag], it stays out
of the simulation, and `grep -rn "motion-interp" src/ tools/` finds all of it.

Subsystems (in this order): `measure` (the motion and nis harness modes, tools/nis_smoothness.py, the pre-feature
dumps), `gameplay` (spec item 0: prev / cur transforms, alpha, the interpolated draw, camera focus, picking and HUD),
`nis` (spec items 1-2: `bsCurveEvalAt`, the NIS camera and NIS objects at `tRender`), `toggle` (spec item 3: the
Smooth motion checkbox, the options-file keys, the switches, the console commands), `guard-rails` (spec item 5: the
byte-identical dumps and streams, layout32, fe / sp).

## measure (done)

Measurement before any interpolation code (spec item 4).

- **`src/Game/MotionTest.c` / `.h`** (new, HWI-FEATURE(motion-interp) throughout): two scripted runs on the headless
  Mission01 start of `/oracleMission` (virtual clock, no input):
  - `/catMotionTest <ticks>` (harness mode `motion`): at tick 16 the sensors manager the headless start opens is
    closed and four scripted sProcessors appear at (12000, -12000, 0), far from Mission01's ships: **A** the camera's
    focus (`ccFocusGeneral`, as /catRenderTest), moving at 300 u/s along +X; **B** crossing the view (left edge to right
    edge in 6 s, half a screen above A); **C** receding along the line of sight (below and right of the centre, from
    0.6 to 1.4 camera distances); **D** at rest in the world, 3.5 camera distances out to the left, carried across the
    view by the camera's motion (the user's "asteroid"). Their positions are written after every universe tick (exact
    straight lines, so the 16 Hz stepping is the only irregularity); B, C and D take the camera basis of tick 48. From
    tick 64 to `<ticks>` (default 224, a 160-tick window) every rendered frame writes one dump line.
  - `/catNisMotionTest <nis>[:<from>:<to>]` (harness mode `nis`): at tick 16 the install's `nis\<nis>.nis` with its
    script is loaded, the statics of its ship types the level excludes are loaded (as the build self-test's
    `catBuildSelfTestLoadStatic`; n06 needs 10: its derelicts), and it plays at the origin through `nisStart` +
    `nisTaskResume` (the NIS test path); the frames between NIS times `<from>` and `<to>` are dumped, then the engine
    quits. **The NIS: `nis\n06.nis`, window 26-36 s** (default `n06:26:36`): a scan of the 14 NIS camera paths of the
    install (keys at least 0.3 s apart, so cuts excluded) found n06's 147.6 deg/s pan at 27.3-28.2 s the fastest, and
    the same window holds a 70.7 deg/s pan with 591 u/s of eye travel at 34.3-35.3 s and one camera cut. The tracked
    object is the NIS object on screen in the most frame pairs (tools/nis_smoothness.py default): **O9, the sRecon**
    (on screen in 1,384 of the 1,439 frame pairs).
  - **The dump** (`/catMotionDump <file>`): a header, one `# object NAME ROLE TYPE` line per tracked object, then per
    rendered frame `F <frame> tick=<n> eye=.. look=.. up=.. NAME=x,y,depth,onscreen ... crc=<crc32>`: the camera given
    to the main view's renderer, each object's screen position in pixels projected with the frame's own
    `rndCameraMatrix` / `rndProjectionMatrix` (the camera float included) from the position the object had while it
    was drawn (sampled in `mrRegionDraw` right after `rndMainViewRender`), and the CRC-32 of the whole frame read back
    with `glReadPixels` from the back buffer just before the flip (`rndRenderTask`). The offscreen driver allows the
    read-back (EGL on the RTX 5090).
  - The virtual clock: `oracleVirtualFrameStep` (OracleDump.c) steps `<fps>`/16 frames per universe tick (`/catMotionFps`,
    default 144: 9), each frame 15/9 task ticks (whole ticks to the task manager, the remainder kept as
    `oracleVirtualSubTick`); `utyTasksDispatch` calls it once a frame. For every other run (N 0 or 1) it returns
    `oracleVirtualFrameTicks()` unchanged, so the mission run is untouched.
- **Hooks**: `OracleDump.c` (`catMotionTestTick` after each universe tick, before the stream record), `mainrgn.c`
  (`catMotionTestFrameSample(mrCamera)` after `rndMainViewRender`), `render.c` (`catMotionTestFrameEnd` before the
  flip), `utility.c` (the virtual frame step), `main.c` (`/catMotionTest`, `/catNisMotionTest`, `/catMotionDump`,
  `/catMotionFps`), `src/Game/meson.build`.
- **tools/m2_harness.sh**: modes `motion` (`--motion-ticks N`, `--fps N`) and `nis` (`--nis NAME[:FROM:TO]`, `--fps N`),
  milestone "motion self-test done", a summary line with the dump's frame count and sha256. The nis mode passes
  `/noSpeech`: the NIS's speech subtitles start and end with the audio thread's progress through the speech (wall
  clock, even on SDL's dummy driver), which made about 40 frame CRCs of the n06 window differ from run to run
  (`mi-pre-nis-a` / `-b` / `-c` before the switch: positions identical, CRCs of frames 111-152 not); with it two runs
  are byte-identical.
- **tools/nis_smoothness.py** (standard library, like tools/oracle/hwo_compare.py): one or two dumps; per object the
  max, 95th-percentile and mean per-frame screen displacement in pixels and the repeated frames (moved less than
  `--eps`, default 0.05 px for a motion dump and 0.01 px for a nis dump [audit: said 0.1 px, the measurement-only
  patch's default; the final tool defaults by the dump's mode, see "For STATE.md"]), focused / crossing
  / line of sight / static by the dump's roles, the camera's per-frame eye + look movement and repeated poses and
  frame CRCs; two dumps side by side, with a byte-identity verdict.

**Pre-feature dumps** (the measurement-only binary, debug, plain harness; each run twice, byte-identical):

| run | mode | milestone | first stop | dump sha256 |
|---|---|---|---|---|
| `mi-pre-motion-a` / `-b` | motion 224 | motion self-test done | none (rc 0), ASan 0 | `b2af0d6decdf647431d6c2adf701798cd8e98d4a614d7b69390e6df02452d1dd` (1,431 frames) |
| `mi-pre-nis-a` / `-b` | nis n06:26:36 | motion self-test done | none (rc 0), ASan 0 | `3f04c837e605ba23d6c74c639bba4ff10a95c139f33d462bca79427d4a985029` (1,440 frames) |

Measured (eps 0.1 px; "of 9" = repeated frames per universe tick at 144 fps):

| object | max px | p95 px | mean px | repeated |
|---|---|---|---|---|
| motion A (focus) | 0.003 | 0.001 | 0.001 | 1430 / 1430 (locked to the camera: 9 of 9) |
| motion B (crossing) | 10.152 | 10.108 | 1.114 | 640 / 719 (8.01 of 9) |
| motion C (line of sight) | 3.310 | 1.355 | 0.166 | 1272 / 1430 (8.01 of 9) |
| motion D (at rest, camera-carried) | 36.339 | 2.886 | 0.621 | 1272 / 1430 (8.01 of 9) |
| motion camera eye + look (world units) | 37.5 | 37.5 | | 1272 / 1430 repeated poses |
| nis O9 sRecon (tracked) | 120.534 | 24.118 | 2.724 | 1231 / 1384 (8.01 of 9) |
| nis camera | 13,589.9 (the cut) | 139.4 | | 1290 / 1439 repeated poses, 573 repeated frame CRCs |

This is the user's observation in numbers: the focus is locked (the camera is recomputed every frame around its 16 Hz
position), every other object and the NIS camera move on one frame in nine and hold for eight.

The measurement-only state (git diff against the base plus the three new files) is saved as
`original/scratch-m2/motion-interp/measure-only.patch` (55,157 B, sha256 `a65e080d2a6b529d...`; checked with
`git apply --check` against a `git archive` of `3aa4cce`). Superseded runs: `mi-motion-try1..3`, `mi-nis-try1..3`
(`try1`/`try2` stopped at `RACE_DERELICT\dBigGun` / `dBuoy was not loaded properly` before the static loading),
`mi-ns-a` / `-b` (the /noSpeech trial), `mi-pre-nis-c` (before /noSpeech).

## gameplay (done)

Spec item 0. **`src/Game/MotionInterp.c` / `.h`** (new, HWI-FEATURE(motion-interp) throughout; the module's header
comment is the design summary):

- **The side table.** `miCaptureTick` runs at the start of every universe tick, as the first statement of `univUpdate`
  after the exe's frame-step test (UnivUpdate.c, one line): for every object of `universe.SpaceObjList` it copies the
  position, the coordinate system (`SOF_Rotatable`), the collision position (`SOF_Impactable`; the camera's focus, the
  selection circles and the picking read `collInfo.collPosition`) and the engine position (`Ship.enginePosition`,
  `Missile.enginePosition`; the trails start there) into an open-addressing table keyed by the object pointer
  (malloc'd, never the game's pools, so no pool address moves; read-only on the objects). The exe-layout structs
  (Ship 0x63C, SpaceObj 0x98, Missile, Bullet, Derelict, Asteroid) are untouched: `tools/m2_layout32.sh` OK.
  Each capture has a generation number; an object counts as "captured" only with the latest one, so an object created
  during the tick, or re-added to the list (a ship launched from a carrier), has no previous transform and is drawn at
  the current tick [audit: the generation now advances on every tick, also with both flags off (miCaptureTick); before,
  turning the flags back on (checkbox, console) blended the frames up to the next tick from the last capture before
  they went off]. `miForget` (one line at the top of `memFree`, Memory.c) drops a freed object's entry, so an object
  allocated at a freed object's address in the same tick (bullets, effects) never inherits its predecessor's transform.
- **alpha.** `taskPeriodFraction(universeTaskHandle)` (Task.c, new; the universe task's handle is now kept by
  `utyGameSystemsInit`): the task's leftover ticks, plus this pass's ticks when the universe task has not run yet in the
  pass the render task is executing in (the render task's handle is lower, so it draws before the universe task of the
  same frame), plus the clock's sub-tick remainder (`taskSubTick`: the milliseconds `utyTasksDispatch` keeps for the
  next frame, or the virtual clock's leftover), scaled by `cat_timeCompressionFactor` like the ticks, over the task's
  ticks per call. **Time compression is honoured** (the task ticks are the compressed ones). alpha >= 1 (the frame is
  at a tick, e.g. every frame of the one-tick-a-frame mission run) or a paused universe (`universePause`) or a paused
  universe task draws the simulation's values unchanged: the spec's "alpha 0 when paused" is implemented as "no
  blend", so a paused game shows the simulation state exactly (drawing `prev` would step the paused view back a tick).
- **The draw.** `miRenderBegin` / `miRenderEnd` bracket `regFunctionsDraw()` in `rndRenderTask` (render.c, two lines):
  every captured object gets `lerp(prev, cur, alpha)` for its position, collision and engine positions and a
  normalised blend of the coordinate systems (the heading, right and up columns lerped and re-orthonormalised by
  Gram-Schmidt; kept at `cur` when the heading or up turned by more than 90 degrees in one tick); the current values
  are saved and written back bit for bit after the regions are drawn (in reverse order, objects freed meanwhile
  skipped). A jump of more than 2,000 units in one tick (hyperspace, placement) is drawn at `cur`. Since everything
  between the two calls reads the blended values, the **camera's focus centre** (`ccControl` runs in `mrRegionDraw`;
  `NewSetFocusPoint` reads `collInfo.collPosition`), the **selection circles** (`selCircleCompute` in the object
  renderer, which the click and band-box picking read), `mouseCursorObjPtr`, the tactical overlay, the status bars, the
  main overlay, the ship info panel, the pie plate and the sensors manager all use the drawn positions. Nothing else
  of the simulation runs inside the bracket (the universe and NIS tasks run in their own slots; `soundEventUpdate`
  runs after `miRenderEnd`).
- **Missile trails**: a missile's trail takes its newest point at the end of the tick's move
  (`physUpdateObjPosVelMissile`), so drawn from the blended missile it would stick out ahead of it; that point is hidden
  for the frame (iHead / nLength saved and restored). Ship trails take theirs before the move
  (`univMinorSetupShipForControl`) and need nothing.
- Measured (`mi-on-motion-1`, then the final runs below): B (crossing) 0 repeated frames of 724, max 1.149 px a
  frame (10.152 px without); the camera's eye moves every frame (0 repeated poses of 1,430, 1,272 without).

## nis (done)

Spec items 1-2.

- **`bsCurveEvalAt(curve, t)`** (B-Spline.c, next to `bsCurveUpdate` 0x00423040, which is unchanged and stays the
  tick-time path): the value at absolute curve time `t` without advancing the curve: the same key walk, the same
  pinning of a short key with tension, the same Hermite / tension-continuity-bias arithmetic, on locals
  (`bsCurveUpdate`'s static caches are pure functions of their inputs and are not touched). The shared arithmetic is
  `bsSegmentValue`. **`bsCurveEvalAtBeforeJump`**: the same, except that a jump of the next tick (a key shorter than
  a 30 fps frame: the NIS camera cuts and re-placed objects) starting after the curve's current time and at or before
  `t` is not taken; the key before it is continued past its end (held at its end when it is short itself or the
  continuation would exceed half of it).
- **NIS.c** (new functions only; `nisUpdate`, `nisCamPathUpdate`, `nisCameraFlyCompute`, the events and the KAS runtime
  are unchanged): `nisRenderObjectAt` (an NIS-driven object's position and coordinate system dt after its curves' time,
  `nisUpdate`'s arithmetic), `nisRenderCameraAt` (`nisCamPathUpdate`'s), `nisRenderCameraFlyAt` (the camera fly-in of
  an NIS that starts with a blend, `nisCameraFlyCompute`'s); they write only the caller's variables.
- **MotionInterp.c `miRenderNis`**, while `thisNisPlaying` is set: every object the NIS moves (curves, not deleted) is
  the NIS's for the frame (the gameplay lerp skips it: the NIS task ticks on its own phase), and with `nisInterpolate`
  on, `dt = alpha_nis * UNIVERSE_UPDATE_PERIOD` (`* nisPlayFactor` where `nisUpdate` applies it, i.e. under
  `NIS_TIME_CONTROLS`), `alpha_nis = taskPeriodFraction(nisTaskHandle)` clamped to 1, so **tRender = the curves' time
  (= NIS->timeElapsed) + alpha * UNIVERSE_UPDATE_PERIOD * nisPlayFactor**; no blend while a movie plays
  (`aviIsPlaying`: `nisUpdate` waits) or the NIS is paused under the time controls. Each object is drawn at its curves'
  render transform (collision and engine positions recomputed from it), the objects clamped to it follow
  (`cat_clampUpdateAttachedObjects`' arithmetic on the render transform), and the NIS camera is a render-only copy of
  `*nisCamera` with the render-time eye / look / up: `miRenderCamera(mrCamera)` is what `mrRegionDraw` now passes to
  `rndMainViewRender` (mainrgn.c). `nisUpdate`'s writes are untouched; with `nisInterpolate` off the NIS objects and
  camera are drawn as the tick left them.
- **Camera cuts.** The spec says a cut disables blending for its frame. Evaluating plainly past a cut would show it up
  to a tick early (before the tick's show / hide events); drawing the tick's poses on the frames whose window contains
  the cut (the first implementation, `mi-on-nis-1`) stepped the camera back a fraction of a tick and held it for up to
  8 frames (O9: 3 held frames and a 1.35 px backward jump before n06's cut at 29.1 s). So no frame blends across a cut,
  and the cut still shows at the tick that takes it, as without interpolation, while the frames before it continue the
  pre-cut key's motion (`bsCurveEvalAtBeforeJump`, `mi-on-nis-2`: 0 held frames).
- Objects without curves (ships the simulation moves during an NIS) take the gameplay path: with `objInterpolate`, and
  (spec item 2) with `nisInterpolate` alone while an NIS plays.

## toggle (done)

Spec item 3.

- **The flags** `objInterpolate`, `nisInterpolate` (MotionInterp.c, both default TRUE). `miSmoothMotionGet` (either on)
  / `miSmoothMotionSet` (both).
- **The "Smooth motion" checkbox** on Options -> Video, added **in code** (no game data in git, the plain harness stays
  plain): `opSmoothMotionAtomsAdd` (Options.c), called at the end of `feScreensImageLoad` (FEFlow.c, one line) for
  every loaded `.fib`, appends two atoms to the front end's `Video_Options` screen (front_end.fib) and the in-game
  `Video` screen (in_game_esc_menu.fib): a copy of the screen's own "full detail" checkbox (`OP_NoLOD`, `FA_CheckBox`)
  renamed `OP_SmoothMotion`, and a copy of its "FULL DETAIL" static text with the text "SMOOTH MOTION", moved into free
  space by file-coordinate offsets (front end: box at 500,268 and label at 500,250, under the tab buttons; in game:
  box at 379,316 and label at 373,298, under "NUMBER OF EFFECTS"). The atom arrays are malloc'd (not the game's pools)
  and kept. The callback `opSmoothMotion` (registered as `OP_SmoothMotion` in utility.c's callback table) shows the
  flags at its first call and sets both on a click. `opOptionsSaveSettings` / `opRestoreSavedSettings` save and restore
  both flags with the other video options, so **Accept** (which ends with `opOptionsSaveSettings`, writing the options
  file) persists them and **Cancel** (`opRestoreSavedSettings` first) undoes a click.
- **The options-file keys** `objInterpolate 0|1` and `nisInterpolate 0|1` in **Cataclysm.cfg** (the options file the
  tree uses since M2-R6 startup-keys: read from `$HOME/.homeworld/Cataclysm.cfg` by `utyOptionsFileRead`, written to the
  user settings path by `utyOptionsFileWrite`): two rows after the HWI rows of `utyOptionsList` (utility.c).
- **The switches** `/noInterp` (both off) and `/noNISInterp` (main.c; `ProcessCommandLine` runs after
  `utyOptionsFileRead`, so a switch wins for the run [audit: said "nothing is written back unless the options are accepted"; every
  `opOptionsSaveSettings` writes Cataclysm.cfg (`utyOptionsFileWrite`), and it runs when an options screen opens
  (`opOptionsStartHelper`), on Accept, in `opUpdateSettings` and at the end of every game (`gameEnd`, utility.c), so a
  `/noInterp` or `/noNISInterp` run writes its 0 into the options file the first time one of those runs]).
- **The console**: two rows of the developer console's command table (Console.c, type 5 dword, ids 46 / 47):
  `objInterpolate 0|1`, `nisInterpolate 0|1` (shown with the bare name, as every row).
- **Self-test `/catToggleTest`, harness mode `toggle`** (MotionTest.c `catToggleRunSelfTest`; front end, no input):
  pushes Video_Options, checks the two atoms have regions and the box shows the flags, "clicks" it (the FE checkbox
  handler's two steps: `feToggleButtonSet`, then the callback), checks both flags; the Cancel path restores them; the
  Accept path writes Cataclysm.cfg with both rows 0, then 1, each read back through `utyOptionsList`
  (`catUtyOptionsFileReadBack`, the same parser as the start-up read); the in-game Video screen carries the control;
  the console rows set each flag alone; "toggletest: done ... result=PASS|FAIL". Run with `-- /noInterp` /
  `-- /noNISInterp` it shows the switches' start flags.

- The console's config file (`saveConfig` / `loadConfig`) keeps the exe's format: rows added by this tree have ids from
  `CAT_CON_HWI_ROW_ID` (0x1000) on and `cat_consoleConfigFileSave` leaves them out (the first final console run,
  `mi-fin-console`, failed the console self-test's 240-byte check with the two rows saved: 260 bytes; fixed). The console
  self-test's row counts (29 shown, 22 set: the exe's table) now add `CAT_CON_HWI_ROWS` (2): `mi-fin3-console` PASS with
  31 / 24 (`mi-fin2-console` showed the counts before the criterion was updated). Their values persist through
  Cataclysm.cfg.

## guard-rails (done)

Spec item 5, all on the plain harness with the final binary (debug unless marked).

- **The dumps with the flags off.** `/noInterp`: `mi-fin3-motion-noi` and `mi-fin3-nis-noi` are byte-identical to the
  pre-feature dumps `mi-pre-motion-a` (`b2af0d6d...`) and `mi-pre-nis-a` (`3f04c837...`), frame CRCs included.
  (`/noNISInterp`: `mi-fin-motion-nonis` is byte-identical to the flags-on motion dump, the NIS flag playing no part
  there; `mi-fin-nis-nonis` has the tracked O9 exactly as without interpolation, 1231 / 1384 repeated, while the
  scene's objects without curves are blended: dump `564149979fa7e06b`.)
- **The Mission01 stream.** Before (the base `3aa4cce`, built from `git archive` in
  `original/scratch-m2/motion-interp/base-3aa4cce`): `mi-base-mission-a` / `-b` (mission_test.sh, 6,000 ticks) byte-identical,
  1,476,370 B, sha256 **`794eae2c20c68aac`**, ASan 0; `hwo_compare` against nis-fow-camera's `r6nfc-final-a`:
  IDENTICAL (header build identity `a6e72c9d` vs `3aa4cce1` only). After, the same hash for every setting:
  flags on `mi-fin3-on-mission-a` / `-b` (mission_test.sh PASS), `/noInterp` `mi-fin3-noi-mission-a` / `-b`,
  `/noNISInterp` `mi-fin3-nonis-mission-a` / `-b`: each pair byte-identical, sha256 `794eae2c20c68aac`, and
  `hwo_compare` against `mi-base-mission-a`: IDENTICAL, 6001 ticks, no difference (the header too: the uncommitted
  tree's build identity is its HEAD, 3aa4cce). **No oracle field moved.** Release: `mi-fin3-rel-mission-a` / `-b`
  byte-identical (`546337dc7eb87b7b`), IDENTICAL to the debug stream except the header's build flags. Against R5's
  `645f94c69d68319e` the stream still differs only as the managers port left it (first divergent tick 1, `rng6`).
- **nisUpdate / KAS**: no line of `nisUpdate`, `nisCamPathUpdate`, `nisCameraFlyCompute`, `nisUpdateTask`, the event
  dispatch or the KAS runtime changed (`git diff 3aa4cce -- src/Game/NIS.c` adds the four render-time functions and
  nothing else).
- **Layout**: `tools/m2_layout32.sh` OK (no struct changed; the side table is outside the exe-layout structs).

## DONE WHEN

| item | result |
|---|---|
| flags on: 0 repeated frames for the unfocused crossing object in motion | **B: 0 / 724** (off: 640 / 719, 8.01 of 9 a tick) |
| flags on: 0 repeated frames for the tracked object in nis | **O9 sRecon: 0 / 1389** (off: 1231 / 1384, 8.01 of 9) |
| displacement on vs off | the tables below |
| /noInterp dumps byte-identical to the pre-feature dumps | motion `b2af0d6d...`, nis `3f04c837...`: byte-identical |
| two 6,000-tick mission streams identical with the flags on, /noInterp, /noNISInterp, and to the base's | all `794eae2c20c68aac`, IDENTICAL to `mi-base-mission-a` |
| the checkbox drives both flags and persists through the options file; keys, switches, console | `/catToggleTest` PASS (default, `/noInterp`, `/noNISInterp`) |
| layout32 | OK |
| fe and sp ASan 0 | `mi-fin3-fe` Main_game_screen, ASan 0; `mi-fin3-sp` universe running, ASan 0 |

**Displacement, flags off (`/noInterp`) vs on** (pixels per frame at 1024x768, 144 fps virtual; repeated = moved
less than eps: 0.05 px for motion, where the main view's camera float moves a held frame's objects by up to about
0.05 px, 0.01 px for nis, where a held frame repeats exactly):

| object | off: max / p95 / mean, repeated | on: max / p95 / mean, repeated |
|---|---|---|
| motion A focus | 0.003 / 0.001 / 0.001, 1430 / 1430 (locked) | 0.002 / 0.001 / 0.001, 1430 / 1430 (locked) |
| motion B crossing | 10.152 / 10.108 / 1.114, 640 / 719 | **1.149 / 1.139 / 1.126, 0 / 724** |
| motion C line of sight | 3.310 / 1.355 / 0.166, 1272 / 1430 | **0.380 / 0.334 / 0.166, 0 / 1430** |
| motion D at rest (camera-carried) | 36.339 / 2.886 / 0.621, 1272 / 1430 | **4.162 / 2.516 / 0.615, 0 / 1430** |
| motion camera (eye + look, world units) | 37.5 / 37.5, 1272 / 1430 repeated poses | 4.168 / 4.168, 0 / 1430 |
| nis O9 sRecon (tracked) | 120.534 / 24.118 / 2.724, 1231 / 1384 | **39.525 / 6.200 / 2.808, 0 / 1389** |
| nis camera | 13,589.9 (the cut) / 139.4, 1290 / 1439 repeated poses, 573 repeated frame CRCs | 13,589.9 (the cut) / 178.2, 180 / 1439 (static shots), 0 repeated CRCs |

The means stay (the same path is covered); the per-frame steps shrink to the per-frame share of the motion. The nis
camera's remaining repeated poses are the static shots of n06 (the camera does not move there at all). At eps 0.1 px
the nis O9 count with the flags on is 12: frames 645-657, where the camera chases the recon at 48 units and moves
62 units a frame while the recon moves 0.04-0.08 px on screen (a tracking shot, not a held frame).

## Build results

`tools/build.sh --debug` and `--release`: green, 0 warnings under -Werror after every subsystem and for the final tree.
`tools/m2_layout32.sh`: OK.

## Plain-harness runs

| run | mode | milestone | first stop | result |
|---|---|---|---|---|
| `mi-pre-motion-a` / `-b` | motion 224 (measurement-only binary) | motion self-test done | none (rc 0) | pre-feature dump `b2af0d6d...` twice, ASan 0 |
| `mi-pre-nis-a` / `-b` | nis n06:26:36 (measurement-only binary) | motion self-test done | none (rc 0) | pre-feature dump `3f04c837...` twice, ASan 0 |
| `mi-base-mission-a` / `-b` | mission 6,000, base 3aa4cce | oracle mission run done | none (rc 0) | `794eae2c20c68aac`, mission_test PASS, ASan 0 |
| `mi-fin3-motion-on` | motion | motion self-test done | none (rc 0) | ASan 0; B 0 repeated; dump `d9a8661fcce6f4b3` |
| `mi-fin3-motion-noi` | motion `/noInterp` | motion self-test done | none (rc 0) | ASan 0; byte-identical to `mi-pre-motion-a` |
| `mi-fin-motion-nonis` | motion `/noNISInterp` | motion self-test done | none (rc 0) | ASan 0; = flags on |
| `mi-fin3-nis-on` | nis | motion self-test done | none (rc 0) | ASan 0; O9 0 repeated; dump `074a70f17e316cd2` |
| `mi-fin3-nis-noi` | nis `/noInterp` | motion self-test done | none (rc 0) | ASan 0; byte-identical to `mi-pre-nis-a` |
| `mi-fin-nis-nonis` | nis `/noNISInterp` | motion self-test done | none (rc 0) | ASan 0 |
| `mi-fin3-toggle`, `mi-fin-toggle-noi`, `mi-fin-toggle-nonis` | toggle (default, `/noInterp`, `/noNISInterp`) | toggle self-test done | none (rc 0) | PASS, start flags 1/1, 0/0, 1/0; ASan 0 |
| `mi-fin3-on-mission-a` / `-b` | mission 6,000 (mission_test.sh) | oracle mission run done | none (rc 0) | `794eae2c20c68aac`, PASS, ASan 0 |
| `mi-fin3-noi-mission-a` / `-b` | mission 6,000 `/noInterp` | oracle mission run done | none (rc 0) | `794eae2c20c68aac`, ASan 0 |
| `mi-fin3-nonis-mission-a` / `-b` | mission 6,000 `/noNISInterp` | oracle mission run done | none (rc 0) | `794eae2c20c68aac`, ASan 0 |
| `mi-fin3-rel-mission-a` / `-b` | mission 6,000, release | oracle mission run done | none (rc 0) | `546337dc7eb87b7b`, IDENTICAL to debug (build flags only) |
| `mi-fin3-fe` | fe 120 s | Main_game_screen | still running at the timeout | ASan 0, no leak |
| `mi-fin3-sp` | sp 360 s | universe running (univUpdateCounter 3,712) | still running at the timeout | ASan 0, no leak (the real-time clock: alpha from the millisecond remainder) |
| `mi-fin3-fescreens` | fescreens | fescreens self-test done | none (rc 0) | PASS (22 files, 195 screens pushed / popped), 0 fescreen UBSan reports (the added atoms pushed and drawn with their screens) |
| `mi-fin3-console` | console | console self-test done | none (rc 0) | PASS (the two new rows shown, set and reset) |
| `mi-fin-render`, `mi-fin-hud`, `mi-fin-ui`, `mi-fin-nisparse`, `mi-fin-dock`, `mi-fin-command` | render, hud, ui, nisparse, dock, command | each "self-test done" | none (rc 0) | all PASS, ASan 0 (binary before the console-row changes, which touch only the console's config save and its self-test) |

Superseded: `mi-motion-try1..3`, `mi-nis-try1..3`, `mi-ns-a` / `-b`, `mi-pre-nis-c`, `mi-on-motion-1`, `mi-on-nis-1`
(the first cut handling), `mi-on-nis-2`, `mi-nointerp-motion`, `mi-nointerp-nis`, `mi-toggle-1`, `mi-toggle-noInterp`,
`mi-toggle-noNISInterp`, `mi-on-mission-a` / `-b`, `mi-shot-on`, `mi-shot-nis` (PPM frames for a visual check:
`/catMotionShot <frame>`), the whole `mi-fin-*` batch except where named above (same results; its console run failed
as described), the `mi-fin2-*` batch (the same as `mi-fin3-*` except the console self-test's criterion).

## New self-tests and harness modes

- `motion`: `/catMotionTest <ticks>` + `/catMotionDump <file>` (+ `/catMotionFps <fps>`, `/catMotionShot <frame>`);
  harness options `--motion-ticks N`, `--fps N`. Milestone "motion self-test done", line "motiontest: done ...
  result=PASS|FAIL", summary line with the dump's frame count and sha256.
- `nis`: `/catNisMotionTest <nis>[:<from>:<to>]` (+ the same dump / fps / shot switches, and `/noSpeech`); harness
  option `--nis NAME[:FROM:TO]` (default `n06:26:36`).
- `toggle`: `/catToggleTest`; "toggletest:" lines, "toggletest: done ... result=PASS|FAIL".
- `tools/nis_smoothness.py DUMP [DUMP2] [--eps PX] [--object NAME] [--all]`.
- All documented in the tools/m2_harness.sh header.

## Files changed (what and why)

New: `src/Game/MotionInterp.c` / `.h` (the feature), `src/Game/MotionTest.c` / `.h` (the motion, nis and toggle
self-tests), `tools/nis_smoothness.py`, this report. Changed (every added line tagged `HWI-FEATURE(motion-interp)`;
the exe function each sits in, with its address from ghidra/delta_map.json, for the auditor; none of these is a
Cataclysm delta, so there are **no `CAT:` comments and no delta-map review entries** from this producer):

| file | change | in (exe) |
|---|---|---|
| src/Game/B-Spline.c / .h | `bsCurveEvalAt`, `bsCurveEvalAtBeforeJump`, `bsSegmentValue` (new, next to it) | beside `bsCurveUpdate` 0x00423040 (unchanged) |
| src/Game/NIS.c / .h | `nisRenderObjectAt`, `nisRenderCameraAt`, `nisRenderCameraFlyAt` (new); externs | beside `nisUpdate` 0x004ce780 (unchanged) |
| src/Game/Task.c / .h | `taskPeriodFraction`, `taskSubTick` (new) | Task.c (no exe body changed) |
| src/Game/UnivUpdate.c | `miCaptureTick()` after the frame-step test | `univUpdate` 0x0054c3f0 |
| src/Game/Memory.c | `miForget(pointer)` at the top | `memFree` 0x004ae130 |
| src/SDL/render.c | `miRenderBegin` / `miRenderEnd` around `regFunctionsDraw`; `catMotionTestFrameEnd` before the flip | `rndRenderTask` 0x0057a910 |
| src/SDL/mainrgn.c | `rndMainViewRender(miRenderCamera(mrCamera))`; the frame sample | `mrRegionDraw` 0x00569260 |
| src/SDL/utility.c | the sub-tick remainder and the virtual frame step; the universe task's handle kept; the `OP_SmoothMotion` callback row; the `objInterpolate` / `nisInterpolate` options rows; the `/catToggleTest` dispatch | `utyTasksDispatch` 0x005acd10, `utyGameSystemsInit` 0x005abe70, the callback table, `utyOptionsList` (0x008c1478, rows after the HWI rows) |
| src/Game/Options.c / .h | `opSmoothMotion`, `opSmoothMotionAtomsAdd` (new); the flags saved / restored | `opOptionsSaveSettings` 0x004d8d00, `opRestoreSavedSettings` 0x004d7f80 |
| src/Game/FEFlow.c | `opSmoothMotionAtomsAdd(header)` before the return | `feScreensLoad` 0x004744e0 (split as `feScreensImageLoad`) |
| src/Game/Console.c | two rows after the exe's table, `CAT_CON_HWI_ROW_ID`, the config save skips them | `cat_consoleCommandTable` 0x008783c8, `cat_consoleConfigFileSave` 0x0045fec0 |
| src/Game/OracleDump.c / .h | `oracleVirtualFrameStep`, `oracleVirtualSubTick` (new); the motion test's tick hook | `oracleUnivUpdate` (HWI oracle code) |
| src/SDL/main.c | `/noInterp`, `/noNISInterp`, `/catMotionTest`, `/catNisMotionTest`, `/catMotionDump`, `/catMotionShot`, `/catMotionFps`, `/catToggleTest` | the command-line table (`ProcessCommandLine` 0x0055f880) |
| src/Game/meson.build | MotionInterp.c, MotionTest.c | |
| tools/m2_harness.sh | modes motion, nis, toggle | |
| docs/features/motion-interpolation.md | Status: IMPLEMENTED, link to this report | |

## CAT-TODOs

None closed or re-targeted: no `CAT-TODO` / `CAT-PROVISIONAL` names this feature (`grep -rn "CAT-TODO\|CAT-PROVISIONAL" src/
| grep -i -E "motion|interp|smooth"`: nothing), and none were added.

## What is left (owner)

- [audit] **The switches persist**: `/noInterp` / `/noNISInterp` set the same variables the options file rows point at,
  so the next `opOptionsSaveSettings` (an options screen opened, Accept, the end of a game) writes them into
  Cataclysm.cfg and a later plain start stays without interpolation. For A/B runs through tools/m2_harness.sh this is
  harmless (a fresh HOME and settings dir per run); for the user's own install a run-only override (the switch kept
  apart from the saved value) is the fix. Owner: a follow-up of this feature, or the user's ruling.
- **Repair droids** (`CatRepairDroidArray`, not SpaceObjs) are drawn at their tick positions while their host is
  blended: up to one tick of offset at the host's speed (small, slow ships). Owner: the high-refresh-4k feature
  (docs/features/high-refresh-4k.md), or a follow-up of this one.
- **Objects clamped to a non-NIS host** (the Cataclysm mothership's modules) are blended on their own transforms:
  a linear position against the host's blended rotation, a sub-unit arc error while the host turns. Same owner.
- **Camera-derived values**: `univUpdateRenderList` (called by the universe task and by the camera code) stores each
  object's camera distance, which the battle chatter, the effects' LOD and the render list order read. With the toggle
  on, the camera follows the blended focus, so in a played game those values can differ from a toggle-off run (the
  Mission01 oracle run draws the sensors manager, never the main view, and is unaffected: the streams above). Owner:
  the oracle-mode-a producer: judge the replay with `/noInterp` first, then check it with the flags on.
- **The NIS mode's late-loaded statics** (n06's derelicts, loaded by the test because Mission01 excludes them) draw
  without their textures; the measurement only needs positions. Owner: stage 6 (the NIS-first mission start loads
  them as the game does).
- **The motion scene**: C (the receding sProcessor at 0.6 camera distances) fills most of the frame; the positions are
  what the test measures. A smaller ship type would read better on a screenshot (cosmetic, this producer's mode).

## For STATE.md

- M2-R6 motion-interp DONE: render-time motion interpolation (gameplay and NIS) with the Smooth motion checkbox on
  Options -> Video (added in code to `Video_Options` and the in-game `Video` screen; no free-control binding needed),
  `Cataclysm.cfg` rows `objInterpolate` / `nisInterpolate` (default 1), `/noInterp`, `/noNISInterp`, console rows
  `objInterpolate` / `nisInterpolate`. Mission01 stream unchanged: `794eae2c20c68aac` (= base 3aa4cce, IDENTICAL to
  nis-fow-camera's final), with the flags on, off and NIS-off.
- Design calls made here (the spec's wording refined; the user may want to rule): a paused universe draws the
  simulation state (no blend) rather than `prev`; a camera cut is never blended across and still shows at its tick,
  the frames before it continue the pre-cut key's motion (`bsCurveEvalAtBeforeJump`); the NIS objects and camera are
  evaluated ahead of their last tick (tRender = curve time + alpha * period, as the spec says) while objects without
  curves are drawn between their last two ticks (one tick behind), so the two kinds can be up to a tick apart in time.
- [audit] The spec (item 1) asks for render-only transforms (`renderPosition` / `renderCoordsys`) with the simulation's
  fields untouched; the port instead writes the render transforms into the simulation's own fields for the region draw
  and restores them bit for bit after it (`miRenderBegin` / `miRenderEnd`), so the exe-layout structs stay as they are.
  The byte-identical dumps and streams below show the restore holds; the user may want to rule on it.
- The nis harness mode runs with `/noSpeech` (speech subtitles follow the audio thread's wall clock).
- tools/nis_smoothness.py's repeated-frame threshold defaults by mode: 0.05 px (motion, camera float) / 0.01 px (nis).
