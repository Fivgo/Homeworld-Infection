<!-- done: a-exe-read, b-core, c-setting, d-selftest-tools, e-fence -->
<!-- subsystems: a-exe-read, b-core, c-setting, d-selftest-tools, e-fence -->
# M3-R1 producer 5: the UI scale (200% at 4K, 150% at 1440p) and the window at 4K (ui-scale)

Base fdf35ee (`git diff fdf35ee`). The user's items 5 ("game UI is very small when at 4k. ensure that the ui scale is
increased by 200% on that mode and 150% on 2k resolution") and 6 ("the mouse can't reach the bottom of the screen when
in 4k. About the bottom 200 pixels are considered the boarder").

Subsystems: (a) exe-read: how cataclysm.exe lays out the front end and the HUD, and its windowed mode; (b) core: the
logical / physical split, the scale, the window fit, the mouse, every physical site; (c) setting:
HWI-FEATURE(ui-scale) on its surfaces; (d) self-test and tools: `/catUiScaleTest` (harness mode `uiscale`),
`/testUsableBounds`, `tools/ui_scale_compare.py`; (e) fence: captures, comparisons, streams, dumps, self-tests.

## (a) What cataclysm.exe does (export cataclysm_gog_r6, tools/m2_fdec.py, tools/m2_exedata.py)

* **One size.** The exe has no UI / render split: MAIN_WindowWidth / Height are the client size and the render size.
  Its 2D path normalises by them (`primTriOutline2` 0x0056fef0, `primLineThick2` 0x00571440, `mouseDraw` 0x0056ec50:
  `(x + 0.325) / MAIN_WindowWidth * 2 - 1`), so a 2D element is a fixed number of pixels: at 3840x2160 the 640x480 front
  end is a sixth of the screen's width. Nothing in the exe scales the UI; the scale is HWI-FEATURE(ui-scale) on top.
* **The front end's anchors.** `feAllScreensReposition` 0x00474ae0 centres every 640x480 screen:
  `atom->x = (MAIN_WindowWidth - 640) / 2 + atom->x`, `atom->y = (MAIN_WindowHeight - 480) / 2 + atom->y` (as the tree's
  `feResRepositionCentredX / Y`). **The HUD's anchors** are the screen's edges in MAIN_Window* units, e.g. the selection
  list at the top right, `x = MAIN_WindowWidth - 150` (`cat_moSelectedDraw` 0x004a6bb0, `DAT_0072ace4` = 150), the
  objectives / support boxes at `MAIN_WindowHeight - height`, the task bar at the bottom edge. The tree's ports use the
  same expressions, so with MAIN_Window* = the logical size the exe's layout holds at that size: nothing to port.
