<!-- done: statusbars, infopanel, overlay, pie-misc, final -->
# M2-R5 producer 5: HUD (status bars and picking, ship info panel, overlays, pie plate, game-stats screen)

Status: **done** (one deferral, the multiplayer/skirmish game-stats screen; see "Leftovers"). Branch `m2`, worktree
`.claude/worktrees/m2`, base commit `a47a8a6` (nothing committed by this producer: the audit checkpoints). Scope:
docs/m2-r4-summary.md "Plan for M2-R5 and M2-R6" row 5. All bodies follow the r6 export
(`ghidra/exports/cataclysm_gog_r6`, `tools/m2_fdec.py` / `tools/m2_fdis.py`); every port site carries a
`CAT: ... (<exe function> 0x00xxxxxx)` comment. Ghidra was not run.

Subsystems (the done marker above lists the finished ones):
- `statusbars`: `Select.c` status bars, the overlay icons, the picking API.
- `infopanel`: the ship info panel (`cat_shipPrint*`), the ship names (`cat_shipNames*`), `ShipView.c`.
- `overlay`: `InfoOverlay.c` (`ioStartup`, `ioUpdateShipTotals`, `cat_io*`, the message scroller), the main overlay
  `cat_mo*`, `Tactical.c`.
- `pie-misc`: `PiePlate.c`, `TaskBar.c`, `Scroller.c`, `GamePick.c`, `HorseRace.c` (the game-stats screen: deferred).
- `final`: `/catHudTest` + harness mode `hud`, builds, harness runs, delta-map merge, this report.

## DONE WHEN

| criterion | result |
|---|---|
| `/catHudTest` (harness mode `hud`) selects Mission01's ships through the selection API and draws status bars, info panel, overlays and pie plate for N frames offscreen, ASan 0 | **met**: `r5h-d-hud` (600 frames), `result=PASS`, ASan 0, no leak (counts below) |
| the 21 UI cat-new of the R4 tool move ported | **met**: all 25 r6 functions of the rule `^cat_(shipPrint\|selStatus\|selHealth)` (the 21 of R4 + 4 the r6 rename added) are defined in `src/` |
| fe and sp unchanged | **met**: same milestones, ASan 0, the same UBSan kinds and counts as base; the only difference is the new start-up data (51 loads: 48 overlay icons, `OverlayIcons.script`, `MainOverlay.script`, `shipnames.script`) and, in sp, the 10 load-screen `ring.lif`/`arrows.lif` loads of GoK's blinking ring, which Cataclysm does not have |
| mission stream unchanged | **met**: `tools/oracle/mission_test.sh` 6,000 ticks: a and b byte-identical, perturbed run and flipped copy caught, and a vs base `r5s-aud2-m-a` IDENTICAL (6,001 ticks, only the build identity differs) |

## Files

New (REULA header as their siblings, added to `src/Game/meson.build`):

| file | what |
|---|---|
| `src/Game/OverlayIcons.c/.h` | Cataclysm's 53 overlay icons (exe table 0x0088cb48, `CatOverlayIcon` 0x20, `CatOverlayIconTexture` 0x108, ILP32 asserts): `OverlayIcons\OverlayIcons.script` (`cat_feOverlayIconTextureSetCB` 0x004dbda0, `cat_overlayIconTextureCreate` 0x004dbb70), `cat_overlayIconsStartup` 0x004dbf80, `cat_overlayIconsDiscardGL` 0x004dbfa0, `cat_overlayIconDraw` 0x004dbfe0 |
| `src/Game/ShipPrint.c/.h` | the ship info panel: `cat_shipPrintLine/Tech/Armour/Guns/KeyHelp/Toggle/Draw/Startup/Reset/HealthColor/DragStart/DragEnd/DragUpdate/IsDragging/Active` 0x0050f910..0x00511720 (the 26-row tech table 0x008937e0), `cat_shipExperienceLevelName`, `cat_shipWeaponTypeUsable` |
| `src/Game/ShipNames.c/.h` | `shipnames.script` names of the local carriers and motherships (`CatShipName` 0x70 x 100): `cat_shipNamesSetCB/Init/ShipIsNameable/Assign/CountFree/Rename/GetFreeName/Release/GetName` 0x0050ee70..0x0050f810 |

Changed (main points; addresses on each site):

