<!-- done: builds, exe, revert, paused-dumps, fences, diff, report -->
<!-- audit of M3-R3 producer 2 (pause-freeze); base c8f97bb; the port is the working tree (no commit after c8f97bb) -->
# Audit: M3-R3 pause-freeze (item 5, the pause)

Auditor's labels: `m3r3a-pause-freeze-*` in `original/scratch-m2/m2r2-harness/runs/`. Every engine run went through
`tools/m2_harness.sh` (offscreen, silent, a fresh home per run). The answers file `cycle/answers/m3-r3-user.md` was read
at the start and again before writing this. It is unchanged (md5 ccdc5c26), and nothing in it bears on item 5 beyond the
user's words and the orchestrator's hypothesis.

## 1. Builds (clean, separate dir)

`original/scratch-m2/build-audit-m3r3-pause-freeze/`, configured with the same options `tools/build.sh` uses:

| build | options | result | size |
|---|---|---|---|
| debug | `--buildtype=debug -Db_sanitize=address,undefined` | 0 warnings, `-Werror` (meson `werror=true`) | 90,858,392 B |
| release | `-Doptimization=2 -Ddebug=false -Db_sanitize=none` | 0 warnings | 5,763,384 B |
| win64 (llvm-mingw, the options of `tools/release_win64.sh`, `-Dmovies=false`) | cross files `tools/cross/x86_64-w64-mingw32.ini` + a paths ini | 0 warnings | 5,373,440 B |

The port adds no source file and no GL entry point, and it touches no `_WIN32` code. `tools/m2_layout32.sh`: OK. After
the audit's one code fix (comment tags in Trails.c), release and win64 were rebuilt with 0 warnings. Debug was rebuilt
after the sp run had finished with its binary.

## 2. The exe's pause (read independently) and the choice

- **The P key.** `catKbPause` 0x00550640 is not in functions.csv, so it was read from `exe.dis`. It returns at once when
  0xa43c94 is set or 0xa75198|0xa7518c (multiplayer / recording). It sends "KB_Pause" (0x8b6d7c) and checks the
  tutorial gate (0xa43ca0 / 0xba2e58). It refuses while the letterbox float 0xa4cb38 != 0 or 0xa4cb58 is set. Then it
  toggles **0xa6ed74** (`sete al; mov ds:0xa6ed74,eax` at 0x5506c0-0x5506c5), prints "Pause ON" (0x8b6d70) / "Pause OFF"
  (0x8b6d64) and plays sound 0x108. The user's log has 7 "Pause ON" lines (play-20261022-155157.log), and keys.cfg line
  110 has `key  Pause  80`. **Confirmed.**
- **univUpdate 0x0054c3f0.** Its head is `mov eax,ds:0xa6ed74; test; je; mov eax,ds:0xa6ed78; test; jle 0x54ce10
  (return); dec; store`. The tree's `miUniversePaused()` (`catUnivFrameStep && catUnivFrameStepsLeft < 1`, with
  UnivUpdate.h mapping them to 0x00a6ed74 / 0x00a6ed78) is exactly that condition. **Confirmed.**
- **Every exe reference to 0xa6ed74**, mapped through functions.csv: catKbPause (unmapped 0x5506xx), three unmapped
  0x54f5xx key callbacks, cat_kasWideScreenInFinish, **cat_moRender 0x4ab373**, **cat_ioClick 0x4aba7a**, nisUpdateTask,
  Save/LoadUniverse, spHyperspaceButtonPushed, univUpdateReset, univUpdate, **rndRenderTask 0x57ab81** and gameEnd. No
  trail or camera-float code reads it. The report gave 0x4aba7a to cat_moRender, but it belongs to cat_ioClick: the
  objectives' click is ignored while paused, which is input, not a draw. Fixed in the report. The conclusion stands.
- **The camera float** (`rndMainViewRenderFunction` 0x005764e0, 0x57659a-0x576640). `cameraFloating` needs
  0xa43c90 != 0, 0xa4cabc == 0, 0xa50c90 == 0 and **0xa6c5b4 == 0**. The clocks test only 0xa6c5b4 (edx, 0x576638).
  Under P the exe's float keeps running. **Confirmed.**
- **The trail jitter.** `trailDrawBillboardedSquareThingz` 0x005a5be0 draws `ranRandomFn(0xe)` when `--ranCounter < 0`.
  `trailSurpriseColorAdjust` 0x005a6c30 draws 0xf, 0x10 and 0x11, in that order, under the same gates as the tree. The
  paused branch still draws all three, so the stream draws equal the exe's. **Confirmed.** `trailUpdate` is called only
  from univUpdate, univUpdateAllPosVelShips and univSetupShipForControl (call_graph.csv), so it runs per tick.
