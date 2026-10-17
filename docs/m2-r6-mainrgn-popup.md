<!-- done: popup, ui-commands, mouse, gamestats, selftest -->
# M2-R6 mainrgn-popup: main region, mouse, the right-click popup menu, the game-stats screen

Status: DONE (producer 1 of M2-R6; base commit `314475c`; nothing committed, the auditor checkpoints).

Subsystems: `popup` (the popup menu, the target flags, the mimic menu items, the prims), `ui-commands` (the
`cat_ui*` commands, the main-region bodies, the tactics / formation / music keys, the band box), `mouse`
(`cat_mouseGetContextAction` and its table, the 31-cursor system, `PlugScreen.c`), `gamestats` (the game-stats
screen), `selftest` (`/catUiTest`, harness mode `ui`, the runs).

## Result

| Gate | Result |
|---|---|
| `/catUiTest` (mode `ui`, run `r6mp-fin-ui`) | `uitest: done selected=154 rows=10 popupframes=16 commands=26 targetrows=1 statsframes=8 result=PASS`, ASan 0, no unsuppressed leak |
| Context-action table | all 18 actions match the exe's two jump tables (0x0056f70c cursor, 0x0056b650 right-click order), table below |
| Mission01 streams | `r6mp-fin-mission-a` and `-b` byte-identical (sha256 `6ea73d317b4aabb0`, 1,476,370 B); `hwo_compare.py` vs R5's `r5-integrate-mission-a` (`645f94c69d68319e`): `IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference` (only the header's build identity differs) |
| sp / fe | unchanged against the base binary (same milestones, stops, ASan 0, UBSan kinds); +15 file loads (the new cursors and popup fonts) |
| Builds | `tools/build.sh --debug` and `--release` green, 0 warnings (-Werror); `tools/m2_layout32.sh` OK (CatPopupItem 60 = 0x3C, CatTargetActions 164 = 0x29 dwords) |

## Mission01 stream before / after

- Before: `r6mp-base-mission` (the base binary, 6,000 ticks): oracle mission run done, ASan 0, 1,476,370 B, sha256
  `eabbed8604d1d256`; IDENTICAL to R5 (6001 ticks).
- Interim (after the popup / ui / gamestats code): `r6mp-interim-mission` IDENTICAL to R5 (6001 ticks).
- After: `r6mp-fin-mission-a` = `r6mp-fin-mission-b` (`cmp` equal), sha256 `6ea73d317b4aabb0`, IDENTICAL to R5. The
  sha differs from base and R5 only by the header's build identity, as hwo_compare reports.

## What changed, file by file

New files (REULA header as the HW1 siblings):

- `src/Game/PopupMenu.c/.h`: the 32 exe functions of the popup menu block 0x004eaa60..0x004ef860
  (`cat_popupMenuFindSpecialTarget` 0x004eaa60 .. `cat_popupMenuRemoveShip` 0x004ef840, with
  `cat_selShipScreenRectDraw` 0x004ed9b0), the item tables from the exe's data (main table 0x0088de88, 49 rows of
  0x3C bytes, submenus 1..13), `CatPopupItem` (0x3C, ILP32 assert). Rows < 0x18 run their key command
  (`cat_kbCommandExecute` 0x0054eb10), rows >= 0x18 the special actions (`cat_popupMenuExecuteSpecial` 0x004ecfe0)
  through the `cat_ui*` commands to the wraps. Also the `/catUiTest` self-test (HWI-TEST, below).

Changed files:

