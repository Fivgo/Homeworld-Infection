// =============================================================================
//  Carrier.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#include "Carrier.h"

#include "Attack.h"
#include "BeastWeapon.h"
#include "Debug.h"
#include "DefaultShip.h"
#include "MeshAnim.h"
#include "Mothership.h"
#include "Randy.h"
#include "RepairCorvette.h"
#include "Salvage.h"
#include "Select.h"
#include "ShipSelect.h"
#include "Universe.h"

// CAT: Carrier.c follows cataclysm.exe's Carrier block (0x005aefb0..0x005af460). The carriers are salvage depots
//      (Salvage.c) and bCarrier carries the Beast infection weapon (BeastWeapon.c); HW1's drop targets and the
//      captured-ship build queue are gone.

typedef struct
{
    real32 carrierGunRange[NUM_TACTICS_TYPES];
    real32 carrierTooCloseRange[NUM_TACTICS_TYPES];
    real32 repairApproachDistance;
} CarrierStatics;

CarrierStatics CarrierStatic;

// CAT: one block per race that has a carrier: Sect (0x00ab73e0), Beast (0x00ab73c0), Kushan (0x00ab7420),
//      Taiidan (0x00ab7400), Derelict (0x00ab7440: dKushanCarrier) (CarrierStaticInit 0x005aefb0); HW1 had R1/R2.
//      cat_carrierVariantStaticInit 0x005af4a0 (custstatinfo = 0x00ab73b0 for race 2, else 0x00ab73ac) has no caller
//      and no header slot in the exe (M2-R5 layout-dock: not ported)
CarrierStatics CarrierStaticSect;
CarrierStatics CarrierStaticBeast;
CarrierStatics CarrierStaticKushan;
CarrierStatics CarrierStaticTaiidan;
CarrierStatics CarrierStaticDerelict;
CarrierStatics CarrierStaticOther;      // CAT: the exe writes into the statinfo itself for any other race

scriptStructEntry CStaticScriptTable[] =
{
    { "repairApproachDistance",    scriptSetReal32CB,  &(CarrierStatic.repairApproachDistance),  &(CarrierStatic) },

    END_SCRIPT_STRUCT_ENTRY
};

void CarrierStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    udword i;
    CarrierStatics *carrstat;

    // CAT: CarrierStaticInit 0x005aefb0: three statinfo flags (+0x211 = 1 for the Beast carrier only), the block by
    //      Cataclysm race, then HW1's ranges and key table, then the Beast-weapon tweaks for bCarrier
    statinfo->canSpecialBandBoxFriendlies = FALSE;
    statinfo->unknownFlag213 = FALSE;
    statinfo->unknownFlag212 = TRUE;
    switch (statinfo->shiprace)
    {
        case RACE_Sect:     carrstat = &CarrierStaticSect;     break;
        case RACE_Beast:
            carrstat = &CarrierStaticBeast;
            statinfo->canSpecialBandBoxFriendlies = TRUE;
            break;
        case RACE_Kushan:   carrstat = &CarrierStaticKushan;   break;
        case RACE_Taiidan:  carrstat = &CarrierStaticTaiidan;  break;
        case RACE_Derelict: carrstat = &CarrierStaticDerelict; break;
        default:
            dbgAssertOrIgnore(FALSE);
            carrstat = &CarrierStaticOther;
            break;
    }

    statinfo->custstatinfo = carrstat;

    for(i=0;i<NUM_TACTICS_TYPES;i++)
    {
        carrstat->carrierGunRange[i] = statinfo->bulletRange[i];
        carrstat->carrierTooCloseRange[i] = statinfo->minBulletRange[i] * 0.9f;
    }
    scriptSetStruct(directory,filename,CStaticScriptTable,(ubyte *)carrstat);

    if (cat_staticHasBeastWeapon(statinfo))
    {
        cat_beastWeaponStaticInit();
    }
}

void CarrierInit(Ship *ship)
{
    CarrierSpec *spec = (CarrierSpec *)ship->ShipSpecifics;

    // CAT: CarrierInit 0x005af090
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponInit(ship);
    }
    spec->doorState = 1;
    spec->doorTimer = -1.0f;
    cat_salvageDepotInit(ship);
}

void CarrierAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    CarrierStatics *carrstat = (CarrierStatics *)shipstaticinfo->custstatinfo;

    attackStraightForward(ship,target,carrstat->carrierGunRange[ship->tacticstype],carrstat->carrierTooCloseRange[ship->tacticstype]);
}

void CarrierAttackPassive(Ship *ship,Ship *target,bool32 rotate)
{
    attackPassive(ship,target);
}

