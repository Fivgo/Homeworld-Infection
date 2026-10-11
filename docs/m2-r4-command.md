<!-- done: flags, orders, commandlayer, wrapformation, final -->
# M2-R4 producer 2: command layer + formations (`CommandLayer.c`, `CommandWrap.c`, `Formation.c`)

Status: **done** (subsystems `flags`, `orders`, `commandlayer`, `wrapformation`, `final`). Branch `m2`, worktree
`.claude/worktrees/m2`, nothing committed. Scope: docs/m2-r3-summary.md "Plan for M2-R4" row 2 and "What is left of
stage 2 part A" item 1.

**DONE WHEN, on the plain harness: met.**
- Every specialFlags/specialFlags2 bit is at the exe's value, with static checks: `_Static_assert`s in `SpaceObj.h`
  check that the live flags of each word are distinct and cover exactly the exe's used bits, and they pin the values
  M2-R3 quoted. No reader uses an old value: the four flags that changed words had their 18 reader lines moved;
  every other reader uses the names; no literal masks either word.
- sp runs as far as before with ASan 0: `r4c-final-sp` reaches universe running, univUpdateCounter **1,248** (R3 and
  r4u-final-sp: 1,248).
- fe is unchanged: `r4c-final-fe` reaches `Main_game_screen` with ASan 0.

The port itself is partial. `CommandLayer.c`, `CommandWrap.c` and `Formation.c` hold 87 / 65 / 1 functions in the
delta map (51,539 MODIFIED bytes), plus 41 cat-new. [audit: 87 / 65 / 1 are the IDENTICAL / MODIFIED /
REWRITTEN counts of the three files together (51+16+20 / 45+7+13 / 1+0+0 at HEAD), not per-file counts; the files
hold 97 / 23 / 33 mapped functions] This round ported:
- 10 MODIFIED bodies of `CommandLayer.c` (3 of them partly, with CAT-TODOs) and 6 of `CommandWrap.c`;
- 5 of the 6 bodies the map calls IDENTICAL that were found modified (the 6th, `FillInFormationSpecifics`, is
  reclassified only);
- 7 cat-new functions: `cat_shipHasJustBeenSalvaged`, `cat_clStartExternalBuildTimer`, `cat_clExternalBuildUpdate`,
  `cat_shipIsLiveWorker`, `cat_shipIsSalvager`, and the two external-build wraps;
- `clFormation`'s accepted types.

The rest is listed with exe addresses and known deltas in CAT-TODO blocks at the top of `CommandLayer.c` and
`Formation.c`, and in "What is left" below.

Files changed: `src/Game/SpaceObj.h`, `CommandDefs.h`, `FormationDefs.h`, `CommandLayer.c/.h`, `CommandWrap.c/.h`,
`Formation.c`, `ShipSelect.c`, `Salvage.c/.h`, `ConsMgr.h`, `UnivUpdate.c`, `Universe.c`, `Gun.c`, `KASFunc.c`,
`src/Ships/GravWellGenerator.c`, `SalCapCorvette.c`; `ghidra/delta_map_review.json`, `ghidra/delta_map.json`. New
tools: `tools/m2_specialflags_scan.py`, `tools/m2_fdis.py`, `tools/m2_fdec.py`, `tools/m2_exedata.py`. In `src/`: 18
files, +659 / -435 lines, 191 added lines carrying `CAT`.

## Subsystem `flags`: specialFlags / specialFlags2 at the exe's bits

**Method.** The exe keeps `Ship.specialFlags` at +0x1C8 and `specialFlags2` at +0x1CC (docs/m2-r3-selection.md;
`makeShipsControllable` reads `dockingship` at +0x1C4). The Ghidra export's `Ship` struct is empty and its typed
accesses go through HW1/CET layouts (`salvageNumTagged[0]`/`[1]` are the two words in many bodies), so the census reads
the disassembly instead: new `tools/m2_specialflags_scan.py` takes the R2 `objdump -d -M intel` listing
(`original/scratch-m2/m2r2-mesh-tools/exe.dis`) and lists, per exe function (`functions.csv` of r6), every masked
access to the two words: immediates, register immediates (`mov edi,0x40000000; or [eax+0x1c8],edi`), byte accesses at
+0x1C9..+0x1CB / +0x1CD..+0x1CF, and loads followed by `shr`/`test`/`and`/`or` on the loaded register (with `fnstsw`
treated as a reload, which removed one false hit). It joins the result with the `SPECIAL_*` names the same HW1
function uses in GoK at the bootstrap commit (`git archive 13723dd src`). Each bit below was then read by hand in the
named exe bodies (`tools/m2_fdis.py`, `tools/m2_fdec.py`, two small new helpers that print one function's
disassembly or decompilation). Five register-register sites the scan cannot resolve were read by hand
(`cat_collSentinelFieldExempt`, `singlePlayerGameUpdate` 0x0051a5a6 = clear 0x200080, `cat_clLinkCleanup`,
`FreeLastOrder`, `RemoveShipsFromDoingStuff` = clear sf2 0x40, `unitCapCreateShip` = 0x20000000).

**Result.** Cataclysm dropped HW1's AI resourcing pair (HW1 bits 14/15) and three HW1 salvage flags (21/23/24), added
a KAS repair modifier at bit 13 (which pushes the damage modifier to 14), and moved HW1's specialFlags2 bits 0-3
into specialFlags bits 27-30. specialFlags2 keeps only HW1's `DontPlowThroughEnemyShips` (now bit 0); the rest of
that word is Cataclysm's. HW1 flags with no exe access are CAT-DORMANT and defined 0, following the retired `SOF_*`
flags of M2-R3: setting one is a no-op and testing one is FALSE, which is what the exe does at those HW1 sites.

### specialFlags (exe Ship+0x1C8)

