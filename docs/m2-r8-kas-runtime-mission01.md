<!-- done: kas-runtime, kas2c-generator, level-load, nis-first-start, mission-selftest, shuffle-verification -->
# M2-R8 producer 2: the KAS runtime, the kas2c dialect, Mission01 + Tutorial1, the NIS-first start, level load

Base commit `7d225de` (M2-R8 kasf-library, audited). The work is `git diff 7d225de` in the m2 worktree plus the new
files (`src/Missions/{CatMissions.h, KasShared.c, SinglePlayer/Mission01.c, Tutorials/Tutorial1.c}`,
`src/Game/MissionSelfTest.{c,h}`, `tools/m2_kas2c_gen.py`, this report). No Ghidra run; the exe was read through the
named export `cataclysm_gog_r6` (decompile, functions, symbols) and `objdump` of `original/game/cataclysm.exe`
(tools/m2_fdec.py, tools/m2_fdis.py, tools/m2_exedata.py).

Subsystems (the done marker above):

1. **kas-runtime**: KAS.c (the unread bodies, `cat_kasShipRemovedFromSelections`, the save format, the function
   converters), HS.c's static-gate tag, Timer.c (all rows already reviewed by producer 3).
2. **kas2c-generator**: `tools/m2_kas2c_gen.py` and the generated Mission01 / Tutorial1 code.
3. **level-load**: LevelLoad.c (`scriptSetShipsToBeNeeded`, `levelPreInit`, the meteor storm, the resource adders, the
   IDENTICAL bodies compared), the nebula records of a cataclysm.exe save, the mission-number mapping.
4. **nis-first-start**: SinglePlayer.c's start, level change, NIS start / stop and the mission flow's entry; Tutor.c;
   `CAT_HW1_MISSION_SCRIPTS` retired.
5. **mission-selftest**: `/catMissionTest`, `/catMissionTestTutorial`, harness mode `missiontest`.
6. **shuffle-verification**: the start-position shuffle (the one move of the Mission01 stream), builds, runs, reviews.

## 1. The KAS runtime (KAS.c, HS.c)

- **`cat_kasShipRemovedFromSelections` 0x004911e0** (Cataclysm-only, the unfolded twin of `kasShipDied`): every KAS
  selection drops a deleted resource or missile. Called where the exe calls it: HW1's `DeleteDeadResource` (inlined by
  the exe in `univReallyDeleteThisResourceRightNow` 0x005490d0 at 0x005491c3, `univDeleteDeadResources` 0x00549270 at
  0x005493ac and `univUpdate` 0x0054c3f0 at 0x0054cc49), `cat_univDeleteMimicResource` 0x00548f40 (0x00549083) and
  `DeleteDeadMissile` 0x00549880 (0x00549a2a). Closes the three `CAT-TODO(stage 6)` of UnivUpdate.c.
- **`kasLabelledEntitiesDestroy` 0x00490f90**: also clears the hashed variable store (`cat_kasVarDestroyAll`
  0x00491ae0) after `aivarShutdown` (= `aivarDestroyAll`, folded).
- **`kasSave` 0x00491220 / `kasLoad` 0x00491450**: the skill slot is the single-player difficulty (0x008941a0); the
  scope, scope hash, name and the watch function's script index are read back as saved (GoK threw the scope away); a
  single-player save ends with `rmCanBuildShip(player 0, type, FALSE)` of sMothershipArmour, sMothershipDockingBay,
  sMothershipMicro, sMothershipSpecial, sMothershipWeapons, dAcolyte (0x15, 0x18, 0x16, 0x13, 0x17, 0x85), read back
  with `rmEnableShip`; a load sets the global `G_RecentlyLoaded` = 1 (the exe hashes the literal inline). The difficulty
  rescale of 0x008941a4 needs `cat_spGetDifficultyModifiers` 0x00516f90: `CAT-TODO(stage 6 single player)`.
- **`kasConvertFuncPtrToOffset` 0x00491800 / `kasConvertOffsetToFuncPtr` 0x00491830**: a saved KAS function is its
  cataclysm.exe address minus the current mission's Watch's (mission-code-map 4.10). The generated scripts carry each
  function's exe address (`CatKasFunction` lists in CatMissions.h), so this tree writes the exe's numbers and reads an
  original save's (an ICF-shared body resolves to any of its contexts). [audit: the converters looked only in the
  current mission's list, so a Tutorial1 function saved during the tutorial (currentMission 1 from singlePlayerInit
  0x00518cb1; Tutorial1's own `kasfSaveLevel` at 0x0066a16d, carried out by univUpdate 0x0054ce0b) was written as -1;
  they now search every generated script's list against the current mission's Watch address, as the exe's plain
  subtraction does] `WatchFunctionToMissionEnum` 0x0051b230 /
  `MissionEnumToWatchFunction` 0x0051b340 use the exe's script index (0..16 Mission01..17, 17 Tutorial1, -1 none).
