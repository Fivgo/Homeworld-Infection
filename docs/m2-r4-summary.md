<!-- done: exit-test, ports, deltamap, left, r5-plan, decisions -->
# M2-R4 summary: stage 2 part B (universe update, command layer, combat/docking/salvage, the Cataclysm-only ships, the rest + Player layout + oracle stage 2)

Status: **COMPLETE, 2026-10-11.** Branch `m2`, worktree `.claude/worktrees/m2`, workflow `cycle/workflows/m2-r4.js`.
The round ran as four workflow runs (`wf_985641f3-673`, `wf_20c186e0-be8`, `wf_6ec89407-0bf`, `wf_c1815ea8-981`): the
five-hour usage limit cut three producers off mid-port (combat, ships-b, rest-oracle), each cut was committed as an
unaudited WIP snapshot (`b39dea7`, `2f1a847`, `619ba34`) and the relaunched producer resumed from it; every finished
port was audited and checkpointed by its auditor. 11 agents per run (5 `src/` ports in sequence, 5 auditors, this
integrate step; the relaunches skipped the ports whose audit doc already had a verdict). Plan: docs/m2-r3-summary.md
"Plan for M2-R4". User decisions: cycle/STATE.md "## M2" (the 2026-10-10 faithfulness defaults are applied; the
oracle reference recording and the two saves still come later and did not block).

