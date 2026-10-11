// =============================================================================
//  DefaultShip.c
//  - specifics for the DefaultShip (placeholder ship)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#include "DefaultShip.h"

#include "Attack.h"
#include "Gun.h"
#include "Universe.h"
#include "UnivUpdate.h"

typedef struct
{
    sdword dummy;
} DefaultShipSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(DefaultShipSpec) == 4, "DefaultShipSpec is 4 bytes in cataclysm.exe (DefaultShipHeader sizeofShipSpecifics)");   // CAT
#endif

typedef struct
{
    udword dummy;
} DefaultShipStatics;

typedef struct
{
    bool32 dontRecurseKill;
} MiningBaseSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(MiningBaseSpec) == 4, "MiningBaseSpec is 4 bytes in cataclysm.exe (MiningBaseHeader sizeofShipSpecifics)");   // CAT
#endif

void DefaultShipAttack(Ship *ship, SpaceObjRotImpTarg *target, real32 maxdist);

void DefaultShipFire(Ship *ship,SpaceObjRotImpTarg *target)
{
    GunInfo *guninfo = ship->gunInfo;
    sdword numGuns;
    sdword i;
    Gun *gun;

    if (guninfo == NULL)
    {
        return;
    }

    numGuns = guninfo->numGuns;

    // CAT: cat_gunCanFire (second fire mode, dissolving ships) in place of gunCanShoot, and fixed guns fire too
    //      (HW1 asserted on them) (DefaultShipFire 0x005b0260)
    for (i=0;i<numGuns;i++)
    {
        gun = &guninfo->guns[i];
        if (cat_gunCanFire(ship, gun))
        {
            switch (gun->gunstatic->guntype)
            {
                case GUN_Fixed:
                case GUN_Gimble:
                case GUN_NewGimble:
                    gunShoot(ship,gun,target);
                    break;

                case GUN_MissileLauncher:
                case GUN_MineLauncher:
                    if (gunHasMissiles(gun))
                    {
                        missileShoot(ship,gun,target);
                    }
                    break;

                default:
                    break;
            }
        }
    }
}

void DefaultShipAttack(Ship *ship,SpaceObjRotImpTarg *target, real32 maxdist)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;

    if (ship->gunInfo == NULL)
    {
        dbgMessagef("WARNING: %s tried to attack but has no guns",ShipTypeToStr(ship->shiptype));
        return;
    }

    attackStraightForward(ship,target,shipstaticinfo->bulletRange[ship->tacticstype],shipstaticinfo->minBulletRange[ship->tacticstype]*0.9f);
}

void DefaultShipAttackPassive(Ship *ship,Ship *target,bool32 rotate)
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

// CAT: no DefaultShipAttackStationary (StationaryGunHeader 0x008c3b60 has no slot 21)

void MiningBaseInit(Ship *ship)
{
    MiningBaseSpec *spec = (MiningBaseSpec *)ship->ShipSpecifics;

    spec->dontRecurseKill = FALSE;
}

void MiningBaseDied(Ship *ship)
{
    Ship *deleteme;
    Node *node;
    MiningBaseSpec *spec = (MiningBaseSpec *)ship->ShipSpecifics;

    // CAT: type trMiningBase (100) throughout, no type assertion (MiningBaseDied 0x005b0370)
    if (ship->header.flags & SOF_NISShip)
    {
        return;
    }

    if (spec->dontRecurseKill)
    {
        return;
    }

    // first pass: Set all other MiningBases to dontRecurseKill so we don't get weird recursion

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        deleteme = (Ship *)listGetStructOfNode(node);
        dbgAssertOrIgnore(deleteme->header.objtype == OBJ_ShipType);

        if ((deleteme->shiptype == trMiningBase) && (deleteme != ship) && ((deleteme->header.flags & (SOF_Dead|SOF_NISShip)) == 0))
        {
            spec = (MiningBaseSpec *)deleteme->ShipSpecifics;
            spec->dontRecurseKill = TRUE;
        }
    }

    // delete all other mining bases

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        deleteme = (Ship *)listGetStructOfNode(node);
        dbgAssertOrIgnore(deleteme->header.objtype == OBJ_ShipType);

        if ((deleteme->shiptype == trMiningBase) && (deleteme != ship) && ((deleteme->header.flags & (SOF_Dead|SOF_NISShip)) == 0))
        {
            deleteme->howDidIDie = ship->howDidIDie;
            deleteme->whoKilledMe = ship->whoKilledMe;
            univDeleteDeadShip(deleteme,EDT_AccumDamage);
            // CAT: no list removals and no ETG_DISABLEABLE wipe here (MiningBaseDied 0x005b0370): the other mining
            //      bases stay in the ship and impactable lists until their death effect wipes them
            //      (univWipeShipOutOfExistence), as every other dead ship does
        }
    }
}

// CAT: the exe's headers (shipHeaders 0x008b1938). Slot 6 of DefaultShipHeader and StationaryGunHeader is the
//      "rotate ? attackPassiveRotate : attackPassive" body that the linker folded into P1MothershipAttackPassive
//      0x005be540; DefaultShipAttackPassive is the same code.
CustShipHeader DefaultShipHeader =                  // 0x008c39b0: trCargoBarge, trResearchStation (+ the 22 Cataclysm-only
{                                                   // headers and the two cloak generators until M2-R4 #4)
    .sizeofShipSpecifics = sizeof(DefaultShipSpec),
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = DefaultShipAttackPassive,
    .CustShipAttack = DefaultShipAttack,
};

CustShipHeader DoNothingShipHeader =                // 0x008c3a40
{
    .sizeofShipSpecifics = sizeof(DefaultShipSpec),
};

CustShipHeader MiningBaseHeader =                   // 0x008c3ad0: trMiningBase
{
    .sizeofShipSpecifics = sizeof(MiningBaseSpec),
    .CustShipInit = MiningBaseInit,
    .CustShipDied = MiningBaseDied,
};

CustShipHeader StationaryGunHeader =                // 0x008c3b60: trJunk_LGun, trJunk_SGun
{
    .sizeofShipSpecifics = sizeof(DefaultShipSpec),
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = DefaultShipAttackPassive,
    // CAT: no slot 21 (HW1: DefaultShipAttackStationary); the junkyard guns only fire passively
};
