<!-- done: exit-test, ports, reread, left, r3-plan, decisions -->
# M2-R2 summary: stage 1, formats and data load

Status: **COMPLETE, 2026-10-10.** Branch `m2`, worktree `.claude/worktrees/m2`, workflow `cycle/workflows/m2-r2.js`
(run `wf_3583f3e2-1ab`), 13 agents: 5 `src/` ports in sequence, each checkpointed by its auditor, the delta-map re-read
in parallel, then this integrate step. Plan: docs/m2-r1-summary.md "M2-R2 plan". User decisions of 2026-10-10:
cycle/STATE.md "## M2".

**Stage-1 exit test: met.** From a clean build, debug `hwi-engine` (ASan + UBSan) reaches `Main_game_screen` on the
unmodified Cataclysm install, with none of data-deltas' 15 workaround stages and no mesh down-conversion. Single player
gets past the run27 `RACE_*` level-load stop. The next stop is stage 2's ship code.

## Exit test (this step, reproduced from a clean tree at 5687473)

Builds: `tools/build.sh --debug --clean` and `--release --clean`, both rc 0, `[293/293] Linking target hwi-engine`,
0 `warning:` and 0 `error:` lines under `-Werror`. Debug 75,796,880 B; release 4,668,184 B (`-O2` since the oracle item),
sha256 `ad633672…08f69c`. `tools/m2_layout32.sh`: OK (ILP32 `ShipStaticInfo` 1112, `GunStatic` 284, `CustShipHeader`
140, `polygonobject` 116, `texreg` 52, `SENTENCELUT` 16). `tools/oracle/dll/build.sh --clean`: `hwi_oracle.dll`
23,040 B, loader 5,120 B.

Harness: `tools/m2_harness.sh` on `original/scratch-m2/m2r2-harness/data` (symlinks to the GOG install's own files under
their own names, no CET, no workarounds), offscreen driver, no input, no `/noSpeech`. Logs in
`original/scratch-m2/m2r2-harness/runs/integrate-exit-*/`.

| run | mode | rc / wall | milestone | first stop | sanitizers |
|---|---|---|---|---|---|
| `integrate-exit-fe` | fe, 40 s | 124 / 40.7 s | **`Main_game_screen`** (`feScreenPush: pushed screen 1`, log line 1396; 21 engine-glow loads) | GoK's shutdown assert `SDL/utility.c (3484) result != -1` (`munmap(heap, 0)`) after the SIGTERM at the timeout: not a data stop | ASan 0. UBSan: only the known misaligned `fescreen`/`PATCH` accesses plus GoK's `fquant.c` (2 misaligned `int` loads, 1 shift by 31), all non-fatal |
| `integrate-exit-sp` | sp, 60 s | 1 / 12.1 s | **level load (mission file read)**: `New single player game started`, `player is race 0`, `Mission01.mission` read 3 times with both `.missphere`, then `shipstatic: loaded RACE_SECT/sRecon.shp type=0 race=0 keys=69 guns=1 ...` | `Ships/GenericInterceptor.c (205): Assertion of (interceptorstat != NULL) failed`: the stage-2 slot body (exe `GenericInterceptorStaticInit` 0x005b1e70 covers 14 Cataclysm types) | ASan 0; UBSan as above plus 7 `TaskBar.c` `fescreen` |
| `integrate-exit-statics` | statics | 1 / 13.4 s | ship statics self-test done | shutdown assert | ASan 0. `shipstatic_engine_check.py` (exe + base data): 143/143 ship-type files parse as the probe does (3 with the declared nav count); 27 other `.shp` loaded, 3 not loaded (never read by the exe); 2 types without a `.shp` logged as disabled |
| `integrate-exit-meshes` | meshes | 1 / 12.3 s | mesh self-test done | shutdown assert | ASan 0. `mesh_engine_check.py` (base + update): `1758 files on disk, 1758 match, 0 mismatch, 0 not loaded, 0 extra` |
| `integrate-exit-textures` | textures | 1 / 21.2 s | texture self-test done | shutdown assert | ASan 0. 12,403 listed / 12,403 found / 10,661 registered and loaded / 0 not loaded / 1,742 rows with no file |
| `integrate-exit-oracle` | oracle (debug) | 1 / 10.8 s | oracle self-test done, 512 ticks, 0 sync errors | shutdown assert | ASan 0; 772 records, 140,723 B |
| `tools/oracle/selftest.sh --label integrate-oracle-rel` | oracle (release) | | A and B byte-identical, `IDENTICAL`; perturbed run: tick 300 / `rng10` / 1 draw ahead; flipped copy: tick 400 / `ships_pos` | | `SELFTEST PASS` |

