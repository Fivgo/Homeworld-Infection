# Audit: M2-R10 render-animatics-sp (producer 2: the Mission04 render stop, the animatics, the sp gate)

Base `36289a8` (producer 1's audited tree); the port is `git diff 36289a8` in the m2 worktree (no WIP commit: the
working tree only). Report: `docs/m2-r10-render-animatics-sp.md`. `<R>` = `original/scratch-m2/m2r2-harness/runs`,
`<B>` = `original/scratch-m2/build-audit-r10-render-animatics-sp`. Every engine run went through
`tools/m2_harness.sh` (plain harness, silent, offscreen) with `--bin <B>/debug/hwi-engine` unless stated. The machine
was loaded during the audit (a game and an editor's indexer; load average 16-28).

## 1. Build (clean, separate dir)

`rm -rf <B>`, then exactly `tools/build.sh`'s setups: `meson setup --buildtype=debug -Db_sanitize=address,undefined`
and `meson setup -Doptimization=2 -Ddebug=false -Db_sanitize=none` (meson's `werror=true`, `warning_level=3`), `meson
compile`: **rc 0 / rc 0, 0 warnings**. Debug 88,608,488 B (`8f55d2e67345808e`), release 5,618,896 B
(`d497dca076955d84`). After the audit's comment fixes (section 5) both rebuilt incrementally: rc 0, 0 warnings, Select.c
recompiled, **both binaries byte-identical** to the first build (comment-only, line numbers unchanged).

## 2. Bodies compared with the exe (`cataclysm_gog_r6`, tools/m2_fdec.py / m2_fdis.py / m2_exedata.py)

| exe body | result |
|---|---|
| trTextureRegister 0x00585760 | the append at `LAB_005858ef` has no bound: first a free (0/0) slot, else write at `nPalettes` and `nPalettes++` (a signed byte at +0xd); the colour block is `memAllocFunction(0x40)` at registration (8 schemes). The report's statement (a ninth scheme is written past the block) is right; the tree's CAT-FIX (palette 0 + warning) is marked |
| trColorsEqual 0x005856b0 | as the tree (pending / shared: +0x14; loaded paletted: palettes + 0x400 * nPalettes; RGB: + 4 * nPalettes; zero slots skipped) |
| trTextureUnregister 0x005860d0 | `nUsageCount--`, then the slot reset only when `flags & 1` (pending) **and** `nUsageCount == 0`: the tree's port matches |
| trTextureDelete 0x005854c0 | after the field clears, `if (+0x14 != NULL) memFree(+0x14); +0x14 = 0` (0x005855b5 .. 0x005855e8, checked in the disassembly); the port matches. No double free: trInternalTexturesDelete leaves +0x14 = its fresh "dupe" list, which is what gets freed |
| trTextureDeleteAllUnregistered 0x00586140, trRegisterRemoval 0x00585e70, trTextureColorsUpdate 0x00585fa0 | as the tree (the review entries' IDENTICAL is right) |
| meshFree 0x004af710 | every handle of the 8 but -1 is unregistered; the tree's HWI fix matches |
| animBinkPlay 0x00421860 vs animAviPlay | the exe: `cat_taskTimeCompressionReset`, `tutorial == 1` return, then the state resets, the empty-filename return, the playback, `DAT_008df988 = 8`. The tree: the same order; the HWI headless skip sits after the tutorial test and before the resets (returns FALSE) |
| its callers (call_graph.csv: exactly 4) | kasfMissionCompleted 0x00491ca5, singlePlayerPostInit 0x005193e5, singlePlayerGameUpdate 0x0051a01a (the function's **only** call; credits and `mrWhiteOut` as the tree), utyVideoPlay 0x005a81ba (between `cat_glcActive` / `glcActivate`): every one reaches animAviPlay in the tree. animBinkPlay's only remaining callers are in unbuilt files (SDL/utility_mac.c, Win32/utility.c) |
| selCircleCompute 0x00506720 | radius: mimic (`test [ebp+0x18],8`) -> `cat_mimicGetSelectRadius` only; bullet +1.0 (0x0072a4c4 = 1.0f); slipgates 0x70 / 0x71 / 0x72 +200 / +300 / +400 (0x0072a440 / a644 / b0c4) then + collspheresize; rounding `x * W * 0.5f + 1.5 * 2^52` (0x0072a4c8 = 0.5f, 0x0072a458 = 0x4338000000000000); the bullet return in the precise branch; the depth test before the rectangle; the radius and depth tests before a precise hit. **Behaviour as ported; three cited addresses wrong (F1)** |
| fowActivateCube 0x0047eeb0 (and the two siblings by the report) | `sub` / `add` of r with signed `jge` / `jl` clips (0x0047eefa ..): the CAT-FIX's two's-complement wrap is the exe's arithmetic |
| partAdvanceTexAnim 0x004df940 | the end test is the 4-byte `*piVar5 == -1`: is_final_tex's low-32-bit test matches |
| HeapSizeSet 0x0055f390, .data 0x008b861c | 0x01800000 (24 MB) default: as the report |
| memAllocFunctionA 0x004adc60 (tail) | the failure tail (0x004add8d: the size-list reset and retry) is HW1's; the port's edit is debug-verbose text only (F2) |

## 3. The diff (`git diff 36289a8`)

Files: texreg.c, Mesh.c, Animatic.c, SinglePlayer.c, utility.c, Select.c, FOW.c, Particle.c, Memory.c, SoundEvent.c,
tools/m2_harness.sh, the delta map / review / deltas doc, the report. All within the round's scope (the render stop,
the animatics, the sp gate, the Mission17 log items of DONE WHEN; SoundEvent.c's exact bank size is the sp gate's heap
pressure, HWI, reported). Every Cataclysm change carries `CAT:` / `CAT-FIX`; the HWI changes carry `HWI`. No CAT-TODO
added or closed: `/bin/grep` gives M3 oracle physics 2, stage 5 23 (8 + 8 + 6 + 1), none naming M2-R9 / M2-R10 or a
render owner, all one-line owner-first. No diagnostic print left (`HWIDIAG|HWI-DIAG|hwiDiag`: none). The generated
files reproduce: `tools/delta_map.py merge` on 36289a8's `delta_map.json` with the review file gave the tree's map
byte-identical ("records changed 15"), and `report` gave `docs/m2-r1-function-deltas.md` byte-identical (before the
audit's fixes; after them the audit regenerated both the same way). No kas2c / kasf / save32 generator input changed.
Findings F2, F3 below.

Checked statements of the report (all right unless listed in section 4): the diagnostic runs (`r10p2-diag-sp1`:
`texreg.c (587)` nPalettes < 8 and the `REGISTER-NEWCOLOR` lines; `r10p2-diag2-sp`: `Memory.c (1714)` pool assertion
at 441 s); the before-counts (producer 1's `r10p1g-m17-a` / `r10a1-m17-a`: 98,718 is_final_tex lines, 21 Select/FOW
lines; m6 / m12 / m14: 4 each); no `.fib` of cataclysm.big (21) or update.big (1) contains `VideoPlay`; the bank size
formula equals SND_BANK_PATCHES_OFFSET + n * sizeof(PATCH) + the old data length; the release build compiles
`dbgAssertOrIgnore` out (`DBG_ASSERT 0`), so before this round it wrote past the block at the same point; every
transition 0->1 .. 17->18 appears in the producer's `r10p2-c2*` logs (union of the `animatic: skipped` lines).

## 4. Findings

- **F1 (wrong claim, fixed):** three exe addresses cited for selCircleCompute's ported tests are wrong (the code is
  right). The depth test before the rectangle is **0x005070b8** (`fld [ebp+0x15c]; fcomp 0.0; test ah,0x41; jne
  0x5072ad`), not 0x0050683f (inside the `call 0x4acd60` at 0x0050683e). The precise hit's tests are **0x00506fa0**
  (radius) / **0x00506fb7** (depth), not "0x00506f0b .." (inside the poly loop's argument pushes). The bullet return
  in the precise branch is **0x005068af** (`cmp [ebp+0x10],1; je 0x5072ad`), not 0x00506a8d (a face-visibility
  `jne`). Fixed in Select.c's three CAT comments (comment-only, binaries byte-identical), in the report (headline,
  3.1, `[audit: ...]`) and in the 0x00506720 review note (map regenerated).
- **F2 (convention, fixed):** memAllocFunctionANV 0x004adae0 and memAllocFunctionA 0x004adc60 are IDENTICAL on the map
  and edited by the port without a review entry. The edit is only inside `#if MEM_VERBOSE_LEVEL >= 1` (retail
  `MEM_VERBOSE_LEVEL 0`), so the class stays IDENTICAL; the audit added two entries saying so (`[audit: ...]`; ANV's
  body not re-compared), merged ("records changed 17", reviewed 3,531, the other counts unchanged) and regenerated the
  deltas doc; the report's section 6 has an `[audit: ...]` note.
- **F3 (change outside scope, fixed):** rewriting `ghidra/delta_map_review.json` re-encoded an unrelated paragraph
  (the ICF_FOLDED note: three `→` as `→`). JSON-equal, but not this port's change: restored.

Observations (no wrong claim): (O1) GoK's `animBinkPlay` stub set `animaticJustPlayed = 8` on every call; the
headless skip returns before it, so a headless run no longer skips 8 `rndFlush` swaps after a transition (render
only; no stream moved). (O2) The skip line is printed before the exe's empty-filename return; every one of the 18
transitions logged in the producer's runs names a file (Animatic01 .. Animatic18), so no transition hits that case. (O3) The "trying the growth heaps" text also prints when a
growth pool itself is full (memAllocFunction then tries the next or a new one): still a note, never a failure in
these runs. (O4) For the M3 soak owner: trRegisterRemoval / trTextureColorsUpdate (exe and tree alike) overwrite +0x14
with their own list after trInternalTexturesDelete stored its "dupe trcolorinfo list" there, which leaks that list:
a likely source of the 2,438 live `dupe trcolorinfo list` blocks the report hands to M3.

## 5. Fixes (5)

1. `src/Game/Select.c`: three CAT comment addresses (F1).
2. `docs/m2-r10-render-animatics-sp.md`: the three F1 citations and the section 6 counts (F2), marked `[audit: ...]`.
3. `ghidra/delta_map_review.json`: the 0x00506720 note's addresses (F1); `ghidra/delta_map.json` and
   `docs/m2-r1-function-deltas.md` regenerated by `tools/delta_map.py merge` (from 36289a8's map) and `report`.
4. `ghidra/delta_map_review.json`: review entries 0x004adae0 / 0x004adc60 (F2), merged and reported as in 3.
5. `ghidra/delta_map_review.json`: the ICF_FOLDED paragraph's `→` restored (F3).

## 6. DONE WHEN, re-checked (plain harness)

| check | run(s) | result |
|---|---|---|
| sp through Mission04 to the timeout | `r10a2-sp` `--timeout 900` | rc 124 at 900 s, **ASan 0, no unsuppressed leak, no fatal**, `trColorsIndexAlloc` 0, Select.c / FOW.c 0; levels Mission01_NIS01, 01, 02, 03, 04_NIS02, **04, 05, 06_NIS04, 06, 07_NIS05, 07, 08, 09** (univUpdateCounter 800 in Mission09 at the timeout); screenshots 94: 01 10, 02 8, 03 14, 04 13, 05 8, 06 13, 07 8, 08 13, 09 7 (= the producer's); `animatics: 9 skipped (headless): Animatic01 .. 09`; growth heap 0 at start-up, 1 in Mission05, 2 in Mission06, 3 in Mission08 (as the report); UBSan kinds only Particle / ETG misaligned members and 2 BTG.c loads |
| the texreg stop | code (section 2) + sp | fixed at the cause (meshFree / trTextureUnregister guards), the exe's trTextureUnregister / trTextureDelete ported, the ninth scheme CAT-FIXed with the exe's and the old release build's behaviour stated (both write past the block) |
| Mission04 pair | `r10a2-m4-{a,b}` | 5/5 PASS, ASan 0, byte-identical **`198d4a9b590735cb`**, IDENTICAL to producer 1's `r10p1g-m4-a`; `Animatic05` |
| Mission06 pair | `r10a2-m6-{a,b}` | 7/7 PASS, ASan 0; a `6fa7405f02c533b4`, b `1d4ec070d0f5ee71`: **rng_all only from 22235** (decision 94's flake; the producer's pair at 4980); b IDENTICAL to `r10p1g-m6-a` in every field, a in every simulation field; `Animatic07`; 0 Select / FOW |
| Mission17 pair | `r10a2-m17-{a,b}` | 4/4 + `game end tick=15584` (credits), ASan 0, byte-identical **`7f2da769dd02065a`**, IDENTICAL to producer 1's audited `r10a1-m17-a`; `Animatic18`; Select / FOW 0; is_final_tex 0 (was 98,718); 1,927 "failed to find block" notes (MAB 876, MassAllocBlobs 487, CatVertexState 239, MAS 128, catMtShips 64, Ship 52, ShipLocalBindings 45, partsys 17, fowLines 8, SBHGrow 3, ...: as the report) |
| Missions 12 / 14 / 15 / 16 | `r10a2-m{12,14,15,16}-a` | all PASS, ASan 0, 0 Select / FOW, 0 is_final_tex; `12ea97cf6febd47f` / `bbcbb55e8abfe8fe` / `54f10604711fa24c` / `a589d57404638a48` (= the producer's); vs `r10p1g-*`: IDENTICAL, Mission14 rng_all only from 10111 (simulation IDENTICAL), as stated; Animatic13 / 15 / 16 / 17 |
| missiontest pair (Mission01, NIS-first) | `r10a2-missiontest-{a,b}` | 6/6, done 11424, ASan 0, byte-identical **`abdb96e858db6f79`**, IDENTICAL to producer 1's audited `r10a1-missiontest-a` (ticks 0-11425); `Animatic01` |
| Mission01 headless stream | `tools/oracle/mission_test.sh --bin <B>/debug/hwi-engine --label r10a2-mission --ticks 6000 --perturb 3000 --copy-tick 4000` | **MISSION TEST PASS**, a = b **`902884367d98025d`**, 1,476,370 B, ASan 0; `hwo_compare` vs producer 1's audited `r10a1f-mission-a` **IDENTICAL** over 0-6000 (and vs R9's `r9i-mission-a` IDENTICAL); release (`--no-perturb-run`, `r10a2-mission-rel-{a,b}`) **`f00d68e03f244551`**, IDENTICAL to `r10a1f-mission-rel-a` |
| motion / nis `/noInterp` | `r10a2-motion-noi`, `r10a2-nis-noi` (`-- /noInterp`) | PASS, dumps **`183681235a3fdc7b` / `13ca70eea998e5de`** = R9's: no move |
| fe | `r10a2-fe` (120 s) | Main_game_screen, rc 124, ASan 0, no leak, 0 fescreen reports, `animatics: 0` |
| other guard rails | `r10a2-r17` (replay 10-17), `r10a2-ai`, `r10a2-skirmish-lfe2`, `r10a2-loadgame`, `r10a2-warp10`, `r10a2-tutorial`, `r10a2-build` | ASan 0 each; `b0cd45c23ec37bf7` (9,601 ticks / 4,800 packets), `b3497a95ecfef7e0`, `1a0025fc8481af43`, `4eb63d48acb54913`, `14268c823f389d61`, `d371fd3484bc6280`, `33d28ad3ba195488`: each **IDENTICAL** to producer 1's run (`r10a1-r17-a`, `r10a1-ai-a`, `r10a1-skirmish-lfe2`, `r10p1g-loadgame-a / warp10-a / tutorial-a / build-a`); the tutorial logs no animatic line (`tutorial == 1`), build logs Animatic01 |
| the user's attended check | report 2.1 | `tools/play.sh` exists (release build by default; logs to `<home>/logs/play-<timestamp>.log`); the release build prints `dbgMessagef` (stdout) and contains the `animatic: play` string; New Game reaches `singlePlayerPostInit(FALSE)` (utility.c 3149) -> Animatic01, Mission01's white-out -> Animatic02; `Movies/Animatic01.bik` .. `18.bik` are in the harness data: the launcher, the transitions and the log lines exist |

No stream moved: every stream the audit reran is IDENTICAL to producer 1's audited stream (or, for Mission06 a and
Mission14, differs in rng_all only, decision 94).

## 7. Precision

Sampled checks 48: the build (1); the bodies and call sites of section 2 (trTextureRegister append, trColorsEqual,
trTextureUnregister, trTextureDelete, meshFree, trRegisterRemoval / ColorsUpdate / DeleteAllUnregistered,
animBinkPlay vs animAviPlay, the four call sites, the single call in singlePlayerGameUpdate, the radius cases and
constants, the rounding, the cited selCircleCompute addresses, the FOW wrap, partAdvanceTexAnim, the heap size; 15);
the report statements of section 3 (diagnostic runs, before-counts, VideoPlay, bank size, release behaviour,
transition union, memAlloc breakdown, growth heaps; 8); the runs of section 6 (sp, m4, m6, m17, m12, m14, m15, m16,
missiontest, mission debug, mission release, motion / nis, fe, replay, ai, skirmish, loadgame, warp, tutorial, build,
the attended check; 21); the generated files, the CAT-TODO grep, the diagnostic grep (3). Wrong: 3 (F1, F2, F3).
**Precision 45 / 48 = 0.9375.**

## 8. References for the next port

Mission01 stream: debug **`902884367d98025d`** (content IDENTICAL to producer 1's `44f6c81a6bf3f58d` and R9's
`553845b65e87f3b5`), release **`f00d68e03f244551`** (IDENTICAL to `96267d55360e6f4e`). missiontest `abdb96e858db6f79`;
Mission04 `198d4a9b590735cb`; Mission06 `1d4ec070d0f5ee71` (its a `6fa7405f02c533b4`, rng_all only); Mission12
`12ea97cf6febd47f`; Mission14 `bbcbb55e8abfe8fe`; Mission15 `54f10604711fa24c`; Mission16 `a589d57404638a48`;
Mission17 `7f2da769dd02065a`; replay 10-17 `b0cd45c23ec37bf7`; ai `b3497a95ecfef7e0`; skirmish LFE2
`1a0025fc8481af43`; loadgame `4eb63d48acb54913`; warp 10 `14268c823f389d61`; Tutorial1 `d371fd3484bc6280`; build
`33d28ad3ba195488`; motion / nis `/noInterp` `183681235a3fdc7b` / `13ca70eea998e5de`. sp: Mission01_NIS01 .. Mission09
in 900 s, 94 screenshots, ASan 0.

Verdict: PASS-WITH-FIXES - the Mission04 stop is fixed at its cause with the exe's trTextureUnregister / trTextureDelete, every caller of 0x00421860 reaches animAviPlay with the headless line at each transition, sp walks to Mission09 with ASan 0 and no Select / FOW / is_final_tex report, no stream moved (all reproduced from a clean build); three wrong exe addresses in selCircleCompute's comments, two missing review entries and one out-of-scope re-encoding fixed in place
