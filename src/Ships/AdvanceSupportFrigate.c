// =============================================================================
//  AdvanceSupportFrigate.c
// =============================================================================
//  Copyright Relic Entertainment, Inc.  All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#include "AdvanceSupportFrigate.h"

#include "Attack.h"
#include "Debug.h"
#include "DefaultShip.h"
#include "RepairCorvette.h"
#include "ShipSelect.h"
#include "StatScript.h"

typedef struct
{
    udword dummy;
} AdvanceSupportFrigateSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(AdvanceSupportFrigateSpec) == 4, "AdvanceSupportFrigateSpec is 4 bytes in cataclysm.exe (AdvanceSupportFrigateHeader sizeofShipSpecifics)");   // CAT
#endif

typedef struct
{
    real32 asfGunRange[NUM_TACTICS_TYPES];
    real32 asfTooCloseRange[NUM_TACTICS_TYPES];
    real32 repairApproachDistance;
} AdvanceSupportFrigateStatics;

AdvanceSupportFrigateStatics AdvanceSupportFrigateStatic;
// CAT: one block per race that has the ship: Kushan (0x00ab74a0) and Taiidan (0x00ab74c0)
//      (AdvanceSupportFrigateStaticInit 0x005aef00); HW1 had R1/R2
AdvanceSupportFrigateStatics AdvanceSupportFrigateStaticKushan;
AdvanceSupportFrigateStatics AdvanceSupportFrigateStaticTaiidan;

scriptStructEntry ASFStaticScriptTable[] =
{
    { "repairApproachDistance",    scriptSetReal32CB,  &(AdvanceSupportFrigateStatic.repairApproachDistance),  &(AdvanceSupportFrigateStatic) },

    END_SCRIPT_STRUCT_ENTRY
};


void AdvanceSupportFrigateStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    udword i;
    AdvanceSupportFrigateStatics *asfstat;

    // CAT: by Cataclysm race (AdvanceSupportFrigateStaticInit 0x005aef00). For any other race the exe uses the
    //      statinfo itself as the block (only kAdvanceSupportFrigate/tAdvanceSupportFrigate use this header); the
    //      port asserts and uses the script template instead
    switch (statinfo->shiprace)
    {
        case RACE_Kushan:  asfstat = &AdvanceSupportFrigateStaticKushan;  break;
        case RACE_Taiidan: asfstat = &AdvanceSupportFrigateStaticTaiidan; break;
        default:
            dbgAssertOrIgnore(FALSE);
            asfstat = &AdvanceSupportFrigateStatic;
            break;
    }

    statinfo->custstatinfo = asfstat;

    for(i=0;i<NUM_TACTICS_TYPES;i++)
    {
        asfstat->asfGunRange[i] = statinfo->bulletRange[i];
        asfstat->asfTooCloseRange[i] = statinfo->minBulletRange[i] * 0.9f;
    }
    
    scriptSetStruct(directory,filename,ASFStaticScriptTable,(ubyte *)asfstat);
}

void AdvanceSupportFrigateAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    AdvanceSupportFrigateStatics *asfstat = (AdvanceSupportFrigateStatics *)shipstaticinfo->custstatinfo;

    attackStraightForward(ship,target,asfstat->asfGunRange[ship->tacticstype],asfstat->asfTooCloseRange[ship->tacticstype]);
}

void AdvanceSupportFrigateAttackPassive(Ship *ship,Ship *target,bool32 rotate)
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

void AdvanceSupportFrigateOrderChanged(Ship *ship)
{
    // CAT: new slot-9 body (AdvanceSupportFrigateOrderChanged 0x005aef90): a new order ends a running repair effect
    if (ship->rceffect != NULL)
    {
        stopRepairEffect(ship);
    }
}

bool32 AdvacedSupportFrigateSpecialTarget(Ship *ship,void *custom)
{
    AdvanceSupportFrigateStatics *asfstat = (AdvanceSupportFrigateStatics *)ship->staticinfo->custstatinfo;
    SelectAnyCommand *targets;
    targets = (SelectAnyCommand *)custom;
    return(refuelRepairShips(ship, targets,asfstat->repairApproachDistance));
}

// CAT: the exe's header (AdvanceSupportFrigateHeader 0x008c3408: kAdvanceSupportFrigate, tAdvanceSupportFrigate).
//      Slot 6 is the shared "rotate ? attackPassiveRotate : attackPassive" body and slot 21 the shared
//      attackStraightForward(gunRange, tooCloseRange) body, which the linker folded into P1MothershipAttackPassive
//      0x005be540 and MissileDestroyerAttack 0x005bb3e0; this file's own copies are the same code.
CustShipHeader AdvanceSupportFrigateHeader =
{
    .sizeofShipSpecifics = sizeof(AdvanceSupportFrigateSpec),
    .CustShipStaticInit = AdvanceSupportFrigateStaticInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = AdvanceSupportFrigateAttackPassive,
    .CustShipSpecialTarget = AdvacedSupportFrigateSpecialTarget,
    .CustShipOrderChanged = AdvanceSupportFrigateOrderChanged,     // CAT: new slot 9
    .CustShipAttack = AdvanceSupportFrigateAttack,
};