- **Judgement of "frozen at the last drawn pose".** The exe has no interpolation: while P pauses it, every object is
  drawn at the simulation's pose, which is the pose its last frame drew. Holding the lerp's alpha is the recreation's
  exact analogue, so it is the exe's behaviour for the objects. Holding the camera float and the glow / trail shimmer
  under P goes past the exe. It is correctly tagged HWI-FEATURE(pause-freeze), the report states it as going past the
  exe, and it is raised as a question for the user. This is sound for the user's item ("ships jitter in place ... as well
  as the camera").

**8 bodies compared with the exe** (decompile against the tree), all equal, so no review row is needed: `nisPause`
0x004ce5f0 (`_DAT_00a4cac8 = bPause`), `trailZeroLength` 0x005a8000 (the same four fields), `cameraCopyPositionInfo`
0x0042b760 (the same 12 floats), `ccFocus` 0x0042e100 (`ccFocusGeneral(.., 0)`), `ccControl` 0x0042e7c0, `utyTasksDispatch`
0x005acd10, `rndShamelessPlug` 0x00579ff0 and `ranRandomFn` 0x004ef860:
- `ccControl`: the structure matches once Ghidra's GoK field names are mapped onto Cataclysm's offsets (the lock
  timestamp / GO_CAMERA_DELAY_TIME branch, cameraControl, cameraSetEyePosition, the zoom-close / FOCUSFAR distance, the
  inlined ccCollapseFocusStack, CameraChase).
- `utyTasksDispatch`: QPC against SDL_GetTicks, the GoK backend. There is no pause test, so the universe task runs on.
- `rndShamelessPlug`: GoK's GL path, with no time or random term.
- `ranRandomFn`: a lagged-Fibonacci add plus the LCG 0x10dcd / 0x3c6ef373. The Mission01 pair below confirms it.

The edited exe functions (rndMainViewRenderFunction, trailDrawBillboardedSquareThingz, trailSurpriseColorAdjust, and
cat_moSupportUnitsDraw, which has an HWI-TEST hook only) are already MODIFIED. `ghidra/` is untouched.

## 3. The fix is not vacuous (a reverted build)

Scratch worktree `original/scratch-m2/audit-pf-revert` (detached at c8f97bb). Only the producer's test hunks were applied
(MotionTest.c / .h, main.c, InfoOverlay.c / .h). MotionInterp.c / .h, render.c and Trails.c are the base, with 0
`miUniversePaused` references. The debug build has 0 warnings. The same pause scripts give:

| run (reverted) | paused pairs with a column move | max per-frame delta | crc / crcv changes | entry F500->F501 |
|---|---|---|---|---|
| `rev-key-on` (P, flags on) | **71 of 71** | eye 16.667 back at F503->F504; B 1.12 px a frame (8.97 back); C 0.18 px (1.45); D 0.19 px (1.48) | 71 / 71 | moves |
| `rev-key-noi` (P, /noInterp) | **71 of 71** (the camera float) | up to 0.016 px on screen | 71 / 71 | moves |
| `rev-univ-on` (universePause) | 0 | - | 67 / 0 | **eye +6.25** (13436.24 -> 13442.49); exit **-4.17** (F573 13438.32) |
| `rev-nis-on` (nisPaused) | 0 | - | 1 / 1 | **eye y 4259.82 -> 4227.92** (back); exit 4263.72 (+35.8) |
| `rev-scene-key` (the user's skirmish recipe, P at F700) | **71 of 71** | eye 0.30 a frame (2.42 back); C 0.155 px (1.29), depth 5.45 (43.5); A 0.107 px (0.86); W 0.74 (6.22) | 71 / 71 | moves |

The position columns of every reverted dump equal the producer's `m3r3-pf-pre-*` dumps (crc / crcv stripped:
c7a16e206937 / ec8aca89c989 / 856f3dfdf7e1 / db24c9b23dc6). Only the crc differs, because the producer's pre-fix build
kept a different Trails.c. **The fix is necessary and it is the cause.**

## 4. The paused dumps after the fix (the audit's clean debug build; independent Python)

The check: for every pair of frames with `pause != 000`, compare every column except crc / crcv / pause / tick; count
crc and crcv changes; F(entry-1) against F(entry); after the unpause, compare frame n with frame n-72 of the un-paused
dump of the same kind.

