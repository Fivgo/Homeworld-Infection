<!-- done: build, exe, revert, captures, streams, dumps, selftests, diff, sp, perf, verdict -->
# Audit: M3-R3 engine-trails (producer 5, the user's item 7)

Adversarial audit of `docs/m3-r3-engine-trails.md` and `git diff 4ae7d91` (working tree; no commits after 4ae7d91).
Every engine run went through `tools/m2_harness.sh` (silent, offscreen), labels `m3r3a-engine-trails-*` in
`original/scratch-m2/m2r2-harness/runs/`. Game-derived files (crops, extracted .mex/.shp) are in
`original/scratch-m2/m3r3a-engine-trails/`.

## 1. Builds (from clean, `original/scratch-m2/build-audit-m3r3-engine-trails/`)

| build | setup (as tools/build.sh / tools/release_win64.sh) | result | warnings (-Werror, warning_level 3) | size |
|---|---|---|---|---|
| debug | `--buildtype=debug -Db_sanitize=address,undefined` | rc 0 | 0 | 91,200,408 B |
| release | `-Doptimization=2 -Ddebug=false -Db_sanitize=none` | rc 0 | 0 | 5,787,216 B |
| win64 cross (llvm-mingw, `tools/cross/x86_64-w64-mingw32.ini`, `-Dmovies=false`) | rc 0 | 0 | 5,391,872 B; imports SDL2, OPENGL32, KERNEL32, USER32, SHELL32 and the api-ms-win-crt set only |

The port adds no source file and no GL entry point and touches no `_WIN32` code. `tools/m2_layout32.sh`: OK (all ILP32
layout asserts hold; `shiptrail` is a heap struct sized by `trailSize()` = `sizeof(shiptrail) + n*sizeof(trailsegment)`,
not pinned). After this audit's tag fixes (section 7) the tree was rebuilt in `m3r3a-engine-trails/fix-debug` and
`fix-release`: rc 0, 0 warnings; the release binary is byte-identical to the one above (comments only).

## 2. The exe, read by me

- **InitializeEngineTrails 0x00541f40** (`tools/m2_fdis.py`, `m2_fdec.py`): the loop at 0x541f67..0x541f95 builds
  "EngX" from 0x8b5d10 (`m2_exedata.py str` = "EngX"), sets byte 3 to '0'+i and calls `mexGetChunk(mex, "Eng"`
  (0x8791ec = "Eng")`, "EngN")` for i = 0..3; a hit sets `numTrails = i + 1`. The four slots +0x42C..+0x438 are cleared;
  with `static+0x2F8` (trailStatic[0]) non-zero, `trailNew(trailStatic[i], ship, i != 0, i)` for i < numTrails, else the
  capital glow (second 2 for race 1 / 3, else 3). **One ribbon per Eng chunk, up to four, no per-type table**: the
  producer's statement is right. The tree's `InitializeEngineTrails` (UnivUpdate.c:1752) is the same loop plus the one
  HWI-FEATURE call after it.
- **The trail pass 0x005772dd** (rndMainViewRenderFunction 0x005764e0): `lea edi,[esi+0x42c]; mov ebx,4`, and for each
  non-null slot `call 0x5a7310` (trailDraw); **all four slots are drawn**. The tree's render.c:4436 loop is the same.
- **The mex, counted by me** (`bigfile.py cat`, a 20-line parser of the MEX header and chunk list, `chunkSize` + 16 per
  chunk as MEX.c walks it): `race_beast\bmissilecorvette\rl0\lod0\bmissilecorvette.mex` and
  `race_p1\p1missilecorvette\rl0\lod0\p1missilecorvette.mex`, both 348 B, version 262, 6 chunks GUN0, GUN1, Eng0, Eng1,
  Col0, Rct0; **Eng0 (0, -17.854, -45.750), Eng1 (0, +17.854, -45.750)**, normal (0, 0, -1), cone 30, edge 5, no object
  name. **Two Eng chunks each: the exe draws two ribbons per missile corvette.** Both .shp: trailWidth/2 42, Height 18,
  Angle 90, RibbonAdjust 0.75, SpriteRadius 60 (segments 20 Beast / 10 P1, granularity 3).
- **Every type**: all 171 lod0 .mex files in cataclysm.big: 79 with no Eng chunk, 69 one, 20 two, **3 three**
  (bRammingFrigate, sRammingFrigate, tCloakGenerator), none four. The producer's per-.shp table (173 .shp, 82/68/20/3)
  maps .shp to mex differently (2 .shp without a mex), but its conclusion holds: **no type has more nozzles than the exe
  draws**, so the "more nozzles than trails" list is empty.
