# Homeworld 1 engine: gameplay and simulation layer

Reference map of the HW1 gameplay code (the code Cataclysm forked), written for the engineer decompiling the
Cataclysm executable (GOG "Homeworld: Emergence") and writing the native Windows 11 / Linux port.

## 0. Sources, conventions, caveats

| Prefix used below | Repo path | Version read |
|---|---|---|
| `GoK:` | `reference/gardens-of-kadesh/` | commit `3af31fb` (2026-09-25). Primary source for line numbers. |
| `HWSDL:` | `reference/HomeworldSDL/` | commit `1bed493` (2024-10-13). Used where GoK has been refactored away from the original layout. |
| `CET:` | `reference/CataclysmEngineTweaks/` | Cataclysm Engine Tweaks. Reconstructed Cataclysm headers in `CET:game/Game/`, data in `CET:assets/`, binary addresses in `CET:Cataclysm.h`. |

- "file:N" means line N of that file; "file:N-M" a range.
- **Layout warning.** GoK rewrote the struct inheritance in `GoK:src/Game/SpaceObj.h` as anonymous unions (see the
  `TODO(type safety, 2026-04-24)` comments, e.g. `GoK:src/Game/SpaceObj.h:478-500`). The byte layout is meant to stay the
  same, but when matching against the binary, use `HWSDL:src/Game/SpaceObj.h`. It keeps Relic's style, in which every
  derived struct re-declares the parent fields flat (`HWSDL:src/Game/SpaceObj.h:977-1060`, `:1294-1456`). The CET
  headers use the same style.
- **Tick-rate warning.** Both GoK and HWSDL set `UNIVERSE_UPDATE_RATE_SHIFT = 2`, which gives 64 Hz
  (`GoK:src/Game/Universe.c:75`, `GoK:src/Game/Universe.h:44-53`). The original HW1 and Cataclysm run at **16 Hz**:
  `#define UNIVERSE_UPDATE_RATE 16` (`CET:game/Game/universe.h:57`), and `ETG_UpdateRoundOff = UNIVERSE_UPDATE_PERIOD/2 = 0.03125`
  (`CET:Cataclysm.h`, Globals). With SHIFT=0, the GoK macro `UNIVERSE_WOODPECKER(r,f)` reduces to the original
  `((univUpdateCounter & r) == f)`. Read every WOODPECKER in this document that way. GoK can run at the original
  16 Hz: the `/vanilla` switch (`GoK:src/SDL/main.c:848`) sets `UNIVERSE_UPDATE_RATE_SHIFT = 0` (`main.c:2195-2197`).
  The comment at `GoK:src/Game/Universe.h:46` calls it `/legacy`, which is stale. Use `/vanilla` when diffing GoK
  behaviour against Cataclysm.
- "UNVERIFIED" marks claims that are not grounded in a file read for this document. "Inferred" marks a reasoned conclusion
  drawn from grounded facts.

---

## 1. Summary for the port

1. The simulation is a **fixed-timestep, deterministic, single-threaded tick**, `univUpdate(1/16 s)`
   (`GoK:src/Game/UnivUpdate.c:7271`). It is driven by a cooperative task (`universeUpdateTask`,
   `GoK:src/Game/Universe.c:3786`) that is registered at `UNIVERSE_UPDATE_PERIOD` (`GoK:src/SDL/utility.c:4040`). The
   task scheduler is an accumulator capped at 8 calls per frame (`GoK:src/Game/Task.c:280-351`, `GoK:src/Game/Task.h:50`).
2. The object model is **C struct-prefix inheritance**. `SpaceObj → Rot → RotImp → RotImpTarg → …Guidance →
   ShipOrDerelict → Ship`, each object points at a shared `*StaticInfo` loaded from a `.shp` script, and per-type
   behaviour is a **vtable-like `CustShipHeader`** of 16 function pointers (`GoK:src/Game/SpaceObj.h:523-543`).
3. Orders flow **UI/AI → `clWrap*` → (SP) `cl*` directly | (MP) `clSend*` → captain → sync packet → `cl*`** into one
   global list of `CommandToDo` (`GoK:src/Game/CommandWrap.c:89-123`). `clProcess` runs every tick and dispatches on
   `order` × `attributes` (`GoK:src/Game/CommandLayer.c:8224`).
4. Multiplayer is **lockstep with a "captain"** that merges command packets into sync packets at 8 Hz. Each sync packet
   drives two univUpdates (`GoK:src/Game/Universe.c:3962-3990`, `GoK:src/Game/CommandNetwork.h:126`). Determinism
   depends on the `RANDOM_GAME` RNG stream and identical float math. Desyncs are detected with rand, position-sum and
   stat checksums (`GoK:src/Game/NetCheck.c:918-933`).
5. **Missions are C code.** Each `.kas` script is preprocessed by cpp, compiled to C by `kas2c` (`GoK:tools/kas2c/`)
   and linked into the executable. Only the layout (`MissionNN.mission`) is data
   (`GoK:src/Game/SinglePlayer.c:416-420, 2913-2929`). Cataclysm's missions therefore have to be decompiled out of the
   EXE (§14.6).
6. Cataclysm keeps this architecture but changes the data shapes. Ship types are a flat 145-entry space with race
   prefixes (`CET:game/Game/ShipDefs.h`). `Player` gains support-unit counters (`CET:game/Game/universe.h:110-148`).
   `ShipStaticInfo` gains about 30 new script fields (`CET:game/Game/spaceobj.h:557-758`). `CommandToDo` grows to 0x330
   bytes (`CET:game/Game/CommandLayer.h:109`). A new skirmish AI ("ais*") is added (`CET:game/Game/AISkirmish.h`).
7. The HW1→Cataclysm struct-offset deltas are **constant over long runs**, for example +0x6C across the middle of
   `Ship` and +0x4C across the `bulletRange..shipclass` run of `ShipStaticInfo` (§2.6). This lets HW1 field names be
   mapped onto the binary quickly. The runs step up between groups (`ShipStaticInfo` goes +0x0C → +0x4C → +0x5C →
   +0x60 → +0x68 → +0x80 → +0x84 → +0x9C), and at least one block moved: CET places `Ship.dockvars` after
   `soundevent`, not after `attackvars` (§2.6). Check each run's ends before trusting a delta.

---

## 2. Object model (Universe, SpaceObj, StaticInfo)

### 2.1 Inheritance chain (`GoK:src/Game/SpaceObj.h`)

| Level | Struct (lines) | Adds | Used by |
|---|---|---|---|
| 0 | `SpaceObjHeader` 941-946 | `Node objlink; ObjType objtype; udword flags` (SOF_*) | all |
| 1 | `SpaceObj` 948-963 | `StaticInfo *staticinfo; Node renderlink; currentLOD; attributes/attributesParam` (mission-editor attributes); camera distance; `collOptimizeDist; blob *collMyBlob; PosInfo posinfo` | Bullet, Effect (via Rot) |
| 2 | `SpaceObjRot` 971-984 | `RotInfo rotinfo` (coordsys 3×3, rotspeed, torque) 852-857 | Bullet, Effect |
| 3 | `SpaceObjRotImp` 990-999 | `CollInfo collInfo` (collOffset, collPosition, rectpos[8], precise sel) 841-850; `Node impactablelink` | Resource |
| 4 | `SpaceObjRotImpTarg` 1005-1020 | `health, flashtimer, lasttimecollided` | Resource |
| 5 | `SpaceObjRotImpTargGuidance` 1028-1048 | `NonStats nonstatvars` (per-instance copies of thrust/rot/turn, 504-509); `ClampInfo *clampInfo` | Missile |
| 6 | `ShipOrDerelict` 1050-1075 | `SalvageInfo*; aistate; putOnDoor; dockingship; specialFlags; specialFlags2; salvageNumTagged[8]` | Ship, Derelict |
| 7a | `Ship` 1220-1362 | see §2.3; ends in `udword sizeofShipSpecifics; udword ShipSpecifics[1]` (variable-length tail for the per-type Spec) | ships |
| 7b | `Derelict` 1511-1549 | derelicttype, tractor state, ambient sounds | derelicts |
| — | `Missile` 1375-1418 | owner, target, damage, mine formation fields | missiles, mines |
| — | `Bullet` 1467-1498 (Rot only) | heading, length, damage/damageFull, owner, gunowner, target, `DFGFieldEntryTime` | bullets and beams |
| — | `Effect` 1426-1461 (Rot only) | ETG effect instance | effects |
| — | `Resource` 1566-1584 → `Asteroid` 1592-1609, `Nebula` 1615-1635, `GasCloud` 1641-1659, `DustCloud` 1665-1684 | resourcevalue, `ResourceVolume*` | resources |

`ObjType` enum (`GoK:src/Game/ObjTypes.h:86-97`): `OBJ_ShipType, OBJ_BulletType, OBJ_AsteroidType, OBJ_NebulaType,
OBJ_GasType, OBJ_DustType, OBJ_DerelictType, OBJ_EffectType, OBJ_MissileType`. Cataclysm uses the identical enum
(`CET:game/Game/objtypes.h`).

Key `SOF_*` object flags (`GoK:src/Game/SpaceObj.h:886-939`): `Rotatable 0x1, Impactable 0x2, DontApplyPhysics 0x4,
NISShip 0x80, CloakGenField 0x100, Cloaked 0x200, Cloaking 0x400, DeCloaking 0x800, Dead 0x1000, Selectable 0x8000,
Resource 0x10000, Hide 0x20000, BigObject 0x80000, StaticInfoIsDynamic 0x100000, NotBumpable 0x200000,
Targetable 0x400000, Slaveable 0x800000, Crazy 0x1000000 (dying spin), Disabled 0x4000000, Clamped 0x8000000,
Hyperspace 0x10000000`.

### 2.2 Static info vs instance data

- **Static** (shared per type) is the chain `StaticInfo` 398-418 (LOD, MEX, mass, moments, 1/mass,
  maxvelocity, maxrot, `StaticCollInfo` 361-391) → `StaticInfoHealth` 420-427 → `…Guidance` 464-476 (thrust/rot/turn
  stats) → `ShipStaticInfo` 545-681. That struct holds the `CustShipHeader custshipheader`, `void *custstatinfo`
  (per-type statics), bullet ranges per tactics, class, flags, cost/time, fuel, repair, dock/gun/nav-light static
  tables, trails, and the `ShipXDocksAtShipY`/`LaunchShipXFromShipY` docking callbacks.
- HW1 stores statics in **per-race arrays** `ShipStaticInfoR1/R2/P1/P2/P3/Traders` (`GoK:src/Game/Universe.c:201-216`).
  `GetShipStaticInfo(type, race)` indexes `RaceShipStaticInfos[race][type - FirstShipTypeOfRace[race]]`
  (`GoK:src/Game/Universe.c:4107-4120`, `GoK:src/Game/ObjTypes.c:27-35`). R1 and R2 share the "STD" type range, so one
  type has two statics. **Cataclysm drops the race parameter:** `GetShipStaticInfo(ShipType)` at `0x0053EA50`
  (`CET:Cataclysm.h`), because every Cataclysm type is unique to a race (`sAcolyte` vs `bAcolyte`).
- Loading happens in `InitStatShipInfo` (`GoK:src/Game/Universe.c:1997-2318`):
  1. Copy `*shipHeaders[type]` into `statinfo->custshipheader` (`:2009`).
  2. Build the path `"<race>/<ShipTypeToStr>.shp"` (`:2014-2031`). If the file is missing, disable the type (`:2038-2044`).
  3. Load defaults and then `scriptSetStruct(dir, shp, ShipStaticScriptTable, statinfo)` (`:2115`, table `:498-666`).
  4. Run the dock/gun/navlight/salvage sub-parsers and the MEX lookups (`:2219-2266`).
  5. Call `CustShipStaticInit(dir, shp, statinfo)` last (`:2312-2315`), which parses the per-type keys (§6.3).
  6. `InitStatShipInfoPost` (defined at `:2327`, called from `:3411`) runs later and calls `CustShipStaticInitPost` (`:2339`).
- **Instance**: `univCreateShip` (`GoK:src/Game/UnivUpdate.c:1320-1636`) allocates
  `sizeof(Ship) - 4 + sizeofShipSpecifics`, plus an optional `PreciseSelection` and MAD animation bindings appended to
  the same block (`:1335-1356`). It then sets
  `flags = SOF_Rotatable|Impactable|Selectable|Targetable`, copies the `*stat` thrust/rot/turn into `nonstatvars`
  (`:1386-1398`), and assigns `shipID = universe.shipNumber++` (`:1400-1402`). At `:1559` it calls `CustShipInit`.
  `SOF_StaticInfoIsDynamic` marks an object whose static info was cloned for per-instance changes.

### 2.3 Ship instance highlights (`GoK:src/Game/SpaceObj.h:1220-1362`)

- Movement: `moveTo, moveFrom, formationOffset, formationHeading`, right-of-way fields (`row*`).
- Tactics: `tacticstype, isDodging, DodgeTime, tactics_ordertype, tacticsTalk`.
- Kamikaze and speed burst: `kamikazeState/Vector, speedBurstTime`. Death: `deathtime`, SOF_Crazy.
- MAD animation state: `madGunStatus, madWingStatus, madDoorStatus`.
- Ownership and economy: `playerowner, resources, fuel, lastTimeRepaired`.
- Combat memory: `gettingrocked, recentAttacker, firingAtUs`, with byte countdowns `recentlyAttacked/recentlyFiredUpon`.
  `RECENT_ATTACK_DURATION = 5*UNIVERSE_UPDATE_RATE` (`:1109`); that is 80 at 16 Hz. The macro casts to `ubyte`, so at GoK's 64 Hz
  it wraps to 320 mod 256 = 64 ticks (1 s instead of 5 s).
- Per-tick flags: `shipidle, autostabilizeship`. AI state bytes: `aistate…, flightman*, rcstate*`.
- Commands: `CommandToDo *command, *formationcommand`.
- `SpecificAttackVars attackvars` (1119-1129: target or multi-targets, leader/wingman); `SpecificDockVars dockvars` (1131-1143).
- Visibility: `visibleToWho` bitmask (cloak), `howDidIDie`/`whoKilledMe` (DEATH_* codes 1156-1167).
- Tractor-beam state; `slaveinfo` (master/slave stacking, §10); `gunInfo, dockInfo, shipsInsideMe`.
- `specialFlags` bits `SPECIAL_*` (1173-1203) and `specialFlags2` `SPECIAL_2_*` (1207-1215).

### 2.4 Containers and IDs

- Intrusive doubly linked lists: `Node{next, prev, belongto, structptr}` and `LinkedList{head, tail, num}`
  (`GoK:src/Game/LinkedList.h`). The pattern is `listGetStructOfNode(node)` → owning struct. An object belongs to
  several lists at once through separate `Node`s (`objlink`, `renderlink`, `impactablelink`, `shiplink`, `slavelink`).
- Universe lists (`GoK:src/Game/Universe.h:140-163`): `SpaceObjList, MinorSpaceObjList, effectList, ShipList,
  BulletList, ResourceList, DerelictList, ImpactableList, MissileList, MineFormationList`; deferred-death
  `DeleteResourceList, DeleteDerelictList, DeleteShipList, DeleteMissileList`; `collBlobList`; tactics `RetreatList,
  AttackMemory`; `ResourceVolumeList`; and `GrowSelection HousekeepShipList`.
- Selections are fixed-header arrays: `SelectCommand{numShips, timeLastStatus, ShipPtr[1]}`, the `Max6` and
  `MaxSelection` variants, and `SelectAnyCommand` for targets (`GoK:src/Game/ShipSelect.h:25-66`). `GrowSelection` is
  a growable version.
- Network-stable IDs: `ShipID/ResourceID/DerelictID/MissileID` (uword) plus `IDToPtrTable` lookups
  (`GoK:src/Game/UnivUpdate.c:260-437`). Network commands carry IDs, never pointers (`SetTargetID`,
  `GoK:src/Game/CommandNetwork.c:438`).

### 2.5 Universe and Player (`GoK:src/Game/Universe.h:104-204`)

- `Player`: `PlayerMothership, resourceUnits, race, playerState, aiPlayer, autoLaunch, playerIndex, sensorLevel, bounty,
  totalships, shiptotals[TOTAL_NUM_SHIPS], classtotals[NUM_CLASSES], PlayerResearchInfo researchinfo, Allies
  (bitmask), AllianceProposals…, initialShipCost`.
- `Universe` (single global `universe`): `mainCommandLayer`, `mainCameraCommand`, render lists,
  `phystimeelapsed, totaltimeelapsed`, the lists above, `univUpdateCounter`, id counters, `wintime/quittime`,
  `shipMaxUnits[]` (unit caps), `aiplayerEnemy[]`, `gameStats`, crates, and `players[MAX_MULTIPLAYER_PLAYERS+1]`. The
  extra player is the "enemy of all" slot used for autoguns (`:202`).
