// =============================================================================
//  Crystal.c
//  - Cataclysm's dCrystal0 / dCrystal1: resource crystals a salvage depot
//    dissolves into resource units (cataclysm.exe 0x005affa0-0x005b00d0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Each body follows the r6 decompile of its exe function (address on each).
//      cat_shipIsCrystal 0x005affa0 stays in ShipSelect.c, where the selection code needed it first.

#include "Crystal.h"

#include "Blast.h"
#include "Debug.h"
#include "ShipDisable.h"
#include "StatScript.h"
#include "Universe.h"

CrystalStatics CrystalStatic0;      // 0x00a762d0: dCrystal0
CrystalStatics CrystalStatic1;      // 0x00a762d4: dCrystal1

// 0x008c3840
scriptStructEntry CrystalScriptTable[] =
{
    { "resourceValue", scriptSetUdwordCB, &(CrystalStatic0.resourceValue), &(CrystalStatic0) },

    END_SCRIPT_STRUCT_ENTRY
};

void CrystalStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)       // 0x005affd0
{
    CrystalStatics *crystalstat = NULL;

    if (statinfo->shiptype == dCrystal0)
    {
        crystalstat = &CrystalStatic0;
    }
    else if (statinfo->shiptype == dCrystal1)
    {
        crystalstat = &CrystalStatic1;
    }
    dbgAssertOrIgnore(crystalstat != NULL);     // the exe writes through NULL for any other type
    crystalstat->resourceValue = 0;
    scriptSetStruct(directory,filename,CrystalScriptTable,(ubyte *)crystalstat);
    if (crystalstat->resourceValue < 1)
    {
        // the exe names the type (ShipTypeToStr) for an assert that compiles to nothing
        dbgAssertOrIgnore(FALSE);
    }
    statinfo->custstatinfo = crystalstat;
}

void CrystalInit(Ship *ship)                                                                // 0x005b0040
{
    CrystalSpec *spec = (CrystalSpec *)ship->ShipSpecifics;

    // a crystal is disabled for good (reason 12), not selectable, and friendly
    cat_shipSetDisableReason(ship, 1, 12);
    bitClear(ship->header.flags, SOF_Selectable);
    bitSet(ship->shipOrDerelict.specialFlags, SPECIAL_FriendlyStatus);
    spec->resourceValue = ((CrystalStatics *)ship->staticinfo->custstatinfo)->resourceValue;
    spec->noBlast = FALSE;
    spec->processing = FALSE;
}

void CrystalDied(Ship *ship)                                                                // 0x005b0090
{
    CrystalSpec *spec = (CrystalSpec *)ship->ShipSpecifics;

    if (!spec->noBlast)
    {
        cat_blastCreate(3, ship->obj.posinfo.position.x, ship->obj.posinfo.position.y, ship->obj.posinfo.position.z,
                        ship->catKilledByPlayer);
    }
}

// CAT: CrystalHeader 0x008c3860 (dCrystal0, dCrystal1)
CustShipHeader CrystalHeader =
{
    .sizeofShipSpecifics = sizeof(CrystalSpec),
    .CustShipStaticInit = CrystalStaticInit,
    .CustShipInit = CrystalInit,
    .CustShipDied = CrystalDied,
};