So the two literal DONE WHENs that the shipstatics and mesh items could only show on isolation trees now hold on the
plain harness: 170 of 173 `.shp` load (143 ship types + 27 others; the 3 left are never read by the exe) and the mesh
stop is passed with the original 0x74 meshes.

## The six items

| # | item | what landed | DONE WHEN | audit | commit |
|---|---|---|---|---|---|
| 2 | identifiers + tweak tables ([report](m2-r2-identifiers.md)) | 145 ship types / 7 races / 12 classes with the exe's ids and names (`ShipDefs.h`, `RaceDefs.h`, `ClassDefs.h`, `ObjTypes.c`); 16 type/race/class tables re-keyed from the exe, 7 `CAT-PROVISIONAL`; `aiship`, `tweak`, `crates`, `Tactical`, `Tactics`, 12 parade files, `Dock.script` dropped, `GatherStatsFor`, `colPick`, `UnitLimitCaps`, `.mission` Include/ExcludeShips; HW1 names kept as 37 aliases + 22 dormant ids 200-221; `tools/m2_harness.sh` + `m2_harness_iso.sh` | met (ranks 2, 3, 5, 7 and both ASan writes gone, level load past `RACE_*`), on the plain harness since startup | PASS-WITH-FIXES, **0.905** (38/42); 4 fixes (unsigned range check, 3 addresses/strides) | `b4e0f23` |
| 3 | ship statics ([report](m2-r2-shipstatics.md)) | `ShipStaticInfo` 0x458, `GunStatic` 0x11C, `CustShipHeader` 0x8C / 34 slots (layout; bodies are stage 2), 178-key ship table, 39-key gun table, gimbal/`WEAPON_*`/`GUNSIZE_*`/35 `DPT_*` names, InitStatShipInfo defaults; 233 static asserts (`tools/m2_layout32.sh`); `/catShipStaticsTest` | met now: 143/143 ship types as the probe, 170/173 `.shp` (was 167 before the mesh item); no type silently disabled | PASS-WITH-FIXES, **0.953** (41/43); force-field threshold fixed to 1.0; 11,798 parsed values checked against the exe | `da09c2f` |
| 1 | mesh 0x500 ([report](m2-r2-mesh.md)) | `polygonobject` 0x74 with the per-vertex info list at +0x1C, no version check, `.lod` → `.peo`, GoK's 64-bit converter rewritten (shared lists stay shared), per-vertex colour path (`shColourSet0` 3 args, `cat_shColourAddGlow`, `enableCPV`, `/noCPV`); `/catMeshLoadTest` | met: 1,729/1,729 `.geo/.peo` + 29 `.mad`, 0 ASan; 21 engine glows at start-up; mesh stop passed on the plain harness since startup | PASS-WITH-FIXES, **0.956** (43/45) | `fe8645d` |
| 4 | start-up names, audio, FE art, textures ([report](m2-r2-startup.md)) | `Cataclysm.big`/`Update.big` (case-correct open), `Music4Cat`, `SectSpeech`/`BeastSpeech`, Cataclysm speech LUTs and EQ scripts, 88 battle-chatter events, renamed construction/research art, `feAllCallOnCreate`, texreg 0x34 entry + 4th `trTextureRegister` argument, `LL_MaxElements` above 12,403, texture-memory fitting; GoK archive age-array defect fixed (allocated at a quarter of its size) | met: ranks 4, 6, 8, 9, 10 cleared without `/noSpeech`; `textures.ll` fully registered; plain harness reaches `Main_game_screen` | PASS-WITH-FIXES, **0.898** (44/49); `cmDrawShipImage` carrier-image defect fixed | `7de1b18` |
| 5 | checksum oracle, recreation side ([report](m2-r2-oracle.md)) | `.hwo` writer (`/oracleDump`, `-Doracle`), scripted lockstep start `/oracleSelfTest`, `tools/oracle/hwo_compare.py` + `selftest.sh`; CAT: 16 Hz default (`/gok64Hz` for GoK's 64 Hz), ships-only `univGetChecksum`, one-CRC `statsGetStatChecksum`; `-fno-strict-aliasing -ffp-contract=off -fno-fast-math`, release `-O2`; GOG-side DLL + loader (H1-H7, calls the game's own record/playback) built with clang-18 + lld-link | met on the scripted start (no recording exists): byte-identical streams twice, right tick on perturbed copies; DLL builds, `check_sites.py` 33/33; never loaded | PASS-WITH-FIXES, **0.96** (48/50); MXCSR status-flag defect in `fpcw` fixed | `5687473` |
| R | delta-map re-read ([report](m2-r2-reread.md)) | all 1,013 IDENTICAL/low functions read against ad1bb82; R1 audit reclassifications applied; `tools/delta_map.py` fixes (folds, callee arity, one-pointer callables, KAS hash keys, deterministic order); `tools/m2_stage_counts.py`; 3 name-fix proposals (`ghidra/proposals/m2-r2-name-fixes.json`, not applied: needs a Ghidra run) | met | PASS, **1.00** (40/40) | `ddc8a18` |

