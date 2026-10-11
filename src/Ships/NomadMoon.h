// =============================================================================
//  NomadMoon.h
//  - Cataclysm's dNomadMoon and its two dNomadFieldEmitter ships: a mission
//    object that sends out a shock wave while big-gun shells fly
//    (cataclysm.exe 0x005bdbb0-0x005be1a0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___NOMADMOON_H
#define ___NOMADMOON_H

#include "SpaceObj.h"
#include "Timer.h"

// CAT: new file (M2-R4 ships-b). 0x1C bytes in cataclysm.exe (NomadMoonHeader 0x008c56e8 sizeofShipSpecifics 28)
typedef struct
{
    struct Ship *emitter[2];            // +0x00 (Ship+0x638) above and below the moon (NomadMoonInit 0x005bdbb0)
    ubyte fieldState;                   // +0x08 (Ship+0x640) 0 idle, 1 charging, 2 cooling down
    ubyte pad09[3];
    CatTimer fieldTimer;                // +0x0C (Ship+0x644)
} NomadMoonSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(NomadMoonSpec) == 0x1C, "NomadMoonSpec is 0x1C bytes in cataclysm.exe");
#endif

// CAT: NomadFieldEmitterHeader 0x008c5778: 4 bytes of specifics, nothing read
typedef struct
{
    udword unused;
} NomadFieldEmitterSpec;

extern CustShipHeader NomadMoonHeader;
extern CustShipHeader NomadFieldEmitterHeader;

Ship *cat_nomadMoonFind(void);                      // 0x005bdc80
bool32 cat_nomadMoonCanFire(Ship *moon);            // 0x005bdcb0
bool32 cat_nomadMoonFire(Ship *moon);               // 0x005bdcf0
sdword cat_nomadMoonEmitterCount(Ship *moon);       // 0x005bdd80

#endif
