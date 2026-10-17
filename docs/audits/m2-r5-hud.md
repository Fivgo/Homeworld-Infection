# M2-R5 audit: HUD (producer 5)

Auditor: adversarial audit of `docs/m2-r5-hud.md` (status bars and picking, ship info panel, ship names, overlay icons,
main overlay and message scroller, pie plate, escape menu / loading screen, `/catHudTest`). Port work = `git diff a47a8a6`
in the m2 worktree (the working tree; no commit after a47a8a6, the producer committed nothing). Reference: the r6 decompile
(`ghidra/exports/cataclysm_gog_r6`, `tools/m2_fdec.py`), the exe disassembly (`tools/m2_fdis.py`) and exe data read with
`tools/m2_exedata.py`. Ghidra was not run. Every engine run went through `tools/m2_harness.sh` (or
`tools/oracle/mission_test.sh`, which calls it): silent, offscreen, no input.

## 1. Clean builds

`rm -rf original/scratch-m2/build-audit-r5-hud`, then `meson setup` with tools/build.sh's arguments (debug:
`--buildtype=debug -Db_sanitize=address,undefined`; release: `-Doptimization=2 -Ddebug=false -Db_sanitize=none`;
`werror=true` from meson.build) and `meson compile`: both green, 0 compiler warnings (the only "WARNING" is meson's
sanitizer notice). Sizes: debug `hwi-engine` 82,315,256 B, release 5,023,224 B (82,315,264 / 5,023,224 after the audit
fixes). The worktree's `tools/build.sh --debug` / `--release` green after every fix; `tools/m2_layout32.sh`: OK.

## 2. Bodies and data compared with the exe

