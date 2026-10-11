<!-- done: salvage, gun, beast, collision, dock, final -->
# M2-R4 producer 3: combat, docking, salvage

Status: **done** (`salvage`, `gun`, `beast`, `collision`, `dock`, `final`), with one large piece of `dock` left (the
Cataclysm dock purposes, see that section). Branch `m2`, worktree
`.claude/worktrees/m2`, nothing committed by this run. Scope: docs/m2-r3-summary.md "Plan for M2-R4" row 3 and "What
is left of stage 2 part A" item 4 (salvage, Beast weapon, gun weapon-type word).

This run resumed an earlier run of the same producer that a usage limit cut off before it wrote this report. Its
`src/` edits were committed unaudited as the WIP snapshot `b39dea7` (39 `src/` files). The resume read
`git diff d47a23f..b39dea7` file by file against the r6 decompile (`ghidra/exports/cataclysm_gog_r6/decompiled.c`), the
exe's disassembly (`tools/m2_fdis.py`) and the exe's data (read from `original/game/cataclysm.exe` by virtual address),
kept what matched, fixed what did not, and finished what was missing. Every finding below names the file and the
command or exe body it rests on.

Subsystems (the done marker above lists the finished ones):
1. `salvage`: the salvage runtime (`Salvage.c`), the deletion of HW1's salvage code (`SalCapCorvette.c`) and of every
   HW1 salvage reader, the clamp lists (`Clamp.c`) the salvagers tow with, `cat_univConvertShip`.
2. `gun`: the gun weapon-type word (exe Gun +0x00), damage by weapon against armour, `gunShoot` /
   `cat_gunCreateBullet`, the other `Gun.c` / `Damage.c` / `Attack.c` deltas.
3. `beast`: the Beast infection weapon runtime (`BeastWeapon.c`).
4. `collision`: `Collision.c`, `Physics.c`, kamikaze steering/hit, `cat_blastCreate` / `cat_blastReset`.
5. `dock`: `Dock.c`, `LaunchMgr.c`, `ResCollect.c`.
6. `final`: clean builds, plain-harness runs, delta-map review entries.

## Subsystem `salvage`

### What the WIP snapshot did, and the verdict per file

