<!-- done: build, exe-read, bodies, grep, revert-proof, captures, fences, doc -->
# Audit: M3-R2 port 3, hyperspace-assembly

Producer report: docs/m3-r2-hyperspace-assembly.md (base 4b799cb, work = `git diff 4b799cb`, 9 files). Adversarial
audit 2026-10-24. Every engine run went through tools/m2_harness.sh (silent, offscreen); labels `m3r2-hsaa-*` under
original/scratch-m2/m2r2-harness/runs/. No GPU measurement was made (no frame time in this port), so no idle-GPU gate
and no claude-gpu-done. cycle/answers/m3-r2-user.md was read first and again before this report: nothing in it
concerns this port (its later entries are the GPU, the M3-R3 pointer and the overlay key, queued for M3-R3).

## 1. Build (from clean, separate dir)
`original/scratch-m2/build-audit-m3r2-hyperspace-assembly/{debug,release}`, the setup arguments of tools/build.sh
(debug: `--buildtype=debug -Db_sanitize=address,undefined`; release: `-Doptimization=2 -Ddebug=false
-Db_sanitize=none`), werror=true from meson.build: rc 0 / 0, **0 compiler warnings** (the only "WARNING" in a log is
meson.build:57's sanitizer note). After the audit's fixes, rebuilt again from clean: debug 89,985,040 B sha256
5c4f42b976d1e02d, release 5,730,088 B 15cf063c8f418f08, byte-equal to the frozen copies the fences ran
(original/scratch-m2/m3r2-hsa-audit-bin). tools/build.sh --debug / --release in the worktree: rc 0. The win64 cross
build (tools/cross/x86_64-w64-mingw32.ini + build/win64-paths.ini, release_win64.sh's options) in the audit dir: rc 0,
0 warnings, hwi-engine.exe 5,341,696 B. No new source file; the only GL calls added are glDisable / glClipPlane (GL
1.1). tools/m2_layout32.sh: OK. No CAT-TODO / CAT-PROVISIONAL added (`git diff 4b799cb | grep '^+' | grep -c` = 0).

## 2. The exe read independently (tools/m2_fdec.py / m2_fdis.py / m2_exedata.py)
| function | what the machine code does | producer's statement |
|---|---|---|
| hsStart 0x0048f1c0 | etgEffectCreate(..., `[static+0x3d0] * [static+0x7c] * [0x72ab3c]`, `[static+0x80] * [0x72a450]`); 0x72ab3c = 1.08, 0x72a450 = 0.25; +0x3d0 = hyperspaceBoxScale, +0x7c rightlength (SpaceObj.h asserts); the effect only when glCapFastFeature(0xbe2) | correct |
| hsOrientEffect 0x0048f070 | tumble[0] = boxScale * uplength(+0x78) * 1.08, tumble[1] = boxScale * rightlength * 1.08, tumble[2] = fwd * 0.25; then x (1 - clipt) for states 1/2, x clipt for 5/6 | correct |
| hsGetEquation 0x0048f320 | default (0,0,1,0); 1 POPUP_INTO (-1, +fwd), 2 (1, -fwd), 3 (-1, fwd*clipt), 4 (1, -fwd*clipt), 5 (-1, -fwd), 6 (1, +fwd) (1.875 = hi dword of the double 1.0) | correct: GoK lacked case 1 |
| hsContinue 0x0048f400 | displayEffect 0 and a clamp host: host hsState != 0 -> the host's equation, clip enabled; else glDisable(0x3000) and return | correct (GoK clipped by the default plane) |
| hsEnd 0x0048fd10 | glDisable; returns unless 0x8818f4 (gate state) and [esp+0x94] (displayEffect) and 1 <= hsState <= 6; hsRectangle(origin, boxScale*rightlength, uplength*boxScale*(1-clipt) / *1 / *clipt) with origin z = fwd*mag - cso.z, fwd*clipt*mag - cso.z, -+fwd*mag - cso.z | correct |
| hsShouldDisplayEffect 0x0048f1a0 | clampInfo != NULL -> (host == NULL) | correct |
| UpdateArrivingShip 0x00518170 | GONE (4): PutShipOutsideAt, state 6, wait 2 x BASE for the player's mothership or a self-hosted ship, else 3 x BASE + RANDOM x ranRandomFn(10) x [0x72a4e0]; flags \|= 0x8000 (SOF_Hide). State 6 at timeout: velocity 0, state 7, flags &= ~0x8000, hsStart(ship, SLICE_RATE, 0, hsShouldDisplayEffect) | correct |
| cat_spGetHyperspaceModuleTypes 0x0051c0c0 | the types of the ether's ships whose clamp host is a ship (+0x10 == 0) of type 0x12 (sMothership) | as the tree (SinglePlayer.c:1510) |
| cat_univCreateMothershipModules 0x005420b0 (callers: univCreateShip only) | each module type via univCreateShip, clamped to the host (clampObjToObj), flags \|= 0x40000000 (SOF_CatShipPart) | modules are separate clamped ships with their own arrival waits: as stated |

So the producer's statement of the exe's gate sizing (hyperspaceBoxScale on the effect and the rectangle, the clip
sweep unscaled +-forwardlength) and of the reveal timing (the hull 2 x BASE, the modules 3 x BASE + random, each
revealed by its own hsStart; a module clipped by its host's plane only while the host's hsState != 0) is the exe's.

**Bodies compared (12)**: hsOrientEffect, hsStart, hsGetEquation, hsContinue, hsEnd (the five edited: the tree now
matches the machine code, the assembly layer aside), UpdateArrivingShip (edited: HWI-TEST lines only), hsShouldDisplayEffect
(IDENTICAL), hsUpdate 0x0048fb60 (IDENTICAL; it inlines the tree's hsFinish, which the port edited with an HWI-TEST
line: rate 2.5, the SLICING scalars, HSF_WAIT, the gravwell / spHoldHyperspaceWindow holds, clipt -2, FINISHED after
COLLAPSE_INTO else INACTIVE: as the tree), hsRectangle 0x0048f750 (IDENTICAL: 0.54 = HS_DIST_2 times uplength on x and
rightlength on y, as the tree's), cat_spGetHyperspaceModuleTypes, cat_univCreateMothershipModules (pre-existing
differences: the exe copies the host's force where the tree copies its position, not this port's), and
UpdateArrivingShip's fleet hold at hsState 4 (the mothership waits while a fleet ship is still in state 2 / 7).

## 3. Render-only grep (done again)
* New writes: `hsAssemblyEnabled` (main.c switch), the locals of hsGateShapeGet / hsAssemblyBoxGet, the particle
  tumble (already written by hsOrientEffect), the local `visible` list via `obj.catVisibleLink` (read only in
  render.c:4252 / 4283), the HWI-TEST log lines. No write to SOF_Hide, hsState, shipHyperspaceState, timeToHyperspace,
  clipt or gamerand. hsGetEquation's only caller is hsContinue (render). `hsAssemblyPartPending` and the gate's fields
  are read only in HS.c and render.c.
* **Finding F1 (fixed)**: the pending parts were added to `visible` BEFORE the level-of-detail loop
  (rndMainViewRenderFunction), which calls lodLevelGet on every object of that list. lodLevelGet writes
  `cameraDistanceVector`, `cameraDistanceSquared` and `currentLOD` (the distance table path keeps hysteresis on the
  previous level) - fields read by UnivUpdate.c's render-list insertion (8272-8328), Gun.c, Damage.c, Clouds.c - and
  under /lodBands it added the part's time to the frame budget that raises other objects' levels. So a hidden part's
  simulation-read fields were written every frame, contradicting the report's section 4 ("no LOD pass", "its
  cameraDistanceSquared ... is not written") and the comment in the code. Fix: the block moved after the loop (before
  catLodTestMainView, so the HWI-TEST lodtest / frame-sig lines still count the parts drawn), comment marked
  `[audit: ...]`. The fences below ran on the fixed binary: every stream IDENTICAL, and the Mission01 frames byte-equal
  to the producer's m3r2-hsag-shot01-new (FULL DETAIL draws LOD 0 either way).
* Drawn paths skipped for a pending part (renderedLODs, nav lights, cloud lightning countdown, edge effect): checked
  in cat_rndRenderObjectList. rndFade, cat_rndSetupObjectFlags, cat_rndSetupObjectEffectParamsInline compute without
  writing the object.

## 4. Proof the fix is not vacuous (scratch worktree)
`git worktree add --detach original/scratch-m2/m3r2-hsa-audit-revert 4b799cb`, the producer's five src files (with
the audit's fix) copied in, then the fix's hunks reverted with the logging kept: boxScale 1.0 in hsGateShapeGet and
hsStart's right scale, hsGetEquation's POPUP_INTO case removed, hsContinue's host-inactive glDisable removed,
hsAssemblyEnabled FALSE (`/* AUDIT-REVERT */`). Debug build rc 0, 0 warnings (the worktree was removed after the runs). Runs (Mission01 NIS-first,
`--mission-test-ticks 3000 --shot 1745-1800,1802-1990/4`): m3r2-hsaa-shot01-rev vs m3r2-hsaa-shot01-new.
* Gate (hsgate: tick 1749, the Kuun-Lan id 21): reverted `now up=339.2 right=293.7` (the core spine), new `now
  up=2035.1 right=1762.1` (6.0 x), sweep 2214.5..-2214.5 both; union `x -868.1..566.3 y -518.4..518.4 z
  -1998.1..1990.0` inside the exe's gate (so the assembly layer leaves the exe's numbers).
