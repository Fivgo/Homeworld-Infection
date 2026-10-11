<!-- done: interceptor, statinits, slots, bodies, final -->
# M2-R3 producer 4: ship code A, the HW1 headers the exe reuses

Status: DONE (2026-10-10); DONE WHEN met (table in section 5). Not committed. Branch `m2`, worktree `.claude/worktrees/m2`. Scope: docs/m2-r2-summary.md "Plan for
M2-R3 and M2-R4", row 4: `CustShipHeader` slot wiring (slots 9, 12-14, 20, 22-34), the StaticInit and slot bodies of
the HW1 ship headers Cataclysm reuses, `GenericInterceptorStaticInit` (0x005b1e70) first; Ships/* 80 / 45 / 12
HW1-matched functions (15,070 MODIFIED bytes) plus the part of the Ships/* cat-new code these headers call.

Subsystems (the done marker above lists the finished ones):

1. `interceptor`: `GenericInterceptor.c`: the 14-type `GenericInterceptorStaticInit`, Cataclysm's cloak (new
   `Game/Cloak.c`) for the cloaked fighter, and the interceptor, cloaked fighter and attack bomber headers.
2. `statinits`: the StaticInit bodies of the other reused HW1 headers (race/type blocks re-keyed from the exe), so
   every Mission01 ship passes its slot-1 call.
3. `slots`: the 29 reused HW1 headers wired to the exe's slot contents: new slots 9/12-14/20/22/27, ICF-folded
   bodies, and the Cataclysm-only functions they point at (Carrier, HeavyCruiser, Mothership, P1Mothership,
   SalCapCorvette, P1StandardCorvette and the small ones), with the salvage depot / salvager and Beast-weapon hooks
   (new `Game/Salvage.c`, `Game/BeastWeapon.c`).
4. `bodies`: the remaining MODIFIED / REWRITTEN slot bodies of Ships/* (interceptor attack/fire, defender, gravwell,
   minelayer, proximity sensor, repair, DFG field, ...).
5. `final`: HW1 aliases, delta-map review entries, builds, plain-harness runs.

Method: the r5 decompile (`ghidra/exports/cataclysm_gog_r5/decompiled.c`) read function by function; the exe's header
contents from `original/scratch-m2/m2r2-shipstatic-tools/headers.txt`; every key table read from cataclysm.exe with
`dumptab.py`, float constants with `fl.py`. Scratch helpers in `/tmp/m2r3sa/` (outside git): `fn2.py` (exe body),
`grepfn.py` (function of each decompile match), `cmphdr.py` (every tree header against the exe's), `slotfns.py`
(every function the exe puts into the 29 reused headers, and whether the tree has it), `dumptab.py`, `fl.py`.

## 1. `interceptor`: GenericInterceptor.c and the cloak

`src/Ships/GenericInterceptor.c`, `src/Ships/GenericInterceptor.h`, new `src/Game/Cloak.c` / `Cloak.h`,
`src/Game/ShipSelect.c` / `.h`, `src/Game/meson.build`.

| change | exe evidence |
|---|---|
| `GenericInterceptorStaticInit` picks one of **14 per-type blocks** by `statinfo->shiptype`: sRecon, sAcolyte, sMimic, bRecon, bAcolyte, bInterceptor, bCloakedFighter, bAttackBomber, kAttackBomber, kHeavyInterceptor, tAttackBomber, tHeavyInterceptor, p1Fighter, dAcolyte (HW1: light/heavy/bomber/cloaked x R1/R2 plus P1/P2/TargetDrone blocks). This was the sp stop `GenericInterceptor.c (205)` | `GenericInterceptorStaticInit` 0x005b1e70: switch on statinfo+0x1E0 over 0, 1, 2, 0x21-0x25, 0x3D, 0x44, 0x4B, 0x52, 0x5D, 0x85, blocks 0x00a7e4a0..0x00aaa5e0 |
| the rest of the body is HW1's (clear 0xD90 dwords, key table, squares, per-class fallback to the default class); the TargetDrone bullet-range override is gone | same body; no TargetDrone type |
| `GenericInterceptorStatics` pinned: 0x3640 bytes, `FlightManProb` 0x148, offsets of breakRange/triggerHappy/maxAttackTime/flightmanProb (only real32/udword members, same on LP64) | key table 0x008c4378 (offsets 0x4/0x13C/0x1D8/0x274/0x310/0x3AC/0x448), memset of 0xD90 dwords |
| `CloakedFighterStaticInit` only calls `GenericInterceptorStaticInit`; `CloakedFighterStatics` and its fuel/cloak key table are gone | `CloakedFighterStaticInit` 0x005b36d0 |
| `CloakedFighterSpec` = interceptor specifics + Cataclysm cloak block (+0x18) + 8 per-player detected flags (+0x30), 0x50 bytes (asserted) | header size 80; `CloakedFighterInit` 0x005b36f0 inits Ship+0x650; `sub_53F9F0` 0x0053f9f0 reads Ship+0x668 |
| `CloakedFighterInit` = activeGun 0 + `cat_cloakInit(ship, &spec->cloak, 0.1, 0.1)` | 0x005b36f0 |
| `CloakedFighterSpecialActivate` = `cat_cloakActivate` (no fuel test, effect or chatter) | 0x005b3720 |
| `CloakedFighterFire`: no fire while fully cloaked, else the interceptor fire | 0x005b37b0 |
| `CloakedFighterHouseKeep` = `cat_cloakUpdate` | 0x005b3840 |
| new `CloakedFighterAttack` / `CloakedFighterAttackPassive` (slots 21 / 6): attack as an interceptor while visible, decloak when fully cloaked | 0x005b3730 / 0x005b3770 |
| `GenericInterceptorSpecialActivate` returns TRUE and does nothing (HW1's LightInterceptor speed burst gone); it is slot 7 of the interceptor and attack-bomber headers, where the exe has the folded `return 1` body | headers 0x008c4448 / 0x008c4568 slot 7 = `kasfSpecialOff` 0x00497db0 (`return 1`) |
| headers: cloaked fighter slots 6/21 = the two new bodies; `TargetDroneHeader` removed (no type, never in `shipHeaders`); specifics sizes asserted (`GenericInterceptorSpec` 0x18, `AttackBomberSpec` 0x38) | headers.txt sizes 24 / 80 / 56 |
| **new `Game/Cloak.c`**: `cat_cloakInit`, `cat_cloakActivate`, `cat_shipIsCloakedFully`, `cat_cloakGetFadeState`, `cat_cloakGetChargeFraction`, `cat_cloakUpdate`: a 1000-point charge, drained while cloaked (`SOF_Cloaked`), refilled while visible; activation refused below 100 or while attacking/launching; a random fade delay 1 - e^(r * 100/2^30 * -0.005); fade step 0.05; per-player detected flags cleared on cloak and set on decloak; `shipHasJustCloaked` when the fade completes. Block layout `CatCloak` 0x18 (asserted) | 0x00435f10, 0x00435f50, 0x00436110, 0x00436130, 0x00436180, 0x004361c0; constants 0x0072a438 (1000), 0x0072a464 (100), 0x0072a654 (0.05), 0x0072a650 (0.001), 0x0072a64c / 0x0072a648 (r scale, -0.005), 0x0072a548 (1.0 double); speech 0x04000889 / 0x0400088B / 0x0400088D, sound 0x811 / 0x812 |
| `cat_shipIsCloakedFully` moved from `ShipSelect.c` (selection item) into `Cloak.c`, where the exe has it | 0x00436110 |
| `cat_shipDetectedByPlayer` (selection item's provisional) now reads the real flags of the three cloaking types; new `cat_shipStealthDetectedSet` sets them (mimic/MCV/p1StandardCorvette and leech flags stay provisional, M2-R4 #4); the speech-alert note it raises is CAT-PROVISIONAL (stage 3) | `sub_53F9F0` 0x0053f9f0, `cat_shipStealthDetectedSet` 0x0053fad0, `cat_speechAlertsNoteShip` 0x00522bc0 |

## 2. `statinits`: the StaticInits of the reused HW1 headers

Every HW1 StaticInit of the headers the exe reuses picks its statics block by `statinfo->shiprace == R1` (HW1's two
races). Cataclysm picks by the 7-race id (or by type). Each body below was compared with the exe; the key tables
(`dumptab.py`) have HW1's keys at HW1's offsets. "Other" = the exe uses the statinfo itself as the block for a race
that has no block (only ships of the listed races use the header); the port asserts and uses a spare block.

| file | blocks now (exe addresses) | exe function | other delta |
|---|---|---|---|
| `AdvanceSupportFrigate.c` | Kushan 0x00ab74a0, Taiidan 0x00ab74c0 | `AdvanceSupportFrigateStaticInit` 0x005aef00 | |
| `DFGFrigate.c` | one block 0x00aadcc0 for both races | `DFGFrigateStaticInit` 0x005b08f0 | |
| `GenericDefender.c` | kHeavyDefender 0x00aadc40, tHeavyDefender 0x00aadc20 (by type) | `GenericDefenderStaticInit` 0x005b1420 | no LightDefender |
| `GravWellGenerator.c` | Kushan 0x00a7ada0, else 0x00a7ade0 | `GravWellGeneratorStaticInit` 0x005b40a0 | |
| `HeavyCorvette.c` | Kushan 0x00a7ad80, else 0x00a7ad90 (Beast) | `HeavyCorvetteStaticInit` 0x005b4e10 | no memset first |
| `HeavyCruiser.c` | Beast 0x00a7ad40, Kushan 0x00a7ad20, Taiidan 0x00a7ad60 | `HeavyCruiserStaticInit` 0x005b5310 | bHeavyCruiser loads the Beast-weapon tweaks (`cat_beastWeaponStaticInit`, below) |
| `IonCannonFrigate.c` | Kushan 0x00a7ac80, Taiidan 0x00a7ac60, Derelict 0x00a7ac40 | `IonCannonFrigateStaticInit` 0x005b6820 | ranges x0.725 / x0.5 as HW1 (0x0072bfb0, 0x0072a4c8) |
| `MinelayerCorvette.c` | Taiidan 0x00a7ab00, else 0x00a7aba0 | `MinelayerCorvetteStaticInit` 0x005ba260 | no memset first |
| `MissileDestroyer.c` | Taiidan 0x00a7aa20, else 0x00a7a9e0 | `MissileDestroyerStaticInit` 0x005bb350 | key-table template moved to the Taiidan block |
| `MultiGunCorvette.c` | Beast 0x00a7a980, Kushan 0x00a7a960, Taiidan 0x00a7a970 | `MultiGunCorvetteStaticInit` 0x005bdb30 | no memset first |
| `P1IonArrayFrigate.c` | Beast 0x00a7a940, P1 0x00a7a900, Derelict 0x00a7a920 | `P1IonArrayFrigateStaticInit` 0x005be1a0 | |
| `P1MissileCorvette.c` | P1 0x00a7a8c0, else 0x00a7a880 (bMissileCorvette) | `P1MissileCorvetteStaticInit` 0x005be2e0 | no memset first |
| `ProximitySensor.c` | Kushan 0x00a7a7a0, else 0x00a7a760 | `ProximitySensorStaticInit` 0x005bf3d0 | |
| `ResourceCollector.c` | Kushan 0x00a7a608, else 0x00a7a604 | `ResourceCollectorStaticInit` 0x005c3d90 | |
| `StandardDestroyer.c` | Sect 0x00a7a520, Kushan 0x00a7a5a0, Taiidan 0x00a7a560 | `StandardDestroyerStaticInit` 0x005c7780 | the Sect block (sDestroyer) also reads the new keys `MissileRegenerateTime`, `MissileVolleyTime`, `MissileLagVolleyTime` (table 0x008c6950, +0x18..+0x20) |
| `StandardFrigate.c` | Kushan 0x00a7a4e0, else 0x00a7a500 | `StandardFrigateStaticInit` 0x005c78e0 | |
| unchanged | `FloatingCityStaticInit` 0x005b13e0, `P1MothershipStaticInit` 0x005be4f0: one block, the HW1 body | | |

The StaticInits of Carrier, Mothership, SalCapCorvette and P1StandardCorvette change their statics and specifics
layouts together with their slot bodies, so they are in `slots` (3).

**New `Game/BeastWeapon.c` / `.h`** (the Beast infection weapon of bHeavyCruiser, bCarrier, bMothership,
dBeastMothership): `cat_staticHasBeastWeapon` 0x00429cf0, `cat_shipHasBeastWeapon` 0x00429bc0 (types 0x2E, 0x2F,
0x35, 0x82), `cat_beastWeaponStaticInit` 0x00429b90 (reads `RACE_BEAST\beastweapon.script` once into
`cat_beastWeaponTweaks` 0x00c52140 with the exe's 6-key table 0x00751698; layout asserted 0x1C).

**Run after these two subsystems** (plain harness, debug ASan build), `sa-r2-sp`: rc 1 after 10.4 s, milestone
"level load (mission file read)", ASan 0. 33 `shipstatic: loaded` lines (every ship of Mission01: 15 Sect, 7 Kushan,
11 Taiidan); all 20 whose header has a StaticInit ran it (`staticinit=next`), the other 13 are the Cataclysm-only
Sect types still on `DefaultShipHeader` (`staticinit=none`: sAcolyte, sACV, sRammingFrigate, sWorker, sRepairDroid,
sProcessor and 7 sMothership modules; M2-R4 #4). First stop is now **after** the ship statics: `Game/LevelLoad.c (860)`,
`GetAppropriateShipTypeForRace`'s `RacesAllowedForGivenShip` assertion (HW1 body with the HW1 swap table; the exe's
0x004a13c0 uses a 145-entry replacement table 0x00883cf8), which is level load, not ship code (owner below).
Before: `sa-r0-sp` stopped at `GenericInterceptor.c (205)` after `sRecon.shp`; `sa-r1-sp` (interceptor only) at
`GenericDefender.c (70)` after 26 ships.

## 3. `slots`: the 29 reused HW1 headers wired to the exe

All 29 HW1 headers the exe reuses now hold the exe's slot contents (`cmphdr.py`: every remaining name difference is
one of the cases below). Each header carries a `CAT:` comment with its exe address and types. Rules used:

- **ICF-folded bodies.** The exe's linker merged identical functions, so a header slot names another ship's function:
  slot 6 `P1MothershipAttackPassive` 0x005be540 (`rotate && rotateToRetaliate ? attackPassiveRotate :
  attackPassive`), slot 21 `MissileDestroyerAttack` 0x005bb3e0 (`attackStraightForward(gunRange[t],
  tooCloseRange[t])`) and `SwarmerAttack` 0x005c79b0 (`attackSideStep(spec, statics)`), Mothership slot 6
  `RKOAttackPassive` 0x005c59a0 (`attackPassive`), slot 7 `kasfSpecialOff` 0x00497db0 (`return 1`) and
  `nullsubReturnFalse` 0x006fe9a0, ResourceCollector slot 3 `MiningBaseInit` 0x005b0360 (spec word 0 = 0). Each tree
  file keeps its own function where the body is the same code (every such statics block starts with gunRange[3] /
  tooCloseRange[3] or the side-step parameters, every such spec with the side-step state; checked file by file).
- **Slot 12** (remove object references) holds the slot-11 function in every header that has it; the tree casts it.
- **Specifics sizes**: every reused header's specifics is pinned to the exe's `sizeofShipSpecifics` on ILP32 (24
  files; `tools/m2_layout32.sh` now syntax-checks them with `-m32`). SalCapCorvette allocates the larger of the exe's
  0x30 and HW1's spec while HW1 readers of HW1's `SalCapCorvetteSpec` remain (memory only, see below).

| header (exe address) | change | exe evidence |
|---|---|---|
| `DefaultShipHeader` 0x008c39b0, `StationaryGunHeader` 0x008c3b60 | `DefaultShipFire`: `cat_gunCanFire`, fixed guns fire too, no assert; StationaryGun has no slot 21 (`DefaultShipAttackStationary` removed) | `DefaultShipFire` 0x005b0260 |
| `MiningBaseHeader` 0x008c3ad0 | `MiningBaseDied`: trMiningBase, no type assert. **Kept** HW1's two `listRemoveNode` calls the exe no longer has, because HW1's `univDeleteDeadShip` leaves the ship in both lists (UnivUpdate.c, M2-R4 #1) | `MiningBaseDied` 0x005b0370 |
| `AdvanceSupportFrigateHeader` 0x008c3408 | new slot 9 `AdvanceSupportFrigateOrderChanged`: stop a running repair effect | 0x005aef90 (Ship+0x38C = `rceffect`) |
| `CarrierHeader` 0x008c34d0 | `CarrierSpec` = weapon block, word, salvage depot (+0x1C), door state/timer (0x168, asserted); StaticInit by 5 races + statinfo +0x211..0x213 flags + Beast tweaks; Init (weapon, door 1/-1, depot); new slot 7 (FALSE), 9, 12, 14 (`CarrierBulletRemoval`); SpecialTarget (weapon if it has a target, else HW1 repair); HouseKeep/RemoveShipReferences/Fix/Died dispatch to weapon + depot; Died flags clamped component ships orphaned; slots 16-18 = Mothership's. HW1 drop targets / captured-ship queue removed. CAT-PROVISIONAL: the sCarrier door animation (needs producer 5's DockStaticInfo and madAnimationStart's 3rd argument) | 0x005aefb0, 0x005af090, 0x005af0f0, 0x005af170, 0x005af210, 0x005af360, 0x005af390, 0x005af3c0, 0x005af3f0 |
| `DFGFrigateHeader` 0x008c3ff8 | new slot 10 `DFGFrigateHouseKeep`: CAT-PROVISIONAL no-op (it disrupts enemy Sentinel fields via Sentinel specifics, M2-R4 #4); the exe's logic is described in the code | 0x005b0cf0 |
| `HeavyCorvetteHeader` 0x008c4998 | new slot 9 `HeavyCorvetteOrderChanged` (burstState = 0) | 0x005b5300 (Ship+0x640) |
| `HeavyCruiserHeader` 0x008c4ab0 | `HeavyCruiserSpec` = weapon block + depot (0x15C, asserted, moved to the .h); new Init, HouseKeep, RemoveShipReferences (11/12), Died, PreFix, Fix (bHeavyCruiser depot + weapon); slots 8/9/17/18 = Mothership's, 14 = Carrier's | 0x005b5390..0x005b5480 |
| `GravWellGeneratorHeader` 0x008c4868 | slot 12 | headers.txt |
| `MinelayerCorvetteHeader` 0x008c5040 | slot 9 = HW1's `MinelayerCorvetteOrderChangedCleanUp` body (the exe's `MinelayerCorvetteOrderChanged` is that code) | 0x005bb320 |
| `MothershipHeader` 0x008c53e8 | **Mothership.c rewritten to the exe's block.** `MothershipSpec` (0x1F0, asserted): weapon block, Beast-system state, big-gun target/position/shell/state/charge effect/aim timer, 7 module slots, death sequence, salvage depot (+0xAC). `MothershipStatics` = the two range arrays (HW1's special-door table and `scriptSetSpecialDoorOffsetCB` gone: no such code in the exe). Init (weapon, big-gun fields, `CatTimer`, sMothership module slots, bMothership systems, trGhostship animation, depot); Attack and the new Move/Halt commands (20/27) abort a big-gun shot; new SpecialActivate, SpecialTarget, OrderChanged, EffectRemoval (13), Save/Load; RemoveShipReferences (big-gun module loss), Fix, Died, PreFix; HouseKeep (death sequence explode/delete, Sect / Beast dispatch, depot, low-health warning at 25/20/15/10/5 %); `beastMothershipSelfDamage`; `cat_mothershipGetBigGun`, `cat_mothershipHasBigGunModule`, `cat_mothershipBigGunFiring`, `cat_mothershipStartDeathSequence`, `cat_mothershipModuleSlotGet`. CAT-PROVISIONAL: the big gun itself (`SectMothershipHouseKeep`'s aim/fire states and the `cat_bigGun*` block, with the sMothershipBigGun module ship, M2-R4 #4), `cat_blastCreate`, the info-overlay message, launch-manager module slots and the Beast-system switch-off. HW1 door helpers kept inert for their unported callers | 0x005bb730..0x005bd730 (MothershipInit 0x005bb840, Died 0x005bb9c0, RemoveShipReferences 0x005bba70, MoveCommand 0x005bbbd0, Attack 0x005bbc50, SpecialActivate 0x005bc050, SpecialTarget 0x005bc200, OrderChanged 0x005bc230, SectMothershipHouseKeep 0x005bc430, beastMothershipSelfDamage 0x005bc900, HouseKeep 0x005bcad0, Save/Load/PreFix/Fix 0x005bcd70..0x005bcdc0, HaltCommand 0x005bce00, EffectRemoval 0x005bce80) |
| `P1IonArrayFrigateHeader` 0x008c5808 | `P1IonArrayFrigateSpec` = cloak block + detected flags (0x38, asserted); new Init (cloak); slots 7/10 = the cloaked fighter's; Attack / AttackPassive: no attack while fully cloaked, decloak first except in single player | 0x005be200, 0x005be220, 0x005be280 |
| `P1MothershipHeader` 0x008c5968 | `P1MothershipSpec` = salvage depot (0x144, asserted); new Init, HouseKeep, RemoveShipReferences (11/12), Died, Fix (depot); slot 16 = Mothership's | 0x005be530..0x005be5c0 |
| `P1StandardCorvetteHeader` 0x008c5a88 | rewritten: a mimic + salvager. `P1StandardCorvetteSpec` (0x168, asserted; mimic fields named as far as Init sets them, salvager block at +0x13C); statics = salvage statics + side-step keys; StaticInit, Init, Attack, AttackPassive (= MCVAttackPassive), new SpecialActivate/SpecialTarget/OrderChanged/HouseKeep/RemoveShipReferences/Died/PreFix/Fix/HaltCommand (= MCVHaltCommand). The Mimic calls are CAT-PROVISIONAL stand-ins (Mimic code, M2-R4 #4) | 0x005be610..0x005be8d0, `MCVAttackPassive` 0x005b8210, `MimicHaltCommand` 0x005b99e0 |
| `ResourceCollectorHeader` 0x008c6378 | no slot 21 (`ResourceCollectorAttack` unused) | headers.txt |
| `SalCapCorvetteHeader` 0x008c6698 | rewritten to the exe's wrappers: StaticInit (Kushan/Taiidan/Traders blocks, side-step keys, salvage statics), Init (side-step + `cat_salvageInit`), SpecialTarget / new OrderChanged / HouseKeep / RemoveShipReferences (11/12) / Died / PreFix / Fix call Salvage.c. HW1's salvage bodies stay as dormant `hw1*` functions for their unported HW1 callers; their carrier/mothership drop-target and capture-queue blocks are cut (those specs no longer exist). `JunkYardDawgHeader` removed (trJunkYardDawg is on DoNothingShipHeader) | 0x005c59c0..0x005c5ae0 |
| `StandardDestroyerHeader` 0x008c6990 | `StandardDestroyerSpec` 0xD30 (asserted, mostly opaque); new slot 10 `StandardDestroyerHouseKeep`: sDestroyer missile regeneration (the gun's current WEAPON_* type is CAT-PROVISIONAL: the first weapon type stands in until the Gun has its type word) | 0x005c7800 |

**New and changed Game files (Cataclysm-only systems the reused headers call):**

- `Game/Salvage.c` / `.h` (new): the salvager block `CatSalvagerInfo` (0x28), the depot block `CatSalvageDepot`
  (0x144), `CatSalvageStatics` / `CatSalvagerStatics`; real: `cat_salvageStaticInit` 0x004f90e0 (the
  `salvageFlyToDistance` key and the "center" dock point), `cat_salvageInit` 0x004f9010, `cat_salvageDepotInit`
  0x004fc7c0, `cat_salvageGetSalvagerInfo` 0x004f8ec0 and the depot lookup (typed offsets per spec);
  CAT-PROVISIONAL (M2-R4 #3): `cat_salvageDetach`, `cat_salvageSpecialTarget` (order done at once),
  `cat_salvageHouseKeep`, `...OrderChanged`, `...RemoveShipReferences`, `...Died`, `..._PreFix`, `..._Fix` and the
  depot's HouseKeep/Died/RemoveShipReferences/PreFix/Fix.
- `Game/BeastWeapon.c` / `.h` (new): `CatBeastWeaponSpec` (0x18 on ILP32), the tweaks; real: the two predicates, the
  tweak loader, Init/Close/OrderChanged, RemoveShipReferences (target part), Fix (target part); CAT-PROVISIONAL
  (M2-R4 #3): HouseKeep, BulletRemoval, HasTarget (FALSE), Attack (done), Save, Load.
- `Game/Timer.c` / `.h`: `CatTimer` (0x10, asserted), `cat_timerStartGameTime` 0x00528410,
  `cat_timerStartRealTime` 0x00528440, `cat_timerExpired` 0x00528490 (the real-time sum `cat_taskRealTimeElapsed`
  0x00a6b20c is CAT-PROVISIONAL until Task.c advances it, M2-R4 #5).
- `Game/Gun.c` / `.h`: `cat_gunCanFire` 0x0048a1c0, `cat_gunGetBulletSpeed` 0x0048b8e0,
  `cat_gunFixedGunAimedAtTarget` 0x0048cd40. The exe's Gun has a current-WEAPON_* word at +0x00 (0x88 bytes; HW1
  0x84) that selects the second fire mode (`StandardDestroyerHouseKeep` tests it for WEAPON_MISSILE); this tree's Gun
  has none yet (M2-R4 #3), so the second-mode paths are CAT-PROVISIONAL and every gun is in its first mode, the
  exe's path for every gun that is not switched.
- `Game/ShipSelect.c`: `cat_shipSetDisableReason` 0x005b3fe0 (13 reasons, `catDisabled`).
- `Game/UnivUpdate.c`: `MakeExplosionRockBumpables` no longer static (the mothership death sequence calls it).
- `Game/SpaceObj.h`: `CAT_SHIPFLAG_Orphaned` (catShipFlags bit 2), `SPECIAL_CatSalvager` (exe specialFlags 0x80000;
  BIT27 here, not HW1's `SPECIAL_IsASalvager`, which makes HW1 code read HW1's SalCap spec),
  `SPECIAL_2_CatSalvageDepot` (the same bit as the selection item's `SPECIAL_2_CatBeingSalvaged`:
  `cat_salvageDepotInit` sets it on every depot, so the bit marks depots), `SPECIAL_2_CatBeastInfectionUnlimited`
  (exe bit 15), `SPECIAL_2_CatExempt20` (exe bit 20, meaning not pinned).
- `Game/StatScript.c` / `.h`: `scriptSetSpecialDoorOffsetCB` removed (no such function in the exe).
- **Diagnostic** `/catShipStaticsStaticInit` (`src/SDL/main.c`, `Universe.c`; use with `/catShipStaticsTest`): the
  self-test also runs every type's CustShipStaticInit. Run `sa-r3-statics-init`: rc 0, ASan 0, 143 loaded, **56
  StaticInits ran** (every type on a reused HW1 header), `RACE_BEAST\beastweapon.script` read once.

**Runs after `slots`** (plain harness, debug ASan): `sa-r3-sp`: same milestone and stop as `sa-r2-sp` (33 ships, 20
StaticInits, `LevelLoad.c (860)`), ASan 0; `sa-r3-statics`: rc 0, 143/143 ship types as the probe, 170/173 `.shp`
(engine check OK 140 + OK-DECLARED-COUNT 3, OTHER-LOADED 27); `sa-r3-fe`: `Main_game_screen`, ASan 0.
`tools/build.sh --debug` and `--release`: 0 warnings; `tools/m2_layout32.sh`: OK (24 ship files).

## 4. `bodies`: the MODIFIED / REWRITTEN slot bodies of Ships/*

Every Ships/* function the delta map lists as MODIFIED or REWRITTEN for a reused header was read against the exe and
ported (or, where the exe body calls a system another producer owns, ported with a documented CAT-PROVISIONAL hook).

| function (exe address) | Cataclysm delta ported |
|---|---|
| `GenericInterceptorCanFire` 0x005b2120 | no TargetDrone test and no trigger-happy edits (the exe never reads `triggerHappy`); a mimicking ship never fires; `cat_gunCanFire`; an Acolyte picks among the first half of its guns (`cat_shipIsAcolyte` 0x005ad300, sAcolyte/bAcolyte, new in ShipSelect.c); fixed guns aim with `cat_gunFixedGunAimedAtTarget` |
| `GenericInterceptorFire` 0x005b22c0 | an Acolyte alternates between its first two guns |
| `GenericInterceptorPassiveAttack` 0x005b2470 | the first gun's `cat_gunGetBulletSpeed`, only with a gun info |
| `GenericInterceptorGetFlightManProb` 0x005b25c0 | no DefenseFighter case |
| `GenericInterceptorAttack` 0x005b2620 (4,198 B, read state by state) | the bombing run only for kAttackBomber/tAttackBomber and never at a ship target; KILL uses `cat_gunGetBulletSpeed`; the break formation comes from the ship's own formation command (`command->ordertype.attributes & COMMAND_MASK_FORMATION`), no R1-mothership exception; no assert in `default`. Every other state (bombing run, approach, kill flight manoeuvres, break, turnaround/reapproach with whip strafe, whip strafe 1/2) is HW1's code |
| `CloakedFighter*` 0x005b36d0..0x005b3840 | see `interceptor` |
| `DefaultShipFire` 0x005b0260, `MiningBaseDied` 0x005b0370 | see `slots` |
| `doKamikazeAttack` 0x005b1a50 | tMinelayerCorvette only, no DefenseFighter case; a mimicking ship holds fire; new: inside 800 units `cat_kamikazeSteer` 0x005b16f0, inside 1 unit a box touch (blob test as the exe) is a hit at once via `cat_kamikazeCheckHit` 0x005b1990; both CAT-PROVISIONAL (they write physics/collision state: M2-R4 #3) |
| `univDFGFieldEffect` 0x005b0940 | a Sect big-gun shell is detonated by the field (`cat_isBigGunBullet` 0x005bcf80, CAT-PROVISIONAL FALSE until bullets carry a weapon type); no `BulletDamageMultiplier` any more; both coin flips use the exe's parity (`gamerand() & 1`; HW1's `randombetween(1,2) & 1` flipped on the other parity); CAT-PROVISIONAL: the effect damage's weapon-vs-armour factor (`cat_weaponDamageVsArmour` 0x00514460) |
| `GravWellGeneratorSpecialActivate` 0x005b4430 | Cataclysm's grav-on speech (0x0400089E) and sound (0x80E) |
| `turnoffGravwell` 0x005b42f0 | Cataclysm's gravwell-off sound (0x80F) |
| `GravWellGeneratorAddObj` 0x005b4570 | fleet-level stuck-in-gravwell chatter with no can-chatter test |
| `runShipEffect` 0x005b4130 | the effect frequency limit in every build |
| `MinelayerCorvetteFire` 0x005ba330 | `cat_gunCanFire` |
| `MineLayerAttackRun` 0x005ba6e0 | the forced mine drop at any mothership-class target; `cat_gunCanFire`; no assert in `default` |
| `P1MissileCorvetteSpecialTarget` 0x005be480 | fires no missile volley: once the volley time has passed it resets the target index and reports the order done |
| `ProximitySensorHouseKeep` 0x005bf580 | no detection battle chatter |
| `refuelRepairShips` 0x005c24e0 | a ship never repairs itself; the doubled distances for any mothership-class target; NIS ships are not healed; CAT-PROVISIONAL: the exe's Cataclysm dock type 0 (HW1's `DOCK_AT_SPECIFIC_SHIP` stays until Dock.c, M2-R4 #3) |
| Carrier / HeavyCruiser / Mothership / P1Mothership / P1StandardCorvette / SalCapCorvette / P1IonArrayFrigate bodies | see `slots` |

**IDENTICAL in the delta map, but the exe body differs** (ported, and recorded as review entries, section 5):
`GenericInterceptorStaticInit` (14 type blocks), `GravWellGeneratorDied` 0x005b41e0 (detaches and times out the field
effect), `GravAddObjectsInProximity` 0x005b46c0 (micro class and sWorker are caught, both SalCap types excluded),
`GravWellGeneratorHouseKeep` 0x005b47e0 (only an ownerless gravwell uses up its time; Cataclysm's collapse speech
0x0200089F), `gravwellIsShipStuckForHyperspaceing` 0x005b4260 (both gravwell types), `ModifyRepairEffect` 0x005c2410
(mothership class), `GenericDefenderStaticInit` 0x005b1420, `AdvanceSupportFrigateStaticInit` 0x005aef00,
`GenericInterceptorGetFlightManProb` 0x005b25c0 (no DefenseFighter case) [audit: this list named `FloatingCityStaticInit` 0x005b13e0 "(unchanged)", which the map keeps IDENTICAL (below), and left out `GenericInterceptorGetFlightManProb`, one of the 19 reclassified], `GravWellGeneratorStaticInit` 0x005b40a0, `IonCannonFrigateStaticInit`
0x005b6820, `MinelayerCorvetteStaticInit` 0x005ba260, `MissileDestroyerStaticInit` 0x005bb350,
`MultiGunCorvetteStaticInit` 0x005bdb30, `P1IonArrayFrigateStaticInit` 0x005be1a0, `P1MissileCorvetteStaticInit`
0x005be2e0, `ProximitySensorStaticInit` 0x005bf3d0, `ResourceCollectorStaticInit` 0x005c3d90,
`StandardFrigateStaticInit` 0x005c78e0 (the race/type re-keying of the statics blocks).

**Not ported, with reason:** `DefenseFighter.c` (10 functions in the exe, but no header in `shipHeaders` points at
them: the DefenseFighter type does not exist in Cataclysm, so the code is unreachable; kept dormant as HW1),
`RepairCorvette.c` apart from `refuelRepairShips` / `ModifyRepairEffect` (the same: no RepairCorvette type; its
`startRepairEffect`/`stopRepairEffect` are used by the repair beam and match the exe).

**HW1 aliases.** In the files this item ported, every HW1 type alias and dormant id in live code is gone except in
`SalCapCorvette.c`'s dormant `hw1*` salvage code (86 uses, deleted with HW1's salvage path by M2-R4 #3) and
`RepairCorvetteStaticInit` (dormant HW1 header). `/tmp/m2r3sa/aliasscan.py` lists them.

## 5. `final`: delta-map reviews, builds, plain-harness runs

### Delta-map review entries (`ghidra/delta_map.json`)

81 records reviewed against the r5 decompile and marked `reviewed: true` with the note "M2-R3 ships-a ...":

- **19 reclassified IDENTICAL -> MODIFIED** (the exe body differs; `change` says how, the note keeps the old class):
  `GenericInterceptorStaticInit` 0x005b1e70, `GenericInterceptorGetFlightManProb` 0x005b25c0,
  `GravWellGeneratorDied` 0x005b41e0, `GravAddObjectsInProximity` 0x005b46c0, `GravWellGeneratorHouseKeep` 0x005b47e0,
  `gravwellIsShipStuckForHyperspaceing` 0x005b4260, `ModifyRepairEffect` 0x005c2410, and the twelve StaticInits whose
  statics blocks the exe re-keys by Cataclysm race or type (`GenericDefender` 0x005b1420, `AdvanceSupportFrigate`
  0x005aef00, `GravWellGenerator` 0x005b40a0, `IonCannonFrigate` 0x005b6820, `MinelayerCorvette` 0x005ba260,
  `MissileDestroyer` 0x005bb350, `MultiGunCorvette` 0x005bdb30, `P1IonArrayFrigate` 0x005be1a0, `P1MissileCorvette`
  0x005be2e0, `ProximitySensor` 0x005bf3d0, `ResourceCollector` 0x005c3d90, `StandardFrigate` 0x005c78e0).
- **8 IDENTICAL confirmed** [audit: was "9"; the eight named here are the map's IDENTICAL -> IDENTICAL records]: `FloatingCityStaticInit`, `P1MothershipStaticInit`, `GenericInterceptorInit`,
  `setupWhipStrafe`, `MiningBaseInit`, and the three ICF-shared bodies `SwarmerAttack`, `MissileDestroyerAttack`,
  `P1MothershipAttackPassive` (their notes say which headers share them).
- **54 MODIFIED / REWRITTEN** [audit: was "53"; 43 MODIFIED + 11 REWRITTEN, so 19 + 8 + 54 = 81] records ported here, each with a one-line `change`.

Totals after the entries (`tools/m2_stage_counts.py`): IDENTICAL 3,118 / MODIFIED 1,226 / REWRITTEN 76 (was 3,137 /
1,207 / 76); reviewed 1,341 (was 1,270). Stage 2: 812 / 561 / 29.

### Builds

- `tools/build.sh --debug --clean`: rc 0, `[295/295] Linking target hwi-engine`, 0 `warning:` and 0 `error` lines
  under `-Werror`; 76,745,600 B.
- `tools/build.sh --release --clean`: rc 0, `[295/295]`, 0 warnings; 4,814,336 B, sha256 `abf759dd2c4123b1...`.
- `tools/m2_layout32.sh`: OK; the new ship pass: "24 files, every sizeofShipSpecifics assert holds".
- The changed and new files were also syntax-checked with clang and `-Werror=implicit-function-declaration` (the gcc
  build has `-Wno-implicit-function-declaration`): no implicit declarations (one found and fixed: `SaveGame.h` in
  BeastWeapon.c; `collCheckRectBumpCollision` is now declared in Collision.h).

### Plain-harness runs (debug ASan build, `original/scratch-m2/m2r2-harness/data`, offscreen, no input)

| run | mode | rc / wall | milestone | first stop | sanitizers / checks |
|---|---|---|---|---|---|
| `sa-r0-sp` (before) | sp | 1 / 10 s | level load (mission file read) | `Ships/GenericInterceptor.c (205)` after `sRecon.shp` | ASan 0 |
| `sa-r1-sp` | sp | 1 | level load | `Ships/GenericDefender.c (70)` after 26 ships | ASan 0 |
| `sa-r2-sp`, `sa-r3-sp` | sp | 1 | level load | `Game/LevelLoad.c (860)` | ASan 0 |
| **`sa-final-sp`** | sp | 1 / 10.8 s | **level load (mission file read)**; all 33 Mission01 ship statics loaded, 20 StaticInits ran (`staticinit=next`), 13 Cataclysm-only types on DefaultShipHeader (`none`) | **`Game/LevelLoad.c (860)`**: `GetAppropriateShipTypeForRace`'s `RacesAllowedForGivenShip` assertion, in level load after the ship statics | ASan 0; LeakSanitizer 44 B in 11 allocations at the fatal exit, the same as `sa-r0-sp` |
| **`sa-final-statics`** | statics | 0 / 10.9 s | ship statics self-test done | none | ASan 0, no leak; engine check: **143/143** ship-type files as the probe (OK 140 + OK-DECLARED-COUNT 3), OTHER-LOADED 27 (**170/173** `.shp`), 2 types without a file logged |
| **`sa-final-statics-init`** | statics + `/catShipStaticsStaticInit` | 0 / 10.9 s | self-test done | none | ASan 0, no leak; **56 StaticInits ran** (every type on a reused HW1 header, all 7 races), `beastweapon.script` read once |
| **`sa-final-fe`** | fe | 124 / 40.3 s (timeout) | `Main_game_screen` | none | ASan 0, no leak; 232 file loads (as `sa-r3-fe` and the selection item's final fe run) |
| `sa-final-oracle` | oracle | 0 / 9.3 s | oracle self-test done | none | ASan 0, no leak |

### DONE WHEN

| criterion | state | evidence |
|---|---|---|
| the sp plain-harness run passes `GenericInterceptor.c (205)` | **met** | `sa-final-sp`: sRecon's StaticInit and every later one run |
| ... and every later ship StaticInit for Mission01's ships | **met** | `sa-final-sp`: 33/33 Mission01 statics loaded, 20/20 StaticInits ran; the first stop is in level load after them (`LevelLoad.c (860)`, not ship code) |
| statics self-test stays at 143/143 types and 170/173 `.shp` | **met** | `sa-final-statics` + `shipstatic_engine_check.py` |
| fe unchanged | **met** | `sa-final-fe`: `Main_game_screen`, ASan 0, same file-load count |

## What is left, and its owner

1. **Next sp stop: `LevelLoad.c (860)`** (`GetAppropriateShipTypeForRace`). The exe's 0x004a13c0 swaps a type its race
   may not have through a 145-entry table (0x00883cf8) instead of HW1's CloakedFighter/DefenseFighter/DDD/DFG switch,
   and has no assertion. Level load, M2-R3 producer 5 ("Mission01 level load completes").
2. **Salvage runtime** (`Game/Salvage.c` provisional entries: special target, housekeep, detach, depot processing,
   save fix-ups) and deleting HW1's dormant salvage code in `SalCapCorvette.c` (the `hw1*` functions, 86 HW1 alias uses)
   with its HW1 readers in Dock.c, Collision.c, CommandLayer.c, AIShip.c, AIMoves.c, KASFunc.c, render.c: **M2-R4 #3**.
   Until then SalCapCorvetteHeader allocates max(exe 0x30, HW1) bytes of specifics.
3. **Beast infection weapon runtime** (`Game/BeastWeapon.c` provisional: housekeep, bullets, attack, has-target,
   save/load): **M2-R4 #3**.
4. **Gun weapon-type word** (exe Gun +0x00, 0x88 bytes): the second fire mode in `cat_gunCanFire` /
   `cat_gunGetBulletSpeed` / `cat_gunFixedGunAimedAtTarget`, `StandardDestroyerHouseKeep`'s WEAPON_MISSILE test, the
   bullet weapon type for `cat_isBigGunBullet` and `univDFGFieldEffect`'s weapon-vs-armour damage: **M2-R4 #3**.
5. **Sect mothership big gun** (`SectMothershipHouseKeep` aim/fire states, the `cat_bigGun*` block 0x005bb7e0..0x005bd6e0,
   with the sMothershipBigGun module ship), the Mimic code behind P1StandardCorvette's `p1scMimic*` stand-ins, Sentinel
   field disruption in `DFGFrigateHouseKeep`, the 22 Cataclysm-only headers: **M2-R4 #4**. Nothing asserts on the
   Cataclysm-only types on DefaultShipHeader (`sa-final-statics-init`: every StaticInit runs, DefaultShipHeader has
   none; `sa-final-sp`: the 13 such Mission01 types load).
6. **Kamikaze steering and immediate hit** (`cat_kamikazeSteer` 0x005b16f0, `cat_kamikazeCheckHit` 0x005b1990: they
   write physics/collision state), `cat_blastCreate`, the dock-type renumbering (`refuelRepairShips` passes HW1's
   `DOCK_AT_SPECIFIC_SHIP` where the exe passes Cataclysm type 0), `MiningBaseDied`'s list removals (kept until
   `univDeleteDeadShip` is ported), the mothership death sequence's trigger (`AddShipToDeleteShipList` calls
   `cat_mothershipStartDeathSequence` in the exe): **M2-R4 #1 / #3**.
7. **specialFlags / specialFlags2 renumbering** (`SPECIAL_CatSalvager`, `SPECIAL_2_CatBeastInfectionUnlimited`,
   `SPECIAL_2_CatExempt20`, the exe's 0x6 kamikaze bits set by mimicking P1 corvettes): **M2-R4 #2**.
8. **sCarrier door animation** (needs producer 5's `DockStaticInfo` 0x50 header and madAnimationStart's loop argument,
   which trGhostship's init also passes): producer 5 (dock statics) and stage 3 (MeshAnim.c).
9. **Speech / UI hooks**: `cat_speechAlertsNoteShip` (stealth-spotted alert flags), the mothership low-health info
   overlay message (`cat_ioMessageAdd`), the Cataclysm speech and sound event ids used here as literals: **stage 3**.
10. `cat_taskRealTimeElapsed` (real-time CatTimers) advanced by Task.c: **M2-R4 #5**. Launch-manager module slots and
    Beast-system switch-off (`cat_lmModuleSlotsRemoveType`, `cat_lmBeastSystemsOff`): **M2-R4 #3**.
11. `DefenseFighter.c` (10 exe functions, unreachable in Cataclysm) and `RepairCorvetteStaticInit` (dormant HW1 header)
    keep their HW1 bodies; they can be deleted with the 22 dormant HW1 types.

## Counts

- Tracked files changed: 54 (+2,293 / -1,314 lines), 386 added lines carrying `CAT`; new files: `Game/Cloak.c/.h`,
  `Game/Salvage.c/.h`, `Game/BeastWeapon.c/.h` (855 lines, 88 `CAT` lines), this report.
- Headers: 29 reused HW1 headers at the exe's contents (TargetDroneHeader and JunkYardDawgHeader removed).
- StaticInits: 21 files (14 interceptor blocks, 2 defender type blocks, race blocks
  elsewhere); 56 StaticInits exercised under ASan.
- Ships/* HW1-matched functions of the scope: 80 IDENTICAL / 45 MODIFIED / 12 REWRITTEN in the map before this item;
  every MODIFIED/REWRITTEN one of the 29 reused headers ported (DefenseFighter's 3 and RepairCorvette's dormant ones
  excepted, see 11); 19 IDENTICAL ones found modified and ported; 97 exe functions outside the HW1 match (cat-new)
  ported or given a documented hook: Cloak 6, Salvage 18, BeastWeapon 14, Mothership 16, Carrier 3, HeavyCruiser 6,
  P1Mothership 5, P1StandardCorvette 10, CloakedFighter 2, Gun 3, Timer 3, ShipSelect 3, kamikaze 2, and one slot
  body each in AdvanceSupportFrigate, DFGFrigate, HeavyCorvette, StandardDestroyer, P1IonArrayFrigate, SalCapCorvette.
- Harness runs: 11 (`sa-r0..r3-*`, `sa-final-*`, all plain).
