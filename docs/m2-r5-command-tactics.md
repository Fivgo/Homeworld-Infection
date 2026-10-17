<!-- done: layout-move, orders, formation, attack-misc, tactics, final -->
# M2-R5 producer 2: command layer follow-up, force-field formations, tactics

Status: **done** (every subsystem in the marker). Branch `m2`, worktree `.claude/worktrees/m2`, base commit `6f64a21`
(nothing committed by this producer: the audit checkpoints). Scope: docs/m2-r4-summary.md "Plan for M2-R5 and M2-R6"
row 2 and "What is left of stage 2" item 2, with the item-6 leftover `cat_beastGetAttackStaticInfo`.

Done when (row 2): every MODIFIED body of `CommandLayer.c`, `CommandWrap.c`, `Formation.c`, `Tactics.c` ported or
listed with an owner outside stage 2; a scripted `/catCommandTest` (link, unlink, point, mimic, research orders and a
Sentinel force-field wall and sphere over N ticks) completes with the exe's end states and ASan 0; sp as before, ASan 0;
two 6,000-tick mission streams byte-identical. **Met** (sections "Self-test" and "Builds and harness runs").

Subsystems:
- `layout-move`: the exe's `CommandToDo` (0x330) with the waypoint `MoveCommand` (0x108: path type, 21 points, current
  point), the saved looping move (+0x120/+0x124), the move and hyperspace order families, the cleanup functions with the
  exe's reason codes, `clProcess`, the retirement of `Ship.formationcommand` (`cat_shipFormationCommand`).
- `orders`: orders 11-16 (link, unlink, point, mimic, research, upgrade), the link flight, the `cat_cl*` helpers, the 10
  `cat_clWrap*` wraps, the Beast-system shut-off and the mimic self-unlink through them.
- `formation`: `clFormation`'s 4th argument (`FormationCommand` +0x288), the force-field wall and sphere with the
  Sentinel links; in the final pass the leader's collision centre as the formation centre.
- `attack-misc`: the attack and passive-attack families, `clSpecial`, `clHalt*`, `clProtect`, the parade and
  holding-pattern code, `clPostProcess`, `GroupShip`, `clRUTransfer`, `LaunchAllInternalShipsOfPlayer`,
  `delegateCommand`, `processAttackToDoInFormation`, `processPassiveAttackToDo`.
- `tactics`: Cataclysm's `TacticsInfo` (0x9C4), `Tactics.c` reduced to the exe's 8 HW1 functions plus the 12
  retaliation functions, `univCheckShipState` / `getEnemiesWithinProximity` on them, `cat_univLimitProtectedShipSpeed`,
  `cat_beastGetAttackStaticInfo`.
- `final`: the IDENTICAL compares, `/catCommandTest` and harness mode `command`, builds, harness runs, delta-map reviews.

## File by file (with the exe addresses)

### `CommandLayer.h` / `CommandLayer.c`

- Layout: `MoveCommand` {pathType (`CAT_MOVEPATH_ONCE/LOOP/PATROL_OUT/PATROL_BACK`), numPoints, points[21], curPoint},
  `CommandToDo` 0x330 with ILP32 asserts (move.curPoint +0x11C, catSavedMoveValid +0x120 / catSavedMove +0x124, attack
  +0x22C, formation +0x230, formation.catForceFieldLevel +0x288, dock +0x290, protect +0x2B4, specialtargets +0x2BC,
  militaryParade +0x2C0, catPoint +0x2DC, catMimic +0x2E8, catResearch +0x2F8, tacticalupdatetime +0x308,
  holdingStartTime +0x30C, hyperSpaceingTime +0x310, hyperspaceState +0x314, holdingPatternVec +0x31C);
  `cat_shipFormationCommand()` replaces `Ship.formationcommand` (no exe access to Ship+0x36C).
- Move family: `processMoveToDo` 0x00443cb0 (4 arguments, per ship, path advance), `ChangeOrderToMove` 0x004460d0,
  `clMoveThese` 0x004467c0, `clMove` 0x00446d90, `cat_clCalculateMoveToOffsets`; hyperspace: `ChangeOrderToHyperSpace`
  0x004470f0, `clMpHyperspaceThese` 0x004472a0, `clMpHyperspace` 0x004474a0, `processMpHyperspaceingToDo` 0x004436a0.
