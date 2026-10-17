/*=============================================================================
    Name    : LOD.c
    Purpose : Service functions for level-of-detail

    Created 7/21/1997 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "StatScript.h"
#include "Memory.h"
#include "SpaceObj.h"
#include "Debug.h"
#include "File.h"
#include "Key.h"
#include "texreg.h"
#include "LOD.h"
#include "Options.h"
#include "Mimic.h"

#include <math.h>
#include <SDL2/SDL.h>

extern meshdata *defaultmesh;          // hack for now.  remove later when defaults no longer needed

/*=============================================================================
    Data:
=============================================================================*/
//script-parsing function for LOD files
static void lodTypeRead(char *directory,char *field,void *dataToFillIn);
static void lodMeshFileLoad(char *directory,char *field,void *dataToFillIn);
static void lodSpriteFileRead(char *directory,char *field,void *dataToFillIn);
static void lodColorScalarRead(char *directory,char *field,void *dataToFillIn);
static lodinfo lodStaticInfo;
scriptStructEntry lodScriptTable[] =
{
    { "pointColor",   scriptSetRGBCB,    (&lodStaticInfo.pointColor),     (&lodStaticInfo)},
    { "type0",        lodTypeRead,       (&lodStaticInfo.level[0]),       (&lodStaticInfo)},
    { "type1",        lodTypeRead,       (&lodStaticInfo.level[1]),       (&lodStaticInfo)},
    { "type2",        lodTypeRead,       (&lodStaticInfo.level[2]),       (&lodStaticInfo)},
    { "type3",        lodTypeRead,       (&lodStaticInfo.level[3]),       (&lodStaticInfo)},
    { "type4",        lodTypeRead,       (&lodStaticInfo.level[4]),       (&lodStaticInfo)},
    { "type5",        lodTypeRead,       (&lodStaticInfo.level[5]),       (&lodStaticInfo)},
    { "mOn0",         scriptSetReal32CB, (&lodStaticInfo.level[0].mOn),   (&lodStaticInfo)},
    { "mOn1",         scriptSetReal32CB, (&lodStaticInfo.level[1].mOn),   (&lodStaticInfo)},
    { "mOn2",         scriptSetReal32CB, (&lodStaticInfo.level[2].mOn),   (&lodStaticInfo)},
    { "mOn3",         scriptSetReal32CB, (&lodStaticInfo.level[3].mOn),   (&lodStaticInfo)},
    { "mOn4",         scriptSetReal32CB, (&lodStaticInfo.level[4].mOn),   (&lodStaticInfo)},
    { "mOn5",         scriptSetReal32CB, (&lodStaticInfo.level[5].mOn),   (&lodStaticInfo)},
    { "mOff0",        scriptSetReal32CB, (&lodStaticInfo.level[0].mOff),  (&lodStaticInfo)},
    { "mOff1",        scriptSetReal32CB, (&lodStaticInfo.level[1].mOff),  (&lodStaticInfo)},
    { "mOff2",        scriptSetReal32CB, (&lodStaticInfo.level[2].mOff),  (&lodStaticInfo)},
    { "mOff3",        scriptSetReal32CB, (&lodStaticInfo.level[3].mOff),  (&lodStaticInfo)},
    { "mOff4",        scriptSetReal32CB, (&lodStaticInfo.level[4].mOff),  (&lodStaticInfo)},
    { "mOff5",        scriptSetReal32CB, (&lodStaticInfo.level[5].mOff),  (&lodStaticInfo)},
    { "bOn0",         scriptSetReal32CB, (&lodStaticInfo.level[0].bOn),   (&lodStaticInfo)},
    { "bOn1",         scriptSetReal32CB, (&lodStaticInfo.level[1].bOn),   (&lodStaticInfo)},
    { "bOn2",         scriptSetReal32CB, (&lodStaticInfo.level[2].bOn),   (&lodStaticInfo)},
    { "bOn3",         scriptSetReal32CB, (&lodStaticInfo.level[3].bOn),   (&lodStaticInfo)},
    { "bOn4",         scriptSetReal32CB, (&lodStaticInfo.level[4].bOn),   (&lodStaticInfo)},
    { "bOn5",         scriptSetReal32CB, (&lodStaticInfo.level[5].bOn),   (&lodStaticInfo)},
    { "bOff0",        scriptSetReal32CB, (&lodStaticInfo.level[0].bOff),  (&lodStaticInfo)},
    { "bOff1",        scriptSetReal32CB, (&lodStaticInfo.level[1].bOff),  (&lodStaticInfo)},
    { "bOff2",        scriptSetReal32CB, (&lodStaticInfo.level[2].bOff),  (&lodStaticInfo)},
    { "bOff3",        scriptSetReal32CB, (&lodStaticInfo.level[3].bOff),  (&lodStaticInfo)},
    { "bOff4",        scriptSetReal32CB, (&lodStaticInfo.level[4].bOff),  (&lodStaticInfo)},
    { "bOff5",        scriptSetReal32CB, (&lodStaticInfo.level[5].bOff),  (&lodStaticInfo)},
    { "pMeshFile0",   lodMeshFileLoad,   (&lodStaticInfo.level[0].pData), (&lodStaticInfo)},
    { "pMeshFile1",   lodMeshFileLoad,   (&lodStaticInfo.level[1].pData), (&lodStaticInfo)},
    { "pMeshFile2",   lodMeshFileLoad,   (&lodStaticInfo.level[2].pData), (&lodStaticInfo)},
    { "pMeshFile3",   lodMeshFileLoad,   (&lodStaticInfo.level[3].pData), (&lodStaticInfo)},
    { "pMeshFile4",   lodMeshFileLoad,   (&lodStaticInfo.level[4].pData), (&lodStaticInfo)},
    { "pMeshFile5",   lodMeshFileLoad,   (&lodStaticInfo.level[5].pData), (&lodStaticInfo)},
    { "pSpriteFile0", lodSpriteFileRead, (&lodStaticInfo.level[0].pData), (&lodStaticInfo)},
    { "pSpriteFile1", lodSpriteFileRead, (&lodStaticInfo.level[1].pData), (&lodStaticInfo)},
    { "pSpriteFile2", lodSpriteFileRead, (&lodStaticInfo.level[2].pData), (&lodStaticInfo)},
    { "pSpriteFile3", lodSpriteFileRead, (&lodStaticInfo.level[3].pData), (&lodStaticInfo)},
    { "pSpriteFile4", lodSpriteFileRead, (&lodStaticInfo.level[4].pData), (&lodStaticInfo)},
    { "pSpriteFile5", lodSpriteFileRead, (&lodStaticInfo.level[5].pData), (&lodStaticInfo)},
    { "baseScalar",   lodColorScalarRead,(void *) 0, (void *) 0},
    { "stripeScalar", lodColorScalarRead,(void *) 2, (void *) 1},

    END_SCRIPT_STRUCT_ENTRY
};