- Cataclysm `Player` (`CET:game/Game/universe.h:110-148`) inserts `udword unk; sdword supportUnitsMax;
  sdword supportUnitsUsed` after `resourceUnits` (race at +0x14). CET notes the struct is "significantly larger".
  Cataclysm `Universe` (`:150-257`) adds `unknown1` and `real32 realtimeelapsed` ("New timer keyed off
  QueryPerformanceCounter"), drops one of `RetreatList`/`AttackMemory`, adds a second GrowSelection `unk1` and
  `unk3[0x1F8]` after `gameStats`, and puts `players` at 0x4B00. `universe` is at **0x00B62A00** and
  `univUpdateCounter` at **0x00B62BAC** (+0x1AC) (`CET:Cataclysm.h`).

### 2.6 HW1 → Cataclysm layout deltas (cross-check)

HW1 offsets were computed by compiling `offsetof` probes with `gcc -m32` against `HWSDL:src` headers
(`-D_LINUX_FIX_ME -DHW_GAME_HOMEWORLD -DHW_BUILD_FOR_DISTRIBUTION`; scratch probe in `/tmp/claude-1000/sz/`).
This is i386 SysV packing. MSVC packing is identical for these structs (no doubles or int64), but the original 1999
build may differ in details (UNVERIFIED). The Cataclysm offsets come from the `ASSERT_OFFSET`/`static_assert` lines in
CET headers.

| Field | HW1 (computed) | Cataclysm (CET) | Δ |
|---|---|---|---|
| `sizeof(Ship)` | 0x470 | 0x660 if CET's tail is right: `buildTimer` 0x5B4, `beastSystemType` 0x650, `beastUnk` 0x654 are asserted (`spaceobj.h:1874-1876`); `sizeofShipSpecifics`/`ShipSpecifics[1]` follow at 0x658/0x65C (declared `:1780-1781`, not asserted) | — |
| `Ship.shiplink` | 0x1D0 | 0x23C | +0x6C |
| `Ship.shiptype` | 0x244 | 0x2B0 | +0x6C |
| `Ship.tacticstype` | 0x24C | 0x2B8 | +0x6C |
| `Ship.timeCreated` | 0x274 | 0x2E0 | +0x6C |
| `Ship.madAnimationFlags` | 0x294 | 0x300 | +0x6C |
| `Ship.playerowner` | 0x2C0 | 0x32C | +0x6C |
| `Ship.shipidle` | 0x2E0 | 0x34C | +0x6C |
| `Ship.command` | 0x304 | 0x370 | +0x6C |
| `Ship.attackvars` | 0x308 | 0x374 | +0x6C |
| `Ship.howDidIDie` | 0x354 | 0x39C | +0x48 |
| `Ship.damageModifier` | 0x394 | 0x3DC | +0x48 |
| `Ship.shipDeCloakTime` | 0x3CC | 0x428 | +0x5C |
| `Ship.trail[]` | 0x3D0 | 0x42C | +0x5C |
| `sizeof(ShipStaticInfo)` | 0x370 | > 0x458 (last asserted field `RKOGetsMissiles` at 0x454, `spaceobj.h:916`; the pointer tail declared after `:733` is unplaced) | — |
| `SSI.maxhealth` | 0x98 | 0xA4 | +0x0C |
| `SSI.bulletRange[0]` | 0x158 | 0x1A4 | +0x4C |
| `SSI.passiveRetaliateZone` | 0x178 | 0x1C4 | +0x4C |
| `SSI.shipclass` | 0x19C | 0x1E8 | +0x4C |
| `SSI.canReceiveShips` | 0x1A1 | 0x1FD | +0x5C |
| `SSI.buildCost` | 0x1D8 | 0x238 | +0x60 |
| `SSI.repairTime` | 0x210 | 0x278 | +0x68 |
| `SSI.shipIsCapital` | 0x240 | 0x2A8 | +0x68 |
| `Universe.phystimeelapsed` | 0xB8 | 0xBC | +4 |
| `Universe.effectList` | 0xDC | 0xE4 | +8 |
| `Universe.ShipList` | 0xE8 | 0xF0 | +8 |
| `Universe.HousekeepShipList` | 0x19C | 0x198 | −4 |
| `Universe.univUpdateCounter` | 0x1A8 | 0x1AC | +4 |
| `Universe.wintime` | 0x1B8 | 0x1BC | +4 |
| `Universe.gameStats` | 0x258 | 0x308 | +0xB0 (shipMaxUnits[] 59→145 entries) |
| `Universe.numPlayers` | 0x22C4 | 0x4AF8 | — |
| `Player.race` | 0x08 | 0x14 | +0x0C |
| `Player.shiptotals` | 0x24 | 0x30 | +0x0C |
| `Player.classtotals` | 0x110 | 0x274 | (59 vs 145 ship types) |
| `SinglePlayerGameInfo.hyperspaceState` | 0x10 | 0x10 | 0 |
| `sizeof(CommandToDo)` | 0x100 | 0x330 (`CET:CommandLayer.h:109`; a comment, not an assert) | +0x230 |
| `sizeof(Player)` / `sizeof(Universe)` | 0x188 / 0x30A0 | larger (CET has a commented-out `static_assert(sizeof(Player) == 0x6530u)`, `universe.h:136`; UNVERIFIED) | — |

CET's asserts for `putOnDoor` 0x1C3, `dockingship` 0x1C4 and `specialFlags` 0x1C8 are commented out
(`CET:game/Game/spaceobj.h:1820-1822`), so the `ShipOrDerelict` block between `buildProgress` (0x170) and `shiplink`
(0x23C) is not pinned. Other asserted Cataclysm `Ship` fields not in the table (`spaceobj.h:1810-1876`): `staticinfo` 0x20,
`collMyBlob` 0x54, `enginePosition` 0x288, `shiprace` 0x2B4, `gettingrocked` 0x340, `autostabilizeship` 0x34D,
`soundevent.hyperspaceHandle` 0x45C, `dockvars.dockship` 0x494, `shipsInsideMe` 0x4B4, `pRepairDroids` 0x4C0,
`shipSinglePlayerGameInfo` 0x4C4, `hotKeyGroup` 0x43E.

HW1 `TOTAL_NUM_SHIPS` is 59: the `classtotals − shiptotals` gap is 0xEC = 59×4 (`GoK:src/Ships/ShipDefs.h`). Cataclysm's
is 145 (`CET:game/Game/ShipDefs.h`). Cataclysm's "STD" range is `sRecon..bMissileCorvette`, so its `TOTAL_STD_SHIPS`
is 60 (`CET:game/Game/ShipDefs.h:147-152`). That sizes every `[TOTAL_STD_SHIPS]` array, for example the construction
manager's `progress[]` (§11) and `aiBuildingShip[]` (`CET:Cataclysm.h:249`).

**More HW1 → Cataclysm deltas** (added by the verification pass from an independent re-probe with the same flags; the
machine has no `libc6-dev-i386`, so the probe compiles with `-c -idirafter /usr/include/x86_64-linux-gnu` plus a stub
`gnu/stubs-32.h`, and reads each offset as the size of a `char[offsetof(T,f)+1]` symbol via `nm -S`). Cataclysm values
are CET asserts (`CET:game/Game/spaceobj.h:760-916, 1214-1219, 1808-1876`).

| Field | HW1 | Cataclysm | Δ | Note |
|---|---|---|---|---|
| `Ship.objtype` / `flags` | 0x10 / 0x14 | 0x10 / 0x14 | 0 | |
| `Ship.staticinfo` | 0x18 | 0x20 | +0x08 | CET's `SpaceObj` inserts `ubyte unk1[8]` after `flags` (`CET:spaceobj.h:1192-1213`) |
| `Ship.collMyBlob` | 0x48 | 0x54 | +0x0C | plus `udword unknown2` before `cameraDistanceSquared` (0x3C) |
| `Ship.enginePosition` | 0x21C | 0x288 | +0x6C | start of the +0x6C run |
| `Ship.shiprace` / `isDodging` | 0x248 / 0x250 | 0x2B4 / 0x2BC | +0x6C | |
| `Ship.madGunStatus` | 0x298 | 0x304 | +0x6C | |
| `Ship.gettingrocked` / `autostabilizeship` | 0x2D4 / 0x2E1 | 0x340 / 0x34D | +0x6C | |
| `Ship.dockvars.dockship` | 0x320 | 0x494 | **moved** | HW1 declares `dockvars` (0x24 bytes) right after `attackvars` (`HWSDL:src/Game/SpaceObj.h:1409-1410`). CET declares it after `soundevent`, just before `shipsInsideMe` (`CET:spaceobj.h:1675-1678`). That relocation is why `howDidIDie` falls from the +0x6C run to +0x48. The deltas are not pure insertions |
| `Ship.showingDamage` | 0x364 | 0x3AC | +0x48 | |
| `Ship.tractorPosinfo` | 0x3AC | 0x400 | +0x54 | |
| `Ship.hotKeyGroup` | 0x3E2 | 0x43E | +0x5C | |
| `Ship.soundevent.hyperspaceHandle` | 0x400 | 0x45C | +0x5C | |
| `Ship.shipsInsideMe` | 0x450 | 0x4B4 | +0x64 | HW1 has `DockInfo *dockInfo` (0x44C) just before it; CET's struct has none there |
| `Ship.shipSinglePlayerGameInfo` | 0x45C | 0x4C4 | +0x68 | CET adds `pRepairDroids` 0x4C0 (commented out in HW1, `HWSDL:src/Game/SpaceObj.h:1451`) |
| `SSI.minCollDamage` / `thruststrengthstat[0]` / `turnspeedstat[0]` | 0xA0 / 0xA8 / 0xD8 | 0xAC / 0xB4 / 0xE4 | +0x0C | |
| `SSI.custshipheader` (0x48 bytes: 2 dwords + 16 function pointers) | 0x100 | not asserted | — | 0x10C if the +0x0C run continues (inferred, UNVERIFIED) |
| `SSI.canBuildBigShips` | 0x1A6 | 0x202 | +0x5C | |
| `SSI.buildTime` | 0x1DC | 0x23C | +0x60 | |
| `SSI.harvestRate` | 0x1F4 | 0x25C | +0x68 | CET's naked hook reads it as `[ecx+0x25C]` too (`CET:PatchUniverse.cpp:74`) |
| `SSI.groupSize` | 0x1FC | 0x264 | +0x68 | |
| `SSI.scaleCap` (key `"N-LIPS"`) | 0x28C | `nlips` 0x30C | +0x80 | |
| `SSI.madGunOpenDamagedHealthThreshold` | 0x298 | 0x318 | +0x80 | |
| `SSI.trailStyle[0]` | 0x2A0 | 0x324 | +0x84 | `nRepairDroids` 0x320 is inserted in between |
| `SSI.minimumZoomDistance` / `hyperspaceColor` | 0x330 / 0x348 | 0x3B4 / 0x3CC | +0x84 | |
| `SSI.tacticalSelectionScale` (key `"Overlay_Selection_Scalar"`) | 0x34C | 0x3E8 | +0x9C | CET names the field after the key; inferred to be the same field |
| `Universe.phystimeelapsed` (via code) | 0xB8 | 0xBC | +4 | CET's `univUpdateReset` patch writes `mov [0x00B62ABC], period`, and 0x00B62ABC = `universe` + 0xBC (`CET:PatchUniverse.cpp:413-417`) |

---

## 3. Time and the update pipeline

### 3.1 Tasks and the fixed timestep

- `taskExecuteAllPending(ticks)` (`GoK:src/Game/Task.c:280`) is called once per rendered frame
  (`GoK:src/SDL/utility.c:4680`). For each real-time task it carries leftover ticks and calls the task
  `(ticks+leftover)/ticksPerCall` times, capped at `TSK_MaxTicks = 8` (`Task.c:313-351`, `Task.h:50`). This is a classic
  accumulator, but calls beyond the cap are dropped, not deferred: only `(ticks+leftover) % ticksPerCall` carries over
  (`Task.c:344-351`), so an overloaded frame slows game time instead of catching up.
- `taskStart(universeUpdateTask, UNIVERSE_UPDATE_PERIOD, 0)` is at `GoK:src/SDL/utility.c:4040`. CET patches the
  pushed period at `0x005AC174` (`CET:Cataclysm.h`, `utyGameSystemsInit_UNIVERSE_UPDATE_PERIOD`). Other tasks pinned
  to the same period are NIS (`GoK:src/Game/NIS.c:580`; CET `0x004CC8F2`) and the "baby" callback task
  (CET `0x0052AF12`). In HW1 the baby task is `taskCallBackInit` → `taskStart(taskCallBackProcess, BABY_CallBackPeriod, 0)`
  (`GoK:src/Game/Task.c:539`), with `BABY_CallBackPeriod` hard-coded as `1.0f / 16.0f` (`GoK:src/Game/Task.h:90`), not
  derived from `UNIVERSE_UPDATE_PERIOD`.
- Tasks are stackless coroutines (`taskBegin/taskYield/taskEnd`, all statics) (`GoK:src/Game/Task.h`).

### 3.2 `universeUpdateTask` (`GoK:src/Game/Universe.c:3786-4060`)

- **Single player** (`:4000-4045`): when `universeTurbo` is set it first runs `turboTimeCompressionFactor-1` extra
  `univUpdate`s (`:4026-4034`), then one more (`:4037`). `turboTimeCompressionFactor = 8` is commented "same as Homeworld
  Cataclysm" (`GoK:src/Game/Universe.c:102`). In the tutorial when the game is not running, KAS is executed every
  other frame (`:4015-4022`).
- **Multiplayer and packet playback** (`:3902-3993`): `clWaitSyncPacket` (`GoK:src/Game/CommandNetwork.c:1873`)
  dequeues the next sync packet and executes its commands through `clProcessSyncPacket` (`:1494`). Then come
  `univUpdate`, a `taskYield`, and a second `univUpdate` (`Universe.c:3973-3990`): **one sync packet = 2 ticks**. The
  captain emits a sync packet every `CAPTAINSERVER_PERIOD = 1/8 s` (`CommandNetwork.h:126`). On
  `TOO_MANY_PACKETS` the yield is skipped, which doubles throughput to catch up. With `universeTurbo` during packet
  playback the loop runs `repeattimes = (turboTimeCompressionFactor-1)>>1` = 3 times (`Universe.c:3949-3956`), i.e.
  6 univUpdates per task call, not the 8 of the SP path.
- The render lists are refreshed on `universeUpdateCounter & REFRESH_RENDERLIST_RATE` (`:3906-3917`).

### 3.3 `univUpdate(phystimeelapsed)` order of operations (`GoK:src/Game/UnivUpdate.c:7271-7590`)

| # | Line | Step |
|---|---|---|
| 1 | 7279-7307 | debug autosave (`autoSaveDebug`) |
| 2 | 7323-7327 | `univUpdateCounter++`; `phystimeelapsed = dt`; `totaltimeelapsed += dt` |
| 3 | 7329-7376 | win/quit timers → `DeclarePlayerWin`, `spMainScreen`, `"SP_Game_Over"` screen (7361); MP sync-queue overrun → quit |
| 4 | 7384-7402 | MP RU regeneration: every `regenerateRUrate` ticks, `+regenerateRUs` up to `regenerateRUsIfUnder` (`GoK:src/Game/ResCollect.c:64-67`: rate 255, ifUnder 400, RUs 25). Rate 255 = every 256 ticks = 16 s at 16 Hz; CET computes it as "once every 16 s" (`CET:PatchUniverse.cpp:287`). |
| 5 | 7417-7428 | collision blobs: full rebuild `collUpdateCollBlobs` on `REFRESH_COLLBLOB_*`; otherwise `collUpdateObjsInCollBlobs`; battle pings |
| 6 | 7432-7433 | `cloudSetFog`, `nebSetFog` |
| 7 | 7436 | `cmDeterministicBuildProcess()`: MP deterministic construction (§11) |
| 8 | 7439 | `cmBuildTaskFunction()`: local build manager (SP RU deduction) |
| 9 | 7441-7444 | `rmUpdateResearch()` on `REFRESH_RESEARCH_RATE 15 / FRAME 13`. CET hooks it at `0x0054C83D` (inside univUpdate at `0x0054C3F0`). |
| 10 | 7446-7470 | MP resource injections, lump sum, alliance proposal timeout |
| 11 | 7480-7485 | first tick only: `univSetupShipsForControl`, `gameStatsCalcShipCostTotals` |
| 12 | 7487-7494 | `wkTradeUpdate()` (trader GUI ships) |
| 13 | 7496-7502 | SP: `singlePlayerGameUpdate()` (CET `0x005197D0`). MP with hyperspace: `UpdateMidLevelHyperspacingShips()` (CET `0x00519700`). |
| 14 | 7505 | `collCheckAllBulletMissileCollisions()`, using last tick's positions |
| 15 | 7510 | `cratesUpdate()` |
| 16 | 7512 | `aiplayerUpdateAll()`: computer players, and **KAS in SP** (§13, §14.4) |
| 17 | 7519-7524 | `clProcess`, then `clPostProcess` (skipped while `nisUniversePause`) |
| 18 | 7529 | `tacticsGlobalUpdate()` (retreat and attack memory) |
| 19 | 7531 | `univUpdateMineWallFormations()` |
| 20 | 7536 | `collCheckAllBumpCollisions()` (ship–ship, ship–resource, ship–derelict) |
| 21 | 7543-7558 | integrate positions: minor objs → bullets → missiles → derelicts → resources → **ships** (`univUpdateAllPosVelShips`, which also runs per-ship state and housekeeping) → effects. A paused-NIS variant is at 7556. |
| 22 | 7562-7565 | `univDeleteDeadShips/Resources/Derelicts/Missiles` (deferred deletion) |
| 23 | 7567-7570 | `univCheckRegrowResources()` on `REGROW_RESOURCES_CHECK_RATE 255` (`ResCollect.c:60`). CET hooks it at `0x0054CD73`. |
| 24 | 7574-7582 | `singlePlayerLoadNewLevel()` if flagged (between-mission hyperspace) |
| 25 | 7586-7590 | `calculatePlayerBounties()` every `CHECK_BOUNTIES_RATE` |

Sensing and commanding happen **before** integration. Integration consumes the forces and torques written by the
command layer and the AIShip/AITrack steering. Bullet collision runs against the previous tick's positions.

### 3.4 Per-ship work inside `univUpdateAllPosVelShips` (`GoK:src/Game/UnivUpdate.c:1920-2041`)

For each ship in `ShipList` order, unless `SOF_DontApplyPhysics`/`SOF_NISShip`:

1. `univCheckShipState(ship)` (`:5846`). It kills NaN ships, handles the `deathtime` self-destruct, runs
   autostabilize (`aitrackSteadyShip`), calls `tacticsUpdate(ship)`, and checks out-of-world every
   `CHECK_SHIP_OUTOFWORLD_RATE`. For idle ships it starts **passive retaliation**: if `gettingrocked` is set it calls
   `clPassiveAttack`; otherwise every `CHECK_PASSIVE_ATTACK_RATE` it scans enemies within `passiveRetaliateZone`.
   ResourceCollectors flee instead.
2. Physics. Slave masters integrate and then `dockUpdateSlaves`. Clamped ships are queued to
   `ClampedShipList` → `updateClampedObject` after the loop. Everything else gets `physUpdateObjPosVelShip`.
3. `univSetupShipForControl(ship)` (`:5565-5711`). It resets `gettingrocked`, `shipidle = TRUE`, `autostabilizeship = TRUE`;
   handles cloak visibility bookkeeping every `TW_ProximitySensorSearchRate`; decays attack memory; calls `dmgShipThink`
   (damage effects); updates trails; applies the **self-repair** rule (`repairTime/repairDamage`, combat variants); and
   queues `CustShipHousekeep` into `HousekeepShipList`.
4. MAD mesh animation update.
5. After the loop: clamped ships, then **all `CustShipHousekeep` calls**. They are deferred so that every ship has been
   set up before any housekeep runs (`:2015-2037`).

Note the ordering trap: `shipidle` is set TRUE at step 3 for the *next* tick. The command layer clears it again during
its own processing.

### 3.5 Periodic ("woodpecker") schedules: Cataclysm values

The values are from `CET:assets/tweak.script` (defaults in `GoK:src/Game/Tweak.c:399-404`). `rate` is a 2^n−1 mask.

| Tweak | rate/frame | Consumer |
|---|---|---|
| REFRESH_COLLBLOB_RATE/FRAME/BATTLEPING_FRAME | 7 / 6 / 4 | univUpdate step 5 |
| REFRESH_RESEARCH_RATE/FRAME | 15 / 13 | rmUpdateResearch |
| REFRESH_RENDERLIST / MINORRENDERLIST | 7/1, 7/3 | universeUpdateTask |
| CHECK_BOUNTIES_RATE/FRAME | 127 / 101 | bounties |
| CHECK_PASSIVE_ATTACK_RATE, …WHILEMOVING_RATE | 31, 31 | univCheckShipState (phase = shipNumber & rate) |
| TW_ProximitySensorSearchRate/Frame | 31 / 27 | cloak visibility reset |
| CHECK_ATTACKMOVE_TO_MOVE_RATE/FRAME | 31 / 22 | attack-move to move conversion |
| EnemyNearByCheckRate/Frame, RetreatCheckRate/Frame | 15/14, 31/26 | tactics |
| ATTACKMEMORY_CLEANUP_RATE/FRAME | 255 / 254 | tactics |
| FORMATION_ERROR_CALCULATE_RATE/FRAME | 7 / 2 | formations |
| SPHERE_CHECK_TARGET_MASK/FRAME, MINE_DO_SEARCH_MASK | 31/21, 15 | sphere formation, mines |
| SP_KAS_UPDATE_FRAME, TUTORIAL_KAS_UPDATE_MASK/FRAME | 0, 1/0 | KAS cadence (§14.4) |
| CLOAKGENERATOR_CLOAKCHECK_RATE | 15 | cloak generator |

---

## 4. Physics, collision, damage

### 4.1 Integrator (`GoK:src/Game/Physics.c`)

- Forces and torques are accumulated in world space by `physApplyForceToObj(obj, mag, TRANS_dir)` (`:42`), which is a
  no-op for `dontapplyforceever` or SOF_Crazy, and by `physApplyRotToObj`. The thrust strengths come from
  `nonstatvars` (so they can be modified per ship, e.g. by upgrades or tactics).
- `physUpdateObjPosVelShip` (`:234-480`) is **semi-implicit Euler**:
  1. `v += (F/m)·dt`, then clamp v **per component** to ±`tacticsGetShipsMaxVelocity(ship)` with `vecCapVectorSloppy`
     (tactics, speed-burst, SP limiter; `GoK:src/Game/Tactics.c:2169`), then `x += v·dt`. `vecCapVectorSloppy`
     (`GoK:src/Game/Vector.c:111`) clips x, y and z independently, so diagonal speed can reach √3× the nominal maximum.
     A 1:1 port must not replace it with a magnitude cap. The cap is skipped when v is exactly zero (`Physics.c:264`).
     That test ORs the raw bit patterns (`Real32ToUdword(v.x) | …`), so a `-0.0f` component counts as moving; a port
     must keep the bit test, not `v != 0.0f`.
  2. Fuel burn uses `getVectDistSloppy(d)`, the L∞ norm (`GoK:src/Game/Vector.c:244`), and applies only in SP or with
     `MG_FuelBurnEnabled`.
  3. `ω += (τ/I_axis)·dt`, then clamp ω per component to ±`maxrot` (same `vecCapVectorSloppy`, `Physics.c:401`). The coordsys is post-multiplied by rotX, rotY, rotZ built from
     `cos/sin(ω·dt)`, **sequentially per axis**.
  4. Recompute `collPosition` (and the collision box every 4th tick, `UPDATE_COLLRECT_RATE 3`) and the engine position;
     zero F and τ.
  5. Crazy and disabled ships get `SHIP_CRAZY_FRICTION`.

  Separate integrators exist for derelicts (`:482`), missiles (`:641`), bullets (`:141`) and basic objects (`:796`).
- Steering lives above physics: `AIShip.c` (`aishipFlyToPointAvoidingObjsFunc`, `GoK:src/Game/AIShip.c:866`) and
  `AITrack.c` (`aitrackHeadingAndUp :306`, `aitrackSteadyShip :1225`, `aitrackVelocityVectorGuidance :939`). Strike-craft
  manoeuvres are in `FlightMan.c` (flip turn, Immelmann, barrel roll and so on, weighted by `flightman*` probabilities
  in each `.shp`).

### 4.2 Collision (`GoK:src/Game/Collision.c`, `Blobs.c`)

- Spatial partitioning uses **blobs**, clusters of nearby objects. They are rebuilt every 8 ticks
  (`collUpdateCollBlobs :554` → `bobListCreate`) and their members refreshed in between (`:572`). All pairwise tests are
  per blob.
- `collCheckAllBulletMissileCollisions` (`:2830`): bullets vs targets (`collCheckBulletTargetColl :1866`; beams use
  `collCheckBeamColl :1748`), then missiles vs ships/resources/derelicts, then mines.
- `collCheckAllBumpCollisions` (`:2779`): small–small, big–big, big–small, ship–resource, ship–derelict. It is skipped
  during NIS, with a special case for Bentusi in Mission 4.
- Narrow phase: sphere tests, then the oriented box `collCheckRectLine` (`:152`) returns the hit **side**
  (`TRANS_*`), which feeds `collSideModifiers`.
- Contact response: `ObjectsCollided` (`GoK:src/Game/UnivUpdate.c:2470`), `ApplyDamageToCollidingObjects` (`:2314`) using
  `minCollDamage/maxCollDamage` and `COLLDAMAGE_*`, and `kamikazeCheck` (`Collision.c:587`).

### 4.3 Damage pipeline

1. Gun fire. `gunShoot` (`GoK:src/Game/Gun.c:831`) rolls `bullet->damage = frandombetween(gunDamageLo[tactics],
   gunDamageHi[tactics]) * damagemult` (`:962`). That is a `RANDOM_GAME` draw, so it is deterministic. Missiles use
   the same roll (`:611`).
2. `univBulletCollidedWithTarget` (`GoK:src/Game/UnivUpdate.c:3111`):
   - momentum transfer `bulletmass/dt` (skipped inside a gravwell);
   - `damage *= collSideModifiers[side]`; Heavy Defender vs fighter `*= armourPiercingModifier` (2.0 in Cataclysm
     tweak); friendly fire `*= friendlyFireModifier` (0.5);
   - sets `gettingrocked/recentAttacker`, then `ApplyDamageToTarget`.

   **Cataclysm's version takes an extra leading `int` parameter** (`CET:Cataclysm.h`, `univBulletCollidedWithTarget`
   at `0x00546A20`).
