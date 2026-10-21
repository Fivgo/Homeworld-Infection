<!-- done: cfg-zeros, scene, realclock, candidates-fix, guards, report -->
<!-- subsystems: cfg-zeros, scene, realclock, candidates-fix, guards, report -->
# M3-R1 producer 4: unfocused objects still jitter (HWI-FEATURE(motion-interp))

Base 8410122. The user's item 4: "the game view shows ships that aren't the main focus still jittering around".
The user's answers and example: cycle/answers/m3-r1-user.md (Smooth motion unknown to the user; the skirmish Recon
recipe).

## (0) Was interpolation on? No: the options file said off, and two code paths could write that 0 without a click

**Evidence.** The options file the user's sessions wrote (original/play-home/.homeworld/Cataclysm.cfg, rows 107-108;
written again at 21:22 by the session after the reported one) holds `objInterpolate 0` / `nisInterpolate 0`. The code
default is 1 (MotionInterp.c:79-80); `utyOptionsFileRead` reads the rows before the command line, so every session
since the zeros were written ran with interpolation OFF unless the box was ticked. The user did not know the checkbox
existed (cycle/answers/m3-r1-user.md). So the 2026-10-20 play-test ran with interpolation off.

**Where the zeros came from.** Two mechanisms, both found in the code, the second reproduced by the toggle test:

