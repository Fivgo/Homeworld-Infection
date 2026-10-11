// =============================================================================
//  Clamp.c
//  - deals with latching one spaceobj to another
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 11/5/1997 by bpasechn
// =============================================================================

#include "Clamp.h"

#include "Debug.h"
#include "Memory.h"
#include "Physics.h"
#include "Select.h"
#include "Universe.h"
#include "UnivUpdate.h"


/*-----------------------------------------------------------------------------
    Name        :   clampObjToObj
    Description :   Attaches obj to dest in exact orientation obj is at with
                    respect to dest at call time (freezes position)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/

void clampObjToObj(SpaceObjRotImpTargGuidance *obj,SpaceObjRotImpTargGuidance *dest)
{
    matrix tmpmat;
    vector tmpvec;
    CatClampedList *clamped;

    // CAT: clampObjToObj 0x004346e0: an object keeps its clamp info (it may already be a host), the host records
    //      what is clamped to it in its own clamp info (allocated with host == NULL if it had none) in a list of up
    //      to 1000 objects (memAlloc 0xFA8), which updateClampedObject walks
    if (obj->clampInfo == NULL)
    {
        obj->clampInfo = memAlloc(sizeof(ClampInfo),"ClampingInformation",NonVolatile);
        obj->clampInfo->clampedObjs = NULL;
    }

    obj->clampInfo->host = (SpaceObjRotImpTarg *)dest;

    vecSub(tmpvec,obj->obj.posinfo.position,dest->obj.posinfo.position);
    matMultiplyVecByMat(&obj->clampInfo->clampOffset,&tmpvec,&dest->rotatable.rotinfo.coordsys);

    //Store slaves coordsys with respect to MASTER's object space
    tmpmat = obj->rotatable.rotinfo.coordsys;
    matTranspose(&tmpmat);
    matMultiplyMatByMat(&obj->clampInfo->clampCoordsys,&tmpmat,&dest->rotatable.rotinfo.coordsys);
    matTranspose(&obj->clampInfo->clampCoordsys);

    bitSet(obj->header.flags,SOF_Clamped);

    if (dest->clampInfo == NULL)
    {
        dest->clampInfo = memAlloc(sizeof(ClampInfo),"ClampingInformation",NonVolatile);
        dest->clampInfo->host = NULL;
        dest->clampInfo->clampedObjs = NULL;
    }
    if (dest->clampInfo->clampedObjs == NULL)
    {
        dest->clampInfo->clampedObjs = memAlloc(sizeof(CatClampedList),"ClampedObjects",NonVolatile);
        dest->clampInfo->clampedObjs->numShips = 0;
    }
    clamped = dest->clampInfo->clampedObjs;
    clamped->ShipPtr[clamped->numShips] = (struct Ship *)obj;
    clamped->numShips++;
}

/*-----------------------------------------------------------------------------
    Name        :   unClampObj
    Description :   DeClamps a clamped object
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void unClampObj(SpaceObjRotImpTargGuidance *obj)
{
    SpaceObjRotImpTargGuidance *host;

    // CAT: unClampObj 0x00434830: the object leaves its host's list (the host's clamp info goes when the host is
    //      clamped to nothing and holds nothing), and its own clamp info goes only when nothing is clamped to it
    if (obj->clampInfo == NULL)
    {
        return;
    }

    host = (SpaceObjRotImpTargGuidance *)obj->clampInfo->host;
    obj->clampInfo->host = NULL;
    if ((host != NULL) && (host->clampInfo != NULL))
    {
        selSelectionRemoveSingleShip((MaxSelection *)host->clampInfo->clampedObjs,(Ship *)obj);
        if (host->clampInfo->clampedObjs->numShips == 0)
        {
            memFree(host->clampInfo->clampedObjs);
            host->clampInfo->clampedObjs = NULL;
            if (host->clampInfo->host == NULL)
            {
                memFree(host->clampInfo);
                host->clampInfo = NULL;
                bitClear(host->header.flags,SOF_Clamped);
            }
        }
    }

    if ((obj->clampInfo->clampedObjs != NULL) && (obj->clampInfo->clampedObjs->numShips == 0))
    {
        memFree(obj->clampInfo->clampedObjs);
        obj->clampInfo->clampedObjs = NULL;
    }
    if ((obj->clampInfo->clampedObjs == NULL) && (obj->clampInfo->host == NULL))
    {
        memFree(obj->clampInfo);
        obj->clampInfo = NULL;
        bitClear(obj->header.flags,SOF_Clamped);
    }
}

/*-----------------------------------------------------------------------------
    Name        :   cat_clampUnclampAllFromShip
    Description :   unclamps every object clamped to ship
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void cat_clampUnclampAllFromShip(SpaceObjRotImpTargGuidance *ship)
{
    // CAT: new (cat_clampUnclampAllFromShip 0x00434900): unclamps the first clamped object until the ship's clamp
    //      info has gone or holds none
    //      [audit: the exe tests the list only on entry and then loops while the clamp info exists, so for a ship
    //      that is clamped itself (its clamp info outlives its list) it would read the freed list's NULL + 8; the
    //      port stops when the list is gone, which is the same in every case that does not crash]
    while ((ship->clampInfo != NULL) && (ship->clampInfo->clampedObjs != NULL))
    {
        unClampObj((SpaceObjRotImpTargGuidance *)ship->clampInfo->clampedObjs->ShipPtr[0]);
    }
}

/*-----------------------------------------------------------------------------
    Name        :   updateClampedObject
    Description :   updates A clamped Objects positional status
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void updateClampedObject(SpaceObjRotImpTargGuidance *obj)
{
    sdword i;
    SpaceObjRotImpTargGuidance *clamped;
    vector tmpvec;

    // CAT: updateClampedObject 0x00434940 is called for the HOST: each object clamped to it runs its physics, is
    //      then put at its clamp offset in the host's frame with the host's velocity, and moves its own clamped
    //      objects in turn
    for (i = 0; i < obj->clampInfo->clampedObjs->numShips; i++)
    {
        clamped = (SpaceObjRotImpTargGuidance *)obj->clampInfo->clampedObjs->ShipPtr[i];

        switch (clamped->header.objtype)
        {
            case OBJ_ShipType:      physUpdateObjPosVelShip((Ship *)clamped,universe.phystimeelapsed);  break;
            case OBJ_DerelictType:  physUpdateObjPosVelDerelicts((Derelict *)clamped,universe.phystimeelapsed);  break;
            default:                physUpdateObjPosVelBasic((SpaceObj *)clamped,universe.phystimeelapsed); break;
        }

        //'decode' clamped ship's location in world space with respect to targets
        matMultiplyMatByVec(&tmpvec,&clamped->clampInfo->host->rotatable.rotinfo.coordsys,&clamped->clampInfo->clampOffset);
        vecAddTo(tmpvec, clamped->clampInfo->host->obj.posinfo.position);
        if (clamped->header.objtype == OBJ_ShipType)
        {
            cat_univSetShipPosition((Ship *)clamped, &tmpvec);
        }
        else
        {
            clamped->obj.posinfo.position = tmpvec;
        }

        //'decode' slaves orientation in world space with respect to Masters.
        matMultiplyMatByMat(&clamped->rotatable.rotinfo.coordsys,&clamped->clampInfo->host->rotatable.rotinfo.coordsys, &clamped->clampInfo->clampCoordsys);
        clamped->obj.posinfo.velocity = clamped->clampInfo->host->obj.posinfo.velocity;

        univUpdateObjRotInfo((SpaceObjRot *)clamped);

        if (clamped->clampInfo->clampedObjs != NULL)
        {
            updateClampedObject(clamped);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        :   cat_updateClampedObjectNoPhysics
    Description :   puts a clamped object at its clamp offset without physics (the ships that apply no physics,
                    univUpdateAllPosVelShips)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void cat_updateClampedObjectNoPhysics(SpaceObjRotImpTargGuidance *obj)
{
    vector tmpvec;

    // CAT: new (cat_updateClampedObjectNoPhysics 0x00434ab0); the position is written directly
    matMultiplyMatByVec(&tmpvec,&obj->clampInfo->host->rotatable.rotinfo.coordsys,&obj->clampInfo->clampOffset);
    vecAdd(obj->obj.posinfo.position, obj->clampInfo->host->obj.posinfo.position, tmpvec);
    matMultiplyMatByMat(&obj->rotatable.rotinfo.coordsys,&obj->clampInfo->host->rotatable.rotinfo.coordsys, &obj->clampInfo->clampCoordsys);
    obj->obj.posinfo.velocity = obj->clampInfo->host->obj.posinfo.velocity;
    univUpdateObjRotInfo((SpaceObjRot *)obj);
}
