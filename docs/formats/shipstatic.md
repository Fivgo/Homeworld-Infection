<!-- done: shipstatic -->
# Ship static info (`.shp`): parser, `ShipStaticInfo`, `CustShipHeader`, 145 ship types

M2-R1 item 2. Sources: Cataclysm `InitStatShipInfo` 0x0053bab0, `scriptSetStruct` 0x00526260, `scriptSetGunStatics`
0x00526790, `scriptSetDockStatics` 0x005270c0, `scriptSetNAVLightStatics` 0x00526dc0, `univCreateShip` 0x00542320
(objdump at 0x542a6a), the `.data` tables `ShipStaticScriptTable` 0x008b2428 (178 x 16 B), gun table 0x008975a0
(39 x 16 B), `shiptypeinfo` 0x00889530, `FirstShipTypeOfRace`/`LastShipTypeOfRace` 0x008894e8/0x00889504,
`shipHeaders` 0x008b1938 and the 52 distinct `CustShipHeader`s they point to, plus an objdump census of every
`call` through `staticinfo+0x10C..0x190` in 0x401000-0x5d6fe0. HW1: `ORIG@ad1bb82:src/Game/spaceobj.h:470-700`
(laid out with `clang -target i686-pc-windows-msvc -fms-extensions -DHW_Release -fdump-record-layouts-complete` over a
case-insensitive header shim: `ShipStaticInfo` 880 B, `CustShipHeader` 72 B, `GunStatic` 252 B),
`universe.c:411-665` (`ShipStaticCapLimits`, `ShipStaticScriptTable`), `statscript.c:82-115` (gun table),
`ShipDefs.h` (enum), and CET's reconstructed `CET:game/Game/spaceobj.h` (asserted offsets). Data checked: all 173
`.shp` files with `tools/formats/shipstatic_probe.py`.

## Summary

* **`.shp` is still a `scriptSetStruct` text file** (see script.md) read four times by `InitStatShipInfo`: the
  ShipStaticInfo keys (`ShipStaticScriptTable`), then the hand-parsed `NUMBER_OF_DOCK_POINTS`/`DockPoint`,
  `NUMBER_OF_NAV_LIGHTS`/`NavLight` and `NUMBER_OF_GUNS`/`GUN n { ... }` blocks, then `BindGun`/`BindAnimations` and the
  MAD tables. As in HW1 the declared count wins: `sRecon.shp`/`bRecon.shp` say `NUMBER_OF_NAV_LIGHTS 0` but list 8
  lights, `trGhostship.shp` says 98 and lists 99; the extra lines are ignored (HW1 statscript.c `goto done`).
* **`ShipStaticInfo` grew from 0x370 (880) to 0x458 (1,112) bytes** (`ShipStaticInfos` 0x00b3b420 is 145 x 0x458). Every
  scripted field moved. The shifts are cumulative: +12 inside `StaticHeader.staticCollInfo` (between `collspheresize`
  +0x4C and `preciseSelection`), -4 between `turnspeedstat` and `navlightStaticInfo`, +68 for the bigger
  `CustShipHeader`, +16 after `shipclass` (`externalBuild`, `forceFieldProtectionDistanceScale`,
  `upgradeTimeModifier`, one unknown), +2 bytes of flags after `canSpecialBandBoxFriendlies`, `fowRadius` after
  `buildTime`, `RKOEatMeTime` after `svFirePower`, 24 unknown bytes after `cantMoveAndAttack`, `repairDroids` before
  the trail arrays, six Beast/hyperspace fields after `hyperspaceColor`, and a 0x6C-byte tail of new fields (RKO, ram,
  cutting beam, EMP, support units, pilot view, link distance) at +0x3EC..+0x457, with the static-info pointers moved
  in front of it.
* **29 new `.shp` keys, 8 HW1 keys dropped** (the five fuel keys `maxfuel`, `lowfuelpoint`, `fuelburnrate`,
  `attackfuelburnrate`, `pumpFuelRate`, plus `scaleCap`, `scaleFactor`, `NumRepairDroids`). The fuel fields still occupy
  their slots (+0x268..+0x274, +0x288); only the keys are gone. `scaleCap` is now set only through the `N-LIPS` key.
* **`CustShipHeader` is 0x8C bytes = `sizeofShipSpecifics` + 34 function slots, no leading `shiptype`** (HW1: 0x48 =
  `shiptype` + `sizeofShipSpecifics` + 16 slots). `InitStatShipInfo` copies 0x23 dwords from `shipHeaders[type]` to
  `statinfo+0x108`. Slots 1-19 keep HW1's order except that **`CustShipAttack` moved from HW1 slot 4 to slot 21 (+0x15C)**
  and four new callbacks were inserted (order changed, object/effect/bullet reference removal); slots 20-34 are new
  command hooks (move, attack, dock, launch, halt, formation). Ghidra's `CustShipHeader` type in `datatypes.h` is still
  HW1's (with `shiptype`) and sits at a different offset in its `ShipStaticInfo`, so decompiled field names through
  `staticinfo->custshipheader` are wrong (example: `processPassiveAttackToDo` shows `CustShipClose`, the raw slot is
  +0x120 = AttackPassive; `univCreateShip` shows `CustShipStaticInit`, the raw load is `[ecx+0x114]` = Init). Use raw
  offsets.
* **145 ship types** in seven race ranges (Sect 0-32, Beast 33-59, Kushan 60-73, Taiidan 74-91, P1 92-97, Traders 98-108,
  Derelict 109-144) instead of HW1's 59 (`TOTAL_NUM_SHIPS`, ShipDefs.h:77) plus derelicts in a separate enum. The
  `.shp` file name is the type name; the 30 `.shp` files that are not type names are derelict/variant statics loaded by
  other paths. `GetShipStaticInfo(type)` takes no race (CET finding, binary-map).
