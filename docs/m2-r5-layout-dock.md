<!-- done: layout, dock, experience, compares, final -->
# M2-R5 producer 1: layout pass, dock purposes, experience, flight/math compares

Status: **complete** (every subsystem done; see `final`). Branch `m2`, worktree `.claude/worktrees/m2`, base commit `1434b90`.
Subsystems (the done marker above lists the finished ones): `layout` (the exe's object layouts: `Ship` 0x63C,
`SpaceObj` +0x38 attributes, Bullet/Missile/Derelict/Asteroid), `dock` (Cataclysm dock purposes, `Dock.c` /
`LaunchMgr.c`, `/catDockTest`), `experience` (the experience level and the R5-1 leftovers), `compares` (the 97
IDENTICAL bodies of FlightMan.c and the math files), `final` (builds, harness runs, delta-map reviews, report).

## Subsystem `layout`: the exe's object layouts (`SpaceObj.h`)

Sources, all read in `ghidra/exports/cataclysm_gog_r6/decompiled.c`: `univCreateShip` 0x00542320 (the 0x63C-byte ship
and about 90 field stores, decoded against Ghidra's 0x80-byte `SpaceObj` typing: `obj[k].field` = `0x80 k` + the
field's offset), `univAddAsteroid` 0x00540a30, `univAddMissile` 0x00540c70, `univAddDerelictByStatInfo` 0x00541000,
`LoadSpaceObj` 0x00501660 (the per-type allocation sizes), `InitializeNavLights` 0x00542040, `kasfShipsAttributesSet`
0x004933d0, `collUpdateCollRectangle` 0x00439cb0, `aitrackStabilizeGuidance` 0x004183f0, `cat_dockShipDocksAtShip`
0x00465860, `cat_dockInitShipDockInfo` 0x00463bb0, plus CET's `spaceobj.h` (40 `ASSERT_OFFSET`s for `Ship`, two for
`Bullet`) [audit: CET's `spaceobj.h` has 66 active `ASSERT_OFFSET(Ship, ...)` lines (and 3 commented out); the tree holds all of them but three, where the exe contradicts CET: `tractorPosinfo` is at 0x408 (CET 0x400; `univCreateShip` zeroes +0x3F8, +0x400, +0x408..+0x414, +0x404 in HW1's order), CET's `beastSystemType` / `beastUnk` 0x650 / 0x654 lie past the exe's 0x63C allocation, and the float CET names `buildProgress` at 0x170 is `health` (`univCreateShip` stores `maxhealth` there)] and a grep of every raw `+ 0xNNN)` user of the slots whose meaning was unknown.

