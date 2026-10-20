<!-- done: baseline, shooter-1721, turning-coordsys, unrounded-returns, blobs-review, gates -->
# M2-R10 oracle-rung-physics: the oracle's next rung (physics)

Status: DONE (M2-R10 producer 1; base commit `29f7c0d`; nothing committed by the producer, the auditor checkpoints).

Subsystems: **baseline** (the round base's binaries and streams), **shooter-1721** (10-18 tick 1721: the shooter
named, the line of sight and the rectangle line test ported), **turning-coordsys** (the attack flight's x87 shapes, the
bullet-target test, the rocks, the gun timing: the rung to 2823 / 2871, the next divergences localised),
**unrounded-returns** (`aiuFindDistanceSquared`, `tacticsGetShipsMaxVelocity`), **blobs-review** (the seven Blobs.c
bodies), **gates** (every stream, the soak, the 17 missions, fe / sp).

## Headline

**The rung moved from tick 1922 to tick 2823 (10-17) and from tick 1721 to tick 2871 (10-18), on the default (the
table square root).** The 10-17 replay is IDENTICAL to the GOG stream in every simulation field of the list through
**tick 2822** and first diverges at **2823 `ships_vel`** (ship 48, a bProcessor of player 1); the 10-18 replay is
IDENTICAL through **tick 2870** and first diverges at **2871 `ships_vel`** (ship 49, a bProcessor of player 1). In
10-18, `rng10`, `bullets`, `ships_health`, `ships_n` and `ships_misc` now agree with GOG over the **whole recording**
(ticks 1 .. 3526); in 10-17 `rng10` and `ships_n` agree over all 9,600 ticks and the game's own sync check counts
`randSyncErr 0` (R9: 1,918). **The shooter of 10-18 tick 1721 is the sMothership 24 (player 0): its guns 3 and 6 fire
at the bRecon 41**; the tree's line-of-sight test saw the mothership's own modules in the way, the exe's skips every
ship clamped to the source. **With `/oracleLibmSqrt`** the first divergences stay at **177** (10-17) and **118** (10-18):
the square root itself. Both replay pairs are byte-identical with ASan 0; the headless Mission01 stream is unchanged
(IDENTICAL to `553845b65e87f3b5`); every mission 02-17 passes with every objective; the 4,000-tick ai soak passes with
ASan 0 and no fatal.

