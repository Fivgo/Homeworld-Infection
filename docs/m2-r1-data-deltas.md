<!-- done: big-inventory, formats-static, ship-types, run-headless, run-windowed, write-up -->
# M2-R1: Homeworld 1 engine vs Cataclysm data (data deltas)

Status: COMPLETE. [audit: audited 2026-10-10, docs/audits/m2-r1-data-deltas.md; 5 corrections marked inline, covering 3 wrong claims] Written 2026-10-10 for M2-R1 item 1 (cycle/M2_BRIEF.md).

**Bottom line.**
- The unmodified HW1 engine (GoK native debug build) stops on Cataclysm's data **17 times** before its first front-end frame. 16 of those stops can be worked around: 15 in a scratch directory with renames, empty stubs or row filters, and the speech crash with the engine's own `/noSpeech` switch. The 17th, the **mesh format (version 0x500, 116-byte polygon objects)**, cannot. It is fatal at startup because the engine-trail module loads 21 engine-glow meshes before the front end opens (section 5, run16).
- With every mesh down-converted to the HW1 layout by a scratch script (an experiment, section 5.3), the same engine **reaches its front end**. It loads all 10 `.fib` startup screens, pushes `Main_game_screen` and idles for 50 s with no ASan error. Started into single player, it stops in level load on Cataclysm's `RACE_*` names (run27).
- Two of the stops are **out-of-bounds writes** that ASan catches (`tweak.script` hyperspace rows, `crates.script` ship probabilities). A release build would silently corrupt globals there. Both come from Cataclysm ship names that HW1's `StrToShipType` maps to -1.
- What blocks the first frame, in order of porting cost: mesh 0x500 (port the Cataclysm loader), the 145-type ship enum and the 7-race enum (they feed 6 script tables), the speech LUT layout (SIGFPE), then missing or renamed files (section 6).

**Sources and conventions.**
- `GoK:` = `reference/gardens-of-kadesh/src/` (the tree the native build was made from: `reference/build-hwsdl/build.ninja` references `../gardens-of-kadesh` 2,795 times; compile flags include `-DHW_BUILD_FOR_DEBUGGING -DHW_PTR_64 -D_LINUX_FIX_ME -DHW_GAME_HOMEWORLD`, `-fsanitize=address,undefined`, `-O0`, from `reference/build-hwsdl/compile_commands.json`).
- `CET:` = `reference/CataclysmEngineTweaks/`.
- `[data]` = a measurement over `original/game_extracted/cataclysm/` (16,528 files, byte-identical to `biggie -x`, docs/big-archive-contents.md section 2) or `original/game/cataclysm.big`. The scripts used are copied to `original/scratch-m2/runs/data-deltas/tools/` (gitignored, outside the worktree; they ran from `/tmp/m2r1-tools/`). Their static outputs are in `original/scratch-m2/static/`.
- `[runN]` = a log line from the engine runs, `original/scratch-m2/runs/data-deltas/runN.log` (+ `runN.asan.*`, `runN.ubsan.*`, `runN.FileLoads.log`; ledger in `runs.txt`). Section 5 lists them.
- This document builds on docs/big-archive-contents.md (archive format, contents, header census) and does not repeat it. Two of that document's statements are corrected in section 2.2: `.peo` is not new in Cataclysm, and the mesh change is more than the version number.

---

## 1. Archive inventory: what HW1 opens vs what Cataclysm ships

### 1.1 Listing with HW1's own tool

`reference/build-hwsdl/tools/biggie/biggie -v original/game/cataclysm.big` reads the archive without complaint: `RBF1.23`, CRC-sorted TOC, 16,528 entries, totals 75,559,532 stored / 128,441,214 real bytes (ratio 0.588); `update.big` 39 entries, 241,947 / 710,998. Both totals equal docs/big-archive-contents.md section 3.

**Tool artefact found (affects every ASan-built HW1 binary, including the game).** Run plainly, biggie exits rc=1 with `LeakSanitizer: detected memory leaks ... 528896 byte(s) ... bigTOCRead ../gardens-of-kadesh/src/Game/BigFile.c:415` (the TOC, 16,528 x 32 bytes, never freed), and its redirected listing is **silently truncated**: the last 6 entries (`btg\bitmaps\bigstar.tga`, `galax4.tga`, `galax51.tga`, `galax52.tga`, `seanblu.tga`, `seanred.tga`) and the totals line are missing (16,522 of 16,528 rows). Cause: LSan reports at exit and terminates before stdio flushes the fully buffered stdout. With `ASAN_OPTIONS=detect_leaks=0` the listing is complete (16,528 rows, rc=0). The engine runs below therefore keep stdout line-buffered (`stdbuf -oL`) or disable leak detection, so no log tail is lost.

### 1.2 Container names (HW1 open code vs Cataclysm files)

| HW1 opens | where | Cataclysm ships | effect on the HW1 engine |
|---|---|---|---|
| `Homeworld.big` (required) | `bigFilePrecedence[]` GoK:Game/BigFile.c:72-91, fatal `Unable to find required .big file` :2515-2519 | `cataclysm.big` | fatal at startup unless renamed or symlinked. [run0c] `BigFile.c (2519): Fatal error - Unable to find required .big file: .../Homeworld.big` |
| `Update.big` (optional) | BigFile.c:74 | `update.big` (lower-case on disk) | skipped with only a warning unless symlinked as `Update.big`: the case-insensitive `fileExists` finds `update.big`, but the plain `fopen` of `Update.big` fails (BigFile.c:2524-2537). [run0c] `BigFile.c (2537): Warning - Unable to open optional .big file: .../update.big`. **The 1.01 patch would be lost without any error** |
| `Extra.big` (pad file, never opened) | `utyPadBigFilename` GoK:SDL/utility.c:213 | none | none. This explains the unreferenced `Extra.big` string in `cataclysm.exe` noted in docs/big-archive-contents.md 1.6: it is HW1's CD pad-file name, inherited |
| `HW_comp.vce` (speech) | `utyVoiceFilename` SDL/utility.c:209 | `SectSpeech.vce`, `BeastSpeech.vce` (two files) | **fatal startup check** (`utyGameSystemsPreInit`, utility.c:3726-3735). [run2] `Unable to open file: HW_comp.vce`. Two-race speech has no HW1 slot |
| `HW_Music.wxd` + `SoundFX/HW_Music.wxh` | utility.c:202, GoK:Game/SpeechEvent.c:249 | `Music4Cat.wxd` + `soundfx/music4cat.wxh` | **fatal startup check**, the first thing the engine tests, before it opens any archive (utility.c:3716-3723). [run0], [run1] `Unable to open file: HW_Music.wxd` |
| `SoundFX/speechsentence_comp.lut`, `speechphrase_comp.lut` | SpeechEvent.c:444, 471 | `sectsentence.lut`, `sectphrase.lut`, `beastsentence.lut`, `beastphrase.lut` | **fatal** at sound init [run9]; once renamed, the layout difference crashes the engine with SIGFPE [run10] (2.1) |
| `Homeworld.cfg` (settings) | SDL/utility.c:142 | `Cataclysm.cfg` (GOG install root) | HW1 reads and writes its own under `~/.homeworld` (writes can be redirected with `/settingspath`, reads cannot, section 4) |
| `AutoLock.txt` | utility.c:214 | present loose in the GOG install root | runtime lock file, no data |

Archive precedence logic, TOC layout, name hashing and LZSS are identical (docs/big-archive-contents.md 1.1-1.6); nothing else in the container needs a change.

### 1.3 HW1 path literals checked against the Cataclysm archive

Method: every string literal in `GoK:Game`, `GoK:SDL`, `GoK:Ships` (`*.c *.h *.cpp`, ThirdParty excluded) that ends in a data extension (645 occurrences, `original/scratch-m2/static/hw1-path-literals.txt`), minus format strings, normalised to lower case and `/`, looked up in the archive listing (`tools/lit_check.py`, output `static/lit-check.txt`). 461 distinct literals:

