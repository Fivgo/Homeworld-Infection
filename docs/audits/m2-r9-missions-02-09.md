# Audit: M2-R9 missions-02-09

Auditor: adversarial audit of the M2-R9 producer 2 port (report `docs/m2-r9-missions-02-09.md`), base `64f49a9`.
No commit follows the base: the port is the working tree (`git diff 64f49a9` plus the eight new `MissionNN.c`
files and the report). Every engine run below went through `tools/m2_harness.sh` (plain harness, silent, offscreen;
no GOG game, no DLL, no input), with the auditor's own clean build passed through `--bin`. Exe bodies were read with
`tools/m2_fdec.py`, `tools/m2_fdis.py` and the R2 objdump listing (`original/scratch-m2/m2r2-mesh-tools/exe.dis`;
the "Mine" reader 0x004a3f20 is not a function in the export, so it was read from the listing). No Ghidra was run.

## 1. Clean builds

`rm -rf original/scratch-m2/build-audit-r9-missions-02-09`, then `meson setup` with `tools/build.sh`'s arguments
(debug: `--buildtype=debug -Db_sanitize=address,undefined`; release: `-Doptimization=2 -Ddebug=false -Db_sanitize=none`;
`werror=true` is the project default) and `meson compile`:

| config | result | warnings | size | sha256 |
|---|---|---|---|---|
| debug | `[262/262]`, rc 0 | 0 | 84,457,264 B | `988d2b8e926cf247` |
| release | `[262/262]`, rc 0 | 0 | 4,938,136 B | `62568351ba120925` |

`tools/m2_layout32.sh`: OK (all ILP32 asserts hold; Missile 600 B = the exe's `memAllocFunction(0x258)` of the
"Mine" reader). `python3 -I tools/m2_kasf_table_gen.py --dispatch --check`: KASFuncTable.c and KASFuncDispatch.c
current.

## 2. Generator

- `python3 -I tools/m2_kas2c_gen.py --check`: rc 0, every output current (Mission01 .. Mission09, Tutorial1,
  KasShared.c, CatMissions.h, meson.build), `verified 891 bodies: callees and constants equal to the machine code's,
  10026 all-constant calls' arguments equal`: the report's counts.
- Determinism: a second, independent run from scratch (tools/ and src/Missions copied to
  `original/scratch-m2/audit-r9m2-gen2`, the eight new MissionNN.c, KasShared.c and CatMissions.h deleted there, the
  generator run in write mode with `--exe` / `--export`): all 13 outputs byte-identical to the tree (`cmp`), which
  covers the "10 random functions" check with every function.
- The 4 OVERRIDES, each against the disassembly: 0x005eb0b0 (`push 0xb7f8e9` at 0x005eb649) and 0x005ebd50
  (`cmp eax,0xb7f8e9` at 0x005ebdab): the hash, correct; 0x00604c90: `call 0x492ae0` (kasfShipsOrderAttributes),
  `neg / sbb / inc / test al,2 / je` then `kasJump(0x4c9664, 0x604c30, 0x604c90)`: always false, the generated body
  reproduces it; 0x006071e0: `setle dl; push edx; call 0x0049ae30` (nullsub, the kas2c table's dbgAssert row), and
  kasfAssert is the empty stub in KASFunc.c: correct.
- The new raw-literal rule's positional mapping (the riskiest new rule: the k-th Ghidra literal = the k-th candidate in
  address order) was instrumented in the scratch copy: it fires in 6 bodies, all Mission03 (`""` = the hashes
  0x4f12a7 / 0x4f12a8, the patrol team labels). Each of the 14 uses was checked against the push it stands for
  (0x005e4c60, 0x005e2460, 0x005e1440 (4 uses), 0x005e2650, 0x005e2850, 0x005e2fd0): all in the right place.

## 3. Bodies compared with the exe (25 + the extras)

