# Window modes: resolution presets and the resizable window (M3-R4 window-modes)

HWI-FEATURE(resolution-presets) and HWI-FEATURE(window-resize). Landed between the M3-R3 and M3-R4 rounds from the
user's terminal session 38027cce; the round doc is `docs/m3-r4-window-modes.md`.

## The user's words (verbatim)
"okay, now for the supported resolutions, please add typical supported resolutions for 16:9, 16:10, and 4:3 aspect
ratios as well as support for dynamic window sizing incase anyone wants a windowed format that the user can drag and
expand the window to the size they want"

## What cataclysm.exe does
- The Video screen's resolution list is the modes the display driver reports for the device (rinit.c's enumeration
  under Windows 98/XP: DirectDraw's mode list, every mode the adapter can switch to, labelled "W x H" and "N bit").
- The window has a fixed client size (WinMain 0x005631f0: no WS_THICKFRAME); a windowed mode is the requested size
  and nothing else, and a resolution change is a mode switch (the Video screen's Accept: the render system torn down
  and remade at the new size).

## What the recreation did before this item
- The SDL port's enumeration (`rinEnumeratePrimary`) listed the desktop mode and its integer divisions only (1, 2, 3,
  4: at a 3840x2160 desktop 3840x2160, 1920x1080, 1280x720, 960x540), so a 4:3 or 16:10 size could not be chosen, and
  a request that was not listed (the options file's size, a /WxH switch) left the list's first entry selected: an
  Accept then switched to that entry.
- The window was `SDL_WINDOW_OPENGL` only: not resizable.

## Design
### The list (rinit.c, Options.c)
- A table of typical sizes, `rinPresets[]`: 4:3 640x480, 800x600, 1024x768, 1152x864, 1280x960, 1400x1050, 1600x1200,
  2048x1536; 16:10 1280x800, 1440x900, 1680x1050, 1920x1200, 2560x1600; 16:9 1280x720, 1366x768, 1600x900, 1920x1080,
  2560x1440, 3840x2160; and the exe's 5:4 1280x1024.
- `rinEnumeratePrimary` lists every preset that fits one of the connected displays (both sides within the display's
  current mode) and every display's own current mode, at the desktop's depth, in ascending order of width then height
  with no duplicates (`rinAddModeIfMissing`, a sorted insert). The offscreen harness driver has one 1024x768 display,
  so a harness run lists 640x480, 800x600 and 1024x768.
- When the Video screen is opened (`opResListSet`) the window's current request is added to the list if it is not
  there (a dragged size, a /WxH switch, an options-file size), so the list always holds and selects the size in use:
  Accept with nothing changed is no mode switch (`opResChanged` compares the selected entry with the request).
- Each entry is drawn as "W x H" and its aspect: 4:3, 16:10, 16:9 or 5:4 when within 1% of it, else the reduced W:H
  (`rinAspectName`). The depth column is gone: every entry carries the desktop's depth. The list's own scrollbar
  (FEFlow.c's ListWindow atoms) takes the longer list.
- The selected entry's size is the request as before (opResItemDraw -> opSave*; Accept -> opOptionsAcceptHelper: the
  mode switch with a new window).

### The resizable window (render.c, uiscale.c, main.c)
- `setupPixelFormat` creates a windowed mode with `SDL_WINDOW_RESIZABLE` (never a fullscreen or fullscreen-desktop one)
  and a minimum size of 640x480 (`SDL_SetWindowMinimumSize`: the front end's base size).
- Every `SDL_WINDOWEVENT_SIZE_CHANGED` of the game window (main.c `HandleEvent`) calls `uiScaleWindowResized`: the new
  drawable size (`SDL_GL_GetDrawableSize`) is the physical size (`MAIN_WindowWidthActual` / `HeightActual`), the window
  size in points (`SDL_GetWindowSize`) the mouse conversion's; the UI scale is chosen again for the new height
  (`uiScaleWanted`: the auto rule 200% at 2160 rows or more, 150% at 1440 or more, else 100%, or the setting), the
  logical size follows (`uiScaleSet`: physical / scale, rounded, the pixel zoom), the 3D projection's aspect
  (`rndAspectRatio`) and the GL viewport are the drawable's, the mouse keeps its place on the logical screen, and the
  exe's reflow after a resolution change runs: `utyChangeResolution` (the main region's rect, every loaded front-end
  screen's atoms re-centred by `feAllScreensReposition`, the subtitle regions). The log's "window: resized to WxH
  (window WxH) scale P% (..) logical WxH (was WxH) aspect A" line and a new "hwi-settings:" line record it.
- In the front end the reflow runs at once (as the Video screen's own mode switch does: the open screens are popped
  and the main menu restarts). In a game `feAllScreensReposition` would pop a manager or the Escape menu past their own
  close paths (`feScreenDisappear` deletes the top screen without the manager's cleanup), so while a front-end screen
  is open the reflow is owed (`uiResizePending`) and runs from the main loop once no screen is open
  (`uiScaleResizeFlush`, before `utyTasksDispatch`); the physical size, scale, logical size, aspect, viewport and mouse
  change at once, so the 3D view and the HUD are right from the first frame and only the open screen waits.
- The dragged size becomes the window's request (`uiRequestW` / `H`, `uiScaleWindowRequestGet`): the options file's
  `screenWidth` / `screenHeight` rows, written at an Accept and at exit, hold it, so the next start opens at the dragged
  size; the Video screen lists and selects it (above). A size change in a fullscreen-desktop window (a display's mode
  change) is applied the same way but does not become the request.
- The window fit rule (M3-R1: a windowed request as large as the desktop opens as fullscreen-desktop; one larger than
  the usable area is shrunk) is unchanged: the user's 3840x2160 windowed request on a 3840x2160 display still opens
  as a borderless fullscreen-desktop window, which cannot be dragged; a smaller size picked on the Video screen (or in
  the options file) gives a framed, draggable window.

### Not changed
- No row or switch: the window is always resizable in a windowed mode (the exe's fixed frame has no look to keep).
- The exe's mode switch on Accept (a new window) is as before: the resizable window is a second way to the same state.
- A mode switch tears the window down by deleting the GL context and destroying the window while keeping SDL's video
  subsystem (the X connection) up: GoK quit the whole subsystem, which under the NVIDIA driver and Xwayland crashed
  (BadPixmap) or hung (the black screen) on the first swap after the switch (HWI-FIX, docs/m3-r4-mode-switch-fix.md).
- The simulation: nothing in the universe reads the window size (the streams are identical).

## HWI-TEST
- `/catResizeTest` (harness `--mode resize`): the list (ascending, unique, every preset that fits display 0, the
  display's mode, the request; the aspect names), scripted size changes (`uiScaleWindowResizedTo`, the drag path less
  SDL's event: the offscreen driver's `SDL_SetWindowSize` changes nothing) to 1600x900, 800x600, 1280x1024, 2560x1440,
  3840x2160 and 1234x777 with the physical size, scale, logical size, main region, main menu centring, 3D aspect,
  request and mouse corner checked at each, the Video screen listing and selecting the dragged size with no switch
  pending, the options file's rows at the dragged size, then the start size again. "resizetest:" lines.
- `/catResizeAt <tick>:<W>x<H>` (harness `--resize-at`): the same size change at a universe tick of any run (the
  in-game path); with `--shot` the screenshots before and after show the HUD and the viewport at both sizes.

## Files
`src/SDL/rinit.c` / `rinit.h` (the presets, the enumeration, `rinAddModeIfMissing`, `rinAspectName`),
`src/Game/Options.c` / `.h` (the aspect label, the request listed, the depth-free match, the HWI-TEST accessors),
`src/SDL/render.c` (the resizable flag, the minimum size), `src/SDL/uiscale.c` / `.h` (the resize path),
`src/SDL/main.c` (the event, the flush, the switches), `src/Game/UiScaleTest.c` / `.h` (the self-test, the at-tick
hook), `src/SDL/utility.c` and `src/Game/OracleDump.c` (the hooks), `tools/m2_harness.sh` (mode resize, --resize-at),
`tools/play.sh` (the header's window paragraph).
