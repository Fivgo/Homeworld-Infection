# Audit: M2-R4 port "rest-oracle" (the rest of stage 2, the Player layout, oracle stage 2, tools)

Audited: `docs/m2-r4-rest-oracle.md` (done marker `layout, oracle, levelload, tools, rest, final`) and the port's
tree: `git diff 3078c10` in the `m2` worktree, which is the unaudited WIP commit `619ba34` (55 files) plus the
relaunch's report edits (the two commits between, `a78bf5f` and `7d4cc37`, are the harness's own STATE / workflow
edits and are not part of the port). Exe evidence: `ghidra/exports/cataclysm_gog_r6` (decompiled.c, functions.csv,
symbols.csv), the tables read from `original/game/cataclysm.exe` with a scratch PE reader (read-only; the game was
never run), the CET headers (`reference/CataclysmEngineTweaks/game/Game/universe.h`). Every engine run below went
through `tools/m2_harness.sh` (silent, headless, no input) with the audit's own binaries.

## 1. Clean builds in a separate directory

`rm -rf original/scratch-m2/build-audit-rest-oracle`, then `meson setup` / `meson compile` exactly as
`tools/build.sh` does (debug: `--buildtype=debug -Db_sanitize=address,undefined`; release: `-Doptimization=2
-Ddebug=false -Db_sanitize=none`; `werror=true` from meson.build):

| build | rc | `warning:` / `error:` | size | after the audit's fix (B-Spline.c) |
|---|---|---|---|---|
| debug (ASan + UBSan) | 0 | 0 / 0 | 80,164,432 B | 80,164,328 B, 0 warnings |
| release (-O2) | 0 | 0 / 0 | 4,934,904 B | 4,934,904 B, 0 warnings |

`tools/build.sh --debug` and `--release` in the worktree: rc 0, 0 warnings after the fix. `tools/m2_layout32.sh`: OK,
ILP32 sizes Player 25904 (0x6530), Universe 252348 (0x3D9BC), PlayerStats 2040 (0x7F8), TechStatics 31412 (0x7AB4),
shipinprogress 48 (0x30), 45 ship files' specifics asserts hold. `tools/oracle/dll/check_sites.py`: all checks passed
(the DLL itself was not rebuilt here: no Windows toolchain step in this audit).

## 2. The DONE WHEN, re-run on the plain harness with the audit binaries

`tools/oracle/mission_test.sh --bin <audit debug> --label r4ro-audit-mission --ticks 6000 --timeout 600 --perturb 3000
--copy-tick 4000`: **MISSION TEST PASS**.

| run | result |
|---|---|
| `r4ro-audit-mission-a`, `-b` (6,000 ticks, ASan + UBSan) | both "oracle mission run done" (ticks=6000 records=6003 ships=154), ASan 0, no unsuppressed leak, rc 0, 22 s wall each; `cmp` equal, 1,476,370 B, sha256 `4bfb8a436bbad1f5…`, the SAME sha256 as the producer's `r4ro-fin2-mission-a`; `hwo_compare.py`: IDENTICAL 6001 ticks and 0 packets |
| `r4ro-audit-mission-perturb` (`--perturb 3000`) | FIRST DIVERGENT TICK 3000, origin `rng10`, CAND 1 draw ahead, events `game-new, perturb, quit` |
| copy of A with bit 0 of `ships_health` flipped at tick 4000 | FIRST DIVERGENT TICK 4000, 1 field, origin `ships_health` |
| `r4ro-audit-mission-rel` (release -O2, 6,000 ticks) | done, rc 0; vs run A: IDENTICAL 6001 ticks (header build flags 1fbfa32f vs 1afeb7ac) |
| `r4ro-audit-fixed-mission-a`, `-b` (after the B-Spline fix) | see the last line of section 4 |

