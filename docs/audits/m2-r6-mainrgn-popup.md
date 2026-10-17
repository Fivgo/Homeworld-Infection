# M2-R6 audit: mainrgn-popup (producer 1)

Auditor: adversarial audit of `docs/m2-r6-mainrgn-popup.md` (the popup menu, the `cat_ui*` commands and main-region
bodies, `cat_mouseGetContextAction` and the cursors, the game-stats screen, `/catUiTest`). Port work = `git diff 314475c`
in the m2 worktree: the commits after 314475c (81d23e7 workflow script, 3e83e51 STATE, 32079c9 oracle recording,
ede0cdd feature target) are the orchestrator's and the user's, not port work; the port is the working tree (48 tracked
files plus the new `src/Game/PopupMenu.c/.h` and the report). Reference: the r6 decompile
(`ghidra/exports/cataclysm_gog_r6`, `tools/m2_fdec.py`), the exe disassembly (`tools/m2_fdis.py`,
`original/scratch-m2/m2r2-mesh-tools/exe.dis`) and exe data (`tools/m2_exedata.py`). Ghidra was not run. Every engine
run went through `tools/m2_harness.sh` (or `tools/oracle/mission_test.sh`, which calls it): silent, offscreen, no input.

## 1. Clean builds

`rm -rf original/scratch-m2/build-audit-r6-mainrgn-popup`, then `meson setup` with tools/build.sh's arguments (debug:
`--buildtype=debug -Db_sanitize=address,undefined`; release: `-Doptimization=2 -Ddebug=false -Db_sanitize=none`;
`werror=true` from meson.build) and `meson compile`: both green (309 steps each), 0 compiler warnings. Sizes: debug
`hwi-engine` 83,412,904 B, release 5,110,832 B. After the audit fixes `tools/build.sh --debug` and `--release` are green
again (`-Werror`), and `tools/m2_layout32.sh` says OK (CatPopupItem 60, CatTargetActions 164).

## 2. Bodies and data compared with the exe

