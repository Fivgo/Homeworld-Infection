<!-- done: layout, oracle, levelload, tools, rest, final -->
# M2-R4 producer 5: the rest of stage 2, the Player layout, oracle stage 2, tools

Status: **done** (subsystems in done-marker order: `layout`, `oracle`, `levelload`, `tools`, `rest`, `final`). Branch
`m2`, worktree `.claude/worktrees/m2`. The producer was cut off by a usage limit after `rest`; the harness committed its
55 edited files UNAUDITED as the WIP snapshot `619ba34` (2026-10-11 17:56), and the relaunched producer (this text's
`final`) resumed from that tree: it changed no `src/` or tool file, only this report, after confirming the WIP tree
builds and re-running every plain-harness run on it. Scope: docs/m2-r3-summary.md "Plan for
M2-R4" row 5 and "What is left of stage 2 part A" items 2 and 5. Exe evidence: `ghidra/exports/cataclysm_gog_r6`
(decompiled.c, functions.csv, symbols.csv, call_graph.csv), tables read from `original/game/cataclysm.exe` (read-only,
never run; a scratch PE reader, not kept) and the CET headers (`reference/CataclysmEngineTweaks/game/Game/universe.h`
asserts). Every Cataclysm change in `src/` carries a `CAT:` comment with the exe function and address; oracle
instrumentation carries `HWI-ORACLE:`.

**DONE WHEN: met** (judged on the WIP tree `619ba34`, clean debug build, plain harness, runs `r4ro-fin2-*`). Two
headless Mission01 oracle runs of **6,000 ticks** (ASan + UBSan) give byte-identical `.hwo` streams: 1,476,370 B, sha256
`4bfb8a436bbad1f5` (`cmp` equal; `hwo_compare.py` "IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000)"), ASan 0
in both; a run perturbed by one `gamerand()` before tick 3000 is reported at tick 3000 / `rng10` / "1 draw(s) ahead", and a
copy of run A with bit 0 of `ships_health` flipped at tick 4000 is reported at tick 4000 / `ships_health`
(`tools/oracle/mission_test.sh --ticks 6000 --perturb 3000 --copy-tick 4000`: MISSION TEST PASS). The 6,001 `T` records
are the same as the earlier producer's 6,000-tick pair (`r4ro-s2-mission-a`, built from `3078c10`: "IDENTICAL", only the
header's build identity differs) and as the **release (-O2) build's** run (`r4ro-fin2-mission-rel`: IDENTICAL, header
flags 1afeb7ac vs 1fbfa32f). `tools/m2_layout32.sh` holds `Player` 0x6530 and `Universe` 0x3D9BC. fe
(`Main_game_screen`), statics 143/145, meshes 1,758/1,758, textures 10,661 loaded / 0 not loaded, oracle self-test 512
ticks / 0 sync errors and the ETG self-test are unchanged; details in `final`.

## Subsystem `layout`: the Universe and Player layouts (part-A item 2) and the research port they need

The Cataclysm `Player` is 0x6530 bytes and the `Universe` 0x3D9BC (the `SaveUniverse` 0x005034c0 chunk, and
`cdLimitCaps` sits at 0x00ba03c0 right after `universe` 0x00b62a00). Pinning them meant porting the research layout
inside `Player` (0x6250 of the 0x6530 bytes), so `ResearchAPI.c` is part of this subsystem.

### The exe's layouts (all offsets ILP32, asserted under `__i386__`)

