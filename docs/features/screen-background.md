# Screen backgrounds: a picture you paint behind a menu screen

HWI-FEATURE(screen-background), with the Video screens' layout rework and the removal of the mod's SHARP TEXTURES
checkbox (the same round). Code: `src/Game/ScreenBackground.c` (whole file), hooks in `FEFlow.c feRegionsAdd`,
`main.c mainResetRender`, `render.c rndRenderTask`; the layout in `src/Game/Options.c`.

## The user's words (verbatim)
"Another thing I want to update is background for the video options menu in the main menu. Right now the UI scale and
smooth motion settings are put to the side where they are in the complete black. Please provide the background image so
I can edit it in gimp. also im noticing that the video settings menu in game is out of alignment in some areas and the
smooth motion toggle could be moved inline with the text to save on space. Also I now realize that the sharp textures
option does the same work as the original "surface filtering" option. So we can now remove that section of our mod since
it seems to be redundant"

## What cataclysm.exe does
- A front-end screen is its `.fib` atoms: procedural panels and boxes (`ferDrawBox*` from small corner and edge
  textures), decorative textures (`FA_DecorativeRegion`), text and controls, laid out on a 640x480 page that the front end
  centres at any resolution (`feResRepositionCentredX/Y`). There is no picture behind the Video screen.
- The space around a front-end screen's panel is masked with black "background" atoms (`FAF_Background`, plain
  rectangles stretched to the window's edges by `feResRescaleBackground`). Video_Options draws two of them (atoms 6 and 7)
  AFTER its panel (atom 5), over the panel's grey in the cut-out region right of it: that is the black the added
  controls sat in. The in-game screens have no fills: the game shows behind them.

