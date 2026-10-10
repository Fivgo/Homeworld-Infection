<!-- done: layout, keys, guns, selftest, runs -->
# M2-R2 port: Cataclysm ship statics (producer 3)

Status: COMPLETE (2026-10-10), not committed. Branch `m2`, worktree `.claude/worktrees/m2`. Scope:
docs/m2-r1-summary.md "M2-R2 plan", producer 3. Spec: docs/formats/shipstatic.md (now with an "M2-R2 findings"
section); probe: tools/formats/shipstatic_probe.py. Subsystems, in done-marker order: `layout`, `keys`, `guns`,
`selftest`, `runs`.

**Result.**
- `src/` loads Cataclysm's `.shp` files with Cataclysm's tables:
  - the ship key table is the exe's 178 keys in the exe's order (29 new keys, HW1's fuel/salvage/scale keys gone);
  - the gun table is the exe's 39 keys (8 new, 4 obsolete);
  - the gun types `GUN_HiddenGimble`/`GUN_VisibleGimble` and the `WEAPON_*`, `GUNSIZE_*` and 35 `DPT_*` names are known;
  - InitStatShipInfo and scriptSetGunStatics apply the exe's defaults and fix-ups.
- `ShipStaticInfo` (0x458), `GunStatic` (0x11C) and `CustShipHeader` (0x8C, 34 slots, `CustShipAttack` in slot 21)
  have the exe's layout on an ILP32 target. Static asserts pin it: 3 sizes, `StaticHeader` 0xA4 and 225 field offsets,
  among them all 178 scripted keys. `tools/m2_layout32.sh` checks them with `clang -m32`. LP64 sizes are pinned too.
- **Parse check.** The self-test `/catShipStaticsTest` loads every one of the 145 ship types and logs one line per
  ship. `tools/formats/shipstatic_engine_check.py` then compares each line with the probe:
  - **143 of 143 ship-type files** parse exactly as the probe does: type, race directory, the number of
    `ShipStaticScriptTable` keys, guns, docks and nav lights;
  - the 3 light-count files (`sRecon`, `bRecon`, `trGhostship`) use the declared count, as the exe does;
  - the 2 types without a `.shp` (107 `trResearchStationBridge`, 108 `trResearchStationTower`) are disabled as in the
    exe, **with a log line** (no silent disabling).
- **Caveat on 143/143:** `p1Outpost` needed a diagnostic data variant. In the isolation tree, the down-converted
  `p1Outpost` mesh corrupts the HW1 memory pool inside `meshLoad`, which is the mesh item. With that ship's mesh
  files removed from a variant tree (its `.shp` unchanged), its `.shp` parses as the probe says.
- **The other 30 `.shp` files** are not ship statics:
  - 24 are loaded by this tree's missile, mine, asteroid, dust-cloud, nebula and derelict loaders and logged;
  - 3 (`Galaxy`, `LavaBall`, `MattWorld`) need Cataclysm's derelict type table (0x00889b68, 8 types), which is not
    ported;
  - 3 (`DefaultShip.shp`, `generic_ship.shp`, `dplasmacloud.shp`) are never read by the exe (no such string in it).
    [audit: imprecise. No `<name>.shp` string exists for any of the 3 and none of the names is in a type table, but
    `defaultship` (the DefaultShip mesh directory) and `race_derelict\dplasmacloud\rl0\lod0\cloud` (0x008bab7c, the fog
    layer texture of `cat_rndFogLayerTextureLoad`) are in the exe. The conclusion "never read as .shp" stands.]
  - [audit: 25 of the 173 `.shp` are overridden by `update.big` (`original/game_extracted/update/`), and the engine
    reads those versions. The check above compares with `game_extracted/cataclysm` only. Re-run against a merged tree
    (update over base): still 143/143, 0 mismatches. The audit's field-value comparison (7,039 ship keys, 4,759 gun
    values, 143 ships, 271 guns) matches only against the update versions.]
- `tools/build.sh --debug` and `--release`, both `--clean`: rc 0, `[292/292] Linking target hwi-engine`, 0 warnings
  under `-Werror`.
