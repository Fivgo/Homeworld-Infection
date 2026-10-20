# Audit: M2-R8 kas-runtime-mission01

Auditor: adversarial audit of the M2-R8 producer 2 port (report `docs/m2-r8-kas-runtime-mission01.md`), base `7d225de`.
The port is `git diff 7d225de` (no commit after the base: the whole port is the working tree, 33 tracked files changed
plus 8 new files). Every engine run below went through `tools/m2_harness.sh` (plain harness, silent, offscreen; no GOG
game, no DLL, no input), with the auditor's own clean build passed with `--bin`. No Ghidra run: the exe was read through
the export `cataclysm_gog_r6` with `tools/m2_fdec.py`, `tools/m2_fdis.py` and `tools/m2_exedata.py`.

## 1. Clean builds

`rm -rf original/scratch-m2/build-audit-r8-kas-runtime-mission01`, then `meson setup` with `tools/build.sh`'s arguments
(debug: `--buildtype=debug -Db_sanitize=address,undefined`; release: `-Doptimization=2 -Ddebug=false -Db_sanitize=none`;
werror true, warning_level 3) and `meson compile`:

| config | result | compiler warnings | size before / after the audit's fixes | sha256 after the fixes |
|---|---|---|---|---|
| debug | `[253/253] Linking target hwi-engine`, rc 0 | 0 | 81,451,824 B / 81,456,896 B | `5d84c1720d7d2a4b` |
| release | `[253/253] Linking target hwi-engine`, rc 0 | 0 | 4,509,080 B / 4,509,080 B | `037d0799135b3582` |

253 targets (the kasf audit: 249): the four generated files of src/Missions are built again. After the fixes both audit
configurations and the worktree's `tools/build.sh --debug` / `--release` were rebuilt: rc 0, 0 compiler warnings (the
debug log's one "WARNING" is meson's sanitizer notice). `tools/m2_layout32.sh`: "m2_layout32: OK (all ILP32 layout
asserts hold)" (incl. the new `ResourceVolume` 0xB0, `nebChunk` 0x58, `nebTendril` 0xFC pins).
`python3 -I tools/m2_save32_gen.py --check`: current; `python3 -I tools/m2_kasf_table_gen.py --dispatch --check`: both
files current.

## 2. Runs (auditor's clean build, plain harness)

