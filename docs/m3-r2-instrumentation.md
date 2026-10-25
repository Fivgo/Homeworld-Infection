<!-- done: settings-line, overlay, framelog, quit-fix, fences -->
# M3-R2 producer 1: the instrumentation (key instrumentation)

Base 06dae5c (this work is `git diff 06dae5c` in the m2 worktree, plus the new files `src/Game/Instrument.c`,
`src/Game/Instrument.h`, `tools/frame_log_report.py` and this report). Subsystems: **(a) settings-line**,
**(b) overlay**, **(c) framelog**, **(d) quit-fix**, **(e) fences**. All five are done.

The user's answers file (`cycle/answers/m3-r2-user.md`) was read at the start and again before this report. One answer
was added during the work: *"okay I have gotten off my game, use the gpu how you need to"* (2026-10-22 11:22). Before
that, at 11:20, `ffxiv_dx11.exe` held the GPU at 76 %. One `claude-ping` went out, and an idle wait (40 % or less for
120 s) ended at 11:23:45. The 4K measurement ran at 11:45, at 1 % GPU (below). No other answer changes this task.

## Why

The user's 07:43 play-test on 2026-10-21 ran with Smooth motion OFF, because the options file's rows were 0, and
nothing in the log said so. M3's soak also needs an instrument (docs/m3-r1-summary.md, "Plan for M3-R2" row 2). This
round adds three things:
- a log line that always gives the session's state;
- an on-screen overlay;
- a per-second frame log with a report tool.

It also fixes GoK's swallowed quit after a resolution change, a finding M3-R1 handed to this round.

## (a) The settings line: HWI-FEATURE(instrumentation)

One line goes to stdout, and so into every harness `run.log` and every `tools/play.sh` log:

```
hwi-settings: objInterpolate=1 (default) nisInterpolate=1 (default) letterboxBehind=0 (default) noLOD=1 (default) uiScale=0->100% (default, auto) overlay=0 (default) fit=window (as requested (no desktop bounds)) 1024x768 switches=none [start]
```

- **When.** Once, when the first window exists (`uiscale.c uiScaleWindowCreated`). By then the options file and the
  command line are both applied (`utyOptionsFileRead`, then `ProcessCommandLine`, then the window). The line prints
  again whenever a value, a switch's state or the UI scale in use changes. The `[why]` tag says what caused it:
  - `[options accept]`: `Options.c opOptionsAcceptHelper` calls `hwiSettingsAccepted`.
  - `[console <row>]`: `Console.c cat_consoleRunCommand`, for an HWI row (id >= `CAT_CON_HWI_ROW_ID`).
  - `[key overlay]`: the overlay key.
  - `[window]`: a mode switch, which builds a new window.
  - `[frame]`: a per-frame poll in `rndRenderTask` (`hwiFrameEnd`) that catches any change nothing announced, such as a
    checkbox click before Accept.
