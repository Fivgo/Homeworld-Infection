// =============================================================================
//  ShipDisable.h
//  - Cataclysm's ship disable state: the 13 disable reasons, helplessness,
//    EMP shock and drift (cataclysm.exe 0x005b3900-0x005b4090)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___SHIPDISABLE_H
#define ___SHIPDISABLE_H

#include "SpaceObj.h"
#include "Types.h"

// CAT: new file (M2-R4 ships-b). The exe links these functions after DreadnoughtFire 0x005b3870 and before
//      GravWellGenerator.c; the names are the export's, the field names chosen here (Ship.catHelpless ...).

// catDisableReason[] slot the helplessness uses (cat_shipSetHelpless 0x005b3bd0 writes Ship+0x200 = +0x1DC + 9 * 4)
#define CAT_DISABLE_REASON_Helpless     9

void cat_shipSetDisableReason(Ship *ship, sdword value, sdword reason);     // 0x005b3fe0
void cat_empShockShip(Ship *ship, sdword strength);                         // 0x005b3900
void cat_shipStartDrift(Ship *ship, real32 time, real32 extra);             // 0x005b3b40
void cat_shipSetHelpless(Ship *ship, real32 duration);                      // 0x005b3bd0
void cat_shipClearHelpless(Ship *ship);                                     // 0x005b3c80
void cat_shipUpdateDrift(Ship *ship);                                       // 0x005b3d60
void cat_shipSetSUExempt(Ship *ship, bool32 exempt);                        // 0x005b4020

#endif