3. `ApplyDamageToTarget` (`:2914`) does nothing during NIS. **Damage to a slave is redirected to its master** (`:2925-2932`).
   Asteroids break or shrink, dust clouds are charged by ion cannons, and hyperspace gates are notified. When a ship
   drops to health ≤ 0 it records `howDidIDie/whoKilledMe`, calls `bountyShipWasKilled`, and goes to
   `AddTargetToDeleteList`.
4. Death is two-phase. `AddShipToDeleteShipList` (`:2640`) leads to `univDeleteDeadShips` (`:4394`) at step 22, which
   calls `univDeleteDeadShip` (`:3909`: explosion, blast `pointExplosionInSpace :3030` / `MakeExplosionRockBumpables
   :2809`, `CustShipDied`, command-layer cleanup) and then frees the ship. Player defeat is decided by
   `univKillOtherPlayersIfDead` (`:4334`, §15).

---

## 5. Determinism and multiplayer lockstep

- **Topology**: a captain (one of the peers; `Captaincy.c` handles migration) collects `PACKETTYPE_COMMAND` (0xcccc)
  from all peers (`packetSendToCaptain`, `GoK:src/Game/CommandNetwork.c:606`). Every 1/8 s `captainServerTask` (`:2024`)
  concatenates them into one `PACKETTYPE_SYNC` (0x5555) with a frame number and broadcasts it (`packetBroadcastSync :645`),
  including to itself. Command types are `COMMANDTYPE_MOVE…RESEARCHINFO 1-19` (`CommandNetwork.h:129-157`); misc
  sub-commands are HALT/SCUTTLE/TACTICS/KAMIKAZE/PARADE (`:161-165`).
- Every peer, including the captain, executes commands **only from sync packets**, in frame order (`clWaitSyncPacket`).
  Packets can be re-requested after a loss (`PACKETTYPE_REQUESTSYNCPKTS 0xe155`).
- **Computer players in MP** are distributed round-robin, each to exactly one human machine
  (`GoK:src/SDL/utility.c:2928-2945`, `ComputerPlayerEnabled[i]` only where `j == sigsPlayerIndex`). The AI runs inside
  that peer's univUpdate and issues orders through `clWrap* → clSend*`, the same as a human
  (`GoK:src/Game/CommandWrap.c:112-123`). The AI is therefore an *input source*, not lockstep-simulated state.
- **RNG**: `ranRandomFn(stream)` (`GoK:src/Game/Randy.c:57-107`) is a subtract-with-borrow generator (Marsaglia & Zaman,
  "A New Class of Random Number Generators", *Ann. Appl. Probab.* 1(3), 1991) summed with a 69069 LCG
  (`n = 69069*n + 1013904243`). There are 25 independent streams (`GoK:src/Game/Randy.h:27-52`). **Only `RANDOM_GAME` (10)
  may influence simulation** (`gamerand()`, `randomG`, `frandom`). Particles, sound, trails, clouds and the like have
  their own streams so that rendering cannot desync the game. Cataclysm's `ranRandomFn` is at `0x004EF860`
  (`CET:Cataclysm.h`). The algorithm attribution is this document's; `Randy.c` itself names no source. AI decisions
  draw from `RANDOM_AI_PLAYER` (6) (`GoK:src/Game/AIUtilities.h:56`, `AIMoves.c:2346` and others), which is safe
  because each AI runs on exactly one peer.
- **Checksums**: each sync packet carries `NetSyncChecksums{randcheck = gamerand(), univcheck = univGetChecksum()
  (sum of live ship positions, GoK:src/Game/UnivUpdate.c:5004), blobcheck = statsGetStatChecksum()}`
  (`GoK:src/Game/NetCheck.c:918-933`). The stat checksum covers ship stats, so a mismatch triggers `SendCheatDetect`
  (`:893-900`). Deep debugging uses `GOD_LIKE_SYNC_CHECKING` universe snapshots (`GoK:src/Game/SpaceObj.h`, end).
  **RNG side effect:** `netcheckShow` is called for every sync packet from `clProcessSyncPacket`
  (`GoK:src/Game/CommandNetwork.c:1520`), and its `randcheck = gamerand()` (`NetCheck.c:923`) sits outside
  `#if SYNC_CHECK`, so it consumes one `RANDOM_GAME` value per packet. A port that drops the checksum call shifts the game
  RNG stream relative to the original in MP and in packet playback.
- **Float determinism**: there is no FP control-word management in the C sources (grep finds no
  `_controlfp`/`fldcw`). Determinism relied on every peer running the same x87 binary. The SDL port's
  `fsqrt = sqrtf` (`HWSDL:src/Game/FastMath.h`) may differ from the 1999 code (UNVERIFIED). **Port implication:**
  network compatibility with original `cataclysm.exe` clients is not achievable in practice. Port-to-port MP needs
  identical math on both OSes (SSE2, `-ffp-contract=off`, no `-ffast-math`, the same libm or own sin/cos/sqrt), plus
  bit-identical struct iteration orders.
- Deterministic construction in MP uses `cmDeterministicBuildProcess` (`GoK:src/Game/ConsMgr.c:4252`) and
  `COMMANDTYPE_DETERMINISTICBUILD`. Costs are charged per tick as a fixed fraction, and ships are created via
  `clCreateShip` when `ticksLeft` reaches 0.

---

## 6. Ship behaviour modules (`GoK:src/Ships/`)

### 6.1 `CustShipHeader`: the per-type vtable (`GoK:src/Game/SpaceObj.h:523-543`)

| Slot | Signature | Called from |
|---|---|---|
| shiptype | `ShipType` (−1 = shared header) | assert in `InitStatShipInfo` |
| sizeofShipSpecifics | bytes of `ShipSpecifics` tail | `univCreateShip` |
| CustShipStaticInit | `(dir, file, ShipStaticInfo*)` | `Universe.c:2313-2315` |
| CustShipStaticInitPost | `(ShipStaticInfo*)` | `Universe.c:2339` |
| CustShipInit / Close | `(Ship*)` | `UnivUpdate.c:1559` / `:6633` |
| CustShipAttack | `(Ship*, target, maxdist)` | `CommandLayer.c:1399-1401, 1585, 1822, 1894` |
| CustShipFire | `(Ship*, target)` | ship modules, `NIS.c:2337`, `TradeMgr.c:1594` |
| CustShipAttackPassive | `(Ship*, Ship *target, rotate)` | `CommandLayer.c:975, 1171, 1388…` |
| CustShipSpecialActivate | `bool32 (Ship*)` (no target) | `processSpecialToDo`, `CommandLayer.c:800` |
| CustShipSpecialTarget | `bool32 (Ship*, void *custom)` (receives a `SelectAnyCommand*`) | `CommandLayer.c:779` |
| CustShipHousekeep | `(Ship*)` per tick | deferred, `UnivUpdate.c:2029` |
| CustShipRemoveShipReferences | `(Ship*, Ship *dying)` | `UnivUpdate.c:3569`, `CommandLayer.c:3891` |
| CustShipDied | `(Ship*)` | `UnivUpdate.c:3945, 6476` |
| CustShip_PreFix / _Save / _Load / _Fix | savegame pointer fix-up | `SaveGame.c:1814-2114` |

`HeaderTable[type]` is `shipHeaders[TOTAL_NUM_SHIPS]` (`GoK:src/Game/Universe.c:218-279`). Docking behaviour is a
second, per-receiver table that `dockInitializeCustomFunctions` writes into
`statinfo->ShipXDocksAtShipY/LaunchShipXFromShipY` (`GoK:src/Game/Dock.c:6343`).

### 6.2 Every HW1 ship type → header → special behaviour

The races come from `RacesAllowedForGivenShip` (`GoK:src/Game/Universe.c:111-167`). The non-NULL slots are extracted
from each header initializer.

| ShipType (enum order, `GoK:src/Ships/ShipDefs.h:10-77`) | Races | Header (file:line) | Behaviour / special ability |
|---|---|---|---|
| AdvanceSupportFrigate | R1 R2 | AdvanceSupportFrigate.c:84 | refuel/repair targets (`SpecialTarget → refuelRepairShips`, :76); strike-craft dock pad (`Dock.c:3344-3384`) |
| AttackBomber | R1 R2 | GenericInterceptor.c:1328 | strike-craft flight AI, plasma bombs |
| Carrier | R1 R2 | Carrier.c:202 | builds ships; dock/launch; captured-ship retrofit queue `CAP*` (`Carrier.h:14-26`); repair special; Init/HouseKeep/Died/RemoveRefs/Fix |
| CloakedFighter | R1 | GenericInterceptor.c:1306 | self-cloak (`CloakedFighterSpecialActivate :1066`) |
| CloakGenerator | R1 R2 | CloakGenerator.c:585 | area cloak with charge/recharge (`CloakGeneratorStatics`, `CloakGenerator.h:23-31`) |
| DDDFrigate | R1 | DDDFrigate.c:610 | launch/retract Drones (`SpecialActivate :177`, DDDstate) |
| DefenseFighter | R2 | DefenseFighter.c:1065 | lasers shoot down bullets/missiles (`DefenseFighterStatics`, `DefenseFighter.h:14-28`) |
| DFGFrigate | R2 | DFGFrigate.c:169 | defence field redirects bullets (`fieldRadius`, `percentageThatGetThrough`, `DFGFrigate.c:32-35`) |
| GravWellGenerator | R1 R2 | GravWellGenerator.c:725 | immobilises strike craft in a radius (`GravWellGeneratorStatics`) |
| HeavyCorvette | R1 R2 | HeavyCorvette.c:299 | charged burst attack (`SpecialTarget :81`; `burst*` tweaks, `Tactics.h`) |
| HeavyCruiser | R1 R2 | HeavyCruiser.c:61 | none (slaved ion guns via GUN `SlaveDriver`) |
| HeavyDefender, LightDefender | R1 R2 | GenericDefender.c:351 | defender AI (armour piercing vs fighters, §4.3) |
| HeavyInterceptor, LightInterceptor | R1 R2 | GenericInterceptor.c:1284 | flight-manoeuvre AI; LightInterceptor **speed burst** (:1270) |
| IonCannonFrigate | R1 R2 | IonCannonFrigate.c:61 | — |
| LightCorvette | R1 R2 | LightCorvette.c:66 | — |
| MinelayerCorvette | R1 R2 | MinelayerCorvette.c:793 | lays mines and mine walls (`SpecialActivate :630`, `MineFormationInfo`) |
| MissileDestroyer | R1 R2 | MissileDestroyer.c:264 | missile volley (`SpecialTarget :146`; `MissileRegenerateTime/VolleyTime/LagVolleyTime` :47-49) |
| Mothership | R1 R2 | Mothership.c:525 | builds; hangar doors; capture queue |
| MultiGunCorvette | R1 R2 | MultiGunCorvette.c:63 | multi-target guns |
| Probe | R1 R2 | Probe.c:135 | scout; HouseKeep orientation logic |
| ProximitySensor | R1 R2 | ProximitySensor.c:396 | reveals cloaked ships in `SearchRadius` (HouseKeep) |
| RepairCorvette | R1 R2 | RepairCorvette.c:524 | repair (`SpecialTarget`) |
| ResearchShip | R1 R2 | ResearchShip.c:853 | provides research labs; stacks with other research ships as master/slave (§10) |
| ResourceCollector | R1 R2 | ResourceCollector.c:69 | harvest (§12); refuel/repair special (:55) |
| ResourceController | R1 R2 | ResourceController.c:61 | RU drop-off; refuel/repair special |
| SalCapCorvette | R1 R2 | SalCapCorvette.c:3096 | salvage/capture state machine (§12) |
| SensorArray | R1 R2 | SensorArray.c:69 | `Init` sets `playerowner->sensorLevel = 2`; `Died` reverts it if no array remains |
| StandardDestroyer, StandardFrigate | R1 R2 | StandardDestroyer.c:61, StandardFrigate.c:61 | — |
| Drone | R1 | Drone.c:48 | DDDF drone (`SOF_NotBumpable`) |
| TargetDrone | R1 R2 | GenericInterceptor.c:1350 | — |
| HeadShotAsteroid, CryoTray | R1 R2 | DoNothingShipHeader (DefaultShip.c:190) | KAS props as ships |
| P1Fighter | P1 | GenericInterceptor | — |
| P1IonArrayFrigate, P1Mothership, P1StandardCorvette | P1 | P1*.c:60/60/62 | — |
| P1MissileCorvette | P1 | P1MissileCorvette.c:222 | missile volley (SpecialTarget, HouseKeep) |
| P2AdvanceSwarmer, P2Swarmer | P2 | P2*.c:66 | swarmer AI |
| P2FuelPod | P2 | P2FuelPod.c:60 | swarmer refuel dock (`Dock.c:6193-6205`) |
| P2Mothership | P2 | P2Mothership.c:70 | — |
| P2MultiBeamFrigate | P2 | P2MultiBeamFrigate.c:210 | rotating multi-beam (`MultiBeamRange`, `BeamFireTime` :50-53) |
| P3Destroyer/Frigate/Megaship | P3 | P3StandardShip.c:81 | — |
| FloatingCity | Traders | FloatingCity.c:61 | Bentusi |
| CargoBarge, ResearchStation | Traders | DefaultShipHeader (DefaultShip.c:168) | default attack/fire |
| MiningBase | Traders | MiningBaseHeader (DefaultShip.c:212) | rotating base |
| JunkYardDawg | Traders | SalCapCorvette.c:3118 | salvager (`SPECIAL_IsASalvager`) |
| JunkYardHQ, Ghostship, ResearchStationBridge/Tower | Traders | DoNothingShipHeader | — |
| Junk_LGun, Junk_SGun | Traders | StationaryGunHeader (DefaultShip.c:234) | stationary turret |

