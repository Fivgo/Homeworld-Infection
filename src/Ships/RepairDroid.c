// =============================================================================
//  RepairDroid.c
//  - Cataclysm's sRepairDroid / bRepairDroid ship type
//    (cataclysm.exe RepairDroidStaticInit 0x005c29b0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b).

#include "RepairDroid.h"

#include "Attack.h"
#include "StatScript.h"

AttackSideStepParameters RepairDroidStatic;     // 0x00a7a610 (both races)

void RepairDroidStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)     // 0x005c29b0
{
    scriptSetStruct(directory,filename,AttackSideStepParametersScriptTable,(ubyte *)&RepairDroidStatic);
    statinfo->custstatinfo = &RepairDroidStatic;
}

// CAT: RepairDroidHeader 0x008c6170 (sRepairDroid, bRepairDroid): slot 1 only
CustShipHeader RepairDroidHeader =
{
    .sizeofShipSpecifics = sizeof(RepairDroidSpec),
    .CustShipStaticInit = RepairDroidStaticInit,
};
