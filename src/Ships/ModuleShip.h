// =============================================================================
//  ModuleShip.h
//  - Cataclysm's module ships: the parts clamped to a Sect or Beast mothership,
//    a carrier or a dreadnought (cataclysm.exe 0x005af4c0-0x005afef0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___MODULESHIP_H
#define ___MODULESHIP_H

#include <stddef.h>

#include "SpaceObj.h"

// CAT: new file (M2-R4 ships-b). 0x3C bytes in cataclysm.exe (ModuleShipHeader 0x008c3720 sizeofShipSpecifics 60).
//      A module drifts away from its host when it is ejected (cat_moduleShipEject) or loses it (ModuleShipHouseKeep).
typedef struct
{
    ubyte ejecting;                     // +0x00 (Ship+0x638)
    ubyte pad01[3];
    real32 ejectEndTime;                // +0x04 (Ship+0x63C) the push ends; then it explodes or stops applying physics
    vector ejectForce;                  // +0x08 (Ship+0x640) added to its force every update while ejecting
    ubyte unknown14[0x20 - 0x14];
    bool32 spinning;                    // +0x20 (Ship+0x658)
    real32 spinAngle;                   // +0x24 (Ship+0x65C) +0.05 per update, wraps at 2 pi (the renderer turns it)
    vector spinAxis;                    // +0x28 (Ship+0x660) its up vector when ejected
    bool32 explodeWhenDone;             // +0x34 (Ship+0x66C)
    real32 ejectStartTime;              // +0x38 (Ship+0x670)
} ModuleShipSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(ModuleShipSpec, spinning) == 0x20, "module spin flag at Ship+0x658 in cataclysm.exe");
_Static_assert(offsetof(ModuleShipSpec, explodeWhenDone) == 0x34, "module explode flag at Ship+0x66C in cataclysm.exe");
_Static_assert(sizeof(ModuleShipSpec) == 0x3C, "ModuleShipSpec is 0x3C bytes in cataclysm.exe");
#endif

// CAT: one 4-byte block per module type (0x00a7626c .. 0x00a762bc): the delay before a module that lost its host
//      starts drifting away (set by ModuleShipStaticInit, no key)
typedef struct
{
    real32 orphanDelay;
} ModuleShipStatics;

extern CustShipHeader ModuleShipHeader;

ModuleShipStatics *cat_moduleShipGetStatics(ShipType type);                                // 0x005af4c0
void cat_moduleShipEject(Ship *module, bool32 explodeWhenDone, real32 delay, real32 duration);  // 0x005af940
real32 cat_moduleShipDrawnSpinAngle(Ship *module);     // HWI-FEATURE(motion-interp) module spin (M3-R4 module-spin-interp)
Ship *cat_moduleIterFirst(Ship *host);                                                      // 0x005afed0
Ship *cat_moduleIterNext(void);                                                             // 0x005afee0

#endif
