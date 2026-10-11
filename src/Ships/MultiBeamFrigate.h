// =============================================================================
//  MultiBeamFrigate.h
//  - Cataclysm's sMultiBeamFrigate / bMultiBeamFrigate: a frigate that closes
//    in, steadies and sweeps its beams over every enemy in range
//    (cataclysm.exe 0x005bd730-0x005bdb30)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___MULTIBEAMFRIGATE_H
#define ___MULTIBEAMFRIGATE_H

#include <stddef.h>

#include "Attack.h"
#include "SpaceObj.h"

// CAT: new file (M2-R4 ships-b), the Cataclysm counterpart of HW1's P2MultiBeamFrigate with different code. 0x28
//      bytes in cataclysm.exe (MultiBeamFrigateHeader 0x008c55a8 sizeofShipSpecifics 40). The attack state is
//      Ship.aistateattack (exe Ship+0x350).
typedef struct
{
    ubyte unknown00[0x20];              // +0x00 not used by the MultiBeamFrigate code
    AttackSideStep attacksidestep;      // +0x20 (Ship+0x658) MultiBeamFrigateInit inits it; the attack keeps its
                                        //   thrust direction in attacksidestep.aidirection (Ship+0x65C)
} MultiBeamFrigateSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(MultiBeamFrigateSpec, attacksidestep) == 0x20, "side-step state at Ship+0x658 in cataclysm.exe");
_Static_assert(sizeof(MultiBeamFrigateSpec) == 0x28, "MultiBeamFrigateSpec is 0x28 bytes in cataclysm.exe");
#endif

// CAT: the statics (sMultiBeamFrigate 0x00a7a9a0, bMultiBeamFrigate 0x00a7a990): MultiBeamRangeRatio (key table
//      0x008c5588), then the side-step keys (AttackSideStepParametersScriptTable, 0x0074e5a0) at +0x04
typedef struct
{
    real32 multiBeamRangeRatio;                     // +0x00: the beam range is the first gun's range times this
    AttackSideStepParameters sidestepParameters;    // +0x04
} MultiBeamFrigateStatics;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(MultiBeamFrigateStatics) == 0x10, "MultiBeamFrigateStatics is 0x10 bytes in cataclysm.exe");
#endif

extern CustShipHeader MultiBeamFrigateHeader;

#endif