| result | count | meaning |
|---|---:|---|
| found at the same path | 72 | all root `*.script` tweak tables HW1 loads (AIPlayer ... Warp), `English/French/German.script`, the 12 [audit: was "11"; 12 names are listed here and `static/lit-check.txt` has 12 distinct found `.fib` names] `FEMan\*.fib` HW1 names (`Front_end`, `Sensors_manager`, `Research_Manager`, `In_game_ESC_menu`, `CSM-All`, `TaskBar`, `single_player_objective`, `Construction_Manager`, `Launch_Manager`, `Horse_Race`, `Hyperspace_Roll_Call`, `Trader_Interface`), `textures.ll`, `kver.kp`, `statfile.bin`, `hsf\default.hsf`, `BTG\default.btg`, `Movies\animatics.lst`, `demos\AutoDemos.list`, `ScreenShots\ShotList.script`, `ColorPicker\Race1|2\Race1|2.lif`, `feman\loadscreen\arrows|ring.lif`, `Parade_P1Mothership.script`, `Parade_R2Carrier.script` |
| basename found (HW1 prepends a directory at run time) | 240 | the font names (`fonts\english\*.hff`), all sound banks and event LUTs (`SoundFX\Guns.bnk`, `GunEvents.lut`, ...), `DefaultShip\DefaultShip.{shp,lod,geo,mex}`, ETG `general.script`, NIS `n02/n04/n06/n07/n09`, FE textures |
| not in Cataclysm at all | 149 | listed below, grouped |

The 149 missing literals, grouped by consequence:

| group | literals (GoK site) | consequence for the HW1 engine |
|---|---|---|
| archive/stream names | `Homeworld.big`, `Update.big` (case), `HW_comp.vce`, `HW_Music.wxd/.wxh`, `speechsentence_comp.lut`, `speechphrase_comp.lut`, `SpecEffectEvents.lut` (commented out in HW1, SoundEvent.c:2762) | 1.2 |
| race-keyed script names | ETG `R1.script`, `R2.script`, `P1.script`, `P2.script`, `P3.script`, `Traders.script` (ETG.c:765-770); `Parade_R1Mothership`, `Parade_R2Mothership`, `Parade_P2Mothership`, `Parade_Carrier` (`paradeSetTweakables`, Formation.c:2748-2762) | Cataclysm keys these by `race_*` or per ship (12 `parade_*.script`, 7 `etg\race_*.script`). The parade ones are **fatal** at startup: [run8] `Fatal error - fileOpen: cannot open file Parade_R1Mothership.script`. The ETG ones were not reached |
| removed tweak tables | `Dock.script` (StatScript.c:1875); `AIStrategy.script` (:1866, but its load is commented out in HW1 too); `etg.script` (ETG.h:212, a define) | a missing script is **fatal**: `scriptSet` -> `fileOpen` -> `dbgFatalf("fileOpen: cannot open file %s")` (File.c:1364). [run5] `Fatal error - fileOpen: cannot open file Dock.script` |
| speech EQ/delay/effect scripts | `eq_pilot1..3`, `delay_*` (5), `effect_*` (5) (SpeechEvent.c:352-381) | only `eq_fleet`/`eq_intel` ship; Cataclysm has no pilot speech actors (not reached: speech was disabled with `/noSpeech` after run10) |
| speech LUTs | `SoundFX\speechsentence_comp.lut`, `speechphrase_comp.lut` (SpeechEvent.c:441-472) | **fatal**: [run9] `File.c (1120): Fatal error - fileSizeGet: can't find file '.../SoundFX/speechsentence_comp.lut'`; renamed, the layout then crashes (2.1, run10) |
| front-end art | `FEMan\Construction_Manager\Build_race1_icon1.lif` (ConsMgr.c:588-599 builds `Build_race{1,2}_` + `icon1/icon2.lif`); `ResearchGUI\Race1\Research_icon.lif` (ResearchGUI.c:59, :2407) | **fatal** (`fileSizeGet`, File.c:1120), [run11], [run12]. Cataclysm ships `build_race1_icon1_{on,off,disabled}.lif` and `research_icon_{engineering,weapons,armour,hangar,special}_{on,off,disabled}.lif` instead |
| HW1 NIS | `n01r1`..`n12r2` race variants, `n02-demo`, `nis\Teaser` (SinglePlayer.c:3080-3112, SDL/utility.c:3197) | Cataclysm ships `n01`..`n11` + `nislet01..03` |
| research GUI art | 30 `*.lif` (`MassDrive1Kt.lif`, `IonWeapons.lif`, `Research_icon.lif` ... ResearchGUI.c:59, 270-299), `icon1/icon2.lif` (ConsMgr.c:598, LaunchMgr.c:218) | Cataclysm's `researchgui\` holds a different technology set (107 files) |
| ETG fallbacks | `ETG\defaultBlast.ebg`, `ETG\genericBOOM.ebg` (ETG.h:240-244) | no default effect if an effect is missing |
| HW1 ship files | `CloakedFighter.shp`, `MinelayerCorvette.shp` (demo stub list, Universe.c:2031), `head00..03.geo` (Particle.c:1884-1896) | see 3 |
| HW1 media | `Movies\sierra.bik`, `Movies\relicintro.bik` (avi.c:650-664), `SierraIntro.plug`, `BuyHomeworld0.plug` | `sierra.bik` is loose in `original/game/Movies/`; `relicintro` replaced by `bdogintro.bik` (Barking Dog) |
| missions | `Mission05_OEM.mission` | Cataclysm has `mission01..17` (+11 `_nisNN`) |
| languages | `Spanish.script`, `Italian.script` | also missing from Cataclysm's archive (docs/big-archive-contents.md 4.1) |
| logs/output only | `aiplayerlog.txt`, `netlog.txt`, `statlog.txt`, `syncDump.txt`, `uv.txt`, `debugMessages.txt`, `texturesUsage.list`, `most_recent_screenshot.jpg`, `testicle.fib` (a debug FE test), ... | written, not read; no effect |
| `FEMan\Sensors_manager_SingleP.fib` | commented out in `utyFrontEndFiles[]` (utility.c:230) | none |

### 1.4 Directories renamed or added