bool32 CarrierSpecialActivate(Ship *ship)
{
    // CAT: new slot-7 body: a bare "return 0" (the exe's nullsubReturnFalse 0x006fe9a0); the special-activate
    //      order does nothing on a carrier
    (void)ship;
    return FALSE;
}

bool32 CarrierSpecialTarget(Ship *ship,void *custom)
{
    CarrierStatics *cstat = (CarrierStatics *)ship->staticinfo->custstatinfo;
    SelectAnyCommand *targets;
    MaxAnySelection copy;

    // CAT: CarrierSpecialTarget 0x005af0f0: bCarrier aims its infection weapon at the targets if it has one among
    //      them (on a copy of the selection); otherwise the carriers repair, as HW1
    targets = (SelectAnyCommand *)custom;
    selSelectionCopy(&copy, (MaxAnySelection *)targets);
    if (cat_shipHasBeastWeapon(ship) && cat_beastWeaponHasTarget(ship, (SelectCommand *)&copy))
    {
        return cat_beastWeaponAttack(ship, &copy);
    }
    return(refuelRepairShips(ship, targets,cstat->repairApproachDistance));
}

void CarrierOrderChanged(Ship *ship)
{
    // CAT: new slot-9 body (CarrierOrderChanged 0x005af170)
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponOrderChanged(ship);
    }
}


void CarrierHouseKeep(Ship *ship)
{
    // CAT: CarrierHouseKeep 0x005af210. An sCarrier with a dock info animates its door: it closes (animation 0) once
    //      its open animation is over while a ship holds one of its DPT_Med_In / DPT_Med_Out (6 / 7) dock points,
    //      and reopens (animation 1) a random 5-10 s after the close animation is over and the points are free.
    CarrierSpec *spec = (CarrierSpec *)ship->ShipSpecifics;
    DockStaticInfo *dockstatic;
    sdword i;
    bool32 pointBusy = FALSE;

    // the exe reads the mesh animation without a test; this tree may load an sCarrier without one
    if ((ship->shiptype == sCarrier) && (ship->dockInfo != NULL) && (ship->madBindings != NULL) &&
        (ship->staticinfo->madStatic != NULL))
    {
        dockstatic = ship->staticinfo->dockStaticInfo;
        for (i = 0; i < dockstatic->numDockPoints; i++)
        {
            if (((dockstatic->dockstaticpoints[i].type == DPT_Med_In) || (dockstatic->dockstaticpoints[i].type == DPT_Med_Out)) &&
                (ship->dockInfo->thisDockBusy[i] != 0))
            {
                pointBusy = TRUE;
                break;
            }
        }
        if (pointBusy)
        {
            if ((spec->doorState == 1) && cat_madAnimationFinished(ship))
            {
                madAnimationStart(ship, 0, 0);   // CAT: the exe's third argument, 0 (M2-R6 nis-fow-camera)
                spec->doorState = 0;
            }
            spec->doorTimer = -1.0f;
        }
        else if (spec->doorState != 0)
        {
            spec->doorTimer = -1.0f;
        }
        else if (cat_madAnimationFinished(ship))
        {
            if (spec->doorTimer < 0.0f)
            {
                spec->doorTimer = (real32)ranRandom(RANDOM_GAME) * (5.0f / 4294967296.0f) + 5.0f + universe.totaltimeelapsed;
            }
            else if (universe.totaltimeelapsed >= spec->doorTimer)
            {
                madAnimationStart(ship, 1, 0);   // CAT: the exe's third argument, 0 (M2-R6 nis-fow-camera)
                spec->doorState = 1;
                spec->doorTimer = -1.0f;
            }
        }
    }

    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponHouseKeep(ship);
    }
    cat_salvageDepotHouseKeep(ship);
}

void CarrierRemoveShipReferences(Ship *ship,Ship *shiptoremove)
{
    // CAT: CarrierRemoveShipReferences 0x005af360 (also slot 12)
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponRemoveShipReferences(ship, shiptoremove);
    }
    cat_salvageDepotRemoveShipReferences(ship, shiptoremove);
}

void CarrierBulletRemoval(Ship *ship, struct Bullet *bullet)
{
    // CAT: new slot-14 body (CarrierBulletRemoval 0x005af390), shared with the HeavyCruiser and Mothership headers
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponBulletRemoval(ship, NULL, (Bullet *)bullet);     // CAT: the slot type names struct Bullet
    }
}

void Carrier_Fix(Ship *ship)
{
    // CAT: Carrier_Fix 0x005af3c0
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponFix(ship);
    }
    cat_salvageDepotFix(ship);
}

