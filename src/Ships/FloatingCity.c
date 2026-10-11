// =============================================================================
//  FloatingCity.c
//  - Bentusi Trader's Mothership
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 5/06/1998 by ddunlop
// =============================================================================

#include "FloatingCity.h"

#include "Attack.h"
#include "DefaultShip.h"

typedef struct
{
    udword dummy;
} FloatingCitySpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(FloatingCitySpec) == 4, "FloatingCitySpec is 4 bytes in cataclysm.exe (FloatingCityHeader sizeofShipSpecifics)");   // CAT
#endif

typedef struct
{
    real32 floatingcityGunRange[NUM_TACTICS_TYPES];
    real32 floatingcityTooCloseRange[NUM_TACTICS_TYPES];
} FloatingCityStatics;

FloatingCityStatics FloatingCityStatic;

void FloatingCityStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    udword i;
    FloatingCityStatics *floatingcitystat = &FloatingCityStatic;

    statinfo->custstatinfo = floatingcitystat;

    for(i=0;i<NUM_TACTICS_TYPES;i++)
    {
        floatingcitystat->floatingcityGunRange[i] = statinfo->bulletRange[i];
        floatingcitystat->floatingcityTooCloseRange[i] = statinfo->minBulletRange[i] * 0.9f;
    }
}

void FloatingCityAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    FloatingCityStatics *floatingcitystat = (FloatingCityStatics *)shipstaticinfo->custstatinfo;

    attackStraightForward(ship,target,floatingcitystat->floatingcityGunRange[ship->tacticstype],floatingcitystat->floatingcityTooCloseRange[ship->tacticstype]);
}

void FloatingCityAttackPassive(Ship *ship,Ship *target,bool32 rotate)
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

// CAT: the exe's header (FloatingCityHeader 0x008c4200: trFloatingCity; original/scratch-m2/m2r2-shipstatic-tools/headers.txt).
//      Slot 6: the exe folded the identical "rotate ? attackPassiveRotate : attackPassive" body into P1MothershipAttackPassive 0x005be540.
//      Slot 21: the exe folded the identical attackStraightForward(gunRange, tooCloseRange) body into MissileDestroyerAttack 0x005bb3e0.
CustShipHeader FloatingCityHeader =
{
    .sizeofShipSpecifics = sizeof(FloatingCitySpec),
    .CustShipStaticInit = FloatingCityStaticInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = FloatingCityAttackPassive,
    .CustShipAttack = FloatingCityAttack,
};