| bit | name | HW1 | exe evidence (set / test / clear) |
|---:|---|---|---|
| 0x1 | `SPECIAL_StopForResearchDocking` | 0x1 | tested by `cat_dockShipDocksAtShip` 0x00465860 (0x0046612f); no exe setter found |
| 0x2 | `SPECIAL_Kamikaze` | 0x2 | `processAttackToDo`, `delegateCommand`, `tacticsGetShipsMaxVelocity` 0x00529450 |
| 0x4 | `SPECIAL_BrokenFormation` | 0x4 | `processFormationToDo`, `AttackCleanup`; `MimicAttack` 0x005b8470, `MCVAttack`, `P1StandardCorvetteAttack` 0x005be6e0, `CruiseMissileAttack` set 0x6 (the "kamikaze bits 0x6" of ships-a 7 are Kamikaze \| BrokenFormation) |
| 0x8 | `SPECIAL_KamikazeCrazyFast` | 0x8 | `doKamikazeAttack` 0x005b1a50 |
| - | `SPECIAL_SpeedBurst`, `SPECIAL_SpeedBurstCooling` | 0x10, 0x20 | **retired (0)**: `tacticsGetShipsMaxVelocity` tests only 0x2, 0x8, 0x100 (HW1 also SpeedBurst/Cooling); no exe access to bits 4/5 |
| 0x40 | `SPECIAL_BurstFiring` | 0x40 | `doBurstFire` 0x005b5100, `cat_gunCreateBullet` 0x0048bb00 |
| 0x80 | `SPECIAL_SinglePlayerWithinHyperSpaceRange` | 0x80 | `CalculateRollCall`, `TellHyperspacingShipsToReady` 0x00517770, `spGoNowHyperspaceCB` 0x005173a0 |
| 0x100 | `SPECIAL_SinglePlayerLimitSpeed` | 0x100 | `kasfShipsSetMaxVelocity` 0x004938f0, `kasfTeamSetMaxVelocity` 0x00493860, `tacticsGetShipsMaxVelocity` |
| - | `SPECIAL_BUSY_BECAUSE_SALVAGE_DOCKING`, `SPECIAL_Harvested`, `SPECIAL_EXITING_DOOR` | 0x200, 0x400, 0x800 | **retired (0)**: no exe access to bits 9-11 |
| 0x1000 | `SPECIAL_STAY_TILL_EXPLICITLAUNCH` | 0x1000 | `InitializeDockVars` 0x00463c40, `ShipHasToLaunch` 0x0044a780, `RemoveShipFromLaunching` 0x00465590 and 12 more |
| 0x2000 | `SPECIAL_CatKasSpecialRepairModifier` (new) | - | `kasfShipsSetRepairFactor` 0x00493a10 sets, `kasfShipsClearRepairFactor` 0x00493a60 clears |
| 0x4000 | `SPECIAL_KasSpecialDamageModifier` | 0x2000 | `kasfShipsSetDamageFactor` 0x00493980, `gunShoot` 0x0048b5b0, `missileShoot` 0x0048aac0 |
| - | `SPECIAL_Finished_Resource`, `SPECIAL_Resourcing` | 0x4000, 0x8000 | **retired (0)**: the exe bits 14/15 are the damage modifier and the parade flag |
| 0x8000 | `SPECIAL_SinglePlayerInParade` | 0x10000 | `spGoNowHyperspaceCB` and `TellHyperspacingShipsToReady` set 0x8080 where HW1 sets WithinHyperSpaceRange \| InParade |
| 0x10000 | `SPECIAL_rowGettingOutOfWay` | 0x20000 | `rowFlyShipOutOfWay` 0x00414280, `rowRemoveShipFromGettingOutOfWay`, `PrepareShipsForCommand` 0x00441000, 24 more (the inlined `PrepareShipsForCommand` in each `cl*` function) |
| 0x20000 | `SPECIAL_ATTACKMOVECANCEL` | 0x40000 | `clProcess` 0x0044b950 sets (7 sites), `ChangeOrderToMove` 0x004460d0 tests |
| 0x40000 | `SPECIAL_FriendlyStatus` | 0x80000 | `kasfShipsSetNonRetaliation` 0x00495da0 / `kasfShipsSetRetaliation`, `selShipUnderMouse` 0x00506550, `getEnemiesWithinProximity`, `MakeTargetsOnlyEnemyShips` 0x00512970; `CrystalInit` 0x005b0040 and `SlipgateInit` 0x005c7250 set it |
| 0x80000 | `SPECIAL_CatSalvager` | - | `cat_salvageInit` 0x004f9010 sets; `collCheckShipShipColl` 0x0043ade0, `collCheckBigShipSmallShipColl`, `collCheckShipDerelictColl`, `aishipFlyToPointAvoidingObjsFunc` 0x00414610, `selStatusDraw0/1/2` 0x00507f10.. test it where HW1 tests `SPECIAL_IsASalvager` |
| - | `SPECIAL_IsASalvager` | 0x100000 | **retired (0)**: HW1's readers cast HW1's `SalCapCorvetteSpec`; the exe's sites read Cataclysm's salvage block (M2-R4 #3 ports them on `SPECIAL_CatSalvager`) |
| - | `SPECIAL_SalvagerHasSomethingAttachedAndIsDoingSomethingElse` | 0x200000 | **retired (0)** (HW1 salvage) |
| 0x100000 | `SPECIAL_AttackFromAbove` | 0x400000 | `attackSideStep` 0x00421fa0; `RemoveShipFromAttacking` 0x004477c0 clears 0x100006 where HW1 clears Kamikaze \| BrokenFormation \| AttackFromAbove |
| - | `SPECIAL_SalvagedTargetGoingIntoDockWithShip`, `SPECIAL_SalvageTakingHomeTechnology` | 0x800000, 0x1000000 | **retired (0)** (HW1 salvage; `selStatusDraw0` tests only 0x80000 where HW1 tests IsASalvager and SalvageTakingHomeTechnology) |
| 0x200000 | `SPECIAL_Hyperspacing` | 0x2000000 | `CalculateHyperspaceTimeForShip` 0x005178a0 sets, `UpdateArrivingShip` 0x00518170 clears, `attackPassive` 0x00421c30, `attackPassiveRotate`, `spHyperspaceSelectionOut`, `getEnemiesWithinProximity` test |
| 0x400000 | `SPECIAL_ForcedAttackStatus` | 0x4000000 | `kasfForceCombatStatus` 0x00493d70; `cat_univShipSelfRepair` 0x005433d0 reads it as "in combat" |
| 0x800000 | `SPECIAL_LaunchedFromKas` | 0x10000000 | `kasfLaunch` 0x004947c0 sets 0x1800000 (HW1: LaunchedFromKas \| KasCheckDoneLaunching), `aiplayerShipLaunchedCallback` 0x00411390 tests |
| 0x1000000 | `SPECIAL_KasCheckDoneLaunching` | 0x20000000 | `kasfLaunch`, `kasfShipsLaunch` set, `kasfTeamFinishedLaunching` 0x004948a0, `kasfShipsFinishedLaunching` test |
| 0x2000000 | `SPECIAL_Launching` | 0x40000000 | `clLaunchShip` 0x0044a220 tests (HW1 `clLaunchShip` tests Launching); no exe setter found (`dockPrepareSingleShipForLaunch` has no exe counterpart) |
| - | `SPECIAL_ParadeNeedTomoveCloser` | 0x80000000 | **retired (0)**: `processMilitaryParadeToDo` 0x00485280 touches neither word |
| 0x8000000 | `SPECIAL_SalvageTargetAlreadyDriven` | sf2 0x1 | `univSetupShipForControl` 0x0054a230 clears 0x8000000 first thing (0x0054a26d), where HW1 clears `SPECIAL_2_SalvageTargetAlreadyDriven` |
| 0x10000000 | `SPECIAL_ShipInGravwell` | sf2 0x2 | `GravWellGeneratorAddObj` 0x005b4570 sets, `GravWellGeneratorDied`/`RemoveShipReferences`/`HouseKeep` clear, `gunShoot` 0x0048b6e8 and `univBulletCollidedWithTarget` test where HW1 tests the specialFlags2 bit |
| 0x20000000 | `SPECIAL_InUnitCaps` | sf2 0x4 | `unitCapCreateShip` 0x0053eab0 (`mov ecx,0x20000000`), `unitCapDeleteShip` 0x0053ec00 tests and clears |
| 0x40000000 | `SPECIAL_CircularGuard` | sf2 0x8 | `clProtect` 0x0044af20 sets (`mov edi,0x40000000; or`) and clears (`0xbfffffff`), `protectShip` 0x004411c0 and `protectShipsAvg` test |

No exe access found to bits 4, 5, 9, 10, 11, 26 and 31.

### specialFlags2 (exe Ship+0x1CC)

| bit | name | exe evidence |
|---:|---|---|
| 0x1 | `SPECIAL_2_DontPlowThroughEnemyShips` (HW1 0x20) | `aishipFlyToPointAvoidingObjsFunc` 0x004151c5 tests it in the enemy-ship avoidance branch, where HW1 tests DontPlowThroughEnemyShips |
| 0x8 | `SPECIAL_2_CatBeingBuilt` | `univCreateShip` 0x00542320 sets (built externally), `cat_clExternalBuildUpdate` 0x0044dfc0 and `cat_shipUpdateExternalBuildAndAI` 0x00543f10 clear, `kasfShipUnderConstruction` 0x00493400 returns `>> 3 & 1`; about 60 readers (AI wraps, collision, docking, render, sound, selection) |
| 0x10 | `SPECIAL_2_CatKasAttackCapable` | `kasfShipsAttackCapable` 0x00498100 writes `(bCapable & 1) << 4`; no reader found |
| 0x40 | `SPECIAL_2_CatNoAvoidance` | `aishipFlyToPointAvoidingObjsFunc` 0x00414b00 skips all avoidance when set; set by `cat_dockShipDocksAtShip`, `kasfMakeShipCamera` 0x00498290, `cat_leechProcess` 0x005b6f30, `cat_salvageSpecialTarget` 0x004faad0, `cat_salvageDepotProcessDockable`, `RammingFrigateHouseKeep`; cleared by `RemoveShipFromDocking`, `RemoveShipFromLaunching`, `processLaunchShipToDo`, `LaunchCleanup`, `cat_clLinkCleanup` 0x00444060, `FreeLastOrder`, `RemoveShipsFromDoingStuff`, `cat_salvageDetach`, `cat_leechDetach`, `ModuleShipRemoveShipReferences` |
| 0x800 | `SPECIAL_2_CatHarvesting` | cleared by `processCollectResource` 0x004f2130 and `cat_rescollectStopHarvest` 0x004f2450; no setter found |
| 0x1000 | `SPECIAL_2_CatSentinelFormation` | `SentinelFormationCommand` 0x005c6df0 sets/clears |
| 0x2000 | `SPECIAL_2_CatBeingSalvaged` = `SPECIAL_2_CatSalvageDepot` | `cat_salvageIsBeingSalvaged` 0x004f7c00 (`shr 0xd; and 1`), `cat_salvageDepotInit` 0x004fc7c0 sets, `cat_salvageDepotCanAccept`, `cat_salvageFindNearestDepot` test |
| 0x4000 | `SPECIAL_2_CatParalyzed` | `kasfShipsParalyze` 0x00493420 writes `(paralyze & 1) << 14`; `cat_empCanAffectShip` 0x005add30, `cat_empPulse`, `ACVSpecialTarget`, `cat_empShockShip` test |
| 0x8000 | `SPECIAL_2_CatBeastInfectionUnlimited` | `kasfShipsSetBeastInfectionUnlimited` 0x00498a60, `cat_beastWeaponInit` 0x0042a040 |
| 0x10000 | `SPECIAL_2_CatBeastified` | `cat_univBeastInfectionUpdate` 0x00543860 sets; `kasfShipsSelectBeastified` 0x00498ae0 selects by it |
| 0x20000 | `SPECIAL_2_CatSalvaged` | `cat_salvageDepotProcessGeneric` 0x004f9500 sets; `kasfShipsSelectSalvaged` 0x00498b30 selects by it |
| 0x40000 | `SPECIAL_2_CatSalvagedFlag18` | `cat_salvageDepotProcessGeneric` writes it next to 0x20000 (set when the depot's +0x32C block word +0x24 is 0); meaning not pinned |
| 0x80000 | `SPECIAL_2_CatExternalBuildTimer` | `cat_clStartExternalBuildTimer` 0x0044aa30 sets, `cat_shipUpdateExternalBuildAndAI` clears, `ModuleShipDied` tests |
| 0x100000 | `SPECIAL_2_CatExempt20` | setter `cat_shipSetSUExempt` 0x005b4020 `(exempt & 1) << 20`; 27 readers (battle pings, speech, support units, launch, taskbar) |
| 0x200000 | `SPECIAL_2_CatKasBulimia` | `kasfShipsSetBulimia` 0x0049a030; `CollectResources` 0x004f17a0 tests |
| 0x400000 | `SPECIAL_2_CatKasNaggarok` | `kasfEnableNaggarok` 0x0049a220; `cat_rkoCanTarget` 0x005c4050, `RKOHouseKeep` test |
| 0x800000 | `SPECIAL_2_CatBeastWeaponAutoAttack` | `kasfEnableBeastWeaponAutoAttack` 0x0049a400; `cat_shipUpdateExternalBuildAndAI` tests |
| 0x1000000 | `SPECIAL_2_CatInvincibleInWidescreen` | `kasfShipsSetInvincibleInWidescreen` 0x0049abd0; `ApplyDamageToTarget` 0x00546250 tests |
| 0x2000000 | `SPECIAL_2_CatSalvageTargeted` | `cat_salvageSpecialTarget` sets, `cat_salvageDetach` 0x004f88b0 clears; `cat_shipIsTargetableBy` 0x0052a080, `cat_trailShouldDraw`, the EMP functions, `cat_processorAutoRepair` test |
| 0x4000000 | `SPECIAL_2_CatResourcing` | `processCollectResource` sets, `rescollectFindNearestResource` 0x004f11f0 tests/clears |
| 0x8000000 | `SPECIAL_2_CatStayMimicked` | `kasfSetStayMimicked` 0x0049a850 `(b & 1) << 27`; `cat_mimicCleanup` 0x005b8ea0 tests |
| 0x10000000 | `SPECIAL_2_CatSUProvided` | `cat_suShipProvidesSU` 0x00459a60 / `cat_suShipAdded` set, `cat_suShipRemoved` 0x00459ae0 tests/clears |
| 0x20000000 | `SPECIAL_2_CatSlipgate` | `SlipgateInit` 0x005c7250 sets; `ThisShipCanDockWith` 0x00464290 tests it on the dock target |
| - | `SPECIAL_2_NoMoreDockingAllowed`, `SPECIAL_2_BusiedDockPoint`, `SPECIAL_2_DisabledForever` (HW1 0x10, 0x40, 0x80) | **retired (0)**: JunkYardHQ docking and HW1 salvage only. The exe's `MothershipDied` 0x005bb9c0 tests `SPECIAL_2_CatBeingBuilt` on the list where HW1 tests DisabledForever (M2-R4 #4) |

No exe access found to bits 1, 2, 5, 7, 8, 9, 10, 30 and 31. The names of the new bits marked "name chosen here" in
`SpaceObj.h` come from their setters; the exe's own names are unknown.

### Static checks and readers

- `SpaceObj.h`: every flag is a hex constant with its exe evidence. `_Static_assert`s check that the 25 live
  specialFlags and 23 live specialFlags2 flags are distinct bits (their OR equals their sum), that they cover exactly
  the exe's used bits (0x7BFFF1CF and 0x3FFFF859), and the values the M2-R3 ports quoted (FriendlyStatus 0x40000,
  Hyperspacing 0x200000, CircularGuard 0x40000000, CatSalvager 0x80000, Kamikaze \| BrokenFormation 0x6; sf2 0x8,
  0x2000, 0x8000, 0x100000).
- The four flags that moved words changed their readers from `specialFlags2`/`SPECIAL_2_*` to
  `specialFlags`/`SPECIAL_*` (18 lines, each with a CAT comment): `CommandLayer.c` (circular guard: `protectShip`,
  `protectShipsAvg`, `clProtect`'s check), `Gun.c` (`gunShoot`), `UnivUpdate.c` (bullet hit, `univSetupShipForControl`),
  `Universe.c` (`unitCapCreateShip`/`unitCapDeleteShip`), `GravWellGenerator.c` (4), `SalCapCorvette.c` (2, dormant).
- Every other reader uses the names, so it now uses the exe's bit. No reader in `src/` (outside `src/reference`)
  masks either word with a literal.
- Readers of the retired names that remain (all now no-ops or FALSE, as in the exe): `Tactics.c` speed burst
  (M2-R4 #5), `Dock.c` exiting door / salvage / JunkYardHQ (M2-R4 #3), `Collision.c`, `AIShip.c`, `UnivUpdate.c`
  `univRemoveAllDerelictReferencesFromSpecialShips`, `CommandLayer.c` `RemoveShipFromSpecial` (HW1 salvage readers,
  M2-R4 #3), `SalCapCorvette.c`, `Carrier.c`, `Mothership.c` (M2-R4 #3/#4), `AIMoves.c`/`AIUtilities.c` resourcing
  (stage 4), `ResCollect.c` (M2-R4 #3), `Formation.c` parade (this port, `wrapformation`), `ResearchShip.c` (a dropped
  HW1 type, M2-R4 #4), `KASFunc.c` (stage 6), `mainrgn.c` `salCapCorvetteInSelection` (stage 3), `NetCheck.c` and
  `univGetChecksum`'s HW1 debug sum (stage 5).

Build after the subsystem: `tools/build.sh --debug`: `[221/221] Linking target hwi-engine`, 0 warnings.

## Subsystem `orders`: command orders 11-17 and formation types 9/10

| change | exe evidence | files |
|---|---|---|
| `FORCEFIELD_WALL_FORMATION` 9 and `FORCEFIELD_SPHERE_FORMATION` 10, the exe's own names | the exe's formation name table (`NumStrXlate` at 0x0087f998, read by `StrToTypeOfFormation` 0x00481190, dumped with the new `tools/m2_exedata.py numstr`): DELTA 0 ... SAME 8, `FORCEFIELD_WALL_FORMATION` 9, `FORCEFIELD_SPHERE_FORMATION` 10. `clFormation` 0x00484c80 and `FillInFormationSpecifics` 0x00484950 accept 0-6, 9, 10 | `FormationDefs.h` |
| the tree's `formationtypeinfo` table gains the two entries, in the exe's order; the "nice" table is unchanged (the exe's strings 0x00880a98..0x00880adc are HW1's seven) | as above | `Formation.c` |
| `COMMAND_CAT_LINK` 11, `COMMAND_CAT_UNLINK` 12, `COMMAND_CAT_13`, `COMMAND_CAT_POINT` 14, `COMMAND_CAT_MIMIC` 15, `COMMAND_CAT_RESEARCH` 16, `COMMAND_CAT_17`. The exe has no order-name table, so the names follow the creating function and the `clProcess` case | creators store the order word at CommandToDo+0x14: `cat_clLink` 0x0044d58b (0xb), `cat_clUnlink` 0x0044d9a3 (0xc), `cat_clPointOrder` 0x0044ddd3 (0xe), `cat_clMimic` 0x0044daf7 (0xf), `cat_clResearch` 0x0044dbfb (0x10); `clProcess` 0x0044b950 runs 11 `cat_processLinkToDo`, 12 `cat_processUnlinkToDo`, 14 `cat_bigGunProcessFireCommand`, 15 `cat_mimicCommand` per ship, 16 `cat_processResearchToDo`. 13 and 17: no store of either value to +0x14 anywhere in the disassembly (immediate or register) and no `clProcess` case; `cat_selMakeShipsProtectCapable` 0x00511f00 still lists 17 | `CommandDefs.h` |
| `ShipSelect.c`'s stand-ins `CAT_FORMATION_9/10` and `CAT_COMMAND_11..17` are gone; the filters use the names above | - | `ShipSelect.c` |

Build after the subsystem: `tools/build.sh --debug`: `[176/176] Linking target hwi-engine`, 0 warnings.

## Subsystem `commandlayer`: CommandLayer.c, external build, the HW1 calls with no exe counterpart

Each body was read in the r6 decompilation (`tools/m2_fdec.py`) and, where field offsets matter, the disassembly
(`tools/m2_fdis.py`); exe offsets are mapped to the tree's fields by name.

| function (exe) | Cataclysm delta ported | files |
|---|---|---|
| `protectShip` 0x004411c0 | capital range for `kGravWellGenerator`/`tGravWellGenerator` (types 64/79; no cloak generator); the follow-behind ships are `sProcessor`/`bProcessor` (16/51, HW1 ResourceController); no `DDDFrigateMakeSureItCanGuard`; CircularGuard read from specialFlags | `CommandLayer.c` |
| `protectShipsAvg` 0x004413f0 | capital range for 64/79/63/77 (both races' grav-well and cloak generators); the docking-with-me scan runs without HW1's `canReceiveSomething` gate; no DDDFrigate | `CommandLayer.c` |
| `ClearProtecting` 0x004447b0 (map: IDENTICAL) | a sphere needs >= 2 ships (HW1 > 0); a `FORCEFIELD_SPHERE_FORMATION` becomes `FORCEFIELD_WALL_FORMATION`, then `formationContentHasChanged` | `CommandLayer.c` |
| `shipHasJustCloaked` 0x004452f0, `shipHasJustDisappeared` 0x00445340 | 4-argument `RemoveShipFromBeingTargeted`; "can the current player still see it" is `cat_shipDetectedByPlayer` (sub_53F9F0, M2-R3) instead of `proximityCanPlayerSeeShip` | `CommandLayer.c` |
| `shipHasJustBeenDisabled` 0x00445360 | new second argument `catKeepInCamera` (the exe's two callers, 0x00429aa2 and `cat_salvageSpecialTarget` at 0x004fb6f7, pass 1); only the grav-well generators (64/79) have a disable hook; no DDDFrigate or cloak generator; the three `madLinkIn` closes run for every type (no SensorArray test). The two dormant HW1 `SalCapCorvette.c` callers pass FALSE (HW1 behaviour) | `CommandLayer.c/.h`, `SalCapCorvette.c` |
| `RemoveShipFromBeingTargeted` 0x004453c0 | rewritten from the exe body: 4th argument (a player) and the new flag `REMOVE_CAT_SALVAGED` 0x20 (only that player's commands); the hyperspacing pass falls through to the other checks (HW1 skipped them); docking removal by `dockship` only (no `busyingThisShip`, no AdvanceSupportFrigate case); the salvager checks use `cat_shipIsSalvager`, the salvager's target and `CustShipRemoveShipReferences` (slot 11); the cloak test is `cat_shipDetectedByPlayer` for the owner of the command's first ship; a disabled crystal stays an attack target; the special order calls `cat_salvageRemoveShipReferences` (not `salCapCleanUpCloakingTarget`); the parade around the ship only loses its protect/attack attributes (the inlined `ChangeOrderToHalt` skips the halt for a parade); unknown orders are ignored. Callers pass NULL where the exe passes 0 (`KASFunc.c` `SelectionSwitchPlayerOwner` call at 0x00493fe1, `clMpHyperspace` at 0x004475a3, `clShipDied` at 0x0044b4ee) | `CommandLayer.c/.h`, `KASFunc.c` |
| `cat_shipHasJustBeenSalvaged` 0x0044b450 (new) | `RemoveShipFromBeingTargeted(&mainCommandLayer, ship, 0x20, player)` | `CommandLayer.c/.h` |
| `cat_shipIsSalvager` 0x004f8f60 (new, salvage) | sWorker, bWorker, kSalCapCorvette, tSalCapCorvette, p1StandardCorvette (trJunkYardDawg reaches a compiled-out nullsub) | `Salvage.c/.h` |
| `cat_salvageGetTarget` (CAT-PROVISIONAL) | the salvager block's target (+0x04) that `RemoveShipFromBeingTargeted` reads; returns NULL until M2-R4 #3 lays out `CatSalvagerInfo` | `Salvage.c/.h` |
| `clScuttle` 0x0044a150 | in the tutorial the sMothership is not scuttled; CAT-TODO(#4): the catShipFlags 0x80 / Ship+0x4E4 exception and `cat_mimicUnmimic` | `CommandLayer.c` |
| `ShipHasToLaunch` 0x0044a780 | a ship in another player's factory always launches; swarmers (9, 43) never; the micro class (11) counts with fighters, live workers (`cat_shipIsLiveWorker` 0x005c8e00, new) with corvettes | `CommandLayer.c/.h` |
| `clCreateShip` 0x0044a840 | a carrier-class (`shipclass == 2`) player mothership is carrier 1; no ResearchShip / `rmActivateFreeLab`; CAT-TODO(#5): the per-class game stats; the exe's `built` is 5 (same bit 0 as the tree's 1) | `CommandLayer.c` |
| `clShipDied` 0x0044b470 | the ships of a parade around the dead ship leave it first (`cat_selSelectionCopy` of the parade selection through `RemoveShipsFromDoingStuff`); no `rmDeactivateLab`; CAT-TODO(#5): `cat_rmResearchModuleDied` 0x004f4620 (Sect) / `cat_rmMothershipLabLost` 0x004f4700 (bMothership) | `CommandLayer.c` |
| external build: `cat_clStartExternalBuildTimer` 0x0044aa30, `cat_clExternalBuildUpdate` 0x0044dfc0 (new) | sets `SPECIAL_2_CatExternalBuildTimer`; one build step: health += (progress - catBuildProgress) x maxhealth, clamped; mode 0 also ends the build (`SPECIAL_2_CatBeingBuilt` cleared, `cmDirtyShipInfo`; CAT-TODO(#5) research module for Sect), mode 1 deletes the ship (`univReallyDeleteThisShipRightNow`), mode 2 only advances. The signature is the exe wrap's (`cat_clWrapExternalBuildUpdate` 0x00453a00 passes cl, playerIndex, creator, ship, progress, mode) | `CommandLayer.c/.h`, `ConsMgr.h` (`cmDirtyShipInfo` declared) |
| `Ship.catBuildProgress` (exe Ship+0x3E8) and the external-build branch of `univCreateShip` 0x00542320 (0x00542ac4..0x00542b10) | an `externalBuild` type created with bit 0 of `built` set starts at health 0, progress 0 and `SPECIAL_2_CatBeingBuilt`; every other ship at full health, progress 1.0, flag clear (`built` is the 5th argument, `[esp+0x3c]` with a 0x28-byte frame). **CAT-PROVISIONAL `CAT_EXTERNAL_BUILD_DRIVER 0`** keeps the first case off: the driver (`cmDeterministicBuildProcess` 0x004592a0, `cmDeterministicBuildCancel` 0x00459040) is ConsMgr's and not ported, and the tree's ConsMgr still builds every type inside the factory and creates it at the end. With the branch on, an externally built type (destroyers, dreadnoughts, carriers, processors, mothership modules: 30 extracted `.shp` set `externalBuild TRUE`) would be created at health 0 and never finish | `SpaceObj.h`, `UnivUpdate.c`, `CommandLayer.h` |

Retired HW1 calls in this file (no exe counterpart at these sites): `DDDFrigateMakeSureItCanGuard` (2),
`DDDFrigateJustDisabled`, `cloakGeneratorJustDisabled`, `proximityCanPlayerSeeShip` (6), `salCapCleanUpCloakingTarget`,
`rmActivateFreeLab`, `rmDeactivateLab`. Still in the tree, CAT-TODO with their bodies (block comment at the top of
`CommandLayer.c`): `tacticsAttackCommandVoided` (`FreeCommandToDoContents`, `FreeLastOrder`), `MakeShipsMobile` and
`salCapExtraSpecialOrderCleanUp` (`clMove`, `clAttack`, `clSpecial`, `clHalt`, `clProtect`),
`dockPrepareSingleShipForLaunch`/`dockInitShipForLaunch` (`clLaunchShip`, which needs Dock's 3-argument
`InitializeDockVars`, M2-R4 #3), `RemoveShipReferencesFromExtraAttackInfo` (`RemoveShipsFromDoingStuff`).

Build after the subsystem: `tools/build.sh --debug`: 0 warnings.

## Subsystem `wrapformation`: CommandWrap.c and Formation.c

| function (exe) | Cataclysm delta ported | files |
|---|---|---|
| `clFlashShips` 0x00452960 | flash time is `cat_taskRealTimeElapsed` (real time); the objects clamped to a ship (its `ClampInfo` list at +0x34, read at 0x0045298a) flash with it. M2-R3's review called them docked ships; the disassembly reads `[ship+0x1bc]` (clampInfo), then `+0x34` | `CommandWrap.c` |
| `clWrapMove` 0x004529e0 | no `tutGameMessage`; HW1's `SpeechEventsForFuel` (no fuel in Cataclysm) is deleted: the exe keeps only an empty scan | `CommandWrap.c` |
| `clWrapMpHyperspace` 0x00452ae0 | the pause check allows orders with `opPauseOrders`; each ship gets `cat_shipSetDisableReason(ship, 1, 6)` instead of losing `SOF_Selectable` | `CommandWrap.c` |
| `clWrapDock` 0x00452d60 | `ioUpdateShipTotals()` for any dock type but retire, in place of `MakeSelectionNonResearchShips`; CAT-TODO(#3): Cataclysm's dock types are numbers (7 retire, 10) and the speech follows them | `CommandWrap.c` |
| `clWrapSpecial` 0x004531d0 | HW1's salvage-corvette block (speech, one-by-one orders) is gone; the exe's `cat_shipIsSalvager` pass over the selection has no effect, so nothing of it is kept; the copy is always freed | `CommandWrap.c` |
| `clWrapFormation` 0x00452ca0 | no formation check (clFormation checks); CAT-TODO: the 4th argument | `CommandWrap.c` |
| `clWrapCollectResource` 0x00453000, `clWrapScuttle` 0x00453390, `clWrapSetKamikaze` 0x00453820, `clWrapSetTactics` 0x00453500 (map: IDENTICAL) | the resource flashes in real time; Scuttle and SetKamikaze allow orders while paused with `opPauseOrders`; SetTactics does nothing for an empty selection | `CommandWrap.c` |
| `cat_clWrapStartExternalBuildTimer` 0x00453660, `cat_clWrapExternalBuildUpdate` 0x00453a00 (new) | the local path; CAT-TODO(stage 5): the network/record path (`clSendMisc` 11, `cat_clSendExternalBuildUpdatePacket` 0x004523a0) | `CommandWrap.c/.h` |
| `clCommandMessage` 0x004ac820 | CAT-TODO(stage 3): two arguments and Cataclysm's message scroller | `CommandWrap.c` |
| compared, unchanged | `clWrapAttack` 0x00452c00, `clWrapHalt` 0x00453300, `clWrapProtect` 0x004530a0, `clWrapSetMilitaryParade` 0x004538b0, `ClearPassiveAttacking` 0x00444780 | - |
| `clFormation` 0x00484c80 | accepts the exe's set: 0-6 and 9/10 (`NO`, `SAME`, `PARADE` are invalid; HW1 accepted 7 and 100). CAT-TODO: the 4th argument, the per-ship `CustShipFormationCommand` (slot 34) veto, the 12/24-ship caps, the passive-attack clear | `Formation.c` |
| the force-field formations | CAT-TODO block at the top of `Formation.c`: `FillInFormationSpecifics` (map: IDENTICAL; 4 arguments, level at +0x288), `FormationCalculateOffsets`, `SetFormationOffsetsForForceFieldWall`, `formationSpecificsSphere`, `createSphereStaticInfo`, `setFormationToDo`, `processFormationToDo`, `GetShipsTravelVel`, `processMilitaryParadeToDo` (no parade flag), `setMilitaryParade`, `paradeSetTweakables` | `Formation.c` |

Not one of these paths is reached by the start of sp's Mission01, so the harness milestone cannot move; the change is
checked by the build, the static asserts and the sp/fe runs staying where they were.

## Subsystem `final`: builds, harness, delta map

**Builds.**
- `tools/build.sh --debug --clean`: `[295/295] Linking target hwi-engine`, 77,343,360 B.
- `tools/build.sh --release --clean`: `[295/295]`, 4,838,944 B, sha256 `c7d23828f8300b89…`.
- Both builds: 0 `warning:` lines under `-Werror`.
- `tools/m2_layout32.sh`: OK. All ILP32 layout asserts hold, and so do the new flag asserts, which are not
  ILP32-only.

**Plain harness** (`tools/m2_harness.sh`, runs in `original/scratch-m2/m2r2-harness/runs/<label>/`). Each run used a
copy of the binary in `build/m2r4c/`, so rebuilding during a run could not change it.

| run | binary | mode | milestone | first stop | univUpdateCounter | ASan / leaks |
|---|---|---|---|---|---:|---|
| r4c-flags-sp | after `flags` | sp, 340 s | universe running | none: still running at 340 s | 1,360 | 0 / none |
| r4c-cl1-sp | after `commandlayer` + `wrapformation` code | sp, 340 s | universe running | none: still running at 340 s | 1,248 | 0 / none |
| **r4c-final-sp** | final (clean build) | sp, 340 s | **universe running** | none: still running at 340 s | **1,248** | **0 / none** |
| **r4c-final-fe** | final | fe, 40 s | **Main_game_screen** | none: still running at 40 s | | 0 / none |
| r4c-final-statics | final | statics | ship statics self-test done: 143 of 145 types (27 other `.shp`) | none (rc 0) | | 0 / none |
| r4c-final-etg | final | etg | etg effect self-test done: 618 created, 3,197 deleted, 1,093 left, peak 2,515 effects / 3,012 particle blocks (as R3 and r4u-final-etg) | none (rc 0) | | 0 / none |

The counter is the last `debugScreenshots univUpdateCounter` line at the timeout, so it depends on how long the
real-time animatic took. r4c-flags-sp's 1,360 is above R3's 1,248, not below it. UBSan: the same non-fatal
misaligned-member kinds as r4u-final-sp (`fescreen`, `etgdecision`, `PATCH`, `etgfunctioncall`, `mhlocalbinding`,
`etgconditional`, `etgbranch`, `fquant.c`), and no new kind.

**Delta-map reviews** (`ghidra/delta_map_review.json`, merged with `tools/delta_map.py merge --done command`): 33
entries, 27 of them new.
- Class changes, IDENTICAL to MODIFIED (6): `ClearProtecting` 0x004447b0, `clWrapCollectResource` 0x00453000,
  `clWrapScuttle` 0x00453390, `clWrapSetKamikaze` 0x00453820, `clWrapSetTactics` 0x00453500,
  `FillInFormationSpecifics` 0x00484950.
- Compared and confirmed IDENTICAL (6): `ClearPassiveAttacking`, `clWrapAttack`, `clWrapHalt`, `clWrapProtect`,
  `clWrapSetMilitaryParade`, `StrToTypeOfFormation` (its table is extended; the body is the same).
- MODIFIED with the delta and what was ported (18): the functions in the two tables above, and `clFormation`.
- Notes added to 3 existing entries: `univSetupShipForControl`, `unitCapCreateShip`, `unitCapDeleteShip`, for the
  moved flags.
- [audit: one edited function was missing: `GravWellGeneratorRemoveShipReferences` 0x005b4760 (map IDENTICAL,
  unreviewed; its flag clear moved words, and the exe clears specialFlags 0x10000000 at 0x005b479d). The audit added
  its review entry (MODIFIED) and merged it with `--done m2r4:command-audit`: the map is now 3,096 / 1,247 / 77,
  1,491 reviewed.]

Map totals went from 3,103 / 1,240 / 77 (reviewed 1,463) to **3,097 / 1,246 / 77 (reviewed 1,490)**. Not every
IDENTICAL body in scope was compared: of 51 + 16 + 20 = 87, these 12 were. The rest is listed under "What is left".

## What is left, and who owns it

- **This port's follow-up (M2-R4 command; scheduling is the integrate step's call):**
  - The CommandLayer.c MODIFIED bodies in the CAT-TODO block at the top of the file:
    - `RemoveShipsFromDoingStuff` with its reason codes;
    - `FreeCommandToDoContents` / `FreeLastOrder`;
    - the attack, move and hyperspace order families;
    - `clSpecial`, `clHalt*`, `clProtect`, `clSetMilitaryParade`, `clHoldingPattern`, `clProcess`, `clPostProcess`,
      `GroupShip`, `clRUTransfer`.
  - The orders 11-16 themselves (`cat_clLink`/`Unlink`/`Mimic`/`Research`/`PointOrder`/`UpgradeShips`, the
    `cat_process*ToDo` functions) and the other cat-new `cat_cl*` functions (about 34).
  - Formation.c's force-field formations: `clFormation`'s 4th argument, `FormationCommand` +0x288, the offsets, the
    Sentinel sphere.
  - The 75 IDENTICAL bodies of the three files not yet compared.
- **M2-R4 #3 (combat, docking, salvage):**
  - the `CatSalvagerInfo` layout behind `cat_salvageGetTarget` (CAT-PROVISIONAL, returns NULL);
  - `clLaunchShip` with the 3-argument `InitializeDockVars`;
  - Cataclysm's numeric dock types in `clWrapDock`;
  - the readers of the retired HW1 salvage flags (`Collision.c`, `AIShip.c`, `Dock.c`, `UnivUpdate.c`
    `univRemoveAllDerelictReferencesFromSpecialShips`, `RemoveShipFromSpecial`) on `SPECIAL_CatSalvager`;
  - `SPECIAL_EXITING_DOOR` in `Dock.c`.
- **M2-R4 #4 (ship code B):** `cat_mimicUnmimic` and the catShipFlags 0x80 / Ship+0x4E4 case in `clScuttle`;
  `cat_shipIsLiveWorker`'s home is `Worker.c`; `MothershipDied`'s `SPECIAL_2_CatBeingBuilt` test on its list; the
  `CustShipFormationCommand` slot users (`SentinelFormationCommand`).
- **M2-R4 #5 (the rest):**
  - ConsMgr's external-build driver. Once it lands, turn `CAT_EXTERNAL_BUILD_DRIVER` to 1.
  - [audit: `univDeleteDeadShip`'s `CAT-TODO(M2-R4 #2)` (UnivUpdate.c) named this port for the
    `SPECIAL_2_CatBeingBuilt` clear of a dead externally built ship (0x00547dab); the port left it neither done
    nor listed. The audit ported the clear and retargeted the remaining `cat_cmExternalBuildShipDied` 0x00455960
    call to this owner.]
  - The research-module calls (`clShipDied`, `cat_clExternalBuildUpdate`).
  - `clCreateShip`'s game stats.
  - `Tactics.c`'s speed-burst readers of the retired flags.
- **Stage 3:** `clCommandMessage` and the message scroller; dock speech ids. **Stage 4:** `AIMoves.c` and
  `AIUtilities.c` resourcing readers of the retired flags. **Stage 5:** the network paths of the external-build
  wraps; `NetCheck.c` and `univGetChecksum`'s use of the retired resourcing mask. **Stage 6:** `KASFunc.c`'s
  `SalvageTakingHomeTechnology` reader.

**Open questions:** none that need the user. Names chosen here for exe bits with unknown names:
- `SPECIAL_CatKasSpecialRepairModifier`;
- specialFlags2: `CatKasAttackCapable`, `CatNoAvoidance`, `CatHarvesting`, `CatSentinelFormation`, `CatParalyzed`,
  `CatBeastified`, `CatSalvaged`, `CatSalvagedFlag18`, `CatExternalBuildTimer`, `CatKasBulimia`, `CatKasNaggarok`,
  `CatBeastWeaponAutoAttack`, `CatInvincibleInWidescreen`, `CatSalvageTargeted`, `CatResourcing`,
  `CatStayMimicked`, `CatSUProvided`, `CatSlipgate`;
- orders `COMMAND_CAT_13` / `COMMAND_CAT_17` (no setter found).

They follow the setter functions' names.