- Cleanup: `FreeCommandToDoContents` 0x004440f0 and `RemoveShipsFromDoingStuff` 0x00444810 with the exe's unread reason
  codes, `FreeLastOrder` 0x00444490, `clProcess` 0x0044b950 (orders 11-16: `cat_processLinkToDo`,
  `cat_processUnlinkToDo`, `cat_bigGunProcessFireCommand` 0x005bbe80, `cat_mimicCommand` 0x005b8d70,
  `cat_processResearchToDo` 0x0044dc20).
- Attack family: `ChangeOrderToAttack` 0x00448460 (3rd argument, reason codes 1-4; `cat_clAddAttachedModulesToTargets`
  0x004482f0, whose first parameter is now the selection), `clAttackThese` 0x00448620 (Leech-on-ship filter, reason 6),
  `clAttack` 0x00448950 (selection copy, `cat_tacticsRemoveRetreatingShips`, the unnamed constraint 0x00448920 as
  `cat_clAttackerIsInWorldCB`, the saved looping/patrol move 0x00448aa3), `ChangeOrderToPassiveAttack` 0x00448b30
  (frees what it replaces), `clPassiveAttack` 0x00448ce0 (own order test `cat_clCanChangeOrderToPassiveAttack`,
  retaliation chatter 0x37/0x39/0x38 every 40 s; HW1's `canChangeOrderToPassiveAttack` removed),
  `processPassiveAttackToDo` 0x00441ab0 (no Evasive exception), `processAttackToDoInFormation` 0x00442fb0 (fleet chatter,
  blob-less early end, `command == NULL` leader-target test, force-field sphere, no-targets end), `delegateCommand`
  0x004427a0 (`ship->command`, force-field sphere, `tMinelayerCorvette`).
- `clSpecial` 0x004492c0 (Acolyte/ACV formation kept, no support-craft speech; `ShipsAreSalCapCorvettes` = the
  constraint 0x00449090, both races), `clHaltThese` 0x00449b50 (speech ids, parade ships out), `clHalt` 0x00449e80
  (speech, `CustShipHaltCommand` slot 27), `clRUTransfer` 0x00449990 (pay check, sound/speech ids),
  `LaunchAllInternalShipsOfPlayer` 0x0044a510 (own list), `FillInCarrierMothershipInfo` 0x0044a490 (CLASS_Mothership
  only, 3 carriers), `GroupShipIntoMilitaryParade` 0x0044b1c0 (no CLASS_Component), `clPostProcess` 0x0044cf80,
  `ChangeOrderToProtect` 0x0044ad50, `clAutoLaunch` 0x0044a640, `clClose` 0x00445a00 / `clReset` 0x00445b40 (the
  out-of-resources message timer 0x00a2db50 cleared), `clCancelAllLaunchOrdersFromPlayer` 0x00445a70,
  `FindNewThreateningTarget` 0x004417c0, `pickMultipleAttackTargets` 0x00447a50 (no range bonus), `clScuttle`
  0x0044a150 (`CAT_SHIPFLAG_Bit7` + infection-timer exemption), `formGroups` 0x00442680 and `clSetKamikaze`
  0x00449af0 (both races' types: HW1 aliases retired); the default-case assertions of `clDerelictDied`,
  `clResourceDied`, `clMissileDied` dropped (orders 11-17 reach them).
- The orders 11-16 and helpers (layout-move/orders subsystems): `cat_clLink` 0x0044d3b0, `cat_clUnlink` 0x0044d7a0,
  `cat_clMimic` 0x0044da80, `cat_clResearch` 0x0044dba0, `cat_clPointOrder` 0x0044dd70, `cat_clUpgradeShips` 0x0044de80,
  `cat_clSpecialTarget` 0x00449190, `cat_clUnleech` 0x00449770, `cat_clSetLeechAction` 0x00449960, `cat_clSetModuleSlot`
  0x0044e0e0, `cat_clSetBeastSystem` 0x0044e120, `cat_clFindLinkPartner` 0x0044d2e0, `cat_clLinkCleanup` 0x00444060.
- `/catCommandTest` (section "Self-test") at the end of the file; `CAT_UPGRADE_Link` (0x8) is defined there.

### `CommandWrap.h` / `CommandWrap.c`

`clWrapMove` (waypoint path) and `clWrapFormation` (level) from the earlier subsystems; new: `cat_clWrapLink` 0x00453940,
`cat_clWrapUnlink` 0x004539a0, `cat_clWrapSetLeechAction` 0x00453730, `cat_clWrapUnleech` 0x004536b0,
`cat_clWrapSetModuleSlot` 0x00453580, `cat_clWrapSetBeastSystem` 0x004535f0, `cat_clWrapPointOrder` 0x00453a70,
`cat_clWrapMimic` 0x00453b30, `cat_clWrapUpgradeShips` 0x00453ad0, `cat_clWrapSpecialTarget` 0x00453130. A network or
recorded game sends `clSendMisc` 5-10 (receive side: stage 5) or, for point/mimic/upgrade/special target, the exe's own
packets (not ported: stage 5). The 8 IDENTICAL wraps were compared and are identical.

### `Formation.h` / `Formation.c`

`FormationCommand.catForceFieldLevel` (+0x58 = CommandToDo+0x288), `clFormation` 0x00484c80 (4 arguments, per-ship
veto, caps 12/24, ChangeOrderToAttack reason 4), `FillInFormationSpecifics` 0x00484950, `FormationCalculateOffsets`
0x00483620 (`SetFormationOffsetsForForceFieldWall` 0x00481ef0, reason 5), `formationSpecificsSphere` 0x00481880,
`createSphereStaticInfo` 0x00481410, `cat_formationSentinelFieldLevel`, `cat_formationSphereSentinelLinks`,
`setFormationToDo` 0x00482a00 and `processFormationToDo` 0x00482b90 (the leader's **collision centre**, Ship+0xE0, as the
formation and sphere centre: 0x00482a28, 0x00482bf3, 0x00482eea, 0x00482f55; found by the self-test: with HW1's
position the force-field sphere never came up), `GetShipsTravelVel` 0x004833d0, `setMilitaryParade` 0x00485790,
`processMilitaryParadeToDo` 0x00485280, `paradeSetTweakables` 0x004850d0, `MajorityShipsAreDoing` 0x00484a30 (waypoint
path), `FreeMilitaryParadeContents` 0x00485b00 / `freeSphereStaticInfo` 0x00481590 (clear the freed pointers), the
proximity-sensor tests with both races' types.

### `Tactics.h` / `Tactics.c` (rewritten)

- `TacticsInfo` is the exe's (0x9C4 bytes at 0x00c523a4; ILP32 asserts, size printed by `tools/m2_layout32.sh`):
  EnemyNearZone, the passive retaliation fire times, RetreatGetAwayDistSqr[12 classes], freeRetreatTime,
  AttackMemoryTimeOut, 15 unread capital multipliers, InterceptorVerticalMultiplier, formationPadding[2],
  DodgeInfo[4][145] (0x00c52430), the dodge add times, the guard check, holdFormationDuringBattle[3],
  movingAttackTurnsIntoMoveCommandDistanceSqr. HW1's bonus tables, menu fields, RetreatDistance, burst modifiers and
  damage modifiers are gone.
- `TacticsInfotable` is the exe's table 0x008a80a0 row for row (generated from the exe; the `CLASS_MonsterShip` row
  points at the `CLASS_Corvette` slot as the exe's does); `tacticsStartUp` 0x005290b0 as the exe (zero
  EnemyNearZone and the dodge table, paddings 1.0, script, the guard square, `oneOverburstSpeed`, the attack memory).
- Kept HW1 functions: `tacticsShutDown`, `tacticsReportMove`, `tacticsShipDied`, `tacticsMakeShipsNotLookForOtherShips`,
  `tacticsMaxDistToTarget`, `tacticsCheckGuardConditionsDuringAttack`, `tacticsGetShipsMaxVelocity` (reviewed before).
  Removed: tacticsUpdate, tacticsGlobalUpdate, tacticsDelegateAttackCommand, tacticsDelegateSingleAttack,
  tacticsAttackCommandVoided, tacticsAreStrikeCraftInSelection and the retreat/dodge/flank/speed-burst helpers (no exe
  body); `tacticsPopUpSetUp` is a CAT-DORMANT no-op (its mainrgn.c callers are stage 3).
- New: `cat_tacticsFindResponsibleShip` 0x005294c0, `cat_tacticsMergeAttackTargets` 0x005295c0,
  `cat_tacticsRetaliatePassive` 0x005296f0, `cat_tacticsAveragePosition` 0x00529830, `cat_tacticsRetaliateEvasive`
  0x005298e0 (flee by the FOW radius, clamped to the universe box), `cat_tacticsSelectionHasFlag80Ship` 0x00529b70,
  `cat_tacticsShipAttacked` 0x00529c20, `cat_tacticsRetaliate` 0x00529cb0, the constraints 0x0052a1e0 / 0x0052a210,
  `cat_shipIsTargetableBy` 0x0052a080 (with the 3 retreat functions of M2-R4 the 12 cat-new of the plan).

### Other files

- `UnivUpdate.c/.h`: `univCheckShipState` 0x0054a650 ported (delayed deaths with modes 2/3/4, retaliation through
  `cat_tacticsShipAttacked`, no tacticsUpdate; the out-of-world fly-back is dead code in the exe and left out: it clips
  the zero vector instead of the position, see the comment; CAT-FIX: `getShipAndItsFormation`'s copy is freed, which
  the exe leaks); `getEnemiesWithinProximity` 0x0054a3f0 ported (the exe's static 6-ship selection; its ship callers
  already did not free it); `cat_univLimitProtectedShipSpeed` 0x00544e00 every 5 s and the per-ship reset (0x005448a7).
- `AIShip.c`: the escort limit read (0x004149c6), the linking Acolyte/Mimic avoidance skip (0x00414cb9) and the
  force-field sphere Sentinel exceptions (0x0041546c) (the last two found by the self-test).
- `ShipSelect.c/.h`: `cat_shipGetFOWRadius` 0x00514a30. `ResCollect.c/.h`: `cat_shipIsCollectingResources` 0x004f2500.
  `BeastWeapon.c/.h`: `cat_beastGetAttackStaticInfo` 0x00429b70 (`CatBeastAttackStatic`), used by `Collision.c`'s
  `cat_kamikazeCheckShip` (0x0043a9f9, 0x0043aa57). `SpaceObj.h`: `CAT_SHIPFLAG_Bit7`.
- `Mimic.c` (the self-unlink through `cat_clUnlink`), `Mothership.c` (the Beast-system shut-off through
  `cat_clWrapSetBeastSystem`), `Dock.c` (the link flight, layout-move/orders), the caller updates of the earlier
  subsystems (CommandNetwork, AIUtilities, Sensors, mainrgn, Battle, LevelLoad, Crates, SinglePlayer, Stats, AIShip,
  KASFunc, AIMoves, BeastWeapon, NIS, ResCollect, Salvage, Leech, RammingFrigate, RKO, ACV, MCV).
- Self-test plumbing: `OracleDump.c` (per-tick hook), `SDL/main.c` (`/catCommandTest`), `LevelLoad.c` (the test also
  loads the sMimic, sMCV and sSentinel statics; port diagnostic, not in the exe), `tools/m2_harness.sh` (mode
  `command`, `--command-ticks`), `tools/m2_layout32.sh` (TacticsInfo size).

## Self-test: `/catCommandTest <ticks>` (harness mode `command`)

The headless Mission01 run of `/oracleMission` with the scripted test of `CommandLayer.c` (`catCommandSelfTestTick`):
at tick 16 a fleet for the local player far from the level's ships (sMothership, sProcessor, 2 sAcolyte, 3 sMimic,
10 sSentinel; link and mimic upgrades set as scripted research), then `cat_clLink` (acolytes, mimic pair),
`cat_clMimic` (MIMIC_COMMAND_ShipType sRecon), `cat_clResearch` (start TECH_LINK, lab 0), `clFormation`
FORCEFIELD_WALL_FORMATION (4 Sentinels), `clProtect` + `clFormation` FORCEFIELD_SPHERE_FORMATION (6 around the
processor); the big gun is scripted charged and `cat_clPointOrder` follows once its charge reads full; after the link,
`cat_clUnlink` on the new sACV; `cat_clResearch` stop after the start state is checked. End states checked (the exe's,
from its code paths): sACV and sMCV created from the pairs, the sACV split into two sAcolytes and gone, the lab
researching TECH_LINK after the start and idle after the stop, the mimic's disguise set and the mimic disguised, the big
gun out of its idle state one tick after the order, every wall and sphere Sentinel in position at field level 1 with
the sphere's (and the wall leader's) field active and the sphere's clamped to the processor (a wall member clears its
own field flag on its own update, as `cat_sentinelFormationUpdate` 0x005c6830 does).

