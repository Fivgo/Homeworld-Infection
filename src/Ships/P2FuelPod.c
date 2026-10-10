// =============================================================================
//  P2FuelPod.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 5/07/1998 by ddunlop
// =============================================================================

#include "P2FuelPod.h"

#include "Attack.h"
#include "DefaultShip.h"

typedef struct
{
    udword dummy;
} P2FuelPodSpec;

typedef struct
{
    real32 podGunRange[NUM_TACTICS_TYPES];
    real32 podTooCloseRange[NUM_TACTICS_TYPES];
} P2FuelPodStatics;

P2FuelPodStatics P2FuelPodStatic;

void P2FuelPodStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    udword i;
    P2FuelPodStatics *podstat = &P2FuelPodStatic;

    statinfo->custstatinfo = podstat;

    for(i=0;i<NUM_TACTICS_TYPES;i++)
    {
        podstat->podGunRange[i] = statinfo->bulletRange[i];
        podstat->podTooCloseRange[i] = statinfo->minBulletRange[i] * 0.9f;
    }
}

void P2FuelPodAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    P2FuelPodStatics *podstat = (P2FuelPodStatics *)shipstaticinfo->custstatinfo;

    attackStraightForward(ship,target,podstat->podGunRange[ship->tacticstype],podstat->podTooCloseRange[ship->tacticstype]);
}

void P2FuelPodAttackPassive(Ship *ship,Ship *target,bool32 rotate)
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

CustShipHeader P2FuelPodHeader =
{
    // CAT: Cataclysm slot order (CustShipHeader, SpaceObj.h; no shiptype field, Attack in slot 21).
    //      The Cataclysm-only slots stay NULL until their bodies are ported (stage 2).
    .sizeofShipSpecifics = sizeof(P2FuelPodSpec),
    .CustShipStaticInit = P2FuelPodStaticInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = P2FuelPodAttackPassive,
    .CustShipAttack = P2FuelPodAttack,
};

