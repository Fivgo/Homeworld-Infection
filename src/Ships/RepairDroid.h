// =============================================================================
//  RepairDroid.h
//  - Cataclysm's sRepairDroid / bRepairDroid ship type
//    (cataclysm.exe RepairDroidStaticInit 0x005c29b0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___REPAIRDROID_H
#define ___REPAIRDROID_H

#include "SpaceObj.h"

// CAT: new file (M2-R4 ships-b). 8 bytes of specifics in cataclysm.exe (RepairDroidHeader 0x008c6170), none used.
//      The droids that crawl over a Sect ship's hull are not ships of this type but render-side proxies
//      (droidCreateArray 0x005c3070, droidUpdateArray 0x005c32f0, cat_droidRenderArray, cat_droidFreeArray: Ship+0x4C0).
//      The renderer half (cat_droidRenderArray) is here (M2-R5 render). CAT-TODO(M2-R6 ships, the repair droids): the
//      create, update and free (with cat_droidRandomWalkVertex 0x005c29e0, cat_droidWalkTowardPoint 0x005c2b80,
//      droidIsValidPredockPos 0x005c2fd0) draw from random stream 0, which the oracle hashes, so they go with the ship
//      runtime and its oracle runs, not the renderer; until then no ship has an array.
typedef struct
{
    udword unused[2];
} RepairDroidSpec;

extern CustShipHeader RepairDroidHeader;

// CAT (M2-R5 render): a ship's repair droids (droidCreateArray 0x005c3070: a 0x18-byte header, 0x2C per droid, each droid
//      a 0x63C-byte sRepairDroid ship object that is not on any list). Names chosen here.
typedef struct CatRepairDroid
{
    struct Ship *ship;                  // +0x00 the droid's object
    bool8 active;                       // +0x04
    ubyte pad05[3];
    vector position;                    // +0x08
    real32 unknown14[2];                // +0x14
    real32 unknown1C;                   // +0x1C
    real32 unknown20[2];                // +0x20 -1.0 at creation
    udword unknown28;                   // +0x28
} CatRepairDroid;
typedef struct CatRepairDroidArray
{
    CatRepairDroid *droids;             // +0x00
    sdword numDroids;                   // +0x04
    struct Ship *owner;                 // +0x08
    sdword unknown0C;                   // +0x0C -1 at creation
    real32 unknown10;                   // +0x10 -1.0 at creation
    real32 lastHealth;                  // +0x14 the owner's health at creation
} CatRepairDroidArray;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(CatRepairDroid) == 0x2C, "a repair droid entry is 0x2C bytes in cataclysm.exe");
_Static_assert(sizeof(CatRepairDroidArray) == 0x18, "the repair droid header is 0x18 bytes in cataclysm.exe");
#endif
void cat_droidRenderArray(CatRepairDroidArray *array);                         // 0x005c3cf0

#endif