//scaling factor for the LOD's
real32 lodScaleFactor = LOD_ScaleFactor;

//CAT: the render-time LOD (see LOD.h)
udword catLodDetail = 0;                        //0x00a4cc38; cat_opSetAllDefaults 0x004db7b0 sets 0 (high) or 2
udword catLodTimeThreshold = 10;                //0x0088486c (10 in .data until the options set it)
real32 catLodTimeScale = 0.0f;                  //0x00a75da4
//CAT: the exe reads the selection circle radius (+0x158) against these (0x0072a674, 0x0072ab84 .. 0x0072ab78)
#define CAT_LOD_RADIUS_MAX      0.4f
#define CAT_LOD_RADIUS_1        0.12f
#define CAT_LOD_RADIUS_2        0.045f
#define CAT_LOD_RADIUS_3        0.017f
#define CAT_LOD_RADIUS_4        0.0065f

//fixed version of above factor for debugging reasons
#if LOD_SCALE_DEBUG
real32 lodDebugScaleFactor = 0.0f;
#endif

/*=============================================================================
    Functions:
=============================================================================*/
/*-----------------------------------------------------------------------------
    Name        : lodTableReadScript
    Description : Load in and allocate a level-of-detail table from a script
                    file.
    Inputs      : fileName - name of script file to read from
    Outputs     : Allocates a LOD table and fills it in.
    Return      : newly allocated LOD table.
----------------------------------------------------------------------------*/
lodmaxinfo lodMaxInfo;
lodinfo *lodTableReadScript(char *directory, char *fileName)
{
    lodinfo *info;
    sdword index;

    for (index = 0; index < LOD_NumberLevels; index++)
    {                                                       //initialize the lodmaxinfo structure
        lodMaxInfo.level[index].flags = LT_Invalid;
        lodMaxInfo.level[index].pData = NULL;
        // CAT: the timing fields start empty (lodTableReadScript 0x004a47e0)
        memset(lodMaxInfo.level[index].catTime, 0, sizeof(lodMaxInfo.level[index].catTime));
        lodMaxInfo.level[index].catTimeCount = 0;
        lodMaxInfo.level[index].catTimeMin = 0;
    }

    trBaseColorScalar = trStripeColorScalar = 0.0f;
    lodMaxInfo.catDistanceLOD = FALSE;                      // CAT: 0x00c36188 = 0 before the script is read
    scriptSetStruct(directory, fileName, lodScriptTable, (ubyte *)&lodMaxInfo);//read the script file
    trBaseColorScalar = trStripeColorScalar = 0.0f;

    for (index = 0; index < LOD_NumberLevels; index++)
    {
        if ((lodMaxInfo.level[index].flags & LM_LODType) == LT_Invalid)
        {
            break;
        }
        if ((lodMaxInfo.level[index].flags & LM_LODType) == LT_Mesh)
        {                                                   //if it's a mesh
            lodMaxInfo.level[index].hBindings =                //create a bindings list
                meshBindingListCreate((meshdata *)lodMaxInfo.level[index].pData);
        }
        else
        {
            lodMaxInfo.level[index].hBindings = NULL;
        }
    }
    info = memAlloc(lodTableSize(index), "LOD Table", NonVolatile); //allocate the table
    memcpy(info, &lodMaxInfo, lodTableSize(index));            //and copy over the max table
    info->nLevels = index;                                  //store number of levels
#if LOD_AUTO_SAVE
    {
        char partialName[PATH_MAX];
        char *fullName;
        sprintf(partialName, "%s%s", directory, fileName);
        fullName = filePathPrepend(partialName, FF_IgnorePrepend);
        info->fileName = memStringDupe(fullName);           //store full path to file for saving
    }
#endif
    return(info);
}