| file | change |
|---|---|
| `Select.c/.h` | status bars (`cat_selStatusRectCompute`, `cat_selStatusBarDraw`, `cat_selHealthColorsGet/FactorGet`, `cat_selStatusUpgradePendingDraw`, `IconsDraw`, `HotKeyDraw`, `AmmoCountDraw`, `StealthDetectDraw`, `cat_selStatusBarsDraw`, `selStatusDraw0-4`); `selSelectedDraw`, `selSelectingDraw` (nearest-pixel rounding); the picking API at the exe bodies: `selRectDragFunction(..., centrePoint)`, `selRectDragAddFunction`, `selSelectionClick(..., pickNothing)`; FOW pick helpers `cat_selPositionHiddenByFOW` / `cat_selShipSelectableInFOW` (no grid until M2-R6) |
| `InfoOverlay.c/.h` | rewritten: `ioStartup` (MainOverlay.script print options), `ioUpdateShipTotals` (own/allied/enemy lists with the mimic tests), the main overlay `cat_mo*` (print options, selected/selecting lists, objectives, tooltips, support units, game messages, player names, construction, research, buttons, `cat_moRender`), `cat_ioClick`, `cat_ioUpdateHover`, objectives, the message scroller (`cat_ioMessageAdd` 0x004ac940, `cat_ioMessageScroll`, `cat_ioCommandMessage` = `clCommandMessage` 0x004ac820's body), mission timer (`cat_missionTimerReset/Save/Load`), `cat_ioRemoveShipReferences`; the HUD self-test |
| `Tactical.c/.h` | `toAllShipsDraw` per exe, `toLegendDraw(y)` |
| `PiePlate.c/.h` | [audit: `cat_pieCompassDraw` skipped the colour 0xff7fffff; the exe skips 0xff000000, fixed] `piePointModeOnOff(mode, callback)` (callers that only turn it off pass mode 0 as the exe), `cat_piePointSpecExecute`, `piePlaneDraw` + `cat_pieCompassDraw`, `cat_smHeightCircleColorGet`, the box `pieMovePointClipToLimits`, `piePointSpecDraw`, `pieDistanceReadoutDraw`, `pieShipDied`, `piePointModeToggle`; read against r6 and changed: `piePointModePause`, `pieMousePosSet3D`, `pieSameOnScreenPoint` (nearest-pixel rounding), `pieMovementCursorDraw` (HW1's stipple back, depth test off in the sensors manager) |
| `ShipView.c/.h` | rewritten from the r6 disassembly: `svSelectShip` (auto-turn start, `catRndShipViewSelectTime`), `svClose`, `svShipViewRender` (one turn in 6 s via `cat_cameraSetAngle`, look-at with only z negated, near plane 5.0, the clip-plane fade-in: solid then wireframe pass, `g_WireframeHack`), `cat_svShipDescriptionDraw` 0x00514dd0, `svStartup`; HW1's six stat panels and `svDirtyShipView` retired |
| `Camera.c/.h` | `cat_cameraSetAngle` 0x0042b940 |
| `ConsMgr.c/.h` | `CatCmJob` 0x28, `cat_cmCountJobsForPlayer`, `catCmSinglePlayerBuildFactor` (0x00894198), `catCmSinglePlayerCostFactor` (0x0089419c), `catCmExtendedInfo` + `cat_cmExtendedInfoToggle` 0x00454620 (CM_ExtendedInfo) |
| `TaskBar.c` | `tbBumperProcess` without the tutorial messages |
| `GamePick.c/.h` | `gpStartInGameEscapeMenu` (quit time against real time), `catGpEscapeMenuScreenStart`, `cat_gameStatsButtonDraw`, `cat_gameStatsStartup`, `cat_gameStatsIsActive` |
| `HorseRace.c` | `hrDrawPlayersProgress` (no hyperspace ring, rounded bars, skill level), `hrDrawChatBox`, `hrChooseSinglePlayerBitmap` (Cataclysm pictures, bar at (243,235) 152x3, `loading.script` colour), `hrChatTextEntry`, `horseRaceShutdown`, background-dirty count 10000 (no glcompat) |
| `CommandWrap.c/.h` + all callers | `clCommandMessage(text, colour)` feeds the scroller; colours per caller from r6 (white, red 0xff0000ff, blue 0xffff0000); `mrCommandMessageDraw` / `gMessage` display retired |
| messages wired | `cat_shipUpdateExperienceLevel`, `cat_rmResearchModuleActivated`, `cat_salvageSpecialTarget` (+ too strong), `cat_salvageNoDepotAbort`, `MothershipHouseKeep`, `LeechHouseKeep`, `cat_clHarvestNearestResource`, `clRUTransfer`, `cat_univConvertShip` |
| hooks | `utyGameSystemsInit` (`cat_shipPrintStartup`, `cat_overlayIconsStartup`, `cat_shipNamesInit`, `cat_gameStatsStartup`), `univCreateShip` / `SelectionSwitchPlayerOwner` / `univRemoveShipReferences` (names, `cat_ioRemoveShipReferences`), `univUpdateReset` + `singlePlayerLoadNewLevel` (`cat_missionTimerReset`), `SaveGame` / `LoadGame` (timer), `universeReset` (`cat_ioReset`, `cat_shipPrintReset`), `mainResetRender` / `mainShutdownRenderer` (`cat_overlayIconsDiscardGL`), `mrRegionDraw` (`cat_moRender`, `cat_shipPrintDraw`, `cat_ioUpdateHover`, drag), `smViewportRender` (HUD hooks, overlay scroll buttons pan), `subTitlesDraw` (`cat_ioSubtitleNotify(0)`) |
| other | `CameraCommand.c` `cat_ccIsPilotingShip/ThisShip`; `prim2d.c` `cat_primCircleSolidBlend2`; `ResearchAPI.c` `cat_rmCollectTopicsByLab`; `Tweak.c/.h` HW1 io tweaks removed; `main.c` `/catHudTest`; `OracleDump.c` test tick; `tools/m2_harness.sh` mode `hud` |

## Self-test and harness mode

`/catHudTest <frames>` (harness `--mode hud --hud-frames N`, default 600): the headless Mission01 run of the mission
mode with the HUD on. Tick 16: the start-up sensors manager is closed, the HUD is viewed as the fleet's owner (the
headless start gives Mission01's 154 ships to player 1), the camera focuses them. Tick 64 (the render list the band box
walks is filled by then): `selRectSelect` over the whole screen (`selRectDragFunction`), the off-screen rest added with
`selSelectionAddSingleShip`, the info panel on, a message referring to a ship, the movement pie plate on. Every frame
then draws status bars, info panel, main overlay and pie plate. PASS needs at least one band-boxed ship and every
counter non-zero.

`r5h-d-hud` counts: see the Runs table.

## Builds

`tools/build.sh --debug` and `--release`: green, 0 warnings (`-Werror`). `tools/m2_layout32.sh`: OK; the new ILP32
asserts (`CatOverlayIcon` 0x20, `CatOverlayIconTexture` 0x108, `CatShipName` 0x70, `CatIoMessage` 0x224, `CatCmJob`
0x28) hold under `clang -m32 -fsyntax-only`.

## Runs (all through `tools/m2_harness.sh`, silent, offscreen; logs under `original/scratch-m2/m2r2-harness/runs/<label>`)

| label | mode | milestone | first stop | ASan | notes |
|---|---|---|---|---|---|
| r5h-d-hud | hud (600) | hud self-test done | none found in the log (rc=0) | 0 | `result=PASS`: band-boxed 19 at tick 64, 154 selected / selecting; frames 535, status bars 10,165 (bars 10,165), icons 332, info panels 535 (9,095 lines), selected / selecting lines 8,025 / 8,025, message lines 202, support units 535, pie plate 535, compass 535; 3,766 file loads |
| r5h-d-a, r5h-d-b, r5h-d-perturb | mission (`mission_test.sh`, 6,000 ticks) | oracle mission run done | none found in the log (rc=0) | 0 | MISSION TEST PASS: a and b byte-identical (1,476,370 bytes, sha256 af00ca976dd69b48), perturbed run caught at tick 1500 (rng10), flipped copy at tick 2000 (`ships_health`); a vs base `r5s-aud2-m-a`: IDENTICAL, 6,001 ticks, no difference |
| r5h-d-fe | fe (120 s) | Main_game_screen | none: still running at the 120s timeout | 0 | UBSan kinds only `fescreen` (92, as base `r5s-fe-fin`); first non-benign UBSan: none; 284 file loads (base 233: + 48 overlay icons, OverlayIcons.script, MainOverlay.script, shipnames.script) |
| r5h-d-sp | sp (340 s) | universe running (univUpdateCounter > 0) | none: still running at the 340s timeout | 0 | first non-benign UBSan: BTG.c:388 (as base); UBSan kinds and counts as base `r5s-sp1`; 33 ship statics; 3,767 file loads (base 3,726: + the 51 start-up loads, - 10 `ring.lif`/`arrows.lif` loads of GoK's blinking ring) |
| r5h-c-* | the same set on the build before the last edits (mode-0 off calls, subtitle notify) | as above | as above | 0 | MISSION TEST PASS, IDENTICAL to base; fe 284 loads (base 233), sp 3767 (base 3726), UBSan kinds as base |
| r5h-b-hud, r5h-b-mission | hud / mission | done | rc=0 | 0 | earlier builds: PASS; mission IDENTICAL to base |

## Delta-map review entries

126 entries with notes `[review: M2-R5 hud]` in `ghidra/delta_map_review.json` (83 new with sample `port-R5-hud`, 43
updated: an existing review's class, change and sample are kept and the note appended, except where this port changed
the class), merged with `python3 -I tools/delta_map.py merge --done m2r5:hud`; `docs/m2-r1-function-deltas.md`
regenerated (`tools/delta_map.py report`). [audit: the port also edited `clWrapDeterministicBuild` 0x00452eb0 (IDENTICAL, the debug-only
enableTextFeedback line's colour) without the note; the audit appended it, so 127 entries carry the tag; counts unchanged] Classes of the 126: 73 MODIFIED, 44 IDENTICAL (compared with r6), 9
REWRITTEN. Five IDENTICAL bodies were found modified and reclassified: `piePointModePause` 0x004e5e70,
`pieMousePosSet3D` 0x004e5ec0, `pieSameOnScreenPoint` 0x004e5fc0, `pieMovementCursorDraw` 0x004e6c50,
`horseRaceInit` 0x0048e3c0. Counts: IDENTICAL 3,014 -> 3,009, MODIFIED 1,329 -> 1,334, REWRITTEN 77, reviewed
2,079 -> 2,162.

IDENTICAL bodies compared in scope (40): GamePick 10, HorseRace 8 (7 kept, `horseRaceInit` reclassified), PiePlate 7
(3 kept, 4 reclassified), Scroller 6, Tactical 4, TaskBar 5; the remaining differences are GoK platform code
(user-settings save path, `readdir`, stb_image, SDL loop), recorded in the notes.

## CAT-TODOs closed / re-targeted

Closed: the message-scroller TODOs (`clCommandMessage`, `cat_ioMessageAdd` in ShipSelect.c, ResearchAPI.c,
Salvage.c x3, Mothership.c, Leech.c, CommandLayer.c x2, UnivUpdate.c), `cat_shipNamesRelease` and
`cat_ioRemoveShipReferences` in `univRemoveShipReferences`, `cat_missionTimerReset` in `univUpdateReset`, the
universeReset io/shipPrint resets. Re-targeted: `cat_popupMenuRemoveShip` (M2-R6 popup menu), the debug-key lists
(M2-R6 key bindings), the rest of `subTitlesDraw` (M2-R6 subtitles), CM_Research / CM_Launch (M2-R6 ConsMgr),
`cat_saveGameMiscChunks` (M2-R6 save game), `sendChatMessage`'s fourth argument (stage 5 LAN), the
`hrChooseSinglePlayerBitmap` NIS flag 0x00a6adac (stage 6). No `CAT-TODO(M2-R5 hud)` is left in `src/`.

## Leftovers and owners

- **Game-stats screen** (`cat_gameStatsValueGet/ValuePrint/PlayerRowFill/LayoutCompute/End/Click/Draw/Begin`, about
  5,000 B): not ported. Its only entry `cat_utyGameQuitToMainOrStats` 0x005ab5b0 (end of a non-single-player game) is
  not in the port, and `cat_gameStatsDraw` needs `cat_popupMenuDrawFrame` 0x004ec9a0 and
  `cat_primRectShadedHorizontal2` 0x00570610 (popup menu). Owner: M2-R6 popup menu / UI screens. Startup, button
  draw and `IsActive` are ported.
- Debug-key overlay (`cat_moDebugKeyLineDraw/KeyGet/KeyValue`): M2-R6 key bindings.
- Key bindings in the overlay/panel (stand-ins `CAT_KB_*`), the FOW grid for picking, the right-click popup menu:
  M2-R6.
- `cat_new_00515930` is ported as `cat_rkoBigGunRadius` in RKO.c (an earlier round).
