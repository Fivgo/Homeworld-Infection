# Audit: M2-R7 oracle-rung-aiship

Auditor: M2-R7 adversarial auditor 1. Base `4c31f6e`; the port is the whole working tree on top of `a68b3ef` (the two
commits after the base, `32291b9` and `a68b3ef`, are the orchestrator's: workflow script and STATE.md run id, not port
work). Producer report: `docs/m2-r7-oracle-rung-aiship.md`. Every engine run went through `tools/m2_harness.sh` on the
plain harness (silent, headless, `--bin` pointing at the audit build). Ghidra was not run; the exe was read with
`tools/m2_fdis.py`, `tools/m2_fdec.py` and `tools/m2_exedata.py`.

## 1. Clean build

`rm -rf original/scratch-m2/build-audit-r7-oracle-rung-aiship`, then exactly as `tools/build.sh`:
`meson setup --buildtype=debug -Db_sanitize=address,undefined <dir>/debug` and
`meson setup -Doptimization=2 -Ddebug=false -Db_sanitize=none <dir>/release`, `meson compile` each (`werror=true` from
the top-level meson.build). Both rc 0, **0 warnings** (grep of both compile logs). Sizes: debug `hwi-engine`
87,371,992 B, release 5,383,064 B. Recompiled after the audit's comment fixes: both rc 0, 0 warnings.
`tools/m2_layout32.sh`: OK (Ship 1596 = 0x63C, Player 25904 = 0x6530, every pin holds).

## 2. Runs (audit build, plain harness)

