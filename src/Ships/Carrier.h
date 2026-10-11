// =============================================================================
//  Carrier.h
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#ifndef ___CARRIER_H
#define ___CARRIER_H

#include "BeastWeapon.h"
#include "Salvage.h"
#include "SpaceObj.h"
#include "Types.h"

// CAT: 0x168 bytes in cataclysm.exe (CarrierHeader sizeofShipSpecifics 360), for sCarrier, bCarrier, kCarrier, tCarrier
//      and dKushanCarrier. HW1's drop targets and captured-ship build queue are gone (salvage goes to the depot block).
//      CarrierInit 0x005af090: weapon block (bCarrier) at +0x00, depot at +0x1C (Ship+0x654), the sCarrier door state
//      at +0x160 (Ship+0x798) and its timer at +0x164 (Ship+0x79C).
typedef struct
{
    CatBeastWeaponSpec beastWeapon;     // +0x00 bCarrier's infection weapon
    udword unknown18;                   // +0x18 not touched by the reused HW1 bodies
    CatSalvageDepot depot;              // +0x1C every carrier is a salvage depot
    sdword doorState;                   // +0x160 sCarrier: 1 = open (animation 1), 0 = closed (animation 0)
    real32 doorTimer;                   // +0x164 sCarrier: when to open the door next (-1 = not set)
} CarrierSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(CarrierSpec, depot) == 0x1C, "carrier depot at Ship+0x654 in cataclysm.exe");
_Static_assert(offsetof(CarrierSpec, doorState) == 0x160, "carrier door state at Ship+0x798 in cataclysm.exe");
_Static_assert(sizeof(CarrierSpec) == 0x168, "CarrierSpec is 0x168 bytes in cataclysm.exe");
#endif

extern CustShipHeader CarrierHeader;

// CAT: slot 14 of the Carrier, HeavyCruiser and Mothership headers (CarrierBulletRemoval 0x005af390)
void CarrierBulletRemoval(Ship *ship, struct Bullet *bullet);

#endif