| run | mode | result |
|---|---|---|
| `r8k2a-mt-a`, `r8k2a-mt-b` (before the fixes), `r8k2a-fix-mt-a`, `r8k2a-fix-mt-b` (after) | missiontest, `--mission-test-ticks 30000` | all four `done ticks=12880 objectives=6/6 result=PASS`, FAIL lines 0, ASan 0, no leak; run.hwo a = b byte-identical (`cmp`), sha256 `b03fdee2b85d36a0` in all four (= the producer's `r8k2-fin-mt-a`); flow: Mission01_NIS01 loaded, n01.nis started / stopped (tick 1771), Mission01 loaded, `kas start mission01`, nislet01 played, fleet arrived (tick 1939), DestroyBombers 4768/4800, ExploreRegion 7792/9088, DestroyResourcers 9200/10192, DestroyProxAttackers 11216/11696, DestroyDestroyer 12272/12752, Hyperspace 12784, jump begun 12848 |
| `r8k2a-tut`, `r8k2a-fix-tut` | missiontest `--tutorial` | `kas start tutorial1`, Lesson1 tick 2, L1Dummy1 / L1Dummy2, prompt "LESSON 01 - Start of Tutorial" tick 7, `result=PASS`; ASan 0 (one non-fatal UBSan shift report at Tutor.c:1958, not this port's) |
| `r8k2a-sk-lfe2`, `r8k2a-fix-lfe2` | skirmish LiveFireExercise2, 600 | PASS `ships=89`, ASan 0; sha256 `436f71fdc2287b45` (= the producer's `r8k2-fin-skirmish`) |
| `r8k2a-sk-armed8` | skirmish Armed8, 600 | PASS `ships=54`, ASan 0; sha256 `8299d4e002a2ceea` (= `r8k2-fin-armed8`) |
| `r8k2a-ai` | ai, 400 | done, ASan 0; sha256 `499a3fccf5031e52` (= `r8k2-fin-ai`) |
| `r8k2a-mission6k-{a,b}` (before), `r8k2a-final6k-{a,b}` (after; `tools/oracle/mission_test.sh --bin <audit> --label ... --ticks 6000 --no-perturb-run`) | mission, 6,000 ticks | MISSION TEST PASS both times: a = b byte-identical, 1,476,370 B, sha256 **`19560d16033efd53`**; flipped copy reported at tick 2000 / ships_health; ASan 0 |
| `r8k2a-fe` | fe, 120 s | Main_game_screen, running at the timeout; ASan 0, no leak, 0 fescreen UBSan reports |
| `r8k2a-sp` | sp, `--timeout 340` | universe running (debugScreenshots univUpdateCounter 2,480), running at the timeout; ASan 0, no leak; FileLoads shows the NIS-first start (`SinglePlayer/Mission01_NIS01/Mission01_NIS01.mission`) |

**The Mission01 stream** against producer 3's audited stream (`r8kfa-final6k-a`, sha256 `b97180de748702e5`):
`python3 -I tools/oracle/hwo_compare.py <runs>/r8kfa-final6k-a/run.hwo <runs>/r8k2a-final6k-a/run.hwo --from-tick 1
--to-tick 6000 --fields rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real
--json <runs>/r8k2a-final6k-a/compare-audit.json` -> "HEADER: same producer, different build identity: e3f05f47/1fbfa32f
vs 7d225de4/1fbfa32f", "IDENTICAL: 6000 ticks". With every field but rng0 / rng_all (incl. ships_rot, statcheck,
players, fpcw) over ticks 0-6000: IDENTICAL. With all fields: "FIRST DIVERGENT TICK 0 ... rng0 ... CAND is 40 draw(s)
ahead of REF" and rng_all only; at tick 6000 rng0 is still exactly 40 draws ahead. **The move is rng0 from tick 0 and
nothing else**, as the producer states. The post-fix pair is IDENTICAL in every field to the pre-fix pair (the fixes
leave the stream alone).

**The skirmish / ai streams** (fields from tick 0): LiveFireExercise2 vs `r8kfa-final-skirmish`: rng0 (40 ahead),
rng_all, ships_misc, ships_pos, ships_vel, ships_rot, ships_health, statcheck, univcheck, players; Armed8 vs
`r8k2-pre-armed8`: rng10 (602 ahead), rng0 (40 ahead), rng_all, ships_misc, ships_pos, ships_rot, ships_health,
resources, univcheck; ai vs `r8kfa-final-ai`: rng0 (40 ahead), rng_all only. These match the report's statements (its
"..." covers statcheck / univcheck / players).

## 3. Generated files

`python3 -I tools/m2_kas2c_gen.py --check`: "verified 149 bodies: callees and constants equal to the machine code's,
1790 all-constant calls' arguments equal", all four files "current". A second full run (`python3 -I
tools/m2_kas2c_gen.py`, writing the files) left all four byte-identical to the producer's copies (`cmp`); 10 random
Mission01 functions (`random.seed(19)`: `Init_Mission01_FSM_x5a79103_Attacking`, `Watch_Mission01_Pester_S_xc2ee6b2`,
`Init_Mission01_KushanProxDefend`, `Watch_Mission01_Pester_Launch`, `Init_Mission01_TaiidanBattle_Create`,
`Watch_Mission01_FSM_x5a79103_InHS`, `Watch_Mission01`, `Watch_Mission01_Pester_WaitCreate`,
`Init_Mission01_TaiidanProxAttack_AttackProx`, `Watch_Mission01_TaiidanDestroyer_Flee`) compared one by one: equal.
`OVERRIDES` is empty (no hand edit). The delta map: `python3 -I tools/delta_map.py merge --out <copy>` on the producer's
map: "records changed 0", byte-identical; `report` reproduced `docs/m2-r1-function-deltas.md` byte for byte;
`m2_stage_counts.py` stage 6: 301 IDENTICAL / 168 MODIFIED / 25 REWRITTEN, as reported. The report's review counts hold
(46 tagged entries = 36 new + 10 appended; IDENTICAL 2,788 -> 2,785, MODIFIED 1,549 -> 1,552, reviewed 3,238 -> 3,274;
KAS.c 30 and LevelLoad.c 33 rows all reviewed).

## 4. Bodies compared with the exe

- **KAS runtime**: `cat_kasHashString` 0x00491860 (`(c & 0xDF) + h*16`, high-nibble fold: = KASVar.c);
  `cat_kasCallFunctionByName` 0x0045b9a0 (the generated dispatcher, reproduced by its generator; the kasf audit checked
  all 486 rows); `kasSave` 0x00491220 (scope, scope hash, 0x30-byte name, `WatchFunctionToMissionEnum`, team index,
  0x008941a0, then the lists, `timTimerSave`, and in single player `rmCanBuildShip(player 0, t, 0)` for 0x15, 0x18, 0x16,
  0x13, 0x17, 0x85 = sMothershipArmour, -DockingBay, -Micro, -Special, -Weapons, dAcolyte by the enum); `kasLoad`
  0x00491450 (the same order back; `G_RecentlyLoaded` found or appended with scope 0, value 1; `rmEnableShip(0, t, n)`;
  the difficulty rescale left as tagged); `kasLabelledEntitiesDestroy` 0x00490f90 (`aivarShutdown` then 0x00491ae0 then
  `timTimerDestroyAll`); `cat_kasShipRemovedFromSelections` 0x004911e0 (= `kasShipDied` 0x004911a0) and its five call sites
  0x005491c3 / 0x005493ac / 0x0054cc49 (after 0x00549230, before 0x0054b030, as DeleteDeadResource), 0x00549083,
  0x00549a2a (before 0x00541df0): all as ported. `kasConvertFuncPtrToOffset` / `kasConvertOffsetToFuncPtr`: **finding F1**.
- **Timer.c** (3): `timTimerSave` 0x0052c860, `timTimerLoad` 0x0052c8a0 (0x18-byte records, `Timer` pinned at 0x18),
  `timTimerExpiredDestroy` 0x0052c290 (find, paused / remaining test with the 1.5 * 2^52 rounding, then the remove):
  as the tree.
- **gameStart** 0x005a9290: the 0..7 fill at 0x005a9630, the shuffle 0x005a9644 .. 0x005a96a7 (`ebp` = 20; two calls of
  0x004ef860 = `ranRandomFn(0)`, each `div` by `spScenarios[spCurrentSelected].maxplayers` (0x00a5790c + 0x14 * 0x00a57910
  + 0xC), swap only when different), on the non-tutorial, non-load, non-multiplayer path (0x005a95b1 jumps past it for
  the tutorial, 0x005a9578 for a load): as ported (the two CAT-FIX guards cannot trigger on a real scenario). The preload
  `GetMissionsDirAndFile(currentMission, currentMission == 1)` (0x005a98a6) and 0x00a6ac68 around 0x0051bdb0
  (0x005a9b57 .. 0x005a9b66): as ported.
- **Level load**: `scriptSetShipsToBeNeeded` 0x004a2f30 (race > 6 / type > 0x90 fatal, no race swap, colorScheme only
  parsed, the 16 capturable cases = `isCapturableCapitalShip`'s list by the enum, race 2 returns after the flag, race 3
  clears 0x00a447b0); `levelPreInit` 0x004a3230 (both words zeroed, then type 0x5b = tStandardFrigate flagged when
  0x00a447b0); `SetTeamColorForShipAndRelatedStaticInfo` (10, 0x55, 0x3b, 1, 0x22, 0x19, 0x5f and 0x54 by the enum);
  `scriptSetResourcesCB` 0x004a07b0 (scale up `fild` qword of the count `fmul` the modifier; scale down `(1 - m)`
  `fimul` the signed count; one `__ftol`); `AddResourceToCylinder` 0x004a03e0 (the half-extent corner turned and
  `fabs`'d into +0xA4 .. +0xAC); `AddResourceToVolumeAtPosition` 0x0049fcf0 (the TowardsVector velocity at +0x64 with
  `isMoving` +0x7C, Asteroid +0x1A0 cleared, the 0x200 drift from +0x98 / +0x9C / +0xA0, the wrap box against
  smUniverseSize, the second rotation chance); `cat_scriptSetMeteorStormCB` 0x004a0d10 (format string 0x008841d8, the
  table entry 0x00883aa8 after "Resources"): as ported, but **finding F2** (its fatal texts) and **finding F5** (the
  table's next two entries).
- **singlePlayerGameUpdate** 0x005197d0 (the start): `G_PlayNis` / `G_PlayNislet`, the 0.5/s fade (0x0072bcb0 0.5,
  0x0072a4e8 0.0), the nislet table 0x008945a0 (rows 4 / 28 / 31 = nislet01 / nislet03 / nislet02, the rest NULL), the
  `fileExists` fatal, Mission01's `cat_trPrecacheShipTextures`, the traders' PopUpGUI by scope "Traders", the
  `HYPERSPACE_LOADDONE` case (`kasMissionStart("missionNN")` 1 .. 0x11, letterbox words for Mission01, the at-once flag at
  realtime + 5.0 (0x0072bcb8)) and case 9 (the white flash 1.0 - 0.0001 at 0x0072a5f0, 3.0 double at 0x0072bcc0;
  `UpdateArrivingShip` per allied ship; `G_ShipsHaveArrived`; the ether list emptied; `cat_soundEventLetterboxVolume`,
  `cat_rmResearchModuleActivated`, `clAutoLaunch`, the launch, `aiplayerGameStart`, the NIS of a mission not entered from
  it): as ported. The four tables 0x008944bc / 0x00894508 / 0x00894470 / 0x00894550 read with `m2_exedata.py`: equal to
  the C arrays; the 18 Watch addresses of `catKasScripts` equal functions.csv and `MissionEnumToWatchFunction`
  0x0051b340's cases.
- **NIS-first start**: `singlePlayerStartGame` 0x0051bdb0 (n01 at the origin, `0x008944c0`, keepMission 1),
  `singlePlayerStartNis` 0x0051b670 (loadLevel -> `singlePlayerLoadNewLevel(1, keep)`, NULL -> "StartPointPlayer"),
  `singlePlayerNisStoppedCB` 0x0051b430 (mothership by class 0, recons of race 0 / 1 into formation 6, `G_Nis%dComplete`
  / `G_Nislet%dComplete`, the white flash, the letterbox lockout, the staging-level hand-off), `singlePlayerLoadNewLevel`
  0x0051ac80 (fade, 0x00894108, mission advance unless keep / 0x00a6ac74, Mission01's ether walk (the infinite restart
  correctly CAT-FIXed), the mid-level deletes, the fleet modifier by class with RUs and the clamps, the staging level's
  sprintfs, the effects / ping / subtitle resets, the arrival times, the trailing calls), `GetMissionsDirAndFile`,
  `tutInitTutorial` 0x0052f690: as ported.
- **Generated code vs the exe's bodies** (disassembly, callees resolved in functions.csv, strings read from the exe):
  `Init_Mission01_TaiidanCarrierDefense` 0x005d7f90 (FSM init), `Watch_Mission01_TaiidanBombers_AttackFlank` 0x005d7e80,
  `Init_Mission01_TaiidanDestroyer_PreFlee` 0x005d8180 and `Watch_Mission01_TaiidanDestroyer_PreFlee` 0x005d8220 (state
  functions), `Watch_Tutorial1_Tutorial_Lesson1` 0x0066d740 (L1Dummy1 0x008ce268 before L1Dummy2 0x008ce25c, the
  objective strings and LSTRING order) and `Watch_Tutorial1_SpeechEvents_WaitMessage2` 0x0066a080 (the message compare
  chain, the requeue `aitMsgSend(team, team, msg)`, the loop's `dec esi; jns`): call order, argument order and branch
  structure equal.

## 5. The diff (`git diff 7d225de`)

- **Scope**: every file is in the producer's scope (KAS runtime, level load, single player start, Tutor.c's start, the
  generated missions and their generator, the self-test and its harness mode) or a consequence of it (comment-only edits
  of the direct-start self-tests in AISelfTest.c / ConsMgr.c / Dock.c / InfoOverlay.c; NIS.c's `nisSeek` CAT-FIX the sp
  run needed; the save32 generator roots for the nebula records). No commit after `7d225de`.
- **Edited functions vs the delta map** (an audit script parsed every changed .c at the base and now, comments and
  whitespace stripped): 50 changed functions; edited IDENTICAL bodies without a review for this edit: **finding F3**
  `nisSeek` 0x004ce2e0 and **finding F4** `tutInitTutorial` 0x0052f690 (its review note still described the skipped
  start). The others are MODIFIED / REWRITTEN with R8 reviews, new, or not in the map (HWI test hooks, CAT-only bodies).
- **CAT comments**: every Cataclysm change sampled carries one (KAS.c, LevelLoad.c, Nebulae.c/.h, Universe.h,
  SinglePlayer.c/.h, Tutor.c, UnivUpdate.c, utility.c, KASFunc.c, HorseRace.c); HWI-TEST / HWI-ORACLE hooks are marked.
  `OracleDump.h` carried both new prototypes' comments on one line (**finding F7**, cosmetic).
- **CAT-TODOs**: `grep -rn CAT_HW1_MISSION_SCRIPTS src/` -> 0; `grep -rn "CAT-TODO(stage 6 KAS\|CAT-TODO(stage 6 level
  load\|CAT-TODO(stage 6)" src/` -> 0. Every added tag is one line, owner first. **Finding F6**: the report lists three
  new `CAT-TODO(stage 6 single player)`; there are four (the `mrRegionDraw` fade / flash drawing is the fourth).
- **Stream moves**: stated and confirmed (section 2).
- **Generated files**: reproduced (section 3).

## 6. DONE WHEN, re-checked

| item | result |
|---|---|
| Mission01 starts NIS-first; the mission self-test PASS, ASan 0, two runs byte-identical, the objectives in order | met (section 2: four runs, `b03fdee2b85d36a0`) |
| Tutorial1 starts and shows its first prompt | met |
| `CAT_HW1_MISSION_SCRIPTS` gone from src/ | met (grep 0) |
| the shuffle's 40 stream-0 draws per new game; the mission stream's move rng0 from tick 0, nothing else; the skirmish moves | met (section 2) |
| the mission mode's 6,000-tick pair byte-identical | met (`19560d16033efd53`) |
| fe and sp ASan 0 | met |

## 7. Findings

- **F1 (code, wrong)** `kasConvertFuncPtrToOffset` 0x00491800 / `kasConvertOffsetToFuncPtr` 0x00491830 looked a function
  up only in the current mission's generated list. The exe subtracts any pointer from
  `MissionEnumToWatchFunction(currentMission - 1)`. During the tutorial currentMission is 1 (`singlePlayerInit` writes 1
  at 0x00518cb1; this tree's `singlePlayerInit` takes `missionSequence[0]`), and Tutorial1's own script asks for a save
  (`kasfSaveLevel(0, ...)` at 0x0066a16d, carried out by univUpdate 0x0054ce0b -> `cat_kasSaveLevelNow` 0x004972d0,
  "Training" path), so every Tutorial1 FSM / state function was written as -1 and a loaded tutorial save would have no
  KAS functions. **Fixed**: both converters search every generated script's list (KasShared bodies have one exe address
  in every list) against the current mission's Watch exe address; `cat_spKasScriptFunctions` now gives the Watch address
  for any index 0 .. 17 (`CAT_SP_KAS_SCRIPTS` in SinglePlayer.h). Mission01's results are unchanged (its list is searched
  first; the mission test and stream are unchanged).
