# Audit: M2-R3 port "ships-a" (the HW1 ship headers Cataclysm reuses, their StaticInits and slot bodies)

Auditor: adversarial audit, 2026-10-10.

- **Subject:** producer report `docs/m2-r3-ships-a.md` and the uncommitted diff in the `m2` worktree (54 tracked
  files, +2,293 / -1,314; new `src/Game/Cloak.c/.h`, `Salvage.c/.h`, `BeastWeapon.c/.h`).
- **Not run:** Ghidra and the GOG game. No input was injected.
- **Evidence:** the r5 export (`ghidra/exports/cataclysm_gog_r5/decompiled.c`, `functions.csv`, `strings.csv`),
  `original/scratch-m2/m2r2-shipstatic-tools/headers.txt` (the exe's 51 headers), `original/game/cataclysm.exe` (floats
  and key tables read through its PE section table), the tree's headers for every constant (`Ships/ShipDefs.h`,
  `Game/RaceDefs.h`, `ClassDefs.h`, `CommandDefs.h`, `FormationDefs.h`, `ObjTypes.h`, `SpaceObj.h`, `Randy.h`), the
  plain harness.
- **Audit helpers** (untracked, `/tmp/audsa/`): `fn.py` prints r5 bodies by address; `pe.py` reads exe floats, dwords
  and `scriptStructEntry` tables (name, callback, field offset, template); `hdr.py` compares all 34 slots and the size
  of every tree `CustShipHeader` with `headers.txt`; `hunks.py` lists diff hunks with no `CAT` tag in reach;
  `changedfns.py` lists every function whose body the diff changes with its delta-map class before/after and review
  state.

## 1. Clean builds (separate dir, as `tools/build.sh` does)

`original/scratch-m2/build-audit-ships-a/` was removed and recreated, then
`meson setup --buildtype=debug -Db_sanitize=address,undefined debug` / `meson compile`, and
`meson setup -Doptimization=2 -Ddebug=false -Db_sanitize=none release` / `meson compile` (`werror` from `meson.build`).

