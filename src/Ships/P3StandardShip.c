// =============================================================================
//  P3StandardShip.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 5/07/1998 by ddunlop
// =============================================================================

#include "P3StandardShip.h"

#include "Attack.h"
#include "Debug.h"
#include "DefaultShip.h"

typedef struct
{
    udword dummy;
} P3StandardShipSpec;


typedef struct
{
    real32 shipGunRange[NUM_TACTICS_TYPES];
    real32 shipTooCloseRange[NUM_TACTICS_TYPES];
} P3StandardShipStatics;

P3StandardShipStatics P3MegashipStatic;
P3StandardShipStatics P3DestroyerStatic;
P3StandardShipStatics P3FrigateStatic;

void P3StandardShipStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    udword i;
    P3StandardShipStatics *shipstat=NULL;

    switch (statinfo->shiptype)
    {
        case P3Destroyer :
            shipstat = &P3DestroyerStatic;
            break;
        case P3Frigate :
            shipstat = &P3FrigateStatic;
            break;
        case P3Megaship :
            shipstat = &P3MegashipStatic;
            break;
        default:
            break;
    }

    dbgAssertOrIgnore(shipstat != NULL);

    statinfo->custstatinfo = shipstat;

    for(i=0;i<NUM_TACTICS_TYPES;i++)
    {
        shipstat->shipGunRange[i] = statinfo->bulletRange[i] * 0.9f;
        shipstat->shipTooCloseRange[i] = statinfo->minBulletRange[i] * 0.8f;
    }
}

void P3StandardShipAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    P3StandardShipStatics *shipstat = (P3StandardShipStatics *)shipstaticinfo->custstatinfo;

    attackStraightForward(ship,target,shipstat->shipGunRange[ship->tacticstype],shipstat->shipTooCloseRange[ship->tacticstype]);
}

void P3StandardShipAttackPassive(Ship *ship,Ship *target,bool32 rotate)
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

CustShipHeader P3StandardShipHeader =
{
    // CAT: Cataclysm slot order (CustShipHeader, SpaceObj.h; no shiptype field, Attack in slot 21).
    //      The Cataclysm-only slots stay NULL until their bodies are ported (stage 2).
    .sizeofShipSpecifics = sizeof(P3StandardShipSpec),
    .CustShipStaticInit = P3StandardShipStaticInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = P3StandardShipAttackPassive,
    .CustShipAttack = P3StandardShipAttack,
};

