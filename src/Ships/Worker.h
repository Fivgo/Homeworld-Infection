// =============================================================================
//  Worker.h
//  - Cataclysm's sWorker / bWorker: harvests resources with its jack, repairs
//    friendly ships and salvages (cataclysm.exe 0x005c7ac0-0x005c9bf0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___WORKER_H
#define ___WORKER_H

#include <stddef.h>

#include "Salvage.h"
#include "SpaceObj.h"

// CAT: new file (M2-R4 ships-b). 0x48 bytes in cataclysm.exe (WorkerHeader 0x008c6b50 sizeofShipSpecifics 72).
//      Field names chosen here from their users.
typedef struct
{
    ubyte unknown00;                    // +0x00 (Ship+0x638) cleared by WorkerInit, no other user found
    ubyte unknown01;                    // +0x01 (Ship+0x639) cleared by WorkerInit, no other user found
    ubyte pad02[2];
    real32 jackRetractTime;             // +0x04 (Ship+0x63C) when the jack folds back (animation 1), 1e21 = never
    ubyte specialMode;                  // +0x08 (Ship+0x640) WORKER_MODE_* below (Salvage.c reads it too)
    ubyte pad09[3];
    CatSalvagerInfo salvager;           // +0x0C (Ship+0x644) cat_salvageGetSalvagerInfo
    sdword repairState;                 // +0x34 (Ship+0x66C) WORKER_REPAIR_* below
    struct Ship *repairTarget;          // +0x38 (Ship+0x670)
    struct Effect *repairEffect;        // +0x3C (Ship+0x674) the repair beam
    bool32 repairEffectOn;              // +0x40 (Ship+0x678)
    real32 repairEffectDist;            // +0x44 (Ship+0x67C) the beam's length when it started
} WorkerSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(WorkerSpec, specialMode) == 0x08, "Worker mode byte at Ship+0x640 in cataclysm.exe");
_Static_assert(offsetof(WorkerSpec, salvager) == 0x0C, "Worker salvager block at Ship+0x644 in cataclysm.exe");
_Static_assert(offsetof(WorkerSpec, repairTarget) == 0x38, "Worker repair target at Ship+0x670 in cataclysm.exe");
_Static_assert(sizeof(WorkerSpec) == 0x48, "WorkerSpec is 0x48 bytes in cataclysm.exe");
#endif

// WorkerSpec.specialMode: what the special-target order means (WorkerSpecialTarget 0x005c9770)
#define WORKER_MODE_Repair      0
#define WORKER_MODE_Salvage     1
#define WORKER_MODE_None        2

// WorkerSpec.repairState (cat_workerRepairTarget 0x005c89a0)
#define WORKER_REPAIR_Choose    0
#define WORKER_REPAIR_Approach  1
#define WORKER_REPAIR_Jack      3   // in place: extend the jack (animation 0), then the beam
#define WORKER_REPAIR_Repair    4
#define WORKER_REPAIR_Done      5

// the statics (WorkerStaticInit 0x005c9580: 0x00a78d78 sWorker, 0x00a78d48 bWorker; 0x30 bytes each)
typedef struct
{
    real32 jackRetractTime;             // +0x00 key jackRetractTime, 6.0 by default
    DockStaticPoint *grinderPoint;      // +0x04 the "Grinder" dock point
    DockStaticPoint *gunPoint;          // +0x08 the "gun0" dock point (the harvest beam's origin)
    CatSalvageStatics salvage;          // +0x0C cat_salvageStaticInit
    real32 repairDistance;              // +0x14 key repairDistance
    real32 repairCone;                  // +0x18 key repairCone (degrees)
    real32 repairBeamWidth;             // +0x1C key repairBeamWidth
    real32 repairConeCos;               // +0x20 cos(repairCone / 2)
    vector repairBeamOrigin;            // +0x24 the "gun0" point's position
} WorkerStatics;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(WorkerStatics, salvage) == 0x0C, "Worker salvage statics at +0x0C in cataclysm.exe");
_Static_assert(offsetof(WorkerStatics, repairBeamOrigin) == 0x24, "Worker repair beam origin at +0x24 in cataclysm.exe");
_Static_assert(sizeof(WorkerStatics) == 0x30, "WorkerStatics is 0x30 bytes in cataclysm.exe");
#endif

extern CustShipHeader WorkerHeader;

Ship *cat_shipFindMostDamagedPart(Ship *ship);                                      // 0x005c8940
void cat_workerHarvestsResource(Ship *ship, struct Resource *resource);             // 0x005c9130
void cat_workerBeamEffectUpdate(Ship *ship, SpaceObj *target, sdword effectIndex, DockStaticPoint *point);   // 0x005c8e30

#endif
