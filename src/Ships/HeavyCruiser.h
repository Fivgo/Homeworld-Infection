// =============================================================================
//  HeavyCruiser.h
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#ifndef ___HEAVYCRUISER_H
#define ___HEAVYCRUISER_H

#include "BeastWeapon.h"
#include "Salvage.h"
#include "SpaceObj.h"

// CAT: 0x15C bytes in cataclysm.exe (HeavyCruiserHeader sizeofShipSpecifics 348): bHeavyCruiser's Beast infection
//      weapon at +0x00 and the salvage depot at +0x18 (cat_salvageDepotInit 0x004fc7c0 uses Ship+0x650 for type 0x2E);
//      kHeavyCruiser and tHeavyCruiser leave both unused
typedef struct
{
    CatBeastWeaponSpec beastWeapon;     // +0x00
    CatSalvageDepot depot;              // +0x18
} HeavyCruiserSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(HeavyCruiserSpec, depot) == 0x18, "bHeavyCruiser depot at Ship+0x650 in cataclysm.exe");
_Static_assert(sizeof(HeavyCruiserSpec) == 0x15C, "HeavyCruiserSpec is 0x15C bytes in cataclysm.exe");
#endif

extern CustShipHeader HeavyCruiserHeader;

#endif
