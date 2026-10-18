# Audit: M2-R6 nis-fow-camera (adversarial)

Port under audit: producer 5 of M2-R6, report `docs/m2-r6-nis-fow-camera.md`, work = `git diff a6e72c9` (no commit
after `a6e72c9`; the whole port is the working tree: 66 tracked src/ and tools/ files, the two delta-map files, plus
the untracked report, `src/Game/FOW.c` and `src/Game/FOW.h`). Every engine run went through `tools/m2_harness.sh`
(silent, headless, plain harness, `--bin` the audit binaries below); no Ghidra, no GOG game, no input. Exe bodies
were read from the export `ghidra/exports/cataclysm_gog_r6` with `tools/m2_fdec.py`, `tools/m2_fdis.py` and
`tools/m2_exedata.py`.

## 1. Clean build

`/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/build-audit-r6-nis-fow-camera`, removed
first, set up and compiled with tools/build.sh's arguments (debug: `--buildtype=debug -Db_sanitize=address,undefined`;
release: `-Doptimization=2 -Ddebug=false -Db_sanitize=none`), `werror=true` from meson.build:

| config | rc | compiler warnings | hwi-engine size (port as delivered) | after the audit fixes |
|---|---|---|---|---|
| debug | 0 | 0 | 85,989,840 B | 85,989,848 B |
| release | 0 | 0 | 5,262,008 B | 5,262,008 B |

`tools/m2_layout32.sh`: `m2_layout32: OK (all ILP32 layout asserts hold)` (now with NIS.h and FOW.h). After the fixes
(comments only) `tools/build.sh --debug` and `--release` are green too; the debug log's only "WARNING" is meson's
sanitizer note.

## 2. Plain-harness runs (audit debug binary, the port as delivered)

| label | mode | result | ASan |
|---|---|---|---|
| aud-r6nfc-postdroid-a / -b | mission 6,000 (`tools/oracle/mission_test.sh --no-perturb-run`) | both `oracle mission run done`, run.hwo 1,476,370 B, sha256 `24253cbae125485f` both (`cmp` equal, hwo_compare IDENTICAL, flipped-copy check PASS, `MISSION TEST PASS`); stdout `oracle: mission fow players=2 curPlayer=0 grid0=set grid1=NULL stealthUpdates=0 repairbots=1 droidUpdates=0` | 0 / 0 |
| aud-r6nfc-nisparse | nisparse | `nisparsetest: done nis=14 nisScripts=14 animatics=18 objects=593 events=1488 cataclysmEvents=31 unknownKeywordLines=47 failed=0 result=PASS`, no leak | 0 |
| aud-r6nfc-render | render (600) | `counts droids=2500 droidUpdates=583`, `rendertest: droids shipsWithDroids=0 ship=sProcessor#154 numDroids=5`, `result=PASS` | 0 |
| aud-r6nfc-dock | dock (5,000) | `docktest: done ... jackPausedTick=1024 result=PASS`; the harvester `sWorker#163 ... mad=0/1/1` on every late harvest line | 0 |
| aud-r6nfc-hud | hud (600) | `hudtest: done tick=599 ... result=PASS` | 0 |
| aud-r6nfc-command | command (3,000) | `commandtest: done ... result=PASS`, `commandtest: fow grid=set stealthUpdates=387` | 0 |
| aud-r6nfc-fe | fe, 120 s | Main_game_screen, still running at the timeout, 300 loads, no leak | 0 |
| aud-r6nfc-sp | sp, 360 s | universe running, `univUpdateCounter` 3,712 at the timeout (R5: 1,248; the animatics now play at their 30 fps), 3,784 loads, no leak | 0 |

All numbers equal the producer's `r6nfc-final-*` / `r6nfc-sp-final2` runs.

### Mission01 stream

