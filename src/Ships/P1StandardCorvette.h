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
#include "Salvage.h"
#include "SpaceObj.h"

// CAT: 0x168 bytes in cataclysm.exe (P1StandardCorvetteHeader sizeofShipSpecifics 360). In Cataclysm the P1 standard
//      corvette is a mimic (the Mimic/MCV specifics, P1StandardCorvetteInit 0x005be660 sets catShipFlags Mimic) and a
//      salvager (salvager block at +0x13C, cat_salvageGetSalvagerInfo 0x004f8ec0 returns Ship+0x774). The mimic fields
//      are named here only as far as P1StandardCorvetteInit sets them; their meaning is the Mimic code's (MimicInit
//      0x005b8360 and the rest of MimicHeader, Cataclysm-only, M2-R4 #4).
typedef struct
{
    ubyte unknown00[0x44];              // +0x00 (mimic block, M2-R4 #4)
    AttackSideStep attacksidestep;      // +0x44 (Ship+0x67C)
    ubyte unknown4C[0xCC - 0x4C];
    sdword mimicApparentType;           // +0xCC (Ship+0x704) -1: the type the mimic shows (cat_mimicGetApparentShipType 0x005b97e0)
    sdword mimicUnknownD0;              // +0xD0 -1
    udword unknownD4;
    void *mimicDataD8;                  // +0xD8 (Ship+0x710) NULL; memFree'd by MimicHaltCommand 0x005b99e0
    ubyte unknownDC[0xE4 - 0xDC];
    sdword mimicUnknownE4;              // +0xE4 -1
    sdword mimicUnknownE8;              // +0xE8 -1
    ubyte unknownEC[0xF8 - 0xEC];
    sdword mimicUnknownF8;              // +0xF8 -1
    sdword mimicUnknownFC;              // +0xFC -1
    udword unknown100;
    void *mimicData104;                 // +0x104 (Ship+0x73C) NULL; memFree'd by MimicHaltCommand
    ubyte unknown108[0x110 - 0x108];
    sdword mimicUnknown110;             // +0x110 -1
    sdword mimicUnknown114;             // +0x114 -1
    ubyte unknown118[0x124 - 0x118];
    sdword mimicUnknown124;             // +0x124 0
    ubyte mimicMode;                    // +0x128 (Ship+0x760) 0 off, 1 mimicking, 8 transforming (MimicHaltCommand)
    ubyte pad129[3];
    ubyte unknown12C[0x134 - 0x12C];
    sdword mimicResource;               // +0x134 (Ship+0x76C) the resource it poses as (cat_univDeleteMimicResource)
    udword unknown138;
    CatSalvagerInfo salvager;           // +0x13C (Ship+0x774)
    ubyte specialMode;                  // +0x164 (Ship+0x79C) 0 none, 1 mimic target, 2 salvage target
    ubyte pad165[3];
} P1StandardCorvetteSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(P1StandardCorvetteSpec, mimicMode) == 0x128, "mimic mode at Ship+0x760 in cataclysm.exe");
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

#endif
