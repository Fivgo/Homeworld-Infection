// =============================================================================
//  HiveFrigate.h
//  - Cataclysm's sHiveFrigate / bHiveFrigate: a frigate that carries six
//    swarmers and sends them at its special targets
//    (cataclysm.exe 0x005b54b0-0x005b6820)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___HIVEFRIGATE_H
#define ___HIVEFRIGATE_H

#include <stddef.h>

#include "SpaceObj.h"

#define HIVE_NUM_SWARMERS       6
#define HIVE_MAX_TARGETS        1000

// CAT: new file (M2-R4 ships-b). 0xFD8 bytes in cataclysm.exe (HiveFrigateHeader 0x008c4b40 sizeofShipSpecifics 4056)
typedef struct
{
    real32 nextSwarmerTime;             // +0x000 (Ship+0x638) when the next lost swarmer is replaced
    sdword swarmState;                  // +0x004 (Ship+0x63C) HIVE_* below
    struct Ship *swarmer[HIVE_NUM_SWARMERS];    // +0x008 (Ship+0x640)
    sdword numSwarmers;                 // +0x020 (Ship+0x658)
    struct Ship *targets[HIVE_MAX_TARGETS];     // +0x024 (Ship+0x65C) the special targets
    sdword numTargets;                  // +0xFC4 (Ship+0x15FC)
    struct Ship *target;                // +0xFC8 (Ship+0x1600) the first live special target
    sdword moveState;                   // +0xFCC (Ship+0x1604) 0 idle, 1 flying to the target, 2 a dock order waits
    struct Ship *dockwith;              // +0xFD0 (Ship+0x1608) the ship of the waiting dock order
    ubyte doorsOpen;                    // +0xFD4 (Ship+0x160C) mesh animation 0 (open) has been started
    ubyte padFD5[3];
} HiveFrigateSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(HiveFrigateSpec, numTargets) == 0xFC4, "hive target count at Ship+0x15FC in cataclysm.exe");
_Static_assert(sizeof(HiveFrigateSpec) == 0xFD8, "HiveFrigateSpec is 0xFD8 bytes in cataclysm.exe");
#endif

// the swarm states (HiveFrigateSpec.swarmState; names chosen here from HiveFrigateHouseKeep 0x005b5b30)
#define HIVE_Idle               0
#define HIVE_Launch             1   // in range of the target: give each docked swarmer a random launch delay
#define HIVE_Launching          2   // launch the swarmers as their delays run out
#define HIVE_Attack             3   // send every launched swarmer at the target
#define HIVE_Attacking          4
#define HIVE_Recall             5   // give each launched swarmer a return time
#define HIVE_Recalling          6   // dock the swarmers as their times run out
#define HIVE_Approach           7   // flying to the target

// CAT: the statics (sHiveFrigate 0x00a7ace0, bHiveFrigate 0x00a7aca0), set by HiveFrigateStaticInit, no keys
typedef struct
{
    real32 swarmerRegenTime;            // +0x00 15 s
    udword unknown04;
    real32 swarmRange;                  // +0x08 7000: the hive launches inside this range of its target
    real32 gunRange[NUM_TACTICS_TYPES];         // +0x0C bulletRange * 0.8
    real32 tooCloseRange[NUM_TACTICS_TYPES];    // +0x18 minBulletRange * 0.7
} HiveFrigateStatics;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(HiveFrigateStatics) == 0x24, "HiveFrigateStatics is 0x24 bytes in cataclysm.exe");
#endif

extern CustShipHeader HiveFrigateHeader;

bool32 cat_hiveFrigateSwarmReady(Ship *hive);       // 0x005b5ac0
void cat_hiveFrigateResetSwarm(Ship *hive);         // 0x005b5af0

#endif
