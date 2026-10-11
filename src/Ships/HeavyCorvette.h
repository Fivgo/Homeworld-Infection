// =============================================================================
//  HeavyCorvette.h
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#ifndef ___HEAVYCORVETTE_H
#define ___HEAVYCORVETTE_H

#include "Attack.h"

typedef struct
{
    AttackSideStep attacksidestep;
    sdword burstState;
    sdword cooldown;
    vector burstFireVector;
    real32 burstChargeState;
    real32 burstChargeState2;
    real32 bulletLifeTime;
    Effect *chargeEffect;
} HeavyCorvetteSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(HeavyCorvetteSpec) == 44, "HeavyCorvetteSpec is 44 bytes in cataclysm.exe (HeavyCorvetteHeader sizeofShipSpecifics)");   // CAT
#endif

extern CustShipHeader HeavyCorvetteHeader;

void heavyCorvetteOrderChanged(Ship *ship);

#endif