- **`kasMissionStart` 0x00490770**, **`kasExecute` 0x004906d0**, **`kasThisTeamsVectorPtr` 0x004909f0**,
  **`kasShipDied` 0x004911a0**: compared, as the tree (review rows).
- **HS.c**: nothing in cataclysm.exe creates a static hyperspace gate (the table 0x00a43e84 / 0x00a43e80 is written only
  by `LoadHyperspaceGates` 0x00490510 and cleared by `hsStaticInit` / `hsStaticReset`): the `CAT-TODO(stage 6)` "where
  the static gates come from" is answered and closed; `SaveHyperspaceGates` / `LoadHyperspaceGates` compared.
- **Timer.c**: every row was reviewed by producer 3 (0 unread); nothing to add.
- **The console's KAS command** (`cat_consoleKasCommand` 0x0045f270 on the 486-row table) was ported by producer 3; the
  console self-test still passes (section 6).

## 2. The kas2c generator and the generated code

**Generator** `tools/m2_kas2c_gen.py` (tracked). Inputs: the export's decompile of the [R2]-created mission functions
(named after their FSM / state contexts, mission-code-map 8), the exe itself (PE sections: every string, every
localised string table, the kas2c table `cat_kasFunctionTable` 0x00755048 and its dispatcher through
tools/m2_kasf_table_gen.py), and `objdump` of each function. It:

1. walks each script's functions from its Init / Watch (mission-code-map 2.5); a function reached from more than one
   of the 18 scripts (MSVC /OPT:ICF, mission-code-map 7.1) goes to `src/Missions/KasShared.c` under its home name;
2. rewrites each decompiled body by the Cataclysm kas2c dialect (mission-code-map 4): the scope prologue's globals by
   name (`CurrentMissionScope` 0x00c3749c, `CurrentMissionScopeName` 0x00c374a0, `CurrentTeamP` 0x00a43ee8 and its KAS
   fields +0x58 / +0x1C / +0x20 / +0x50); every kasf call rebuilt from the table's parameter list (a UDWORD is a name
   hash whatever Ghidra took it for: a string label, `&DAT_x`, a code label, a pointer into `aisTeams`; a CHARPTR a
   string read from the exe; `(&PTR_x)[strCurLanguage]` one of `LSTRING_<addr>[3]`); invented arguments (`this`,
   `extraout_ECX`) dropped by the table's arity; a kasf jmp-thunk Ghidra decompiled as its target's body put back as
   the kasf call (`kasfMissionFailed`, `kasfTutResetGameMessageQueue`, the formation kasf); locals that hold hashes,
   strings or counts become `uintptr_t` (a hash and a string share a register in the exe); the garbled message hashes
   (`aisTeams[0x21].selection[8].sel.ShipPtr + k`) are resolved by calibrating the base against the functions'
   machine-code constants;