Self-tests, audit debug binary: fe 40 s **Main_game_screen**, ASan 0, 232 file loads; statics 143 of 145 types, 27
other .shp, ASan 0; meshes 1,758 listed, 1,729 .geo/.peo + 29 .mad, 0 bad; textures 12,403/12,403 lookups, 10,661
registered and loaded, 0 not loaded; oracle self-test 512 ticks, randSyncErr 0 univSyncErr 0 blobSyncErr 0, 772
records, 140,723 B; etg 197 effects, 618 created, ASan 0. Release oracle self-test a/b: `cmp` equal, sha256
`fffe906105ab086c`. All as the report states. The producer's `r4ro-fin2-sp` finished after the report was returned:
rc 124 at the 400 s timeout, `univUpdateCounter: 2256` at the end, ASan 0, no unsuppressed leak (the report's 1,584 was
the in-flight reading; annotated).

## 3. Layout (20 offsets) against the exe and CET

| check | evidence | result |
|---|---|---|
| Player stride 0x6530 | `statsGetStatChecksum` 0x00525170 and `PreLoadGame` 0x004fdfc0 step 0x6530 (0x194c dwords); `LoadConsMgr` `* 0x6530 + 0xb67500` | OK |
| +0x4 resourceUnits, +0xC supportUnitsMax, +0x2C totalships, +0x30 shiptotals (crc 0x244), +0x35C listoftopics.num | `statsGetStatChecksum`: `puVar5 = players[0]+0x35C`; reads -0x358, -0x350, -0x330, crc32(-0x32C, 0x244), +0 | OK (CET names +0xC/+0x10 `supportUnitsMax/Used`, asserts 0xC / 0x10) |
| +0x14 race, +0x1C aiPlayer, +0x24 playerIndex | `rmCanBuildShip` 0x004f2630 (+0x14, +0x1c), `allianceIsShipAlly` (+0x24 byte) | OK |
| +0x2A4 HasTechnology (42 sdwords, flag [0x29]) | `cat_rmTechSetAnd` 0x004f4070 `a[0x29]`; `rmInitializeResearchStruct` `cat_rmTechSetInit(player+0x2a4)`; +0x348 flag read in `cat_rmTechRequiredForShipUpgrade` | OK |
| +0x34C CanDoResearch, +0x350 techstat, +0x354 listoftopics, +0x360 researchlabs[5] | `rmInitializeResearchStruct` 0x004f3a00 (+0x34c, +0x350 = 0x00c282a0 / 0x00c18d20 / 0x00c207e0, 5 x {+0x360,+0x364}), `rmUpdateResearch` (+0x2EC+0x60/+0x68) | OK |
| +0x388 catShipUpgradeTechs (stride 0xA8), +0x62B0 catShipTypeEnabled | `cat_rmTechRequiredForShipUpgrade` `player + type*0xa8 + 0x388`; `cat_playerShipTypeEnabled` 0x004f4780 `+0x62b0 + type*4`; the Beast cases of `rmCanBuildShip` (+0x62b4 .. +0x62f4 = indices 1,4,7,8,6,0x11) | OK |
| +0x64F4 Allies | `allianceIsShipAlly` 0x00420ec0 `*(ushort*)(player+0x64f4)` | OK |
| +0x6500 catFOWGrid | `cat_playerGetFOWGrid` 0x00480d10; `universeInit` 0x0053de80 zeroes it for 8 players (< 0xba0380); `universeReset` likewise | OK |
| +0x6504 / +0x6508[9] / +0x652C upgrades | `cat_playerUpgradesReset` 0x00514100, `cat_playerUpdateUpgradesFromResearch` 0x00514140 | OK |
| Universe 0x3D9BC | `SaveUniverse` 0x005034c0 `memAllocFunction(0x3d9c4)` = 0x3D9BC + 8 | OK |
| universe+0xB8 catSensorsHoldRenderList | CET `unknown1` right before `phystimeelapsed` (asserted 0xBC); `smSensorsBegin` writes `universe.unknown1 = 1` | OK |
| +0x180 one tactics list (AttackAtoms) | `SaveRetreatAtom` 0x00503290: chunk 0x24 + 4n, n at atom+0x1C, ship at +0x10, ShipPtr[0] at +0x24 (the HW1 AttackAtom layout); CET keeps the name `RetreatList` | OK |
| +0x198 HousekeepShipList, +0x1A0 catSelection1A0, +0x1A8 star3dinfo .. +0x4B00 players | CET asserts 0x198 / 0x1A0 (`unk1`) / 0x1A8 / 0x1AC / 0x1BC / 0x2EA / 0x308 / 0x4AF0 / 0x4AF8 / 0x4B00; `GameStats.universeRUWorth` 0x328-0x308 = 0x20 | OK |
| PlayerStats 0x7F8: +0x28, +0x2A8[12], +0x7D0 | `initGameStats` 0x0053f100 (stride 0x1fe dwords; `psVar2[9]`, `psVar4[-0x9f]` of the class loop, `psVar2[499]`), `gameStatsAddShip` 0x0053f220 (+0x30, +0x34, +0x278, +0x2C), `clCreateShip` 0x0044a840 (+0x2a8), `cat_gameStatsPlayerRowFill` 0x00488c80 (row[1] = +2000, row[3] = +0x28) | OK |
| shipinprogress 0x30, +0x28, +0x2C; shipsinprogress 0x1B48 | `SaveConsMgr` 0x00459780 (chunk 0x1b48 = 0x18 + 0x91 x 0x30; step 0xc dwords; +0x38 info by ID pair, +0x44 ship by ID), `LoadConsMgr`, `unitCapCanCreateShip` (`progress + var*0x30`, `ppLVar8[10]` = +0x28); the tree's `shipsinprogress` has the 0x18 prefix (`canBuildBigShips`) | OK |
| TechStatics 0x7AB4: 0x5F28, 0x7A10 | `rmAllTechRequiredforTech` 0x004f2db0 `(tech + 0x91) * 0xa8`; `rmAssignPlayersLabToResearch` `+0x7a10 + tech*4`; `rmAPIStartup` clears 0x6ba dwords at table+0x5F28 | OK |
| ResearchTopic 0x18, techresearch +0x10, timeleft +0x14 | `rmAssignPlayersLabToResearch` 0x004f2960 | OK |
| Ship.health +0x170 (DLL `CAT_SHIP_health`) | `ApplyDamageToTarget` 0x00546250 is in functions.csv (1,204 B); the decompile's `+0x170` read was not re-derived here (the tree's `Ship.health` is still at 0x15C ILP32, as the report says; the recreation side hashes by name, so the stream is unaffected) | not re-checked (counts as neither) |