* **The windowed mode.** `WinMain` 0x005631f0: `fullScreen` clears `showBorder`; with a border the style is 0x06cf0008
  (WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN) and the window is
  `CreateWindowExA(.., 0, 0, W + 2 * SM_CXFRAME, H + SM_CYCAPTION + 2 * SM_CYFRAME, ..)`: the requested client size at
  the screen's origin plus the frame, whatever the desktop's size; without (`/noBorder` or fullscreen) WS_POPUP
  (0x80000000) of the client size. A request as large as the desktop is therefore partly off-screen in the exe too (the
  caption and frame push the client's bottom rows down); 2000-era desktops were never that small relative to the game.
  `mainRescaleMainWindow` 0x0055fc70 and `mainDestroyWindow` 0x00560350 recreate / resize it the same way.
* **The mouse.** `mousePositionSet` 0x0056e8d0 clamps the position to the client rectangle (`utyClientRectGet`, then
  0 .. width - 1 / height - 1) before `SetCursorPos`; GoK had commented the clamp out (the tree's mouse.c): ported (b).
* **GoK's split, which the scale builds on** (no second pair of size globals): `src/SDL/render.c` setupPixelFormat fills
  MAIN_WindowWidthActual / HeightActual from `SDL_GL_GetDrawableSize`; the GL viewport is never set by the game, so it
  stays the drawable; the 2D projection (`prim2d.h primScreenToGL*`, the font quads, the cursor, the animatics'
  `rgluOrtho2D(0, MAIN_WindowWidth, 0, MAIN_WindowHeight)`) maps 0 .. MAIN_Window* onto the whole viewport;
  `mouse.c mousePoll` scaled SDL's position by MAIN / Actual; `Clipper.c` scaled the projected points the same way.

## (b) The core (HWI-FEATURE(ui-scale), HWI-FIX for the window)

### The three sizes

| size | holds | where |
|---|---|---|
| the window **request** | the options file's `screenWidth` / `screenHeight`, the /640 .. /1600 switches, the Video screen's resolution list | MAIN_WindowWidth / Height until the window exists; afterwards `uiScaleWindowRequestGet` (the window's own request, kept in uiscale.c) |
| **physical** | the drawable (`SDL_GL_GetDrawableSize`) | MAIN_WindowWidthActual / HeightActual: the GL viewport, every glScissor box, every glReadPixels, the 3D projection's aspect, point sizes |
| **logical** | physical / scale, rounded | MAIN_WindowWidth / Height once the window exists: every 2D layout and draw, the mouse, picking and the 3D-to-2D projections |

The scale (`uiscale.c uiScaleChoose`): `uiScale` 0 = auto by the **physical height**: 200% at 2160 rows or more, 150% at
1440 or more, 100% below (the user's rule); else the setting's percent (100 .. 400). Capped so the logical size never
drops under the front end's 640x480 (a 1024x768 window at 200% gets 160%: logical 640x480).

**Rounding at 150%.** logical = round(physical * 100 / percent): 2560x1440 -> 1706.67 -> **1707 x 960**. The 2D projection
maps 0 .. 1707 onto the 2560-wide viewport, so it carries the exact per-axis factor physical / logical (1.49971 x 1.5,
printed in the "uiscale:" line); the mouse uses the same per-axis factor, so drawing and picking agree to the physical
pixel. Why rounded and not floored: the nearest integer keeps the factor closest to the asked 1.5 (1706 would give
1.50059); the 0.02% anisotropy is 0.5 physical pixel across the whole width.

**The mouse at a non-integer scale.** A window pixel belongs to the logical pixel its centre falls in, as the rasteriser
draws it: logical = floor((window + 0.5) * logical / window size) (`uiScaleMouseToLogical`; the identity at 100%). With a
plain truncation the self-test's click just right of a button at 2560x1440 landed one logical pixel inside it (window x
1435 -> 956.8 -> 956, while the button's edge column 957 covers window x 1435.2 .. 1436.7): fixed, the miss now misses.
The warp (`uiScaleLogicalToMouse`) goes to the logical pixel's centre.

**Fonts and the 640x480 art at 1.5.** Everything 2D is a textured quad through the projection: the glfont pages and the
cursor are GL_NEAREST (font.c), the front end's .lif art too (the captures prove it: below). At 2.0 every logical pixel is
an exact 2x2 block (the 4K frame box-downscaled equals the 1080p frame to the bit over the UI). At 1.5 nearest gives
columns 2, 1, 2, 1 .. physical pixels wide: crisp, every glyph the same shape, strokes alternating 1 / 2 pixels (the
1440p frame sampled at each logical pixel's centre equals the 1707x960 frame to the bit). **Chosen: nearest** (no change:
the filters the game already uses): linear would blur the 11-pixel bitmap fonts (HW_EuroseCond_11) and the art's 1-pixel
lines at every scale, including the exact 2.0. glDrawPixels (the font's bitmap fallback, never taken when the glfont page
exists) gets `glPixelZoom(physical / logical)`.

### The window at 4K (HWI-FIX, the user's item 6)

The user's options (play-home Cataclysm.cfg: screenWidth 3840, screenHeight 2160, fullScreen 0) made GoK's
`SDL_CreateWindow(SDL_WINDOWPOS_UNDEFINED, 3840, 2160, SDL_WINDOW_OPENGL)` a bordered 3840x2160 window on a 3840x2160
desktop (COSMIC, 150%): the title bar and the panel cover about 200 rows, the window's bottom rows sit below the screen,
and the pointer never reaches them. (The task's "RESIZABLE" is only GoK's Emscripten branch; the Linux window is not
resizable.) The fit (`uiscale.c uiScaleWindowFit`, called by setupPixelFormat before the window is made):

1. **the display**: display 0 when it is at least the request on both sides, else the smallest display that is, else the
   largest (`uiScaleFitDisplay`; the user's desktop has a 4K and a smaller monitor, and XWayland's display 0 can be the
   smaller one: a 3840x2160 request belongs on the 4K); the window is made there (`SDL_WINDOWPOS_UNDEFINED_DISPLAY(i)`);
2. **a request of the display's size or larger (both sides)** -> `SDL_WINDOW_FULLSCREEN_DESKTOP`: borderless, the
   display's size, above the panel. Chosen over shrinking because the whole display was asked for: a decorated window of
   that size can never be seen whole, and a shrunk one would change the physical size the user picked (and with it the
   UI scale's auto rule: 3840x1960 is under 2160 rows);
3. **larger than the usable bounds** (`SDL_GetDisplayUsableBounds`: the desktop less its panels) but smaller than the
   display -> a decorated window shrunk to the usable bounds keeping the request's aspect, then (`uiScaleWindowPlace`)
   shrunk again by its frame once the window manager reports it (`SDL_GetWindowBordersSize`) and placed inside them;
4. else the request, as before. `/fullscreen` keeps GoK's fullscreen-desktop; `/noBorder` now makes the window borderless
   (the exe's WS_POPUP; GoK ignored it); `/window` is the windowed mode the rule applies to.

The offscreen and dummy drivers (the harness) have no desktop: no fit, unless `/testUsableBounds WxH[@DWxDH]` forces the
usable bounds (and the display's size) for the run's own window. One line is logged per window (stdout, the play log):
`window: requested WxH, usable WxH (display N: WxH), created WxH (flags): rule`, then
`uiscale: physical WxH window WxH scale P% (auto|cfg|switch, uiScale N) logical WxH factor fx x fy`. The created size is
the drawable: the physical size the scale and the mouse use.

### The mouse: physical events, logical clamp

`mousePoll` (mouse.c): SDL's position (window coordinates) -> `uiScaleMouseToLogical` -> clamped to 0 .. MAIN_Window* - 1
(SDL reports positions outside the window while a button is held); the relative path (`mouseClipToRect`, the camera
drag) scales SDL's motion with the fraction carried to the next poll (a 1-pixel move at 200% was lost both ways before:
`+= 0.5` truncated); `mousePositionSet` clamps to the logical screen (CAT: the exe's clamp, 0x0056e8d0) and warps through
`uiScaleLogicalToMouse`. So the physical bottom row (y = 2159 at 4K) lands on the logical bottom row (1079), the corner
on (1919, 1079), and a held drag below the window on 1079 (the self-test's lines below).

### Every size, viewport, scissor and read-back site (classification)

GL calls (`grep glViewport glScissor glReadPixels glCopyTexImage SDL_GetWindowSize SDL_GL_GetDrawableSize` in src/SDL and
src/Game) and the size globals:

| site | class | after this round |
|---|---|---|
| render.c setupPixelFormat: SDL_CreateWindow size / position / flags | request -> physical | the fit's size, display and flags; `SDL_GL_GetDrawableSize` -> Actual (physical); `uiScaleWindowCreated` sets MAIN = logical; the "nothing changed" test compares the request |
| render.c rndInit `rndAspectRatio` | physical | the drawable's aspect (was the request's: equal at 100% and whenever request = drawable) |
| GL viewport (never set by the game; the drawable) | physical | unchanged |
| ShipView.c svShipViewRender `glViewport` (GoK: x MAIN -> viewport factor) | physical | unchanged (GoK already scaled it) |
| ShipView.c svShipViewRender `glScissor` (drawRect) | physical | `uiScaleScissor` (GoK had left it logical: wrong whenever logical != physical) |
| TradeMgr.c tmNumberRUsDraw, ConsMgr.c (3 RU / ship count boxes) `glScissor` | physical | `uiScaleScissor` |
| mainrgn.c mrRegionDraw `glScissor` (2 sites, the letterbox; dead in the exe and the tree: rndScissorEnabled is FALSE every frame) | physical | `uiScaleScissor` |
| MotionTest.c catMotionTestFrameEnd `glReadPixels` (the dumps' crc, /catMotionShot) | physical | MAIN_WindowWidthActual / HeightActual (equal at 1024x768: the dumps EQUAL) |
| MotionTest.c catShotFrameEnd (/catShot), screenshot.c | physical | unchanged (already `SDL_GL_GetDrawableSize` / Actual) |
| Particle.c glReadPixels (Emscripten only, a texture) | n/a | unchanged |
| Clipper.c (2 sites): projected points x logical / physical | logical | unchanged (GoK: the projection in logical units, as the mouse) |
| prim2d.h primScreenToGL* / primGLToScreen*, font.c, mouse.c mouseDraw, Animatic.c / avi.c ortho, render.c rndShamelessPlug, lens flares (CAT_LF_X) | logical | unchanged: the 2D projection; at a scale > 100% it is what makes the UI bigger |
| prim2d.c 2D line widths (8 thickness sites; primModeSet2's default) | logical | width x scale (`uiScaleLineWidth`; primModeSet2 / Clear2 set it only at a scale != 100%) |
| PiePlate.c piePointSpecDraw (the pie plate, its spokes, the move and height lines, drawn in 3D) and pieMovementCursorDraw's stipple | logical (HUD) | line width x scale, stipple factor round(scale) (100%: 1, the exe's) |
| render.c rndBackgroundRender star point sizes, BTG.c btgConvertStar background-star size | physical (3D view) | Actual (the exe's MAIN, which is its render size): the 3D view is the same at any UI scale |
| Select.c, PopupMenu.c, PiePlate.c, Sensors.c, InfoOverlay.c, Tutor.c, NIS.c / NIS.h (NIS_LetterHeight), Subtitle.c, TaskBar.c, Tracking.c, GamePick.c, KeyBindings.c, KAS.c, KASFunc.c, HorseRace.c, PlugScreen.c, FEFlow.c, Options.c layout, Animatic.c, avi.c, font.c, mainrgn.c layout, render.c 2D text (~380 MAIN_Window* uses) | logical | unchanged: the exe's own anchors at the logical size |
| Options.c opResChanged, the screen's start (opSaveMAIN_Window*), the mode switch; main.c mainSaveRender, mainSetupSoftware, mainRestoreRender | request | `uiScaleWindowRequest` / `uiScaleWindowRequestGet` (MAIN_Window* are logical once the window exists) |
| utility.c options rows `screenWidth` / `screenHeight` (-> MAIN_Window*) | request | `uiScaleWindowRows` around the write and the self-tests' read-back: the file keeps the request |
| render.c /catLodStats "size=" | physical | Actual (the measured cost) |
| SDL_GetWindowSize | window coordinates | uiscale.c: the mouse's denominator (= the drawable on X11 / Windows; points on a high-DPI Wayland / macOS window) |
| main.c mainStartupGL / mainStartupParticularRGL -> mainRescaleMainWindow (`utyChangeResolution(MAIN_Window*)`, before the new window exists) [audit: row missing] | request (a mode switch) | [audit: was laid out at the REQUEST on a mode switch (the Video screen's resolution list, mainRestoreRender): at 4K -> 2560x1440 the main region stayed 2560x1440 with the logical size 1707x960 and the main menu off-centre (OPTIONS at 1178,766 instead of 751,526; run m3r1-aus-proof-us-4k); fixed: uiscale.c uiScaleWindowCreated lays the front end out again at the logical size when a window replaces another] |
| main.c Emscripten resize branch, utility.c utyChangeResolution's Emscripten glViewport, render.c's HW_ENABLE_GLES SDL_CreateWindow, utility_mac.c [audit: the second and third were not listed] | n/a | not built (GoK's) |

### Files (src)

* **src/SDL/uiscale.c / .h** (new; HWI-FEATURE(ui-scale), HWI-FIX, HWI-TEST): the setting and its run-only switch, the
  scale's choice, the logical size, the window fit and display choice, the placement of a shrunk window, the apply and
  relayout (`uiScaleApply`: MAIN = logical, `utyChangeResolution`: the main region, every loaded front-end screen re-centred,
  the subtitle regions, the info overlay), the conversions, the test hooks.
* **src/SDL/render.c**: setupPixelFormat (the request, the fit, the created size, `uiScaleWindowCreated`), rndInit's
  aspect, the star point sizes, /catLodStats' size.
* **src/SDL/mouse.c**: mousePositionSet (CAT: the exe's clamp; the warp), mousePoll (the conversion, the clamp, the relative
  fraction, the test hook).
* **src/SDL/prim2d.c**: the 2D line widths. **src/SDL/mainrgn.c**, **src/Game/TradeMgr.c**, **ConsMgr.c**, **ShipView.c**:
  the scissors. **src/Game/PiePlate.c**: the pie plate's line width and stipple. **src/Game/BTG.c**: the physical size
  and `CAT-TODO(M3-R2)` (the exe's btgConvertStar has no resolution term). **src/Game/MotionTest.c**: the read-back.
* **src/SDL/main.c**: `/uiScale`, `/testUsableBounds`, `/catUiScaleTest`; mainSaveRender / mainSetupSoftware /
  mainRestoreRender on the request. **src/SDL/utility.c**: the `uiScale` row, the window rows around the write and the
  read-back, `OP_UiScale` in the callback table, the self-test's dispatch.
* **src/Game/Options.c / .h**: the UI SCALE control (`opUiScale`, `opUiScaleAddToScreen`, `opUiScaleAtomsAdd`), Cancel's
  copy, Accept's apply (`uiScaleUpdate` before the next screen is made), the request sites. **src/Game/FEFlow.c**: the
  atoms added at load. **src/Game/Console.c**: the console row.
* **src/Game/UiScaleTest.c / .h** (new; HWI-TEST): the self-test. **src/SDL/meson.build**, **src/Game/meson.build**.

## (c) The setting: HWI-FEATURE(ui-scale) `uiScale`

Default **0 = auto** (200% at a physical height of 2160 or more, 150% at 1440 or more, 100% below); 100 .. 400 a fixed
percent (the Video control offers 100 / 150 / 200). Never below a 640x480 logical screen.

| surface | where | behaviour |
|---|---|---|
| Cataclysm.cfg row | src/SDL/utility.c utyOptionsList, after `letterboxBehind`: `uiScale 0\|100\|150\|200` (scriptSetUdwordCB on `uiScale`) | read by utyOptionsFileRead before the window is made; written by every utyOptionsFileWrite (the file's own value under a switch: `uiScaleFileValues`) |
| switch | src/SDL/main.c `/uiScale N` (next to `/letterboxBehind`) | **run-only**, as /noInterp: the saves write the options file's value (`uiScaleSwitchSet` / `uiScaleSwitchEnd` / `uiScaleFileValue`); a click on the Video control ends it (the user's choice is saved as it is) |
| console row | src/Game/Console.c, HWI row `CAT_CON_HWI_ROW_ID + 4` (type 5, `uiScale`), CAT_CON_HWI_ROWS 4 -> 5 (the console self-test's shown / set 33 / 26 -> 34 / 27) | sets the setting (applied at the next start, or at a front-end Accept); not in the console's config file (HWI rows are left out); the retail console has no opener |
| Video-options control | src/Game/Options.c `opUiScale` + `opUiScaleAtomsAdd` (called by FEFlow.c feScreensImageLoad after `opSmoothMotionAtomsAdd`), callback `OP_UiScale` in utility.c's table | four radio buttons AUTO / 100% / 150% / 200% and the title "UI SCALE", copies of the screen's own OP_Effects radio button and "HIGH" label (the .fib files stay untouched): front end **Video_Options** (title 500,290; buttons 500 / 560 x rows 307 / 325, under SMOOTH MOTION) and in-game **Video** (title 373,64; buttons 379 / 459 x rows 81 / 99, right of IN GAME BRIGHTNESS) [audit: these are 640x480 positions, and the copies were placed AT them (x = loadedX) after the loader had centred the screen's own atoms, so at any logical size above 640x480 the control sat outside the panel (at 1920x1080 logical the 150% button at 500,325 instead of 1140,625; the self-test clicked the region wherever it was, so it passed); fixed in opUiScaleAddToScreen: the copied radio button's centring offset (x - loadedX) is added; the self-test now checks the exe's centring. The positions overlap no control of either screen (only the panels / the column's cut-out region: the audit's atom dump)]. Cancel restores it (`uiScaleStateSave / Restore` in opOptionsSaveSettings / opRestoreSavedSettings); **Accept in the front end applies it at once** (opOptionsAcceptHelper: `uiScaleUpdate` before the next screen's regions are made: the relayout of a resolution change) [audit: now proven by a click on the screen's own ACCEPT button inside the region processing; the producer's test called the steps one by one]; **in a game** it is saved and takes effect at the next start (the exe allows no render switch in game: opOptionsAcceptHelper returns while gameIsRunning) |
| tools/play.sh | header: the four surfaces, the default, the window fit and its log lines; `/uiScale N` among the switches | |

## (d) The self-test, the switches, the harness and the compare tool

* **`/catUiScaleTest`** (src/Game/UiScaleTest.c, HWI-TEST; harness **`--mode uiscale`**, documented in the script's header):
  once the front end is up at Main_game_screen, synchronously, with **no OS input**. The mouse is the engine's own path:
  `mousePoll` takes a window position from uiscale.c's test hook instead of `SDL_GetMouseState` (as SDL reports it, in
  window = physical coordinates on the offscreen driver); the buttons are `SDL_MOUSEBUTTONDOWN` / `UP` events handed to
  main.c `HandleEvent` (the function SDL's event loop calls; `keyPressDown(LMOUSE_BUTTON)` and the double-click timer), and
  the regions are processed as `regProcessTask` does (`mousePoll`, `regRegionProcess` on the root): pointer in
  (RPE_Enter), press (RPE_PressLeft), release (RPE_ReleaseLeft: the button's callback). Checks (42) [audit: 45 after the audit: + the UI SCALE buttons at the exe's centring, + the ACCEPT button's real click (replacing the call-by-call copy of its order), + 7. a mode switch's relayout at the logical size]:
  1. the scale and the logical size for the window (auto rule, the cap, the rounding);
  2. the fit display choice (`uiScaleFitDisplay`: a 1920x1080 display 0 and a 3840x2160 display 1: 3840x2160 -> 1,
     1920x1080 -> 0, 5120x2880 -> 1) and nine fit cases (`uiScaleWindowFitCheck`, "window: fit .." lines): the user's
     3840x2160 windowed request against 3840x1960 usable bounds and against 3840x2160 -> 3840x2160 fullscreen-desktop; the
     same with /noBorder and /fullscreen; 2560x1440 on the 4K desktop and 3840x2160 on a 5K one -> as requested;
     3840x2300 against 3840x2200 usable on a 3840x2400 display -> 3673x2200 window; 2400x1600 on a 2560x1440 display
     (2560x1400 usable) -> 2100x1400 window; 2560x1600 on it -> fullscreen-desktop;
  3. the mouse at the physical bottom row, the bottom-right corner, the top-left corner, 50 rows below the window and right
     of it: the logical edge, inside the screen, a region reaching the bottom edge under the bottom-row points;
  4. a click at the physical centre of Main_game_screen's OPTIONS button (OP_Options_Start: its callback opens the
     options, Audio_Options on top) and a click one logical pixel right of it (nothing); the button at the exe's centring
     at the logical size;
  5. the Video screens' control: the four buttons, labels and title on Video_Options (pushed) and on the in-game Video
     screen (loaded, not pushed: no game runs); the current setting's button checked; a scripted click on 150% sets 150,
     Cancel restores; a click on 200%, Accept's order (`uiScaleUpdate`, the screen gone, Main_game_screen made, the save):
     applied at once (capped at 640x480 logical), the main menu re-made at the new logical size, the saved
     `screenWidth` / `screenHeight` the window request, `uiScale 200` saved; the start scale back;
  6. the options file's `uiScale` row written and read back at 150 and 0 (scrambled first) with the window's logical size
     kept, `/uiScale` run-only (a save under it writes the file's 0), the console row (150, 0), the start state and row
     back.
  Lines `uiscaletest: ...`, `uiscaletest: done physical=.. logical=.. scale=.. checks=N fails=F result=PASS|FAIL`; the
  summary prints them and every mode's summary now prints the engine's `window: requested ..` and `uiscale: physical ..`
  lines.
* **`/testUsableBounds WxH[@DWxDH]`** (HWI-TEST, main.c): the usable bounds (and display size, default the usable's) the
  fit uses for the run's own window; the offscreen driver then makes a fullscreen-desktop window of ITS display mode
  (1024x768), so such a run checks the decision and the log line, not a 4K frame.
* **`/uiScale N`**: (c). **`--mode uiscale`**: tools/m2_harness.sh (header documented; milestone "uiscale self-test done").
  The size and the overrides go through the existing `--res WxH` and `--cfg 'uiScale N'` (producer 1's) or `-- /uiScale N`.
* **tools/ui_scale_compare.py** (python3 -I, standard library; reads PPMs through tools/ppm_region.py):
  `ui_scale_compare.py SCALED REFERENCE [--preset fe|hud] [--region NAME=x0,y0,x1,y1 ..] [--tol T] [--min-match F]
  [--sample area|centre] [--out DIR]`: the scaled frame reduced to the reference's size (area: the exact box average over
  each reference pixel's footprint, the 2x2 block at 200%; centre: the pixel at each reference pixel's centre, what a
  nearest-filtered scale draws there), compared over named regions in the reference's pixels; per region the pixel count,
  match= (the share within T of the largest channel difference, default 24), mean=, p99=, max=, PASS / FAIL against
  min-match (default 0.98); `--preset fe` = Main_game_screen's buttons, title, version label, whole panel and the cursor at
  the exe's centring; `--preset hud` = the hud self-test's info panel (left), selected / selecting lists (top right),
  support-units box (bottom right) and the cursor, anchored as the exe anchors them.

## (e) The fence (debug ASan build unless stated; runs in original/scratch-m2/m2r2-harness/runs/)

Two batches: `m3r1-ui-f-*` (the streams, dumps, missiontest pair, Tutorial1 and the self-tests) ran on the build before
the last source edits, which touch only the window fit's display choice (reached only with a desktop or
/testUsableBounds), /catLodStats' printed size and one comment; `m3r1-ui-g-*` (everything at other sizes, the captures,
the uiscale self-tests, the mission pair and the four dumps again, toggle, console, sp) ran on the final build. The base
captures `m3r1-ui-base-*` ran on fdf35ee's own build (producer 4's audited commit; producer 4's audit made no captures).
The front end draws the build identity at its bottom right ("Emergence 1 fdf35ee" against "fdf35ee+" for the
uncommitted tree): 211 pixels in that string's box differ in every front-end comparison with another build and are
excluded by the regions (as a .hwo header's build identity); every other pixel is compared.

### (a) and (b): the 4K and 1440p frames against the logical-size frames (`tools/ui_scale_compare.py`)

| comparison | runs | result |
|---|---|---|
| Main_game_screen, 3840x2160 auto 200% box-downscaled 2x vs 1920x1080 auto 100%, frame 800 (and 400) | `m3r1-ui-g-fe-4k` vs `m3r1-ui-g-fe-1080` | **tol 0: every region match=1.0000 mean 0 max 0** (fe-buttons 47,632 px, fe-title 77,355, fe-version-label 4,278, fe-panel 307,200, cursor 576): the 4K front end is the 1080p one pixel-doubled, to the bit |
| the in-game HUD (the hud self-test's scene), ticks 100 / 300 / 450, 4K vs 1080p, area 2x, tol 24 | `m3r1-ui-g-hud-4k` vs `-hud-1080` | **PASS** at every tick: info panel match 0.9996 (mean 0.04-0.09), lists 0.9999, support box 1.0000, cursor 1.0000 (the residue is the 3D view behind the translucent panel and lists, drawn at 4K in one and 1080p in the other) |
| the pie plate's lines (3D, HUD), tick 300, 4K vs 1080p | the same, regions pie-lines 480,20,620,560 / pie-plane 0,530,1920,580 | match 0.9900 / 0.9981 (the first build, lines 1 physical pixel wide: 0.9605 / 0.9577: why the pie plate's width follows the scale) |
| Main_game_screen, 2560x1440 auto 150% vs 1707x960 auto 100% (the nearest logical-size frame: the 150% frame's own logical size, drawn at 100%) | `m3r1-ui-g-fe-1440` vs `-fe-960` | **centre-sampled, tol 0: every region 1.0000 / max 0** (nearest filtering: each logical pixel's centre is its colour); area-averaged (the 1.5 box), tol 24: buttons 0.9340 (mean 6.08), title 0.9558, version 0.9294, panel 0.9743 (mean 2.63), cursor 0.9288: the expected mixing of glyph edges at a non-integer factor, not an error |
| the HUD, 1440p 150% vs 1707x960, centre-sampled, tol 24 | `m3r1-ui-g-hud-1440` vs `-hud-960` | PASS: ticks 100 / 300: info panel 0.9998, lists 1.0000, support 1.0000, cursor 1.0000; tick 450 (the sensors manager): 0.9954 / 0.9961 / 1.0000 / 0.9844 |
| 1024x768 (scale 100%) vs fdf35ee's own captures | `m3r1-ui-g-fe-1024` vs `m3r1-ui-base-fe` (frames 400, 800); `m3r1-ui-g-hud-1024` vs `m3r1-ui-base-hud` (ticks 100, 300, 450) | **EQUAL**: hud differ=0 over 1024x768 at all three ticks; fe differ=0 over rows 0-749, the 211 pixels of the build string (905..1021, 756..765) only |
| uiScale 100 at 4K (today's small UI) vs fdf35ee at 4K | `m3r1-ui-g-fe-4k-100` (`--cfg 'uiScale 100'`) vs `m3r1-ui-base-fe-4k` | **EQUAL** but the build string (differ=0 over rows 0-2139; 211 pixels at 3721..3837, 2148..2157) |

The overrides: `--cfg 'uiScale 100'` at 4K -> "scale 100% (cfg, uiScale 100) logical 3840x2160"; `-- /uiScale 150` at 4K
-> "scale 150% (switch, uiScale 150) logical 2560x1440" (`m3r1-ui-g-us-4k-cfg100`, `-us-4k-sw150`, both PASS); the
options-file row read back and the control's atoms on both screens: the self-test (d) at every size; fescreens
`optionstest: done options=35 startdefaults=35 readback=35 result=PASS` (the exe's Cataclysm option set: unchanged, uiScale
is an HWI row outside it), `fescreentest: done files=22 loaded=22 screens=195 pushed=195 popped=195 ... result=PASS`.

### (c) the scripted 4K click, the edges, the window fit

`m3r1-ui-g-us-4k` (3840x2160, auto 200%, logical 1920x1080): `uiscaletest: done ... checks=42 fails=0 result=PASS`;
* click just outside: window 2129,1205 -> logical 1064,602 (the button's x1; OPTIONS 858,586-1064,618 = 640x480
  218,286-424,318 centred) -> screen stays Main_game_screen; **click at the centre**: window 1923,1205 -> logical 961,602
  -> the button's callback opens the options (Audio_Options on top): **PASS / miss as required**;
* the physical bottom row: window 1920,2159 -> logical 960,1079 (a region 732,956-1226,1133 reaching the bottom edge);
  the bottom-right corner: 3839,2159 -> 1919,1079 (the main region, to 1920,1080); 50 rows below the window -> 1079;
  right of it -> 1919: the clamp never cuts a row off;
* the control: Video_Options buttons=4 title=yes; a click on 150% at logical 523,333 -> uiScale 150 checked; Cancel ->
  0; the in-game Video screen buttons=4 title=yes; the file rows 150 / 0 read back, the logical size kept; the switch
  run-only (row 0 under /uiScale 200); console 150 / 0.

At 2560x1440 (`-us-1440`, 150%, 1707x960): the miss window 1435,813 -> 957 (the edge column, outside), the hit 1281,813
-> 854,542 -> options: PASS; bottom row 1280,1439 -> 959, corner 2559,1439 -> 1706,959. At 1920x1080 / 1024x768
(`-us-1080`, `-us-1024`, 100%): PASS, and the live apply: Accept at 200% -> "uiscale: applied scale 200% .. logical
960x540 (was 1920x1080)", at 1024x768 capped "scale 160% .. logical 640x480", the main menu re-made there, then back.

**The window fit** (the exe's base: (a); the rule: (b)). The self-test's table (every run): the user's
3840x2160 windowed request against 3840x1960 usable bounds of a 3840x2160 display -> "3840x2160 (fullscreen-desktop):
display size -> fullscreen desktop"; against 3840x2160 usable -> the same; the display choice puts a 3840x2160 request on
the 4K display of a (1920x1080, 3840x2160) desktop. The forced runs on the real creation path:
`m3r1-ui-g-us-fit1960` (`-- /testUsableBounds 3840x1960@3840x2160`) logs
`window: requested 3840x2160, usable 3840x1960 (display 0: 3840x2160), created 1024x768 (fullscreen-desktop): display size -> fullscreen desktop`
and `-us-fit2160` (`3840x2160@3840x2160`) the same line with usable 3840x2160 (the offscreen driver gives its
fullscreen-desktop window its own display mode, 1024x768: the decision and the flags are the check; on the user's desktop
the created size is the display's 3840x2160). Without the switch the harness logs `usable none ... as requested (no
desktop bounds)` and every run is unchanged.

### (d) the streams and the dumps (hwo_compare, every field)

| stream / dump | run(s) | reference | result |
|---|---|---|---|
| headless Mission01, 6,000 ticks (pair, final build) | `m3r1-ui-g-mission-a` / `-b` (sha256 787e98865eb42c38, 1,476,370 B, 6,001 T) | r10i-mission-a (5bd51234405ea0d8) | **IDENTICAL in every field** (ticks 0-6000, rng_all included); a / b IDENTICAL |
| the same, release | `m3r1-ui-g-mission-rel` (034e6f849e4488f4) | r10i-mission-rel-a, r10i-mission-a | IDENTICAL |
| the same, the first build | `m3r1-ui-f-mission-a` / `-b` / `-rel` | r10i-mission-a / -rel-a | IDENTICAL |
| missiontest Mission01 (pair) | `m3r1-ui-f-m1-a` / `-b` | producer 4's audited `m3r1-amj-m1-a` | **IDENTICAL** (ticks 0-11425); 6/6, done 11,424, PASS |
| Tutorial1 leg | `m3r1-ui-f-tutorial` | `m3r1-fd3-tutorial` (the last tutorial run, producer 3; producer 4 ran none) and r10i-tutorial-a | IDENTICAL (ticks 0-10445); 12/12 lessons, PASS |
| motion /noInterp | `m3r1-ui-g-motion-noi` (and `-f-`) | `m3r1-amj-motion-noi` c503c869f0dc9cdb / nocrc 3016280e9e0dd3c8 | **EQUAL** byte for byte (cmp), whole and crc-stripped |
| motion flags on | `m3r1-ui-g-motion-on` | c0e411662fa2f527 / 428ef41479eb5749 | EQUAL |
| nis /noInterp | `m3r1-ui-g-nis-noi` | 86cb155acffc180e / 196bab32d244f614 | EQUAL |
| nis flags on | `m3r1-ui-g-nis-on` | 0307f3be2044c29c / a5de13a44b8d264f | EQUAL |

**No stream and no dump moved in any field or column**: nothing to re-baseline.

### The plain-harness runs (label, milestone, first stop)

| label | mode | milestone | first stop | result |
|---|---|---|---|---|
| m3r1-ui-g-us-{4k,1440,1080,1024,fit1960,fit2160,4k-cfg100,4k-sw150} | uiscale | uiscale self-test done | none (rc=0) | 42/42 PASS each, ASan 0 |
| m3r1-ui-g-fe-{1080,4k,1440,960,1024,4k-100} | fe, 120 s | Main_game_screen | none: running at the 120 s timeout (as every fe run) | ASan 0 (the 4K fe run: ASan 0) |
| m3r1-ui-g-hud-{1080,4k,1440,960,1024} | hud | hud self-test done | none (rc=0) | `done tick=599 boxed=19 selected=154 frames=537 statusbars=6840 infopanels=537 pie=537 result=PASS` at every size, ASan 0 |
| m3r1-ui-f-ui | ui | ui self-test done | none (rc=0) | `done selected=154 rows=10 popupframes=16 commands=26 targetrows=1 statsframes=8 result=PASS`, ASan 0 |
| m3r1-ui-f-fescreens | fescreens | fescreens self-test done | none (rc=0) | 22 files / 195 screens PASS; optionstest 35/35 PASS; ASan 0 |
| m3r1-ui-f-console, m3r1-ui-g-console | console | console self-test done | none (rc=0) | `shown=34 set=27 ... failures=0 result=PASS` (33 / 26 + the uiScale row), ASan 0 |
| m3r1-ui-f-keys | keys | keys self-test done | none (rc=0) | `rows=197 ... failures=0 result=PASS`, ASan 0 |
| m3r1-ui-f-strings | strings | strings self-test done | none (rc=0) | `ids=1705 ... result=PASS`, ASan 0 |
| m3r1-ui-f-toggle, m3r1-ui-g-toggle | toggle | toggle self-test done | none (rc=0) | `done start=1/1 fails=0 result=PASS`, ASan 0 |
| m3r1-ui-{f,g}-mission-*, -f-m1-*, -f-tutorial, -{f,g}-motion-*, -{f,g}-nis-* | (d) | as (d) | none (rc=0) | ASan 0 |
| m3r1-ui-g-sp | sp, 1,200 s | universe running | none: still running at the 1,200 s timeout (as every sp run) | ASan 0, 12 animatics skipped (Animatic01 .. 12), last screenshot golden-12-00464 (producer 4's m3r1-mj-sp: golden-12-00576) |
| m3r1-ui-g-sp-4k | sp at 3840x2160 (200%), 1,200 s | universe running | **AddressSanitizer:DEADLYSIGNAL in the shutdown after the timeout's SIGTERM** (rc 124; the last line before it the quit's memory analysis of the growth heaps, no stack: the harness's SIGKILL 5 s later) | ASan 1; Mission01 .. Mission11 (golden-11-01472), 12 animatics skipped |
| m3r1-ui-h-sp4k-b | the same again, 1,200 s | universe running | none: at the 1,200 s timeout | **ASan 0**, the same last screenshot golden-11-01472 |
| m3r1-ui-h-sp4k-1300 | the same, 1,300 s | universe running | none: at the 1,300 s timeout | **ASan 0**, Mission12 (golden-12-00800) |
| m3r1-ui-h-base-sp4k | fdf35ee's own build at 3840x2160 (100%: the user's small UI), 1,200 s | universe running | none | ASan 0 (golden-11-01360) |
| m3r1-ui-h-sp4k-300 / -base-sp4k-300 / -sp4k100-300 | 300 s, final / fdf35ee / final at uiScale 100 | universe running | none | ASan 0 each |

The one 4K sp signal did not come back in three more 4K runs of the final build (two of them through the same moment,
golden-11-01472, and the same timeout) nor in fdf35ee's: it struck in the quit path the harness's timeout starts
mid-level (SDL_QUIT from SIGTERM: etgShutdown, trShutdown, titanShutdown, taskClose, then the debug memory analysis), not
in a frame, and no UI-scale code runs there. Stated as an intermittent shutdown fault under a timeout kill, owner M3 soak
(its headless hour and the growth heaps' analysis, docs/m2-r10-summary.md "Plan for M3" step 2); the ASan-0 gate for the
4K sp is met by m3r1-ui-h-sp4k-b and -1300, the 4K fe by m3r1-ui-g-fe-4k.

Builds: `tools/build.sh --debug` and `--release` green, 0 warnings under -Werror, after every subsystem and at the end;
`tools/m2_layout32.sh` OK (no struct touched).

## Delta-map review entries (sample `port-M3-R1-ui-scale`, notes `[review: M3-R1 ui-scale]`)

[audit: the four IDENTICAL -> MODIFIED rows below are reverted: the class compares the exe with HW1's source, and in all four the exe's body equals HW1's (Win32/main.c:2354 / :2365, Win32/mouse.c:457 with MOUSE_CURSOR_CLAMP, Game/btg.c:776: `tools/delta_map.py show`); the differences are GoK's tree, recorded in the notes. After the audit's merge: IDENTICAL 2,762, MODIFIED 1,575 (as at fdf35ee); notes added to the edited IDENTICAL bodies the producer left without one: cmNumberRUsDraw 0x004578e0, tmNumberRUsDraw 0x0052ded0, primRoundRectOutline 0x005718b0, setupPixelFormat 0x00574550.]
`python3 -I tools/delta_map.py merge --done m3r1:ui-scale`: records changed 13; IDENTICAL 2,762 -> 2,758, MODIFIED
1,575 -> 1,579, REWRITTEN 83; `report` rewritten (docs/m2-r1-function-deltas.md).
* **IDENTICAL -> MODIFIED**: 0x0056e8d0 `mousePositionSet` (the exe's clamp to the client rectangle; GoK had removed it:
  **ported**, CAT); 0x00428c20 `btgConvertStar` (GoK's 640 / W term; the exe's half size is width * 0.5 at 0x0072a480 with
  no resolution term: not ported, `CAT-TODO(M3-R2)`); 0x005604e0 `mainSaveRender` and 0x00560550 `mainSetupSoftware`
  (the exe's renderer names / software 640x480x16; GoK's SDL layer: decision 28).
* **compared, IDENTICAL stands** (notes): 0x00574b90 `rndBackgroundRender` (the point sizes on the width: the exe's
  MAIN_WindowWidth is its render size, the tree's Actual), 0x0056fd10 / 0x0056fd90 `primModeSet / ClearFunction2`, the
  thickness lines 0x0056fef0 `primTriOutline2`, 0x00570ac0 `primRectOutline2`, 0x00570f00 `primOvalArcOutline2`,
  0x00571440 `primLineThick2`, 0x00571500 `primLineLoopStart2` (glPushAttrib(GL_LINE_BIT) / glLineWidth(thickness) /
  glPopAttrib against GoK's read-and-restore: the same; decisions 28 / 29 otherwise), and relied on: 0x00474ae0
  `feAllScreensReposition` (the centring at (MAIN - 640) / 2).
* Read and found as the tree (no row): `cat_moSelectedDraw` 0x004a6bb0 (MAIN_WindowWidth - 150; Cataclysm-new, no HW1
  row), `mouseDraw` 0x0056ec50 (the cursor through (x + 0.325) / MAIN: decision 29), `tmNumberRUsDraw` 0x0052ded0 and
  `svShipViewRender` 0x00515010 (the scissor boxes in MAIN units: the exe's one size; the tree's physical conversion is the
  feature), `WinMain` 0x005631f0 / `mainRescaleMainWindow` 0x0055fc70 / `mainDestroyWindow` 0x00560350 (the windowed mode).

## CAT-TODOs

One added, none closed: `CAT-TODO(M3-R2)` in src/Game/BTG.c (btgConvertStar's resolution term: the exe's background
star is a fixed share of the screen, GoK's a fixed pixel size; porting it moves every frame with a BTG star, so it waits
for the fix round's look review). 27 -> 28 tags; the 23 stage-5 sites untouched; `/bin/grep -rn "M3-R1" src |
/bin/grep -c "CAT-TODO\|CAT-PROVISIONAL"` = 0. HWI-FEATURE(ui-scale) tags: 0 -> 290 lines.

## THE USER'S CHECK (for the play-test brief)

1. `tools/play.sh --rebuild` (release; your play-home as it is: screenWidth 3840, screenHeight 2160, fullScreen 0). The
   window should now cover the whole 4K monitor with no title bar, over the panel (borderless "fullscreen desktop": the
   whole display was asked for). The log (original/play-home/logs/play-*.log) shows
   `window: requested 3840x2160, usable WxH (display N: 3840x2160), created 3840x2160 (fullscreen-desktop): display size -> fullscreen desktop`
   and `uiscale: physical 3840x2160 ... scale 200% (auto, uiScale 0) logical 1920x1080`. Check it opens on the 4K monitor.
2. **The mouse at the edges** (item 6): in the main menu move the pointer to the very bottom, the bottom-left and
   bottom-right corners and the right edge: the cursor must reach the last row / column. In a game, push the pointer to the
   bottom edge: the task bar must pop up, and its buttons must be clickable.
3. **The UI size** (item 5): the main menu should be twice the size it was (1280x960 pixels in the middle of the screen
   instead of 640x480), the text sharp (pixel-doubled). New Game -> Mission01: the HUD text (the ship lists top right, the
   info panel, the objectives, the RU / support box bottom right), the subtitles and the letterbox of the opening cutscene,
   a right-click menu, the build / research / launch screens, the Escape menu, the movement disc and its lines should all be
   twice their old size and placed as at 1080p. Click small things (the menu's buttons, list entries, the movement disc's
   height) to see that clicks land where the pointer is.
4. **The control**: Options -> Video (front end): UI SCALE AUTO / 100% / 150% / 200%. Pick 150%, Accept: the menu
   redraws at once at 1.5x (a 2560x1440 look); 100%: the old small UI; back to AUTO. In a game (Escape -> Options ->
   Video) a change is saved and takes effect at the next start. One-off: `tools/play.sh -- /uiScale 150` (the options
   file keeps its row).
5. **Report**: whether 200% at 4K (and 150% for 2K, if you have a 1440p screen or try /uiScale 150) is the size you want;
   anything 2D that stayed small or moved to a wrong place (where, which screen); any click that misses; whether the
   window is on the right monitor and whether switching to the other monitor (Alt-Tab, the mouse) minimises it or behaves
   badly (fullscreen-desktop windows can minimise on focus loss); whether you prefer a decorated window (then pick a
   smaller resolution in Options -> Video [audit: that mode switch laid the menu out at the request until the audit's fix; GoK's mainDestroyWindow also makes the next quit's first SDL_QUIT be ignored (main.c mainActuallyQuit; a harness run that switched mode ended rc 1 without the normal shutdown): report whether Quit needs a second try after a resolution change; owner M3-R2] or in the options file's screenWidth / screenHeight: a request that fits
   the desktop's usable area stays a decorated window).

## For STATE.md

* Setting `uiScale` (HWI-FEATURE(ui-scale)): Cataclysm.cfg row `uiScale 0|100..400`, default 0 = auto (200 at a
  physical height >= 2160, 150 at >= 1440, 100 below; capped at a 640x480 logical screen); switch `/uiScale N` run-only;
  console row `uiScale` (HWI row id 0x1004, CAT_CON_HWI_ROWS 5); Video-options control "UI SCALE" (AUTO / 100% / 150% /
  200%, `OP_UiScale` radio buttons copied from OP_Effects / "HIGH" on Video_Options and the in-game Video screen); applies
  at once at a front-end Accept, at the next start from a game.
* Names: src/SDL/uiscale.c / .h (`uiScale`, `uiScalePercent`, `uiScaleWindowFit`, `uiScaleFitDisplay`,
  `uiScaleWindowPlace`, `uiScaleWindowCreated`, `uiScaleWindowCurrent`, `uiScaleWindowRequest` / `RequestGet`,
  `uiScaleWindowRows`, `uiScaleFileValues`, `uiScaleApply`, `uiScaleUpdate`, `uiScaleMouseToLogical` /
  `LogicalToMouse` / `RelativeToLogical`, `uiScaleToPhysicalX / Y`, `uiScaleScissor`, `uiScaleLineWidth`);
  Options.c `opUiScale`, `opUiScaleAtomsAdd`; Game/UiScaleTest.c `/catUiScaleTest` (harness `--mode uiscale`);
  `/testUsableBounds WxH[@DWxDH]`; tools/ui_scale_compare.py.
* Decisions (proposed): MAIN_WindowWidth / Height = the logical size once the window exists, MAIN_WindowWidthActual /
  HeightActual the physical (no new size globals; the window request lives in uiscale.c and the options file's
  screenWidth / screenHeight keep it); logical = round(physical / scale), the projection carrying the exact per-axis
  factor; the mouse maps a window pixel by its centre; nearest filtering at every scale (the exact pixel-doubling at 2.0);
  the 3D view (aspect, point sizes, BTG stars) is the physical size's at any UI scale, the HUD's 3D lines (the pie plate)
  follow the scale; HWI-FIX window fit: a windowed request of the display's size -> fullscreen-desktop on that display,
  larger than the usable bounds -> shrunk keeping the aspect, the display chosen by the request's size; CAT: the exe's
  mousePositionSet clamp ported.
* Baselines: no stream or dump moved (Mission01 787e98865eb42c38 debug / 034e6f849e4488f4 release, IDENTICAL to R10;
  the four dumps EQUAL to producer 4's audited ones).

## What is left (owner)

* BTG background-star size: the exe's resolution-free term (`CAT-TODO(M3-R2)`), a look decision for the fix round.
* The window fit on the user's real desktop (COSMIC XWayland's display order, usable bounds and frame extents) is only
  checked by the forced bounds here; the attended play-test confirms it (owner: the play-test, M3-R2 if it misbehaves).
* The sensors manager's 3D map (points, range circles) and the hyperspace / trail lines stay physical-pixel thin at 4K
  (3D view); if the user finds them too thin at 200%, the fix round can scale them as the pie plate (owner M3-R2).
* Frame pacing / the 4K soak are untouched (M3 soak).
* The intermittent AddressSanitizer:DEADLYSIGNAL in the quit path after a timeout kill (m3r1-ui-g-sp-4k, once in four 4K
  sp runs of 1,200 s or more; no stack): owner M3 soak (the headless hour's shutdown and growth heaps).