| file | change in `b39dea7` | verdict |
|---|---|---|
| `src/Game/Salvage.c` / `.h` | The runtime of 0x004f7a90..0x004fd600: queries, target choice, the 15-state `cat_salvageSpecialTarget` 0x004faad0, detach, housekeep, the depot slots and their processing (generic conversion, Beast infection, big gun, dockables), save fix-ups, the attached-salvager iterator. `CatSalvagerInfo` 0x28 and `CatSalvageDepot` 0x144 laid out field by field (ILP32 asserts). | Kept, one bug fixed (below). Compared with the exe: `cat_salvageGetSalvagerInfo`, `GetDepotInfo`, `Init`, `StaticInit`, `DepotInit`, `BeamEffect`, `DepotCanAccept`, `TargetDamagedEnough`, `CheckTarget` (all 9 reason counters and their order), `TargetAllowed`, `ChooseTarget`, `FindNearestDepot`, `AlignToVelocity`, `GetTargetAttachPoint`, `FlyToTarget`, `ApproachTarget`, `SpecialTarget` (states 0-14), `HouseKeep`, `RemoveShipReferences`, `NextAttachedSalvager`, `ConvertType`, `DeliverToDepot`, `DepotHouseKeep`, `DepotDied`. |
| `src/Ships/SalCapCorvette.c` / `.h` | HW1's salvage code (3,206 lines) deleted; the file is the exe's eight slot wrappers 0x005c59c0..0x005c5ae0 around `Salvage.c`; `sizeofShipSpecifics = sizeof(CatSalCapCorvetteSpec)`, which is 0x30 on ILP32 (asserted; `tools/m2_layout32.sh` checks it with every reused header). | Kept. |
| `src/Game/SpaceObj.h` | `ShipOrDerelict.salvageInfo` removed; the six retired HW1 salvage `SPECIAL_*` flags and `SPECIAL_2_BusiedDockPoint` / `DisabledForever` deleted; `CatClampedList`; `DockInfo.catNumBusyOfType` (exe DockInfo +0x00); `SpecificDockVars.catDockPointIndex` (exe Ship+0x4A0); the salvage-target fields of `Ship` (exe +0x238, +0x3EC, +0x504..+0x514, +0x5A4, +0x608, +0x615..+0x61C) and the Beast-infection fields (+0x4D0..+0x500). | Kept. |
| `src/Game/Clamp.c` / `.h` | `clampObjToObj` 0x004346e0 (reuse, per-host list of up to 1000), `unClampObj` 0x00434830, `updateClampedObject` 0x00434940 (called for the host; recursive), new `cat_clampUnclampAllFromShip` 0x00434900, `cat_updateClampedObjectNoPhysics` 0x00434ab0. | Kept: every body matches the exe, including the host's `SOF_Clamped` clear and the 0xFA8 list. |
| `src/Game/UnivUpdate.c` / `.h` | The exe's clamp scheme in `univUpdateAllPosVelShips` 0x00544f40 (the hosts go on the list; `cat_updateClampedObjectNoPhysics` for ships that apply no physics); `cat_clampUnclampAllFromShip` in `univDeleteDeadShip` / `univRemoveShipReferences`; `univCreateShip`'s salvage-field inits (0x005423c2..0x005423f6, 0x00542899) and the per-type busy counts; HW1's `setSalvageInfo` and `univRemoveAllDerelictReferencesFromSpecialShips` deleted; new `cat_univConvertShip` 0x00543950 and `cat_univBeastInfectionUpdate` 0x00543860 (called from `cat_univShipDeferredUpdate`). | Kept: outside this port's file list, but each change is the clamp/salvage/infection runtime this port owns and replaces a CAT-TODO that named M2-R4 #3; the clamp-scheme condition matches 0x00544fa6 term by term. |
| `src/Game/Dock.c` / `.h` | `GetDirectionVectorOfShip` 0x004631f0 takes a frame (direction 3 leaves the result); `dockPutShipInside` 0x004652c0 (no double insert, micro = fighter, resource = corvette, no HW1 SalCap hand-over); new `cat_dockDepositResourceUnits`, `cat_salvageOpenAnimIndex` / `CloseAnimIndex`, `cat_matCoordSysAligned`, `cat_linkComputeTargetCoordSys`, `cat_dockFlyToConeOriginLatch`, `cat_dockFlyToConeInside`, `cat_dockFlyToConeOrigin`, `cat_linkFlyIntoPosition`; HW1 salvage readers removed. | Kept (the `dock` subsystem builds on it). [audit: `dockRemoveShipFromInside` 0x004632e0, the matching decrement, was left at HW1's fighter/corvette test, so a docked micro or resource ship that left would keep its count; fixed] |
| `src/Game/Collision.c` | Salvager readers on `SPECIAL_CatSalvager` and the Cataclysm salvager block (noAvoid, noDamageTarget, dockwith); the clamp test both ship/ship functions inline, as `cat_collClampExempt`; no derelict salvage. | Kept (the rest of `Collision.c` is the `collision` subsystem). |
| `src/Game/BeastWeapon.c` / `.h` | `cat_beastInfectShip` 0x004298f0 (compared: flags 0xA00000, spin by the three inertia terms, the stun/convert timers, the bindings of the convert type, colour, speech 0x10008a1, the AI and disable calls; Mission 7's KAS part is a stage-6 CAT-TODO). | Kept (the rest is the `beast` subsystem). |
| `src/Game/ShipSelect.c` / `.h` | `cat_shipGetSpeedMultiplier` 0x00514940 (experience level CAT-PROVISIONAL #5, Beast-mothership and ramming bonuses CAT-TODO #4), `cat_salvageTargetAllowed` 0x00513920 (reason indexes 0/3/6/7/8 checked against the exe's stack offsets), `cat_salvageDepotHoldsCrystal`. | Kept. |
| `src/Game/Tweak.c` / `.h` | The five salvage tweakables 0x008ac5dc..0x008ac608 and `twSalvageDamageLimit[]` (the exe's entry for `CLASS_MonsterShip` points at the corvette slot). | Kept. |
| `AITrack.c`, `ConsMgr.c`, `Universe.c`, `Vector.c` (+ headers) | Helpers the runtime calls: `cat_MoveReachedDestinationRadius` 0x00419e80, `cat_cmHasSupportUnits` 0x00459b40 (CAT-PROVISIONAL #5: TRUE until the Player layout), `cat_playerHasCompletedShip` 0x0053f620, `cat_vecDistance` 0x00550d60; `universeReset` calls `cat_salvageResetGlobals` 0x004f7a90. | Kept. |
| `AIMoves.c`, `AIShip.c`, `CommandLayer.c`, `Formation.c`, `KASFunc.c`, `NetCheck.c`, `SaveGame.c`, `SoundEvent.c`, `Tactics.c`, `CloakGenerator.c`, `P1Mothership.h`, `SDL/mainrgn.c`, `SDL/render.c` | The HW1 salvage readers: each either reads the Cataclysm salvager block / `SPECIAL_CatSalvager` as the exe does at that site, or is deleted where the exe has no counterpart (`salCapExtraSpecialOrderCleanUp` x6, `kasfTargetDrop`, the HW1 pushing velocity, `Save/Load/FixSalvageInfo`, the salvage-point debug lines). `RemoveShipFromSpecial` calls the header's slot 9 (`RemoveShipsFromDoingStuff` 0x00444810 calls `[statinfo+0x12c]` for order 7). | Kept: these edits are outside the file list but are exactly "the deletion of HW1 salvage code and its readers" the plan gives this port. |

### Fixed in this run
- **`cat_salvageDepotPointType` and `cat_salvageApproachTarget` used the wrong dock point types for `dBigGunUnit`.**
  The exe computes `((depot == sMothership) - 1 & 0xB) + 0x12` (`cat_salvageDepotCanAccept` 0x004f7c20 and
  `cat_salvageApproachTarget` 0x004fa410): type 0x12 `DPT_Destroyer_EB` at an sMothership, else 0x1D
  `DPT_SalvageProcess`. The WIP had `DPT_Swarmer_In` / `DPT_Mothership_Out` (21 / 34), a miscount of the 35-entry enum
  (values printed with the tree's headers). The byte table 0x004f7d58 the exe indexes by target type reads
  `0, 3 x10, 1, 1` (types 0x6F..0x7B): `dBigGunUnit` uses that computation, `dMessagePod` / `dBuoy` `DPT_Med_In`.
- **NULL depot in tow state 10.** When no depot takes the target any more, `cat_salvageFindNearestDepot` returns NULL
  and the exe reads NULL+0x58 (an access violation). The port treats no depot as no change (comment in the code).
- The Beast type range in `Salvage.c` is now written with its own ends (`bRecon`..`bMissileCorvette`, 0x21..0x3B)
  instead of the neighbouring ids `sMothershipOreCanArmour` / `kAdvanceSupportFrigate`; same values.

### Finished in this run: no HW1 salvage reader left
The WIP left the HW1 salvage statics behind. Neither `NUMBER_OF_SALVAGE_POINTS` nor `SalvagePoint` is in
cataclysm.exe's strings, and the exe's mex readers (0x004b7130..0x004b7680) have no salvage chunk reader. Deleted:
- `SpaceObj.h`: `SalvageStaticPoint`, `SalvageStaticInfo`, `SalvagePoint`, `SalvageInfo`, their size macros,
  `SALVAGE_MAX_CAPTURABLE` and the `CAT_SALVAGESTATICINFO` stand-in (always NULL);
- `StatScript.c` / `.h`: the `#if 0` HW1 loaders `scriptSetSalvageStatCB`, `scriptSetSalvagePointCB`,
  `scriptSetSalvageStatics` and `StaticSalvageInfoScriptTable`;
- `MEX.c` / `.h`: `mexGetSalChunk`, `mexGetSalvageStaticInfo`, `MEXSalvageChunk`;
- `ObjTypes.c` / `.h`: `SalvagePointType` and its two converters;
- the `SalCapCorvette.h` includes of `Formation.c`, `SaveGame.c`, `render.c` (they used only HW1 declarations).
`SAVE_SALVAGEINFO` stays in `SaveGame.h` as a CAT-DORMANT chunk id (nothing writes or reads it).

`grep -rn "SalvageInfo|salvageInfo|SalCapCorvetteSpec\b|SPECIAL_IsASalvager|SalvageStaticInfo|SALVAGESTATICINFO|SalvagePoint|salCap|SalCapDropTarget|SPECIAL_Harvested|TakingHomeTechnology|SalvagerHasSomething|BusiedDockPoint|DisabledForever"`
over `src/Game src/Ships src/SDL` now finds only comments. [audit: not so: it finds 25 code lines. Most are Cataclysm names (`catSalvagePointIndex`,
`CatSalCapCorvetteSpec`, `SAVE_SALVAGEINFO`) and `salCapCorvetteInSelection` (mainrgn.c), which already tests
`SPECIAL_CatSalvager`. But `salCapCorvettesInSelectionCantDoFormation` (CommandLayer.c, called by `clProcess`) still
tested HW1's dormant `SalCapCorvette` type, where the exe's `clProcess` 0x0044c448 tests `cat_shipIsSalvager` and
specialFlags 0x4 inline: an HW1 salvage reader that was left. Fixed (the test is `cat_shipIsSalvager` now).] What remains named after HW1 salvage is Cataclysm's:
`ShipOrDerelict.salvageNumTagged[]` is the exe's per-player claim count that `cat_salvageChooseTarget` /
`cat_salvageDetach` / state 0 read and write (the sync checksum in `UnivUpdate.c` still sums it, as HW1).

### Checks
- `SalCapCorvetteHeader.sizeofShipSpecifics = sizeof(CatSalCapCorvetteSpec)`: 0x30 on ILP32
  (`_Static_assert` in `SalCapCorvette.h`; `tools/m2_layout32.sh`: "ILP32 ship specifics: 24 files, every
  sizeofShipSpecifics assert holds", "m2_layout32: OK").
- `tools/build.sh --debug`: rc 0, no `warning:` line besides meson's sanitizer notice.
- Plain harness, `m2r4-combat-salvage-sp` (sp, 360 s, binary copied to `original/scratch-m2/m2r4-combat/`): milestone
  **universe running**, no stop (still running at the timeout), univUpdateCounter **1,584** (R3 and r4c-final-sp:
  1,248) [audit: 360 s against 340 s timeouts; at 340 s this port also reaches 1,248, see `final`], **ASan 0**, no unsuppressed leak; UBSan kinds as before (`fescreen`, `etgdecision`, `PATCH`,
  `etgfunctioncall`, `mhlocalbinding`, `etgconditional`, `etgbranch`, `fquant.c`).

### What is left of salvage (owners)
- `cat_salvageDepotProcessCrystal` 0x004f98b0 and the crystal dissolve stop in `cat_salvageDepotDied`: need the Crystal
  header and the dissolve effect (**M2-R4 #4**). A delivered crystal waits in its slot until then.
- sWorker/bWorker and sProcessor/bProcessor are on `DefaultShipHeader`; their salvager/depot blocks fall back to the
  shared default block until their headers are ported (**M2-R4 #4**, CAT-PROVISIONAL in `Salvage.c`).
- The Player-layout reads: `cat_cmHasSupportUnits` (TRUE), the experience level in `cat_shipGetSpeedMultiplier`, the
  crystal game option 0x008860ec bit 0x20, `SpaceObj.attributes` as a dword (bit 0x40000) (**M2-R4 #5**).
- The command messages (`clCommandMessage`, `cat_ioMessageAdd`) and the `madAnimationStart` mode argument
  (**stage 3**); `RemoveShipsFromDoingStuff`'s reason argument (**M2-R4 command follow-up**); the AI's dock order for
  converted ships (**stage 4**).

## Subsystem `gun`: the weapon word, damage by weapon against armour, the gun and damage bodies

**Data model** (`SpaceObj.h`, every field placed by the exe offsets read from the disassembly):
- `Gun.catWeaponType`, the exe's Gun +0x00 (the current WEAPON_*). The exe's Gun is 0x88 bytes (`univCreateShip`
  0x00542320 allocates numGuns * 0x88 + 4); the ILP32 pins `sizeof(Gun) == 0x88` and `gunstatic` at +0x84 hold
  (`tools/m2_layout32.sh` OK).
- `Bullet.bulletType` now holds the weapon word (exe Bullet +0x108, where HW1 keeps the bullet type; type `sdword`). The
  exe's readers test 2 (WEAPON_ION, which is HW1's BULLET_Beam value), 4 (WEAPON_LASER: no move, no collision) and 8
  (WEAPON_BEASTINFECT). New `Bullet` fields: `catBurst` (+0x10C, replaces HW1's BULLET_SpecialBurst type), `catMoved`
  (+0x13C), `catBigGunRange` / `catBigGunTravelled` (+0x150 / +0x154), `catPrevPosition` (+0x158). HW1's BULLET_Laser
  (3) is WEAPON_PLASMABOMB now, so the dormant DefenseFighter laser uses its own marker `CAT_BULLET_HW1DefenseLaser`
  (render.c, Collision.c, Physics.c, DefenseFighter.c).
- `Ship`: `catArmourLevel` (+0x3F0), `catBurstFiring` (+0x610), `catAttackStarted` (+0x616), `catQueuedAttackTarget`
  (+0x4B0), `catKillsByClass[12]` (+0x5C8), `catKilledByPlayer` (+0x60C).

**Tables read from cataclysm.exe** (`/tmp` reader over the PE sections; `ShipSelect.c`): `cat_damageVsArmourTable`
0x0072b228 (20 damage types x 3 armour levels), `cat_shipTypeArmourLevels` 0x0072b318 (145 x {start, max}),
`cat_shipTypeUpgradeMasks` 0x0072b7a0 (145), `cat_experienceDamageScale` 0x0072bb78.

**Functions** (exe address; what changed against HW1):

| function | port |
|---|---|
| `cat_weaponDamageVsArmour` 0x00514460, `cat_gunStaticBaseDamage` 0x00514600, `cat_shipGetDamageMultiplier` 0x005148f0 | new (`ShipSelect.c`); experience level CAT-PROVISIONAL #5 (level 0), Beast-mothership bonus CAT-TODO #4; easy single player doubles damage |
| `cat_shipUpgradesInit` 0x005140a0, `cat_shipApplyUpgrades` 0x00514490 | new; called by `univCreateShip` (the second unless bit 2 of `built`, an NIS ship). The player's research (Player +0x6504/+0x650C/+0x652C) is CAT-PROVISIONAL #5 and reads 0, the state `cat_playerUpgradesReset` gives a new game |
| `cat_gunSoundTypeLookup` 0x0048b7c0, `cat_gunStaticSetSoundTypes` 0x0048b790, `cat_gunGetSoundType` 0x0048b8c0, `cat_gunInSecondMode` (inlined test) | new (`Gun.c`): a gun's two damage types from its weapons and GUNSIZE into GunStatic +0x04/+0x08; also set by `LoadGunInfo` (LoadShip 0x00500490) |
| `cat_gunCanFire` 0x0048a1c0 | the second-mode firetime2 rules (shared between a ship's second-mode guns) |
| `gunCanShoot` 0x0048a2f0 | **delta map IDENTICAL, body MODIFIED**: no fuel test, health-scaled fire and burst-wait times (x1.25/1.75/2.5/4), no tactics/formation burst modifiers, Ship+0x610, passive-attack penalty also after a burst |
| `gunShoot` 0x0048b5b0 + new `cat_gunCreateBullet` 0x0048bb00 | REWRITTEN: returns the bullet; no fire from or at a ship being handed to a depot; LOS only for capital classes and not for the big gun; damage = multiplier x DamageMult (x KAS factor), no HW1 random/tactics damage; a shot at a Leech also does 10 damage; the bullet carries weapon, burst flag, damage type; velocity by weapon; lifetime/speed from the second mode; effects keyed on the damage type (ion beam 5 parameters, big gun at LOD 0); no recoil, no cloak-field decloak. The RKO's held-ship test is CAT-TODO #4 |
| `gunOrientGimbleGun` 0x0048a5e0 | no ion-cannon angles; bullet speed via `cat_gunGetBulletSpeed`; beam test on StartUpgradeLevel (ION, LASER); no assert |
| `gunShootGunsAtTarget` 0x0048c2e0, `gunShootGunsAtMultipleTargets` 0x0048c690 | no range bonus; big gun and Beast weapon skipped; dissolving ships; `cat_gunCanFire`; fixed guns aim with `cat_gunFixedGunAimedAtTarget`; Acolytes launch no missiles; Mission 15's KAS stop is CAT-TODO stage 6 |
| `missileShoot` 0x0048aac0 | depot early-outs; damage = multiplier x weapon-vs-target-armour x DamageMult at launch; damage type via the second mode; the fire effect's second parameter by launcher type; no cloak-field decloak. The rest of the body (its Missile layout) is HW1's |
| `gunShipFirePower` 0x0048cb60 | one argument; base damage per firetime; the exe's 3 interceptor types; HW1's `gunFirePower` deleted (no exe counterpart); callers in Battle.c, ShipView.c |
| `ApplyDamageToTarget` 0x00546250 (`UnivUpdate.c`, named for this port by the univupdate port) | 7 arguments (attacker, call-site code; every caller passes the exe's values, read from the pushes before each call); ship parts pass damage to their host; KAS derelicts, NIS bars; kill counts per class and per player; no slave ships; experience update CAT-TODO #5; game-stats layout CAT-TODO #5 |
| new `cat_tacticsEvasiveRetreatToDock` 0x00529f90 (`Tactics.c`) | an evasive ship below 0.333 health docks; the exe's dock type 0 is CAT-PROVISIONAL (HW1's refuel+repair) until the `dock` subsystem |
| `univBulletCollidedWithTarget`, `univUpdateAllPosVelBullets` (`UnivUpdate.c`) | damage x `cat_weaponDamageVsArmour(type, target armour)`; the burst explosion on `catBurst` scaled by the table; beams and Beast bullets keep their flight effect |
| `physUpdateBulletPosVel` 0x004e45b0 (`Physics.c`) | switch on the weapon word; previous position; no expiry before the first move; big-gun range and DFG-field test (`cat_dfgFieldContainsPoint` CAT-TODO #4) |
| `dmgPlayEffect` 0x00462340, `dmgShipThink` 0x004627b0 (`Damage.c`) | **dmgShipThink: map IDENTICAL, body MODIFIED**: a fourth argument (health over the level's limit), chance 1 in 1 + 20x from stream 0, crystals flash (Ship +0x4D0..), `etgFrequencyExceeded`; dmgShipThink skips ships being built externally, not hyperspacing ones |
| `needToGoToSameVerticalPlane` 0x00421a60, `attackStraightForward` 0x00421cf0 (`Attack.c`) | carrier class + sProcessor/bProcessor/tMissileDestroyer/sDreadnought/p1Mothership; a mimicking mimic does not shoot |
| `univDeleteDeadShip` (`UnivUpdate.c`) | the AI's salvage depot is reset in single player (0x00547d37) |
| `cat_univShipDeferredUpdate` (`UnivUpdate.c`) | the queued attack target becomes a clAttack; the stun timers (+0x4D0..+0x4DC) cycle; a burst 0.1 s past its fire time ends |
| `StandardDestroyerHouseKeep`, `cat_isBigGunBullet`, `DFGFrigate` deflect effect (Ships) | read the gun word / bullet word / damage table instead of their M2-R4 #3 stand-ins |

## Subsystem `beast`: the Beast infection weapon runtime (`BeastWeapon.c`)

Every CAT-PROVISIONAL entry of M2-R3 is a port now (0x004298f0..0x0042b650):
`cat_beastWeaponHouseKeep` 0x0042a900 (charge, Sentinel-field retargeting, the shot through `gunShoot`, the cascade
with its filter 0x0042a740), `cat_beastAttackHit` 0x0042af40 (hits, stun, infection, non-infectable damage, the hit
list), `cat_beastWeaponBulletUpdate` 0x00429d80 (the beam between its ships), `cat_beastWeaponBulletRemoval`
0x0042a1b0, `cat_beastWeaponRemoveShipReferences` 0x0042a280, `cat_beastWeaponFindNearestTarget` 0x0042a3d0,
`cat_beastWeaponHasTarget` 0x0042a4a0, `cat_beastWeaponAttack` 0x0042a550, `cat_beastWeaponSave/Load/Fix`
0x0042b270/0x0042b380/0x0042b490, `cat_beastWeaponChargeFraction` 0x0042b570, `cat_beastTimeSpentStunned` 0x0042b630,
`cat_shipCanUseBeastWeapon` 0x00429c50. The hit entries are `CatBeastHit` (0x1C on ILP32, asserted). The exe leaves
removed entries allocated (`listRemoveNode`/`listRemoveAll`); the port frees them (`listDeleteNode`/`listDeleteAll`;
no behaviour depends on it). New helpers: `cat_shipStealthTargetState` 0x0053ff30, `cat_shipIsAcolyteOrDerelict`
0x005ad360 (`ShipSelect.c`), `cat_collLineTriangleIntersect` 0x0043a6b0, `cat_collLineSentinelFieldIntersect`
0x0043abf0 (`Collision.c`; the Sentinel's triangle list is CAT-PROVISIONAL #4: no Sentinel field exists before ship
code B), `cat_vecPlaneNormalFrom3Points` 0x00550c90 (`Vector.c`).

## Subsystem `collision`: kamikaze, blasts, physics, the collision gates

**Kamikaze** (`Collision.c`, `Ships/GenericDefender.c`):
- `kamikazeCheck` 0x0043ab20 (declared in `Collision.h` now) asks the new static `cat_kamikazeCheckShip` 0x0043a980
  for each of the two objects: the damage to itself and to the other, mimics (`CAT_SHIPFLAG_Mimic`), the Beast cruise
  missile's infection hits (Ship +0x614, `catKamikazeInfectionHits`), no hit between ships clamped together
  (`cat_shipsClampedTogether` 0x005bfbd0) and the ram damage scale (`cat_shipGetRamDamageScale` 0x005b9980:
  CAT-PROVISIONAL 1.0 until the mimic / P1 corvette specifics, **M2-R4 #4**).
- `doKamikazeAttack` 0x005b1a50: `cat_kamikazeSteer` 0x005b16f0 (inside 800 units a kamikaze at least as fast as its
  target aims at where the target will be next frame and turns 7% towards it), `cat_kamikazeCheckHit` 0x005b1990 (a
  touch of the target's box hits at once) and `cat_shipBlendHeadingDefault` 0x005bffe0 (weights 0.07 / 1 from the
  exe's data 0x008c5da4 / 0x008c5da8).

**Blasts** (new `Game/Blast.c` / `Blast.h`, REULA header, in `src/Game/meson.build` after `Battle.c`):
the exe's expanding shock waves 0x00515940..0x00516f90. The six templates (0x00893da0, 0x68 bytes apart) and the
random-angle tables are the exe's; `CatBlast` is 0x64 bytes on ILP32 (asserted). Ported: `cat_blastInit` 0x00516370,
`cat_blastReset` 0x00516390, `cat_blastCreate` 0x00515a50, `cat_blastCreateSlipgateExplosion` 0x00515b40,
`cat_blastCreateBigGunExplosion` 0x00515bd0, `cat_blastUpdate` 0x00516020 / `cat_blastUpdateAll` 0x00516320,
`cat_blastAffectShip` 0x00515d30 (damage = const + k/d + k/sqrt(d) + k/d^2 credited to the blast's player, push by
size) [audit: the port summed the damage terms in another order than the exe; fixed to the exe's
((damage + const) + k/d) + k/d^2, then k/sqrt(d) + that], `cat_blastAffectObject` 0x00516da0, `cat_blastSpawnEffect` 0x00515940, the big-gun shell list
(`cat_bigGunBulletTrack` / `Untrack` / `RebuildList` / `InFlight` 0x00516f20 / 0x00516f50 / 0x00516d60 / 0x00516d40).
Wired as in the exe: `universeReset` -> `cat_blastReset`, `univUpdate` -> `cat_blastUpdateAll`, `gameStart` (new game,
`SDL/utility.c`) and `singlePlayerLoadNewLevel` -> `cat_blastInit`, `MothershipDied` 0x005bb9c0 ->
`cat_blastCreate(CAT_BLAST_MOTHERSHIP, ...)` (and the clamped ships being built die one second later). Open: the blast
drawing and lightning (`cat_blastRenderAll` 0x00516750, `cat_blastDrawLightning` 0x005163b0) and its sound
(`cat_soundEventBlast` 0x0051fc60) are **stage 3**; the drift a blast starts and the ramming-frigate release are
**M2-R4 #4**; the type-1 blast's attribute bits 0x10000 / 0x20000 need the attribute dword (**M2-R4 #5**,
CAT-PROVISIONAL); nothing fires big-gun shells before the big gun (**M2-R4 #4**), so the shell list stays empty.

**Physics** (`Physics.c`, `Timer.c`, `UnivUpdate.c`):
- `physApplyForceToObj` 0x004e43f0: the single-player hyperspace condition (no translation force while decelerating
  out of a jump).
- `physUpdateObjPosVelBasic` 0x004e5720: top speed x the ship's speed multiplier; a drifting non-killer asteroid that
  was not pushed this frame slows down and stops.
- `physUpdateObjPosVelShip` 0x004e4950: a drifting ship (Ship +0x550) is not accelerated and may reach 1400; the
  speed multiplier; helpless (+0x530) or drifting ships keep their speed; the universe clamp in multiplayer; no fuel
  burn (`FALSE &&` keeps HW1's fuel lines compiled out); the FOW grid update of fog-seeing ships is CAT-TODO
  **stage 3**.
- New `cat_physApplyForceToVelocity` 0x004e4390, `cat_physClampVelocityToUniverse` 0x004e4850, `cat_timerFractionElapsed`
  0x005284c0, `cat_univPointInUniverse` 0x005c1f70.

**Collision gates** (`Collision.c`):
- `collCheckShipShipColl` 0x0043ade0 / `collCheckBigShipSmallShipColl` 0x0043b460: the clamp test, no bump damage from
  an attached Leech (`cat_leechIsAttached`: CAT-PROVISIONAL FALSE, **M2-R4 #4**) or between dRKO pieces
  (`cat_shipIsRKO` 0x005c54c0); the ramming frigate's own collision (`cat_rammingFrigateCollide`) is CAT-TODO
  **M2-R4 #4**.
- `collCheckShipResourceColl` 0x0043bac0: every resource-class ship and the killer-collision attribute decide;
  `CAT_ATTRIBUTES_KillerCollision` moved from `UnivUpdate.c` to `Attributes.h` (Physics.c reads it too).
- `collCheckBeamColl` 0x0043c900 (this run): a beam that hits an sSentinel / bSentinel with its field up only draws
  the hit (`univBulletCollidedWithTarget` first argument 0); a missed Beast beam (`GS_BeastWeapon`) still draws its hit
  on its own target's box (`collCheckRectLine`), no damage. The exe pushes a seventh value (1, 2, 3) that
  `univBulletCollidedWithTarget` 0x00546a20 never reads (no access above `[esp+0xa8]` in its disassembly).
- `collCheckBulletTargetColl` 0x0043cd90 (this run): an enemy bullet whose line crosses an active Sentinel field within
  its travel is drawn and deleted (`cat_collLineSentinelFieldIntersect`); DFG frigates act through the new
  `cat_collBulletDFGFieldCheck` 0x0043cd10 after the collision-sphere prefilter. **Before this, no DFG field acted on
  any bullet**: the HW1 test used the dormant HW1 type `DFGFrigate`, not `bDFGFrigate` / `tDFGFrigate`. Open:
  `cat_sentinelFieldBulletHit` 0x005c6c60 and the TRUE return after a detonated big-gun shell (**M2-R4 #4**), the mesh
  hit location `cat_meshLineIntersect` 0x004b5910 (**stage 3**).
- `univMissileCollidedWithTarget` 0x00547080 (this run; `UnivUpdate.c`) takes the new first argument (all four exe
  callers, `collCheckMissile{Ship,Resource,Derelict,Mine}Coll` 0x0043d330 / 0x0043d720 / 0x0043d970 / 0x0043dbb0, push
  1) and applies the friendly-fire modifier in every build (HW1 skipped it in debug builds); the mine-field chatter's
  `battleChatterFleetAttempt` is CAT-TODO **stage 3**.

## Subsystem `dock`: resources, the sCarrier door, the launch manager's module slots; the dock rewrite that is left

**`ResCollect.c`** (asteroid break pieces and the harvest loop):
- `AsteroidTakesDamage` 0x004f1730 (REWRITTEN): the asteroid's RUs follow its health fraction; TRUE when it was alive
  and its health is gone. HW1's RU damage ratio, health test and shrinking are gone.
- `BreakAsteroidUp` 0x004f1580: the pieces its .shp lists (`NUMBER_OF_ASTEROID_PIECES` / `AsteroidBreakInfo`, loaded by
  M2-R3 into `AsteroidStaticInfo` +0xC0 / +0xC4): offset by the asteroid's matrix, pushed along the offset by the first
  value, oriented by the piece matrix, spun by the other three. `univDeleteDeadResources` 0x00549270 breaks every dying
  asteroid that is no killer. The break sound (`cat_soundEventAsteroidBreak` 0x005215d0) is **stage 3**.
- `CollectResources` 0x004f17a0: tutorial x5, Bulimia (specialFlags2 0x200000), the asteroid's health follows its RUs,
  "collector full" battle chatter 0x4B, the AI resourcing marks as Ship words (`catResourceFinished` +0x1D4,
  `catResourcing` +0x1D8).
- `processCollectResource` 0x004f2130 (this run) and new `cat_rescollectStopHarvest` 0x004f2450: a resource that moves
  too fast, is taken or is outside the world is dropped with the harvest; a found resource is used only below
  `resourcesAtOneTime`, else the ship docks; a full ship sets `SPECIAL_2_CatResourcing` (0x4000000) to resume in its
  volume; no harvester-done messages or speech; `aitrackSteadyShip` when the order ends or becomes a dock order.
- `rescollectFindNearestResource` 0x004f11f0 (this run): the `SPECIAL_2_CatResourcing` restart with
  `COLLECTRESOURCE_ZONE`; only the local and the AI players search outside a volume. Its tail,
  `cat_clHarvestNearestResource` 0x00449790 (CommandLayer.c: a search with the fourth argument, the out-of-resources
  message and speech, the multiplayer dock, the re-issued collect order), is not ported; HW1's plain search stands in.
- `ChangeSingleShipToCollectResource` 0x004f12c0 (this run): the resource is a second argument (clProcess passes NULL);
  `R1ResourcerHarvestsAsteroid` 0x004f1d40: a NULL resource resets the harvest state; `ResourceMovingTooFast`
  0x004f10f0: no Mission 6 limit. The three are **delta map IDENTICAL, body MODIFIED** (review entries).
- `TurnHarvestEffectOn` 0x004f1ac0 / `TurnHarvestEffectOff`: no harvest sound. The exe folds `TurnHarvestEffectOff(ship)`
  into `stopRepairEffect` 0x005c23b0 (identical code), and its 0x004f1a50, which the delta map calls
  `TurnHarvestEffectOff`, is `TurnOffAnyResourcingEffects(todo)` (its callers `clResourceDied` and
  `processCollectResource` pass the command; review entry). The exe passes the beam length and radius to the effect as
  real32 values; the tree's ETG still takes integers (**stage 3**).
- Removed as the exe has none of them (`strings -a cataclysm.exe`): the tweakables `ASTEROID_RU_DAMAGE_RATIO[]`,
  `ASTEROIDBREAK_{LO,HI}_VELOCITY`, `ASTEROIDBREAK_{LO,HI}_ROTATE`, `ASTEROIDBREAK_RADIUS_SCALE`,
  `MAX_RESOURCE_VELOCITY_TO_CHASE_LEVEL6`; `HandleAsteroidScaling`; the dormant `AsteroidStaticInfo` fields
  `asteroidCanShrink`, `asteroidCanBreak`, `asteroidMinShrink`, `breakinto[]` (`sizeof(AsteroidStaticInfo) == 0xC8` is
  now asserted on ILP32, the stride of `universeStaticInit` 0x0053d2a0). `MakeNewAsteroidStaticInfo` (UnivUpdate.c) has
  no caller left; it stays for the univupdate owner.
- `univFindNearestResource` 0x00549c50's fourth argument, fog-of-war, technology and stealth tests: the CAT-TODO in
  `UnivUpdate.c` that named this port now names **M2-R4 #5** (they read Player +0x6500 / +0x2A4).

**The sCarrier door** (`Ships/Carrier.c`, `MeshAnim.c`): `CarrierHouseKeep` 0x005af210 closes the door (animation 0)
once it is open and a ship holds one of its `DPT_Med_In` / `DPT_Med_Out` points (busy words, DockInfo +0x24), and
reopens it (animation 1) a random 5-10 s (stream 10, `RANDOM_GAME`; 5 / 2^32 x random + 5, constants 0x0072bf7c /
0x0072a624) after the close animation is over and the points are free; state and timer at CarrierSpec +0x160 / +0x164
(Ship +0x798 / +0x79C). New `cat_madAnimationFinished` 0x004b7090 (MeshAnim.c; also on ship code B's list, it is done).
Open (**stage 3**): the exe's `madAnimationStart` 0x004b6b90 looks the animation number up in `madstatic.animIndexes`
itself and takes a third argument; the door does the lookup before calling the tree's two-argument version, and an
sCarrier without mesh animation is skipped (the exe has no such test).

**The launch manager** (`LaunchMgr.c`, `Ships/Mothership.c`): the seven module slots (0x00a4478c) and
`cat_lmModuleSlotsRemoveType` 0x0049e970 (empties the slots of a type, packs the rest forward); `MothershipRemoveShipReferences`
0x005bba70 calls it, as the exe does, while `lmActive` (0x00a446e8) is set for a component-class ship of the same
player. Open: `cat_lmFillInModuleSlots` 0x0049e9f0 needs the module list (`cat_moduleIterate` family, **M2-R4 #4**),
so the slots stay empty; the slot draw callbacks are **stage 3**. "Beast systems off": in single player
`cat_lmBeastSystemsOff` 0x0049fc30 comes down to `cat_clSetBeastSystem` 0x0044e120 storing 0, which
`beastMothershipSelfDamage` already does; the multiplayer packet (misc command 10, `cat_clWrapSetBeastSystem`
0x004535f0) is **M2-R4 #4** and the radio button **stage 3**.

**Not done: the Cataclysm dock purposes.** Cataclysm replaces HW1's dock bitmask (`DOCK_FOR_REFUEL`, `DOCK_TO_DEPOSIT_RESOURCES`
...) with a purpose number 0..10 and a dock class per ship pair (`cat_dockGetDockClass` 0x00463d70, 784 B), and HW1's
per-ship-type docking paths (`ShipDocksAtCarrierMother`, `ShipDocksAtResourceCollector`, `ShipDocksAtASF`, the research
ship paths, the DDDF drones ...) with one generic, cone-based state machine (`cat_dockShipDocksAtShip` 0x00465860,
3,395 B, with `cat_dockFlyToConeOriginLatch`, `cat_dockFlyOutOfCone`, `cat_dockFindNearestDockPoint`,
`cat_dockFindLaunchPoint`, `cat_dockFinishedDocking`, `cat_dockCanDockWithBusyHost`, `cat_dockInitShipDockInfo`,
`cat_dockHasFreeSlotForShip`, `cat_dockFlyInFrontOfHost` ...). Of `Dock.c`'s 123 functions only 18 have an exe
counterpart; 95 are HW1-only. `ThisShipCanDockWith` 0x00464290 takes a fourth argument and tests per-class counts at
DockInfo +0x00 against `DockStaticInfo.numOfType` (+0x23). Porting this means switching every `clDock` /
`FindNearestShipToDockAt` / `ThisShipCanDockWith` caller (CommandLayer, CommandWrap, KAS, AI, Tactics, ResCollect,
Salvage, RepairCorvette) to purposes, replacing the per-type paths and the `StaticDockInit` / launch-point tables, and
testing it with harnesses that dock. That did not fit this run. The WIP's cone helpers that salvage uses are in place,
and the code marks every waiting site `CAT-TODO(M2-R4 #3 follow-up, dock purposes)` (ResCollect.c) or names #3 (the
existing notes in `CommandWrap.c`, `CommandLayer.c`, `RepairCorvette.c`, `Tactics.c`, `SpaceObj.h`
`SPECIAL_EXITING_DOOR`). **Owner: an M2-R4 #3 follow-up (dock purposes)**, before stage 4's AI uses docking.

## Subsystem `final`: builds, runs, delta-map reviews, what is left

### Builds
- `tools/build.sh --debug --clean`: rc 0, 0 `warning:` lines (-Werror); the later incremental debug builds too.
- `tools/build.sh --release --clean`: rc 0, 0 `warning:` lines.
- `tools/m2_layout32.sh`: "ILP32 ship specifics: 24 files, every sizeofShipSpecifics assert holds", "m2_layout32: OK"
  (with the new `sizeof(AsteroidStaticInfo) == 0xC8`, `CatBlast` 0x64, `CatBeastHit` 0x1C, `Gun` 0x88,
  `CatSalCapCorvetteSpec` 0x30).

### Plain-harness runs
All with a copy of the debug binary in `original/scratch-m2/m2r4-combat/`, `tools/m2_harness.sh <label> --mode sp
--timeout 360 --bin <copy>`; logs in `original/scratch-m2/m2r2-harness/runs/<label>/`.

| run | after | milestone | first stop | univUpdateCounter | ASan | leaks |
|---|---|---|---|---|---|---|
| `m2r4-combat-salvage-sp` | salvage | universe running | none: still running at the 360 s timeout | 1,584 | 0 | none unsuppressed |
| `m2r4-combat-gun1-sp`, `-gun2-sp`, `-gun3-sp` | gun steps | universe running | none (timeout) | 1,584 each | 0 | none |
| `m2r4-combat-coll1-sp` | kamikaze, blasts | universe running | none (timeout) | 1,584 | 0 | none |
| `m2r4-combat-coll2-sp` | physics | universe running | none (timeout) | 1,584 | 0 | none |
| `m2r4-combat-final-sp` | ResCollect, door, module slots, beam, missiles | universe running | none (timeout) | 1,584 | 0 | none |
| `m2r4-combat-final2-sp` | everything (DFG and Sentinel bullet checks last) | universe running | none (timeout) | **1,584** | **0** | none |
| `m2r4-combat-statics` (`--mode statics`) | everything | ship statics self-test done | none (rc 0) | n/a | 0 | none |

The R3 / univupdate / command runs reached 1,248 ticks (`r4c-final-sp`); every run of this port reached 1,584. [audit: the difference is the timeout, not progress: this port's runs used `--timeout 360`, `r4c-final-sp`
used 340, and the counter is the debugScreenshots line printed every 112 ticks; the audit's 340 s run of this port's
binary, `r4cb-audit-sp`, reached 1,248, as `r4c-final-sp`.] UBSan
kinds are the ones before this port (`fescreen`, `etgdecision`, `PATCH`, `etgfunctioncall`, `mhlocalbinding`,
`etgconditional`, `etgbranch`, `fquant.c`). Statics: 143 of 145 ship types loaded, 2 not found, 27 other .shp, as
`r4c-final-statics`. The sp run does not reach a dock, a DFG field or a blast in its 1,584 ticks, so those paths are
build- and read-checked only.

### Delta-map review entries
40 entries in `ghidra/delta_map_review.json`, merged with `tools/delta_map.py merge --done m2r4:combat-review`
(IDENTICAL 3,096 -> 3,091, MODIFIED 1,247 -> 1,252, reviewed 1,491 -> 1,529):
- **IDENTICAL found modified (now MODIFIED):** `gunCanShoot` 0x0048a2f0, `dmgShipThink` 0x004627b0,
  `ResourceMovingTooFast` 0x004f10f0, `ChangeSingleShipToCollectResource` 0x004f12c0, `R1ResourcerHarvestsAsteroid`
  0x004f1d40.
- IDENTICAL kept with a note: `battleAccountForLosses` 0x00423e50 (only the `gunShipFirePower` call), `stopRepairEffect`
  0x005c23b0 (also HW1's folded `TurnHarvestEffectOff`).
- MODIFIED / REWRITTEN ports: `TurnHarvestEffectOff` 0x004f1a50 (really `TurnOffAnyResourcingEffects`),
  `TurnHarvestEffectOn`, `processCollectResource`, `rescollectFindNearestResource`, `BreakAsteroidUp`,
  `CollectResources`; `kamikazeCheck`, `collCheckShipShipColl`, `collCheckBigShipSmallShipColl`,
  `collCheckShipResourceColl`, `collCheckBeamColl`, `collCheckBulletTargetColl`, the four `collCheckMissile*Coll`;
  `physUpdateBulletPosVel`, `physUpdateObjPosVelShip`, `physUpdateObjPosVelBasic`; `gunOrientGimbleGun`,
  `missileShoot`, `gunShoot`, `gunShootGunsAtTarget`, `gunShootGunsAtMultipleTargets`, `gunShipFirePower`;
  `dmgPlayEffect`, `attackStraightForward`, `unClampObj`, `updateClampedObject`, `kasfShipsDamage`,
  `dockPutShipInside`, `CarrierHouseKeep` and `MothershipRemoveShipReferences` (M2-R3 change text kept, note added).
Functions this port touched only at a salvage reader or a call (e.g. `SaveShip`, `clDock`, `gameStart`,
`aishipFlyToPointAvoidingObjsFunc`) got no review entry: they were not re-read whole.

### Edits outside the file list, and why
`UnivUpdate.c` (bullet / missile hits, the 7-argument `ApplyDamageToTarget`, the clamp scheme, the resource TODO),
`CommandLayer.c` (salvage readers, `ChangeSingleShipToCollectResource`'s argument), `ShipSelect.c` (weapon / armour /
upgrade tables, salvage helpers), `Tactics.c` (`cat_tacticsEvasiveRetreatToDock`), `Universe.c` / `SDL/utility.c` /
`SinglePlayer.c` (salvage and blast resets), `Ships/Mothership.c` (blast, module slots), `Ships/GenericDefender.c`
(kamikaze), `Ships/Carrier.c` (door), `MeshAnim.c` (`cat_madAnimationFinished`), `Battle.c` / `ShipView.c`
(`gunShipFirePower`), `Clouds.c` / `KASFunc.c` (`ApplyDamageToTarget` call-site codes), `SaveGame.c` (salvage chunks,
gun sound types), `Timer.c` / `Vector.c` / `Attributes.h` (helpers the ports call), `SDL/render.c` and the Ships files
`DefenseFighter.c` / `DFGFrigate.c` / `StandardDestroyer.c` (readers of the bullet / gun word): each is a caller or a
callee of a function this port changed, or a reader of a field it renumbered.

### What is left, and its owner
- **M2-R4 #3 follow-up (dock purposes):** the Cataclysm dock rewrite (purpose numbers, `cat_dockGetDockClass`,
  `ThisShipCanDockWith`'s fourth argument, `cat_dockShipDocksAtShip` and its helpers, `clDock` 0x00466c70,
  `dockReserveDockPoint` / `RemoveShipFromDocking` / `RemoveShipFromLaunching` (REWRITTEN), `processDockToDo`'s
  swarm part, `dockPutShipOutside`'s modules / FOW / rotations), `cat_clHarvestNearestResource`, and every
  `CAT-TODO(M2-R4 #3 follow-up, dock purposes)` site; `SPECIAL_EXITING_DOOR`'s Dock.c readers go with it. Also the
  Missile layout float at +0x254 (`Select.c`).
- **M2-R4 #4 (ship code B):** crystal processing in depots, Worker / Processor headers and the Worker harvest
  callback 0x005c9130, Sentinel field (`cat_sentinelFieldBulletHit`, its triangle list), Leech attach, RKO hold,
  ramming frigate collide / release, drift start, the big gun (shell detonation in DFG fields and blasts, the
  big-gun block), the module list (`cat_lmFillInModuleSlots`), `cat_clWrapSetBeastSystem`'s multiplayer path, the
  mimic ram damage scale, `cat_dfgFieldContainsPoint`.
- **M2-R4 #5 (Player layout):** experience levels, research upgrades, the attribute dword (blast bits 0x10000 /
  0x20000, crystal option), game stats, `cat_cmHasSupportUnits`, `univFindNearestResource`'s fourth argument and the
  per-asteroid movement box, the two globals in `processCollectResource`.
- **Stage 3:** sounds (blast, asteroid break, harvest), blast rendering, FOW updates, `madAnimationStart`'s number
  lookup and third argument, ETG real32 parameters, the mesh hit location, command messages and battle chatter
  (`battleChatterFleetAttempt`), the launch manager's slot drawing and radio buttons.
- **Stage 4:** the AI readers of the retired `SPECIAL_Finished_Resource` / `SPECIAL_Resourcing`, the AI's dock order
  for converted ships. **Stage 6:** Mission 7 / 15 KAS parts. **M2-R4 command follow-up:** `RemoveShipsFromDoingStuff`'s
  reason argument.

### Open questions
1. The dock purposes were not ported in this run; should they be a separate follow-up producer before ship code B
   (several of #4's ships dock: Workers, Processors, module ships), or go with #4?
2. `cat_madAnimationFinished` 0x004b7090 is on ship code B's list and is done here; #4 should not port it again.