**Stage-2 exit test: met.** From a clean build on the plain harness, the debug `hwi-engine` (ASan + UBSan) runs
Mission01 headless for 6,000 universe ticks with ASan 0, and two such runs give byte-identical `.hwo` streams
(1,476,370 B, 6,001 `T` records, 154 ships); a run perturbed by one `gamerand()` is caught at the perturbed tick and a
flipped checksum bit at its tick and field. sp still reaches the running universe (1,248 ticks at the 340 s timeout,
as R3), fe still reaches `Main_game_screen`, the five self-tests give R3's numbers, the release oracle self-test says
`SELFTEST PASS` with 27 RNG streams, every quitting run returns rc 0, `tools/m2_layout32.sh` holds every ILP32 pin
(now `Player` 0x6530 and `Universe` 0x3D9BC too). Two provisional switches are still on: `CAT_HW1_MISSION_SCRIPTS 0`
(stage 6) and `CAT_EXTERNAL_BUILD_DRIVER 0` (the construction manager's external-build driver, stage 3).

## Exit test (this step, reproduced from a clean build at 127b5b9)

**Builds.** `tools/build.sh --debug --clean` and `--release --clean`: both rc 0, `[305/305] Linking target hwi-engine`,
0 `warning:` and 0 `error:` lines under `-Werror` (the only WARNING line is meson's sanitizer notice at setup).

| build | size | sha256 |
|---|---:|---|
| debug (ASan + UBSan) | 80,149,976 B | - |
| release (`-O2`, no sanitizers) | 4,934,904 B | `dcffa943…247b241` |

`tools/m2_layout32.sh`: OK. ILP32 sizes: `ShipStaticInfo` 1112, `GunStatic` 284, `CustShipHeader` 140, `polygonobject`
116, `texreg` 52, `SENTENCELUT` 16, `ClampInfo` 56, `StaticCollInfo` 112, `AlternativeShips` 168,
`MilitaryParadeCommand` 260, `ranstream` 20, `SaveChunk` 8, `particle` 256, `eboltSystem` 36, **`Player` 25904
(0x6530), `Universe` 252348 (0x3D9BC), `PlayerStats` 2040 (0x7F8), `TechStatics` 31412 (0x7AB4), `shipinprogress` 48
(0x30)**; the specifics asserts of all 45 ship files hold (R3: 24).

**Harness.** `tools/m2_harness.sh` on `original/scratch-m2/m2r2-harness/data` (the GOG install's own files, no CET, no
workarounds), offscreen video, dummy audio (docs/CONSTRAINTS.md 5), no input. The two binaries were copied to
`original/scratch-m2/m2r4-integrate/` first so that no rebuild could touch a running test. Logs are in
`original/scratch-m2/m2r2-harness/runs/r4-integrate-*/`.

| run | mode | rc / wall | milestone | first stop | sanitizers and checks |
|---|---|---|---|---|---|
| `r4-integrate-mission-a`, `-b` (`tools/oracle/mission_test.sh --ticks 6000 --perturb 3000 --copy-tick 4000`) | mission, 6,000 ticks, debug | **0** / 22.9 s and 21.9 s | **oracle mission run done** (`ticks=6000 records=6003 ships=154`) | none (rc 0) | **ASan 0** in both, no unsuppressed leak; 3,722 file loads each; `cmp` equal, **1,476,370 B, sha256 `06546872892ca92a`**; `hwo_compare.py`: `IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000)`. Against the audit's stream `r4ro-audit-fixed-mission-a` (built from 7d4cc37): IDENTICAL over all 6,001 `T` records, only the header's build identity differs |
| `r4-integrate-mission-perturb` | mission, 6,000, `--perturb 3000` | 0 / 21.8 s | oracle mission run done | none | ASan 0; `FIRST DIVERGENT TICK 3000`, 2 fields, likely origin `rng10`, `CAND is 1 draw(s) ahead`; events `game-new, perturb, quit` |
| copy of A with bit 0 of `ships_health` flipped at tick 4000 | | | | | `FIRST DIVERGENT TICK 4000`, 1 field, origin `ships_health`. **MISSION TEST PASS** |
| `r4-integrate-mission-rel` | mission, 6,000 ticks, **release** | 0 / 4.4 s | oracle mission run done | none | vs run A: `IDENTICAL: 6001 ticks` (header build flags 1fbfa32f vs 1afeb7ac): the -O2 build gives the same stream as the ASan build |
| `r4-integrate-sp` | sp, `--timeout 340` | 124 / 340.4 s (timeout) | **universe running**: last `debugScreenshots univUpdateCounter` line **1,248** (R3 `r3-integrate-sp`: 1,248); the animatic plays in real time first | none: still running at the timeout. 3,723 file loads; 33 Mission01 statics; 21 engine glows | **ASan 0**; no unsuppressed leak. UBSan: only the known non-fatal misaligned-member kinds of GoK's packed 64-bit structs (`fescreen` 101, `etgdecision` 21, `PATCH` 17 + 15, `etgfunctioncall` 10, `mhlocalbinding` 9, `etgconditional` 8, `etgbranch` 8, `fquant.c`) |
| `r4-integrate-fe` | fe, 40 s | 124 / 40.4 s | **`Main_game_screen`** | none: still running at the timeout | ASan 0; no unsuppressed leak; 232 file loads |
| `r4-integrate-statics` | statics | **0** / 11.5 s | ship statics self-test done | none | ASan 0, no leak. **143 of 145** ship types loaded, 2 not found (the disabled cloak-generator `.shp`), 27 other `.shp` (170 of 173) |
| `r4-integrate-statics-init` | statics + `/catShipStaticsStaticInit` | **0** / 12.2 s | ship statics self-test done | none | ASan 0, no leak. 143 of 145; **108 StaticInits ran** (`staticinit=next`), 35 types with the exe's NULL slot 1 |
| `r4-integrate-meshes` | meshes | **0** / 11.1 s | mesh self-test done | none | ASan 0, no leak. 1,758 listed, 1,729 `.geo/.peo` + 29 `.mad` loaded, 0 with bad objects |
| `r4-integrate-textures` | textures | **0** / 18.3 s | texture self-test done | none | ASan 0, no leak. 12,403 / 12,403 listing lookups, 10,661 registered and loaded, 0 not loaded, 1,742 rows with no file |
| `r4-integrate-oracle` | oracle (debug), 512 ticks | **0** / 9.8 s | oracle self-test done | none | ASan 0, no leak. `randSyncErr=0 univSyncErr=0 blobSyncErr=0`, 772 records, 140,723 B |
| `r4-integrate-etg` | etg, 128 ticks | **0** / 11.9 s | etg effect self-test done | none | ASan 0, no leak. 9 of 9 scripts parsed, 197 effects loaded, 0 referenced but not loaded; 618 created, 3,197 deleted while running, 1,093 left, peak 2,515 effects / 3,012 particle blocks (as R3) |
| `tools/oracle/selftest.sh --label r4-integrate-oracle-rel` | oracle (release), 512 ticks | 0 / 0.35 s each | oracle self-test done | none | runs a and b `cmp` equal (140,723 B, sha256 `4a2a38a464d9a395`) and `IDENTICAL`; the perturbed run reported at tick 300 / `rng10` / 1 draw ahead (and packet 150's `randcheck`); the flipped copy at tick 400 / `ships_pos`; `rngs=27` in every `T` record. **`SELFTEST PASS`** |

So every row of the R4 exit test holds:

| condition (docs/m2-r3-summary.md "R4 integrate") | result |
|---|---|
| Mission01 runs a few thousand ticks headless with ASan 0 | **met**: 6,000 ticks, three debug runs (a, b, perturbed) and the release run, ASan 0 in each |
| two `hwi-engine` runs give byte-identical `.hwo` streams | **met**: `r4-integrate-mission-a` = `-b` byte for byte; the release build and the audit's build give the same 6,001 `T` records |
| fe still reaches `Main_game_screen` | **met** |
| self-tests unchanged or better | **met**: statics 143/145 (108 StaticInits, R3: 56), meshes 1,758/1,758, textures 10,661 / 0 not loaded, oracle 0 sync errors, etg 618 effects, all as R3 |
| release oracle self-test `SELFTEST PASS` | **met**, 27 RNG streams |
| quitting runs return rc 0 | **met**: 9 of 9 quitting debug runs (statics, statics-init, meshes, textures, oracle, etg, the three mission runs) and 4 of 4 release runs |
| `tools/m2_layout32.sh` OK; clean builds 0 warnings | **met** |

Harness facts for the next rounds, in addition to R3's two (sp needs `--timeout` of about 300 s or more; the
`/debugScreenshots` next-level loop after about 816 ticks):
- **The mission mode is the fast gate.** `tools/m2_harness.sh <label> --mode mission --mission-ticks N` runs Mission01
  on the virtual clock (one universe tick per frame, no animatic, no next-level loop): about 22 s for 6,000 ticks under
  ASan, 4.4 s in release. `tools/oracle/mission_test.sh` is the four-run acceptance test. The 154 Mission01 ships hold
  position under `CAT_HW1_MISSION_SCRIPTS 0`, so the stream exercises the universe update, the deferred per-ship
  update, the collision blobs, the RNG streams and the ship hashes, not combat or docking.
- **Compare streams with `hwo_compare.py`, not by whole-file sha256:** the 64-byte header carries the build identity,
  so the sha256 changes with every commit while the `T` records stay the same.

## The five ports

| # | item | what landed | DONE WHEN | audit | commit |
|---|---|---|---|---|---|
| 1 | universe update + mesh state ([report](m2-r4-univupdate.md), [audit](audits/m2-r4-univupdate.md)) | **Mesh state:** `SpaceObj` +0x80/+0x84 (`catMeshState`, `catEffectBlock`) set by every creator and freed by every free path; the per-vertex `CatVertexState` (0x28) and slot tables; binding parent links (`mhbinding` 0x18, `mhlocalbinding` 0x58); the nozzle/matrix helpers behind `trailGetNozzleOffset`. **Glow:** `cat_shGlowUpdate`, `cat_shColourAddGlow` with a state, team colour, the bullet-hit glow (`cat_meshApplyBulletHit` and its vertex searches), the state passed through `meshRenderShipHierarchy`. **Deaths:** `univDeleteDeadShip` in the exe's order (the sMothership death sequence, big-ship explosions with 10 parameters, inline bullet/missile clean-up, 5 s missile self-destruct), the delete lists, `univFindBackupMothership` per race, killer collisions (`ApplyDamageToCollidingObjects` REWRITTEN). **Update:** `univUpdate`'s frame stepping, real-time quit/win, `cat_univShipDeferredUpdate` (partial), the per-player ship counters, 22 cat-new functions. HW1 Drone/DefenseFighter/CryoTray cases retired. 98 reviews; all 52 IDENTICAL bodies compared, 10 found modified | met: sp 1,248 ticks ASan 0 (`r4u-final-sp`); etg self-test unchanged | PASS-WITH-FIXES, **0.95** (38/40). Fixes: the crate caller's unused argument and claim, `univupdateInit`'s delta-map note, 3 unreviewed edited IDENTICAL functions (`meshRender`, `meshRenderShipHierarchy`, `LoadSpaceObj`), 4 missing `CAT:` comments | `3160fc0` |
| 2 | command layer + formations ([report](m2-r4-command.md), [audit](audits/m2-r4-command.md)) | **First, the systemic header:** `specialFlags` / `specialFlags2` at the exe's bits from a disassembly census of 61 sites (`tools/m2_specialflags_scan.py`): 25 + 23 live flags, 14 HW1 flags retired to 0, 4 flags moved between the words with their 18 reader lines, `_Static_assert`s on distinctness and the used-bit masks 0x7BFFF1CF / 0x3FFFF859; 19 new `SPECIAL_2_Cat*` names chosen from their setters. **Orders:** `COMMAND_CAT_LINK` 11 .. `COMMAND_CAT_17`, `FORCEFIELD_WALL/SPHERE_FORMATION` 9/10 from the exe's name table. **Bodies:** 10 `CommandLayer.c` and 6 `CommandWrap.c` MODIFIED bodies (`RemoveShipFromBeingTargeted` rewritten with its 4th argument and `REMOVE_CAT_SALVAGED`, `protectShip(sAvg)`, `ShipHasToLaunch`, `clCreateShip`, `clShipDied`, `clFlashShips` in real time, the pause gates), 5 IDENTICAL found modified, 7 cat-new (the external-build timer and update, `cat_shipIsSalvager`, `cat_shipIsLiveWorker`), `clFormation`'s accepted set; 8 HW1 calls with no exe counterpart retired. `CAT_EXTERNAL_BUILD_DRIVER 0` (CAT-PROVISIONAL) keeps externally built types inside the factory until ConsMgr's driver lands. New tools `m2_fdis.py`, `m2_fdec.py`, `m2_exedata.py`. 33 reviews | met: flags at the exe's bits with static checks; sp 1,248 ASan 0; fe unchanged. The port of the three files is declared partial (about 35 `CommandLayer.c` bodies, the orders' own functions, the force-field formations, 75 uncompared IDENTICALs: the R5 plan below) | PASS-WITH-FIXES, **0.938** (61/65). Fixes: an edited IDENTICAL function without a review (`GravWellGeneratorRemoveShipReferences`), the `SPECIAL_2_CatBeingBuilt` clear this port dropped in `univDeleteDeadShip`, a misleading per-file count, 4 stale comments | `d47a23f` |
| 3 | combat, docking, salvage ([report](m2-r4-combat.md), [audit](audits/m2-r4-combat.md)) | **Salvage:** the runtime of 0x004f7a90..0x004fd600 in `Salvage.c` (`CatSalvagerInfo` 0x28, `CatSalvageDepot` 0x144, the 15-state special target, depots, conversion, Beast infection); HW1's 3,206-line salvage code, its statics, MEX chunk and every reader deleted; `SalCapCorvetteHeader` at the exe's 0x30; the exe's clamp lists (`Clamp.c`). **Gun:** the weapon word (`Gun` 0x88, `catWeaponType` at +0), `Bullet.bulletType` as the weapon word with the burst flag, damage by weapon vs armour from the exe's tables, `gunShoot` / `cat_gunCreateBullet` rewritten, `gunCanShoot`'s health-scaled fire times, `missileShoot`, the 7-argument `ApplyDamageToTarget`, `cat_shipUpgradesInit/ApplyUpgrades`. **Beast weapon:** every provisional entry a port (`BeastWeapon.c`, `CatBeastHit` 0x1C). **Collision/physics:** kamikaze steering and hit, `Blast.c` (the six shock-wave templates, `CatBlast` 0x64), DFG and Sentinel bullet checks, the Sentinel beam draw-only frames, the 4 missile collisions, drift and speed multiplier in `Physics.c`. **Dock:** `ResCollect.c` (asteroid break pieces, the harvest loop, `cat_rescollectStopHarvest`), the sCarrier door, the launch manager's module slots, `dockPutShipInside`'s classes. **Not done:** the Cataclysm dock purposes (declared, see below). 40 reviews, 5 IDENTICAL found modified | met: `SalCapCorvetteHeader` 0x30 on ILP32; no HW1 salvage reader left (after the audit's fix); sp 1,248 ASan 0; statics 143/145 | PASS-WITH-FIXES, **0.891** (41/46). Fixes: `dockRemoveShipFromInside`'s class counts, `cat_blastAffectShip`'s float sum order (the exe's), a left HW1 salvage type test (`salCapCorvettesInSelectionCantDoFormation`), 3 non-addresses, the tick-count framing, a clamp deviation note | `d407410` |
| 4 | ship code B: the Cataclysm-only ships ([report](m2-r4-ships-b.md), [audit](audits/m2-r4-ships-b.md)) | **22 new ship files** on the exe's headers slot for slot (Acolyte, ACV, Mimic, MCV, Leech, RammingFrigate, MultiBeamFrigate, HiveFrigate, Swarmer, Dreadnought, ModuleShip, Worker, RepairDroid, Processor, Sentinel, CruiseMissile, RKO, Slipgate, Crystal, dBeastMothership, NomadMoon with the field emitters), plus `ShipDisable.c` (disable reasons, EMP shock, helplessness, drift), `Dissolve.c`, `ShipLink.h`; the sMothership big gun, the Beast-mothership bonuses, the mothership modules (`cat_univCreateMothershipModules`), the Worker repair and harvest, the Processor auto-repair, the RKO's 18-state machine, the Sentinel force fields and DFG disruption, the mimic disguises; the provisional queries moved to their home files; `cat_madAnimationStartNumber`, `cat_randomExpDelay`, `cat_timer*`. **Retired:** the 22 dormant HW1 ids (200-221), 14 HW1-only ship files (28 files), `DefenseFighter.c`, the `RepairCorvetteHeader`, and every reader of a dropped type in `src/Game`, `src/Ships`, `src/SDL` (the AI files got `CAT-TODO(stage 4 AI)` stand-ins). 48 ship `.c` files now; 41 CAT-TODO/PROVISIONAL markers in the new files name their owner | met: 143 of 145 `shipHeaders` rows equal the exe's (the two cloak generators keep `DefaultShipHeader` by decision); no HW1-only type or file left; statics 143/145 with 108 StaticInits under ASan; sp 1,248 ASan 0 | PASS-WITH-FIXES, **0.921** (58/63). Fixes: `soundEventBurstStop`'s missed `soundevent.gun` store; 20 retire-edited IDENTICAL functions reclassified MODIFIED with notes; the report's silence on the WIP's AI/Dock/CommandLayer retire edits and on `WorkerHeader` slot 26 | `3078c10` |
| 5 | the rest, Player layout, oracle stage 2, tools ([report](m2-r4-rest-oracle.md), [audit](audits/m2-r4-rest-oracle.md)) | **Layout:** `Player` 0x6530 and `Universe` 0x3D9BC with 36 asserts (FOW grid +0x6500, the upgrade fields +0x6504.., the one tactics list, `catSensorsHoldRenderList`), `PlayerStats` 0x7F8, `shipinprogress` 0x30, and the research layout they need: `ResearchAPI.c` rewritten from the exe (41 technologies, 42-word tech sets, `TechStatics` 0x7AB4, the module labs, the 128-row dependency table). **Oracle:** `Ship.health` settled at exe +0x170 and the Player fields VERIFIED in the GOG DLL (`HWO_TV_HEALTH` and `HWO_TV_PLAYERS` set); `/oracleMission N` (virtual clock, no animatic, no next-level loop), harness mode `mission`, `tools/oracle/mission_test.sh`. **Level load:** `scriptSetShipsCB`'s race override and research hooks, `unitCapCanCreateShip`'s full body. **Rest:** `Bounties`, `Alliance`, `Crates` (`cratesRewardPlayer` with the exe's ladders), `Task.c` time compression, `Tactics`, `Stats` (the fight tables), `B-Spline`, `Captaincy`, `HS`, `Camera*`, notes in `Key`/`Region`/`Memory`. **Tools:** `delta_map.py report --calib-samples`, `classify` rerun on r6, the 21 UI cat-new moved to stage 3. 70 reviews, 9 IDENTICAL found modified | met: two 6,000-tick headless Mission01 runs byte-identical, ASan 0 (`r4ro-fin2-mission-a/b`), perturbed run and flipped copy caught; fe and the self-tests unchanged | PASS-WITH-FIXES, **0.954** (62/65). Fixes: `bsCurveUpdate`'s persisted key-end time (the exe pins a local), 3 unreviewed edited IDENTICAL functions, the in-flight sp reading (the finished run: 2,256 ticks at 400 s, ASan 0) | `127b5b9` |

**Round size** (`git diff 9c2543d HEAD`, the R4 launch to the last audit): 257 tracked files changed,
+32,262 / -16,821 lines. `src/` alone: 229 files, +26,350 / -16,231, with **1,772 added lines carrying `CAT`** and 5
`HWI-ORACLE`; 49 new files (`Game/Blast.c/.h`, `Game/Dissolve.c/.h`, `Ships/ShipDisable.c/.h`, `Ships/ShipLink.h`, the
22 Cataclysm ship files with headers) and 28 deleted (the 14 HW1-only ship files). The tree now carries 2,006 `CAT:`
lines, 167 `CAT-TODO`, 37 `CAT-PROVISIONAL`, 14 `CAT-DORMANT`, 7 `CAT-FIX`, 18 `HWI-ORACLE`. New tools:
`tools/m2_specialflags_scan.py`, `m2_fdis.py`, `m2_fdec.py`, `m2_exedata.py`, `tools/oracle/mission_test.sh`.
`runs.txt` logs 125 harness runs since the R3 integrate (lines 356-480), 15 of them this step's.

## Delta map after the round

The ports recorded their comparisons as reviews in `ghidra/delta_map.json` (`tools/delta_map.py mark/merge`); the
rest-oracle port re-ran `classify` on the r6 export (0 class diffs) and synced the 125 reviews that earlier ports had
written straight into the map back into `ghidra/delta_map_review.json`.

| class | M2-R3 end | M2-R4 end | M2-R4 bytes |
|---|---:|---:|---:|
| IDENTICAL | 3,110 | **3,065** | 589,609 |
| MODIFIED | 1,234 | **1,278** | 639,526 |
| REWRITTEN | 76 | **77** | 9,712 |
| hand-reviewed | 1,374 | **1,615** | |

Per stage (`tools/m2_stage_counts.py --md`): HW1-matched IDENTICAL / MODIFIED / REWRITTEN functions (MODIFIED bytes),
then cat-new by the tool's rules.

| stage | M2-R3 end | M2-R4 end | cat-new (M2-R3 → M2-R4) |
|---|---|---|---|
| 1 | 218 / 97 / 0 (51,602 B) | 215 / 100 / 0 (52,943 B) | 76 |
| **2** | 804 / 569 / 29 (240,423 B) | **783 / 589 / 30 (245,967 B)** | 829 → **808 / 180,868 B** |
| 3 | 932 / 259 / 7 (173,691 B) | 928 / 263 / 7 (175,469 B) | 429 → **450 / 139,196 B** |
| 4 | 403 / 74 / 2 (43,113 B) | 387 / 90 / 2 (50,387 B) | 23 |
| 5 | 406 / 114 / 12 (74,848 B) | 406 / 114 / 12 (74,848 B) | 67 |
| 6 | 347 / 121 / 26 (39,619 B) | 346 / 122 / 26 (39,912 B) | 2,365 |

What moved: 58 functions the map called IDENTICAL were found modified by the ports and their audits (univupdate 10 of
the 52 it compared, command 6 of 12, combat 5, ships-b 5 + 20 in its audit, rest-oracle 9 + 3 in its audit); stage 4's
16 are the retire step's edits to the AI files (a type id that does not exist in Cataclysm cannot be tested by the
exe). `ApplyDamageToCollidingObjects` became REWRITTEN. The 21 UI functions (`cat_shipPrint*`, `cat_selStatus*`,
`cat_selHealth*`) moved from stage 2 to stage 3 by a name rule. The R2 estimate stands: `docs/m2-r1-function-deltas.md`,
regenerated with the new sample filter (1,171 calibration reviews, the 384 targeted port reviews excluded), gives an
IDENTICAL-row agreement of 92.0% and an estimated true IDENTICAL share of 92.4%.

**Cat-new in the tree.** 573 of stage 2's 808 cat-new names (103,984 of 180,868 B) are now defined in `src/` (ported,
hooked, or a `CAT-PROVISIONAL` stand-in that names its owner); 112 of stage 3's 450 (23,575 B) are too, and those are
stage-2 code that the address-neighbour rule placed next to a stage-3 file (the salvage runtime next to `ResearchGUI.c`,
the blasts and upgrade helpers next to `ShipView.c`, the Acolyte/ACV/EMP code next to `utility.c`, the Beast weapon
next to `BTG.c`, the cloak next to `Clipper.c`). The 235 stage-2 names not in the tree (76,884 B) split into: the dock
purposes and the command orders (53 / 22,496 B, the R5 close-out producers), UI/render/FOW functions the rule placed in
stage 2 (146 / 48,330 B: the popup menu next to `Randy.c`, the console next to `Crates.c`, key bindings and hotkey groups
next to `UnivUpdate.c`/`Vector.c`/`Key.c`, the game-stats screen next to `Globals.c`, the repair-droid proxies next to
`RepairCorvette.c`, the Sentinel-field drawing next to `SalCapCorvette.c`, the FOW next to `FlightMan.c`, the camera
UI; owned by the stage-3 producers below), and 36 small leftovers (6,058 B) listed in the next section.

