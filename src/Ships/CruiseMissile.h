// =============================================================================
//  CruiseMissile.h
//  - Cataclysm's bCruiseMissile: a Beast kamikaze missile ship
//    (cataclysm.exe 0x005afef0-0x005affa0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___CRUISEMISSILE_H
#define ___CRUISEMISSILE_H

#include "SpaceObj.h"

// CAT: new file (M2-R4 ships-b). 4 bytes of specifics in cataclysm.exe (CruiseMissileHeader 0x008c37b0), none used
typedef struct
{
    udword unused;
} CruiseMissileSpec;

extern CustShipHeader CruiseMissileHeader;

#endif
