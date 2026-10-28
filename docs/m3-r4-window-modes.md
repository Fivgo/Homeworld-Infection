# M3-R4 ad hoc: resolution presets and the resizable window (HWI-FEATURE(resolution-presets), HWI-FEATURE(window-resize)), 2026-10-28

Landed from the user's terminal session 38027cce between rounds, after module-spin-interp (93e853f). The feature spec
is `docs/features/window-modes.md`.

## The user's words (verbatim)
- "okay, now for the supported resolutions, please add typical supported resolutions for 16:9, 16:10, and 4:3 aspect
  ratios as well as support for dynamic window sizing incase anyone wants a windowed format that the user can drag and
  expand the window to the size they want"

## What was there
- cataclysm.exe lists the display driver's modes on the Video screen (rinit.c under Windows: every mode the adapter
  switches to) and opens a window of a fixed client size (WinMain 0x005631f0, no WS_THICKFRAME); a resolution change
  is the Video screen's Accept (a mode switch: the render system remade at the new size).
- The SDL port's enumeration (`rinEnumeratePrimary`) listed the desktop mode and its integer divisions by 1 .. 4 only:
  on the user's 3840x2160 desktop 3840x2160, 1920x1080, 1280x720 and 960x540, so no 4:3 or 16:10 size could be chosen;
  a request that was not listed (an options-file size, a /WxH switch) left the list's first entry selected, and an
  Accept with nothing touched switched to it.
- The window was created with `SDL_WINDOW_OPENGL` alone: not resizable.

## The change
- rinit.c: a table of the typical sizes of the three aspects (4:3 640x480 .. 2048x1536, 16:10 1280x800 .. 2560x1600,
  16:9 1280x720 .. 3840x2160) and the exe's 5:4 1280x1024; the enumeration lists every preset that fits one of the
  connected displays (SDL's current mode of each), each display's own mode, and the start's request
  (`MAIN_WindowWidth` / `Height`: the options file's or a switch's size, read before `InitWindow` enumerates), as an
  ascending, duplicate-free list (`rinAddModeIfMissing`, a sorted insert). `rinAspectName`: 4:3, 16:10, 16:9 or 5:4
  within 1%, else the reduced W:H.
- Options.c: the list's second column is the aspect name instead of the depth (every entry carries the desktop's
  depth); `opResListSet` adds the window's current request (a dragged size, a later switch) if it is missing and
  selects it; the selection match no longer compares the depth. HWI-TEST accessors `opResListFind` / `Selected` /
  `Count`.
- render.c: a windowed mode gets `SDL_WINDOW_RESIZABLE` and a minimum size of 640x480 (never a fullscreen or
  fullscreen-desktop window).
- uiscale.c: `uiScaleWindowResized` on every `SDL_WINDOWEVENT_SIZE_CHANGED` of the game window (main.c `HandleEvent`):
  the drawable is the physical size, the window size the mouse conversion's, the UI scale is chosen again for the new
  height (the auto rule or the setting), the logical size follows (`uiScaleSet`), `rndAspectRatio` and the GL
  viewport are the drawable's, the mouse keeps its place on the logical screen, the dragged size becomes the request
  (uiRequestW / H: the options file's `screenWidth` / `screenHeight` rows at an Accept and at exit, so the next start
  opens at it), and the exe's reflow after a resolution change runs (`utyChangeResolution`: the main region, every
  loaded front-end screen re-centred, the subtitle regions). In the front end at once; in a game the reflow would pop
  a manager or the Escape menu past their close paths (`feAllScreensReposition` -> `feScreenDisappear`), so while a
  screen is open it is owed and runs from the main loop once none is (`uiScaleResizeFlush`); the view, the HUD and the
  mouse change at once either way. A fullscreen-desktop window's size change (a display's mode change) is applied the
  same way but does not become the request. "window: resized to WxH (window WxH) scale P% (..) logical WxH (was WxH)
  aspect A" and a new "hwi-settings:" line per change.
- The M3-R1 fit rule is unchanged: a windowed request the size of the display opens as a borderless fullscreen-desktop
  window (no frame to drag). The user's 3840x2160 `fullScreen 0` options file keeps opening that way; a smaller size
  picked on the Video screen gives a framed, draggable window, and the dragged size is what the next start opens at.
