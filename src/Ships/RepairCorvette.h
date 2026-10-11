// =============================================================================
//  RepairCorvette.h
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#ifndef ___REPAIRCORVETTE_H
#define ___REPAIRCORVETTE_H

#include "Attack.h"
#include "ShipSelect.h"

// CAT (M2-R4 ships-b): the shared repair beam and refuel/repair loop only; the dormant RepairCorvetteHeader is gone
bool32 refuelRepairShips(Ship *ship, SelectAnyCommand *targets, real32 rangetoRefuel);
void stopRepairEffect(Ship *ship);

#endif
