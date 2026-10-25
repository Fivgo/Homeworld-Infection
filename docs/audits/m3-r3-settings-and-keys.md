# Audit: M3-R3 producer 1, settings-and-keys

Auditor's scope: `git diff 7aa3111` in the m2 worktree (no port commits after 7aa3111; c698dd5, 06976ff, dae51e1 and 7f24d76
are the orchestrator's: play.sh's rebuild rule, cycle/answers, the workflow script, STATE.md). The producer's report is
`docs/m3-r3-settings-and-keys.md`. The answers file `cycle/answers/m3-r3-user.md` was read at the start and again before
this report: md5 ccdc5c26, unchanged since dae51e1. No later answer changes the task.

Every engine run went through `tools/m2_harness.sh` (silent, offscreen) with labels `m3r3a-settings-and-keys-*`, using the
auditor's clean build (below) through `--bin`. No GPU timing was measured, so the idle-GPU rule did not apply. No
gamescope, no OS input, no Ghidra.

## 1. Clean builds

`original/scratch-m2/build-audit-m3r3-settings-and-keys/`, set up with the exact options of `tools/build.sh`:

| config | setup | result | warnings | size |
|---|---|---|---|---|
| debug | `--buildtype=debug -Db_sanitize=address,undefined` (werror true, warning_level 3) | rc 0 | 0 | 90,810,680 B |
| release | `-Doptimization=2 -Ddebug=false -Db_sanitize=none` | rc 0 | 0 | 5,758,536 B |
| win64 cross | the `tools/release_win64.sh` options: llvm-mingw cross files, `-Dmovies=false` | rc 0 | 0 | 5,369,344 B (`hwi-engine.exe`) |

After the auditor's one-line help-string fix (finding A), all three were rebuilt with rc 0 and 0 warnings. The tree's own
`tools/build.sh --debug` and `--release` also give rc 0. The only "warning" text in their logs is meson's line
"Building with memory sanitizers on", which is not a compiler warning. `tools/m2_layout32.sh`: OK.

The port adds no source file and no GL entry point beyond GL 1.1 (`glGetTexParameteriv` is GL 1.0). No `_WIN32` code was
touched: the `#if` lines in the diff are context only.

## 2. The exe, read by the auditor

### The overlay's key (F1)

- **`mrStartup 0x00563b50`** (decompile): `regKeyChildAlloc(parent, ID, 0x180000, mrRegionProcess, ...)` for ID 0..0xff.
  So every key goes to `mrRegionProcess 0x00567930`, whose key event (0x80000) calls `cat_kbKeyPress(0, ID)`. There is no
  hardwired F-key case.
- **`WindowProc 0x00561f10`**: on WM_KEYDOWN every key goes to `keyPressDown` and `cat_consoleKeyPress`. On WM_KEYUP
  (0x101) only Escape (0x1b), F11 (0x7a, swallowed) and F12 (0x7b, the renderer-reload path) are special-cased.
- **`default_keys.cfg`** (original/game, `v 84`):
  - Plain F2..F12 are bound: 113-115 to Tactics_*, 116-123 to Formation_*. Row 59 is `Formation_Parade 123`.
  - Plain 112 (F1) has no row. F1 appears only as `Leech_Spy SHIFT+112`, `ToggleSV_AreasVisited CTRL+112` and
    `Toggle_LOD_Drawing SHIFT+CTRL+112`.
- **`cmBuildHotKey 0x00458d40`**:
  - It takes 0x70..0x7b and returns 1 even when the key is not in its table.
  - Its table at `.data 0x00753ad0` (`m2_exedata.py dwords`) holds 0x74, 0x73, 0x76, 0x7a, 0x7b, 0x7b, 0x12 and then
    0xffff. There is no 0x70, so F1 builds nothing in the construction manager.
- **Hot keys in the .fib files.** The auditor wrote an independent scan of the 21 .fib files in cataclysm.big: 4,664 atoms,
  with `hotKeyModifiers` at disk offset 48 and `hotKey[5]` at 49. The hot keys used are 0x1b, '1'-'3', 'A'-'Z' and 0xcd.
  No atom uses an F key (0x70-0x7b).
- `decompiled.c` has no key compare against 0x70. All twelve `0x70` hits are ship types, struct offsets, or CRT and
  library code.

