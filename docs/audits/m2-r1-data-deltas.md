<!-- done: archive-relist, missing-renamed, headers, mesh-layout, engine-rerun, source-crosscheck, ledger, hygiene, fixes, verdict -->
# Audit: M2-R1 item 1 "data-deltas" (docs/m2-r1-data-deltas.md)

Adversarial audit, 2026-10-10. Target: `docs/m2-r1-data-deltas.md` (producer: M2-R1 data-deltas). Every check below was re-run by the auditor. Nothing reused the producer's outputs unless this report says so.

**Where the audit artefacts are.** Everything is in `original/scratch-m2/audit-data-deltas/`, which is gitignored and outside the worktree. It holds:
- the fresh `biggie` listings (`biggie-cat.txt`, `biggie-lsan.txt` + `.err`);
- `audit28.log`, `audit28.ubsan.*` and `FileLoads.log` from the engine re-run;
- the audit scripts in `tools/` (`chk_list.py`, `geochk.py`, `geochk2.py`, `enum.py`, `strs.py`, `scan.py`, `run.sh`), which ran from `/tmp/m2audit-dd/`.

**Rules kept.**
- The GOG game was not launched and Ghidra was not run.
- No input was sent and no gamescope was used.
- Reference clones were only read.
- No game data was written into the worktree, and nothing was committed.
- The engine re-run used a separate scratch `HOME` and settings path (`audit-data-deltas/home`, `/settings`). `~/.homeworld` still does not exist (checked with `ls`).

## 1. Archive re-list (`cataclysm.big`) and missing/renamed entries

Command: `ASAN_OPTIONS=detect_leaks=0 reference/build-hwsdl/tools/biggie/biggie -v original/game/cataclysm.big` (rc=0).

