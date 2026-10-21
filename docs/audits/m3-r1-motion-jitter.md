<!-- done: build, code-read, guard-rails, scene, real-clock, exe-bodies, fixes, report -->
# Audit: M3-R1 motion-jitter (producer 4, item 4: "ships that aren't the main focus still jittering")

Adversarial audit of docs/m3-r1-motion-jitter.md and `git diff 8410122` (all of it is the working tree; no commit after
8410122). Every engine run went through tools/m2_harness.sh (silent, offscreen, fresh HOME), labels `m3r1-amj-*`; no
gamescope, no input, the GOG game and the oracle DLL not run.

## 1. Build from clean

`original/scratch-m2/build-audit-m3r1-motion-jitter/{debug,release}`, meson set up exactly as tools/build.sh does
(debug: `--buildtype=debug -Db_sanitize=address,undefined`; release: `-Doptimization=2 -Ddebug=false -Db_sanitize=none`;
werror true, warning_level 3): both rc 0, **0 warnings**; hwi-engine debug 89,190,304 B, release 5,666,920 B (sizes
unchanged by the audit's comment / help-string edits; both rebuilt after them, and tools/build.sh --debug / --release
rebuilt green, 0 warnings). tools/m2_layout32.sh OK. For the "pre" comparison the audit built its OWN release binary
from a copy of the tree with the three render fixes reverted (MI_ALPHA_MAX 1 and `alpha >= 1` return; the clamped-child
skip disabled; taskPeriodFraction on taskSubTick): `build-audit-m3r1-motion-jitter/pre/release/hwi-engine`, 0 warnings.

## 2. What was read

