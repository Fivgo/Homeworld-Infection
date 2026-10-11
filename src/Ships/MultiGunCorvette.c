// =============================================================================
//  MultiGunCorvette.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#include "MultiGunCorvette.h"

#include "Attack.h"
#include "Debug.h"
#include "DefaultShip.h"

typedef struct
{
    AttackSideStep attacksidestep;
} MultiGunCorvetteSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(MultiGunCorvetteSpec) == 8, "MultiGunCorvetteSpec is 8 bytes in cataclysm.exe (MultiGunCorvetteHeader sizeofShipSpecifics)");   // CAT
#endif

typedef struct
{
    AttackSideStepParameters sidestepParameters;
} MultiGunCorvetteStatics;

// CAT: one block per race that has the ship: Beast (0x00a7a980), Kushan (0x00a7a960), Taiidan (0x00a7a970)
//      (MultiGunCorvetteStaticInit 0x005bdb30); HW1 had R1/R2
MultiGunCorvetteStatics MultiGunCorvetteStaticBeast;
MultiGunCorvetteStatics MultiGunCorvetteStaticKushan;
MultiGunCorvetteStatics MultiGunCorvetteStaticTaiidan;
MultiGunCorvetteStatics MultiGunCorvetteStaticOther;    // CAT: the exe writes into the statinfo itself for any other race

void MultiGunCorvetteStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    MultiGunCorvetteStatics *corvstat;

    // CAT: by Cataclysm race; no memset first (MultiGunCorvetteStaticInit 0x005bdb30)
    switch (statinfo->shiprace)
    {
        case RACE_Beast:   corvstat = &MultiGunCorvetteStaticBeast;   break;
        case RACE_Kushan:  corvstat = &MultiGunCorvetteStaticKushan;  break;
        case RACE_Taiidan: corvstat = &MultiGunCorvetteStaticTaiidan; break;
        default:
            dbgAssertOrIgnore(FALSE);
            corvstat = &MultiGunCorvetteStaticOther;
            break;
    }
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

// CAT: the exe's header (MultiGunCorvetteHeader 0x008c5648: bMultiGunCorvette, kMultiGunCorvette, tMultiGunCorvette; original/scratch-m2/m2r2-shipstatic-tools/headers.txt).
//      Slot 6: the exe folded the identical "rotate ? attackPassiveRotate : attackPassive" body into P1MothershipAttackPassive 0x005be540.
//      Slot 21: the exe folded the identical attackSideStep(spec, statics) body into SwarmerAttack 0x005c79b0.
CustShipHeader MultiGunCorvetteHeader =
{
    .sizeofShipSpecifics = sizeof(MultiGunCorvetteSpec),
    .CustShipStaticInit = MultiGunCorvetteStaticInit,
    .CustShipInit = MultiGunCorvetteInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = MultiGunCorvetteAttackPassive,
    .CustShipAttack = MultiGunCorvetteAttack,
};