- `src/SDL/mainrgn.c/.h`: the main region's Cataclysm bodies. The 11 MODIFIED bodies: `mrStartup` 0x00563b50 (256 key
  children, `cat_mrKeyChildrenAlloc` 0x00563be0), `mrObjectClick` 0x00566b60, `mrDockingOrders` 0x00567750 (the
  slipgate order), `mrSelectHold` 0x0056a1e0, `mrRetire` 0x005653b0, `mrScuttle` 0x00565210, `mrFormationShips`
  0x00566a00, `toMoveLineDraw` 0x00568e30, `mrRemoveDerelictsAndAllPlayerShipsFromSelecting` 0x00563aa0,
  `mrSetTheFormation` 0x005659d0, `mrReset` 0x00569f80; the band box (`cat_mrBandBoxRelease` 0x0056a610,
  `cat_mrReleaseRight` 0x0056b0b0, `cat_mrReleaseLeft` 0x0056af60, `cat_mrClickRelease` 0x0056ad50,
  `cat_mrSelectRectDraw` 0x0056ac00); tactics / formations (`cat_mrSetTactics` 0x00565c30, `cat_mrTacticsCycle`
  0x00566860, the formation cycle, `catMrCyclesApply` in `mrRegionDraw`); `cat_mrMimicTarget` 0x00567860; the 26
  `cat_ui*` commands 0x00563e20..0x00566510 (`cat_uiSpecialTargetCommand` 0x00564060 and its eight special wrappers
  0x00564030..0x00564750, hive swarm, siege cannon, cloak, waypoint mode 0x00565100, halt 0x00565190, link / unlink,
  unmimic, the force-field sphere 0x00565f30, the Dreadnought repulse 0x005663f0, the Leech actions 0x00566470 /
  0x00566510); the waypoint UI (`cat_mrWaypointsStart` 0x00564e20 ..); `mrRegionProcess` sends the mouse events to the
  `cat_mr*` handlers and the popup keys first; `mrRegionDraw` draws the band box, the popup menu and, in the
  non-render branch, the game-stats screen. Bodies found modified while comparing the IDENTICAL list (below):
  `mrBuildShips` 0x005649b0, `mrLaunch` 0x005654b0, `mrResearch` 0x00565530, `mrNextMothershipPtr` 0x00565da0 (the
  exe's eight mothership types, table 0x008ba39c), `toDrawPulsedLine` 0x00567d40 and `toDrawRadialIndicator4`
  0x00569030 (task real time), `toDrawMoveToLine` 0x00568100 (the waypoint path legs, `toDrawMovePath`). The
  fast-click test of `cat_mrReleaseLeft` reads `universe.realtimeelapsed` (0x00b62ac4) as the exe.
- `src/SDL/mouse.c/.h`: `cat_mouseGetContextAction` 0x0056f8f0 (1,052 B) and `catMouseContextActions` (the two jump
  tables); the cursor system: `mouseStartup` 0x0056d670 (31 cursor LiFs in 32 slots, each mirrored),
  `mouseInitGL` 0x0056e400 / `mouseReset` 0x0056e620 (31 textures), `mouseGetLIF` 0x0056e9c0, `mouseDraw` 0x0056ec50,
  `mouseSetCursorSetting` 0x0056f3b0 (exe body: objective click, the band-box modifier cursors, then the context
  action's cursor); the enum's `link` is `link_cursor` (POSIX `link()`); `cat_mouseSaveCursorUnder` 0x0056ebb0 /
  `cat_mouseRestoreCursorUnder` 0x0056eb40 empty (decision 28: the Win32 front buffer; SDL redraws the frame).
- `src/Game/PlugScreen.c`: `psLinkDraw` 0x004e9850 (the game-on link sets `RSF_DrawThisFrame` on the root region
  and `feRenderEverything`), the https fix of `psPlugLinkSet` 0x004ea240 writes one slash.
- `src/SDL/render.c`, `src/Game/HorseRace.c/.h`: `mouseSetCursorSetting` only (no `mouseSelectCursorSetting`, gone
  from Cataclysm); `hrInitBackground` / `hrShutdownBackground` declared.
- `src/Game/GamePick.c/.h`: the game-stats screen: `cat_gameStatsValueGet` 0x00488970, `cat_gameStatsValuePrint`
  0x00488a50, `cat_gameStatsPlayerRowFill` 0x00488c80, `cat_gameStatsLayoutCompute` 0x00488e70, `cat_gameStatsEnd`
  0x00489290, `cat_gameStatsClick` 0x004892e0, `cat_gameStatsDraw` 0x004896b0, `cat_gameStatsBegin` 0x00489e90
  (layout data 0x00881400..0x00881438, titles 0x008813b8). The word the screen clears and sets back
  (0x008b9ecc) is `mrRenderMainScreen` (written by globalsInit, cmClose, cmConstructionBegin, read by mrRegionDraw).
- `src/Game/KeyBindings.c/.h`: the Cataclysm command table (0x97 commands, two callbacks each, 0x008b6310) with the
  main-region / ui callbacks; `cat_kbCommandExecute` 0x0054eb10, `cat_kbCommandKeyString` 0x0054e960,
  `cat_kbCommandKeyMatch` 0x0054ebc0 (stand-in: no bindings loaded yet); the music next / previous commands 0x60 / 0x61
  (0x00550510 / 0x00550520) call `musicEventNextTrack` / `musicEventPrevTrack` (0x00523c50 / 0x00523ca0, compared:
  same as the tree's, 0x19 tracks).
- `src/Game/ShipSelect.c/.h`: `cat_selComputeTargetActions` 0x00513a00 (`CatTargetActions`, 0x29 dwords).
- `src/Ships/Mimic.c/.h`: the mimic menu tables `cat_mimicInitDisguiseTables` 0x005b9c60 .. `cat_mimicItemAdd`
  0x005ba0d0; called by `gameStart` (a new game) and `singlePlayerLoadNewLevel` as the exe.
- `src/Ships/Mothership.c/.h`: `cat_bigGunFireCommand` 0x005bc0c0, its point callback 0x005bbce0 (point order
  `cat_clWrapPointOrder` 0x00453a70 for the selection's mothership), `cat_bigGunIsCharged` 0x005bd440 (CAT-FIX: no
  gun, not charged).
- `src/Game/CameraCommand.c/.h`: `cat_ccShipIsLegalToFocusOn` 0x0042cd40.
- `src/Game/InfoOverlay.c/.h`: `cat_ioFocusLastMessageShip` 0x004ac740; the overlay's popup test is
  `cat_popupMenuIsOpen`.
- `src/Game/Sensors.c/.h`: the sensors-view option words (`catSmView*`, 12, exe defaults); `smViewportRender` draws
  the popup menu and updates its hover as the exe.
- `src/Game/SinglePlayer.c/.h`: `cat_spPlayerCanHyperspace` 0x005193b0, `cat_spMissionFlagValue` 0x0051c120,
  `catSpBigGunAllowed`; `spHyperspaceRollCallScreen` declared.
- `src/Game/SpeechEvent.c`, `SoundEvent.h`: `cat_musicRadioReset` (the three words cat_gameStatsBegin writes inline);
  the music-key comment corrected.
- `src/Game/UnivUpdate.c`: `cat_popupMenuRemoveShip` after `mrShipDied` (univRemoveShipReferences).
- `src/SDL/utility.c/.h`, `utility_mac.c`: `cat_utyMultiplayerQuitNotify` 0x005aa0f0; `cat_popupMenuStartup`
  after `ioStartup` (utyGameSystemsInit); `cat_mimicInitDisguiseTables` in `gameStart`; no `CSM_Dock` /
  `CSM_Harvest` menu callbacks (the exe's table names `CSM_Hyperspace` only; both functions take other arguments now).
- `src/SDL/prim2d.c/.h`, `prim3d.c/.h`: `cat_primRectSolidTexturedFullRectBlendC2`, `cat_primRectShadedHorizontal2`,
  `cat_primLine3Smooth` 0x00572320 (the SDL GL always smooths, as `primLine3`).
- `src/Game/PiePlate.c/.h`: 0x00b39cbc is `catPieWaypointMode` (set by `cat_mrWaypointsStart` 0x00564e20, not
  `mrTradeStuff`); notes updated. `src/Game/Select.h`: band-box note. `src/Game/SpaceObj.h`: `CAT_UPGRADE_Link`
  0x8. `src/Game/OracleDump.c`, `src/SDL/main.c`: the `/catUiTest` hooks. `src/Game/meson.build`: PopupMenu.c.
- `tools/m2_harness.sh`: mode `ui` (`--ui-ticks N`, milestone "ui self-test done", `uitest:` summary line).
  `tools/m2_layout32.sh`: CatPopupItem and CatTargetActions.
- `ghidra/delta_map_review.json`, `ghidra/delta_map.json`, `docs/m2-r1-function-deltas.md`: the reviews below.

## Context-action table (checked entry by entry)

A scratch script (`ctxcheck.py`, session scratchpad, not in the tree; it uses `tools/m2_exedata.py` and the full
disassembly `original/scratch-m2/m2r2-mesh-tools/exe.dis`) reads `catMouseContextActions` from `src/SDL/mouse.c`, the cursor each case of
`mouseSetCursorSetting`'s jump table 0x0056f70c writes to `mouseCursorType` (0x00a757f4), and the calls / pushed
abilities of each case of `cat_mrReleaseRight`'s table 0x0056b650:

| action | cursor (tree = exe) | order (tree) | exe right-click case |
|---|---|---|---|
| 0 none | normal_mouse (no case) | popup | `cat_popupMenuOpen` 0x004eb810 |
| 1 harvest | resource | harvest | `mrHarvestResources` |
| 2 salvage | salvage | special 0 | `cat_uiSpecialTargetCommand`, push 0 |
| 3 attack | band_box_attack | attack | `MakeShipsAttackCapable` .. `clWrapAttack` |
| 4 repair | support | special 1 | push 1, the common special tail |
| 5 force field | force_field | protect | `cat_uiProtectFormation` 0x00565f30 |
| 6-8 slipgate s/m/l | slipgate_small/medium/large | slipgate | `mrDockingOrders` |
| 9 dock | small_docking | dock | `MakeShipsGuardCapable`, `clWrapDock` |
| 10 leech | leech | special 2 | push 2 |
| 11 link | link_cursor | link | `cat_clWrapLink` 0x00453940 |
| 12 mimic | mimic | mimic | `cat_mrMimicTarget` 0x00567860 |
| 13 EMP | emp | special 4 | push 4 |
| 14 swarm | swarm | special 3 | push 3 |
| 15 ram | ramming | special 6 | push 6 |
| 16 missiles | fire_missile | special 7 | push 7 |
| 17 infect | beast_weapon | special 5 | push 5 |

Script output: "cursor column: all 18 match"; the order column matches each case's callee / pushed ability.

## Self-test and harness mode `ui`

`/catUiTest <ticks>` (PopupMenu.c, HWI-TEST; `tools/m2_harness.sh <label> --mode ui`): the headless Mission01 run of
the mission mode. Tick 16: views the game as the fleet's owner (the headless start gives Mission01's fleet to player
1, as `/catHudTest`), selects it through the selection API. Tick 24: `cat_popupMenuOpen(NULL, &selSelected)`; the
frames draw it (counted in `cat_popupMenuDraw`). Tick 40: every `cat_ui*` command once through the wraps (the special
target commands on a resource: Mission01 has no enemy ship then), the menu once more for that target, closed. Tick 48:
`cat_gameStatsBegin` (the universe pauses); after 8 drawn frames `uitest: done ... result=PASS|FAIL`, then
`cat_gameStatsEnd` (back to the main menu) and the quit. PASS needs ships selected, the menu open with rows, drawn,
26 commands run, closed, the stats screen active. The orders change the simulation: not a stream reference.

## Harness runs

| Run | Mode | Milestone | First stop | ASan | Notes |
|---|---|---|---|---|---|
| r6mp-base-mission | mission 6000 (base) | oracle mission run done | none | 0 | `eabbed8604d1d256`, IDENTICAL to R5 |
| r6mp-interim-mission | mission 6000 | oracle mission run done | none | 0 | IDENTICAL to R5 |
| r6mp-ui-1, -2 | ui | ui self-test done | none | 0 | PASS (target none, then a resource) |
| r6mp-fin-ui | ui | ui self-test done | none | 0 | PASS, 3,800 file loads |
| r6mp-fin-mission-a / -b | mission 6000 | oracle mission run done | none | 0 | byte-identical, `6ea73d317b4aabb0`, IDENTICAL to R5; 3,781 file loads |
| r6mp-base-fe / r6mp-fin-fe | fe (120 s) | Main_game_screen | none: still running at the 120s timeout | 0 / 0 | UBSan kinds as base (92 `fescreen` only); 284 -> 299 file loads |
| r6mp-base-sp / r6mp-fin-sp | sp (340 s) | universe running (univUpdateCounter > 0) | none: still running at the 340s timeout | 0 / 0 | first non-benign UBSan BTG.c:388 in both; UBSan kinds and counts as base; 3,767 -> 3,782 file loads |

The 15 extra file loads of fe and sp (sorted FileLoads.log compared): the 13 new cursor bitmaps (beast_weapon, emp,
fire_missile, force_field, leech, link, mimic, objectives, ramming, slipgate_small/medium/large, swarm; salvage.LiF
replaces the third special_activate.LiF load) and the popup menu's fonts `arial_12.hff` / `arial_15.hff`. Otherwise
sp and fe are unchanged (milestones, stops, ASan, UBSan kinds).

## Delta-map review entries

50 entries with `[review: M2-R6 mainrgn-popup]` in `ghidra/delta_map_review.json` (37 new with sample
`port-R6-mainrgn-popup`, 13 existing ones with the note appended), merged with
`python3 -I tools/delta_map.py merge --done m2r6:mainrgn-popup` (50 records changed); `docs/m2-r1-function-deltas.md`
regenerated. Counts: IDENTICAL 3,009 -> 2,999, MODIFIED 1,334 -> 1,344, REWRITTEN 77, reviewed 2,162 -> 2,199.

- Every IDENTICAL body of mainrgn.c / mouse.c / PlugScreen.c that had no review was compared with r6. Same:
  `mrEnable`, `mrShutdown`, `mrTradeStuff`, `mrCameraMotion`, `toFieldSphereDraw`, `mouseCreateGLHandle`,
  `mousePositionSet` (HW1's clamp + SetCursorPos; SDL warps), `mouseClickShipDied`, `mouseEnable`,
  `psCrossFadeLinkSet`, `psScreenStart`. Reclassified MODIFIED (and ported): `mrBuildShips`, `mrLaunch`, `mrResearch`,
  `mrNextMothershipPtr`, `toDrawPulsedLine`, `toDrawMoveToLine`, `toDrawRadialIndicator4`, `mrNISStarting`,
  `psLinkDraw`, `psPlugLinkSet`.
- The unread MODIFIED bodies in scope: the 11 mainrgn ones (ported, above), `mouseStartup`, `mouseSetCursorSetting`
  (ported), `mousePoll` (Win32 cursor calls inline; the SDL platform read is unchanged), `psModeBegin` (psStartup
  inline before the glcompat test; no glcompat in SDL: unchanged).
- Notes appended to `mrHarvestResources`, `mrRegionProcess`, `mrRegionDraw`, `mouseInitGL`, `mouseReset`,
  `mouseGetLIF`, `mouseDraw` (now fully compared), `smViewportRender`, `univRemoveShipReferences`,
  `singlePlayerLoadNewLevel`, `utyGameSystemsInit`, `rndRenderTask`, `piePointModeOnOff`; `gameStart` gets a partial
  review (only the mimic call).

## Decision 29 (the 0.325 pixel-centre offset)

Kept GoK's default (no offset). What it would change here: the band box rectangle (`cat_mrSelectRectDraw`), the
popup menu's frame, rows, icons and target highlights (`cat_popupMenuDrawFrame`, `cat_selShipScreenRectDraw`), the
game-stats screen's panel, bars and icons, the three new prims and the cursor quad (`mouseDraw` adds 0.325 before the
divide) would each sit 0.325 px right / down of where the tree draws them.

## CAT-TODOs closed / retargeted

Closed: `UnivUpdate.c` (popup remove ship), `Sensors.c` (popup draw / hover in the sensors manager), `SpeechEvent.c`
(the music keys), `render.c` "M2-R6 front end and mouse" (mouse half; the front-end half retargeted to
`M2-R6 front end`), the four `mainrgn.c` "stage 3 UI" tags (the bodies are ported), `Mimic.c`'s menu note,
`PiePlate.h` / `Select.h` notes (M2-R6). Retargeted: `PiePlate.c` waypoint cursor 0x004e6e70 -> `M2-R7 waypoint draw`;
`Sensors.c` (two, the sensors manager's move) -> `M2-R6 sensors`. New: `CameraCommand.c` FOW grid branch
(`M2-R6 FOW`), `mouse.c` key bindings (`M2-R6 key bindings`).

## What is left, and owner

- Key bindings: `cat_kbLoadKeyBindings`, `kbCommandKeyIsHit` for the commands (the band-box modifiers use the default
  keys via `catMrKbHeld`), the Pause command (M2-R6 key bindings).
- The waypoint draw 0x004e6e70 (it draws jitter from `ranRandomFn(0)`) (M2-R7 waypoint draw).
- The FOW grid tests (`cat_ccShipIsLegalToFocusOn`, cat_selPositionHiddenByFOW) (M2-R6 FOW); the developer console
  test `CAT_MR_CONSOLE_OPEN` (M2-R6 console).
- The front end's saved-cursor frame handling in `rndRenderTask` (M2-R6 front end); `Region.c`'s
  `regRegionDelete` dirtying (stage 3 UI, not this scope).
- `mrHoldRight` / `mrHoldLeft` stay as the tree's stand-ins next to Cataclysm's state words (other modules test them).
