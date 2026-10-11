// =============================================================================
//  Processor.h
//  - Cataclysm's sProcessor / bProcessor: the salvage depot with auto-repair
//    beams (cataclysm.exe 0x005be8f0-0x005bf3a0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___PROCESSOR_H
#define ___PROCESSOR_H

#include <stddef.h>

#include "Salvage.h"
#include "SpaceObj.h"
#include "Timer.h"

#define PROCESSOR_MAX_BEAMS     4

// CAT: new file (M2-R4 ships-b). One auto-repair beam per DPT_AutoRepair dock point (0x20 bytes on ILP32; names chosen
//      here from their users)
typedef struct
{
    struct Ship *processor;             // +0x00
    DockStaticPoint *point;             // +0x04 the beam's DPT_AutoRepair point (saved as its dockindex)
    struct Ship *target;                // +0x08
    CatTimer timer;                     // +0x0C the next target search
    struct Effect *effect;              // +0x1C
} ProcessorBeam;

// 0x1C8 bytes in cataclysm.exe (ProcessorHeader 0x008c5b60 sizeofShipSpecifics 456)
typedef struct
{
    CatSalvageDepot depot;              // +0x000 (Ship+0x638) cat_salvageGetDepotInfo
    ProcessorBeam beams[PROCESSOR_MAX_BEAMS];   // +0x144 (Ship+0x77C)
    sdword numBeams;                    // +0x1C4 (Ship+0x7FC)
} ProcessorSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(ProcessorBeam) == 0x20, "a Processor beam is 0x20 bytes in cataclysm.exe");
_Static_assert(offsetof(ProcessorSpec, beams) == 0x144, "Processor beams at Ship+0x77C in cataclysm.exe");
_Static_assert(offsetof(ProcessorSpec, numBeams) == 0x1C4, "Processor beam count at Ship+0x7FC in cataclysm.exe");
_Static_assert(sizeof(ProcessorSpec) == 0x1C8, "ProcessorSpec is 0x1C8 bytes in cataclysm.exe");
#endif

extern CustShipHeader ProcessorHeader;

#endif
