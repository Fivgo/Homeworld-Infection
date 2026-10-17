// =============================================================================
//  RammingFrigate.h
//  - Cataclysm's sRammingFrigate / bRammingFrigate: rams a target, latches
//    on and pushes it away (cataclysm.exe 0x005bf760-0x005c21e0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___RAMMINGFRIGATE_H
#define ___RAMMINGFRIGATE_H

#include <stddef.h>

#include "SpaceObj.h"
#include "Timer.h"

// CAT: new file (M2-R4 ships-b). 0xA4 bytes in cataclysm.exe (RammingFrigateHeader 0x008c5e38 sizeofShipSpecifics 164)
typedef struct
{
    ubyte state;                        // +0x00 (Ship+0x638) RAM_* below
    ubyte pad01[3];
    udword unknown04[3];
    sdword unknown10;                   // +0x10 (Ship+0x648) 300 at init, 0 when the ram starts
    struct Ship *target;                // +0x14 (Ship+0x64C)
    struct Effect *hitEffect;           // +0x18 (Ship+0x650)
    udword unknown1C;
    CatTimer pushTimer;                 // +0x20 (Ship+0x658) RamPushTime
    vector pushDirection;               // +0x30 (Ship+0x668) the pushed ship's heading when latched
    struct Effect *pushEffect;          // +0x3C (Ship+0x674) the cutting beam
    udword unknown40;                   // +0x40 (Ship+0x678) cleared by RammingFrigateInit
    CatTimer cooldownTimer;             // +0x44 (Ship+0x67C) RamWaitAfterPushTime
    CatTimer chargeTimer;               // +0x54 (Ship+0x68C) 25 s: an upgraded ram gives up then
    CatTimer thrustTimer;               // +0x64 (Ship+0x69C) 0.8 s of thruster effects when the push starts
    bool32 thrustActive;                // +0x74 (Ship+0x6AC)
    bool32 damageTarget;                // +0x78 (Ship+0x6B0) the target is an enemy (no crystal)
    CatTimer losTimer;                  // +0x7C (Ship+0x6B4) the line-of-sight retry
    ubyte unknown8C[0xA4 - 0x8C];
} RammingFrigateSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(RammingFrigateSpec, target) == 0x14, "ram target at Ship+0x64C in cataclysm.exe");
_Static_assert(offsetof(RammingFrigateSpec, pushEffect) == 0x3C, "ram push effect at Ship+0x674 in cataclysm.exe");
_Static_assert(offsetof(RammingFrigateSpec, losTimer) == 0x7C, "ram LOS timer at Ship+0x6B4 in cataclysm.exe");
_Static_assert(sizeof(RammingFrigateSpec) == 0xA4, "RammingFrigateSpec is 0xA4 bytes in cataclysm.exe");
#endif

// ram states (names chosen here from RammingFrigateHouseKeep 0x005c0090)
#define RAM_Idle        0
#define RAM_Turn        1   // turn to the target until facing it and cooled down
#define RAM_Approach    2   // fly at it until close and in line of sight
#define RAM_Ram         3   // full speed at it
#define RAM_4           4
#define RAM_Push        5   // latched: push it away

// CAT: the statics (sRammingFrigate 0x00a7a700, bRammingFrigate 0x00a7a6e0), copied from the ship's own keys
typedef struct
{
    udword unknown00;
    real32 ramDamage;                   // +0x04 RamDamage (statinfo +0x418)
    real32 cuttingBeamDamagePerSecond;  // +0x08 (+0x41C)
    real32 cuttingBeamUpgradedDamagePerSecond;  // +0x0C (+0x420)
    real32 ramPushTime;                 // +0x10 (+0x424)
    real32 ramWaitAfterPushTime;        // +0x14 (+0x428)
} RammingFrigateStatics;

extern CustShipHeader RammingFrigateHeader;

void RammingFrigateRemoveShipReferences(Ship *ship, Ship *shiptoremove);    // 0x005bfe60
bool32 cat_shipImmuneToRamming(Ship *ship);                         // 0x005bf760
bool32 cat_shipHasRamUpgrade(Ship *ship);                           // 0x005bf840 (soundEventUpdate's; M2-R5 layout-dock)
bool32 cat_shipInfectsOnRam(Ship *ship);                            // 0x005bf790
void cat_rammingFrigateRelease(Ship *ship, ubyte newstate);         // 0x005bf8a0
bool32 cat_shipIsLiveRammingFrigate(Ship *ship);                    // 0x005c0a70
bool32 cat_shipIsRammingFrigate(Ship *ship);                        // 0x005c0a90
bool32 cat_rammingFrigateIsPushing(Ship *ship);                     // 0x005c0ac0
void cat_rammingFrigateForceRelease(Ship *ship);                    // 0x005c0ae0
void cat_rammingFrigateCollide(Ship *a, Ship *b);                   // 0x005c14a0
bool32 cat_rammingFrigateStateActive(Ship *ship);                   // 0x005c1ff0
real32 cat_rammingFrigateSpeedMultiplier(Ship *ship);               // 0x005c2120
bool32 cat_rammingFrigateTrailActive(void *trail);                  // 0x005c2180 (a shiptrail)
void cat_shipSetHeading(Ship *ship, vector *dir);                   // 0x005bff90
void cat_shipBlendHeading(Ship *ship, vector *dir, real32 wdir, real32 wcur);  // 0x005bfef0

#endif
