// =============================================================================
//  P1IonArrayFrigate.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 5/06/1998 by ddunlop
// =============================================================================

#include "P1IonArrayFrigate.h"

#include "Attack.h"
#include "DefaultShip.h"

typedef struct
{
    udword dummy;
} P1IonArrayFrigateSpec;

typedef struct
{
    real32 frigateGunRange[NUM_TACTICS_TYPES];
    real32 frigateTooCloseRange[NUM_TACTICS_TYPES];
} P1IonArrayFrigateStatics;

P1IonArrayFrigateStatics P1IonArrayFrigateStatic;

void P1IonArrayFrigateStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    udword i;
    P1IonArrayFrigateStatics *frigstat = &P1IonArrayFrigateStatic;

    statinfo->custstatinfo = frigstat;

    for(i=0;i<NUM_TACTICS_TYPES;i++)
    {
        frigstat->frigateGunRange[i] = statinfo->bulletRange[i];
        frigstat->frigateTooCloseRange[i] = statinfo->minBulletRange[i] * 0.9f;
    }
}

void P1IonArrayFrigateAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    P1IonArrayFrigateStatics *frigstat = (P1IonArrayFrigateStatics *)shipstaticinfo->custstatinfo;

    attackStraightForward(ship,target,frigstat->frigateGunRange[ship->tacticstype],frigstat->frigateTooCloseRange[ship->tacticstype]);
}

void P1IonArrayFrigateAttackPassive(Ship *ship,Ship *target,bool32 rotate)
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

CustShipHeader P1IonArrayFrigateHeader =
{
    // CAT: Cataclysm slot order (CustShipHeader, SpaceObj.h; no shiptype field, Attack in slot 21).
    //      The Cataclysm-only slots stay NULL until their bodies are ported (stage 2).
    .sizeofShipSpecifics = sizeof(P1IonArrayFrigateSpec),
    .CustShipStaticInit = P1IonArrayFrigateStaticInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = P1IonArrayFrigateAttackPassive,
    .CustShipAttack = P1IonArrayFrigateAttack,
};

