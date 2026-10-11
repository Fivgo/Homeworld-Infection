// =============================================================================
//  Salvage.h
//  - Cataclysm's salvage system: salvagers (Worker, SalCap corvettes,
//    p1StandardCorvette) that tow targets to salvage depots (carriers,
//    motherships, processors, bHeavyCruiser) (cataclysm.exe 0x004f7a90-0x004fd600)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___SALVAGE_H
#define ___SALVAGE_H

#include <stddef.h>

#include "Attack.h"
#include "SpaceObj.h"
#include "Types.h"

// CAT: new file (M2-R3 ships-a). The exe's salvage block replaces HW1's SalCapCorvette.c salvage code. This round
//      ports only what the reused HW1 ship headers need at load and init time: the static init, the per-ship blocks
//      and their initialisation. The salvage runtime (cat_salvageSpecialTarget 0x004faad0 and the 60-odd functions
//      around it) is M2-R4 #3 (combat and docking, "the salvage system"); until then its entry points below are
//      CAT-PROVISIONAL and do nothing, so a salvage order completes at once and nothing is ever towed.

// The salvager block, 0x28 bytes on ILP32, inside the salvager's specifics (cat_salvageGetSalvagerInfo 0x004f8ec0):
// sWorker/bWorker +0x0C (Ship+0x644), kSalCapCorvette/tSalCapCorvette +0x08 (Ship+0x640), p1StandardCorvette +0x13C
// (Ship+0x774). Its layout is M2-R4 #3's; this round only clears the byte at +0x24 (cat_salvageInit 0x004f9010).
typedef struct CatSalvagerInfo
{
    udword opaque[9];       // +0x00..+0x23 (M2-R4 #3)
    ubyte busy;             // +0x24 cleared by cat_salvageInit
    ubyte pad[3];
} CatSalvagerInfo;
_Static_assert(sizeof(CatSalvagerInfo) == 0x28, "the salvager block is 0x28 bytes in cataclysm.exe");

// The salvage depot block, 0x144 bytes on ILP32, inside the depot's specifics (cat_salvageDepotInit 0x004fc7c0):
// carriers +0x1C (Ship+0x654), processors +0x00 (Ship+0x638), motherships +0xAC (Ship+0x6E4), bHeavyCruiser +0x18
// (Ship+0x650). cat_salvageDepotInit clears 0x51 dwords and sets the byte at +0x140 to 0xFF; the rest is M2-R4 #3's.
typedef struct CatSalvageDepot
{
    udword opaque[0x50];    // +0x000..+0x13F (M2-R4 #3)
    ubyte slot;             // +0x140 0xFF after init
    ubyte pad[3];
} CatSalvageDepot;
_Static_assert(sizeof(CatSalvageDepot) == 0x144, "the salvage depot block is 0x144 bytes in cataclysm.exe");

// The salvager statics, 8 bytes (cat_salvageStaticInit 0x004f90e0): the "salvageFlyToDistance" key and the ship's
// "center" dock point (found by name; NULL if the ship has none)
typedef struct CatSalvageStatics
{
    real32 salvageFlyToDistance;
    DockStaticPoint *centerDockPoint;
} CatSalvageStatics;

// The statics of sWorker/bWorker and the SalCap corvettes (0x18 bytes on ILP32; SalCapCorvetteStaticInit 0x005c59c0
// clears 0x14 of them and reads AttackSideStepParametersScriptTable into the start): the side-step parameters, then
// the salvage statics at +0x0C (cat_salvageStaticInit 0x004f90e0)
typedef struct CatSalvagerStatics
{
    AttackSideStepParameters sidestepParameters;    // +0x00
    CatSalvageStatics salvage;                      // +0x0C
} CatSalvagerStatics;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(CatSalvagerStatics, salvage) == 0x0C, "the salvage statics are at +0x0C in cataclysm.exe");
_Static_assert(sizeof(CatSalvagerStatics) == 0x14, "0x14 bytes used of the 0x18-byte blocks in cataclysm.exe");
#endif

CatSalvagerInfo *cat_salvageGetSalvagerInfo(Ship *ship);
CatSalvageDepot *cat_salvageGetDepotInfo(Ship *ship);

void cat_salvageStaticInit(char *directory, char *filename, ShipStaticInfo *statinfo);
void cat_salvageInit(Ship *ship);
void cat_salvageDetach(Ship *salvager);
bool32 cat_salvageSpecialTarget(Ship *ship, void *custom);
void cat_salvageHouseKeep(Ship *ship);
void cat_salvageOrderChanged(Ship *ship);
void cat_salvageRemoveShipReferences(Ship *ship, Ship *shiptoremove);
void cat_salvageDied(Ship *ship);
void cat_salvage_PreFix(Ship *ship);
void cat_salvage_Fix(Ship *ship);

void cat_salvageDepotInit(Ship *ship);
void cat_salvageDepotHouseKeep(Ship *ship);
void cat_salvageDepotDied(Ship *ship);
void cat_salvageDepotRemoveShipReferences(Ship *ship, Ship *shiptoremove);
void cat_salvageDepot_PreFix(Ship *ship);
void cat_salvageDepotFix(Ship *ship);

#endif