- **Harness.** The plain harness (the gate) still stops first at `Unable to open file: HW_Music.wxd`, data-deltas
  rank 9, which is producer 4's item. The isolation level-load run gets as far as before: `Mission01.mission` is read
  and `RACE_SECT/sRecon.shp` parses (logged). It then stops in `GenericInterceptorStaticInit`, a stage-2 slot body
  (see "What is left").

**DONE WHEN.**

| criterion | state | evidence |
|---|---|---|
| 170 of 173 `.shp` parse as the probe does, the 3 light-count cases handled like the exe | **met for the ship statics, not for all 173.** 143/143 ship-type files match (3 of them with the declared nav-light count). Of the 30 non-ship files, 24 are loaded by other loaders. 3 need the unported derelict type table and 3 are never read by the exe | runs `ss-r6-statics-iso`, `ss-r9-statics-iso-p1nomesh`; `engine_check.txt` in both run dirs |
| a log line per ship proving it | met: `shipstatic: loaded RACE_SECT/sRecon.shp type=0 race=0 keys=69 guns=1 docks=0 navs=0 lod=yes staticinit=...`, one per type | same runs; also `ss-r8-sp-iso` (mission path) |
| no ship type silently disabled | met: `shipstatic: RACE_TRADERS/trResearchStationBridge.shp not found, ship type 107 (trResearchStationBridge) disabled` (and 108) | same runs |
| static asserts pin the three struct sizes | met: ILP32 0x458 / 0x11C / 0x8C plus 225 offsets (`tools/m2_layout32.sh`: OK; a deliberately wrong size makes it fail with rc 1); LP64 0x540 / 0x11C / 0x118 | `src/Game/SpaceObj.h` |

## Subsystem `layout`

`src/Game/SpaceObj.h`

| struct | change | exe evidence |
|---|---|---|
| `GunStatic` | 4 fields after `bulletType` (`startUpgradeLevel` +0x0C, `endUpgradeLevel` +0x10, `gunSize` +0x14, `damageMult` +0x18); `bulletlifetime2`, `bulletspeed2`, `bulletrange2`, `firetime2` after `burstWaitTime` (+0xF4..+0x100); 0x11C | gun key table 0x008975a0 (offset = field − template); `scriptSetGunStatics` 0x00526790 allocates `n * 0x11C + 4` and writes `slaveDriver` at +0x118 |
| `StaticCollInfo` | new `vector collrectmid` after `collrectoffset`: +12 bytes, `preciseSelection` moves to +0x88; `StaticHeader` is 0xA4 | `mexSetCollisionInfo` 0x004b7880 writes `(collrectoffset + lengths) * 0.5` at `StaticCollInfo`+0x38; the MiningBase hack in `InitStatShipInfo` still adds to +0x54/+0x60 |
| `StaticInfoShipOrDerelict` | `salvageStaticInfo` removed | `teamColor[8]` at +0xF0 (`universeDefaultTeamColors` 0x0053c940), `sinbank` at +0xF8 (`aishipPrecalcInfo` 0x00413bd0); no `NUMBER_OF_SALVAGE_POINTS`/`SalvagePoint` string in the exe |
| `CustShipHeader` | no `shiptype`; `sizeofShipSpecifics` + 34 slots in the exe's order. New slots 9 (order changed), 12-14 (object/effect/bullet removal), 20 (move), 22 (dock), 23 (launch), 27 (halt), 34 (formation); 24-26 and 28-33 unused; `CustShipAttack` moved to slot 21 | `InitStatShipInfo` copies 0x23 dwords from `shipHeaders` 0x008b1938; slot meanings per docs/formats/shipstatic.md; new-slot signatures from the Round-5 prototypes of `CarrierOrderChanged`, `CarrierBulletRemoval`, `MothershipEffectRemoval`, `HiveFrigateDockCommand` (the others UNVERIFIED) |
| `ShipStaticInfo` | exe layout, 0x458. New fields: `catTypeFlags` +0x1EC, `externalBuild` +0x1F0, `forceFieldProtectionDistanceScale` +0x1F4, `upgradeTimeModifier` +0x1F8, `unknownFlag212/213`, `fowRadius` +0x240, `RKOEatMeTime` +0x250, `unknown2B0[6]`, `nRepairDroids` +0x320 (key `repairDroids`), `hyperspaceBoxScale`, `leechRUsPerSecond`, `leechHPsPerSecond`, `beastAttackCascadeCost`, `beastAttacksRequired`, `beastAttackConvertTo`, the RKO/ram/cutting-beam/EMP/support-unit/pilot-view/link-distance tail +0x408..+0x457. HW1's `tractorEmitterStatic` and the debug-only `scaleFactor` are gone. Fuel fields keep their slots (their keys are gone) | `ShipStaticScriptTable` 0x008b2428 (every key); `InitStatShipInfo` 0x0053bab0 (defaults); `CopyPointersFromRealShipStaticInfo` 0x004ffc60 (+0x194, +0x3F4, +0x400/+0x404, and no salvage or tractor pointer); `ShipStaticInfos` 0x00b3b420 (145 x 0x458); CET `game/Game/spaceobj.h` |
| asserts | ILP32: the 3 sizes, `sizeof(StaticInfo)` 0xA4, 225 `CAT_ASSERT_OFFSET`s (all 178 keys plus the non-scripted fields above); LP64: `GunStatic` 0x11C, `CustShipHeader` 0x118, `ShipStaticInfo` 0x540 | |

