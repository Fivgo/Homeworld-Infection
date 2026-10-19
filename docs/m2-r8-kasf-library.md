<!-- done: kasf-table-dispatch, kasf-bodies, kas-hash-callers, objectives-taskbar-letterbox, ui-sound-ids, verification -->
# M2-R8 producer 3: the kasf library

Base commit `15df4d6` (M2-R8 oracle-rung-physics, audited). The work is `git diff 15df4d6` in the m2 worktree; a
"WIP (snapshot 2026-10-19 15:35)" commit `e3f05f4` was cut by the five-hour limit and this run continued from it (no
restart). Scope (docs/m2-r7-summary.md "Plan for M2-R8", producer 3): KASFunc.c (the HW1-matched bodies and the 184
Cataclysm-only kasf of docs/mission-code-map.md 5.1, the 9 ICF-folded stubs of 5), Objectives.c, the task bar's
removal, the letterbox callers, the kasf side of the tutorial buttons, the KAS variable store's callers, the two
AITeam.c KAS tags, the 30 remaining `soundEvent(NULL, UI_*)` sites, and the console's KAS command on the exe's
486-entry table.

Subsystems (the done marker above):

1. **kasf-table-dispatch**: the exe's kas2c function table and dispatcher, generated (tools/m2_kasf_table_gen.py).
2. **kasf-bodies**: every kasf at the exe's body (KASFunc.c, KASFunc.h), the 9 folded stubs, the review of every
   KASFunc.c / Objectives.c row of the delta map.
3. **kas-hash-callers**: every KAS name is a `cat_kasHashString` hash (KAS.c / KAS.h, Timer.c, AITeam.c, LevelLoad.c,
   NIS.c, SinglePlayer.c) and the KAS variable store's callers (Gun.c, Dock.c, Salvage.c, BeastWeapon.c, render.c,
   RKO.c, ShipDisable.c), the console's KAS command (Console.c).
4. **objectives-taskbar-letterbox**: Objectives.c, the task bar's removal (TaskBar.c / .h, utility.c, SinglePlayer.c,
   TradeMgr.c), the KAS letterbox (kasfWideScreenIn / Out, NIS.c, KeyBindings.c), the kasf side of the tutorial.
5. **ui-sound-ids**: the UI sound-id sites (decision 65).
6. **verification**: builds, self-tests, the Mission01 stream, the report.

## 1. The KAS function table and its dispatcher (generated)

cataclysm.exe carries its own kas2c table, `cat_kasFunctionTable` **0x00755048**: 487 HW1 kas2c `FunctionCall` rows of
0x990 bytes (tools/kas2c/kas2c.h of HW1: `name[129]`, `returnsNumber` at +132, `realName[129]` at +136, `numParams` at
+268, 16 `FunctionParam {name[129], type}` rows of 136 bytes from +272), the last the `"*"` sentinel: **486 KAS
functions**. The dev console's KAS command `cat_consoleKasCommand` **0x0045f270** parses a line by it and calls
`cat_kasCallFunctionByName` **0x0045b9a0**, an MSVC compare tree on `cat_kasHashString(name)` (**0x00491860**, the PJW
hash of the characters with bit 5 cleared) whose leaves call one kasf each.

**Generator** `tools/m2_kasf_table_gen.py` (tracked; reads the PE file and `objdump -d -M intel` of the dispatcher, no
Ghidra):

1. reads the 486 rows and the sentinel from `.data`;
2. hashes every KAS name with the exe's hash;
3. walks the dispatcher's compare tree (0x0045b9a0 .. 0x0045f270, a small simulation of eax and the flags over cmp /
   sub / dec / ja / je / jne) for every hash to the kasf it calls: **the exe address of every row, no guessing**;
4. writes `src/Game/KASFuncTable.c` (each row: KAS name, hash, returnsNumber, C name, params, exe address) and, with
   `--dispatch`, `src/Game/KASFuncDispatch.c`: `cat_kasCallFunctionByName` as a C `switch` on the hash, 486 cases, each
   converting the console's argument pointers as the exe's branch does (`*(T *)args[i]`).

Rerun: `python3 -I tools/m2_kasf_table_gen.py --dispatch` (writes both files); `--check` compares with the tracked files
and exits 1 on a difference; `--list` prints row, KAS name, hash, address, C name, parameters. `--exe` defaults to the
main checkout's `original/game/cataclysm.exe`. Last check: `python3 -I tools/m2_kasf_table_gen.py --dispatch --check`
-> "src/Game/KASFuncTable.c current", "src/Game/KASFuncDispatch.c current", rc 0.

