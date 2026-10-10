// =============================================================================
//  Drone.c
//  - DDDFrigate drones
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#include "Drone.h"

#include "Attack.h"
#include "DefaultShip.h"

typedef struct
{
    udword dummy;
} DroneStatics;

DroneStatics DroneStaticRace1;
DroneStatics DroneStaticRace2;

void DroneStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    DroneStatics *dronestat = (statinfo->shiprace == R1) ? &DroneStaticRace1 : &DroneStaticRace2;

    statinfo->custstatinfo = dronestat;
}

void DroneAttackPassive(Ship *ship,Ship *target,bool32 rotate)
{
    DroneSpec *spec = (DroneSpec *)ship->ShipSpecifics;

    if (spec->droneState != DRONE_STATE_LAUNCHED)
    {
        return;     // only allow fully launched drones to shoot
    }

    if ((rotate) & ((bool32)((ShipStaticInfo *)(ship->staticinfo))->rotateToRetaliate))
    {
        attackPassiveRotate(ship,target);
    }
    else
    {
        attackPassive(ship,target);
    }
}

CustShipHeader DroneHeader =
{
    // CAT: Cataclysm slot order (CustShipHeader, SpaceObj.h; no shiptype field, Attack in slot 21).
    //      The Cataclysm-only slots stay NULL until their bodies are ported (stage 2).
    .sizeofShipSpecifics = sizeof(DroneSpec),
    .CustShipStaticInit = DroneStaticInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = DroneAttackPassive,
};

