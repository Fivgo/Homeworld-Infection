<!-- done: builds, code+exe, scans, forced cases, fences, real clock, verdict -->
# Audit: M3-R2 producer 2, the cutscene's one-frame flicker (key nis-flicker)

Auditor's scope: `git diff bbdc613` in the m2 worktree (the WIP snapshot 641a9be plus the working tree; 1e7cffa,
e44dfeb and ae6fe85 are the orchestrator's cycle/answers and STATE.md commits, not port work). The producer's report:
docs/m3-r2-nis-flicker.md. Claim: **the flicker is not reproduced, there is no engine fix**, and a per-frame signal
(`/catFrameSig`), a virtual frame rate for any mode (`/catVirtualFps`), a scan tool (tools/dump_outliers.py) and
harness flags (`--frame-sig`, `--virtual-fps N`) are added, all HWI-TEST.

cycle/answers/m3-r2-user.md was read first and again before this report: no answer to the flicker hints (cut or
pan, which cutscene). The later entries (the GPU is free, the overlay key goes to F1-F12 in M3-R3, the overlay reads
well at 200%) do not touch this item.

## 1. Builds (clean, separate dir)
`original/scratch-m2/build-audit-m3r2-nis-flicker/`, meson setup exactly as tools/build.sh does:

| build | rc | warnings | size |
|---|---|---|---|
| debug (`--buildtype=debug -Db_sanitize=address,undefined`) | 0 | 0 (meson's sanitizer notice only) | 89,855,832 B |
| release (`-Doptimization=2 -Ddebug=false -Db_sanitize=none`) | 0 | 0 | 5,721,544 B |
| win64 cross (the release_win64.sh options, llvm-mingw) | 0 | 0 | hwi-engine.exe 5,337,088 B |

No new source file, no new GL entry point, no `_WIN32` code touched. `SDL_GetPerformanceCounter` / `fopen` only.

## 2. The cause: the code and the exe
Read: render.c `rndRenderTask` and `rndMainViewRenderFunction`, mainrgn.c `mrRegionDraw` and `cat_mrEscape`, NIS.c
`nisUpdateTask`, KASFunc.c `kasfWideScreenInBody` / `kasfWideScreenOut`, every `GL_SCISSOR_TEST` site in src/.

- **The frame order** is as the report says: the BTG (`rndBackgroundRender`, render.c ~4180) and the worlds first,
  then the visible list and the objects (opaque pass from render.c 4275), and the late bars in `rndRenderTask` after
  `regFunctionsDraw`. A frame showing the backdrop with neither objects nor bars therefore needs the object passes AND
  the late-bar branch to be skipped in one frame, or an unfinished frame to be presented.
- **The late-bar condition** `nisScissorFade != 0 || (nisFullyScissored && !rndScissorEnabled)` is the exe's
  (m2_fdec rndRenderTask 0x0057a910: `DAT_00a4cb44 != 0 || (DAT_00a4cb58 != 0 && DAT_00a75d84 == 0)`,
  `DAT_00a75d84 = glIsEnabled(GL_SCISSOR_TEST)` read before `regFunctionsDraw`). The exe's flip skips
  (`DAT_00a74d50` reinit countdown, `DAT_008df988` animatic count, `DAT_00a3e144` front-end no-flush) are the port's
  `mainReinitRenderer`, `animaticJustPlayed`, `feDontFlush`; the exe's conditional clear (`DAT_008b6d94`) is off only
  inside `binkPlay` 0x00551840. **Confirmed.**
- **`rndScissorEnabled` is FALSE at every frame's top**: the only enables inside a frame are mainrgn.c 5452 / 5504
  (inside `regFunctionsDraw`) and the paired ShipView / TradeMgr / ConsMgr sites; render.c 5259 disables it after the
  bars; nothing after that enables it. **Confirmed.**
- **The state writers**: nisUpdateTask 0x004cc600 matches NIS.c line for line (fade-out, black fade, fade-in with
  the KAS finish, the NIS end with `catNisKeepScissored` = `DAT_00a4cacc`); kasfWideScreenIn 0x00497420 cancels a
  running fade-out by zeroing time / out / fade as `kasfWideScreenInBody` does; kasfWideScreenOut 0x00497630 likewise.
  The bars are therefore absent only on an NIS's first tick (under the NIS's black fade: every scan shows `fd=1.000`
  from the first NIS frame of n01, nislet01, n06 and the NIS-first start) and on a KAS Out->In cancel (objects drawn).
  **Confirmed.**
