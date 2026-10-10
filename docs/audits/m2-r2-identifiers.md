# Audit: M2-R2 port "identifiers" (producer 2)

Audited: `docs/m2-r2-identifiers.md` and the uncommitted `src/` diff of branch `m2` (21 files, +2,447/-1,256 after
the audit's fixes), plus `tools/m2_harness.sh` and `tools/m2_harness_iso.sh`. Date 2026-10-10. Ghidra was not run.
Exe values were read from `original/game/cataclysm.exe` with a small read-only PE reader of my own
(`/tmp/audit-ident/pe.py`, not the producer's tools), and code from `ghidra/exports/cataclysm_gog_r5/decompiled.c`
and `symbols.csv`. CET headers: `reference/CataclysmEngineTweaks/game/Game/{ShipDefs,RaceDefs,ClassDefs,objtypes}.h`.

## 1. Build from clean

- `rm -rf original/scratch-m2/build-audit-identifiers`, then `meson setup` with tools/build.sh's arguments
  (`--buildtype=debug -Db_sanitize=address,undefined`; `--buildtype=release -Db_sanitize=none`) and `meson compile`.
  meson.build sets `warning_level=3`, `werror=true`.
- Both built. Compiler `warning:`/`error:` lines: 0 in both logs (the one "WARNING" line is meson's own
  sanitizer notice).
- Sizes: debug `hwi-engine` 75,115,720 bytes (text 41,240,600, data 23,904,160, bss 2,482,016); release
  5,173,576 bytes (text 4,201,485, data 240,056, bss 1,942,048). After the audit's fixes: debug 75,115,728,
  release 5,173,576. `tools/build.sh --debug` and `--release` in the worktree also rebuilt with 0 warnings.

## 2. Values checked against the exe and CET (sampled claims)

| # | claim | evidence | verdict |
|---:|---|---|---|
| 1 | `ShipType_enum` = 145 types, CET's ids | exe `shiptypeinfo` 0x00889530 (145 rows) vs CET `ShipDefs.h` (145 defines) vs `src/Ships/ShipDefs.h`: 0 mismatches | correct |
| 2 | `ObjTypes.c` `shiptypeinfo` in id order | 145 rows, same order as the exe table | correct |
| 3 | `shipraceinfo` = `RACE_SECT`..`RACE_DERELICT`, exe spelling `RACE_TAIDAN` | exe 0x008899c0 | correct |
| 4 | `nisraceinfo` = 7 races + `Asteroids`..`Missile` (7-14) | exe 0x00889a00, 15 rows, strings equal | correct |
| 5 | `shipclassinfo` = 12 classes | exe 0x00889a80 | correct |
| 6 | `RaceDefs.h` 0-6, `NUM_RACES` 7 | CET `RaceDefs.h` | correct |
| 7 | `ClassDefs.h` `MonsterShip` 9, `Component` 10, `Micro` 11 | CET `ClassDefs.h` | correct |
| 8 | `FirstShipTypeOfRace` {0,33,60,74,92,98,109} | exe 0x008894e8 | correct |
| 9 | `LastShipTypeOfRace` {32,59,73,91,97,108,144} | exe 0x00889504 | correct |
| 10 | `STD_FIRST_SHIP sRecon`, `STD_LAST_SHIP bMissileCorvette` | CET `ShipDefs.h:147-148` | correct |
| 11 | `StrToShipType`/`ShipTypeToStr`/`StrToShipRace`/`ShipRaceToStr`/`StrToShipClass`/`StrToNisRace` are the same table walks | decompiled 0x004d6e30, 0x004d6bc0, 0x004d6db0, 0x004d6b90, 0x004d6d70, 0x004d6bf0 | correct |
| 12 | `GetShipStaticInfoSafe` "checks only the type range" like the exe | exe bytes `8b 4c 24 04 81 f9 91 00 00 00 72 03`: `cmp ecx,0x91; jb` (unsigned). The port tested `shiptype >= TOTAL_NUM_SHIPS` on a signed `ShipType` (`smemsize` under `HW_PTR_64`), so a negative type indexed `ShipStaticInfos[-1]`; HW1 returned NULL there (`offset < 0`) | **wrong (fixed)** |
| 13 | `RacesAllowedForGivenShip[145]` | exe 0x008b1814: 145/145 equal (incl. `trGhostship` Sect, `dAcolyte` Traders, nis* 0) | correct |
| 14 | `shipHeaders[145]` | exe 0x008b1938 resolved through `symbols.csv`: 89 rows equal; the other 56 are `DefaultShipHeader` where the exe has one of the 22 Cataclysm-only headers (54 rows) or NULL (2), exactly as documented | correct |
| 15 | `kCloakGenerator`/`tCloakGenerator` headers are NULL in the exe | same read | correct |
| 16 | `rowAvoidBy[145]` | exe 0x008b1b7c, 145/145 | correct |
| 17 | `rightOfWays[145]` | exe 0x008b1dc0, 145/145 | correct |
| 18 | `ShipStaticCapLimits`: 123 rows, keys and targets | exe rows (12 bytes) vs src: 123/123 names equal; every `cdLimitCaps[t]` target = 0x00ba03c0 + 4t, every `cdClassCaps[c]` = 0x00b3aba0 + 4c | correct |
| 19 | the cap table is at 0x008b1e54 | 0x008b1e54 holds 0; the table starts at 0x008b1e58 (`scriptSet(... &PTR_s_ShipTotalCaps_008b1e58)`, decompiled.c:266143, 288328) | **wrong (fixed)** |
| 20 | `StatIndexRaceOffsets` = {0,33,60,74,92} | exe 0x008961b8 | correct |
| 21 | `StatIndexRaceOffsets` is at 0x008961bc | 0x008961bc reads {33,60,74,92,257}; the code reads `DAT_008961b8` (decompiled.c:261800 ff.) | **wrong (fixed)** |
| 22 | 5x5 race table initial values | exe 0x008961cc = src | correct |
| 23 | 5 per-race flag arrays | pointers at 0x00896254 -> 0x008961e8.. ; 33/27/14/18/6 bytes, all equal | correct |
| 24 | `battleClassDependencies` {0,3,3,3,3,0,0,2,0,0,0,0} | exe 0x00750710 | correct |
| 25 | crate class lists (34 entries) | exe 0x00878934: same 34 types in the same order | correct |
| 26 | `crateInit` resets 0x2e entries, `NUM_IN_CLASSES` 0,2,2,2,8,5,9,4,2 | decompiled `crateInit` 0x00462080 | correct |
| 27 | `colPickScalarScan`: `%s%f%f`, Kushan->Sect, Taiidan->Beast | string at 0x007530c8 = `%s%f%f`; decompiled 0x0043dea0 | correct |
| 28 | `SetInfoNeededForShipAndRelatedStaticInfo`: missiles 0x55,10,0x5f,0x19,1,0x22; mine 0x54 | decompiled 0x004a29f0 | correct |
| 29 | `universeRaceStaticsScan` types (inlined in `universeFlagRaceNeeded`) | decompiled 0x0053c870: 0x55,10,0x5f,0x19; 0x54 for any race | correct |
| 30 | `mineStaticInfos` indexed "with a 1200-byte stride" | `[race * 300]` on a byte base; universeStaticInit/universeLoadEverythingNeeded step 300 bytes from 0x00b3ac10 to 0x00b3b444 (7 entries) | **wrong (fixed in doc)** |
| 31 | 12 parade files in exe order | decompiled `paradeSetTweakables` 0x004850d0: file order and buffers (0x00a3e448 + k*0x6e4, k = 0..11) match the `PARADE_*` ids | correct |
| 32 | parade choice: 23-type switch | decompiled 0x00486050: all 23 case labels and their parade ids match the src switch | correct |
| 33 | `PrintOrder` (spacers 146/147, 12 + 9 types) | exe 0x00883340 | correct |
| 34 | Tactics: 116 dodge + 29 kamikaze rows | exe `TacticsInfotable` 0x008a80a0: 145 `DodgeInfo`/`kamikazeDamage` rows, names equal and in order; each family's targets have one consistent base, so the `[type]` index = the type id | correct |
| 35 | `CLASS_Mine` -> 12 in Tactical | decompiled `toNewClassSet` 0x005289a0 (`0xc`) | correct |
| 36 | no `Dock.script`; 145 x 20-byte hyperspace rows | decompiled `scriptSetTweakableGlobals` 0x00527820: Flightman then Soundevent; clear loop 0x00ba1fd0..0x00ba2b24 step 20 | correct |
| 37 | `GenericInterceptorStaticInit` handles 14 Cataclysm types | decompiled 0x005b1e70: cases 0,1,2,0x21-0x25,0x3d,0x44,0x4b,0x52,0x5d,0x85 | correct |
| 38 | `crates.script` writes types up to 59 | 33 distinct type names in game_extracted `crates.script`, max id 59, none unknown | correct |
| 39 | provisional tables use the HW1 counterpart's value | 6 rows sampled in `PilotOffsets`, `PilotOffsetsR2`, `mrMenuActionsByShipType` (kCarrier, bCarrier, sMothership, kHeavyInterceptor, tResourceCollector) equal HEAD's HW1 rows | correct |
| 40 | DONE WHEN in the isolation tree | see section 4 | correct |
| 41 | plain gate first stop `HW_Music.wxd` | run `audit-ident-gate-sp` | correct |
| 42 | diff size +2,444/-1,255 | `git diff --shortstat -- src` before the audit's 3-line change | correct |

**Precision: 38 correct / (38 + 4 wrong) = 0.905** over 42 sampled checks.

Arrays indexed by type/race/class (grep of every declaration sized by `TOTAL_NUM_SHIPS`, `NUM_RACES`, `NUM_CLASSES`,
`TOTAL_STD_SHIPS`, `STD_LAST_SHIP`, `P1_LAST_SHIP`, ... and of every `[<HW1-only type or P2/P3>]` index):
- All type/race/class tables are sized by the new enums. `Crates.c` `[StandardFrigate+1]` and `mineStaticInfos[2]`
  were the two HW1-sized ones; both are fixed by the port.
- Left on HW1-style bounds, all consistent with CET or HW1: `tutBuildRestrict[TOTAL_STD_SHIPS]` and ConsMgr
  `progress[TOTAL_STD_SHIPS]` (CET `consMgr.h:71` has the same size; only buildable types 0-59 index them);
  ResearchAPI `shipcanbuild[STD_LAST_SHIP]` (HW1's off-by-one, now skips `bMissileCorvette`).
- `src/Win32/mainrgn.c` still has HW1's 59-row `mrMenuActionsByShipType`; it is not compiled (`src_Win32` does not
  occur in build.ninja).
- HW1-only constants still used as indices: `shiptotals[ResearchShip]` (LevelLoad.c 939/1204/1292/1360,
  Dock.c 5519/5919), each behind `ship->shiptype == ResearchShip` or in R1 research-ship dock code, so dormant;
  `Mothership.c` `specialDoorOffset[...][DDDFrigate|P2...]` script keys (doc item 3). Both are covered by the
  producer's "What is left" item 3.
- `RaceBitsToRace` (ObjTypes.c:539) still tests `P2_VALID`/`P3_VALID` (bits 16/17 of a `uword`, never set) and maps
  Kushan/Taiidan/Derelict bits to 0. It has no callers. Observation only.

## 3. Diff review (scope, IDENTICAL functions, CAT comments)

- **Out-of-scope files.** The working tree also has changes to `ghidra/delta_map.json`, `ghidra/delta_map_review.json`,
  `tools/delta_map.py`, `docs/m2-r1-function-deltas.md`, `docs/m2-r1-summary.md` and new `docs/m2-r2-reread.md`,
  `ghidra/proposals/m2-r2-name-fixes.json`, `tools/m2_stage_counts.py`. They belong to the concurrent re-read stage
  (`docs/m2-r2-reread.md`: "Analysis only; src/ is not edited"), not to this producer; no `src/` change comes from
  it. `tools/checkpoint.sh` runs `git add -A`, so the checkpoint below also records that stage's in-progress files.
- **Functions edited** (from the hunks): `colPickScalarScan`, `crateInit`, `paradeSetTweakables`,
  `CreateMilitaryGroupAroundShip`, `ioStartup`, `SetInfoNeededForShipAndRelatedStaticInfo`, `scriptSetTweakableGlobals`,
  `universeDefaultTeamColors`, `universeRaceStaticsScan`, `GetShipStaticInfo`, `GetShipStaticInfoSafe`,
  `GetValidRaceForShipType`. Classes in `ghidra/delta_map.json` (HEAD and now):
  - MODIFIED/REWRITTEN at HEAD: `paradeSetTweakables`, `ioStartup`, `scriptSetTweakableGlobals`,
    `GetShipStaticInfoSafe`. Not in the map: `universeRaceStaticsScan` (the re-read assigns the
    `universeFlagRaceNeeded` delta to it, review note for 0x0053c870), `GetValidRaceForShipType`.
  - IDENTICAL-low at HEAD, MODIFIED-high after the re-read: `CreateMilitaryGroupAroundShip`,
    `universeDefaultTeamColors`, `GetShipStaticInfo`.
  - **Finding (map):** `colPickScalarScan` 0x0043dea0 and `SetInfoNeededForShipAndRelatedStaticInfo` 0x004a29f0 are
    IDENTICAL (medium) and outside the re-read's 1,013 targets (`original/scratch-m2/reread/targets.json`), but the
    exe differs from HW1 beyond renumbering (new sscanf format and race folding; six missile carriers and no
    DDDFrigate/Drone branch). The port's edits match the exe; the map should say MODIFIED. The producer did not flag
    the conflict. Added to the producer doc ("What is left" 6, [audit]); `delta_map.json` not edited (the re-read
    stage owns it while it runs).
  - `crateInit` 0x00462080 is IDENTICAL (medium); its edit only replaces HW1's `<= StandardFrigate` bound by the
    exe's 0x2e (= `bDFGFrigate + 1`), a renumbering, so no finding.
- **CAT comments.** Every hunk carries a `CAT:`/`CAT-PROVISIONAL:` comment except 7 one-line renames
  `ShipStaticInfoR1[0]` -> `ShipStaticInfos[0]` in the `preciseSelection` rows of the asteroid, dust cloud, nebula,
  gas cloud, derelict, missile and mine script tables. They follow from the array change, whose declaration carries
  the CAT comment (Universe.c:297). Minor; not changed.

## 4. DONE WHEN, re-run

Isolation tree built by me: `tools/m2_harness_iso.sh iso-audit-ident 3,4,5,6,7,8,13,14`; `readlink -f` confirms
`aiship`, `tweak` (update copy), `gatherstatsfor`, `crates`, `colpick`, `tactical`, `tactics`, `unitlimitcaps`
and `parade_smothership` resolve to `original/game_extracted/`, and `dock.script`/`parade_r1mothership.script` are
absent.

| run | data | result |
|---|---|---|
| `audit-ident-gate-sp` | plain harness | rc 213, milestone `process started`, first stop `Unable to open file: HW_Music.wxd` (rank 9, other item), 0 ASan |
| `audit-ident-iso` | isolation, `/ignoreBigfiles /noSpeech` | rc 1, milestone `level load (mission file read)`, first stop `GenericInterceptor.c (205): Assertion of (interceptorstat != NULL)`, ASan errors 0, 187 file loads |
| `audit-ident-iso2` | same, after the audit's code fix | identical result |

In `audit-ident-iso`: `AIShip.script`, `Tweak.script`, `GatherStatsFor.script`, `Crates.script`, `colPick.script`,
`Tactical.script`, `Tactics.script`, `UnitLimitCaps.script`, `UnitLimitCaps2.script` and the 12 `Parade_*.script`
load (FileLoads.log); `feScreenPush: pushed screen 1` (run.log:1397), `New single player game started` (1539),
`player is race 0` (1678); `Mission01.mission` is read 3 times (its lines 22-26 are `ExcludeShips RACE_*,All`); the last
loads are `RACE_SECT/sRecon.shp` and its LODs. So ranks 2, 3, 5 and 7 are gone, both ASan writes are gone, and
level load is past the run27 `RACE_*` stop; the next stop is the ship StaticInit item, as the producer says.
UBSan: only the known misaligned `fescreen`/`PATCH` accesses and the `/noSpeech` `MUSICHEADER` NULL access.

## 5. Other finding (omission, documented, not fixed)

**Mine statics are still loaded for races 0-1 only.** The port resized `mineStaticInfos` to `[NUM_RACES]` and
flags `mineStaticInfos[RACE_Taiidan]` for `tMinelayerCorvette`, but `universeStaticInit` (Universe.c ~3747),
`universeSaveEverythingNeeded` (~3325), `universeLoadEverythingNeeded` (~3404) keep `shiprace<2`, and
`universeNoStaticLoadedOrNeeded`/`universeDefaultTeamColors` keep `shiprace <= R2`. A Taiidan minelayer's mine
statics would be flagged but never loaded. The exe walks all 7 entries in universeStaticInit 0x0053d2a0,
universeSaveEverythingNeeded 0x0053ce80 and universeLoadEverythingNeeded 0x0053d090 (0x00b3ac10..0x00b3b444, 300
bytes each), while universeStaticCloseAllStatics 0x0053e0d0 walks only 2 (0x00b3abe4..0x00b3ae3c). Not reached by
any run yet (level load stops earlier). Several functions and an exe quirk are involved, so it is left to stage 2
and recorded in the producer doc ("What is left" 6, [audit]).

## 6. Fixes made (4 factual errors)

1. `src/Game/Universe.c` `GetShipStaticInfoSafe`: range test `(udword)shiptype >= TOTAL_NUM_SHIPS`, matching the exe's
   unsigned compare and HW1's NULL for negative types (comment `[audit: ...]`). Doc row updated.
2. Cap table address 0x008b1e54 -> 0x008b1e58: `src/Game/Universe.c` comment, doc rows `tables` and `scripts-ship`.
3. `StatIndexRaceOffsets` address 0x008961bc -> 0x008961b8: `src/Game/Stats.c` comment, doc `tables` row.
4. `mineStaticInfos` stride 1200 -> 300 bytes: doc `scripts-race` row.

Also added to the producer doc: the mine-statics omission and the two delta-map misclassifications ([audit] items).
After the fixes: `tools/build.sh --debug` and `--release` built with 0 warnings; the clean audit build dir rebuilt
with 0 warnings; the isolation run is unchanged.

Verdict: PASS-WITH-FIXES - every exe-table claim checked holds and the DONE WHEN reproduces; 4 factual errors fixed (one signed range check, three addresses/strides); the mine-statics loops left at 2 races are recorded for stage 2
