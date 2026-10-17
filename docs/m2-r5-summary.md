<!-- done: exit-test, ports, deltamap, left, r6-plan, decisions -->
# M2-R5 summary: stage-2 close-out + stage 3 part A (layout + dock purposes, command layer + formations + tactics, render/GL, sound and speech, HUD)

Status: **COMPLETE, 2026-10-17.** Branch `m2`, worktree `.claude/worktrees/m2`, workflow `cycle/workflows/m2-r5.js`.
The round ran as two workflow runs: `wf_2afecf39-75d` (launched 2026-10-11 from base `1434b90`, cut off by the weekly
usage limit on 2026-10-12 with the layout-dock port produced but unaudited, committed as the WIP snapshot `f59e2ef`)
and `wf_9f1eaba2-4de` (relaunched 2026-10-16 after the weekly reset with the ports, audits and the probe on opus
(high) and this integrate step on fable (max); it skipped the layout-dock producer and ran its audit first). 12
agents (5 `src/` ports in sequence, 5 auditors, the probe, this integrate step). Each port was checkpointed by its
auditor. Plan: docs/m2-r4-summary.md "Plan for M2-R5 and M2-R6" (the M2-R5 table) and "What is left of stage 2"
(items 1-7). User decisions: cycle/STATE.md "## M2" (the 2026-10-10 faithfulness defaults and the M2-R4 defaults
17-22 are applied; the oracle reference recording and the two saves still come later and did not block).

**R5 exit test: met.** From a clean build on the plain harness (silent, `tools/m2_harness.sh` only): (a) the stage-2
exit test still holds: two 6,000-tick headless Mission01 runs of the debug `hwi-engine` (ASan + UBSan) give
byte-identical `.hwo` streams with ASan 0, the release build gives the same 6,001 `T` records, a run perturbed by one
`gamerand()` and a flipped checksum bit are caught at their ticks, sp reaches the running universe, fe reaches
`Main_game_screen`, the statics / meshes / textures / oracle / etg self-tests give R4's numbers, the release oracle
self-test says `SELFTEST PASS` with 27 RNG streams, every quitting run returns rc 0, `tools/m2_layout32.sh` holds
every ILP32 pin with **`Ship` 0x63C**; (b) the five new self-tests `dock`, `command`, `render`, `sound` and `hud`
complete with ASan 0 (sound: 0 UBSan reports in `fquant.c` / `fqcodec.c`, 0 `struct PATCH` reports); (c) no
`CAT-TODO` / `CAT-PROVISIONAL` in `src/` names an M2-R4 (or M2-R5) owner any more: this step settled the last nine
(two small ports and seven re-targets, below). Stage 2 is closed except for the two provisional switches
`CAT_HW1_MISSION_SCRIPTS 0` (stage 6) and `CAT_EXTERNAL_BUILD_DRIVER 0` (M2-R6 producer 4), the repair-droid runtime
and the externally built ships' driver (both M2-R6, both move the mission stream when they land).

## Exit test (this step, reproduced from a clean build at 02e8e4d plus this step's nine marker edits)

