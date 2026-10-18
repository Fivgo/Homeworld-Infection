# Audit: M2-R7 r6-leftovers (producer 4: the R6 render / UI leftovers)

Auditor: adversarial audit of `docs/m2-r7-r6-leftovers.md` and of the working-tree diff against base `9b34dda`. The
producer made no commits after `9b34dda`, so the port is the working tree only (27 files; no user or orchestrator commits
in the range). The GOG game and the oracle DLL were never run; every engine run went through `tools/m2_harness.sh`
(silent, offscreen); no Ghidra.

## 1. Clean build

`rm -rf original/scratch-m2/build-audit-r7-r6-leftovers`, then `meson setup` / `meson compile` with tools/build.sh's
arguments (debug: `--buildtype=debug -Db_sanitize=address,undefined`; release: `-Doptimization=2 -Ddebug=false
-Db_sanitize=none`; `werror=true` is the project default in meson.build):

| build | rc | `warning:` lines | size | sha256 |
|---|---|---|---|---|
| debug, producer's tree | 0 | 0 | 87,488,104 B | `086f1e564f4d6a76…` |
| release, producer's tree | 0 | 0 | 5,415,480 B | `4eb5d908bdf13636…` |
| debug, after the audit fixes | 0 | 0 | 87,488,112 B | `03cd9cf15f17176d…` |
| release, after the audit fixes | 0 | 0 | 5,415,480 B | `64391004caf5e436…` |

(The debug size differs from the producer's 87,477,480 B by the build directory's path strings in the debug info.)
`tools/m2_layout32.sh`: `m2_layout32: OK` (Ship 1596 = 0x63C, Player 25904 = 0x6530).

## 2. Bodies compared with the exe (tools/m2_fdis.py, tools/m2_fdec.py, tools/m2_exedata.py, the raw exe.dis)

