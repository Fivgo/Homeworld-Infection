/*=============================================================================
    Name    : MeshAnim.h
    Purpose : Definitions for scripted mesh animations.

    Created 7/21/1998 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___MESHANIM_H
#define ___MESHANIM_H

#include <stddef.h>

#include "B-Spline.h"
#include "CRC32.h"
#include "Mesh.h"

/*=============================================================================
    Switches:
=============================================================================*/
#define MAD_TEST_ANIMATION      0               //test animations
#ifdef HW_BUILD_FOR_DEBUGGING

#define MAD_ERROR_CHECKING      1               //basic error checking
#define MAD_VERBOSE_LEVEL       1               //control verbose printing

#else

#define MAD_ERROR_CHECKING      0               //no error ckecking in retail
#define MAD_VERBOSE_LEVEL       0               //don't print any verbose strings in retail

#endif


/*=============================================================================
    Definitions:
=============================================================================*/
#define MAD_FileVersion         0.91f
#define MAD_FileIdentifier      "Mellonn"
#define MAD_Extension           ".MAD"
#define MAD_MaxAnimations       32

//animation flags
#define MAF_Loop                0x00000001      //this animation will loop
#define MAD_NoAnimation         -1              //nCurrentAnim will be this if no animation playing

/*=============================================================================
    Type definitions
=============================================================================*/
//structure for a single object's keyframe list in the file.
typedef struct
{
    char *name;                                 //name of the object for this keyframe list
    crc16 nameCRC;                              //16-bit crc of the name.  (should upgrade to 32 later).
    ubyte pad[2];
    udword animationBits;                       //bit-flags: one for each animation
    sdword nKeyframes;                          //number of keyframes in all of file for this object
    real32 *times;                              //times array
    tcb *parameters;                            //tension, continuity, bias
    real32 *path[6];                            //arrays of x,y,z,h,p,b
}
madobjpath;

//structure for a single animation within the file
typedef struct
{
    char *name;                                 //name of the animation
    real32 startTime;                           //limits of the animation
    real32 endTime;
    udword flags;                               //flags for this animation
}
madanimation;

//structure for the header of a mesh animation file
typedef struct
{
    char identifier[8];                         //compared to MAD_FileIdentifier
    real32 version;                             //compared to MAD_FileVersion
    sdword stringBlockLength;                   //length of string block
    char *stringBlock;                          //string block: where all strings for this file are located and offsets are relative to
    real32 length;                              //length of all animations
    real32 framesPerSecond;                     //rate the animation was scripted at
    sdword nObjects;                            //number of objects in the animation (should be same as number of objects in the mesh to animate)
    madobjpath *objPath;                        //list of keyframe lists for each object
    sdword nAnimations;                         //number of separate animations in the file
    madanimation anim[1];                       //ragged array of animation headers
}
madheader;