| struct | exe | what changed in the tree (ILP32 asserts in `SpaceObj.h`, run by `tools/m2_layout32.sh`) |
|---|---|---|
| `SpaceObjHeader` | 0x20 | `catHeader18` and `Ship.catShipFlags` are **one field `catFlags18`** at +0x18 (the `CAT_SHIPFLAG_*` bits and `CAT_OBJFLAG18_RemoveReferences` 0x100; 38 sites renamed; the `CAT_OBJ_FLAGS18` bridge in `UnivUpdate.c` is gone) |
| `SpaceObj` | 0x98 | the mission attribute word is a **dword at +0x38** (`kasfShipsAttributesSet` stores it whole; `cat_blastAffectShip` 0x00515d30 tests 0x10000/0x20000 there, the killer collision 0x200, the crystal option 0x40000), HW1's uword slot +0x36 is padding, `attributesParam` a sword at +0x3C (`univAddAsteroid` clears that word), `cameraDistanceSquared` +0x40 (CET); after `catMeshState`/`catEffectBlock` (+0x80/+0x84) four reserved dwords `catObjReserved88[4]`: the exe's `rotinfo` is at **+0x98** (`cat_dockShipDocksAtShip` and `TurnHarvestEffectOn` pass `ship+0x98` as the matrix), so `collInfo` is at +0xD4, `precise` +0x14C (`univCreateShip` stores the precise block there), `impactablelink` +0x160, **`health` +0x170**. No reader of +0x88..+0x94 on an object pointer was found; every creator zeroes them |
| `NonStats` | 0x40 | a 16th float `catMaxVelocity` (+0x3C = Ship +0x1B8): `univCreateShip`, `univAddMissile` and `univAddDerelictByStatInfo` store `staticheader.maxvelocity` there; `clampInfo` follows at +0x1BC |
| `ShipOrDerelict` | 0x238 | the shared prefix at the exe's offsets: `aistate` 0x1C0, `putOnDoor` 0x1C3, `dockingship` 0x1C4, `specialFlags` 0x1C8, `specialFlags2` 0x1CC, **new `catRetreating` 0x1D0** (set 1 by `battlePingEvaluate` 0x00428b70 with the retreat chatter, cleared by every new order: `PrepareOneShipForCommand`, `clHalt`, `ChangeOrderTo*`, `cat_clLink/Mimic/PointOrder`, `RemoveShipFromBeingTargeted`; name chosen), `catResourceFinished` 0x1D4, `catResourcing` 0x1D8, `catDisableReason[13]` 0x1DC, `catDisabled` 0x210, `salvageNumTagged[8]` 0x214 (both creators zero 8 dwords there), `navLightInfo` 0x234 (`InitializeNavLights` stores at +0x234 for ships and derelicts). The four `cat*` fields moved here from `Ship` (33 sites now go through `shipOrDerelict.`) |
| `Ship` | **0x63C** (pinned by the dock pass: `dockvars` is still HW1's 0x28 bytes until then, so the tree's `Ship` is 0x650 after this subsystem) | every field at the exe's offset (comments carry them): `catSalvagePlayer` 0x238, `shiplink` 0x23C ... `fuel` 0x334 (still stored from `maxfuel` at 0x005427c4; CET's `lastTimeRepaired` 0x334 is HW1's order shifted), `attackvars` 0x374, `rceffect` 0x38C, `holdingPatternPos` 0x3D0, `damageModifier` 0x3DC, **new `catDamageFactor` 0x3E0** (1.0 at creation; `kasfShipsSetDamageFactor` 0x00493980 stores the percentage; `gunShoot`, `missileShoot`, the Beast weapon scale by it; 39 exe users), `passiveAttackCancelTimer` 0x3E4 (`clHalt` stores it), the R4 fields 0x3E8..0x3F4, HW1's tractor block 0x3F8..0x414 and `hyperspacePing` 0x418 (zeroed by the exe in HW1's order; `processMpHyperspaceingToDo` reads +0x418), `soundevent` 0x448 (+0x45C `hyperspaceHandle`, CET), `gunInfo` 0x490, `dockvars` 0x494, `dockInfo` 0x4A8 (`cat_dockInitShipDockInfo`), `catCreator` 0x4AC, `catQueuedAttackTarget` 0x4B0, `shipsInsideMe` 0x4B4, `bindings` 0x4B8, `madBindings` 0x4BC, `catRepairDroids` 0x4C0 (the exe's `droidCreateArray` result; NULL until stage 3), `shipSinglePlayerGameInfo` 0x4C4, `lightning[2]` 0x4C8 (no exe reader), the Beast / salvage / helpless / drift / dissolve blocks at the offsets R4 recorded, `catUpgradeStart`/`End` 0x59C/0x5A0 (`cat_clUpgradeShips`), `catDockLatchHeading` 0x5A4, `catShipReserved5B0` (= 1 at creation, no reader), `catBuildTimer` 0x5B4 (CET `buildTimer`), `catProtectedSpeedLimit` 0x5C4 (`GetShipsTravelVel`), `catKillsByClass[12]` 0x5C8, **`catExperienceLevel` 0x5F8, `catExperienceMessageTime` 0x5FC**, 0x600..0x61C (R4), `catDockStateTime` 0x620 (`cat_dockShipDocksAtShip`), `catLinkTimer` 0x624 (`cat_processLinkToDo`), `catNavLightStatic` 0x634 (a copy of the static's `navlightStaticInfo`, 0x005423fc; no reader), `ShipSpecifics` 0x638. **Gone:** `slavelink`, `slaveinfo` (no slavery in the exe: `SlaveInfo`, `SF_*`, `MakeShipMastersIncludeSlaves` and its 14 UI/KAS calls, `addSlavesToDeleteList`, the slave branches of `Select.c`/`Collision.c`/`Tactical.c`/`AIShip.c`, `Save/Load/FixSlaveInfo`, Dock.c's `dockMakeMaster` family), `sizeofShipSpecifics` (the exe keeps the size only in the static) |
| `Missile` | 0x258 | the HW1 fields in HW1's order after the exe's prefix (`trail` 0x1C0, `missilelink` 0x1D0, `playerowner` 0x1F8, `missileID` 0x210, `colorScheme` 0x244), then `catMissileReserved248[3]` and `catMissileScale` 0x254 (the Select.c scale float R4 named) |
| `Bullet` | 0x164 | HW1's `damageFull` removed (its two readers were the debug net log; `Gun.c` no longer stores it); `catMoved` sits at +0x13C, `playerowner` +0x14C, the big-gun fields +0x150..+0x158; CET's `bulletType` 0x108 / `owner` 0x11C hold |
| `Derelict` | 0x294 | `derelictlink` 0x238, `derelictID` 0x248, `colorScheme` 0x24A, sound handles 0x250 (set -1 at 0x0054119b), `nextRandomTime` 0x258, `needNewLeader` 0x280, `creationTime` 0x28C, `derelicttype` 0x290: HW1's fields after the shared prefix, nothing added |
| resources | `Resource` prefix 0x198; `Asteroid` 0x1B4 (`asteroidtype` 0x198, `scaling` 0x19C, `catMimicShip` 0x1A4, four reserved dwords); `Nebula` 0x1A8, `GasCloud` 0x1A0, `DustCloud` 0x1A4 | sizes from the creators and `LoadSpaceObj` |

Other changes: `univCreateShip` initialises the new words as the exe does (`catKilledByPlayer` -1, `catShipReserved5B0` 1,
`catDamageFactor` 1.0, `catMaxVelocity`, `catNavLightStatic`); `univAddMissile` stores `catMaxVelocity`;
`tools/m2_layout32.sh` prints the five object sizes. One defect found on the way: `InitializeNavLights((Ship *)derelict)`
relied on the shared `navLightInfo` slot, and my first draft (which had moved it to +0x634) left a derelict's pointer
uninitialised (ASan SEGV in `univFreeDerelictContents` at quit); the exe's own `InitializeNavLights` writes +0x234,
which settled the question.

Medium-confidence readings (recorded as such): `catRetreating`'s meaning (its setter is the retreat chatter, its
clearers are the order functions); `catShipReserved5B0`, `catObjReserved88[4]`, `catMissileReserved248[3]` and the
Asteroid's reserved dwords have no exe reader found by the raw-offset grep; `catNavLightStatic` has no reader.

Runs (plain harness, silent): `tools/oracle/mission_test.sh --label r5ld-layout3-mission --ticks 6000 --perturb 3000
--copy-tick 4000`: runs a and b byte-identical (1,476,370 B, sha256 `6fba67c89e675a37`), **ASan 0** in each, the
perturbed run caught at tick 3000 / `rng10`, the flipped copy at 4000 / `ships_health`: **MISSION TEST PASS**. Against
R4's `r4-integrate-mission-a`: `hwo_compare.py` **IDENTICAL over all 6,001 T records** (only the header's build
identity differs), so the oracle's terms are unchanged by the layout. `tools/build.sh --debug` and `--release`: 0
warnings under `-Werror`. `tools/m2_layout32.sh`: OK (Ship 0x650 for now, Missile 0x258, Bullet 0x164, Derelict
0x294, Asteroid 0x1B4).

## Subsystem `dock`: the Cataclysm dock purposes (`Dock.c`, `Dock.h`, `LaunchMgr.c`, the callers), `/catDockTest`