- HWI-TEST: `/catResizeTest` (harness `--mode resize`), `/catResizeAt <tick>:<W>x<H>` (harness `--resize-at`,
  any mode with a universe). `uiScaleWindowResizedTo` is the drag path less SDL's event: the offscreen driver's
  `SDL_SetWindowSize` changes nothing, so a scripted change pretends the drawable is the new size (the real
  framebuffer stays: a `--shot` after a scripted change UP is clipped to the real buffer, one after a change DOWN from
  a 4K start shows the new layout in the buffer's bottom-left corner).

## Fence (debug builds of the worktree, ASan 0 in every run, both builds 0 warnings)
Runs in `original/scratch-m2/m2r2-harness/runs/`.

| run | what | result |
|---|---|---|
| m3r4-wm-resize | `--mode resize` (the offscreen driver's one 1024x768 display, the request 1024x768) | list 640x480, 800x600, 1024x768 (4:3, ascending, unique; the 17 presets larger than the display rightly absent); the six scripted sizes: 1600x900 100% logical 1600x900, 800x600 100%, 1280x1024 100% (aspect 1.25), 2560x1440 150% logical 1707x960, 3840x2160 200% logical 1920x1080, 1234x777 100%; at each the main region = logical, the main menu's OPTIONS button at loadedX + (logical - 640) / 2, the 3D aspect = w / h, the request = the size, the far corner -> the logical far corner; the Video screen lists 1234x777 (added as the request, "16:10") at index 3 and selects it, no switch pending; the file's screenWidth 1234 / screenHeight 777 once each; back to 1024x768; 68 checks, PASS |
| m3r4-wm-resize-4k | `--mode resize --res 3840x2160` (the request larger than the offscreen display) | the list holds 3840x2160 (16:9) as the start's request beside the three 4:3 sizes; the same six sizes and checks from a 200% start; 68 checks, PASS (the first build FAILED this one check, "the window's request listed": the request was added only when the Video screen opened; fixed by listing the start's request at enumeration) |
| m3r4-wm-resize-1234 | `--mode resize --res 1234x777` | the odd request listed and selected from the start ("16:10": within 1%); 68 checks, PASS |
| m3r4-wm-sk-down | skirmish LiveFireExercise2 `--res 3840x2160 --resize-at 300:1280x720 --shot 280,340 --real-clock 60` | "window: resized to 1280x720 .. scale 100% .. logical 1280x720 (was 1920x1080) aspect 1.7778", "resizetest: at tick 300 .. ok=1 screens=0" (no screen open: the reflow at once); the tick-340 shot's bottom-left 1280x720 (the real 4K buffer's corner the 1280x720 viewport draws into) shows the HUD laid out for 1280x720 at 100% (the player list top-left, SU / RU / clock bottom-right) and the Kuun-Lan at the right aspect; skirmish self-test PASS |
| m3r4-wm-sk-up | skirmish `--resize-at 300:1600x900 --shot 280,340 --real-clock 60` from 1024x768 | "window: resized to 1600x900 .. logical 1600x900 (was 1024x768)", at tick 300 ok=1 screens=0; the tick-340 shot is the 1600x900 layout clipped to the real 1024x768 buffer (the harness limit above); skirmish self-test PASS |
| m3r4-wm-uiscale, -4k | `--mode uiscale` at 1024x768 and `--res 3840x2160` | 45 checks PASS, 45 checks PASS (the mode switch audit row included) |
| m3r4-wm-modequit | `--mode modequit` (1024x768 -> 1280x720, then one quit) | switched, rc 0, "Quit game" once: PASS |
| m3r4-wm-toggle | `--mode toggle` | PASS, 0 fails |
| m3r4-wm-mission | `--mode mission --mission-ticks 6000` | IDENTICAL to m3r3i-e-mission-a (6001 ticks): nothing in the universe reads the window |

The uiscale, modequit, toggle and mission runs were repeated on the final binary (the enumeration's request line
was the last source change) with the same results. The real drag on a desktop (the frame, SDL's event, the live
reflow while dragging, a Wayland / X11 compositor's drawable) is the user's check: the harness has no window.

## Files
`src/SDL/rinit.c` / `rinit.h`, `src/Game/Options.c` / `.h`, `src/SDL/render.c`, `src/SDL/uiscale.c` / `.h`,
`src/SDL/main.c`, `src/Game/UiScaleTest.c` / `.h`, `src/SDL/utility.c`, `src/Game/OracleDump.c`,
`tools/m2_harness.sh`, `tools/play.sh`, `docs/features/window-modes.md`, this file, `cycle/STATE.md`.
