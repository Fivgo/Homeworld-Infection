<!-- done: a-detect, b-cause, c-instrument, d-fences -->
# M3-R2 producer 2: the cutscene's one-frame flicker (key nis-flicker)

Base bbdc613. This work is `git diff bbdc613` in the m2 worktree: the cut-off's WIP snapshot 641a9be, continued on
2026-10-24. Subsystems: **(a) detect**, **(b) cause**, **(c) the play-test instrument**, **(d) fences**.

**Result: not reproduced. No engine fix.** No scan finds the frame the user saw: 450,644 scanned frames over 22 runs,
covering n01, nislet01 and n06 whole; Mission01's NIS-first start with both cutscenes and the KAS Kuun-Lan close-up;
virtual rates 60, 120, 144, 165 and 240; and the real clock. That frame was the backdrop alone, with no objects and no
bars. In the exe and in the port alike, every NIS / KAS-letterbox frame after an NIS's first tick draws the bars late.
So a frame with neither objects nor bars cannot come out of the engine's frame loop during a cutscene. The one frame
that holds exactly that picture is a frame shown before it finished drawing: the BTG is drawn first, the objects next
and the bars last. That points at presentation (XWayland, NVIDIA, the compositor), which no headless run can see. For
the attended play-test, `-- /catFrameSig <file>` now writes one cheap line per frame, so the next session settles which
side of the swap the flicker is on.

The user's file `cycle/answers/m3-r2-user.md`, re-read before this report: there is no answer yet to the flicker hint.
The new entries (the overlay key goes to F1-F12, queued for M3-R3; the overlay is readable at 200%; the GPU is free)
change nothing in this task.

## (a) Detect: the instrument and the scans

**`/catFrameSig <file>`** (HWI-TEST; src/Game/MotionTest.c `catFrameSigSet` / `catFrameSigMainView` /
`catFrameSigBars` / `catFrameSigEnd`; hooks at render.c `rndMainViewRenderFunction` after `catLodTestMainView`, at
render.c `rndRenderTask` inside the late-bar branch, and at render.c `rndRenderTask` before `catMotionTestFrameEnd`;
the switch is in main.c). It writes one line per rendered frame, in any mode:
`F <n> tick=<t> seg=<level segment> dump=<dump frame|-> sig mv=<main views drawn> rl=<render list> vis=<visible>
lb=<late bars: -|full|fade> fd=<nisBlackFade> nis=<NIS time|-> cut=<nisCameraCutTime> a=<NIS period fraction|->
ft=<wall-clock frame period ms>`, plus, **on the virtual clock only**, the read-back counts
`bar=<non-black pixels in the bar rows> mid=<between them> lum=<mean luma> d1=<16x16 thumbnail difference to the previous
frame> d2=<to the frame before that>`.
- With the switch, the motion / nis dump lines get the same ` sig ...` **after** `crc=`. Without it every dump is
  byte-identical (fence below).
- There is no read-back on the wall clock (`--real-clock`, or an attended run): a 4K read-back each frame would cost more
  than the frame does.
- The header line is `# hwi-framesig v1 ... readback=0|1`.
- Nothing in it is read by the game. The read-back changes only GL_PACK_ALIGNMENT.

**`/catVirtualFps <fps>`** (HWI-TEST; MotionTest.c `catVirtualFpsSet`; `catMotionTestFramesPerTick` honours it in any
mode). It gives any virtual-clock run (mission, missiontest, replay) <fps>/16 frames a tick instead of one, so the
render-time interpolation runs in the real NIS-first context. Before this, the mission self-test drew alpha 1 only.
It is a scan, never a baseline: the extra frames draw the shared random stream.

**Harness** (tools/m2_harness.sh, documented in its header):
- `--frame-sig` passes `/catFrameSig <run>/frame.sig`. The summary line `framesig:` is
  `dump_outliers.py --oneline`.
- `--virtual-fps N` passes `/catVirtualFps N`.

