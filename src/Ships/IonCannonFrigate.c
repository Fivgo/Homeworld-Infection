// =============================================================================
//  IonCannonFrigate.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 11/5/1997 by khent
// =============================================================================

#include "IonCannonFrigate.h"

#include "Attack.h"
#include "Debug.h"
#include "DefaultShip.h"

typedef struct
{
    udword dummy;
} IonCannonFrigateSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(IonCannonFrigateSpec) == 4, "IonCannonFrigateSpec is 4 bytes in cataclysm.exe (IonCannonFrigateHeader sizeofShipSpecifics)");   // CAT
#endif

typedef struct
{
    real32 frigateGunRange[NUM_TACTICS_TYPES];
    real32 frigateTooCloseRange[NUM_TACTICS_TYPES];
} IonCannonFrigateStatics;

// CAT: one block per race that has the ship: Kushan (0x00a7ac80), Taiidan (0x00a7ac60), Derelict (0x00a7ac40:
//      dIonArrayFrigate, dIonCannonFrigate) (IonCannonFrigateStaticInit 0x005b6820); HW1 had R1/R2
IonCannonFrigateStatics IonCannonFrigateStaticKushan;
IonCannonFrigateStatics IonCannonFrigateStaticTaiidan;
IonCannonFrigateStatics IonCannonFrigateStaticDerelict;
IonCannonFrigateStatics IonCannonFrigateStaticOther;    // CAT: the exe writes into the statinfo itself for any other race

void IonCannonFrigateStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    udword i;
    IonCannonFrigateStatics *frigstat;

    // CAT: by Cataclysm race (IonCannonFrigateStaticInit 0x005b6820)
    switch (statinfo->shiprace)
    {
        case RACE_Kushan:   frigstat = &IonCannonFrigateStaticKushan;   break;
        case RACE_Taiidan:  frigstat = &IonCannonFrigateStaticTaiidan;  break;
        case RACE_Derelict: frigstat = &IonCannonFrigateStaticDerelict; break;
        default:
            dbgAssertOrIgnore(FALSE);
            frigstat = &IonCannonFrigateStaticOther;
            break;
    }

    statinfo->custstatinfo = frigstat;

    for(i=0;i<NUM_TACTICS_TYPES;i++)
    {
        frigstat->frigateGunRange[i] = statinfo->bulletRange[i]*0.725f;
        frigstat->frigateTooCloseRange[i] = statinfo->minBulletRange[i] * 0.5f;
    }
}

void IonCannonFrigateAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    IonCannonFrigateStatics *frigstat = (IonCannonFrigateStatics *)shipstaticinfo->custstatinfo;

    attackStraightForward(ship,target,frigstat->frigateGunRange[ship->tacticstype],frigstat->frigateTooCloseRange[ship->tacticstype]);
}

void IonCannonFrigateAttackPassive(Ship *ship,Ship *target,bool32 rotate)
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

// CAT: the exe's header (IonCannonFrigateHeader 0x008c4bd0: kIonCannonFrigate, tIonCannonFrigate, dIonArrayFrigate, dIonCannonFrigate; original/scratch-m2/m2r2-shipstatic-tools/headers.txt).
//      Slot 6: the exe folded the identical "rotate ? attackPassiveRotate : attackPassive" body into P1MothershipAttackPassive 0x005be540.
//      Slot 21: the exe folded the identical attackStraightForward(gunRange, tooCloseRange) body into MissileDestroyerAttack 0x005bb3e0.
CustShipHeader IonCannonFrigateHeader =
{
    .sizeofShipSpecifics = sizeof(IonCannonFrigateSpec),
    .CustShipStaticInit = IonCannonFrigateStaticInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = IonCannonFrigateAttackPassive,
    .CustShipAttack = IonCannonFrigateAttack,
};

