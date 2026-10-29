# M3-R4 ad hoc: the black screen after a resolution switch (HWI-FIX mode-switch-fix), 2026-10-28

Landed from the user's terminal session 38027cce between rounds, right after window-modes (fbf80ba).

## The user's words (verbatim)
- "I tried switching between 2560x1600 to 3840x2160 and now I just see a black screen for the game"
- "also when you have a chance, could you kill hwi-engine? it needs to be shut down" (done: the engine ignored
  SIGTERM, SDL turns it into an SDL_QUIT event that the hung main loop never read; SIGKILL)

## What the play logs showed (original/play-home/logs)
| log | switch | outcome |
|---|---|---|
| play-20261020-200121 | 1024x768 -> 3840x2160 | X error BadPixmap, Present (opcode 145) PresentPixmap (minor 1), the process gone: before window-modes |
| play-20261028-215719 | 3840x2160 -> 2560x1600 | the same X error |
| play-20261028-215753 | 2560x1600 -> 3840x2160 | the user's black screen: the log stops after the new window's "GL Extensions:", the process alive, its main thread asleep in poll (0.0% CPU), deaf to SIGTERM |

Opcode 145 is Present on the user's Xwayland (`xdpyinfo -queryExtensions`: Present 145, DRI3 146, GLX 149). No Xid in
the kernel log: no GPU fault.

## The cause
A resolution change (the Video screen's Accept: Options.c opOptionsAcceptHelper -> main.c mainLoadGL ->
mainShutdownGL -> render.c rndClose, then setupPixelFormat) tore the renderer down with GoK's rndClose, which quit
SDL's whole video subsystem (`SDL_QuitSubSystem(SDL_INIT_VIDEO)`: the X connection closed) and never deleted the GL
context; setupPixelFormat then opened a new connection, window and context. Under the NVIDIA driver (580.173.02) and
Xwayland the first buffer swap after that fails: the driver presents a pixmap that no longer exists (BadPixmap, the
crash) or waits for a Present completion that never comes (the hang, the black screen).

Proof, a stand-alone SDL program on the user's display with HIDDEN windows (never mapped; `tools/glswitch.c`):
four windows in a row (1024x768, 3840x2160, 2560x1600, 3840x2160), 30 swaps each.

| teardown between windows | swap interval 0 | swap interval 1 (the engine's) |
|---|---|---|
| GoK's: the video subsystem quit, the context leaked | BadPixmap PresentPixmap after window 1 | hung after window 1 |
| the fix: context released and deleted, window destroyed, the connection kept | 4 windows, 120 swaps | 4 windows, 120 swaps (twice) |

The engine itself, on the same display with a hidden window (the new harness flag `--hidden-x11`), with the switch
test drawing frames after the switch (`--mode modequit --mode-to WxH:120`): before the fix every switch
(2560x1600 -> 3840x2160, 3840x2160 -> 2560x1600, 1024x768 -> 3840x2160) died with the same BadPixmap on the swap of
the first frame after the switch, and LeakSanitizer found 4.3 MB leaked in libnvidia-glcore (the context).

## The change
- render.c `rndWindowDestroy` (HWI-FIX): `SDL_GL_MakeCurrent(window, NULL)`, `SDL_GL_DeleteContext`,
  `SDL_DestroyWindow`, both pointers NULL. rndClose (a mode switch and the shutdown) and setupPixelFormat's palette
  failure path call it instead of quitting the video subsystem; main's `SDL_Quit` at exit closes the connection.
- setupPixelFormat's "nothing changed" early return now also needs a window (after rndClose there is none, so a
  restore to the same size makes one).
- HWI-TEST: `/catHiddenWindow` (every window created SDL_WINDOW_HIDDEN, the pointer never warped by mousePositionSet
  nor put in relative mode by mouseClipToRect; the Linux build sends no synthetic keys: both utyToggleKeyStatesRestore
  calls are compiled out); harness `--hidden-x11` (the x11 driver on $DISPLAY with /catHiddenWindow; silent like
  every run); `/catModeQuitTest <W>x<H>:<frames>` (harness `--mode-to WxH:N`): the main loop draws N frames after the
  switch before the quit, a log line before the swaps of frames 1-3 and N; the switch test's size check allows one
  pixel (the user's Xwayland makes a 2560x1600 window's drawable 2561x1601 and 1024x768 1025x768).
- tools/lsan.supp: `leak:libnvidia-glcore.so` (the driver's own ~22 KB, 12 allocations, the same at every window size,
  after every context is deleted).

## Fence
Debug builds of the worktree; runs in `original/scratch-m2/m2r2-harness/runs/`. Both builds 0 warnings.

| run | what | before the fix | after |
|---|---|---|---|
| m3r4-msf-{before,after}-a | `--mode modequit --res 2560x1600 --mode-to 3840x2160:120 --hidden-x11` (the user's switch) | BadPixmap on frame 1's swap, rc 1, LSan 4.3 MB in libnvidia-glcore | 120 frames, 0 X errors, quit PASS, rc 0 |
| m3r4-msf-{before,after}-b | `--res 3840x2160 --mode-to 2560x1600:120 --hidden-x11` | the same BadPixmap | 120 frames, quit PASS, rc 0 |
| m3r4-msf-{before,after}-c | `--res 1024x768 --mode-to 3840x2160:120 --hidden-x11` (the 2026-10-20 crash) | the same BadPixmap | 120 frames, quit PASS, rc 0 |
| m3r4-msf-after-d | `--res 1280x720 --mode-to 1920x1080:120 --hidden-x11` (window to window) | | 120 frames, quit PASS, rc 0 |
| m3r4-msf-resize-x11 | `--mode resize --res 2560x1600 --hidden-x11` (the list against the user's two 3840x2160 displays) | | all 20 presets listed (4:3 640x480 .. 2048x1536, 16:10 1280x800 .. 2560x1600, 16:9 1280x720 .. 3840x2160, 5:4 1280x1024), 85 checks PASS |
| m3r4-msf-modequit, -f | `--mode modequit` offscreen, and with `--mode-to 1280x720:60` | | PASS, rc 0, quit once |
| m3r4-msf-resize | `--mode resize` offscreen | | 68 checks PASS |
| m3r4-msf-uiscale, -4k | `--mode uiscale` at 1024x768 and 3840x2160 | | 45 checks PASS each |
| m3r4-msf-toggle | `--mode toggle` | | PASS |
| m3r4-msf-mission | `--mode mission --mission-ticks 6000` | | IDENTICAL to m3r3i-e-mission-a (6001 ticks) |

ASan 0 and no unsuppressed leak in every after-run.

## Not verified here
The switch on the user's mapped, visible window (the compositor's full-screen path): the hidden window exercises the
same driver, connection and Present path and reproduced both failures, but the user's own switch is the final check.

## Files
`src/SDL/render.c`, `src/SDL/mouse.c`, `src/SDL/main.c`, `src/SDL/utility.c`, `src/Game/UiScaleTest.c` / `.h`,
`tools/m2_harness.sh`, `tools/lsan.supp`, `tools/glswitch.c`, this file, `docs/features/window-modes.md`, `cycle/STATE.md`.
