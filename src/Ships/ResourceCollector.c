// =============================================================================
//  ResourceCollector.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#include "ResourceCollector.h"

#include "RepairCorvette.h"
#include "ResCollect.h"
#include "ShipSelect.h"
#include "StatScript.h"

typedef struct
{
    sdword resourcesCollected;
} ResourceCollectorSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(ResourceCollectorSpec) == 4, "ResourceCollectorSpec is 4 bytes in cataclysm.exe (ResourceCollectorHeader sizeofShipSpecifics)");   // CAT
#endif


typedef struct
{
    real32 repairApproachDistance;
} ResourceCollectorStatics;

ResourceCollectorStatics ResourceCollectorStatic;

// CAT: Kushan (0x00a7a608) and every other race (0x00a7a604, i.e. Taiidan: tResourceCollector)
//      (ResourceCollectorStaticInit 0x005c3d90); HW1 had R1/R2
ResourceCollectorStatics ResourceCollectorStaticKushan;
ResourceCollectorStatics ResourceCollectorStaticTaiidan;

scriptStructEntry ResourceCollectorStaticScriptTable[] =
{
    { "repairApproachDistance",    scriptSetReal32CB,  &(ResourceCollectorStatic.repairApproachDistance),  &(ResourceCollectorStatic) },

    END_SCRIPT_STRUCT_ENTRY
};

void ResourceCollectorStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    // CAT: Kushan, else the Taiidan block (ResourceCollectorStaticInit 0x005c3d90)
    ResourceCollectorStatics *resourcestat = (statinfo->shiprace == RACE_Kushan) ? &ResourceCollectorStaticKushan : &ResourceCollectorStaticTaiidan;

    statinfo->custstatinfo = resourcestat;
    scriptSetStruct(directory,filename,ResourceCollectorStaticScriptTable,(ubyte *)resourcestat);

}

void ResourceCollectorInit(Ship *ship)
{
    ResourceCollectorSpec *spec = (ResourceCollectorSpec *)ship->ShipSpecifics;

    spec->resourcesCollected = 0;
}


bool32 ResourceCollectorSpecialTarget(Ship *ship, void *custom)
{
    ResourceCollectorStatics *resourcestat = (ResourceCollectorStatics *)ship->staticinfo->custstatinfo;
    SelectAnyCommand *targets;
    targets = (SelectAnyCommand *)custom;
    return(refuelRepairShips(ship, targets,resourcestat->repairApproachDistance));
}


void ResourceCollectorAttack(Ship *ship, SpaceObjRotImpTarg *target,real32 maxdist)
{
    R1ResourcerAttacksShip(ship,target,FALSE);
}

// CAT: the exe's header (ResourceCollectorHeader 0x008c6378: tResourceCollector; original/scratch-m2/m2r2-shipstatic-tools/headers.txt).
//      Slot 3 is the "specifics word 0 = 0" body the exe folded into MiningBaseInit 0x005b0360 (the same code as
//      ResourceCollectorInit). No slot 21 (HW1: ResourceCollectorAttack): the collector does not attack.
CustShipHeader ResourceCollectorHeader =
{
    .sizeofShipSpecifics = sizeof(ResourceCollectorSpec),
    .CustShipStaticInit = ResourceCollectorStaticInit,
    .CustShipInit = ResourceCollectorInit,
    .CustShipSpecialTarget = ResourceCollectorSpecialTarget,
};