New files: `src/Game/KASFuncTable.h` (the row types, `CAT_KAS_FUNCTION_COUNT` 486, kas2c's 18 parameter types; a
`KAS_PARAM_UDWORD` is a KAS name hash in Cataclysm), `src/Game/KASFuncTable.c`, `src/Game/KASFuncDispatch.c` (both
GENERATED, REULA headers), added to `src/Game/meson.build`. Because every row's kasf is called by the dispatcher and the
build links, **all 486 kasf exist**.

## 2. KASFunc.c / KASFunc.h: the kasf bodies

- **Signatures**: the kasf take the exe's parameter types from the table (KASFunc.h rewritten in the table's order):
  every timer, variable, message and label argument is a `udword` hash, not a `char *`; the "number" returns are
  `sdword`.
- **The 184 Cataclysm-only kasf** of mission-code-map 5.1 are all defined, each at the exe's body with a `CAT:` header
  naming its address (groups of identical shapes share one header: e.g. `kasfShipsSelectIdle / kasfShipsSelectNotIdle
  0x00495140 / 0x00495160`, `kasfTutShowNextTextButton .. kasfTutQuitButtonClicked 0x00496610 .. 0x00496690`,
  `kasfGetVectorX .. kasfGetVectorW 0x0049ace0 .. 0x0049ad40`). Among them: the ship / team / selection operations
  (the 26 `kasfShipsSelect*` on the exe's shared filter `cat_kasShipsSelectByPredicate` **0x00494f90** with one
  predicate each, `kasfShipsSetFormation` 0x00492740, `kasfShipsParalyze`, `kasfShipsSetInvincible` 0x004984c0,
  `kasfShipsSetColor`, `kasfShipsUpgrade`, `kasfShipsSort` / `kasfShipsShuffle`, `kasfFirstShip`,
  `kasfGetPlayerMothership`, the mines / meteors finders, `kasfTimesHitByEMP`), the Cataclysm ship types and modules
  (`kasfMimic` / `kasfSetMCVItemIndex` with `cat_spSetMCVItemIndex` in Mimic.c, the RKO, Naggarok, Nomad moon, beast,
  swarm and slipgate kasf, `kasfMission13DeleteEngineeringModuleHack`), spawn / effect / sound / speech / subtitle /
  camera / NIS calls (`kasfSpawnEffect`, `kasfSpeechEventLink` / `NoLink`, `kasfSubtitleReset`, `kasfCameraFocus`,
  `kasfCameraSetAngleDeg` / `DeclinationDeg` / `Distance`, `kasfMakeShipCamera`, `kasfWideScreenInDontCloseSensors`,
  `kasfStartLevelByHyperspacingShips`), the parameter conversions (`kasfVector*`, `kasfGetVectorX..W` =
  `floor(c + 0.5)`, the universe clamp of the point functions).
- **The 9 ICF-folded stubs** (mission-code-map 5): `kasfLog`, `kasfTeamSkillSet`, `kasfGateMoveToNearest`,
  `kasfToggleActor`, `kasfGameEnd`, `kasfSetPlayerResourceCap`, `kasfAssert` are empty (the stub **0x0049ae30**);
  `kasfTeamSkillGet`, `kasfGateShipsOutNearest` return 0 (**0x006fe9a0**); each says so in a `CAT:` comment.
  `kasfDockSupport` shares `kasfDock`'s 0x00494570. HW1's `kasfForceTaskbar`, `kasfTargetDrop`, `kasfTeamGiveToPlayer`
  are not in the exe's table (no row, no dispatcher case).
- **Every kasf carries its address**: the HW1-matched ones that had no `CAT:` comment carry a trailing
  `// KAS <name>, cataclysm.exe 0x00xxxxxx` on the signature line (234 signatures; on the line, so no `__LINE__` moved
  and the object code is unchanged). Check: every row of KASFuncTable.c has a definition in src/Game with its address
  next to it.
