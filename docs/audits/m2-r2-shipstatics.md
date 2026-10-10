# Audit: M2-R2 port "shipstatics" (producer 3)

Audited: `docs/m2-r2-shipstatics.md`, the spec addendum in `docs/formats/shipstatic.md`, and the uncommitted diff of
branch `m2` (57 files: 55 under `src/`, `tools/`, docs; +1,888/-1,359 before the audit), plus the new
`tools/m2_layout32.sh` and `tools/formats/shipstatic_engine_check.py`. Date 2026-10-10. Ghidra was not run.

Evidence came from four sources:
- Exe values from `original/game/cataclysm.exe`, read with the read-only PE reader of the identifiers audit
  (`/tmp/audit-ident/pe.py`, not the producer's tools) and `objdump -d`.
- Code from `ghidra/exports/cataclysm_gog_r5/decompiled.c`, `functions.csv`, `symbols.csv` and `strings.csv`.
- The audit's own scripts in `/tmp/audit-ss/`: `keytab.py` (key tables), `headers.py` (`CustShipHeader`), `values.py`
  (field values), `hunks.py` (CAT comments), `probecmp.py`. None is committed.
- Harness runs, all in `original/scratch-m2/m2r2-harness/runs/audit-ss-*`.

## 1. Build from clean

- I deleted `original/scratch-m2/build-audit-shipstatics` and ran `meson setup` there with tools/build.sh's arguments:
  `debug/` with `--buildtype=debug -Db_sanitize=address,undefined`, `release/` with `--buildtype=release
  -Db_sanitize=none`. Then `meson compile` in each. meson.build sets `warning_level=3`, `werror=true`.
- Both builds finished with rc 0 and `[292/292] Linking target hwi-engine`. Neither log has a compiler `warning:`
  or `error:` line. The one "WARNING" line in the debug setup is meson's own sanitizer notice.
- Sizes:

  | build | file size | text | data | bss |
  |---|---:|---:|---:|---:|
  | debug | 75,247,416 B | 41,232,808 | 23,903,104 | 2,527,488 |
  | release | 5,186,232 B | 4,207,341 | 247,880 | 1,988,320 |

- After the audit's code fix, both clean dirs rebuilt with 0 warnings and the same file sizes. `tools/build.sh
  --debug` and `--release` in the worktree: rc 0, 0 compiler warnings.

## 2. Offsets, key handlers and parsed values against the exe

### 2a. Tables and layout (independent re-derivation)

`keytab.py` reads the exe's 16-byte `scriptStructEntry` rows (field offset = field − template) at
`ShipStaticScriptTable` 0x008b2428 and at the gun table 0x008975a0. It then compiles `offsetof(ShipStaticInfo, <field
expression of each src row>)` with `clang -m32` against the worktree's `SpaceObj.h`.

| # | claim | evidence | verdict |
|---:|---|---|---|
| 1 | ship key table = the exe's 178 keys, in the exe's order, each at the exe's offset | 178/178 names, order and ILP32 offsets equal; 0 mismatches | correct |
| 2 | gun table = the exe's 39 keys, in order, at the exe's `GunStatic` offsets | 39/39 equal (the 4 obsolete keys point at offset 0 in both) | correct |
| 3 | the HW1 signed callbacks equal the exe's `scriptSetU{byte,word,dword}CB`, which are folded signed bodies | decompiled 0x00525bc0/0x00525bf0/0x00525c20 all `sscanf("%d")` (0x00753118 = `%d`); 15 ship and 2 gun rows differ only in that name | correct |
| 4 | ILP32 sizes 0x458 / 0x11C / 0x8C | `tools/m2_layout32.sh`: `char (*)[1112] char (*)[284] char (*)[140]`, OK. Negative test in a scratch copy (`== 0x459`): `static assertion failed ... 1113`, `m2_layout32: FAIL`, rc 1 | correct |
| 5 | 8 `_Static_assert` + 225 `CAT_ASSERT_OFFSET` | grep: 9 / 226 lines, minus the macro definition | correct |
| 6 | 29 new keys, 7 GoK keys gone | key names vs `git show HEAD:src/Game/Universe.c`: 156 → 178; new 29 (each marked `// CAT: new key`, 29 markers); gone `NUMBER_OF_SALVAGE_POINTS`, the 5 fuel keys, `scaleFactor` | correct |
| 7 | 47 header initializers rewritten with 0 slot differences from HEAD | `headers.py`: HW1 positional order mapped to the named slots, 47 headers, 0 differences | correct |
| 8 | slot meanings (Attack in slot 21, Fire in slot 5, AttackPassive in slot 6, ...) | port vs exe `shipHeaders` 0x008b1938 for 28 distinct HW1 headers: 864 slot pairs agree. The 88 differences are Cataclysm body contents (stage 2), NULL new slots, or linker-folded names (`P1MothershipAttackPassive`, `MissileDestroyerAttack` in every Attack slot). None puts a function in the wrong slot | correct |
| 9 | `CopyPointersFromRealShipStaticInfo` 0x004ffc60: +0x194, +0x3F4, +0x400/+0x404, no salvage or tractor pointer | disassembly: copies +0, +4, 0x23 dwords at +0x108, +0x194, +0x2F8[4], +0x314, +0x3EC, +0x3F0, +0x3F4, +0x104, +0x3F8, +0x400, +0x404 | correct |
| 10 | `CloseStatShipInfo` 0x0053c500 frees +0x3F4, +0x3F8, +0x3FC, +0x3F0, +0x104, +0x3EC | disassembly 0x53c755..0x53c7e5 | correct |
| 11 | `collrectmid` at `StaticCollInfo`+0x38 | `mexSetCollisionInfo` 0x004b7880 stores at `[esi+0x38/0x3c/0x40]` | correct |
| 12 | `sinbank` at +0xF8 | `aishipPrecalcInfo` 0x00413bd0 writes +0xF8/+0xFC/+0x100 | correct |
| 13 | no `NUMBER_OF_SALVAGE_POINTS`/`SalvagePoint` string; no `dockInitializeCustomFunctions`/`mexGetTractorBeamStatic` | byte search of the exe: both strings absent; functions.csv: 0 rows | correct |

### 2b. `InitStatShipInfo` 0x0053bab0 (decompiled.c + disassembly) against `src/Game/Universe.c`

| # | claim | evidence | verdict |
|---:|---|---|---|
| 14 | defaults before the file check (+0x408 1, +0x1F4 −1, +0x1F8 1, +0x3D4 1, +0x3D8 20, +0x320/+0x240/+0x40C 0, +0x1F0 0) | decompiled lines 1-12 of the body | correct |
| 15 | defaults before the read (+0x3DC 1, +0x3E0 −1, +0x3E4 0x31 = `bWorker`, +0x43C..+0x448 0, byte +0x454 0, +0x29C 0, +0x211 0, +0x212 1, +0x213 0, +0x3D0 1) | decompiled; `bWorker` = 49 in `src/Ships/ShipDefs.h` | correct |
| 16 | `repairCombatDamage = repairDamage * 0.2` | `fmul dword [0x0072a510]` (D8 0D), float 0.2 | correct |
| 17 | `cat_univInitShipTypeFlags` = the exe's per-type bits | 60 type→bits pairs from decompiled 0x0053b8c0 equal the port's switch (names resolved through ShipDefs.h) | correct |
| 18 | `isStrikeCraft` = the exe's 27 types (also the inline padding switch) | 0x0053ba20 and the inline switch in 0x0053bab0: same 27 ids as the port | correct |
| 19 | single-player build costs `bHeavyCruiser` 3000, `bIonArrayFrigate` 600, `sMothershipBigGun` 5000 | exe tests 0x2e/0x2c/0x1f = those ids | correct |
| 20 | MiningBase hack keyed on type 100 (`trMiningBase`), +0x54/+0x60 | decompiled `if (type == 100)` | correct |
| 21 | thrust order `fld mass; fmul thrust; fmul modifier` at 0x0053c05d | objdump 0x53c05d..0x53c06d | correct |
| 22 | `dockShipRange` kept as read; Dock.c forms HW1's range at its only reader | no padding/squaring in 0x0053bab0; grep: `ShipWithinDockRange` is the only reader | correct |
| 23 | **`forceFieldProtectionDistanceScale <= 0` becomes 2.0 after the slot-1 call** | 0x0053c3af `fld [ebp+0x1f4]; fcomp qword [0x0072a548]` (DC 1D: a **double**, 1.0), `test ah,0x41` → set 2.0 when ≤ 1.0. The stripped warning at 0x008b58c8 reads "Force Field Protection Distance Scale is smaller than or equal to 1.0 ... Defaulting to 2.0". The port tested `<= 0.0f` | **wrong (fixed)** |

### 2c. Gun, dock and nav-light parsers

| # | claim | evidence | verdict |
|---:|---|---|---|
| 24 | at a gun's `}` the damage arrays take the base values for all 3 tactics (no `DamageBonus`) | 0x5269e0..0x526a88: +0xCC → +0xB4/B8/BC, +0xD0 → +0xC0/C4/C8 (gun-relative) | correct |
| 25 | no range/speed bonus; `BulletLifeTime2` = range2/speed2, each falling back to mode 1 when ≤ 0 | 0x526b49..0x526c7f (`test ah,0x41` fallbacks, `fdiv`) | correct |
| 26 | gunless `CLASS_Resource` ships get `ASTEROID_HARVEST_RANGE` | 0x526c81 `cmp [ebx+0x1e8],7`; `CLASS_Resource` = 7 | correct |
| 27 | allocation `n * 0x11C + 4`; `slaveDriver` (+0x118) = −1 at `GUN n` | 0x5268fa..0x52692e, 0x526992..0x5269a4 | correct |
| 28 | obsolete gun keys only print | 0x00526050/60/70: `push str; call 0x6fe9a0; ret` | correct |
| 29 | `GUN_HiddenGimble` 1, `GUN_VisibleGimble` 2; `WEAPON_*` 0-8; `GUNSIZE_*` 0-3 | exe tables 0x00889c08, 0x00889ea8, 0x00889e80 | correct |
| 30 | `WEAPON_*`/`GUNSIZE_*` are matched case-insensitively (`_stricmp` 0x00676c90) | disassembly of 0x00676c90: the CRT `_stricmp` fast path (`sub al,0x41; cmp al,0x1a; ... add cl 0x20`) | correct |
| 31 | 35 `DPT_*` names and values in exe order | `dockpointtypeinfo` 0x00889c48: 35 rows; the port's enum is 0..34 in the same order | correct |
| 32 | `scriptSetDockStatics` replaces equal heading/up by 0/2 | decompiled 0x005270c0: `+0x30 == +0x34` → 0/2 | correct |
| 33 | nav lights: the declared count wins, and a count < 1 reads none | decompiled 0x00526dc0: `if (count < 1) break;`, stops at `count <= read` | correct |

### 2d. Probe against the engine's parse, with field values

- **Counts.** The audit ran `shipstatic_engine_check.py` on run `audit-ss-statics-p1nomesh`. Against
  `game_extracted/cataclysm`: OK 140, OK-DECLARED-COUNT 3, OTHER-LOADED 24, OTHER-NOT-LOADED 6, TYPE-NO-FILE 2,
  **143 of 143**.
- **Merged tree.** 25 `.shp` are overridden by `update.big`, and the engine reads those versions. The audit
  re-ran the check against a scratch tree with update over base (`/tmp/audit-ss/merged`): the same 143 of 143.
- **Field values.** The engine logs only counts. So I built an instrumented copy of the tree in `/tmp/audit-ss/tree`
  (not the worktree). After the `.shp` read it dumps every `ShipStaticScriptTable` field, and after
  `scriptSetGunStatics` every gun-table field of every gun.
  - I ran it with `/catShipStaticsTest` (run `audit-ss-dump2`, `iso-ss-p1geo` tree, all 143 types; a release
    build cannot run the harness because it has no `/logFileLoads`).
  - `values.py` then rebuilt the expected value of every key from the `.shp` text (update version where one exists)
    and the exe's enum tables. It emulates `parseLine`, `%f`/`%d`, bool, cos/deg and the
    class/ship-type/gun-type/`WEAPON_*`/`GUNSIZE_*` lookups, plus the lifetime/tracking defaults.
  - Result: **143 ships: 7,039 ship key values equal, 0 differ. 271 guns: 4,759 gun values equal, 0 differ.**
  - 433 ship keys were not compared: the `LODFile`/`pMexData` pointers, `scriptSetFramesCB`, `scriptSetRGBCB` and
    `scriptSetReal32SqrCB` rows.
  - The first run against the base files alone gave 27 differences. All 27 were in the 25 update-overridden files,
    which proves the engine reads `update.big`'s versions, as the game does.
- The 10 ships of different races asked for are a subset of this: `sRecon`, `sMothership`, `bCarrier`, `bRecon`,
  `kHeavyCorvette`, `tStandardDestroyer`, `p1Outpost`, `trFloatingCity`, `trGhostship`, `dRKO`. A run restricted to
  these 10 compares 637 ship values and 763 values on 47 guns: all match.

| # | claim | evidence | verdict |
|---:|---|---|---|
| 34 | 143/143 ship-type files parse as the probe does | engine check (base and merged) plus the field-value comparison above | correct |
| 35 | the 3 light-count files use the declared count, as the exe does | log: `sRecon`/`bRecon` `navs=0`, `trGhostship` `navs=98`; exe behaviour row 33 | correct |
| 36 | the 3 unread files: "never read by the exe (no such string in it)" | no `<name>.shp` string and no type-table name, but `defaultship` and `race_derelict\dplasmacloud\rl0\lod0\cloud` (0x008bab7c, fog layer texture of `cat_rndFogLayerTextureLoad`) are in the exe. The conclusion holds; the stated evidence is wrong for 2 of 3 | **wrong (doc fixed)** |
| 37 | the probe's output is byte-identical to before its refactor | `probecmp.py`: HEAD probe vs current on all 173 files, 8,247 lines, identical | correct |

## 3. Diff review

- **Scope.** Every `src/` change belongs to the ship-statics item. That covers the layout and its fallout (salvage,
  tractor and scale-factor removal in `Dock.c`, `SaveGame.c`, `UnivUpdate.c`, `SalCapCorvette.c` and `render.c`), the
  47 header initializers, the key and gun tables, the self-test (`main.c`, `utility.c`, `Universe.c`) and the enum
  tables (`ObjTypes.c`/`.h`). Outside `src/` the diff touches only the producer's report, the spec addendum,
  `tools/m2_harness.sh` (a `statics` mode), the probe refactor and two new tools. Since the re-read stage is
  committed (ddc8a18), the working tree has nothing else uncommitted.
- **CAT comments.** `hunks.py` checked the 116 changed hunks under `src/`. Only 1 has no `CAT` in it: `#include
  <stddef.h>` in SpaceObj.h, needed by the `offsetof` asserts. That is trivial. The new slots, new keys and new
  functions all carry the exe address.
- **IDENTICAL functions edited** (class from `ghidra/delta_map.json`). These are findings; none was fixed in code:
  1. `CopyPointersFromRealShipStaticInfo` 0x004ffc60, IDENTICAL/medium. The edit matches the exe (row 9: the exe
     copies neither pointer), so the **map** is wrong and should say MODIFIED.
  2. `precalcStaticInfoGuidance` 0x0053ad70, IDENTICAL/medium. The edit changed the shared body to the ship inline
     order `(mass*thrust)*mod`. The exe's standalone body, which `universeStaticInit` still calls (0x53d9f0), does
     `fld mod; fmul thrust; fmul mass`. The exe's missile and mine inits inline the ship order (0x53b5aa, 0x53b835).
     So the port's derelict path now differs from the exe's in operation order. This is neutral while
     `GLOBAL_SHIP_SPEED_MODIFIER` = 1.0 (exe 0x008a8098 and shipped `tweak.script`). Recorded for stage 2.
  3. `scriptSetStruct` 0x00526260, IDENTICAL/medium. It gained a diagnostic counter. Behaviour is unchanged.

  The other edited functions are MODIFIED/high in the map: `InitStatShipInfo`, `isStrikeCraft`,
  `scriptSetGunStatics`, `scriptSetDockStatics`, `CloseStatShipInfo` and `univAddDerelictByStatInfo`. The rest are HW1
  salvage paths reduced to the always-NULL `CAT_SALVAGESTATICINFO`, so their edits only follow from the removed field.
- **Unported exe steps, all already listed by the producer:** the `DockStaticInfo` "center" index (+0x4C) after
  `mexGetDockStaticInfo`, the `mexSetCollisionInfo` changes, and the `NAVLightStatic` 0x48 stride. The port sets
  +0x400/+0x404 to HW1's default dock handlers; the exe does not set them in `InitStatShipInfo`. The producer
  documents this as a deliberate safe default (stage 2).

## 4. DONE WHEN, re-run

| run | data | result |
|---|---|---|
| `audit-ss-gate-statics` | plain harness | rc 213, `process started`, first stop `Unable to open file: HW_Music.wxd` (producer 4's item) |
| `audit-ss-statics-iso` | `iso-ss` | rc 1, `Main_game_screen`, `Memory.c (1625): Corrupt cookie: 0x5f317000` right after `tStandardFrigate.shp` (type 91), 92 ships logged. This reproduces the producer's p1Outpost mesh stop |
| `audit-ss-statics-p1nomesh` | `iso-ss-p1geo` | rc 1, `ship statics self-test done`, 143 loaded, 2 not found (disabled), 24 other; quit-time `utility.c (3462)` `munmap(heap, 0)` assertion (pre-existing GoK code, verified); 0 ASan errors |
| `audit-ss-statics-p1nomesh-fix` | same, after the audit's fix | identical result |
| `audit-ss-sp-iso` | `iso-ss`, single player | rc 1, `level load (mission file read)`, `shipstatic: loaded RACE_SECT/sRecon.shp ... staticinit=next`, then `GenericInterceptor.c (205)` (stage-2 slot body); 0 ASan errors |

- **"170 of 173 .shp parse as the probe does (3 light-count cases like the exe)": not met as written. The
  producer says so.**
  - The 143 ship-type files (140 + the 3 light-count cases) are verified down to field values.
  - Of the 30 non-ship files, 24 are only logged as loaded by HW1's missile, mine, asteroid, dust-cloud, nebula and
    derelict loaders, whose key tables this item did not check.
  - 3 (`Galaxy`, `LavaBall`, `MattWorld`) are not loaded, because Cataclysm's 8-entry derelict type table (0x00889b68)
    is not ported. The exe has those names (byte search above).
  - So 167 of 173 are loaded, and 143 are verified.
- **"A log line per ship": met.** There is one `shipstatic: loaded ...` line per type, 143 in all.
- **"No ship type silently disabled": met.** Types 107 and 108 are logged `... not found, ship type N (...) disabled`.
- **"Static asserts pin the three struct sizes": met** (rows 4-5).

## 5. Precision

Checks 1-37 in section 2, plus 6 more:
- 38: the clean build with 0 warnings (section 1).
- 39: the plain gate's stop.
- 40: the level-load stop after the `sRecon` line.
- 41: the p1Outpost corruption stop.
- 42: the two types logged as disabled.
- 43: the LP64 pins hold (the x86-64 build compiles them).

All 6 are correct.

**Precision: 41 correct / (41 + 2 wrong) = 0.953** over 43 sampled checks.

## 6. Fixes made (2 factual errors)

1. **`forceFieldProtectionDistanceScale` threshold.** `src/Game/Universe.c` `InitStatShipInfo` now tests `<= 1.0f`
   instead of `<= 0.0f`, with an `[audit: ...]` comment citing 0x0053c3b5 and the string at 0x008b58c8. The
   SpaceObj.h field comment and the producer doc's `keys` table row are corrected too. No shipped `.shp` has a value
   in (0, 1] (values 1.1-11.5), so the shipped data is unaffected; a mod with a scale ≤ 1.0 would now behave as in
   the exe.
2. **"No such string in it".** The producer doc's line about the 3 unread `.shp` files gets an `[audit: ...]`
   correction.

Also added to the producer doc, marked `[audit]` (additions, not corrections):
- the update.big note (25 overridden files, check still 143/143);
- "What is left" 11 (the three IDENTICAL-function edits and the delta-map conflict).

`ghidra/delta_map.json` is not edited. After the fixes, both build configurations rebuilt with 0 warnings and the
self-test and level-load runs are unchanged.

Verdict: PASS-WITH-FIXES - ship statics verified exactly against the exe (178 + 39 key offsets, layout asserts, the InitStatShipInfo/gun/dock/nav logic) and down to 11,798 parsed field values on all 143 ship types; 2 factual errors fixed (force-field threshold 1.0, not 0; evidence for the unread files); DONE WHEN's literal "170 of 173" is not reached (167 loaded, 143 verified; the 3 derelict files need the unported derelict type table), as the producer reported