- **Verdict on the cause**: no engine path found or known draws the backdrop with neither objects nor bars during a
  cutscene; the producer's remaining candidate (a frame presented before the GPU finished it) is outside the engine
  and cannot be produced headless. The report's reasoning holds, with one factual error (below, F1).

### Bodies compared with the exe (12; tools/m2_fdec.py / m2_fdis.py / m2_exedata.py, no Ghidra)
| function | address | map | result |
|---|---|---|---|
| rndRenderTask | 0x0057a910 | MODIFIED | bar test, clear, flip skips as above; the differences are the recorded ones |
| nisUpdateTask | 0x004cc600 | MODIFIED | as recorded (the KAS finish ported); line for line |
| nisScissorBarsReset | 0x004cd260 | IDENTICAL | the same 7 globals zeroed |
| nisScissorOut | 0x004d02d0 | IDENTICAL | sound 0x124, fade-out = param, time 0 |
| kasfWideScreenIn | 0x00497420 | MODIFIED | as recorded |
| kasfWideScreenOut | 0x00497630 | MODIFIED | as recorded |
| rndDrawScissorBars | 0x0057a410 | MODIFIED | as recorded (the CAT-FIX on the restored blend state stands) |
| nisCameraFlyCompute | 0x004cc4c0 | IDENTICAL | the exe inlines nisPause(FALSE) (`DAT_00a4cac8 = 0`), cut time 0, stage2 -1 |
| nisCameraCut | 0x004d0170 | IDENTICAL | `nisSeeking` (`DAT_00a4cb5c`) guard, object then camera paths x6, time += |
| nisPause | 0x004ce5f0 | IDENTICAL | one store |
| nisCameraCutSet | 0x004d24d0 | IDENTICAL | strupr, "%f FRAME" x 1/30 |
| nisFadeToSet | 0x004d30b0 | IDENTICAL | as the source; the exe's `/ 100` and `/ 30` are MSVC's reciprocal multiplies (0x3c23d70a = 0.01f at 0x0072a5b8, 0x3d088889 = 1/30 at 0x0072ab58), not a body change |

No body found modified against its map row; no review row needed. The producer added none and changed no
IDENTICAL body.

## 3. The scans, rerun from the clean build
All through tools/m2_harness.sh (offscreen, silent).

| run | what | frames | dump_outliers |
|---|---|---|---|
| m3r2-nfa-sig-n01 | nis n01:0:999, 144 virtual, debug | 16,084 | 0 outliers, 0 short runs |
| m3r2-nfa-sig-nislet01 | nislet01:0:999 | 15,553 | 0 / 0 |
| m3r2-nfa-sig-n06 | n06:0:999 | 9,505 | 0 / 0 |
| m3r2-nfa-mt144 | missiontest NIS-first, `--virtual-fps 144`, release, ticks to 4,899 | 60,031 | 0 / 0 |
| m3r2-nfa-sig-mt | missiontest NIS-first, 1 frame a tick, debug | 13,197 | 1 / 1: frame 3483 (tick 1712, fd=1.000: nislet01's last fade frame) and run 3489-3499 (ticks 1718-1728, the arrival's white flash, lum 183.95, bars drawn): the producer's two scripted hits exactly |
| m3r2-nfa-mission-sig | Mission01 6,000 ticks with `--frame-sig` | 6,001 | 0 / 0 |

Frame counts equal the producer's. The early producer files scanned by an older tool (sig-n01/-nislet01/-n06,
sig60-*, rc-*, mt144) rescanned with the final dump_outliers.py: 0 outliers and 0 short runs each.
Cut coverage recounted from my dumps' eye column (jumps over 300 units): n01 12 (ticks 256, 384, 544, 661, 820, 916,
1002, 1082, 1162, 1232, 1354, 1449), nislet01 10, n06 8: as the report says. The producer's real-clock alpha counts
(1,012 / 1,010 / 992 distinct; 1,781 / 1,711 / 1,045 over 1) are the dumps' `alpha=` at 3 decimals (exact: 12,441 /
11,959 / 7,450); the late counts are exact. The totals (334,111 over 17 runs; 450,644 over 22) add up.

