<!-- done: dump, x87-helpers, rung, soak-faults, collision, gates -->
# M2-R8 oracle-rung-physics: the oracle's next rung, then the physics / combat leftovers

Status: DONE (M2-R8 producer 1; base commit `145352a`; nothing committed by the producer, the auditor checkpoints).
Decision 23 / 56 this round: **"sqrtf"** (the default square root stays `sqrtf`; the exe's table `fsqrt` 0x00472860 stays
behind the HWI-EXPERIMENT switch `/oracleTableFsqrt`); the rung is judged with the switch on and off.

Subsystems: **dump** (the permanent `/oracleShipDump` switch and `tools/oracle/hwo_invert.py`), **x87-helpers** (the
exe's x87 shape of the vector / matrix helpers and of the whole flight path: Vector.c, Matrix.c, AITrack.c, AIShip.c,
Physics.c, Formation.c, GenericInterceptor.c, CommandLayer.c, Collision.c, UnivUpdate.c), **rung** (both recordings
replayed twice with the switch on and off, the next divergences localised, two further simulation fixes the
localisation pointed at: the collision distance shape / clamped-host test and the killer-asteroid wrap), **soak-faults**
(the R7 ai soak's ETG and bullet-owner faults, plus the two the soak met after them), **collision**
(`collCheckShipShipColl`'s exe-only tests), **gates**.

## Headline

**The rung moved from tick 177 to tick 247** (10-17 recording, switch on): the replay of `oracle01` (10-17) is
IDENTICAL to the GOG stream `oracle01-rec.hwo` in every simulation field of the list **through tick 246**; the first
divergence is **tick 247, `ships_vel`, ship 40** (a bRecon of player 1 guarding the bMothership), by (-2, +1, -2) ulps;
the tick-177 velocity of ship 35 is now exactly GOG's `(c20420c2, c110b4d6, 41d3f9da)`. The **10-18 recording** (switch
on) is IDENTICAL through **tick 154** (was 117); first divergence **tick 155 `ships_vel`, ship 28** (an sWorker, its
v.z -1 ulp). **With the switch off** (`sqrtf`, the default) the first divergences stay at 177 (10-17) and 118 (10-18),
but most later fields moved out (10-17: `ships_n` 2048 -> 5559, `ships_misc` 871 -> 1593, `ships_health` 1934 -> 2043,
`resources` 243 -> 1408; [audit: not every one: `rng10` stays at 1408 and `bullets` at 1917, R7's ticks]): the table `fsqrt` is what the user must rule on (decision 23 / 56; the rung cannot pass 177 without it).
Both replay pairs are byte-identical with ASan 0. **The Mission01 stream moved in `ships_rot` only** (from tick 1;
every other field IDENTICAL over 6,001 ticks to `9f7416bb64fec98f`): new hash **`245b3d88387ed076`** (debug) /
`e0c18fcf00229e0f` (release, IDENTICAL content). The **4,000-tick ai soak** runs to the end with ASan 0, no fatal and
`result=PASS` (the R7 soak died at tick ~1,300).

## 1. dump: `/oracleShipDump` and `tools/oracle/hwo_invert.py`

`src/Game/OracleDump.c` / `.h`, `src/SDL/main.c`, `src/Game/CommandNetwork.c`, `src/Game/Physics.c`,
`src/Game/AIShip.c` (HWI-ORACLE, instrumentation only):

- **`/oracleShipDump <ships>[@<from>-<to>]`**: `<ships>` is a ship number (`shipID.shipNumber`, the id the stream
  hashes), a comma-separated list (up to 32) or `all`; the optional tick range bounds the output. It writes
  `shipdump.txt` next to the `/oracleDump` file (in the harness: `<run>/shipdump.txt`). Lines:
  - `S <tick> <ship> <type> <owner> <order> <attributes> <aistate> <flags> pos= vel= rotspeed= coordsys= health=`: one
    per dumped ship per universe tick, after the tick (where the 'T' record is written), in ShipList order, every
    float as `bits:value` (the exact words the stream hashes; the coordsys in hash order m11 m21 m31 m12 ...);
  - `A <tick> <ship> force|rot <dir> <value>`: every force / torque given to a dumped ship (`physApplyForceToObj` /
    `physApplyRotToObj`, after the sign of the direction);
  - `F <tick> <ship> force= torque=`: the sums the physics update integrates (`physUpdateObjPosVelShip`'s entry);
  - `V <tick> <ship> target|repulse|desired v=`: the flight's capped target velocity, each avoidance term and the
    velocity handed to the guidance (`aishipFlyToPointAvoidingObjsFunc`);
  - `R <tick> <index> <objtype> pos= vel=`: with `all`, every live resource in ResourceList order (the `resources`
    hash's order);
  - `C <tick> <frame> <index> <type> <from> <bytes>`: every command of every sync packet as `clProcessSyncPacket`
    parses it (its raw bytes).
  It draws nothing and changes nothing: a replay with `/oracleShipDump 35@170-180` (`r8f2-r17-on-a`) is byte-identical
  to the same build's replay without it (sha256 `b040b22074ca9c3b`), `r8a-dump-replay17` (`all@1-400`, base tree plus
  the dump) to the base replay `r8a-base-replay17`, and `r8a-res17` (`all@238-250`) to its build's replay. Documented in the
  harness header (`tools/m2_harness.sh`, the replay mode) and in `main.c`'s switch table.
- **`tools/oracle/hwo_invert.py`** (new, standard-library Python): recovers the words of one or two ships (or ship
  groups sharing their words, `N+M+K`, e.g. a mothership and its modules) or one resource (`--field res`) from a GOG
  tick hash, by FNV-1a's byte-wise inverse and a meet in the middle over +-N ulps of our dump's words. It reproduced
  R7's inversion of tick 177 exactly and found every divergence localised below. (Two- and three-unit searches with
  wide ranges were also run with small C helpers in the scratchpad, not tracked.)

## 2. x87-helpers: the exe's x87 shape

The exe runs its float code on the x87 at the 53-bit precision of its control word (0x027f in every 'T' record of the
GOG streams): float operands are loaded as they are, products and sums are doubles, a value is rounded to float only
where it is stored (`fstp dword`), and the order of the sums is the compiler's. The tree computed in float, which
rounds every product and sum. Each function below was read in the disassembly (`tools/m2_fdis.py`) and ported with the
exe's order of the sums, its doubles and its stores, under a `CAT (M2-R8 oracle-rung-physics)` comment with the
addresses. The square root of a double argument is `catFsqrtD` (FastMath.h): with `/oracleTableFsqrt` the exe's table
on the double (the low dword of the argument is kept by the table, so the double's last bits matter), by default
(decision 23 "sqrtf") `sqrtf` of the argument rounded to float, widened. The old `fsqrt` / `fmathSqrt` macros are
unchanged.

| file | function (exe) | the exe's shape now in the tree |
|---|---|---|
| Vector.c | `vecNormalize` 0x00550770, `vecCopyAndNormalize` 0x00550820, `vecNormalizeToLength` 0x00550880 | `(x*x + y*y) + z*z` double into fsqrt, `fdivr 1.0` (or the length) a double, one rounding per component |
| Vector.c | `vecCapVector` 0x00550950, `vecCapMinVector` 0x005509b0, `vecCapMinMaxVector` 0x00550a10 | the double magnitude compared with the float limit, the double ratio |
| Vector.c | `vecHomogenize` 0x005507d0, `vecLineIntersectWith{XY,YZ,XZ}Plane` 0x00550b10/50/90, `vecLineIntersectWithPlane` 0x00550bd0, `vecVectorsBlend` 0x00550c40 | the reciprocal / parameter / 1 - factor doubles, both dot products in the exe's order, one rounding per component (no assertion in the exe) |
| Vector.c | `cat_vecDistance` 0x00550d60, `cat_vecPlaneNormalFrom3Points` 0x00550c90 | double differences, `(dz*dz + dy*dy) + dx*dx`; the normal: c.x-b.x and c.y-b.y stored as floats, the double nz in the length |
| Matrix.c | `matMultiplyMatByMat` 0x004ad1a0 (the exe's hand-written loop), `matMultiplyMatByVec` 0x004ad260, `matMultiplyVecByMat` 0x004ad2f0, `matCreateCoordSysFromHeading` 0x004ad010 | each element a double sum in the exe's order (`P(i,0) = (A2*B20 + A0*B00) + A1*B10`, ...; `x = (p0+p1)+p2`, `y,z = p0+(p1+p2)`), rounded once; the cross products rounded once per component |
| AITrack.c | `aitrackHeadingFunc` 0x00418de0 | the heading dot product a double; each desired rotation speed a double; the torque `((desired - w) * I) * (1/t)` a double compared with the strength and rounded once (`catAitrackTurn`) |
| AITrack.c | `aitrackHeadingWithBankPitchFunc` 0x00419200 | the des. square stored float, the cur. square double, 1/|cur| stored float, 1/|des| double, the dot product with the unrounded des.y, the pitch a double; `fptan`'s extended result (`tanl`) stored once as des.z and multiplied **unrounded** by that float (one 53-bit rounding: `fma` over the exact split of the long double), + 1 into fsqrt |
| AITrack.c | `aitrackVelocityVectorGuidance` 0x00419460 | the error vector and its square double; each thrust (difference * mass/t) a double compared with the strength, rounded once (`catAitrackThrust`) |
| Physics.c | `physApplyForceToObj` 0x004e43f0 | x and y products stored before the add, z's product added to force.z with one rounding |
| Physics.c | `physUpdateObjPosVelShip` 0x004e4950 | a.x = (F/m)*t on the FPU; pos.x += t*v.x with one rounding; **w.x += (T/I)*t with one rounding** (the rotation); the angles t*w doubles into sin / cos (rounded once to float, as the exe's fsin / fcos at their fstp); the drift cap in double |
| AIShip.c | `GetCollSizeInDirection` 0x00413f90 (+ `catGetCollSizeD`) | three double components `(z+y)+x`, `(heading*F + right*R) + up*U`, * 0.5, the double returned |
| AIShip.c | `aishipGetTrajectoryWithVelPrediction` 0x00413e10 | the double square into fsqrt over the bullet speed; z's product added with one rounding |
| AIShip.c | `aishipFlyToPointAvoidingObjsFunc` 0x00414610 (the avoidance, 0x00414d28 .. 0x004158d1) | `maxpossibledist` and the formation / parade pads rounded once; the squared distance and maxdist squared double; fsqrt of the double stored float; the normal by `1.0f/dist` a double; the second collision size added as a double; `1/(dtc*scale)` a double; the obscured-point size, test and normal in double; the pass-on-right dot stored float, the target square double, pass-right z added with one rounding |
| GenericInterceptor.c | `GenericInterceptorPassiveAttack` 0x005b2470 | the trajectory's double square into fsqrt, the distance stored float, its double reciprocal |
| Formation.c | `formationWingmanTrackLeader` 0x00482890 | separation `(size+size)*1.5` rounded once; desired x `(right.x-heading.x)*sep + leader.x` rounded once, y/z step by step |
| Formation.c | `catParadeSlotPosition` (`setMilitaryParade` 0x00485790, `processMilitaryParadeToDo` 0x00485280) | position + up*upoffset, then + right*rightoffset, then + heading*headingoffset, each step rounded once; the spacing x `heading*hs + (up*us + right*rs)` once, y/z step by step (the R6 helper had the exe's order but float precision) |
| CommandLayer.c | `CalculateMoveToPoints` 0x00445bf0, `hyperspaceCost` 0x00443c30 | the move distance double into fsqrt (stored float), squared distances double vs the stored float maximum, ratio and scale doubles; hyperspaceCost's distance, costs and sum doubles across the ships [audit: the exe returns that double sum on the FPU and its callers `_ftol` it (0x00407ea9, 0x0044750a, 0x00564c1c) or add it as a double (0x00433170); the tree's `real32` return rounds it to float first: not yet the exe's shape, `CAT-TODO(M2-R9 physics)` in CommandLayer.c] |
| Collision.c | `RangeToTarget` 0x00439c30 | `(x*x + y*y) + z*z` into fsqrt, minus the sphere size on the FPU, rounded once |
| UnivUpdate.c | `univFindNearestResource` 0x00549c50 | differences and squared distance double against the stored float minimum |

Compared and left as they are (no arithmetic or only exact compares): `vecCapVectorSloppy`, `getVectDistSloppy`,
`matMakeRotAboutX/Y/Z`, `physApplyRotToObj`, `aishipGetTrajectory`, `RangeToTargetGivenDist`, `MoveLeftToGo` (z returned
as the exact double difference, rounded by the caller: the same float), `MoveReachedDestinationVariable` (the exact
double difference in the compares: the same results). `fsin` / `fcos`: an experiment with the real x87 instructions
(`__asm__("fsin")` on a long double) in place of libm gave the same replay stream; libm's `sin` / `cos` of the double
angle stay (portable to the Windows build). `fptan` is `tanl` (the x87 extended format where `long double` is the x87's).

## 3. rung: the replays (headlines first)

Command lines (`<R>` = `original/scratch-m2/m2r2-harness/runs`, `<O>` = `original/oracle`; final debug binary
`original/scratch-m2/m2r8-rung/final/hwi-engine-debug`, passed with `--bin`):

```
tools/m2_harness.sh r8f-r17-on-{a,b} --mode replay --recording oracle01 --recording-src <O>/oracle01-rec-20261017-140428 \
    --recording-as oracle01-r6 --timeout 900 -- /oracleTableFsqrt          # r8f-r17-off-{a,b}: without the switch
tools/m2_harness.sh r8f-r18-on-{a,b} --mode replay --recording oracle01 --timeout 900 -- /oracleTableFsqrt   # r8f-r18-off-{a,b}
python3 -I tools/oracle/hwo_compare.py <O>/oracle01-rec-20261017-140428/oracle01-rec.hwo <R>/r8f-r17-on-a/run.hwo \
    --from-tick 1 --to-tick 9600 \
    --fields rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real \
    --json <R>/r8f-r17-on-a/compare.json
python3 -I tools/oracle/hwo_compare.py <O>/oracle01-rec-20261018-102253/oracle01-rec.hwo <R>/r8f-r18-on-a/run.hwo \
    --from-tick 1 --to-tick 3526 --fields <the same list> --json <R>/r8f-r18-on-a/compare.json
... and each again without the excluded fields (rng6; for 10-18 also real) -> compare-sim.json; each field alone (--fields <one>)
```

| run (pair) | mode | rc / wall | milestone | sanitizers, stream | GOG comparison (simulation fields; exclusions below) |
|---|---|---|---|---|---|
| `r8f-r17-on-a`, `-b` | replay 10-17, **switch on** | 0 / 56 s, 55 s | oracle replay done (`ticks=9601 packets=4800 ships=80 randSyncErr=3406 univSyncErr=4611 blobSyncErr=4746 pktSyncErr=0`) | ASan 0, no leak; **byte-identical**, 2,630,929 B, sha256 `90ec0345fef510e5` | **IDENTICAL through tick 246; first divergence 247 `ships_vel`** (REF `83380d15912e50a0`, CAND `f959f314d5ba4d97`). Per field: ships_vel 247, ships_pos 259, univcheck 315, ships_misc 1593, resources 1944, bullets 2005, rng10 2043, ships_health 2043, ships_n 5559; missiles, derelicts, dt, total, real none; (rng6 161, players 93, ships_rot 1: excluded) |
| `r8f-r17-off-a`, `-b` | replay 10-17, switch off (`sqrtf`) | 0 / 52 s, 53 s | oracle replay done (`randSyncErr=3587 univSyncErr=4693 blobSyncErr=4746 pktSyncErr=0`) | ASan 0; byte-identical, sha256 `8be77c5d7d8032de` | IDENTICAL through 176; first divergence 177 `ships_vel` (ship 35, as R7). Per field: ships_vel 177, ships_pos 180, univcheck 190, rng10 1408, resources 1408, ships_misc 1593, bullets 1917, ships_health 2043, ships_n 5559 (R7: 177, 180, 180, 1408, 243, 871, 1917, 1934, 2048) |
| `r8f-r18-on-a`, `-b` | replay 10-18, **switch on** | 0 / 33 s, 32 s | oracle replay done (`ticks=3527 packets=1763 ships=57 randSyncErr=888 univSyncErr=1621 blobSyncErr=1619 pktSyncErr=0`) | ASan 0; byte-identical, 966,646 B, sha256 `0ad45d1587d6caf9` | **IDENTICAL through tick 154 (`real` excluded); first divergence 155 `ships_vel`** (REF `be9dd23a2cdceb2a`, CAND `cfa0591c536ab701`). Per field: ships_vel 155, ships_pos 179, univcheck 216, bullets 1721, rng10 1735, ships_health 1741, resources 1752, ships_misc 1753, ships_n 1753; missiles, derelicts, dt, total none; (real 2, rng6 161, players 272, ships_rot 1: excluded) |
| `r8f-r18-off-a`, `-b` | replay 10-18, switch off | 0 / 31 s, 31 s | oracle replay done (`randSyncErr=984 univSyncErr=1664 blobSyncErr=1619 pktSyncErr=0`) | ASan 0; byte-identical, sha256 `5cef18fd015db1c8` | IDENTICAL through 117; first divergence 118 `ships_pos` + `ships_vel` (as R7). Per field: ships_pos 118, ships_vel 118, univcheck 137, rng10 944, resources 944, bullets 1721, ships_health 1741, ships_misc 1753, ships_n 1753 |

**Exclusions** as decided: `players` at tick 93 / 272 and `rng6` at 161 (the stock record / playback differences,
decision 47), `ships_rot` (the DLL offset defect, decision 48: it differs from tick 1 in every run), `rng_all`, `fpcw`;
for the 10-18 recording also `real` (tick 2, decision 60). The first divergent packet stays the stock one (packet 46,
tick 93, blobcheck +1) for 10-17 and is packet 108 (tick 217, univcheck) for 10-18 with the switch on (was 68 / 137).

### 3.1 tick 177 (10-17): fixed

With the switch on, ship 35's velocity at 177 is `(c20420c2, c110b4d6, 41d3f9da)`, GOG's exactly (R7: -13 / +137 / +9
ulps with sqrtf, -1 / +4 / -2 with the table alone). The pieces were the double magnitude into the table (vecNormalize
and the rest of Vector.c), aitrackHeadingFunc's doubles, the matrix products of the rotation, physApplyForceToObj's
z, and the guidance's double thrust. With the switch off (sqrtf), 177 stays the first divergence: the remaining
difference is the square root itself (decision 23 / 56).

### 3.2 ticks 179, 180, 183 (10-17, switch on): fixed on the way

- **179** (`ships_vel`, ship 42, a bRecon wingman, v.z +2 ulps): the formation's desired position
  (`formationWingmanTrackLeader`) and the passive-attack heading (`GenericInterceptorPassiveAttack`), plus the
  avoidance; located with temporary stack traces of the dump's force lines (removed), then fixed by the ports above.
- **180** (`ships_pos` + `ships_vel`, at least three units; no one- or two-unit solution within +-40 ulps): fixed by
  `collCheckShipShipColl`'s exe shape (section 5: the squared-distance test before the root and the clamped-host test)
  together with the formation / parade avoidance pads' single rounding (both landed in the same run, `r8a-x10-on17`).
- **183** (ship 46, a bWorker leaving the military parade: GOG's position y +1 ulp, so its velocity differs by ~1%):
  the parade slot positions (`catParadeSlotPosition`, the step-by-step roundings); fixed.

### 3.3 tick 243 (10-17): resources, fixed

`resources` (the positions of the 324 resources) first differed at 243 in R7 too. The dump's `R` lines show 82
asteroids (indices 30-51 and 162-221, objtype asteroid, attribute 0x200 `CAT_ATTRIBUTES_KillerCollision`) drifting at
v = (4.4e-05, 3.8e-12, -1000) from tick 2; at 243 asteroid 200 crosses z = -40,000. `hwo_invert.py --field res` recovers
GOG's asteroid 200 at **z = +40,000** exactly (x and y unchanged): the exe wraps it. That is the clamp the R5
`CAT-TODO(stage 6 level load / KAS)` in `univUpdateAllPosVelResources` 0x005432d0 described: below -e on an axis the
position is put at +e, above +e at -e, with the half extents at Asteroid +0x1A8 .. +0x1B0. A recorded (saved) game
carries those dwords (the save converter copies them), so the wrap is ported (`catResourceWrap`, the field named
`catWrapExtent`); a new game's asteroid gets a zero box (univAddAsteroid; zero = no wrap, CAT-FIX) since the box's
writer for a new game is still unknown (the tag stays for it, re-worded). `resources` now first differs at 1944.

### 3.4 the next divergence (10-17, switch on): tick 247, `ships_vel`, ship 40 (owner: M2-R9 physics)

- **First divergent tick 247** (last equal 246); fields that differ there: `ships_vel` only (`ships_pos` follows at
  259, `univcheck` at 315). First divergent packet: the stock one (46); the engine's own counters first fail at frames
  rand 2059 / univ 331 / blob 109.
- **The ship**: inverting GOG's hash with an `all` dump gives one solution, ship **40** (bRecon, player 1, order 0 with
  attributes 2 = `COMMAND_MASK_PROTECTING`: `protectShip`, guarding the bMothership 36): GOG `(420767c8, 4300eec6, 429fe606)` vs ours
  `(420767ca, 4300eec5, 429fe608)` = (-2, +1, -2) ulps; every other ship equal.
- **How**: at 247 its guidance thrusts are UP 907.63 (computed), RIGHT 2120 and FORWARD 2400 (saturated); its desired
  velocity is its target (the bMothership's position, scaled, plus its velocity) plus two avoidance terms in every
  tick of the dump (243-247: two objects close by); it rolls (ROT 5) and turns from 245. Differences of a few ulps in all three
  components with two saturated thrusters point at the **coordsys** after its turn at 245-246 (not hashed comparably:
  decision 48) or the computed UP thrust.
- **Commands near it**: none for ship 40: the `C` lines of frames 115-123 (ticks 231-247) are two type-20 commands each
  (the external-build progress of player 0's build); ship 40's protect order is older.
- **Likely owner** (M2-R9 physics, the rung): the rest of the flight path's x87 shape on a guarding ship: `protectShip`
  0x004411c0 (its desired range and protect point), the avoidance's remaining compares, and the rotation of a rolling
  ship. **What would confirm it**: a DLL with `CAT_SHIP_coordsys` 0x98 (decision 48) shows the coordsys directly; then
  the port that makes ship 40's velocity `(420767c8, 4300eec6, 429fe606)` at 247.

### 3.5 the 10-18 recording, switch on: tick 155, ship 28 (owner: M2-R9 physics)

- **First divergent tick 155** (last equal 154; `real` excluded); the field: `ships_vel` (`ships_pos` 179, `univcheck`
  216). First divergent packet: 108 (tick 217, univcheck 1234686647 vs 1234686645).
- **The ship**: ship **28** (sWorker, player 0, `COMMAND_COLLECT_RESOURCES`): GOG v = `(c316f4ba, c3d48000, 4234afe1)`,
  ours `(..., 4234afe2)`: v.z -1 ulp (v.y is the capped -425). From the dump's force sum, GOG's force.z must be >= 5 ulps
  (0.0098) more negative: a coordsys difference of an ulp or two in a column's z times the saturated 120,000.
- **How**: it avoids another object in every tick dumped (from 140 at least); at 152 the avoidance passes on the right (a repulsion of
  (-1899, 222, 0.3)), and it turns hard at 152-155 (torques up to -9,479). The coordsys after that turn is the likely
  first difference (not comparable, decision 48).
- **Owner** (M2-R9 physics): the same as 3.4 (the rotation of a hard turn; avoidance compares). **Confirms**: the
  coordsys from a fixed DLL, or the port that makes v.z `4234afe1` at 155.

### 3.6 switch off (the default)

10-17: first divergence 177 `ships_vel` (ship 35, as R7: the square root itself). 10-18: 118 `ships_pos` / `ships_vel`
(as R7). Most later fields moved out, as listed in the table [audit: was "every"; `rng10` 1408 and `bullets` 1917 (10-17) are R7's ticks]: the x87 helpers help without the table too, but the
rung cannot pass 177 / 118 without decision 23 / 56's table square root.

## 4. soak-faults (decision 67)

The R7 soak (`--ai-ticks 4000`) died at tick ~1,300 on `ETG.c:7295`; the soak of this round met two more faults behind
it, all fixed (the ai mode's default stays 400 ticks):

1. **`ETG.c` `nParams >= 0 && nParams <= 8` (fatal in debug)**: `univDeleteDeadShip`'s big death explosion (`bigdeath`)
   passed `nParams` 10 with **nine** values; the exe (`univDeleteDeadShip` 0x00547d00, the pushes at 0x0054811c) passes
   ten, the tenth `collrectoffset.z + forwardlength` (+0x68 + +0x80). Ported. `etgEffectCreate` 0x00470990's parameter
   block is its ten-dword local area (`sub esp,0x28`) filled for nParams > 0 with no other bound: the tree's array and
   assertion now take `CAT_ETG_CREATE_PARAMETERS` 10 (ETG.h); `etgEffectCodeStart` 0x0046a5b0 writes the parameters with
   no bound in the exe, the tree stops at the end of the effect's variable block (`CAT-FIX`).
2. **`univRemoveAllTargetReferencesFromBullets`' NULL owner (UBSan null member access)**: the exe compares the owner
   pointer with the target (0x005474e4 `cmp [eax+0x11c],edx`; the targetable part is at offset 0), no member address;
   ported, and the same in `univRemoveAllTargetReferencesFromMissiles` 0x00547585.
3. **`AISkirmish.c` global-buffer-overflow** (`cat_aisTeamAttack` reading `targetTeams[numTargetTeams]`): the exe
   0x00432eb5 reads one past its table when the previous selection found no target in any team, i.e. the next table's
   first entry in `.data` (0x00752578 .. 0x007525e0), or past the last table the dword at 0x007525e4 (`aisFirstPlay`).
   The five tables are now one contiguous block in the exe's order and `catAisTargetTeam` returns the exe's value
   (`CAT-FIX`).
4. **The game over**: near tick 1,100-1,400 the computer player destroys player 0's mothership; `univKillPlayer` /
   `CheckPlayerWin` set the quit time and the game ends on the game-over screen with the universe stopped, so the run
   never reached tick 4,000 (it timed out). The ai self-test (`AISelfTest.c`, HWI-TEST) now clears the quit and win
   times once they are set, with one log line (`aitest: game over tick=N player0=dead player1=alive (the test keeps the
   universe running)`), so the computer player runs to the end; the 400-tick exit-test run never reaches it.

**Proof**: `r8f-ai-soak` (`tools/m2_harness.sh r8f-ai-soak --mode ai --ai-ticks 4000`): rc 0, 31 s, milestone `ai self-test done`, **ASan 0, no unsuppressed leak, no fatal**; `aitest: game over tick=1101
player0=dead player1=alive (the test keeps the universe running)`, then `aitest: done tick=3998 ... checks=16/16
result=PASS`; `oracle: mission run done ticks=4000 records=4003 ships=250`, `run.hwo` 984,370 B, sha256
`fd1480fef8611673`. UBSan kinds: only the known misaligned ETG bytecode / particle / BTG loads (Particle.c 94, ETG.c
98, BTG.c 2); **no null member access** (the bullet owner) and no ETG assertion. Before the fixes: `r8a-soak-pre`
(ETG fatal at tick ~1,290 in `univDeleteDeadShip`'s `bigdeath.ebg`, located with a temporary stack trace, removed),
`r8a-soak-x8` (AISkirmish.c:1530 global-buffer-overflow after the ETG fix), `r8a-soak-x9` (the universe stopped at
the game over, killed by the harness timeout).

## 5. collision: `collCheckShipShipColl` 0x0043ade0

Read against the tree in full. Ported: **the clamped-host test** (0x0043aec4 .. 0x0043aef3: a ship with
`SOF_Clamped` whose clamp info's host is the other ship does not collide with it, either way round, before the sphere
distance and the Sentinel test); **the distance test's shape** (0x0043b1ba .. 0x0043b252: the squared distance
`(z*z + y*y) + x*x` is a double stored as the float `distsquared` and compared, unrounded, with `colldist` squared
before any root; the root is taken from the stored float after the too-close case and the NotBumpable tests). Together with the avoidance pads' rounding
this fixed the 10-17 replay's tick-180 divergence (several ships at once). Compared and as the tree: the docking exemption
(`catCollDockingExempt`), the `dockingship` tests, the Sentinel field (`cat_collSentinelFieldExempt`), the salvager tests,
`SOF_Hide`, the clamp exemption (`cat_collClampExempt`: every branch of 0x0043b078 .. 0x0043b1b4 matches), the kamikaze
checks (two `cat_kamikazeCheckShip` calls, both damages, then `ObjectsCollided`, as `kamikazeCheck` and the else
branch), the ramming frigate, the owner / disabled / salvager / Leech / RKO damage tests.

## 6. gates (final tree, plain harness)

**Builds**: `tools/build.sh --debug --clean` and `--release --clean`: rc 0, `[319/319] Linking target hwi-engine`,
**0 warnings, 0 errors** under `-Werror` (logs `original/scratch-m2/m2r8-rung/build-{debug,release}3.log`); debug
87,567,848 B (sha256 `eccae76f349bec1a`), release 5,433,928 B (`14450708115fa874`), copied to
`original/scratch-m2/m2r8-rung/final/`. `tools/m2_layout32.sh`: OK (Ship 1596 = 0x63C, Player 25904, Universe 252348);
`python3 -I tools/m2_save32_gen.py --check`: current (the Asteroid field renamed, same layout).

| run | mode | rc / wall | milestone | first stop | sanitizers and checks |
|---|---|---|---|---|---|
| `r8f-mission-a`, `-b` (`mission_test.sh`) | mission, 6,000 ticks, debug | 0 / 28 s, 27 s | oracle mission run done (`ticks=6000 records=6003 ships=154`) | none | ASan 0, no leak; `cmp` equal, 1,476,370 B, **sha256 `245b3d88387ed076`**; IDENTICAL 6,001 ticks; perturbed run caught at 3000 / `rng10`, flipped copy at 4000 / `ships_health`: **MISSION TEST PASS** |
| `r8f-mission-rel-a`, `-b`, `-rel-perturb` | mission, 6,000, **release** | 0 / 6.3 s each | oracle mission run done | none | `cmp` equal, sha256 `e0c18fcf00229e0f`; IDENTICAL to the debug stream over 6,001 ticks (header build flags only); **MISSION TEST PASS** |
| `r8f-r17-{on,off}-{a,b}`, `r8f-r18-{on,off}-{a,b}` | replay (section 3) | 0 / 31-56 s | oracle replay done | none | ASan 0, no leak; each pair byte-identical |
| `r8f-ai-a`, `-b` | ai, 400 ticks | 0 / 19 s, 20 s | ai self-test done | none | ASan 0, no leak; `checks=16/16 result=PASS`; `cmp` equal, 98,768 B, sha256 `dcab88df1f2d0dab` |
| `r8f-ai-soak` | ai, **4,000 ticks** | 0 / 31 s | ai self-test done | none | ASan 0, no leak, no fatal; `result=PASS` (section 4) |
| `r8f-skirmish-lfe2-a`, `-b` | skirmish, LiveFireExercise2, 600 | 0 / 23 s, 23 s | skirmish self-test done | none | ASan 0, no leak; `done ticks=600 ships=89 result=PASS`; `cmp` equal, sha256 `955744cb5e558abb` |
| `r8f-skirmish-armed8` | skirmish, Armed8, 600 | 0 / 20 s | skirmish self-test done | none | ASan 0, no leak; `done ticks=600 ships=54 result=PASS`; sha256 `03376d1dd4392208` |
| `r8f-fe` | fe, 120 s | 124 / 120 s (timeout) | **Main_game_screen** | none: still running at the timeout | ASan 0, no leak; 0 UBSan reports; 0 `fescreen` reports |
| `r8f-sp` | sp, `--timeout 340` | 124 / 340 s (timeout) | universe running: last `univUpdateCounter` **3,264** (gate 1,248) | none: still running at the timeout | **ASan 0**, no leak; 0 `fescreen` reports; UBSan kinds R7's list (ETG opcode kinds, BTG.c, stb_image_write.h) |
| `r8f-motion-noi`, `r8f-nis-noi` | motion 224 / nis n06:26:36, `-- /noInterp` | 0 / 41 s, 46 s | motion self-test done (`result=PASS` both) | none | ASan 0, no leak; dumps `0838a011db819920` / `d1e6f6e477906b52` (moved, below) |

(The final runs were made from five parallel chains on the final binaries. Earlier checks: `r8f2-mission-a` gave the
same Mission01 stream from the build before the killer-asteroid wrap, and `r8f2-r17-on-a` (with
`/oracleShipDump 35@170-180`) was byte-identical to that build's replay without the switch: the dump is read only.)

**The Mission01 stream** (`tools/oracle/mission_test.sh --bin <final debug> --label r8f-mission --ticks 6000 --perturb
3000 --copy-tick 4000`, and the same with the release binary):

- **Before** (the round base `145352a`, its binary kept in `original/scratch-m2/m2r8-rung/hwi-engine-base-debug`):
  `r8a-base-mission-a` = `-b`, sha256 `a97da06b142b590a`, **IDENTICAL over all 6,001 ticks to the accepted baseline
  `r7i2-mission-a` (`9f7416bb64fec98f`)**, header build identity only (cd5866de vs d190685e).
- **After**: debug **`245b3d88387ed076`** (A = B), release `e0c18fcf00229e0f` (A = B, IDENTICAL content to debug).
  Against `9f7416bb64fec98f` (`hwo_compare.py r7i2-mission-a/run.hwo r8f-mission-a/run.hwo`): **FIRST DIVERGENT TICK 1,
  field `ships_rot` only**; with every other field (`--fields rng10,rng6,rng0,rng_all,ships_n,ships_misc,ships_pos,
  ships_vel,ships_health,missiles,bullets,derelicts,resources,statcheck,univcheck,players,dt,total,real`): **IDENTICAL
  over 6,001 ticks**. The coordsys of the ships Mission01 rotates (the matrix products and the rotation integration in
  their x87 shape) moved; no position, velocity, health, RNG or list field did (Mission01's ships do not fly under
  their own AI in this headless run; decision 57). The stream moved once in this producer.

**The ai and skirmish self-test streams move** (as expected: the flight path's arithmetic changed): `ai` `dcab88df1f2d0dab` (R7 `c6ff74db1a364512`): from tick 1 in `ships_rot`, from
tick 17 in `ships_pos` / `ships_vel` (the scripted fleet's first flight), `univcheck` from 73; `rng10`, `rng6`, `rng0`,
`ships_n`, `ships_misc`, `ships_health`, the lists and `players` IDENTICAL over 401 ticks; 16/16 checks hold at R7's
ticks. `skirmish` LiveFireExercise2 `955744cb5e558abb` (R7 `979f7d23606207f5`): `ships_pos` from tick 0 (the start
positions placed through the matrix / vector helpers by the MissionSphere readers), `ships_rot` 321, `ships_vel` 322,
`univcheck` 392; RNG streams, `ships_n`, `ships_misc`, health and the lists IDENTICAL over 601 ticks. Armed8
`03376d1dd4392208` (R7 `af269ea4bc7dca8f`): `ships_pos`, `resources`, `univcheck` from tick 0 (the placement of its
242 asteroids too), `ships_rot` 321, `ships_vel` 323. All three re-baseline with this round.

**The motion / nis guard-rail dumps** (`--mode motion` / `--mode nis` with `-- /noInterp`) move with the shared helpers:
`motion.dump` `0838a011db819920` (pre-feature `b2af0d6decdf6474`): 34 of 1,431 frame lines differ,
only in the frame CRC (camera pose and every ship's screen position equal: the render's own matrix products, e.g. the
mesh transforms, moved by an ulp somewhere in 34 frames); `nis.dump` `d1e6f6e477906b52` (pre-feature
`3f04c837e605ba23`): 1,170 of 1,440 lines differ (the NIS objects' screen positions: the NIS player's matrices go
through `matMultiplyMatByMat` / `matMultiplyMatByVec`). The guard-rail's meaning (flags off = the feature changes
nothing) cannot be re-measured against the pre-feature binary any more: the helpers under every frame changed. The
integrate re-baselines both from this build's `/noInterp` dumps (or the user rules).

## Files changed

- `src/Game/Vector.c`, `src/Game/Matrix.c`, `src/Game/FastMath.h` (`catFsqrtD`): the helpers' x87 shape.
- `src/Game/AITrack.c`: `aitrackHeadingFunc` (+ `catAitrackTurn`), `aitrackHeadingWithBankPitchFunc`,
  `aitrackVelocityVectorGuidance` (+ `catAitrackThrust`).
- `src/Game/Physics.c`: `physApplyForceToObj`, `physUpdateObjPosVelShip`; the dump's A / F hooks.
- `src/Game/AIShip.c`: `GetCollSizeInDirection` (+ `catGetCollSizeD`), `aishipGetTrajectoryWithVelPrediction`, the
  avoidance of `aishipFlyToPointAvoidingObjsFunc`; the dump's V hooks.
- `src/Ships/GenericInterceptor.c`: `GenericInterceptorPassiveAttack`.
- `src/Game/Formation.c`: `formationWingmanTrackLeader`, `catParadeSlotPosition`.
- `src/Game/CommandLayer.c`: `CalculateMoveToPoints`, `hyperspaceCost`.
- `src/Game/Collision.c`: `RangeToTarget`, `collCheckShipShipColl`.
- `src/Game/UnivUpdate.c`: the bullet / missile owner compare, `univFindNearestResource`, `univDeleteDeadShip`'s tenth
  parameter, the killer-asteroid wrap (`catResourceWrap`) and univAddAsteroid's zero box.
- `src/Game/SpaceObj.h`: Asteroid +0x1A8 `catWrapExtent` (was `catAsteroidReserved1A8[3]`).
- `src/Game/ETG.c`, `src/Game/ETG.h`: the ten-parameter block, the bounded parameter write.
- `src/Game/AISkirmish.c`: the contiguous target tables; `src/Game/AISelfTest.c`: the game-over line.
- `src/Game/OracleDump.c`, `src/Game/OracleDump.h`, `src/SDL/main.c`, `src/Game/CommandNetwork.c`: `/oracleShipDump`.
- `tools/oracle/hwo_invert.py` (new), `tools/m2_harness.sh` (header: the dump).
- `ghidra/delta_map_review.json`, `ghidra/delta_map.json` (merged), `docs/m2-r1-function-deltas.md` (regenerated), this
  report.

## Delta-map review entries

50 entries `[review: M2-R8 oracle-rung-physics]` in `ghidra/delta_map_review.json` (2 new: `RangeToTarget`
0x00439c30, `etgEffectCreate` 0x00470990; 48 existing ones with the note appended, sample
`port-R8-oracle-rung-physics`), merged with `python3 -I tools/delta_map.py merge --done m2r8:oracle-rung-physics`
(48 + 2 records changed in two merges) and `python3 -I tools/delta_map.py report`. Counts: IDENTICAL 2,853 -> 2,828, MODIFIED 1,483 ->
1,508, REWRITTEN 84, reviewed 3,036 -> 3,038. **IDENTICAL found modified (now MODIFIED)**: vecNormalize, vecHomogenize,
vecCopyAndNormalize, vecNormalizeToLength, vecCapVector, vecCapMinVector, vecCapMinMaxVector,
vecLineIntersectWithXYPlane / YZ / XZ, vecLineIntersectWithPlane, vecVectorsBlend, matCreateCoordSysFromHeading,
matMultiplyMatByMat, matMultiplyMatByVec, matMultiplyVecByMat, aitrackHeadingWithBankPitchFunc,
aitrackVelocityVectorGuidance, GetCollSizeInDirection, aishipGetTrajectoryWithVelPrediction, formationWingmanTrackLeader,
CalculateMoveToPoints, hyperspaceCost, RangeToTarget, univRemoveAllTargetReferencesFromBullets (25). **Compared,
identical** (note appended): vecCapVectorSloppy, getVectDistSloppy, matMakeRotAboutX / Y / Z, physApplyRotToObj,
aishipGetTrajectory, RangeToTargetGivenDist, MoveLeftToGo, MoveReachedDestinationVariable. **MODIFIED, the change
extended**: aitrackHeadingFunc, physUpdateObjPosVelShip, physApplyForceToObj, aishipFlyToPointAvoidingObjsFunc,
GenericInterceptorPassiveAttack, processMilitaryParadeToDo, setMilitaryParade, collCheckShipShipColl,
univRemoveAllTargetReferencesFromMissiles, univFindNearestResource, univDeleteDeadShip, etgEffectCreate,
etgEffectCodeStart, univUpdateAllPosVelResources (the wrap), univAddAsteroid (the zero box). Not in the map (cat-new):
`cat_vecDistance`, `cat_vecPlaneNormalFrom3Points`, `cat_aisTeamAttack`.

## CAT-TODOs

No `CAT-TODO` named the physics / combat / effects owner or M2-R8 at the base (`grep -rn 'CAT-TODO(' src` lists stage 5
and stage 6 owners only); none was added for them. **Re-worded**: the `CAT-TODO(stage 6 level load / KAS)` of
`univUpdateAllPosVelResources` now names only the box's writer for a new game (the wrap itself is ported).
`CAT-FIX` added: `etgEffectCodeStart`'s bounded parameter write, `catAisTargetTeam`, the zero wrap box of a new
asteroid. The HWI-EXPERIMENT `/oracleTableFsqrt` stays (decision 23 / 56).

## For STATE.md

- **Decision 23 / 56 (still the user's)**: with the x87 helpers in place, the 10-17 replay matches GOG through tick 246
  with the table square root and only through 176 without it; the 10-18 through 154 and 117. Recommendation unchanged:
  make the table the default. Measured: the headless Mission01 stream does not move with it (`r8f-mission-table`,
  `-- /oracleTableFsqrt`: sha256 `245b3d88387ed076`, byte-identical to the default run, ASan 0); the replays do (section 3);
  the ai and skirmish streams were not measured with it.
- **Chosen names**: `catFsqrtD` (FastMath.h), `catVecMagSqD` / `catVecScaleD` (Vector.c), `catAitrackTurn` /
  `catAitrackThrust` (AITrack.c), `catGetCollSizeD` (AIShip.c), `catResourceWrap` / `catResourceWrapAxis`
  (UnivUpdate.c), `Asteroid.catWrapExtent` (+0x1A8), `CAT_ETG_CREATE_PARAMETERS` (ETG.h), `aisTargetTables` /
  `catAisTargetTeam` (AISkirmish.c), `oracleShipDump*` (OracleDump.c).
- **Defaults kept**: libm `sin` / `cos` for the exe's `fsin` / `fcos` (an experiment with the x87 instructions gave the
  same stream); `tanl` + `fma` for `fptan`'s extended product; a new game's killer asteroid does not wrap (zero box)
  until the box's writer is found (stage 6 level load / KAS); the ai self-test keeps the universe running after player
  0's defeat (HWI-TEST; only long soaks reach it).
- **Moved streams / dumps to re-baseline**: Mission01 `ships_rot` only (`245b3d88387ed076` debug / `e0c18fcf00229e0f` release); ai `dcab88df1f2d0dab`; skirmish `955744cb5e558abb` (LiveFireExercise2) / `03376d1dd4392208` (Armed8);
  motion / nis `/noInterp` dumps `0838a011db819920` / `d1e6f6e477906b52`.
- **Tooling**: `/oracleShipDump` and `tools/oracle/hwo_invert.py` replace the temporary per-ship instrumentation of R6 /
  R7 (decision 48's DLL fix would make the coordsys comparable and is the next tooling step).

## What is left (owners)

1. **The rung** (M2-R9 physics): 10-17 tick 247 ship 40 (3.4), 10-18 tick 155 ship 28 (3.5): the rotation of a hard
   turn / roll and the guard and avoidance paths; with the user's ruling on decision 23 / 56 and the DLL's
   `CAT_SHIP_coordsys` 0x98 (decision 48) as the means to see the coordsys.
2. **The killer-asteroid box's writer for a new game** (stage 6 level load / KAS; the tag in UnivUpdate.c).
3. **The x87 shape elsewhere**: about 200 call sites of `fsqrt` and every float expression outside the flight path keep
   the tree's float evaluation; each will show in the rung when it matters (the dump and the inverter find it).
4. **The motion / nis guard-rail dumps** moved with the helpers (section 6): the integrate re-baselines them (or the
   user rules), as with the Mission01, ai and skirmish streams.
5. [audit: M2-R8 oracle-rung-physics] **hyperspaceCost's return** (M2-R9 physics): the exe returns the unrounded double;
   the tree's `real32` return rounds it before the callers' integer conversion (`CAT-TODO(M2-R9 physics)` in CommandLayer.c).