- **The fields.** Each setting's effective value and its source: `objInterpolate`, `nisInterpolate`,
  `letterboxBehind`, `noLOD` (FULL DETAIL), `uiScale` (the row, then `->` the percent in use, plus `auto` when the row
  is 0), `overlay`, `fit` (the window's kind from SDL's flags, then the fit rule's choice from `uiscale.c
  uiScaleWindowFit` through the new accessor `uiScaleFitRule()`, then the physical size), and `switches` (the run-only
  switches in force: `/noInterp`, `/noNISInterp`, `/letterboxBehind`, `/lodBands`, `/uiScale N`, `/overlay`).
  The existing `window:` and `uiscale:` lines are unchanged.
- **The sources:**
  - `default`: the options file read at start-up has no row of that name. `hwiSettingsRowsRead` is called from
    `utyOptionsFileRead` right after `scriptSetFileSystem` and only notes which names are present.
  - `cfg`: the file's row, or the row that the Options screen's Accept saved.
  - `switch`: a run-only switch stands. Its state is read from `miSwitchObjStands` / `miSwitchNisStands` (new),
    `rndLetterboxSwitchStands`, `catLodSwitchStands`, `uiScaleSwitchStands`, and `/overlay`'s flag.
  - `console`: a developer-console row set it.
  - `options`: an Options-screen control changed it and it is not accepted yet. This is a fifth source beyond the
    task's four. It labels changes that only the frame poll sees.
  - `key`: the overlay key.
- **How producers 2-4 add their settings and switches.** Add one row to `hwiSettings[]` in `src/Game/Instrument.c`
  with five entries:
  1. the row name;
  2. a value getter (`sdword (*)(void)`);
  3. the switch test (`bool32 (*)(void)`, or NULL);
  4. the switch-name function;
  5. whether `Cataclysm.cfg` has a row of that name.

  The line, the change detection and the sources then follow on their own. A console row only needs the existing
  `CAT_CON_HWI_ROW_ID + n` id: `cat_consoleRunCommand` already announces every HWI row to `hwiSettingsConsole`. A
  setting that is neither a value nor a switch can still be printed by calling `hwiSettingsChanged("<why>")`.
  Note: `hwiSettingsPrint` finds the `uiScale` row (index 4) and the `overlay` row (index 5) by position, so append
  new rows at the end.

### What the user's play-test logs would have said

The user's options file before 07:43 is `original/play-home/.homeworld/Cataclysm.cfg.bak-20261021`. Its rows are
`objInterpolate 0` and `nisInterpolate 0` (rows 107-108), `letterboxBehind 0`, `uiScale 0`, `noLOD 1`,
`screenWidth 3840` / `screenHeight 2160` and `fullScreen 0`. The 07:43 log's own lines (9-10) are `window: requested
3840x2160, usable 3840x2160 (display 0: 3840x2160), created 3840x2160 (fullscreen-desktop): display size -> fullscreen
desktop` and `uiscale: ... 200% (auto, uiScale 0)`. From these, the start-up line would have been:

```
07:43  hwi-settings: objInterpolate=0 (cfg) nisInterpolate=0 (cfg) letterboxBehind=0 (cfg) noLOD=1 (cfg) uiScale=0->200% (cfg, auto) overlay=0 (default) fit=fullscreen-desktop (display size -> fullscreen desktop) 3840x2160 switches=none [start]
08:15  hwi-settings: objInterpolate=1 (cfg) nisInterpolate=1 (cfg) letterboxBehind=0 (cfg) noLOD=1 (cfg) uiScale=0->200% (cfg, auto) overlay=0 (default) fit=fullscreen-desktop (display size -> fullscreen desktop) 3840x2160 switches=none [start]
```

So the 07:43 line would have said `objInterpolate=0 (cfg)` on its first line. These lines were reproduced on the
harness with the same rows:
- `m3r2-set-0743b` (`--res 3840x2160` and the 07:43 rows) printed `objInterpolate=0 (cfg) nisInterpolate=0 (cfg)
  letterboxBehind=0 (cfg) noLOD=1 (cfg) uiScale=0->200% (cfg, auto) ... 3840x2160`.
- `m3r2-set-0743` and `m3r2-set-0815` (the same rows with `/testUsableBounds 3840x2160@3840x2160`) printed
  `fit=fullscreen-desktop (display size -> fullscreen desktop)`. The offscreen driver's 1024x768 display then made
  them 1024x768 at 100 %.

`switches=none` is an assumption. `tools/play.sh` prints its switches on stderr before the log starts, so the old logs
do not record them. From now on the line records them.

## (b) The overlay: HWI-FEATURE(instrumentation)

- **Default.** Off. The variable is `hwiOverlay`; it has no options-file row, so nothing is saved.
- **Three toggles.**
  - The console row `overlay 0|1`: `CAT_CON_HWI_ROW_ID + 5`, so `CAT_CON_HWI_ROWS` goes from 5 to 6.
  - The run-only switch `/overlay`: `main.c OverlaySet` calls `hwiOverlaySwitch`.
  - The **Menu (Application) key**, `SDL_SCANCODE_APPLICATION` (`HWI_OVERLAY_KEY`). `main.c HandleEvent` handles it on
    `SDL_KEYDOWN` with `repeat == 0`. The key then goes on to `keyPressDown` as before.
  - **[M3-R3 settings-and-keys] F1**, the user's choice of an F key (2026-10-24): `main.c HandleEvent` toggles the
    overlay on `SDL_SCANCODE_F1` (`HWI_OVERLAY_FKEY`, VK 0x70) while `KeyBindings.c hwiKbOverlayFKeyFree()` holds: no
    Shift / Alt / Ctrl held, no binding of plain F1 in the table, the key-binding screen not waiting for a key. F1 is the
    one F key the exe leaves free in the game view: `default_keys.cfg` binds plain F2..F12 (Tactics_*, Formation_*; F12
    = Formation_Parade, so not F12) and F1 only with modifiers; `WindowProc 0x00561f10` special-cases only the F11 / F12
    key-ups; `mrStartup 0x00563b50` gives every key to the bindings; no `.fib` atom has an F key as its hot key; the
    construction manager's `cmBuildHotKey 0x00458d40` takes F1 but its table builds nothing on it. The key goes on to
    `keyPressDown` as before. The Menu key stays as a second toggle. Keys self-test: `keystest: overlay fkey F1
    scancode=58 vk=0x70 plain bindings F1=0 F12=1 free=1 bound-free=0 reset-free=1` (rows / bindings still 197 / 149).
    docs/m3-r3-settings-and-keys.md (a).
- **Why the Menu key.** No default binding of cataclysm.exe uses it, and this tree cannot bind it [audit: said "It belongs
  to no part of cataclysm.exe's key handling". The exe's `WindowProc 0x00561f10` (WM_KEYDOWN 0x100) passes VK_APPS to
  `keyPressDown` and `cat_consoleKeyPress` like every key, and its `cat_kbKeyToString 0x0049b590` returns a name for
  every code below 0x100 (0x5d: the `%c` fallback `]`, the English name table at 0x008b0d62 being 0), so the exe's
  `kbListWindowCB 0x0049bca0`, which shows Invalid_Key only on a NULL name, would bind it. The tree's refusal (no 0x5d in
  `catKeyVkTable`) is a pre-existing gap of M2-R6's table, not the exe's rule: if a later port adds VK_APPS to the table,
  the overlay key and a user binding would share the key]:
  - `default_keys.cfg` (200 lines, v 84) binds nothing to VK_APPS (0x5d). The keys self-test's new check confirms it:
    `bindings on VK_APPS=0`.
  - The tree's VK table `Key.c catKeyVkTable` has no virtual-key code for this key (`catKeyVkFromKey` returns 0). So a
    `keys.cfg` row cannot bind it (`catKeyFromVk(0x5d)` is 0), and the key-binding screen refuses it
    (`kbListWindowCB`'s CM_KeyCaptured goes to Invalid_Key when vk is 0).
  - The main region's fixed key filter `mainrgn.c mrKeyFunction` does not include it.
  - The developer console gets vk 0, which it ignores.
  - In the exe's export, `0x5d` appears in `switch` cases only in ship-type functions (`GetShipSlot`, `isStrikeCraft`,
    `InitStatShipInfo`, `GenericInterceptorStaticInit`), never in key code.

  Every other key of a standard keyboard has a VK code in the table, so any of them could be bound in `keys.cfg` (the
  exe's `cat_kbKeyToString` names every code) [audit: "every other key" narrowed: F13-F24 and the media keys have no
  entry either].
- **Drawing.** `rndRenderTask` draws it after the subtitles, the cursor, the shameless plug, the build version and the
  fill counter, and before `catShotFrameEnd`, so `--shot` captures it.
  - Position: the top-left corner at logical (4, 4). Coordinates are logical, so it draws at the UI scale.
  - Font: the game's `default.hff` (`ghDefaultFont`), pale yellow, on a translucent black box (`primRectTranslucent2`,
    alpha 160).
  - The corner: in the captured HUD scene, the HUD has the ship list at the top right, the info panel at the left from
    y 190, the task-bar arrow at the bottom centre and the SU / RU / timer box at the bottom right. The top-left
    corner holds nothing (`full1024-70.png`).
- **Fields** (five lines):
  - `FRAME <last> MS  MED <median> P99 <p99>`: the frame period of the last frame, and the median and 99th percentile of
    the last whole second. They show `-` until one second has been measured.
  - `TICK <ms> MS X<n>  ALPHA <alpha>`: the universe task's time in the last frame that ran a tick, the number of ticks
    it ran, and `miRenderAlpha()` (-1 means no interpolation).
  - `OBJ <0|1> NIS <0|1> LBB <letterboxBehind> NOLOD <noLOD> SW <switches>`.
  - `VERTS <n>  OBJECTS <n>`: the /catLodStats count of the levels drawn in the main view, or `VERTS -` when no main
    view was drawn, for example in the sensors manager.
  - `FULL DETAIL ON|OFF  UI <percent>%`.
- **The clocks.** SDL's performance counter, as `Task.c taskClockSample` reads it.
  - **FRAME** is the period from one end of the render task (after the flip, `afterTheSwap`) to the next. It includes
    every task of the frame and the swap's wait, which is what the display shows.
  - **RND** (in the frame log) is the render task alone, from its top to after the flip.
  - Neither calls `glFinish` or reads anything back.
  - **/catLodStats' ms** is RND plus a `glFinish` before the clock is read (`render.c catLodStatsFrameEnd`).
  - The universe tick's cost is timed around each call of the universe task in `Task.c taskExecuteAllPending`
    (`taskCurrentTask == universeTaskHandle`).
  - The vertex count comes from `render.c catLodTestMainView`. Its early return now also lets `hwiWantVerts()` through,
    which is true while the overlay or /frameLog is on. It reads the same side-effect-free `cat_lodDrawLevelGet` /
    `catLodLevelVerts`, and `fsqrt` stays inside the `logIt` branch.

## (c) /frameLog: HWI-TEST

- **The switch.** `/frameLog <file>` (run-only): `main.c FrameLogSet` calls `hwiFrameLogSet`, and an `atexit` handler
  closes the file. The harness flag is `--frame-log`, which writes `<run>/frame.log` and adds a `framelog:` summary
  line (the report's `--oneline`).
- **Header.** `# hwi-framelog v1 build=.. debug|release commit=<HWI_GIT_HASH> size=WxH logical=WxH refresh=R (source)
  heap=67108864 driver=..`, then `# settings: <the settings line>` and `# fields: ...`. [audit: `build=` printed `UNSUPPORTED_BUILD` in
  every log: meson defines HOMEWORLD_BUILD_VERSION only for src/SDL (src/SDL/meson.build), so src/Game/Instrument.c got
  mainswitches.h's fallback. The audit added `main.c hwiBuildVersion()` (HWI-TEST); the header now says e.g.
  `build=9f6e6a7+`.] The refresh is the
  `--real-clock` rate (`MotionTest.c catMotionRealClockFps`, new), else the display mode's `refresh_rate`, else 144.
  The header says which.