| HW1 path scheme | Cataclysm | HW1 consequence |
|---|---|---|
| `R1\`, `R2\`, `P1\`, `P2\`, `P3\`, `Traders\` + `<ShipType>.shp` (`ShipRaceToStr` + `ShipTypeToStr`, GoK:Game/Universe.c:2012-2027) | `race_sect\`, `race_beast\`, `race_kushan\`, `race_taidan\`, `race_p1\`, `race_traders\`, `race_derelict\` + race-prefixed names (`race_kushan\kcarrier.shp`) | **no HW1 ship file resolves.** `InitStatShipInfo` disables any ship whose `.shp` is missing (`!fileExists(...)` -> `SetInfoNeededForShipAndRelatedStaticInfo(FALSE)`, `rmEnableShip(FALSE)`, Universe.c:2040-2044), so this is silent, not fatal |
| `R1\Missile.shp`, `R1\Mine.shp` per race (Universe.c:1872-1935) | `missile.shp` and `mine.shp` at each `race_*\` root (checked in race_sect, race_kushan, race_taidan) | the race directory name differs, so `WARNING: Could not find Missile for %s` (Universe.c:1887) is expected (not reached in a run) |
| `Derelicts\<DerelictType>.shp` (Universe.c:1804-1815) | `derelicts\` with 7 objects (crate, galaxy, homeworld, lavaball, mattworld, planetoforigin, planetoforigin_scarred); most derelicts became ship types in `race_derelict\` | HW1's derelict enum names mostly unresolved |
| `Resources\Asteroids|DustClouds|GasClouds|Nebulae\` (Universe.c:1569-1723) | same, no `gasclouds\` | gas clouds missing |
| `ETG\R1.script` ... | `etg\race_*.script` | 1.3 |
| `SoundFX\Scripts\` | `soundfx\scripts\` (case only) | none (archive lookups are case-insensitive) |
| (none) | `lensflares\`, `overlayicons\`, `race_derelict\`, `singleplayer\missionNN_nisMM\` (11), `tutorials\tutorial1\` | never requested by HW1 |

### 1.5 Cataclysm files HW1 never asks for

By type (archive counts from docs/big-archive-contents.md 3.2; HW1 reference check: `grep -rli "\.<ext>\b" GoK:Game GoK:SDL`):
- **`.missphere` (128) and `.dist` (322)**: 0 GoK files mention either extension. They are named from inside `.mission`/`.level` files (`MissionSphere` lines), which HW1's `LevelLoad.c` parses with the same `MissionSphereScriptTable` keys (`Ships`, `AIPoint`, `AIPath`, `AIBox`, `AISphere`, `Resources`; LevelLoad.c:109-116), so they are only reached through a level load.
- **`.mif` (17, `texturedone.mif`)**: 0 GoK references; a tool artefact (UNVERIFIED that the game never reads it).
- **New scripts**: `mainoverlay.script`, `overlayicons\overlayicons.script`, `shipnames.script`, `countries.script`, `race_beast\beastweapon.script`, `lensflares\*.script`, 10 of the 12 `parade_*.script`, `unitlimitcaps1..8.script` (HW1 builds `UnitLimitCaps%i.script` by player count, Universe.c:4426, so those 8 ARE used), `etg\race_*.script`, `etg\derelict.script`, `mad.script`, `nis\n01..n11.script`, `nis\nislet01..03`.
- **Ship directories**: every `race_*` tree (16,528 - all non-ship files).

---

## 2. File formats: what HW1's loaders expect vs the bytes Cataclysm ships

Headers were read with `tools/hdrdump.py`, `geo_hdr.py`, `geo500.py` (all read-only over the extracted data).

### 2.1 Summary table

| format | HW1 expects (GoK) | Cataclysm ships [data] | where HW1 fails | evidence |
|---|---|---|---|---|
| `.big` | `RBF1.23`, 32-byte TOC (BigFile.c:404-418, 499-512) | identical | name only (1.2) | biggie -v listing, 16,528 rows |
| `.geo` mesh | `RMF97ba`/`RMF99ba`, `MESH_Version 0x402` (Mesh.h:51-54), 68-byte header, **112-byte** `polygonobject_disk` (Mesh.h:275-293) | `RMF97ba` v**0x500** (966/967), 68-byte header, **116-byte** polygon objects | `meshLoad` `dbgFatalf("meshLoad: Invalid version # %s.  Expected 0x%x, found 0x%x")` (Mesh.c:915-918; `MESH_ERROR_CHECKING` is 1 because the build defines `HW_BUILD_FOR_DEBUGGING`, Mesh.h:27-30). [run16], [run28] `Mesh.c (917): Fatal error - meshLoad: Invalid version # misc\EngineGlows\energybolt.geo.  Expected 0x402, found 0x500`, the first mesh the engine loads (engine-trail startup). A release build would skip the check and misparse every object after the first (2.2) | `sacolyte.geo` bytes 8-11 `00 05 00 00` |
| `.peo` mesh | the **paged** variant of a `.geo`: `meshLoad` substitutes `name.peo` for `name.geo` when `mainAllowPacking` and the file exists (`meshPagedName`/`meshPagedVersionExists`, Mesh.c:398-441, 870-873); ident `RMF99ba` sets `newFormat` (localSize 0, Mesh.c:921-938) and enables `meshObjectFixupPacoUV` | `RMF99ba` v0x500 (761/762), same 116-byte objects | same version fatal | `sacolyte.peo` bytes 0-11 |
| `.mex` | `mannngo`, `{char[8], u16 version, u16 nChunks}` | `mannngo`, version 0x106, 13 chunks in `sacolyte.mex` | none expected | header bytes `6d 61 6e 6e 6e 67 6f 00 06 01 0d 00` |
| `.lif` | `Willy 7`, 0x104 (GoK:SDL/texreg.h:115-116) | `Willy 7`, 0x104 (all 12,096, docs/big-archive-contents.md 2) | none expected | `page0.lif` bytes 0-11 |
| `textures.ll` | `Event13`, 0x104 (texreg.h:110-111) | `Event13`, 0x104 | none expected | bytes 0-11 |
| `.fib` | `Bananna`, 0x212 (FEFlow.h:40-41) | `Bananna`, 0x212, e.g. `front_end.fib` 26 screens | screens present; see 2.6 | header `42 61 6e 61 6e 6e 61 00 12 02 1a 00` |
| `.nis` | `Crannnberry`, 0x101 | same | NIS names differ (1.3) | `n01.nis` bytes 0-13 |
| `.btg` | first u32 0x600 | 0x600 | none expected | `default.btg` `00 06 00 00` |
| `.hsf` | `rrasberry`, 0x100 | same | none expected | `default.hsf` |
| SFX `.bnk` / event `.lut` | banks checked against their LUT `checksum` (SoundEvent.c:2736-2804 "Lookup tables do not match.  Not from same generate.") | `BNK0`/`PKG0` with a shared checksum word `0x40568380` (`guns.bnk`, `gunevents.lut`) | **none**: all 10 SFX LUTs and 4 banks load from the archive under their HW1 names and no checksum message is printed [run28 `FileLoads.log`, run28.log]. UBSan reports misaligned `struct PATCH` accesses in the 64-bit bank conversion (SoundEvent.c:2640-2654, soundlow.c:466-473); this is probably a GoK 64-bit artefact and not caused by the data (inference) | header bytes, run28 |
| speech `SENTENCELUT` (`*sentence.lut`) | `{sdword ID; udword checksum; uword numcolumns, numevents, numactors; uword compbitrate[4]; sword lookup[]}` (GoK:Game/SoundStructs.h:56-74) | `sectsentence.lut`: `LUT1`, checksum `0x0d692490` (equal to `SectSpeech.vce` bytes 4-7, so the voice-file check passes), numcolumns 4, numevents 863, **numactors 1**, then `512, 2, 4, 61, ...` | read with HW1's 4-entry `compbitrate[]`, entry 1 is `2`, so `(bitrate >> 3) * 4` is 0 and `soundstreamcreatebuffer` divides by zero: **SIGFPE** at GoK:SDL/sstream.c:245 <- `speechEventInit` SpeechEvent.c:546 [run10c, symbolised with addr2line]. Inference: Cataclysm's table has one `compbitrate` per actor (here 1), so the lookup array starts 6 bytes earlier than HW1 expects | header bytes, run10c |
| music header `.wxh` | `SoundFX\HW_Music.wxh` | `soundfx\music4cat.wxh` (`WXH1`) | not loaded with `/noSpeech`; `musicEventPlay` then dereferences a NULL `musicheader` (UBSan, SpeechEvent.c:2666) when the front end starts its music [run20] | run20 |
| `.shp` | text, `scriptStructEntry` tables; unknown keys ignored (`findStructEntry` returns NULL, `scriptSetStruct` StatScript.c:783-823); unknown enum strings map to -1 (`StrToNum`, ObjTypes.c:486-499) | 173 files, 323 distinct keys, 88 unknown to every HW1 table (2.3) | silent loss of the new fields; new enum values become -1 | `tools/shpkeys.py` |
| `.lod` | `lodScriptTable` keys `type0..5`, `pMeshFile0..5`, `mOn/bOn/mOff/bOff0..5`, `pointColor` (LOD.c:37-71) | same keys; `pMeshFileN` names `.peo` directly (e.g. `sAcolyte\rl0\lod0\sAcolyte.peo`) | none in the parser; `meshPagedName` maps `.peo` to itself | `race_sect/sacolyte.lod` |
| language `.script` | `scriptSet("", "English.script", LanguageStrings)` by key name, missing keys stay `*NO_STRING*` (StringSupport.c:18-23, 721-768) | 1,682 keys (1,694 in update.big) | 226 of HW1's 590 string ids missing (2.5) | `tools/strings_cmp.py` |
| tweak tables keyed by **ship name** (`aiship.script` `rowDetails`, `tweak.script` `hyperspace`, `crates.script` `shipProbability`/`shipGroupSize`) | `StrToShipType(name)` into `TOTAL_NUM_SHIPS`-sized arrays; only `rowSetDetails` range-checks (AIShip.c:461-479) | Cataclysm names (`sRecon`, `bCarrier`, `kCarrier`, `p1Fighter`, ...): 103 of 108 `rowDetails`, 19 of 19 `hyperspace`, 48 of 48 crate rows are unknown to HW1 | `aiship.script`: **fatal** [run3] `AIShip.c (475): Fatal error - Invalid shiptype read at line sRecon 0 0 in aiship.script`. `tweak.script` (1.01 copy): **ASan global-buffer-overflow WRITE** of 4 bytes 20 bytes before `TW_HyperSpaceCostStruct` in `scriptSetHyperspaceCostCB` StatScript.c:305 [run4]. `crates.script`: **ASan global-buffer-overflow WRITE** 4 bytes before `SHIP_PROBS` in `scriptSetShipProbCB` StatScript.c:289 [run7]. The last two write index -1 and would corrupt memory silently in a non-ASan build | run3, run4.asan, run7.asan |
| tweak tables keyed by **race name** (`GatherStatsFor.script` `StatsForRace`, `colPick.script`, `.mission` `ExcludeShips`/`IncludeShips`) | `StrToShipRace` over `R1 R2 P1 P2 P3 Traders` (ObjTypes.c:125-134) | `RACE_SECT`, `RACE_BEAST`, `RACE_KUSHAN`, `RACE_TAIDAN`, `RACE_P1`, `RACE_TRADERS`, `RACE_DERELICT` | **fatal asserts**: [run6] `Stats.c (208): Fatal error - Assertion of (race >= 0 && race < 5) failed.`; [run14] `ColPick.c (195): Fatal error - Assertion of (race >= 0 && race < (5 + 1)) failed.` (`colPick.script` is also whitespace-separated where HW1 scans `"%2s,%f,%f"`); [run27] `LevelLoad.c (1885): Fatal error - Assertion of (race != 0xffffffff) failed.` on `ExcludeShips RACE_P1,All` in `mission01.mission` | run6, run14, run27 |
| `BattleChatter.script` | event names from `battleChatterEvent[]` (`CHAT_Grp_PositiveBattle`, ..., Battle.c:410-422) | 11 event names: 2 exist in HW1 (`CHAT_Fighter_Wingman`, `CHAT_Docking_More`); 9 do not (`CHAT_Intel_Grp_PositiveBattle`, `..._NegativeBattle`, `..._NegativeBattleMore2`, `..._NeutralBattle`, `..._NeutralBattleMore`, `..._FriendlyFire`, `CHAT_Ship_Grp_PositiveBattleMore`, `CHAT_Ship_Grp_NegativeBattleMore`, `CHAT_Fighter_ManeuverGrunts`). New keys `battleStatusUpdate`/`battleStatusSuppress` | **fatal** [run13] `Battle.c (628): Fatal error - Unable to find battle chatter event 'CHAT_Intel_Grp_PositiveBattle'.` | run13 |
| `Tactical.script` icon classes | `StrToShipClass` over 9 classes + `CLASS_Mine` (ClassDefs.h:14-24) | adds `CLASS_Micro` (Cataclysm has 12 classes: + `MonsterShip` 9, `Component` 10, `Micro` 11, CET:game/Game/ClassDefs.h) | **fatal assert** [run15] `Tactical.c (152): Fatal error - Assertion of (toCurrentClass < ((8 + 1)+1)) failed.` | run15 |
| `.fib` link callbacks | names registered with `feCallbackAddMultiple` | `Main_game_screen` executes `Main_game_screen-TO-Select_tutorial` | non-fatal: [run20] `feFunctionExecute: function 'Main_game_screen-TO-Select_tutorial' not found`. Why it runs with no input is UNVERIFIED (probably a link the screen fires on start) | run20, run24 |

