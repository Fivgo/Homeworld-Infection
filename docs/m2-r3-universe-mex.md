<!-- done: levelload, mex-dock, statics, universe, globals, final -->
# M2-R3 producer 5: universe statics + MEX

Status: DONE (2026-10-10); DONE WHEN met (`um-final-sp`). Not committed. Branch `m2`, worktree `.claude/worktrees/m2`. Scope: docs/m2-r2-summary.md
"Plan for M2-R3 and M2-R4", row 5: `Universe.c` (mine statics over 7 races, `universeDefaultTeamColors`, the derelict
loader over the 8-type table), `MEX.c` (`mexLoad`, `mexSetCollisionInfo`, `collrectmid`), the `DockStaticInfo` 0x50
header and `scriptSetDockStatics`, `Globals.c`; HW1-matched 38 / 31 / 0 (15,684 MODIFIED bytes) and 20 cat-new
functions (5,549 B) by the plan. DONE WHEN: Mission01 level load completes and sp reaches "universe running"
(`univUpdateCounter > 0`) on the plain harness with ASan 0.

Subsystems (the done marker above lists the finished ones):

1. `levelload`: the level-load stops between ships-a's last stop (`LevelLoad.c (860)`) and "universe running".
2. `mex-dock`: `MEX.c`, the `DockStaticInfo` header, `scriptSetDockStatics`, `cat_dockFindDockStaticPoint`.
3. `statics`: the static loaders and closers of `Universe.c`.
4. `universe`: the rest of `Universe.c` (init/reset/close, team colours, unit caps, game stats, cat-new helpers).
5. `globals`: `Globals.c`.
6. `final`: delta-map review entries, builds, plain-harness runs.

Method: the r5 decompile (`ghidra/exports/cataclysm_gog_r5/decompiled.c`) read function by function against the tree;
tables and constants read from cataclysm.exe. Scratch helpers in `/tmp/m2r3um/` (outside git): `fn2.py` (exe body),
`grepfn.py`, `showfn.py`, `fl.py`, `dumptab.py` (copied from ships-a), `tab32.py` (u32/u16/float table dump),
`scope.py` (this item's HW1-matched and cat-new functions by `tools/m2_stage_counts.py`'s neighbour rule: today 37
IDENTICAL / 33 MODIFIED HW1-matched, 20 cat-new / 5,549 B).

## 1. `levelload`: from `LevelLoad.c (860)` to "universe running"

| file | change | exe evidence |
|---|---|---|
| `src/Game/LevelLoad.c` | `GetAppropriateShipTypeForRace`: a type the race may not own is swapped through the exe's 60-entry Sect <-> Beast table (new `catAppropriateShipTypeSwap`, e.g. sMimic -> bCloakedFighter, bInterceptor -> sAcolyte, sDestroyer -> bIonArrayFrigate); no race assertion, no check of the substitute. The exe reads past its table (string data) for a type >= 60; the port returns such a type unchanged. This was the sp stop `LevelLoad.c (860)` | `GetAppropriateShipTypeForRace` 0x004a13c0; table 0x00883cf8 (60 ShipType values, dumped with `tab32.py`; entry 60 onwards is string data) |
| `src/Game/LevelLoad.c` | `scriptSetShipsCB`: the race override (player's race replaces a Sect/Beast ship's race) runs only when `!singlePlayerGame` (HW1: every mission-manager level); the player's mothership is any of sMothership, bMothership, kCarrier, tCarrier, trFloatingCity (HW1: `Mothership`) | `scriptSetShipsCB` 0x004a13f0: `singlePlayerGame == 0 && missionman && ...`; types 0x12, 0x35, 0x3E, 0x4C, 0x62 |
| `src/Game/SinglePlayer.h`, `src/Game/SinglePlayer.c` | **CAT-PROVISIONAL (stage 6)**, switch `CAT_HW1_MISSION_SCRIPTS` 0: `singlePlayerKasMissionStart` starts no mission script, and `singlePlayerStartGame` plays no mission-start NIS. Reason: the tree's mission scripts are HW1's compiled Kharak..Hiigara KAS; they name HW1 teams that Cataclysm's `.mission` files do not define (Mission01: `KAS.c (265)` "unresolved reference to team TDFormationTrial", run `um-r1-sp`), and HW1's race-suffixed `nis\n01r1.nis` does not exist in Cataclysm's data (`File.c (1120)` fatal, run `um-x1-sp-nokas`). `kasExecute` does nothing while no mission watch function is set. Stage 6 replaces both with Cataclysm's kas2c missions and its NIS-first start, then deletes the switch | Cataclysm's own Mission01 script: `Init_Mission01_Objectives` 0x005d6fe0 and 84 more (16,378 B); `singlePlayerStartGame` 0x0051bdb0 plays `nis\n01.nis` / `nis\n01.script` and `singlePlayerStartNis` 0x0051b670 loads the level (`singlePlayerLoadNewLevel`) |