| # | function / data | result |
|---|---|---|
| 1 | cat_mouseGetContextAction 0x0056f8f0 | as the exe: every CatTargetActions index (resource 6, enemy 4, ally 5, slipgates 8..10, salvageable 7, force field 26, MCV 23, the "all" words 32..39), playPackets 0x00a7518c, the Homeworld-CSA word 0x00a7526c (set by cat_opHomeworldCSA 0x004d9f30) turning EMP / swarm / ram / missiles / infect into attack, the own-ship dock/link tests (+0x210 catDisabled, SOF_Dead 0x800, shiptype +0x2b0), the mimic list test and the asteroid case |
| 2 | cat_mrReleaseRight 0x0056b0b0 | as the exe (disassembly 0x0056b0b0..0x0056b64f): the guards, the camera-turn end, the 12-pixel click limit, the point-mode popup, the mission-10 big gun (0x6e / 0x6f), every case of the 18-entry switch; the exe reads universe+0xA0 for the render list (never +0x9C: 34 reads), the tree's `RenderList.head` |
| 3 | cat_popupMenuOpen 0x004eb810 | as the exe (open guard, mouse, targets copy, special target cleared, build, first selectable row from 0, the special-row search, sub row -1) |
| 4 | cat_popupMenuFindSpecialTarget 0x004eaa60 | as the exe (rows 0x1a/0x1b/0x1d,0x28,0x2c,0x2e..0x30/0x2d -> kinds 1/0/2/3, selSelected.ShipPtr[0] = 0x00c0ab28) |
| 5 | cat_popupMenuExecuteSpecial 0x004ecfe0 | as the exe in all 25 cases (return values, the target filters: attackable 0/1, non-force-attack only for leech / swarm / infect / EMP, none for ram and missiles, salvage unfiltered) |
| 6 | cat_popupMenuFilterTargets 0x004ecf60 | **differs** (F1): inverted. The disassembly (0x004ecf80..0x004ecfac) keeps the ALLIED ships for argument 0 and the non-allied ones for nonzero; the port kept the non-allies for 0, so the repair row (its only caller, argument 0) dropped every ally it was opened on and kept enemies. Fixed |
| 7 | cat_popupMenuMouseInside 0x004ec720 | as the exe: the main rectangle 0x00a574b8 and the submenu rectangles 0x00a574b8 + 0x10*n for the rows 0x0e, 0x12, 0x0f, 0x1c, 0x10, 0x11, 0x13, 0x08, 0x21, 0x22, 0x15 (submenus 1..11) and 0x0c / 0x0d (12 / 13) |
| 8 | cat_popupMenuRemoveShip 0x004ef840, cat_popupMenuClose 0x004ec6f0 | as the exe |
| 9 | cat_popupMenuSpecialFireMissile 0x004ed510 | as the exe (attackable 0, no non-force filter) |
| 10 | cat_uiSpecialTargetCommand 0x00564060 | as the exe: the salvage reason order (reasons [1], [2] != [3], [5] != [3], [4]) and their four messages (MessageStrings 0x00bb4f00 / 4f04 / 4e1c / 4e18 = the string ids used), speech 0x200103a, the EMP linked-ACV message 0x00bb5018, the eight "no ship to ... found" messages 0x00bb4e44..0x00bb4e60 per ability, the wraps |
| 11 | cat_uiHaltSelection 0x00565190 | as the exe (tutorial bit 0x8000, halt + upgrade cancel) |
| 12 | cat_uiSetWaypointMode 0x00565100 | as the exe (mode 0/1/2 -> 0x00bb4de4 / 4dec / 4de8 = Normal / PatrolCircular / PatrolEndToEnd) |
| 13 | cat_uiFireDreadnoughtCannon 0x005663f0 | as the exe (upgrade 0x400, not busy, clWrapSpecial NULL) |
| 14 | cat_uiUnleech 0x00566510 | as the exe |
| 15 | cat_uiProtectFormation 0x00565f30 | as the exe: the wall (formation 9, string 0x00bb4dd8) for no protectees, Sentinels 0x11 / 0x34 skipped, the sphere (5) broken to delta, the force-field sphere (10) joined, the 12-ship cap (`0xb < n` break), speech 0x400088f / 0x4000810, string 0x00bb4dc0 |
| 16 | cat_selComputeTargetActions 0x00513a00 | as the exe: all 41 words (local frame mapped: local_28 = [31], local_24..local_8 = [32..39], local_4 = the leech action), the eight "all" words start 1, the mission-2 dMessagePod 0x7a exception, the salvage reasons tested ([0], [3], [6], [7], [8]), every upgrade bit and ship type (checked against the tree's enum values) |
| 17 | cat_gameStatsValueGet 0x00488970 | as the exe (column -> word map {0,1,2,3,4,13,9,5,14,10,6,15,11,7,16,12,8}; default 0.0 = float at 0x0072a460) |
| 18 | cat_gameStatsEnd 0x00489290 | as the exe |
| 19 | cat_gameStatsBegin 0x00489e90 | **differs** (F2): the exe plays track 0x28 and resets the radio words only when `_strnicmp(playerNames[cur], "TerryStats", 0x12) == 0` (0x0048a01b call 0x006929d0, `jne 0x48a048` skips it otherwise); the port played it for every OTHER name, with a case-sensitive `strncmp`. The rest (sensors close, pause, mrRenderMainScreen 0x008b9ecc, rows, sort with the 2 s bar timer, cursor, quit notify, SFX stop/unpause, mrReset) is as the exe. Fixed |
| 20 | mrBuildShips 0x005649b0 | as the exe (UI_Click = 0x108; SPECIAL_2 bit 8 at +0x1cc, +0x210, types 0x12 / 0x0c / 0x35 / 0x2f) |
| 21 | mrNextMothershipPtr 0x00565da0 | as the exe; the type table 0x008ba39c = 3e 4c 0c 2f 12 35 60 62 = kCarrier, tCarrier, sCarrier, bCarrier, sMothership, bMothership, p1Mothership, trFloatingCity |
| 22 | mrScuttle 0x00565210 | as the exe |
| 23 | mrRetire 0x005653b0 | as the exe (including the exe's `0 < numShips` chatter return) |
| 24 | mrReset 0x00569f80 | as the exe (0x00a75284 / 0x00a75288 cleared, point mode off); the extra `mrHoldRight = mrHoldLeft = mrNULL` keeps the tree's stand-ins in step (report "What is left") |
| 25 | mrSetTheFormation 0x005659d0 | as the exe (parade 8, custom 6 voiced as 7, ship voice 0x4000890 on ShipPtr[1], strings 0x00bb4d68 / 4d6c / 4d70 + type) |
| 26 | cat_utyMultiplayerQuitNotify 0x005aa0f0 | as the exe (Sleep 100 -> SDL_Delay 100) |
| 27 | cat_ccShipIsLegalToFocusOn 0x0042cd40 | as the exe except the FOW test, which the CAT-TODO(M2-R6 FOW) states |
| 28 | cat_spMissionFlagValue 0x0051c120 | as the exe; the 19-entry table equals 0x00894424 (0,1,1,0,0,1,0,1,0,0,0,0,0,0,0,1,0,1,0), strings 0x00bb4e04 / 4e08 |
| 29 | psPlugLinkSet 0x004ea240 (the one slash) | as the exe (`pcVar2[6] = '/'` only) |
| 30-32 | mrEnable, mrShutdown, mrTradeStuff (claimed IDENTICAL) | as the exe |
| T1 | context-action cursor table 0x0056f70c | all 17 cases read in the disassembly (`mov ds:0xa757f4, N`): 13, 20, 6, 21, 22, 27, 28, 29, 16, 30, 31, 32, 33, 34, 36, 35, 37 = resource .. beast_weapon, entry by entry as `catMouseContextActions` (the enum values counted from mouse.h) |
| T2 | right-click order table 0x0056b650 | all 18 cases: 0 popupMenuOpen(0,0), 1 mrHarvestResources 0x563cc0, 2/4/10/13/14/15/16/17 push 0/1/2/4/3/6/7/5 to 0x564060 behind the playPackets / universePause / mrDisabled test, 3 the attack chain to clWrapAttack 0x452c00, 5 0x565f30, 6-8 0x567750, 9 clWrapDock 0x452d60, 11 cat_clWrapLink 0x453940, 12 0x567860: as the report's table and the tree's switch |
| T3 | popup item tables 0x0088de88 .. 0x0088f330 | 91 rows of the 8 static tables (text 0, string id, command, submenu, group, visible, flags 0, id) equal the exe's 0x3C-byte rows |
| T4 | key command table 0x008b6310 | the 88 rows the port fills: both callback slots equal the exe's (the function address on each callback; the 12 sensors-view toggles 0x00550270..0x005503d0 each flip the variable the exe flips: 0x00892b4c .. 0x00a5b1a4) |
| T5 | game-stats layout 0x00881400..0x00881438 | the 15 words equal the `CAT_GS_*` constants |
| T6 | mouseStartup's cursor list | 31 names in the exe's order (slot 3 empty) |
| T7 | 0x00b39cbc's writer | 0x00564f2c lies in the unexported function 0x00564e20 (prologue at 0x564e20, ret at 0x564f39; calls piePointModeOnOff and smSensorsBegin), not in mrTradeStuff (0x00564a70, 21 bytes): the report's PiePlate correction is right |

## 3. Runs (plain harness)

| Run | Mode | Result |
|---|---|---|
| r6mp-aud-ui | ui (audit build) | `uitest: done selected=154 rows=10 popupframes=16 commands=26 targetrows=1 statsframes=8 result=PASS`; milestone "ui self-test done", ASan 0, no unsuppressed leak, 3,800 file loads |
| r6mp-aud-mission-a / -b / -perturb | `tools/oracle/mission_test.sh --ticks 6000 --perturb 3000 --copy-tick 4000` (audit build) | MISSION TEST PASS: A and B `cmp` equal, 1,476,370 B, sha256 `6ea73d317b4aabb0` (= the producer's), ASan 0; perturbed run reported at tick 3000 origin rng10; flipped copy at tick 4000 ships_health. `hwo_compare.py` R5 `r5-integrate-mission-a` (645f94c69d68319e) vs A: `IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000)`, header: build identity only |
| r6mp-aud-fe | fe 120 s | Main_game_screen, ASan 0; UBSan kinds and counts as `r6mp-base-fe`; 284 -> 299 file loads |
| r6mp-aud-sp | sp 340 s | universe running, ASan 0, first non-benign UBSan BTG.c:388 as base; UBSan kinds and counts as `r6mp-base-sp`; 3,767 -> 3,782 file loads |
| r6mp-aud-fix-ui | ui (tree after the fixes) | PASS, same counts, ASan 0 |
| r6mp-aud-fix-mission | mission 6000 (tree after the fixes) | `cmp` equal to r6mp-aud-mission-a (sha256 `6ea73d317b4aabb0`), IDENTICAL to R5 over ticks 0-6000, ASan 0 |

The sorted FileLoads.log difference against base (fe and sp alike) is exactly the report's: 13 new cursor bitmaps,
salvage.LiF in place of one special_activate.LiF, arial_12.hff and arial_15.hff.

## 4. The diff (git diff 314475c)

- Scope: every changed file is in the producer's list; no change outside it (the four commits after 314475c are not port work).
- Edited bodies: 54 functions changed between base and the tree (a body extractor over every changed .c file). None of them
  is IDENTICAL in `ghidra/delta_map.json`; the 35 mapped ones that the port changed carry a `[review: M2-R6 mainrgn-popup]`
  note; the others are the pre-existing MODIFIED `horseRaceRender` and `mrNISStopping` (call-site changes, CAT-commented)
  and functions the exe does not have. Delta map: 50 records changed, IDENTICAL 3,009 -> 2,999, MODIFIED 1,334 -> 1,344,
  reviewed 2,162 -> 2,199, the ten reclassifications as the report lists.
- CAT comments: every edited body has one except the HW1 CSM formation callbacks `mrDeltaFormation` .. `mrCustomFormation`,
  whose only change is the new `mrSetTheFormation(type, TRUE, FALSE)` arguments (call sites; the CAT note is at
  `mrSetTheFormation`). Not counted as a finding.
- CAT-TODOs: all owner first. One owner was split across two lines (F3): KeyBindings.c `cat_kbCommandKeyMatch` read
  "CAT-TODO(M2-R6 key" / "bindings): ...", so `grep "CAT-TODO(M2-R6 key bindings)"` missed it. Fixed.
- Harness header (F4): mode `ui` said the menu opens once "for an enemy target"; the test falls back to a resource when
  Mission01 has none, which every r6 run shows (`target=resource`). Fixed, marked `[audit: ...]`.
- Observation (not a finding): the self-test's `commands=26` is a fixed count (the counter starts at 2 for the two helper
  calls and each of the 24 calls adds 1 unconditionally), so `commands == 26` in PASS cannot fail; the run does reach and
  run every cat_ui* function.

## 5. Findings and fixes

| # | finding | evidence | fix |
|---|---|---|---|
| F1 | `cat_popupMenuFilterTargets` inverted (repair row kept enemies, dropped allies) | disassembly 0x004ecf60: `test ebx,ebx; je 0x4ecf9a` -> for 0 a nonzero `allianceIsShipAlly` keeps the ship (0x4ecfac `jne 0x4ecfc4`) | `src/Game/PopupMenu.c/.h`: parameter `keepEnemies`, allies kept for FALSE |
| F2 | `cat_gameStatsBegin` stats music inverted and case-sensitive | 0x0048a01b `call 0x6929d0` (`_strnicmp`, the lowercasing CRT loop), 0x0048a026 `jne 0x48a048` skips the track | `src/Game/GamePick.c`: `strncasecmp(...) == 0` (with `_strnicmp` under MSVC), description corrected |
| F3 | a CAT-TODO owner split over two lines | `src/Game/KeyBindings.c` cat_kbCommandKeyMatch header | owner on one line |
| F4 | harness `ui` header claimed an enemy target | r6mp-aud-ui `uitest: commands ... target=resource` | `tools/m2_harness.sh` header |

Neither F1 nor F2 is reached by the Mission01 stream (the audit-fixed stream is byte-identical to the unfixed one) or by
the self-test's PASS conditions (rerun PASS).

## 6. Precision

Sampled checks: 32 bodies (#1-#32), 7 tables / data claims (T1-T7), 8 run / report claims (ui PASS, mission A = B,
IDENTICAL to R5, sp unchanged, fe unchanged, the +15 file loads, builds 0 warnings and layout32, the delta-map counts),
4 process checks (scope, edited IDENTICAL without review, CAT-TODO format, the harness header). Wrong: F1, F2 (bodies),
F3, F4 (process). Precision = 47 / 51 = 0.92.

Fixes: 4 (2 code, 2 comment / doc), build green after them.

Verdict: PASS-WITH-FIXES - 47/51 checks hold (32 bodies and 7 tables vs r6 including both context-action jump tables entry by entry, ui self-test PASS ASan 0, two 6,000-tick Mission01 streams byte-identical and IDENTICAL to R5's 645f94c69d68319e, sp/fe as base plus the 15 new loads); fixed the inverted popup repair-target filter, the inverted TerryStats music test, one split CAT-TODO owner and the ui harness header