- **Mission01 and Tutorial1** (mission-code-map 8): the disassembly of the two missions' FSM / state / Init / Watch
  bodies (the map's addresses) calls 101 (Mission01) and 112 (Tutorial1) distinct kasf addresses, 159 C names in the
  union [audit: a walk that stops at each body's first `ret` misses calls after it; the audit's scan of the blocks
  (Mission01 0x005d6fe0 .. 0x005db2a0 plus the shared 0x005f4730 / 0x00613940, Tutorial1 0x00669530 .. 0x00670a10 plus
  0x005f4730 / 0x0060fd90) finds 103 / 109 by `call` (the map's section 8 counts) and 106 / 112 with the tail `jmp`s, 160
  C names in the union; every one links, since the dispatcher calls all 486]; every one is defined at the exe's body with its address (the 23 + 28 Cataclysm-only ones the map lists among
  them).
- [audit: two bodies were not at the exe's: `kasfShipsSort` 0x0049a100 rounded all six differences to float and summed
  x first, where the exe keeps five on the x87 stack (53-bit) and stores only bz as a float (0x0049a197), summing
  (z*z + y*y) + x*x; `kasfWideScreenIn*` / `kasfWideScreenOut` scaled the frames by the literal 0.0333333f (0x3d088880)
  where the exe's constant 0x0072ab58 is 1/30 as a float (0x3d088889). Both fixed by the audit; no KAS runs in the
  oracle streams, so nothing moved.]
- **The HW1-matched bodies**: every KASFunc.c row of the delta map (208 IDENTICAL, 66 MODIFIED, 20 REWRITTEN after the
  merge) now has a review entry; 91 IDENTICAL rows never reviewed were compared with the exe's disassembly (call lists
  by `kt/cmpcalls.py` in the scratchpad, then each flagged body by hand: the "missing" calls were inlined HW1 helpers,
  e.g. `DockSupportWith` in `kasfDockSupportWith` / `kasfDockStay`, `ShipReadyForLaunch` in
  `kasfTeamDockedReadyForLaunch`, `kasfFindPlayersShipsOfClass` in `kasfFindEnemyShipsOfClass`); **30 KASFunc.c
  bodies classed IDENTICAL were found modified** and ported (`kasfShipsOrderAttributes`, `kasfTeamHealthAverage`,
  `kasfTeamHealthLowest`, `kasfTeamFuelAverage`, `kasfTeamFuelLowest`, `kasfTeamHyperspaceInNear`,
  `kasfHyperspaceDelay`, `kasfShipsSetDamageFactor`, `kasfTeamSetPercentDamaged`, `kasfTeamSetPercentFueled`,
  `kasfRequestShips`, `kasfRequestShipsOriginal`, `kasfTeamGiveToAI`, `kasfTeamFinishedLaunching`,
  `kasfFindEnemyShipsOfType`, `kasfFindFriendlyShipsOfType`, `kasfPopupTextDraw`, `kasfObjectiveCreate`,
  `kasfShipsInFormation`, `kasfCameraGetAngleDeg`, `kasfCameraGetDeclinationDeg`, `kasfTutSetPointerTargetShip`,
  `kasfTutSetPointerTargetShipSelection`, `kasfCloseSensors`, `kasfSpeechEvent`, `kasfRenderedShips` (the LOD mask
  has bits 1 .. min(LOD, 7)), `kasfSubtitleSimulate`, `kasfLocationCard`, `kasfRotateDerelictType`,
  `kasfSensorsStaticOn`); `kasfSaveLevel` REWRITTEN -> MODIFIED (the request is carried out by `univUpdate`).
  Examples of the exe deltas: the timers / variables scope prologue (`kasfScopeHash`: a `G_` name has bit 31 and is
  global, others are scoped to the running FSM / state hash `CurrentMissionScopeName` 0x00c374a0),
  `kasfTimerStop` 0x00491fe0 stops (HW1 called `timTimerStart`), `kasfMissionCompleted` 0x00491c70 plays the animatic
  between mission N and N+1, `kasfFindDistance` 0x004943d0 takes the table `fsqrt` path in the exe (decision 56 keeps
  `sqrtf`), `SelectionSwitchPlayerOwner` 0x00493f80 with the exe's `keepSU` argument (closes its stage-6 KAS tag).

## 3. KAS names are hashes; the KAS variable store's callers; the console's KAS command

- **KASVar.c / .h**: `cat_kasHashString` **0x00491860** (`h = (c & 0xDF) + (h << 4); g = h & 0xF0000000; if g,
  h ^= (g >> 24) ^ g`).
- **KAS.c / KAS.h**: every label (labelled vectors, volumes, paths, selections, teams) is its hash; `kasAITeamPtr`
  0x004907a0 / `kasAITeamShipsPtr` 0x004907e0 by hash with no `"MsgSender"` label and no fatal; `kasJump` 0x00490640 and
  `kasFSMCreate` 0x00490660 store the state / FSM hashes; `CurrentMissionScopeName` is the scope hash; 7 KAS.c bodies
  classed IDENTICAL found modified (`kasShipsVectorPtr`, `kasTeamsVectorPtr`, `kasVolumeVectorPtr`,
  `kasPathPtrNoErrorChecking`, `kasVolumePtr`, `kasGetGrowSelectionPtrIfExists`, `kasLabelledVolumeAdd`: by hash).
  The rest of KAS.c (the runtime: `kasExecute`, save / load, the 10 runtime helpers) is producer 2's.
