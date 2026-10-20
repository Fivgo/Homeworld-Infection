# Audit: M2-R9 oracle-rung-physics

Auditor: adversarial audit of the M2-R9 producer 1 port (report `docs/m2-r9-oracle-rung-physics.md`), base `54dcbb2`.
The port is the working tree on top of `9f2c5bd` (the four commits after the base, `e249cc0` tools/play.sh, `48925e2`
the M2-R9 workflow script, `fd00c1a` / `9f2c5bd` STATE.md and `9e92b43` decision 70, are the user's / orchestrator's:
not port work). Every engine run below went through `tools/m2_harness.sh` (plain harness, silent, offscreen; no GOG
game, no DLL, no input), with the auditor's own clean build passed with `--bin`. Exe bodies were read with
`tools/m2_fdis.py`, `tools/m2_exedata.py` and the R2 objdump listing (`original/scratch-m2/m2r2-mesh-tools/exe.dis`);
no Ghidra.

## 1. Clean builds

`rm -rf original/scratch-m2/build-audit-r9-oracle-rung-physics`, then `meson setup` with `tools/build.sh`'s arguments
(debug: `--buildtype=debug -Db_sanitize=address,undefined`; release: `-Doptimization=2 -Ddebug=false -Db_sanitize=none`;
`werror=true` is the project default) and `meson compile`:

| config | result | warnings | size | sha256 |
|---|---|---|---|---|
| debug | `[254/254]`, rc 0 | 0 | 81,716,944 B | `0e7c4d68c5aaa9fa` |
| release | `[254/254]`, rc 0 | 0 | 4,528,080 B | `db18c96a572e2128` |