- **Per second.**
  - Frames: `s t frames ft_p50 ft_p95 ft_p99 ft_max`.
  - `over over1 over2 over3p`: frames longer than one refresh interval, with how many refreshes late. A frame's late
    refreshes are `ceil(ft / interval - 0.05) - 1`.
  - Render: `rnd_p50 rnd_max`.
  - Universe: `ticks tick_p50 tick_max`.
  - Heap: `heap_used heap_growth growth_pools dupe`.
  - Draw: `verts_p50 verts_max objects`.
  - Then an `h s=N bin:count,...` line: the second's frame periods in 0.1 ms bins, so the whole run's percentiles can
    be computed.
- **The heap.** `Memory.c memHwiUsage` (new, HWI-TEST, both builds) walks the cookies of the main pool and the growth
  pools without the debug verification. It adds the small-block heaps' allocated blocks at their block size, and
  counts the live `"dupe trcolorinfo list"` blocks (main cookies plus the small-block-heap lists) where the build keeps
  names. That is the debug build; the release build gives -1.
  - Cross-check: `m3r2i-sp-fl`'s `dupe` went from 0 to 152 at the jump to Mission02 (s=117). The `--teamcolour`
    mission test `m3r2i-tc-fl` logs `teamcolour: heap level=Mission02.mission dupe=152` (from mem.analysis). The two
    counts agree.