| struct | size | fields placed (offset: name, evidence) |
|---|---|---|
| `Player` | 0x6530 | 0x0 `PlayerMothership`, 0x4 `resourceUnits`, 0x8 `catUnknown08` (CET `unk`, no reader in r6), **0xC `supportUnitsMax`, 0x10 `supportUnitsUsed`** (CET; `statsGetStatChecksum` 0x00525170 adds +0xC), 0x14 `race`, 0x18 `playerState`, 0x1C `aiPlayer`, 0x20 `autoLaunch`, 0x24 `playerIndex`, 0x26 `sensorLevel`, 0x27 `bounty`, 0x28 `timeMoShipAttacked`, 0x2C `totalships`, 0x30 `shiptotals[145]`, 0x274 `classtotals[12]`, 0x2A4 `researchinfo` (below), **0x64F4 `Allies`** (`allianceIsShipAlly` 0x00420ec0 reads the ushort at +0x64f4), 0x64F6 `AllianceProposals`, 0x64F8 `AllianceProposalsTimeout`, 0x64FC `initialShipCost` (`gameStatsCalcShipCostTotals` 0x0053f500), **0x6500 `catFOWGrid`** (`cat_playerGetFOWGrid` 0x00480d10, `cat_fowCreatePlayerGrids` 0x004a38c0, `universeReset` 0x0053df80), **0x6504 `catArmourLevelCap`, 0x6508 `catWeaponAvailable[9]`, 0x652C `catUpgradeBits`** (`cat_playerUpgradesReset` 0x00514100, `cat_playerUpdateUpgradesFromResearch` 0x00514140) |
| `PlayerResearchInfo` | 0x6250 | 0x2A4 `HasTechnology` as a **42-sdword tech set** (`cat_rmTechSetAnd` 0x004f4070 tests `a[0x29]`), 0x34C `CanDoResearch`, 0x350 `techstat`, 0x354 `listoftopics` (`Researching` 0x004f25b0; `Save_PlayerResearchInfo` is called with Player+0x354), 0x360 `researchlabs[5]` (`rmInitializeResearchStruct` 0x004f3a00 clears 5 at +0x360/+0x364), 0x388 `catShipUpgradeTechs[145]` (`cat_rmTechRequiredForShipUpgrade` 0x004f30e0: `player + 0x388 + type * 0xa8`), 0x62B0 `catShipTypeEnabled[145]` (`cat_playerShipTypeEnabled` 0x004f4780) |
| `TechStatics` | 0x7AB4 | `TechNeededToBuildShip[145]` sets, `TechNeededToResearch[41]` at 0x5F28 (`rmAllTechRequiredforTech` 0x004f2db0: row `(0x91 + tech) * 0xa8`), `TimeToComplete[41]` at 0x7A10 (`rmAssignPlayersLabToResearch` 0x004f2960); the three tables 0x00c282a0 / 0x00c18d20 / 0x00c207e0 are 0x7AC0 apart |
| `ResearchTopic` | 0x18 | no `numlabsresearching` (`rmAssignPlayersLabToResearch` allocates 0x18 and copies 6 dwords) |
| `PlayerStats` | 0x7F8 | HW1's layout with 145 types plus `catStat28` (+0x28), `catBuiltForEachClass[12]` (+0x2A8, `clCreateShip` 0x0044a840) and `catStat7D0` (+0x7D0); `initGameStats` 0x0053f100 zeroes dwords 9, 0xa9.. and 0x1f3, `gameStatsAddShip` 0x0053f220 / `gameStatsShipDied` 0x0053f290 use the stride 0x1FE dwords; the two singles are stats-screen rows (`cat_gameStatsPlayerRowFill` 0x00488c80 reads +0x28 and +0x7D0) |
| `GameStats` | 0x47E0 | 10 dwords + 9 `PlayerStats`; `universeRUWorth` at 0x20 (CET) |
| `Universe` | 0x3D9BC | CET's asserts (phystimeelapsed 0xBC .. players 0x4B00) plus: **0xB8 `catSensorsHoldRenderList`** (CET `unknown1`: set by `smSensorsBegin` 0x0050cdb0, cleared by `smSensorsCloseForGood` 0x0050ba90 and `univUpdateReset` 0x0054acd0, tested by `universeUpdateTask` before `univUpdateRenderList`); **one tactics list at 0x180, `AttackMemory`** (its atoms are HW1 `AttackAtom`s: `SaveRetreatAtom` 0x00503290 saves 0x24 + 4n bytes with the retreater at +0x10 and the attacker `SelectCommand` at +0x1C; `tacticsShipDied` 0x00529150 walks it so; HW1's `RetreatList` is gone and CET's name for the field is misleading), `ResourceVolumeList` 0x18C, `HousekeepShipList` 0x198, **`catSelection1A0`** (CET `unk1`, a second grow-selection saved after HousekeepShipList and reset with it), `star3dinfo` 0x1A8, `univUpdateCounter` 0x1AC, `shipMaxUnits[145]` 0x1C8, `aiplayerEnemy` 0x2EA, `gameStats` 0x308, `DerelictTech` 0x4AE8, `numCratesInWorld` 0x4AF0, `numPlayers` 0x4AF8, `players[9]` 0x4B00, `world[3]` 0x3D9B0 |
| `shipinprogress` (ConsMgr.h) | 0x30 | + `catUnknown24`, `catExternalShipStarted` (+0x28, `unitCapCanCreateShip` 0x0053ece0), `catExternalShip` (+0x2C, `SaveConsMgr` 0x00459780 saves it by ID; `LoadConsMgr` 0x004598a0 fixes +0x20 and +0x2C of 0x91 entries) |

Names chosen here (no exe name): `catUnknown08`, `catSensorsHoldRenderList`, `catSelection1A0`, `catStat28`,
`catBuiltForEachClass`, `catStat7D0`, `catFOWGrid`, `catArmourLevelCap`, `catWeaponAvailable`, `catUpgradeBits`,
`catShipUpgradeTechs`, `catShipTypeEnabled`, `catUnknown24`, `catExternalShipStarted`, `catExternalShip`.

### Files changed

