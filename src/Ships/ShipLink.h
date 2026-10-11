// =============================================================================
//  ShipLink.h
//  - the link block of Cataclysm's linkable ships (sAcolyte, bAcolyte, sMimic,
//    sACV, bACV, sMCV): two ships of a type fly and fight as one
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___SHIPLINK_H
#define ___SHIPLINK_H

#include <stddef.h>

#include "SpaceObj.h"

// CAT: new file (M2-R4 ships-b). Every linkable type keeps this block at specifics +0x18 (Ship+0x650):
//      cat_clLinkCleanup 0x00444060, cat_clFindLinkPartner 0x0044d2e0, cat_linkPairShips 0x00467ae0 and
//      cat_unlinkPrepareShip 0x00468580 (the link orders, COMMAND_CAT_LINK / UNLINK: the command layer's follow-up)
//      read and write it; the ship files only clear and save it. Names chosen here.
typedef struct
{
    struct Ship *partner;               // +0x00 (Ship+0x650) the linked partner, NULL when unlinked
    sdword role;                        // +0x04 (Ship+0x654) 2 leader, 5 follower, 0 unlinking (cat_linkPairShips)
    udword unknown08;                   // +0x08 (Ship+0x658)
    sdword mode;                        // +0x0C (Ship+0x65C) 2 for linked Acolytes, 0 for mimics
    vector unlinkDirection1;            // +0x10 (Ship+0x660) cat_unlinkPrepareShip
    vector unlinkDirection2;            // +0x1C (Ship+0x66C)
    void *effect;                       // +0x28 (Ship+0x678) not saved: the _PreFix of every linkable ship clears it
} CatLinkInfo;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(CatLinkInfo) == 0x2C, "the link block is 0x2C bytes in cataclysm.exe");
#endif

#endif