**tools/dump_outliers.py** reads a frame.sig, or a motion / nis dump with or without the sig columns. Its tests:
- **image**: the frame differs from both neighbours while they agree (d1/d2).
- **bars**: the bars are drawn on both sides but not on this frame; or bar-row pixels jump.
- **objects**: the main view is missing, or under half the neighbours' visible objects.
- **camera**: the eye or look point is more than 50 units off both neighbours while they agree.
- **short runs** (`--max-run`, default 12): 1-12 frames without the bars or the objects, between frames that have them.
  A state that lasts one universe tick shows for 9 frames at 144 Hz, and the user may call that "1 frame". Frames under a
  full black fade (fd=1) are excluded: the main view is skipped there on purpose.

`--oneline` gives `frames= readback= segments= outliers= (...) first=...; short runs= (bars, objects) first=`.

### The scans (all before any change to a render path; there is none)
| run | what | frames | outliers / short runs |
|---|---|---|---|
| m3r2-nf-sig-n01 / -nislet01 / -n06 | nis mode, whole NIS, 144 virtual, read-back | 16,084 / 15,553 / 9,505 | 0 / 0 each |
| m3r2-nf-sig60-n01 / -nislet01 | 60 virtual | 5,362 / 5,185 | 0 / 0 |
| m3r2-nf-v120-, -v165-, -v240-n01 | 120 / 165 / 240 virtual | 12,510 / 17,871 / 26,806 | 0 / 0 |
| m3r2-nf-v120-, -v165-, -v240-nislet01 | 120 / 165 / 240 virtual | 12,097 / 17,281 / 25,921 | 0 / 0 |
| m3r2-nf-rc-n01 / -nislet01 / -n06 | real clock 144, release, no read-back (2026-10-22) | 15,348 / 14,849 / 9,046 | 0 / 0 |
| m3r2-nf-sig-mt | missiontest NIS-first, 1 frame a tick, ticks to 11,425 | 13,197 | 1 / 1, both scripted (below) |
| m3r2-nf-mt144 | missiontest NIS-first, `--virtual-fps 144` (release), both cutscenes and the KAS close-up, ticks to 4,899 | 60,031 | 0 / 0 |
| m3r2-nf-mt-rc | the same on the real clock 144 (release; GPU 14 % / 15 %, load 15.1-15.3) | 57,465 | 0 / 0 |

Total: 334,111 frames over 17 runs. With the 5 final-build scans in (d): 450,644 frames over 22 runs.

- **Cut coverage.** n01 has 12 hard camera cuts (eye jumps over 300 units: ticks 256, 384, 544, 661, 820, 916, ...),
  nislet01 has 10 and n06 has 8. Every scan crosses all of them: on every virtual alpha grid (k/7, k/9, k/10, k/15) and,
  on the real clock, at 1,012 / 1,010 / 992 distinct alphas [audit: distinct at 3 decimals of the dumps' alpha= column; 12,441 / 11,959 / 7,450 exact], with 1,781 / 1,711 / 1,045 late frames (alpha over 1, up to
  MI_ALPHA_MAX 2).
- **The two missiontest hits are scripted.** Frame 3483 (tick 1712, segment 1) is nislet01's last fade frame: fd=1.000,
  bars drawn, main view skipped, `fadeTo 100%` at frame 3149. Run 3489-3499 (ticks 1718-1728) is inside the arrival's
  white flash: `catSpWhiteFlash`, bars up, a static white middle, lum 183.95.
