/*=============================================================================
    Name    : LOD.h
    Purpose : Defininitions for level-of-detail control

    Created 7/20/1997 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___LOD_H
#define ___LOD_H

#include "Mesh.h"
#include "Vector.h"

/*=============================================================================
    Switches
=============================================================================*/
#ifdef HW_BUILD_FOR_DEBUGGING

#define LOD_ERROR_CHECKING          1           //general error checking
#define LOD_VERBOSE_LEVEL           2           //control specific output code
#define LOD_PRINT_DISTANCE          1           //print distances to selected ship
#define LOD_AUTO_SAVE               1           //enable auto-save of LOD files
#define LOD_SCALE_DEBUG             1           //debug of LOD tuning

#else

#define LOD_ERROR_CHECKING          0           //general error checking
#define LOD_VERBOSE_LEVEL           0           //control specific output code
#define LOD_PRINT_DISTANCE          0           //print distances to selected ship
#define LOD_AUTO_SAVE               0           //enable auto-save of LOD files
#define LOD_SCALE_DEBUG             0           //debug of LOD tuning

#endif

/*=============================================================================
    Definitions:
=============================================================================*/
//max number of Levels of detail
#define LOD_NumberLevels            6

//level-of-detail types
#define LT_Invalid                  0x0000
#define LT_Mesh                     0x0001      //regular mesh
#define LT_TinySprite               0x0002      //tiny sprite
#define LT_SubPixel                 0x0003      //a pixel or less
#define LT_Function                 0x0004      //a custom LOD function
#define LT_NULL                     0x0005      //draw nothing
#define LM_LODType                  0x0007      //mask for all types

#define LOD_ScaleFactor             0.6f

/*=============================================================================
    Type definitions:
=============================================================================*/
//structure for each level-of-detail
typedef struct
{
#if LOD_AUTO_SAVE
    float baseScalar, stripeScalar;             //not used by the LOD's really, but specified in the LOD file
#endif
    udword flags;
    real32 mOn, bOn;                            //params for on xition
    real32 mOff, bOff;                          //params for off xition
    void *pData;                                //type-specific pointer
    sdword nMatrices;                           //numnber of matrices
    //these matrices will be in the same order as they will be rendered.
//    hmatrix **startMatrix;                      //pointers to the starting hierarchy matrices
    mhbinding *hBindings;                       //hierarchy bindings, same order as matrices
    //CAT: render-time measurement per level (0x38-byte level in the exe; cat_lodTimingStart 0x004a4cf0 /
    //     cat_lodTimingStop 0x004a4d20 fill it, lodLevelGet 0x004a4d70 estimates it from the polygon count)
    udword catTime[4];                          //+0x20: the first 4 render times, performance-counter ticks
    ubyte catTimeCount;                         //+0x30: how many of catTime are measured (4 = done)
    udword catTimeMin;                          //+0x34: the least of catTime (or the estimate)
}
lod;
//LOD structure found in the StaticInfo for all spaceobjs
typedef struct
{
    sdword nLevels;                             //number of levels of detail for this ship
    color pointColor;                           //color to draw when it's just a point
#if LOD_AUTO_SAVE
    char *fileName;                             //name of the name of the LOD file
#endif
    bool8 catDistanceLOD;                       //CAT: +8: TRUE = choose the level by distance (HW1), FALSE = by the
                                                //     selection circle; lodTableReadScript 0x004a47e0 clears it and
                                                //     no LOD script sets it
    lod level[1];                               //actual LOD info for <n> levels
}
lodinfo;
//LOD structure with a maximum number of levels-of-detail
typedef struct
{
    sdword nLevels;                             //number of levels of detail for this ship
    color pointColor;                           //color to draw when it's just a point
#if LOD_AUTO_SAVE
    char *fileName;                             //name of the name of the LOD file
#endif
    bool8 catDistanceLOD;                       //CAT: as lodinfo
    lod level[LOD_NumberLevels];                //actual LOD info
}
lodmaxinfo;

/*=============================================================================
    Data:
=============================================================================*/
#if LOD_PRINT_DISTANCE
extern sdword rndLOD;
extern sdword lodTuningMode;
extern sdword lodDrawingMode;
#endif

//scaling factor for the LOD's
extern real32 lodScaleFactor;

//CAT: the render-time LOD (lodLevelGet 0x004a4d70, cat_lodLevelGetTimed 0x004a5000)
extern udword catLodDetail;                     //0x00a4cc38: the first level used (cat_opLODDetail 0x004d9d20)
extern udword catLodTimeThreshold;              //0x0088486c: frame render budget, counter ticks
extern real32 catLodTimeScale;                  //0x00a75da4: seconds per polygon, measured by the main view

#if LOD_SCALE_DEBUG
extern real32 lodDebugScaleFactor;
#endif

/*=============================================================================
    Macros:
=============================================================================*/
#define lodTableSize(n)         (sizeof(lodinfo) + (n - 1) * sizeof(lod))

/*=============================================================================
    Functions:
=============================================================================*/
lodinfo *lodTableReadScript(char *directory, char *fileName);

lod *lodLevelGet(void *spaceObj, vector *camera, vector *ship);
//CAT: the exe's lodLevelGet returns the level's render time; here lodLevelGet keeps HW1's return and the time is
//     cat_lodLevelTime's (the tail of lodLevelGet 0x004a4d70)
udword cat_lodLevelTime(lod *level);
lod *cat_lodCurrentLevelGet(void *spaceObj);                    //0x004a4af0
udword cat_lodLevelGetTimed(void *spaceObj, sdword pass);       //0x004a5000
void cat_lodTimingStart(lod *level);                            //0x004a4cf0
void cat_lodTimingStop(lod *level);                             //0x004a4d20
void cat_lodSetTimeThreshold(udword framesPerSecond);           //0x004a50e0
sdword cat_lodHierarchyCount(lod *level);                       //0x00575ee0
lod *lodPanicLevelGet(void *spaceObj, vector *camera, vector *ship);
void lodAllMeshesRecolorize(lodinfo *LOD);
sdword lodHierarchySizeCompute(lodinfo *LOD);

void lodFree(lodinfo *LOD);

#if LOD_AUTO_SAVE
sdword lodAutoSave(lodinfo *LOD);
#endif

#endif