Fallout of the layout, file by file:
- `src/Ships/*.c` (40 files): all 47 `CustShipHeader` initializers are rewritten as designated initializers in the
  Cataclysm slot order, each with a `CAT:` comment. The HW1 `shiptype` entry is dropped. A script compared every
  header with HEAD: 47 checked, 0 slot differences. The new slots stay NULL. No code in the tree calls them yet:
  their callers (`clMove`, `clDock`, `cat_univRemove*References`, ...) are stage 2.
- `src/Game/Universe.c`:
  - `InitStatShipInfo` loses HW1's header/type assertion. The exe has no `shiptype` in the header, and the HW1
    aliases (`Carrier = kCarrier`, ...) would have tripped it for `bCarrier` etc.;
  - `CloseStatShipInfo` no longer frees the salvage and tractor statics (the exe frees +0x3F4, +0x3F8, +0x3FC,
    +0x3F0, +0x104, +0x3EC, `CloseStatShipInfo` 0x0053c500);
  - `CloseStatDerelictInfo` no longer frees salvage statics.
- `SpaceObj.h` `CAT_SALVAGESTATICINFO(statinfo)`, which is always NULL, replaces every read of the removed pointer, in
  `src/Game/Dock.c`, `src/Game/UnivUpdate.c`, `src/Game/SaveGame.c` and `src/Ships/SalCapCorvette.c`. HW1's salvage
  paths therefore stay dormant. `src/Game/SaveGame.c` `CopyPointersFromRealShipStaticInfo` no longer copies the two
  removed pointers.
- `src/Game/StatScript.c`/`.h`: HW1's salvage loaders (`scriptSetSalvageStatCB`, `scriptSetSalvagePointCB`,
  `scriptSetSalvageStatics`, `StaticSalvageInfoScriptTable`) are under `#if 0`. `NUMBER_OF_SALVAGE_POINTS` is gone
  from the ship and derelict tables.
- `src/SDL/render.c`: the `SO_CLOOGE_SCALE` ship scaling is under `#if 0` (no `scaleFactor`).
- `tools/m2_layout32.sh` (new): `clang -m32 -fsyntax-only` of SpaceObj.h, using an empty `gnu/stubs-32.h` shim in a
  temp dir. Result: `ILP32 sizes (ShipStaticInfo GunStatic CustShipHeader): char (*)[1112] char (*)[284] char (*)[140]`,
  `m2_layout32: OK`.

## Subsystem `keys`

`src/Game/Universe.c`

