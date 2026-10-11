// =============================================================================
//  StandardFrigate.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#include "StandardFrigate.h"

#include "Attack.h"
#include "DefaultShip.h"

typedef struct
{
    udword dummy;
} StandardFrigateSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(StandardFrigateSpec) == 4, "StandardFrigateSpec is 4 bytes in cataclysm.exe (StandardFrigateHeader sizeofShipSpecifics)");   // CAT
#endif

typedef struct
{
    real32 frigateGunRange[NUM_TACTICS_TYPES];
    real32 frigateTooCloseRange[NUM_TACTICS_TYPES];
} StandardFrigateStatics;

// CAT: Kushan (0x00a7a4e0) and every other race (0x00a7a500, i.e. Taiidan: tStandardFrigate)
//      (StandardFrigateStaticInit 0x005c78e0); HW1 had R1/R2
StandardFrigateStatics StandardFrigateStaticKushan;
StandardFrigateStatics StandardFrigateStaticTaiidan;

void StandardFrigateStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    udword i;
    // CAT: Kushan, else the Taiidan block (StandardFrigateStaticInit 0x005c78e0)
    StandardFrigateStatics *frigstat = (statinfo->shiprace == RACE_Kushan) ? &StandardFrigateStaticKushan : &StandardFrigateStaticTaiidan;

    statinfo->custstatinfo = frigstat;

    for(i=0;i<NUM_TACTICS_TYPES;i++)
    {
        frigstat->frigateGunRange[i] = statinfo->bulletRange[i];
        frigstat->frigateTooCloseRange[i] = statinfo->minBulletRange[i] * 0.9f;
    }
}

void StandardFrigateAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    StandardFrigateStatics *frigstat = (StandardFrigateStatics *)shipstaticinfo->custstatinfo;

    attackStraightForward(ship,target,frigstat->frigateGunRange[ship->tacticstype],frigstat->frigateTooCloseRange[ship->tacticstype]);
}

void StandardFrigateAttackPassive(Ship *ship,Ship *target,bool32 rotate)
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

// CAT: the exe's header (StandardFrigateHeader 0x008c6a20: tStandardFrigate; original/scratch-m2/m2r2-shipstatic-tools/headers.txt).
//      Slot 6: the exe folded the identical "rotate ? attackPassiveRotate : attackPassive" body into P1MothershipAttackPassive 0x005be540.
//      Slot 21: the exe folded the identical attackStraightForward(gunRange, tooCloseRange) body into MissileDestroyerAttack 0x005bb3e0.
CustShipHeader StandardFrigateHeader =
{
    .sizeofShipSpecifics = sizeof(StandardFrigateSpec),
    .CustShipStaticInit = StandardFrigateStaticInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = StandardFrigateAttackPassive,
    .CustShipAttack = StandardFrigateAttack,
};