Final run `r5c-fin2-command` (3,000 ticks): **result=PASS**, 0 FAIL lines; link (sMCV) at tick 247, link (sACV) 258,
unlink 291, point ordered 18 and big gun state 1 at 19, research started 1 / stopped 1, mimic disguised; wall 4/4 in
position, level 1, 2 links; sphere 6/6 in position, level 1, 5 links, 6 clamped. ASan 0, no leak, rc 0.

What the self-test found and fixed in the port (each as in the exe): the formation centre is the leader's collision
centre (`setFormationToDo`, `processFormationToDo`); a linking Acolyte/Mimic avoids nothing once its partner is in the
blob (`aishipFlyToPointAvoidingObjsFunc` 0x00414cb9); a force-field sphere Sentinel avoids neither its formation nor the
protected ship (0x0041546c). Development runs `r5c-cmd1`..`r5c-cmd6` (cmd1 stopped on the unloaded sMimic static, fixed
by the LevelLoad hook; cmd2-cmd5 failed the link/mimic/sphere/point checks that led to the fixes above, to sRecon as the
disguise (an sMimic cannot pose as a resource-class Worker) and to the scripted gun charge; cmd6 PASS).

## Builds and harness runs

- `tools/build.sh --debug`: rc 0, 0 warnings (-Werror). `tools/build.sh --release`: rc 0, 0 warnings.
  `tools/m2_layout32.sh`: OK (TacticsInfo 2500 = 0x9C4, CommandToDo 816, FormationCommand 96).