* The first exit frames (1749 .. ~1775) are the hyperspace white-out (two colours: 567,296 white + 219,136 black bar
  pixels at 1750 / 1760 / 1770), identical in both builds; the parts can only show once it clears (1790: 65,321 pixels
  differ, the gate).
* The back modules: `tools/ppm_region.py diff --tol 40 --region 545,325,638,512` rev vs new: 1914 225, **1918 8,659,
  1922 8,647, 1926 8,625, 1930 6,773, 1934 6,756**, 1938 0, 1942 0; an independent Python count (largest channel
  difference > 40, own P6 reader) gives the same 8,659 / 8,625 / 6,756 at 1918 / 1926 / 1934. The sheet
  original/scratch-m2/m3r2-hsa-audit-png/m01-rev-vs-new.png shows the stern modules missing at 1918 / 1930 in the
  reverted build, present in the new. The reverted frames are byte-equal to the producer's base build
  (m3r2-hsa-shot01-base), the new ones to m3r2-hsag-shot01-new (0 differing pixels at 1914..1938).
* Mission02 arrival (`--teamcolour --shot 20-60,62-260/3 -- /catTeamColourTicks 300`, segment s2): gate 339.2 x 293.7
  vs 2035.1 x 1762.1 at tick 22; frames differ from 29 (58,806 px) through 221, 0 from 227
  (m02-rev-vs-new.png: a sliver at the bow vs a rectangle spanning the ship). In Mission02 the hull's exit ends at 226,
  after its modules' (208 / 209), so the back-module fault does not occur there in either build.