/*=============================================================================
    Callback functions for script reading
=============================================================================*/
//set correct LOD type
static void lodColorScalarRead(char *directory,char *field,void *dataToFillIn)
{
    real32 data;

    sscanf(field, "%f", &data);
    dbgAssertOrIgnore(data >= 0.0f && data <= 10.0f);
    if (data == 0.0f)
    {                                                       //0 is regarded as 'no change', hence 1.0
        data = 1.0f;
    }
    if (dataToFillIn == (void *)(&lodMaxInfo))
    {
        trBaseColorScalar = data;
    }
    else
    {
        trStripeColorScalar = data;
    }
}
//set correct LOD type
static void lodTypeRead(char *directory,char *field,void *dataToFillIn)
{                                                           //set correct LOD type
    lod *dest;
    unsigned int i;

    dest = (lod *)dataToFillIn;
#if LOD_AUTO_SAVE
    dest->baseScalar = (trBaseColorScalar == 0.0f) ? 1.0f : trBaseColorScalar;
    dest->stripeScalar = (trStripeColorScalar == 0.0f) ? 1.0f : trStripeColorScalar;
#endif
    /*_strupr(field);*/
    for (i = 0; (field[i] = toupper(field[i])); i++) { }

    if (strstr(field, "INVALID"))
    {
        dest->flags = (udword)((dest->flags & (~LM_LODType)) | LT_Invalid);
    }
    else if (strstr(field, "MESH"))
    {
        dest->flags = (udword)((dest->flags & (~LM_LODType)) | LT_Mesh);
    }
    else if (strstr(field, "TINYSPRITE"))
    {
        dest->flags = (udword)((dest->flags & (~LM_LODType)) | LT_TinySprite);
    }
    else if (strstr(field, "SUBPIXEL"))
    {
        dest->flags = (udword)((dest->flags & (~LM_LODType)) | LT_SubPixel);
    }
    else if (strstr(field, "FUNCTION"))
    {
        dest->flags = (udword)((dest->flags & (~LM_LODType)) | LT_Function);
    }
    else if (strstr(field, "NULL"))
    {
        dest->flags = (udword)((dest->flags & (~LM_LODType)) | LT_NULL);
    }
#if LOD_ERROR_CHECKING
    else
    {
        dbgFatalf(DBG_Loc, "Invalid LOD type: '%s'", field);
    }
#endif
}
//load in a mesh
static void lodMeshFileLoad(char *directory,char *field,void *dataToFillIn)
{
    char fullfilename[80];
#if LOD_VERBOSE_LEVEL >= 1
//    dbgMessagef("lodMeshFileLoad: %s", field);
#endif
    if (directory != NULL)
    {
        strcpy(fullfilename,directory);
        strcat(fullfilename,field);
    }
    else
    {
        strcpy(fullfilename,field);
    }

    if (fileExists(fullfilename,0) || meshPagedVersionExists(fullfilename))
    {
        *((meshdata **)dataToFillIn) = meshLoad(fullfilename);
    }
    else
    {
        *((meshdata **)dataToFillIn) = defaultmesh;
    }
}
//load in a tiny sprite file
static void lodSpriteFileRead(char *directory,char *field,void *dataToFillIn)
{
#if LOD_VERBOSE_LEVEL >= 1
    dbgMessagef("lodSpriteFileRead: %s", field);
#endif
    dbgFatal(DBG_Loc, "Can't load sprite images yet!");
}

