<!-- done: filters, catfilters, select, final -->
# M2-R3 producer 3: selection

Status: DONE (2026-10-10); DONE WHEN met (table at the end). Branch `m2`, worktree `.claude/worktrees/m2`. Scope: docs/m2-r2-summary.md "Plan for
M2-R3 and M2-R4", row 3: `ioUpdateShipTotals` in the selection filters, `Select.c`, `ShipSelect.c`
(33 / 44 / 4 HW1-matched functions, 14,536 MODIFIED bytes; 59 cat-new functions, 11,278 B by address neighbour).

Subsystems (the done marker above lists the finished ones):

1. `filters`: the HW1 filters of `ShipSelect.c` to the exe bodies (`ioUpdateShipTotals` after every filter, the new
   "skip disabled ships" arguments and their callers, class tests in place of HW1 type tests).
2. `catfilters`: the Cataclysm-only filters and predicates of the exe's `ShipSelect.c` block (0x00511870-0x00514210),
   the per-ship Cataclysm state they read, and the ship-state queries they call.
3. `select`: `Select.c` (HW1 aliases, picking/geometry deltas, status bars).
4. `final`: delta-map review entries, builds, plain-harness runs.

Method: the r5 decompile (`ghidra/exports/cataclysm_gog_r5/decompiled.c`) read function by function; every Cataclysm
field the exe bodies touch was traced to its writers with a grep over the whole decompile (scratch helpers in
`/tmp/m2r3sel/`: `fn2.py` prints a body, `grepfn.py` names the function of each match, `catnew.py` lists the 59 cat-new
functions with the stage tool's neighbour rule). Struct offsets: exe `Ship` from the CET header
(`reference/CataclysmEngineTweaks/game/Game/spaceobj.h`, whose commented-out asserts put `dockingship` at 0x1C4 and
`specialFlags` at 0x1C8, which the exe bodies confirm: `makeShipsControllable` reads `dockingship` at +0x1C4), exe
`ShipStaticInfo` from the pinned layout in `src/Game/SpaceObj.h`.

## 1. `filters`: the HW1 filters of ShipSelect.c

**Every filter ends with `ioUpdateShipTotals()`.** The exe calls it (through the thunk at 0x00513000) at the end of 45
functions of its `ShipSelect.c` block: 26 HW1 functions and 19 Cataclysm-only ones (list from `grepfn.py` over the
block 0x00511800-0x00514300). In this tree every one of the 45 now does, directly (43 calls) or through a filter it
calls (`selectMergeTwoSelections` via `MakeShipsNotIncludeTheseShips`, `cat_selMakeShipsMobileNotBusy` via
`MakeShipsMobile`). GoK's `ioUpdateShipTotals` (InfoOverlay.c) only acts while the info overlay runs (`ioRunning`),
so calls from game code before the overlay starts are harmless; its body is stage 3's.

**New "skip disabled ships" argument.** Six filters take one more argument in the exe; every exe call site was read
(`grepfn.py`): the UI callers (`mrObjectClick` 0x00566b60, `mrSelectHold` 0x0056a1e0, `cat_mrBandBoxRelease`
0x0056a610, `cat_mrReleaseRight` 0x0056b0b0, `mrHarvestResources` 0x00563cc0, `mrRetire` 0x005653b0, the
`cat_kb*CB` key callbacks) pass 1, the game code (`aiuAttack` 0x0041ea60, `aiuWrapMove` 0x0041eea0, `clAttack`
0x00448950, `clAttackThese` 0x00448620, `clPostProcess` 0x0044cf80, `kasfHarvest` 0x00495f00, `cat_tacticsRetaliate`
0x00529cb0) passes 0. The tree's callers follow that split: `src/SDL/mainrgn.c` (10 calls), `src/SDL/mouse.c` (2),
`src/Game/Sensors.c` (1) pass `TRUE`; `src/Game/CommandLayer.c` (4), `src/Game/AIUtilities.c` (1),
`src/Game/KASFunc.c` (1) pass `FALSE`; each call carries a `CAT:` comment [audit: the `mrObjectClick` harvest call
had none, added; the `clMove` comment now says the exe's `clMove` 0x00446d90 has no `MakeShipsMobile` call (its only
game-code caller is `aiuWrapMove`)]. (`src/Win32/` is reference only, not built.)

