// =============================================================================
//  SalCapCorvette.h
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 11/5/1997 by khent
// =============================================================================

#ifndef ___SALCAPCORVETTE_H
#define ___SALCAPCORVETTE_H

#include <stddef.h>

#include "Attack.h"
#include "Salvage.h"
#include "ShipSelect.h"
#include "render.h"

#define SALVAGE_AT_GET_TECH       0x01

#define SAL_WL_FLYTOCONEORIGIN    7

typedef struct
{
    AttackSideStep attacksidestep;
    sdword salvageState;               //sal/cap corv aistate for salvaging
    ShipOrDerelict *target;
    Ship *dockwith;
    sdword dockindex;
    sdword salvageIndex;
    sdword salvageAttributes;
    real32 timeCounter;
    real32 getTechTime;
    sdword tractorBeam;
    sdword noAvoid;
    real32 noDamageTargetTime;
    sdword groupme;
	ShipOrDerelict *noDamageTarget;
} SalCapCorvetteSpec;

typedef struct
{
    AttackSideStepParameters sidestepParameters;
    real32 HealthThreshold;
    real32 healthRemovedPerSecond;
    real32 getTechTime;
    real32 flyToDistance;
    real32 maxPushingVelocitySingle;
    real32 noLightClampingDistance;
} SalCapCorvetteStatics;

// CAT: the exe's specifics (SalCapCorvetteHeader sizeofShipSpecifics 48): the side-step state and the salvager
//      block at +0x08 (cat_salvageGetSalvagerInfo 0x004f8ec0 returns Ship+0x640)
typedef struct
{
    AttackSideStep attacksidestep;      // +0x00
    CatSalvagerInfo salvager;           // +0x08
} CatSalCapCorvetteSpec;
_Static_assert(offsetof(CatSalCapCorvetteSpec, salvager) == 0x08, "SalCap salvager block at Ship+0x640 in cataclysm.exe");
_Static_assert(sizeof(CatSalCapCorvetteSpec) == 0x30, "the exe's SalCapCorvetteSpec is 0x30 bytes");

extern CustShipHeader SalCapCorvetteHeader;

bool32 DropTargetInShip(Ship *dockwith,sdword *targetDepotState, ShipOrDerelict *target,sdword *dockindex);
bool32 isThereAnotherTargetForMe(Ship *ship,SelectAnyCommand *targets);
void salCapCleanUpCloakingTarget(Ship *ship, Ship *shiptoremove);
void salCapClearTechBool();
void SalCapDropTarget(Ship *ship);
void salCapExtraSpecialOrderCleanUp(SelectCommand *selection,udword ordertype,Ship *dockwith,SelectCommand *targets);
void salCapHarvestTarget(ShipOrDerelict *target,Ship *dockwith);
sdword salCapNeedBig(Ship *ship,ShipOrDerelict *target);
void SalCapOrderChangedCleanUp(Ship *ship);
void salCapRemoveDerelictReferences(Ship *ship,Derelict *d);

#if RND_VISUALIZATION
void dockDrawSalvageInfo(ShipOrDerelict *obj);
#endif

#endif