| # | claim (doc section) | result | evidence |
|---|---|---|---|
| 1 | 16,528 entries; totals 75,559,532 / 128,441,214, ratio 0.588 (1.1) | correct | 16,528 rows matching `^[0-9a-f]{16} ` (including stored "na"-ratio rows); totals line identical |
| 2 | Without `detect_leaks=0`: rc=1, LSan leak of 528,896 bytes at `bigTOCRead BigFile.c:415`, listing truncated, last 6 rows and totals lost (1.1) | correct | re-run gave rc=1, `Direct leak of 528896 byte(s) ... bigTOCRead ../gardens-of-kadesh/src/Game/BigFile.c:415`. 16,523 row starts, the last one cut mid-line, so 16,522 complete rows and no totals |
| 3 | `Homeworld.big` absent (1.2) | correct | not in the TOC; `original/game/` has `cataclysm.big` |
| 4 | `HW_comp.vce` absent, Cataclysm ships `SectSpeech.vce` + `BeastSpeech.vce` (1.2) | correct | not in the TOC; both `.vce` are loose in `original/game/` |
| 5 | `HW_Music.wxd` absent; `Music4Cat.wxd` + `soundfx\music4cat.wxh` (1.2) | correct | `music4cat.wxh` in the TOC; `Music4Cat.wxd` loose in `original/game/` |
| 6 | `SoundFX\speechsentence_comp.lut` absent; `sectsentence.lut` present (1.2, 1.3) | correct | TOC: `soundfx\sectsentence.lut` and `soundfx\sectphrase.lut` only |
| 7 | `Dock.script` and `AIStrategy.script` absent (1.3) | correct | neither is in the TOC |
| 8 | `Parade_R1Mothership/R2Mothership/P2Mothership/Carrier.script` absent; 12 `parade_*.script` present (1.3) | correct | 0 of the 4; 12 `parade_*` (`parade_p1mothership`, `parade_r2carrier`, ...) |
| 9 | ETG `R1.script` absent; 7 `etg\race_*.script` (1.3) | correct | 0 `etg\r1.script`; 7 `etg\race_*.script` |
| 10 | `FEMan\Construction_Manager\Build_race1_icon1.lif` absent; `_on/_off/_disabled` variants (1.3) | correct | 3 variants in the TOC, base name absent |
| 11 | `ResearchGUI\Race1\Research_icon.lif` absent; `research_icon_engineering_{on,off,disabled}` (1.3) | correct | as stated |
| 12 | `Movies\relicintro.bik`, `Mission05_OEM.mission`, `Spanish.script`, `ETG\genericBOOM.ebg`/`defaultBlast.ebg` absent; `bdogintro.bik` and `sierra.bik` loose (1.3) | correct | none in the TOC; both `.bik` are in `original/game/Movies/` |
| 13 | Inventory counts: 21 `.fib`, 107 `researchgui\` files, 173 `.shp`, 967 `.geo` + 762 `.peo` (= 1,729), 7 derelict objects, no `resources\gasclouds\`, the 21 named engine-glow meshes (1.3-1.5, 2.6, 5.3) | correct | `chk_list.py` over the fresh listing (`resources\` subdirectories are asteroids, dustclouds and nebulae only) |
| 14 | "the 11 `FEMan\*.fib` HW1 names" (1.3) and "21 files vs HW1's 11 named" (2.6) | **wrong** | 1.3 lists 12 names itself, and the producer's own `static/lit-check.txt` has 12 distinct found `.fib` names (Construction_Manager, CSM-All, Front_end, Horse_Race, Hyperspace_Roll_Call, In_game_ESC_menu, Launch_Manager, Research_Manager, Sensors_manager, single_player_objective, TaskBar, Trader_Interface). **Fixed** in both places |
| 15 | Literal check totals 72 / 240 / 149 (1.3) | correct (consistency only) | `== full 72`, `== base 240`, `== missing 149` in `static/lit-check.txt`. The lookups were not recomputed, but the 10 entries in rows 3-12 agree |

## 2. Format headers (xxd) and the mesh layout

`xxd -l 32` on files in `original/game_extracted/cataclysm/`:

| # | file | doc claim | bytes read | result |
|---|---|---|---|---|
| 16 | `race_sect/sacolyte/rl0/lod0/sacolyte.geo` | `RMF97ba`, v0x500, bytes 8-11 `00 05 00 00` | `524d4639376261 00 / 00050000` | correct |
| 17 | `.../sacolyte.peo` | `RMF99ba` v0x500 | `RMF99ba\0`, `00050000` | correct |
| 18 | `.../sacolyte.mex` | `mannngo`, 0x106, 13 chunks | `6d616e6e6e676f00 0601 0d00` | correct |
| 19 | `feman/front_end.fib` | `Bananna`, 0x212, 26 screens | `42616e616e6e6100 1202 1a00` | correct |
| 20 | `nis/n01.nis` | `Crannnberry`, 0x101 | `Crannnberry\0` `0101` | correct |
| 21 | `btg/default.btg` | first u32 0x600 | `00060000` | correct |
| 22 | `hsf/default.hsf` | `rrasberry`, 0x100 | `rrasberry\0` pad, `00010000` | correct |
| 23 | `textures.ll` | `Event13`, 0x104 | `Event13\0` `0401` | correct |
| 24 | `soundfx/sectsentence.lut` | `LUT1`, checksum 0x0d692490 = `SectSpeech.vce` bytes 4-7; numcolumns 4, numevents 863, numactors 1, then 512, 2, 4, 61 | `4c555431 9024690d 0400 5f03 0100 0002 0200 0400 3d00`; `SectSpeech.vce` = `VCE0 9024690d` | correct |
| 25 | `soundfx/guns.bnk`, `gunevents.lut` | `BNK0`/`PKG0`, shared checksum 0x40568380 | `424e4b30 80835640` / `504b4730 80835640` | correct |
| 26 | v0x402 outlier `race_derelict/nislensflaresource1/.../nislightsource1.geo`; `misc/engineglows/energybolt.geo` v0x500 | as stated | `02040000` / `00050000` | correct |

Layout claims (2.2), checked against GoK `Mesh.h`. The disk header is 68 bytes; `polygonobject_disk` is 4+1+1+2+16+24+64 = 112 bytes; `MESH_Version` is 0x402; `MESH_ERROR_CHECKING` is 1 under `HW_BUILD_FOR_DEBUGGING`.

| # | claim | result | evidence (`geochk.py`, `geochk2.py`) |
|---|---|---|---|
| 27 | 116-byte records, materials at `68 + n*116`, never at `68 + n*112`; `fileSize == len-68`; the udword at +0x24 = `oPublicMaterial + (nPub+nLoc)*32` | correct (random sample: 60 of the 1,729 files, seed 7) | 60/60 fileSize, 60/60 at 116, 0/60 at 112, 60/60 for the +0x24 rule; all 60 are v0x500 |
| 28 | sacolyte example: 6 objects, materials at 764, 53 materials, new block at 2460, first vertex list at 12340, `reserved` = 0xAB, `1.0f` at record offset 112 | correct | as measured |
| 29 | "`p1outpost.geo` **object 0** decoded with the HW1 stride gives `pVertexList = 0x3f800000`, outside the 13,328-byte file" | **wrong** | the 13,328-byte file is `lod3/p1outpost.geo`. At the 112-byte stride, objects 0-7 read `pVertexList` 0x182c, 0x67, 0x55, 0x44, 0x6, 0x72, 0x32f0, **0x3f800000**. The value belongs to **object 7**: object 0 is at the same offset under both strides. The conclusion (garbage pointers) stands. **Fixed** |

## 3. Engine re-run

The doc's final canonical headless command is run28 (stages 0-14, `hw1-assets/`). Run16 has the same configuration without `/logFileLoads`; runs 20-29 are the stage-15 experiment and were not repeated. The audit re-ran run28 with an audit copy of the producer's `tools/run.sh`. Only the output directory, the settings path and `HOME` differ; the audit `HOME` is a copy of the scratch `.homeworld`:
`SDL_VIDEODRIVER=offscreen HW_Data=original/scratch-m2/hw1-assets timeout -s TERM -k 5 60 stdbuf -oL -eL reference/build-hwsdl/homeworld /window /settingspath <audit>/settings /nodebugInt /noSpeech /logFileLoads`. No input was sent.

| # | claim | result | evidence |
|---|---|---|---|
| 30 | The first blocking failure with stages 0-14 is `Mesh.c (917): Fatal error - meshLoad: Invalid version # misc\EngineGlows\energybolt.geo.  Expected 0x402, found 0x500`, right after `Startup engine trail module` (run16, run28) | correct | `audit28.log` line 1373 `Startup engine trail module`, line 1375 the identical fatal; rc=1, 8.2 s, 1,375 lines (run28: rc=1, 8.4 s, 1,375 lines) |
| 31 | run28 `FileLoads.log`: 125 lines, last one `Homeworld.big \| misc\EngineGlows\energybolt.geo`; 56 from `Homeworld.big`, 1 from `Update.big`; 46 `.script`, 14 `.lut`/`.bnk`, 28 `.lif`, 15 `.hff`, 9 `.fib` (7 startup + `Multiplayer_Game.FIB` from `Update.big` + `Choose_Server.FIB`), 10 `.level` (5.1) | correct | the audit `FileLoads.log` is identical to `run28.FileLoads.log` (`diff` empty); all category counts match |