**Verdict on the key.** F1 is right and F12 would have been wrong:
- Plain F1 is unbound in the exe's main view and in the tree's table.
- No default binding is plain F1.
- A user's `keys.cfg` can bind plain F1, and `hwiKbOverlayFKeyFree()` then yields the key (the keys self-test proves this).
- No front-end hot key uses F1.
- The construction manager swallows F1 but builds nothing on it. The overlay still toggles there, as stated.

### Texture creation and filtering (the "original look")

- **`trRGBTextureCreate 0x005866a0` and `trPalettedTextureCreate 0x00586500`**:
  - Both set `GL_TEXTURE_WRAP_S/T` (0x2802/0x2803) to `GL_REPEAT` (0x2901).
  - On `texLinearFiltering`, both set `MIN_FILTER` (0x2801) and `MAG_FILTER` (0x2800) to 0x2601 (`GL_LINEAR`), or both
    to 0x2600 (`GL_NEAREST`).
  - There is no mip-map enum and no anisotropy.
- **`trFilterEnable 0x005888e0`**: the same walk as the tree (allocated, not pending, `sharedFrom == -0x204050b`). It sets
  min and mag on every branch.
  - HW1's `src/Win32/texreg.c` does the same.
  - So the double `MIN_FILTER` in GoK's third branch is GoK's fault. The `CAT:` fix of the mag filter is correct.
- **`opEffectsHelper 0x004d9b90`** (called from `opUpdateSettings 0x004d71d0`):
  - The HIGH preset (`opEffectsVal 0`) sets `texLinearFiltering = 1` and LOW sets 0. Every path ends in `trFilterEnable`.
  - The producer's correction holds: the exe undoes a hand-written `texLinearFiltering 0` and `/noFilter` at every start
    under HIGH.
- **The Cancel paths.** `opRestoreSavedSettings 0x004d7f80`, `opEffectsToVideoCancel 0x004d8990` and
  `opEffectsToInGameVideoCancel 0x004d8b60` restore `texLinearFiltering` without a `trFilterEnable` call. The render.c
  `CAT-FIX` statement is true.

**Verdict on the base.** "The original look" = `GL_LINEAR` magnification. This is stated correctly. The tree's GoK
mip-mapped minification and 8x anisotropy (decision 28) are stated as a difference and left to the user (question 2).

### The eight bodies compared (four IDENTICAL, four MODIFIED)

| function | delta-map class | what the auditor found |
|---|---|---|
| `trFilterEnable 0x005888e0` | IDENTICAL | exe = HW1, both set min and mag. The tree differs only by GoK's mip / aniso enums; the mag fix restores the exe's pairs. M3-R3 review row present |
| `trRGBTextureCreate 0x005866a0` | IDENTICAL | as above; review row present |
| `trPalettedTextureCreate 0x00586500` | IDENTICAL | as above; review row present |
| `utyFilterToggle 0x005a86d0` | IDENTICAL | exe: first call shows the box and saves the custom effects; a click sets the flag and the twin box; `trFilterEnable` in both. Tree as the exe plus the switch end. M3-R3 note present |
| `utyFilterBitmapToggle 0x005a8740` | IDENTICAL | as the exe; M3-R3 note present |
| `opEffectsHelper 0x004d9b90` | MODIFIED | HIGH sets 1, LOW sets 0, CUSTOM leaves it, ends in `trFilterEnable`. The tree's hooks sit before that call. Note present |
| `mrStartup 0x00563b50` / `mrRegionProcess 0x00567930` | MODIFIED / REWRITTEN | 256 key children; key event to `cat_kbKeyPress`; no F-key case |
| `WindowProc 0x00561f10` / `cmBuildHotKey 0x00458d40` | MODIFIED | as stated (above). Notes present |

Also read: `partFilter 0x004dc4a0` (IDENTICAL, tree = exe), `trClearCurrent 0x005883d0` (IDENTICAL) and
`opCustomEffects 0x004da710` (IDENTICAL). Each matches the tree.

**Delta map.** Re-running `python3 -I tools/delta_map.py merge` on 7aa3111's `delta_map.json` with the current review
file reproduces the committed `delta_map.json` byte for byte ("records changed 15"). The counts are IDENTICAL 2757,
MODIFIED 1580, REWRITTEN 83, reviewed 3582.

## 3. The fences, rerun from the clean build