| # | body | result |
|---|---|---|
| 1 | `rndLoadShamelessPlug` 0x00579fa0 | as the tree: load `plug.tga` into 0x00a75dc0 with the size at 0x00b37800 / 0x00b377a0, or glDeleteTextures and clear. Correct. |
| 2 | `rndShamelessPlug` 0x00579ff0 | walked the stack frame: glGetIntegerv(0xba0) / glGetFloatv(0xba7), projection identity, `rgluOrtho2D(0, W, 0, H)` (0x0057dc80 with the doubles in that order), modelview push + identity, glIsEnabled(GL_BLEND), the inlined `rndLightingEnable(FALSE)` (0x008baac8), depth test off, `W - 1.0` (0x0072a4c4 = 1.0), the inlined `rndTextureEnable(TRUE)` / `rndAdditiveBlends(FALSE)`, `trClearCurrent`, bind, colour 255x4, GL_QUADS with texcoords (0,1)(0,0)(1,0)(1,1) at (left, h)(left, 0)(W-1, 0)(W-1, h), pop, projection reload, modelview back only if it was the mode, lighting / blend / depth restored. Matches the tree line for line. Correct. |
| 3 | the plug call in `rndRenderTask` (0x0057ab78..0x0057ab8a) | `universePause` 0x00a6c5b4 or 0x00a6ed74 (the tree's `catUnivFrameStep`), after `mouseDraw`. Correct. |
| 4 | the waypoint cursor 0x004e6e70 (raw disassembly; not in the export) | depth test off under `smSensorsActive`; dt = now - last unless last > now or last == 0, rounded once at the float store; the jitter loop runs 21 times ((0xa50c40 - 0xa50b44) / 12), x / y / z each `ranRandomFn(0) * 2^-32 + axis` with one rounding; per waypoint the white line (selCentrePoint for the first), the angle `((r * (20 - 10)) * 2^-32 + 10) * dt + angle`, `fst` then `-360` from the unrounded value when > 360; three circles via `primCircleOutline3(&centre, 200.0, 32, [0x00a50ce0], 0xff5decef, axis)` inside push / translate / rotate / pop; the circle centre on the exe's stack is (0, 0, 0) (its x is the zeroed high word of the qword temp); then 0x00a752b4 selects 0x0088d78c / 90 / 94 into 0x00c30138 (moveLineColor) and `pieMovementCursorDraw(distance)` (0x004e6c50). Data read: 0x43480000 (200.0), 0x20, 0xff5decef, 0xffffffff, 10.0, 20.0, 0xfff7b591, 0xffcd59ac, 0xffad7982, flag 1 at 0x0088d7a8, 360.0 at 0x0072a6b4, 2^-32 at 0x0072a4e0. 0x00a50ce0 is only ever read (three loads). Correct (the exe re-reads `smSensorsActive` before re-enabling the depth test; the tree keeps its own flag, equivalent). |
| 5 | `piePointModeOnOff` | `push 0x116` / `soundEventPlay` at 0x004e5ce5; the waypoint case writes 0x004e6e70 to 0xa50cb8 at 0x004e5e34. Every write of 0xa50cb4 / b8 / bc in the exe is in this function (15 writes, all 0x004e5cb2..0x004e5e34). Correct. |
| 6 | the pie line callback 0x004e6530 (raw disassembly) | the 0x00892b64 early return, depth off under sensors, the ship list 0x00b62af0, the filters (SOF_Dead 0x800, objtype, SOF_Hide 0x8000, shiptype 9 / 43 = sSwarmer / bSwarmer, class 10 = CLASS_Component; own ships; `cat_shipIsCloakedFully` 0x00436110 then sub_53F9F0 with the player's index; the mimic flag +0x18 & 8 with `cat_mimicAppearsAsResource` / `AlliedTo` / `AsDerelict`; shiptypes 0x74..0x79 = dJunk1PartA..dJunk3BoilerCasing; sub_53F9F0 with 0x00b674fa), `cat_fowCanPlayerSeeShip(curPlayer, ship, 0)`, `singlePlayerGame && ship+0x1e0` (= `catDisableReason[1]`, ShipOrDerelict +0x1DC asserted), the universe box; the pings (`cat_pingListGet`, TOMask +0x54 != 4 → `cat_selPositionHiddenByFOW` 0x00509400, line in `c` +0x10, circle of `minScreenSize` +0x1c with the segment table lookup = `pieCircleSegmentsCompute`, no index increment), the crates (DerelictList 0x00b62b14, derelicttype +0x290 == 0 = Crate, the FOW test, line 0xff00ffff), the closest-ship logic and `soundEventPlay(0, 0x118, 0)`. Ping offsets checked against Ping.h's 32-bit layout. Correct. |
| 7 | `cat_pingListGet` 0x004e8840 | `mov eax, ds:0xc300f0; ret`. Correct. |
| 8 | `selShipUnderMouse` 0x00506550 | decompile and disassembly: 0x00a75288 and SOF_Hide early returns; ships: with sensors up `allianceIsShipAlly` (0x00420ec0) else `cat_fowCanPlayerSeeShip`, otherwise skip when `[+0x40] > 0x008ba73c`; the nearest distance stored before the type checks; component (class 10) with clampInfo +0x1bc → `*clampInfo` = host (ClampInfo.host is +0x00); own / friendly-status ships, 0x62, ally (-4), enemy (-1) keep the object; derelicts: type 7 (HyperspaceGate) or salvageable; else MC_RESOURCE (-2). Correct; the one HWI line (cursor-text expiry) is marked HWI with its reason. |
| 9 | `mrRegionDraw` 0x0056939a | only `mov [0xa75d6c], 0x4e7a70` (the post-object callback). Correct. |
| 10 | `TurnHarvestEffectOn` 0x004f1ac0 | frame walked: the fsqrt result is stored in the ship argument's slot (E+4), the radius is E+0x10; `push [E+0x10]; push [E+4]` → (…, 2, dist word, radius word). The tree passes `Real32ToUdword` of the same values. Correct. |
| 11 | `kasfSpawnEffect` 0x00497cb0 | `fild [param]` stored as real32 into the argument slot and pushed last. Correct. |
| 12 | `startRepairEffect` 0x005c22c0 (the second ETG caller, one the producer listed as matching) | **WRONG**: `fld [radius]; fcomp 0x0072bfe4 (150.0)`; when larger, `0x43160000` (150.0) is stored over it before the push. The tree (and HW1's ad1bb82 source) pass the radius uncapped; the delta map called it IDENTICAL with no review. Finding F1. |
| 13 | the exe's etgEffectCreate sites | 47 `call 0x470990` in exe.dis; the tree has 49 calls outside the definition (50 occurrences). Producer's counts correct. |
| 14 | `cat_cpValueGradientCreateSized` 0x0043e180 | one caller, `cpValueDraw` 0x00440583, behind `cat_glcActive` 0x0055c480 (the else branch is 0x00586890 / 0x00570010). Not ported under decision 28: correct. |
| 15 | `memStartup` 0x004ad5f0 | crc32 of USERNAME / USERPROFILE else `ranRandomFn(0x1a)`, then `memPoolReset(&memMainPool, wholePool, poolLength, 1)`. The tree's `MEM_VOLATILE_CLEARING` block (1 at the top of Memory.h, outside the debug switch) with `RANDOM_ETG` 26. Settled correctly. |
| 16 | `cat_moTooltipDraw` 0x004a79e5 | `mov eax, ds:0xa75288; test; jne`. Correct. |
| 17 | `pieDistanceReadoutDraw` 0x004e63c0 | `mov eax, ds:0xa75288; test; jne`. Correct. |

## 3. Diff review

* **Scope.** Every changed file is in the producer's list; every subsystem is in row 4 of the M2-R7 plan
  (docs/m2-r6-summary.md) or its "What is left of stage 3" items 2-6 and 11. No out-of-scope change.
* **Sensors.c cleanup.** A function-level comparison of `git show 9b34dda:src/Game/Sensors.c` and the new file: 19
  definitions removed (the blob render, the weirdness helpers and the commented-out HW1 blob functions), none added, none
  of the 52 kept bodies changed (the large diff hunks for `smZoomUpdate` / `smSensorsCloseForGood` are diff alignment).
  None of the removed names is in the export's functions.csv or symbols.csv; the only remaining mention is inside a
  comment block of `kasfSensorsWeirdness`.
* **HWI-FEATURE(motion-interp) sites.** `miSwitchOff`, `miOptionsFileValues`, `miSwitchStateSave/Restore`,
  `miSwitchStands`, `NoInterpSet`, `NoNISInterpSet`, the utility.c / Options.c hooks write only the two interpolation
  flags and the module's own statics; `miClampedWrite`, `miGameplayClamped` and `miDroids(TRUE)` write object transforms
  only after `miSave` of that object, which `miRenderEnd` restores; `miDroids(FALSE)` (at capture) writes the side table
  only. No simulation field is written outside the begin / end bracket. MotionTest.c is test code.
* **CAT comments / one-line CAT-TODOs.** The port writes no CAT-TODO; `grep -rn "CAT-TODO(M2-R7" src/` gives **0** and no
  CAT-TODO / CAT-PROVISIONAL in the changed files names M2-R6, M2-R7, stage 3 or stage 4. Every Cataclysm change carries
  a `CAT:` comment (the two `selCircleCompute` call lines rely on `selShipUnderMouse`'s header note; accepted).
* **Delta map.** The 12 review records are present (2 new, 10 appended, format kept: a json round trip of the file is
  byte-identical); the counts before my merge were IDENTICAL 2855 / MODIFIED 1481 / REWRITTEN 84 / reviewed 3031, as the
  report says. Edited functions the map calls IDENTICAL without a review entry: none (`selCircleCompute`, whose call
  changed, is MODIFIED).

## 4. Findings

* **F1 (code, fixed):** `startRepairEffect` 0x005c22c0 caps the beam width (the target's collision radius) at 150.0
  (`fcomp 0x0072bfe4` at 0x005c230b, `mov [E+8], 0x43160000`); the tree passed it uncapped, and the producer's review of
  every `etgEffectCreate` caller listed RepairCorvette.c as matching. Fixed in src/Ships/RepairCorvette.c with a `CAT:`
  comment; review entry `0x005c22c0` (MODIFIED, sample `audit-R7-r6-leftovers`) added to ghidra/delta_map_review.json,
  merged (`tools/delta_map.py merge`: records changed 1; IDENTICAL 2854, MODIFIED 1482, REWRITTEN 84, reviewed 3032;
  only that record and the counts changed against a copy taken before) and docs/m2-r1-function-deltas.md regenerated
  (`tools/delta_map.py report`). The Mission01 and replay streams are unchanged by it (section 5).
* **F2 (doc, fixed):** the report's "mrHoldRight was mrCameraMotion exactly while catMrRightMode == 2, so the tests are
  equivalent" is not exact: in the base, `cat_gameStatsBegin` (GamePick.c), `smSensorsBegin` and
  `smSensorsCloseForGood` (Sensors.c) zero `catMrRightMode` without clearing `mrHoldRight`. The new tests follow the exe,
  so the code is right; the report sentence is annotated `[audit: ...]`.
* **F3 (comment, fixed):** the producer's "What is left" item on Task.c is correct (the exe stores the clock with
  `fstp ds:0xa6b20c` at 0x0052a8f4; 0x00bb3270 is `taskNumberCalls`, a byte written at 0x00463032). The audit fixed both
  comments (src/Game/Task.c, src/Game/Task.h) and annotated the report item.

## 5. DONE WHEN, re-run on the plain harness (audit debug build; all ASan 0, no unsuppressed leak)

| label | mode | result |
|---|---|---|
| aud7d-mission-a / -b | `mission_test.sh --ticks 6000 --no-perturb-run` | MISSION TEST PASS, byte-identical `5bab894e28d6dc7b`; vs `aud7c-final-mission-a` (producer 3's audit, `0ea19f070fede4c0`): IDENTICAL, 6,001 ticks, header build identity only (1410c223 vs 9b34ddaf) |
| aud7d-motion-noi | motion `-- /noInterp` | PASS, 1,431 frames, `b2af0d6decdf6474` (= pre-feature) |
| aud7d-nis-noi | nis `-- /noInterp` | PASS, 1,440 frames, `3f04c837e605ba23` (= pre-feature) |
| aud7d-toggle / -noi / -non | toggle (none, `/noInterp`, `/noNISInterp`) | `fails=0 result=PASS` x3; start 1/1, 0/0, 1/0; run-only rows=2 |
| aud7d-etg | etg | done: 618 created, 3,197 deleted, 1,093 left, peak 2,515 / 3,012 (= r6-integrate-etg) |
| aud7d-render | render | `result=PASS` (droids=2500) |
| aud7d-hud | hud | `waypoints start tick=420 type=4 waypoints=3 cursor=waypoint`, `draws=181 randoms=606 (expected 606)`, `result=PASS` |
| aud7d-ui / aud7d-console | ui / console | `result=PASS`; `plug.tga` in both FileLoads.log, not in fe |
| aud7d-replay | replay oracle01 | `ticks=3527 packets=1763 pktSyncErr=0`, `4cad08230a974195`; vs `aud7c-final-replay-a`: IDENTICAL (3,527 ticks, 1,763 packets) |
| aud7d-fe | fe, 90 s | Main_game_screen (rc 124 at the timeout, as designed) |
| aud7d-sp | sp, 360 s | universe running, univUpdateCounter 3,600 (rc 124 at the timeout) |
| after the F1 fix: aud7d-final-mission-a / -b | `mission_test.sh --ticks 6000 --no-perturb-run` | MISSION TEST PASS, `5bab894e28d6dc7b` both; vs `aud7c-final-mission-a`: IDENTICAL 6,001 ticks |
| after the F1 fix: aud7d-final-replay, -etg, -motion-noi, -render | replay / etg / motion `/noInterp` / render | replay `4cad08230a974195` IDENTICAL to `aud7c-final-replay-a`; etg counts unchanged; motion `b2af0d6decdf6474`; render PASS |

Commands: `python3 -I tools/oracle/hwo_compare.py <runs>/aud7c-final-mission-a/run.hwo <runs>/aud7d-final-mission-a/run.hwo`
and `python3 -I tools/oracle/hwo_compare.py <runs>/aud7c-final-replay-a/run.hwo <runs>/aud7d-final-replay/run.hwo`. The
producer's claim holds: the Mission01 stream is unchanged by this port (no field moved, from any tick).

## 6. Precision

32 sampled checks: the 17 bodies of section 2, the etg call-site counts, the Sensors removal claims, the HWI-FEATURE
no-simulation-write claim, the mrHoldRight equivalence claim, the Task.c claim, the delta-map counts, and the gates
(motion, nis, toggle x3, etg / render / hud / ui / console, mission, replay, fe / sp, the CAT-TODO grep, the builds,
layout32). Wrong: 2 (F1: startRepairEffect listed as matching; F2: the equivalence claim). **Precision 30 / 32 =
0.9375.**

## 7. Fixes (5)

1. src/Ships/RepairCorvette.c: the 150.0 width cap of `startRepairEffect` (F1).
2. ghidra/delta_map_review.json `0x005c22c0` + merge into ghidra/delta_map.json + docs/m2-r1-function-deltas.md (F1).
3. src/Game/Task.c, src/Game/Task.h: `cat_taskRealTimeElapsed` is 0x00a6b20c (F3).
4. docs/m2-r7-r6-leftovers.md: `[audit: ...]` on the mrHoldRight equivalence sentence (F2).
5. docs/m2-r7-r6-leftovers.md: `[audit: ...]` on the RepairCorvette ETG line and on the Task.c "What is left" item.

## Mission stream for the next port

Final pair `aud7d-final-mission-a` = `-b` (audit debug build after the fixes, 6,000 ticks):
**sha256 `5bab894e28d6dc7b479cf1806e225d58abaaf8e032f4a0859949fb05745e94cd`**, IDENTICAL in every record to producer 3's
`0ea19f070fede4c0` (only the header's build identity differs).

Verdict: PASS-WITH-FIXES - render / UI leftovers ported at the exe's in 17 compared bodies, guard-rails and self-tests pass, mission and replay streams unchanged; fixed the startRepairEffect 150.0 width cap the ETG review missed, the Task.c address and two report statements