20 checks, 20 correct.

## 4. Tables and bodies against the exe

Tables read from `cataclysm.exe` (PE section map, read-only): `cat_techLabType` 0x00890328 (41 sdwords) equal;
`techtypeinfo` 0x008901a8 (41 names) equal; `cat_moduletypeinfo` 0x008902f8 equal; the time-compression table
0x0072bd14 (0.25 .. 8.0) and 0x0072bd10 (0.7) equal; `cat_statsKillRating` (60 rows x 5 reals at 0x008962d8, first
real) equal; `cat_statsClassFactor` 0x00897078, the 12 x 12 matrix 0x008970a8, `cat_statsCounterShip` 0x00896e2c
(indexed `[type][race]` as the exe's `race + type*2`), the 0x4C has-counter bytes 0x008961e8, `statsOverkillfactor` 1.0
equal; `cat_upgradeFlagTechTable` 0x0072b9e8 (25 x {tech, bit, extra}: (9,1,0) .. (20,0x4000000,0)), the weapon table
0x0072bb18 ({41,0},{18,1},{19,2},{2,3},{41,4},{20,5},{41,6},{27,7},{32,8}) and the armour table 0x0072bb60 equal; the 21
crate thresholds 0x0072a470 .. 0x0072a7b0 (0.01, 0.04, 0.1, 0.15 .. 0.95) equal and the per-race ship / technology
ladders of `cratesRewardPlayer` 0x00460ed0 match type for type (sDreadnought 10 .. sWorker 14; bHeavyCruiser 46 ..
bWorker 49) and technology for technology (the decompiler's HW1 enum names map to the Cataclysm indices: e.g.
`Chassis1` = 6 = TECH_REPAIR, `0x22` = TECH_CRYSTAL_HARVEST); `0x0072a4e0` = 2.3283064e-10 (2^-32).

Bodies compared with the r6 decompile (the exe's body on the left, the port's reading on the right):

| # | function | result |
|---|---|---|
| 1 | `cat_rmTechSetAnd` 0x004f4070 | match (both-ALL zero + flag 2; one-ALL copies the other's 41 entries, flag 0; AND, flag 0) |
| 2 | `rmCanBuildShip` 0x004f2630 | match (Beast-AI MP cases bAcolyte 0x22 .. bSentinel 0x34 -> enabled[1,4,7,8,6,0x11]; -2 flag; moon 0x85; Sect hangar test with class != 10 and `cat_playerHasCompletedShip(0x18)`; the curPlayerPtr sMothership (0x12) `COMMAND_MP_HYPERSPACING` (10) test; AND-compare) |
| 3 | `rmFindFreeLab` 0x004f2870 | match (Beast: lab 0 only; `tech == -1` or `cat_techLabType[tech] == lab`) |
| 4 | `rmUpdateResearch` 0x004f2a20 | match (8.0 in `tutorial == 1`, `cat_beastMothershipResearchFactor`, 1.0; the race's types `First <= i < Last` (the exe's `<`), Beast enabled test at +0x62B0; speech 0x2001033 / 0x20010bc / 0x2001034; `clCommandMessage` for the current player; Sect 5 labs / Beast lab 0 with `rmClearAllLabs`; `tmTechForSale = 2`) |
| 5 | `rmAssignPlayersLabToResearch` 0x004f2960, `rmClearResearchlab` 0x004f28f0 | match (Beast one topic; 0x18 zeroed topic; `LS_RESEARCHITEMSOON -> LS_NORESEARCHITEM`) |
| 6 | `rmAPIStartup` 0x004f3c00 / `rmResetStaticInfo` 0x004f3d30 | match (0x6ba dwords at each table's +0x5F28, 0x56a at the two Sect tables' start, 0x46e at Beast+0x15A8; `rmSetPrintList(1,3,4)`) |
| 7 | `rmInitializeResearchStruct` 0x004f3a00 | match (techstat choice; 5 labs cleared; Sect types 0..0x20 `cat_shipTypeGetUpgradeTechs`; Beast lab 0 open, 0x21..0x3A: case 0 TECH_ADVANCED_SENSORS (`RepairTech` = 28), 0xb TECH_CLOAKING, 0xd/0x19 TECH_INFECT, 0xe/0x14 CRYSTAL_HARVEST + INFECT, 0x10 TECH_SALVAGE then enabled, 0x12 enabled, default from the build row) |
| 8 | `rmResearchTechForShip` 0x004f3380 | match (Beast-enabled gate; closure, NOT has, AND, count; the second closure into `canget`; per technology the missing-dependency test, `rmFindFreeLab(player, tech)`, `clWrapResearch(0, playerIndex, lab, tech)`; the upgrade-set pass when the build set needs nothing) |
| 9 | `rmEnableShip` 0x004f3ca0 | match (`ship < 0x3c || ship == 0x85`; MP Sect table forced -2 for the moon) |
| 10 | `statsBestShipToBuyToKillShip` 0x00524cf0, `statsNumShipsNeededToKillTarget` 0x00524e40, `statsBestShipsToUseToKillTarget` 0x00524ee0 | match (type > 0x3b -> type 1 / 0x23; has-counter +3 for Beast targets; `ranRandomFn(6)` = RANDOM_AI_PLAYER `% 9 + 1`; the greedy pick, the `1/(1/x)` strength test, the un-freed selections on a non-standard best ship) |
| 11 | `tacticsGetShipsMaxVelocity` 0x00529450, `tacticsShutDown` 0x00529120 | match (command +0x16 & 1 = `COMMAND_MASK_FORMATION`, formationtype 5 = SPHERE, 0.7; flags 2 & 8 = Kamikaze + CrazyFast; 0x100 = SinglePlayerLimitSpeed; one `listDeleteAll`) |
| 12 | `calculatePlayerBounties` 0x00428070, `BountyInit` 0x00428050 | match (flags 0x800000 / 0x800 = SOF_Disabled / SOF_Dead; 8 players; floor 1.0) |
| 13 | `cratesRewardPlayer` 0x00460ed0 | match (ladders and thresholds above; `IF_InfoLoaded` test; up to 5 technology draws; `gamerand() % (max-min+1) + min`; the stats +0xC / +0x8) |
| 14 | `taskExecuteAllPending` 0x0052a840 and the 8 `cat_taskTimeCompression*` 0x0052a350-0x0052a4c0 | match (`(double)(ticks * factor + 1.5*2^52)`'s low dword = round to nearest even = `nearbyintf`; the unscaled real-time delta; max index 6, default 2; Increase/Decrease outside multiplayer) |
| 15 | `unitCapCanCreateShip` 0x0053ece0 | match (itemtype 1; race types `<= Last`; the +0x28 external-ship decrement; `shiplagtotals` once per test; sCarrierSupport (0xd) count = `cat_carrierCountSupportModules` + own queue + the queue's type count; class cap -1 skipped; speech 0x2001039 / 0x2001038) |
| 16 | `scriptSetShipsCB` 0x004a13f0 | match (race override swaps the type only when `RacesAllowedForGivenShip` lacks the race; `cat_shipInitResearchUpgrades` after each non-MiningBase create; `cat_rmResearchModuleActivated(player, type, 0)` for a Sect player; `clFormation(.., 0)` left as the CAT-TODO) |
| 17 | `cat_playerUpdateUpgradesFromResearch` 0x00514140, `cat_shipTypeGetUpgradeTechs` 0x005146f0, `cat_shipInitResearchUpgrades` 0x00514af0 | match (armour 2/1/0 from TECH_ARMOUR3/2; the weapon table with 0x29 = always; bits OR'd never cleared; the bit-index-vs-mask quirk kept; gun start/end upgrade level == 1 (WEAPON_ENERGY) with the 0x11C gun stride; the MP Sect row copy for an unenabled Beast type, tested on the Sect row as the exe) |
| 18 | `universeInit` 0x0053de80, `universeReset` 0x0053df80, `universeClosePlayers` 0x0053def0, `universeClose` 0x0053e3b0, `cat_playerGetFOWGrid` 0x00480d10, `cat_spWarpToLevelEnabled` 0x0051bd90 | match (8 grid pointers; the upgrade reset except player 0 under a warp; the time-compression reset first in universeClose and last in universeReset; the allied grid in multiplayer) |
| 19 | `allianceIsShipAlly` 0x00420ec0, `allianceSetAlliance` 0x00420a20, `bobBlobItemize` 0x004271d0, `cameraMinimumZoom` 0x0042b9a0, `ccGetShipCollCenter` 0x0042cf30, `hsStaticInit` 0x004901b0, `captaincyLog` 0x0042ec90 | match (crystal / objtype first; FOW off/on loops; `cat_shipIsCloakedFully`; Ship+0x18 bit 3 = `CAT_SHIPFLAG_Mimic` -> `cat_mimicGetCameraRadius`; no slave circle; free + zero; vsprintf + command message) |
| 20 | `gameStart` 0x005a9290 research level | match (human: 2 when the research-off global 0x00a75240 is set, else -1; computer: -1 in multiplayer or the tutorial, else 2; the global is unnamed in r6 and read here as the setup screen's "research off", the natural reading of HW1's `!MG_ResearchEnabled`) |
| 21 | `bsCurveUpdate` 0x00423040 | **WRONG**: the exe pins a LOCAL copy of the time (`fVar4 = prVar7[iVar5 + 1]` after `curve->timeElapsed = fVar4` was stored), the port wrote `curve->timeElapsed = curve->times[currentPoint + 1]`, so after a zero-length tensioned key the curve would have continued from the key's end instead of the real time (at most 0.034 s off, NIS / camera splines only). Fixed: `pinnedTime` local, `[audit:]` comment |
| 22 | `rmGiveTechToPlayerByType` 0x004f38e0 | ambiguous in the decompile: the downward scan's counter starts at 0x29 one entry past the set, so `missing[k]` may be given as `k` or `k+1`; the port reads it as `k` (the sane reading); not counted either way, noted in the report |

Fixed-build streams: `r4ro-audit-fixed-mission-a`/`-b` (6,000 ticks, the audit debug binary after the B-Spline fix):
MISSION TEST PASS (`--no-perturb-run --copy-tick 4000`): both done at tick 6000 (records 6003, ships 154), ASan 0,
`cmp` equal, 1,476,370 B, sha256 `4bfb8a436bbad1f5…` (the same stream as before the fix and as the producer's runs;
`hwo_compare.py` vs `r4ro-fin2-mission-a`: IDENTICAL 6001 ticks), the flipped copy caught at tick 4000 /
`ships_health`. The spline fix touches NIS / camera curves only, which the headless stream does not hash.

## 5. The diff (`git diff 3078c10`)

57 files: 44 `src/`, 7 `tools/`, 2 `ghidra/`, the report, `cycle/STATE.md` and `cycle/workflows/m2-r4.js` (the two
`cycle/` files are the harness's relaunch bookkeeping, commits `a78bf5f` / `7d4cc37`, not the producer's). Every `src/`
change is inside the scope named in the report (research, Player / Universe / GameStats / ConsMgr layouts, the oracle
mission mode, level load, the small files of "rest", and the callers the API changes forced: `ResearchGUI.c`,
`TradeMgr.c`, `ConsMgr.c`, `AIFleetMan.c`, `CommandLayer.c`, `UnivUpdate.c`, `SaveGame.c`, `KAS.c`). Nothing outside
the scope. The 139 function names the hunks touch were cross-checked against `ghidra/delta_map.json`: 59 are
HW1-matched; of those 10 are IDENTICAL in the map, 6 of them only as the hunk's context (`regRecursiveDisable`,
`taskStop`, `taskResumeAll`, `timTimerLoad`: the change is a comment or new code after them) or already reviewed
(`TechTypeToString`, `allianceArePlayersAllied`, `rmAPIShutdown`, `rmResearchingAnything`,
`statShipConstraintsFrigatesOrBetterCB`, `tacticsCheckGuardConditionsDuringAttack`: untouched bodies). Three edited
IDENTICAL functions had no review entry: `utyTasksDispatch` (HWI-ORACLE instrumentation), `PlayerAcquiredTechnology`
(the call adapted to the exe's one-technology `rmAddTechToPlayer`; the body is the exe's) and `tmTechInfoDraw` (the
exe reads a per-race description table, so it is MODIFIED). Entries added (sample `audit-R4-rest-oracle`), merged:
counts IDENTICAL 3,065 / MODIFIED 1,278 / REWRITTEN 77, reviewed 1,615 (were 3,066 / 1,277 / 77 / 1,612). CAT markers:
every Cataclysm change read carries a `CAT:` (or `CAT-TODO` / `CAT-PROVISIONAL` / `CAT-DORMANT`) comment with the exe
address; the oracle changes carry `HWI-ORACLE`; the `cat_hw1RetreatList` file-static and the `RetreatList` removal are
explained at the Universe.h field. No change without a marker found.

Tools: `tools/m2_stage_counts.py --md` reproduces the report's table (stage 2: 783 / 589 / 30 + 808 cat-new 180,868
B; stage 3: 929 / 262 / 7 + 450). `python3 -I tools/delta_map.py report` regenerates
`docs/m2-r1-function-deltas.md` byte for byte (no diff), so the calibration numbers are reproducible. The review file
holds 195 `port-R4` entries, as claimed; `done_subsystems` ends with `port-R4-rest-oracle`; `export` is r6.
`tools/oracle/dll/check_sites.py`: all checks passed. `tools/m2_harness.sh --mode mission` and
`tools/oracle/mission_test.sh` behave as documented (the PASS above).

## 6. Findings

| # | finding | evidence | fix |
|---|---|---|---|
| F1 (code) | `bsCurveUpdate` wrote the pinned key-end time into `curve->timeElapsed`; the exe pins a local | 0x00423040: `fVar4 = prVar7[iVar5 + 1]` after the store `curve->timeElapsed = fVar4` | `src/Game/B-Spline.c`: `pinnedTime` local, `[audit:]` comment; rebuilt (debug + release, 0 warnings), streams re-run; report row annotated |
| F2 (delta map) | three edited IDENTICAL functions without a review entry (`utyTasksDispatch`, `PlayerAcquiredTechnology`, `tmTechInfoDraw`) | the cross-check of section 5; the exe's `tmTechInfoDraw` 0x0052e1f0 uses `TechTypeToNiceString` and the string tables 0x00bb3a18 / 0x00bb3abc | `delta_map.py mark` + `merge`: 3 entries (sample `audit-R4-rest-oracle`), `tmTechInfoDraw` now MODIFIED; report annotated with the new totals |
| F3 (doc) | the report's `r4ro-fin2-sp` row gives the in-flight reading (1,584 ticks, no summary) | the finished `summary.txt` and `run.log`: rc 124 at 400 s, `univUpdateCounter: 2256`, ASan 0, no leak | report row annotated `[audit: ...]` |
| note (not counted) | `rmGiveTechToPlayerByType`'s dependency index is ambiguous in the decompile | 0x004f38e0's scan counter starts at 0x29 | report annotated, confidence medium |

Sampled checks: 20 layout offsets + 13 exe tables + 21 body comparisons (rows 1-21 above; row 22 not counted) + 6
DONE WHEN / harness claims (two identical 6,000-tick streams, the perturbed run, the flipped copy, the release stream,
fe + 5 self-tests unchanged) + 4 tool / delta-map claims (stage counts, function-deltas regeneration, 195 `port-R4`
entries, the unreviewed-IDENTICAL check) + 1 sp claim = **65 sampled, 3 wrong** (F1, F2, F3); precision 62/65 =
**0.954**. Fixes made in place: 3 (1 code, 1 delta map, 1 doc, plus the report annotations they needed).

Open, with owners (all already named in the report's "What is left"): the `Ship` prefix pin (`health` at 0x15C vs the
exe's 0x170), the stage-3 FOW grids, the research GUI / construction manager bodies, `clFormation`'s 4th argument,
the 97 unread IDENTICAL functions of FlightMan / Vector / Matrix / Volume / fastmath / Twiddle / Teams / Undo / Debug.

Verdict: PASS-WITH-FIXES - 62/65 checks match the exe and reproduce (20 Player/Universe/stats/research offsets, 13 exe tables, 20 of 21 bodies, two 6,000-tick Mission01 streams byte-identical with ASan 0 and the perturbed run and flipped copy caught at ticks 3000 / 4000, fe and the self-tests unchanged, clean builds 0 warnings); the audit fixed bsCurveUpdate's persisted key-end time (the exe pins a local), added the three missing review entries for edited IDENTICAL functions and recorded the finished sp run (2,256 ticks, ASan 0).
