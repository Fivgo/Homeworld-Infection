# Audit: M2-R8 oracle-rung-physics

Auditor: adversarial audit of the M2-R8 producer 1 port (report `docs/m2-r8-oracle-rung-physics.md`), base `145352a`.
The port is the working tree on top of `d190685` (the two commits after the base, `bc96f39` and `d190685`, are the
orchestrator's: the M2-R8 workflow script and STATE.md's run id; not port work). Every engine run below went through
`tools/m2_harness.sh` (plain harness, silent, offscreen; no GOG game, no DLL, no input), with the auditor's own clean
build passed with `--bin`.

## 1. Clean builds

`rm -rf original/scratch-m2/build-audit-r8-oracle-rung-physics`, then `meson setup` with `tools/build.sh`'s arguments
(debug: `--buildtype=debug -Db_sanitize=address,undefined`; release: `-Doptimization=2 -Ddebug=false -Db_sanitize=none`)
and `meson compile`:

| config | result | warnings | size | sha256 |
|---|---|---|---|---|
| debug | `[319/319] Linking target hwi-engine`, rc 0 | 0 (`-Werror` in build.ninja) | 87,582,576 B | `94f0d2acd41ec6e2` |
| release | `[319/319] Linking target hwi-engine`, rc 0 | 0 (`-O2 -fno-strict-aliasing` on all 276 compile lines) | 5,433,928 B | `e2c75840d55c9aaa` |

The release size equals the producer's; the debug size differs from the producer's 87,567,848 B only through the
longer build directory path in the debug info. After the audit's comment edit (section 5) both configurations were
recompiled: rc 0, 0 warnings.

## 2. Runs (auditor's clean build, plain harness)