| run | result |
|---|---|
| `aud7a-replay-a`, `-b` (`--mode replay --timeout 600`, debug ASan) | `oracle: replay done ticks=9601 packets=4800 ships=82 randSyncErr=3857 univSyncErr=4702 blobSyncErr=4746 pktSyncErr=0`, rc 0, 32.6 s, ASan 0; **byte-identical** (`cmp`), 2,630,923 B, sha256 `bc9b8a5d0cf0f0ec18ed...` = the producer's `r7a-final-replay-a/b`; 0 AIShip.c UBSan reports |
| `aud7a-replay-rel` (release) | rc 0; against `aud7a-replay-a` every simulation field plus rng6, ships_rot and players IDENTICAL over 9,601 ticks / 4,800 packets (only `rng_all` differs, from 2061: the sound streams, an excluded field) |
| `aud7a-mission-a`, `-b` (`tools/oracle/mission_test.sh --bin <audit debug> --label aud7a-mission --ticks 6000 --no-perturb-run`) | MISSION TEST PASS, byte-identical, 1,476,370 B, sha256 **`7c074b0b122f2fb5ecffd8d401b3df300c29dace5e46d56c8c86ec14920898e2`**, ASan 0 both; `hwo_compare r6-integrate-mission-a/run.hwo aud7a-mission-a/run.hwo`: IDENTICAL, 6,001 ticks, header build identity only (9e20b5bd vs a68b3ef0) |
| `aud7a-dock` (`--mode dock --timeout 600`) | `docktest: done fighters=6 outside=6 corvettes=2 carrierleg=2 processorleg=1 RUs=1850 jackPausedTick=1024 result=PASS` (the `r6-integrate-dock` line exactly), ASan 0, **0 AIShip.c UBSan reports** (r6-integrate-dock: 1 "member access within null pointer of type 'struct Ship'"): the AIShip.c:1467 kind is gone |
| `aud7a-fe` (`--mode fe --timeout 120`) | `Main_game_screen`, ASan 0, rc 124 at the timeout (as designed) |
| `aud7a-sp` (`--mode sp --timeout 360`) | universe running, univUpdateCounter 3,712 at 360 s (the producer's number), ASan 0, no unsuppressed leak |

### 2.1 hwo_compare against the GOG recording (the `docs/m2-r6-oracle-mode-a.md` arguments)

```
python3 -I tools/oracle/hwo_compare.py original/oracle/oracle01-rec-20261017-140428/oracle01-rec.hwo \
    <runs>/aud7a-replay-a/run.hwo --from-tick 1 --to-tick 9600 \
    --fields rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real \
    --json <runs>/aud7a-replay-a/compare.json
  FIRST DIVERGENT TICK 161 (last equal tick 160); rng6, CAND is 1 draw(s) behind REF          <- excluded (decision 47)
... the same without rng6, --json <runs>/aud7a-replay-a/compare-sim.json
  FIRST DIVERGENT TICK 177 (last equal tick 176); 1 field(s) differ, likely origin: ships_vel
    ships_vel REF 740322b034279499  CAND 445c0bd4842e549c
  FIRST DIVERGENT PACKET 46 (REF tick 93, CAND tick 93): blobcheck 1785610478 vs 1785610479   <- stock (decision 47)
  CAND sync errors: randSyncErr 3857, univSyncErr 4702, blobSyncErr 4746 (first frames rand 1425 univ 197 blob 109)
```

Per field (`--fields <one>`): rng10 1408, rng6 161 (excluded), ships_n 2048, ships_misc 871, ships_pos 180, ships_vel
177, ships_health 1934, bullets 1917, resources 243, univcheck 180; missiles, derelicts, dt, total, real: no divergent
tick in 1..9600. All equal to the report's final numbers.

**Before / after** (rerun of hwo_compare on the producer's kept runs): `r7a-base-replay` (sha `9f261bc97cc24bd8`)
first divergence **136 ships_health**, sync 4037 / 4703 / 4746 (first frames 1393 / 195 / 109) = R6; `r7a-x87-replay-a`
(`b7e87c4cd15694d2`, = `r7a-x87-replay-b` and `r7a-x87sw-replay`) first divergence **177 ships_vel**, ships_pos 178,
univcheck 179, rng10 1376, ships_health 1934; `r7a-fsqrt-exp-replay` (args carry `/oracleTableFsqrt`) still 177
ships_vel, rng10 1384, randSyncErr 3966. `r7a-x87-replay-a` against `aud7a-replay-a`: first difference tick 177
`ships_rot`, and packet 89's univcheck is now 1234478809 (the GOG playback's value in R6). Every claim matches.

### 2.2 The next divergence (tick 177, ship 35): the hash inversion re-checked

The producer's temporary dumps are kept in `r7a-dbg-dump1..5t/run.log`. A script (scratchpad `velhash.py`) rebuilt
the `ships_vel` FNV-1a-64 of tick 177 exactly as `OracleDump.c` does (per live ship in list order: u16 shipID, then 3 x
u32 velocity bits, byte-wise) from the 48 ships of the dump: it gives `445c0bd4842e549c` = our run.hwo; with only ship
35's velocity replaced by the claimed GOG value `(c20420c2, c110b4d6, 41d3f9da)` it gives **`740322b034279499` = the
GOG recording's** hash at 177. The inversion is confirmed. Ship 35 is type 14 (sWorker), player 0, order 5; its
rotation speed at 171 is `3f7b79a6` (sqrtf, `r7a-dbg-dump5`) vs `3f7b7ad1` (table, `r7a-dbg-dump5t`), as stated. The
table run's velocity is `(c20420c3, c110b4d2, 41d3f9dc)`: GOG minus it is -1, +4, **-2** raw ulps (the report said
+2; fixed, finding 3).

### 2.3 The x87 fix judged against the disassembly

`tools/m2_fdis.py 0x0044dfc0`: mode 2 at 0x0044dfe5 and the mode-0 branch at 0x0044e05a (taken when `catBuildProgress`
< the double 1.0 at 0x0072a548, read: `0x3ff00000:00000000`) are `fld dword [esp+0x14]; fsub dword [ecx+0x3e8]; fmul
dword [edx+0xa4]; fadd dword [ecx+0x170]; fst dword [ecx+0x170]; fcomp dword [edx+0xa4]; test ah,0x41` and the clamp
store. Every operand is a dword (float) load; the intermediate stays on the FPU and is rounded once by the `fst`; the
`fcomp` compares the unrounded st0 (the clamp fires only when C0 and C3 are clear: st0 > maxhealth). The GOG stream's
control word 0x027f has PC = 10b (53 bits), so each x87 step rounds to 53 bits as an SSE2 double does (no exponent
range issue at these magnitudes). The tree's `clExternalBuildAdvance` (CommandLayer.c) evaluates the same expression in
double, stores `(real32)health` once and clamps on the double sum: faithful, with a `CAT:` comment.

