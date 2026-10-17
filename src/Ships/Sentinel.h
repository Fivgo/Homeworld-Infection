// =============================================================================
//  Sentinel.h
//  - Cataclysm's sSentinel / bSentinel: in a force-field formation they span
//    a shield of triangles that soaks up hits (cataclysm.exe 0x005c5af0-0x005c6fa0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___SENTINEL_H
#define ___SENTINEL_H

#include <stddef.h>

#include "SpaceObj.h"

#define SENTINEL_MAX_LINKS      100

// one triangle of the field: the Sentinel and two partners (the formation code fills the list)
typedef struct
{
    struct Ship *a;
    struct Ship *b;
} SentinelLink;

// CAT: new file (M2-R4 ships-b). 0x350 bytes in cataclysm.exe (SentinelHeader 0x008c6830 sizeofShipSpecifics 848)
typedef struct SentinelSpec
{
    bool32 fieldActive;                 // +0x000 (Ship+0x638)
    real32 fieldLevel;                  // +0x004 (Ship+0x63C) 0 .. 1, the field is up at 1.0
    sdword numLinks;                    // +0x008 (Ship+0x640)
    SentinelLink links[SENTINEL_MAX_LINKS];     // +0x00C (Ship+0x644) filled by the force-field formations (Formation.c)
    sdword chargeLevel;                 // +0x32C (Ship+0x964) the row of the charge tables (0x008c6808 / 0x008c6814)
    real32 lastHitTime;                 // +0x330 (Ship+0x968)
    real32 fade;                        // +0x334 (Ship+0x96C) 1.0 when the field comes up, falls to 0 (the renderer)
    udword unknown338;                  // +0x338 (Ship+0x970) cleared by SentinelInit
    real32 charge;                      // +0x33C (Ship+0x974) hits take it away; it recharges
    bool32 depleted;                    // +0x340 (Ship+0x978) the field is down until fully recharged
    bool32 disrupted;                   // +0x344 (Ship+0x97C) a DFG frigate's field (DFGFrigateHouseKeep)
    real32 disruptEndTime;              // +0x348 (Ship+0x980) cat_univShipDeferredUpdate clears disrupted after it
    bool32 inPosition;                  // +0x34C (Ship+0x984) at its formation slot
} SentinelSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(SentinelSpec, chargeLevel) == 0x32C, "Sentinel charge level at Ship+0x964 in cataclysm.exe");
_Static_assert(offsetof(SentinelSpec, disrupted) == 0x344, "Sentinel disruption at Ship+0x97C in cataclysm.exe");
_Static_assert(sizeof(SentinelSpec) == 0x350, "SentinelSpec is 0x350 bytes in cataclysm.exe");
#endif

extern CustShipHeader SentinelHeader;

bool32 cat_shipIsLiveSentinel(Ship *ship);                                      // 0x005c5af0
real32 cat_sentinelFieldChargeFraction(SentinelSpec *spec);                     // 0x005c5ea0
void cat_sentinelFieldBulletHit(Ship *sentinel, void *unused, Bullet *bullet);            // 0x005c6c60
void cat_sentinelFieldMissileHit(Ship *sentinel, void *unused, Missile *missile);          // 0x005c6cd0
bool32 cat_shipIsSentinelInSphere(Ship *ship);                                  // 0x005c6e80
sdword cat_sentinelFieldLinks(Ship *sentinel, SentinelLink **links);            // CAT: the list the exe reads inline

#endif