**What Cataclysm does** (read in `decompiled.c`): a dock order carries a PURPOSE 0..10 instead of HW1's `DOCK_*` bitmask
(`Dock.h` `CAT_DOCK_*`, names chosen from the call sites: 0 support `kasfShipsDockSupportWith`/`mrObjectClick`/
`refuelRepairShips`/`cat_tacticsEvasiveRetreatToDock`, 1 stay `kasfShipsDockStay`/`spHyperspaceButtonPushed`, 2 launch
`clLaunchShip`/`kasfShipsLaunch`/`HiveFrigateHouseKeep`, 3 deposit `processCollectResource`/`LeechHouseKeep`/
`univCheckShipState`, 4 leech, 5 tow, 7 retire `mrRetire`/Mission 9's collectors, 8/9 the instant forms `aimProcessDock`/
`spGoNowHyperspaceCB`, 10 slipgate `cat_slipgateDockWaitingShips`/`mrDockingOrders`). `cat_dockGetDockClass` 0x00463d70
turns purpose + ship class/type/`externalBuild`/`catTypeFlags` into the dock point type (`DPT_*`) used on the host (its
0x4000 host flag is the slipgate, 0x1 / 0x200 the Worker / Leech transfer points). `ThisShipCanDockWith` 0x00464290
takes the exe's fourth argument (test the per-type busy counts) and tests, in the exe's order: the dRKO's three guests,
the dBeastMothership against player 0, the host's dock statics, the class, same type, `SOF_Dead`, the host's closed
bays (attribute 0x400, `CAT_ATTRIBUTES_DockBaysClosed`, admitted only through `cat_dockCanDockWithBusyHost`
0x00463a70), untargetable / disabled hosts (not for purpose 10), the Mission 6/7/15 exemption flag, docking with
non-allies (attribute 0x40000 `CAT_ATTRIBUTES_DockWithEnemies`, slipgates, crystals under tow), a host under
construction, the latch types' need for a `center` point, and the counts. The flight is one cone state machine for every
ship pair, `cat_dockShipDocksAtShip` 0x00465860 (3,395 B; `CAT_DOCKSTATE_*` 0..17), built on the M2-R4 cone helpers
plus `cat_dockFindNearestDockPoint` 0x00464660 (engineering fall-backs of an sMothership without its docking bay and of
the Ghostship, the sCarrier's single medium bay, the clamped `catDockDistance`), `cat_dockFindLaunchPoint` 0x004649b0,
`cat_dockFlyOutOfCone` 0x004650d0 (the ship is snapped onto the cone axis, flies to 4 x `flyawaydist`, the sCarrier's
door), `cat_dockFlyInFrontOfHost` 0x00465630 (100 units in front; x2 for the sMothership and Ghostship),
`cat_dockCanDockWithBusyHost`, `cat_dockHasFreeSlotForShip` 0x00464910 (for the construction manager, R6-4),
`cat_dockShipIsInsideHost` 0x00468f80, `cat_dockInitShipDockInfo` 0x00463bb0, and the REWRITTEN `dockReserveDockPoint`
0x00464610, `RemoveShipFromDocking` 0x00465360, `RemoveShipFromLaunching` 0x00465590. `clDock` 0x00466c70 (the
`CustShipDockCommand` slot filter, the in-place order change unless the ships are harvesting, `wasHarvesting` for
purposes 0/3/10 over a collect order, the all-same-host flag), `processDockToDo` 0x00466950, `cat_dockFinishedDocking`
0x004667f0 (the retire refund, Mission 9's KAS variable, `GroupShip` for a ship that flew out), `dockPutShipOutside`
0x00463350 (modules, unhide, the support modules' rotations by launch-point parity: pi about X for sCarrierSupport /
sMothershipSupport on odd points, pi about Y then +-7 degrees about Z then pi about X for bMothershipSupport),
`InitializeDockVars` 0x00463c40 (3 arguments), `dockChangeSingleShipToDock` 0x004644c0, `FindNearestShipToDockAt`
0x00465750 (purpose 2 finds the host whose inside list holds the ship), `ShipIsWaitingForSoftLaunch` 0x00467a60 and
`ShipIsRefuelingAtCarrierMother` 0x00467a90 (states 8/9), `DockCleanup` 0x00467970, `LaunchCleanup` 0x004679b0 are at
the exe's bodies. Constants read from `.rdata` with a small PE reader: 4.0 / 6.0 s dock-time bonuses (0x0072a530 /
0x0072a7f0), 1.5 s slipgate transit (0x0072a7ec), 100.0 (0x0072a464), 2.0 (0x00879298), pi as a double (0x0072a7d8),
+-0.1221730 (0x0072a7cc / 0x0072a7d0), 7.0 / 10.0 module-eject delay and duration (0x0087929c / 0x008792a0).

**Deleted**: 95 HW1-only functions of `Dock.c` (the per-type `ShipDocksAt*` / `LaunchShipFrom*` paths, the
`StaticDockInit` and launch-point tables, `dockFindNearestDockPoint`/`dockFindLaunchPoint*`, `dockBusyThisShip` /
`dockUnbusyThisShip`, `dockNeedRotationOveride` / `dockNeedOffsetOveride`, `dockShipRefuelsAtShip`, the cone helpers
`dockFlyToConeInsideAndAlign` ... `dockFlyToConeInside`, `dockGetAppropriateTypeOfDocking`,
`MakeShipsAbleToDockWithThisShip`, `allShipsDockingWithSameShip`, `ShipWithinDockRange`, `ThisShipIs`,
`shipRetireNeedsMothership`, `dockInitializeCustomFunctions`, `findFormationGuardingShip`, the slave trade), the 60 dock
tweakables and `DockTweaks` (none in the exe's strings; `StatScript.c`), HW1's `DOCK_*` bits, `AtResearchShipDockInfo`,
the `ASF_*` states, `SpecificDockVars`' seven HW1 words (now the exe's five: `dockship`, `catDockDistance`, `dockstate`,
`catDockPointIndex`, `catDockTime`), `DockInfo`'s busyness and per-point static pointers (now the exe's 0x24 + 4n block:
`catNumBusyOfType[0x23]`, `thisDockBusy[]`), `ShipStaticInfo`'s two dock callbacks (dormant words `catStaticDormant400/404`
at the exe's +0x400/+0x404, never set by `InitStatShipInfo`). `Dock.c` went from 5,247 to 3,098 lines (of which 482
are the self-test block, 2,617..3,098) [audit: the report said 2,710 / about 480; `git show 1434b90:src/Game/Dock.c | wc -l` and `wc -l`] [audit, second pass: the first audit's 3,093 / 443 were off too: `git show f59e2ef:src/Game/Dock.c | wc -l` is 3,098 and the self-test banner starts at line 2,617; 3,105 after this audit's `cat_dockGetDockClass`, refund-comment and CAT-FIX-marker edits].

