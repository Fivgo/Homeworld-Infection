// =============================================================================
//  P1StandardCorvette.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 5/06/1998 by ddunlop
// =============================================================================

#include "P1StandardCorvette.h"

#include "Attack.h"
#include "DefaultShip.h"

typedef struct
{
    AttackSideStep attacksidestep;
} P1StandardCorvetteSpec;

typedef struct
{
    AttackSideStepParameters sidestepParameters;
} P1StandardCorvetteStatics;

P1StandardCorvetteStatics P1StandardCorvetteStatic;

void P1StandardCorvetteStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    P1StandardCorvetteStatics *corvstat = &P1StandardCorvetteStatic;

    memset(corvstat, 0, sizeof(P1StandardCorvetteStatics));
    scriptSetStruct(directory,filename,AttackSideStepParametersScriptTable,(ubyte *)&corvstat->sidestepParameters);

    statinfo->custstatinfo = corvstat;
}

void P1StandardCorvetteInit(Ship *ship)
{
    P1StandardCorvetteSpec *spec = (P1StandardCorvetteSpec *)ship->ShipSpecifics;

    attackSideStepInit(&spec->attacksidestep);
}

void P1StandardCorvetteAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    P1StandardCorvetteSpec *spec = (P1StandardCorvetteSpec *)ship->ShipSpecifics;
    P1StandardCorvetteStatics *corvstat = (P1StandardCorvetteStatics *)((ShipStaticInfo *)ship->staticinfo)->custstatinfo;

    attackSideStep(ship,target,&spec->attacksidestep,&corvstat->sidestepParameters);
}

void P1StandardCorvetteAttackPassive(Ship *ship,Ship *target,bool32 rotate)
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

CustShipHeader P1StandardCorvetteHeader =
{
    // CAT: Cataclysm slot order (CustShipHeader, SpaceObj.h; no shiptype field, Attack in slot 21).
    //      The Cataclysm-only slots stay NULL until their bodies are ported (stage 2).
    .sizeofShipSpecifics = sizeof(P1StandardCorvetteSpec),
    .CustShipStaticInit = P1StandardCorvetteStaticInit,
    .CustShipInit = P1StandardCorvetteInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = P1StandardCorvetteAttackPassive,
    .CustShipAttack = P1StandardCorvetteAttack,
};