| function (exe address) | Cataclysm delta ported |
|---|---|
| `MakeShipsNotIncludeTheseShips` 0x00511940 | `ioUpdateShipTotals` (the exe inlines `clRemoveShipFromSelection`) |
| `isShipPassiveAttackCapable` 0x00511b10 | FALSE for a ship in formation type 9 or 10 (sentinel fields) or being built (specialFlags2 bit 3 in the exe) |
| `MakeShipsAttackCapable` 0x00511b60 | 3rd argument (skip disabled); drops a Sentinel holding a field (`cat_shipIsSentinelInField` 0x005c6ec0) |
| `ShipCanHarvest` 0x00513680 | resource class with `harvestAmount > 0` (HW1: type ResourceCollector) |
| `MakeShipsHarvestCapable` 0x00511be0 | 3rd argument: only non-disabled sWorker/bWorker; harvesting must be on (`singlePlayerGame` or `MG_HarvestinEnabled`, `tpGameCreated.flag` = exe 0x008860ec) |
| `ShipCanGuard` (inlined in `MakeShipsGuardCapable` 0x00511c70) | no mothership-class, component-class or `externalBuild` ship (HW1: Mothership and Probe types) |
| `MakeShipsGuardCapable` 0x00511c70 | 2nd argument (skip disabled) |
| `makeShipsRetireable` 0x00511db0 | removes `tResourceCollector` (type 0x58; HW1 removed Probe/TargetDrone) |
| `makeShipsControllable` 0x00512020 | the "cannot comply" chatter is spoken for the selected ship with its type as the variable; no research-ship exception |
| `makeSelectionHyperspaceCapable` 0x005120d0 | also removes: a host whose clamped list (`ClampInfo.clampedObjs`) holds a ship being built; a ship being salvaged into a depot holding a crystal; a Sect mothership on `COMMAND_SPECIAL` or with its big gun active |
| `MakeShipsSpecialActivateCapable` 0x00512250 | 2nd argument (skip disabled, except sLeech, sSentinel, bSentinel) |
| `MakeShipsSpecialTargetCapable` 0x005122c0 | 3rd argument (skip disabled); `bFriendlies` and `canSpecialBandBoxFriendlies` are ignored, as in the exe |
| `MakeShipsMobile` 0x005126b0 | 2nd argument (skip disabled); the "mothership cannot move" speech for any mothership-class ship of the player |
| `makeShipsFormationCapable` 0x00512780 | removes disabled ships (HW1 removed Probes) |
| `MakeShipsNonCapital` 0x005127c0, `MakeShipsOnlyCapital` 0x00512810, `MakeShipsOnlyFollowConstraints` 0x00512860, `MakeTargetsNotIncludeMissiles` 0x00512930, `MakeTargetsOnlyBeWithinRangeAndNotIncludeMe` 0x00512d80, `MakeShipsFriendlyShips` 0x00512e50, `MakeShipsFriendlyAndAlliesShips` 0x00512ea0, `MakeShipsNotHidden` 0x00513010 | `ioUpdateShipTotals` only |
| `MakeTargetsOnlyEnemyShips` 0x00512970 | also drops KAS non-retaliation ships (exe specialFlags 0x40000 = `SPECIAL_FriendlyStatus`, proven by `kasfShipsSetNonRetaliation` 0x00495da0) and crystals; no Drone test |
| `MakeTargetsOnlyNonForceAttackTargets` 0x005129f0 | rewritten as the exe: keeps another player's derelict-race ship (not a crystal), the hyperspace gate (derelict type 7), enemy ships unless crystal, KAS derelict (`Ship+0x18` bit 4, `kasfShipsSetDerelict` 0x00498510) or a mimic that looks allied/resource/derelict, and mines (`missileType == MISSILE_Mine`); HW1's KillerCollDamage exception and Drone test are gone; an enemy `trJunkYardHQ` is still removed, as in HW1 |
| `makeShipsNotHaveNonCombatShipsForGuardAttack` 0x00512ca0 | the six exe types: k/tCloakGenerator, k/tGravWellGenerator, bDFGFrigate, tDFGFrigate (HW1: three aliases) |
| `MakeSelectionNotHaveNonVisibleEnemyShips` 0x00512d00 | fully cloaked (`cat_shipIsCloakedFully` 0x00436110: `SOF_CatCloakDevice` and `SOF_Cloaked`) and not detected by the player (`sub_53F9F0` 0x0053f9f0); HW1 asked the proximity sensors |
| `selectAllCurrentPlayersNonHyperspacingShips` 0x00512f00, `selectAllCurrentPlayersHyperspacingShips` 0x00512f80 | `cat_spShipCanHyperspace` 0x00517650 plus "not capital and not component" / "capital or component" |
| `MothershipOrCarrierIndexInSelection` 0x00513310 | by class (Mothership returns, Carrier remembered) |
| `growSelectAddShip` 0x00513350 | grows with `memRealloc` |
| `MakeSelectionKamikazeCapable` 0x005134e0 | keeps fighter, corvette and resource classes |

