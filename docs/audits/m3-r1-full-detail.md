<!-- done: build, exe-read, diff-review, fixes, fence, perf -->
# M3-R1 audit: full-detail (producer 3, FULL DETAIL = LOD 0 always, default ON)

Auditor's scope: the producer's report docs/m3-r1-full-detail.md and `git diff 2b454b6` in the m2 worktree (no commit
after 2b454b6: the whole port is the working tree; every changed path is in the producer's scope: src/ LOD / render /
options / console / main / utility / the draw sites, tools/m2_harness.sh, tools/play.sh, ghidra/delta_map*.json, the
report). `<R>` = original/scratch-m2/m2r2-harness/runs, `<B>` = original/scratch-m2/build-audit-m3r1-full-detail.
Every engine run went through tools/m2_harness.sh (silent, offscreen); no Ghidra, no GOG game, no input, no gamescope.

## 1. Clean builds

`rm -rf <B>` then exactly tools/build.sh's setups: `meson setup --buildtype=debug -Db_sanitize=address,undefined
<B>/debug` and `meson setup -Doptimization=2 -Ddebug=false -Db_sanitize=none <B>/release`, `meson compile` each:
rc 0 / 0, **0 compiler warnings** (werror=true, warning_level=3 in both intro-buildoptions); debug 88,996,072 B,
release 5,648,760 B (the producer's tree). After the audit's fixes: `meson compile` of both again rc 0, 0 warnings
(debug sha256 6717b2550815edaf, release 721eee6cd11012d0), and the worktree's `tools/build.sh --debug` / `--release`
rc 0 (the only "WARNING" line is meson's sanitizer notice), `tools/m2_layout32.sh` rc 0 ("all ILP32 layout asserts
hold").

## 2. The exe, read again (tools/m2_fdis.py / m2_fdec.py / raw bytes of original/game/cataclysm.exe)

| check | evidence | producer |
|---|---|---|
| opNoLODVal 0x00a4cc34: 9 code references + the options row | byte scan for `34 cc a4 00`: code at 0x4d814f/0x4d8ff0/0x4d9d6e/0x4d9d8a/0x4d9dc2/0x4da652/0x4db827/0x4db97a/0x576c03, data at 0x008c14b0 (the noLOD row) | correct |
| the only reader outside the options code is rndMainViewRenderFunction at 0x00576c03 | the list above (0x576c03 the only one outside 0x4d7f80..0x4dbb30) | correct |
| the level loop | 0x00576baa..0x00576c18: pass 0 `call 0x4a4d70`, else `call 0x4a5000` (pass); `mov eax,[0xa4cc34]; test; jne 0x576c1a; cmp esi,[0x88486c]; jbe; inc ebx; cmp ebx,0xb; jb` | correct |
| pass table 0x0072be84 = 99,3,2,1,0,3,2,1,3,2,3 | bytes 63 03 02 01 00 03 02 01 03 02 03 | correct |
| cat_lodLevelGetTimed 0x004a5000: an object AT the pass's level drops one, clamped | decompile; tree LOD.c identical in logic | correct |
| lodLevelGet 0x004a4d70 never reads FULL DETAIL; bands from catLodDetail 0x00a4cc38 | decompile; floats 0x0072a674 0.4, 0x0072ab84 0.12, ab80 0.045, ab7c 0.017, ab78 0.0065, a460 0.0; tree LOD.c the same | correct |
| the distance-table flag "+0x20 of the table" | `mov al, BYTE PTR [ecx+0x8]` at 0x004a4dce (tree: lodinfo.catDistanceLOD, "+8") | **wrong: +8** (report and review note fixed) |
| slider = 30 - value, 0..21, default 10 (0x0088be6c) | opDetailThreshold 0x004da5e0: max 0x15, value 0x1e - val; opDetailThresholdProcess 0x004d9b20; .data word 10 | correct |
| cat_lodSetTimeThreshold 0x004a50e0 = frequency / fps | `__alldiv(lpFrequency, fps)` | correct |
| the slider greyed by FULL DETAIL (region 0x00a4cc70, \|4 / &0xfffb, \|8) | opNoLOD 0x004d9d60 + opDetailThreshold decompile | correct |
| cat_opLODDetail 0x004d9d20 = the DetailQuality radio (catLodDetail) | decompile; tree Options.c the same | correct |
| cat_opSetAllDefaults 0x004db7b0: both presets write opNoLODVal 0; no OP_NoLOD toggle refresh | decompile (opNoLODVal = 0 in both branches; the feToggleButtonSet list has no OP_NoLOD) | correct |
| effects cap: etgFrequencyExceeded 0x00470bd0 with 31 callers; high preset = 256 | call_graph.csv: 31 distinct callers, the function calls ranRandomFn; preset `0x100 - etgHistoryScalarMin`, general.script `etgHistoryScalarMin 64`; tree `etgHistoryScalar = opNumEffects + etgHistoryScalarMin` | correct (the M3-R2 deferral stands: the cap draws the random stream in the universe update) |

The producer's statement of what FULL DETAIL does in the exe (only the timed passes stop; the bands still act; the
slider is a frame-rate floor only acting through the passes) is right. The tree already matched; the only CAT: line
is the comment in lodLevelGet, which is accurate.

**12 bodies compared with the exe** (cataclysm_gog_r6): lodLevelGet 0x004a4d70 (MODIFIED, as the tree),
cat_lodLevelGetTimed 0x004a5000, cat_lodSetTimeThreshold 0x004a50e0, cat_opLODDetail 0x004d9d20, opNoLOD 0x004d9d60
(IDENTICAL stands: the tree adds HWI-FEATURE lines only), opDetailThreshold 0x004da5e0, opDetailThresholdProcess
0x004d9b20, cat_opSetAllDefaults 0x004db7b0, rndMainViewRenderFunction's loop 0x005764e0, **nebRenderNebula
0x004cadb0 (IDENTICAL in the delta map, edited by the port, no review row: finding F-3)**, cat_rndRenderObjectList
0x005776f0 (the renderedLODs bit is set only after meshRender in the LT_Mesh case; case 3 = SubPixel draws a point and
sets nothing: finding F-1), cat_meshObjectRenderEdgeEffect 0x004b4a60 (tree: stream-0 bolts per a tenth of the
polygons, segments by the level: the producer's reason for keeping the edge lightning on the exe's level holds).

## 3. The diff (`git diff 2b454b6`), findings

Scope: every change is the producer's (no user / orchestrator commit after 2b454b6). Layout untouched (m2_layout32
OK). No CAT-TODO / CAT-PROVISIONAL added; `/bin/grep -rn "CAT-TODO(M3-R1\|CAT-PROVISIONAL(M3-R1" src` = 0.
`git diff 2b454b6 -U0 -- src`: every added line carries HWI-FEATURE / HWI-TEST / CAT, or is a comment or an HWI-TEST
block line (the /catLodTest / /catLodStats code, as earlier rounds' self-tests), except F-2.

- **F-1 (correctness, latent simulation reader; fixed in code).** The report's (d) says `renderedLODs` is "set from
  `currentLOD` (unchanged)". The bit is set inside the LT_Mesh draw path (render.c ship mesh path and the generic
  asteroid / derelict / missile path). Under FULL DETAIL an object whose exe level is NOT a mesh is drawn through the
  mesh path at LOD 0 and sets `1 << currentLOD`, which the exe never sets (exe 0x005776f0: `renderedLODs |= ...` only
  after `meshRender` in case 1; case 3 `primPointSize3`). In the data (original/game_extracted/cataclysm/**/*.lod: 11
  tables with a non-mesh level) the planet derelicts (planetoforigin, planetoforigin_scarred, homeworld, mattworld,
  lavaball, galaxy) have `type1 SubPixel`, and the dust clouds / nebula `type4 Subpixel` (the cloud path sets no bit).
  `kasfRenderedDerelicts` (KAS RenderedDerelictType) reads these bits; the shipped scripts call it only for Shipwreck
  (Mission02) and PrisonShipOld (Mission08), whose tables (djunk2shipwreck.lod, dprisonship.lod) are all MESH, and the
  AI's aitCheckAmIBeingWatched reads ships only (every ship table is all MESH): so no stream moved, but the claim "no
  simulation reader of the level the setting changes" was not true by construction. **Fix:** both sites set the bit
  only when the exe's level (`cat_lodCurrentLevelGet`) is LT_Mesh (HWI-FEATURE(full-detail), "[audit: ...]"); with
  FULL DETAIL off the condition is always true there (drawn = exe level), so the setting off stays the exe bit for
  bit. Re-run on the fixed binary: Mission01 pair, release, Mission01/02/04/08 self-tests (02 and 08 are the two
  RenderedDerelictType / RenderedShips missions), ai pair, LiveFireExercise2, the four default dumps, render, hud,
  lodtest: all IDENTICAL / PASS (section 5).
- **F-2 (tagging; fixed).** Three changed render.c lines had no tag: the two `...->level[drawLOD].pData,` continuation
  lines of the Beast / salvage conversion draws and the edge effect's `else if` (the exe's `if`). Tagged
  HWI-FEATURE(full-detail). `/bin/grep -r "HWI-FEATURE(full-detail)" src | wc -l`: 169 (producer, verified) -> 178
  (after F-1 / F-2), 14 files.
- **F-3 (review entry; fixed).** nebRenderNebula 0x004cadb0 is IDENTICAL in ghidra/delta_map.json and the port edits
  it (the tendril level is 0 under FULL DETAIL) without a review row. Compared with the exe: nebLOD inlined
  (univSpaceObjInRenderList of both chunks), nebColourTendril, the far test, nebRenderChunk / nebRenderTendril with that
  level: as the tree; IDENTICAL stands. Row added to ghidra/delta_map_review.json (sample port-M3-R1-full-detail).
- **F-4 (fact; fixed).** "+0x20" for the distance flag (report (a) and the lodLevelGet review note): it is +8 (above).
- **F-5 (generated file; fixed).** `tools/delta_map.py merge` reproduces ghidra/delta_map.json (records changed 0 on
  a copy), but `tools/delta_map.py report` did not reproduce docs/m2-r1-function-deltas.md: its done marker lacked
  `m3r1:full-detail` (the producer merged without regenerating). After F-3 / F-4: `merge --done
  m3r1:full-detail-audit` (records changed 2) and `report` (regenerated; IDENTICAL 2762 / MODIFIED 1575 / REWRITTEN 83,
  reviewed 3573).

Checked and correct (no finding): the edge lightning kept on the exe's level (its stream-0 draws count polygons and
segments; hierarchy vs rest-matrix walk visit the same objects, so the draw count is the same); the dust / gas cloud
level (cloudRenderSystem's RANDOM_CLOUDS draws depend on the bolt length, the level only picks the ellipsoid and the
bolt width); the nebula (both tendril levels' geometry built at once with the RANDOM_NEBULAE draws); the trails' style,
the ACV link bolts and the ship lightning keep `currentLOD`; the nozzle (cat_shipNozzleInfoGet / trailGetNozzleOffset)
reads the level drawn, positions only; the mesh state is allocated for every level (catMeshStateSize(nLevels)); every
other `currentLOD` reader (my grep: Gun, ETG, Damage, UnivUpdate effect levels, Select / PopupMenu, Leech, Crates,
Salvage, Sentinel, Slipgate, Worker, Processor, RammingFrigate, GravWell, GenericInterceptor, RepairCorvette, DFG,
RepairDroid runtime, HeavyCorvette, NetCheck, the creators' `= 0`) is unchanged and reads the exe's level; src/Win32 is
not compiled. The /lodBands switch is run-only (catLodFileValues around utyOptionsFileWrite), the console row is
HWI row 0x1003, the default 1 is persisted by the exe's noLOD row; the user's play-home Cataclysm.cfg (read only) has
`noLOD 1` and `detailThreshold 10`. tools/play.sh's header documents the row, the console row, `-- /lodBands` and the
checkbox; the harness header documents --lodtest / --lodstats and the toggle checks.

## 4. Precision

46 sampled checks of the producer's claims (the 13 exe rows above, the 12 bodies' "as the tree", the reader table,
the tag count, the review rows, the 4K figures, the lodtest / toggle / console / fescreens numbers, every stream and
dump row I reran, the crc-move counts 603 / 605 / 1,260 / 1,267, the CAT-TODO grep, the user's check): **wrong 4**
(F-1 the renderedLODs claim, F-2 "every line tagged", F-3 the review rows complete, F-4 +0x20); F-5 is a process miss,
not a claim. Precision = 42 / 46 = **0.913**.

## 5. The fence (the audit's binaries; ASan 0 in every run)

Pre-fix = the producer's tree (labels `m3r1-afd-*`, debug sha256 of <B>/debug before the fix not kept; built from
the tree at 01:11); fixed = after F-1 / F-2 (labels `m3r1-afd2-*`, debug 6717b2550815edaf, release 721eee6cd11012d0).
All compared with `python3 -I tools/oracle/hwo_compare.py` (the 21 fields).

| stream | run (sha256 / size, first 16 hex) | vs | result |
|---|---|---|---|
| headless Mission01, 6,000 ticks | `m3r1-afd2-mission-a` / `-b` (61936514c76cc61f / 1,476,370 B, 6,001 T) | r10i-mission-a (5bd51234405ea0d8, present) | **IDENTICAL in every field** (ticks 0-6000, both); pre-fix `m3r1-afd-mission-a` / `-b` IDENTICAL too |
| the same, release | `m3r1-afd2-mission-rel` (9739a7da83101f85) | r10i-mission-rel-a (a44d585d71233c4a) and r10i-mission-a | IDENTICAL |
| the same, `-- /lodBands` | `m3r1-afd-mission-lb` (61936514c76cc61f) | r10i-mission-a | IDENTICAL |
| missiontest Mission01 | `m3r1-afd2-m1` (56e54bd089f9ecea) | r10i-missiontest-a | IDENTICAL, 6/6, done 11,424 (pre-fix `-afd-m1` the same) |
| Mission02 | `m3r1-afd2-m2` (4df2d55272e906bb) | r10i-m2-a | IDENTICAL, 6/6, 25,936 |
| Mission04 | `m3r1-afd2-m4` (7b39e4351913fac1) | r10i-m4-a, m3r1-fd3-m4 | IDENTICAL, 5/5, 20,818 (pre-fix too) |
| Mission08 | `m3r1-afd2-m8` (c46ccb3984c81cc0) | r10i-m8-a | IDENTICAL, 6/6, 29,361 |
| Mission09 | `m3r1-afd-m9` (8a95eb2cc67048da) | r10i-m9-a, m3r1-fd3-m9 | IDENTICAL, 6/6, 20,617 |
| Mission17 | `m3r1-afd-m17` (050d9e306d85d997) | r10i-m17-a, m3r1-fd3-m17 | IDENTICAL, 4/4, game end 15,584 (credits) |
| ai pair (400) | `m3r1-afd2-ai-a` / `-b` (8582f520b214b2b3) | r10i-ai-a | IDENTICAL (pre-fix pair too) |
| ai soak (4,000) | `m3r1-afd-ai-soak` (3ca3b101bdf68965) | r10i-ai-soak | IDENTICAL in all 21 fields (rng_all included this time; vs the producer's m3r1-fd3-ai-soak rng_all from 1316 only: decision 84) |
| LiveFireExercise2 | `m3r1-afd2-skirmish-lfe2` (3ee8b46a048adcae) | r10i-skirmish-lfe2-a | IDENTICAL (pre-fix too) |
| Armed8 | `m3r1-afd-skirmish-armed8` (aa3b7fd421469b9e) | r10i-skirmish-armed8 | IDENTICAL |
| replay 10-17 | `m3r1-afd-r17` (fed0cefb26d33248, 9,601 ticks / 4,800 packets) | r10i-r17-a | IDENTICAL; vs GOG's oracle01-rec.hwo (--from-tick 1, the R10 list less rng6 / real): **FIRST DIVERGENT TICK 2823**, ships_vel (the rung unchanged) |

**Dumps** (whole sha256 / crc= stripped, first 16 hex):

| dump | producer 2 audited (`m3r1-aud-tcw-*`) | `/lodBands` (`m3r1-afd-*-lb`) | default, pre-fix (`m3r1-afd-*`) | default, fixed (`m3r1-afd2-*`) = the next port's reference |
|---|---|---|---|---|
| motion /noInterp | 183681235a3fdc7b / 3016280e9e0dd3c8 | **183681235a3fdc7b** / 3016280e9e0dd3c8 | c503c869f0dc9cdb / 3016280e9e0dd3c8 | c503c869f0dc9cdb / 3016280e9e0dd3c8 |
| motion on | 428fff6c765ab81b / 428ef41479eb5749 | **428fff6c765ab81b** / 428ef41479eb5749 | c0e411662fa2f527 / 428ef41479eb5749 | c0e411662fa2f527 / 428ef41479eb5749 |
| nis /noInterp | 2a7fec4de8cfe716 / 196bab32d244f614 | **2a7fec4de8cfe716** / 196bab32d244f614 | 86cb155acffc180e / 196bab32d244f614 | 86cb155acffc180e / 196bab32d244f614 |
| nis on | 46df1d391a86a404 / 7ce8472514254f4f | **46df1d391a86a404** / 7ce8472514254f4f | da532dafedb89960 / 7ce8472514254f4f | da532dafedb89960 / 7ce8472514254f4f |

With /lodBands every dump equals the previous audit's; the default's crc= moves (counted from the producer's files
against m3r1-aud-tcw: motion 603 / 605 of 1,431 frames, nis 1,260 / 1,267 of 1,440, all from frame 0) match the
producer's statement; the default dumps reproduce the producer's hashes exactly and are unchanged by the audit's fix.

**Self-tests:** lodtest `m3r1-afd-lod-ai-on` (ai, 300:fleet): 164 objects, bands 4 x132 / 3 x10 / 2 x22, drawn 0 for
all, 25,169 vertices, PASS; `-lod-ai-bands`: drawn = band for all 164, 5,330 vertices, PASS; `-lod-far-on` /
`-lod-far-bands` (render, 300:far, camera 14,500): 1 object band 2, drawn 0 / drawn 2, PASS / PASS; `-lod-m1-on` /
`-bands` (Mission01 opening NIS, 800): 30 objects, 24 above band 0, drawn 0 (2,892 vertices) / drawn = band (1,791),
PASS / PASS; fixed binary `m3r1-afd2-lod-ai-on` the same. toggle `m3r1-afd-toggle` / `-toggle-sw` (-- /lodBands):
`toggletest: done start=1/1 fails=0 result=PASS` both, the fulldetail lines (start noLOD=1 / 0 switch=0 / 1, clicks 0 /
1, accept 0 / 1 read back, presets low=0 high=1, switch run-only row=1). console `m3r1-afd-console`: shown=33 set=26
failures=0 PASS. fescreens `m3r1-afd-fescreens`: `optionstest: done options=35 startdefaults=35 readback=35
result=PASS`, 22 files / 195 screens, wrong=0, PASS. hud (`-afd-hud`, `-afd2-hud`) and render (`-afd-render`,
`-afd2-render`): PASS. fe `m3r1-afd-fe`: Main_game_screen at the 120 s timeout (as every fe run), ASan 0.
**sp** `m3r1-afd-sp` (pre-fix) and `m3r1-afd2-sp` (fixed), `--timeout 1200`: Mission01_NIS01 .. Mission12 (12_NIS07), 12
animatics skipped, 23,289 file loads, **ASan 0**, no leak; the UBSan kinds and counts the same as the producer's
`m3r1-fd3-sp` (diff of the per-site counts: none).

## 6. The 4K frame time (GPU measurement)

nvidia-smi before 13 %, after each run 12 / 9 / 10 / 10 % (no game; ps: no .exe running). Release (<B>/release,
fixed), ai, `--res 3840x2160 --lodstats 150-450:fleet`, 301 frames each:

| run | FULL DETAIL | median ms | p99 ms | max ms | verts |
|---|---|---|---|---|---|
| `m3r1-afd-perf4k-on` | on | 11.470 | 14.222 | 15.930 | 25,169 |
| `m3r1-afd-perf4k-on2` | on | 11.946 | 16.934 | 18.472 | 25,169 |
| `m3r1-afd-perf4k-bands` | off | 3.146 | 4.764 | 5.589 | 4,726 |
| `m3r1-afd-perf4k-bands2` | off | 3.482 | 4.889 | 5.196 | 4,726 |

Against the producer's (on 10.466 / 9.819, off 2.713 / 3.308): my on medians are +9.6 % / +14.1 % over its first run,
+15.5 % over the mean of its two; off +10 % over its mean. Explained: the machine was not CPU-quiet (load average ~16:
a VS Code ripgrep at ~1000 % CPU and two ASan sp runs of this audit), and the cost is vertex submission on the CPU
(the producer's 1024x768 run is as slow); the ratio on / off is the same (3.5x mine, 3.4x the producer's) and the
vertex counts are exact. `claude-gpu-done` run after the last measurement.

## 7. DONE WHEN, re-checked

What FULL DETAIL and the slider do, from the machine code: yes (section 2; one offset wrong, fixed). LOD 0 in every LOD
site under the setting: yes (the draw sites read cat_lodDrawLevel / drawLOD; lodtest at a far camera all 0, off the
bands above 0). Default ON, persisted by noLOD, run-only switch and console row documented: yes. Effects cap verdict:
stated (M3-R2, the random draw in the universe update), confirmed. No simulation reader of the drawn level: true after
F-1 (before it, a latent one: renderedLODs of a non-mesh exe level). 4K on / off on an idle GPU: reported, reproduced
(section 6). Mission01 pair IDENTICAL to the baseline in every field: yes. The per-mission self-tests I reran (01, 02,
04, 08, 09, 17) PASS with every objective and IDENTICAL (the producer's 17 claimed; 02 / 08 added for F-1). The rung
2823: yes. Dumps with /lodBands = the previous audit's: yes. fe, hud, render, fescreens PASS: yes. sp --timeout 1200
ASan 0: yes (both binaries, Mission12 reached). THE USER'S CHECK names `tools/play.sh` (and `tools/play.sh -- /lodBands`, a valid play.sh form:
everything after `--` goes to the engine), New Game -> the Mission01 opening cutscene and Mission01, a skirmish, the
Options -> Video FULL DETAIL checkbox (OP_NoLOD on Video_Options): all exist.

## 8. Fixes (9)

1. render.c ship path: renderedLODs set only when the exe's level is a mesh (F-1).
2. render.c asteroid / derelict / missile path: the same (F-1).
3. render.c: 3 untagged changed lines tagged HWI-FEATURE(full-detail) (F-2).
4. ghidra/delta_map_review.json: lodLevelGet note +0x20 -> +8 (F-4), merged.
5. ghidra/delta_map_review.json: nebRenderNebula 0x004cadb0 review row (F-3), merged (`--done m3r1:full-detail-audit`).
6. docs/m2-r1-function-deltas.md regenerated (F-5).
7. docs/m3-r1-full-detail.md (a): +0x20 -> +8 "[audit: ...]".
8. docs/m3-r1-full-detail.md (d): the renderedLODs row "[audit: ...]".
9. docs/m3-r1-full-detail.md: the tag count and the review-rows section "[audit: ...]".

## 9. For the next port (the references)

The mission stream is unchanged (IDENTICAL in every field to r10i-mission-a); every guard rail above is IDENTICAL to
R10's r10i-* runs; the four dumps' references are the default (FULL DETAIL on) hashes of section 5's last column
(c503c869f0dc9cdb, c0e411662fa2f527, 86cb155acffc180e, da532dafedb89960; crc-stripped unchanged since R10), with
`-- /lodBands` the previous audit's. The audit's code fix (F-1) moved nothing.

Verdict: PASS-WITH-FIXES - FULL DETAIL is the exe's own checkbox with the recreation's LOD-0 draw on top, default on, no stream moved (Mission01 pair IDENTICAL to r10i-mission-a, rung 2823); 9 fixes: renderedLODs guarded to the exe's mesh path (a latent non-mesh-level sim-reader deviation), 3 untagged lines, nebRenderNebula review row, +0x20 -> +8, the stale function-deltas report, 4 doc notes
