/*=============================================================================
    Name    : Mex.c
    Purpose : Mesh extensions

    Created 7/18/1997 by gshaw
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include <string.h>

#if !defined _MSC_VER
#include <strings.h>
#endif

#include <math.h>
#include "Matrix.h"
#include "Memory.h"
#include "Debug.h"
#include "File.h"
#include "MEX.h"
#include "Color.h"
#include "FastMath.h"

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

bool32 mexVerify(void *mex)
{
    return (strcasecmp((char *)mex,"mannngo") == 0);
}

void *mexLoad(char *filename)
{
    void *address;

#if FIX_ENDIAN
	MEXFileHeader *mex;
	MEXChunk *chunk;
	unsigned char i;
#endif

    fileLoadAlloc(filename,&address,NonVolatile);

#if FIX_ENDIAN
	mex          = ( MEXFileHeader *)address;
	mex->version = FIX_ENDIAN_INT_16( mex->version );
	mex->nChunks = FIX_ENDIAN_INT_16( mex->nChunks );

	chunk = ( MEXChunk *)( ( ( ubyte *)mex ) + sizeof( MEXFileHeader ) );
	for( i=0; i<mex->nChunks; i++ )
	{
		chunk->chunkSize = FIX_ENDIAN_INT_32( chunk->chunkSize );

		if(( strcasecmp( chunk->type, "Gun" ) == 0 )
	    || ( strcasecmp( chunk->type, "Eng" ) == 0 )
		|| ( strcasecmp( chunk->type, "Dok" ) == 0 )
		|| ( strcasecmp( chunk->type, "Sal" ) == 0 ) )
		{
			MEXGunChunk *chunk1 = ( MEXGunChunk *)chunk;

			chunk1->position.x = FIX_ENDIAN_FLOAT_32( chunk1->position.x );
			chunk1->position.y = FIX_ENDIAN_FLOAT_32( chunk1->position.y );
			chunk1->position.z = FIX_ENDIAN_FLOAT_32( chunk1->position.z );

			chunk1->normal.x = FIX_ENDIAN_FLOAT_32( chunk1->normal.x );
			chunk1->normal.y = FIX_ENDIAN_FLOAT_32( chunk1->normal.y );
			chunk1->normal.z = FIX_ENDIAN_FLOAT_32( chunk1->normal.z );

			chunk1->coneAngle = FIX_ENDIAN_FLOAT_32( chunk1->coneAngle );
			chunk1->edgeAngle = FIX_ENDIAN_FLOAT_32( chunk1->edgeAngle );
		}

		if( strcasecmp( chunk->type, "Col" ) == 0 )
		{
			MEXCollisionSphereChunk *chunk1 = ( MEXCollisionSphereChunk *)chunk;

			chunk1->level = FIX_ENDIAN_INT_32( chunk1->level );

			chunk1->offset.x = FIX_ENDIAN_FLOAT_32( chunk1->offset.x );
			chunk1->offset.y = FIX_ENDIAN_FLOAT_32( chunk1->offset.y );
			chunk1->offset.z = FIX_ENDIAN_FLOAT_32( chunk1->offset.z );

			chunk1->r = FIX_ENDIAN_FLOAT_32( chunk1->r );
		}
		
		if( strcasecmp( chunk->type, "Rct" ) == 0 )
		{
			MEXCollisionRectangleChunk *chunk1 = ( MEXCollisionRectangleChunk *)chunk;

			chunk1->level = FIX_ENDIAN_INT_32( chunk1->level );

			chunk1->ox = FIX_ENDIAN_FLOAT_32( chunk1->ox );
			chunk1->oy = FIX_ENDIAN_FLOAT_32( chunk1->oy );
			chunk1->oz = FIX_ENDIAN_FLOAT_32( chunk1->oz );

			chunk1->dx = FIX_ENDIAN_FLOAT_32( chunk1->dx );
			chunk1->dy = FIX_ENDIAN_FLOAT_32( chunk1->dy );
			chunk1->dz = FIX_ENDIAN_FLOAT_32( chunk1->dz );
		}

		if( strcasecmp( chunk->type, "Nav" ) == 0 )
		{
			MEXNAVLightChunk *chunk1 = ( MEXNAVLightChunk *)chunk;

			chunk1->position.x = FIX_ENDIAN_FLOAT_32( chunk1->position.x );
			chunk1->position.y = FIX_ENDIAN_FLOAT_32( chunk1->position.y );
			chunk1->position.z = FIX_ENDIAN_FLOAT_32( chunk1->position.z );
		}

		chunk = ( MEXChunk *)( ( ( ubyte *)chunk ) + sizeof( MEXChunk ) + chunk->chunkSize );
	}
#endif // FIX_ENDIAN

    dbgAssertOrIgnore(address != NULL);
    if (!mexVerify(address))
    {
        dbgFatal(DBG_Loc,"Invalid MEX file");
        return NULL;
    }
    return address;
}

void mexFree(void *mex)
{
    memFree(mex);
}

void *mexGetChunk(void *mex,char *type,char *name)
{
    MEXFileHeader *header = (MEXFileHeader *)mex;
    MEXChunk *curmexchunk = (MEXChunk *) ( ((ubyte *)mex) + sizeof(MEXFileHeader) );
    uword numChunks = header->nChunks;
    uword i;

    for (i=0;i<numChunks;i++)
    {
        if (strcasecmp(curmexchunk->type,type) == 0)
        {
            if (strcasecmp(curmexchunk->name,name) == 0)
            {
                return (void *)curmexchunk;
            }
        }

        curmexchunk = (MEXChunk *)(((ubyte *)curmexchunk) + sizeof(MEXChunk) + curmexchunk->chunkSize);
    }

    return NULL;
}

MEXDockingChunk *mexGetDockChunk(void *mex,char *dockname)
{
    MEXFileHeader *header = (MEXFileHeader *)mex;
    MEXChunk *curmexchunk = (MEXChunk *) ( ((ubyte *)mex) + sizeof(MEXFileHeader) );
    uword numChunks = header->nChunks;
    uword i;

    for (i=0;i<numChunks;i++)
    {
        if (strcasecmp(curmexchunk->type,"Dok") == 0)
        {
            if (strcasecmp(curmexchunk->name,"Dock") == 0)
            {
                if (strcasecmp(((MEXDockingChunk *)curmexchunk)->dockName,dockname) == 0)
                {
                    return (MEXDockingChunk *)curmexchunk;
                }
            }
        }

        // CAT: a "Sal"/"SALVAGE" chunk is a dock point too (mexGetDockChunk 0x004b7190): Cataclysm's salvage points
        //      are dock points of type DPT_Salvage/DPT_SalvageLatch, looked up by the same name at the same offset
        if (strcasecmp(curmexchunk->type,"Sal") == 0)
        {
            if (strcasecmp(curmexchunk->name,"SALVAGE") == 0)
            {
                if (strcasecmp(((MEXDockingChunk *)curmexchunk)->dockName,dockname) == 0)
                {
                    return (MEXDockingChunk *)curmexchunk;
                }
            }
        }

        curmexchunk = (MEXChunk *)(((ubyte *)curmexchunk) + sizeof(MEXChunk) + curmexchunk->chunkSize);
    }

    return NULL;
}

MEXSalvageChunk *mexGetSalChunk(void *mex,char *salvagename)
{
    MEXFileHeader *header = (MEXFileHeader *)mex;
    MEXChunk *curmexchunk = (MEXChunk *) ( ((ubyte *)mex) + sizeof(MEXFileHeader) );
    uword numChunks = header->nChunks;
    uword i;

    for (i=0;i<numChunks;i++)
    {
        if (strcasecmp(curmexchunk->type,"Sal") == 0)
        {
            if (strcasecmp(curmexchunk->name,"SALVAGE") == 0)
            {
                if (strcasecmp(((MEXSalvageChunk *)curmexchunk)->Name,salvagename) == 0)
                {
                    return (MEXSalvageChunk *)curmexchunk;
                }
            }
        }

        curmexchunk = (MEXChunk *)(((ubyte *)curmexchunk) + sizeof(MEXChunk) + curmexchunk->chunkSize);
    }

    return NULL;
}


MEXNAVLightChunk *mexGetNAVLightChunk(void *mex,char *navLightName)
{
    MEXFileHeader *header = (MEXFileHeader *)mex;
    MEXChunk *curmexchunk = (MEXChunk *) ( ((ubyte *)mex) + sizeof(MEXFileHeader) );
    uword numChunks = header->nChunks;
    uword i;

    for (i=0;i<numChunks;i++)
    {
        if (strcasecmp(curmexchunk->type,"Nav") == 0)
        {
            if (strcasecmp(curmexchunk->name,"Nav") == 0)
            {
                if (strcasecmp(((MEXNAVLightChunk *)curmexchunk)->NAVLightName,navLightName) == 0)
                {
                    return (MEXNAVLightChunk *)curmexchunk;
                }
            }
        }

        curmexchunk = (MEXChunk *)(((ubyte *)curmexchunk) + sizeof(MEXChunk) + curmexchunk->chunkSize);
    }

    return NULL;
}

ResNozzleStatic *mexGetResNozzleStatic(void *mex)
{
    MEXGunChunk *gunchunk;
    ResNozzleStatic *resnozzle = NULL;

    if (mex == NULL)
    {
        dbgMessage("WARNING: ResNozzle info not retrieved. Mesh extensions not loaded");
        return NULL;
    }

    gunchunk = mexGetChunk(mex,"Gun","Gun0");       // just use gun chunk for now
    if (gunchunk != NULL)
    {
        resnozzle = memAlloc(sizeof(ResNozzleStatic),"resnozzle",NonVolatile);

        resnozzle->position = gunchunk->position;

        mexVecToVec(&resnozzle->nozzlenormal,&gunchunk->normal);

        return resnozzle;
    }

    return NULL;
}

RepairNozzleStatic *mexGetRepairNozzleStatic(void *mex)
{
    MEXGunChunk *gunchunk;
    RepairNozzleStatic *resnozzle = NULL;

    if (mex == NULL)
    {
        dbgMessage("WARNING: RepairNozzle info not retrieved. Mesh extensions not loaded");
        return NULL;
    }

    gunchunk = mexGetChunk(mex,"Gun","Rep0");       // just use gun chunk for now
    if (gunchunk != NULL)
    {
        resnozzle = memAlloc(sizeof(RepairNozzleStatic),"repairnozzle",NonVolatile);

        resnozzle->position = gunchunk->position;

        return resnozzle;
    }

    return NULL;
}

TractorBeamStatic *mexGetTractorBeamStatic(void *mex)
{
    MEXGunChunk *gunchunk;
    TractorBeamStatic *tractoremitter = NULL;

    if (mex == NULL)
    {
        dbgMessage("WARNING: ResNozzle info not retrieved. Mesh extensions not loaded");
        return NULL;
    }

    gunchunk = mexGetChunk(mex,"Gun","Beam0");       // just use gun chunk for now
    if (gunchunk != NULL)
    {
        tractoremitter = memAlloc(sizeof(TractorBeamStatic),"tractoremitter",NonVolatile);

        tractoremitter->position = gunchunk->position;

        mexVecToVec(&tractoremitter->nozzlenormal,&gunchunk->normal);

        return tractoremitter;
    }

    return NULL;
}

void mexGetGunStaticInfo(GunStaticInfo *gunstaticinfo,void *mex)
{
    sdword i;
    MEXGunChunk *gunchunk;
    char gunname[8];
    sdword numGuns = gunstaticinfo->numGuns;
    GunStatic *gunstatic;

    if (mex == NULL)
    {
//        dbgFatal(DBG_Loc,"Could not get gun info because mesh extensions were not loaded");
        dbgMessage("WARNING: Gun info not retrieved. Mesh extensions not loaded");
        return;
    }

    for (i=0;i<numGuns;i++)
    {
        sprintf(gunname,"Gun%d",i);
        gunchunk = mexGetChunk(mex,"Gun",gunname);
        if (gunchunk != NULL)
        {
            gunstatic = &gunstaticinfo->gunstatics[i];

            gunstatic->position = gunchunk->position;

            mexVecToVec(&gunstatic->gunnormal,&gunchunk->normal);

            if ((gunstatic->cosminAngleFromNorm == 0.0f) && (gunstatic->cosmaxAngleFromNorm == 0.0f))
            {
                gunstatic->cosminAngleFromNorm = (real32)cos(DEG_TO_RAD(gunchunk->edgeAngle));
                gunstatic->cosmaxAngleFromNorm = (real32)cos(DEG_TO_RAD(gunchunk->coneAngle + gunchunk->edgeAngle));
            }
        }
        else
        {
            dbgMessagef("WARNING: Could not find gun in mex at %x",mex);
        }
    }
}

void mexGetDockStaticInfo(DockStaticInfo *dockstaticinfo,void *mex)
{
    sdword i;
    MEXDockingChunk *dockchunk;
    sdword numDockPoints = dockstaticinfo->numDockPoints;
    DockStaticPoint *dockstaticpoint;

    if (mex == NULL)
    {
        dbgMessage("WARNING: Dock info not retrieved. Mesh extensions not loaded");
        return;
    }

    for (i=0;i<numDockPoints;i++)
    {
        dockstaticpoint = &dockstaticinfo->dockstaticpoints[i];
        dockstaticpoint->dockindex = i;
        dockchunk = mexGetDockChunk(mex,dockstaticpoint->name);
        if (dockchunk != NULL)
        {
            dockstaticpoint->position = dockchunk->position;

            mexVecToVec(&dockstaticpoint->conenormal,&dockchunk->normal);

            dockstaticpoint->coneangle = (real32)cos(DEG_TO_RAD(dockchunk->coneAngle + dockchunk->edgeAngle));
        }
        else
        {
            // CAT: a point with no chunk gets position 0, cone normal (1,0,0) and a 90-degree cone (mexGetDockStaticInfo
            //      0x004b7560: cos of the double 1.5707963705062866 at 0x0072af58); HW1 left the zeroed values
            dockstaticpoint->position.x = 0.0f;
            dockstaticpoint->position.y = 0.0f;
            dockstaticpoint->position.z = 0.0f;
            dockstaticpoint->conenormal.x = 1.0f;
            dockstaticpoint->conenormal.y = 0.0f;
            dockstaticpoint->conenormal.z = 0.0f;
            dockstaticpoint->coneangle = (real32)cos(1.5707963705062866);
            dbgMessagef("WARNING: Could not find dock point in mex at %p",mex);
        }
    }
}

void mexGetSalvageStaticInfo(SalvageStaticInfo *salvagestaticinfo,void *mex)
{
    sdword i;
    MEXSalvageChunk *salchunk;
    sdword numSalvagePoints = salvagestaticinfo->numSalvagePoints;
    SalvageStaticPoint *salvagestaticpoint;

    if (mex == NULL)
    {
        dbgMessage("WARNING: Salvage info not retrieved. Mesh extensions not loaded");
        return;
    }

    for (i=0;i<numSalvagePoints;i++)
    {
        salvagestaticpoint = &salvagestaticinfo->salvageStaticPoints[i];
        salvagestaticpoint->number = i;
        salchunk = mexGetSalChunk(mex,salvagestaticpoint->name);
        if (salchunk != NULL)
        {
            salvagestaticpoint->position = salchunk->position;

            mexVecToVec(&salvagestaticpoint->conenormal,&salchunk->normal);

            salvagestaticpoint->coneangle = (real32)cos(DEG_TO_RAD(salchunk->coneAngle + salchunk->edgeAngle));
        }
        else
        {
            dbgMessagef("WARNING: Could not find salvage point in mex at %x",mex);
        }
    }
}

void mexGetNAVLightStaticInfo(NAVLightStaticInfo *navlightstaticinfo,StaticInfo *staticheader)
{
    sdword i;
    MEXNAVLightChunk *navlightchunk;
    sdword numNAVLights = navlightstaticinfo->numNAVLights;
    NAVLightStatic *navlightstatic;
    void *mex = staticheader->pMexData;
    lodinfo *LOD;
    sdword level, object;

    if (mex == NULL)
    {
        dbgMessage("WARNING: NAV Light info not retrieved. Mesh extensions not loaded");
        return;
    }

    for (i=0;i<numNAVLights;i++)
    {
        navlightstatic = &navlightstaticinfo->navlightstatics[i];
        navlightchunk = mexGetNAVLightChunk(mex,navlightstatic->name);
        if (navlightchunk != NULL)
        {
            navlightstatic->position = navlightchunk->position;
            navlightstatic->color = colRGB(navlightchunk->red,
                                           navlightchunk->green,
                                           navlightchunk->blue);

            // CAT: for each LOD level, the index of the polygon object the chunk names (case-insensitive), or 0xFF
            //      when it names none or the level has no such object (mexGetNAVLightStaticInfo 0x004b7680, the same
            //      walk as mexSetEngineNozzle 0x004b7c20). The exe writes LOD->nLevels bytes unbounded; the port caps
            //      the walk at the 8 bytes of navlightObject and treats a level without mesh or bindings as "none"
            LOD = staticheader->LOD;
            if (LOD != NULL)
            {
                for (level = 0; (level < LOD->nLevels) && (level < (sdword)sizeof(navlightstatic->navlightObject)); level++)
                {
                    meshdata *mesh = (meshdata *)LOD->level[level].pData;
                    object = -1;
                    if ((navlightchunk->objectName[0] != 0) && (mesh != NULL) && (LOD->level[level].hBindings != NULL))
                    {
                        for (object = 0; object < mesh->nPolygonObjects; object++)
                        {
                            if (strcasecmp(LOD->level[level].hBindings[object].object->pName, navlightchunk->objectName) == 0)
                            {
                                break;
                            }
                        }
                        if (object >= mesh->nPolygonObjects)
                        {
                            object = -1;
                        }
                    }
                    navlightstatic->navlightObject[level] = (ubyte)object;
                }
            }
        }
        else
        {
            dbgMessagef("WARNING: Could not find NAVLight point in mex at %p",mex);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : mexVecToVec
    Description : Convert a mex vector (rotation in x, y, z) to a real vector
    Inputs      : dest - where to store new vector
                  source - rotation of x, y, z
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void mexVecToVec(vector *dest, vector *source)
{
    *dest = *source;  //!!! pass straight through !!!
    if (dest->x == 0.0f && dest->y == 0.0f && dest->z == 0.0f)
    {
        dest->x = 1.0f;
    }
    else
    {
        vecNormalize(dest);     //!!! normalize just for now
    }
}

void mexSetCollisionInfo(StaticCollInfo *staticCollInfo,void *mex,real32 forwardScale,real32 upScale,real32 rightScale, real32 xslide, real32 yslide, real32 zslide)
{
    MEXCollisionSphereChunk *spherechunk;
    MEXCollisionRectangleChunk *rectchunk;
    real32 maxScale;
    real32 rightforwardSqr, upSqr;

    if (mex == NULL)
    {
        return;
    }

    //set uninitialized scale factors to 1.0f;
    if(forwardScale == 0.0f)
        forwardScale = 1.0f;
    if(upScale == 0.0f)
        upScale = 1.0f;
    if(rightScale == 0.0f)
        rightScale = 1.0f;

    // CAT: the sphere radius is scaled by the largest of the three box scales, the avoidance sphere gets 3 units on
    //      top, and the padded avoidance sphere is left alone (mexSetCollisionInfo 0x004b7880; 3.0 at 0x0072a534)
    maxScale = max(upScale, forwardScale);
    if (maxScale <= max(rightScale, upScale))
    {
        maxScale = max(rightScale, upScale);
    }

    spherechunk = mexGetChunk(mex,"Col","Col0");
    if (spherechunk != NULL)
    {
        staticCollInfo->originalCollSphereSize = maxScale * spherechunk->r;
        staticCollInfo->collspheresize = maxScale * spherechunk->r;
        staticCollInfo->approxcollspheresize = maxScale * spherechunk->r;
        staticCollInfo->avoidcollspheresize = maxScale * spherechunk->r + 3.0f;
        staticCollInfo->collsphereoffset = spherechunk->offset;
        staticCollInfo->collsphereoffset.x += xslide;
        staticCollInfo->collsphereoffset.y += yslide;
        staticCollInfo->collsphereoffset.z += zslide;
        staticCollInfo->collspheresizeSqr = staticCollInfo->collspheresize * staticCollInfo->collspheresize;
    }

    rectchunk = mexGetChunk(mex,"Rct","Rct0");
    if (rectchunk != NULL)
    {
        // CAT: the box offset is taken as stored; the slides move only the sphere (mexSetCollisionInfo 0x004b7880)
        staticCollInfo->collrectoffset.x = rectchunk->ox;
        staticCollInfo->collrectoffset.y = rectchunk->oy;
        staticCollInfo->collrectoffset.z = rectchunk->oz;

        staticCollInfo->uplength = rectchunk->dx;
        staticCollInfo->rightlength = rectchunk->dy;
        staticCollInfo->forwardlength = rectchunk->dz;

        if (upScale != 1.0f)
        {
            real32 olduplength = staticCollInfo->uplength;
            staticCollInfo->uplength *= upScale;
            staticCollInfo->collrectoffset.x -= (staticCollInfo->uplength - olduplength) * 0.5f;
        }

        if (rightScale != 1.0f)
        {
            real32 oldrightlength = staticCollInfo->rightlength;
            staticCollInfo->rightlength *= rightScale;
            staticCollInfo->collrectoffset.y -= (staticCollInfo->rightlength - oldrightlength) * 0.5f;
        }

        if (forwardScale != 1.0f)
        {
            real32 oldforwardlength = staticCollInfo->forwardlength;
            staticCollInfo->forwardlength *= forwardScale;
            staticCollInfo->collrectoffset.z -= (staticCollInfo->forwardlength - oldforwardlength) * 0.5f;
        }
        // scale the sphere up to guarantee it surrounds the rectangle if necessary
        {
            real32 up = staticCollInfo->uplength * 0.5f;
            real32 right = staticCollInfo->rightlength * 0.5f;
            real32 forward = staticCollInfo->forwardlength * 0.5f;

            real32 minr = sqrt(up*up+right*right+forward*forward) * 1.1f;

            if (minr > staticCollInfo->collspheresize)
            {
                staticCollInfo->originalCollSphereSize = minr;
                staticCollInfo->collspheresize = minr;
                staticCollInfo->approxcollspheresize = minr;
                staticCollInfo->avoidcollspheresize = minr;
                staticCollInfo->avoidcollspherepad = minr;
                staticCollInfo->collspheresizeSqr = staticCollInfo->collspheresize * staticCollInfo->collspheresize;
            }
        }
    }
    else
    {
        // approximate rectangle using collsphere info
        real32 temp = staticCollInfo->collspheresize * 0.5f;

        staticCollInfo->collrectoffset.x = staticCollInfo->collsphereoffset.x - temp;
        staticCollInfo->collrectoffset.y = staticCollInfo->collsphereoffset.y - temp;
        staticCollInfo->collrectoffset.z = staticCollInfo->collsphereoffset.z - temp;

        staticCollInfo->uplength = staticCollInfo->collspheresize*upScale;
        staticCollInfo->rightlength = staticCollInfo->collspheresize*rightScale;
        staticCollInfo->forwardlength = staticCollInfo->collspheresize*forwardScale;
    }

    // CAT: the diagonal sums up^2 + (right^2 + forward^2) (the exe's order), and the box midpoint collrectmid is
    //      (collrectoffset + length) * 0.5 per axis, in both branches (mexSetCollisionInfo 0x004b7880)
    upSqr = staticCollInfo->uplength*staticCollInfo->uplength;
    rightforwardSqr = staticCollInfo->rightlength*staticCollInfo->rightlength +
                      staticCollInfo->forwardlength*staticCollInfo->forwardlength;
    staticCollInfo->diagonallength = sqrt(upSqr + rightforwardSqr);

    staticCollInfo->collrectmid.x = (staticCollInfo->uplength + staticCollInfo->collrectoffset.x) * 0.5f;
    staticCollInfo->collrectmid.y = (staticCollInfo->collrectoffset.y + staticCollInfo->rightlength) * 0.5f;
    staticCollInfo->collrectmid.z = (staticCollInfo->collrectoffset.z + staticCollInfo->forwardlength) * 0.5f;
}

void mexSetEngineNozzle(ShipStaticInfo *statinfo, void* mex, sdword index)
{
    MEXEngineChunk* enginechunk;
    char engine[5];
    lodinfo *LOD;
    sdword level, object;

    if (mex == NULL)
    {
        return;
    }

    strcpy(engine, "Eng0");
    engine[3] = (char)('0' + index);

    enginechunk = mexGetChunk(mex,"Eng",engine);
    if (enginechunk != NULL)
    {
        statinfo->engineNozzleOffset[index] = enginechunk->position;

        // CAT: for each LOD level, the index of the polygon object the chunk names (case-insensitive), or 0xFF when
        //      the chunk names none or the level has no such object (mexSetEngineNozzle 0x004b7c20; read by
        //      trailGetNozzleOffset 0x005a4cc0 through cat_shipNozzleInfoGet 0x0053f8c0). The exe walks the level's
        //      hierarchy bindings (binding->object->pName); the level count is capped at this table's 6 rows and a
        //      level without bindings counts as "none" (the exe reads both unchecked)
        LOD = statinfo->common.LOD;
        if (LOD != NULL)
        {
            for (level = 0; (level < LOD->nLevels) && (level < LOD_NumberLevels); level++)
            {
                meshdata *mesh = (meshdata *)LOD->level[level].pData;
                object = -1;
                if ((enginechunk->objectName[0] != 0) && (mesh != NULL) && (LOD->level[level].hBindings != NULL))
                {
                    for (object = 0; object < mesh->nPolygonObjects; object++)
                    {
                        if (strcasecmp(LOD->level[level].hBindings[object].object->pName, enginechunk->objectName) == 0)
                        {
                            break;
                        }
                    }
                    if (object >= mesh->nPolygonObjects)
                    {
                        object = -1;
                    }
                }
                statinfo->engineNozzleObject[level][index] = (ubyte)object;
            }
        }
    }
}

