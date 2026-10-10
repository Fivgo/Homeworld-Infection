# Audit: M2-R2 port "startup" (producer 4)

Audited: `docs/m2-r2-startup.md` and the uncommitted diff of branch `m2` against HEAD fe8645d (24 tracked files,
+969/-162 before the audit). Date 2026-10-10. Ghidra was not run.

Evidence came from these sources:
- Code: `ghidra/exports/cataclysm_gog_r5/decompiled.c`, read one function at a time with the audit's own extractor
  `build/audit-startup/fn.sh` and `build/audit-startup/grepfn.py` (gitignored). I also used `strings.csv`,
  `functions.csv` and `ghidra/delta_map.json`.
- Exe data: `original/game/cataclysm.exe`, read with the audit's own PE reader `build/audit-startup/pe.py`. The
  producer's `pe.py` was not used.
- Data: `textures.ll`, taken from `cataclysm.big` with `tools/bigfile.py cat`, and the TOCs of both archives
  (`tools/bigfile.py list`). Both are in `original/scratch-m2/audit-startup/`. The census script is
  `build/audit-startup/llcensus.py`.
- Harness runs: `original/scratch-m2/m2r2-harness/runs/audit-startup-{fe,textures}`. These ran on the plain harness
  with the audit build after the fixes.

## 1. Build from clean

- I deleted `original/scratch-m2/build-audit-startup` and ran `meson setup` there with the arguments of
  `tools/build.sh`. The debug build used `--buildtype=debug -Db_sanitize=address,undefined` and the release build
  `--buildtype=release -Db_sanitize=none`. Then I ran `meson compile` in each. meson.build sets
  `warning_level=3` and `werror` true.
- Both builds finished with rc 0 and `[292/292] Linking target hwi-engine`. Neither log has a `warning:` line.
- After the audit fixes, both audit dirs rebuilt incrementally (`[64/64]`, 0 warnings). `tools/build.sh --debug`
  and `--release` in the tree are also green (rc 0, 0 warnings). `tools/m2_layout32.sh` reports `OK`, with ILP32
  `texreg` 52 and `SENTENCELUT` 16.

| build | file size (clean, before fixes) | after fixes | text | data | bss |
|---|---:|---:|---:|---:|---:|
| debug | 75,609,624 B | 75,609,640 B | 41,422,932 | 24,017,704 | 2,529,760 |
| release | 5,205,784 B | 5,205,784 B | 4,221,685 | 249,712 | 1,990,208 |

The release file is the same size as the producer's. Its sha256 (`faaf2be4...`) differs from the producer's
`d37afcac...` (that one is `build/release`) because the two builds ran in different directories. That is expected
and not a finding.

## 2. Exe checks: texreg, the speech LUT and the archive names

Each row is one claim of the port, checked against the decompiled exe or the exe's data.