## 4. Source and data cross-checks (sampled)

| # | claim | result | evidence |
|---|---|---|---|
| 32 | SIGFPE mechanism: `SpeechEvent.c:546` passes `compbitrate[1]` (= 2) and `sstream.c:245` computes `buffersize % ((bitrate >> 3) * 4)` (2.1) | correct | GoK source lines as cited; `SENTENCELUT` has `compbitrate[4]` (`SoundStructs.h`); (2>>3)*4 = 0 |
| 33 | Fatal sites `AIShip.c:475`, `File.c:1364`, `File.c:1120`, `BigFile.c:2519/2537`, `Mesh.c:917` | correct | lines read |
| 34 | `utyFrontEndFiles[]` holds 7 active files (2.6) | correct | GoK `SDL/utility.c:225-237` |
| 35 | The options file is read from `$HOME/.homeworld`, whatever `/settingspath` says (4) | correct | `utyOptionsFileRead` uses `getenv("HOME")` + `CONFIGDIR` |
| 36 | Shutdown assert from `munmap(heap, 0)` (5.4) | correct | `utility.c:3460-3462` |
| 37 | HW1 has 59 ship types; CET has 145 (`sRecon` 0 ... `nisLensflareSource1` 144, `STD_LAST_SHIP bMissileCorvette` = 59) and 7 races (3) | correct | `enum.py` on GoK `ShipDefs.h`; CET `ShipDefs.h`, `RaceDefs.h` |
| 38 | Strings: 590 HW1 ids, 364 present, 226 missing, led by `strAdvanceSupportFrigate` (2.5) | correct | `strs.py` (independent parser): 590/364/226. Cataclysm-only keys came out as 1,317 against the doc's 1,318, which is within parser fuzz on one key |
| 39 | `aiship.script` has 108 `rowDetails`, 103 unknown, the 5 kept are `p1*` (stage 3) | correct | 108 rows; 6 `p1*` rows exist, but `p1Outpost` has no HW1 type, so 5 are kept. `hw1-assets/aiship.script` has exactly those 5 |
| 40 | 19 `hyperspace` rows, 48 crate rows, `ExcludeShips RACE_P1,All` at mission01.mission line 22 | correct | grep counts; line 22 |
| 41 | Guns: `GUN_HiddenGimble` 153, `GUN_VisibleGimble` 93, `GUN_Fixed` 4, `GUN_MissileLauncher` 20, `GUN_MineLauncher` 1 (2.3) | correct | active `Type` lines (3 + 2 more are commented out with `;`) |
| 42 | `fowRadius` in 138 `.shp`; 67 root `.script` | correct | grep / ls |
| 43 | Missing `.shp` disables the ship silently (`Universe.c` ~2040); `.peo` paged substitution (`Mesh.c:399-441, 868-873`) | correct | source read (line numbers within 1-2 of those cited) |