Round size: `git diff --stat 0a913f7 HEAD`: 112 files under `src/` and `tools/`, +9,621 / -2,898 lines; `src/` alone
94 files, +6,285 / -2,814, 742 added lines containing `CAT` (`CAT:` and `CAT-PROVISIONAL` tags). 137 harness runs in `runs/runs.txt` (all items, audits and
this step).

## The re-read's totals

1,013 read: 870 IDENTICAL (85.9%), 140 MODIFIED, 3 REWRITTEN; 143 changed class (59 ETG/particle callables taking a
pointer to their argument slot, 10 KAS hash keys, 8 ship-type/race tests, 4 callee-arity, 62 other logic). With the
audit reclassifications and tool fixes, the HW1-matched split is:

| class | M2-R1 | M2-R2 | M2-R2 bytes | share |
|---|---:|---:|---:|---:|
| IDENTICAL | 3,357 | 3,175 | 633,417 | 51.1% |
| MODIFIED | 983 | 1,168 | 595,973 | 48.1% |
| REWRITTEN | 80 | 77 | 9,457 | 0.8% |

1,191 functions are now hand-reviewed. Estimated true IDENTICAL agreement: about 93.6% (M2-R1 audit: 80%). Per stage
(HW1-matched IDENTICAL / MODIFIED / REWRITTEN functions, then cat-new, from `tools/m2_stage_counts.py`):
stage 1 220 / 95 / 0, 76 cat-new; **stage 2 860 / 512 / 30 (184,562 / 216,998 / 4,189 B), 829 cat-new (187,242 B)**;
stage 3 938 / 253 / 7, 429; stage 4 403 / 74 / 2, 23; stage 5 406 / 114 / 12, 67; stage 6 348 / 120 / 26, 2,365.

## What is left of stage 1

The exit test is met. These stage-1 items were found or deferred during the round and are carried into M2-R3's first
producer (they are small, and none blocks the exit test):

1. **Derelict type table** (`ObjTypes`): Cataclysm has 8 derelict types (0x00889b68: `Crate`, `Homeworld`,
   `PlanetOfOrigin`, `PlanetOfOrigin_scarred`, `Galaxy`, `LavaBall`, `MattWorld`, `HyperspaceGate`); HW1 has 55, used
   in 157 source lines. `Galaxy`/`LavaBall`/`MattWorld` are appended as `CAT-PROVISIONAL` so their meshes load.
2. **GoK quit defect**: `utyGrowthHeapFree` calls `munmap(heap, 0)` (EINVAL on Linux), so every run that quits after a
   growth heap was created ends with the `utility.c (3484)` assertion (every row above). Not a Cataclysm delta.
3. **Save container and chunk sizes** (`SaveGame.c`): the R1 plan's "container and chunk sizes only" was not ported
   this round; `ShipStaticInfo` saves are LP64-sized. The full layout still waits for the user's two saves.