| run | paused frames | column moves | crc | crcv | entry | after the unpause |
|---|---|---|---|---|---|---|
| `pz-key-on` | 72 (F501-F572) | **0 of 71** | 67 (the SU blink) | **0** | F501 == F500 | eye behind by 2.08, 4.17, 6.25 (F573-575), then 5.56 ... 0.69 (F576-583), equal from F584. F501-F572 equal the un-paused dump's F500 in every column |
| `pz-key-noi` | 72 | **0 of 71** | 67 | **0** | equal | eye equal; A..D ≤ 0.006 px on screen |
| `pz-univ-on` | 72 | **0 of 71** | 67 | **0** | equal | the same hold and carry as key-on; then the exe's float restart (≤ 0.065 px) |
| `pz-univ-noi` | 72 | **0 of 71** | 67 | **0** | equal | the float restart only |
| `pz-nis-on` | 72 (F601-F672) | **0 of 71** | 1 | 1 | F601 == F600 | 0 frames with a column off (offset 72) |
| `pz-nis-noi` | 72 | **0 of 71** | 1 | 1 | equal | 0 frames with a column off |
| `scene-key` (the recipe) | 72 (F701-F772) | 24 of 71, all from the eye's 1-ulp alternation (z 5830.0693 / 5830.0688); the objects' screen columns move with it, ≤ 0.0014 px on screen | 69 | 24 | equal except `raw` (the task clock's raw alpha, which keeps cycling by design) | - |

Every paused dump is **byte-equal to the producer's** (whole sha256: pz-key-on 4cc39108eb1a0193, pz-key-noi
84b84e7378bf1838, pz-univ-on 0e644c0efc600acc, pz-univ-noi a3f5c8cef15f050a, pz-nis-on 055ff7ff35748319, pz-nis-noi
06c1c33fd84457fb). The unpause behaves as the producer states: the eye's lag figures and F584 match exactly, and in the
NIS 0 of the frames after the unpause have a column off.

## 5. Fences (the audit's clean debug build)

| fence | run | result |
|---|---|---|
| motion /noInterp | `m3r3a-pause-freeze-motion-noi` | c503c869f0dc9cdb / 3016280e9e0dd3c8 (1,431 frames): **EQUAL** to M3-R1 / M3-R2 / the previous audit, motiontest PASS |
| motion flags on | `-motion-on` | c0e411662fa2f527 / 428ef41479eb5749: **EQUAL**, PASS |
| nis /noInterp | `-nis-noi` | 86cb155acffc180e / 196bab32d244f614 (1,440): **EQUAL**, PASS |
| nis flags on | `-nis-on` | 0307f3be2044c29c / a5de13a44b8d264f: **EQUAL**, PASS |
| toggle | `-toggle`, `-toggle-lb` (/letterboxBehind) | `toggletest: done start=1/1 fails=0 result=PASS` in both |
| headless Mission01 | `-mission-a`, `-mission-b` (6,000 ticks) | .hwo sha256 f5064299e536486c, 1,476,370 B, 6,001 T records. hwo_compare against `m3r1i-mission-a/run.hwo` (sha256 1ae66610..., present): **IDENTICAL: 6001 ticks, no difference** for both; a against b: IDENTICAL |
| missiontest Mission01 NIS-first | `-missiontest-a`, `-missiontest-b` | `done mission=1 ticks=11424 objectives=6/6 result=PASS`. Both **IDENTICAL** to `m3r2i-e-missiontest-a` (11,426 ticks), sha256 16be83a5cbfb98df |
| fe | `-fe` (--timeout 120) | milestone Main_game_screen, rc 124 (the timeout, as every fe run), ASan 0 |
| sp | `-sp` (--timeout 1200) | reached **Mission12** (the screenshot loop), 23,289 file loads, rc 124 (still running at the timeout, as every sp run), **ASan 0**, no unsuppressed leak |

Every run: **ASan errors 0**, LeakSanitizer "no unsuppressed leak". No stream and no default dump moved. The producer
stated none, and none is expected: the change writes only the side table, the frame's own write-back, the float clocks
(render only) and what a trail draws, with the random draws unchanged.

## 6. The diff (`git diff c8f97bb`, working tree; no commit after c8f97bb)

Files: MotionInterp.c / .h, render.c, Trails.c (HWI-FEATURE(pause-freeze)); MotionTest.c / .h, main.c, InfoOverlay.c / .h
(HWI-TEST); docs/features/motion-interpolation.md (item 7); tools/m2_harness.sh (header only); the report. Nothing is
outside the producer's scope, no generated file is involved, and no CAT line, CAT-FIX or CAT-TODO was added. The one
CAT-TODO naming M3-R3 (BTG.c:1151) is the integrate's.

Logic checked:
- The freeze starts at the first paused frame. Alpha is held at the last frame's alpha, or 0 when a capture ran since
  then.
- The capture after the pause carries the held pose: miCarryFrozen lerps position, coordsys, coll and engine with the
  held alpha. A new, teleported or NIS-driven object takes the simulation's values.
- The droids carry through `miCarryGen`, in capture mode only.
- With both flags off, nothing happens.
- A frame step (0xa6ed78 > 0) runs a normal lerp frame, and the tick ends the hold.
- The NIS hold is compiled only with NIS_TIME_CONTROLS. The release build has no NIS pause, as the exe has none.
- The synthetic P key is an SDL_Event handed in-process to `HandleEvent`. This is the same path as M3-R3 producer 1's
  `mtToggleKey`, with no OS-level input.

