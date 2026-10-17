// =============================================================================
//  Dreadnought.h
//  - Cataclysm's sDreadnought: a missile destroyer whose upgrade grows a
//    charge cannon module (cataclysm.exe 0x005b0e50-0x005b13e0, 0x005b3850-0x005b3900)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___DREADNOUGHT_H
#define ___DREADNOUGHT_H

#include <stddef.h>

#include "SpaceObj.h"
#include "Timer.h"

// CAT: new file (M2-R4 ships-b). 0xD68 bytes in cataclysm.exe (DreadnoughtHeader 0x008c4130 sizeofShipSpecifics 3432):
//      StandardDestroyer's missile regeneration words, 40 entries of 0x54 bytes (only their last dword is set, by
//      DreadnoughtInit), then the cannon state
typedef struct
{
    ubyte unknown00[0x50];
    sdword enabled;                     // +0x50 set to 1 by DreadnoughtInit; no reader in the ship code
} DreadnoughtEntry;

typedef struct
{
    real32 lasttimeRegeneratedMissiles; // +0x000 (Ship+0x638)
    udword unknown04;                   // +0x004
    real32 lasttimeFiredVolley;         // +0x008 (Ship+0x640) -100000 at init
    udword unknown0C;                   // +0x00C
    DreadnoughtEntry entries[40];       // +0x010
    CatTimer cannonChargeTimer;         // +0xD30 (Ship+0x1368) 70 s between cannon blasts
    CatTimer cannonBuildTimer;          // +0xD40 (Ship+0x1378) the cannon module's build progress
    CatTimer cannonFireTimer;           // +0xD50 (Ship+0x1388) 1 s from the order to the blast
    bool32 cannonFiring;                // +0xD60 (Ship+0x1398)
    struct Ship *cannon;                // +0xD64 (Ship+0x139C) the sDreadnoughtCannon module ship
} DreadnoughtSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(DreadnoughtSpec, cannonChargeTimer) == 0xD30, "Dreadnought cannon timer at Ship+0x1368 in cataclysm.exe");
_Static_assert(offsetof(DreadnoughtSpec, cannon) == 0xD64, "Dreadnought cannon at Ship+0x139C in cataclysm.exe");
_Static_assert(sizeof(DreadnoughtSpec) == 0xD68, "DreadnoughtSpec is 0xD68 bytes in cataclysm.exe");
#endif

// CAT: the statics (0x00aadc80): the gun ranges as StandardDestroyer's, the missile keys (table 0x008c40f0)
typedef struct
{
    real32 gunRange[NUM_TACTICS_TYPES];
    real32 tooCloseRange[NUM_TACTICS_TYPES];
    real32 missileRegenerateTime;       // +0x18
    real32 missileVolleyTime;           // +0x1C
    real32 missileLagVolleyTime;        // +0x20
} DreadnoughtStatics;

extern CustShipHeader DreadnoughtHeader;

void DreadnoughtFire(Ship *ship, SpaceObjRotImpTarg *target);      // 0x005b3870 (also the Sentinel's slot 5)
bool32 cat_dreadnoughtHasCannonUpgrade(Ship *ship);                 // 0x005b1290
real32 cat_dreadnoughtChargeFraction(Ship *ship);                   // 0x005b12c0
void cat_dreadnoughtCreateCannon(Ship *ship);                       // 0x005b12f0
void cat_dreadnoughtReclampCannon(Ship *ship);                      // 0x005b13b0
void cat_dreadnoughtRenderCharge(Ship *ship);                       // 0x005b1110 (CAT, M2-R5 render)

#endif
