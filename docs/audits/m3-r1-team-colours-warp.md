# Audit: M3-R1 team-colours-warp (producer 2, play-test item 2)

Auditor: adversarial audit of `docs/m3-r1-team-colours-warp.md` and `git diff 94599c8` (no commits after 94599c8 before
this audit; the producer's work is the working tree). `<R>` = `original/scratch-m2/m2r2-harness/runs`. Every engine run
went through `tools/m2_harness.sh` (silent, offscreen). No GPU measurement was taken (correctness runs only).

## Headline

- **The producer's cause is right about the tree and wrong about the exe.** The dump evidence reproduces (the Kuun-Lan's
  palettes are rebuilt during Mission01's trRegistryRefresh without the team colour), but the exe never does that:
  its measurement loop in trRegistryRefresh 0x00587a50 skips every entry that is not pending, so a loaded texture is
  never put in the size-sort list, never shrunk by trCramRAMComputeAndScale and never reloaded. The stale-word push at
  0x00588231 the producer blamed is only reached for a pending entry, whose own measurement wrote the word. The tree
  kept HW1's loop, which also listed the loaded textures.
- **Fix replaced** (audit, in place): the producer's CAT-FIX (`trMeshSortAdd(index, trPending(index) ? listFlags :
  reg->flags)`) is replaced by the exe's behaviour, a **CAT** port: `if (trAllocated(index) && trPending(index))`
  around the measurement, the size-sort list and trMeshSortAdd (src/SDL/texreg.c, comment with 0x00587d1c /
  0x00587d24 / 0x00588246 / 0x0058823e). Loaded textures keep their size and palettes into the next level, as in the
  exe. Every gate of the DONE WHEN was rerun on this tree: PASS.
- No simulation field moved in any stream; no dump moved.

## 1. Clean builds (`original/scratch-m2/build-audit-m3r1-team-colours-warp/`, meson exactly as tools/build.sh)

| tree | debug (ASan/UBSan, -O0) | release (-O2) | compiler warnings (-Werror, warning_level 3) |
|---|---|---|---|
| producer's (as handed over) | `debug/` 88,909,024 B | `release/` 5,638,816 B | 0 / 0 |
| audit-fixed (the gate) | `fixed-debug/` | `fixed-release/` | 0 / 0 |
| producer's with the CAT-FIX reverted (scratch worktree `original/scratch-m2/audit-tcw/nofix`) | `audit-tcw/build-nofix-debug/` | - | 0 |

After the audit's edits `tools/build.sh --debug` and `--release` are green (the only "WARNING" line is meson's
sanitizer notice at meson.build:57), `tools/m2_layout32.sh` OK. The last source edit after the audit binaries were
built is a comment/hook reorder in MissionSelfTest.c (finding F4: no code change in behaviour).

## 2. Bodies compared with the exe (cataclysm_gog_r6; tools/m2_fdec.py / m2_fdis.py)

| # | function | class | result |
|---|---|---|---|
| 1 | trRegistryRefresh 0x00587a50 (changed) | MODIFIED | **differs**: exe tests flags & 4 at 0x00587d1c and flags & 1 at 0x00587d24, both `je 0x00588246` (after the trMeshSortAdd call at 0x0058823e); the decompile reads `if ((flags & 4) && (flags & 1)) { measure; size list; trMeshSortAdd(index, flags); }`. Tree (HW1) listed loaded entries too. Ported (finding F1) |
| 2 | trCramRAMComputeAndScale 0x00589a50 (relied on) | MODIFIED | as the tree (shift reset + sum over the size list, Mission17 30 % / 1 MB, the halving passes with bScalable and > 8, the last pass: loaded -> trPreLoadedTextureScale, pending -> scaled size). In the exe the list holds only pending entries, so its loaded branch is dead |
| 3 | trPreLoadedTextureScale 0x00586bd0 (relied on) | IDENTICAL | only caller trCramRAMComputeAndScale (call_graph.csv); not reached in a level change in the exe (nor in the tree after F1) |
| 4 | trMeshSortAdd 0x00586190 (relied on) | IDENTICAL | as the tree (meshReference + CRC of n*8 colour bytes, the memcmp, the 16-slot growth, teamFlags \|= flags & 0x30, 10 new lists cleared) |
| 5 | trMeshSortListLoad 0x00587210 (changed: HWI-TEST record only) | MODIFIED | producer's new row checked: palette coloured iff sortList teamFlags & 0x30; the HWI-TEST lines change no state |
| 6 | trTextureRegister 0x00585760 | MODIFIED | as the tree: CRC + name match, same colour -> usage++; new colour -> the inlined slot allocation (unused 0/0 slot, else append at 0x005858ef: R10's CAT-FIX bound in trColorsIndexAlloc); new entry: pending, 0x40 colour block, bScalable at +0x30 |
| 7 | trTextureDelete 0x005854c0 | MODIFIED | as the tree (R10): the exe inlines trAllSharedFromDelete's loop; the +0x14 memFree is ported |
| 8 | trTextureUnregister 0x005860d0 | MODIFIED | as the tree (R10): usage--, colour slot cleared only when pending and usage 0 |
| 9 | trRegisterAddition 0x00585b60 | IDENTICAL | as the tree: pending/shared -> found slot or alloc; loaded -> match, else 0x40 copy, trInternalTexturesDelete, pending, alloc (callees: memAllocFunction, trInternalTexturesDelete) |
| 10 | trRegisterRemoval 0x00585e70 | IDENTICAL | as the tree; the decompile shows a second usage-- after the loaded branch, but the disassembly returns at 0x00585f7f after it (a decompiler artefact): one decrement |
| 11 | trTextureColorsUpdate 0x00585fa0 | IDENTICAL | as the tree (copy, delete, new colour, pending: the O4 64-byte loss per reload confirmed) |
| 12 | colour-index allocation (trColorsIndexAlloc, inlined in 0x00585760 / 0x00585b60) | - | as the tree but R10's ninth-scheme CAT-FIX |
| 13 | singlePlayerNextLevel | - | no exe function: the exe's only caller of singlePlayerMissionCompleteCB is kasfMissionCompleted 0x00491c70 (compared: tutorial, clCommandMessage, speechEventCleanup, the CB, animBinkPlay(n, n+1): as the tree's KASFunc.c); the tree's singlePlayerNextLevel is GoK's /debugScreenshots path |
| 14 | meshFree 0x004af710 | IDENTICAL | as the tree (every handle but -1 of 8 per registered material unregistered, list freed; R10's HW_PTR_64 guard) |
| 15 | singlePlayerLoadNewLevel 0x0051ac80 (changed: three HWI-TEST hooks) | MODIFIED | hooks are read-only (catTeamColourLevelChange returns unless /catTeamColourTest) |

Also read: trColorInfosPointer 0x00585650 (IDENTICAL, as the tree). Review rows: the producer's rows are present for
every edited exe function; the audit appended `[audit: M3-R1 team-colours-warp]` notes to 0x00587a50 (its "change" now
also says the pass lists only pending entries), 0x00589a50 and 0x00586bd0; `tools/delta_map.py merge --done
m3r1:team-colours-warp` (records changed 3; IDENTICAL 2,762 / MODIFIED 1,575 / REWRITTEN 83, unchanged) and `report`
rerun. The producer's generated files reproduce: in the scratch worktree, the 94599c8 delta_map.json + the producer's
review file + merge + report gave byte-identical ghidra/delta_map.json and docs/m2-r1-function-deltas.md.

## 3. The cause, rerun (registry dump before / after the switch)

Runs `m3r1-aud-tcw-p-st-user` / `-p-st-red` (producer's tree), `m3r1-aud-tcw-nofix-st-user` (CAT-FIX reverted),
`m3r1-aud-tcw-st-user` / `-st-red` (audit tree); missiontest Mission01 NIS-first, `--teamcolour --player-colours`.
Kuun-Lan texture `RACE_SECT/SMOTHERSHIP/RL0/LOD0/SMO_MOTHERSHIP_RIBS` idx 141, scheme 0:

| run | "before" (NIS01 end) | "statics" (Mission01) | "after" Mission01's refresh |
|---|---|---|---|
| no fix, user | 32x16, palcrc 533174dd, built 0xb0 | same block, 533174dd | **new block, 32x16 -> 16x16, palcrc bf3432db, built 0x80** (no team bit) |
| producer, user / red | 533174dd / 9d25decd | same | new block, 16x16, 533174dd / 9d25decd, built 0xb0 |
| audit, user / red | 533174dd / 9d25decd | same | **same block, 32x16**, 533174dd / 9d25decd (not reloaded, as in the exe) |

Registry entry, colour info ff1bca82/ffff5539 at palette 0, nPalettes 2, use 6, scheme 0: unchanged in all, as the
producer stated. The shrink + reload is what the exe never does to a loaded texture.

## 4. Self-test not vacuous

| run | engine | checks | result |
|---|---|---|---|
| m3r1-aud-tcw-nofix-st-user | producer's tree, CAT-FIX reverted, self-test kept | 6, **4 FAIL**, 960 fail lines (240 textures: mothership=0 worker=0 recon=0) | FAIL |
| m3r1-aud-tcw-ppm-nofix | same, 2,600 ticks into Mission02 | 7, 5 FAIL, 1,200 fail lines | FAIL |
| m3r1-aud-tcw-p-st-user / -p-st-red | producer's tree | 6, 0 FAIL | PASS / PASS |
| m3r1-aud-tcw-st-user / -st-red | audit tree, user ff1bca82/ffff5539, contrasting ff2020e0/ff20e020 | 6, 0 FAIL (textures 513 / 594, mothership 10, acolyte 53, worker 38, recon 51, Kuun-Lan scheme0) | PASS / PASS |
| m3r1-aud-tcw-st-rel | audit release build, user's colours | 6, 0 FAIL | PASS |
| m3r1-aud-tcw-m3-tc | Mission03 -> Mission04_NIS02 -> Mission04 (the level after an NIS level) | 6, 0 FAIL | PASS |
| m3r1-aud-tcw-loadgame-tc | "01 - Hiigara" | 2, 0 FAIL | PASS |
| m3r1-aud-tcw-sp-tc | sp 1,200 s, user's colours | **34 checks, 0 FAIL**, every level Mission01_NIS01 .. Mission12_NIS07 "after" PASS (248 .. 1,005 textures) | PASS |

## 5. PPM fence (own reader: `scratchpad/region.py`, mean RGB of a box, P6 top row first)

Runs `m3r1-aud-tcw-ppm-user` / `-red` / `-nofix` (`--shot 900,1500,2016,2208,2400,0-2600/100 -- /catTeamColourTicks
2600`, 76 frames each). The producer's regions:

| frame, region | user (base R130 G202 B27) | red (R224 G32 B32) | no fix, user | producer's tree, user |
|---|---|---|---|---|
| NIS01 t900 444,450,468,474 | 53.0, 71.2, 35.2 | 72.2, 37.0, 32.1 | 53.0, 71.2, 35.2 | 53.0, 71.2, 35.2 |
| Mission01 t2016 342,314,366,338 | 93.0, 118.2, 32.2 | 133.4, 48.9, 33.5 | 73.4, 70.1, 47.7 | 95.6, 122.6, 32.0 |
| Mission02 t1000 518,418,542,442 | 113.8, 117.7, 57.5 | 149.5, 63.8, 59.2 | 119.1, 99.1, 92.0 | 116.3, 120.5, 58.2 |

The region follows the base colour (G > R > B with the user's, R with red; grey-brown without a fix). Pixels differing
user vs red: Mission01 t1800 55,150, t2016 60,281; Mission02 t100 58,403, t1000 60,868. Audit tree vs producer's tree
(user's colours) differ in ~100-115 k pixels from Mission01 t1800 on (0 before the arrival): the Kuun-Lan's textures
are no longer halved at the level change (higher resolution, as in the exe). Producer's no-fix figures reproduce
exactly (73.4, 70.1, 47.7).

## 6. Streams and dumps (audit tree, debug ASan unless stated; `hwo_compare.py`, all 21 fields)

| stream | run (sha256/16, this build) | against | result |
|---|---|---|---|
| headless Mission01, 6,000 ticks | m3r1-aud-tcw-mission-a / -b 565e2e43c4117191 (1,476,370 B; pair `cmp` identical) | r10i-mission-a 5bd51234405ea0d8 | **IDENTICAL**, 6,001 ticks, every field incl. rng_all |
| headless Mission01, release | m3r1-aud-tcw-mission-rel 6e822afc839caaec | r10i-mission-a | IDENTICAL, 6,001 ticks |
| missiontest Mission01 NIS-first | -missiontest-a / -b a87356c48cff3eb9 | r10i-missiontest-a, m3r1-tcw-missiontest-a | IDENTICAL, 11,426 ticks; 6/6 PASS, done 11,424 |
| Mission02 | -m2 6bf979beca7efc0f | r10i-m2-a, m3r1-tcw-m2 | IDENTICAL 25,905 ticks; 6/6, done 25,936 |
| Mission03 | -m3 8cabb1fbcd741600 | r10i-m3-a, m3r1-tcw-m3 | IDENTICAL 17,794; 5/5, done 17,825 |
| Mission04 | -m4 759f6deec57079dc | r10i-m4-a, m3r1-tcw-m4 | IDENTICAL 20,787; 5/5, done 20,818 |
| Mission09 | -m9 f6cf9c5f507ec54d | r10i-m9-a, m3r1-tcw-m9 | IDENTICAL 20,586; 6/6, done 20,617 |
| loadgame "01 - Hiigara" | -loadgame-a / -b 028bf19a208c9904 (pair identical) | r10i-loadgame-a | IDENTICAL ticks 1984-5586; PASS |
| team-colour runs | -m3-tc, -loadgame-tc | -m3 (`--to-tick 17793`), -loadgame-a | IDENTICAL |

Dumps (whole / ` crc=` stripped), equal to the producer's and to producer 1's audited (m3r1-aud-lbf) dumps:
motion /noInterp 183681235a3fdc7b / 3016280e9e0dd3c8; motion on 428fff6c765ab81b / 428ef41479eb5749; nis /noInterp
2a7fec4de8cfe716 / 196bab32d244f614; nis on 46df1d391a86a404 / 7ce8472514254f4f.

Not a stream: FileLoads.log counts fall where the reloads are gone (missiontest 4,345 -> 3,872; the team-colour run
5,218 -> 4,745); the mission mode (one level) 3,781 unchanged.

Plain gates: `m3r1-aud-tcw-fe` (fe, 120 s): Main_game_screen, ASan 0, no unsuppressed leak. `m3r1-aud-tcw-sp` (sp,
1,200 s): universe running, still running at the timeout (as R10), ASan 0, no unsuppressed leak, 12 animatics skipped,
last screenshot golden-12-00240 (the producer's golden-12-00128: the chain runs faster without the reloads).
Live "dupe trcolorinfo list" blocks (sp-tc heap lines): **Mission09 1,587** (producer's tree: 2,437), Mission12_NIS07
1,898. Every summary in this audit: ASan errors 0.

## 7. Findings

- **F1 (FAIL-grade, fixed)**: the cause attributed to the exe and the CAT-FIX built on it. Evidence §2 row 1 and §3.
  The producer's review row for 0x00587a50 ("trMeshSortAdd(index, flags) for every allocated entry") and the report's
  "exe and tree alike" / "An exe defect" are wrong. Fixed: the exe's pending gate ported as CAT (texreg.c), the CAT-FIX
  removed, the comment above `//only pending textures get here` updated; review rows 0x00587a50 / 0x00589a50 /
  0x00586bd0 annotated `[audit: ...]`; delta map merged and report regenerated; the producer's report corrected with
  `[audit: ...]` notes (headline, (a), (b), (e) heap, (f) tags, files, For STATE.md, What is left, the user's check).
- **F2 (doc)**: the heap baseline for the soak (2,437 at Mission09) belongs to the producer's tree; with F1 it is 1,587
  (Mission12_NIS07 1,898). Fixed in the report.
- **F3 (doc)**: THE USER'S CHECK runs `tools/play.sh`, which only builds build/release when it is missing; the user must
  run `tools/play.sh --rebuild` once after the integrate. Fixed in the report. The rest of the check exists: New Game,
  Mission01 fleet arrival at tick 1,939 (about two minutes), Mission02, Mission03 -> Mission04_NIS02 -> Mission04; the
  optional `-- /catTeamColourTest` switch exists in main.c; the user's Cataclysm.cfg holds PlayerBaseColor -14955902,
  PlayerStripeColor -43719, ColorsPicked 1.
- **F4 (nit, fixed)**: in catMtMissionEnd the hook `catTeamColourDone()` was inserted between the "// ticks: ..."
  comment and the dbgMessagef it describes; moved above the comment.
- Scope / tags: every src change is in the producer's scope; every test line carries HWI-TEST; no recreation feature
  (no HWI-FEATURE needed: no setting); CAT-TODO lines 35 as at 94599c8 (23 stage-5), none names M3-R1; no
  CAT-PROVISIONAL names M3-R1. tools/m2_harness.sh `--teamcolour` / `--player-colours` documented in the header.
- Minor, not fixed: the user's play log holds two New Games (a "Quit to main menu!" between); neither reached a
  Mission02 static, so the producer's reading stands.
- Leftovers (owners unchanged): O4 colour-info leak (M3 soak); GoK's utyShipsAlwaysUseOwnerColors condition (not the
  exe's, harmless while FALSE).

## 8. Precision

Sampled checks of the producer's claims: 22. Correct 20 (the dump evidence; the registry and scheme right before and
after; Mission01's cram shrinking idx 141; the user's log never reaching Mission02; self-test PASS with the user's
scheme; FAIL without the fix (240 textures); the PPM figures (NIS01 and no-fix values exact); mission pair; missiontest
pair; Mission02 / 03 / 04 / 09; loadgame pair; four dumps; fe ASan 0; sp ASan 0; trMeshSortAdd IDENTICAL; the O4
mechanism in trTextureColorsUpdate / trRegisterRemoval; the generated delta-map files reproduce; the CAT-TODO counts; a
user's check with existing launcher / moment / switch). Wrong 2 (the exe passes a stale word for loaded entries; the
exe lists every allocated entry in the measurement pass / reloads loaded textures at a level change).
Precision = 20 / 22 = 0.91.

Fixes in place: 6 (the CAT port replacing the CAT-FIX in texreg.c; the MissionSelfTest.c hook placement; three review
rows; the producer's report corrections), plus the regenerated delta map and report.

## Reference for the next port (audit tree)

Mission01 6,000-tick stream IDENTICAL to r10i-mission-a (this build 565e2e43c4117191); missiontest a87356c48cff3eb9;
Mission02 6bf979beca7efc0f, 03 8cabb1fbcd741600, 04 759f6deec57079dc, 09 f6cf9c5f507ec54d; loadgame 028bf19a208c9904;
release mission 6e822afc839caaec; dumps as §6. Runs under `<R>/m3r1-aud-tcw-*`.

Verdict: PASS-WITH-FIXES - the colours now survive every level change (34/34 checks, PPM follows the base colour, no stream moved), but by the audit's port of the exe's pending-only gate in trRegistryRefresh: the producer's cause and CAT-FIX blamed the exe for a loop the exe does not have.
