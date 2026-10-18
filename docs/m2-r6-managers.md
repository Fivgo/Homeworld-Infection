<!-- done: build, consmgr, research, launch, sensors, dock -->
# M2-R6 managers: construction, research, trade, launch and sensors managers; the external-build driver

Status: DONE (producer 4 of M2-R6; base commit `c3e7c1f`; nothing committed by the producer, the auditor
checkpoints). Every DONE WHEN item is met (summary at the end).

Subsystems (in this order): `build` (ConsMgr.c: the factory build task, the deterministic build list, the
external-build driver and its helpers, the support units, `CAT_EXTERNAL_BUILD_DRIVER` 1; `/catBuildTest`, harness mode
`build`), `consmgr` (the rest of ConsMgr.c: the construction screen's bodies, the SU bar, the IDENTICAL compares),
`research` (ResearchGUI.c, TradeMgr.c, `cat_rmAcquireShipTechs`, `cat_rsMPRBShipDependSetCB`), `launch` (LaunchMgr.c:
the Cataclysm launch screen, its module slots and the Beast systems; the build test's screens phase), `sensors`
(Sensors.c: `cat_sm*`, the viewport scene, the manager's open / close / zoom; the HUD test's sensors frames), `dock`
(the dock self-test's sCarrier and sProcessor host legs and the ACV judgement; Collision.c's docking exemption).

## Mission01 stream before

- Before: `r6mg-base-mission`, the base binary (c3e7c1f, a copy kept in the session scratchpad), 6,000 ticks: oracle
  mission run done, ASan 0, 1,476,370 B, sha256 `949362895b3d41ad`; `hwo_compare.py` against R5's
  `r5-integrate-mission-a` (`645f94c69d68319e`): `IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no
  difference` (header build identity 02e8e4da vs c3e7c1f2 only).

## build (done)

The external build in a single-player game is driven by the factory list (`cmBuildTaskFunction`), in a multiplayer or
recorded game by the deterministic build list; a computer player's externally built ship runs on its own timer. All
three are ported, and `CAT_EXTERNAL_BUILD_DRIVER` is 1 (`CommandLayer.h`; the user's decision 17 retired).

- `src/Game/ConsMgr.h`: `shipinprogress` +0x24 renamed `catExternalShipAttached` (it has readers: the build task, the
  attach, the ship-died path; R4 said none), static asserts on +0x24 / +0x28 / +0x2C; `shipsinprogress.progress` is
  one entry per ship type, `TOTAL_NUM_SHIPS` (0x91: the build task 0x00454940 walks 0x91 entries and
  `cmAddFactory` 0x004542f0 allocates 0x1B48 = 0x18 + 0x91 * 0x30; HW1's `TOTAL_STD_SHIPS` 60 was too small: the
  Sect list holds dAcolyte 133), static asserts 0x1B48 / +0x18; declarations of the new functions;
  `catCmSinglePlayerBuildFactor` exported.
- `src/Game/ConsMgr.c`:
  - `cmBuildTaskFunction` 0x00454940 at the exe's body: the backspace key (`keyScanCode[VK_BACK]`, 0x00c36b88) skips
    the sensors speech (`smSensorsSkip`), `playPackets` returns, the single-player cost per tick is
    `(buildCost << (17 - 0x0089419c)) / timeStart`, the Beast mothership's build factor
    (`cat_beastMothershipBuildFactor` 0x005bca90) multiplies the cost and the count-down, nothing is charged while
    the universe is paused, the exe's speech ids 0x2001037 / 0x2001047; an externally built type advances only with
    `catExternalShipStarted` and `catExternalShipAttached`, its ship gets the progress (`cat_clWrapExternalBuildUpdate`
    mode 2, `(real32)(1.0 - timeLeft / timeStart)` as the exe's double 0x0072a548 on the x87 stack); a job's end is
    `cat_cmBuildJobFinished`; an external job with no ship asks `cat_cmFactoryHasFreeSlot` and (but for a module or at a
    Beast mothership) `cat_playerHasCompletedShip(sMothershipDockingBay)`, then creates its ship
    (`clWrapCreateShip`, kept in `catExternalShip`) and restarts its clock.
  - New: `cat_cmFactoryMarkDirty` 0x00453c10, `cat_cmBuildJobFinished` 0x00453c20 (the external last step, or the
    ship created; the job counts, the cost, the next clock, `battleLaunchWelcomeExchange` for classes 0..4: sound's
    leftover closed; the new-ship ping), `cat_cmFactoryHasFreeSlot` 0x00454920, `cat_cmExternalBuildShipDied`
    0x00455960, `cat_cmShipDockedReclampCannon` 0x00455ad0, `cat_cmExternalBuildAttach` 0x00455af0,
    `cat_cmCancelJobsOfDeadFactory` 0x00455ea0, `cat_suShipProvidesSU` 0x00459a60, `cat_suShipAdded` 0x00459a90,
    `cat_suShipRemoved` 0x00459ae0, `cat_cmQueuedSupportUnits` 0x00459c20; `cat_cmHasSupportUnits` 0x00459b40 at the
    exe's body (was a CAT-PROVISIONAL TRUE).
  - The deterministic list: `cmDetermProgress_t` is the exe's 0x48-byte node (HW1's fields keep their offsets, then
    `catAttached` +0x3C, `catShipStarted` +0x40, `catShip` +0x44; static asserts), `cmDeterministicBuildStart`
    0x00458e70 (IDENTICAL in the map, found modified: the static by type alone, the single-player divisors, the
    factory entry's external words cleared, the sanity check folded in as `type < 0x3c` and the race; CAT-FIX: the
    exe leaks the node of a "subtract" count and of an unlisted type and writes through a NULL factory entry),
    `cmDeterministicBuildCancel` 0x00459040 (the whole cost refunded, the external ship deleted: mode 1),
    `cmDeterministicBuildProcess` 0x004592a0 (the external ship created once a slot is free, then waiting for the
    attach; the Beast factor; the external end through `cat_cmBuildJobFinished` for the local player; CAT-FIX: the
    exe writes through a NULL factory entry when the creator is no factory). `cmDeterministicBuild` compared: same.
  - `cmBuildJobsAdd` 0x004586c0 (IDENTICAL in the map, found modified): the single-player build-time divisor.
- `src/Game/CommandWrap.c` / `.h`: `clWrapCreateShip` returns the ship (0x00452f30 returns `clCreateShip`'s ship; the
  build task keeps it).
- `src/Game/CommandLayer.c`: `clCreateShip` passes the creator to `univCreateShip` (0x0044a840 passes it as the 6th
  argument).
- `src/Game/UnivUpdate.c` / `.h`: `univCreateShip` takes the exe's 6th argument, the creator (stored in `catCreator`
  +0x4AC at 0x00542c0f; the callers: `univAddShip` NULL, `clCreateShip` the creator, the hive frigate's swarmers the
  hive, the fight statistics NULL); the factory block for the four builder types only (sMothership, sCarrier,
  bMothership, bCarrier with `canBuildShips`; not an externally built ship with a creator; in single player only the
  local player's: 0x0054275a); the external-build start without the provisional switch; `cat_suShipAdded` at
  0x00542b13; `univFreeShipContents` starts with `cat_suShipRemoved` (0x0054b550); `univDeleteDeadShip` calls
  `cat_cmExternalBuildShipDied` (0x00547d97); new `cat_shipUpdateExternalBuildAndAI` 0x00543f10 (2,088 B: the
  computer player's external build timer, its auto-upgrade every 256th tick, its special abilities and the
  multiplayer attack every 32nd tick with the difficulty's chance from random stream 6), called at the end of
  `cat_univShipDeferredUpdate` for a live ship with `universe.aiplayerProcessing` set (0x00544ada).
- `src/Game/Dock.c`: the three `CAT-TODO(M2-R6 ConsMgr)` sites are calls (`cat_cmExternalBuildAttach` at
  0x00465f9f, `cat_suShipProvidesSU`, `cat_cmShipDockedReclampCannon`; the TODOs named the wrong addresses
  0x00459be0 / 0x00459b00 / 0x00456f60), and the retire path calls `cat_rmAcquireShipTechs` (research, below).
- `src/Ships/ModuleShip.c`: `ModuleShipDied` calls `cat_cmCancelJobsOfDeadFactory` (0x005afe30).
- `src/Ships/ShipDisable.c`: `cat_shipSetSUExempt` calls `cat_suShipRemoved` / `cat_suShipAdded` (0x005b4020).
- `src/Game/KASFunc.c`: `SelectionSwitchPlayerOwner` keeps the support units (0x00494050 removed / 0x00494151 added);
  the rest of the exe's two-argument body is re-targeted (`CAT-TODO(stage 6 KAS)`).
- `src/SDL/utility.h`: `catSpDifficultySetting` exported.

### /catBuildTest (harness mode `build`)

New self-test (HWI-TEST, not in the exe; `ConsMgr.c` `catBuildSelfTestTick`, `main.c` `/catBuildTest <ticks>`,
`OracleDump.c` hook; `tools/m2_harness.sh --mode build [--build-ticks N]`, default 7,000, writes `run.hwo`, documented
in the header). On the headless Mission01 run (the oracle mission mode, virtual clock) at tick 16 the local player
gets a scripted sMothership at the grid point farthest from the level's ships (the level's 154 ships are player 1's;
the player's own fleet would come from the kas2c script, off under `CAT_HW1_MISSION_SCRIPTS 0`) and 30,000 RUs; three
jobs are queued at its factory exactly as the screen's build button does (`cmBuildJobsAdd`): the docking bay module
(none came with the mothership; built externally, a module), an sDestroyer (built externally; it needs the docking
bay; Mission01's level excludes the destroyer, so the test loads its static first as `universeStaticInit` would, its
own action) and an sAcolyte (built inside). Checked every tick: an external ship appears at health 0, progress 0,
`SPECIAL_2_CatBeingBuilt`, clamped to the mothership; its health never falls and stays at most maxhealth; at its end
full health (within 0.1%: the health is the sum of the per-tick steps, as in the exe), progress 1.0, flag clear, the
job gone from the factory; the internal ship appears at full health when its job ends.

| run | milestone | first stop | result |
|---|---|---|---|
| `r6mg-build-b` | build self-test done | none (rc 0), ASan 0, no leak | **PASS** at tick 4177: docking bay appeared tick 17 (health 0, dock state 11 = build-attached, clamped) and finished tick 1457 (17,999.9 / 18,000, stays a clamped module); sDestroyer appeared tick 1457 at health 0 (its job waited for the docking bay), finished tick 4177 at 40,000 / 40,000, unclamped, dock state 12 (fly out); sAcolyte appeared tick 208 at 220 / 220; SU 40 / 100, RUs 30,000 -> 25,925 |
| `r6mg-build-b2` | build self-test done | none (rc 0), ASan 0 | PASS; `run.hwo` 1,722,370 B sha256 `ac91d9b0269564ba` **byte-identical** to `r6mg-build-b` (`cmp`; `hwo_compare.py`: IDENTICAL, 7,001 ticks) |

## consmgr (done)

The rest of `src/Game/ConsMgr.c` (and its header) at the exe's bodies; every function of the file's address range
(0x00453c10..0x0045a260) was compared (the delta-map entries below), and the GUI callbacks that have no Ghidra function
(the callback tables 0x00753a18 / 0x00753a60 and the region handlers) were read from the disassembly.

- The screen's tables: the callbacks of the exe (CM_Research 0x004545e0 and CM_Launch 0x00454600 close the screen and
  open the research / launch manager: `cat_cmResearch`, `cat_cmLaunch`; the draw table has no CM_ConnectorDraw and
  no third / fourth carrier, and adds CM_SUBarDraw and CM_FactoryNameDraw); the six hot-key rows of 0x00753ad0
  (Beast fighters / corvettes and the Workers); **the exe's nine construction lists** (0x00753b10 .. 0x00754940:
  Sect, Beast, Kushan, Taiidan small and big, P1; generated from the exe's data) replace HW1's six; the row kind 2
  (`CAT_ITEM_SHIPENABLED`, listed only when `cat_playerShipTypeEnabled`) and the stat 4 (`CAT_STAT_UNAVAILABLE`, a
  listed type that cannot be built now, drawn in the new grey `catCmUnavailableTextColor` 0x007549dc); three carrier
  slots (0x00a2dc4c) and regions; the SU bar region 0x00a2dc30 and the factory-name region 0x00a2dc34; the third
  font (EuroseWideHeavy_i13.hff, 0x00a2dcc0); the sensors-were-open flag 0x00a2dc38; `catCmFactoryShip` 0x00c439b4.
  HW1's per-class job caps (`cmClassCapRemap`, `cmMaxJobsPerClass`, `cmClassCapCanCreateShip`: two
  `CAT-PROVISIONAL` tables "the exe copy was not located") are deleted: the exe asks only the unit caps and the
  support units. No HW1 type alias is left in the file.
- Ported bodies: `updateShipView` 0x00453de0, `cmScroller` 0x00453f30, `cmAddFactory` 0x004542f0 (a disabled ship
  is no factory in single player, carriers by class), `cmReset`, `cmDirtyShipInfo` 0x00454550 (only while open, the
  list refreshed first; the inlined region redraw of the callbacks is `catCmDirtyRegions`), `cmRemoveFactory`
  0x004546d0 (the dead carrier's auto-launch bit, `cat_lmShipAutoLaunchBit` 0x0049fa60: new, LaunchMgr.c),
  `cmStartup` 0x004552b0, `cmUpdateShipAvailable` 0x00455410, `cmClose` 0x00455680 (the sensors manager reopened
  through `catSmReopenAfterManager`, Sensors.c), `cmBuildShips` 0x00455790 / `cmCancelJobs` 0x00455c10 /
  `cmPauseJobs` 0x00456060 (pointer-only callbacks: the exe's speech ids 0x400103e / 0x4001035 / 0x400103b /
  0x4001036 / 0x400103c, the 0x91 entries, the external ship deleted on cancel), `cmShipCostsDraw` 0x00456210,
  `cmSelectShipType` 0x00456430, `cmIncrement` 0x004565a0 (support units, the factory-off / hyperspace test:
  attribute bit `CAT_ATTRIBUTES_FactoryOff` 0x80000, new in Attributes.h), `cmDecrement` 0x00456760, `cmDrawArrow`
  0x00456ca0 and the arrow draws, `cmShipNumbersDraw` 0x00456f00, `cmShipInfoDraw` 0x004573d0, `cmTotalRUsDraw`
  0x00457a00, `cmTotalShipsDraw` 0x00457d00, `cmCanBuildShipType` 0x00457e50 (its third argument: the research and
  kind-2 test; closes the `CAT-TODO(stage 3)` on it), `cmUpdateFactoryNoReset` 0x00457fc0, `cmUpdateShipsAvailable`
  0x00458150, `cmUpdateFactory` 0x00458180, `cmConstructionBegin` 0x00458390 (the Beast screen file
  `Beast_Construction_Manager.fib`), the factory buttons 0x00458780 / 0x00458800 / 0x00458890 (the factory's name
  spoken, 0x400101c), `cmDrawShipImage` 0x004589c0 (its third argument), `cmMotherShipDraw` 0x00458b80,
  `cmCarrierXDraw` 0x00458bf0, `cmBuildHotKey` 0x00458d40 (the virtual-key range), `SaveConsMgr` / `LoadConsMgr` and
  the deterministic list's save callbacks 0x00459e50 / 0x00459ea0 (0x91 entries, the external ships by ID).
- New: `cat_cmFactoryNameDraw` 0x00458920, `cat_cmSUBarDraw` 0x00459f50 (the support-unit gauge: free share
  translucent, the share left after the queue solid; the prim draw `cat_primRectSolidTexturedFullRectBlendC2` of
  M2-R6 mainrgn-popup), `cat_cmIsOpen` 0x0045a250, `cat_cmResearch`, `cat_cmLaunch`.
- CAT-FIXes: `cmBuildHotKey` stops at the list's end row (the exe walks 0x91 rows); `catCmFactorySelect` tests a
  NULL factory entry; the deterministic-list ones of `build`. Not applicable: `rndPerspectiveCorrection` (GoK dropped
  it, render's note) and the glcompat path of `cmDrawShipImage` (no glcompat in the SDL build, decision 28);
  `g_bNeedReverseWordsForOtherLanguage` 0x00a6c538 has no writer in the exe (the class name comes first). Re-targeted:
  `cat_tutButtonsHideForScreen` / `RestoreAfterScreen` (`CAT-TODO(stage 6 tutorial)`), the recorder tests
  (`CAT-TODO(stage 5 recorder)`).
- Runs: `r6mg-build-c` (build mode after the port): PASS as `r6mg-build-b` (3,802 file loads: +1, the third font);
  `r6mg-cm-mission` (6,000 ticks): `run.hwo` sha256 `b0129e78bc6b9a50`, every field equal to `r6mg-build1-mission`
  (below: the build subsystem's stream).

## research (done)

`src/Game/ResearchGUI.c` is rewritten on the exe's research manager (0x004f4bd0 .. 0x004f7a40; the callback tables
0x00891330 / 0x00891378 and the pointer-only callbacks read from the disassembly); `ResearchGUI.h`, `ResearchAPI.c`
/ `.h`, `TradeMgr.c` and `UnivUpdate.c` (`cat_univConvertShip`) carry the rest.

- **The tech sets at the exe's:** `TechPrintList` is the exe's 12-byte row (+ `catLab`, static assert); the two
  static lists are the exe's (Sect 0x008913c0, 37 rows: a header per lab, then its technologies; Beast 0x00891588, 189
  rows: a header per ship, the rows of its upgrade technologies; generated from the exe's data), `rmSetPrintList`
  0x004f75f0 copies them into lists 1, 3 and 4 (192-row arrays as the exe's), `cat_rmSelectTechPrintList` 0x004f7a40
  picks the race's list, `rmUpdateTechList` 0x004f6d30 sets every row's state from the tech sets as the exe
  (held / researching / the mission's allowance `tmTechResearchable` 0x00ba4680 / in multiplayer the requirements
  through `cat_rmTechSetAnd` + `cat_rmTechSetEqual` and the crystals option [audit: said the infection option; the exe gates technology 0x22 = `TECH_CRYSTAL_HARVEST` with bit 0x20 of 0x008860ec, the code tested `TECH_INFECT` (32): fixed]; the end-row count). HW1's
  `PlayerTechListStatic` (41 technologies under four HW1 class headings, a `CAT-PROVISIONAL`), `ShipsNeedTech` and
  the per-race HW1 lists are gone; row kinds 2 and 3 are the listed technologies (`CAT_ITEM_TECH2` / `3`).
- New: `cat_rmBeastPrintIndexToShipType` 0x004f4790, `cat_rmAcquireShipTechs` 0x004f4870 (747 B: a retired or
  converted Beast ship's upgrade technologies to its player, the list rows marked kind 2 / 3, the type enabled; called
  by Dock.c's retire (closes `CAT-TODO(M2-R6 research GUI)`) and `cat_univConvertShip`), `cat_rmPussyBlobDraw`
  0x004f5c90 (the Beast animation), `cat_rmModuleNameDraw` 0x004f4b60, `cat_rmConstruction` 0x004f4f20 /
  `cat_rmLaunch` 0x004f5010, `cat_rsMPRBShipDependSetCB` 0x004f4430 (its own function in the "MPRBShipDepend" row).
- Ported bodies: `rmClearAllLabs` 0x004f4bd0 (the selected lab cleared), RM_ClearLab 0x004f4be0 (the selected lab
  stops: clWrapResearch type 1, speech 0x4001032), `rmResearchItem` 0x004f4c50 (the row's own lab), RM_ExitMenu /
  `rmCloseIfOpen` 0x004f4e30 (the shared close, the sensors manager reopened), `rmDrawLabButton` 0x004f5100 (the
  module icons, the chop-rounded progress bar), the five lab draws and their select handlers 0x004f54c0 .. 0x004f59b0
  (a lab shows its module, technology 0x29 + lab), `getWord` (same), `getShipBuild` 0x004f5e70 (tech sets),
  `rmExtendedInfo` 0x004f6090, `rmTechInfoDraw` 0x004f60b0, `rmTechImageDraw` 0x004f6380 (module pictures, the
  title), `rmTechListWindow` 0x004f6ae0 with the item draw 0x004f6810 (`rmDrawTechListItem`), `rmResearchGUIBegin`
  0x004f7140 (the Beast screen `Beast_Research_Manager.fib`, the default picture), `rmGUIStartup` /
  `rmGUIShutdown` (the borrowed HW1 lab-image hack of R2 removed). The HW1 brief, connectors, sixth lab, marquees and
  select-all are gone; the three `CAT-TODO(stage 3)` and two `CAT-PROVISIONAL` tags of the file are closed.
- `cat_univConvertShip` 0x00543950: the build-requirement copy between the owners' tech statics (closes
  `CAT-TODO(M2-R6 research)`) and, in multiplayer, `cat_rmAcquireShipTechs` for a bProcessor and the
  `strNewTechFound` / `strNewTechFoundRetire` messages [audit: the message test reads the OLD owner's build and upgrade
  rows of the old type (0x00543d7f; owner and type saved from the old ship at 0x00543962 / 0x00543968); the code read the
  new owner's: fixed]; the computer player's dock order stays
  `CAT-TODO(stage 4 AI)`.
- `TradeMgr.c`: `tmAcceptOffer` (0x0052d810, TM_AcceptOffer, pointer-only) adds the technology with `cat_rmTechSetAdd`
  alone (closes its `CAT-TODO(stage 3)`); `tmTechImageDraw` 0x0052e620 has a path for race 0 / 1 only (CAT-FIX for
  another race); `tmTechInfoDraw`'s `CAT-TODO(stage 3)` closed by the compare; the IDENTICAL bodies compared.
- Not applicable: the glcompat paths (decision 28), `rndPerspectiveCorrection`. Re-targeted:
  `cat_tutButtons*` (`CAT-TODO(stage 6 tutorial)`), the multiplayer crystals option [audit: said infection option]
  (`CAT-TODO(stage 5 multiplayer options)`). Medium confidence: `FEC_ListItemInactive` for the exe's 0x00ba1e88
  (the held kind-2 row's colour) by HW1's colour order. [launch: confirmed by the exe's colour script table
  0x008b0b10..0x008b0b60: `FEC_ListItemSelected` 0x00ba1e74, `FEC_Background` 0x00ba1e7c, `FEC_ListItemStandard`
  0x00ba1e80, `FEC_ListItemInvalid` 0x00ba1e84, `FEC_ListItemInactive` 0x00ba1e88]

## launch (done)

`src/Game/LaunchMgr.c` is rewritten on the exe's launch manager (0x0049ccd0 .. 0x0049fc70). The callback tables
0x008833a0 (12 callbacks) and 0x00883408 (13 draw callbacks), and the callbacks with no Ghidra function (LM_Launch
0x0049ce30, LM_LaunchOne 0x0049cf10, LM_AutoLaunchM 0x0049cfd0, LM_LaunchAll 0x0049d1a0, LM_Close 0x0049d250,
LM_Construction 0x0049ccd0, LM_Research 0x0049cd80, LM_BeastSystemsSet 0x0049fbb0, the list item draw 0x0049d3b0,
LM_FighterUsed 0x0049d5e0, LM_CorvetteUsed 0x0049d700, the factory buttons 0x0049d800 / 0x0049d850 / 0x0049d890,
LM_FactoryNameDraw 0x0049d8d0, LM_MotherShipDraw 0x0049db40, LM_Carrier1/2Draw 0x0049dc60 / 0x0049dce0, the seven
module-slot region callbacks 0x0049ec60 + 0x90 n) were read from the disassembly; the screens' atoms were checked
against both `.fib` files (`Launch_Manager.fib`, `Beast_Launch_Manager.fib`: no `LM_AutoLaunchC3` atom, no third or
fourth carrier, the Beast screen has `LM_BeastSystemsSet` and no module slots).

- Data at the exe's: two carrier slots (0x00a44714; HW1 four), the Beast screen file and name, the factory-name font
  (EuroseWideHeavy_i13.hff, 0x00a44710), the sensors-were-open flag 0x00a446ec, the screens' race 0x00a446f4, the
  factory-name region 0x00a44704 and ship 0x00c367e4, the module-system hover words 0x00883604 / 0x00883608, the count
  colour 0xff000080; the four tweakable list colours kept (the exe's tweak table still names two of them, 0x008acd54).
  HW1's `lmUpdateShipView` / `lmcurshipview` (no ship view in the exe), the I/O lock-out, the taskbar flag and the
  Italian header special case are gone.
- Ported bodies: `lmLaunch` (tutorial gate 0x40000 of word 2, the exe's speech 0x4001067), `lmAutoLaunchM` (live for
  a mothership-class mothership), `lmAutoLaunchCX` 0x0049d080, `lmLaunchAll` (gate 0x80000; a dEcclesiam is never
  launched; the screen stays open), `lmClose` / `lmCloseIfOpen` 0x0049d300 (gate 0x100000; the sensors manager
  reopened through `catSmReopenAfterManager`), `lmShipItemDraw` (strLaunchMgrFighterClass / strLaunchMgrCorvetteClass),
  `lmShipsToLaunchDraw` 0x0049d4b0, the docking bars, the factory buttons (no ship view), `lmDrawShipImage` 0x0049d980
  (the slot-present argument, the line to the factory name), `lmMotherShipDraw`, `lmCarrierXDraw` 0x0049dbb0,
  `lmCarrier1/2Draw` (their autolaunch boxes), `lmStartup` 0x0049dfb0, `lmUpdateShipsInside` 0x0049e070,
  `lmLaunchBegin` 0x0049e1e0 (the passed ship's owner and `SPECIAL_2_CatExempt20` tested, the Sect / Beast screen
  loaded again when the race changed, the module slots), `lmReLaunch` 0x0049e520, `lmLaunchCapableShipDied`
  0x0049e690. The launch list's class headers also cover resource-class (corvette header) and micro-class (fighter
  header) ships.
- New: `cat_lmLaunchOne` (the first docked ship of the row's type), `cat_lmConstruction` / `cat_lmResearch` (close,
  then open the other manager, as `cat_cmResearch`), `cat_lmBeastSystemsSet` (radio buttons on
  `MothershipSpec.beastSystemState`, `cat_clWrapSetBeastSystem` with the atom's byte), `cat_lmBeastSystemsOff`
  0x0049fc30 (now called by `beastMothershipSelfDamage`, Mothership.c: closes its `CAT-TODO(stage 3)`),
  `cat_lmFactoryNameDraw`, `cat_lmBackgroundDraw` 0x0049df90, `cat_lmModuleSlotXDraw` 0x0049eb40 (three system
  buttons per slot, disabled without the mothership's upgrade flag 0x200000), `cat_lmModuleSlot1..7Draw` 0x0049f050 ..
  0x0049f8f0 (name, health bar, buttons; regions take every mouse event), the slot region callbacks (a left release
  picks the system: `cat_mothershipModuleSlotSet` 0x005bd6a0, new in Mothership.c, a one-ship selection through
  `cat_clWrapSetModuleSlot`).
- Fixed: `cat_lmFillInModuleSlots` (M2-R4) had slots 1 and 3 swapped: the exe puts sMothershipWeapons (0x17) in
  0x00a44790 and sMothershipMicro (0x16) in 0x00a44798.
- CAT-FIXes: the list walk starts on the last ship, not on PrintOrder's -1 sentinel (the exe reads
  `shipsavailable[-1]`); `lmAutoLaunchCX` ignores a third carrier (the exe indexes past its two-entry tables; no atom
  calls it); `lmLaunchBegin` compares only the two carrier slots; `lmLaunchCapableShipDied` closes the screen when the
  first carrier slot is empty (the exe would read through a NULL launch ship). Not applicable: the glcompat paths
  (decision 28), `rndPerspectiveCorrection`. Re-targeted: the tutorial buttons (`CAT-TODO(stage 6 tutorial)`).
  No HW1 type alias is left in the file.

### /catBuildTest screens phase

The build self-test now ends with the three managers (HWI-TEST; `ConsMgr.c` `catBuildSelfTestScreens`; accessors
`catLmSelfTest*` in LaunchMgr.c, the research screen's regions declared in ResearchGUI.h): once every job is done the
construction screen opens at the mothership and draws 8 frames offscreen (its factory name drawn), closes; the
research screen opens (the player holds TECH_DRIVE_FIGHTER, the test's grant, and has no research module: the list
must be exactly that row under its lab's header, held), draws 8 frames, closes; the launch screen opens (its list
must hold each docked listed type plus its class header), draws 8 frames (factory name and module slots drawn), Launch
One launches the docked acolyte (the test clears the player's autolaunch bits at tick 16, as the dock self-test does)
and it must leave the mothership.

| run | milestone | first stop | result |
|---|---|---|---|
| `r6mg-build-d` | build self-test done | none (rc 0), ASan 0 | FAIL (the test's research expectation: 0 rows is right with no lab and nothing held; the grant added) |
| `r6mg-build-e` | build self-test done | none (rc 0), ASan 0, no leak, 3,857 file loads | **PASS** at tick 4205: jobs as `r6mg-build-b` (docking bay 1457, destroyer 4177, acolyte 208, now kept docked); construction screen open/drawn (factory name, SU bar); research rows 2, last row TECH_DRIVE_FIGHTER (8) held (stat 4); launch rows 2 = expected (fighter header + sAcolyte), factory sMothership, 1 module slot; Launch One: the acolyte left after 1 tick (dock state 12) |

## sensors (done)

`src/Game/Sensors.c` now draws Cataclysm's sensors manager: the exe has no blob render (smBlobsDraw,
smBlobDrawClear, smBlobDrawCloudy and their helpers are absent); its viewport draws `cat_smViewportRenderScene`
0x0050b4b0. All bodies were read from the decompile and checked against the disassembly where Ghidra's types were
wrong (the ship draw's fields, the scene's arguments: two, the matrices).

- New (the scene): `cat_smViewportRenderScene` 0x0050b4b0 (background and worlds with the background option, then
  the range circles, universe bounds, the FOW render hook, decorative asteroids, mines, crates, resources, ships, the
  compass, pings and hot-key numbers, the depth test off), `cat_smSensorRangeBlobsDraw` 0x00509be0 (the player's
  ships' FOW-radius circles, a circle mostly inside a bigger one of the same collision blob dropped, far first, near/far
  colours, faded in by the zoom), `cat_smShipMeshDraw` 0x0050a0d0 (the SM_Mesh types as meshes with their clamped
  modules; the level by the selection circle: `cat_lodLevelGetMimic` 0x004a4b20, new in LOD.c), `cat_smShipBlipType`
  0x0050a280, `cat_smShipsDraw` 0x0050a310 (points in the relation's colour, the class icons `toClassIcon` with the
  icons option, the selection flash on the real-time clock, crystals always, slipgate effects and paths: the cubic
  `cat_ccEvalCubicVel` 0x0042d4a0, new in CameraCommand.c), `cat_smShipHotkeyNumbersDraw` 0x0050ab50,
  `cat_smResourcesDraw` 0x0050ae00 (meteors by `CAT_ATTRIBUTES_KillerCollision`, asteroids not posed by a mimic, dust),
  `cat_smBoundsEdgeDraw` / `cat_smUniverseBoundsDraw` 0x0050b090 / 0x0050b150, `cat_smMinesDraw` 0x0050b380 (mission 3's
  `catSmShowMines`). The scene's colours (.data 0x00893048..0x00893084, 0x00892c90) and the zoom factor 0x00892b74.
- **`smShipTypeRenderFlags` is the exe's table** (.data 0x00892ba8, 145 bytes, generated from the exe's data): closes
  the M2-R2 `CAT-PROVISIONAL` HW1 re-keying (e.g. sSwarmer is SM_Exclude, the module types SM_Mesh|SM_Exclude).
- Ported bodies: `smViewportRender` 0x0050be70 (the scene; the hide flag 0x00a5b228; the band box `cat_mrSelectRectDraw`
  in its sensors flavour; no cursor text or player names; no pan with the popup menu open), `smZoomUpdate` 0x0050b8d0
  (no scrolling side regions: the viewport is the whole screen; the BTG dimming is the exe's own, so the
  `CAT-TODO(M2-R6 sensors)` that called it HW1's is closed by the compare), `smSensorsCloseForGood` 0x0050ba90,
  `smSensorsClose` 0x0050c950, `smStartup` 0x0050ccc0 (no SM_* button callbacks: the screen has only SM_ViewPort, and
  the exe has none of HW1's button names), `smSensorsBegin` 0x0050cdb0 (the KAS force-open 0x00a5b22c, wired in
  `kasfOpenSensors`; the full-screen viewport; the exe's region callback 0x0050c7e0 as `smViewportProcess`, which hands
  the mouse to the main region's `cat_mr*` handlers in their sensors flavour and the keys to the key bindings; the
  reopen from a closing manager), `smCullAndFocusSelecting` 0x0050c580, `smSensorsSkip` 0x0050cc60 (the close-captioned
  branch), `smHorizonLineDraw` 0x005097a0 (the depth test), `cat_smZoomFadeColor` (the real zoom clock; closes its
  `CAT-TODO(M2-R6 sensors)`).
- **`catSmReopenAfterManager`** (the managers' close): the reopen flag 0x00a5b230 and the camera copy 0x00c0aac0 ->
  0x00c0a7e0 before `smSensorsBegin` (closes the temporary `CAT-TODO(M2-R6 sensors)` of `consmgr`).
- `mainrgn.c` `cat_mrWaypointAdd`: with the waypoint-centre option the sensors manager pans to the last waypoint
  (`smCameraLookatPoint` 0x00a5b1f0; closes its `CAT-TODO(M2-R6 sensors)`).
- `Region.h`: `REG_RenderEventsDefault` 1024, the exe's 0x400 (.data 0x0088f484; HW1 512 overflowed with the
  viewport's 256 key children: `r6mg-hud-a` stopped at "All 512 in use").
- The other `CAT-TODO(M2-R6 sensors)` sites (the HW1 viewport process's two move orders) went with that function.
  Re-targeted: the FOW render and grid tests (`CAT-TODO(stage 3 FOW)`; with no grid and the FOW mode at its default 2,
  every grid-tested point is hidden, as the exe would with a NULL grid: the hud run's resources count is 0), the
  motion cues (`CAT-TODO(M2-R6 NIS/camera)`), the hide flag's writers (stage 6, single player / NIS). Not applicable:
  the glcompat hooks (0x00b3a70c, decision 28). HW1's blob render functions are left in the file uncalled.

### /catHudTest sensors frames (harness mode `hud`)

The HUD self-test (InfoOverlay.c) gets a sensors phase (HWI-TEST): for its last 120 frames the selection is hot-key
group 1 and the sensors manager opens with the instant transition; `catSmDrawCounts` counts the scene's draws.

| run | milestone | first stop | result |
|---|---|---|---|
| `r6mg-hud-a` | universe running | `Region.c` "All 512 in use" (the HW1 render-event count) | fixed by the exe's 1024 |
| `r6mg-hud-b`, `r6mg-final-hud` | hud self-test done | none (rc 0), ASan 0, no leak | **PASS**: sensors open tick 480; 119 scenes, 14,364 range circles, 8,791 ship points, 236 meshes, 236 icons, 5,969 hot-key numbers (resources 0: hidden by the missing grid) |

## dock (done)

`src/Game/Dock.c` `catDockSelfTestTick` (HWI-TEST): an sCarrier joins the scripted fleet (its static loaded first:
Mission01 excludes it; `catBuildSelfTestLoadStatic` is shared), two more acolytes dock for support at the sCarrier
(the carrier leg: checked in its inside list at checkpoint A, launched, outside at B), a fourth Worker deposits 250 RUs
at the sProcessor (the processor leg: its host must be the sProcessor, its hold emptied, back outside; RUs up by 500),
and **the two ACVs are judged** as the fighters (inside at A, outside at B; checkpoint A waits for them).

- First run `r6mg-dock-b`: FAIL, the processor leg: the Worker never left dock state 7 (the link flight), 80..130
  units from its point, while the light sProcessor drifted away (velocity -1.5 per tick from the Worker's arrival).
  Cause: **`Collision.c` lacked the exe's docking exemption**: `collCheckShipShipColl` 0x0043ade0,
  `collCheckBigShipSmallShipColl` 0x0043b460 and `collCheckShipResourceColl` 0x0043bac0 skip any object with
  `SOF_CatDocking` (unless `SPECIAL_2_CatBeingBuilt`), which Dock.c sets once a ship is in its host's cone; this tree
  still had only HW1's `dockingship` rejection, a field the exe never writes during docking (its writers are
  `univCreateShip`, `missileShoot`, the save code). Ported (`catCollDockingExempt`, CAT comment with the addresses).
- **The corvette-cone orbit does not persist**: with the exemption both ACVs are inside by tick 1,250 (without it the
  late ACV latched at ~2,048 in `r6mg-dock-b`, after R5's checkpoint); not recorded as stage 4 (docs/m2-r5-layout-dock.md
  annotated).

| run | milestone | first stop | result |
|---|---|---|---|
| `r6mg-dock-a` | level load | `UnivUpdate.c` "sCarrier was not loaded" | the static loader added |
| `r6mg-dock-b` | dock self-test done | none | FAIL (the processor leg, above) |
| `r6mg-dock-d`, `r6mg-final-dock` | dock self-test done | none (rc 0), ASan 0, no leak | **PASS** at checkpoint A tick 1,250: 6 fighters and both ACVs in the sMothership, both carrier-leg acolytes in the sCarrier, both depositors back with empty holds (RUs 750 -> 1,250); B: all ten outside |

## Mission01 stream after

`r6mg-final-mission-a` / `-b` (6,000 ticks, the final binary): rc 0, ASan 0, `run.hwo` 1,476,370 B sha256
`b0129e78bc6b9a50` both (`cmp` identical). Against R5's `r5-integrate-mission-a` (`645f94c69d68319e`):
`hwo_compare.py`: first divergent tick 1, 2 fields: `rng6` (the candidate 10 draws ahead) and `rng_all`; per field
`r6.n` and `rng_all` differ on all 6,000 ticks, nothing else (no position, health or command field). Cause: the
computer player's per-ship timer (`cat_shipUpdateExternalBuildAndAI` 0x00543f10, the build subsystem) draws from random
stream 6 for Mission01's 154 player-1 ships every 32nd tick, as the exe. The stream is the same as after `build`
(`r6mg-build1-mission`): the research, launch, sensors and collision changes moved nothing in it.

## Delta map

107 review entries in `ghidra/delta_map.json` (`review_note` "[review: M2-R6 managers] ...", `reviewed` true) and
`ghidra/delta_map_review.json` (sample `port-R6-managers`); `done_subsystems` + `m2r6:managers`; counts now IDENTICAL
2,966, MODIFIED 1,370, REWRITTEN 84, reviewed 2,635 [audit: 2,965 / 1,371 after the audit reclassified the edited
IDENTICAL `clWrapCreateShip` 0x00452f30, which had no review entry]. Per file: ConsMgr.c 46, ResearchGUI.c / TradeMgr.c / ResearchAPI.c
29, LaunchMgr.c 13, Sensors.c 15, Collision.c 3, KASFunc.c 1. Every IDENTICAL body in scope was compared (the ones an
earlier reread had already confirmed "same" and this work did not touch keep their entry: `cmCloseIfOpen`,
`cmPauseAllJbos`, `cmUnPauseAllJobs`, `lmAutoLaunchC1..C3`, `smStrchr`, `smHotkeyGroupsDraw`, `smUpdateHyperspaceStatus`,
TradeMgr.c's tech-table helpers); 26 classes
changed, all with the reason in the note (e.g. `updateShipView`, `cmBuildJobsAdd`, `cmDeterministicBuildStart`,
`lmUpdateShipsInside`, `lmReLaunch`, `smCullAndFocusSelecting`, `smSensorsSkip` IDENTICAL -> MODIFIED;
`cmUpdateShipAvailable`, `cmCanBuildShipType`, `cmConstructionBegin`, `cmDeterministicBuildProcess`, `rmDrawLabButton`,
`rmUpdateTechList`, `rmResearchGUIBegin` MODIFIED -> REWRITTEN).

## CAT-TODOs and tags

Closed (removed from the tree): `CAT-TODO(M2-R6 ConsMgr)` x8 and its two variants (the external-build driver, decision
17); `CAT-TODO(M2-R6 research)`, `CAT-TODO(M2-R6 research GUI)`; `CAT-TODO(M2-R6 sensors)` x6 (Sensors.c 155, 2288,
3409, 3439, 4431's fade clock; mainrgn.c 1567; the temporary one this work put in the reopen helper is gone too);
`CAT-TODO(stage 3)` x7 (ConsMgr.c 1, ResearchGUI.c 2, ResearchGUI.h 1, TradeMgr.c 2, Mothership.c's beast-systems radio
buttons 1); `CAT-TODO(stage 4)` x1 (UnivUpdate.c, the computer player's new-ship dock order, re-targeted below);
`CAT-PROVISIONAL` x9 (CommandLayer.h's driver switch 1, ConsMgr.c's class caps 3, ConsMgr.h 1, ResearchGUI.c's print
lists 2, Sensors.c's render-flags table and derelict table 2, UnivUpdate.c's external-build start 1; the derelict
table's note rewritten as a fact). No `CAT-TODO(M2-R6 ConsMgr|sensors|research)` is left in the tree.

Re-targeted (added, owner first): `CAT-TODO(stage 6 tutorial)` x8 (cat_tutButtonsHideForScreen /
RestoreAfterScreen in the three managers), `CAT-TODO(stage 5 recorder)` x9 (the recorder tests), `CAT-TODO(stage 5
multiplayer options)` x1 (the crystals option [audit: said infection option]), `CAT-TODO(stage 4 AI)` x1 (`cat_univConvertShip`'s computer-player dock
order), `CAT-TODO(stage 6 KAS)` x1 (`SelectionSwitchPlayerOwner`'s second argument), `CAT-TODO(stage 3 FOW)` x2 (the
scene's FOW render), `CAT-TODO(M2-R6 NIS/camera)` x1 (the motion cues in `smSensorsCloseForGood`).

## Build

`tools/build.sh --debug` and `--release`: green, 0 compiler warnings (the debug build prints meson's sanitizer note
only). Every touched C file compiles with `-Werror=implicit-function-declaration` (no new implicit declaration;
Dock.c's `ioUpdateShipTotals` / `cat_shipReleaseAttachedRammers` and CommandLayer.c's `oracleSetMission` are the base's).

## Final gate runs (plain harness)

| run | mode | milestone | first stop | result |
|---|---|---|---|---|
| `r6mg-final-build-a` / `-b` | build | build self-test done | none, ASan 0 | PASS both at tick 4205; `run.hwo` sha256 `237157c9e5f7730e` both, byte-identical |
| `r6mg-final-mission-a` / `-b` | mission 6000 | oracle mission run done | none, ASan 0 | identical streams `b0129e78bc6b9a50` (above) |
| `r6mg-final-dock` | dock | dock self-test done | none, ASan 0 | PASS |
| `r6mg-final-hud` | hud | hud self-test done | none, ASan 0 | PASS (sensors frames) |
| `r6mg-final-sp` | sp (340 s) | universe running | still running at the timeout (as R6's earlier sp runs) | ASan 0, 0 fescreen UBSan |
| `r6mg-final-fe` | fe (90 s) | Main_game_screen | still running at the timeout (as before) | ASan 0, 0 fescreen UBSan |

## DONE WHEN

- `/catBuildTest` (harness mode `build`) with `CAT_EXTERNAL_BUILD_DRIVER` 1 builds an externally built destroyer at the
  sMothership (after its docking bay module) and an internal acolyte on headless Mission01: the external ships appear at
  health 0 and finish at full health, ASan 0, two runs byte-identical: **met** (`r6mg-final-build-a/-b`).
- The dock self-test's sCarrier and sProcessor host legs and the two ACVs judged: **met** (`r6mg-final-dock` PASS).
- The sensors frame of `/catHudTest` draws through `cat_sm*`: **met** (`r6mg-final-hud`).
- The research screen's tech sets at the exe's: **met** (the exe's print lists and state rules, `research`; the build
  test's screens phase checks the list).
- Mission01 stream before / after vs R5: stated (`Mission01 stream before`, `Mission01 stream after`); sp and fe
  unchanged.

## What is left (owner)

- The FOW grids and render (stage 3 FOW): until then the sensors scene hides every grid-tested point.
- The tutorial buttons around the managers (stage 6 tutorial), the recorder tests (stage 5 recorder), the multiplayer
  crystals option [audit: said infection option] (stage 5 multiplayer options), the computer player's dock order after a conversion (stage 4 AI),
  `SelectionSwitchPlayerOwner`'s second argument (stage 6 KAS), the motion cues (M2-R6 NIS/camera), the writers of the
  sensors hide flag (stage 6 single player / NIS).
- HW1's blob render (`smBlobsDraw` and helpers) and the derelict mesh table are uncalled in Sensors.c (cleanup, auditor's
  call); `collCheckShipShipColl`'s other exe-only tests (the clamped-host comparison through `dockingship`) were not
  compared beyond the docking exemption (physics owner).
