// =============================================================================
//  Acolyte.h
//  - Cataclysm's sAcolyte / bAcolyte / dAcolyte: the linkable fighter with
//    folding wings and a linked missile weapon
//    (cataclysm.exe 0x005ad280-0x005adba0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___ACOLYTE_H
#define ___ACOLYTE_H

#include <stddef.h>

#include "GenericInterceptor.h"
#include "ShipLink.h"
#include "SpaceObj.h"

// CAT: new file (M2-R4 ships-b). 0x5C bytes in cataclysm.exe (AcolyteHeader 0x008c3260 sizeofShipSpecifics 92)
typedef struct
{
    ubyte interceptor[0x18];            // +0x00 the interceptor specifics (GenericInterceptorInit)
    CatLinkInfo link;                   // +0x18 (Ship+0x650)
    ubyte wingsOpen;                    // +0x44 (Ship+0x67C)
    ubyte wingsPending;                 // +0x45 (Ship+0x67D) a wing animation waits for wingsTime
    ubyte pad46[2];
    real32 wingsTime;                   // +0x48 (Ship+0x680)
    sdword wingsState;                  // +0x4C (Ship+0x684) 0 open (animation 0), 1 closed (animation 1)
    struct Ship *linkTarget;            // +0x50 (Ship+0x688) the target of the linked weapon
    bool32 linkFire;                    // +0x54 (Ship+0x68C) fire the linked weapon at linkTarget
    bool32 linkAttackPending;           // +0x58 (Ship+0x690) an attack order on linkTarget is due
} AcolyteSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(AcolyteSpec, link) == 0x18, "Acolyte link block at Ship+0x650 in cataclysm.exe");
_Static_assert(offsetof(AcolyteSpec, linkTarget) == 0x50, "Acolyte link target at Ship+0x688 in cataclysm.exe");
_Static_assert(sizeof(AcolyteSpec) == 0x5C, "AcolyteSpec is 0x5C bytes in cataclysm.exe");
#endif

extern CustShipHeader AcolyteHeader;

void cat_acolyteLinkReset(Ship *ship);              // 0x005ad280
void cat_acolyteLinkGunsOff(Ship *ship);            // 0x005ad2d0
bool32 cat_acolyteCanLink(Ship *ship);              // 0x005ad320
bool32 cat_acolyteCanLinkFire(Ship *ship);          // 0x005adab0 (returns 2 or 0)

#endif