**sqrtf stays the default**: FastMath.h's `fsqrt` / `fmathSqrt` take the table path only while `catFsqrtTable`
(FastMath.c, initialised FALSE) is set, and only `/oracleTableFsqrt` (main.c, inside `#if HWI_ORACLE`, `entryVr` to
TRUE) sets it. The default replay with the switch compiled in is byte-identical to the one before it existed
(`r7a-x87sw-replay` = `r7a-x87-replay-a` = `b7e87c4cd15694d2`). One experiment this round (decision 23).

## 3. Bodies compared with the exe (cataclysm_gog_r6 helpers)

29 bodies, all at the exe's (two carry a wrong address in a comment, finding 1):

* `cat_clExternalBuildUpdate` 0x0044dfc0 (above).
* `aishipFlyToPointAvoidingObjsFunc` 0x00414610: the small-target pass skip (0x00414c8a: `ShipStaticInfo+0x48` =
  `originalCollSphereSize`, pinned by the 0x34 / 0x4C offset asserts, against 500.0 at 0x0072a4ac; pass 0 reads blob
  +0x94, pass 1 +0x98, the link skip jumps to the pass counter 0x00415916); the 0x608 block 0x00414e0e .. 0x00414ea0
  (both directions, CLASS_Component = 10, clampInfo at +0x1bc, single player 0x88/0x62 and 0x12/0x8c); the cloak test
  0x004154c2 (`cat_shipIsCloakedFully` reads only header flags, so calling it on a non-ship is safe; non-ships ignored,
  ships only if `sub_53F9F0` detects them); the 30 m pass-on-right (900.0 at 0x0072a4cc); the salvager pointer
  compare.
* `aishipGuideMissile` 0x00415e60, `aishipFlyMineToTarget` 0x00415fa0 (cloak tests; SOF_Disabled = 0x800000 at +0x14),
  `aishipmineaquiretarget` 0x004162c0 (race 6 outside single player, catFlags18 0x20 = `CAT_SHIPFLAG_KasMines`, set by
  `kasfShipsEffectedByMines` 0x0049a080, cloak / Leech type 3 / mimic bit 0x8), `aishipGuideMine` 0x00416410 (state
  `formationinfo (+0x244) ? 4 : 2` at 0x00416783; MINE_WALL_FORMATION 4, MINE_SEARCH_AND_STOP 2).
* AIUtilities.c: `aiuFindCaptureTarget` 0x0041cb50 (no mothership exclusion; visibility = flags & 0x2008000, then
  `cat_shipIsTargetableBy`, then the aiblob visibility; the HW1 `ShipPtr[0]` quirk kept as the exe has it),
  `aiuWrapAttack` 0x0041ed00, `aiuWrapSpecial` 0x0041f2e0, `aiuWrapMove` 0x0041eea0, `cat_aiuWrapCapture` 0x0041f3d0,
  `aiuAttack` 0x0041ea60 (the exe really calls `aiuWrapAttack` twice for one target: 0x0041ebff then 0x0041ec0d),
  `aiuRateShip` 0x0041acb0 (races 0/1, class 0 or type 0x62 -> 0x157c = 5,500, `fadd qword [0x0072a458]` =
  `0x43380000:00000000` = 1.5 x 2^52), `aiuRateBlobs` 0x0041fd90 (`fild qword; fmul qword [0x0072a518]` = 1.5, magic
  add), `aiuShipNotGoodAgainstFighters` 0x0041aa90 (switch byte table decoded: 0x2c 0x2e 0x42 0x45 0x49 0x50 0x53
  0x5a), `aiuIsShipDangerous` 0x0041ac40 (jump table: classes 1/2/3/5/6/9/11 TRUE, 4 the DFG test 0x2d/0x4e, 0/7/8/10
  FALSE), `aiuUnitCapCanBuildShip` 0x0041e500 (race range, last excluded), `aiuFindArmadaTarget` 0x0041e860 (fallback
  from 0x0041e9fa: mothership not SOF_Dead 0x800, blob non-NULL, the mothership alone), `aiuFillInAIBlobArrays`
  0x0041faf0 (memFree of the emptied selection at 0x0041fc3f).