| step | change (exe function) | 10-17 first divergence | 10-18 first divergence |
|---|---|---|---|
| R9 (base) | the round base `29f7c0d` | 1922 `ships_vel` (ship 41) | 1721 `bullets` |
| x1 | `collCheckLineOfSight` 0x0043c2e0's clamp skip and x87 shape; `collCheckRectLine` 0x00439ef0 (no rectangle = miss, no early "pointing away" miss, x87) | 1922 | **1762** `ships_vel` (bRecon 43: +1 ulp in each component) |
| x2 | `GenericInterceptorAttack` 0x005b2620's x87 trajectory normalisation (STATE_KILL, WHIPSTRAFE1), dot products, break angle; `aishipGetTrajectoryWithAngleCorrection` 0x00413f10; `GenericInterceptorCanFire` 0x005b2120's elapsed time | **2043** `rng10` (a bullet hits a tick early) | **1796** `ships_vel` (bRecon 43, v.z) |
| x3 | `collCheckBulletTargetColl` 0x0043cd90's sphere test (`v*v - dsqr < traveldist^2`, no square root) and x87 shape | **2823** `ships_vel` (ship 48) | 1796 |
| x4 | the inlined rocks of a bullet / missile hit and an explosion, `ApplyCareenRotationDirectly` 0x00545a80 (the exe's x87 `frandombetween`, `catFrandomBetweenD`), the hit's recoil | 2823 | **1854** `bullets` + `rng10` (the bRecon 43's gun fires in GOG) |
| x5 | the burst cut's double 0.1 (`cat_univShipDeferredUpdate` 0x005449ce), `gunCanShoot` 0x0048a2f0's x87 shape | 2823 | **2871** `ships_vel` (ship 49) |
| x6 .. x8 | `tacticsGetShipsMaxVelocity` 0x00529450's double return, `aiuFindDistanceSquared` 0x0041a2c0's double return, the seven Blobs.c bodies | 2823 (byte-identical stream) | 2871 (byte-identical stream) |

## 1. shooter-1721: the sMothership's guns, the line of sight and the rectangle line test

**Naming the shooter.** GOG has 2 bullets at 10-18 tick 1721 (hash `1ff62819fb2ff28a`), 4 at 1726, 6 at 1731, 8 at
1736, and the pair of 1721 dies at 1737 (16 ticks: a 1.0 s lifetime, `BulletRange / BulletSpeed` = 6000 / 6000, the
sMothership's guns 1-6; a bRecon's is 0.4 s, an sRecon's 0.5 s). A forced-fire experiment binary (scratch only:
`original/scratch-m2/m2r10-rung/exp`, an `HWI_EXP_FF` hook that orients and shoots chosen guns at tick 1721, never in
the tree) gave the candidate bullets' exact words (`r10p1-e1-ff18`: 15 sRecon and 15 bRecon shots; `r10p1-e2-ff18`: the
sMothership's guns 1-6); a meet-in-the-middle search of the two-bullet FNV hash within +-16 ulps per component found no
sRecon or bRecon pair. The new permanent dump lines (section 6) then showed in our engine that at 1720 the sMothership
24 gets the passive target 41 (`T 1720 24 ... cmd=41`) and at 1721 its **guns 3 and 6 turn onto 41** (their `G`
headings change) **but never fire** (`last=0` through 1740). After the line-of-sight port (below) our replay creates
exactly those two bullets at 1721 (`B 1721 0/1 owner=24 target=41`, both guns' `last=107.5625`) and the bullet hash
equals GOG's: **the shooter is the sMothership 24 (player 0), guns 3 and 6, at the bRecon 41**.

**`src/Game/Collision.c`, `collCheckLineOfSight` 0x0043c2e0** (capital ships check their line of fire in `gunShoot`
0x0048b5b0). In the source's own blob (the target in it, or no target) the exe skips every ship clamped to the source or
to the target (0x0043c333 .. 0x0043c367: `Ship+0x1BC` clampInfo's host), and for a clamped target every ship clamped to
the target's host and that host; the other blobs' loop has no skip. The tree tested the mothership's modules
(sMothershipEnginesUpper etc., clamped to it, sharing its position and hull) and found the line obscured. Also the x87
shape: the length is fsqrt of the double `(dz*dz + dy*dy) + dx*dx` stored as a float; a blob is crossed when `v = (dz*dir.z
+ dx*dir.x) + dy*dir.y > 0` and `r*r - (((dz*dz + dy*dy) + dx*dx) - v*v) > 0` on the FPU. New helper
`cat_collLosSkipClamped`.

**`collCheckRectLine` 0x00439ef0**: an object without rectangle information is a **miss** (-1.0, 0x00439f08 .. 0x00439f18;
HW1 returned 0.0, a hit); **no early miss** for a line pointing away from the centre (HW1's `pointInWorld . dir > 0`
test is not in the exe); the plane distances `(candidate - origin) / dir` rounded once; the candidate coordinate
`largest * dir + origin` compared unrounded with both bounds. Constants checked: 0x0072a460 = 0.0f, 0x0072a468 = -1.0f.

## 2. turning-coordsys: the attack flight, the bullet hits, the rocks and the gun timing

After 1721, every next divergence was a ship whose **rotation** (coordsys, not in the comparable fields: decision 48)
differed: ulps of the coordsys flip the roundings of the guidance's local-space velocities. Each was localised with the
dump and `hwo_invert.py`, then the exe's machine code of the code path was read; each fix below moved the rung.

**`src/Ships/GenericInterceptor.c`, `GenericInterceptorAttack` 0x005b2620** (10-18 tick 1762: the bRecon 43 in
STATE_KILL, GOG's velocity +1 raw ulp in all three components; an offline replica reproduced ours exactly and showed that
no input of the target velocity reproduces GOG's: the forces' projection, i.e. the coordsys, after the heading torque).
New helper `catInterceptorNormalizeTrajectory`: fsqrt of the double `(z*z + y*y) + x*x`, the distance stored as a float,
`1.0f / that float` kept as a double, one rounding per component (STATE_KILL 0x005b2eb1 .. 0x005b2f04, STATE_WHIPSTRAFE1
0x005b2bed .. 0x005b2c41; the tree's `vecMagnitudeSquared` float sum and `vecDivideByScalar` float reciprocal). Also: the
KILL dot product `(z*vz + y*vy) + x*vx` on the FPU, below 0 or unordered flies without the target's velocity
(0x005b2f08 .. 0x005b2f2e); WHIPSTRAFE1's `bulletRange * 0.7f` and the elapsed time on the FPU; WHIPSTRAFE2's break angle
`randeg * RADIAN_PER_DEG` kept a double into the sine / cosine (0x005b2e09 .. 0x005b2e22); BREAK's squared distance and
heading dot product on the FPU. Not ported: the bombing run (kAttackBomber / tAttackBomber only, not in Cataclysm's
races). `GenericInterceptorCanFire` 0x005b2120: the elapsed time against the fire time on the FPU (0x005b2183); the rest
(the gun-open animation, the gun pick, `cat_gunCanFire` with a third argument 1 that it never reads, the gun type switch)
as the tree. **`src/Game/AIShip.c`, `aishipGetTrajectoryWithAngleCorrection` 0x00413f10**: the future x stays on the FPU
and the x difference is rounded once.

**`collCheckBulletTargetColl` 0x0043cd90** (10-17 tick 2043: GOG keeps a bullet of the bRecon 42 one tick longer; the
GOG hash of 2043 is exactly that bullet moved one step, `fnv(pos + vel/16)` = `8d0c893980ee593e`; ours hit the sWorker
28 a tick early). The exe's sphere test has **no square root**: the line is tested when `vsq - dsqr < traveldist^2`
(0x0043d140 .. 0x0043d15b and 0x0043ceaf .. 0x0043ceca; HW1 `v - sqrt(dsqr) < traveldist`), with `v` over the double
differences (other targets `(dx*hx + dy*hy) + dz*hz`, a missile target `(dz*hz + dy*hy) + dx*hx`), `v*v` stored as a
float, `dsqr = size^2 - (((dz*dz + dy*dy) + dx*dx) - vsq)` on the FPU; the `collOptimizeDist` differences compared
unrounded (the binary search, the break, the stored |difference| against `collspheresize + traveldist` on the FPU).
The same `v*v - dsqr < length^2` test R6 found in `collCheckBeamCollWithTargetsInBlob`.

**The exe's inlined `frandombetween`** (10-18 tick 1796: the bRecon 43, v.z one ulp, after two sMothership bullets hit
it at 1794: the hit's rock torque). The exe loads the stream-10 draw whole (`fild` of the unsigned 32 bits) and computes
`draw * ((hi - lo) * 2^-32) + lo` on the FPU (0x0072a4e0 = 2^-32), the caller rounding where it stores; HW1's macro rounds
the draw, each product and the sum to floats. New `catFrandomBetweenD` in `src/Game/Randy.h`, used where the exe's
shape was read: **`ApplyRockToTarget`** (inlined in `univBulletCollidedWithTarget` 0x00546cf4 .. and
`univMissileCollidedWithTarget` 0x00547166 ..: draw times the mass plus the torque, one rounding), **
`ApplyExplosionRockToTarget`** (inlined in `MakeExplosionRockBumpables` 0x005460c0 ..), **`ApplyCareenRotationDirectly`**
0x00545a80 (each draw times the time step, x and y stored, z added unrounded), **`gunCanShoot`**'s passive-attack
penalty. The hit's **recoil**: `mass / time` stays a double, one rounding per component (0x00546ca5 .., 0x0054711d ..).
The other `frandom` sites of the tree (Crates.c, LevelLoad.c, the careen torque, the scuttle and death times,
GravWellGenerator.c, Gun.c's forced-drop damage, AIShip.c) were not read: listed in "What is left".

**The gun timing** (10-18 tick 1854: GOG has a third bullet and two more `rng10` draws; the third bullet is **the bRecon
43's gun at the sMothership command module 26**, reproduced exactly by a forced-fire experiment `r10p1-e3-ff18`; the
dump's `G` lines show our gun 43.0 in its burst cool-off until 1855). **`src/Game/UnivUpdate.c`, the burst cut of
`cat_univShipDeferredUpdate` 0x005447b0** (0x005449ce .. 0x005449e9): a burst ends when `useBurstFireTime + 0.1 <
elapsed`, with the **double 0.1** (0x0072a7a8) against the unrounded elapsed time: `0.9f + 0.1` stays below 1.0, so a
0.9 s burst ends on its 16th tick; the tree's `0.9f + (real32)0.1` rounds to 1.0f and ended it a tick late (the gun's
cool-off then started at 1812 instead of 1811 and the next burst at 1855 instead of 1854). **`src/Game/Gun.c`,
`gunCanShoot` 0x0048a2f0**: the health fraction stored as a float but first compared unrounded with 0.1f; the fire time
times the double factors 4.0 / 2.5 / 1.75 / 1.25 on the FPU; every elapsed time compared unrounded; the passive
penalty as above.

## 3. unrounded-returns: `aiuFindDistanceSquared` and `tacticsGetShipsMaxVelocity`

**`src/Game/AIUtilities.c` / `.h`, `aiuFindDistanceSquared` 0x0041a2c0** returns the double `(dz*dz + dy*dy) + dx*dx`
of the double differences (the exe's body read again: no store before `ret`). Its **35 call sites** in the exe
(aiaSendRecon, aidCheckSphereOfInfluence x2, aihSlowDefenseDistressHandler, aimProcessHyperspace, aimProcessIntercept x2,
aimProcessMoveTo, aimProcessMoveAttack, aimProcessHarassAttack, aimProcessReinforce, aimProcessSupport,
aimProcessControlResources, aimProcessMothershipMove x2, aitTeamIsFinishedMoving, aitTeamIsInMothershipRange x2,
cat_gunPickMothershipModuleTarget x3, eleven kasf functions and two unnamed ones at 0x00495a25 / 0x00495ad5,
OrientHyperspacingShips, volPointInside, volSphereIntersection) store it (`fstp dword`), compare it unrounded with a
stored float, or hand it to fsqrt as a double; the AIUtilities.c callers are the exe's inlined copies of the same
function. The C callers do the same through the double return (a `real32` assignment rounds once, a compare is made in
double). Read site by site where the C source has arithmetic: **`aitTeamIsFinishedMoving` 0x00417361** compares with
`range * range` computed on the FPU (written out as `(double)range*range`); **`aimProcessMoveTo` 0x00408388** passes the
double to fsqrt and compares the root unrounded (`catFsqrtD`); `aitTeamIsInMothershipRange` 0x00417583 / 0x00417686,
`OrientHyperspacingShips` 0x0051803f, `aimProcessIntercept` 0x004081cd / 0x00408237 and `kasfFindMinesNearTeam`
0x004930fd compare with stored floats (as the C does). No replay or mission stream moved by it.

**`src/Game/Tactics.c` / `.h`, `tacticsGetShipsMaxVelocity` 0x00529450** returns `speedmult * maxvelocity` unrounded
(0x005294b2 `fmul`, `ret`). Its 16 call sites read: the stored speed multiplier times it, one rounding —
`physUpdateObjPosVelShip` 0x004e4a05 (Physics.c, the tag closed), `aishipFlyToPointAvoidingObjsFunc` 0x00414ac9
(AIShip.c), `cat_salvageFlyToTarget` 0x004fa3c0 / `cat_salvageApproachTarget` 0x004fa43c (Salvage.c: the product now
formed once where the tree multiplied the float speed by the multiplier at the call), `GetShipsTravelVel`'s second branch
0x004834f0, the trails (render only, automatic); compared unrounded — `aiuFindSlowestShipMaxSpeed` 0x0041bd1f (stored
when lower), `cat_kamikazeSteer` 0x005b1742 / 0x005b1916 (GenericDefender.c: the ship's product stored, the target's
compared unrounded); stored first — `GetShipsTravelVel`'s protect branch 0x00483455 (Formation.c) and `ACVAttack`
0x005aece5 (ACV.c), both with an explicit `(real32)`; `aitTeamIsInMothershipRange` already took `(double)` of it
(AITeam.c). The words differ only when the formation (0.7) or kamikaze factor is not 1: no replay or mission stream
moved by it.

## 4. blobs-review: the seven Blobs.c bodies

Each compared with the exe's body (decompile and disassembly); **all seven were found modified** (review rows below):

| function | exe | finding | ported |
|---|---|---|---|
| `FillInCollisionBlobSpecificLists` | 0x00425d90 | bullets left out of the target lists; the all-ships, resource and derelict maxima take `collspheresize` (StaticInfo +0x4C), the big / small lists and their maxima the original size (+0x48) | yes |
| `bobListCreate` | 0x00426950 | a dead or blob-satisfied object's `collMyBlob` is cleared (0x00426bd2); the sphere volume `((r*r)*r) * 4.18879f` (0x0072a570) on the FPU, stored once, its reciprocal of the unrounded volume; the sort distance `(x*x + y*y) + z*z` rounded once | yes |
| `bobListUpdate` | 0x00427460 | the sort-distance difference against `biggest + radius` and the stored check radius `(r1 + r2) * sqrtOverlap` on the FPU; the squared distance of the stored differences against `radius^2 * overlap` unrounded; the new radius `((sqrt + r1) + r2) * 0.5f` stored, its unrounded value times the stored one twice times 4.18879f the stored volume; the densities compared on the FPU (this blob's unrounded, the other's stored) | yes |
| `bobBlobCombine` | 0x00426ff0 | the sort distance `(x*x + y*y) + z*z` rounded once; the flags, the joined lists, `1.0f / volume`, the masses, the frees as the tree | yes |
| `bobObjectListMedian` | 0x00426dc0 | the z extent stays on the FPU, the radius (larger extent * 0.5f + margin) rounded once; the min / max loop compares the unrounded edge with the stored extreme (the same floats as the tree's) | yes |
| `AddObjToObjectListsOfBlob` | 0x00427b20 | bullets left out of the target lists | yes |
| `bobObjectDied` | 0x004278e0 | bullets left out of the target lists; the recursion, the shift, the per-type lists, `BTF_RecentDeath`, the return after the find as the tree | yes |

New helpers `catBlobSortDistance`, `catBlobVolumeFactor`. No replay stream moved by them (the replays' streams are
byte-identical before and after).

## 5. Exe bodies compared and found as the tree

`gunShoot` 0x0048b5b0 (the line-of-sight call and its arguments), `aishipGetTrajectoryWithVelPrediction` 0x00413e10,
`aishipGetTrajectory` 0x00413ee0, `vecCapVector` 0x00550950, `vecCapMinVector` 0x005509b0, `vecCapVectorSloppy`
0x005508d0 (per component: not the cause of 1762), `protectShip` 0x004411c0 (the follow-behind point, re-read for
the 2823 localisation), `RangeToTarget` / `RangeToTargetGivenDist`, the start-flying target velocity of
`aishipFlyToPointAvoidingObjsFunc` (0x00414953 .. 0x00414adb), `cat_gunCanFire` 0x0048a1c0 (head).

[audit: M2-R10 oracle-rung-physics: `RangeToTarget` 0x00439c30 and `RangeToTargetGivenDist` 0x00439c80 are NOT as the
tree in their return width: both end `fsub` (`approxcollspheresize`), the clamp to 0, `ret` with no store, so they
return the UNROUNDED difference on the FPU (the third and fourth "unrounded returns"; the tree's `real32` return rounds
it, and R8's "one rounding at the return" comment in Collision.c says so too). `RangeToTarget`'s 25 exe call sites
include unrounded uses (0x0041e487, 0x0044ba56, 0x0044bd97, 0x005aff45); `RangeToTargetGivenDist`'s callers read here
compare it unrounded: `GenericInterceptorAttack`'s WHIPSTRAFE1 test against `bulletRange * 0.7f` (0x005b2c45 .. 0x005b2c6f) and
STATE_KILL's against `bulletRange` (0x005b3001 .. 0x005b3014, which also stores it as a float for the later test at
0x005b317a). Not ported (it would move streams); owner: M3 oracle physics, with the other callers read first. Also
`cat_gunCanFire`'s second-mode threshold (0x0048a2a8 .. 0x0048a2ba: `totaltimeelapsed - firetime2 / n` on the FPU against
the latest fire time) stays unrounded in the exe where the tree computes it in floats; same owner.]

## 6. The permanent instrumentation (HWI-ORACLE, read only)

`src/Game/OracleDump.c`: with `/oracleShipDump`, one **'T'** line per dumped ship (its `aistateattack`, attack target,
the command's attack targets and, for a ship with multiple targets, each gun's target, and `madGunStatus`) and one
**'G'** line per gun (type, burst state, `lasttimefired`, heading in ship space). Every replay with the dump was
byte-identical to the same build's replay without it (`r10p1-d1-d18` vs `r10p1-base-d18`; `r10p1-x1-d18` vs
`r10p1-x1-r18`; `r10p1-x2-d17` vs `r10p1-x2-r17`; `r10p1-x3-d18` vs `r10p1-x3-r18`; `r10p1-x5-d17` vs `r10p1-x5-r17`;
`r10p1-x5-d18` vs `r10p1-x6-r18`). The header of `tools/m2_harness.sh` documents the two lines. The forced-fire
experiments and the bullet-hash search were scratch-only (not in the tree): `original/scratch-m2/m2r10-rung/exp`, the
session's scratchpad `bulinv.py` / `bul3.py` (meet-in-the-middle over the FNV-1a list hash) and `x87.py` (the table fsqrt
and the cap / target-velocity replicas).

## 7. The rung: the replays (headlines first)

Final debug binary `original/scratch-m2/m2r10-rung/final/hwi-engine-debug` (sha256 `8ab246f000b3aa0c`), passed with
`--bin`; `<O>` = `original/oracle`, `<R>` = `original/scratch-m2/m2r2-harness/runs`:

```
tools/m2_harness.sh r10p1g-r17-{a,b} --mode replay --recording oracle01 --recording-src <O>/oracle01-rec-20261017-140428 \
    --recording-as oracle01-r6 --timeout 900                    # r10p1g-r17-libm: the same with "-- /oracleLibmSqrt"
tools/m2_harness.sh r10p1g-r18-{a,b} --mode replay --recording oracle01 --recording-src <O>/oracle01-rec-20261018-102253 \
    --recording-as oracle01-r18 --timeout 900                   # r10p1g-r18-libm: the same with "-- /oracleLibmSqrt"
python3 -I tools/oracle/hwo_compare.py <O>/oracle01-rec-20261017-140428/oracle01-rec.hwo <R>/r10p1g-r17-a/run.hwo \
    --from-tick 1 --to-tick 9600 \
    --fields rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real \
    --json <R>/r10p1g-r17-a/compare.json
  FIRST DIVERGENT TICK 161 (last equal tick 160); 1 field(s) differ, likely origin: rng6      (excluded, decision 47)
... without rng6 (--json <R>/r10p1g-r17-a/compare-sim.json):
  FIRST DIVERGENT TICK 2823 (last equal tick 2822); 1 field(s) differ, likely origin: ships_vel
    ships_vel     REF  ships_vel=4038fc77886a4684   CAND ships_vel=ba2254447d2e8a51
  FIRST DIVERGENT PACKET 46 (REF tick 93, CAND tick 93): blobcheck 1785610478 vs 1785610479   (stock, excluded)
python3 -I tools/oracle/hwo_compare.py <O>/oracle01-rec-20261018-102253/oracle01-rec.hwo <R>/r10p1g-r18-a/run.hwo \
    --from-tick 1 --to-tick 3526 --fields <the same list> --json <R>/r10p1g-r18-a/compare.json
  FIRST DIVERGENT TICK 2 (last equal tick 1); likely origin: real                            (excluded, decision 60)
... without rng6 and real (--json <R>/r10p1g-r18-a/compare-sim.json):
  FIRST DIVERGENT TICK 2871 (last equal tick 2870); 1 field(s) differ, likely origin: ships_vel
    ships_vel     REF  ships_vel=66d45cfe2163a333   CAND ships_vel=24bde02b80327b46
  FIRST DIVERGENT PACKET 136 (REF tick 273, CAND tick 273): blobcheck 1785610477 vs 1785610479   (stock, excluded)
... and each field alone (--fields <one>) for the per-field column
```

| recording, square root | run (pair) | stream | IDENTICAL through | first divergence | per field (first divergent tick; the excluded ones in brackets) |
|---|---|---|---|---|---|
| **10-17**, **the table (default)** | `r10p1g-r17-a` = `-b` | `74db522c2b63cf23`, 2,630,926 B | **tick 2822** | **2823 `ships_vel`, ship 48** | ships_vel 2823, ships_pos 3251, univcheck 3335, bullets 5479, resources 5885, ships_misc 6281, ships_health 7593; **rng10, ships_n, missiles, derelicts, dt, total, real never in 1..9600**; (rng6 161, players 93, ships_rot 1, rng0 / rng_all 2: excluded) |
| 10-17, `/oracleLibmSqrt` | `r10p1g-r17-libm` | `dcdf6b40e943e1c3` | tick 176 | 177 `ships_vel` (the square root) | ships_vel 177, ships_pos 180, univcheck 190, rng10 1408, resources 1408, bullets 1917, ships_health 2043, ships_misc 2321, ships_n 5559 |
| **10-18**, **the table** | `r10p1g-r18-a` = `-b` | `d5cc7d6b526491d6`, 966,649 B | **tick 2870** (`real` excluded) | **2871 `ships_vel`, ship 49** | ships_vel 2871, ships_pos 2909, univcheck 3062; **rng10, ships_n, ships_misc, ships_health, missiles, bullets, derelicts, resources, dt, total never in 1..3526**; (real 2, rng6 161, players 272, ships_rot 1, rng0 / rng_all 3: excluded) |
| 10-18, `/oracleLibmSqrt` | `r10p1g-r18-libm` | `d958d1556eea3646` | tick 117 | 118 `ships_pos` + `ships_vel` | ships_pos / vel 118, univcheck 137, rng10 944, resources 944, bullets 1721, ships_health 1861, ships_n / misc 1955 |

Engine sync counters (the game's own check against the recorded packets): 10-17 `randSyncErr 0 univSyncErr 2911
blobSyncErr 4746 pktSyncErr 0` (R9: 1918 / 3767 / 4746); 10-18 `randSyncErr 0 univSyncErr 4 blobSyncErr 1619 pktSyncErr
0` (R9: 880 / 875 / 1619); with `/oracleLibmSqrt` 3347 / 4693 / 4746 and 781 / 1658 / 1619. Each pair is byte-identical
(`cmp`), ASan 0, no unsuppressed leak, rc 0.

### 7.1 The next divergence (10-17): tick 2823, `ships_vel`, ship 48 (owner: M3 oracle physics, see "What is left")

- First divergent tick 2823 (last equal 2822), the only field there `ships_vel` (`ships_pos` follows at 3251).
- `hwo_invert.py` on `r10p1-x5-d17` (`all@2810-2823`): one solution, ship **48**, a **bProcessor of player 1** (order
  NULL, attributes PROTECTING), GOG `(c33eeb29, 43115032, 40bff944)` vs ours `(c33eeb2c, 43115036, 40bff948)`: raw bits
  -3 / -4 / -4; every other ship equal.
- How: `protectShip` 0x004411c0 flies it to the follow-behind point `P - heading * 1.5 * desiredrange` of the ship it
  guards (flags 0x32, `withVel` = the guarded ship's velocity). An offline replica of the start-flying target velocity
  (scale 0.2, the tweaked VELOCITY_SCALE_FACTOR; the cap 240) reproduces ours exactly; GOG's x and y are reproduced by
  the destination's x one to five ulps lower (or the ship's x higher): **the follow-behind point**, i.e. the guarded
  ship's **heading** (coordsys column 3). The guarded ship is the **bWorker 44**, which turns to dock from 2809 (order
  DOCK, a heading torque of -4404.7 at 2809, then up to the 2.0 rad/s cap): its coordsys is not in the comparable
  fields (decision 48), its position and velocity are equal.
- No command near it (the last packet command before is at 2723).
- Likely owner: the docking bWorker's rotation path (the dock flight's heading tracking for a turning Worker: `aitrackHeadingFunc`
  0x00418de0 and its caller in the dock state machine `cat_dockShipDocksAtShip` 0x00465860, or the rotation integration);
  **what confirms it**: decision 48's recording (`ships_rot` comparable) shows the bWorker's coordsys directly; without
  it, the dock flight's heading vector and the tracker read against the exe at 2809 .. 2822.

### 7.2 The next divergence (10-18): tick 2871, `ships_vel`, ship 49 (owner: M3 oracle physics)

- First divergent tick 2871 (last equal 2870), the only field `ships_vel` (`ships_pos` follows at 2909).
- `hwo_invert.py` on `r10p1-x5-d18` (`all@2860-2871`): one solution, ship **49**, a **bProcessor of player 1**
  protecting, GOG - ours raw `+7 / +3 / +5`; every other ship equal.
- How: the same follow-behind flight (`withVel` 0: the guarded ship is at rest), thrust saturated (375,000 on UP and
  RIGHT). The guarded ship is again the **bWorker 44**, now **harvesting at rest** (order COLLECT_RESOURCES) and spinning
  (from 2860: torques of -9,600 / -3,949.8 / 0.049, the rotation speed at the -2.0 cap, then slowing): its heading moves
  the follow-behind point every tick.
- Likely owner: the harvesting Worker's turn (the Cataclysm harvest path that applies those torques); what confirms it
  as in 7.1.

Both next divergences are the same mechanism: **a bProcessor following a bWorker whose rotation is invisible to the
stream**; decision 48's fourth recording would show it at once.

## 8. The mode-B rung (the user's step; no `original/oracle/hwirec*-play-*` directory exists)

Still the user's session (decision 95; never an agent's). The recordings are unchanged (`hwirec01` `4640546c0708c284` /
`.pkts` `e852ee9f3a457e3b`, `hwirec02` `13a17b9f3a2d8664` / `d24604387e083cf5`, in `r9r-rec01` / `r9r-rec02`'s
`settings/SavedGames/RecordedGames`). **This round's engine plays `hwirec01` exactly as R9's did** (`r10p1g-play01-a` =
`-b` `630bea8811134dbc`, IDENTICAL in every field to `r9i-play01-a`) **but `hwirec02` differently from tick 706**
(`r10p1g-play02-a` = `-b` `5162bf78fca13f62`: ships_rot 706, ships_vel 709, rng10 / ships_pos / ships_health / bullets /
univcheck 831, ships_n / ships_misc 1214, ... against `r9i-play02-a`): the comparison's right-hand side is this round's
playback. The commands, from the main checkout (nothing in `original/` is written by an agent):

```
cd ~/Documents/GameDev/Homeworld-Infection
R=original/scratch-m2/m2r2-harness/runs
cp $R/r9r-rec01/settings/SavedGames/RecordedGames/hwirec01 $R/r9r-rec01/settings/SavedGames/RecordedGames/hwirec01.pkts original/game/SavedGames/RecordedGames/
cp $R/r9r-rec02/settings/SavedGames/RecordedGames/hwirec02 $R/r9r-rec02/settings/SavedGames/RecordedGames/hwirec02.pkts original/game/SavedGames/RecordedGames/
ORACLE_NAME=hwirec01 tools/oracle/record_reference.sh verify     # two GOG playbacks on the virtual clock, rng0 split
ORACLE_NAME=hwirec02 tools/oracle/record_reference.sh verify
python3 -I tools/oracle/hwo_compare.py original/oracle/hwirec01-play-<timestamp>/hwirec01-play-a.hwo \
  original/scratch-m2/m2r2-harness/runs/r10p1g-play01-a/run.hwo --from-tick 1 --to-tick 4800 \
  --fields rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,players,dt,total \
  --json original/oracle/hwirec01-play-<timestamp>/compare-modeB.json
python3 -I tools/oracle/hwo_compare.py original/oracle/hwirec02-play-<timestamp>/hwirec02-play-a.hwo \
  original/scratch-m2/m2r2-harness/runs/r10p1g-play02-a/run.hwo --from-tick 1 --to-tick 4800 \
  --fields <the same list> --json original/oracle/hwirec02-play-<timestamp>/compare-modeB.json
```

(Excluded: `ships_rot` (decision 48), `rng0` (the DLL splits stream 0), `rng_all`, `fpcw`, `real` (decision 60); `rng6`
and `players` stay in, both sides being playbacks. For `hwirec01` R9's `r9i-play01-a` / `r9r-play01-a` remain
equivalent right-hand sides; for `hwirec02` only this round's `r10p1g-play02-a`.)

## 9. gates (final tree, plain harness)

**Builds**: `tools/build.sh --debug` and `--release` from the final tree: rc 0, 0 warnings under `-Werror` (every
incremental build of the round likewise). Final binaries `original/scratch-m2/m2r10-rung/final/hwi-engine-{debug,release}`
(`8ab246f000b3aa0c` / `3c9e013a2b4aaa25`). No struct changed (`tools/m2_layout32.sh` not affected).

Runs (labels `r10p1g-*`, `<R>` as above, every one with `--bin` the final debug binary unless stated; each against R9's
guard-rail stream with `hwo_compare.py` one field at a time, every field, the first divergent tick per field; "IDENTICAL"
means only the header's build identity differs):

| run | mode | rc / wall | milestone | first stop | sanitizers, checks, stream; the move against R9 (fields, first tick) |
|---|---|---|---|---|---|
| `r10p1g-mission-{a,b,perturb}` (`tools/oracle/mission_test.sh --ticks 6000 --perturb 3000 --copy-tick 4000`) | mission, debug | 0 / 25 s | oracle mission run done | none | ASan 0, no leak; a = b **`44f6c81a6bf3f58d`** (1,476,370 B); perturbed run caught, flipped copy at 4000 / ships_health: **MISSION TEST PASS**; **IDENTICAL to R9's `553845b65e87f3b5` over all 6,001 ticks** (no ship flies there, decision 57) |
| `r10p1g-mission-rel-{a,b,perturb}` | mission, release | 0 / 8 s | oracle mission run done | none | a = b `96267d55360e6f4e`, MISSION TEST PASS; IDENTICAL to `r9i-mission-rel-a` and to the debug stream |
| `r10p1g-r17-{a,b}`, `-libm`; `r10p1g-r18-{a,b}`, `-libm` | replay | 0 / 31-58 s | oracle replay done | none | ASan 0, no leak; pairs byte-identical; section 7. Against R9: 10-17 `74db522c2b63cf23` ships_rot 1921, ships_vel 1922, ships_pos 1954, univcheck 2020, rng10 / rng_all / ships_health 2043, bullets 2005, ships_misc 5885, ships_n / players / statcheck 5890; 10-18 `d5cc7d6b526491d6` bullets / rng_all 1721, ships_rot 1743, rng10 / rng0 / ships_pos / ships_vel / ships_health / univcheck 1749, ships_n / ships_misc / players 1753, rng6 1769, statcheck 1805 (the rung); libm 10-17 `dcdf6b40e943e1c3` ships_rot 1921, ships_vel 1923 ...; libm 10-18 `d958d1556eea3646` bullets 1721, ships_rot 1740 ... |
| `r10p1g-play01-{a,b}` | replay `hwirec01` (Armed8) | 0 / 30 s | oracle replay done | none | ASan 0; a = b `630bea8811134dbc`, ticks=4801 packets=2400; **IDENTICAL to `r9i-play01-a`** |
| `r10p1g-play02-{a,b}` | replay `hwirec02` (LiveFireExercise2) | 0 / 28 s | oracle replay done | none | ASan 0; a = b `5162bf78fca13f62`; against `r9i-play02-a`: ships_rot 706, ships_vel 709, rng10 / rng_all / ships_pos / ships_health / bullets / univcheck 831, ships_n / ships_misc / statcheck / players 1214, rng6 1800, resources 2976, derelicts 3840 (the round's combat / flight ports: the skirmish's first fights) |
| `r10p1g-ai-{a,b}` | ai, 400 | 0 / 19 s | ai self-test done | none | ASan 0; checks=16/16 result=PASS; a = b `4250574d4d40fa6b`; **IDENTICAL to `r9i-ai-a`** |
| `r10p1g-ai-soak` | ai, **4,000** | 0 / 33 s | ai self-test done | none | **ASan 0, no leak, no fatal**; `done tick=3998 ... checks=16/16 result=PASS`; game over at tick 1377 (R9: 1389; the test keeps the universe running); `1061c06edbc69903`; against `r9i-ai-soak` `4cd65bfa42cfa6c6`: **rng10 / rng_all / ships_health 431**, ships_pos / ships_vel / univcheck 455, ships_rot 457, bullets 458, missiles 589, rng0 / ships_n / ships_misc 717, rng6 1072, players 1137, statcheck 1430, resources 1984: the first hits (rng10, health together) of the round's combat ports (the bullet-target test, the line of sight, the gun timing, the rocks); `rng_all` is judged with decision 84 |
| `r10p1g-skirmish-lfe2-{a,b}` | skirmish LiveFireExercise2 | 0 / 21 s | skirmish self-test done | none | ASan 0; ticks=600 ships=91 PASS; a = b `97744e77a08306ac`; **IDENTICAL to `r9i-skirmish-lfe2-a`** |
| `r10p1g-skirmish-armed8` | skirmish Armed8 | 0 / 21 s | skirmish self-test done | none | ASan 0; ships=54 PASS; `728bfa5c67376178`; **IDENTICAL to `r9i-skirmish-armed8`** |
| `r10p1g-build-{a,b}` | build | 0 / 29 s | build self-test done | none | ASan 0; done tick=4205 PASS; a = b `f9144d0c347e9863`; **IDENTICAL to `r9i-build-a`** |
| `r10p1g-missiontest-{a,b}` | missiontest (Mission01, NIS-first) | 0 / 68 s | mission self-test done | none | ASan 0; **objectives=6/6 in order** (DestroyBombers 4800, ExploreRegion 8096, DestroyResourcers 8752, DestroyProxAttackers 10240, ...), **done 11424**, PASS; a = b `3775f5b80e300357`; against `r9i-missiontest-a`: rng10 / rng_all / ships_pos / ships_vel / ships_rot / univcheck 1772 (the fleet's arrival), bullets 1782, ships_misc 3564, ships_health 4754, ships_n / players 5643, rng6 5673, rng0 6689 |
| `r10p1g-loadgame-{a,b}` | loadgame "01 - Hiigara", 3,600 | 0 / 28 s | save-load self-test done | none | ASan 0; PASS; a = b `47c9c62a03b997f3`; against `r9i-loadgame-a`: ships_rot 1985, rng10 / rng_all / ships_vel / bullets 1986, ships_pos 1991, univcheck 2029, ships_health 4753, ships_n / ships_misc / players 5062, rng6 5065 |
| `r10p1g-tutorial-a` | missiontest `--tutorial` | 0 / 46 s | mission self-test done | none | ASan 0; `ticks=10444 lessons=12/12 complete=12/12 inOrder=1 nextClicks=11 result=PASS`; `4a165cb9055e3269`; against `r9i-tutorial-a`: ships_rot 4753, bullets 4764, ships_vel 4814, ships_pos 4932, univcheck 4936, rng10 / rng0 / rng_all / ships_health 4966 |
| `r10p1g-warp10-{a,b}` | warpstart, level 10 | 0 / 20 s | warp-start self-test done | none | ASan 0; PASS; a = b `d45b5ee41af801c2`; against `r9i-warp10-a`: **ships_rot 1 only** — the KAS-spun junk (dJunk3Boiler 70 ...: rotation speeds 1-2 ulps apart at tick 1; `ApplyCareenRotationDirectly`'s x87 shape through `kasfTeamMakeCrazy`, checked by dumping both binaries at ticks 0-1 and by an experiment binary with the old `aiuFindDistanceSquared`, which moves the same way) |
| `r10p1g-tload-{a,b}` | loadgame `--training` "LESSON 11 - Researching and Upgrading" from `r10p1g-tutorial-a`'s save, 3,600 | 0 / 24 s | save-load self-test done | none | ASan 0; `from=8361 ... objectives=1/1 result=PASS`; a = b `806aae61866f410a`; against `r9i-tload-a`: ships_pos / vel / rot / rng0 / rng_all 8360, univcheck 8364 (the save is this round's tutorial run's, whose game moved at 4753) |
| `r10p1g-motion-noi`, `r10p1g-nis-noi` | motion / nis `-- /noInterp` | 0 / 41-44 s | motion self-test done | none | ASan 0, PASS; dumps `183681235a3fdc7b` / `13ca70eea998e5de`: **equal to R9's** |
| `r10p1g-fe` | fe, 120 s | 124 / 120 s | **Main_game_screen** | none: running at the timeout | ASan 0, no leak; 0 fescreen UBSan reports |
| `r10p1g-sp` | sp, `--timeout 340` | 124 / 340 s | universe running: last `univUpdateCounter` 800, walked Mission01_NIS01 -> Mission01 -> 02 -> 03 -> Mission04_NIS02 (R9's run reached Mission04's load in its 340 s) | none: running at the timeout | **ASan 0**, no leak; 0 fescreen reports; UBSan kinds R9's (Particle / ETG misaligned members, BTG.c loads, `Select.c:1153/1154`'s `384 - INT_MIN`: producer 2's item) |

**The 17 per-mission self-tests** (`--mission N --mission-test-ticks 90000 --timeout 3600`; Mission01 is `missiontest`
above), each run once, each against R9's stream:

| mission | run | objectives | done tick (R9) | stream | ASan | the move against R9 (fields, first tick) |
|---|---|---|---|---|---|---|
| 01 | `r10p1g-missiontest-{a,b}` | 6/6 | 11424 (11424) | `3775f5b80e300357` (pair) | 0 | as above, from 1772 |
| 02 | `r10p1g-m2-a` | 6/6 | 25936 (25136) | `8b1e79afdead7e09` | 0 | ships_rot 2, ships_vel 5 (the KAS-spun derelict destroyer 59: `ApplyCareenRotationDirectly`'s x87 shape, dumped at ticks 0-6 in both binaries), ships_pos 11065, bullets 11090, univcheck 11113, rng10 / rng_all 11138, missiles 11218, rng0 / ships_health 11462, ships_n / ships_misc / players 11640, rng6 11643, statcheck 18733 |
| 03 | `r10p1g-m3-a` | 5/5 | 17825 (17825) | `128fa364a23116d5` | 0 | **IDENTICAL** |
| 04 | `r10p1g-m4-a` | 5/5 | 20818 (20816) | `3dbafcac0357b8bb` | 0 | ships_rot 8467, ships_vel 8468, ships_pos 8473, univcheck 8492, rng10 / rng_all / bullets 8510, ... (the first fight) |
| 05 | `r10p1g-m5-a` | 6/6 | 16384 (16384) | `71dca0e67fb1271a` | 0 | ships_rot 2518, ships_vel 3394, ships_pos 5033, rng10 / rng_all / ships_health / bullets 5034, ... |
| 06 | `r10p1g-m6-a` | 7/7 | 31011 (30931) | `ef3f7c8577646c1f` | 0 | ships_rot 3169, rng10 / rng_all / bullets 3234, ships_vel 3236, ships_pos 3242, univcheck 3402, ... |
| 07 | `r10p1g-m7-a` | 4/4 | 16592 (16592) | `6e92c9624f422880` | 0 | **IDENTICAL** |
| 08 | `r10p1g-m8-a` | 6/6 | 29361 (29361) | `88f7af92d28857db` | 0 | ships_rot 8809, ships_vel 8810, rng10 / rng_all / ships_pos / ships_health / bullets / univcheck 12558 |
| 09 | `r10p1g-m9-a` | 6/6 | 20617 (20579) | `097cabb36c639945` | 0 | ships_rot 3242, rng10 / rng_all / ships_health / bullets 3262, rng0 3273, ships_vel 3276, ships_pos 3279, ... |
| 10 | `r10p1g-m10-a` | 5/5 | 21939 (21938) | `c110fe40224ec341` | 0 | ships_rot 1 (the spun junk, as the warp start), ships_pos / ships_vel 358, univcheck 359, rng_all 6695, rng10 / bullets 9582, ... |
| 11 | `r10p1g-m11-a` | 5/5 | 8275 (8275) | `ebdeea8a4fd9c9e6` | 0 | ships_rot 2, ships_vel 3, ships_pos 8, univcheck 20, bullets 45, rng10 / rng_all / ships_misc 104, ... |
| 12 | `r10p1g-m12-a` | 3/3 | 10995 (10995) | `0045add242a6bf1e` | 0 | ships_rot 1, ships_vel 4, ships_pos 6365 only |
| 13 | `r10p1g-m13-a` | 3/3 | 19283 (21746) | `ceef39efaac10698` | 0 | ships_rot 2083, rng10 / rng_all / ships_pos / ships_vel / bullets 2105, ships_health 2160, ... |
| 14 | `r10p1g-m14-a` | 4/4 | 22338 (22339) | `9d89db45349f4ced` | 0 | rng10 / rng_all / bullets 10146, ships_rot 10168, ships_vel 10172, ships_pos / univcheck 10178, ... |
| 15 | `r10p1g-m15-a` | 3/3 | 15424 (15424) | `d7de3da02c4a11bf` | 0 | ships_rot 6634, ships_vel 6638, ships_pos 8938, bullets 9134, missiles 9170 only |
| 16 | `r10p1g-m16-a` | 6/6 | 21523 (21522) | `b2e43291adcb9ed6` | 0 | ships_pos / ships_vel 1874, ships_rot / univcheck 1875, bullets 2158, rng10 / rng_all 2235, ... |
| 17 | `r10p1g-m17-a` | 4/4 + the game's end (`game end tick=15584`) | 15584 (15584) | `1941035ad9047ec7` | 0 | ships_rot 2, ships_vel 3, ships_pos 15, bullets 78, rng10 / rng_all 108, univcheck 170, ... |

Every mission passes with every objective in order and ASan 0. The moves are this round's flight and combat ports
(the early `ships_rot` / `ships_vel` ones at ticks 1-5 are the KAS-spun derelicts' `ApplyCareenRotationDirectly`, checked
for Mission02 and the warp start; the later ones begin with the first fight's rotations, hits and bullets). Producer 3's
virtual audio clock will move every mission's `rng_all` once more; the integrate re-baselines all of them.

## Files changed

- `src/Game/Collision.c`: `collCheckLineOfSight` (+ `cat_collLosSkipClamped`), `collCheckRectLine`,
  `collCheckBulletTargetColl`.
- `src/Ships/GenericInterceptor.c`: `GenericInterceptorAttack` (+ `catInterceptorNormalizeTrajectory`),
  `GenericInterceptorCanFire`.
- `src/Game/AIShip.c`: `aishipGetTrajectoryWithAngleCorrection`; the `velMult` store.
- `src/Game/Randy.h`: `catFrandomBetweenD`. `src/Game/UnivUpdate.c`: `ApplyRockToTarget`, `ApplyExplosionRockToTarget`,
  `ApplyCareenRotationDirectly`, the bullet / missile recoil, the burst cut.
- `src/Game/Gun.c`: `gunCanShoot`.
- `src/Game/Tactics.c` / `.h`: `tacticsGetShipsMaxVelocity`'s double; its callers `src/Game/Physics.c`,
  `src/Game/AIShip.c`, `src/Game/AIUtilities.c` (`aiuFindSlowestShipMaxSpeed`), `src/Game/Formation.c`,
  `src/Game/Salvage.c`, `src/Ships/ACV.c`, `src/Ships/GenericDefender.c`.
- `src/Game/AIUtilities.c` / `.h`: `aiuFindDistanceSquared`'s double; `src/Game/AITeam.c`, `src/Game/AIMoves.c`: the two
  written-out sites.
- `src/Game/Blobs.c`: the seven bodies (+ `catBlobSortDistance`, `catBlobVolumeFactor`).
- `src/Game/OracleDump.c`: the 'T' and 'G' dump lines (+ `oracleShipDumpTarget`, `oracleShipDumpAttack`).
- `tools/m2_harness.sh`: header comments only (the 'T' / 'G' lines).
- `ghidra/delta_map_review.json`, `ghidra/delta_map.json` (merged), `docs/m2-r1-function-deltas.md` (regenerated), this
  report.

## Delta-map review entries

27 entries `[review: M2-R10 oracle-rung-physics]` (9 new, 18 existing with the note appended; sample
`port-R10-oracle-rung-physics`), merged with `python3 -I tools/delta_map.py merge --done m2r10:oracle-rung-physics`
("records changed 27") and `python3 -I tools/delta_map.py report`. Counts: IDENTICAL 2,773 -> 2,760, MODIFIED 1,564 ->
1,577, REWRITTEN 83, reviewed 3,512 -> 3,521. **IDENTICAL found modified (now MODIFIED), 13**: collCheckLineOfSight,
collCheckRectLine, aishipGetTrajectoryWithAngleCorrection, ApplyCareenRotationDirectly, aitTeamIsFinishedMoving,
aimProcessMoveTo, FillInCollisionBlobSpecificLists, bobListCreate, bobListUpdate, bobBlobCombine, bobObjectListMedian,
AddObjToObjectListsOfBlob, bobObjectDied. **MODIFIED, the change extended or ported**: collCheckBulletTargetColl,
GenericInterceptorAttack, GenericInterceptorCanFire (compared), univBulletCollidedWithTarget,
univMissileCollidedWithTarget, MakeExplosionRockBumpables, gunCanShoot, tacticsGetShipsMaxVelocity,
physUpdateObjPosVelShip, aishipFlyToPointAvoidingObjsFunc, aiuFindSlowestShipMaxSpeed, GetShipsTravelVel,
aiuFindDistanceSquared, aitTeamIsInMothershipRange (compared). Not in the map (cat-new): `cat_univShipDeferredUpdate`
0x005447b0 (the burst cut), `cat_salvageFlyToTarget` 0x004fa3a0, `cat_salvageApproachTarget` 0x004fa410,
`cat_kamikazeSteer` 0x005b16f0, `ACVAttack` 0x005aec20, `cat_gunCanFire` 0x0048a1c0.

## CAT-TODOs

**Closed**: both `CAT-TODO(M2-R10 physics)` (`aiuFindDistanceSquared` in AIUtilities.c, the top-speed product in
Physics.c): `/bin/grep -rn 'CAT-TODO(M2-R10 physics' src/` is empty; no other tag names the physics owner. None added.

## For STATE.md

- **The shooter of 10-18 tick 1721**: the sMothership 24's guns 3 and 6 at the bRecon 41 (the line of sight skips the
  source's clamped modules).
- **The rung's reference numbers** for the next round: **2823** (10-17, ship 48 `ships_vel`) and **2871** (10-18, ship 49
  `ships_vel`) on the table; 177 / 118 with `/oracleLibmSqrt`.
- **Defaults chosen**: the exe's inlined `frandombetween` shape is ported only where its disassembly was read (the
  rocks, the careen rotation, the passive penalty); the other `frandom` sites stay HW1's macro until read. The x87 helpers
  are named `catFrandomBetweenD` (Randy.h), `catInterceptorNormalizeTrajectory` (GenericInterceptor.c),
  `cat_collLosSkipClamped` (Collision.c), `catBlobSortDistance` / `catBlobVolumeFactor` (Blobs.c); the dump's new 'T' /
  'G' lines (`oracleShipDumpTarget`, `oracleShipDumpAttack`).
- **Moved streams to re-baseline** (the integrate): see section 9's table (the replays, the soak, the missiontest, the
  loadgame, the tutorial leg, the lesson load, the warp start's `ships_rot`, `hwirec02`'s playback, missions 02-17 as
  stated); unchanged: the headless Mission01 stream (debug and release), ai 400, both skirmishes, build, the motion / nis
  dumps, `hwirec01`'s playback.

## What is left (owners)

1. **The rung** (M3 oracle physics, or an M2-R11 physics round if one is opened): 10-17 tick 2823 (7.1) and 10-18 tick
   2871 (7.2), both a bProcessor following a turning bWorker; decision 48's recording (the user's) would show the
   Worker's coordsys directly.
2. **The rest of the inlined `frandom` / `frandombetween` sites** (M3 oracle physics): Crates.c, LevelLoad.c (the
   asteroid fields and the derelicts' placement), `ApplyCareen`'s torques, the scuttle / player-lose death times
   (CommandLayer.c, UnivUpdate.c), GravWellGenerator.c, Gun.c's forced-drop damage, AIShip.c, UnivUpdate.c's `frandom(1)`
   short-circuit test; each to be read against its exe site before porting (`catFrandomBetweenD` is the helper).
3. **`GenericInterceptorAttack`'s bombing run** (kAttackBomber / tAttackBomber): not ported (not in Cataclysm's races;
   M3 if those types ever fly).
4. **The mode-B rung** (the user's session, section 8) and **decision 48's fourth recording** (the user's).
5. [audit: M2-R10 oracle-rung-physics] **The unrounded returns of `RangeToTarget` 0x00439c30 / `RangeToTargetGivenDist`
   0x00439c80 and `cat_gunCanFire`'s second-mode threshold** (M3 oracle physics; section 5's audit note): read their
   callers against the exe before porting, each a stream move.