Runs (plain harness, debug ASan build, offscreen, no input):

| run | change in the tree | rc / wall | milestone | first stop | ASan |
|---|---|---|---|---|---|
| `um-r0-sp` | none (ships-a's tree) | 1 | level load (mission file read) | `Game/LevelLoad.c (860)` | 0 |
| `um-r1-sp` | `GetAppropriateShipTypeForRace` + `scriptSetShipsCB` | 1 / 10.2 s | level load; `levelInit` done (its last step `TryToFindMothershipsForPlayers` logged) | `Game/KAS.c (265)`: KAS unresolved reference to team TDFormationTrial (HW1 Mission01 script) | 0 |
| `um-x1-sp-nokas` (experiment, temporary edit, reverted) | + no HW1 KAS | 1 | level load | `File.c (1120)`: `nis/n01r1.nis` not found | 0 |
| `um-x2-sp-nokas-nonis`, `um-x3-...-300` (experiment) | + no HW1 NIS | 137 / 65 s; 124 / 300 s | 60 s: still in `Movies/Animatic01.bik`; 300 s: universe running, 688 ticks | none | 0 |
| **`um-r2-sp`** | the provisional switch in the tree | 124 / 330 s (timeout) | **universe running (univUpdateCounter > 0)**: 1,136 ticks | none: still running at the timeout, clean shutdown (no quit assert) | **0**, no leak |

Two harness facts for the integrate step: (1) the sp start plays Cataclysm's intro animatic `Movies/Animatic01.bik` in
real time (about 246 s, `aviPlayLoop: play_time=245933`) before the universe starts, so the sp run needs `--timeout`
of about 300 s or more (the 60 s default ends inside the animatic, and a SIGTERM during `aviPlay` is not handled, rc
137); (2) after 816 ticks GoK's `/debugScreenshots` hook calls `singlePlayerNextLevel()` every frame (45,833 lines in
`um-r2-sp`) because Cataclysm's hyperspace/next-level flow is not ported (stage 6); the universe keeps running.

## 2. `mex-dock`: `MEX.c`, the `DockStaticInfo` header, `scriptSetDockStatics`

