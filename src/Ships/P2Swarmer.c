// =============================================================================
//  P2Swarmer.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#include "P2Swarmer.h"

#include "Attack.h"
#include "DefaultShip.h"

typedef struct
{
    AttackSideStep attacksidestep;
} P2SwarmerSpec;

typedef struct
{
    AttackSideStepParameters sidestepParameters;
} P2SwarmerStatics;

P2SwarmerStatics P2SwarmerStaticRace1;
P2SwarmerStatics P2SwarmerStaticRace2;


void P2SwarmerStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    P2SwarmerStatics *corvstat = (statinfo->shiprace == R1) ? &P2SwarmerStaticRace1 : &P2SwarmerStaticRace2;

    memset(corvstat, 0, sizeof(P2SwarmerStatics));
    scriptSetStruct(directory,filename,AttackSideStepParametersScriptTable,(ubyte *)&corvstat->sidestepParameters);

    statinfo->custstatinfo = corvstat;
}

void P2SwarmerInit(Ship *ship)
{
    P2SwarmerSpec *spec = (P2SwarmerSpec *)ship->ShipSpecifics;

    attackSideStepInit(&spec->attacksidestep);

}

void P2SwarmerAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    P2SwarmerSpec *spec = (P2SwarmerSpec *)ship->ShipSpecifics;
    P2SwarmerStatics *corvstat = (P2SwarmerStatics *)((ShipStaticInfo *)ship->staticinfo)->custstatinfo;

    attackSideStep(ship,target,&spec->attacksidestep,&corvstat->sidestepParameters);
}

void P2SwarmerAttackPassive(Ship *ship,Ship *target,bool32 rotate)
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


CustShipHeader P2SwarmerHeader =
{
    // CAT: Cataclysm slot order (CustShipHeader, SpaceObj.h; no shiptype field, Attack in slot 21).
    //      The Cataclysm-only slots stay NULL until their bodies are ported (stage 2).
    .sizeofShipSpecifics = sizeof(P2SwarmerSpec),
    .CustShipStaticInit = P2SwarmerStaticInit,
    .CustShipInit = P2SwarmerInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = P2SwarmerAttackPassive,
    .CustShipAttack = P2SwarmerAttack,
};