* **`GunStatic` grew from 0xFC to 0x11C**: four new fields after `bulletType` (`StartUpgradeLevel`,
  `EndUpgradeLevel`, `GunSize`, `DamageMult`) and four "second fire mode" fields after `burstWaitTime`
  (`BulletLifeTime2`, `BulletSpeed2`, `BulletRange2`, `FireTime2`). `SoundType`, `BulletType`, `DamageLo`, `DamageHi` are
  still accepted but routed to stub/obsolete callbacks (`cat_scriptSetSoundTypeObsoleteCB` prints "Out Of Data Script
  Entry Found: SoundType") [audit: retail prints nothing; 0x00526050 passes the string to `nullsubReturnFalse` 0x006fe9a0].

## Layout: `ShipStaticInfo` (Cataclysm, 0x458 bytes; in memory, filled from `.shp`)

```c
typedef struct ShipStaticInfo {                 /* offsets proven by the key table unless marked */
/*000*/ StaticHeader staticheader;              /* 0xA4 (HW1 0x98): LOD +0, pMexData +4, mass +8, moments +0xC..+0x14,
                                                   maxvelocity +0x28, maxrot +0x2C, immobile +0x31, rightOfWay +0x32,
                                                   staticCollInfo.collspheresize +0x4C, preciseSelection +0x88,
                                                   collBox* +0x8C..+0xA0  (12 unknown bytes between +0x50 and +0x88) */
/*0A4*/ real32 maxhealth, oneOverMaxHealth, minCollDamage, maxCollDamage;
/*0B4*/ real32 thruststrengthstat[6], rotstrengthstat[6], turnspeedstat[3];      /* to +0xEF */
/*0F0*/ /* teamColor[], salvageStaticInfo, sinbank, pitchturn, pitchdescend: 0x14 bytes, HW1 0x18 (which field
           shrank is UNVERIFIED) */
/*104*/ NAVLightStaticInfo *navlightStaticInfo;  /* InitStatShipInfo: mexGetNAVLightStaticInfo(*(statinfo+0x104)) */
/*108*/ CustShipHeader custshipheader;          /* 0x8C, see below */
/*194*/ void  *custstatinfo;                    /* inferred: between header end and bulletRangeSquared */
/*198*/ real32 bulletRangeSquared[3];           /* InitStatShipInfo squares +0x1A4.. into +0x198.. */
/*1A4*/ real32 bulletRange[3], minBulletRange[3];
/*1BC*/ real32 blastRadiusShockWave, blastRadiusDamage, passiveRetaliateZone;
/*1C8*/ real32 collSideModifiers[6];
/*1E0*/ ShipType shiptype; ShipRace shiprace; ShipClass shipclass;            /* +0x1E0/+0x1E4 written first */
/*1EC*/ udword unknown1EC;                      /* NEW, not scripted */
/*1F0*/ bool8  externalBuild;                   /* NEW (CET declares bool32) */
/*1F4*/ real32 forceFieldProtectionDistanceScale;/* NEW, default -1.0 */
/*1F8*/ real32 upgradeTimeModifier;             /* NEW, default 1.0 */
/*1FC*/ bool8  canBuildShips, canReceiveShips, canReceiveResources, canReceiveShipsPermanently,
               canReceiveShipsForRetire, canReceiveSomething, canBuildBigShips, canTargetMultipleTargets,
               rotateToRetaliate, canReceiveTheseShips[4];                     /* +0x1FC..+0x208 */
/*20C*/ bool   passiveAttackPenaltyExempt;
/*210*/ bool8  specialActivateIsContinuous, canSpecialBandBoxFriendlies;
/*212*/ bool8  newFlag212 /* default 1 */, newFlag213 /* default 0 */;        /* NEW, not scripted */
/*214*/ bool8  canSingleClickSpecialActivate;
/*216*/ sword  maxDockableFighters, maxDockableCorvettes;
/*21C*/ sdword canHandleNumShipsDocking, canHandleNumShipsDepositingRU;
/*224*/ bool   repairBeamCapable;
/*228*/ real32 healthPerSecond, CapitalDistanceRepairStart2, CapitalDistanceRepairStart, AngleDotProdThreshold;
/*238*/ sdword buildCost, buildTime;
/*240*/ real32 fowRadius;                       /* NEW */
/*244*/ sdword svManeuverability, svCoverage, svFirePower;
/*250*/ real32 RKOEatMeTime;                    /* NEW */
/*254*/ sdword resourcesAtOneTime, maxresources, harvestRate, harvestAmount, groupSize;
/*268*/ real32 maxfuel, lowfuelpoint, fuelburnrate, attackfuelburnrate;      /* fields kept, keys dropped (CET names) */
/*278*/ real32 repairTime, repairDamage, repairCombatTime, repairCombatDamage;
/*288*/ real32 pumpFuelRate;                    /* field kept, key dropped */
/*28C*/ real32 repairOtherShipRate, repairResourceCollectorRate, clearanceDistance;
/*298*/ udword clearanceDirection;
/*29C*/ real32 dockShipRange, formationPaddingModifier;
/*2A4*/ bool   cannotForceAttackIfOwnShip, shipIsCapital, cantMoveAndAttack;
/*2B0*/ udword unknown2B0[6];                   /* NEW 24 bytes, UNVERIFIED */
/*2C8*/ vector engineNozzleOffset[4];           /* inferred from trailStatic position */
/*2F8*/ trailstatic *trailStatic[4];            /* InitStatShipInfo: trailStaticInfoParse x4 -> +0x2F8 */
/*308*/ udword multipleEngines;                 /* set from mexGetChunk("Eng1") */
/*30C*/ real32 scaleCap;                        /* key "N-LIPS" */
/*310*/ sdword hierarchySize;
/*314*/ madstatic *madStatic;                   /* tested after the BindGun pass */
/*318*/ real32 madGunOpenDamagedHealthThreshold, madWingOpenDamagedHealthThreshold;
/*320*/ udword repairDroids;                    /* NEW key; HW1 had it commented out (spaceobj.h "nRepairDroids") */
/*324*/ udword trailStyle[4]; real32 trailWidth[4], trailHeight[4], trailAngle[4], trailRibbonAdjust[4],
        trailLength[4], trailScaleCap[4], trailSpriteRadius[4], trailSpriteOffset[4];  /* to +0x3B3 */
/*3B4*/ real32 minimumZoomDistance, renderlistFade, renderlistLimitSqr, dockLightNear, dockLightFar;
/*3C8*/ color  dockLightColor, hyperspaceColor;
/*3D0*/ real32 hyperspaceBoxScale;              /* NEW */
/*3D4*/ udword leechRUsPerSecond;               /* NEW, default 1 */
/*3D8*/ real32 leechHPsPerSecond;               /* NEW, default 20.0 */
/*3DC*/ real32 beastAttackCascadeCost;          /* NEW, default 1.0 */
/*3E0*/ sdword beastAttacksRequired;            /* NEW, default -1 */
/*3E4*/ ShipType beastAttackConvertTo;          /* NEW, default 0x31 (bWorker); cat_scriptSetBeastAttackConvertToCB */
/*3E8*/ real32 tacticalSelectionScale;          /* key "Overlay_Selection_Scalar", default 1.0 */
/*3EC*/ GunStaticInfo *gunStaticInfo;           /* InitStatShipInfo after scriptSetGunStatics */
/*3F0*/ DockStaticInfo *dockStaticInfo;         /* after scriptSetDockStatics */
/*3F4*/ DockOverideInfo *dockOverideInfo;       /* inferred (between dock and resNozzle) */
/*3F8*/ ResNozzleStatic *resNozzleStatic;       /* mexGetResNozzleStatic when ShipCanHarvest */
/*3FC*/ RepairNozzleStatic *repairNozzleStatic; /* mexGetRepairNozzleStatic when repairBeamCapable */
/*400*/ void *unknown400, *unknown404;          /* tractorEmitterStatic / dock callbacks? UNVERIFIED */
/*408*/ bool   bUseRayTriangleIntersectionForHitLocation;  /* NEW, default TRUE */
/*40C*/ real32 linkDistance;                    /* NEW */
/*410*/ udword supportUnits, providesSU;        /* NEW */
/*418*/ real32 RamDamage, cuttingBeamDamagePerSecond, cuttingBeamUpgradedDamagePerSecond, RamPushTime,
               RamWaitAfterPushTime;            /* NEW */
/*42C*/ udword unknown42C;
/*430*/ real32 TimeShockedByEMP, EMP_Range, EMP_Max_Combine_Distance;     /* NEW */
/*43C*/ bool   UpgradeByDocking;                /* NEW */
/*440*/ real32 pilotViewOffsetX, pilotViewOffsetY, pilotViewOffsetZ;      /* NEW */
/*44C*/ udword unknown44C, unknown450;
/*454*/ udword RKOGetsMissiles;                 /* NEW (byte-cleared at init) */
} ShipStaticInfo;                               /* 0x458 */
```

## `CustShipHeader` (Cataclysm, 0x8C bytes) and its slots

Slot k lives at `staticinfo + 0x108 + 4k`. "Callers" are the functions that contain a `call` through that offset
(objdump census); "targets" are the most common Round-5 names of the function pointers stored in the 52 distinct headers.

| slot | +off | Cataclysm meaning | HW1 slot | callers (exe) | targets (examples) |
|---|---|---|---|---|---|
| 0 | 0x108 | `sizeofShipSpecifics` (data) | 1 (HW1 slot 0 = `shiptype`, gone) | - | 24 GenericInterceptor, 92 Acolyte, 316 Mimic, 496 Mothership |
| 1 | 0x10C | StaticInit | StaticInit | InitStatShipInfo | GenericInterceptorStaticInit, AcolyteStaticInit |
| 2 | 0x110 | StaticInitPost | StaticInitPost | none found | NULL in all 51 headers |
| 3 | 0x114 | Init | Init | univCreateShip (0x542a6a) | GenericInterceptorInit, MiningBaseInit |
| 4 | 0x118 | Close | Close | univFreeShipContents | LeechClose, RammingFrigateClose |
| 5 | 0x11C | Fire | Fire (HW1 slot 5 after Attack) | GenericInterceptorAttack, MultiBeamFrigateAttack, doKamikazeAttack, nisFire | DefaultShipFire (22 headers) |
| 6 | 0x120 | AttackPassive | AttackPassive | processPassiveAttackToDo, processAttackToDo, delegateCommand | P1MothershipAttackPassive, RKOAttackPassive |
| 7 | 0x124 | SpecialActivate | SpecialActivate | processSpecialToDo | CloakedFighterSpecialActivate, kasfSpecialOff |
| 8 | 0x128 | SpecialTarget | SpecialTarget | processSpecialToDo | MothershipSpecialTarget, MimicSpecialTarget |
| 9 | 0x12C | **new**: order changed | - | RemoveShipsFromDoingStuff, FreeCommandToDoContents, FreeLastOrder | MothershipOrderChanged, LeechOrderChanged |
| 10 | 0x130 | Housekeep | Housekeep | univUpdateAllPosVelShips, univSetupShipsForControl | CloakedFighterHouseKeep |
| 11 | 0x134 | RemoveShipReferences | RemoveShipReferences | RemoveShipFromBeingTargeted, univRemoveAllShipReferencesFromSpecialShip | AcolyteRemoveShipReferences |
| 12 | 0x138 | **new**: remove object references | - | cat_univRemoveObjReferencesFromShips, cat_univShipDeferredUpdate | same targets as slot 11 in shipped headers |
| 13 | 0x13C | **new**: effect removal | - | cat_univRemoveEffectReferencesFromShips | MothershipEffectRemoval, ProcessorEffectRemoval |
| 14 | 0x140 | **new**: bullet removal | - | cat_univRemoveBulletReferencesFromShips, univFreeBulletContents, univUpdateAllPosVelBullets | CarrierBulletRemoval |
| 15 | 0x144 | Died | Died | univDeleteDeadShip | MimicDied, LeechDied |
| 16 | 0x148 | _PreFix | _PreFix | SaveShip | Mothership_PreFix |
| 17 | 0x14C | _Save | _Save | SaveShip | Mothership_Save |
| 18 | 0x150 | _Load | _Load | LoadShip | Mothership_Load |
| 19 | 0x154 | _Fix | _Fix | FixShip, aitExecute | Acolyte_Fix |
| 20 | 0x158 | **new**: move command | - | clMove, aitShipDied | ACVMoveCommand, MothershipMoveCommand |
| 21 | 0x15C | **Attack** (moved) | Attack (HW1 slot 4) | processAttackToDo, delegateCommand, aitDestroy | GenericInterceptorAttack, MissileDestroyerAttack (10) |
| 22 | 0x160 | **new**: dock command | - | clDock, aitResourceDied | HiveFrigateDockCommand |
| 23 | 0x164 | **new**: launch command | - | clLaunchShip | NULL in all headers |
| 24-26 | 0x168-0x170 | unused | - | none found | NULL (slot 26: kasfSpecialOff in one header) |
| 27 | 0x174 | **new**: halt command | - | clHalt | MCVHaltCommand, AcolyteHaltCommand |
| 28-33 | 0x178-0x18C | unused | - | none found | NULL (slot 29: MimicSlot74Command in one header) |
| 34 | 0x190 | **new**: formation command | - | clFormation | SentinelFormationCommand |

HW1 for comparison (`spaceobj.h:521-540`): `shiptype, sizeofShipSpecifics, StaticInit, StaticInitPost, Init, Close,
Attack, Fire, AttackPassive, SpecialActivate, SpecialTarget, Housekeep, RemoveShipReferences, Died, _PreFix, _Save,
_Load, _Fix`.

## `GunStatic` (Cataclysm 0x11C, HW1 0xFC)

| field / key | HW1 | Cataclysm | note |
|---|---|---|---|
| `Type` (guntype) | +0x00 | +0x00 | |
| `SoundType` / `BulletType` / `DamageLo` / `DamageHi` | +0x04 / +0x08 / +0xBC / +0xC0 | table entries with offset 0, stub callbacks | obsolete keys, values ignored |
| `StartUpgradeLevel`, `EndUpgradeLevel` | - | +0x0C, +0x10 | **new**, `cat_scriptSetWeaponTypeCB` |
| `GunSize` | - | +0x14 | **new**, `cat_scriptSetGunSizeCB` (`GUNSIZE_*`) |
| `DamageMult` | - | +0x18 | **new** |
| `MinAngle`, `MaxAngle` | +0x2C, +0x30 | +0x3C, +0x40 | +0x10 |
| `minturnangle` .. `declinationtracking` | +0x7C..+0x98 | +0x8C..+0xA8 | +0x10 |
| `MaxMissiles`, `TriggerHappy` | +0x9C, +0xA0 | +0xAC, +0xB0 | +0x10 |
| `BulletLength` .. `BurstWaitTime` | +0xC4..+0xE0 | +0xD4..+0xF0 | +0x10 |
| `BulletLifeTime2`, `BulletSpeed2`, `BulletRange2`, `FireTime2` | - | +0xF4, +0xF8, +0xFC, +0x100 | **new** second fire mode |
| `BarrelLength`, `RecoilLength`, `OffsetX/Y/Z`, `SlaveDriver` | +0xE4..+0xF8 | +0x104..+0x118 | +0x20 |
| size | 0xFC | 0x11C | `InitStatShipInfo` gun loop stride 0x17*12+8 = 0x11C |

## 145 ship types

`shiptypeinfo` (0x00889530, `{ShipType, char *name}` x 145, NULL row at 0x008899b8) gives type = index, in race ranges
from `FirstShipTypeOfRace`/`LastShipTypeOfRace`:

| race | types | names |
|---|---|---|
| Sect | 0-32 (33) | 0 sRecon, 1 sAcolyte, 2 sMimic, 3 sLeech, 4 sACV, 5 sMCV, 6 sRammingFrigate, 7 sMultiBeamFrigate, 8 sHiveFrigate, 9 sSwarmer, 10 sDreadnought, 11 sDreadnoughtCannon, 12 sCarrier, 13 sCarrierSupport, 14 sWorker, 15 sRepairDroid, 16 sProcessor, 17 sSentinel, 18 sMothership, 19 sMothershipSpecial, 20 sMothershipEngineering, 21 sMothershipArmour, 22 sMothershipMicro, 23 sMothershipWeapons, 24 sMothershipDockingBay, 25 sDestroyer, 26 sMothershipSupport, 27 sMothershipCommand, 28 sMothershipEnginesLower, 29 sMothershipEnginesUpper, 30 sMothershipLowerSpine, 31 sMothershipBigGun, 32 sMothershipOreCanArmour |
| Beast | 33-59 (27) | 33 bRecon, 34 bAcolyte, 35 bInterceptor, 36 bCloakedFighter, 37 bAttackBomber, 38 bACV, 39 bMultiGunCorvette, 40 bHeavyCorvette, 41 bMultiBeamFrigate, 42 bHiveFrigate, 43 bSwarmer, 44 bIonArrayFrigate, 45 bDFGFrigate, 46 bHeavyCruiser, 47 bCarrier, 48 bRammingFrigate, 49 bWorker, 50 bRepairDroid, 51 bProcessor, 52 bSentinel, 53 bMothership, 54 bMothershipDockingBay, 55 bMothershipEnginesLower, 56 bSectMoship, 57 bMothershipSupport, 58 bCruiseMissile, 59 bMissileCorvette |
| Kushan | 60-73 (14) | 60 kAdvanceSupportFrigate, 61 kAttackBomber, 62 kCarrier, 63 kCloakGenerator, 64 kGravWellGenerator, 65 kHeavyCorvette, 66 kHeavyCruiser, 67 kHeavyDefender, 68 kHeavyInterceptor, 69 kIonCannonFrigate, 70 kMultiGunCorvette, 71 kProximitySensor, 72 kSalCapCorvette, 73 kStandardDestroyer |
| Taiidan | 74-91 (18) | 74 tAdvanceSupportFrigate, 75 tAttackBomber, 76 tCarrier, 77 tCloakGenerator, 78 tDFGFrigate, 79 tGravWellGenerator, 80 tHeavyCruiser, 81 tHeavyDefender, 82 tHeavyInterceptor, 83 tIonCannonFrigate, 84 tMinelayerCorvette, 85 tMissileDestroyer, 86 tMultiGunCorvette, 87 tProximitySensor, 88 tResourceCollector, 89 tSalCapCorvette, 90 tStandardDestroyer, 91 tStandardFrigate |
| P1 | 92-97 (6) | 92 p1Outpost, 93 p1Fighter, 94 p1IonArrayFrigate, 95 p1MissileCorvette, 96 p1Mothership, 97 p1StandardCorvette |
| Traders | 98-108 (11) | 98 trFloatingCity, 99 trCargoBarge, 100 trMiningBase, 101 trResearchStation, 102 trJunkYardDawg, 103 trJunkYardHQ, 104 trGhostship, 105 trJunk_LGun, 106 trJunk_SGun, 107 trResearchStationBridge, 108 trResearchStationTower |
| Derelict | 109-144 (36) | 109 dRKO, 110 dBigGun, 111 dBigGunUnit, 112 dSlipgateSmall, 113 dSlipgateMedium, 114 dSlipgateLarge, 115 dCrate, 116 dJunk1PartA, 117 dJunk1Shell, 118 dJunk1Strut, 119 dJunk2Shipwreck, 120 dJunk3Boiler, 121 dJunk3BoilerCasing, 122 dMessagePod, 123 dBuoy, 124 dFirstPlanet, 125 dDeadPlanet, 126 dCrystal0, 127 dCrystal1, 128 dBeastDockingBay, 129 dNISBuoy, 130 dBeastMothership, 131 dNomadMoon, 132 dNomadFieldEmitter, 133 dAcolyte, 134 dPrisonShip, 135 dPrisonShipInfected, 136 dSlipgateNode, 137 dKushanCarrier, 138 dIonArrayFrigate, 139 dIonCannonFrigate, 140 dEcclesiam, 141 dEcclesiamInfected, 142 nisLightSource1, 143 nisLightSource2, 144 nisLensflareSource1 |

HW1 had 59 ship types (`ShipDefs.h:17-77`, `TOTAL_NUM_SHIPS = TRADERS_LAST_SHIP+1`) split into per-race static arrays
(`ShipStaticInfoR1/R2/P1/P2/P3/Traders`, universe.c:199-204) with derelicts in their own enum. Cataclysm uses one flat
145-entry array; `GetShipStaticInfo` 0x0053ea50 indexes it by type alone.

## Loader control flow (`InitStatShipInfo(statinfo, type, race)` 0x0053bab0)

1. Defaults for the new fields (`+0x408 = 1`, `+0x1F4 = -1.0`, `+0x1F8 = 1.0`, `+0x3D4 = 1`, `+0x3D8 = 20.0`,
   `+0x320 = 0`, `+0x240 = 0`, `+0x40C = 0`, `+0x1F0 = 0`), `shiptype`/`shiprace` at +0x1E0/+0x1E4, copy the
   0x8C-byte `CustShipHeader` from `shipHeaders[type]` to +0x108.
2. Build `<race dir>\<ShipTypeToStr(type)>.shp`; if it does not exist, or a global skip flag (0x00a6c5b8) is set,
   `SetInfoNeededForShipAndRelatedStaticInfo(type, race, 0)`, `rmEnableShip(race, type, 0)` and return.
3. More defaults (formation padding: strike craft get `STRIKECRAFT_PADDING_MODIFIER`, a 25-type switch; trail
   arrays; `preciseSelection = 0xFF`; `collSideModifiers` = 0; `rightOfWay` from a per-type byte table 0x008b1dc0;
   Beast defaults; flags +0x211..+0x213).
4. `scriptSetStruct(dir, file, ShipStaticScriptTable, statinfo)`.
5. Fix-ups as HW1 (repair rates, `cat_univInitShipTypeFlags`, side modifiers, single-player build-cost overrides for
   types 0x1F/0x2C/0x2E, blast radius, retaliation zone, `canReceiveSomething`, 4 x `trailStaticInfoParse`,
   `multipleEngines` from the MEX `Eng1` chunk, `precalcStaticHeader`, MiningBase hack, collision damage defaults,
   speed modifiers, `aishipPrecalcInfo`, engine nozzles).
6. `scriptSetDockStatics`, `scriptSetDockOverideStatics`, MEX dock/res-nozzle/repair-nozzle lookups,
   `scriptSetNAVLightStatics`, `scriptSetGunStatics` (+ MEX gun info), `bulletRangeSquared`, then the `BindGun` and MAD
   tables, then `CustShipStaticInit` (slot 1) near the end.

## Diff vs HW1

| Field / key / branch | HW1 (`ad1bb82`) | Cataclysm | Evidence |
|---|---|---|---|
| `sizeof(ShipStaticInfo)` | 0x370 | 0x458 | clang layout of ORIG; `ShipStaticInfos` 145 x 0x458 (binary-map) |
| `StaticHeader` | 0x98 | 0xA4 (+12 inside `staticCollInfo` before `preciseSelection`) | key offsets: `collspheresize` +0x4C both, `preciseSelection` 0x7C -> 0x88 |
| `CustShipHeader` | 0x48, `shiptype` first, 16 slots | 0x8C, no `shiptype`, 34 slots | `InitStatShipInfo` copies 0x23 dwords; header dump; call census |
| `CustShipAttack` | slot 4 (+0x18 in header [audit: was "+0x10"; HW1 header is shiptype +0, sizeofShipSpecifics +4, StaticInit +8, StaticInitPost +0xC, Init +0x10, Close +0x14, Attack +0x18 (spaceobj.h:522-528); "slot 4" is the 0-based function-pointer index]) | slot 21 (+0x15C in statinfo) | callers `processAttackToDo`, `delegateCommand`; targets `*Attack` |
| New slots | - | 9 order-changed, 12 obj refs, 13 effect refs, 14 bullet refs, 20 move, 22 dock, 23 launch, 27 halt, 34 formation | call census (table above) |
| ShipType count | 59 + derelict enum | 145, one enum, 7 race ranges | `shiptypeinfo` 0x00889530, 0x008894e8/0x00889504 |
| Per-race arrays | `ShipStaticInfoR1/R2/P1/P2/P3/Traders` | one array `ShipStaticInfos` | `GetShipStaticInfo` 0x0053ea50 |
| `.shp` keys | 158 entries, 157 distinct keys in `ShipStaticScriptTable` | 178; 29 new, 8 dropped | table 0x008b2428 vs universe.c:495-665 |
| Fuel keys | scripted | keys gone, fields kept (+0x268.., +0x288) | offsets of neighbours unchanged relative to each other |
| `scaleCap` key | `scaleCap` and `N-LIPS` | `N-LIPS` only (+0x30C) | table 0x008b2428 |
| `NumRepairDroids` / `scaleFactor` | `NumRepairDroids` -> commented field; `scaleFactor` under `SO_CLOOGE_SCALE` (off in release) | `repairDroids` key at +0x320; `scaleFactor` gone | table; spaceobj.h |
| Unsigned callbacks | `scriptSetSdwordCB`, `scriptSetSwordCB`, `scriptSetSbyteCB` | `scriptSetUdwordCB`, `scriptSetUwordCB`, `scriptSetUbyteCB` in the table | probably identical-code folding by the linker (same bodies), UNVERIFIED |
| `GunStatic` | 0xFC | 0x11C, 8 new keys, 4 obsolete | gun table 0x008975a0; stride in `InitStatShipInfo` |
| `GetShipStaticInfo` | `(type, race)` | `(type)` | CET (cet-knowledge.md) |
| Declared counts vs lines | count wins | count wins (3 shipped files disagree) | probe |

## Every key in `ShipStaticScriptTable` (0x008b2428), Cataclysm offset vs HW1

| Cat offset | key | callback | HW1 offset | shift / note |
|---|---|---|---|---|
| 0x000 | `LODFile` | LODScriptLoad | 0x000 |  |
| 0x004 | `pMexData` | loadMexDataCB | 0x004 |  |
| 0x008 | `mass` | scriptSetReal32CB | 0x008 |  |
| 0x00c | `momentOfInertiaX` | scriptSetReal32CB | 0x00c |  |
| 0x010 | `momentOfInertiaY` | scriptSetReal32CB | 0x010 |  |
| 0x014 | `momentOfInertiaZ` | scriptSetReal32CB | 0x014 |  |
| 0x028 | `maxvelocity` | scriptSetReal32CB | 0x028 |  |
| 0x02c | `maxrot` | scriptSetReal32CB | 0x02c |  |
| 0x031 | `immobile` | scriptSetBool8 | 0x031 |  |
| 0x032 | `rightOfWay` | scriptSetUbyteCB | 0x032 |  |
| 0x040 | `approxcollmodifier` | scriptSetReal32CB | 0x040 |  |
| 0x044 | `avoidcollmodifier` | scriptSetReal32CB | 0x044 |  |
| 0x04c | `collspheresize` | scriptSetReal32CB | 0x04c |  |
| 0x088 | `preciseSelection` | scriptSetUdwordCB | 0x07c | +12 |
| 0x08c | `collBoxScaleForward` | scriptSetReal32CB | 0x080 | +12 |
| 0x090 | `collBoxScaleUp` | scriptSetReal32CB | 0x084 | +12 |
| 0x094 | `collBoxScaleRight` | scriptSetReal32CB | 0x088 | +12 |
| 0x098 | `collBoxOffsetX` | scriptSetReal32CB | 0x08c | +12 |
| 0x09c | `collBoxOffsetY` | scriptSetReal32CB | 0x090 | +12 |
| 0x0a0 | `collBoxOffsetZ` | scriptSetReal32CB | 0x094 | +12 |
| 0x0a4 | `maxhealth` | scriptSetReal32CB | 0x098 | +12 |
| 0x0ac | `minCollDamage` | scriptSetReal32CB | 0x0a0 | +12 |
| 0x0b0 | `maxCollDamage` | scriptSetReal32CB | 0x0a4 | +12 |
| 0x0b4 | `thruststrength[TRANS_UP]` | scriptSetReal32CB | 0x0a8 | +12 |
| 0x0b8 | `thruststrength[TRANS_DOWN]` | scriptSetReal32CB | 0x0ac | +12 |
| 0x0bc | `thruststrength[TRANS_RIGHT]` | scriptSetReal32CB | 0x0b0 | +12 |
| 0x0c0 | `thruststrength[TRANS_LEFT]` | scriptSetReal32CB | 0x0b4 | +12 |
| 0x0c4 | `thruststrength[TRANS_FORWARD]` | scriptSetReal32CB | 0x0b8 | +12 |
| 0x0c8 | `thruststrength[TRANS_BACK]` | scriptSetReal32CB | 0x0bc | +12 |
| 0x0cc | `rotstrength[ROT_YAWLEFT]` | scriptSetReal32CB | 0x0c0 | +12 |
| 0x0d0 | `rotstrength[ROT_YAWRIGHT]` | scriptSetReal32CB | 0x0c4 | +12 |
| 0x0d4 | `rotstrength[ROT_PITCHUP]` | scriptSetReal32CB | 0x0c8 | +12 |
| 0x0d8 | `rotstrength[ROT_PITCHDOWN]` | scriptSetReal32CB | 0x0cc | +12 |
| 0x0dc | `rotstrength[ROT_ROLLRIGHT]` | scriptSetReal32CB | 0x0d0 | +12 |
| 0x0e0 | `rotstrength[ROT_ROLLLEFT]` | scriptSetReal32CB | 0x0d4 | +12 |
| 0x0e4 | `turnspeed[TURN_YAW]` | scriptSetReal32CB | 0x0d8 | +12 |
| 0x0e8 | `turnspeed[TURN_PITCH]` | scriptSetReal32CB | 0x0dc | +12 |
| 0x0ec | `turnspeed[TURN_ROLL]` | scriptSetReal32CB | 0x0e0 | +12 |
| 0x1a4 | `bulletRange[0]` | scriptSetReal32CB | 0x158 | +76 |
| 0x1a8 | `bulletRange[1]` | scriptSetReal32CB | 0x15c | +76 |
| 0x1ac | `bulletRange[2]` | scriptSetReal32CB | 0x160 | +76 |
| 0x1b0 | `minBulletRange[0]` | scriptSetReal32CB | 0x164 | +76 |
| 0x1b4 | `minBulletRange[1]` | scriptSetReal32CB | 0x168 | +76 |
| 0x1b8 | `minBulletRange[2]` | scriptSetReal32CB | 0x16c | +76 |
| 0x1bc | `blastRadiusShockWave` | scriptSetReal32CB | 0x170 | +76 |
| 0x1c0 | `blastRadiusDamage` | scriptSetReal32CB | 0x174 | +76 |
| 0x1c4 | `passiveRetaliateZone` | scriptSetReal32CB | 0x178 | +76 |
| 0x1c8 | `collSideModifiers[TRANS_UP]` | scriptSetReal32CB | 0x17c | +76 |
| 0x1cc | `collSideModifiers[TRANS_DOWN]` | scriptSetReal32CB | 0x180 | +76 |
| 0x1d0 | `collSideModifiers[TRANS_RIGHT]` | scriptSetReal32CB | 0x184 | +76 |
| 0x1d4 | `collSideModifiers[TRANS_LEFT]` | scriptSetReal32CB | 0x188 | +76 |
| 0x1d8 | `collSideModifiers[TRANS_FORWARD]` | scriptSetReal32CB | 0x18c | +76 |
| 0x1dc | `collSideModifiers[TRANS_BACK]` | scriptSetReal32CB | 0x190 | +76 |
| 0x1e8 | `shipclass` | setClassCB | 0x19c | +76 |
| 0x1f0 | `externalBuild` | scriptSetBool8 | - | **new** |
| 0x1f4 | `forceFieldProtectionDistanceScale` | scriptSetReal32CB | - | **new** |
| 0x1f8 | `upgradeTimeModifier` | scriptSetReal32CB | - | **new** |
| 0x1fc | `canBuildShips` | scriptSetBool8 | 0x1a0 | +92 |
| 0x1fd | `canReceiveShips` | scriptSetBool8 | 0x1a1 | +92 |
| 0x1fe | `canReceiveResources` | scriptSetBool8 | 0x1a2 | +92 |
| 0x1ff | `canReceiveShipsPermanently` | scriptSetBool8 | 0x1a3 | +92 |
| 0x200 | `canReceiveShipsForRetire` | scriptSetBool8 | 0x1a4 | +92 |
| 0x202 | `canBuildBigShips` | scriptSetBool8 | 0x1a6 | +92 |
| 0x203 | `canTargetMultipleTargets` | scriptSetBool8 | 0x1a7 | +92 |
| 0x204 | `rotateToRetaliate` | scriptSetBool8 | 0x1a8 | +92 |
| 0x205 | `canReceiveFighters` | scriptSetBool8 | 0x1a9 | +92 |
| 0x206 | `canReceiveCorvettes` | scriptSetBool8 | 0x1aa | +92 |
| 0x207 | `canReceiveResourceCollectors` | scriptSetBool8 | 0x1ab | +92 |
| 0x208 | `canReceiveOtherNonCapitalShips` | scriptSetBool8 | 0x1ac | +92 |
| 0x20c | `passiveAttackPenaltyExempt` | scriptSetBool | 0x1b0 | +92 |
| 0x210 | `specialActivateIsContinuous` | scriptSetBool8 | 0x1b4 | +92 |
| 0x211 | `canSpecialBandBoxFriendlies` | scriptSetBool8 | 0x1b5 | +92 |
| 0x214 | `canSingleClickSpecialActivate` | scriptSetBool8 | 0x1b6 | +94 |
| 0x216 | `maxDockableFighters` | scriptSetUwordCB | 0x1b8 | +94 |
| 0x218 | `maxDockableCorvettes` | scriptSetUwordCB | 0x1ba | +94 |
| 0x21c | `canHandleNumShipsDocking` | scriptSetUdwordCB | 0x1bc | +96 |
| 0x220 | `canHandleNumShipsDepositingRU` | scriptSetUdwordCB | 0x1c0 | +96 |
| 0x224 | `repairBeamCapable` | scriptSetBool | 0x1c4 | +96 |
| 0x228 | `healthPerSecond` | scriptSetReal32CB | 0x1c8 | +96 |
| 0x22c | `CapitalDistanceRepairStart2` | scriptSetReal32CB | 0x1cc | +96 |
| 0x230 | `CapitalDistanceRepairStart` | scriptSetReal32CB | 0x1d0 | +96 |
| 0x234 | `AngleDotProdThreshold` | scriptSetReal32CB | 0x1d4 | +96 |
| 0x238 | `buildCost` | scriptSetUdwordCB | 0x1d8 | +96 |
| 0x23c | `buildTime` | scriptSetFramesCB | 0x1dc | +96 |
| 0x240 | `fowRadius` | scriptSetReal32CB | - | **new** |
| 0x244 | `svManeuverability` | scriptSetUdwordCB | 0x1e0 | +100 |
| 0x248 | `svCoverage` | scriptSetUdwordCB | 0x1e4 | +100 |
| 0x24c | `svFirePower` | scriptSetUdwordCB | 0x1e8 | +100 |
| 0x250 | `RKOEatMeTime` | scriptSetReal32CB | - | **new** |
| 0x254 | `resourcesAtOneTime` | scriptSetUdwordCB | 0x1ec | +104 |
| 0x258 | `maxresources` | scriptSetUdwordCB | 0x1f0 | +104 |
| 0x25c | `harvestRate` | scriptSetUdwordCB | 0x1f4 | +104 |
| 0x260 | `harvestAmount` | scriptSetUdwordCB | 0x1f8 | +104 |
| 0x264 | `groupSize` | scriptSetUdwordCB | 0x1fc | +104 |
| 0x278 | `repairTime` | scriptSetReal32CB | 0x210 | +104 |
| 0x27c | `repairDamage` | scriptSetReal32CB | 0x214 | +104 |
| 0x280 | `repairCombatTime` | scriptSetReal32CB | 0x218 | +104 |
| 0x284 | `repairCombatDamage` | scriptSetReal32CB | 0x21c | +104 |
| 0x28c | `repairOtherShipRate` | scriptSetReal32CB | 0x224 | +104 |
| 0x290 | `repairResourceCollectorRate` | scriptSetReal32CB | 0x228 | +104 |
| 0x294 | `clearanceDistance` | scriptSetReal32CB | 0x22c | +104 |
| 0x298 | `clearanceDirection` | scriptSetUdwordCB | 0x230 | +104 |
| 0x29c | `dockShipRange` | scriptSetReal32CB | 0x234 | +104 |
| 0x2a0 | `formationPaddingModifier` | scriptSetReal32CB | 0x238 | +104 |
| 0x2a4 | `cannotForceAttackIfOwnShip` | scriptSetBool | 0x23c | +104 |
| 0x2a8 | `isCapitalShip` | scriptSetBool | 0x240 | +104 |
| 0x2ac | `cantMoveAndAttack` | scriptSetBool | 0x244 | +104 |
| 0x30c | `N-LIPS` | scriptSetReal32CB | 0x28c | +128 |
| 0x318 | `madGunOpenDamagedHealthThreshold` | scriptSetReal32CB | 0x298 | +128 |
| 0x31c | `madWingOpenDamagedHealthThreshold` | scriptSetReal32CB | 0x29c | +128 |
| 0x320 | `repairDroids` | scriptSetUdwordCB | - | **new** |
| 0x324 | `trailStyle` | scriptSetUdwordCB | 0x2a0 | +132 |
| 0x328 | `trailStyle2` | scriptSetUdwordCB | 0x2a4 | +132 |
| 0x32c | `trailStyle3` | scriptSetUdwordCB | 0x2a8 | +132 |
| 0x330 | `trailStyle4` | scriptSetUdwordCB | 0x2ac | +132 |
| 0x334 | `trailWidth` | scriptSetReal32CB | 0x2b0 | +132 |
| 0x338 | `trailWidth2` | scriptSetReal32CB | 0x2b4 | +132 |
| 0x33c | `trailWidth3` | scriptSetReal32CB | 0x2b8 | +132 |
| 0x340 | `trailWidth4` | scriptSetReal32CB | 0x2bc | +132 |
| 0x344 | `trailHeight` | scriptSetReal32CB | 0x2c0 | +132 |
| 0x348 | `trailHeight2` | scriptSetReal32CB | 0x2c4 | +132 |
| 0x34c | `trailHeight3` | scriptSetReal32CB | 0x2c8 | +132 |
| 0x350 | `trailHeight4` | scriptSetReal32CB | 0x2cc | +132 |
| 0x354 | `trailAngle` | scriptSetReal32CB | 0x2d0 | +132 |
| 0x358 | `trailAngle2` | scriptSetReal32CB | 0x2d4 | +132 |
| 0x35c | `trailAngle3` | scriptSetReal32CB | 0x2d8 | +132 |
| 0x360 | `trailAngle4` | scriptSetReal32CB | 0x2dc | +132 |
| 0x364 | `trailRibbonAdjust` | scriptSetReal32CB | 0x2e0 | +132 |
| 0x368 | `trailRibbonAdjust2` | scriptSetReal32CB | 0x2e4 | +132 |
| 0x36c | `trailRibbonAdjust3` | scriptSetReal32CB | 0x2e8 | +132 |
| 0x370 | `trailRibbonAdjust4` | scriptSetReal32CB | 0x2ec | +132 |
| 0x374 | `trailLength` | scriptSetReal32CB | 0x2f0 | +132 |
| 0x378 | `trailLength2` | scriptSetReal32CB | 0x2f4 | +132 |
| 0x37c | `trailLength3` | scriptSetReal32CB | 0x2f8 | +132 |
| 0x380 | `trailLength4` | scriptSetReal32CB | 0x2fc | +132 |
| 0x384 | `trailScaleCap` | scriptSetReal32CB | 0x300 | +132 |
| 0x388 | `trailScaleCap2` | scriptSetReal32CB | 0x304 | +132 |
| 0x38c | `trailScaleCap3` | scriptSetReal32CB | 0x308 | +132 |
| 0x390 | `trailScaleCap4` | scriptSetReal32CB | 0x30c | +132 |
| 0x394 | `trailSpriteRadius` | scriptSetReal32CB | 0x310 | +132 |
| 0x398 | `trailSpriteRadius2` | scriptSetReal32CB | 0x314 | +132 |
| 0x39c | `trailSpriteRadius3` | scriptSetReal32CB | 0x318 | +132 |
| 0x3a0 | `trailSpriteRadius4` | scriptSetReal32CB | 0x31c | +132 |
| 0x3a4 | `trailSpriteOffset` | scriptSetReal32CB | 0x320 | +132 |
| 0x3a8 | `trailSpriteOffset2` | scriptSetReal32CB | 0x324 | +132 |
| 0x3ac | `trailSpriteOffset3` | scriptSetReal32CB | 0x328 | +132 |
| 0x3b0 | `trailSpriteOffset4` | scriptSetReal32CB | 0x32c | +132 |
| 0x3b4 | `minimumZoomDistance` | scriptSetReal32CB | 0x330 | +132 |
| 0x3b8 | `renderlistFade` | scriptSetReal32CB | 0x334 | +132 |
| 0x3bc | `renderlistLimit` | scriptSetReal32SqrCB | 0x338 | +132 |
| 0x3c0 | `dockLightNear` | scriptSetReal32CB | 0x33c | +132 |
| 0x3c4 | `dockLightFar` | scriptSetReal32CB | 0x340 | +132 |
| 0x3c8 | `dockLightColor` | scriptSetRGBCB | 0x344 | +132 |
| 0x3cc | `hyperspaceColor` | scriptSetRGBCB | 0x348 | +132 |
| 0x3d0 | `hyperspaceBoxScale` | scriptSetReal32CB | - | **new** |
| 0x3d4 | `leechRUsPerSecond` | scriptSetUdwordCB | - | **new** |
| 0x3d8 | `leechHPsPerSecond` | scriptSetReal32CB | - | **new** |
| 0x3dc | `beastAttackCascadeCost` | scriptSetReal32CB | - | **new** |
| 0x3e0 | `beastAttacksRequired` | scriptSetUdwordCB | - | **new** |
| 0x3e4 | `beastAttackConvertTo` | cat_scriptSetBeastAttackConvertToCB | - | **new** |
| 0x3e8 | `Overlay_Selection_Scalar` | scriptSetReal32CB | 0x34c | +156 |
| 0x408 | `bUseRayTriangleIntersectionForHitLocation` | scriptSetBool | - | **new** |
| 0x40c | `linkDistance` | scriptSetReal32CB | - | **new** |
| 0x410 | `supportUnits` | scriptSetUdwordCB | - | **new** |
| 0x414 | `providesSU` | scriptSetUdwordCB | - | **new** |
| 0x418 | `RamDamage` | scriptSetReal32CB | - | **new** |
| 0x41c | `cuttingBeamDamagePerSecond` | scriptSetReal32CB | - | **new** |
| 0x420 | `cuttingBeamUpgradedDamagePerSecond` | scriptSetReal32CB | - | **new** |
| 0x424 | `RamPushTime` | scriptSetReal32CB | - | **new** |
| 0x428 | `RamWaitAfterPushTime` | scriptSetReal32CB | - | **new** |
| 0x430 | `TimeShockedByEMP` | scriptSetReal32CB | - | **new** |
| 0x434 | `EMP_Range` | scriptSetReal32CB | - | **new** |
| 0x438 | `EMP_Max_Combine_Distance` | scriptSetReal32CB | - | **new** |
| 0x43c | `UpgradeByDocking` | scriptSetBool | - | **new** |
| 0x440 | `pilotViewOffsetX` | scriptSetReal32CB | - | **new** |
| 0x444 | `pilotViewOffsetY` | scriptSetReal32CB | - | **new** |
| 0x448 | `pilotViewOffsetZ` | scriptSetReal32CB | - | **new** |
| 0x454 | `RKOGetsMissiles` | scriptSetUdwordCB | - | **new** |

HW1 keys with no Cataclysm table entry: `maxfuel` (HW1 +0x200), `lowfuelpoint` (HW1 +0x204), `fuelburnrate` (HW1 +0x208), `attackfuelburnrate` (HW1 +0x20c), `pumpFuelRate` (HW1 +0x220), `scaleCap` (HW1 +0x28c), `scaleFactor` (HW1 +n/a in release), `NumRepairDroids` (HW1 +n/a in release)

## Probe

`tools/formats/shipstatic_probe.py --exe cataclysm.exe [-v] FILE.shp...` rebuilds the type table, the static-info key
table and the gun table from the exe at run time and checks each `.shp`. `-v` lists every static-info key of the file
with its Cataclysm offset. "other_keys" are keys outside `ShipStaticScriptTable`: per-ship custom tables read by the
ship's `CustShipStaticInit` (e.g. `flightman*`, `repairApproachDistance`; see script.md), MAD keys, or keys the exe
ignores (`explosionType`, `trail2*`).

```
exe: 145 ship types, ShipStaticScriptTable 178 keys (max offset 0x454), gun table 39 keys
OK   type=12 keys=89 staticinfo=76 other=9 guns=4 docks=44 navs=18 other_keys=ANIM,BindGun,NUMBER_OF_ANIMS,explosionType,trail2Height,...  race_sect/scarrier.shp
OK   type=47 keys=75 staticinfo=74 other=1 guns=12 docks=25 navs=8 other_keys=explosionType  race_beast/bcarrier.shp
OK   type=62 keys=76 staticinfo=74 other=2 guns=4 docks=5 navs=5 other_keys=explosionType,repairApproachDistance  race_kushan/kcarrier.shp
FAIL type=0 ... :: NUMBER_OF_NAV_LIGHTS=0 but 8 blocks/lines  race_sect/srecon.shp
FAIL type=104 ... :: NUMBER_OF_NAV_LIGHTS=98 but 99 blocks/lines  race_traders/trghostship.shp
all 173 .shp: 170 OK, 3 count mismatches (srecon, brecon, trghostship; the engine uses the declared count);
30 files are not ShipType names (derelict/variant statics)
```

## Open points

* Unknown Cataclysm fields: +0x1EC, +0x212/+0x213 (flags, defaults 1/0), +0x2B0..+0x2C7, +0x400/+0x404, +0x42C,
  +0x44C/+0x450, the 12 new bytes in `StaticCollInfo` and the 4 bytes lost between +0xF0 and +0x104. Next step: xrefs
  on these offsets in M2-R2.
* `custstatinfo` at +0x194 and `dockOverideInfo` at +0x3F4 are inferred from position, not from an access.
* Slots 24-26 and 28-33 have no caller in the census; slot 26/29 targets in one header each suggest they are reached
  through a path the census misses (for example a computed offset).