- **Timer.c / .h**: the KAS timers are addressed by (scope hash, name hash) (`timTimerFind`, the search every exe timer
  function starts with, 0x0052bfe0 .. 0x0052c820); the time left rounds to the nearest second as the exe's inline code
  (`timTimerLeft`; HW1 truncated); a running timer keeps its start time (0x0052bfe0), a paused one keeps
  `(float)0x7fffffff` (0x0052c060); `timTimerPauseAllNotScoped` found modified.
- **AITeam.c / .h (the two AITeam.c KAS tags)**: a queued message is its kas2c hash (the queue's 64 words were taken for
  pointers in R7): `aitMsgSend` 0x00417e10 / `aitMsgReceived` 0x00417e60 / `cat_aitMsgQueueRemoveAt` 0x00417ec0 take a
  `udword`, and `SaveThisAITeam` 0x00418040 saves the words as they are (the tag "save as kas2c id" closed); the team's
  KAS identity is three hashes at +0x58 / +0x5C / +0x60 and the two watch functions as kas2c offsets at +0x64 / +0x68,
  which the mode-A loader now reads (`aitCat32LoadTeam`; the tag "the exe's KAS identity" closed). AITeam is now
  0x280 bytes on ILP32 with the KAS fields at the exe's offsets: **pinned** (`_Static_assert` under `__i386__`,
  checked by tools/m2_layout32.sh: OK).
- **LevelLoad.c / NIS.c / SinglePlayer.c**: the mission file's AI point / path / box / sphere callbacks and
  `scriptSetShipsCB` add by `cat_kasHashString(label)`; `nisStart` 0x004cd2b0 looks up "NisTriggerShips" by hash,
  `nisRemainAtEnd` 0x004d0050 and `nisAttackKasSelectionSet` 0x004d45a0 pass hashes; `GetPointOfName` 0x00517280 by
  hash (closes the five NIS / LevelLoad stage-6 KAS tags).