- **F2 (code, text)** `cat_scriptSetMeteorStormCB`'s two fatal messages were not the exe's
  ("No distribution specified in %s %s" 0x00884110, "Invalid meteorstorm layout (must be cylinder or sphere) in %s."
  0x00884198). **Fixed**.
- **F3 (convention)** `nisSeek` 0x004ce2e0 (IDENTICAL) edited by the CAT-FIX without a review entry. **Fixed**: a
  `[review: M2-R8 kas-runtime-mission01 audit]` entry (stays IDENTICAL).
- **F4 (convention)** `tutInitTutorial` 0x0052f690 (IDENTICAL) edited (the start is live) without a review entry; its note
  still said the start was skipped. **Fixed** the same way. Both merged with `python3 -I tools/delta_map.py merge`
  ("records changed 2", classes and the reviewed count unchanged), `docs/m2-r1-function-deltas.md` regenerated.
- **F5 (convention, untagged gap)** the exe's mission-sphere table 0x00883a48 has "Mine" (0x004a3f20) and "Slipgate"
  (`cat_scriptSetSlipgateCB` 0x004a1020) after "MeteorStorm"; the tree's `MissionSphereScriptTable` lacks both with no
  tag, so their lines are skipped silently (LiveFireExercise2's resource.missphere has them; Mission03 / 10 / 12 / 14
  too; Mission01 and Tutorial1 none). **Fixed**: a one-line `CAT-TODO(M2-R9 part B)` in the table.
