<!-- done: options, keys, console, overlay, startup, review -->
# M2-R6 startup-keys: start-up, main, utility, key bindings, console

Status: DONE (producer 3 of M2-R6; base commit `142a8e2`; the first session was cut off by the usage limit and left the
WIP snapshot `57ae3b4` with options, keys, console and overlay done; this session continued it with startup and review
and the gate runs; nothing committed by the producer, the auditor checkpoints). `docs/PLAN.md` in `git diff 142a8e2` is
commit `404e74e` (another agent), not this producer's.

Subsystems (in this order): `options` (the options file Cataclysm.cfg: utility.c's table at the exe's, debugwnd.h,
the harness's home template), `keys` (Key.c, KeyBindings.c: `cat_kb*`, the key names, the key dispatch;
`/catKeysTest`, harness mode `keys`), `console` (the developer console `cat_console*`; `/catConsoleTest`, harness mode
`console`), `startup` (main.c: `WinMain` / `ProcessCommandLine` and the command-line table, the window functions;
utility.c: the game-systems start-up and shut-down, `gameStart` / `gameEnd`, `utyNewGameStart`, `utyGameQuit`,
`cat_utyGameQuitToMainOrStats`, the `cat_uty*` toggles), `overlay` (the debug-key overlay of InfoOverlay.c, the
`CAT_KB_*` stand-ins of InfoOverlay.c / ShipPrint.c / mainrgn.c / mouse.c / Sensors.c / Options.c, screenshot.c's
message, debugwnd.c), `review` (delta-map review entries).

## Mission01 stream before

- Before: `r6sk-base-mission`, the base binary (142a8e2, a copy kept in the session scratchpad), 6,000 ticks: oracle
  mission run done, ASan 0, 1,476,370 B, sha256 `7fed8b8718b96fc2`; `hwo_compare.py` against R5's
  `r5-integrate-mission-a`: `IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference` (header build
  identity 02e8e4da vs 142a8e21 only).

## options (done)