**Callers converted to purposes** (each with the exe's value at that site): `CommandWrap.c` `clWrapDock` (the exe's body:
totals refresh for any purpose but 7, speech 0x400106a / 0x4000801 / 0x4000803 by purpose through `speechEventQueue`),
`CommandLayer.c` (`clLaunchShip` 0x0044a220 with the closed-bays attribute and the `CustShipLaunchCommand` slot;
`processLaunchShipToDo` 0x00466b00 with the ore-can armour ejection and no HW1 hyperspace waits;
`needToWaitForThisShip` 0x004435d0; the dock case of `clProcess` without HW1's fly-in-formation pre-flight; new
`cat_clHarvestNearestResource` 0x00449790; `RemoveShipsFromDoingStuff`'s reason codes 0x1F / 0xB left to the command
follow-up), `ResCollect.c` (purpose 3; `rescollectFindNearestResource`'s tail calls `cat_clHarvestNearestResource`),
`UnivUpdate.c` (`univCheckShipState` 3; `cat_dockInitShipDockInfo` in `univCreateShip`; the five dock words in the sync
checksum; the net log), `Tactics.c` 0, `SinglePlayer.c` 9 / 1, `KASFunc.c` (`kasfShipsDockSupportWith` 0,
`kasfShipsDockStay` 0x004946a0 with Mission 9's retire), `AIMoves.c` (`aimProcessDock` 0x00409350: stay -> 1, instant
+8; the HW1 health choice is gone), `AIUtilities.c` (0; the busyness term is gone), `mainrgn.c` / `mouse.c` (0; retire 7),
`HiveFrigate.c` (0 / 1 / 2 straight to `clDock`), `Slipgate.c` (10 with the 4th argument), `Leech.c` (3),
`RepairCorvette.c` (0), `Salvage.c` / `Carrier.c` / `Leech.c` (`thisDockBusy[i]`), `SaveGame.c` (the dock info as a plain
block, only `dockship` fixed up), `NetCheck.c`, `Universe.c`. `LaunchMgr.c`: new `cat_lmShipAutoLaunchOn` 0x0049fb00
(the player's `autoLaunch` bits for its mothership and three carriers; CAT-FIX: a four-slot array for
`FillInCarrierMothershipInfo`, which the exe's three-slot frame overruns). `AIShip.c`: `aishipFlyToPointAvoidingObjsFunc`
0x00413e40 also skips avoidance for `SPECIAL_2_CatNoAvoidance` (the exe's test; HW1's keyed on `dockingship`). `UnivUpdate.c`:
`cat_univClampResourceTimer` 0x00547530 (the missile's 5 s life clamp when its target dies; `univRemoveAllTargetReferencesFromMissiles`
calls it); `univCreateShip`'s `CustShipInit` runs after the mesh-animation bindings as the exe orders it (ModuleShipInit
starts an animation: creating an sMothership faulted); CAT-FIX: the ship's extra blocks start at an 8-byte boundary (the
exe-shaped Ship plus a specifics block is not always a multiple of 8 on LP64; `memFree` of the `madanim` block read no
cookie). `Attributes.h`: `CAT_ATTRIBUTES_DockBaysClosed` 0x400, `CAT_ATTRIBUTES_DockWithEnemies` 0x40000 (names chosen).
`SpaceObj.h`: `CAT_SHIPFLAG_Launching` BIT0 of the +0x18 word (name chosen).

**CAT-FIXes** (the exe would fault): [audit: removed from this list: `cat_dockGetDockClass` no longer returns -1 for a monster/component class launching; the exe's jump table 0x004641dc sends classes 9, 10 (and above 11) into the purpose-3 code at 0x00463fcf, which returns -1 unless the type flags carry 0x1 / 0x200, without a fault, so the tree now falls through as the exe does] `cat_dockHasFreeSlotForShip` / `cat_dockFindNearestDockPoint` /
`cat_dockFindLaunchPoint` refuse a -1 class before indexing, `cat_dockCanDockWithBusyHost` tests the point index,
`cat_dockFlyOutOfCone` reads the point only with a host, `InitializeDockVars` tests the host before the hive reset.
Medium-confidence readings: the retire refund's float (the decompile shows only its `__ftol`; HW1's
`TW_RETIRE_MONEYBACK_RATIO x buildCost` is used) [audit: confirmed, no longer medium: the disassembly at 0x0046685e is `fildl 0x238(staticinfo)` / `fmuls 0x00ba1fa8` / `__ftol`, and 0x00ba1fa8 is the variable of the tweak-table entry 0x008ad324 named "TW_RETIRE_MONEYBACK_RATIO"], the dock state names.

