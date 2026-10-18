<!-- done: skirmish, player, fleet-resource, team, aitest -->
# M2-R7 ai-player-skirmish: AI player, fleet and resource managers, teams, the Cataclysm skirmish AI

Status: DONE except the final mission / sp / dock / build / layout32 gates still running at hand-off (M2-R7 producer 3; base commit `1410c22`; nothing committed by the producer, the auditor checkpoints).

Subsystems (in this order): **skirmish** (new `src/Game/AISkirmish.c` / `.h`: the 17 `cat_ais*` functions, `aisRequestShip`,
`aisFleetUpdate`, the 8 play timers, `cat_aisReset`), **player** (AIPlayer.c / .h and AITeam.h at the exe's layouts, the
message queue, the KAS variable store, the AI save chunks of the mode-A loader), **fleet-resource** (AIFleetMan.c,
AIResourceMan.c, the AI counters of Dock.c / Salvage.c, the computer player's dock order), **team** (AITeam.c),
**aitest** (build / research / harvest rows of `/catAiTest` and a skirmish leg), **gates**.

## Summary

* The skirmish AI (`cat_aisPlay` 0x00433d50 and the 16 functions it calls) is ported into the new `AISkirmish.c`, with the
  exe's 34 teams (0x9D10 bytes each, static-asserted on i386), the 8 per-player play timers (0x008dfc44), the five
  attack-target tables and the research tables. `aiplayerUpdateAll` runs it for every multiplayer computer player;
  single player keeps HW1's fleet command.
* AIPlayer (0x1D18 bytes) and the message queue are at the exe's layouts; the armada targets are an embedded
  MaxSelection; the KAS variable store (`KASVar.c`, new) is created, saved, loaded and destroyed with the AI player.
* The AI part of a cataclysm.exe save loads through the one tree loader: the AIPlayer, RequestShips,
  TeamWaitingForTheseShips and AITeamMove chunks are converted by SaveCat32 layouts (`tools/m2_save32_gen.py`, 108
  layouts), AITeam by the exe's offsets. M2-R6's stand-in (`aiplayerCat32Load`) is gone.
* Fleet and resource managers: `BuildShip`, `aifBuildRequestedShips`, `aifProcessShipBuildRequests` and the resource
  manager are at the exe's (support modules, external construction, Cataclysm collectors / processors / carriers,
  multiplayer research). `aitMsgSend` / `aitMsgReceived` are rewritten to the exe's pointer queue.
* All 11 `CAT-TODO(stage 4 ...)` tags are closed (`grep -rn "CAT-TODO(stage 4" src/` gives 0), as is producer 2's
  `CAT-TODO(M2-R7 AI player)`; two new `CAT-TODO(stage 6 KAS)` tags carry the KAS message / team identity.
* The ai self-test now also checks that the computer player builds, harvests and, in a skirmish leg, plays, builds,
  researches and harvests with the skirmish AI: result=PASS, 16/16 checks, ASan 0, two runs byte-identical.
* One bug the self-test found while it was being written (before any gate): `cat_aisMothershipUpdate` tested the enemy
  modules team (16) where the exe tests the own charged big gun team (15, 0x0097ce5c), so a Beast mothership got a big
  gun point order and `cat_bigGunProcessFireCommand` dereferenced its missing gun. Fixed; all `aisTeams` references of
  the 17 functions were then decoded from the disassembly (team address = 0x008dfd60 + 0x9D10 * team) and match.
* Mission01 stream: unchanged by the player / fleet changes (wip1 IDENTICAL to base); the final 6000-tick runs were not finished at hand-off. Replay byte-identical to the base binary's.
  **The oracle01 recording changed under this producer**: see "Replay" below.

## 1. skirmish (src/Game/AISkirmish.c, AISkirmish.h; new)