### The real clock (release, `--real-clock 144 --frame-sig`, no read-back)
| run | frames | dump_outliers | clock (nis_smoothness.py) |
|---|---|---|---|
| m3r2-nfa-rc-n01 | 15,357 | 0 outliers, 0 short runs | alpha 0.0919..1.1479, 1,794 frames with raw >= 1 |
| m3r2-nfa-rc-nislet01 | 14,844 | 0 / 0 | alpha 0.0901..1.4434, 1,747 frames with raw >= 1 |

GPU: the user's FFXIV (ffxiv_dx11.exe) held the GPU at 91-99 % from 09:33; claude-ping was sent once (09:45) and the
auditor waited 2 x 10 minutes; the GPU stayed at 46-74 % (never 120 s at or under 40 %), so these runs went ahead at
59 % before / 67 % after, load average 20.8 / 19.0. They are a frame-signal scan (which frames drew the bars and the
objects), not a timing measurement: their `ft=` figures are not quoted. claude-gpu-done was run after them.

### Not vacuous: two scripted forced cases (scratch worktree original/scratch-m2/audit-m3r2-nf/wt)
There is no fix to revert, so the auditor proved instead that the instrument sees the user's picture and forced the
one in-engine candidate the producer left on input:

1. **The user's frame, forced.** The producer's tree plus a scratch-only hack: at the first frame with NIS time >= 24 s
   (n01's Kuun-Lan close-up) the visible list is emptied and the late bars skipped (patch kept as
   original/scratch-m2/audit-m3r2-nf/forced-objects.diff). Run m3r2-nfa-forced-n01b (nis n01:22:27, debug, ASan 0):
   `outliers=1 (image 1, bars 1, objects 1) first=3599@399; short runs=2 (bars 1, objects 1)`; the same file with the
   read-back columns stripped (what an attended `tools/play.sh -- /catFrameSig` file holds) still gives
   `outliers=1 (bars 1, objects 1); short runs=2`. PPMs (m3r2-nfa-forced-shots, tick 399 frames 6-8, tick 400 frames
   0-1), counted with an independent Python reader (every 2nd pixel; LB 107 rows): bar rows non-black 47 / 47 /
   **109,568** / 47 / 47; middle pixels differing from the forced frame by more than 24 (sum of RGB) 131,316 / 131,343 /
   **0** / 131,608 / 131,763. The forced frame is exactly "the backdrop only, no objects, no bars", and the scan flags
   it with or without the read-back.
2. **The Escape skip (candidate 9), forced by a scripted in-engine call.** `cat_mrEscape()` at NIS time 24 s of n01
   and of nislet01 in the NIS-first start at 144 virtual fps (release; m3r2-nfa-forced-esc): two skips logged
   (`auditescape: rndFrameCount=3464 tick=384 nis=24.0000`, `rndFrameCount=6938 tick=385`), each a hard cut to the
   NIS's end (110.57 s / 106.90 s) with `lb=full` and 11 / 78 visible objects on the cut frame; 47,557 frames,
   **0 outliers, 0 short runs**. Ruled out.

## 4. The fences (clean build)
**Streams** (hwo_compare.py, never sha256 across builds):

| stream | run | sha256 (16) | result |
|---|---|---|---|
| Mission01 6,000 ticks | m3r2-nfa-mission6k-a / -b | eb23c462eebfcb23 / eb23c462eebfcb23 (1,476,370 B, a = b byte for byte) | vs m3r1i-mission-a/run.hwo (1ae66610c3f02410... confirmed): **IDENTICAL, 6,001 ticks (0-6000), every field incl. rng_all** |
| Mission01 with `--frame-sig` | m3r2-nfa-mission-sig | eb23c462eebfcb23 | IDENTICAL to the baseline |
| missiontest NIS-first | m3r2-nfa-missiontest-a / -b | ec8c3c315d886bfc both | `missiontest: done mission=1 ticks=11424 objectives=6/6 result=PASS`; vs m3r1i-missiontest-a: **IDENTICAL, ticks 0-11,425** |
| missiontest with `--frame-sig` | m3r2-nfa-sig-mt | ec8c3c315d886bfc | IDENTICAL to m3r1i-missiontest-a |

