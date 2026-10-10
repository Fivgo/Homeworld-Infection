<!-- done: names, speech, chatter, feart, textures, runs -->
# M2-R2 port: start-up names, audio, front-end art, textures (producer 4)

Status: COMPLETE (2026-10-10), not committed. Branch `m2`, worktree `.claude/worktrees/m2`. Scope:
docs/m2-r1-summary.md "M2-R2 plan", producer 4. Specs: docs/formats/textures.md, docs/formats/fib.md,
docs/m2-r1-data-deltas.md (stops ranked 4, 6, 8, 9, 10).

Subsystems, in done-marker order: `names`, `speech`, `chatter`, `feart`, `textures`, `runs`.

**Result.**
- On the plain harness (only what a GOG install has, under its own names, no `/noSpeech`), the debug engine now opens
  `Update.big` and `Cataclysm.big`, starts speech with `SectSpeech.vce` and the Somtaaw LUTs, parses
  `BattleChatter.script`, loads the Cataclysm construction/launch/research art, loads the 21 engine-glow meshes and
  pushes `Main_game_screen`, then idles to the timeout with 0 ASan errors (run `startup-final3-fe`). Before this item it
  stopped at `Unable to open file: HW_Music.wxd`, before any archive was opened (run `startup-r0-gate-fe`).
- `textures.ll` loads whole (12,403 elements, `LL_MaxElements` raised), every element is found again by the engine's
  binary search, and every listed texture that has a file in the install (10,661) registers and loads through the
  0x34-style registry entry and Cataclysm's texture-memory fitting (port diagnostic `/catTexturesTest`, run
  `startup-final2-textures`).
- The plain harness also runs the other producers' self-tests now: meshes 1,758/1,758 (with `mesh_engine_check.py`
  1,758 match), ship statics 143/145 types.
- Found on the way, outside the brief: GoK's per-archive "local file is newer" array was allocated at a quarter of
  its size, so reads and writes past it went into the game heap (ASan cannot see that heap); with Cataclysm.big's
  16,528 entries it made `etg\meshes\plasmashock.geo` unloadable. Fixed (subsystem `runs`).
- Single player (`--mode sp`) passes the `RACE_*` level-load stop and stops in `GenericInterceptorStaticInit`
  (`GenericInterceptor.c (205)`), the stage-2 ship-code stop the identifiers item already recorded.

**DONE WHEN.**

| criterion | state | evidence |
|---|---|---|
| data-deltas stop rank 4 (speech LUT layout, SIGFPE) cleared without `/noSpeech` | **met** | `speech`; runs `startup-r3-speech-fe` onward load `Sectsentence.lut`, `Sectphrase.lut`, `SectSpeech.vce`, `Music4Cat.wxh`, `Music4Cat.wxd` with no checksum message and no FPE; no run uses `/noSpeech` |
| rank 6 (battle chatter event names) cleared | **met** | `chatter`; run `startup-r5-chatter-fe` passes `BattleChatter.script` (stopped there in `startup-r4-feart-fe`) |
| rank 8 (renamed front-end art) cleared | **met** | `feart`; run `startup-r4-feart-fe` loads 27 `FEMan/Construction_Manager` and 21 `ResearchGUI` files (stopped at `Build_race1_icon1.lif` in `startup-r3-speech-fe`) |
| rank 9 (file names: archives, music, voice, speech LUT names) cleared | **met** | `names` + `speech`; run `startup-r1-names-fe` opens both archives (`39 files found in Update.big`, `16528 files found in Cataclysm.big`) |
| rank 10 (FE callback `Main_game_screen-TO-Select_tutorial`) cleared | **met** | `feart`: the atom is a call-on-create link with no callback in the exe either; Cataclysm's `feAllCallOnCreate` skips it silently and so does the port now (run `startup-r6-fecb-fe`: no "not found" line) |
| `textures.ll` fully registered | **met** | `textures`; run `startup-final2-textures`: 12,403 listed, 12,403 found, 10,661 registered and loaded, 0 not loaded, 0 ASan; the other 1,742 rows name files the install does not ship |
| the harness reaches `Main_game_screen` on the unmodified data | **met** | run `startup-final3-fe` (final clean debug build): `feScreenPush: pushed screen 1`, idles to the 40 s timeout; the reported "first stop" is GoK's `munmap(heap, 0)` assert during the shutdown after SIGTERM (data-deltas 5.4), not a data stop |

Exe evidence comes from `ghidra/exports/cataclysm_gog_r5/decompiled.c` (read with a scratch extractor,
`build/startup-tools/dec.sh`), `strings.csv`, and `.data` read from `original/game/cataclysm.exe` with a scratch PE
reader (`build/startup-tools/pe.py`, read-only; `build/` is gitignored). Addresses are the exe's.

Baseline (HEAD fe8645d, debug): `tools/m2_harness.sh startup-r0-gate-fe --mode fe` on the plain harness stops at
`Unable to open file: HW_Music.wxd` (milestone `process started`).

