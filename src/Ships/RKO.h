// =============================================================================
//  RKO.h
//  - Cataclysm's dRKO (the Naggarok): picks a ship, flies at it, swallows it
//    into its weapon point and gains missiles from it (cataclysm.exe
//    0x005c3df0-0x005c59a0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___RKO_H
#define ___RKO_H

#include <stddef.h>

#include "SpaceObj.h"
#include "Timer.h"

// CAT: new file (M2-R4 ships-b). 0xD0 bytes in cataclysm.exe (RKOHeader 0x008c6458 sizeofShipSpecifics 208).
//      Field names chosen here from their users.
typedef struct
{
    bool32 fleeing;                     // +0x00 (Ship+0x638) after a meal: fly off until fleeTimer runs out
    bool32 ateTaiidan;                  // +0x04 (Ship+0x63C) the last meal was a RACE_Taiidan ship (short flee)
    udword naggarokEMP;                 // +0x08 (Ship+0x640) the KAS variable G_NaggarokEMP
    CatTimer fleeTimer;                 // +0x0C (Ship+0x644)
    vector fleePoint;                   // +0x1C (Ship+0x654)
    ubyte state;                        // +0x28 (Ship+0x660) RKO_* below
    ubyte pad29[3];
    struct Ship *target;                // +0x2C (Ship+0x664)
    vector targetStartPos;              // +0x30 (Ship+0x668) where the target was when the approach was planned
    vector approachPoint;               // +0x3C (Ship+0x674) just short of the target
    vector approachHeading;             // +0x48 (Ship+0x680)
    vector toTarget;                    // +0x54 (Ship+0x68C)
    real32 toTargetDist;                // +0x60 (Ship+0x698)
    real32 waitStart;                   // +0x64 (Ship+0x69C)
    real32 waitEnd;                     // +0x68 (Ship+0x6A0)
    sdword naggarokMissiles;            // +0x6C (Ship+0x6A4) missiles gained from meals (cat_rkoGetNaggarokMissiles)
    real32 nextSearchTime;              // +0x70 (Ship+0x6A8)
    vector restrictCenter;              // +0x74 (Ship+0x6AC) with restrictActive: targets only within
    real32 restrictRadius;              // +0x80 (Ship+0x6B8) 1e6 at init
    real32 restrictRadiusSq;            // +0x84 (Ship+0x6BC) 1e12 at init
    bool32 restrictActive;              // +0x88 (Ship+0x6C0) no setter in the ship code (KAS)
    bool32 bigGunWarning;               // +0x8C (Ship+0x6C4) a Mothership big-gun shot is on its way
    vector bigGunPos;                   // +0x90 (Ship+0x6C8)
    real32 bigGunRadiusSq;              // +0x9C (Ship+0x6D4)
    vector avoidPos;                    // +0xA0 (Ship+0x6D8) a random point outside the danger sphere
    CatTimer bigGunTimer;               // +0xAC (Ship+0x6E4) 70 s
    bool32 turnTimerActive;             // +0xBC (Ship+0x6F4)
    CatTimer turnTimer;                 // +0xC0 (Ship+0x6F8) 10 s to face the target before forcing the heading
} RKOSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(RKOSpec, state) == 0x28, "RKO state at Ship+0x660 in cataclysm.exe");
_Static_assert(offsetof(RKOSpec, naggarokMissiles) == 0x6C, "RKO missiles at Ship+0x6A4 in cataclysm.exe");
_Static_assert(offsetof(RKOSpec, bigGunWarning) == 0x8C, "RKO big-gun warning at Ship+0x6C4 in cataclysm.exe");
_Static_assert(offsetof(RKOSpec, turnTimer) == 0xC0, "RKO turn timer at Ship+0x6F8 in cataclysm.exe");
_Static_assert(sizeof(RKOSpec) == 0xD0, "RKOSpec is 0xD0 bytes in cataclysm.exe");
#endif

// RKOSpec.state (RKOHouseKeep 0x005c4260 runs them in order; names chosen here)
#define RKO_Start           0
#define RKO_Search          1
#define RKO_Found           2
#define RKO_Plan            3
#define RKO_Turn            4
#define RKO_Turned          5
#define RKO_WaitStart       6
#define RKO_Wait            7
#define RKO_Waited          8
#define RKO_Go              9
#define RKO_Approach        10
#define RKO_Arrived         11
#define RKO_Arrived2        12
#define RKO_CheckTarget     13
#define RKO_Grab            14
#define RKO_Swallow         15
#define RKO_Eating          16
#define RKO_Eaten           17

extern CustShipHeader RKOHeader;

bool32 cat_vecWithinDistance(vector *a, vector *b, real32 dist);           // 0x005c3e00
bool32 cat_vecWithinDistanceSq(vector *a, vector *b, real32 distsq);       // 0x005c3e50
void cat_shipReleaseAttachedRammers(Ship *ship);                            // 0x005c4130
bool32 cat_shipIsRKO(Ship *ship);                                           // 0x005c54c0
sdword cat_rkoGetNaggarokMissiles(Ship *rko);                               // 0x005c54e0
void cat_rkoDecNaggarokMissiles(Ship *rko);                                 // 0x005c54f0
void cat_rkoSetBigGunPos(Ship *rko, vector *pos);                           // 0x005c5500
bool32 cat_rkoGetAvoidLocation(Ship *rko, vector *pos, vector *dir);        // 0x005c56b0
bool32 cat_rkoIsSafe(Ship *rko);                                            // 0x005c5700
bool32 cat_rkoBeware(Ship *rko);                                            // 0x005c5760
void cat_rkoNotifyBigGunExplosion(vector *pos);                             // 0x005c5780
void cat_rkoWarnBigGunFire(vector *pos);                                    // 0x005c57c0
void RKOAttackPassive(Ship *ship, Ship *target, bool32 rotate);              // 0x005c59a0 (also the Processor's slot 6)

#endif