(The first mission pair, m3r2-nfa-mission-a/-b, ran the harness default of 3,000 ticks by the auditor's mistake:
a = b, 779b168e7940958c; superseded by the 6,000-tick pair.)

**Dumps** (whole / `sed 's/ crc=[0-9a-f]*//'`, first 16 hex of sha256):

| dump | run | hashes | frames |
|---|---|---|---|
| motion /noInterp | m3r2-nfa-motion-noi | c503c869f0dc9cdb / 3016280e9e0dd3c8 | 1,431 |
| nis /noInterp | m3r2-nfa-nis-noi | 86cb155acffc180e / 196bab32d244f614 | 1,440 |
| motion on | m3r2-nfa-motion-on | c0e411662fa2f527 / 428ef41479eb5749 | 1,431 |
| nis on | m3r2-nfa-nis-on | 0307f3be2044c29c / a5de13a44b8d264f | 1,440 |
| motion opt-out (`--cfg 'letterboxBehind 1' -- /lodBands /noInterp`) | m3r2-nfa-motion-optout | 183681235a3fdc7b / 3016280e9e0dd3c8 | 1,431 |
| nis opt-out | m3r2-nfa-nis-optout | 13ca70eea998e5de / 196bab32d244f614 | 1,440 |

All EQUAL whole to M3-R1's (m3r1i-*) and to the producer's; the opt-outs equal R10's /noInterp dumps; no crc moved.
Nothing to re-baseline.

**Self-tests**: toggle `start=1/1`, `-- /noInterp` `start=0/0`, `-- /noNISInterp` `start=1/0`: each
`fails=0 result=PASS`, ASan 0. fe: Main_game_screen, ASan 0, rc 124 at the 120 s timeout. sp `--timeout 1200` (m3r2-nfa-sp): the universe running through Mission12 (Mission12.btg loaded), ASan 0, no LeakSanitizer report, rc 124 at the timeout, as M3-R1's and the producer's.

## 5. The diff, line by line
- src/Game/MotionTest.c: the signal (`catFrameSigSet` / `MainView` / `Bars` / `End`), `catVirtualFpsSet`,
  `catMotionTestFramesPerTick` honouring it, the sig text after `crc=` only when `sigOn`. Block comment and every
  variable tagged HWI-TEST. Read by nothing in the game. The read-back runs only when `!mtRealClock &&
  oracleVirtualFrameTicks() != 0`; `oracleVirtualFrameTicks` returns 0 outside a mission / replay run
  (OracleDump.c 438), so an attended `tools/play.sh -- /catFrameSig` run never reads pixels back. It leaves
  GL_PACK_ALIGNMENT at 1 (as /catShot does; the report says so).
- src/Game/MotionTest.h, src/SDL/main.c (two switches with help text, tagged, outside any `#ifdef`: the release binary
  carries them), src/SDL/render.c (three tagged hook calls).
- tools/m2_harness.sh: `--frame-sig`, `--virtual-fps N`, the `framesig:` summary line, the header; tools/play.sh:
  header only; tools/dump_outliers.py: new.
- No CAT, CAT-FIX, HWI-FIX or HWI-FEATURE line; no setting, cfg row, console row, key or default; no CAT-TODO added
  (`/bin/grep` finds only M3-R1's BTG.c:1151 `CAT-TODO(M3-R2)`, the integrate's to re-target); ghidra/ untouched;
  tools/m2_layout32.sh rc 0. Nothing outside the producer's scope.

## Findings
- **F1 (doc, fixed).** Report (b) item 10: "The engine never sets a swap interval." Wrong: render.c:1062 (GoK, in the
  tree since M2-R1) calls `SDL_GL_SetSwapInterval(1)` right after `SDL_GL_CreateContext`. Struck through with an
  `[audit: ...]` note; it does not decide the presentation hypothesis either way.
- **F2 (doc, fixed).** Report (b) item 9 cites only the 08:15 log's "Keyboard Event"; the 08:40 log has the same line,
  and it is avi.c's animatic-skip message, so no log can show an in-game key press. Added the 08:40 log and the
  forced Escape case (above) to the item.
- **F3 (doc, fixed).** Report (a) "1,012 / 1,010 / 992 distinct alphas": distinct at 3 decimals; noted with the exact
  counts.
