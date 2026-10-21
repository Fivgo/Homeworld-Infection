<!-- done: a-exe-read, b-port, c-selftest, d-readers, e-fence, f-perf -->
# M3-R1 producer 3: FULL DETAIL = LOD 0 always, default ON (full-detail)

Subsystems: (a) exe-read: what cataclysm.exe's FULL DETAIL checkbox and detail slider do; (b) port: the draw level,
the default, the presets and the setting's surfaces; (c) selftest: `/catLodTest`, `/catLodStats`, the toggle /
console / optionstest checks, the harness flags; (d) readers: the simulation-reader proof and the effects-cap verdict;
(e) fence: the streams, the dumps, fe / hud / render / fescreens, sp; (f) perf: the 4K frame time.

Base 2b454b6 (producer 2's audited tree). Work so far: `git diff 2b454b6` in the m2 worktree. `<R>` below is
`original/scratch-m2/m2r2-harness/runs`. All six subsystems are finished.

## Headline

- **What the exe does**: FULL DETAIL (`noLOD`, opNoLODVal 0x00a4cc34) only stops the main view's frame-time detail
  cuts (rndMainViewRenderFunction 0x00576c03); `lodLevelGet` 0x004a4d70 still drops small / far objects to levels
  1..5 by their size on screen. The detail slider is a frame-rate floor (30 - slider fps) for those cuts, greyed while
  FULL DETAIL is on. So the user's "noLOD 1, models still change" is the exe's behaviour; the tree already had it.
- **The feature**: HWI-FEATURE(full-detail): with FULL DETAIL on every object is DRAWN at its LOD 0 (ships and their
  conversion / mimic meshes, asteroids, derelicts, missiles, dust and gas clouds, nebula tendrils, the sensors
  manager's meshes, nav lights, repair droids, the engine-trail nozzles); `currentLOD` keeps the exe's level for every
  non-draw reader. **Default ON** (`noLOD 1`), persisted by the exe's `noLOD` row, the exe's FULL DETAIL checkbox,
  the high preset 1 / low preset 0, the run-only switch `/lodBands`, the console row `noLOD`.
- **No stream moved**: the headless Mission01 pair and the release run IDENTICAL in every field to r10i-mission-a /
  -rel-a; all 17 per-mission self-tests PASS with every objective, each IDENTICAL in every field (rng_all included) to
  R10's; every other guard rail IDENTICAL (the 4,000-tick soak in its 20 fields, rng_all load-dependent as decision
  84); the rung still 2823 / 2871. One draw had to be kept on the exe's level to get there: the Beast-stun edge
  lightning (its bolts draw stream 0; a LOD-0 lightning moved `rng0` in Mission04 / 10 / 14, found and fixed).
- **The cost at 4K** (release, offscreen, idle GPU, 164 ships in view): FULL DETAIL on median 10.5 / 9.8 ms, p99
  15.0 / 14.3 ms, 25,169 vertices / 46,255 polygons a frame; off (the exe's bands) median 2.7 / 3.3 ms, p99 4.1 /
  4.2 ms, 4,726 vertices. The same at 1024x768 (12.5 / 3.7 ms): vertex submission, not fill: under 144 Hz's 6.9 ms in
  that scene. An item for the 4K / frame-pacing work (below).
- **The effects cap** is not lifted: it is Relic's per-effect data limit, applied in the simulation's update: M3-R2.

## (a) What cataclysm.exe does (export cataclysm_gog_r6, tools/m2_fdec.py / m2_fdis.py, raw xrefs of the exe)

- **FULL DETAIL** is `opNoLODVal` at **0x00a4cc34** (.bss, 0 at start; the options table row `noLOD` at 0x008c14a8 ..
  0x008c14b0, scriptSetUdwordCB). A scan of cataclysm.exe for the address finds 9 code references, all named:
  `opRestoreSavedSettings` 0x004d7f80 (0x004d8150: Cancel restores it), `opOptionsSaveSettings` 0x004d8d00
  (0x004d8ff1: the Cancel copy), `opNoLOD` 0x004d9d60 (0x004d9d6f / 0x004d9d8c / 0x004d9dc3: the checkbox),
  `opDetailThreshold` 0x004da5e0 (0x004da654: the slider greyed while it is set), `cat_opSetAllDefaults` 0x004db7b0
  (0x004db829 / 0x004db97c: both frame-rate presets write **0**) and **one** reader outside the options code:
  `rndMainViewRenderFunction` 0x005764e0 at **0x00576c03** (`mov eax,[0xa4cc34]; test eax,eax; jne 0x576c1a`).
- The main view (0x00576baa .. 0x00576c18): pass 0 calls `lodLevelGet` 0x004a4d70 for every visible ship / asteroid /
  gas / dust / derelict / missile with a LOD table and sums the levels' estimated render times; passes 1..10 call
  `cat_lodLevelGetTimed` 0x004a5000, which drops every object at the pass's level (table 0x0072be84: 99, 3, 2, 1, 0, 3,
  2, 1, 3, 2, 3) one level, while the sum exceeds `catLodTimeThreshold` 0x0088486c. **FULL DETAIL on ends the loop
  after pass 0**: no frame-time reduction. That is all the checkbox does in the exe.
- `lodLevelGet` 0x004a4d70 never reads 0x00a4cc34. It sets `currentLOD` (+0x34) from the **DetailQuality** radio
  `catLodDetail` 0x00a4cc38 (`cat_opLODDetail` 0x004d9d20; 0 = high, 2 = low) plus a band of the object's selection
  circle radius (its size on screen, set by the visibility test): radius < 0 or > 0.4 (0x0072a674): the detail level;
  > 0.12: +1; > 0.045: +2; > 0.017: +3; > 0.0065: +4; else +5; clamped to the table's last level. A table with the
  distance flag (+8, `catDistanceLOD`; [audit: said +0x20; the exe reads `BYTE PTR [ecx+0x8]` at 0x004a4dce]) uses HW1's bOff / bOn distances times `lodScaleFactor` 0x00884868 instead, never above
  `catLodDetail`. So with FULL DETAIL on, a small or distant object still drops to level 1..5 **by the bands**: the
  user's report ("noLOD 1, detailThreshold 10, models still changed detail") is the exe's behaviour.
- **The detail slider** (`opDetailThreshold` 0x004da5e0 / `opDetailThresholdProcess` 0x004d9b20): a frame-rate floor,
  not a polygon count: `opDetailThresholdVal = 30 - slider` (slider 0 .. 21: 30 .. 9 fps; .data default 10 at
  0x0088be6c), and `cat_lodSetTimeThreshold` 0x004a50e0 sets the budget `catLodTimeThreshold = counter frequency /
  fps` (the user's 10: 100 ms of estimated render time). It only acts through the passes, so FULL DETAIL greys it
  (RSF_RegionDisabled on the region 0x00a4cc70). The estimates come from timed renders (`cat_lodTimingStart` /
  `Stop` 0x004a4cf0 / 0x004a4d20) and, once 20 ship levels are timed, a seconds-per-polygon scale `catLodTimeScale`
  0x00a75da4 measured by the main view (0x00577402).
- The tree already had all of this ported (M2-R5 render, M2-R6 frontend): `lodLevelGet` / `cat_lodLevelGetTimed`
  (LOD.c), the loop with `!opNoLODVal` (render.c), the slider and the checkbox (Options.c). Compared again this round
  and found as the exe (the review rows below); no Cataclysm-behaviour change was needed, so the CAT: lines are the
  comments that now state the exe's reader (LOD.c `lodLevelGet`; delta-map review notes).

## (b) The port: HWI-FEATURE(full-detail)

Design: **`currentLOD` keeps the exe's level everywhere; the level DRAWN is a separate value.** `cat_lodDrawLevel(obj)`
(LOD.c) is 0 while FULL DETAIL (`opNoLODVal`) is on and the object's (apparent, a mimic's disguise) table has a mesh at
level 0, else `currentLOD`; `cat_lodDrawLevelGet(obj, &level)` returns that level of the apparent table. With FULL
DETAIL off every draw site gets `currentLOD` back, so the setting at 0 is the exe bit for bit. Because `currentLOD`,
`renderedLODs` and every non-draw reader are untouched, the simulation sees exactly the exe's levels with the setting on
or off (section (d)).

File by file (every line `HWI-FEATURE(full-detail)`):

Counts: `/bin/grep -r "HWI-FEATURE(full-detail)" src | wc -l` = 169 lines in 14 files. [audit: 178 after the audit: 3 changed lines in render.c had no tag (the two `level[drawLOD].pData` continuation lines and the edge effect's `else if`), tagged; and the two `renderedLODs` guards (6 lines)]

- **src/Game/LOD.c / LOD.h**: `cat_lodDrawLevel`, `cat_lodDrawLevelGet`; the run-only switch state
  (`catLodBandsSwitch`, `catLodSwitchEnd`, `catLodSwitchStands`, `catLodFileValueGet`, `catLodFileValues` around
  utyOptionsFileWrite, `catLodSwitchStateSave` / `Restore` with the Cancel copies). `lodLevelGet`'s comment now states
  the exe's single FULL DETAIL reader (CAT:).
- **src/SDL/render.c** `cat_rndRenderObjectList` (the main view's draw of every object): the ship's mesh level
  (`cat_lodDrawLevelGet` instead of `cat_lodCurrentLevelGet`), the bindings / mesh-state level passed to the mesh draw,
  the Beast-conversion, salvage-conversion and mimic meshes (and the mimic crossfade), the repair droids' draw gate,
  the asteroid / derelict / missile mesh and the dust / gas cloud's level (`cloudRenderSystem`). **Kept on the exe's
  level** (comments say why): `renderedLODs` (KAS `RenderedShips` / `RenderedDerelicts` and the AI's
  `aitCheckAmIBeingWatched` read it), the trails' style (`trailDraw` with `currentLOD`: its TRAILS-stream draws depend
  on the level), the ACV's link bolts (`cat_rndLinkLightningDraw`: stream-0 draws per segment), the ship lightning's
  width, the selection's status bars and precise hit (UI, the exe's).
- **The Beast-stun edge lightning** (render.c, before `cat_meshRenderEdgeEffect` / `cat_meshRenderShipHierarchyEdgeEffect`;
  Mesh.c unchanged): kept on the exe's level, because its bolts draw stream 0 by the level's polygons and segments
  (section (e)); when FULL DETAIL draws another level it is drawn on the exe's level's mesh with the objects' rest
  matrices (that level's animated binding matrices are refreshed only when it is drawn).
- **src/Game/NavLights.c** `RenderNAVLights`: the `minLOD` test reads the level drawn (a ship drawn at LOD 0 shows its
  LOD-0 lights).
- **src/Ships/RepairDroid.c** `cat_droidRenderArray`: the draw gate (owner level >= 2) reads the level drawn; the
  droids' runtime (`droidUpdateArray`, stream-0 draws) keeps `currentLOD`.
- **src/Game/Trails.c** `trailGetNozzleOffset` and **src/Game/UnivUpdate.c** `cat_shipNozzleInfoGet`: the engine
  nozzle's mesh object and its binding matrix come from the level drawn (the render updates the bindings of the level
  it draws, so the trail leaves the mesh on screen); positions only, no draw.
- **src/Game/Sensors.c** `cat_smShipMeshDraw`: a ship (and its clamped modules) the sensors manager draws as a mesh is
  drawn at LOD 0; one it draws as a blip (its band level is no mesh) stays a blip (the manager's display language).
- **src/Game/Nebulae.c** `nebRenderNebula`: the tendrils at LOD 0 (the exe: `nebLOD`, 1 when neither chunk is on the
  render list); the tendril geometry builds both levels at once, so no RANDOM_NEBULAE draw moves.
- **The default**: `opNoLODVal = 1` (Options.c; the exe's .bss 0). The options file's `noLOD` row (utility.c,
  unchanged position) persists it; a file without the row (every harness run) gets 1.
- **The presets** (`cat_opSetAllDefaults`, the Options frame-rate radio, the exe's only "defaults"): high sets 1 (the
  exe 0), low keeps 0 (weak hardware keeps the exe's bands); both end a standing `/lodBands`; afterwards the FULL
  DETAIL checkbox and the slider's greying are refreshed (`feToggleButtonSet("OP_NoLOD")`, `opDetailDisable`; the exe
  leaves them stale).
- **The checkbox** (`opNoLOD`, IDENTICAL to the exe): a click that changes the value ends `/lodBands` (the screen's
  FAS_OnDelete call re-reads the box and must not end it: found by the toggle test). The control is the exe's own
  OP_NoLOD on Video_Options and the in-game Video screen; no new control.
- **src/SDL/main.c**: `/lodBands` (run-only, as `/noInterp`), `/catLodTest`, `/catLodStats` (HWI-TEST).
- **src/SDL/utility.c**: `catLodFileValues(TRUE/FALSE)` around the write (the switch never reaches the file).
- **src/Game/Console.c**: HWI row `CAT_CON_HWI_ROW_ID + 3`, type 5, `noLOD` (CAT_CON_HWI_ROWS 3 -> 4: the console
  self-test shows 33 and sets 26).
- **src/Game/Options.c / .h**: the default, the presets, the switch state in the Cancel copies, the optionstest row
  `noLOD` (expected 1, exe address 0x00a4cc34: options=35), `opNoLOD`'s prototype for the toggle test.
- **tools/play.sh**: the header documents the setting, its three surfaces and the default.

| surface | where | behaviour |
|---|---|---|
| Cataclysm.cfg row | `noLOD 0\|1` (the exe's row) | default 1 (FULL DETAIL); 0 = the exe's bands + frame-time passes |
| Options control | the exe's FULL DETAIL checkbox (OP_NoLOD, Video_Options / Video) | ticked by default; greys the detail slider (the exe) |
| presets | Options frame-rate radio (cat_opSetAllDefaults) | high: 1, low: 0 |
| switch | `/lodBands` | FULL DETAIL off for the run; the file keeps its row (run-only) |
| console row | `noLOD 0\|1` (HWI row) | sets the run's value at once |

## (d) LOD is render-side: every reader of currentLOD / renderedLODs, and the effects cap

`/bin/grep -rn "currentLOD\|renderedLODs\|lodLevelGet\|cat_lodCurrentLevelGet\|cat_lodLevelGetMimic" src` (src/reference
excluded). The proof is by construction: FULL DETAIL changes only `cat_lodDrawLevel`'s value, and only the draw sites
of section (b) read it; `currentLOD` is written by the same code as before (`lodLevelGet`, `cat_lodLevelGetTimed`,
`cat_lodLevelGetMimic`, the creators' `= 0`, the copies below) and `renderedLODs` from `currentLOD`. Every reader that
is not a draw keeps the exe's level, so its result is the same with the setting on or off:

| reader | file | kind | reads |
|---|---|---|---|
| `kasfRenderedShips` / `kasfRenderedDerelicts` (KAS RenderedShips 0x00497020 / RenderedDerelictType 0x004970c0) | KASFunc.c | **simulation** (mission scripts) | `renderedLODs`, set from `currentLOD` (unchanged) [audit: not quite: the bit is set in the LT_Mesh draw path, and under FULL DETAIL an object whose exe level is not a mesh (the planet derelicts' level 1 is `SubPixel`, derelicts/*.lod; the dust clouds' / nebula's level 4) took that path and set `1 << currentLOD` where the exe (0x005776f0: the bit after meshRender only; case 3 draws a point) sets none. No shipped mission reads a planet's bits (RenderedDerelictType is called for Shipwreck and PrisonShipOld only, all-mesh tables), so no stream moved; the audit guarded both sites in render.c with the exe level's type] |
| `aitCheckAmIBeingWatched` | AITeam.c | **simulation** (AI) | `renderedLODs & 7` (unchanged) |
| `droidUpdateArray` / `cat_droidOwnerDone` (0x005c32f0) | RepairDroid.c | **simulation** (stream-0 draws) | `currentLOD` (unchanged); the droids walk LOD 0's hull whatever the level |
| the effect level of a fire / hit / damage / explosion / beam (`etgLOD->level[currentLOD]`) | Gun.c, Damage.c, UnivUpdate.c, ResCollect.c, Salvage.c, Crates.c, DFGFrigate.c, Leech.c, RammingFrigate.c, Sentinel.c, Slipgate.c, Processor.c, Worker.c, GravWellGenerator.c, GenericInterceptor.c, RepairCorvette.c, RepairDroid.c | **simulation** (created in the universe update; RANDOM_ETG / particle draws) | `currentLOD` (unchanged) |
| the copies `missile/bullet/effect->obj.currentLOD = ship->obj.currentLOD`, `dummyTarg` | Gun.c, ETG.c, HeavyCorvette.c | simulation | the copy of the unchanged level |
| the mesh collision (`cat_meshLineIntersect` 0x004b5910) | Collision.c, RammingFrigate.c | simulation | fixed levels (bullets / missiles 3: 0x00752c50 / 0x00752c54; the ram 0), `localMatrix`: never the current level |
| the mesh state (damage per vertex) | UnivUpdate.c, Mesh.c | simulation | every level's slots, all updated (`cat_univMeshStateAlloc` / the vertex walks loop over every level) |
| the gun / dock / nav-light / engine points | Gun.c, Dock.c, MEX.c | simulation | static info (`GunStatic`, `DockStaticPoint`, MEX chunks bound per level at load); the guns' binding callback `gunMatrixUpdate` writes only the drawn matrix |
| the bounding boxes / collision spheres | StaticCollInfo | simulation | static, no level |
| the selection's precise hit, the status bars | Select.c, PopupMenu.c | UI (input) | `currentLOD` (unchanged, the exe's) |
| `trailDraw` / `mistrailDraw` style, the ACV link bolts, ship lightning width | render.c, Trails.c, Clouds.c | render (stream draws) | `currentLOD` (unchanged: their TRAILS / stream-0 draws depend on it) |
| the save / load | SaveGame.c, SaveCat32.c | | no LOD reader (grep: none); the byte travels inside the saved object block as the exe's level wrote it (unchanged by the setting) and the main view recomputes it every frame; NetCheck.c sets 0 |
| sensors | Sensors.c | render (the manager's draw) | `cat_lodLevelGetMimic` still sets `currentLOD` (the exe's); the mesh drawn is LOD 0 |
| the AI's visibility, the sensors' detection | AIShip.c, AIPlayer.c, Sensors.c | simulation | no LOD reader (grep: none) |

No simulation reader of the level the setting changes: the item is not stopped.

**The effects cap: not lifted; an M3-R2 item.** What the exe caps: `etgFrequencyExceeded` 0x00470bd0 (the tree's is the
same): each effect static's history list (+0x5c entries, +0x60 index, +0x64 times) counts the instances spawned in the
last second; the limit is `nHistory * etgHistoryScalar / 256`, and below it the effect is still skipped when
`ranRandom(RANDOM_ETG = 26) % (max - count + 1) + count < count`. `etgHistoryScalar` = the effects slider + 64
(`etgHistoryScalarMin` from etg/general.script); the effects radio's high preset (`effectsLevel 0`, the default and the
user's) already sets 256, i.e. the data's own per-effect limits; the hit / damage / fire / bullet toggles are on.
There is no particle budget beyond the per-system counts in the data. Every caller (call_graph.csv: 31 functions,
e.g. `cat_gunCreateBullet`, `univBulletCollidedWithTarget`, `dmgPlayEffect`, `ApplyDamageToCollidingObjects`,
`missileShoot`, `droidUpdateArray`) runs in the universe update; the effect's own level is likewise chosen there from
the owner's `currentLOD`. Lifting the cap (or picking effect level 0) changes which Effect objects the universe
creates and updates, their RANDOM_ETG / particle-stream draws in the update and the ETG create callbacks that run with
them: not render-only, and the cap at the default is Relic's data, not a user limit. So FULL DETAIL does not touch it;
an M3-R2 item (owner: M3-R2 the fix round) if the user wants more effects than the data allows: it needs the ETG
callback audit and its own stream proof.

## (c) The self-tests, switches and harness flags

- **`/catLodTest <tick>[:far][:fleet]`** (HWI-TEST, render.c `catLodTestMainView`, harness `--lodtest SPEC`): in the
  main view's first frame of the tick, after the levels are chosen, one `lodtest: obj type=.. name=.. id=.. band=B
  drawn=D levels=N radius=.. dist=..` line per visible object with a LOD table (B = `currentLOD`, the exe's; D = the
  level drawn), `lodtest: frame ...` and `lodtest: done ... result=PASS|FAIL` (on: every D 0 and at least one object;
  off: every D = B and at least one B > 0). `:far` pulls the camera to its farthest zoom from tick 16, `:fleet` closes
  the sensors manager the headless start opens and focuses the camera on every ship (ai / skirmish / mission draw the
  main view then). Read only (the camera moves, so such a run is no stream reference).
- **`/catLodStats <from>-<to>[:fleet]`** (harness `--lodstats SPEC`): per frame of those ticks `lodstats: tick=.. ms=..
  objects=.. verts=.. polys=..` (the render task's top to after the flip with a glFinish) and `lodstats: done frames=N
  median=.. p99=.. max=.. verts_median=.. size=WxH`.
- **The toggle self-test** (MotionTest.c `mtToggleFullDetail`, in `/catToggleTest`): "toggletest: fulldetail ..."
  lines (start-up save, the Video_Options FULL DETAIL checkbox shown and clicked 0 / 1, Accept writes and reads back
  0 / 1, console 0 / 1, presets low 0 / high 1, `/lodBands` run-only, the start state back). Run plain and with
  `-- /lodBands`.
- **The console self-test** counts the new row (shown 33, set 26); **the options test** (fescreens) has the `noLOD` row
  (options=35, default 1).
- tools/m2_harness.sh header documents `--lodtest`, `--lodstats` and the toggle mode's fulldetail checks (edited while
  no run was in flight).

| run (final binary) | result |
|---|---|
| `m3r1-fd3-lod-ai-on` (ai, `--lodtest 300:fleet`) | 164 ships: band 4 x132, 3 x10, 2 x22; **drawn 0 for all 164**; 25,169 vertices; PASS |
| `m3r1-fd3-lod-ai-bands` (+ `/lodBands`) | the same 164 bands (band unchanged by the setting), **drawn = band** (164 above 0); 5,330 vertices; PASS |
| `m3r1-fd3-lod-m1-on` / `-bands` (missiontest Mission01, `--lodtest 800`, the opening NIS's main view) | 30 objects: 13 asteroids (band 1-3) and 17 ships (band 0-3); on all drawn 0 (2,892 vertices), off drawn = band (24 above 0; 1,791 vertices); PASS / PASS |
| `m3r1-fd3-lod-m5-on` / `-bands` (Mission05, `--lodtest 1500`) | 16 objects (5 asteroids band 2/4, 11 ships band 0-3): on drawn 0, off drawn = band; PASS / PASS |
| `m3r1-fd3-lod-rf-on` / `-bands` (render, `300:fleet`), `-lod-render-on` / `-bands` (`300:far`) | 27 / 1 objects, PASS / PASS |
| `m3r1-fd3-toggle`, `m3r1-fd3-toggle-sw` (`-- /lodBands`) | `toggletest: done start=1/1 fails=0 result=PASS` both (`fulldetail start noLOD=1 switch=0 file=1` / `noLOD=0 switch=1 file=1`) |
| `m3r1-fd3-console` | `consoletest: done ... shown=33 set=26 ... failures=0 result=PASS` |
| `m3r1-fd3-fescreens` | `optionstest: done options=35 startdefaults=35 readback=35 result=PASS`; 22 files, 195 screens pushed / popped, 0 wrong |

Found by the toggle test (fixed): the FULL DETAIL checkbox's callback also runs at the screen's deletion
(FAS_OnDelete, the exe's opNoLOD sets the value from the box then too), so ending `/lodBands` on every non-first call
ended it whenever Video_Options closed; it ends only on a change now.
Not covered by a frame: derelicts, dust / gas clouds and nebulae (none in view in the scenes the harness reaches with
a main view); their draw sites are the same `cat_lodDrawLevelGet` / `drawLOD` lines.

## (e) The fence: streams, dumps, self-tests (debug ASan build unless stated; all on the final binary, label m3r1-fd3-*)

Builds: `tools/build.sh --debug` and `--release` green, 0 warnings under -Werror, after every subsystem;
`tools/m2_layout32.sh` rc 0.

| stream | label | vs | result |
|---|---|---|---|
| headless Mission01 6,000 ticks, pair | `m3r1-fd3-mission-a` / `-b` | r10i-mission-a | **IDENTICAL** in every field (6,001 T records, 1,476,370 B; sha256 61936514c76cc61f: the build identity differs); also IDENTICAL to producer 2's m3r1-aud-tcw-mission-a |
| the same, release | `m3r1-fd3-mission-rel` | r10i-mission-rel-a | IDENTICAL |
| missiontest pair (Mission01, NIS-first) | `-missiontest-a` / `-b` | r10i-missiontest-a | IDENTICAL (6/6, done 11,424) |
| Mission02 .. 17 | `-m2` .. `-m17` | r10i-m2-a .. r10i-m17-a | **all IDENTICAL**, every objective PASS (02 6/6 25,936; 03 5/5 17,825; 04 5/5 20,818; 05 6/6 16,384; 06 7/7 31,011; 07 4/4 16,592; 08 6/6 29,361; 09 6/6 20,617; 10 5/5 21,939; 11 5/5 8,275; 12 3/3 10,995; 13 3/3 19,283; 14 4/4 22,338; 15 3/3 15,424; 16 6/6 21,523; 17 4/4, the game's end 15,584); 02 / 03 / 04 / 09 also IDENTICAL to producer 2's audited runs |
| ai pair (400) | `-ai-a` / `-b` | r10i-ai-a | IDENTICAL |
| ai soak (4,000) | `-ai-soak` | r10i-ai-soak | the other 20 fields IDENTICAL; `rng_all` from 1316 (decision 84: not reproducible under load; the earlier candidate's run `m3r1-fd2-ai-soak` was IDENTICAL in rng_all too); aitest PASS, ASan 0, no fatal |
| skirmishes | `-skirmish-lfe2`, `-skirmish-armed8` | r10i-skirmish-lfe2-a / -armed8 | IDENTICAL |
| build | `-build` | r10i-build-a | IDENTICAL |
| replays 10-17 / 10-18 | `-r17`, `-r18` | r10i-r17-a / r10i-r18-a | IDENTICAL (9,601 ticks / 4,800 packets; 3,527 / 1,763); against GOG's oracle01-rec.hwo with the task's list less rng6 / real: **first divergent ships_vel 2823 and 2871** (unchanged) |
| loadgame, lesson load, Tutorial1, warp start, failed jump | `-loadgame`, `-tload`, `-tutorial`, `-warp10`, `-fj3` | r10i-* | IDENTICAL (Tutorial1 12/12 PASS; fj3 PASS) |
| hwirec playbacks | `-play01`, `-play02` | r10i-play01-a / -play02-a | IDENTICAL |
| oracle self-test | `-oracle` | r10i-oracle | IDENTICAL |

The stream that moved during the work and how it was fixed: on the first final candidate (`m3r1-fd2-m4` / `-m10` /
`-m14`) `rng0` and `rng_all` (only) moved at Mission04 tick 8490, Mission10 16186, Mission14 7169 (Mission04: 540
stream-0 draws ahead). A diagnostic build (logs of the frame-time passes and of the ACV link bolts; removed) showed no
pass and identical bolts, and `/lodBands` gave IDENTICAL, so the draw was in a draw-level site: the Beast-stun edge
lightning (`cat_meshObjectRenderEdgeEffect` 0x004b4a60: a tenth of the drawn object's polygons, each a bolt particle
of `((last - lod + 1) * 6) / last` segments, `cat_partRenderEBoltSystem` drawing stream 0 per segment vertex): the LOD-0
mesh has more polygons and the level more segments. The edge lightning now stays on the exe's level (render.c; with
FULL DETAIL drawing another level it uses that level's mesh with the objects' rest matrices), and the three are
IDENTICAL (`m3r1-fd3-m4` / `-m10` / `-m14`).

**Dumps** (whole sha256 / with the crc= column stripped, first 16 hex):

| dump | R10 | producer 2 audited (`m3r1-aud-tcw-*`) | `/lodBands` (`m3r1-fd3-*-lb`) | default (`m3r1-fd3-*`) |
|---|---|---|---|---|
| motion /noInterp | 183681235a3fdc7b / 3016280e9e0dd3c8 | 183681235a3fdc7b / 3016280e9e0dd3c8 | **183681235a3fdc7b** / 3016280e9e0dd3c8 | c503c869f0dc9cdb / **3016280e9e0dd3c8** |
| motion on | 428fff6c765ab81b / 428ef41479eb5749 | 428fff6c765ab81b / 428ef41479eb5749 | **428fff6c765ab81b** / 428ef41479eb5749 | c0e411662fa2f527 / **428ef41479eb5749** |
| nis /noInterp | 13ca70eea998e5de / 196bab32d244f614 | 2a7fec4de8cfe716 / 196bab32d244f614 | **2a7fec4de8cfe716** / 196bab32d244f614 | 86cb155acffc180e / **196bab32d244f614** |
| nis on | 6a4c1850799187ac / 7ce8472514254f4f | 46df1d391a86a404 / 7ce8472514254f4f | **46df1d391a86a404** / 7ce8472514254f4f | da532dafedb89960 / **7ce8472514254f4f** |

With `/lodBands` every dump EQUALS producer 2's audited one (the nis pair moved from R10's in producer 1's letterbox
round, as its audit recorded). With the default every column but `crc=` is IDENTICAL; the crc= moves are the models
drawn at LOD 0: motion 603 / 605 of 1,431 frames (from frame 0, tick 64), nis 1,260 / 1,267 of 1,440 (from frame 0,
tick 432). Reason: the pixels of the ships drawn with their LOD-0 meshes (no position column moves).

**Self-tests**: `m3r1-fd3-fe` (Main_game_screen at the 120 s timeout, as every fe run), `-hud` (`hudtest: done ...
result=PASS`), `-render` and `-render-lb` (`rendertest: done ... result=PASS`, droids 2,500), `-fescreens` (above):
PASS, ASan 0. **sp** `m3r1-fd3-sp --timeout 1200`: Mission01_NIS01 .. Mission12 (12 animatics skipped), ASan 0, the
same UBSan kinds and counts as producer 2's `m3r1-aud-tcw-sp`. Every fd3 run ASan 0.

## (f) The 4K frame time (GPU measurement)

nvidia-smi utilisation 2 % before each run, 2-3 % after (no game running; the machine's other processes only).
Release build (`--bin build/release/hwi-engine`), ai mode (the 156-ship fleet), `--res 3840x2160`, `--lodstats
150-450:fleet` (the camera on every ship, 164 objects in view), offscreen driver on the RTX 5090:

| run | FULL DETAIL | median ms | p99 ms | max ms | vertices / polygons a frame |
|---|---|---|---|---|---|
| `m3r1-fd3-perf4k-on` | on | 10.466 | 15.039 | 17.293 | 25,169 / 46,255 |
| `m3r1-fd3-perf4k-on2` | on | 9.819 | 14.343 | 15.323 | 25,169 / 46,255 |
| `m3r1-fd3-perf4k-bands` | off (`/lodBands`) | 2.713 | 4.145 | 5.444 | 4,726 / ~5,700 |
| `m3r1-fd3-perf4k-bands2` | off | 3.308 | 4.159 | 5.193 | 4,726 / ~5,700 |
| `m3r1-fd3-perf768-on` (1024x768) | on | 12.524 | 13.808 | 15.064 | 25,169 / 46,255 |
| `m3r1-fd3-perf768-bands` (1024x768) | off | 3.686 | 4.780 | 7.237 | 5,330 |

The cost is the same at 1024x768: the time goes into submitting the vertices (GoK's immediate-mode mesh path), not
into pixels. With 164 ships in view FULL DETAIL draws ~5x the vertices and takes ~3.5x the time: ~95-100 fps median,
~66-70 at the 99th percentile in this proxy, under the 144 Hz target (6.9 ms). PLAN.md's "~47k ship vertices" fleet
would be about twice this scene. Not a correctness issue and the default is the user's decision; the faster mesh
path is an item for the high-refresh-4k work (owner: M3-R2), and the attended 4K hour at 144 Hz is the user's
measurement. `claude-gpu-done` was run after the last measurement.

## Delta-map review entries (ghidra/delta_map_review.json, merged: `python3 -I tools/delta_map.py merge --done m3r1:full-detail`, records changed 4)

- `opNoLOD` 0x004d9d60 (IDENTICAL, stands): compared again; the tree adds only HWI-FEATURE lines.
- `lodLevelGet` 0x004a4d70 (MODIFIED): the bands, the distance branch, no FULL DETAIL reference: as the tree.
- `rndMainViewRenderFunction` 0x005764e0 (MODIFIED): the level loop and its single FULL DETAIL test at 0x00576c03.
- `RenderNAVLights` 0x004c78e0 (MODIFIED): the minLOD test now reads the level drawn under the feature.
No IDENTICAL body in scope was found modified.
[audit: one edited IDENTICAL body had no review row: `nebRenderNebula` 0x004cadb0 (Nebulae.c, the tendril level line); the audit compared it with the exe (nebLOD inlined, the same calls with the same level): IDENTICAL stands, row added and merged (`--done m3r1:full-detail-audit`, records changed 2 with the `lodLevelGet` note's +0x20 -> +8 fix); docs/m2-r1-function-deltas.md regenerated (`tools/delta_map.py report`: the producer's merge had left its done marker stale)]

## CAT-TODOs

None added, none closed; no CAT-TODO or CAT-PROVISIONAL names M3-R1 (`/bin/grep -rn "CAT-TODO(M3-R1\|CAT-PROVISIONAL(M3-R1" src`:
none).

## THE USER'S CHECK (for the play-test brief)

`tools/play.sh` (release; the user's own Cataclysm.cfg already has `noLOD 1`, so nothing to change). New Game, the
Mission01 opening cutscene and the first minutes of Mission01: zoom the camera out (mouse wheel) from the Kuun-Lan
and the fleet to the farthest zoom, then back in; also a skirmish against one computer with the fleet spread out.
Where to look: the ships far from the camera and at the screen's edges, and the asteroids: they should keep their
full shape (no simpler model popping in or out as the zoom changes; previously visible as a sudden change of
silhouette or of the hull's detail). A/B: the same with `tools/play.sh -- /lodBands` (this run only, the saved option
stays). What to report: any model still changing detail with distance in the default run; any visual glitch on a ship
(missing parts, a turret or engine in the wrong place, an engine trail starting off the hull); the frame rate in a
large battle at 4K compared with `/lodBands` (FULL DETAIL costs ~3.5x the render time in the harness' 164-ship
scene); whether the Options -> Video FULL DETAIL box is ticked and the detail slider greyed.

## For STATE.md

- Decision (user's machine): FULL DETAIL = `noLOD` row, **default 1** (the exe 0): every object drawn at LOD 0; the
  high frame-rate preset sets 1, the low 0.
- Design: `currentLOD` stays the exe's level (bands + frame-time passes + DetailQuality) for every non-draw reader;
  `cat_lodDrawLevel` / `cat_lodDrawLevelGet` (LOD.c) are the level drawn.
- Names: switch `/lodBands` (run-only), console row `noLOD` (HWI row id 0x1003), HWI-TEST `/catLodTest`,
  `/catLodStats`, harness `--lodtest`, `--lodstats`.
- Kept on the exe's level under the feature (each a stream draw or a simulation / UI reader): `renderedLODs`, effect
  levels, droid runtime, trail style, ACV link bolts, ship lightning width, selection and status bars, the Beast-stun
  edge lightning (stream 0); the sensors manager's blips stay blips.
- The effects cap: not lifted (Relic's per-effect limits, applied in the universe update); M3-R2 if wanted.
- The 4K cost: FULL DETAIL ~10 ms median / ~15 ms p99 at 164 ships (offscreen release), vertex-bound: M3-R2
  (high-refresh-4k).

## What is left

- The faster mesh submission for FULL DETAIL at 144 Hz (owner M3-R2, with docs/features/high-refresh-4k.md).
- The effects cap / effect level 0 under FULL DETAIL (owner M3-R2; needs the ETG callback audit and a stream proof).
- Trails at LOD 0 style (their TRAILS-stream draws follow the level: would move rng_all; the user's call).