`RacesAllowedForGivenShip` has initializers only up to JunkYardHQ, so Ghostship, Junk_LGun, Junk_SGun,
ResearchStationBridge and ResearchStationTower have no `*_VALID` bit there. They are Traders only through the
`TRADERS_FIRST_SHIP..TRADERS_LAST_SHIP` range (`GoK:src/Ships/ShipDefs.h:105-106`). The STD (R1/R2) range is
AdvanceSupportFrigate..CryoTray (`STD_LAST_SHIP`, `:94`).

### 6.3 Per-type statics in the same `.shp` file

`CustShipStaticInit` runs a second `scriptSetStruct` over the same `.shp` with a per-module table. Examples:
`DFGFrigate.c:32-35` (`fieldRadius`, `BulletDamageMultiplier`, `BulletLifeExtension`, `percentageThatGetThrough`),
`MissileDestroyer.c:47-49`, `P1MissileCorvette.c:38`, `Attack.c:31-33` (`repositionTime`, `circleRange`,
`fullMovementFreedom`: the side-step corvette attack), `RepairCorvette.c:51-64`, `P2MultiBeamFrigate.c:50-53`.
GenericInterceptor-style flight tables (`flyPastDist`, `breakRange`, `flightman*`, indexed by tactics × class, with a
`Default` row) are in `GenericInterceptor.h:32-43`.

**These exact keys reappear in the Cataclysm `.shp` files.** `bDFGFrigate.shp:60-63` uses `fieldRadius` and
`percentageThatGetThrough`. `sDestroyer.shp:61-63`, `bMissileCorvette.shp:60-62` and `sDreadnought.shp` use
`MissileRegenerateTime` and the volley keys. `sMCV.shp` uses `repositionTime/circleRange/fullMovementFreedom`.
`sMimic.shp` uses the full `flightman*` tables. The HW1 behaviour modules therefore survive in Cataclysm, re-bound to new
type IDs (inferred).

---

## 7. Command layer (`GoK:src/Game/CommandLayer.c`, `.h`)

### 7.1 Data

- `CommandLayer{LinkedList todolist}` (`CommandLayer.h:103-106`) holds one global list,
  `universe.mainCommandLayer`.
- `CommandToDo` (`CommandLayer.h:77-101`) is one **group** of ships with one order. It contains `selection`;
  `ordertype{uword order; uword attributes}`; and the payloads `move{heading,destination}`, `attack*`,
  `FormationCommand formation` (`Formation.h:53-74`), `DockCommand dock` (`Dock.h:29-37`), `launchship`, `collect`,
  `protect*`/`protectFlags`, `specialtargets*`, `militaryParade*` and `buildingship`. It also holds holding-pattern and
  hyperspace timers and ping bookkeeping. A ship is in at most one CommandToDo (`ship->command`).
- Orders (`GoK:src/Game/CommandDefs.h:12-22`): `NULL 0, MOVE 1, ATTACK 2, DOCK 3, LAUNCH_SHIP 4, COLLECT_RESOURCES 5,
  BUILDING_SHIP 6, SPECIAL 7, HALT 8, MILITARY_PARADE 9, MP_HYPERSPACING 10`.
  Attribute masks (`:24-28`): `FORMATION 0x1, PROTECTING 0x2, PASSIVE_ATTACKING 0x4, HOLDING_PATTERN 0x8,
  ATTACKING_AND_MOVING 0x10`. Cataclysm's copy has the same values (`CET:game/Game/CommandDefs.h:12-28`) under older spellings
  (`COMMAND_LAUNCHSHIP`, `COMMAND_IS_FORMATION`, …); any waypoint order would
  be new (§16.10).

### 7.2 Entry points

`clWrap*` (`GoK:src/Game/CommandWrap.c`; 21 wrappers including `clWrapMove :89` and `clWrapAttack :168`) filter the
selection (`makeShipsControllable`), add UI feedback, and route to `cl*` (SP) or `clSend*` (MP). Cataclysm addresses:
`clWrapCreateShip 0x00452F30`, `clWrapBuildShip 0x00452FA0`, `clWrapScuttle 0x00453390`, `clWrapSetKamikaze 0x00453820`
(`CET:Cataclysm.h`).

The `cl*` constructors include `clMove :4712`, `clAttack :6042`, `clPassiveAttack :6247`, `clSpecial :6417`,
`clHalt :6829`, `clScuttle :6925`, `clProtect :7669`, `clBuildShip :7481`, `clCreateShip :7290`,
`clSetMilitaryParade :7850`, `clMpHyperspace :4959`, `clDock` (`Dock.c:885`), `clFormation` (`Formation.c:2539`) and
`clCollectResource` (`ResCollect.c:276`). They usually remove the ships from their old commands
(`RemoveShipsFromDoingStuff :3210`) or mutate the existing group (`ChangeOrderTo*`), then `PrepareShipsForCommand`.

### 7.3 `clProcess` dispatch (`CommandLayer.c:8224-9008`)

For each CommandToDo: skip it if any ship is in hyperspace or crazy (SP) or crazy (MP), and clear
`autostabilizeship`. Then:

- **`attributes & FORMATION`** (8277-8632). `ATTACK` either uses the "olden" per-ship attack (`processAttackToDo`) or,
  for neutral/aggressive tactics, `processAttackToDoInFormation` (`tacticsInfo.holdFormationDuringBattle`). Until in
  range, the leader flies with `aishipFlyToShipAvoidingObjs` and the others hold formation (`processFormationToDo`).
  Attack-move: when an attack ends, or the closest target is beyond `movingAttackTurnsIntoMoveCommandDistanceSqr`, the
  group reverts to `MOVE` (`SPECIAL_ATTACKMOVECANCEL`). `DOCK` brings the group to the dock ship first. `HALT` and
  `NULL` run passive attack, protect (sphere formation surrounds the protectee) and holding pattern. `MOVE` uses
  `processMoveLeaderToDo`. `SPECIAL` and `MP_HYPERSPACING` are also handled here.
- **No formation** (8633-9006): `ATTACK→processAttackToDo :992`, `MOVE→processMoveToDo :2711`,
  `DOCK→processDockToDo` (`Dock.c:6489`), `SPECIAL→processSpecialToDo :757` (calls `CustShipSpecialTarget` or
  `CustShipSpecialActivate`), `COLLECT_RESOURCES→processCollectResource` (`ResCollect.c:1014`),
  `LAUNCH_SHIP→processLaunchShipToDo :579`, `HALT→processHaltToDo :818`,
  `MILITARY_PARADE→processMilitaryParadeToDo` (`Formation.c:2771`), `BUILDING_SHIP→processBuildingShipToDo :7402`,
  `MP_HYPERSPACING→processMpHyperspaceingToDo :2452`, `NULL→`passive attack, protect or holding pattern.
- Convention: `process*ToDo` returns **TRUE when the order is complete**. The caller then `FreeLastOrder`s and sets
  `order = COMMAND_NULL` (keeping formation/protect attributes), or deletes the node when the selection is empty.

`clPostProcess` (`:9010`) handles guard retaliation: if a protected ship's `gettingrocked` is an enemy, the guards that
can attack are split off into a new protect + attack command.

### 7.4 State machines worth knowing

- **Attack** (`processAttackToDo :992-1418`). It picks targets (`pickMultipleAttackTargets :5429` for
  `canTargetMultipleTargets`), forms leader/wingman pairs for strike craft (`BreakupIntoLeaderWingman :5575`,
  `attackvars.myLeaderIs/myWingmanIs`), and per ship calls `CustShipAttack(ship, target, bulletRange[tactics])`, or
  `CustShipAttackPassive` for ships that cannot attack. It completes when all targets are dead, hidden or out of reach.
  `FindNewThreateningTarget :677` and `AttackClosestOpponent :5071` handle retargeting. Guns go through
  `gunShootGunsAtTarget` (`Gun.c:1234`).
- **Passive attack** (`processPassiveAttackToDo :898`) shoots while doing something else; it is set by retaliation in
  `univCheckShipState`. `passiveAttackPenaltyExempt` applies the tactics fire-time modifier.
- **Move** (`processMoveToDo :2711`). `CalculateMoveToPoints :4318` keeps the relative offsets; it is done when all ships
  arrive. Moves can enter a **holding pattern** (`clHoldingPattern :8050`, `TW_HOLDING_PATTERN_*` tweaks).
- **MP hyperspace** (`processMpHyperspaceingToDo :2452`): `COM_HYPERSPACE_START→OUT→WAIT→IN`
  (`CommandLayer.h:35-38`). Cost comes from `hyperspaceCost(distance, selection)` (`:2675`).
- **Build** (`clBuildShip :7481`): creates a `BUILDING_SHIP` command with `frameAtWhichToCreate`; on completion it calls
  `clCreateShip`, which places the ship at the creator and launches it.
- **Kamikaze, scuttle, tactics, parade** arrive as `MISCCOMMAND_*` in MP (`CommandNetwork.h:161-165`). Kamikaze sets
  `SPECIAL_Kamikaze` and uses `kamikazeDamage[]` (`Tactics.h`).

---

## 8. Tactics (`GoK:src/Game/Tactics.c`, `.h`)

- Each ship has `tacticstype ∈ {Evasive, Neutral, Aggressive}` (`ObjTypes.h:241-249`). The situation is
  `tactics_ordertype` (`NO_ORDER_NO_ENEMY … ATTACK_ORDERS_ENEMY`, `Tactics.h:20-26`). Bonuses apply to
  `Tactics_Fighter` / `Tactics_Corvette` only.
- `TacticsInfo` (`Tactics.h:73-115`, loaded from `Tactics.script`) holds MaxVelocityBonus, FuelBurnBonus, ManeuvBonus
  [situation][class][tactics], DamageBonus, BulletRangeBonus, BulletSpeedBonus, dodge parameters, retreat distances,
  the capital multipliers, formation padding (tight/loose), burst-fire time modifiers, and
  `holdFormationDuringBattle[]`.
- Per-ship `tacticsUpdate` (`Tactics.c:1810`) covers dodge, order status and manoeuvre. Global `tacticsGlobalUpdate`
  (`:1872`) covers retreat detection: `RetreatAtom/AttackAtom` in `universe.RetreatList/AttackMemory`, and attackers
  "remember" a retreater. Also here: `speedBurstUpdate :2100` and `tacticsGetShipsMaxVelocity :2169`, which the
  physics cap uses.
- Gun damage is **pre-multiplied per tactics** at load time: `gunDamageLo/Hi[NUM_TACTICS_TYPES]` from
  `baseGunDamageLo/Hi` (`SpaceObj.h:49-99`).

## 9. Formations and parade (`GoK:src/Game/Formation.c`, `FormationDefs.h`)

- Types: `DELTA 0, BROAD 1, DELTA3D 2 (shown as "X"), CLAW 3, WALL 4, SPHERE 5, CUSTOM 6, NO_FORMATION 7,
  SAME_FORMATION 8 (AI)`, plus `PARADE_FORMATION 100` (`FormationDefs.h`). Cataclysm's menu strings list the same seven
  plus Parade (`CET:assets/english.script` strMenuDeltaDesc…strMenuParadeDesc).
- `FormationCommand` (`Formation.h:53-74`): type, lock, coordsys, `tacticalState` (tight/loose), `travelvel` (slowest
  ship), `percentmaxspeed`, flip state, and `formationSpecificInfo`. Sphere layout uses precomputed declination tables
  (`SphereStaticInfo`, `Formation.h:26-46`).
- `formationArrangeOptimum` (`Formation.c:322`) assigns slots; `processFormationToDo` (`:972`) steers each ship to
  leader position + offset.
- The military parade (F12) arranges ships in `MilitarySlot`s around a carrier or mothership using per-mothership
  `Parade_*.script` files (`Formation.h:80-142`).

## 10. Docking (`GoK:src/Game/Dock.c`)

- Dock types are bit flags (`Dock.h:16-26`): `BOTHREFUELANDREPAIR 1, REFUEL 2, REPAIR 4, TO_DEPOSIT_RESOURCES 8,
  PERMANENTLY 16, FOR_RESEARCH 32, AT_SPECIFIC_SHIP 64, STAY_TILL_EXPLICITLAUNCH 128, INSTANTANEOUSLY 256,
  FOR_RETIRE 512`.
- Static dock points (`DockStaticPoint`: `DockingCone | LatchPoint | LaunchPoint`, cone, fly-away and up/heading
  directions) are parsed from `.shp` `DockPoint` lines. Their positions come from the MEX file.
- `clDock` (`:885`) chooses a target (`FindNearestShipToDockAt :638`, `ThisShipCanDockWith :550`).
  `processDockToDo` (`:6489`) calls `dockwith->staticinfo->ShipXDocksAtShipY(...)`, a receiver-specific state machine
  driven by `dockvars.dockstate/2/3`. The receivers are carrier/mothership (`ShipDocksAtCarrierMother :4403`, with door
  sequences `dockFlyToBottomOfDoor1-3`), ASF (`:3384`), resource controller (`:3672`), resource collector (`:3116`),
  repair corvette (`:2917`), research ship (`R1ResearchShipDocksAtResearchShip :5332`, `R2… :5754`), fuel pod (`:6205`),
  JunkYardHQ (`:2741`) and default (`:6021`). Launching mirrors this (`LaunchShipFromCarrierMother :4237`).
- Ships inside a carrier or mothership go to `ShipsInsideMe.insideList` (`dockPutShipInside :2306`). They are
  removed from space and refuelled/repaired by `repairOtherShipRate`/`pumpFuelRate`.
- **Master/slave stacking** (`SlaveInfo`, `SpaceObj.h:862-869`; `dockMakeMaster :2457`, `dockAddSlave :2483`,
  `dockUpdateSlaves :2675`) rigidly attaches ships. Slaves skip physics and damage is redirected to the master
  (§4.3). HW1 uses it for research-ship assemblies (`Dock.c:5546-5553, 5942-5951`). It is the natural base for
  Cataclysm's linked composite vehicles and mothership modules (inferred, UNVERIFIED).
- **Clamping** (`Clamp.c:27-75`: `clampObjToObj`, `updateClampedObject`) attaches salvage targets to salvagers.

## 11. Construction, research, unit caps

- **Construction manager** (`GoK:src/Game/ConsMgr.c`). There is one `shipsinprogress` per factory ship
  (`cmAddFactory :974`), with a `shipinprogress progress[TOTAL_STD_SHIPS]` job array. `cmBuildTaskFunction`
  (`:1105`) charges `buildCost/timeStart` RUs per tick using 16.16 fixed point (`:1177-1179`); in SP it deducts directly.
  `buildTime` is written in the `.shp` as integer **seconds** and stored as **frames**: `scriptSetFramesCB` does
  `sscanf("%d")` then `*= UNIVERSE_UPDATE_RATE` (`Universe.c:1408-1412`; table entry `:555`). CET's replacement hook does
  the same with its configurable rate (`CET:PatchUniverse.cpp:61-65`). In MP, cost and time flow through
  `cmDeterministicBuildProcess` (§5). Availability is `cmCanBuildShipType :3195` = research + unit caps + factory
  capability (`canBuildBigShips`).
- **Unit caps** (`Universe.c:4227-4436`: `unitCapCreateShip/DeleteShip/CanCreateShip`) use `cdLimitCaps[type]`,
  `cdClassCaps[class]` and `cdMaxShipsAllowed` (from `UnitLimitCaps.script`), toggled by `MG_UnitCapsEnabled`.
- **Research** (`GoK:src/Game/ResearchAPI.c`). `TechnologyType` has 30 entries, `NewAlloys…SalvageTech`
  (`ResearchAPI.h`). The player holds `HasTechnology` as a **udword bitmask** (`TechToBit`, `AllTechnology = 0x7FFFFFFF`).
  There are 6 labs (`NUM_RESEARCHLABS`), and a lab exists per Research Ship (`rmActivateFreeLab :534`,
  `rmDeactivateLab :493`). Several labs on one topic speed it up per `labdec[] = {0, 1.0, 1.6, 2.25, 2.8, 3.25, 3.6}`
  (`:47-56`). Completion happens in `rmUpdateResearch` (`:559-650`), which plays speech and updates the build manager.
  Dependencies and times come from `ResearchDepend.script` (`:996-998`) with keys `SPR1ShipDepend`, `SPR1TechDepend`,
  and `SPR1<Tech>` times for SP/MP/MPC × R1/R2 (`:27-32, 58-226`). Ships can be enabled or disabled with
  `rmEnableShip :1090`. Cataclysm `rmUpdateResearch` is at `0x004F2A20`; `rmCanBuildShip(Player*, ShipType, int)` is at
  `0x004F2630` and **gained a third parameter** (`CET:Cataclysm.h`).

## 12. Resources, salvage, trade, retire, crates, bounties

- **Harvest**: `processCollectResource` (`ResCollect.c:1014`) → `CollectResources` (`:575`). Every `harvestRate+1`
  frames (`UNIVERSE_WOODPECKER(harvestRate, 0)`, `:596`) the collector takes `harvestAmount` RUs, up to
  `maxresources`/`resourcesAtOneTime`, then docks `TO_DEPOSIT_RESOURCES`. Asteroids shrink and break
  (`AsteroidTakesDamage :539`, `BreakAsteroidUp :482`). Resources regrow inside `ResourceVolume`s
  (`univCheckRegrowResources`, `UnivUpdate.c:7181`). CET hooks the harvest-rate check at `0x004F17C9`, inside the
  Cataclysm `CollectResources`.
