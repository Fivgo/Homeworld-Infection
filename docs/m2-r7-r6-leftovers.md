<!-- done: plug, cursor, etg, motion, cleanup, gates -->
# M2-R7 r6-leftovers: the R6 render / UI leftovers (no stream impact)

Status: DONE (M2-R7 producer 4; base commit `9b34dda`; nothing committed by the producer, the auditor checkpoints).

Subsystems: **plug** (`rndShamelessPlug` / `rndLoadShamelessPlug`), **cursor** (the waypoint cursor 0x004e6e70, the pie
plate's line callback 0x004e6530 with `cat_pingListGet`, `selShipUnderMouse`, the `mrHoldRight` / `mrHoldLeft`
stand-ins, the hud self-test's waypoint leg), **etg** (the real32 variable parameters of every `etgEffectCreate`
caller), **motion** (the motion-interpolation follow-ups), **cleanup** (`cat_cpValueGradientCreateSized`, HW1's blob
render and the derelict mesh table in Sensors.c, the `Memory.c` tag), **gates**.

## Headline

* `grep -rn "CAT-TODO(M2-R7" src/` gives **0**. It was 4: two `M2-R7 render`, one `M2-R7 waypoint draw` and one
  `M2-R7 ETG`. `CAT-TODO(Memory.c)` is closed too. This producer wrote no new CAT-TODO.
* **The Mission01 stream is UNCHANGED.** The hash is `5bab894e28d6dc7b` before (base binary, `r7d-base-mission-a/-b`)
  and after (final binary, `r7d-final-mission-a/-b`). Each pair is byte-identical and MISSION TEST PASS. Both are
  IDENTICAL (6,001 ticks, no difference) to producer 3's audit stream `aud7c-final-mission-a` (`0ea19f070fede4c0`).
  Only the header's build identity differs, and that is the only reason the sha256 differs (`1410c223` vs
  `9b34ddaf`). No field moved, from any tick.
* **The guard-rails hold.** The `/noInterp` dumps are `b2af0d6decdf6474` for the motion mode (1,431 frames) and
  `3f04c837e605ba23` for the nis mode (1,440 frames), which are the pre-feature hashes. The toggle self-test PASSes
  with no switch, with `/noInterp` and with `/noNISInterp`.
* **The ETG item needed no ETG change.** The tree already passes the same 32-bit words as the exe:
  `Real32ToUdword` copies the bits, like HW1's `TreatAsUdword`. A review of all 47 `etgEffectCreate` call sites in
  the exe found one real mismatch: `kasfSpawnEffect` passed an integer where the exe passes a real32. It is fixed.
* **The waypoint cursor 0x004e6e70 is ported as the exe draws it, including its stream-0 draws.** Neither headless
  stream mode enters the waypoint mode, so the mission and replay streams cannot show it. The extended hud self-test
  enters it: 181 cursor draws with 606 stream-0 draws, which equals 63 + 3 x 181, so PASS.

## What changed, file by file

### plug