| file | change |
|---|---|
| `src/Game/Universe.h` | `Player` and `Universe` as above; 36 ILP32 asserts (`sizeof(Player) == 0x6530`, `sizeof(Universe) == 0x3D9BC`, the offsets of the table); `cat_playerGetFOWGrid` prototype; `AttackMemory` is the one tactics list |
| `src/Game/GameStats.h` | `PlayerStats` with the three additions and 6 asserts |
| `src/Game/ConsMgr.h` | `shipinprogress` 0x30 with 3 asserts |
| `src/Game/ResearchAPI.h` | **rewritten:** the 41-technology enum with the exe's names (`techtypeinfo` 0x008901a8: `TECH_CLOAKING` .. `TECH_DRIVE_CORVETTE`), `CatTechSet` (42 sdwords, flag index 41: 2 = all, -2 = disabled), `TechStatics`, `ResearchTopic` 0x18, `NUM_RESEARCHLABS` 5, `PlayerResearchInfo` 0x6250, `MODULE_*` lab ids (0x008902f8), `TECH_NOTECHNOLOGY` -1 / `TECH_ALLTECHNOLOGY` 2 (the techlevel of `cat_rmTechSetInit`), the new prototypes, 11 asserts. HW1's `TechToBit`, `AllTechnology`, `RM_Disabled`, `rmSetShipDependCB`/`rmSetTechDependCB`, `rmDeactivateLab`, `rmActivateFreeLab`, `rmNumTechRequiredForTech`, `rmTechRequiredForTech`, `rmAllTechRequredForShip`, `rmRemoveAllUnneededTech` are gone (no exe counterpart) |
| `src/Game/ResearchAPI.c` | **rewritten from the exe (0x004f2530-0x004f4790, 0x005146f0):** the 8 tech-set operations (`cat_rmTechSetAdd` 0x004f3fc0 .. `cat_rmTechSetNot` 0x004f4300), `rmCanBuildShip` with the 3rd argument (hangar check; the six Beast-AI multiplayer cases; the Nomad moon 0x85; the hyperspacing sMothership test on `universe.curPlayerPtr`, as the exe), `rmFindFreeLab(player, tech)` with the per-technology lab type (`cat_techLabType`, 0x00890328) and the Beast single lab, `rmClearResearchlab` without `numlabsresearching`, `rmAssignPlayersLabToResearch` (Beast: one topic at a time), `rmUpdateResearch` (1 s per tick, 8 in the tutorial, `cat_beastMothershipResearchFactor` for the Beast; speech 0x2001033/0x20010bc/0x2001034; `rmClearAllLabs`; `tmTechForSale`), `rmAllTechRequiredforTech(out, player, tech)` (accumulating set closure), `rmTechRequiredForShip` and `cat_rmTechRequiredForShipUpgrade` over one shared counter, `cat_rmAIResearchTech`, `rmResearchTechForShip` (through `clWrapResearch`; falls back to the upgrade technologies), `rmGiveTechToPlayerByType` (highest missing dependency first, upgrades refreshed each step [audit: the decompile's downward scan starts its counter at 0x29 one entry past the set, so whether the exe gives `missing[k]` as `k` or `k+1` is ambiguous in 0x004f38e0; the port reads it as `k`, confidence medium]), `rmInitializeResearchStatics` (3 tables, no `MG_CarrierIsMothership`), `rmInitializeResearchStruct` (Sect: `cat_shipTypeGetUpgradeTechs` for types 0..0x20; Beast: lab 0 open, per-type upgrade sets and enabled flags for 0x21..0x3A with the exe's seven special cases), `rmAPIStartup`/`rmResetStaticInfo` (the exe's partial clears), `rmEnableShip` (flag word 0 / -2; the moon always disabled in the MP Sect table), `cat_rmResearchModuleActivated` 0x004f4530 / `cat_rmResearchModuleDied` 0x004f4620 / `cat_rmMothershipLabLost` 0x004f4700 (the Sect modules are the labs: 0x13 -> 3, 0x15 -> 1, 0x16 -> 2, 0x17 -> 4, 0x18 -> 0), `cat_playerShipTypeEnabled`, the script callbacks (`cat_rsShipDependSetCB` 0x004f3dc0 / `cat_rsTechDependSetCB` 0x004f3ec0, one body) and the exe's 128-row `ResearchDepend.script` table (0x0088fb98: `SPRS*`, `MPRS*`, `MPRB*`; no `MPRBTECH_RESEARCH_CLEESAN_ATTACK`). The FOW refreshes around a set change (techs -1, 2, 28) walk the ships whose player has a grid; the grids are NULL until stage 3 (CAT-TODO) |
| `src/Game/ShipSelect.c`, `.h` | the provisional player-side upgrade readers now read the Player (`catArmourLevelCap`, `catWeaponAvailable[WEAPON_ENERGY]`, `catUpgradeBits`); new `cat_playerUpgradesReset` 0x00514100, `cat_playerUpdateUpgradesFromResearch` 0x00514140 (armour cap 2/1/0 from TECH_ARMOUR3/2; the 9 weapon types from `cat_weaponUpgradeTechTable` 0x0072bb18; the bits from the 25-row `cat_upgradeFlagTechTable` 0x0072b9e8, {tech, bit, extra}), `cat_shipTypeGetUpgradeTechs` 0x005146f0 (the exe compares the bit INDEX with the table's bit MASKS, so only bits 1, 2, 4 match; ported as is; TECH_ENERGY from a gun's start/end weapon; the armour technologies up to the type's maximum), `cat_shipInitResearchUpgrades` 0x00514af0 (levelload) |
| `src/Game/Universe.c` | `universeInit` clears the 8 FOW pointers (0x0053de80); `cat_playerGetFOWGrid`; `universeClosePlayers` resets each player's upgrades except player 0 under a warp (0x0053def0); `universeReset` clears the grids (fowDestroy is stage 3) and resets the time compression; `universeClose` starts with the time-compression reset (0x0053e3b0); `initGameStats` zeroes the three new stats |
| `src/Game/SinglePlayer.c`, `.h` | `cat_spWarpToLevelEnabled` 0x0051bd90 |
| `src/SDL/utility.c` | `gameStart`: `rmInitializeResearchStruct` gets the exe's techlevel (-1 or 2: a human gets everything when research is off, a computer when research is off or in a non-tutorial single-player game, 0x005a9290); the `rmAddTechToPlayer(techLevel)` pass is gone |
| `src/Game/Stats.c` | `statsGetStatChecksum`: +0xC is `supportUnitsMax`, +0x35C `listoftopics.num` (the two UNVERIFIED fields of docs/m2-r2-oracle.md are named) |
| `src/Game/Tactics.c` | the exe's one tactics list is `universe.AttackMemory`; HW1's retreat list lives in a file-static `cat_hw1RetreatList` (CAT-PROVISIONAL) with the retreat code that has no exe counterpart |
| `src/Game/SaveGame.c`, `src/Game/UnivUpdate.c` | the universe chunk saves/loads/inits the one list; `catSelection1A0` reset; `catSensorsHoldRenderList` cleared in `univUpdateReset` |
| `src/Game/Crates.c`, `TradeMgr.c`, `ResearchGUI.c`, `.h`, `ConsMgr.c`, `AIFleetMan.c`, `CommandLayer.c`, `UnivUpdate.c`, `LevelLoad.c` | callers of the changed API: tech sets instead of masks (`cat_rmTechSetHas`, two set helpers in ResearchGUI.c under CAT-TODO stage 3), `rmCanBuildShip(.., TRUE)` in the construction manager (0x00455410 passes 1), `rmFindFreeLab(player, TECH_NONE)` in the AI (0x00413070 passes -1), `RM_SPRS/MPRS/MPRB` print lists, the 41-entry `PlayerTechListStatic` (CAT-PROVISIONAL stage 3), the research-module calls at the three CAT-TODO sites (clExternalBuildUpdate DONE, clShipDied, the mothership module loop, scriptSetShipsCB) |
| `src/Game/Timer.c`, `.h` | `cat_taskRealTimeElapsed` moved to Task.c (it was a CAT-PROVISIONAL stand-in) |
| `tools/m2_layout32.sh` | includes Universe.h, GameStats.h, ResearchAPI.h, ConsMgr.h; prints the five new sizes (25904 252348 2040 31412 48) |

Checks: `tools/m2_layout32.sh` OK (all ILP32 asserts hold); debug build 0 warnings; sp on Mission01 still reaches
1,248 ticks with ASan 0 (run `r4ro-layout-sp`, judged from `run.log`: the harness summary was lost because the harness
script was edited while that run was executing).

## Subsystem `oracle`: health and Player fields in the checksum; the headless Mission01 run

- **Ship.health offset settled: +0x170.** `ApplyDamageToTarget` 0x00546250 reads/writes `[esi+0x170]` and clamps it to
  `ShipStaticInfo+0xA4` (`maxhealth`); the repair path and the target tests at decompiled.c 292678 / 398814 / 406887 do
  the same. CET's `buildProgress 0x170` is the same word: a Cataclysm ship under construction grows its health. The
  GOG DLL (`tools/oracle/dll/hook_sites.h`) marks `CAT_SHIP_health`, `CAT_PLAYER_resourceUnits` (+0x4) and
  `CAT_PLAYER_totalships` (+0x2C) VERIFIED and `hwi_oracle.c` now sets `HWO_TV_HEALTH | HWO_TV_PLAYERS` in every `T`
  record, so `ships_health` and `players` are compared between the two producers. Rebuilt with
  `tools/oracle/dll/build.sh --clean` (23,040 B / 5,120 B); `check_sites.py`: all checks passed. The recreation side
  already hashed `ship->health` by name. The tree's ILP32 `Ship.health` is at 0x15C (the HW1 prefix is 0x14 bytes short
  of the exe's before `health`); pinning the Ship prefix is left (below).
- **`/oracleMission <ticks>`** (`src/Game/OracleDump.c`, `main.c`, `utility.c`, `Animatic.c`; HWI-ORACLE): starts
  single player at once as `/debugScreenshots` does (`utyGameSystemsInit`), with none of its screenshots and no
  `singlePlayerNextLevel` loop; `animAviPlay` returns at once (the exe's `animBinkPlay` 0x00421860 has no such switch;
  the animatic is real time only); `utyTasksDispatch` advances the task clock by exactly one universe tick per render
  frame (`oracleVirtualFrameTicks` = `UNIVERSE_UPDATE_PERIOD * taskFrequency` = 15 of the 250 Hz task ticks, so the
  universe task runs once per frame and every per-frame draw of a render-side random stream is a function of the tick
  count); the run quits through `utyGameQuit` (the normal exit path) at the requested tick after an `E quit` record;
  "oracle: mission tick N" every 256 ticks, "oracle: mission run done ticks=N records=N ships=N" at the end. The
  `/oracleSelfTestPerturb` draw moved into `oracleUnivUpdate` so it works in both the self-test and a mission run
  (header option bit `perturbed` set in both). `/disableAVI` exists (`enableAVI`) but only gates GoK's intro movies,
  not the mission animatic, so the mission switch gates it itself.
- **Harness:** `tools/m2_harness.sh --mode mission [--mission-ticks N] [--perturb T]`, milestones "universe running"
  and "oracle mission run done", a `mission:` summary line. **Acceptance test:** `tools/oracle/mission_test.sh
  [--ticks N] [--timeout S] [--perturb T] [--copy-tick T] [--copy-field F]`: runs A and B (cmp + `hwo_compare.py`,
  ASan 0 required), run P (perturbed), and a flipped copy of A (default field `ships_health`); MISSION TEST PASS / FAIL.
- **Speed:** with the virtual frame clock a 3,000-tick ASan run takes about 18 s wall and a 6,000-tick run 22 s
  (the sp mode needs 340 s because the animatic plays in real time). 154 ships are live on Mission01 with
  `CAT_HW1_MISSION_SCRIPTS 0` (no KAS, no AI orders; they hold position, so the stream exercises the universe update,
  the collision blobs, the RNG streams and the ship hashes, not combat).
- **Save tick-0 compare:** stays the documented hook at `saveCatLoadHook` (`src/Game/SaveGame.c`); the user's saves
  come later. The mission mode gives the recreation-side `T` records it will be compared against.

## Subsystem `levelload`: `scriptSetShipsCB`'s open deltas and `unitCapCanCreateShip` (part-A item 5)

| function | exe | change |
|---|---|---|
| `scriptSetShipsCB` (`LevelLoad.c`) | 0x004a13f0 | no `MG_HarvestinEnabled` ResourceCollector gate and no `MG_CarrierIsMothership` swap (the exe has neither); the multiplayer race override swaps the type only when `RacesAllowedForGivenShip[type]` lacks the player's race; `cat_shipInitResearchUpgrades(ship)` after every `univAddShip` of a non-MiningBase ship; a Sect player's placed module opens its lab (`cat_rmResearchModuleActivated(player, type, FALSE)`, once after a single ship, per ship in the two loops). Still open: `clFormation`'s 4th argument (M2-R4 command: the tree's `clFormation` has 3) and the `cat_kasHashString` team match (stage 6: the exe hashes the label and keeps no text) |
| `cat_shipInitResearchUpgrades` (`ShipSelect.c`) | 0x00514af0 | the type's static upgrade bits (`ShipStaticInfo+0x450`, `unknown450`) from the technologies its build requires; a Beast ship of a type the player has not enabled first gets the multiplayer Sect row of its Sect equivalent copied into the player's table |
| `unitCapCanCreateShip` (`Universe.c`) | 0x0053ece0 | the full body: caps with the unit-caps option or in single player; queue items of type 1 only; the factory loop over the player's race's types, one job less for an externally built type whose job is already a ship (`shipinprogress.catExternalShipStarted`); `shiplagtotals[type]` added once to each test; `sCarrierSupport` counts `cat_carrierCountSupportModules` + the factory's own queue; the class cap skipped at -1; speech 0x2001039 / 0x2001038 |

## Subsystem `tools`

| file | change |
|---|---|
| `tools/delta_map.py` | `EXPORT` and the map's `export` string are `ghidra/exports/cataclysm_gog_r6`; new `--calib-samples` option of `report` (default `calibration`: only the reviews drawn as samples, the M2-R1 buckets `IDENTICAL/medium`, `uniform/...`, the M2-R2 `reread-R2` and the `audit-*` reclassifications, calibrate the estimate; `all` is the old behaviour; a comma list names samples); the report says which reviews it used |
| `tools/m2_stage_counts.py` | `EXPORT` r6; the prefix rule `^cat_(shipPrint|selStatus|selHealth)` puts the 21 UI functions in stage 3 |
| `ghidra/delta_map_review.json` | 125 review entries synced from `ghidra/delta_map.json` before the classify rerun (sample `port-R4`): 102 functions reviewed in the map with no review-file entry (R4 port reviews written straight into the map), and 23 whose class and change line had been set in the map with the reviewed flag unset (ships-b audit and R2 audit edits: `aiaPriorityShipsConstraints` ... `speechEventUnderAttack`). Without the sync a `classify` rerun would have reset them (it did, on the first pass; the entries were carried and re-merged) |
| `ghidra/delta_map.json` | regenerated by `python3 -I tools/delta_map.py classify` on r6, then `merge`: the per-function classes are exactly the pre-rerun ones (0 class diffs against `git show HEAD:ghidra/delta_map.json`), auto classes unchanged (the 3 r6 renames change no feature), `reviewed` 1,532 -> 1,555 (the 23 carried entries), export string r6; plus this round's review entries (`final`) |
| `docs/m2-r1-function-deltas.md` | regenerated with the sample filter: calibration from 1,171 sampled reviews (384 targeted port reviews excluded); IDENTICAL-row agreement 92.0% (958 / 80 / 3), MODIFIED 85.7%; the new IDENTICAL agreement estimate 92.4% (R3's worry was that the targeted reviews would pull it to about 80%) |

Stage counts after the move (`tools/m2_stage_counts.py --md`): stage 2 cat-new 829 -> **808** (180,868 B), stage 3
429 -> **450**. HW1-matched totals unchanged: 3,066 / 1,277 / 77.

## Subsystem `rest`: the small files

Every MODIFIED / REWRITTEN function of the scope's files was read against the exe; the result per file:

| file | exe deltas ported (function, address) | not changed, and why |
|---|---|---|
| `Bounties.c` | `BountyInit` 0x00428050 (clears a per-player worth array, stores the raw setting), `calculatePlayerBounties` 0x00428070 (worth = build cost of live, enabled ships + resource units, floor 1.0; no HW1 percentages, carriers' contents or human/computer averaging), `getPlayerBountyRender` 0x00428250 (the worth as an integer; the array read is the reading of the evidence, confidence medium); `getPlayerBountyWorthDeterm` deleted (no counterpart, no caller) | `bountyShipWasKilled` 0x00428130 IDENTICAL (`getBountyOnShip` inlined) |
| `Blobs.c` | `bobBlobItemize` 0x004271d0: `cat_shipIsCloakedFully` instead of `SOF_Cloaked` | `bobSubBlobListCreate` 0x00426510: the Drone skip was already gone (R2) |
| `Alliance.c` | `allianceIsShipAlly` 0x00420ec0 (crystals and non-ships are never allies), `allianceSetAlliance` 0x00420a20 (the FOW cubes off/on around the change; grids NULL until stage 3) | |
| `Crates.c` | `cratesRewardPlayer` 0x00460ed0: probabilities `gamerand() / 2^32`, the exe's per-race ship ladders (12 steps each, types and counts) and technology ladders (20 steps each, the double thresholds 0x0072a470..0x0072a7b0 read from the exe), up to 5 draws for a technology the player lacks, the RU reward; `getShipClass`/`getShipType` deleted (no counterpart), HW1's SHIP_PROBS tables stay as script keys | `cratesPlayerFoundACrate` 0x00460d60: speech ids and effects are stage 3 (CAT-TODO not added: the 3-argument `AddTargetToDeleteList` was already the combat port's) |
| `Task.c`, `.h` | **time compression** (0x0052a350-0x0052a4c0): `cat_timeCompressionFactor` (table 0x0072bd14: 1/4 .. 8, default index 2), the eight `cat_taskTimeCompression*` functions, `taskExecuteAllPending` 0x0052a840 scales the ticks (round to nearest, the exe's 1.5 x 2^52 trick) and keeps `cat_taskRealTimeDelta` / `cat_taskRealTimeElapsed` from the unscaled ticks; the resets in `universeReset`, `universeClose`, `animAviPlay`; HW1's `universeTurbo` stays only in the playback turbo path (R2) and GoK's key toggle (stage 3) | `taskStart`'s 4th argument (the name) is GoK's `taskStartName`; `taskCallBackInit`'s "BabyTasks" name likewise |
| `Tactics.c` | `tacticsShutDown` 0x00529120 (the attack memory only), `tacticsReportMove` 0x00529130 (an empty walk), `tacticsShipDied` 0x00529150 (attack memory only), `tacticsGetShipsMaxVelocity` 0x00529450 (sphere formation 0.7, both kamikaze flags, the single-player limiter; no speed burst, fuel or velocity bonus table) | `tacticsMakeShipsNotLookForOtherShips` 0x005291b0, `tacticsMaxDistToTarget` 0x00529250, `tacticsCheckGuardConditionsDuringAttack` 0x005292f0 IDENTICAL; `tacticsStartUp` 0x005290b0 zeroes fewer tables (the Cataclysm `TacticsInfo` is smaller) but the extra inits are harmless; the HW1-only retreat/dodge/flank code is CAT-DORMANT with its 7 remaining callers named |
| `Stats.c` | the fight code's static tables (kill rating per type 0x008962d8, class factor 0x00897078, 12 x 12 class matrix 0x008970a8, counter ship per type and race 0x00896e2c, the has-counter bytes 0x008961e8, all read from the exe) and `statShipConstraintsFightingShipsCB` 0x00524b30 / `..CarrierFightingShipsCB` 0x00524bd0 (with the exe's fall-through of the four frigate exceptions into the corvette test), `statsGetMostDangerousShipNonStatConstraints` 0x00524c70, `statsBestShipToBuyToKillShip` 0x00524cf0, `statsNumShipsNeededToKillTarget` 0x00524e40, `statsNumShipsNeededToKillFleet` 0x00524eb0, `statsGetRelativeFleetStrengths` 0x00524ed0, `statsBestShipsToUseToKillTarget` 0x00524ee0 | `SetupShipsForFight`, `statsShowFancyFight`, `statsGatherFightStats` (0x00524120 / 0x00524790 / 0x00525640): the developer fight-stats gatherer (`cat_statsGetPostCombatStats`, a 60 x 60 x 5 table at 0x00bb5140); not ported, no game path reaches them |
| `B-Spline.c` | `bsCurveUpdate` 0x00423040: a curve on its last key returns the key's value (HW1: REALlyBig), running past the end returns the last value, a zero-length key between tensioned keys pins the time [audit: the exe pins a LOCAL copy of the time (`fVar4`), not `curve->timeElapsed`; the port wrote the key's end back into the curve, so the next frame would have continued from the key's end instead of the real time. Fixed in the audit: `pinnedTime` local] | |
| `Captaincy.c` | `captaincyLog` 0x0042ec90: no debug echo, no captaincylog.txt | `TransferCaptaincyStateMachine` 0x0042f270: LAN (stage 5, nice-to-have) |
| `HS.c` | `hsStaticInit` 0x004901b0: no argument, frees the gate table (callers `KAS.c` updated) | `hsStaticGateRender` 0x00490240: render (`cat_hsGateRectangle`, stage 3) |
| `Camera.c`, `CameraCommand.c` | `cameraMinimumZoom` 0x0042b9a0 (a mimicking ship zooms to `cat_mimicGetCameraRadius`), `ccGetShipCollCenter` 0x0042cf30 (no slave/master circle) | `cameraControl` (key bindings `kbCommandKeyIsHit`), `ccFocusIsLegalToFocusOn` (`cat_fowCanPlayerSeeShip`), `NewSetFocusPoint`, `CameraChase` (`cat_mcResetRequest`), `ccFocusOnPlayersMothership`, `ccFocusGeneral`, `ccCopyCamera`, `ccFocusOnFleetSmooth`: camera/UI, stage 3 |
| `Key.c`, `Region.c`, `Memory.c` | CAT-TODO notes: `keyInit` 0x0049ae00 ends with `cat_keyNameStringsInit` (stage 3); `regRegionDelete` 0x004f0e10 dirties overlapping parents when the cursor is saved (stage 3 UI); `memStartup` 0x004ad5f0 seeds the fill patterns from `USERNAME`/`USERPROFILE` crc32 or `ranRandomFn(26)` (GoK's constants kept; only under MEM_VOLATILE_CLEARING) | `regStartup`, `regMoveLinkChild`, `memAllocFunctionSBH`, `memStringDupe`, `memReallocFunction`: inlined callees / the growth heaps GoK already has |
| `LinkedList.c`, `Eval.c`, `Transformer.c`, `StubTransform.c`, `Clouds.c`, `Nebulae.c` | | `listMoveNodeToHead`, `listMergeSort`, `listMergeSortGeneral`: `listAddNode` inlined, IDENTICAL; `factor`: the assert path string; `transStartup` / `kniTransFreeVertexLists`: the Katmai transform paths GoK replaced (render); `nebGo`, `ellipsoid_seq`, `cloudZapLoop`, `cloudUpdateSystem`, `ellipsoid_render`: the deltas against ad1bb82 (`NEB_VEL_*`, `DUSTCLOUD_RAN_MULTIPLIER`, the 7-argument `ApplyDamageToTarget`, `shSpecularColour`) are already in the GoK tree or came with the combat port |
| `FlightMan.c`, the math files, `Teams.c`, `Undo.c`, `Debug.c`, `Volume.c`, `Twiddle.c` | all IDENTICAL by the map; not read this round (97 functions): recorded under "What is left" | |

## Subsystem `final`: builds, runs, delta-map reviews, what is left

### Builds

| build | result |
|---|---|
| `tools/build.sh --debug --clean` (WIP tree `619ba34`, 2026-10-11 relaunch) | rc 0, 0 `warning:` / 0 `error:` under `-Werror` (ASan + UBSan); the incremental build before it was already up to date |
| `tools/build.sh --release --clean` (same tree) | rc 0, 0 `warning:` / 0 `error:` (-O2, no sanitizers) |
| `tools/m2_layout32.sh` | OK: ILP32 sizes ShipStaticInfo 1112, GunStatic 284, CustShipHeader 140, polygonobject 116, texreg 52, SENTENCELUT 16, ClampInfo 56, StaticCollInfo 112, AlternativeShips 168, MilitaryParadeCommand 260, ranstream 20, SaveChunk 8, particle 256, eboltSystem 36, **Player 25904 (0x6530), Universe 252348 (0x3D9BC), PlayerStats 2040 (0x7F8), TechStatics 31412 (0x7AB4), shipinprogress 48 (0x30)**; 45 ship files' specifics asserts hold |
| `tools/oracle/dll/build.sh --clean` | `hwi_oracle.dll` 23,040 B, `hwi_oracle_loader.exe` 5,120 B; `check_sites.py`: all checks passed |

### Plain-harness runs (WIP tree `619ba34`, `tools/m2_harness.sh`, offscreen + dummy audio, no input; logs in `original/scratch-m2/m2r2-harness/runs/r4ro-fin2-*`)

| run | mode / build | rc / wall | milestone | first stop | ASan / notes |
|---|---|---|---|---|---|
| `r4ro-fin2-mission-a`, `-b` | mission 6,000 ticks, debug | 0 / 19 s each | **oracle mission run done** (ticks=6000 records=6003 ships=154) | none (rc 0) | ASan 0 both; `cmp` equal, 1,476,370 B, sha256 `4bfb8a436bbad1f5`; `hwo_compare.py` IDENTICAL 6001 ticks |
| `r4ro-fin2-mission-perturb` | mission 6,000, `--perturb 3000`, debug | 0 | oracle mission run done | none | ASan 0; FIRST DIVERGENT TICK 3000, origin `rng10`, CAND 1 draw ahead; event records `game-new, perturb, quit` |
| `r4ro-fin2-mission-a/run.perturbed-copy.hwo` | copy of A, bit 0 of `ships_health` at tick 4000 | | | | FIRST DIVERGENT TICK 4000, 1 field, origin `ships_health` |
| `r4ro-fin2-mission-rel` | mission 6,000 ticks, **release** | 0 / 4.5 s | oracle mission run done | none | vs `r4ro-fin2-mission-a`: IDENTICAL 6001 ticks (header build flags 1afeb7ac vs 1fbfa32f) |
| `r4ro-fin2-sp` | sp (`/debugScreenshots`), `--timeout 400`, debug | still running when this report was returned (the run ends at the 400 s timeout, rc 124 as every sp run) [audit: finished: rc 124 at 400.5 s wall, `summary.txt` written] | **universe running**: the last `debugScreenshots univUpdateCounter` line read **1,584** (the R3 runs read 1,248 at a 340 s timeout; this run was given 400 s) [audit: the final `run.log` reads `univUpdateCounter: 2256`; the tick count is a function of the timeout, not of this port] | none in `run.log` so far [audit: none in the finished log either] | 0 `ERROR: AddressSanitizer` lines in `run.log` at 1,584 ticks [audit: 0 at the end too; `summary.txt`: ASan errors 0, no unsuppressed leak, 3,723 file loads]; the harness `summary.txt` is written at the timeout and was not yet present (the two earlier sp runs of this tree, `r4ro-s2-sp` and `r4ro-final-sp`, both: universe running, 1,248 ticks at 340 s, ASan 0, 3,723 file loads) |
| `r4ro-fin2-fe` | fe, `--timeout 40`, debug | 124 / 40 s (timeout) | **Main_game_screen** | none: still running at the timeout | ASan 0; 232 file loads |
| `r4ro-fin2-statics` | statics, debug | 0 / 12 s | ship statics self-test done | none | ASan 0; 143 of 145 ship types loaded, 2 not found (disabled) |
| `r4ro-fin2-meshes` | meshes, debug | 0 / 12 s | mesh self-test done | none | ASan 0; 1,758 listed, 1,729 .geo/.peo + 29 .mad loaded, 0 bad objects |
| `r4ro-fin2-textures` | textures, debug | 0 / 20 s | texture self-test done | none | ASan 0; 12,403/12,403 listing lookups, 10,661 registered and loaded, 0 not loaded |
| `r4ro-fin2-oracle` | oracle self-test 512 ticks, debug | 0 / 10 s | oracle self-test done | none | ASan 0; randSyncErr 0 univSyncErr 0 blobSyncErr 0, 772 records, 140,723 B |
| `r4ro-fin2-oracle-rel-a`, `-b`, `-perturb` | oracle self-test 512, **release** | 0 / 0.4 s each | oracle self-test done | none | a and b `cmp` equal (sha256 `fffe906105ab086c`); `--perturb 300` reported at tick 300 / packet 150, origin `rng10` |
| `r4ro-fin2-etg` | etg, debug | 0 / 12 s | etg effect self-test done | none | ASan 0; 197 effects loaded, 0 referenced but not loaded, 618 created |

Every debug run's only UBSan reports are the known non-fatal misaligned-member kinds of GoK's packed structs
(`fquant.c:33`, `fescreen`, `etgdecision`, `PATCH`, ...), as in R3.

The same suite ran twice in the cut-off session (`r4ro-s2-*` before the last `rest` edits and `r4ro-final-*`, 3,000-tick
missions, both from the tree later committed as `619ba34`; builds `3078c109`): the `r4ro-s2-mission-a` 6,000-tick stream
(1,476,370 B, sha256 `ef442a52b9454a35`) is IDENTICAL to `r4ro-fin2-mission-a` over all 6,001 `T` records, only the
header's build identity (`3078c109` vs `7d4cc37b`) differs, which is why the whole-file sha256 changed. Earlier pairs:
`r4ro-mission-a/b` (3,000 ticks, 738,370 B, sha256 `2fa84fe60298f1fa`), `r4ro-mission-probe` (300 ticks, 15 s wall).

### Delta map

70 review entries (sample `port-R4`), recorded with `tools/delta_map.py mark` and merged (`ghidra/delta_map_review.json`
holds 195 `port-R4` entries: the 125 synced under `tools` plus this round's 70; the map's `reviewed` count went 1,555 ->
1,612, the difference being the functions that already had a review); `done_subsystems` + `port-R4-rest-oracle`.
Nine IDENTICAL functions were found modified and are MODIFIED now: `taskExecuteAllPending`
0x0052a840 (time compression), `tacticsShipDied` 0x00529150 and `tacticsStartUp` 0x005290b0 (one tactics list),
`rmClearResearchlab` 0x004f28f0, `rmAssignPlayersLabToResearch` 0x004f2960, `rmAPIStartup` 0x004f3c00,
`rmResetStaticInfo` 0x004f3d30, `rmInitializeResearchStatics` 0x004f39b0 (tech sets, 3 tables), `initGameStats`
0x0053f100 (the three new stats). Nine MODIFIED functions are IDENTICAL by the R2 convention (inlined callees, a
name string, GoK's growth heaps): the three `listMergeSort*`, `factor`, `memAllocFunctionSBH`, `memStringDupe`,
`memReallocFunction`, `regStartup`, `regMoveLinkChild`. Totals after the round: **IDENTICAL 3,066 / MODIFIED 1,277 /
REWRITTEN 77, reviewed 1,612** (`counts` line refreshed by `merge`). [audit: three functions this port edited were
IDENTICAL in the map with no review entry; the audit added entries (sample `audit-R4-rest-oracle`): `utyTasksDispatch`
0x005acd10 (IDENTICAL; the edit is HWI-ORACLE instrumentation under `#if HWI_ORACLE`), `PlayerAcquiredTechnology`
0x0052ea10 (IDENTICAL; the call was adapted to the exe's one-technology `rmAddTechToPlayer`, the body is the exe's) and
`tmTechInfoDraw` 0x0052e1f0 (MODIFIED: the exe titles with `TechTypeToNiceString` and reads the description from a
per-race string table at 0x00bb3a18 / 0x00bb3abc, not `strGetString(tech + offset)`; stage 3). Totals now
**IDENTICAL 3,065 / MODIFIED 1,278 / REWRITTEN 77, reviewed 1,615**.] Stage 2 (`tools/m2_stage_counts.py --md`):
783 / 589 / 30 (245,967 MODIFIED B) + 808 cat-new. docs/m2-r1-function-deltas.md regenerated (the calibration section
now names its 1,171 sampled reviews).

### Counts

| item | value |
|---|---|
| tracked files changed (`git diff 3078c10..619ba34 --stat`) | 55 files, +5,027 / -2,660 lines (44 in `src/`, 7 in `tools/`, 2 in `ghidra/`, 2 docs); new: `docs/m2-r4-rest-oracle.md`, `tools/oracle/mission_test.sh`; this report is the only file the relaunch changed |
| `src/` lines added carrying `CAT` / `HWI-ORACLE` (`git diff 3078c10 -- src`) | 284 / 5 |
| exe functions ported or re-ported with addresses in the text | about 75 (ResearchAPI.c 36 incl. the 8 set operations and the 3 module functions; Stats.c 8; Tactics.c 4; Bounties.c 3; Alliance.c 2; Crates.c 1; Task.c 9; LevelLoad.c 1; Universe.c 6; ShipSelect.c 4; B-Spline, Captaincy, HS, Camera, CameraCommand 1 each; Blobs 1) |
| ILP32 asserts added | 36 (Universe.h) + 6 (GameStats.h) + 11 (ResearchAPI.h) + 3 (ConsMgr.h) = 56 |
| tables read from the exe | techtypeinfo (41), moduletypeinfo (5), cat_techLabType (41), the 128-row ResearchDepend script table, cat_upgradeFlagTechTable (25 x 3), cat_weaponUpgradeTech (9), cat_armourUpgradeTech (3), crate ladders (4 x 12/20), time-compression factors (7), kill ratings (60), class factor (12), class matrix (144), counter ships (120), has-counter bytes (76) |
| harness runs this round (`runs.txt`) | 46 lines with the `r4ro-` prefix (47 once `r4ro-fin2-sp` ends), 28 of them this relaunch's `r4ro-fin2-*` and the mission test's runs |

### What is left, and who owns it

- **The `Ship` prefix pin.** The tree's ILP32 `Ship.health` is at 0x15C, the exe's at 0x170: 0x14 bytes of the
  `SpaceObjRotImpTarg` prefix (between `posinfo` 0x58 and `health`) are not the exe's yet, and `Ship` is not pinned
  at 0x63C (CET asserts 40 of its offsets). The oracle hashes `health` by name on this side, so the stream is
  unaffected; the pin is the Ship header pass (owner: the next round's layout item, with the univupdate audit's
  "+0x18 word" request: `SpaceObjHeader.catHeader18` and `Ship.catShipFlags` should be one field).
- **Research GUI, construction manager, trade manager (stage 3):** the tech-set compile fixes in `ResearchGUI.c`
  (`cat_rmTechSetCovers`, the 41-entry `PlayerTechListStatic`, `rmSetPrintList`'s HW1 bodies), `rmCanBuildShip`'s 3rd
  argument in `cmCanBuildShipType` (the exe computes it), `cat_ioMessageAdd` and the message colours of
  `clCommandMessage`, `cat_taskTimeCompressionPrint`'s `%s` argument, the Accelerate/Decelerate_Time keys and
  `universeTurbo`'s key toggle, `cat_keyNameStringsInit`, `regRegionDelete`'s dirtying block, `hsStaticGateRender`,
  the 8 camera functions (`cat_fowCanPlayerSeeShip`, `kbCommandKeyIsHit`, `cat_mcResetRequest`),
  `cratesPlayerFoundACrate`'s speech and effects.
- **Stage 3 FOW:** `cat_fowCreatePlayerGrids` 0x004a38c0 (`levelInit`), `fowDestroy` in `universeReset`,
  `fowSelect` in `universeSwitchToPlayer`, `cat_shipAffectsFOW` / `cat_univShipFOWActivate` / `Deactivate` in the
  tech-set and alliance loops; `Player.catFOWGrid` stays NULL until then.
- **M2-R4 command (#2):** `clFormation`'s 4th argument (`scriptSetShipsCB`, `cratesRewardPlayer`, `SetupShipsForFight`
  pass 0); the 7 HW1 tactics callers (`tacticsUpdate`, `tacticsGlobalUpdate`, `tacticsDelegate*`,
  `tacticsAttackCommandVoided`, `tacticsPopUpSetUp`, `tacticsAreStrikeCraftInSelection`) in UnivUpdate.c /
  CommandLayer.c and the Cataclysm `TacticsInfo` layout (the exe's `tacticsStartUp` zeroes 0x244 dwords and sets 2
  reals; HW1's bonus tables are gone).
- **Stage 6 (KAS / missions):** `cat_kasHashString` team matching in `scriptSetShipsCB`; the static hyperspace gates
  of `hsStaticInit`; `singleHackResearchWarp`'s HW1 technology names (`SinglePlayer.c`, warp-level only; the strings
  no longer resolve).
- **Stage 5 (LAN):** `TransferCaptaincyStateMachine` 0x0042f270.
- **Developer fight statistics:** `SetupShipsForFight`, `statsShowFancyFight`, `statsGatherFightStats`,
  `cat_statsGetPostCombatStats` (not reachable in a game).
- **Memory.c:** the fill-pattern seeding of `memStartup` (only matters under MEM_VOLATILE_CLEARING).
- **Not read this round:** the 97 IDENTICAL functions of `FlightMan.c` (45), `Vector.c` (14), `Matrix.c` (20),
  `Volume.c` (4), `fastmath.c` (2), `Twiddle.c` (2), `Teams.c` (6), `Undo.c` (2), `Debug.c` (2); the convention asks
  for a comparison of every IDENTICAL body, and this round compared the IDENTICAL bodies of the files it touched
  (Tactics, Blobs, Bounties, Alliance, Crates, LinkedList, Eval, Memory, Region, Clouds, Nebulae, ResearchAPI) only.
- **The user's inputs:** the save tick-0 compare at `saveCatLoadHook` (the two saves); the first live oracle session
  (decision 10) now compares `ships_health` and `players` too.

### Open questions

None that need the user. Names chosen here for exe fields and tables without names are listed under `layout`
(`catUnknown08`, `catSensorsHoldRenderList`, `catSelection1A0`, `catStat28`, `catBuiltForEachClass`, `catStat7D0`,
`catUnknown24`, `catExternalShipStarted`, `catExternalShip` and the Player upgrade fields).