- **Salvage/capture** (`GoK:src/Ships/SalCapCorvette.c`). The states run from `SALVAGE_BEGIN` through `SAL_FLYTOTARGET`,
  `SAL_DOCKINGSTATE1`, `SAL_CLAMPED`, `SAL_STRIPTECH` (derelict technology) or `SAL_SALVAGE1`, the tow through
  `SAL_FLYBACK_TO_DOCKING_SHIP`, and `SAL_DO_DOCK*` (`:43-72`). The target needs `numNeededForSalvage` latched
  salvagers at its `SalvageStaticPoint`s (`.shp NUMBER_OF_SALVAGE_POINTS`, `Universe.c:663`). Damage limits come from
  `twSalvageDamageLimit[class]` (Cataclysm tweak). Captured ships go into the receiver's `CAP*` retrofit queue
  (`Carrier.h`, `Mothership.h`) and pop out after `SALVAGE_MIN/MAX_RETROFIT_TIME`. The result is counted as
  `DEATH_I_DIDNT_DIE_I_WAS_SALAVAGED` (1004).
- **Trade** (`GoK:src/Game/TradeMgr.c`): the Bentusi tech shop. `tmTechForSale[]`, `tmTechResearchable[]` and
  `TM_NUM_TECHS 50` (`TradeMgr.h`) are driven by KAS `TraderGUI*`, `TechSetPurchase` and `TechSetCost`. `wkTradeUpdate`
  (`:1542`) animates the trade ships.
- **Retire**: `DOCK_FOR_RETIRE` refunds `buildCost * TW_RETIRE_MONEYBACK_RATIO` (0.6 in Cataclysm `tweak.script`).
  `DEATH_BY_RETIREMENT` is 1001.
- **RU transfer between allies**: `clRUTransfer` (`CommandLayer.c:6601`) / `COMMANDTYPE_RUTRANSFER`.
- **Crates** (`Crates.c:655 cratesUpdate`, `MG_CratesEnabled`) give a ship, RUs or research (EGT_CRATE_* effects,
  `ObjTypes.h:290-293`). **Bounties** (`Bounties.c:38, 231`).

## 13. AI architecture (`GoK:src/Game/AI*.c`)

```
aiplayerUpdateAll (AIPlayer.c:828)           every tick; per AI player when WOODPECKER(aiplayerUpdateRate, i & rate)
 ├─ SP only: kasExecute() for players[1]      AIPlayer.c:836-852 (call at :849) (KAS = mission brain, §14.4)
 └─ aiplayerPlay → aifFleetCommand (AIFleetMan.c:1252)   "fleet manager"
      ├─ aiuCreateBlobArrays / aiuUpdateKnowledgeOfEnemyShips / aidClearDistressSignal / aifAssignNewShips
      ├─ first turn: aiaProcessSpecialTeams
      ├─ aifSkimHyperspaceRUs (if AIF_HYPERSPACING)
      ├─ aiaAttackManager()   AIAttackMan.c   → requests ships via aifAttackManRequestsShipsCB
      ├─ aidDefenseManager()  AIDefenseMan.c
      ├─ airResourceManager() AIResourceMan.c (collectors, controllers, ASF, research ships)
      ├─ aitExecute()         AITeam.c:2089   per team: run curMove; process*Move; advance list
      └─ aifProcessShipBuildRequests (→ clWrapBuildShip)
```

- **AITeam** (`AITeam.h:384-414`): ship list, team flags (`AITeam.h:20-49`: harass, recon, armada, cloak co-op…),
  strength, an ordered **list of `AITeamMove`**, and a message queue. A `ScriptTeam` stores the **KAS FSM name, state
  name and watch-function pointers** (`kasFSMName/kasStateName/kasFSMWatchFunction/kasStateWatchFunction`).
- **AIMoves** (`AIMoves.c`): 49 move types `MOVE_DONE … MOVE_DELETETEAM` (`AITeam.h:60-111`), each with
  `aimCreate*`, `aimProcess*`, `*ShipDied`, `*Close` and save/fix routines. The P2 (Turanic) race has its own fleet
  command (`aifP2FleetCommand`).
- **AI ship-level** (`AIShip.c`, `AITrack.c`) is the steering for everyone, not just the CPU.
- Difficulty: `AI_BEGINNER/INTERMEDIATE/ADVANCED` (`AIPlayer.h:19-25`); team levels `TEAM_BEGINNER…TEAM_ADVANCED`.
  The SP enemy is always `AI_ADVANCED` (`SinglePlayer.c:2765`).
- Cataclysm keeps `AIPlayer` (`CET:game/Game/AIPlayer.h`, offsets asserted to 0x10). The skirmish fleet logic is
  replaced by "ais": `aisFleetUpdate 0x00431750`, `aisRequestShip 0x00431510`, and a table
  `AISTeamEntry aisTeams[34]` at `0x008DFD60` (each 0x9D10 bytes, `CET:game/Game/AISkirmish.h`). Its team roles are
  Scout, Fighter, AntiFighter, Sentinel, MediumCombat, Main, MothershipModules, CarrierSupport1/2, MothershipSupport,
  MothershipBigGun, Carrier, Mothership, Processor, Mimic, CruiseMissile, Leech, DFGFrigate, Worker, CrystalSalvage, and
  the `*Enemy` variants. Its targets are tuned by `aiskirmish.script` (`AIS_*_TARGET_COUNT[AI_BEG/INT/ADV]`,
  `CET:assets/aiskirmish.script`). AI externally-built ships use a timer. CET patches
  `aiUpdateExternalShipBuild` at `0x00543F48`/`0x00543F51` (patch sites inside the function, not its entry). Related
  globals (`CET:Cataclysm.h:249-258`): `aiBuildingShip[]` 0x008DFC64, `aiCarrier1Ship` 0x008DFC38, `aiCarrier2Ship`
  0x008DFC3C, `aiHasExternalConstruction` 0x00A2D984, `aiHasCarrier1ModuleQueued` 0x00A2D98C,
  `aiHasSupportModuleQueued` 0x00A2D994, `aiHasCarrier2ModuleQueued` 0x00A2D9A8, `aiSupportUnitsPending` 0x00A2D9B4.

## 14. Single player and missions

### 14.1 A mission = data + code

| Part | Format | Location (HW1) |
|---|---|---|
| Layout: ships, teams, points, paths, volumes, resources, derelicts, lighting, background | text `.mission` (scriptSet tables `"Ships"`, `"AIPoint"`, `"AIPath"`, `"AIBox"`, `"AISphere"`, `"Resources"`, `"Derelict"`, `"MissionSphere"`, `"Lighting"`, `"Background"`; `GoK:src/Game/LevelLoad.c:111-199`) | `SinglePlayer/MissionNN/MissionNN.mission` in the `.big` (`SinglePlayer.c:410-420`) |
| Logic: objectives, FSMs, dialogue, triggers | **KAS → C, compiled into the EXE** | `GoK:src/Missions/SinglePlayer/MissionNN.kas` (17 files = Mission01-16 plus the OEM demo variant `Mission05_OEM.kas`; 42,581 lines with the two `.kash`). The tutorial is separate: `GoK:src/Missions/Tutorials/Tutorial1.kas` |
| Cinematics | `.nis` and NIS scripts, started via the AIVars `"PlayNis"`/`"PlayNislet"` | `SinglePlayer.c:2401-2440, 3239-3331` |
| Per-mission tweaks | `SinglePlayer.script`, `n01r1.script` etc. | (string list from `scriptSet` calls) |

### 14.2 KAS language ("Kick Ass Script", `HWSDL:documentation/Glossary.txt`)

The grammar is in `GoK:tools/kas2c/parser.y`; the lexer is `lexer.l` built with `flex -i`, so it is case-insensitive
(`GoK:tools/kas2c/meson.build`). Structure:

```
LOCALIZATION  LSTRING_name "en" "fr" "de" "es" "it" ... ENDL
FSM name  STATES s1, s2, ...
   INIT ... ENDINIT   WATCH ... ENDWATCH          (FSM-level)
   STATE s1  INIT ... ENDINIT  WATCH ... ENDWATCH  ENDSTATE s1
ENDFSM name
INIT ... ENDINIT   WATCH ... ENDWATCH              (mission-level, end of file)
```

Statements are `IF/ELSEIF/ELSE/ENDIF`, `IFONCE … ENDIFONCE`, `JUMP state`, `FSMCREATE(fsm, TEAM_x)`, and calls to
**302 built-in functions**. These are table-mapped to C `kasf*` functions with typed parameters
(`GoK:tools/kas2c/kas2c.c:62-…`). Examples: Timer*, Var*, Msg*, Attack*, Formation*, Ships*/Team* selection queries,
Objective*, Tech*, TraderGUI*, RUs*, Speech/Sound/Music events, wideScreenIn/Out, Pause*, HyperspaceDelay, Ping*,
Tut* (tutorial) and Builder*. Entity references are typed prefixes: `TEAM_x`, `SHIPS_x`, `TEAMSHIPS_x`, `POINT_x`,
`PATH_x`, `VOLUME_x`, `SHIPSPOINT_x`… (`lexer.l`). The `.kas` files are C-preprocessed first, so they `#include`
engine headers (`SpeechEvent.h`, `SoundMusic.h`, `Attributes.h`) and use their enums as numbers
(`Mission02.kas:5-10`).

### 14.3 kas2c output (what the binary contains)

`gcc -E -x c MissionNN.kas > .kp` then `kas2c MissionNN.kp MissionNN.c MissionNN.h MissionNN.functions.c`
(`GoK:tools/kas2c/README.txt`; meson rules in `GoK:src/Missions/SinglePlayer/meson.build`). The generated C:

- One `void Init_<Mission>()`/`Watch_<Mission>()` pair, one `Init_<Mission>_<FSM>`/`Watch_<Mission>_<FSM>` pair per FSM,
  and one `Init_<Mission>_<FSM>_<State>`/`Watch_…` pair per state (`kas2c.c:1505-1680`). Each begins by setting
  `CurrentMissionScope` and `CurrentMissionScopeName` (`:1490-1502`).
- `JUMP s` → `kasJump("s", Init_…_s, Watch_…_s)` (`kas2c.c:1921`). `FSMCREATE` →
  `kasFSMCreate("FSM", Init_…, Watch_…, kasAITeamPtr("label"))` (`:1365`).
- `IFONCE` → `if (!kasfVarValueGet("_VIFONCE%03d") && (...)) { kasfVarCreateSet("_VIFONCE%03d",1); ... }`
  (`parser.y:181`).
- Entity references → `kasAITeamPtr("x")`, `kasAITeamShipsPtr("x")`, `kasShipsVectorPtr("x")`… (`parser.y:230-247`).
  Localised text → `static char *LSTRING_x[] = {"en", "fr", ...}`, indexed `[strCurLanguage]` (`kas2c.c:1751, 1789`; `LSTRING_COUNT_REQUIRED 5`, `kas2c.h:27`).
- `MissionNN.functions.c` is an ordered `void*` array of every Init/Watch function, used for savegames. It is an
  **SDL-port addition**: the kas2c README says portable savegames were "never done in the original version of
  Homeworld" (`GoK:tools/kas2c/README.txt:4-6`), so the original EXE is not expected to contain these arrays.

### 14.4 Runtime (`GoK:src/Game/KAS.c`)

- `singlePlayerKasMissionStart(mission)` (`SinglePlayer.c:2909-2935`) calls `kasMissionStart("missionNN", Init_MissionNN,
  Watch_MissionNN)` (`KAS.c:232`), which stores the mission watch, inits hyperspace, and runs Init.
- `kasExecute()` (`KAS.c:192-230`) runs the mission Watch, then for every AI team with `teamType == ScriptTeam` its FSM
  Watch and its current state Watch. `PauseOtherKAS` restricts this to one team (`kasUnpausedTeam`).
- It is called from `aiplayerUpdateAll` for SP player 1 when `WOODPECKER(aiplayerUpdateRate, SP_KAS_UPDATE_FRAME &
  rate)` (`AIPlayer.c:836-852`). It therefore runs **inside univUpdate**, deterministically. Cadence: `aiplayerUpdateRate
  = AIPLAYER_UPDATE_RATE[level]` with defaults `{63, 31, 15}` for BEGINNER/INTERMEDIATE/ADVANCED (`AIPlayer.c:30, 723`;
  tweakable from `AIPlayer.script`, `:204-206`). The SP enemy is `AI_ADVANCED`, so KAS runs once every 16 ticks (1 Hz
  at 16 Hz) on ticks with `counter & 15 == SP_KAS_UPDATE_FRAME` (0 in Cataclysm `tweak.script:752`). Each computer
  player `i` runs `aiplayerPlay` on its own phase `i & rate` (`AIPlayer.c:861-865`). Cataclysm's `AIPLAYER_UPDATE_RATE`
  values are UNVERIFIED (CET ships no `AIPlayer.script`). KAS is skipped entirely while
  `singlePlayerGameInfo.hyperspaceState != NO_HYPERSPACE` (`AIPlayer.c:837-840`); that early `return` leaves the whole
  `aiplayerUpdateAll`, so in SP the computer players and the tutorial branch pause too. The tutorial uses a separate
  branch, `WOODPECKER(TUTORIAL_KAS_UPDATE_MASK, TUTORIAL_KAS_UPDATE_FRAME)` = every other tick (`AIPlayer.c:871-876`),
  plus the not-running path in `universeUpdateTask` (§3.2).
- `kasJump` (`:133`) stores the state name and watch pointer, then runs Init immediately. `kasFSMCreate` (`:152`) turns
  an AITeam into a ScriptTeam.
- Mission flow is controlled from KAS via AIVars (`aivar*`), for example `kasfMissionCompleted` (`KASFunc.c:115`) and
  `kasfMissionFailed` (`:134`).

### 14.5 Savegames and function pointers

KAS state is saved as **indices into the mission's function list**: `kasConvertFuncPtrToOffset` and
`kasConvertOffsetToFuncPtr` (`GoK:src/Game/KAS.c:1385-1440`, via `MissionEnumToFunctionList`, `SinglePlayer.c:2968-3054`).
According to `GoK:tools/kas2c/README.txt`, the original Homeworld did **not** have portable savegames; the index
scheme is an SDL-port change. The original encoding (likely raw code addresses or a base-relative offset) is
UNVERIFIED. It must be recovered from Cataclysm's `kasSave/kasLoad` if loading original `.SAV` files is a goal.
`CustShip_PreFix/_Fix` do the same pointer-to-index conversion for ship Specs.

### 14.6 Why Cataclysm's missions are compiled code in the EXE, and how to recover them

- In HW1 the only mission logic is the kas2c-generated C, linked into the executable (`src/Missions/*` → build;
  `HWSDL:documentation/Glossary.txt` "converted by tools/kas2c into compilable C code"). The runtime has **no
  interpreter**: `kasExecute` calls C function pointers. Cataclysm is the same engine (`CET:README.md:54-55`: CET was built by
  reverse engineering with insights from the HW1 source, and "The source code for Cataclysm is unfortunately lost"), so its missions are expected to be
  kas2c-generated functions inside `cataclysm.exe`. CET's README names "Mission 17" (`CET:README.md:40`), which proves at
  least 17; the exact total of 17 is not stated in any reference file (UNVERIFIED). This is inferred: UNVERIFIED until
  the EXE is available.
- **Evidence that the KAS runtime survives in Cataclysm:** the Cataclysm string table defines debug key bindings
  `Kas_Dump`, `Toggle_Draw_KAS_States`, `…_Timers`, `…_Vars`, `…_Volumes`, `Toggle_Draw_KAS_All` and
  `Inc_KAS_Skip_Factor` (`CET:assets/english.script:1546-1559`). They map one-to-one onto HW1's KAS debug globals
  `mrKASDebugDrawStates/Timers/Vars/Volumes` and `mrKASSkipFactor` (`GoK:src/Game/KAS.c:52-56`).
- **Fingerprints for finding them in the binary:**
  - call sites of `kasJump`/`kasFSMCreate`/`kasMissionStart` taking string literals (state, FSM and mission names);
  - the literals `"_VIFONCE001"`, `"_VIFONCE002"`… (the `"_VIFONCE%03d"` format string lives in kas2c, not in the game);
  - arrays of 5 (or more) localised `char*` (`LSTRING_*`);
  - dense runs of `kasf*` calls with label strings (`kasAITeamPtr("…")`);
  - per-mission function-pointer arrays for save/load exist only in the SDL port (§14.3), so do not expect them;
    if Cataclysm saves raw `Init_*/Watch_*` addresses instead (UNVERIFIED, §14.5), those pointers inside a `.SAV`
    point straight at the mission functions.
- **Port strategy (recommendation).** Decompile each Init/Watch function back to C, preserving names as
  `Init_<Mission>_<FSM>_<State>` from the strings. Because kas2c output is mechanical (§14.3), a small "c2kas" lifter
  can turn the decompiled C back into KAS source for editing.

### 14.7 Single-player flow

- The mission order comes from `missionSequence[]` (`SinglePlayer.c:95-112`). Between missions the fleet hyperspaces:
  `hyperspaceState` (`SinglePlayer.h:43`), `spHyperspaceSelectionOut/In`, roll call, and ships that are not docked get
  `DEATH_Left_Behind_At_Hyperspace`. `singlePlayerLoadNewLevel` (`SinglePlayer.c:2746-2838`) deletes the remaining
  enemies, `universeReset`s while **keeping the player's fleet and RUs**, re-inits the AI (`AI_ADVANCED`), loads the next
  `.mission`, and starts KAS.
- **Dynamic difficulty**: `SetFleetModifier` (`SinglePlayer.c:608-660`) maps the player's fleet strength between
  `BaseFleetStrengthForLevel` and `AdvancedFleetStrengthForLevel` into `spFleetModifier ∈ [0,1]`, which KAS uses to scale
  enemy forces. Cataclysm's equivalent is UNVERIFIED.
- `SinglePlayerGameInfo` sits at `0x00C0A5C0` in Cataclysm, with `hyperspaceState` at 0x10 as in HW1
  (`CET:game/Game/singleplayer.h:137`).

## 15. Multiplayer game rules

- Flags in `tpGameCreated.flag` (`GoK:src/Game/MultiplayerGame.h:42-55`): `MG_LastMotherShip, MG_CaptureCapitalShip,
  MG_ResearchEnabled, MG_HarvestinEnabled, MG_ResourceInjections, MG_ResourceLumpSum, MG_PasswordProtected,
  MG_UnitCapsEnabled, MG_FuelBurnEnabled, MG_AlliedVictory, MG_CratesEnabled, MG_CarrierIsMothership, MG_Hyperspace`.
  The settings struct is `CaptainGameInfo` (`GoK:src/SDL/TitanInterfaceC.h:152-185`): starting fleet and resources,
  bounty size, injection interval/amount in **univupdate frames**, AI difficulty/bigotry, and per-player info.
  Cataclysm's struct is 688 bytes (`CET:game/Win32/TitanInterfaceC.h:149`). `tpGameCreated` is at `0x00886028`.
  CET rewrites the injection UI to convert minutes to frames at the current rate (`CET:game/Game/MultiplayerGame.cpp`).
