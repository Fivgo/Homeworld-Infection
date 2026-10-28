# Feature: render-time motion interpolation (gameplay and cutscenes), with an in-game toggle

Requested by the user 2026-10-17 after the first play-test: fast camera pans in in-game cutscenes look jittery.
Status: IMPLEMENTED (M2-R6 motion-interp, 2026-10-18; report: [docs/m2-r6-motion-interp.md](../m2-r6-motion-interp.md),
which is the "docs/m2-r6-nis.md" item 5 asks for). Approved 2026-10-17 for M2-R6 right after the NIS port, as its own
commit. The toggle is a hard requirement: the user wants A/B comparisons.

## The user's observation (second play-test, 2026-10-17)
Whatever the camera focuses on moves smoothly; every other ship and asteroid looks a frame behind, and the effect grows
with the relative velocity of camera and object. The user's ruling: this is a core engine issue, not a cinematic one,
to be fixed for good in the recreation.
Refinement (same day): it is not the size of the combined velocity but how TANGENTIAL the relative motion is. An
asteroid near the screen centre travelling straight away from the camera looks correct; objects crossing the view
stutter. Reading: the visible jump is the per-tick SCREEN-SPACE displacement, i.e. the relative velocity's component
across the line of sight divided by distance (angular velocity). Depth-only motion changes apparent size by a fraction
of a pixel per tick, so the 16 Hz stepping is invisible there; lateral motion moves many pixels per tick. The camera
lock on the focus cancels the focus's angular motion and nobody else's, which is why only unfocused objects stutter.

## Cause (verified in the source, 2026-10-17)
- **Gameplay:** every object's position is written once per universe tick (16 Hz) and drawn as-is. The camera is the
  exception: `ccControl` runs every RENDER frame (`SDL/mainrgn.c:5467`), derives the look-at from the focused ships'
  current positions (`CameraCommand.c:152,166`), and `cameraSetEyePosition` recomputes the eye each frame with
  time-based easing (`EvalCubic` transitions). So the camera is a continuous per-frame function anchored to the focus
  ships' 16 Hz positions: the focus moves with the camera (no relative jump), while everything else jumps each tick by
  its own displacement PLUS the camera's. Exactly the user's observation. Homeworld 1 behaves the same; CET's 64 Hz
  option only makes the jumps four times smaller.
- **Cutscenes (NIS):** same root, different path, below.
- NIS files hold 30 fps keyframes (`NIS_FrameRate 30.0f`) for object position/heading/pitch/bank and camera eye/look/up;
  `nisStart` fits B-spline curves through them (`bsCurveStart`).
- `nisUpdateTask` is started with `taskStart(nisUpdateTask, UNIVERSE_UPDATE_PERIOD, 0)` (NIS.c:580) and calls
  `nisUpdate(NIS, dt)`, which evaluates every curve with `bsCurveUpdate(curve, dt)` and writes object `posinfo`/`rotinfo`
  and the camera (`nisCameraFlyCompute`) ONCE PER UNIVERSE TICK (16 Hz). CET's anchors confirm the exe's NIS task uses the
  universe period.
- The render task runs every display frame and draws the last written poses; nothing in the render path interpolates.
  At 144 Hz each pose is held ~9 frames, then jumps: visible on fast pans.

## Design (presentation only; the simulation and the oracle stream stay bit-identical)
0. **Gameplay motion (the general case, now the primary deliverable).** Keep per-object `prevPosition/prevCoordsys`
   (and for bullets/missiles/effects where visible), captured at the start of each universe tick. At render time draw
   every object at `lerp(prev, cur, alpha)`, `alpha` = fraction of the current tick elapsed (clamped 0..1; 0 when the
   universe is paused). The camera's focus centre uses the SAME interpolated positions, so the camera and the focused
   ship stay locked as today while unfocused objects stop lagging. Objects created this tick draw at `cur`. Picking,
   selection boxes and HUD overlays use the interpolated positions too, so the cursor matches what is drawn.