* `aimProcessDock` 0x00409350 and `aimProcessAdvancedAttack` 0x00408ca0 (passes the move's list, stores aiuAttack's
  result, frees the original; aiuAttack has no other caller in the tree).
* AITrack.c: `aitrackHeadingFunc` 0x00418de0 (zero-heading return), `aitrackSteadyShipDriftOnly` 0x00419a00,
  `aitrackSteadyShipAnywhere` 0x00419b60, `aitrackForceSteadyShip` 0x00419cd0, `aitrackForceZeroVelocity` 0x00419d30
  (no gimbal reset in the exe: the GoK removal is right). AIVar.c: `aivarCreate` 0x00420420 (64 / +32 slots, label 47),
  `aivarFind` 0x00420510, `aivarDestroy` 0x00420560: identical.
* Every ship type the port names was compiled against the tree's enums (scratchpad `enum.c`): the hex values in the
  CAT comments all match (e.g. dRKO 0x6d, dBeastMothership 0x82, kCloakGenerator 0x3f, tCloakGenerator 0x4d,
  dSlipgateNode 0x88, dEcclesiam 0x8c).

## 4. Diff review (`git diff 4c31f6e`)

* Scope: CommandLayer.c (the x87 item), FastMath.c/.h + meson.build + main.c (the one fsqrt experiment, marked
  HWI-EXPERIMENT, behind a default-off switch), AIShip.c, AIUtilities.c/.h, AITrack.c, SpaceObj.h (one `#define`),
  AIMoves.c (`aimProcessDock`, in scope; `aimProcessAdvancedAttack`, producer 2's file but coupled to aiuAttack's
  ownership change and stated in the report), the delta map files and the report. No change outside the scope.