The release size equals the producer's (its hash differs only through the `__FILE__` strings, which carry the build
directory's relative path to the source); the debug size differs through the same paths. After the audit's code fixes
(section 5) both configurations were recompiled (`[4/4]`, rc 0, 0 warnings): debug 81,717,096 B `948c4bc8bfb0b960`,
release 4,528,080 B `46480993d5d6af08`; every run of section 2 used these final binaries (`r9a2-*`). `tools/m2_layout32.sh`:
OK (Ship 1596, Player 25904, Universe 252348; 45 specifics files). `tools/delta_map.py merge` on the base's map with
the producer's review file reproduced `ghidra/delta_map.json` byte for byte, and `report` reproduced
`docs/m2-r1-function-deltas.md` byte for byte (before the audit's review-note fixes, which were merged the same way).

## 2. Runs (auditor's clean build, plain harness)

The first replay set (`r9a-*`, the unfixed clean build) and the final set (`r9a2-*`, after the fixes) gave the same
bytes for every replay stream: the audit's fixes move no stream.

| run | mode | result | stream sha256 (producer's) |
|---|---|---|---|
| `r9a2-r17-{a,b}` | replay 10-17, default (table) | `ticks=9601 packets=4800 ships=80 randSyncErr=1918 univSyncErr=3767 blobSyncErr=4746 pktSyncErr=0`; ASan 0, no leak; **a = b** (cmp) | `c27c314ed8c95b7f`, 2,630,929 B (same) |
| `r9a2-r18-{a,b}` | replay 10-18, default | `ticks=3527 packets=1763 ships=55 randSyncErr=880 univSyncErr=875 blobSyncErr=1619 pktSyncErr=0`; ASan 0; **a = b** | `0aa88c67961516de`, 966,653 B (same) |
| `r9a2-r17-libm` | replay 10-17, `-- /oracleLibmSqrt` | done, `randSyncErr=3347 univSyncErr=4693`; ASan 0 | `d3bdc1a5c525415f` (same) |
| `r9a2-r18-libm` | replay 10-18, `-- /oracleLibmSqrt` | done, `randSyncErr=983 univSyncErr=1658`; ASan 0 | `a67d3048dbeb47f0` (same) |
| `r9a2-d17` | replay 10-17, `-- /oracleShipDump all@1920-1922` | stream byte-identical to `r9a2-r17-a` | (dump for hwo_invert) |
| `r9a2-mission-{a,b}` | mission, 6,000 ticks, debug | oracle mission run done; ASan 0, no leak; a = b (cmp, and hwo_compare IDENTICAL over 6,001 ticks) | **`1028375d20f15d75`**, 1,476,370 B (same) |
| `r9a2-mission-rel-{a,b}` | mission, 6,000 ticks, release | a = b | `e06354cba52dca2b` (same) |
| `r9a2-ai-{a,b}` | ai, 400 | ASan 0; `checks=16/16 result=PASS`; a = b | `124358d63c5cb0f9` (same) |
| `r9a2-ai-soak`, `-2`, `-3` | ai, **4,000** | each: ASan 0, no leak, no fatal, `done tick=3998 ... checks=16/16 result=PASS`, game over at 1389 | `-2`, `-3`: `f2036d7b92e10f37` (same); the first, run with four other ASan runs beside it: `b09734b25fbdcbde` (`rng_all` only, from tick 1115; finding 6) |
| `r9a2-ai-soak-prodbin` | ai, 4,000, the producer's final binary | ASan 0, PASS | `f2036d7b92e10f37` |
| `r9a2-skirmish-lfe2-{a,b}` | skirmish LiveFireExercise2 | `ticks=600 ships=89 result=PASS`; ASan 0; a = b | `2eb7b79363150f2d` (same) |
| `r9a2-skirmish-armed8` | skirmish Armed8 | `ships=54 result=PASS`; ASan 0 | `462a2414ccffa6e5` (same) |
| `r9a2-missiontest-{a,b}` | missiontest | `done ticks=11424 objectives=6/6 result=PASS`; ASan 0; a = b (61 / 63 s) | `929138a2d6baac97` (same) |
| `r9a2-loadgame-a` | loadgame "01 - Hiigara", 3,600 | PASS; ASan 0 | `dd6bcab1d2af2a6b` (same) |
| `r9a2-build-a` | build | PASS; ASan 0 | `b3dc0cf689bf6cef` (same) |
| `r9a2-motion-noi`, `r9a2-nis-noi` | motion / nis `-- /noInterp` | PASS; ASan 0 | dumps `183681235a3fdc7b` / `13ca70eea998e5de` (same) |
| `r9a2-fe` | fe, 120 s | rc 124 at the timeout, **Main_game_screen**, ASan 0, no leak, 0 fescreen reports | |
| `r9a2-sp` | sp, `--timeout 340` | rc 124, universe running (last `univUpdateCounter` 4,608, real time), ASan 0, no leak; UBSan kinds: R8's plus `Select.c:1153/1154` (finding 4) | |
| `r9a2-sp-libm` | sp, `--timeout 340`, `-- /oracleLibmSqrt` | ASan 0; the same `Select.c` report | |

### hwo_compare (the report's arguments)

```
python3 -I tools/oracle/hwo_compare.py original/oracle/oracle01-rec-20261017-140428/oracle01-rec.hwo <run>/run.hwo \
    --from-tick 1 --to-tick 9600 \
    --fields rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real \
    --json <run>/compare.json                       # then without rng6 (decision 47) -> compare-sim.json
python3 -I tools/oracle/hwo_compare.py original/oracle/oracle01-rec-20261018-102253/oracle01-rec.hwo <run>/run.hwo \
    --from-tick 1 --to-tick 3526 --fields <the same list> --json <run>/compare.json   # then without rng6 and real (decisions 47, 60)
```

| recording, sqrt | full list | without the excluded fields | per field |
|---|---|---|---|
| 10-17, table (`r9a-r17-a`, `r9a2-r17-a`) | 161 rng6 (excluded) | **1922 `ships_vel`** (last equal 1921) | rng10 2043, ships_n 5890, ships_misc 5885, ships_pos 1954, ships_vel 1922, ships_health 2043, bullets 2005, resources 5885, univcheck 2020; missiles, derelicts, dt, total, real none |
| 10-17, libm | 161 rng6 | **177 `ships_vel`** | ships_vel 177, ships_pos 180, univcheck 190, rng10 1408, resources 1408, bullets 1917, ships_health 2043, ships_misc 2321, ships_n 5559 |
| 10-18, table | 2 real (excluded) | **1721 `bullets`** (REF `bullets_n=2`, CAND `bullets_n=0`) | bullets 1721; rng10, ships_pos, ships_vel, ships_health, univcheck 1749; ships_n, ships_misc 1753; resources, missiles, derelicts, dt, total none |
| 10-18, libm | 2 real | **118 `ships_pos` + `ships_vel`** | ships_pos/vel 118, univcheck 137, rng10 944, resources 944, bullets 1721, ships_health 1749, ships_n / misc 1753 |

All equal the report's section 6 tables. The first divergent packets (10-17: packet 46, REF tick 93, blobcheck; 10-18:
packet 136, tick 273) are the stock `players` differences (decision 47). Ship 40's velocity at 10-17 tick 247 and ship
28's v.z at 10-18 tick 155 lie inside the IDENTICAL ranges. `hwo_invert.py <10-17 GOG .hwo> r9a2-d17/shipdump.txt
--tick 1922 --ship 41 --field vel`: one solution, ship 41 REF `(c20658b4, c458d6a4, c11179e3)`, ours `(..., c11179e2)`,
raw bits REF - ours `+0 +0 +1` (GOG's v.z one ulp more negative): the report's 6.1 holds.

### Stream moves against R8's guard-rail streams (`perfield`: hwo_compare one field at a time, every field)

The table step alone (the producer's `r9p1fs-*` streams against `r8i-*`): both replays and both libm replays IDENTICAL
in every field to R8's switch-on / switch-off replays; Mission01 debug and release IDENTICAL; ai 400 `ships_pos/vel/rot
17, univcheck 20`; skirmish LFE2 `321 / 321 / 321, univcheck 326`; Armed8 `ships_pos 323, ships_vel/rot 322, univcheck
357`; build `ships_vel 4206, ships_pos 4212, ships_rot 4236, univcheck 4250`; missiontest `ships_vel 2, ships_pos / rot /
univcheck 1772, bullets 1795, rng10 2055 ...`; loadgame `ships_pos/vel/rot 1985, bullets 1985, univcheck 1987, rng10
2206 ...`. All as section 1 of the report states.

The whole round (`r9a2-*` against `r8i-*`): Mission01 (debug `0d2b76d565105a26` -> `1028375d20f15d75`, release
content likewise) `ships_pos 0, univcheck 0, ships_rot 1`, every other field IDENTICAL over 6,001 ticks; replay 10-17
`ships_rot 241, ships_vel 247, ships_pos 259, univcheck 315 ...`; 10-18 `ships_vel 155, ships_rot 172, ships_pos 179
...`; ai `ships_pos 0, univcheck 0, ships_rot 1, ships_vel 17`; ai soak `ships_pos 0, ships_rot 1, ships_vel 17, bullets
413, rng10 471 ...`; skirmish LFE2 `321 / 321 / 321, univcheck 326` (against the table-only stream: `ships_vel/rot 323,
ships_pos 381`); Armed8 unchanged against the table-only stream; build `ships_pos 0, univcheck 0, ships_rot 1, ships_vel
4206, rng_all 4319`; missiontest `ships_pos/misc/rot/health/resources 0, ships_vel 2, rng6 5, univcheck 39, bullets 1783,
rng10 1785 ...`; loadgame `1985 ...`. All as the report's section 8 states. The Mission01 attribution to the exe's qsort
(`formationArrangeOptimum`) holds against the producer's experiment `r9p1-e4-mission` (C library qsort): IDENTICAL in
every field to the table-only `r9p1fs-mission-a`.

## 3. The exe bodies compared (23)

| # | function | verdict |
|---|---|---|
| 1 | `physUpdateObjPosVelShip` 0x004e4950 | as the tree: a.x `(1/m * F.x) * t` one rounding, a.y / a.z stored; rotation alpha.x one rounding, the cap, `fsin` / `fcos` of the double angle rounded at their `fstp`, `matMakeRotAbout*(m, cos, sin)` order. Not the producer's scope but found: the top-speed product (finding 7) |
| 2 | `aitrackHeadingWithBankPitchFunc` 0x00419200 | as the tree: des² stored float, cur² double; `1/|cur|` stored, `1/|des|` double; des.y `fst` with the unrounded product in the dot; the pitch selection (both `pitchdescend` signs); `fptan` -> `fst` des.z -> `fmul` by the float at 53 bits (the tree's `fma` split reproduces that single rounding) -> `+ 1.0f` -> fsqrt -> `fdivr`. One caveat: the tree takes `tanl` for the x87 `fptan` (precision control does not apply to `fptan`); the two can differ in the last extended bit; R8's choice, unchanged |
| 3 | `protectShip` 0x004411c0 | as the port: `max(size2, size1)` on the FPU, `(range + larger) + shipSize` one rounding at 0x00441236; followbehind `1.5 * desiredrange(float)` a double, `point - heading * fb` one rounding per component |
| 4 | `hyperspaceCost` 0x00443c30 | as the port: no store at the end; the cumulative `dist -= minDistance` on the FPU; `> max` replaced |
| 5 | `aimProcessHyperspace` 0x00407dd0 | `aiuFindDistanceSquared`'s FPU double into `fstp qword` + fsqrt (0x00407e8c), distance stored (0x00407e94), `_ftol` 0x00407ea9: as the port |
| 6 | `clMpHyperspace` 0x004474a0 | from - to doubles, `(dz² + dy²) + dx²`, fsqrt, stored, `_ftol` 0x0044750a: as the port |
| 7 | `pieDistanceReadoutDraw` 0x004e61b0 | `_ftol` at 0x004e6240: as the port |
| 8 | the main region's hyperspace move 0x00564bca .. 0x00564c2a | selCentrePoint - destination doubles, fsqrt, `_ftol`, `RUs >= cost` skips: as the port |
| 9 | `cat_aisTeamAttack` at 0x00433170 | `fild` RUs, `cost + 10000.0f` on the FPU, `fcompp`: as the port |
| 10 | `aishipFlyToPointAvoidingObjsFunc` 0x00414610 (0x00415516 .. 0x00415613) | esi = the blob list's object (0x00414de3), edi = the ship (0x00414624, `[ebp+8]`): the first `GetCollSizeInDirection` takes esi and is stored as a float (0x004155c5), the second takes edi and is added unrounded (0x004155e0); the too-close test compares the unrounded root (0x00415554 `fst`, 0x0041555b `fcomp`): the producer's claims hold |
| 11 | `fsqrt` 0x00472860 | the table lookup as the tree, but the 0.0 test is `fcomp` / `test ah,0x40`, which also takes a NaN argument: finding 5 (fixed) |
| 12 | `fmathInit` 0x004728c0 | identical: `(i | 0x7fe000) << 7` and `(i | 0x800000) << 7`, `fsqrt` at 53 bits, the high mantissa word into 0x00a2e144 + 4i and + 0x8000 |
| 13 | `ObjectsCollided` 0x005456d0 | the port's shape holds (repulsion, the two inlined helpers, the speed squares and their compare, the y / z momenta, the kamikaze tests, the ratio and the dot with their unordered outcomes, the tangential force, the left / right test, the force signs) except the x reciprocal: finding 1 (fixed) |
| 14 | `collCheckBigShipSmallShipColl` 0x0043b460 | the clamped-host test (0x0043b578 .. 0x0043b5a7, edi first, then esi) and the distance test (0x0043b83f `fst` distsquared, the unrounded compare with colldist², the root of the stored float after the bump and NotBumpable tests): as the port |
| 15 | `cat_dockFlyOutOfCone` 0x004650d0 (0x0046515e .. 0x0046528e) | far4 double; target x / y stored then added, z unrounded; dz `fst` and its unrounded double in the dot; t double; onaxis stored; the distance test over the stored differences: as the port |
| 16 | `cat_gunFixedGunAimedAtTarget` 0x0048cd40 (0x0048ce50 .. 0x0048cf26) | as the port, including `0.75f` (0x0072a670 = 0x3f400000) and the `fcompp` sense |
| 17 | `protectShipsAvg` 0x004413f0 | `fild` / `fdivr 1.0f`, one rounding per component; `range + size` on the FPU against `getVectDistSloppy`'s float (0x00550a90 returns a loaded float): as the port |
| 18 | `bobFindNearestBlobToObject` 0x00425cb0 | objtype 0 -> +0xE0, else +0x58; the double square `fst` as the float and compared unrounded with the float minimum: as the port |
| 19 | `bobUpdateObjsInBlobCollInfo` 0x00426050 | the flags (`+0x35a`, `+0x358 && +0x616`), the collision position, the double square into fsqrt, the qsort at 0x00426145 with comparator 0x00425d60: as the port; `bobAddObjToSpecificBlob` 0x00427e90 and `bobBlobItemize` 0x00427371 .. 0x00427381 likewise |
| 20 | CRT `qsort` 0x00671060, `shortsort` 0x006712a0, `swap` 0x00671310 | `catQsort` matches line for line: `size = (hi - lo) / width + 1`, cutoff 8, middle swapped to lo, `comp(loguy, lo) <= 0` / `comp(higuy, lo) >= 0` with the `loguy <= hi` / `higuy > lo` guards, signed `higuy - 1 - lo >= hi - loguy`, two stacks of 30, shortsort's `comp(p, max) > 0`, the byte swap that skips a == b |
| 21 | the blob comparator 0x00425d60 | `fcomp`, `test ah,0x41`: 1 when greater, else -1 (never 0): as `CompareSpaceObjCollOptimizeDistCB` |
| 22 | `aiuFindDistanceSquared` 0x0041a2c0 | returns `(dz² + dy²) + dx²` on the FPU with no store: the producer's `CAT-TODO(M2-R10 physics)` is right |
| 23 | `getVectDistSloppy` 0x00550a90 (IDENTICAL) | identical (absolute values, the larger of the three, loaded from memory as a float) |

The exe has exactly 14 calls to 0x00671060 (0x00421798, 0x00426145, 0x00426234, 0x00428ae9, 0x00428afb, 0x0048138b,
0x00487644, 0x004d4ab1, 0x004d4f62, 0x00504462, 0x00508fcd, 0x00508fde, 0x00509e39, 0x00588327); each is the address
in a `CAT` comment at a `catQsort` call; the tree's remaining `qsort` calls are `Memory.c` (debug statistics),
`selShipsInSelection` (none in the exe) and `src/Win32/texreg.c` (not built).

## 4. The diff (git diff 54dcbb2 + the untracked CatSort.c / .h and the report)

- Scope: every src/ change is in the producer's listed files; the out-of-scope paths in the diff (`cycle/STATE.md`,
  `cycle/workflows/m2-r9.js`, `tools/play.sh`) come from the user's / orchestrator's commits. `tools/m2_harness.sh`:
  header comments only (the new dump lines, `/oracleLibmSqrt`).
- CAT comments: every Cataclysm change carries one with the exe address; CatSort.c / .h have the REULA header; the
  instrumentation (`oracleShipDumpFly`, `oracleShipDumpAvoid`, the 'B' lines) is inside `#if HWI_ORACLE` and marked
  HWI-ORACLE; its extra `catGetCollSizeD` calls are pure (no state written), so the stream cannot depend on them.
- CAT-TODO: `grep -rn 'CAT-TODO(M2-R9 physics' src/` = 0; every CAT-TODO is on one line, owner first. By owner after the
  audit: stage 5 9, stage 5 LAN 5, stage 5 multiplayer 1, stage 5 multiplayer options 7, stage 5 recorder 4, M2-R9 part B
  2, M2-R10 physics 2 (the producer's `aiuFindDistanceSquared` and the audit's top-speed product).
- Delta map: 34 review entries (6 new, 28 with the note appended), nothing removed, every edited function that the base
  map called IDENTICAL (`bobFindNearestBlobToObject`, `bobUpdateObjsInBlobCollInfo`, `bobUpdateExtraCollBobInfo`,
  `bobAddObjToSpecificBlob`, `ObjectsCollided`, `aiuFindDistanceSquared`, `animLoadNISScript`, `btgZip`,
  `formationArrangeOptimum`, `gpTitleListLoad`, `selSelectionCompare`) has a review entry and is MODIFIED now; counts
  IDENTICAL 2,762, MODIFIED 1,575, REWRITTEN 83, reviewed 3,346 (as reported). Generated files: the map and
  `docs/m2-r1-function-deltas.md` reproduce from their generator (section 1); no kas2c output was touched.
- Stream moves: every moved stream is stated in the report with its fields and first ticks (section 2 confirms them).

## 5. Findings

1. **`ObjectsCollided` 0x005456d0: the x momentum difference is scaled by the UNROUNDED 1/t, not the stored float**
   (wrong claim + code shape). 0x00545807 `fld 1.0f`, `fdiv phystimeelapsed`, 0x00545813 `fst dword [esp+0x40]` (the
   float, used for y and z), 0x00545817 `fmul st,st(1)`: st(0) is still the double quotient. The port used
   `(double)oneovertime * collforcexd` with the float, and its comment and the report said "the stored 1/t". Same words
   while `phystimeelapsed` is a power of two (1/16 s: 1/t = 16 exactly), so no stream moves. **Fixed**:
   `(1.0 / (double)universe.phystimeelapsed) * collforcexd`, the comment and the report's section 3 marked `[audit]`.
2. **`hyperspaceCost`'s review entry contradicted itself**: its change text read "... rounded once at the return; returns
   the unrounded double". **Fixed** in `ghidra/delta_map_review.json` ("the sum returned unrounded on the FPU (M2-R9; R8
   rounded it once at the return)", marked `[audit]`), merged (`records changed 1`) and the report regenerated.
3. **Stale review notes for `fsqrt` 0x00472860 / `fmathInit` 0x004728c0**: they still said "NOT ported, left as an open
   question for the user" / "absent from this tree (FastMath.h uses sqrtf)", while this port made the table square root
   the engine default. **Fixed**: a `[review: M2-R9 oracle-rung-physics audit]` note appended to both (class stays
   IDENTICAL: the tree's CAT file reproduces the exe's HW1 bodies), merged (`records changed 2`), report regenerated.
4. **sp: a new UBSan kind, not "R8's"**: the report's sp row said "UBSan kinds R8's". The producer's `r9p1f-sp`, the
   audit's `r9a2-sp` and `r9a2-sp-libm` each have 2 reports `Select.c:1153` / `1154` signed integer overflow `384 -
   -2147483648` in `selCircleCompute` (called from `rndShipVisibleUsingCoordSys`, render.c:1592); R8's `r8i-sp` has none.
   It is present with `/oracleLibmSqrt`, so it is not the square root; non-fatal, ASan 0. A screen coordinate converted
   from an out-of-range float (`INT_MIN`) enters an `int` subtraction (a ship's selection circle off any sane range in
   the NIS-first start). **Report annotated** `[audit]`; owner M2-R10 (find which ship / frame, then decide whether the
   exe's `_ftol` arithmetic needs an unsigned or 64-bit shape here).
5. **`catFmathSqrtDouble` (fsqrt 0x00472860) on a NaN argument**: the exe's `fcomp qword [0x0072a4e8]` (0.0) /
   `test ah,0x40` / `je` returns 0.0 whenever C3 is set, which an unordered compare also sets; the tree tested `f == 0.0`
   only. R7's body, made the default by this port. **Fixed**: `(f == 0.0) || isnan(f)` with a `CAT` comment marked
   `[audit]`. No stream moved (every rerun stream equals the producer's).
6. **The ai soak's `rng_all` is not reproducible under load** (observation; not the producer's claim, which its own run
   and the audit's two quiet reruns reproduce, `f2036d7b92e10f37`): the audit's first soak, run beside four other ASan
   runs, differed only in `rng_all` from tick 1115 (`b09734b25fbdcbde`). Every other field was IDENTICAL; the ai mode
   does not turn sound off as `/oracleReplay` does (main.c `OracleReplaySet`: the sound stream drawn on the mixer
   thread's wall clock). The 400-tick ai pair is not affected. Owner: the harness (ai / soak runs should be judged without
   `rng_all`, or the mode should silence that stream as the replay mode does); recorded for the integrate.
7. **`physUpdateObjPosVelShip`'s top speed** (found, outside the port's changes): `tacticsGetShipsMaxVelocity`
   0x00529450 returns `speedmult * maxvelocity` UNROUNDED on the FPU (0x005294b2 `fmul`, `ret`) and the caller multiplies
   it by the stored speed multiplier with one rounding (0x004e4a05, 0x004e4a14 `fstp`); the tree's `real32` return rounds
   the product first. Equal words unless both factors are not 1. **Tagged** `CAT-TODO(M2-R10 physics)` (one line, owner
   first) in `src/Game/Physics.c`; no stream moved.
8. Doc precision: the headline's "v.z +1 ulp" (raw bits, GOG - ours) and section 6.1's "-1 ulp" (value) use two
   conventions; **annotated** `[audit]` in the headline (GOG's v.z is one ulp more negative, as `hwo_invert` shows).

Not findings: the `tanl` stand-in for `fptan` in `aitrackHeadingWithBankPitchFunc` (R8's documented choice; no
divergence points at it); the instrumentation's extra `catGetCollSizeD` calls (pure).

## 6. Precision

55 sampled checks: the 2 clean builds; the switch (table default, `/oracleLibmSqrt`, `/oracleTableFsqrt` gone); both
replay pairs' identity and hashes; the four first divergences (1922, 1721, 177, 118) and the libm hashes; the four
per-field rows; the sync counters; ship 41's words; the table-only moves (replays, Mission01 debug / release, ai,
skirmish x 2, build, missiontest, loadgame); the round's Mission01 move and its qsort attribution; the release pair; ai
pair; ai soak; skirmish x 2; build; missiontest; loadgame; motion / nis; fe; sp ASan and counter; the 23 exe bodies
above counted as 19 claims (avoidance order, too-close compare, catQsort, the 14 call sites, the comparator, the blob
positions, the attacking flag, the big / small test, the launch cone, the fixed-gun aim, hyperspaceCost and its five
callers, protectShip, protectShipsAvg, `aiuFindDistanceSquared`, the bank / pitch tracker, the rotation path,
fmathInit); the delta-map reproduction and counts; the CAT-TODO greps; layout32. Wrong: 5 (findings 1, 2, 3, 4, 5).
**Precision = 50 / 55 = 0.91.**

## 7. Fixes (7)

1. `src/Game/UnivUpdate.c` `ObjectsCollided`: the unrounded double 1/t for x (and its comment).
2. `src/Game/FastMath.c` `catFmathSqrtDouble`: a NaN argument returns 0.0 as the exe's.
3. `ghidra/delta_map_review.json` 0x00443c30: the contradictory change text.
4. `ghidra/delta_map_review.json` 0x00472860 / 0x004728c0: the stale "not ported" notes (both merged into
   `ghidra/delta_map.json`, `docs/m2-r1-function-deltas.md` regenerated: counts unchanged).
5. `docs/m2-r9-oracle-rung-physics.md` section 3: "the stored 1/t" `[audit]`.
6. `docs/m2-r9-oracle-rung-physics.md` section 7 sp row: the new UBSan kind `[audit]`.
7. `docs/m2-r9-oracle-rung-physics.md` headline: the ulp sign convention `[audit]`.

Plus one owned tag added: `CAT-TODO(M2-R10 physics)` at the top-speed product (`src/Game/Physics.c`).

## 8. The next port's reference (final tree, the audit's binaries)

Mission01, 6,000 ticks: debug **`1028375d20f15d75`** (a = b; against `0d2b76d565105a26`: `ships_pos` and `univcheck`
from tick 0, `ships_rot` from tick 1, every other field IDENTICAL), release `e06354cba52dca2b` (a = b). Replays: 10-17
`c27c314ed8c95b7f`, 10-18 `0aa88c67961516de` (libm `d3bdc1a5c525415f` / `a67d3048dbeb47f0`); rung 1922 (10-17, ship 41
v.z) / 1721 (10-18, bullets) on the table, 177 / 118 with `/oracleLibmSqrt`. ai `124358d63c5cb0f9`, ai soak
`f2036d7b92e10f37` (quiet machine; finding 6); skirmish `2eb7b79363150f2d` (LiveFireExercise2) / `462a2414ccffa6e5`
(Armed8); build `b3dc0cf689bf6cef`; missiontest `929138a2d6baac97` (6/6); loadgame `dd6bcab1d2af2a6b`; motion / nis
`/noInterp` dumps `183681235a3fdc7b` / `13ca70eea998e5de`. fe Main_game_screen ASan 0; sp ASan 0 (with the `Select.c`
UBSan report of finding 4).

Verdict: PASS-WITH-FIXES - the table fsqrt is the default with /oracleLibmSqrt as the switch, the rung reproduces from a clean build at 10-17 tick 1922 / 10-18 tick 1721 (177 / 118 with libm), both replay pairs byte-identical with ASan 0, every stated stream move confirmed; 7 fixes (ObjectsCollided's unrounded 1/t, fsqrt's NaN case, three delta-map notes, three report statements) moved no stream
