# Audit: M3-R3 port 4 "mines" (items 4 and 6)

Auditor's re-check of [docs/m3-r3-mines.md](../m3-r3-mines.md). Base `96b0137` (the mission02 checkpoint). The port is
the uncommitted working tree on top of `852bdd7` (an orchestrator STATE.md commit, not port work). This audit started
from scratch: the cut-off audit of 2026-10-25 left no report. Its one tracked edit, a `tools/play.sh` header block marked
`[audit: added]` that documents `/catMineLog` and `/catMeshTexLog`, was read, checked against the release binary (both
switches and the `minelog: change` format string are in it) and kept. Every run is under the label prefix
`m3r3a-mines-` in `original/scratch-m2/m2r2-harness/runs/`, through `tools/m2_harness.sh` (silent, offscreen) with
`--bin` set to this audit's clean builds. I re-read `cycle/answers/m3-r3-user.md` at the start and again before writing
this report: there is no later answer that touches items 4 or 6.

## 1. Builds (clean, separate dir)

Build dir: `original/scratch-m2/build-audit-m3r3-mines/`, configured with the setup args of `tools/build.sh`.

| Build | Result | Warnings (`-Werror`) | Size |
|---|---|---|---|
| debug (`--buildtype=debug -Db_sanitize=address,undefined`) | rc 0, 274/274 | 0 | 91,056,960 B |
| release (`-Doptimization=2 -Ddebug=false -Db_sanitize=none`) | rc 0, 274/274 | 0 | 5,773,584 B |
| win64 cross (llvm-mingw, `tools/cross/x86_64-w64-mingw32.ini`, the flags of `tools/release_win64.sh`) | rc 0, 268/268 | 0 | 5,384,704 B |
| reverted debug (`revert-debug/`, see 3) | rc 0 | 0 | 91,055,704 B |

The port adds no source file and no GL entry point. `tools/m2_layout32.sh` reports OK: all ILP32 layout asserts hold.

## 2. The exe read by me (tools/m2_fdis.py / m2_fdec.py / m2_exedata.py)

**The white cause.**
- `meshTextureRegisterAllPlayers` 0x004ae910 calls `meshNameToStaticInfo` 0x004ae7b0. It then reads the colour-scheme
  row at `lea ebx,[eax+0xf0]` (0x004ae94f) and loops 8 schemes over `teColorSchemes` 0x00bb3020 (step 0x30).
- `meshNameToStaticInfo` returns one of these statics:
  - a derelict's: `0x00ba0620 + type*0x124`
  - a ship's: `GetShipStaticInfo`
  - `MISSILE`: `0x00ba0f40 + race*300`
  - `MINE`: `0x00b3abe0 + race*300`
- `meshRecolorize` 0x004af3a0 reads `[...+0xf0]` at 0x004af4e3 and 0x004af576, and adds 0xf0 at 0x004af5a8.
- `universeDefaultTeamColors` 0x0053c940 writes the rows at these addresses:
  - ships: `0x00b3b510 + n*8`
  - missiles: `[esi+0xba1030]` (0x00ba0f40 + 0xF0)
  - mines: `[esi+0xb3acd0]` (0x00b3abe0 + 0xF0)
  - derelicts: the loop up to 0x00ba0710 (0x00ba0620 + 0xF0)

  In the exe's ILP32 layout, +0xF0 is the right row for every static kind.