| file | change | exe evidence |
|---|---|---|
| `src/Game/SpaceObj.h` | `DockStaticInfo` gets Cataclysm's 0x50-byte header: `firstOfType[35]` (+0x00, 0xFF = none), `numOfType[35]` (+0x23), `numDockPoints` (+0x48), `centerDockPoint` (+0x4C); the points follow at +0x50 (0x4C each). ILP32 pins (`DockStaticPoint` 0x4C, `name` +0x38, `headingdirection` +0x30, the header offsets). `sizeofDockStaticInfo` uses `sizeof`, so the allocation follows | `scriptSetDockStatics` 0x005270c0 (`0x4C * n + 0x50`, n at +0x48, the type index at +0/+0x23); `InitStatShipInfo` 0x0053bab0 stores "center" at +0x4C |
| `src/Game/StatScript.c` | `scriptSetDockStatics`: after the read the points are indexed by type (`firstOfType`, `numOfType`; type -1 skipped) once any point was read | `scriptSetDockStatics` 0x005270c0 tail |
| `src/Game/Dock.c`, `Dock.h` | new `cat_dockFindDockStaticPoint`: case-insensitive name search returning the point or NULL (not fatal, unlike `dockFindDockIndex`) | `cat_dockFindDockStaticPoint` 0x00463d00 (`_stricmp` 0x00676c90) |
| `src/Game/Salvage.c` | `cat_salvageFindDockStaticPoint` (ships-a) calls the Dock.c function instead of its own copy | same |
| `src/Game/Universe.c` | `InitStatShipInfo`: after `mexGetDockStaticInfo`, `dockStaticInfo->centerDockPoint = cat_dockFindDockStaticPoint("center", ...)` | `InitStatShipInfo` 0x0053bab0 |
| `src/Game/MEX.c` | `mexGetDockChunk`: a `Sal`/`SALVAGE` chunk of that name also matches (Cataclysm's salvage points are dock points) | `mexGetDockChunk` 0x004b7190 |
| `src/Game/MEX.c` | `mexGetDockStaticInfo`: a point without a chunk gets position 0, cone normal (1,0,0), cone `cos(1.5707963705062866)` (HW1 left zeros) | `mexGetDockStaticInfo` 0x004b7560, double 0x0072af58 |
| `src/Game/MEX.c` | `mexSetCollisionInfo`: the sphere radius is scaled by the largest of the three box scales; the avoidance sphere is `r + 3.0`, the padded one untouched; the box offset is taken as stored (the slides move only the sphere); the diagonal sums `up^2 + (right^2 + forward^2)` in that order; `collrectmid = (collrectoffset + length) * 0.5` per axis in both branches | `mexSetCollisionInfo` 0x004b7880 (3.0 at 0x0072a534, 0.5 at 0x0072a4c8, `(float)1.1` at 0x0072af60) |
| `src/Game/MEX.c`, `MEX.h`, `SpaceObj.h` | `mexSetEngineNozzle(ShipStaticInfo *, mex, index)`: the nozzle goes to `engineNozzleOffset[index]` (+0x2B0; M2-R2 had +0x2C8) and, per LOD level, `engineNozzleObject[level][index]` (+0x2E0) holds the index of the mesh object the Eng chunk names (0xFF = none). `MEXEngineChunk` gains `objectName[32]` at +0x30 (85 of the install's 463 Eng chunks name one) | `mexSetEngineNozzle` 0x004b7c20; `trailGetNozzleOffset` 0x005a4cc0 reads both through `cat_shipNozzleInfoGet` 0x0053f8c0 |
| `src/Game/MEX.c`, `MEX.h`, `SpaceObj.h`, `Universe.c` | `mexGetNAVLightStaticInfo(NAVLightStaticInfo *, StaticInfo *header)`: takes the static header (the exe reads the mex at header+4 and the LOD at header+0) and fills, per LOD level, `navlightObject[level]` with the index of the mesh object the Nav chunk names. `MEXNAVLightChunk` gains `objectName[32]` at +0x34 (5 of 2,951 Nav chunks name one: sleech "LeechHead"). `NAVLightStatic` is now 0x48 bytes like the exe (M2-R2 had left the two extra dwords unidentified); ILP32 pins on the size, `name` +0x28, `navlightObject` +0x40. Both callers (ship, derelict) pass `&statinfo->common` | `mexGetNAVLightStaticInfo` 0x004b7680 (stride 0x48, bytes at +0x40 + level); callers `InitStatShipInfo` 0x0053bab0 and `universeStaticInit` 0x0053d2a0 pass the static |

Checked and left as they are: `mexLoad` 0x004b70e0 (inlines `mexVerify`'s `_stricmp(..., "mannngo")`),
`mexGetResNozzleStatic` 0x004b7240 and `mexGetRepairNozzleStatic` 0x004b7330 (inlined `mexGetChunk`/`mexVecToVec`,
same result), `mexGetGunStaticInfo` 0x004b73c0 (the exe passes the static header and reads the mex at +4; same result,
the tree keeps the mex argument). Port caps where the exe reads unchecked: the LOD walk of both object tables stops at
the table size (6 nozzle rows, 8 nav bytes), and a level without mesh or bindings counts as "none".

## 3. `statics`: the static loaders and closers of `Universe.c`

| file | change | exe evidence |
|---|---|---|
| `src/Game/Universe.c` | the thrust product of the guidance precalc follows each exe site's order: `modifier * thrust * mass` (`precalcStaticInfoGuidance`, used for derelicts), `mass * thrust * modifier` (ships, inline), `thrust * mass * modifier` (missiles, mines, inline); new static `catPrecalcStaticInfoGuidanceOrder`. M2-R2 had put the ship order into the shared function | `precalcStaticInfoGuidance` 0x0053ad70, `InitStatDerelictInfoByPath` 0x0053b2a0, `InitStatShipInfo` 0x0053bab0 (0x0053c05d), `InitStatMissileInfo` 0x0053b400, `InitStatMineInfo` 0x0053b660 |
| `src/Game/Universe.c`, `MeshAnim.h` | new keys `NUMBER_OF_ANIMS n` (loads the ship's .mad like BindAnimations and allocates an n-entry table, all -1) and `ANIM i name` (`madAnimIndexFindByName`) in `MadMaxMadInfoLoad`; `madstatic` gains `numAnims` +0x84, `animIndexes` +0x88, `animsByNumber` +0x8C (0x90 bytes, ILP32 pin). 18 .shp files use them (e.g. `bacolyte.shp`); none of them also uses BindAnimations or a Mad* key | table 0x008b2f88 rows 5-6; `cat_madNumberOfAnimsSetCB` 0x00539e40, `setMadAnim` 0x00539f70 ("%i", "%i %s") |
| `src/Game/Universe.c` | `CloseStatShipInfo`: clears `madStatic` and `dockOverideInfo` after freeing them; frees `animIndexes` (**CAT-FIX**: the exe leaks it) | `CloseStatShipInfo` 0x0053c500 |
| `src/Game/SpaceObj.h`, `StatScript.c/.h`, `Universe.c` | asteroids: new `cat_scriptAsteroidBreakInfoLoad` (NUMBER_OF_ASTEROID_PIECES n, then n `AsteroidBreakInfo` lines) and `cat_scriptAsteroidPieceParse` (type, position, orientation RotX(a)*RotY(b)*RotY(b), four values; the exe never uses the 6th value), called by `InitStatAsteroidInfo` after the key table. `AsteroidStaticInfo` gets `numBreakPieces` +0xC0 and `breakPieces` +0xC4 (exe size 0xC8, ILP32 pins); new `AsteroidBreakPiece` (0x44). HW1's shrink/break keys are gone from the key table (the exe's has none; no Cataclysm file uses them); the HW1 fields stay after the exe layout as CAT-DORMANT for `ResCollect.c` until stage 2B | `cat_scriptAsteroidBreakInfoLoad` 0x00528190, `cat_scriptAsteroidPieceParse` 0x005280a0 (disassembly 0x005280a0..0x0052818f: two `matMakeRotAboutY` 0x004ad470 calls with the 5th value), table 0x00897930, key table 0x008b3008 |
| `src/Game/SpaceObj.h`, `Universe.c` | mines: key `N-LIPS` sets the new `MissileStaticInfo.scaleCap` (+0x110; 0.0007 in four races' mine.shp, 0.0001 in race_derelict's). ILP32 pins: MissileStaticInfo 0x12C, `trailStatic` +0x120; DerelictStaticInfo 0x124 | mine key table 0x008b3b88 row 37; `InitStatMissileInfo` 0x0053b400 stores trailStatic at +0x120 |
| `src/Game/Universe.c` | `universeStaticInit`: nebulae no longer needed are not closed; mines of all 7 races (HW1: 2), and the Derelict race's mine is loaded whether needed or not; after the load every loaded ship static gets `cat_univSetShipMaxHealth`; no Mothership-immobile step | `universeStaticInit` 0x0053d2a0 (mine loop to 0x00b3b444 stride 300, the 0x00b3b318 test; health loop after 0x0053dc5e) |
| `src/Game/Universe.c`, `SpaceObj.h` | `InitStatShipInfoPost`: the zero test and `1/maxhealth` run whatever `GLOBAL_SHIP_HEALTH_MODIFIER` is, and the result goes to the new `maxhealthBase` (+0x44C, M2-R2's `unknown44C`) | `universeStaticInit` 0x0053d2a0 (inline) |
| `src/Game/Universe.c/.h`, `SinglePlayer.c/.h` | new `cat_univSetShipMaxHealth`: `maxhealth = maxhealthBase`, times `spDifficultyHealthModifier` for race Sect in a single-player game; new global `spDifficultyHealthModifier = 1.0f` (the exe's image value; the difficulty screen that sets it is stage 6) | `cat_univSetShipMaxHealth` 0x0053c4a0, 0x00894194, `cat_spGetDifficultyModifiers` 0x00516f90 (6, 3, 1, 0.85, 0.7) |

Checked and left as they are: `InitStatShipInfo` (M2-R2 had ported the defaults, the inlined strike-craft list, the
build costs and repair defaults; the exe's inline gun-binding loop is `gunSetStaticGunInfoFromBindings`),
`isStrikeCraft` (the 27 types match), `InitStatDerelictInfoByPath`, `InitStatHyperspaceGateInfo`,
`InitStatDustCloudInfo`, `InitStatNebulaInfo`, `CloseStatInfo`, `setAnimationBindInfo` (the 0x90-byte madstatic now
follows from `sizeof`); the asteroid, derelict, missile, dust-cloud and nebula key tables match the exe's names,
callbacks and offsets (`scriptSetUbyteCB` and `scriptSetSbyteCB` are one ICF-folded body in the exe). The mine
table's three `collBoxScale*` rows keep HW1's wrong base (`asteroidStaticInfos`), as the exe does; no mine.shp sets
them.

## 4. `universe`: the rest of `Universe.c`

| file | change | exe evidence |
|---|---|---|
| `src/Game/Universe.c` | `universeDefaultTeamColors`: the colour schemes of the non-player races come from the game: the tutorial, multiplayer, and each single-player mission 1..17 has its own set (decoded from the exe's [7][8] stack table); HW1's fixed Traders/P1/P2/P3 rows are gone. Mines of every race get their row (HW1: R1/R2), derelicts take the Derelict race's row (HW1: Traders') [audit: the code set the four entries shared by missions 8..17 for any mission >= 8, so also above 17, where the exe's default case sets nothing; fixed to 8..17] | `universeDefaultTeamColors` 0x0053c940 (row r at stack -0x38 + 8r; the copy loops to 0x00b3b510 + type * 0x458, 0x00ba1030/0x00b3acd0 + race * 300, 0x00ba0710 + n * 0x124) |
| `src/Game/Universe.c` | `universeClosePlayers`: the current player's research, construction and launch managers close (`rmCloseIfOpen`, `cmCloseIfOpen`, `lmCloseIfOpen`); `universeReset` and `universeClose` reach it through `universeClosePlayers`, as the exe inlines the same loop | `universeClosePlayers` 0x0053def0, `universeReset` 0x0053df80, `universeClose` 0x0053e3b0 |
| `src/Game/Universe.c` | `universeUpdateTask`: the free-running branch runs one update per pass (no `universeTurbo` repeat; Cataclysm's time compression is `cat_taskTimeCompressionApply` 0x0052a370, not ported); turbo playback repeats 9 times (Relic HW1's `UNIVERSE_TURBO_REPEAT 19 >> 1`; GoK's `(turboTimeCompressionFactor-1)>>1` gave 3); a playback pass that finds no packet sets the new `universePlaybackNoPacket`, cleared each pass. The oracle hook H1 now wraps the exe's 3 `univUpdate` call sites (GoK had 4; docs/m2-r2-oracle.md still says 4) | `universeUpdateTask` 0x0053e460 (`repeattimes = 9`; 0x00a6c5c8 set at 0x0053e8f9, read by `cat_moRender` 0x004aafb0); docs/m2-checksum-oracle.md section 2 |
| `src/Game/Universe.c/.h` | `unitCapCreateShip`: no exempt types (HW1: Drone, TargetDrone, HeadShotAsteroid, CryoTray); a Somtaaw mothership counts its starting modules (new `cat_univMothershipModuleTypesGet`: tutorial, missions 1-2, mission 3, maps DemoBigGun2 and DemoSect2, default) and a Beast mothership its two (new `cat_univBeastMothershipModuleTypesGet`: none in missions 1-3), each as a ship of its type and in `classtotals[CLASS_Component]` (Player+0x29c is `classtotals[10]`, since `unitCapInitialize` clears 12 classes) [audit: for DemoSect2 the exe writes 11 types but stores the count 12 (`mov [list+0x50],0xc` at 0x0053f81c), so it counts a never-written 12th entry of the caller's uninitialised stack array; the port counts 11, now marked CAT-FIX in the code]; `unitCapDeleteShip` loses the same exemptions | `unitCapCreateShip` 0x0053eab0, `cat_univMothershipModuleTypesGet` 0x0053f680, `cat_univBeastMothershipModuleTypesGet` 0x0053f840, `unitCapDeleteShip` 0x0053ec00, `unitCapInitialize` 0x0053efd0 |
| `src/Game/Universe.c` | `universeSaveEverythingNeeded` / `universeLoadEverythingNeeded`: 7 mines (HW1 2); `universeNoStaticLoadedOrNeeded`: clears the mines of races 0..3 only | 0x0053ce80 (mine loop to 0x00b3b504), 0x0053d090 (to 0x00b3b444), 0x0053dd90 (offsets 0, 300, 600, 900) |
| `src/Game/Universe.c` | `gameStatsAddShip`: no `totalShipsInGame++`; `gameStatsShipDied`: no Drone exception, the killer is any index below 8 other than the owner (signed), and the per-class kill and loss counters skip sSwarmer/bSwarmer and count sACV, sMCV and bACV twice | `gameStatsAddShip` 0x0053f220 (disassembly 0x0053f220..0x0053f28e), `gameStatsShipDied` 0x0053f290 |
| `src/Game/Universe.c/.h` | new `cat_univFindNearestShip(from, filter)` for the Beast weapon (M2-R4) | `cat_univFindNearestShip` 0x0053f590 |

Checked and left as they are: `universeInit` (the exe also clears each player's fog-of-war pointer at Player+0x6500,
which needs the Cataclysm Player layout; see "left"), `universeStaticCloseAllStatics` (inlined closes, the same
loops; the exe closes only 2 mines too), `GetShipStaticInfo`/`GetShipStaticInfoSafe` (M2-R2), the needed-flag scans,
`precalcStaticHeader`, `scriptSetFramesCB` (`<< 4`), `unitCapInitStatics`, `resourceInjectionTask`, `resourceLumpSum`,
`gameStatsCalcShipCostTotals` (the same sum; the exe inlines `GetShipStaticInfoSafe`).

## 5. `globals`: `Globals.c`

| file | change | exe evidence |
|---|---|---|
| `src/Game/Globals.c` | `globalsResetFunc` (the exe's `globalsInit`): also resets `universeTurbo`; no longer clears the command-message list `gMessage` (the exe clears the two big messages and `playersReadyToGo` only) | `globalsInit` 0x0048a060 (0x00a6c5b0 = 0; the 2 x 0x104 loop at 0x00c39364 is `bMessage`) |
| `src/Game/Globals.h` | `MAX_BIGMESSAGE_LENGTH` 256 (HW1 150) | `bigmessageDisplay` 0x00567cf0 (stride 0x104 from 0x00c39360) |

Kept: GoK's `profInit`/`profReset`/`profClose` (profiling timers; the exe has none, they touch no game state).
`globalsClose` 0x0048a1a0 otherwise matches.

## 6. `final`: alias retirement, delta-map reviews, builds, runs

**HW1 aliases retired** in the files ported here (`Universe.c/.h`, `MEX.c/.h`, `Globals.c/.h`; a scan of the code
outside comments and strings finds none left): `gameStatsGetShipClass` (GoK's debug game-stats log check, absent from
the exe) takes the class from the loaded ship static instead of a table of 61 HW1 type names, including the dormant
ids 200-221; `unitCapCreateShip`/`unitCapDeleteShip` lost the Drone/TargetDrone/HeadShotAsteroid/CryoTray tests;
`unitCapCanCreateShip` the ResearchShip test; `gameStatsShipDied` the Drone test; `GetValidRaceForShipType` returns
`RACE_Sect` for `R1`.

**One fix outside the scope's files**, needed by the new ANIM key: `MeshAnim.c` `madAnimIndexFindByName` returns -1
for a missing name instead of `dbgFatalf` (the exe's 0x004b6ed0 calls only `_stricmp`; disassembly
0x004b6ed0..0x004b6f11). The statics self-test `um-r4-statics` had stopped at `MeshAnim.c (707)`: "Unable to find
animation 'Link'" (`dAcolyte.shp`'s `ANIM 2 Link` and `ANIM 3 Unlink`; `um-r5-statics` now logs the two setMadAnim warnings).

**Delta-map review entries** (`ghidra/delta_map.json`, 43 rows, `review_note` "M2-R3 universe-mex: ..."):
- IDENTICAL in the map, modified in the exe (class set to MODIFIED, change text added): `mexGetNAVLightStaticInfo`,
  `unitCapDeleteShip`, `gameStatsAddShip`, `gameStatsShipDied`, `universeSaveEverythingNeeded`,
  `universeLoadEverythingNeeded`, `universeNoStaticLoadedOrNeeded`, `initGameStats` (PlayerStats layout only, no src
  edit; M2-R4 #5); outside the scope `madAnimIndexFindByName` (already MODIFIED; note added).
- `globalsClose`: identical but for GoK's `profClose`.
- the 33 MODIFIED rows of the scope, each marked ported (with the change) or read with "same behaviour" / "not
  ported, owner".

Scope after the review: HW1-matched 29 IDENTICAL / 41 MODIFIED (19,484 B) by `scope.py` (the plan's estimate was
38 / 31); cat-new 20 / 5,549 B, of which this item ports `cat_univMothershipModuleTypesGet`,
`cat_univBeastMothershipModuleTypesGet`, `cat_univFindNearestShip`, `cat_madNumberOfAnimsSetCB`, `setMadAnim` and (via
`universeStaticInit`) `cat_univSetShipMaxHealth`; `cat_gunCanFire`, `cat_scriptSetBeastAttackConvertToCB`,
`cat_univInitShipTypeFlags` and `cat_shipStealthDetectedSet` were already in the tree. Plus, outside the neighbour
rule, `cat_dockFindDockStaticPoint`, `cat_scriptAsteroidBreakInfoLoad`, `cat_scriptAsteroidPieceParse`.

**Builds**: `tools/build.sh --debug` and `--release`: built, 0 warnings under `-Werror`, after every subsystem and at
the end. `tools/m2_layout32.sh`: OK (new pins: DockStaticInfo header, DockStaticPoint, NAVLightStatic 0x48,
madstatic 0x90, AsteroidStaticInfo / AsteroidBreakPiece, DerelictStaticInfo 0x124, MissileStaticInfo 0x12C,
ShipStaticInfo nozzle fields).

**Plain-harness runs** (debug ASan build, offscreen, no input; `original/scratch-m2/m2r2-harness/runs/<label>`):

| run | mode | rc / wall | milestone | first stop | sanitizers / checks |
|---|---|---|---|---|---|
| `um-r3-sp` (after statics, universe, globals) | sp | 124 / 330 s | universe running, 1,136 ticks | none (timeout) | ASan 0, no leak; dock-point warnings 28 -> 1 (the Sal chunks) |
| `um-r4-sp` | sp | 124 / 340 s | universe running | none (timeout) | ASan 0, no leak |
| `um-r4-statics` | statics | 1 / 11 s | Main_game_screen | `MeshAnim.c (707)` "Unable to find animation 'Link'" | ASan 0; fixed above |
| **`um-final-sp`** | sp | 124 / 340 s | **universe running (univUpdateCounter > 0)**: 1,248 ticks logged by `/debugScreenshots` | none: still running at the timeout | **ASan 0**, no unsuppressed leak; 3,713 file loads |
| **`um-final-fe`** | fe | 124 / 60 s | `Main_game_screen` | none | ASan 0, no leak; 232 file loads (as ships-a's final fe) |
| **`um-r5-statics`** | statics | 0 / 11 s | ship statics self-test done | none | ASan 0, no leak; `shipstatic_engine_check.py` against `game_extracted/cataclysm`: **143 / 143** ship-type files as the probe (OK 140 + OK-DECLARED-COUNT 3), OTHER-LOADED 27 (170/173), 2 types without a file (as ships-a) |
| **`um-r5-statics-init`** | statics + `/catShipStaticsStaticInit` | 0 / 11 s | self-test done | none | ASan 0, no leak; 56 StaticInits ran (as ships-a) |
| **`um-final-oracle`** + `tools/oracle/selftest.sh` (`um-final-oracle-st-*`, release) | oracle | 0 / 10 s | oracle self-test done | none | ASan 0; **SELFTEST PASS** (A/B identical, perturbed run tick 300, perturbed copy tick 400 `ships_pos`) |
| **`um-final-etg`** | etg | 0 / 12 s | etg effect self-test done | none | ASan 0, no leak; 618 effects created, 128 ticks |

**DONE WHEN**: Mission01 level load completes and sp reaches "universe running" with ASan 0 on the plain harness:
**met** (`um-final-sp`; first met in `um-r2-sp` after `levelload`). It holds with the CAT-PROVISIONAL switch
`CAT_HW1_MISSION_SCRIPTS 0` (section 1): no HW1 mission script and no mission-start NIS run until stage 6 ports
Cataclysm's own; the integrate step should judge the sp run with `--timeout` of about 300 s or more (the intro
animatic plays in real time first).

## What is left, and its owner

1. **Mission scripts and the NIS start** (the `CAT_HW1_MISSION_SCRIPTS` switch, section 1): Cataclysm's kas2c
   Mission01 script and `singlePlayerStartGame`'s NIS-first start; then delete the switch. Owner: stage 6
   (single player).
2. **The next-level flow**: after ~816 ticks GoK's `/debugScreenshots` calls `singlePlayerNextLevel()` every frame
   because Cataclysm's hyperspace/next-level code is not ported; the universe keeps running. Owner: stage 6.
3. **`unitCapCanCreateShip`** beyond the gating (the CAT-TODO in Universe.c): needs Cataclysm's 0x30-byte
   `shipinprogress` and `cat_carrierCountSupportModules`. Owner: M2-R4 (construction manager; with #5's Player layout).
4. **Player layout and FOW**: `universeInit`'s per-player fog-of-war pointer clear (Player+0x6500), `universeReset`'s
   `fowDestroy`, `universeSwitchToPlayer`'s `fowSelect`. Owner: M2-R4 #5 (Player layout) and stage 3 (FOW).
5. **Resets of unported subsystems** listed as CAT-TODO in `universeReset`/`universeClose`/`universeClosePlayers`:
   `cat_salvageResetGlobals`, `cat_blastReset` (M2-R4 #3), `cat_playerUpgradesReset` + `cat_spWarpToLevelEnabled`
   (M2-R4 research / stage 6), `cat_mcSetMode`, `cat_ioReset`, `cat_shipPrintReset`, `cat_tutPointerListReset` (stage 3
   / 6), `cat_taskTimeCompressionReset` and the time-compression task rate that replaces `universeTurbo` (M2-R4 #5,
   Task.c); `globalsInit`'s two recorder globals 0x00a7519c / 0x00a751a0 (recorder).
6. **cat-new helpers of this neighbourhood not ported**: `cat_shipNozzleInfoGet`, `cat_meshObjectMatrixGet`,
   `cat_meshObjectMatrixConcat` (they read the per-ship mesh state at Ship+0x4B8, the 0x58-byte binding records;
   owner M2-R4 #1, which ports `cat_univShipMeshStateCreate`, and stage 3 trails), `cat_madAnimationFinished`
   (Ship+0x4BC Cataclysm madanim; M2-R4 #4 ship code), `cat_playerHasCompletedShip` (a Ship flag at +0x1CC not yet
   identified; M2-R4 #2/#5), the five `cat_gameStats*` screen functions (3,018 B; stage 3 UI).
7. **Readers of the new data**: the asteroid break pieces (`ResCollect.c`, whose HW1 shrink/break fields are
   CAT-DORMANT; M2-R4 #3), `MissileStaticInfo.scaleCap` for mines, `madstatic.animIndexes`, `navlightObject` /
   `engineNozzleObject` (render, stage 3), `universePlaybackNoPacket` (`cat_moRender`, stage 3).
8. **The single-player difficulty**: `spDifficultyHealthModifier` stays 1.0 until the difficulty screen and its save
   are ported (stage 6); the other two difficulty values (0x00894198, 0x0089419c) are not ported.
9. **GameStats layout** (`initGameStats`: a fourth per-class array): M2-R4 #5.
10. docs/m2-r2-oracle.md still says hook H1 wraps 4 `univUpdate` call sites; there are 3 now (as in the exe). [audit: annotated in docs/m2-r2-oracle.md]
11. [audit] `scriptSetShipsCB` is only partly ported: the tree keeps HW1's CryoTray swap and the multiplayer MG_HarvestinEnabled / MG_ResearchEnabled / MG_CarrierIsMothership gates, which the exe lacks, and the research-module / kasHashString / clFormation deltas are open (delta-map review note added). Owner: M2-R4 (level load, multiplayer).
