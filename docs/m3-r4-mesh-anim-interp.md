# M3-R4 ad hoc: mesh animations at the frame rate (HWI-FEATURE(motion-interp) mesh animations), 2026-10-28

Landed from the user's terminal session 38027cce between rounds, after the user's full playthrough.

## The user's words (verbatim)
- "okay I was able to do a full playthrough of the single player campaign. All changes look stable and I want to start
  moving towards the wrapping up process. There's a couple of things I would like to smoothen out like the animations
  that are attached to the harvesters and the scene where the ore canister is jettisoned. It looks like they have a
  specific frame animation but I would like to add a few interpolation steps in between so that the animations look
  like they are at 60 fps instead if what I can assume is 16 hz animations"

## What stepped
A ship's scripted mesh animation is a `.mad` file: per animated object six B-spline curves (x, y, z, heading, pitch,
bank) over the animation's frames (`race_sect/sworker.mad`: JackOut, JackClose, DockClose, DockOpen, BladesSpin,
JackSpin, DrillAndBlades; the Beast Worker the same; the Carrier's and Hive Frigate's bays, the Acolyte's wings, the
Leech's mouth, the Taiidan heavy interceptor, the Kushan ships; the NIS `meshAnimStart` events, Mission01's intro
has the Workers' DockOpen / DockClose). The simulation advances the animation once per universe tick
(`madAnimationUpdate(ship, universe.phystimeelapsed)`, MadLinkIn.c / UnivUpdate.c, 16 Hz): `madanim.time` and
`madanim.timeElapsed` grow by the tick. When the ship is drawn, `madAnimBindingUpdate` (the binding callback of every
animated object, Mesh.c `meshObjectRenderHierarchy`) advances the object's curves by `timeElapsed` and evaluates them,
and `madBindingsPost` zeroes `timeElapsed` after the draw: the second, third .. frame of the same tick advance by 0
and draw the pose of the tick. At 60 Hz that is the same pose for three or four frames, then a step: the Worker's
arms and the canister bay moved at 16 Hz while the ship itself (HWI-FEATURE(motion-interp), M2-R6) moved at the frame
rate. The user's "ore canister" scene is the Workers' dock animation (DockOpen / DockClose: the bay opens, the
canister goes, the bay closes), in the intro NIS and at every deposit.

## The change (MeshAnim.c, MotionInterp.c / .h, main.c)
- `madCurvesAdvance`: the body of `madAnimBindingUpdate` (0x004b6720: the end rule, loop wrap / end clamp, the six
  `bsCurveUpdate`s) split out, so it can run on any curve array. The simulation's call is unchanged.
- `madRenderStep`: `alpha * universe.phystimeelapsed` where alpha is `miRenderAlpha()` (the frame's alpha of the
  ships' own blend: the fraction of the universe tick elapsed at the render time, time compression included, the
  M3-R1 extrapolation above 1 and the M3-R3 pause hold included), only between `miRenderBegin` and `miRenderEnd`
  (`miRenderDrawing`, new) and only while the animation runs (`bPaused` holds); 0 otherwise.
- `madAnimBindingUpdate`: after the simulation's advance, with a step above 0, the six curves are COPIED
  (`splinecurve` holds the time, the current point and pointers to the shared keys) and advanced by the step with the
  same end rule; the drawn matrix comes from the copy. The simulation's curves, `madanim.time`, the gun and dock
  evaluation (`madAnimBindingMatrix`, which rewinds and re-advances the real curves outside a frame) and the save game
  are untouched; outside an interpolated frame (the toggle off, alpha 1 at the tick, a frame that is not drawing) the
  function is the exe's.
- The convention is the NIS curves' (spec item 1: the pose at `tick time + alpha * period`), a known curve evaluated
  ahead, exact; the ships' positions blend behind (item 0) by up to one tick, as before.
- HWI-TEST `/catMadProbe`: in every interpolated frame each animated object logs `madprobe: t= tick= alpha= step=
  ship= obj= anim= time= sim=(xyz|hpb) render=(xyz|hpb)` (the first 600 lines).

## Fence (debug builds of the worktree, ASan 0 in every run, both builds 0 warnings)
Runs in `original/scratch-m2/m2r2-harness/runs/`.

| run | what | result |
|---|---|---|
| m3r4-mai-rc60 | skirmish LiveFireExercise2 `--scene recon --real-clock 60`, `/catMadProbe` | 54 madprobe lines: the Mothership's GravRotate (ticks 8-15, 27 frames, alpha 0.27-1.27): the drawn pitch monotonic, within 0.0001 rad of the straight line between the tick poses; ASan 0 |
| m3r4-mai-rc60-noi | the same with `/noInterp` | 0 madprobe lines (the toggle off: the tick's pose, the exe's); ASan 0 |
| m3r4-mai-rc60-worker | `--scene worker:0:500:800 --real-clock 60` (the camera on H, sWorker#73 depositing at sProcessor#101; the scene kind is new) | 600 lines (the cap): DockOpen on all 12 objects from tick 504, 3.5 frames per tick, a new pose every frame: object 0's x per tick 0.86, 2.03, 3.38, 4.79 .. drawn 1.22, 1.54, 1.88, 2.23, 2.60, 2.98, 3.37, 3.76, 4.16 ..; the frame at alpha 0.99 (3.368) meets the next tick's pose (3.375); the curve's own ease at the end is followed (ticks 511-516 reverse smoothly); ASan 0 |
| m3r4-mai-rc60-harvest | `--scene harvest:0:500:800 --real-clock 60` (the camera on W, sWorker#75 harvesting its asteroid) | 600 lines: JackOut on all 12 objects from tick 649, 3.5 frames per tick: object 11's z per tick -0.052, -0.192, -0.396, -0.640 .. drawn -0.114, -0.156, -0.204, -0.256, -0.313, -0.373, -0.437 ..; ASan 0 |
| m3r4-mai-mission | `--mode mission --mission-ticks 6000` | IDENTICAL to m3r3i-e-mission-a (6001 ticks); ASan 0 |
| m3r4-mai-m4 | Mission04 self-test (the module's jettison NIS) | IDENTICAL to m3r3i-e-m4-a (20787 ticks); ASan 0 |
| m3r4-mai-m1 | Mission01 self-test (the intro NIS's `meshAnimStart` DockOpen / DockClose on the Workers) | 11425 ticks, rc 0, ASan 0 (M3-R3 has no Mission01 baseline to compare against) |

The streams are identical because the render step touches only a copy; the probe lines are the only output of the
feature. The Mission04 "canister" cutscene itself (n02.nis, the Genetics Module jettisoned) moves its objects on NIS
curves, which spec item 1 already interpolates; the Workers' DockOpen / DockClose and the harvesters' JackOut / drill
animations in play and in Mission01's intro are the ones this item smooths.

## Files
- `src/Game/MeshAnim.c` (the feature), `src/Game/MotionInterp.c` / `.h` (`miRenderDrawing`), `src/SDL/main.c`
  (`/catMadProbe`), `docs/features/motion-interpolation.md` (design item 8).