### 2.2 Mesh v0x500: the layout change, measured

All 1,727 version-0x500 `.geo`/`.peo` files obey one rule, and the HW1 layout fits none of them (`geo500.py`):
- The 68-byte header is unchanged in field order: `fileSize == file length - 68` in 967/967 `.geo` and 762/762 `.peo`; `oPublicMaterial` is a file offset; `reserved[24]` is filled with `0xAB`.
- **Polygon object records are 116 bytes, not HW1's 112.** In 1,727/1,727 files the material table starts exactly at `68 + nPolygonObjects * 116`; with 112 it matches in 0 files. In every file the `pMother/pDaughter/pSister` links are file offsets of the form `68 + k*116`, and each record's 4x4 `localMatrix` ends with `1.0f` at record offset 112.
- **The extra udword sits at record offset 0x24**, between `pPolygonList` (0x20) and `pMother` (now 0x28). Its value in object 0 equals `oPublicMaterial + (nPublic + nLocal) * 32` in 1,727/1,727 files, i.e. it points to a **new data block placed right after the (unchanged, 32-byte) material table** and before every vertex list. Example `race_sect/sacolyte/rl0/lod0/sacolyte.geo`: 6 objects, materials at 764 (= 68 + 6*116), 53 materials, new block at 2460 (= 764 + 53*32), first vertex list at 12340. What the block holds is not decoded here (R1 item 2, `docs/formats/mesh.md`).
- The two version-0x402 files (`race_derelict/nislensflaresource1/rl0/lod0/nislightsource1.{geo,peo}`) also have 116-byte records (materials follow at `68 + n*116`), so they are not HW1-layout either despite the version number.
- Consequence: with `MESH_ERROR_CHECKING` off, HW1's `meshLoadAndConvertTo64Bit` (Mesh.c:615ff, reads `polygonobject_disk` at a 112-byte stride) would read object 1 from offset 180 instead of 184 and fix up garbage pointers (example: `p1outpost.geo` object 7 [audit: was "object 0"; object 0 sits at offset 68 under both strides and reads 0x182c; at the 112-byte stride object 7's `pVertexList` lands on object 6's trailing `1.0f`, measured on `race_p1/p1outpost/rl0/lod3/p1outpost.geo`] decoded with the HW1 stride gives `pVertexList = 0x3f800000`, outside the 13,328-byte file).
- Correction to docs/big-archive-contents.md 2/3.2: `.peo` **is** an HW1 concept (the paged/packed-texture mesh, `.peo` = "paco"), not new in Cataclysm; HW1's loader prefers it. What is new is that Cataclysm's `.lod` files name `.peo` directly and that both flavours moved to 0x500 with 116-byte objects.

### 2.3 `.shp` ship static info

