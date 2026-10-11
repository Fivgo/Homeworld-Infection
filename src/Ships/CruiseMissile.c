// =============================================================================
//  CruiseMissile.c
//  - Cataclysm's bCruiseMissile: a Beast kamikaze missile ship
//    (cataclysm.exe 0x005afef0-0x005affa0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Each body follows the r6 decompile of its exe function (address on each). Its hit
//      is the kamikaze code (cat_kamikazeCheckShip, the Beast infection: Collision.c, BeastWeapon.c).

#include "CruiseMissile.h"

#include "AIShip.h"
#include "Attack.h"
#include "BeastWeapon.h"
#include "Collision.h"
#include "DefaultShip.h"

static udword CruiseMissileStatic;      // 0x00ab73a8: a 4-byte block nothing reads

#define CRUISEMISSILE_HOMING_RANGE      5000.0f     // 0x0072a618

void CruiseMissileStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)     // 0x005afef0
{
    (void)directory;
    (void)filename;
    statinfo->custstatinfo = &CruiseMissileStatic;
    statinfo->canSpecialBandBoxFriendlies = FALSE;
    statinfo->unknownFlag212 = TRUE;
    statinfo->unknownFlag213 = FALSE;
    cat_beastWeaponStaticInit();
}

void CruiseMissileAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)             // 0x005aff20
{
    vector trajectory;

    aishipGetTrajectory(ship, target, &trajectory);
    if (RangeToTarget(ship, target, &trajectory) > CRUISEMISSILE_HOMING_RANGE)
    {
        // far away it flies at the target's position, matching its velocity
        aishipFlyToPointAvoidingObjsFunc(ship, &target->obj.posinfo.position, 7, -400.0f,
                                         &target->obj.posinfo.velocity);
        return;
    }
    // close in it goes kamikaze (specialFlags 0x6: SPECIAL_Kamikaze | SPECIAL_BrokenFormation) and flies straight in
    bitSet(ship->shipOrDerelict.specialFlags, SPECIAL_Kamikaze | SPECIAL_BrokenFormation);
    attackStraightForward(ship, target, maxdist, 0.0f);
}

// CAT: CruiseMissileHeader 0x008c37b0 (bCruiseMissile); slots 3, 6 and 10 are the exe's nullsub
CustShipHeader CruiseMissileHeader =
{
    .sizeofShipSpecifics = sizeof(CruiseMissileSpec),
    .CustShipStaticInit = CruiseMissileStaticInit,
    .CustShipInit = nullsubShip,
    .CustShipAttackPassive = nullsubAttackPassive,
    .CustShipHousekeep = nullsubShip,
    .CustShipAttack = CruiseMissileAttack,
};
