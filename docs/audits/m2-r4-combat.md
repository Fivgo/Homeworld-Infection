# Audit: M2-R4 port "combat" (salvage, gun weapon word, Beast weapon, collision/kamikaze/blasts, docking/resources)

Auditor: adversarial audit, 2026-10-11 (fresh session after the reboot; the WIP snapshot `b39dea7` plus the
resumed producer's working tree).

- **Subject:** producer report `docs/m2-r4-combat.md` and `git diff d47a23f` in the `m2` worktree: the WIP commit
  `b39dea7` (unaudited) plus the uncommitted working tree, 77 tracked `src/` files and the two `ghidra/` files
  (+6,935 / -5,441 lines with `cycle/`), and the new untracked `src/Game/Blast.c` / `Blast.h`. `cycle/STATE.md` and
  `cycle/workflows/m2-r4.js` in that range are the orchestrator's relaunch commit `6780eb6`, not this port.
- **Not run:** Ghidra and the GOG game. No input was injected.
- **Evidence:** the r6 export (`ghidra/exports/cataclysm_gog_r6/decompiled.c`, `functions.csv`, `symbols.csv`,
  `strings.csv`), the R2 `objdump -d -M intel` listing of `original/game/cataclysm.exe`
  (`original/scratch-m2/m2r2-mesh-tools/exe.dis`), exe data read by VA through the PE section table, the tree, the
  plain harness.
- **Audit helpers** (untracked, `/tmp/audit-combat/`, written for this audit, independent of the producer's tools):
  `fd.py` (one exe function's disassembly from the `functions.csv` range), `dec.py` (one r6 decompiled body),
  `exe.py` (dwords, floats, doubles, C strings by VA, on top of R2's `pe.py`), `pv.sh` (enum/macro values from the
  tree's headers, LP64), `off32.sh` (ILP32 `offsetof` / `sizeof` from the tree's headers via `clang -m32`
  diagnostics), `editedfuncs.py` (every function the diff touches with its delta-map class and review state),
  `hunks.py` (diff hunks with no `CAT` marker in or above them).

## 1. Clean builds (separate dir, as `tools/build.sh` does)

`original/scratch-m2/build-audit-combat/` was removed and recreated, then
`meson setup --buildtype=debug -Db_sanitize=address,undefined debug` / `meson compile` and
`meson setup -Doptimization=2 -Ddebug=false -Db_sanitize=none release` / `meson compile`. `meson-info`:
`werror=true`, `warning_level=3`, optimization 0 / 2, sanitizers address,undefined / none.

| build | rc | compiler `warning` lines | size |
|---|---|---|---|
| debug (audit dir, producer's tree) | 0 | 0 | 77,958,008 B |
| release (audit dir, producer's tree) | 0 | 0 | 4,866,352 B |
| debug (audit dir, after the audit fixes, incremental) | 0 | 0 | 77,958,832 B |
| release (audit dir, after the audit fixes, incremental) | 0 | 0 | 4,866,352 B |

The only "WARNING" line is meson's sanitizer notice at setup. After the fixes `tools/build.sh --debug` and
`--release` are rc 0 with 0 compiler warnings, and `tools/m2_layout32.sh` says "ILP32 ship specifics: 24 files, every
sizeofShipSpecifics assert holds" / "m2_layout32: OK".

## 2. Plain-harness runs

| run | binary | mode | result | univUpdateCounter | ASan / leaks |
|---|---|---|---|---:|---|
| `r4cb-audit-sp` | audit dir, producer's tree | sp, `--timeout 340` | universe running; still running at the timeout (rc 124); 3,717 file loads | **1,248** (R3 and `r4c-final-sp`, also 340 s: 1,248) | **0 / none** |
| `r4cb-audit-statics` | audit dir, producer's tree | statics | "self-test done, 143 of 145 ship types loaded", 2 not found, 27 other .shp (as `r4c-final-statics`); rc 0 | | 0 / none |
| `r4cb-audit-fixed-sp` | audit dir, after fixes 1, 2, 4-6 | sp, `--timeout 340` | universe running; still running at the timeout (rc 124); 3,717 file loads | 1,248 | 0 / none |
| `r4cb-audit-fixed2-sp` | audit dir, after all fixes (with fix 3) | sp, `--timeout 340` | universe running; still running at the timeout (rc 124); 3,717 file loads; same UBSan kinds | **1,248** | **0 / none** |

UBSan (non-fatal) in `r4cb-audit-sp`: the known misaligned-member kinds only (`fescreen`, `etgdecision`, `PATCH`,
`etgfunctioncall`, `mhlocalbinding`, `etgconditional`, `etgbranch`, `fquant.c`), no new kind. The counter is the
`debugScreenshots univUpdateCounter` line, printed every 112 ticks; what it reaches at the timeout depends on how long
the real-time animatic took and on the timeout (finding 4).

## 3. Checks (46)

"OK" = the port or claim matches the evidence; "WRONG" = a factual error in code, comment, report or delta map.
Exe offsets are ILP32. The tree's `Ship` and `Bullet` are not exe-laid-out (fields carry "exe Ship+0x..." comments),
so Ship-field checks compare the field the exe touches with the field the port names; `Gun`, `GunStatic`,
`ShipStaticInfo`, `DockStaticInfo`, `ShipsInsideMe`, `ClampInfo`, `CatSalCapCorvetteSpec`, `CatSalvagerInfo`,
`CatBlast` were checked by `off32.sh` against the offsets in the disassembly.

### Builds and runs (5)

| # | check | evidence | result |
|---|---|---|---|
| 1 | clean debug build, 0 warnings, -Werror | section 1 | OK |
| 2 | clean release build, 0 warnings, -Werror | section 1 | OK |
| 3 | ILP32 layout asserts (`Gun` 0x88, `CatSalCapCorvetteSpec` 0x30, `CatBlast` 0x64, `CatBeastHit` 0x1C, `AsteroidStaticInfo` 0xC8) | `tools/m2_layout32.sh` OK; `off32.sh`: `sizeof(CatSalCapCorvetteSpec)` 0x30, `salvager` at 0x08, `sizeof(CatBlast)` 0x64 | OK |
| 4 | DONE WHEN: sp runs as far as before with ASan 0 | `r4cb-audit-sp`: universe running, 1,248 ticks at 340 s (= `r4c-final-sp`), ASan 0, no leak | OK |
| 5 | statics self-test | `r4cb-audit-statics`: 143 of 145, rc 0, ASan 0 | OK |

### Bodies compared with the exe (30)

| # | function | evidence (exe) | result |
|---|---|---|---|
| 6 | `cat_salvageDepotCanAccept` 0x004f7c20 | specialFlags2 0x2000 / 0x8, kCarrier 0x3E in sp, Beast 0x10000 upgrade in mp unless crystal; outer byte table 0x004f7d34 (0x6F,0x7A,0x7B inner; 0x7E,0x7F upgrade 0x100000), inner table 0x004f7d58 `0,3x10,1,1`; `((depot==0x12)-1 & 0xB)+0x12`; `numOfType` at DockStaticInfo+0x23 `> 0`. Enum values printed: dBigGunUnit 0x6F, DPT_Destroyer_EB 0x12, DPT_SalvageProcess 0x1D, DPT_Med_In 6 | OK (the producer's fix of the WIP's 21/34 is right) |
| 7 | `cat_salvageInit` 0x004f9010 | sf \|= 0x80000, `cat_salvageDetach`, byte 0 at Ship+0x668 (sWorker/bWorker: block +0x0C, +0x24), +0x664 (SalCaps: +0x08, +0x24), +0x798 (p1StandardCorvette: +0x13C, +0x24), default 0x00a57794 | OK |
| 8 | `cat_salvageTargetDamagedEnough` 0x004f7d70 | Mission 8 skill < 2 TRUE, crystal, 0x7A/0x7B/0x6F TRUE, Mission 8 skill 2 limit 0.75, else `[class*4+0x00ba2c00]`; `health <= limit*maxhealth` | OK |
| 9 | `clampObjToObj` 0x004346e0 | keeps an existing ClampInfo (0x38), host's ClampInfo with host NULL, list memAlloc 0xFA8, count at +0, objects from +8 | OK |
| 10 | `unClampObj` 0x00434830 | host's list: remove, free at 0, host ClampInfo freed and 0x1000000 cleared when it has no host; own list freed at 0; own ClampInfo freed when both are NULL | OK |
| 11 | `cat_clampUnclampAllFromShip` 0x00434900 | the exe tests the list only on entry, then loops while the ClampInfo exists (0x434922..0x43492d); the port also stops when the list is gone. The two differ only where the exe would read NULL+8 (a dying ship that is clamped and holds objects; `univDeleteDeadShip` 0x00548280 calls it before `unClampObj`) | OK (deviation now documented, fix 6) |
| 12 | clamp scheme of `univUpdateAllPosVelShips` 0x00544f40 | 0x00544fa6..0x00544fe8: clampInfo, host, host objtype 0, host's host NULL, host list's first object == ship, then the host goes on the list; else `physUpdateObjPosVelShip` | OK |
| 13 | `kamikazeCheck` 0x0043ab20 | locals 0x309 / 99 / 0.0, two `cat_kamikazeCheckShip` calls, return 0 if neither, `ApplyDamageToTarget(...,GS 0, how, killer, other, 5 / 6)` | OK |
| 14 | `cat_kamikazeCheckShip` 0x0043a980 | objtype / sf 0x2; `cat_shipsClampedTogether(attacktarget, other)`; mimic (+0x18 bit 8), same owner; bCruiseMissile 0x3A + `cat_beastGetAttackStaticInfo` (0x00429b70: statinfo+0x3DC when +0x3E0 >= 0), mp needs upgrade 0x10000 and another owner; hits byte +0x614; dPrisonShip 0x86 in sp below the required hits: health x 0.1 (0x0072a4d4); else infect; else `kamikazeDamage` 0x00bb32c0; mimic x `cat_shipGetRamDamageScale`; self = maxhealth x 9999 (0x0072a6a4), how 0x29C, killer = playerIndex, velocity zeroed | OK |
| 15 | `cat_kamikazeCheckHit` 0x005b1990 | `kamikazeCheck(ship, target)`, size sum +0x4C, distance target-ship on the collision positions (+0xE0), `fsqrt`, `ObjectsCollided`; the trailing 0x0049ae30 is `nullsub` | OK |
| 16 | `cat_collBulletDFGFieldCheck` 0x0043cd10 | target objtype ship and type 0x4E/0x2D (tDFGFrigate/bDFGFrigate), dist^2 from collPosition (+0xE0) to bullet position (+0x58) < custstatinfo(+0x194)+4, `univDFGFieldEffect(ship, bullet, universe_totaltimeelapsed 0x00b62ac0)`; the exe returns its result, the port FALSE with a CAT-TODO (no big-gun shell before #4) | OK |
| 17 | `dmgShipThink` 0x004627b0 | render list, specialFlags2 0x8 skip, classes 5/6 strike; limits 0x008790dc.. (0.6, 0.4, 0.6, 0.4, 0.2, 0.075) as the tree's; `ratio <= limit` plays with `ratio/limit`; table 0x00c41fc0 | OK |
| 18 | `dmgPlayEffect` 0x00462340 (head) | chance `1 - ftol(x * -20.0)` (0x0072a7bc), < 0 -> 1; crystals 0x7E/0x7F: `ranRandomFn(0) % chance`, then +0x4D4 = time, +0x4D8 = +0.5 (0x0072a4c8), +0x4D0 = 2 | OK |
| 19 | `cat_weaponDamageVsArmour` 0x00514460 | `[(type*3 + armour)*4 + 0x0072b228]` | OK |
| 20 | `cat_shipGetDamageMultiplier` 0x005148f0 | `[level*4+0x0072bb78]`; bMothership 0x35: + `cat_beastMothershipDamageBonus` 0x005bca30 (CAT-TODO #4); sp and skill (0x008941a0) 0: doubled | OK (its comment's address was wrong, check 46) |
| 21 | `cat_gunCanFire` 0x0048a1c0 | dissolving (+0x570 == 1) FALSE; not second mode -> `gunCanShoot` (the exe pushes a third argument that `gunCanShoot` never reads: no `[esp+0x24]` access); firetime2 <= 0: `firetime < elapsed`; `elapsed <= firetime2` FALSE; latest fire of the second-mode guns (stride 0x88); n > 1: `latest < now - firetime2/n` | OK |
| 22 | `gunCanShoot` 0x0048a2f0 | getShipAndItsCommand first; no burst: firetime x 4 / 2.5 / 1.75 / 1.25 below 0.1 / 0.25 / 0.5 / 0.75 (doubles 0x0072ab20..08) unless GS 0x12/0x13; burst states 1/2/0 with Ship+0x610 1/0, `soundEventBurstStop` 0x00521710, wait time scaled the same; passive check `attributes & 4`, `!statinfo+0x20C`, not (2 and 1 and formation 5); `ranRandomFn(10)` x (Hi-Lo) / 2^32 | OK |
| 23 | `cat_shipUpgradesInit` 0x005140a0 (the gun word) | armour = `[type*8+0x0072b318]` at Ship+0x3F0, upgrades +0x3F4 = 0, each gun: Gun+0x00 = GunStatic+0x0C (StartUpgradeLevel), `cat_gunStaticSetSoundTypes` 0x0048b790; `off32.sh`: `Gun.catWeaponType` 0x00, `gunstatic` 0x84, `startUpgradeLevel` 0x0C | OK |
| 24 | `dockPutShipInside` 0x004652c0 | already-inside test, memAlloc 0x14, `listAddNode`, `lmUpdateShipsInside` 0x0049e070, class 6/0xB -> +0x0C, 5/7 -> +0x0E (`off32.sh`: FightersInsideme 0x0C, CorvettesInsideme 0x0E) | OK |
| 25 | `GetDirectionVectorOfShip` 0x004631f0 | third argument read at offset 0 as a matrix: columns at +0, +0xC, +0x18, case 3 writes nothing, bit 4 negates | OK |
| 26 | `dockRemoveShipFromInside` 0x004632e0 (the counterpart of 24) | class 6/0xB -> `dec +0x0C`, 5/7 -> `dec +0x0E`; the port had kept HW1's fighter/corvette-only decrement | **WRONG** (fix 1) |
| 27 | `cat_lmModuleSlotsRemoveType` 0x0049e970 | seven slots 0x00a4478c..0x00a447a8 cleared by type, then each empty slot 0..5 takes the next filled one | OK |
| 28 | `cat_beastWeaponChargeFraction` 0x0042b570 | GS 0x13 guns: 1.0 when charged, else elapsed / firetime (warning and 0 when firetime <= 0), capped at 1 | OK |
| 29 | `cat_beastTimeSpentStunned` 0x0042b630 | type 0x8C (dEcclesiam) doubles 0x00c52150 | OK |
| 30 | `cat_beastWeaponHasTarget` 0x0042a4a0 | types 0x2E, 0x2F, 0x35, 0x82 (byte table 0x0042a4f4): specifics +0 target, else `cat_beastWeaponFindNearestTarget` | OK |
| 31 | `cat_shipCanUseBeastWeapon` 0x00429c50 | statinfo type (+0x1E0) in the same four, then upgrade 0x10000 | OK |
| 32 | `cat_blastInit` 0x00516370 / `cat_blastReset` 0x00516390 | `listInit` both lists then the random tables 0x005166b0; `listDeleteAll` object list then blast list | OK |
| 33 | `cat_blastCreate` 0x00515a50 + the six templates | jump table 0x00515b24 -> 0x00893da0 + 0x68 x type; all 25 dwords of each template equal `cat_blastTemplates[]`; position, start time, player, +0x50 = 0, alloc 0x74, +0x54 = 0, `listAddNode` | OK |
| 34 | `cat_blastAffectShip` 0x00515d30 | shell test, push, types 5 / 4 (mass < 700), drift damage 0.2 x maxhealth, `cat_shipStartDrift` 0x005b3b40 (CAT-TODO), torque 100, type-1 attribute bits, `SOF 0x40000000` skip, `ApplyDamageToTarget(..., 0x29A, player, 0, 2)`, `cat_rammingFrigateForceRelease` 0x005c0ae0 (CAT-TODO). The damage sum: the exe computes `((d0 + const) + k1/d) + k3/d^2`, stores it, then `k2/sqrt(d) + that` (0x00515f6e..0x00515fa5); the port summed `k3/d^2 + k1/d + d0 + const`, another rounding order in a per-tick damage path | **WRONG** (fix 2) |
| 35 | `AsteroidTakesDamage` 0x004f1730 | fraction = health / maxhealth (0 when health <= 0), RUs = ftol(static RUs x fraction) at +0x190, return `wasAlive && health <= 0` | OK |

### Data the port reads (4)

| # | check | evidence | result |
|---|---|---|---|
| 36 | `WEAPON_*` (the gun word's values) | exe `NumStrXlate` table at 0x00889ea8: PROJECTILE 0 .. BEASTINFECT 8, the tree's enum (ION 2, PLASMABOMB 3, LASER 4, BEASTINFECT 8 printed) | OK |
| 37 | `Gun` / `GunStatic` layout behind the gun word | `off32.sh`: Gun +0x00 word, +0x70 lasttimefired, +0x74..+0x80 burst, +0x84 gunstatic, size 0x88; GunStatic +0x04 / +0x08 damage types, +0x0C / +0x10 upgrade levels, +0xE8 firetime, +0xEC / +0xF0 burst, +0x100 firetime2: every offset the disassembly of checks 21-23 and 28 uses | OK |
| 38 | dock point types | exe `dockpointtypeinfo` 0x00889c48: 35 names, values 0..34 in the tree's order (DPT_Med_In 6, DPT_Destroyer_EB 18, DPT_Swarmer_In 21, DPT_Crystal_Process 23, DPT_SalvageProcess 29, DPT_Mothership_Out 34) | OK |
| 39 | `SalCapCorvetteHeader` | exe 0x008c6698: sizeofShipSpecifics **0x30**, slots 1, 3, 8, 9, 10, 11, 12 (= 11), 15, 16, 19 = 0x005c59c0..0x005c5ae0, the same slots the tree's header fills | OK |

### Claims of the report and the conventions (7)

| # | check | evidence | result |
|---|---|---|---|
| 40 | DONE WHEN: the provisional salvage / Beast-weapon entries are real ports or carry a CAT-TODO naming their stage | `grep CAT-PROVISIONAL\|CAT-TODO` in `Salvage.c`, `BeastWeapon.c`, `SalCapCorvette.c`: 13 markers, each names M2-R4 #4, #5, stage 3 or stage 6 | OK |
| 41 | DONE WHEN / report: "no HW1 salvage reader left"; the report's grep "now finds only comments" | the report's own grep finds 25 code lines. Most are Cataclysm names (`catSalvagePointIndex`, `CatSalCapCorvetteSpec`, the dormant `SAVE_SALVAGEINFO`) and `salCapCorvetteInSelection` (already on `SPECIAL_CatSalvager`). But `salCapCorvettesInSelectionCantDoFormation` (`CommandLayer.c`, called by `clProcess` after a special order) still tested HW1's dormant `SalCapCorvette` type; the exe's `clProcess` tests `cat_shipIsSalvager` 0x004f8f60 and specialFlags 0x4 inline (0x0044c448..0x0044c45a), so a Cataclysm salvager never redid its formation | **WRONG** (fix 3) |
| 42 | delta-map counts: 40 review entries, IDENTICAL 3,096 -> 3,091, MODIFIED 1,247 -> 1,252, reviewed 1,491 -> 1,529 | `d47a23f` vs the tree: 40 new review keys; counts as stated | OK |
| 43 | no edited function the map calls IDENTICAL without a review entry | `editedfuncs.py`: 306 edited functions (by the enclosing definition of each changed line); 17 are or were IDENTICAL: 5 are MODIFIED now with this port's review (`gunCanShoot`, `dmgShipThink`, `ResourceMovingTooFast`, `ChangeSingleShipToCollectResource`, `R1ResourcerHarvestsAsteroid`), `battleAccountForLosses` has this port's note, and the other 11 (`MoveReachedDestinationVariable`, `collUpdateObjsInCollBlobs`, `LoadConsMgrDetermOptional`, `kasfIntercept`, `physApplyRotToObj`, `DustCloudChargesUp`, `scriptSetBlobBiggestRadius`, `MakeNewGasCloudStaticInfo`, `univObjectOutsideWorld`, `univupdateClose`, `vecVectorsBlend`) are only followed by new functions, globals or deleted HW1 code: their bodies are unchanged (hunks read) | OK |
| 44 | every Cataclysm change carries a `CAT:` comment | `hunks.py`: 147 of 658 hunks have no `CAT` in or within 60 lines above them; read one by one, they are includes, declarations, call sites updated for a commented signature change (`GetDirectionVectorOfShip`, `gunShipFirePower`, `dmgPlayEffect`), bodies inside Cataclysm-only files or functions (`BeastWeapon.c`, `Salvage.c`, `cat_gunCanFire`), static asserts, and deletions of HW1 salvage code under a file-level CAT note (`SaveGame.c` "no SaveSalvageInfo/...", `KASFunc.c`, `MEX.c`, `ObjTypes.c`, `StatScript.c`) | OK |
| 45 | report: "every run of this port reached 1,584" against R3's 1,248 | the port's runs used `--timeout 360`, `r4c-final-sp` 340 (both `summary.txt`); the audit's 340 s run of the producer's binary reached 1,248. The sentence reads as progress; it is the timeout | **WRONG** (fix 5) |
| 46 | exe addresses in the new comments | `cat_beastMothershipSpeedBonus 0x005bc9xx`, `cat_beastMothershipDamageBonus 0x005bc9xx` (`ShipSelect.c`) and `cat_shipStartDrift 0x005b...` (`Blast.c`) are not addresses; `functions.csv`: 0x005bca00, 0x005bca30, 0x005b3b40 (the calls at 0x00514940+, 0x00514912, 0x00515ec2) | **WRONG** (fix 4) |

**Precision: 41 / 46 = 0.891** (41 correct, 5 wrong).

## 4. Findings and fixes

1. **`dockRemoveShipFromInside` was left at HW1's counting (check 26).** The port made `dockPutShipInside` count micro
   ships as fighters and resource ships as corvettes, but the decrement in `dockRemoveShipFromInside` 0x004632e0
   still tested only `CLASS_Fighter` / `CLASS_Corvette`; the exe decrements +0x0C for classes 6/0xB and +0x0E for
   5/7. A docked micro or resource ship that left would keep its count (`FightersInsideme` / `CorvettesInsideme`,
   read by the launch manager and `CommandLayer.c`). `univRemoveShipReferences` already counts the exe's way.
   *Fixed* in `Dock.c` with a `CAT:` comment; review entry 0x004632e0 (MODIFIED) added and merged with
   `python3 tools/delta_map.py merge --done m2r4:combat-audit` (reviewed 1,529 -> 1,530; classes unchanged); report
   annotated `[audit: ...]`.
2. **`cat_blastAffectShip` summed the damage terms in another order (check 34).** Exe 0x00515f6e..0x00515fa5:
   `((d0 + const) + k1/d) + k3/d^2`, stored to a float, then `k2/sqrt(d) + that`. The port wrote
   `k2/sqrt(d) + (k3/d^2 + k1/d + d0 + const)`. Float addition is not associative and this damage feeds the
   universe checksum. *Fixed* to the exe's order (two statements, the first one stored); report annotated.
3. **An HW1 salvage reader was left (check 41).** `salCapCorvettesInSelectionCantDoFormation` tested the dormant HW1
   `SalCapCorvette` id. *Fixed:* it tests `cat_shipIsSalvager` (the exe's inline test in `clProcess` 0x0044c448, with
   the same specialFlags 0x4 `SPECIAL_BrokenFormation`), with a `CAT:` comment; the report's "finds only comments" is
   annotated with what the grep finds.
4. **Three non-addresses in new comments (check 46).** *Fixed* to 0x005bca00, 0x005bca30, 0x005b3b40 (and
   `cat_rammingFrigateSpeedMultiplier` 0x005c2120 named), marked `[audit: was ...]`.
5. **Tick-count framing (check 45).** *Fixed:* report annotated at both places (360 s against 340 s; 1,248 at 340 s).
6. **Undocumented deviation in `cat_clampUnclampAllFromShip` (check 11, not counted wrong).** *Fixed:* `[audit: ...]`
   comment in `Clamp.c` stating where the exe and the port differ (only where the exe would fault).

**Fixes: 6** (3 code: findings 1-3; 3 comment/doc: findings 4-6), plus one delta-map review entry for fix 1.

## 5. Observations (no fix; owners named)

- **Dock purposes are not ported**, as the report states ("Not done: the Cataclysm dock purposes", 95 of `Dock.c`'s
  123 functions HW1-only). `ThisShipCanDockWith` 0x00464290 takes four arguments in the exe, `dockReserveDockPoint`,
  `RemoveShipFromDocking` and `RemoveShipFromLaunching` are REWRITTEN and unported. This is the main gap of the
  `dock` subsystem; owner as the report says (an M2-R4 #3 follow-up before stage 4), open question 1.
- **Saved clamp lists:** `LoadClampInfo` sets `clampedObjs` to NULL (since before this port, "save layout waits for
  the user's saves"). With this port's host-driven `updateClampedObject`, a loaded game would no longer carry clamped
  ships along until the list is restored from the exe's selection chunk (`SaveClampInfo` inlined in 0x00501050).
  Owner: the save-format work (the user's saves).
- Dormant HW1 type tests outside this port's files remain (`AIUtilities.c`, `AIAttackMan.c`, `AIOrders.c`,
  `Stats.c`, `InfoOverlay.c`; `Tactics.c` `tacticsSetShipToDoDodge`, which has no exe counterpart in the map): AI
  (stage 4) and UI (stage 3) owners; none reads salvage data.
- The sp run reaches no dock, DFG field, blast or salvage; those paths are build- and read-checked only (as the
  report says).

## 6. After the fixes

Builds: audit dir debug and release rc 0, 0 compiler warnings (sizes in section 1); `tools/build.sh --debug` and
`--release` rc 0, 0 compiler warnings (the only WARNING line is meson's sanitizer notice); `tools/m2_layout32.sh` OK.
Plain harness `r4cb-audit-fixed2-sp`: universe running at 1,248 ticks (as before the fixes and as `r4c-final-sp`),
ASan 0, no unsuppressed leak, no new UBSan kind. Delta map: IDENTICAL 3,091, MODIFIED 1,252, REWRITTEN 77, reviewed
1,530 (label `m2r4:combat-audit`).

Verdict: PASS-WITH-FIXES - 41/46 checks match r6 and the exe (30 bodies across salvage, clamps, kamikaze, DFG, damage, gun, dock, launch, Beast weapon, blasts, resources; the gun word, WEAPON_*, the 35 dock types, the 0x30 SalCap header); sp 1,248 ticks ASan 0, statics 143/145; 6 fixes (dockRemoveShipFromInside counts, blast damage order, a left HW1 salvage type test, 3 non-addresses, the tick-count framing, a clamp deviation note); the dock purposes remain an open follow-up the report declares