HW1 functions the exe does not have (no name, no call in the r5 decompile): `ShiptypeInSelection` (the exe has the
same counter as `selNumShipsInSelection` 0x005118d0), `isShipAttackCapable`, `AreAny/AllShips(Passive)AttackCapable`,
`makeShipsDockCapable`, `makeShipsNotIncludeSinglePlayerMotherships`, `MakeTargetsSalvageable`,
`MakeSelectionNotHaveCloakedEnemyShips`, `MakeShipMastersIncludeSlaves`, `selectAllPlayersShips`,
`MakeSelectionNonResearchShips`, `MakeShipsSingleClickSpecialCapable`, `shipLinkedListAsSelectCommand`,
`clRemoveTargetFromSelection` (the exe's `clRemoveShipFromSelection` 0x00511830 is the same body). They stay for
their unported HW1 callers (stage 3 UI, stage 6). Where their bodies tested HW1-only types, the test is gone with a
`CAT:` note: `makeShipsDockCapable` (Probe/TargetDrone/SensorArray) and `MakeSelectionNonResearchShips` (ResearchShip)
now remove nothing, `MakeTargetsSalvageable` lost its Drone and CryoTray/Mission-1 exceptions; all behave as before
on Cataclysm data, where those types cannot occur.

`ShipSelect.h`: the six new prototypes; `ShipCanHyperspace` is `isCapitalShip(ship)` (its HW1 CryoTray/Probe terms
named dormant ids only).

## 2. `catfilters`: Cataclysm-only functions and the state they read

**Per-ship state (`src/Game/SpaceObj.h`).** The exe filters read Cataclysm fields of its own 0x63C-byte `Ship`. This
tree's `Ship` is GoK's, so they are appended by name (zeroed by `univCreateShip`'s memset, as the exe zeroes its
ship); each was traced to its writers:

| field | exe | meaning (writers) |
|---|---|---|
| `catShipFlags` | Ship+0x18 | bit 3 `CAT_SHIPFLAG_Mimic` (`MimicInit` 0x005b8360, `MCVInit` 0x005b8130, `P1StandardCorvetteInit` 0x005be660); bit 4 `CAT_SHIPFLAG_KasDerelict` (`kasfShipsSetDerelict` 0x00498510); bit 1 `CAT_SHIPFLAG_NoTactics` (`cat_shipCanSetTactics` reads it; setter not located) |
| `catDisableReason[13]`, `catDisabled` | Ship+0x1DC, Ship+0x210 | disabled while any reason is set (`cat_shipSetDisableReason` 0x005b3fe0, `cat_shipSetHelpless` 0x005b3bd0, `cat_shipClearHelpless` 0x005b3c80, `cat_empShockShip` 0x005b3900, `cat_shipUpdateDrift` 0x005b3d60) |
| `catUpgradeFlags` | Ship+0x3F4 | `cat_shipHasUpgrade` 0x00514210 |
| `catDissolving` | Ship+0x570 | `cat_shipDissolveStart` 0x004603d0 / `cat_shipDissolveInit` 0x00460220 |
| `catUpgrading` | Ship+0x598 | `cat_clUpgradeShips` 0x0044de80 sets, `cat_univShipDeferredUpdate` 0x005447b0 clears |
| `SPECIAL_2_CatBeingBuilt`, `SPECIAL_2_CatBeingSalvaged` | specialFlags2 (Ship+0x1CC) bits 3 and 13 | being built externally (`cat_clExternalBuildUpdate` 0x0044dfc0 clears it); being salvaged (`cat_salvageIsBeingSalvaged` 0x004f7c00). In this tree they take free bits 8 and 9: the exe reuses bit 3, which HW1 used for `SPECIAL_2_CircularGuard` and which Cataclysm moved into specialFlags (`clProtect` 0x0044af20 clears specialFlags 0x40000000); the full specialFlags/specialFlags2 renumbering belongs to the CommandLayer port (M2-R4 #2) |

None of their setters is ported yet (each belongs to ship code B, the command layer, salvage or construction), so on
the current program every new test sees the value the exe has for a ship that is not disabled, not upgraded, not
dissolving, not being built or salvaged and not a mimic. The fields are the hook those ports fill.

Cataclysm-only functions ported, in `ShipSelect.c` and declared in `ShipSelect.h` (exe names and addresses kept):

| function | exe | body |
|---|---|---|
| `cat_selSelectionAddShip`, `cat_selSelectionCopy`, `cat_growSelectNullShip` | 0x00511870, 0x005139b0, 0x005133e0 | append with the 1000 limit; copy at most 255 pointers but take the source count (as the exe); null the first match |
| `cat_selMakeShipsScuttleable(sel, check)` | 0x00511ce0 | with check: drop disabled ships and Sect/Beast carriers and motherships (chatter from `selSelected.ShipPtr[0]`) |
| `cat_selMakeShipsNotLinkedACVs` | 0x00511df0 | drop sACV/bACV with link state 1 or 2 |
| `cat_selMakeShipsSameOwnerAsFirst` | 0x00511e50 | |
| `cat_selMakeShipsHaltable(sel, check)` | 0x00511ea0 | with check: drop disabled ships that are not upgrading |
| `cat_selMakeShipsProtectCapable(sel, check)` | 0x00511f00 | drop disabled (check) and ships on orders 4, 6, 10, 11, 12, 14-17 |
| `cat_selMakeShipsMobileNotBusy` | 0x00511f90 | `MakeShipsMobile(sel, TRUE)` (the exe body is that copy) |
| `cat_selMakeShipsHaveAbilityUpgrade`, `cat_selMakeShipsHaveUpgrade(sel, flags)` | 0x00512330, 0x005124e0 | upgrade bits 0x1000/0x2000/0x4000; any of `flags` |
| `cat_selMakeShipsLeechCapable`, `cat_selMakeShipsLeeching` | 0x005123a0, 0x00512650 | `cat_shipCanLeech`; leech upgrade and leeching |
| `cat_selMakeShipsRammingFrigates(sel, check)` | 0x00512410 | s/bRammingFrigate (not disabled with check) |
| `cat_selMakeShipsRepairCapable`, `cat_selMakeShipsAcolyteLinkFire` | 0x00512470, 0x00512530 | `cat_shipCanRepair`; `cat_acolyteCanFireLinkedWeapon` 0x005ada50 |
| `cat_selMakeShipsNotBigShips` | 0x00512580 | drop sMothership, sCarrier, sDreadnought, sDestroyer, sProcessor, bMothership, bCarrier, bHeavyCruiser, bProcessor |
| `cat_selMakeShipsNotBusy` | 0x005125f0 | drop disabled ships except sLeech, sSentinel, bSentinel |
| `cat_selMakeTargetsAttackable(t, force)`, `cat_selMakeTargetsBeastAttackable` | 0x00512b50, 0x00512c40 | drop untargetable types (`catTypeFlags` bit 14), sAcolyte, dissolving, hyperspacing (exe specialFlags 0x200000 = `SPECIAL_Hyperspacing`: `attackPassive` 0x00421c30 tests it where HW1 does), dEcclesiamInfected, an up Sentinel field outside single player without force, dust clouds; then crystals |
| `cat_selMakeShipsDockCapableFor`, `cat_shipClassCanUseSlipgate` | 0x00513710, 0x00513800 | slipgate size 1/2/3 (dSlipgateSmall/Medium/Large) against the class size table |
| `cat_shipCanMove`, `cat_shipCanDock`, `cat_shipCanBeHalted`, `cat_shipCanSetTactics`, `cat_shipCanCollectResources`, `cat_shipCanLeech`, `cat_shipCanUnleech`, `cat_shipCanRepair`, `cat_shipCanSalvage` | 0x00513540-0x005138f0 | as the exe |
| `cat_shipHasUpgrade`, `cat_upgradeFlagsBaseTime`, `cat_shipUpgradeFlagsTime` | 0x00514210, 0x00513f40, 0x00514080 | 10 s per upgrade bit, 20 s for 0x10000 (constants 0x0072a4f0 = 10.0f, 0x0072a4d0 = 20.0f read from the exe), times `upgradeTimeModifier` |

Ship-state queries the filters call (exe homes elsewhere; they sit at the end of `ShipSelect.c` until their owners
are ported, then move): exact now: `cat_shipIsCrystal` 0x005affa0 (dCrystal0/1), `cat_shipIsCloakedFully` 0x00436110,
`cat_shipIsUntargetable` 0x0052a060, `cat_shipIsSentinelInField` 0x005c6ec0, `cat_salvageIsBeingSalvaged` 0x004f7c00,
`cat_acolyteCanFireLinkedWeapon` 0x005ada50 (the exe's `Gun` has a leading upgrade dword, so its +0x68 is
`numMissiles`), `cat_spShipCanHyperspace` 0x00517650. `CAT-PROVISIONAL` (they read ship specifics that are not ported;
each returns the exe's value for a ship whose ability never activated): `cat_shipIsSentinelFieldActive` 0x005c6e50,
`cat_leechIsLeeching` 0x005b7ce0, `cat_acvLinkState` (Ship+0x6F0), `cat_salvageDepotHoldsCrystal`
(`cat_salvageGetDepotInfo` 0x004f91c0), `cat_mothershipBigGunActive` 0x005bbf60 (the clamped-list scan is ported,
the firing byte Ship+0x674 is not), `cat_shipDetectedByPlayer` = `sub_53F9F0` 0x0053f9f0 (stealth types count as
detected), the mimic queries `cat_shipIsMimicking` 0x005b99b0, `cat_mimicGetApparentShipType` 0x005b97e0,
`cat_mimicGetSelectRadius` 0x005b94e0, `cat_mimicAppearsAlliedTo` 0x005b96f0, `cat_mimicAppearsAsResource`
0x005b9860, `cat_mimicAppearsAsDerelict` 0x005b98d0 (no mimic is active until `MimicInit` sets `CAT_SHIPFLAG_Mimic`).

Not ported here, with owner: `cat_salvageTargetAllowed` 0x00513920 (calls `cat_salvageCheckTargetReasons`: salvage,
M2-R4 #3); `cat_shipUpgradesInit` 0x005140a0, `cat_playerUpgradesReset` 0x00514100,
`cat_playerUpdateUpgradesFromResearch` 0x00514140 (Player fields +0x6504..+0x652c, tech tables: research, M2-R4 #5);
`cat_selComputeTargetActions` 0x00513a00 (popup menu, stage 3); the eleven `cat_shipPrint*` functions
0x00510940-0x00511720 (ship info panel drawn by `smViewportRender` and the info overlay, stage 3).

Build after subsystems 1-2: `tools/build.sh --debug` and `--release` rc 0, no `warning:`/`error:` line;
`tools/m2_layout32.sh` OK. Plain-harness sp run `r3-selection-filters-sp`: unchanged from the etg audit's
`audit-r3-etg-sp` (milestone level load, first stop `GenericInterceptor.c (205)`, ASan 0).

**Spot checks of filter bodies against the exe** (each row: the exe's test as read in the r5 decompile, and how
`src/Game/ShipSelect.c` implements it):

| exe function | exe test (decompile) | port |
|---|---|---|
| `MakeShipsGuardCapable` 0x00511c70 | remove if `static+0x1e8 == 0 \|\| == 10 \|\| static+0x1f0 != 0 \|\| (arg && ship+0x210)`, then `ioUpdateShipTotals()` | `!ShipCanGuard(shipstatic)` (class Mothership/Component, `externalBuild`) `\|\| (skipDisabled && catDisabled)`; `ioUpdateShipTotals()` |
| `makeShipsRetireable` 0x00511db0 | remove if `ship+0x2b0 == 0x58` | `ship->shiptype == tResourceCollector` (88) |
| `MakeShipsSpecialActivateCapable` 0x00512250 | remove if `static+0x124 == 0 \|\| (arg && ship+0x210 && type not in {3, 0x11, 0x34})` | `CustShipSpecialActivate == NULL` (static+0x124 = custshipheader+0x1C) `\|\| (skipDisabled && catDisabled && type not sLeech/sSentinel/bSentinel)` |
| `makeShipsNotHaveNonCombatShipsForGuardAttack` 0x00512ca0 | remove types 0x3f, 0x40, 0x4d, 0x4f, 0x2d, 0x4e | kCloakGenerator 63, kGravWellGenerator 64, tCloakGenerator 77, tGravWellGenerator 79, bDFGFrigate 45, tDFGFrigate 78 |
| `MakeTargetsOnlyEnemyShips` 0x00512970 | keep if `objtype 0 && owner != cur && !(ship+0x1c8 & 0x40000) && !ally && type != 0x7e && != 0x7f` | `OBJ_ShipType`, owner, `SPECIAL_FriendlyStatus`, `allianceIsShipAlly`, `cat_shipIsCrystal` (dCrystal0 126 / dCrystal1 127) |
| `MakeSelectionKamikazeCapable` 0x005134e0 | keep classes 6, 5, 7; return `numShips > 0` after `ioUpdateShipTotals()` | `CLASS_Fighter`, `CLASS_Corvette`, `CLASS_Resource`; same return |
| `cat_selMakeShipsNotBigShips` 0x00512580 | remove types 0x12, 0xc, 10, 0x19, 0x10, 0x35, 0x2f, 0x2e, 0x33 | sMothership 18, sCarrier 12, sDreadnought 10, sDestroyer 25, sProcessor 16, bMothership 53, bCarrier 47, bHeavyCruiser 46, bProcessor 51 |
| `cat_shipClassCanUseSlipgate` 0x00513800 | gate size 1/2/3 for types 0x70/0x71/0x72; classes {0,9} need 3, {1,2,3,4,7} 2, {5,6,8,10,11} 1, other 4 | `cat_slipgateSize` (dSlipgateSmall 112, Medium 113, Large 114) and `cat_shipClassGateSizeNeeded` |

Coverage check (a scratch script over the source, `grepfn.py` over the exe block): the 45 exe functions of the block
that call `ioUpdateShipTotals` are exactly the 45 ported functions that call it, directly or through
`MakeShipsMobile`/`MakeShipsNotIncludeTheseShips`/`cat_selMakeTargetsAttackable`. The one extra is HW1's
`makeShipsNotIncludeSinglePlayerMotherships` (not in the exe), which calls `MakeShipsMobile`.

## 3. `select`: Select.c

The exe's `Select.c` changes split into two groups.

**Ported here** (alias retirement plus the deltas whose inputs exist in this tree):

| function (exe) | change |
|---|---|
| `selRectDragFunction` 0x005053a0, `selSelectionClick` 0x00505ed0, `selClickFromArray` (GoK, not in the exe) | the cloak test is the exe's: a fully cloaked (`cat_shipIsCloakedFully`) enemy ship the player has not detected (`sub_53F9F0` = `cat_shipDetectedByPlayer`) is skipped; HW1 asked the proximity sensors (`ProximitySensor.h` include gone). `selSelectionClick` no longer tests HW1's drone type |
| `selShipUnderMouse` 0x00506550 | the trader special case keys on `trFloatingCity` (the exe compares type 0x62); every resource under the cursor is a cursor object (HW1 took dust clouds only while a resource collector was selected); the `PIE_MOVE_NEARTO` block (compiled out) tests the mothership class |
| `selStatusDraw0/1/2` 0x00507f10 / 0x005080f0 / 0x005082c0 | HW1 alias tests retired the way the exe's status bars handle these ships: no cloak-generator bar (`k/tCloakGenerator` run on `DefaultShipHeader`; the exe draws a cloak charge only for `SOF_CatCloakDevice` ships); the gravwell bar for `k/tGravWellGenerator` (the exe's bar: `OperationTime` at custstatinfo+8 over `TimeOn` at specifics+0x14, HW1's structs); no salvage-capture tech bar (the exe draws none); the missile count for sDreadnought, sDestroyer, tMissileDestroyer, tMinelayerCorvette (`cat_selStatusAmmoCountDraw` 0x00507740's types, without the linked Acolytes); docked counts without the DDD-frigate exclusion (`CloakGenerator.h`/`SalCapCorvette.h` includes gone) |
| `selSelectedDraw` 0x005086a0 | the one-LOD-finer rule for any mothership-class ship (the exe tests shipclass 0) |
| `selStartup` 0x00505300 | registers a fifth font, `default.hff`, as `selDefaultFont` (exe 0x00c0bae8) |
| `selCentrePointComputeGeneral` 0x00508be0 | ships and missiles count; a mimic's select radius (`cat_mimicGetSelectRadius`); scale 1 where `magnitudeSquared` is 0; the missile's scale field (exe Missile+0x254) is `CAT-PROVISIONAL` 1 |
| `selSelectionDimensions` 0x00509250 | a mimic's select radius |

`Select.c` keeps one HW1 ship-file include, `GravWellGenerator.h`, for the gravwell bar's structs. That is the HW1
ship code the exe also runs for `k/tGravWellGenerator` (`Universe.c` header table), not an alias use. No HW1 alias
identifier and no dormant HW1-only id is left in `Select.c`, `Select.h`, `ShipSelect.c` or `ShipSelect.h`: the
`grep -w` check over the 37 alias names and 22 dormant ids, comments included, lists only that include line.

**Deferred, with the reason** (the exe rewrote these together with their callers, which are stage-3 code):

- The picking API. `selRectDragFunction` takes 9 arguments: a centre-point mode, fog-of-war hiding through
  `cat_selPositionHiddenByFOW`/`cat_selShipSelectableInFOW`, component ships resolved to their host, mines, the
  mission-3 rule, mimic disguises, and `cat_shipSetDisableReason(ship, 1, 7)` on an allied mimic.
  `selRectDragAddFunction` takes 4 arguments and toggles the dragged ships off when all of them are already selected.
  `selSelectionClick` takes 7 arguments: a zero-radius object is picked at the exact cursor pixel, and a component
  class returns its host. `selShipUnderMouse` takes 1 argument and applies fog of war through
  `cat_fowCanPlayerSeeShip`. `selCircleCompute` and `selSelectingDraw` are also in this group. Their exe callers are
  the rewritten mainrgn functions (`mrSelectHold`, `cat_mrBandBoxRelease`, `cat_mrSelectRectDraw`,
  `cat_mrClickRelease`, `cat_mrReleaseRight`), so the new arguments have no callers in this tree yet, and the
  fog-of-war module is not ported. Owner: stage 3 (mainrgn/mouse and the fog-of-war code), together with this file's
  picking functions.
- The status-bar rewrite. `selStatusDraw0-4` become thin wrappers over `cat_selStatusRectCompute` 0x00507390 and
  `cat_selStatusBarsDraw` 0x00507bc0. The rows are health, ACV EMP charge, resources, beast weapon, Dreadnought
  cannon, big gun, cloak charge, gravwell, Sentinel field, upgrade progress (`Ship+0x598..0x5a0`), upgrade pending,
  disabled icon, stealth-detect dots per player, icons (`Ship+0x5f8`), hot-key and ammo count. The group also takes
  in the eight other `cat_selStatus*`/`cat_selHealth*` helpers (0x005074f0-0x00507b70) and `selSelectedDraw`'s
  focus-ship health bar. Most rows read charge sources in the Cataclysm-only ship code and call
  `cat_overlayIconDraw` (stage 3), and the rectangle maths sits in `__ftol` operands that the decompile hides, so
  the port needs the disassembly. Owner: stage 3 (UI), after M2-R4 #4 (ship code B) provides the charge sources.

## 4. `final`: delta-map reviews, builds, harness

**Delta-map review entries** (`build/m2r3-sel/review_sel.txt`, sample `port-R3`, applied with
`tools/delta_map.py mark` and then `merge --done m2r3:selection-review`): `ghidra/delta_map_review.json` went from
1,247 to 1,272 entries, and 35 records of `ghidra/delta_map.json` changed.

- 1 class change: `selectSelectionIntersection` 0x00513060 MODIFIED -> **IDENTICAL**/high (the automatic
  differences are the inlined `selectMemDupSelection` and `clRemoveShipFromSelection`).
- 1 corrected description: `isShipPassiveAttackCapable` 0x00511b10 (formation types 9/10 and the being-built bit,
  not "disabled ships, two ship types").
- 31 MODIFIED entries confirmed with the exact change and raised to high: the other ported `ShipSelect.c` functions
  (list in the review file), including two whose tails the earlier text had not read (`MakeShipsNotHidden`
  0x00513010, `selectMergeTwoSelections` 0x00513130).
- 1 header-level IDENTICAL/high: `selSelectHotKeyGroup` 0x00509030 (mask 0x2a08000 = HW1's four flags renumbered).
  [audit: the entry's note in `delta_map_review.json`/`delta_map.json` said the mask "is HW1's SOF_Hide"; it now
  names the four flags SOF_Hide|SOF_Disabled|SOF_Hyperspace|SOF_Crazy and SOF_Selectable.]
- 1 outside the scope: `allianceIsShipAlly` 0x00420ec0 (Alliance.c) MODIFIED/high: FALSE for crystals and for
  non-ship objects before HW1's test.
- No function that the map calls IDENTICAL was found modified in the scope. Spot-checked against HW1:
  `selSelectionAddSingleShip`, `selSelectionRemoveSingleShip`, `selHotKeyNumbersSet`, `selSelectionCopy`,
  `selSelectionIsReinforced`, `growSelectInit`, `AreAllShipsAttackCapable`, `AreShipsMobile`.

`tools/m2_stage_counts.py` (before -> after; `build/m2r3-sel/stage_counts_{before,after}.txt`): stage 2 IDENTICAL
830 -> **831** (167,487 -> 167,690 B), MODIFIED 543 -> **542** (234,204 -> 234,001 B), REWRITTEN 29, cat-new 829;
total IDENTICAL 3,136 -> 3,137, MODIFIED 1,208 -> 1,207.

**Builds** (final tree): `tools/build.sh --debug --clean` and `--release --clean` both rc 0, `[292/292] Linking target
hwi-engine`, 0 `warning:`/`error:` lines under `-Werror` (logs `build/m2r3-sel/build-{debug,release}-clean.log`).
Debug 76,684,752 B, release 4,797,856 B. The eight changed `.c` files also compile with
`-Werror=implicit-function-declaration`; the tree's flags otherwise turn that warning off. `tools/m2_layout32.sh`: OK.

**Plain-harness runs** (`original/scratch-m2/m2r2-harness/runs/<label>/`, GOG data, no workarounds, no input):

| run | mode | rc / wall | milestone | first stop | sanitizers |
|---|---|---|---|---|---|
| `r3-selection-final-fe` | fe, 40 s | 124 / 40.3 s | **`Main_game_screen`** | none (timeout) | ASan 0, LeakSanitizer clean; UBSan only the known `fquant.c`/`fescreen`/`PATCH` reports; 232 file loads (as `audit-r3-etg-fe`) |
| `r3-selection-final-sp` | sp, 90 s | 1 / 10.1 s | **level load (mission file read)** | `Ships/GenericInterceptor.c (205)` (M2-R3 #4's stop) | ASan 0; 250 file loads; the 44-byte leak summary on the assert exit, as in `audit-r3-etg-sp` |
| `r3-selection-statics` | statics | 0 | ship statics self-test done | none | ASan 0, LeakSanitizer clean |
| `tools/oracle/selftest.sh --label r3-selection-oracle-rel` | oracle (release) | | A/B identical; perturbed copy found at tick 400, `ships_pos` | | `SELFTEST PASS` |

Earlier runs of this port gave the same results: `r3-selection-filters-sp` (after subsystems 1-2),
`r3-selection-select-fe` and `r3-selection-select-sp` (after subsystem 3).

## Files changed

| file | what |
|---|---|
| `src/Game/ShipSelect.c` | sections 1-2: the HW1 filters, 33 Cataclysm-only functions of the block (+ `cat_shipHasUpgrade`), 19 ship-state queries |
| `src/Game/ShipSelect.h` | new prototypes, `ShipCanHyperspace` without dormant ids, Cataclysm declarations |
| `src/Game/SpaceObj.h` | `Ship` fields `catShipFlags`, `catDisableReason[13]`, `catDisabled`, `catUpgradeFlags`, `catDissolving`, `catUpgrading`; `SPECIAL_2_CatBeingBuilt/CatBeingSalvaged`, `CAT_SHIPFLAG_*`, `CAT_NUM_DISABLE_REASONS`, `CAT_UPGRADE_*` |
| `src/Game/Select.c` | section 3 |
| `src/SDL/mainrgn.c`, `src/SDL/mouse.c`, `src/Game/Sensors.c` | UI callers pass TRUE for the new arguments |
| `src/Game/CommandLayer.c`, `src/Game/AIUtilities.c`, `src/Game/KASFunc.c` | game-code callers pass FALSE |
| `ghidra/delta_map_review.json`, `ghidra/delta_map.json` | 35 review entries |
| `docs/m2-r3-selection.md` | this report |

`git diff --stat -- src/`: 10 files, +1,395 / -449; 203 added lines carry `CAT` tags. Nothing is committed, and no
game data is in the tree (scratch: `/tmp/m2r3sel/`, `build/m2r3-sel/`).

## Counts

Scope 33 / 44 / 4 HW1-matched functions, 59 cat-new.

- `ShipSelect.c` MODIFIED: 33. Of these, 30 changed in the source. The other 3 match the exe without a source change:
  `selectSelectionIntersection` is now IDENTICAL; `growSelectAddShipNoDuplication` and `selectMergeTwoSelections`
  get the exe's behaviour through the ported `growSelectAddShip` and `MakeShipsNotIncludeTheseShips`.
- `Select.c` MODIFIED 11 + REWRITTEN 4. 3 are ported in full (`selStartup`, `selCentrePointComputeGeneral` apart
  from the provisional missile scale, `selSelectionDimensions`). 7 are ported in part: the alias and cloak tests of
  `selRectDragFunction`, `selSelectionClick`, `selShipUnderMouse`, `selSelectedDraw` and `selStatusDraw0/1/2`.
  5 are deferred: `selRectDragAddFunction`, `selCircleCompute`, `selSelectingDraw`, `selStatusDraw3/4`.
- IDENTICAL 33: unchanged (8 spot-checked).
- cat-new 59: **33 ported** (3,073 B), 26 deferred (8,205 B: 10 `cat_selStatus*`/`cat_selHealth*`, 11
  `cat_shipPrint*`, `cat_salvageTargetAllowed`, `cat_selComputeTargetActions`, the 3 upgrade-state functions).
  In addition, 20 functions from other exe blocks: `cat_shipHasUpgrade` and the 19 queries.

## DONE WHEN

| condition | result |
|---|---|
| the filters compile against the new flags | **met**: they use producer 1's `SOF_*` values (`SOF_Dead` 0x800, `SOF_Disabled` 0x800000, `SOF_Hide` 0x8000, `SOF_Cloaked` 0x400, `SOF_CatCloakDevice` 0x200); clean builds above |
| and match the exe's filter bodies (spot-checked in the report) | **met**: every MODIFIED filter of the block was read and ported (section 1); 8 spot checks in the table; `ioUpdateShipTotals` coverage 45/45 |
| no HW1 alias left in Select.c and ShipSelect.c | **met** (one HW1 ship-header include, `GravWellGenerator.h`, remains: a file name, see section 3) |
| fe and sp unchanged or further | **met**: fe reaches `Main_game_screen`, sp reaches level load and stops at `GenericInterceptor.c (205)`, both as before, ASan 0 |

## What is left, and its owner

1. Setters of the new per-ship state. `catDisableReason`/`catDisabled`: `cat_shipSetDisableReason` 0x005b3fe0 and the
   helpless/EMP/drift functions 0x005b3900-0x005b3d60, M2-R4 #4 (ship code B). `catShipFlags`: the Mimic/MCV/P1
   inits, M2-R4 #4; the KAS derelict flag, stage 6. `catUpgradeFlags` and `catUpgrading`: upgrades, M2-R4 #5
   (research). `catDissolving`: dissolve, M2-R4 #4. `SPECIAL_2_CatBeingBuilt`: external build, M2-R4 #2.
   `SPECIAL_2_CatBeingSalvaged`: salvage, M2-R4 #3.
2. The `CAT-PROVISIONAL` queries: `cat_shipIsSentinelFieldActive`, `cat_leechIsLeeching`, `cat_acvLinkState`,
   `cat_salvageDepotHoldsCrystal`, the firing byte of `cat_mothershipBigGunActive`, `cat_shipDetectedByPlayer` for
   the stealth types, and the six mimic queries. Their state lives in Cataclysm ship specifics (M2-R4 #4; the depot,
   M2-R4 #3). Once that state is ported, the queries move to their home files.
3. The renumbering of specialFlags/specialFlags2 to the exe's bits (FriendlyStatus 0x40000, Hyperspacing 0x200000,
   CircularGuard into specialFlags 0x40000000, the two new specialFlags2 bits 0x8 and 0x2000): M2-R4 #2 (command
   layer).
4. The Cataclysm command orders 11-17 and formation types 9/10 (named `CAT_COMMAND_*`/`CAT_FORMATION_*` in
   `ShipSelect.c` until then): M2-R4 #2.
5. The deferred cat-new functions: `cat_salvageTargetAllowed` 0x00513920 (M2-R4 #3, salvage); the upgrade state
   `cat_shipUpgradesInit`/`cat_playerUpgradesReset`/`cat_playerUpdateUpgradesFromResearch` (Player fields +0x6504..,
   M2-R4 #5); `cat_selComputeTargetActions` 0x00513a00 (popup menu) and the eleven `cat_shipPrint*` (ship info panel),
   both stage 3.
6. `Select.c`: the picking API and the status-bar rewrite (section 3 "Deferred"): stage 3; the status bars after
   M2-R4 #4.
7. The HW1 functions the exe does not have (section 1 list) stay for their unported callers. Whoever ports those
   callers removes them: stage 3 for mainrgn/mouse, stage 6 for SinglePlayer, M2-R4 #2 for CommandWrap.
8. The sp mode still stops at `GenericInterceptor.c (205)`: M2-R3 #4 (ship code A).
