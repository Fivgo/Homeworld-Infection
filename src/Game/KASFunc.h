#ifndef ___KASFUNC_H
#define ___KASFUNC_H

#include "AIUtilities.h"
#include "ShipSelect.h"
#include "Volume.h"

//
//  --------------------------------
//  these are for the placeholder for a real fleet intelligence message window
//
//
//  PETER:  we can nuke all this now, right?
//

// seconds
#define POPUPTEXT_DURATION_PER_LINE  4

// chars
#define POPUPTEXT_MAX_LINE_LENGTH  128
#define POPUPTEXT_MAX_LINES  16

// pixels
#define POPUPTEXT_BOX_WIDTH  200

void kasfPopupTextDraw(void);                       // 0x00494ca0 (CAT: no kasfPopupText / kasfPopupTextClear, M2-R8)

//
//  --------------------------------
//

#define kasSpecial_ReturningTechnology  1  //ships are salcapcorvettes returning technology
#define kasSpecial_ShipsDisabled        2  //ships have been disabled (salcapcorvette has locked on)
#define kasSpecial_ShipsAreArmed        3  //ships have arms on them - includes GravWellGenerator as an armed ship
#define kasSpecial_ShipsDefected        4  //ships have defected to the other side

/*-----------------------------------------------------------------------------
    CAT (M2-R8 kasf library): the KAS functions of cataclysm.exe's function table (cat_kasFunctionTable 0x00755048,
    KASFuncTable.c), one prototype per C name in the exe's address order (the address and KAS name on each; the
    folded stubs 0x0049ae30 / 0x006fe9a0 last). The parameter types are the table's: KAS names of timers, variables,
    messages and points are cat_kasHashString hashes (udword).
-----------------------------------------------------------------------------*/
void kasfMissionCompleted(void);                                                                     // 0x00491c70 MissionCompleted
void kasfMissionFailed(void);                                                                        // 0x00491cb0 MissionFailed
void kasfFadeToWhite(void);                                                                          // 0x00491cc0 FadeToWhite
void kasfVarCreate(udword name);                                                                     // 0x00491cf0 VarCreate
void kasfVarCreateSet(udword name, sdword value);                                                    // 0x00491d30 VarCreateSet
void kasfVarValueSet(udword name, sdword value);                                                     // 0x00491d70 VarSet
void kasfVarValueInc(udword name);                                                                   // 0x00491db0 VarInc
void kasfVarValueDec(udword name);                                                                   // 0x00491df0 VarDec
sdword kasfVarValueGet(udword name);                                                                 // 0x00491e30 VarGet
void kasfVarDestroy(udword name);                                                                    // 0x00491e70 VarDestroy
void kasfCommandMessage(char *string);                                                               // 0x00491eb0 Print
void kasfTimerCreate(udword name);                                                                   // 0x00491ec0 TimerCreate
void kasfTimerSet(udword name, sdword duration);                                                     // 0x00491f00 TimerSet
void kasfTimerStart(udword name);                                                                    // 0x00491f50 TimerStart
void kasfTimerCreateSetStart(udword name, sdword duration);                                          // 0x00491f90 TimerCreateSetStart
void kasfTimerStop(udword name);                                                                     // 0x00491fe0 TimerStop
sdword kasfTimerRemaining(udword name);                                                              // 0x00492020 TimerRemaining
sdword kasfTimerExpired(udword name);                                                                // 0x00492060 TimerExpired
sdword kasfTimerExpiredDestroy(udword name);                                                         // 0x004920a0 TimerExpiredDestroy
void kasfTimerDestroy(udword name);                                                                  // 0x004920e0 TimerDestroy
sdword kasfMsgReceived(udword msg);                                                                  // 0x00492120 MsgReceived
void kasfAttackMothership(void);                                                                     // 0x00492140 AttackMothership
void kasfAttack(GrowSelection *targets);                                                             // 0x00492190 Attack
void kasfShipsAttack(GrowSelection *targets, GrowSelection *attackers);                              // 0x00492260 ShipsAttack
void kasfAttackSpecial(GrowSelection *targets);                                                      // 0x00492280 AttackSpecial
void kasfMoveAttack(GrowSelection *targets);                                                         // 0x004922d0 MoveAttack
sdword kasfBulgeAttack(GrowSelection *targets, GrowSelection *bulgeTargets, GrowSelection *attackers, sdword radius); // 0x00492320 BulgeAttack
void kasfIntercept(GrowSelection *targets);                                                          // 0x004924e0 Intercept
void kasfAttackFlank(GrowSelection *targets);                                                        // 0x00492530 AttackFlank
void kasfAttackHarass(void);                                                                         // 0x00492580 AttackHarass
void kasfSetSwarmerTargets(GrowSelection *targets);                                                  // 0x004925a0 SetSwarmTargets
void kasfSwarmMoveTo(GrowSelection *targets);                                                        // 0x00492610 SwarmMoveTo
void kasfShipsSetFormation(GrowSelection *Ships, sdword Formation);                                  // 0x00492740 ShipsSetFormation
void kasfFormationDelta(void);                                                                       // 0x00492780 FormationDelta
void kasfFormationBroad(void);                                                                       // 0x004927b0 FormationBroad
void kasfFormationDelta3D(void);                                                                     // 0x004927e0 FormationDelta3D
void kasfFormationClaw(void);                                                                        // 0x00492810 FormationClaw
void kasfFormationWall(void);                                                                        // 0x00492840 FormationWall
void kasfFormationSphere(void);                                                                      // 0x00492870 FormationSphere
void kasfFormationParade(GrowSelection *pShips);                                                     // 0x004928a0 FormationParade
void kasfFormationCustom(GrowSelection *ships);                                                      // 0x004928f0 FormationCustom
sdword kasfRandom(sdword lowestNum, sdword highestNum);                                              // 0x00492910 Random
void kasfGuardMothership(void);                                                                      // 0x00492940 GuardMothership
void kasfGuardShips(GrowSelection *ships);                                                           // 0x00492960 Guard
void kasfShipsGuardShips(GrowSelection *Guard, GrowSelection *Ships);                                // 0x004929a0 ShipsGuardShips
void kasfPatrolPath(Path *path);                                                                     // 0x004929d0 PatrolPath
void kasfPatrolActive(void);                                                                         // 0x00492a10 Patrol
void kasfLogInteger(char *string, sdword integer);                                                   // 0x00492a40 LogInteger
sdword kasfShipsOrder(GrowSelection *ships);                                                         // 0x00492ab0 ShipsOrder
sdword kasfShipsOrderAttributes(GrowSelection *ships);                                               // 0x00492ae0 ShipsOrderAttributes
sdword kasfTeamHealthAverage(void);                                                                  // 0x00492b20 TeamHealthAverage
sdword kasfTeamHealthLowest(void);                                                                   // 0x00492b90 TeamHealthLowest
sdword kasfTeamFuelAverage(void);                                                                    // 0x00492c00 TeamFuelAverage
sdword kasfTeamFuelLowest(void);                                                                     // 0x00492c70 TeamFuelLowest
sdword kasfTeamCount(void);                                                                          // 0x00492ce0 TeamCount
sdword kasfNewShipsAdded(void);                                                                      // 0x00492cf0 NewShipsAdded
void kasfGrowSelectionClear(GrowSelection *ships);                                                   // 0x00492d10 ShipsClear
sdword kasfShipsCount(GrowSelection *ships);                                                         // 0x00492d20 ShipsCount
sdword kasfPointInside(Volume *volume, hvector *point);                                              // 0x00492d30 PointInside
void kasfFindMeteors(GrowSelection *meteors);                                                        // 0x00492d60 FindMeteors
sdword kasfFindShipsInside(Volume *volume, GrowSelection *ships);                                    // 0x00492db0 FindShipsInside
sdword kasfFindEnemiesNearby(GrowSelection *ships, sdword radius);                                   // 0x00492e90 FindEnemiesNearby
sdword kasfFindEnemiesNearTeam(GrowSelection *ships, sdword radius);                                 // 0x00492f80 FindEnemiesNearTeam
sdword kasfFindMinesNearTeam(GrowSelection *ships, sdword radius);                                   // 0x00493060 FindMinesNearTeam
sdword kasfFindMinesInside(Volume *volume, GrowSelection *ships);                                    // 0x00493130 FindMinesInside
sdword kasfFindShipsNearPoint(GrowSelection *ships, hvector *location, sdword radius);               // 0x004931a0 FindShipsNearPoint
sdword kasfFindEnemiesInside(Volume *volume, GrowSelection *ships, sdword neighborRadius);           // 0x00493280 FindEnemiesInside
void kasfShipsAttributesBitSet(GrowSelection *ships, sdword attributes);                             // 0x004932a0 ShipsAttributesBitSet
void kasfShipsAttributesBitClear(GrowSelection *ships, sdword attributes);                           // 0x004932d0 ShipsAttributesBitClear
void kasfShipsAttributesSet(GrowSelection *ships, sdword attributes);                                // 0x004933d0 ShipsAttributesSet
sdword kasfShipUnderConstruction(GrowSelection *ships);                                              // 0x00493400 ShipUnderConstruction
void kasfShipsParalyze(GrowSelection *ships, sdword paralyze);                                       // 0x00493420 ShipsParalyze
sdword kasfTimesHitByEMP(GrowSelection *ships);                                                      // 0x00493460 TimesHitByEMP
void kasfTeamAttributesBitSet(sdword attributes);                                                    // 0x00493490 TeamAttributesBitSet
void kasfTeamAttributesBitClear(sdword attributes);                                                  // 0x004934d0 TeamAttributesBitClear
void kasfTeamAttributesSet(sdword attributes);                                                       // 0x00493510 TeamAttributesSet
sdword kasfTeamAttributesBitTest(sdword attributes);                                                 // 0x00493550 TeamAttributesBitTest
void kasfTeamMakeCrazy(sdword makecrazy);                                                            // 0x00493580 TeamMakeCrazy
void kasfDisablePlayerHyperspace(void);                                                              // 0x004935e0 DisablePlayerHyperspace
void kasfHoldHyperspaceWindow(bool32 Hold);                                                          // 0x004935f0 HoldHyperspaceWindow
void kasfTeamHyperspaceIn(hvector *destination);                                                     // 0x00493600 TeamHyperspaceIn
void kasfTeamHyperspaceInNear(hvector *destination, sdword distance);                                // 0x00493630 TeamHyperspaceInNear
void kasfHyperspaceDelay(sdword milliseconds);                                                       // 0x004937e0 HyperspaceDelay
void kasfTeamHyperspaceOut(void);                                                                    // 0x00493800 TeamHyperspaceOut
void kasfTeamSetMaxVelocity(sdword maxVelocity);                                                     // 0x00493860 TeamVelocityMaxSet
void kasfTeamClearMaxVelocity(void);                                                                 // 0x004938b0 TeamVelocityMaxClear
void kasfShipsSetMaxVelocity(GrowSelection *ships, sdword maxVelocity);                              // 0x004938f0 ShipsVelocityMaxSet
void kasfShipsClearMaxVelocity(GrowSelection *ships);                                                // 0x00493940 ShipsVelocityMaxClear
void kasfShipsSetDamageFactor(GrowSelection *ships, sdword damageModifier);                          // 0x00493980 ShipsDamageModifierSet
void kasfShipsClearDamageFactor(GrowSelection *ships);                                               // 0x004939d0 ShipsDamageModifierClear
void kasfShipsSetRepairFactor(GrowSelection *ships, sdword repairModifier);                          // 0x00493a10 ShipsRepairModifierSet
void kasfShipsClearRepairFactor(GrowSelection *ships);                                               // 0x00493a60 ShipsRepairModifierClear
void kasfTeamSetPercentDamaged(sdword percentHealth);                                                // 0x00493aa0 TeamHealthSet
void kasfShipsSetPercentDamaged(GrowSelection *ships, sdword percentHealth);                         // 0x00493af0 ShipsHealthSet
void kasfTeamSetPercentFueled(sdword percentFueled);                                                 // 0x00493b30 TeamFuelSet
void kasfMissionSkillSet(sdword skillLevel);                                                         // 0x00493b80 MissionSkillSet
sdword kasfMissionSkillGet(void);                                                                    // 0x00493b90 MissionSkillGet
void kasfRequestShips(char *shipType, sdword numShips);                                              // 0x00493ba0 RequestShips
void kasfRequestShipsOriginal(sdword percentOriginal);                                               // 0x00493c00 RequestShipsOriginal
void kasfReinforceTeamWithNewShips(struct AITeam *teamtoreinforce, GrowSelection *shipstoadd);       // 0x00493c90 ReinforceTeamWithNewShips
void kasfReinforceTeamWithShips(struct AITeam *teamtoreinforce, GrowSelection *shipstoadd);          // 0x00493cf0 ReinforceTeamWithShips
void kasfReinforce(struct AITeam *team);                                                             // 0x00493d40 Reinforce
void kasfForceCombatStatus(GrowSelection *ships, sdword on);                                         // 0x00493d70 ForceCombatStatus
sdword kasfThisTeamIs(struct AITeam *team);                                                          // 0x00493de0 ThisTeamIs
sdword kasfTeamCountOriginal(void);                                                                  // 0x00493e00 TeamCountOriginal
void kasfTeamGiveToAI(void);                                                                         // 0x00493e10 TeamGiveToAI
void kasfDisableAIFeature(sdword feature, sdword type);                                              // 0x00493e70 DisableAIFeature
void kasfEnableAIFeature(sdword feature, sdword type);                                               // 0x00493ee0 EnableAIFeature
void kasfDisableAllAIFeatures(void);                                                                 // 0x00493f40 DisableAllAIFeatures
void kasfEnableAllAIFeatures(void);                                                                  // 0x00493f60 EnableAllAIFeatures
void kasfTeamSwitchPlayerOwner(sdword bMakeControllable);                                            // 0x00494160 TeamSwitchPlayerOwner
void kasfShipsSwitchPlayerOwner(GrowSelection *ships, sdword bMakeControllable);                     // 0x00494180 ShipsSwitchPlayerOwner
void kasfMoveTo(hvector *destination);                                                               // 0x004941a0 MoveTo
void kasfShipsMoveTo(GrowSelection *ships, hvector *destination);                                    // 0x00494200 ShipsMoveTo
void kasfSetTactics(sdword Tactics);                                                                 // 0x00494250 SetTactics
void kasfTacticsAggressive(void);                                                                    // 0x00494280 TacticsAggressive
void kasfTacticsNeutral(void);                                                                       // 0x004942b0 TacticsNeutral
void kasfTacticsEvasive(void);                                                                       // 0x004942e0 TacticsEvasive
sdword kasfNearby(hvector *location, sdword distance);                                               // 0x00494310 Nearby
sdword kasfFindDistance(hvector *location1, hvector *location2);                                     // 0x004943d0 FindDistance
sdword kasfUnderAttackElsewhere(struct AITeam *otherTeam, GrowSelection *attackers);                 // 0x00494450 UnderAttackElsewhere
sdword kasfUnderAttack(GrowSelection *attackers);                                                    // 0x00494510 UnderAttack
sdword kasfShipsCountType(GrowSelection *ships, char *shipType);                                     // 0x00494530 ShipsCountType
void kasfDock(struct AITeam *withTeam);                                                              // 0x00494570 Dock
void kasfDockSupport(void);                                                                          // 0x00494570 DockSupport
void kasfDockSupportWith(struct AITeam *withTeam);                                                   // 0x004945a0 DockSupportWith
void kasfShipsDockSupportWith(GrowSelection *ships, GrowSelection *withShips);                       // 0x00494610 ShipsDockSupportWith
void kasfDockStay(struct AITeam *withTeam);                                                          // 0x00494630 DockStay
void kasfShipsDockStay(GrowSelection *ships, GrowSelection *withShips);                              // 0x004946a0 ShipsDockStay
void kasfDockStayMothership(void);                                                                   // 0x004946f0 DockStayMothership
void kasfDockInstant(struct AITeam *withTeam);                                                       // 0x00494750 DockInstant
void kasfLaunch(void);                                                                               // 0x004947c0 Launch
void kasfShipsLaunch(GrowSelection *ships, GrowSelection *host);                                     // 0x00494830 ShipsLaunch
sdword kasfTeamDocking(void);                                                                        // 0x00494890 TeamDocking
sdword kasfTeamFinishedLaunching(void);                                                              // 0x004948a0 TeamFinishedLaunching
sdword kasfShipsFinishedLaunching(GrowSelection *Ships);                                             // 0x004948f0 ShipsFinishedLaunching
sdword kasfTeamDockedReadyForLaunch(void);                                                           // 0x00494940 TeamDockedReadyForLaunch
void kasfMsgSendAll(udword msg);                                                                     // 0x004949b0 MsgSendAll
void kasfMsgSend(struct AITeam *team, udword msg);                                                   // 0x00494a00 MsgSend
void kasfMsgQueueFlush(struct AITeam *team);                                                         // 0x00494a20 MsgQueueFlush
sdword kasfRUsEnemyCollected(void);                                                                  // 0x00494a30 RUsEnemyCollected
sdword kasfFindEnemyShipsOfType(GrowSelection *ships, char *shipType);                               // 0x00494a40 FindEnemyShipsOfType
sdword kasfFindFriendlyShipsOfType(GrowSelection *ships, char *shipType);                            // 0x00494ac0 FindFriendlyShipsOfType
sdword kasfFindEnemyShipsOfClass(GrowSelection *ships, char *shipClass);                             // 0x00494b40 FindEnemyShipsOfClass
sdword kasfFindFriendlyShipsOfClass(GrowSelection *ships, char *shipClass);                          // 0x00494bc0 FindFriendlyShipsOfClass
sdword kasfPathNextPoint(void);                                                                      // 0x00494c40 PathNextPoint
void kasfPopup(char *string);                                                                        // 0x00494e00 Popup
void kasfPopupInteger(char *string, sdword integer);                                                 // 0x00494e10 PopupInteger
void kasfObjectiveCreate(char *label, char *briefText, char *fullText);                              // 0x00494e70 ObjectiveCreate
void kasfObjectiveCreateSecondary(char *label, char *briefText, char *fullText);                     // 0x00494ed0 ObjectiveCreateSecondary
void kasfObjectiveSet(char *label, sdword status);                                                   // 0x00494f10 ObjectiveSet
sdword kasfObjectiveGet(char *label);                                                                // 0x00494f30 ObjectiveGet
sdword kasfObjectiveGetAll(void);                                                                    // 0x00494f40 ObjectivesGetAll
sdword kasfObjectiveGetAny(void);                                                                    // 0x00494f50 ObjectivesGetAny
void kasfObjectiveDestroy(char *label);                                                              // 0x00494f60 ObjectiveDestroy
void kasfObjectiveDestroyAll(void);                                                                  // 0x00494f70 ObjectivesDestroyAll
sdword kasfNISRunning(void);                                                                         // 0x00494f80 NISRunning
sdword kasfShipsSelectGuarding(GrowSelection *NewShips, GrowSelection *OldShips, GrowSelection *Guarding); // 0x004950d0 ShipsSelectGuarding
sdword kasfShipsSelectIdle(GrowSelection *NewShips, GrowSelection *OldShips);                        // 0x00495140 ShipsSelectIdle
sdword kasfShipsSelectNotIdle(GrowSelection *NewShips, GrowSelection *OldShips);                     // 0x00495160 ShipsSelectNotIdle
sdword kasfShipsSelectClamped(GrowSelection *NewShips, GrowSelection *OldShips, GrowSelection *Target); // 0x00495180 ShipsSelectClamped
sdword kasfShipsGetClamped(GrowSelection *pShips, GrowSelection *pTarget);                           // 0x00495200 ShipsGetClamped
sdword kasfShipsSelectNumber(GrowSelection *NewShips, GrowSelection *OldShips, sdword Number);       // 0x00495260 ShipsSelectNumber
sdword kasfShipsSelectPlayer(GrowSelection *NewShips, GrowSelection *OldShips);                      // 0x004952b0 ShipsSelectPlayer
void kasfShipsSelectSalvaging(GrowSelection *NewShips, GrowSelection *OldShips);                     // 0x004952f0 ShipsSelectSalvaging
sdword kasfShipsSelectSalvagable(GrowSelection *NewShips, GrowSelection *OldShips, GrowSelection *Targets); // 0x00495390 ShipsSelectSalvagable
sdword kasfShipsSelectShips(GrowSelection *newships, GrowSelection *oldships, GrowSelection *compareships); // 0x00495410 ShipsSelectShips
sdword kasfShipsSelectInside(GrowSelection *newships, GrowSelection *oldships, Volume *volume);      // 0x00495460 ShipsSelectInside
sdword kasfShipsSelectUpgradable(GrowSelection *newShips, GrowSelection *originalShips);             // 0x004954a0 ShipsSelectUpgradable
sdword kasfShipsSelectEnemyNoStealthy(GrowSelection *NewShips, GrowSelection *OriginalShips);        // 0x004954d0 ShipsSelectEnemyNoStealthy
sdword kasfShipsSelectEnemy(GrowSelection *newShips, GrowSelection *originalShips);                  // 0x00495560 ShipsSelectEnemy
sdword kasfShipsSelectFriendly(GrowSelection *newShips, GrowSelection *originalShips);               // 0x00495620 ShipsSelectFriendly
sdword kasfShipsSelectClass(GrowSelection *newShips, GrowSelection *originalShips, char *shipClass); // 0x00495640 ShipsSelectClass
sdword kasfShipsSelectType(GrowSelection *newShips, GrowSelection *originalShips, char *shipType);   // 0x00495690 ShipsSelectType
sdword kasfShipsSelectCapital(GrowSelection *newShips, GrowSelection *originalShips);                // 0x004956e0 ShipsSelectCapital
sdword kasfShipsSelectNonCapital(GrowSelection *newShips, GrowSelection *originalShips);             // 0x00495730 ShipsSelectNonCapital
sdword kasfShipsSelectDocked(GrowSelection *NewShips, GrowSelection *OldShips);                      // 0x00495750 ShipsSelectDocked
sdword kasfShipsSelectNotDocked(GrowSelection *newShips, GrowSelection *originalShips);              // 0x00495790 ShipsSelectNotDocked
sdword kasfShipsSelectDamaged(GrowSelection *newShips, GrowSelection *originalShips, sdword maxHealthPercent); // 0x004957d0 ShipsSelectDamaged
sdword kasfShipsSelectMoreThanDamaged(GrowSelection *newShips, GrowSelection *originalShips, sdword maxHealthPercent); // 0x00495840 ShipsSelectMoreThanDamaged
sdword kasfShipsSelectMoving(GrowSelection *newShips, GrowSelection *originalShips);                 // 0x004958b0 ShipsSelectMoving
sdword kasfShipsSelectIndex(GrowSelection *newShips, GrowSelection *originalShips, sdword Index);    // 0x00495950 ShipsSelectIndex
void kasfShipsSelectFarther(GrowSelection *newships, GrowSelection *oldships, hvector *point, sdword distance); // 0x004959a0 ShipsSelectFarther
sdword kasfShipsSelectNearby(GrowSelection *newShips, GrowSelection *originalShips, hvector *location, sdword Index); // 0x00495a50 ShipsSelectNearby
sdword kasfShipsSelectSpecial(GrowSelection *newShips, GrowSelection *originalShips, sdword specialFlag); // 0x00495b00 ShipsSelectSpecial
sdword kasfShipsAdd(GrowSelection *shipsA, GrowSelection *shipsB);                                   // 0x00495bd0 ShipsAdd
sdword kasfShipsRemove(GrowSelection *shipsA, GrowSelection *shipsB);                                // 0x00495c40 ShipsRemove
sdword kasfSelectedShipsInFormation(sdword formation);                                               // 0x00495cc0 SelectedShipsInFormation
sdword kasfShipsInFormation(GrowSelection *ships, sdword formation);                                 // 0x00495d10 ShipsInFormation
void kasfShipsSetNonRetaliation(GrowSelection *ships);                                               // 0x00495da0 ShipsSetNonRetaliation
void kasfShipsSetRetaliation(GrowSelection *ships);                                                  // 0x00495de0 ShipsSetRetaliation
void kasfPingAddSingleShip(GrowSelection *ships, char *label);                                       // 0x00495e20 PingAddSingleShip
void kasfPingAddShips(GrowSelection *ships, char *label);                                            // 0x00495e50 PingAddShips
void kasfPingAddPoint(hvector *vector, char *label);                                                 // 0x00495e80 PingAddPoint
void kasfPingRemove(char *label);                                                                    // 0x00495ea0 PingRemove
void kasfStop(void);                                                                                 // 0x00495eb0 Stop
void kasfShipsStop(GrowSelection *ship);                                                             // 0x00495ee0 ShipsStop
void kasfHarvest(void);                                                                              // 0x00495f00 Harvest
void kasfBuildControl(sdword on);                                                                    // 0x00495f50 BuildControl
void kasfBuildingTeam(struct AITeam *Builder);                                                       // 0x00495f60 BuildingTeam
void kasfKamikaze(GrowSelection *targets);                                                           // 0x00495f80 Kamikaze
void kasfKamikazeEveryone(GrowSelection *targets);                                                   // 0x00495fd0 KamikazeEveryone
void kasfSpecialToggle(void);                                                                        // 0x00496050 SpecialToggle
void kasfShipsSpecialToggle(GrowSelection *ships);                                                   // 0x00496080 ShipsSpecialToggle
void kasfShipsDamage(GrowSelection *ships, sdword points);                                           // 0x004960b0 ShipsDamage
void kasfBuilderRestrictShipTypes(char *shipTypes);                                                  // 0x00496110 BuilderRestrictShipTypes
void kasfBuilderUnrestrictShipTypes(char *shipTypes);                                                // 0x00496120 BuilderUnrestrictShipTypes
void kasfBuilderRestrictAll(void);                                                                   // 0x00496130 BuilderRestrictAll
void kasfBuilderRestrictNone(void);                                                                  // 0x00496140 BuilderRestrictNone
void kasfBuilderCloseIfOpen(void);                                                                   // 0x00496150 BuilderCloseIfOpen
void kasfResearchCloseIfOpen(void);                                                                  // 0x00496160 ResearchCloseIfOpen
void kasfForceBuildShipType(char *shipType);                                                         // 0x00496170 ForceBuildShipType
sdword kasfCameraGetAngleDeg(void);                                                                  // 0x00496190 CameraGetAngleDeg
void kasfCameraSetAngleDeg(sdword angle);                                                            // 0x004961b0 CameraSetAngleDeg
sdword kasfCameraGetDeclinationDeg(void);                                                            // 0x004961f0 CameraGetDeclinationDeg
void kasfCameraSetDeclinationDeg(sdword declination);                                                // 0x00496210 CameraSetDeclinationDeg
sdword kasfCameraGetDistance(void);                                                                  // 0x00496250 CameraGetDistance
void kasfCameraSetDistance(sdword distance);                                                         // 0x00496260 CameraSetDistance
sdword kasfSelectNumSelected(void);                                                                  // 0x00496280 SelectNumSelected
sdword kasfSelectIsSelectionShipType(sdword index, char *shipType);                                  // 0x00496290 SelectIsSelectionShipType
sdword kasfSelectContainsShipTypes(char *shipTypes);                                                 // 0x004962d0 SelectContainsShipTypes
void kasfTutSetPointerTargetXY(char *name, sdword pointerX, sdword pointerY);                        // 0x004962e0 TutSetPointerTargetXY
void kasfTutSetPointerTargetXYRight(char *name, sdword pointerX, sdword pointerY);                   // 0x00496300 TutSetPointerTargetXYRight
void kasfTutSetPointerTargetXYBottomRight(char *name, sdword pointerX, sdword pointerY);             // 0x00496320 TutSetPointerTargetXYBottomRight
void kasfTutSetPointerTargetXYTaskbar(char *name, sdword pointerX, sdword pointerY);                 // 0x00496340 TutSetPointerTargetXYTaskbar
void kasfTutSetPointerTargetXYFE(char *name, sdword pointerX, sdword pointerY);                      // 0x00496360 TutSetPointerTargetXYFE
void kasfTutSetPointerTargetShip(char *name, GrowSelection *pShipList);                              // 0x00496380 TutSetPointerTargetShip
void kasfTutSetPointerTargetShipSelection(char *name, GrowSelection *pShipList);                     // 0x004963a0 TutSetPointerTargetShipSelection
void kasfTutSetObjPointerTargetShip(char *name, GrowSelection *pShipList);                           // 0x004963c0 TutSetObjPointerTargetShip
void kasfTutSetObjPointerTargetShipSelection(char *name, GrowSelection *pShipList);                  // 0x004963e0 TutSetObjPointerTargetShipSelection
void kasfTutSetPointerTargetShipHealth(char *name, GrowSelection *pShipList);                        // 0x00496400 TutSetPointerTargetShipHealth
void kasfTutSetPointerTargetShipGroup(char *name, GrowSelection *pShipList);                         // 0x00496420 TutSetPointerTargetShipGroup
void kasfTutSetPointerTargetFERegion(char *name, char *pAtomName);                                   // 0x00496440 TutSetPointerTargetFERegion
void kasfTutSetPointerTargetRect(char *name, sdword x0, sdword y0, sdword x1, sdword y1);            // 0x00496460 TutSetPointerTargetRect
void kasfTutSetPointerTargetAIVolume(char *name, Volume *volume);                                    // 0x00496490 TutSetPointerTargetAIVolume
void kasfTutSetObjPointerTargetAIVolume(char *name, Volume *volume);                                 // 0x004964b0 TutSetObjPointerTargetAIVolume
void kasfTutSetPointerTargetObj(char *name);                                                         // 0x004964d0 TutSetPointerTargetObj
void kasfTutRemovePointer(char *name);                                                               // 0x004964e0 TutRemovePointer
void kasfTutRemoveAllPointers(void);                                                                 // 0x00496530 TutRemoveAllPointers
void kasfTutSetTextDisplayBoxGame(sdword posX, sdword posY, sdword width, sdword height);            // 0x00496570 TutSetTextDisplayBoxGame
void kasfTutSetTextDisplayBoxToSubtitleRegion(void);                                                 // 0x00496590 TutSetTextDisplayBoxToSubtitleRegion
void kasfTutSetTextDisplayBoxFE(sdword posX, sdword posY, sdword width, sdword height);              // 0x004965a0 TutSetTextDisplayBoxFE
void kasfTutShowText(char *text);                                                                    // 0x004965c0 TutShowText
void kasfTutHideText(void);                                                                          // 0x004965d0 TutHideText
void kasfTutShowNextButton(void);                                                                    // 0x004965e0 TutShowNextButton
void kasfTutHideNextButton(void);                                                                    // 0x004965f0 TutHideNextButton
sdword kasfTutNextButtonClicked(void);                                                               // 0x00496600 TutNextButtonClicked
void kasfTutShowNextTextButton(void);                                                                // 0x00496610 TutShowNextTextButton
void kasfTutHideNextTextButton(void);                                                                // 0x00496620 TutHideNextTextButton
sdword kasfTutNextTextButtonClicked(void);                                                           // 0x00496630 TutNextTextButtonClicked
void kasfTutShowPrevButton(void);                                                                    // 0x00496640 TutShowPrevButton
void kasfTutHidePrevButton(void);                                                                    // 0x00496650 TutHidePrevButton
sdword kasfTutPrevButtonClicked(void);                                                               // 0x00496660 TutPrevButtonClicked
void kasfTutShowQuitButton(void);                                                                    // 0x00496670 TutShowQuitButton
void kasfTutHideQuitButton(void);                                                                    // 0x00496680 TutHideQuitButton
sdword kasfTutQuitButtonClicked(void);                                                               // 0x00496690 TutQuitButtonClicked
void kasfTutShowBackButton(void);                                                                    // 0x004966a0 TutShowBackButton
void kasfTutHideBackButton(void);                                                                    // 0x004966b0 TutHideBackButton
void kasfTutSaveLesson(sdword LessonNumber, char *LessonName);                                       // 0x004966c0 TutSaveLesson
void kasfTutShowImages(char *imageList);                                                             // 0x004966e0 TutShowImages
void kasfTutHideImages(void);                                                                        // 0x004966f0 TutHideImages
void kasfTutEnableEverything(void);                                                                  // 0x00496700 TutEnableEverything
void kasfTutDisableEverything(void);                                                                 // 0x00496710 TutDisableEverything
void kasfTutEnableFlags(char *flagsList);                                                            // 0x00496720 TutEnableFlags
void kasfTutDisableFlags(char *flagsList);                                                           // 0x00496730 TutDisableFlags
void kasfTutForceUnpaused(void);                                                                     // 0x00496740 TutForceUnpaused
sdword kasfTutGameSentMessage(char *commandNames);                                                   // 0x00496750 TutGameSentMessage
void kasfTutResetGameMessageQueue(void);                                                             // 0x00496760 TutResetGameMessageQueue
sdword kasfTutContextMenuDisplayedForShipType(char *shipType);                                       // 0x00496770 TutContextMenuDisplayedForShipType
void kasfTutResetContextMenuShipTypeTest(void);                                                      // 0x00496780 TutResetContextMenuShipTypeTest
void kasfTutRedrawEverything(void);                                                                  // 0x00496790 TutRedrawEverything
sdword kasfBuildManagerShipTypeInBatchQueue(char *shipType);                                         // 0x004967a0 BuildManagerShipTypeInBatchQueue
sdword kasfBuildManagerShipTypeInBuildQueue(char *shipType);                                         // 0x004967b0 BuildManagerShipTypeInBuildQueue
sdword kasfBuildManagerShipTypeSelected(char *shipType);                                             // 0x004967c0 BuildManagerShipTypeSelected
sdword kasfTutCameraFocusedOnShipType(char *shipType);                                               // 0x004967d0 TutCameraFocusedOnShipType
void kasfTutCameraFocus(GrowSelection *pShipList);                                                   // 0x004967e0 TutCameraFocus
void kasfCameraFocus(GrowSelection *ships);                                                          // 0x00496870 CameraFocus
void kasfCameraZoom(sdword zoom);                                                                    // 0x00496910 CameraZoom
void kasfCameraDistance(sdword distance);                                                            // 0x00496930 CameraDistance
void kasfCameraFocusClose(GrowSelection *ships);                                                     // 0x00496950 CameraFocusClose
void kasfTutCameraFocusDerelictType(char *derelictType);                                             // 0x004969e0 TutCameraFocusDerelictType
void kasfTutCameraFocusFar(GrowSelection *pShipList);                                                // 0x00496a60 TutCameraFocusFar
void kasfTutCameraFocusCancel(void);                                                                 // 0x00496ae0 TutCameraFocusCancel
void kasfDisablePlayer(bool32 toggle);                                                               // 0x00496b00 DisablePlayer
sdword kasfTutShipsInView(GrowSelection *ships);                                                     // 0x00496b20 TutShipsInView
sdword kasfTutShipsTactics(GrowSelection *ships);                                                    // 0x00496b70 TutShipsTactics
sdword kasfTutPieDistance(void);                                                                     // 0x00496b90 TutPieDistance
sdword kasfTutPieHeight(void);                                                                       // 0x00496ba0 TutPieHeight
void kasfForceFISensors(void);                                                                       // 0x00496bb0 ForceFISensors
void kasfOpenSensors(sdword flags);                                                                  // 0x00496bc0 OpenSensors
void kasfCloseSensors(sdword flags);                                                                 // 0x00496c30 CloseSensors
sdword kasfSensorsIsOpen(sdword flags);                                                              // 0x00496c80 SensorsIsOpen
sdword kasfResearchIsOpen(sdword flags);                                                             // 0x00496cc0 ResearchIsOpen
sdword kasfBuildIsOpen(sdword flags);                                                                // 0x00496cd0 BuildIsOpen
void kasfSensorsWeirdness(sdword flags);                                                             // 0x00496ce0 SensorsWeirdness
void kasfAllowPlayerToResearch(char *techName);                                                      // 0x00496cf0 TechSetResearch
void kasfAllowPlayerToPurchase(char *techName);                                                      // 0x00496d00 TechSetPurchase
void kasfPlayerAcquiredTechnology(char *techName);                                                   // 0x00496d10 TechSet
sdword kasfCanPlayerResearch(char *techName);                                                        // 0x00496d20 TechGetResearch
sdword kasfCanPlayerPurchase(char *techName);                                                        // 0x00496d30 TechGetPurchase
sdword kasfDoesPlayerHave(char *techName);                                                           // 0x00496d40 TechGet
void kasfSetBaseTechnologyCost(char *techName, sdword cost);                                         // 0x00496d50 TechSetCost
sdword kasfGetBaseTechnologyCost(char *techName);                                                    // 0x00496d70 TechGetCost
sdword kasfTechIsResearching(void);                                                                  // 0x00496d80 TechIsResearching
void kasfEnableTraderGUI(void);                                                                      // 0x00496d90 TraderGUIDisplay
sdword kasfTraderGUIActive(void);                                                                    // 0x00496da0 TraderGUIIsDisplayed
void kasfSetTraderDialog(sdword dialogNum, char *dialogText);                                        // 0x00496db0 TraderGUIDialogSet
void kasfSetTraderPriceScale(sdword scalePercent);                                                   // 0x00496dd0 TraderGUIPriceScaleSet
void kasfSetTraderDisabled(sdword disable);                                                          // 0x00496de0 TraderGUIDisable
sdword kasfGetTraderPriceScale(void);                                                                // 0x00496df0 TraderGUIPriceScaleGet
sdword kasfRUsGet(sdword player);                                                                    // 0x00496e00 RUsGet
void kasfRUsSet(sdword player, sdword RUs);                                                          // 0x00496e20 RUsSet
sdword kasfGetWorldResources(void);                                                                  // 0x00496e40 GetWorldResources
void kasfSoundEvent(sdword event);                                                                   // 0x00496e60 SoundEvent
void kasfSoundEventShips(GrowSelection *ships, sdword event);                                        // 0x00496e80 SoundEventShips
void kasfSpeechEvent(sdword event, sdword variable);                                                 // 0x00496eb0 SpeechEvent
void kasfSpeechEventLink(sdword event, sdword variable, sdword bFirstLink);                          // 0x00496f00 SpeechEventLink
void kasfSpeechEventNoLink(sdword event, sdword variable);                                           // 0x00496f80 SpeechEventNoLink
void kasfSpeechEventShips(GrowSelection *ships, sdword event, sdword variable);                      // 0x00496fc0 SpeechEventShips
void kasfMusicPlay(sdword trackNum);                                                                 // 0x00497000 MusicPlay
void kasfMusicStop(sdword fadeTime);                                                                 // 0x00497010 MusicStop
sdword kasfRenderedShips(GrowSelection *ships, sdword LOD);                                          // 0x00497020 RenderedShips
void kasfResetShipRenderFlags(GrowSelection *ships);                                                 // 0x00497080 ResetShipRenderFlags
sdword kasfRenderedDerelicts(char *derelicttype, sdword LOD);                                        // 0x004970c0 RenderedDerelictType
void kasfResetDerelictRenderFlags(char *derelicttype);                                               // 0x00497110 ResetDerelictRenderFlags
void kasfGateDestroy(hvector *gatePoint);                                                            // 0x00497140 GateDestroy
void kasfGateShipsIn(GrowSelection *ships, hvector *gatePoint);                                      // 0x00497150 GateShipsIn
void kasfGateShipsOut(GrowSelection *ships, hvector *gatePoint);                                     // 0x00497170 GateShipsOut
sdword kasfFindSelectedShips(GrowSelection *ships);                                                  // 0x004971a0 FindSelectedShips
sdword kasfShipsDisabled(GrowSelection *ships);                                                      // 0x00497200 ShipsDisabled
void kasfSaveLevel(sdword LevelNumber, char *LevelName);                                             // 0x00497290 SaveLevel
void kasfClearScreen(void);                                                                          // 0x004973b0 ClearScreen
void kasfWideScreenIn(sdword frames);                                                                // 0x00497420 wideScreenIn
void kasfWideScreenInDontCloseSensors(sdword frames);                                                // 0x00497580 wideScreenInDontCloseSensors
void kasfWideScreenOut(sdword frames);                                                               // 0x00497630 wideScreenOut
void kasfSubtitleSimulate(sdword actor, sdword milliseconds, char *text);                            // 0x004976a0 SubtitleSimulate
void kasfSubtitleReset(void);                                                                        // 0x004976f0 SubtitleReset
void kasfLocationCard(sdword milliseconds, char *location);                                          // 0x00497710 LocationCard
sdword kasfRaceOfHuman(void);                                                                        // 0x00497790 RaceOfHuman
void kasfHideShips(GrowSelection *shipstohide);                                                      // 0x004977a0 HideShips
void kasfUnhideShips(GrowSelection *shipstounhide);                                                  // 0x00497800 UnhideShips
void kasfDeleteShips(GrowSelection *shipstodelete);                                                  // 0x00497850 DeleteShips
void kasfRotateDerelictType(char *derelictType, sdword rot_x, sdword rot_y, sdword rot_z, sdword variation); // 0x00497980 RotateDerelictType
void kasfUniversePause(void);                                                                        // 0x00497a90 PauseUniverse
void kasfUniverseUnpause(void);                                                                      // 0x00497aa0 UnpauseUniverse
void kasfPauseBuildShips(void);                                                                      // 0x00497ab0 PauseShipBuilding
void kasfUnpauseBuildShips(void);                                                                    // 0x00497ac0 UnpauseShipBuilding
void kasfOtherKASPause(void);                                                                        // 0x00497ad0 PauseOtherKAS
void kasfOtherKASUnpause(void);                                                                      // 0x00497af0 UnpauseOtherKAS
sdword kasfIntelEventEnded(void);                                                                    // 0x00497b00 IntelEventEnded
void kasfIntelEventNotEnded(void);                                                                   // 0x00497b80 IntelEventNotEnded
void kasfForceIntelEventEnded(void);                                                                 // 0x00497b90 ForceIntelEventEnded
void kasfSensorsStaticOn(void);                                                                      // 0x00497ba0 SensorsStaticOn
void kasfSensorsStaticOff(void);                                                                     // 0x00497ca0 SensorsStaticOff
void kasfSpawnEffect(GrowSelection *ships, char *effect, sdword effectParam);                        // 0x00497cb0 SpawnEffect
sdword kasfSpecialOn(GrowSelection *ships);                                                          // 0x00497d30 SpecialOn
sdword kasfSpecialOff(void);                                                                         // 0x00497db0 SpecialOff
sdword kasfShipsSpecialOn(GrowSelection *srcShips, GrowSelection *destShips);                        // 0x00497dc0 ShipsSpecialOn
sdword kasfShipsSpecialOff(GrowSelection *ships);                                                    // 0x00497e40 ShipsSpecialOff
sdword kasfShipsSelectLowFuel(GrowSelection *newShips, GrowSelection *originalShips, sdword percentFuel); // 0x00497e70 ShipsSelectLowFuel
void kasfDeleteShipsWhenMissionEnds(GrowSelection *shipstodelete);                                   // 0x00497ed0 DeleteShipsWhenMissionEnds
void kasfSetCurrentMimicItem(sdword itemType, char *shipName);                                       // 0x00497f10 SetCurrentMimicItem
void kasfSetMCVItemIndex(sdword itemType, sdword index);                                             // 0x00497f70 SetMCVItemIndex
void kasfMimic(GrowSelection *ships, char *shipName, sdword playerIndex);                            // 0x00497fd0 Mimic
void kasfUnmimic(GrowSelection *ships);                                                              // 0x00498080 Unmimic
void kasfEnableShipConstruction(char *shipType, char *shipRace, sdword bEnabled);                    // 0x004980c0 EnableShipConstruction
void kasfShipsAttackCapable(GrowSelection *pShip, sdword bCapable);                                  // 0x00498100 ShipsAttackCapable
sdword kasfGetShipsClampedTo(GrowSelection *pShipsAttached, GrowSelection *pHostShips);              // 0x00498150 GetShipsClampedTo
void kasfStartLevelByHyperspacingShips(void);                                                        // 0x00498230 StartLevelByHyperspacingShips
void kasfLockoutMainRegion(sdword lock);                                                             // 0x00498250 LockoutMainRegion
void kasfShipsScuttle(GrowSelection *ships);                                                         // 0x00498270 ShipsScuttle
void kasfMakeShipCamera(GrowSelection *ship);                                                        // 0x00498290 MakeShipCamera
void kasfSetShipPositionXYZ(GrowSelection *ship, sdword x, sdword y, sdword z);                      // 0x00498300 SetShipPositionXYZ
void kasfSetShipPosition(GrowSelection *ship, hvector *destination);                                 // 0x00498360 SetShipPosition
void kasfSetShipPositionNear(GrowSelection *ship, hvector *detinstation, sdword distance);           // 0x004983b0 SetShipPositionNear
void kasfShowMinesInSensors(sdword showMines);                                                       // 0x00498490 ShowMinesInSensors
sdword kasfChaseMode(void);                                                                          // 0x004984a0 ChaseMode
void kasfToggleChaseMode(void);                                                                      // 0x004984b0 ToggleChaseMode
void kasfShipsSetInvincible(GrowSelection *ships, sdword invincible);                                // 0x004984c0 ShipsSetInvincible
void kasfShipsSetDerelict(GrowSelection *ships, sdword bDerelict);                                   // 0x00498510 ShipsSetDerelict
void kasfSetMotionCues(sdword cue);                                                                  // 0x00498550 SetMotionCues
void kasfHaltAllPlayerShips(void);                                                                   // 0x00498590 HaltAllPlayerShips
sdword kasfShipsSelectRace(GrowSelection *newShips, GrowSelection *originalShips, sdword shipType);  // 0x004985f0 ShipsSelectRace
sdword kasfShipsUnderAttackElsewhere(GrowSelection *ships, GrowSelection *attackers);                // 0x00498630 ShipsUnderAttackElsewhere
void kasfShipsBeastify(GrowSelection *ships, GrowSelection *originator);                             // 0x004986f0 ShipsBeastify
void kasfGetPlayerMothership(GrowSelection *ships);                                                  // 0x00498790 GetPlayerMothership
sdword kasfFindEnemyShipsInsideWithoutStealthy(Volume *volume, GrowSelection *ships);                // 0x004987e0 FindEnemyShipsInsideWithoutStealthy
sdword kasfFindEnemiesNearbyWithoutStealthy(GrowSelection *ships, sdword radius);                    // 0x00498870 FindEnemiesNearbyWithoutStealthy
sdword kasfFindEnemiesNearTeamWithoutStealthy(GrowSelection *ships, sdword radius);                  // 0x00498970 FindEnemiesNearTeamWithoutStealthy
void kasfShipsSetBeastInfectionUnlimited(GrowSelection *ships, sdword enable);                       // 0x00498a60 ShipsSetBeastInfectionUnlimited
sdword kasfShipsSelectBeastified(GrowSelection *newShips, GrowSelection *originalShips);             // 0x00498ae0 ShipsSelectBeastified
sdword kasfShipsSelectSalvaged(GrowSelection *newShips, GrowSelection *originalShips);               // 0x00498b30 ShipsSelectSalvaged
void kasfPingMultipleAddShips(GrowSelection *ships, char *label);                                    // 0x00498b80 PingMultipleAddShips
void kasfGiveTechToAI(char *szTechName);                                                             // 0x00498c10 GiveTechToAI
void kasfShipsUpgrade(GrowSelection *ships);                                                         // 0x00498c70 UpgradeShips
sdword kasfGetShipCountSalvaged(char *shipType, sdword player);                                      // 0x00498cb0 GetShipCountSalvaged
sdword kasfGetShipCountBeastified(char *shipType, sdword player);                                    // 0x00498cd0 GetShipCountBeastified
void kasfMakeShipsControllable(GrowSelection *ships, sdword controllable);                           // 0x00498cf0 MakeShipsControllable
sdword kasfGetNumberShipsPlayerKilled(sdword playerIndex, sdword shipClass);                         // 0x00498d50 GetNumberShipsPlayerKilled
sdword kasfGetNumberShipsPlayerLost(sdword playerIndex, sdword shipClass);                           // 0x00498d70 GetNumberShipsPlayerLost
void kasfFirstShip(GrowSelection *pDst, GrowSelection *pSrc);                                        // 0x00498d90 FirstShip
void kasfVectorAdd(udword label, sdword x, sdword y, sdword z);                                      // 0x00498dd0 PointAdd
void kasfVectorCopy(udword label, hvector *vec);                                                     // 0x00498e30 PointCopy
void kasfVectorCrossProduct(hvector *Result, hvector *One, hvector *Two);                            // 0x00498e80 PointCrossProduct
sdword kasfVectorDotProduct(hvector *One, hvector *Two);                                             // 0x00498ef0 PointDotProduct
void kasfVectorParametric(hvector *result, hvector *start, hvector *end, sdword t, sdword maxt);     // 0x00498f30 PointParametric
void kasfVectorAddTo(hvector *result, hvector *add);                                                 // 0x00499020 PointAddTo
void kasfVectorSubFrom(hvector *result, hvector *sub);                                               // 0x004990e0 PointSubFrom
void kasfVectorScalarMult(hvector *result, sdword numerator, sdword denominator);                    // 0x004991a0 PointScalarMult
void kasfVectorNear(hvector *result, hvector *near, sdword distance);                                // 0x004991d0 PointNear
void kasfSalvage(GrowSelection *ships);                                                              // 0x00499330 Salvage
void kasfRepair(GrowSelection *ships);                                                               // 0x00499460 Repair
sdword kasfHasShipBeenDiscovered(GrowSelection *ship);                                               // 0x00499570 HasShipBeenDiscovered
void kasfFindSlipgate(Volume *volume, GrowSelection *slipgates);                                     // 0x004995e0 FindSlipgate
void kasfShipsDockInstant(GrowSelection *shipSel, GrowSelection *hostSel);                           // 0x00499670 ShipsDockInstant
void kasfSlipgateDockInstant(GrowSelection *slipgate);                                               // 0x004996e0 SlipgateDockInstant
void kasfSlipgateDock(GrowSelection *slipgate);                                                      // 0x00499760 SlipgateDock
void kasfStopInstant(void);                                                                          // 0x00499790 StopInstant
sdword kasfEnteringSlipgate(void);                                                                   // 0x00499820 EnteringSlipgate
sdword kasfIsIdle(GrowSelection *ships);                                                             // 0x00499870 IsIdle
sdword kasfIsTeamIdle(struct AITeam *team);                                                          // 0x004998b0 IsTeamIdle
sdword kasfThisTeamIdle(void);                                                                       // 0x004998c0 ThisTeamIdle
sdword kasfShipsInVolume(Volume *volume, GrowSelection *shipsToFind);                                // 0x004998d0 ShipsInVolume
void kasfInfectionMissileAttack(GrowSelection *targetSel);                                           // 0x004999c0 InfectionMissileAttack
sdword kasfGetBuildTime(char *ShipType);                                                             // 0x00499b80 GetBuildTime
sdword kasfGetBuildCost(char *ShipType);                                                             // 0x00499bb0 GetBuildCost
void kasfShipsCreate(GrowSelection *createdShips, char *shiptype, sdword count, sdword owner);       // 0x00499bd0 ShipsCreate
void kasfShipsCreateLaunch(GrowSelection *createdShips, char *shiptype, sdword count, GrowSelection *host, sdword owner); // 0x00499c70 ShipsCreateLaunch
void kasfShipsCreateHyperspace(GrowSelection *createdShips, char *shiptype, sdword count, hvector *dest, sdword owner); // 0x00499e40 ShipsCreateHyperspace
sdword kasfPlayerNearShips(GrowSelection *ships, sdword distance);                                   // 0x00499f90 PlayerNearShips
void kasfShipsSetBulimia(GrowSelection *ships, sdword bulimia);                                      // 0x0049a030 ShipsSetBulimia
void kasfShipsEffectedByMines(GrowSelection *ships, sdword effectedByMines);                         // 0x0049a080 ShipsEffectedByMines
sdword kasfShipInfectionTerminal(GrowSelection *ships);                                              // 0x0049a0c0 ShipInfectionTerminal
void kasfShipsSort(GrowSelection *ships, hvector *origin);                                           // 0x0049a100 ShipsSort
void kasfEnableNaggarok(GrowSelection *pNaggarok, sdword bEnable);                                   // 0x0049a220 EnableNaggarok
void kasfForceCataclysmDefaultMode(void);                                                            // 0x0049a250 ForceCataclysmDefaultMode
void kasfSetSensorsLookAtPoint(hvector *point);                                                      // 0x0049a2a0 SetSensorsLookAtPoint
void kasfShipsSetHelpless(GrowSelection *ships, sdword helpless);                                    // 0x0049a2d0 ShipsSetHelpless
void kasfTutSelectShips(GrowSelection *pShips);                                                      // 0x0049a350 TutSelectShips
sdword kasfNomadMoonEmitterCount(GrowSelection *NomadMoon);                                          // 0x0049a360 NomadMoonEmitterCount
sdword kasfNomadMoonCanFire(GrowSelection *NomadMoon);                                               // 0x0049a380 NomadMoonCanFire
sdword kasfNomadMoonFire(GrowSelection *NomadMoon);                                                  // 0x0049a3a0 NomadMoonFire
sdword kasfGetNaggarokMissiles(GrowSelection *Naggarok);                                             // 0x0049a3c0 GetNaggarokMissiles
void kasfDecNaggarokMissiles(GrowSelection *Naggarok);                                               // 0x0049a3e0 DecNaggarokMissiles
void kasfEnableBeastWeaponAutoAttack(GrowSelection *pBeastShips, sdword bEnable);                    // 0x0049a400 EnableBeastWeaponAutoAttack
void kasfSetAIDepotShip(GrowSelection *pShip);                                                       // 0x0049a460 SetAIDepotShip
void kasfMission13DeleteEngineeringModuleHack(void);                                                 // 0x0049a480 Mission13DeleteEngineeringModuleHack
void kasfSlipgateExplode(hvector *center);                                                           // 0x0049a490 SlipgateExplode
void kasfShipsHyperspaceOut(GrowSelection *ships);                                                   // 0x0049a4d0 ShipsHyperspaceOut
void kasfShipsHyperspaceIn(GrowSelection *ships, hvector *point);                                    // 0x0049a530 ShipsHyperspaceIn
void kasfShipsHyperspaceInDefault(GrowSelection *ships);                                             // 0x0049a560 ShipsHyperspaceInDefault
void kasfForceParadeFormation(GrowSelection *mothership, GrowSelection *fleet);                      // 0x0049a5f0 ForceParadeFormation
sdword kasfIsForceFieldActive(GrowSelection *Ships);                                                 // 0x0049a6a0 IsForceFieldActive
sdword kasfIsEMPReady(GrowSelection *Ships);                                                         // 0x0049a6f0 IsEMPReady
sdword kasfIsBeastReady(GrowSelection *Ships);                                                       // 0x0049a740 IsBeastReady
sdword kasfIsGuarding(GrowSelection *Ships, GrowSelection *Guarded);                                 // 0x0049a790 IsGuarding
void kasfSetStayMimicked(GrowSelection *pShips, sdword bStayMimicked);                               // 0x0049a850 SetStayMimicked
void kasfGetDockHost(GrowSelection *Ships, GrowSelection *Host, GrowSelection *PossibleHost);        // 0x0049a8b0 GetDockHost
sdword kasfGetTargets(GrowSelection *Targets, GrowSelection *Ships);                                 // 0x0049a950 GetTargets
void kasfRKOBigGunPos(GrowSelection *RKO, hvector *Bullet);                                          // 0x0049aa00 RKOBigGunPos
sdword kasfRKOIsSafe(GrowSelection *RKO);                                                            // 0x0049aa40 RKOIsSafe
sdword kasfRKOAvoidLocation(GrowSelection *RKO, hvector *Avoid, hvector *Safe);                      // 0x0049aa60 RKOAvoidLocation
sdword kasfRKOBeware(GrowSelection *RKO);                                                            // 0x0049aac0 RKOBeware
void kasfShipsSetColor(GrowSelection *Ships, sdword Color);                                          // 0x0049aae0 ShipsSetColor
sdword kasfIsSwarmReady(GrowSelection *Ships);                                                       // 0x0049ab20 IsSwarmReady
sdword kasfShipsHealthAverage(GrowSelection *Ships);                                                 // 0x0049ab70 ShipsHealthAverage
void kasfShipsSetInvincibleInWidescreen(GrowSelection *pShips, sdword bInvincibleInWidescreen);      // 0x0049abd0 ShipsSetInvincibleInWidescreen
void kasfDisableBigGunFiring(void);                                                                  // 0x0049ac30 DisableBigGun
void kasfShipsShuffle(GrowSelection *pShips);                                                        // 0x0049ac40 ShipsShuffle
void kasfShipsStopForResearchDocking(GrowSelection *pShips, sdword bStop);                           // 0x0049ac90 ShipsStopForResearchDocking
sdword kasfGetVectorX(hvector *vector);                                                              // 0x0049ace0 GetPointX
sdword kasfGetVectorY(hvector *vector);                                                              // 0x0049ad00 GetPointY
sdword kasfGetVectorZ(hvector *vector);                                                              // 0x0049ad20 GetPointZ
sdword kasfGetVectorW(hvector *vector);                                                              // 0x0049ad40 GetPointW
void kasfCameraSetChaseMode(sdword chaseMode);                                                       // 0x0049ad60 CameraSetChaseMode
sdword kasfShipsInsideMe(GrowSelection *InsideMe, GrowSelection *Me);                                // 0x0049ad80 ShipsInsideMe
void kasfAssert(sdword value);                                                                       // 0x0049ae30 dbgAssert
void kasfGameEnd(void);                                                                              // 0x0049ae30 GameEnd
void kasfGateMoveToNearest(void);                                                                    // 0x0049ae30 GateMoveToNearest
void kasfLog(char *string);                                                                          // 0x0049ae30 Log
void kasfSetPlayerResourceCap(sdword maxResources);                                                  // 0x0049ae30 SetPlayerResourceCap
void kasfTeamSkillSet(sdword skillLevel);                                                            // 0x0049ae30 TeamSkillSet
void kasfToggleActor(sdword Actor, sdword on);                                                       // 0x0049ae30 ToggleActor
sdword kasfGateShipsOutNearest(GrowSelection *ships);                                                // 0x006fe9a0 GateShipsOutNearest
sdword kasfTeamSkillGet(void);                                                                       // 0x006fe9a0 TeamSkillGet

/*-----------------------------------------------------------------------------
    CAT (M2-R8 kasf library): the kasf library's helpers outside the function table
-----------------------------------------------------------------------------*/
void SelectionSwitchPlayerOwner(SelectCommand *selection, bool32 keepSU);   // 0x00493f80
sdword kasfFindEnemyShipsInside(Volume *volume, GrowSelection *ships);       // 0x00492e10
void cat_kasAutoSaveOnLoad(char *filename);                                  // 0x00497230 (LoadGame 0x004fe0e0)
void cat_kasSaveLevelNow(void);                                              // 0x004972d0 (univUpdate 0x0054ce03)
extern bool32 cat_kasSaveLevelRequested;                                     // 0x00a43fec (kasfSaveLevel sets it)
bool32 cat_kasWideScreenActive(void);                                        // 0x004973c0 (nisUpdateTask)
void cat_kasWideScreenInFinish(void);                                        // 0x004973d0 (nisUpdateTask)
void cat_kasWideScreenInHyperspace(sdword frames);                           // 0x004974d0 (the single-player hyperspace code)

#endif