* Departure (Mission03 `--shot 17570-17720/2`): the reverted build cuts the EnginesLower module at 17592 / 17594 and
  restores it at 17596; the new build keeps it (m03-dep-rev-vs-new.png); frames differ from 17580 (the gate).
* The reverted build's stream equals the new one's (m3r2-hsaa-shot01-rev / -new both 62d66fdc8116cb21; shot03-rev =
  m3-a 329b6be24259b285): the fix is render-only.
* /noHsAssembly A/B with the fixed binary: Mission01 0 of 130 frames differ, Mission02 run 0 of 324: the layer has no
  visible effect in the shipped data (as the producer says); the user's items are fixed by the CAT port alone.

## 5. Fences (fixed binary; hwo_compare.py, every field incl. rng_all)
| stream | run | sha256 (16), size | result |
|---|---|---|---|
| headless Mission01 6,000 (debug) | m3r2-hsaa-mission-a / -b | 8285a210f60e4110 both, 1,476,370 B (byte-equal pair) | **IDENTICAL to m3r1i-mission-a/run.hwo (sha256 1ae66610c3f02410...) ticks 0-6000** |
| same, release | m3r2-hsaa-mission-rel-a | efacfabd75e6b220 | IDENTICAL to m3r1i-mission-rel-a |
| missiontest 01 pair | m3r2-hsaa-missiontest-a / -b | 0826eebadcbfbf76 both | 6/6 PASS done 11,424; IDENTICAL 0-11425 |
| 02 | m3r2-hsaa-m2-a | 5c10254447c28a4d | 6/6 PASS 25,936; IDENTICAL |
| 03 (+ shots) | m3r2-hsaa-m3-a | 329b6be24259b285 | 5/5 PASS 17,825; IDENTICAL |
| 09 | m3r2-hsaa-m9-a | 8b404b637819f726 | 6/6 PASS 20,617; IDENTICAL |
| 10 | m3r2-hsaa-m10-a | 1620a89f51e3d96e | 5/5 PASS 21,939; IDENTICAL |
| 11 | m3r2-hsaa-m11-a | e9d9bdac27d12d05 | 5/5 PASS 8,275; IDENTICAL |
| 13 | m3r2-hsaa-m13-a | c78510056cd1e34c | 3/3 PASS 19,283; IDENTICAL |
| 14 | m3r2-hsaa-m14-a | 1f7b6f55d4176942 | 4/4 PASS 22,338; IDENTICAL |
| 16 | m3r2-hsaa-m16-a | a35da4ac77ca9364 | 6/6 PASS 21,523; IDENTICAL |
| failed jump (Mission03) | m3r2-hsaa-fj3-a | 19f38ca54774375a | armed 17472, return 17635 PASS; IDENTICAL |
| warp start level 10 | m3r2-hsaa-warp10-a | 75bdd2c42a4e771c | arrived=35 PASS; IDENTICAL |
| replay 10-17 | m3r2-hsaa-r17-a | 47c4180abac1beb4 | IDENTICAL to m3r1i-r17-a (9,601 ticks, 4,800 packets); vs GOG's oracle01-rec.hwo: FIRST DIVERGENT TICK **2823** (ships_vel) |
| replay 10-18 | m3r2-hsaa-r18-a | ab438c220e7f1780 | IDENTICAL to m3r1i-r18-a (3,527 / 1,763); vs GOG: FIRST DIVERGENT TICK **2871** (ships_vel) |
Missions with a modular arrival (1, 2, 3, 9, 11, 13, 16) and the beast's KAS jumps (10, 14, 16) all ran. Every run ASan
0, no LeakSanitizer report. **No simulation field moved anywhere; nothing to re-baseline.**

