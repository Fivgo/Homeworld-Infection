// =============================================================================
//  CommandWrap.h
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 7/30/1997 by gshaw
// =============================================================================

#ifndef ___COMMANDWRAP_H
#define ___COMMANDWRAP_H

#include "CommandLayer.h"
#include "Globals.h"

void clCommandMessage(char *CommandMessage, udword colour);                // CAT: 0x004ac820, the colour as the exe

void clWrapMove(CommandLayer *comlayer,SelectCommand *selectcom,sdword pathType,vector from,sdword numPoints,vector *points);   // CAT: waypoint path (0x004529e0)
void clWrapAttack(CommandLayer *comlayer,SelectCommand *selectcom,AttackCommand *attackcom);
void clWrapFormation(CommandLayer *comlayer,SelectCommand *selectcom,TypeOfFormation formation,sdword catForceFieldLevel);   // CAT: + level (0x00452ca0)
void clWrapDock(CommandLayer *comlayer,SelectCommand *selectcom,DockType dockType,ShipPtr dockwith);
// CAT (M2-R6 managers): returns the new ship (clWrapCreateShip 0x00452f30 returns clCreateShip's ship, NULL when it
//      sends the order; cmBuildTaskFunction 0x00454940 keeps it)
Ship *clWrapCreateShip(CommandLayer *comlayer,ShipType shipType,ShipRace shipRace,uword playerIndex,ShipPtr creator);
void clWrapBuildShip(CommandLayer *comlayer,ShipType shipType,ShipRace shipRace,uword playerIndex,ShipPtr creator);

void clWrapDeterministicBuild(udword command,
                              CommandLayer* comlayer, sdword numShips,
                              ShipType shipType, ShipRace shipRace,
                              uword playerIndex, ShipPtr creator);

void clWrapCollectResource(CommandLayer *comlayer,SelectCommand *selectcom,ResourcePtr resource);
void clWrapProtect(CommandLayer *comlayer,SelectCommand *selectcom,ProtectCommand *protectcom);
void clWrapSpecial(CommandLayer *comlayer,SelectCommand *selectcom,SpecialCommand *targets);
void clWrapHalt(CommandLayer *comlayer,SelectCommand *selectcom);
void clWrapScuttle(CommandLayer *comlayer,SelectCommand *selectcom);
void clWrapAutoLaunch(udword OnOff,udword playerIndex);
void clWrapSetAlliance(udword AllianceType, uword curalliance, uword newalliance);
void clWrapLaunchMultipleShips(CommandLayer *comlayer,SelectCommand *selectcom,ShipPtr launchFrom);
void clWrapSetTactics(CommandLayer *comlayer,SelectCommand *selectcom,TacticsType tacticstype);
void clWrapSetKamikaze(CommandLayer *comlayer,SelectCommand *selectcom);
void clWrapSetMilitaryParade(CommandLayer *comlayer,SelectCommand *selectcom);
void clWrapRUTransfer(CommandLayer *comlayer,sdword fromIndex,sdword toIndex, sdword resourceUnits,ubyte flags);
void clWrapMpHyperspace(CommandLayer *comlayer,SelectCommand *selectcom,vector from,vector to);
void clWrapResearch(udword type, udword playernum, udword labnum, udword tech);
void cat_clWrapStartExternalBuildTimer(CommandLayer *comlayer,SelectCommand *selectcom);   // CAT: 0x00453660
void cat_clWrapExternalBuildUpdate(CommandLayer *comlayer,uword playerIndex,Ship *creator,Ship *ship,real32 progress,sdword mode);   // CAT: 0x00453a00
void cat_clWrapLink(CommandLayer *comlayer,SelectCommand *selectcom);                                         // CAT: 0x00453940
void cat_clWrapUnlink(CommandLayer *comlayer,SelectCommand *selectcom);                                       // CAT: 0x004539a0
void cat_clWrapSetLeechAction(CommandLayer *comlayer,SelectCommand *selectcom,ubyte mode);                    // CAT: 0x00453730
void cat_clWrapUnleech(CommandLayer *comlayer,SelectCommand *selectcom);                                      // CAT: 0x004536b0
void cat_clWrapSetModuleSlot(CommandLayer *comlayer,SelectCommand *selectcom,sdword moduleType,sdword count); // CAT: 0x00453580
void cat_clWrapSetBeastSystem(CommandLayer *comlayer,Ship *ship,sdword system);                               // CAT: 0x004535f0
void cat_clWrapPointOrder(CommandLayer *comlayer,SelectCommand *selectcom,vector *point);                     // CAT: 0x00453a70
void cat_clWrapMimic(CommandLayer *comlayer,SelectCommand *selectcom,udword mode,udword shipType,udword player,udword index);   // CAT: 0x00453b30
void cat_clWrapUpgradeShips(CommandLayer *comlayer,SelectCommand *selectcom,bool32 cancel);                   // CAT: 0x00453ad0
void cat_clWrapSpecialTarget(CommandLayer *comlayer,SelectCommand *selectcom,SpecialCommand *targets,bool32 flag);   // CAT: 0x00453130
#endif
