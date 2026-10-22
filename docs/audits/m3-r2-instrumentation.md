<!-- done: build, key, settings, exe, overlay, framelog, dumps, streams, selftests, modequit, diff, fixes, final-runs -->
# Audit: M3-R2 producer 1, instrumentation (the settings line, the overlay, /frameLog, the quit after a mode switch)

Auditor's scope: the producer's report docs/m3-r2-instrumentation.md and `git diff 06dae5c` in the m2 worktree. The
commits after 06dae5c (f47d384, 944565c, 27e159d, 9f6e6a7) are the orchestrator's (cycle/answers, the workflow script,
STATE.md): the whole port is the working tree, 21 tracked files changed and 4 new. Every engine run went through
tools/m2_harness.sh (silent, headless, offscreen), labels `m3r2a-ins-*`, `--bin` pointing at the audit's clean builds
(`original/scratch-m2/build-audit-m3r2-instrumentation/`) or, for the `m3r2a-ins-f*` runs, the tree's own
build/debug after the audit's fix. No gamescope, no input injected (the overlay key reached `HandleEvent` as the toggle
self-test's in-engine SDL_Event), no Ghidra. cycle/answers/m3-r2-user.md was read first and again before this report:
nothing in it changes this task (the GPU note of 11:22 applies; the GPU was idle at the measurement, below).

## 1. Builds (clean, separate directory)

meson set up exactly as tools/build.sh does (debug `--buildtype=debug -Db_sanitize=address,undefined`; release
`-Doptimization=2 -Ddebug=false -Db_sanitize=none`; werror=true, warning_level 3 in both, read from
meson-info/intro-buildoptions.json), plus the win64 cross build exactly as tools/release_win64.sh sets it up:

| build | tree | rc | compiler warnings | bytes |
|---|---|---|---|---|
| debug/ | producer's | 0 | 0 | 89,800,488 |
| release/ | producer's | 0 | 0 | 5,716,344 |
| win64/ (llvm-mingw, -Dmovies=false) | producer's | 0 | 0 | 5,331,456 (`src_Game_Instrument.c.obj` compiled: meson.build carries it) |
| win64/ (incremental) | after the audit's fix | 0 | 0 | 5,330,944 |
| tools/build.sh --debug / --release (build/) | after the audit's fix | 0 / 0 | 0 / 0 | 89,785,096 / 5,716,384 |

`tools/m2_layout32.sh`: OK (all ILP32 layout asserts hold). No `_WIN32` code was touched (the diff has none); no GL
entry point was added.

## 2. The overlay key

- **The exe** (tools/m2_fdec.py / m2_fdis.py / m2_exedata.py): `WindowProc 0x00561f10` WM_KEYDOWN (0x100) gives EVERY
  virtual-key code to `keyPressDown` and `cat_consoleKeyPress`; there is no VK_APPS (0x5d) case. `cat_kbKeyToString
  0x0049b590` returns NULL only for a code of 0x100 or more (0x49b5a2 `jge 0x49b7d6` -> `xor eax,eax`), unreachable
  after its `and ecx,0xff`; for 0x5d it falls through to the language table (0x008b0ca8 + 2*key = 0x008b0d62: 0, 0) and
  `sprintf("%c", key)` = `]`. `kbListWindowCB 0x0049bca0` shows Invalid_Key only on a NULL name. So the exe's
  key-binding screen WOULD bind the Menu key (named `]`), and a keys.cfg row with 0x5d would load.
- **The tree**: `Key.c catKeyVkTable` has no 0x5d (nor F13-F24 or media keys), so `catKeyVkFromKey(101)` = 0,
  `catKeyFromVk(0x5d)` = 0: this engine's keys.cfg cannot bind it and its binding screen refuses it. `default_keys.cfg`
  binds nothing to 0x5d: the keys self-test (`m3r2a-ins-keys`) prints `keystest: overlay key scancode=101 vk=0
  bindings on VK_APPS=0`, rows=197 bindings=149, result=PASS. No key index of the tree is 101 besides
  `SDL_SCANCODE_APPLICATION` (`/bin/grep`), and `HandleEvent` still calls `keyPressDown` for it.