- **The one-trail cause**: none in the tree. The exe and the tree both draw two 42-unit ribbons, each turned 90 degrees
  between an upper and a lower painted circle; the "single jet stream" is the exe's look (proven by pixels below).

### Bodies compared with cataclysm_gog_r6 (11)
| function | address | delta map | result |
|---|---|---|---|
| InitializeEngineTrails | 0x00541f40 | MODIFIED | as the tree (above); the review note's description right |
| rndMainViewRenderFunction (the trail pass) | 0x005772dd | - | four slots, trailDraw each |
| trailNew | 0x005a4b40 | IDENTICAL (edited: 2 tagged lines) | allocation nSegments*100+0xB0 (0x114 none), ribbon branch width/height/angle*deg2rad or -1/ribbonadjust/style, capital branch: as the tree; review note present |
| trailDelete | 0x005a4cb0 | IDENTICAL (relied on: ship deletion frees slots 2/3, UnivUpdate.c:8018) | memFree: same |
| trailGetNozzleOffset | 0x005a4cc0 | MODIFIED | offset of trailNum times cat_meshObjectMatrixGet(cat_shipNozzleInfoGet(+0x2E0[LOD][trailNum])): the tree's `trailNozzleOffset(.., FALSE)` |
| trailUpdate | 0x005a4e10 | IDENTICAL (relied on) | style < 3: grain counter, trailGetNozzleOffset, coordsys, position, horizontal/vertical turned by the angle, head/length update, no random draw; style > 2: ranRandomFn(0xd) (RANDOM_TRAILS_0) and the state machine: as the tree |
| trailZeroLength | 0x005a8000 | IDENTICAL (relied on: the added ribbons emptied while off) | same 4 fields |
| mexGetChunk | 0x004b7130 | IDENTICAL (relied on) | walks nChunks at +10 from +12, compares type then name, step chunkSize+16: as MEX.c |
| trailDraw | 0x005a7310 | MODIFIED | the glow segment is the nozzle in ship space (trailGetNozzleOffset), glow only at LOD < 3, head z -= 0.3*XADJ, times the N-LIPS factor: as the tree's trailDrawBody; the split pair's glow at the exe nozzle reproduces it |
| trailDrawBillboardedSquareThingz | 0x005a5be0 | MODIFIED | one ranRandomFn(0xe) = RANDOM_TRAILS_1 draw: the tree's trailRandom(RANDOM_TRAILS_1) |
| trailSurpriseColorAdjust | 0x005a6c30 | MODIFIED | ranRandomFn(0xf), (0x10), (0x11) = RANDOM_TRAILS_2..4: the tree's three trailRandom draws |

No IDENTICAL function relied on was found modified. The only IDENTICAL function edited (trailNew) has its review note.
`trailHeightScalar` / `trailHalfWidth` (edited, HWI-FEATURE tagged) have no delta-map record. The ribbon's thickness
stays the exe's: the vertical half extent is `halfWidth * VS = (w/2)(h/w) = h/2` (Trails.c:1513), so halving the width
leaves the height 18.

**Other readers of the four slots** (all checked): Shader.c:862 reads trail[i] for glow vertices with flag 0x10<<i and
breaks on a CONTRACTED trail; ribbons (style <= 2) never leave CONTRACTED (only style > 2 changes state,
Trails.c:787-797, 2169-2192), so the added slots change nothing there. Dock.c, SinglePlayer.c, NIS.c zero or move every
slot (the added ones too, harmless); NetCheck.c zeroes the pointers; SaveGame.c saves none (InitializeEngineTrails
remakes them on load); RammingFrigate.c and render.c:871/5088 read slot 0 only. `trail->second` is never read outside
trailNew.

## 3. The fix is not vacuous

- **Revert worktree** (`original/scratch-m2/m3r3a-engine-trails/wt-revert`: 4ae7d91 + the producer's src/ diff, the
  `hwiEngineTrailsSplitCreate` call removed; built in `build-revert`). Its trail view logs `trails=1,1,0,0` and its frames
  are byte-equal to the final build's `/exeTrails` frames: 1024x768 tick 300 `0d11d085f3f612e7`, 400
  `34208164fc395f64`; 3840x2160 300 `1d5c7d46904470da`, 400 `f980bf9cea5d0bbf`; P1 1024x768 160 `6ebe558a32bf24c0`.
  Each shows **two** ribbons (counts below).