Dumps (whole / crc-stripped, `sed 's/ crc=[0-9a-f]*//'`): motion on c0e411662fa2f527 / 428ef41479eb5749, motion
/noInterp c503c869f0dc9cdb / 3016280e9e0dd3c8, nis on 0307f3be2044c29c / a5de13a44b8d264f, nis /noInterp
86cb155acffc180e / 196bab32d244f614 (m3r2-hsaa-motion-*, -nis-*): all equal to M3-R1's, ASan 0.

fe (m3r2-hsaa-fe): Main_game_screen, ASan 0, rc 124 at 120 s. sp (m3r2-hsaa-sp, --timeout 1200): the universe through Mission12 (Mission12.btg loaded), ASan 0, no LeakSanitizer
report, rc 124 at the timeout (as M3-R1).

## 6. Diff review (git diff 4b799cb)
Scope: only the producer's 9 files (no commits after 4b799cb). CAT comments with addresses on every Cataclysm change
(hsGateShapeGet, hsOrientEffect, hsStart, hsGetEquation, hsContinue, hsEnd). Generated files: `tools/delta_map.py merge`
+ `report` on the base's delta_map.json with the producer's review file reproduce ghidra/delta_map.json and
docs/m2-r1-function-deltas.md byte for byte. Findings:
* F1 (fixed, code): see 3.
* F2 (fixed, code): five recreation-only lines in render.c without the HWI-FEATURE(hyperspace-assembly) tag (the
  `hsPending = ...` and the `i < 2 && !hsPending` loop after the lightning comment, the two `hsAssemblyPartPending(ship)))`
  continuation lines of the hsContinue / hsEnd guards, the `if (hsAssemblyEnabled)` block head): tagged (render.c now
  17 tags, 55 in all).
* F3 (fixed, review map): the tree's hsFinish (edited: HWI-TEST hsfinish line) has no row of its own; the exe inlines
  it in hsUpdate 0x0048fb60, IDENTICAL and unreviewed: compared and recorded in ghidra/delta_map_review.json, merged
  (`tools/delta_map.py merge`: records changed 1, reviewed 3,579) and docs/m2-r1-function-deltas.md regenerated.
