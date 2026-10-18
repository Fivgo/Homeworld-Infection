# Audit: M2-R7 ai-orders-moves (producer 2: AI orders, moves, handlers, events, attack / defense managers, ai self-test)

Auditor: M2-R7 adversarial audit, 2026-10-18. Base `a3cc7f4`; the port is the working tree (no WIP commits after the
base). Producer report: `docs/m2-r7-ai-orders-moves.md`. Every engine run below went through `tools/m2_harness.sh` (or
`tools/oracle/mission_test.sh`, which calls it) on the plain harness, silent and headless, with the audit's own binary.

## 1. Clean build

`rm -rf original/scratch-m2/build-audit-r7-ai-orders-moves`, then the two configurations exactly as `tools/build.sh`
sets them up (`meson setup --buildtype=debug -Db_sanitize=address,undefined` and `meson setup -Doptimization=2
-Ddebug=false -Db_sanitize=none`; `werror=true` from the project's default options, `-Werror` on every compile line of
both `build.ninja` files): both rc 0, **0 warnings** in either compile log. Sizes: debug `hwi-engine` 87,248,936 B,
release 5,384,992 B. Rebuilt incrementally after the audit's comment fix (section 5): both rc 0, still 0 warnings.
`tools/m2_layout32.sh`: `m2_layout32: OK` (AITeam.h is in its translation unit, so the new AITeamMove 0x170 / AIEvents
0x88 asserts are evaluated).

## 2. Bodies compared with the exe (`ghidra/exports/cataclysm_gog_r6`, `tools/m2_fdec.py` / `m2_fdis.py` / `m2_exedata.py`)