- Binary of the final runs: `build/m2r5c/hwi-engine-fin2` (the final debug build).

| label | mode | milestone | first stop | notes |
|---|---|---|---|---|
| `r5c-s1-mission` | mission 6,000 | oracle mission run done | none (rc 0) | after layout-move; ASan 0 |
| `r5c-s2-mission` | mission 6,000 | oracle mission run done | none (rc 0) | after attack-misc; ASan 0 |
| `r5c-s3-mission` | mission 6,000 | oracle mission run done | none (rc 0) | after tactics; ASan 0 |
| `r5c-cmd1`..`6` | command 3,000 / 1,200 | level load .. command self-test done | cmd1: unloaded static (fixed) | development of the self-test |
| `r5c-fin2-command` | command 3,000 | command self-test done | none (rc 0) | **PASS**, ASan 0, no leak |
| `r5c-fin2-a` / `-b` | mission 6,000 | oracle mission run done | none (rc 0) | **byte-identical**, sha256 3359dba35cd4c93d, 1,476,370 B, ASan 0 |
| `r5c-fin2-perturb` | mission + perturb 1500 | oracle mission run done | none | divergence reported at tick 1500 (rng10), flipped copy at 2000: MISSION TEST PASS |
| `r5c-fin-sp`, `r5c-fin2-sp` | sp, timeout 340 | universe running | none: still running at the timeout (rc 124 by design) | both: ASan 0, no leak, milestone "universe running (univUpdateCounter > 0)"; `r5c-fin-sp` reached univUpdateCounter 1,248 at 340 s (as `r5ld-final-sp`); `r5c-fin2-sp` on the final binary, 3,719 file loads |