| build | rc | `warning:` lines | link | size |
|---|---|---|---|---|
| debug (audit dir, producer's tree) | 0 | 0 | `[295/295] Linking target hwi-engine` | 76,755,568 B |
| release (audit dir, producer's tree) | 0 | 0 | `[295/295] Linking target hwi-engine` | 4,814,336 B (= producer) |
| debug (audit dir, after the audit fixes) | 0 | 0 | `[207/207]` relinked | 76,756,104 B |
| release (audit dir, after the audit fixes) | 0 | 0 | `[207/207]` relinked | 4,814,336 B |
| `tools/build.sh --debug` (after the fixes) | 0 | 0 | `[207/207]` | 76,746,136 B |
| `tools/build.sh --release` (after the fixes) | 0 | 0 | `[207/207]` | 4,814,336 B |

The debug size differs from the producer's 76,745,600 B because ASan builds embed the build path; the release size
is the producer's to the byte. `tools/m2_layout32.sh`: OK, "24 files, every sizeofShipSpecifics assert holds" (before
and after the fixes).

## 2. Slot table and StaticInit / slot bodies against the exe

### 2.1 Slot table (`hdr.py`)

All 29 tree headers that the exe also has were compared slot by slot (29 x 34 = 986 slot entries) and by size:

- **Sizes:** every header's `sizeofShipSpecifics` is pinned by an `__i386__` `_Static_assert` equal to `headers.txt`
  (ASF 4, AttackBomber 0x38, Carrier 0x168, CloakedFighter 0x50, DFG 4, Default/DoNothing/StationaryGun 4,
  FloatingCity 4, GenericDefender 4, GenericInterceptor 0x18, GravWell 40, HeavyCorvette 44, HeavyCruiser 0x15C,
  IonCannon 4, Minelayer 136, MiningBase 4, MissileDestroyer 20, Mothership 0x1F0, MultiGun 8, P1IonArray 0x38,
  P1MissileCorvette 24, P1Mothership 0x144, P1StandardCorvette 0x168, ProximitySensor 16, ResourceCollector 4,
  StandardDestroyer 0xD30, StandardFrigate 4; SalCap allocates max(exe 0x30, HW1) as reported). 29/29.
- **Slots:** 38 name differences; none is a wrong slot.
  - 7 are parser artefacts of the slot-12 casts: `grep` shows Carrier, HeavyCruiser, Mothership, GravWell, P1Mothership,
    P1StandardCorvette and SalCap all set slot 12 to their slot-11 function, as the exe does.
  - 31 are ICF folds or equivalent bodies, each read against the exe: 13 tree `*AttackPassive` bodies are the
    `rotate & rotateToRetaliate ? attackPassiveRotate : attackPassive` of `P1MothershipAttackPassive` 0x005be540;
    8 `*Attack` bodies are `attackStraightForward(gunRange[t], tooCloseRange[t])` (`MissileDestroyerAttack` 0x005bb3e0)
    and every one of those statics starts with `GunRange[3]`, `TooCloseRange[3]` (offsets 0 / 0xC as the exe reads);
    HeavyCorvette/MultiGun `attackSideStep(&spec->attacksidestep, &stat->sidestepParameters)` = `SwarmerAttack`
    0x005c79b0 (both at offset 0); `MothershipAttackPassive` = bare `attackPassive` (`RKOAttackPassive` 0x005c59a0);
    `GenericInterceptorSpecialActivate` returns TRUE (`kasfSpecialOff` 0x00497db0 `return 1`);
    `CarrierSpecialActivate` returns FALSE (`nullsubReturnFalse` 0x006fe9a0); `ResourceCollectorInit` clears spec +0
    (`MiningBaseInit` 0x005b0360); `MinelayerCorvetteOrderChangedCleanUp` = `MinelayerCorvetteOrderChanged` 0x005bb320
    (state 99 = FIRST_OFF, form info FULL and NULL); `P1StandardCorvetteAttackPassive` / `HaltCommand` =
    `MCVAttackPassive` 0x005b8210 / `MimicHaltCommand` 0x005b99e0 (mimicMode at Ship+0x760, frees +0x710 / +0x73C, the
    type-5-formation flag write and `cat_univDeleteMimicResource` left CAT-PROVISIONAL as reported).

Result: 29/29 headers correct.

### 2.2 StaticInit bodies (16 compared with r5)

Race/type ids and float bit patterns decoded against `RaceDefs.h` / `ShipDefs.h`; blocks by exe address, key tables
dumped with `pe.py`; constants 0x0072a5b4 = 0.9, 0x0072bfb0 = 0.725, 0x0072a4c8 = 0.5.

| function (exe) | result |
|---|---|
| `GenericInterceptorStaticInit` 0x005b1e70 | the 14 cases 0,1,2,0x21-0x25,0x3D,0x44,0x4B,0x52,0x5D,0x85 = the 14 tree types; 0xD90-dword clear, squares and per-class fallback as HW1; no TargetDrone tail. OK |
| `GenericDefenderStaticInit` 0x005b1420 | type 67 / 81 blocks; ranges x0.9; CIRCLE_RIGHT 20 / 0.1 before the table (keys at +0x18/+0x1C). OK |
| `AdvanceSupportFrigateStaticInit` 0x005aef00 | race 2 / 3; table `repairApproachDistance` +0x18. OK |
| `HeavyCruiserStaticInit` 0x005b5310 | race 1/2/3; no key table; Beast tweaks via `cat_staticHasBeastWeapon`. OK |
| `IonCannonFrigateStaticInit` 0x005b6820 | race 2/3/6; x0.725 / x0.5. OK |
| `MissileDestroyerStaticInit` 0x005bb350 | Taiidan else other; table before ranges; keys +0x18..+0x20. OK |
| `StandardDestroyerStaticInit` 0x005c7780 | Sect block reads table 0x008c6950 (+0x18..+0x20) first; Kushan / Taiidan. OK |
| `ResourceCollectorStaticInit` 0x005c3d90 | Kushan else Taiidan; key at +0. OK |
| `P1MissileCorvetteStaticInit` 0x005be2e0 | P1 else other; side-step table at +0x24, missile keys +0x18..+0x20, no memset. OK |
| `CarrierStaticInit` 0x005aefb0 | statinfo +0x211/+0x212/+0x213 = 0/1/0, +0x211 = 1 for Beast; 5 race blocks; table +0x18; Beast tweaks. OK |
| `HeavyCorvetteStaticInit` 0x005b4e10 | Kushan else other; side-step table; no memset. OK |
| `MinelayerCorvetteStaticInit` 0x005ba260 | Taiidan else other; both tables; the three squares. OK |
| `MultiGunCorvetteStaticInit` 0x005bdb30 | race 1/2/3. OK |
| `ProximitySensorStaticInit` 0x005bf3d0 | Kushan else other; two squares. OK |
| `StandardFrigateStaticInit` 0x005c78e0 | Kushan else other; ranges. OK |
| `MothershipStaticInit` 0x005bb730 | one block, ranges only, Beast tweaks. OK |

16/16 correct.

### 2.3 Slot and helper bodies (36 compared with r5)

| function(s) (exe) | result |
|---|---|
| `CloakedFighterInit` 0x005b36f0, `SpecialActivate` 0x005b3720, `Attack` 0x005b3730, `AttackPassive` 0x005b3770, `Fire` 0x005b37b0, `HouseKeep` 0x005b3840 | activeGun (spec +0x16) 0, cloak at Ship+0x650 with 0.1/0.1; the visible/decloak split; Fire = interceptor fire incl. the Acolyte wrap. 6 OK |
| `cat_cloakInit` 0x00435f10, `cat_cloakActivate` 0x00435f50, `cat_shipIsCloakedFully` 0x00436110, `cat_cloakUpdate` 0x004361c0 | flags 0x200/0x400 = `SOF_CatCloakDevice`/`SOF_Cloaked`; orders 2/4 = ATTACK/LAUNCH_SHIP; 1000 / 100 / 0.05 / 1.0 (0x0072a4c4); fade delay `1 - e^(r * 9.3132e-8 * -0.005)` on stream 1 (`RANDOM_CAT_SHIPS`); detected flags cleared at 3->1, set at 2->0; `shipHasJustCloaked`. 4 OK |
| `P1IonArrayFrigateInit` 0x005be200, `Attack` 0x005be220, `AttackPassive` 0x005be280 | cloak at spec +0; `singlePlayerGame` rule. 3 OK |
| `P1MissileCorvetteSpecialTarget` 0x005be480 | +0x10 / +0x0C / statics +0x1C / +0x14 reset. OK |
| `DefaultShipFire` 0x005b0260 | `cat_gunCanFire`; types 0-2 shoot, 3-4 missiles when loaded. OK |
| `cat_gunCanFire` 0x0048a1c0, `cat_gunFixedGunAimedAtTarget` 0x0048cd40 | dissolving test (Ship+0x570); first-mode path; muzzle offset, heading column 3, 0.75 x collspheresize (+0x4C). 2 OK |
| `cat_gunGetBulletSpeed` 0x0048b8e0 | **wrong in the second-mode branch** (finding F1). 1 wrong |
| `cat_timerStartGameTime` 0x00528410, `StartRealTime` 0x00528440, `Expired` 0x00528490 | 3 OK (but see F4 for the header comment) |
| `cat_shipIsAcolyte` 0x005ad300, `cat_shipSetDisableReason` 0x005b3fe0, `cat_shipStealthDetectedSet` 0x0053fad0 | types 1/0x22; 13 reasons at Ship+0x1DC, flag +0x210; cloaking-type branches (+0x668, +0x650). 3 OK |
| `cat_salvageInit` 0x004f9010, `cat_salvageGetSalvagerInfo` 0x004f8ec0 | bit 0x80000; +0x644/+0x640/+0x774, busy byte +0x24. 2 OK |
| `MothershipInit` 0x005bb840, `MothershipEffectRemoval` 0x005bce80 | every write at spec +0x20..+0x54 and the 7 module ids (24,23,21,22,19,26,31); bMothership +0x18/+0x1C then depot; 2.5 (0x0072a5b0) in the provisional note; effect at +0x48. 2 OK |
| `CarrierDied` 0x005af3f0 | weapon close, depot died, CLASS_Component (10) clamped ships flagged. OK (see F6) |
| `GravWellGeneratorHouseKeep` 0x005b47e0 | time used only when Ship+0x32C (`playerowner`, docs/hw1-engine-gameplay.md) is NULL; speech 0x0200089F. OK |
| `P1StandardCorvetteAttackPassive`, `P1StandardCorvetteHaltCommand` | as 2.1. 2 OK |
| `cat_shipHasBeastWeapon` 0x00429bc0 / `cat_staticHasBeastWeapon` 0x00429cf0, `cat_beastWeaponStaticInit` 0x00429b90 | types 0x2E/0x2F/0x35/0x82; table 0x00751698 = 6 `scriptSetReal32CB` keys at +0x04..+0x18 of 0x00c52140. 2 OK |
| `GenericInterceptorAttack` 0x005b2620 (STATE_INIT, break formation) | only types 0x3D/0x4B, `targetIndex > 4` or a ship target skips the run; formation = command +0x230 when attributes & 1 and tactics != Evasive, else 7 (NO_FORMATION). OK |
| `GenericInterceptorCanFire` / `Fire` (Acolyte) | numGuns/2 for Acolytes, wrap at 2. OK |

35 OK, 1 wrong.

### 2.4 Harness runs (plain harness, audit debug build, offscreen, no input)

| run | mode | rc / wall | milestone | first stop | checks |
|---|---|---|---|---|---|
| `audit-sa-sp` | sp | 1 / 10.7 s | level load (mission file read) | `Game/LevelLoad.c (860)` `RacesAllowedForGivenShip` | ASan 0; 33 statics loaded; 20 `staticinit=next`, 13 `none` (sAcolyte, sACV, sRammingFrigate, sWorker, sRepairDroid, sProcessor, 7 sMothership modules: all Cataclysm-only headers); LSan 44 B / 11 allocs as the producer's |
| `audit-sa-statics` | statics | 0 / 10.6 s | ship statics self-test done | none | ASan 0, no leak; `shipstatic_engine_check.py`: 173 .shp: OK 140, OK-DECLARED-COUNT 3, OTHER-LOADED 27, OTHER-NOT-LOADED 3 -> **143/143 types, 170/173 .shp** |
| `audit-sa-statics-init` | statics + `/catShipStaticsStaticInit` | 0 | self-test done | none | ASan 0, no leak; **56** `staticinit=next` (= the 56 types of `headers.txt` on the 25 reused headers with a slot 1), `beastweapon.script` loaded once |
| `audit-sa-fe` | fe | 124 / 40.4 s (timeout) | `Main_game_screen` | none | ASan 0, no leak, 232 file loads (as the producer's `sa-final-fe`) |
| `audit-sa-sp2`, `audit-sa-statics2` | sp, statics (after the fixes) | 1 / 0 | as above | as above | identical counts |

The producer's furthest point is confirmed: sp passes `GenericInterceptor.c (205)` and all 20 StaticInits of the
Mission01 ships whose header has one, and stops in level load.

## 3. Full diff review

- **Outside the producer's scope:** `src/SDL/main.c` and `Universe.c/.h` add the `/catShipStaticsStaticInit` port
  diagnostic (CAT-tagged, used for the 56-StaticInit run); `UnivUpdate.c/.h` un-statics `MakeExplosionRockBumpables`
  for the mothership death sequence; `Collision.h` declares `collCheckRectBumpCollision`; `StatScript.c/.h` drop
  `scriptSetSpecialDoorOffsetCB`. All serve the ported headers; none is a finding.
- **CAT tags (`hunks.py`):** 22 hunks have no `CAT` within 6 lines. All are includes, meson entries, header
  prototypes, the `hw1*` renames of HW1's dormant salvage code, the `trMiningBase` alias retirement, or lines inside a
  function whose body-level `CAT:` comment lists them (`GenericInterceptorCanFire`, `MinelayerCorvetteFire`). No
  untagged Cataclysm change.
- **IDENTICAL edited without review (`changedfns.py`):** 86 functions changed in place. Every one the map had as
  IDENTICAL (19) now carries a ships-a review entry and is MODIFIED; the 13 with no map record are HW1-only or
  ICF-folded (`GenericInterceptorSpecialActivate`, Mothership door helpers, `P1StandardCorvetteAttackPassive`,
  `proxInFocusSelection`, SalCap `hw1*` helpers, `cat_shipDetectedByPlayer`). No IDENTICAL function edited without a
  review entry.
- **Delta map:** `tools/m2_stage_counts.py` totals 3,118 / 1,226 / 76 and stage 2 812 / 561 / 29, as reported; reviewed
  1,341 (was 1,270); 81 records carry the ships-a note: 19 IDENTICAL->MODIFIED, 8 IDENTICAL confirmed, 43
  MODIFIED, 11 REWRITTEN.

## 4. Findings

| # | severity | finding | evidence | action |
|---|---|---|---|---|
| F1 | low (code, path unreachable today) | `cat_gunGetBulletSpeed` in the second fire mode returned `bulletspeed2 > 0 ? bulletspeed2 : bulletspeed`; the exe returns `bulletspeed` only when `bulletspeed2 <= 0 && bulletspeed > 0`, otherwise `bulletspeed2` (differs when both are <= 0) | r5 0x0048b8e0; the inline copy in 0x0048cd40 | **fixed** in `Gun.c` (marked `[audit: ...]`) |
| F2 | doc | report section 5: "9 IDENTICAL confirmed" is 8 (it names 8) and "53 MODIFIED / REWRITTEN" is 54 (43 + 11) | `delta_map.json` vs HEAD | **fixed** in the report |
| F3 | doc | report section 4's "IDENTICAL in the delta map, but the exe body differs" list named `FloatingCityStaticInit` "(unchanged)", which the map keeps IDENTICAL, and left out `GenericInterceptorGetFlightManProb`, one of the 19 | same | **fixed** in the report |
| F4 | comment | `Timer.h`: `CatTimer.startTime` said a real-time timer starts from `universe.realtimeelapsed`; the exe (and `Timer.c`) use `cat_taskRealTimeElapsed` | r5 0x00528440 | **fixed** |
| F5 | comment | `SpaceObj.h` `CustShipHeader` comment still said the new slots are NULL in every header and that no code calls them; this port fills them in 29 headers and `ETG.c` calls slot 13 | `hdr.py`; `ETG.c:6821` | **fixed** |
| F6 | comment | `proxInFocusSelection`'s new `CAT:` comment presented the two-type test as Cataclysm behaviour; the exe has no `proxInFocusSelection` (the map's `ccFocusGeneral` 0x0042deb0 record: "drops calls to proxInFocusSelection (absent from Cataclysm)") | `delta_map.json` | **fixed** (comment now says it is an alias retirement in dead-in-Cataclysm code) |
| F7 | latent, not fixed | `CarrierDied` and two Mothership.c helpers (like the selection port's `ShipSelect.c` uses) read `ClampInfo.clampedObjs` as a `SelectCommand` (`ShipPtr` at +4 on ILP32, +8 on LP64), while `SpaceObj.h` documents the exe's list as count at +0, objects from +8. Harmless now (`clampedObjs` is always NULL until clampObjToObj's list is ported, M2-R4); whoever ports the list must pick one layout for all readers | `SpaceObj.h:362-372`, r5 0x005af3f0 (`iVar4 = 8`) | noted for M2-R4 |
| F8 | method note, not counted | several bodies drop HW1 `dbgAssertOrIgnore` lines "as the exe" (`GenericInterceptorCanFire`/`Fire`/`Attack` default, `MineLayerAttackRun`); the retail exe has no `dbgAssert` at all (strings.csv has no source-file assert strings), so it is no evidence for removing a debug check. No behaviour change in release | `strings.csv` | none |
| F9 | low, documented | HW1 readers gated by `shiptype == SalCapCorvette` (`Dock.c:2333`, `CommandLayer.c:3887`, `SoundEvent.c:1503`) read HW1's `SalCapCorvetteSpec` over memory now laid out as the exe's 0x30-byte spec; the spec is zeroed at creation (`UnivUpdate.c:1365`) and the Cataclysm init writes only the busy byte (+0x2C), so they read NULL / 0. The producer's "memory only" note covers it; M2-R4 #3 removes the readers | as cited | none |

## 5. Precision

Sampled checks: builds and layout 3; header slot/size checks 29; StaticInit bodies 16; slot/helper bodies 36; report
claims 13 (map totals, 81 records, 19 reclassified, 9 confirmed, 53 MOD/REW, section-4 list, reviewed counts, sp,
statics 143/170, statics-init 56, fe, CAT coverage, no unreviewed IDENTICAL edit); code comments with a factual claim
10 (Cloak.h offsets, Mothership.h layout, Salvage.h layout, Gun.c second-mode note, CarrierStaticInit, StatScript,
UnivUpdate.h, Timer.h, SpaceObj.h slot comment, ProximitySensor). Total **107**; wrong **7** (F1-F6, counting F2 as
two claims). Precision = 100 / 107 = **0.935**.

## 6. Fixes and state

Fixes in place: **6** (F1 `Gun.c`; F2 and F3 in `docs/m2-r3-ships-a.md` marked `[audit: ...]`; F4 `Timer.h`; F5
`SpaceObj.h`; F6 `ProximitySensor.c`). After them the audit-dir debug and release builds relink with 0 warnings,
`tools/m2_layout32.sh` is OK and the plain-harness sp / statics runs are unchanged (section 2.4).
`tools/build.sh --debug` and `--release` after the fixes: rc 0 and 0, `[207/207] Linking target hwi-engine` each,
0 `warning:` lines; `build/debug/hwi-engine` 76,746,136 B, `build/release/hwi-engine` 4,814,336 B.

Verdict: PASS-WITH-FIXES - all 29 reused headers match the exe slot for slot and by size, 16/16 StaticInits and 35/36 slot bodies match r5, sp passes GenericInterceptor.c (205) and all 20 Mission01 StaticInits (stop in LevelLoad.c 860), statics 143/143 and 170/173, 56 StaticInits under ASan, fe unchanged; fixed cat_gunGetBulletSpeed's second-mode fallback, three report count/list errors and three wrong comments
