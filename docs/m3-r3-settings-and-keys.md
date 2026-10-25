<!-- done: a-fkey, b-toggle-lb, c-sharp-textures, d-captures, e-fences -->
# M3-R3 producer 1: the settings and keys (key settings-and-keys)

Base 7aa3111 (this work is `git diff 7aa3111` in the m2 worktree). Subsystems: **(a) fkey** (the overlay's F key),
**(b) toggle-lb** (the switch-label rule and the toggle self-test), **(c) sharp-textures** (the option), **(d) captures**
(the capture pairs and the Kuun-Lan's hull textures at 4K), **(e) fences**.

The answers file `cycle/answers/m3-r3-user.md` was read at the start (item 0's decision, the reports verbatim, the
launching session's findings for item 3, the mandate "a new option to support sharp texture meshes or the original texture
look", DEFAULT = the original look). Re-read before this report (unchanged since dae51e1, md5 ccdc5c26): no later
answer changes this task. One of its findings is corrected here: "`tools/play.sh -- /noFilter` or `texLinearFiltering 0` in
the cfg" would not have worked (the exe's start-up effects preset undid both; (c)).

## (a) The overlay's F key: F1 (HWI-FEATURE(instrumentation))

**The key is F1, not F12.** F12 is bound by cataclysm.exe in the game view.

What the exe does with the F keys (read from the exe and the install):
- **`default_keys.cfg`** (the install's, v 84, 200 lines; `original/game/default_keys.cfg` = the harness data copy, cmp
  equal) binds plain F2..F12 (VK 113..123): `Tactics_Evasive / _Neutral / _Aggresive` (F2-F4), `Formation_Delta / Broad /
  Delta3D / Claw / Wall / Sphere / Custom` (F5-F11), **`Formation_Parade 123` (F12, row 59)**. Plain F1 (112) has **no**
  binding; F1 is bound only with modifiers: `Leech_Spy SHIFT+112`, `ToggleSV_AreasVisited CTRL+112`, `Toggle_LOD_Drawing
  SHIFT+CTRL+112`. The user's own `original/play-home/.homeworld/keys.cfg` has the same F rows (read only).
- **`mrStartup 0x00563b50`** registers a key child for all 256 key ids (`regKeyChildAlloc(parent, ID, 0x180000,
  mrRegionProcess, 1, ID, ...)`, ID 0..0xff): every key goes to the key bindings (`mrRegionProcess 0x00567930` ->
  `cat_kbKeyPress 0x0054e7c0`). The tree's `src/SDL/mainrgn.c mrKeyFunction` table (F1-F11 at 180-183 / 198-205, the
  task's lead) is HW1's dead table: the tree's `mrStartup` uses `cat_mrKeyChildrenAlloc` like the exe.
- **`WindowProc 0x00561f10`**: WM_KEYDOWN gives every key to `keyPressDown` and `cat_consoleKeyPress`; WM_KEYUP
  special-cases only Escape (0x1b), **F11 (0x7a, swallowed)** and **F12 (0x7b: the Shift+Ctrl renderer reload)**.
- **The construction manager** (`cmBuildHotKey 0x00458d40`, tree `ConsMgr.c`): its detour takes every key in F1..F12
  (`0x70..0x7b`) while the screen is up; its table (`.data 0x00753ad0`) builds on F4 / F5 / F7 / F11 / F12 only, so F1
  builds nothing there.
- **The front end**: no `.fib` atom of the 22 files has an F key (or VK_APPS) as its hot key (a scan of every atom's
  `hotKey[5]` bytes; the keys used are A-Z, 1-3, Escape and 205).
- `decompiled.c` has no other key-code compare against 0x70 (the 0x70 hits are ship types and struct offsets).

**The binding.** `main.c HandleEvent` (SDL_KEYDOWN, repeat 0) toggles the overlay on `SDL_SCANCODE_F1`
(`Instrument.h HWI_OVERLAY_FKEY`, VK `HWI_OVERLAY_FKEY_VK` 0x70) while `KeyBindings.c hwiKbOverlayFKeyFree()` holds:
no Shift / Alt / Ctrl held (the modified F1 bindings stay the commands'), no binding of plain F1 in the table
(`hwiKbPlainBindings(0x70) == 0`), and the key-binding screen is not waiting for a key (`kbCommandBeingSet`). So F1
never takes a binding: if the user binds plain F1 on the KEY BINDINGS screen, F1 is that command's (the keys self-test
proves it). The key goes on to `keyPressDown` and the console as before (no binding: nothing happens; in the
construction manager the detour takes it and builds nothing, as in the exe). **The Menu key stays** as a second toggle
(unchanged; M3-R2).

**Keys self-test** (`/catKeysTest`, extended in `KeyBindings.c`): `m3r3-sk-keys`
- `keystest: overlay fkey F1 scancode=58 vk=0x70 plain bindings F1=0 F12=1 free=1 bound-free=0 reset-free=1`
  (step 5 of the test binds plain F1 to Retire, saves and reloads: the overlay key then yields; the reset frees it)
- `keystest: overlay key scancode=101 vk=0 bindings on VK_APPS=0` (the Menu key, unchanged)
- `keystest: done rows=197 known=149 unknown=48 commands=147 bindings=149 ... conflicts=1 shares=1 failures=0 result=PASS`
  (**197 / 149, as M3-R2**: the overlay key is not a key-binding command, so no row and no binding is added)

**The toggle self-test** presses F1 through `HandleEvent` (as SDL's loop does): `toggletest: overlay fkey F1 (scancode
58, vk 0x70, free 1) -> 0 line=yes` (source `key`, `[key overlay]`), then F1 again -> 1.

**The scripted run with captures** (`m3r3-sk-fkey`: `--mode ai --ai-ticks 260 --shot 140,160,210 -- /catOverlayKeyAt
150,200`, new HWI-TEST switch: F1 as SDL_KEYDOWN / SDL_KEYUP events to `HandleEvent` at those ticks; no OS input) against
its twin without the key (`m3r3-sk-fkey-off`), `tools/ppm_region.py diff`:

| tick | the log | diff against the twin |
|---|---|---|
| 140 | (before the key) | `differ=0` |
| 160 | `hwiscript: tick=150 key F1 (scancode 58) overlay=1` | `differ=14400 bbox=2,2,227,66 maxdiff=204`: only the overlay's box (M3-R2's box 2,2,253,66; this run's text is shorter: `SW none`). Read off the PPM: `FRAME 3.88 MS MED - P99 -` / `TICK 1.42 MS X1 ALPHA -1.000` / `OBJ 1 NIS 1 LBB 0 NOLOD 1 SW none` / `VERTS -` / `FULL DETAIL ON UI 100%` |
| 210 | `hwiscript: tick=200 key F1 ... overlay=0` | `differ=0` (the box gone) |

Files: `src/Game/Instrument.h` (the F1 constants and the reason), `src/SDL/main.c` (`HandleEvent`), `src/Game/KeyBindings.c`
/ `.h` (`hwiKbPlainBindings`, `hwiKbOverlayFKeyFree`, the self-test), `src/Game/MotionTest.c` (the toggle test),
`tools/play.sh` header (F1 first, the Menu key second, F1's yielding), `docs/m3-r2-instrumentation.md` (the key
paragraph: an M3-R3 bullet).

## (b) The toggle self-test under /letterboxBehind: the switch-label rule

**The rule (decided with the code).** A run-only switch over an options-file row (`/noInterp`, `/noNISInterp`,
`/letterboxBehind`, `/lodBands`, `/uiScale`, and now `/noFilter`) keeps standing when a console row changes the value:
the console changes the run's value only, and the switch still makes every save write the file's own row
(`MotionInterp.c miSwitchOff`'s comment: "The console rows change the run's flags only, as the switch does, while it
stands"; `render.c rndLetterboxFileValues`, `LOD.c catLodFileValues`, `uiscale.c uiScaleFileValues`). So the label
"switch" is the truth: it says the file is protected. This is the rule docs/m3-r2-instrumentation.md:415 records for
`/noInterp` (`objInterpolate=1 (switch)`, "the line reports the state faithfully"). A switch with **no** options-file
row (`/overlay`, `/meshImmediate`) has nothing to protect, and `Instrument.c hwiSettingsConsole` ends it on its console
row ("console"). So the fix is the first of the two: **the check accepts "(switch)" while `rndLetterboxSwitchStands()`**;
ending the switch on a console row would make the next save write the console's value into the file under a run-only
switch.

**Applied consistently.** `MotionTest.c mtToggleOverlay`'s letterbox check now expects `letterboxBehind=<v> (switch)`
while the switch stands, else `(console)`; and a new HWI-TEST `mtToggleSwitchLabels` checks the rule on every row that has
a console row and a switch: objInterpolate, nisInterpolate, letterboxBehind, noLOD, texLinearFiltering (row-backed:
`switch` while standing, the switch still standing after) and meshImmediate (row-less: `console`, the switch ended; the
test puts it back). uiScale's console row re-lays out the UI and is covered by the uiscale self-test; overlay by its own
checks above.

**The toggle self-test** (`--mode toggle`, quitting rc 0, ASan 0 each):

| run | switch | done line | the rule's lines (examples) |
|---|---|---|---|
| m3r3-sk-toggle | none | `toggletest: done start=1/1 fails=0 result=PASS` | every `switch label <row> switch=0 -> expect '<row>=<v> (console)' line=yes` |
| m3r3-sk-toggle-noInterp | /noInterp | `done start=0/0 fails=0 result=PASS` | `switch label objInterpolate switch=1 -> expect 'objInterpolate=0 (switch)' line=yes`, `nisInterpolate ... (switch)` |
| m3r3-sk-toggle-noNISInterp | /noNISInterp | `done start=1/0 fails=0 result=PASS` | `nisInterpolate ... (switch)` |
| m3r3-sk-toggle-letterboxBehind | /letterboxBehind | `done start=1/1 fails=0 result=PASS` | **`settings line console letterboxBehind -> 1 expect 'letterboxBehind=1 (switch)' line=yes`** (M3-R2's failing check) |
| m3r3-sk-toggle-lodBands | /lodBands | `done start=1/1 fails=0 result=PASS` | `switch label noLOD switch=1 -> expect 'noLOD=1 (switch)' line=yes` |
| m3r3-sk-toggle-overlay | /overlay | `done start=1/1 fails=0 result=PASS` | `overlay start=1 switch=1 (expected 1)` |
| m3r3-sk-toggle-meshImmediate | /meshImmediate | `done start=1/1 fails=0 result=PASS` | `switch label meshImmediate switch=1 -> expect 'meshImmediate=0 (console)' line=yes` |
| m3r3-sk-toggle-noFilter | /noFilter | `done start=1/1 fails=0 result=PASS` | `switch label texLinearFiltering switch=1 -> expect 'texLinearFiltering=0 (switch)' line=yes` |
| m3r3-sk-toggle-sharpTextures | /sharpTextures | `done start=1/1 fails=0 result=PASS` | as /noFilter |

(The run labelled m3r2i-e-toggle-lb in M3-R2 failed exactly "settings line: a console row's change prints it with source
console"; m3r3-sk-toggle-letterboxBehind is that run with this tree.)

## (c) SHARP TEXTURES (HWI-FEATURE(sharp-textures))

### The base: what cataclysm.exe does (read first)

- **The filters.** `trRGBTextureCreate 0x005866a0` (disassembly) and `trPalettedTextureCreate 0x00586500` set
  `GL_TEXTURE_WRAP_S/T = GL_REPEAT`, then, by `texLinearFiltering` (`.data 0x008bb69c`), **min and mag both `GL_LINEAR`**
  or **both `GL_NEAREST`**: no mip maps, no anisotropy (as HW1 `Win32/texreg.c:1443-1453`). `trFilterEnable 0x005888e0`
  re-filters the registry's live textures the same way (allocated, not pending, not shared; the handle, each palette's
  object, or the no-palette pool's objects), brackets the rGL device, clears the current texture and sets the flag (HW1
  `Win32/texreg.c:3370`). **The original look is GL_LINEAR magnification** (and GL_LINEAR minification without mip maps).
- **The exe already has the option**, under another name: Options -> Video -> **CUSTOM EFFECTS** (the `Options_New`
  screen, front_end.fib, linked from Video_Options' button `Video_Options-TO-Options_New`; in game `Custom_options`) ->
  **SURFACE FILTERING** (atom 9 `VO_Filter`, a checkbox, and atom 11 `VO_BitmapFilter`) -> `utyFilterToggle 0x005a86d0`
  / `utyFilterBitmapToggle 0x005a8740` -> `trFilterEnable`. The tree already ports both (identical, M2-R6).
- **The row and the switch.** `texLinearFiltering` is an options-file row (string `0x008c21d4`) and `/noFilter` a switch
  (`0x008b9714`, "disable bi-linear filtering of textures", an `entryVr`). **But the exe undoes both at every start**: the
  start's `opUpdateSettings 0x004d71d0` calls `opEffectsHelper 0x004d9b90`, whose HIGH preset (effectsLevel 0, the default
  and the user's row 1) sets `texLinearFiltering = 1` (LOW sets 0; CUSTOM leaves it), and the start's save then writes it.
  Proven on the base build: `m3r3-sk-base-cfg0` (`--cfg 'texLinearFiltering 0'`) and `m3r3-sk-base-nofilter` (`-- /noFilter`)
  both ended with `texLinearFiltering 1` saved; `m3r3-sk-base-cfg0c` (`effectsLevel 2` + `texLinearFiltering 0`) kept 0.
  **So the launching session's advice ("`-- /noFilter` or `texLinearFiltering 0` in the cfg") would not have worked.**
  The exe's Video screen also re-applies the preset each time it opens (`opEffects 0x004d9cc0`'s first call).
- **The exe covers the same set as this tree's option**: the creation flag reaches every texture made through
  `trRGBTextureCreate` / `trPalettedTextureCreate` / the no-palette upload while it is set (the registry's ship, derelict,
  asteroid, resource and effect textures, and anything else created through them then); the live pass reaches the
  registry only. Textures with their own filters keep them in both: the fonts and the mouse cursor (GL_NEAREST, font.c /
  mouse.c:262), the front end's images (render.c:4638, rndLoadTarga) [audit: said texreg.c:4638], the BTG (BTG.c:436), the movies (avi.c:189). `PlugScreen.c:365-398`
  forces 0 around its own quilt (the exe the same, decompiled.c 210177).
  [audit: the effects keep the smooth look in both settings. The live pass does re-filter the particle textures, but the
  particle renderers set GL_LINEAR min and mag on the bound texture at every draw: Particle.c `partFilter(TRUE)` in
  `partRenderBillSystem` and `partMeshMaterialPrepare`, and Trails.c `trailDrawBillboardedSquareThingz`, as the exe does
  (`partRenderBillSystem 0x004dc6f0` inline, `partMeshMaterialPrepare 0x004dd480` pushes 0x2601 for 0x2801 / 0x2800,
  `partFilter 0x004dc4a0` from `trailDrawBillboardedSquareThingz 0x005a5be0` and `cat_sentinelFieldSegmentDraw`).
  So SHARP TEXTURES, like the exe's SURFACE FILTERING off, sharpens the mesh textures (ships, derelicts, asteroids,
  resources) and not the ETG effect sprites, the trails' billboard sprites or the sentinel fields.]
- **The tree's difference (GoK, decision 28):** the creation adds `glGenerateMipmap`, `GL_LINEAR_MIPMAP_LINEAR` /
  `GL_NEAREST_MIPMAP_LINEAR` min filters and 8x anisotropy (texreg.c:1241-1251, 1320-1330, ~4055). Magnification (the
  close-up look the user reported) is the exe's in both settings; minification at a distance is GoK's (smoother than the
  exe's unfiltered minification). Not changed: porting the exe's minification moves every frame of every capture and dump;
  it is a look question for the user (below). A GoK fault fixed: `trFilterEnable`'s third branch set the min filter twice
  and left the mag filter linear (`CAT:` line; the exe sets min and mag on every branch).

### The option on top

| surface | what | default |
|---|---|---|
| Options -> Video **SHARP TEXTURES** (new checkbox, `OP_SharpTextures`, Options.c `opSharpTextures` / `opSharpTexturesAtomsAdd`, added in code from OP_NoLOD's box and the FULL DETAIL label as SMOOTH MOTION is; the .fib files untouched; registered in utility.c's callback table; `FEFlow.c feScreensImageLoad` adds it after UI SCALE) | ticked = sharp (`texLinearFiltering 0`). Front end: right column under UI SCALE (box 500,364, label 500,346 in 640x480 coordinates); in game: top middle (box 256,81, label 250,64). A click ends a `/noFilter` switch (the user's choice) and, where the HIGH / LOW preset no longer describes the flag, makes the effects level CUSTOM (as the exe's SURFACE FILTERING box does through `opEffectsToVideoAccept 0x004d88a0`), or the next start's preset would undo it. The textures change at the next frame (a live preview, as SURFACE FILTERING); **Accept** re-filters every live texture at once (`opOptionsAcceptHelper`: `trFilterSync`) and saves the rows; Cancel restores flag and effects level | unticked: the original look |
| Cataclysm.cfg row `texLinearFiltering` (the exe's / GoK's row; no second row) | 1 original, 0 sharp; holds by hand only with `effectsLevel 2` (the exe's start-up preset, kept: the settings line now says so: `texLinearFiltering=0 (cfg) [start]` then `texLinearFiltering=1 (preset) [effects preset]`, m3r3-sk-fe-cfg0) | 1 |
| switch `/noFilter` (the exe's) and alias `/sharpTextures` (main.c `NoFilterSet`) | **run-only** (texreg.c `trFilterSwitchSet`, as /noInterp: `utyOptionsFileWrite` writes the file's own row through `trFilterFileValues`) and **kept over the start's effects preset** (`trFilterPresetApplied` at the end of `opEffectsHelper`'s presets); a click on SHARP TEXTURES / SURFACE FILTERING / an effects or frame-rate preset ends it; Cancel restores its state (`trFilterSwitchStateSave / Restore` with the custom effects' copies) | off |
| console row `texLinearFiltering 0|1` (Console.c, `CAT_CON_HWI_ROW_ID + 7`, `CAT_CON_HWI_ROWS` 7 -> 8) | the run's value; the live textures follow at the next frame | |
| the settings line: row `texLinearFiltering` (Instrument.c `hwiSettings[]`, appended at index 7) | `texLinearFiltering=1 (default|cfg|preset)`, `=0 (switch)` with `switches=/noFilter`, `(console)`, `(options)` after a click, `(cfg)` after Accept; new source **`preset`** (`hwiSettingsPreset`, a preset that changed the value). The row's name is the options-file / console row's (the line's mechanism matches names), so the user can search the cfg for what the line says | |
| the log | `texfilter: filter=linear|sharp textures=N globjects=M wrong=W scaled=S pass=P` at every pass (texreg.c `trFilterCount`: the same walk as the pass, every GL object's min and mag filters read back, W must be 0; S = live textures the TEXTURE MEMORY fitting loaded smaller than on disk) | |
| the live sync (render.c `rndRenderTask`: `trFilterSync`) | a changed flag reaches the registry's live textures at the next frame (console row, a click, Cancel); `CAT-FIX`: the exe's Cancel restores the flag without a pass | |

**The sharp look**: magnification `GL_NEAREST` (each texel a square up close), minification `GL_NEAREST_MIPMAP_LINEAR`
(GoK's sharp path, kept). `GL_NEAREST_MIPMAP_NEAREST` would look a little crisper at a distance but pops between mip levels
as the camera zooms (a visible seam where the level changes, worst on the big hulls the camera glides over); the item is
the close-up look, which only the mag filter decides, so GoK's choice stays. The PACO UV inset (`meshFixupPacoUV`, the
exe's `meshRender 0x004b4280`: half a texel inward while filtering) follows the flag at every draw in both.

**Proof that every live texture is re-filtered** (the count and the read-back, in game):
- `m3r3-sk-cap-live` (ai scene, `-- /catConsoleAt 180:texLinearFiltering=0,220:texLinearFiltering=1`, new HWI-TEST switch:
  a console row at a tick through `cat_consoleRunCommand`): `texfilter: filter=sharp textures=2343 globjects=2343 wrong=0`,
  then `filter=linear textures=2343 globjects=2343 wrong=0`; its frame at tick 200 is **byte-equal** to `/sharpTextures`'
  (`differ=0`) and at 240 to the default's (`differ=0`).
- `m3r3-sk-mission-fkey` (Mission01): the same 2,343 / 2,343 / wrong=0 both ways; the stream IDENTICAL.
- the toggle self-test's Accept path (front end, where the registry is empty): `toggletest: sharp accept passes=1
  textures=0 globjects=0 wrong=0 row=0 rows=1 effectsLevel=2 line='... texLinearFiltering=0 (cfg) ... [options accept]'`.

**The toggle self-test's SHARP TEXTURES checks** (`mtToggleSharp`, all PASS in the nine runs of (b)): the start-up's save
wrote the file's row; the checkbox and label on Video_Options (`at 692,508 (label 692,490)` at 1024x768) and on the in-game
Video screen; the box shows the flag; a click to sharp sets 0 and the effects level CUSTOM (the OP_Effects radio shows it),
line `(options)`; Accept's steps re-filter (1 pass, wrong 0), write `texLinearFiltering 0` and `effectsLevel 2`, line
`(cfg) [options accept]`, read back; the next start's preset keeps a CUSTOM level's row; Cancel after a click restores it;
the HIGH preset sets 1 with line `texLinearFiltering=1 (preset) [effects preset]`; the switch is run-only (the save keeps
1) and outlives the HIGH preset; the start state back. Under `/noFilter`: `sharp start texLinearFiltering=0 switch=1 file=1
saved row=1` and the start line `texLinearFiltering=0 (switch) ... switches=/noFilter [start]`.

The Video screens with the new box (captured with the new HWI-TEST `/catFeScreen <screen>[@<tick>]`): `m3r3-sk-scr-fe`
(front end 1024x768), `m3r3-sk-scr-fe4k` (3840x2160, `/sharpTextures`: the box ticked, effects still HIGH), `m3r3-sk-scr-ig`
(the in-game Video screen pushed at tick 100). No control overlapped.

## (d) The capture pairs and the Kuun-Lan's hull textures at 4K

### Pairs (`tools/ppm_region.py diff`, same frame, original vs sharp)

| scene | runs | result |
|---|---|---|
| ai scene, fleet camera, 1024x768, tick 200 | m3r3-sk-cap-lin / -cap-sw (`/sharpTextures`) | `differ=2685 bbox=810,366,906,410 maxdiff=35` (only the hull of the ship in view; tick 240: 2,674 in 811,369,908,414) |
| the cfg row under CUSTOM | m3r3-sk-cap-cfg (`effectsLevel 2`, `texLinearFiltering 0`) | sharp; differs from the switch run by 445 px (160..901 x 352..438): the harness file has no effect rows, so CUSTOM leaves the effects at the code's values instead of HIGH's (not the textures) |
| the cfg row alone (the exe's preset) | m3r3-sk-cap-cfgh (`texLinearFiltering 0`, effectsLevel 0) | **`differ=0` against the default**: the row is undone at the start, as in the exe |
| the live console toggle | m3r3-sk-cap-live | tick 200 `differ=0` against the switch run, tick 240 `differ=0` against the default |
| ai scene, fleet camera, 3840x2160, tick 200 | m3r3-sk-cap4k-lin / -sw | `differ=20864 bbox=2758,1028,3027,1153 maxdiff=84` |
| Mission01 fleet zoom, 3840x2160 (`--lodtest 100:fleet`) | m3r3-sk-m14k-lin / -sw | tick 150 `differ=17781 bbox=878,545,2424,1500`; tick 300 `differ=17867`: the Kuun-Lan is small at this zoom (minification: the two look alike) |
| **the Kuun-Lan at its closest zoom** (skirmish LiveFireExercise2, new HWI-TEST scene kind `--scene mothership:2300`: the camera on player 0's sMothership at 2,300 units, its `minimumZoomDistance`, from the zenith), tick 100 | 1024x768: m3r3-sk-ku-lin / -sw | `differ=113372 bbox=279,0,766,768 maxdiff=123` |
| the same, 3840x2160 | m3r3-sk-ku4k-lin / -sw | `differ=890954 bbox=1265,0,2634,2160 maxdiff=125` |
| the same, 3840x2160, TEXTURE MEMORY HI (`TextureDetail 100`) | m3r3-sk-ku4k-lin100 / -sw100 | `differ=861115`; lin vs lin100 `differ=818385` (the full-size textures) |

**The geometry does not move**: the motion / nis dumps with `/sharpTextures` (m3r3-sk-motion-sharp, -nis-sharp) are
crc-stripped **equal** to the flags-on dumps (428ef41479eb5749 / a5de13a44b8d264f: 0 position lines differ); only `crc=`
moved, on 1,431 of 1,431 motion frames and 1,439 of 1,440 nis frames (whole 1a2227d966ec9712 / 871eac333fd603d0). Every
Mission01 stream with the option on is IDENTICAL (e).

### Why the Kuun-Lan looked "more blurry" at 4K (the user's 2026-10-25 report)

Nothing in the filtering changed since R9 (the old binary of the report has the same `GL_LINEAR` magnification as the exe).
Two things make the hull soft at 3840x2160, both the exe's behaviour:

1. **The hull textures are small for a 4K screen.** The Kuun-Lan's LOD-0 hull textures are 32x16 .. 256x128 (121 .lif
   files over its modules; the core's sides 256x32, its top 256x16). From the meshes (`.peo`, the UV area against the world
   area of each textured triangle), one texel covers **about 4-9 world units** (core sides 6.8, top 6.3, docking bay 5.3-7.7,
   engines 4.9-9.6, lower spine 4.1-9.0 per texture). The camera's vertical field of view is 75 degrees (camera.script,
   `rgluPerspective(fieldofview, aspect)`), so at a distance D a world unit is 2160 / (2 D tan 37.5) = 1407.5 / D pixels at
   2160 rows and 500.4 / D at 768. At the Kuun-Lan's closest zoom (D = 2,300 to its centre; the near hull is closer still)
   **a texel is about 4 pixels at 4K against 1.4 at 1024x768**; GL_LINEAR spreads each texel over that span. The sharp
   frame (m3r3-sk-ku4k-sw100) shows colour steps every 5-6 pixels on the hull (the median run between steps along rows and
   columns of a 180x180 hull patch).
2. **The TEXTURE MEMORY slider halves many of them.** The user's `TextureDetail 50` (the default; Video -> TEXTURE MEMORY
   in the middle) gives the exe's texture fitting a 6 MB budget (`cat_trTextureMemorySet` table: `catTrCramMB[50*11/100]` =
   6 MB; `trCramRAMComputeAndScale 0x00589a50` halves the largest scalable textures until the level fits). In Mission01
   that is **462 of the 2,343 live textures** loaded at half size on one axis or both, among them **35 of the Kuun-Lan's 58
   LOD-0 textures** (e.g. `SMO_DOCK_BTM 256x128 -> 128x128`, `SMO_DOCK_SIDES_1 128x128 -> 128x64`, `SMO_COM_FANCY 128x128 ->
   128x64`; m3r3-sk-m1-tl with the new HWI-TEST `/catTexList smothership`). With `TextureDetail 100` (the slider at HI: 12
   MB) **none** is shrunk (m3r3-sk-m1-tl100: `scaled=0`; 0 of 58). So along the halved axis a texel is ~8 pixels at 4K.

The hull crop (m3r3-sk-ku4k-*, 1560..1880 x 1450..1650 at 4K) shows the four looks: TEXTURE MEMORY middle / original (soft,
the half-size texels smeared), middle / sharp (8-pixel blocks, stretched on the halved axis), HI / original (the full
texture: the module's green lights and panel lines come back), HI / sharp (square ~5-6 px texels). The GPU memory budget
the slider was made for (1999) is no constraint now; whether the recreation should default to HI is the user's call (below).

## (e) The fences (every run on the plain harness; debug build unless noted)

**Builds** (final tree): `tools/build.sh --debug` rc 0, `--release` rc 0, **0 warnings** under -Werror each; the win64
cross build (`ninja -C build/win64`) rc 0, 0 warnings; no `_WIN32` code touched; `tools/m2_layout32.sh`: OK (no struct
changed).

| fence | run(s) | result |
|---|---|---|
| keys (197 / 149) | m3r3-sk-keys | `rows=197 known=149 unknown=48 commands=147 bindings=149 ... conflicts=1 shares=1 failures=0 result=PASS`; `overlay fkey F1 ... plain bindings F1=0 F12=1 free=1 bound-free=0 reset-free=1` |
| the F key toggles the overlay (PPMs) | m3r3-sk-fkey / -fkey-off | tick 140 differ 0; tick 160 the box (`differ=14400 bbox=2,2,227,66`); tick 210 differ 0 |
| toggle, plain and 8 switches | m3r3-sk-toggle, -toggle-{noInterp, noNISInterp, letterboxBehind, lodBands, overlay, meshImmediate, noFilter, sharpTextures} | all `result=PASS fails=0`, rc 0, ASan 0 (table in (b)) |
| the four dumps (+ opt-outs) | m3r3-sk-motion-noi / -motion-on / -nis-noi / -nis-on / -motion-optout / -nis-optout | **EQUAL whole and crc-stripped to M3-R2's / M3-R1's**: motion-noi c503c869f0dc9cdb / 3016280e9e0dd3c8 (1,436 lines, 1,431 frames), motion-on c0e411662fa2f527 / 428ef41479eb5749, nis-noi 86cb155acffc180e / 196bab32d244f614 (1,440 frames), nis-on 0307f3be2044c29c / a5de13a44b8d264f; the opt-outs 183681235a3fdc7b / 3016280e9e0dd3c8 and 13ca70eea998e5de / 196bab32d244f614 = R10's /noInterp dumps; motion-on and nis-on byte-equal (cmp) to m3r2i-e-motion-on / -nis-on |
| sharp dumps | m3r3-sk-motion-sharp, -nis-sharp | crc-stripped equal to flags-on; crc moved on 1,431 / 1,431 and 1,439 / 1,440 frames (a stated render change: the option) |
| Mission01 pair (6,000 ticks) | m3r3-sk-mission-a / -b; final tree m3r3-sk-fmission-a / -b | every one `IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference` against m3r1i-mission-a/run.hwo (1ae66610...), all 21 fields; 1,476,370 B |
| Mission01 sharp | m3r3-sk-mission-sharp (`/sharpTextures`), -mission-cfg0 (`effectsLevel 2`, `texLinearFiltering 0`), -mission-fkey (F1 at 100 / 200, the console row at 300 / 400) | all IDENTICAL, 6,001 ticks |
| Mission01 release pair | m3r3-sk-fmission-rel-a / -b (`--bin build/release/hwi-engine`) | IDENTICAL against m3r2i-e-mission-rel-a, 6,001 ticks |
| guard rails | m3r3-sk-ai-a (400 ticks), -skirmish-lfe2, -missiontest (Mission01 NIS-first) | IDENTICAL to m3r2i-e-ai-a (401 ticks), m3r2i-e-skirmish-lfe2-a (601), m3r2i-e-missiontest-a (11,426); `aitest ... checks=16/16 result=PASS`, `skirmishtest: done ticks=600 ships=91 result=PASS`, `missiontest: done mission=1 ticks=11424 objectives=6/6 result=PASS` |
| fe | m3r3-sk-fe | Main_game_screen, rc 124 at the 120 s timeout (as every fe run), ASan 0 |
| hud | m3r3-sk-hud | `hudtest: done tick=599 boxed=19 selected=154 frames=537 statusbars=6840 infopanels=537 pie=537 result=PASS` |
| console (36 / 29 / 486) | m3r3-sk-console | `consoletest: done ... shown=37 set=30 commands=8 keys=32 kas=6 closed=1 failures=0 result=PASS`, `tablerows=486` (+1 row: texLinearFiltering) |
| fescreens (35) | m3r3-sk-fescreens | `optionstest: done options=35 startdefaults=35 readback=35 result=PASS` (no new options-file row), `fescreentest: done files=22 ... screens=195 pushed=195 popped=195 wrong=0 ... result=PASS` |
| textures | m3r3-sk-textures | `12403 listed, 12403 found in listing, 1742 without a file to load, 10661 registered in 3 batches, 10661 loaded, 0 not loaded` (as M3-R2) |
| sp --timeout 1200 | m3r3-sk-sp | universe running, to Mission12 (FileLoads: Mission12_NIS07.mission; 12 animatics skipped, Animatic01 .. 12, as M3-R2's), **ASan 0**, no unsuppressed leak, rc 124 (the timeout); one `texfilter:` line (the start's preset pass, no other) |

**The final tree.** The runs above were made as the work went; the last code changes after them were HWI-TEST only (the
`scaled=` count, `/catTexList`, the scene kind `mothership`) and one comment (render.c's CAT-FIX note). Re-run on the final
debug build: m3r3-sk-f-motion-noi / -f-motion-on / -f-nis-noi / -f-nis-on **EQUAL** (c503c869f0dc9cdb / 3016280e9e0dd3c8,
c0e411662fa2f527 / 428ef41479eb5749, 86cb155acffc180e / 196bab32d244f614, 0307f3be2044c29c / a5de13a44b8d264f),
m3r3-sk-f-toggle and m3r3-sk-f-toggle-lb `result=PASS fails=0`, m3r3-sk-f-keys `rows=197 ... bindings=149 ... result=PASS`,
and the Mission01 pairs m3r3-sk-fmission-a / -b (debug, sha256 prefix **7dd5f0181bb56ce1**) and -rel-a / -b (release,
6c2550c34a89cf6f) IDENTICAL. (sp ran on the build before the comment.)

**Streams and dumps before and after: nothing moved.** Every stream above is IDENTICAL to M3-R2's exit run in all 21 fields
(rng_all included) and every default dump EQUAL whole and stripped; nothing for the integrate to re-baseline. The new code
draws from no random stream; the default (`texLinearFiltering 1`, effectsLevel HIGH) makes no texture pass beyond the exe's
own `opEffectsHelper` ones; the live sync and the F key do nothing unless the flag changes or F1 is pressed. The only stated
render change is the option itself (`crc=` with `/sharpTextures`, above).

**Plain-harness milestones and first stops** of the final runs: mission `oracle mission run done`, first stop none (rc 0);
toggle `toggle self-test done`, none; keys `keys self-test done`, none; sp `universe running`, none (still running at the 1,200 s timeout).

## Files changed

- `src/SDL/texreg.c` / `.h`: the `CAT:` mag filter in `trFilterEnable`; `trFilterCount` (the "texfilter:" line, the
  read-back, the scaled count, HWI-TEST `/catTexList`); `trFilterSync`; the run-only switch (`trFilterSwitchSet / End /
  Stands`, `trFilterFileValues`, `trFilterSwitchStateSave / Restore`, `trFilterPresetApplied`); accessors.
- `src/SDL/render.c`: `trFilterSync()` at the top of each render-task frame (HWI-FEATURE + the CAT-FIX note).
- `src/SDL/main.c`: `/noFilter` -> `NoFilterSet` (run-only), the alias `/sharpTextures`; the F1 key in `HandleEvent`;
  HWI-TEST `/catOverlayKeyAt`, `/catConsoleAt`, `/catFeScreen`, `/catTexList`.
- `src/SDL/utility.c`: `utyOptionsFileWrite`'s `trFilterFileValues`; `utyFilterToggle` / `utyFilterBitmapToggle` end the switch
  on a click; the `OP_SharpTextures` callback row.
- `src/Game/Options.c` / `.h`: `opEffectsHelper`'s preset hooks; `opEffects` (a click ends the switch); `cat_opSetAllDefaults`
  (a preset ends it); the Cancel copies; `opOptionsAcceptHelper`'s `trFilterSync`; the SHARP TEXTURES checkbox
  (`opSharpTextures`, `opSharpTexturesShow`, `opSharpTexturesAtomsAdd`); `opEffectsHelper` declared for the self-test.
- `src/Game/FEFlow.c`: `opSharpTexturesAtomsAdd(header)` after the UI SCALE buttons.
- `src/Game/Console.c`: the `texLinearFiltering` row, `CAT_CON_HWI_ROWS` 8.
- `src/Game/Instrument.c` / `.h`: the F1 constants and the reason; the settings row and the `preset` source
  (`hwiSettingsPreset`); the HWI-TEST script hooks (`hwiScriptFrame`: `/catOverlayKeyAt`, `/catConsoleAt`, `/catFeScreen`).
- `src/Game/KeyBindings.c` / `.h`: `hwiKbPlainBindings`, `hwiKbOverlayFKeyFree`, the keys self-test's F-key checks.
- `src/Game/MotionTest.c`: the toggle self-test (F1, the letterbox check's rule, `mtToggleSwitchLabels`, `mtToggleSharp`); the
  scene kind `mothership` (HWI-TEST).
- `tools/play.sh` (header: the settings line's `preset` source and the switch-label rule, the overlay's F1 key, the SHARP
  TEXTURES section); `docs/m3-r2-instrumentation.md` (the key paragraph); `ghidra/delta_map_review.json` and the merged
  `ghidra/delta_map.json`; this report.
- `tools/m2_harness.sh`: **not edited** (every new test input is an engine switch passed after `--`).

Tag counts (`/bin/grep -rn` over `src/`): **196** `HWI-FEATURE(sharp-textures)` lines (new); `HWI-FEATURE(instrumentation)`
102 -> 141; `HWI-TEST` 286 -> 318; one new `CAT:` line (`trFilterEnable`'s mag filter) and one `CAT-FIX` note (render.c,
the exe's Cancel without a pass). CAT-TODO count unchanged (37): none added, none closed; the one `CAT-TODO(M3-R3)` left
(BTG.c:1151, the star size) is the integrate's to re-target (the answers file does not decide it).

**Delta map** (`ghidra/delta_map_review.json`, merged with `python3 -I tools/delta_map.py merge`: "records changed 15"):
three new review rows, all IDENTICAL (compared with the exe and HW1): `trFilterEnable 0x005888e0`, `trRGBTextureCreate
0x005866a0`, `trPalettedTextureCreate 0x00586500` (the base of the option; the tree's GoK mip maps / anisotropy and the
mag-filter fix noted). M3-R3 notes appended to twelve reviewed rows edited or relied on: `opEffectsHelper 0x004d9b90`,
`opEffects 0x004d9cc0`, `utyFilterToggle 0x005a86d0`, `utyFilterBitmapToggle 0x005a8740`, `mrStartup 0x00563b50`,
`WindowProc 0x00561f10`, `cmBuildHotKey 0x00458d40`, `opOptionsSaveCustomEffectsSettings 0x004d8c40`,
`opOptionsAcceptHelper 0x004d76d0`, `utyOptionsFileWrite 0x005a84f0`, `meshRender 0x004b4280`, `kbListWindowCB 0x0049bca0`.
Counts after: IDENTICAL 2757, MODIFIED 1580, REWRITTEN 83, reviewed 3582.

## New and extended self-tests, switches and tools

- Keys self-test: the F-key line (above). Toggle self-test: F1, the switch-label rule (`toggletest: switch label ...`),
  SHARP TEXTURES (`toggletest: sharp ...`).
- Engine switches (HWI-FEATURE): `/sharpTextures` (alias of `/noFilter`, now run-only for both).
- Engine switches (HWI-TEST, pass after `--` to any harness mode): `/catOverlayKeyAt <tick>[,..]`, `/catConsoleAt
  <tick>:<row>=<value>[,..]`, `/catFeScreen <screen>[@<tick>]`, `/catTexList <substring>`; the `--scene` kind `mothership`.
- Log lines: `texfilter: ...` (every pass), `texlist: ...` (with /catTexList), `hwiscript: ...`.
- No new harness flag, no new tool script.

## THE USER'S CHECK (for the play-test brief)

1. **Start**: `tools/play.sh --rebuild`. The log's first `hwi-settings:` line now ends with `... meshImmediate=0 (default)
   texLinearFiltering=1 (cfg) fit=fullscreen-desktop ... 3840x2160 switches=none [start]`. Report it if it says anything
   else.
2. **The overlay's key, F1**: in the front end and in Mission02 or later, press **F1**: the dark panel appears top-left;
   F1 again hides it. The Menu key still works too. Check that Shift+F1 (Leech_Spy), Ctrl+F1 (the sensors' areas visited)
   and F2..F12 (tactics, formations; F12 = parade formation) still do what they did. Report: does F1 toggle it in the game,
   in the sensors manager, in the build screen; does it ever do anything else.
3. **SHARP TEXTURES, the original look first**: in a mission, zoom all the way in on the Kuun-Lan (its closest zoom) and look
   at the hull plates. This is the original look (smooth). Then press Escape -> Options -> Video and tick **SHARP
   TEXTURES** (top middle of that screen): the hull should change behind the menu (or at once when you close it with
   ACCEPT): each texel a square block (about 5-8 pixels at 4K). The EFFECTS row of the same screen switches to CUSTOM
   (that is how the original game keeps its own SURFACE FILTERING box: it is the same setting). Press ACCEPT. Report:
   which look you prefer; whether anything else on screen changed (it should not: only the ship / derelict / asteroid mesh
   textures [audit: said "ship / asteroid / effect": the effect sprites and the trails' billboard sprites stay smooth, as in the exe]; the HUD, fonts, menus and the starfield keep their own look); whether the log has `texfilter: filter=sharp
   textures=N globjects=N wrong=0` (wrong must be 0).
4. **The blur at 4K, the TEXTURE MEMORY slider**: still on Options -> Video (the front end's screen: the TEXTURE MEMORY slider
   bottom left, in the middle by default), slide it to **HI** and ACCEPT; restart the game (the textures are sized when a
   level loads). Look at the Kuun-Lan again with the original look: the hull should be noticeably less soft (the middle
   setting loads 35 of its 58 hull textures at half size: a 1999 memory budget). Report whether HI fixes the "more blurry"
   look for you, and whether you want HI as the recreation's default.
5. **A run-only check without the menu**: `tools/play.sh -- /sharpTextures` starts sharp for that run only (your
   Cataclysm.cfg keeps its rows); the log's line says `texLinearFiltering=0 (switch) ... switches=/noFilter`.

## For STATE.md

- **Decision (proposed): the overlay's F key is F1** (not F12: the exe's `default_keys.cfg` binds plain F12 to
  Formation_Parade and F2..F11 to tactics / formations; F1 is bound only with modifiers). F1 toggles the overlay in
  `main.c HandleEvent` while free (`KeyBindings.c hwiKbOverlayFKeyFree`: no modifier held, no binding of plain F1, the
  binding screen not waiting); the key still goes on to the game. The Menu key stays as a second toggle. Keys self-test
  197 / 149 unchanged, plus the F-key line. HWI-FEATURE(instrumentation).
- **Decision (proposed): the switch-label rule** of the settings line: a console row under a run-only switch over an
  options-file row (/noInterp, /noNISInterp, /letterboxBehind, /lodBands, /uiScale, /noFilter) keeps the switch standing and
  the label "switch" (the saves still keep the file's row); a switch with no options-file row (/overlay, /meshImmediate)
  ends on its console row ("console"). The toggle self-test checks it for every such row (PASS under all eight switches).
- **Decision (proposed): SHARP TEXTURES** (HWI-FEATURE(sharp-textures)): the exe's base is GL_LINEAR (original) or GL_NEAREST
  min and mag with no mip maps (`trRGBTextureCreate 0x005866a0`, `trFilterEnable 0x005888e0`), its own control SURFACE
  FILTERING on CUSTOM EFFECTS and its effects presets (`opEffectsHelper 0x004d9b90`, re-applied at every start). On top: the
  Options -> Video checkbox SHARP TEXTURES (`OP_SharpTextures`, Video_Options and the in-game Video screen; a click makes
  the effects level CUSTOM where the preset no longer describes it; Accept re-filters every live texture); the store is the
  existing row `texLinearFiltering` (1 original = **default**, 0 sharp; by hand it needs `effectsLevel 2`, as in the exe);
  `/noFilter` and the alias `/sharpTextures` are run-only and outlive the start's preset; the console row
  `texLinearFiltering` (HWI rows now 8: console shown 37 / set 30, tablerows 486); the settings-line row
  `texLinearFiltering` with the new source `preset`; the `texfilter:` log line (count and read-back, wrong=0). The sharp
  look is GL_NEAREST magnification with GoK's GL_NEAREST_MIPMAP_LINEAR minification. The tree keeps GoK's mip maps and 8x
  anisotropy for both settings (decision 28; a user look question). A CAT fix: `trFilterEnable`'s third branch now sets
  the mag filter; a CAT-FIX: the live textures follow the flag at the next frame (the exe's Cancel left them on the
  cancelled filter).
- **The counts**: keys 197 / 149; console shown 37 / set 30 / tablerows 486; fescreens options 35 (no new row); textures
  10,661 loaded / 0 not loaded.
- **Test inputs (HWI-TEST)**: `/catOverlayKeyAt`, `/catConsoleAt`, `/catFeScreen`, `/catTexList`, the `--scene` kind
  `mothership`.

## What is left, and its owner

- **The attended check** above (F1 in every screen, the sharp look, TEXTURE MEMORY HI at 4K). Owner: the play-test.
- **TEXTURE MEMORY's default** (TextureDetail 50: the exe's 6 MB fitting halves 462 of Mission01's 2,343 live textures,
  35 of the Kuun-Lan's 58 LOD-0 ones; 100 shrinks none): a default change moves every capture and dump with a shrunk
  texture in view. Owner: the user (question 1), then a fix round.
- **The minification**: the tree's GoK mip maps and anisotropy against the exe's unfiltered minification (a different
  look at a distance in both settings). Owner: the user (question 2); no port without a "the exe's".
- **The `CAT-TODO(M3-R3)` at BTG.c:1151** (the star size) is the M3-R3 integrate's to re-target, as its task says.

## Questions for the user

1. USER: the TEXTURE MEMORY slider (Options -> Video, front end) is in the middle by default (`TextureDetail 50`, your file
   too); the original game then loads 462 of Mission01's 2,343 textures at half size, 35 of the Kuun-Lan's 58 hull
   textures among them (a 1999 video-memory budget), which is most of the "more blurry" mothership at 4K. Should the
   recreation default it to HI (100: nothing shrunk), or keep the original game's middle and leave HI to you?
2. USER: at a distance, this engine smooths textures with mip maps and 8x anisotropic filtering (from the Gardens of Kadesh
   port); the original game drew distant textures without mip maps (crisper but shimmering when the camera moves). Keep
   the smoother distance look for both the original and the sharp setting, or should "the original texture look" be the
   original game's exact distance look too?
