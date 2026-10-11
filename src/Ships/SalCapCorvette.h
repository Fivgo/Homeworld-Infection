// =============================================================================
//  SalCapCorvette.h
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 11/5/1997 by khent
// =============================================================================

#ifndef ___SALCAPCORVETTE_H
#define ___SALCAPCORVETTE_H

#include <stddef.h>

#include "Attack.h"
#include "Salvage.h"
#include "ShipSelect.h"

// CAT: the exe's specifics (SalCapCorvetteHeader sizeofShipSpecifics 48): the side-step state and the salvager
//      block at +0x08 (cat_salvageGetSalvagerInfo 0x004f8ec0 returns Ship+0x640)
typedef struct
{
    AttackSideStep attacksidestep;      // +0x00
    CatSalvagerInfo salvager;           // +0x08
} CatSalCapCorvetteSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(CatSalCapCorvetteSpec, salvager) == 0x08, "SalCap salvager block at Ship+0x640 in cataclysm.exe");
_Static_assert(sizeof(CatSalCapCorvetteSpec) == 0x30, "the exe's SalCapCorvetteSpec is 0x30 bytes");
#endif

extern CustShipHeader SalCapCorvetteHeader;

#endif