## Subsystem `names` (data-deltas rank 9)

| file | change | exe evidence |
|---|---|---|
| `src/Game/BigFile.c` | `bigFilePrecedence[]`: the required archive is `Cataclysm.big` (was `Homeworld.big`); `Update.big` keeps its name and stays optional | `utyGameSystemsPreInit` 0x005ab810 calls `bigOpen(utyBigFilename, utyUpdateBigFilename)` (bigOpen 0x00425260); strings `.data` 0x008c0ef8 `Cataclysm.big`, 0x008c0f38 `Update.big` (the CET `UpdateBigFileName1-3` patch sites push 0x008c0f38) |
| `src/Game/BigFile.c` | `bigOpenAllBigFiles`: after `fileExists()` matched the archive name case-insensitively, `fopen` the on-disk spelling (`filePathTempBuffer`) instead of the configured one, when the two differ only in case | the exe runs on a case-insensitive file system; the GOG install ships `cataclysm.big`/`update.big`. Without this the 1.01 `update.big` was skipped with only a warning (data-deltas 1.2, run0c) |
| `src/SDL/utility.c`, `utility.h` | `utyMusicFilename` = `Music4Cat.wxd`; `utyVoiceFilename` = `SectSpeech.vce`; new `cat_utyBeastVoiceFilename` = `BeastSpeech.vce`; `utyGameSystemsPreInit` checks the Beast voice file like the other two (fatal "cannot open" if missing) | `utyGameSystemsPreInit` 0x005ab810: `fileExists` on `utyMusicFilename`, `utyVoiceFilename`, `cat_utyBeastVoiceFilename` in that order, each with the CD-ROM fallback; `.data` 0x008c0f08, 0x008c0f18, 0x008c0f28 |
| `src/Game/SpeechEvent.c` | `musicHeaderName` = `Music4Cat.wxh` | `speechEventInit` 0x00521a40 loads `SoundFX\` + `.data` 0x00895f60 `Music4Cat.wxh` |
| `src/SDL/utility.c` | log label `Cataclysm.big TOC` (was `Homeworld.big TOC`) | log text only |

Result (run `startup-r1-names-fe`): `39 files found in Update.big`, `16528 files found in Cataclysm.big`; the
music/voice checks pass; next stop `fileSizeGet: can't find file '.../SoundFX/speechsentence_comp.lut'` (data-deltas
run9, rank 9's LUT names, handled by `speech`).

## Subsystem `speech` (data-deltas rank 4, and rank 9's LUT names)

Cataclysm moved the speech tables per race. `soundEventInit(race)` 0x0051c6e0 calls `cat_speechEventInitForRace(race)`
0x00521d50, which calls `cat_speechEventLoadRaceVoice(race)` 0x005217f0 and, the first time only, `speechEventInit`
0x00521a40. `gameStart` 0x005a9290 reloads the set for the current player's race.