- Post-droid pair (audit): `24253cbae125485f`, byte-identical; `hwo_compare` against the producer's pre-droid run
  `r6nfc-predroid-a`: `IDENTICAL: 6001 ticks and 0 packets compared`; against the managers audit's
  `aud-r6mg-mission-a` (the stream this port started from): IDENTICAL (header build identity c3e7c1f2 vs a6e72c9d only).
- Against R5's baseline `r5-integrate-mission-a` (sha256 `645f94c69d68319e`, checked): `FIRST DIVERGENT TICK 1`, 2 fields,
  `rng6` (CAND 10 draws ahead) and `rng_all`; with every other field selected (`--fields rng10,rng0,ships_n,ships_misc,
  ships_pos,ships_vel,ships_rot,ships_health,missiles,bullets,derelicts,resources,statcheck,univcheck,players,dt,total,
  real,fpcw`): IDENTICAL over 6,001 ticks. These two fields are the managers port's documented move
  (docs/audits/m2-r6-managers.md "stream"), so **no field moved for this port**.
- The producer's pre-droid pair exists: `r6nfc-predroid-a` / `-b` (runs.txt: rc 0, "oracle mission run done"), both
  sha256 `24253cbae125485f`; it equals the post-droid pair. It cannot be IDENTICAL to R5's `645f94c69d68319e` (the DONE
  WHEN's wording) because producer 4 moved rng6/rng_all before this port; it is IDENTICAL to R5 in every other field and
  to producer 4's audited stream, which the report says correctly.

## 3. Bodies compared with the exe (25 asked, 37 read)

