# Audit: M2-R6 frontend (producer 2 of M2-R6)

Port report: [docs/m2-r6-frontend.md](../m2-r6-frontend.md). Base `60bfd21`; the port is the working tree on top of
`3e1bea0` (the user's asset-export commit after `60bfd21`, `tools/assets/` + `docs/assets-export.md`, not port work).
Every engine run below went through `tools/m2_harness.sh` (plain harness, silent, offscreen) with the audit's own clean
debug binary (`--bin`); no Ghidra, no GOG game, no input.

## 1. Clean builds

`rm -rf original/scratch-m2/build-audit-r6-frontend`, then `meson setup` / `meson compile` with `tools/build.sh`'s
arguments (debug: `--buildtype=debug -Db_sanitize=address,undefined`; release: `-Doptimization=2 -Ddebug=false
-Db_sanitize=none`; `werror=true` from meson.build):

| build | result | warnings | size |
|---|---|---|---|
| debug (ASan + UBSan) | 309/309, rc 0 | 0 | 83,677,184 B |
| release (-O2) | 309/309, rc 0 | 0 | 5,131,392 B |

After the audit's fixes (comments, one `CAT-TODO` re-wrapped): both rebuilt, rc 0, 0 warnings, same sizes.
`tools/m2_layout32.sh`: OK (now with FEFlow.h).

## 2. Harness runs (audit binary, plain harness)

| run | mode | result | ASan | fescreen UBSan |
|---|---|---|---|---|
| aud6fe-fescreens | fescreens | `fescreentest: done files=22 loaded=22 screens=195 pushed=195 popped=195 wrong=0 skipped=0 listchanged=0 userregionstandins=86 result=PASS`; `optionstest: done options=34 startdefaults=34 readback=34 result=PASS`; 312 file loads, rc 0, no UBSan report of any kind | 0 | 0 |
| aud6fe-strings | strings | `stringstest: done ids=1705 mapback=1705 resolved=1681 nostring=24 wrongtext=0 wrongnostring=0 result=PASS`; ship names 145 / 145; 283 file loads, rc 0 | 0 | 0 |
| aud6fe-fe | fe 120 s | `Main_game_screen`, still running at the timeout, 299 file loads, no UBSan report of any kind | 0 | 0 |
| aud6fe-mission-a / -b | mission 6000 | `oracle mission run done`, rc 0; the two `run.hwo` byte-identical (`cmp`), sha256 `0bb66db9e257f356`; `hwo_compare.py` against R5's `r5-integrate-mission-a` (`645f94c69d68319e`): `HEADER: same producer, different build identity: 02e8e4da/1fbfa32f vs 3e1bea00/1fbfa32f`, `IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference` | 0 | 0 |
| aud6fe-sp | sp 340 s | universe running, univUpdateCounter 1,248 (base `r6mp-fin-sp` 1,248), 3,782 file loads (base 3,782), still running at the timeout | 0 | 0 |

The sha differs from the producer's `a36f9a53b3f8d377` only through the header's build identity (the audit binary is
built at `3e1bea0`); the tick records are identical to R5's. Fescreen UBSan reports in every summary of the audit: 0.

## 3. The DONE WHEN, re-checked

| gate | verdict |
|---|---|
| `/catFeScreensTest` (mode `fescreens`): every screen of the install's 22 `.fib` files pushed and popped once, ASan 0, 0 `fescreen` UBSan | **met** (aud6fe-fescreens; the list is both TOCs: 21 names + `multiplayer_game.fib` from each archive) |
| `/catStringsTest` (mode `strings`): all 1,705 ids | **met** (aud6fe-strings). Independently: the 1,705 `strEntry` rows of `LanguageStrings` against the keys of `game_extracted/update/english.script` leave exactly the 24 ids the test reports at `*NO_STRING*` (`strR1TECH_BIOWARHEADinfo`, `strR1TECH_DRIVE_CORVETTEinfo`, the 18 `str_UI_ForceField*` names / descriptions, `str_GSD_BasicGroup` / `ShipsKilled` / `ShipsLost` / `ShipsBuilt`); `strLoadLanguage` 0x005285f0 leaves them so (its warning loop calls a nullsub) |
| the Cataclysm option set at the exe's defaults round-trips through the options file | **met** (34 / 34 at the start-up's values, 34 / 34 read back) |
| fe `Main_game_screen` with 0 fescreen reports | **met** (aud6fe-fe: 0 reports of any kind; the base had 92 `struct fescreen`) |
| sp unchanged | **met** by the milestone, univUpdateCounter and ASan (1,248, ASan 0, same file loads). The report's "the base's UBSan kinds minus the 110 fescreen reports" is not exact: finding W1 |
| mission stream IDENTICAL to R5's `645f94c69d68319e` | **met** (aud6fe-mission-a = -b; IDENTICAL to R5 but for the build identity) |
| builds, layout32 | **met** |

