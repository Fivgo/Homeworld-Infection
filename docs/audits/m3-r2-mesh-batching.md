<!-- done: build, profile, perf, bodies, diff, pixels, streams, selftests, generated, win64, sp, doc -->
# Audit: M3-R2 port 4, mesh-batching

Producer report: docs/m3-r2-mesh-batching.md (base 83c4b26, work = `git diff 83c4b26`: 11 tracked files + the
untracked tools/hwiprof/ and the report; no commit after 83c4b26). Adversarial audit 2026-10-24. Every engine run
went through tools/m2_harness.sh (silent, offscreen), labels `m3r2-mba-*` under
original/scratch-m2/m2r2-harness/runs/, all from the audit's own clean build (frozen copies in
original/scratch-m2/m3r2-mb-audit-bin: debug sha256 1266d1ebc3c7a2c8, 90,549,392 B; release f528fd623fa5abe0,
5,739,400 B). cycle/answers/m3-r2-user.md was read first and again before this report: its later entries (the GPU
free, the overlay key queued for M3-R3, the overlay readable at 200 %, the M3-R3 pointer) do not touch this port.

## 1. Build (from clean, separate dir)
original/scratch-m2/build-audit-m3r2-mesh-batching/{debug,release}, the setup arguments of tools/build.sh (debug
`--buildtype=debug -Db_sanitize=address,undefined`, release `-Doptimization=2 -Ddebug=false -Db_sanitize=none`,
werror=true and warning_level=3 from meson.build): rc 0 / 0, **0 warnings** (274/274 each). Sizes: debug 90,549,392 B,
release 5,739,400 B. The win64 cross build (build/win64-paths.ini + tools/cross/x86_64-w64-mingw32.ini,
release_win64.sh's options) in the audit dir: rc 0, 0 warnings, hwi-engine.exe 5,351,424 B. No new source file in
src/ (tools/hwiprof is a standalone tool, not in meson); the new GL calls (glVertexPointer, glTexCoordPointer,
glColorPointer, glEnable/DisableClientState, glDrawArrays, glIsEnabled) are GL 1.1 (opengl32), none linked beyond it;
no _WIN32 code changed (catShFastToInt's cvtsd2si is under `__x86_64__ && __SSE2__ && !_WIN32`). tools/m2_layout32.sh:
OK. A profiling build (release flags + `-Dc_args=-g`) in the same dir for section 3.

## 2. The batching code read (Mesh.c, Shader.c)
* **Order / passes.** cat_meshObjectRenderLitBatched and cat_meshSpecObjectRenderBatched walk the polygons in the
  immediate loops' order. At a new material the run so far is drawn (glDrawArrays) BEFORE cat_meshMaterialSwitch, i.e.
  where the immediate path's glEnd stands, with the previous run's GL state still current; bFade's glEnable(GL_BLEND)
  follows the switch as before. So every run is drawn under its own material (texture bind / team colour scheme via
  iColorScheme, cull and two-sided state, blend, shade model) and the runs keep their order (translucent and two-sided
  materials included: nothing is sorted or merged across runs). Flat shading takes the provoking (last) corner's
  colour from the array exactly as from the last glColor4ub.
* **Lighting.** Every lighting call is the immediate path's, except the reuses: (a) MB_LIT_ONCE in the smooth modes
  (a vertex's colour depends on its normal, the material's lighting terms (shUpdateLighting, called only by the
  material switch) and its glow; per run the stamp `meshBatchRun` is new, so nothing crosses a material switch);
  (b) MB_CPV_NEXT in the flat modes: shColourReuseLit makes shColourSet0 reuse `shColourLastLit`, which the same
  polygon's first corner wrote with the same face normal (nothing between the corners calls shColourSet0); (c) the
  glow cache: cat_shColourAddGlow works the addend out once per vertex and object draw. (c) is exact only if
  cat_shGlowUpdate is idempotent within a frame: checked in the tree and in the exe (0x0050d560, section 4): with
  `universe.totaltimeelapsed == lastUpdate` the hit glow loses `0 * 0.2`, the scorch gains `(old - hitGlow) * 0.2 = 0`
  (hitGlow is already <= 1 after the first update, so no clamp moves it) and loses `0 * 0.1`, `changed` reads the
  cleared dirty flag, and the colour is recomputed (flags 0x30) from the same scorch / info / trail / team colour, so
  the second update leaves the state and the colour as the first. The addend `FAST_TO_INT(colour*256)*2` /
  the info's signed bytes is then added with the same clamp (exe 0x0050d7c0: `byte + low_dword*2`, clamped).
* **catShFastToInt.** `_mm_cvtsd_si64` = cvtsd2si; glibc's lrint/llrint on this machine is `endbr64; cvtsd2si
  %xmm0,%rax; ret` (objdump of /lib/x86_64-linux-gnu/libm.so.6 at 0x33360): the same instruction under the same MXCSR.
* **Buffers / state.** One scratch array (24-byte corners) plus three per-vertex arrays, grown by realloc in
  meshBatchReserve (a failed realloc returns FALSE and the immediate CAT body draws), freed in meshShutdown (called by
  utility.c:5269); stamps cleared on growth and on wrap. Nothing is kept per mesh, so no buffer object exists to leak.
  meshBatchBegin re-points the arrays every draw (so a realloc never leaves a stale pointer in use); meshBatchEnd turns
  the arrays off and makes the last colour / texture coordinate current (the immediate calls' end state; the GL leaves
  an enabled array's current value undefined after a draw, which this covers; an untextured run after a textured one
  gets the last glTexCoord explicitly). The Lit path restores the vertex pointer meshEnableVertexArrays leaves. Pointers
  left at the scratch array (colour / texture coordinate, and the vertex pointer after the Spec path): every other array
  user in src (BTG.c, HS.c, FEReg.c, HorseRace.c, render.c stars) sets its own pointers before drawing and every
  glBindBuffer(GL_ARRAY_BUFFER, x) is followed by a bind of 0 before any draw, so client pointers are valid at
  meshBatchBegin. The Spec path disables the normal / edge / index arrays without restoring them: no code in src enables
  them (grep), so harmless (note N1).
* Settings / switch / console: hwiSettings row 6 `meshImmediate` (no options-file row), the console row id 0x1006
  (CAT_CON_HWI_ROWS 7), hwiSettingsConsole index 6 ends the switch; the settings line reads
  `meshImmediate=0 (default)` and, with the switch, `meshImmediate=1 (switch) ... switches=/meshImmediate`
  (m3r2-mba-motion-on / -on-imm run.log).

## 3. Profile headline and the 4K frame (GPU measurements, release / profiling build)
nvidia-smi before / between / after: 6 %, 5 %, 7 %, 2 %, 2 %, 1 %, 0 % (perf runs), 0 / 0 / 0 % and 22 / 9 / 5 %
(profile runs); load average 12.7 -> 9.1 (perf), 10.8 -> 7.0 (profiles) (the user's VS Code ripgrep at ~940 % CPU);
no game running; `claude-gpu-done` after the last one.

**4K ai scene** (M3-R1's command, `--bin` the audit's release, interleaved):

| run | path | p50 ms | p99 ms | max ms | verts |
|---|---|---|---|---|---|
| m3r2-mba-perf4k-imm1 / -bat1 | /meshImmediate / batched | 8.855 / **3.365** | 12.623 / **5.124** | 13.847 / 6.337 | 25,169 both |
| m3r2-mba-perf4k-imm2 / -bat2 | | 8.426 / **3.284** | 11.160 / **4.847** | 12.332 / 5.095 | 25,169 |
| m3r2-mba-perf4k-imm3 / -bat3 | | 7.399 / **3.084** | 10.835 / **4.493** | 11.812 / 5.937 | 25,169 |

The 6.94 ms p99 target is met (batched p99 4.49-5.12). Against the producer's (batched p50 3.72-4.55 / p99 5.21-6.19,
immediate 9.24-12.47 / 13.40-15.30) the audit's figures are 10-30 % lower on BOTH paths: the load was lower here (9-13
against 12-15); the speed-up ratio is the same (p50 2.4-2.7x here, 2.4-2.8x there). Explained, not a discrepancy.

**Profile** (audit profiling build, own libhwiprof.so from tools/hwiprof/hwiprof.c, `--within rndRenderTask
--breakdown`; runs m3r2-mba-prof2-imm 22,324 samples / -bat 7,811; their p50 9.168 / 3.412 ms):

| part | /meshImmediate audit (producer) | batched audit (producer) |
|---|---|---|
| corner submission / recording | 59.40 % (58.90) | 24.81 % (23.41) |
| CPU lighting | 25.12 % (25.05) | 26.80 % (26.38) |
| batched draw (glDrawArrays) | - | 13.02 % (13.85) |
| material / texture switch | 6.87 % (7.24) | 10.74 % (12.37) |
| flip / glFinish | 0.07 % (0.07) | 0.13 % (0.05) |

By GL class: vertex calls 56.17 % -> 11.33 % (producer 56.28 -> 10.59). The headline reproduces within 1.5 points.
Finding F1 (doc, fixed): hwiprof_report.py finds the executable only by a mapped path ending in `hwi-engine`; the
audit's first profiles (binary copied as `hwi-engine-prof`, runs m3r2-mba-prof-*) gave "0 of 39,958 within
rndRenderTask" with no error. The usage text now says so (`[audit: ...]`); prof-run.sh's copy as .../hwi-engine was
right.

## 4. Bodies compared with the exe (tools/m2_fdec.py, export cataclysm_gog_r6) - 10
| function | the machine code | tree |
|---|---|---|
| meshObjectRenderLit 0x004b0970 (edited) | glShadeModel, lighting off, glVertexPointer(3, GL_FLOAT, 0x10), vertex array on + five disables, glLockArrays if the capability, glBegin(4); per material (or the forced polygon 0x00c32c0c, 0 in .bss) glEnd, the default / alternative material, glEnable(GL_BLEND) on fade, glBegin; per corner shColourSet0(normal, state, info) (CPV) then glTexCoord2f (textured) then glArrayElement; glEnd, vertex array off, GL_SMOOTH, two-side off unless shader, blend off, lighting on | the CAT body, unchanged under /meshImmediate; the batched function the same sequence recorded |
| meshSpecObjectRender 0x004b1d40 (edited) | per corner glTexCoord2f (textured), shSpecularColour inlined + glColor4ub, glVertex3fv inside glBegin / glEnd | same |
| meshSpecColour 0x004b00c0 (edited) | shSpecularColour(specIndex, 0, ...) then glColor4ub | same; the capture replaces only the glColor4ub |
| shColourSet0 0x0050dfe0 (edited) | the lighting sum, dock blend, FAST_TO_INT bytes clamped, cat_shColourAddGlow when info, the 0x80 path when the toggle is clear, glColor4ub | the arithmetic moved unchanged into shColourLit0 |
| cat_shColourAddGlow 0x0050d7c0 (edited, CAT function) | state: update then `byte + FAST_TO_INT(c*256)*2` clamped; else the info's signed bytes when its low 24 bits are set | the cache gives the same sums |
| cat_shGlowUpdate 0x0050d560 (relied on) | dirty / 0x31 / hit-glow decay `(T - last) * 0.2`, scorch `(old - hit) * 0.2`, `- (T - last) * 0.1`, recompute on 0x30 or changed, lastUpdate = T | as the tree: idempotent at equal T (section 2) |
| shSpecularColour 0x0050e280 (relied on) | ends with cat_shColourAddGlow(color, state, info) when info | same |
| shUpdateLighting 0x0050dc60 (relied on, IDENTICAL row) | the base colour and the lights' matdiffuse from the material; called from the material switch only | the per-run lighting state the reuses assume |
| meshObjectRenderHierarchy 0x004b4370 (relied on) | GL path -> meshObjectRenderLit; the RGL-device branch (0x00b3a6d8: cat_meshObjectRenderLitTransformed / -Immediate) not taken on GL; g_SpecHack -> meshSpecObjectRender | the base the batching sits on is the exe's immediate path |
| utyGameSystemsShutdown 0x005ac710 | no meshShutdown call (the empty function folded away) | the claim stands |

No IDENTICAL body was edited (the four edited functions are MODIFIED rows with the producer's review notes; the CAT
helpers are not in the map). Generated files: `tools/delta_map.py merge --review ghidra/delta_map_review.json --done
m3r2:mesh-batching` on 83c4b26's delta_map.json ("records changed 4", reviewed 3,579) and `report` reproduce
ghidra/delta_map.json and docs/m2-r1-function-deltas.md byte for byte.

## 5. Pixels (A/B, the audit binaries)
Dumps (debug, 1024x768, 144 fps virtual, nis n06:26:36; whole / `sed 's/ crc=[0-9a-f]*//'`, first 16 hex of sha256):

| dump | run | whole | stripped | frames |
|---|---|---|---|---|
| motion on | m3r2-mba-motion-on | c0e411662fa2f527 | 428ef41479eb5749 | 1,431 |
| motion on, /meshImmediate | m3r2-mba-motion-on-imm | c0e411662fa2f527 | 428ef41479eb5749 | 1,431 |
| motion /noInterp | m3r2-mba-motion-noi | c503c869f0dc9cdb | 3016280e9e0dd3c8 | 1,431 |
| nis on | m3r2-mba-nis-on | 0307f3be2044c29c | a5de13a44b8d264f | 1,440 |
| nis on, /meshImmediate | m3r2-mba-nis-on-imm | 0307f3be2044c29c | a5de13a44b8d264f | 1,440 |
| nis /noInterp | m3r2-mba-nis-noi | 86cb155acffc180e | 196bab32d244f614 | 1,440 |

All equal to M3-R1's and the previous audit's; the batched and immediate dumps whole-equal, so every frame's crc=
equal; no sub-LSB frame exists to explain. Independent read-back (own P6 reader, a few lines of Python): the
/catMotionShot frames motion 700 and nis 900, batched vs immediate: 0 differing pixels, largest difference per channel
[0, 0, 0] (48,780 / 47,952 distinct colours: real ship frames).

Captures (release, `cmp` of every file against its /meshImmediate twin; and against the producer's batched files):

| capture | files | size | differ A/B | differ vs producer |
|---|---|---|---|---|
| ai fleet (`--ai-ticks 420 --res 3840x2160 --lodstats 150-410:fleet --shot 200,300,400`) | 3 | 3840x2160 | 0 | 0 |
| Mission01 NIS-first (`missiontest --mission-test-ticks 2000 --res 3840x2160 --shot 100-1900/150`) | 25 | 3840x2160 | 0 | 0 |
| render self-test (`--shot 20-599/20`) | 29 | 1024x768 | 0 | 0 |
| hud self-test (`--shot 20-599/40`) | 15 | 1024x768 | 0 | 0 |
| audit extra: 4,000-tick AI battle (`--lodstats 150-3990:fleet --shot 1000-3900/300`) | 10 | 1024x768 | 0 | - |

Python on two 4K pairs (Mission01 s1 tick 1000, ai tick 300): 0 differing pixels, max [0, 0, 0]. The profile shows the
batched path really ran in the batched runs (glDrawArrays 13 %) and not in the immediate ones, so the A/B is not
vacuous. (The 2,000-tick missiontest capture runs end "objectives=0/6 result=FAIL" by design: the cut.)

## 6. Streams and self-tests (audit binaries; hwo_compare.py, every field incl. rng_all)
| stream | run | sha256 (16) | result |
|---|---|---|---|
| headless Mission01 6,000 (debug) | m3r2-mba-mission-a / -b | b5388ee24a6d7972 both, 1,476,370 B | **IDENTICAL to m3r1i-mission-a/run.hwo (sha256 1ae66610c3f02410...) ticks 0-6000** |
| same, /meshImmediate | m3r2-mba-mission-imm | b5388ee24a6d7972 | IDENTICAL |
| same, release | m3r2-mba-mission-rel-a | 56c18be474650c55 | IDENTICAL to m3r1i-mission-rel-a |
| missiontest 01 (NIS-first) | m3r2-mba-missiontest-a | fe8da9e1a2e14e40 | 6/6 PASS done 11,424; IDENTICAL to m3r1i-missiontest-a (0-11425) |
| ai pair | m3r2-mba-ai-a / -b | ae1b04eb9e9df6bf both | 16/16 PASS; IDENTICAL to m3r1i-ai-a |
| ai soak 4,000 | m3r2-mba-ai-soak | 47208c8a810ff9b9 | 16/16 PASS, ASan 0, no fatal; IDENTICAL to m3r1i-ai-soak in all 21 fields (rng_all too) |

Self-tests: render PASS (frames 583, objects 582), hud PASS (boxed 19, selected 154), meshes 1,758 files 0 bad,
console PASS (shown 36, set 29), toggle PASS. Every run ASan 0, no LeakSanitizer report. **No stream or dump moved;
nothing to re-baseline; no rng_all draw added.**

sp chain (debug, --timeout 1200): m3r2-mba-sp reached Mission12 (Mission12.btg loaded, 12 animatics skipped
headless), rc 124 at the timeout as M3-R1; m3r2-mba-sp-4k (--res 3840x2160) reached Mission11 (11 animatics) as
m3r1i-sp-4k. Both ASan 0, no LeakSanitizer report, UBSan kinds and counts equal to m3r1i-sp / m3r1i-sp-4k (the
summaries differ only in the wall-clock-dependent memory-report tail).

## 7. Diff review (git diff 83c4b26)
Scope: only the producer's files (no commits after 83c4b26; the working tree holds the port). HWI-FEATURE(mesh-batching)
160 (Mesh.c 72, Shader.c 63, Instrument.c 8, main.c 7, Shader.h 5, Mesh.h 3, Console.c 2) as reported; untagged added
lines are only inside the tagged helpers' bodies, the tagged struct / typedef bodies and comments, the convention the
M3-R1 audits accepted (helpers tagged at the signature and block comment). No Cataclysm behaviour changed (no CAT line
needed); no CAT-TODO / CAT-PROVISIONAL added (`git diff 83c4b26 | grep '^+' | grep -c` = 0; 37 in src; the only
M3-R2-named one is BTG.c:1151, M3-R1's, the integrate's). tools/hwiprof/* carry HWI-TEST. play.sh documents the switch,
the console row and the settings line. No change outside the scope; no _WIN32 code touched.
* F1 (doc, fixed): section 3.
* N1 (note, not changed): the Spec batched path restores the vertex / texture / colour array enables it found but not
  the vertex pointer, and it turns off the normal / edge / index arrays it found; nothing in src enables those arrays
  or draws from a vertex pointer it did not set, so no pixel or state consequence today. The report's "puts the array
  enables back as found" is true for the three it names.
* N2 (note): the exe's hierarchy has an RGL-device branch with its own lit renderers (0x00b3a6d8); not on the GL path,
  so the base is right.

## 8. DONE WHEN, re-checked
* 10 ms profiled by function and by GL call class: reproduced (3).
* HWI-FEATURE(mesh-batching) batching by material into arrays behind the run-only /meshImmediate; the unbatched paths
  listed (meshObjectRender 2.87 % / audit 3.38 %, warp, morph, edge lightning 0 %): yes.
* 4K p50 / p99 with and without /meshImmediate on an idle GPU, vertices equal, the 6.94 ms p99 met: yes (3).
* Dumps' crc= and the 4K captures EQUAL between the paths frame by frame: yes (5). The four default dumps EQUAL to the
  previous audit's: yes.
* render, hud, meshes PASS; Mission01 pair IDENTICAL in every field; ai pair and soak IDENTICAL to M3-R1's; no buffer
  leaked (nothing per mesh; LeakSanitizer silent); sp and sp 4K ASan 0: yes (6).
* The user's check: `tools/play.sh -- /overlay /frameLog <file>` and `-- /overlay /meshImmediate /frameLog <file>`:
  play.sh passes everything after `--` to the engine; /overlay, /frameLog and /meshImmediate are registered in main.c's
  commandOptions; the overlay's first line is "FRAME .. MS  MED ..  P99 .." (Instrument.c:673); Mission01's opening
  cutscene and the first minutes of Mission01 are reachable from New Game; the frame-log report tool exists. Correct.

## 9. Precision
Sampled producer claims checked: 34 - the profile headline by part (immediate, batched) and by GL class, the GPU
share, the corner count (46,255 polys x 3), the 4K p50 / p99 of both paths (explained 10-30 % lower here at lower
load), the 6.94 ms target, the equal vertices, the four default dumps, the /meshImmediate dump equality, the four
capture sets A/B, the Mission01 pair and its hash, the /meshImmediate mission run, the release pair and its hash, the
missiontest guard rail, the ai pair, the soak, render, hud, meshes, console (36 / 29), toggle, sp, sp 4K, the glibc
lrint instruction, the exe base of the two renderers and meshSpecColour, the folded meshShutdown, the reuse
exactness (glow idempotence), the delta-map merge (4 records) and generated files, the HWI count (160 and per file),
CAT-TODO count, the win64 build, the layout check, the /frameLog figures (p50 4.8 / p99 7.1 / 1.0 %; 11.4 / 15.5 /
94.8 %, the producer's frame.log files), the settings line, PLAN.md's ~47k vertices - wrong: 0. The one inaccuracy
found is an omission in a tool's usage text (F1), not a wrong claim. Precision 34 / 34 = 1.00.

Coverage note: the hit-glow branch of the cache (vertices with info flag 0x01 on recently hit ships) is proven
exact by reading the code and the exe (section 2), not by a capture that is known to contain a hit ship; the trail /
team-colour glow (flags 0x30) and the info-offset branch are in every captured ship frame.

Fixes in place: 1 (F1: hwiprof_report.py usage text).

Next port's reference: the audit binaries 1266d1ebc3c7a2c8 (debug) / f528fd623fa5abe0 (release); every stream and dump
above equal to M3-R1's in content.

Verdict: PASS-WITH-FIXES - FULL DETAIL at 4K is inside the 144 Hz budget (batched p99 4.5-5.1 ms vs 10.8-12.6 immediate, vertices equal); every dump and capture byte-equal between the paths and to the baselines; Mission01 pair, missiontest, ai pair and soak IDENTICAL in every field; profile headline reproduced; one tool-doc fix
