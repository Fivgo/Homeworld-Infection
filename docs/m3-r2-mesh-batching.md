<!-- done: a-profile, b-batching, c-surfaces, d-pixels, e-streams, f-perf -->
# M3-R2 producer 4: FULL DETAIL under the 144 Hz budget (mesh-batching)

Base 83c4b26 (the hyperspace-assembly audit). Work: `git diff 83c4b26` in the m2 worktree. `<R>` below is
`original/scratch-m2/m2r2-harness/runs`, `<P>` is `original/scratch-m2/m3r2-prof` (the profiler's samples, the
wrapper, the copied binaries, the batch scripts). Nothing is committed.

Subsystems: (a) profile: tools/hwiprof (the frame by function and by GL call class); (b) batching:
HWI-FEATURE(mesh-batching), Mesh.c's CPU-lit renderers drawn from arrays, plus the exact lighting reuses (Shader.c);
(c) surfaces: /meshImmediate, the settings line, the console row, tools/play.sh; (d) pixels: the motion / nis dumps,
the render / hud self-test captures and the 4K captures, batched against /meshImmediate; (e) streams and self-tests:
the Mission01 pair, the ai pair and the soak, render / hud / meshes / console / toggle, sp and sp at 4K; (f) the perf
fence: the 4K ai scene on an idle GPU, both paths twice, and /frameLog.

`/home/evanmedinger/Documents/GameDev/Homeworld-Infection/.claude/worktrees/m2/cycle/answers/m3-r2-user.md` was read
first and again before this report: nothing in it changes this task (its later entries are the GPU being free, the
overlay key queued for M3-R3, the overlay readable at 200%; none touches the mesh path).

## Headline

- **Where the 10 ms went** (section (a), the idle-GPU profile of the /meshImmediate path): not the GPU (the flip and
  the glFinish are under 0.2 % of the render task) but the CPU: 58.9 % GoK's immediate-mode corner submission
  (glArrayElement / glTexCoord2f / glBegin-glEnd through the NVIDIA dispatch, one call per attribute per corner: 46,255
  triangles = 138,765 corners a frame), 25.1 % the CPU lighting (Cataclysm lights every corner on the CPU: shColourSet0
  0x0050dfe0 with the per-vertex glow), 7.2 % the material switches, the rest trails, nav lights, the object list.
- **The cut** (HWI-FEATURE(mesh-batching), default ON, run-only `/meshImmediate` restores the old path): the two
  CPU-lit renderers (meshObjectRenderLit 0x004b0970, the ships' path; meshSpecObjectRender 0x004b1d40, the engine glows
  and particle meshes) run the same loop with the same lighting calls in the same order but RECORD the corners (the
  position glArrayElement / glVertex3fv would send, the texture coordinate and colour the GL's current values would
  hold at that call) into an interleaved array, and draw each material run with one glDrawArrays after its material is
  made current; the GL's current colour / texture coordinate are left as the immediate calls leave them. Exact
  reuses cut the lighting: a vertex lit once per material run in the smooth modes (its colour is a function of its
  normal, the material and its glow state, and a second glow update in the same frame changes nothing), a flat
  polygon's lit bytes once for its three corners (same face normal), a vertex's glow addend once per object draw,
  and catShFastToInt's lrint inlined as the one instruction glibc's lrint is (cvtsd2si; Linux only).
- **The pixels are byte-identical**: every dump (motion / nis, flags on and /noInterp) whole-equal to M3-R1's and the
  previous audit's, batched and under /meshImmediate; every capture (the render self-test's 29 frames, the hud's 15,
  the 4K ai fleet scene's 3, Mission01's opening NIS and Mission01 at 4K, 25) byte-equal between the paths.
- **No stream moved**: the headless Mission01 pair (debug and release), the ai pair and the 4,000-tick soak IDENTICAL in
  every field (rng_all included) to M3-R1's.