| function | exe | notes |
|---|---|---|
| `cat_aisTeamAddShip` | 0x0042fa80 | joins the nearest selection within 5000 (recentred), else a new one (10 at most) |
| `cat_aisUpdateTeams` | 0x0042fd50 | sorts every live ship into the 34 teams; enemy filter by the bounty leader (integer division, `> 5.0`); SOF_Dead skipped; builds `aiBuildingShip[60]` and the support units pending from the build queue; carriers / support modules / big gun charged |
| `cat_aisSetupAlliances` | 0x004310a0 | the computers follow the humans (tpGameCreated.numPlayers); `aiplayerBigotry` 0 / 1 |
| `cat_aisResearchUpdate` | 0x00431290 | Sect: per completed module the first missing tech of its table (5/5/5/7/7); Beast: 18-entry table while nothing researches; Beast system 4 above 0.7 health (unrounded double compare) |
| `cat_aisTeamGetShip` | 0x00431490 | |
| `aisRequestShip` | 0x00431510 | support units and RUs; external build at the mothership (`clWrapCreateShip`), else `clWrapBuildShip`, then the carriers |
| `aisFleetUpdate` | 0x00431750 | Sect modules in the exe's first-missing order (DockingBay, Micro, Weapons, Special, Armour, BigGun at 5000), support modules (< 12), capital ship choice, carrier support (< 6), Workers 5/8/12 (2/4/6 above 50,000 RUs), scouts, DFG, leeches, mimics + MCV, cruise missiles (400), anti-fighter above 15 enemy fighters, the random medium choice (stream 6 `% 6` / `% 7`), kamikaze / scuttle, fighters 25/35/60 |
| `cat_aisShipIsBusy` | 0x004324a0 | |
| `cat_aisWorkersUpdate` | 0x004324d0 | idle Workers harvest with harvesting on (both upgrade branches add to the harvest group); above 50,000 RUs they repair the least-built ship |
| `cat_aisProcessorUpdate` | 0x00432630 | processor / carrier to the crystal + (500,500,0) above 4000, else guard the first Worker |
| `cat_aisScoutsUpdate` | 0x00432760 | |
| `cat_aisTeamFindNearestSelection` | 0x00432d20 | |
| `cat_aisTeamAttack` | 0x00432df0 | hyperspace when `(double)cost + 10000 < RUs`; formation `ranRandom % 5`; target index 9/18 -> 5; mimic stance; kamikaze at 5; Aggressive |
| `cat_aisMothershipUpdate` | 0x00433310 | standoff 40,000 / 20,000; the point order while the own big gun is charged (team 15) and no DFG near (< 90,000); the world clamp with the exe's y upper bound taken from X (kept) |
| `cat_aisCarrierUpdate` | 0x00433620 | |
| `cat_aisSentinelsUpdate` | 0x00433760 | keeps the exe's inverted selection test |
| `cat_aisLeechesUpdate` | 0x004338e0 | with `aisLeechesSend` (inlined in the exe) |
| `cat_aisReset` | 0x00433d30 | the 8 timers and the alliance flag; called by `gameStart` (0x005a9ef4) |
| `cat_aisPlay` | 0x00433d50 | interval 20 / 10 / 5 s (unrounded double compare), chance 20 / 50 / 100, DFG hyperspace, the five attack groups with the exe's parameters |

