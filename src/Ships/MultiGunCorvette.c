// =============================================================================
//  MultiGunCorvette.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#include "MultiGunCorvette.h"

#include "Attack.h"
#include "DefaultShip.h"

typedef struct
{
    AttackSideStep attacksidestep;
} MultiGunCorvetteSpec;

typedef struct
{
    AttackSideStepParameters sidestepParameters;
} MultiGunCorvetteStatics;

MultiGunCorvetteStatics MultiGunCorvetteStaticRace1;
MultiGunCorvetteStatics MultiGunCorvetteStaticRace2;

void MultiGunCorvetteStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    MultiGunCorvetteStatics *corvstat = (statinfo->shiprace == R1) ? &MultiGunCorvetteStaticRace1 : &MultiGunCorvetteStaticRace2;

    memset(corvstat, 0, sizeof(MultiGunCorvetteStatics));
    scriptSetStruct(directory,filename,AttackSideStepParametersScriptTable,(ubyte *)&corvstat->sidestepParameters);

    statinfo->custstatinfo = corvstat;
}

void MultiGunCorvetteInit(Ship *ship)
{
    MultiGunCorvetteSpec *spec = (MultiGunCorvetteSpec *)ship->ShipSpecifics;

    attackSideStepInit(&spec->attacksidestep);
}

void MultiGunCorvetteAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    MultiGunCorvetteSpec *spec = (MultiGunCorvetteSpec *)ship->ShipSpecifics;
    MultiGunCorvetteStatics *corvstat = (MultiGunCorvetteStatics *)((ShipStaticInfo *)ship->staticinfo)->custstatinfo;

    attackSideStep(ship,target,&spec->attacksidestep,&corvstat->sidestepParameters);
}

void MultiGunCorvetteAttackPassive(Ship *ship,Ship *target,bool32 rotate)
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

CustShipHeader MultiGunCorvetteHeader =
{
    // CAT: Cataclysm slot order (CustShipHeader, SpaceObj.h; no shiptype field, Attack in slot 21).
    //      The Cataclysm-only slots stay NULL until their bodies are ported (stage 2).
    .sizeofShipSpecifics = sizeof(MultiGunCorvetteSpec),
    .CustShipStaticInit = MultiGunCorvetteStaticInit,
    .CustShipInit = MultiGunCorvetteInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = MultiGunCorvetteAttackPassive,
    .CustShipAttack = MultiGunCorvetteAttack,
};