3. **verifies every body against the machine code** and stops on any difference: the multiset of called functions
   (calls and tail jumps; thunks resolved), the set of 32-bit constants (pushed arguments, compared message hashes
   including MSVC's `sub` / `dec` compare chains, string and table addresses) and the argument tuples of every call
   whose arguments are all pushed constants in its own basic block. Last run: **149 bodies verified, 1,790 all-constant
   calls' arguments equal**, 0 overrides (no hand edit was needed: the `OVERRIDES` table is empty);
4. writes `src/Missions/SinglePlayer/Mission01.c` (84 functions + 14 LSTRING tables + the function list),
   `src/Missions/Tutorials/Tutorial1.c` (59), `src/Missions/KasShared.c` (6 shared: `Init_Mission01_FleetIntel_Fail`
   0x005db0b0, `Init_Mission06_FleetIntel_WaitIntel` 0x005f4730, `Init_Mission10_Salvage_Hidden` 0x0060fd90,
   `Init_Mission11_FleetIntel_WaitMessage` 0x00613940, `Watch_Mission14_BeastMothership` 0x0062ca80 (E1),
   `Watch_Mission17_HeavyCruisers_Null` 0x00652ef0 (E2)) and `src/Missions/CatMissions.h` (prototypes, the runtime
   globals, `CatKasFunction`, the per-script lists with each function's exe address, `CAT_<SCRIPT>_WATCH_EXE`), all
   with REULA "GENERATED" headers. Each function carries its exe address and size.

Rerun: `python3 -I tools/m2_kas2c_gen.py` (writes the four files; `--scripts Mission01,Tutorial1` is the default,
part B adds its missions there), `--check` (compares, exit 1 on a difference; last: all four "current"), `--report`
(the per-function verification table). `--exe` / `--export` default to the main checkout's.

Build: `src/Missions/meson.build` now lists only the generated files; `src/meson.build` builds them (`src_missions`,
`inc_src_missions`). HW1's `.kas` scripts and their kas2c meson files stay in the tree, unbuilt.

## 3. Level load (LevelLoad.c, Universe.h, Nebulae.c, SaveCat32)

- **`scriptSetShipsToBeNeeded` 0x004a2f30**: no single-player race swap; `colorScheme` parsed, no team colour loaded;
  race > 6 / type > 0x90 fatal; a capturable ship of a Kushan (2) / Taiidan (3) player clears 0x00a447ac / 0x00a447b0
  (`needR1CapitalShip` / `needR2CapitalShip`), the Kushan case returning after flagging the ship. Closes its
  `CAT-TODO(stage 6 level load)`.
- **`levelPreInit` 0x004a3230**: both need words start at 0 (HW1 set them per player race); a multiplayer game with
  captures tests only 0x00a447b0, for tStandardFrigate's InfoNeeded bit (never set in the exe, faithfully).
- **`SetTeamColorForShipAndRelatedStaticInfo`** (inlined in `llAvailableColorScheme` 0x004a2cf0): the related missile
  statics are Cataclysm's seven carriers (10, 0x55, 0x3b, 1, 0x22, 0x19, 0x5f), the mine one tMinelayerCorvette.
- **`scriptSetResourcesCB` 0x004a07b0** (found modified): the scaled asteroid count is computed in extended precision
  and truncated once (x87, 0x004a0917 .. 0x004a094b); HW1 rounded the product to a float.
- **The meteor storm** (Mission02 and the multiplayer maps' resource spheres, Armed8 among them; not in the tree
  before): `cat_scriptSetMeteorStormCB` 0x004a0d10 (the "MeteorStorm" entry after "Resources" in the mission-sphere
  table 0x00883aa8; "%f %f %f %s %s %d %f %f %f %f %d %f %f %f": position, layout, distribution, number, radius,
  length, rot y / z, attributes, drift speed, added y and x velocity), `ResourceVolume` grows to the exe's 0xB0 bytes
  (`catDriftSpeed` +0x98, `catDriftAddY` +0x9C, `catDriftAddX` +0xA0, `catWrapExtent` +0xA4; pinned on ILP32),
  `AddResourceToCylinder` 0x004a03e0 (found modified) stores the volume's half extents, and
  **`AddResourceToVolumeAtPosition` 0x0049fcf0** (MODIFIED, unread before) is ported: the velocity towards
  "TowardsVector" without random deviation, HW1's head-shot velocity gone, the 0x200 drift (the axis turned by
  roty / rotz, times the speed, plus the y / x terms), the asteroid's wrap box (`catWrapExtent` +0x1A8, the larger of
  the universe's half size and the volume's extent per axis), Asteroid +0x1A0 cleared, a second rotation chance. This
  is the "box's writer for a new game" the oracle rung was missing: the `CAT-TODO(stage 6 level load / KAS)` of
  UnivUpdate.c is closed. CAT-FIX: the exe leaves the speed terms a "?" ends early uninitialised and a sphere storm's
  box unset (zero here). [audit: the two fatal messages now carry the exe's texts (0x00884110, 0x00884198); the
  same table's "Mine" 0x004a3f20 and "Slipgate" 0x004a1020 entries (between "MeteorStorm" and "Derelict") are not
  ported and had no tag: one-line `CAT-TODO(M2-R9 part B)` added (LiveFireExercise2 has such lines; Mission01 and
  Tutorial1 have none)]
- **`nebLoad_Nebula` 0x004cb7f0**: a cataclysm.exe save's nebula chunk (0x58) and tendril (0xFC) records are this
  tree's structs on ILP32 (pinned); the mode-A loader converts them (`nebChunk` / `nebTendril` added to
  tools/m2_save32_gen.py, SaveCat32Layouts.c regenerated) instead of refusing such a save. Closes Nebulae.c's
  `CAT-TODO(stage 6 level load)`. No sample save with nebulae exists to run it on.
