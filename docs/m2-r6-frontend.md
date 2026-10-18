<!-- done: fescreen, strings, options, subtitles, misc -->
# M2-R6 frontend: front-end flow, screens, controls, fonts, strings, options, subtitles

Status: DONE (producer 2 of M2-R6; base commit `60bfd21`; nothing committed, the auditor checkpoints).

Subsystems (in this order): `fescreen` (FEFlow.c, FEReg.c, UIControls.c, FontReg.c; the `fescreen` UBSan kind;
`/catFeScreensTest`, harness mode `fescreens`), `strings` (StringSupport.c, font.c; `/catStringsTest`, harness mode
`strings`), `options` (Options.c, the `cat_op*` set, texture detail, the voice preview, the options-file round trip),
`subtitles` (Subtitle.c, `cat_fontPrintSubtitle`, the Bink term of `speechQueueUpdate`), `misc` (ScenPick.c,
ColPick.c, Demo.c, Titan.c, bink.c, `rndRenderTask`'s front-end part, `universeReset`'s leech words).

## Result

| Gate (DONE WHEN) | Result |
|---|---|
| `/catFeScreensTest` (mode `fescreens`) pushes and pops every screen of the install's 22 `.fib` files once, ASan 0, 0 `fescreen` UBSan reports | **met**, run `r6fe-fin-fescreens`: `fescreentest: done files=22 loaded=22 screens=195 pushed=195 popped=195 wrong=0 skipped=0 listchanged=0 userregionstandins=86 result=PASS`, ASan 0, `fescreen UBSan reports: 0`, no UBSan report of any kind, rc 0 |
| `/catStringsTest` (mode `strings`) resolves all 1,705 string ids | **met**, run `r6fe-fin-strings`: `stringstest: done ids=1705 mapback=1705 resolved=1681 nostring=24 wrongtext=0 wrongnostring=0 result=PASS`; ship names 145 / 145; ASan 0, rc 0 (the 24 ids the install's English.script has no row for stay `*NO_STRING*`, as in the exe: below) |
| the Cataclysm option set at the exe's defaults round-trips through the options file | **met**, same run `r6fe-fin-fescreens`: `optionstest: done options=34 startdefaults=34 readback=34 result=PASS` |
| fe `Main_game_screen` with 0 fescreen reports | **met**, run `r6fe-fin-fe`: `Main_game_screen`, ASan 0, 0 UBSan reports of any kind (base `r6fe-base-fe`: 92 `struct fescreen` reports at 80 sites), 299 file loads (base 299) |
| sp unchanged | **met**, run `r6fe-fin-sp`: universe running, univUpdateCounter 1,248 (base 1,248), ASan 0, 3,782 file loads (base 3,782), the base's UBSan kinds minus the 110 `fescreen` reports [audit: not only: two kinds the base has not appeared, one report each, in r6fe-fin-sp and the audit's aud6fe-sp alike: BTG.c:864 (btgLoad's star-filename length read, a misaligned 'udword' load, the same LP64 loader kind as BTG.c:388; also in every mission run since r6fe-mid-mission, absent from r6fe-base-mission and R5's) and stb_image_write.h:1263 (a signed left shift in the JPEG writer of the /debugScreenshots shot); both non-fatal, ASan 0] |
| the mission stream IDENTICAL to R5's baseline `645f94c69d68319e` | **met**: `r6fe-fin-mission-a` = `r6fe-fin-mission-b` (`cmp` equal), sha256 `a36f9a53b3f8d377`; `hwo_compare.py` against `r5-integrate-mission-a`: `IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference` (only the header's build identity differs) |
| builds | `tools/build.sh --debug` and `--release`: rc 0, 0 `warning:` lines under `-Werror`; `tools/m2_layout32.sh`: OK (now also FEFlow.h) |

## Mission01 stream before / after

- Before: `r6fe-base-mission`, the base binary (60bfd21, a copy kept in the session scratchpad), 6,000 ticks: oracle
  mission run done, ASan 0, 1,476,370 B, sha256 `a36f9a53b3f8d377`; IDENTICAL to R5 (6001 ticks).
