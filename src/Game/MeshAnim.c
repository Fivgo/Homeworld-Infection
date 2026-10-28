/*=============================================================================
    Name    : MeshAnim.c
    Purpose : Code for animating meshes in ships

    Created 7/22/1998 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include <math.h>
#include <stdlib.h>
#include <string.h>

#if !defined _MSC_VER
#include <strings.h>
#endif

#include "Debug.h"
#include "File.h"
#include "Matrix.h"
#include "Memory.h"
#include "MeshAnim.h"
#include "MotionInterp.h"   // HWI-FEATURE(motion-interp) mesh animations: the frame's alpha
#include "NIS.h"
#include "SoundEvent.h"     // CAT: cat_madAnimSoundEvent (the animation sounds)
#include "SpaceObj.h"
#include "Types.h"
#include "UnivUpdate.h"     // CAT: cat_univMeshBindingsLinkParents
#include "Universe.h"
#include "Vector.h"
#include "mainrgn.h"

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

/*=============================================================================
    Data:
=============================================================================*/
#if MR_TEST_HPB
sdword madTestHPBIndex = 0;
extern bool32 mrTestHPBMode;
extern real32 mrHeading, mrPitch, mrBank;
#endif

//global data for recursive-walk callbacks
static mhlocalbinding *madBindingDest;
static madheader *madDupeHeader;
static Ship *madDupeShip;
//static sdword madBindingIndex, madIncIndex;
//callback(static char *madDupeName;

/*=============================================================================
    Functions:
=============================================================================*/


#ifdef HW_PTR_64
/*-----------------------------------------------------------------------------
    CAT (M2-R9 recorder-leftovers): the .mad file's own offset of each object's keyframe block (the file's
    madobjpath.times), kept per loaded header. cataclysm.exe loads the file as it is (minus the trailing string block),
    so its saves hold the spline curves' pointers as these file offsets (PreFix_madBindings 0x004b6fb0); this tree
    converts the file to an LP64 layout first, so the offsets are remembered here for the exe-format save writer
    (SaveCat32.c saveCat32SaveShip)
----------------------------------------------------------------------------*/
typedef struct MadCat32FileOffsets
{
    madheader *header;
    sdword nObjects;
    udword *times;
    struct MadCat32FileOffsets *next;
} MadCat32FileOffsets;

static MadCat32FileOffsets *madCat32Offsets = NULL;
static udword *madCat32PendingTimes = NULL;         // the converter's offsets until madFileLoad has the header
static sdword madCat32PendingObjects = 0;

bool32 madCat32ObjectFileOffset(madheader *header, sdword object, udword *offset)
{
    MadCat32FileOffsets *e;

    for (e = madCat32Offsets; e != NULL; e = e->next)
    {
        if ((e->header == header) && (object >= 0) && (object < e->nObjects))
        {
            *offset = e->times[object];
            return TRUE;
        }
    }
    return FALSE;
}

sdword madFileLoadAndConvertTo64Bit(char *fileName, void** loadAddress)
{
    int i;
    int j;

    int loopvar;
    int loopcount;

    int oldLength;
    int newLength;

    int madOpArgs=0;
    int madOpArgsThis=0;

    madheader_disk *oldHeader = NULL;
    madheader *newHeader = NULL;

    madanimation_disk *oldAnim = NULL;
    madanimation *newAnim = NULL;

    madobjpath_disk *oldMOP = NULL;
    madobjpath *newMOP = NULL;

    int madobjloop = 0;

    memsize offset=0;
    memsize loopsize=0;

    void * oldptr = NULL;
    void * newptr = NULL;
    void * oldbase = NULL;
    void * newbase = NULL;


    oldLength = fileLoadAlloc(fileName, (void **)&oldbase, NonVolatile);
    oldHeader = oldptr = oldbase;

    //printf("%s size is %d\n",oldFile, oldLength);

    newLength = 2*oldLength;

    offset = newLength;

    //newbase = malloc(2*oldLength);
    newbase = memAlloc(2* oldLength, "mad 64bit conversion", 0); 
    newHeader = newbase;
    newptr =  (void*) newbase;

    memset (newbase, 0 ,2*oldLength);
  

    strncpy(newHeader->identifier,oldHeader->identifier, 8);
    newHeader->version = oldHeader->version;
    newHeader->stringBlockLength = oldHeader->stringBlockLength;
    newHeader->length = oldHeader->length;
    newHeader->framesPerSecond = oldHeader->framesPerSecond;
    newHeader->nObjects = oldHeader->nObjects;
    newHeader->nAnimations = oldHeader->nAnimations;

    oldAnim=(madanimation_disk*)&oldHeader->anim;    
    newAnim=(madanimation*)&(newHeader->anim);    

    for (loopvar = 0; loopvar < newHeader->nAnimations; loopvar++){
        newAnim->name = (char*)(memsize)oldAnim->name;
        newAnim->startTime = oldAnim->startTime;
        newAnim->endTime = oldAnim->endTime;
        newAnim->flags = oldAnim->flags;

        newAnim++;
        oldAnim++;
    }
        
    newHeader->objPath=(madobjpath*)((memsize) newAnim - (memsize)newbase);

    oldMOP = (madobjpath_disk*) oldAnim;
    newMOP = (madobjpath*) newAnim;

    oldptr = oldbase + oldMOP->times;

    newptr = (void*) newMOP;
    newptr += sizeof(madobjpath) * newHeader->nObjects;
    offset = (memsize) ( (memsize)(newptr) - (memsize)(newbase));

    madOpArgs=sizeof(udword) /*times*/ + sizeof(tcb) /*parameters*/
                + sizeof(real32[6]) /*path*/;

    //printf("sizeof madOpArgs= %d\n",madOpArgs);

    // CAT (M2-R9 recorder-leftovers): the file's keyframe block offsets, for the exe-format save writer
    if (madCat32PendingTimes != NULL)
    {
        memFree(madCat32PendingTimes);
    }
    madCat32PendingObjects = newHeader->nObjects;
    madCat32PendingTimes = memAlloc(sizeof(udword) * (madCat32PendingObjects > 0 ? madCat32PendingObjects : 1), "madCat32Times", NonVolatile);

    for (madobjloop=0; madobjloop <newHeader->nObjects ; madobjloop++){

        madCat32PendingTimes[madobjloop] = (udword)oldMOP->times;
        madOpArgsThis = oldMOP->nKeyframes * madOpArgs;

        oldptr = oldbase + oldMOP->times;

        memcpy (newptr,oldptr,madOpArgsThis);

        newMOP->name = (char*)(memsize)oldMOP->name;
        newMOP->nameCRC = oldMOP->nameCRC;
        newMOP->animationBits = oldMOP->animationBits;
        newMOP->nKeyframes = oldMOP->nKeyframes;

        newMOP->times = (real32*)offset;
        newMOP->parameters = (tcb*)( offset + sizeof(real32) * oldMOP->nKeyframes);
        for (j=0; j<6 ; j++){
        
            newMOP->path[j] = (real32*) (offset + 
            (sizeof(tcb) + sizeof(real32) +(sizeof(real32) * j)) 
            * oldMOP->nKeyframes );
        }

    
        oldMOP++;
        newMOP++;
        offset +=madOpArgsThis;
        newptr +=madOpArgsThis;
    }
 
    newHeader->stringBlock = (char*)offset;

    memcpy(newptr, oldbase + oldHeader->stringBlock, oldHeader->stringBlockLength);

    newptr += oldHeader->stringBlockLength;
    offset += oldHeader->stringBlockLength;

    memFree(oldbase);
    *loadAddress = newbase;
    return offset;
}
#endif