- **The KAS variable store's callers** (`cat_kasVarFind` 0x00491b20, global variables at scope 0, the value at +8 as
  `gunShootGunsAtTarget` reads it): Gun.c `gunShootGunsAtTarget` 0x0048c2e0 (no ship fires in Mission 15 while
  `G_StopShipsFiringOnMission15` is set), Dock.c (`G_ResourcersRetired` = 1, Mission 9), Salvage.c
  (`G_CrystalsHarvested` incremented if it exists), BeastWeapon.c (Mission 7's first infected prison ship 0x86 added to
  the KAS selection), render.c and ShipDisable.c (`G_NaggarokEMP`: the EMP-shock fraction), RKO.c (`RKOHouseKeep`
  0x005c4ba0: a global variable's value, 0 when absent). Gun.c also: the KAS damage factor is Ship **+0x3E0**
  (`catDamageFactor`; `gunShoot` 0x0048b72b and `missileShoot` 0x0048af06 read it, `kasfShipsSetDamageFactor`
  0x00493980 writes it); +0x3DC is the KAS repair factor.
- **SaveGame.c / UnivUpdate.c**: a single-player load ends with `cat_kasAutoSaveOnLoad` (LoadGame 0x004fe3aa); a KAS
  save request (`kasfSaveLevel`) is carried out last in `univUpdate` (0x0054ce03 -> `cat_kasSaveLevelNow`).
- **SpaceObj.h**: `CAT_SHIPFLAG_Orphaned` renamed `CAT_SHIPFLAG_Invincible` (BIT2 of `catFlags18`: its KAS setter
  `kasfShipsSetInvincible` 0x004984c0 names it; Carrier.c, Mothership.c, Slipgate.c, GenericDefender.c, Salvage.c,
  UnivUpdate.c follow).
- **Console.c**: `cat_consoleKasCommand` **0x0045f270** ported: the first table row whose KAS name starts with the typed
  name (case ignored), arguments parsed per row type (numbers by atoi / atof; a UDWORD that is not a number is a name,
  hashed, `G_` global; selections / teams / paths / points / volumes by their label hashes with the kas2c prefixes
  `SHIPS_` .. `VOLUME_`; a SHIPPTR is never parsed), then `cat_kasCallFunctionByName`; replies "KAS Ok", "KAS Returned
  %d", "Error <name>: bad parameter <n>"; a line naming no KAS function is the caller's "Not a command". CAT-FIX: the
  exe frees the strtok name (a pointer into the line); this tree does not.

## 4. Objectives, the task bar, the letterbox, the tutorial (kasf side)

- **Objectives.c (6 rows, all reviewed)**: no task bar: an objective goes to the info overlay's list
  (`cat_ioObjective*`; the hyperspace objective first), `objectiveDestroy` 0x004d63a0 -> `cat_ioObjectiveRemove`
  0x004ac650, `objectiveDestroyAll` 0x004d6480 -> `cat_ioObjectivesClear` 0x004ac6a0; `cat_objectiveCountActive`
  0x004d6370 (KAS `ObjectivesGetAny`); `poPopupTextDraw` 0x004d6530 also draws in the tutorial (found modified).
- **The task bar (decision 41)**: `tbStartup` / `tbShutdown` deleted from TaskBar.c / .h (gameStart 0x005a9290 and
  utyGameSystemsShutdown 0x005ac710 call neither); their callers in SDL/utility.c removed; SinglePlayer.c's
  `tbForceTaskbar` / task-bar lockout calls gone (`spMainScreen` 0x00518d80 ends with `feAllMenusDelete`; `spLockout`
  0x00518db0 never writes `tbDisable` 0x00a6b254), TradeMgr.c's two lockouts likewise; Objectives.c no longer calls
  `tbObjectivesListAddItem`; KASFunc.c's `kasfForceTaskbar` is gone with its table row. `tbForceTaskbar` 0x0052b3b0
  and `tbBumperProcess` 0x0052b100 stay (the exe has them; Tutor.c's back button calls `tbForceTaskbar`). The unbuilt
  mac original src/SDL/utility_mac.c still names `tbStartup` (not in any meson.build) [audit: was "src/Win32/utility.c
  and src/SDL/utility_mac.c"; src/Win32/utility.c has no task bar call].
- **The letterbox**: `kasfWideScreenIn` 0x00497420, `cat_kasWideScreenInHyperspace` 0x004974d0 and
  `kasfWideScreenInDontCloseSensors` 0x00497580 (the same body three times in the exe: `kasfWideScreenInBody`),
  `kasfWideScreenOut` 0x00497630, `cat_kasWideScreenActive` 0x004973c0 / `cat_kasWideScreenInFinish` 0x004973d0
  (called by `nisUpdateTask` at the end of the scissor fade-in, NIS.c); the time / Pause keys test the word
  **0x00a4cb38** = `nisScissorFadeIn` (`fld` / compare with 0 at 0x005504f0 ..; KeyBindings.c `CAT_KB_LETTERBOX_IN`,
  was `FALSE`).
- **The tutorial, kasf side**: the kasf of the Cataclysm tutorial buttons and pointers call Tutor.c entry points
  (`cat_tutSetPointerTargetObj` 0x00530060, `cat_tutShowNextTextButton` 0x00532190 .. `cat_tutQuitButtonClicked`
  0x00532ab0, `cat_tutSelectShips`), added at the end of Tutor.c as stand-ins that show nothing and report no click,
  with one `CAT-TODO(stage 6 tutorial)` for producer 4 who ports their bodies; the pointer setters take the exe's third
  argument (objective-list flag), not yet stored (second `CAT-TODO(stage 6 tutorial)`, producer 4). The six
  `cat_tutButtonsHideForScreen` / `RestoreAfterScreen` sites in LaunchMgr.c, ResearchGUI.c and ConsMgr.c call Tutor.c
  bodies and stay producer 4's tags.
- **HW1's compiled missions out of the build**: src/Missions (HW1's kas2c output) calls the kasf with HW1's string
  signatures, so it is out of `src/meson.build`; SinglePlayer.c / Tutor.c keep the references under
  `#if CAT_HW1_MISSION_SCRIPTS` (0). Tutor.c's `tutInitTutorial` carries a `CAT-PROVISIONAL (stage 6, the kas2c mission
  code)` until the generated Tutorial1 exists (producer 2). This changes nothing at run time: under
  `CAT_HW1_MISSION_SCRIPTS 0` no HW1 mission ran.

## 5. The UI sound ids (decision 65)

Each site's `soundEventPlay` 0x005209f0 push read in the exe. New ids in SoundEventDefs.h:
`CAT_UI_TradingAmbient` 0x103, `CAT_UI_Letterbox` 0x123, `CAT_UI_Unletterbox` 0x124, `CAT_UI_CrateFound` 0x128,
`CAT_UI_ChatMessage` 0x12b, `CAT_UI_SensorsPing` 0x12c, `CAT_UI_ManagerIntro` 0x12f, `CAT_UI_ManagerExit` 0x130,
`CAT_UI_PingProximity` 0x131, `CAT_UI_PingNewShips` 0x132, `CAT_UI_PingBattle` 0x133, `CAT_UI_PingHyperspace` 0x134.

| file | sites | exe id |
|---|---|---|
| Crates.c `cratesPlayerFoundACrate` | 1 | 0x128 (0x00460dc2) |
| GameChat.c `gcProcessGameChatPacket` | 1 | 0x12b (0x00486dcc; the body found modified) |
| KASFunc.c `kasfWideScreenIn` / `kasfWideScreenOut` | 2 | 0x123 (0x00497481) / 0x124 (0x0049766a) |
| NIS.c `nisScissorOut` | 1 | 0x124 (0x004d02d9) |
| Ping.c `pingListDraw` | 5 | 0x12c, 0x133, 0x134, 0x131, 0x132 (0x004e8add .. 0x004e8bb6) |
| TradeMgr.c `tmLeave` / `tmTradeBegin` | 4 | 0x130, 0x100 (as HW1) / 0x12f, 0x103 |
| ResearchGUI.c | 1 | 0x100 (as HW1) |
| SDL/mainrgn.c `mrBuildShips` / `mrLaunch` / `mrResearch` | 3 | 0x108 = HW1's `UI_Click` |
| UIControls.c `uicButtonProcess` (2), `uicScrollBarProcess`, `uicListWindowProcess`, `uicTextEntryProcess`, `uicDragButtonProcess` (2), `uicEscProcess` | 8 | 0x108 = HW1's `UI_Click` (0x005349f9, 0x00534a2d, 0x005350c6, 0x0053620d, 0x00536c96, 0x00537f77, 0x00537fe3; `uicEscProcess` is inlined at 0x0053610c / 0x00536d59) |
| FEFlow.c `feMenuItemProcess` | 2 | no exe body (no popup menus): HW1's ids kept, marked |
| Sensors.c (`UI_MovementGUIoff`) | 4 | no exe body (the Cataclysm sensors screen has no SM_* buttons): HW1's id kept, marked |

32 sites; every `soundEvent(NULL, UI_*)` / `soundEventPlay(NULL, UI_*)` site in src/Game, src/SDL and src/Ships now
carries a `CAT:` comment with its exe id (`grep -raE` over the three directories: 37 live sites, 0 without one; note
UIControls.c is not UTF-8, so a plain `grep -r` skips it). UIControls.c's ids are HW1's, so its IDENTICAL bodies stay
IDENTICAL (no review entry).

## 6. Delta-map review entries (decision 55)

229 entries in ghidra/delta_map_review.json carry `[review: M2-R8 kasf-library]` [audit: plus three `[review: M2-R8 kasf-library audit]` entries for edited IDENTICAL bodies that had none: `kasfMsgSend` 0x00494a00, `kasfMsgReceived` 0x00492120 (both stay IDENTICAL), `tutInitTutorial` 0x0052f690 (IDENTICAL; CAT-PROVISIONAL start)], merged into ghidra/delta_map.json
and docs/m2-r1-function-deltas.md as the R7 producers did. Per file (class after the merge): KASFunc.c 91 IDENTICAL /
49 MODIFIED / 20 REWRITTEN; KAS.c 13 / 3 (MODIFIED / REWRITTEN); Objectives.c 6 IDENTICAL / 5 MODIFIED / 1
REWRITTEN; Timer.c 3 IDENTICAL / 9 MODIFIED; AITeam.c 2 / 2; LevelLoad.c 5; NIS.c 1 / 3; SinglePlayer.c 3; utility.c
3; Gun.c 2 / 1; Crates.c, GameChat.c, Ping.c, SaveGame.c, TradeMgr.c, Tutor.c, UnivUpdate.c 1 each. **Class changes**
(IDENTICAL -> MODIFIED, found modified): KASFunc.c 30 (section 2), KAS.c 7, Timer.c 1 (`timTimerPauseAllNotScoped`),
Objectives.c 1 (`poPopupTextDraw`), GameChat.c 1 (`gcProcessGameChatPacket`); KASFunc.c `kasfSaveLevel` REWRITTEN ->
MODIFIED. Every KASFunc.c and Objectives.c row of the map now has a review (0 unread; the plan's "35 / 5 unread" were
60 / 6 MODIFIED-or-REWRITTEN rows with no review at the base, plus 91 / 6 never-reviewed IDENTICAL).

## 7. CAT-TODOs

Closed (21): AITeam.c 2 (stage 6 KAS: the message id, the KAS identity), Console.c 1 (the KAS table), KASFunc.c 1
(`SelectionSwitchPlayerOwner`'s keepSU), KeyBindings.c 1 (the letterbox), LevelLoad.c 1, NIS.c 4, render.c 1 (stage 6
KAS); BeastWeapon.c, Gun.c, Salvage.c, UnivUpdate.c, RKO.c, ShipDisable.c 1 each (plain stage 6); Dock.c 1 (stage 6
KAS); SaveGame.c 1 and SDL/utility.c 2 (stage 6 single player: the auto save on load, the task bar).
`grep -ran "CAT-TODO(stage 6 KAS\|CAT-TODO(stage 6 kasf\|CAT-TODO(stage 6, KAS" src/` -> **0**.

Added (owner first, one line each): Tutor.c 2 x `CAT-TODO(stage 6 tutorial)` (producer 4: the stand-ins' bodies, the
pointer's objective flag); Tutor.c 1 `CAT-PROVISIONAL (stage 6, the kas2c mission code)` (producer 2: Tutorial1).
Not mine and left: 3 x `CAT-TODO(stage 6)` in UnivUpdate.c for `cat_kasShipRemovedFromSelections` 0x004911e0 (a KAS
runtime helper, mission-code-map 3: producer 2), the stage-6 single player / tutorial / level load tags of producers 2
and 4.

## 8. Builds and runs (plain harness)

`tools/build.sh --debug` and `--release`: rc 0, 0 compiler warnings under -Werror (the debug log's one "WARNING" is
meson's sanitizer notice). `tools/m2_layout32.sh`: OK (incl. the new AITeam pins).
`python3 -I tools/m2_kasf_table_gen.py --dispatch --check`: current.

Final runs, on the final tree's binaries (labels under original/scratch-m2/m2r2-harness/runs/):

| label | mode | milestone | first stop | result |
|---|---|---|---|---|
| `r8kf-mission6k-a`, `-b`, `-perturb` (`tools/oracle/mission_test.sh --label r8kf-mission6k --ticks 6000 --perturb 3000 --copy-tick 4000`) | mission, 6,000 ticks | oracle mission run done | none (rc 0) | **MISSION TEST PASS**: a = b byte-identical (1,476,370 B, sha256 **`b97180de748702e5`**), ASan 0 both; perturb caught at 3000 / rng10; flipped copy at 4000 / ships_health |
| `r8kf-mission` | mission (default 3,000) | oracle mission run done | none | ASan 0; sha256 `570ffcae181a3b26` (3,000 ticks; equal to the audited stream's first 3,001 ticks) |
| `r8kf-console` | console | console self-test done | none | **PASS** (`kas=6`, `tablerows=486`, failures 0), ASan 0 |
| `r8kf-ai` | ai, 400 | ai self-test done | none | **PASS** `checks=16/16`, ASan 0; stream IDENTICAL to the audit's `r8au-ai-a` (401 ticks) |
| `r8kf-skirmish` | skirmish LiveFireExercise2, 600 | skirmish self-test done | none | **PASS** `done ticks=600 ships=89`, ASan 0; IDENTICAL to `r8au-skirmish-lfe2` (601 ticks) |
| `r8kf-hud` | hud | hud self-test done | none | **PASS**, ASan 0 |
| `r8kf-ui` | ui | ui self-test done | none | **PASS**, ASan 0 |
| `r8kf-sound` | sound | sound self-test done | none | **PASS**, ASan 0 |
| `r8kf-fe` | fe, 120 s | Main_game_screen | none: still running at the 120 s timeout | ASan 0, no leak, 0 fescreen UBSan reports |
| `r8kf-sp` | sp, `--timeout 340` | universe running, last univUpdateCounter **3,264** | none: still running at the 340 s timeout | ASan 0, no leak, 0 fescreen UBSan reports |

Earlier in this run on the pre-console-extension binary (comment-only differences elsewhere): `r8k-mission-before`
(base tree) and `r8k-mission-after`: both sha256 `6d8315ed3804895e`, 6,000 ticks; `r8k-ai`, `r8k-skirmish`, `r8k-hud`,
`r8k-ui`, `r8k-sound`, `r8k-console` PASS, `r8k-fe` Main_game_screen, `r8k-sp` universe running
(univUpdateCounter 3,264 at the 340 s timeout), all ASan 0.

**The extended self-test**: the console self-test (`/catConsoleTest`, harness mode `console`, Console.c
`catConTestTable`) now also runs six KAS lines through `cat_consoleKasCommand`, the generated table and dispatcher:
`VarCreateSet G_ConsoleTest 7` -> "KAS Ok", `VarGet` -> "KAS Returned 7", `VarInc` -> "KAS Ok", `VarGet` -> "KAS
Returned 8", `VarDestroy` -> "KAS Ok", `VarGet` -> "KAS Returned 0", and counts the table's rows to the `"*"` sentinel
(486). New log line `consoletest: kas lines=6 ok=6 tablerows=486 last='KAS Returned 0'`; the done line adds `kas=N`;
PASS needs `kas=6`. No new harness mode.

## 9. The Mission01 stream

- **Before** (base tree `15df4d6`, `r8k-mission-before`, 6,000 ticks): sha256 `6d8315ed3804895e`.
- **After** (final tree, `r8kf-mission6k-a` = `-b`): sha256 **`b97180de748702e5`**.
- Against producer 1's audited stream (`r8au-mission-a`, sha256 `245b3d88387ed076`): `python3 -I
  tools/oracle/hwo_compare.py <runs>/r8au-mission-a/run.hwo <runs>/r8kf-mission6k-a/run.hwo --from-tick 1 --to-tick 6000
  --fields rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real
  --json <runs>/r8kf-mission6k-a/compare-audit.json` -> "HEADER: same producer, different build identity: d190685e/1fbfa32f
  vs e3f05f47/1fbfa32f", **"IDENTICAL: 6000 ticks ... no difference"**; with all fields (no `--fields`): IDENTICAL over
  ticks 0-6000. The hashes differ only through the header's build identity (the commit), as the before / after pair
  did against the audit (`d190685e` vs `15df4d62`). **No oracle field moved**: no KAS runs under
  `CAT_HW1_MISSION_SCRIPTS 0`, and every changed non-KAS path (the damage factor offset, the invincible flag, the
  variable reads) is inert without a mission script.

## For STATE.md

- **Chosen names** (no exe name; the 486 kasf keep the exe table's own C names): `kasfScopeHash` (the timers' /
  variables' scope prologue, inline in 0x00491cf0 .. 0x004920e0); the predicates of `cat_kasShipsSelectByPredicate`
  0x00494f90: `kasfPredGuarding` 0x00495060, `kasfPredIdle` 0x004950f0, `kasfPredNotIdle` 0x00495110,
  `kasfPredClamped` 0x004951d0, `kasfPredNumber` 0x00495290, `kasfPredPlayer` 0x004952d0, `kasfPredSalvaging`
  0x00495310, `kasfPredShips` 0x00495430, `kasfPredInside` 0x00495480, `kasfPredUpgradable` 0x004954c0,
  `kasfPredEnemyNoStealthy` 0x004954f0, `kasfPredEnemy` 0x00495580, `kasfPredClass` 0x00495670, `kasfPredType`
  0x004956c0, `kasfPredCapital` 0x00495700, `kasfPredDocked` 0x00495770, `kasfPredNotDocked` 0x004957b0,
  `kasfPredDamaged` 0x004957f0, `kasfPredMoreThanDamaged` 0x00495860, `kasfPredMoving` 0x004958d0, `kasfPredFarther`
  0x004959f0, `kasfPredNearby` 0x00495aa0, `kasfPredLowFuel` 0x00497ea0, `kasfPredRace` 0x00498610,
  `kasfPredBeastified` 0x00498b00, `kasfPredSalvaged` 0x00498b50; `kasfDistanceSquaredBits` (the float-rounded squared
  distance the exe passes them), `kasfShipsCreateSome` (the shared body of kasfShipsCreate / Launch / Hyperspace),
  `kasfVectorClampToUniverse`, `kasfWideScreenInBody` (the body the exe has three times), `kasfWideScreenIsActive`
  0x00a43ff0, `kasfWideScreenCloseSensors` 0x00882104, `timTimerFind`, `timTimerLeft`, `CAT_SHIPFLAG_Invincible` (was
  `CAT_SHIPFLAG_Orphaned`, renamed from its setter), the `CAT_UI_*` ids of section 5, `catDamageFactor` (Ship +0x3E0).
- **Defaults**: the kasf take the exe's table signatures (hashes, not strings), so **HW1's compiled missions
  (src/Missions) are out of the build** (they could not compile against them and never ran under
  `CAT_HW1_MISSION_SCRIPTS 0`); the Tutor.c stand-ins of the Cataclysm tutorial buttons show nothing until producer 4;
  `cat_consoleKasCommand` does not free the strtok name (CAT-FIX); the AITeam layout is pinned for ILP32.

## What is left (owner)

- `cat_kasShipRemovedFromSelections` 0x004911e0 and its three `CAT-TODO(stage 6)` call sites in UnivUpdate.c; the KAS
  runtime (kasExecute, save / load, the hashed save format); the generated Mission01 / Tutorial1 code calling this
  library; `CAT_HW1_MISSION_SCRIPTS`' retirement and the src/Missions directory itself: **producer 2** (KAS runtime).
- The Tutor.c bodies behind the kasf (`cat_tutSetPointerTargetObj`, the next-text / previous / quit buttons,
  `cat_tutSelectShips`), the pointer's objective flag, the six manager sites of `cat_tutButtonsHideForScreen` /
  `RestoreAfterScreen`: **producer 4** (single player and the tutorial).
- src/SDL/utility_mac.c (not built) still names `tbStartup` / `tbShutdown` [audit: src/Win32/utility.c does not]: whoever next touches
  those reference copies (no build effect).
- The 486-row table is exercised by the console self-test only for the variable rows; the mission self-test of
  producer 2 exercises the Mission01 / Tutorial1 kasf at run time.