- **x86-64 measured by me** (a probe compiled with the release build's flags):
  - `ShipStaticInfo.shipOrDerelict.teamColor` = **0xfc**
  - `DerelictStaticInfo.shipOrDerelict.teamColor` = 0xfc
  - `MissileStaticInfo.teamColor` = **0x100**
  - `sizeof(StaticInfoHealthGuidance)` = 0x100

  The tree's ship cast therefore read a missile's or mine's row 4 bytes early. The cause is confirmed. Because the exe
  does not have this fault, `HWI-FIX` is the right label.

**The state machine.**
- `aishipGuideMine` 0x00416410:
  - a dead mine returns 0;
  - target objtypes 2/3/5/8 are cleared;
  - state 0 goes to 3;
  - state 1 goes to 4 when there is no owner, or when the owner is farther than `custstatinfo+0x68`;
  - state 2 applies friction (×`static+0x100`, or zero when every component is in [-1.0, 1.0], 0x0072a468 = -1.0,
    0x0072a4c4 = 1.0). On the search tick (`counter & 15 == missileNumber & 15`; 0x008ac564 = 15) it acquires a target,
    and with a target it falls through to state 3;
  - state 3: with no target, or when `aishipFlyMineToTarget` returns 0, it goes to `formationinfo ? 4 : 2`;
  - state 4: the search, then the move to `formation_position` within 10.0, else friction;
  - the lifetime test closes the function.
- `aishipFlyMineToTarget` 0x00415fa0:
  - top speed: +0x10C when `+0x230` (FORCE_DROPPED) is set, else +0x28;
  - range: `MoveReachedDestinationVariable` with +0x108;
  - cloak or flag 0x800000: drop the target;
  - faster-away: `(target - mine)·v > 0.0` (0x0072a460 = 0.0, `test ah,0x41`) **and** `|v|² > max²`: drop the target.

The tree's `aishipGuideMine`, `aishipslowminewithFriction`, `mine_do_search` (`UNIVERSE_WOODPECKER` with
`UNIVERSE_UPDATE_RATE_SHIFT` 0 at 16 Hz) and `aishipFlyMineToTarget` match the exe line by line. The producer's
statement is right. The data (`bigfile.py cat`) agrees:
- the Derelict `mine.shp` has maxvelocity / _FORCED 1000;
- the Taiidan one has 600 / 600;
- both have `MINE_STOP_FRICTION` 0.50 and `MINE_RANGESQR(_FORCED)` 8000 (`scriptSetReal32SqrCB`: 64,000,000);
- sAcolyte 825, sMimic 750, sRecon 1050.

**Found missing:** the lost-target lifetime. In `univRemoveAllTargetReferencesFromMissiles` 0x00547570, every missile
**and mine** that targeted a dead or departed ship gets target 0, and its life is clamped to 5.0 s
(`0x008b5c68` = 0x40a00000). The tree has the same behaviour (`cat_univClampResourceTimer`). The report's state machine
left it out (finding F2).

**12 bodies compared with the exe** (the export `cataclysm_gog_r6` through the tools):

