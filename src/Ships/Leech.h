// =============================================================================
//  Leech.h
//  - Cataclysm's sLeech: latches onto an enemy ship and drains its resource
//    units or its hull (cataclysm.exe 0x005b6890-0x005b80f0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___LEECH_H
#define ___LEECH_H

#include <stddef.h>

#include "ShipSelect.h"
#include "SpaceObj.h"

// CAT: new file (M2-R4 ships-b). 0x4C bytes in cataclysm.exe (LeechHeader 0x008c4c60 sizeofShipSpecifics 76)
typedef struct
{
    ubyte state;                        // +0x00 (Ship+0x638) LEECH_* below
    ubyte drainMode;                    // +0x01 (Ship+0x639) LEECH_ACTION_* it drains with now
    ubyte action;                       // +0x02 (Ship+0x63A) the action the player chose (cat_leechSetAction)
    ubyte bodyOpen;                     // +0x03 (Ship+0x63B) mesh animation 0 (latched) has been started
    struct Ship *target;                // +0x04 (Ship+0x63C)
    MaxSelection *targets;              // +0x08 (Ship+0x640) the special targets left (cat_leechFilterTargets)
    sdword latchPoint;                  // +0x0C (Ship+0x644) the DPT_Leech point it holds on target, -1 none
    real32 approachDist;                // +0x10 (Ship+0x648)
    real32 nextEffectTime;              // +0x14 (Ship+0x64C) of the hull-drain effect
    real32 nextDrainTime;               // +0x18 (Ship+0x650) one drain per second
    bool32 detected[MAX_MULTIPLAYER_PLAYERS];   // +0x1C (Ship+0x654) per-player stealth detection (cat_shipDetectedByPlayer)
    ubyte returnToDock;                 // +0x3C (Ship+0x674) LeechHouseKeep sends it home to unload
    ubyte returning;                    // +0x3D (Ship+0x675)
    ubyte full;                         // +0x3E (Ship+0x676)
    ubyte clampPending;                 // +0x3F (Ship+0x677) clamp to target next housekeep
    ubyte released;                     // +0x40 (Ship+0x678) shaken off (cat_empShockShip, cat_shipReleaseAttachedRammers)
    ubyte pad41[3];
    real32 lastTargetTime;              // +0x44 (Ship+0x67C) lastTarget is remembered until then
    struct Ship *lastTarget;            // +0x48 (Ship+0x680) a Leech with one does no bump damage (collCheckShipShipColl)
} LeechSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(LeechSpec, detected) == 0x1C, "Leech detected flags at Ship+0x654 in cataclysm.exe");
_Static_assert(offsetof(LeechSpec, released) == 0x40, "Leech release flag at Ship+0x678 in cataclysm.exe");
_Static_assert(offsetof(LeechSpec, lastTarget) == 0x48, "Leech last target at Ship+0x680 in cataclysm.exe");
_Static_assert(sizeof(LeechSpec) == 0x4C, "LeechSpec is 0x4C bytes in cataclysm.exe");
#endif

// Leech states (LeechSpec.state; names chosen here)
#define LEECH_Idle              0
#define LEECH_PickTarget        1
#define LEECH_Approach          2
#define LEECH_Latch             3
#define LEECH_ACTION_None       6   // latched, not draining (the actions are also states while draining)
#define LEECH_ACTION_DrainRUs   7   // resource units (and some hull)
#define LEECH_ACTION_DrainHull  8
#define LEECH_Detach            9
#define LEECH_DetachFull        10
#define LEECH_Leave             11
#define LEECH_Leave2            12
#define LEECH_DetachNext        13
#define LEECH_Done              14

extern CustShipHeader LeechHeader;

ubyte cat_leechGetAction(Ship *ship);                       // 0x005b6890
void cat_leechSetAction(Ship *ship, ubyte action);          // 0x005b68a0
bool32 cat_shipIsLeech(Ship *ship);                         // 0x005b7fd0
LeechSpec *cat_leechGetSpecifics(Ship *ship);               // 0x005b7ff0
bool32 cat_leechIsLeeching(Ship *ship);                     // 0x005b7ce0

#endif
