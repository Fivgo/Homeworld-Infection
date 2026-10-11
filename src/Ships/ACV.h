// =============================================================================
//  ACV.h
//  - Cataclysm's sACV / bACV (assault corvette): side-stepping gun corvette
//    with the EMP pulse upgrade (cataclysm.exe 0x005adba0-0x005aeed0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___ACV_H
#define ___ACV_H

#include <stddef.h>

#include "Attack.h"
#include "ShipLink.h"
#include "ShipSelect.h"
#include "SpaceObj.h"

// CAT: new file (M2-R4 ships-b). 0xD4 bytes in cataclysm.exe (ACVHeader 0x008c3328 sizeofShipSpecifics 212).
//      Field names chosen here from their users.
typedef struct
{
    ubyte unknown00[0x18];              // +0x00 (Ship+0x638) no reader in the ACV code
    CatLinkInfo link;                   // +0x18 (Ship+0x650)
    AttackSideStep sidestep;            // +0x44 (Ship+0x67C) attackSideStep
    udword unknown4C[0x18];             // +0x4C (Ship+0x684) cleared by ACVInit, no other user found
    GrowSelection empTargets;           // +0xAC (Ship+0x6E4) the ACVs of the pulse and the ships it hit
    real32 empTime;                     // +0xB4 (Ship+0x6EC) when the last pulse started
    ubyte empState;                     // +0xB8 (Ship+0x6F0) ACV_EMP_* below
    ubyte padB9[3];
    struct Ship *empTarget;             // +0xBC (Ship+0x6F4) the ship to pulse at (state ACV_EMP_Targeted)
    bool32 attackPending;               // +0xC0 (Ship+0x6F8) an attack order on empTarget is due
    sdword linkStateA;                  // +0xC4 (Ship+0x6FC) set to 2 by cat_acvLinkReset
    sdword linkStateB;                  // +0xC8 (Ship+0x700) set to 2 by cat_acvLinkReset
    udword unknownCC;                   // +0xCC (Ship+0x704) cleared by ACVInit
    udword unknownD0;                   // +0xD0 (Ship+0x708) cleared by ACVInit
} ACVSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(ACVSpec, link) == 0x18, "ACV link block at Ship+0x650 in cataclysm.exe");
_Static_assert(offsetof(ACVSpec, sidestep) == 0x44, "ACV side step at Ship+0x67C in cataclysm.exe");
_Static_assert(offsetof(ACVSpec, empTargets) == 0xAC, "ACV EMP selection at Ship+0x6E4 in cataclysm.exe");
_Static_assert(offsetof(ACVSpec, empState) == 0xB8, "ACV EMP state at Ship+0x6F0 in cataclysm.exe");
_Static_assert(offsetof(ACVSpec, empTarget) == 0xBC, "ACV EMP target at Ship+0x6F4 in cataclysm.exe");
_Static_assert(sizeof(ACVSpec) == 0xD4, "ACVSpec is 0xD4 bytes in cataclysm.exe");
#endif

// ACVSpec.empState (names chosen here)
#define ACV_EMP_Ready       0   // charged (with the upgrade)
#define ACV_EMP_Pulsing     1   // 2 s after a pulse (also the state after ACVInit)
#define ACV_EMP_Recharging  2   // 75 s more
#define ACV_EMP_Targeted    3   // flying at empTarget to pulse there

// the statics (ACVStaticInit 0x005ae360: 0x00ab74e0 Sect, 0x00ab7500 Beast)
typedef struct
{
    udword unknown00;
    real32 empMaxCombineDistance;       // +0x04 EMP_Max_Combine_Distance (statinfo +0x438)
    real32 empRange;                    // +0x08 EMP_Range (statinfo +0x434)
    real32 unknown0C;                   // +0x0C 2400.0, no reader found
    AttackSideStepParameters sidestepParameters;    // +0x10
} ACVStatics;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(ACVStatics, sidestepParameters) == 0x10, "ACV side-step parameters at +0x10 in cataclysm.exe");
#endif

extern CustShipHeader ACVHeader;

bool32 cat_empShipClampedToPlayer(udword playerIndex, Ship *ship);     // 0x005adba0
void cat_acvLinkReset(Ship *ship);                                      // 0x005adc10
bool32 cat_acvIsFullyLinked(Ship *ship);                                // 0x005adc60
bool32 cat_acvIsLinking(Ship *ship);                                    // 0x005add10
bool32 cat_empCanAffectShip(Ship *target, Ship *attacker);              // 0x005add30
void cat_empPulse(GrowSelection *group, Ship *ship);                    // 0x005ade10
bool32 cat_shipIsACV(Ship *ship);                                       // 0x005ae1b0
real32 cat_acvEMPChargeFraction(Ship *ship);                            // 0x005ae840
GrowSelection *cat_acvGetSpecifics(Ship *ship);                         // 0x005aed60

#endif
