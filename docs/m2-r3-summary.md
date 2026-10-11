<!-- done: exit-test, ports, ghidra, deltamap, left, r4-plan, decisions -->
# M2-R3 summary: stage 2 part A (stage-1 close-out, systemic headers, effects, selection, ship code A, universe statics)

Status: **COMPLETE, 2026-10-10.** Branch `m2`, worktree `.claude/worktrees/m2`, workflow `cycle/workflows/m2-r3.js`
(run `wf_914794bc-f5e`), 12 agents: 5 `src/` ports in sequence, each checkpointed by its auditor; one Ghidra stage in
parallel (the 3 name fixes); then this integrate step. Plan: docs/m2-r2-summary.md "Plan for M2-R3 and M2-R4". User
decisions of 2026-10-10: cycle/STATE.md "## M2".

**R3 exit test: met.** From a clean build, the debug `hwi-engine` (ASan + UBSan) on the plain harness takes single
player through the Mission01 level load into the running universe (`univUpdateCounter > 0`) with ASan 0. The front end
still reaches `Main_game_screen`. All self-tests match R2 or do better, and every quitting run now returns rc 0 (in R2
each one ended with GoK's `munmap` assert). One provisional switch is needed for sp to get this far:
`CAT_HW1_MISSION_SCRIPTS 0` (below and in "What is left").

## Exit test (this step, reproduced from a clean build at e068e39)

**Builds.** `tools/build.sh --debug --clean` and `--release --clean`: both rc 0, `[295/295] Linking target hwi-engine`,
0 `warning:` and 0 `error:` lines under `-Werror`.

| build | size | sha256 |
|---|---:|---|
| debug | 76,937,752 B | - |
| release (`-O2`) | 4,819,232 B | `9e242d53…4a7cc4` |

`tools/m2_layout32.sh`: OK. ILP32 sizes: `ShipStaticInfo` 1112, `GunStatic` 284, `CustShipHeader` 140,
`polygonobject` 116, `texreg` 52, `SENTENCELUT` 16, `ClampInfo` 56, `StaticCollInfo` 112, `AlternativeShips` 168,
`MilitaryParadeCommand` 260, `ranstream` 20, `SaveChunk` 8, `particle` 256, `eboltSystem` 36. All 24 ship-specifics
asserts hold.

**Harness.** `tools/m2_harness.sh` on `original/scratch-m2/m2r2-harness/data`: the GOG install's own files, no CET,
no workarounds. Offscreen driver, no input. Logs are in `original/scratch-m2/m2r2-harness/runs/r3-integrate-*/`.

| run | mode | rc / wall | milestone | first stop | sanitizers |
|---|---|---|---|---|---|
| `r3-integrate-sp` | sp, `--timeout 340` | 124 / 340.5 s (timeout) | **universe running (`univUpdateCounter > 0`)**: the last `debugScreenshots univUpdateCounter` line reads **1,248**, the same as `um-final-sp` and `uma-audit-sp-postfix`. Before that: Mission01 level load and the intro animatic `Movies/Animatic01.bik`, which plays in real time for about 246 s | none: still running at the timeout. 3,713 file loads; 33 ship statics loaded | **ASan 0**; no unsuppressed leak. UBSan: the same 8 non-fatal kinds as the universe-mex audit run, all misaligned members of GoK 64-bit packed structs: `fescreen` 101, `madanim` 27, `etgdecision` 21, `PATCH` 32, `etgfunctioncall` 10, `etgconditional` 8, `etgbranch` 8; plus `fquant.c` |
| `r3-integrate-fe` | fe, 40 s | 124 / 40.4 s | **`Main_game_screen`** | none: still running at the timeout (SIGTERM). The R2 `munmap` assert after the SIGTERM is gone | ASan 0; no unsuppressed leak. UBSan: the known misaligned `fescreen` (92) and `PATCH` (29) accesses, plus `fquant.c` (2 misaligned loads, 1 shift); all non-fatal and the same as R2 |
| `r3-integrate-statics` | statics | **0** / 10.7 s | ship statics self-test done | none | ASan 0, no leak. `shipstatic_engine_check.py`: **143 of 143** ship-type files parse as the probe does (OK 140 + OK-DECLARED-COUNT 3); OTHER-LOADED 27 (170/173 `.shp`); 2 types have no file |
| `r3-integrate-meshes` | meshes | **0** / 10.4 s | mesh self-test done | none | ASan 0, no leak. `mesh_engine_check.py`: `1758 files on disk, 1758 match, 0 mismatch, 0 not loaded, 0 extra` |
| `r3-integrate-textures` | textures | **0** / 15.6 s | texture self-test done | none | ASan 0, no leak. 12,403 listed, 10,661 registered and loaded, 0 not loaded, 1,742 rows with no file |
| `r3-integrate-oracle` | oracle (debug) | **0** / 9.5 s | oracle self-test done | none | ASan 0, no leak. 512 ticks, `randSyncErr=0 univSyncErr=0 blobSyncErr=0`, 772 records |
| `r3-integrate-etg` | etg, 128 ticks | **0** / 11.2 s | etg effect self-test done | none | ASan 0, no leak. 8 start-up scripts, 197 effects, 0 referenced but not loaded; 9 of 9 scripts parsed; 618 effects created, peak 2,515 effects. UBSan: the known misaligned `etgconditional` reports, a GoK 64-bit artefact |
| `tools/oracle/selftest.sh --label r3-integrate-oracle-rel` | oracle (release) | 0 | runs A and B `IDENTICAL`; perturbed run reported at tick 300 / `rng10` / 1 draw ahead; flipped copy reported at tick 400 / `ships_pos`; `rngs=27` in every `T` record | | **`SELFTEST PASS`** |

So every row of the R3 exit test holds:

| condition (docs/m2-r2-summary.md) | result |
|---|---|
| sp reaches `universe running` on Mission01, ASan 0 | **met** (`r3-integrate-sp`) |
| fe still `Main_game_screen` | **met** |
| self-tests unchanged or better | **met**. statics, meshes, textures and oracle give the same numbers as R2 and now exit rc 0 (R2: rc 1 from the quit assert). The new ETG/effect self-test passes |
| release oracle self-test `SELFTEST PASS` | **met**, with 27 RNG streams (HW1 and R2: 25) |
| a quitting run returns rc 0 | **met**: 5 of 5 quitting debug modes and 3 of 3 release oracle runs |

Two harness facts for the next rounds:
- **The sp timeout.** sp needs `--timeout` of about 300 s or more, because the animatic plays in real time. With the
  default 60 s the run ends inside `aviPlay`, where a SIGTERM is not handled (rc 137).
- **The next-level loop.** After about 816 ticks, GoK's `/debugScreenshots` hook calls `singlePlayerNextLevel()` every
  frame, because Cataclysm's next-level flow is not ported. The universe keeps running.

## The five ports and the Ghidra stage

| # | item | what landed | DONE WHEN | audit | commit |
|---|---|---|---|---|---|
| 1 | close-out + systemic headers ([report](m2-r3-closeout-headers.md), [audit](audits/m2-r3-closeout-headers.md)) | **Quit:** GoK's `munmap(heap, 0)` replaced by the recorded lengths. The SDL audio threads are stopped before the heaps are unmapped (the audit made this run on Linux). `tools/lsan.supp` covers 11 start-up allocations. **Randy:** `Randy.c` has 27 streams with the exe's numbering, the wall-clock advance of stream 0, `cat_ranResetGameStreams` and `realtimeelapsed`. **Headers:** 24 `SOF_*` flags renumbered, 3 retired (defined 0), 3 new (`SOF_CatCloakDevice` 0x200, `SOF_CatShipPart` 0x40000000, `SOF_CatDocking` 0x80000000, names chosen here). Also: 63 parade slots + `GetShipSlot`, `AlternativeShips` 0xA8, `ClampInfo` 0x38, `StaticCollInfo` and `CustShipFire` +0x11C pins. **Derelicts:** the 8-type table, with 50 HW1 ids kept dormant from 0x1000. **Strings:** the 1,705-row string-id enum, generated from the exe by `tools/m2_strings_gen.py`, with 226 HW1 ids kept dormant. **Saves:** save version 0x10ad0027, the chunk sizes, and the `saveCatLoadHook` hook for the user's saves. 27 static asserts; 19 delta-map reviews | met (the size-assert row partly met: `Universe` has no exe-size assert until the Player/Universe layout lands, M2-R4 #5) | PASS-WITH-FIXES, **0.92** (47/51). Fixes: the quit fix never ran on Linux (`_LINUX_FIX_ME`) and quits still crashed about 1 in 5; after the fix 25 of 25 rc 0. Also one review entry and 3 doc fixes | `2a5f51c` |
| 2 | ETG + particle argument arrays ([report](m2-r3-etg-particles.md), [audit](audits/m2-r3-etg-particles.md)) | The one-pointer callable convention (`etgFunctionCall` 0x0046e7b0) for every ETG/particle callable; GoK's generated wrappers (4 headers) are deleted. The exe's 167-row `etgFunctionTable`, of which 17 rows are new. The per-race script list. The 0x100-byte particle and particle system types 5/6. The bolt renderer, ported from the disassembly. `/catEtgParseTest` and `/catEtgEffectTest` (harness mode `etg`). 33 delta-map reviews | met: 9/9 scripts, 618 effects at 128 and 1,024 ticks with ASan 0 | PASS-WITH-FIXES, **0.875** (42/48). Fix: PART_ALPHA was forced on in `modifyColor`/`modifyAlphaValue`. Also 1 review and 4 doc fixes | `2ee01a0` |
| 3 | selection ([report](m2-r3-selection.md), [audit](audits/m2-r3-selection.md)) | The `ShipSelect.c` filters now match the exe bodies: `ioUpdateShipTotals` coverage 45/45, and six filters gain a skip-disabled argument with the exe's caller split. 33 of 59 cat-new functions ported (3,073 B). Per-ship Cataclysm state (`catDisabled` and others) appended to `Ship`. `Select.c` aliases and picking deltas. No HW1 alias is left in either file. 35 reviews, 1 class change | met | PASS-WITH-FIXES, **0.94** (47/50). Fixes: 1 CAT comment, the `clMove` comment, 1 review note | `91602f4` |
| 4 | ship code A ([report](m2-r3-ships-a.md), [audit](audits/m2-r3-ships-a.md)) | The 29 reused HW1 `CustShipHeader`s match the exe slot for slot (TargetDrone and JunkYardDawg headers removed). `GenericInterceptorStaticInit` has 14 type blocks. The StaticInits of 21 files are re-keyed. New `Game/Cloak.c`, `Salvage.c` and `BeastWeapon.c` (partly provisional). 97 cat-new functions are ported or hooked. 81 reviews, 19 IDENTICAL changed to MODIFIED | met: sp passes `GenericInterceptor.c (205)` and all 20 Mission01 StaticInits. The next stop, `LevelLoad.c (860)`, went to #5 | PASS-WITH-FIXES, **0.935** (100/107). Fixes: the `cat_gunGetBulletSpeed` second-mode fallback, 3 count/list errors, 3 comments | `ce5726d` |
| 5 | universe statics + MEX ([report](m2-r3-universe-mex.md), [audit](audits/m2-r3-universe-mex.md)) | **Level load:** `GetAppropriateShipTypeForRace` now uses the exe's 60-entry Sect/Beast swap table, and `scriptSetShipsCB`'s race override is partly ported. **`MEX.c`:** collision, dock and salvage chunks, nozzle and NAV parsing. **Dock statics:** the `DockStaticInfo` 0x50 header with `firstOfType`/`numOfType`/`center`, and `cat_dockFindDockStaticPoint`. **`Universe.c`:** mine statics for 7 races, team colours, the derelict loader over 8 types, init/reset/close. **`Globals.c`.** **CAT-PROVISIONAL `CAT_HW1_MISSION_SCRIPTS 0`:** no HW1 KAS script and no HW1 mission-start NIS until stage 6. 43 review rows; 9 IDENTICAL found modified | met: Mission01 level load completes and the universe runs (1,248 ticks in `um-final-sp`) | PASS-WITH-FIXES, **0.925** (37/40). Fixes: the mission > 17 team-colour case, DemoSect2's 12/11 count, a stale comment, 2 review notes, the 3 "4 `univUpdate` call sites" statements in docs/m2-r2-oracle.md (3 since this port) | `e068e39` |
| G | Ghidra name fixes (docs/binary-map.md "M2-R3 name fixes") | `ghidra/symbols_m2r2_namefix.json` applied; new export `ghidra/exports/cataclysm_gog_r6`. Renames: 0x00404090 `aieCheckEnemyNearby` to `aieCheckHealthLow`, 0x00404150 `aieCheckEnemyNotNearby` to `aieCheckHealthHigh`, 0x004aeb10 `meshFixupPacoUV` to `meshObjectFixupPacoUV`, with prototypes | applied 3, failed 0 | not audited (the 3 names were audited in M2-R2) | `d2c5494` |

**Round size** (`git diff 7ac664f HEAD`, the R3 launch to the last port): 134 tracked files changed, +14,900 / -4,976
lines. `src/` alone: 109 files, +11,284 / -4,694, with 1,295 added lines carrying `CAT` (`CAT:`, `CAT-PROVISIONAL`,
`CAT-TODO`, `CAT-DORMANT`, `CAT-FIX`). New source files: `Game/Cloak.c/.h`, `Game/Salvage.c/.h`,
`Game/BeastWeapon.c/.h`. Removed: GoK's 4 ETG wrapper headers. `runs.txt` logs 221 harness runs since the R2
integrate (all items, audits and this step).

## Delta map after the round

The ports recorded their findings as reviews in `ghidra/delta_map.json` (`tools/delta_map.py mark/merge` or direct
review rows). This step refreshed the file's stale `counts` line so that it matches its records. Before the refresh it
read 3,118 / 1,226 / 76 / reviewed 1,341; the records give 3,110 / 1,234 / 76 / 1,374.

| class | M2-R2 end | M2-R3 end | M2-R3 bytes |
|---|---:|---:|---:|
| IDENTICAL | 3,175 | 3,110 | 606,225 |
| MODIFIED | 1,168 | 1,234 | 623,296 |
| REWRITTEN | 77 | 76 | 9,326 |
| hand-reviewed | 1,191 | 1,374 | |

Per stage (`tools/m2_stage_counts.py --md`): HW1-matched IDENTICAL / MODIFIED / REWRITTEN functions, then cat-new.

| stage | M2-R2 end | M2-R3 end (MODIFIED bytes) | cat-new |
|---|---|---|---|
| 1 | 220 / 95 / 0 | 218 / 97 / 0 (51,602 B) | 76 |
| **2** | 860 / 512 / 30 | **804 / 569 / 29 (240,423 B)** | 829 / 187,242 B |
| 3 | 938 / 253 / 7 | 932 / 259 / 7 (173,691 B) | 429 |
| 4 | 403 / 74 / 2 | 403 / 74 / 2 (43,113 B) | 23 |
| 5 | 406 / 114 / 12 | 406 / 114 / 12 (74,848 B) | 67 |
| 6 | 348 / 120 / 26 | 347 / 121 / 26 (39,619 B) | 2,365 |

**What this means for M2-R4.** The ports compared every scope body with the exe, and they keep finding "IDENTICAL"
functions that differ:
- ships-a: 19 of 80 in its scope;
- universe-mex: 9 of 38;
- etg-particles: 24 in stage 2.

So stage 2's 804 IDENTICAL is an upper bound. The M2-R4 producers must compare each IDENTICAL body in their scope, as
the porting conventions already say.

The tool questions from the ports are not user decisions. Their owner is M2-R4 #5's tools subsystem (plan below):
- `delta_map.py report` would count the targeted review entries as calibration samples. Run on the current map, it
  would lower the IDENTICAL-agreement estimate from R2's 93.6% to about 80%, so it needs a sample filter.
  docs/m2-r1-function-deltas.md was therefore not regenerated.
- `delta_map.py classify` should be rerun against `cataclysm_gog_r6`. The `features` of the 3 renamed entries and
  `aieExecute`'s `calls_inlined` still refer to the old names.
- `m2_stage_counts.py` assigns 21 cat-new UI functions (`cat_shipPrint*`, `cat_selStatus*`/`cat_selHealth*`) to stage
  2 by the address-neighbour rule. They are stage-3 work.

## What is left of stage 2 part A

**Nothing blocks the stage-2 exit.** Each R3 port's DONE WHEN is met, and so is the R3 exit test. The deferred part-A
items have an owner in the reports' "What is left" sections. The stage-2 items among them are folded into the M2-R4
plan below, as the first subsystem of the producer that owns them:

1. **specialFlags / specialFlags2 renumbering** to the exe's bits. Examples: FriendlyStatus 0x40000, Hyperspacing
   0x200000, CircularGuard moving into specialFlags 0x40000000, the specialFlags2 bits 0x8 and 0x2000,
   `SPECIAL_CatSalvager`, the kamikaze bits 0x6. This is a systemic header change (selection Q1, ships-a 7), so it
   goes first in M2-R4 #2.
2. **`Universe`/`Player` layout.** This covers the exe-size assert (0x3D9BC / 0x6530), the per-player FOW pointer
   (Player+0x6500) and the upgrades at Player+0x6504. Needed by the oracle's Ship-health and Player checks: M2-R4 #5.
3. **The effect's +0x80 mesh state** (`cat_univShipMeshStateCreate`) and the +0x84 block, plus the per-ship mesh
   state helpers `cat_shipNozzleInfoGet` and `cat_meshObjectMatrixGet/Concat`: M2-R4 #1.
4. **Provisional ship state.** These are stand-ins whose real state lives in Cataclysm ship specifics:
   - the six Mimic queries;
   - `cat_shipIsSentinelFieldActive`, `cat_leechIsLeeching`, `cat_acvLinkState`;
   - the `catDisabled`/`catDisableReason` setters;
   - the salvage and Beast-weapon runtime in `Salvage.c`/`BeastWeapon.c`;
   - the gun weapon-type word.

   Owners: M2-R4 #3 and #4.
5. **`scriptSetShipsCB`'s open deltas.** Still open: HW1's CryoTray swap, the multiplayer gates, and the
   research-module, `kasHashString` and `clFormation` deltas. Also `unitCapCanCreateShip` beyond the gating, which
   needs `ConsMgr.c`'s 0x30-byte `shipinprogress` (universe-mex Q3: the R4 table named no owner). Both go to M2-R4 #5.

Outside stage 2, with the owners the reports name:
- **Stage 3:** the billboard, fog and lens-flare renderers; the status bars and the picking API; the ship info panel;
  speech/UI hooks; `fquant.c`/`fescreen` UBSan.
- **Stage 6:** the kas2c Mission01 script and the NIS-first start, which replace `CAT_HW1_MISSION_SCRIPTS`; the
  next-level flow; the single-player difficulty.
- **The user's saves:** the 32-bit save reader behind `saveCatLoadHook`, and the LP64 save blob, which changes with
  every appended `Ship` field.

## Plan for M2-R4: stage 2 part B

5 producers in sequence + 5 auditors + integrate (11 agents), as in M2-R3. The porting conventions, harness and session
rules stay the same. Each producer retires the HW1 aliases and the dormant HW1-only ids in the files it ports.

The counts come from `ghidra/delta_map.json` at the end of M2-R3, with `tools/m2_stage_counts.py`'s rules, grouped by
HW1 file. Cat-new is assigned by the nearest HW1-matched neighbour, so that column is a guess. For reference, R3's
files (`ETG`, `Particle`, `Select`, `ShipSelect`, `Randy`, `Universe`, `Globals`, `MEX`) account for
215 / 275 / 5 (71,592 B) and 118 cat-new.

| # | producer | scope | I / M / R fn (M bytes) | cat-new | done when |
|---|---|---|---|---|---|
| 1 | universe update + mesh state | `UnivUpdate.c`. The per-vertex glow runtime (`cat_univShipMeshStateCreate`, `cat_shGlowUpdate`, bullet-hit glow). The effect +0x80/+0x84 state and the per-ship mesh-state helpers (part-A item 3). `univDeleteDeadShip` with `MiningBaseDied`'s list removals and the mothership death-sequence trigger (ships-a 6) | 52 / 46 / 0 (30,978 B) | 51 / 15,092 B | sp on Mission01 runs at least as far as R3 (ASan 0) with the new `UnivUpdate.c`; the effect self-test still passes; every IDENTICAL body of the file is compared with the exe |
| 2 | command layer + formations | **First:** the specialFlags/specialFlags2 renumbering (part-A item 1). Then `CommandLayer.c`, `CommandWrap.c`, `Formation.c`; the Cataclysm command orders 11-17 and formation types 9/10 (`CAT_COMMAND_*`/`CAT_FORMATION_*` in `ShipSelect.c` until then); `SPECIAL_2_CatBeingBuilt` (external build); the HW1 functions with no exe counterpart that CommandWrap still calls | 87 / 65 / 1 (51,539 B) | 41 / 11,084 B | flags at the exe's bits with static checks; sp as far as before, ASan 0 |
| 3 | combat, docking, salvage | `Collision.c`, `Physics.c`, `Clamp.c`, `Gun.c` (damage by `WEAPON_*`/`GUNSIZE_*` and the gun weapon-type word), `Damage.c`, `Attack.c`, `Dock.c` (Cataclysm dock types, the sCarrier doors), `LaunchMgr.c` (module slots, Beast systems off), `ResCollect.c` (asteroid break pieces). The salvage runtime (`Salvage.c` provisional entries, `cat_salvageTargetAllowed`, `cat_salvageResetGlobals`) and the deletion of `SalCapCorvette.c`'s dormant HW1 salvage code and its readers. The Beast infection weapon runtime. Kamikaze steering/hit, `cat_blastCreate`/`cat_blastReset` | 48 / 56 / 5 (34,228 B) | 42 / 20,039 B | `SalCapCorvetteHeader` allocates the exe's 0x30 bytes; no HW1 salvage reader left; sp as far as before, ASan 0 |
| 4 | ship code B: the Cataclysm-only ships | The 22 headers on `DefaultShipHeader` (Acolyte, Mimic, Leech, ACV, MCV, RammingFrigate, MultiBeamFrigate, HiveFrigate, Swarmer, Dreadnought, ModuleShip, Worker, RepairDroid, Processor, Sentinel, CruiseMissile, RKO, Slipgate, Crystal, dBeastMothership, NomadMoon, NomadFieldEmitter) and the cloak-generator NULL headers. The rest of Ships/* cat-new: the sMothership big gun (`cat_bigGun*`), dissolve, the disable/EMP/drift functions, `cat_madAnimationFinished`. The provisional queries move to their home files (part-A item 4). The 22 dormant HW1 types (ids 200-221), `DefenseFighter.c` and the dormant `RepairCorvette` header are deleted | Ships/* 61 / 64 / 12 (18,603 B; R3 ported the reused headers' MODIFIED bodies) | Ships/* 422 / 78,155 B (R3 ported or hooked 97) | every Mission01 ship type uses the exe's header; no HW1 ship type left; statics 143/143 with the StaticInits under ASan |
| 5 | the rest, Player layout, oracle stage 2, tools | `Clouds.c`, `Nebulae.c`, `Camera*.c`, `Stats.c` (fight code, GameStats layout), `ResearchAPI.c` (upgrades: `catUpgradeFlags`, `cat_playerUpgradesReset`), `Crates.c` (`cratesRewardPlayer`), `Captaincy.c`, `Alliance.c`, `Tactics.c`, `Eval.c`, `Bounties.c`, `Blobs.c`, `Region.c`, `Memory.c`, `Task.c` (`cat_taskRealTimeElapsed`, time compression in place of `universeTurbo`), `HS.c`, `FlightMan.c`, the math files. Also: **`Universe`/`Player` layout** with size asserts (part-A item 2); `scriptSetShipsCB` and `unitCapCanCreateShip` (part-A item 5, takes `ConsMgr.c`'s `shipinprogress`). **Oracle stage 2:** Ship health offset and Player layout in the checksum; a **headless Mission01 oracle run**, meaning sp with the virtual clock and `/oracleDump`, no real-time animatic (check `/disableAVI` or add a harness switch), and the `/debugScreenshots` next-level loop kept out of it; the save tick-0 compare at the `saveCatLoadHook` (waits for the saves). **Tools:** the `delta_map.py report` sample filter, a `classify` rerun on `cataclysm_gog_r6`, and moving the 21 UI cat-new functions to stage 3 in `m2_stage_counts.py` | 341 / 63 / 6 (33,483 B) | 155 / 30,655 B | two headless Mission01 runs of a few thousand ticks with ASan 0 give byte-identical `.hwo` streams |

Stage 2 in all, at the end of R3: 804 / 569 / 29 HW1-matched (240,423 MODIFIED bytes) + 829 cat-new (187,242 B).

**R4 integrate (stage-2 exit):** from a clean build on the plain harness:
- Mission01 runs a few thousand ticks headless with ASan 0;
- two `hwi-engine` runs of it give byte-identical `.hwo` streams (oracle mode C on the recreation side);
- fe still reaches `Main_game_screen`;
- the self-tests are unchanged or better;
- the release oracle self-test reports `SELFTEST PASS`;
- quitting runs return rc 0.

## Decisions

**Answered by the user, 2026-10-10:**
- The faithfulness defaults already in the tree are accepted (R2 item 11):
  - the 22 dropped HW1 types stay dormant at ids 200-221 until M2-R4 #4 deletes them;
  - `k`/`tCloakGenerator` use `DefaultShipHeader` where the exe has NULL;
  - `crateInit` resets 46 entries;
  - the mission-17 texture cut follows the stock exe;
  - the fq audio code follows the binary.
- The Ghidra name-fix pass ran (R2 item 12): commit `d2c5494`, export `cataclysm_gog_r6`.
- The oracle reference recording and the two saves come later and do not block (R2 items 6 and 8). The hooks are in
  place: `saveCatLoadHook` in `SaveGame.c` for the saves, `tools/oracle/` and docs/m2-r2-oracle.md "What is left" 1
  for the recording.

**Still open (the user's):**
- **6.** The first oracle reference recording: which skirmish map and which players.
- **8.** Two saves from the M0 play-test, one mission and one skirmish.
- **10.** The first live oracle session: load the DLL under Proton, record, then two playback runs.
- **The M0 play-test sign-off.**

**Surfaced by M2-R3.** Each has a default in the tree; tell us if you want otherwise.
- **The LeakSanitizer suppressions.** `tools/lsan.supp` covers 11 start-up allocations of libdbus, the GL driver,
  GoK's SDL mutexes and one `rinit` mode record. Every other leak still fails a run. The entries are to be dropped when
  stages 3 and 5 port those paths.
- **`CAT_HW1_MISSION_SCRIPTS 0` (CAT-PROVISIONAL)** stays until stage 6 ports Cataclysm's kas2c missions and its
  NIS-first start. Without it, sp stops at `KAS.c (265)` on HW1's Mission01 script.
- **Three new `SOF_*` flags got names chosen here,** because the exe's names are unknown: `SOF_CatCloakDevice`,
  `SOF_CatShipPart`, `SOF_CatDocking`. Three HW1 flags with no exe bit are defined 0: `SOF_CloakGenField`,
  `SOF_Cloaking`/`DeCloaking`, `SOF_Slaveable`.
- **The LP64 save blob changes with each appended `Ship` field,** so no save layout is pinned until the user's saves
  arrive (decision 8).