| fence | run(s) | result |
|---|---|---|
| keys 197 / 149 | `keys` | `overlay fkey F1 scancode=58 vk=0x70 plain bindings F1=0 F12=1 free=1 bound-free=0 reset-free=1`; `done rows=197 known=149 ... bindings=149 ... failures=0 result=PASS` |
| F1 toggles the overlay (PPMs) | `fkey` (`-- /catOverlayKeyAt 150,200`) and twin `fkey-off` | `hwiscript: tick=150 ... overlay=1`, `tick=200 ... overlay=0`. `ppm_region.py diff` gives tick 140 `differ=0`, tick 160 `differ=14400 bbox=2,2,227,66 maxdiff=204`, tick 210 `differ=0`. The auditor's own P6 reader agrees: 0 / 14,400 / 0 differing pixels, inclusive bbox (2,2)-(226,65), and the mean of the 2..227 x 2..66 box is 36.4 with the key against 40.0 without at tick 160 and equal at 140 / 210 (box present only between the presses) |
| toggle, plain and 8 switches | `toggle`, `toggle-{noInterp,noNISInterp,letterboxBehind,lodBands,overlay,meshImmediate,noFilter,sharpTextures}` | all `fails=0 result=PASS`, rc 0, ASan 0. The rule is applied consistently: `letterboxBehind -> 1 expect 'letterboxBehind=1 (switch)' line=yes` under /letterboxBehind (M3-R2's failing check); objInterpolate / nisInterpolate `(switch)` under /noInterp; noLOD `(switch)` under /lodBands; texLinearFiltering `(switch)` under /noFilter; meshImmediate `(console)` under /meshImmediate (row-less: the switch ends); `(console)` for every unswitched row |
| the Options Accept path | `toggle` (`toggletest: sharp ...`) | checkbox and label on Video_Options (692,508 / 692,490) and on the in-game Video screen; a click sets `texLinearFiltering=0 effectsLevel=2`; Accept gives `passes=1 ... wrong=0 row=0 rows=1 effectsLevel=2`, line `(cfg) [options accept]`; read back 0 / 2; Cancel; the HIGH preset sets 1 `(preset)`; the switch is run-only; end state restored |
| the re-filter count in game | `cap-live` (`/catConsoleAt 180:texLinearFiltering=0,220:texLinearFiltering=1`) | `filter=sharp textures=2343 globjects=2343 wrong=0 scaled=462 pass=2` then `filter=linear ... 2343 / 2343 wrong=0 ... pass=3`; tick 200 byte-equal to `/sharpTextures` and tick 240 byte-equal to the default (`differ=0` both) |
| capture pairs, original vs sharp | `cap-lin` / `cap-sw` 1024x768 | t200 `differ=2685 bbox=810,366,906,410 maxdiff=35`, t240 `differ=2674` |
| | `cap4k-lin` / `cap4k-sw` 3840x2160 | `differ=20864 bbox=2758,1028,3027,1153 maxdiff=84` |
| | `ku4k-lin` / `ku4k-sw` (Kuun-Lan at 2,300 u, 4K) | `differ=890954 bbox=1265,0,2634,2160 maxdiff=125`, counted independently: 890,954 of 8,294,400 |
| texel edges on the hull | `ku4k-*` crop 1560..1880 x 1450..1650 (auditor's script) | identical-neighbour fraction 0.714 sharp against 0.197 linear. Runs of near-equal pixels (tolerance 6): median 5 / mean 7.0 sharp against median 2 linear. At tolerance 0, median 2 / mean 3.5 sharp against 1 / 1.24. So the sharp frame shows texel blocks. The producer's TextureDetail 100 pair gives median 4 / mean 6.0 at tolerance 6, in line with its "5-6 px steps" (the Gouraud shading inside a texel breaks exact equality) |
| sharp dumps (geometry) | `motion-sharp`, `nis-sharp` | crc-stripped equal to flags-on (428ef41479eb5749 / a5de13a44b8d264f), 0 non-crc lines differ; crc moved on 1,431 / 1,431 motion frames and 1,439 / 1,440 nis frames; whole 1a2227d966ec9712 / 871eac333fd603d0 (= the producer's) |
| the four dumps | `motion-noi`, `motion-on`, `nis-noi`, `nis-on` | **EQUAL**, cmp-identical to M3-R2's `m3r2i-e-*` (table in section 6) |
| opt-outs | `motion-optout`, `nis-optout` (`--cfg 'letterboxBehind 1' -- /lodBands /noInterp`) | 183681235a3fdc7b / 3016280e9e0dd3c8 and 13ca70eea998e5de / 196bab32d244f614 = R10's /noInterp dumps |
| Mission01 pair | `mission-a`, `mission-b`; final tree `fmission-a`, `fmission-b` | each `IDENTICAL: 6001 ticks ... no difference` against `m3r1i-mission-a/run.hwo` (1ae66610...), all 21 fields, 1,476,370 B, ASan 0 |
| Mission01 sharp / cfg | `mission-sharp` (`/sharpTextures`, line `texLinearFiltering=0 (switch) switches=/noFilter`), `mission-cfg0` (`--cfg 'texLinearFiltering 0'`: start `(cfg)`, then `=1 (preset) [effects preset]`, saved row 1), `mission-cfg0c` (`effectsLevel 2` + row 0: sharp pass, saved 0) | all IDENTICAL, 6,001 ticks |
| Mission01 release pair | `fmission-rel-a`, `fmission-rel-b` (clean release build) | IDENTICAL against `m3r2i-e-mission-rel-a` |
| guard rails | `ai-a`, `skirmish-lfe2`, `missiontest` | IDENTICAL to `m3r2i-e-ai-a` (401 ticks), `m3r2i-e-skirmish-lfe2-a` (601), `m3r2i-e-missiontest-a` (11,426). `aitest ... checks=16/16 result=PASS`; `skirmishtest: done ticks=600 ships=91 result=PASS`; `missiontest: done mission=1 ticks=11424 objectives=6/6 result=PASS` |
| fe | `fe` | Main_game_screen, rc 124 at the timeout (as every fe run), ASan 0; settings line `texLinearFiltering=1 (default)` |
| hud | `hud` | `hudtest: done tick=599 boxed=19 selected=154 frames=537 statusbars=6840 infopanels=537 pie=537 result=PASS` |
| console | `console` | `consoletest: done ... shown=37 set=30 commands=8 keys=32 kas=6 closed=1 failures=0 result=PASS`, `tablerows=486` |
| fescreens | `fescreens` | `optionstest: done options=35 startdefaults=35 readback=35 result=PASS`; `fescreentest: done files=22 ... screens=195 pushed=195 popped=195 wrong=0 ... result=PASS` |
| textures | `textures` | `12403 listed ... 10661 registered in 3 batches, 10661 loaded, 0 not loaded` |
| the Video screens | `scr-fe` (`/catFeScreen Video_Options`), `scr-ig` (`Video@100`) | inspected as images: SHARP TEXTURES sits under UI SCALE (front end) and top middle above IN GAME BRIGHTNESS (in game); nothing overlaps; unticked by default |
| sp --timeout 1200 | `sp` | universe running, reaches `Mission12_NIS07.mission`, 12 animatics skipped, **ASan 0**, no unsuppressed leak, rc 124; one `texfilter:` line |

**play.sh header.** It documents the F1 key (and its yielding), the Menu key, SHARP TEXTURES (the Options control, the
`texLinearFiltering` row with its effectsLevel caveat, the `/sharpTextures` / `/noFilter` switch, the console row, the
`texfilter:` line) and the `preset` source. The fix of finding A is applied there.

**The user's check.** It names things that exist:
- the launcher: `tools/play.sh --rebuild`, an option the script parses;
- the missions and moments: Mission02 or later; the Kuun-Lan at its closest zoom;
- the controls: F1; Options -> Video -> SHARP TEXTURES, seen in both screen captures; TEXTURE MEMORY on the front end's
  screen (the in-game Video screen has none, as the capture shows); `-- /sharpTextures`.

## 4. The diff, read in full

- **Scope.** Only the producer's files are touched:
  - `src/` (14 files);
  - `tools/play.sh`'s header;
  - `docs/m3-r2-instrumentation.md`;
  - the two delta-map files;
  - the report.

  `tools/m2_harness.sh` was not edited. Nothing under original/, reference/ or build/ is in the tree.
- **Tags.** Counted with `/bin/grep` over `src/`:
  - `HWI-FEATURE(sharp-textures)` 196, `HWI-FEATURE(instrumentation)` 141 (was 102), `HWI-TEST` 318 (was 286).
  - `CAT-TODO` 37, unchanged. The one `CAT-TODO(M3-R3)` (BTG.c:1151) is M3-R2's, left for the integrate.
  - `CAT-FIX` +1 (render.c, the exe's Cancel). `CAT:` +1 (`trFilterEnable`'s mag filter).
  - The `HWI-FEATURE(sharp-textures)` lines in `MotionTest.c` tag test code with the feature name, as M3-R1 / M3-R2 did
    there (12 such lines at 7aa3111).
- **Stream safety.**
  - The new per-frame hooks do nothing unless a flag changes or a scripted input is given: `rndRenderTask`'s
    `trFilterSync` and `hwiFrameBegin`'s `hwiScriptFrame`.
  - `trFilterCount` only reads GL state and prints.
  - No simulation field moved in any stream (section 3). No stream or dump moved that the producer did not state.
- **Logic, checked by reading:**
  - The run-only switch saves the file's value through `utyOptionsFileWrite`'s `trFilterFileValues`, and outlives the HIGH
    preset through `trFilterPresetApplied`, which runs before `opEffectsHelper`'s `trFilterEnable`.
  - Each user choice ends the switch: SHARP TEXTURES, SURFACE FILTERING, the effects radio, or a frame-rate preset in
    `cat_opSetAllDefaults`.
  - The Cancel copies sit next to `savetexLinearFiltering`.
  - The count walk is the pass's walk: the nopal handles, the handle, or each palette.
  - `PlugScreen.c`'s save / restore of the flag is synchronous, so the per-frame sync cannot catch it half-way.

## Findings

**A (wrong claim, fixed): SHARP TEXTURES does not sharpen the effects.**
- **The claim.** The report says the option changes "the ship / asteroid / effect textures" (user's check, item 3) and the
  "registry's ... effect textures" (section (c)). The `/sharpTextures` help string (main.c) says "the ship and effect
  textures", and the play.sh header says "the ship, asteroid and effect textures".
- **What the code does.** The live pass does re-filter the particle textures (the read-back sees them on the new filter).
  But the particle renderers then set `GL_LINEAR` min and mag on the bound texture at every draw. In the tree:
  - Particle.c `partFilter(TRUE)` in `partRenderBillSystem` (lines 838, 878, 921);
  - Particle.c `partMeshMaterialPrepare` (line 975);
  - Trails.c:1152 `trailDrawBillboardedSquareThingz`;
  - Sentinel.c.
- **The exe does the same.** `partRenderBillSystem 0x004dc6f0` inlines 0x2801/0x2800 = 0x2601, and
  `partMeshMaterialPrepare 0x004dd480` pushes 0x2601 for 0x2801 and 0x2800 (disassembly at 0x4dd4df-0x4dd503).
  `partFilter 0x004dc4a0` is called from `trailDrawBillboardedSquareThingz 0x005a5be0` and
  `cat_sentinelFieldSegmentDraw 0x005c60a0`.
- **The consequence.** The ETG effect sprites, the trails' billboard sprites and the sentinel fields stay smooth in both
  settings, as with the exe's SURFACE FILTERING off. The behaviour is the exe's, so this is not a code fault. But the user
  would have been told to expect the effects to change.
- **Fixed, all marked `[audit: ...]`:**
  - the report's (c) paragraph and user's check item 3;
  - the play.sh header;
  - the help string. It is a text-only change; all builds were rebuilt with 0 warnings, and the final Mission01 pairs
    (debug and release) were rerun IDENTICAL on the rebuilt binaries.

**B (wrong citation, fixed).** The report's (c) cites "the front end's images (texreg.c:4638)". texreg.c:4638 is a path
split in the file loader. The GL_NEAREST image upload is `render.c:4638` in `rndLoadTarga`. Fixed in the report.

**Limits of the evidence (not faults).**
- **Accept in game.** The Options Accept re-filter is exercised in the front end, where the registry is empty
  (`textures=0`). The in-game count (2,343 / 2,343, wrong 0) is proven through the console row, which runs the same
  `trFilterSync` -> `trFilterEnable` that Accept calls (`opOptionsAcceptHelper`). No in-game Accept click was scripted.
- **Ship textures under the pass.** The read-back checks the textures right after the pass. Finding A is why draw-time
  state can differ, for the particle textures only.

**Observation outside this port (no finding against it).** The exe's `partRenderBillSystem 0x004dc6f0` ends with min and
mag `GL_NEAREST` on the last bound texture (decompile, before `rndAdditiveBlends(0)`); the tree's does not. Its
delta-map row (MODIFIED, reviewed) does not mention this. It is pre-existing and does not touch this port's streams. It is
for the owner of the particle code (the M3 plan's effects / ETG owner) to record in `delta_map_review.json`.

## Precision

Sampled checks of the producer's claims: 34. The 32 correct ones are:
- the F-key evidence (8);
- the keys and toggle runs (4);
- the exe's filtering, preset and Cancel (5);
- the capture pairs, counts and texel blocks (6);
- the dumps (3);
- the Mission01 streams (3);
- the guard rails and fence suites (2);
- the delta map and the tag counts (1).

The 2 wrong ones are findings A and B. **Precision = 32 / 34 = 0.94.**

## Fixes made by the audit (5 edits for 2 findings)

1. `src/SDL/main.c`: the `/sharpTextures` help string ("ship, derelict and asteroid textures").
2. `tools/play.sh` header: the textures sharpened, and the effects staying smooth `[audit: ...]`.
3. `docs/m3-r3-settings-and-keys.md` (c): the effects paragraph `[audit: ...]`.
4. `docs/m3-r3-settings-and-keys.md`, user's check item 3 `[audit: ...]`.
5. `docs/m3-r3-settings-and-keys.md` (c): the `render.c:4638 (rndLoadTarga)` citation `[audit: ...]`.

After the fixes: debug, release, win64 and the tree's `tools/build.sh --debug / --release` all give rc 0 with 0 warnings.

## The reference for the next port (the auditor's runs, final tree)

| stream / dump | run | content |
|---|---|---|
| Mission01 (debug) | m3r3a-settings-and-keys-fmission-a / -b | IDENTICAL to m3r1i-mission-a (1ae66610...) in all 21 fields, 6,001 ticks, 1,476,370 B; sha256 prefix 7dd5f0181bb56ce1 (this build identity) |
| Mission01 (release) | -fmission-rel-a / -b | IDENTICAL to m3r2i-e-mission-rel-a; 6c2550c34a89cf6f |
| ai 400 | -ai-a | IDENTICAL to m3r2i-e-ai-a; 128fade260ec7709 |
| skirmish LFE2 | -skirmish-lfe2 | IDENTICAL to m3r2i-e-skirmish-lfe2-a; 48a9a16e5e7c481d |
| missiontest Mission01 | -missiontest | IDENTICAL to m3r2i-e-missiontest-a (11,426 ticks), 6/6 at 11,424; b53671b14d0b9138 |
| Mission01 /sharpTextures, cfg rows | -mission-sharp, -mission-cfg0, -mission-cfg0c | IDENTICAL to the baseline |
| motion /noInterp | -motion-noi | c503c869f0dc9cdb / 3016280e9e0dd3c8 (1,436 lines), cmp = m3r2i-e-motion-noi |
| motion flags on | -motion-on | c0e411662fa2f527 / 428ef41479eb5749, cmp = m3r2i-e-motion-on |
| nis /noInterp | -nis-noi | 86cb155acffc180e / 196bab32d244f614 (1,489 lines), cmp = m3r2i-e-nis-noi |
| nis flags on | -nis-on | 0307f3be2044c29c / a5de13a44b8d264f, cmp = m3r2i-e-nis-on |
| opt-outs | -motion-optout / -nis-optout | 183681235a3fdc7b / 3016280e9e0dd3c8, 13ca70eea998e5de / 196bab32d244f614 |
| sharp (stated render change) | -motion-sharp / -nis-sharp | 1a2227d966ec9712 / 428ef41479eb5749, 871eac333fd603d0 / a5de13a44b8d264f |

Nothing moved; nothing for the integrate to re-baseline.

Verdict: PASS-WITH-FIXES - F1 overlay key, the switch-label rule and SHARP TEXTURES verified against the exe and every fence (streams IDENTICAL, dumps EQUAL, ASan 0); fixed the claim that the option sharpens effects (the particle renderers force GL_LINEAR, as the exe) in report, play.sh and help string, and one wrong line citation