1. **Render-time evaluation of the NIS curves.** Add a non-advancing evaluator `bsCurveEvalAt(curve, t)` (the existing
   `bsCurveUpdate` advances internal state and must stay the tick-time path). Each render frame, when an NIS is playing
   and interpolation is on, compute `tRender = NIS->timeElapsed + alpha * UNIVERSE_UPDATE_PERIOD * nisPlayFactor`, where
   `alpha` = fraction of the current tick elapsed at render time (from the task scheduler's accumulator; clamp 0..1),
   and evaluate the camera curves and each NIS-driven object's curves at `tRender` into RENDER-ONLY transforms
   (a per-object `renderCoordsys/renderPosition` used by the renderer; the sim fields written by `nisUpdate` are untouched).
   Events (show/hide/fire/speech/cuts) stay at tick granularity; a camera cut disables blending for that frame.
2. **Fallback for objects without curves** (ships moved by the sim during an NIS): linear interpolation between the
   previous and current tick transforms (keep `prevPosition/prevCoordsys` per object), same `alpha`. This is the general
   mechanism for smooth gameplay motion later; in this feature it is only enabled while an NIS plays.
3. **Toggle.** One UI control, "Smooth motion", driving two internal flags `objInterpolate` and `nisInterpolate` (both default ON, both separately settable from the cfg and the console for A/B tests):
   - Options → Video: a checkbox "Smooth motion" (ship an overridden options screen .fib under the recreation's data
     dir; docs/formats/fib.md has the format; if the .fib edit cannot be done this round, bind to a free control and
     say so in STATE.md, but the button is required, not optional);
   - `Cataclysm.cfg` keys `objInterpolate 0|1` and `nisInterpolate 0|1` (persisted like the other options);
   - command line `/noInterp` (both off) and `/noNISInterp`, plus dev-console commands, for scripted runs.
4. **Comparison tooling (so changes are measurable, not just eyeballed).** Harness mode `motion`: run a mission headless at a fixed virtual frame rate (144 fps) with the camera focused on one ship and a second ship crossing the view; dump per-frame screen-space positions of both; report max and 95th-percentile per-frame SCREEN-SPACE displacement (pixels, i.e. angular motion) and repeated-frame counts for focused vs unfocused objects, interpolation on and off; include one object moving across the view and one moving along the line of sight, since only the first shows the defect. Plus harness mode `nis`: play a given NIS headless
   at a fixed frame rate (e.g. 144 fps virtual) with interpolation on and off, dumping per-frame camera pose and the
   screen-space position of one tracked object; `tools/nis_smoothness.py` reports the max and 95th-percentile per-frame
   displacement and the count of repeated frames. Expected: identical sim stream in both runs; repeated-frame count
   drops from ~8 of 9 to 0 with interpolation on.
5. **Guard-rails.** Interpolation off must reproduce the pre-feature frames exactly (byte-identical oracle stream in
   both settings; per-frame dump identical with the toggle off). No change to `nisUpdate` semantics, timing, or the KAS
   runtime. Document in `docs/m2-r6-nis.md` (written as docs/m2-r6-motion-interp.md).
6. **M3-R1 (motion-jitter, docs/m3-r1-motion-jitter.md): the wall clock.** The render task runs before the universe and
   NIS tasks in each scheduler pass, so on the wall clock one frame a tick sees alpha above 1 (up to a frame's task
   ticks); that frame is drawn at its own time (gameplay: the last tick's motion continued; NIS: the curves at the render
   time; at most 2 ticks, `MI_ALPHA_MAX`), not held at the tick (item 1's "clamp 0..1" is replaced by this). The
   render's alpha takes the clock's sub-millisecond part (Task.c `taskClockSample`, SDL's performance counter); the
   scheduler's ticks are unchanged. A ship clamped to a host during a tick (a dock's latch) slides there over that tick.
   Measurement: harness `--scene` (the user's skirmish recipe and the object kinds) and `--real-clock FPS`.
7. **M3-R3 (pause-freeze, docs/m3-r3-pause-freeze.md): the pause.** The user pauses with the P key (the key binding
   Pause, catKbPause 0x00550640), which toggles the exe's frame-step flag 0x00a6ed74 (`catUnivFrameStep`): univUpdate
   0x0054c3f0 returns at once, but the universe task keeps running, so before M3-R3 the lerp kept blending between the
   last tick's start and end with an alpha still cycling every 62.5 ms (every moving ship and the camera replayed the
   last tick 16 times a second). Now (HWI-FEATURE(pause-freeze), always on, no setting): while the universe is paused
   (`miUniversePaused()`: universePause, or the P key's flag with no frame step left) every object is drawn at the pose
   the frame before the pause drew (the alpha held; the camera's focus with it), the camera float's clocks stand still
   (the exe stops them for universePause only), the engine glows keep their size and the trail segments their colour
   (the random streams are still drawn as the exe draws them), and the NIS's own pause (nisPaused, NIS_TIME_CONTROLS)
   holds the NIS's render time. On the unpause the hold lasts until the next tick's capture, which starts that tick's
   blend from the held pose: no step back or forward, the pose catches up over that one tick. The HUD's real-time
   animations (the over-cap SU blink, the upgrade pulse, the ship view) run on as in the exe. Measurement: the HWI-TEST
   switch `/catMotionPause <frame>:<frames>[:key|univ|nis]` in the motion / nis / scene runs (dump tokens ` pause=` and
   ` crcv=`, the CRC with the SU blink blanked).
8. **M3-R4 (mesh-anim-interp, docs/m3-r4-mesh-anim-interp.md): the mesh animations.** A ship's scripted mesh
   animation (the `.mad` B-spline curves: the Workers' drill arms and dock bays, the Carrier's and Hive Frigate's bays,
   the Acolyte's wings, the Leech's mouth, the NIS `meshAnimStart` events) is advanced by `madAnimationUpdate` once per
   universe tick and its curves were evaluated at that tick time when the ship was drawn, so every frame of a tick drew
   the same pose (the user's "16 Hz animations" after the full playthrough). Now `madAnimBindingUpdate` evaluates the
   drawn pose on a COPY of the object's six curves stepped on by `alpha * phystimeelapsed` (`madRenderStep`: the same
   alpha as item 0 / 6, read through `miRenderAlpha` and only between `miRenderBegin` and `miRenderEnd`; a paused
   animation and alpha <= 0 give the tick's pose), with the animation's own end rule (loop wraps, else the end holds).
   The simulation's curves, `madanim.time` and the gun / dock evaluation (`madAnimBindingMatrix`, outside a frame) are
   untouched, so the oracle stream is bit-identical; with the toggle off nothing changes. Measurement: HWI-TEST
   `/catMadProbe` (madprobe: lines: the tick's pose and the frame's pose of every animated object in an interpolated
   frame) in a `--scene` `--real-clock` run.
