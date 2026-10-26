/*=============================================================================
    Name    : Trails.h
    Purpose : Definitions for drawing ship trails

    Created 8/11/1997 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___TRAILS_H
#define ___TRAILS_H

#include "Color.h"
#include "MaxMultiplayer.h"
#include "Types.h"
#include "Vector.h"

/*=============================================================================
    Switches:
=============================================================================*/
#ifdef HW_BUILD_FOR_DISTRIBUTION

#define TRAIL_ERROR_CHECKING       0               //general error checking
#define TRAIL_VERBOSE_LEVEL        0               //print extra info
#define TRAIL_GATHER_STATS         0               //gather performance stats

#else

#define TRAIL_ERROR_CHECKING       1               //general error checking
#define TRAIL_VERBOSE_LEVEL        1               //print extra info
#define TRAIL_GATHER_STATS         0               //gather performance stats

#endif

/*=============================================================================
    Definitions:
=============================================================================*/
#if FIX_ENDIAN
    #define TP_KeyColor  0xffffff00
#else
    #define TP_KeyColor  0x00ffffff  //high bit (alpha) clear means this is a key color
#endif

/*=============================================================================
    Type definitions:
=============================================================================*/
//static trail information block
typedef struct
{
    sdword nSegments;                           //number in segments in trail
    sdword granularity;                         //frame-granularity counter.  Will act as a trail-length multiplier
    color *segmentColor[MAX_MULTIPLAYER_PLAYERS];//color of trail at each segment for each team
}
trailstatic;

typedef struct
{
    real32 position[3];
    real32 rotation[9];
    real32 translation[3];
    real32 horizontal[3];
    real32 vertical[3];
    real32 direction[3];
    bool8  wide;
    ubyte  pad[3];
}
trailsegment;

typedef struct
{
    vector position;
}
missiletrailsegment;

#define TRAIL_CONTRACTING 1
#define TRAIL_EXPANDING   2
#define TRAIL_CONTRACTED  4
#define TRAIL_EXPANDED    8

//dynamic trail structure
typedef struct
{
    trailstatic *staticInfo;            //non-dynamic trail info reference
    void*  vship;

    real32 width, height, angle;        //== -1.0f if defaults should be used
    real32 ribbonadjust;
    bool8  style;
    bool8  second;
    ubyte  trailNum;
    ubyte  state;
    ubyte  counter;

    bool32   wobbly;

    real32 lastvelsquared;
    real32 prevvelsquared;
    sdword ranCounter;
    real32 ran;
    real32 exponent;
    real32 scalecap;
    sdword grainCounter;                //counter to reflect if the trail should be updated this time around
    sdword iHead, iTail, nLength;       //circular queue members
    sbyte  hwiSplit;                    // HWI-FEATURE(engine-trails): 0 the exe's trail; +1 the exe's trail split to its upper circle; -1 the added lower one
    ubyte  hwiSplitType;                // HWI-FEATURE(engine-trails): the row of hwiTrailSplits[] (Trails.c)
    trailsegment segments[1];           //trail segments
}
shiptrail;

typedef struct
{
    trailstatic* staticInfo;
    void* vmissile;

    real32 width, height;
    sdword grainCounter;
    sdword iHead, iTail, nLength;
    missiletrailsegment segments[1];
}
missiletrail;

/*=============================================================================
    Data:
=============================================================================*/
extern sdword trailInsertCount;
extern sdword TRAIL_EXPANSION_TICKS;    // CAT: read by cat_shGlowUpdate (Shader.c)

/*=============================================================================
    Macros:
=============================================================================*/
#define mistrailSize(n)     (sizeof(missiletrail) + (n)*sizeof(missiletrailsegment))
#define trailSize(n)        (sizeof(shiptrail) + (n)*sizeof(trailsegment))
#define trailStaticSize(n)  (sizeof(trailstatic) + (n) * sizeof(color) * MAX_MULTIPLAYER_PLAYERS)
#define trailInsertTrail()  ((trailInsertCount ^= TRUE) != 0)

/*=============================================================================
    Functions:
=============================================================================*/

//startup/shutdown
void trailStartup(void);
void trailShutdown(void);

//allocate and initialize a new missile trail
missiletrail* mistrailNew(trailstatic* staticInfo, void* vmissile);
void mistrailDelete(missiletrail* trail);
void mistrailUpdate(missiletrail* trail, vector* position);
void mistrailDraw(vector* current, missiletrail* trail, sdword LOD, sdword teamIndex);

//allocate and initialize a new ship trail
shiptrail *trailNew(trailstatic *staticInfo, void* vship, bool8 second, ubyte trailNum);
void trailDelete(shiptrail *trail);

//update a trail by adding in a new position into its queue
void trailUpdate(shiptrail *trail, vector *position);

//draw a ship trail
void trailDraw(vector *current, shiptrail *trail, sdword LOD, sdword teamIndex);
void trailZeroLength(shiptrail *trail);
void trailMove(shiptrail* trail, vector *delta);

//cause a trail or glow to enter/exit its "wobbly" state
void trailMakeWobbly(void* vship, bool32 state);

// HWI-FEATURE(engine-trails) (M3-R3): the missile corvettes' four jet streams. cataclysm.exe draws one ribbon per mex Eng
//      chunk (InitializeEngineTrails 0x00541f40): two for bMissileCorvette / p1MissileCorvette, each centred between the
//      upper and lower engine circle of one side. On (the default) each of the two is drawn from its side's upper circle
//      and a second ribbon from the lower one, each half the exe's 42-unit span; /exeTrails (run-only) and the console row
//      engineTrails 0 give the exe's two (docs/m3-r3-engine-trails.md)
extern sdword hwiEngineTrails;                                  // HWI-FEATURE(engine-trails): on by default (1); the console row engineTrails
extern bool32 hwiEngineTrailsSwitch;                            // HWI-FEATURE(engine-trails): /exeTrails given
void hwiEngineTrailsSplitCreate(void *vship, sdword numTrails); // HWI-FEATURE(engine-trails): InitializeEngineTrails' added ribbons
void hwiEngineTrailsExeSwitch(void);                            // HWI-FEATURE(engine-trails): /exeTrails
void trailGetNozzleOffset(real32* vec, shiptrail* trail);
void hwiTrailHeadWorld(shiptrail *trail, sdword which, real32 panelZ, vector *world);   // HWI-TEST (M3-R3 engine-trails): /catTrailView
void trailInplaceShipToWorld(real32* vec, real32* coordsys, real32* position);

//parse a ship definition file and create a trailstatic structure
trailstatic *trailStaticInfoParse(char *directory, char *fileName);
void trailStaticDelete(trailstatic *tailInfo);

//recolorize trails
void trailRecolorize(trailstatic *trailStatic);
void trailsRecolorize(void);
void mistrailsRecolorize(void);

#endif
