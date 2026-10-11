// =============================================================================
//  Slipgate.h
//  - Cataclysm's dSlipgateSmall / Medium / Large: paired gates ships dock with
//    to cross to the partner gate (cataclysm.exe 0x005c6fa0-0x005c7780)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___SLIPGATE_H
#define ___SLIPGATE_H

#include <stddef.h>

#include "ShipSelect.h"
#include "SpaceObj.h"

// CAT: new file (M2-R4 ships-b). 0x30 bytes in cataclysm.exe (SlipgateHeader 0x008c68c0 sizeofShipSpecifics 48)
typedef struct
{
    struct Ship *partner;               // +0x00 (Ship+0x638) the other gate (cat_slipgateSetPartner, KAS)
    ubyte state;                        // +0x04 (Ship+0x63C) SLIPGATE_* below
    ubyte pad05[3];
    real32 openTime;                    // +0x08 (Ship+0x640) when it was opened; it closes 4 s later
    struct Effect *effect;              // +0x0C (Ship+0x644)
    real32 permanentSize;               // +0x10 (Ship+0x648) the first effect parameter of the closed gate's effect
    real32 openingSize;                 // +0x14 (Ship+0x64C) ... of the opening/closing effects
    udword color;                       // +0x18 (Ship+0x650) 0xAABBGGRR, the third effect parameter
    udword unknown1C;                   // +0x1C (Ship+0x654) cleared by SlipgateInit
    udword unknown20;                   // +0x20 (Ship+0x658) cleared by SlipgateInit
    GrowSelection waiting;              // +0x24 (Ship+0x65C) ships that arrived at it this update (cat_slipgateAddShip)
    real32 stateEndTime;                // +0x2C (Ship+0x664) when the opening/closing effect is over
} SlipgateSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(SlipgateSpec, waiting) == 0x24, "slipgate grow selection at Ship+0x65C in cataclysm.exe");
_Static_assert(sizeof(SlipgateSpec) == 0x30, "SlipgateSpec is 0x30 bytes in cataclysm.exe");
#endif

#define SLIPGATE_Closed     0
#define SLIPGATE_Opening    1
#define SLIPGATE_Open       2
#define SLIPGATE_Closing    3

extern CustShipHeader SlipgateHeader;

bool32 cat_slipgateOpen(Ship *gate);                    // 0x005c70e0
void cat_slipgateClose(Ship *gate);                     // 0x005c7180
void cat_slipgateSetPartner(Ship *gate, Ship *partner); // 0x005c71b0
void cat_slipgateAddShip(Ship *gate, Ship *ship);       // 0x005c73b0

#endif
