// =============================================================================
//  HeavyCruiser.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#include "HeavyCruiser.h"

#include "Attack.h"
#include "BeastWeapon.h"
#include "Carrier.h"
#include "Debug.h"
#include "DefaultShip.h"
#include "Mothership.h"
#include "Salvage.h"

typedef struct
{
    real32 heavycruiserGunRange[NUM_TACTICS_TYPES];
    real32 heavycruiserTooCloseRange[NUM_TACTICS_TYPES];
} HeavyCruiserStatics;

// CAT: one block per race that has the ship: Beast (0x00a7ad40), Kushan (0x00a7ad20), Taiidan (0x00a7ad60)
//      (HeavyCruiserStaticInit 0x005b5310); HW1 had R1/R2
HeavyCruiserStatics HeavyCruiserStaticBeast;
HeavyCruiserStatics HeavyCruiserStaticKushan;
HeavyCruiserStatics HeavyCruiserStaticTaiidan;
HeavyCruiserStatics HeavyCruiserStaticOther;    // CAT: the exe writes into the statinfo itself for any other race

void HeavyCruiserStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    udword i;
    HeavyCruiserStatics *heavycruiserstat;

    // CAT: by Cataclysm race (HeavyCruiserStaticInit 0x005b5310)
    switch (statinfo->shiprace)
    {
        case RACE_Beast:   heavycruiserstat = &HeavyCruiserStaticBeast;   break;
        case RACE_Kushan:  heavycruiserstat = &HeavyCruiserStaticKushan;  break;
        case RACE_Taiidan: heavycruiserstat = &HeavyCruiserStaticTaiidan; break;
        default:
            dbgAssertOrIgnore(FALSE);
            heavycruiserstat = &HeavyCruiserStaticOther;
            break;
    }

    statinfo->custstatinfo = heavycruiserstat;

    for(i=0;i<NUM_TACTICS_TYPES;i++)
    {
        heavycruiserstat->heavycruiserGunRange[i] = statinfo->bulletRange[i];
        heavycruiserstat->heavycruiserTooCloseRange[i] = statinfo->minBulletRange[i] * 0.9f;
    }

    // CAT: bHeavyCruiser carries the Beast infection weapon and loads its tweaks (0x005b5310)
    if (cat_staticHasBeastWeapon(statinfo))
    {
        cat_beastWeaponStaticInit();
    }
}

void HeavyCruiserAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    HeavyCruiserStatics *heavycruiserstat = (HeavyCruiserStatics *)shipstaticinfo->custstatinfo;

    attackStraightForward(ship,target,heavycruiserstat->heavycruiserGunRange[ship->tacticstype],heavycruiserstat->heavycruiserTooCloseRange[ship->tacticstype]);
}

void HeavyCruiserAttackPassive(Ship *ship,Ship *target,bool32 rotate)
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

// CAT: the Cataclysm-only slot bodies of the heavy cruiser header (HeavyCruiserInit 0x005b5390 .. HeavyCruiser_Fix
//      0x005b5480): bHeavyCruiser (type 0x2E) is a salvage depot and carries the Beast infection weapon
void HeavyCruiserInit(Ship *ship)
{
    if (ship->shiptype == bHeavyCruiser)
    {
        cat_salvageDepotInit(ship);
    }
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponInit(ship);
    }
}

void HeavyCruiserHouseKeep(Ship *ship)
{
    if (ship->shiptype == bHeavyCruiser)
    {
        cat_salvageDepotHouseKeep(ship);
    }
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponHouseKeep(ship);
    }
}

void HeavyCruiserRemoveShipReferences(Ship *ship, Ship *shiptoremove)
{
    if (ship->shiptype == bHeavyCruiser)
    {
        cat_salvageDepotRemoveShipReferences(ship, shiptoremove);
    }
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponRemoveShipReferences(ship, shiptoremove);
    }
}

void HeavyCruiserDied(Ship *ship)
{
    if (ship->shiptype == bHeavyCruiser)
    {
        cat_salvageDepotDied(ship);
    }
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponClose(ship);
    }
}

void HeavyCruiser_PreFix(Ship *ship)
{
    if (ship->shiptype == bHeavyCruiser)
    {
        cat_salvageDepot_PreFix(ship);
    }
}

void HeavyCruiser_Fix(Ship *ship)
{
    if (ship->shiptype == bHeavyCruiser)
    {
        cat_salvageDepotFix(ship);
    }
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponFix(ship);
    }
}

// CAT: the exe's header (HeavyCruiserHeader 0x008c4ab0: bHeavyCruiser, kHeavyCruiser, tHeavyCruiser).
//      Slot 6: the exe folded the identical "rotate ? attackPassiveRotate : attackPassive" body into P1MothershipAttackPassive 0x005be540.
//      Slot 21: the exe folded the identical attackStraightForward(gunRange, tooCloseRange) body into MissileDestroyerAttack 0x005bb3e0.
//      Slots 8/9/17/18 are the Mothership's Beast-weapon bodies and slot 14 the Carrier's.
CustShipHeader HeavyCruiserHeader =
{
    .sizeofShipSpecifics = sizeof(HeavyCruiserSpec),
    .CustShipStaticInit = HeavyCruiserStaticInit,
    .CustShipInit = HeavyCruiserInit,                        // CAT: new
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = HeavyCruiserAttackPassive,
    .CustShipSpecialTarget = MothershipSpecialTarget,        // CAT: new
    .CustShipOrderChanged = MothershipOrderChanged,          // CAT: new
    .CustShipHousekeep = HeavyCruiserHouseKeep,              // CAT: new
    .CustShipRemoveShipReferences = HeavyCruiserRemoveShipReferences, // CAT: new
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))HeavyCruiserRemoveShipReferences, // CAT: new
    .CustShipBulletRemoval = CarrierBulletRemoval,           // CAT: new
    .CustShipDied = HeavyCruiserDied,                        // CAT: new
    .CustShip_PreFix = HeavyCruiser_PreFix,                  // CAT: new
    .CustShip_Save = Mothership_Save,                        // CAT: new
    .CustShip_Load = Mothership_Load,                        // CAT: new
    .CustShip_Fix = HeavyCruiser_Fix,                        // CAT: new
    .CustShipAttack = HeavyCruiserAttack,
};