- **`tools/frame_log_report.py`** (`python3 -I`). Its options are `--window 10`, `--tolerance 0.10` and `--oneline`.
  It reports:
  - the whole run's frame-period p50 / p95 / p99 / max, from the histograms;
  - the frames over budget, with how many refreshes late;
  - the universe ticks and their cost, and the vertices drawn;
  - **the drift test**: the last N minutes' p50 and p99 against the first N minutes' (a run shorter than two windows
    compares its first and last thirds);
  - **the heap's growth per minute** (least squares) and the growth heaps;
  - **the seconds over budget** (a second whose ft_p99 is above the refresh interval).

  It is documented in `docs/features/high-refresh-4k.md`, new section "The soak's instrument".

### The 4K release measurement (fence c)

- **GPU state.** nvidia-smi showed 1 % before both runs and 1 % after them (11:45:42 to 11:46:00). The load average
  was 8.8-9.3, which includes VS Code's ripgrep. `claude-gpu-done` was run afterwards.
- **Command.** `--mode ai --ai-ticks 500 --res 3840x2160 --lodstats 150-450:fleet --bin build/release/hwi-engine
  --frame-log`, on M3-R1's frame-time scene.

| run | lodstats (ms: top to after the flip + glFinish), ticks 150-450 | frame log, seconds 2-5 (inside 150-450, ~90 frames each) | frame_log_report (whole run, 500 frames) |
|---|---|---|---|
| m3r2i-perf4k-fl | 301 frames, median 10.026, p99 14.964, max 16.289, 25,169 verts | ft_p50 10.16 / 10.62 / 10.91 / 10.14; rnd_p50 9.80 / 10.33 / 10.55 / 9.80; ft_p99 15.9 / 15.7 / 16.8 / 16.0; verts 25,169 | ft p50 10.3, p95 15.1, p99 16.6; 485 of 500 frames over 6.94 ms (432 one refresh late, 50 two, 3 three or more); 6 of 6 seconds over budget |
| m3r2i-perf4k-fl2 | 301 frames, median 9.927, p99 14.505, max 24.216, 25,169 verts | ft_p50 10.14 / 10.40 / 10.11 / 10.85; rnd_p50 9.72 / 9.99 / 9.77 / 10.55 | ft p50 10.4, p95 14.2, p99 15.7, 485 over |

- **The clocks agree.**
  - RND (no glFinish) sits about 0.2 ms below lodstats' glFinish figure. The offscreen driver's swap is synchronous
    enough that the glFinish adds little.
  - FRAME sits about 0.35 ms above RND: the other tasks of the frame and SDL's event poll.
  - All three agree with M3-R1's 9.08-11.9 ms for FULL DETAIL at 4K. FULL DETAIL is still above the 6.94 ms budget at
    144 Hz; that is producer 4's work.
- **Two frames outside the window.** The `tick_max` of 1,201 ms in the last second is the universe task's last call,
  which runs `oracle: mission run done`, then the quit and `titanShutdown`. It comes after tick 450, outside the
  measured window. The 125-129 ms call in second 1 is in the ai test's start, before tick 150.

## (d) The quit after a resolution change

