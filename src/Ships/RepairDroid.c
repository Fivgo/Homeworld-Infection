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

#include "glinc.h"
#include "LOD.h"
#include "Matrix.h"
#include "Mesh.h"

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

// CAT (M2-R5 render): the active droids crawl over a ship drawn at LOD 0 or 1, each at its own place and orientation
// (the first mesh of its type's LOD 0)
void cat_droidRenderArray(CatRepairDroidArray *array)                                  // 0x005c3cf0
{
    static hmatrix droidMatrix;                                                         // 0x00a77b10
    sdword i;
    Ship *droid;

    if (array->owner->obj.currentLOD >= 2)
    {
        return;
    }
    for (i = 0; i < array->numDroids; i++)
    {
        if (!array->droids[i].active)
        {
            continue;
        }
        droid = array->droids[i].ship;
        glPushMatrix();
        glTranslatef(droid->obj.posinfo.position.x, droid->obj.posinfo.position.y, droid->obj.posinfo.position.z);
        hmatMakeHMatFromMat(&droidMatrix, &droid->rotatable.rotinfo.coordsys);
        glMultMatrixf((GLfloat *)&droidMatrix);
        cat_meshRenderState(NULL, (meshdata *)droid->staticinfo->common.LOD->level[0].pData, 0);
        glPopMatrix();
    }
}