/*-----------------------------------------------------------------------------
    Name        : cat_carrierDoorAnimUpdate
    Description : CAT (M2-R4 ships-b): may a ship use an sCarrier dock point of state 6/7 (DPT_Med_In / DPT_Med_Out)?
                  Any other state may; for those the door must be open and its animation over. An open door whose
                  animation is over closes again (animation 0, door timer reset), and the ship must wait
                  (cat_carrierDoorAnimUpdate 0x005af190; called by cat_dockFlyOutOfCone, the dock purposes follow-up)
    Inputs      : ship - the carrier, dockstate - the dock point state
    Return      : TRUE when the ship may go on
----------------------------------------------------------------------------*/
bool32 cat_carrierDoorAnimUpdate(Ship *ship, sdword dockstate)
{
    CarrierSpec *spec = (CarrierSpec *)ship->ShipSpecifics;

    if ((dockstate != 6) && (dockstate != 7))
    {
        return TRUE;
    }
    if ((spec->doorState == 0) && cat_madAnimationFinished(ship))
    {
        return TRUE;
    }
    if ((spec->doorState == 1) && cat_madAnimationFinished(ship))
    {
        madAnimationStart(ship, 0, 0);   // CAT: the exe's third argument, 0 (M2-R6 nis-fow-camera)
        spec->doorState = 0;
        spec->doorTimer = -1.0f;
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_carrierCountSupportModules
    Description : CAT (M2-R4 ships-b): the finished sCarrierSupport modules clamped to ship (not SPECIAL_2_CatBeingBuilt)
                  (cat_carrierCountSupportModules 0x005af460; called by cmUpdateShipAvailable and unitCapCanCreateShip,
                  M2-R4 rest-oracle)
----------------------------------------------------------------------------*/
sdword cat_carrierCountSupportModules(Ship *ship)
{
    CatClampedList *clamped;
    sdword count = 0;
    sdword i;

    if ((ship->guidance.clampInfo == NULL) || ((clamped = ship->guidance.clampInfo->clampedObjs) == NULL))
    {
        return 0;
    }
    for (i = 0; i < clamped->numShips; i++)
    {
        if ((clamped->ShipPtr[i]->shiptype == sCarrierSupport) &&
            !bitTest(clamped->ShipPtr[i]->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt))
        {
            count++;
        }
    }
    return count;
}

void CarrierDied(Ship *ship)
{
    SelectCommand *clamped;
    sdword i;

    // CAT: CarrierDied 0x005af3f0: the weapon and the depot close; the component-class ships clamped to the carrier
    //      are flagged orphaned
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponClose(ship);
    }
    cat_salvageDepotDied(ship);
    if ((ship->guidance.clampInfo != NULL) && (ship->guidance.clampInfo->clampedObjs != NULL))
    {
        clamped = (SelectCommand *)ship->guidance.clampInfo->clampedObjs;
        for (i = 0; i < clamped->numShips; i++)
        {
            if (clamped->ShipPtr[i]->staticinfo->shipclass == CLASS_Component)
            {
                bitSet(clamped->ShipPtr[i]->header.catFlags18, CAT_SHIPFLAG_Invincible);
            }
        }
    }
}

// CAT: the exe's header (CarrierHeader 0x008c34d0: sCarrier, bCarrier, kCarrier, tCarrier, dKushanCarrier).
//      Slot 21: the exe folded the identical attackStraightForward(gunRange, tooCloseRange) body into
//      MissileDestroyerAttack 0x005bb3e0. Slots 16-18 are the Mothership depot/weapon save hooks.
CustShipHeader CarrierHeader =
{
    .sizeofShipSpecifics = sizeof(CarrierSpec),
    .CustShipStaticInit = CarrierStaticInit,
    .CustShipInit = CarrierInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = CarrierAttackPassive,
    .CustShipSpecialActivate = CarrierSpecialActivate,                      // CAT: new
    .CustShipSpecialTarget = CarrierSpecialTarget,
    .CustShipOrderChanged = CarrierOrderChanged,                            // CAT: new slot 9
    .CustShipHousekeep = CarrierHouseKeep,
    .CustShipRemoveShipReferences = CarrierRemoveShipReferences,
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))CarrierRemoveShipReferences,   // CAT: new slot 12
    .CustShipBulletRemoval = CarrierBulletRemoval,                          // CAT: new slot 14
    .CustShipDied = CarrierDied,
    .CustShip_PreFix = Mothership_PreFix,                                   // CAT: was Carrier_PreFix
    .CustShip_Save = Mothership_Save,                                       // CAT: new
    .CustShip_Load = Mothership_Load,                                       // CAT: new
    .CustShip_Fix = Carrier_Fix,
    .CustShipAttack = CarrierAttack,
};