| # | function / data | result |
|---|---|---|
| 1 | cat_selHealthColorsGet 0x00507b00 | as the exe (green / yellow factor thresholds, strict `<`) |
| 2 | cat_selHealthFactorGet 0x00507b70 | as the exe (`oneOverMaxHealth` +0xa8 times health +0x170, clamp to [0, 1]) |
| 3 | cat_selStatusRectCompute 0x00507390 | as the exe, read in the disassembly: nearest rounding of the centre and radius (1.5*2^52 trick), width 1.6 / height 0.03125 of the radius pixels, minimums 8 / 2, `x0 = -2 - trunc(w/2) + x + 1`, `y0 = y - trunc(pixels + h)`, `x1 = trunc(w - 2h) + x0 + 4`; the float `w - 2h` truncates as the x87 value for every radius 0..19,999 (checked) |
| 4 | cat_selStatusBarDraw 0x00507a10 | **differs** (F3, minor): the factor clamp is in the exe (the decompile hides it), but the exe takes `(x1 - x0) * factor` on the x87 stack (0x00507a8e) and the port rounded the product to float first (1 in 392,000 random factors truncates one pixel apart). Fixed (double product) |
| 5 | cat_selStatusBarsDraw 0x00507bc0 | as the exe: row order (health, ACV EMP with upgrade 0x200, resources, Beast weapon, Dreadnought cannon, big gun, cloak flag 0x200, gravwell, Sentinel with its clamp, upgrade progress +0x598..+0x5a0), the disabled icon `cat_overlayIconDraw(0x34, y0, x0 - 3h - 1, 3h, 3h, -1)` (disassembly 0x00507ec2..0x00507ede); all 16 bar colours and the geometry constants match the exe data |
| 6 | cat_selStatusIconsDraw 0x00507630 | as the exe (icon 0x1f, size = bar height * 2 (int 0x0072b0b4), y0 - size, one star per +0x5f8 level) |
| 7 | clCommandMessage 0x004ac820 (cat_ioCommandMessage) | as the exe: scroll when count > 24, text, +0x214 = 0, colour +0x220, the shown-line rule (at most 5), last = count, count + 1, sound 0x12a unless strSavedGame; type/data of the slot left as they are (as the exe) |
| 8 | cat_ioMessageAdd 0x004ac940 | as the exe (type / data stored after the unlock into slot count - 1) |
| 9 | cat_ioMessageScroll 0x004ac6b0 | as the exe (shift text, type, data, colour; +0x214 reset to 0; count - 1) |
| 10 | cat_ioRemoveShipReferences 0x004ab800 | as the exe (type 1 and ship -> 0, NULL) |
| 11 | piePointModeOnOff 0x004e5c90 | as the exe (off for any mode while on: cursor, 8 fields incl. 0x00b39cbc and MP_HyperSpaceFlag; on: modes 1-4 with the exe's callbacks and colours 0x0088d79c..a4 (white); mode 4's cursor 0x004e6e70 is an honest CAT-TODO) |
| 12 | pieMovePointClipToLimits 0x004e76b0 | as the exe (box +-(size - 8000.0), clamp when A is outside, x/y exit fractions, the `tx != 1 || ty != 1` choice, z clamp, fallback B = A) |
| 13 | cat_pieCompassDraw 0x004e7240 | **differs** (F1): the exe skips the colour 0xff000000 (`cmp ebx,0xff000000` at 0x004e724b; cat_smZoomFadeColor's fully faded colour), the port skipped 0xff7fffff and would draw a black compass. Fixed. The rest is as the exe (0x30 segments, 15-degree ticks at 0.9..1 r with the 1e-5 axis test, four spokes from 0x0088d7ac/bc, cross ticks every 700 to r - 350, 300 each side, depth test only in the sensors manager) |
| 14 | cat_smHeightCircleColorGet 0x004e7690 | as the exe (0xffb8b838 through cat_smZoomFadeColor(c, 0)) |
| 15 | pieDistanceReadoutDraw 0x004e61b0 (moving-camera case) | as the exe: both screen coordinates from the projected x (as written, commented) |
| 16 | selRectDragFunction 0x005053a0 | as the exe in every test (untargetable bypass, 0x102000 / 0x8000 / 0x800, KAS derelict except 0x6f, own / mimic with cat_shipSetDisableReason(1, 7), mines with the mission-3 sensors rule and FOW, the sensors-manager FOW test, the cloak / mimic / attack rules, asteroids only with selectAnything, derelicts never), the centre-point ninth argument, the precise-poly area test against 0.2 (0x00892a30), the component -> clamp host, the duplicate check, 1000 cap. Observation O1 below (pixel offset) |
| 17 | cat_shipPrintHealthColor 0x00511510 + table 0x0072b190 | as the exe (6 rows 70/50/35/25/12/0 with the exe's colours; fallback row 5) |
| 18 | cat_shipNamesAssign 0x0050f320 | as the exe (first free record of the type, else the record after the last; an earlier record freed) |
| 19 | cat_shipNamesRelease 0x0050f660 / owner test | as the exe (player index +0x24 == 0, or sigsPlayerIndex in multiplayer; types 0xc, 0x12, 0x2f, 0x35, 0x89) |
| 20 | cat_shipNamesSetCB 0x0050ee70 / CatShipName 0x70 | as the exe (type +0x08, name +0x0c, the 99 cap message) |
| 21 | cat_cameraSetAngle 0x0042b940 | as the exe (float 2PI 0x40c90fdb; TWOPI is the same float) |
| 22 | cat_missionTimerReset / Save / Load 0x004acb80..0x004acbc0 | as the exe (raw dword, no conversion, disassembly); SaveGame / LoadGame call them right after the misc chunks, before kasSave / kasLoad, as the exe |
| 23 | cat_cmCountJobsForPlayer 0x00459ce0 | as the exe (145-entry line map, mothership slot 0, carriers 1.., totalTime / 0x00894198 (1) in single player, nJobs reset but not timeLeft, the unbounded slot index) |
| 24 | cat_gameStatsStartup 0x00489e50 / cat_gameStatsIsActive 0x0048a050 | as the exe (default.hff x2, Arial_b17.hff, 2.0 s real-time timer) |
| 25 | cat_ccIsPilotingShip 0x0042c2e0 / ThisShip 0x0042c310 | as the exe (one-ship focus, CCMODE_PILOT_SHIP, a ship) |
| 26 | subTitlesDraw 0x00584620 head / cat_ioSubtitleNotify 0x004ab620 | as the exe (`tutorial != 1` -> notify(0); the notify body) |
| 27 | tbBumperProcess 0x0052b100 | as the exe (no tutGameMessage calls) |

Data: the overlay icon table (53 entries at 0x0088c4a8, names read through their pointers) is in the enum's order (31
Exp_Star, 52 X); the script table 0x0088cb48 points into it. The clCommandMessage callers: every port site matches its
exe caller's colour and string (string-table addresses mapped through `StringsOnly.h`: 0x00bb4de0
strNoMoreResourcesVisible, 0x00bb4e18 / 0x00bb4e1c the crystal / infection tech, 0x00bb4e28 strResearchComplete,
0x00bb4f08, 0x00bb5008, 0x00bb501c, 0x00bb5038, 0x00bb5040, 0x00bb5044, 0x00bb507c, 0x00bb3f34 / 0x00bb410c in
recPackInGameStartCBSafeToStart (red / white), LeechHouseKeep's 0xff2e3def).

## 3. The diff (`git diff a47a8a6`)

- Files: the HUD scope plus the clCommandMessage / cat_ioMessageAdd call sites, the piePointModeOnOff callers, the
  hooks the report lists (utility.c, main.c, mainrgn.c, Universe.c, UnivUpdate.c, SinglePlayer.c, SaveGame.c,
  KASFunc.c), `CameraCommand.c` (the piloting test moved from SoundEvent.c's stand-in) and `tools/m2_harness.sh` mode
  `hud`. No change outside the producer's scope.
- A hunk-to-function map (each changed line to the C function around it in the old and new file, then the delta map by
  name) found 219 edited functions, 91 of them in the map (11 IDENTICAL, 71 MODIFIED, 9 REWRITTEN). The 11 IDENTICAL
  ones: 10 carry a `[review: M2-R5 hud]` note; **clWrapDeterministicBuild** 0x00452eb0 was edited (the debug-only
  enableTextFeedback line's colour) without one (finding F2; it had a command-tactics review). Note appended, merged
  with `--done m2r5:hud-audit`, `docs/m2-r1-function-deltas.md` regenerated (counts unchanged). The ConsMgr.h hit is a
  false positive (a new struct after a prototype).
- Review counts: 126 entries tagged by the port (83 new with sample `port-R5-hud`, 43 updated), classes 73 MODIFIED /
  44 IDENTICAL / 9 REWRITTEN, the five reclassified IDENTICALs (piePointModePause, pieMousePosSet3D,
  pieSameOnScreenPoint, pieMovementCursorDraw, horseRaceInit) and the totals IDENTICAL 3,014 -> 3,009, MODIFIED
  1,329 -> 1,334, reviewed 2,079 -> 2,162: all as the report says (review file at a47a8a6 vs the port). No other entry
  changed.
- `CAT:` comments: hunks without a CAT/HWI word within their context are includes, the mechanical second argument of
  clCommandMessage in HW1 call sites, piePointModeOnOff call-site arguments, and lines inside rewritten bodies whose
  header carries the CAT comment (ShipView.c, Tactical.c toAllShipsDraw, selSelectionClick, pieSameOnScreenPoint).
  No Cataclysm change without a comment found.
- CAT-TODOs: no `CAT-TODO(M2-R5 hud)` is left. Two stale stage-3 TODOs the port made true without closing them
  (finding F4): ResearchAPI.c's "the exe's clCommandMessage takes a colour (0xffff0000)" (the call passes it) and the
  colour half of Task.c's cat_taskTimeCompressionPrint TODO. Closed / reworded; the %s half stays.
- The 25 r6 functions of `^cat_(shipPrint|selStatus|selHealth)` (functions.csv) all have a definition in src/Game.

## 4. DONE WHEN on the plain harness

| claim | run | result |
|---|---|---|
| `/catHudTest` 600 frames, ASan 0 | `r5h-aud-hud` (clean audit build), `r5h-aud2-hud` (after the fixes) | `result=PASS` both: band-boxed 19 at tick 64, 154 selected / selecting, pie mode 2; frames 535, status bars 10,165, bars 10,165, icons 332, info panels 535 (9,095 lines), selected / selecting lines 8,025, message lines 202, support units 535, pie 535, compass 535 (the report's numbers); ASan 0, no unsuppressed leak |
| 25 UI cat-new ported | functions.csv vs src | all 25 defined (section 3) |
| fe unchanged | `r5h-aud-fe` (120 s) | Main_game_screen, ASan 0, UBSan kinds only `fescreen` 92 (as base `r5s-aud2-fe`), 284 file loads vs 233: + 48 OverlayIcons .lif, OverlayIcons.script, MainOverlay.script, shipnames.script, nothing else |
| sp unchanged | `r5h-aud-sp` (`--timeout 340`) | universe running, univUpdateCounter 1,248, ASan 0, no leak, first non-benign UBSan BTG.c:388 and all kinds / counts as base `r5s-aud2-sp`; 3,767 loads vs 3,726: the same 51 plus, minus 5 + 5 `feman/loadscreen/ring.lif` / `arrows.lif` (no such string in the exe's strings.csv) |
| mission stream unchanged | `mission_test.sh --ticks 6000`: `r5h-aud-m-*` and `r5h-aud2-m-*` (after the fixes) | MISSION TEST PASS both: A = B byte-identical (1,476,370 B, sha256 af00ca976dd69b48), ASan 0, perturbed run at 1500 (rng10), flipped copy at 2000 (ships_health); vs base `r5s-aud2-m-a`: IDENTICAL, 6,001 ticks (only the build identity differs) |

## 5. Findings

| id | finding | evidence | fix |
|---|---|---|---|
| F1 (exe body) | cat_pieCompassDraw's "no compass" colour is 0xff000000 in the exe, 0xff7fffff in the port | `cmp ebx,0xff000000` / `je` at 0x004e724b; cat_smZoomFadeColor returns 0xff000000 at zoom 0 | test and comment fixed, annotated in the report |
| F2 (delta map) | clWrapDeterministicBuild (IDENTICAL) edited without the port's review note | hunk map; review file 0x00452eb0 | note appended, merged, report regenerated; report annotated (127 tagged entries) |
| F3 (exe body, minor) | the bar split rounded `(x1 - x0) * factor` to float; the exe keeps it on the x87 stack | 0x00507a68..0x00507a96 | double product |
| F4 (convention) | two stale stage-3 CAT-TODOs about clCommandMessage's colour left open | ResearchAPI.c:819, Task.c:557 | closed / reworded |

Observations (not counted):
- O1: the exe's band box (`selRectDragFunction`) and icon quads convert screen to GL with HW1's pixel-centre offset
  `(x + 0.325) / W * 2 - 1` (0x0072a858 = 0.325f; `src/Win32/prim2d.h` has it), GoK's `src/SDL/prim2d.h` dropped
  it tree-wide. The pick rectangle is therefore 0.325 px off the exe's. A tree-wide renderer decision, not this port's;
  for the integrator / M2-R6 main region.
- O2: the report says off-only piePointModeOnOff callers pass mode 0; mainrgn.c's off-only sites (lines 1835, 2231,
  2349, 3866) pass PIE_SPEC_Move. Same behaviour (any mode turns an active plate off); their exe bodies are not in the
  export as named functions, so the exe's argument there is unknown.
- O3: catIoMessageInsert copies at most 0x1ff bytes where the exe's strcpy is unbounded (0x214 bytes before +0x214); a
  safety deviation without a CAT-FIX marker; no caller passes a longer text.
- O4: cat_overlayIconDraw's decompile is broken (wrong calling convention), so its body was not compared; the icon ids
  and argument order at its callers were (rows 5, 6).
- O5: the game-stats screen stays deferred as the report says (no `cat_utyGameQuitToMainOrStats` caller in the port).

## 6. Precision

46 sampled checks: builds 1, bodies 27 (2 wrong: F1, F3), data tables 5 (icon table, bar geometry/colours, health
colour table, pie/compass constants, caller colours/strings), runs 6 (hud, fe, sp, file-load deltas, mission A/B with
perturb/flip, mission vs base), diff and delta-map checks 7 (scope, edited IDENTICALs F2, review counts, totals and the
five reclassifications, 25 cat-new defined, CAT comments, CAT-TODO closure F4). Wrong: 4. **Precision = 42 / 46 = 0.91.**

Fixes in the tree: 6 (compass colour test; bar split precision; clWrapDeterministicBuild review note + merge +
regenerated report; ResearchAPI.c and Task.c stale TODOs; two report annotations). Builds green after every fix (clean
audit debug and release, the worktree's build/debug and build/release, 0 warnings); hud self-test and the mission test
re-run after the fixes: PASS, stream unchanged.

Verdict: PASS-WITH-FIXES - 42/46 checks hold (27 status-bar/picking/message-scroller/pie/info-panel/ship-name/timer bodies and 5 data tables vs r6, hud self-test PASS ASan 0 with the report's counts, fe/sp as base plus the 51 new start-up loads minus the 10 ring/arrows loads, two 6,000-tick Mission01 streams identical and IDENTICAL to base); fixed the compass none-colour (0xff000000), the bar split precision, one unreviewed edited IDENTICAL, two stale CAT-TODOs