* **src/SDL/render.c**
  * `rndLoadShamelessPlug` 0x00579fa0 is ported from the exe body. TRUE loads `plug.tga` through `rndLoadTarga`;
    FALSE deletes the texture.
  * `rndShamelessPlug` 0x00579ff0 is ported from the disassembly, using the GL function-pointer table resolved
    through `glDLLGetProcs` 0x0055dd30 (0xb3a4c4 glGetIntegerv, 0xb3a5b8 glTexCoord2f, 0xb3a5b0 glVertex2f, ...).
    In order, it:
    * saves `GL_MATRIX_MODE` and `GL_PROJECTION_MATRIX`;
    * sets an ortho projection of the window (`rgluOrtho2D(0, W, 0, H)`) and an identity modelview;
    * turns blend on and lighting and depth test off;
    * draws one textured quad from `W - 1 - plugW` to `W - 1` (the exe's `fsub 1.0`), y 0..plugH, in white, with
      `GL_QUADS` and texcoords (0,1) (0,0) (1,0) (1,1);
    * restores the matrices and the states.
    The exe's software-renderer branch (RGLtype 0x00b3a700 == 0: glDrawPixels) does not apply (decision 28).
  * `rndRenderTask` now calls the plug after `mouseDraw` while `universePause` (0x00a6c5b4) or `catUnivFrameStep`
    (0x00a6ed74) is set. This is the exe's order at 0x0057ab78..0x0057ab8a: sync status, cursor setting, mouse, plug.
  * `rndLoadTarga` (HWI, values unchanged): it now reads the two colour-map words through `memcpy`. They sit at file
    offsets 3 and 5, and the first run that drew the plug showed two new UBSan misaligned-uword reports at
    render.c:4458/4461 (`r7d-ui1`); they are gone in `r7d-final-ui`.
* **src/SDL/render.h**: declares `rndLoadTarga`, `rndLoadShamelessPlug` and `rndShamelessPlug`.
* **src/SDL/main.c**: `mainCloseRender` frees the plug after `cpPreviewImageDelete`. This matches mainShutdownRenderer
  0x00561288 and mainLoadGL 0x005615a8, and every exe caller passes FALSE.
* **Shown by:** `plug.tga` is loaded once in `r7d-final-ui` (game-stats screen, universe paused) and in
  `r7d-final-console` (console open). It is not loaded in fe or sp, which never pause.

### cursor

* **src/Game/PiePlate.c**
  * **`cat_pieWaypointCursorDraw` (0x004e6e70)** is ported from the raw disassembly; the export does not list the
    function. Its only reference is piePointModeOnOff's waypoint case, `mov [0xa50cb8], 0x4e6e70` at 0x004e5e34.
    * Line: white (0x0088d780), from `selCentrePoint` to the first waypoint, then from waypoint to waypoint.
    * Circles: three at each waypoint, one about each axis, via `primCircleOutline3`: radius 200.0 (0x0088d774),
      32 slices (0x0088d778), spokes 0 (0x00a50ce0, .bss and never written), colour 0xff5decef (0x0088d77c).
    * Rotation: each waypoint's circles turn about that waypoint's own axis (0x00a50b40, 21 vectors) by its angle
      (0x00a50c3c). The angle grows by `(10 + r*(20-10)*2^-32) * dt` degrees, with r = `ranRandomFn(0)` drawn once
      per waypoint per frame. The angle wraps past 360.
    * dt: `cat_taskRealTimeElapsed` (0x00a6b20c) minus the previous clock (0x00a50ce8). It is 0 when the previous
      clock is 0 or later than now.
    * First draw of the run: the jitter flag (0x0088d7a8, 1 in .data) adds three stream-0 draws times 2^-32 to each
      of the 21 axes (63 draws), then clears.
    * Move-line colour: set by path type (0x00a752b4: 0 -> 0x0088d78c, 1 -> 0x0088d790, 2 -> 0x0088d794).
    * Then it calls `pieMovementCursorDraw`.
    * Depth test: turned off while `smSensorsActive` (0x00a5b1c8).
    The x87 sums are evaluated in double with one rounding at each float store.
  * `piePointModeOnOff` sets this as the waypoint mode's cursor callback. Its movement GUI sound is Cataclysm's UI id
    0x116 (pushed at 0x004e5ce5). HW1's `UI_MovementGUIon` is 0x109.
  * **`pieAllShipsToPiePlateDraw` (0x004e6530, also unlisted)** is the exe's body, read from the disassembly:
    * It does nothing while `catSmViewPieplateLines` (0x00892b64, "show friendly ships") is off.
    * It turns the depth test off with the sensors manager up and leaves it off, as the exe does.
    * It walks `universe.ShipList` (0x00b62af0); HW1 walked the render list.
    * Ships it skips:
      * dead or hidden ships, and objects that are not ships;
      * `sSwarmer` / `bSwarmer`, and `CLASS_Component` ships;
      * cloaked ships, unless `cat_shipDetectedByPlayer` (sub_53F9F0) reports them;
      * mimics that appear as a resource, an ally or a derelict;
      * the junk types `dJunk1PartA` .. `dJunk3BoilerCasing`;
      * any other ship that is not detected.
    * Every ship must also pass `cat_fowCanPlayerSeeShip(curPlayer, ship, 0)`, not be disabled by reason 1 in single
      player, and lie inside `smUniverseSize*`.
    * With the sensors manager up it also draws a line and a circle for each ping (`cat_pingListGet`; an anomaly
      ping always, the others only where the fog of war shows their position) and a magenta line for each crate
      derelict.
    * The closest-ship chime is UI id 0x118 (HW1's `UI_MoveChimes` is 0x10b).
  * `pieDistanceReadoutDraw` tests `catMrRightMode == 0` (0x00a75288; this was the mrHoldRight stand-in).
* **src/Game/Ping.c / Ping.h**: **`cat_pingListGet` 0x004e8840** returns `pingList.head` (`mov eax, ds:0x00c300f0`).
* **src/Game/Select.c: `selShipUnderMouse` 0x00506550**
  * The exe's body has one parameter; the two callers are updated. It does nothing while the right button turns the
    camera (`catMrRightMode`, 0x00a75288) and skips SOF_Hide objects.
  * Taking a ship:
    * normally only if its `cameraDistanceSquared` (+0x40) is not above the nearest so far;
    * with the sensors manager up (0x00a5b1c8), with no distance test, an ally or a ship the player can see
      (`cat_fowCanPlayerSeeShip(curPlayer, target, 0)`, the R6 item).
  * Non-ships are taken with no distance test. The nearest distance is updated before the type checks.
  * A clamped `CLASS_Component` names its host.
  * Allied and enemy ships keep the cursor object (HW1 cleared it).
  * Derelicts are taken only if they are hyperspace gates or salvageable.
  * **One HWI line stays:** HW1's `mouseCursorTextExpire` is still set. The exe has no cursor text (it writes
    0x008ba744/0x008ba748/0x00a75808 and never reads them), but the tree's `mrRegionDraw` still calls HW1's
    `mouseCursorTextDraw`, which reads the expiry.
    * Without that line, the motion guard-rail's `/noInterp` dump changed in 144 frame CRCs (frames 216..359, ticks
      88..103, exactly `TW_CURSORTEXT_DELAY_TIME` = 1 s); every position and camera field stayed equal (`r7d-motion-noi`).
    * Experiment `r7d-x-motion-noi-oldsel` (HW1's body back) gave `b2af0d6d` again, and `r7d-x-motion-noi-nullptr`
      (cursor object NULL for enemies) did not, which pins the cause on the expiry.
    * With the line, `r7d-motion-noi2` and `r7d-final-motion-noi` give `b2af0d6d`.
* **src/Game/InfoOverlay.c, TaskBar.c, src/SDL/mainrgn.c / .h**: **the `mrHoldRight` / `mrHoldLeft` stand-ins are
  retired** (decision 36).
  * Every tester now reads Cataclysm's state word:
    * `cat_moTooltipDraw` uses `catMrRightMode != 0` (0x004a79e5 reads 0x00a75288);
    * `pieDistanceReadoutDraw` the same (0x004e63c0);
    * `selShipUnderMouse` the same (0x00506550);
    * HW1's task bar too (no exe counterpart; stage 6 removes it).
  * `mrHoldLeft` was never set to anything but `mrNULL`. `mrHoldRight` was `mrCameraMotion` exactly while
    `catMrRightMode == 2`, so the tests are equivalent. [audit: not exactly: in the base, `cat_gameStatsBegin`
    (GamePick.c), `smSensorsBegin` and `smSensorsCloseForGood` (Sensors.c) set `catMrRightMode = 0` without clearing
    `mrHoldRight`, so after those the stand-in could stay `mrCameraMotion` with the state word 0; the new tests read
    the state word as the exe does, so the change is right, but it is not a pure equivalence]
  * Both pointers, their assignments and their externs are gone. `mrCameraMotion` is still called directly.
* **src/SDL/mainrgn.c `mrRegionDraw`**: in point mode the exe sets only `rndPostObjectCallback = piePointSpecDraw`
  (0x0056939a). Nothing outside piePointModeOnOff writes 0xa50cb4/b8/bc. HW1's per-frame reset of the three pie
  callbacks is removed: it replaced the waypoint cursor and gave `PIE_SPEC_MoveNoLines` its lines back.
* **The hud self-test is extended** (InfoOverlay.c `catHudSelfTestTick`, InfoOverlay.h counters, harness header).
  * 60 frames before the sensors frames, `cat_mrWaypointsStart` and three `cat_mrWaypointAdd` run (no input), and
    the cursor draws to the end of the run, in the main view and then in the sensors manager.
  * `hudtest: waypoints draws=N randoms=M` is logged. PASS needs mode `PIE_SPEC_Waypoint`, N > 0 and M == 63 + 3N.
  * Final run `r7d-final-hud`: `waypoints start tick=420 type=4 waypoints=3 cursor=waypoint`, `draws=181 randoms=606
    (expected 606)`, `result=PASS`, ASan 0.

**The waypoint draw's stream effect (decision 35).**
* Every frame in waypoint mode draws `catMrWaypointCount` stream-0 numbers. The first frame of a run draws 63 more.
* In the exe, stream 0 is also read by simulation code (`univFindNearestResource`, `cat_leechDrainEffect`,
  `droidUpdateArray`, `gameStart`, ...; docs/m2-checksum-oracle.md 2.6). So a game in which the player sets
  waypoints moves the stream-0 values the simulation reads later, by a number of draws that depends on the frame
  rate, as in the exe.
* Which runs can show it: a played game, a recorded game's playback if the user opens the waypoint mode during
  playback, and the hud self-test.
* The headless stream modes do not, because nothing enters the mode:
  * the mission mode has no input and no UI script that calls `cat_mrWaypointsStart`;
  * the replay plays packets only;
  * the ui self-test runs `cat_uiSetWaypointMode(0)`, which sets only the path type.
* So the Mission01 and replay streams stay byte-identical (above and in "gates").

### etg

* **src/Game/ResCollect.c**: the `CAT-TODO(M2-R7 ETG)` is closed with a CAT note.
  * `TurnHarvestEffectOn` 0x004f1ac0 pushes the words of the beam length and the resource radius (0x004f1bb0..0x004f1bb9).
  * The tree's `intLength` / `intWidth` are `Real32ToUdword` of the same values (Types.c: a `memcpy`). The integrate
    step's premise ("SCALECAST integers") was a misreading.
  * The effect's variable block gets the same 32-bit words, so no change in ETG.c is needed.
* **Review of every caller.**
  * I extracted the variable arguments of all 47 `call 0x00470990` sites (43 functions) from decompiled.c and the
    disassembly, and compared them with the tree's 49 `etgEffectCreate` calls.
  * The real32 callers already pass the words: Gun.c, Damage.c, Dissolve.c, Blast.c, Worker.c, Processor.c,
    RepairDroid.c, Leech.c, Slipgate.c, UnivUpdate.c (bullet / missile hit, dead ship / derelict / missile),
    GravWellGenerator.c, DFGFrigate.c, HS.c, RepairCorvette.c and RammingFrigate.c. [audit: the words are real32 in
    each, but `startRepairEffect` 0x005c22c0 (RepairCorvette.c) caps the width at 150.0 before passing it (fcomp
    0x0072bfe4 at 0x005c230b) where the tree passed the radius as it is: a value mismatch this review missed; the
    audit ported the cap and added the review entry]
  * The integer ones match too: the missile hit's fatal flag (`push ebx`, an int), sentinel 100/0 and slipgate 0.
* **src/Game/KASFunc.c `kasfSpawnEffect` 0x00497cb0**: the exe converts the KAS parameter to real32 (`fild` /
  `fstp` at 0x00497cd5) and passes its word, as HW1's lines (commented out in the tree) did. GoK passed
  `(udword)parameter`; now it is `Real32ToUdword((real32)parameter)`. No headless run reaches it (KAS missions are
  stage 6).
* `SpawnCloakingEffect` (GenericInterceptor.c, HW1's cloak generator, not in the exe) passes a `real32` straight into
  the variadic call. That is a 64-bit varargs hazard, but the function is not reachable in Cataclysm; left alone.
* The etg self-test: `r7d-final-etg`, `etg effect self-test done`. It gives the same counts as `r6-integrate-etg`
  (618 created, 3,197 deleted, 1,093 left, peak 2,515 effects / 3,012 particle blocks), ASan 0.

### motion (every line HWI-FEATURE(motion-interp), never CAT:)

* **Run-only switches** (MotionInterp.c / .h, src/SDL/main.c, src/SDL/utility.c, src/Game/Options.c):
  * `miSwitchOff(obj, nis)` remembers the options file's value, then sets the flag off for the run. It is used by
    `/noInterp`, and by `/noNISInterp`, which is now an `entryFn` instead of an `entryVr`.
  * While the switch stands, `utyOptionsFileWrite` writes the remembered values (`miOptionsFileValues` before and
    after the rows). So `opOptionsSaveSettings` never persists a switch: an options screen opening, Accept,
    `opUpdateSettings` and `gameEnd` all go through it.
  * A click on the Smooth motion checkbox (`miSmoothMotionSet`) is the user's choice and ends the override.
  * `opOptionsSaveSettings` / `opRestoreSavedSettings` save and restore the override state with the flags
    (`miSwitchStateSave/Restore`), so Cancel brings it back.
  * The console rows change only the run while a switch stands.
* **The toggle self-test is extended** (MotionTest.c). With a switch it checks four things and logs
  `toggletest: run-only` lines:
  * the start-up save writes the file's values while the run's flags stay off;
  * the switch differs from the file;
  * Cancel restores the override state;
  * the final save keeps the file's values after the start-up state is rebuilt.
  Results: `r7d-final-toggle` `start=1/1 fails=0 PASS`, `-noi` `start=0/0` PASS (`run-only save rows=2`, `final save
  rows=2`), `-non` `start=1/0` PASS.
* **Clamped objects against a blended host** (MotionInterp.c `miGameplayClamped`): after the gameplay lerp, every
  object clamped to a host the lerp wrote is drawn at its clamp offset in the host's blended frame. This uses the
  arithmetic of `cat_clampUpdateAttachedObjects`, shared with the NIS path through `miClampedWrite`. It replaces the
  sub-unit arc error of blending the child on its own. NIS-owned children are left to the NIS path.
* **Repair droids** (`miDroids`): the R6 note said the droids lag a blended host, but `cat_droidRenderArray` draws
  them inside the owner's model matrix (render.c: `glMultMatrixf` of the ship's blended transform, then the droid
  pass), so they already follow it. What was left is the droids' own owner-space walk. It is now captured at each
  tick (droids outside the ship) and blended between ticks. A droid launched this tick is not blended.
* `miRenderEnd` restores everything bit for bit (saves in reverse order), so a double save of a child is harmless.
* **Camera-derived values, judged.**
  * The replay with flags on and with `/noInterp` (`r7d-mid-replay`, `r7d-mid-replay-noi`) is byte-identical
    (`4cad08230a974195`), but its virtual clock steps one tick a frame, so it hardly blends.
  * The nis mode at 144 Hz (extra engine args `/oracleDump`) blends. On vs off over 593 ticks
    (`r7d-nishwo-on` / `-noi`), every simulation field is IDENTICAL: rng10, rng6, rng0, ships_*, missiles,
    bullets, derelicts, resources, statcheck, univcheck, players, dt, total and real.
  * Only `rng_all` differs, from tick 250. That is not caused by interpolation: two flags-on runs of the same binary
    already differ at tick 292 (`r7d-nishwo-on` / `-on2`). The likely source is the sound mixer thread's
    wall-clock draws, which `/oracleReplay` turns off for this reason.
  * The base binary behaves the same: `r7d-base-nishwo-on` vs `-noi` first differs at 250 in `rng_all`, and with
    `/noInterp` the base and final streams are IDENTICAL.
  * The motion mode cannot be judged this way. Its own script takes B's and C's paths from the live camera at tick
    48 (`MT_BASIS_TICK`), so on and off differ from tick 48 by construction (`r7d-motionhwo2-on` / `-noi`).
  * The design point stays open for the feature's owner: with flags on, the camera follows the blended focus, and
    `univUpdateRenderList` stores camera distances from it. See "What is left".

### cleanup

* **`cat_cpValueGradientCreateSized` 0x0043e180**: not ported, decision 28. Its only caller, `cpValueDraw`
  0x00440583, uses it only when `cat_glcActive()` is set, to feed `glcRectSolidTexturedScaled2` (glcompat). The
  other branch, `trRGBTextureMakeCurrent` + `primRectSolidTextured2`, is the tree's.
* **`cat_pingListGet`**: ported (see "cursor").
* **src/Game/Sensors.c / .h** (decision 42): removed, 1,730 lines.
  * Removed: `smBlobsDraw`, `smBlobDrawClear`, `smBlobDrawCloudy`, `smBlobsSortToCamera`, `smBlobListSort`,
    `smBlobDropLineDistance`, `smEnemyFogOfWarBlobCallback`, `smBlobSphereDraw` and `smCursorTextDraw`.
  * Also removed: the sensor weirdness `smSensorWeirdnessInit` / `Draw` / `smwGenerate*`, and the commented-out
    `smBlobRenderSelected`, `smAllBlobsPiePlateDraw`, `smMovePlaneDraw`, `smBabyRattleDraw` and
    `smTacticalOverlayDraw`.
  * Also removed: the blob sort globals, `smBigPoints`, the derelict mesh table `smDerelictTypeMesh` and the
    `smSensorWeirdnessInit` prototype. A CAT note marks the spot.
  * Each is absent from the exe's function list and was not called in the tree. The only remaining mention, in
    `kasfSensorsWeirdness`, is inside a comment.
  * Kept: what other modules set or the scripts read. That is the blob tweakables in `smTweaks` and
    `smSensorWeirdness`, which KAS sets and `smSave`/`smLoad` save.
  * Not removed (not blob render, uncalled, not in the exe): `smClickedOnPlayer`, `smPlayerNamesDraw` and
    `smEyeFactorGet` / `smLookFactorGet`. See "What is left".
* **src/Game/Memory.c**: `CAT-TODO(Memory.c)` is settled. The exe's `memStartup` 0x004ad5f0 seeds
  `memClearSetting` / `memFreeSetting` from crc32 of USERNAME / USERPROFILE, or from `ranRandomFn(0x1a)` each. That
  is exactly the tree's `MEM_VOLATILE_CLEARING` block, which Memory.h turns on in every build (RANDOM_ETG = 26). The
  tag's "this tree keeps GoK's constant patterns" was stale. Not the exe's: the 64-bit cookie-size guard and the
  debug print.

## Delta-map review entries

There are 12 records `[review: M2-R7 r6-leftovers]` in `ghidra/delta_map_review.json`: 2 new with sample
`port-R7-r6-leftovers` and 10 existing ones with the note appended. The format is kept (indent 1, ASCII, key order).

| address | name | entry |
|---|---|---|
| 0x00506550 | selShipUnderMouse | new, MODIFIED |
| 0x00497cb0 | kasfSpawnEffect | new, IDENTICAL to ORIG; the tree had GoK's integer |
| 0x00579fa0, 0x00579ff0 | rndLoadShamelessPlug, rndShamelessPlug | ported |
| 0x0057a910 | rndRenderTask | the plug call |
| 0x00561260 | mainShutdownRenderer | the plug free |
| 0x00579e30 | rndLoadTarga | HWI memcpy |
| 0x004f1ac0 | TurnHarvestEffectOn | change text corrected; the ETG review |
| 0x004ad5f0 | memStartup | confidence high; the stale note corrected |
| 0x004e5c90 | piePointModeOnOff | callbacks 0x004e6e70 / 0x004e6530, UI id 0x116 |
| 0x00569260 | mrRegionDraw | the pie callbacks no longer reset |
| 0x004e61b0 | pieDistanceReadoutDraw | catMrRightMode |

* Merged with `python3 -I tools/delta_map.py merge --done m2r7:r6-leftovers`: `records changed 12`. The counts are
  unchanged (IDENTICAL 2855, MODIFIED 1481, REWRITTEN 84); reviewed is now 3031.
* `docs/m2-r1-function-deltas.md` was regenerated (`delta_map.py report`).
* Not in the map, because they are cat-new or unlisted: `cat_pingListGet`, `cat_cpValueGradientCreateSized`, 0x004e6e70
  and 0x004e6530. They are recorded here.

## CAT-TODOs closed or re-targeted

* Closed: `CAT-TODO(M2-R7 render)` x2 (render.c, main.c), `CAT-TODO(M2-R7 waypoint draw)` (PiePlate.c),
  `CAT-TODO(M2-R7 ETG)` (ResCollect.c) and `CAT-TODO(Memory.c)` (Memory.c).
* Re-targeted: none.
* The changed files hold no CAT-TODO / CAT-PROVISIONAL naming M2-R6, M2-R7, stage 3 or stage 4
  (`git diff --name-only 9b34dda | xargs grep` gives nothing).

## Builds

* `tools/build.sh --debug --clean` and `--release --clean` on the final tree: rc 0, `[319/319] Linking target
  hwi-engine` (debug), 0 `warning:` and 0 ` error` lines under -Werror.
* Builds: debug 87,477,480 B, sha256 `54fd75c82b4bbdc2b3…`; release 5,415,480 B, `a6274c76daf8263609…`.
* `tools/m2_layout32.sh`: OK (Ship 1596 = 0x63C, Player 25904 = 0x6530, every assert holds).

## Plain-harness runs (final binary unless noted; all ASan 0, no unsuppressed leak)

| label | mode | milestone / result | first stop |
|---|---|---|---|
| r7d-base-mission-a/-b (base binary) | `mission_test.sh --ticks 6000 --no-perturb-run` | MISSION TEST PASS, `5bab894e28d6dc7b` both | none (rc 0) |
| r7d-final-mission-a/-b | same | MISSION TEST PASS, `5bab894e28d6dc7b` both; vs `aud7c-final-mission-a`: IDENTICAL 6,001 ticks | none (rc 0) |
| r7d-final-replay | replay oracle01 | `replay done ticks=3527 packets=1763 ... pktSyncErr=0`, `4cad08230a974195`; vs producer 3's `aud7c-final-replay-a`: IDENTICAL (3,527 ticks, 1,763 packets) | none (rc 0) |
| r7d-final-etg | etg | etg effect self-test done (618 / 3,197 / 1,093) | none |
| r7d-final-render | render | `rendertest: done ... result=PASS` | none |
| r7d-final-hud | hud | `hudtest: waypoints draws=181 randoms=606 (expected 606)`, `result=PASS` | none |
| r7d-final-ui | ui | `uitest: done ... result=PASS` (plug.tga loaded; the render.c UBSan reports gone) | none |
| r7d-final-console | console | `consoletest: done ... result=PASS` (plug.tga loaded) | none |
| r7d-final-toggle / -noi / -non | toggle | PASS x3 (`fails=0`) | none |
| r7d-final-motion-noi | motion `-- /noInterp` | PASS, dump `b2af0d6decdf6474` (= pre-feature) | none |
| r7d-final-nis-noi | nis `-- /noInterp` | done, dump `3f04c837e605ba23` (= pre-feature) | none |
| r7d-final-fe | fe, 90 s | Main_game_screen | still running at the timeout (rc 124, as designed) |
| r7d-final-sp | sp, 360 s | universe running, univUpdateCounter 3,600 | still running at the timeout (rc 124) |

Experiments, not gates:
* `r7d-x-motion-noi-oldsel` / `-nullptr`: the selShipUnderMouse cause.
* `r7d-motionhwo2-*`, `r7d-nishwo-*`, `r7d-base-nishwo-*` (base binary built from `git archive 9b34dda` in the
  scratchpad, run with `--bin`), `r7d-mid-*`: the camera-derived values.
* `r7d-x-nishwo-noclamp`: the clamp pass is not the rng_all source.

Replay comparison command: `python3 -I tools/oracle/hwo_compare.py <runs>/aud7c-final-replay-a/run.hwo
<runs>/r7d-final-replay/run.hwo`. This producer did not move the replay, so it is compared with producer 3's final
replay, not with the GOG recording; that comparison is unchanged from producer 1's.

## What is left (owner)

* **HW1's cursor text** (`mouseCursorTextDraw`, called by `mrRegionDraw`) has no Cataclysm counterpart. Removing it,
  and with it the HWI expiry line in `selShipUnderMouse`, changes the frame CRCs of the motion guard-rail dump in the
  frames where the text shows. Owner: the user's ruling on re-baselining the guard-rail CRCs, or the integrate step.
* **UI sound ids**: 30 more `soundEvent(NULL, UI_*)` sites in src/Game and src/SDL use HW1's numbering. The two
  checked here are Cataclysm's HW1 id + 13 (0x109 -> 0x116, 0x10b -> 0x118). Owner: the sound owner or the
  integrate step; each site needs its exe push read.
* **Task.c / Task.h comment**: `cat_taskRealTimeElapsed` is annotated 0x00bb3270, but the symbol table says
  0x00a6b20c (0x00bb3270 is `taskNumberCalls`). Comment only. Owner: the next Task.c reader. [audit: fixed in the
  audit: the exe stores the clock at 0x0052a8f4 (`fstp ds:0xa6b20c`); both comments now say 0x00a6b20c]
* **Uncalled HW1 sensors helpers** that are not blob render: `smClickedOnPlayer`, `smPlayerNamesDraw`,
  `smEyeFactorGet` and `smLookFactorGet`. Owner: the next Sensors.c reader.
* **Camera-derived values with interpolation on**: measured above with no simulation field moved. The design point
  remains: with flags on, the camera follows the blended focus, and `univUpdateRenderList` stores camera distances
  from it. Owner: the high-refresh-4k feature / the user's ruling.
* `rndLoadTarga` wraps with GL_CLAMP_TO_EDGE where the exe uses GL_REPEAT (GoK's choice, R5 review). This is visible
  only on the plug's edge texels with NEAREST filtering.
