// =============================================================================
//  Dissolve.h
//  - Cataclysm's ship dissolve: a ship (a crystal, an RKO piece) cut away by a
//    moving plane while sparks fly to the point it dissolves into
//    (cataclysm.exe 0x004601d0-0x00460cb0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___DISSOLVE_H
#define ___DISSOLVE_H

#include "SpaceObj.h"
#include "Types.h"

// CAT: new file (M2-R4 ships-b). The exe links it between the console code and Crates.c.

#define CAT_DISSOLVE_DEFAULT_TIME       10.0f       // 0x00878730: cat_shipDissolveStart with no time

void cat_vecShipLocalToWorld(vector *out, vector *local, Ship *ship);  // 0x004601d0
void cat_vecWorldToShipLocal(vector *out, vector *pos, Ship *ship);    // 0x005c85b0
void cat_shipDissolveComputePlane(Ship *ship, real32 plane[4]);        // 0x00460270 (CAT, M2-R5 render)
void cat_shipDissolveInit(Ship *ship);                                  // 0x00460220
bool32 cat_shipDissolveIsActive(Ship *ship);                            // 0x00460250
void cat_shipDissolveStart(Ship *ship, vector *point, real32 time);     // 0x004603d0
void cat_shipDissolveSetDirection(Ship *ship, vector *point);           // 0x004604a0
real32 cat_shipDissolveFraction(Ship *ship);                            // 0x004604d0
void cat_shipDissolveUpdate(Ship *ship);                                // 0x00460850
void cat_shipDissolveStop(Ship *ship);                                  // 0x00460c70

#endif
