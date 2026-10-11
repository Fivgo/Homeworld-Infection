// =============================================================================
//  Salvage.c
//  - Cataclysm's salvage system: salvagers (Worker, SalCap corvettes,
//    p1StandardCorvette) that tow targets to salvage depots (carriers,
//    motherships, processors, bHeavyCruiser) (cataclysm.exe 0x004f7a90-0x004fd600)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R3 ships-a); see Salvage.h for what is ported now and what is M2-R4 #3's.

#include "Salvage.h"

#include <string.h>
#ifndef _MSC_VER
    #include <strings.h>
#endif

#include "Carrier.h"
#include "Debug.h"
#include "Dock.h"
#include "HeavyCruiser.h"
#include "Mothership.h"
#include "P1Mothership.h"
#include "P1StandardCorvette.h"
#include "SalCapCorvette.h"
#include "StatScript.h"

#ifdef _MSC_VER
    #define strcasecmp _stricmp
#endif

// the statics of salvagers without their own block, and their key table (cat_salvageStaticInit 0x004f90e0: the
// block 0x00a57768, the table 0x008922d8 whose only key is salvageFlyToDistance)
static CatSalvageStatics cat_salvageDefaultStatics;
static CatSalvagerInfo cat_salvageDefaultSalvagerInfo;      // 0x00a57770, for any other type
static CatSalvageDepot cat_salvageDefaultDepot;             // 0x00a577a0, for any other type

scriptStructEntry cat_salvageStaticScriptTable[] =
{
    { "salvageFlyToDistance", scriptSetReal32CB, &(cat_salvageDefaultStatics.salvageFlyToDistance), &(cat_salvageDefaultStatics) },

    END_SCRIPT_STRUCT_ENTRY
};

static DockStaticPoint *cat_salvageFindDockStaticPoint(char *name, DockStaticInfo *dockstaticinfo)
{
    // CAT: cat_dockFindDockStaticPoint 0x00463d00 (Dock.c, M2-R3 universe-mex); this wrapper only adds a NULL test
    //      for a type without dock points (the exe reads the pointer unchecked; every salvager has dock points)
    if (dockstaticinfo == NULL)
    {
        return NULL;
    }
    return cat_dockFindDockStaticPoint(name, dockstaticinfo);
}

CatSalvagerInfo *cat_salvageGetSalvagerInfo(Ship *ship)                     // 0x004f8ec0
{
    ubyte *specifics = (ubyte *)ship->ShipSpecifics;

    switch (ship->shiptype)
    {
        case sWorker:
        case bWorker:
            // CAT-PROVISIONAL: the exe's +0x0C of the Worker specifics (WorkerHeader, M2-R4 #4 gives it a type)
            return (CatSalvagerInfo *)(specifics + 0x0C);
        case kSalCapCorvette:
        case tSalCapCorvette:
            return (CatSalvagerInfo *)(specifics + offsetof(CatSalCapCorvetteSpec, salvager));     // exe +0x08
        case p1StandardCorvette:
            return (CatSalvagerInfo *)(specifics + offsetof(P1StandardCorvetteSpec, salvager));    // exe +0x13C
        default:
            return &cat_salvageDefaultSalvagerInfo;
    }
}

CatSalvageDepot *cat_salvageGetDepotInfo(Ship *ship)                        // the switch of cat_salvageDepotInit
{
    ubyte *specifics = (ubyte *)ship->ShipSpecifics;

    switch (ship->shiptype)
    {
        case sCarrier:
        case bCarrier:
        case kCarrier:
        case tCarrier:
        case dKushanCarrier:
            return (CatSalvageDepot *)(specifics + offsetof(CarrierSpec, depot));          // exe +0x1C
        case sProcessor:
        case bProcessor:
            // CAT-PROVISIONAL: the exe's +0x00 of the Processor specifics (ProcessorHeader, M2-R4 #4)
            return (CatSalvageDepot *)(specifics + 0x00);
        case p1Mothership:
            return (CatSalvageDepot *)(specifics + offsetof(P1MothershipSpec, depot));     // exe +0x00
        case sMothership:
        case bMothership:
        case trGhostship:
            return (CatSalvageDepot *)(specifics + offsetof(MothershipSpec, depot));       // exe +0xAC
        case bHeavyCruiser:
            return (CatSalvageDepot *)(specifics + offsetof(HeavyCruiserSpec, depot));     // exe +0x18
        default:
            return &cat_salvageDefaultDepot;
    }
}

