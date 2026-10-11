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
//      Taiidan (0x00ab7400), Derelict (0x00ab7440: dKushanCarrier) (CarrierStaticInit 0x005aefb0); HW1 had R1/R2
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

/*-----------------------------------------------------------------------------
    Name        : cat_carrierDoorAnimate
    Description : CAT: starts sCarrier's door animation by number (0 closes, 1 opens; CarrierHouseKeep 0x005af210
                  calls madAnimationStart(ship, number, 0))
----------------------------------------------------------------------------*/
static void cat_carrierDoorAnimate(Ship *ship,sdword number)
{
    madstatic *mstatic = ship->staticinfo->madStatic;
    sdword index = number;

    // CAT-TODO(stage 3): the exe's madAnimationStart 0x004b6b90 looks the number up in madstatic.animIndexes itself
    //      and takes a third argument (0 here); this tree's takes the header index, so the lookup is done here
    if (mstatic->animsByNumber)
    {
        index = (number < mstatic->numAnims) ? mstatic->animIndexes[number] : -1;
    }
    if ((index >= 0) && (index < mstatic->header->nAnimations))
    {
        madAnimationStart(ship, index);
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
                (ship->dockInfo->dockpoints[i].thisDockBusy != 0))
            {
                pointBusy = TRUE;
                break;
            }
        }
        if (pointBusy)
        {
            if ((spec->doorState == 1) && cat_madAnimationFinished(ship))
            {
                cat_carrierDoorAnimate(ship, 0);
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
                cat_carrierDoorAnimate(ship, 1);
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
                bitSet(clamped->ShipPtr[i]->catShipFlags, CAT_SHIPFLAG_Orphaned);
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
