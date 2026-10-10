<!-- done: harness, enums, tables, scripts-ship, scripts-race, runs -->
# M2-R2 port: Cataclysm identifiers and tweak tables (producer 2)

Status: COMPLETE (2026-10-10), not committed. Branch `m2`, worktree `.claude/worktrees/m2`. Scope: docs/m2-r1-summary.md
"M2-R2 plan", producer 2. Subsystems (done marker order): `harness`, `enums`, `tables`, `scripts-ship`, `scripts-race`,
`runs`.

**Result.**
- `src/` now uses Cataclysm's 145 ship types, 7 races and 12 classes with the exe's ids and names. Every value was
  read back from `cataclysm.exe`'s own tables, and every table indexed by type, race or class was re-keyed: from the
  exe where the table was found (16 tables), marked `CAT-PROVISIONAL` where it was not (7 tables, mostly UI).
- Script tables ported: `aiship` (no code change needed), `tweak` hyperspace rows, `crates` (sizes, init, class
  lists), `Tactical` (12 classes), `Tactics` (type-keyed dodge and kamikaze rows), the 12 per-ship parade files and
  the parade choice, `Dock.script` dropped, `GatherStatsFor` (5 stat races, 98 indices), `colPick` (new format and race
  folding), `UnitLimitCaps` keys, `.mission` `ExcludeShips`/`IncludeShips` (races and the related missile/mine statics).