## What is left of stage 2

**Nothing blocks the stage-2 exit test**: it is met above, and each port's DONE WHEN is met. But two ports declared
their scope partly unported, and the reports' "What is left" sections name more. These are **stage-2 items** and go
first in M2-R5 (producers 1 and 2 below):

1. **The Cataclysm dock purposes** (combat's declared gap). Cataclysm replaces HW1's dock bitmask with a purpose
   number 0..10 and a dock class per ship pair (`cat_dockGetDockClass` 0x00463d70), and the per-type docking paths
   with one cone-based state machine (`cat_dockShipDocksAtShip` 0x00465860, 3,395 B, with `cat_dockFlyOutOfCone`,
   `cat_dockFindNearestDockPoint`, `cat_dockFindLaunchPoint`, `cat_dockFinishedDocking`, `cat_dockCanDockWithBusyHost`,
   `cat_dockInitShipDockInfo`, `cat_dockHasFreeSlotForShip`, `cat_dockFlyInFrontOfHost`); `ThisShipCanDockWith`'s 4th
   argument and per-class counts; `clDock` 0x00466c70; the REWRITTEN `dockReserveDockPoint` / `RemoveShipFromDocking` /
   `RemoveShipFromLaunching`; `processDockToDo`'s swarm part; `dockPutShipOutside`'s modules, FOW and rotations;
   `clLaunchShip`'s 3-argument `InitializeDockVars`; `cat_clHarvestNearestResource` 0x00449790; the `SPECIAL_EXITING_DOOR`
   readers; `cat_univClampResourceTimer`. Waiting on it (CAT-PROVISIONAL HW1 stand-ins): the hive, slipgate, Leech,
   Worker and Processor dock paths, the hive's purpose 2 (swarmers out), the carrier door, the Missile +0x254 float.
   `Dock.c` is 4 / 11 / 3 in the map plus 14 unported cat-new (7,629 B); 95 of its 123 functions are HW1-only.
2. **The command layer follow-up** (command's declared partial port). `CommandLayer.c` 47 / 49 / 1 (38,512 MODIFIED
   bytes, 10 bodies ported in R4): `RemoveShipsFromDoingStuff` with its reason codes, `FreeCommandToDoContents` /
   `FreeLastOrder`, the attack, move and hyperspace order families, `clSpecial`, `clHalt*`, `clProtect`,
   `clSetMilitaryParade`, `clHoldingPattern`, `clProcess`, `clPostProcess`, `GroupShip`, `clRUTransfer`; the orders
   11-16 themselves (`cat_clLink` 998 B, `cat_clUnlink` 734, `cat_processLinkToDo` 2,559, `cat_processUnlinkToDo` 2,346,
   `cat_clMimic`, `cat_clResearch`, `cat_clPointOrder`, `cat_clUpgradeShips`, `cat_clSpecialTarget`,
   `cat_clCalculateMoveToOffsets`, `cat_clAddAttachedModulesToTargets`, `cat_clFindLinkPartner`, the link flight
   `cat_linkFlyIntoPosition` / `cat_linkComputeTargetCoordSys`); `COMMAND_CAT_POINT` for the big gun;
   `cat_clWrapSetModuleSlot`, `cat_clWrapSetLeechAction`, `cat_clWrapMimic`, `cat_clWrapUnleech`, the Beast-system MP
   path. `Formation.c` 18 / 15 (12,524 B): `clFormation`'s 4th argument, `FormationCommand` +0x288, the force-field wall
   and sphere (`SetFormationOffsetsForForceFieldWall` 1,743 B, `cat_formationSphereSentinelLinks`,
   `formationSpecificsSphere`, `createSphereStaticInfo`, `setFormationToDo`, `processFormationToDo`, `GetShipsTravelVel`,
   `processMilitaryParadeToDo`, `setMilitaryParade`, `paradeSetTweakables`). `Tactics.c`: the Cataclysm `TacticsInfo`
   layout, the 7 HW1 tactics callers (`tacticsUpdate`, `tacticsGlobalUpdate`, `tacticsDelegate*`,
   `tacticsAttackCommandVoided`, `tacticsPopUpSetUp`, `tacticsAreStrikeCraftInSelection`), `cat_tacticsRetaliate*`
   (12 cat-new, about 3,300 B), `cat_univLimitProtectedShipSpeed`. The 75 IDENTICAL bodies of the three files not yet
   compared.
3. **The Ship layout pass** (rest-oracle and the univupdate audit). The tree's ILP32 `Ship.health` is at 0x15C, the
   exe's at 0x170: the `SpaceObjRotImpTarg` prefix is 0x14 bytes short and `Ship` is not pinned at 0x63C (CET asserts
   40 offsets). The exe's +0x18 word is two fields (`SpaceObjHeader.catHeader18`, `Ship.catShipFlags`, bridged by
   `CAT_OBJ_FLAGS18`); `SpaceObj.attributes` is a uword at +0x36 where the exe has a dword at +0x38 (the blast bits
   0x10000 / 0x20000 and the crystal game option read it); the Bullet/Missile layouts. The oracle hashes `health` by
   name, so the stream is unaffected until then.