- Interim (after the options defaults, the effects preset that turns the dynamic CPV, lens flares and repair droids
  on at start-up): `r6fe-mid-mission`, IDENTICAL to R5, sha `a36f9a53b3f8d377`.
- After: `r6fe-fin-mission-a` = `r6fe-fin-mission-b`, sha `a36f9a53b3f8d377`, IDENTICAL to R5. No oracle field moved
  (the base, interim and final files are byte-identical; they differ from R5's only in the header's build identity
  02e8e4da vs 60bfd218).

## What changed, file by file

### fescreen: FEFlow.c / FEFlow.h, FEReg.c, UIControls.c, FontReg.c, BigFile.c

- **The `fescreen` UBSan kind, fixed the faithful way** (as sound fixed `struct PATCH`): GoK's 64-bit loader converts
  each `.fib` image into 64-bit structs but put the 32-byte screens at +12 (after the 12-byte header), so every
  screen access was misaligned. `FIB_ScreensOffset` (FEFlow.h) is the screens' offset in a loaded image: the header
  size on ILP32 (the exe's layout, `feScreensLoad` 0x004744e0), the next 8-byte boundary (+16) in the converted
  64-bit image; `feScreensLoad` and `feScreensDelete` use it (CAT-FIX). The converter also sizes its buffer from the
  image (every struct at its 64-bit size, every copied string with its terminator and alignment) instead of GoK's
  guess of twice the file. Static asserts pin the `.fib` header 12, the ILP32 screen 20 / link 12 / atom 0x4C
  (`tools/m2_layout32.sh` now includes FEFlow.h) and the 64-bit `_disk` structs.
- `feScreensLoad` is split into the file load and `feScreensImageLoad` (HWI: the screens self-test loads each
  archive's copy itself); `feScreensConvertTo64Bit` takes the image.
- `feCurrentScreenDelete` 0x00475380: no `tacticsMenuRegion` clear (Cataclysm has no tactics popup; Tactics.c's
  comment updated); the trailing `glcRenderEverything` is glcompat (decision 28).
- `feShouldSaveMouseCursor` 0x00472950: not in the tree (GoK removed the Win32 front-buffer cursor save, decision 28).
- FEReg.c `nextLog2` 0x00476920: the exe's ladder 2..256; CAT-FIX above 256, where the exe returns -1 and `ferDraw`
  would size a negative texture (none of the 75 textures the 22 `.fib` files name is over 256; the install's one
  wider front-end texture, `region_hw_logo.lif` 436 px, is not named by any screen).
- UIControls.c: `uicToggleProcess` 0x00534c00 plays Cataclysm's UI events (the colour picker's `CP_RaceOne` /
  `CP_RaceTwo` toggles 0x13a / 0x13b on a press, every other toggle 0x10c / 0x10d by its state; HW1 `UI_Click`),
  `uicRadioButtonProcess` 0x00534dc0 plays 0x10f (HW1 `UI_Click`). SoundEventDefs.h names them (`CAT_UI_*`; the
  install's UIEvents.lut has 62 events x 2 races). `uicRightArrowProcess` / `uicLeftArrowProcess` (0x00538fc0 /
  0x00539230) inline the radio key-down and the tab-stop helpers: same as the tree.
- BigFile.c / .h: `catBigFileLoadAllocFrom` (HWI, the screens self-test): load a file from one named archive.

### strings: StringSupport.c / .h, font.c / .h, NIS.c, Subtitle.c, UIControls.c

- `strLoadLanguage` 0x005285f0: after English.script the exe walks the 0x6A9 strings and reports each one left at
  `*NO_STRING*` (a debug message its release build compiles out); ported with a debug-build `dbgMessagef` (24 lines
  on the install).
- `strSetStringCB` 0x00528800: a string with `http:` in it is stored as written (no backslash processing: the web
  addresses `strTroubleShootPage`, `strSierraComPage`, `strContestServerPage`).
- font.c: `fontPrintN` 0x005282c0 takes a fourth argument `bAdditive` (fontPrint and every other exe caller pass 0),
  passed to its shadow pass and to `glfontDisplayString` 0x00557be0, which sets `rndAdditiveBlends(bAdditive)` (HW1:
  FALSE) and puts the previous mode back at the end. New `cat_fontPrintSubtitle` 0x00558270 (additive). The tree's
  callers (UIControls.c text entry, NIS.c text cards, Subtitle.c's commented line) pass FALSE.
- `fontLoad` 0x00557e00: compared, same as the tree (the 64-bit conversion is GoK's).

### options: Options.c / .h, texreg.c / .h, utility.c / .h, InfoOverlay.c / .h, Sensors.c / .h, PopupMenu.c / .h, ShipSelect.c / .h, SoundEvent.c, Tactics.c, LOD (words only)

- **Cataclysm's option callbacks** (Options.c 0x004d9d20 .. 0x004dbb30), registered by utility.c's `utyCallbacks`
  with the exe's names (its table 0x008c1360 .. 0x008c1438): `cat_opLODDetail` (radio, `catLodDetail`),
  `cat_opSmallFonts` (inverted `catPmLargeFont`), `cat_opHWTaskbar` / `TaskbarButtons` / `TaskbarBackdrop`
  (`catMo*`, now extern), `cat_opExperienceMessages` (the word renamed `catOpExperienceMessages`, the exe's name
  being the function's), `cat_opHomeworldCSA` / `cat_opRightClickCSA` (exclusive pair), `cat_opAutoFocusOnWaypoints`
  (`catMrWaypointCentre`), the ten sensors toggles (`catSmView*`; `cat_opResources` sets the resources and the meteors
  together), the presets `cat_opHomeworldClassic` 0x004da2c0 and `cat_opCataclysmDefault` 0x004da460,
  `cat_opTextureDetail` 0x004db6f0 (a 0..100 slider) with `cat_opTextureDetailProcess` 0x004db6d0,
  `cat_opSpeechTest` 0x004db760 (speech event 0x2001024), `cat_opSoundFXTest` 0x004db790 (UI event 0x1c8),
  `cat_opSetAllDefaults` 0x004db7b0 (884 B: the full and the low-end preset of `OP_CataclysmFramerate`) and
  `cat_opCataclysmFramerate` 0x004dbb30. Two nullsub rows (`OP_SpecialActionBeforeAttack`,
  `OP_HardwareTransformLighting`) get `utyNullCallback`. New words: `catOpTextureDetail` (0x0088be74, 50),
  `catOpGameFramerate` (0x0088be98, 1), `catOpHardwareTL` (0x00a4cc18), `catOpTextureRequirementsSet` (0x00a76260),
  and in Sensors.c `catSmViewMeteors` (0x00892b68, 1) and `catSmViewUniverseExtents` (0x00a5b1a0).
- **HW1 option bodies** at the exe's: `opOptionsInit` 0x004d71a0 (no `kbInitKeyBindings`: moved to
  `utyGameSystemsInit` as the exe), `opUpdateSettings` 0x004d71d0 (effects, `cat_lodSetTimeThreshold`, gamma,
  `cat_trTextureMemorySet`, save; `opUpdateVideoSettings` and `opSetDetailThreshold` removed), `opOptionsSaveSettings`
  0x004d8d00 / `opRestoreSavedSettings` 0x004d7f80 (Cataclysm's words saved / restored through one table, the texture
  detail and the LOD frame rate re-applied, no sound-quality restore, no `ioEnable` / `ioDisable`, no auto-LOD
  read-back, no no-palette pool), `opOptionsAcceptHelper` 0x004d76d0 (no auto-LOD read-back; the device switch stays
  GoK's SDL path), the in-game options' tutorial gating 0x004d9280 / 0x004d7e50 / 0x004d83a0 (the
  `Game_options-TO-Gameplay` link disabled while `tutorial == 1`), the advanced audio page 0x004d85e0 .. 0x004d87f0
  (voices, chatter, sound quality, speaker and equalizer; no HW1 voice actors), the custom effects' save / restore
  0x004d8c40 / 0x004d8990 (+ CPV, lens flares, repair droids, LOD detail), `opEffectsHelper` 0x004d9b90 (the presets
  also set CPV / lens flares / repair droids / the effect kinds), `opEffects` 0x004d9cc0 + `opNumEffectsProcess`
  0x004d9900 (moving the effects slider off a preset switches the radio to custom), `opDetailThreshold` 0x004da5e0 /
  `opDetailThresholdProcess` 0x004d9b20 (a frame-rate slider 30..9), `opCPUDifficulty` / `opCPUAttacks` 0x004db400 /
  0x004db440 (REWRITTEN: radio buttons), `opInfoOverlay` 0x004db480 (the word only), `opRender` 0x004dada0 (no
  no-palette display), `opDrawNumChannels` 0x004d9630 (slate background, white digits), `opTroubleShoot` 0x004db6c0
  (`strGetString(strTroubleShootPage)`). Defaults at the exe's: music 60, speech 75, detail threshold 10 (a frame
  rate now).
- **The voice preview** (sound's leftover): the exe's options have no voice buttons: `opVoice`, `opVoice0..2` and
  `opHearVoice1..3` are gone with their `utyCallbacks` rows; `soundEventHearActor` has no caller (its CAT-TODO
  closed). The `OP_KeyBindingsPoolList` row (HW1's key pool list, not in the exe) is gone too; the `NP_*` draw rows
  (no-palette pool display) are gone (the exe's draw table 0x008c1448 has none).
- **The options file**: utility.c's `utyOptionsList` gets a `[cataclysm options]` section with the exe's rows of
  Cataclysm's options (0x008c1790 .. 0x008c18c8: `LargePopupMenuFonts` .. `SetTextureRequirements`, in the exe's
  order and names). The rest of the table stays as it was (producer 3's options-file port, CAT-TODO below).
- texreg.c: `cat_trTextureMemorySet` 0x00589cd0 (the option 0..100 to the cram limit 1..12 MB and the no-palette pool
  10..90 MB, then `trNoPalReadjust` as the exe inlines it; CAT-FIX: a value out of 0..100 is clamped) and
  `cat_trPrecacheShipTextures` 0x00589910 (its caller `singlePlayerGameUpdate` is stage 6; CAT-FIX: types without
  statics and unregistered materials are skipped).
- utility.c `utyGameSystemsInit`: `kbInitKeyBindings` after `cat_gameStatsStartup` (0x005abe70) and the exe's memory
  test (SDL_GetSystemRAM: under 42,000,001 B the texture detail 0, under 74,000,001 B 25, under 128,000,000 B the
  dynamic CPV off and the low-memory switch on: a no-op on any current machine). The CAT-TODOs of InfoOverlay.c,
  Sensors.c and ShipSelect.c that named the options are closed.
- The options round trip (`catOptionsRunSelfTest`, Options.c; `catUtyOptionsFileReadBack`, utility.c) is part of
  `/catFeScreensTest`.

### subtitles: Subtitle.c, Tutor.c / .h, SpeechEvent.c

- `subTitlesDraw` 0x00584620 (the rest): over the letterbox region, while no movie plays and no NIS scissor bar is
  up, a translucent strip (colour 40 in every byte, .data 0x008bb4fc = 40.0 rounded) is added behind the text with
  additive blending, no texture, no lighting (HW1: an invisible strip); fade multipliers rounded (HW1 truncated);
  text through `cat_fontPrintSubtitle`; the picture without HW1's blend toggles, then the tutorial's button row
  (`cat_tutButtonRowSet` 0x00532480, new in Tutor.c, its readers stage 6) and `cat_ioSubtitleNotify` while the main
  region is enabled; any region that runs out of cards ends the message (HW1: only the letterbox's). The software
  renderer's clear behind the construction manager (`cat_cmIsOpen`) is not taken (decision 28).
- `subStringsChop` 0x00583b00: wraps at width - 1 (HW1: > width).
- The other 13 IDENTICAL bodies compared: same.
- **The Bink term** of `speechQueueUpdate` 0x00521eb0 (`|| !binkDonePlaying`, 0x008b6d94): GoK plays movies with
  its avi player (Win32/bink.c is not built), whose `aviIsPlaying` is the movie-on flag: the term is back as
  `|| aviIsPlaying` (CAT-TODO closed); `subTitlesDraw`'s movie test uses it too.

### misc: ScenPick.c, ColPick.c, Demo.c, Titan.c, bink.c, render.c, Universe.c, Leech.c / .h

- ScenPick.c: `spTitleListLoad` 0x005040c0 (every map name is its own scenario: fileSpec the whole name, no `%d`
  pattern, no merge of sizes, minplayers 2; `gameStart`'s `sprintf` of the level name then copies the name), the
  scenario description as a list window (`spScenarioBitmap` 0x00504fe0 is a function callback `CS_ScenarioBitmap`,
  its item draw 0x00504f70 is `spDescriptionItemDraw`; `spNewItem` 0x00504a20 shows no preview bitmap, fills its
  area with `FEC_Background`, reads `MultiPlayer\<name>\description.txt` and fills the list), the default
  description font registered (`spDescriptionDefaultsAndFreeText`, as `spDonePicking` 0x005046f0 / `spBackPicking`
  0x00504850 inline it), the list and the name cut to 30 characters (0x00503da0, 0x00503e10).
- ColPick.c: `cpValueDraw` 0x00440450's glcompat branch is GoK's backend; the 30 IDENTICAL bodies and both callback
  tables (0x00752ca0, 0x00752cf8) are the tree's. HW1 race aliases `R1` / `R2` retired (`RACE_Sect` / `RACE_Beast`;
  also in Subtitle.c; the `SpeechIcons\R1\` / `R2\` paths are the install's).
- Demo.c `demStateLoad` 0x00462f50: an empty function where HW1 called `mouseLClick`; removed. `demPlayStart`
  0x00462e20: the tree's `DEM_RANDY_SAVE` path (27 streams).
- Titan.c `titanShutdown` 0x0058b1d0: the exe's empty `titanLogFileClose`; the WON interface is GoK's stub (stage 5).
- bink.c: no Bink in the SDL build (GoK's avi player); `binkPause` (cat-new) is not applicable.
- render.c `rndRenderTask` 0x0057a910's front-end part: the saved-cursor frames (no clear, no flip), the screenshot
  flag and the pool resize after `mainReinitRGL` are the Win32 front-buffer / GoK screenshot / renderer-reinit paths
  (decision 28); `rndShamelessPlug` re-targeted (below).
- Universe.c `universeReset` 0x0053df80: also clears the leeches' message words (Leech.c `cat_leechMessagesReset`;
  0x00a762dc .. 0x00a762e4), in the exe's order.

## Self-tests and harness modes

- `/catFeScreensTest <listfile>` (FEFlow.c `catFeScreensRunSelfTest`, HWI-TEST; `tools/m2_harness.sh <label> --mode
  fescreens`): the harness lists every `.fib` of both archives' TOCs as `archive<TAB>name` (22 lines:
  `multiplayer_game.fib` from each). Once the front end is up at `Main_game_screen`, each file is loaded from its
  archive (`catBigFileLoadAllocFrom`, `feScreensImageLoad`), its screens are put first in the screen list, and each
  screen is pushed with `feScreenStart` from an empty stack, its regions drawn once (`regFunctionsDraw`) and popped
  (`feAllScreensDelete`); the file is then deleted and the screen list must be the game's again. The front-end files
  run the game's callbacks; the in-game files (the managers, the in-game menus, the loading screen, chat, console)
  run with every callback and draw callback pointed at a no-op stand-in, and a user region whose draw callback the
  game registers only with its module (the multiplayer screens) gets a stand-in for that file (86). Before the
  screens, `catOptionsRunSelfTest` (the options round trip: 34 options of the Cataclysm set checked at the exe's
  defaults, written with `utyOptionsFileWrite`, scrambled, read back through `utyOptionsList`, compared). Lines
  `fescreentest:` / `optionstest:`, `... done ... result=PASS|FAIL`; then `Main_game_screen` again and the quit.
- `/catStringsTest` (StringSupport.c `catStringsRunSelfTest`; mode `strings`): after the start-up the language script
  is read again line by line (as `scriptSet`); each `LanguageStrings` row maps back by name and fills one id through
  its `dataPtr` (rows 564 / 565 fill ids 565 / 564, as the exe's table); an id whose key the script has must hold the
  script's text (through the `strSetStringCB` rules), one without stays `*NO_STRING*`; ship names counted. The 24 ids
  without a row in the install's English.script (update.big): 293 294 (`strR1TECH_BIOWARHEADinfo`,
  `strR1TECH_DRIVE_CORVETTEinfo`), 1181 .. 1198 (the force-field key-binding names / descriptions), 1556 1569 1574
  1579 (`str_GSD_*`).
- The harness: modes `fescreens` and `strings` (header, milestones `fescreens self-test done`, `options round trip
  done`, `strings self-test done`, summary lines). The misaligned `struct fescreen` / `featom` / `fibfileheader`
  reports are no longer filtered as benign: they count in every mode, and the summary prints their number.

## Harness runs (plain harness)

| Run | Mode | Milestone | First stop | ASan | Notes |
|---|---|---|---|---|---|
| r6fe-base-fe | fe 90 s (base) | Main_game_screen | none: still running at the 90s timeout | 0 | 92 `struct fescreen` reports at 80 FEFlow.c sites, 299 file loads |
| r6fe-base-mission | mission 6000 (base) | oracle mission run done | none (rc 0) | 0 | `a36f9a53b3f8d377`, IDENTICAL to R5 |
| r6fe-align-fe | fe 60 s | Main_game_screen | none: timeout | 0 | the alignment fix alone: 0 UBSan reports |
| r6fe-strings-1 / -2 | strings | strings self-test done | none (rc 0) | 0 | -1 FAIL (the test compared by row, not by `dataPtr`: rows 564 / 565); -2 PASS |
| r6fe-fescreens-1 .. -3 | fescreens | Main_game_screen | ASan (callbacks of in-game screens out of context, then a user region with no callback) | 1 | the test's stand-ins came from these |
| r6fe-fescreens-4 .. -7 | fescreens | fescreens self-test done | none (rc 0) | 0 | PASS; -5 an ASan in the read-back's path length (fixed), -7 `startdefaults=31` (the start-up's effects preset sets CPV / lens flares / repair droids: table fixed) |
| r6fe-mid-mission | mission 6000 | oracle mission run done | none (rc 0) | 0 | IDENTICAL to R5 |
| r6fe-fin-fescreens | fescreens | fescreens self-test done | none found in the log (rc=0) | 0 | PASS / PASS (above); 312 file loads; no UBSan report |
| r6fe-fin-strings | strings | strings self-test done | none found in the log (rc=0) | 0 | PASS (above); 283 file loads |
| r6fe-fin-fe | fe 120 s | Main_game_screen | none: still running at the 120s timeout | 0 | 0 UBSan reports (base 92); 299 file loads (base 299) |
| r6fe-fin-mission-a / -b | mission 6000 | oracle mission run done | none found in the log (rc=0) | 0 | byte-identical, `a36f9a53b3f8d377`, IDENTICAL to R5; 3,781 file loads (base 3,781) |
| r6fe-fin-sp | sp 340 s | universe running (univUpdateCounter > 0) | none: still running at the 340s timeout | 0 | univUpdateCounter 1,248 (base `r6mp-fin-sp`, the 60bfd21 binary: 1,248); 3,782 file loads (base 3,782); first non-benign UBSan BTG.c:388 as base; the UBSan kinds are the base's minus the 110 `struct fescreen` reports (FEFlow.c 101, TaskBar.c 7, Sensors.c 2) [audit: plus BTG.c:864 and stb_image_write.h:1263, one report each: see the gate table] |

## Delta-map review entries

272 entries with `[review: M2-R6 frontend]` in `ghidra/delta_map_review.json` (260 new with sample
`port-R6-frontend`, 12 existing ones with the note appended), merged with `python3 -I tools/delta_map.py merge --done
m2r6:frontend` (272 records changed); `docs/m2-r1-function-deltas.md` regenerated. Counts: IDENTICAL 2,999 -> 2,985,
MODIFIED 1,344 -> 1,358, REWRITTEN 77, reviewed 2,199 -> 2,459. [audit: + 1 review by the audit, `nisTextCardListDraw` 0x004d51f0 (IDENTICAL, edited for the new `fontPrintN` argument without a review), and `fontPrint` 0x00558240's R2 note corrected (the exe passes `bAdditive` 0, not a scrambled display): reviewed 2,460]

- Every IDENTICAL body of the scope that had no review (230) was compared with r6 through `tools/delta_map.py show`
  (and `m2_fdec.py` / `m2_fdis.py` / the full disassembly where the side-by-side was cut): 214 same. Reclassified
  IDENTICAL -> MODIFIED and ported (15): `uicRadioButtonProcess`, `glfontDisplayString`, `fontPrintN`,
  `opTroubleShoot`, `opEffectsHelper`, `opOptionsSaveCustomEffectsSettings`, `opEffectsToVideoAccept` / `Cancel`,
  `opEffectsToInGameVideoAccept` / `Cancel`, `opDrawNumChannels`, `opEffects`, `opDetailThreshold`, `subStringsChop`,
  `spScenarioNameDraw`. Reclassified MODIFIED -> IDENTICAL (1): `ferDrawBitmapButton` (the inlined string loops).
- The unread MODIFIED / REWRITTEN bodies of the scope (39): FEFlow 3, FEReg 1, UIControls 3, font 1, ScenPick 6,
  ColPick 1, Options 19, Demo 2, StringSupport 2, Titan 1: all read; ported where the change applies (above), noted as
  GoK's backend / not applicable otherwise (`feShouldSaveMouseCursor`, `cpValueDraw`, `titanShutdown`, the device code
  of `opOptionsAcceptHelper`).

Decision 29 (the 0.325 pixel-centre offset) and decision 28 (GoK's GL backend) are kept: none of the ported draws
adds the offset, and the RGL / glcompat / software-renderer branches (the GL_POLYGON_STIPPLE toggles of the effects
presets, the subtitle picture's `glDrawPixels` path, the software clear) are not taken.

## CAT-TODOs closed / re-targeted

Closed: SoundEvent.c (`soundEventHearActor`: the voice preview's callers are gone), ShipSelect.c (the
`OP_ExperienceMessages` toggle and option row), Subtitle.c (`subTitlesDraw`), Sensors.c (the options half of its
line), InfoOverlay.c (the taskbar options), SpeechEvent.c (the Bink term), Universe.c (the three UI words: they are
the leeches' message words), render.c (the front-end half: see below).

New / re-targeted, owner first:
- `CAT-TODO(M2-R6 key bindings)` x3 in Options.c: `cat_kbReloadKeyBindings(FALSE)` (0x004d7f80, 0x004d8370) and
  `cat_kbSaveKeyBindings` (0x004d8d00); HW1's `kbRestoreSavedSettings` / `kbSaveSettings` stand in.
- `CAT-TODO(M2-R6 start-up)` in utility.c: the rest of the options table at the exe's (0x008c1478 .. 0x008c18d4:
  names, order, rows; no GoK sections), with `DifficultySetting` for stage 6. [audit: the comment spanned three lines; now on one line, owner first]
- `CAT-TODO(stage 6 single player)` in utility.c: `{cat_spGameDifficulty, "SP_GameDifficulty"}` (0x0051c140).
- `CAT-TODO(M2-R7 render)` in render.c: `rndShamelessPlug` 0x00579ff0 (plug.tga while the console or the game-stats
  screen is up, or during a load): its GL quad is to be read from the disassembly (HW1's source has no body).

No `CAT-TODO` / `CAT-PROVISIONAL` in src/ names an M2-R6 front-end, options or subtitles owner any more.

## What is left, and owner

- The key-binding calls of the options (M2-R6 key bindings, producer 3).
- The options table's other rows at the exe's and the `/settingspath` read of the options file (`utyOptionsFileRead`
  reads `$HOME`; the round trip reads the written file back explicitly) (M2-R6 start-up, producer 3).
- `rndShamelessPlug` (M2-R7 render); `cat_trPrecacheShipTextures`'s caller and `cat_spGameDifficulty` (stage 6);
  the Titan / WON interface and `cat_titanAdBannerSetImage` (stage 5).
- `cat_tutButtonRowSet`'s readers (the tutorial's button layout, stage 6 tutorial).
- Not applicable to the SDL build (decision 28): the front-buffer cursor save, the RGL / glcompat / software-renderer
  branches, Bink (`binkPause`) and the CD-key code that sits after it.
