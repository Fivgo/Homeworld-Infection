<!-- done: mesh, textures, script, shipstatic, savegame, fib -->
# Audit: M2-R1 item 2 "formats" (docs/formats/*.md, tools/formats/*_probe.py)

Adversarial audit of the six format documents and six probes. Date 2026-10-10. No commit.

## Method

* Cataclysm side: functions cut out of `ghidra/exports/cataclysm_gog_r5/decompiled.c` by their `decompiled_index.csv`
  line ranges. Where Ghidra's C was unreliable I used `objdump -d -M intel` on `original/game/cataclysm.exe`. I also read
  `.data` directly from the PE file with a small Python reader in `/tmp/hwi-audit-fmt/` (key tables, limits, ship headers,
  initial values). The exe was only read, never run. Ghidra was not run.
* HW1 side: the producer's extraction of `ad1bb82` in `/tmp/hwi-m2fmt/orig/src`. I confirmed it is genuine by checking that
  `git show ad1bb82:src/Win32/texreg.h`, run with `GIT_DIR` set to the full GoK clone, is byte-identical to it. HW1 struct
  sizes are taken from the producer's clang layout dump `/tmp/hwi-m2fmt/ss_layout.txt`. I checked them by hand where a
  hand count was possible: `GunStatic` 252 and `CustShipHeader` 72 both match.
* Probes: each probe was run with `python3 -I` on at least two real files other than the producer's examples. The mesh
  and fib probes were also run over every file. All of these files are under `original/game_extracted/`. The savegame
  probe could not be run on real data (see below). None of the six probes opens a file for writing, copies a file or
  imports shutil (checked with grep).
* Goal: check 5 or more claims per document (layout fields, keys, diff rows with their function and address). Fixes
  are marked inline in the producer's documents with `[audit: ...]`.

## mesh.md