| # | claim | exe evidence (audit) | result |
|---|---|---|---|
| 1 | Registry entry is 0x34 bytes | `trStartup` 0x00584d30: `memAllocFunction(DAT_008bb688 * 0x34, 8)` | OK |
| 2 | `bScalable` is the 4th argument and is stored at +0x30 | `trTextureRegister` 0x00585760: `*(DAT_00a75fa8 + 0x30 + i) = param_4` | OK |
| 3 | `sharedFrom` is at +0x28 | 0x00585760 compares `*(int *)(+0x28) == -0x204050b` (= `TR_NotShared` 0xfdfbfaf5) | OK |
| 4 | The ILP32 layout asserts (0x28/0x2C/0x2E/0x30, size 0x34) hold | `tools/m2_layout32.sh` OK, texreg 52 | OK |
| 5 | `scaleShiftX` is at +0x2C and shifts the width (+0) | `trCramRAMComputeAndScale` 0x00589a50: `*psVar1 >> psVar1[0x16]` | OK |
| 6 | `scaleShiftY` is at +0x2E and shifts the height (+2) | 0x00589a50: `psVar1[1] >> psVar1[0x17]` | OK |
| 7 | `trTextureRegister` "zeroes the shifts", cited to 0x00585760 | 0x00585760 writes +0x8,+0xc..+0x30 but never +0x2C/+0x2E | **wrong** (doc; see F5) |
| 8 | Size per entry is w*h when paletted, otherwise nPalettes*w*h*2 | 0x00589a50: `if ((+0xc & 2) == 0) size = *(char*)(+0xd) * size * 2` | OK |
| 9 | Limit is `.data` 0x008bb698 = 0x00a00000 | `pe.py dwords 0x008bb698` = `00a00000` | OK |
| 10 | Mission 17: max(0x100000, limit * 0.3f) | 0x00589a50 `currentMission == 0x11`, `< 0x100000`; `.data` 0x0072b064 = `3e99999a` (0.3f) | OK |
| 11 | Halve the longer side when `bScalable` and w > 8 or h > 8 | 0x00589a50: `+0x30 != 0`, `8 < sVar6 \|\| 8 < h`, `sVar8 < sVar6 ? shiftX++ : shiftY++` | OK |
| 12 | Stop when the total fits or a whole pass changes nothing | 0x00589a50: `if (!bVar2) goto LAB_00589c5b` | OK |
| 13 | Then: loaded and smaller -> `trPreLoadedTextureScale`; pending -> set scaled size | 0x00589a50 last loop (`+0xc & 1` = `TRF_Pending`) | OK |
| 14 | `trRegistryRefresh` calls it without a sort, right after freeing the listing | `trRegistryRefresh` 0x00587a50 lines 543-545: `memFree(local_170); ... trCramRAMComputeAndScale();`; no `qsort` | OK |
| 15 | `trListFileLoad` has no element limit | 0x005878f0 reads the header and allocates, with no check | OK |
| 16 | `trImageScaleIndexed` has no power-of-two check | 0x00586af0: same point sampling, no check | OK |
| 17 | 4th argument at each caller (subtitles 0, all others 1) | `grepfn.py --calls trTextureRegister`: the 2 subtitle sites push 0; etgSubTextureDWORD, etgSetTextureParse, meshTextureRegisterAllPlayers (x2), meshRecolorize, scriptSetNAVLightCB and trailStartup push 1; the port's 9 sites match | OK |
| 18 | "10 callers" | 9 call sites in 7 functions; the 10th is the port's self-test | **wrong** (doc; F6) |
| 19 | `SENTENCELUT`: bitrate at +0x0E, table at +0x10 | asserts hold (layout32: 16); every reader below agrees | OK |
| 20 | `SEselectsentence` reads +0x12 + ((numevents*actor+event)*numcolumns)*2 | 0x005223c0 lines 28-30; port `lookup[... + 1]` | OK |
| 21 | `speechEventQueue` reads the priority at +0x10 | 0x00522560 line 214 | OK |
| 22 | `speechPlayQueue` reads the bitrate at +0x0E | 0x005220e0 line 36 | OK |
| 23 | `speechEventInit` creates its speech buffers with the +0x0E bitrate and starts at `soundstreamquery` | 0x00521a40 lines 31-42 | OK |
| 24 | `cat_speechEventLoadRaceVoice`: frees, picks Sect (race 0) or Beast, loads `SoundFX\` + race + `sentence.lut`/`phrase.lut`, compares the checksums, opens the race's .vce and compares again | 0x005217f0 (whole body read) | OK |
| 25 | `cat_speechEventInitForRace`: first-time flag, `speechEventInit` once, frees everything on -1 | 0x00521d50; `SOUND_OK` 0 / `SOUND_ERR` -1 (`soundlow.h`) | OK |
| 26 | `soundEventInit(race)` -> `cat_speechEventInitForRace`; speech is switched off on failure; `utyGameSystemsInit` passes `whichRaceSelected` | 0x0051c6e0 lines 175-178 (`DAT_008b8640 = 0`); 0x005abe70 line 112 | OK |
| 27 | `gameStart` reloads the race set after LoadGame/universeSwitchToPlayer, guarded by speech on | 0x005a9290 lines 475-480 | OK |
| 28 | "two sites" in `gameStart` | one site; the other copy is `utyFatalErrorWaitLoop` 0x005a91d0, which Ghidra runs on into gameStart | **wrong** (doc; F7) |
| 29 | `SEloadscripts`: only fleet and intel, then eq[8] = eq[7] | 0x00521a40 lines 145-150; `STREAMEQ` is 0x28 bytes, so eq[7]/eq[8] are at +0x20/+0x24 (0xc097e0/0xc097e4) | OK |
| 30 | Speech `.data` strings | `strings.csv`: 0x00895f60 `Music4Cat.wxh`, 0x00896144 `Sect`, 0x0089613c `Beast`, 0x0089612c `sentence.lut`, 0x008960fc `phrase.lut`; `pe.py`: 0x00895bf4 `SoundFX\` | OK |
| 31 | Archive names | 0x008c0ef8 `Cataclysm.big`, 0x008c0f38 `Update.big`; 0x005ab810 line 212 `bigOpen(utyBigFilename, utyUpdateBigFilename)` | OK |
| 32 | `bigFilePrecedence`: Update.big is optional and overrides; Cataclysm.big is required | 0x005ab810: `updateNewerAvailable` only when the update TOC is open (lines 228-240); `bigCheck(utyUpdateBigFilename)` | OK |
| 33 | Start-up file checks: music, voice, Beast voice, each with the CD fallback | 0x005ab810 lines 164-173 (`fileExists(x,0)` then `(x,0x80)`); 0x008c0f08/18/28 | OK |
| 34 | GoK's age array is too small; the exe keeps 1 byte per entry | `BigFile.h:145` is a 4-byte enum array; GoK allocated `numFiles` bytes. 0x005ab810 lines 215-225: `memAllocFunction(mainTOC,0)`, cleared bytewise | OK |
| 35 | The case-correct open uses the on-disk spelling | `fileExists` -> `filePathPrepend` (filePathTempBuffer) -> `fileNameCorrectCase` in place (`File.c` 1062-1081, 1895-1926) | OK |

## 3. Diff review

I read the whole diff. It has 98 `src/` hunks.

**CAT comments.** `build/audit-startup/hunks.py` lists 21 hunks whose added lines have no `CAT`. I read each one. All
of them sit inside or next to a commented CAT block: the 88-row chatter table under its CAT header, the
`#if/#endif` lines of the demo split, the widened texture initialisers, the `rmGUIShutdown` loop bodies, and the
`trTextureRegister` signature, whose header comment carries the CAT note. There are also two forward declarations
of the new `cat_bat*SetCB` callbacks. No Cataclysm change is left untraceable, so there is no finding here.

