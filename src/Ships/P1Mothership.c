// =============================================================================
//  P1Mothership.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 5/06/1998 by ddunlop
// =============================================================================

#include "P1Mothership.h"

#include "Attack.h"
#include "DefaultShip.h"
#include "Mothership.h"
#include "Salvage.h"

typedef struct
{
    real32 p1mothershipGunRange[NUM_TACTICS_TYPES];
    real32 p1mothershipTooCloseRange[NUM_TACTICS_TYPES];
} P1MothershipStatics;

P1MothershipStatics P1MothershipStatic;
// CAT (M2-R5 layout-dock): cat_p1MothershipVariantStaticInit 0x005be5d0 (a 3-float copy of the statics scaled by 0.9 into
//      0x00a7a800) has no caller and no header slot in the exe: not ported

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

// CAT: the Cataclysm-only slot bodies (P1MothershipInit 0x005be530 .. P1Mothership_Fix 0x005be5c0): the P1
//      mothership is a salvage depot; each body is the depot call
void P1MothershipInit(Ship *ship)
{
    cat_salvageDepotInit(ship);
}

void P1MothershipHouseKeep(Ship *ship)
{
    cat_salvageDepotHouseKeep(ship);
}

void P1MothershipRemoveShipReferences(Ship *ship, Ship *shiptoremove)
{
    cat_salvageDepotRemoveShipReferences(ship, shiptoremove);
}

void P1MothershipDied(Ship *ship)
{
    cat_salvageDepotDied(ship);
}

void P1Mothership_Fix(Ship *ship)
{
    cat_salvageDepotFix(ship);
}

// CAT: the exe's header (P1MothershipHeader 0x008c5968: p1Mothership). Slot 6 is this file's own
//      P1MothershipAttackPassive (the body several headers share); slot 21: the exe folded the identical attackStraightForward(gunRange, tooCloseRange) body into MissileDestroyerAttack 0x005bb3e0.
//      Slot 16 is the Mothership's depot pre-fix.
CustShipHeader P1MothershipHeader =
{
    .sizeofShipSpecifics = sizeof(P1MothershipSpec),
    .CustShipStaticInit = P1MothershipStaticInit,
    .CustShipInit = P1MothershipInit,                        // CAT: new
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = P1MothershipAttackPassive,
    .CustShipHousekeep = P1MothershipHouseKeep,              // CAT: new
    .CustShipRemoveShipReferences = P1MothershipRemoveShipReferences, // CAT: new
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))P1MothershipRemoveShipReferences, // CAT: new
    .CustShipDied = P1MothershipDied,                        // CAT: new
    .CustShip_PreFix = Mothership_PreFix,                    // CAT: new
    .CustShip_Fix = P1Mothership_Fix,                        // CAT: new
    .CustShipAttack = P1MothershipAttack,
};