4. **Delta-map bookkeeping**: the ports and their audits found 13 functions the map calls IDENTICAL whose exe body
   differs from HW1 (`colPickScalarScan`, `SetInfoNeededForShipAndRelatedStaticInfo`,
   `CopyPointersFromRealShipStaticInfo`, `meshLoad`, `cmCloseTextures`, `lmFreeTextures`, `rmGUIShutdown`,
   `feAllCallOnCreate`, `etgSetTextureParse`, `meshRecolorize`, `scriptSetNAVLightCB`, `subThemePictureSet`,
   `subTexturesReset`) and one MODIFIED that is IDENTICAL (`madFileLoad`). Record them as reviews in
   `ghidra/delta_map.json` and re-run `tools/m2_stage_counts.py`. (`precalcStaticInfoGuidance` differs only in operand
   order, docs/m2-r2-shipstatics.md "What is left" 11.)
5. **Cataclysm string-id enum** (1,705 `LanguageStrings` rows, `strNameToEnum` 0x005288f0): until it lands,
   `ShipTypeToNiceStr` shows unrelated strings. Its readers are UI, so it can also go to stage 3; listed here because
   `StringSupport` is a stage-1 file.

Deferred on purpose to later stages (owner in the item reports): MEX (`mexLoad` salvage chunks, `mexSetCollisionInfo`)
and the per-vertex glow runtime → stage 2; texture-memory option, speech/chatter runtime, FE draw code, `fquant.c`
UBSan → stage 3; mission-17 texture cut → stage 6.

## Plan for M2-R3 and M2-R4: stage 2, universe and ships

Stage 2 is 1,402 HW1-matched functions (860 / 512 / 30) plus 829 cat-new functions (187,242 B). It splits over two
rounds of 5 producers + 5 auditors + integrate (11 agents each). Producers run in sequence (they share `src/` and the
build), as in M2-R2. Porting conventions, harness and session rules stay as in M2-R2. Each producer also retires the
HW1 type aliases (1,387 lines outside `ShipDefs.h`) and the dormant HW1-only ids in the files it ports.

Counts are HW1-matched IDENTICAL / MODIFIED / REWRITTEN functions and bytes by HW1 file from `ghidra/delta_map.json`
with `tools/m2_stage_counts.py`'s stage rules; cat-new by nearest HW1-matched neighbour (the tool's rule, so a guess).

**M2-R3: stage-1 close-out, systemic headers, effects, selection, ship code A, universe statics**

