// =============================================================================
//  GenericInterceptor.h
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#ifndef ___GENERICINTERCEPTOR_H
#define ___GENERICINTERCEPTOR_H

#include <stddef.h>

#include "ClassDefs.h"
#include "FlightMan.h"
#include "ObjTypes.h"
#include "Types.h"

#define STATE_INIT              0
#define STATE_APPROACH          1
#define STATE_KILL              2
#define STATE_BREAK             3
#define STATE_TURNAROUND        4
#define STATE_REAPPROACH        5

#define STATE_WHIPSTRAFE1       10
#define STATE_WHIPSTRAFE2       11
#define STATE_BARRELROLL_OUT    12
#define STATE_START_BOMBING_RUN 13
#define STATE_BOMBING_DIVE      14

#define isShipInterceptor(ship) (((ShipStaticInfo *)((ship)->staticinfo))->custshipheader.CustShipAttack == GenericInterceptorAttack)
#define isShipStaticInterceptor(shipstatic) ((shipstatic)->custshipheader.CustShipAttack == GenericInterceptorAttack)

typedef struct
{
    real32 firetime;
    real32 maxFlyAwayDist[NUM_TACTICS_TYPES][NUM_CLASSES+1];
    real32 maxFlyAwayDistSquared[NUM_TACTICS_TYPES][NUM_CLASSES+1];    // use NUM_CLASSES entry as DefaultClass (so array len = NUM_CLASSES+1)
    real32 breakRange[NUM_TACTICS_TYPES][NUM_CLASSES+1];
    real32 flyPastDist[NUM_TACTICS_TYPES][NUM_CLASSES+1];
    real32 triggerHappy[NUM_TACTICS_TYPES][NUM_CLASSES+1];
    real32 faceTargetAccuracy[NUM_TACTICS_TYPES][NUM_CLASSES+1];
    real32 maxAttackTime[NUM_TACTICS_TYPES][NUM_CLASSES+1];
    FlightManProb flightmanProb[NUM_TACTICS_TYPES][NUM_CLASSES+1];
} GenericInterceptorStatics;

// CAT: 0x3640 bytes in cataclysm.exe (GenericInterceptorStaticInit 0x005b1e70 clears 0xD90 dwords; key table
//      0x008c4378: maxFlyAwayDist +0x4, breakRange +0x13C, flyPastDist +0x1D8, triggerHappy +0x274,
//      faceTargetAccuracy +0x310, maxAttackTime +0x3AC, flightmanProb +0x448, FlightManProb 0x148), with 13 = 12
//      classes + default per tactics row. Only real32/udword members, so the size is the same on LP64.
_Static_assert(sizeof(FlightManProb) == 0x148, "FlightManProb is 0x148 bytes in cataclysm.exe");
_Static_assert(sizeof(GenericInterceptorStatics) == 0x3640, "GenericInterceptorStatics is 0x3640 bytes in cataclysm.exe");
_Static_assert(offsetof(GenericInterceptorStatics, breakRange) == 0x13C, "breakRange at +0x13C in cataclysm.exe");
_Static_assert(offsetof(GenericInterceptorStatics, triggerHappy) == 0x274, "triggerHappy at +0x274 in cataclysm.exe");
_Static_assert(offsetof(GenericInterceptorStatics, maxAttackTime) == 0x3AC, "maxAttackTime at +0x3AC in cataclysm.exe");
_Static_assert(offsetof(GenericInterceptorStatics, flightmanProb) == 0x448, "flightmanProb at +0x448 in cataclysm.exe");

extern CustShipHeader AttackBomberHeader;
extern CustShipHeader CloakedFighterHeader;
extern CustShipHeader GenericInterceptorHeader;

void GenericInterceptorAttack(Ship *ship, SpaceObjRotImpTarg *target, real32 maxdist);
// CAT: the cloak slots the exe shares with P1IonArrayFrigateHeader (slots 7 and 10)
bool32 CloakedFighterSpecialActivate(Ship *ship);
void CloakedFighterHouseKeep(Ship *ship);
bool32 GenericInterceptorCanFire(Ship *ship, SpaceObjRotImpTarg *target, vector *trajectory, real32 triggerHappy);
FlightManProb *GenericInterceptorGetFlightManProb(Ship *ship, SpaceObjRotImpTarg *target);
bool32 InterceptorInRange(Ship *ship, SpaceObjRotImpTarg *target);
void SpawnCloakingEffect(Ship *ship, etglod *etgLOD);

#endif