| change | exe evidence |
|---|---|
| `ShipStaticScriptTable` = the exe's 178 entries in the exe's order. The 29 new keys are each marked `// CAT: new key`. Gone from GoK's table: `maxfuel`, `lowfuelpoint`, `fuelburnrate`, `attackfuelburnrate`, `pumpFuelRate`, `NUMBER_OF_SALVAGE_POINTS` and the debug-only `scaleFactor`. HW1 already had `scaleCap`/`NumRepairDroids` commented out; `N-LIPS` and `repairDroids` are the Cataclysm keys. Callbacks: HW1's for keys HW1 had; for the new keys, the exe's callback or the signed twin the linker folded it with (`beastAttacksRequired` defaults to -1) | 0x008b2428 (dump `m2r2-shipstatic-tools/shiptab.txt`: 178 rows, 29 keys not in GoK's table, 7 GoK keys not in the exe's). The exe's `scriptSetU{dword,word,byte}CB` all scan `"%d"` (0x00753118), i.e. they are the folded signed bodies |
| `cat_scriptSetBeastAttackConvertToCB` (new): `StrToShipType` | 0x0053ac50 |
| `InitStatShipInfo`, before the file check: defaults `bUseRayTriangleIntersectionForHitLocation` TRUE, `forceFieldProtectionDistanceScale` -1, `upgradeTimeModifier` 1, `leechRUsPerSecond` 1, `leechHPsPerSecond` 20, `nRepairDroids`/`fowRadius`/`linkDistance` 0, `externalBuild` FALSE | 0x0053bab0 |
| a missing `.shp` still disables the type (`SetInfoNeeded...`, `rmEnableShip`), but logs `shipstatic: <path> not found..., ship type N (...) disabled` | same |
| before the read: `hyperspaceBoxScale` 1, `beastAttackCascadeCost` 1, `beastAttacksRequired` -1, `beastAttackConvertTo` `bWorker` (0x31), `UpgradeByDocking`/pilot view 0, `RKOGetsMissiles` low byte 0, `dockShipRange` 0, `canSpecialBandBoxFriendlies` 0, +0x212 = 1, +0x213 = 0 | same |
| after the read: `repairCombatDamage` defaults to `repairDamage * 0.2` (HW1: equal to `repairDamage`); `cat_univInitShipTypeFlags`; single-player build costs `bHeavyCruiser` 3000, `bIonArrayFrigate` 600, `sMothershipBigGun` 5000; `attackfuelburnrate` 0 when 0 and no `lowfuelpoint` scaling (no fuel); `dockShipRange` kept as read, not padded and squared; no `tractorEmitterStatic`; MiningBase hack keyed on `trMiningBase` (100); `forceFieldProtectionDistanceScale <= 1.0` becomes 2.0 after the slot-1 call [audit: the producer had `<= 0`; 0x0053c3b5 compares with the double 1.0 at 0x0072a548, warning string 0x008b58c8 "smaller than or equal to 1.0"] | 0.2 at 0x0072a510, 2.0 = 0x40000000 (both in `InitStatShipInfo`); the exe's dock code adds the collision radius where it reads +0x29C (`cat_dockShipDocksAtShip` 0x00465860) |
| `dockInitializeCustomFunctions` is no longer called. The exe has no such function. HW1's per-type set-up looks up HW1 dock-point names and stopped the self-test with `Dock.c (306): Fatal error - Dock name In not found` (sMothership, run `ss-r2-statics-iso`). The two dock callbacks get HW1's default handlers as safe defaults (declared in `src/Game/Dock.h`) | no `dockInitializeCustomFunctions`/`mexGetTractorBeamStatic` in functions.csv |
| `src/Game/Dock.c` `ShipWithinDockRange` forms HW1's padded, squared range itself (the loader no longer does) | keeps HW1 dock behaviour until stage 2 |
| `cat_univInitShipTypeFlags` (new): the exe's per-type bit switch into +0x1EC (`catTypeFlags`) | 0x0053b8c0 |
| `isStrikeCraft` = the exe's 27 types (it is also the formation-padding default) | 0x0053ba20; `InitStatShipInfo` inlines the same switch |
| `precalcStaticInfoGuidance`: `thrust = mass * thrust * GLOBAL_SHIP_SPEED_MODIFIER`, the exe's operation order (HW1: `thrust *= mass * modifier`) | `fld mass; fmul thrust; fmul modifier` at 0x0053c05d |
| per-ship log line `shipstatic: loaded <path> type= race= keys= guns= docks= navs= lod= staticinit=`, written before the slot-1 call (`staticinit=none|skipped|next`) | replaces the exe's stripped "SHP file loaded: %s" print |
| `src/Game/StatScript.c`/`.h`: `scriptSetStruct` records how many keys it matched (`scriptStructEntriesSet`, a port diagnostic; the value is left by the call that finishes last, so a nested LOD read does not inflate the ship count) | diagnostic only |

