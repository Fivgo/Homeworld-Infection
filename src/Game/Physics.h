/*=============================================================================
    Name    : Physics.h
    Purpose : Definitions for physics.c

    Created 6/18/1997 by gshaw
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___PHYSICS_H
#define ___PHYSICS_H

#include "SpaceObj.h"

/*=============================================================================
    Macros:
=============================================================================*/

#define physApplyForceVectorToObj(target,forcevector) vecAddTo((target)->obj.posinfo.force,(forcevector))

/*=============================================================================
    Functions:
=============================================================================*/

void physApplyForceToObj(SpaceObj *obj,real32 force,uword transdir);
void physApplyRotToObj(SpaceObjRot *obj,real32 torque,uword rotdir);
bool32 physUpdateBulletPosVel(Bullet *bullet,real32 phystimeelapsed);

void physUpdateObjPosVelShip(Ship *obj,real32 phystimeelapsed);
void physUpdateObjPosVelDerelicts(Derelict *obj,real32 phystimeelapsed);
void physUpdateObjPosVelMissile(Missile *obj,real32 phystimeelapsed);
void physUpdateObjPosVelBasic(SpaceObj *obj,real32 phystimeelapsed);

void cat_physApplyForceToVelocity(struct Ship *ship);                    // CAT: 0x004e4390 (M2-R4 combat)

void cat_physClampVelocityToUniverse(struct Ship *ship);                 // CAT: 0x004e4850 (M2-R4 combat)

#endif