void cat_salvageStaticInit(char *directory, char *filename, ShipStaticInfo *statinfo)    // 0x004f90e0
{
    CatSalvageStatics *salstat;

    // sWorker/bWorker and the SalCap corvettes keep the salvage statics at +0x0C of their own statics, p1StandardCorvette
    // at +0x00; any other type shares one block
    switch (statinfo->shiptype)
    {
        case sWorker:
        case bWorker:
        case kSalCapCorvette:
        case tSalCapCorvette:
            salstat = &((CatSalvagerStatics *)statinfo->custstatinfo)->salvage;
            break;
        case p1StandardCorvette:
            salstat = &((P1StandardCorvetteStatics *)statinfo->custstatinfo)->salvage;
            break;
        default:
            salstat = &cat_salvageDefaultStatics;
            break;
    }
    salstat->salvageFlyToDistance = 0.0f;
    salstat->centerDockPoint = NULL;
    scriptSetStruct(directory, filename, cat_salvageStaticScriptTable, salstat);
    salstat->centerDockPoint = cat_salvageFindDockStaticPoint("center", statinfo->dockStaticInfo);
}

void cat_salvageInit(Ship *ship)                                            // 0x004f9010
{
    // CAT: the exe's specialFlags bit 0x80000 marks a salvager (SPECIAL_CatSalvager, SpaceObj.h)
    bitSet(ship->shipOrDerelict.specialFlags, SPECIAL_CatSalvager);
    cat_salvageDetach(ship);
    cat_salvageGetSalvagerInfo(ship)->busy = 0;
}

void cat_salvageDetach(Ship *salvager)                                      // 0x004f88b0
{
    // CAT-PROVISIONAL: releases the towed target, the beam effect and the clamp (1,041 bytes in the exe); nothing is
    //      ever attached until the runtime is ported (M2-R4 #3)
    (void)salvager;
}

bool32 cat_salvageSpecialTarget(Ship *ship, void *custom)                  // 0x004faad0
{
    // CAT-PROVISIONAL: M2-R4 #3; the salvage order is reported done at once
    (void)ship;
    (void)custom;
    return TRUE;
}

void cat_salvageHouseKeep(Ship *ship)                                       // 0x004fbeb0
{
    (void)ship;     // CAT-PROVISIONAL: M2-R4 #3
}

void cat_salvageOrderChanged(Ship *ship)                                    // 0x004fbde0
{
    (void)ship;     // CAT-PROVISIONAL: M2-R4 #3 (drops the current salvage target)
}

void cat_salvageRemoveShipReferences(Ship *ship, Ship *shiptoremove)       // 0x004fc1a0
{
    (void)ship;     // CAT-PROVISIONAL: M2-R4 #3
    (void)shiptoremove;
}

void cat_salvageDied(Ship *ship)                                            // 0x004fc190
{
    // the exe's body is cat_salvageDetach(ship)
    cat_salvageDetach(ship);
}

void cat_salvage_PreFix(Ship *ship)                                         // 0x004fc460
{
    (void)ship;     // CAT-PROVISIONAL: M2-R4 #3 with the save format
}

void cat_salvage_Fix(Ship *ship)                                            // 0x004fc550
{
    (void)ship;     // CAT-PROVISIONAL: M2-R4 #3 with the save format
}

void cat_salvageDepotInit(Ship *ship)                                       // 0x004fc7c0
{
    CatSalvageDepot *depot = cat_salvageGetDepotInfo(ship);

    bitSet(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatSalvageDepot);
    memset(depot, 0, sizeof(*depot));
    depot->slot = 0xFF;
}

void cat_salvageDepotHouseKeep(Ship *ship)                                  // 0x004fc9d0
{
    (void)ship;     // CAT-PROVISIONAL: M2-R4 #3 (processes what the salvagers deliver)
}

void cat_salvageDepotDied(Ship *ship)                                       // 0x004fcc30
{
    (void)ship;     // CAT-PROVISIONAL: M2-R4 #3 (releases the salvagers docked at the depot)
}

void cat_salvageDepotRemoveShipReferences(Ship *ship, Ship *shiptoremove)  // 0x004fcf60
{
    (void)ship;     // CAT-PROVISIONAL: M2-R4 #3
    (void)shiptoremove;
}

void cat_salvageDepot_PreFix(Ship *ship)                                    // 0x004fd0e0
{
    (void)ship;     // CAT-PROVISIONAL: M2-R4 #3 with the save format
}

void cat_salvageDepotFix(Ship *ship)                                        // 0x004fd1e0
{
    (void)ship;     // CAT-PROVISIONAL: M2-R4 #3 with the save format
}