- **The tree before, without the producer's hook** (`wt-base` = 4ae7d91, built in `build-base`): the ai scene with the
  fleet view (`--lodtest 16:fleet --shot 200-400/20`), 11 frames each: **base = /exeTrails in all 11 frames, byte for
  byte**; the default (four streams) differs from base in all 11, only in small regions round the corvettes (e.g. tick
  200: 64 px in 384,367..459,385; 300: 301 px; 400: 290 px).

## 4. The captures (from this audit's debug build)

Counted with my own counter (rows 30 / 50 / 80 px x (W/1024) below the mean of the projected heads, runs of >= 2 px
whose R+G+B exceeds the row's median by > 45) and with `tools/ppm_region.py stats --colour 255,152,57 --tol 4`
(the Beast's ribbon colour): ribbon pixels in 380,425..520,500 at tick 300: 2,493 (default) against 2,185
(/exeTrails, = the reverted build).

| run (m3r3a-engine-trails-) | size | tick | default runs per row | /exeTrails | reverted build | widths default / exe |
|---|---|---|---|---|---|---|
| c-on / c-exe / c-rev (bMissileCorvette #302) | 1024x768 | 300 | 3 / 4 / 4 | 2 / 2 / 2 | 2 / 2 / 2 | 11-14 / 18-24 px |
| | | 400 | 4 / 4 / 4 | 2 / 2 / 2 | 2 / 2 / 2 | 10-13 / 18-24 |
| c4k-on / -exe / -rev | 3840x2160 | 300 | 4 / 4 / 4 (+ one 2-px speck) | 2 / 2 / 2 | 2 / 2 / 2 | 31-41 / 51-73 |
| | | 400 | 4 / 4 / 4 | 2 / 2 / 2 | 2 / 2 / 2 | 32-38 / 51-70 |
| p1mv-on / -exe / -rev (p1MissileCorvette #31, Mission03) | 1024x768 | 160 | 4 / 4 / 4 | 2 / 2 / 2 | 2 / 2 / 2 | 11-12 / 19-25 |
| p1mv4k-on / -exe | 3840x2160 | 160 | 4 / 4 / 4 | 2 / 2 / 2 | - | 32-36 / 55-73 |

The producer's counts are reproduced exactly; every frame is byte-equal to the producer's (c-on 300
`bd153523a3ce9ea0`, 400 `9d7cda8505f753a2`; c4k-on `23836faf3bb4260b` / `08601255c5e071ea`; p1mv-on `46f0fb91877fda03`;
p1mv4k-on `3a0f657624aeabe9`, -exe `a01260ff1b81f1f3`). Looked at the crops (`p1mv-on.png`, `p1mv-exe.png`,
`c-on-300.png`, `c-exe-300.png`): four ribbons, one from each glowing circle, against two broad ribbons each starting
between a pair. (My exe widths for the P1 ribbon, 19-25 px, are wider than the producer's 12-13 because its yellow
threshold keeps only the core; the counts agree.)

**The nozzles on the circles** (p1rest / p1rest4k, the camera straight behind at rest; my measure: the centroid of the
bright low-saturation ring pixels, min channel >= 150, spread <= 90, in a window round each projected circle centre):

| circle | 1024x768 centroid | c - centroid | n - centroid | 3840x2160 centroid | c - centroid | n - centroid |
|---|---|---|---|---|---|---|
| 0 upper -y | (417.9, 333.0) | (+0.4, -0.2) | (+0.7, -0.0) | (1655.6, 936.6) | (+1.1, -0.4) | (+1.7, -0.0) |
| 1 upper +y | (614.5, 333.0) | (-0.6, -0.2) | (-0.8, -0.0) | (2208.4, 936.5) | (-1.7, -0.4) | (-2.4, -0.0) |
| 2 lower -y | (417.9, 458.5) | (+0.4, +0.1) | (+0.7, -0.0) | (1655.4, 1289.3) | (+1.0, +0.6) | (+1.7, +0.0) |
| 3 lower +y | (614.6, 458.5) | (-0.6, +0.2) | (-0.8, -0.0) | (2208.6, 1289.4) | (-1.7, +0.5) | (-2.4, +0.0) |

Rings 113 px across at 1024x768 and 316 px at 3840x2160: every split nozzle within 0.8 px / 2.4 px (under 1% of the
diameter) of its ring's centre. The producer's "within 0.8 px / 3.3 px" holds.

**The random streams, measured**: the `rs=` column of the trail view (all 27 stream states, 321 frames, ticks 100-420):
c-on, c-exe and c-rev hold **every** stream equal at every frame; c4k-on against c4k-exe / c4k-rev differ only in
stream 11 (RANDOM_SOUND_GAME_THREAD, from tick 202); the trail streams 13-17 advance (320 / 165 / 321 distinct states of
13 / 14 / 17) and are equal in all runs. The added ribbons draw nothing from the shared stream.

## 5. Streams, replays, dumps, self-tests (this audit's debug build; every run ASan 0, no unsuppressed leak)

`python3 -I tools/oracle/hwo_compare.py`, all 21 fields.

| stream | run(s) | against | result | sha256 (16) |
|---|---|---|---|---|
| headless Mission01, 6,000 ticks | mission-a / -b | `m3r1i-mission-a/run.hwo` (1ae66610c3f02410..., present) | **IDENTICAL** 6,001 ticks every field incl. rng_all; a = b; 1,476,370 B | a549f68d3a06bad5 (both) |
| ai 400 | ai-a / -b | m3r2i-e-ai-a | IDENTICAL 401 | |
| ai soak 4,000 | ai-soak | m3r2i-e-ai-soak | IDENTICAL 4,001, every field (rng_all too in this run; decision 84's noise did not show) | |
| skirmish LiveFireExercise2 / Armed8 | lfe2 / armed8 | m3r3a-mines-lfe2 / -armed8 | IDENTICAL 601 each; skirmishtest PASS | |
| missiontest Mission01 NIS-first | missiontest-a / -b | m3r3a-mines-missiontest-a | IDENTICAL 11,426; 6/6, done 11,424, PASS | |
| Mission03 | m3-a / -b | m3r3a-mines-m3-a | IDENTICAL 17,794; 5/5, done 17,825, PASS | |
| replay 10-17 | r17 | m3r3a-mines-r17 | IDENTICAL 9,601 ticks / 4,800 packets; vs GOG (the 13 RUNG fields, 1-9600): **first divergent 2823** ships_vel | |
| replay 10-18 | r18 | m3r3a-mines-r18 | IDENTICAL 3,527 / 1,763; vs GOG: **first divergent 2871** ships_vel | |
| hwirec01 / hwirec02 | play01 / play02 | m3r2i-e-play01-a / -play02-a | IDENTICAL 4,801 ticks / 2,400 packets each | |

No simulation field and no rng_all moved anywhere; nothing to state or re-baseline.

**The dumps** (whole / crc= stripped, sha256 16):

| dump | this audit | baseline m3r1i-* | lines |
|---|---|---|---|
| motion-noi | c503c869f0dc9cdb / 3016280e9e0dd3c8 | equal | 1,436 |
| motion-on | c0e411662fa2f527 / 428ef41479eb5749 | equal | 1,436 |
| nis-noi | 86cb155acffc180e / 196bab32d244f614 | equal | 1,489 |
| nis-on | 0307f3be2044c29c / a5de13a44b8d264f | equal | 1,489 |
| motion-exe (`/exeTrails`) | c0e411662fa2f527 / 428ef41479eb5749 | = motion-on, byte for byte | 1,436 |

Position columns IDENTICAL, 0 crc= moves (no missile corvette in those scenes).

**Self-tests**: render `rendertest: done tick=599 frames=583 objects=582 ... result=PASS`; hud `hudtest: done tick=599 ...
frames=537 ... result=PASS`; meshes 1,758 files, 0 with bad objects; toggle and toggle under `/exeTrails`
`toggletest: done start=1/1 fails=0 result=PASS` (the settings line `engineTrails=0 (switch)` in all 39 lines,
`switches=/exeTrails`); console `consoletest: done ... shown=38 set=31 commands=8 ... failures=0 result=PASS`,
`table rows=38 shown=38 set=31`, `tablerows=486`; fe `Main_game_screen` at the 120 s timeout (rc 124), ASan 0, settings
line `engineTrails=1 (default)`.

## 6. The diff (`git diff 4ae7d91`)

Port work only: src/Game/Trails.c, Trails.h, UnivUpdate.c (one line), Console.c, Instrument.c; src/SDL/main.c, render.c,
render.h; tools/m2_harness.sh (header only, 8 lines), tools/play.sh (header only, 8 lines); ghidra/delta_map_review.json
(7 notes), ghidra/delta_map.json, docs/m2-r1-function-deltas.md, the report. Nothing outside the scope. The one commit
after 4ae7d91, 9c07c9b (cycle/STATE.md, cycle/answers: "the GPU is free again", 10:35 during this audit), is the
orchestrator's, not port work; its answer was re-read and the idle check applied (section 8).

- **Generated files reproduce**: in the scratch worktree from 4ae7d91 with the producer's review file,
  `tools/delta_map.py merge --done m3r3:engine-trails` ("records changed 7; IDENTICAL 2757, MODIFIED 1580, REWRITTEN 83,
  reviewed 3582") and `report` give `ghidra/delta_map.json` and `docs/m2-r1-function-deltas.md` **byte for byte**.
- No Cataclysm-behaviour change (so no `CAT:` comment is due); no CAT-TODO added or closed (the only one naming M3-R3 is
  M3-R2's BTG.c:1151, the integrate's).
- Not a `_WIN32` line touched; meson.build unchanged.

### Findings
1. **Untagged recreation-only lines (fixed).** The signature lines of the feature's new functions (`trailRandom`,
   `hwiTrailSplitOn`, `hwiEngineTrailsSplitCreate`, `trailNozzleOffset`, the `trailGetNozzleOffset` wrapper,
   `hwiTrailWidth`, the `trailDrawBody` prototype), the `HwiTrailSplit` typedef's first field and closing line, and the
   changed exe call `trailDrawBillboardedSquareThingz(trail, glowSegment, ...` (the exe passes `&lastSegment`) carried no
   `HWI-FEATURE(engine-trails)` tag, so `/bin/grep` missed them. Tagged (10 lines, `[audit: tagged]`).
2. **A tag moved off an earlier port's line (fixed).** The producer inserted the `catTrailViewFrameEnd` prototype in
   render.c by splitting the line `static void catLodStatsFrameEnd(void); // HWI-TEST (M3-R1 full-detail)`: the M3-R1
   prototype lost its tag and the new line carried two. Restored the M3-R1 line exactly (no diff against 4ae7d91 now)
   and gave the new line its own tag.

No other finding: every edited IDENTICAL function has its review note, the switch / console row / settings-line row
exist and work, no stream or dump moved.

## 7. The DONE WHEN, re-checked
- The exe's count from its machine code (one per Eng chunk, four slots drawn), each corvette's mex chunks (2) and nozzles
  ((0, +-17.854, -45.75)), the empty list of types with more nozzles than trails: **correct**.
- The tree's one-trail cause named (none: the exe's look): **correct** (the revert and the 4ae7d91 base equal
  `/exeTrails`).
- HWI-FEATURE(engine-trails) default ON behind `/exeTrails`, the console row `engineTrails`, the settings-line row:
  **present and working** (toggle-exe, console, fe settings line).
- Captures at 1024x768 and 3840x2160 with four ribbons (two before / under the switch), counts per row; nozzle centres
  on the circles (measured): **correct**.
- Dumps' positions IDENTICAL, 0 crc moves; no simulation field moved; the Mission01 pair IDENTICAL to the baseline; ai
  pair, soak, skirmishes, Mission03 IDENTICAL (no rng_all move to state); missiontest pair; both replays with the rung
  2823 / 2871; render, hud, meshes PASS: **correct**.
- fe ASan 0: correct. sp / sp at 4K and the 4K cost: section 8.

### The user's check (report section 7)
`tools/play.sh` exists, passes everything after `--` to the engine (play.sh:168), rebuilds a stale binary; `/exeTrails`
is a registered switch (main.c commandOptions); Mission03 loads p1MissileCorvette (the Mission03 runs' corvette #31) and
the raiders' corvettes are the type the user saw. The settings-line text it quotes is what the logs print. The console
row cannot be reached in a play-test (no key opens the console, as play.sh says), so the switch is the user's A/B: the
check names it. `cycle/answers/m3-r3-user.md` re-read: no later word on item 7.

## 8. sp and the 4K cost
- **sp** (`--timeout 1200`): universe running, Mission12 reached (12 animatics skipped), still running at the timeout
  (rc 124, as M3-R2's and the producer's), ASan 0, no unsuppressed leak, 23,331 file loads.
- **sp at 3840x2160**: Mission11 reached (11 animatics), UI scale 200% auto, rc 124 at the timeout, ASan 0, no leak.
- **The 4K cost** (a GPU measurement; this audit's release binary; `--mode ai --ai-ticks 500 --res 3840x2160 --lodstats
  150-450:fleet`, 301 frames, 25,169 vertices drawn a frame, FULL DETAIL on, batched meshes). The user's game
  (Helldivers 2) held the GPU at 61-81% from 10:15: one `claude-ping`, then waiters until the GPU stayed <= 40% for
  120 s (10:36:43, 8%). nvidia-smi before 9%, between and after each run 4%; CPU load average (1 min / 5 / 15) 12.2 /
  21.0 / 25.7 before, 11.7 / 20.2 / 25.3 after, on 24 cores (other agents' builds and runs; a moderately loaded CPU, so
  the p99 is the loaded one, as the producer's). `claude-gpu-done` after the last run.

| run (m3r3a-engine-trails-) | look | median ms | p99 ms | max ms | producer's run |
|---|---|---|---|---|---|
| perf-on1 | four streams (default) | 3.535 | 5.556 | 6.502 | 3.524 / 5.611 |
| perf-exe1 | `/exeTrails` | 3.397 | 5.812 | 6.277 | 3.572 / 5.313 |
| perf-on2 | four streams (default) | 3.286 | 5.068 | 5.425 | 3.073 / 5.151 |
| perf-exe2 | `/exeTrails` | 3.221 | 5.661 | 5.812 | 3.345 / 5.001 |

Every median within 7% and every p99 within 14% of the producer's; the default against its neighbouring `/exeTrails`
run differs by +0.14 / +0.07 ms of median and -0.26 / -0.59 ms of p99, inside the run-to-run spread (the two default
runs differ by 0.25 ms of median). The four streams cost nothing measurable; every run meets the 144 Hz budget (6.94 ms)
at p99. The producer's statement holds.

## 9. Checks, precision, fixes

Sampled checks of the producer's claims and work (36): the exe's count per Eng chunk; the four slots drawn; each
corvette's 2 Eng chunks and nozzles; the .shp parameters; the empty more-nozzles list (max 3); the tree = the exe (the
revert); `/exeTrails` = 4ae7d91 (fleet frames); four ribbons at 1024 and 4K for each corvette type (4); the frames'
hashes equal to the producer's; the nozzles on the circles at 1024 and 4K (2); the random streams; the Mission01 pair;
the ai pair; the soak; the skirmishes; the missiontest pair; Mission03; the replays and the rung; the hwi recordings;
the dumps; render / hud / meshes; the console counts; the toggle runs and the settings line; fe; sp; sp at 4K; the 4K
cost; the generated delta map; the builds / win64 / layout; every recreation-only line tagged (**wrong**, finding 1);
the earlier ports' lines left as they were (**wrong**, finding 2); the user's check.

**Precision = 34 / (34 + 2) = 0.944.** Fixes made in place: **2** (findings 1 and 2: tags only, comments; the
release binary is byte-identical before and after). No doc fix needed in the report.

The scratch worktrees `wt-revert` and `wt-base` were removed after the audit (`git worktree remove`); their builds and
the crops stay under `original/scratch-m2/m3r3a-engine-trails/`.

### For the next port: this audit's references
Mission01 pair a549f68d3a06bad5 (IDENTICAL to `m3r1i-mission-a` 1ae66610c3f02410... in every field); ai
`m3r3a-engine-trails-ai-a`; soak `-ai-soak`; skirmishes `-lfe2`, `-armed8`; missiontest `-missiontest-a`; Mission03
`-m3-a`; replays `-r17` (2823), `-r18` (2871); recordings `-play01`, `-play02`; the dumps as in section 5 (equal to
M3-R1's / M3-R2's); captures `-c-on` / `-c-exe` / `-c-rev`, `-c4k-*`, `-p1mv-*`, `-p1mv4k-*`, `-p1rest*`, the fleet
frames `-fleet-base` / `-exe` / `-on`.

Verdict: PASS-WITH-FIXES - the exe draws one ribbon per mex Eng chunk (two per missile corvette, read from 0x00541f40 / 0x005772dd and counted in the mex); HWI-FEATURE(engine-trails) draws four from the painted circles (measured within 0.8 px / 2.4 px), /exeTrails equals the reverted build and 4ae7d91 byte for byte, no stream / replay / dump moved (rung 2823 / 2871), ASan 0; fixed 10 untagged feature lines and a moved M3-R1 tag