* F4 (fixed, doc): the report's pixel counts for item 3 (7,601 / 7,590 / 7,574 / 6,056 / 6,067 and the frame-to-frame
  7,980 / 6,888 vs 2,511 / 856) do not reproduce on the producer's own files with any per-pixel metric (largest
  channel, sum, mean, luma > 40): the reproducible figures are in 4; marked `[audit: ...]` in the report.
* F5 (fixed, doc): the report's render-only proof (section 4) and render.c bullet (section 3) claimed no LOD pass
  and no cameraDistanceSquared write; true only after F1; marked. The HWI-FEATURE count (50) updated to 55.
* F6 (note, not changed): hsContinue lost GoK's `dbgAssertOrIgnore(ship->guidance.clampInfo->host->header.objtype ==
  OBJ_ShipType)` (a debug-only test that dereferenced the host before the NULL test; the exe has none) without its own
  comment; harmless.
* F7 (note): the HWI-FEATURE draws nothing different in the shipped data (A/B 0 frames): the gate and sweep already
  cover the union (Kuun-Lan and bMothership), and each pending part is drawn while the host's POPUP_OUTOF plane (z >=
  +fwd) clips it entirely. It only guards longer waits. The report says so.

## 7. DONE WHEN, re-checked
* Which arrival: Mission01's (n01.script and nislet01.script contain no hyperspace keyword; the hsarrive / hsgate lines
  give GONE 1729, hull out 1749, modules 1759..1770, hull exit over 1917, modules 1927..1938). The 10-21 logs load the
  sMothership statics and play only Animatic01 (no Mission02 / Animatic02 string). Correct.
* Exe gate sizing, module creation, reveal timing: from the machine code (2), ported with CAT lines. Correct.
* Under the feature: gate before / after against the assembly's extent in the hsgate line (core 339.2 x 293.7, exe and
  now 2035.1 x 1762.1, union 1434.4 x 1036.8 x 3988.1); parts present under the host's clip from the host's first exit
  frame (hsassembly: 6 parts from tick 1750, Mission02 from 29); one reveal / one collapse; render-side (after F1).
* Captures: parts absent before the fix at 1918..1934 and the core-sized gate (4); Mission02 and the departure the
  same. The first exit frames are white-out in Mission01, so "the part pixels in the first exit frame" cannot be seen
  there in any build.
* Missions: every arrival mission and 10 / 14 rerun PASS and IDENTICAL; the producer's 17 runs cover 04-08, 12, 15, 17
  (not rerun: no modular arrival there and the binary differs only in the pending-part block, which needs a pending
  part). Mission01 pair IDENTICAL to the baseline; replays IDENTICAL with the rung 2823 / 2871; fe / sp ASan 0.
* The user's check (report 10): `tools/play.sh` exists with `-- <engine switches>`, `/noHsAssembly` is registered in
  main.c's commandOptions and documented in play.sh's header; Mission01's opening arrival is the right moment. Correct.

## 8. Precision
Sampled producer claims checked: 28 (the 9 exe statements of 2, the which-arrival statement, the gate numbers, the
back-module absence / presence, the departure, Mission02's gate, the A/B identity, the hsassembly / hsfinish ticks, the
arrival-mission list, the 13 stream / replay / dump claims rerun as a group of 4 (streams, replays, dumps, fe), the
delta-map rows, the generated files, the win64 build, the HWI count) - wrong: 3 (the item-3 pixel counts, the
frame-to-frame counts, the render-only "no LOD pass / no cameraDistanceSquared write" claim). Precision 25 / 28 = 0.89.

Fixes in place: 6 (F1 render.c block moved; F2 five tags; F3 hsUpdate review row + merge + report; F4, F5 report
text in three places; the HWI count).

Next port's reference: the binaries 5c4f42b976d1e02d (debug) / 15cf063c8f418f08 (release); every stream and dump above
equal to M3-R1's in content.

Verdict: PASS-WITH-FIXES - the exe's gate (hyperspaceBoxScale), POPUP_INTO plane and host-inactive unclip are ported faithfully and proven non-vacuous (reverted build: core-sized gate, stern modules missing 1918-1934, EnginesLower cut at the departure); every stream, replay (rung 2823 / 2871) and dump IDENTICAL; fixed: hidden parts were run through the LOD pass (sim-read fields written), 5 untagged lines, a missing review row, unreproducible pixel counts in the report