/*-----------------------------------------------------------------------------
    Name        : lodLevelGet
    Description : Get level of detail for specified ship
    Inputs      : spaceObj - space object to get
                  camera, ship - location of ship and camera, respectively
    Outputs     : updates the currentLOD of the specified space object.
    Return      : lod structure for current level of detail
----------------------------------------------------------------------------*/
#if LOD_PRINT_DISTANCE
sdword rndLOD = 0;
sdword lodTuningMode = FALSE;
sdword lodDrawingMode = FALSE;
#endif
lod *lodLevelGet(void *spaceObj, vector *camera, vector *ship)
{
    real32 distance;
    //TODO(type safety, 2026-04-24): Take SpaceObj* instead of void*
    SpaceObj *obj = (SpaceObj *)spaceObj;
    // CAT: the selection circle radius is read first (the exe's 0x004a4d70 reads +0x158 of any object here) and
    //      the table is the apparent one of a mimic (cat_mimicGetStaticInfo 0x005b9640)
    real32 radius = ((SpaceObjRotImpTarg *)obj)->impactable.collInfo.selCircleRadius;
    lodinfo *info = (obj->header.objtype == OBJ_ShipType) ? cat_mimicGetStaticInfo((Ship *)obj) : obj->staticinfo->LOD;
    udword level;

    dbgAssertOrIgnore(info != NULL);                                //verify the LOD table exists

    vecSub(obj->cameraDistanceVector,*camera,*ship);
    obj->cameraDistanceSquared = distance = vecMagnitudeSquared(obj->cameraDistanceVector);

#if LOD_SCALE_DEBUG
    if (lodDebugScaleFactor != 0.0f)
    {
        lodScaleFactor = lodDebugScaleFactor;
    }
#endif
    if (!info->catDistanceLOD)
    {
        // CAT: by the size on screen: the selection circle radius against 5 bands, from catLodDetail on
        if (radius < 0.0f || radius > CAT_LOD_RADIUS_MAX)
        {
            obj->currentLOD = (ubyte)catLodDetail;
        }
        else if (radius > CAT_LOD_RADIUS_1)
        {
            obj->currentLOD = (ubyte)(catLodDetail + 1);
        }
        else if (radius > CAT_LOD_RADIUS_2)
        {
            obj->currentLOD = (ubyte)(catLodDetail + 2);
        }
        else if (radius > CAT_LOD_RADIUS_3)
        {
            obj->currentLOD = (ubyte)(catLodDetail + 3);
        }
        else if (radius > CAT_LOD_RADIUS_4)
        {
            obj->currentLOD = (ubyte)(catLodDetail + 4);
        }
        else
        {
            obj->currentLOD = (ubyte)(catLodDetail + 5);
        }
        if ((sdword)obj->currentLOD >= info->nLevels)
        {
            obj->currentLOD = (ubyte)(info->nLevels - 1);
        }
    }
    else
    {
        if (distance > info->level[obj->currentLOD].bOff * lodScaleFactor)
        {                                                       //if drop a level of detail
            do
            {
                obj->currentLOD++;                                  //go to lower level
                if (obj->currentLOD >= info->nLevels)
                {
                    obj->currentLOD = info->nLevels-1;
                    break;
                }
            }
            while (distance > info->level[obj->currentLOD].bOff * lodScaleFactor);
        }
        else
        {
            while (obj->currentLOD > 0 && distance < info->level[obj->currentLOD - 1].bOn * lodScaleFactor)
            {                                                   //if go higher level of detail
                obj->currentLOD--;                              //go to higher level
            }
        }
        // CAT: never above the detail option's first level
        if (obj->currentLOD < catLodDetail)
        {
            obj->currentLOD = (ubyte)(((udword)info->nLevels <= catLodDetail) ? info->nLevels - 1 : catLodDetail);
        }
    }

    // CAT: no opNoLODVal override here (the exe's main view tests it before it raises levels for time)

    dbgAssertOrIgnore(obj->currentLOD >= 0);
    dbgAssertOrIgnore(obj->currentLOD < info->nLevels);             //verify we are within the available levels of detail
#if LOD_PRINT_DISTANCE
    if (keyIsStuck(WKEY))
    {
        keyClearSticky(WKEY);
        lodScaleFactor *= 0.99f;
        dbgMessagef("lodScaleFactor = %.3f", lodScaleFactor);
    }
    if (keyIsStuck(OKEY))
    {
        keyClearSticky(OKEY);
        lodScaleFactor *= 1.01f;
        dbgMessagef("lodScaleFactor = %.3f", lodScaleFactor);
    }
    if (lodTuningMode)
    {
        obj->currentLOD = min(rndLOD, info->nLevels - 1);
        return(&info->level[min(rndLOD, info->nLevels - 1)]);
    }
#endif
    level = obj->currentLOD;
    return(&info->level[level]);                            //return pointer to lod structure
}

