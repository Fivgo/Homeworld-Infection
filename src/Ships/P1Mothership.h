// =============================================================================
//  P1Mothership.h
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 5/06/1998 by ddunlop
// =============================================================================

#ifndef ___P1MOTHERSHIP_H
#define ___P1MOTHERSHIP_H

#include "Salvage.h"
#include "SpaceObj.h"

// CAT: 0x144 bytes in cataclysm.exe (P1MothershipHeader sizeofShipSpecifics 324): the salvage depot block only
//      (cat_salvageDepotInit 0x004fc7c0 uses Ship+0x638 for type 0x60)
typedef struct
{
    CatSalvageDepot depot;              // +0x00
} P1MothershipSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(P1MothershipSpec) == 0x144, "P1MothershipSpec is 0x144 bytes in cataclysm.exe");
#endif

extern CustShipHeader P1MothershipHeader;

#endif
