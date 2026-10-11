/*=============================================================================
    Name    : Mex.h
    Purpose : Definitions for Mex.c (Mesh extensions)

    Created 7/18/1997 by gshaw
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___MEX_H
#define ___MEX_H

#include "SpaceObj.h"
#include "Vector.h"

typedef struct tagMEXFileHeader
{
    char            identifier[8];                  // File identifier.
    unsigned short  version;                        // File version.
    unsigned short  nChunks;                        // Number of chunks in the file.
} MEXFileHeader;

typedef struct tagMEXChunk
{
    // Generic stuff.
    char            type[4];
    char            name[8];
    udword          chunkSize;
} MEXChunk;

typedef struct tagMEXGunChunk
{
    // Generic stuff.
    char            type[4];
    char            name[8];
    udword          chunkSize;

    // Specific stuff...
    vector          position;
    vector          normal;

    real32          coneAngle;
    real32          edgeAngle;
} MEXGunChunk;

typedef struct tagMEXEngineChunk
{
    // Generic stuff.
    char            type[4];
    char            name[8];
    udword          chunkSize;

    // Specific stuff...
    vector          position;
    vector          normal;

    real32          coneAngle;
    real32          edgeAngle;

    // CAT: Cataclysm's engine chunk is 0x40 bytes of data: the name of the mesh object the nozzle moves with
    //      follows at +0x30 (mexSetEngineNozzle 0x004b7c20; 85 of the install's 463 Eng chunks name one, e.g.
    //      bswarmer, sentinel; the others leave it empty)
    char            objectName[32];
} MEXEngineChunk;

typedef struct tagMEXCollisionSphereChunk
{
    // Generic stuff.
    char            type[4];
    char            name[8];
    udword          chunkSize;

    // Specific stuff...
    udword          level;

    vector          offset;
    real32          r;
} MEXCollisionSphereChunk;


typedef struct tagMEXCollisionRectangleChunk
{
    // Generic stuff.
    char            type[4];
    char            name[8];
    udword          chunkSize;

    // Specific stuff...
    udword          level;

    float           ox;
    float           oy;
    float           oz;

    float           dx;
    float           dy;
    float           dz;
} MEXCollisionRectangleChunk;


typedef struct tagMEXDockingChunk
{
    // Generic stuff.
    char            type[4];
    char            name[8];            // Always "DOCK"
    udword          chunkSize;

    // Specific stuff...
    vector          position;
    vector          normal;

    real32          coneAngle;
    real32          edgeAngle;

    char            dockName[20];       // Actual name.
} MEXDockingChunk;

typedef struct tagMEXNAVLightChunk
{
    // Generic stuff.
    char            type[4];
    char            name[8];            // Always "NAV"
    udword          chunkSize;

    // Specific stuff...
    vector          position;

    unsigned char   red;
    unsigned char   blue;
    unsigned char   green;
    unsigned char   pad;

    char            NAVLightName[20];       // Actual name.

    // CAT: Cataclysm's Nav chunk is 0x44 bytes of data: the name of the mesh object the light moves with follows at
    //      +0x34 (mexGetNAVLightStaticInfo 0x004b7680; 5 of the install's 2,951 Nav chunks name one: sleech's
    //      "LeechHead"; the others leave it empty)
    char            objectName[32];
} MEXNAVLightChunk;

void *mexGetChunk(void *mex,char *type,char *name);

void *mexLoad(char *filename);
void mexFree(void *mex);
bool32 mexVerify(void *mex);

ResNozzleStatic *mexGetResNozzleStatic(void *mex);
RepairNozzleStatic *mexGetRepairNozzleStatic(void *mex);
TractorBeamStatic *mexGetTractorBeamStatic(void *mex);
void mexGetGunStaticInfo(GunStaticInfo *gunstaticinfo,void *mex);
void mexGetDockStaticInfo(DockStaticInfo *dockstaticinfo,void *mex);
void mexGetNAVLightStaticInfo(NAVLightStaticInfo *navlightstaticinfo, StaticInfo *staticheader);   // CAT: the header, not the mex (0x004b7680)
void mexSetCollisionInfo(StaticCollInfo *staticCollInfo,void *mex,real32 forwardScale,real32 upScale,real32 rightScale, real32 xslide, real32 yslide, real32 zslide);
void mexSetEngineNozzle(ShipStaticInfo *statinfo, void* mex, sdword index);   // CAT: statinfo, not the offset
void mexVecToVec(vector *dest, vector *source);
MEXDockingChunk *mexGetDockChunk(void *mex,char *dockname);
MEXNAVLightChunk *mexGetNAVLightChunk(void *mex,char *navLightName);

#endif