Names chosen here (no CET name) [audit: CET's Cataclysm.h names `aisRequestShip` 0x00431510 and `aisFleetUpdate` 0x00431750, and its
AISkirmish.h names the 34 teams (`TEAM_Scout` ..., the tree's `AIS_TEAM_*` follow it except 10, 15, 16, 24 and 25); so the
names chosen here are only:] the statics
(`aisShipBeingBuilt` 0x00a2d980, `aisEnemyBigGunActiveShip` 0x00a2d9a4, `aisEnemyBigGunChargedShip` 0x00a2d998,
`aisCrystalTarget` 0x00a2d99c, `aisOwnBigGunShip` 0x008dfd54, `aisEnemyDFGNearMothership` 0x008dfd58,
`aisLeastBuiltShip` 0x00a2d9b0, `aisFirstPlay` 0x007525e4) and the globals `aiBuildingShip` 0x008dfc64,
`aiSupportUnitsPending` 0x00a2d9b4, `aiCarrier1Ship` / `aiCarrier2Ship` 0x008dfc38 / 3c, `aiHasExternalConstruction`
0x00a2d984 and the three module-queued flags.

CAT-FIX: `cat_aisUpdateTeams` divides the leader's bounty by the ship owner's bounty as integers unguarded
(0x0042fe8f..0x0042fed4); a bounty of 0 would fault, so the tree skips the filter then.

Decompile corrections (Ghidra's HW1 AIPlayer layout shifts every field from +0x950 by 4, from +0xBC8 by 8): the enemy
bounty filter, the Workers' second selection and the `clWrapMove` arguments were read from the disassembly.

## 2. player (AIPlayer.c / .h, AITeam.h, KASVar.c / .h, SaveCat32)

* `AIPlayer.h`: the exe's 0x1D18 bytes; new fields `catNumSupportModulesBeingBuilt` (+0x950), `catSupportModuleRequested`
  (+0xB8E), `catAirNumSupportModules` (+0xBC8); `ArmadaType.targets` embedded (+0xCF8); 30 offsets static-asserted
  under `__i386__`.
* `aiplayerStartup` 0x00410fa0: `cat_kasVarStoreInit`; `utyGameSystemsInit` already passes the exe's arguments
  (the word at 0x00a43cac, tpGameCreated.numPlayers 0x008860ee, numComputers 0x008860d4); the stage-4 tag closed.
* `aiplayerShutdown` 0x00411000: `cat_kasVarDestroyAll`. `aiplayerClose` 0x00411180: same as HW1 (the exe's `nullsub`
  is the empty `aifClose`).
* `aiplayerUpdateAll` 0x00411240: single player also runs while the ships arrive; `aiplayerPlay` (inlined in the exe)
  runs `cat_aisPlay` in multiplayer, `aifFleetCommand` in single player; the playback skip kept.
* `aiplayerPlayerDied` 0x00411410: a dead player's ships go to no computer when it is single player's human (0x0041180a).
* `aiplayerGameStart` 0x00411850 (BuildShipList inlined): ships under external construction left out (0x004118bf).
* `aiplayerSave` / `aiplayerLoad` 0x00412380 / 0x00412400: `cat_kasVarSave` / `cat_kasVarLoad`; one load path;
  `SaveThisAIPlayer` / `LoadThisAIPlayer` save and load the embedded armada targets always (0x00411ff0, 0x00412302).
  CAT-FIX: the exe saves `ResourceManRequestShips.creator` as a raw pointer of the saving process; a cataclysm.exe
  save's value is cleared on load.
* KASVar.c (new; `cat_kasVarStoreInit` .. `cat_kasVarLoad`, stamp 0x6aac470, 0xC-byte entries, grows 64 then +32) [audit:
  `cat_kasVarFind` / `cat_kasVarFindOrCreate` returned the value word; the exe returns the entry and its callers read +8
  (gunShootGunsAtTarget 0x0048c32e): they now return `CatKasVar *`; no caller yet].
  CAT-FIX: `cat_kasVarLoad` frees an array still allocated when the save has none (the exe leaks it).
* AITeam.h: the 64-entry pointer message queue (0x204 bytes on i386), `catMsgQueue` in the team, `catUnknown08`.
* SaveCat32: `AIPlayer`, `RequestShips`, `TeamWaitingForTheseShips`, `AITeamMove` added to the generator's roots;
  `SaveCat32Layouts.c` regenerated (108 layouts; `--check`: current).

The oracle01 load: the same chunks are consumed as by M2-R6's chunk walk (a temporary probe read both paths from the
same file position: lists end 1172004, selections end 1172040, end 1172064, identical), and the ship list after the load
is identical with either path. The replay log reports 1843 converted chunks (base 1842: the AIPlayer chunk is now
converted).

## 3. fleet-resource (AIFleetMan.c / .h, AIResourceMan.c / .h, Dock.c, Salvage.c, UnivUpdate.c, ResearchAPI.c, ShipSelect.c)

* `BuildShip` 0x00404470, `aifBuildRequestedShips` 0x004045b0, `aifProcessShipBuildRequests` 0x004052f0 (with
  `aifRequestStatic`, `aifSupportModuleNotBuilt`, `aifCatRequestUsable`, `aifCatQueueRequest`,
  `cat_aifRequestSupportModule`), `aifTeamDied` 0x00405100, `aifAssignNewShips` 0x00405850; `aifShipDied` and
  `GetSubstituteShipStatic` removed (not in the exe).
* `NumRUDockPointsOnThisShip` 0x00412490, `airInit` 0x004124b0, `airShipDied` 0x00412590, `airAddedResourceCollector`
  0x004126f0 (multiplayer without harvesting: `clWrapDock` retire, 0x0041275b), `airAddNewShip` 0x00412950,
  `aiuCanBuildShipType` 0x00412cb0, `airDivideSupportTasks` 0x00412d40, `airDumbResourcerRequests` 0x00412e10,
  `airDumbResearchRequests` 0x00413070, `airResourceManager` 0x00413240; `airProcessSpecialTeams`,
  `airSmartResearchRequests` and the HW1 per-type helpers removed.
* `cat_rmAIResearchTech` returns whether it ordered research; `selNumShipsInSelection` 0x005118d0 added.
* Dock.c `cat_dockDepositResourceUnits`: the player's collected / total RU stats (0x00463294 / 0x004632aa).
  Salvage.c: the depot owner's crystal stat (0x004f99a5). UnivUpdate.c: a computer player's converted ship docks, 1 in
  10 retire else support (stream 6; 0x00543ce8..0x00543d5c).

## 4. team (AITeam.c)

`aitCreate` (multiplayer tactics Aggressive, 0x004168d6), `aitDestroy`, `aitAddShip` (bCloakedFighter, k/tCloakGenerator),
the coop functions by race (k/t cloak and grav well generators), `aitExecute` (cloaking fighters only),
`aitTeamIsInMothershipRange` 0x00417470 (rewritten; `aitTeamIsInRange` removed), `aitFindMostValuable` (bound order),
`aitMsgSend` 0x00417e10 / `aitMsgReceived` 0x00417e60 (rewritten) with `cat_aitMsgQueueRemoveAt` /
`cat_aitMsgQueueFlush`, the message queue save / load in the exe's format, `aitCat32LoadTeam` (a cataclysm.exe team by
the exe's offsets). HW1's cloak generator processing and `aitOptimize*` removed.

## 5. aitest (src/Game/AISelfTest.c, tools/m2_harness.sh)

New rows: `build` (a COMMAND_BUILDING_SHIP of player 1, clBuildShip 0x0044aaa0, or an external construction),
`harvest` (a Worker of player 1 under COMMAND_COLLECT_RESOURCES), then the skirmish leg (starts once the 12
single-player rows have held): the single-player AI stops (`mrNoAI`, fleet control off), tpGameCreated becomes a
one-human one-computer skirmish with research and harvesting on, `cat_aisReset`, every static of player 1's race is
loaded, its unit cap lifted 200 above use (the scripted fleet is 1119 / 145), its Workers halted, and the multiplayer
branch of `aiplayerPlay` runs on `aiplayerUpdateAll`'s schedule; rows `skirmishplay`, `skirmishbuild` (a build not
queued at the switch), `skirmishresearch`, `skirmishharvest`. The run ends at ticks-2.

