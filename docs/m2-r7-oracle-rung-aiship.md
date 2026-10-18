<!-- done: x87-divergence, aiship, aiutil, aitrack-aivar, aimdock, gates -->
# M2-R7 oracle-rung-aiship: the oracle's first divergence, then the AI ship and utility code

Status: DONE (M2-R7 producer 1; base commit `4c31f6e`; nothing committed by the producer, the auditor
checkpoints).

Subsystems: **x87-divergence** (the tick-136 health rounding, the replay rerun, the next divergence localised, the
fsqrt experiment), **aiship** (AIShip.c), **aiutil** (AIUtilities.c), **aitrack-aivar** (AITrack.c / AIVar.c, the
IDENTICAL comparisons), **aimdock** (the rest of `aimProcessDock`), **gates** (builds, mission stream, replay, fe, sp).

## Headline

The oracle's first divergence is fixed: with `cat_clExternalBuildUpdate` 0x0044dfc0's single x87 rounding reproduced,
the replay of `oracle01` is **IDENTICAL to the GOG recording `oracle01-rec.hwo` in every simulation field of the list
through tick 176** (rng6 at 161 excluded, decision 47); `ships_health` now first differs at 1934. The **next divergence
is tick 177, `ships_vel`, ship 35** (an sWorker of player 0 starting to harvest): GOG's velocity was recovered exactly by
inverting the hash, `(c20420c2, c110b4d6, 41d3f9da)` against ours `(c20420cf, c110b44d, 41d3f9d1)`, and the difference
was traced to the coordsys the ship built while turning in place from tick 171 (aitrackHeadingFunc 0x00418de0 /
vecNormalize 0x00550770 with the exe's TABLE `fsqrt` 0x00472860 on a 53-bit argument); the one fsqrt experiment
(`/oracleTableFsqrt`) brings the velocity to -1/+4/-2 ulps of GOG's [audit: was -1/+4/+2; GOG minus ours in raw bits, as the sqrtf -13/+137/+9]. After the AI ship port the positions match GOG
through tick 179 (were 177) and `univcheck` through 179. The AI ship, utility, track and variable bodies (AIShip.c 20,
AIUtilities.c 85 + `cat_aiuWrapCapture`, AITrack.c 19, AIVar.c 13) are at the exe's; 105 new delta-map reviews. The
Mission01 stream is unchanged by every step of this producer (`7c074b0b122f2fb5`, IDENTICAL to the baseline
49240abcf9f0ed97 but for the header's build identity).

## 1. x87-divergence

### 1.1 The fix (src/Game/CommandLayer.c `clExternalBuildAdvance`)

`cat_clExternalBuildUpdate` 0x0044dfc0, read with `tools/m2_fdis.py 0x0044dfc0`: mode 2 (progress, 0x0044dfe5) and
the mode-0 branch (done, 0x0044e05a, taken while `catBuildProgress` < the double 1.0 at 0x0072a548) are the same five
instructions:

```
fld   dword [esp+0x14]        ; progress            (float load)
fsub  dword [ecx+0x3e8]       ; - catBuildProgress  (float operand)   [the new progress is stored to +0x3e8 here]
fmul  dword [edx+0xa4]        ; * statinfo maxhealth (float operand)
fadd  dword [ecx+0x170]       ; + health            (float operand)
fst   dword [ecx+0x170]       ; ONE rounding, to float, at the store
fcomp dword [edx+0xa4]        ; the clamp compares st0 = the UNROUNDED sum with maxhealth (test ah,0x41)
```

Every operand is a `dword` (float) load; the intermediates live on the FPU. The precision control is the 53-bit
default: the GOG stream's per-tick control word, read in the game thread at the tick hook, is 0x027f in all 12,282 T
records of `oracle01-rec.hwo` (and the header's), so the sum is rounded to 53 bits, then once to 24 at the `fst`. The
tree now evaluates `((double)progress - (double)old) * (double)maxhealth + (double)health` and rounds once
(`(real32)health`), with the clamp on the double sum, under a `CAT:` comment that gives the sequence. The build keeps
`-ffp-contract=off` (meson.build), the x86-64 build evaluates double in SSE2 (no excess precision), and no flag changed
tree-wide: this is one expression.

### 1.2 The Mission01 stream (headless, 6,000 ticks; `tools/oracle/mission_test.sh --ticks 6000 --no-perturb-run`)

| step | runs | sha256 (16) | against the baseline `r6-integrate-mission-a` (49240abcf9f0ed97) |
|---|---|---|---|
| before (base tree 4c31f6e + nothing) | `r7a-base-mission-a`, `-b` | `7c074b0b122f2fb5` (A = B) | IDENTICAL, 6,001 ticks; only the header build identity differs (9e20b5bd vs a68b3ef0) |
| after the rounding fix | `r7a-x87-mission-a`, `-b` | `7c074b0b122f2fb5` (A = B) | IDENTICAL, 6,001 ticks; header build identity only |

Unchanged, as expected: nothing is built externally in the headless Mission01 run (no `cat_clExternalBuildUpdate`
call), so no field moved. (The sha256 differs from 49240abcf9f0ed97 only by the build identity the engine writes in
the header: the HEAD commit, a68b3ef0 here.) MISSION TEST PASS both times, ASan 0.

### 1.3 The replay of oracle01 after the fix

Command lines (the `docs/m2-r6-oracle-mode-a.md` arguments; `<runs>` = `original/scratch-m2/m2r2-harness/runs`,
`<rec>` = `original/oracle/oracle01-rec-20261017-140428/oracle01-rec.hwo`):

```
tools/m2_harness.sh r7a-x87-replay-a --mode replay --timeout 600      # and r7a-x87-replay-b
python3 -I tools/oracle/hwo_compare.py <rec> <runs>/r7a-x87-replay-a/run.hwo --from-tick 1 --to-tick 9600 \
    --fields rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real \
    --json <runs>/r7a-x87-replay-a/compare.json
  FIRST DIVERGENT TICK 161 (last equal tick 160); rng6, CAND is 1 draw(s) behind REF      <- excluded (decision 47)
python3 -I tools/oracle/hwo_compare.py <rec> <runs>/r7a-x87-replay-a/run.hwo --from-tick 1 --to-tick 9600 \
    --fields rng10,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real \
    --json <runs>/r7a-x87-replay-a/compare-sim.json
  FIRST DIVERGENT TICK 177 (last equal tick 176); 1 field(s) differ, likely origin: ships_vel
    ships_vel REF 740322b034279499  CAND 445c0bd4842e549c
  FIRST DIVERGENT PACKET 46 (REF tick 93, CAND tick 93): blobcheck 1785610478 vs 1785610479   <- the stock record/playback difference (decision 47)
```

* Both replays: `oracle: replay done ticks=9601 packets=4800 ships=81`, ASan 0, rc 0, 33 s; **byte-identical**
  (`cmp`), 2,630,923 B, sha256 `b7e87c4cd15694d2` (before the fix, `r7a-base-replay`: `9f261bc97cc24bd8`, first
  divergence tick 136 `ships_health` as in R6).
* **Headline: every simulation field of the list is IDENTICAL to the GOG recording through tick 176**; the health
  divergence at 136 is gone (`ships_health` now first differs at **1934**, behind the velocity divergence).
* Per field (`--fields <one>`, after / R6): ships_vel 177 / 177, ships_pos 178 / 178, univcheck 179 / 179, resources
  243 / 243, ships_misc 871 / 871, rng10 1376 / 1376, bullets 1917 / 1917, **ships_health 1934 / 136**, ships_n 2048 /
  2048; missiles, derelicts, dt, total, real IDENTICAL over 1..9600; rng6 161 (excluded, decision 47).
* The engine's own sync counters are unchanged (randSyncErr 4037, univSyncErr 4703, blobSyncErr 4746, pktSyncErr 0):
  those compare with the recorded checksums of later frames, whose first errors (rand 1393, univ 195, blob 109) lie
  behind the tick-177 divergence or are the stock blob difference.

### 1.4 The next divergence, localised: tick 177, `ships_vel`, ship 35

Method (as R6): temporary per-ship / per-packet instrumentation (an `HWI_R7DUMP` environment switch printing, per T
record, each ship's id, type, owner, order, position, velocity, health, rotation speed, coordsys and AI state, the
non-extbuild commands of each packet, and the inputs of `aishipFlyToPointAvoidingObjsFunc`'s velocity cap,
`aitrackVelocityVectorGuidance` and `physUpdateObjPosVelShip` for the ticks asked for; runs `r7a-dbg-dump1` ..
`dump5`, removed again; the patch is kept outside git), and the GOG hash inverted offline.

* **First divergent tick 177** (last equal 176); the one field: `ships_vel`. Causal order: velocity first; the
  positions follow at 178 (`pos += v dt`), `univcheck` at 179.
* **The ship**: the first ship of the replay that moves at all is ship **35** (type 14 = `sWorker`, player 0, order 5
  `COMMAND_COLLECT_RESOURCES`): no ship has a nonzero velocity before tick 177 (ship 47 is clamped while built). Ships
  33/34 (the other two Workers) and player 1's ships 40/42 start at 178, 36..39/41/43 at 179.
* **The packet and its commands**: frame **84** (processed at tick 169) carries three `collectresource` commands
  (type 6: resource 0x05a4 for ships 0x21, 0x22, 0x23 = 33, 34, 35), and the Workers' order becomes 5 at tick 170.
  Frame 88 (tick 177) carries 21 `alliance` commands (type 13), one `research` (19), one `misc` (10), a
  `collectresource` for ships 0x2c..0x2e, two `move` (1), a `protect` (7) and an `attack` (2): none for ship 35.
* **What GOG's ship 35 did**: the GOG `ships_vel` hash at 177 was inverted by a meet-in-the-middle search over ship
  35's three velocity words (FNV-1a-64 is invertible byte by byte; the 47 other ships taken from our run: all at rest;
  x and y within +-1000 ulps, z within +-150): exactly **one** solution, `(c20420c2, c110b4d6, 41d3f9da)`, against
  ours `(c20420cf, c110b44d, 41d3f9d1)`: -13, +137, +9 ulps (relative 4e-7 .. 1.5e-5). Our position at 177 equals
  GOG's because 1/16 of these differences is below the position's ulp (0.0078 at |x|, |y| ~ 83,000..87,000; 3e-5 at
  z ~ -330). (A random hash collision in that 1.2e9-candidate search has a probability of about 7e-11.)
* **Why the velocity differs**: at tick 177 `aitrackVelocityVectorGuidance` 0x00419460 saturates all three thrusters
  (desired local velocity (30.5, -194.7, 428.8) x mass/dt 4,800 > `thruststrength` 120,000 on every axis), so the
  force is 120,000 x (col1 - col2 + col3) of the ship's coordsys and the velocity from rest is a pure function of the
  **coordsys**. Ship 35 turned in place before (aistate 1 `STATE_POINT_IN_DIRECTION` of
  `aishipFlyToPointAvoidingObjsFunc` 0x00414610, `AISHIP_FirstPointInDirectionFlying`) from tick 171 to 175, via
  `vecNormalize` 0x00550770 of the target vector and `aitrackHeadingWithBankFlags` (`aitrackHeadingFunc` 0x00418de0),
  then the rotation update of `physUpdateObjPosVelShip` 0x004e4950. The coordsys is not comparable with the GOG
  stream (`ships_rot` hashes Ship+0x80 in the DLL, decision 48), so the true first divergence is in that turn, from
  tick 171, and becomes visible at 177.
* **The owning arithmetic**: the exe's vector helpers on the turn path pass a 53-bit magnitude to the TABLE square
  root: `vecNormalize` 0x00550770 is `fld z; fld y; fld x; x*x + y*y + z*z` on the FPU, `fstp qword` (the DOUBLE, not
  rounded to float) as the argument of `fsqrt` 0x00472860 (the 16,384-entry table of `fmathInit` 0x004728c0: 13
  mantissa bits and the exponent's parity pick the entry, the argument's low dword is kept), then `fdivr 1.0` and each
  component multiplied by the double 1/mag and rounded once at its `fstp`; `vecCapVector` 0x00550950 likewise. The
  tree computes the magnitude in float and takes `sqrtf` (decision 23).
* **The fsqrt experiment** (one per round, decision 23): `/oracleTableFsqrt` (new, `src/Game/FastMath.c` +
  `FastMath.h`: `fsqrt` / `fmathSqrt` become HW1's `(real32)fmathSqrtDouble((real32)(x))` over the exe's table while
  the switch is set; the default stays `sqrtf`, verified: the default replay is byte-identical to the one before the
  switch existed, `r7a-x87sw-replay` = `b7e87c4cd15694d2`). Run `r7a-fsqrt-exp-replay` (`-- /oracleTableFsqrt`): replay
  done, ASan 0; **first divergent tick still 177 `ships_vel`** (positions 178, univcheck 179, resources 243, ships_misc
  871, rng10 1384, bullets 1917, ships_health 1934, ships_n 2048); ship 35's velocity at 177 becomes
  `(c20420c3, c110b4d2, 41d3f9dc)`: **-1, +4, -2 ulps from GOG's** [audit: was +2; 41d3f9da - 41d3f9dc = -2, the convention of the sqrtf triple] (sqrtf: -13, +137, +9). The rotation speed at 171
  already differs between the two (`3f7b79a6` sqrtf, `3f7b7ad1` table), which confirms the turn as the place.
  Both first divergent ticks: default 177 (ships_vel), table 177 (ships_vel).
* **Proposal for M2-R8** (simulation, not made here: decision 23 is the user's, and the remaining few ulps need more
  than the table): (a) the user's ruling on the table `fsqrt`; with it, (b) the exe's x87 shape of the vector helpers on
  the flight path (`vecNormalize` 0x00550770, `vecCapVector` 0x00550950 and the other `vecCap*` / `getVectDist*`
  that `fstp qword` the magnitude: the double magnitude into the table, the double reciprocal, one rounding per
  component), then `aitrackHeadingFunc` 0x00418de0's own intermediates. **What would confirm it**: with (a) and (b)
  the tick-177 velocity must come out as `(c20420c2, c110b4d6, 41d3f9da)` exactly (the inverted GOG value), and
  `ships_vel` must then be IDENTICAL at 177; a DLL with `CAT_SHIP_coordsys` 0x98 (decision 48) would show the
  rotation's first divergence directly (tick 171 expected).

## 2. aiship (src/Game/AIShip.c)

The 20 exe functions of AIShip.c (15 IDENTICAL, 5 MODIFIED) read against the tree with `tools/m2_fdec.py` and, where
the decompile was ambiguous, `tools/m2_fdis.py`.

| function | exe | what changed |
|---|---|---|
| `aishipPrecalcInfo` | 0x00413bd0 | the massive-ship default cases are the exe's six types `p1Mothership`, `trFloatingCity`, `trResearchStation`, `trGhostship`, **`dRKO`**, **`dBeastMothership`** (0x60 0x62 0x65 0x68 0x6d 0x82); the HW1 aliases retired |
| `rowSignalShipToGetOutOfWayOfMe` | 0x00414400 | IDENTICAL in the map, found modified: no HW1 R1-Mothership branch (with the `R1`/`Mothership` aliases it fired for the Sect mothership); every ship parts by `rowAvoidBy` |
| `aishipFlyToPointAvoidingObjsFunc` | 0x00414610 (the task's 0x00413e40 is inside `aishipGetTrajectoryWithVelPrediction`; the R5 comments carry that address) | see below |
| `aishipGuideMissile` | 0x00415e60 | the lost-target test is `cat_shipIsCloakedFully` (HW1: `SOF_Cloaked`) |
| `aishipFlyMineToTarget` | 0x00415fa0 | the lost-target test is `cat_shipIsCloakedFully` or `SOF_Disabled` |
| `aishipmineaquiretarget` | 0x004162c0 | no derelict-race ship outside single player; no ship with catFlags18 bit 5 (new `CAT_SHIPFLAG_KasMines`, set by the mission function `kasfShipsEffectedByMines` 0x0049a080); a fully cloaked ship only once `cat_shipDetectedByPlayer` (sub_53F9F0); otherwise not disabled, a Leech only when detected, a mimic not while `cat_mimicAppearsAlliedTo` (HW1: `proximityCanPlayerSeeShip`) |
| `aishipGuideMine` | 0x00416410 | IDENTICAL in the map, found modified: a mine whose fly-to-target fails goes to `MINE_SEARCH_AND_STOP` when it has no formation (0x00416783 [audit: was 0x0041658b, not an instruction of the function]: `formationinfo ? 4 : 2`), HW1 always reformed the wall; the fake target clears `staticinfo` as the exe does |

`aishipFlyToPointAvoidingObjsFunc` 0x00414610, whole body (5,298 B) read and the avoidance loop rewritten in the exe's
order:

* entry: a single-player ship whose hyperspace effect plays (`shipSinglePlayerGameInfo->hyperspaceEffect`, +0x4C4 /
  +0x1C; 0x0041468d) is not flown;
* the formation leader's speed: not for `COMMAND_DOCK`, and `FORCEFIELD_SPHERE_FORMATION` (10) counts as the sphere
  (0x004149e2 .. 0x00414a38);
* the speed cap multiplies `tacticsGetShipsMaxVelocity` by `cat_shipGetSpeedMultiplier` (0x00414ab2);
* the mimic avoid radius `cat_mimicGetAvoidRadius` for the flying ship (0x00414b28) and for a mimic in the way
  (0x004152b5);
* no right of way for a fighter or corvette with `COMMAND_DOCK` and a dock point (`dockvars.catDockPointIndex != -1`;
  0x00414b59);
* **the small-target pass is skipped when `originalCollSphereSize` (ShipStaticInfo +0x48) is below 500.0** (0x0072a4ac;
  0x00414c8a): small ships avoid only the big targets;
* the link exception (R5) skips only the small-target pass, as the exe jumps to the pass counter (0x00415916; R5 skipped
  both);
* **the Ship+0x608 / mission exceptions** (0x00414e0e .. 0x00414ea0): a ship does not avoid the ship it keeps at
  +0x608 (`catSalvageBigGun`: a towed gun's dBigGun, a Worker's repair target) nor a `CLASS_Component` clamped to it, and
  the other way round; in single player a `trFloatingCity` does not avoid a `dSlipgateNode`, nor a `dEcclesiam` the
  `sMothership`;
* the right-of-way block: only allied ships are asked to get out of the way, enemy ships are avoided only with
  `SPECIAL_2_DontPlowThroughEnemyShips`; HW1's potshot at an asteroid in the way (`attackSimple`) and the Asteroid3
  right-of-way bonus are gone (the exe skips every non-ship object with a lower right of way);
* the parade padding: `AVOID_OBJ_MSHIP_MILPARADE_PADDING_SCALE` when the host is of `CLASS_Mothership` (no R2 variant),
  and the ship's own pad unchanged (0x00415305 .. 0x00415339);
* the clamp test also checks `clampInfo != NULL` (0x0041545a);
* cloaking: `cat_shipIsCloakedFully` plus `cat_shipDetectedByPlayer` (0x004154c2; HW1 `proximityCanPlayerSeeShip`);
* the squared range is tested before the square root (0x00415516 .. 0x00415543);
* no HW1 R1-mothership flattening of the repulsion;
* pass-on-right from 30 m (`|targetVec|^2 >= 900.0`, 0x0072a4cc; HW1 20 m);
* **the UBSan report at AIShip.c:1467** ("member access within null pointer of type 'struct Ship'", in every dock run
  since the managers port and once per replay): `&salvager->target->impactable == avoidobj` with a salvager that has
  no target took the member address of NULL. The exe compares the pointers (0x004153f5 `cmp ecx,esi`); so does the tree
  now. Gone in `r7a-ai1-dock` and `r7a-ai1-replay-a` (R6's `r6-integrate-dock`: 1 report).

The HW1 aliases (`Mothership`, `Carrier`, `R1`, `R2`, `P1Mothership`, `FloatingCity`, `ResearchStation`, `Ghostship`)
are gone from AIShip.c. The script variables `AVOID_ROW_R1MOTHERSHIP_BY` and `AVOID_OBJ_R2MSHIP_MILPARADE_PADDING_SCALE`
stay: the exe's aiship.script table still names them (strings 0x0074dac4, 0x0074ddf0), unread.

## 3. aiutil (src/Game/AIUtilities.c, AIUtilities.h, AIMoves.c)

All 85 exe functions of AIUtilities.c (63 IDENTICAL, 22 MODIFIED) read against the tree; `cat_aiuWrapCapture`
0x0041f3d0 (cat-new) added. Changes (exe addresses; "found" = IDENTICAL in the map, found modified):

* **Helpers**: `cat_aiuRoundToUdword` (the exe's integer conversion `fadd qword [0x0072a458]` = 1.5 x 2^52, low dword:
  round to nearest, ties to even; HW1 truncated), `cat_aiuRemoveUntargetableTargets` (the in-place compaction of
  `aiuWrapAttack` / `aiuWrapSpecial`).
* **Macros** (AIUtilities.h): `aiuShipIsntSelectable` = disabled or crazy (no `SOF_Hide` test), hyperspaced out, or
  `specialFlags2 & SPECIAL_2_CatBeingBuilt` (aiuFilterSelectableShips 0x0041abe0, aiuWrapDock 0x0041ef70,
  kasfDockSupportWith 0x004945a0: `flags & 0xa00000`, `ShipAlreadyHyperspaceOut`, `+0x1cc & 8`);
  `aiuShipIsntTargetable` = `SOF_Hide | SOF_Hyperspace` or `!cat_shipIsTargetableBy(player, ship)` (aiuWrapAttack
  0x0041ed9a, aiuWrapSpecial, aiuFindCaptureTarget: `flags & 0x2008000`, `cat_shipIsTargetableBy` 0x0052a080), where
  HW1 asked the proximity sensors. This is the "aiuWrap* on cat_shipIsTargetableBy" item and `aiuFindCaptureTarget`'s
  targetable test (through `aiuEnemyShipIsVisible`).
* `aiuRescueShip` 0x0041a700 (found): a single-player `ScriptTeam` attacks with its own tactics (AITeam +0x74 in the exe,
  `kasTactics` in the tree; `aitCreate` 0x00416810 initialises it to 2 in multiplayer and 1 in single player, the tree
  to `Neutral`: producer 3's AITeam), others `Aggressive`.
* `aiuShipNotGoodAgainstFighters` 0x0041aa90: the 8 exe types (b/k/t heavy cruisers, ion cannon frigates, standard
  destroyers, bIonArrayFrigate).
* `aiuIsShipDangerous` 0x0041ac40: `CLASS_MonsterShip` and `CLASS_Micro` dangerous; both DFG frigates excepted.
* `aiuRateShip` 0x0041acb0: strength only for Sect and Beast ships, rounded to nearest; value 5,500 for
  `CLASS_Mothership` or `trFloatingCity`, the build cost for Sect/Beast, 1 for any other race.
* `aiuEnemyShipsInMothershipBlob` 0x0041ad40 (found): no crystals.
* `aiuMakeShipsOnlyDangerousToMothership` 0x0041ae00: frigates except both support frigates; `tMinelayerCorvette` or
  more than 3 corvettes; both attack bombers with 6 or more ships.
* `aiuFindUnarmedUndefendedEnemyShips` 0x0041b690 (found): neither salvage corvette, crystal or slipgate protects a
  blob. `aiuFindUnarmedEnemyShips` 0x0041b7d0: both races' support frigates and salvage corvettes, neither grav well
  generator. `aiuFindFighterVulnerableEnemyShips` 0x0041b900: no DFG frigate, grav well generator or mothership of
  either race.
* `aiuDestinationNotNearOtherMothership` 0x0041bb80 (found): by class (Mothership, Carrier).
* `aiuFindSlowestShipMaxSpeed` 0x0041bcf0: times `cat_shipGetSpeedMultiplier`.
* `aiuFindClosestVulnerableGoodGuyShips` 0x0041be90 (found, via the inlined `aiuShipIsVulnerableGoodGuy` /
  `aiuShipIsLargeVulnerableGoodGuy`): both races' support frigates and salvage corvettes.
* `aiuFindHarassTarget` 0x0041c620 (found): the near-mothership exclusion is by `CLASS_Resource`.
* `aiuFindCaptureTarget` 0x0041cb50: no mothership exclusion; visibility as above.
* `aiuFindClosestEnemyShipWorthCapturing` 0x0041c990: identical; the exe returns an uninitialised local when nothing is
  found; the tree keeps NULL (`CAT-FIX`).
* `aiuFindDecloakedShipInSphereOfInfluence` 0x0041d160: any visible enemy ship in the sphere that is not fully cloaked.
* `aiuShipIsResourcingOrSomething` 0x0041dd50 (found): `Ship.catResourcing` (+0x1D8).
* `aiuFindBestResource` 0x0041dd90 (found): a resource with attribute 0x200 (`CAT_ATTRIBUTES_KillerCollision`) is never
  chosen.
* `aiuFindMinimumGunRangeSquared` 0x0041e2d0 (found): the 0.10-0.25 factor for `tMissileDestroyer`, `sDreadnought`,
  `sDestroyer`. `aiuShipsInGunRangeOfTargets` 0x0041e3a0 (found): skips both races' cloak and grav well generators and
  both DFG frigates.
* `aiuUnitCapCanBuildShip` 0x0041e500: the player's race range `FirstShipTypeOfRace .. LastShipTypeOfRace` (last
  excluded, as the exe's loop); Workers / `tResourceCollector`, Processors and both support frigates in the being-built
  switch (0x0041e71c); the class cap only when it is not -1 (0x0041e7bf).
* `aiuFindArmadaTarget` 0x0041e860: the mothership fallback needs a live mothership and its blob and targets the
  mothership alone (HW1's postfinal "ships within 9000" gone). The exe fills a caller-owned buffer (aiaArmada 0x00401120):
  re-targeted, see CAT-TODOs.
* `aiuAttack` 0x0041ea60: returns its duplicate (the exe never frees it here), homogenous attack = "one target:
  `aiuWrapAttack`; then `aiuWrapAttack`" with **no `aiuFindPriorityTarget`** (absent from cataclysm.exe; it drew from
  stream 6 on every homogenous attack). Coupled: **`aimProcessAdvancedAttack` 0x00408ca0** (AIMoves.c) now passes the
  move's own target list, stores the returned duplicate and frees the original, as the exe does (HW1's postfinal passed a
  duplicate and leaked the move's list).
* `aiuWrapAttack` 0x0041ed00: untargetable targets dropped in place (HW1 refused the whole attack); every `sLeech` of the
  attackers is taken out of the attack and, unless `cat_leechIsLeeching` 0x005b7ce0, given `clWrapSpecial` on the
  targets.
* `aiuWrapMove` 0x0041eea0: `MakeShipsMobile(filtered, 0)` before `clWrapMove`. `aiuWrapSpecial` 0x0041f2e0:
  untargetable targets dropped, no order when none are left. `aiuWrapDock`, `aiuWrapProtect`, `aiuWrapCollectResource`,
  `aiuWrapSetTactics`, `aiuWrapSetKamikaze`, `aiuWrapHalt`, `aiuWrapFormation`: the Cataclysm selectable filter.
* **`cat_aiuWrapCapture` 0x0041f3d0** (new): the selectable ships get `cat_clWrapSpecialTarget(..., NULL, flag)` unless
  `dontIssue`; its caller `aimProcessActiveCapture` 0x0040b3f0 is producer 2's (AIMoves.c).
* `aiuFindCloakersInEnemyShipsIAmAwareOf` 0x0041f770 (found): `kCloakGenerator` / `tCloakGenerator` only.
* `aiuUpdateKnowledgeOfEnemyShips` 0x0041f810 (found): crystals and slipgates not remembered.
* `aiuFillInArrays` 0x0041f900: an enemy ship counts when not fully cloaked, detected (`cat_shipDetectedByPlayer`),
  targetable (`!cat_shipIsUntargetable`) and not a crystal. `aiuFillInAIBlobArrays` 0x0041faf0: the emptied enemy
  selection is freed (the tree leaked it). `aiuRateBlobs` 0x0041fd90: Cataclysm's `aiuRateShip`, the mothership value
  rounded to nearest (x1.5), resourcers by `CLASS_Resource`.
* The other 43 IDENTICAL bodies compared: identical (inlined helpers agree).

All HW1 aliases in AIUtilities.c's changed functions replaced by the exe's types; aliases remain only in tree-only HW1
helpers with no exe counterpart (the P2 swarm helpers; `aiuFindPriorityTarget` is removed [audit: was "now uncalled"; the body is deleted, see "Files changed"]) and in
`aiuFindNearbyEnemyShips`'s comment.

## 4. aitrack-aivar (src/Game/AITrack.c; AIVar.c unchanged)

AITrack.c's 19 and AIVar.c's 13 exe functions (all IDENTICAL in the map) compared:

* `aitrackHeadingFunc` 0x00418de0 (found): a zero desired heading returns TRUE at once (0x00418e34, the double 0.0 at
  0x0072a4e8).
* `aitrackSteadyShipDriftOnly` 0x00419a00, `aitrackSteadyShipAnywhere` 0x00419b60, `aitrackSteadyShip` 0x00419c70:
  identical to HW1; **GoK's gun-gimbal reset** (`gunResetGimbleGuns`, active unless `/vanilla`) removed, as the exe has
  none (they return TRUE once steady). `gunResetGimbleGuns` is now uncalled (Gun.c, not this scope).
* The other 16 AITrack bodies and all 13 AIVar bodies: identical (the x87 evaluation, `fptan` and the table `fsqrt`
  aside: section 1).

## 5. aimdock (src/Game/AIMoves.c `aimProcessDock` 0x00409350)

The rest of the body compared with the exe and kept: the "dockdup" copy, the processing flag (move +0x164),
`TEAM_DOCKING` (team +4 bit 0x4000) set and cleared, and the wait loop that drops a ship with no host that is not hidden
(`SOF_Hide` 0x8000) or a stay-till-launch ship (`specialFlags` 0x1000) waiting for a soft launch. The exe still sums the
team's health (0x004093a1) and discards it, as the tree does. The `CAT-TODO(stage 4 AI)` closed.

## 6. gates (final tree, plain harness)

**Builds**: `tools/build.sh --debug` and `--release` rc 0, 0 warnings / errors under `-Werror` (after every subsystem
and on the final tree). `tools/m2_layout32.sh`: OK (Ship 0x63C and the other pins unchanged: no struct changed; one
`#define` added, `CAT_SHIPFLAG_KasMines`). `python3 -I tools/m2_save32_gen.py --check`: current. The files that call
new prototypes were compiled once with `-Werror=implicit-function-declaration` (AIShip.c, AIUtilities.c): clean.

| run | build | result |
|---|---|---|
| `r7a-final-replay-a`, `-b` | debug ASan | `oracle replay done ticks=9601 packets=4800 ships=82`, ASan 0, rc 0, 33 s; **byte-identical**, sha256 `bc9b8a5d0cf0f0ec`; 0 AIShip.c UBSan reports |
| `r7a-final-rel-replay` | release | replay done, rc 0, 7.5 s; against `r7a-final-replay-a` every simulation field and rng6 IDENTICAL over 9601 ticks / 4800 packets |
| `r7a-final-mission-a`, `-b` (`tools/oracle/mission_test.sh --ticks 6000 --no-perturb-run`) | debug ASan | MISSION TEST PASS, byte-identical, sha256 **`7c074b0b122f2fb5`**, ASan 0; `hwo_compare` against `r6-integrate-mission-a` (49240abcf9f0ed97): IDENTICAL, 6001 ticks (header build identity 9e20b5bd vs a68b3ef0 only) |
| `r7a-final-dock` | debug ASan | dock self-test done, `docktest: done ... result=PASS` (the R6 line exactly), ASan 0, **0 AIShip.c UBSan reports** (r6-integrate-dock: 1) |
| `r7a-final-fe` | debug ASan | `Main_game_screen`, ASan 0 (rc 124 at the 120 s timeout, as designed) |
| `r7a-final-sp` | debug ASan | universe running, univUpdateCounter 3,712 at 360 s, ASan 0, no unsuppressed leak |
| development runs | debug | `r7a-base-*` (before), `r7a-x87-*` (after the rounding fix), `r7a-x87sw-replay` (default path with the switch compiled in: `b7e87c4cd15694d2`, = before the switch), `r7a-fsqrt-exp-replay` (the experiment), `r7a-aiship1-replay`, `r7a-ai1-*`, `r7a-dbg-dump1..5` (temporary instrumentation, removed) |

**Mission01 stream** (each stream-moving step; baseline `r6-integrate-mission-a` 49240abcf9f0ed97):

| step | sha256 (16) | fields moved |
|---|---|---|
| base 4c31f6e | `7c074b0b122f2fb5` | none (header build identity only) |
| x87 rounding fix | `7c074b0b122f2fb5` | none: nothing is built externally in the headless Mission01 run |
| AIShip / AIUtilities / AITrack / aimProcess* ports (final) | `7c074b0b122f2fb5` | none: the computer player runs (rng6 advances every tick, 6,000 changes) but no ported path changes its draws or any ship within 6,000 ticks (no movement, attack or blob rating that the changes touch occurs in Mission01's opening) |

The task expected the AI ports to move the stream; in this run they do not, and the replay (where player-ordered ships
fly through `aishipFlyToPointAvoidingObjsFunc`) is where the AIShip port shows: positions identical to GOG through 179.

**Replay comparison** (final, `r7a-final-replay-a`, the `docs/m2-r6-oracle-mode-a.md` arguments):
`python3 -I tools/oracle/hwo_compare.py original/oracle/oracle01-rec-20261017-140428/oracle01-rec.hwo <runs>/r7a-final-replay-a/run.hwo --from-tick 1 --to-tick 9600 --fields rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real --json <runs>/r7a-final-replay-a/compare.json`
-> first divergent tick 161 rng6 (excluded); without rng6 (`compare-sim.json`) -> **first divergent tick 177 ships_vel**
(last equal 176). Per field: ships_vel 177, **ships_pos 180** (x87 step: 178), **univcheck 180** (179), resources 243,
ships_misc 871, rng10 1408 (1376), bullets 1917, ships_health 1934, ships_n 2048; missiles, derelicts, dt, total, real
IDENTICAL. Sync counters: randSyncErr 3857 (4037; first frame 1425, was 1393), univSyncErr 4702 (4703; first frame 197,
was 195), blobSyncErr 4746 (stock), pktSyncErr 0. Against the x87-step replay the first difference is tick 177
`ships_rot` (the turn of the flying Workers through the reworked avoidance), and our packet-89 univcheck now equals the
GOG playback's (1234478809; R6 noted 1234478826 vs it).

## Delta-map review entries

139 entries `[review: M2-R7 oracle-rung-aiship]` in `ghidra/delta_map_review.json` (105 new with sample
`port-R7-oracle-rung-aiship`, 34 existing ones with the note appended), merged with `python3 -I tools/delta_map.py merge
--done m2r7:oracle-rung-aiship` (139 records changed, nothing else); `docs/m2-r1-function-deltas.md` regenerated
(`delta_map.py report`). Counts: IDENTICAL 2,942 -> 2,922, MODIFIED 1,394 -> 1,414, REWRITTEN 84, reviewed 2,816 ->
2,921. Stage 4 now: 366 / 111 / 2 (`tools/m2_stage_counts.py`). IDENTICAL found modified (now MODIFIED): rowSignalShipToGetOutOfWayOfMe,
aishipGuideMine, aitrackHeadingFunc, aiuRescueShip, aiuEnemyShipsInMothershipBlob, aiuFindUnarmedUndefendedEnemyShips,
aiuDestinationNotNearOtherMothership, aiuFindClosestVulnerableGoodGuyShips, aiuFindHarassTarget,
aiuShipIsResourcingOrSomething, aiuFindBestResource, aiuFindMinimumGunRangeSquared, aiuShipsInGunRangeOfTargets,
aiuFilterSelectableShips, aiuWrapDock, aiuWrapProtect, aiuWrapCollectResource, aiuWrapSetTactics, aiuWrapSetKamikaze,
aiuWrapHalt, aiuFindCloakersInEnemyShipsIAmAwareOf, aiuUpdateKnowledgeOfEnemyShips. MODIFIED found identical after
reading (inlining only): aiuFindBestUnarmedEnemyShip, aiuFindBestFighterVulnerableEnemyShip. Not in the map (no HW1
counterpart): `cat_clExternalBuildUpdate` 0x0044dfc0, `cat_aiuWrapCapture` 0x0041f3d0, `fsqrt` / `fmathInit`.

## CAT-TODOs

Closed: `CAT-TODO(stage 4 AI)` in AIMoves.c `aimProcessDock` (the rest of the body compared and kept). Re-targeted
(one line, owner first, to producer 2 who owns the callers): AIUtilities.c `aiuSwarmDock` / `aiuFindSwarmerTarget` /
`aiuFindSwarmTargets` (no exe counterparts; remove with the P2 swarm moves) -> `CAT-TODO(M2-R7 AI orders)`. Added:
`CAT-TODO(M2-R7 AI orders)` aiuFindArmadaTarget's caller-owned buffer (aiaArmada 0x00401120), `CAT-TODO(M2-R7 AI
orders)` aiuShipsCloseToEnemyMothership (no exe counterpart; callers absent). No `CAT-TODO(stage 4 ...)` or
`CAT-PROVISIONAL` is left in AIShip.c, AIUtilities.c, AITrack.c, AIVar.c; 20 `CAT-TODO(stage 4 ...)` remain in the
tree, all in other producers' files (AIOrders 7, AIResourceMan 4, AIHandler 2, utility.c 2, AIFleetMan, AIPlayer,
Dock.c, Salvage.c, UnivUpdate.c 1 each). One `CAT-FIX` added (aiuFindClosestEnemyShipWorthCapturing's uninitialised
result kept NULL).

## Files changed

* `src/Game/CommandLayer.c`: `clExternalBuildAdvance` single rounding (section 1).
* `src/Game/FastMath.c` (new, REULA header), `src/Game/FastMath.h`, `src/Game/meson.build`, `src/SDL/main.c`: the
  `/oracleTableFsqrt` experiment switch (HWI-EXPERIMENT; default sqrtf).
* `src/Game/AIShip.c`: section 2. `src/Game/SpaceObj.h`: `CAT_SHIPFLAG_KasMines` (BIT5).
* `src/Game/AIUtilities.c`, `src/Game/AIUtilities.h`: section 3 (macros, `cat_aiuWrapCapture`, `aiuFindPriorityTarget`
  removed). `src/Game/AIMoves.c`: `aimProcessAdvancedAttack` ownership (coupled to aiuAttack), `aimProcessDock` tag.
* `src/Game/AITrack.c`: section 4.
* `ghidra/delta_map_review.json`, `ghidra/delta_map.json`, `docs/m2-r1-function-deltas.md`: reviews.
* `docs/m2-r7-oracle-rung-aiship.md`: this report.

## What is left (owners)

* **M2-R8 (simulation, the user's call first)**: the next oracle rung, tick 177: decision 23 (the table `fsqrt`) and,
  with it, the exe's x87 shape of the vector helpers on the flight path (`vecNormalize` 0x00550770, `vecCapVector`
  0x00550950 ...: double magnitude into the table, double reciprocal, one rounding per component), then
  `aitrackHeadingFunc`'s intermediates and `physUpdateObjPosVelShip`'s rotation (`fsin`/`fcos`, `fptan` in
  `aitrackHeadingWithBankPitchFunc`). Confirmation target: `(c20420c2, c110b4d6, 41d3f9da)` at tick 177.
* **Oracle tooling**: the DLL's coordsys offset (decision 48) would show the rotation divergence directly (tick 171
  expected); a per-ship dump switch for the engine (this round's instrumentation was temporary).
* **M2-R7 producer 2 (AI orders)**: aiaArmada's selection buffer for `aiuFindArmadaTarget`; `aimProcessActiveCapture`
  0x0040b3f0 calling `cat_aiuWrapCapture`; the swarm helpers and `aiuShipsCloseToEnemyMothership` with their callers;
  the single-player ScriptTeam tactics choice (`team +0x74`) recurs in the exe's AI moves.
* **M2-R7 producer 3 (AI player / teams)**: AITeam's Cataclysm layout (0x280 bytes; `aitCreate` 0x00416810 sets the
  tactics word to 2 in multiplayer, 1 in single player, the tree's `kasTactics` to Neutral).
* Gun.c: `gunResetGimbleGuns` is now uncalled (GoK feature; `/vanilla` still sets `vanillaGunsMode`).