| # | body | exe | result |
|---|---|---|---|
| 1 | Init_Mission02_DestroyGates_Attack | 0x005df690 | same: call order, argument order (ShipsSelectNearby dest/src/vector/10000), `if` senses, kasfRandom(10, 30), TimerCreateSetStart |
| 2 | Watch_Mission03_BigGuard_Hyperspace | 0x005e4210 | same (kasJump targets 0x5e4270 / 0x5e4330 = Init/Watch_Mission03_BigGuard_Guard) |
| 3 | Init_Mission04_BeastFighter_Null | 0x005e8c80 | same |
| 4 | Watch_Mission05_BEASTCarrier | 0x005edc60 | same (`jge` skip = `< 0x50`, +0x1c shipList arguments) |
| 5 | Watch_Mission05_RammingFrigates | 0x005f3010 | same (ICF targets 0x65f6f0 / 0x5f30d0) |
| 6 | Init_Mission06_HvCruiser | 0x005f6ae0 | same |
| 7 | Init_Mission07_InfectedShips_Attack | 0x00600f50 | same (ShipsRemove(0x36980eb, 0x4843b49)) |
| 8 | Watch_Mission08_SpyChasers_Null | 0x00603b20 | same (MusicPlay(7)) |
| 9 | Watch_Mission09_FighterPatrol_Alert | 0x00608180 | same (both kasJumps, the early return) |
| 10 | Watch_Mission09_Resourcers | 0x00607150 | same |
| 11 | "Mine" reader cat_scriptSetMineCB | 0x004a3f20 | same: player = singlePlayerGame ? players[1] (0xb6da30) : players[7] (0xb93950); flags 0x180003; race 6 at +0x1e8 and +0x214; staticinfo 0xb3b2e8; MISSILE_Mine; maxtraveldist = maxvelocity * 0.0625; REALlyBig lifetime / camera distance; soundType 8 (GS_MineLauncher); hitEffect `[0xc41238 + race*0x190]` = etgGunEventTable[race][GS_MissleLauncher][EGT_GunHit] (base 0xc41180, cross-checked with two other addresses the tree cites); damage 5000; ID table only under multiPlayerGame; univInitSpaceObjPosRot on the scanned (untransformed) vector at esp+0x18; isMoving / haventCalculatedDist words 1; bobAddObjToNearestBlob(collBlobList) and the three lists; cat_univShipMeshStateCreate |
| 12 | cat_scriptSetSlipgateCB | 0x004a1020 | same: format, both fatals, missionman branch, univCreateShip(type, 6, pos, player, 0, 0, 0) = the tree's univAddShip, gameStatsAddShip, yaw, the 0x00c362dc pairing, flags `& ~0x100000 | 0x80000`, disable reason (1, 6) |
| 13 | flow: roll call under fire, cat_spEmergencyHyperspaceCB | 0x005174b0 | same (wide screen 90, 0x2f / 0x23, catFlags18 0x40, specialFlags +0x1c8 `or 0x8080`, screen delete) |
| 14 | flow: at-once arrival, cat_spLaunchDockedShipsOnArrival | 0x005191a0 | same (hive frigates 8 / 0x2a, oldAutoLaunch, upgrades, link resets, healing, flag 0x80 cleared, parade; Mission04's dEcclesiams) |
| 15 | flow: failed jump return, singlePlayerGameUpdate's case | 0x005197d0 | same (catSpEtherCleanup(FALSE, FALSE): ally ships out of the ether; with hyperspaceFails the ShipList pass under ShipCanMakeHyperspace, whose macro holds the exe's range-flag test; spUnlockout, launches, autolaunch, Game_HyperspaceFailed) |
| 16 | flow: the animatic, HYPERSPACE_WHITEOUT case | 0x005197d0 | same (animBinkPlay(cur, cur + 1), mission 17's credits, the fade words, binkDonePlaying) |
| 17 | kasfTeamHealthAverage | 0x00492b20 | same (sum, *100 / max, 1.5*2^52 rounding = lrint) |
| 18 | kasfFindMinesInside | 0x00493130 | same |
| 19 | kasfShipsSelectNumber (+ predicate 0x00495290) | 0x00495260 | same (unsigned `jae`) |
| 20 | kasfShipsEffectedByMines | 0x0049a080 | same (catFlags18 bit 5) |
| 21 | self-test start site, singlePlayerInit after Warp.script | 0x00518c50 | same as the exe's warp branch; the HWI-TEST hook only sets the two words Warp.script would |
| 22-25 | IDENTICALs kasfObjectiveCreateSecondary, kasfObjectiveSet, kasfObjectiveDestroy, AllowPlayerToResearch | 0x00494ed0, 0x00494f10, 0x00494f60, 0x0052e9a0 | bodies as the exe (the hooks are HWI_ORACLE-only and reviewed; AllowPlayerToResearch's CAT-FIX skips the exe's write at index -1 = 0x00ba467e, as the review note says) |

Extras: the three kasf fixes (kasfSpecialOn `mov eax,1` at 0x00497da4 / `xor eax,eax` at 0x00497dab; kasfVectorAdd /
kasfVectorCopy return after the overwrite in the decompile): correct. The Tweak.c claim (StatScript.h `makeEntry`
passes its argument through `strdollar(x) #x`, so `CLASS_*` is macro-expanded before stringification: the keys were
"twRepairRate[0]" .. and Tweak.script spells `twRepairRate[CLASS_Mothership]`): correct. The BeastWeapon.c claim
(the offset asserts in BeastWeapon.h hold for ILP32 only, every caller reads the fields at once): correct.

## 4. Runs (auditor's clean debug build, plain harness)

Missions 02 .. 09 (`--mode missiontest --mission N --mission-test-ticks 90000 --timeout 3600`, pairs
`a9m2-mN-a` / `-b`, four at a time): every run rc 0, `mission self-test done`, first stop none, ASan 0, no
unsuppressed leak; every `end` line `level=1 kas=1 arrived=1 inOrder=1 failed=0`; each pair byte-identical and equal
to the producer's hash:

| mission | done line | run.hwo | arrival | jump / next level |
|---|---|---|---|---|
| 02 | 6/6 PASS, ticks 25136 | `724546f07da17829` | 213 | letterbox -> Mission03 |
| 03 | 5/5 PASS, 17825 | `9e9c9e9460dbd582` | 212 | GoNow (remaining 22) -> Mission04_NIS02, n02.nis |
| 04 | 5/5 PASS, 20816 | `cdbd36b35a03bc05` | 35 | GoNow (remaining 5) -> Mission05 |
| 05 | 6/6 PASS, 16384 | `fa71938ce90ba5a7` | 35 | letterbox -> Mission06_NIS04, n04.nis |
| 06 | 7/7 PASS, 30931 | `b20c11c04e77f9e5` | 35 | GoNow (remaining 39) -> Mission07_NIS05, n05.nis |
| 07 | 4/4 PASS, 16592 | `66b6ec46c98c6e55` | 35 | letterbox -> Mission08 |
| 08 | 6/6 PASS, 29363 | `a405cd7344b40d48` | 35 | GoNow (remaining 55) -> Mission09 |
| 09 | 6/6 PASS, 20579 | `89efb68ab6f952ee` | 213 | EmergencyHyperspace -> Mission10_NIS06, n06.nis |

The objective tables (`catMtMission02..09`) were checked against the generated Objectives FSMs: every label the
scripts create is in its table except Mission06's "Kill", created only on message 0x4fe0c, which no Mission06 or
shared body sends (grep): unreachable, correctly left out. The held objectives (Mission04 "Destroy", Mission09
"Protect") are never set or destroyed by name in their scripts (grep), as the report says.

Guard rails:

| run | result | stream | vs the audited (`r9a2-*`, producer 1's audit) |
|---|---|---|---|
| `tools/oracle/mission_test.sh --bin <audit debug> --label a9m2-mission --ticks 6000 --no-perturb-run` | MISSION TEST PASS (a = b, flipped copy caught at 2000 / ships_health); ASan 0 | **`be3e5030cebbea0b`**, 1,476,370 B | `r9a2-mission-a`: IDENTICAL, 6,001 ticks (build identity only) |
| `a9m2-mt1-a`, `-b` (missiontest, Mission01) | `done mission=1 ticks=11424 objectives=6/6 result=PASS`, a = b, ASan 0 | `aac59b117769f451` | `r9a2-missiontest-a`: IDENTICAL, 11,426 ticks |
| `a9m2-lfe2-a`, `-b` (skirmish LiveFireExercise2) | `ticks=600 ships=91 result=PASS`, a = b, ASan 0 | `4f97795dd286d44d` | `r9a2-skirmish-lfe2-a`: first divergent tick 0: ships_n 89 -> 91, ships_misc, ships_pos, ships_vel, ships_rot, ships_health, univcheck (the two Slipgate lines of `resource.missphere`), as stated |
| `a9m2-armed8` | `ships=54 result=PASS`, ASan 0 | `3ac685f989ce36a0` | `r9a2-skirmish-armed8`: IDENTICAL, 601 ticks |
| `a9m2-ai` (400) | PASS, ASan 0 | `b9bdad1e3cae9349` | `r9a2-ai-a`: IDENTICAL |
| `a9m2-build` | PASS, ASan 0 | `3c29833a5ef6e20d` | `r9a2-build-a`: IDENTICAL |
| `a9m2-loadgame` ("01 - Hiigara", 3,600) | PASS, ASan 0 | `5e88987d393b6f7d` | `r9a2-loadgame-a`: IDENTICAL (ticks 1984-5586) |
| `a9m2-console` | `kas lines=6 ok=6 tablerows=486`, PASS, ASan 0 | | |
| `a9m2-fe2` (120 s) | rc 0, Main_game_screen, running at the timeout, ASan 0, no leak, 0 fescreen reports | | |
| `a9m2-sp2` (`--timeout 340`) | rc 1, 338 s: `texreg.c (569): Assertion of (reg->nPalettes < 8) failed` after three `debugScreenshots singlePlayerNextLevel` (Mission01 -> 02 -> 03 -> 04); ASan 0; 40 bytes leaked after the fatal exit | | producer 1's audit: rc 124, still in Mission01 (finding 1) |

Comparison command (every pair): `python3 -I tools/oracle/hwo_compare.py <ref> <cand> --fields
rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real`
(LFE2's per-field list also through `--json`).

The first parallel attempt of fe and sp (`a9m2-fe`, `a9m2-sp`, run beside five other harness runs) ended with harness
rc 2 and no summary.txt although both engines had run (sp's log shows the same texreg fatal); run alone (`a9m2-fe2`,
`a9m2-sp2`) both summarised normally. Not a port defect; noted for whoever runs the harness in parallel.

## 5. The diff (`git diff 64f49a9` + the untracked files)

- Scope: every changed file is in the producer's list. Outside the missions proper but justified by the runs:
  Blobs.c (a GoK refactor assertion), BeastWeapon.c (64-bit layout), Debug.c (an ASan-only stack trace in dbgFatal),
  Tweak.c (the M2-R4 key bug), TradeMgr.c (CAT-FIX). Each has its comment and, where the function has a delta-map
  row, a review note.
- IDENTICAL rows edited: dbgFatal, AllowPlayerToResearch, kasfObjectiveCreateSecondary / Set / Destroy / DestroyAll:
  each has an `[review: M2-R9 missions-02-09]` note (50 notes in delta_map_review.json, 40 of them "compared again",
  merged into delta_map.json with `m2r9:missions-02-09` in done_subsystems: the report's counts). rndRenderTask
  (MODIFIED) changed in a comment only. The Tweaks[] table belongs to scriptSetTweakableGlobals's unreviewed row,
  which the report names as left for M2-R10.
- CAT comments: present on every Cataclysm change (LevelLoad.c readers, includes, table rows; KASFunc.c fixes;
  SinglePlayer.c rows). CAT-TODOs: the only one added is on one line, owner first (`CAT-TODO(M2-R9 part B): ..
  Mission10 .. Mission17 .. producer 3`); none names M2-R8; LevelLoad.c has none left.
- Generated files: all reproduced by a rerun (section 2). Tutorial1.c's one changed line is the generator's (the
  localised table at 0x008cd960, string "Turn on the Tactical view."), reproduced.
- Stream moves: every stream the port moves is stated (LFE2 only among the guard rails; the eight new self-test
  streams are new); confirmed in section 4.

## 6. Findings

1. **sp guard rail now stops on a fatal (open, not fixed; owner M2-R10 render as the report says).** Opening the
   debugScreenshots gate to Mission02 .. 09 (render.c, through `cat_spKasScriptGenerated`) lets the sp run reach
   Mission04, where `trColorsIndexAlloc`'s HW1 assertion `reg->nPalettes < 8` stops the debug build (rc 1 at 338 s;
   producer 1's audited sp ran to the timeout, rc 124). ASan stays 0, so the DONE WHEN wording holds, but the guard
   rail's outcome changed; the report states it (section 7.3) with the evidence that the colour infos read back
   corrupt. Reproduced from the clean build (`a9m2-sp2`).
2. **Report: kasf fix count** (fixed): section 1 said "2 fixes" for kasfSpecialOn, kasfVectorAdd and kasfVectorCopy;
   section 2.4 and the producer's summary count 3. Corrected `[audit: ...]`.
3. **Report: wrong section reference** (fixed): "The mission runs found 6 more bugs, fixed (section 4)"; the table is
   section 2.4 (section 4 is the flows). Corrected `[audit: ...]`.
4. **Report: roll-call counts** (fixed): section 4 said the Go Now jumps left "remaining 38-55" ships out; the roll-call
   lines of the final pairs (identical in the auditor's runs) give 22 (Mission03), 5 (04), 39 (06), 55 (08).
   Corrected `[audit: ...]`.
5. **Harness header stale** (fixed): the missiontest paragraph of `tools/m2_harness.sh` still gave Mission01's line as
   `missiontest: done ticks=N ..`; the port made it `missiontest: done mission=1 ticks=N ..`
   (MissionSelfTest.c:2199). Corrected `[audit: ...]` (comment only; `bash -n` OK).

## 7. Precision and fixes

Sampled checks: 10 generated bodies, 4 overrides, 6 positional raw-literal mappings, the 2 readers, 4 flows, 4 kasf,
the 3 fixed kasf, the start site, 4 IDENTICALs, 3 bug claims (Tweak, BeastWeapon, TradeMgr), the generator's counts and
determinism (2), 8 mission pairs, 10 guard-rail claims (mission stream, Mission01 self-test, LFE2's move, Armed8, ai,
build, loadgame, console, kasf table check, fe), the delta-map counts, the CAT-TODO state, sp's stated stop: 64
correct; wrong: 4 (findings 2-5). Precision = 64 / 68 = 0.94.

Fixes: 4 (three report statements, one harness header line; no code change, no stream moved, the build unaffected).

## 8. Reference streams for the next port

Headless Mission01 6,000 ticks (debug, auditor's clean build): **`be3e5030cebbea0b`** (content = `r9a2-mission-a`
`1028375d20f15d75`, 1,476,370 B). Mission01 self-test `aac59b117769f451` (6/6); LFE2 `4f97795dd286d44d` (91 ships);
Armed8 `3ac685f989ce36a0`; ai `b9bdad1e3cae9349`; build `3c29833a5ef6e20d`; loadgame `5e88987d393b6f7d`; missions 02 ..
09 as in section 4. fe Main_game_screen ASan 0; sp ASan 0 with the Mission04 texreg stop (finding 1).

Verdict: PASS-WITH-FIXES - Mission02-09 generated and reproduced byte-for-byte by a rerun (891 bodies / 10,026 tuples, 4 verified overrides), 25 bodies match the exe, every mission self-test PASS with its pair byte-identical and ASan 0, Mission01 stream and every guard rail as stated (LFE2's Slipgate move only); 4 doc fixes; open: the sp run now stops on texreg's palette assertion in Mission04 (M2-R10 render)