| # | Claim | Result | Evidence |
|---|---|---|---|
| M1 | `polygonobject` is 0x74 B (HW1 0x70) | correct | `meshLoad` 0x004aec90: `ppnVar16 = ppnVar16 + 0x1d` (29 dwords); the HW1 mesh.h layout sums to 0x70 |
| M2 | +0x18..+0x24 are fixed up unconditionally; +0x28/+0x2C/+0x30 and the name only when non-zero | correct | `meshLoad`: `ppnVar16[-1],[0],[1],[2]` are always fixed, `[3],[4],[5]` only under `!= 0`. The base is `&object[0].pNormalList` (+0x1C in Ghidra's HW1 type) |
| M3 | `pNormalList` is at +0x20; a new per-vertex 16 B array at +0x1C goes to `shColourSet0` | correct | asm 0x4b0224 `edx=[ebp+0x1c]` and 0x4b0227 `eax=[ebp+0x20]`. At 0x4b0424 the normals come from `[esp+0x14]` (= +0x20). At 0x4b045e, `[esp+0x10]` (= +0x1C) + iV*0x10 is pushed as the 3rd argument of the call to 0x50dfe0. Ghidra's C mislabels these, and the doc says so |
| M4 | `pPolygonList` is at +0x24; polyentry is 40 B | correct | asm 0x4b022d `esi=[ebp+0x24]`; `meshObjectFixupUV` 0x004af220 steps `prVar5 + 10` floats |
| M5 | Header: oName +0x0C, nPublic +0x18, nLocal +0x1C, oPublic +0x20, oLocal +0x24, nPolygonObjects +0x28. Version is never read; only `strncmp("RMF99ba")` | correct | `meshLoad` locals `local_d44` (header), `local_d38`, `local_d2c`, `local_d28`, `local_d24`, `local_d20`, `local_d1c`; `memAlloc(size-0x24)`; the only compare is `_strncmp(...,"RMF99ba",8)` |
| M6 | `mainAllowPacking` `.data` 0x008b8614 = 1 | correct | PE bytes `01 00 00 00`. 0x00a74d90 (`mainOnlyPacking`) is in bss with no initial value, which fits the open point |
| M7 | HW1 cites: mesh.h:40/56, mesh.c:347/414/430/598, shader.c:701 | correct | grep of the ad1bb82 files |
| M8 | Counts: 1,729 files, 762 .peo / 967 .geo, 761 RMF99ba, 2 files at 0x402, 400 with shared spans | correct | Full probe run: 1,729 OK / 0 fail, 761 RMF99ba, 968 RMF97ba (967 .geo + the one 0x402 .peo), 2 files at ver=0x402, 400 with shared_arrays > 0 |
| M9 | Probe output for scarrier.peo is `shared_arrays=8` | **wrong, fixed** | Re-running prints `shared_arrays=40`, and the count is deterministic (mesh_probe.py:74-78). The doc's sample output was stale |

Probe runs: `scarrier.peo`, `defaultship.geo`, `race_beast/bacolyte/rl0/lod0/bacolyte.peo` and `race_beast/bacv/rl0/lod0/bacv.peo`
are all OK with plausible counts (6 and 2 objects, 177 and 234 vertices). The full run over all 1,729 files is OK.

## textures.md

| # | Claim | Result | Evidence |
|---|---|---|---|
| T1 | `.lif` header is 0x30; offsets +0x20..+0x2C are fixed up | correct | `trLIFFileLoad` 0x00586aa0 adds the base to +0x20, +0x24, +0x28 and +0x2c |
| T2 | `textures.ll` header is 0x1C (nElements +0x0C, stringLength +0x10, sharingLength +0x14, totalLength +0x18); an element is 32 B with nShared at +0x14 and sharedTo at +0x18 | correct | `trListFileLoad` 0x005878f0: `fileBlockRead(...,0x1c)`, `memAlloc(local_4)`, loop step `ppsVar3 + 8`, test on `ppsVar3[-1]`, and sharedTo += base + n*32 + stringLength. This matches the HW1 `llfileheader` in texreg.h |
| T3 | `texreg` is 0x34 B (HW1 release 0x2C); fileName +0x1C, handle +0x20, sharedFrom +0x28 | correct | `trStartup` 0x00584d30 allocates `DAT_008bb688 * 0x34`. `trTextureRegister` runs stricmp on +0x1c, sets `+0x20 = 0xffffffff` and tests `+0x28 == -0x204050b` (0xfdfbfaf5). The HW1 texreg.h struct with `TR_DEBUG_TEXTURES 0` is 0x2C |
| T4 | New fields: shift X/Y at +0x2C/+0x2E, and a scalable flag at +0x30 that is the 4th argument | correct | `trTextureRegister` sets `+0x30 = param_4`. `trCramRAMComputeAndScale` 0x00589a50 zeroes +0x2c/+0x2e and tests `psVar1+0x18` (+0x30). It bumps `[0x16]` when height < width and `[0x17]` otherwise, and only for textures larger than 8 |
| T5 | Every caller passes 1 except the subtitle code, which passes 0 | correct | All 13 `trTextureRegister(` call sites in decompiled.c pass 1 as the 4th argument, except decompiled.c:340696 (`subThemePictureSet`) and :341044 (subtitle reset), which pass 0 |
| T6 | Cites texreg.h:29 (`TR_ERROR_CHECKING 0`) and texreg.h:112 (`LL_MaxElements`) | **wrong, fixed** | In ad1bb82 texreg.h the release define is on line 36 and `LL_MaxElements 12000` is on line 114 |

Probe runs: `textures.ll`, `derelicts/crate/rl0/lod0/fronts.lif`, `speechicons/r1/fleetcommand.lif`,
`race_beast/bsentinel/rl0/lod1/sen_3.lif` and `race_beast/bsentinel/rl0/lod1/sen_11.lif` are all OK. The ll file gives
n=12403, strings=547729 and sharing=15464, as documented.

## script.md

| # | Claim | Result | Evidence |
|---|---|---|---|
| S1 | The line buffer is 650 (0x28a), as in HW1 | correct | `scriptSetStruct` 0x00526260 `fileLineRead(handle,local_28c,0x28a)`; HW1 statscript.c:65 `MAX_LINE_CHARS 650` |
| S2 | `//` is no longer stripped when the line contains `http:` | correct | `parseLine` 0x00526120: `_strstr(line,"http:")` (0x00897e04) comes before `_strstr(line,"//")` (0x0087bb18). Both strings were read from the PE. HW1 statscript.c:705-707 always strips `//` |
| S3 | `scriptStructEntry` is 16 B and the field address is `(offset1 - offset2) + struct` | correct | decompiled `scriptSetStruct`: `(*psVar9->setVarCB)(directory, value, (offset1-offset2)+structureToFillIn)` |
| S4 | New table `cat_beastWeaponScriptTable` 0x00751698: stride 16, 6 entries, first key `beastAttackCascadePoints` | correct | PE table dump: 6 entries, all `scriptSetReal32CB` 0x005259d0, base 0xc52140 |
| S5 | New popup/stats texture table 0x0088cb48: 53 entries, `cat_feOverlayIconTextureSetCB` | correct | PE dump: entries 0..52 (`PopupMenu_BorderCorner_TopLeft`..`X`) with callback 0x004dbda0, which functions.csv names `cat_feOverlayIconTextureSetCB` |
| S6 | Table sizes: `ShipStaticScriptTable` 178, `utyOptionsList` 94, tactics 198, formation slots 63 | correct | PE dumps end after 178, 94, 198 and 63 entries |
| S7 | `cat_scriptSetSoundTypeObsoleteCB` "prints" its warning | **wrong, fixed** | 0x526050 is `push 0x897d18; call 0x6fe9a0; pop ecx; ret`, and 0x6fe9a0 is `nullsubReturnFalse` (`xor eax,eax; ret`). Retail prints nothing |

Probe runs: `script_probe.py --exe ... --export ... --hw1-src ...` on `tweak.script`, `race_sect/scarrier.shp`,
`race_beast/bacolyte.shp` and `singleplayer/mission01/mission01.mission`. All parse. The rebuilt tables are 105 tables
with 4,428 keys, as documented. The `tweak.script` (453/16/36) and scarrier.shp lines are identical to the doc.

## shipstatic.md

| # | Claim | Result | Evidence |
|---|---|---|---|
| SS1 | Field offsets for 29 keys: e.g. maxhealth +0xA4, preciseSelection +0x88, externalBuild +0x1F0, forceFieldProtectionDistanceScale +0x1F4, upgradeTimeModifier +0x1F8, buildTime +0x23C, fowRadius +0x240, RKOEatMeTime +0x250, repairDroids +0x320, N-LIPS +0x30C, hyperspaceBoxScale +0x3D0, beastAttackConvertTo +0x3E4, Overlay_Selection_Scalar +0x3E8 | correct (29/29) | Computed as offset1 - offset2 from the `ShipStaticScriptTable` 0x008b2428 PE dump |
| SS2 | `ShipStaticInfo` is 0x458 (HW1 0x370) | correct | decompiled.c `&ShipStaticInfos + type * 0x458` (5 sites). The clang dump gives HW1 `ShipStaticInfo` sizeof=880 |
| SS3 | `CustShipHeader` is 0x23 dwords copied to +0x108; slots 20 move, 21 Attack (+0x15C), 22 dock, 27 halt; slot 0 = sizeofShipSpecifics | correct | `InitStatShipInfo` 0x0053bab0 has a 0x23-dword copy loop to `statinfo + 0x108`. I dumped the headers from `shipHeaders` 0x008b1938 (51 distinct non-NULL): slot 21 holds `*Attack` (GenericInterceptor, Acolyte, Mimic, ACV, ...), slot 22 holds `HiveFrigateDockCommand`, slot 27 holds `*HaltCommand`, and slot 0 is 24/92/316 as the doc says. Users of +0x15C are found in `delegateCommand` (call), `processAttackToDo` (load/compare) and `aitDestroy` (load) |
| SS4 | `GunStatic` is 0x11C (HW1 0xFC); new keys at +0x0C/+0x10/+0x14/+0x18 and +0xF4..+0x100; HW1 offsets MinAngle +0x2C, DamageLo/Hi +0xBC/+0xC0, BarrelLength +0xE4 | correct | Gun table 0x008975a0 dump; `InitStatShipInfo` gun stride `0x17*12+8`; HW1 offsets hand-computed from spaceobj.h:50-100 |
| SS5 | HW1 sizes 880 / 72 / 252 | correct | Clang dump lines 2005, 823 and 1119; 72 and 252 also match hand counts |
| SS6 | Diff row: HW1 `CustShipAttack` is "slot 4 (+0x10 in header)" | **wrong, fixed** | HW1 spaceobj.h:522-528: shiptype +0, sizeofShipSpecifics +4, StaticInit +8, StaticInitPost +0xC, Init +0x10, Close +0x14, Attack +0x18 |
| SS7 | `cat_scriptSetSoundTypeObsoleteCB` "prints" | **wrong, fixed** | Same evidence as S7 |
| SS8 | `sRecon.shp` declares `NUMBER_OF_NAV_LIGHTS 0` but has 8 lights | correct | grep of `race_sect/srecon.shp`; the probe reports the same mismatch |

Probe runs: `shipstatic_probe.py --exe ...` on `race_sect/srecon.shp`, `race_beast/bacolyte.shp` and `race_sect/scarrier.shp`.
All parse. bacolyte gives type 34 and scarrier gives type 12, both OK. srecon is flagged FAIL for the documented
declared-count mismatch. The FAIL is a data-consistency flag, not a parser error.

## savegame.md

| # | Claim | Result | Evidence |
|---|---|---|---|
| SG1 | The version is 0x10ad0027 in both games; `PreLoadGame` compares it at 0x4fdff3 | correct | asm 0x4fdff3 `cmp DWORD PTR [esp+0x8],0x10ad0027`. `SaveGame` stores the float 6.8236703e-29, which is 0x10ad0027 as float bits (checked with struct). HW1 SaveGame.h:13 |
| SG2 | `tpGameCreated` chunk is 0x2B0 (HW1 0x268); playerNames is 0x90 at 0x00c39640 | correct | `SaveGame` 0x004fdaf0: `SaveStructureOfSize(&tpGameCreated,0x2b0)` and `(&DAT_00c39640,0x90)`. Clang dump: `CaptainGameInfo` sizeof=616 (0x268) |
| SG3 | New BASIC 0x9C state block (39 dwords from 0x00c32c00) after `universeSaveEverythingNeeded`, before `SaveAllSpaceObjs`; new SP INFO 0x00894194 | correct | `SaveGame`: header bytes `00 00 00 80` / `9c 00 00 00`, a copy loop of `0x27` dwords from `&DAT_00c32c00`, then `SaveAllSpaceObjs`, `SaveAllBlobs`, `cat_saveGamePreGameTable`, `SaveUniverse`. Also `SaveInfoNumber(DAT_00894194)` |
| SG4 | Universe chunk `0x80000020` is 0x3D9BC (HW1 12,448) | correct | `SaveUniverse` 0x005034c0: header bytes `20 00 00 80 bc d9 03 00`, copy count `0xf66f` dwords = 0x3D9BC. Clang dump has sizeof=12448 |
| SG5 | Misc chunks 0x8000B000/0x17A0 @0xa76310, 0x8000A000/0x24 @0xa77ab0, 0x80009000/0x24 @0xa762e8 | correct | `cat_saveGameMiscChunks` 0x005ba170: `CreateChunk(-0x7fff5000,0x17a0,..)`, `(-0x7fff6000,0x24,..)`, `(-0x7fff7000,0x24,..)` |
| SG6 | Pre-game table chunk 0x80004000 with three `{ptr,4}` entries at 0x00892660 | correct | PE dump: 0xa6b204/4, 0xa6b20c/4, 0x883960/4. `cat_saveGamePreGameTable` 0x004fda00 writes type bytes `00 40 00 80` |
| SG7 | `Player` stride is 0x6530 (HW1 392) | correct | asm 0x4fe078 `add esi,0x6530` / 0x4fe081 `cmp esi,0xb99e94`. Clang dump: `Player` sizeof=392 |
| SG8 | Ship chunk is 0x63C + 0x7C (PreciseSelection) | correct | `SaveShip` 0x004ffd70: `uVar11 = 0x63c; ... uVar11 + 0x7c`. Clang dump: HW1 `Ship` sizeof=1136 (0x470) |
| SG9 | The probe parses real saves | **unverifiable** | No save exists. The only files under `original/game/SavedGames/*` are 0-byte `dummy.txt` placeholders. A search of `original/` and `reference/` for files that start with `27 00 ad 10` found none. `--selftest` passes, and the two placeholders are rejected cleanly ("not a save (too short)") |

## fib.md

| # | Claim | Result | Evidence |
|---|---|---|---|
| F1 | Ident "Bananna" / 0x212, never checked in release (FeFlow.h:30, :40-41) | correct | HW1 FeFlow.h lines 30, 40 and 41. `feScreensLoad` 0x004744e0 only calls `nullsubReturnFalse` where the debug checks were |
| F2 | Links are 12 B, fixed only when `flags & 1`; screens are 20 B | correct | `feScreensLoad`: `iVar5 + 0xc` stride with a `& 1` test before fixing +0 and +8 |
| F3 | Atoms are 0x4C with type at +0x0C; menu item (11) or `HyperspaceRollCall` disables re-centring | correct | `pcVar6 + 0x4c`, `*pcVar6 == '\v'` on `atoms+0xc`, `strcmp(.., "HyperspaceRollCall")`. HW1 enum: FA_UserRegion=1, so MenuItem=11, RadioButton=12, ListWindow=16 and BitmapButton=17 |
| F4 | ListWindow width = x2 - 18 - x1; 640x480 on-screen test, FAF_Hidden 0x8000, FAF_Background 0x40000, centring by `(MAIN_WindowWidth-640)/2` | correct | `-0x12 - *(short*)(atom+0x18)` added to +0x20 for type 0x10; bounds 0x27f/0x1df; `uVar9 | 0x8000`; `& 0x40000`; `(MAIN_WindowWidth + -0x280) / 2`. HW1 FeFlow.h:69,74 |
| F5 | Limits are 400/150/150 | correct | PE `.data` 0x0087be0c = 0x190, 0x96, 0x96; HW1 FeFlow.h:125-127 |
| F6 | 22 files, 6,605 atoms, 3,354 of type 0; the update `multiplayer_game.fib` has 1,941 atoms | correct | Probe run over every `.fib` under `game_extracted`: 22/22 OK, type histogram sums to 6,605 with 3,354 of type 0, and the update file has 1,941 atoms |

Probe runs: `front_end.fib`, `beast_research_manager.fib` and `console.fib` are OK, and all 22 files are OK.
Not checked: the sub-claim "542-of-765 in front_end.fib + in_game_esc_menu.fib". The probe's default output does not
break type-0 atoms down by pData or font, and I did not re-derive that number.

## Totals

* Checked claims: 46. **Correct 40, wrong 5, unverifiable 1** (SG9).
* Each "diff vs HW1" row I sampled names a function and address that really does what the row says. Rows checked:
  M1-M5, T3-T5, S1-S3, SS3, SS4, SG1-SG8 and F2-F5. The one wrong row (SS6) had the right function; the error was in
  the HW1 offset.
* **Precision = 40 / (40 + 5) = 0.889.** All five errors are minor and none changes a layout the M2-R2 port would rely
  on: a stale probe sample output, two HW1 header line numbers, one HW1 struct offset in a diff row, and the same
  "prints" wording in two documents. Every Cataclysm-side offset, size, stride and chunk type I checked is correct.

## Fixes made in place (6 edits, 5 findings)

1. `docs/formats/mesh.md` probe output: `shared_arrays=8` changed to `40` [audit].
2. `docs/formats/textures.md`: texreg.h:29 changed to :36 [audit].
3. `docs/formats/textures.md`: texreg.h:112 changed to :114. The same fix was applied in the summary and in the diff table [audit].
4. `docs/formats/script.md` diff table: the SoundType obsolete handler prints nothing in retail [audit].
5. `docs/formats/shipstatic.md` diff table: HW1 `CustShipAttack` is at +0x18 in the header, not +0x10 [audit].
6. `docs/formats/shipstatic.md` summary: the same SoundType wording fix as edit 4 [audit].

## Notes / not settled

* **The savegame layout is checked only against code, not data.** The brief requires each probe to be run on two real
  files, and that cannot be done for savegame without making saves in the GOG build, which this round forbids.
  Someone, for example the user during a play-test, needs to make one single-player save and one skirmish save, then
  run `tools/formats/savegame_probe.py -v` on them.
* Ship-header slots 22 and 27 also hold `*MoveCommand` for ACV and RammingFrigate (slot 22 = `ACVMoveCommand` /
  `RammingFrigateMoveCommand`). This does not contradict the doc's "dock command" label, but that label rests on
  `clDock` calling the slot. The names inside the headers are Round-5 names and may be imprecise.
* The `/tmp/hwi-m2fmt` HW1 extraction and clang dump are outside the repo and will be lost when the machine restarts.
  The docs cite `ORIG@ad1bb82` paths, so they can be re-derived with `git show ad1bb82:...` from a full GoK clone.

Verdict: PASS-WITH-FIXES - 40 of 45 settled claims correct (precision 0.889); 5 minor errors (stale probe output, 2 HW1 line numbers, 1 HW1 offset in a diff row, "prints" wording) fixed inline; all Cataclysm offsets, sizes and chunk types checked are correct; the savegame probe could not be run on real saves because none exist