**The integrate edits** (uncommitted when the builds ran; committed by this step's checkpoint): the eight markers
whose owner still named M2-R4 and the one split-line `CAT-TODO(M2-R5 hud)` the hud grep missed. Two are ports
grounded in the r6 decompile and the disassembly: `univRemoveAllShipReferencesFromSpecialShips` 0x005476e0 now also
drops the removed ship as a `catQueuedAttackTarget` (Ship +0x4B0) of every ship on the ShipList, and
`cat_univConvertShip` 0x00543950 copies each gun's current-weapon word (`Gun` +0x00) to the new ship for the first
min(numGuns) guns (the 0x88-stride loop at 0x00543ad6). Three are closed as dead arguments: the exe pushes a third
word to `univDeleteDeadShip` 0x00547d00 (`push 1` at 0x00543ba8), a second to `univWipeShipOutOfExistence`
0x005477d0 (`push 2` at 0x00543bce) and a second to `univReallyDeleteThisShipRightNow` 0x00548f00, and none of the
three bodies reads it. Four are re-targeted with their reason: `univCreateShip`'s header now lists what is done
since M2-R4 and gives the three leftovers to M2-R6 ships (the repair-droid array), M2-R6 ConsMgr (`cat_suShipAdded`,
the external-build start, the factory block) and M2-R6 FOW (the FOW cube); the asteroid box clamp of
`univUpdateAllPosVelResources` 0x005432d0 (attribute bit 0x200, the box at Asteroid +0x1A8..+0x1B0, three reserved
dwords nothing writes yet; neither `univAddAsteroid` 0x00540a30 nor `scriptSetResourcesCB` 0x004a07b0 stores it) goes
to stage 6 level load / KAS; the `OP_ExperienceMessages` toggle goes to M2-R6 front end (Options.c); the "7-argument
univCreateShip" note in `cat_univConvertShip` is closed as this tree's split. Every edit carries `[integrate: M2-R5]`.
The two ports do not touch the Mission01 stream (this step's stream is IDENTICAL to the hud audit's, below).

**Builds.** `tools/build.sh --debug --clean` and `--release --clean`: both rc 0, `[308/308] Linking target hwi-engine`
(R4: 305; three new files), 0 `warning:` and 0 `error:` lines under `-Werror` (the only WARNING line is meson's
sanitizer notice), 11.1 s and 10.2 s of wall time.

| build | size | sha256 |
|---|---:|---|
| debug (ASan + UBSan) | 82,303,928 B | `fa6744b2fa79fe16…` |
| release (`-O2`, no sanitizers) | 5,023,224 B | `56b8a7bcee70d8c1…` |

`tools/m2_layout32.sh`: OK. ILP32 sizes: `ShipStaticInfo` 1112, `GunStatic` 284, `CustShipHeader` 140, `polygonobject`
116, `texreg` 52, `SENTENCELUT` 16, `ClampInfo` 56, `StaticCollInfo` 112, `AlternativeShips` 168,
`MilitaryParadeCommand` 260, `ranstream` 20, `SaveChunk` 8, `particle` 256, `eboltSystem` 36, `Player` 25904,
`Universe` 252348, `PlayerStats` 2040, `TechStatics` 31412, `shipinprogress` 48, **`Ship` 1596 (0x63C), `Missile` 600
(0x258), `Bullet` 356 (0x164), `Derelict` 660 (0x294), `Asteroid` 436 (0x1B4), `CommandToDo` 816 (0x330),
`FormationCommand` 96, `TacticsInfo` 2500 (0x9C4)**, plus the `PATCH` / `PATCH_DISK` 0x38 and the HUD structs
(`CatOverlayIcon` 0x20, `CatOverlayIconTexture` 0x108, `CatShipName` 0x70, `CatIoMessage` 0x224, `CatCmJob` 0x28);
the specifics asserts of all 45 ship files hold.

**Harness.** `tools/m2_harness.sh` on `original/scratch-m2/m2r2-harness/data` (the GOG install's own files, no CET, no
workarounds), offscreen video, dummy audio (docs/CONSTRAINTS.md 5), no input. The two binaries were copied to
`original/scratch-m2/m2r5-integrate/` first so that no rebuild could touch a running test. Logs are in
`original/scratch-m2/m2r2-harness/runs/r5-integrate-*/`. The runs below were made from three parallel chains (the
mission test is on the virtual clock and does not depend on the wall clock; sp and fe ran afterwards).

| run | mode | rc / wall | milestone | first stop | sanitizers and checks |
|---|---|---|---|---|---|
| `r5-integrate-mission-a`, `-b` (`tools/oracle/mission_test.sh --ticks 6000 --perturb 3000 --copy-tick 4000`) | mission, 6,000 ticks, debug | **0** / 23.9 s and 23.1 s | **oracle mission run done** (`ticks=6000 records=6003 ships=154`) | none (rc 0) | **ASan 0** in both, no unsuppressed leak; 3,766 file loads each; `cmp` equal, **1,476,370 B, sha256 `645f94c69d68319e`**; `hwo_compare.py`: `IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000)`. Against the hud audit's `r5h-aud2-m-a` (built from a47a8a6 plus the hud port and its fixes) and the command-tactics audit's `r5c-aud-mission-a`: IDENTICAL over all 6,001 `T` records (only the header's build identity differs). Against R4's `r4-integrate-mission-a` and the layout-dock `r5ld-final-mission-a`: `FIRST DIVERGENT TICK 0` in `ships_pos` and `univcheck`: the command-tactics port moved the baseline (its audit's observation; "Decisions" 30) |
| `r5-integrate-mission-perturb` | mission, 6,000, `--perturb 3000` | 0 / 23 s | oracle mission run done | none | ASan 0; `FIRST DIVERGENT TICK 3000`, 2 fields, likely origin `rng10`, `CAND is 1 draw(s) ahead`; events `game-new, perturb, quit` |
| copy of A with bit 0 of `ships_health` flipped at tick 4000 | | | | | `FIRST DIVERGENT TICK 4000`, 1 field, origin `ships_health`. **MISSION TEST PASS** |
| `r5-integrate-mission-rel` | mission, 6,000 ticks, **release** | 0 / 4.8 s | oracle mission run done | none | vs run A: `IDENTICAL: 6001 ticks` (header build flags 1fbfa32f vs 1afeb7ac): the -O2 build gives the same stream as the ASan build |
| `r5-integrate-sp` | sp, `--timeout 340` | 124 / 340.5 s (timeout) | **universe running**: last `debugScreenshots univUpdateCounter` line **1,248** (R3, R4 and every shared-machine run this round: 1,248; the animatic plays in real time first) | none: still running at the timeout. 3,767 file loads (as the hud audit's `r5h-aud-sp`); 33 Mission01 statics; 21 engine glows | **ASan 0**; no unsuppressed leak. UBSan: only the known non-fatal misaligned-member kinds (`fescreen` 101 + 7 + 2, `etgdecision` 21, `etgfunctioncall` 10, `etgconditional` 8, `etgbranch` 8, `BTG.c:388` 2); the `PATCH` (17 + 15) and `fquant.c` kinds of R4 are gone |
| `r5-integrate-fe` | fe, 120 s | 124 / 120.4 s (timeout) | **`Main_game_screen`** | none: still running at the timeout | ASan 0; no unsuppressed leak; 284 file loads (as the hud audit's `r5h-aud-fe`: R4's 232 plus the 48 overlay icons, `OverlayIcons.script`, `MainOverlay.script`, `shipnames.script` and `GunSpecEvents.lut`); UBSan: only `fescreen` (92) |
| `r5-integrate-statics` | statics | **0** / 12.4 s | ship statics self-test done | none | ASan 0, no leak. **143 of 145** ship types loaded, 2 not found (the disabled cloak-generator `.shp`), 27 other `.shp` (170 of 173); 2,995 file loads |
| `r5-integrate-statics-init` | statics + `/catShipStaticsStaticInit` | **0** / 11.7 s | ship statics self-test done | none | ASan 0, no leak. 143 of 145; **108 StaticInits ran** (`staticinit=next`, as R4) |
| `r5-integrate-meshes` | meshes | **0** / 11.1 s | mesh self-test done | none | ASan 0, no leak. 1,758 listed, 1,729 `.geo/.peo` + 29 `.mad` loaded, 0 with bad objects |
| `r5-integrate-textures` | textures | **0** / 18.8 s | texture self-test done | none | ASan 0, no leak. 12,403 / 12,403 listing lookups, 10,661 registered in 3 batches and loaded, 0 not loaded, 1,742 rows with no file, 6,289 scaled down |
| `r5-integrate-oracle` | oracle (debug), 512 ticks | **0** / 9.9 s | oracle self-test done | none | ASan 0, no leak. `randSyncErr=0 univSyncErr=0 blobSyncErr=0`, 772 records, 140,723 B |
| `r5-integrate-etg` | etg, 128 ticks | **0** / 11.2 s | etg effect self-test done | none | ASan 0, no leak. 9 of 9 scripts parsed, 197 effects loaded, 0 referenced but not loaded; 618 created, 3,197 deleted while running, 1,093 left, peak 2,515 effects / 3,012 particle blocks (as R3 and R4) |
| `tools/oracle/selftest.sh --label r5-integrate-oracle-rel` | oracle (release), 512 ticks | 0 each | oracle self-test done | none | runs a and b `cmp` equal (140,723 B, sha256 `cfe931ebee1e5344`) and `IDENTICAL: 513 ticks and 256 packets`; the perturbed run reported at tick 300 / `rng10` / 1 draw ahead (and packet 150's `randcheck`); the flipped copy at tick 400 / `ships_pos`; `rngs=27` in every `T` record. **`SELFTEST PASS`** |
| `r5-integrate-dock` | dock, 5,000 ticks | **0** / 22.1 s | **dock self-test done** | none | ASan 0, no leak. `orders tick=16 fighters=6 corvettes=2 workers=3 RUs=750`; `clDock purpose 0: 8 of 8 fighters and corvettes docking`; `purpose 3: 1 worker(s) depositing 250 RUs`; `A inside=6 of 6 fighters; RUs 750 -> 1000`; `done fighters=6 outside=6 corvettes=2 RUs=1000 result=PASS`, 0 FAIL lines; 3,766 file loads |
| `r5-integrate-command` | command, 3,000 ticks | **0** / 20.2 s | **command self-test done** | none | ASan 0, no leak. `link ordered: 2 acolytes, 2 mimics; point ordered at tick 18; link done at tick 258: sACV#178; unlink done at tick 291; done tick=2998 link=258 mcv=1 unlink=291 research=1/1 mimic=1 point=1/1 result=PASS`, 0 FAIL lines (the audit's numbers exactly); 4,044 file loads |
| `r5-integrate-render` | render, 600 frames | **0** / 16.0 s | **render self-test done** | none | ASan 0, no leak. `frames=583 listed=1148 ships=1148 trails=10906; lensflares=581 lensflareElements=9296 foglayers=1166; result=PASS` (the producer's and the audit's counts) |
| `r5-integrate-sound` | sound | **0** / 17.7 s | **sound self-test done** | none | ASan 0, no leak. `audio driver dummy`; `Music4Cat.wxd: 62 streams, track 0 (bitrate 512, 2 channels) decoded 43 blocks; SectSpeech.vce: event 0 decoded 124 blocks; BeastSpeech.vce: event 26 decoded 281 blocks; chatter: 88 events, 88 named, 84 with an id (4 have 0, as in the exe), 64 with a Somtaaw sentence, 57 with a Beast sentence; 403 audible output blocks` (into the dummy device); `result=PASS`; **UBSan reports in `fquant.c` / `fqcodec.c`: 0; `struct PATCH` reports: 0**; UBSan kinds: only `fescreen` (92) |
| `r5-integrate-hud` | hud, 600 frames | **0** / 16.6 s | **hud self-test done** | none | ASan 0, no leak. `selected tick=64 band-boxed=19 player ships=154 selected=154 selecting=154 piemode=2; counts frames=535 statusbars=10165 bars=10165 icons=332 infopanels=535 infolines=9095; selectedlines=8025 selectinglines=8025 messagelines=202 supportunits=535 pie=535 compass=535; result=PASS` (the audit's numbers) |

So every row of the R5 exit test holds:

| condition (docs/m2-r4-summary.md "R5 integrate (exit)") | result |
|---|---|
| two 6,000-tick Mission01 streams byte-identical with ASan 0, the perturbed run and the flipped copy caught, also in release | **met**: `r5-integrate-mission-a` = `-b` byte for byte, ASan 0 in a, b and the perturbed run; perturb at 3000 / `rng10`, flip at 4000 / `ships_health`; the release build gives the same 6,001 `T` records |
| sp universe running (`--timeout 340`, `univUpdateCounter` at least 1,248, ASan 0) | **met**: 1,248 ticks at the 340 s timeout on a shared machine (load average about 19), ASan 0, no leak |
| fe `Main_game_screen` | **met**, ASan 0 |
| statics, meshes, textures, oracle, etg unchanged or better | **met**: 143/145 (108 StaticInits), 1,758/1,758, 10,661 / 0 not loaded, 0 sync errors, 618 effects: R4's numbers |
| release oracle self-test `SELFTEST PASS` | **met**, 27 RNG streams |
| quitting runs return rc 0 | **met**: 16 of 16 quitting runs (the three debug mission runs, the release mission run, statics, statics-init, meshes, textures, oracle, etg, dock, command, render, sound, hud, and the three release self-test runs) |
| `tools/m2_layout32.sh` OK with the `Ship` 0x63C pin; clean builds 0 warnings | **met** |
| the new self-tests dock, command, render, sound and hud complete with ASan 0; sound with 0 UBSan reports in `fquant.c` / `fqcodec.c` | **met**: all five `result=PASS`, rc 0, ASan 0; sound 0 / 0 |
| `grep -rn "M2-R4" src/` finds no `CAT-TODO` / `CAT-PROVISIONAL` naming an M2-R4 owner | **met** after this step's nine edits: `grep -rn -E 'CAT-(TODO\|PROVISIONAL)\([^)]*M2-R[45]' src/` is empty; the remaining `M2-R4` mentions are attributions (`CAT (M2-R4 ...)`) or "gone with the HW1-only types (M2-R4 ships-b)" inside `CAT-TODO(stage 4 AI)` markers |

**UBSan kinds left** (all non-fatal misaligned-member accesses of GoK's packed 64-bit structs; none in the sound,
fe or release runs beyond `fescreen`): `fescreen` (`FEFlow.c` 92-101 per run, `TaskBar.c` 7, `Sensors.c` 2 in sp;
GoK's packed front-end screens: M2-R6 producer 2); the ETG bytecode kinds in sp, etg, dock, command, render and hud
(`etgdecision` 21-25, `etgfunctioncall` 10-16, `etgconditional` 8-32, `etgbranch` 8-10, `etgvariablecopy` 4,
`particle` 166 + 18 in the etg self-test: GoK's packed effect opcodes, stage-2 files, not owned by a stage-3
producer); and `BTG.c:388` (2 misaligned `short` loads and 1 `udword` load of the packed `.btg` file data in every
Mission01 run; present since the base). Gone since R4: the `struct PATCH` kinds (17 + 12..15 per run) and the
`fquant.c` kinds (M2-R5 sound).

Harness facts for the next rounds, in addition to R3's and R4's:
- **The sp counter follows the wall clock and the machine's load**: this step's run shared a loaded machine (load
  average about 19 on 24 cores from other work) and the producers' / auditors' runs reached 1,248 (shared) to 1,360
  and R4's solo run 2,256; the mission mode is unaffected (one universe tick per frame on the virtual clock).
- **The mission stream baseline moved once this round** (command-tactics, from tick 0): compare a stream with the
  nearest audited baseline (`hwo_compare.py`, not sha256), and record the stream hash before and after any port that
  touches ship creation, formations or the command layer. M2-R6's repair-droid port (random stream 0) will move it
  again.
- The five new modes (`dock`, `command`, `render`, `sound`, `hud`) and their `--*-ticks` / `--*-frames` options are
  documented in the script header; `sound` needs no ticks (about 18 s), the others ride on the mission mode.

## The five ports

| # | item | what landed | DONE WHEN | audit | commit |
|---|---|---|---|---|---|
| 1 | layout pass, dock purposes, experience, flight/math compares ([report](m2-r5-layout-dock.md), [audit](audits/m2-r5-layout-dock.md)) | **Layout:** the exe's object layouts in `SpaceObj.h`: `SpaceObjHeader` 0x20 with one `catFlags18`, `SpaceObj` 0x98 with the attribute **dword at +0x38** and `rotinfo` at +0x98, `NonStats` with `catMaxVelocity`, `ShipOrDerelict` 0x238 (`catRetreating` 0x1D0, `navLightInfo` 0x234), **`Ship` 0x63C with `health` at 0x170** and every CET offset that the exe confirms (63 of 66; `tractorPosinfo` 0x408, the 0x650 fields and the 0x170 name are CET's errors), `catDamageFactor` 0x3E0, `catExperienceLevel` 0x5F8, `dockvars` 0x494 / `dockInfo` 0x4A8; `Missile` 0x258, `Bullet` 0x164, `Derelict` 0x294, `Asteroid` 0x1B4; the slave trade deleted. **Dock:** `Dock.c` rewritten on the purposes 0..10 (`cat_dockGetDockClass`, `ThisShipCanDockWith`'s 4th argument, the 17-state cone machine `cat_dockShipDocksAtShip`, the point / launch / cone helpers, the REWRITTEN dock-point functions, `clDock`, `processDockToDo`, `cat_dockFinishedDocking` with the confirmed refund, `dockPutShipOutside`'s module rotations, `clLaunchShip` / `processLaunchShipToDo`, `cat_clHarvestNearestResource`, `cat_lmShipAutoLaunchOn`); 95 HW1-only dock functions and the 60 dock tweakables deleted; every caller converted to purposes; the hive / slipgate / Leech / Worker / Processor stand-ins gone. **Experience:** `cat_shipUpdateExperienceLevel` with the exe's weights and thresholds, 7 upgrade query helpers, the end of an upgrade in the deferred update. **Compares:** the 97 IDENTICAL bodies of `FlightMan.c` and the math files (2 found modified: `volFindCenter`, `teReset`; `fsqrt` is HW1's table in the exe, "Decisions" 23). `/catDockTest` + harness mode `dock`. 8 CAT-FIXes | met: layout32 holds `Ship` 0x63C / +0x38 dword; dock self-test PASS ASan 0; no HW1 dock path or stand-in; mission streams identical. **Partly unmet** (audit F8): the self-test's host is the sMothership (no sCarrier / sProcessor leg) and the two ACVs and the harvesting Worker are logged, not judged: M2-R6 producer 4 | PASS-WITH-FIXES, **0.895** (77/86). Fixes: `cat_dockGetDockClass`'s unjustified CAT-FIX (the exe falls through), the `CustShipLaunchCommand` arity, 7 unreviewed edited IDENTICALs (3 now MODIFIED), the review file synced with the map, 4 missing CAT-FIX markers, 6 missing CAT comments, two doc counts | `6f64a21` |
| 2 | command layer follow-up, force-field formations, tactics ([report](m2-r5-command-tactics.md), [audit](audits/m2-r5-command-tactics.md)) | **Layout:** `CommandToDo` 0x330 with the waypoint `MoveCommand` (21 points, path types), `FormationCommand` +0x288, `Ship.formationcommand` retired. **Orders 11-16:** `cat_clLink` / `cat_clUnlink` / `cat_clMimic` / `cat_clResearch` / `cat_clPointOrder` / `cat_clUpgradeShips` with their `cat_process*ToDo`, the link flight, 10 `cat_clWrap*` wraps (packets: stage 5), the Beast-system shut-off and the mimic self-unlink. **The remaining `CommandLayer.c` bodies:** the move / hyperspace / attack / passive-attack families, `FreeCommandToDoContents` / `RemoveShipsFromDoingStuff` with the exe's reason codes, `clSpecial`, `clHalt*`, `clRUTransfer`, the parade and holding pattern, `clProcess` / `clPostProcess`, `GroupShip`. **Formations:** `clFormation`'s 4th argument, the force-field wall (25 places) and sphere with the Sentinel links and field levels, the leader's collision centre as the formation centre (found by the self-test). **Tactics:** the exe's `TacticsInfo` 0x9C4 and 198-row table, 12 retaliation functions, `univCheckShipState` / `getEnemiesWithinProximity`, `cat_univLimitProtectedShipSpeed`, `cat_beastGetAttackStaticInfo`. 55 IDENTICAL compared, 13 + 1 found modified. `/catCommandTest` + mode `command` | met: every MODIFIED body of the four files ported or owned outside stage 2; command self-test PASS ASan 0 (link 258, unlink 291, point 18/19, research 1/1, mimic, wall 4/4, sphere 6/6 clamped); sp as before; mission streams identical (the baseline moved: "Decisions" 30) | PASS-WITH-FIXES, **0.943** (50/53). Fixes: 3 unreviewed edited IDENTICALs, 10 call sites without CAT comments, one wrong retaliation comment | `84a9d05` |
| 3 | render/GL ([report](m2-r5-render.md), [audit](audits/m2-r5-render.md)) | **The object list:** `cat_rndRenderObjectList` 0x005776f0 with the render state block (fade, dissolve, mimic, Beast stun, timed effects, EMP flicker), the deferred depth-sorted pass, `rndMainViewRenderFunction` as the Cataclysm main view, `meshSetFade`'s 8 channels, the render-time LOD and its timing, the edge-effect and warped mesh renderers, the material switch. **Lens flares and fog:** the `.script` callbacks, source visibility and renderer, the NIS source, the fog layer (script, fade, update, render), BTG's per-channel multiplier and background flashes. **Ship renderers:** Sentinel field segments (79 on GLtype), Dreadnought charge, Nomad beam, blast lightning, the repair-droid proxies' renderer, `cat_meshLineIntersect` with the ramming latch / cutting beam and the collision mesh-hit tests, the render limits as distances, the damage points compared. **GL files:** every unread function of the 21 render files compared (22 ported, the GoK-backend ones recorded). `/catRenderTest` + mode `render` | met: render self-test PASS (583 frames, 1,148 objects, 581 lens flares, 1,166 fog layers) ASan 0; the render-test stream equals the mission stream (render-side randoms stay on their streams); mission streams IDENTICAL to base; fe and sp unchanged | PASS-WITH-FIXES, **0.944** (51/54). Fixes: the Sentinel segment cap (34 -> 79 on GLtype), one missing review entry, the swapped lens-flare shape comments | `af90827` |
| 4 | sound and speech ([report](m2-r5-sound.md), [audit](audits/m2-r5-sound.md)) | **The fq codec at the exe:** `fquant.c`'s byte counters, band / index caps, unaligned loads through `memcpy`, unsigned shifts (both GoK UBSan kinds gone), `fqWriteTBlock` without GoK's limiter and with Relic's rint, `dct.c` / `fqeffect.c` at the exe's precision. **Mixer:** the exe's `isoundmixerprocess` (no effect / delay / acoustic stages), the 64-bit bank's patch table at an aligned +0x10 (every `struct PATCH` report gone), rint-rounded volumes. **Sound events:** the 92-row LUT layout, `GunSpecEvents.lut`, Cataclysm's `soundEventUpdate` / `soundEventPlay`, the mesh-animation sound table, `cat_soundEventBlast`, `cat_soundEventAsteroidBreak`, `cat_soundLevelReset`. **Speech:** 3 speech queues + 1 music stream, the exe's id layout (index 0x7ff, actors 0x3ff800) with HW1-named ids muted by a marker bit, the per-race voice, the stealth alerts, the Easter eggs, the letterbox, the 62-track music state machine. **Battle chatter:** raw weights, squared distances, the status-event spacing, `battlePingEvaluate` every 8 s, the 88-row table verified. 13 sound / speech CAT-TODOs of R3 / R4 are calls. `/catSoundTest` + mode `sound` | met: sound self-test PASS into the dummy device, ASan 0, 0 UBSan in `fquant.c` / `fqcodec.c`, 0 PATCH reports in any run; fe and sp unchanged minus the removed reports; mission streams IDENTICAL to base | PASS-WITH-FIXES, **0.85** (39/46). Fixes: the clean-build break (kas2c cannot parse `\|` in the new HW1 speech ids: `+`), the letterbox and chatter-distance precision (double where the exe uses the x87 stack), 4 unreviewed edited IDENTICALs, 4 stripped IDENTICAL asserts, the review counts, one missing comment | `a47a8a6` |
| 5 | HUD ([report](m2-r5-hud.md), [audit](audits/m2-r5-hud.md)) | **New files** `OverlayIcons.c` (53 icons), `ShipPrint.c` (the ship info panel: 15 `cat_shipPrint*`), `ShipNames.c` (`shipnames.script`, 100 names). **Status bars and picking:** `cat_selStatus*` / `cat_selHealth*` with `selStatusDraw0-4`, the picking API at the exe bodies (`selRectDragFunction`'s centre point, `selSelectionClick`'s pickNothing, the FOW helpers without a grid). **Overlays:** `InfoOverlay.c` rewritten (`ioStartup`, `ioUpdateShipTotals`, the `cat_mo*` main overlay, `cat_ioClick`, hover, objectives, the message scroller `cat_ioMessageAdd` / `clCommandMessage(text, colour)` fed by 9 callers, the mission timer), `Tactical.c`. **Pie plate:** `piePointModeOnOff(mode, callback)`, the compass, the box clip, 4 IDENTICAL pie functions found modified. **Ship view** rewritten from the disassembly; `TaskBar.c`, `GamePick.c` (the escape menu, the game-stats start-up), `HorseRace.c` (the Cataclysm loading screen). 40 IDENTICAL compared, 5 found modified. `/catHudTest` + mode `hud` | met: hud self-test PASS ASan 0 (19 band-boxed, 154 selected, 10,165 bars, 535 panels / pie plates / compasses); the 25 UI cat-new of the R4 tool move defined; fe and sp unchanged but for 51 start-up loads and 10 fewer `ring.lif` / `arrows.lif` loads; mission streams IDENTICAL to base. **Deferred:** the game-stats screen (`cat_gameStats*` x8, about 5 KB, needs the popup menu's frame and shaded rectangle): M2-R6 producer 1 | PASS-WITH-FIXES, **0.913** (42/46). Fixes: the compass "none" colour (0xff000000), the bar split precision, one unreviewed edited IDENTICAL, two stale CAT-TODOs | `02e8e4d` |

**Round size** (`git diff 1434b90`, the R5 launch to this step's tree): 188 tracked files changed, +34,852 / -21,031
lines. `src/` alone: 170 files, +24,521 / -16,852, with **2,476 added lines carrying `CAT`**; 6 new files
(`Game/OverlayIcons.c/.h`, `Game/ShipNames.c/.h`, `Game/ShipPrint.c/.h`), 0 deleted (`Dock.c` lost 2,149 lines of
HW1-only code in place). The tree now carries 2,771 `CAT:` lines, 188 `CAT-TODO` (R4: 167), 22 `CAT-PROVISIONAL`
(37), 15 `CAT-DORMANT`, 40 `CAT-FIX` (7), 18 `HWI-ORACLE`. Tools: `tools/m2_harness.sh` (the five modes),
`tools/m2_layout32.sh` (the object, command, tactics and sound layouts), `tools/oracle/record_reference.sh` (the
user's one-command record / verify of the reference session, commit 1ad47c6). `runs.txt` logs 181 harness runs this
round (lines 482-662), 20 of them this step's.

## Delta map after the round

The ports recorded their comparisons as reviews in `ghidra/delta_map_review.json`, merged with `tools/delta_map.py
merge` into `ghidra/delta_map.json` (the layout-dock producer wrote into the map directly; its audit synced the review
file, F7). Notes tagged `[review: M2-R5 <port>]`: layout-dock 125, command-tactics 103, render 209, sound 84, hud 127
entries.

| class | M2-R4 end | M2-R5 end | M2-R5 bytes |
|---|---:|---:|---:|
| IDENTICAL | 3,065 | **3,009** | 567,646 |
| MODIFIED | 1,278 | **1,334** | 661,489 |
| REWRITTEN | 77 | **77** | 9,712 |
| hand-reviewed | 1,615 | **2,162** | (MODIFIED 769 of 1,334, REWRITTEN 34 of 77, IDENTICAL 1,359 of 3,009) |

Per stage (`tools/m2_stage_counts.py --md` on the map after the hud audit): HW1-matched IDENTICAL / MODIFIED /
REWRITTEN functions (MODIFIED bytes), then cat-new by the tool's rules.

| stage | M2-R4 end | M2-R5 end | cat-new |
|---|---|---|---|
| 1 | 215 / 100 / 0 (52,943 B) | 211 / 104 / 0 (54,697 B) | 76 / 27,383 B |
| **2** | 783 / 589 / 30 (245,967 B) | **765 / 607 / 30 (253,494 B)** | 808 / 180,868 B |
| **3** | 928 / 263 / 7 (175,469 B) | **896 / 295 / 7 (187,650 B)** | 450 / 139,196 B |
| 4 | 387 / 90 / 2 (50,387 B) | 386 / 91 / 2 (50,741 B) | 23 / 14,404 B |
| 5 | 406 / 114 / 12 (74,848 B) | 406 / 114 / 12 (74,848 B) | 67 / 17,483 B |
| 6 | 346 / 122 / 26 (39,912 B) | 345 / 123 / 26 (40,059 B) | 2,365 / 656,918 B |

What moved: 56 functions the map called IDENTICAL were found modified by the ports and their audits (layout-dock 2 +
3 in its audit, command-tactics 13 + 1, render 6 net of the GL files, sound 24 + 2 in its audit, hud 5); stage 1's
four are the mesh / LOD render functions (`meshBindingFindByName`, `meshSpecColour`, `lodTableReadScript`,
`meshAlmostCurrentMaterial`), stage 4's one is `aimProcessDock`, stage 6's `tutHideText`. No class became REWRITTEN.
The hud port moved no function between stages (the 21 UI names of the R4 tool move are now defined). The R2
estimate stands: `docs/m2-r1-function-deltas.md` was regenerated by each port's merge.

**Cat-new in the tree** (this step's count: a name is "defined" when a line of `src/` starts its definition;
"mentioned" when it appears only in a comment or a `CAT-TODO`; R4 counted names present in `src/`): stage 2: **650
of 808 defined** (132,791 of 180,868 B), 50 mentioned only (20,234 B), 108 absent (27,843 B); stage 3: **223 of 450
defined** (78,918 B), 44 mentioned only (7,138 B), 183 absent (53,140 B); stage 1: 63 of 76. Of the 158 stage-2 names
not defined (48,077 B), nearly all are UI, console, key-binding, FOW, popup-menu, motion-cue and repair-droid code
that the address-neighbour rule places next to stage-2 files (owned by the M2-R6 producers below), or names the
tree holds under another one (`cat_partRenderUnk1System` = `cat_partRenderEBoltSystem`, the four dissolve helpers =
`Dissolve.c`'s statics, `cat_dmgShipLoadDamagePoints` / `Free` = `dmgGetLights` / `dmgClearLights`,
`cat_hsGateRectangle` inlined), or GoK-backend paths not ported on purpose (`cat_trans*`, `cat_kniTrans*`,
`cat_meshObjectRenderLitTransformed` / `Immediate`), or code no game path reaches (the three variant StaticInits,
the developer fight statistics `cat_stats*`). The stage-2 leftovers proper are the repair-droid runtime (6
functions, 4,807 B), `cat_shipUpdateExternalBuildAndAI` 2,088 B with `cat_mothershipModuleSlotSet` and
`cat_cmBuildJobFinished` (ConsMgr-driven), the save / load helpers (`cat_saveShipCounters` / `Load`,
`cat_saveGameMiscChunks` / `Load`: the user's saves) and `cat_DeclarePlayerWinReportResults`.

## What is left of stage 2

**Nothing blocks the stage-2 exit test** (met above) and every port's DONE WHEN is met or carried with an owner:

1. **The two provisional switches:** `CAT_HW1_MISSION_SCRIPTS 0` (stage 6) and `CAT_EXTERNAL_BUILD_DRIVER 0`
   (`cmDeterministicBuildProcess` 0x004592a0 / `Cancel` 0x00459040, `cat_shipUpdateExternalBuildAndAI`,
   `cat_cmExternalBuildShipDied`, `cat_cmExternalBuildAttach`, `cat_suShipAdded` / `Removed`: M2-R6 producer 4).
2. **The dock self-test's coverage** (layout-dock audit F8): the host is the sMothership (no sCarrier / sProcessor leg;
   the depositor picks the mothership at an equal distance) and the two ACVs and the harvesting Worker are logged, not
   judged. One ACV orbits its dock point for minutes before latching (the corvette flight model along the cone,
   `AIShip.c`: stage 4; the dock code matches the exe); the Worker never reaches the paused end of its jack animation
   (`madAnimationStart`'s third argument: M2-R6 producer 5). Owner of the extension: M2-R6 producer 4 (with
   `/catBuildTest`).
3. **The repair-droid runtime** (`droidCreateArray` 0x005c3070, `droidUpdateArray` 0x005c32f0, `cat_droidFreeArray`,
   `cat_droidWalkTowardPoint`, `cat_droidRandomWalkVertex`, `droidIsValidPredockPos`; `Ship+0x4C0` NULL until then)
   and **`madAnimationStart`'s third argument** (the exe's madanim layout, the pause-instead-of-stop at the end, the
   previous-animation word, the per-type animation sounds through `cat_madAnimSoundEvent`): M2-R6 producer 5. Both
   are simulation-side (stream 0 draws; gun bindings swap on the simulation side) and move the mission stream.
4. **`univCreateShip`'s three leftovers** (this step's header): the droid array (producer 5), `cat_suShipAdded` with the
   external-build start and the factory block (producer 4), the FOW cube (producer 5).
5. **`aishipFlyToPointAvoidingObjsFunc` 0x00413e40** still differs in two places (the command-tactics report): the
   small-target pass is skipped for a ship whose static +0x48 is below 500 (0x00414c8a), and the Ship+0x608 /
   single-player mission-type exceptions (0x00414e0e). `AIShip.c` is a stage-4 file: stage 4 (the flight re-review),
   with the corvette-cone item of 2.
6. **`levelPreInit` 0x004a3230**: its fog-layer / lens-flare part landed in render and its sound part in sound
   (`cat_soundLevelReset`); the Beast-convert type marking through `cat_beastGetAttackStaticInfo` is left: stage 6
   level load (with `kasfShipsBeastify`).
7. **The asteroid box clamp** of `univUpdateAllPosVelResources` 0x005432d0 (this step's re-target): stage 6 level load /
   KAS, with the writer of the three reserved `Asteroid` dwords.
8. **The beam's sixth argument**: `collCheckBeamCollWithTargetsInBlob` 0x0043c570 takes a Sentinel-field triangle
   out-parameter the tree's five-argument signature lacks (render's leftover): M2-R6 producer 5 (with the field
   segments).
9. **Stage 5 and the saves:** the network packets of orders 11-17 and the receive side of `clSendMisc` 5-10
   (`CommandWrap.c` CAT-TODOs), the five dock words in the sync checksum, `sendChatMessage`'s fourth argument;
   `universe.catSelection1A0` is not saved (the exe's `SaveUniverse` 0x005034c0 saves it after `HousekeepShipList`),
   `cat_saveShipCounters` / `cat_saveGameMiscChunks`, `SaveGame.c`'s retreat atoms: the user's two saves.
10. **Readings kept as they are:** `COMMAND_CAT_13` / `COMMAND_CAT_17` have no setter in the exe (left defined, unused);
    `cat_clUnlink` may create a command with no ships when the unlinked pair is not in a command (what the exe does;
    not a CAT-FIX); `catRetreating`'s meaning (set by the retreat chatter, cleared by every new order) and the reserved
    words `catObjReserved88[4]`, `catShipReserved5B0`, `catMissileReserved248[3]`, `catNavLightStatic` (no exe reader
    found) are medium confidence.
11. **The `fsqrt` question** (the user's, "Decisions" 23).

## What is left of stage 3 part A

Each report's "What is left" with its owner, all inside the M2-R6 plan below unless marked:
- **render:** the repair droids and the mesh animation (producer 5, items 3 above); the motion cues `cat_mcRender`
  (producer 5); `pingListDraw`'s FOW test (producer 5); `rndRenderTask`'s front-end / mouse parts (producers 1 and
  2); `ssSaveScreenshot`'s message (producer 3); the HW1 BTG dimming of the sensors manager (producer 4);
  `cat_primRectSolidTexturedFullRectBlendC2` 456 B and `cat_primLine3Smooth` 215 B (two prim draws no ported caller
  needs yet: producer 1 with the popup menu); GoK's backend kept on purpose (glcompat, the RGL / Glide device
  list, the Transformer, the WGL set-up, `rndLoadTarga`'s GL_CLAMP_TO_EDGE: "Decisions" 28).
- **sound:** the Win32 device layer (DirectSound / waveOut mixer thread, `cat_smixPrepareWaveOut`,
  `cat_isoundmixerStartThread`, `cat_glide*`: not applicable, SDL replaces it; reviewed MODIFIED low); the letterbox
  callers (`kasfWideScreenIn` / `Out`, `cat_kasWideScreenInHyperspace`, `kasfWideScreenInDontCloseSensors`,
  `singlePlayerGameUpdate`: stage 6, with the KAS bodies); `madAnimationUpdate`'s call of `cat_madAnimSoundEvent`
  (producer 5); the Bink term of `speechQueueUpdate` and the Options voice preview (producer 2); the music next /
  previous keys (producer 1); `battleLaunchWelcomeExchange`'s caller `cat_cmBuildJobFinished` (producer 4); the
  about 90 HW1-named speech call sites muted by `HW1_SPEECH_ID` until each port passes the exe's id (their owners:
  AI, KAS, tutorial, HUD; "Decisions" 26); the uncompared IDENTICAL bodies of `Subtitle.c` (about 16), `sstream.c` (4),
  `soundlow.c` (8) and `smixer.c` (2) (the next sound reader: producer 2 takes `Subtitle.c`, the rest an audit of
  opportunity).
- **hud:** the game-stats screen (producer 1: `cat_gameStats*` x8, `cat_popupMenuDrawFrame`,
  `cat_primRectShadedHorizontal2`, the entry `cat_utyGameQuitToMainOrStats` in producer 3); the debug-key overlay
  `cat_moDebugKeyLineDraw` / `KeyGet` / `KeyValue` and the `CAT_KB_*` stand-ins (producer 3); the FOW grid for picking
  (producer 5); the right-click popup menu and `cat_popupMenuRemoveShip` (producer 1); the rest of `subTitlesDraw`
  (producer 2); CM_Research / CM_Launch (producer 4); the NIS loading-picture flag 0x00a6adac (stage 6);
  `sendChatMessage`'s fourth argument (stage 5); the 0.325 pixel-centre offset ("Decisions" 29).
- **layout-dock, command-tactics:** the console's `experienceships` command 0x0054d1c0 and the 0x00a751c0 re-apply
  toggle, `tacticsPopUpSetUp` and the main-region tactics keys (`cat_mrSetTactics` 0x00565c30, `cat_mrTacticsCycle`
  0x00566860), the UI callers of the wraps (`cat_ui*`, `cat_mrMimicTarget`, `cat_popupMenuMimicSelect`,
  `mrObjectClick`, `mrDockingOrders`'s slipgate order): producers 1 and 3; the AI callers (`cat_ais*`,
  `aiuFindCaptureTarget`, `aiuWrapAttack` / `Special` on `cat_shipIsTargetableBy`): stage 4.

## Plan for M2-R6: stage 3 part B (main region and popup menu, front end, start-up, managers, NIS / FOW / camera)

Stage 3 at the end of R5 is 896 / 295 / 7 HW1-matched functions (187,650 MODIFIED bytes), of which the five R6
producers' files hold 664 / 205 / 4 with **155 MODIFIED / REWRITTEN bodies still unread (89,073 B)** and 1,650
IDENTICAL bodies to compare, plus about 330 cat-new functions not yet in the tree (about 103 KB) once the hud, render
and sound files are taken out. That is about 190 KB of exe code to read against the tree, the size of R5's part A
(which one round of 5 producers + 5 auditors finished with one deferral), so M2-R6 is **one round of 5 producers + 5
auditors + integrate** (11 agents), in sequence as before. The porting conventions, the harness and the session rules
stay (every producer compares each IDENTICAL body in its scope, runs the engine only through `tools/m2_harness.sh`,
retires the HW1 aliases of the files it ports and replaces the `CAT-TODO`s that name it or re-targets them with a
reason), with two additions from this round: a producer that changes the mission stream (producers 4 and 5) records
the stream hash before and after, states which oracle fields moved, and its audit confirms the pre-change stream
IDENTICAL to R5's `645f94c69d68319e` records; and a `CAT-TODO` is written on one line, owner first
(`CAT-TODO(<owner>): ...`), so the owner greps work.

Counts (this step's scratch grouping of `ghidra/delta_map.json` by HW1 file and of the r6 cat-new by name prefix,
else by the file of the nearest HW1-matched function; "unread" = MODIFIED / REWRITTEN without a review; "not in
tree" = no definition in `src/`; the owners should rerun `tools/m2_stage_counts.py` and the grouping on the map they
start from):

| # | producer | scope | I / M / R fn (M+R bytes); unread M/R | cat-new not in tree | done when |
|---|---|---|---|---|---|
| 1 | main region, mouse, the popup menu, the game-stats screen | `mainrgn.c` (11 unread: `mrStartup`, `mrObjectClick`, `mrDockingOrders` = the slipgate order, `mrSelectHold`, `mrRetire`, `mrScuttle`, `mrFormationShips`, `toMoveLineDraw`, ...; the band box `cat_mrBandBoxRelease`, `cat_mrReleaseRight` / `Left`, `cat_mrSelectRectDraw`, the tactics and formation keys), `mouse.c` (`cat_mouseGetContextAction` 1,052 B, the cursor save / restore), `PlugScreen.c`; the right-click popup menu (`cat_popupMenu*`, 31 functions, about 19 KB: items, submenus, layout, draw, click, hotkey scan, the mimic and Leech submenus); the `cat_ui*` special commands (link, unlink, mimic, unmimic, leech, EMP, swarm, ram, siege cannon, cloak, halt, the protect formation, `cat_uiSpecialTargetCommand` 1,320 B, `cat_uiFindNearestSpecialTarget`); `cat_selComputeTargetActions` 1,336 B and `cat_selShipScreenRectDraw`; the mimic menu (`cat_mimicInitDisguiseTables` and the item functions) and `cat_bigGunFireCommand` / `IsCharged`; **the game-stats screen** (`cat_gameStats*` x8 with `cat_popupMenuDrawFrame`, `cat_primRectShadedHorizontal2`); the two prim draws; the music keys | 36 / 23 / 1 (19,329 B); 15 unread (11,355 B) | 90 / 38,322 B | a scripted `/catUiTest` (harness mode `ui`) opens the popup menu for a Mission01 selection, runs each `cat_ui*` command once through the wraps, closes it, and shows the game-stats screen once; the exe's context-action table (`cat_mouseGetContextAction`) reproduced; ASan 0; sp, fe and the mission stream unchanged |
| 2 | front-end flow, screens, controls, fonts, strings, options, subtitles | `FEFlow.c` (3 unread), `FEReg.c`, `UIControls.c` (3), `font.c`, `FontReg.c`, `ScenPick.c` (6), `ColPick.c`, `Options.c` (19 unread: `opOptionsInit`, `opUpdateSettings`, the accept / cancel / save paths, `opRender`; the Cataclysm toggle set `cat_op*`, `cat_opSetAllDefaults` 884 B, `cat_opHomeworldClassic` / `cat_opCataclysmDefault`, `OP_ExperienceMessages`, texture detail with `cat_trTextureMemorySet` / `cat_trPrecacheShipTextures`), `Demo.c` (2), `Titan.c`, `bink.c`, `StringSupport.c` (`strLoadLanguage`, `strSetStringCB`: the 1,705-string readers, `ShipTypeToNiceStr` and the UI strings resolve to the install's strings), `Subtitle.c` (the rest of `subTitlesDraw`, `cat_fontPrintSubtitle`, its 16 uncompared IDENTICALs), the `fescreen` UBSan kind (GoK's packed 64-bit FE structs); the Bink term and the voice preview (sound's leftovers) | 282 / 48 / 2 (17,627 B); 41 unread (15,024 B) | 33 / 4,757 B | a new `/catFeScreensTest` pushes and pops every screen of the install's 22 `.fib` files once: ASan 0 and **0 `fescreen` UBSan reports**; a `/catStringsTest` resolves all 1,705 string ids; the Cataclysm option set at the exe's defaults round-trips through the options file; fe `Main_game_screen` |
| 3 | start-up, main, utility, key bindings, console | `main.c` (15 unread, 15,298 B: `WinMain` / `ProcessCommandLine` as the exe's start-up and command line, `mainStartupGL` / `mainLoadGL`, the window functions as far as SDL allows; `cat_mainCDROMPathSet`, `cat_mainLoadMPGameSet`, `cat_mainRGLFeaturesSet`), `utility.c` (11 unread: `utyGameSystemsPreInit` / `PreShutdown` / `Shutdown`, `gameStart` / `gameEnd`, `utyNewGameStart`, `utyGameQuit`, the options file; `cat_utyGameQuitToMainOrStats`, the toggles `cat_uty*`), `KeyBindings.c` (3) with `cat_kb*` (about 7 KB: load / save / reload `default_keys.cfg`, key-to-string, conflicts, the 10 hot-key groups, `cat_keyNameStringsInit` 951 B), `Key.c`, the developer console (`cat_console*`, 17 functions, about 9.5 KB: key press, run / execute, tab completion, the config file, `experienceships`, `upgradeships`, `alltech`, the 0x00a751c0 toggle), the debug-key overlay (`cat_moDebugKey*`), `screenshot.c`'s message, `debugwnd.c` (`cat_dd*` is the Win32 debug window: not applicable) | 89 / 39 / 0 (29,297 B); 29 unread (24,167 B) | 89 / 21,025 B | the install's `default_keys.cfg` loads and saves round-trip through the Cataclysm key-binding code (a `/catKeysTest`); a scripted `/catConsoleTest` runs the console's command table headless (ASan 0); start-up as the exe's `main` (the R2 workaround list stays empty); fe and sp unchanged |
| 4 | construction, research, trade and launch managers; sensors manager; the external-build driver | `ConsMgr.c` (19 unread, incl. **`cmDeterministicBuildProcess` 0x004592a0 / `Cancel` 0x00459040**, `cmCanBuildShipType`'s 3rd argument, `cmUpdateFactory`, `cmShipInfoDraw`, `cmBuildTaskFunction`; `cat_shipUpdateExternalBuildAndAI` 2,088 B, `cat_cmExternalBuildShipDied` / `Attach`, `cat_cmBuildJobFinished`, `cat_cmCancelJobsOfDeadFactory`, the SU bar `cat_cmSUBarDraw`, the support units `cat_su*`, `cat_cmQueuedSupportUnits`, `cat_mothershipModuleSlotSet`; **`CAT_EXTERNAL_BUILD_DRIVER` to 1**), `ResearchGUI.c` (11 unread: the tech lists, lab buttons, `rmResearchGUIBegin`; `cat_rmAcquireShipTechs` 747 B, `cat_rmPussyBlobDraw`, the print lists), `TradeMgr.c` (1), `LaunchMgr.c` (7 unread: the ships-to-launch and carrier draws; `cat_lmModuleSlot1..7Draw` / `XDraw`, `cat_lmBackgroundDraw`, `cat_lmShipAutoLaunchBit`), `Sensors.c` (3 unread: `smStartup`, `smSensorsBegin`, `smHorizonLineDraw`; `cat_sm*`: ships, blobs, resources, mines, hot-key numbers, bounds, `cat_smViewportRenderScene`, the HW1 BTG dimming), `cat_rsMPRBShipDependSetCB`; the dock self-test's extension (item 2 of stage 2) | 73 / 56 / 1 (30,789 B); 41 unread (22,676 B) | 41 / 17,274 B | a scripted `/catBuildTest` (harness mode `build`) builds an externally built type (a destroyer at the sMothership) and an internal one in a headless Mission01 run with the driver on: the ship appears at health 0 and finishes, ASan 0, two runs give identical streams; the dock self-test gets an sCarrier and an sProcessor host leg and judges the two ACVs; the sensors frame of `/catHudTest` draws through `cat_sm*`; the research screen's tech sets at the exe's; the mission stream unchanged until something is built (no KAS: nothing is), stated either way |
| 5 | NIS, animatic, mesh animation + repair droids, fog of war, camera | `NIS.c` (16 unread, incl. `nisStart` / `nisStop` / `nisUpdate`, `nisMeshAnimationStart`, `nisCamPathUpdate`; 114 IDENTICAL to compare; the 27 `cat_nis*` script callbacks: fog, lights, mimic, repair, slipgates, subtitles, clip planes, e-bolts, the Beast effect), `Animatic.c` (`animBinkPlay`); **mesh animation**: `MeshAnim.c` / `MadLinkIn.c` (5 unread: `madAnimationUpdate`, `madAnimBindingsDupe`, `madLinkIn*`; `madAnimationStart`'s third argument, the exe's madanim layout, `cat_madAnimClear`, the animation sounds); **the repair droids** (6 functions, 4,807 B, `Ship+0x4C0`); the FOW (`cat_fow*` and `fow*`: `cat_fowCreatePlayerGrids` at `levelInit`, the cubes `fowCreate` / `fowActivateCube` / `fowDeactivateCube` / `fowCubeHasUnvisited`, the boundary (`cat_fowBoundary*`, 3.7 KB), `cat_univShipFOW*`, `cat_shipStealthDetectionUpdate`, `cat_fowCanPlayerSeeShip` at its waiting sites, `fowDestroy` / `fowSelect`, the FOW cube of `univCreateShip` / `univWipeShipOutOfExistence`, `pingListDraw`'s test, the picking helpers' grid, `cat_saveSavePlayerFOW` / `Load`), the camera UI (`CameraCommand.c` 7 unread, `Camera.c`, `cat_mc*` motion cues 2.6 KB, `cat_cc*` scripted focus / zoom / chase), the beam's sixth argument, `cat_clampUpdateAttachedObjects` 177 B (to locate) | 184 / 39 / 0 (20,083 B); 29 unread (15,851 B) | 74 / 21,546 B | a new `/catNisParseTest` parses every NIS and NIS script of the install with ASan 0; `Player.catFOWGrid` is non-NULL for every player in a Mission01 run and the stealth update runs; the camera functions at the exe bodies; the dock self-test's harvesting Worker reaches the paused end of its jack animation; the droids of Mission01's ships update and render (`/catRenderTest`'s droid count > 0); **the mission stream re-baselined**: the pre-droid pair IDENTICAL to R5's, the post-droid pair byte-identical with the change confined to the stream-0 fields, both recorded in the report |

**R6 integrate (stage-3 exit):** from a clean build on the plain harness: fe reaches `Main_game_screen` and
`/catFeScreensTest` passes with 0 `fescreen` UBSan reports, `/catStringsTest` 1,705 / 1,705; the `dock`, `command`,
`render`, `sound`, `hud`, `ui`, `keys`, `console`, `build` and `nis` self-tests complete with ASan 0; sp reaches the
running universe with the Cataclysm HUD and renderers drawing (1,248+ ticks at 340 s, ASan 0); two 6,000-tick
Mission01 streams are byte-identical in debug and release on the new baseline, and the report states the hash and
which fields moved against R5's `645f94c69d68319e`; `/catBuildTest` passes with `CAT_EXTERNAL_BUILD_DRIVER 1`; the
statics, meshes, textures, oracle and etg self-tests are unchanged or better; every quit returns rc 0; `m2_layout32.sh`
OK; the `fescreen` UBSan kind is gone from every run (`PATCH` and `fquant.c` already are; the ETG / particle and
`BTG.c:388` kinds are stage-1/2 LP64 artefacts, listed, not required); no `CAT-TODO` / `CAT-PROVISIONAL` in `src/`
names an M2-R5 or M2-R6 owner. Then stage 4 (AI: 386 / 91 / 2, 23 cat-new; first the 14 `CAT-TODO(stage 4 AI)`
stand-ins, the `aishipFlyToPointAvoidingObjsFunc` and corvette-cone items, the `cat_ais*` team code), stage 5 (LAN,
nice-to-have: the packets of orders 11-17 and the recorder) and stage 6 (missions: the kas2c code, the NIS-first
start, `levelPreInit`'s Beast marking, the asteroid box, the letterbox callers, `CAT_HW1_MISSION_SCRIPTS` retired),
each sized by `tools/m2_stage_counts.py` when its turn comes.

## Decisions

**Applied this round:**
- The M2-R4 defaults 17-22 stand: `CAT_EXTERNAL_BUILD_DRIVER 0` until M2-R6 producer 4; the chosen names; the
  HWI deviations marked `CAT-FIX`; the two medium-confidence readings; the deleted HW1 types; the `Ship` prefix is
  now the exe's (decision 22 closed: `Ship` 0x63C, `health` 0x170, the +0x38 dword).
- The faithfulness defaults of 2026-10-10 (the fq audio code follows the binary: done in sound).
- The oracle may call the game's own functions; the user's reference session has its one-command script
  (`tools/oracle/record_reference.sh`, commit 1ad47c6, the desktop shortcuts "Cataclysm Oracle: Record / Verify").

**Still open (the user's; none blocks M2-R6):**
- **6.** The first oracle reference recording: which skirmish map and which players (`record_reference.sh record`).
- **8.** Two saves from the M0 play-test, one mission and one skirmish (`tools/formats/savegame_probe.py`,
  `saveCatLoadHook`; now also `universe.catSelection1A0`, the counters, the FOW grids and the retreat atoms).
- **10.** The first live oracle session (`record_reference.sh verify`): unverified until then: the DLL loads under
  Proton, the H6 trigger, `vqpc`.
- **The M0 play-test sign-off** (docs/PLAN.md).

**Surfaced by M2-R5.** Each has a default in the tree; say if you want otherwise.
23. **`fsqrt`.** The exe keeps HW1's table square root (`fsqrt` 0x00472860, `fmathInit` 0x004728c0: 2 x 8192
    mantissa entries, the root looked up by exponent parity and mantissa >> 7); this tree's `FastMath.h` maps `fsqrt`
    to `sqrtf` (GoK's choice) and has no `fmathInit`. Default: `sqrtf` stays. Switching changes every distance in the
    simulation and every oracle baseline, so it is the user's call; the reference recording will show it first.
24. **Names chosen** where the exe's are unknown (from the setters, the call sites or the call order): `catRetreating`
    (ShipOrDerelict 0x1D0), `catObjReserved88[4]`, `catShipReserved5B0`, `catMissileReserved248[3]`, `catNavLightStatic`
    0x634, `catDamageFactor`, `catMaxVelocity`, `catDockLatchHeading`, `catDockStateTime`, `catLinkTimer`;
    `CAT_SHIPFLAG_Launching` (bit 0 of +0x18), `CAT_SHIPFLAG_Bit7`; `CAT_ATTRIBUTES_DockBaysClosed` 0x400,
    `DockWithEnemies` 0x40000, `BlastHit` 0x20000, `BlastImmune` 0x10000; the dock purposes `CAT_DOCK_*` 0..10 and the
    dock states `CAT_DOCKSTATE_*` 0..17; `CAT_MOVEPATH_*`, `CAT_UPGRADE_Link` 0x8; `CAT_SPEECH_ACTOR_FLEET_INTEL`
    (actor 0x2000), the game-stats music track 0x28, `catBatLastStatus*`; the HUD structs `CatOverlayIcon`,
    `CatShipName`, `CatIoMessage`, `CatCmJob`. `COMMAND_CAT_13` / `COMMAND_CAT_17` stay defined but unused (no setter
    in the exe).
25. **HWI deviations where the exe would fault or leak** (40 `CAT-FIX` markers now, 7 in R4), the notable new ones:
    the dock class helpers refuse a -1 class before indexing, `cat_dockCanDockWithBusyHost` tests the point index,
    `cat_dockFlyOutOfCone` reads the point only with a host, `InitializeDockVars` tests the host before the hive
    reset, `FillInCarrierMothershipInfo` gets a four-slot array (the exe's three-slot frame overruns),
    `flightmanEndoverInit` allocates `sizeof(EndoverInfo)` (HW1 and the exe write 0x14 bytes into 0xC), a ship's extra
    blocks start at an 8-byte boundary, the `madBindings` / dock-point guards of `RemoveShipFromDocking` and
    `dockPutShipOutside`; `univCheckShipState` frees `getShipAndItsFormation`'s copy (the exe leaks it); the lens-flare
    element bound (48 / 17); `musicEventUpdateVolume`'s restart keeps the start level (the exe sets an uninitialised
    volume), `cat_soundEventBlast` / `cat_soundEventAsteroidBreak` test `enableSFX` (the exe reads a NULL LUT with
    sound effects off); `catIoMessageInsert` copies at most 0x1ff bytes (unmarked; hud audit O3).
26. **HW1-named speech ids are muted** (`HW1_SPEECH_ID` 0x400 on the HW1 actor flags): about 90 HW1-named speech call
    sites stay silent until each port passes the exe's id, instead of playing an unrelated Cataclysm line. Default:
    keep; the alternative is a bulk conversion of the call sites now.
27. **Four battle-chatter events have id 0 in the exe** (table 0x0074e8d0, entries 76 / 77 / 81 / 87) and never speak;
    the tree does the same.
28. **`rndLoadTarga` keeps GoK's `GL_CLAMP_TO_EDGE`** where the exe uses `GL_REPEAT` (the images are NEAREST-filtered
    full quads and look the same); the GoK GL backend stays (glcompat, the RGL / Glide / D3D device lists, the
    Transformer, the WGL set-up, the perspective hint, the `glMaterialfv` cache, the vertex-array BTG stars).
29. **The 0.325 pixel-centre offset.** The exe converts screen to GL with HW1's `(x + 0.325) / W * 2 - 1`; GoK's
    `src/SDL/prim2d.h` dropped it tree-wide, so the band box and the icon quads sit 0.325 px off the exe's (hud audit
    O1). A renderer decision for the user or M2-R6 producer 1; default: GoK's.
30. **The mission-stream baseline moved in command-tactics**, from tick 0 in `ships_pos` and `univcheck` (its audit's
    observation; the report recorded no hash before and after, so the cause was not isolated: the formation centre at
    the leader's collision centre and the ported command-layer bodies are the candidates, both the exe's). Default:
    the new baseline is accepted (this step: `645f94c69d68319e`, IDENTICAL to the three later audits' streams); the
    reference recording is the real check, and M2-R6's droid port moves it again (the plan's re-baseline rule).
31. **Medium-confidence readings kept:** `catRetreating`'s meaning; `cat_clUnlink` may create a command with no ships
    (as the exe, not a CAT-FIX); the dock state names; `SEselectsentence` without the N01 rotations; the two reserved
    `Ship` words.
32. **This step's two small ports** (the +0x4B0 clear in `univRemoveAllShipReferencesFromSpecialShips`, the gun-word
    copy in `cat_univConvertShip`) are grounded in the disassembly and unaudited; the M2-R6 audits may re-check them.
33. **The Win32 device layers are not ported** (the DirectSound / waveOut mixer thread, Glide, the DirectDraw debug
    window `cat_dd*`): SDL replaces them; recorded as reviewed MODIFIED low in the map.
34. **The LeakSanitizer suppressions** (`tools/lsan.supp`) and **`CAT_HW1_MISSION_SCRIPTS 0`** stand as in R3; the
    `CAT_EXTERNAL_BUILD_DRIVER 0` default as in R4 until M2-R6 producer 4.
