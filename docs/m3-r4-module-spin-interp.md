# M3-R4 ad hoc: an ejected module's spin at the frame rate (HWI-FEATURE(motion-interp) module spin), 2026-10-28

Landed from the user's terminal session 38027cce between rounds, after mesh-anim-interp (654b900) and sound-burst-log
(b5e82c3).

## The user's words (verbatim)
- "for mission 2, constructing the engineering module triggers the ingame cutscene that jettisons the ore canister. I
  still see its animation at the low refresh rate"

## What the "cutscene" is
It is not an NIS (Mission02.kas plays none; only n01.script has a `meshAnimStart`) and not a mesh animation (the
mesh-anim-interp item): it is engine-side module code. The Kuun-Lan of Mission01 / 02 carries `sMothershipOreCanArmour`
(the ore canister, `cat_univMothershipModuleTypesGet`'s mission12Modules) clamped to it. Building one of the four
modules that sit under it (sMothershipSpecial, Armour, Micro, Weapons: the construction manager's external build
creates the module inside the Kuun-Lan, `clCreateShip` launches it, `processLaunchShipToDo` 0x00466b00) calls
`cat_moduleShipEject(can, TRUE, 7 s, 10 s)` (0x005af940): the canister is unclamped, given an eject push (a random
force of 50,000-70,000 along the host's up / right vectors, `ModuleShipHouseKeep` adds it to the force every tick) and
a spin (`spinning`, `spinAxis` = its up vector), and explodes when the push ends. Mission02.kas then focuses the camera
on the canister (`kasfCameraFocusClose`) and plays the Fleet Intelligence line.

## What stepped
The drift is physics, so the gameplay lerp (spec item 0) blended it, and the camera followed smoothly. The spin is
`spec->spinAngle += 0.05` once per universe tick in `ModuleShipHouseKeep` (0x005af800, wrapping at 2 pi), and the
renderer (`rndMainViewRender`, the exe's 0x00577d92) drew `glRotatef(spinAngle)` about the collision-sphere centre in
every frame of the tick: 2.9 degrees per step at 16 Hz, three or four frames per step at 60 fps, while the canister's
position moved at the frame rate under it. That is the user's "animation at the low refresh rate".

## The change (ModuleShip.c / .h, render.c, MeshAnim.h)
- `cat_moduleShipDrawnSpinAngle(module)`: `spinAngle + alpha * MODULE_SPIN_STEP` where alpha is `miRenderAlpha()` (the
  frame's alpha of the ships' own blend, M3-R1 extrapolation and M3-R3 pause hold included), only between
  `miRenderBegin` and `miRenderEnd` (`miRenderDrawing`) and only while the next housekeep would step the angle
  (`ejecting == 1` and `totaltimeelapsed + phystimeelapsed <= ejectEndTime`: the housekeep's own test one tick on);
  `spinAngle` otherwise (alpha <= 0, the toggle off, the push over). The simulation's `spinAngle` is never written, so
  the oracle stream and the save game are untouched and `/noInterp` draws the exe's angle. The wrap at 2 pi is
  modular for `glRotatef`, so the frame drawn past it (6.30 rad) meets the next tick's 0.
- `render.c`: the one `glRotatef` call takes the drawn angle.
- HWI-TEST: `/catMadProbe` (the mesh-anim probe switch, now declared in MeshAnim.h) also logs
  `spinprobe: t= tick= alpha= ship= sim= drawn= step= end=` for every interpolated frame of a spinning module (the
  first 600 lines).
- HWI-TEST scene kind `eject` (MotionTest.c, `--scene eject[:..]`): O is the ore-can armour clamped to the player's
  mothership, the camera focuses on it, no Recons are needed, M stays put, and at the order tick O is jettisoned
  exactly as a module launch does (`cat_moduleShipEject(o, TRUE, 7.0f, 10.0f)`); PASS = frames sampled and O still
  ejecting. The harness's mission mode has no mission selector (it is the campaign start), so the scene runs in
  missiontest mode with `--mission 2` (the Kuun-Lan is out of hyperspace at tick 190; the scene starts at 240).

## Fence (debug builds of the worktree, ASan 0 in every run, both builds 0 warnings)
Runs in `original/scratch-m2/m2r2-harness/runs/`.

| run | what | result |
|---|---|---|
| m3r4-msi-eject | Mission02 self-test `--scene eject:0:500:200:240 --real-clock 60`, `/catMadProbe` | O = sMothershipOreCanArmour#5 ejected at tick 272 (end 34.00 s); 600 spinprobe lines (the cap) over ticks 272-439, 3.57 frames per tick; every drawn step = alpha * 0.05 (0 misses at 1e-4); the drawn angle continuous across ticks (worst gap between a tick's last frame and the next tick's angle 0.019 rad, modulo the wrap at tick 398: 6.3024 drawn, then 0); scene PASS; ASan 0 |
| m3r4-msi-eject-noi | the same with `/noInterp` | O ejected the same, 0 spinprobe lines (the toggle off: the tick's angle, the exe's); scene PASS; ASan 0 |
| m3r4-msi-m2 | Mission02 self-test, 90000 ticks, `/catMadProbe` | IDENTICAL to m3r3i-e-m2-a (25905 ticks), 6/6 objectives PASS; 0 probe lines because the headless self-test draws no interpolated frame; ASan 0 |
| m3r4-msi-mission | `--mode mission --mission-ticks 6000` | IDENTICAL to m3r3i-e-mission-a (6001 ticks); ASan 0 |
| m3r4-msi-m2-natural | Mission02 self-test, 1500 ticks, `--scene eject:0:500:160:1200` (the virtual clock, 9 frames a tick), `/catMadProbe`: the self-test's own Build event (tick 1192, sMothershipMicro) is the user's path | the canister ejected by the module's launch at tick 1192 (end 91.56 s = 74.56 + 7 + 10): 600 spinprobe lines from tick 1193 (8 interpolated frames a tick), every step = alpha * 0.05, continuity 0.0056 rad; the scene itself reports FAIL as expected (O was already unclamped when the scene started at tick 1200: "no clamped canister"), and the mission's 0/6 is the 1500-tick cut-off; ASan 0 |

The streams are identical because the drawn angle is computed from the simulation's and never written back.

## Files
- `src/Ships/ModuleShip.c` / `.h` (the feature and the probe), `src/SDL/render.c` (the call), `src/Game/MeshAnim.h`
  (`catMadProbeOn`), `src/Game/MotionTest.c` (scene kind `eject`), `tools/m2_harness.sh` (the kind's note),
  `docs/features/motion-interpolation.md` (design item 9).
