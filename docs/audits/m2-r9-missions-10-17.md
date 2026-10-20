# Audit: M2-R9 missions-10-17

Auditor: adversarial audit of the M2-R9 producer 3 port (report `docs/m2-r9-missions-10-17.md`), base `307c3a5`.
No commit follows the base: the port is the working tree (`git diff 307c3a5` plus the eight new `Mission10.c` ..
`Mission17.c` and the report). Every engine run below went through `tools/m2_harness.sh` (plain harness, silent,
offscreen; no GOG game, no DLL, no input), with the auditor's own clean build passed through `--bin`. Exe bodies were
read with `tools/m2_fdis.py` and `tools/m2_exedata.py`. No Ghidra was run.

## 1. Clean builds

`rm -rf original/scratch-m2/build-audit-r9-missions-10-17`, then `meson setup` with `tools/build.sh`'s arguments
(debug: `--buildtype=debug -Db_sanitize=address,undefined`; release: `-Doptimization=2 -Ddebug=false -Db_sanitize=none`;
`werror=true` is the project default) and `meson compile`:

| config | result | warnings | size | sha256 |
|---|---|---|---|---|
| debug | `[270/270]`, rc 0 | 0 | 88,430,472 B | `e65ba3131288da49` (every run below); `cea1104502b2bd53` after the audit's comment fix (finding 2: `__LINE__` shifts) |
| release | `[270/270]`, rc 0 | 0 | 5,604,616 B | `68a72fc6a7c714a9` (unchanged by the comment fix) |

Both rebuilt incrementally after the audit's edit: rc 0, 0 warnings. `tools/m2_layout32.sh`: OK (45 ship-specifics
files). `python3 -I tools/m2_kasf_table_gen.py --dispatch --check`: KASFuncTable.c and KASFuncDispatch.c current.
`python3 -I tools/m2_save32_gen.py --check`: current.

## 2. Generator

- `python3 -I tools/m2_kas2c_gen.py --check`: rc 0, all 21 outputs current, `verified 2085 bodies: callees and
  constants equal to the machine code's, 25279 all-constant calls' arguments equal`: the report's counts.
- Body counts: Mission10 .. 17 hold 90 / 160 / 73 / 124 / 171 / 31 / 158 / 370 = 1,177 `// cataclysm.exe` bodies;
  KasShared.c 46 (29 at the base); Mission01 .. 09 + Tutorial1 862; total 2,085 = `m2_stage_counts.py`'s `mission 2085`.
- OVERRIDES: 15 `HAND-EDIT` markers in src/Missions (producer 2's 4 + this port's 11: M11 2, M13 1, M14 3, M16 2,
  M17 2, KasShared 1); this port's 16 patches = 13 Sentinels asserts + M13's `== 3` + 0x0062dfe0's 2: as reported.
- Determinism: an independent run from scratch (tools/, src/, ghidra/ copied to `original/scratch-m2/audit-r9m3-gen2`,
  Mission10 .. 17, KasShared.c and CatMissions.h deleted there, the generator run in write mode with `--exe` /
  `--export`): all 21 outputs byte-identical to the tree (`cmp`), which covers the "10 random functions" check with
  every function.
- The new `s_x + N` hash rule: checked at 0x0066616f (`push 0x8c7f91` = `s_Hyperspace_008c7f90 + 1`; the generated
  `kasAITeamPtr(0x8c7f91)` in Mission17.c) and in Mission12's `0x8d9fd3` (send and compare).

## 3. Bodies compared with the exe (25 + extras)

