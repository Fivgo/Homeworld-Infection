<!-- done: sp-hyperspace, sp-flow, sp-difficulty-save, tutorial, misc-verify, runs (r8p4-f2, all PASS, ASan 0), stream IDENTICAL -->
# M2-R8 producer 4: single player and the tutorial

Base commit `3903f02` (M2-R8 kas-runtime-mission01, audited). The work is `git diff 3903f02` in the m2 worktree (no new
source files; this report is new). No Ghidra run: the exe was read through the named export `cataclysm_gog_r6`
(tools/m2_fdec.py, tools/m2_fdis.py, tools/m2_exedata.py) and an `objdump -d -M intel` of `original/game/cataclysm.exe`
kept in `original/scratch-m2/r8p4/exe.dis` (gitignored, game-derived), with the decompiles read there
(`spgu.c`, `tutbtn1.c`, `tutbtn2.c`, `tutdtp.c`).

Subsystems (the done marker above):

1. **sp-hyperspace**: SinglePlayer.c's roll call, hyperspace out / in, the ether, the arriving fleet.
2. **sp-flow**: SinglePlayer.c's mission flow (init, lockouts, cleanup, the rest of `singlePlayerGameUpdate`, the
   end-of-mission callbacks, the warp start, the fades), the defeat flow after `univKillPlayer`.
3. **sp-difficulty-save**: the difficulty (`cat_spGameDifficulty`, `cat_spGetDifficultyModifiers`, kasLoad's rescale,
   the KAS skill level), the single-player save's mission tail through the SaveCat32 tables, the "01 - Hiigara" load
   (new self-test `loadgame`).
4. **tutorial**: Tutor.c (the flags' layout, the pointers, Cataclysm's buttons, save / load, start-up), the buttons
   around the managers, the objective pointer, the Tutorial1 leg of the mission self-test (three prompts and the Next
   button).
5. **misc-verify**: the uncalled HW1 sensors helpers, `cat_trPrecacheShipTextures`' caller, the KAS-hashed selection
   names and the AITeam.c tags (both found closed), the tags, the delta-map reviews, builds, runs, the stream.

## 1. sp-hyperspace (SinglePlayer.c)

The exe's jumping fleet is "a ship the player may take along (`cat_spShipCanHyperspace` 0x00517650, which holds HW1's
ally and disabled tests) that jumps by itself (a capital ship or a module, `CLASS_Component`) and is in range
(specialFlags 0x80)". The exe inlines this test in a dozen places; the tree now has it as two macros at the top of
SinglePlayer.c (`catSpCapitalOrComponent`, `ShipCanMakeHyperspace`) and HW1's `ShipCanHyperspace` (ShipSelect.h) is
retired. Each function, with its exe address:

- **`spAbortHyperspaceCB` 0x00517320**: Fleet Command's abort line by its Cataclysm id `0x4001021`; no auto-launch
  restore; the fleet halted, the roll-call screen deleted, `spUnlockout`, `playerCanHyperspace` set again (the exe
  inlines the same body in `singlePlayerGameUpdate` when the mothership is lost; the tree calls the function there).
- **`spGoNowHyperspaceCB` 0x005173a0**: `cat_kasWideScreenInHyperspace(90)`; the small ships dock at once
  (`CAT_DOCK_STAY_INSTANT`); the test is the class (not the type); no mothership door cleaning.
- **`cat_spEmergencyHyperspaceCB` 0x005174b0** (new; the third button "EmergencyHyperspace" of the callback table
  0x008943f0): letterbox; the ships that cannot jump stop (reason 0x2f) and are flagged left behind
  (`CAT_SHIPFLAG_LeftBehind`, catFlags18 0x40, name chosen: its other reader is `cat_meshCurrentMaterialAlt`
  0x004afcb0); the jumping ships but the mothership halt (0x23), steady and are flagged in range / in parade.
- **`CalculateRollCall` 0x005176e0**: counts the ships docked in any ship the player may take, every ship that does not
  jump by itself as remaining unless left behind, the capital ships (not the mothership, not the modules) as docked
  (in range) or remaining. HW1 counted every allied non-capital ship as remaining.
- **`TellHyperspacingShipsToReady` 0x00517770**: a `tResourceCollector` is healed to full (the exe's 0x58 test); the
  launch exception is for capital ships and modules (HW1's `MakeShipsNotIncludeLaunchingShips`, inlined, deleted).
- **`CalculateHyperspaceTimeForShip` 0x005178a0** (found modified): the random wait in the exe's x87 shape (the whole
  32-bit stream-10 draw times `RANDOM * 2^-32` plus BASE at the FPU's 53 bits, one rounding to float; HW1's `frandom`
  rounds the draw and each product to a float).
- **`AddShipToHyperspace` 0x00517a10**: the four trails cut; in single player the ship and its modules
  (`cat_moduleIterFirst` / `Next`) healed and refuelled, the ships inside it too.
- **`UpdateHyperspacingShip` 0x00517af0** (the whole body now; R6 had read one call): no hyperspace sounds (HW1 played
  `Ship_SinglePlayerHyperspace` / `Ship_Hyperdrive`; the failed jump's stop handle 0x0089469c is never started in the
  exe and stays -1); the fleet tests are the exe's (`catSpFleetAnyInHsState`); the mothership's collapse clears the
  white-out and sets it again once every other ship has finished.
- **`OrientHyperspacingShips` 0x00517f30**: the mothership keeps idling, the modules only stop idling, the capital
  ships turn / fly / take the range flag as HW1's.
- **`UpdateArrivingShip` 0x00518170** (read with the disassembly): the arrival angle in the exe's x87 shape (the sine
  of the 53-bit product, the cosine of the float angle); a multiplayer arrival clipped to the universe; the ship
  hidden (`SOF_Hide`) from its arrival until it decelerates in; the mothership and a module whose clamp host is itself
  arrive after 2 x BASE, the others after 3 x BASE + the random wait (x87); the camera's motion cues restart with the
  mothership (`cat_mcResetRequest`); no `Ship_HyperdriveOff`; the white-out runs down at 3 slice rates (x87); an
  arrived ship leaves the range flag with the hyperspacing one (`& 0xffdfff7f`); a multiplayer module waits on its
  host's telegraph.
