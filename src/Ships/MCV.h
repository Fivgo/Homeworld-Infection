// =============================================================================
//  MCV.h
//  - Cataclysm's sMCV (mimic corvette): a side-stepping corvette with the
//    mimic block (cataclysm.exe 0x005b80f0-0x005b8310)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___MCV_H
#define ___MCV_H

#include <stddef.h>

#include "Attack.h"
#include "Mimic.h"
#include "SpaceObj.h"

// CAT: new file (M2-R4 ships-b). 0x13C bytes in cataclysm.exe (MCVHeader 0x008c4d10 sizeofShipSpecifics 316): the
//      mimic block (Mimic.h) with the side step in its +0x44 gap
typedef union
{
    MimicSpec mimic;
    struct
    {
        ubyte pad00[0x44];
        AttackSideStep sidestep;        // +0x44 (Ship+0x67C) attackSideStep
        sdword linkKills[2][NUM_CLASSES];   // +0x4C (Ship+0x684) the kills of the two linked mimics (cat_processLinkToDo
                                            //      0x004680a2, cat_processUnlinkToDo 0x00468eaa); cleared by MCVInit
    };
} MCVSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(MCVSpec, sidestep) == 0x44, "MCV side step at Ship+0x67C in cataclysm.exe");
_Static_assert(sizeof(MCVSpec) == 0x13C, "MCVSpec is 0x13C bytes in cataclysm.exe");
#endif

extern CustShipHeader MCVHeader;

void MCVAttackPassive(Ship *ship, Ship *target, bool32 rotate);    // 0x005b8210 (also p1StandardCorvette's slot 6)
void cat_mcvUnmimic(Ship *ship, bool32 animate);                    // 0x005b8280
bool32 MCVHaltCommand(Ship *ship);                                  // 0x005b82e0 (also p1StandardCorvette's slot 27)

#endif