1. The M2-R6 build (af50963, 2026-10-18 03:48) persisted the switches: `/noInterp` set the same variables the
   options-file rows point at, and every `opOptionsSaveSettings` (an options screen opening, Accept, a game's end) wrote
   them (docs/m2-r6-motion-interp.md, its audit's "The switches persist"); the run-only override came only with M2-R7
   r6-leftovers (3e33fee, 13:17 the same day). The play-home file was created 2026-10-18 08:15 and four play sessions
   ran 08:15-10:12 (original/play-home/logs/play-20261018-*.log), inside that window, and tools/play.sh's header
   suggested `-- /noInterp` "for an A/B run". play.sh echoes its switches to stderr only, so the logs cannot show
   whether one did.
2. **A live bug (fixed):** the Smooth motion checkbox is a copy of the screen's OP_NoLOD atom, whose flags in both
   .fib files are 0x4862 (front_end.fib Video_Options atom 26, in_game_esc_menu.fib Video atom 21; read with a
   throw-away parser of docs/formats/fib.md's layout): FAF_CallOnCreate | FAF_CallOnDelete. So `opSmoothMotion` also
   ran at the screen's deletion (feCurrentScreenDelete / feScreenDeleteFlags: `FAS_OnDelete`) and set both flags from
   the box. Two consequences, both shown by the new toggle checks before the fix:
   * **Cancel did not cancel**: `opOptionsCancelHelper` restores the saved flags (opRestoreSavedSettings) and THEN
     `opOptionsEndHelper` deletes the screen, whose deletion call re-applied the click; the next save wrote it
     (`m3r1-mj-toggle-prefix`: `screen cancel objInterpolate=0 nisInterpolate=0 ... (before 1/1)`, FAIL).
   * **Under a switch an untouched close wrote 0**: with `/noInterp` the box shows off; opening Video options and
     closing it with Accept (`opOptionsAcceptHelper`: the deletion, then `opOptionsSaveSettings`) made the switch's 0
     the user's choice (miSmoothMotionSet clears the run-only state) and wrote `objInterpolate 0` /
     `nisInterpolate 0` (`m3r1-mj-toggle-prefix-noi`: `screen untouched ... switch=0 (before 0/0 switch=1) rows=0`,
     FAIL). This is a path that writes the zeros with no click at all.

   Fix (src/Game/Options.c `opSmoothMotion`): only a click sets the flags (`else if (!FELASTCALL(atom))`); the
   deletion call is ignored. After the fix `m3r1-mj-toggle` / `-noi` / `-nonis` PASS (`screen untouched` and
   `screen cancel` keep the flags, the switch state and the file's rows). The exe's own OP_NoLOD keeps its deletion
   call (the exe's behaviour, producer 3).

The user's existing file still says 0: THE USER'S CHECK below asks to tick SMOOTH MOTION once (or set the two rows
to 1).

## The measurement (subsystems scene and realclock)

The motion mode's four scripted sProcessors are placed by position writes, so they could not show what a real ship's
flight, a formation, a dock or a clamped module does. Two measurement tools were added (HWI-FEATURE(motion-interp)
measurement; nothing runs without their switches):

* **`/catMotionScene <kind>[:<distance>[:<move>[:<ticks>[:<start>]]]]`** (src/Game/MotionTest.c "the scene mode";
  harness `--scene SPEC`, any mode that plays a game): the user's recipe on the skirmish test's game
  (LiveFireExercise2, the Sect). At tick 16 it picks player 0's Recon nearest its mothership (**A**, sRecon#77) and
  the Recon nearest A (**B**, sRecon#76), **C** (the nearest other free ship, sRecon#91), **D** (the nearest
  asteroid), **E** (the nearest derelict: dSlipgateLarge#0), **F** (a module clamped to the mothership:
  sMothershipEnginesUpper#63), **H** (the nearest pair of a free ship and a host ThisShipCanDockWith 0x00464290 accepts:
  sWorker#73 depositing at sProcessor#101, 6,544 units), **G** / g0 / g2 (three Recons put in delta formation: the
  wingman sRecon#89, the lead #90, #86), **W** (a Worker sent to harvest the nearest asteroid: sWorker#75) and **M**
  (the mothership); it focuses the camera on A (`recon`; `follow`: on B) at the zenith (declination -85, the camera's
  limit: the eye straight above) at the given distance (the camera's own snap, Camera +0x55 / +0x58). At tick 48 the
  command layer (clWrapMove / clWrapFormation / clWrapDock / clWrapCollectResource; no input) orders B 500 units
  straight ahead (the user's "about 500 m"), C 1,500 units across A, the formation 2,000 units, H to dock, W to
  harvest and the mothership 2,000 units ahead (its modules follow, clamped). From tick 40 every frame writes the
  motion dump's line plus `alpha=` (the alpha the frame was blended with, -1 none), `raw=` (taskPeriodFraction of the
  universe task), `calls=` (universe ticks since the previous frame), `mi=blended/new/teleported`, per object
  `<name>m=` (1: the interpolation wrote its transform that frame) and `<name>w=` (its drawn world position).
* **`/catMotionRealClock <fps>`** (harness `--real-clock FPS`): the run on the wall clock instead of the virtual one,
  each frame's dispatch paced to a 1/FPS grid by SDL's performance counter (utyTasksDispatch calls
  catMotionRealClockPace before it reads the clock: the vertical sync's role, which the offscreen driver does not
  have); the dump lines add `ms=` / `ft=` (dispatch time, frame time), `tt=` (task ticks), `sub=` / `rsub=`
  (taskSubTick, taskSubTickRender) and carry `crc=00000000` (no read-back). The nis mode takes it too (its lines get
  the clock tokens only on the real clock, so the virtual nis dumps keep their format).
* **tools/nis_smoothness.py** reads both: `--span` (each object's own motion on screen), `--offscreen`, `--world`
  (the drawn world positions: each object's motion without the camera), `--objects`, `--jerk`; per object the
  **jerk** |p(k+1) - 2p(k) + p(k-1)| (max, p95: 0 for a constant screen velocity, a tick's step at each tick for
  16 Hz stepping, a spike for a hitch) and a **clock** line (alpha's range and per-frame steps, the frames drawn
  unblended, raw above 1, the frames in which the universe ran 0 / 1 / more ticks, the frame times). The default
  output for the old dumps is unchanged.

## (1) The residual with the flags on, and (2) the candidates (subsystem candidates-fix)

All scene runs: skirmish LiveFireExercise2, `--scene recon:1500:500:640` (camera 1,500 units above A, B ordered 500
units ahead, a 640-tick window = 5,832 frames at 144 Hz virtual) or `follow:1500:500:640` (the camera on B, so A, C and
the formation move on screen by the camera). Virtual clock: the debug build (`m3r1-mj-v-*`). Real clock: the release
build, `--real-clock 144` (or 60), the offscreen driver, nvidia-smi 2 % before and after the batch (no game running);
**pre** = a comparison binary built from this tree with the three render fixes below reverted
(original/scratch-m2/m3r1-mj-prefix, measurement only), **fix** = the tree. Cells: per-frame displacement max / p95 /
mean, the repeated frames (moved under 0.05 px) / frame pairs in the object's own motion span, and the jerk max / p95
(px; world tables: world units, eps 0.01).

### The user's case (B, the moving Recon next to the focused one) and the other moving objects, on screen

| object | /noInterp virtual | on, virtual | /noInterp real 144 | on, real 144 **pre** | on, real 144 **fix** |
|---|---|---|---|---|---|
| **B** (mover, the user's Recon) | 1.978 / 1.451 / 0.160, rep 446/501, jerk 1.978 / 1.693 | 0.221 / 0.213 / 0.159, rep **0**/501, jerk 0.091 / **0.006** | 1.978 / 1.109 / 0.130, rep 825/933, jerk 1.978 / 1.491 | 0.428 / 0.222 / 0.126, rep **41**/933, jerk 0.400 / **0.143** | 0.261 / 0.215 / 0.126, rep **0**/933, jerk 0.166 / **0.010** |
| C (crossing) | 2.291 / 2.000 / 0.218, rep 2042/2297, jerk 2.278 / 2.210 | 0.276 / 0.265 / 0.206, rep 0/2297, jerk 0.038 / 0.022 | 2.295 / 1.865 / 0.197, rep 2573/2912, jerk 2.294 / 2.161 | 0.506 / 0.286 / 0.185, rep 79/2912, jerk 0.458 / 0.199 | 0.305 / 0.274 / 0.185, rep 3/2912, jerk 0.064 / 0.026 |
| G (formation wingman) | 1.317 / 1.237 / 0.146, rep 139/156, jerk 1.332 / 1.300 | 0.147 / 0.141 / 0.123, rep 0/156, jerk 0.027 / 0.023 | 1.327 / 0.870 / 0.112, rep 756/856, jerk 1.326 / 1.096 | 0.286 / 0.153 / 0.099, rep 40/856, jerk 0.257 / 0.102 | 0.176 / 0.146 / 0.099, rep 2/856, jerk 0.042 / 0.022 |
| g0 (formation lead) | 1.475 / 1.318 / 0.154, rep 189/212, jerk 1.480 / 1.414 | 0.165 / 0.159 / 0.131, rep 0/212, jerk 0.027 / 0.023 | 1.484 / 0.894 / 0.117, rep 771/873, jerk 1.480 / 1.175 | 0.314 / 0.171 / 0.104, rep 38/873, jerk 0.281 / 0.114 | 0.192 / 0.164 / 0.104, rep 0/873, jerk 0.040 / 0.023 |
| A (the focus) | at rest (max 0.002 px) | at rest | at rest | at rest | at rest |

The focus-follows-the-mover variant (`follow`: the camera on B; A at rest in the world, so A moves on screen only by the
camera, as the asteroid / derelict / modules would):

| object | /noInterp virtual | on, virtual | /noInterp real 144 | on, real **pre** | on, real **fix** |
|---|---|---|---|---|---|
| A (at rest, carried by the camera) | 2.142 / 1.522 / 0.168, rep 519/583, jerk 2.142 / 1.806 | 0.241 / 0.230 / 0.166, rep 0/583, jerk 0.097 / 0.007 | 2.140 / 1.161 / 0.138, rep 901/1020, jerk 2.141 / 1.602 | 0.459 / 0.242 / 0.133, rep 43/1020, jerk 0.425 / 0.151 | 0.281 / 0.233 / 0.133, rep 0/1020, jerk 0.143 / 0.010 |
| C (crossing) | 1.874 / 1.665 / 0.182, rep 1880/2115, jerk 1.881 / 1.758 | 0.218 / 0.210 / 0.173, rep 0/2115, jerk 0.032 / 0.017 | 1.879 / 1.517 / 0.159, rep 2612/2955, jerk 1.880 / 1.736 | 0.401 / 0.234 / 0.149, rep 202/2955, jerk 0.365 / 0.168 | 0.237 / 0.216 / 0.149, rep 99/2955, jerk 0.052 / 0.019 |
| G (formation wingman) | 1.623 / 1.219 / 0.144, rep 577/649, jerk 1.639 / 1.426 | 0.180 / 0.166 / 0.128, rep 0/649, jerk 0.036 / 0.023 | 1.625 / 0.975 / 0.121, rep 947/1072, jerk 1.623 / 1.275 | 0.341 / 0.188 / 0.110, rep 101/1072, jerk 0.321 / 0.124 | 0.200 / 0.177 / 0.110, rep 60/1072, jerk 0.055 / 0.022 |
| B (the focus) | max 0.004 px all frames | 0.004 | 0.004 | 0.004 | 0.004 |

[audit: in both real-clock columns the span is the FIRST dump's (`nis_smoothness.py <noi> <pre|fix> --span`: the /noInterp run's
span, e.g. B 933 frames); with the pre or fix dump alone the span is shorter (B 553 frames: pre rep 17, jerk p95 0.168).]
(The follow real-clock "rep" for C and G count the frames inside the union span where they are already at rest; their
jerk is the virtual run's.)

### The six object kinds, each object's own drawn motion (world units, the camera left out)

| kind (object) | /noInterp virtual | on, virtual | on, real 144 **pre** | on, real 144 **fix** |
|---|---|---|---|---|
| crossing ship (C) | max 65.627, rep 3217/3618, jerk p95 34.920 | max 7.293, rep 1/3618, jerk 0.452 / 0.014 | rep 1/3476, jerk 13.126 / 2.967 | rep 0/3476, jerk 1.415 / 0.095 |
| asteroid (D, asteroid#0) | at rest in the world (on screen: carried by the camera, as A in `follow`) | at rest | at rest | at rest |
| derelict (E, dSlipgateLarge#0) | at rest | at rest | at rest | at rest |
| mothership module (F, sMothershipEnginesUpper#63, clamped; the mothership moving) | max 8.750, rep 3482/3917, jerk 8.750 / 7.578 | max 0.977, rep 0/3917, jerk 0.009 / 0.009 | rep 2/3949, jerk 1.751 / 0.663 | rep 0/3949, jerk 0.586 / 0.016 |
| formation ship (G) | max 21.279, rep 2449/2754 [audit: was 2448; nis_smoothness.py --world --span on m3r1-mj-v-recon-noi and the audit's identical m3r1-amj-v-recon-noi], jerk p95 10.691 | rep 9/2754, jerk 0.397 / 0.005 | rep 8/2643, jerk 4.273 / 0.880 | rep 8/2643, jerk 0.701 / 0.050 |
| docking ship (H, sWorker#73 to sProcessor#101, latched at tick 538) | max 28.567, jerk 28.567 / 26.562 | **before the clamp fix 5.277** / 0.006; after: max 5.277 -> step 0.585 a frame over the tick, jerk 0.585 max | rep 699/5494, jerk 5.706 / 2.214 | rep 689/5494, jerk 1.741 / 0.090 |
| (harvesting Worker W) | max 26.565, jerk p95 26.565 | jerk 2.952 / 0.002 | jerk 5.313 / 2.658 | jerk 5.332 / 0.108 |

(H's "rep" ~690 are the frames it is latched and still.)

### The clock (the real-clock runs' frame lines)

| run | alpha step min / median / max | unblended frames | raw >= 1 (max) | universe ticks a frame 0 / 1 / >1 | frame ms min / median / p99 / max | task ticks a frame |
|---|---|---|---|---|---|---|
| virtual on | 0.1111 / 0.1111 / 0.1111 | 648 (raw exactly 1) | 648 (1.0000) | 5185 / 647 / 0 | - | - |
| real 144 pre | 0.0833 / 0.1167 / 0.1500 | 648 | 648 (1.1000) | 4951 / 647 / 0 | 5.806 / 6.944 / 6.945 / 9.082 | 1 x1476, 2 x4121 |
| real 144 fix | 0.0638 / 0.1157 / 0.1770 | 13 | 648 (1.1142) | 4951 / 647 / 0 | 3.820 / 6.944 / 6.945 / 10.645 | 1 x1476, 2 x4121 |
| real 60 pre | 0.2667 / 0.2833 / 0.2833 | 648 | 648 (1.2500) | 1686 / 647 / 0 | 16.234 / 16.667 / 16.670 / 17.100 | 4 x1943, 5 x389 |
| real 60 fix | 0.2731 / 0.2778 / 0.2879 | 52 | 661 (1.2780) | 1685 / 647 / 0 | 16.033 / 16.667 / 16.667 / 17.300 | 4 x1942, 5 x389 |

(fix's alpha step extremes are the paced frames that came late, ft 3.8 / 10.6 ms: alpha follows the real time.)
The 60 Hz real-clock run, screen: B pre 0.963 / 0.546 / 0.234, rep 37/611, jerk 0.866 / 0.378 -> fix 0.629 / 0.496 /
0.234, rep 4/611, jerk 0.402 / 0.011; C jerk p95 0.654 -> 0.030.

**The NIS on the real clock** (`--mode nis --real-clock 144`, n06 26-36 s; the tracked object O9, an sRecon, and the
NIS camera): /noInterp O9 max 120.532 / p95 24.739 / mean 2.839 px, rep 1175/1328, jerk p95 41.188; on **pre** 40.543 /
6.698 / 2.900, rep 0, jerk 19.006 / **3.592**, camera jerk p95 39.986; on **fix** 41.013 / 6.344 / 2.951, rep 0, jerk
1.419 / **0.296**, camera jerk p95 **5.581** (the NIS task's alpha above 1 in 163 of 1,374 frames, max 1.1163).

### The candidates, each with its verdict and number

1. **taskPeriodFraction above 1: KEPT, fixed.** The render task is started before the universe task (utility.c
   utyRenderTask, then universeTaskHandle) so it runs first in each pass of taskExecuteAllPending: in the frame whose
   pass will run the next universe tick, alpha is above 1. The virtual clock hits exactly 1.0000 (648 of 5,832 frames:
   drawn at the current tick, correct); the wall clock lands at 1.0 .. 1.10 (144 Hz) / 1.25 (60 Hz) in one frame of
   every tick, and miRenderBegin drew those frames at the current tick (`alpha >= 1`: the simulation's values), the
   NIS path clamped them to 1. That frame holds the motion and the next one doubles it: B 41 repeated frames of 933 and
   a jerk p95 of 0.143 px (virtual: 0 and 0.006). **Fix (MotionInterp.c, HWI-FEATURE(motion-interp), MI_ALPHA_MAX 2):**
   above 1 the frame is drawn at its own time: miLerp / miBlendCoordsys continue the last tick's motion (they
   extrapolate for t > 1), the NIS curves are evaluated at the render time (bsCurveEvalAtBeforeJump leaves every jump
   ahead out, so a cut still shows at its tick); exactly 1 still draws the simulation's values (the virtual clock's
   frames: the motion / nis dumps unchanged).
2. **taskPeriodFraction's resolution: KEPT, fixed.** GoK's utyTasksDispatch counts SDL_GetTicks milliseconds in 4 ms
   task ticks and keeps the millisecond remainder (taskSubTick): at 144 Hz a 6.94 ms frame counts 6, 7 or 8 ms, so
   alpha's per-frame step was 0.100 / 0.117 / 0.133 of a tick (a 14 % speed ripple; B's jerk p95 0.027 px after fix 1,
   world 0.058). The exe's utyTasksDispatch 0x005acd10 divides QueryPerformanceCounter counts by a 64-bit divisor
   (QPF / 240) and has no such step. **Fix (Task.c taskClockSample, utility.c, HWI-FEATURE(motion-interp)):** the same
   monotonic clock read with SDL's performance counter gives the sub-millisecond part (the counter minus SDL_GetTicks,
   averaged); the render's alpha uses taskSubTickRender, the scheduler's ticks are untouched. B's jerk p95 0.027 ->
   0.010 px (virtual 0.006), world 0.058 -> 0.013. The task clock's own deviation (250 task ticks a second and a 60 ms
   universe tick on the wall clock, against the exe's 240 and 62.5 ms) is a simulation-timing change:
   `CAT-TODO(M3 soak)` at utyTimerDivisor (utility.c), not this round's.
3. **Alpha drops to 0 / several universe ticks in one frame: RULED OUT.** No frame ran more than one universe tick at
   144 or 60 Hz (">1" 0 in every real run); alpha's minimum blended value 0.064 (144) / 0.273 (60); the 13 / 52
   unblended frames are raw exactly 1.0 (drawn at the tick, correct).
4. **Objects the capture misses: RULED OUT** (with one exception that never moves). miCaptureTick walks
   universe.SpaceObjList: every ship, asteroid, dust cloud, nebula, derelict, missile and bullet is on it; the dump's
   `mi=` counts 825,760 blends over 5,832 frames (141.6 a frame) with 24 "new" (created that tick) and 0 teleported.
   The one list it does not walk, MinorSpaceObjList (the point asteroids, Asteroid0), moves only when a single-player
   script sets Asteroid0sCanMove (univUpdateAllPosVelMinorObjs): 0 of the 29 .mission files in cataclysm.big set it,
   and a skirmish never moves them.
5. **MI_TELEPORT false positives: RULED OUT.** 0 teleported objects in every scene frame (a Worker at 286 u/s, the
   Recons, the moving mothership; 2,000 units a tick is 32,000 u/s).
6. **Positions rewritten after the capture (formation, docking, clamps, the hand-off): one KEPT, fixed.** Everything
   the tick writes is in "cur" (the capture is at the tick's start), so formation (G jerk p95 0.005 virtual) and the
   clamped modules of a moving mothership (F 0.009) are smooth. The exception was the dock's latch: when a ship is
   clamped to its host during a tick (CAT_DOCKSTATE_LINK_IN; an externally built ship), miGameplayClamped (M2-R7) drew
   it at its clamp point in the host's frame from the tick's first frame: the Worker H jumped 5.277 units in one frame
   and then stood still for eight. **Fix (MotionInterp.c, HWI-FEATURE(motion-interp)):** the capture records whether
   the object was clamped (MiEntry.clamped); a child clamped since the capture keeps its own blend for that tick (it
   slides 0.585 units a frame onto the clamp point); from the next tick the host-frame write as before.
7. **The camera: RULED OUT.** CameraChase eases per task tick (`Frames = utyNFrameTicks`, at most 64, the exe's the
   same, 0x0042d510) with the velocity prediction, inside the draw with the interpolated focus; universe time does not
   drive it. The focus stays on its pixel to 0.004 px in every run (virtual and real, `follow`), the eye's per-frame
   move is constant (follow virtual: max 1.375 / p95 1.133 world units, real fix 1.641 / 1.180), 0 repeated poses
   while the focus moves.
8. **Draws outside miRenderBegin / miRenderEnd: RULED OUT for gameplay; one tutorial case left.** The selection
   circles, health bars, tactical overlay, move line, sensors manager and trails are drawn by the regions inside the
   bracket. After it rndRenderTask draws only the scissor bars, subtitles, the sync / debug text, the mouse and the
   plug; the one position read there is tutDrawTextPointers' TUT_PointerTypeShips (selSelectionDimensions over the
   simulation's positions) when no tutorial text region drew the pointers that frame: a tutorial pointer at a
   selection can step at 16 Hz (not measured; owner below).
9. **Frame pacing: the measurement's, measured; the user's own pacing left with its owner.** With every frame on its
   1/144 slot (p99 6.945 ms) the residual is the figures above. SDL_GL_SetSwapInterval(1) (render.c) ties the flip to
   the monitor; producer 3 measured the 4K FULL DETAIL frame at a 9.8-10.5 ms median (ai scene, RTX 5090): above a
   144 Hz monitor's 6.94 ms, so the user's frames present at 1 or 2 refresh intervals. Interpolation follows the real
   frame times (the dispatch time), but a frame that misses its refresh shows later than it was timed: a pacing
   judder of up to one refresh that interpolation cannot remove (the M3 soak's frame-pacing measurement, owner below).

## (3) The guard rails (subsystem guards; the final build, debug unless noted)

| check | runs | reference | result |
|---|---|---|---|
| headless Mission01, 6,000 ticks, flags on (pair) | `m3r1-mj-mission-a` / `-b` (sha256 2a992dcf2ff752c4, 1,476,370 B, 6,001 T) | r10i-mission-a (5bd51234405ea0d8) | **IDENTICAL in every field** (ticks 0-6000, rng_all included); a / b IDENTICAL |
| the same, `-- /noInterp` | `m3r1-mj-mission-noi` | r10i-mission-a | **IDENTICAL** |
| the same, `-- /noNISInterp` | `m3r1-mj-mission-nonis` | r10i-mission-a | **IDENTICAL** |
| the same, release | `m3r1-mj-mission-rel` (c9433d8260e1c160) | r10i-mission-a | IDENTICAL |
| missiontest Mission01 (pair) | `m3r1-mj-m1-a` / `-b` | producer 3's audited `m3r1-afd2-m1` | **IDENTICAL** (ticks 0-11425), 6/6, done 11,424, PASS |
| ai (pair, 400) | `m3r1-mj-ai-a` / `-b` | `m3r1-afd2-ai-a` | **IDENTICAL** (ticks 0-400) |
| skirmish LiveFireExercise2 | `m3r1-mj-skirmish-lfe2` | `m3r1-afd2-skirmish-lfe2` | IDENTICAL (ticks 0-600), PASS |
| motion dump /noInterp | `m3r1-mj-motion-noi` | producer 3 audited (`m3r1-afd2-motion-noi`) c503c869f0dc9cdb / 3016280e9e0dd3c8 | **EQUAL** whole and crc-stripped: c503c869f0dc9cdb / 3016280e9e0dd3c8 |
| motion dump flags on | `m3r1-mj-motion-on` | c0e411662fa2f527 / 428ef41479eb5749 | **EQUAL** (did not move: the universe task's alpha is exactly 1 on the virtual clock) |
| nis dump /noInterp | `m3r1-mj-nis-noi` | 86cb155acffc180e / 196bab32d244f614 | **EQUAL** |
| nis dump flags on | `m3r1-mj-nis-on` | da532dafedb89960 / 7ce8472514254f4f | **moved: 0307f3be2044c29c / a5de13a44b8d264f** (below) |
| toggle (plain, /noInterp, /noNISInterp) | `m3r1-mj-toggle`, `-noi`, `-nonis` | | `toggletest: done start=1/1`, `0/0`, `1/0` `fails=0 result=PASS`, the new `screen untouched` / `screen cancel` checks included |
| fe | `m3r1-mj-fe` | | Main_game_screen at the 120 s timeout, ASan 0 |
| sp | `m3r1-mj-sp --timeout 1200` | | Mission01_NIS01 .. Mission12 (golden-12-00576.jpg; 12 animatics skipped), still running at 1,200 s, ASan 0 |

ASan 0 in every m3r1-mj run (the scene runs included).

**The nis flags-on dump's move (stated reason: fix 1).** 160 of 1,440 frame lines differ (crc= and the position
columns), one frame in every tick from frame 7 (tick 432): the NIS task's phase is not aligned with the universe
task's, so on the virtual clock too its alpha passes 1 in the frame whose pass runs the next NIS tick (unlike the
universe task's, which hits exactly 1); those frames were held at the NIS tick and are now drawn at their own time.
nis_smoothness.py, tracked O9 (sRecon): max / p95 / mean 39.527 / 6.203 / 2.808 -> 39.527 / 6.107 / 2.814 px, repeated
0 -> 0, jerk max / p95 8.426 / 1.318 -> 1.043 / 0.231; camera jerk p95 19.509 -> 5.121 (repeated poses 169 -> 168).

No stream moved: every .hwo above is IDENTICAL to its reference in all 21 fields.

## What changed, file by file

* **src/Game/Options.c** `opSmoothMotion` (HWI-FEATURE(motion-interp)): only a click sets the flags; the screen's
  FAS_OnDelete call is ignored ((0) above). The header comment says why.
* **src/Game/MotionInterp.c / .h** (HWI-FEATURE(motion-interp), the whole file): `MI_ALPHA_MAX` 2 and the frame drawn
  at alpha above 1 (gameplay continuation, NIS curves at the render time; exactly 1 draws the simulation's values);
  `MiEntry.clamped` (set by miCaptureTick) and the newly clamped child's own blend in miGameplayClamped; the
  measurement's getters `miRenderStats` / `miRenderWritten` and their per-frame counts.
* **src/Game/Task.c / .h** (HWI-FEATURE(motion-interp)): `taskClockSample` and `taskSubTickRender`; taskPeriodFraction
  uses it when set (the wall clock), taskSubTick otherwise (the virtual clock).
* **src/SDL/utility.c** utyTasksDispatch (HWI-FEATURE(motion-interp) lines): taskClockSample after the clock read,
  taskSubTickRender -1 on the virtual clock; the measurement's `/catMotionRealClock` pacing and stamp; the virtual
  branch not taken on the real clock. `CAT-TODO(M3 soak)` at utyTimerDivisor (the exe's QPC / 240 task clock).
* **src/Game/MotionTest.c / .h** (HWI-FEATURE(motion-interp) measurement): the scene mode, the real clock, the toggle
  test's `mtToggleScreenOrder` (screen untouched, screen cancel); the nis lines' clock tokens on the real clock only.
* **src/SDL/main.c**: `/catMotionScene`, `/catMotionRealClock` (HWI-FEATURE(motion-interp)).
* **tools/m2_harness.sh**: `--scene SPEC`, `--real-clock FPS` (documented in the header; the summary's `scene:` line, the
  milestone "motion scene done"); the nis mode leaves `/noSpeech` out on the real clock; the toggle mode's header
  documents the screen-order checks.
* **tools/nis_smoothness.py**: the scene tokens, `--span`, `--offscreen`, `--world`, `--objects`, `--jerk`, the jerk
  figures and the clock line (the old dumps' default output unchanged: same md5 of the output for m3r1-afd2-motion-on).
* **tools/play.sh**: the Smooth motion paragraph rewritten (it still said a switch's 0 is saved; how to turn it back
  on).
* **docs/features/motion-interpolation.md**: item 6 (the wall clock).
* **ghidra/delta_map_review.json** (+ merged ghidra/delta_map.json, docs/m2-r1-function-deltas.md regenerated):
  0x005acd10 utyTasksDispatch, a `[review: M3-R1 motion-jitter]` note: compared (decompile): QueryPerformanceCounter's
  delta over the 64-bit utyTimerDivisor (QPF / 240, utyGameSystemsInit 0x005abe70): HW1's body, IDENTICAL stands; the
  tree's GoK SDL layer counts milliseconds (CAT-TODO(M3 soak)). `tools/delta_map.py merge --done m3r1:motion-jitter`:
  records changed 1 (IDENTICAL 2762 / MODIFIED 1575 / REWRITTEN 83, reviewed 3573), then `report`. Compared and found
  as the tree (no row): ccControl 0x0042e7c0 (CameraChase's caller, the camera verdict), CameraChase 0x0042d510
  (`Frames = utyNFrameTicks`, at most 0x40), feScreenDeleteFlags 0x00475550 (the FAF_CallOnDelete call with
  FAS_OnDelete set: the box's deletion call is the exe's mechanism).

No new setting this round: every fix is under the existing HWI-FEATURE(motion-interp) and its surfaces (the
Cataclysm.cfg rows objInterpolate / nisInterpolate, default 1; the switches /noInterp, /noNISInterp, run-only; the
console rows; the SMOOTH MOTION checkbox on Options -> Video, front end and in game). Builds: tools/build.sh --debug
and --release green, 0 warnings (-Werror). No CAT-TODO / CAT-PROVISIONAL names M3-R1 (`/bin/grep -rn` 0) [audit: it was 1: the added CAT-TODO(M3 soak) line's
body said "M3-R1 motion-jitter measured them"; the audit reworded it, now 0]; one added: CAT-TODO(M3 soak). HWI-FEATURE(motion-interp) tags 441 -> 482.

**Plain-harness runs** (label, milestone, first stop): the scene runs `m3r1-mj-v-{recon,follow}-{on,noi}` (motion
scene done, none, Bmoved=451, PASS), `m3r1-mj-rc-{recon,follow}-{noi,pre,fix}`, `m3r1-mj-rc60-recon-{pre,fix}` (motion
scene done, PASS; 0-5 late pacing slots of ~5,870), `m3r1-mj-rc-nis-{noi,pre,fix}` (motion self-test done, PASS),
`m3r1-mj-toggle-prefix` / `-prefix-noi` (before the fix: FAIL as described), the guard rails above.

## THE USER'S CHECK (for the play-test brief)

1. **Turn Smooth motion back on** (the play-home options file still says off: rows 107-108 are 0). Run
   `tools/play.sh --rebuild`, open Options -> Video in the front end, tick **SMOOTH MOTION**, press Accept. (Or set
   `objInterpolate 1` and `nisInterpolate 1` in original/play-home/.homeworld/Cataclysm.cfg before starting.) Then open
   and close the Video screen once more: the box should still be ticked.
2. **Your recipe**: Multiplayer -> Skirmish (one computer), the Sect. Focus the Recon nearest the mothership, put the
   camera straight above it, select the adjacent Recon and order it about 500 m straight ahead. Watch the moving Recon
   along its whole path: **report whether it still jitters**, and whether anything else on screen does (ships crossing,
   a formation, a Worker docking at a Processor: the moment it latches on).
3. **The same with the camera moving**: focus the moving Recon instead and watch the still ones and the asteroids slide
   by.
4. **A cutscene**: New Game -> the Mission01 opening; watch the camera pans and ships in them.
5. **Tell us the monitor's refresh rate and whether the game ran at it** (the 4K FULL DETAIL frame is ~10 ms on this
   machine, more than a 144 Hz refresh's 6.9 ms: if the motion is smooth but uneven in a regular beat, that is frame
   pacing, not interpolation, and goes to the soak round). For an A/B, `tools/play.sh -- /noInterp` turns it off for
   that run only (the options file keeps its rows).

## For STATE.md

* Decision (M3-R1 motion-jitter): the user's 2026-10-20 play-test ran with interpolation OFF (Cataclysm.cfg's
  objInterpolate / nisInterpolate 0, probably from an M2-R6-build /noInterp run on 2026-10-18 or the OnDelete path);
  `opSmoothMotion` acts on clicks only (the copied OP_NoLOD atom's FAF_CallOnDelete call ignored).
* Names: `MI_ALPHA_MAX` (2.0: frames past the current tick drawn at their own time), `MiEntry.clamped`,
  `taskClockSample` / `taskSubTickRender` (the render alpha's sub-millisecond part), `/catMotionScene`,
  `/catMotionRealClock`, harness `--scene` / `--real-clock`, MotionTest.c `mtToggleScreenOrder`; nis_smoothness.py
  `--span`, `--offscreen`, `--world`, `--objects`, `--jerk`.
* Defaults: unchanged (both flags on; the switches run-only).
* Baselines: no stream moved; motion dumps EQUAL; the nis flags-on dump moved by fix 1 (0307f3be2044c29c /
  a5de13a44b8d264f; 160 frames, one a tick): the next producer's reference.

## What is left, and its owner

* The task clock (GoK's SDL_GetTicks / 4 ms: 250 task ticks a second, a 60 ms universe tick, 4 % fast on the wall
  clock, against the exe's QPC / 240): `CAT-TODO(M3 soak)` in utility.c; a simulation-timing change, measured with the
  soak's universe tick rate.
* Frame pacing at 4K (frames above the refresh interval present 1 or 2 refreshes late): the M3 soak's frame-time
  measurement (`/frameLog`) and the user's answer on the refresh rate.
* The tutorial's TUT_PointerTypeShips pointer computed after miRenderEnd (selSelectionDimensions on the simulation's
  positions) when no tutorial text region drew it: not measured, owner M3-R2 (the fix round), only if the user sees a
  tutorial pointer step.
* Piecewise-linear interpolation keeps a velocity kink at each tick (B's world jerk max 0.281 units at its start and
  stop, p95 0.005): not a jitter; no owner unless the play-test asks.