**HW1 IDENTICAL functions that were edited (F8).** `delta_map.json` classes 10 of the edited functions as
IDENTICAL (all at medium confidence): `cmCloseTextures`, `lmFreeTextures`, `rmGUIShutdown`, `feAllCallOnCreate`,
`etgSetTextureParse`, `meshRecolorize`, `scriptSetNAVLightCB`, `subThemePictureSet`, `subTexturesReset` and
`trImageScaleIndexed`. I checked each edit against the exe:
- `cmCloseTextures` 0x004551e0 has a 3rd loop that frees the 5 support textures (0x00a2dca0/0x00754a7c).
- `lmFreeTextures` 0x0049df00 has a 2nd loop over 15 entries (0x00a4474c/0x00883578).
- `rmGUIShutdown` 0x004f7970 frees the 15+6 research tables (0x00c17ac0/0x00c16800, 0x00c168a0/0x00c17b00).
- `feAllCallOnCreate` 0x00474050 inlines the callback search and has no "not found" path.
- The five texture callers push the 4th argument (row 17 in section 2).

So in 9 of the 10 the exe body differs from HW1, the delta map's IDENTICAL is wrong, and the port's edit is the
faithful one. The 10th, `trImageScaleIndexed`, is really IDENTICAL in release code: the only change drops a
debug-only assert, which the exe does not have either.
- Recommendation, not applied: reclassify the 9 as MODIFIED when the delta map is next regenerated.
- I did not edit `delta_map.json` (it is not in this port's scope).

**Changes outside the producer's scope (F9).**
- `BigFile.c` now sizes the age array correctly. This fixes a GoK defect, not a Cataclysm delta, and is outside
  the brief. The producer flagged it. It is proven by row 34, and without it the mesh self-test on the plain
  harness fails (the producer's runs `startup-final-meshes` and `-meshes2`). Kept.
- The fix carries a `CAT:` tag although it is a port defect. The comment says so explicitly, so it stays
  traceable.
- `utility_mac.c` is not built. Its change is a one-line signature change, and it is kept.
- `/catTexturesTest` and the harness `--mode textures` are port diagnostics, labelled as such. They are in scope as
  the textures evidence.

**Front end (checked against `.data`).**
- `cm` icons: table 0x00754a20, 6 names. Support names: table 0x00754a68, 5 names, path 0x00754eac.
- `lm` icons: table 0x00883508. Launch-system buttons: table 0x0088353c, 15 names, path 0x00883520.
- Research art: lab names at 0x00891e80 in the port's order (disabled x5, off x5, on x5); Beast frames at 0x00891ec8;
  prefixes 0x00891e7c `Research_icon_` and 0x00891ec4 `bresearch_ani_`; font 0x008912f0.
- The support textures and launch buttons are created as RGB in the exe (`cmLoadTextures` line 158,
  `lmLoadTextures` line 162).

All of these match.

**Battle chatter.**
- `build/audit-startup/chatcmp.py` compared the port's 88 rows with the exe table 0x0074e8d0 (stride 0x58) and found
  0 mismatches in names and ids. The exe's zero-id slots are 76, 77, 81 and 87. The maximum speech index is 197,
  and no id sets bit 0x400, so HW1's 0x3ff mask is safe.
- Entry 88 (0x00750710) has no name, which ends the table.
- `cat_batStatusUpdateSetCB` 0x004235f0 writes `+0x50` (0x0074e920 - 0x0074e8d0) when 0.0 < value.
- Script table 0x00750758 holds 21 12-byte entries: entry 7 is `battleStatusUpdate`, entry 8
  `battleStatusSuppress`, then `batGlobalFrequencyModifier` and `batMouthpieceTries`.

## 4. DONE WHEN, re-checked

Run `audit-startup-fe`: audit debug build after the fixes, plain harness, `--mode fe`, 40 s, no `/noSpeech`.
- Harness dir: `data/` holds only symlinks to install names (`cataclysm.big`, `update.big`, `Music4Cat.wxd`,
  `SectSpeech.vce`, `BeastSpeech.vce`, `default_keys.cfg`, `devstats.dat`, `credits`, `feman`, `Movies`) and the
  empty writable dirs. There is no `update102.big`, no renamed copy and no workaround stage.

| criterion | audit result |
|---|---|
| rank 9 (names) | `39 files found in Update.big`, `16528 files found in Cataclysm.big`; `Music4Cat.wxd` and `SectSpeech.vce` opened (FileLoads `[F]`) |
| rank 4 (speech LUT, SIGFPE) | `SoundFX/Sectsentence.lut`, `Sectphrase.lut`, `Music4Cat.wxh`, `eq_fleet`/`eq_intel` loaded. The log has no checksum message, no FPE and no `eq_pilot`. |
| rank 6 (chatter) | `BattleChatter.script` loaded, no "Unable to find battle chatter event" |
| rank 8 (FE art) | Loaded: 6 `Build_race1_icon*` (x2), 5 `b_Build_Support*`, 15 `R1_Sys*`, 15 `Research_icon_*`, 6 `bresearch_ani_*`, `EuroseWideHeavy_i8.hff` |
| rank 10 (FE callback) | No "not found" / `Main_game_screen-TO-` line. `grep -a` finds no `Main_game_screen-TO-` in cataclysm.exe, and `strings.csv` has none. |
| `Main_game_screen` reached | milestone `Main_game_screen` (`feScreenPush: pushed screen 1`), 232 file loads, 0 ASan errors. The first stop is GoK's `munmap(heap, 0)` assert at shutdown after the timeout (`utility.c (3478)`, data-deltas 5.4), not a data stop. |
| `textures.ll` fully registered | Run `audit-startup-textures`: `12403/12403 found, 0 mismatches, 0 bad sharedFrom`; `1742 without a file, 10661 registered in 3 batches, 10661 loaded, 0 not loaded`; 0 ASan. My own census of `textures.ll` against both archive TOCs also gives 12,403 elements, a string block of 547,729 bytes (odd), 1,742 rows with no `.lif` of their own or of their share parent, and exactly one non-power-of-two texture, `race_beast\bcruisemissile\rl0\LOD4\4bcr5` (32x90). |

The DONE WHEN is met. "Fully registered" means every listed texture whose file the install ships. The other 1,742
rows have no file, as the census confirms. Whether any game data references them is UNVERIFIED, as the producer
says.

Non-fatal UBSan reports are unchanged from the producer's runs. They are the known misaligned `fescreen`/`PATCH`
accesses plus 2 kinds in `fquant.c`, both stage 3.

## Findings

| id | severity | finding | evidence | action |
|---|---|---|---|---|
| F1 | code, medium | `cmDrawShipImage` kept HW1's `(shipID == 0) ? 0 : 1` after the table grew to 6 images. The Mothership button drew `icon1_disabled` and the carrier drew `icon1_off` (Mothership art). The producer fixed the same thing in `lmDrawShipImage` only. | `ConsMgr.c` 3836; exe `cmDrawShipImage` 0x004589c0: base `-(shipID != 0) & 3`, +1 off, +2 on | **fixed**: `? 1 : 4` with a CAT comment, the same scheme as `lmDrawShipImage`; row added to the producer doc |
| F2 | comment | `SoundEvent.h`: inserting the new prototypes moved `soundEventClose`'s HW1 comment onto the `#endif` line | diff | **fixed** (HW1 line restored verbatim) |
| F3 | comment | `Battle.c`: the "battleStatusUpdate entry 0x007507b0 / battleStatusSuppress entry 0x007507bc" addresses are the callback fields. The entries are 12 bytes and start at 0x007507ac/0x007507b8. | `pe.py dwords 0x00750758 63` | **fixed** |
| F4 | info | `trTextureRegister` zeroes `scaleShiftX/Y`, which the exe does not do. Harmless, because `trCramRAMComputeAndScale` zeroes the shifts of every listed entry first, in both. | row 7 | doc annotated (F5) |
| F5 | doc | "stores `bScalable`, zeroes the shifts \| 0x00585760" attributes the zeroing to the exe | row 7 | **fixed** `[audit: ...]` |
| F6 | doc | "10 callers" should be 9 call sites in 7 functions | row 18 | **fixed** `[audit: ...]` |
| F7 | doc | "two sites" in `gameStart` should be one; the second is a Ghidra fall-through copy | row 28 | **fixed** `[audit: ...]` |
| F8 | process | 10 delta-map-IDENTICAL functions edited; 9 of them differ from HW1 in the exe | section 3 | justified; recommend reclassifying as MODIFIED |
| F9 | scope | GoK archive age-array fix (non-Cataclysm, labelled CAT) | row 34, producer runs | justified, kept |

## Precision

I sampled 49 checks: the 35 rows of section 2, 7 front-end/chatter/diff checks (names tables, close loops,
`cmDrawShipImage`, `feAllCallOnCreate`, chatter table, status flags, script-entry address) and the 7 DONE WHEN
criteria. 5 are wrong: rows 7, 18 and 28, F1 and F3. Precision = 44/(44+5) = **0.898**. None of the wrong ones
affects the gate.

## Fixes

There are 6 fixes in place:
- 1 code fix: `ConsMgr.c` `cmDrawShipImage`.
- 2 comment fixes: `SoundEvent.h` and `Battle.c`.
- 3 producer-doc annotations, plus the doc row for the code fix.

Both build configurations are green after the fixes, in the audit dirs and in `build/`. The harness re-run after
the fixes reaches `Main_game_screen`.

Verdict: PASS-WITH-FIXES - ranks 4, 6, 8, 9 and 10 cleared and Main_game_screen reached on the plain harness without /noSpeech (reproduced with the audit build); texreg 0x34, the cram, the speech LUT and archive names match the exe in 32/35 checks (3 doc errors); textures.ll 12,403/10,661/1,742 reproduced independently; 1 code defect fixed (cmDrawShipImage carrier image), 2 comments and 3 doc claims corrected; 9 delta-map IDENTICAL misclasses noted
