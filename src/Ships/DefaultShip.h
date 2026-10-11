// =============================================================================
//  DefaultShip.h
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#ifndef ___DEFAULTSHIP_H
#define ___DEFAULTSHIP_H

#include "SpaceObj.h"

extern CustShipHeader DefaultShipHeader;
extern CustShipHeader DoNothingShipHeader;
extern CustShipHeader StationaryGunHeader;
extern CustShipHeader MiningBaseHeader;

void DefaultShipFire(Ship *ship, SpaceObjRotImpTarg *target);
void DefaultShipAttackPassive(Ship *ship,Ship *target,bool32 rotate);

// CAT (M2-R4 ships-b): the exe's shared one-instruction bodies several headers point at: nullsub 0x0049ae30 ("ret")
//      and kasfSpecialOff 0x00497db0 ("return 1"); C needs one function per slot type
void nullsubStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo);
void nullsubShip(Ship *ship);
void nullsubAttackPassive(Ship *ship,Ship *target,bool32 rotate);
bool32 kasfSpecialOffShip(Ship *ship);
bool32 kasfSpecialOffTarget(Ship *ship,void *custom);
bool32 nullsubReturnFalseTarget(Ship *ship,void *custom);     // CAT: nullsubReturnFalse 0x006fe9a0 in slot 8

#endif