## Delta-map entries

`ghidra/delta_map_review.json`: 93 new review entries and 10 updated (sample `port-R5-command-tactics`, notes tagged
`[review: M2-R5 command-tactics]`), merged with `tools/delta_map.py merge --done m2r5:command-tactics`;
`docs/m2-r1-function-deltas.md` regenerated. Counts now IDENTICAL 3,046 / MODIFIED 1,297 / REWRITTEN 77, reviewed 1,798
(was 3,060 / 1,283 / 77, 1,705). [audit: M2-R5 command-tactics: three edited IDENTICAL functions outside the four
files had no review entry (`kasfSelectedShipsInFormation`, `kasfShipsInFormation`, `toDrawMoveCircle`, edited for the
`Ship.formationcommand` retirement); the audit compared them, added the entries (sample `audit-R5-command-tactics`),
merged and regenerated the report: reviewed 1,801. The call sites of `RemoveShipsFromDoingStuff`'s reason in
KASFunc.c / NIS.c / ResCollect.c / SinglePlayer.c and `clHoldingPattern`'s / Stats.c's `clFormation` level 0 got
their `CAT:` comments; see docs/audits/m2-r5-command-tactics.md.]

- The 55 IDENTICAL bodies of the four files the map still had unread were compared (the plan's 75 counted an older
  map; 20 of them were reviewed since): 42 are identical; 13 were modified, are ported and now MODIFIED
  (`PrepareShipsForCommand`, `PrepareOneShipForCommand`, `FindNewThreateningTarget`, `clClose`, `clReset`,
  `clCancelAllLaunchOrdersFromPlayer`, `RemoveShipFromAttacking`, `AttackCleanup`, `pickMultipleAttackTargets`,
  `clAutoLaunch`, `ChangeOrderToProtect`, `GroupShipIntoMilitaryParade`, `MajorityShipsAreDoing`); the reviewed
  `FillInCarrierMothershipInfo` was found modified too (ported, now MODIFIED); `freeSphereStaticInfo` /
  `FreeMilitaryParadeContents` stay IDENTICAL with the exe's pointer clearing ported.