- **Captures.** m3r2-nf-mt-shots holds 38 PPMs of the hand-off and the flash. The black of Mission01's ticks 0-1 is
  `catSpLevelFade` 1.0, a full-screen quad while no NIS runs (mainrgn.c, "the black fade over the main view after a
  level load").

## (b) Cause: the candidates, each checked
The symptom needs two things in one frame: the BTG drawn and both the objects and the late bars missing.

1. **The late bars.** In render.c `rndRenderTask` they are drawn when `nisScissorFade != 0 || (nisFullyScissored &&
   !rndScissorEnabled)`. That is the exe's own condition (rndRenderTask 0x0057a910). `rndScissorEnabled` is FALSE at
   every frame's top: every `glEnable(GL_SCISSOR_TEST)` is paired, and the frame ends with `glDisable`. The writers of
   the state match the exe:
   - nisUpdateTask 0x004cc600: decompiled and identical, line for line.
   - nisScissorBarsReset 0x004cd260: IDENTICAL, confirmed.
   - nisScissorOut 0x004d02d0.
   - kasfWideScreenIn 0x00497420 (`kasfWideScreenInBody`): identical.
   - kasfWideScreenOut 0x00497630.
   - The NIS end (`catNisKeepScissored`) and SinglePlayer's `HYPERSPACE_LOADDONE`.

   So the bars are missing only on (i) an NIS's first tick, where `wideScreenIn 1 frames` gives fade 0/in = 0, and
   (ii) the first tick of a KAS `wideScreenIn` that cancels a running `wideScreenOut` (the exe's body sets the fade to
   0). (i) falls under `fadeSet 100%` in n01 and nislet01: the scans show fd=1.000 from the NIS's first frame. (ii)
   leaves the objects drawn, and the bars come back gradually (fade 1/48 a tick), so it is not the user's frame. Both
   are the exe's behaviour; neither is changed. **Ruled out.**
2. **The render-time NIS path** (MotionInterp.c `miRenderNis` / `miRenderBegin`, B-Spline.c
   `bsCurveEvalAtBeforeJump`, MI_ALPHA_MAX 2, MI_TELEPORT). It never touches the bars. Its objects and camera are
   clean across all the cuts at every alpha above (camera / objects / runs tests: 0). `miNisCameraValid` resets every
   frame. **Ruled out**: it cannot remove the bars, and no scan shows an object drop.
3. **nisCameraCut / nisUpdate at a cut; the frustum after a teleport.** Same evidence: 0 camera or object outliers at
   any cut. **Ruled out.**
4. **The main region skipped or cut short** (mainrgn.c: `!mrRenderMainScreen || nisBlackFade == 1.0f`; mrDisabled). A
   skip draws no BTG either: the frame is black. **Ruled out** (the fd=1 frames above are black, bars drawn).
5. **hideUniverseToggle** (nislet01 frames 0 and 3209). It falls under the full fade at both ends; the frame signal
   shows fd=1.000 there. **Ruled out.**
6. **The level / NIS hand-offs** (n01 to Mission01, nislet01's start). Black: nisBlackFade 1, then `catSpLevelFade`.
   The 1-a-tick, 144-virtual and real-clock runs show mid=0 there. **Ruled out.**
7. **A buffer swap from inside a frame**, which would present the BTG-only back buffer (HorseRace `rndFlush`,
   PlugScreen, avi, Animatic, `rndClear`). Every caller sits on a level-load or animatic path, never inside an NIS
   frame. The animatic path (`animaticJustPlayed` 8 skipped flips) only withholds frames. **Ruled out.**
8. **The sensors manager** (Mission01's KAS opens it under the letterbox; Sensors.c draws the BTG itself). It is a clean
   cut with the bars up throughout (mt144 frames 50665-50666, ticks 3858-3859). **Ruled out.**
9. **The Escape skip** (mainrgn.c `cat_mrEscape`). Input-driven. The user's 08:15 log shows a "Keyboard Event" during
   Animatic01, not during an NIS. **Not a candidate without a key press.**
   [audit: the 08:40 log has the same line during Animatic01; it is avi.c's animatic-skip message, so the logs cannot
   show an in-game key press either way. A scripted Escape (cat_mrEscape) at NIS time 24 s of n01 and of nislet01, in
   the NIS-first start at 144 virtual fps, cuts hard to the NIS's end with the bars and the objects drawn: 47,557
   frames, 0 outliers, 0 short runs (docs/audits/m3-r2-nis-flicker.md). Ruled out.]
10. **Outside the engine: a frame presented before the GPU finished it.** This alone gives exactly "the backdrop only,
    no objects, no bars", because rndMainViewRenderFunction draws the BTG before the objects and rndRenderTask draws the
    bars last.
    - The user's sessions ran on XWayland (tools/play.sh's default `SDL_VIDEODRIVER=x11`): Xwayland 24.1.12, NVIDIA
      580.173.02-open, cosmic-comp.
    - Both XWayland and cosmic-comp carry `wp_linux_drm_syncobj_*` (explicit sync), so this is unconfirmed.
    - There is no Xid / NVRM line in the kernel log for 2026-10-21 07:30-09:00.
    - ~~The engine never sets a swap interval.~~ [audit: wrong: render.c:1062 (GoK, since M2-R1) calls
      `SDL_GL_SetSwapInterval(1)` right after `SDL_GL_CreateContext`, so the flip is vsynced wherever the driver honours
      it; this does not decide the hypothesis either way]

    **Open: the play-test settles it** (THE USER'S CHECK). No engine change without evidence: a `glFinish` before the
    swap would cost the frame time producer 4 is cutting.

The exe was read with tools/m2_fdec.py for nisUpdateTask, rndRenderTask, nisScissorBarsReset, nisScissorOut and
kasfWideScreenIn. No Ghidra was run.

## (c) The play-test instrument
`/catFrameSig` in an attended run has no read-back (the wall clock) and costs a fopen'd line a frame, about 15 MB an
hour at 144 Hz. It is documented in tools/play.sh's header with `python3 -I tools/dump_outliers.py <file>` as the
report.
- If the flicker recurs and the file shows `short runs=0` and no outlier, the engine drew the objects and the bars on
  every frame, and the cause is downstream of the swap.
- If it shows a run, the line gives the frame, the tick, the NIS time and which part was missing.

## Changes, file by file
| file | change | tag |
|---|---|---|
| src/Game/MotionTest.c | the per-frame signal (WIP 641a9be); this session: read-back only on the virtual clock (`!mtRealClock && oracleVirtualFrameTicks() != 0`), the `ft=` wall-clock period, `catVirtualFpsSet`, `catMotionTestFramesPerTick` honouring it | HWI-TEST |
| src/Game/MotionTest.h | the prototypes | HWI-TEST |
| src/SDL/main.c | `/catFrameSig`, `/catVirtualFps` switches and help | HWI-TEST |
| src/SDL/render.c | 3 hook calls (main view counts, late bars, frame end) | HWI-TEST |
| tools/dump_outliers.py | new: the outlier and short-run scan | HWI-TEST tool |
| tools/m2_harness.sh | `--frame-sig`, `--virtual-fps N`, header | |
| tools/play.sh | header: the per-frame signal for the play-test | |

No CAT:, CAT-FIX, HWI-FIX or HWI-FEATURE line was added. No setting was added: no cfg row, console row, Options control
or key, and no default changed. CAT-TODOs: none added, closed or re-targeted. The one left naming M3-R2 is M3-R1's
BTG.c:1151 star size, which the integrate re-targets.

Delta-map review entries: none. The bodies I relied on matched the exe: nisScissorBarsReset (IDENTICAL), and the
MODIFIED nisUpdateTask, rndRenderTask's bar test and kasfWideScreenIn as already recorded.

## Builds
- `tools/build.sh --debug` and `--release`: rc 0, 0 compiler warnings (meson's sanitizer notice only).
- The win64 cross build (`ninja -C build/win64`): rc 0, 0 warnings. No `_WIN32` code was touched.

**Harness incident.** I edited tools/m2_harness.sh's header for a few seconds while runs were in flight (decision 92).
I reverted it at once to the exact bytes those runs had started with, and re-applied it with nothing in flight. Every
run in flight then completed with a sane summary.

## (d) Fences (final build)
**Streams.**

| stream | run | result |
|---|---|---|
| Mission01 6,000 ticks | m3r2-nf-mission-a / -b | eb23c462eebfcb23 (1,476,370 B), a = b byte for byte; hwo_compare vs m3r1i-mission-a/run.hwo (1ae66610...): IDENTICAL, 6,001 ticks, every field incl. rng_all |
| Mission01 with `--frame-sig` (read-back each frame) | m3r2-nf-mission-sig | eb23c462eebfcb23, IDENTICAL |
| missiontest NIS-first | m3r2-nf-missiontest-a / -b | ec8c3c315d886bfc both; IDENTICAL to m3r1i-missiontest-a, ticks 0-11,425 |

**Dumps** (whole / crc= stripped).

| dump | run | hashes | frames |
|---|---|---|---|
| motion /noInterp | m3r2-nf-motion-noi | c503c869f0dc9cdb / 3016280e9e0dd3c8 | 1,431 |
| nis /noInterp | m3r2-nf-nis-noi | 86cb155acffc180e / 196bab32d244f614 | 1,440 |
| motion on | m3r2-nf-motion-on | c0e411662fa2f527 / 428ef41479eb5749 | |
| nis on | m3r2-nf-nis-on | 0307f3be2044c29c / a5de13a44b8d264f | |
| motion opt-out | m3r2-nf-motion-optout | 183681235a3fdc7b / 3016280e9e0dd3c8 | |
| nis opt-out | m3r2-nf-nis-optout | 13ca70eea998e5de / 196bab32d244f614 | |

All are EQUAL whole to M3-R1's and the instrumentation audit's. **No crc moved, no stream moved, nothing to
re-baseline.**

**Self-tests.**
- toggle: m3r2-nf-toggle / -noi / -nonis `toggletest: done start=1/1|0/0|1/0 fails=0 result=PASS`.
- fe: m3r2-nf-fe reaches Main_game_screen, ASan 0.
- sp `--timeout 1200`: m3r2-nf-sp, universe running to Mission12 (Mission12.btg, as m3r2a-ins-sp), ASan 0, rc 124 at
  the timeout.

**Final-build scans.**

| run | frames | outliers / short runs |
|---|---|---|
| m3r2-nf-fsig-n01 / -nislet01 / -n06, 144 virtual | 16,084 / 15,553 / 9,505 | 0 / 0 |
| m3r2-nf-fsig-mt144 | 60,031 | 0 / 0 |
| m3r2-nf-frc-n01, real clock | 15,360 | 0 / 0 |

m3r2-nf-frc-n01 ran while the user's game held the GPU (94 % / 99 %): my wait timed out and the chained command went on.
Its `ft=` figures are **not** a measurement; the per-frame signal is valid. claude-ping was sent once (FFXIV, 08:30);
claude-gpu-done was run after the GPU runs.

## THE USER'S CHECK (for the play-test brief)
Run `tools/play.sh -- /catFrameSig ~/hwi-sig-$(date +%H%M).txt`. That is the release build at 4K with the settings as
they are; Smooth motion ON, as in the 08:15 / 08:40 sessions.
- **The moment:** start a new campaign and watch the opening in-engine cutscene (after Animatic01): the Kuun-Lan
  close-up (n01 23-33 s, the narrow-angle shot). Then watch Mission01's arrival cutscene and the following letterboxed
  close-up of the Kuun-Lan.
- **Where to look:** the whole frame. Note whether a flash shows the backdrop with no ships and no black bars, and when
  (which cutscene, roughly how many seconds in, on a cut between shots or mid-pan).
- **What to report:** whether it happened, the moment, and the file. Then run `python3 -I tools/dump_outliers.py
  <file>` (or hand the file over).
  - `short runs=0` with a flicker seen means the engine drew everything and the cause is the presentation.
  - A run gives the frame and what was missing.
- **Optional A/B:** `tools/play.sh --driver wayland -- /catFrameSig ...` (SDL's native Wayland instead of XWayland).

## For STATE.md
- Names: `/catFrameSig <file>` (HWI-TEST, any mode, the read-back on the virtual clock only); `/catVirtualFps <fps>`
  (HWI-TEST, any virtual-clock mode).
- Harness: `--frame-sig`, `--virtual-fps N`. Tool: tools/dump_outliers.py (`--max-run`, `--oneline`).
- No defaults changed, no setting added.
- Finding: the flicker was not reproduced in 450,644 scanned frames (22 runs); every engine path draws the bars, as the exe does.
  The suspect is presentation of an unfinished frame (XWayland / NVIDIA / cosmic-comp), open until the play-test's
  frame signal.

## What is left (owner)
- Settle the presentation hypothesis from the play-test's frame.sig (owner: the M3-R2 integrate's play-test brief, then
  M3-R3).
- If it is confirmed downstream, the options are `--driver wayland` as play.sh's default, or a swap-interval / finish
  setting, measured against producer 4's budget (owner: M3-R3).
- The KAS Out→In bar snap (exe behaviour) stays as is.