/*-----------------------------------------------------------------------------
    Name        : cat_lodLevelTime
    Description : CAT: the tail of lodLevelGet 0x004a4d70 / cat_lodLevelGetTimed 0x004a5000: the level's render
                  time. Measured (catTimeMin) once 4 renders are timed or once a time is known; otherwise, when
                  the main view has measured the seconds per polygon (catLodTimeScale), estimated from the
                  level's polygon count and stored in the unmeasured slots.
    Inputs      : level - the level
    Outputs     : level->catTimeMin, level->catTime[catTimeCount..3]
    Return      : performance-counter ticks (0 while nothing is known)
----------------------------------------------------------------------------*/
udword cat_lodLevelTime(lod *level)
{
    sdword i;

    if (catLodTimeScale != 0.0f)
    {
        if (level->catTimeCount > 3 || level->catTimeMin != 0)
        {
            return level->catTimeMin;
        }
        // CAT: rounded to the nearest integer (the exe adds 1.5 * 2^52, 0x0072a458)
        level->catTimeMin = (udword)lrint((double)((real32)cat_lodHierarchyCount(level) *
                                                   (real32)(sdword)SDL_GetPerformanceFrequency() * catLodTimeScale));
        for (i = level->catTimeCount; i < 4; i++)
        {
            level->catTime[i] = level->catTimeMin;
        }
    }
    return level->catTimeMin;
}

/*-----------------------------------------------------------------------------
    Name        : cat_lodCurrentLevelGet (Cataclysm 0x004a4af0)
    Description : The current level of an object, from its apparent LOD table (a mimic's disguise)
    Inputs      : spaceObj - the object
    Outputs     :
    Return      : the level
----------------------------------------------------------------------------*/
lod *cat_lodCurrentLevelGet(void *spaceObj)
{
    SpaceObj *obj = (SpaceObj *)spaceObj;
    lodinfo *info = (obj->header.objtype == OBJ_ShipType) ? cat_mimicGetStaticInfo((Ship *)obj) : obj->staticinfo->LOD;

    return &info->level[obj->currentLOD];
}

/*-----------------------------------------------------------------------------
    Name        : cat_lodLevelGetTimed (Cataclysm 0x004a5000)
    Description : One pass of the main view's frame-budget loop: an object at the pass's level (0x0072be84:
                  99, 3, 2, 1, 0, 3, 2, 1, 3, 2, 3) drops one level (not past the last)
    Inputs      : spaceObj - the object; pass - 1..10
    Outputs     : spaceObj->currentLOD
    Return      : the render time of the object's level (cat_lodLevelTime)
----------------------------------------------------------------------------*/
static const ubyte catLodPassLevel[11] = {99, 3, 2, 1, 0, 3, 2, 1, 3, 2, 3};
udword cat_lodLevelGetTimed(void *spaceObj, sdword pass)
{
    SpaceObj *obj = (SpaceObj *)spaceObj;
    lodinfo *info = (obj->header.objtype == OBJ_ShipType) ? cat_mimicGetStaticInfo((Ship *)obj) : obj->staticinfo->LOD;

    if (catLodPassLevel[pass] == obj->currentLOD)
    {
        obj->currentLOD++;
        if ((sdword)obj->currentLOD >= info->nLevels)
        {
            obj->currentLOD = (ubyte)(info->nLevels - 1);
        }
    }
    return cat_lodLevelTime(&info->level[obj->currentLOD]);
}