## Subsystem `guns`

| file | change | exe evidence |
|---|---|---|
| `src/Game/ObjTypes.h`/`.c` | `GUN_HiddenGimble` (= `GUN_Gimble` 1) and `GUN_VisibleGimble` (= `GUN_NewGimble` 2) added to the gun type table | `guntypeinfo` 0x00889c08 rows 5-6 |
| same | `WeaponType` (`WEAPON_PROJECTILE`..`WEAPON_BEASTINFECT`, 0-8) and `GunSize` (`GUNSIZE_SMALL`..`GUNSIZE_VERYLARGE`, 0-3) enums and tables; `cat_StrToWeaponType`, `cat_StrToGunSize` (-1 if unknown; case-insensitive) | 0x00889ea8, 0x00889e80; 0x004d7130, 0x004d70f0; compare = `_stricmp` 0x00676c90 |
| same | `DockPointType` = the exe's 35 `DPT_*` names and values. HW1's `DockingCone`/`LatchPoint`/`LaunchPoint` are kept at 100-102 so unported dock code compiles and stays dormant (before this change, every `DPT_*` line parsed to -1 anyway) | `dockpointtypeinfo` 0x00889c48; `StrToDockPointType` 0x004d7030 |
| `src/Game/StatScript.c` | gun table = the exe's 39 entries in order; `SoundType`/`BulletType`/`DamageLo`/`DamageHi` go to `cat_scriptSetObsoleteGunKeyCB` (ignored); new `cat_scriptSetWeaponTypeCB`, `cat_scriptSetGunSizeCB` | 0x008975a0; obsolete callbacks 0x00526050/60/70/80 only print; 0x00525ff0, 0x00526020 |
| same, `scriptSetGunStatics` | at a gun's `}`, the damage arrays take the base values for all 3 tactics (no fighter/corvette `DamageBonus`); after the parse, the ranges get no `BulletRangeBonus`/`BulletSpeedBonus`; `BulletLifeTime2` defaults to `range2/speed2`, each falling back to the first mode when <= 0; a gunless `CLASS_Resource` ship gets `ASTEROID_HARVEST_RANGE` (HW1: type `ResourceCollector`) | disassembly 0x005269e0..0x00526a88, 0x00526b49..0x00526c7f, 0x00526c81 |
| same, `scriptSetDockStatics` | a dock point whose heading equals its up direction gets 0/2 | 0x005270c0 (DockStaticPoint +0x30/+0x34) |

Nav lights: the exe's `scriptSetNAVLightStatics` 0x00526dc0 also stops at the declared count (`<= 0` reads none),
like HW1, so `sRecon`/`bRecon` (declared 0, 8 lines) get no nav lights and `trGhostship` gets 98 of its 99. No change
was needed. The exe's `NAVLightStatic` is 0x48 bytes against HW1's 0x40; this port does not adopt that (render, stage 3).

## Subsystem `selftest`

