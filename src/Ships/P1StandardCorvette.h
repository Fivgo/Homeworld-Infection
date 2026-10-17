// =============================================================================
//  P1StandardCorvette.h
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 5/06/1998 by ddunlop
// =============================================================================

#ifndef ___P1STANDARDCORVETTE_H
#define ___P1STANDARDCORVETTE_H

#include <stddef.h>

#include "Attack.h"
#include "Mimic.h"
#include "Salvage.h"
#include "SpaceObj.h"

// CAT: 0x168 bytes in cataclysm.exe (P1StandardCorvetteHeader sizeofShipSpecifics 360). In Cataclysm the P1 standard
//      corvette is a mimic (the Mimic/MCV specifics, P1StandardCorvetteInit 0x005be660 sets catFlags18 Mimic) and a
//      salvager (salvager block at +0x13C, cat_salvageGetSalvagerInfo 0x004f8ec0 returns Ship+0x774). The first 0x13C
//      bytes are the mimic block of sMimic/sMCV (Mimic.h) [M2-R4 ships-b].
typedef struct
{
    union
    {
        MimicSpec mimic;                // +0x00 (Ship+0x638) the mimic block (Mimic.h, M2-R4 ships-b)
        struct
        {
            ubyte pad00[0x44];
            AttackSideStep attacksidestep;      // +0x44 (Ship+0x67C)
        };
    };
    CatSalvagerInfo salvager;           // +0x13C (Ship+0x774)
    ubyte specialMode;                  // +0x164 (Ship+0x79C) 0 none, 1 mimic target, 2 salvage target
    ubyte pad165[3];
} P1StandardCorvetteSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(P1StandardCorvetteSpec, mimic.mode) == 0x128, "mimic mode at Ship+0x760 in cataclysm.exe");
_Static_assert(offsetof(P1StandardCorvetteSpec, salvager) == 0x13C, "salvager block at Ship+0x774 in cataclysm.exe");
_Static_assert(sizeof(P1StandardCorvetteSpec) == 0x168, "P1StandardCorvetteSpec is 0x168 bytes in cataclysm.exe");
#endif

// CAT: the statics (P1StandardCorvetteStaticInit 0x005be610: one block 0x00a7a7e0): the salvage statics first
//      (cat_salvageStaticInit uses custstatinfo +0 for p1StandardCorvette), the side-step keys at +0x08
typedef struct
{
    CatSalvageStatics salvage;                      // +0x00
    AttackSideStepParameters sidestepParameters;    // +0x08
} P1StandardCorvetteStatics;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(P1StandardCorvetteStatics, sidestepParameters) == 0x08, "side-step keys at 0x00a7a7e8 in cataclysm.exe");
#endif

extern CustShipHeader P1StandardCorvetteHeader;

bool32 P1StandardCorvetteSpecialActivate(Ship *ship);     // 0x005be7a0 (also sMCV's slot 7)

#endif