/*-----------------------------------------------------------------------------
    Name        : madFileLoad
    Description : Load in a .MAD mesh animation file
    Inputs      : fileName - name of file to load
    Outputs     :
    Return      : newly allocated animation data
    Note        : the string block for binding information is allocated as a
                    separate block of memory so it can be freed after all
                    animation binding is complete.
----------------------------------------------------------------------------*/
madheader *madFileLoad(char *fileName)
{
    madheader header;
    madheader *newHeader;
    sdword index, j;
    sdword fileSize;
    ubyte *loadAddress;

    dbgAssertOrIgnore(fileName != NULL);

#ifdef HW_PTR_64
    fileSize = madFileLoadAndConvertTo64Bit(fileName, (void **)&loadAddress);
#else
    fileSize = fileLoadAlloc(fileName, (void **)&loadAddress, NonVolatile);     //load in the file
#endif

    memcpy(&header, loadAddress, madHeaderSize(0));

#if FIX_ENDIAN
	header.version           = FIX_ENDIAN_FLOAT_32( header.version );
	header.stringBlockLength = FIX_ENDIAN_INT_32( header.stringBlockLength );
	header.stringBlock       = ( char *)FIX_ENDIAN_INT_32( ( udword )header.stringBlock );
	header.length            = FIX_ENDIAN_FLOAT_32( header.length );
	header.framesPerSecond   = FIX_ENDIAN_FLOAT_32( header.framesPerSecond );
	header.nObjects          = FIX_ENDIAN_INT_32( header.nObjects );
	header.objPath           = ( madobjpath *)FIX_ENDIAN_INT_32( ( udword )header.objPath );
	header.nAnimations       = FIX_ENDIAN_INT_32( header.nAnimations );
#endif

#if MAD_ERROR_CHECKING
    if (strcmp(header.identifier, MAD_FileIdentifier) != 0)
    {
        dbgFatalf(DBG_Loc, "Invalid header in '%s'.  Expected '%s', found '%s'.", fileName, MAD_FileIdentifier, header.identifier);
    }
    if (header.version != MAD_FileVersion)
    {
        dbgFatalf(DBG_Loc, "Invalid file version in '%s'.  Expected %.2f, found %.2f", fileName, MAD_FileVersion, header.version);
    }
    if (header.nAnimations > MAD_MaxAnimations)
    {
        dbgFatalf(DBG_Loc, "Too many animations in '%s': %d", fileName, header.nAnimations);
    }
#endif

    newHeader = memAlloc(fileSize - header.stringBlockLength, "meshAnimation", NonVolatile);
    *newHeader = header;
#ifdef HW_PTR_64
    if (madCat32PendingTimes != NULL)
    {                                                       // CAT (M2-R9 recorder-leftovers): keep the file offsets
        MadCat32FileOffsets *e = memAlloc(sizeof(MadCat32FileOffsets), "madCat32Offsets", NonVolatile);
        e->header = newHeader;
        e->nObjects = madCat32PendingObjects;
        e->times = madCat32PendingTimes;
        e->next = madCat32Offsets;
        madCat32Offsets = e;
        madCat32PendingTimes = NULL;
        madCat32PendingObjects = 0;
    }
#endif
    memcpy(&newHeader->anim[0], loadAddress + madHeaderSize(0), fileSize - newHeader->stringBlockLength - madHeaderSize(0));

    newHeader->stringBlock = memAlloc(newHeader->stringBlockLength, "madStringBlock", NonVolatile);//!!!should this be non volatile?  I guess it depends on how soon the string block is freed, if ever.
    memcpy(newHeader->stringBlock, loadAddress + fileSize - newHeader->stringBlockLength, newHeader->stringBlockLength);
    memFree(loadAddress);

#if MAD_VERBOSE_LEVEL >= 2
    dbgMessagef("madFileLoad: loaded %d animations for %d objects from '%s'", newHeader->nAnimations, newHeader->nObjects, fileName);
#endif

    //loop through all the structures and fix up pointers
    for (index = 0; index < newHeader->nAnimations; index++)
    {                                                       //fixup the name of all animations
#if FIX_ENDIAN
		newHeader->anim[index].name      = ( char *)FIX_ENDIAN_INT_32( ( memsize )newHeader->anim[index].name );
		newHeader->anim[index].startTime = FIX_ENDIAN_FLOAT_32( newHeader->anim[index].startTime );
		newHeader->anim[index].endTime   = FIX_ENDIAN_FLOAT_32( newHeader->anim[index].endTime );
		newHeader->anim[index].flags     = FIX_ENDIAN_INT_32( newHeader->anim[index].flags );
#endif

        newHeader->anim[index].name += (memsize)newHeader->stringBlock;
    }
    newHeader->objPath = (madobjpath *)((memsize)newHeader + (ubyte *)newHeader->objPath);
    for (index = 0; index < newHeader->nObjects; index++)
    {
#if FIX_ENDIAN
		newHeader->objPath[index].name = ( char *)FIX_ENDIAN_INT_32( ( memsize )newHeader->objPath[index].name );
		newHeader->objPath[index].nameCRC = FIX_ENDIAN_INT_16( newHeader->objPath[index].nameCRC );
		newHeader->objPath[index].animationBits = FIX_ENDIAN_INT_32( newHeader->objPath[index].animationBits );
		newHeader->objPath[index].nKeyframes = FIX_ENDIAN_INT_32( newHeader->objPath[index].nKeyframes );
		newHeader->objPath[index].times = ( real32 *)FIX_ENDIAN_INT_32( ( udword )newHeader->objPath[index].times );
		newHeader->objPath[index].parameters = ( tcb *)FIX_ENDIAN_INT_32( ( udword )newHeader->objPath[index].parameters );
#endif
        newHeader->objPath[index].name += (memsize)newHeader->stringBlock;//fixup name
        newHeader->objPath[index].nameCRC = crc16Compute((ubyte *)newHeader->objPath[index].name, strlen(newHeader->objPath[index].name));
        newHeader->objPath[index].times = (real32 *)((memsize)newHeader + (ubyte *)newHeader->objPath[index].times);//fixup times pointers
        newHeader->objPath[index].parameters = (tcb *)((memsize)newHeader + (ubyte *)newHeader->objPath[index].parameters);//fixup times pointers
        for (j = 0; j < 6; j++)
        {                                                   //fixup the motion path array pointers
#if FIX_ENDIAN
			newHeader->objPath[index].path[j] = ( real32 *)FIX_ENDIAN_INT_32( ( udword )newHeader->objPath[index].path[j] );
#endif
            newHeader->objPath[index].path[j] = (real32 *)((memsize)newHeader + (ubyte *)newHeader->objPath[index].path[j]);
        }

#if FIX_ENDIAN
		for( j = 0; j < newHeader->objPath[index].nKeyframes; j++ )
		{
			int k = 0;

			newHeader->objPath[index].times[j] = FIX_ENDIAN_FLOAT_32( newHeader->objPath[index].times[j] );
			newHeader->objPath[index].parameters[j].tension = FIX_ENDIAN_FLOAT_32( newHeader->objPath[index].parameters[j].tension );
			newHeader->objPath[index].parameters[j].continuity = FIX_ENDIAN_FLOAT_32( newHeader->objPath[index].parameters[j].continuity );
			newHeader->objPath[index].parameters[j].bias = FIX_ENDIAN_FLOAT_32( newHeader->objPath[index].parameters[j].bias );

			for( k = 0; k < 6; k++ )
			{
				newHeader->objPath[index].path[k][j] = FIX_ENDIAN_FLOAT_32( newHeader->objPath[index].path[k][j] );
			}
		}
#endif
    }

    return(newHeader);
}

