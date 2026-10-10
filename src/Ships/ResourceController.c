// =============================================================================
//  ResourceController.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#include "ResourceController.h"

#include "RepairCorvette.h"
#include "ShipSelect.h"
#include "StatScript.h"

typedef struct
{
    sdword dummy;
} ResourceControllerSpec;

typedef struct
{
    real32 repairApproachDistance;
} ResourceControllerStatics;

ResourceControllerStatics ResourceControllerStatic;

ResourceControllerStatics ResourceControllerStaticRace1;
ResourceControllerStatics ResourceControllerStaticRace2;


scriptStructEntry RCStaticScriptTable[] =
{
    { "repairApproachDistance",    scriptSetReal32CB,  &(ResourceControllerStatic.repairApproachDistance),  &(ResourceControllerStatic) },

    END_SCRIPT_STRUCT_ENTRY
};


void ResourceControllerStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    ResourceControllerStatics *resourcestat = (statinfo->shiprace == R1) ? &ResourceControllerStaticRace1 : &ResourceControllerStaticRace2;

    statinfo->custstatinfo = resourcestat;
    scriptSetStruct(directory,filename,RCStaticScriptTable,(ubyte *)resourcestat);
}

bool32 ResourceControllerSpecialTarget(Ship *ship, void *custom)
{
    ResourceControllerStatics *rcstat = (ResourceControllerStatics *)ship->staticinfo->custstatinfo;
    SelectAnyCommand *targets;

    targets = (SelectAnyCommand *)custom;
    if(targets->numTargets == 0)
            return TRUE;
    //fix later: sort from closest to furthest in command layer once!
    //also need to remove non-ships from selection

    return(refuelRepairShips(ship, targets,rcstat->repairApproachDistance));

}

CustShipHeader ResourceControllerHeader =
{
    // CAT: Cataclysm slot order (CustShipHeader, SpaceObj.h; no shiptype field, Attack in slot 21).
    //      The Cataclysm-only slots stay NULL until their bodies are ported (stage 2).
    .sizeofShipSpecifics = sizeof(ResourceControllerSpec),
    .CustShipStaticInit = ResourceControllerStaticInit,
    .CustShipSpecialTarget = ResourceControllerSpecialTarget,
};

