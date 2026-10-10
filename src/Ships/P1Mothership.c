// =============================================================================
//  P1Mothership.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 5/06/1998 by ddunlop
// =============================================================================

#include "P1Mothership.h"

#include "Attack.h"
#include "DefaultShip.h"

typedef struct
{
    udword dummy;
} P1MothershipSpec;

typedef struct
{
    real32 p1mothershipGunRange[NUM_TACTICS_TYPES];
    real32 p1mothershipTooCloseRange[NUM_TACTICS_TYPES];
} P1MothershipStatics;

P1MothershipStatics P1MothershipStatic;

void P1MothershipStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    udword i;
    P1MothershipStatics *mothershipstat = &P1MothershipStatic;

    statinfo->custstatinfo = mothershipstat;

    for(i=0;i<NUM_TACTICS_TYPES;i++)
    {
        mothershipstat->p1mothershipGunRange[i] = statinfo->bulletRange[i];
        mothershipstat->p1mothershipTooCloseRange[i] = statinfo->minBulletRange[i] * 0.9f;
    }
}

void P1MothershipAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    P1MothershipStatics *motherstat = (P1MothershipStatics *)shipstaticinfo->custstatinfo;

    attackStraightForward(ship,target,motherstat->p1mothershipGunRange[ship->tacticstype],motherstat->p1mothershipTooCloseRange[ship->tacticstype]);
}

void P1MothershipAttackPassive(Ship *ship,Ship *target,bool32 rotate)
{
    if ((rotate) & ((bool32)((ShipStaticInfo *)(ship->staticinfo))->rotateToRetaliate))
    {
        attackPassiveRotate(ship,target);
    }
    else
    {
        attackPassive(ship,target);
    }
}

CustShipHeader P1MothershipHeader =
{
    // CAT: Cataclysm slot order (CustShipHeader, SpaceObj.h; no shiptype field, Attack in slot 21).
    //      The Cataclysm-only slots stay NULL until their bodies are ported (stage 2).
    .sizeofShipSpecifics = sizeof(P1MothershipSpec),
    .CustShipStaticInit = P1MothershipStaticInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = P1MothershipAttackPassive,
    .CustShipAttack = P1MothershipAttack,
};