- **F6 (report)** the count of new `CAT-TODO(stage 6 single player)` tags (three stated, four present). **Fixed**
  `[audit: ...]`.
- **F7 (comment)** OracleDump.h's doubled comment on `oracleMissionStopAfterThisTick`. **Fixed** (one comment per
  prototype).

The report also got `[audit: ...]` notes for F1, F2 / F5 and F3 / F4. Fixes: **7** (F1-F7).

## 8. Precision

Sampled checks: 34 exe bodies / tables / call-site groups (section 4; 32 correct, `kasConvert*` and the meteor storm's
texts wrong); 10 random generated functions vs a second generator run (1 check, correct); 20 report claims and gates (the
generator / save32 / kasf / layout32 checks, the two builds, the mission self-test pair, the tutorial test, both skirmish
maps and the ai stream with their moves, fe, sp, the 6,000-tick pair, the rng0-only move, the two retirement greps, the
review counts, the map merge and report reproduced, the stage counts: 20 correct) and 5 statements or conventions that
were wrong (F3-F7). Correct 53, wrong 7: **precision 53 / 60 = 0.88**.

Mission-stream hash of the final pair (the next port's reference): **`19560d16033efd53`** (6,000 ticks, 1,476,370 B, build
identity `7d225de4`; content = producer 3's audited `b97180de748702e5` with rng0 40 draws ahead from tick 0; the
checkpoint's commit changes only the header's build identity).

Verdict: PASS-WITH-FIXES - Mission01 starts NIS-first and its generated KAS script completes all six objectives (PASS, byte-identical, ASan 0), Tutorial1 shows its first prompt, the generator reproduces its output, the stream moves only by rng0's 40 draws; fixed the save converters for tutorial functions, the meteor storm's fatal texts, 2 missing reviews, an untagged Mine/Slipgate gap, a report count and a comment