## 4. Bodies compared with the exe (`cataclysm_gog_r6`, `tools/m2_fdec.py` / `m2_fdis.py` / `m2_exedata.py`)

All of these match the tree (35 checks, 0 wrong):

- **UIControls / FEFlow / FEReg:** `uicToggleProcess` 0x00534c00 (0x13a / 0x13b for `CP_RaceOne` / `CP_RaceTwo` on a press by
  `strcmp`, else 0x10d / 0x10c by `FAS_Checked`; the key-down plays by the state before the toggle), `uicRadioButtonProcess`
  0x00534dc0 (0x10f on press and key-down), `nextLog2` 0x00476920 (the 2..256 ladder; `((0x100 < n) - 1 & 0x101) - 1` is
  -1 above 256: the CAT-FIX is marked), `feCurrentScreenDelete` 0x00475380 (no tactics clear; `regKeysFocussed` is
  0x00a57644; trailing `glcRenderEverything`).
- **Options (Cataclysm set):** `cat_opSetAllDefaults` 0x004db7b0 (both presets field for field, including LOD detail 0 / 2,
  texture 50 / 0, detail threshold 10 / 30, the five sensors words, then experience / compass / sensors sphere 0 and the
  eleven buttons; the `GL_POLYGON_STIPPLE` call through 0x00b3a4b0 / 0x00b3a47c is the RGL device, decision 28),
  `cat_opCataclysmFramerate` 0x004dbb30, `cat_opHomeworldClassic` 0x004da2c0 and `cat_opCataclysmDefault` 0x004da460
  (every word and every button, `OP_AreaExplored` passed as the constant), `cat_opResources` 0x004da110 (resources +
  meteors), `cat_opSmallFonts` 0x004d9df0 (inverted), `cat_opHomeworldCSA` 0x004d9f30 (exclusive pair),
  `cat_opLODDetail` 0x004d9d20, `cat_opTextureDetail` / `Process` 0x004db6f0 / 0x004db6d0 (max 100, `RPE_HoldLeft`),
  `cat_opSpeechTest` 0x004db760 (0x2001024, 5.0) and `cat_opSoundFXTest` 0x004db790 (0x1c8).