- `/catShipStaticsTest` (`src/SDL/main.c`, `src/SDL/utility.c`, `src/Game/Universe.c` `catShipStaticsRunSelfTest`)
  is a port diagnostic, not a Cataclysm option. After system start-up it:
  1. loads the shared DefaultShip statics, as `universeStaticInit` does;
  2. runs `InitStatShipInfo` for all 145 types, with the race = the type's race range, and closes each one again.
     `CustShipStaticInit` is skipped, because the slot-1 bodies are stage 2;
  3. runs the missile and mine loaders for every race, plus the asteroid, dust-cloud, nebula and (existing-file)
     derelict loaders, logging `shipstatic: other <kind> <path> loaded`;
  4. quits through `utyGameQuit`.
- `/catShipStaticsSkip n,n,...` leaves types out (used once to get past `p1Outpost`, see below).
- `tools/m2_harness.sh --mode statics` runs it. The summary adds a milestone `ship statics self-test done` and a line
  `ship statics: N loaded, N not found (disabled), N other .shp`.
- `tools/formats/shipstatic_engine_check.py` (new) runs the probe's per-file analysis on every `.shp` of the extracted
  data and compares it with the log lines (rules in its docstring).
- `tools/formats/shipstatic_probe.py`: the per-file analysis moved into `read_tables()`/`analyse()` so the check can
  reuse it. Its output on all 173 files is byte-identical to before (`diff`: identical).

## Subsystem `runs`

All runs use the debug build (ASan + UBSan), SDL `offscreen` and no input. Logs are in
`original/scratch-m2/m2r2-harness/runs/<label>/`, ledger `runs/runs.txt`. "iso" = `m2r2-harness/iso-ss/`
(`tools/m2_harness_iso.sh iso-ss 3,4,5,6,7,8,13,14`, the same tree as the identifiers item) with
`-- /ignoreBigfiles /noSpeech`. "plain" = the unmodified install, which is the gate.

| label | data | change since the previous run | rc | milestone | first stop |
|---|---|---|---|---|---|
| ss-r1-statics-iso | iso | layout + keys + guns + self-test | 1 | Main_game_screen | ASan SEGV `Mesh.c:3043 meshBindingListCreate`. The self-test had not loaded `defaultmesh`, so `sRepairDroid`'s missing LOD levels fell back to NULL (fixed: the self-test loads the DefaultShip statics). 15 ships logged |
| ss-r2-statics-iso | iso | DefaultShip preamble; key count fixed (nested LOD reads) | 1 | Main_game_screen | `Dock.c (306): Fatal error - Dock name In not found`, in HW1's `dockInitializeCustomFunctions` for `sMothership`. Fixed: the exe has no such step |
| ss-r3-statics-iso | iso | no `dockInitializeCustomFunctions` | 1 | Main_game_screen | `Memory.c (1625): Fatal error - Corrupt cookie: 0x5f317000` while loading type 92 `p1Outpost`; 92 ships logged |
| ss-tmp-heapwalk, ss-tmp-bt | iso | temporary diagnostics, both removed again: a heap walk after each ship, and a sanitizer stack trace in `dbgFatalf` | 1 | Main_game_screen | same. The heap is intact after type 91. The overwrite happens inside `meshLoad` (`Mesh.c:955` `memFree` after the `memcpy` at :953) on `RACE_P1/p1Outpost/Rl0/LOD0/p1Outpost.peo`, a down-converted mesh: the mesh item's territory |
| ss-r4-statics-iso | iso | `/catShipStaticsSkip 92` | 1 | ship statics self-test done | after the test, at quit: `utility.c (3462): Assertion of (result != -1)`. GoK's `utyGrowthHeapFree` calls `munmap(heap, 0)`, a pre-existing GoK bug that shows whenever a growth heap was created. 142 loaded, 2 not found, 24 other |
| ss-r5-sp-iso | iso | none | 1 | level load (mission file read) | `GenericInterceptor.c (205): Assertion of (interceptorstat != NULL)`, the identifiers item's stop (stage-2 slot body) |
| **ss-r6-statics-iso** | iso | log line moved before the slot-1 call | 1 | ship statics self-test done | the quit-time `munmap` assertion as in r4. **Check: 139 OK + 3 OK-DECLARED-COUNT + 1 skipped of 143 ship-type files; 24 other loaded; 0 mismatches** |
| ss-r7-statics-iso-p1geo | variant `iso-ss-p1geo` (p1Outpost `.peo` links removed, so `.geo` loads) | none | 1 | Main_game_screen | the same corrupt cookie, in p1Outpost's `.geo` too |
| **ss-r9-statics-iso-p1nomesh** | same variant with p1Outpost's `.geo` links also removed (its LODs fall back to DefaultShip, its `.shp` unchanged) | none | 1 | ship statics self-test done | the quit-time `munmap` assertion. **Check: 140 OK + 3 OK-DECLARED-COUNT = 143 of 143 ship-type files; 24 other loaded; 0 mismatches; `p1Outpost.shp` type=92 keys=62 guns=0 docks=5 navs=0** |
| ss-gate-sp / ss-gate-fe / ss-gate-statics | plain | all | 213 | process started | `Unable to open file: HW_Music.wxd` (data-deltas rank 9, producer 4); 0 ASan errors |
| **ss-r8-sp-iso** | iso | all (final) | 1 | level load (mission file read) | `GenericInterceptor.c (205)`, right after `shipstatic: loaded RACE_SECT/sRecon.shp type=0 race=0 keys=69 guns=1 docks=0 navs=0 lod=yes staticinit=next`; 0 ASan errors |

