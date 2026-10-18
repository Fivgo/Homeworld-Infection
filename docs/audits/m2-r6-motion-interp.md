# M2-R6 motion-interp: adversarial audit

Auditor for the M2-R6 motion-interp producer (render-time motion interpolation, gameplay and NIS, with the "Smooth
motion" toggle; report docs/m2-r6-motion-interp.md, spec docs/features/motion-interpolation.md). Base `3aa4cce`
(nis-fow-camera checkpoint); the port is the whole working tree on top of it (no WIP commits). Every engine run
below went through tools/m2_harness.sh on the plain harness (silent, offscreen, no input); no Ghidra, no GOG game.

## 1. Clean builds

Separate directories, configured exactly as tools/build.sh does (`--buildtype=debug -Db_sanitize=address,undefined`;
`-Doptimization=2 -Ddebug=false -Db_sanitize=none`; werror on from meson.build):

| build | dir (original/scratch-m2/) | result | size |
|---|---|---|---|
| final tree, debug | build-audit-r6-motion-interp/debug | 313/313, 0 compiler warnings (only meson's "sanitizers on" note) | 86,605,352 B |
| final tree, release | build-audit-r6-motion-interp/release | 313/313, 0 warnings | 5,299,552 B |
| pre-feature (git archive 3aa4cce + measure-only.patch), debug | build-audit-r6-motion-interp-pre/debug | 312/312, 0 warnings | 86,162,256 B |

measure-only.patch: sha256 `a65e080d2a6b529d...` (as the report), applies cleanly to the archive (11 files: the
hooks, MotionTest.c/.h, the harness modes, nis_smoothness.py). After the audit's code fix (below) both audit builds
and `tools/build.sh --debug` / `--release` rebuilt green, 0 warnings; `tools/m2_layout32.sh`: OK.

## 2. Guard-rails (the heart of the audit)

### (a) Per-frame dumps, flags off vs the pre-feature state (independent rebuild)

| run | binary | mode | dump sha256 | ASan |
|---|---|---|---|---|
| aud-mi-pre-motion | pre | motion | `b2af0d6decdf6474` (1,431 frames) | 0 |
| aud-mi-pre-nis | pre | nis n06:26:36 | `3f04c837e605ba23` (1,440 frames) | 0 |
| aud-mi-motion-noi / aud-mi-fix-motion-noi | final (before / after the fix) | motion `/noInterp` | `b2af0d6decdf6474` | 0 |
| aud-mi-nis-noi / aud-mi-fix-nis-noi | final | nis `/noInterp` | `3f04c837e605ba23` | 0 |
| aud-mi-motion-on / aud-mi-fix-motion-on | final | motion | `d9a8661fcce6f4b3` | 0 |
| aud-mi-nis-on / aud-mi-fix-nis-on | final | nis | `074a70f17e316cd2` | 0 |
| aud-mi-motion-nonis | final | motion `/noNISInterp` | `d9a8661fcce6f4b3` (= flags on) | 0 |
| aud-mi-nis-nonis | final | nis `/noNISInterp` | `564149979fa7e06b` | 0 |

`cmp`: the pre-feature dumps and the final `/noInterp` dumps are byte-identical (motion and nis, frame CRCs
included). The pre binary was built from an archive without .git, so the dumps carry no build identity; all hashes
equal the producer's.

tools/nis_smoothness.py (default eps: 0.05 px motion, 0.01 px nis), off (`/noInterp`) -> on:

| object | off: max / p95 / mean, repeated | on: max / p95 / mean, repeated |
|---|---|---|
| motion A focus | 0.003 / 0.001 / 0.001, 1430/1430 | 0.002 / 0.001 / 0.001, 1430/1430 |
| motion B crossing | 10.152 / 10.108 / 1.114, 640/719 (8.01 of 9) | 1.149 / 1.139 / 1.126, **0/724** |
| motion C line of sight | 3.310 / 1.355 / 0.166, 1272/1430 | 0.380 / 0.334 / 0.166, 0/1430 |
| motion D at rest | 36.339 / 2.886 / 0.621, 1272/1430 | 4.162 / 2.516 / 0.615, 0/1430 |
| motion camera (world units) | 37.5 / 37.5, 1272/1430 poses | 4.168 / 4.168, 0/1430 |
| nis O9 sRecon (tracked) | 120.534 / 24.118 / 2.724, 1231/1384 (8.01 of 9) | 39.525 / 6.200 / 2.808, **0/1389** |
| nis camera | 13,589.9 / 139.4, 1290/1439 poses, 573 repeated CRCs | 13,589.9 / 178.2, 180/1439 poses, 0 repeated CRCs |

Every number equals the report's table. `/noNISInterp` alone: O9 1231/1384 (as off), camera 1290/1439 (as off).
At eps 0.1 px the flags-on O9 count is 12 (as the report says); frames 640-642 of aud-mi-nis-on show the camera's eye
moving about 4 units and its look point about 41 units a frame while O9 moves 0.07-0.1 px: a tracking shot, not
held frames. Also at eps 0.1 the flags-on motion C / D counts are 313 / 163 (their true per-frame motion is
sub-0.1 px); the DONE WHEN names only B and the tracked NIS object, which are 0 at either eps.

### (b) Mission01 streams, 6,000 ticks

| runs | setting | run.hwo | ASan |
|---|---|---|---|
| aud-mi-on-mission-a / -b (`tools/oracle/mission_test.sh --ticks 6000 --no-perturb-run`) | flags on | 1,476,370 B, `794eae2c20c68aac` both, MISSION TEST PASS | 0 / 0 |
| aud-mi-noi-mission-a / -b | `/noInterp` | `794eae2c20c68aac` both | 0 / 0 |
| aud-mi-nonis-mission-a / -b | `/noNISInterp` | `794eae2c20c68aac` both | 0 / 0 |
| aud-mi-fix-mission-on-a, aud-mi-fix-mission-noi-a (after the fix) | on, `/noInterp` | `794eae2c20c68aac` | 0 |
| aud-mi-fix-rel-mission-a / -b (release) | on, `/noInterp` | `546337dc7eb87b7b` both | - |

`cmp`: all six debug streams byte-identical. `hwo_compare` against the nis-fow-camera audit's post-droid stream
`aud-r6nfc-postdroid-a` (`24253cbae125485f`): "HEADER: ... different build identity: a6e72c9d/1fbfa32f vs
3aa4cce1/1fbfa32f", "IDENTICAL: 6001 ticks and 0 packets compared". Release vs debug: IDENTICAL, header build flags
only. Against R5's `645f94c69d68319e`: first divergent tick 1, rng6 + rng_all (the managers port's audited move), as
the report says. No oracle field moved.

### Code checks

- **Simulation writes.** Every `HWI-FEATURE(motion-interp)` site read. The only writes to simulation fields are in
  `miWriteTransform` (position, coordsys, collPosition, enginePosition) and the missile trail's iHead / nLength, all
  between `miRenderBegin` and `miRenderEnd` around `regFunctionsDraw()` (render.c) and written back from the saved
  values in reverse order (so a doubly-saved object ends at its first, simulation, copy); objects freed meanwhile are
  skipped (`miForget` clears their saved slot). `taskPeriodFraction` reads the task table only; `taskSubTick` and
  `universeTaskHandle` are new variables; `oracleVirtualFrameStep` returns `oracleVirtualFrameTicks()` unchanged
  unless a motion / nis run is requested; the console rows and options rows write only the two flags. No RNG call, no
  task clock write. The camera code inside the draw (ccControl) follows the blended focus, and a camera snap there
  calls `univUpdateRenderList` (CameraCommand.c:794), which stores camera distances computed from the blended
  positions; these are per-machine render values the universe task recomputes, and the report already lists the
  camera-derived values as an open item for the oracle-mode-a owner.
- **Side table**: malloc'd open-addressing table in MotionInterp.c (backward-shift removal checked), no struct
  changed; `tools/m2_layout32.sh` OK.
- **bsCurveUpdate / nisUpdate unchanged**: `git diff -U0 3aa4cce -- src/Game/B-Spline.c src/Game/NIS.c` has no
  removed line; the hunks are pure insertions after `bsCurveUpdate` and after `nisCamPathUpdate`.
  `nisCameraFlyCompute`, `nisUpdateTask` and the KAS files are untouched. `bsSegmentValue` keeps bsCurveUpdate's
  HW_PTR_64 NaN guard.
- **Toggle paths** (all run): `aud-mi-fix-toggle` PASS start 1/1, `-noi` PASS start 0/0, `-nonis` PASS start 1/0
  (switches); the checkbox callback sets both flags, Cancel restores, Accept writes `objInterpolate` / `nisInterpolate`
  as rows 107/108 of settings/Cataclysm.cfg and they read back; the console rows set each flag alone;
  `aud-mi-fix-console` PASS (shown 31, set 24); `aud-mi-fix-fescreens` PASS (195 screens pushed / drawn, 0 fescreen
  UBSan reports, optionstest PASS).
- **fe / sp**: `aud-mi-fix-fe` Main_game_screen, ASan 0, no leak; `aud-mi-fix-sp` universe running,
  `univUpdateCounter` 3,712 at the 360 s timeout (= mi-fin3-sp), ASan 0, no leak.
- **git**: the new files are MotionInterp.c/.h, MotionTest.c/.h, tools/nis_smoothness.py and the report; no game
  data, dump, log or build output added. No CAT:, CAT-TODO or CAT-PROVISIONAL line added (none needed: the feature
  is not in the exe).

## 3. Diff review

All 21 changed files and 5 new files read. Every change is inside the producer's scope (the feature, its
measurement modes, the toggle, the console self-test's row counts, the harness header). Delta-map status of the
edited exe functions: `memFree` 0x004ae130 is **IDENTICAL with no review entry** (finding 3); `utyTasksDispatch`
0x005acd10 is IDENTICAL with an R4 review entry (note extended); the rest (univUpdate, rndRenderTask, mrRegionDraw,
utyGameSystemsInit, opOptionsSaveSettings, opRestoreSavedSettings, feScreensLoad, ProcessCommandLine, bsCurveUpdate,
nisUpdate) are MODIFIED and reviewed. `cat_consoleConfigFileSave` has no map row by that name (Cataclysm-only code).

## Findings

1. **Stale previous transforms after the flags come back on (code, fixed).** `miCaptureTick` returned before
   advancing `miGen` when both flags were off, so the entries of the last capture before the flags went off kept the
   latest generation. After the flags came back on (checkbox, console, Cancel), `miRenderBegin` treated those entries
   as this tick's previous transforms and blended every object from where it was when interpolation was switched off
   (anything under 2,000 units away) on the frames up to the next universe tick. Fix: the generation advances first,
   on every tick (MotionInterp.c, marked "[audit: ...]"). With the flags on nothing changes; with them off
   `miRenderBegin` returns early. Re-verified: motion / nis dumps on and `/noInterp`, two mission streams, toggle,
   console, fe, sp, fescreens all as before the fix.
2. **"Nothing is written back unless the options are accepted" is wrong (doc, fixed; behaviour left as an open item).**
   `/noInterp` and `/noNISInterp` set `objInterpolate` / `nisInterpolate`, which are the variables the
   Cataclysm.cfg rows point at, and every `opOptionsSaveSettings` ends with `utyOptionsFileWrite` (Options.c:1796).
   It runs when an options screen opens (`opOptionsStartHelper`, Options.c:611), on Accept (1343), in
   `opUpdateSettings`, and at every `gameEnd` (utility.c:2404). So a switch persists into the options file. Harmless for
   harness runs (a fresh HOME and settings dir per run), but a user's A/B launch with `/noInterp` turns smooth motion
   off for later plain starts. Report corrected; the follow-up is now listed under "What is left".
3. **`memFree` (IDENTICAL) was edited without a review entry (convention, fixed).** Added an
   `[audit: M2-R6 motion-interp]` review entry to ghidra/delta_map.json (the miForget line, not a Cataclysm delta);
   `utyTasksDispatch`'s existing review note now names the two new HWI-FEATURE lines too.
   `python3 -I tools/m2_stage_counts.py` still parses the map.
4. **The report's eps default for nis_smoothness.py was stale (doc, fixed).** The measure section said
   "default 0.1 px"; the final tool defaults by mode (0.05 motion / 0.01 nis), as the DONE WHEN section and "For
   STATE.md" say.
5. **"Every line of it carries HWI-FEATURE(motion-interp)" overstated (doc, fixed).** About 60 continuation lines of
   tagged comment blocks carry no tag (`git diff -U0 3aa4cce -- src/ | grep '^+' | grep -v motion-interp`). Every code
   line does, and so does every block, so the report's grep claim holds.
6. **Spec deviation not listed among the design calls (doc, added).** Spec item 1 asks for render-only transforms
   with the simulation fields untouched. The port writes the render transforms into the simulation's own fields for
   the draw and restores them bit for bit. That is equivalent in effect, as the identical dumps and streams show, and
   it keeps the exe layouts. Added as an "[audit]" design-call bullet for the user.

Observations (not counted): the in-game Video screen's checkbox is only checked for presence by the toggle test and
drawn by the fescreens test (no game runs, so it is never clicked in game); the NIS-only objects of n06 draw without
textures (the report says so); the C / D counts at eps 0.1 above.

## Precision

43 sampled checks: builds (2), pre-feature dumps reproduced (2), `/noInterp` dumps identical (2), flags-on and
`/noNISInterp` dump hashes (4), smoothness numbers (B, O9, the A / C / D / camera rows, the eps-0.1 explanation: 4),
mission pairs and comparisons (on, `/noInterp`, `/noNISInterp`, vs post-droid, release, vs R5: 6), bsCurveUpdate /
nisUpdate unchanged and the NaN guard (3), layout32 and side-table placement (2), toggle / console / fescreens / fe /
sp (5), the switch order after the options read, the time-compression claim, the console config format, the options
rows, the measure patch hash, no game data in git, no CAT tags, the hash-table removal (8), and the report's
claims on persistence, eps default, tagging, the delta-map convention and the capture generation (5). Wrong: 5
(findings 1-5). Precision = 38 / 43 = 0.884.

## Fixes (9)

Code: MotionInterp.c `miCaptureTick` generation (1). Delta map: memFree review entry, utyTasksDispatch note (2).
Report: the tagging sentence, the eps default, the switch persistence sentence, the generation note, the spec-
deviation design call, the persistence follow-up (6). All marked "[audit: ...]".

Verdict: PASS-WITH-FIXES - every DONE WHEN item reproduced on the plain harness with independent clean builds (pre-feature dumps byte-identical to /noInterp, 0 repeated frames for B and the NIS recon, six Mission01 streams byte-identical and IDENTICAL to the post-droid stream, toggle/console/fescreens PASS, layout32 OK, fe/sp ASan 0); fixed a stale-capture blend after re-enabling, a wrong persistence claim, a missing memFree review entry and three report details