- **The 4K frame** (section (f), idle GPU, the release binary, M3-R1's command, load 12-15 from the user's VS Code
  ripgrep): the batched path **p50 3.72-4.55 ms, p99 5.21-6.19 ms** in four of five runs (the fifth 8.52 ms, its max
  25.3 ms a system stall that the /meshImmediate run beside it shows too) against /meshImmediate's p50 9.24-12.47 ms,
  p99 13.40-15.30 ms; 25,169 vertices a frame in every run. **The 6.94 ms p99 target is met** (2.4-2.8x faster at the
  median). /frameLog: batched p50 4.8 / p99 7.1 ms over the whole run (every frame of the harness runs a universe tick,
  and the first second holds the level start), /meshImmediate 11.4 / 15.5 ms; frames over 6.94 ms 1.0 % against
  94.8 %.

## (a) The profile: tools/hwiprof

No perf / valgrind on this machine, so a sampling profiler was written: `tools/hwiprof/hwiprof.c` (HWI-TEST), an
LD_PRELOAD shim that samples the MAIN thread on a CLOCK_MONOTONIC timer (wall time, so a wait in glFinish or the swap
is sampled too; 200 us here) and records each sample's stack (the interrupted PC and backtrace()), written at exit with
/proc/self/maps; `tools/hwiprof/hwiprof_report.py` symbolises it (addr2line on a binary built with the release flags
plus -g: `build/prof`, `meson setup -Doptimization=2 -Ddebug=false -Db_sanitize=none -Dc_args=-g`; nm -D for the
libraries) and prints the inclusive and self shares, the engine call sites (`--lines`), the GL call classes (`--class`:
the call site's source text sorted into vertex / colour / material / bind / matrix / state / flip / other GL / engine
CPU) and the frame by part (`--breakdown`). The run goes through the harness (docs/CONSTRAINTS.md item 5): `<P>/prof-run.sh
<label> [-- switches]` copies build/prof's binary (so the samples stay symbolisable after a rebuild) and runs
`tools/m2_harness.sh <label> --mode ai --ai-ticks 500 --res 3840x2160 --lodstats 150-450:fleet --bin <P>/prof-wrap.sh`
(the wrapper exports LD_PRELOAD / HWIPROF_OUT and execs the copied binary; it must not name the debug-only file-load
switch, which the harness greps its --bin for). Samples are taken `--within rndRenderTask` (the render task's frames).

The fence's profiles (idle GPU: nvidia-smi 19 % just after the previous measurement run, 6 % after; load 14.2 / 12.9;
`<P>/m3r2-mbf-prof-imm.txt` / `-bat.txt`, runs `<R>/m3r2-mbf-prof-imm` (`-- /meshImmediate`) and `-bat`; their
lodstats p50 12.393 / 4.389 ms, p99 14.916 / 5.991 ms; 27,047 / 9,517 samples within rndRenderTask; the ms column is
the share times that run's p50, so approximate: the samples cover every render frame of the run, the profiler adds
its own cost). The shared Shader.c changes (shColourLit0, the inlined cvtsd2si) are in both runs: the /meshImmediate column is this
tree's immediate path, not M3-R1's binary.

**By function / part** (`--breakdown`: each sample counted once, for the first part its stack holds):