//structure for animation data in shipstaticinfo
typedef struct
{
    madheader *header;                          //animation data loaded from disk
    sdword needStartPlacedAnimation;                   //True or false flag indicating the object possesing these animations needs a starting animation/pause hack (thats right luke, a hack!)
    sdword startPlacedAnimationIndex;                 //if above flag is true, this index is valid and points to that starting animation
    sdword needStartBuiltAnimation;                   //True or false flag indicating the object possesing these animations needs a starting animation/pause hack (thats right luke, a hack!)
    sdword startBuiltAnimationIndex;                 //if above flag is true, this index is valid and points to that starting animation
    //extra context sensitive animation data
    sdword numGunOpenIndexes;                   //number of below indices
    sdword numGunOpenDamagedIndexes;                   //number of below indices
    sdword numGunCloseIndexes;                   //number of below indices
    sdword numGunCloseDamagedIndexes;                   //number of below indices
    sdword *gunOpenIndexes;                      //arrays for gunopening/closing animation indexes both damaged and undamaged.  So we can support a number of different animationsfor the same ship
    sdword *gunOpenDamagedIndexes;
    sdword *gunCloseIndexes;
    sdword *gunCloseDamagedIndexes;
    //same, but for docking animations
    sdword numDockIndexes;                   //number of below indices
    sdword numDockDamagedIndexes;                   //number of below indices
    sdword numPostDockIndexes;                   //number of below indices
    sdword numPostDockDamagedIndexes;                   //number of below indices
    sdword *DockIndexes;                      //arrays for gunopening/closing animation indexes both damaged and undamaged.  So we can support a number of different animationsfor the same ship
    sdword *DockDamagedIndexes;
    sdword *PostDockIndexes;
    sdword *PostDockDamagedIndexes;
    sdword numDoorOpenIndexes;
    sdword numDoorCloseIndexes;
    sdword *DoorOpenIndexes;
    sdword *DoorCloseIndexes;
    sdword numSpecialOpenIndexes;
    sdword numSpecialCloseIndexes;
    sdword numSpecialOpenDamagedIndexes;
    sdword numSpecialCloseDamagedIndexes;
    sdword *specialOpenIndexes;
    sdword *specialCloseIndexes;
    sdword *specialOpenDamagedIndexes;
    sdword *specialCloseDamagedIndexes;
    // CAT: madstatic is 0x90 bytes in Cataclysm: a table of animations by number, filled by the NUMBER_OF_ANIMS and
    //      ANIM keys (cat_madNumberOfAnimsSetCB 0x00539e40, setMadAnim 0x00539f70) and read by
    //      cat_madAnimationFinished 0x004b7090
    sdword numAnims;                            /*+0x84*/
    sdword *animIndexes;                        /*+0x88 madheader animation index per number, -1 if none */
    sdword animsByNumber;                       /*+0x8C set to 1 by NUMBER_OF_ANIMS */
}
madstatic;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(madstatic) == 0x90, "madstatic is 0x90 bytes in cataclysm.exe (cat_madNumberOfAnimsSetCB 0x00539e40)");
#endif

//structure of animation data stored within a ship
// CAT (M2-R6 nis-fow-camera): the exe's madanim (cat_madAnimClear 0x004b69d0 clears 0x38 bytes, madAnimationStart
//      0x004b6b90 / madAnimationUpdate 0x004b6ce0 / madAnimationStop 0x004b6e70 use the words): flags is the mode
//      madAnimationStart's third argument sets (MAD_ModeLoop, MAD_ModeBackToPrevious; 0 pauses on the last frame),
//      nCurrentAnim the animation NUMBER (madstatic.animIndexes maps it to the header's index when animsByNumber),
//      two new words: the previous animation (+0x10) and the deferred binding swap (+0x18: set by the start, done by
//      the next update)
typedef struct
{
    madheader *header;                          //static animation data
    sdword totalSize;                           //size of this structure, bindings, motion curves and the whole bit
    udword flags;                               // +0x08 CAT: the mode (madAnimationStart's third argument)
    sdword nCurrentAnim;                        // +0x0C animation number we're currently on, or MAD_NoAnimation for none
    sdword catPreviousAnim;                     // +0x10 CAT: the animation before it (MAD_NoAnimation for none)
    bool32   bPaused;                             // +0x14 TRUE when the animation is paused
    sdword catBindingSwapPending;               // +0x18 CAT: 1 after a start: the next update swaps in the animation bindings; -1 done
    real32 startTime;                           //time the animation starts at
    real32 time;                                //time within the animation, from starttime to endtime inclusive
    real32 timeElapsed;                         //time elapsed since last rendered
    splinecurve *curves;                        //spline curve array, enough for 6 * number of objects to animate
    shipbindings *saveBindings;                 //gun-mesh bindings are saved here when scripted animation is going on.
    shipbindings bindings;                      //animation bindings for this ship, must be last in list; variable size
}
madanim;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(madanim, catBindingSwapPending) == 0x18, "madanim +0x18 in cataclysm.exe (madAnimationUpdate 0x004b6ce0)");
_Static_assert(offsetof(madanim, curves) == 0x28, "madanim.curves at 0x28 (madAnimBindingsDupe 0x004b6a10)");
_Static_assert(offsetof(madanim, saveBindings) == 0x2C, "madanim.saveBindings at 0x2C");
_Static_assert(offsetof(madanim, bindings) == 0x30, "madanim.bindings at 0x30");
#endif