The task's "requests reach cmBuildJobsAdd": the exe's AI builds go through `clWrapBuildShip` -> `clBuildShip` 0x0044aaa0
(a delayed COMMAND_BUILDING_SHIP) or `clWrapCreateShip` (external construction); `cmBuildJobsAdd` 0x004586c0 has one
caller, `cmBuildHotKey` (call_graph.csv). The rows check those two.

First ticks (`r7c-final-ai-a`, Beast player 1, advanced): build 17 (bMultiGunCorvette), harvest 33 (bWorker#308);
switch at 65; skirmishplay 81, skirmishbuild 81 (bWorker), skirmishresearch 81 (TECH_DRIVE_FIGHTER), skirmishharvest 177.

## 6. gates (final tree, plain harness; binaries in original/scratch-m2/m2r7-player/)

Builds: `tools/build.sh --debug` and `--release` rc 0, 0 warnings; the changed C files compiled once more with
`-Werror=implicit-function-declaration`: clean. `tools/m2_save32_gen.py --check`: current. `tools/m2_layout32.sh`: queued in the gate script, result not recorded at hand-off [audit: OK; it did not
include AIPlayer.h or AISkirmish.h, so their ILP32 asserts were not checked by it: added, they hold (AIPlayer 0x1D18,
AISTeamEntry 0x9D10)].

| run | result |
|---|---|
| `r7c-base-mission-a/-b` (`mission_test.sh --ticks 6000 --no-perturb-run`, base binary) | MISSION TEST PASS, byte-identical, `0ea19f070fede4c0` (vs producer 2's `7c224543c9ad751a`: IDENTICAL, build identity only), ASan 0 |
| `r7c-wip1-mission-a/-b` (the player / fleet changes) | MISSION TEST PASS, IDENTICAL to the base stream, ASan 0 |
| `r7c-final-mission-a/-b`, `r7c-final-rel-mission-a/-b` | started by the gate script (`gates.sh` in the scratchpad); not finished at hand-off: the auditor reruns them |
| `r7c-final-ai-a`, `-b` | ai self-test done, result=PASS 16/16, ASan 0, no leak, `cmp` equal (`96f479fe44f729cb`); UBSan: the known ETG kinds only |
| `r7c-final-replay-a`, `-b` | oracle replay done ticks=3527 packets=1763 (the new recording's end), ASan 0, rc 0, `cmp` equal (`8dc7808919e8834b`), IDENTICAL to the base binary's `r7c-base-replay-a` |
| `r7c-final-fe` | Main_game_screen, ASan 0, no leak, 0 UBSan reports |
| `r7c-final-sp`, `r7c-final-dock`, `r7c-final-build` | started by the gate script; not finished at hand-off |

### Replay

The recording changed: `original/game/SavedGames/RecordedGames/oracle01` was re-recorded on 2026-10-18 10:13
(sha256 `28d37f75...`, packets `ebe13cda...`; the GOG recording run is `original/oracle/oracle01-rec-20261018-102253/`),
after producer 2's audit (`aud7b-replay-a`, 08:37, recording `f9756e29...`). The harness copies the current recording,
so every replay of this producer, the base binary's included, plays the new one: 1763 packets, the run ends at tick
3527 when the packets end (the old one had 4800 / 9601). The old recording is not in `original/game` any more and the
harness takes no other source, so producer 2's 9601-tick comparison cannot be repeated here. What can be said:

* `r7c-base-replay-a` (base binary) and `r7c-final-replay-a` / `-b` (final): all `8dc7808919e8834b`, `cmp` equal,
  hwo_compare IDENTICAL over 3527 ticks and 1763 packets: the AI port changes nothing in the replay.
* Against the new GOG recording stream (`oracle01-rec-20261018-102253/oracle01-rec.hwo`, the docs/m2-r6 arguments):
  base and final alike, FIRST DIVERGENT TICK 2 (`real`, 1 ulp: 615605.125 vs 615605.188), first packet divergence 68
  (tick 137, univcheck 1234771699 vs 1234771701). The `oracle01-play-a/-b.hwo` files in that directory are copies of
  the 10-17 playbacks of the old recording (`cmp` equal to `original/game/oracle01-play-a.hwo`), not of the new one.

## Delta map

44 review entries (`[review: M2-R7 ai-player-skirmish]`) in `ghidra/delta_map_review.json`, merged
(`delta_map.py merge --done m2r7:ai-player-skirmish`: 44 records changed; counts IDENTICAL 2856, MODIFIED 1480,
REWRITTEN 84, reviewed 3028) and `docs/m2-r1-function-deltas.md` regenerated [audit: `aiaClose` 0x00402780, edited here (the
armada targets' free dropped) but still IDENTICAL without a review, got a MODIFIED entry; totals now IDENTICAL 2855,
MODIFIED 1481, reviewed 3029]. Found modified (were IDENTICAL):
`aiplayerGameStart`, `SaveThisAIPlayer`, `airInit`, `aiuCanBuildShipType`, `airDivideSupportTasks`,
`airDumbResourcerRequests`, `aitCreate`, `aitDestroy`, `aitAddShip`, `aitSpecialDefenseCoopTeamDiedCB`,
`aitTeamIsntCoopWithAnotherTeam_GravWell`, `aitFindGravWellGeneratorCoopTeam`, `aitFindGoodCoopTeam`, `aitExecute`,
`aitFindMostValuable`, `SaveThisAITeam`, `aitLoad`. `aiplayerClose` re-classed IDENTICAL (the exe's nullsub is the
empty `aifClose`). The other IDENTICAL bodies of the four files were read against the decompile (calls, constants,
renumbered move ids) without a further delta: `aiplayerChooseEnemies`, `aiplayerInit`, `aiplayerShipLaunchedCallback`,
`aiplayerShipDied`, `aiplayerResourceDied`, `aifResourceManRequestsShipsCB`, `aifTeamRequestsShipsCB`,
`aitDeleteCurrentMove`, `aitDeleteMovesUntilMoveType`, `aitShipDied`, `aitResourceDied`, `aitTeamIsntDefendingMothership`,
`aitTeamIsFinishedMoving`, `aitTeamIsDoingSpecialOp`, `aitAnyTeamOfPlayerAttackingThisShip`, `aitMakeTeamSupportShips`,
`aitCheckIfOtherDefTeamAnsweringSignalNeedsHelp`; the remaining small save / fix callbacks were not re-read.

## CAT-TODOs

Closed (12): AIFleetMan.c (the R1/R2 substitute), AIPlayer.c (the mode-A AI chunk stand-in), AIResourceMan.c (4: the
HW1 ResearchShip / ResourceController requests), Dock.c and Salvage.c (the AI counters), UnivUpdate.c (the converted
ship's dock), utility.c (2: aiplayerStartup's arguments, cat_aisReset), AIUtilities.c (`CAT-TODO(M2-R7 AI player)`:
the embedded armada). Re-targeted / new (2, both `CAT-TODO(stage 6 KAS)` in AITeam.c): a queued message's kas2c id
in saves; a cataclysm.exe team's KAS identity (label hash, FSM / state names, watch functions).

HW1 type aliases: none left in the code of AIPlayer.c, AIFleetMan.c, AIResourceMan.c, AITeam.c, AISkirmish.c (comments
and one commented-out HW1 block only).

## What is left (owners)

* **stage 6 KAS**: the two tags above; a single-player AI save from cataclysm.exe with team moves loads them through
  the AITeamMove layout, whose `params` union is one of the union conflicts the generator reports (bytes 0x28-0x2b
  written twice, converted member by member, the later write wins; oracle01's teams have no moves, so it is
  unexercised).
* **combat / effects (not this producer's code, seen in a 4000-tick ai soak and with the base binary too)**:
  `ETG.c:7295` assertion `nParams >= 0 && nParams <= 8` (fatal in a debug build; the base binary hits it near tick
  1,300 of the same run) and a UBSan null member access in `univRemoveAllTargetReferencesFromBullets`
  (`&bullet->owner->targetable` with a NULL owner, HW1 code).
* **oracle**: a GOG playback stream of the new oracle01 recording (only its record stream exists), or the old
  recording back under another name, to redo producer 2's 9601-tick replay comparison.
