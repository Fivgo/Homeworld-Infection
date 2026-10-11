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
//      (droidCreateArray 0x005c3070, droidUpdateArray 0x005c32f0, cat_droidRenderArray, cat_droidFreeArray: Ship+0x4C0),
//      which are stage 3 (UnivUpdate.c's CAT-TODOs).
typedef struct
{
    udword unused[2];
} RepairDroidSpec;

extern CustShipHeader RepairDroidHeader;

#endif