Check commands, for reproduction:
`python3 -I tools/formats/shipstatic_engine_check.py --exe original/game/cataclysm.exe --data original/game_extracted/cataclysm <run>/run.log`.
The output is saved as `engine_check.txt` in `ss-r6-statics-iso/` and `ss-r9-statics-iso-p1nomesh/`.

Sanitizers in the self-test runs:
- 0 ASan errors in every run after r1.
- UBSan reports only the known misaligned `fescreen`/`PATCH` accesses and the `/noSpeech` `MUSICHEADER` NULL
  (identifiers item).

## What is left for later stages

1. **Slot bodies (stage 2).** These are the next level-load stop. `GenericInterceptorStaticInit` (exe 0x005b1e70)
   uses 14 per-type statics of 0x3640 bytes and a new key table (`gunsFireTime`, ...), where HW1 had 8 per-race
   `GenericInterceptorStatics`. The other HW1 StaticInits (`GenericDefender`, `Carrier`, `Mothership`, ...) will
   follow. The 22 Cataclysm-only headers still point at `DefaultShipHeader`. Each new slot (9, 12-14, 20, 22-34) is
   NULL in every header until its callers and bodies are ported. The exe's slot contents for all 51 headers are in
   `original/scratch-m2/m2r2-shipstatic-tools/headers.txt` (from `headers.py`: `shipHeaders` 0x008b1938, 35 dwords
   per header, names from the Round-5 symbols; this item's helper scripts are kept there too, outside the repository).
2. **Mesh (producer 1).**
   - `p1Outpost`'s down-converted mesh corrupts the HW1 memory pool in `meshLoad` (runs r3/r7, evidence above).
   - `mexSetCollisionInfo` 0x004b7880 is MODIFIED in ways this item did not port: it scales the collision sphere by
     the largest scale factor, adds a constant to the avoid size, recentres the scaled rectangle, grows the sphere to
     the rectangle's half-diagonal, and fills `collrectmid`. `collrectmid` stays 0 here, and nothing reads it yet.
3. **Derelict type table.** Cataclysm has 8 derelict types (`Crate`, `Homeworld`, `PlanetOfOrigin`,
   `PlanetOfOrigin_scarred`, `Galaxy`, `LavaBall`, `MattWorld`, `HyperspaceGate`; 0x00889b68). HW1's enum has 55 types,
   named in 157 source lines, with 32 uses of `NUM_DERELICTTYPES`. `Galaxy`/`LavaBall`/`MattWorld.shp` load once it is ported.
4. **Docking (stage 2).**
   - Cataclysm's `DockStaticInfo` has a 0x50-byte header (per-type first-index and count tables over the 35 `DPT_*`,
     `numDockPoints` at +0x48, a "center" index at +0x4C) and the post-parse index build in `scriptSetDockStatics`.
   - Who sets +0x400/+0x404 in the exe is unknown; this port gives them HW1's default handlers.
   - `ShipWithinDockRange` keeps HW1 semantics.