## 5. Run ledger (`runs.txt`) against the doc

| # | claim | result | evidence |
|---|---|---|---|
| 44 | run0 and run0c are listed with no arguments (5.1) | **wrong** | `runs.txt` has `run=run0 ... args=/nodebugInt` and `run=run0c ... args=/nodebugInt`. These runs come last in the ledger, after run29; this also qualifies "`/nodebugInt` (from run4 on)". It changes no conclusion: both stops come before any `dbgFatal`. **Fixed** in both rows |
| 45 | rc/wall values for the other rows (run1-run29) | correct (sampled: run3, run4, run16, run20, run27, run28) | match the ledger |

## 6. Hygiene

| # | check | result | evidence |
|---|---|---|---|
| 46 | The doc never asks to commit or copy game data into git | correct | grep for commit / `git add` / "check in" finds nothing; every data path points to `original/scratch-m2/` |
| 47 | Nothing derived from game data is in the worktree | correct | `git status --porcelain --untracked-files=all` lists 637 paths: `docs/`, `src/` (HW1 source + `COPYING.REULA`), `tools/`, `meson*`, `ghidra/delta_map*.json`, `README.md`. `scan.py` found no file with a game-data extension, no game-format magic and no binary content. The largest file is `ghidra/delta_map.json` (4.9 MB, another item's output) |

## Unverifiable or not settled (notes)

- **run23 is missing from the doc.** It used `/demoPlay demos/abc640x480.dem`, printed the usage text and exited with rc 0 after 7.9 s, so it was an attempt with a bad argument. No conclusion depends on it.
- **The first run16 attempt is not explained.** `runs.txt` has an earlier `run16` with rc 213 and 14 lines; its log was overwritten by the second attempt.
- The run29 `.fib` count (10, with `FEMAN/Horse_Race.fib`) was confirmed from the producer's `run29.FileLoads.log`. The stage-15 experiment (runs 20, 24-27, 29) was not re-run; it was not part of the requested re-run.
- The truncation count for biggie was reproduced, but the exact six missing names depend on where the 4 KiB stdio buffer boundary falls; the doc's names were not checked one by one.
- Literal-check counts (72/240/149), the ship-type stem matching (59 rows -> 37 distinct) and the 88 unknown `.shp` keys were checked for internal consistency only, not recomputed. The table sums are consistent: 59 rows, 86 Cataclysm-only types, 22 HW1-only types.
- `original/game/` also holds `update102.big`, which the doc does not mention. HW1's `bigFilePrecedence[]` never opens it, so this changes nothing here. What it contains is for the archive docs to answer.

## Score

- Checked claims: 47 (rows 1-47 above).
- Correct: 44. Wrong: 3 (rows 14, 29, 44). Unverifiable: none among the counted rows; the open points are in the notes.
- Precision = 44 / (44 + 3) = **0.936**.
- Fixes made in `docs/m2-r1-data-deltas.md`: **5 inline corrections** marked `[audit: ...]`. They cover the 3 wrong claims: the `.fib` count in 1.3 and 2.6, `p1outpost` object 0 -> 7 in 2.2, and the run0 and run0c arguments in 5.1. The status line now notes the audit.
- None of the errors touches a conclusion. The first-blocker result (mesh v0x500 at `Mesh.c:917`, `energybolt.geo`) was reproduced exactly, with an identical load trace.

Verdict: PASS-WITH-FIXES - archive listing, 10 missing/renamed entries, 10 format headers, the 116-byte mesh layout and the first blocking failure all reproduced; 3 minor factual errors (a count, an object index, two ledger argument cells) fixed inline.