- **The mission-number mapping**: `GetMissionsDirAndFile` 0x00517200 takes the exe's second argument (the staging
  level `SinglePlayer\MissionNN_NISmm\MissionNN_NISmm.mission`, mm = 0x00894508[mission]: Missions 01, 04, 06, 07, 10,
  12, 14, 15, 16, 17; the data's eleventh, Mission05_NIS03, is entered by no table) and records 0x00a6adac
  (`catHrMissionNisVariant`, the loading picture's; closes HorseRace.c's tag); `MISSION_17` added to the enum and the
  sequence (Cataclysm's 17 missions); the exe's tables 0x008944bc (entered from its NIS), 0x00894508 (the NIS),
  0x00894470 (arrives at once), 0x00894550 (the NIS files) and 0x008945a0 (the nislets) are in SinglePlayer.c.
- **The IDENTICAL bodies of LevelLoad.c** (18 unread): all compared (disassembly where the decompile garbles the
  volume fields): 15 identical, 3 found modified and ported (`scriptSetResourcesCB`, `llAvailableColorScheme`,
  `AddResourceToCylinder`).
- The AI point / path / box / sphere callbacks were already by hash (producer 3) and reviewed.

## 4. The NIS-first start and the mission flow's entry (SinglePlayer.c, Tutor.c, utility.c, KASFunc.c)

- **`singlePlayerStartGame` 0x0051bdb0** (REWRITTEN): a new game plays nis\n01.nis / nis\n01.script at the origin
  (`singlePlayerNisNumber` 1); `singlePlayerStartNis` first loads the staging level Mission01_NIS01 (the mission number
  stays). The warp start (`cat_spWarpToLevel` 0x0051b9c0) is producer 4's (`CAT-TODO(stage 6 single player)`).
  **HWI-ORACLE**: `/oracleMission` and the self-tests on it (dock, command, render, hud, ui, console, build, ai,
  motion, nis) keep the direct Mission01 start without NIS or KAS (`oracleMissionDirectStart`), the mission stream's
  start since M2-R4; `/catMissionTest` takes the exe's start.
- **`singlePlayerStartNis` 0x0051b670**: the exe's signature (nis, script, position or NULL = KAS point
  "StartPointPlayer", loadLevel, keepMission); no mothership-centred NIS (Cataclysm has none); the derelict centre and
  instance from the KAS globals `G_NisCentreDerelict` / `G_NisCentreInstance`.