- Defeat (`univKillOtherPlayersIfDead`, `UnivUpdate.c:4334`). When the mothership dies, the player looks for a backup
  (`univFindBackupMothership :4304`: a Carrier or CLASS_Mothership becomes `PlayerMothership`). If there is none, the
  player dies when `MG_LastMotherShip` is set or the player has 0 ships. A player with 0 ships dies in any case.
- Victory (`CheckPlayerWin :4142`): at most one player left alive, or all living players allied (`MG_AlliedVictory`).
  This sets `wintime/quittime`. Cataclysm checks the game type at `0x0054860C` (`CET:Cataclysm.h`).
- RU regeneration (§3.3 step 4), injections and lump sum (step 10), bounties (step 25), crates (step 15) and
  alliances (`Alliance.c`; Cataclysm `allianceFormWith 0x00420880`) complete the rule set.

---

## 16. Cataclysm gameplay additions to expect in the binary

Sources: `CET:game/Game/*.h` (reconstructed from the GOG EXE), `CET:assets/RACE_*/*.shp`, `CET:assets/english.script`,
`CET:assets/tweak.script`, `CET:assets/aiskirmish.script`, `CET:Cataclysm.h`. Anything else is labelled.

### 16.1 Races, classes, ship types

- Races (`CET:game/Game/RaceDefs.h`): `RACE_Sect 0` (Somtaaw, the old R1 slot), `RACE_Beast 1` (the R2 slot),
  `RACE_Kushan 2`, `RACE_Taiidan 3`, `RACE_P1 4`, `RACE_Traders 5`, `RACE_Derelict 6`. The string table confirms
  it: `strR1TECH_*info` is the Somtaaw text and `strR2TECH_*info` the Beast "SELF" text (`english.script:299-388`).
  The data directories are `RACE_SECT/`, `RACE_BEAST/` (`CET:assets/`).
- Classes (`CET:game/Game/ClassDefs.h`): the HW1 0-8 plus `CLASS_MonsterShip 9, CLASS_Component 10, CLASS_Micro 11`.
  `tweak.script` also references `CLASS_Composite`, and `english.script` has `strCLASS_Composite/Component/Micro` and
  `strCLASS_SuperCapital`. The numeric value of Composite is UNVERIFIED. The data contradicts CET's numbering: the
  string table lists `strCLASS_Composite, Component, Micro` in that order (`english.script:175-177`), and the
  class-indexed tweak arrays list `…NonCombat, Composite, Component, Micro, MonsterShip`
  (`tweak.script:828-840`, `twRepairRate[]`). Composite may therefore be 9 and MonsterShip 12 (`NUM_CLASSES` 13, not
  12). Settle it from the binary's `shipclassinfo` `NumStrXlate` table; `Player.classtotals[NUM_CLASSES]` depends on it.
- New explosion type `ET_DestroyerExplosion` (`sDreadnought.shp`, `sProcessor.shp`, `bProcessor.shp`, `bHeavyCruiser.shp`;
  `sDestroyer.shp:274` uses `ET_CarrierExplosion`); it is not in the HW1 enum.
- **145 ship types**, flat `#define`s (`CET:game/Game/ShipDefs.h`). Somtaaw 0-32, Beast 33-59, Kushan k* 60-73, Taiidan
  t* 74-91, Turanic p1* 92-97, Traders tr* 98-108, derelict/scenario d* 109-141 (Naggarok = `dRKO`, slipgates,
  crystals, Clee-San, prison ship…), NIS light sources 142-144. Whether the d* types are instantiated as `Ship` or
  `Derelict` objects is UNVERIFIED.

Somtaaw and Beast roster. SU means support units (from `english.script:734-795` info texts and `.shp` `supportUnits`/
`providesSU`). Class is from `.shp` where CET ships one.

| ID | Somtaaw (s*) | Name | Class / SU | ID | Beast (b*) | Name | Class / SU |
|---|---|---|---|---|---|---|---|
| 0 | sRecon | RECON | 2 | 33 | bRecon | RECON | 2 |
| 1 | sAcolyte | ACOLYTE | 2 | 34 | bAcolyte | ACOLYTE | 2 |
| 2 | sMimic | MIMIC | Fighter / 3 | 35 | bInterceptor | INTERCEPTOR | 2 |
| 3 | sLeech | LEECH | 2 | 36 | bCloakedFighter | CLOAKED FIGHTER | 2 |
| 4 | sACV | ACV (2 Acolytes linked) | 4 | 37 | bAttackBomber | ATTACK BOMBER | 3 |
| 5 | sMCV | MCV (2 Mimics linked) | Corvette / 6 | 38 | bACV | ACV | 4 |
| 6 | sRammingFrigate | RAMMING FRIGATE (heavy tug) | 10 | 39 | bMultiGunCorvette | MULTIGUN CORVETTE | Corvette / 5 |
| 7 | sMultiBeamFrigate | MULTIBEAM FRIGATE | Frigate / 12 | 40 | bHeavyCorvette | HEAVY CORVETTE | Corvette / 5 |
| 8 | sHiveFrigate | HIVE FRIGATE (6 swarmers) | Frigate / 20 | 41 | bMultiBeamFrigate | MULTIBEAM FRIGATE | Frigate / 12 |
| 9 | sSwarmer | (hive-launched) | Micro / 0 | 42 | bHiveFrigate | HIVE FRIGATE | Frigate / 20 |
| 10 | sDreadnought | DREADNOUGHT | HeavyCruiser / 80 | 43 | bSwarmer | (hive-launched) | Micro / 0 |
| 11 | sDreadnoughtCannon | CANNON (component) | — | 44 | bIonArrayFrigate | ION ARRAY FRIGATE | Frigate / 15 |
| 12 | sCarrier | CARRIER (+40 SU, builds) | Carrier / 0 | 45 | bDFGFrigate | DEFENCE FIELD FRIGATE | Frigate / 10 |
| 13 | sCarrierSupport | SUPPORT MODULE (+12 SU) | Component | 46 | bHeavyCruiser | HEAVY CRUISER | HeavyCruiser / 70 |
| 14 | sWorker | WORKER | 4 | 47 | bCarrier | CARRIER (+96 SU) | Carrier / 0 |
| 15 | sRepairDroid | REPAIR DROID | — | 48 | bRammingFrigate | RAMMING FRIGATE | 10 |
| 16 | sProcessor | PROCESSOR | Resource / 13 | 49 | bWorker | WORKER | Resource / 4 |
| 17 | sSentinel | SENTINEL | 3 | 50 | bRepairDroid | REPAIR DROID | — |
| 18 | sMothership | COMMAND SHIP (+70 SU) | Mothership / 0 | 51 | bProcessor | PROCESSOR | Resource / 13 |
| 19 | sMothershipSpecial | ADV ENGINEERING MODULE | Component | 52 | bSentinel | SENTINEL | 3 |
| 20 | sMothershipEngineering | RESOURCE PROCESSING | — | 53 | bMothership | COMMAND SHIP | — |
| 21 | sMothershipArmour | ARMOR MODULE | — | 54 | bMothershipDockingBay | HANGAR MODULE | — |
| 22 | sMothershipMicro | ENGINEERING MODULE | — | 55 | bMothershipEnginesLower | LOWER ENGINES | — |
| 23 | sMothershipWeapons | WEAPONS MODULE | — | 56 | bSectMoship | GENETICS LAB MODULE | — |
| 24 | sMothershipDockingBay | HANGAR MODULE (+30 SU) | Component | 57 | bMothershipSupport | SUPPORT MODULE (+15 SU) | — |
| 25 | sDestroyer | DESTROYER | Destroyer / 38 | 58 | bCruiseMissile | CRUISE MISSILE | Corvette / 4 |
| 26 | sMothershipSupport | SUPPORT MODULE (+10 SU) | Component | 59 | bMissileCorvette | MISSILE CORVETTE | Corvette / 4 |
| 27 | sMothershipCommand | COMMAND CENTER | — | | | | |
| 28-30 | sMothershipEnginesLower/Upper, LowerSpine | (mothership parts) | — | | | | |
| 31 | sMothershipBigGun | SIEGE CANNON | — | | | | |
| 32 | sMothershipOreCanArmour | ORE CANISTER | — | | | | |

### 16.2 New `ShipStaticInfo` script fields (`CET:game/Game/spaceobj.h:557-758`, asserted offsets `:760-916`)

`externalBuild` (0x1F0), `forceFieldProtectionDistanceScale` (0x1F4), `upgradeTimeModifier` (0x1F8), `fowRadius` (0x240),
`RKOEatMeTime` (0x250), `nlips` (0x30C), `nRepairDroids` (0x320; `.shp` key `repairDroids`), `hyperspaceBoxScale`,
`leechRUsPerSecond`, `leechHPsPerSecond`, `beastAttackCascadeCost`, `beastAttacksRequired`, `beastAttackConvertTo`,
`Overlay_Selection_Scalar` (not new: an HW1 key, `GoK:src/Game/Universe.c:664`, filling `tacticalSelectionScale`),
`bUseRayTriangleIntersectionForHitLocation`, `linkDistance`, `supportUnits`, `providesSU`,
`RamDamage`, `cuttingBeamDamagePerSecond`, `cuttingBeamUpgradedDamagePerSecond`, `RamPushTime`, `RamWaitAfterPushTime`,
`TimeShockedByEMP`, `EMP_Range`, `EMP_Max_Combine_Distance`, `UpgradeByDocking`, `pilotViewOffsetX/Y/Z`, `RKOGetsMissiles`.
CET marks the location of every field after `RKOGetsMissiles` (:731) as unknown (`:733`).

Asserted Cataclysm offsets for the rest (`spaceobj.h:893-916`):

| Field | SSI+ | Field | SSI+ | Field | SSI+ |
|---|---|---|---|---|---|
| `hyperspaceBoxScale` | 0x3D0 | `linkDistance` | 0x40C | `RamWaitAfterPushTime` | 0x428 |
| `leechRUsPerSecond` | 0x3D4 | `supportUnits` | 0x410 | `TimeShockedByEMP` | 0x430 |
| `leechHPsPerSecond` | 0x3D8 | `providesSU` | 0x414 | `EMP_Range` | 0x434 |
| `beastAttackCascadeCost` | 0x3DC | `RamDamage` | 0x418 | `EMP_Max_Combine_Distance` | 0x438 |
| `beastAttacksRequired` | 0x3E0 | `cuttingBeamDamagePerSecond` | 0x41C | `UpgradeByDocking` | 0x43C |
| `beastAttackConvertTo` | 0x3E4 | `cuttingBeamUpgradedDamagePerSecond` | 0x420 | `pilotViewOffsetX/Y/Z` | 0x440/0x444/0x448 |
| `Overlay_Selection_Scalar` | 0x3E8 | `RamPushTime` | 0x424 | `RKOGetsMissiles` | 0x454 |
| `bUseRayTriangleIntersectionForHitLocation` | 0x408 | | | | |

`RamDamage`, `RamPushTime`, `RamWaitAfterPushTime`, `cuttingBeam*`, `EMP_Range` and `EMP_Max_Combine_Distance` occur in
none of the 27 `.shp` files CET ships (grep over `CET:assets/RACE_*`), so their script spelling is UNVERIFIED. The new instance fields on `Ship`
are `buildProgress` (0x170), `buildTimerElapsed` (0x3E8), `Timer buildTimer` (external build), and
`BeastSystemType beastSystemType` (enum `BS_None, BS_Thrust, BS_Infection, BS_Defensive, BS_Research, BS_Build`,
`CET:game/Game/objtypes.h`) (`spaceobj.h:1517-1782`; buildTimer :1742, beastSystemType :1778).

### 16.3 Support units replace HW1 unit caps

`Player.supportUnitsMax/Used` (`CET:game/Game/universe.h:115-116`); `supportUnits` is the cost and `providesSU` the
capacity. The command ship gives +70, carriers +40 (Somtaaw) or +96 (Beast), support modules +10 (`sMothershipSupport`)
and +12 (`sCarrierSupport`), and the hangar module +30 (`.shp` `providesSU`, `CET:assets/RACE_SECT/*.shp:37-39`). The
Beast support module's +15 comes only from its info text (`strbMothershipSupportInfo` "SU BONUS: 15"; CET ships no
`bMothershipSupport.shp`). The data disagree for the carrier support module: `.shp` 12 versus info text
`strsCarrierSupportInfo` "SU BONUS: 10" (`english.script`); the binary's `.big` copy decides. The text: "AN EFFECTIVE FLEET IS LIMITED BY THE AMOUNT OF LOGISTICAL SUPPORT …
BUILDING MORE SUPPORT MODULES … WILL ALLOW FOR A LARGER ARMADA" (`english.script` `strR1TECH_SUPPORT_SYSTEMSinfo`).
Expect `unitCap*`/`cmClassCapCanCreateShip` to be rewritten around SU (inferred).

### 16.4 Modules and external construction

- The Somtaaw mothership is assembled from module ship types (19-24, 26-32). The research modules are
  `ModuleDefs.h`: `MODULE_HANGAR 0, ARMOUR 1, ENGINEERING 2, ADVANCED_ENGINEERING 3, WEAPON 4`.
  Each module gates a fixed tech list (`english.script` `strHangarModuleinfo`, `strArmourModuleinfo`,
  `strEngineeringModuleinfo`, `strAdvEngineeringModuleinfo`, `strWeaponModuleinfo`, lines 393-397).
  `CET:PatchUniverse.cpp:193-215` lists all modules for the mothership self-damage fix, and the engine has
  `beastMothershipSelfDamage(ShipStaticInfo*)` at `0x005BC900`.
- `externalBuild TRUE` (carriers, destroyers, dreadnought, processor, modules, heavy cruiser) means the ship is
  constructed in space by a timer (`Ship.buildTimer`, `buildProgress`) instead of inside a hangar. The AI uses
  `aiUpdateExternalShipBuild`, which computes `buildTime / rate` (`CET:PatchUniverse.cpp:179-189`).
- How modules attach to the hull (master/slave per §10, or a new mechanism) is UNVERIFIED.

### 16.5 Research: 41 technologies, "upgrades"

- The tech list, in string-table order (`english.script:214-257`): `TECH_CLOAKING, TARGETING_SYSTEMS, PLASMA_WEAPONS,
  MEDIUM_GUNS, DEFENSE_FIELD, HEAVY_GUNS, REPAIR, SALVAGE, DRIVE_FIGHTER, ADRIVE_FIGHTER, DRIVE_FRIGATE, ADRIVE_FRIGATE,
  DRIVE_DESTROYER, ADRIVE_DESTROYER, ARMOUR2, ARMOUR3, SUPPORT_SYSTEMS, MICRO_SHIP, ENERGY, ION, MISSILE, REPULSE, EMP,
  EXPLOSIVE_CHARGE, LINK, MIMIC, AFTERBURNER, BIG_GUN, ADVANCED_SENSORS, FORCE_FIELD1..3, INFECT, REPAIRBOTS,
  CRYSTAL_HARVEST, RESEARCH_CLEESAN_ATTACK, BEAST_VACCINE, PHASED_TELEMETRY, INERTIALESS_DRIVE, BIOWARHEAD,
  DRIVE_CORVETTE`. The enum order probably matches this list, but that is UNVERIFIED.
- **41 > 31**, so HW1's `udword HasTechnology` bitmask with `AllTechnology 0x7FFFFFFF` cannot hold it. Expect a wider
  mask or an array (inferred). CET's `researchapi.h` is an unmodified HW1 copy and is not authoritative.
- Many techs are **upgrades applied to existing ships** ("UPGRADE FOR …" texts; menu `strMenuUpgradeSectShipsDesc
  "Upgrade"` / `strMenuUpgradeBeastShipsDesc "Mutate"`; key `Upgrade_Ships`). The upgrade time scales with
  `upgradeTimeModifier`. `UpgradeByDocking TRUE` (Mimic, MCV) means the ship must dock to receive an upgrade.
- **Weapon upgrade levels in GUN blocks.** Cataclysm `.shp` GUN blocks drop HW1's `SoundType/BulletType/DamageLo/DamageHi`
  (`GoK:src/Game/StatScript.c:96-127`) and add `GunSize` (`GUNSIZE_SMALL/MEDIUM/LARGE`), `StartUpgradeLevel/EndUpgradeLevel`
  (`WEAPON_PROJECTILE, WEAPON_ENERGY, WEAPON_ION, WEAPON_MISSILE, WEAPON_BIGGUN, WEAPON_BEASTINFECT`), `DamageMult`,
  `FireTime2` and `BulletSpeed2`. The new gun types are `GUN_HiddenGimble` and `GUN_VisibleGimble`.
  `bMultiGunCorvette.shp:88-233` spells six sizes `GUNSIZE_Medium`. That only works if the gun-size lookup is
  case-insensitive, as HW1's `NumStrXlate` lookup is (`strcasecmp`/`_stricmp`, `HWSDL:src/Game/ObjTypes.c:18, 493`);
  inferred, so keep the lookup case-insensitive in the port.
  - Inferred: a gun is active only while the owner's weapon upgrade level lies in [Start, End].
  - Inferred: damage, bullet type and sound come from a central table keyed by size × weapon level, scaled by `DamageMult`.
  - The `*2` values are probably the upgraded set (UNVERIFIED).

### 16.6 Beast infection

- Beast weapons use `WEAPON_BEASTINFECT`: 14 guns, all in two CET files, `bCarrier.shp` (8) and `bHeavyCruiser.shp`
  (6) (each gun names it twice, as Start and End level). Each target type declares
  `beastAttackCascadeCost` ("number of points required for attack to cascade to this ship"), `beastAttacksRequired`
  ("number of times you must hit this ship before it converts") and `beastAttackConvertTo` (the Beast type it becomes,
  e.g. `sProcessor → bProcessor`, `sHiveFrigate → bHiveFrigate`) (`CET:assets/RACE_SECT/sProcessor.shp:61-63`,
  `sHiveFrigate.shp`).
- UI: `strMenuSpecialBeastAttackDesc "Infect"`, key `Beast_Attack` "Infection Attack". Techs: `TECH_INFECT` ("PROJECTS
  NANO SELF ONTO TARGET PARTS"), `TECH_BIOWARHEAD` (Beast cruise missile), `TECH_BEAST_VACCINE` (lets Somtaaw salvage
  infected Somtaaw ships). Instance field: `Ship.beastSystemType`.
- Conversion probably reuses the salvage/capture "change owner and type" path, but that is UNVERIFIED.

### 16.7 New specials (UI strings in `english.script:997-1066`; parameters from `.shp`)