| run | mode | result | stream sha256 (producer's) |
|---|---|---|---|
| `r8au-r17-on-{a,b}` | replay 10-17, `-- /oracleTableFsqrt` | oracle replay done, `ticks=9601 packets=4800 ships=80 randSyncErr=3406 univSyncErr=4611 blobSyncErr=4746 pktSyncErr=0`; ASan 0, no leak; **a = b** | `90ec0345fef510e5` (same) |
| `r8au-r17-off-{a,b}` | replay 10-17, default sqrtf | done, `randSyncErr=3587 univSyncErr=4693`; ASan 0; a = b | `8be77c5d7d8032de` (same) |
| `r8au-r18-on-{a,b}` | replay 10-18, switch on | done, `ticks=3527 packets=1763 ships=57 randSyncErr=888 univSyncErr=1621 blobSyncErr=1619`; ASan 0; a = b | `0ad45d1587d6caf9` (same) |
| `r8au-r18-off-{a,b}` | replay 10-18, switch off | done, `randSyncErr=984 univSyncErr=1664`; ASan 0; a = b | `5cef18fd015db1c8` (same) |
| `r8au-r17-on-dump` | replay 10-17, switch on, `/oracleShipDump all@240-250` | done, ASan 0; `shipdump.txt` 730,906 B; run.hwo **byte-identical** to `r8au-r17-on-a` (the dump is read only) | `90ec0345fef510e5` |
| `r8au-mission-{a,b,perturb}` (`tools/oracle/mission_test.sh --bin <audit debug> --label r8au-mission --ticks 6000 --perturb 3000 --copy-tick 4000`) | mission, 6,000, debug | **MISSION TEST PASS** (a = b, ASan 0 both, perturbed run caught at 3000 / rng10, flipped copy at 4000 / ships_health) | **`245b3d88387ed076`** (same) |
| `r8au-mission-rel-{a,b}` (`--bin <audit release> --perturb 0`) | mission, 6,000, release | MISSION TEST PASS; IDENTICAL to the debug stream over 6,001 ticks (header build flags only) | `e0c18fcf00229e0f` (same) |
| `r8au-mission-table` | mission, 6,000, `-- /oracleTableFsqrt` | done, ASan 0; byte-identical to the default run | `245b3d88387ed076` |
| `r8au-ai-a` | ai, 400 | `checks=16/16 result=PASS`, ASan 0 | `dcab88df1f2d0dab` (same) |
| `r8au-ai-soak` | ai, `--ai-ticks 4000` | ai self-test done, rc 0, **ASan 0, no leak, no fatal**; `game over tick=1101 player0=dead player1=alive`, `done tick=3998 ... checks=16/16 result=PASS` | `fd1480fef8611673` (same) |
| `r8au-skirmish-lfe2` | skirmish LiveFireExercise2, 600 | `done ticks=600 ships=89 result=PASS`, ASan 0 | `955744cb5e558abb` (same) |
| `r8au-skirmish-armed8` | skirmish Armed8, 600 | `done ticks=600 ships=54 result=PASS`, ASan 0 | `03376d1dd4392208` (same) |
| `r8au-fe` | fe, 120 s | Main_game_screen, still running at the timeout; ASan 0, no leak, 0 fescreen reports | - |
| `r8au-sp` | sp, `--timeout 340` | universe running, last `univUpdateCounter` **3,264** (the producer's 3,264); ASan 0, no leak, 0 fescreen reports | - |
| `r8au-motion-noi`, `r8au-nis-noi` | motion / nis, `-- /noInterp` | motion self-test done, ASan 0 | dumps `0838a011db819920` / `d1e6f6e477906b52` (same) |

`tools/m2_layout32.sh`: OK (Ship 1596, Player 25904, Universe 252348); `python3 -I tools/m2_save32_gen.py --check`:
current.

## 3. GOG comparisons (restated command lines)

```
python3 -I tools/oracle/hwo_compare.py original/oracle/oracle01-rec-20261017-140428/oracle01-rec.hwo <runs>/r8au-r17-{on,off}-a/run.hwo \
    --from-tick 1 --to-tick 9600 \
    --fields rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real \
    --json <runs>/r8au-r17-{on,off}-a/compare.json        # -> 161 rng6 (excluded, decision 47)
python3 -I tools/oracle/hwo_compare.py original/oracle/oracle01-rec-20261018-102253/oracle01-rec.hwo <runs>/r8au-r18-{on,off}-a/run.hwo \
    --from-tick 1 --to-tick 3526 --fields <the same list> --json <runs>/r8au-r18-{on,off}-a/compare.json   # -> 2 real (excluded, decision 60)
... and again without the excluded fields (rng6; for 10-18 also real) -> compare-sim.json; and each field alone (--fields <one>)
```

| replay | first divergence (simulation fields) | per field (first tick) | first divergent packet |
|---|---|---|---|
| 10-17 on | **247 `ships_vel`** (last equal 246; REF `83380d15912e50a0`, CAND `f959f314d5ba4d97`) | ships_vel 247, ships_pos 259, univcheck 315, ships_misc 1593, resources 1944, bullets 2005, rng10 2043, ships_health 2043, ships_n 5559; missiles, derelicts, dt, total, real none | 46 (tick 93, blobcheck +1, stock) |
| 10-17 off | 177 `ships_vel` | ships_vel 177, ships_pos 180, univcheck 190, rng10 1408, resources 1408, ships_misc 1593, bullets 1917, ships_health 2043, ships_n 5559 | 46 |
| 10-18 on | **155 `ships_vel`** (REF `be9dd23a2cdceb2a`, CAND `cfa0591c536ab701`) | ships_vel 155, ships_pos 179, univcheck 216, bullets 1721, rng10 1735, ships_health 1741, resources 1752, ships_misc 1753, ships_n 1753; real 2 (excluded) | 108 (tick 217, univcheck) |
| 10-18 off | 118 `ships_pos` + `ships_vel` | ships_pos 118, ships_vel 118, univcheck 137, rng10 944, resources 944, bullets 1721, ships_health 1741, ships_misc 1753, ships_n 1753 | 68 (tick 137) |

Every number of the report's section 3 table holds. The localisation of 3.4 reproduces exactly:
`python3 -I tools/oracle/hwo_invert.py <10-17 GOG .hwo> <runs>/r8au-r17-on-dump/shipdump.txt --tick 247 --ship 40 --field vel`
-> one solution, ship 40 REF `420767c8 4300eec6 429fe606`, ours `420767ca 4300eec5 429fe608`, REF - ours `-2 +1 -2`
(rc 0); the dump's `S 247 40 bRecon 1 ...` line agrees.

**Mission01 stream**: `hwo_compare.py runs/r7i2-mission-a/run.hwo runs/r8au-mission-a/run.hwo` (baseline sha256
`9f7416bb64fec98f`, present) -> FIRST DIVERGENT TICK 1, `ships_rot` only; with `--fields rng10,rng6,rng0,rng_all,ships_n,
ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,statcheck,univcheck,players,dt,total,real`
-> IDENTICAL over 6,001 ticks (header build identity `cd5866de` vs `d190685e` only). The producer's before / after
statement holds; the stream moved once. sqrtf is still the default (`FastMath.c` `catFsqrtTable = FALSE`; `catFsqrtD`
takes `sqrtf` unless `/oracleTableFsqrt`), and the switch does not move the headless Mission01 stream.

## 4. Bodies compared with the exe (`tools/m2_fdis.py`, `tools/m2_exedata.py`; export `cataclysm_gog_r6`)

Operand widths and roundings judged on the x87 stack (53-bit precision control; `fstp dword` = the one float rounding):

- `vecNormalize` 0x00550770: `fld z; fld y; fld x`, `(x*x + y*y) + z*z` -> `fstp qword` -> fsqrt; `fcom` with 0.0f
  (0x0072a460 = 0), `fdivr` 1.0f (0x0072a4c4 = 0x3f800000), each component `fmul dword; fstp dword`. Tree matches.
- `vecCapVector` 0x00550950: same magnitude; `fcom [max]`, `test ah,0x41` (cap only above), `fdivr [max]`, one
  rounding per component. Matches.
- `aitrackHeadingFunc` 0x00418de0: dot `(cz*dz + cy*dy) + cx*dx` kept on the FPU, `<= -1.0f` and `>= accuracy` tests;
  `oneovertime` stored as a float; each desired speed a double (`-(y*ts)`, `x*ts`, `(up.y - yawcloseness)*ts`);
  torque `((desired - w)*I)*oneovertime` compared with the strength (`test ah,0x41`: above -> the strength, else the
  rounded torque); directions 0..5 pushed as in `catAitrackTurn`; yawcloseness stored float. Matches.
- `physUpdateObjPosVelShip` 0x004e4950: `a.x = (m*Fx)*t` on the FPU, a.y / a.z stored after each product; drift cap
  `(1-f)*(1400-sm)+sm` double; `pos.x = t*v.x + pos.x` one rounding, d.y / d.z stored; `w.x = (Ix*Tx)*t + w.x` one
  rounding; angles `t*w` doubles into `fsin` / `fcos`, `fstp dword`. Matches (libm sin / cos for the x87's fsin /
  fcos is the stated default).
- `matMultiplyMatByMat` 0x004ad1a0 (stack traced through all fxch): `P0 = (A2*B20 + A0*B00) + A1*B10`,
  `P1 = (A2*B21 + A1*B11) + A0*B01`, `P2 = (A1*B12 + A2*B22) + A0*B02`. Matches.
- `matMultiplyMatByVec` 0x004ad260 / `matMultiplyVecByMat` 0x004ad2f0: `x = (p0+p1)+p2`, `y,z = p0+(p1+p2)`, all
  loads before the stores. Matches.
- `matCreateCoordSysFromHeading` 0x004ad010, `GetCollSizeInDirection` 0x00413f90 (components `(z*m3k + y*m2k) +
  x*m1k`, abs, `(h*F + r*R) + u*U`, * 0.5; F/R/U at staticinfo +0x80/+0x7C/+0x78),
  `aitrackVelocityVectorGuidance` 0x00419460 (`(ez*ez + ey*ey) + ex*ex`, `mass/t` stored float, thrust double vs
  strength, no thrust when equal). Match.
- `hyperspaceCost` 0x00443c30: distance, costs and sum doubles across the ships, `(dist*slope)+min`, max test
  `test ah,0x41`. Matches **except the return** (finding F2).
- `fsqrt` 0x00472860 (zero test against the double 0.0, low dword kept, high dword from the table and the halved
  exponent, logical shift as `uint32_t`) and `fmathInit` 0x004728c0 (entries `sqrt` of `(i<<7)|0x3ff00000` and
  `(i<<7)|0x40000000`): `catFmathSqrtDouble` / `catFmathInit` match.
- `collCheckShipShipColl` 0x0043ade0: clamped-host test 0x0043aec4..0x0043aef3 (SOF_Clamped 0x01000000, clamp info
  +0x1BC, host at its offset 0, both ways, before the Sentinel call 0x0043acc0 and the sphere test);
  0x0043b1ba..: `(z*z + y*y) + x*x`, `fst` as the float distsquared, `fcompp` with `colldist*colldist` before the root,
  bump test, `< 1.0f` special case, NotBumpable tests (0x80000), then fsqrt of the stored float. Matches (only an
  unordered compare differs, finding F4).
- `etgEffectCreate` 0x00470990: `sub esp,0x28`, the `nParams > 0` copy loop into `[esp+0x10]` with no bound
  (0x00470b16..0x00470b2c), its address passed to `etgEffectCodeStart` 0x0046a5b0, which writes `nParams` dwords into
  `effect->variable` (0x0046a684..) unbounded; `univDeleteDeadShip` pushes `z + forwardlength` (+0x68 + +0x80) as the
  tenth parameter at 0x0054811c. The port and the `CAT-FIX` bound are as described.
- `univRemoveAllTargetReferencesFromBullets` 0x005474d0: `cmp [eax+0x11c],edx` (owner pointer), beam `+10000.0f`,
  target cleared. Matches.
- `univUpdateAllPosVelResources` 0x005432d0: after `physUpdateObjPosVelBasic`, attribute bit 0x200, per axis: `-e >
  pos` -> `+e`, else `pos > e` -> `-e` (`test ah,0x41`). Matches; the tree's zero-box / asteroid-only restrictions
  are marked `CAT-FIX`.
- `cat_aisTeamAttack` tables: `.data` 0x00752578..0x007525e4 = `5 25 9 32 7 3 18 20 | 25 3 7 9 18 20 | 18 9 7 20 |
  9 32 18 20 | 5 9 32 18 20 | 1` (the last dword `aisFirstPlay`). Matches `aisTargetTables` / `catAisTargetTeam`.
- Eight the producer kept IDENTICAL: `vecCapVectorSloppy` 0x005508d0, `getVectDistSloppy` 0x00550a90,
  `matMakeRotAboutX` 0x004ad430, `matMakeRotAboutY` 0x004ad470, `matMakeRotAboutZ` 0x004ad3f0 (element placement
  checked against the column-major layout), `physApplyRotToObj` 0x004e4520 (SOF_Crazy 0x200000 on ships, odd
  direction negated, jump table 0..5 -> x,x,y,y,z,z), `aishipGetTrajectory` 0x00413ee0, `RangeToTargetGivenDist`
  0x00439c80. All as the tree.

## 5. Diff review (`git diff 145352a`)

- Scope: the 22 src / tool / map files of the producer's list plus the orchestrator's `cycle/STATE.md` and
  `cycle/workflows/m2-r8.js` (commits `bc96f39`, `d190685`: not port work). No change outside the producer's scope.
- Every edited function with a delta-map row has a `[review: M2-R8 oracle-rung-physics]` entry (script over the diff's
  changed lines: 40 mapped functions; the only one without an R8 note is `clProcessSyncPacket`, already MODIFIED, which
  gets the HWI-ORACLE dump hook only); every one the base map called IDENTICAL has its entry; new helpers
  (`catVecMagSqD`, `catAitrackTurn`, ...) are cat-new.
- Every hunk carries `CAT` / `CAT-FIX` / `HWI-ORACLE` / `HWI-TEST` or sits in a function whose port comment covers it.
- CAT-TODOs: one re-worded (`CAT-TODO(stage 6 level load / KAS)`, owner first, on the marker line); none added by the
  producer.
- Generated files reproduce: `git show 145352a:ghidra/delta_map.json` + `python3 -I tools/delta_map.py merge --out
  <copy> --done m2r8:oracle-rung-physics` ("records changed 50") is byte-identical to `ghidra/delta_map.json`, and
  `report --out <copy> --md <copy>` to `docs/m2-r1-function-deltas.md`. Counts 2,853 -> 2,828 IDENTICAL, 1,483 ->
  1,508 MODIFIED, reviewed 3,036 -> 3,038 as stated; the 25 names listed.
- `/oracleShipDump` is documented in `tools/m2_harness.sh`'s header and main.c's switch table.

## Findings

- **F1 (doc, wrong, fixed)**: the headline (line 24) and section 3.6 said that with the switch off "every later field
  moved out" against R7. Not so: `rng10` stays at 1408 and `bullets` at 1917 (10-17), R7's ticks
  (`docs/m2-r7-oracle-rung-aiship.md` line 330: "rng10 1408 ..., bullets 1917"); this audit's per-field runs give the
  same 1408 / 1917. Fixed in place: "most later fields" with an `[audit: ...]` note in both places.
- **F2 (code, incomplete port, annotated)**: `hyperspaceCost` 0x00443c30 leaves its double sum on the FPU (`ret` after
  `fstp st(0)` of the distance) and its callers convert the unrounded double: `_ftol` 0x00670ad4 at 0x00407ea9
  (`aimProcessHyperspace`), 0x0044750a (`clMpHyperspace`), 0x00564c1c, or `fadd` with it as a double at 0x00433170
  (`cat_aisTeamAttack`). The tree's `real32 hyperspaceCost` rounds it to float first, so a sum just below an integer
  can convert one RU higher; its comment ("one rounding at the return") reads as the exe's shape. Not changed (it
  touches the prototype and five callers and would move the ai / skirmish streams); a one-line
  `CAT-TODO(M2-R9 physics)` was added at the function's comment (CommandLayer.c 2717) and the report's table row and
  "What is left" (item 5) carry an `[audit: ...]` note.
- **F3 (code quality, minor, not changed)**: dead locals kept alive by casts: `(void)oneOverW` (vecHomogenize),
  `(void)k` (vecLineIntersectWith{XY,YZ,XZ}Plane), `(void)dist` (univFindNearestResource), `(void)temp`
  (GenericInterceptorPassiveAttack); `rotstr` stays declared in aitrackHeadingFunc. Harmless; the next editor of those
  functions can drop them.
- **F4 (informational)**: in `collCheckShipShipColl` the exe's `fcompp; test ah,0x1; je nocollision` takes an
  unordered (NaN) squared distance down the collision path; the tree's `!(d < c*c)` skips it. Only NaN positions
  differ; not changed.
- **F5 (informational)**: `univRemoveAllTargetReferencesFromBullets` was moved IDENTICAL -> MODIFIED, but the exe body
  is HW1's semantics (the owner's targetable part is at offset 0); the tree's change removes a C-level null member
  access, i.e. a tree-side fix. `catAisTargetTeam` is marked `CAT-FIX` although it reproduces the exe's read rather
  than deviating from it. Labels only; behaviour as the exe.