* Every changed function (53, listed by a body-diff script) has a M2-R7 review entry; the 22 IDENTICAL ones found
  modified are MODIFIED in both the review file and the merged map; the three `aitrackSteadyShip*` stay IDENTICAL
  (the exe equals HW1; the removed code was GoK's) with review entries; `aiuFindClosestEnemyShipWorthCapturing` stays
  IDENTICAL with a `CAT-FIX` (the exe returns an uninitialised local). 139 tagged entries (105 new, sample
  `port-R7-oracle-rung-aiship`; 34 notes appended); no other entry changed or removed; review class = map class for
  all 139; `done_subsystems` ends with `m2r7:oracle-rung-aiship`. `tools/m2_stage_counts.py`: stage 4 366 / 111 / 2,
  totals IDENTICAL 2,922, MODIFIED 1,414, REWRITTEN 84 (the report's numbers).
* Every Cataclysm change carries a `CAT:` comment with the exe function and address.
* CAT-TODOs: none left in AIShip.c, AIUtilities.c, AITrack.c, AIVar.c naming stage 4 (or CAT-PROVISIONAL); 20
  `CAT-TODO(stage 4` left tree-wide (22 at the base: AIMoves.c's closed, AIUtilities.c's re-targeted). The three new
  ones are one line each, owner first: `CAT-TODO(M2-R7 AI orders): ...` (AIUtilities.c 1265, 3797, 4043).
* Stream: the Mission01 stream did not move at any step (checked above against `49240abcf9f0ed97`), as the report says.

## 5. Findings

1. **Two wrong exe addresses in comments (fixed).** `aishipGuideMine`'s state choice was cited "at 0x0041658b" (AIShip.c,
   the report's table, the review entry and the merged map); 0x0041658b is inside `fcomp` at 0x00416587, and the
   `neg / sbb / and 2 / add 2` on `formationinfo` (+0x244) is at 0x00416783. `aiuFillInAIBlobArrays`' memFree was
   cited at 0x0041fb6b (inside the `mov` at 0x0041fb68); the call is at 0x0041fc3f/0x0041fc40. A scan of all 98 exe
   addresses in the added lines (each checked against the instruction starts of its function) found only these two
   point addresses wrong (0x004151d0 and 0x00416340 are approximate range bounds, left).
2. **"aiuFindPriorityTarget, now uncalled" (report section 3) is wrong (fixed)**: the body is deleted (the "Files
   changed" list says so; `grep` finds only the CAT comments).
3. **The fsqrt-experiment ulps were mis-signed (fixed)**: -1/+4/+2 in the headline and section 1.4; with the
   convention of the sqrtf triple (GOG minus ours, raw bits) the table run is -1/+4/**-2**.
4. Note (no change): the three new `CAT-TODO(M2-R7 AI orders)` hand work to producer 2; the R7 integrate exit requires
   no `CAT-TODO` naming M2-R7, so producer 2 must close or re-target them.
5. Note (no change): removing GoK's gun-gimbal reset from the three `aitrackSteadyShip*` (the exe has none) leaves
   `gunResetGimbleGuns` (Gun.c) uncalled and changes GoK's default (non-`/vanilla`) behaviour; this follows "Cataclysm
   behaviour wins" and the report states it.
6. Note (no change): `cat_aiuWrapCapture`'s third parameter is `bool32 flag` where the CET signature says
   `SpaceObjRotImpTarg *target`; the exe passes it straight to `cat_clWrapSpecialTarget`'s 4th argument, which the tree
   already declares `bool32 flag`: consistent with the tree (decisions 18/24/54).

## 6. Precision

Sampled checks: 28 report claims (build, the two replays and their hash, the first divergence and its field, rng6 161,
the per-field ticks, the identical fields, the sync counters, the first packet, the before / x87 / switch / experiment
runs, the x87 rounding claim, sqrtf default, the hash inversion, the ulps, the rotation speeds, release vs debug, the
mission stream, dock, fe, sp, layout32, stage counts, review / merge counts, the aiuFindPriorityTarget statement, the
CAT-TODO census, the x87-vs-final ships_rot / packet-89 claim) + 29 body comparisons = **57**. Wrong: the ulps sign,
the aiuFindPriorityTarget statement, and the two cited addresses (bodies right, citation wrong) = **4**.
**Precision = 53 / 57 = 0.93.**

## 7. Fixes (4, all comments / docs; build green after them)

* `0x0041658b` -> `0x00416783`: src/Game/AIShip.c, docs/m2-r7-oracle-rung-aiship.md ("[audit: ...]"),
  ghidra/delta_map_review.json and ghidra/delta_map.json (the merged copy of the same note).
* `0x0041fb6b` -> `0x0041fc3f`: src/Game/AIUtilities.c.
* The ulps: docs/m2-r7-oracle-rung-aiship.md headline and section 1.4 ("[audit: ...]").
* aiuFindPriorityTarget "now uncalled" -> removed: docs/m2-r7-oracle-rung-aiship.md section 3 ("[audit: ...]").

## 8. Reference for the next port

Mission01 stream of the final pair (`aud7a-mission-a` = `-b`, audit debug build, 6,000 ticks):
**sha256 `7c074b0b122f2fb5ecffd8d401b3df300c29dace5e46d56c8c86ec14920898e2`**, IDENTICAL to the baseline
`r6-integrate-mission-a` (`49240abcf9f0ed97...`) in every field (header build identity only). oracle01 replay (debug):
`bc9b8a5d0cf0f0ec18ed822a5243493f76aecc8a130783c329cdec7628884155`, first simulation divergence tick 177 `ships_vel`.

Verdict: PASS-WITH-FIXES - replay IDENTICAL to the GOG recording through tick 176 with the tick-136 health divergence gone and tick 177 (ship 35's velocity, the turn's sqrt) localised with a verified hash inversion; bodies at the exe's; mission stream unchanged; 4 comment/doc errors fixed