- **F4 (process, self-reported, no effect found).** The producer edited tools/m2_harness.sh's header while runs were
  in flight (decision 92), reverted it to the exact bytes and re-applied it with nothing in flight. Every run of
  theirs I checked has a sane summary and my reruns reproduce their numbers. Recorded, not a fault of the code.
- **F5 (DONE WHEN, open by nature).** The flicker is not fixed: no engine path produces it, and the remaining
  candidate (presentation of an unfinished frame under XWayland / NVIDIA 580.173.02-open / cosmic-comp; both carry
  `wp_linux_drm_syncobj`, confirmed by strings in /usr/bin/Xwayland (24.1.12) and /usr/bin/cosmic-comp; no Xid / NVRM
  line in the kernel log 2026-10-21 07:30-09:00, confirmed) cannot be forced headless. The instrument that settles it
  in the attended play-test is proven to see the user's exact picture with and without the read-back. Item 1 stays
  OPEN for the play-test (owner per the report: the integrate's brief, then M3-R3).

## THE USER'S CHECK (verified)
`tools/play.sh -- /catFrameSig ~/hwi-sig-$(date +%H%M).txt`: play.sh passes everything after `--` to the engine
(play.sh:127-133), `~` is expanded by the user's shell before play.sh replaces HOME, `/catFrameSig` is in the release
binary, and `--driver wayland` exists (play.sh:111) for the optional A/B. The moment (a new campaign, the opening NIS
after Animatic01, the Kuun-Lan close-up; then Mission01's arrival and its letterboxed close-up), where to look and what
to report are named; `python3 -I tools/dump_outliers.py <file>` is the report. Smooth motion is the existing setting.

## Precision
34 claims of the report sampled and checked by a command or a read: the three builds; the exe's bar test, the frame
order, `rndScissorEnabled` at the frame top, nisUpdateTask, nisScissorBarsReset, the KAS cancel; the NIS-start fade;
the n01 / nislet01 / n06 / mt144 / 1-a-tick missiontest scans and their hits; the cut counts; the alpha counts; the
totals; the old files' short runs; the six dumps; the Mission01 pair; Mission01 with the signal; the missiontest pair;
toggle x3; fe; sp; the Xwayland / driver versions and syncobj; no Xid; the 08:15 log; no read-back when attended; the
switch in the release binary; the real-clock scans; the swap interval; no tag / setting / CAT-TODO / delta-map change.
Wrong: 1 (the swap interval, F1). F2 and F3 were true but incomplete or unqualified. **Precision = 33 / 34 = 0.97.**

## Fixes (3, all in docs/m3-r2-nis-flicker.md, marked `[audit: ...]`)
1. The swap-interval claim struck and corrected (F1).
2. Item 9: the 08:40 log, the avi.c meaning of the line, and the forced Escape case added (F2).
3. The alpha counts qualified as distinct at 3 decimals, with the exact counts (F3).
No code was changed; the build stays green (the debug, release and win64 builds above are of this tree).

## Reference for the next port (this tree, unchanged by the audit)
Mission01 6,000 ticks eb23c462eebfcb23 (1,476,370 B; IDENTICAL in every field to m3r1i-mission-a 1ae66610...);
missiontest NIS-first ec8c3c315d886bfc (6/6, done 11,424; IDENTICAL to m3r1i-missiontest-a); the dumps as M3-R1's:
motion /noInterp c503c869f0dc9cdb / 3016280e9e0dd3c8, nis /noInterp 86cb155acffc180e / 196bab32d244f614, motion on
c0e411662fa2f527 / 428ef41479eb5749, nis on 0307f3be2044c29c / a5de13a44b8d264f, opt-outs 183681235a3fdc7b /
13ca70eea998e5de. Runs: original/scratch-m2/m2r2-harness/runs/m3r2-nfa-*. Forced-case patches:
original/scratch-m2/audit-m3r2-nf/forced-objects.diff, forced-escape.diff (scratch only, never in src/).

Verdict: PASS-WITH-FIXES - the flicker is not reproduced and no engine path makes it (confirmed against the exe; the user's exact frame forced in a scratch tree is caught by the new per-frame signal with and without read-back; the Escape skip forced and ruled out); no stream or dump moved; item 1 stays open for the attended play-test's /catFrameSig file; 3 doc fixes (one wrong claim: the engine does set a swap interval)