4. **Experience and the last Player readers:** `cat_shipUpdateExperienceLevel` 0x005147d0 and 9 upgrade helpers
   next to `ShipView.c` (1,191 B), the experience level in `cat_shipGetSpeedMultiplier` /
   `cat_shipGetDamageMultiplier` (CAT-PROVISIONAL level 0), kills → experience in `ApplyDamageToTarget`, the per-class
   game stats of `clCreateShip`.
5. **Compared bodies:** the 97 IDENTICAL functions of `FlightMan.c` (45, 13,987 B), `Vector.c` (14), `Matrix.c` (20),
   `Volume.c` (4), `fastmath.c` (2), `Twiddle.c` (2), `Teams.c` (6), `Undo.c` (2), `Debug.c` (2) were not read this
   round; the convention asks for it.
6. **Small leftovers** (36 names not in the tree, 6,058 B; owners in brackets): `cat_gunPickMothershipModuleTarget`
   423 B and `cat_collSentinelFieldExempt` 284 B [R5-1]; four dissolve helpers whose names are not in `Dissolve.c`
   (`cat_shipDissolveCutEffects`, `EffectCreate`, `ComputePlane`, `EdgeIntersect`: the port may hold them inlined, to
   check) [R5-1]; the three variant StaticInits `cat_p1MothershipVariantStaticInit`, `cat_carrierVariantStaticInit`,
   `cat_dreadnoughtVariantStaticInit` and `cat_shipHasRamUpgrade` [R5-1]; `cat_beastGetAttackStaticInfo` [R5-2]; the
   mimic menu items (`cat_mimicInitDisguiseTables` and 8 item functions) and `cat_bigGunFireCommand` /
   `cat_bigGunIsCharged` [stage 3 UI, R6-1]; `cat_dmgShipLoadDamagePoints` / `Free` [stage 3 render, R5-3];
   `cat_saveShipCounters` / `Load`, `cat_saveGameMiscChunks` / `Load` [the user's saves]; `cat_DeclarePlayerWinReportResults`
   [stage 3/5]; `cat_rsMPRBShipDependSetCB`, `cat_shipIsCollectingResources` [R6-4]; the developer fight statistics
   `cat_statsSetupShipsForFight`, `cat_statsGetPostCombatStats`, `cat_statsRandomAttackShipStatic` (no game path reaches
   them).
7. **Two provisional switches:** `CAT_HW1_MISSION_SCRIPTS 0` (stage 6) and `CAT_EXTERNAL_BUILD_DRIVER 0` (the
   construction manager's `cmDeterministicBuildProcess` 0x004592a0 / `Cancel` 0x00459040 and
   `cat_shipUpdateExternalBuildAndAI` 2,088 B, `cat_cmExternalBuildShipDied`, `cat_cmExternalBuildAttach`: stage 3's
   ConsMgr producer, R6-4; until then the 30 `externalBuild TRUE` types are built inside the factory).

Outside stage 2, with the owners the reports name:
- **Stage 3** (the R5/R6 plan): the renderers deferred since R2 (the object list `cat_rndRenderObjectList` 9,890 B,
  lens flares, fog layers, billboards `cat_partRenderUnk1System`, the Sentinel field, Dreadnought charge glow and beam
  drawing, blast rendering, the repair-droid proxies, `cat_meshLineIntersect` for the ramming latch and cutting beam,
  the ETG real32 parameters, `madAnimationStart`'s third argument); the status bars and the picking API; the ship info
  panel; the main overlay, popup menu, console, key bindings, options and the game-stats screen; speech and sound hooks
  (dock speech ids, `cat_soundEventBlast`, asteroid break and harvest sounds, battle chatter, `speechEventUnderAttack`'s
  merged branch, `cat_ioMessageAdd` and `clCommandMessage`); the research GUI, construction, trade and launch managers
  (`cmCanBuildShipType`'s 3rd argument, the module slot draws, the support units); FOW (`cat_fowCreatePlayerGrids`,
  the cubes and boundary, `cat_univShipFOW*`, the stealth detection update); NIS and camera UI; the 1,705-string
  readers; `fquant.c` / `fescreen` UBSan; `cat_keyNameStringsInit`, `regRegionDelete`, `hsStaticGateRender`.
- **Stage 4:** the AI readers of the retired `SPECIAL_Finished_Resource` / `SPECIAL_Resourcing`, the AI's dock order for
  converted ships, the 14 `CAT-TODO(stage 4 AI)` stand-ins where the AI asked for a dropped HW1 type.
- **Stage 5:** the network paths of the external-build wraps (`cat_clSend*`), `NetCheck.c` and `univGetChecksum`'s
  retired resourcing mask, `TransferCaptaincyStateMachine`, the recorder paths (`univAddMissile`'s ID table, KAS
  derelicts while recording), the counters' save/load.
- **Stage 6:** `CAT_HW1_MISSION_SCRIPTS` (the kas2c Mission01 script and the NIS-first start), the next-level flow, the
  single-player difficulty, `cat_kasHashString` team matching in `scriptSetShipsCB`, `hsStaticInit`'s gates, the Mission
  7 / 15 KAS parts, the RKO and Naggarok KAS variables, `singleHackResearchWarp`'s HW1 technology names.
- **The user's inputs:** the save tick-0 compare at `saveCatLoadHook` and the 32-bit save reader (the two saves; also
  `LoadClampInfo`'s `clampedObjs`, which the host-driven clamp update needs restored); the first live oracle session,
  which now compares `ships_health` and `players` too (decisions 6, 8, 10).

## Plan for M2-R5 and M2-R6: stage-2 close-out, then stage 3 (UI and presentation)

Stage 3 at the end of R4 is 928 / 263 / 7 HW1-matched functions (175,469 MODIFIED bytes) plus 450 cat-new by the
tool's rules, of which 338 (115,621 B) are not yet in the tree; the UI, render and FOW functions the neighbour rule
counts in stage 2 add 146 (48,330 B). That is about 340 KB of exe code to read against the tree, about the size of
R4's stage-2 part B, which one round did not finish (two scopes were declared partial), so stage 3 takes **two rounds
of 5 producers + 5 auditors + integrate** (11 agents each), with the stage-2 close-out as the first two producers of
M2-R5. Producers run in sequence (they share `src/` and the build), as in R2-R4.
The porting conventions, harness and session rules stay the same, with two additions from this round: every `src/`
producer compares each IDENTICAL body in its scope (R4 found 58 of them modified), and every engine run goes through
`tools/m2_harness.sh` (silent). Each producer also retires the HW1 aliases in the files it ports and replaces the
`CAT-TODO`s that name it (or re-targets them with a reason).

Counts are HW1-matched IDENTICAL / MODIFIED / REWRITTEN functions by HW1 file (MODIFIED + REWRITTEN bytes), then the
cat-new functions assigned by name rule or neighbour that are **not yet defined in `src/`** (the ones already in the
tree are the R3/R4 ports); the producer file sets straddle the tool's stage boundary where a stage-2 file is really
presentation (`Transformer.c`, `StubTransform.c`, `LaunchMgr.c`'s draws, the UI functions next to stage-2 files). Scratch script for the table:
`python3 -I tools/m2_stage_counts.py` plus a per-file grouping (this step; the owners should rerun it on the map they
start from).

**M2-R5: stage-2 close-out + stage 3 part A (render, sound, HUD)**

| # | producer | scope | I / M / R fn (M+R bytes) | cat-new not in tree | done when |
|---|---|---|---|---|---|
| 1 | layout pass + dock purposes + flight/math compares | **First, the systemic header:** `Ship` pinned at 0x63C with CET's 40 offsets (`health` at 0x170), the +0x18 word as one field, `SpaceObj.attributes` as the exe's dword at +0x38, the Bullet/Missile layouts (the +0x254 float); then `Dock.c` and `LaunchMgr.c`: the purposes 0..10, `cat_dockGetDockClass`, `cat_dockShipDocksAtShip` and its cone helpers, `ThisShipCanDockWith`'s 4th argument, `clDock`, the REWRITTEN dock-point functions, `processDockToDo`, `dockPutShipOutside`, `clLaunchShip`, `cat_clHarvestNearestResource`, the hive/slipgate/Leech/Worker/Processor stand-ins and the carrier door, `cat_univClampResourceTimer`; the experience level (item 4); the small leftovers of item 6 marked R5-1; the 97 IDENTICAL bodies of `FlightMan.c` and the math files compared | 108 / 20 / 3 (8,123 B) | 14 / 7,629 B (+ the layout pass) | `tools/m2_layout32.sh` holds `Ship` 0x63C and the +0x38 dword; a new scripted self-test `/catDockTest` (harness mode `dock`: a scripted start with a carrier, a processor, workers and fighters, orders through `clDock` / `clLaunchShip` / `clHarvest`, no input) ends with every ship where the exe's state machine puts it, ASan 0; no HW1 dock path, purpose bitmask or `CAT-PROVISIONAL` dock stand-in left; two 6,000-tick mission streams still byte-identical |
| 2 | command layer follow-up + force-field formations + tactics | `CommandLayer.c`, `CommandWrap.c`, `Formation.c`, `Tactics.c` (item 2): the remaining MODIFIED bodies, the orders 11-16 and their `cat_process*ToDo`, the link flight, the force-field wall and sphere with the Sentinel links, `clFormation`'s 4th argument, the Cataclysm `TacticsInfo` and `cat_tacticsRetaliate*`, `COMMAND_CAT_POINT` for the big gun, the module-slot and Leech/mimic wraps; the 75 uncompared IDENTICAL bodies | 80 / 79 / 2 (53,636 B; R4 ported 16 of the MODIFIED bodies and the salvage readers) | 40 / 15,044 B | every MODIFIED body of the four files ported or listed with an owner outside stage 2; a scripted `/catCommandTest` (link, unlink, point, mimic, research orders and a force-field wall and sphere of Sentinels over N ticks) completes with the exe's end states and ASan 0; sp as before, ASan 0 |
| 3 | render/GL | `render.c` (`cat_rndRenderObjectList` 9,890 B: the object list with the per-vertex glow state, fade, dissolve and mimic flags, depth sort), lens flares (`cat_rndLensFlaresRender`, the `.script` callbacks), the fog layers (`cat_rndFogLayer*`, `cat_scriptSetFogCB`), billboards (`cat_partRenderUnk1System`), `Shader.c`, `Light.c`, `Clipper.c`, `prim2d/3d.c`, `Color.c`, `glcompat.c`, `glcaps.c`, `rinit.c/.cpp`, `Star3d.c`, `BTG.c`, `NavLights.c`, `Trails.c`, `Ping.c`, `screenshot.c`, `debugwnd.c`, `Transformer.c`; the renderer halves the ship ports left (Sentinel field segments, Dreadnought charge, Nomad beam, blast lightning, hyperspace gate rectangle, repair-droid proxies, `cat_meshLineIntersect`, damage points) | 218 / 36 / 0 (27,544 B) | 71 / 36,696 B | a new `/catRenderTest` renders N frames of Mission01 offscreen through the Cataclysm object-list renderer and logs its object, lens-flare and fog-layer counts: counts > 0, ASan 0, and the mission stream unchanged (render-side randoms stay on their streams); fe unchanged |
| 4 | sound and speech | `SoundEvent.c` (13,629 MODIFIED B), `SoundEventPlay/Stop.c`, `SpeechEvent.c` (the alerts, the race voice, the Easter egg), `smixer.c`, `soundlow.c`, `sstream.c`, `mixfft.c`, the fq codec (`fqcodec`, `fqeffect`, `fquant`: follow the binary, decision 11), `dct.c`, `Subtitle.c`, `Battle.c` (battle chatter); every sound/speech `CAT-TODO` of the R3/R4 ports becomes a call (dock speech ids, `cat_soundEventBlast`, asteroid break and harvest sounds, `speechEventUnderAttack`'s merged branch, `cat_clSpecialTargetSpeech`) | 122 / 44 / 1 (35,711 B) | 17 / 2,771 B | a new `/catSoundTest` (harness mode `sound`) opens `Music4Cat.wxd`, `SectSpeech.vce` and `BeastSpeech.vce`, decodes one stream of each through the fq codec into the dummy device and looks up the 88 battle-chatter events: completes with ASan 0 and **0 UBSan reports in `fquant.c` / `fqcodec.c`**; sp and fe unchanged |
| 5 | HUD: status bars and picking, ship info panel, overlays, pie plate, game-stats screen | the 25 `cat_shipPrint*` / `cat_selStatus*` / `cat_selHealth*` functions (the ship info panel 7,471 B and the status bars 2,896 B) with `selStatusDraw0/1/2` and the picking API at the exe bodies; `ShipView.c`, `InfoOverlay.c` (`cat_ioClick`, the overlay buttons and print options), `Tactical.c` (the `cat_mo*` main overlay: construction, research, tooltips, game messages, player names, support units, objectives), `PiePlate.c` (`cat_pieCompassDraw`), `TaskBar.c`, `Scroller.c`, the message scroller (`cat_ioMessageAdd`, `cat_ioUpdateHover`), `GamePick.c` + the game-stats screen (`cat_gameStats*`), `HorseRace.c`, the ship names (`cat_shipNames*`), the overlay icons | 40 / 22 / 2 (15,938 B) | 100 / 43,905 B | a new `/catHudTest` selects Mission01's ships through the selection API and draws the status bars, the info panel, the overlays and the pie plate for N frames offscreen: ASan 0; the 21 UI cat-new of the R4 tool move are ported; fe and sp unchanged |

**R5 integrate (exit):** from a clean build on the plain harness: (a) the stage-2 exit test still holds (two 6,000-tick
Mission01 streams byte-identical with ASan 0, sp universe running, fe `Main_game_screen`, the self-tests unchanged or
better, release `SELFTEST PASS`, quits rc 0, `m2_layout32.sh` OK with the `Ship` 0x63C pin); (b) the four new
self-tests (`dock`, `command`, `render`, `sound`) and `/catHudTest` complete with ASan 0; (c) `grep -rn "M2-R4"
src/` finds no `CAT-TODO` or `CAT-PROVISIONAL` that still names an M2-R4 owner (each is done or re-targeted to a stage
with a reason). Stage 2 is then closed except for the two switches above.

**M2-R6: stage 3 part B (main region, front end, start-up, managers, NIS/FOW)**

| # | producer | scope | I / M / R fn (M+R bytes) | cat-new not in tree | done when |
|---|---|---|---|---|---|
| 1 | main region, mouse, the popup menu | `mainrgn.c` (band box, `cat_ui*` special commands: link, unlink, mimic, leech, EMP, swarm, ram, siege cannon, the protect formation), `mouse.c` (`cat_mouseGetContextAction`), `PlugScreen.c`, the right-click popup menu (`cat_popupMenu*`, 31 functions, 19,342 B), the mimic menu and `cat_bigGunFireCommand` | 36 / 23 / 1 (19,329 B) | 67 / 26,569 B | a scripted `/catUiTest` opens the popup menu for a selection, runs each `cat_ui*` command once and closes it, ASan 0; the exe's context actions table reproduced; sp and fe unchanged |
| 2 | front-end flow, screens, controls, fonts, strings, options | `FEFlow.c`, `FEReg.c`, `UIControls.c`, `font.c`, `FontReg.c`, `ScenPick.c`, `ColPick.c`, `Options.c` (`cat_op*`: the Cataclysm defaults, the Homeworld-classic set, texture detail), `Demo.c`, `Titan.c`, `bink.c`; the 1,705-string readers (`ShipTypeToNiceStr` and the UI strings resolve to the install's strings); the `fescreen` / `PATCH` UBSan kinds (GoK's packed 64-bit FE structs) | 255 / 40 / 2 (14,812 B) | 28 / 3,397 B | a new `/catFeScreensTest` pushes and pops every screen of the install's 22 `.fib` files once: ASan 0 and **0 `fescreen` UBSan reports**; a `/catStringsTest` resolves all 1,705 string ids; fe `Main_game_screen` |
| 3 | start-up, main, key bindings, console | `main.c` (17 MODIFIED, 15,298 B: the exe's start-up and command line), `utility.c` (`utyGameSystemsInit` and the toggles, `cat_utyGameQuitToMainOrStats`), `KeyBindings.c` + the key-binding and hotkey-group functions next to `UnivUpdate.c` / `Vector.c` / `Key.c` (`cat_kb*`, `cat_keyNameStringsInit`), the developer console (`cat_console*`, 17 functions, about 9,500 B) | 82 / 36 / 0 (27,227 B) | 84 / 18,912 B | the install's `default_keys.cfg` loads and saves round-trip through the Cataclysm key-binding code; the console runs its command table from a scripted test; start-up as the exe's `main` (the R2 workaround list stays empty); fe and sp unchanged |
| 4 | construction, research, trade and launch managers; sensors manager | `ConsMgr.c` (the external-build driver `cmDeterministicBuildProcess`/`Cancel`, `cat_shipUpdateExternalBuildAndAI`, the SU bar, the support units `cat_su*`, `cmCanBuildShipType`'s 3rd argument; **`CAT_EXTERNAL_BUILD_DRIVER` to 1**), `ResearchGUI.c` (tech sets, `cat_rmAcquireShipTechs`, the print lists), `TradeMgr.c`, `LaunchMgr.c`'s slot draws, `Sensors.c` (`cat_sm*`: the sensors manager's ships, blobs, resources, bounds), `cat_rsMPRBShipDependSetCB` | 66 / 47 / 1 (27,463 B) | 32 / 14,463 B | a scripted `/catBuildTest` builds an externally built type (a destroyer at the sMothership) and an internal one in a headless Mission01 run with the driver on: the ship appears at health 0 and finishes, ASan 0, and two runs give identical streams; the sensors frame of `/catHudTest`; the research screen's tech sets at the exe's |
| 5 | NIS, animatic, fog of war, camera | `NIS.c` (122 IDENTICAL to compare, 17 MODIFIED; the 27 `cat_nis*` script callbacks: fog, lights, mimic, repair, slipgates, subtitles), `Animatic.c`; the FOW (`fow*` and `cat_fow*`: `cat_fowCreatePlayerGrids` at `levelInit`, the cubes, the boundary, `cat_univShipFOW*`, `cat_shipStealthDetectionUpdate`, `cat_fowCanPlayerSeeShip` at its 4 waiting sites, `fowDestroy` / `fowSelect`); the camera UI (`cat_mc*`, `cat_cc*`, the 8 deferred `Camera*.c` functions) | 125 / 18 / 0 (10,302 B) | 73 / 15,844 B | a new `/catNisParseTest` parses every NIS and NIS script of the install with ASan 0; `Player.catFOWGrid` is non-NULL for every player in a Mission01 run and the stealth update runs (the two mission streams stay identical between runs); the camera functions at the exe bodies |

**R6 integrate (stage-3 exit):** from a clean build on the plain harness: fe reaches `Main_game_screen` and
`/catFeScreensTest` passes with 0 `fescreen` UBSan reports; the `render`, `sound`, `hud`, `ui`, `build`, `nis` and
`strings` self-tests complete with ASan 0; sp reaches the running universe with the Cataclysm HUD and renderers drawing
(1,248+ ticks at 340 s, ASan 0); two 6,000-tick Mission01 streams are byte-identical in debug and release; the statics,
meshes, textures, oracle and etg self-tests are unchanged or better; every quit returns rc 0; the three stage-3 UBSan
kinds (`fescreen`, `PATCH`, `fquant.c`) are gone from every run. Then stage 4 (AI: 387 / 90 / 2, 23 cat-new, the
`CAT-TODO(stage 4 AI)` stand-ins first), stage 5 (LAN, nice-to-have) and stage 6 (missions: the kas2c code, the
NIS-first start, `CAT_HW1_MISSION_SCRIPTS` retired), each sized by `tools/m2_stage_counts.py` when its turn comes.

## Decisions

**Applied this round (answered by the user 2026-10-10):**
- The faithfulness defaults: the 22 dropped HW1 types are deleted (ships-b), `k`/`tCloakGenerator` keep
  `DefaultShipHeader` where the exe has NULL, `crateInit` resets 46 entries, the mission-17 texture cut and the fq
  audio code follow the binary.
- The oracle may call the game's own functions: the headless mission mode starts single player through
  `utyGameSystemsInit` as `/debugScreenshots` does, with no input.
- The oracle reference recording and the two saves come later and did not block: the hooks are
  `saveCatLoadHook` (`src/Game/SaveGame.c`), `tools/oracle/` with the GOG DLL's `HWO_TV_HEALTH | HWO_TV_PLAYERS` bits
  now set, and `tools/oracle/mission_test.sh` for the recreation side.

**Still open (the user's; none blocks M2-R5):**
- **6.** The first oracle reference recording: which skirmish map and which players.
- **8.** Two saves from the M0 play-test, one mission and one skirmish (`tools/formats/savegame_probe.py`,
  `saveCatLoadHook`; also the clamp lists of a loaded game).
- **10.** The first live oracle session: load the DLL under Proton, record, then two playback runs
  (docs/m2-r2-oracle.md "What is left" 1); unverified until then: the DLL loads, the H6 trigger, `vqpc`.
- **The M0 play-test sign-off** (docs/PLAN.md).

**Surfaced by M2-R4.** Each has a default in the tree; say if you want otherwise.
- **`CAT_EXTERNAL_BUILD_DRIVER 0` (CAT-PROVISIONAL, `CommandLayer.h`).** The 30 `.shp` types with `externalBuild TRUE`
  (destroyers, dreadnoughts, carriers, processors, mothership modules) are built inside the factory and created at the
  end, as HW1 does, until ConsMgr's external-build driver lands (M2-R6 producer 4). With the branch on now they would be
  created at health 0 and never finish.
- **Names chosen here for exe bits, orders and fields whose names are unknown**, all from their setter functions:
  `SPECIAL_CatKasSpecialRepairModifier` 0x2000; 18 `SPECIAL_2_Cat*` flags (`CatBeingBuilt` 0x8, `CatNoAvoidance` 0x40,
  `CatParalyzed` 0x4000, `CatBeastified` 0x10000, `CatSalvaged` 0x20000, `CatExempt20` 0x100000, ...);
  `COMMAND_CAT_13` and `COMMAND_CAT_17` (no setter found; `cat_selMakeShipsProtectCapable` lists 17); the Player fields
  `catFOWGrid`, `catArmourLevelCap`, `catWeaponAvailable`, `catUpgradeBits`, `catUnknown08`; `catSensorsHoldRenderList`,
  `catSelection1A0`, `catStat28`, `catBuiltForEachClass`, `catStat7D0`, `catUnknown24`, `catExternalShipStarted`,
  `catExternalShip`.
- **HWI deviations where the exe would fault or leak, each marked in the code** (`CAT-FIX` or a comment): the
  bullet-hit glow skips LOD levels without slots; `cat_salvageSpecialTarget` treats a vanished depot as no change
  (the exe reads NULL+0x58); `cat_clampUnclampAllFromShip` also stops when the list is gone; `cat_moduleShipEject`
  uses direction 0 for an unlisted module type (the exe indexes with the low byte of a pointer);
  `cat_mimicBecomeTarget` returns on an asteroid type outside 1-3; `cat_univDeleteMimicResource` frees the asteroid
  (the exe leaks 0x1B4 B); the big-gun shell is tested for NULL before it is tracked; the Beast-weapon hit entries are
  freed (the exe leaves them allocated).
- **Two readings of medium confidence**, noted in the reports: `rmGiveTechToPlayerByType`'s dependency index (the
  decompile's scan counter starts one entry past the set; the port gives `missing[k]`), and `getPlayerBountyRender`'s
  array read.
- **The 22 dormant HW1 types are gone**, and so are 14 HW1-only ship files; the stage-4 AI files ask for a Cataclysm
  type instead or drop the request, each site `CAT-TODO(stage 4 AI)`.
- **The LeakSanitizer suppressions** (`tools/lsan.supp`, 11 start-up allocations) and **`CAT_HW1_MISSION_SCRIPTS 0`**
  stand as in R3.