/*-----------------------------------------------------------------------------
    Name        : madHeaderDelete
    Description : Delete mesh amimations
    Inputs      : header - header of data to delete
    Outputs     :
    Return      :
    Note        : if header->stringBlock was freed, it should have been set to NULL
----------------------------------------------------------------------------*/
void madHeaderDelete(madheader *header)
{
    MadCat32FileOffsets **link, *e;

    for (link = &madCat32Offsets; (e = *link) != NULL; link = &e->next)
    {                                                       // CAT (M2-R9 recorder-leftovers)
        if (e->header == header)
        {
            *link = e->next;
            memFree(e->times);
            memFree(e);
            break;
        }
    }
    if (header->stringBlock != NULL)
    {
        memFree(header->stringBlock);
    }
    memFree(header);
}

/*-----------------------------------------------------------------------------
    Name        : madAnimBindingPause
    Description : Mesh binding function for scripted animations that are paused
    Inputs      :
    Outputs     :
    Return      : (Matrix not updated)
----------------------------------------------------------------------------*/
bool32 madAnimBindingPause(udword flags, hmatrix *startMatrix, hmatrix *matrixDest, void *data, smemsize ID) {
    return(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : madCurvesAdvance
    Description : The six motion curves of one animated object advanced by timeElapsed with the animation's end rule
                  (loop: wrap to the start keyframe; else clamp at the end) and evaluated: the body of
                  madAnimBindingUpdate (0x004b6720), split out so the render-time pose (HWI-FEATURE(motion-interp)
                  mesh animations) can run it on a copy of the curves
    Inputs      : anim, animation, header - the ship's animation state; curve - its object's six curves
                  timeElapsed - the time to advance by
    Outputs     : xyz, hpb - the object's position and heading / pitch / bank at the new time
----------------------------------------------------------------------------*/
static void madCurvesAdvance(Ship *ship, madanim *anim, madanimation *animation, madheader *header, splinecurve *curve, real32 timeElapsed, vector *xyz, vector *hpb)
{
    sdword j, startPoint;

    (void)ship;
    if (curve[0].timeElapsed + timeElapsed > animation->endTime)
    {                                                       //if we'll go past the end
        if (bitTest(anim->flags, MAD_ModeLoop))
        {                                                   //if this is a looping animation
            timeElapsed = (real32)fmod((double)(curve[0].timeElapsed + timeElapsed - animation->endTime), (double)(animation->endTime - animation->startTime));

            for (startPoint = 0; startPoint < curve[0].nPoints; startPoint++)
            {                                               //find a point to restart at
                if (curve[0].times[startPoint] == anim->startTime)
                {
                    goto foundTime;
                }
            }
#if MAD_ERROR_CHECKING
            dbgFatalf(DBG_Loc, "madAnimationStart: Object #%d ('%s') has no keyframe at frame %.0f",
                  anim->nCurrentAnim, header->objPath[anim->nCurrentAnim].name, anim->startTime / header->framesPerSecond);
#endif
foundTime:
            for (j = 0; j < 6; j++)
            {                                               //reset all the motion curves
                curve[j].timeElapsed = animation->startTime;
                curve[j].currentPoint = startPoint;
            }
        }
        else
        {                                                   //else it's a non-looping animation; clamp at the end
            timeElapsed = animation->endTime - curve[0].timeElapsed;//clamp to end
        }
    }
    //now update the motion curves
    xyz->x =  bsCurveUpdate(&curve[1], timeElapsed);
    xyz->y =  bsCurveUpdate(&curve[2], timeElapsed);
    xyz->z = -bsCurveUpdate(&curve[0], timeElapsed);
    hpb->x =  bsCurveUpdate(&curve[3], timeElapsed);
    hpb->y =  bsCurveUpdate(&curve[4], timeElapsed);
    hpb->z =  bsCurveUpdate(&curve[5], timeElapsed);

    dbgAssertOrIgnore(xyz->x != REALlyBig && xyz->y != REALlyBig && xyz->z != REALlyBig);
    dbgAssertOrIgnore(hpb->x != REALlyBig && hpb->y != REALlyBig && hpb->z != REALlyBig);
}

// HWI-FEATURE(motion-interp) mesh animations (M3-R4 mesh-anim-interp, docs/m3-r4-mesh-anim-interp.md): the simulation
//      advances a ship's mesh animation once per universe tick (madAnimationUpdate, 16 Hz) and the renderer evaluated
//      the curves at that time, so every frame of a tick drew the same pose (the user's "16 Hz animations" on the
//      Workers' arms and dock bays). The drawn pose now steps the curves on by the fraction of the tick elapsed at the
//      render time (miRenderAlpha, the same alpha the ships' positions are blended with; -1 outside a frame or with the
//      interpolation off), evaluated on a COPY of the curves: the simulation's curves, time and the oracle stream are
//      untouched. A paused animation holds. Not in cataclysm.exe.
bool32 catMadProbeOn = FALSE;                               // HWI-TEST: /catMadProbe (madprobe: lines)
static real32 madRenderStep(madanim *anim)
{
    real32 alpha;

    if (anim->bPaused || !miRenderDrawing())
    {
        return 0.0f;
    }
    alpha = miRenderAlpha();
    if (alpha <= 0.0f)
    {
        return 0.0f;
    }
    return alpha * universe.phystimeelapsed;
}

/*-----------------------------------------------------------------------------
    Name        : madAnimBindingUpdate
    Description : Mesh binding function for scripted mesh animations.
    Inputs      : flags - ignored
                  startMatrix - initial matrix
                  data - pointer to ship
                  ID - index of object path in the animation for this ship
    Outputs     : matrixDest - animated matrix
    Return      : TRUE = matrix updated
----------------------------------------------------------------------------*/
bool32 madAnimBindingUpdate(udword flags, hmatrix *startMatrix, hmatrix *matrixDest, void *data, smemsize ID) {
    real32 timeElapsed;
    Ship *ship = (Ship *)data;
    real32 renderStep;                                      // HWI-FEATURE(motion-interp) mesh animations
    madheader *header;
    madanim *anim = ship->madBindings;
    madobjpath *path;
    madanimation *animation;
    splinecurve *curve;
    vector hpb, xyz;
    matrix _3x3Matrix;
    hmatrix rotMatrix;

#if MR_TEST_HPB
    if (mrTestHPBMode && ID == madTestHPBIndex)
    {
        hpb.x = mrHeading;
        hpb.y = mrPitch;
        hpb.z = mrBank;
        curve = &anim->curves[ID * 6];                      //pointer to spline curves for this object
        xyz.x = curve[0].points[0];
        xyz.y = curve[1].points[0];
        xyz.z = curve[2].points[0];
        nisObjectEulerToMatrix(&_3x3Matrix, &hpb);          //make a rotation matrix
        hmatMakeHMatFromMatAndVec(&rotMatrix, &_3x3Matrix, &xyz);//make a hmatrix
        *matrixDest = rotMatrix;
        return(TRUE);
    }
#endif
    dbgAssertOrIgnore(anim != NULL);
    // CAT (M2-R6 nis-fow-camera): no animation, no update (madAnimBindingUpdate 0x004b6720)
    if (anim->nCurrentAnim == MAD_NoAnimation)
    {
        return(FALSE);
    }
    curve = &anim->curves[ID * 6];                          //pointer to spline curves for this object
    if (curve->currentPoint == BS_NoPoint)
    {                                                       //if this object not animating
        *matrixDest = *startMatrix;
        return(FALSE);                                      //don't bother updating animation
    }
    timeElapsed = anim->timeElapsed;
    header = anim->header;                                  //pointer to animation data
    path = &header->objPath[ID];                            //pointer to motion path
    // CAT: nCurrentAnim is the animation number (madstatic.animIndexes) and the mode word, not the header's
    //      MAF_Loop, says whether it loops (0x004b6720)
    animation = &header->anim[cat_madAnimIndexOfNumber(ship, anim->nCurrentAnim)];

    madCurvesAdvance(ship, anim, animation, header, curve, timeElapsed, &xyz, &hpb);   // the simulation's advance
    renderStep = madRenderStep(anim);                       // HWI-FEATURE(motion-interp) mesh animations
    if (renderStep > 0.0f)
    {
        splinecurve renderCurve[6];
        vector renderXyz, renderHpb;

        memcpy(renderCurve, curve, sizeof(renderCurve));    // the curves after the simulation's advance, copied
        madCurvesAdvance(ship, anim, animation, header, renderCurve, renderStep, &renderXyz, &renderHpb);
        if (catMadProbeOn)
        {
            static sdword lines = 0;
            if (lines < 600)
            {
                lines++;
                dbgMessagef("madprobe: t=%.4f tick=%u alpha=%.3f step=%.4f ship=%s#%u obj=%d anim=%d:%s time=%.3f sim=(%.3f,%.3f,%.3f|%.4f,%.4f,%.4f) render=(%.3f,%.3f,%.3f|%.4f,%.4f,%.4f)",
                            universe.totaltimeelapsed, (unsigned)universe.univUpdateCounter, miRenderAlpha(), renderStep,
                            ShipTypeToStr(ship->shiptype), (unsigned)ship->shipID.shipNumber, (int)ID, (int)anim->nCurrentAnim,
                            (animation->name != NULL) ? animation->name : "?", anim->time,
                            xyz.x, xyz.y, xyz.z, hpb.x, hpb.y, hpb.z, renderXyz.x, renderXyz.y, renderXyz.z, renderHpb.x, renderHpb.y, renderHpb.z);
            }
        }
        xyz = renderXyz;
        hpb = renderHpb;
    }

    nisObjectEulerToMatrix(&_3x3Matrix, &hpb);                //make a rotation matrix
    hmatMakeHMatFromMatAndVec(&rotMatrix, &_3x3Matrix, &xyz); //make a hmatrix
        *matrixDest = rotMatrix;
    //remember when this curve was last updated
    return(TRUE);
}

/*-----------------------------------------------------------------------------
    Name        : madBindingsPost
    Description : Post-render callback for animated bindings
    Inputs      : mesh - ignored
                  bindings - binding list for the ship
                  currentLOD - ignored
    Outputs     : sets the animation's time elapsed to zero
    Return      : void
----------------------------------------------------------------------------*/
void madBindingsPost(meshdata *mesh, struct shipbindings *bindings, sdword currentLOD)
{
    mhlocalbinding *binding;
    Ship *ship;

    binding = bindings->localBinding[0];
    ship = (Ship *)binding->userData;
    dbgAssertOrIgnore(ship->madBindings);
    ship->madBindings->timeElapsed = 0.0f;
}

/*-----------------------------------------------------------------------------
    Name        : madBindingListCallback
    Description : Recursive-walk function for creating binding lists
    Inputs      : mesh - mesh we're walking
                  object - object we're now stepping on
                  iObject - index of object
    Outputs     :
    Return      : FALSE (keep on walking)
----------------------------------------------------------------------------*/
bool32 madBindingListCallback(meshdata *mesh, polygonobject *object, sdword iObject)
{
    sdword j;

    //find the animation for this object
    for (j = 0; j < madDupeHeader->nObjects; j++)
    {
        if (object->nameCRC == madDupeHeader->objPath[j].nameCRC)
        {                                                   //if the CRC's match
            if (strcmp(object->pName, madDupeHeader->objPath[j].name) == 0)
            {                                               //if object name matches motion path name
                goto foundOne;
            }
        }
    }

    madBindingDest->userData = madDupeShip;                 //pass in pointer to ship
    madBindingDest->function = NULL;                       //object not in animation; no binding
    madBindingDest->object = object;                        //CAT: the object is recorded here too (madBindingListCallback
                                                            //0x004b68f0; cat_meshObjectMatrixConcat 0x0053f900 reads it)
    madBindingDest++;                                       //skip to next binding struct
    return(FALSE);
foundOne:
    madBindingDest->function = madAnimBindingUpdate;        //mesh matrix update function
    madBindingDest->userData = madDupeShip;                 //pass in pointer to ship
    madBindingDest->userID = j;                             //pass in index of motion path
    madBindingDest->flags = 0;                              //flags not needed here
    madBindingDest->object = object;                        //object to animate
    madBindingDest->matrix = IdentityHMatrix;               //CAT: starts as the identity (0x004b694c, copy of 0x0072aed8)

    madBindingDest++;                                       //skip to next binding struct
    return(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : madAnimBindingsDupe
    Description : Make ship local mesh bindings for mesh animation
    Inputs      : ship - ship to make the bindings for
                  staticInfo - static info for that ship
    Outputs     : fills in the newship->madBindings->bindings structure with
                    bindings for all LOD's of the mesh.
    Return      : void
----------------------------------------------------------------------------*/
void madAnimBindingsDupe(Ship *ship, ShipStaticInfo *staticInfo,bool32 LoadingGame)
{
    shipbindings *bindings = &ship->madBindings->bindings;
    lodinfo *LOD = staticInfo->common.LOD;
    sdword index, j, level;
    meshdata * mesh;
    splinecurve *curve;
    madobjpath *path;
    sdword curveIndex;

    madDupeHeader = staticInfo->madStatic->header;
    madDupeShip = ship;
    //splines occur right after the actual shipbinding structure
    curve = (splinecurve *)(((ubyte *)ship->madBindings) +
                                     madAnimSize(LOD->nLevels));
    ship->madBindings->curves = curve;
#if MESH_PRE_CALLBACK
    ship->madBindings->preCallback = NULL;                  //no precallback required
#endif
    ship->madBindings->bindings.postCallback = madBindingsPost;//clear the time elapsed to zero

    //binding structures allocated directly after the spline curves
    madBindingDest = (mhlocalbinding *)(((ubyte *)curve) + madDupeHeader->nObjects * sizeof(splinecurve) * 6);

    for (level = curveIndex = 0; level < LOD->nLevels; level++)
    {                                                       //for all LOD's of the ship
        if ((LOD->level[level].flags & LM_LODType) != LT_Mesh)
        {                                                   //if this level not a mesh
            bindings->localBinding[level] = NULL;           //no bindings for this level
            continue;
        }
        bindings->localBinding[level] = madBindingDest;        //binding list for this LOD
        mesh = (meshdata *)LOD->level[level].pData;         //get pointer to mesh
        meshHierarchyWalk(mesh, madBindingListCallback, NULL);
    }

    if (!LoadingGame)       // don't initialize curves, etc, if loading game
    {
        //'start' all the spline curves now.  They will be updated
        //later to reflect what animation they are to play.
        path = madDupeHeader->objPath;
        for (index = 0; index < madDupeHeader->nObjects; index++, path++)
        {
            for (j = 0; j < 6; j++, curve++)
            {
                bsCurveStartPrealloced(curve, path->nKeyframes, path->path[j], path->times, path->parameters);
            }
        }

        // CAT (M2-R6 nis-fow-camera): the current animation becomes the previous one (madAnimBindingsDupe 0x004b6a10)
        ship->madBindings->catPreviousAnim = ship->madBindings->nCurrentAnim;
        ship->madBindings->nCurrentAnim = MAD_NoAnimation;      //start off by doing no animation
        ship->madBindings->bPaused = FALSE;
    }
    // CAT: the animation bindings get their parent links too (madAnimBindingsDupe 0x004b6a10 ends with
    //      cat_univMeshBindingsLinkParents 0x00541ae0).
    cat_univMeshBindingsLinkParents(staticInfo, bindings);
}

/*-----------------------------------------------------------------------------
    Name        : cat_madAnimClear
    Description : CAT (M2-R6 nis-fow-camera): a new ship's madanim before the bindings are made (cat_madAnimClear
                  0x004b69d0, univCreateShip): everything 0 but the current and previous animations (none)
----------------------------------------------------------------------------*/
void cat_madAnimClear(madanim *anim)
{
    anim->header = NULL;
    anim->totalSize = 0;
    anim->flags = 0;
    anim->nCurrentAnim = MAD_NoAnimation;
    anim->catPreviousAnim = MAD_NoAnimation;
    anim->bPaused = FALSE;
    anim->catBindingSwapPending = 0;
    anim->startTime = 0.0f;
    anim->time = 0.0f;
    anim->timeElapsed = 0.0f;
    anim->curves = NULL;
    anim->saveBindings = NULL;
    anim->bindings.frequencyCounter = 0;                    // the exe's +0x30, +0x34
    anim->bindings.postCallback = NULL;
}

/*-----------------------------------------------------------------------------
    Name        : cat_madAnimIndexOfNumber
    Description : CAT: the header index of an animation number: madstatic.animIndexes when the .shp numbers its
                  animations (animsByNumber), else the number itself (madAnimationStart, madAnimationUpdate and
                  madAnimBindingUpdate inline it)
----------------------------------------------------------------------------*/
sdword cat_madAnimIndexOfNumber(Ship *ship, sdword number)
{
    madstatic *mstatic = ship->staticinfo->madStatic;

    if (mstatic->animsByNumber)
    {
        // CAT-FIX: the exe reads animIndexes[number] unchecked
        return ((number >= 0) && (number < mstatic->numAnims)) ? mstatic->animIndexes[number] : -1;
    }
    return number;
}

/*-----------------------------------------------------------------------------
    Name        : madAnimationStart
    Description : Start an animation playing for a given ship
    Inputs      : ship - ship to play the animation for
                  animNumber - the animation number (CAT: mapped through madstatic.animIndexes when the .shp numbers
                      its animations)
                  mode - CAT: the exe's third argument, kept in madanim.flags (MAD_ModeLoop, MAD_ModeBackToPrevious;
                      0 pauses on the last frame)
    Outputs     : sets time, rate and splinecurves up for an animation.
    Return      : void
----------------------------------------------------------------------------*/
// CAT (M2-R6 nis-fow-camera): madAnimationStart 0x004b6b90: a playing animation becomes the previous one (a paused
//      one is unpaused first); the bindings swap waits for the next madAnimationUpdate (+0x18); the participation
//      bit is the animation NUMBER's (the exe takes it before the lookup); a curve with no keyframe at the start time
//      starts past its last point (no fatal); the mode is stored last.
void madAnimationStart(Ship *ship, sdword animNumber, sdword mode)
{
    madanim *anim = ship->madBindings;
    madheader *header;
    sdword index, j, startPoint, animIndex;
    splinecurve *curve;
    madanimation *animation;
    udword animBit = (1u << (animNumber & 31));

    // CAT-FIX: the exe assumes a ship with mesh animations and a valid number (its callers ensure it)
    if ((anim == NULL) || ((animIndex = cat_madAnimIndexOfNumber(ship, animNumber)) < 0) ||
        (animIndex >= anim->header->nAnimations))
    {
        return;
    }
    header = anim->header;

    if (anim->nCurrentAnim == MAD_NoAnimation)
    {
        anim->catPreviousAnim = MAD_NoAnimation;
    }
    else
    {
        if (anim->bPaused)
        {
            madAnimationPause(ship, FALSE);
        }
        anim->catPreviousAnim = anim->nCurrentAnim;
        anim->nCurrentAnim = MAD_NoAnimation;
    }
    anim->catBindingSwapPending = 1;
    anim->nCurrentAnim = animNumber;

    animation = &header->anim[animIndex];
    anim->startTime = anim->time = animation->startTime;
    anim->timeElapsed = 0.0f;

    //start all the b-splines.  They've already been set up,
    //we just need to initialize the time and current point.
    curve = anim->curves;
    for (index = 0; index < header->nObjects; index++)
    {
        if (!bitTest(header->objPath[index].animationBits, animBit))
        {                                                   //if this object doesn't need to participate in the animation
            curve->currentPoint = BS_NoPoint;               //set the curves to not update
            curve += 6;
            continue;
        }
        for (startPoint = 0; startPoint < curve->nPoints; startPoint++)
        {
            if (curve->times[startPoint] == anim->time)
            {
                break;
            }
        }
        for (j = 0; j < 6; j++, curve++)
        {
            curve->timeElapsed = anim->time;
            curve->currentPoint = startPoint;
        }
    }
    anim->flags = (udword)mode;
#if MAD_VERBOSE_LEVEL >= 2
    dbgMessagef("madAnimationStart: started animation #%d('%s') on ship 0x%x", animNumber, animation->name, ship);
#endif
}

/*-----------------------------------------------------------------------------
    Name        : madAnimationUpdate
    Description : Update the mesh animation for an object
    Inputs      : ship - ship to update the animation for
                  timeElapsed - time elapsed since last updated
    Outputs     :
    Return      : TRUE if the animation ends
----------------------------------------------------------------------------*/
// CAT (M2-R6 nis-fow-camera): madAnimationUpdate 0x004b6ce0: the deferred binding swap first (the gun bindings are
//      saved once); at the animation's first frame the type's animation sound (cat_madAnimSoundEvent: the first 60
//      types, animations 0-5); at the end the mode decides: loop (bit 0), pause on the last frame and stay current
//      (0: madAnimationPause(ship, TRUE), TRUE returned), or end as madAnimationStop (bit 1). The header's MAF_Loop
//      is not read.
bool32 madAnimationUpdate(Ship *ship, real32 timeElapsed)
{
    madanim *anim = ship->madBindings;
    madheader *header = anim->header;
    madanimation *animation;
    sdword number, index, sound;

    dbgAssertOrIgnore(anim != NULL);

    if (anim->catBindingSwapPending != -1)
    {
        anim->catBindingSwapPending = -1;
        if (anim->saveBindings == NULL)
        {
            anim->saveBindings = ship->bindings;
        }
        ship->bindings = &anim->bindings;
    }
    number = anim->nCurrentAnim;
    index = cat_madAnimIndexOfNumber(ship, number);
    dbgAssertOrIgnore(index != MAD_NoAnimation);
    animation = &header->anim[index];

    if (anim->bPaused)
    {                                                       //if paused
        return(FALSE);                                      //just chillin'
    }

    if ((anim->time == animation->startTime) && ((sound = cat_madAnimSoundEvent(ship->shiptype, number)) != SOUND_NOTINITED))
    {
        soundEventPlay(ship, sound, NULL);
    }

    if (anim->time + timeElapsed > animation->endTime)
    {                                                       //if we'll go past the end
        if (bitTest(anim->flags, MAD_ModeLoop))
        {                                                   //if this is a looping animation
            do
            {
                anim->time = anim->time + timeElapsed - animation->endTime + animation->startTime;
            }
            while (anim->time + timeElapsed > animation->endTime);
            anim->timeElapsed += timeElapsed;
            return(FALSE);
        }
        if (!bitTest(anim->flags, MAD_ModeBackToPrevious))
        {                                                   // CAT: pause on the last frame
            anim->time = animation->endTime;
            madAnimationPause(ship, TRUE);
            return(TRUE);
        }
        madAnimationStop(ship);
        return(TRUE);
    }
    anim->time += timeElapsed;
    anim->timeElapsed += timeElapsed;
    return(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : cat_madAnimationFinished
    Description : CAT: TRUE when ship plays no mesh animation, its animation is paused, or it has reached the end of
                  the animation (cat_madAnimationFinished 0x004b7090)
    Inputs      : ship - a ship with mesh animation bindings
    Return      : TRUE when the animation is over
    Notes       : the exe maps nCurrentAnim (the number) through madstatic.animIndexes unconditionally (its callers'
                  types number their animations); CAT-FIX: cat_madAnimIndexOfNumber uses the number itself otherwise
----------------------------------------------------------------------------*/
bool32 cat_madAnimationFinished(Ship *ship)
{
    madanim *anim = ship->madBindings;

    if ((anim->nCurrentAnim != MAD_NoAnimation) &&
        (anim->time < anim->header->anim[cat_madAnimIndexOfNumber(ship, anim->nCurrentAnim)].endTime) &&
        (!anim->bPaused))
    {
        return FALSE;
    }
    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : madAnimationStop
    Description : Stop an animation from playing.
    Inputs      : ship - ship to stop animating
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT (M2-R6 nis-fow-camera): madAnimationStop 0x004b6e70: unpause first; the saved gun bindings come back only if
//      they were swapped out (and are forgotten); the animation becomes the previous one
void madAnimationStop(Ship *ship)
{
    madanim *anim = ship->madBindings;

    dbgAssertOrIgnore(anim != NULL);
    if (anim->bPaused)
    {
        madAnimationPause(ship, FALSE);
    }
    if (anim->saveBindings != NULL)
    {
        ship->bindings = anim->saveBindings;                //restore the saved gun bindings
        anim->saveBindings = NULL;
    }
#if MAD_VERBOSE_LEVEL >= 2
    dbgMessagef("madAnimationStop: stopped animation #%d on ship 0x%x", anim->nCurrentAnim, ship);
#endif
    anim->catPreviousAnim = anim->nCurrentAnim;
    anim->nCurrentAnim = MAD_NoAnimation;
}

/*-----------------------------------------------------------------------------
    Name        : madAnimIndexFindByName
    Description : Find the index of an animation by name
    Inputs      : header - header to search
                  name - name of animation to find
    Outputs     :
    Return      : index of animation, -1 if not found (CAT; HW1 generated a fatal error) [audit: comment was stale]
----------------------------------------------------------------------------*/
sdword madAnimIndexFindByName(madheader *header, char *name)
{
    sdword index;

    dbgAssertOrIgnore(header->stringBlock != NULL);

    for (index = 0; index < header->nAnimations; index++)
    {
        if (strcasecmp(header->anim[index].name, name) == 0)
        {
            return(index);
        }
    }
    // CAT: a missing animation returns -1 without the fatal error (madAnimIndexFindByName 0x004b6ed0 calls only
    //      _stricmp; setMadAnim 0x00539f70 warns instead). Cataclysm data names animations its .mad lacks, e.g.
    //      "Link" (statics self-test um-r4-statics)
    return(-1);
}

/*-----------------------------------------------------------------------------
    Name        : madBunBindingIndexFindByName
    Description : Find the index of a named mesh binding.
    Inputs      : info - static info to look for info in
                  name - name of binding to search for.
    Outputs     :
    Return      : index of named mesh object, if -1 if not found
----------------------------------------------------------------------------*/
sdword madGunBindingIndexFindByName(ShipStaticInfo *info, char *name)
{
    sdword index;
    mhbinding *list = info->common.LOD->level[0].hBindings;
    meshdata *mesh = (meshdata *)info->common.LOD->level[0].pData;

    dbgAssertOrIgnore(list != NULL);
    dbgAssertOrIgnore(mesh != NULL);

    for (index = 0; index < mesh->nPolygonObjects; index++)
    {                                                       //search rest of list
        if (list->object->pName != NULL)
        {
            if (strcmp(list->object->pName, name) == 0)
            {                                               //if names match
                return(index);                              //all done
            }
        }
        list++;                                             //update pointer
    }
    return(-1);
}


/*-----------------------------------------------------------------------------
    Name        : madBindingFindCallback
    Description : Mesh-walk callback for finding a mesh binding by name.
    Inputs      : mesh - mesh we're searching
                  object - current object we're on
                  iObject - index of object
    Outputs     :
    Return      : TRUE means stop walking, take a rest
----------------------------------------------------------------------------*/
/*
bool32 madBindingFindCallback(meshdata *mesh, polygonobject *object, sdword iObject)
{
    sdword j;

    if (!strcmp(object->pName, madDupeName))
    {
        madBindingIndex = madIncIndex;
        return(TRUE);
    }
    madIncIndex++;                                          //skip to next binding struct

    return(FALSE);
}
*/
/*-----------------------------------------------------------------------------
    Name        : madBindingIndexFindByName
    Description : Find the index of a binding by it's object's name.
    Inputs      : header - header of
    Outputs     :
    Return      : index of first binding to match the name or -1 if not found in animation.
----------------------------------------------------------------------------*/
sdword madBindingIndexFindByName(madheader *header, char *name)
{
    /*
    sdword level;
    dbgAssertOrIgnore(header != NULL);
    dbgAssertOrIgnore(name != NULL);

    madBindingIndex = 0xffffffff;
    madBindingName = name;
    for (level = madIncIndex = 0; level < LOD->nLevels; level++)
    {
        if ((LOD->level[level].flags & LM_LODType) != LT_Mesh)
        {                                                   //if this level not a mesh
            continue;
        }
        meshHierarchyWalk((meshdata *)LOD->level[level].pData, madBindingFindCallback, NULL);
    }

    dbgAssertOrIgnore(madBindingIndex != 0xffffffff);
    return(madBindingIndex);
    */
    sdword index;

    for (index = 0; index < header->nObjects; index++)
    {
        if (!strcmp(header->objPath[index].name, name))
        {
            return(index);                                  //found, return index of it
        }
    }
    return(-1);                                             //not found, return error code
}

/*-----------------------------------------------------------------------------
    Name        : madAnimFreeze
    Description : Pause or unpause an animation in it's current position.
    Inputs      : ship - ship to pause
                  bFreeze - TRUE to pause, false to resume
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void madAnimationPause(Ship *ship, bool32 bFreeze)
{
    madanim *anim = ship->madBindings;
    sdword index;
    mhlocalbinding *binding;

    dbgAssertOrIgnore(anim != NULL);

    binding = anim->bindings.localBinding[0];

    if (bFreeze)
    {                                                       //if freezing
        for (index = ship->staticinfo->hierarchySize - 1; index >= 0; index--, binding++)
        {
            if (binding->function == madAnimBindingUpdate)
            {                                               //if this binding animating
                madAnimBindingUpdate(binding->flags, &binding->object->localMatrix,//bring animation 'up to date'
                              &binding->matrix, binding->userData, binding->userID);
                binding->function = madAnimBindingPause;    //simplified update function
            }
        }
    }
    else
    {                                                       //else unpausing
        for (index = ship->staticinfo->hierarchySize - 1; index >= 0; index--, binding++)
        {
            if (binding->function == madAnimBindingPause)
            {                                               //if this binding animating
                binding->function = madAnimBindingUpdate;   //simplified update function
            }
        }
    }
    anim->bPaused = bFreeze;
}

/*-----------------------------------------------------------------------------
    Name        : madAnimBindingMatrix
    Description : Retrieve the matrix for a given ship's animation bindings.
    Inputs      : ship - ship to get matrix for
                  gunIndex - index returned by meshBindingIndexFindByName
                  madIndex - index returned by madAnimIndexFindByName
    Outputs     : destMatrix - where to put the matrix
                  destPos - where to put the position vector
    Return      : pointer to matrix
----------------------------------------------------------------------------*/
void madAnimBindingMatrix(matrix *destMatrix, vector *destPos, Ship *ship, sdword gunIndex, sdword madIndex)
{
    madanim *anim = ship->madBindings;
    splinecurve *curve;
    lodinfo *lodInfo;
    mhbinding *bindingListSource;
    hmatrix *hMatrix;
    hmatrix hMatrixTemp;
    meshdata *mesh;
    splinecurve saveCurve[6];
    real32 saveTimeElapsed, saveTime;
    sdword index;

    dbgAssertOrIgnore(anim != NULL);
    if (anim->nCurrentAnim != MAD_NoAnimation)
    {                                                       //if there's an animation playing
        curve = &anim->curves[madIndex * 6];                //pointer to spline curves for this object
        memcpy(saveCurve, curve, sizeof(splinecurve) * 6);  //save copies of the spline curves
        saveTime = anim->time;                              //save time elapsed flags
        saveTimeElapsed = anim->timeElapsed;
        for (index = 0; index < 6; index++)
        {                                                   //rewind all the splines
            curve[index].timeElapsed = 0.0f;
            curve[index].currentPoint = 0;
        }
        anim->timeElapsed = anim->time;                     //advance all splines to deterministic animation point
        madAnimBindingUpdate(0, NULL, &hMatrixTemp, (void *)ship, madIndex);//make like we're updating the curves
        memcpy(curve, saveCurve, sizeof(splinecurve) * 6);  //restore the saved curves
        anim->time = saveTime;                              //restore animation time info
        anim->timeElapsed = saveTimeElapsed;
        matGetMatFromHMat(destMatrix, &hMatrixTemp);        //convert to 3x3/position
        hmatGetVectFromHMatrixCol4(*destPos, hMatrixTemp);
    }
    else
    {                                                       //no animation playing
        lodInfo = ship->staticinfo->common.LOD;
        dbgAssertOrIgnore((lodInfo->level[0].flags & LM_LODType) == LT_Mesh);//make sure right type
        mesh = (meshdata *)lodInfo->level[0].pData;
        bindingListSource = lodInfo->level[0].hBindings;    //pointer to LOD0 bindings
        dbgAssertOrIgnore(gunIndex < mesh->nPolygonObjects);
        hMatrix = &mesh->object[gunIndex].localMatrix;      //get the matrix
        matGetMatFromHMat(destMatrix, hMatrix);             //convert to 3x3/position
        hmatGetVectFromHMatrixCol4(*destPos, *hMatrix);
    }
}

/*=============================================================================
    Save Game Stuff:
=============================================================================*/

void PreFix_madBindings(Ship *ship,Ship *fixcontents)
{
    sdword i;

    sdword madOffset = ((ubyte *)ship->madBindings) - ((ubyte *)ship);
    madanim *madBindings = (madanim *)(((ubyte *)fixcontents) + madOffset);

    sdword curvesOffset = ((ubyte *)ship->madBindings->curves) - ((ubyte *)ship->madBindings);
    splinecurve *curvesToModify = (splinecurve *)(((ubyte *)madBindings) + curvesOffset);

    sdword numCurves = 6 * madBindings->header->nObjects;

    for (i=0;i<numCurves;i++)
    {
        // store offsets from madBindings->header
        curvesToModify[i].points = (real32 *) (((ubyte *)curvesToModify[i].points) - ((ubyte *)madBindings->header));
        curvesToModify[i].times = (real32 *) (((ubyte *)curvesToModify[i].times) - ((ubyte *)madBindings->header));
        curvesToModify[i].params = (tcb *) (((ubyte *)curvesToModify[i].params) - ((ubyte *)madBindings->header));
    }

    // store offset from madBindings
    //madBindings->curves = ((ubyte *)&madBindings->curves) - ((ubyte *)madBindings);
}

void Save_madBindings(Ship *ship)
{

}

void Load_madBindings(Ship *ship)
{

}

void Fix_madBindings(Ship *ship)
{
    madanim *madBindings = ship->madBindings;
    splinecurve *curve;
    sdword i;
    sdword numCurves;

    madBindings->header = ship->staticinfo->madStatic->header;

    madAnimBindingsDupe(ship,ship->staticinfo,TRUE);

    numCurves = 6 * madBindings->header->nObjects;

    // fix, offset from madBindings
    //madBindings->curves = ((ubyte *)madBindings) + ((udword)madBindings->curves);

    for (i=0;i<numCurves;i++)
    {
        curve = &madBindings->curves[i];

        // fix, offsets from madBindings->header
        curve->points = (real32 *)(((ubyte *)madBindings->header) + ((udword)curve->points));
        curve->times = (real32 *)(((ubyte *)madBindings->header) + ((udword)curve->times));
        curve->params = (tcb *)(((ubyte *)madBindings->header) + ((udword)curve->params));
    }

    if (madBindings->nCurrentAnim == MAD_NoAnimation)
    {
        madBindings->saveBindings = NULL;
    }
    else
    {
        madBindings->saveBindings = ship->bindings;
        ship->bindings = &madBindings->bindings;
    }
}