- **Options (HW1 bodies):** `opOptionsInit` 0x004d71a0 (no `kbInitKeyBindings`), `opUpdateSettings` 0x004d71d0,
  `opEffectsHelper` 0x004d9b90 (high / low presets with CPV, lens flares, repair droids, the effect kinds),
  `opNumEffectsProcess` 0x004d9900 (custom switch only off the preset's value), `opDetailThreshold` / `Process`
  0x004da5e0 / 0x004d9b20 (max 0x15, `0x1e - value`), `opCPUDifficulty` 0x004db400 (radio, pData low byte),
  `opOptionsSaveSettings` 0x004d8d00 (the 23 Cataclysm words the tree's `catOpSavedWords` saves are the exe's, in its
  order), `opEffectsToVideoAccept` / `Cancel` 0x004d88a0 / 0x004d8990 against the tree's custom-effects save / restore
  (CPV 0x00c52378, lens flare 0x00c52380, repair droids 0x00c52374, LOD detail 0x00a4cc38 added), `opDrawNumChannels`
  0x004d9630 (0xff7a635a, white).
- **Strings / fonts / subtitles:** `strSetStringCB` 0x00528800 (`http:` kept as written), `strLoadLanguage` 0x005285f0,
  `subTitlesDraw` 0x00584620 (the backing strip's rectangle checked in the disassembly: the call passes `[esp+0x30]` =
  {0, y0 + iconOffsetY, MAIN_WindowWidth, +0x40}, colour round(40.0) in every byte, blend on / off through 0x00b3a47c /
  0x00b3a4b0; the fade rounding; `cat_tutButtonRowSet` then `cat_ioSubtitleNotify` while `mrDisabled` 0x00a752bc is 0;
  any region ends the message), `cat_fontPrintSubtitle` 0x00558270 (pushes x, y, c, 1, string, 0x7fffffff) with
  `fontPrintN` 0x005582c0 passing its fourth argument as `glfontDisplayString` 0x00557be0's sixth.
- **ScenPick:** `spNewItem` 0x00504a20 (`"%s%s\description.txt"` with `MultiPlayer\` and spscenario+8 =
  `bitmapfileSpec`; the inset 6 `FEC_Background` fill; language fallbacks; the list refill), `spScenarioBitmap`
  0x00504fe0 (`uicListWindowInit` with `fontHeight / 3 + fontHeight`, flags 2 = `UICLW_CanSelect`), `spTitleListLoad`
  0x005040c0 (whole name, maxplayers from the name, minplayers 2, no merge).
- **Other:** `universeReset` 0x0053df80 (0x00a762e0 / e4 / dd / dc cleared after `cat_taskTimeCompressionReset`),
  `cat_trTextureMemorySet` 0x00589cd0 (pool 10..90 MB, cram 1..12 MB, the 0x80000 fallback),
  `cat_trPrecacheShipTextures` 0x00589910 (the six rows), `utyGameSystemsInit` 0x005abe70 (`kbInitKeyBindings` after
  `cat_gameStatsStartup`; the memory test 0x280de81 / 0x4692681 / 128,000,000 with 0x00a76260, 0x00c52378 and
  0x00c5236c), and the options-file rows 0x008c1790 .. 0x008c18c8 (27 rows `LargePopupMenuFonts` ..
  `SetTextureRequirements`: names, order and target addresses as the tree's `[cataclysm options]` rows).

**Option defaults against the exe's data** (15 checked, 0 wrong): music 60 (0x0088be4c), SFX 75 (0x0088be50), speech 75
(0x0088be54), detail threshold 10 (0x0088be6c), texture detail 50 (0x0088be74), game frame rate 1 (0x0088be98),
RightClick_CSA 1 (0x008b9ec8), and the sensors words 0x00892b48 .. 0x00892b64 (8) all 1; also 0x00892b68 .. 0x00892b70
1 and the .bss words (0x00a5749c, 0x00a4cc18, 0x00a76260, 0x00a4cc38, 0x00a5b19c .. 0x00a5b1a4) beyond the file image,
i.e. 0: the test table's defaults are the exe's.

## 5. The diff (git diff 60bfd21)

- **Scope:** every file touched is the plan row's (FEFlow, FEReg, UIControls, font, ScenPick, ColPick, Options, Demo,
  StringSupport, Subtitle, the Bink term in SpeechEvent.c, the voice preview in SoundEvent.c, `rndRenderTask`'s front-end
  comment) or carries what that work needs: the option words' owners made extern (InfoOverlay, Sensors, PopupMenu,
  ShipSelect's word renamed after the function), `cat_tutButtonRowSet` for `subTitlesDraw` (Tutor.c), the
  `universeReset` words the R5 `CAT-TODO(M2-R6 front end)` named (Universe.c / Leech.c), the `fontPrintN` call sites
  (NIS.c, UIControls.c), texreg.c / utility.c for the option set, main.c / BigFile.c / the harness for the self-tests.
  The memory test in `utyGameSystemsInit` sits in producer 3's file but is the texture-detail option's start-up half.
- **HW1 aliases:** none left (outside comments and string literals) in FEFlow.c, FEReg.c, UIControls.c, font.c, FontReg.c,
  ScenPick.c, ColPick.c, Options.c, Demo.c, StringSupport.c, Subtitle.c, Titan.c (the 37 ship aliases of ShipDefs.h and
  R1 / R2 / P1 / P2 / P3 / Traders).
- **CAT comments:** every Cataclysm change reads with a `CAT:` / `CAT-FIX` comment and its address (`feScreensDelete`'s
  offset uses `FIB_ScreensOffset`, whose definition carries the CAT-FIX).
- **CAT-TODOs:** none names a front-end / options / subtitles owner any more; the three `CAT-TODO(M2-R6 key bindings)`
  are on one line, owner first. One new one was not: W2.
- **Edited IDENTICAL functions:** `cpAcceptColors` (comment only), `feScreensDelete`, `uicTextEntryDraw` have M2-R6 frontend
  reviews; `fontPrint` has an R2 review; `nisTextCardListDraw` had none: W3.
- **Delta map:** 272 entries with `[review: M2-R6 frontend]` (260 new, sample `port-R6-frontend`), counts 2,999 -> 2,985 /
  1,344 -> 1,358 / 77, reviewed 2,199 -> 2,459, and the 16 reclassifications (15 IDENTICAL -> MODIFIED, `ferDrawBitmapButton`
  MODIFIED -> IDENTICAL) are as reported; `docs/m2-r1-function-deltas.md` regenerated from them (the audit's rerun of
  `delta_map.py report` changed only what the audit's own review added).

## Findings

| # | finding | evidence | fix |
|---|---|---|---|
| W1 | The report's sp row (and the run table) says the sp UBSan kinds are the base's minus the 110 fescreen reports. Two kinds the base never had appear, one report each, in the producer's `r6fe-fin-sp` and the audit's `aud6fe-sp` alike: `BTG.c:864` (btgLoad's star-filename length, a misaligned `udword` load; also in every mission run from `r6fe-mid-mission` on, absent from `r6fe-base-mission` and R5's runs) and `stb_image_write.h:1263` (signed left shift in the JPEG writer of the `/debugScreenshots` shot). No earlier sp run of R5 / R6 (20 surveyed) has either. Both non-fatal, ASan 0, the stream IDENTICAL; same file (`BTG/Mission01.btg`) loaded as in the base, so the cause is not settled here (the BTG kind is the same LP64 loader artefact as the known `BTG.c:388`) | the summaries named | report corrected `[audit: ...]` (gate table and run table) |
| W2 | New `CAT-TODO(M2-R6 start-up)` in utility.c spanned three comment lines (the owner grep returns a cut sentence) | utility.c 824-826 before the fix | re-written on one line, owner first |
| W3 | `nisTextCardListDraw` 0x004d51f0 (IDENTICAL, unreviewed) was edited (the new `fontPrintN` argument) without a review entry. The edit is right: the exe pushes 0 as the fourth argument (0x004d5484) | `delta_map_review.json` had no key | review entry added (sample `audit-R6-frontend`), `delta_map.py merge` (2 records), `report` regenerated: reviewed 2,460 |
| W4 | `fontPrint` 0x00558240's R2 review note says its call is `fontPrintN(x, y, c, string, SDWORD_Max)` and Ghidra "scrambles the argument display"; the port showed (and the disassembly confirms: push 0x7fffffff, string, 0, c, y, x) that the fourth argument is Cataclysm's `bAdditive` 0. The producer edited `fontPrint` and left the note | `m2_fdis.py 0x00558240` | note corrected `[audit: ...]`, merged |
| W5 | UnivUpdate.c's comment on `catMeshStateDisabled` still said "This tree never sets it" after this port's memory test sets it under 128 MB | UnivUpdate.c 1424-1426 | comment corrected `[audit: ...]` |
| W6 | `subTitlesDraw`'s comment on the software-renderer branch named only `cat_cmIsOpen`; the exe also takes it while 0x00a57744 is set, clears only above 640 px and sets bit 0 of 0x0088f454 (branch not taken: decision 28) | `m2_fdec.py 0x00584620` | comment completed `[audit: ...]` |

Not findings (noted): the options round trip's `result=PASS` does not require `startdefaults == options` (both are 34
here); `mainrgn.c`'s `catMrWaypointCentre` comment still cites 0x004d9fde (inside `cat_opAutoFocusOnWaypoints`
0x004d9fd0, so not wrong); the `[cataclysm options]` section header of the options file is GoK-style, which the
re-targeted `CAT-TODO(M2-R6 start-up)` already covers.

## Precision

Sampled checks: 35 body comparisons + 15 option defaults + 8 gate re-runs (fescreens, options round trip, strings, fe,
sp, mission pair, builds, layout32) + 4 further claims (the 24 unresolved ids, the delta-map counts and
reclassifications, the alias retirement, the closed CAT-TODOs) + the 6 findings = 68; wrong 6 (W1-W6); correct 62.
**Precision 62 / 68 = 0.912.**

## Fixes (6)

1. src/SDL/utility.c: `CAT-TODO(M2-R6 start-up)` on one line (W2).
2. ghidra/delta_map_review.json: review of `nisTextCardListDraw` 0x004d51f0 (W3); merged into ghidra/delta_map.json;
   docs/m2-r1-function-deltas.md regenerated.
3. ghidra/delta_map_review.json: `fontPrint` 0x00558240's note corrected (W4), merged.
4. src/Game/UnivUpdate.c: `catMeshStateDisabled` comment (W5).
5. src/Game/Subtitle.c: the software-branch comment (W6).
6. docs/m2-r6-frontend.md: the sp UBSan-kind claim (W1), plus `[audit: ...]` notes on the delta-map counts and the
   re-wrapped CAT-TODO.

Builds after the fixes: debug and release rc 0, 0 warnings.

Verdict: PASS-WITH-FIXES - every DONE WHEN gate reproduced on the plain harness (fescreens 195/195 with 0 fescreen UBSan, strings 1,705, options 34/34, fe, sp 1,248, mission IDENTICAL to R5), 35 bodies and 15 defaults match the exe; fixed a three-line CAT-TODO, a missing and a stale review, two comments and the report's sp UBSan claim (two new non-fatal kinds, BTG.c:864 and stb_image_write.h:1263, left open)