| Special | Ships | Evidence |
|---|---|---|
| Link / Unlink (composite vehicles) | 2 Acolytes → ACV, 2 Mimics → MCV | `TECH_LINK`; `linkDistance` (`sMimic.shp`); `CLASS_Composite` |
| EMP | ACV | `TECH_EMP`; `TimeShockedByEMP`, `EMP_Range`, `EMP_Max_Combine_Distance` ("BY TRIGGERING IN TANDEM, A GROUP OF ACVs CAN AFFECT EVEN LARGE CAPITAL SHIPS") |
| Mimic / UnMimic | Mimic, MCV | `TECH_MIMIC`; `Select_Mimic_Type/Item` (disguise as ships or asteroids) |
| Kamikaze ×2 | MCV | "MCVs KAMIKAZE FOR 2X THE DAMAGE"; `TECH_EXPLOSIVE_CHARGE` |
| Leech / Spy / Vent / Unleech | Leech | `leechRUsPerSecond/HPsPerSecond`; keys `Leech_Spy`, `Leech_Leech`, `Leech_Vect` |
| Ram / push | Ramming Frigate | `RamDamage, RamPushTime, RamWaitAfterPushTime`; `TECH_AFTERBURNER` |
| Repulse | Dreadnought | `TECH_REPULSE` |
| Force field (sphere faces, wall) | Sentinels (linked) | `strMenuFFSphere*`, `strMenuFFWall`, `TECH_FORCE_FIELD1-3`, `forceFieldProtectionDistanceScale` |
| Siege Cannon | Command ship module | `sMothershipBigGun`, GUN 0 `WEAPON_BIGGUN` (`BulletRange 900000`, `FireTime 240`, `sMothership.shp`); `TECH_BIG_GUN`; "FRIENDLY-FIRE RISK" |
| Swarm / Cancel Swarm | Hive Frigate | `Launch_Swarmers` |
| Cloak | Beast Cloaked Fighter, Ion Array Frigate | `TECH_CLOAKING` |
| Fire Missiles | Destroyer, Dreadnought, Missile Corvette | HW1 MissileDestroyer volley keys |
| Harvest / Repair / Salvage, crystal haul | Worker, Processor | `repairDistance/repairCone/repairBeamWidth/salvageFlyToDistance/jackRetractTime` (`bWorker.shp:60-74`); `twCrystalProcessingTime 60`; Processor auto-repair `twProcessorAutoRepair*` (`tweak.script`) |
| Cutting beam | UNVERIFIED which ship | `cuttingBeamDamagePerSecond`, `…Upgraded…` |
| Naggarok "eats" ships | `dRKO` | `RKOEatMeTime`, `RKOGetsMissiles` per victim |
| Repair droids / repair bots | most capitals | `repairDroids N`, `TECH_REPAIRBOTS` |
| Slipgates | derelicts | `dSlipgate*`, key `Enter_Slipgate`, `strMenuSV_SlipgatePaths` |
| Pilot view | all | `pilotViewOffsetX/Y/Z`, `strMenuViewChaseModeDesc "Pilot View"` |

### 16.8 Time compression

The key bindings `Accelerate_Time` / `Decelerate_Time` and `Stop_All_Time` (debug), and the string
`strTimesTimeCompression "X Time Compression"` (`english.script:1321-1324, 1576, 1723`). GoK notes that the maximum
factor 8 is "same as Homeworld Cataclysm" (`Universe.c:102`). In HW1, turbo is a debug or replay feature
(`universeTurbo`, `Universe.c:4024-4037`). Cataclysm adds `Universe.realtimeelapsed`, driven by QueryPerformanceCounter
(`CET:game/Game/universe.h:162`), for real-time effects such as fades that must not speed up. CET fixes its uptime
overflow (`CET:PatchUniverse.cpp:294-325`). Inferred: compression runs N univUpdates per task tick, as `universeTurbo`
does. Whether it is SP-only is UNVERIFIED.

### 16.9 Fog of war and sensors

`fowRadius` per ship (12000 typical; 8000 for swarmers and the hangar module; 9000 for the destroyer and the advanced engineering module;
0 for the two support modules). There are sensor
overlays `strMenuSV_ShipFOWRange "Sensors (Sphere)"`, `strMenuSV_AreasVisited "Area Explored"` and `strMenuSV_AreasOccupied`,
plus `strUnexplored` (`english.script`). Advanced sensors detect mimics, cloaked ships and leeches. HW1 has only
cloak visibility (`visibleToWho`) and the global `sensorLevel` (§6.2 SensorArray). Cataclysm FOW is new code.

### 16.10 Waypoints

`strMenuStartWaypointsDesc`, `End/Cancel/RemoveLast Waypoint`, and the modes `Linear`, `End To End` and `Circular`
(`english.script:1004-1008, 1126-1128`), with key bindings `Waypoints`, `Toggle_Waypoint_Modes`, `Waypoint_Mode_*` and
`Cancel_Last_Waypoint`. `CommandToDo` grew by 0x230 bytes (0x100 → 0x330, §2.6), which likely holds the waypoint list
and mode (inferred). A new `COMMAND_*` order or attribute bit for waypoints is UNVERIFIED (CET's `CommandDefs.h` is the
HW1 copy).

### 16.11 Engine-level gameplay changes already identified by CET (`CET:Cataclysm.h`, `CET:PatchUniverse.cpp`)

- The universe stays at 16 Hz. Hyperspace uses `HYPERSPACE_SLICE_RATE` (0x0089410C), overridden to 0.025 by script
  (`PatchUniverse.cpp:20-21`; the engine default is 0.075).
- `RegenerateRURate` is at 0x0088F580. When CET raises the tick rate, its `univUpdateReset` hook overwrites it with
  `16*rate − 1` (`PatchUniverse.cpp:47-50, 287`). At 16 Hz that formula gives 255, the HW1 default. The `.shp` keys appear in `sMothership.shp:89-91`
  with different values: `regenerateRUrate 511` (every 512 ticks = 32 s), `regenerateRUsIfUnder 200`,
  `regenerateRUs 10` (HW1 defaults 255/400/25, `GoK:src/Game/ResCollect.c:64-67`). Whether the engine reads them from
  the `.shp` is UNVERIFIED.
- Mothership and module bullets damaging themselves through bad firing arcs is a Cataclysm bug (`PatchUniverse.cpp:224-238`).

### 16.12 Gameplay-relevant Cataclysm addresses (GOG Emergence EXE, `CET:Cataclysm.h`)

| Symbol | Address | | Symbol | Address |
|---|---|---|---|---|
| univUpdate | 0x0054C3F0 | | universe (global) | 0x00B62A00 |
| univUpdateReset | 0x0054ACD0 | | universe.univUpdateCounter | 0x00B62BAC |
| univCheckRegrowResources | 0x0054C270 | | singlePlayerGameInfo | 0x00C0A5C0 |
| univBulletCollidedWithTarget | 0x00546A20 | | ShipStaticScriptTable | 0x008B2428 |
| GetShipStaticInfo(type) | 0x0053EA50 | | tpGameCreated | 0x00886028 |
| ShipTypeToStr | 0x004D6BC0 | | multiPlayerGame / singlePlayerGame | 0x00A43C94 / 0x00A43C9C |
| rmUpdateResearch / rmCanBuildShip | 0x004F2A20 / 0x004F2630 | | aisTeams[34] | 0x008DFD60 |
| singlePlayerGameUpdate / UpdateMidLevelHyperspacingShips | 0x005197D0 / 0x00519700 | | aiCurrentAIPlayer (ptr) | 0x008DF604 |
| clWrapCreateShip / clWrapBuildShip | 0x00452F30 / 0x00452FA0 | | aiBuildingShip[] | 0x008DFC64 |
| clWrapScuttle / clWrapSetKamikaze | 0x00453390 / 0x00453820 | | HYPERSPACE_SLICE_RATE | 0x0089410C |
| aisFleetUpdate / aisRequestShip | 0x00431750 / 0x00431510 | | RegenerateRURate | 0x0088F580 |
| allianceFormWith | 0x00420880 | | taskFrequency | 0x00BB3260 |
| beastMothershipSelfDamage | 0x005BC900 | | etgTotalTimeElapsed | 0x00C41C74 |
| ranRandomFn | 0x004EF860 | | networkVersion / minorBuildVersion | 0x008B8850 / 0x008B8860 |
| scriptSet / scriptSetTweakableGlobals | 0x00526420 / 0x00527820 | | | |
| SaveGame | 0x004FDAF0 | | | |
| speechEventQueue | 0x00522560 | | | |

More CET-named functions that the gameplay port touches (`CET:Cataclysm.h:125-182`): `scriptSetFramesCB` 0x0053AC70
(reads `buildTime` and other frame counts, so it bakes in the tick rate), `scriptSetReal32CB` 0x005259D0,
`scriptSetUdwordCB` 0x00525C20 (these callback addresses identify the `scriptStructEntry` tables, §17.1),
`selNumShipsInSelection(MaxSelection*, ShipType)` 0x005118D0, `clCommandMessage` 0x004AC820, `memAlloc(size)` 0x006725D0,
`memFree` 0x00673080 (CET declares `memAlloc` with one parameter where HW1 has three, `UnivUpdate.c:1349`; UNVERIFIED
which is right), `mgResourceInjectionInterval` 0x004BDE80, `mgResourceLumpSumInterval` 0x004BE090.

Inferred heuristic for placing unnamed functions: inside one source file, Cataclysm addresses follow HW1 source order
for every CET-named pair checked. `UnivUpdate.c` `univBulletCollidedWithTarget :3111` → `CheckPlayerWin :4142` →
`univCheckRegrowResources :7181` → `univUpdate :7271` maps to 0x00546A20 < 0x0054860C < 0x0054C270 < 0x0054C3F0;
`CommandWrap.c` `clWrapCreateShip :319 < clWrapBuildShip :341 < clWrapScuttle :578 < clWrapSetKamikaze :686` maps to
ascending addresses; so do `ResearchAPI.c` `rmCanBuildShip :387 < rmUpdateResearch :559` and `SinglePlayer.c`
`UpdateMidLevelHyperspacingShips :2345 < singlePlayerGameUpdate :2398`. A function between two known neighbours in HW1
is probably between their Cataclysm addresses. Two more pairs fit (added by the verification pass): HW1's
`univupdateReset` (`GoK:src/Game/UnivUpdate.c:6214`, which sets `universe.phystimeelapsed = UNIVERSE_UPDATE_PERIOD` at
`:6216`) lies between `:3111` and `:7181`, and Cataclysm's `univUpdateReset` 0x0054ACD0 lies between 0x00546A20 and
0x0054C270. In `Universe.c`, `scriptSetFramesCB :1408 < universeUpdateTask :3786 < GetShipStaticInfo :4107` maps to
0x0053AC70 < 0x0053E8A5 < 0x0053EA50. One counter-example: HW1 `clCommandMessage(char[])` is at `CommandWrap.c:33`, but
Cataclysm's two-argument `clCommandMessage` is at 0x004AC820, far from the `clWrap*` cluster at 0x00452F30, so it was
rewritten or moved.

Inferred, UNVERIFIED: **files also look link-ordered alphabetically** (case-insensitive). The CET-named functions
whose HW1 file is known run Alliance 0x00420880 → CommandWrap 0x00452F30 → ETG 0x0046A6F0 → FEFlow 0x00475990 →
Gun 0x0048CBD0 → LOD 0x004A4D70 → MultiplayerGame 0x004BDE80 → NIS 0x004CC634 → ObjTypes 0x004D6BC0 →
Options 0x004D7E30 → Particle 0x004DD43E → Randy 0x004EF860 → ResCollect 0x004F17C9 → ResearchAPI 0x004F2630 →
SaveGame 0x004FDAF0 → SinglePlayer 0x00519700 → SpeechEvent 0x00522560 → StatScript 0x005259D0 → Task 0x0052AF12
(baby task, `Task.c:539`) → Universe 0x0053AC70 → UnivUpdate 0x00546A20. A second ascending run follows, apparently
platform code: main.c 0x0055FC70 → rinit.c 0x0057DCF0 → Trails.c 0x005A4D90 → utility.c 0x005AC174. If this holds, an
unnamed function's HW1 file can be guessed from its address, and the two `univUpdate` pushes at 0x00524841/0x005256B7
(between SpeechEvent and StatScript) would sit in `Stats.c`, whose `HaveShipsFight` calls `univUpdate`
(`HWSDL:src/Game/Stats.c:427-434`).

The skirmish-AI team table is exactly `34 × 0x9D10` bytes, 0x008DFD60..0x00A2D980 (`CET:game/Game/AISkirmish.h:5,
131, 134`). It ends right before `aiHasExternalConstruction` 0x00A2D984.

### 16.13 Code sites that bake in the 16 Hz tick (CET patch list, `CET:Cataclysm.h:40-121, 185-195`)

A port that changes the tick rate, or a decompiler checking that it has found every timing constant, needs this list.
CET patches each site when it raises the update rate.

| Site (CET name) | Address | What is there |
|---|---|---|
| `univUpdate1` … `univUpdate5` | 0x00524841, 0x005256B7, 0x0053E8A5, 0x0053E8DB, 0x0053E9B5 | `push phystimeelapsed` before each `univUpdate` call. The last three lie just below `GetShipStaticInfo` 0x0053EA50, matching HW1, where `universeUpdateTask` (`Universe.c:3786`, calls at `:3973, 3986, 4028, 4037`) precedes `GetShipStaticInfo` (`:4107`). Inferred: they are `universeUpdateTask`; HW1 has 4 calls there, CET lists 3 |
| `utyGameSystemsInit_UNIVERSE_UPDATE_PERIOD` | 0x005AC174 | `taskStart(universeUpdateTask, PERIOD)` |
| `utyGameSystemsInit_TimerResolutionMax` | 0x005ABF68 | `push TimerResolutionMax` in the same init function (`CET:Cataclysm.h:71`, patched at `PatchUniverse.cpp:380-381`) |
| `nisStartup_UNIVERSE_UPDATE_PERIOD`; `nisUpdateTask_UNIVERSE_UPDATE_PERIOD_1/_2`; `nisUpdateTask_UniverseUpdatePeriod1-4` | 0x004CC8F2; 0x004CC8BE, 0x004CC801; 0x004CC634, 0x004CC701, 0x004CC72C, 0x004CC7B8 | NIS task period and NIS time steps. `_1` is `mov [0x00A4CAB4], period` (the HW1 `timeElapsed = UNIVERSE_UPDATE_PERIOD` store, `HWSDL:src/Game/NIS.c:516`); `_2` pushes the period to `nisUpdate`; `1-4` are `fadd/fcomp/fmul/fadd` with a period memory operand (`CET:PatchUniverse.cpp:364-372, 390-395`) |
| `BabyTasksStartup_UNIVERSE_UPDATE_PERIOD` | 0x0052AF12 | baby callback task period |
| `univUpdateReset_UNIVERSE_UPDATE_PERIOD` | 0x0054ACDB | `mov [0x00B62ABC], period`: `universe.phystimeelapsed = UNIVERSE_UPDATE_PERIOD` inside `univUpdateReset` (0x0054ACD0); HW1 `univupdateReset`, `GoK:src/Game/UnivUpdate.c:6216` |
| `CollectResources_HarvestRateCheck`; `collectResources` / `skipCollectResources` | 0x004F17C9; 0x004F17D5 / 0x004F1A46 | the `WOODPECKER(harvestRate, 0)` test (`ResCollect.c:596`) and its two branches |
| `startUnivCheckRegrowResources` / `end…` | 0x0054CD73 / 0x0054CD8F | step 23 WOODPECKER block in `univUpdate` |
| `startRmUpdateResearch` / `end…` | 0x0054C83D / 0x0054C859 | step 9 WOODPECKER block in `univUpdate` |
| `gunShipFirePower_UniverseUpdatePeriod1/2` | 0x0048CBD0, 0x0048CC57 | per-tick firepower estimate (HW1 `gunShipFirePower`, `Gun.c`); both are `mov [esp+0x1C], period` into a local (`CET:PatchUniverse.cpp:419-424`) |
| `etgDefineAt_UpdateRate1-3` | 0x0046E042, 0x0046E04F, 0x0046E05B | ETG effect timing: `fcom/fsub/fld` of a constant equal to `ETG_UpdateRoundOff / 2` (0.015625). CET redirects the operands because the `.data` constant is shared with other code (`CET:PatchUniverse.cpp:352-362`) |
| `allianceFormWith_UniverseUpdatePeriod` | 0x0042090D | alliance timing inside `allianceFormWith` |
| `CheckPlayerWin_GameTypeCheck` / `_IsMultiplayerGame` | 0x0054860C / 0x00548619 | victory check (§15) |
| `aiUpdateExternalShipBuild_CalculateTimerDuration` / `_SetTimerDuration` | 0x00543F48 / 0x00543F51 | `buildTime / rate` for externally built ships (`EAX = buildTime` in frames, divided back to seconds, `CET:PatchUniverse.cpp:179-189`) |
| per-tick functions CET throttles to 16 Hz when it raises the rate | `mistrailUpdate` 0x005A4D90, `beastMothershipSelfDamage` 0x005BC900, `univBulletCollidedWithTarget` for `BULLET_Beam` | hooks run the original only when `univUpdateCounter % factor == 0` (`CET:PatchUniverse.cpp:98-105, 134-141, 243-250`). Inferred from the hooks: in the original all three run once per tick, so beam damage is applied per tick |
| `trailSegmentsRead` (`.shp` trail segment count) | 0x005A7A70 | segment count is per tick; CET shifts it left by the rate shift (`CET:PatchUniverse.cpp:107-117`) |
| globals `ETG_UpdateRoundOff`, `etgEffectorCI_UniverseUpdateRate` | 0x0072A818, 0x0072A82C | 0.03125 and the ETG rate constant in `.data` |
| globals `HYPERSPACE_SLICE_RATE`, `RegenerateRURate`, `taskFrequency`, `TimerResolutionMax`, `utyTimerDivisor`, `realtimeremainder` | 0x0089410C, 0x0088F580, 0x00BB3260, 0x0072A614, 0x00AB7520, 0x00A6D9A0 | timing state. Inferred, UNVERIFIED: the last two (`LARGE_INTEGER`/`LONGLONG`) feed `Universe.realtimeelapsed` (§16.8); CET declares them but never reads them |

---

## 17. Strings and constants to search for in the Cataclysm binary

Grouped by what a hit tells you. HW1 provenance is given so a hit can be mapped straight to the HW1 function or table.

### 17.1 Engine script keys (each hit lands in a `scriptStructEntry` table → its callback and field offset)