5. **Salvage (stage 2):** HW1's salvage statics are gone; Cataclysm's salvage uses `DPT_Salvage` dock points.
6. **Gun damage (stage 2):** `DamageLo`/`DamageHi` are ignored keys, so `gunDamageLo/Hi` are 0 until Cataclysm's
   damage source (by `WEAPON_*`/`GUNSIZE_*`/`DamageMult`, presumably `damage.script`) is ported.
7. **Unknown fields:** +0x212/+0x213, +0x2B0..+0x2C7, +0x42C, +0x44C/+0x450; the meaning of the +0x1EC bits;
   `NAVLightStatic`'s two extra dwords.
8. **Save format:** `SaveGame.c` writes `sizeof(ShipStaticInfo)` chunks for dynamic statics. The size changed (LP64
   0x540), so pre-port saves of this tree do not load. Save layout waits for real saves (M2-R1 plan).
9. **GoK quit bug:** `utyGrowthHeapFree` calls `munmap(heap, 0)` (EINVAL on Linux), so every quit after a growth heap
   was created ends with an assertion (rc 1).
10. **HW1 type aliases:** `isStrikeCraft` and `trMiningBase` were switched to Cataclysm types here, but other HW1
    alias uses in `InitStatShipInfo`'s callees remain (e.g. `aishipPrecalcInfo`, `ShipCanHarvest`).
11. [audit] **IDENTICAL functions edited / delta-map conflicts** (docs/audits/m2-r2-shipstatics.md section 3):
    - `CopyPointersFromRealShipStaticInfo` 0x004ffc60 is IDENTICAL (medium) in `ghidra/delta_map.json`, but the exe
      copies no salvage and no tractor pointer (disassembly), so the map should say MODIFIED; the port's edit matches
      the exe.
    - `precalcStaticInfoGuidance` 0x0053ad70 (IDENTICAL, medium) was edited to the ship inline order
      `(mass * thrust) * modifier`. The exe's standalone body, which `universeStaticInit` still calls, multiplies
      `(modifier * thrust) * mass`; `InitStatMissileInfo`/`InitStatMineInfo` inline the ship order. Every order gives
      the same result while `GLOBAL_SHIP_SPEED_MODIFIER` is 1.0 (exe default 0x008a8098 and shipped `tweak.script`),
      so this matters only if a script changes the modifier. Revisit with the derelict loader in stage 2.
    - `scriptSetStruct` 0x00526260 (IDENTICAL) gained the diagnostic counter `scriptStructEntriesSet`; behaviour is
      unchanged.

## Counts

- `ShipStaticScriptTable`: 178 keys (29 new; 7 GoK keys removed).
- Gun table: 39 keys (8 new, 4 obsolete).
- Gun type names: 2 added. `WEAPON_*`: 9. `GUNSIZE_*`: 4. `DPT_*`: 35.
- `CustShipHeader` initializers rewritten: 47 in 40 Ships files.
- Static asserts in SpaceObj.h: 8 `_Static_assert` plus 225 `CAT_ASSERT_OFFSET`.
- Self-test coverage:
  - ship-type `.shp`: 143/143 match the probe (3 with the declared nav count);
  - types without a file: 2, both logged;
  - other `.shp`: 24/30 loaded (3 need the derelict table, 3 are unread by the exe).
- Files changed: 56 under `src/` and `tools/`, plus 3 new (`tools/m2_layout32.sh`,
  `tools/formats/shipstatic_engine_check.py`, this report) and the spec addendum. Diff +1,854 / -1,359 lines before
  this report. 205 added lines carry a `CAT:` comment.
- Harness runs: 14 (3 plain gate, 11 isolation or diagnostic).