// CAT: the modes of madAnimationStart's third argument (madanim.flags; madAnimationUpdate 0x004b6ce0 tests bit 0, then bit 1)
#define MAD_ModePauseAtEnd      0x00000000      // the animation pauses on its last frame and stays current
#define MAD_ModeLoop            0x00000001      // it loops
#define MAD_ModeBackToPrevious  0x00000002      // it ends as madAnimationStop does (bindings back, current -> previous)


typedef struct
{
    udword name; //*                                //name of the object for this keyframe list
    crc16 nameCRC;                              //16-bit crc of the name.  (should upgrade to 32 later).
    ubyte pad[2];
    udword animationBits;                       //bit-flags: one for each animation
    sdword nKeyframes;                          //number of keyframes in all of file for this object
    udword times; //real32*                     //times array
    udword parameters;   //tcp*                   //tension, continuity, bias
    udword path[6];  //real32*                 //arrays of x,y,z,h,p,b
}
madobjpath_disk;

typedef struct
{
    udword name;   // *                                //name of the animation
    real32 startTime;                           //limits of the animation
    real32 endTime;
    udword flags;                               //flags for this animation
}
madanimation_disk;

typedef struct
{
    char identifier[8];                         //compared to MAD_FileIdentifier
    real32 version;                             //compared to MAD_FileVersion
    sdword stringBlockLength;                   //length of string block
    udword stringBlock;  //*                        //string block: where all strings for this file are located and offsets are relative to
    real32 length;                              //length of all animations
    real32 framesPerSecond;                     //rate the animation was scripted at
    sdword nObjects;                            //number of objects in the animation (should be same as number of objects in the mesh to animate)
    udword objPath; //*                       //list of keyframe lists for each object
    sdword nAnimations;                         //number of separate animations in the file
    madanimation_disk anim[1];                       //ragged array of animation headers
}
madheader_disk;


/*=============================================================================
    Macros:
=============================================================================*/
#define madHeaderSize(n)    ((sdword)sizeof(madheader) + (sdword)sizeof(madanimation) * ((n) - 1))
#define madAnimSize(n)      (sizeof(madanim) - sizeof(shipbindings) + shipBindingsLength(n))

/*=============================================================================
    Functions:
=============================================================================*/
struct Ship;
struct ShipStaticInfo;

bool32 madCat32ObjectFileOffset(madheader *header, sdword object, udword *offset);  // CAT (M2-R9 recorder-leftovers)
madheader *madFileLoad(char *fileName);
void madHeaderDelete(madheader *header);

//make ship local bindings for mesh animation
void madAnimBindingsDupe(struct Ship *ship, struct ShipStaticInfo *staticInfo,bool32 LoadingGame);

//play an animation
void madAnimationStart(struct Ship *ship, sdword animNumber, sdword mode);     // CAT: the exe's third argument (M2-R6 nis-fow-camera)
void cat_madAnimClear(madanim *anim);                                           // CAT: 0x004b69d0
void madAnimationPause(struct Ship *ship, bool32 bFreeze);
bool32 madAnimationUpdate(struct Ship *ship, real32 timeElapsed);
void madAnimationStop(struct Ship *ship);
bool32 cat_madAnimationFinished(struct Ship *ship);     // CAT: 0x004b7090 (M2-R4 combat: the sCarrier door)
sdword cat_madAnimIndexOfNumber(struct Ship *ship, sdword number);     // CAT: the animIndexes lookup the exe inlines

//find things in animations
sdword madAnimIndexFindByName(madheader *header, char *name);
sdword madGunBindingIndexFindByName(struct ShipStaticInfo *info, char *name);
sdword madBindingIndexFindByName(madheader *header, char *name);
void madAnimBindingMatrix(matrix *destMatrix, vector *destPos, struct Ship *ship, sdword gunIndex, sdword madIndex);
extern bool32 catMadProbeOn;                            // HWI-TEST (M3-R4 mesh-anim-interp): /catMadProbe (madprobe: / spinprobe: lines)

void PreFix_madBindings(struct Ship *ship,struct Ship *fixcontents);
void Save_madBindings(struct Ship *ship);
void Load_madBindings(struct Ship *ship);
void Fix_madBindings(struct Ship *ship);

#endif