- **`singlePlayerNisStoppedCB` 0x0051b430**: after NIS 1 the mothership is the player's ship of class Mothership and
  its recons take the custom formation; the scripts learn the end through `G_Nis<n>Complete` /
  `G_Nislet<n>Complete` (KAS globals; HW1's AIVars and level-1 save gone); Mission01's nislet ends in the white flash
  (0x00a75280); the lockout stays with the letterbox; an NIS that opens its mission sets `HYPERSPACE_LOADDONE` and loads
  the mission's own level.
- **`singlePlayerLoadNewLevel` 0x0051ac80**: the exe's whole body: (nisLevel, keepMission); entering Mission01 from its
  opening NIS, the player's non-hyperspacing ships dock in the mothership and its capital ships and modules go into the
  ether (CAT-FIX: the exe's restart of the ship walk never ends on a ship already in the ether);
  `cat_spDeleteLeftBehindShips` 0x0051abb0 (ported); the fleet modifier by ship class; the effects started on the
  first level; `cat_playerUpdateUpgradesFromResearch`. univUpdate's call passes (0, 0).
- **`singlePlayerGameUpdate` 0x005197d0** (part): the KAS globals `G_PlayNis` / `G_PlayNislet` (the nislets of
  0x008945a0, `cat_trPrecacheShipTextures` for Mission01's), the traders' `PopUpGUI` variable by hash, the black fade
  (0x00a6ac70); **`HYPERSPACE_LOADDONE` starts the mission's kas2c script** (`kasMissionStart("missionNN")`, missions
  1 .. 17; only Mission01 is generated: `CAT-TODO(M2-R9 part B)` for 02 .. 17), closes the screens and locks out; the
  level then waits in `HYPERSPACE_SHIPSARRIVING` (7, no case) until its script calls `StartLevelByHyperspacingShips`
  (0x00498230), which sets **`HYPERSPACE_CATSHIPSHYPERSPACINGIN` (9)**: the fleet arrives (`UpdateArrivingShip`),
  `G_ShipsHaveArrived` = 1, the AI starts, a mission not entered from its NIS plays it. Two states added to the enum
  (9, 10; names chosen; the kasf library had written 9 as `HYPERSPACE_FAILED | HYPERSPACE_PREWAIT`). The at-once
  arrival of the 0x00894470 missions, the roll call / whiteout / case 10 are producer 4's (tags).
- **`tutInitTutorial` 0x0052f690**: starts the generated Tutorial1 (`kasMissionStart("tutorial1", ...)`).
- **`gameStart` 0x005a9290** (utility.c): Mission01's preload is its staging level; 0x00a6ac68
  (`catSpStartInLoadingScreen`) around `singlePlayerStartGame`.
- **`CAT_HW1_MISSION_SCRIPTS` retired**: the switch, its 0 branches and the HW1 mission includes deleted;
  `grep -rn CAT_HW1_MISSION_SCRIPTS src/` -> **0** (the four prose mentions in Dock.c, InfoOverlay.c, AISelfTest.c,
  ConsMgr.c now name the direct start). HW1's NIS tables and lookups (race-suffixed names, nislet tables, the
  mothership-relative flags) are gone; the debug paths use Cataclysm's.

## 5. The mission self-test (new)

`/catMissionTest <ticks>`, harness mode **`missiontest`** (`tools/m2_harness.sh <label> --mode missiontest
[--mission-test-ticks N] [--tutorial]`; documented in the script header). A new single-player game started as sp
starts it, on the mission mode's virtual clock (one universe tick a frame, `/oracleDump` stream), with the exe's
NIS-first start, no sound and no speech (as `/oracleReplay`: the mixer and speech threads run on the wall clock; the
intel events then end by `kasfIntelEventEnded`'s 15 s safety timeout on game time). It logs `missiontest: flow` (NIS
start / stop, level loads, KAS start), `missiontest: fsm` (every KAS team's FSM / state change), the Objectives
FSM's counter, and plays the player's part where the KAS scripts act (inside `univUpdate`, after the AI players; an
HWI-TEST hook) the moment each objective appears:

| objective | the scripted event |
|---|---|
| DestroyBombers | the TaiidanBombers team destroyed with the mission language's `ShipsDamage` (`kasfShipsDamage` 0x004960b0: `ApplyDamageToTarget`, the game's death path) |
| ExploreRegion | a recon launched from the mothership (`clWrapLaunchMultipleShips`, the launch manager's order) and moved to the Taiidan resourcers (`clWrapMove`) until `HasShipBeenDiscovered` (the fog of war) |
| DestroyResourcers | TaiidanResourcers destroyed (`ShipsDamage`) |
| DestroyProxAttackers | the three TaiidanProxAttack teams destroyed (`ShipsDamage`) |
| DestroyDestroyer | TaiidanDestroyer destroyed (`ShipsDamage`) |
| Hyperspace | the player's hyperspace command (`spHyperspaceButtonPushed`, the button's): the jump begins |

The player's combat is not simulated (`ShipsDamage` stands for the player's fleet; an early run without it lost the
mission at tick 17,536 when the Taiidan raiders destroyed all 33 proximity sensors, the script's failure condition).
PASS needs: the opening NIS started and stopped, Mission01's level loaded from it, the KAS script started, the nislet
played, the fleet arrived, the six objectives created and the five destroy / explore ones completed in this order, the
Objectives counter at 6, the hyperspace command accepted (the state machine leaves `NO_HYPERSPACE`), no mission loss.
Lines: `missiontest: <objective> tick=N created|completed`, `missiontest: done ticks=N objectives=N/6
result=PASS|FAIL`; the run then ends through the mission run's own quit (`oracleMissionStopAfterThisTick`), rc 0.
The jump's roll call / whiteout / animatic / Mission02 is producer 4's flow. Both KAS saves the script asks for are
written ("01 - Hiigara", "01 - ~ End of Mission" in the run's SavedGames/SinglePlayer: `kasSave` exercised).

`/catMissionTestTutorial <ticks>` (`--tutorial`): the tutorial started as the front end's Tutorial Alert does
(`utyStartTutorial`), speech on (dummy driver); PASS when the KAS script started, the Tutorial FSM reached Lesson1
(0x00a883a1), its objectives L1Dummy1 / L1Dummy2 exist and the first prompt shows (the narration's subtitle card):
**Tutorial1's first screen only**, as asked.

## 6. Builds and runs (plain harness)

`tools/build.sh --debug` and `--release`: rc 0, 0 compiler warnings under -Werror. `tools/m2_layout32.sh`: OK (new pins:
`ResourceVolume` 0xB0, `nebChunk` 0x58, `nebTendril` 0xFC). `python3 -I tools/m2_kas2c_gen.py --check`: current;
`python3 -I tools/m2_save32_gen.py --check`: current; `python3 -I tools/m2_kasf_table_gen.py --dispatch --check`:
current. `grep -rn CAT_HW1_MISSION_SCRIPTS src/` -> 0.

Final runs (labels under original/scratch-m2/m2r2-harness/runs/; the r8k2-fin2-* ones on the final binaries, the
r8k2-fin-* ones on the binaries before the last change, a CAT-FIX in NIS.c's debug seek that only the sp mode reaches):

| label | mode | milestone | first stop | result |
|---|---|---|---|---|
| `r8k2-fin2-mt`, `r8k2-fin-mt-a`, `r8k2-fin-mt-b` | missiontest (30,000 limit) | mission self-test done | none (rc 0) | **PASS `objectives=6/6`** at tick 12,880; a = b **byte-identical** (`cmp`), sha256 **`b03fdee2b85d36a0`** (all three); ASan 0, no leak |
| `r8k2-fin-tut` | missiontest `--tutorial` | mission self-test done | none (rc 0) | **PASS**: Lesson1 tick 2, L1Dummy1 / L1Dummy2, prompt "LESSON 01 - Start of Tutorial" tick 7; ASan 0 |
| `r8k2-fin2-mission`, `r8k2-fin-mission6k-{a,b,perturb}` (`tools/oracle/mission_test.sh --label r8k2-fin-mission6k --ticks 6000 --perturb 3000 --copy-tick 4000`) | mission, 6,000 ticks | oracle mission run done | none (rc 0) | **MISSION TEST PASS**: a = b byte-identical, sha256 **`19560d16033efd53`** (fin2 the same); perturb caught at 3000 / rng10, flipped copy at 4000 / ships_health; ASan 0 |
| `r8k2-fin2-sp` | sp, `--timeout 360` | universe running (golden-01-02816) | none: still running at the 360 s timeout | ASan 0, no leak (the first `r8k2-fin-sp` stopped on the NIS seek overflow: fixed, below) |
| `r8k2-fin2-fe` | fe, 120 s | Main_game_screen | none: running at the timeout | ASan 0, no leak |
| `r8k2-fin-ai` | ai, 400 | ai self-test done | none | PASS; sha256 `499a3fccf5031e52` |
| `r8k2-fin-skirmish` | skirmish LiveFireExercise2 600 | skirmish self-test done | none | PASS; `436f71fdc2287b45` |
| `r8k2-fin-armed8` | skirmish Armed8 600 | skirmish self-test done | none | PASS; `8299d4e002a2ceea` (324 resources: the meteor storms) |
| `r8k2-fin-console`, `-dock`, `-build`, `-hud`, `-ui`, `-command`, `-render` | the self-tests on the mission run | each "... self-test done" | none | all PASS, ASan 0 (build `2d7bbafaa9b094c8`) |
| `r8k2-r17-off`, `r8k2-r18-off` | replay 10-17 (archive, `--recording-as oracle01-r6`), 10-18 (archive `oracle01-rec-20261018-102253`, `--recording-as oracle01-r18`) | oracle replay done (9601 / 3527 ticks) | none | **IDENTICAL** in every field to producer 1's audited `r8au-r17-off-a` / `r8au-r18-off-a` (only the header's build identity differs) |

Note: the install's `SavedGames/RecordedGames/oracle01` was re-recorded on 2026-10-19 14:35 (archive
`original/oracle/oracle01-rec-20261019-143530`, 133 packets); the 10-18 recording must now be replayed from its archive.

**CAT-FIX in NIS.c `nisSeek` 0x004ce2e0**: HW1 and the exe read `currentPos[6]` (one past the array) after the channel
loop; the sp mode's `/debugScreenshots` `nisGoToEnd` now seeks the opening NIS and ASan stopped on it (`r8k2-fin-sp`);
the first channel is tested instead (and only for a path with keyframes).

The mission self-test's Mission01 FSMs (from `missiontest: fsm` lines of `r8k2-fin-mt-a`): all 20 FSMs of
mission-code-map 8 were created (NISLet1 creates them once the nislet has played); FleetIntel went NISLet1 ->
WaitMessage -> WaitIntel <-> WaitTimer (the intel cycle) through the six objectives; the Objectives counter 0 -> 6;
states reached elsewhere: KushanFlankFrigates UnderAttack / AttackCarrier, KushanProxDefend Patrolling / Attacking,
Pester S_xc2ee6b2 / Launch / Attacking, TaiidanBattle Docked / Create / Launch / Attacking / GuardingCruiser,
TaiidanBombers AttackFlank, TaiidanDestroyer PreFlee / Flee, TaiidanProxAttack Nothing / AttackProx,
TaiidanPunishmentFrigate NotThere, TaiidanReinforcement SitThere, FSM_x5a79103 PreHS / InHS, FSM_x2cce515 Attacking.

## 7. Delta-map review entries (decision 55)

46 entries carry `[review: M2-R8 kas-runtime-mission01]` in ghidra/delta_map_review.json (36 new, 10 existing ones
appended), merged with `python3 -I tools/delta_map.py merge --done m2r8:kas-runtime-mission01` and
`delta_map.py report` (docs/m2-r1-function-deltas.md). Counts: IDENTICAL 2,788 -> 2,785, MODIFIED 1,549 -> 1,552,
reviewed 3,238 -> 3,274. Per file: KAS.c 9 (all 30 rows reviewed now), LevelLoad.c 21 (all 33 reviewed), SinglePlayer.c
8 (`GetMissionsDirAndFile`, `singlePlayerGameUpdate` (part), `singlePlayerLoadNewLevel`, `WatchFunctionToMissionEnum`,
`MissionEnumToWatchFunction`, `singlePlayerNisStoppedCB`, `singlePlayerStartNis`, `singlePlayerStartGame`), HS.c 2,
UnivUpdate.c 4, utility.c `gameStart`, Nebulae.c `nebLoad_Nebula`. **Found modified** (IDENTICAL -> MODIFIED):
`scriptSetResourcesCB` 0x004a07b0, `llAvailableColorScheme` 0x004a2cf0, `AddResourceToCylinder` 0x004a03e0. Stage 6
after the merge (`python3 -I tools/m2_stage_counts.py`): 301 IDENTICAL / 168 MODIFIED / 25 REWRITTEN HW1-matched. [audit: two
edited IDENTICAL bodies had no review for their edit: `nisSeek` 0x004ce2e0 (the CAT-FIX) and `tutInitTutorial` 0x0052f690
(the live Tutorial1 start); `[review: M2-R8 kas-runtime-mission01 audit]` entries added and merged (records changed 2,
classes unchanged)]

## 8. CAT-TODOs

Closed (10): UnivUpdate.c 3 x `CAT-TODO(stage 6)` (KAS selections) and 1 `CAT-TODO(stage 6 level load / KAS)` (the
wrap box's writer); LevelLoad.c `CAT-TODO(stage 6 level load)` (scriptSetShipsToBeNeeded); SDL/utility.c
`CAT-TODO(stage 6 level load)` (the shuffle) with its `CAT-PROVISIONAL` (the identity table: decision 68 retired);
Nebulae.c `CAT-TODO(stage 6 level load)`; HS.c `CAT-TODO(stage 6)`; HorseRace.c `CAT-TODO(stage 6 single player)`
(0x00a6adac). `CAT-PROVISIONAL` removed: SinglePlayer.h's switch, SinglePlayer.c's two (the kas2c code, the
mission-start NIS), Tutor.c's (Tutorial1). `grep -rn "CAT-TODO(stage 6 KAS\|CAT-TODO(stage 6 level load\|CAT-TODO(stage 6)"
src/` -> 0.

Added (owner first, one line): `CAT-TODO(M2-R9 part B)` the generated code of Mission02 .. 17 (SinglePlayer.c);
`CAT-TODO(stage 6 single player)` x3 for producer 4: the at-once arrival of the 0x00894470 missions, the warp start
`cat_spWarpToLevel`, kasLoad's difficulty rescale (KAS.c; SinglePlayer.c's existing tag on 0x008941a4 updated). [audit: a fourth new `CAT-TODO(stage 6 single player)`: the black fade / white flash drawing in `mrRegionDraw`
0x00569260 (SinglePlayer.c); the audit added `CAT-TODO(M2-R9 part B)` for the "Mine" / "Slipgate" mission-sphere
readers (LevelLoad.c)]

## 9. The Mission01 stream

- **Before** (base tree `7d225de`, `r8k2-mission-before`, 6,000 ticks): sha256 `e30f67f90181dddf` (the audited
  `b97180de748702e5`'s content: IDENTICAL in every field; the stock difference is the header's build identity).
- Everything that leaves the stream alone came first (the KAS runtime, the generator, the NIS-first start behind the
  HWI-ORACLE direct start, the level-load deltas): `r8k2-pre-mission` = `e30f67f90181dddf`, IDENTICAL to the audited
  `r8kfa-final6k-a` in every field; skirmish LFE2 and ai IDENTICAL to `r8kfa-final-skirmish` / `r8kfa-final-ai`.
- **After** (the shuffle, final tree): `r8k2-fin-mission6k-a` = `-b` = `r8k2-fin2-mission`, sha256
  **`19560d16033efd53`**. Against the audited stream: `python3 -I tools/oracle/hwo_compare.py
  <runs>/r8kfa-final6k-a/run.hwo <runs>/r8k2-fin-mission6k-a/run.hwo --from-tick 1 --to-tick 6000 --fields
  rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real
  --json <runs>/r8k2-fin-mission6k-a/compare-audit.json` -> **IDENTICAL: 6000 ticks**; with all fields: **first divergent
  tick 0, rng0 (40 draws ahead) and rng_all only**; with every field but rng0 / rng_all (incl. ships_rot, statcheck,
  players, fpcw): IDENTICAL over ticks 0-6000; rng0 is still exactly 40 draws ahead at tick 6000. **The move: rng0 from
  tick 0, nothing else** (the 40 stream-0 draws of the shuffle; Mission01's own positions do not depend on the table).
- **The skirmish streams** (vs `r8k2-pre-*`, the same content as before this round): LiveFireExercise2 `cd9519217726d6ad`
  -> `436f71fdc2287b45`: rng0 40 ahead and the ships' fields (misc, pos, vel, rot ...) from tick 0 (the start
  positions are shuffled); Armed8 `bdbdbbcd9e4ba2c4` -> `1eaa135254cf503b` (shuffle: rng0, ships' fields, health from
  tick 0) -> `8299d4e002a2ceea` (the meteor storms: rng10 602 draws ahead and resources 242 -> 324 from tick 0); ai
  `4bddd8a7d401a205` -> `499a3fccf5031e52` (rng0 / rng_all only, every other field IDENTICAL over 400 ticks).

## For STATE.md

- **Chosen names**: `cat_kasShipRemovedFromSelections` (the map's), `HYPERSPACE_CATSHIPSHYPERSPACINGIN` (9) and
  `HYPERSPACE_CATFAILEDRETURN` (10), `MISSION_17`, `catSpMissionStartsInNis` 0x008944bc, `catSpMissionNis` 0x00894508,
  `catSpMissionArrivesAtOnce` 0x00894470, `catSpNisNames` 0x00894550, `catSpNisletName` 0x008945a0,
  `catSpLevelFade` 0x00a6ac70, `catSpWhiteFlash` 0x00a75280, `catSpLevelFromNisStop` 0x00a6ac74, `catSpFleetArriving`
  0x00a6ad78, `catSpArriveAtOnce` / `catSpArriveAtOnceTime` 0x00a6adbc / 0x00a6adb8, `catSpStartInLoadingScreen`
  0x00a6ac68, `catHrMissionNisVariant` 0x00a6adac (now shared), `CatKasFunction`, `cat_spKasScriptFunctions`,
  `ResourceVolume.catDriftSpeed` / `catDriftAddY` / `catDriftAddX` / `catWrapExtent`; the generated functions keep the
  export's kas2c names (mission-code-map 8, unresolved hashes as `FSM_x` / `S_x`).
- **Defaults**: the mission oracle run (`/oracleMission` and the self-tests on it) keeps the direct Mission01 start
  (HWI-ORACLE); the mission self-test destroys the destroy objectives' targets with `ShipsDamage` (the player's combat
  is not simulated) and runs without sound / speech; the tutorial test keeps speech; the generated code is written from
  the export's decompile and verified against the machine code (no hand edits); a meteor storm's unread "?" terms and a
  sphere storm's box are zero (CAT-FIX); the start-position shuffle is live in every non-tutorial new game; the
  10-18 recording is replayed from its archive (the install's oracle01 was re-recorded 2026-10-19 14:35).

## What is left (owner)

- **M2-R8 producer 4 (stage 6 single player / tutorial)**: the rest of `singlePlayerGameUpdate` (the roll call, whiteout,
  animatic, next level, case 10; the at-once arrival of the 0x00894470 missions), `cat_spWarpToLevel`,
  `cat_spGetDifficultyModifiers` and kasLoad's rescale, `UpdateArrivingShip` 0x00518170 (MODIFIED, unread),
  `spHyperspaceButtonPushed`, the black fade / white flash drawing in `mrRegionDraw` 0x00569260, the Tutor.c bodies
  (the tutorial buttons; the tutorial test shows only its first screen). Each has a one-line tag.
- **M2-R9 part B**: the generated code of Mission02 .. Mission17 (`tools/m2_kas2c_gen.py --scripts ...` then the
  scripts' table in SinglePlayer.c; the generator's verification covers them as it does Mission01; Mission02 uses the
  meteor storm ported here).
- **The mission self-test's combat**: the destroy objectives are completed with `ShipsDamage`; a player's battle is
  not played (the KAS-controlled Kushan teams fight on their own). A later test could fly the player's fleet.
- **Original saves**: the KAS function offsets now follow the exe (save compatibility for Mission01 / Tutorial1); no
  original single-player save was available to load (the user's saves, R6 list 8).