- 173 `.shp` files; 323 distinct keys; 235 known to some HW1 script table (string literals in `{ "key", ... }` entries across GoK:Game/Ships/SDL), **88 unknown** (`original/scratch-m2/static/shp-unknown.txt`). The parser ignores them, so they are silently lost. The most widespread: `fowRadius` (138 files: fog of war), `forceFieldProtectionDistanceScale` (140), `n-LIPS` (125), `explosionType` (124), `supportUnits`/`providesSU` (117: Cataclysm's support-unit cap), `externalBuild`, `repairDroids` (117), `TimeShockedByEMP` (92), `pilotViewOffsetX/Y/Z` (88), `UpgradeByDocking` (43), `upgradeTimeModifier` (40), `beastAttack*` (37: infection), `rkoGetsMissiles`, `rkoEatMeTime` (23), `bUseRayTriangleIntersectionForHitLocation` (24), plus per-ship specials (`leechRUsPerSecond`, `cuttingBeamDamagePerSecond`, `ramDamage`, `EMP_range`, `multiBeamRangeRatio`, `harvestableByMultiplyShips`, ...).
- Gun blocks (`NUMBER_OF_GUNS` / `GUN n { ... }`) use HW1's state machine (StatScript.c:995-1075) but carry new keys `GunSize`, `StartUpgradeLevel`, `EndUpgradeLevel`, `DamageMult`, `FireTime2`, `BulletSpeed2` (weapon upgrades), and new gun types: `GUN_HiddenGimble` 153 and `GUN_VisibleGimble` 93 guns, versus HW1's `GUN_Fixed/Gimble/NewGimble/MissileLauncher/MineLauncher` (GoK:Game/ObjTypes.h:34-41). Only `GUN_Fixed` (4), `GUN_MissileLauncher` (20), `GUN_MineLauncher` (1) parse; the other 246 guns would get type -1.
- `.shp` files also carry `pMexData` paths (e.g. `sAcolyte\Rl0\LOD0\sAcolyte.mex`).

### 2.4 Root `.script` tweak tables

67 root scripts. Apart from the language files, every key unknown to HW1 belongs to a table Cataclysm added or extended; the bulk is `unitlimitcaps*.script` with per-type caps for the 145 Cataclysm types (`capsSwarmer`, `capbHeavyCorvette`, ... in 9 files each), which HW1's `ShipStaticCapLimits` table cannot name. Measured with `shpkeys.py script`.

### 2.5 String tables

HW1 loads strings by key name (`strEntry(var)` = `{#var, strSetStringCB, &MessageStrings[var]}`, StringSupport.c:18) and pre-fills every slot with `*NO_STRING*` (:728-731, :24). Against `english.script` [data]:
- HW1 string ids: 590 (`GoK:Game/StringsOnly.h`, up to `NumStrings` at :657).
- Present in Cataclysm: 364. **Missing: 226** (render as `*NO_STRING*`), led by every HW1 ship name (`strAdvanceSupportFrigate`, `strCarrier`, ... `strP1StandardCorvette`), because Cataclysm names ships per race (`strkCarrier`, `strsMothership`, ...).
- Cataclysm-only keys: 1,318 in `cataclysm.big`, 1,330 in the 1.01 `update.big` copy (ignored by HW1).
- The front end will therefore show real text for shared UI strings and `*NO_STRING*` for ship/research names.

### 2.6 Front-end screens (`.fib`)

- The 7 files in HW1's startup list `utyFrontEndFiles[]` (GoK:SDL/utility.c:225-236) all exist in Cataclysm (`feman\front_end.fib` ... `feman\single_player_objective.fib`), with the HW1 header (`Bananna`, 0x212).
- 35 distinct screen names are passed to `feScreenStart` in GoK:Game/SDL; 32 appear as strings in Cataclysm's 21 `.fib` files. Missing: `Fleet_manager` (TaskBar.c:353), `MainScreen` (only in a commented-out call, FEFlow.c:1509), `Single_player_options`. The startup screen `Main_game_screen` (SDL/utility.c:809) exists.
- Cataclysm adds `beast_*_manager.fib`, `choose_server`, `console`, `game_chat`, `multiplayer_game`, `multiplayer_lan_game`, `front_end_demo` (21 files vs HW1's 12 [audit: was "11"; GoK code names 12 distinct `.fib` files that exist in the archive, plus the commented-out `Sensors_manager_SingleP` and the debug `testicle.fib`] named).
- At run time (stage-15 experiment, run20) all of these load and `Main_game_screen` is pushed. Only one callback is reported missing, `Main_game_screen-TO-Select_tutorial` (2.1). Other unregistered callbacks would only show up when a button is pressed, which no run did.

---

## 3. Ship types: 145 Cataclysm types vs HW1's enum

HW1: `ShipType_enum` in GoK:Ships/ShipDefs.h:10-71, **59 types** (0 `AdvanceSupportFrigate` ... 58 `ResearchStationTower`), race-neutral: one type is shared by R1 and R2 and the race picks the directory (`TOTAL_NUM_SHIPS = TRADERS_LAST_SHIP+1`, :102). Races: 6 (`R1 R2 P1 P2 P3 Traders`, GoK:Game/RaceDefs.h).

Cataclysm: CET:game/Game/ShipDefs.h, **145 types** (0 `sRecon` ... 144 `nisLensflareSource1`, `TOTAL_NUM_SHIPS 145`), one type per race-ship, `STD_LAST_SHIP bMissileCorvette` (59). Races: 7 (`RACE_Sect 0, Beast 1, Kushan 2, Taiidan 3, P1 4, Traders 5, Derelict 6`, CET:game/Game/RaceDefs.h).

Matching by stem (race prefix `s b k t p1 tr d nis` removed; `tools/shiptypes.py`, output `original/scratch-m2/static/shiptypes.json`):

| Cataclysm race | types | with an HW1 counterpart | Cataclysm-only types |
|---|---:|---:|---|
| Sect (`s*`, 0-32) | 33 | 2 (`sCarrier`=Carrier, `sMothership`=Mothership) | 31: sRecon sAcolyte sMimic sLeech sACV sMCV sRammingFrigate sMultiBeamFrigate sHiveFrigate sSwarmer sDreadnought sDreadnoughtCannon sCarrierSupport sWorker sRepairDroid sProcessor sSentinel sDestroyer and 13 Mothership modules (Special, Engineering, Armour, Micro, Weapons, DockingBay, Support, Command, EnginesLower, EnginesUpper, LowerSpine, BigGun, OreCanArmour) |
| Beast (`b*`, 33-59) | 27 | 8 (CloakedFighter, AttackBomber, MultiGunCorvette, HeavyCorvette, DFGFrigate, HeavyCruiser, Carrier, Mothership) | 19: bRecon bAcolyte bInterceptor bACV bMultiBeamFrigate bHiveFrigate bSwarmer bIonArrayFrigate bRammingFrigate bWorker bRepairDroid bProcessor bSentinel bMothershipDockingBay bMothershipEnginesLower bSectMoship bMothershipSupport bCruiseMissile bMissileCorvette |
| Kushan (`k*`, 60-73) | 14 | 14 | none |
| Taiidan (`t*`, 74-91) | 18 | 18 | none |
| P1 (`p1*`, 92-97) | 6 | 5 | p1Outpost |
| Traders (`tr*`, 98-108) | 11 | 11 | none |
| Derelict (`d*`, `nis*`, 109-144) | 36 | 1 (`dIonCannonFrigate`=IonCannonFrigate) | 35 (dRKO, dBigGun, slipgates, junk, planets, crystals, Beast/Nomad objects, nisLightSource1/2, nisLensflareSource1) |
| **total** | **145** | **59 rows -> 37 distinct HW1 types** | **86** |

- **No Cataclysm type keeps its HW1 id** (0 of 59 matched rows; e.g. Carrier 2 -> sCarrier 12 / bCarrier 47 / kCarrier 62 / tCarrier 76). Every table indexed by `ShipType` (the per-type `shipHeaders[]` C code in GoK:Ships/*.c, `TOTAL_NUM_SHIPS`-sized arrays in AIPlayer.h, Sensors.h, GameStats.h, Universe.h, ConsMgr.h, the `.shp` path builder) must be re-keyed, not extended.
- **22 HW1 types have no Cataclysm counterpart:** DDDFrigate DefenseFighter LightCorvette LightDefender LightInterceptor Probe RepairCorvette ResearchShip ResourceController SensorArray Drone TargetDrone HeadShotAsteroid CryoTray, all 5 P2 and all 3 P3 types.
- 143 of the 145 types have a `.shp` at `race_<race>\<name>.shp`; `trResearchStationBridge` and `trResearchStationTower` have none (as docs/big-archive-contents.md 6 also found). HW1 itself has the same two Traders types.
- Consequence for an unmodified HW1 engine: its `.shp` path for every type is `<R1|R2|P1|P2|P3|Traders>\<HW1 name>.shp`, none of which exist (1.4), so it would start a game with **every ship disabled**, not crash. This is inferred from the code: no run got as far as universe init. The furthest run (run27) stopped in level load first, on race names.
- Before any of that, the ship **names** already break 3 tweak tables at startup (2.1: `aiship.script` fatal, `tweak.script` and `crates.script` out-of-bounds writes), and `GatherStatsFor.script` breaks on the race names. The enum is therefore a startup blocker, not only a gameplay one.

---

## 4. Run setup

Nothing was installed or patched. The reference build and clones were only read. The GOG game was not launched. No input was sent to any process. No gamescope was used.

**Engine.** `reference/build-hwsdl/homeworld` (GoK, debug, ASan+UBSan; OpenGL 4.6 NVIDIA 580.173.02 on an RTX 5090, from `rndInit:` lines in run28.log).

**Command** (`tools/run.sh <name> <driver> <timeout> [args]`). Every run used:

```
cd original/scratch-m2/runs/data-deltas
HOME=<scratch home, runs 27-29 only>  HW_Data=<assets dir>  SDL_VIDEODRIVER=<offscreen|dummy|unset=x11>
ASAN_OPTIONS=log_path=<run>.asan:detect_leaks=1:verify_asan_link_order=0
UBSAN_OPTIONS=print_stacktrace=1:log_path=<run>.ubsan
timeout -s TERM -k 5 <T> stdbuf -oL -eL reference/build-hwsdl/homeworld /window /settingspath original/scratch-m2/settings [args]
```

- `stdbuf -oL` keeps the log complete when LSan ends the process (1.1). It needs `verify_asan_link_order=0` because `stdbuf` preloads a library (run1, first attempt: `ASan runtime does not come first in initial library list`).
- `/nodebugInt` (from run4 on) makes `dbgFatal` exit cleanly. Without it, every fatal error ends in a deliberate NULL write (Debug.c:199-203) and an ASan SEGV report on top of the real message [run3].
- `/noSpeech` (from run11 on) skips speech init after the SIGFPE (2.1).
- `/logFileLoads` (runs 18, 28, 29) writes `FileLoads.log`: one line per data file, prefixed with the archive it came from. These are the load traces quoted below.
- `/ignoreBigfiles` (runs 19-29 on the tree dir) loads every file from the filesystem.
- **Settings path.** `/settingspath` redirects only writes. The GoK build reads `Homeworld.cfg` from `$HOME/.homeworld/` no matter what (`utyOptionsFileRead`, GoK:SDL/utility.c:866-884). Runs 27-29 therefore set `HOME=original/scratch-m2/home`. The user's `~/.homeworld` does not exist and was never created (`ls` after the runs).

**Assets directories** (all in `original/scratch-m2/`, gitignored).
- `hw1-assets/` is built by `tools/setup_assets.sh <stage>`.
- `hw1-tree/` is built by `tools/setup_tree.sh [plain|mesh]`. It is a symlink mirror of `game_extracted/cataclysm`, with the 39 `update.big` files overlaid and the same stages applied, for `/ignoreBigfiles` runs.
- Each stage removes the stop seen in the run named in the last column. Stage 0 was set up before run1, from reading the code; run0c later showed the stop it avoids.

| stage | workaround | class | removes the stop of |
|---:|---|---|---|
| 0 | `Homeworld.big` -> `original/game/cataclysm.big`, `Update.big` -> `update.big` | rename | run0c |
| 1 | `HW_Music.wxd` -> `Music4Cat.wxd` | rename | run0, run1 |
| 2 | `HW_comp.vce` -> `SectSpeech.vce` | rename | run2 |
| 3 | `aiship.script`: the 103 `rowDetails` rows with non-HW1 ship names removed (5 `p1*`... rows kept, see note) | row filter | run3 |
| 4 | `tweak.script` (1.01 copy): the 19 `hyperspace` rows removed | row filter | run4 |
| 5 | `Dock.script`: empty file | stub | run5 |
| 6 | `GatherStatsFor.script`: empty file | stub | run6 |
| 7 | `crates.script`: the 48 `shipProbability`/`shipGroupSize` rows removed | row filter | run7 |
| 8 | `Parade_R1Mothership`, `Parade_R2Mothership`, `Parade_P2Mothership`, `Parade_Carrier.script`: empty | stub | run8 |
| 9 | `SoundFX/speechsentence_comp.lut` -> `sectsentence.lut`, `speechphrase_comp.lut` -> `sectphrase.lut` | rename | run9 |
| 10 | `FEMan/Construction_Manager/Build_race{1,2}_icon{1,2}.lif` -> `build_race*_icon*_on.lif` | rename | run11 |
| 11 | `ResearchGUI/Race1/Research_icon.lif` -> `research_icon_engineering_on.lif`; `Race2` -> `bresearch_ani_01.lif` | rename | run12 |
| 12 | `BattleChatter.script`: empty | stub | run13 |
| 13 | `colPick.script`: empty | stub | run14 |
| 14 | `Tactical.script`: the `IconClass CLASS_Micro` block removed | row filter | run15 |
| 15 | **EXPERIMENT:** all 1,729 `.geo`/`.peo` down-converted to the HW1 layout (`tools/mesh500to402.py`: version := 0x402, drop the udword at polygon-object offset 0x24, re-link `pMother/pDaughter/pSister`, pad 4*n bytes after the object table so every other offset is unchanged) | data conversion | run16 |

Note on stage 3: the 5 kept rows are the `p1*` rows, which HW1's `StrToShipType` matches case-insensitively against `P1Fighter`... (ObjTypes.c:493 `strcasecmp`).

Stages 3-14 change file contents in scratch and are therefore stronger than the brief's "symlink or rename". Each one is the smallest edit that removes a single fatal, and section 5 keeps the unedited result of every run. Stage 15 is an experiment, not a workaround: it answers whether anything besides the mesh layout blocks the front end.

---

## 5. Runs and log excerpts

### 5.1 Headless canonical sequence (`hw1-assets/`, SDL `offscreen`)

| run | stage | args | rc / wall | where it stops (log line) | cause |
|---|---:|---|---|---|---|
| run0 | none (empty dir) | `/nodebugInt` [audit: args were blank; `runs.txt` records `args=/nodebugInt` for run0, run0b and run0c, which were run after run29] | 213 / 7.8 s | `Unable to open file: HW_Music.wxd` | startup stream check comes before any archive |
| run0c | 1+2 only, GOG names `cataclysm.big`/`update.big` | `/nodebugInt` [audit: was blank, per `runs.txt`] | 213 / 8.4 s | `BigFile.c (2537): Warning - Unable to open optional .big file: .../update.big` then `BigFile.c (2519): Fatal error - Unable to find required .big file: .../Homeworld.big` | archive names (1.2) |
| run1 | 0 | | 213 / 9.4 s | `Unable to open file: HW_Music.wxd` | 1.2 |
| run2 | 0-1 | | 213 / 9.1 s | `Unable to open file: HW_comp.vce` | 1.2 |
| run3 | 0-2 | | 1 / 9.7 s | `AIShip.c (475): Fatal error - Invalid shiptype read at line sRecon 0 0 in aiship.script` (+ UBSan `Debug.c:203:19: store to null pointer`, ASan SEGV from the deliberate interrupt) | ship names |
| run4 | 0-3 | `/nodebugInt` | 1 / 11.8 s | ASan `global-buffer-overflow ... WRITE of size 4 ... in scriptSetHyperspaceCostCB StatScript.c:305` <- `scriptSetTweakableGlobals StatScript.c:1873` | ship names, OOB write |
| run5 | 0-4 | `/nodebugInt` | 213 / 9.4 s | `File.c (1364): Fatal error - fileOpen: cannot open file Dock.script` | missing file |
| run6 | 0-5 | `/nodebugInt` | 213 / 9.5 s | `Stats.c (208): Fatal error - Assertion of (race >= 0 && race < 5) failed.` | race names |
| run7 | 0-6 | `/nodebugInt` | 1 / 9.5 s | ASan `global-buffer-overflow ... WRITE of size 4 ... in scriptSetShipProbCB StatScript.c:289` <- `crateInit Crates.c:774` (4 bytes before `SHIP_PROBS`) | ship names, OOB write |
| run8 | 0-7 | `/nodebugInt` | 213 / 9.4 s | `fileOpen: cannot open file Parade_R1Mothership.script` | missing file |
| run9 | 0-8 | `/nodebugInt` | 1 / 9.9 s | `File.c (1120): Fatal error - fileSizeGet: can't find file '.../SoundFX/speechsentence_comp.lut'.` Sound banks loaded before this (UBSan `struct PATCH` misalignment only) | renamed speech LUT |
| run10 / 10c | 0-9 | `/nodebugInt` | 1 / 10.1 s | `AddressSanitizer: FPE` in `soundstreamcreatebuffer sstream.c:245` <- `speechEventInit SpeechEvent.c:546` <- `soundEventInit SoundEvent.c:798` <- `utyGameSystemsInit utility.c:3930` (run10c with `symbolize=0`, frames resolved with `addr2line`; with symbolisation on, ASan printed only `DEADLYSIGNAL`) | speech LUT layout |
| run11 | 0-9 | `/nodebugInt /noSpeech` | 1 / 10.0 s | `Starting front end module` ... `fileSizeGet: can't find file '.../FEMan/Construction_Manager/Build_race1_icon1.lif'.` | FE art name |
| run12 | 0-10 | same | 1 / 9.4 s | `fileSizeGet: can't find file '.../ResearchGUI/Race1/Research_icon.lif'.` | FE art name |
| run13 | 0-11 | same | 1 / 9.5 s | `Battle.c (628): Fatal error - Unable to find battle chatter event 'CHAT_Intel_Grp_PositiveBattle'.` | speech event names |
| run14 | 0-12 | same | 1 / 9.7 s | `ColPick.c (195): Fatal error - Assertion of (race >= 0 && race < (5 + 1)) failed.` | race names |
| run15 | 0-13 | same | 1 / 9.6 s | `Startup subtitle module` ... `Tactical.c (152): Fatal error - Assertion of (toCurrentClass < ((8 + 1)+1)) failed.` | ship class `CLASS_Micro` |
| **run16** | 0-14 | same | 1 / 9.0 s | `Startup engine trail module` / `Mesh.c (917): Fatal error - meshLoad: Invalid version # misc\EngineGlows\energybolt.geo.  Expected 0x402, found 0x500` | **mesh format; no harmless workaround exists** |
| run28 | 0-14 | `+ /logFileLoads` | 1 / 8.4 s | same as run16; `FileLoads.log` has 125 lines, the last `Homeworld.big \| misc\EngineGlows\energybolt.geo` | load trace |

What loaded before the mesh stop (run28 `FileLoads.log`, 125 lines; 56 from `Homeworld.big`, 1 from `Update.big`, the rest from the filesystem):
- 46 `.script` loads (the stage overrides come from the filesystem; the others come from the archive);
- 10 SFX `.lut` + 4 `.bnk` (`SoundFX/Volume.lut` ... `SoundFX/UI.bnk`);
- 28 `.lif`/`.LiF` loads (18 cursors `CursorBitmaps//cursor.LiF` ..., plus the stage-10/11 icons);
- 15 fonts (`fonts/English/HW_EuroseCond_11.hff` ...);
- 9 `.fib`: the 7 of `utyFrontEndFiles[]`, then `FEMan/Multiplayer_Game.FIB` (from `Update.big`, the 1.01 version) and `FEMan/Choose_Server.FIB`;
- the 10 listed multiplayer `.level` files;
- `Movies/animatics.lst`;
- the failing `.geo`.

All of these parsed without a further error once the stages above were in place. The rest of the HW1 startup path accepts Cataclysm's files of these types.

### 5.2 Other video drivers

| run | driver | result |
|---|---|---|
| run21, run21b | `dummy` | `Couldn't set video mode: OpenGL support is either not configured in SDL or not available in current SDL video driver (dummy)` -> `rndInit: GL couldn't setupPixelFormat` -> `Fatal Error: Couldn't initialize default rendering system.`, then SIGSEGV in libc called from libSDL2 after `Starting front end module`. This does not depend on the data: the dummy driver has no GL, so it cannot run this engine |
| run22 | `dummy`, tree | same |
| run25 | unset (x11 / Xwayland, windowed `/window`), tree + stage 15, `/debugScreenshots`, 40 s | the same message sequence as offscreen run24 (`feScreenPush: pushed screen 1`, `... 'Main_game_screen-TO-Select_tutorial' not found`, `feScreenPush: pushed screen 2`), display mode 2560x1440 instead of offscreen's 1024x768. Closed by the timeout. No desktop capture was taken |

The `offscreen` driver gives the engine a real GL 4.6 context and gets exactly as far as x11 does, so a windowed run was not needed to measure the data deltas. Run25 was made only to see whether the engine's own screenshot shows a front-end frame. It does not: `/debugScreenshots` captures inside the 3D view render (GoK:SDL/render.c:3888-3910), and with no game running that buffer is black in both drivers (identical 22,117-byte JPEGs, kept only in `original/scratch-m2/settings/offscreen-shots/`). **Whether the front end draws Cataclysm's art correctly under HW1 is therefore UNVERIFIED.**

### 5.3 Experiment: the mesh layout is the only remaining startup blocker

| run | setup | rc / wall | result |
|---|---|---|---|
| run17, run18 | `hw1-assets/` stage 15 (converted meshes as loose files next to the archives) | 1 / 8.9 s | same mesh fatal: `FileLoads.log` shows `Homeworld.big \| misc\EngineGlows\energybolt.geo`, although the startup scan lists `misc/engineglows/energybolt.geo` among `1736 newer files found on filesystem`. Why the newer loose file is not used is UNVERIFIED (`bigFindFile`, BigFile.c:2869-2891, should skip an entry marked `LOCAL_FILE_IS_NEWER`). The tree with `/ignoreBigfiles` sidesteps it |
| run19 | `hw1-tree/` plain (stages 0-14), `/ignoreBigfiles` | 1 / 8.7 s | same stop as run16. The tree reproduces the archive path |
| **run20** | `hw1-tree/` mesh (stage 15), `/ignoreBigfiles`, 60 s | 124 (timeout) / 60.7 s | `Startup engine trail module` / `trNoPalResizePool: 64MB` / `feScreenPush: pushed screen 1` / `feFunctionExecute: function 'Main_game_screen-TO-Select_tutorial' not found`, then idle until SIGTERM, then a clean shutdown (`Shutdown engine trail module` ... `taskClose: closing task module`). 0 ASan errors during the run. The 21 engine-glow meshes (`misc\EngineGlows\energybolt.geo`, `engine_glow00-03`, `engine_glow_frigate00-03`, `r1moship0-3`, `r2moship0-3`, `p1moship0-3`) load through HW1's `meshLoadAndConvertTo64Bit` without any sanitizer report |
| run24 | as run20 + `/debugScreenshots` (starts single player at once, GoK:SDL/utility.c:4241-4244), 40 s | 124 / 40.6 s | stops at the `Tutorial_Alert` screen (`feScreenPush: pushed screen 2`): `needtutorial` was read as TRUE (utility.c:2762-2767) |
| run26 | as run24, `TutorialNeeded 0` written in the settings-path cfg | 124 / 40.6 s | unchanged, because the cfg is read from `$HOME`, not `/settingspath` (section 4) |
| **run27** / run29 | as run24, `HOME` = scratch with `TutorialNeeded 0` (+ `/logFileLoads` in run29) | 1 / 8.3 s | `New single player game started` / `feScreenPop` / `feScreenPush` / `tutGameMessage: 'CSM_Hyperspace'` / `tutGameMessage: 'TB_ObjectivesWindowInit'` / `player is race 0` / `LevelLoad.c (1885): Fatal error - Assertion of (race != 0xffffffff) failed.` The trace ends with `SinglePlayer/Mission01/Mission01.mission` (loaded twice; run29 `FileLoads.log`, 154 lines, including 21 `.geo` and 10 `.fib`). The two `tutGameMessage` lines are task-bar region names (GoK:Game/TaskBar.c:1511-1512, TaskBar.h:48), so they show that the in-game task bar initialised. The level file then broke on `ExcludeShips RACE_P1,All` (mission01.mission line 22) |

Bounds of the experiment:
- The universe never ticked (`univUpdateCounter` stayed 0), so nothing about ship loading or in-game rendering was measured.
- The down-converted meshes only prove that HW1's loader accepts them without memory errors. They do not prove that vertices, normals, polygons and materials mean the same as in 0x402, or that the new block (2.2) is optional.
- The space-key emulation in GoK's `/debugScreenshots` (render.c:3935-3941, `keyPressDown`) only starts after 832*4 universe updates, about 52 s of game time. No run lasted that long in game time (the universe never started), so the engine never pressed a key internally either.

### 5.4 Engine-side findings that do not depend on Cataclysm's data

These show up in every run and would show up with HW1 data too (inferred from the code; no HW1 data is on this machine):
- **LSan truncates buffered stdout** (1.1).
- **Shutdown assert** `utility.c (3462): Fatal error - Assertion of (result != -1) failed.` [run20, run24-26]: `utyGrowthHeapFree` calls `munmap(heap, 0)` (GoK:SDL/utility.c:3453-3463), and a zero length always fails on Linux.
- **Options file path**: `/settingspath` is ignored for reading (section 4).
- **UBSan misaligned accesses** to `struct fescreen` (FEFlow.c:1906-2336, TaskBar.c:1499; 92 report lines in run20, 102 in run29) and `struct PATCH` (sound banks; 29 per run). They come from the 64-bit conversion of packed on-disk structs (docs/hw1-port-lessons.md A8).
- `stb_image_write.h:1263:14: runtime error: left shift of 16711680 by 8 places` when saving a screenshot [run24].
- LSan at exit: about 2.5-3 KB leaked in 11-19 allocations (`rinAddMode` rinit.c:121, `memStatsTaskFunction` Memory.c:136, ...).
- Every run spends about 8-9 s before its first data error, even when the first check fails (run0: 7.8 s). Where that time goes was not measured (UNVERIFIED; the CD-ROM probe `utyGetFirstCDPath` is a candidate).
- `Homeworld CRCs: Update.big TOC = 0x0 / Homeworld.big TOC = 0x0` [run28]. The network-version CRC is not computed in this build, so this run says nothing about Cataclysm's TOC CRC.

---

## 6. Ranked "blocks the first frame" list

"First frame" here means the HW1 front end's first screen (`Main_game_screen`), reached with no input. The ranking is by what an M2 port must do, hardest first. Every item has a run that shows it.

| rank | blocker | evidence | what M2 must do | workaround used here |
|---:|---|---|---|---|
| 1 | **Mesh v0x500**: 116-byte polygon objects, plus a new data block after the materials. Fatal in debug builds; a release build would silently misparse | run16, run28 (`Mesh.c (917)`); 2.2 (1,727/1,727 files) | port Cataclysm's mesh loader (R1 item 2 `docs/formats/mesh.md`, then R2 file formats first). The 21 engine-glow meshes load before the front end, so nothing displays without it | none harmless; the stage-15 down-conversion experiment gets past it |
| 2 | **Ship-type enum (59 -> 145, all ids moved)**: tweak tables keyed by ship name | run3 (fatal), run4 and run7 (ASan OOB writes) | adopt CET's 145-type `ShipDefs.h` ids; re-key every `TOTAL_NUM_SHIPS` table and `StrToShipType` | rows filtered (stages 3, 4, 7) |
| 3 | **Race enum (6 -> 7, `RACE_*` names)**: GatherStatsFor, colPick (also a new text format), `.mission` Exclude/IncludeShips | run6, run14, run27 | adopt `RaceDefs.h` (Sect, Beast, Kushan, Taiidan, P1, Traders, Derelict) and `RACE_*` string table | stubs (stages 6, 13) |
| 4 | **Speech LUT layout** (`numactors` 1, short `compbitrate[]`): SIGFPE | run10c | port Cataclysm's speech LUT reader and its two-race streams (`SectSpeech.vce`, `BeastSpeech.vce`) | `/noSpeech` |
| 5 | **Ship-class enum (9 -> 12)**: `CLASS_Micro` in `Tactical.script` | run15 | adopt `ClassDefs.h` (`MonsterShip`, `Component`, `Micro`) | block removed (stage 14) |
| 6 | **Battle-chatter event names** | run13 | port Cataclysm's speech event table (`CHAT_Intel_Grp_*`, `CHAT_Ship_Grp_*`) | stub (stage 12) |
| 7 | **Missing tweak scripts**: `Dock.script`, 4 `Parade_*.script` | run5, run8 | drop the loads (`Dock` is gone) and switch parade loading to Cataclysm's 12 per-ship `parade_*.script` | empty stubs (stages 5, 8) |
| 8 | **Renamed front-end art**: `Build_race{1,2}_icon{1,2}.lif`, `ResearchGUI\Race{1,2}\Research_icon.lif` | run11, run12 | port Cataclysm's construction-manager and research-manager art lists (`_on/_off/_disabled` variants, module icons) | symlinks (stages 10, 11) |
| 9 | **File names**: `Homeworld.big`/`Update.big` (case), `HW_Music.wxd`, `HW_comp.vce`, speech LUT names | run0, run0c, run1, run2, run9 | Cataclysm names in `bigFilePrecedence[]` and the stream file names (`Cataclysm.big`, `Update.big`, `Music4Cat.wxd`, `SectSpeech.vce`/`BeastSpeech.vce`) | symlinks (stages 0-2, 9) |
| 10 | **FE callback names**: `Main_game_screen-TO-Select_tutorial` (non-fatal) | run20 | register Cataclysm's FE callbacks | none needed |

Not first-frame blockers, but measured or proven on the way:
- **Level load**: `RACE_*` names in `.mission` (run27). The next step after the front end; the race enum (rank 3) fixes it.
- **Ship files**: no HW1 `.shp` path resolves, so every ship would be disabled (1.4, 3). Ship names also show as `*NO_STRING*` (226 missing string ids, 2.5).
- **Gun types**: 246 of 271 guns use `GUN_HiddenGimble`/`GUN_VisibleGimble`, which HW1 maps to -1 (2.3).
- **The 88 `.shp` keys HW1 ignores** (fog of war, support units, EMP, upgrades, Beast infection, RKO, ...) (2.3).
- **The 1.01 patch** would be dropped silently on a case-sensitive file system unless `Update.big` is spelled exactly that way (run0c).

### Open questions
1. Why loose newer files in subdirectories did not override archive entries in run17/18 while root-level overrides did (`aiship.script`, run5+). Find out before relying on `bigFilesystemCompare` for mod/override workflows.
2. The content of the new mesh block (polygon-object offset 0x24) and whether the 0x402 meaning of the vertex, normal and polygon lists still holds. That is R1 item 2.
3. Whether HW1's front end renders Cataclysm's `.fib` screens correctly (5.2: no capture is possible without input or a desktop grab).
4. Cataclysm's speech LUT layout beyond the first 16 bytes (`numactors`, how many bitrates there are), and how `BeastSpeech.vce` is selected.
5. What fires `Main_game_screen-TO-Select_tutorial` with no input.