- **The exe (read first).**
  - `mainDestroyWindow 0x00560350`: `DAT_008b85b0 = 0; DestroyWindow(ghMainWindow);`, then `CreateWindowExA(...)`
    again.
  - `WindowProc 0x00561f10`, `case 2` (WM_DESTROY): if the flag is set, `utyGameSystemsShutdown(); ...
    PostQuitMessage(0)`; else it sets the flag back to 1 and returns.

  So the exe's own `DestroyWindow` consumes the clear at once. `mainShutdownRenderer 0x00561260` and `mainLoadGL
  0x00561570` follow the same pattern: every clear (decompiled.c 316061-317047) comes right before a `DestroyWindow`.
  `mainRescaleMainWindow 0x0055fc70` was read as well; it is the same windowed path (docs/m3-r1-ui-scale.md:32). It
  needs no change.
- **GoK's fault.** `main.c mainDestroyWindow` clears `mainActuallyQuit`, but the window is destroyed in `rndClose`
  (`SDL_DestroyWindow`), which posts no `SDL_QUIT`. The clear therefore stays until the next quit. That quit's
  `SDL_QUIT` reaches `HandleEvent` with the flag clear: it sets the flag and returns without `WindowsCleanup`. The main
  loop still breaks on `SDL_QUIT`, so the process ends without the shutdown (rc 1).
- **The fix** (`HWI-FIX`, `src/SDL/main.c mainDestroyWindow`): right after the clear, `mainActuallyQuit = TRUE;`, which
  is what the destroyed window's WM_DESTROY does in the exe. The comment cites both addresses.
- **The fence.** The new self-test `/catModeQuitTest <W>x<H>` (`src/Game/UiScaleTest.c`, harness mode `modequit`,
  `--mode-to WxH`, default 1280x720). Once the front end is up, it performs `opOptionsAcceptHelper`'s render switch:
  `soundEventShutdown`, `mainSaveRender`, the new request, `mainLoadGL` (a real second window), and `mainRestoreRender`
  on a failure, then `soundEventRestart`. Then `utility.c` quits once.
  - The Video list's own selection is left out: the offscreen driver lists only its one 1024x768 display mode, so
    `opResHackSupported` would refuse any other size.
  - The harness judges the run: rc 0, and the debug build's `Quit game, baby!` exactly once.

| run | build | result |
|---|---|---|
| m3r2-modequit-before | without the fix | `switched ok=1 ... physical=1280x720`; **rc=1**; `Quit game, baby!` once (run.log:118) and no shutdown after it; LeakSanitizer 44 bytes in 11 allocations (the shutdown never ran) |
| m3r2-modequit-after, m3r2i-modequit | with the fix | **rc=0**, one quit line, no unsuppressed leak, `quit: PASS` |

The before-run's summary printed `"Quit game" lines: 2`. That miscount came from the done line's own wording, which
contained the words "Quit game"; the log has the quit line once (line 118). The summary now counts `Quit game, baby`
and the done line no longer contains the words. A quit after a UI-scale change was never affected: `uiScaleApply` makes
no new window. The uiscale self-test changes the scale and then quits, and `m3r2i-uiscale` / `-4k` both give rc 0.

## Files changed

- `src/Game/Instrument.c` / `.h` (new; HWI-FEATURE(instrumentation), HWI-TEST for /frameLog): the settings line, the
  overlay, the frame clocks and the frame log.
- `src/Game/meson.build`: adds `Instrument.c`. Both the Linux build and the win64 cross build compile it.
- `src/SDL/utility.c`:
  - `utyOptionsFileRead` calls `hwiSettingsRowsRead`;
  - the `/catModeQuitTest` hook sits next to `/catUiScaleTest`.
- `src/SDL/uiscale.c` / `.h`: `hwiSettingsWindow()` at the end of `uiScaleWindowCreated`; the new accessor
  `uiScaleFitRule()`.
- `src/SDL/main.c`:
  - the switches `/overlay`, `/frameLog <file>` and `/catModeQuitTest <W>x<H>` (table entries next to `/lodBands`);
  - the overlay key in `HandleEvent`;
  - the **HWI-FIX** in `mainDestroyWindow`.
- `src/SDL/render.c`: in `rndRenderTask`, `hwiFrameBegin`, `hwiOverlayDraw` and `hwiFrameEnd`; in
  `catLodTestMainView`, the `hwiWantVerts` gate and `hwiVertsDrawn`.
- `src/Game/Task.c` (`taskExecuteAllPending`): times each universe task call.
- `src/Game/Console.c`:
  - the `overlay` row, and `CAT_CON_HWI_ROWS` 5 to 6;
  - `cat_consoleRunCommand` case 5 announces HWI rows to the settings line.
- `src/Game/Options.c` (`opOptionsAcceptHelper`): `hwiSettingsAccepted()` after `uiScaleUpdate()`.
- `src/Game/MotionInterp.c` / `.h`: `miSwitchObjStands`, `miSwitchNisStands`.
- `src/Game/MotionTest.c` / `.h`:
  - `catMotionRealClockFps`;
  - the toggle self-test's overlay and settings-line checks (`mtToggleOverlay`).
- `src/Game/Memory.c` / `.h`: `memHwiUsage` (HWI-TEST).
- `src/Game/KeyBindings.c`: the keys self-test's overlay-key check.
- `src/Game/UiScaleTest.c` / `.h`: `/catModeQuitTest`.
- `tools/m2_harness.sh`:
  - mode `modequit`, `--mode-to`, `--frame-log`;
  - a `settings:` summary line in every run;
  - `modequit:` and `framelog:` summary lines;
  - the header documents all of it.
- `tools/frame_log_report.py` (new).
- `tools/play.sh`: the header documents the settings line and the overlay's three toggles for the user [audit: it did
  not document `/frameLog`, which THE USER'S CHECK below uses; the audit added the switch and the report tool to the
  header].
- `docs/features/high-refresh-4k.md`: the soak section.

Tag counts (`/bin/grep -rn`): **102** `HWI-FEATURE(instrumentation)` lines in `src/`, and one new `HWI-FIX`
(`mainDestroyWindow`). There are no `CAT:` lines: no Cataclysm behaviour was ported. Neither the exe nor GoK has the
instrumentation, and the quit fix is GoK's fault with the exe as its reference. No struct changed:
`tools/m2_layout32.sh` reports OK.

**Builds** (final tree): `tools/build.sh --debug` and `--release` give rc 0 and 0 warnings under `-Werror`. The win64
cross build (`ninja -C build/win64`, llvm-mingw) gives rc 0, 0 warnings, `hwi-engine.exe` 5,330,944 B. No `_WIN32` code
was touched.

**Delta map.** No review rows were added. Every function edited or relied on that has a map entry is already
MODIFIED (reviewed): `mainDestroyWindow`, `WindowProc`, `mainLoadGL`, `mainRescaleMainWindow`, `utyOptionsFileRead`,
`opOptionsAcceptHelper`, `taskExecuteAllPending`, `rndRenderTask`, `utyGameQuit`, `kbListWindowCB`.
`cat_consoleRunCommand` is Cataclysm-new and has no HW1 row. `utyTasksDispatch` (IDENTICAL) was neither edited nor
relied on.

**CAT-TODOs.** None added and none closed. The one `CAT-TODO(M3-R2)` that M3-R1 left (`BTG.c:1151`, the star size)
is the integrate's to re-target.

## (e) The fences: every run on the plain harness, debug build unless noted

| fence | run(s) | result |
|---|---|---|
| (a) settings line, fe / sp / mission | m3r2i-fe, m3r2i-sp, m3r2i-mission-a | each run's first line: `objInterpolate=1 (default) nisInterpolate=1 (default) letterboxBehind=0 (default) noLOD=1 (default) uiScale=0->100% (default, auto) overlay=0 (default) fit=window (as requested (no desktop bounds)) 1024x768 switches=none [start]` |
| (a) /noInterp | m3r2-set-noi | `objInterpolate=0 (switch) nisInterpolate=0 (switch) ... switches=/noInterp [start]` |
| (a) /noNISInterp | m3r2-set-nonis | `objInterpolate=1 (default) nisInterpolate=0 (switch) ... switches=/noNISInterp` |
| (a) /lodBands | m3r2-set-lodb | `noLOD=0 (switch) ... switches=/lodBands` |
| (a) /letterboxBehind | m3r2-set-lbb | `letterboxBehind=1 (switch) ... switches=/letterboxBehind` |
| (a) /uiScale 150 | m3r2-set-ui150 | `uiScale=150->150% (switch) ... switches=/uiScale 150` |
| (a) a --cfg row for each | m3r2-set-cfg (`objInterpolate 0`, `nisInterpolate 0`, `letterboxBehind 1`, `noLOD 0`, `uiScale 200`) | `objInterpolate=0 (cfg) nisInterpolate=0 (cfg) letterboxBehind=1 (cfg) noLOD=0 (cfg) uiScale=200->160% (cfg)` (200 % is clamped to 160 % at 1024x768, the 640x480 floor) |
| (a) at 4K with the user's rows | m3r2-set-0743b, m3r2-set-0743, m3r2-set-0815 | quoted above |
| (a) a change in a session | m3r2i-uiscale-4k | `[start]` with the defaults, then `objInterpolate=1 (cfg) ... uiScale=200->200% (cfg) [options accept]`, then `uiScale=150->200% (console) [console uiScale]`, then `uiScale=0->200% (console, auto) [console uiScale]`, then `[window]` |
| toggle (plain, /noInterp, /noNISInterp; plus /overlay, /lodBands) | m3r2-toggle, -toggle-noi, -toggle-nonis, -toggle-ov, -toggle-lodb; final m3r2i-toggle | all `result=PASS fails=0`. New lines: `toggletest: overlay start=0 switch=0 (expected 0) line=yes`, `overlay console 1 -> 1 lines=1 line=yes`, `overlay key (scancode 101, vk 0) -> 0 line=yes`, `settings line console letterboxBehind -> 1 line=yes`, `overlay end=0`; under /overlay `start=1 switch=1 (expected 1)` and `end=1` |
| (b) overlay 1024x768 | m3r2-ov-ai-on / -off (ai scene, `--lodstats 150-230:fleet`, `--shot 200`) | diff `differ=16064 bbox=2,2,253,66` (only the overlay's box); region 2,2,254,67: 16,380 px, 167 colours, 1,642 text pixels within 40 of the text colour 255,255,160. Read off the PPM (`ai1024.png`): `FRAME 62.02 MS MED 54.97 P99 60.57` / `TICK 1.62 MS X1 ALPHA -1.000` / `OBJ 1 NIS 1 LBB 0 NOLOD 1 SW /overlay` / `VERTS 25169 OBJECTS 164` / `FULL DETAIL ON UI 100%` (debug ASan frame times) |
| (b) overlay 3840x2160 (UI 200 %) | m3r2-ov-ai4k-on / -off | diff `differ=64256 bbox=4,4,506,132` (the same box at 2x); region 4,4,508,134: 65,520 px, 6,640 text pixels. Read off the PPM: `FRAME 54.98 MS MED 56.25 P99 65.10` / `TICK 1.53 MS X1 ALPHA -1.000` / `OBJ 1 NIS 1 LBB 0 NOLOD 1 SW /overlay` / `VERTS 25169 OBJECTS 164` / `FULL DETAIL ON UI 200%` |
| (b) overlay, HUD scene | m3r2-ov-hud-on / -off, m3r2-ov-hud4k-on / -off (`--shot 70,72`) | diffs `bbox=2,2,249,66` (1024) and `4,4,498,132` (4K) at both ticks. The headless hud scene is the sensors manager, so `VERTS -` |
| (c) frame log, 4K release | m3r2i-perf4k-fl, -fl2 | the table above |
| (d) the dumps | m3r2i-motion-noi, -nis-noi, -motion-on, -nis-on, -motion-optout, -nis-optout | **all six EQUAL in whole hash to M3-R1's:** motion-noi c503c869f0dc9cdb / 3016280e9e0dd3c8 (1,431 frames), nis-noi 86cb155acffc180e / 196bab32d244f614 (1,440), motion-on c0e411662fa2f527 / 428ef41479eb5749, nis-on 0307f3be2044c29c / a5de13a44b8d264f, optout motion 183681235a3fdc7b / 3016280e9e0dd3c8 and nis 13ca70eea998e5de / 196bab32d244f614 (= R10's /noInterp dumps) |
| (d) Mission01 pair | m3r2i-mission-a / -b; final m3r2i-fmission-a / -b; release m3r2i-mission-rel-a / -b | every one `IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference` against m3r1i-mission-a/run.hwo (1ae66610...), all 21 fields; 1,476,370 B, 154 ships; the release pair also against m3r1i-mission-rel-a |
| (d) with /overlay, with /frameLog | m3r2i-mission-ov, m3r2i-mission-fl | both IDENTICAL, 6001 ticks |
| (d) mission test with --teamcolour --frame-log | m3r2i-tc-fl | 6/6 PASS, `teamcolour: done ... failed=0`; IDENTICAL to m3r1i-missiontest-a over ticks 0-11424 |
| fe | m3r2i-fe | Main_game_screen, rc 124 at the 120 s timeout (as m3r1i-fe), ASan 0 |
| hud | m3r2i-hud | `hudtest: done ... result=PASS`, ASan 0 |
| console | m3r2i-console | `tablerows=486`, **shown=35 set=28** (M3-R1 34 / 27, plus the new `overlay` row), `result=PASS` |
| keys | m3r2i-keys (final build) | `keystest: overlay key scancode=101 vk=0 bindings on VK_APPS=0`; **rows=197 bindings=149**, conflicts=1 shares=1, `result=PASS` |
| fescreens | m3r2i-fescreens | `optionstest: ... result=PASS`, `fescreentest: files=22 ... result=PASS` |
| uiscale (UI-scale change, then quit) | m3r2i-uiscale, -uiscale-4k | 45/45 PASS, rc 0 |
| modequit | m3r2i-modequit | quit PASS, rc 0 |
| sp --timeout 1200 | m3r2i-sp | universe running, to Mission12 (FileLoads: Mission12_NIS07.mission and Mission12.btg; 12 animatics skipped, Animatic01 .. 12, as M3-R1's run reached); **ASan 0**, no unsuppressed leak; rc 124 (timeout) |
| sp with --frame-log, 600 s | m3r2i-sp-fl | to Mission06, ASan 0; 567 s logged (findings below) |

`harness_milestone` and first stop of the final runs: mission `oracle mission run done`, first stop none (rc 0);
sp `universe running`, none (still running at the 1,200 s timeout).

**Streams and dumps, before and after.** Nothing moved: every stream and dump above is IDENTICAL or EQUAL to its M3-R1
counterpart. No field and no first tick to state, and nothing for the integrate to re-baseline. The instrumentation
draws from no random stream. The overlay is off by default, so the dumps' `crc=` frames are unchanged. Only
`--shot`'s own frames differ, and only inside the overlay's box when `/overlay` is given.

**The final headless Mission01 run** is `m3r2i-fmission-a/run.hwo`: sha256 prefix **8d32ad5a04b0ff43** (debug build of
this tree), IDENTICAL in content to 1ae66610c3f02... The release run `m3r2i-mission-rel-a` is d47444b16a4d1cd9.

### Soak findings the frame log already shows (owner: M3 soak)

`m3r2i-sp-fl` is the debug build on the virtual sp loop, Mission01 to Mission06 in 567 s:
- The heap grew from 27.8 MB to 75.3 MB, +4.0 MB a minute (least squares).
- A **second growth heap** was made at s=317, so the 64 MB main heap plus 8 MB growth steps were exceeded.
- `dupe trcolorinfo list` went 0, then 152 (Mission02), 224, 293, then **912**.

O4's leak candidate now has a per-second counter. The frame periods drifted no more than 10 % (thirds p50 1.9 / 1.8,
p99 8.5 / 8.7). These are inputs for the soak round, not regressions: they are the same code as M3-R1's.

## THE USER'S CHECK (for the play-test brief)

1. **Start.** Run `tools/play.sh --rebuild` (or `tools/play.sh --rebuild -- /frameLog
   ~/hwi-frame-$(date +%H%M).log` for the soak's numbers). Before anything else, read the top of the newest
   `original/play-home/logs/play-*.log`. The `hwi-settings:` line should say `objInterpolate=1 (cfg)
   nisInterpolate=1 (cfg) ... noLOD=1 (cfg) uiScale=0->200% (cfg, auto) ... fit=fullscreen-desktop (display size ->
   fullscreen desktop) 3840x2160`. Report the line if it says anything else.
2. **The overlay.** In the front end, and again in Mission01 after the opening cutscene, press the keyboard's **Menu
   key** (the "application" key, usually right of the right Windows / Alt key). A dark panel should appear in the
   top-left corner. Press it again and the panel should go. Without a Menu key, start with `-- /overlay`. Report:
   - whether the key toggles it;
   - whether the five lines are readable at 200 %;
   - whether anything in the HUD is hidden under the panel;
   - the `FRAME ... MED ... P99` numbers during the Kuun-Lan close-up, and while the whole fleet is in view in
     Mission01 (FULL DETAIL on: above 6.94 ms means it misses 144 Hz, which is producer 4's work);
   - whether `ALPHA` moves between 0 and 1 in game (Smooth motion on), or shows -1.
3. **The quit after a resolution change.** In the front end, go to Options -> Video, pick another resolution, press
   Accept, and answer the "keep these settings" box with Yes. Back at the main menu, press Quit **once**. The game
   should close at the first try. Report whether it needed a second Quit. Afterwards, set the resolution back to
   3840x2160 the same way (or edit `screenWidth` / `screenHeight` in `Cataclysm.cfg`).
4. **Changing a setting in the session.** Untick and tick SMOOTH MOTION on Options -> Video and press Accept. The log
   should get a new `hwi-settings: ... [options accept]` line.

## For STATE.md

- **Decision 122 (proposed): the instrumentation.**
  - The settings line `hwi-settings:` is printed at start-up (when the window exists) and at every change. Sources are
    default / cfg / switch / console / options / key, and the `[why]` tag names the trigger.
  - The overlay is off by default. Its toggles are the console row `overlay` (`CAT_CON_HWI_ROW_ID + 5`,
    `CAT_CON_HWI_ROWS` 6), the run-only switch `/overlay`, and the **Menu key** (`SDL_SCANCODE_APPLICATION`: no VK
    code, so keys.cfg cannot bind it and the exe does not use it). It has no options-file row.
  - It is drawn at the top-left (logical 4,4) at the UI scale with `default.hff`.
  - `/frameLog <file>` is HWI-TEST, run-only; the harness flag is `--frame-log`; the report tool is
    `tools/frame_log_report.py`.
  - FRAME is the period between two ends of the render task (after the flip); RND is the render task alone; neither
    uses glFinish. /catLodStats' ms is RND plus glFinish.
  - Producers add settings by appending a row to `hwiSettings[]` (Instrument.c).
- **Decision 123 (proposed): the quit after a resolution change.** HWI-FIX in `main.c mainDestroyWindow`: the clear is
  consumed at once, as the exe's window's WM_DESTROY consumes it (`mainDestroyWindow 0x00560350`, `WindowProc
  0x00561f10` case 2). The new self-test `/catModeQuitTest` (harness mode `modequit`) judges rc 0 and one quit.
- **The console's counts** are now shown 35 / set 28 (tablerows 486). The keys counts stay 197 / 149.
- **Harness additions:** mode `modequit`, `--mode-to WxH`, `--frame-log`, the `settings:` summary line in every run.

## What is left, and its owner

- **The attended check above** (the overlay's look and legibility, the key, the quit at 4K on the user's desktop).
  Owner: the play-test.
- **The heap's growth over the sp loop, the second growth heap and the `dupe` blocks (912 by Mission06).** Owner: the M3
  soak (O4). The counter is in place.
- **FULL DETAIL's 4K frame time (10.0 ms against 6.94 ms).** Owner: M3-R2 producer 4. The frame log is its measurement
  instrument.
- **The `options` source** labels any change that no hook announced (a checkbox before Accept, the frame-rate
  presets). If producers 2-4 add a setting that changes outside the Options screen or the console, they should call
  `hwiSettingsChanged("<why>")` and set a source.
- **The toggle self-test's mid-test lines** show `objInterpolate=1 (switch)` under `/noInterp` after the letterbox
  sub-test. That sub-test reads the whole options file back through `utyOptionsList`, which sets the flags to the
  file's values while the switch stands. This is test-only state juggling, not a product path; the line reports the
  state faithfully. No owner needed.