## Findings

1. **Untagged recreation lines (code, fixed).** Trails.c `trailDrawBillboardedSquareThingz` had 4 changed lines without
   the tag: `real32 ran = ...`, `{`, `trail->ran = ran;` and `}`. They are now tagged `HWI-FEATURE(pause-freeze)
   [audit: tagged]`, comments only, with the build green. That makes 90 tagged lines.
2. **The report quoted depth as pixels (doc, fixed).** The pre-fix "C 0.42 (3.45), D 1.47 (11.74)" (motion) and "Recon C
   5.4 px a frame (43.5 back)" (the user's recipe) are the depth column, in world units. On screen the figures are C 0.18
   px (1.45 back) and D 0.19 (1.48) in motion, and C 0.155 px (1.29 back) in the recipe. The jitter is real either way.
   The integrate's brief should not quote "5.4 px".
3. **Misattributed exe address (doc, fixed).** 0x4aba7a is in `cat_ioClick` 0x004ab840, not `cat_moRender`.
4. **Bounds after the unpause understated (doc, fixed).** "0.0001-0.006 px" holds on screen. B's off-screen projection is
   off by up to 0.036 px (key-on, 3 frames above 0.006) and 0.037 (noi, 2 frames).
5. **Pre-fix /noInterp bound understated (doc, fixed).** "A..D up to 0.008 px" is really up to 0.016 px (C 0.0157).
6. **"No object column moves" in the recipe after the fix (doc, fixed).** The objects stand still in the world, but
   their screen columns move with the camera's 1-ulp alternation in the same 24 pairs (≤ 0.0014 px on screen).
7. **HWI-TEST count (doc, fixed).** There are 34 added lines in src/, not 35.
8. **Undocumented test side effect (doc, fixed).** In a `/catMotionPause` run, a `/catMotionShot` PPM is written after
   the crcv blanking, so the over-cap SU value's rect is black in the saved frame. This is test-only, and the 3D view is
   unaffected.
9. Observations, not faults. render.c's `!universePause && !miUniversePaused()` is redundant, because the second term
   includes the first. MotionTest.c includes UnivUpdate.h twice (harmless). The freeze of the float and the glows also
   applies under `/noInterp`, which goes past the exe there too. The report states this and asks the user.

No simulation field moved in any stream, and no stream or dump moved that the producer did not state.

## Precision

There were 37 sampled checks of the producer's claims (exe facts, figures, fences, counts, docs). **29 correct and 8
wrong** (findings 1-8; each wrong one is a figure, label, address or count, and none changes the fix or a fence).
Precision = 29 / 37 = **0.78**.

## The user's check (verified)

`tools/play.sh` exists and takes `-- /noInterp`. The user's keys.cfg binds P to Pause (`key  Pause  80`). The Escape
menu sets universePause (`cat_mrEscape`). The check in the report names the launcher, a moment (any mission with moving
ships, Mission02 / Mission03 or a skirmish), the key (P), the Escape menu and the A/B switch. All of these exist.

## Reference for the next port (this audit's runs)

The motion and nis dumps above (EQUAL to the baselines). The paused dumps: pz-key-on 4cc39108eb1a0193 / stripped
dc1e563b3305302e, pz-key-noi 84b84e7378bf1838 / 83847a385482cca5, pz-univ-on 0e644c0efc600acc / 0e686b47a86a8d37,
pz-univ-noi a3f5c8cef15f050a / 26837d660f7242db, pz-nis-on 055ff7ff35748319 / b35081a86b09bdb9, pz-nis-noi
06c1c33fd84457fb / 6c90d6b1ea8a077a. Mission01 m3r3a-pause-freeze-mission-a / -b (IDENTICAL to m3r1i-mission-a).
missiontest m3r3a-pause-freeze-missiontest-a / -b (IDENTICAL to m3r2i-e-missiontest-a).

Fixes: 11 (1 code, 10 doc annotations `[audit: ...]` in docs/m3-r3-pause-freeze.md).

Verdict: PASS-WITH-FIXES - the P key's flag 0x00a6ed74 is proven from the exe as the user's pause and as the cause (the lerp replayed the last tick: 71/71 paused pairs moved in a reverted build); after the fix every paused dump (motion / nis, flags on and /noInterp, key / univ / nis) holds every column with crcv constant, no step at the pause, a stated catch-up on the unpause; the four default dumps EQUAL, toggle / motion / nis PASS, the Mission01 pair and the missiontest pair IDENTICAL, fe and sp ASan 0; fixed 4 untagged Trails.c lines and 10 report figures (depth quoted as px, an exe address, bounds, a count)