- **Judgement**: the choice stands (no default binding in the exe, not bindable in the tree, nothing in the tree reads
  key 101). The report's reason "It belongs to no part of cataclysm.exe's key handling" was WRONG (finding W1, fixed in
  the report, the play.sh header and Instrument.h's comment). The tree's refusal of VK_APPS is a pre-existing gap of
  M2-R6's table, not the exe's rule; if a later port completes the table, the overlay key would share the key with a
  user binding (finding F8, owner: whoever ports the key table; no action this round).

## 3. The settings line: sources against the code, with runs

Code: `default`/`cfg` from `hwiSettingsRowsRead` (called in `utyOptionsFileRead` right after `scriptSetFileSystem`, a
second read-only parse of the same `Cataclysm.cfg` the exe reads: `utyOptionsFileRead 0x005a83a0` calls
`scriptSetFileSystem(.., "Cataclysm.cfg", utyOptionsList)`); `switch` from `miSwitchObjStands`/`miSwitchNisStands`
(new), `rndLetterboxSwitchStands`, `catLodSwitchStands`, `uiScaleSwitchStands`, `/overlay`; `console` from
`cat_consoleRunCommand` case 5 for an HWI row (id >= 0x1000; the exe's 0x0045b520 has no such rows); `cfg` at Accept
from `opOptionsAcceptHelper`; `key` from `hwiOverlayKey`; `options` from the per-frame poll. Runs (first line quoted,
`fit=window (as requested (no desktop bounds)) 1024x768` unless shown):

| run | switch / rows | the line |
|---|---|---|
| m3r2a-ins-fe, -mission-a/-b, -sp, -keys, -hud, -toggle | none | `objInterpolate=1 (default) nisInterpolate=1 (default) letterboxBehind=0 (default) noLOD=1 (default) uiScale=0->100% (default, auto) overlay=0 (default) ... switches=none [start]` |
| m3r2a-ins-set-noi | /noInterp | `objInterpolate=0 (switch) nisInterpolate=0 (switch) ... switches=/noInterp` |
| m3r2a-ins-set-nonis | /noNISInterp | `objInterpolate=1 (default) nisInterpolate=0 (switch) ... switches=/noNISInterp` |
| m3r2a-ins-set-lodb | /lodBands | `noLOD=0 (switch) ... switches=/lodBands` |
| m3r2a-ins-set-lbb | /letterboxBehind | `letterboxBehind=1 (switch) ... switches=/letterboxBehind` |
| m3r2a-ins-set-ui150 | /uiScale 150 | `uiScale=150->150% (switch) ... switches=/uiScale 150` |
| m3r2a-ins-set-ov, -mission-ov | /overlay | `overlay=1 (switch) ... switches=/overlay` |
| m3r2a-ins-set-cfg | --cfg objInterpolate 0, nisInterpolate 0, letterboxBehind 1, noLOD 0, uiScale 150 | `objInterpolate=0 (cfg) nisInterpolate=0 (cfg) letterboxBehind=1 (cfg) noLOD=0 (cfg) uiScale=150->150% (cfg) overlay=0 (default) ... switches=none` |
| m3r2a-ins-set-0743 | the user's 07:43 rows (Cataclysm.cfg.bak-20261021 rows 5/97/98/101/107-110 read) + `/testUsableBounds 3840x2160@3840x2160` | `objInterpolate=0 (cfg) nisInterpolate=0 (cfg) letterboxBehind=0 (cfg) noLOD=1 (cfg) uiScale=0->100% (cfg, auto) overlay=0 (default) fit=fullscreen-desktop (display size -> fullscreen desktop) 1024x768` (the offscreen display) |
| m3r2a-ins-set-0815 | the 08:15 rows, --res 3840x2160 | `objInterpolate=1 (cfg) nisInterpolate=1 (cfg) letterboxBehind=0 (cfg) noLOD=1 (cfg) uiScale=0->200% (cfg, auto) ... 3840x2160` |
| m3r2a-ins-uiscale-4k | changes in a session | `[start]` defaults, then `... (cfg) uiScale=200->200% (cfg) [options accept]`, `uiScale=150->200% (console) [console uiScale]`, `uiScale=0->200% (console, auto) [console uiScale]`, `[window]`; 45/45 PASS |
| m3r2a-ins-console | the console self-test | 9 lines, each `[console <row>]` with `(console)` (noLOD=7 / uiScale=7 are the test's own values) |
| m3r2a-ins-toggle / -noi / -nonis / -ov | the toggle self-test | 15 / 15 / 15 / 16 lines; `[console ..]` and `[key overlay]` lines with `(console)` / `(key)` |

The user's 07:43 log's own lines 9-10 (`window: ... created 3840x2160 (fullscreen-desktop): display size ->
fullscreen desktop`, `uiscale: ... 200% (auto, uiScale 0)`) and the backup's rows agree with the report's
reconstruction of what the 07:43 / 08:15 lines would have said.

Code-read finding F5 (PLAUSIBLE, not run: no self-test spans two frames of the Options screen): after Options ->
Cancel, `opRestoreSavedSettings` puts a changed value back and the next frame's poll labels it `(options)` ("an Options
control, not yet accepted") although it is the file's row again (Instrument.c `hwiSettingsPrint`: a value change with
no pending source is HWI_SRC_OPTIONS). Cosmetic; a `hwiSettingsChanged`-style hook in `opRestoreSavedSettings` would
label it. Owner: the integrate's choice (left as is).

## 4. Exe bodies compared (8 touched Cataclysm bodies, 3 IDENTICALs)

| function | exe | finding |
|---|---|---|
| mainDestroyWindow | 0x00560350: `DAT_008b85b0 = 0; DestroyWindow(ghMainWindow)` | the HWI-FIX's reasoning holds: DestroyWindow sends WM_DESTROY synchronously |
| WindowProc (HandleEvent) | 0x00561f10: message 2 -> flag 0: set back to 1, no quit; 0x100: keyPressDown + console for every key | the fix reproduces case 2; the key insert is additive, keyPressDown still runs (section 2) |
| rndRenderTask | 0x0057a910 (MODIFIED, reviewed) | three pure insertions; the overlay draws only when on, before catShotFrameEnd / catMotionTestFrameEnd |
| taskExecuteAllPending | 0x0052a840 (MODIFIED, reviewed) | timing around the universe task's call only |
| utyOptionsFileRead | 0x005a83a0 (MODIFIED, reviewed) | a second read-only parse of the same Cataclysm.cfg |
| opOptionsAcceptHelper | 0x004d76d0 (MODIFIED, reviewed) | one call after uiScaleUpdate; values are already set by the checkbox clicks (toggle self-test: a click sets both flags) |
| cat_consoleRunCommand | 0x0045b520 (Cataclysm-new, no HW1 row) | hook only for ids >= 0x1000, rows the exe's table lacks |
| utyGameSystemsInit | 0x005abe70 (MODIFIED, reviewed) | an HWI-TEST hook behind /catModeQuitTest (the producer did not list it; MODIFIED, so no review row needed) |
| keyPressDown (IDENTICAL) | 0x0049ae40 | identical within its M2-R6 review note (keyBufferAdd inlined, the keystick bits) |
| fontMakeCurrent, fontHeight (IDENTICAL) | 0x005580d0, 0x005587f0 | identical |

No edited function is IDENTICAL in ghidra/delta_map.json; no review row was owed (the producer's claim holds).

## 5. The overlay from the pixels

`m3r2a-ins-ov-on/-off` (ai, --ai-ticks 240, --lodstats 150-230:fleet, --shot 200) and `m3r2a-ins-ov4k-on/-off`
(--res 3840x2160, UI 200 %):
- tools/ppm_region.py diff: `differ=16064 bbox=2,2,253,66` and `differ=64256 bbox=4,4,506,132` (the producer's figures).
- An independent reader (P6 parse, per-pixel compare): 1024x768 differ=16,064 bbox (2,2)..(252,65) inclusive = 251 x
  64 = 16,064: every pixel of the box differs and none outside; 3840x2160 differ=64,256 bbox (4,4)..(505,131) = 502 x
  128: exactly 2 x in each axis, drawn at the UI scale. Inside: 1,636 / 6,548 text pixels within 40 of (255,255,160),
  13,395 / 53,524 darkened (on <= off in every channel), the rest the glyphs' edges.
- Read off the crops (original/scratch-m2/audit-m3r2-ins-shots/ov1024.png, ov4k.png): `FRAME 67.37 MS MED 60.09 P99
  77.52` / `TICK 1.58 MS X1 ALPHA -1.000` / `OBJ 1 NIS 1 LBB 0 NOLOD 1 SW /overlay` / `VERTS 25169 OBJECTS 164` /
  `FULL DETAIL ON UI 100%`; at 4K the same five lines with `UI 200%` (debug ASan frame times). Legible at both sizes.

## 6. /frameLog and tools/frame_log_report.py

- **Release 4K measurement** (`--mode ai --ai-ticks 500 --res 3840x2160 --lodstats 150-450:fleet --frame-log`, the
  clean release binary). GPU: nvidia-smi 1 % (idle samples, 4 % at the call) at 12:12:10, 5 % after each run
  (12:12:19, 12:12:27); no game running (no non-Steam .exe process); load average 5.19 / 4.99 (the producer's: 8.8-9.3).
  `claude-gpu-done` run afterwards.

| run | lodstats 150-450 | frame log (report, whole run) | vs the producer |
|---|---|---|---|
| m3r2a-ins-perf4k-fl | median 9.916, p99 12.686, max 13.180, 25,169 verts | ft p50 10.2, p95 12.3, p99 13.8; 485 of 500 over 6.94 ms; 6/6 seconds over budget | median -1.1 %, p99 -15.2 %; ft p50 -1 %, p99 -17 % |
| m3r2a-ins-perf4k-fl2 | median 9.451, p99 12.057, max 12.418 | ft p50 9.7, p95 11.9, p99 12.8 | median -4.8 %, p99 -16.9 %; ft p50 -7 %, p99 -18 % |

  The medians agree within 7 %; the p99s are 15-18 % LOWER, explained by the lower CPU load (5.0-5.2 against 8.8-9.3:
  the tail is CPU contention; a p99 of 301 frames is its 3rd-largest frame) and in line with M3-R1's 11.389 / 13.689.
  FULL DETAIL at 4K stays above 6.94 ms (producer 4's).
- **Hand-made logs** (scratchpad mklog.py: 2,400 s at 144 frames a second): (a) the frame period drifting 5.0 -> 8.0 ms
  and the heap growing 1,048,576 B a minute: `drift (10 min): first p50 5.4 p99 6.5, last p50 7.7 p99 8.8 ... FAIL`,
  `+1048576 bytes a minute (least squares)` (exact), 1,644 seconds over budget; (b) flat: drift PASS, +0 B a minute.
  The drift is flagged; the heap growth is MEASURED exactly but carries no verdict (finding F6: the tool prints the
  rate and the growth heaps, no PASS / FAIL line; the soak's threshold is the soak's to set).
- Finding F7 (minor): a harness run's last second holds the quit call (`tick_max` 1,200 ms, `oracle: mission run
  done` and the shutdown), so a SHORT run's thirds drift test can FAIL on it (`m3r2a-ins-perf4k-fl2`: 5 seconds,
  drift(thirds)=FAIL from s=5's 1,211 ms frame). The producer explained the frame; the report tool does not exclude
  it. Harmless for an hour's soak with 10-minute windows.
- Finding W2 (fixed): the header's `build=` was `UNSUPPORTED_BUILD` in every log (`m3r2a-ins-perf4k-fl`,
  `-mission-fl`): `-DHOMEWORLD_BUILD_VERSION` is given only to src/SDL (src/SDL/meson.build:37), and
  src/Game/Instrument.c got mainswitches.h's fallback. Fix: `main.c hwiBuildVersion()` (HWI-TEST), declared in
  Instrument.h, used by `hwiLogHeader`; `m3r2a-ins-fix-fl` (build/release) header: `build=9f6e6a7+ release
  commit=9f6e6a73`.
- `m3r2a-ins-mission-fl` (debug, 6,000 ticks): 12 seconds, heap_used 41,813,504 constant (the direct-start Mission01
  allocates nothing net), growth_pools 1, dupe 0. The producer's sp-fl log shows the counter moving (27.8 -> 75.3 MB,
  dupe 0 -> 912), so a constant mission figure is not a dead counter.

## 7. The quit after a resolution change

| run | build | result |
|---|---|---|
| m3r2a-ins-modequit | audit clean debug (with the fix) | `switched ok=1 ... physical=1280x720`; rc=0; `Quit game, baby!` once (run.log:118); no unsuppressed leak; `quit: PASS` |
| m3r2a-ins-modequit-before | a scratch worktree (`git worktree add` of HEAD under original/scratch-m2/audit-m3r2-ins-revert, the working tree's src diff applied, the one `mainActuallyQuit = TRUE; // HWI-FIX` line commented out, built clean; removed afterwards) | rc=1; the quit line once and no shutdown after it; LeakSanitizer `44 byte(s) leaked in 11 allocation(s)`; `quit: FAIL` |

The fix is shown necessary and sufficient on the self-test's path. The user's path (opOptionsAcceptHelper:
soundEventShutdown, mainSaveRender, opGLCStop, the request, mainLoadGL -> mainShutdownGL -> rndClose +
mainDestroyWindow) reaches the same mainDestroyWindow; the self-test leaves out opGLCStop/Start and the countdown box.

## 8. Streams and dumps (the next port's reference)

| stream / dump | run | hash (first 16 hex; dumps whole / crc-stripped) | against M3-R1 |
|---|---|---|---|
| Mission01 6,000 ticks (debug, audit clean build) | m3r2a-ins-mission-a / -b | 8d32ad5a04b0ff43 / 8d32ad5a04b0ff43 (1,476,370 B) | IDENTICAL: 6001 ticks, every field, vs m3r1i-mission-a/run.hwo (1ae66610...); a = b byte for byte |
| Mission01 with /overlay | m3r2a-ins-mission-ov | 8d32ad5a04b0ff43 | IDENTICAL |
| Mission01 with /frameLog | m3r2a-ins-mission-fl | 8d32ad5a04b0ff43 | IDENTICAL |
| **final pair** (build/debug after the audit's fix) | m3r2a-ins-fmission-a / -b | **8d32ad5a04b0ff43** / 8d32ad5a04b0ff43 | IDENTICAL: 6001 ticks and 0 packets, no difference, every field (rng_all included) |
| missiontest Mission01 | m3r2a-ins-missiontest-a | 02a825c6fd6f890f | IDENTICAL to m3r1i-missiontest-a, ticks 0-11425 (6/6, done 11,424) |
| ai 400 | m3r2a-ins-ai-a | 6f4d9290c07e7e71 | IDENTICAL, ticks 0-400 |
| loadgame "01 - Hiigara" 3,600 | m3r2a-ins-loadgame-a | a9c7ae7bbb022bfb | IDENTICAL, ticks 1984-5586 |
| skirmish LiveFireExercise2 | m3r2a-ins-skirmish-lfe2-a | dd4ca17339c2693a | IDENTICAL, ticks 0-600 (91 ships) |
| motion /noInterp | m3r2a-ins-motion-noi, final m3r2a-ins-fmotion-noi | c503c869f0dc9cdb / 3016280e9e0dd3c8 (1,431 frames) | EQUAL byte for byte (cmp) |
| nis /noInterp | m3r2a-ins-nis-noi | 86cb155acffc180e / 196bab32d244f614 (1,440) | EQUAL |
| motion on | m3r2a-ins-motion-on | c0e411662fa2f527 / 428ef41479eb5749 | EQUAL |
| nis on | m3r2a-ins-nis-on | 0307f3be2044c29c / a5de13a44b8d264f | EQUAL |

No stream field and no dump moved; nothing to re-baseline. Self-tests: toggle (plain, /noInterp, /noNISInterp,
/overlay; final m3r2a-ins-ftoggle) all `result=PASS fails=0`, with `overlay start=0|1`, `overlay console 1 -> 1
line=yes`, `overlay key (scancode 101, vk 0) -> 0 line=yes`, `overlay end=0|1`; fe Main_game_screen rc 124 at 120 s,
ASan 0; hud PASS; console `shown=35 set=28` (m3r1i-console 34 / 27) PASS; keys PASS (above); uiscale-4k 45/45 PASS; sp
--timeout 1200 (m3r2a-ins-sp): universe running to Mission12 (Mission12_NIS07.mission, Mission12.btg), 12 animatics
skipped (Animatic01 .. 12), ASan 0, no unsuppressed leak, rc 124. Every run listed: ASan errors 0.

## 9. The diff

- Scope: every changed file belongs to the instrumentation (its report's list); nothing outside it.
- Tags: 102 `HWI-FEATURE(instrumentation)` lines (`/bin/grep -rn src`); the new file tags its functions as M3-R1's
  uiscale.c does; test lines HWI-TEST; one HWI-FIX with both exe addresses. No `CAT:` line was owed (no Cataclysm
  behaviour ported). No CAT-TODO / CAT-PROVISIONAL added or touched; the one `CAT-TODO(M3-R2)` is M3-R1's BTG.c:1151
  (the integrate's to re-target).
- No generated file; no struct change; Instrument.c is in src/Game/meson.build and compiles in the win64 cross build.
- tools/m2_harness.sh: mode `modequit`, `--mode-to`, `--frame-log`, the `settings:` / `modequit:` / `framelog:` summary
  lines, documented in its header.
- Finding W3 (fixed): tools/play.sh's header documented the settings line, the key and `/overlay`, but not
  `/frameLog`, which the report's USER'S CHECK tells the user to pass; the audit added the switch and the report tool
  there, and corrected "cataclysm.exe has no use for that key".
- Finding W4 (fixed in the report): "Every other key has a VK code in the table" is too broad (F13-F24, media keys).

## 10. THE USER'S CHECK (checked)

`tools/play.sh --rebuild` and `-- /frameLog <path>` exist (play.sh `--rebuild` at its option parse; `/frameLog` in
main.c's table); the Menu key, `-- /overlay`, Options -> Video's resolution list with Accept and the keep-settings
countdown (opCountdownBoxStart), and the SMOOTH MOTION checkbox exist. Step 4 holds: a click sets the flags at once, the
next frame prints `(options) [frame]`, and Accept prints `[options accept]` with `(cfg)` because the source changed. The
mission and moment (Mission01 after the opening cutscene; the Kuun-Lan close-up in Mission01_NIS01) are the play-test's.

## 11. Precision, fixes

Sampled checks: 36 (builds 3; key 2; settings-line runs 12; exe bodies 3 claims; overlay 3; frame log 4; dumps 4;
Mission01 pair and variants 3; self-tests 6 (toggle, fe, hud, console, keys, uiscale); modequit 2; sp 1; delta map /
tags / CAT-TODO / layout 4; play.sh documentation 1; "every other key" 1; user's check 1) minus overlaps = 36 counted.
Wrong: 4 (W1 the Menu key "belongs to no part of cataclysm.exe's key handling"; W2 the frame log header's `build=`;
W3 play.sh without `/frameLog`; W4 "every other key"). **Precision = 32 / 36 = 0.89.** Not counted as wrong claims:
F5 (the Cancel label, PLAUSIBLE), F6 (no heap verdict), F7 (the quit frame in a short drift test), F8 (the tree's
VK_APPS gap, pre-existing).

Fixes (5, build green after them: tools/build.sh --debug / --release rc 0, 0 warnings; win64 rc 0, 0 warnings; the
final pair and dumps above on the fixed build):
1. src/SDL/main.c `hwiBuildVersion()` + src/Game/Instrument.c/.h: the frame log's `build=` (W2).
2. tools/play.sh header: `/frameLog` and the report tool documented; the key's wording (W3, W1).
3. src/Game/Instrument.h: the key comment corrected, and its section name ("Why the Menu key") (W1).
4. docs/m3-r2-instrumentation.md "Why the Menu key" (W1) and the build note in (c) (W2), marked [audit: ...].
5. docs/m3-r2-instrumentation.md "Every other key" and the play.sh line in Files changed (W4, W3), marked [audit: ...].

Verdict: PASS-WITH-FIXES - every fence reproduced on clean builds (settings line with every source, overlay confined to its box at 1024x768 and 4K and drawn at the UI scale, the four dumps EQUAL, Mission01 IDENTICAL also with /overlay and /frameLog, guard rails IDENTICAL, the quit fix shown failing without it, sp to Mission12 ASan 0); 4 factual errors fixed (the frame log's build field, play.sh lacking /frameLog, the Menu key's exe claim, an overbroad key claim)