/*-----------------------------------------------------------------------------
    Name        : cat_lodTimingStart / cat_lodTimingStop (Cataclysm 0x004a4cf0 / 0x004a4d20)
    Description : Time one render of a level: the first 4 renders are kept; catTimeMin is their least
                  (QueryPerformanceCounter's low dword in the exe; SDL's counter here)
    Inputs      : level - the level being rendered
    Outputs     : level->catTime[], catTimeCount, catTimeMin
    Return      :
----------------------------------------------------------------------------*/
void cat_lodTimingStart(lod *level)
{
    if (level->catTimeCount < 4)
    {
        level->catTime[level->catTimeCount] = (udword)SDL_GetPerformanceCounter();
    }
}

void cat_lodTimingStop(lod *level)
{
    sdword i;

    if (level->catTimeCount < 4)
    {
        level->catTime[level->catTimeCount] = (udword)SDL_GetPerformanceCounter() - level->catTime[level->catTimeCount];
        level->catTimeMin = level->catTime[0];
        for (i = 1; i < 4; i++)
        {
            if (level->catTime[i] < level->catTimeMin)
            {
                level->catTimeMin = level->catTime[i];
            }
        }
        level->catTimeCount++;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_lodSetTimeThreshold (Cataclysm 0x004a50e0)
    Description : The frame render budget: counter frequency / framesPerSecond (opDetailThresholdVal: 10 or 30)
    Inputs      : framesPerSecond
    Outputs     : catLodTimeThreshold
    Return      :
----------------------------------------------------------------------------*/
void cat_lodSetTimeThreshold(udword framesPerSecond)
{
    catLodTimeThreshold = (udword)(SDL_GetPerformanceFrequency() / (Uint64)framesPerSecond);
}

/*-----------------------------------------------------------------------------
    Name        : lodPanicLevelGet
    Description : Get level of detail for specified ship
    Inputs      : spaceObj - space object to get
                  camera, ship - location of ship and camera, respectively
    Outputs     : updates the currentLOD of the specified space object.
    Return      : lod structure for current level of detail
----------------------------------------------------------------------------*/
lod* lodPanicLevelGet(void* spaceObj, vector* camera, vector* ship)
{
    //TODO(type safety, 2026-04-24): Take SpaceObj* as param instead of void*
    SpaceObj* obj = (SpaceObj*)spaceObj;
    lodinfo* info = obj->staticinfo->LOD;

    if (obj->currentLOD == 3)
    {
        obj->currentLOD++;
        if (obj->currentLOD >= info->nLevels)
        {
            obj->currentLOD--;
        }
        if ((info->level[obj->currentLOD].flags & LM_LODType) != LT_Mesh)
        {
            obj->currentLOD--;
        }
    }

    return(&info->level[obj->currentLOD]);
}

/*-----------------------------------------------------------------------------
    Name        : lodFree
    Description : frees LOD
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void lodFree(lodinfo *LOD)
{
    sdword i;
    void *pData;

    for (i=0;i<LOD->nLevels;i++)
    {
        pData = LOD->level[i].pData;
        switch (LOD->level[i].flags)
        {
            case LT_Mesh:
                if ((pData != defaultmesh) && (pData != NULL))
                {
                    meshFree((meshdata *)pData);
                }
                dbgAssertOrIgnore(LOD->level[i].hBindings != NULL);
                meshBindingListDelete(LOD->level[i].hBindings);
                break;

            default:        // later add other stuff you want to free such as sprites Luke.
                break;
        }
    }

    memFree(LOD);
}

/*-----------------------------------------------------------------------------
    Name        : lodAllMeshesRecolorize
    Description : Recolorizes all meshes in a particular LOD to reflect changes
                    in team color schemes.
    Inputs      : LOD - what LOD to recolorize;
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void lodAllMeshesRecolorize(lodinfo *LOD)
{
    sdword index;

    for (index = 0; index < LOD->nLevels; index++)
    {                                                       //for each level in this LOD
        if ((LOD->level[index].flags & LM_LODType) == LT_Mesh)
        {                                                   //if it's a mesh type
            meshRecolorize((meshdata *)LOD->level[index].pData);//recolorize it
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : lodHierarchySizeCompute
    Description : Compute the number of objects in all meshes in an LOD
    Inputs      : LOD - LOD to count the objects in
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
sdword lodHierarchySizeCompute(lodinfo *LOD)
{
    sdword index, size;
    lod *level;

    for (index = size = 0; index < LOD->nLevels; index++)
    {                                                       //for all levels of detail
        level = &LOD->level[index];
        if ((level->flags & LM_LODType) != LT_Mesh)
        {
            continue;
        }
        size += ((meshdata *)level->pData)->nPolygonObjects;
    }
    return(size);
}


/*-----------------------------------------------------------------------------
    Name        : lodAutoSave
    Description : Save out an LOD file after internal tuning.
    Inputs      : LOD - what LOD to save
    Outputs     :
    Return      : 0 (success) or ERROR
    Note        : Prints a warning if the .LOD file cannot be opened for writing.
----------------------------------------------------------------------------*/
#if LOD_AUTO_SAVE
char *lodTypeStrings[] = {"INVALID", "MESH", "TINYSPRITE", "SUBPIXEL", "FUNCTION", "NULL"};
sdword lodAutoSave(lodinfo *LOD)
{
    sdword level;
    FILE *fp;
    char *filePath;
    char *lodFileNameFull;
    real32 baseScalar = 1.0f, stripeScalar = 1.0f;

    lodFileNameFull = filePathPrepend(LOD->fileName, FF_UserSettingsPath);

    if (!fileMakeDestinationDirectory(lodFileNameFull))
    {
        dbgWarningf(
            DBG_Loc,
            "Cannot create destination directory for file '%s'.",
            LOD->fileName);
        return ERROR;
    }

    fp = fopen(lodFileNameFull, "wt");
    if (fp == NULL)
    {
        dbgWarningf(DBG_Loc, "Cannot open '%s' for writing - not checked out?", LOD->fileName);
        return(ERROR);
    }
    fprintf(fp, "[Auto-Saved LOD file]\n");
    fprintf(fp, "pointColor                  %d,%d,%d\n\n", colRed(LOD->pointColor), colGreen(LOD->pointColor), colBlue(LOD->pointColor));

    for (level = 0; level < LOD->nLevels; level++)
    {
        fprintf(fp, "\n");
        if (LOD->level[level].baseScalar != baseScalar)
        {
            baseScalar = LOD->level[level].baseScalar;
            fprintf(fp, "baseScalar                  %.2f\n", baseScalar == 0.0f ? 1.0f : baseScalar);
        }
        if (LOD->level[level].stripeScalar != stripeScalar)
        {
            stripeScalar = LOD->level[level].stripeScalar;
            fprintf(fp, "stripeScalar                %.2f\n", stripeScalar == 0.0f ? 1.0f : stripeScalar);
        }
        fprintf(fp, "type%d                       %s\n", level, lodTypeStrings[LOD->level[level].flags & LM_LODType]);
        if ((LOD->level[level].flags & LM_LODType) == LT_Mesh)
        {
#ifdef _WIN32
            filePath = strchr(((meshdata *)LOD->level[level].pData)->fileName, '\\') + 1;
#else
            filePath = strpbrk(((meshdata *)LOD->level[level].pData)->fileName, "\\/") + 1;
#endif
#if LOD_ERROR_CHECKING
            if (*filePath == 0)
            {
                dbgFatalf(DBG_Loc, "Could not find '\\' in '%s'", ((meshdata *)LOD->level[level].pData)->fileName);
            }
#endif
            fprintf(fp, "pMeshFile%d                  %s\n", level, filePath);
        }
        fprintf(fp, "mOn%d                        %.1f\n", level, LOD->level[level].mOn);
        fprintf(fp, "bOn%d                        %.0f\n", level, LOD->level[level].bOn);
        fprintf(fp, "mOff%d                       %.1f\n", level, LOD->level[level].mOff);
        fprintf(fp, "bOff%d                       %.0f\n", level, LOD->level[level].bOff);
    }
    fclose(fp);
    return(0);
}
#endif