| body | exe | verdict |
|---|---|---|
| `aioCreateGuardShips` | 0x0040e160 | matches: Sect 6 / Beast 9 alternatives, types and equivalents (stack stores decoded through the ShipType enum), the three Beast rows gated by `catShipTypeEnabled` (+0x2EC+0x5FC4+4*type = 0x62B0+4*type, = `cat_playerShipTypeEnabled` 0x004f4780), k/tHeavyDefender with no alternatives (CAT-FIX for the exe's uninitialised stack) |
| `aioCreateHarass` | 0x0040e740 | matches (15, standoff 25,000, priority 1, Beast bACV fallback bAttackBomber) |
| `aioCreateFastRovingDefense` | 0x0040e970 | matches (6 / 8 alternatives, both bACV rows' fallbacks, patrol type 0, distress interrupt) |
| `aioCreateFighterStrike` | 0x0040ee70 | matches (13 sAcolyte + 9, 15 bAttackBomber + 12; script-team tactics at both sites; DELTA / CLAW) |
| `aioCreateActiveSupport` | 0x0040ed80 | matches (sCarrier / bCarrier, sProcessor / bProcessor by ShipPtr[0] +0x2B4; CAT-FIX for the empty team) |
| `aioCreateCapture` | 0x0040f550 | matches (r % 5 + 3; kSalCap 0x48 / tSalCap 0x59) |
| `aioCreateSpecialDefense` | 0x0040f620 | matches (0x40 / 0x4f, radius * 0.8, patrol(=remove) team-died handler) |
| `aioCreateTakeoutMothershipBig` | 0x0040f9c0 | matches (both alternative tables in the big and carrier cases, priorities 0 / 1, WALL armada, hyperspaceable flag) |
| `aioCreateFancyTakeoutTarget` + `cat_statsRandomAttackShipStatic` | 0x00410920, 0x00524d70 | matches: reserve test (SOF_Dead 0x800, Traders 5), class-chosen constraints, guess switch (Kushan keeps the stats' ship), 16,000, redraw above 20; the five tables at 0x0089700c..0x00897074 read with `m2_exedata.py` equal the tree's (Sect 5, Beast 7, Kushan 6, Taiidan 7, P1 2) |
| `aimCreateGetShipsNoAdd` / `aimCreateGetShips` | 0x004070d0 / 0x004071b0 | matches (type 0x16, SAME_FORMATION 8, tactics 2, the dropped allocation is the documented CAT-FIX) |
| `aimProcessFlankAttack` | 0x004083f0 | matches (move-team 6 / hyperspace 0x2C, tactics 2; the inlined advanced attack: script-team tactics only on the advanced path, plain attack Aggressive, as `aimCreateAdvancedAttackNoAdd` does) |
| `aimProcessHarassAttack` | 0x00408d30 | matches (both attack sites, events copy, kamikaze 0x80000, firing handler) |
| `aimProcessActiveCapture` | 0x0040b3f0 | matches; the dontIssue claim re-checked in the disassembly of `cat_aiuWrapCapture` 0x0041f3d0 (`[esp+0xfb8]` after three pushes and one pop is the second argument, tested for zero before `cat_clWrapSpecialTarget`): the exe passes `&onesel`, so no capture order is ever given |
| `aimProcessActiveRecon` | 0x0040a440 | matches (no tactics draw; the two later stream-6 draws kept) |
| `aihHarassNumbersLowHandler` | 0x00405d40 | matches (move-team tactics 0 = Evasive, Sect 2 / Beast 4 alternatives, 16, priority 1) |
| `aihFastDefenseDistressHandler` | 0x00406880 | matches (rescue type by the first ship's race, script-team tactics, resourcer rescue by the AI's race) |
| `aieExecute` / `aieHandlersClear` | 0x00403a70 / 0x00403eb0 | match (12 slots; offsets agree with the new asserts: numbersLow +0x114, teamDied +0x13C, interrupt +0x140) |
| handler table | 0x0074a3a4 | 20 entries + NULL, 15 = 16 = 0x00406e10, 18 = 19 = 0x00406e80, as the tree's comment says |
| `aidDefenseManager` | 0x004035a0 | matches (the inlined defendable count: 8 types count the multiplier, 5 count one) |
| `aidAddAllShips` | 0x00403070 | matches (class 0 / 2 excluded, no fuel handler) |
| `aidMothershipDefense` | 0x00403130 | matches (the lone-fighter `ranRandomFn(6)` then the no-attack branch; the slot refill only when empty, count decremented) |
| `aiaDivideNewShips` | 0x00401b50 | matches (the 145-entry count table decoded: sDreadnought+sDestroyer, sRamming+(sMultiBeam or sHive), bHeavyCruiser+bIonArray, MP return, t pairs, k/t grav well, k/t cloak) |
| `aiaProcessAttackTeams` | 0x004016f0 | matches (r % (SP ? 15 : 11), CAPTURE = 11 and NUM_ATTACK_TYPES = 15 in AIPlayer.h; 30,000) |
| `aiaInit` (kamikaze tail) | 0x00402726 | matches (threshold draw r % (2*range+1) + prob - range first, then r % 101, unsigned compare) |
| IDENTICAL: `aioCreateTakeoutTargetWithCurrentTeam` 0x004106b0, `aihPatrolEnemyNearbyHandler` 0x00406650, `aimProcessKamikaze` 0x0040d8c0, `aimProcessMoveTo` 0x004082a0, `aimProcessPatrolMove` 0x00409760 | | all five identical to the tree |

Also checked: the move-type numbers the exe's creators store (dock 0x17, armada 0x21, control resources 0x22, active
resource 0x29, kamikaze 0x2B, delete team 0x2D, reinforce 0x19, support 0x20, active recon 0x0D) equal the new enum, and
`theSaveMoveTypeFuncTable` (AITeam.c) is still in enum order; control resources / active resource store tactics 1 (the
tree keeps Neutral), active patrol / delete team / kamikaze 2.

## 3. Runs (plain harness, audit debug binary unless stated)

| run | result |
|---|---|
| `aud7b-ai-a`, `-b` (`--mode ai --timeout 300`) | `aitest: done tick=65 ... checks=10/10 result=PASS`, ASan 0, no unsuppressed leak, 15.4 s; `cmp` equal, 98,768 B, sha256 `2f86c13e83803037...` (= the producer's `r7b-final-ai-a`, hwo_compare IDENTICAL 401 ticks); first ticks teams / orders 17, attack move + command 33 (team 2, 11 ships, bMultiGunCorvette#286 order 1, move-team), recon 49 (bCloakedFighter#240, move-to), defence 65 (team 0, 9 ships, bHeavyCorvette#251, move-team-index): as the report states |
| `aud7b-mission-a`, `-b` (`mission_test.sh --ticks 6000 --no-perturb-run`) | MISSION TEST PASS, ASan 0, byte-identical, sha256 `7c224543c9ad751a`; vs the baseline `r6-integrate-mission-a` (`49240abcf9f0ed97`): `IDENTICAL: 6001 ticks` (header build identity only); vs the producer's base-binary `r7b-base-mission-a`: IDENTICAL |
| `aud7b-final-mission-a`, `-b` (after the audit's comment fix, rebuilt binary) | MISSION TEST PASS, byte-identical, **sha256 `7c224543c9ad751a44f76f5267c1663aba531d2daa03f02009953e0f62642d63`**, IDENTICAL to `r6-integrate-mission-a` over 6,001 ticks |
| `aud7b-replay-a` (`--mode replay --timeout 600`) | `oracle: replay done ticks=9601 packets=4800 ships=82 randSyncErr=3857 univSyncErr=4702 blobSyncErr=4746 pktSyncErr=0`, rc 0, ASan 0; IDENTICAL (9,601 ticks, 4,800 packets) to producer 1's audit run `aud7a-replay-a` and to the producer's `r7b-final-replay-a` |
| `aud7b-dock` | `docktest: done ... result=PASS`, ASan 0 |
| `aud7b-build` | `buildtest: done tick=4205 ... result=PASS`, ASan 0; stream IDENTICAL to the producer's base-binary `r7b-base-build` (7,001 ticks) |
| `aud7b-fe` (`--timeout 90`) | `Main_game_screen`, ASan 0 |
| `aud7b-sp` (`--timeout 340`) | universe running, univUpdateCounter 3,376, ASan 0, no unsuppressed leak |

Replay against the GOG recording (the `docs/m2-r6-oracle-mode-a.md` arguments):

```
python3 -I tools/oracle/hwo_compare.py original/oracle/oracle01-rec-20261017-140428/oracle01-rec.hwo \
  <runs>/aud7b-replay-a/run.hwo --from-tick 1 --to-tick 9600 \
  --fields rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real \
  --json <runs>/aud7b-replay-a/compare.json
  -> FIRST DIVERGENT TICK 161, rng6 (CAND 1 draw behind; excluded, decision 47)
(the same without rng6, compare-sim.json) -> FIRST DIVERGENT TICK 177 (last equal 176), ships_vel; packet 46 blobcheck
```

Unchanged from producer 1's audit: IDENTICAL in every simulation field through tick 176. The replay's computer player is
skipped under playback, and the replay did not move: no rerun was needed.

**Mission stream, before / after:** the producer's statement holds: `7c224543c9ad751a` before (base binary) and after,
no field moved. The expected rng6 / computer-player move does not happen because the managers only run with fleet
control (`aifFleetCommand`, AIFleetMan.c 1218: `hasFleetControl = singlePlayerGame ? giveComputerFleetControl : TRUE`),
which only the KAS call (`KASFunc.c` 2892) sets, and Mission01 runs no KAS under `CAT_HW1_MISSION_SCRIPTS 0`. The one
stream-6 change that does run in Mission01, `aiaInit`'s kamikaze pair, keeps its two draws.

## 4. Diff review (`git diff a3cc7f4`)

* Scope: every changed file is the producer's (the AI files, `Stats.c/.h` for `cat_statsRandomAttackShipStatic`,
  `OracleDump.c` / `main.c` / `meson.build` / `AISelfTest.c/.h` / `tools/m2_harness.sh` for the self-test, the delta
  map and its report, the producer's doc). No commit after the base.
* CAT comments: every behavioural change carries a `CAT:` / `CAT-FIX:` / `HWI-TEST` comment; the one removal without a
  local comment (`aieHandlerSetFuelLow` in `aioCreateTakeoutTargetsWithCurrentTeam`) is covered by the file-level
  `CAT (M2-R7 ai-orders-moves)` note of AIOrders.c ("no order has a fuel-low handler"). Accepted.
* CAT-TODOs: the 9 `CAT-TODO(stage 4 AI)` of AIOrders.c / AIHandler.c removed (grep counts 0 and 0); the three
  `CAT-TODO(M2-R7 AI orders)` closed (code removed) or re-targeted; the one added, `CAT-TODO(M2-R7 AI player)` in
  AIUtilities.c, is on one line, owner first. `CAT-TODO(stage 4` in `src/`: 20 at the base, 11 now (AIResourceMan 4,
  AIFleetMan 1, AIPlayer 1, utility.c 2, Dock.c, Salvage.c, UnivUpdate.c: producer 3's scope).
* IDENTICAL bodies edited without a review entry: a script compared every map function of the scope's files at the
  base and in the tree (comments and whitespace stripped) against the review file: **two** (finding F1).
* Stream moves not stated: none (section 3).

## 5. Findings

**F1 (fixed): two edited IDENTICAL functions had no review entry, and the report called them identical.**
`aiePreFixAIEvents` 0x00404210 and `aieFixAIEvents` 0x00404310 lost their fuelLow / fuelHigh lines in this port (correct:
the exe's bodies convert 12 slots, 12 `aieHandlerToNum` / `aieNumToHandler` calls at +0xD0 .. +0x150), but the delta map
still classed them IDENTICAL with no review, and the report's section 1 listed "the fix functions" among the identical
AIEvents bodies. Fix: two MODIFIED entries (sample `audit-R7-ai-orders-moves`) appended to
`ghidra/delta_map_review.json` (the file's existing key order kept), `python3 -I tools/delta_map.py merge` (2 records
changed, nothing else: checked against a copy of the map taken before), `tools/delta_map.py report` regenerated
`docs/m2-r1-function-deltas.md` (it regenerates the producer's version byte-identically before the change). Totals now
IDENTICAL 2,872, MODIFIED 1,464, reviewed 2,995; stage 4 316 / 161 / 2. The report's two statements are marked
`[audit: ...]`.

**F2 (fixed): stale comment in `aimProcessActiveCapture`.** The M2-R4 comment in its processing branch still said the
three-way capture target and `cat_aiuWrapCapture` were left to stage 4; this port made them. Comment corrected
(AIMoves.c, comment only; rebuilt, 0 warnings; the mission pair rerun on the rebuilt binary gave the same stream).

**F3 (note, no change): save indices of the folded handlers.** The exe's table holds the same address twice (15 / 16 and
18 / 19), so its `aieHandlerToNum` writes 15 for the recon-ship handler and 18 for the remove handler, where the tree
writes 16 and 19. Loading either number gives the same body in both, so behaviour is equal; the bytes of an AI save
chunk would differ. For producer 3, who owns the AI save chunks.

**F4 (note, no change): `cat_aihRescueSupportType` for another race.** It returns -1 and the tree still calls
`aiuRescueShipType` (the exe skips the call); `FindFirstInstanceOfShipType` never matches -1, so the call has no effect.

## 6. Precision

Sampled checks of the producer's claims: 50 (the two builds; the 25 bodies of section 2 plus aieHandlersClear, the
handler table, the random-attack tables, active recon and hyperspace; the move numbers and the Neutral creators; the ai
self-test PASS, its byte identity, hash and stage ticks; the mission stream and the fleet-control explanation; the
replay and its divergence ticks; dock, build (and its stream), fe, sp; the stand-in greps, the stage-4 tag count, the
layout asserts; the review totals as stated). Wrong: 1 (the AIEvents fix functions called identical, F1).
**Precision = 49 / 50 = 0.98.** Fixes made in place: 4 (the two review entries with merge and report, the AIMoves.c
comment, two `[audit: ...]` marks in the producer's report).

## 7. Reference for the next port

Mission01 stream of the final pair (`aud7b-final-mission-a` = `-b`, audit debug build, 6,000 ticks): **sha256
`7c224543c9ad751a44f76f5267c1663aba531d2daa03f02009953e0f62642d63`**, IDENTICAL to the baseline `r6-integrate-mission-a`
(`49240abcf9f0ed97...`) in every field (header build identity only). ai self-test stream (400 ticks):
`2f86c13e838030374e0663ae01e392c623f215f9a2d3babd9e3ccada2d5e73ed`. oracle01 replay (debug):
`fe47ed978825c7b141651774d16b60dd55614e319f69b6270b00052839f23fd8` (IDENTICAL to `aud7a-replay-a` but for the header),
first simulation divergence tick 177 `ships_vel`.

Verdict: PASS-WITH-FIXES - orders, moves, handlers, events and the two managers at the exe's in all 25 sampled bodies, ai self-test PASS and byte-identical, mission stream and replay unchanged as stated; two missing delta-map reviews (event fix functions) added and one stale comment fixed
