<!-- done: fsqrt-default, avoid-blobs, collision-dock, hyperspace-protect, gates -->
# M2-R9 oracle-rung-physics: the table square root as the default, then the oracle's next rung

Status: DONE (M2-R9 producer 1; base commit `54dcbb2`; nothing committed by the producer, the auditor checkpoints).
Decision 23 / 56 / 70 this round: **"table"** (the user's ruling of 2026-10-19, "keep the game relying on fsqrt"): the
exe's table `fsqrt` 0x00472860 is the engine default and the HWI-EXPERIMENT switch is inverted to `/oracleLibmSqrt`.

Subsystems: **fsqrt-default** (the table square root as the default, every stream's move recorded once), **avoid-blobs**
(the avoidance's collision-size order, the exe's qsort, the collision blobs by collision position), **collision-dock**
(`ObjectsCollided`, the big / small ships' clamped-host test, `cat_dockFlyOutOfCone`, the fixed-gun aim test),
**hyperspace-protect** (`hyperspaceCost`'s double return and its callers, `protectShip` / `protectShipsAvg`), **gates**.

## Headline

**The rung moved from tick 247 to tick 1922 (10-17) and from tick 155 to tick 1721 (10-18), on the default (the
table).** The 10-17 replay is IDENTICAL to the GOG stream in every simulation field of the list through **tick 1921**
and first diverges at **1922 `ships_vel`** (ship 41, a bRecon of player 1 attacking, v.z +1 ulp [audit: in raw bits, GOG - ours; GOG's v.z `c11179e3` is one ulp MORE NEGATIVE than ours `c11179e2`, the "-1 ulp" of section 6.1; confirmed with hwo_invert on the audit's dump `r9a2-d17`]); the 10-18 replay is
IDENTICAL through **tick 1720** and first diverges at **1721 `bullets`** (GOG fires two bullets, ours none: a firing
decision, R8's per-field `bullets` tick of this recording). Ship 40's velocity at 247 is GOG's `(420767c8, 4300eec6,
429fe606)` and ship 28's v.z at 155 is GOG's `4234afe1` (both IDENTICAL ticks now). **With `/oracleLibmSqrt`** the first
divergences stay at **177** (10-17) and **118** (10-18), R8's `sqrtf` ticks: the square root itself. Both replay pairs
are byte-identical with ASan 0; the 4,000-tick ai run ends with ASan 0, no fatal, `result=PASS`. The fixes were made in
this order, each one the exe's machine code read at the tick the dump and `hwo_invert.py` pointed at:

| step | change (exe function) | 10-17 first divergence | 10-18 first divergence |
|---|---|---|---|
| R8 / move 1 | the table `fsqrt` as the default (`r9p1fs-*`) | 247 (ship 40) | 155 (ship 28) |
| x1 | the avoidance takes the AVOIDED object's collision size first (`aishipFlyToPointAvoidingObjsFunc` 0x00414610) | 239 (ship 40) | 212 (ships 29, 30) |
| x4 | + the exe's qsort (CRT 0x00671060) | 239 | 154 |
| x5 | + ships in the collision blobs by their collision position (`bobUpdateObjsInBlobCollInfo` 0x00426050 and 3 more) | 592 (ships 34, 35) | 424 (mothership 24 + modules) |
| x7 | + `ObjectsCollided` 0x005456d0's x87 shape | 856 (ship 49) | 424 |
| x8 | + `collCheckBigShipSmallShipColl` 0x0043b460's clamped-host test and distance shape | 856 | 1044 |
| x9 | + `cat_dockFlyOutOfCone` 0x004650d0's x87 shape | **1922** (ship 41) | **1721** (bullets) |
| final | + the fixed-gun aim test, the blob attacking flag, `protectShip` / `protectShipsAvg`, `hyperspaceCost`'s double (no rung move) | **1922** | **1721** |

## 1. fsqrt-default: the table square root as the engine default

- `src/Game/FastMath.c`: `catFsqrtTable = TRUE` (was FALSE); the purpose header says the table is the default since
  M2-R9 and `/oracleLibmSqrt` is the A/B switch (HWI-EXPERIMENT).
- `src/Game/FastMath.h`: the comments of `fsqrt` / `fmathSqrt` / `catFsqrtD` (the macros are unchanged: they take the
  table while `catFsqrtTable` is set, `sqrtf` otherwise).
- `src/SDL/main.c`: the switch table entry `/oracleLibmSqrt` (sets `catFsqrtTable` FALSE) replaces `/oracleTableFsqrt`.
- `tools/m2_harness.sh`: the header documents `-- /oracleLibmSqrt` (any mode) and that `/oracleTableFsqrt` is gone.

**Every stream moved once by this step** (binaries `original/scratch-m2/m2r9-rung/fs/`, runs `r9p1fs-*`, each against
R8's guard-rail stream with `hwo_compare.py`, every field, first divergent tick per field):

| stream | R8 (guard-rail) | after the table (`r9p1fs-*`) | fields that moved (first tick) |
|---|---|---|---|
| Mission01, 6,000 ticks, debug | `0d2b76d565105a26` | `493a747570184c19` (a = b, MISSION TEST PASS) | **none: IDENTICAL over 6,001 ticks** (header build identity only) |
| Mission01, release | `3dc687dbb95feab6` | `034032f149478b9a` (a = b) | none |
| replay 10-17 | `d88fd3c4a6e2de54` (R8, switch on) | `ded29523d86218f6` (a = b) | none: IDENTICAL to R8's switch-on replay |
| replay 10-18 | `98985af637e37bf3` (on) | `d90dfa585b7937ca` (a = b) | none |
| replay 10-17 / 10-18 `/oracleLibmSqrt` | `6b3e3c15a42de5af` / `9c51469ece54c88f` (off) | `f1760be433eac492` / `4fae96cf8a41d06f` | none: IDENTICAL to R8's sqrtf replays |
| ai, 400 | `df95f21bdb82abbf` | `b00b051832212870` (a = b, 16/16) | ships_pos 17, ships_vel 17, ships_rot 17, univcheck 20 |
| ai soak, 4,000 | `a2b04b8998bc6818` | `bf4e05fd17d7e72a` (PASS, ASan 0) | ships_pos/vel/rot 17, univcheck 20, bullets 413, missiles 477, rng10 481, ... |
| skirmish LiveFireExercise2 | `1b648d098a7659c5` | `0f822ced8198bfc0` (a = b) | ships_pos/vel/rot 321, univcheck 326 |
| skirmish Armed8 | `898f68a5fc352ba2` | `462a2414ccffa6e5` | ships_vel/rot 322, ships_pos 323, univcheck 357 |
| build | `b7851d51d1c21bfb` | `a42bd09a30579a75` (a = b) | ships_vel 4206, ships_pos 4212, ships_rot 4236, univcheck 4250 |
| missiontest (Mission01 NIS-first) | `becb49b579aba435` | `9beedb21de3440d5` (a = b, 6/6) | ships_vel 2, ships_pos / rot / univcheck 1772, bullets 1795, rng10 2055, ... |
| loadgame "01 - Hiigara" | `68b0075ad1e20bb7` | `7cf62355b5591578` (a = b) | ships_pos/vel/rot 1985, bullets 1985, univcheck 1987, rng10 2206, ... |
| motion dump `/noInterp` | `0838a011db819920` | `5e2ad2ba1015f940` (`r9p1-e5-mo-fs`, PASS) | the camera pose from frame 0 (the focused ship's flight) |
| nis dump `/noInterp` | `d1e6f6e477906b52` | `dcec053b0dba1f88` (`r9p1fs-nis-noi`, PASS) | the NIS objects' screen positions from frame 0 |

The headless Mission01 stream does not move with the table (R8's measurement holds); the motion and nis guard-rail dumps
move because their scenes fly ships (the square root under the flight's vectors), not because of motion interpolation.

## 2. avoid-blobs: the avoidance's order of collision sizes, the exe's qsort, the blobs by collision position

**`src/Game/AIShip.c`, `aishipFlyToPointAvoidingObjsFunc` 0x00414610** (the 10-18 tick-155 fix). In the collision
distance the exe calls `GetCollSizeInDirection` first with the AVOIDED object (0x0041558b `push esi`, esi = the blob's
object, loaded at 0x00414de3 and used as the avoided object at 0x004156a3 and 0x00415721) and stores its result as a
float (0x004155c5), then calls it with the ship (0x004155d7 `push edi`, edi = the ship: its position at 0x004156c9) and
adds that double unrounded (0x004155e0). R8 had the two the other way round. Also the too-close special case compares
fsqrt's unrounded double with 0.1f (0x00415554 `fst`, 0x0041555b `fcomp`). With the dump's new 'O' lines: ship 28
avoids the sMothership 24 and its sMothershipEnginesUpper 25 (sizes 574 / 539: their float rounding moved 155).

**`src/Game/CatSort.c` / `.h` (new): `catQsort`, the exe's CRT qsort 0x00671060** (median to `lo`, the two-pointer
partition with `comp <= 0` / `comp >= 0`, the smaller side first, the stack of 30, `shortsort` 0x006712a0 for at most 8
elements: the last maximum to `hi`, the byte `swap` 0x00671310). Every sorting function of the exe calls it, so every
call site the exe has goes through it (each with a `CAT` comment and the call address): `Blobs.c`
(`bobUpdateObjsInBlobCollInfo`, `bobUpdateExtraCollBobInfo`), `Formation.c` (`formationArrangeOptimum` 0x00481200),
`NIS.c` (`nisLoad` 0x004d4a00: object paths and events), `Animatic.c` (`animLoadNISScript` 0x00421610), `BTG.c`
(`btgZip` 0x00428a40), `Select.c` (`selSelectionCompare` 0x00508fb0), `ScenPick.c` / `GamePick.c` (the title lists),
`Sensors.c` (`cat_smSensorRangeBlobsDraw` 0x00509be0), `SDL/texreg.c` (`trRegistryRefresh` 0x00587a50). Not changed:
`Memory.c` (debug statistics, not in the exe) and `selShipsInSelection` (not in the exe, uncalled). A comparator that ties
(equal keys, or HW1's blob comparator 0x00425d60, which never returns 0) now gives the exe's order.

**`src/Game/Blobs.c`: a ship is placed in the collision blobs by its collision position** (the 10-17 tick-239 and
10-18 tick-212 fixes). The exe branches on objtype 0 and reads `collInfo.collPosition` (+0xE0) for a ship, the position
(+0x58) for anything else, in `bobFindNearestBlobToObject` 0x00425cb0, `bobUpdateObjsInBlobCollInfo` 0x00426050,
`bobUpdateExtraCollBobInfo` 0x00426160 and `bobAddObjToSpecificBlob` 0x00427e90 (helpers `catBlobObjPosition`,
`catBlobDistSqD`); the differences stay doubles and `(dz*dz + dy*dy) + dx*dx` goes to fsqrt as a double;
`bobFindNearestBlobToObject` compares that double with the stored float minimum. Evidence: at 10-18 tick 212 ships 29
and 30 match GOG only when the avoidance adds the module 26 between 24 and 25 (every order tested offline with the
dump's 'O' words: only `(24, 26, 25)` and `(25, 26, 24)` reproduce both ships' GOG velocities); a mothership's modules
share its position (so they tied in our blob sort) but not its collision position. With the C library's qsort the
rung reaches the same ticks (experiment `r9p1-e4-*`): after this fix the blob sort has no ties left in the replays.
The same two functions' blob "action flags" were read too: the exe sets `BTF_Attacking` only when a ship's
`shipisattacking` (+0x358) AND `catAttackStarted` (+0x616) are set (`bobUpdateObjsInBlobCollInfo` 0x004260a4 ..
0x004260b8, `bobBlobItemize` 0x00427371 .. 0x00427381; the tree tested the first only): ported in both.

## 3. collision-dock: the collision response, the big / small clamp test, the launch cone, the fixed-gun aim

**`src/Game/UnivUpdate.c`, `ObjectsCollided` 0x005456d0** (the 10-17 tick-592 fix: sWorkers 34 / 35 bump, each 1 ulp
off). The x87 shape with `ObjectsHaveCollidedLately` and `ShouldDoGlancingCollision` inlined as the exe has them: the
repulsion `((colldist - dist) * factor) / dist` one double; the elapsed times doubles; obj1's speed squared `(vx*vx +
vy*vy) + vz*vz` a double compared with obj2's stored float, the faster one's stored, the slower one's kept as it is (a
double for obj1, the float for obj2); the x momenta `m*v` doubles, their difference times the unrounded double `1/t` rounded once [audit: was "the stored `1/t`": 0x00545813 `fst` stores the float but 0x00545817 multiplies st(0); the same words while t is 1/16 s; the code now takes the double],
y and z stored after each product and the difference; the speed ratio and the dot product doubles with the exe's
unordered outcomes (0x0054585e `test ah,1`: two ships at rest, `0 / 0`, are not glancing: R8's tree had this right, the
first draft of the port did not and was corrected at 10-18 tick 423, where GOG's mothership group stays at rest:
`hyp.py` with velocity 0 reproduces GOG's hash); the tangential magnitude fsqrt of the double `(z*z + y*y) + x*x`; the
left / right test `dx*vy - dy*vx` over the unrounded collision-position differences.

**`src/Game/Collision.c`, `collCheckBigShipSmallShipColl` 0x0043b460** (the 10-18 tick-424 fix: GOG's mothership 24 and
its building module sMothershipMicro 47 do not collide). The clamped-host test of 0x0043b578 .. 0x0043b5a7 (a ship with
`SOF_Clamped` whose clamp info's host is the other ship does not collide with it, either way round, after the moving
test, as R8 ported in `collCheckShipShipColl`) and the distance test's shape (0x0043b83f: the squared distance a double
stored as the float `distsquared`, compared unrounded with `colldist` squared before any root; the root of the stored
float after the too-close case and the bump tests).

**`src/Game/Dock.c`, `cat_dockFlyOutOfCone` 0x004650d0** (the 10-17 tick-856 fix: the launched sWorker 49, GOG's
position z +1 ulp and v.z +4 ulps). `flyawaydist * 4` stays a double; the target's x and y products are stored then
added to the origin, z's product is added unrounded; the snapped position uses `t = ((dz*nz + dy*ny) + dx*nx) / ((nz*nz
+ ny*ny) + nx*nx)` as a double with the unrounded dz; the distance test compares the double `(dz*dz + dy*dy) + dx*dx` of
the stored differences with `flyawaydist` squared. The dump's new 'V fly' line showed the flight's destination
(`dest.z` constant) and `withVel`, which located the target's z.

**`src/Game/Gun.c`, `cat_gunFixedGunAimedAtTarget` 0x0048cd40** (read for the 10-18 tick-1721 firing difference; no
rung move): the distance fsqrt of `(z*z + y*y) + x*x` as a double; `1/dist` a double that normalises x and y (stored)
and z unrounded; the cosine `((inv*z)*h.z + h.y*y) + h.x*x` a double capped at 1; the sine fsqrt of `1 - cos*cos`;
`sin * dist` compared as a double with the target's collision sphere * 0.75f.

## 4. hyperspace-protect: `hyperspaceCost`'s double, `protectShip` / `protectShipsAvg`

- **`hyperspaceCost` 0x00443c30 returns the double** (`CommandLayer.h`, `CommandLayer.c`): the `CAT-TODO(M2-R9 physics)`
  of the R8 audit is closed. Its callers as the exe has them: `clMpHyperspace` 0x004474a0 (the double differences, the
  double square into fsqrt, the stored distance, the cost truncated, 0x0044750a), `aimProcessHyperspace` 0x00407dd0
  (`AIMoves.c`: `aiuFindDistanceSquared`'s double into fsqrt, ported locally; `cost` a double truncated at 0x00407ea9),
  `cat_aisTeamAttack` 0x00432df0 (`AISkirmish.c`: `cost + 10000.0` with the unrounded double, 0x00433170),
  `pieDistanceReadoutDraw` 0x004e61b0 (`PiePlate.c`: truncated, 0x004e6240), the main region's hyperspace move at
  0x00564c1c (`SDL/mainrgn.c`: the double square into fsqrt, truncated).
- **`protectShip` 0x004411c0** (`CommandLayer.c`): `desiredrange = (range + the larger size) + the ship's size`, one
  double rounded at its store (0x0044121b .. 0x00441236; the tree had `(range + ship) + larger` in floats); the
  processors' follow-behind point `point - heading * followbehind` with the double `followbehind`, one rounding per
  component. **`protectShipsAvg` 0x004413f0**: `1 / numShips` a double (fild, fdivr), one rounding per component of the
  average velocity; `range + size` a double compared unrounded with the distance.
- **Found, not ported** (no divergence points at it yet; tagged `CAT-TODO(M2-R10 physics)` in `AIUtilities.c`):
  `aiuFindDistanceSquared` 0x0041a2c0 returns the unrounded double `(dz*dz + dy*dy) + dx*dx` on the FPU to about 70
  callers; the tree's `real32` rounds the float sums.

Compared with the exe and found as the tree (R8's ports re-verified for the rung's suspects): `aitrackHeadingFunc`
0x00418de0 (yaw, pitch, roll, every torque compare), `aitrackHeadingWithBankPitchFunc` 0x00419200 in full (the squares,
the reciprocals, the pitch, fptan, the normalisation), `aitrackVelocityVectorGuidance` 0x00419460,
`matMultiplyMatByMat` 0x004ad1a0 (the stack traced through every fxch), `matMakeRotAboutX / Y / Z`, `GetCollSizeInDirection`
0x00413f90, `physUpdateObjPosVelShip` 0x004e4950's rotation path (alpha, the cap, fsin / fcos, the three products; an
experiment with the x87 `fsin` / `fcos` instructions, `r9p1-e1-r17`, gave a byte-identical replay, so libm stays),
`bobAddObjToNearestBlob` 0x00427f10, `AddSpaceObjToSelectionPreserveOrder` 0x00427aa0, the blob comparator 0x00425d60,
the start-flying target velocity of `aishipFlyToPointAvoidingObjsFunc` (0x00414953 .. 0x00414adb), the avoidance's
obscured-point and pass-on-right sections (0x00415682 .. 0x004158d1).

## 5. The permanent instrumentation (HWI-ORACLE, read only)

`src/Game/OracleDump.c` / `.h`, `src/Game/AIShip.c`; documented in `tools/m2_harness.sh`'s header:
- **'O' lines** (`oracleShipDumpAvoid`): every object a dumped ship's flight avoids, after the distance test: its objtype,
  ship number and type, collision position and coordsys, the distance (the stored float), the normal, and the three
  doubles of the collision distance (the avoided object's size, the ship's size, `dtc`) as raw 64-bit words;
- **'V fly' lines** (`oracleShipDumpFly`): the flight's destination, `AISHIP_` flags, `limitvel` and `withVel`;
- **'B' lines** (with `all`): every live bullet in BulletList order with its firing ship and target.
Every replay with the dump was byte-identical to the same build's replay without it (`cmp`: `r9p1-x1-d17`,
`r9p1-x2-d17`, `r9p1-x3-d18`, `r9p1-x6-d18`, `r9p1-x8-d17`, `r9p1-x9-d17`, `r9p1-x10-d18`, ...). The offline replicas
of the avoidance term, the guidance and the force sum were small scripts in the session's scratchpad (not tracked); they
reproduced the engine's words exactly before being used to test hypotheses.

## 6. rung: the replays (headlines first)

Command lines (`<O>` = `original/oracle`, `<R>` = `original/scratch-m2/m2r2-harness/runs`, final debug binary
`original/scratch-m2/m2r9-rung/final/hwi-engine-debug` (sha256 `17a123bbb7d85851`), passed with `--bin`):

```
tools/m2_harness.sh r9p1f-r17-{a,b} --mode replay --recording oracle01 --recording-src <O>/oracle01-rec-20261017-140428 \
    --recording-as oracle01-r6 --timeout 900                       # r9p1f-r17-libm: the same with "-- /oracleLibmSqrt"
tools/m2_harness.sh r9p1f-r18-{a,b} --mode replay --recording oracle01 --recording-src <O>/oracle01-rec-20261018-102253 \
    --recording-as oracle01-r18 --timeout 900                      # r9p1f-r18-libm: the same with "-- /oracleLibmSqrt"
python3 -I tools/oracle/hwo_compare.py <O>/oracle01-rec-20261017-140428/oracle01-rec.hwo <R>/r9p1f-r17-a/run.hwo \
    --from-tick 1 --to-tick 9600 \
    --fields rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real \
    --json <R>/r9p1f-r17-a/compare.json
  FIRST DIVERGENT TICK 161 (last equal tick 160); 1 field(s) differ, likely origin: rng6        (excluded, decision 47)
  FIRST DIVERGENT PACKET 46 (REF tick 93, CAND tick 93): blobcheck 1785610478 vs 1785610479   (stock, excluded)
... without rng6 (--json <R>/r9p1f-r17-a/compare-sim.json):
  FIRST DIVERGENT TICK 1922 (last equal tick 1921); 1 field(s) differ, likely origin: ships_vel
    ships_vel     REF  ships_vel=954e484bd2242935   CAND ships_vel=4bedc8b2adec4be8
python3 -I tools/oracle/hwo_compare.py <O>/oracle01-rec-20261018-102253/oracle01-rec.hwo <R>/r9p1f-r18-a/run.hwo \
    --from-tick 1 --to-tick 3526 --fields <the same list> --json <R>/r9p1f-r18-a/compare.json
  FIRST DIVERGENT TICK 2 (last equal tick 1); 1 field(s) differ, likely origin: real           (excluded, decision 60)
... without rng6 and real (--json <R>/r9p1f-r18-a/compare-sim.json):
  FIRST DIVERGENT TICK 1721 (last equal tick 1720); 1 field(s) differ, likely origin: bullets
    bullets       REF  bullets_n=2 bullets_h=1ff62819fb2ff28a   CAND bullets_n=0 bullets_h=cbf29ce484222325
  FIRST DIVERGENT PACKET 136 (REF tick 273, CAND tick 273): blobcheck 1785610477 vs 1785610479
... and each field alone (--fields <one>) for the table below; the libm runs the same way
```

| recording, square root | run (pair) | stream | IDENTICAL through | first divergence | per field (first divergent tick; none = equal over the range) |
|---|---|---|---|---|---|
| **10-17**, **the table (default)** | `r9p1f-r17-a` = `-b` | `c27c314ed8c95b7f`, 2,630,929 B | **tick 1921** | **1922 `ships_vel`, ship 41** | ships_vel 1922, ships_pos 1954, bullets 2005, univcheck 2020, rng10 2043, ships_health 2043, ships_misc 5885, resources 5885, ships_n 5890; missiles, derelicts, dt, total, real none in 1..9600; (rng6 161, players 93, ships_rot 1: excluded) |
| 10-17, `/oracleLibmSqrt` | `r9p1f-r17-libm` | `d3bdc1a5c525415f` | tick 176 | 177 `ships_vel` (R8's sqrtf tick) | ships_vel 177, ships_pos 180, univcheck 190, rng10 1408, resources 1408, bullets 1917, ships_health 2043, ships_misc 2321, ships_n 5559 |
| **10-18**, **the table** | `r9p1f-r18-a` = `-b` | `0aa88c67961516de`, 966,653 B | **tick 1720** (`real` excluded) | **1721 `bullets`** | bullets 1721, rng10 1749, ships_pos 1749, ships_vel 1749, ships_health 1749, univcheck 1749, ships_n 1753, ships_misc 1753; resources, missiles, derelicts, dt, total none in 1..3526; (real 2, rng6 161, players 272, ships_rot 1: excluded); first divergent packet 136 (tick 273, blobcheck: the stock `players` difference of tick 272) |
| 10-18, `/oracleLibmSqrt` | `r9p1f-r18-libm` | `a67d3048dbeb47f0` | tick 117 | 118 `ships_pos` + `ships_vel` (R8's) | ships_pos 118, ships_vel 118, univcheck 137, rng10 944, resources 944, bullets 1721, ships_health 1749, ships_misc 1753, ships_n 1753; first divergent packet 68 (tick 137) |

Engine sync counters: 10-17 `randSyncErr 1918 univSyncErr 3767 blobSyncErr 4746 pktSyncErr 0` (first failing frames
rand 2059, **univ 2059** (R8: 331), blob 109); 10-18 `880 / 875 / 1619 / 0` (first frames rand 1765, **univ 1765** (R8:
233), blob 289). Each pair is byte-identical (`cmp`), ASan 0, no leak, rc 0 (50-58 s / 31-32 s).

**Exclusions** as decided: `players` at 93 / 272 and `rng6` at 161 (decision 47), `ships_rot` (decision 48: no
recording with `CAT_SHIP_coordsys` 0x98 exists; the DLL hashes Ship+0x80, i.e. two pointers and a list node before the
coordsys, so its words cannot be inverted either), `rng_all`, `fpcw`; for 10-18 also `real` (decision 60).

### 6.1 the next divergence (10-17): tick 1922, `ships_vel`, ship 41 (owner: M2-R10 physics)

- First divergent tick 1922 (last equal 1921); the only field there is `ships_vel` (`ships_pos` follows at 1954).
- `hwo_invert.py` on an `all@1905-1923` dump (`r9p1-x9-d17`) gives one solution: ship **41**, a bRecon of player 1 with
  an attack order (order 2), GOG `(c20658b4, c458d6a4, c11179e3)` vs ours `(..., c11179e2)`: v.z -1 ulp; every other ship
  equal.
- How: it flies (no avoidance) to its attack point with `AISHIP_FastAsPossible` and `limitvel` -1000 (the 'V fly'
  line); UP and RIGHT thrusts computed, FORWARD saturated (2400); it turns (rotation speed -0.39 rad/s about x).
- The offline guidance replica reproduces our velocity exactly; GOG's is reproduced by desired.y +1 ulp, or by single
  ulps of the coordsys elements m11, m21, m31, m32 or m33 (11 single-element solutions): the attack point / target
  vector or the coordsys of the turning ship. No command near it.
- Likely owner: the attack flight's destination (`GenericInterceptor` / `aishipFlyToShipAvoidingObjs`' attack point, its
  target lead) or the rotation of a turning ship; **what confirms it**: a recording with `CAT_SHIP_coordsys` 0x98
  (decision 48) shows the coordsys; otherwise the attack-point functions read against the exe at this tick.

### 6.2 the next divergence (10-18): tick 1721, `bullets` (owner: M2-R10 physics / combat)

- First divergent tick 1721 (last equal 1720): GOG has 2 bullets from 1721 (4 at 1726, 6 at 1731: two every 5 ticks),
  ours none until 1740 (the dump's new 'B' lines: our first bullet is ship 41's at its target, the sRecon 31, at 1740;
  31 and 32 fire back at 1742). R8's per-field list had `bullets` 1721 for this recording with either square root.
- The three bRecons of player 1 with attack orders (41, 42, 43) close on player 0's sRecons; a gun fires in GOG 19
  ticks before ours: a firing decision (`GenericInterceptorCanFire` 0x005b2120: the gun-open animation, the fire time,
  `cat_gunCanFire`, the fixed-gun aim `cat_gunFixedGunAimedAtTarget` 0x0048cd40, ported in x87 shape this round without
  moving the tick) or the attack AI's choice of when to call it.
- **What confirms it**: invert the GOG bullet hash at 1721 with the 'B' line format (positions only) against the gun
  positions of 41 / 42 / 43 and of player 0's ships, to name the shooter, then read its attack path against the exe.

## 7. gates (final tree, plain harness)

**Builds**: `tools/build.sh --debug --clean` and `--release --clean` from the final tree: rc 0, `[254/254]`, 0
warnings ("Build results" below); every incremental debug and release build of the round: rc 0, 0 warnings under
`-Werror`. (One release build early in the round ran from the main checkout's directory by a shell `cd` mistake: it
rebuilt the gitignored `build/release` of the main checkout incrementally from that checkout's own sources; no tracked
file there was touched, and every build used in this report is the worktree's.) Final binaries
`original/scratch-m2/m2r9-rung/final/hwi-engine-{debug,release}` (`17a123bbb7d85851` / `77180dfb3e01177b`).

| run | mode | rc / wall | milestone | first stop | sanitizers, checks, stream |
|---|---|---|---|---|---|
| `r9p1f-mission-a`, `-b`, `-perturb` (`mission_test.sh --ticks 6000 --perturb 3000 --copy-tick 4000`) | mission, debug | 0 / 28 s | oracle mission run done | none | ASan 0, no leak; a = b, **`1028375d20f15d75`**; perturbed run caught at 3000 / rng10, flipped copy at 4000 / ships_health: **MISSION TEST PASS** |
| `r9p1f-mission-rel-a`, `-b` | mission, release | 0 / 6 s | oracle mission run done | none | a = b, `e06354cba52dca2b`, MISSION TEST PASS |
| `r9p1f-r17-{a,b}`, `-libm`; `r9p1f-r18-{a,b}`, `-libm` | replay | 0 / 31-58 s | oracle replay done | none | ASan 0, no leak; pairs byte-identical (section 6) |
| `r9p1f-ai-a`, `-b` | ai, 400 | 0 / 21 s | ai self-test done | none | ASan 0; checks=16/16 result=PASS; a = b `124358d63c5cb0f9` |
| `r9p1f-ai-soak` | ai, **4,000** | 0 / 33 s | ai self-test done | none | **ASan 0, no leak, no fatal**; game over at tick 1389 (the test keeps the universe running), `done tick=3998 ... checks=16/16 result=PASS`; `f2036d7b92e10f37` |
| `r9p1f-skirmish-lfe2-a`, `-b` | skirmish LiveFireExercise2 | 0 / 19-21 s | skirmish self-test done | none | ASan 0; ticks=600 ships=89 PASS; a = b `2eb7b79363150f2d` |
| `r9p1f-skirmish-armed8` | skirmish Armed8 | 0 / 21 s | skirmish self-test done | none | ASan 0; ships=54 PASS; `462a2414ccffa6e5` |
| `r9p1f-build-a`, `-b` | build | 0 / 29 s | build self-test done | none | ASan 0; PASS; a = b `b3dc0cf689bf6cef` |
| `r9p1f-missiontest-a`, `-b` | missiontest | 0 / 61-72 s | mission self-test done | none | ASan 0; **objectives=6/6 in order** (DestroyBombers 4800, ExploreRegion 8096, DestroyResourcers 8752, DestroyProxAttackers 10240, DestroyDestroyer 11296, Hyperspace jump 11392), done 11424, PASS; a = b `929138a2d6baac97` |
| `r9p1f-loadgame-a`, `-b` | loadgame "01 - Hiigara", 3,600 | 0 / 25-28 s | save-load self-test done | none | ASan 0; PASS; a = b `dd6bcab1d2af2a6b` |
| `r9p1f-fe` | fe, 120 s | 124 / 120 s | **Main_game_screen** | none: running at the timeout | ASan 0, no leak; 0 fescreen UBSan reports |
| `r9p1f-sp` | sp, `--timeout 340` | 124 / 340 s | universe running: last `univUpdateCounter` **4,496** (R8: 4,496; gate 1,248), Mission01.mission loaded | none: running at the timeout | **ASan 0**, no leak; 0 fescreen reports; UBSan kinds R8's (Particle / ETG misaligned members, BTG.c loads) [audit: not only: a NEW kind, `Select.c:1153` / `1154` signed integer overflow `384 - -2147483648` in `selCircleCompute` (from `rndShipVisibleUsingCoordSys`), 2 reports, in this run and in the audit's `r9a2-sp` and `r9a2-sp-libm` (so not the square root); R8's `r8i-sp` has none; non-fatal, ASan 0; owner M2-R10] |
| `r9p1f-motion-noi`, `r9p1f-nis-noi` | motion / nis `-- /noInterp` | 0 / 44, 48 s | motion self-test done | none | ASan 0, PASS; dumps `183681235a3fdc7b` / `13ca70eea998e5de` (moved: section 8) |

(The final set was run twice: the first time six runs were stopped from outside at about 23:57 (the engine shut down
normally, no summary written) and were rerun; then the blob attacking-flag port landed and the whole set was run again
from the final binaries, all runs completing: the rows above. Against the set before that port only the ai soak's
stream changed.)

## 8. Every stream before and after (against R8's guard-rail streams)

**The headless Mission01 stream** (`0d2b76d565105a26` debug, release `3dc687dbb95feab6` content): after the table
(`r9p1fs-mission-a`, `493a747570184c19`) IDENTICAL over all 6,001 ticks (header build identity only); **after the round
(`r9p1f-mission-a`, debug `1028375d20f15d75`, release `e06354cba52dca2b`, IDENTICAL content): `ships_pos` and
`univcheck` from tick 0, `ships_rot` from tick 1; every other field (rng10, rng6, rng0, rng_all, ships_n, ships_misc,
ships_vel, ships_health, missiles, bullets, derelicts, resources, statcheck, players, dt, total, real, fpcw) IDENTICAL
over 6,001 ticks.** The cause is the exe's qsort alone: the experiment binary with the C library's qsort and every other
change of the round (`r9p1-e4-mission`) is IDENTICAL to `r9p1fs-mission-a` in every field. At tick 1 the
kStandardDestroyers 46 / 47 exchange positions and the six tHeavyDefenders 74 .. 79 take other slots
(`r9p1-e3-mfs` vs `r9p1-e3-mfin`, `/oracleShipDump all@0-2`): the level's formations of equal-size ships are arranged by
`formationArrangeOptimum` 0x00481200, whose sort ties go the exe's quicksort way now (the C library's qsort orders
them otherwise). The physics ports do not touch this stream (Mission01's ships do not fly under their own AI here,
decision 57).

| stream | R8 | after the round (`r9p1f-*`) | fields that moved against R8 (first tick); cause |
|---|---|---|---|
| replay 10-17 (table) | `d88fd3c4a6e2de54` | `c27c314ed8c95b7f` (a = b) | ships_rot 241, ships_vel 247, ships_pos 259, ...: the rung (section 6) |
| replay 10-18 (table) | `98985af637e37bf3` | `0aa88c67961516de` (a = b) | ships_vel 155, ships_rot 172, ships_pos 179, ...: the rung |
| replay 10-17 / 10-18 libm | `6b3e3c15a42de5af` / `9c51469ece54c88f` | `d3bdc1a5c525415f` / `a67d3048dbeb47f0` | later fields only (the first divergences stay 177 / 118) |
| ai, 400 | `df95f21bdb82abbf` | `124358d63c5cb0f9` (a = b, 16/16) | ships_pos 0, univcheck 0, ships_rot 1 (the start's formations: qsort), ships_vel 17 (the table) |
| ai soak, 4,000 | `a2b04b8998bc6818` | `f2036d7b92e10f37` | ships_pos 0, ships_rot 1, ships_vel 17, bullets 413, rng10 471, ... (game over 1389, R8 1101); the only stream the blob attacking-flag port moved |
| skirmish LFE2 | `1b648d098a7659c5` | `2eb7b79363150f2d` (a = b) | ships_pos / vel / rot 321, univcheck 326 (the table: 321; this round's other changes: 323 against the table-only stream) |
| skirmish Armed8 | `898f68a5fc352ba2` | `462a2414ccffa6e5` | ships_vel / rot 322, ships_pos 323, univcheck 357: the table only (unchanged by the ports) |
| build | `b7851d51d1c21bfb` | `b3dc0cf689bf6cef` (a = b) | ships_pos 0, univcheck 0, ships_rot 1 (qsort), ships_vel 4206 (the table), rng_all 4319 |
| missiontest | `becb49b579aba435` | `929138a2d6baac97` (a = b, 6/6) | ships_pos / misc / rot / health / resources 0, ships_vel 2, rng6 5, univcheck 39, bullets 1783, rng10 1785, ...: the table moved ships_vel from 2 (move 1); the flight ports alone (the C-library-qsort experiment `r9p1-e6-mt`) move ships_pos / vel / rot and univcheck from 1772 (the fleet's arrival after the opening NIS), bullets 1783, rng10 1785; the exe's qsort moves the tick-0 fields (the opening NIS's sorted object paths and events, the start's formations) |
| loadgame | `68b0075ad1e20bb7` | `dd6bcab1d2af2a6b` (a = b) | ships_pos / vel / rot 1985, bullets 1985, univcheck 1987, rng_all 2036, rng10 2206, ... (the table from 1985; this round's other changes from 1985 too) |
| motion `/noInterp` | `0838a011db819920` | `183681235a3fdc7b` | the table: `5e2ad2ba1015f940` (camera pose from frame 0: the focused ship's flight); the qsort: the start's formations |
| nis `/noInterp` | `d1e6f6e477906b52` | `13ca70eea998e5de` | the table: `dcec053b0dba1f88` (NIS objects' screen positions); the qsort (the start's formations, the NIS's sorted events / paths) |

Decision 46's guard-rails are not touched (no motion-interpolation code changed); the two dumps moved with the stated
physics (decision 70's table square root, the exe's qsort) and need re-baselining by the integrate:
`183681235a3fdc7b` / `13ca70eea998e5de`.

## Build results

From the final tree: `tools/build.sh --debug --clean` and `tools/build.sh --release --clean`: rc 0, `[254/254] Linking
target hwi-engine`, **0 warnings, 0 errors** under `-Werror` (logs `original/scratch-m2/m2r9-rung/build-{debug,release}-clean.log`);
debug 81,701,952 B sha256 `17a123bbb7d85851`, release 4,528,080 B `77180dfb3e01177b`: both byte-identical to the
binaries the final runs used (`original/scratch-m2/m2r9-rung/final/`). `tools/m2_layout32.sh`: OK (Ship 1596, Player
25904, Universe 252348; 45 specifics files); `python3 -I tools/m2_save32_gen.py --check`: current (no struct changed).

## Files changed

- `src/Game/FastMath.c`, `src/Game/FastMath.h`, `src/SDL/main.c`, `tools/m2_harness.sh`: the table as the default,
  `/oracleLibmSqrt`; the harness header documents the switch and the new dump lines.
- `src/Game/CatSort.c`, `src/Game/CatSort.h` (new), `src/Game/meson.build`: `catQsort`.
- `src/Game/Blobs.c` (also the blob attacking flag), `Formation.c`, `NIS.c`, `Animatic.c`, `BTG.c`, `Select.c`, `ScenPick.c`, `GamePick.c`, `Sensors.c`,
  `src/SDL/texreg.c`: the exe's qsort; `Blobs.c` also the collision positions and the x87 distances.
- `src/Game/AIShip.c`: the avoidance's size order, the 0.1 compare, the dump hooks.
- `src/Game/UnivUpdate.c`: `ObjectsCollided`. `src/Game/Collision.c`: `collCheckBigShipSmallShipColl`.
- `src/Game/Dock.c`: `cat_dockFlyOutOfCone`. `src/Game/Gun.c`: `cat_gunFixedGunAimedAtTarget`.
- `src/Game/CommandLayer.c` / `.h`: `hyperspaceCost`, `clMpHyperspace`, `protectShip`, `protectShipsAvg`;
  `src/Game/AIMoves.c`, `AISkirmish.c`, `PiePlate.c`, `src/SDL/mainrgn.c`: the callers of `hyperspaceCost`.
- `src/Game/AIUtilities.c`: the `CAT-TODO(M2-R10 physics)` of `aiuFindDistanceSquared`.
- `src/Game/OracleDump.c` / `.h`: the 'O', 'V fly' and 'B' dump lines.
- `ghidra/delta_map_review.json`, `ghidra/delta_map.json` (merged), `docs/m2-r1-function-deltas.md` (regenerated), this
  report.

## Delta-map review entries

34 entries `[review: M2-R9 oracle-rung-physics]` in `ghidra/delta_map_review.json` (6 new: `bobUpdateObjsInBlobCollInfo`,
`bobUpdateExtraCollBobInfo`, `bobAddObjToSpecificBlob`, `bobFindNearestBlobToObject`, `bobAddObjToNearestBlob`,
`AddSpaceObjToSelectionPreserveOrder`; 28 existing with the note appended; sample `port-R9-oracle-rung-physics`), merged
into the round base's map (`git show 54dcbb2:ghidra/delta_map.json`) with `python3 -I tools/delta_map.py merge --done
m2r9:oracle-rung-physics` ("records changed 34") and
`python3 -I tools/delta_map.py report`. Counts: IDENTICAL 2,773 -> 2,762, MODIFIED 1,564 -> 1,575, REWRITTEN 83,
reviewed 3,340 -> 3,346. **IDENTICAL found modified (now MODIFIED), 11**: bobUpdateObjsInBlobCollInfo,
bobUpdateExtraCollBobInfo, bobAddObjToSpecificBlob, bobFindNearestBlobToObject, ObjectsCollided (R4 had compared it
"identical": the x87 shape differs), animLoadNISScript, btgZip, formationArrangeOptimum, gpTitleListLoad,
selSelectionCompare (the exe's qsort), aiuFindDistanceSquared (the double return, not ported). **Compared, identical**:
bobAddObjToNearestBlob, AddSpaceObjToSelectionPreserveOrder, matMakeRotAboutX / Y / Z; re-verified as R8 ported them:
aitrackHeadingFunc, aitrackHeadingWithBankPitchFunc, aitrackVelocityVectorGuidance, matMultiplyMatByMat,
GetCollSizeInDirection, physUpdateObjPosVelShip. **MODIFIED, the change extended**: bobBlobItemize (the attacking flag), aishipFlyToPointAvoidingObjsFunc,
collCheckBigShipSmallShipColl, protectShip, protectShipsAvg, hyperspaceCost, aimProcessHyperspace, clMpHyperspace,
pieDistanceReadoutDraw, nisLoad, spTitleListLoad, trRegistryRefresh. Not in the map (cat-new or library):
`cat_dockFlyOutOfCone` 0x004650d0, `cat_gunFixedGunAimedAtTarget` 0x0048cd40, `cat_aisTeamAttack` 0x00432df0,
`cat_smSensorRangeBlobsDraw` 0x00509be0, the CRT qsort 0x00671060 / shortsort 0x006712a0 / swap 0x00671310, the blob
comparator 0x00425d60.

## CAT-TODOs

**Closed**: `CAT-TODO(M2-R9 physics)` (`hyperspaceCost`'s return, CommandLayer.c): `grep -rn 'CAT-TODO(M2-R9 physics'
src/` is empty. **Added**: `CAT-TODO(M2-R10 physics)` at `aiuFindDistanceSquared` (AIUtilities.c), a finding with a
reason (the exe's double return to ~70 callers; no divergence points at it yet). The tags by owner after the round:
stage 5 9, stage 5 LAN 5, stage 5 multiplayer 1, stage 5 multiplayer options 7, stage 5 recorder 4, M2-R9 part B 2
(the missions producers'), M2-R10 physics 1.

## For STATE.md

- **Decision 70 applied**: the table `fsqrt` is the default (`catFsqrtTable` TRUE); `/oracleLibmSqrt` (HWI-EXPERIMENT,
  inside `#if HWI_ORACLE` as the old switch was) gives libm's `sqrtf` for an A/B run; `/oracleTableFsqrt` is gone.
- **The exe's qsort for every sort the exe makes** (`catQsort`, CatSort.c): ties in a sort now come out as in the exe;
  default kept: `Memory.c`'s debug sort and the uncalled `selShipsInSelection` use the C library's.
- **Chosen names**: `catQsort`, `catQsortShort`, `catQsortSwap` (CatSort.c), `catBlobObjPosition`, `catBlobDistSqD`
  (Blobs.c), `oracleShipDumpAvoid`, `oracleShipDumpFly` (OracleDump.c), the dump's 'O', 'V fly', 'B' lines.
- **Moved streams to re-baseline** (the integrate): Mission01 `1028375d20f15d75` (debug) / `e06354cba52dca2b` (release):
  ships_pos / univcheck from tick 0, ships_rot from 1, against `0d2b76d565105a26` (the exe's qsort in the level's
  formations; the table alone moves nothing); replays `c27c314ed8c95b7f` (10-17) / `0aa88c67961516de` (10-18); ai
  `124358d63c5cb0f9` (soak `f2036d7b92e10f37`); skirmish `2eb7b79363150f2d` / `462a2414ccffa6e5`; build `b3dc0cf689bf6cef`; missiontest
  `929138a2d6baac97`; loadgame `dd6bcab1d2af2a6b`; motion / nis `/noInterp` dumps `183681235a3fdc7b` / `13ca70eea998e5de`.
- **The rung's reference numbers** for the next round: 1922 (10-17, ship 41 v.z) and 1721 (10-18, bullets) on the
  table; 177 / 118 with `/oracleLibmSqrt`.

## What is left (owners)

1. **The rung** (M2-R10 physics): 10-17 tick 1922, ship 41 (6.1); 10-18 tick 1721, the first bullets (6.2). A recording
   with the DLL's `CAT_SHIP_coordsys` 0x98 (decision 48, the user's) would show the coordsys of the turning ships.
2. **`aiuFindDistanceSquared`'s double return** (M2-R10 physics; the tag in AIUtilities.c).
3. **The rest of `Blobs.c`** (M2-R10 physics): `bobListCreate`, `bobListUpdate`, `bobBlobCombine`,
   `bobObjectListMedian`, `FillInCollisionBlobSpecificLists`, `AddObjToObjectListsOfBlob`, `bobObjectDied` are still
   IDENTICAL and unreviewed; the four ported here showed the Cataclysm change (collision positions) the R1 map missed.
4. **The motion / nis guard-rail dumps and the stream baselines** (the integrate re-baselines them, section 8).