`scriptStructEntry` is 16 bytes: `{char const *name; setVarCback setVarCB; size_t offset1; size_t offset2}`
(`GoK:src/Game/StatScript.h:21-27`). In HW1 `offset1`/`offset2` are `&ShipStaticInfoR1[0].field` and `&ShipStaticInfoR1[0]`,
so `offset1 − offset2` is the field offset. Walking the table at 0x008B2428 therefore gives the Cataclysm SSI layout
directly (inferred: assumes Cataclysm kept the encoding). The HW1 release table has 155 entries (156 in debug builds,
which add `scaleFactor` under `SO_CLOOGE_SCALE`, `GoK:src/Game/Universe.c:613-615`, `SpaceObj.h:27-39`). `N-LIPS` is
entry 106 there and `trailScaleCap…` entries are 113/120/127/134. CET's pointers to their callback slots
(`shipNLipsRead` 0x008B2B9C, `trailScaleCapRead…` 0x008B2C0C/…2C7C/…2CEC/…2D5C, `CET:Cataclysm.h:261-265`) are entries
119/126/133/140/147 (`(addr − 0x008B2428 − 4) / 16`). So Cataclysm inserted exactly 13 entries before `N-LIPS`.

| # | String | Expected table / meaning | Grounding |
|---|---|---|---|
| 1 | `"thruststrength[TRANS_UP]"` | ShipStaticScriptTable (0x008B2428) | `GoK:src/Game/Universe.c:596`; every Cataclysm `.shp` |
| 2 | `"rotstrength[ROT_YAWLEFT]"` | same | `Universe.c:602` |
| 3 | `"canReceiveShipsForRetire"` | same | `Universe.c:537` |
| 4 | `"passiveRetaliateZone"` | same | `Universe.c:529` |
| 5 | `"collSideModifiers[TRANS_UP]"` | same | `Universe.c:590` |
| 6 | `"N-LIPS"` | same (scaleCap / nlips) | `Universe.c:611`; CET `shipNLipsRead` 0x008B2B9C |
| 7 | `"NUMBER_OF_SALVAGE_POINTS"` | salvage sub-parser | `Universe.c:663` |
| 8 | `"Overlay_Selection_Scalar"` | same | `Universe.c:664`; `sMothership.shp` |
| 9 | `"isCapitalShip"` | same | `Universe.c:581` |
| 10 | `"externalBuild"` | **new**, SSI+0x1F0 | `CET:spaceobj.h:794`; all `.shp` |
| 11 | `"forceFieldProtectionDistanceScale"` | **new**, SSI+0x1F4 | `.shp` |
| 12 | `"upgradeTimeModifier"` | **new**, SSI+0x1F8 | `.shp` |
| 13 | `"fowRadius"` | **new**, SSI+0x240 | `.shp` |
| 14 | `"supportUnits"` / `"providesSU"` | **new** | `.shp` |
| 15 | `"TimeShockedByEMP"` | **new** | `.shp` |
| 16 | `"UpgradeByDocking"` | **new** | `.shp` |
| 17 | `"beastAttackCascadeCost"` | **new** | `RACE_*/*.shp` |
| 18 | `"beastAttacksRequired"` / `"beastAttackConvertTo"` | **new** | same |
| 19 | `"RKOEatMeTime"` / `"RKOGetsMissiles"` | **new** | `sCarrier.shp:82-83` |
| 20 | `"leechRUsPerSecond"` / `"leechHPsPerSecond"` | **new** | `sDreadnought.shp` |
| 21 | `"linkDistance"` | **new** | `sMimic.shp` |
| 22 | `"pilotViewOffsetX"` | **new** | `.shp` |
| 23 | `"hyperspaceBoxScale"` | **new** | `sMothership.shp` |
| 24 | `"bUseRayTriangleIntersectionForHitLocation"` | **new** | `.shp` |
| 25 | `"repairDroids"` | **new** (nRepairDroids) | `.shp` |
| 26 | `"RamDamage"` / `"RamPushTime"` / `"cuttingBeamDamagePerSecond"` / `"EMP_Range"` | **new** (field names from CET; `.shp` spelling UNVERIFIED) | `CET:spaceobj.h:715-728` |
| 27 | `"GunSize"`, `"StartUpgradeLevel"`, `"EndUpgradeLevel"`, `"DamageMult"`, `"FireTime2"`, `"BulletSpeed2"` | new GUN-block keys (Cataclysm `scriptSetGunStatics`) | `.shp` GUN blocks |
| 28 | `"fieldRadius"`, `"percentageThatGetThrough"` | DFGFrigate statics (HW1 module reused) | `GoK:src/Ships/DFGFrigate.c:32-35`; `bDFGFrigate.shp` |
| 29 | `"MissileRegenerateTime"`, `"MissileLagVolleyTime"` | MissileDestroyer statics | `GoK:src/Ships/MissileDestroyer.c:47-49` |
| 30 | `"repositionTime"`, `"circleRange"`, `"fullMovementFreedom"` | Attack.c side-step | `GoK:src/Game/Attack.c:31-33` |
| 31 | `"MultiBeamRangeRatio"` | Cataclysm MultiBeam statics (HW1 had `"MultiBeamRange"`) | `sMultiBeamFrigate.shp:60`; `P2MultiBeamFrigate.c:50` |
| 32 | `"regenerateRUrate"`, `"regenerateRUsIfUnder"` | ResCollect tweak table | `GoK:src/Game/ResCollect.c:116` |

### 17.2 Enum string tables (`NumStrXlate`, `GoK:src/Game/ObjTypes.c:61-405`; a hit gives the full enum order)

| # | String | Table |
|---|---|---|
| 33 | `"sRecon"`, `"sMothershipBigGun"`, `"bCruiseMissile"`, `"dRKO"`, `"dCrystal0"` | shiptypeinfo → `ShipTypeToStr` (0x004D6BC0) |
| 34 | `"RACE_SECT"` / `"RACE_BEAST"` (also as path prefix) | shipraceinfo |
| 35 | `"CLASS_MonsterShip"`, `"CLASS_Component"`, `"CLASS_Micro"`, `"CLASS_Composite"` | shipclassinfo |
| 36 | `"GUN_HiddenGimble"`, `"GUN_VisibleGimble"` | guntypeinfo |
| 37 | `"GUNSIZE_LARGE"` | new gun-size table |
| 38 | `"WEAPON_BEASTINFECT"`, `"WEAPON_BIGGUN"`, `"WEAPON_PROJECTILE"` | new weapon-level table |
| 39 | `"ET_DestroyerExplosion"` | explosiontypeinfo |
| 40 | `"DockingCone"`, `"LatchPoint"`, `"LaunchPoint"` | dockpointtypeinfo (unchanged; anchors the file) |
| 41 | `"Evasive"`, `"Neutral"`, `"Aggressive"` | tacticsinfo |

### 17.3 String-table identifiers (`LanguageStrings[]` via `strEntry`, `GoK:src/Game/StringSupport.c:27`; order = enum order)

| # | String | Note |
|---|---|---|
| 42 | `"strsMothershipBigGun"` | ship nice name |
| 43 | `"strTECH_INFECT"`, `"strTECH_INERTIALESS_DRIVE"` | tech nice names → tech enum order |
| 44 | `"strR1TECH_CLOAKINGinfo"` | per-race tech info (R1 = Somtaaw) |
| 45 | `"strHANGAR_HEADING"`, `"strGENETICS_HEADING"` | module and Beast research headings |
| 46 | `"strMenuSpecialBeastAttackDesc"`, `"strMenuSpecialEMPDesc"`, `"strMenuSpecialRamDesc"` | special-action menu |
| 47 | `"strMenuStartWaypointsDesc"`, `"strMenuWaypointTypeCircular"` | waypoints |
| 48 | `"strTimesTimeCompression"`, `"str_UI_AccelerateTime_KeyBindingName"` | time compression |
| 49 | `"strMenuSV_ShipFOWRange"`, `"strMenuSV_AreasVisited"` | FOW sensor overlay |
| 50 | `"*NO_STRING*"` | missing-string fallback (`StringSupport.c:25`) |

### 17.4 Mission, KAS and script files

| # | String | Meaning / grounding |
|---|---|---|
| 51 | literals `"_VIFONCE001"`, `"_VIFONCE002"`… (the `%03d` format is in kas2c only, not in the EXE) | kas2c IFONCE vars → mission code (`GoK:tools/kas2c/parser.y:181`) |
| 52 | `"mission01"` … `"mission17"` | `kasMissionStart` names (HW1 `SinglePlayer.c:2913-2929`; Cataclysm count from `CET:README.md`; exact spelling UNVERIFIED) |
| 53 | `"PlayNis"`, `"PlayNislet"` | AIVars polled by `singlePlayerGameUpdate` (`SinglePlayer.c:2401-2402`) |
| 54 | `"SinglePlayer\\Mission%02d\\"`, `"Mission%02d.mission"` | mission layout path (`SinglePlayer.c:416-420`) |
| 55 | `"SP_Game_Over"` | end screen (`UnivUpdate.c:7361`) |
| 56 | `"ResearchDepend.script"`, `"SPR1ShipDepend"`, `"SPR1TechDepend"` | research tree file and keys (`ResearchAPI.c:58-61, 998`) |
| 57 | `"Tweak.script"`, `"UnitLimitCaps.script"`, `"Tactics.script"`, `"Formation.script"`, `"AIPlayer.script"`, `"AISkirmish.script"` | tweak tables (HW1 list from `scriptSet` calls; `aiskirmish.script` from CET) |
| 58 | `"Ships"`, `"AIPoint"`, `"AIPath"`, `"MissionSphere"` | `.mission` layout keys (`LevelLoad.c:111-141`) |
| 59 | `"AIS_MIMIC_TARGET_COUNT[AI_ADV]"`, `"AIS_HIGH_RESOURCE_THRESHOLD"` | Cataclysm skirmish AI tweak keys (`CET:assets/aiskirmish.script`) |
| 60 | `"twCrystalProcessingTime"`, `"twProcessorAutoRepairDistance"`, `"twnLipsBaseOffset"` | Cataclysm tweak keys (`CET:assets/tweak.script:864-871`) |

### 17.5 Numeric constants

| # | Constant | Where |
|---|---|---|
| 61 | `0.0625f` (`0x3D800000`), `16.0f` (`0x41800000`), `0.03125f` (`0x3D000000`) | UNIVERSE_UPDATE_PERIOD, RATE, ETG round-off (CET patches these sites) |
| 62 | `69069` (`0x00010DCD`) and `1013904243` (`0x3C6EF373`) | `ranRandomFn` (`Randy.c:107`) |
| 63 | `0.125f` (`0x3E000000`) | CAPTAINSERVER_PERIOD (`CommandNetwork.h:126`) |
| 64 | `0x5555`, `0xcccc`, `0xe555`, `0xca00`, `0xb1ab` | packet types (`CommandNetwork.h:166-193`) |
| 65 | `666, 667, 668, 777, 888, 999, 1000-1004` | `howDidIDie` codes (`SpaceObj.h:1156-1167`) |
| 66 | `{1.0, 1.6, 2.25, 2.8, 3.25, 3.6}` | `labdec[]` research speed-up table (`ResearchAPI.c:47-56`); its presence or absence tells whether HW1 research survives |

---

## 18. Open questions (to settle against the binary)

1. Where the HW1 tech bitmask went (41 techs), and the Cataclysm `TechnologyType` order.
2. The Cataclysm `CommandToDo` layout (+0x230 bytes): waypoints? new order codes?
3. The module attachment mechanism (slave/master vs a new "component" system); how `CLASS_Component` ships are targeted and killed.
4. The Beast conversion code path, and how `beastAttackCascadeCost` "points" accumulate.
5. The gun damage source after `DamageLo/Hi` were removed: the table keyed by `GunSize` × weapon level.
6. Whether time compression is SP-only and how it interacts with the task cap (`TSK_MaxTicks = 8`).
7. The KAS savegame function-pointer encoding in the original EXE, and whether Cataclysm added new `kasf*` functions
   (CET's `KASFunc.h` is a near-copy of HW1, with three extras: `kasfShipsSelectStrikeCraft`, `kasfTeamGiveToPlayer`,
   `kasfTeamMemberHandle`).
8. Whether `RACE_Derelict` `d*` types are `Ship` objects.

---

## Verification

A skeptical pass on 2026-10-08 checked this document against GoK `3af31fb`, HWSDL `1bed493` and CET `19a08c3`.

**Claims checked (about 430, most of them by script), by group:**

| Group | How it was checked | Result |
|---|---|---|
| 161 `file:line` function anchors (§2-§15: `univUpdate :7271`, `clProcess :8224`, every `process*ToDo`, `cl*`, `dock*`, `kas*`, `aif*/ait*`, `rm*`, `CheckPlayerWin :4142` and the rest) | a script printed each cited line and grepped it for the function name | 160 correct; `CustShipStaticInit` cited at `Universe.c:2312` (a blank line) |
| All 59 `shipHeaders[]` entries and 47 `CustShipHeader` definitions (§6.2) | `GoK:src/Game/Universe.c:218-279`; grep of `^CustShipHeader` in `src/Ships` | all correct |
| ~80 CET addresses (§2, §13, §16.12, §16.13, §17.1) | `CET:Cataclysm.h` read in full | all correct; `0x005ABF68` and the code operands CET writes (§16.13) were missing |
| HW1 struct offsets in §2.6 (35 fields) | independent `gcc -m32` probe against HWSDL headers (§2.6 recipe) | all 35 reproduced exactly; 25 more added |
| Cataclysm offsets in §2.6 / §16.2 | `CET:game/Game/spaceobj.h:760-916, 1808-1876`, `universe.h:138-148, 234-257` | all correct |
| `ShipStaticScriptTable` count 155/156, `N-LIPS` = entry 106, `trailScaleCap*` 113/120/127/134, CET slots 119…147, "+13 entries" | counted `Universe.c:500-665`; arithmetic on `CET:Cataclysm.h:261-265` | correct |
| Tick facts: `UNIVERSE_UPDATE_RATE_SHIFT = 2`, `/vanilla`, `TSK_MaxTicks 8` (and `TASK_MAX_TICKS 1` in both builds), the turbo factor of 8, 1 sync packet = 2 ticks, `CAPTAINSERVER_PERIOD` | source read | correct; the packet-playback turbo (6, not 8) was added |
| RNG (`Randy.c:57-107`, 25 streams, `RANDOM_GAME` 10, `RANDOM_AI_PLAYER` 6), `netcheckShow` RNG side effect (`NetCheck.c:918-923`, `CommandNetwork.c:1520`) | source read | correct |
| Physics (`vecCapVectorSloppy` per-axis clip, `getVectDistSloppy` L∞ norm, `Physics.c:264/401`) and damage pipeline (`collSideModifiers`, HeavyDefender armour piercing, friendly fire, slave → master) | source read | correct; the `-0.0f` bit-test nuance was added |
| KAS: 302 `kasf*` built-ins, `_VIFONCE%03d` (`parser.y:181`), `LSTRING_COUNT_REQUIRED 5`, `flex -i`, 17 `.kas` + 2 `.kash` = 42,581 lines, CET `KASFunc.h` extras | counted the table and the files; diffed function names | correct |
| CET assets: 145 types and every roster ID, `shipclass`/`supportUnits`/`providesSU`/`fowRadius`/`externalBuild`/`UpgradeByDocking` per `.shp`, 41 techs at `english.script:214-257`, all tweak values in §3.5 and §4.3, waypoint/time-compression/special strings | grep over `CET:assets` | correct except the items below |

**Wrong or unverifiable claims, and the fixes made:**

1. `CustShipStaticInit` called at `Universe.c:2312`: the call is at `:2313-2315`. Fixed in the §6.1 table.
2. `strR1TECH_*/strR2TECH_*` text at `english.script:299-383`: the range is `:299-388`. Fixed (§16.1).
3. "`buildTime` is read in frames": the `.shp` value is in integer seconds and is stored ×`UNIVERSE_UPDATE_RATE` as frames (`Universe.c:1408-1412`). Fixed (§11).
4. Mission fingerprint "per-mission function-pointer arrays used by save/load" and `MissionNN.functions.c` "used for savegames": both are SDL-port additions (`GoK:tools/kas2c/README.txt:4-6`), so the original EXE is not expected to have them. Fixed (§14.3, §14.6).
5. "Cataclysm's 17 missions": CET only names "Mission 17" (`README.md:40`), which proves at least 17. Relabelled UNVERIFIED (§14.6).
6. `realtimeremainder`/`utyTimerDivisor` "feed `Universe.realtimeelapsed`": CET declares them but never reads them. Relabelled inferred/UNVERIFIED (§16.13).
7. "support modules +10/+12/+15 (`.shp` values)": +15 comes only from Beast info text (there is no `bMothershipSupport.shp`), and `sCarrierSupport` is 12 in `.shp` but "SU BONUS: 10" in its text. Fixed with sources (§16.3).

Clarified (not wrong): the 14 `WEAPON_BEASTINFECT` guns are all in `bCarrier.shp` and `bHeavyCruiser.shp`. The SP-hyperspace `return` skips all of `aiplayerUpdateAll`, not just KAS. "+0x4C across `ShipStaticInfo`" holds only for one run (§1 item 7).

**Missing content that was added:** KAS/AI cadence (`AIPLAYER_UPDATE_RATE {63,31,15}`, so SP KAS runs at 1 Hz). Cataclysm KAS debug key bindings that match `KAS.c:52-56` (evidence that the KAS runtime survives). The `Ship.dockvars` relocation and 25 more HW1 → Cataclysm offset pairs, including `SSI.harvestRate` cross-checked by CET's `[ecx+0x25C]`. Cataclysm `TOTAL_STD_SHIPS` = 60. The bounds of the `aisTeams` table. The baby task's hard-coded `1/16` (`Task.c:539`, `Task.h:90`). The `sMothership.shp` RU-regeneration values (511/200/10). Case-insensitive `GUNSIZE_Medium`. The exact operands CET rewrites at each 16 Hz site, plus `0x005ABF68`. Two more same-file ordering confirmations, one counter-example (`clCommandMessage`), and an inferred cross-file alphabetical link-order map (§16.12).

**Remaining issues:**
- Everything Cataclysm-side still rests on CET's reconstruction until the real EXE is in `original/`. That includes the `dockvars` move, which comes from CET's declaration order, not from a CET assert on HW1-equivalent neighbours.
- The cross-file alphabetical link order (§16.12) is an inference from about 25 data points. Test it first in Ghidra.
- The HW1 offsets are i386 SysV (gcc) layout. MSVC 6 layout for these structs is expected to match but was not compiled (UNVERIFIED).
- `Tweak.c:399-404` in §3.5 covers only some of the listed defaults; the rest are spread through `Tweak.c` (not re-cited).
- Not re-checked line by line: §2.4 list membership, §7.3 sub-ranges `8277-8632`/`8633-9006`, and §9-§10 prose details. Their function anchors passed the bulk check.