* MotionInterp.c / .h diff (MI_ALPHA_MAX 2, the `alpha == 1.0f` return, the NIS clamp at 2, MiEntry.clamped and the
  newly-clamped skip in miGameplayClamped, the stats getters); miLerp / miBlendCoordsys extrapolate linearly for t > 1
  (plain lerp + re-orthonormalisation): the claim holds. bsCurveEvalAtBeforeJump (B-Spline.c) walks every key from the
  current point, so every jump ahead within the 2-tick reach is left out: the claim holds (its header still said "at
  most a tick": fixed, finding F5).
* Task.c taskPeriodFraction / taskClockSample: the scheduler's ticks are untouched; taskSubTickRender is -1 on the
  virtual clock (utility.c sets it in the oracle branch) and only feeds the render alpha. SDL's GetTicks and
  GetPerformanceCounter read the same monotonic clock, so (counter - ticks) = SDL's start offset + the sub-ms fraction;
  the offset's slow average is a sound estimator. utility.c: the render task is started (4461) before the universe
  task (4474), so it runs first in each pass (Task.c's pass walks the handles in order, as the exe's
  taskExecuteAllPending 0x0052a840 does): the alpha > 1 frame per tick is real.
* The camera path (CameraCommand.c CameraChase: `Frames = utyNFrameTicks`, capped at 64, as the exe's 0x0042d510
  `local_18 = utyNFrameTicks; if (0x40 < ...)`): unchanged by the port; the producer's "ruled out" is grounded.
* Options.c opSmoothMotion: `else if (!FELASTCALL(atom))`; FEFlow.c feScreenDeleteFlags sets FAS_OnDelete for a
  FAF_CallOnDelete atom before calling it (as the exe's 0x00475550: `flags & 0x4000` -> `status |= 8`).

### The options-file finding (checked)

* original/play-home/.homeworld/Cataclysm.cfg rows **107 `objInterpolate 0` / 108 `nisInterpolate 0`** (file born
  2026-10-18 08:15:03, last written 2026-10-20 21:22:03); the code default is TRUE (MotionInterp.c 81-82); no row is
  written anywhere but by utyOptionsFileWrite from the live flags (or miOptionsFileValues' file values under a switch).
  The play logs do not record the switches (grep: no interp line), so the 2026-10-20 session ran with interpolation
  OFF unless the box was ticked: **confirmed**.
* Path 1 (M2-R6 build persisted /noInterp): af50963 (2026-10-18 03:48) main.c NoInterpSet writes objInterpolate /
  nisInterpolate directly and utility.c 805-806 list them as rows; the run-only override came with 3e33fee (13:17).
  The four 10-18 play logs (08:15-10:12) fall in that window; play.sh (committed e249cc0, "untracked in the main
  checkout since 2026-10-18") suggests `/noInterp` "for an A/B run". **Confirmed as a possible path** (the producer
  says "probably"; the logs cannot show the switches).
* Path 2 (the live OnDelete bug): reproduced from the producer's pre-fix runs (`m3r1-mj-toggle-prefix`: `screen cancel
  objInterpolate=0 ... (before 1/1)` FAIL; `-prefix-noi`: `screen untouched ... switch=0 (before 0/0 switch=1) rows=0`,
  3 FAILs) and the exe's own flow (opOptionsAcceptHelper 0x004d76d0: feScreenDisappear, then opOptionsSaveSettings;
  Cancel: opRestoreSavedSettings, then the end helper). **Confirmed, and fixed** (the audit's toggle x3 below).
* No other writer of 0: the only assignments are MotionInterp.c's toggle / switch functions, opRestoreSavedSettings'
  saved copy, the console rows and the toggle test; cat_opSetAllDefaults and a missing row leave the TRUE default.
* The exe's own OP_NoLOD (opNoLOD 0x004d9d60) acts on its deletion call too (it sets opNoLODVal from the box whenever
  FAS_OnCreate is clear): the producer's "the exe's own OP_NoLOD keeps its deletion call" is right.

## 3. Reruns (audit build, plain harness)

| check | runs | reference | result |
|---|---|---|---|
| headless Mission01, 6,000 ticks, flags on (pair) | `m3r1-amj-mission-a` / `-b` (2a992dcf2ff752c4, 1,476,370 B) | r10i-mission-a (5bd51234405ea0d8) | **IDENTICAL in every field, ticks 0-6000** (a = b) |
| the same, `-- /noInterp` | `m3r1-amj-mission-noi` (2a992dcf2ff752c4) | r10i-mission-a | **IDENTICAL** |
| the same, `-- /noNISInterp` | `m3r1-amj-mission-nonis` (2a992dcf2ff752c4) | r10i-mission-a | **IDENTICAL** |
| final pair (final tree, tools/build.sh debug, after the audit's edits) | `m3r1-amj-final-mission-a` / `-b` (2a992dcf2ff752c4) | r10i-mission-a | **IDENTICAL, ticks 0-6000, no difference** |
| missiontest Mission01 (pair) | `m3r1-amj-m1-a` / `-b` (c184541b6795fc4e) | `m3r1-afd2-m1` | IDENTICAL (ticks 0-11425); done mission=1 ticks=11424 objectives=6/6 PASS |
| ai (pair, 400) | `m3r1-amj-ai-a` / `-b` (e3b237f182e11c20) | `m3r1-afd2-ai-a` | IDENTICAL (ticks 0-400); checks=16/16 PASS |
| motion /noInterp | `m3r1-amj-motion-noi` | afd2 c503c869f0dc9cdb / 3016280e9e0dd3c8 | **EQUAL** whole and crc-stripped |
| motion on | `m3r1-amj-motion-on` | afd2 c0e411662fa2f527 / 428ef41479eb5749 | **EQUAL** (not moved, as stated) |
| nis /noInterp | `m3r1-amj-nis-noi` | afd2 86cb155acffc180e / 196bab32d244f614 | **EQUAL** |
| nis on | `m3r1-amj-nis-on` | afd2 da532dafedb89960 / 7ce8472514254f4f | moved to **0307f3be2044c29c / a5de13a44b8d264f** = the producer's; 160 of 1,440 frame lines differ (positions and crc), frames 7, 16, .. 1438 (every 9th, one a tick from tick 432): as stated |
| toggle x3 | `m3r1-amj-toggle`, `-noi`, `-nonis` | | `done start=1/1`, `0/0`, `1/0` `fails=0 result=PASS`; `screen untouched` / `screen cancel` keep flags, switch state and rows |
| fe | `m3r1-amj-fe` | | Main_game_screen at the 120 s timeout, ASan 0, 0 fescreen reports |
| sp | `m3r1-amj-sp --timeout 1200` | | still running at 1,200 s, last golden-12-00576.jpg (Mission08 .. Mission12 loaded), 12 animatics skipped, ASan 0, no unsuppressed leak |

Dumps hashed as the previous audit did (sha256 first 16 hex; crc-stripped = `sed 's/ crc=[0-9a-fA-F]*//'`, which
reproduces producer 3's 3016280e9e0dd3c8). The nis-on move's smoothness (nis_smoothness.py, afd2 vs amj): O9 jerk p95
1.318 -> 0.231, max 8.426 -> 1.043; camera jerk p95 19.509 -> 5.121; repeated 0 -> 0: **the producer's numbers exactly**.
nis_smoothness.py's default output for the old dumps is unchanged (md5 of the output with 8410122's tool and this
one: equal for the afd2 motion pair and nis-on).

**The scene measurement, virtual clock (debug, `--scene recon|follow:1500:500:640`, skirmish 700 ticks):** the audit's
`m3r1-amj-v-recon-on` / `-noi` / `-v-follow-on` dumps are **byte-identical** to the producer's (7c3ff03ce186a044,
f7a99a6a3ce15db7, 89940ce1dde0d096); nis_smoothness.py reproduces every cell of the producer's screen tables (B
1.978 / 1.451 / 0.160 rep 446/501 jerk 1.978 / 1.693 -> 0.221 / 0.213 / 0.159 rep 0 jerk 0.091 / 0.006; C, G, g0; the
follow table's A, C, G) and of the world table (C, F, G, H, W) except one figure: G /noInterp rep **2449**/2754, not 2448
(finding F1). `mi=` sums to 825,760 blends over 5,832 frames, 24 new, 0 teleported: as stated. D (asteroid#0) and E
(dSlipgateLarge#0) are never on screen in any scene run and at rest in the world; their on-screen behaviour is the
camera's carry, measured on A in `follow` (the producer says so; a proxy, not a measurement of D / E themselves).

**The real-clock runs (release, `--real-clock 144|60`; GPU measurement):** nvidia-smi 2 % before and 2 % after the
batch, no game running (no .exe, no other engine); `~/.local/bin/claude-gpu-done` after the last one. Independent runs
(the real clock is not reproducible run to run), pre = the audit's own revert build:

| object (span of the /noInterp run, as the producer's table) | /noInterp real 144 | on, pre | on, fix |
|---|---|---|---|
| B (the user's Recon) | 1.979 / 1.094 / 0.130, rep 832/942, jerk 1.980 / 1.491 | 0.407 / 0.224 / 0.126, rep **44**/942, jerk 0.375 / **0.141** | 0.262 / 0.215 / 0.126, rep **0**/942, jerk 0.169 / **0.008** |
| C (crossing) | 2.299 / 1.865 / 0.197, rep 2574/2912, jerk 2.291 / 2.168 | rep 91/2912, jerk 0.461 / 0.206 | rep 5/2912, jerk 0.083 / 0.024 |
| follow: A (at rest, camera-carried; own span) | | rep 19/631, jerk 0.411 / 0.176 | rep 0/631, jerk 0.109 / 0.010 |
| 60 Hz: B (own span) | | rep 54/612, jerk 1.028 / 0.439 | rep 5/612, jerk 0.437 / 0.011 |
| world (own spans): C / F / G / H / W jerk p95 | | 3.210 / 0.696 / 0.912 / 2.214 / 3.098 | 0.062 / 0.015 / 0.025 / 0.039 / 0.024 |
| NIS n06 26-36 s, O9 jerk p95 / camera jerk p95 | | 3.364 / 37.814 | 0.406 / 5.721 |

Clock (144 Hz): pre alpha step 0.0833 / 0.1167 / 0.1833, 648 unblended frames, raw max 1.1000; fix 0.0954 / 0.1158 /
0.1783, 0 unblended, raw max 1.1244; frame time median 6.944 ms, p99 6.945; 0 frames with more than one universe tick.
60 Hz: pre step 0.2667 / 0.2833 / 0.3000 (raw max 1.2667) -> fix 0.2732 / 0.2779 / 0.3144. **The producer's direction
and size reproduce** (B jerk p95 down ~20x, repeated frames to 0). Running nis_smoothness.py on the PRODUCER's own
real-clock dumps reproduces its table exactly (B pre 41/933 jerk 0.400 / 0.143, fix 0/933 0.166 / 0.010; C pre 79/2912,
fix 3/2912) — but only with the /noInterp dump first, whose span the tool applies to the second dump; alone, the pre
dump's B span is 553 frames (rep 17, jerk p95 0.168). Not an error; the table did not say it (clarified in the report,
F-doc 3).

## 4. Exe bodies compared (8)

utyTasksDispatch 0x005acd10 (QueryPerformanceCounter delta, __alldiv / __allrem by the 64-bit utyTimerDivisor:
HW1's body, the review row's note correct), utyGameSystemsInit 0x005abe70 (`utyTimerDivisor = QPF / 0xf0`, taskStartup
(QPF / divisor) = 240: the CAT-TODO(M3 soak)'s figures correct), CameraChase 0x0042d510 (Frames = utyNFrameTicks capped
at 0x40: the tree's), feScreenDeleteFlags 0x00475550 (CallOnDelete -> FAS_OnDelete -> callback), opNoLOD 0x004d9d60 (acts
on the deletion call), opOptionsAcceptHelper 0x004d76d0 (feScreenDisappear before opOptionsSaveSettings),
taskExecuteAllPending 0x0052a840 (handles walked 0 .. taskMaxTask in order: the render-before-universe order is the
exe's too), ThisShipCanDockWith 0x00464290 (the address the scene comment cites: functions.csv). No IDENTICAL body is
edited by the port except utyTasksDispatch, which has its review row (`[review: M3-R1 motion-jitter]`). The delta map's
generators rerun (`tools/delta_map.py merge`, `report`): ghidra/delta_map.json, delta_map_review.json and
docs/m2-r1-function-deltas.md **byte-identical** to the producer's (records changed 0; IDENTICAL 2762 / MODIFIED 1575
/ REWRITTEN 83, reviewed 3573).

## 5. Diff review (scope, tags, CAT-TODO)

Every changed file is in the producer's list and in its scope (no stream-affecting change; MotionTest.c's scene / real
clock run only under their switches; the harness's `--scene` / `--real-clock` and the nis mode's `/noSpeech` only with
--real-clock). MotionInterp.c/.h and MotionTest.c/.h are whole-file HWI-FEATURE(motion-interp); Task.c / main.c /
Options.c lines carry the tag; HWI-FEATURE(motion-interp) count 441 -> 482 as stated (484 after the audit's two tags).
No Cataclysm-behaviour change (so no "CAT:" comment is owed). One CAT-TODO added, on one line, owner first
(`CAT-TODO(M3 soak)`, an M3 plan row).

## Findings

* **F1 (doc, fixed)** report's six-kinds table: G /noInterp "rep 2448/2754"; the tool gives **2449** on the producer's
  own dump (and on the audit's byte-identical one).
* **F2 (doc/code, fixed)** report: "No CAT-TODO / CAT-PROVISIONAL names M3-R1 (`/bin/grep -rn` 0)": it was **1** — the
  new CAT-TODO(M3 soak) line in utility.c said "(M3-R1 motion-jitter measured them: ...)". Reworded to "(measured by the
  motion-jitter scene, docs/m3-r1-motion-jitter.md: ...)"; `/bin/grep -rn "CAT-TODO\|CAT-PROVISIONAL" src | /bin/grep -c
  M3-R1` = 0.
* **F3 (doc, fixed)** tools/m2_harness.sh `--scene`: SPEC lacked the parsed fifth field `:<start>`, H was described as
  "another small ship ... dock at the mothership for support" and W / M's orders were missing (MotionTest.c picks H as
  the nearest free-ship / host pair ThisShipCanDockWith accepts, support or a Worker's deposit, orders W to harvest and
  M 2,000 units ahead). Header fixed (no run in flight: checked with ps); main.c's /catMotionScene help and
  MotionTest.h's prototype comment got `[:<start>]` too.
* **F4 (tags, fixed)** utility.c utyTasksDispatch: `if (catMotionRealClock())` and the first line of the two-line
  `taskClockSample(...)` call carried no HWI-FEATURE(motion-interp) tag (the /bin/grep count missed them); tagged.
* **F5 (doc in code, fixed)** B-Spline.c bsCurveEvalAtBeforeJump's header still said the time is "at most a tick past
  its current time"; with MI_ALPHA_MAX the NIS path evaluates up to two ticks ahead: corrected, marked [audit].
* Observation (no finding): with alpha clamped at 2, a frame after a hitch longer than a tick extrapolates one tick
  ahead and then lands back on the simulation (before: held at the tick). Presentation only; the soak's frame-time
  measurement owns hitches.

Doc clarification (not counted wrong): the report's real-clock columns use the /noInterp dump's span (added as an
[audit] note).

**Precision.** Sampled checks 34 (the build; cfg rows; the M2-R6 persistence path; the OnDelete mechanism in tree and
exe; the pre-fix toggle FAILs; toggle x3; the exe's task clock; the divisor; CameraChase; the render-first task order
(tree and exe); Asteroid0sCanMove 0 / 29 .mission files (tools/bigfile.py cat); the four Mission01 streams; missiontest
pair; ai pair; the four dumps; the nis-on move's frame count and pattern; the nis-on smoothness numbers; the virtual
recon table; the follow table; the world table; the mi= counts; the real-clock table from the producer's dumps; the
real-clock direction on independent runs; the NIS real-clock figures; the delta-map generators; the tag count; the
CAT-TODO grep; the smoothness tool's unchanged default output; the exe's opNoLOD; the Accept order; sp; fe; the harness
--scene header; the utility.c tags; bsCurveEvalAtBeforeJump's doc): **wrong 5** (F1-F5), correct 29: precision
**29 / 34 = 0.853**.

**Fixes: 8** ((1) F1's figure, (2) F2's CAT-TODO wording in utility.c, (3) F2's report claim annotated, (4) F3's harness
header, (5) F3's main.c / MotionTest.h help text, (6) F4's two tags, (7) F5's comment, (8) the report's real-clock span
note): report docs/m3-r1-motion-jitter.md
(3 `[audit: ...]` notes), src/SDL/utility.c (CAT-TODO wording, 2 tags), src/Game/B-Spline.c (comment), src/SDL/main.c
and src/Game/MotionTest.h (help text), tools/m2_harness.sh (header). Builds green after them; the final Mission01 pair
from the final tree IDENTICAL.

## DONE WHEN, re-checked

* Interpolation in the user's session: **off** (rows 107-108 = 0), with both write paths shown and the live one fixed
  (toggle PASS x3 with the new screen-order checks; FAIL before the fix): **yes**.
* Residual with the flags on for the six kinds at 144 Hz virtual and in a real-time run (frame times, alpha): **yes**
  (D / E at rest and off screen: measured by the camera-carried proxy, stated).
* Every candidate kept or ruled out with a number: 9 candidates, 3 kept and fixed under HWI-FEATURE(motion-interp)
  (alpha > 1, the ms resolution, the dock latch), the rest ruled out with figures: **yes**.
* Before / after table (repeated frames and jerk / p95 down): **yes**, reproduced.
* /noInterp dumps EQUAL to producer 3's; flags-on moves stated (motion: none; nis: 160 frames, fix 1): **yes**.
* Mission01 pair IDENTICAL with flags on, /noInterp, /noNISInterp; missiontest and ai pairs IDENTICAL to producer 3's;
  fe and sp ASan 0: **yes**.
* The user's questions / check: concrete — `tools/play.sh --rebuild` (exists: play.sh `--rebuild`), Options -> Video
  -> SMOOTH MOTION (the control exists on Video_Options and the in-game Video screen), the recipe the user gave
  (skirmish, Recon at zenith, adjacent Recon 500 ahead), the camera-moving variant, the Mission01 opening NIS, and the
  monitor's refresh rate; the A/B switch `-- /noInterp` (run-only, verified by toggle-noi): **yes**.

## Reference for the next port (audit runs)

Mission01 debug 2a992dcf2ff752c4 (IDENTICAL to r10i-mission-a in all 21 fields, ticks 0-6000; /noInterp, /noNISInterp
the same); missiontest Mission01 c184541b6795fc4e (6/6, done 11,424); ai e3b237f182e11c20 (16/16); dumps whole / crc-
stripped: motion /noInterp c503c869f0dc9cdb / 3016280e9e0dd3c8, motion on c0e411662fa2f527 / 428ef41479eb5749, nis
/noInterp 86cb155acffc180e / 196bab32d244f614, **nis on 0307f3be2044c29c / a5de13a44b8d264f** (the new reference);
scene dumps (virtual, deterministic) recon-on 7c3ff03ce186a044, recon-noi f7a99a6a3ce15db7, follow-on 89940ce1dde0d096.

Verdict: PASS-WITH-FIXES - the user's play-test ran with interpolation off (cfg rows 0; the OnDelete path that could write them is fixed, toggle PASS x3); the three real-clock residuals are fixed and reproduce on independent runs; every stream IDENTICAL, the /noInterp dumps EQUAL, only nis-on moved as stated; 5 doc/tag errors fixed