| file | change | exe evidence |
|---|---|---|
| `src/Game/SoundStructs.h` | `SENTENCELUT.compbitrate[1]` (HW1: 4), so `lookup[]` is at +0x10; `_Static_assert`s on +0x0E and +0x10 | every reader: `speechEventInit` takes the bitrate at `SentenceLUT+0xe`; `SEselectsentence` 0x005223c0 reads `SentenceLUT+0x12+((numevents*actor+event)*numcolumns)*2`; `speechEventQueue` 0x00522560 reads the priority at `SentenceLUT+0x10+numcolumns*event*2`. Data: `sectsentence.lut` numactors 1, first table words `2, 4, 61` (data-deltas 2.1) |
| `src/Game/SpeechEvent.c` | new `cat_speechEventLoadRaceVoice(race)`: frees the previous tables and closes the voice file, loads `SoundFX\Sectsentence.lut` + `Sectphrase.lut` for race 0, `Beast*` for any other race, checks the two checksums, opens `SectSpeech.vce` or `BeastSpeech.vce` and checks its checksum | 0x005217f0: `"Sect"` 0x00896144 / `"Beast"` 0x0089613c (`p1 == 0` picks Sect), `"sentence.lut"` 0x0089612c, `"phrase.lut"` 0x008960fc, `soundstreamopenfile(p1 ? cat_utyBeastVoiceFilename : utyVoiceFilename)` |
| `src/Game/SpeechEvent.c` | new `cat_speechEventInitForRace(race)` with the exe's first-time flag | 0x00521d50 (flag `.data` 0x00a6ae98); on failure frees both tables and closes the file |
| `src/Game/SpeechEvent.c` | `speechEventInit`: the LUT load, the checksum check and the voice-file open are now demo-build only (moved to the function above); all speech stream buffers are created with `compbitrate[0]`; on `soundstreamquery` failure the tables are left to the caller | 0x00521a40 starts at `soundstreamquery(4, ...)` and creates its speech buffers with `*(uword *)(SentenceLUT + 0xe)` |
| `src/Game/SpeechEvent.c` | `SEloadscripts`: only `eq_fleet.script` and `eq_intel.script`, then `eq[8] = eq[7]` for both; no pilot EQ, delay or effect scripts (Cataclysm ships none of them) | `speechEventInit` 0x00521a40 (SEloadscripts inlined): two `scriptSetStruct` calls with the table at 0x00895f70 (`flags`, `eq0`..`eq7`), then `_DAT_00c097e4 = DAT_00c097e0` and `_DAT_00c0980c = DAT_00c09808` |
| `src/Game/SpeechEvent.c` | `speechPlayQueue`-side bitrate: always `compbitrate[0]` | `speechPlayQueue` 0x005220e0: `bitrate = *(sword *)(SentenceLUT + 0xe)` |
| `src/Game/SoundEvent.c`, `SoundEvent.h` | `soundEventInit(sdword race)`; speech start-up through `cat_speechEventInitForRace(race)`; prototypes of the two new functions in `SoundEvent.h` | `soundEventInit` 0x0051c6e0 (`sdword` parameter), `utyGameSystemsInit` 0x005abe70 calls `soundEventInit(whichRaceSelected)` |
| `src/SDL/utility.c` | `utyGameSystemsInit` passes `whichRaceSelected`; `gameStart` calls `cat_speechEventLoadRaceVoice(universe.players[curPlayerIndex].race)` when speech is on, after the load/reset branch | 0x005abe70; `gameStart` 0x005a9290 (two sites, after `universeSwitchToPlayer`, guarded by the speech-enabled flag 0x008b8640) [audit: one call site in the exe; the second copy is Ghidra's `utyFatalErrorWaitLoop` 0x005a91d0 body, which runs on into `gameStart`'s code (192-byte function, decompiled with `gameStart`'s locals)] |
| `src/SDL/utility_mac.c` | same one-line call change (file not built; kept consistent) | |

Not ported here (stage 3, speech runtime): Cataclysm's event-id format (event mask 0x7ff, actor flags from 0x800, 863
events; `speechEventQueue` rejects `event & 0x7ff > 0x35e`), its 3-speech + 1-music stream layout (HW1's 5 + 2 kept),
the EQ slot indices (Cataclysm: fleet 0, intel 1; HW1's indices kept), the per-event table set up in `speechEventInit`'s
62-entry loop (0x00c09f00), and the music buffer bitrate 0x200.

Result (run `startup-r3-speech-fe`, no `/noSpeech`): `FileLoads.log` has `SoundFX/Sectsentence.lut`,
`SoundFX/Sectphrase.lut`, `SectSpeech.vce`, `SoundFX/Music4Cat.wxh`, `Music4Cat.wxd`, `eq_fleet.script`,
`eq_intel.script`; no checksum message, no SIGFPE, 0 ASan errors; the engine reaches `Starting front end module` and
stops at `fileSizeGet: can't find file '.../FEMan/Construction_Manager/Build_race1_icon1.lif'` (rank 8, `feart`).
(Run `startup-r2-speech-fe`, before the `SEloadscripts` change, stopped at `fileOpen: cannot open file
SoundFX/Scripts/eq_pilot1.script`.)

## Subsystem `chatter` (data-deltas rank 6)

`src/Game/Battle.c`, `src/Game/Battle.h`. Cataclysm's battle chatter table has the same 88 slots in the same
`BCE_*` order as HW1 (table `.data` 0x0074e8d0, stride 0x58 = 88 entries up to 0x00750710, read by
`batRandomWeightSetCB` 0x004232c0 and the other `bat*SetCB` callbacks), dumped with `pe.py table 0x74e8d0 0x58 88`.

| change | exe evidence |
|---|---|
| all 88 event names replaced by Cataclysm's (`CHAT_Intel_Grp_PositiveBattle`, `CHAT_Ship_Grp_PositiveBattleMore`, ...; each changed line carries `// HW1: <old name>`); 7 slots carry new events (14 `STAT_Spec_ShipBeingBeastified`, 45 `STAT_Spec_HiveSwarmersUnderway`, 66 `STAT_Spec_RammerRamComplete`, 73 `STAT_MoShip_EMPPulseDetected`, 75 `STAT_Spec_WorkerFull`, 83 `STAT_Spec_ResourcesTransferred`, 84 `COMM_Spec_RammerStartingRam`) | entry +0x00 name pointers |
| the event numbers are the exe's speech event ids (raw hex, e.g. `CHAT_Fighter_Wingman` 0x10008c4; 4 slots are 0 in the exe: 76, 77, 81, 87; the two docking slots are `(sdword)0x81000804`) | entry +0x04. Cataclysm's ids keep the speech index below 0x400 (863 events), so HW1's decode (`SPEECH_EVENT_MASK` 0x3ff) still finds the right LUT row |
| `battleevent` gains `statusUpdate`, `statusSuppress` (bool32) | entry +0x50/+0x54, written by `cat_batStatusUpdateSetCB` 0x004235f0 / `cat_batStatusSuppressSetCB` 0x00423680 (`movl $1/$0, 0x50(%ecx)` after `fcompl` with 0.0 at 0x0072a4e8) |
| `battleTweaks[]` gains `battleStatusUpdate` and `battleStatusSuppress` (value > 0.0 sets the flag) | script table `.data` 0x00750758 (21 entries: the 7 HW1 per-event keys, the 2 new ones, `batGlobalFrequencyModifier`, then 11 globals) |

Not ported (stage 3, battle chatter runtime): Cataclysm stores `randomWeight` as the raw script `double` at +0x08 and
`battleChatterAttempt` 0x00423790 (rewritten) multiplies it at use; HW1's scaled `udword` and its consumers are kept.
The exe's per-entry defaults differ from HW1's `BAD_*` (randomWeight 1.0, maxDistance 1e6, expDistance 1,
minWavelength 10.1, expWavelength 1, minRepeatProximity 0, expRepeatProximity 1). Cataclysm's script table has no
`batGlobalFrequencyMin`/`Max` (HW1's rows kept; Cataclysm's script sets neither). The `BCE_*` enum names keep HW1's
wording for the 7 repurposed slots.

Result (run `startup-r5-chatter-fe`): `BattleChatter.script` parses; the engine loads the 21 engine-glow meshes and
reaches `feScreenPush: pushed screen 1` (`Main_game_screen`) on the plain harness.

## Subsystem `feart` (data-deltas ranks 8 and 10)

| file | change | exe evidence |
|---|---|---|
| `src/Game/ConsMgr.c` | `NUM_CMTEXTURES` 2 -> 6; `ShipIcons[]` = `icon1_disabled.lif`, `icon1_off.lif`, `icon1_on.lif`, `icon2_disabled.lif`, `icon2_off.lif`, `icon2_on.lif` (prefix `FEMan\Construction_Manager\Build_race{1,2}_` unchanged) | `cmLoadTextures` 0x00455040: prefix table 0x00754a18, icon table 0x00754a20..0x00754a38, images 0x00a2dc58, textures 0x007549e8 (2 x 6) |
| `src/Game/ConsMgr.c` | new: 5 support-unit textures `Feman\texDecorative\b_Build_Support{Mask_Top,Mask_Mid,Mask_Low,Icon_Top,Icon_Low}.lif`, loaded as RGB in `cmLoadTextures`, freed in `cmCloseTextures` | `cmLoadTextures` last loop (path 0x00754eac, names 0x00754a68, images 0x00a2dca0, textures 0x00754a7c); `cmCloseTextures` 0x004551e0 third loop |
| `src/Game/LaunchMgr.c` | `NUM_LMTEXTURES` 2 -> 6 with the same six names | `lmLoadTextures` 0x0049dd60: tables 0x00883500/0x00883508, images 0x00a4471c, textures 0x008834d0 |
| `src/Game/LaunchMgr.c` | new: 15 launch-system button textures `FEMan\Construction_Manager\R1_Sys{1,2,3}_{disabled,off,off_mouse,off_focus,on}.lif`, loaded as RGB, freed in `lmFreeTextures` | `lmLoadTextures` second loop (path 0x00883520, names 0x0088353c, images 0x00a4474c, textures 0x00883578); `lmFreeTextures` 0x0049df00 |
| `src/Game/ConsMgr.c` | [audit: added] `cmDrawShipImage` picks the `_off` image (1 or 4); the producer left HW1's 0/1, which with the 6-image table drew `icon1_disabled` for the Mothership and `icon1_off` (Mothership art) for the carrier | Cataclysm's `cmDrawShipImage` 0x004589c0: base `-(shipID != 0) & 3`, +1 off, +2 on (focused), same scheme as `lmDrawShipImage` |
| `src/Game/LaunchMgr.c` | `lmDrawShipImage` picks the `_off` image (1 or 4) | Cataclysm's `lmDrawShipImage` 0x0049d980 takes a 3rd argument and picks base 0/3 + disabled/off/on (stage 3); `_off` is its enabled, unfocused case |
| `src/Game/ResearchGUI.c` | `rmGUIStartup`: HW1's per-race `Research_icon.lif` replaced by Cataclysm's art: 15 Somtaaw lab buttons `ResearchGUI\Race1\Research_icon_{Hangar,Armour,Engineering,Special,Weapons}_{disabled,off,on}.lif` and 6 Beast frames `ResearchGUI\Race2\bresearch_ani_0{1..6}.lif`; third font `EuroseWideHeavy_i8.hff`; `rmGUIShutdown` frees both tables | `rmGUIStartup` 0x004f76f0 (paths 0x00891e70/74, `Research_icon_` 0x00891e7c, names 0x00891e80, `bresearch_ani_` 0x00891ec4, frames 0x00891ec8; images 0x00c17ac0/0x00c168a0, textures 0x00c16800/0x00c17b00; font string 0x008912f0); `rmGUIShutdown` 0x004f7970 |
| `src/Game/ResearchGUI.c` | HW1's lab draw code still reads `rmLabImage/rmLabTexture[race]`: they now borrow the Somtaaw `Engineering_on` button and Beast frame 01 (not owned, not freed) | the exe's research draw functions are rewritten (stage 3) |
| `src/Game/FEFlow.c` | `feAllCallOnCreate` looks the callback up itself and skips an atom without one, silently | `feAllCallOnCreate` 0x00474050 inlines the `feCallback[]` search and has no "not found" path |

Rank 10 finding: `Main_game_screen-TO-Select_tutorial` is a **link** atom (front_end.fib screen `Main_game_screen`,
atom 16, type 3, flags 0x861 = `FAF_Link|FAF_CallOnCreate|FAF_ContentsVisible|FAF_BorderVisible`) that the data marks
call-on-create. Cataclysm registers no callback of that name (`grep -a` finds no `Main_game_screen-TO-` string in
cataclysm.exe), so the atom is inert in the exe as well; the HW1 engine only printed a debug message for it. This
answers data-deltas open question 5.

Callback census (scratch `build/startup-tools/fibcallbacks.py` over the 22 `.fib` files: every atom flagged
`FAF_Function` or `FAF_CallOnCreate`, 440 names): 325 are string literals in `src/` and strings in the exe; 68 are
strings in the exe only (callbacks Cataclysm can register that the port lacks; all fire on user input, none is
call-on-create); 43 are in `src/` only; 4 in neither. Every call-on-create name either has a callback in `src/` or
has none in the exe either (`Main_game_screen-TO-Select_tutorial`, `FE_LessonGameWindowInit`). "String in the exe" is
an approximation of "registered callback". The 68 belong to the UI stage.

Results: run `startup-r4-feart-fe` loaded 27 `FEMan/Construction_Manager` and 21 `ResearchGUI` files and the new font,
then stopped at the battle chatter event (rank 6, fixed in `chatter`); run `startup-r6-fecb-fe`: `Main_game_screen`,
no "not found" line in the log.

## Subsystem `textures` (docs/formats/textures.md)

| file | change | exe evidence |
|---|---|---|
| `src/SDL/texreg.h` | `LL_MaxElements` 12000 -> 16384 | Cataclysm's `textures.ll` has 12,403 elements; `trListFileLoad` 0x005878f0 has no limit (HW1 checked it in debug builds only, so the debug build would `dbgFatalf("Unexpected number of elements")`) |
| `src/SDL/texreg.h` | `texreg` gains `uword scaleShiftX, scaleShiftY; udword bScalable;` after `sharedFrom`; `_Static_assert`s for the exe's ILP32 layout (`sharedFrom` 0x28, shifts 0x2C/0x2E, `bScalable` 0x30, size 0x34) when built `-m32` without debug textures | `trStartup` 0x00584d30 allocates `trRegistrySize * 0x34`; `trTextureRegister` 0x00585760 stores `param_4` at +0x30; `trCramRAMComputeAndScale` 0x00589a50 uses +0x2C/+0x2E |
| `src/SDL/texreg.c`, `texreg.h` | `trTextureRegister(fileName, info, meshReference, bScalable)`: stores `bScalable`, zeroes the shifts | 0x00585760 (callers clean 16 bytes) [audit: the exe stores `param_4` at +0x30 but never writes +0x2C/+0x2E there; the port's zeroing is its own (harmless: `trCramRAMComputeAndScale` zeroes the shifts of every listed entry first, as the exe does)] |
| 10 callers [audit: 9 call sites in 7 functions; the 10th is the port's own self-test] | 4th argument: `FALSE` for the two subtitle pictures (`Subtitle.c`: `subThemePictureSet`, `subTexturesReset`), `TRUE` for `trailStartup` (`Trails.c`), `etgSubTextureDWORD` and `etgSetTextureParse` (`ETG.c`), `meshTextureRegisterAllPlayers` (2 calls) and `meshRecolorize` (`Mesh.c`), `scriptSetNAVLightCB` (`StatScript.c`) | exe call sites: `subThemePictureSet` 0x005833e0 and `subTexturesReset` 0x00583990 push 0; `trailStartup` 0x005a49d0, `etgSubTextureDWORD` 0x0046caf0, `etgSetTextureParse` 0x0046cc60, `meshTextureRegisterAllPlayers` 0x004ae910, `meshRecolorize` 0x004af3a0, `scriptSetNAVLightCB` 0x00526ce0 push 1 (the exe's Cataclysm-only fog/lens-flare callers `cat_rndFogLayerTextureLoad` 0x0057b7d0, `cat_lfTextureFileSetCB` 0x0057b820, `cat_scriptSetFogCB` 0x0057cb70 do not exist in `src/` yet) |
| `src/SDL/texreg.c`, `texreg.h` | new `trCramRAMComputeAndScale()` and `catTrCramLimit` (10 MB); `trRegistryRefresh` calls it instead of HW1's `qsort` + `trCramRAMScaleCompute` + `trCramIntoRAM` | 0x00589a50, called from `trRegistryRefresh` 0x00587a50 right after the listing is freed, with no sort. Sum of `w*h` (paletted) or `nPalettes*w*h*2` (RGB); limit `.data` 0x008bb698 = 0x00a00000; mission 17: `max(0x100000, limit * 0.3f)` (0.3f at 0x0072b064); passes that halve the longer side of each `bScalable` texture still above 8 texels until the total fits or a pass changes nothing; then `trPreLoadedTextureScale` for loaded textures, scaled size for pending ones |
| `src/SDL/texreg.c` | `trImageScaleIndexed`: the power-of-two input assert is gone | Cataclysm's listing has one non-power-of-two texture, `race_beast\bcruisemissile\rl0\LOD4\4bcr5` 32x90 (scratch `build/startup-tools/llnpot.py`); HW1's cram and this function asserted on it (runs r9, r12). Same algorithm in the exe (0x00586af0), no check |
| `src/SDL/texreg.c` | `trListFileLoadAndConvertTo64Bit`: the sharing block is copied with `memcpy` | the string block is 547,729 bytes (odd), so the sharing block is unaligned; HW1's element-wise copy is a misaligned load (UBSan, run r9). GoK 64-bit conversion only |
| `src/SDL/texreg.c`, `texreg.h`, `src/SDL/main.c`, `src/SDL/utility.c` | port diagnostic `/catTexturesTest` (`catTexturesRunSelfTest`, not in the exe): loads `textures.ll`, looks every element up again with `trImageMeasureFromListing`, then registers every element whose file exists (its own `.LiF`, or its share parent's, which is what `trRegistryRefresh` loads for a sharer) in batches that fit the 6,000-entry registry, loads each batch with `trRegistryRefresh` (inside `horseRaceInit`/`horseRaceShutdown`, as a level load does), checks each entry is loaded, then unregisters it | |
| `tools/m2_harness.sh` | `--mode textures` (`/catTexturesTest`), milestone `texture self-test done`, a `textures:` summary line | |
| `tools/m2_layout32.sh` | also includes `texreg.h` and `SoundStructs.h`; prints the ILP32 `texreg` (52 = 0x34) and `SENTENCELUT` (16) sizes | |

Not ported here: `cat_trTextureMemorySet` 0x00589cd0 (the texture-detail option mapping 0-100 % to 1..12 MB and the
no-palette pool to 10..90 MB, and freeing over-limit no-palette textures) belongs with the options screen (stage 3);
until then the limit stays at the exe's initial 10 MB. `cat_trPrecacheShipTextures` 0x00589910 was not compared.
`singlePlayerGameInfo.currentMission == 17` uses HW1's `MissionEnum` (17 is HW1's `MISSION_5B`); the mission enum is
stage 6. CET's `LoadMission_AdjustTextureMemoryLimit` hook skips the mission-17 cut; the port follows the stock exe.

Result (run `startup-r13-textures`, plain harness, 21.6 s): `textures.ll has 12403 elements (LL_MaxElements 16384)`;
`listing lookups 12403/12403 found, 0 size/flag mismatches, 0 bad sharedFrom`; `12403 listed, 12403 found in listing,
1742 without a file to load, 10661 registered in 3 batches, 10661 loaded, 0 not loaded, 6289 scaled down, 0 without
bScalable`; 8,776 file loads; 0 ASan errors; no UBSan report from texreg code. The 1,742 elements without a file are
listing rows for textures the install does not ship; a scratch census over the extracted archives (both `cataclysm/` and
`update/`) gives the same 1,742: `race_sect` 710, `race_beast` 609, `etg` 264 (e.g. `ETG\Textures\akira\akira01`),
`race_traders` 85, `race_derelict` 58, `speechicons` 9, `overlayicons` 6, plus element 74
`e_sect\smothershiplowerspine\rl0\LOD1\OCLtop`, a truncated name. 1,848 rows have no own `.lif`; 124 of them load through
their share parent. Whether any game data asks for one of the 1,742 was not checked (UNVERIFIED; a plausible reason
for the ship rows is that the `.peo` meshes use paged textures instead); the exe would fail the same way if it did. `sizeof(texreg)` is 96 in the 64-bit debug
build (pointers, debug-texture fields); the exe layout is checked by `tools/m2_layout32.sh`.
Earlier runs: r8 (no horse race set up: NULL region in `regRegionProcess`), r9 (HW1 cram power-of-two assert), r10
(element 74's missing file), r11/r12 (`trImageScaleIndexed` power-of-two assert).

## Subsystem `runs` (gate runs, and the archive fix they forced)

**Archive "newer local file" array (`src/Game/BigFile.c`).** Once the plain harness reached the front end, the mesh
self-test on it (run `startup-final-meshes`) stopped at `fileSizeGet: can't find file '.../etg\meshes\plasmashock.geo'`
after 105 meshes, although the entry is in the `cataclysm.big` TOC (index 13088, verified with the scratch
`build/startup-tools/biglookup.py`, which repeats GoK's CRC-half lookup). Temporary log lines (removed again) showed
`bigTOCFileExists` finding index 13088 and `bigFindFile` then rejecting it because `localFileRelativeAge[13088]` read
`LOCAL_FILE_IS_NEWER`, although `bigFilesystemCompare` had marked only indices 14975, 6369, 6343 and 549 (the GOG
install's loose `feman\texdecorative\a_main_title_*.lif` and `credits\english\01_title.jpg`) and the value was still 0
right after the compare. Cause: the array holds `bigLocalFileAgeComparison` (a 4-byte enum) but GoK allocates and
clears `numFiles` bytes, so every index above `numFiles / 4` lies outside it, in the engine's own heap, where ASan cannot
see it (the compare's writes for those indices land there too). Fix: allocate and clear
`numFiles * sizeof(bigLocalFileAgeComparison)`. The exe keeps one byte per entry (`utyGameSystemsPreInit` 0x005ab810:
`mainNewerAvailable = memAlloc(mainTOC)`, cleared bytewise), so this is a GoK port defect, not a Cataclysm delta; it
would also have hit HW1's 13,887-entry archive. It may explain data-deltas open question 1 (run17/18: loose newer files
in subdirectories not overriding the archive) and possibly other unexplained heap damage on archive-based runs
(UNVERIFIED). After the fix: run `startup-final-meshes2`, 1,758/1,758 meshes from the archive, 0 bad, 0 ASan;
`tools/formats/mesh_engine_check.py`: `1758 files on disk, 1758 match, 0 mismatch, 0 not loaded, 0 extra`.

**Builds** (final tree, `--clean`): `tools/build.sh --debug --clean` rc 0, `[292/292] Linking target hwi-engine`,
0 warnings, 75,595,696 B; `tools/build.sh --release --clean` rc 0, `[292/292]`, 0 warnings, 5,205,784 B (sha256
`d37afcacc0559ff5...`). `tools/m2_layout32.sh`: `ILP32 sizes (ShipStaticInfo GunStatic CustShipHeader polygonobject
texreg SENTENCELUT): ... [1112] [284] [140] [116] [52] [16]`, `m2_layout32: OK`. Both builds were also green after every
subsystem.

**Harness runs** (`tools/m2_harness.sh`, plain harness `original/scratch-m2/m2r2-harness/data` unless noted; logs in
`original/scratch-m2/m2r2-harness/runs/<label>/`, ledger `runs.txt`; offscreen driver, no input, no `/noSpeech`):

| run | build / change | mode | milestone | first stop | note |
|---|---|---|---|---|---|
| `startup-r0-gate-fe` | HEAD fe8645d | fe | process started | `Unable to open file: HW_Music.wxd` | baseline |
| `startup-r1-names-fe` | + `names` | fe | archives opened | `fileSizeGet: ... SoundFX/speechsentence_comp.lut` | both archives open |
| `startup-r2-speech-fe` | + race LUT reader | fe | archives opened | `fileOpen: cannot open file SoundFX/Scripts/eq_pilot1.script` | LUTs and voice file load |
| `startup-r3-speech-fe` | + `SEloadscripts` | fe | front end module started | `fileSizeGet: ... FEMan/Construction_Manager/Build_race1_icon1.lif` | rank 4 and 9 cleared |
| `startup-r4-feart-fe` | + `feart` art | fe | front end module started | `Battle.c (633): Unable to find battle chatter event 'CHAT_Intel_Grp_PositiveBattle'` | rank 8 cleared |
| `startup-r5-chatter-fe` | + `chatter` | fe, 40 s | `Main_game_screen` | shutdown assert at the timeout | rank 6 cleared; 21 engine glows |
| `startup-r6-fecb-fe` | + `feAllCallOnCreate` | fe, 40 s | `Main_game_screen` | shutdown assert | rank 10: no "not found" line |
| `startup-r7-sp` | same | sp | level load (mission file read) | `GenericInterceptor.c (205)` | stage 2 (see below) |
| `startup-r8`..`r12-textures` | `textures` in progress | textures | `Main_game_screen` | see `textures` | horse race set-up, cram and scale asserts, file-less rows |
| `startup-r13-textures` | `textures` done | textures | texture self-test done | shutdown assert | 10,661 loaded |
| `startup-final-{fe,sp,statics,textures}` | before the archive fix | fe / sp / statics / textures | as below | as below | same results as final2 |
| `startup-final-meshes` | before the archive fix | meshes | `Main_game_screen` | `fileSizeGet: ... etg\meshes\plasmashock.geo` | the archive-array defect |
| `startup-dbg-plasma`..`plasma6` | temporary log lines (removed) | fe + `/catMeshLoadTest` two-line list | `Main_game_screen` | same | diagnosis above |
| `startup-final-meshes2` | + archive fix | meshes | mesh self-test done | shutdown assert | 1,758/1,758, 0 bad, 0 ASan |
| `startup-final2-fe` | final | fe, 40 s | `Main_game_screen` | shutdown assert | 232 file loads, 0 ASan |
| `startup-final2-sp` | final | sp, 60 s | level load (mission file read) | `GenericInterceptor.c (205): Assertion of (interceptorstat != NULL)` | past `RACE_*`; stage 2 |
| `startup-final2-statics` | final | statics | ship statics self-test done | shutdown assert | 143 of 145 types (the two `trResearchStation*` have no `.shp`, as before) |
| `startup-final2-textures` | final | textures | texture self-test done | shutdown assert | 12,403 / 10,661 / 0 failed |
| `startup-final3-fe` | final clean debug build | fe, 40 s | `Main_game_screen` | shutdown assert | the gate run |

Sanitizers in the final runs: 0 ASan errors. UBSan reports only the known misaligned `struct fescreen`/`struct PATCH`
accesses (data-deltas 5.4) plus two new kinds in `SDL/fquant.c` (misaligned `int` loads in `fqUnpack` :33 and
`fqSUnpack` :24, and `left shift of 3 by 31 places` in `fqSUnpack` :28, called from `fqDequantBlock` <-
`isoundmixerdecodeEffect`). They appear because speech and music streams now run
(before, the engine never got past speech init); they are GoK's audio bit-unpacking, not Cataclysm data, and are left
for the sound port (stage 3). The `SpeechEvent.c:2666` NULL `MUSICHEADER` report of the `/noSpeech` runs is gone.

## What is left for later stages

| item | owner | evidence |
|---|---|---|
| `GenericInterceptorStaticInit` assert in single player (first stop after `RACE_*`) | stage 2, ship code (the exe's version 0x005b1e70 switches over 14 Cataclysm types) | runs `startup-final2-sp`; docs/m2-r2-identifiers.md |
| speech runtime: Cataclysm's event-id table (863 events, mask 0x7ff), 3 speech + 1 music streams, EQ slot indices, the 62-entry per-event table in `speechEventInit`, music buffer bitrate 0x200 | stage 3, sound | `speech` |
| battle chatter runtime: `randomWeight` as raw double, the exe's defaults, `battleChatterAttempt` 0x00423790 (rewritten), readers of `statusUpdate`/`statusSuppress` | stage 3 | `chatter` |
| construction/launch/research draw code for the new art (`lmDrawShipImage` 3-argument version 0x0049d980, research lab buttons, Beast animation, support masks) and the 68 front-end callbacks named only in the exe | stage 3, UI | `feart` |
| `cat_trTextureMemorySet` 0x00589cd0 (texture-detail option, 1..12 MB) and `cat_trPrecacheShipTextures` 0x00589910; the fog/lens-flare texture callers (`cat_rndFogLayerTextureLoad`, `cat_lfTextureFileSetCB`, `cat_scriptSetFogCB`) | stage 3, render/options | `textures` |
| mission-17 texture limit keyed on HW1's `MissionEnum` value 17 | stage 6, missions | `textures` |
| `fquant.c` UBSan reports (GoK audio decoder) | stage 3, sound | `runs` |

## Files changed

`src/Game/BigFile.c` (archive name, case-correct open, age-array size), `src/SDL/utility.c`/`utility.h` (stream names,
Beast voice check, `soundEventInit(race)`, `gameStart` voice reload, `/catTexturesTest` hook, log label),
`src/SDL/utility_mac.c` (call signature only; not built), `src/Game/SoundStructs.h` (SENTENCELUT), `src/Game/SpeechEvent.c`
(race LUT reader, init, EQ scripts, bitrate, music header), `src/Game/SoundEvent.c`/`SoundEvent.h` (`soundEventInit(race)`,
prototypes), `src/Game/Battle.c`/`Battle.h` (event table, status flags and keys), `src/Game/ConsMgr.c`,
`src/Game/LaunchMgr.c`, `src/Game/ResearchGUI.c` (front-end art), `src/Game/FEFlow.c` (`feAllCallOnCreate`),
`src/SDL/texreg.h`/`texreg.c` (entry, 4th argument, listing limit, texture-memory fitting, scale assert, sharing-block
copy, self-test), `src/Game/Subtitle.c`, `src/Game/Trails.c`, `src/Game/ETG.c`, `src/Game/Mesh.c`,
`src/Game/StatScript.c` (4th argument), `src/SDL/main.c` (`/catTexturesTest` switch), `tools/m2_harness.sh`
(`--mode textures`), `tools/m2_layout32.sh` (texreg, SENTENCELUT), `docs/m2-r2-startup.md` (this file). 24 tracked files,
969 insertions, 162 deletions (`git diff --stat` before this section was written); 90 added lines carry `CAT:`.
Scratch helpers, all gitignored under `build/startup-tools/` (`dec.sh`, `fnat.py`, `pe.py`, `fibatoms.py`,
`fibcallbacks.py`, `llnpot.py`, `biglookup.py`, `bigentry.py`) and one list file in
`original/scratch-m2/m2r2-startup-tools/`. Nothing was committed; no game data, build output or log is in git.