| # | producer | scope | I / M / R fn (M bytes) | cat-new | done when |
|---|---|---|---|---|---|
| 1 | close-out + systemic headers | the 5 stage-1 leftovers above; `SOF_*` renumbering (`SOF_Crazy` 0x200000, hidden 0x8000), `MAX_MILITARY_SLOTS` 63 + parade slot enum, `AlternativeShips` 0xA8, `ClampInfo` 0x38, `StaticCollInfo` lengths, `CustShipFire` at +0x11C; `Randy.c` 27 streams (particles on 25), wall-clock advance in `ranStartup`/`cat_ranResetGameStreams`, `realtimeelapsed` (oracle prerequisites) | Randy 8 / 0 / 0 | 15 / 8,705 B | quit returns rc 0; size asserts for every changed struct; oracle self-test still PASS with 27 streams |
| 2 | ETG + particle argument arrays | the one-pointer callable convention (59+ functions), ETG per-race scripts (`etg\race_*.script`), Particle.c and ETG.c | 171 / 164 / 2 (21,936 B) | 24 / 6,685 B | every `etg\*.script` of the install parses; ASan 0 in an effect self-test |
| 3 | selection | `ioUpdateShipTotals` in the 30 selection filters, `Select.c`, `ShipSelect.c` | 33 / 44 / 4 (14,536 B) | 59 / 11,278 B | filters compile against the new flags; no HW1 alias left in both files |
| 4 | ship code A: HW1 headers | `CustShipHeader` slot wiring (slots 9, 12-14, 20, 22-34; exe contents in `original/scratch-m2/m2r2-shipstatic-tools/headers.txt`), the StaticInit and slot bodies of the HW1 headers the exe reuses, `GenericInterceptorStaticInit` (0x005b1e70, 14 types) first | Ships/* 80 / 45 / 12 (15,070 B) | part of Ships/* 422 / 78,155 B | the sp harness passes `GenericInterceptor.c (205)` |
| 5 | universe statics + MEX | `Universe.c` (mine statics over 7 races, `universeDefaultTeamColors`, derelict loader with the new table), `MEX.c` (`mexLoad`, `mexSetCollisionInfo`, `collrectmid`), `DockStaticInfo` 0x50 header and `scriptSetDockStatics`, `Globals.c` | 38 / 31 / 0 (15,684 B) | 20 / 5,549 B | Mission01 level load completes; sp reaches `universe running` |

R3 integrate: sp on the plain harness reaches `universe running` (`univUpdateCounter > 0`) on Mission01 with ASan 0;
fe still `Main_game_screen`; the five self-tests unchanged; oracle `SELFTEST PASS`.

**M2-R4: simulation, commands, combat, Cataclysm-only ships, the rest**

| # | producer | scope | I / M / R fn (M bytes) | cat-new |
|---|---|---|---|---|
| 1 | universe update | `UnivUpdate.c`; the per-vertex glow runtime (`cat_univShipMeshStateCreate`, `cat_shGlowUpdate`, bullet-hit glow) | 52 / 46 / 0 (30,978 B) | 51 / 15,092 B |
| 2 | command layer + formations | `CommandLayer.c`, `CommandWrap.c`, `Formation.c` | 88 / 64 / 1 (51,225 B) | 41 / 11,084 B |
| 3 | combat and docking | `Collision.c`, `Physics.c`, `Clamp.c`, `Gun.c` (damage from `WEAPON_*`/`GUNSIZE_*`), `Damage.c`, `Attack.c`, `Dock.c`, `LaunchMgr.c`, `ResCollect.c`, the salvage system (`DPT_Salvage`) | 49 / 55 / 5 (34,086 B) | 42 / 20,039 B |
| 4 | ship code B: Cataclysm-only headers | the 22 headers on `DefaultShipHeader` (Acolyte, Mimic, Leech, ACV, MCV, RammingFrigate, MultiBeamFrigate, HiveFrigate, Swarmer, Dreadnought, ModuleShip, Worker, RepairDroid, Processor, Sentinel, CruiseMissile, RKO, Slipgate, Crystal, dBeastMothership, NomadMoon, NomadFieldEmitter); the cloak-generator NULL headers | none (new files) | rest of Ships/* 422 / 78,155 B |
| 5 | the rest + oracle stage 2 | `Clouds.c`, `Nebulae.c`, `Camera*.c`, `Stats.c` (fight code), `ResearchAPI.c`, `Crates.c` (`cratesRewardPlayer`), `Captaincy.c`, `Alliance.c`, `Tactics.c` (rest of `TacticsInfotable`), `Eval.c`, `Bounties.c`, `Blobs.c`, `Region.c`, `Memory.c`, `Task.c`, `HS.c`, `FlightMan.c`, math files; oracle: Ship health offset and Player layout, save loader tick-0 compare (needs the user's saves) | 341 / 63 / 6 (33,483 B) | 155 / 30,655 B |

R4 integrate (stage-2 exit): Mission01 runs a few thousand ticks headless with ASan 0, and two `hwi-engine` runs of it
give byte-identical `.hwo` streams (oracle mode C on the recreation side).

## Decisions for the user (surfaced or still open)

Still open from M2-R1: **6** which skirmish map and players become the first oracle reference recording; **8** two
saves from the M0 play-test (one mission, one skirmish). New this round:

- **mingw is not needed**: the oracle DLL builds with clang-18 + lld-link (decision 6 of 2026-10-10 settled without a
  package install).
- **The first live oracle session** is the user's: copy `build/oracle-dll/hwi_oracle.dll` and
  `hwi_oracle_loader.exe` next to `cataclysm.exe`, record one skirmish (decision 6), then two playback runs. Procedure:
  docs/m2-r2-oracle.md "What is left" 1. Unverified until then: the DLL loads under Proton, the H6 trigger, `vqpc`.
- **Faithfulness choices** with a default already in the tree (say if you want otherwise): the 22 dropped HW1 types
  stay dormant until stages 2/4 delete their code; `kCloakGenerator`/`tCloakGenerator` use `DefaultShipHeader` where
  the exe has NULL; `crateInit` resets 46 entries as the exe does; the mission-17 texture cut follows the stock exe,
  not CET; the fq audio functions should follow the binary (`fqMix`, `fqEqualize`, `fqDecOver`), not GoK.
- **A Ghidra run** (one small pass) to apply the 3 name fixes in `ghidra/proposals/m2-r2-name-fixes.json`.