| # | function | result |
|---|---|---|
| 1 | `nisUpdate` 0x004ce780 | same: binkDonePlaying 0x008b6d94 (initial 1, so 0 = playing) = `aviIsPlaying`, return 1e-7 (0x0072af90); ships through `cat_univSetShipPosition`, then the clamp update and lights / lens flare for shiptypes 0x8e/0x8f/0x90 (= nisLightSource1/2, nisLensflareSource1, w 0); velocity cap for path type < 7 = NUM_RACES; events and seek as the tree |
| 2 | `nisStart` 0x004cd2b0 | same (catSmHideViewport, unitCapDisable, mrNISStarting, lens flare source add, fog off, the two-stage cut with HW1's `nisFocusMaxRadius` formula inlined, the fades and 0x00a4cb4c cleared only for a new NIS); the "NisTriggerShips" lookup is hashed in the exe (finding 4) |
| 3-6 | `cat_nisRepair` 0x004cf370, `cat_nisSubtitle` 0x004cf740, `cat_nisMimic` 0x004d5fa0, `cat_nisFogSet` 0x004d3490 | same (if/else confirmed in the disassembly of Repair / Mimic; subtitle string slots 0x00bb504c.. = strNIS01, strNISLET1, strNIS02.. by the StringsOnly.h order, format `#r%x#t%x%s`, ms x 0.001; FogSet's sscanf out-argument order recomputed from the pushes at 0x004d351b..0x004d3550 and the stores after it: on, time, colour 1, colour 2, speed, density x 1e7) |
| 7-12 | IDENTICAL NIS.c bodies `nisObjectDied`, `nisSeek`, `nisFocus`, `nisMeshAnimationStartSet`, `nisDamageLevelSet`, `nisDeathDamageSet` | all the exe's (nisObjectDied's two inlined copies both stop at the first match, as the tree's `break`) |
| 13 | the dispatch table | all 75 entries of 0x0088888c match the tree's `nisEventDispatch` by function name |
| 14-15 | `madAnimationStart` 0x004b6b90, `madAnimationUpdate` 0x004b6ce0 | same (previous / current words, deferred swap, number mapping, participation bit of the number, mode last; loop / pause-at-end / stop paths read in the disassembly at 0x004b6da6..0x004b6e4e) |
| 16-17 | `droidCreateArray` 0x005c3070, `droidUpdateArray` 0x005c32f0 (+ `cat_droidRandomWalkVertex` 0x005c29e0) | same: entry init, race switch, object fields (+0x2b8 = tacticstype, 0x2b0 = 15); the 11 constants checked by value; states 0..6 and the weld effect's three varargs (0x20, 5.0, the distance) read in the disassembly at 0x005c3877..0x005c399a; the returning state's stuck path 0x005c3b3b..0x005c3c3c |
| 18 | `cat_fowCreatePlayerGrids` 0x004a38c0 | same; 0x00a43cae is `curPlayer` (the exe passes it to `universeSwitchToPlayer` and `ccFocusOnPlayersMothership`), so only the local player gets a grid |
| 19 | `cat_fowCanPlayerSeeShip` 0x0047e810 | same (disassembly: x87 1/cellSize, pushes x, y, z; SOF_Hide 0x8000; `shiplink.belongto` +0x244 returns 0) |
| 20-21 | `fowActivateCube` 0x0047eeb0, `fowDeactivateCube` 0x0047f070 | bodies the exe's; the NULL-grid early return is a deviation without a CAT-FIX mark (finding 1) |
| 22 | `cat_shipStealthDetectionUpdate` 0x0053fd90 | same (catTypeFlags +0x1ec 0x10000, upgrade 0x800000, sLeech = 3, mission 14 / player 1) |
| 23-25 | camera: `ccFocusOnPlayersMothership` 0x0042dc30, `ccCancelFocus` 0x0042e3d0, `cameraInit` 0x0042b7d0 | the first two the exe's (with the producer's one-off Ghidra field naming of CameraCommand, checked field by field); `cameraInit` clears only `.x` of the two look-at points in the exe (finding 3) |
| + | `NewSetFocusPoint`, `cat_univShipFOWUpdate` 0x00544b10, `cat_selPositionHiddenByFOW`, `univFindNearestResource` 0x00549c50, `cat_univAddShipModules` gate 0x00542bba, `processAttackToDo` prelude, `pingListDraw` 0x004e8985, the ACV / Hive / Ramming waiting sites, the beam length test 0x0043c6a9..0x0043c6fc | code the exe's; univFindNearestResource's comment is wrong (finding 2) |

Other facts re-checked: the 18 animatics are 30 fps (Bink headers: frame rate 30/1; sierra.bik 14.9999), so the
avi pacing change is right; the delta-map counts (IDENTICAL 2,942, MODIFIED 1,394, REWRITTEN 84, reviewed 2,797) and
the 183 review entries; the 75 removed `CAT-TODO`/`CAT-PROVISIONAL` lines; no tag naming this scope is left
(`M2-R6 FOW/ships/mesh/NIS/camera`, `stage 3 FOW`); every new CAT-TODO is on one line with its owner first; no HW1
alias (R1/R2/P1/P2/P3/Traders and the 36 ship-type aliases of ShipDefs.h) in code of the ported files.

## 4. Diff review

All changed files belong to the port's subsystems (NIS/animatic incl. GoK's avi pacing, camera, FOW and its call
sites, beam/clamp, mesh animation and its 25 callers, droids, the self-tests and harness). Hunks without a nearby
`CAT` marker were each traced to a function-level CAT comment or an HWI-TEST block. Edited functions the base map
calls IDENTICAL: all have an M2-R6 nis-fow-camera review entry (the 22 reclassified plus `nisCurrentTimeSet`, which
stays IDENTICAL as its removed check was HW1's `NIS_ERROR_CHECKING` debug check). The merge also rewrote 91 untagged
entries of `delta_map.json` (82 of them the managers port's): each now equals the review file committed at a6e72c9
(notes, change text, confidence; no class change), i.e. the base map had not been re-merged; not a defect.

## Findings

1. **Unmarked CAT-FIX guards in FOW.c** (convention). `fowActivateCube`, `fowDeactivateCube`, `fowGetUsageCountW`,
   `fowCubeHasUnvisited`, `fowGetVisited`, `fowGetVisitedW`, `fowWorldToGrid` and `fowIsValidW` return early on a NULL
   grid; the exe only bumps its error counter and then reads the NULL grid (decompiles of 0x0047eeb0, 0x0047f070,
   0x0047f300, 0x0047f400, 0x0047f540, 0x0047f5b0, 0x0047f6b0, 0x0047f750), so these are fault-avoiding deviations that
   need the CAT-FIX mark (fowActivate and fowGetUsageCount had it). **Fixed**: one CAT-FIX line each.
2. **Wrong comment on `univFindNearestResource`**: it said a mimic-asteroid is skipped "unless that player has not
   detected the mimic" and only for "another player"'s mimic; the exe (0x00549c50) and the code skip it when the mimic
   is the player's own, or NOT detected, or (fog 0) the tech is held and the draw fails. The recorder term
   (0x00a75188, tested at 0x00549eac) was mentioned but not tagged. **Fixed**: comment corrected, `CAT-TODO(stage 5
   recorder)` added on one line.
3. **cameraInit's undocumented deviation**: the exe clears only `lookatpoint.x` and `oldlookatpoint.x` (0x0042b7ea /
   0x0042b7ed); the tree clears all three components (HW1). Keeping that avoids uninitialised reads of a heap camera
   (nisStart's), but the review entry said only "also clears +0x55 and +0x58". **Fixed**: CAT-FIX comment in Camera.c
   and the review note extended (both map files).
4. **Missing KAS tag in nisStart**: the exe hashes "NisTriggerShips" (`cat_kasHashString`) as in the two tagged sites;
   the tree's string lookup had no tag. **Fixed**: `CAT-TODO(stage 6 KAS)` on one line.
5. **Stale review notes**: `utyNewGameStart` 0x005ab180 and `smSensorsCloseForGood` 0x0050ba90 still said the motion
   cues are `CAT-TODO(M2-R6 NIS/camera)`, which this port closed. **Fixed**: audit notes appended
   (`utyNewGameStart` added to the review file, sample `audit-R6-nis-fow-camera`), `tools/delta_map.py merge`: 3 records
   changed, counts unchanged.
6. **Report count composition**: "9 + 3, 23 in all" - the map's 23 class changes are 11 + 8 NIS (not 9:
   `nisCurrentTimeSet` stays IDENTICAL) + 4 camera (`cameraInit` was IDENTICAL too). **Fixed** with an `[audit: ...]` note.

Not findings: the DONE WHEN's "catFOWGrid non-NULL for every player" is the exe's local-player-only grid (verified,
the report says so); the stealth update runs in the command self-test (387) while Mission01 has no stealth type; the
pre-droid pair is not byte-IDENTICAL to R5 only because of producer 4's audited rng6/rng_all move.

## Precision

56 sampled checks (37 body / table comparisons, 9 harness and stream checks, 10 report / map / convention claims):
50 correct, 6 wrong (findings 1-6). **Precision = 50 / 56 = 0.89.**

## Fixes (6)

FOW.c (8 CAT-FIX lines), UnivUpdate.c (univFindNearestResource comment + CAT-TODO), NIS.c (CAT-TODO), Camera.c
(CAT-FIX comment), ghidra/delta_map_review.json + delta_map.json (cameraInit, utyNewGameStart, smSensorsCloseForGood
notes), docs/m2-r6-nis-fow-camera.md (one `[audit: ...]` note). Code changes are comments only; builds green.

Verdict: PASS-WITH-FIXES - every DONE WHEN item met on the plain harness (nisparse PASS ASan 0, local-player FOW grid as the exe, camera bodies the exe's, dock jack paused at tick 1024, render droids 2500, Mission01 unmoved and byte-identical, sp/fe ASan 0); fixed 8 unmarked CAT-FIX guards, a wrong mimic comment, an undocumented cameraInit deviation, a missing KAS tag, two stale review notes and a report count