| # | body | exe | result |
|---|---|---|---|
| 1 | Watch_Mission10_CaalShto_S_x4a93a83 (random) | 0x00611b10 | same (ShipsAdd dest/src, SelectNearby 4000, VarCreateSet, MsgSend) |
| 2 | Watch_Mission11_RammingFrigates (random) | 0x0061e110 | same (kasJump 0x65f6f0 / 0x61e1d0 = Init_Mission17_tCarriers_Build (ICF) / Watch_Mission11_RammingFrigates_Build) |
| 3 | Watch_Mission12_FSM_x5c43eb3 (random) | 0x00623040 | same (the `je` to the 0x4186922 branch = `== 0`) |
| 4 | Watch_Mission13_ThirdWaveAntiFighter_Attack (random) | 0x00627c30 | same |
| 5 | Init_Mission14_FSM_x62b637b_Create (random) | 0x0062e720 | same (the five team tests, random(1,2) cases, six ship-type strings read at 0x88b404 .. 0x88b534, ShipsCreateLaunch argument order) |
| 6 | Watch_Mission15_NewStations (random) | 0x00639a20 | same |
| 7 | Watch_Mission16_DockMimic_Docking (random) | 0x0063c390 | same |
| 8 | Watch_Mission17_tCorvettes_Dock (random) | 0x006626b0 | same (`jle 1` = `< 2`, `jl` RUs-vs-cost senses, imul build time, GetDockHost argument order) |
| 9 | Init_Mission14_Wave1_WaitCreate (override) | 0x0062dfe0 | same: one-argument kasfGetBuildTime("tMultiGunCorvette"), `lea*3; shl; cdq; sub; sar` = `(t * 6) / 2` |
| 10 | Init_Mission13_SupportWave2 (override) | 0x006295b0 | same: `sub 3; neg; sbb; inc` = `TeamCount == 3` pushed to 0x0049ae30; skill 0 / 1 cases |
| 11 | Watch_Mission11_Sentinels_Defend (override, extra) | 0x006167b0 | same: `cmp 0xc; setle` at 0x0061686d and 0x00616ac8 |
| 12 | kasfMissionCompleted (the end path) | 0x00491c70 | same (tutorial word, clCommandMessage, speechEventCleanup, singlePlayerMissionCompleteCB, animBinkPlay 0x00421860 = the tree's animAviPlay); the hook is HWI_ORACLE only |
| 13 | singlePlayerMissionCompleteCB | 0x0051be40 | same (selection 0, ioUpdateShipTotals, mrRenderMainScreen 0, quittime = realtimeelapsed, utyCreditsSequence 1) |
| 14 | utyGameQuitToMain (IDENTICAL row, the credits) | 0x005ab6b0 | same: track 0x29, psModeBegin("Credits\\", 0xd = PMF_Credits / MusicTrack / LanguageSpecific), "Credits-GameEnd.cred" (strings read from the exe) |
| 15 | cat_spWarpToLevel | 0x0051b9c0 | same (sMothership 0x12, the 0x91-type loop, carriers kept to 8, CLASS_Component 0xa, sCarrierSupport 0xd host search and its fallback, DPT_Support_EB point and the PI turn on odd points, clamp, research module, resources, tech OR, upgrades, the ether walk (the R8 CAT-FIX on the restart), state 6, singlePlayerLoadNewLevel(0, 1)) |
| 16 | the warp records' reader cat_spWarpShipsSetCB + table | 0x005170b0, 0x00894350 | same (record stride 0x2F0, ships at +4; the six table rows WarpEnabled .. Tech with their callbacks) |
| 17 | flow, Bentusi: Watch_Mission17_Bentusi (+ _Wait, extra) | 0x0064a450 (0x0064a4e0) | same |
| 18 | flow, pirates: Watch_Mission10_PirateWave2_AttackPlayer | 0x00610a00 | same |
| 19 | flow, Beast: Watch_Mission17_Naggarok_RunNear | 0x0064ad10 | same (ShipsSelectCapital argument order, 300000 = 0x493e0, "bCruiseMissile" at 0x88b65c, DockInstant) |
| 20-22 | Tutorial1: Watch_..._Lesson7, Init_..._Lesson11, Init_..._Lesson12 | 0x0066ec50, 0x0066f820, 0x0066fd30 | same (Lesson7 tests ClickAttack and BandAttack in one watch, then resets the queue: the case the tutGameSentMessage fix serves) |
| 23-26 | kasf first called by M10-17: kasfShipsParalyze, kasfTimesHitByEMP, kasfNomadMoonFire, kasfSlipgateExplode | 0x00493420, 0x00493460, 0x0049a3a0, 0x0049a490 | same (`& ~0x4000 | (p & 1) << 14` at +0x1CC; sum of +0x52C; cat_nomadMoonFire 0x005bdcf0; the vector by value to 0x00515b40) |
| 27-28 | IDENTICALs kasfMissionFailed, kasfTeamDockedReadyForLaunch | 0x00491cb0, 0x00494940 | same (jmp 0x0051be70; SOF_Hide 0x8000 without SOF_Hyperspace, 0x467a90 / SPECIAL 0x1000 + 0x467a60) |

The port's seven exe fixes, each against the disassembly: scriptStringToBool (`cmp al,0x31 / 0x39` at 0x00525c6a),
tutGameSentMessage (0x00a6c2c4 only read; no reset call), MakeExplosionRockBumpables (`test 0x100000` / `test
0x40000000` at 0x00546157 / 0x005461a4), cat_kbFocus (`sub esp,0xfa8`, `rep movs` 0x3EA dwords; MaxSelection =
SelectCommand layout, FocusCommand is a SelectCommand typedef), cat_bigGunProcessFireCommand (edi held across the
three calls, field cleared after), spHyperspaceSelectionOut (host test 0x00518a05 .. 0x00518a13; the StartInHS branch
clears +0x4 only when 0x2000000 is clear, 0x005189b8 .. 0x005189d3; the tree's `leaves` read before
InstantlyHyperspaceOut is equivalent since nothing in between changes SOF_Hyperspace): all correct.

## 4. Runs (auditor's clean debug build `e65ba313`, plain harness)

Missions 10 .. 17 (`--mode missiontest --mission N --mission-test-ticks 90000 --timeout 3600`, pairs `a9m3-mN-a/-b`):
every run rc 0, ASan 0, no unsuppressed leak, first stop none; each pair byte-identical and **equal to the producer's
whole-file hash** (same base, same build identity); objective ticks equal the report's table:

| mission | done line | run.hwo | end |
|---|---|---|---|
| 10 | 5/5 PASS, 21938 | `c2ce3bd90fde7545` | GoNow (remaining 41) -> Mission11; mines=4 |
| 11 | 5/5 PASS, 8275 | `6d68a150e477d31c` | GoNow -> Mission12 |
| 12 | 3/3 PASS, 10995 | `0f692f9bf5aa50b0` | GoNow -> Mission13; mines=26 |
| 13 | 3/3 PASS, 21746 | `101cf559320b56bd` | GoNow -> Mission14 |
| 14 | 4/4 PASS, 22339 | `6771039a945af3c0` | GoNow -> Mission15; mines=32 slipgates=6 |
| 15 | 3/3 PASS, 15424 | `4e003c8c99f7680b` | letterbox -> Mission16; slipgates=2 |
| 16 | 6/6 PASS, 21522 | `c4b0e21e6cc9a1e0` | GoNow -> Mission17 |
| 17 | 4/4 PASS, 15584 | `244b3664ab20976a` | **game end**: `missiontest: game end tick=15584 credits=Credits-GameEnd.cred completed=15584`, milestone "the game's end (credits)" |

Missions 01 .. 09 once (`a9m3-mt1` without `--mission`, `a9m3-m2` .. `-m9`): all PASS, rc 0, ASan 0, hashes equal to
the producer's `r9f-*`. Against producer 2's audited `a9m2-*` (`hwo_compare.py <ref> <cand>`, all fields): 01
(11,426 ticks), 02, 03, 04, 07, 09 **IDENTICAL**; 06 IDENTICAL (`d77009ce7c2b39db`, the variant matching the
reference; consistent with the report's rng_all-only flake in 2 of 4 runs); 05 first divergent tick **8535,
ships_health**; 08 first divergent tick **18792, rng10** (4 draws ahead). Both moves verified at their stated cause:
- 05: a variant build with only the UnivUpdate.c ship-part exclusion reverted (`original/scratch-m2/audit-r9m3-variant`,
  run `a9m3-m5-noblastfix`) gives records IDENTICAL to `a9m2-m5-a` (16,353 ticks): the move is the blast fix.
- 08: `missiontest: event tick=18792 CaptureResourcers (support units): scuttle 4 small craft`: the test player's
  new scuttle (test code).

Other DONE WHEN items:

| run | result | stream | vs the audited (`a9m2-*`) |
|---|---|---|---|
| `tools/oracle/mission_test.sh --bin <audit debug> --label a9m3-mission --ticks 6000` | MISSION TEST PASS (a = b, perturb at 1500 rng10, flipped copy at 2000 ships_health); ASan 0/0 | **`dc2fb8ce71c0d26b`**, 1,476,370 B | `a9m2-mission-a` (`be3e5030`): IDENTICAL, 6,001 ticks |
| `a9m3-warp10-a/-b` (`--mode warpstart --warp-level 10 --mission-test-ticks 4000`) | `done level=10 ticks=99 applied=1 level=1 kas=1 arrived=35 fleet=1 failed=0 result=PASS`; applied `resources=8000/8000 types=18/18 ships=76/76 (all 97) ether=79 outside=0 tech=20/20`; ASan 0/0 | `46690e6a7b4cfeb1` pair | new (= producer's) |
| `a9m3-tut` (`--tutorial --mission-test-ticks 30000`) | `lessons=12/12 complete=12/12 inOrder=1 nextClicks=11 result=PASS`, Lesson12 5/5 at 15106; ASan 0; 12 lesson saves written | `2dbb97754c1ca05f` (wall-clock speech: not a pair, as reported) | - |
| `a9m3-tload-a/-b` (`--mode loadgame --training --save-src runs/a9m3-tut/settings/SavedGames/Training --save "LESSON 11 - Researching and Upgrading" --load-ticks 3600`) | `loaded singlePlayer=1 tutorial=1 ... kas='tutorial1' watch=1`, `done ... objectives=1/1 kasTeams=10 result=PASS`; ASan 0/0 | `d6ff243bd85f6cfb` pair (the save is this audit's tutorial run's, so not the producer's hash) | - |
| `a9m3-lfe2`, `a9m3-armed8` | PASS (91 / 54 ships), ASan 0 | `875350e737d7a520`, `34784de5c3d0d9f6` | IDENTICAL (601 ticks each) |
| `a9m3-ai`, `a9m3-build` | PASS, ASan 0 | `365ec02f314dd779`, `23d9e51680225092` | IDENTICAL (401 / 7,001 ticks) |
| `a9m3-loadgame` ("01 - Hiigara", 3,600) | PASS, ASan 0 | `f1a3fc673a2a6019` | IDENTICAL (ticks 1984-5586) |
| `a9m3-fe` (120 s) | rc 124, Main_game_screen, ASan 0, no leak, 0 fescreen reports | | as before |
| `a9m3-sp` (`--timeout 340`) | rc 1 at 339 s: `texreg.c (569): Assertion of (reg->nPalettes < 8)` after Mission01 -> 04 (the known M2-R10 stop); ASan 0; 40 bytes leaked after the fatal exit | | as producer 2's audit |

Comparison command: `python3 -I tools/oracle/hwo_compare.py <ref>/run.hwo <cand>/run.hwo` (all fields; only the
header's build identity differs, 64f49a98 vs 307c3a50).

## 5. The diff (`git diff 307c3a5` + the untracked files)

- Scope: every changed file is in the report's list; nothing outside the port (no orchestrator/user files touched).
  Non-test engine edits: StatScript.c, Tutor.c, UnivUpdate.c, KeyBindings.c, Mothership.c, SinglePlayer.c
  (spHyperspaceSelectionOut, catKasScripts); each carries a `CAT (M2-R9 missions-10-17)` comment with the exe address.
  Everything else is `HWI_ORACLE` / HWI-TEST (hooks, switches, MissionSelfTest.c, OracleDump.c, the harness).
- Delta map: 20 `[review: M2-R9 missions-10-17]` notes in delta_map_review.json, merged (20 in delta_map.json,
  `m2r9:missions-10-17` in done_subsystems); counts IDENTICAL 2761 / MODIFIED 1576 / REWRITTEN 83 / reviewed 3346 as
  reported. Every edited function with a row has a note (utyGameQuitToMain, the one IDENTICAL row edited, has one);
  cat_kbFocus 0x0054f2c0, chase 0x0054f1c0, cat_bigGunProcessFireCommand 0x005bbe80, cat_spWarpToLevel 0x0051b9c0 have
  no row (catnew). Observation, not counted: the merge re-encoded one unrelated note's `→` as `→` (same JSON value).
- CAT-TODOs: none added; `grep -rn "CAT-TODO(M2-R9 part B" src/` = 0; no CAT-TODO names M2-R8 or M2-R9.
- Generated files: all reproduced by a rerun (section 2).
- Stream moves: the port states 05 and 08 (verified above) and the new streams; every other guard rail is unchanged.

## 6. Findings

1. **Report: M12's unreachable WorkerReturn misattributed** (fixed). Section 5 said "M12 WorkerDock / WorkerReturn:
   message 0x04f3cd02 is never sent". 0x04f3cd02 (FleetIntel, Mission12.c:284) only leads to WorkerDock's 0x008d9fd3;
   WorkerReturn waits on 0x0d00b093, sent by Watch_Mission12_SecurityTeam_Docking 0x006218c0, whose team stays empty
   (`missiontest: fsm tick=1 team=be7033d ... ships=0` in `a9m3-m12-a`), as MissionSelfTest.c's own Mission12 note
   says. Corrected `[audit: ...]`.
2. **Stale comment pointing at a closed CAT-TODO** (fixed). SinglePlayer.c `singlePlayerKasMissionStart`: `return; //
   not generated yet (CAT-TODO above)`; the port closed that CAT-TODO and filled every row, so the branch is never
   taken. Replaced with an `[audit: ...]` comment (no code change; release binary byte-unchanged).

Not findings (stated by the report, confirmed): animBinkPlay is GoK's stub (M2-R10); the sp texreg stop (M2-R10); the
Mission06 rng_all flake (my run matched the reference); tutorial pairs need speech off the wall clock.

## 7. Precision and fixes

Sampled checks: clean builds; generator counts; determinism; 11 generated bodies (8 random, 3 overrides); 3 end-path
bodies; the warp start and its reader (2); 3 flow bodies; 3 Tutorial1 lessons; 4 kasf; 2 IDENTICALs; the 6 port fixes;
8 mission pairs; 9 claims about missions 01-09 (incl. the 05 / 08 causes, 06 flake); game end; warp start; tutorial
leg; tutorial save load; headless Mission01 stream; 7 guard rails; level-object counts; CAT-TODO state; delta-map
notes / counts; 3 unreachable-objective claims; body counts; override counts; the CAT-TODO's leftovers: 75 checks,
73 correct, 2 wrong (findings 1-2). Precision = 73 / 75 = 0.97.

Fixes: 2 (one report statement, one source comment). No stream moved by the fixes; the build stays green.

## 8. Reference streams for the next port

Headless Mission01 6,000 ticks (debug, auditor's clean build): **`dc2fb8ce71c0d26b`** (records = `a9m2-mission-a`).
Mission01 self-test `5bcaf13b1cba4a45`; missions 02 .. 09: `0917cc32`, `4803d7cc`, `a95a64d1`, `fb4398be`, `d77009ce`
(06; `e95151c4` in the flaky variant), `55571ec3`, `c89457cc`, `93d35ed5`; missions 10 .. 17 as in section 4; warp start
`46690e6a7b4cfeb1`; LFE2 `875350e737d7a520`; Armed8 `34784de5c3d0d9f6`; ai `365ec02f314dd779`; build
`23d9e51680225092`; loadgame `f1a3fc673a2a6019`. fe Main_game_screen ASan 0; sp ASan 0 with the Mission04 texreg stop.

Verdict: PASS-WITH-FIXES - Mission10-17 generated (2,085 of 2,085 bodies verified, reproduced byte-for-byte), 28 bodies match the exe, every mission self-test 10-17 PASS with byte-identical pairs equal to the producer's and ASan 0, the game's end (credits) reached, warp start / Tutorial1 leg / lesson-save load PASS, Mission01 stream and every guard rail unchanged (05 / 08 moves verified at their stated causes); 2 fixes (a report statement, a stale comment)
