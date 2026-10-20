# Audit: M2-R10 oracle-rung-physics (producer 1)

Auditor: adversarial audit of the producer's report `docs/m2-r10-oracle-rung-physics.md` and of `git diff 29f7c0d`
(the working tree; the commits after 29f7c0d are the orchestrator's: `74816ff` workflow script, `bb051c8` / `cb31afa`
STATE.md, not port work). Every engine run went through `tools/m2_harness.sh` (plain harness, silent, headless); no
Ghidra, no GOG game, no oracle DLL. `<R>` = `original/scratch-m2/m2r2-harness/runs`, `<O>` = `original/oracle`,
`<B>` = `original/scratch-m2/build-audit-r10-oracle-rung-physics`.

## 1. Clean builds

`rm -rf <B>`, then `meson setup` / `meson compile` exactly as `tools/build.sh` does (debug: `--buildtype=debug
-Db_sanitize=address,undefined`; release: `-Doptimization=2 -Ddebug=false -Db_sanitize=none`; `werror=true` and
`warning_level=3` from meson.build): both rc 0, **0 warnings**. Sizes: debug 88,617,208 B (`246d5ea71c9b333f` before
the audit's comment fixes, `8af9bc30fd46310f` after), release 5,618,896 B (`32b6bcca2f42956d`, unchanged by the
comment fixes). Both rebuilt after the fixes: rc 0, 0 warnings. (The hashes differ from the producer's
`8ab246f0` / `3c9e013a` by the build directory only; every stream below is equal to the producer's.)

## 2. The rung, from the clean build

```
tools/m2_harness.sh r10a1-r17-{a,b} --mode replay --recording oracle01 --recording-src <O>/oracle01-rec-20261017-140428 \
    --recording-as oracle01-r6 --timeout 900 --bin <B>/debug/hwi-engine        # r10a1-r17-libm: + "-- /oracleLibmSqrt"
tools/m2_harness.sh r10a1-r18-{a,b} --mode replay --recording oracle01 --recording-src <O>/oracle01-rec-20261018-102253 \
    --recording-as oracle01-r18 --timeout 900 --bin <B>/debug/hwi-engine       # r10a1-r18-libm: + "-- /oracleLibmSqrt"
python3 -I tools/oracle/hwo_compare.py <O>/oracle01-rec-20261017-140428/oracle01-rec.hwo <R>/r10a1-r17-a/run.hwo \
    --from-tick 1 --to-tick 9600 \
    --fields rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real \
    --json <R>/r10a1-r17-a/compare.json          # then without rng6 (decision 47) -> compare-sim.json
python3 -I tools/oracle/hwo_compare.py <O>/oracle01-rec-20261018-102253/oracle01-rec.hwo <R>/r10a1-r18-a/run.hwo \
    --from-tick 1 --to-tick 3526 --fields <the same list> --json <R>/r10a1-r18-a/compare.json
                                                 # then without rng6 and real (decisions 47, 60) -> compare-sim.json
... and every field alone (--fields <one>) for the per-field ticks
```

| run | stream (pair) | first divergence (simulation fields) | per field | sync counters |
|---|---|---|---|---|
| 10-17 table | `74db522c2b63cf23` a = b (`cmp`), 2,630,926 B | **2823 ships_vel** (REF `4038fc77886a4684`, CAND `ba2254447d2e8a51`); full list: rng6 161 (excluded) | ships_vel 2823, ships_pos 3251, univcheck 3335, bullets 5479, resources 5885, ships_misc 6281, ships_health 7593; rng10, ships_n, missiles, derelicts, dt, total, real never | rand 0, univ 2911, blob 4746, pkt 0 |
| 10-17 libm | `dcdf6b40e943e1c3` | **177 ships_vel** | ships_vel 177, ships_pos 180, univcheck 190, rng10 / resources 1408, bullets 1917, ships_health 2043, ships_misc 2321, ships_n 5559 | 3347 / 4693 / 4746 / 0 |
| 10-18 table | `d5cc7d6b526491d6` a = b, 966,649 B | **2871 ships_vel** (REF `66d45cfe2163a333`, CAND `24bde02b80327b46`); full list: real 2 (excluded) | ships_vel 2871, ships_pos 2909, univcheck 3062, real 2; rng10, ships_n, ships_misc, ships_health, missiles, bullets, derelicts, resources, dt, total never | 0 / 4 / 1619 / 0 |
| 10-18 libm | `d958d1556eea3646` | **118 ships_pos + ships_vel** | univcheck 137, rng10 / resources 944, bullets 1721, ships_health 1861, ships_n / misc 1955 | 781 / 1658 / 1619 / 0 |

Every number of the report's section 7 table and headline is reproduced exactly; ASan 0, no unsuppressed leak, rc 0 in
all six runs. Against R9 (`hwo_compare` per field): 10-17 vs `r9i-r17-a`: ships_rot 1921, ships_vel 1922, ships_pos 1954,
bullets 2005, univcheck 2020, rng10 / rng_all / ships_health 2043, ships_misc 5885, ships_n / statcheck / players 5890;
10-18 vs `r9i-r18-a`: rng_all / bullets 1721, ships_rot 1743, rng0 / rng10 / ships_pos / ships_vel / ships_health /
univcheck 1749, ships_n / ships_misc / players 1753, rng6 1769, statcheck 1805 — both exactly the producer's statement.

**The shooter of 10-18 tick 1721, checked independently.** `r10a1-r18-dump` (the same replay with
`-- /oracleShipDump all@1715-1722`, stream `d5cc7d6b526491d6`, byte-identical to the replay without it) has
`T 1720 24 state=0 target=41 cmd=41`, `G 1721 24 3 ... last=42d72000:107.5625` and `G 1721 24 6 ... last=107.5625`,
and two 'B' lines at 1721 (`owner=24 target=41`); the 'S' lines name 24 an **sMothership of player 0** and 41 a
**bRecon of player 1**. An independent FNV-1a-64 over the two bullets' position words (scratch script, the hash of
`oracleHashList`) gives `1ff62819fb2ff28a`, and the GOG stream's tick 1721 record has `bullets_n 2,
bullets_h 0x1ff62819fb2ff28a` (tick 1720: 0 bullets, the empty hash in both). The producer's shooter is right.

## 3. The exe bodies, read with tools/m2_fdis.py / m2_fdec.py (26 bodies and 10 call sites)

All operand widths and roundings checked instruction by instruction against the diff:

- `collCheckLineOfSight` 0x0043c2e0: the clamp skip (0x0043c333 .. 0x0043c367: `Ship+0x10` objtype 0, `+0x1BC`
  clampInfo, host == source / target, then the target's host and the host itself) only in the source-blob loop; the
  length `fsqrt` of the QWORD `(dz*dz + dy*dy) + dx*dx`, stored float; the universe loop's
  `v = (dz*dir.z + dx*dir.x) + dy*dir.y > 0` and `r*r - (((dz*dz + dy*dy) + dx*dx) - v*v) > 0`; the target-NULL length
  `0x6258d727` = REALlyBig. Matches.
- `collCheckRectLine` 0x00439ef0: uplength 0 returns -1.0 (0x0072a468); no pointing-away miss; `(cand - origin) / dir`
  stored once; `largestMaxT * dir + origin` unrounded against both bounds. Matches.
- `collCheckBulletTargetColl` 0x0043cd90, both branches: v's operand order, `v*v` stored, dsqr on the FPU,
  `vsq - dsqr < traveldist^2`, the binary search and break unrounded against the stored float maximum, the stored
  |difference| against `collspheresize + traveldist`. Matches.
- `GenericInterceptorAttack` 0x005b2620: STATE_WHIPSTRAFE1 / STATE_KILL normalisation (QWORD `(z*z + y*y) + x*x`,
  stored distance, `1.0f / dist` kept, one rounding per component), `bulletRange * 0.7f` (0x0072aaf4 = 0x3f333333),
  the elapsed time, the break angle (`fild randeg; fmul 0x0072a4f8` = 0x3c8efa35 = the tree's float RADIAN_PER_DEG,
  `fsin` / `fcos`), the KILL dot product (below 0 or unordered: no target velocity), BREAK's squared distance and
  heading dot. Matches.
- `GenericInterceptorCanFire` 0x005b2120 (0x005b2183 elapsed on the FPU), `aishipGetTrajectoryWithAngleCorrection`
  0x00413f10, `gunCanShoot` 0x0048a2f0 (first health compare unrounded, the QWORD factors, every elapsed compare
  unrounded, the passive penalty `draw * ((hi - lo) * 2^-32) + lo + firetime` against the stored elapsed time),
  the burst cut in `cat_univShipDeferredUpdate` (0x005449ce: `useBurstFireTime + QWORD 0x3fb999999999999a`),
  `ApplyCareenRotationDirectly` 0x00545a80, the rocks and recoils of `univBulletCollidedWithTarget` 0x00546ca5 ..,
  `univMissileCollidedWithTarget` 0x0054711d .., `MakeExplosionRockBumpables` 0x005460c0 .. (and
  `pointExplosionInSpace` 0x005468e0 .., the helper's other caller, the same shape), `aiuFindDistanceSquared`
  0x0041a2c0, `tacticsGetShipsMaxVelocity` 0x00529450, `cat_kamikazeSteer` 0x005b172f / 0x005b1916. Match.
- Call sites: 0x00417361, 0x00408388, 0x004e4a05, 0x00414ac9, 0x004fa3c0, 0x004fa43c, 0x00483455, 0x004834f0,
  0x005aece5, 0x0041bd1f: each as the C now does. `call 0x41a2c0` occurs **35** times in the exe and
  `call 0x529450` **16** times: the report's counts are right.
- The seven Blobs.c bodies: `FillInCollisionBlobSpecificLists` 0x00425d90 (bullets out of the target lists, +0x48 for
  big / small, +0x4C for the ship / resource / derelict maxima), `AddObjToObjectListsOfBlob` 0x00427b20,
  `bobObjectDied` 0x004278e0 (bullets out), `bobListCreate` 0x00426950 (the jump table 0x00426d98: bullet, Asteroid0
  and non-mine missile clear `collMyBlob` at 0x00426bd2 as the dead / satisfied objects do, a world derelict and an
  effect do not — the C matches; the volume `(r*r)*r * 4.18879f` (0x0072a570 = 0x40860a92) stored, 1 / the unrounded
  volume; the sort distance), `bobBlobCombine` 0x00426ff0 (0x0042715e), `bobListUpdate` 0x00427460 (0x0042750b ..
  0x004276a8, every compare and store), `bobObjectListMedian` 0x00426dc0 (maxV.z held on the FPU but always a loaded
  float, so the tree's floats are right; the extents and the z-wins tie). Match; review rows present.
- IDENTICAL / unchanged bodies compared: `aishipGetTrajectory` 0x00413ee0 and `vecCapVector` 0x00550950 (as the tree).
- **`RangeToTarget` 0x00439c30 and `RangeToTargetGivenDist` 0x00439c80: NOT as the tree (finding F1).**
- `cat_gunCanFire` 0x0048a1c0: the head (dissolving, second mode, the call into `gunCanShoot`) as the tree, as the
  report says; the second-mode tail keeps `totaltimeelapsed - firetime2 / n` unrounded (0x0048a2a8 .. 0x0048a2ba)
  where the tree computes it in floats (open item O1, not a claim of the report).

## 4. The diff

- Scope: every changed file is in the producer's list; no struct changed; `tools/m2_harness.sh` changes only header
  comments (the 'T' / 'G' lines). Every Cataclysm change carries a `CAT` comment; the one deviation where the exe would
  fault was not marked CAT-FIX (finding F2, fixed).
- `/bin/grep -rn 'CAT-TODO(M2-R10' src` = 0; no CAT-TODO names M2-R9 or M2-R10; both `CAT-TODO(M2-R10 physics)` closed by
  the port.
- Delta map: every edited exe function the map knows (27) is MODIFIED with an `[review: M2-R10 oracle-rung-physics]`
  row; the 13 formerly IDENTICAL are among them. **Generated files reproduce**: `tools/delta_map.py merge` on 29f7c0d's
  `delta_map.json` with the current review file (`--done m2r10:oracle-rung-physics`) gives a file byte-identical to the
  tree's ("records changed 27"; IDENTICAL 2,760, MODIFIED 1,577, REWRITTEN 83, reviewed 3,521), and `report` gives a
  `docs/m2-r1-function-deltas.md` byte-identical to the tree's.
- The 'T' / 'G' dump is read only: the dumped replay's stream equals the plain one.

## 5. The guard rails (clean build, plain harness)

| run | result | stream | against R9 / the producer |
|---|---|---|---|
| `r10a1-mission-{a,b,perturb}` (`tools/oracle/mission_test.sh --ticks 6000 --perturb 3000 --copy-tick 4000`, debug) | MISSION TEST PASS, ASan 0 | a = b **`44f6c81a6bf3f58d`**, 1,476,370 B | **IDENTICAL to `r9i-mission-a` `553845b65e87f3b5` over ticks 0-6000** (only the build identity differs) |
| `r10a1f-mission-{a,b,perturb}` (the same after the audit's fixes) | PASS, ASan 0 | **`44f6c81a6bf3f58d`** | IDENTICAL to R9 |
| `r10a1f-mission-rel-{a,b}` (release, `--no-perturb-run`) | PASS | **`96267d55360e6f4e`** | IDENTICAL to `r9i-mission-rel-a` `ba8ee6e78597c175` and to the debug stream |
| `r10a1-ai-{a,b}` | 16/16 PASS, ASan 0 | `4250574d4d40fa6b` (pair) | IDENTICAL to `r9i-ai-a` |
| `r10a1-ai-soak` (4,000) | `done tick=3998 checks=16/16 result=PASS`, game over 1377, ASan 0, no leak, 0 fatal | `1061c06edbc69903` (= producer's) | vs `r9i-ai-soak`: rng10 / rng_all / ships_health 431, ships_pos / vel / univcheck 455, ships_rot 457, bullets 458, missiles 589, rng0 / ships_n / ships_misc 717, rng6 1072, players 1137, statcheck 1430, resources 1984: the producer's statement |
| `r10a1-skirmish-lfe2` | 600 ticks, 91 ships, PASS, ASan 0 | `97744e77a08306ac` | IDENTICAL to `r9i-skirmish-lfe2-a` |
| `r10a1-skirmish-armed8` | 54 ships, PASS, ASan 0 | `728bfa5c67376178` | IDENTICAL to `r9i-skirmish-armed8` |
| `r10a1-missiontest-{a,b}` | 6/6, done 11424, PASS, ASan 0 | `3775f5b80e300357` (pair, = producer's) | vs `r9i-missiontest-a` from 1772 (rng10 / rng_all / pos / vel / rot / univcheck), bullets 1782, ...: the producer's statement |
| `r10a1-m5-{a,b}` | 6/6, 16384, PASS, ASan 0 | `71dca0e67fb1271a` (pair, = producer's) | ships_rot 2518, ships_vel 3394, ships_pos 5033, rng10 / ships_health / bullets 5034: as stated |
| `r10a1-m10-{a,b}` | 5/5, 21939, PASS, ASan 0 | `c110fe40224ec341` (pair, = producer's) | ships_rot 1, ships_pos / vel 358, univcheck 359, rng_all 6695, rng10 / bullets 9582: as stated |
| `r10a1-m17-{a,b}` | 4/4 + `game end tick=15584`, PASS, ASan 0 | `1941035ad9047ec7` (pair, = producer's) | ships_rot 2, ships_vel 3, ships_pos 15, bullets 78, rng10 / rng_all 108, univcheck 170: as stated |
| `r10a1-fe` (120 s) | Main_game_screen, rc 124 at the timeout, ASan 0, 0 fescreen reports | - | as R9 |
| `r10a1-sp` (`--timeout 340`) | universe running, walked Mission01_NIS01 -> 01 -> 02 -> 03 -> Mission04_NIS02, last univUpdateCounter 688 (machine loaded: a game and an editor were running; the producer's 800), ASan 0, no leak | - | the same walk as the producer's |
| `r10a1f-r18-a` (10-18 replay on the final debug binary) | ASan 0 | `d5cc7d6b526491d6` | unchanged by the audit's fixes |

## 6. Findings

- **F1 (wrong claim, doc fixed):** the report's section 5 lists `RangeToTarget` / `RangeToTargetGivenDist` among the
  bodies "found as the tree". Both return the **unrounded** difference on the FPU (0x00439c64 / 0x00439c8b `fsub`, then
  only the clamp and `ret`); the tree's `real32` return rounds it. `GenericInterceptorAttack`, which this port changed,
  compares `RangeToTargetGivenDist`'s value unrounded with `bulletRange * 0.7f` (0x005b2c6f) and with `bulletRange`
  (0x005b3014), so the port's WHIPSTRAFE1 `(double)range < ...` compares a value already rounded; `RangeToTarget`'s 25
  call sites include unrounded uses (0x0041e487, 0x0044ba56, 0x0044bd97, 0x005aff45). No stream shows it yet. Not ported
  by the audit (a stream move): owner M3 oracle physics, two one-line `CAT-TODO(M3 oracle physics)` at the functions
  and an `[audit: ...]` note in the report's section 5 and "What is left" item 5.
- **F2 (convention, fixed):** `cat_collLosSkipClamped`'s `target != NULL` guard is a deviation where the exe would fault
  (it reads the target unguarded at 0x0043c34e) but was not marked CAT-FIX (decisions 19 / 25 / 62); now marked.
- **O1 (open item, not a wrong claim):** `cat_gunCanFire`'s second-mode threshold is unrounded in the exe (section 3);
  recorded with F1's owner.

No other finding: every stream move is stated, every number checked is right, generated files reproduce.

## 7. Fixes (3)

1. `src/Game/Collision.c`: the guard comment marked `CAT-FIX` (comment only).
2. `src/Game/Collision.c`: `CAT-TODO(M3 oracle physics)` one-liners at `RangeToTarget` and `RangeToTargetGivenDist`
   (comment only; release binary byte-identical, the debug streams unchanged).
3. `docs/m2-r10-oracle-rung-physics.md`: `[audit: ...]` note in section 5 and item 5 of "What is left".

## 8. Precision

Sampled checks 67: the build, the six replay runs and their per-field and sync numbers (13), the shooter, the moves
against R9 of the replays, the mission stream (debug and release), the ai pair and soak, both skirmishes, the
missiontest pair and Missions 05 / 10 / 17 pairs, fe, sp, the CAT-TODO grep, the delta map merge and report, the two
call-site counts, 26 bodies and the call sites, the constants, the review rows. Wrong: 2 (F1, F2).
**Precision 65 / 67 = 0.970.**

## 9. References for the next port

Mission01 stream: debug **`44f6c81a6bf3f58d`** (content IDENTICAL to `553845b65e87f3b5`), release **`96267d55360e6f4e`**
(IDENTICAL to `ba8ee6e78597c175`). Replays: 10-17 `74db522c2b63cf23` (rung 2823), 10-18 `d5cc7d6b526491d6` (rung 2871),
libm `dcdf6b40e943e1c3` / `d958d1556eea3646` (177 / 118). ai `4250574d4d40fa6b`, soak `1061c06edbc69903`, skirmish
LFE2 `97744e77a08306ac`, Armed8 `728bfa5c67376178`, missiontest `3775f5b80e300357`, Mission05 `71dca0e67fb1271a`,
Mission10 `c110fe40224ec341`, Mission17 `1941035ad9047ec7` (the others as the producer's section 9, not rerun here).

Verdict: PASS-WITH-FIXES - the rung moved to 2823 / 2871 exactly as reported (shooter of 1721 confirmed by the GOG bullet hash), every stream and number reproduced from a clean build; one wrong "as the tree" claim (RangeToTarget's unrounded returns, now owned by M3) and one unmarked CAT-FIX, both fixed in place