## Design
- **The file**: `<Assets folder>/feman/backgrounds/<screen name>.png`, the Assets folder of
  `docs/features/assets-folder.md` (`assetsFolderPath`): `Documents/Homeworld Infection/Assets`, a visible folder
  (HWI-FEATURE(user-files), 2026-10-31; it was first the hidden settings folder's `Assets`); the user's development runs
  use the project's `Assets/`. The screen names are the `.fib`'s: `Video_Options`
  (Options -> Video in the main menu), `Video` (the Escape menu's Video screen); any other screen works the same way.
  Loaded with stb_image (any PNG stb reads; RGBA). This loader is self-contained: it does not go through the general
  `Assets/` override folder (File.c / texreg.c), which overrides the game's own files by their archive paths.
- **The mapping**: the picture is stretched over the screen's 640x480 layout area in logical coordinates (the area's
  origin is the centring the loader gave the screen's atoms, `x - loadedX` of the first atom that is not a stretched
  fill; 0 for the menus the front end does not centre). So it moves with the window and scales with the UI scale like
  the screen; any size maps onto the area, 1280x960 (2x) is pixel-sharp at 200% and smooth below (linear filtering with
  mip maps). Proven to the pixel at 100%, 150% and 200% (below).
- **Where it is drawn** (the screen's own order otherwise unchanged; only when the file exists):
  1. under the screen's base atom: the base region's draw (`sbgBaseDraw`) draws the picture over the layout area, then
     the base atom as before (`feBaseRegionDraw`; for the in-game screens that is the panel, the game behind);
  2. inside each black background fill (`sbgFillDraw`): the fill's black as before (`feStaticRectangleDraw`), then the
     picture clipped to the fill. A picture drawn only before the atoms would stay hidden in the front end, where the
     fills are drawn after the panel.
  Everything else (panels, boxes, decorative pieces, buttons, text) is drawn over the picture. Alpha blends normally
  (`GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`): transparent parts show the screen's own look (black in the front end, the
  game in the Escape menu).
- **Reload**: the file is looked at each time the screen opens and once a second while it is shown (modification time
  and size), so a new or re-exported picture shows without a restart and a deleted one goes back to the original look.
  The GL texture is dropped at a render reset (`mainResetRender`: a mode switch) and the file decoded again at the next
  draw. One "screenbg: <screen>: <path> WxH loaded" line per load ("... gone" / "... does not load (reason)").
- **Not changed**: without the file nothing is hooked. No options-file row, no switch, nothing in the simulation (the
  streams are identical).

## The picture to paint on (HWI-TEST `/catScreenBgExport`)
- `/catScreenBgExport <screen>` (and `/catScreenBgDir <dir>`, default the Assets folder's
  `feman/backgrounds`): once `<screen>` is the top screen (30 frames to settle), four frames are drawn and read
  back over the layout area at the physical scale: the screen's art only (its panels, boxes, decorative pieces and
  cut-outs; no buttons, text, sliders, lists or user regions: their regions' draws are switched off for two frames), then
  the whole screen, each once over black and once over white (the base region clears the area to the pass's colour, the
  fills draw it too). Each pixel's alpha is 255 - (white - black) and its colour black / alpha, so the panels' own soft
  edges keep their alpha and the space around them comes out transparent. Written: `<screen>-reference.png` (the art),
  `<screen>-controls.png` (the whole screen: a guide), `<screen>.ora` and `README.txt`; one "screenbgexport: ..
  result=PASS|FAIL" line; then the engine quits.
- `<screen>.ora` is OpenRaster (a zip GIMP opens with its layers): `mimetype` (first, stored), `stack.xml`, the layers
  "Controls guide (hide before exporting)" (hidden), "Your paint" (empty, transparent) and "Game art (reference)",
  `mergedimage.png`, `Thumbnails/thumbnail.png` (256x192). GIMP 3.2.4 (the user's flatpak) opens it with the three
  layers, names and visibility (checked headless: `gimp-console -i --batch-interpreter=plug-in-script-fu-eval`), and its
  export of the visible layers equals the reference to the pixel (paint empty).
- Run at 3840x2160 for the 2x files (the UI scale is 200% there):
  `tools/m2_harness.sh <label> --mode fe --res 3840x2160 --timeout 120 -- /catFeScreen Video_Options /catScreenBgExport Video_Options /catScreenBgDir <dir>`
  and for the in-game screen
  `tools/m2_harness.sh <label> --mode sp --res 3840x2160 --timeout 180 -- /catFeScreen Video@60 /catScreenBgExport Video /catScreenBgDir <dir>`.
- The harness's `--user-file SRC:REL` copies a file into a run's fresh user folder (every run passes
  `/userFilesPath <run>/user`), e.g. a test picture as `Assets/feman/backgrounds/Video_Options.png`; the summary prints
  the "screenbg" lines. (`--settings-file` copies into the settings folder, where the picture is no longer read.)
- The files for the user are game-derived (the game's own art): they live in the project's `Assets/feman/backgrounds/`
  (gitignored) and are never committed.

## How the user paints one (the README.txt next to the files)
1. Open `Video_Options.ora` in GIMP (File > Open): "Controls guide" (hidden; switch it on to see where the controls
   sit), "Your paint" (empty), "Game art (reference)".
2. Paint on "Your paint", around the panel: the game draws its own panels, buttons and text over the picture.
3. Hide "Controls guide", File > Export As..., name it `Video_Options.png` in that folder, Export, Export.
4. Start the game, or open the Video screen again. Delete the PNG to go back to the original look.

## The Video screens' layout (same round)
All positions are the files' 640x480 coordinates; the controls are added in code (`Options.c`), the `.fib` files stay
untouched.
- **SMOOTH MOTION** is one row now: the checkbox with its label 20 to the right and 2 down (the screens' radio labels'
  offsets: the 16-high glyph and the text centred on one line); the checkbox's click area spans the label; out of the
  tab order (it copied OP_NoLOD's tab stop before).
- **Front end (Video_Options)**: right of the panel, left-aligned with the AUDIO / VIDEO / GAMEPLAY / KEY BINDINGS tab
  buttons (x 502) on their 50-row pitch: "UI SCALE" at 502,251, its buttons at 502 / 562 on rows 270 / 289 (19 apart, as
  the EFFECTS radio buttons), SMOOTH MOTION at 502,316. Nothing else is in that space.
- **In game (Video)**: "UI SCALE" at 373,51 on the row of VIDEO SETTINGS (the space SHARP TEXTURES left and the top of
  the right column), its buttons at 379 / 449 on rows 70 / 89 (the EFFECTS column's x and pitch; the second row ends 14
  above the EFFECTS title instead of 4); SMOOTH MOTION at 379,306 under NUMBER OF EFFECTS, its row centred on TEXTURE
  INFO's. [Since the video-layout job (docs/features/video-layout.md) the in-game screen is laid out again: UI SCALE
  and SMOOTH MOTION one block under NUMBER OF EFFECTS, the right column 22 up, the sliders inside their boxes, FULL
  DETAIL on a row of its own, TEXTURE INFO centred; these are the positions `/exeVideoLayout` keeps.]
- The labels carry one string per language (`feStaticTextDraw` takes the strCurLanguage-th string of an atom's text).
- **The exe's own overlaps, not changed** [in game they are changed since, at the user's request:
  docs/features/video-layout.md; the front end keeps them]: on both Video screens "FULL DETAIL" sits on the MODEL DETAIL box's top border
  and the NUMBER OF EFFECTS knob at HI touches the box's right edge. Both come from the `.fib` and are drawn the same by
  cataclysm.exe: its text draw prints at the atom's y (`fontPrint(x, rect.y0, ..)` at 0x004735af, the exe's
  feStaticTextDraw at 0x004734f0; the label's y 215 / 293 is inside the box's frame, drawn about 8 above the rect at 226 /
  299), and its slider puts the knob at `value / max * (x1 - x0) + x0 - 4` (ferDrawHorizSlider 0x004789f0, 0x478b3c ..
  0x478b78) on a slider as wide as the box (in game 382 .. 520 = the box; front end 309 .. 448, the box 308 .. 446).
  `ferDrawBox*`, `feStaticRectangleDraw`, `ferDrawHorizSlider` and `ferDrawCheckbox` are IDENTICAL, reviewed rows of the
  delta map. No port fault was found on either screen.

## SHARP TEXTURES removed (same round)
The mod's checkbox (M3-R3, `docs/m3-r3-settings-and-keys.md` (c)) was cataclysm.exe's SURFACE FILTERING box the other
way round (Options -> Video -> CUSTOM EFFECTS, `VO_Filter`, `utyFilterToggle` -> `trFilterEnable`, the
`texLinearFiltering` row, the `/noFilter` switch). Removed: the checkbox and its label on both Video screens
(`opSharpTextures`, `opSharpTexturesShow`, `opSharpTexturesAtomsAdd`, the callback row `OP_SharpTextures`), the alias
switch `/sharpTextures`, the toggle self-test's checkbox steps. Kept, because they serve SURFACE FILTERING, `/noFilter`
and the row too:
- the live re-filter `trFilterSync` (render.c each frame, and at Accept): after CUSTOM EFFECTS' Cancel the exe restores
  the flag without a pass (the CAT-FIX), and the console row changes the flag directly;
- `trFilterEnable`'s CAT mag-filter fix (the exe sets min and mag on every branch);
- `/noFilter` run-only (the saves keep the file's row; `trFilterFileValues`), kept over the start's effects preset
  (`trFilterPresetApplied`), ended by a click on SURFACE FILTERING or an effects / frame-rate preset, restored by Cancel
  (`trFilterSwitchStateSave / Restore`): without it the exe's start-up preset undoes the switch;
- the console row `texLinearFiltering`, the settings line's row and its `preset` source (`hwiSettingsPreset`), the
  "texfilter:" log line, HWI-TEST `/catTexList`;
- the `HWI-FEATURE(sharp-textures)` tag stays on those lines (it names the texture-filter support now); texreg.c's
  comments still name `/sharpTextures` (texreg.c is another job's file this round: a comment-only follow-up).
The toggle self-test now drives the exe's box: CUSTOM EFFECTS pushed, a click on SURFACE FILTERING (the flag 0, one pass,
none wrong, the line's source "options"), its ACCEPT (the effects level CUSTOM, the Video screen's radio shows it), the
Video screen's Accept steps (the rows written and read back), its CANCEL after a click (the flag back, the next frame's
sync re-filters), the HIGH preset, the switch run-only, no SHARP TEXTURES control on either Video screen, the in-game
CUSTOM EFFECTS screen's box. HWI-TEST `/catFeClick <atom>[@<tick>]` (Instrument.c) clicks an atom of the top screen
through its callback, for the in-game check (`/catFeScreen Custom_options@60 /catFeClick VO_Filter@64`).