## Precision

66 sampled checks (2 builds; 4 replay pairs and their counter lines; 4 first divergences, 4 per-field rows, the first
packets; the ship-40 inversion; the dump's read-only claim; the harness header; the switch-off headline; 6 mission
claims (debug pair, release pair, debug = release, ships_rot only, other fields IDENTICAL, table = default) and the
sqrtf default; ai 400 and soak; 2 skirmish maps; fe; sp; motion and nis dumps; layout32; save32 gen; delta map json,
deltas md, counts and the 25 names, review rows; 26 bodies in section 4 counted one each (the eight IDENTICALs
separately, the two matrix-vector products as one, fsqrt and fmathInit separately); CAT comments; CAT-TODO format).
Wrong: 2 (F1, F2). **Precision 64 / 66 = 0.970.**

## Fixes (2)

1. `docs/m2-r8-oracle-rung-physics.md` headline and section 3.6: "every later field" -> "most later fields" with the
   two unchanged fields named (`[audit: ...]`).
2. `src/Game/CommandLayer.c` `hyperspaceCost`: one-line `CAT-TODO(M2-R9 physics)` (comment only; both configurations
   rebuilt, rc 0, 0 warnings); the report's table row and "What is left" item 5 annotated `[audit: ...]`.

## Reference for the next port

Mission01 stream after this port (the next producer's "before"): **debug sha256 `245b3d88387ed076`** (1,476,370 B,
`r8au-mission-a` = `-b`), release `e0c18fcf00229e0f` (IDENTICAL content); against `9f7416bb64fec98f` only `ships_rot`
moved, from tick 1. Replay streams: 10-17 on `90ec0345fef510e5`, off `8be77c5d7d8032de`; 10-18 on
`0ad45d1587d6caf9`, off `5cef18fd015db1c8`; ai `dcab88df1f2d0dab`, ai soak `fd1480fef8611673`; skirmish
`955744cb5e558abb` (LiveFireExercise2) / `03376d1dd4392208` (Armed8).

Verdict: PASS-WITH-FIXES - rung confirmed at 10-17 tick 247 / 10-18 tick 155 with the table switch (177 / 118 without), all gates reproduce on a clean build; one doc overstatement fixed and hyperspaceCost's float return re-targeted to M2-R9