- **`spHyperspaceSelectionOut` 0x00518940** (reviewed in R5 for one line): the call to **`cat_spPrepareShipHyperspaceOut`
  0x00518880** (new) was missing: before a ship hyperspaces out mid-level every object clamped to it lets go (a ramming
  frigate releases, a leech stops with reason 0x25, a salvager detaches, all but a module are unclamped; the walk
  restarts when the list changed size, as the exe's; CAT-FIX: it stops when the clamp list is gone).
- **`UpdateMidLevelHyperspacingShips` 0x00519700**: a ship that has hyperspaced out loses the references other ships
  hold to it (`cat_univRemoveObjReferencesFromShips`); an arrived ship leaves the range flag; HW1's wipe of the
  `DeleteAfterHSOut` ships moved to the end of the player's jump (as the exe).
- **`cat_spLaunchDockedShipsOnArrival` 0x005191a0** and **`cat_spForgetDamageEffects` 0x00519490** (new, see 2).
- **Compared, as the tree**: `spShipsRemainingDrawCB` 0x005175b0, `spShipsDockedDrawCB` 0x00517600 (`selGroupFont2` is
  the exe's 0x00c0bae0), `spNewShipSinglePlayerGameInfo` 0x00517870, `spHyperspaceSelectionIn` 0x005186c0,
  `spHyperspaceSelectionInStatic` 0x005187c0, `ShipAlreadyHyperspaceOut` 0x00518850, `singlePlayerShipDied` 0x005194d0.
- Deleted (HW1 helpers the exe inlines or does not have, no caller left): `TellShipsToAbortDockForHyperspace`,
  `TellShipsToDockForHyperspace`, `AreNotInParadeConstraint`, `MakeShipsNotIncludeLaunchingShips`,
  `HyperspaceRollCallBegin`, `CalculateHyperspaceTimesForShips`, `NoneDoing`, `AllDoingExceptMe`, `NoneDoingHS`,
  `UpdateHyperspacingShips`, `CalculateArrivingTimesForShips`, `UpdateArrivingShips`, `CleanupDamageEffects`,
  `CleanupHyperspacingShip(s)`, `CleanupHyperspacingShipsThatDidntHyperspace` (the ether cleanup the exe inlines is
  `catSpEtherCleanup`).

## 2. sp-flow (SinglePlayer.c, SinglePlayer.h, mainrgn.c, utility.c, HS.c, LevelLoad.c, InfoOverlay.c)

- **No mission sequence**: Cataclysm numbers its missions 1 .. 17 in `currentMission` (`singlePlayerInit` 0x00518c50
  starts at 1, `singlePlayerLoadNewLevel` adds one). GoK's `missionSequence`, its index, `convertMissionToSequenceIndex`,
  `spResetMissionSequenceToBeginning`, `spGetPreviousMission` / `NextMission` / `AdjacentMission` are deleted;
  `utySinglePlayerGameStart` 0x005aaf80 no longer resets a sequence; `spSetCurrentMission` sets the number.
  `singlePlayerLevelLoaded` (HW1's mission-10 carrier hack; the exe has no such function) is deleted.
- **`singlePlayerInit` 0x00518c50**: the warp records cleared and **Warp.script** read through the exe's table 0x00894350
  (`WarpEnabled`, `LevelToWarpTo`, then per `Level N` its `Resources`, `Ships <type>, <n>`, `Tech`: 19 records of 0x2F0
  bytes, `CatSpWarpLevel`, pinned on ILP32; the setters `cat_spWarpLevelSetCB` 0x00517060, `cat_spWarpResourcesSetCB`
  0x00517130, `cat_spWarpShipsSetCB` 0x005170b0, `cat_spWarpTechSetCB` 0x00517180 with the exe's fatal texts); the
  mission is the warp's or 1; `oldAutoLaunch` defaults to 0xF. The install's warp.script says `WarpEnabled 0`.
- **`cat_spWarpToLevel` 0x0051b9c0** (new; the warp start of `singlePlayerStartGame` 0x0051bdb0 after `tmTechInit` /
  `universeSwitchToPlayer`): an sMothership and the record's ships for player 0 in the parade, the modules clamped to
  the mothership, an `sCarrierSupport` to the first of up to eight sCarriers with a free `DPT_Support_EB` point, a
  support module placed at its host's next such point (turned round on every second), the research modules activated;
  the record's resources and technologies; every ship of the player into the ether; then `singlePlayerLoadNewLevel(0,
  1)` in `HYPERSPACE_LOADDONE`. CAT-FIX: the exe restarts its walk at the list's head after each ship and would loop
  forever on a ship already in the ether. HW1's warp (`WarpToLevelEnabled`, `WarpInFleet`, the `WarpFleet*` callbacks,
  `singleHackResearchWarp`, `GetFleetStrengthWarpScript`, `singlePlayerPreLoadCheck` and its call in `levelPreInit`,
  which the exe's `levelPreInit` 0x004a3230 does not have) is deleted. The HWI-ORACLE direct start keeps its place.
- **`spMainScreen` 0x00518d80** takes the exe's `closeSensors` argument (the 11 callers pass the exe's push: 1, but
  `kasfOpenSensors` 0x00496bc0 passes 0). **`spLockout` 0x00518db0 / `spMainScreenAndLockout` 0x00518e10 /
  `cat_spMainScreenAndLockoutKeepSensors` 0x00518e90 / `spUnlockout` 0x00518f00**: a lockout in force is lifted first;
  the deselect empties `selSelected` and refreshes the totals; the main-region lockout hides / shows the cursor;
  `spUnlockout` clears `smFleetIntel` (0x00a5b1b0) and the flags whole. Closes the kasf hand-off's tag.
- **`spHyperspaceButtonPushed` 0x00518f50**: not while the Pause key's frame step is on (`catUnivFrameStep`); refused
  with the red `strShipsStillUnderConstruction` while a ship of the player is under construction
  (`cat_univPlayerHasFlaggedShip`); the missions of `catSpMissionNextLevel` (0x00894424: 1, 2, 5, 7, 15, 17) jump in the
  letterbox at once (the fleet leaves 5 s of real time later); else Fleet Command's `0x200101f`, the jumping ships ready,
  the lockout, the roll-call screen, and **Go Now or Emergency Hyperspace disabled** (`RSF_RegionDisabled` on the atom)
  by whether a ship of player 0 was recently attacked or fired upon. GoK's end-of-mission resource collection is gone.
- **`singlePlayerGameUpdate` 0x005197d0** (the rest; R8 producer 2 had the NIS / nislet / KAS start): the letterbox jump
  (the fade in over the 5 s, the ships left in the ether wiped, the small ships docked at once, every ship of the list
  flagged in range first and the fleet put into the ether, the white-out with `SPEECH_HYPERSPACE_DELAY`); the roll call
  ending through `spGoNowHyperspaceCB`; Fleet Command's `0x2001020` and the 2.5 s wait; the ships left in the ether from
  before wiped when the jump begins; no "Game_BeginHyperspace" tutorial message, no `Ship_SinglePlayerHyperspace`; the
  inlined fleet test; the white-out plays the animatic `(cur, cur + 1)` (`animBinkPlay`; mission 17 ends the game with
  the credits); once no movie plays the ether's ships forget their damage effects (`cat_spForgetDamageEffects`,
  recursive into the ships inside), the `DeleteAfterHSOut` ships go (and are taken out of their hosts); then the next
  level loads, or, for a mission entered from its NIS, its opening NIS plays in its staging level (state 8,
  `HYPERSPACE_FAILED`'s value, no case); **the at-once arrival** of the `catSpMissionArrivesAtOnce` missions (each ship at
  its offset turned by the start point's angle, out of the ether; player 0's ships steadied into the parade with
  reason 0x26 and their docked ships launched by `cat_spLaunchDockedShipsOnArrival` 0x005191a0; the black fade
  restarted; the arrival ends with the fade); the failed jump returns through case 10 (`HYPERSPACE_CATFAILEDRETURN`),
  reached only by falling through, as in the exe. CAT-FIX: the at-once arrival deletes the ship's own node (the exe
  deletes the list's head, the same node unless a non-allied ship came first). Closes the at-once tag.
- **`singlePlayerGameCleanup` 0x00519510**: `objectiveDestroyAll` (HW1 `objectiveShutdown`), the exe's ether tests, the
  black fade 0 and `catSpBigGunAllowed` 1 at the end.
- **`singlePlayerPostInit` 0x005193d0**: `animBinkPlay(0, 1)` (HW1's; GoK went through its sequence).
  **`singlePlayerClose` 0x005193f0**: the `soundEventSetActorFlag` call the tree still had is gone (its R2 review said so).
  **`singlePlayerSetMissionAttributes` 0x00519440** (found modified): player 0's race 0 and player 1's race 1 before
  the mission's script. **`SetOnMissionCompleteInfo` 0x0051ab50** (found modified): `NOHYPERSPACE` compared without case.
  **`singlePlayerMissionFailedCB` 0x0051be70**: Fleet Command's `0x2001028` first. **`singlePlayerMissionCompleteCB`
  0x0051be40**: compared, as the tree.
- **The fades in `mrRegionDraw` 0x00569260** (mainrgn.c): the black level fade (`catSpLevelFade`, not while an NIS
  plays, 0x00a4cabc = `nisIsRunning`) and Mission01's white flash (`catSpWhiteFlash`, additive) as full-screen quads
  between the NIS fade and the white-out (the corners through this tree's pixel centre, decision 29). Closes the tag.
- **`hsStaticGateRender`** reads 0x00a6ad78 (`catSpFleetArriving`), not HW1's `singlePlayerHyperspacingInto` (deleted).
- **The hyperspace objective's row** (`cat_moObjectivesDraw` 0x004a76b0, InfoOverlay.c) prints `cat_spMissionFlagValue`
  ("next level" / "hyperspace"). Closes the tag.
- **The defeat flow after `univKillPlayer`** (checked, nothing to change): `singlePlayerMissionFailedCB` ->
  `univKillPlayer(0, 2, -1)` -> `CheckPlayerWin` 0x005485e0 sets the quit / win times (real time) -> `univUpdate`
  0x0054c3f0 brings up "SP_Game_Over" in a single-player game (`cat_utyGameQuitToMainOrStats` only outside one), as the
  tree has since M2-R6. The KAS end of a mission (`kasfMissionCompleted` 0x00491c70 / `kasfMissionFailed` 0x00491cb0) is
  the kasf library's and calls these callbacks.
- **`cat_trPrecacheShipTextures`' caller** (checked): `singlePlayerGameUpdate` 0x00519a96 calls it when Mission01's
  nislet starts; producer 2's call is that; texreg.c's note updated.

## 3. sp-difficulty-save (SinglePlayer.c, utility.c, KAS.c, KASFunc.c, SaveGame.c, the SaveCat32 tables, the new self-test)

- **`cat_spGetDifficultyModifiers` 0x00516f90** (new): the difficulty 0..4 gives the Somtaaw health factor (6, 3, 1,
  0.85, 0.7; `spDifficultyHealthModifier` 0x00894194), the build-time and the build-cost divisors (4, 2, 1, 1, 1;
  `catCmSinglePlayerBuildFactor` 0x00894198, `catCmSinglePlayerCostFactor` 0x0089419c, whose readers ConsMgr.c has);
  Ghidra typed the integer outputs as floats.
- **`cat_spGameDifficulty` 0x0051c140** (new, `"SP_GameDifficulty"` in utility.c's callback table): the radio's first
  call sets the button of `catSpDifficultySetting` (0x008941a0) and redraws, a click takes the button's pData; both set
  the modifiers.
- **kasLoad 0x00491450**: the modifiers follow the current difficulty and `spDifficultyHealthLoadFactor` (0x008941a4,
  FixShip's) is the current health factor over the saved difficulty's (CAT-FIX: a bad saved difficulty leaves the
  divisor 1, the exe divides by an uninitialised local).
- **The KAS skill level is the difficulty word**: `kasfMissionSkillSet` / `Get` 0x00493b80 / 0x00493b90 write and read
  0x008941a0 in the exe. The tree had HW1's separate `CurrentMissionSkillLevel` (0, deleted), which made
  `cat_shipGetDamageMultiplier` 0x005148f0 double every single-player ship's damage and changed
  `cat_salvageTargetDamagedEnough` and the loading screen's skill text. (The Mission01 oracle stream has no damage in
  its 6,000 ticks: unchanged, section 8; the mission self-test's combat timing moved, section 7.)
- **A cataclysm.exe single-player save's mission tail**: the mission number is Cataclysm's 1 .. 17 = this tree's
  MissionEnum (SaveGame.c `LoadPreGameInfo`; closes its tag); the KAS label entries (`KasSelection` 0xC, `LabelledPath`
  / `LabelledVector` / `LabelledVolume` 8: `kasLoadEntry`), the objectives (0x34) and fleet intelligence (0x18,
  `objectiveLoad` 0x004d6960 / 0x004d68a0), the static hyperspace gates (0x30 each, `LoadHyperspaceGates` 0x00490510)
  and the pings (0x58 head + the user data, `pingLoad` 0x004e9790) are converted through the SaveCat32 tables; the
  timers (`Timer` 0x18, no pointer), the paths, the volumes and the trade manager's block are the same on both ABIs.
  **Generator**: `tools/m2_save32_gen.py` has the eight new roots (`KasSelection`, `LabelledPath`, `LabelledVector`,
  `LabelledVolume`, `Objective`, `FleetIntelligence`, `hsStaticGate`, `ping`; headers KAS.h, Objectives.h, HS.h, Ping.h,
  also included by SaveCat32.h); rerun `python3 -I tools/m2_save32_gen.py` (writes src/Game/SaveCat32Layouts.c: 119
  layouts, 45 ship specifics, the 3 known union conflicts) and `--check`. `savegame_probe.py` on the save first:
  `OK sp=1 mission=1 players=2; chunks=2354 ... ship_chunks=167 min_ship=0x63c`.
- **New self-test `loadgame`** (`/catLoadGameTest <ticks> /catLoadGameName <name>`, MissionSelfTest.c; harness mode
  `loadgame`, options `--save NAME` (default "01 - Hiigara") and `--load-ticks N` (default 400); documented in the
  script header): the save is copied from `original/game/SavedGames/SinglePlayer` into the run's settings dir's
  `SavedGames/SinglePlayer` only (where the engine reads saved games; never into the harness data dir, whose SavedGames
  stay empty, and never written in original/game: its sha256 `73f52d0766d309fc` unchanged); once the main screen is up the engine loads it as the front end's Load Game does (`gpLoadGivenGame`)
  and runs the ticks on the mission run's virtual clock (the oracle stream with `/oracleDump`), no sound or speech.
  Lines `loadgametest: load ...`, `loadgametest: loaded ...`, `loadgametest: status ...`, the KAS FSMs' and objectives'
  `missiontest:` lines, `loadgametest: done ... result=PASS|FAIL`, rc 0. PASS: a single-player game in Mission01, the
  player's mothership, the KAS watch function restored, KAS script teams running, the player alive at the end.

## 4. tutorial (Tutor.c, Tutor.h, LaunchMgr.c, ResearchGUI.c, ConsMgr.c, Universe.c, InfoOverlay.c, mainrgn.c)

- **The flags in the exe's layout**: Cataclysm has 54 tutorial flags (name table 0x008ab74c): `Waypoint`, `WaypointIssue`
  after `MoveIssue` and `ManagersLocked` at the end are new; a flag's bit is its index (word index >> 5). The bitfield
  and the name table now match, so the managers' and the main region's raw bit tests (`catMrTutEnabled`,
  `catRmTutEnabled1/2`, `catCmTutEnabled`, the MothershipFocus test) read the right flags. `tutGameEnableFlags` is
  pinned at 8 bytes and `tutpointer` at 0x68 on ILP32 (tools/m2_layout32.sh now includes Tutor.h). Closes 5 tags. [audit: the name table did not match: inserting `Waypoint` / `WaypointIssue` had replaced `"Attack"` (the exe's index 13) instead of going before it, so `tutGameEnableString` had 53 names against the struct's and the exe's 54 and every flag from Attack on (`Harvest` .. `ManagersLocked`) set the bit below its own (e.g. `MothershipFocus` set bit 47, FocusCancel's); fixed by the audit: `"Attack"` restored after `"WaypointIssue"` and a static assert on the table's 54 + 1 entries]
- **The pointers**: the objective flag of the Ship / ShipSelection / AIVolume setters is kept (+0x64,
  `tutSetPointerTargetShip` 0x00530450 ..), `tutRemoveAllPointers` 0x00530a20 keeps those pointers, names compare
  without case (`_stricmp`: `tutRemovePointerByName` 0x005309a0, the inlined allocation); `tutSetPointerTargetXY`
  0x0052ff70 scales the 640 x 480 point to the window (its R6 review said so; the tree had not stored it) and the
  Right / BottomRight / Taskbar / FE setters place theirs unscaled; **`cat_tutPointerListReset` 0x0052f890** (new; called
  by `universeReset` 0x0053df80, `tutStartup`, `tutLoadTutorialGame`), **`cat_tutObjPointerXYSet` 0x00531290** (new;
  `cat_moObjectivesDraw` gives the objective list's right edge and top every frame) and **`cat_tutSetPointerTargetObj`
  0x00530060** (the kasf stub replaced: an XY pointer at that point). **`tutDrawTextPointers` 0x005312b0** (found
  modified): the pointers' colour (red a quarter, green the pulse, blue two thirds), the box's top under the NIS
  letterbox, an objective pointer drawn only in the sensors manager (not zooming, the objective-pointers option on, no
  letterbox) with its line from the objective list.
- **The buttons** (each a button region under the tutorial's root, `catTutButtonCreate` [audit: it kept HW1's `sizeof(buttonhandle) - sizeof(regionhandle)` = 0 extra bytes, so `processFunction` lay past the region's block (the exe passes 0x1c); now `sizeof(uicbutton) - sizeof(region)`]; atoms "TutorialNextText",
  "TutorialPrev", "TutorialQuit" added): Next (the next lesson's: width - 192 in the button row, 128 x 32, the
  next-lesson art; `tutShowNextButton` 0x00532010 and `tutDrawNextButtonFunction` 0x00532820 found modified;
  `tutProcessNextButton` 0x00532750 counts a click only with the main view or the sensors manager up), previous lesson
  (`tutShowPrevButton` 0x00532300: its own region at width - 320, the restart art; `cat_tutHidePrevButton` 0x005326b0,
  `cat_tutPrevButtonClicked` 0x00532aa0), quit (`cat_tutShowQuitButton` 0x005324a0 at the right edge, 64 x 32; its click
  leaves for the main menu, `cat_tutProcessQuitButton` 0x005327e0), next text (`cat_tutShowNextTextButton` 0x00532190
  at the text box's left; its click also runs key command 0x6b, `cat_tutProcessNextTextButton` 0x00532780), back
  (`tutDrawBackButtonFunction` 0x00532d80 found modified: always the restart art). The row is the text box's bottom
  (0x00a6bc14, `tutTextRect.y1`): `catTutButtonRowY` was that word, so `cat_tutButtonRowSet` 0x00532480 now writes
  `tutTextRect.y1`. The perspective-correction hint of the draws is not issued (render.c's rule). Closes the kasf
  stubs' tag.
- **`cat_tutButtonsHideForScreen` 0x005341f0 / `cat_tutButtonsRestoreAfterScreen` 0x00534300** (new): the construction,
  launch and research managers hide the Next, previous and quit buttons when they open (`cmConstructionBegin`
  0x004586a6, `lmLaunchBegin` 0x0049e50b, `rmResearchGUIBegin` 0x004f7575) and show the ones that were up when they close
  (`cmClose` 0x00455780, the launch and research closes). Closes 6 tags.
- **Save / load** (`tutSaveTutorialGame` 0x0052f750 found modified, `tutLoadTutorialGame` 0x0052f8b0): the previous,
  quit and next-text buttons' visible flags are saved too; a load shows the saved buttons again, clears the clicks,
  frees the pointers. **`tutSaveLesson` 0x0052f6d0** (found modified): a comma of the lesson's name becomes a space
  (CAT-FIX: on a copy; the exe edits the KAS literal). **`tutStartup` 0x00533140** resets the pointers;
  **`tutInitialize` 0x00533190** loads Cataclysm's 35 images per language (the five lists generated from the exe's tables
  0x008ab47c ..; HW1 had 32); **`tutUnInitialize` 0x005332c0** also hides the three new buttons, clears the trade
  manager's technologies and drops the KAS watch function. **`tutGameMessage` 0x00533e20** (found modified): only in
  the tutorial (tutorial 1), every message while there is room (GoK queued in any single-player game, skipping
  duplicates). **`tutRootDrawFunction` 0x00530b40** (found modified): the manager names compared without case.
- **Compared, as the tree**: `tutPreInitTutorial`, `tutTutorial1`, `tutSetTextDisplayBox`, `tutAllocateRootRegion`,
  `tutShowText`, `tutHideText`, `tutGetNextTextLine`, `tutClipSegToTextBox`, `tutHideNextButton`, `tutNextButtonClicked`,
  `tutShowBackButton`, `tutHideBackButton`, `tutShowImages` (the list), `tutDrawImageFunction`, `tutHideImages`,
  `tutSetEnableFlags` / `tutBuilderSetRestrictions` / `RestrictAll` / `RestrictNone` (the exe inlines the tokenizer),
  `tutSelectedContainsShipTypes`, `tutBuildManagerShipTypeInBatchQueue`, `tutBuildManagerShipTypeSelected`,
  `tutCameraFocusedOnShipType`, the text draw (0x00531f20: the box `colRGBA(0, 0, 64, 192)`, the text `colRGB(255, 255,
  128)`).
- **The Tutorial1 leg of the mission self-test** (`/catMissionTestTutorial`, `--tutorial`): Tutorial1's script starts,
  the SpeechEvents FSM puts up the Next, previous and quit buttons, the Tutorial FSM reaches Lesson1 and its two
  objectives, the lesson's **first three prompts** show (each a new subtitle text: "LESSON 01 - Start of Tutorial", then
  the narration of speech events 0x80012d3 and 0x80012d4), the objective list's pointer "ptr" is set, then the Next
  button is clicked through its region's own click callback (no input) and the Tutorial FSM must reach Lesson2. Lines
  `missiontest: tutorial buttons|Lesson1|objectives|prompt N|objective pointer|Lesson2 ...`, `missiontest: done ...
  tutorial ... result=PASS|FAIL`. (A test-only helper `tutPointerIsSet`, HWI-TEST.)

## 5. misc-verify

- **The uncalled HW1 sensors helpers**: `smClickedOnPlayer` and `smPlayerNamesDraw` are deleted with their prototypes
  (cataclysm.exe has neither; nothing called them since the M2-R6 sensors port; the unbuilt src/Win32/mainrgn.c still
  names one), and HW1's commented-out `smEyeFactorGet` / `smLookFactorGet` are removed.
- **The KAS-hashed selection names**: found done by the kasf library producer (every KAS label is its hash: KAS.h,
  KAS.c's lookups by hash, the level loader hashing the .mission labels); the two **AITeam.c KAS tags** were closed by
  the same producer (`grep -n CAT-TODO src/Game/AITeam.c` -> none).
- The prose "stage 6" notes of my files updated (ShipSelect.h, SinglePlayer.c, Sensors.c, UnivUpdate.c 9618, texreg.c).

## 6. Builds

`tools/build.sh --debug` and `--release`: rc 0, 0 compiler warnings under -Werror. `tools/m2_layout32.sh`: OK (Tutor.h
added: `tutpointer` 0x68, `tutGameEnableFlags` 8). `python3 -I tools/m2_save32_gen.py --check`: see section 7.

## 7. Runs (plain harness)

All with the final debug (ASan + UBSan) binary through `tools/m2_harness.sh` (silent, headless), the runs in
`original/scratch-m2/m2r2-harness/runs/` (gitignored), labels `r8p4-f2-*` (the same batch `r8p4-f1-*` before the last
two fixes below gave the same results; the tutorial leg's prompt ticks moved with them). [audit: the third prompt's tick is not reproducible run to run, fixes or not: the tutorial leg keeps speech on (SDL's dummy driver, the audio thread's wall clock); the audit's runs of one binary gave different ticks (docs/audits/m2-r8-singleplayer-tutorial.md); the Tutorial1 leg PASSes every time but its stream is not byte-identical between runs]

| Run | Milestone / first stop | ASan | Result |
|---|---|---|---|
| `tools/oracle/mission_test.sh --label r8p4-f2-mission6k --ticks 6000 --perturb 3000 --copy-tick 4000` | rc 0, 9 PASS lines | 0 | a = b sha256 `00ae16e7f73689d9` (section 8) |
| `r8p4-f2-mt-a` / `-b` (`--mode missiontest`) | mission self-test done / none (rc 0) | 0 / 0 | `ticks=11424 objectives=6/6 result=PASS`, a = b sha256 `35d612581c658119` (byte-identical) |
| `r8p4-f2-tut` (`--mode missiontest --tutorial`) | mission self-test done / none (rc 0) | 0 | `kas=1 lesson1=2 objectives=2 buttons=1 prompts=7/102/1152 pointer=1 next=1152 lesson2=1154 result=PASS` |
| `r8p4-f2-load-a` / `-b` (`--mode loadgame --load-ticks 3600`, "01 - Hiigara") | save-load self-test done / none (rc 0) | 0 / 0 | `from=1985 to=5585 mission=1 ships=140 objectives=0/1 kasTeams=36 result=PASS`, a = b sha256 `b9ba66b8a1638bc2` |
| `r8p4-f2-ai` | ai self-test done / none | 0 | `checks=16/16 result=PASS`, sha256 `a88770b54df64d4e` (= audited) |
| `r8p4-f2-sk` (LFE2) / `-sk8` (Armed8) | skirmish self-test done / none | 0 / 0 | PASS, `7e135b2daee5d99e` / `1d6d27adc0a79ca5` (= audited) |
| `r8p4-f2-dock`, `-command`, `-render`, `-hud`, `-ui`, `-console`, `-build` | each self-test done / none | 0 each | PASS each (build `tick=4205 SU=40/100 RUs=25925`) |
| `r8p4-f2-fe` (`--mode fe`, 120 s) | Main_game_screen / still running at the timeout | 0 | front end up |
| `r8p4-f2-sp` (`--mode sp`, NIS-first start, 360 s) | universe running / still running at the timeout | 0 | `debugScreenshots univUpdateCounter: 4832`, last screenshot `golden-01-04832.jpg`: Mission01 running (>= 1,248 at 340 s), no KAS save written |
| `tools/oracle/selftest.sh --label r8p4-f2-oracle-rel --bin build/release/hwi-engine` | rc 0 | - | `SELFTEST PASS` |

No LeakSanitizer leak in any run. Generators: `python3 -I tools/m2_save32_gen.py --check` and `python3 -I
tools/m2_kas2c_gen.py --check` current; `tools/m2_layout32.sh` OK. `grep -rn "CAT-TODO(stage 6" src/` -> 0.

Fixes found by these runs (in the tree):

- **UBSan**: `tutSetFlagIndex` shifted a signed 1 by 31 for the flags' top bit; now an unsigned shift (the exe's `shl`).
- **sp went on to Mission02** (no generated script yet): render.c's `debugScreenshots` next-level step now runs only
  when `cat_spKasScriptGenerated(spGetCurrentMission() + 1)` (HWI test hook; part B's missions turn it on).
- **The harness's loadgame copy**: the first version put the save into the harness data dir, where other modes' KAS
  saves overwrote it (the mission runs then lacked "01 - Hiigara"); the copy now goes into the run's settings dir only
  and the data-dir copy is deleted (original/game untouched, sha256 `73f52d0766d309fc`).
- **The mission self-test stream moved** against producer 2's `b03fdee2b85d36a0` (an expected change, not the Mission01
  stream): first divergence at tick 1772, the game RNG [audit: stream 0, rng0: `cat_mcResetRequest` draws `ranRandom(RANDOM_STREAM_0)`] 300 draws ahead, from `UpdateArrivingShip`'s exe motion-cue reset
  (100 points x 3 per arriving ship); the later fields follow. Still PASS 6/6, byte-identical a = b: objectives
  created / met DestroyBombers 4768 / 4800, ExploreRegion 6864 / 8096, DestroyResourcers 8208 / 8752, ProxAttackers
  9760 / 10240, Destroyer 10816 / 11296, Hyperspace 11328, the jump 11392.

## 8. The Mission01 stream

- **Before** (the base tree `3903f02`, `r8p4-mission-before`, 6,000 ticks): sha256 `00ae16e7f73689d9`; against producer
  3's audited stream `r8k2a-final6k-a` (sha256 `19560d16033efd53`): `python3 -I tools/oracle/hwo_compare.py
  <runs>/r8k2a-final6k-a/run.hwo <runs>/r8p4-mission-before/run.hwo --from-tick 1 --to-tick 6000 --fields
  rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real
  --json <runs>/r8p4-mission-before/compare-audit.json` -> **IDENTICAL: 6000 ticks**; the header differs only in the
  build identity (7d225de4 vs 3903f026).
- Interim checks after subsystems 1 and 3 (`r8p4-a-mission`, `r8p4-c-mission`): IDENTICAL to the audited stream, the
  same command.
- **After** (the final tree, `r8p4-f2-mission6k-a` / `-b`, 6,000 ticks): sha256 `00ae16e7f73689d9` both,
  **byte-identical** to the before run; the same compare against `r8k2a-final6k-a` -> **IDENTICAL: 6000 ticks and 0
  packets compared, no difference** (header: build identity only). Also IDENTICAL with every field (r8p4-f1).

## 9. Delta-map review entries (decision 55)

98 entries carry `[review: M2-R8 singleplayer-tutorial]` in ghidra/delta_map_review.json (66 new, 32 existing ones
appended; the file's order and encoding kept), merged with `python3 -I tools/delta_map.py merge --done
m2r8:singleplayer-tutorial` and `delta_map.py report` (docs/m2-r1-function-deltas.md). Counts: IDENTICAL 2,785 -> 2,773,
MODIFIED 1,552 -> 1,564, reviewed 3,274 -> 3,340. **Found modified (IDENTICAL -> MODIFIED), 12**:
`CalculateHyperspaceTimeForShip` 0x005178a0, `singlePlayerSetMissionAttributes` 0x00519440, `SetOnMissionCompleteInfo`
0x0051ab50, `tutSaveLesson` 0x0052f6d0, `tutSaveTutorialGame` 0x0052f750, `tutRemovePointerByName` 0x005309a0,
`tutRootDrawFunction` 0x00530b40, `tutDrawTextPointers` 0x005312b0, `tutShowNextButton` 0x00532010,
`tutDrawNextButtonFunction` 0x00532820, `tutDrawBackButtonFunction` 0x00532d80, `tutGameMessage` 0x00533e20. Every
row of SinglePlayer.c and Tutor.c is reviewed now (and of KASFunc.c, KAS.c, Timer.c, LevelLoad.c, Objectives.c). Stage
6 after the merge (`python3 -I tools/m2_stage_counts.py`): 289 IDENTICAL / 180 MODIFIED / 25 REWRITTEN HW1-matched.

## 10. CAT-TODOs

Closed (24 [audit: 25: the base had 25 `CAT-TODO(stage 6` lines and all are gone; single player is 11, as its own list counts], every `CAT-TODO(stage 6` in src/: `grep -rn "CAT-TODO(stage 6" src/` -> **0**): single player 10 (KAS.c
the rescale; SinglePlayer.c the difficulty factor, the fades, the lockout hand-off, the at-once arrival, the warp
start; SaveGame.c the mission number; utility.c the SP_GameDifficulty entry, the difficulty readers, the sequence
reset; InfoOverlay.c the mission flag text), tutorial 14 (LaunchMgr.c 2, ResearchGUI.c 3, ConsMgr.c 3, mainrgn.c 2, the
flags' layout; Universe.c the pointer list; InfoOverlay.c the objective pointer; Tutor.c 2: the flag in the pointer, the
kasf stubs). Re-targeted: none. Added: none (the CAT-FIX notes are in section 1, 2, 3, 4). Left of part B: producer 2's
`CAT-TODO(M2-R9 part B)` for missions 02-17's generated code and the "Mine" / "Slipgate" mission-sphere readers.

## For STATE.md

- **Chosen names**: `catSpCapitalOrComponent`, `ShipCanMakeHyperspace` (the exe's test; HW1's `ShipCanHyperspace`
  retired), `CAT_SHIPFLAG_LeftBehind` (catFlags18 0x40), `catSpEtherCleanup`, `catSpFleetAnyInHsState` /
  `catSpEtherAnyInHsState`, `catSpFailedJumpSoundHandle` 0x0089469c, `catSpMissionNextLevel` 0x00894424 (moved; was the
  popup's), `catSpWideScreenJumpTime` 0x00a6adb4, `catSpFleetCommandTime` 0x00a67488, `CatSpWarpLevel` /
  `catSpWarpLevels` 0x00a67498, `catSpWarpEnabled` 0x00a67490, `catSpWarpLevelTo` 0x00a67494, `catSpWarpLevelParsing`
  0x0089434c, `catSpWarpScriptTable` 0x00894350, `CAT_SP_DEG_TO_RAD` (0x0072a4f8), `catTutNextTextVisible` 0x00a6c4f0,
  `catTutQuitVisible` 0x00a6c508, `catTutObjPointerX/Y` 0x00a6c51c / 0x00a6c520, the saved button flags 0x00a6c524 ..,
  `CAT_TUT_POINTER_COLOUR`, the tutorial image indices `TUT_NEXTLESSON_*` / `TUT_QUIT_*` / `TUT_REST_*` / `TUT_NEXT_*`,
  the flags `bWaypoint`, `bWaypointIssue`, `bManagersLocked` (the exe's names), `tutpointer.objective`; the exe's names
  `cat_spEmergencyHyperspaceCB`, `cat_spPrepareShipHyperspaceOut`, `cat_spLaunchDockedShipsOnArrival`,
  `cat_spForgetDamageEffects`, `cat_spGetDifficultyModifiers`, `cat_spGameDifficulty`, `cat_spWarpToLevel` and its four
  setters, `cat_tutPointerListReset`, `cat_tutObjPointerXYSet`, `cat_tutButtonsHideForScreen` /
  `RestoreAfterScreen` and the button functions.
- **Defaults**: no mission sequence (the mission number is Cataclysm's 1 .. 17); the KAS skill level is the
  difficulty word; the warp start follows the install's warp.script (off); the tutorial test clicks the Next button
  through its click callback (not input); the save-load test copies the user's "01 - Hiigara" from the install into the
  run's settings dir only; CAT-FIX: the warp's ship walk, the at-once arrival's node, the clamp release walk, the lesson name
  copy, kasLoad's bad saved difficulty.

## What is left (owner)

- **M2-R9 part B**: missions 02-17's generated code (producer 2's tag) and their screens; the warp start and the
  at-once arrival / letterbox jump are exercised only by Mission01's own path so far (Mission01 is a
  `catSpMissionNextLevel` mission: its jump is the letterbox one).
- **Unexercised**: the warp start (warp.script is off in the install), a tutorial save's load (no tutorial save
  exists), the Emergency Hyperspace button (needs a roll call under fire).