| # | Function | Class (delta map) | Verdict |
|---|---|---|---|
| 1 | `meshTextureRegisterAllPlayers` 0x004ae910 | MODIFIED | as the review row (the +0xF0 read, the 4th arg 1, no static: one colourless registration); the HWI-FIX restores it |
| 2 | `meshRecolorize` 0x004af3a0 | MODIFIED | the three +0xF0 reads; HWI-FIX at all three |
| 3 | `meshNameToStaticInfo` 0x004ae7b0 | IDENTICAL (reviewed) | relied on; as the tree (MISSILE/MINE arrays, stride 300) |
| 4 | `universeDefaultTeamColors` 0x0053c940 | MODIFIED | the missile/mine rows at their static +0xF0; as the tree |
| 5 | `aishipGuideMine` 0x00416410 | MODIFIED | as the tree (above) |
| 6 | `aishipFlyMineToTarget` 0x00415fa0 | MODIFIED | as the tree (above) |
| 7 | `aishipmineaquiretarget` 0x004162c0 | MODIFIED | as the tree's M2-R7 port: range from static +0xF8 (`mineRange`) or +0xFC (`mineRangeForced`) by FORCE_DROPPED, the derelict/ally/KasMines/cloak-detected/Leech/mimic rejections, heaviest wins, ties by `ranRandomFn(10)` parity (= `randombetween(1,2) & 1`) |
| 8 | `aishipGuideMissile` 0x00415e60 (the missile update) | MODIFIED | as the tree (fly, cloaked-target loss, the loss check every `tweakCheckMissileTargetLossRate`, heading from velocity, the lifetime) |
| 9 | `MinelayerCorvetteFire` 0x005ba330 (the minelayer) | MODIFIED | as the tree (the gun whose last fire is older fires; the other when it is out of ammo; `cat_gunCanFire`) |
| 10 | `meshCurrentMaterialDefault` 0x004af790 (the mesh draw's texture bind) | MODIFIED | `rndTextureEnable(1); trMakeCurrent(texture[iColorScheme])` when the material is registered, else untextured: as `cat_meshMaterialTextureAndShade` |
| 11 | `InitStatMineInfo` 0x0053b660 (the mine's static and LOD load) | MODIFIED | `<race>\Mine.shp` through the mine table; the tree's port is the same (no texture code here) |
| 12 | `univRemoveAllTargetReferencesFromMissiles` 0x00547570 | MODIFIED | as the tree (owner and target cleared; the 5.0 s clamp) |

No edited function is IDENTICAL in the delta map. The functions the port edits are `meshTextureRegisterAllPlayers`,
`meshRecolorize`, `meshLoad` (a log hook only), `aishipGuideMine`, `aishipFlyMineToTarget` and `rndRenderTask`'s
callees; all are MODIFIED or tree-only. The 6 new review notes are all present. A rerun of `tools/delta_map.py merge
--done m3r3:mines` followed by `report`, in a scratch worktree from HEAD with the port applied, reproduced
`ghidra/delta_map.json` and `docs/m2-r1-function-deltas.md` **byte for byte** (records changed 6; IDENTICAL 2,757,
MODIFIED 1,580, REWRITTEN 83, reviewed 3,582).

## 3. The texture fix is not vacuous (a reverted build)

The scratch worktree `original/scratch-m2/wt-audit-m3r3-mines` (detached at `852bdd7`) has the port's diff applied,
then **the four call sites reverted to the ship cast**
(`((ShipStaticInfo *)info)->shipOrDerelict.teamColor[...]`, identical to HEAD's lines). The logging stays in, and its
`teamColor=` column prints the true row. The reverted build is `revert-debug/hwi-engine`. The worktree was removed after the audit; the port's diff is kept in the session scratchpad (`mines-port.diff`).

The draw log (`-- /catMeshTexLog /MINE/RL /catMineView 300:150`, Mission03):

| Run | Level mine (scheme 0) | Laid mine (scheme 5) |
|---|---|---|
| `shot-lvl-before` / `shot-drop-before` (reverted) | `textures=11 bound=0 white=11`, all `no-handle-for-scheme`; Derelict handles registered for schemes 4, 5 (`..,486,10486,..`) | `textures=4 bound=0 white=4`, all `unregistered`; `handles=none (nothing registered)` |
| `shot-lvl-after` / `shot-drop-after` (fix, batched) | `textures=11 bound=11 white=0` (`sidereardisks` 64x64, `botmid` 32x64); handles schemes 0, 1 | `textures=4 bound=4 white=0` (`page0` 64x64); handle scheme 5 |
| `shot-lvl-imm` / `shot-drop-imm` (`/meshImmediate`) | the same | the same |

FileLoads: 3,170 before, 3,174 after. The 4 new loads are `RACE_TAIDAN/MINE/RL0/LOD0..3/PAGE0.LiF` (finding F3).

**The captures**, counted with `tools/ppm_region.py stats` and independently with my own PPM reader
(`scratchpad/minepx.py`: non-black pixels split into those inside the reverted frame's flat palette and the rest; region
and whole-frame pixel diffs):

| Capture | Region | reverted | fix (batched) | `/meshImmediate` | pixels changed (region) | after vs immediate (whole frame) |
|---|---|---|---|---|---|---|
| level mine, tick 400 | 432,304,592,464 | 19 colours, mean 221.17/176.46/130.23; 25,389 non-black, **all** in the flat palette | 10,657 colours, mean 85.02/62.92/41.19; 25,136 non-black, **0** in the flat palette | identical | 25,389 / 25,600 | **0 px** |
| level mine, tick 640 | same | 19 colours; 25,401 flat | 10,582 colours; 0 flat | identical | 25,401 | **0 px** |
| laid mine, tick 5000 | 480,350,544,418 | 6 colours; 1,343 flat | 970 colours; 2 flat, 1,341 textured | identical | 1,341 / 4,352 | **0 px** |
| laid mine, tick 6000 | same | 6 colours; 1,333 flat | 977 colours; 1 flat, 1,332 textured | identical | 1,332 | **0 px** |

These match the report's figures exactly. A visual crop (`scratchpad/mines_cmp.png`) shows two flat, pale, untextured
solids before the fix. After it, the level mine is textured (brown metal, vents, panels) and the laid mine is a textured
cube with yellow-tinted corners.

**The missile misread** (`-- /catMeshTexLog MISSILE/RL`; `missile-before` on the reverted build, `missile-after` on the
fix):

| Static | Row | Schemes before | Schemes after |
|---|---|---|---|
| Sect | 11000000 | 4, 5 | 0, 1 |
| P1 | 10000100 | 4 | 0, 5 |
| Beast | 01000000 | 5 | 1 |
| Kushan | 00000000 | none | none |

This is the report's table. Both runs' streams are IDENTICAL to `m3r2i-e-m3-a`: the reverted and the fixed build move
no simulation field.

## 4. The per-tick mine log

`m3r3a-mines-chase-recon` (`--mission-test-ticks 1400 -- /catMineLog 480 /catMineChase 400`):
- `minechase` orders 2 Recons past mine 63.
- Mine 63 is acquired at 1295 (on its search tick: 1295 & 15 = 63 & 15) at distance 9,818, and reaches 1,000 u/s at
  1310.
- It closes to 239 at 1375, and at **1376** goes `3->2 why=faster-away` at 975.3 u/s.
- It brakes: 487.6, 243.8, 121.9, 61.0, 30.5, 15.2, 7.6, 3.8, 1.9 ...
- 1391 `2->3 acquired`, then 1392 `3->2 faster-away`.
- Mine 62: acquired at 1294, `faster-away` at 1397 (103 ticks).

All of this is exactly the report's table.

`m3r3a-mines-chase-acolyte`:
- Mine 63 is acquired at 1535 and its log ends at 1751 at 38 units.
- Mine 66 goes `cleared` at 1753 and re-acquires Acolyte #10 at 1762.
- Mine 62 goes `faster-away` at 1755.

All as stated. **Also seen:** mine 66's log ends at **1833**, 80 ticks after its `cleared`, at 5,666 units from Acolyte
#10: the 5 s lost-target expiry (F2).

`m3r3a-mines-ailog` (the full self-test, `-- /catMineLog 480`, 5/5 PASS):
- 199 `0->3` and 199 `3->2` (level mines);
- 30 `1->4` (laid);
- 6 `4->3` on sMimic#19 at 14138 .. 14171;
- **4** `3->2 why=cleared` at 14391 (ids 217, 219, 220, 222);
- 4 `2->3` on sMothership#0 at 14393 / 14395 / 14396 / 14398;
- 2 level mines acquire Recons at 16871 / 16875.

**Not as stated:**
- Mines 218 and 221 log their last ticks at 14389, at 45 and 71 units from the Mimic: they reached it. Only four
  "lose it" (F1).
- The four that re-acquired the command ship log their last ticks at 14470 / 14471, about 1,000 units from it: they
  expired 5 s after the `cleared` (F2).

The log is read only: the `ailog` stream is IDENTICAL to the baseline.

## 5. Streams, replays, dumps, self-tests (from this audit's clean debug build; ASan 0 and no unsuppressed leak in every run)

Judged with `python3 -I tools/oracle/hwo_compare.py`, all 21 fields. The sha256 values below are of this build's
files, the next port's reference.

| Stream | Run | Against | Result | sha256 (16) |
|---|---|---|---|---|
| headless Mission01, 6,000 ticks | `mission-a` / `-b` | `m3r1i-mission-a/run.hwo` (1ae66610c3f02410, present) | **IDENTICAL** 6,001 ticks, every field incl. `rng_all`; a vs b IDENTICAL; 1,476,370 B | 61025ebd90b619bf (both) |
| missiontest Mission01 NIS-first | `missiontest-a` / `-b` | `m3r2i-e-missiontest-a` | **IDENTICAL** 11,426 ticks; 6/6 PASS, done 11,424 | 3d8eba9d55eb5888 (both) |
| **Mission03** | `m3-a` / `-b` | `m3r2i-e-m3-a` (d737643a58c0f58b) | **IDENTICAL** 17,794 ticks; a vs b IDENTICAL; 5/5 PASS, 17,825 | bd440d2584b51df8 (both) |
| Mission03 failed jump (pair) | `fj3-a` / `-b` | `m3r2i-e-fj3-a` (a3044330e4a63b5b) | **IDENTICAL** 17,669 ticks both; `failed jump judged ... PASS`; 5/5 at 17,667 | f1a67179fd1a0d70 (both) |
| team colours Mission03 into Mission04 | `tc-m3` | `m3r2i-e-tc-m3` (df68de4e18348963) | **IDENTICAL** 17,794; teamcolour 6 checks, 0 FAIL | 367ed7c7b6c7f80c |
| Mission02 | `m2` | `m3r2i-e-m2-a` (7497c9e5900b59b0) | **IDENTICAL** 25,905; 6/6, 25,936 | 272832c0d19367cd |
| Mission04 | `m4` | `m3r2i-e-m4-a` (9f141e8fb1927d7f) | **IDENTICAL** 20,787; 5/5, 20,818 | 32a5c0e5a46e63ce |
| Mission06 (minelayers) | `m6` | `m3r2i-e-m6-a` | **IDENTICAL** 30,980; 7/7, 31,011 | b02c882d130d2f57 |
| Mission10 | `m10` | `m3r2i-e-m10-a` | **IDENTICAL** 21,908; 5/5, 21,939 | 154e3138d78d181b |
| Mission12 | `m12` | `m3r2i-e-m12-a` | **IDENTICAL** 10,964; 3/3, 10,995 | bc62735baf5cc62f |
| Mission14 | `m14` | `m3r2i-e-m14-a` | **IDENTICAL** 22,307; 4/4, 22,338 | 0ec14f47f715932c |
| skirmish LiveFireExercise2 | `lfe2` | `m3r2i-e-skirmish-lfe2-a` | **IDENTICAL** 601 | cf88260521d6d3e3 |
| skirmish Armed8 | `armed8` | `m3r2i-e-skirmish-armed8` | **IDENTICAL** 601 | 5a823cbe4d7249e9 |
| replay 10-17 | `r17` | `m3r2i-e-r17-a` | **IDENTICAL** 9,601 ticks / 4,800 packets; vs GOG `oracle01-rec-20261017-140428/oracle01-rec.hwo` (`--from-tick 1 --to-tick 9600 --fields rng10,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total`): **first divergent 2823** `ships_vel` | f5aecb5d4cd6c45f |
| replay 10-18 | `r18` | `m3r2i-e-r18-a` | **IDENTICAL** 3,527 / 1,763; vs GOG `oracle01-rec-20261018-102253/oracle01-rec.hwo`, same fields: **first divergent 2871** `ships_vel` | 0226586babc26fa4 |

The replays are run with `--recording oracle01 --recording-src original/oracle/<session> --recording-as oracle01-r6|r18`.
The install's own recording is gone, so the harness's default call (`--recording oracle01-r6`) stops with "no
recording". This is not a finding: it is how M2-R7's integrate set them up.

The capture runs (`shot-*-after`, with `/catMineView`) move `rng_all` from the camera move: 2569 in `shot-lvl`, 4833 in
`shot-drop`. They are test scenes and carry no baseline, as the report says.

**The four dumps** (first 16 hex of sha256, whole / with `crc=` stripped):

| Dump | This audit | M3-R2 `m3r2i-e-*` | Lines |
|---|---|---|---|
| `motion-noi` | c503c869f0dc9cdb / 3016280e9e0dd3c8 | the same | **EQUAL** (byte-equal), 1,436 |
| `motion-on` | c0e411662fa2f527 / 428ef41479eb5749 | the same | **EQUAL**, 1,436 |
| `nis-noi` | 86cb155acffc180e / 196bab32d244f614 | the same | **EQUAL**, 1,489 |
| `nis-on` | 0307f3be2044c29c / a5de13a44b8d264f | the same | **EQUAL**, 1,489 |

**The self-tests:**
- render: `rendertest: done tick=599 frames=583 objects=582 ... result=PASS`.
- hud: `hudtest: done tick=599 ... frames=537 ... result=PASS`.
- meshes: 1,758 files, 0 with bad objects.
- textures: 12,403 listed and found, 10,661 registered and loaded, 0 not loaded.
- fe: `Main_game_screen` at the 120 s timeout, ASan 0.
- sp (`--timeout 1200`): reached Mission12 (`Mission12.btg` loaded) at the timeout (rc 124, as M3-R2's `m3r2i-e-sp` and the producer's `m3r3-mines-sp`), ASan 0, no unsuppressed leak, under a load average of 15-20 from other processes.

## 6. The diff (`git diff 96b0137`)

Port work:
- src/Game/Mesh.c, Mesh.h, AIShip.c, AIShip.h, OracleDump.c; src/SDL/render.c, render.h, main.c;
- tools/m2_harness.sh (header only);
- the delta map trio and the report.

Not port work: `cycle/STATE.md` (orchestrator commit `852bdd7`) and the cut-off audit's `tools/play.sh` header (kept,
see the top).

Checks:
- **Tags.** The HWI-FIX is tagged on every line of `meshStaticTeamColor` and at its four call sites. Every test-only
  block carries `HWI-TEST`: the function headers, the includes, the switch-table rows, the call hooks in `meshLoad`,
  `cat_rndRenderObjectList`, `aishipGuideMine` / `aishipFlyMineToTarget`, `oracleUnivUpdate` and
  `catLodStatsFrameBegin`.
- No `CAT:` line is added: no Cataclysm behaviour change, correctly.
- No `HWI-FEATURE` is needed: no setting.
- No CAT-TODO or CAT-PROVISIONAL is added. The only `CAT-TODO(M3-R3)` in src/ is BTG.c:1151, the integrate's.
- **Simulation.** With the switches off, the hooks read state only (`catMineLogStep` returns at `catMineLogTicks <= 0`;
  `catMineChaseRequested()` is false). Every guard-rail stream is IDENTICAL, so no simulation field moved.
- **Windows.** No `_WIN32` code is touched and no file is added. The cross build compiles with 0 warnings.
- **Generated files.** They are reproduced by their generator (section 2).
- **The address test.** `meshStaticTeamColor` uses an address range test on `missileStaticInfos[NUM_RACES]` /
  `mineStaticInfos[NUM_RACES]` (both declared `[NUM_RACES]`, Universe.h:313-314). A one-past-end compare is valid C. The
  derelict row is at 0xfc as the ship's, so the ship cast stays right for derelicts.

## Findings

| # | Finding | Evidence | Fixed |
|---|---|---|---|
| F1 | Report (b): "six laid mines ... lose it at 14391" is wrong. Two of the six (218, 221) reached the Mimic and only four logged `cleared`. | `m3r3a-mines-ailog`: 218 last tick 14389 at 45 units, 221 last 14389 at 71 units; `3->2 why=cleared` only for 217/219/220/222 | yes, `[audit: ...]` in docs/m3-r3-mines.md |
| F2 | Report (b): the stated exe state machine leaves out the exe's **lost-target lifetime**. When a mine's target is cleared by `univRemoveAllTargetReferencesFromMissiles` 0x00547570, its remaining life is clamped to 5.0 s (0x008b5c68). The mines that "re-acquire the command ship" expire 80 ticks later (14470 / 14471), and so does Acolyte-chase mine 66 (1833). This is the exe's behaviour and the tree's (`cat_univClampResourceTimer`), so nothing is to be ported, but the user will see it and the user's check did not say so. | the disassembly / decompile of 0x00547570; the two logs | yes: a table row, two narrative notes and a sentence in THE USER'S CHECK, all `[audit: ...]` |
| F3 | Report (a): "3,170 -> 3,174 (the mine `.LiF` files now read)". The four new loads are the Taiidan mine's `PAGE0` LOD0-3 only. The Derelict mine's materials add no load: they resolve to `RACE_DERELICT\dJunk2Shipwreck\...\sidereardisks.LiF` / `dJunk1Strut\...\botmid.LiF`, already read for those derelicts (registry sharing), before and after. | `diff` of the two FileLoads.log; the draw log's `loaded 64x64` | yes, clarified `[audit: ...]` |

Nothing else was found. The cause, the fix, the HWI-FIX label, the AI statement ("the exe does the same, nothing
ported"), every stream / replay / dump statement and every figure I re-measured are correct.

**Precision.** 40 sampled checks:
- the cause (5: the +0xF0 read, the three recolour reads, the x86-64 offsets, the default rows' addresses, the static
  mapping);
- the before / after logs (4);
- the FileLoads count (1, the number right);
- the four capture regions and the batched / immediate equality (2);
- the missile table (1);
- the state machine and the faster-away rule (3);
- the constants and the speeds (2);
- the Recon chase (2);
- the Acolyte chase (1);
- the self-test log narrative (1, wrong: F1);
- the completeness of the stated machine (1, wrong: F2);
- the 16 stream / replay / dump / self-test statements (Mission03, failed jump, team colours, missions 6 / 10 / 12 / 14,
  skirmishes, Mission01 pair, missiontest, replays with the rungs, dumps, render, hud, meshes, textures);
- the builds, the generator, the layout, the CAT-TODO rule and the user's check (5).

Precision = 38 / (38 + 2) = **0.95**. F3 is a clarification of a correct number, not counted wrong.

**Fixes in place: 5.** All are doc edits marked `[audit: ...]` in docs/m3-r3-mines.md:
- the FileLoads attribution;
- the lost-target lifetime row;
- the Acolyte mine-66 expiry;
- the "six -> four" note with the expiry;
- the note in THE USER'S CHECK.

There are no code changes, and the build is green. The cut-off audit's `tools/play.sh` header was verified and kept.

## The user's check (verified)

`tools/play.sh` exists and rebuilds when the binary is older than the newest src/ commit. Mission03 and its minefields
on the way to the CLEE-SAN are real: `mineview` focuses level mine 63 at (36747 39894 -348). The minelayers' mines are
from 4832 on. `-- /catMineLog 480` is a real switch: the release binary carries `/catMineLog`, `/catMeshTexLog` and the
`minelog: change` format. With the F2 note added, the check is complete.

Verdict: PASS-WITH-FIXES - the white mines were an x86-64 colour-row misread (HWI-FIX), proven by a reverted build (flat 19 / 6 colours -> 10,657 / 970 textured, batched = immediate); the mine AI is the exe's; every stream, both replays (2823 / 2871) and the four dumps are IDENTICAL / EQUAL, ASan 0; fixed 3 report errors (six -> four mines, the missing 5 s lost-target lifetime, the FileLoads attribution) in 5 doc edits