- `src/SDL/utility.c`: `UTY_CONFIG_FILENAME` is `"Cataclysm.cfg"` (string 0x008b6f80, `utyOptionsFileRead`
  0x005a83a0 / `utyOptionsFileWrite` 0x005a84f0). `utyOptionsList` is the exe's table 0x008c1478 .. 0x008c18e0 row for
  row (95 rows, its order and names, no sections: `PrevColorN.detail`, `PlayerPwd`, `spCurrentSelected`,
  `MPGameFlags`, `MultiPlayerGameSetting`, `ResInjInterval`, `ResInjAmmount`, `ResLumpSumTime`, `ResLumpSumAmmount`,
  `DifficultySetting`), then the HWI rows: the exe's registry rows GoK keeps in the file (`deviceCRC`, `deviceIndex`,
  `screenWidth` / `Height` / `Depth`; the exe's `regOptionsList` 0x008c0e58) and GoK's own (`HomeworldDataPath`,
  `fullScreen`, `CollectResourcesAtEndOfMission`, `PauseOrders`, `PlayIntros`, `ShipRecoil`,
  `ShipsAlwaysUseOwnerColors`, `TimeCompressionFactor`). HW1's `noPalMB`, `Voice0..2` and the 19 HW1 key rows are gone
  (no exe row; Cataclysm's bindings are keys.cfg). A udword row is written `"%s    %d"` (the exe's 0x008c2e7c; GoK
  `%u`). The two multiplayer dwords (`MPGameFlags` = the dword at 0x008860ea, CaptainGameInfo's `pad2` + `flag`;
  `MultiPlayerGameSetting` = the dword at 0x008860d4, `numComputers` .. `startingResources`) are staged through
  `catUtyMPGameFlags` / `catUtyMPGameSetting` (HWI: the LP64 struct's offsets differ). `DifficultySetting` stores
  `catSpDifficultySetting` (0x008941a0, 2 in .data); its readers are stage 6's (`CAT-TODO(stage 6 single player)`).
  The registry part of the exe's read / write (`SOFTWARE\Sierra On-Line\Cataclysm`) is not applicable (SDL).
- `src/SDL/debugwnd.h`: `DIS_FileName` "Cataclysm.cfg" (the Win32 debug window is not built).
- Where a run reads its options file: `utyOptionsFileRead` reads `$HOME/.homeworld/Cataclysm.cfg` (GoK's CONFIGDIR,
  not /settingspath), `utyOptionsFileWrite` writes the run's /settingspath. `tools/m2_harness.sh` now puts a
  `Cataclysm.cfg` (`TutorialNeeded    0`, `PlayIntros    0`, the exe's format) next to the old `Homeworld.cfg` in the
  home template, on every run when missing (function `options_template`, documented in the header). Run
  `r6sk-opt-fe`: `Main_game_screen`, ASan 0, 299 file loads, the same list as the base's `r6sk-base-fe` but
  `Cataclysm.cfg` in place of `Homeworld.cfg` (lines 281 and 299): the tutorial and the intros stay off.

## keys (done)

- `src/Game/Key.c` / `Key.h`: `keyInit` 0x0049ae00 ends with `cat_keyNameStringsInit` 0x0049b1b0 (the key-name string
  ids per virtual-key code, the exe's table 0x00a44004: `catKeyNameStrings`; closes the `CAT-TODO(stage 3)`). HWI: the
  exe's key ids are Win32 virtual-key codes (the numbers in default_keys.cfg / keys.cfg, its region key ids, its
  `keyScanCode` index), this tree's are SDL scancodes (GoK); `catKeyVkFromKey` / `catKeyFromVk` / `catKeyVkIsHit`
  convert through one table (every key the exe's name table and keyboard tables know, plus HW1's mouse ids 1..7 / 0xf8
  from Relic's Key.h).
- `src/Game/KeyBindings.c` / `.h`: HW1's 19-command key table (kbBoundKeys, the five per-language default tables,
  kbCanMapKey, the key pool list, kbCheckBindings, kbKeyUsed, kbKeyBoundToCommand, kbRestoreSavedSettings /
  kbSaveSettings, kbSetNewKey) is gone; the exe has none of it. Ported from the exe:
  - the binding table `catKbBindings` (0x00a6ee90: per command a count and three 16-byte `{key, shift, alt, ctrl}`
    records, 0x34 bytes; static asserts on both sizes), `catKbBindingsLoaded` (0x00a70d3c), the version (0x00a70d40);
    the command names / descriptions (0x008b5e58, string ids) and the debug-key groups (0x008b67c8) as data;
  - `cat_kbLoadKeyBindings` 0x0054da10 (default_keys.cfg's version; keys.cfg made from it when missing or older; parsed
    through `cat_kbKeySetCB` 0x0054d460: "key <command> [ALT+][SHIFT+][CTRL+]<code>", a name the exe does not know is
    skipped: the install's file has 48 developer rows), `cat_kbReloadKeyBindings` 0x0054de10 (resetToDefaults copies
    default_keys.cfg first), `cat_kbSaveKeyBindings` 0x0054e2d0 ("v %d", the header, one "key  %s  %s" line per
    binding with `"%s%s%s%d"` ALT+ / SHIFT+ / CTRL+ / code); HWI: default_keys.cfg is read from the data path (the
    install) and keys.cfg is read and written in the user settings path (the exe: both in the current directory);
  - `cat_kbKeyCharParse` 0x0049b570, `cat_kbKeyToString` 0x0049b590 (the named keys through the name strings, the others
    through the exe's five keyboard tables 0x008b0ca8 + language * 0x200, shifted character but for the digits),
    `cat_kbCommandsCanShareKey` 0x0054d6a0, `cat_kbBindingToString` 0x0054d750, `cat_kbCheckBindingConflict`
    0x0054d880, `cat_kbSetNewKey` 0x0054e410 (CAT-FIX: the exe shifts from an unset index when the other command's
    binding is not found), `cat_kbCommandsForKey` 0x0054e680, `cat_kbKeyPress` 0x0054e7c0 / `cat_kbKeyRelease`
    0x0054e8c0, `cat_kbBindingGet` 0x0054e910, `cat_kbCommandNameGet` 0x0054e940 (CAT-FIX: row 0 reads
    MessageStrings[-1] in the exe), `cat_kbCommandKeyString` 0x0054e960, `cat_kbModifiersMatch` 0x0054eb40,
    `cat_kbCommandKeyMatch` 0x0054ebc0, `kbCommandKeyIsHit` 0x0054ec60, `cat_moDebugKeyGet` / `Value` 0x0054d3c0 /
    0x0054d3d0;
  - the options screen's list: `kbListWindowCB` 0x0049bca0 (the 150 commands; the captured key bound through
    `cat_kbSetNewKey`; HWI: a key the exe has no code for shows the exe's invalid-key screen), its title and item draws
    0x0049b830 / 0x0049b8f0 (group headers, "??????", "NONE" in red), `kbKeyResetToDefault` 0x0049be80
    (`cat_kbReloadKeyBindings(TRUE)`), `kbInitKeyBindings` 0x0049bea0 (the font only);
  - every row of the command table 0x008b6310 now has the exe's callbacks: Select_All_Visible 0x0054f3c0, the
    Multiple_Select release 0x00550530 (`cat_ioMultipleSelectRelease` 0x004ac4e0, InfoOverlay.c), Next_Overlay_View
    0x00550550, Enable_Height_Move 0x0054f4d0 / 0x0054f4f0, PageDown / PageUp_GameChat 0x0054f510 / 0x0054f520
    (`cat_ioMessageScrollUp` / `Down` 0x004aca90 / 0x004acad0), Start_Ally / Global_Message 0x005503f0 / 0x00550420,
    Accelerate / Decelerate_Time 0x005504d0 / 0x005504f0, Toggle_Game_Speed 0x00550720 (HWI accessor
    `cat_taskTimeCompressionIndexSet`, Task.c), Change_Overlay_Backdrops_Toggle 0x00550540 (`cat_moPrintBoxesToggle`
    0x004acb90), GammaUp / Down 0x00550600 / 0x00550620 (the Glide gamma call is decision 28's), Pause 0x00550640
    (it toggles 0x00a6ed74 = `catUnivFrameStep`, univUpdate stops while it is set; `CAT_KB_PAUSED()` of the managers'
    keys is that flag now), Take_Screenshot 0x005506f0 (`rndTakeScreenshot` = 0x00a75d58), Skip_Sensors 0x00550710,
    Create_ShipGroup_1..10 0x0054f740 .., `cat_kbSelectHotKeyGroupX` 0x0054fe30, `cat_kbAddHotKeyGroupX` 0x00550060,
    Remove_Selected_Ship_Group 0x005501e0 (CAT-FIX: group 10 indexes past the exe's ten groups, unbound by default).
    The rows the exe leaves empty (Pan_Sensors_*, Zoom_*, Bandbox_*, Toggle_Overlay_Options, Show_Keys_*) are polled.
    The letterbox test of the time keys and Pause (0x00a4cb38, written by kasfWideScreenIn / Out) stays 0:
    `CAT-TODO(stage 6 KAS)`.
- `src/SDL/mainrgn.c` / `.h`: `mrRegionProcess` 0x00567930's key events are the exe's: during an NIS Escape / Space /
  Enter run the Escape command (not while a single-player movie plays), else `cat_kbKeyPress` and the middle button's
  Focus command, with the main region disabled the stats screen's Escape / X and Space / Escape run Skip_Sensors; key
  ups go to `cat_kbKeyRelease`. HW1's `mrKeyPress` / `mrKeyRelease` (1,100 lines) and `mrTacticalOverlayState` (the
  caps-lock sync, also removed from main.c's activation: the exe's WindowProc has none) are gone: no exe counterpart.
  `catMrKbHeld` is `kbCommandKeyIsHit`; the CAT_KB_* ids live in KeyBindings.h.
- `kbCommandKeyIsHit` users: `cameraControl` 0x0042bab0 polls Zoom_In / Zoom_Out (Camera.c; HW1's zoomInNow /
  zoomOutNow were set by mrKeyPress), `smViewportRender` the four Pan_Sensors keys (Sensors.c), mouse.c's cursor
  choice the band-box keys.
- `src/Game/Options.c`: `opRestoreSavedSettings` 0x004d7f80 ends with `cat_kbReloadKeyBindings(FALSE)`, the cancels
  0x004d8370 / 0x004d83a0 call it, `opOptionsSaveSettings` 0x004d8d00 calls `cat_kbSaveKeyBindings` (HW1
  kbRestoreSavedSettings / kbSaveSettings). As in the exe, the start-up's `opUpdateSettings` saves keys.cfg once.
- `src/SDL/utility.c`: `cat_kbLoadKeyBindings` between the overlay icons and the ship names (utyGameSystemsInit
  0x005abe70); `/catKeysTest` after the start-up. `src/SDL/main.c`: the `/catKeysTest` switch; `/NoBind` is gone (the
  exe's table has none; `bkDisableKeyRemap` went with HW1's key remap).
- Self-test `/catKeysTest`, harness mode `keys` (documented in the harness header): run `r6sk-keys-3`:
  `keystest: done rows=197 known=149 unknown=48 commands=147 bindings=149 startsave=1 saved=1 reload=1 reset=1
  newkey=1 conflicts=1 shares=1 failures=0 result=PASS`, ASan 0, no UBSan report, rc 0. The keys.cfg the run leaves
  is identical, line for line (CR/LF aside), to the keys.cfg the GOG game itself wrote in the install
  (`original/game/keys.cfg`, 152 lines).
- Mission01 after this subsystem: `r6sk-keys-mission` (a copy of this build), 6,000 ticks, ASan 0, sha256
  `918bb8445f5172f9`, `hwo_compare.py` against R5: `IDENTICAL: 6001 ticks and 0 packets compared`.

## console (done)

- New `src/Game/Console.c` / `Console.h` (REULA header style, next to ConsMgr.c: the exe links the console right
  after `cat_cmIsOpen` 0x0045a250): `cat_consoleStartup` 0x0045a260 (28 output lines, 8 history lines, the TAB line,
  Arial_12.hff, FEMan\console.fib with CON_TextDraw; CAT-FIX: the exe allocates again in every gameStart and frees
  only at the shut-down), `cat_consoleShutdown` 0x0045a3d0, `cat_consoleOpen` 0x0045a460 (spLockout(MOUSE), a
  single-player game paused, the "console" screen scrolled up by 300), `cat_consoleClose` 0x0045a4d0,
  `cat_consoleAddChar` 0x0045a520, `cat_consoleOutputScroll` 0x0045a560, `cat_consoleExecuteCommand` 0x0045a590
  ("PROF", show / set / run, "Not a command"), `cat_consoleKeyPress` 0x0045a700 (letters, shifted digits, `;` `-` `.`
  `\`, space, backspace, Escape, up / down history, TAB with the exe's prefix quirk, tilde scroll-out, Enter or a full
  line of 0x4B characters), `cat_conTextDraw` 0x0045b1c0 (the scroll, shadowed lines, the blinking `|`),
  `cat_consoleTabComplete` 0x0045b420, `cat_consoleRunCommand` 0x0045b520 (types 0-9: bool with true / on / false /
  off, byte, word, dword, float, string; the callbacks get type, variable, value, reply), `cat_consoleShowVariable`
  0x0045f110 (`"%s true"` / `"%s false"`, `"%s: %d"` reading a dword for every integer type, `"%s: %f"`, `"%s: %s"`),
  `cat_consoleConfigFileLoad` / `Save` 0x0045faf0 / 0x0045fec0 (per row type, id, value; floats as 16.16; the reader
  decodes the writer's little-endian words big-endian, so a saved file does not read back: exe behaviour, logged by
  the self-test; CAT-FIX: the exe's writer reads through the callback rows' NULL variable, alltech's first, and would
  fault; the reader skips them too; HWI: the exe's developer path "\cataclysm\exe\dog.cfg" is the user settings path
  + "dog.cfg"), `cat_consoleLoadConfig` / `SaveConfig` 0x00460160 / 0x00460180, `cat_consoleConfigStartup` /
  `Shutdown` 0x004601a0 / 0x004601c0, and the command rows `cat_spConsoleAddResources` 0x0054d080 (up to 20,000 RUs),
  `cat_spConsoleWarpMission` 0x00518820 (missions 1..19 through the hyperspace button), `cat_consoleAllTech`
  0x004f4350 (`cat_rmTechSetInit(.., 2)`, `cat_playerUpdateUpgradesFromResearch`), `cat_consoleUpgradeShips`
  0x0054d0e0, `cat_consoleExperienceShips` 0x0054d1c0 (Ship +0x5E0 = `catKillsByClass[CLASS_Fighter]` + 1,
  `cat_shipUpdateExperienceLevel`).
- The command table `catConsoleCommandTable` (0x008783c8, 29 rows, the exe's types, ids, names and variables):
  `tactical` = `mrDrawTactical`; `morphStartScale` (0x0087813c) is now Mimic.c's `MIMIC_RADIUS_SCALE`
  (`catConsoleMorphStartScale`), `rammingtime` / `rammingangle` (0x00878144 / 0x00878148) RammingFrigate.c's 300 and
  `RAM_TURN_ACCURACY` (`catConsoleRammingTime` / `Angle`), `constyle` (0x00878168) render.c's build-in fade mode
  (`catRndConsoleStyle`), `cbv` / `lighting` Shader.c's `catShColourByVertex` / `catShLighting`, `BTG` (0x0087816e)
  render.c's new `catRndDrawBTG` (rndBackgroundRender 0x00574c25 tests it before btgRender), `TRAILS` (0x00878171)
  render.c's `catRndDrawTrails` (no longer const), `cameracues` (0x008b8844, 1 in .data) `catMotionCues` for the motion
  cues; the rows the exe's other code never reads (mimictime, the ramming distances / abort angle, the Leech rates,
  boxes, spheres, PLANETS, FOG) are the console's own variables. All values unchanged (the same constants as
  variables). `experienceships` 0x0054d1c0 and `upgradeships` 0x0054d0e0 are ported; the "0x00a751c0 re-apply
  toggle" has no writer anywhere in the exe (one reader, `cat_univShipDeferredUpdate` +0x2d3; no console row names
  it): it stays FALSE (UnivUpdate.c's comment says so).
- Not ported, re-targeted: `cat_consoleKasCommand` 0x0045f270 (a line naming a KAS function through the exe's
  cat_kasFunctionTable and `cat_kasCallFunctionByName` 0x0045b9a0, 14 KB): `CAT-TODO(stage 6 KAS)`; until then an
  unknown name says "Not a command", as the exe for a name that is no KAS function.
- Hooks: `gameStart` starts the console after `subTexturesReset` and sets the new started bit `SS2_CatConsole`
  (0x00ab7656, name chosen), `utyGameSystemsShutdown` shuts it down before `cmShutdown`; main.c's SDL key-down hands
  every key down to `cat_consoleKeyPress` (the exe's WindowProc does, untranslated; HWI: as the virtual-key code); the
  `CAT_MR_CONSOLE_OPEN()` of mainrgn.h is `cat_consoleIsOpen()` (0x00a2dcd4). As in the exe, nothing opens the console:
  its only opener is the tilde key of `cat_consoleKeyPress`, which returns while it is closed.
- Self-test `/catConsoleTest <ticks>`, harness mode `console` (documented in the harness header): run `r6sk-console-1`:
  `consoletest: done selected=154 frames=4 shown=29 set=22 commands=8 keys=32 closed=1 failures=0 result=PASS`, ASan
  0, rc 0 (the config file 240 bytes; loadConfig left `rammingtime` at the scribbled 12345: the byte-order quirk).

## overlay (done)

- `src/Game/InfoOverlay.c`: the `CAT_KB_HELD` / `CAT_CONSOLE_OPEN` stand-ins are `kbCommandKeyIsHit` and
  `cat_consoleIsOpen()` (the camera buttons 0x41..0x44, Toggle_Overlay_Options 0x62, Show_Keys 100..102, the console
  0x00a2dcd4); `cat_moDebugKeysDraw` 0x004aa2a0 is ported with `cat_moDebugKeyLineDraw` 0x004aa0a0: with a Show_Keys
  key held the screen darkens (0xd2050505 through `primRectTranslucent2`, glEnable / glDisable GL_BLEND around it:
  the pointers 0x00b3a47c / 0x00b3a4b0, identified through the exe's primRectTranslucent2) and the five lists of
  0x0072add0 .. 0x0072ae90 print each group header, each command's name, ":  " and its bindings (green / yellow /
  blue per binding, " , " between, "NONE"); the "DEBUG KEYS ON" line's test is the exe's empty stub (never drawn). New
  for the key commands: `cat_ioMessageScrollUp` / `Down` 0x004aca90 / 0x004acad0, `cat_moPrintBoxesToggle` 0x004acb90,
  `cat_ioMultipleSelectRelease` 0x004ac4e0. The "pause" flag the overlay prints as paused (0x00a6ed74) is
  UnivUpdate.c's `catUnivFrameStep`, which the Pause key toggles now: the overlay's `catMoWideScreen` (named after
  cat_kasWideScreenIn, with a `CAT-TODO(stage 6 KAS)`) was the same word and is that flag now.
- `src/Game/ShipPrint.c`: the panel's help and dragging test `kbCommandKeyIsHit(Toggle_Overlay_Options)` and the
  console.
- `src/SDL/screenshot.c`: `ssSaveScreenshot` 0x0057ef80's file names (`shota%03d.jpg`, the first free of 000..199, else
  `shot200.jpg`) and its message `"Screenshot Saved: %s"` through `clCommandMessage` (colour 0xffffffff) for the
  Take_Screenshot key's shots; GoK's read-back, its stb JPEG writer (decision 28) and its directory (the user
  settings path's ScreenShots/; HWI: the exe's %HW_Data%\screenshots\) stay, and GoK's named /debugScreenshots shots
  stay silent.
- `src/SDL/debugwnd.c` / `cat_dd*` (`cat_ddDelete` 0x005567a0, `cat_ddCreate` 0x005567f0, `cat_ddActivate`
  0x005568d0, `cat_ddErrorString` 0x00556990): the Win32 DirectDraw debug window, not applicable (SDL, decision 28;
  debugwnd.c is not in the build).

## startup (done)

Read against the r6 decompile and disassembly (`tools/m2_fdec.py`, `tools/m2_fdis.py`, `tools/m2_exedata.py`).

- `src/SDL/main.c`:
  - start-up (`WinMain` 0x005631f0): `main()` already had the exe's order (single-instance check on Win32, options
    file, `opKeyboardLoad`, command line, `utyGameSystemsPreInit`, window, `utyGameSystemsInit`, the loop dispatching
    tasks and the options timer); a comment lists what SDL does not have (the "CloseCataclysm" message, the registry
    copy of the command line, `glCapNT` / `glCap95` / `cat_glCapWin2K`, device enumeration and rGL / Glide selection,
    the loop's `CataclysmPatch.exe` launch, the message box titled "Emergence"). The window title is the exe's
    "Emergence" (class / title 0x008b85b8 / 0x008b85c4; `windowTitle`, render.c `CAT_RND_WINDOW_TITLE`). The R2
    workaround list stays empty: every run below is the plain harness.
  - `ProcessCommandLine` 0x0055f880: a switch whose parameter is missing, or whose function fails, prints the help
    and ends the start-up (-1; HW1 / GoK went on with the next token). The exe's 63-row table 0x008b8870 (dumped from
    the exe) compared with the tree's: rows added `/prepath` (`cat_mainPrependPathSet` 0x0055f3d0, WIP), `/CDpath`
    through `cat_mainCDROMPathSet` 0x0055f3f0 (GoK's `fileCDROMPathSet` keeps the Win32 drive test and the exe's
    message; a set path stops the CD search), `/SaveMPGames` / `/LoadMPGame` (`cat_mainLoadMPGameSet` 0x0055f500, WIP),
    the hidden `/d3dDeviceCRC` (0x008b8c00, `mainOutputCRC`), `/aiplayerLog` also in release builds (row 0x008b8d20 is
    visible; HW1 had it only in debug builds); `/NoBind` gone (WIP). The tree's other rows are GoK's (`/settingspath`,
    `/bigoverride`, `/ignoreBigfiles`, ...) or HWI diagnostics. `DebugHelpDefault` 0x0055f6c0 is HW1's over that table
    (GoK prints it to stdout).
  - `networkVersion` "Emergence 1" and the new `minorBuildVersion` "01" (0x008b8850 / 0x008b8860; GoK
    "GardensOfKadesh" / `HOMEWORLD_BUILD_VERSION`; mainswitches.h declares it).
  - `HandleEvent` (`WindowProc` 0x00561f10): Escape now ends its case (GoK fell through into F11's); every key down
    goes to `cat_consoleKeyPress` (WIP). The Bink stop on Escape, the Shift+Ctrl+F11 / F12 renderer resets and the
    rGL / Glide cases stay GoK's SDL handling (decision 28).
  - `mainCloseRender`: comment for the exe's copies (inlined in `mainLoadGL` 0x00561570, `mainRestoreRender`
    0x00560ba0, `WindowProc`), which also free the glcompat textures (not built) and the plug texture:
    `CAT-TODO(M2-R7 render)` for `rndLoadShamelessPlug(FALSE)` 0x00579fa0 with `rndShamelessPlug`'s port.
    `mainResetRender` 0x00560420 has the exe's order with the three unloads (WIP); `mainOpenRender`,
    `mainRestoreSoftware`, `mainLoadGL`, `mainLoadParticularRGL` get them through it.
  - `DeactivateMe` 0x0055fb90: comment only (minimise, renderer deactivation and `binkPause` are SDL's and GoK's movie
    player's, which has no pause).
- `src/SDL/utility.c`:
  - `gameStart` 0x005a9290, read in full (it was "not reviewed in full"): the tutorial is Sect against Beast and a
    single-player game Sect (player 0) against Beast whatever race was picked; a skirmish gives player 0 the picked
    race and the others the other race and the picked one by turns; player 1's colour only for Sect / Beast; the
    current player's research / construction / launch managers closed first (`rmCloseIfOpen` / `cmCloseIfOpen` /
    `lmCloseIfOpen`); `sensorLevel` always 0; a skirmish computer's AI level is the stored level itself
    (`opCPUDifficulty` stores it by radio button; `ConvertSliderToAIPlayerDifficulty` would map 0..2 all to
    beginner); the first research init uses the word 0x00a75240, which has no writer in the exe (three reads, all in
    gameStart), so 0; a computer player gets five times the starting resources (2000 for the "none" setting) when flag
    bit 5 is set, and there is no harvesting `rmEnableShip` pass; `universeSwitchToPlayer(curPlayer)` after the level
    or load; after the sync number every player's tech set again (`cat_rmTechSetInit`: everything in a non-tutorial
    skirmish / multiplayer game with research off, else nothing; the human player keeps its set while the
    single-player warp is on) with `cat_playerUpdateUpgradesFromResearch`; at the end of a new game every ship gets
    `cat_shipApplyUpgrades` and a host with clamped objects `updateClampedObject`; the tutorial plays track 0x12; no
    `universeFlagEverythingNeeded` for the stats switches; `bobInitProperties` is 0x00425ca0, a jump to
    `bobResetProperties` (Blobs.c: no mission-6 blob radius). Each of the tech-set pass and the upgrade pass was first
    built as an experiment and its 6,000-tick Mission01 stream compared: IDENTICAL to R5 (`r6sk-exp-tech-mission`,
    `r6sk-exp-upg-mission`), so both are ported. Not ported, re-targeted (one line, owner first):
    `CAT-TODO(stage 6 level load)` the start-position shuffle (20 swaps of a table 0x00ab77e0 with two
    `ranRandomFn(0)` picks each, read by `scriptSetMissionSphereCB` 0x004a2420 / `scriptPreMissionSphereCB` 0x004a2820:
    it would draw 40 numbers from stream 0 in Mission01 too and its reader is the level loader's);
    `CAT-TODO(stage 6 single player)` the task bar (the exe has no `tbStartup` / `tbShutdown`; Objectives.c,
    SinglePlayer.c and KASFunc.c still call into the task bar's regions); `CAT-TODO(stage 4 AI)` `cat_aisReset`
    0x00433d30 and the `aiplayerStartup` arguments; `CAT-TODO(stage 5 LAN)` the colour checksum 0x00ab7658 (the fourth
    ring word `netcheckFillInChecksum` sends). Not applicable: `glcActivate` (glcompat). The support-unit clear and the
    `/LoadMPGame` file (WIP), the console start-up (WIP) and GoK's `cameraJoystickReset` (HWI, decision 28) stay.
  - `gameEnd` 0x005a9f40: no `writeGameStatsToFile`; ends with `opOptionsSaveSettings` (options file and keys.cfg)
    and the pause flag 0x00a6ed74 (`catUnivFrameStep`) cleared. `universeStaticClose` is empty in the exe too (the
    folded empty function 0x0049ae30); `tbShutdown` stays with gameStart's task-bar CAT-TODO; `glcActivate` N/A.
  - `utyNewGameStart` 0x005ab180: `shiplagtotals` cleared in every game (HW1: multiplayer only);
    `CAT-TODO(M2-R6 NIS/camera)` for its end (`cat_mcSetMode` / `cat_mcResetRequest` on the motion cues 0x00c49400
    while `catMotionCues` is set).
  - `utyGameQuit` 0x005ab570: `soundEventStopMusic(0.5)` and `soundEventPause(TRUE)` before the quit command.
  - `utyGameSystemsPreInit` 0x005ab810: no `bigCRC` (the CRCs stay 0); the version string ends with
    `minorBuildVersion`; `fmathInit` not needed (decision 23: fsqrt stays sqrtf); HW_Data, the CD search, the big
    files and the heap stay GoK's file layer.
  - `utyGameSystemsPreShutdown` 0x005ac690 and `utyGameSystemsShutdown` 0x005ac710: compared, no change beyond the
    console's (WIP): the dropped calls of the delta map (`keyClose`, `battleChatterShutdown`, `bsShutdown`,
    `dmgShutdown`, ...) are calls to empty functions the linker folded into 0x0049ae30; the options file is written
    inline (`utyOptionsFileWrite` here); the registry rows and `glcShutdown` N/A; `tbShutdown` see above.
  - `versionNumDraw` 0x005a8250: "<Version> Emergence 1.01".
  - `utySinglePlayerOptions` 0x005aa580 (an IDENTICAL found modified): sets `whichRaceSelected` back to the Sect after
    starting Create_new_game. `utySinglePlayerGameStart` 0x005aaf80: identical but GoK's
    `spResetMissionSequenceToBeginning` (`CAT-TODO(stage 6 single player)`, kept until `singlePlayerInit` is ported).
  - `utyFatalErrorWaitLoop` 0x005a91d0: Ghidra's body runs past `exit()` into gameStart (the "added calls" of the delta
    map); the real body is HW1's, GoK's SDL version kept.
  - HW1 race aliases `R1` / `R2` retired in utility.c (`RACE_Sect` / `RACE_Beast`); main.c, KeyBindings.c, Key.c,
    Console.c, screenshot.c, InfoOverlay.c and ShipPrint.c have none.
  - From the WIP: `cat_utyGameQuitToMainOrStats` 0x005ab5b0 ("QuitToMain": outside single player the game-stats screen,
    `cat_gameStatsBegin`, producer 1's; in single player `utyGameQuitToMain`), the six video-option toggles
    `cat_utyDynamicCPV*` / `LensFlare*` / `Repairbots*` 0x005a8cb0 .. 0x005a8ec0, the credits track 0x29, no CSM_* /
    PLM_* callback rows (the exe's table 0x008c0f80).
- `src/Game/Blobs.c`: `bobInitProperties` = `bobResetProperties` (0x00425ca0 is a 5-byte jump).
- `src/Game/KeyBindings.h` / `.c`: the two size asserts moved into the header (they were doubled in the .c) and
  `tools/m2_layout32.sh` now includes KeyBindings.h (prints `CatKbCommandBindings` 52 = 0x34): `m2_layout32: OK`.

## review (done)

Every IDENTICAL body of the scope compared with the exe (60 unreviewed: Key.c 3, main.c 6, utility.c 51): one found
modified (`utySinglePlayerOptions`, ported); the rest identical, or Win32 / rGL bodies where GoK's SDL version stays
(`mainDevStatsInit`, `mainReinitRGL`, `utyBrowserExec` are not in GoK's tree; `mainSaveRender`, `mainSetupSoftware`,
`mainCleanupAfterVideo`, `utyClientRectGet`, `utyClipMouse`, `utyToggleKeyStatesRestore`). `keyPressDown` /
`keyRepeat` inline `keyBufferAdd` with the exe's no-buffer key list 0x00883268 (Win32 codes); the 2-bit `keystick`
explains the `| 2 & 0xfb` / `& 0xf9` masks.

`ghidra/delta_map.json`: 96 review entries `[review: M2-R6 startup-keys] ...` (appended to an existing note where there
was one): Key.c 4 (`keyInit`, `keyPressDown`, `keyRepeat`, `keyClearAllStuckKeys`), KeyBindings.c 4 (all four),
main.c 23 (every MODIFIED function and the six compared IDENTICALs), screenshot.c 1, utility.c 63 (12 MODIFIED: all but
`utyForceTopmost` / `utyChangeResolution`, reviewed before; the 51 compared IDENTICALs incl. the Win32 ones), Blobs.c 1 (`bobResetProperties`: the
jump). `done_subsystems` += `m2r6:startup-keys`; `counts.reviewed` 2460 -> 2549. debugwnd.c's functions were already
reviewed as not built; `cat_dd*` (`cat_ddDelete` 0x005567a0, `cat_ddCreate` 0x005567f0, `cat_ddActivate`
0x005568d0, `cat_ddErrorString` 0x00556990) are the Win32 DirectDraw debug window: not applicable (cat-new functions
have no map rows; listed here).

## Build

`tools/build.sh --debug` and `tools/build.sh --release`: green, 0 warnings under -Werror (after every subsystem and
at the end). `tools/m2_layout32.sh`: OK.

## Plain-harness runs (final binary)

| label | mode | milestone | first stop | ASan | notes |
|---|---|---|---|---|---|
| r6sk-fin-mission-a / -b / -p | mission_test.sh --ticks 6000 | oracle mission run done | none (rc 0) | 0 / 0 | MISSION TEST PASS: A and B byte-identical (sha256 `95bfbfa4afa117bb`), the perturbed run found at tick 1500 (rng10), the flipped copy at tick 2000 (ships_health) |
| r6sk-fin-mission-rel | mission, release | oracle mission run done | none (rc 0) | n/a | IDENTICAL to R5 |
| r6sk-fin2-mission | mission (after the assert move) | oracle mission run done | none (rc 0) | 0 | byte-identical to r6sk-fin-mission-a |
| r6sk-fin-keys, r6sk-fin2-keys | keys | keys self-test done | none (rc 0) | 0 | `keystest: done rows=197 known=149 unknown=48 commands=147 bindings=149 startsave=1 saved=1 reload=1 reset=1 newkey=1 conflicts=1 shares=1 failures=0 result=PASS` |
| r6sk-fin-console | console | console self-test done | none (rc 0) | 0 | `consoletest: done selected=154 frames=4 shown=29 set=22 commands=8 keys=32 closed=1 failures=0 result=PASS` |
| r6sk-fin-fe | fe, 90 s | Main_game_screen | none: running at the timeout | 0 | 299 loads, the base's set (`r6sk-base-fe`) with Cataclysm.cfg for Homeworld.cfg; 0 fescreen UBSan |
| r6sk-fin-sp | sp, 340 s | universe running, univUpdateCounter 1,248 | none: running at the timeout | 0 | base `r6sk-base-sp` (142a8e2): 1,248; loads 3,783 = the base's 3,782 + `FEMan\console.fib` (the console's start-up), Cataclysm.cfg for Homeworld.cfg; the same UBSan kinds; 0 fescreen UBSan |

The sp run and the first mission / keys / console / fe runs used the binary from before the KeyBindings assert move
(compile-time asserts only); `r6sk-fin2-*` repeat keys and mission on the final binary.

The options file: every run reads `$HOME/.homeworld/Cataclysm.cfg` from the harness's home template (`TutorialNeeded
0`, `PlayIntros 0`) and writes the run's `settings/Cataclysm.cfg`: the tutorial and the intros stay off.

## Mission01 stream after

`r6sk-fin-mission-a` (6,000 ticks, debug, ASan 0): sha256 `95bfbfa4afa117bb`, 1,476,370 B; `hwo_compare.py` against
R5's `r5-integrate-mission-a` (sha256 `645f94c69d68319e`): `IDENTICAL: 6001 ticks and 0 packets compared (ticks
0-6000), no difference` (header build identity 02e8e4da vs 57ae3b42 only). No oracle field moved. Release run: the
same verdict.

## New self-tests and harness modes

`/catKeysTest` (harness mode `keys`) and `/catConsoleTest <ticks>` (harness mode `console`, `--console-ticks N`), both
documented in `tools/m2_harness.sh`'s header (the keys and console sections above). The harness's home template now
also holds `.homeworld/Cataclysm.cfg` (function `options_template`, header lines 121-123).

## CAT-TODOs

Closed (26 removed lines in `git diff 142a8e2 -- src`): `CAT-TODO(M2-R6 key bindings)` x17, `(M2-R6 key bindings /
console)` x2, `(M2-R6 console)`, `(M2-R6 start-up)` (the options table), `(M2-R6 HUD messages)` (the screenshot
message), `CAT-TODO(stage 3)` x2 (Key.c `cat_keyNameStringsInit`; utility.c's "the exe goes to the game stats screen", now `cat_utyGameQuitToMainOrStats`). No `CAT-TODO` / `CAT-PROVISIONAL` in
`src/` names an M2-R6 key-bindings / console / start-up owner any more.

Re-targeted or new (each on one line, owner first): `CAT-TODO(stage 6 level load)` the start-position shuffle;
`CAT-TODO(stage 6 single player)` x3 (the task bar, `spResetMissionSequenceToBeginning`, `DifficultySetting`'s readers
and setter from the WIP); `CAT-TODO(stage 6 KAS)` x2 (the console's KAS command, the letterbox
amount of the time keys); `CAT-TODO(stage 4 AI)` x2 (`cat_aisReset`, `aiplayerStartup`'s arguments);
`CAT-TODO(stage 5 LAN)` (the colour checksum); `CAT-TODO(M2-R6 NIS/camera)` (the motion cues of `utyNewGameStart`);
`CAT-TODO(M2-R7 render)` (`rndLoadShamelessPlug(FALSE)` in the close-render copies).

## What is left (owner)

- The start-position shuffle and its two readers: stage 6 level load (moves stream 0 when it lands).
- The task bar's removal (`tbStartup` / `tbShutdown` and their callers): stage 6 single player.
- `spResetMissionSequenceToBeginning` (GoK) and `DifficultySetting`'s readers: stage 6 single player.
- `cat_consoleKasCommand` 0x0045f270 (the console's KAS line) and the letterbox word 0x00a4cb38 of the time / Pause
  keys: stage 6 KAS.
- `cat_aisReset` and `aiplayerStartup`'s arguments: stage 4 AI. The colour checksum 0x00ab7658: stage 5 LAN.
- The motion cues at the end of `utyNewGameStart`: M2-R6 producer 5 (NIS / camera).
- `rndLoadShamelessPlug` / `rndShamelessPlug`: M2-R7 render.
- Not applicable (SDL / GoK, decision 28): the Win32 debug window `cat_dd*`, the registry, rGL / Glide
  (`cat_mainRGLFeaturesSet`, `cat_glideIsLoaded`, `cat_glideShutdown`), glcompat, Bink pause / stop, the patch launch.