**Stand-ins replaced**: the CAT-PROVISIONAL dock stand-ins of `HiveFrigate.c` (purposes 0/1 and the never-issued 2),
`Slipgate.c`, `Leech.c`, `RepairCorvette.c`, `Tactics.c` and the `CAT-TODO(M2-R4 #3 follow-up, dock purposes)` of
`ResCollect.c` (both) and `CommandWrap.c` are gone; `grep -rn "dock purposes\|M2-R4 #3" src/` finds only one comment
(`UnivUpdate.c` 2617, an asteroid item of M2-R4 #5).

**The self-test** `/catDockTest <ticks>` (`Dock.c` `catDockSelfTestTick`, `main.c`, the per-tick hook in
`OracleDump.c`'s `oracleUnivUpdate`; harness mode `dock`, `--dock-ticks`, default 5,000): it rides on the headless
Mission01 run (virtual clock, no animatic, quits at the tick). The mission file's 154 ships are player 1's (the player's
Somtaaw fleet comes from the kas2c script, off under `CAT_HW1_MISSION_SCRIPTS 0`), so at tick 16 the test creates the
local player's fleet with `univAddShip` at the grid point inside 80% of the world sphere (radius 20,000 here) farthest
from the level's ships: an sMothership (75 dock points; made the player's mothership as the script does), an sProcessor,
three Workers, four Acolytes, two Recons, two ACVs; clears the auto-launch bits; orders the acolytes, recons and ACVs to
dock for support (`clDock` purpose 0, nearest host), one Worker to deposit a scripted 250 RUs (purpose 3) and one to
harvest (`cat_clHarvestNearestResource`). Checkpoint A fires once every fighter is inside (tick 1,250 in the run below):
each fighter is in a host's inside list, off the ShipList, in dock state 8, with no command and no host left in its
dock vars (the exe's `RemoveShipFromDocking` clears it when the command finishes); the depositor is back outside with an
empty hold and the player's RUs are 750 -> 1000; then every docked ship is launched (`clLaunchMultipleShips` ->
`clLaunchShip`, purpose 2). Checkpoint B (two ticks before the quit): the fighters are outside with no host and no dock
or launch command (they end in `GroupShip`'s parade order, as the exe's `cat_dockFinishedDocking` does). Lines:
`docktest: orders|created|census|ordered|tick|A|B|checkpoint|clLaunchShip|latch|link|FAIL|done ... result=PASS|FAIL`;
the harness prints the orders, A and done lines and the FAIL count. **Run `r5ld-dock15-dock`: rc 0, 19.1 s, milestone
`dock self-test done`, ASan 0, no leak, `done fighters=6 outside=6 corvettes=2 RUs=1000 result=PASS`, 0 FAIL lines.**
The two corvette-class ACVs are ordered, logged and launched if they got in but not judged: one docks like the fighters,
the other orbits its dock point at ~250 units (cone dot 0.1, speed 50-80) for minutes and latches only after the others
launch; the harvesting Worker reaches its asteroid and sits in its jack state (its mesh animation never reaches the
paused end the exe's three-argument `madAnimationStart` mode gives; a stage-3 CAT-TODO in `Worker.c`), so it is traced.
Both are open questions below. Earlier runs of the series (`r5ld-dock1..14`) found and fixed: the empty universe
(no player ships without KAS), the world-boundary push-back of a fleet placed outside `universe.radius`, the
`CustShipInit` order, the `madanim` alignment, the `FillInCarrierMothershipInfo` overrun, the avoidance flag, the
mothership designation, and the test's own expectations and pacing.

**Mission stream**: `tools/oracle/mission_test.sh --label r5ld-dock1-mission --ticks 6000 --perturb 3000 --copy-tick
4000` after the dock port: runs a and b byte-identical (sha256 `6fba67c89e675a37`, the same as after the layout pass),
ASan 0, the perturbed run and the flipped copy caught. `tools/m2_layout32.sh`: OK with **`Ship` 0x63C** (the
`dockvars` asserts 0x494..0x4A4 and every field after them, `ShipSpecifics` 0x638).

## Subsystem `experience`: the experience level, the upgrade queries, the R5-1 leftovers

**The experience level** (`ShipSelect.c`): `cat_shipUpdateExperienceLevel` 0x005147d0 is ported from its decompile and
disassembly: the ship's kills per class (`Ship+0x5C8`) weighted by the victim's class (`cat_experienceKillWeights`
0x0072bbb4: 20, 8, 10, 9, 5, 3, 1 x 6) against the four thresholds of the ship's own class (`cat_experienceThresholds`
0x0072bbe4, 12 x 4: 10/30/60/100 for the capital classes, 5/10/20/40 destroyers, 3/6/12/16 frigates, 2/4/6/8
corvettes, 1/2/3/4 fighters, 2/4/6/8 monsters and micro) give the level 0..4 (`Ship+0x5F8`); passing a threshold with
the ExperienceMessages option on, outside the tutorial, for the local player's ship that is not disabled, formats
`"%s %s %s"` (0x00893c40) from `ShipTypeToNiceStr`, string 0x605 `strShipLevelUpNotify` and string 0x5F5+level
`strExperienceLevel1..4` (the exe's table 0x0072bba4) and sets `catExperienceMessageTime` = `realtimeelapsed` + 2.0
(0x0072a6c0). The option is the global `cat_opExperienceMessages` (the exe's 0x00a5e248, 0 after its resets;
`feToggleButtonSet OP_ExperienceMessages` is the front end's, CAT-TODO(M2-R5 hud)); the `cat_ioMessageAdd` 0x004ac940
call is a CAT-TODO(M2-R5 hud) too (the message is formatted and timed as the exe does). Callers wired: `ApplyDamageToTarget`
(after the killer's `catKillsByClass++`, swarmers credited to their hive), `univCreateShip` (the kills zeroed and one
call; the exe calls it once per class in its zeroing loop, same result). The exe's other callers are
`cat_processLinkToDo` / `cat_processUnlinkToDo` (orders 11-17, not in this tree: M2-R5 command-tactics) and the console's
`cat_consoleExperienceShips` 0x0054d1c0 (stage 3). The CAT-PROVISIONAL `cat_shipExperienceLevel` stand-in is gone:
`cat_shipGetSpeedMultiplier` 0x00514940 and `cat_shipGetDamageMultiplier` 0x005148f0 read `ship->catExperienceLevel`.
`clCreateShip` 0x0044a840 counts the built ship's class in `PlayerStats.catBuiltForEachClass` (+0x2A8; swarmers
skipped; sACV, sMCV and bACV twice, as the exe repeats the increment).

**The upgrade helpers**: the seven query functions the exe's upgrade UI, `cat_clUpgradeShips` and `cat_rmAcquireShipTechs`
read are in `ShipSelect.c` / `.h`: `cat_shipCanUpgradeArmour` 0x00514280, `cat_shipGetPendingUpgradeFlags` 0x005142b0,
`cat_shipGetPendingWeaponUpgrades` 0x005142e0 (projectile guns whose static ends at WEAPON_ENERGY once the player has
energy weapons; the switch's case 7 asks `cat_mothershipHasBigGunModule` and ignores it, as the exe), `cat_shipIsUpgradable`
0x005143c0, `cat_shipTypeUpgradeMask` 0x00514480, `cat_shipGetUpgradeTime` 0x00514540 (`cat_armourUpgradeTimes`
0x0072b1f8 = 10/20/30 by the player's armour cap, `cat_weaponUpgradeTimes` 0x0072b204 = 10, 10, 0 x 7, plus
`cat_upgradeFlagsBaseTime` of the pending bits, times `upgradeTimeModifier`), `cat_shipGetUpgradeTechs` 0x00514620 (the
exe writes its "none" slot 41 of the 42-entry set for a gun on a tech-less weapon; the tree's `TECH_NONE` is -1, so that
write is skipped). With R4's `cat_shipUpgradesInit` 0x005140a0, `cat_playerUpgradesReset` 0x00514100,
`cat_playerUpdateUpgradesFromResearch` 0x00514140, `cat_shipHasUpgrade` 0x00514210, `cat_shipApplyUpgrades` 0x00514490,
`cat_shipTypeGetUpgradeTechs` 0x005146f0 and `cat_shipInitResearchUpgrades` 0x00514af0 every function of
0x00513f40..0x00514af0 is in. **The end of an upgrade** is ported into `cat_univShipDeferredUpdate` 0x005447b0
(`UnivUpdate.c`, 0x00544a4e): `catUpgrading` with `catUpgradeEnd` (`Ship+0x5A0`) passed clears the flag,
`cat_shipSetDisableReason(ship, 0, 0)`, `cat_shipApplyUpgrades`, speech 0x2000895 for the owner; then the debug
re-apply switch 0x00a751c0 (a file static `catDebugReapplyUpgrades`, its console toggle is stage 3's).
`cat_univConvertShip` 0x00543950 carries `catArmourLevel` (`Ship+0x3F0`) over with the upgrade flags (0x00543d6f).

**Other `M2-R4 #5` leftovers closed with the layouts**: the deferred-update list is `universe.catSelection1A0` (the exe's
second grow-selection; the file static is gone and `univDeleteDeadShip` 0x005482f2 and the outside-removal path null
the ship's entry there as the exe does); `cat_blastAffectShip` 0x00515d30 marks a large shockwave's victims with the
attribute dword's 0x20000 and spares 0x10000 (`CAT_ATTRIBUTES_BlastHit` / `CAT_ATTRIBUTES_BlastImmune`, names chosen,
`Attributes.h`); `Salvage.c`'s `cat_salvageAttributes` macro is gone (`CAT_ATTRIBUTES_DockWithEnemies` is tested on the
dword); `univDeleteDeadShip`'s explosion speed term is `cat_shipGetSpeedMultiplier x maxvelocity` (0x00548010); the
damage-done table note is resolved (the exe's `+0x7D4` is `pointsOfDamageDoneToWho`). The other 17 `M2-R4 #5` notes
were re-targeted to their owners: M2-R6 ConsMgr (`cat_cmHasSupportUnits`, `cat_suShipAdded/Removed`,
`cat_cmCancelJobsOfDeadFactory`, the external-build driver), M2-R6 FOW (`univFindNearestResource`'s 4th argument,
the stealth detection), M2-R6 research (the convert-ship research block copy, `cat_rmAcquireShipTechs`), stage 5
multiplayer options (`gameOptionNoModules`, crystals salvageable), stage 5 recorder (`processCollectResource`'s
recorder globals), stage 3 (`univKillPlayer`, `univKillOtherPlayersIfDead`), M2-R5 command-tactics (`SPECIAL_SpeedBurst`'s
readers). `grep -rn "M2-R4 #5" src/` is empty.

**R5-1 leftovers**: `cat_gunPickMothershipModuleTarget` 0x0048b950 (`Gun.c`; `processAttackToDo` 0x00441c10 calls it on
the leader's target in the wingman branch: the nearest clamped CLASS_Component of an sCarrier / sMothership /
bMothership by collision position, the motherships starting from their first clamped object); `cat_collSentinelFieldExempt`
0x0043acc0 (`Collision.c`, called by `collCheckShipShipColl` 0x0043ade0 before the distance test: an active Sentinel
field spares every non-kamikaze ship, a Sentinel on a protect order spares the first ship it protects);
`cat_shipHasRamUpgrade` 0x005bf840 (`RammingFrigate.c`, the wrapper `soundEventUpdate` 0x0051cd10 calls: M2-R5 sound);
the four dissolve helpers `cat_shipDissolveCutEffects` 0x004605b0 / `EffectCreate` 0x00460af0 / `ComputePlane`
0x00460270 / `EdgeIntersect` 0x00460500 are already `Dissolve.c`'s statics `catShipDissolve*` (R5-1), verified
present; the three variant StaticInits `cat_p1MothershipVariantStaticInit` 0x005be5d0, `cat_carrierVariantStaticInit`
0x005af4a0, `cat_dreadnoughtVariantStaticInit` 0x005b3850 have **no caller and no data reference in the exe**
(`call_graph.csv`, `symbols.csv`, `decompiled.c`: only their own definitions), so they are not ported; a note in
`Carrier.c` and `P1Mothership.c` joins `Dreadnought.c`'s. Run `r5ld-exp1-dock` after these edits: dock self-test
PASS, ASan 0.

## Subsystem `compares`: the 97 IDENTICAL bodies

Every decompile was dumped (`ghidra/exports/cataclysm_gog_r6/decompiled.c`) and read against the tree's body; the
`.rdata` constants were read from the exe where the decompile shows only an address (the pi multiples 0x0072a58c /
0x0072a60c / 0x0072a594 / 0x0072a85c, 0.25 0x0072a450, 0.05 0x0072a654, 1.5 / 60 / 540 / pi/180 of the endover, 1/3
0x0072a520, 0.5 0x0072a4c8, 256 / 1/255 of the colours, the heading tweak 0.11914522 / 0.99287684 at 0x0072af40 / 3c).

| file | bodies | result |
|---|---|---|
| `FlightMan.c` | 45 | identical: every manoeuvre's states, angles, thrust modes and tweakables, `flightmanInitFunc` / `Close` / `Execute`, the random pick and test (the subtype numbers 1..5 index the same blocks of `FlightManProb`), the five script callbacks (7 values and a 7-entry cumulative loop for the evasive tables too). **CAT-FIX**: `flightmanEndoverInit` 0x0047c590 allocates 0xC bytes and writes a 0x14-byte `EndoverInfo` in HW1 and the exe alike; the tree allocates `sizeof(EndoverInfo)` |
| `Vector.c` | 14 | identical (`vecLineIntersectWithPlane` keeps HW1's debug assert; `getVectDistSloppy`'s branches match) |
| `Matrix.c` | 20 | identical (`matMultiplyMatByMat` and `matCopyAndScale` are HW1's asm-era bodies with their three static pointers; `matCreateCoordSysFromHeading`'s up-vector tweak values hold) |
| `Volume.c` | 4 | **two MODIFIED**: `volFindCenter` 0x00551140 halves with 0.5 and uses **z0 + z1** (HW1's source has `z0 + z0` and `/2`); `volFindRadius` 0x005511e0 multiplies by 1/3 then 0.5 (the tree divided by 3 and 2). Both fixed in `Volume.c`. `volPointInside`, `volSphereIntersection` identical |
| `Teams.c` | 6 | **`teReset` MODIFIED**: the race-2 swap of the two colour schemes happens only when `!singlePlayerGame` (fixed in `Teams.c`); the three script callbacks, `teColorAdjust`, `teTeamColorsSet` (8 players' six light colours, the 4 trail colours, the cheater check against `cpDarkestColor0 x 200 / 255`) identical |
| `Twiddle.c`, `Undo.c` | 2 + 2 | identical |
| `Debug.c` | 2 | identical in effect (`dbgFatal` / `dbgFatalf` format "%s (%d): Fatal error - %s" and wait; the tree adds its stack dump) |
| `fastmath.c` | 2 | **not the tree's**: the exe keeps HW1's table `fsqrt` 0x00472860 (`fmathInit` 0x004728c0 fills 2 x 8192 mantissa entries at 0x00a2e144; `fsqrt` looks the root up by exponent parity and mantissa >> 7); this tree's `FastMath.h` maps `fsqrt` to `sqrtf` (GoK's choice) and has no `fmathInit`. Left as is: an open question for the user (it changes every distance in the simulation, so it is the first thing the oracle's reference recording will show) |

`ghidra/delta_map.json`: the 97 entries carry `reviewed` and a `[review: M2-R5 layout-dock]` note (`volFindCenter`,
`teReset` now MODIFIED with their change; `fsqrt`, `fmathInit`, `flightmanEndoverInit`, `volFindRadius` with their
findings), plus review notes on the HW1-mapped functions this round edited (`univCreateShip`, `needToWaitForThisShip`,
`aishipFlyToPointAvoidingObjsFunc`, `FillInCarrierMothershipInfo`, `clCreateShip`, `ApplyDamageToTarget`,
`collCheckShipShipColl`, `processAttackToDo`, `univDeleteDeadShip`); `m2r5:layout-dock` is in `done_subsystems`. The
file is written with `tools/delta_map.py`'s `write_compact` (107 lines changed). [audit: the entries were written into `delta_map.json` only, not into `ghidra/delta_map_review.json`, so a `delta_map.py merge` would have put back the other notes of 34 entries (28 of the 97 already had a review note from M2-R1, which the new notes replaced) and a re-classify would drop 72 reviews; `counts` still said 3,065 / 1,278. The audit carried them (with its own: 90 new, 35 updated) into the review file (the 28 old notes kept in front of the new ones), added 19 review entries of its own (7 edited IDENTICAL functions that had none: `aimProcessDock`, `ShipIsRefuelingAtCarrierMother` and `SaveDockInfo` now MODIFIED, `aimProcessResourceVolume`, `kasfShipsDockSupportWith`, `DockCleanup`, `LaunchCleanup`; and the 12 dock bodies it compared), refreshed `counts` (IDENTICAL 3,060, MODIFIED 1,283, REWRITTEN 77, reviewed 1,705; `delta_map.py merge` is now a no-op) and regenerated `docs/m2-r1-function-deltas.md`]

## Subsystem `final`: builds, runs, what is left

**Builds**: `tools/build.sh --debug` and `tools/build.sh --release --clean`: 0 warnings, 0 errors (`-Werror`).
`tools/m2_layout32.sh`: OK; ILP32 sizes `Ship` 1596 (**0x63C**), `Missile` 600, `Bullet` 356, `Derelict` 660,
`Asteroid` 436, `Player` 25904, `Universe` 252348; 45 ship files' specifics asserts hold.

**Runs** (all through `tools/m2_harness.sh` / `tools/oracle/mission_test.sh`, SDL dummy audio, offscreen video):

| label | mode | milestone | first stop | result |
|---|---|---|---|---|
| `r5ld-final-mission` (-a, -b, -perturb, the flipped copy) | mission, 6,000 ticks | mission run done | none | **PASS**: a and b byte-identical (1,476,370 B, sha256 `6fba67c89e675a37`), ASan 0, no leak; the perturbed run caught at tick 3000 (rng10, 1 draw ahead), the flipped copy at 4000 (`ships_health`). `hwo_compare.py`: identical to R4's `r4-integrate-mission-a` and to `r5ld-layout3-mission-a` (6001 T records, no difference) |
| `r5ld-final-sp` | sp, `--timeout 340` | universe running (univUpdateCounter > 0) | none: still running at the 340 s timeout (rc 124 by design) | Mission01, `univUpdateCounter` 1,248 at the timeout (this run shared the machine with the mission test; R4's solo run reached 2,256), ASan 0, no leak |
| `r5ld-final-fe` | fe | Main_game_screen | none (60 s timeout) | ASan 0, no leak |
| `r5ld-final-dock` | dock (the new self-test) | dock self-test done | none (rc 0, 20.1 s) | `done fighters=6 outside=6 corvettes=2 RUs=1000 result=PASS`, 0 FAIL lines, ASan 0, no leak |

Earlier runs of the round: `r5ld-layout-mission(-a/-b/-perturb)`, `r5ld-layout2-mission`, `r5ld-layout3-mission` (PASS),
`r5ld-dock1-mission` (PASS), `r5ld-dock1..15-dock` (the self-test's development), `r5ld-exp1-dock` (PASS).

**The new self-test and harness mode**: `/catDockTest <ticks>` (engine switch, `main.c`; `Dock.c` `catDockSelfTestSet` /
`catDockSelfTestTick`; `OracleDump.c` calls the tick hook after `univUpdate`), ends with `docktest: done ... result=PASS|FAIL`;
`tools/m2_harness.sh <label> --mode dock [--dock-ticks N]` (default 5,000; milestone `dock self-test done`; the summary
carries the `docktest:` lines and the FAIL count); documented in the script header.

**Files changed** (`git diff 1434b90 --name-only`, the tree's): `src/Game/SpaceObj.h` (layouts), `Attributes.h`,
`Dock.c` / `Dock.h` (rewritten), `LaunchMgr.c` / `.h`, `CommandLayer.c` / `.h`, `CommandWrap.c`, `UnivUpdate.c` / `.h`,
`Universe.c`, `ShipSelect.c` / `.h`, `Gun.c` / `.h`, `Collision.c`, `Blast.c`, `Salvage.c`, `ResCollect.c`, `Tactics.c`,
`Tactical.c`, `SinglePlayer.c`, `KASFunc.c`, `AIMoves.c`, `AIShip.c`, `AIUtilities.c`, `Attack.c`, `BeastWeapon.c`,
`Camera.c`, `ETG.c`, `NavLights.c`, `NetCheck.c`, `OracleDump.c`, `SaveGame.c`, `Select.c`, `Shader.c`, `StatScript.c`,
`ConsMgr.c` / `.h`, `FlightMan.c`, `Volume.c`, `Teams.c`; `src/SDL/main.c`, `mainrgn.c`, `mouse.c`, `render.c`;
`src/Ships/Carrier.c`, `GenericDefender.c`, `GenericInterceptor.c`, `HiveFrigate.c`, `Leech.c`, `MCV.c`, `Mimic.c`,
`ModuleShip.c`, `Mothership.c`, `P1Mothership.c`, `P1StandardCorvette.c` / `.h`, `RammingFrigate.c` / `.h`,
`RepairCorvette.c`, `ShipDisable.c`, `Slipgate.c`; `tools/m2_harness.sh`, `tools/m2_layout32.sh`; `ghidra/delta_map.json`;
this report. Nothing committed (the auditor checkpoints).

**CAT-TODOs closed / re-targeted / added**: closed: the dock-purpose stand-ins (`HiveFrigate.c`, `Slipgate.c`, `Leech.c`,
`RepairCorvette.c`, `Tactics.c`, `ResCollect.c`, `CommandWrap.c`), `cat_shipExperienceLevel`, the attribute-dword
notes of `Blast.c` and `Salvage.c`, the explosion speed, the convert-ship 0x3F0, the deferred-list notes, the
damage-done table, the upgrade end, the 27 `M2-R4 #5` labels (see `experience`). Added, with owners:
`CAT-TODO(M2-R5 hud)` x2 (`cat_ioMessageAdd`, the `OP_ExperienceMessages` toggle), `CAT-TODO(M2-R5 sound)` x3 and
`hud / sound` x1 (the dock / harvest speech and sound ids in `CommandWrap.c`, `CommandLayer.c`),
`CAT-TODO(M2-R5 command follow-up)` x2 (`RemoveShipsFromDoingStuff`'s reason codes 0x1F / 0xB), `CAT-TODO(stage 3)`
(the `madAnimationStart` third argument in `dockPutShipOutside`), `CAT-TODO(M2-R6 ConsMgr)` (`cat_cmExternalBuildAttach`,
`cat_suShipProvidesSU`, `cat_cmShipDockedReclampCannon` in `cat_dockFinishedDocking`), `CAT-TODO(M2-R6 research)`
(`cat_rmAcquireShipTechs`), `CAT-TODO(M2-R6 FOW)` (`cat_univShipFOWActivate` in `dockPutShipOutside`, the harvest
order's 4th argument), `CAT-TODO(stage 6)` (Mission 9's `G_ResourcersRetired`), `CAT-TODO(stage 5)` (the five dock
words in the sync checksum).

**What is left, and the owner**: the experience message and option UI (M2-R5 hud); the speech / sound ids (M2-R5 sound);
`cat_processLinkToDo` / `cat_processUnlinkToDo`'s experience calls come with the orders 11-17 (M2-R5 command-tactics);
`cat_consoleExperienceShips` 0x0054d1c0 and the 0x00a751c0 console toggle (stage 3); the ConsMgr, FOW and research
TODOs above (M2-R6); the save of `universe.catSelection1A0` (the exe's `SaveUniverse` 0x005034c0 saves it after
`HousekeepShipList`; this tree's `SaveGame.c` does not yet: stage 5 save games); the `fsqrt` decision (user); the ACV
that orbits its dock point for minutes before latching (the corvette flight model along the cone, `AIShip.c`, stage 4;
the dock code matches the exe) [M2-R6 managers: does not persist: with the exe's `SOF_CatDocking` collision exemption
added to `Collision.c` both ACVs are inside by tick 1250 (without it the late one at ~2048); docs/m2-r6-managers.md `dock`] and the harvesting Worker's jack animation (`Worker.c`, stage 3 `madAnimationStart`).
Medium-confidence readings: `catRetreating`'s meaning; the retire refund's float (`TW_RETIRE_MONEYBACK_RATIO x buildCost`
stands in for the `__ftol` the decompile shows) [audit: confirmed by the disassembly, see `dock`]; the dock state names; `catObjReserved88[4]`, `catShipReserved5B0`,
`catMissileReserved248[3]`, `catNavLightStatic` have no exe reader found.