| part of the render task | /meshImmediate | ms | batched | ms |
|---|---|---|---|---|
| mesh: corner submission (immediate: glArrayElement / glTexCoord2f / glBegin-glEnd per corner; batched: recording the corners) | 58.90 % | 7.30 | 23.41 % | 1.03 |
| mesh: CPU lighting (shColourSet0 / shColourLit0, cat_shColourAddGlow, cat_shGlowUpdate, meshSpecColour / shSpecularColour) | 25.05 % | 3.10 | 26.38 % | 1.16 |
| mesh: batched draw (glDrawArrays: the driver's copy of the client arrays) | - | - | 13.85 % | 0.61 |
| mesh: material / texture switch (meshCurrentMaterialDefault: cull, texture env, trMakeCurrent, shade model, fade) | 7.24 % | 0.90 | 12.37 % | 0.54 |
| mesh: hierarchy, matrices, per-ship setup | 1.27 % | 0.16 | 3.94 % | 0.17 |
| mesh: other renderers (meshObjectRender: the planet backdrop rndRenderAHomeworld 98 %, particle meshes) | 0.99 % | 0.12 | 2.87 % | 0.13 |
| trails and engine glows (trailDraw, trailLineSequence; the glow meshes are in the lighting / submission rows) | 1.51 % | 0.19 | 5.10 % | 0.22 |
| nav lights | 1.14 % | 0.14 | 3.53 % | 0.15 |
| background (BTG) and stars | 0.34 % | 0.04 | 0.69 % | 0.03 |
| rest of the main view (object list, effects, culling, overlap) | 2.00 % | 0.25 | 4.23 % | 0.19 |
| 2D: regions, HUD, text | 0.45 % | 0.06 | 0.99 % | 0.04 |
| rest of the render task | 1.04 % | 0.13 | 2.58 % | 0.11 |
| flip / glFinish | 0.07 % | 0.01 | 0.05 % | 0.00 |

**By GL call class** (`--class`: the innermost engine frame's source line; "engine CPU" = no GL call at that line):

| class | /meshImmediate | batched |
|---|---|---|
| vertex calls (glArrayElement, glTexCoord2f, glBegin / glEnd, the colour capture lines) | 56.28 % | 10.59 % |
| engine CPU (lighting arithmetic, corner recording, culling) | 33.99 % | 59.65 % |
| other GL / driver (batched: mostly glDrawArrays, whose sample lines fall on `if (end > first)`) | 2.31 % | 17.42 % |
| state (rndBackFaceCullEnable, glEnable / glDisable, rndTextureEnvironment) | 3.60 % | 3.82 % |
| binds (trMakeCurrent) | 2.04 % | 4.88 % |
| matrix (glPushMatrix / glMultMatrixf / glPopMatrix) | 0.67 % | 2.18 % |
| colour (glColor4ub) | 0.86 % | 0.97 % |
| flip / glFinish | 0.17 % | 0.25 % |
| material (glMaterial, glShadeModel) | 0.08 % | 0.24 % |

The GPU is never the limit (the flip and the glFinish of /catLodStats are under 0.3 %): the frame is CPU-bound
submission, as M3-R1 said, and after the cut it is CPU-bound lighting and bookkeeping.

## (b) The batching: HWI-FEATURE(mesh-batching)

**What the exe does** (tools/m2_fdec.py): meshObjectRenderLit 0x004b0970 switches the material per run and then, per
corner, makes the CPU-lit colour current (shColourSet0 0x0050dfe0: the sum over the lights, the dock blend,
FAST_TO_INT bytes clamped at both ends, the per-vertex glow cat_shColourAddGlow 0x0050d7c0 with cat_shGlowUpdate
0x0050d560, glColor4ub), glTexCoord2f in the textured modes and glArrayElement, inside glBegin / glEnd; meshSpecObjectRender
0x004b1d40 the same with meshSpecColour 0x004b00c0 / shSpecularColour 0x0050e280 and glVertex3fv. That stays the
base: the CAT bodies are unchanged and are what `/meshImmediate` runs; the batched path is a recreation feature on top.

File by file:

- **src/Game/Mesh.c**
  - `meshImmediate` (sdword, 0; the console row's value), `meshImmediateSwitched`, `meshImmediateSwitch()` (/meshImmediate).
  - The corner array `CatMeshCorner` (x, y, z, s, t, rgba: 24 bytes) in one scratch array `meshBatch` grown to the
    largest object (realloc; `meshBatchReserve`), with the per-vertex run stamps `meshBatchStamp`, the lit colours
    `meshBatchLit` and the glow addends `meshBatchGlow`; `meshBatchFree` from `meshShutdown` (the exe folds its empty
    meshShutdown away; GoK's is called by utyGameSystemsShutdown). Nothing is kept per mesh: the colours are lit every
    frame and the texture coordinates can change in place (meshFixupPacoUV), so there is no buffer to free with a mesh
    or at a level change (LeakSanitizer: no report in any run, sp and sp 4K included).
  - `meshBatchBegin` (the colour capture on, a new glow stamp, the vertex and colour arrays on, the normal / edge /
    index arrays off as meshEnableVertexArrays leaves them), `meshBatchDraw` (one glDrawArrays a run; the texture
    coordinate array on only for a run of a textured mode, so an untextured run takes the GL's current texture
    coordinate exactly as the immediate path does: the engine glow's g_NoMatSwitch path leaves texturing to its
    caller; before such a run an earlier run's last coordinate is made current), `meshBatchEnd` (the arrays off, the
    hooks off, glColor4ub / glTexCoord2f of the last values given: the GL's current values as the immediate calls leave
    them), `MB_TEX` / `MB_EMIT` (the recorded glTexCoord2f / glArrayElement), `MB_LIT_ONCE` (a smooth mode's vertex lit
    once per run), `MB_GLOW` / `MB_CPV_NEXT` (the glow addend cache; a flat polygon's next corner reuses the lit bytes).
  - `cat_meshObjectRenderLitBatched` / `cat_meshSpecObjectRenderBatched`: the immediate loops line for line (material
    switch, blend, the poly modes, the per-vertex colour with enableCPV, RND_POLY_STATS / MESH_MATERIAL_STATS, the
    asserts) with the GL calls recorded; the Lit one sets the vertex pointer back to the vertex list as
    meshEnableVertexArrays left it; the Spec one puts the array enables back as found (glIsEnabled), since its
    immediate path uses no array.
  - meshObjectRenderLit / meshSpecObjectRender: `if (!meshImmediate && meshBatchReserve(object))` the batched
    function, else the CAT body (also the fallback if the array cannot grow). meshSpecColour: stores its colour in
    `shColourCapture` while the batched path runs, instead of glColor4ub.
- **src/Game/Shader.c / Shader.h**: `shColourCapture` (shColourSet0 stores its bytes there instead of glColor4ub while
  set); shColourSet0's lighting sum and byte conversion moved unchanged into `shColourLit0` with `shColourReuseLit`
  (the batched flat corner after the first reuses the bytes: same face normal, same lighting state, so the same
  bytes); `ShGlowCacheEntry` / `shGlowCache` / `shGlowStamp`: cat_shColourAddGlow works the addend out once per vertex
  and object draw (the same arithmetic: the state's `FAST_TO_INT(colour * 256) * 2` after cat_shGlowUpdate, or the
  info entry's signed bytes) and applies `clamp(byte + addend)` as before; `catShFastToInt` inlines
  `_mm_cvtsd_si64` on x86-64 Linux: glibc's lrint is exactly `cvtsd2si %xmm0,%rax` (libm.so.6 llrint/lrint at
  0x33360, disassembled), so every input gives the same result; the Windows build keeps mingw's lrint (no _WIN32 code
  changed).

**Why the reuses are exact.** Within one object draw nothing the lighting reads changes between corners except at a
material switch (shUpdateLighting: base colour, the lights' material diffuse; meshSetFade, the EMP flicker), which
starts a new run. cat_shGlowUpdate stamps `lastUpdate = universe.totaltimeelapsed`; a second update in the same frame
subtracts `(T - T) * 0.2` from the hit glow and `* 0.1` from the scorch (exactly 0), adds `(old - hitGlow) * 0.2 = 0`
to the scorch, and recomputes the colour from the same scorch / info / trail state: the same colour and the same end
state as one update. So lighting a vertex once per run (smooth modes) and working its glow addend out once per object
draw give the immediate path's bytes and leave every glow state as the immediate path leaves it. The dumps and
captures confirm it byte for byte (section (d)).

**Every mesh path left immediate** (its share of the batched 4K frame, m3r2-mbf-prof-bat): meshObjectRender 0x004b0170
(lighting off or wireframe, and meshObjectRenderTex's single-texture meshes: here the planet backdrop
rndRenderAHomeworld and a few particle meshes, 2.87 %, 0.13 ms; it also carries GoK's g_SpecificPoly UV editing),
cat_meshObjectRenderWarped (the hyperspace warp's per-vertex deformation: 0 % in this scene), meshMorphedObjectRender
(morphed meshes: 0 %), the Beast-stun edge lightning (cat_meshObjectRenderEdgeEffect: draws stream-0 particles, 0 %).
They were left as they are because their share is small and (warp, morph, edge) each computes or draws per vertex
differently; any of them can take the same recording later. Trails (5.1 %), nav lights (3.5 %) and the background are
not mesh paths.

## (c) The surfaces

| surface | where | behaviour |
|---|---|---|
| switch | `/meshImmediate` (src/SDL/main.c) | this run only: the immediate path (no options-file row, so nothing is saved) |
| console row | `meshImmediate 0\|1` (src/Game/Console.c HWI row `CAT_CON_HWI_ROW_ID + 6`, CAT_CON_HWI_ROWS 6 -> 7) | sets the path at once |
| settings line | `meshImmediate=0 (default)` / `=1 (switch)` / `(console)` (src/Game/Instrument.c row 6; the console row ends the switch's source) | logged at start and at every change |
| default | ON (the batched path) | |

No Cataclysm.cfg row, no Options control, no key (the task asked for none). tools/play.sh documents the switch and the
console row; the harness needs no new flag (the switch goes after `--`). Console self-test: shown 36, set 29 (29 + 7,
22 + 7), PASS.

## (d) The pixels (final code; debug build for the dumps, release for the captures)

Dumps (1024x768, 144 fps virtual, nis n06:26:36; whole / with `sed 's/ crc=[0-9a-f]*//'`, first 16 hex of sha256):

| dump | M3-R1 / previous audit (m3r2-hsaa-*) | batched (`m3r2-mbf-*`) | /meshImmediate (`m3r2-mbf-*-imm`) |
|---|---|---|---|
| motion flags on (1,431 frames) | c0e411662fa2f527 / 428ef41479eb5749 | **c0e411662fa2f527** / 428ef41479eb5749 | **c0e411662fa2f527** / 428ef41479eb5749 |
| motion /noInterp | c503c869f0dc9cdb / 3016280e9e0dd3c8 | **c503c869f0dc9cdb** / 3016280e9e0dd3c8 | **c503c869f0dc9cdb** / 3016280e9e0dd3c8 |
| nis flags on (1,440 frames) | 0307f3be2044c29c / a5de13a44b8d264f | **0307f3be2044c29c** / a5de13a44b8d264f | **0307f3be2044c29c** / a5de13a44b8d264f |
| nis /noInterp | 86cb155acffc180e / 196bab32d244f614 | **86cb155acffc180e** / 196bab32d244f614 | **86cb155acffc180e** / 196bab32d244f614 |

Every dump whole-equal (so every crc= equal, frame by frame): no render change, no render-timing change; all PASS,
ASan 0. Captures (`--shot`, the physical framebuffer read back before the flip; `cmp` of every file against its
`-imm` twin):

| capture | frames | size | differing |
|---|---|---|---|
| render self-test (`m3r2-mbf-render-shot`, ticks 20-599/20) | 29 | 1024x768 | 0 (both PASS) |
| hud self-test (`m3r2-mbf-hud-shot`, 20-599/40) | 15 | 1024x768 | 0 (both PASS) |
| ai fleet scene (`m3r2-mbf-shot-ai`, `--lodstats 150-410:fleet`, ticks 200 / 300 / 400) | 3 | 3840x2160 | 0 |
| Mission01_NIS01 (12: the Kuun-Lan close-up with its team colours and its green-lit bays) and Mission01 (13) (`m3r2-mbf-shot-m01`, missiontest, ticks 100-1900/150) | 25 | 3840x2160 | 0 |

(The capture missiontest runs end at 2,000 ticks by design, so their "missiontest: done ... objectives=0/6
result=FAIL" is the cut, not a fault.) No difference to explain: no sub-LSB rounding either.

## (e) Streams and self-tests (final code)

| run | what | result |
|---|---|---|
| m3r2-mbf-mission-a / -b (debug) | headless Mission01, 6,000 ticks | IDENTICAL to m3r1i-mission-a in every field (6,001 ticks); a = b; sha256 b5388ee24a6d7972 (build identity in the header), 1,476,370 B, 154 ships |
| m3r2-mbf-mission-imm (debug, /meshImmediate) | the same | IDENTICAL |
| m3r2-mbf-mission-rel-a / -b (release) | the same | IDENTICAL to m3r1i-mission-rel-a (56c18be474650c55) |
| m3r2-mbf-missiontest-a | Mission01 from the NIS-first start | 6/6 PASS at 11,424; IDENTICAL to m3r1i-missiontest-a (11,426 ticks) |
| m3r2-mbf-ai-a / -b | ai, 400 ticks, 156 ships | PASS; IDENTICAL to m3r1i-ai-a |
| m3r2-mbf-ai-soak | ai, 4,000 ticks | PASS; IDENTICAL to m3r1i-ai-soak in all 21 fields (rng_all too this time) |
| m3r2-mbf-render / -render-imm | render self-test | PASS (583 frames, 582 objects) |
| m3r2-mbf-hud | hud self-test | PASS |
| m3r2-mbf-meshes | mesh load self-test | 1,758 files, 0 with bad objects |
| m3r2-mbf-console | console self-test | PASS (shown 36, set 29: the new row) |
| m3r2-mbf-toggle | Smooth motion toggle | PASS |
| m3r2-mbf-sp (--timeout 1200) | the sp chain | Mission01_NIS01 .. Mission12 (12 animatics skipped), rc 124 at the timeout as M3-R1, ASan 0, no LeakSanitizer report, UBSan kinds and counts as m3r1i-sp |
| m3r2-mbf-sp-4k (--res 3840x2160) | the sp chain at 4K | .. Mission11 as m3r1i-sp-4k, ASan 0, no leak, UBSan as M3-R1 |

Every run ASan 0 and no LeakSanitizer report: the scratch arrays are freed by meshShutdown, and there is nothing per
mesh to free. **No stream or dump moved; nothing to re-baseline; no rng_all draw added** (the batched path makes the
same calls; nothing in it draws a random stream).

Builds: `tools/build.sh --release` and `--debug` green, 0 warnings under -Werror; the Windows cross build
(`meson compile -C build/win64`, llvm-mingw) compiles clean (the new GL calls are GL 1.1, in opengl32; no _WIN32 code
changed; catShFastToInt keeps lrint there); `tools/m2_layout32.sh` OK (no struct changed).

## (f) The 4K frame (GPU measurement)

M3-R1's command: `tools/m2_harness.sh <label> --mode ai --ai-ticks 500 --res 3840x2160 --lodstats 150-450:fleet --bin
build/release/hwi-engine [-- /meshImmediate]` (the camera on every ship of the 156-ship fleet, 164 objects in view, 301
frames; ms = the render task to after the flip with a glFinish). The user's game had closed (the idle watcher saw 120 s
at <= 40 %, 13:32); nvidia-smi 0 % before the first run, 6-19 % between runs (the previous run's own work), 6 % after;
load average 12-15 (the user's VS Code ripgrep at ~940 % CPU, as in M3-R1's 12.5-13.4). Runs interleaved:

| run | path | p50 ms | p99 ms | max ms | vertices |
|---|---|---|---|---|---|
| m3r2-mbf-perf4k-imm1 | /meshImmediate | 12.465 | 14.950 | 25.960 | 25,169 |
| m3r2-mbf-perf4k-bat1 | batched | 4.549 | 8.524 | 25.291 | 25,169 |
| m3r2-mbf-perf4k-imm2 | /meshImmediate | 9.244 | 15.297 | 17.647 | 25,169 |
| m3r2-mbf-perf4k-bat2 | batched | 3.722 | 6.188 | 7.305 | 25,169 |
| m3r2-mbf-perf4k-imm3 | /meshImmediate | 10.534 | 13.988 | 16.001 | 25,169 |
| m3r2-mbf-perf4k-bat3 | batched | 4.500 | 5.609 | 6.356 | 25,169 |
| m3r2-mbf-perf4k-imm4 | /meshImmediate | 11.511 | 13.399 | 16.978 | 25,169 |
| m3r2-mbf-perf4k-bat4 | batched | 4.259 | 5.213 | 6.105 | 25,169 |
| m3r2-mbf-perf4k-imm5 | /meshImmediate | 11.982 | 13.795 | 14.901 | 25,169 |
| m3r2-mbf-perf4k-bat5 | batched | 3.748 | 5.658 | 6.447 | 25,169 |
| m3r2-mbf-prof-bat (profiled, build/prof) | batched | 4.389 | 5.991 | 6.571 | 25,169 |

**The target (p99 under 6.94 ms) is met**: batched p99 5.21-6.19 ms in four of five runs and the profiled run; bat1's
8.52 ms p99 comes from a 25 ms stall that imm1, run just before it, shows too (a system hiccup under the ripgrep load,
not the path). The vertices are equal between the paths in every run (/catLodStats' count of the levels drawn).
(M3-R1's /meshImmediate-equivalent figures were 9.08-9.58 / 11.39-13.69 ms at load 12.5-13.4; this session's immediate
runs are a little slower at the same load.)

**/frameLog of the same scene** (`--frame-log`, `python3 -I tools/frame_log_report.py`): `m3r2-mbf-flog-bat`: 500
frames, frame period p50 4.8 / p95 5.4 / p99 7.1 ms, 5 frames (1.0 %) over 6.94 ms (2 one refresh late, 3 three or
more: the level start's first second, the 1.2 s first tick), vertices median 25,169, largest 25,541; `m3r2-mbf-flog-imm`:
p50 11.4 / p95 13.7 / p99 15.5 ms, 474 frames (94.8 %) over, vertices median 25,169, largest 25,541 (equal: the
overlay's VERTS reads the same count). The frame log's period is the whole frame (every task, and a universe tick in
every frame of this harness mode: one a frame instead of one in nine at 144 Hz), so its p99 sits above the render
task's; the drift verdict is meaningless on a 3-6 s run (it compares thirds).

**What is left of the batched frame** (the profile above, ~4.4 ms): the CPU lighting 1.16 ms (Cataclysm's per-vertex
CPU lighting, now once per lit value), the corner recording 1.03 ms, glDrawArrays' copy of the client arrays 0.61 ms,
the material switches 0.54 ms, the rest ~1 ms (trails, nav lights, the object list). A fleet twice this size (PLAN.md's
~47k ship vertices) would land near 8-9 ms. The next cuts, in order of size, for the M3 soak's frame-pacing work:
(1) static per-object buffer objects for the positions and texture coordinates (built on first draw, rebuilt when
meshFixupPacoUV changes the UVs, freed in meshFree), so only the 4-byte colours are written and uploaded per corner
(the recording and most of glDrawArrays' copy, ~1.2 ms); (2) the material switches' redundant GL state (cull, texture
environment, alpha test) cached; (3) the lighting spread over threads or moved to a shader, which would have to match
FAST_TO_INT's byte rounding exactly (a pixel risk; not done).

## THE USER'S CHECK (for the play-test brief)

`tools/play.sh` (release, the user's own Cataclysm.cfg: FULL DETAIL on, Smooth motion on), at 3840x2160 with the overlay
on: `tools/play.sh -- /overlay /frameLog ~/hwi-frame-batched-$(date +%H%M).log`. New Game: the Mission01 opening
cutscene (the Kuun-Lan close-up) and the first minutes of Mission01 with the whole fleet in view (zoom out to see every
ship), then a skirmish against one computer with a large battle on screen. Where to look: the overlay's FRAME line (the
p99 should stay at or under 6.9 ms where the M3-R1 build showed ~10-15 ms) and the ships themselves: the hulls, the team
colours (base and stripe), the Kuun-Lan's green-lit bays, the engine glows and the damage glow on hit
ships must look exactly as before. A/B: the same moments with `tools/play.sh -- /overlay /meshImmediate /frameLog
~/hwi-frame-immediate-$(date +%H%M).log` (the old path, this run only). What to report: any difference in how a ship
looks between the two runs (a missing face, a wrong colour or texture, a glow that does not fade, a part drawn through
another), the FRAME figures of both runs in the big battle, and both frame logs (`python3 -I
tools/frame_log_report.py <file>` gives the p50 / p99 and the seconds over budget).

## For STATE.md

- HWI-FEATURE(mesh-batching) (M3-R2 producer 4): the CPU-lit mesh renderers (meshObjectRenderLit 0x004b0970, the ships;
  meshSpecObjectRender 0x004b1d40, the engine glows / particle meshes) record their corners (the same lighting calls,
  in the same order) and draw each material run with one glDrawArrays from one scratch array; **default ON**; run-only
  `/meshImmediate` (main.c) and the console row `meshImmediate` (HWI row id 0x1006, CAT_CON_HWI_ROWS 7) restore the
  immediate path; the settings line's `meshImmediate=` row (no options-file row, no Options control, no key).
- Exact reuses (Shader.c, HWI-FEATURE(mesh-batching)): `shColourLit0` (shColourSet0's lighting sum, moved unchanged),
  `shColourReuseLit` (a flat polygon's next corner), `shGlowCache` / `shGlowStamp` (the glow addend once per vertex
  and object draw), `shColourCapture` (the colour stored instead of glColor4ub); `catShFastToInt` = `_mm_cvtsd_si64`
  on x86-64 Linux (glibc's lrint is that instruction), lrint elsewhere.
- Unbatched: meshObjectRender (lighting off / wireframe / single texture: the planet backdrop, particle meshes),
  the warp, morphed meshes, the edge lightning.
- Tools: `tools/hwiprof/hwiprof.c` (LD_PRELOAD wall-clock sampler, HWI-TEST) and `tools/hwiprof/hwiprof_report.py`
  (`--within`, `--lines`, `--class`, `--breakdown`); the profiling build `build/prof` (release flags + `-Dc_args=-g`).
- The 4K ai scene: batched p50 3.7-4.6 / p99 5.2-6.2 ms (the 6.94 ms target met), /meshImmediate 9.2-12.5 / 13.4-15.3.

## Delta-map review entries

Notes appended (classes unchanged, all MODIFIED already) in ghidra/delta_map_review.json and merged (`python3 -I
tools/delta_map.py merge --done m3r2:mesh-batching`: records changed 4; `tools/delta_map.py report` regenerated
docs/m2-r1-function-deltas.md): meshObjectRenderLit 0x004b0970, meshSpecObjectRender 0x004b1d40, meshSpecColour
0x004b00c0, shColourSet0 0x0050dfe0 (each compared with the exe: per-corner immediate submission, the CAT bodies
stand under /meshImmediate). No IDENTICAL body was edited: cat_shColourAddGlow 0x0050d7c0 and cat_shGlowUpdate
0x0050d560 are not in the map (CAT functions), meshShutdown has no exe body (folded into utyGameSystemsShutdown
0x005ac710's dropped calls), the hierarchy walkers (meshObjectRenderR 0x004b4110, meshObjectRenderHierarchy 0x004b4370,
meshRender 0x004b4280, meshRenderShipHierarchy 0x004b4520) are relied on unchanged and are MODIFIED rows already.

## CAT-TODOs

None added, closed or re-targeted. `/bin/grep -rn "CAT-TODO(M3-R2\|CAT-PROVISIONAL(M3-R2" src` finds only BTG.c:1151's
star size, M3-R1's, which the integrate re-targets (not this producer's scope). 37 `CAT-TODO` lines in src (unchanged).

## Counts

`/bin/grep -r "HWI-FEATURE(mesh-batching)" src | wc -l` = 160 (Mesh.c 72, Shader.c 63, Instrument.c 8, main.c 7,
Shader.h 5, Mesh.h 3, Console.c 2; the two batched functions' bodies are marked by their headers, as earlier producers'
new functions). Files: src/Game/Mesh.c, Mesh.h, Shader.c, Shader.h, Instrument.c, Console.c, src/SDL/main.c,
tools/play.sh, tools/hwiprof/hwiprof.c, tools/hwiprof/hwiprof_report.py, ghidra/delta_map_review.json,
ghidra/delta_map.json, docs/m2-r1-function-deltas.md, this report.

## What is left (owner)

- The faster frame for bigger fleets (static per-object buffers for positions / UVs, the material switches' state
  cache, the lighting): owner **M3 soak** (the 4K@144 frame-pacing row of the M3 plan); this scene is under budget.
- The unbatched renderers (meshObjectRender's planet / particle meshes, the warp, morphs, edge lightning: ~3 % here):
  owner M3 soak, if the attended hour shows them.
- The attended 4K hour at 144 Hz (the user's measurement; the overlay and /frameLog are its instruments).
