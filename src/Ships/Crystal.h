// =============================================================================
//  Crystal.h
//  - Cataclysm's dCrystal0 / dCrystal1: resource crystals a salvage depot
//    dissolves into resource units (cataclysm.exe 0x005affa0-0x005b00d0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___CRYSTAL_H
#define ___CRYSTAL_H

#include <stddef.h>

#include "SpaceObj.h"

// CAT: new file (M2-R4 ships-b). 0x0C bytes in cataclysm.exe (CrystalHeader 0x008c3860 sizeofShipSpecifics 12)
typedef struct
{
    sdword resourceValue;               // +0x00 (Ship+0x638) from the statics (CrystalInit 0x005b0040)
    bool32 noBlast;                     // +0x04 (Ship+0x63C) CrystalDied 0x005b0090 makes a type-3 blast unless set;
                                        //   a depot sets it when the crystal is used up (cat_salvageDepotProcessCrystal)
    bool32 processing;                  // +0x08 (Ship+0x640) a depot is dissolving it
} CrystalSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(CrystalSpec) == 0x0C, "CrystalSpec is 0x0C bytes in cataclysm.exe");
#endif

// CAT: the statics (one block per type: dCrystal0 0x00a762d0, dCrystal1 0x00a762d4; key table 0x008c3840)
typedef struct
{
    sdword resourceValue;
} CrystalStatics;

extern CustShipHeader CrystalHeader;

#endif
