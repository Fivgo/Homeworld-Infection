// =============================================================================
//  Swarmer.h
//  - Cataclysm's sSwarmer / bSwarmer: the fighters a hive frigate launches
//    (cataclysm.exe 0x005c7930-0x005c7ac0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___SWARMER_H
#define ___SWARMER_H

#include <stddef.h>

#include "Attack.h"
#include "SpaceObj.h"

// CAT: new file (M2-R4 ships-b). 0x20 bytes in cataclysm.exe (SwarmerHeader 0x008c6ab0 sizeofShipSpecifics 32)
typedef struct
{
    AttackSideStep attacksidestep;      // +0x00 (Ship+0x638)
    sdword hiveSlot;                    // +0x08 (Ship+0x640) its slot in the hive's swarmer array (HiveFrigate.c)
    struct Ship *hive;                  // +0x0C (Ship+0x644) the hive frigate it belongs to (HiveFrigate.c sets it; a
                                        //   swarmer's kills count for it, UnivUpdate.c)
    sdword state;                       // +0x10 (Ship+0x648) CAT_SWARMER_* (cat_swarmerSetState)
    real32 moveTime;                    // +0x14 (Ship+0x64C) when it may launch / return (HiveFrigateHouseKeep)
    real32 moveDelay;                   // +0x18 (Ship+0x650) its random launch delay, reused for the return
    real32 lastHealTime;                // +0x1C (Ship+0x654) docked, it heals 1 point per second
                                        //   (cat_hiveFrigateUpdateSwarmers)
} SwarmerSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(SwarmerSpec, hive) == 0x0C, "the hive at Ship+0x644 in cataclysm.exe");
_Static_assert(offsetof(SwarmerSpec, state) == 0x10, "the swarm state at Ship+0x648 in cataclysm.exe");
_Static_assert(sizeof(SwarmerSpec) == 0x20, "SwarmerSpec is 0x20 bytes in cataclysm.exe");
#endif

// the swarm states (cat_swarmerGetState 0x005c79e0 / cat_swarmerSetState; names chosen here from HiveFrigate.c's use)
#define CAT_SWARMER_Docked          0
#define CAT_SWARMER_Launching       1
#define CAT_SWARMER_Free            2
#define CAT_SWARMER_Returning       3
#define CAT_SWARMER_Attacking       4

extern CustShipHeader SwarmerHeader;

void SwarmerAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist);
sdword cat_swarmerGetState(Ship *ship);                 // 0x005c79e0
void cat_swarmerSetState(Ship *ship, sdword state);     // 0x005c7a50

#endif