- Every unreviewed MODIFIED body of the four files has an entry (29 in CommandLayer.c, 9 in Formation.c); reviewed ones
  edited here got notes appended (`clScuttle`, `clFormation`, `tacticsStartUp`, `formGroups`, `clSetKamikaze`,
  `univCheckShipState`, `getEnemiesWithinProximity`, `univUpdateAllPosVelShips`, `aishipFlyToPointAvoidingObjsFunc`).

## CAT-TODOs closed and re-targeted

Closed: `CommandLayer.c` and `Formation.c` header blocks (CAT-TODO(M2-R4 command follow-up)), `Tactics.c`'s
CAT-DORMANT/PROVISIONAL block, `UnivUpdate.c` `univCheckShipState` (M2-R4 #2/#4/#5), `getEnemiesWithinProximity`
(M2-R4 #4/#5), the protected-speed limit (2x M2-R4 #2), `Mimic.c` self-unlink (M2-R4 command follow-up),
`Mothership.c` Beast-system MP path (M2-R4 #4), `clScuttle`'s Ship+0x4E4 flag (M2-R4 #4). Re-targeted: `univCreateShip`
(two CAT-TODOs: #2 removed, #3..#5 left to their owners), `Mothership.c` health-warning message (CAT-PROVISIONAL to the
message scroller, M2-R5 hud). No CAT-TODO or CAT-PROVISIONAL in the four files names an M2-R4 owner now; the new ones
name M2-R5 sound, M2-R5 hud, M2-R6 FOW, stage 3 or stage 5.

## Leftovers and owners

- Network packets of the new orders and the receive side of `clSendMisc` 5-10, `clSendMove`'s path,
  `clSendFormation`'s level: **stage 5** (CAT-TODOs in CommandWrap.c).
- The speech and sound ids passed as the exe does (clHalt, clHaltThese, clRUTransfer, clAutoLaunch,
  cat_clHarvestNearestResource): **M2-R5 sound**; the command messages (clRUTransfer, the harvest message,
  clCommandMessage): **stage 3 / M2-R5 hud**.
- `tacticsPopUpSetUp` (no-op) and the mainrgn.c tactics keys (`cat_mrSetTactics` 0x00565c30, `cat_mrTacticsCycle`
  0x00566860), the UI callers of the wraps (`cat_uiLink`, `cat_uiUnlink`, `cat_uiSetLeechAction`, `cat_uiUnleech`,
  `cat_uiSpecialTargetCommand`, `cat_mrMimicTarget`, `cat_popupMenuMimicSelect`, `mrObjectClick`, ...): **stage 3**.
- AI callers (`cat_aisTeamAttack`, `cat_aisMothershipUpdate`, `cat_aisResearchUpdate`, `cat_aisWorkersUpdate`,
  `cat_aiuWrapCapture`, `aiuFindCaptureTarget`, `aiuWrapAttack`, `aiuWrapSpecial` on `cat_shipIsTargetableBy`): **stage 4**.
- `cat_shipUpdateExternalBuildAndAI` and `cat_mothershipModuleSlotSet` (callers of the mimic/upgrade/module-slot
  wraps): **M2-R6 ConsMgr**.
- `levelPreInit` 0x004a3230 (unreviewed MODIFIED; marks the Beast-convert types through `cat_beastGetAttackStaticInfo`,
  resets fog, lens flares and sound) and `kasfShipsBeastify`: **M2-R6 level load / M2-R5 render+sound**, **stage 6**.
- `aishipFlyToPointAvoidingObjsFunc` (reviewed in layout-dock) still differs: the small-target pass is skipped for a
  ship whose static +0x48 is below 500 (0x00414c8a), and the Ship+0x608 / single-player mission-type exceptions
  (0x00414e0e): **M2-R5 integrate / M2-R6 flight re-review** (found here, outside this producer's files).
- `COMMAND_CAT_13` / `COMMAND_CAT_17` have no setter in the exe; `SaveGame.c`'s retreat atoms: **saves**.