- `tools/build.sh --debug` and `--release`: both build, 0 warnings, 0 errors under `-Werror`.
- **Isolation harness** (data-deltas tree with this item's 8 workaround stages undone): the stops ranked 2, 3, 5 and
  7 are gone, ASan reports 0 errors (both out-of-bounds writes gone), the front end pushes `Main_game_screen`, single
  player starts, and level load parses `Mission01.mission` past the run27 `RACE_*` assertion. The next stop is
  `GenericInterceptor.c (205)`: `GenericInterceptorStaticInit` has no static for Cataclysm's `sRecon`. That is the
  ship-statics item (producer 3, `CustShipHeader` slot bodies), see "What is left".
- **Plain harness** (the unmodified install, the gate): first stop `Unable to open file: HW_Music.wxd`, data-deltas
  rank 9, owned by producer 4. Rank 1 (mesh, producer 1) follows. So the stops this item removed cannot be seen in the
  plain harness until those two items land.

## Test harness

- `tools/m2_harness.sh --setup` creates `original/scratch-m2/m2r2-harness/data/` (gitignored, outside the worktree):
  symlinks to the data entries of the GOG install's own `MANIFEST.sizes` under the install's names
  (`cataclysm.big`, `update.big`, `Music4Cat.wxd`, `SectSpeech.vce`, `BeastSpeech.vce`, `default_keys.cfg`,
  `devstats.dat`, `credits/`, `feman/`, `Movies/`) and real empty `SavedGames/{MultiPlayer,RecordedGames,
  SinglePlayer,Training}`, `ScreenShots/`, `MultiPlayer/` (so nothing can be written into `original/game`). No
  executables, no CET files (`update102.big` is CET's, docs/m0-first-run.md, so it is left out), none of the 15
  data-deltas workaround stages, no mesh down-conversion.
- `tools/m2_harness.sh <label> [--mode sp|fe] [--timeout N] [--driver offscreen|dummy] [-- engine args]` runs
  `build/debug/hwi-engine /window /nodebugInt /logFileLoads /settingspath <run>/settings [/debugScreenshots]` with a
  fresh `HOME` copied from `m2r2-harness/home-template/` (`TutorialNeeded 0`, `PlayIntros 0`; the GoK build reads
  `Homeworld.cfg` from `$HOME/.homeworld` only, data-deltas section 4), `stdbuf -oL`, ASan/UBSan logs to the run dir,
  `timeout -s TERM -k 5`. Offscreen first; it retries with `dummy` only if the offscreen driver cannot set a video
  mode. Output: `m2r2-harness/runs/<label>/{run.log,FileLoads.log,asan.*,ubsan.*,summary.txt}` plus a line in
  `runs/runs.txt`; it prints the furthest milestone (process started, archives opened, front end module started, engine trail module started, `Main_game_screen`, single player started, level load, universe running), the first stop (first fatal error, failed assertion or ASan report) and, separately, the first UBSan report other than the known misaligned `fescreen`/`PATCH` accesses (most UBSan reports do not stop the engine; run ident-r2-iso ended right after one). Mode `sp` (default) uses GoK's
  `/debugScreenshots`, which starts single player by itself (no input), as data-deltas run24/run27 did.
- `tools/m2_harness_iso.sh <name> <stages>` builds an **isolation** tree (`m2r2-harness/<name>/`) from the
  data-deltas `hw1-tree` and undoes the listed workaround stages. It exists only to test one item while other
  stage-1 items (mesh, file names, speech) are unported; `m2_harness.sh --data` marks such runs as ISOLATION in the
  summary. They are never the gate.

Baseline (`ident-r0-baseline`, unmodified tree, before any change here): milestone `process started`, first stop
`Unable to open file: HW_Music.wxd` (data-deltas rank 9, producer 4). Isolation baseline (`ident-r0-iso`, stages
3-8, 13, 14 undone, `/ignoreBigfiles /noSpeech`): first stop `AIShip.c (475): Fatal error - Invalid shiptype read at
line sRecon 0 0 in aiship.script`, the data-deltas run3 stop.

## Enums (`enums`)

Every value was checked against the exe's own tables, read from `original/game/cataclysm.exe` with a small read-only
PE reader (`original/scratch-m2/m2r2-ident-tools/pe.py`, outside git, with `syms.py`, `dec.py` and the other helpers used here; they expect to be copied to `/tmp/m2r2-ident/` and run from there): `shiptypeinfo` 0x00889530 has 145 `{uword, char*}` rows, `sRecon`
0 ... `nisLensflareSource1` 144, identical in name and value to CET `game/Game/ShipDefs.h`; `shipraceinfo` 0x008899c0
has `RACE_SECT` 0 ... `RACE_DERELICT` 6 (note the exe spells `RACE_TAIDAN`; CET's macro is `RACE_Taiidan`);
`nisraceinfo` 0x00889a00 has those 7 plus `Asteroids` 7 ... `Missile` 14; `shipclassinfo` 0x00889a80 has
`CLASS_Mothership` 0 ... `CLASS_Micro` 11; `FirstShipTypeOfRace` 0x008894e8 = {0,33,60,74,92,98,109} and
`LastShipTypeOfRace` 0x00889504 = {32,59,73,91,97,108,144}.

| file | change | exe evidence |
|---|---|---|
| `src/Ships/ShipDefs.h` | `ShipType_enum` is now Cataclysm's 145 types with CET's ids; race-range macros `SECT_/BEAST_/KUSHAN_/TAIIDAN_/DERELICT_FIRST/LAST_SHIP`; `STD_FIRST_SHIP sRecon`, `STD_LAST_SHIP bMissileCorvette` (CET); `P1_*`, `TRADERS_*` re-pointed; `TOTAL_NUM_SHIPS` 145; `P2_*`/`P3_*`/`TOTAL_OTHER_SHIPS` removed | `shiptypeinfo` 0x00889530; `GetShipStaticInfoSafe` 0x0053ea70 tests `type <= 0x90` |
| same | **HW1 names kept as aliases** so that unported HW1 code compiles: the 37 HW1 types with a Cataclysm counterpart alias ONE counterpart (rule: k* else t* else s* else b* else the P1/Traders ship; e.g. `Carrier = kCarrier`, `Mothership = sMothership`, `ResourceCollector = tResourceCollector`, `CloakedFighter = bCloakedFighter`); the 22 dropped HW1 types (`DDDFrigate`, `Probe`, `ResearchShip`, `ResourceController`, `Drone`, all P2/P3, ...) get ids 200-221, which `StrToShipType` can never return, so HW1 code keyed on them is dormant | transitional, documented in the header; stage 2 replaces each use per function |
| `src/Game/RaceDefs.h` | `RACE_Sect`..`RACE_Derelict` 0-6, `NUM_RACES` 7. HW1 names kept: `R1`=`RACE_Sect` (0) and `R2`=`RACE_Beast` (1), the playable races keep their ids; `P1`=`RACE_P1` (4), `Traders`=`RACE_Traders` (5); `P2`=16, `P3`=17 (above every race and `NSR_*` value, so `case P2:` labels stay distinct and no data produces them) | `shipraceinfo` 0x008899c0; CET `RaceDefs.h` |
| `src/Game/ClassDefs.h` | `CLASS_MonsterShip` 9, `CLASS_Component` 10, `CLASS_Micro` 11, `NUM_CLASSES` 12 | `shipclassinfo` 0x00889a80; CET `ClassDefs.h` |
| `src/Game/ObjTypes.c` | `FirstShipTypeOfRace`/`LastShipTypeOfRace`/`NumShipTypesInRace` in Cataclysm race order (7 entries); `shiptypeinfo` = the 145 names; `shipraceinfo` and `nisraceinfo` = the `RACE_*` strings (exe spelling); `shipclassinfo` = 12 classes. `StrToShipType`, `StrToShipRace`, `StrToShipClass`, `ShipTypeToStr`, `ShipRaceToStr`, `StrToNisRace` bodies unchanged (the exe's are the same `StrToNum`/`NumToStr` walks) | `StrToShipType` 0x004d6e30, `ShipTypeToStr` 0x004d6bc0, `StrToShipRace` 0x004d6db0, `ShipRaceToStr` 0x004d6b90, `StrToShipClass` 0x004d6d70, `ShipClassToStr` 0x004d6b40, `StrToNisRace` 0x004d6bf0 |

Not changed, on purpose: `ShipTypeToNiceStr` (`strGetString(type + strShipOffset)`). The exe's version 0x004d6c30
is `MessageStrings[type]`, i.e. the first 145 string ids are the Cataclysm ship names (`strsRecon`, ... looked up by
name through `LanguageStrings`, 1,705 rows, `strNameToEnum` 0x005288f0). Porting the string-id enum is a separate
item (226 HW1 ids gone, 1,318 new; data-deltas 2.5); until then ship names show other strings (no crash: every id
stays below `NumStrings`).

## Per-type and per-race tables (`tables`)

Tables found in the exe were copied value for value; the others are marked `CAT-PROVISIONAL` in the source.

| file | table | source |
|---|---|---|
| `src/Game/Universe.c` | `RacesAllowedForGivenShip[145]`: each type's own race bit, except `trGhostship` (Sect), `dAcolyte` (Traders), `nisLightSource1/2`, `nisLensflareSource1` (0) | exe 0x008b1814 (read by `aiuCanBuildShipType` 0x00412cb0) |
| same | `ShipStaticInfoR1/R2/P1/P2/P3/Traders` + `RaceShipStaticInfos` replaced by one `ShipStaticInfos[TOTAL_NUM_SHIPS]`; `GetShipStaticInfo(type, race)` returns `&ShipStaticInfos[type]` (race ignored, parameter kept for the unported callers); `GetShipStaticInfoSafe` checks only the type range and `IF_InfoLoaded` [audit: the range test is now unsigned, `(udword)shiptype >= TOTAL_NUM_SHIPS`, as the exe's `cmp ecx,0x91; jb`; the signed test let a negative type index `ShipStaticInfos[-1]`]; `ShipStaticScriptTable` template base renamed to `ShipStaticInfos[0]` | `ShipStaticInfos` 0x00b3b420 (145 x 0x458); `GetShipStaticInfo` 0x0053ea50; `GetShipStaticInfoSafe` 0x0053ea70 |
| same | `shipHeaders[145]`: the exe's header per type where HW1 has that header (GenericInterceptor, Carrier, Mothership, StandardDestroyer, ..., DoNothingShip, StationaryGun); the 22 Cataclysm-only headers (Acolyte, Mimic, Leech, ACV, MCV, RammingFrigate, MultiBeamFrigate, HiveFrigate, Swarmer, Dreadnought, ModuleShip, Worker, RepairDroid, Processor, Sentinel, CruiseMissile, RKO, Slipgate, Crystal, dBeastMothership, NomadMoon, NomadFieldEmitter) point at `DefaultShipHeader` until stage 2; `kCloakGenerator`/`tCloakGenerator` are NULL in the exe (`InitStatShipInfo` 0x0053bab0 copies `*shipHeaders[type]` unconditionally, so the exe would crash if it ever loaded them) and use `DefaultShipHeader` here | exe 0x008b1938 |
| same | `rowAvoidBy[145]`, `rightOfWays[145]` | exe 0x008b1b7c, 0x008b1dc0 (the latter named in `InitStatShipInfo`, docs/formats/shipstatic.md) |
| same | `ShipStaticCapLimits`: the exe's 123 rows (`ShipTotalCaps`, 110 per-type keys such as `CapsRecon`, `CapbCarrier`, `CapkCarrier`, `CaptrGhostShip`, `CapdAcolyte`, the 12 `CapCLASS_*`); HW1's 59 type keys and the 9 `MaxJobsPerClass[...]` rows are gone (no Cataclysm `UnitLimitCaps*.script` has them) | exe 0x008b1e58 [audit: was 0x008b1e54; scriptSet passes &PTR_s_ShipTotalCaps_008b1e58] (12-byte `scriptEntry` rows, `cdLimitCaps` 0x00ba03c0, `cdClassCaps` 0x00b3aba0) |
| same | `GetValidRaceForShipType`: first race whose bit is set, over all 7 races (HW1 tested its 6 races one by one) | no exe counterpart found; the exe derives a type's race from `LastShipTypeOfRace` (`cat_ShipTypeToRace` 0x004d7170) |
| same | `universeDefaultTeamColors`: the `racePlayerTable[P3][6]`/`[P2][5]` writes dropped (P2/P3 are not races any more, the table is `[7][8]`) | `universeDefaultTeamColors` 0x0053c940 (its other Cataclysm changes are stage 2) |
| `src/Game/Stats.c`, `Stats.h` | GatherStatsFor: 5 stat races = Sect, Beast, Kushan, Taiidan, P1; `StatIndexRaceOffsets` = their first type ids, so the stat index is the type id; per-race flag arrays and the 5x5 race table with the exe's initial values; `NUM_SHIPS_TO_GATHER_STATS_FOR` = 98 | `StatsForRaceCB` 0x00523fb0 (5x5 at 0x008961cc), `StatsForCB` 0x00524030 (pointers 0x00896254 -> 0x008961e8/0x0089620c/0x00896228/0x00896238/0x0089624c), `StatIndexRaceOffsets` 0x008961b8 [audit: was 0x008961bc, which reads {33,60,74,92,257}] = {0,33,60,74,92}; `statsLoadFightStats` 0x00524a50 expects `statfile.bin` of 0x41a70 = 98*98*28 bytes and loops 0x62 = 98 |
| `src/Game/Battle.c` | `battleClassDependencies[12]`: three zeros appended | exe 0x00750710 = {0,3,3,3,3,0,0,2,0,0,0,0} |
| `src/Game/ConsMgr.c` | `cmClassCapRemap[12]` (new classes map to themselves), `cmMaxJobsPerClass[12]` (new classes take the NonCombat limit) | **CAT-PROVISIONAL**: not located in the exe |
| `src/Game/Camera.c` | `PilotOffsets[145]`, `PilotOffsetsR2[145]` | **CAT-PROVISIONAL**: no such table found (Cataclysm reads `pilotViewOffsetX/Y/Z` from the `.shp`); HW1 counterpart's value, 0.5f (the function's default) for Cataclysm-only types |
| `src/Game/Sensors.c` | `smShipTypeRenderFlags[145]` | **CAT-PROVISIONAL**: HW1 counterpart's flags, 0 for Cataclysm-only types |
| `src/SDL/mainrgn.c` | `mrMenuActionsByShipType[145]` | **CAT-PROVISIONAL**: HW1 counterpart's menu, `MAM_ExtSet` for Cataclysm-only types |

The provisional tables are generated by `m2r2-ident-tools/prov.py` from the HW1 table and the data-deltas stem match
(`original/scratch-m2/static/shiptypes.json`); every row carries its source in a comment.

Build after `enums` + `tables`: `tools/build.sh --debug` and `--release` both `built:`, 0 warnings, 0 errors under
`-Werror`. Isolation run `ident-r1-iso`: the run3 (`aiship.script`) fatal and the run4 ASan write (`tweak.script`
hyperspace rows) are gone with no source change to their parsers (`StrToShipType` now knows the names, and
`TW_HyperSpaceCostStruct` is 145 long); next stop `fileOpen: cannot open file Dock.script` (data-deltas rank 7).

Static asserts added (`src/Game/ObjTypes.c`, `src/Game/Stats.c`): `TOTAL_NUM_SHIPS == 145`, `NUM_RACES == 7`,
`NUM_CLASSES == 12`, the seven race ranges, `NSR_Asteroid == 7`/`NSR_Missile == 14`, the HW1-only ids above every
table and below 256 (they still fit the `ubyte` list in `Gun.c`), `NUM_SHIPS_TO_GATHER_STATS_FOR == 98`,
`sizeof(FightStats) == 28`.

## Ship-keyed script tables (`scripts-ship`)

| script / table | file | change | exe evidence |
|---|---|---|---|
| `aiship.script` `rowDetails` | none | the parser (`rowSetDetails`, AIShip.c) is unchanged; the run3 fatal went away because `StrToShipType` knows `sRecon` etc. and `TOTAL_NUM_SHIPS` is 145 | `rowSetDetails` 0x00414090 IDENTICAL in `ghidra/delta_map.json` |
| `tweak.script` `hyperspace` | none (beyond `TW_HyperSpaceCostStruct[TOTAL_NUM_SHIPS]`) | the 19 rows now land in a 145-entry table: the run4 ASan write (index -1) is gone | `scriptSetTweakableGlobals` 0x00527820 clears `canMpHyperspace` in 145 20-byte rows from 0x00ba1fd0 to 0x00ba2b24 |
| `Dock.script` | `src/Game/StatScript.c` | no longer read (Cataclysm ships none); `DockTweaks` keep their compiled defaults | `scriptSetTweakableGlobals` 0x00527820 goes from `Flightman.script` to `Soundevent.script` |
| `crates.script` | `src/Game/Crates.c` | `SHIP_PROBS` and `NUM_IN_GROUPS` sized `TOTAL_NUM_SHIPS` (were `[StandardFrigate+1]` = 31, the run7 ASan write); `crateInit` resets the first 0x2e = 46 entries as the exe does; the 8 class lists are Cataclysm's (`sDreadnought, bHeavyCruiser` / `sCarrier, bCarrier` / `bIonArrayFrigate, sDestroyer` / 8 frigates / 5 corvettes / 9 strike craft / 4 resourcers / `sSentinel, bSentinel`) | `crateInit` 0x00462080 (46-entry loops, `NUM_IN_CLASSES` = 0,2,2,2,8,5,9,4,2); lists at 0x00878934..0x008789bb; `SHIP_PROBS` 0x00c43400 spans 0x180 bytes |
| `Tactical.script` | none (beyond `NUM_CLASSES` 12) | `CLASS_Micro` (11) now passes `toCurrentClass < NUM_CLASSES+1`; `CLASS_Mine` maps to 12 | `toNewClassSet` 0x005289a0 sets 0xc for `CLASS_Mine`; `toStartup` 0x00528920 fills 0xd = 13 icons |
| `Tactics.script` `DodgeInfo[...][type]`, `kamikazeDamage[type]` | `src/Game/Tactics.c` | HW1's 72 dodge rows and 18 kamikaze rows (HW1 names) replaced by the exe's 116 dodge rows (29 Cataclysm types x 4) and 29 kamikaze rows; the rest of the table (HW1's `RetreatDistance`, `ManeuvBonus`, fuel keys ... which Cataclysm dropped or changed) is left for the tactics port | `TacticsInfotable` 0x008a80a0 (rows 0x008a81c0.. and 0x008a8754..0x008a88a4) |
| 12 parade files | `src/Game/Formation.c` | 12 parade types in the exe's order (`Parade_sMothership`, `bMothership`, `sCarrier`, `bCarrier`, `Slipgate`, `sProcessor`, `bProcessor`, `dPrisonShip`, `R1Carrier`, `R2Carrier`, `P1Mothership`, `bHeavyCruiser`); HW1's `Parade_R1Mothership/R2Mothership/Carrier/P2Mothership.script` are gone; `CreateMilitaryGroupAroundShip` picks the parade by ship type with the exe's switch (23 types). HW1's 41-slot parade table is kept, so Cataclysm-only slot keys (`SLOT_SRECON`...) are ignored on load | `paradeSetTweakables` 0x004850d0 (12 x 0x6E4 bytes from 0x00a3e448); `CreateMilitaryGroupAroundShip` 0x00486050; slot table 0x0087fb68 (63 rows) |
| `UnitLimitCaps*.script` | `src/Game/Universe.c` | see `tables` (`ShipStaticCapLimits`) | 0x008b1e58 [audit: was 0x008b1e54] |
| launch list | `src/Game/LaunchMgr.c` | `PrintOrder` = the exe's list (12 strike craft, 9 corvettes/workers, spacers); `LM_NUMSHIPS` 23. Found because HW1's list wrote `lmShipInList[203]` (UBSan, run `ident-r3-iso`) | `PrintOrder` 0x00883340 |
| info overlay | `src/Game/InfoOverlay.c` | **CAT-PROVISIONAL**: HW1's list entries with HW1-only ids are skipped when `lookupforlist` is built (it wrote index 203, run `ident-r2-iso`). Cataclysm replaced this overlay (`ioStartup` 0x004ab4b0 reads `MainOverlay.script`); stage 3 | |

## Race-keyed script tables (`scripts-race`)

| script / table | file | change | exe evidence |
|---|---|---|---|
| `GatherStatsFor.script` | `src/Game/Stats.c`, `Stats.h` | see `tables`; `RACE_SECT`..`RACE_P1` now parse to 0-4, inside the 5 stat races (run6 asserted `race < 5` on `RACE_*` = -1) | `StatsForRaceCB` 0x00523fb0, `StatsForCB` 0x00524030 |
| `colPick.script` | `src/Game/ColPick.c` | `colPickScalarScan` reads `"%s%f%f"` (HW1 `"%2s,%f,%f"`), and folds `RACE_KUSHAN` into the Sect curve and `RACE_TAIDAN` into the Beast curve; the curve table is 14 entries (7 races x 2) | `colPickScalarScan` 0x0043dea0 (format string 0x007530c8 `%s%f%f`, race 2 or 0 -> 0, 3 or 1 -> 1); `cpStartup` 0x00440690 walks 14 x 12 bytes from 0x00a2daa0 |
| `.mission` `ExcludeShips`/`IncludeShips` | `src/Game/LevelLoad.c` | `llExcludeShip` itself is unchanged (`RACE_*` parse through the new race table; the single-player R1/R2 swap is the exe's, inlined there). `SetInfoNeededForShipAndRelatedStaticInfo` now uses Cataclysm's missile carriers (`tMissileDestroyer`, `sDreadnought`, `p1MissileCorvette`, `sDestroyer`, `sAcolyte`, `bAcolyte`) and mine layer (`tMinelayerCorvette`); HW1's `DDDFrigate`/`Drone` pair is gone | `llExcludeShip` 0x004a2a80; `SetInfoNeededForShipAndRelatedStaticInfo` 0x004a29f0 (types 0x55, 10, 0x5f, 0x19, 1, 0x22; 0x54) |
| `universeRaceStaticsScan` (used by `ExcludeShips <race>,All`) | `src/Game/Universe.c` | missile statics for `tMissileDestroyer`, `sDreadnought`, `p1MissileCorvette`, `sDestroyer`; mine statics for `tMinelayerCorvette` in any race (HW1: `race < 2`) | `universeFlagRaceNeeded` 0x0053c870 (inlines the scan: types 0x55, 10, 0x5f, 0x19; 0x54) |
| `mineStaticInfos` | `src/Game/Universe.c`, `Universe.h` | `[NUM_RACES]` (HW1 `[2]`): `tMinelayerCorvette` is a Taiidan ship (race 3), so `mineStaticInfos[race]` would be out of bounds | `SetInfoNeededForShipAndRelatedStaticInfo` 0x004a29f0 indexes it by race with a 300-byte stride [audit: was "1200-byte"; the flag byte is indexed `[race * 300]` on a byte base, and universeStaticInit 0x0053d2a0 walks 7 entries of 300 bytes from 0x00b3ac10 to 0x00b3b444] |

## Harness runs (`runs`)

All runs: `build/debug/hwi-engine` (ASan + UBSan), SDL `offscreen` (it set a video mode in every run, so the `dummy`
fallback never triggered), no input, fresh `HOME`. Logs: `original/scratch-m2/m2r2-harness/runs/<label>/`, ledger
`runs/runs.txt`. "iso" = the isolation tree `m2r2-harness/iso-ident/` (`tools/m2_harness_iso.sh iso-ident
3,4,5,6,7,8,13,14`: the data-deltas `hw1-tree`, i.e. stages 0-2, 9-12 and the stage-15 mesh down-conversion of other
items, with this item's stages undone) and `-- /ignoreBigfiles /noSpeech`.

| label | data | change since previous | rc / wall | milestone | first stop |
|---|---|---|---|---|---|
| ident-r0-baseline | plain | none (bootstrap tree) | 213 / 9.1 s | process started | `Unable to open file: HW_Music.wxd` |
| ident-r0-iso | iso | none | 213 / 9.1 s | process started | `AIShip.c (475): Fatal error - Invalid shiptype read at line sRecon 0 0 in aiship.script` (= data-deltas run3) |
| ident-r1-iso | iso | `enums`, `tables` | 213 / 8.7 s | process started | `File.c (1364): fileOpen: cannot open file Dock.script` (rank 7); no ASan report, so the run4 `tweak.script` write is gone |
| ident-r2-iso | iso | `Dock.script`, parade, crates, colPick, mines | 1 / 9.7 s | archives opened (`ioStartup` runs before the "Starting front end module" line) | UBSan `InfoOverlay.c:312: index 203 out of bounds for type 'sdword [146]'`; the process ended right after it, rc 1 (HW1 list, fixed) |
| ident-r3-iso | iso | info overlay | 1 / 10.0 s | front end module started | UBSan `LaunchMgr.c:1468: index 203 out of bounds for type 'bool32 [145]'` (HW1 list, fixed) |
| ident-r4-iso | iso | `PrintOrder`, Tactics rows | 1 / 11.0 s | level load (mission file read) | the harness then reported the first UBSan line (`SpeechEvent.c:2666` NULL `MUSICHEADER`, non-fatal); the stop-detection was changed to report the first fatal line and the first UBSan line separately |
| ident-r4b-iso | iso | none (re-run with the corrected harness report) | 1 / 11.0 s | level load (mission file read) | `GenericInterceptor.c (205): Assertion of (interceptorstat != NULL) failed` |
| ident-gate-sp | plain | all | 213 / 8.9 s | process started | `Unable to open file: HW_Music.wxd` |
| ident-gate-fe | plain, `--mode fe` | all | 213 / 9.2 s | process started | `Unable to open file: HW_Music.wxd` |
| ident-r5-iso | iso | static asserts | 1 / 11.2 s | level load (mission file read) | same as r4b; ASan errors 0 |

What `ident-r5-iso` shows (run.log, FileLoads.log, 187 loads):
- All the scripts of this item load from the original Cataclysm files: `AIShip.script`, `Tweak.script` (the 1.01
  copy), `UnitLimitCaps.script`, `GatherStatsFor.script`, `Crates.script`, the 12 `Parade_*.script`, `colPick.script`,
  `Tactical.script`, `UnitLimitCaps2.script`.
- `feScreenPush: pushed screen 1` (`Main_game_screen`), `New single player game started`, `player is race 0`.
- `SinglePlayer/Mission01/Mission01.mission` is read 3 times plus its two `.missphere` files. Its `ExcludeShips
  RACE_P1,All` line 22, the run27 stop, and the `IncludeShips` lines after it now parse.
- Then the ship statics load: `DefaultShip/*`, then `RACE_SECT/sRecon.shp`, `.lod`, five `.peo` LODs, `.mex`. The
  `.shp` path `RACE_SECT\sRecon.shp` resolves now that `ShipRaceToStr` returns the `RACE_*` directory names.
- Then `GenericInterceptorStaticInit` asserts. `shipHeaders[sRecon]` is `GenericInterceptorHeader`, as in the exe, but
  HW1's StaticInit only knows HW1's interceptor types. The exe's version (0x005b1e70) switches over 14 Cataclysm
  types: 0, 1, 2, 0x21-0x25, 0x3d, 0x44, 0x4b, 0x52, 0x5d, 0x85.
- Sanitizers: 0 ASan errors. UBSan reports only the known misaligned `struct fescreen`/`struct PATCH` accesses
  (data-deltas 5.4) and `SpeechEvent.c:2666` NULL `MUSICHEADER`, which is the `/noSpeech` artefact data-deltas run20
  already showed (producer 4).

**DONE WHEN, item by item.**

| criterion | state | where shown |
|---|---|---|
| rank 2 stop (`aiship.script` fatal) gone | yes | ident-r1-iso onward |
| rank 3 stops (`GatherStatsFor` run6, `colPick` run14) gone | yes | `GatherStatsFor.script` loads from ident-r2-iso on, `colPick.script` from ident-r4-iso on (earlier runs stopped before the front end read it); no assertion |
| rank 5 stop (`Tactical.script` `CLASS_Micro`, run15) gone | yes | ident-r4-iso onward: `Tactical.script` loads, no assertion |
| rank 7 stops (`Dock.script` run5, `Parade_R1Mothership.script` run8) gone | yes | ident-r2-iso onward |
| ASan writes in `tweak.script` (run4) and `crates.script` (run7) gone | yes | 0 ASan errors in every iso run from ident-r1-iso on |
| level load past the run27 `RACE_*` stop | yes | ident-r4b-iso, ident-r5-iso: `Mission01.mission` parsed, ship statics loading |
| the above in the plain harness | **not observable yet** | the plain harness stops first at rank 9 (`HW_Music.wxd`, producer 4) and then would stop at rank 1 (mesh, producer 1); these runs used the isolation tree |

## What is left for later stages

1. **Next level-load blocker (producer 3 / stage 2): the ship StaticInit slot bodies.** `GenericInterceptorStaticInit`
   (exe 0x005b1e70) has a per-type static for 14 Cataclysm types. Other HW1 StaticInits that the exe's
   `shipHeaders` reuses will pick HW1 per-race statics or assert in the same way (GenericDefender, Carrier,
   Mothership, ...). The 22 Cataclysm-only headers point at `DefaultShipHeader` until their code is ported.
2. **The HW1 type aliases** (`Carrier = kCarrier`, `Mothership = sMothership`, ...) are transitional. Each HW1 test of a
   type name sees one race variant only. Stage 2 replaces them function by function: `grep -w` on the 37 alias names
   finds them, 1,387 source lines outside ShipDefs.h.
3. **HW1-only type ids (200-221) used as constants at run time.** No data produces them, but some unported code
   still uses them:
   - the HW1 AI requests `LightInterceptor`, `Probe`, `DDDFrigate`, `ResourceController`, ... by constant
     (AIOrders.c, AIHandler.c, AIFleetMan.c, AIMoves.c): stage 4;
   - `Ships/Mothership.c` script keys `specialDoorOffset[P2][...]`: address constants that no Cataclysm script writes;
   - `utility.c`/`LevelLoad.c` `Drone` lookups, guarded by `DDDFrigate`.

   Any of these that runs would index a `TOTAL_NUM_SHIPS` table out of bounds. UBSan catches it in the debug build,
   as it did for InfoOverlay and LaunchMgr.
4. **String ids:** `ShipTypeToNiceStr` must become `MessageStrings[type]` once the 1,705-entry Cataclysm string enum is
   ported (`strNameToEnum` 0x005288f0, `LanguageStrings`). Until then ship names show unrelated strings.
5. **CAT-PROVISIONAL tables** (stage 3, exe copies not located): `PilotOffsets`, `PilotOffsetsR2`,
   `smShipTypeRenderFlags`, `mrMenuActionsByShipType`, `cmClassCapRemap`, `cmMaxJobsPerClass`, the InfoOverlay list.
6. **Tables ported only as far as this item's keys reach:**
   - the parade slot enum (63 slots in the exe, 41 in HW1) and its slot assignment;
   - the rest of `TacticsInfotable` (Cataclysm dropped `RetreatDistance`, `ManeuvBonus`, ... and its
     `RetreatGetAwayDistance[CLASS_MonsterShip]` row writes the Corvette slot, an exe quirk to keep when ported);
   - `cratesRewardPlayer` (rewritten in the exe);
   - `universeDefaultTeamColors` (per-race defaults);
   - the Stats fight code (`statsGatherFightStats` REWRITTEN);
   - ETG's per-race script list (`etgScriptFilename`, still HW1's `R1.script`...; Cataclysm has `etg\race_*.script`).
   - [audit: the mine statics. `mineStaticInfos` is `[NUM_RACES]` now and `tMinelayerCorvette` flags
     `mineStaticInfos[RACE_Taiidan]` as needed, but universeStaticInit, universeSaveEverythingNeeded,
     universeLoadEverythingNeeded, universeNoStaticLoadedOrNeeded (`shiprace <= R2`) and universeDefaultTeamColors
     (`shiprace <= R2`) still handle races 0-1 only, so a Taiidan minelayer's mine statics are flagged but never
     loaded. The exe's universeStaticInit 0x0053d2a0, universeSaveEverythingNeeded 0x0053ce80 and
     universeLoadEverythingNeeded 0x0053d090 walk all 7 entries (0x00b3ac10..0x00b3b444, 300 bytes each);
     universeStaticCloseAllStatics 0x0053e0d0 still walks 2 (0x00b3abe4..0x00b3ae3c), an exe quirk to keep.]
   - [audit: `ghidra/delta_map.json` lists `colPickScalarScan` 0x0043dea0 and
     `SetInfoNeededForShipAndRelatedStaticInfo` 0x004a29f0 as IDENTICAL (medium); this port edits both, correctly
     (new `%s%f%f` format and race folding; six missile carriers and no DDDFrigate/Drone branch). They are not in the
     re-read's 1,013 targets; reclassify them MODIFIED.]
7. **Data-deltas stops of other items**, still in the plain harness:
   - rank 9 file names (producer 4);
   - rank 4 speech LUT and rank 6 battle chatter (producer 4);
   - rank 8 FE art (producer 4);
   - rank 1 mesh (producer 1).

## Counts

- Enum values checked against the exe: 145 ship types, 7 races, 15 NIS races, 12 classes, 7 + 7 race-range bounds.
- Per-type/race/class tables re-keyed from the exe: 16. They are `shiptypeinfo`, `shipraceinfo`, `nisraceinfo`,
  `shipclassinfo`, `First/LastShipTypeOfRace`, `RacesAllowedForGivenShip`, `shipHeaders`, `rowAvoidBy`, `rightOfWays`,
  `ShipStaticCapLimits`, the GatherStatsFor tables, `battleClassDependencies`, the crate lists, the Tactics type rows,
  the parade files and choice, and `PrintOrder`.
- Provisional: 7 (item 5 above).
- Files changed: 21 under `src/` plus 2 new tools (`tools/m2_harness.sh`, `tools/m2_harness_iso.sh`) and this report.
  Diff: +2,444 / -1,255 lines, mostly generated tables.
- Harness runs: 10 (2 plain gate, 1 plain baseline, 7 isolation).

## Open questions

1. Should HW1 code that names the 22 HW1-only types be deleted now rather than kept dormant? Deleting it is
   stage-2/4 work; keeping it dormant is what this round did.
2. `kCloakGenerator`/`tCloakGenerator` have NULL `shipHeaders` in the exe. This port uses `DefaultShipHeader` to avoid
   a crash the exe would also have if it ever loaded them. Is a faithful NULL wanted, guarded at the load site?
3. The exe resets only 46 entries of `SHIP_PROBS`/`NUM_IN_GROUPS`, but `crates.script` writes types up to 59. This
   port keeps the exe's count. The entries above 45 start at 0, as in the exe's `.bss`.

