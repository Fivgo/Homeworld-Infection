// =============================================================================
//  SalCapCorvette.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 11/5/1997 by khent
// =============================================================================

#include "SalCapCorvette.h"

#include <string.h>

#include "Attack.h"
#include "Debug.h"
#include "Salvage.h"
#include "StatScript.h"

/*-----------------------------------------------------------------------------
    CAT: cataclysm.exe's SalCapCorvette block (0x005c59c0..0x005c5af0) is the header's slot bodies only, each a
    wrapper around the salvage system (Salvage.c). HW1's salvage code that was here (the HW1 salvage states, tractor
    beam, target hand-off, drop into carriers/motherships, technology stripping, docking-light flying, the
    RND_VISUALIZATION salvage-point lines and the order clean-up the command layer called) has no counterpart in
    the exe and is gone (M2-R4 combat), with its readers in the other files.
-----------------------------------------------------------------------------*/

// CAT: one block per race (SalCapCorvetteStaticInit 0x005c59c0): Kushan 0x00a77b50, Taiidan 0x00a77b68, Traders
//      0x00a77b80, 0x18 bytes apart: the side-step parameters and the salvage statics (Salvage.h CatSalvagerStatics)
CatSalvagerStatics SalCapCorvetteStaticKushan;
CatSalvagerStatics SalCapCorvetteStaticTaiidan;
CatSalvagerStatics SalCapCorvetteStaticTraders;
CatSalvagerStatics SalCapCorvetteStaticOther;   // CAT: the exe writes into the statinfo itself for any other race

void SalCapCorvetteStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    CatSalvagerStatics *corvstat;

    switch (statinfo->shiprace)
    {
        case RACE_Kushan:  corvstat = &SalCapCorvetteStaticKushan;  break;
        case RACE_Taiidan: corvstat = &SalCapCorvetteStaticTaiidan; break;
        case RACE_Traders: corvstat = &SalCapCorvetteStaticTraders; break;
        default:
            dbgAssertOrIgnore(FALSE);
            corvstat = &SalCapCorvetteStaticOther;
            break;
    }
    // the exe clears 0x14 bytes, reads the side-step keys, then the salvage statics
    memset(corvstat, 0, sizeof(*corvstat));
    scriptSetStruct(directory,filename,AttackSideStepParametersScriptTable,(ubyte *)&corvstat->sidestepParameters);
    statinfo->custstatinfo = corvstat;
    cat_salvageStaticInit(directory, filename, statinfo);
}

void SalCapCorvetteInit(Ship *ship)                                         // 0x005c5a40
{
    CatSalCapCorvetteSpec *spec = (CatSalCapCorvetteSpec *)ship->ShipSpecifics;

    attackSideStepInit(&spec->attacksidestep);
    cat_salvageInit(ship);
}

bool32 SalCapCorvetteSpecialTarget(Ship *ship, void *custom)               // 0x005c5a60
{
    return cat_salvageSpecialTarget(ship, custom);
}

void SalCapCorvetteOrderChanged(Ship *ship)                                 // CAT: new slot 9 (0x005c5a80)
{
    cat_salvageOrderChanged(ship);
}

void SalCapCorvetteHouseKeep(Ship *ship)                                    // 0x005c5a90
{
    cat_salvageHouseKeep(ship);
}

void SalCapRemoveShipReferences(Ship *ship, Ship *shiptoremove)            // 0x005c5aa0 (also slot 12)
{
    cat_salvageRemoveShipReferences(ship, shiptoremove);
}

void SalCapDied(Ship *ship)                                                 // 0x005c5ac0
{
    cat_salvageDied(ship);
}

void SalCapCorvette_PreFix(Ship *ship)                                      // 0x005c5ad0
{
    cat_salvage_PreFix(ship);
}

void SalCapCorvette_Fix(Ship *ship)                                         // 0x005c5ae0
{
    cat_salvage_Fix(ship);
}

// CAT: the exe's header (SalCapCorvetteHeader 0x008c6698: kSalCapCorvette, tSalCapCorvette). The specifics are
//      the exe's 0x30 bytes (CatSalCapCorvetteSpec, asserted on ILP32 in SalCapCorvette.h; M2-R4 combat removed
//      HW1's SalCapCorvetteSpec and its readers). No JunkYardDawgHeader: trJunkYardDawg is on DoNothingShipHeader.
CustShipHeader SalCapCorvetteHeader =
{
    .sizeofShipSpecifics = sizeof(CatSalCapCorvetteSpec),
    .CustShipStaticInit = SalCapCorvetteStaticInit,
    .CustShipInit = SalCapCorvetteInit,
    .CustShipSpecialTarget = SalCapCorvetteSpecialTarget,
    .CustShipOrderChanged = SalCapCorvetteOrderChanged,                         // CAT: new slot 9
    .CustShipHousekeep = SalCapCorvetteHouseKeep,
    .CustShipRemoveShipReferences = SalCapRemoveShipReferences,
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))SalCapRemoveShipReferences,   // CAT: new slot 12
    .CustShipDied = SalCapDied,
    .CustShip_PreFix = SalCapCorvette_PreFix,
    .CustShip_Fix = SalCapCorvette_Fix,
};
