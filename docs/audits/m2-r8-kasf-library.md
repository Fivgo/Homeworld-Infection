# Audit: M2-R8 kasf-library

Auditor: adversarial audit of the M2-R8 producer 3 port (report `docs/m2-r8-kasf-library.md`), base `15df4d6`. The port
is `git diff 15df4d6`: the WIP snapshot `e3f05f4` (cut by the five-hour limit, port work) plus the working tree
(Console.c, KASFunc.c, UIControls.c and the untracked report). Every engine run below went through
`tools/m2_harness.sh` (plain harness, silent, offscreen; no GOG game, no DLL, no input), with the auditor's own clean
build passed with `--bin`.

## 1. Clean builds

`rm -rf original/scratch-m2/build-audit-r8-kasf-library`, then `meson setup` with `tools/build.sh`'s arguments
(debug: `--buildtype=debug -Db_sanitize=address,undefined`; release: `-Doptimization=2 -Ddebug=false -Db_sanitize=none`)
and `meson compile` (werror true, warning_level 3):

| config | result | warnings | size | sha256 (after the audit's fixes) |
|---|---|---|---|---|
| debug | `[249/249] Linking target hwi-engine`, rc 0 | 0 | 80,331,784 B (80,331,792 B after the fixes) | `d550c955fffa987d` |
| release | `[249/249] Linking target hwi-engine`, rc 0 | 0 | 4,405,040 B | `68dd6fb754bac42e` |

249 targets (R8 producer 1's audit: 319): src/Missions (HW1's kas2c output) is out of `src/meson.build`, as the report
says. After the audit's fixes both audit configurations and `tools/build.sh --debug` / `--release` in the worktree
were rebuilt: rc 0, 0 compiler warnings (the debug log's one "WARNING" is meson's sanitizer notice).
`tools/m2_layout32.sh`: "m2_layout32: OK (all ILP32 layout asserts hold)" (incl. the new AITeam 0x280 pins and the KAS
label entry sizes).

## 2. Runs (auditor's clean build, plain harness)

| run | mode | result |
|---|---|---|
| `r8kfa-mission6k-{a,b}` (`tools/oracle/mission_test.sh --bin <audit> --label r8kfa-mission6k --ticks 6000 --no-perturb-run`) | mission, 6,000 ticks | MISSION TEST PASS: a = b byte-identical, 1,476,370 B, sha256 `b97180de748702e5`; ASan 0 both; flipped copy reported at tick 2000 / ships_health |
| `r8kfa-final6k-{a,b}` (same, after the audit's fixes) | mission, 6,000 ticks | MISSION TEST PASS: a = b, sha256 **`b97180de748702e5`**, ASan 0 |
| `r8kfa-ai`, `r8kfa-final-ai` | ai, 400 | PASS `checks=16/16`, ASan 0; run.hwo IDENTICAL to producer 1's audit `r8au-ai-a` (401 ticks) |
| `r8kfa-skirmish`, `r8kfa-final-skirmish` | skirmish LiveFireExercise2, 600 | PASS `done ticks=600 ships=89`, ASan 0; IDENTICAL to `r8au-skirmish-lfe2` (601 ticks) |
| `r8kfa-hud` | hud | PASS, ASan 0 |
| `r8kfa-ui` | ui | PASS, ASan 0 |
| `r8kfa-sound` | sound | PASS, ASan 0 |
| `r8kfa-console`, `r8kfa-final-console` | console | PASS (`kas lines=6 ok=6 tablerows=486 last='KAS Returned 0'`, failures 0), ASan 0 |
| `r8kfa-fe` | fe, 120 s | Main_game_screen, still running at the timeout; ASan 0, no leak, 0 fescreen UBSan reports |
| `r8kfa-sp` | sp, `--timeout 340` | universe running, last univUpdateCounter 3,264, still running at the timeout; ASan 0, no leak, 0 fescreen UBSan reports |
| `r8kfa-r17` | replay 10-17 (`--recording-src .../oracle01-rec-20261017-140428 --recording-as oracle01-r6`) | done `ticks=9601 packets=4800`, ASan 0; IDENTICAL to producer 1's audit `r8au-r17-off-a` (9,601 ticks, 4,800 packets) |
| `r8kfa-r18b` | replay 10-18 (`--recording-src .../oracle01-rec-20261018-102253 --recording-as oracle01-r18`) | done `ticks=3527 packets=1763`, ASan 0; IDENTICAL to `r8au-r18-off-a` (3,527 ticks, 1,763 packets) |

**The Mission01 stream**: `python3 -I tools/oracle/hwo_compare.py <runs>/r8au-mission-a/run.hwo
<runs>/r8kfa-final6k-a/run.hwo --from-tick 1 --to-tick 6000 --fields
rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real
--json <runs>/r8kfa-final6k-a/compare-audit.json` -> "HEADER: same producer, different build identity: d190685e/1fbfa32f
vs e3f05f47/1fbfa32f", "IDENTICAL: 6000 ticks and 0 packets compared (ticks 1-6000), no difference"; with all fields
(run b): IDENTICAL over ticks 0-6000. The stream is producer 1's audited `245b3d88387ed076` in content (only the
header's build identity differs); **no field moved**, as the producer states. The next port's reference: sha256
`b97180de748702e5` for a build of `e3f05f4`'s identity (the checkpoint's commit changes the header, not the content).

Harness note (not a finding of this port): the install's `original/game/SavedGames/RecordedGames/oracle01` was
replaced at 14:34 today by a new recording (oracle session 10-19, 133 packets: `r8kfa-r18`, run first from the
install's copy, played 267 ticks). The 10-18 recording now replays only from its archive
`original/oracle/oracle01-rec-20261018-102253` through `--recording-src` / `--recording-as`, as `r8kfa-r18b` does.

## 3. Generated files

`python3 -I tools/m2_kasf_table_gen.py --dispatch --check` -> "src/Game/KASFuncTable.c current",
"src/Game/KASFuncDispatch.c current" (before and after the fixes). `python3 -I tools/delta_map.py merge --out <copy>`
on the producer's map: "records changed 0", the copy byte-identical to `ghidra/delta_map.json`;
`python3 -I tools/delta_map.py report` regenerated `docs/m2-r1-function-deltas.md` byte-identical to the producer's.

**20 random rows of the 486-entry table** (`random.seed(8)`: rows 15, 22, 43, 64, 70, 98, 107, 116, 126, 189, 192, 199,
205, 232, 235, 249, 259, 328, 360, 415) read by the audit's own parser from cataclysm.exe's `.data` (0x00755048 + row *
0x990: name, returnsNumber +0x84, realName +0x88, numParams +0x10C, the typed parameters from +0x110) and hashed with an
independent implementation of `cat_kasHashString`; the address taken from the Ghidra export's decompile of
`cat_kasCallFunctionByName` 0x0045b9a0 (hash constant -> callee) and the callee's address in functions.csv: **20 / 20
equal** in name, hash, returnsNumber, C name, parameter names and types and exe address (row 43 `Print` -> 0x00491eb0
`kasfCommandMessage` through the export's comment, the decompile folds that branch). The same check over all 486 rows:
0 mismatches (11 branches the decompile shows in another shape resolved by name, all equal); the sentinel `"*"`.

## 4. Bodies compared with the exe (decompile, disassembly where it mattered)

Cataclysm-only kasf (20): `kasfShipsSetFormation` 0x00492740 (formation 8 = the parade), `kasfShipsSetInvincible`
0x004984c0 (catFlags18 bit 2), `kasfShipsParalyze` 0x00493420 (Ship+0x1CC bit 14, disassembly),
`kasfTimesHitByEMP` 0x00493460 (+0x52C), `kasfFindMinesNearTeam` 0x00493060 (argument order and `<=` from the
disassembly), `kasfGetVectorX` 0x0049ace0 (0.5f = 0x3f000000, floor, ftol), `kasfCameraSetDistance` 0x00496260,
`kasfSubtitleReset` 0x004976f0 (0x00a75f84 = `subMessageEnded`, confirmed through `kasfIntelEventEnded` 0x00497b00),
`kasfSetMCVItemIndex` 0x00497f70 with `cat_spSetMCVItemIndex` 0x005ba150 (0x00a762e8), `kasfGetPlayerMothership`
0x00498790, `kasfShipsSetInvincibleInWidescreen` 0x0049abd0 (0x01000000 set when the argument is 0),
`kasfStartLevelByHyperspacingShips` 0x00498230, `kasfShipsSelectIdle` 0x00495140 with
`cat_kasShipsSelectByPredicate` 0x00494f90 (the NULL CAT-FIX marked) and the predicates 0x004950f0 / 0x00495110
(command +0x370, order word +0x14), `kasfMimic` 0x00497fd0, `kasfFirstShip` 0x00498d90, `kasfHasShipBeenDiscovered`
0x00499570 (SOF_Hide 0x8000), `cat_kasWideScreenInFinish` 0x004973d0, `kasfShipsSort` 0x0049a100 (**finding F1**),
`kasfWideScreenInDontCloseSensors` 0x00497580 and `kasfWideScreenOut` 0x00497630 (**finding F2**).

HW1-matched KASFunc.c bodies (9): `kasfTimerStop` 0x00491fe0 with `kasfScopeHash` (KAS_SCOPE_MISSION = 0, the
`-(scope != 0) & name` mask), `kasfTeamHealthAverage` 0x00492b20 (sums on the x87 stack, *100 (0x42c80000), / , +
1.5 * 2^52 (0x4338000000000000): `lrint` of the double quotient), `kasfRenderedShips` 0x00497020 (the LOD mask loop),
`kasfMissionCompleted` 0x00491c70 (animBinkPlay 0x00421860 = this tree's `animAviPlay`), `kasfTeamGiveToAI`
0x00493e10, `kasfSpawnEffect` 0x00497cb0, `kasfShipsSetDamageFactor` 0x00493980 (+0x3E0, 0.01f = 0x3c23d70a, special
flag 0x4000), `kasfShipsClearDamageFactor` 0x004939d0, `kasfShipsSetRepairFactor` 0x00493a10 (+0x3DC, 0x2000): all at
the exe's. Gun.c's two reads of +0x3E0 (`missileShoot` 0x0048af06 `fld [ebx+0x3e0]`, `gunShoot` 0x0048b72b `fmul
[esi+0x3e0]`): as ported.

Objectives.c (3): `objectiveDestroy` 0x004d63a0 (fleet intelligence inlined in the exe), `objectiveDestroyAll`
0x004d6480, `cat_objectiveCountActive` 0x004d6370 (+0x2c): at the exe's.

ICF-folded stubs (2): 0x0049ae30 is a lone `ret` (`kasfSetPlayerResourceCap`, `kasfAssert`, `kasfLog`, ...: empty
bodies, the dispatcher's cases name 0x0049ae30), 0x006fe9a0 is `xor eax,eax; ret` (`kasfTeamSkillGet`,
`kasfGateShipsOutNearest` return 0): as ported.

UI sound ids (24 sites): the five `pingListDraw` pushes 0x12c / 0x133 / 0x134 / 0x131 / 0x132, mapped to the PTOM types
through the exe's byte table 0x004e8e78 and jump table 0x004e8e60 (Proximity 0x131, NewShips 0x132, Anomaly 0x12c,
Battle 0x133, Hyperspace 0x134: as the C cases); the pushes before the calls at 0x00497481 (0x123), 0x004d02d9 (0x124),
0x00460dc2 (0x128), 0x00486dcc (0x12b, pushed at 0x00486dbe), 0x0052e558 (0x12f), 0x0052d7b7 (0x130), 0x0052e566
(0x103), 0x0049766a (0x124), the eight UIControls.c sites (0x108) and the three mainrgn.c sites 0x005649fd /
0x005654d4 / 0x00565554 (0x108): all equal. `grep -raEn` over src/Game, src/SDL, src/Ships: 37 live
`soundEvent(Play)(NULL, (CAT_)UI_*)` sites, 0 without a `CAT:` comment. The task bar: `0xa6b254` appears once in the
exe's disassembly, a read in `tbBumperProcess` (0x0052b100), so no write; `tbForceTaskbar` 0x0052b3b0 is called at
0x00532d48 (Tutor.c's kept call). Console.c: `cat_consoleKasCommand` 0x0045f270 matches the row by
`lib_crt_006929d0`, a case-insensitive `strnicmp` over `strlen(name)`: the report's "starts with the typed name, case
ignored" holds.

## 5. The diff (`git diff 15df4d6`, 59 files)

- **Scope**: every file is in the producer's scope (the kasf, the KAS label / variable callers, the AITeam tags, the
  task bar, the letterbox, the UI ids, the console) or a consequence of it (the flag rename `CAT_SHIPFLAG_Invincible`
  in Carrier.c / Mothership.c / Slipgate.c / GenericDefender.c / Salvage.c / UnivUpdate.c; `cat_spSetMCVItemIndex` in
  Mimic.c; the `#if CAT_HW1_MISSION_SCRIPTS` guards in SinglePlayer.c / Tutor.c forced by the new signatures). No
  commit after `15df4d6` other than the WIP snapshot.
- **Edited functions vs the delta map** (an audit script parsed every changed .c file at the base and now, compared each
  function's code with comments stripped): 89 MODIFIED and 27 REWRITTEN carry R8 reviews, 38 new functions are not in
  the map; **finding F3**: three edited bodies classed IDENTICAL had no review for this edit (`kasfMsgSend`,
  `kasfMsgReceived`, `tutInitTutorial`). Seven edited MODIFIED bodies have no R8 review (`kasSave`, `kasLoad`,
  `singlePlayerStartNis`, `spHyperspaceButtonPushed`, `spLockout`, `spMainScreen`, `spUnlockout`: the hash / task
  bar call sites; MODIFIED already, so not a finding; `kasSave`'s scope words checked: `SaveInfoNumber(0x00c3749c)`,
  `SaveInfoNumber(0x00c374a0)` as ported).
- **CAT comments**: the hash conversion is covered by the KAS.h group comment and per-line `CAT:` comments; every UI
  site carries one. **Finding F4**: `cat_spMainScreenAndLockoutKeepSensors` 0x00518e90 hands its lockout difference
  (the exe calls `spUnlockout` first when locked and hides the cursor for flag 2; this tree's `spLockout` does not) to
  producer 4 in prose only, no `CAT-TODO`, so the owner grep misses it.
- **CAT-TODOs**: 21 removed (the report's 21); `grep -rn "CAT-TODO(stage 6 KAS\|CAT-TODO(stage 6 kasf\|CAT-TODO(stage
  6, KAS" src/` -> 0. The two added `CAT-TODO(stage 6 tutorial)` are one line each, owner first. Note for the
  integrate: both name "M2-R8 producer 4" in their text, so they must be closed by producer 4 (the exit test forbids a
  `CAT-TODO` naming M2-R8).
- **Every kasf carries its address**: an audit script looked for each of the 485 C names' exe address within 60 lines
  above its definition: 7 have it only inside the body or on the folded sibling (`kasfBulgeAttack`,
  `kasfSetSwarmerTargets`, `kasfSwarmMoveTo`, `kasfFindShipsInside`, `kasfDock`, `kasfShipsDockSupportWith`,
  `kasfShipsDockStay`): all carry it. **Finding F5**: inserting `kasfFindMeteors` left HW1's comment "given a volume, find
  all ships inside it" above `kasfFindMeteors`.
- **Stream moves**: none, stated by the producer and confirmed (section 2).
- **Generated files**: reproduced (section 3).

## 6. DONE WHEN, re-checked

| item | result |
|---|---|
| every kasf Mission01 and Tutorial1 call exists at the exe's body with its address | met: all 486 link (the dispatcher calls each); the audit's block scan of the two missions (section 8's ranges plus the shared bodies) gives 106 / 112 kasf with tail jumps, 160 C names, every one defined with its address. **Finding F6**: the report's 101 / 112 / 159 come from a walk stopping at each body's first `ret` (the map's own section 8 counts by `call` are 103 / 109) |
| the 486-entry table generated and tracked | met (section 3) |
| `grep ... CAT-TODO(stage 6 KAS / kasf` = 0 | met: 0 |
| the task bar's callers gone, `tbStartup` / `tbShutdown` deleted | met: deleted from TaskBar.c / .h, no built caller; only the unbuilt src/SDL/utility_mac.c names them. **Finding F7**: the report also names src/Win32/utility.c, which has no task bar call |
| the 30 UI_* sites carry the exe's ids | met (section 4) |
| the Mission01 stream IDENTICAL to producer 1's audited stream | met (section 2) |
| ai, skirmish, hud, ui, sound PASS; fe and sp ASan 0 | met (section 2) |

## 7. Findings

- **F1 (code, wrong)** `kasfShipsSort` 0x0049a100: the C rounded all six differences to float and summed x*x + y*y +
  z*z; the exe (53-bit control word) keeps five differences on the x87 stack and stores only bz as a float
  (`fstp DWORD PTR [esp+0x1c]` at 0x0049a197), and sums (z*z + y*y) + x*x for both ships (0x0049a19b .. 0x0049a1bd).
  Near-equal distances could order differently. **Fixed**: double differences, bz as a float, the exe's sum order.
- **F2 (code, wrong)** `kasfWideScreenIn` / `cat_kasWideScreenInHyperspace` / `kasfWideScreenInDontCloseSensors` (the
  shared `kasfWideScreenInBody`) and `kasfWideScreenOut`: the frames were scaled by the literal 0.0333333f
  (0x3d088880); the exe's constant at 0x0072ab58 (read by all four, `fmul DWORD PTR ds:0x72ab58`) is 0x3d088889
  (1/30 as a float). **Fixed**: `(real64)(1.0f / 30.0f)`.
- **F3 (convention)** `kasfMsgSend` 0x00494a00, `kasfMsgReceived` 0x00492120 (string -> hash) and `tutInitTutorial`
  0x0052f690 (the start guarded by `CAT_HW1_MISSION_SCRIPTS`) were edited while classed IDENTICAL with no review for
  the edit. **Fixed**: three `[review: M2-R8 kasf-library audit]` entries (all stay IDENTICAL: the exe's bodies are
  HW1's), merged with `python3 -I tools/delta_map.py merge` ("records changed 3"; reviewed 3,237 -> 3,238, the
  classes unchanged), `docs/m2-r1-function-deltas.md` regenerated.
- **F4 (convention)** the lockout hand-off of `cat_spMainScreenAndLockoutKeepSensors` had no tag. **Fixed**: a one-line
  `CAT-TODO(stage 6 single player)` in its body (no round named).
- **F5 (comment)** HW1's `kasfFindShipsInside` comment sat above `kasfFindMeteors`. **Fixed**: moved back, with the
  address.
- **F6 (report)** the Mission01 / Tutorial1 kasf counts (101 / 112 / 159). **Fixed** `[audit: ...]` in the report.
- **F7 (report)** src/Win32/utility.c named as still calling `tbStartup` (twice). **Fixed** `[audit: ...]` in the report.

The report also got an `[audit: ...]` bullet for F1 / F2 and one for F3's three entries. Fixes: **7** (F1-F7).

## 8. Precision

Sampled checks: 20 table rows (20 correct); 20 Cataclysm-only bodies (17 correct; `kasfShipsSort`,
`kasfWideScreenInDontCloseSensors`, `kasfWideScreenOut` wrong); 9 HW1-matched bodies (9); 3 Objectives.c (3); 2 stub
groups (2); 24 UI push sites (24); 2 Gun.c offsets (2); 1 console match rule (1); 15 report claims and gates (the stream
unchanged, the five self-tests, fe, sp, the generator, layout32, the CAT-TODO grep, the task bar removal, the address
on every kasf, the CAT comment on every UI site, the delta map merged and its report reproduced: 15) and 5 more
statements of the report or conventions that were wrong (F3-F7). Correct 93, wrong 8: **precision 93 / 101 = 0.92**.

Mission-stream hash of the final pair (the next port's reference): **`b97180de748702e5`** (6,000 ticks, 1,476,370 B;
content IDENTICAL to producer 1's audited `245b3d88387ed076`, build identity `e3f05f47`).

Verdict: PASS-WITH-FIXES - the table, dispatcher, kasf bodies, UI ids and task bar removal are at the exe's and every gate holds with the stream unchanged; fixed kasfShipsSort's x87 shape, the letterbox 1/30 constant, 3 missing reviews, an untagged hand-off, a misplaced comment and 2 report statements
