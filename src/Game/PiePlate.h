/*=============================================================================
    Name    : Pieplate.h
    Purpose : Definitions for the movement pie-plate

    Created 4/9/1998 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___PIEPLATE_H
#define ___PIEPLATE_H

#include "Color.h"
#include "SpaceObj.h"
#include "Types.h"
#include "Vector.h"

/*=============================================================================
    Switches:
=============================================================================*/
#define PIE_MOVE_NEARTO             0           //enables the 'move near to' variant of the move to command

#ifdef HW_BUILD_FOR_DEBUGGING

#define PIE_ERROR_CHECKING          1           //general error checking
#define PIE_VERBOSE_LEVEL           2           //control specific output code
#define PIE_VISUALIZE_VERTICAL      0           //draw the vertical through the specified point
#define PIE_VISUALIZE_EXTENTS       1           //visualize universe extents

#else

#define PIE_ERROR_CHECKING          0           //general error checking
#define PIE_VERBOSE_LEVEL           0           //control specific output code
#define PIE_VISUALIZE_VERTICAL      0           //draw the vertical through the specified point
#define PIE_VISUALIZE_EXTENTS       0           //visualize universe extents

#endif

/*=============================================================================
    Definitions:
=============================================================================*/
//default point spec attributes
#define PIE_PizzaDishRadius         0.75f
#define PIE_PizzaSlices             32

#define PIE_PlanePointFudge         0.00001f

#define PIE_LineStipple             0xff00
#define PIE_StippleSpeed            1.5f

#define PIE_OriginSizeCentre        0.1f
#define PIE_OriginSizeDish          0.08f
#define PIE_OriginSizeHeight        0.1f
#define PIE_ShipLineTickSize        0.03f
#define PIE_ClosestDistance         200.0f
#define PIE_DottedDistance          2
#define PIE_VerticalVectorHeight    100000.0f
#define PIE_MaxMoveHorizontal       15000.0f
#define PIE_MaxMoveVertical         15000.0f
#define PIE_PlaneScreenPointIndex   256
#define PIE_NumberCircleLODs        7

#define PSM_Idle                    0
#define PSM_Waiting                 1
#define PSM_XY                      2
#define PSM_Z                       4

#define PIE_UnprojectDepth          0.99

/*=============================================================================
    Type definitions:
=============================================================================*/
typedef struct
{
    real32 screenRadius;
    sdword nSegments;
}
pieplanecirclesegments;

/*=============================================================================
    Data:
=============================================================================*/
//parameters of the point spec overlays
extern sdword piePointSpecMode;
extern sdword pieOldPointSpecMode;
extern sdword piePointSpecMouseReset;
extern real32 piePointSpecZ;
extern vector piePlanePoint;
extern vector pieHeightPoint;
extern udword pieStipplePattern;
extern uword pieLineStipple;
extern real32 pieLineStippleCounter, pieLineStippleSpeed;
extern color piePointLineColor;
extern color pieOriginColor;
extern void (*pieLineDrawCallback)(real32 distance);
extern void (*pieMovementCursorDrawCallback)(real32 distance);
extern void (*piePlaneDrawCallback)(real32 distance);

#if PIE_VISUALIZE_VERTICAL
extern vector pieScreen0, pieScreen1;
extern bool32 pieOnScreen;
#endif //PIE_VISUALIZE_VERTICAL

#if PIE_VISUALIZE_EXTENTS
extern bool32 pieVisualizeExtents;
#endif


/*=============================================================================
    Functions
=============================================================================*/
//movement mechanism functions
// CAT (M2-R5 hud): piePointModeOnOff 0x004e5c90 takes what the point is for and the function that gets it
//      (cat_piePointSpecExecute 0x004e8500). The callers that only turn it off pass PIE_SPEC_None (0) as the exe;
//      the main region passes the exe's functions since M2-R6 mainrgn-popup (cat_mrMoveCallback, cat_mrWaypointAdd,
//      the big gun's cat_bigGunFirePoint); the HW1 callers left (the sensors manager's) pass PIE_SPEC_Move and no
//      function until their port
#define PIE_SPEC_None               0
#define PIE_SPEC_Move               1
#define PIE_SPEC_MoveNoLines        2
#define PIE_SPEC_BigGun             3
#define PIE_SPEC_Waypoint           4
extern sdword piePointSpecType;
extern bool32 catPieWaypointMode;
void piePointModeOnOff(sdword mode, void (*callback)(vector *point));
void cat_piePointSpecExecute(void);                                     // CAT: 0x004e8500
void cat_pieCompassDraw(vector *centre, color colour, real32 radius);   // CAT: 0x004e7240
color cat_smHeightCircleColorGet(void);                                 // CAT: 0x004e7690
void piePointSpecDraw(void);
void piePointModeToggle(sdword bOn);
void piePointModePause(sdword bPause);
void pieShipDied(Ship *ship);
void pieAllShipsToPiePlateDraw(real32 distance);
void piePlaneDraw(real32 distance);
void pieMovementCursorDraw(real32 distance);
void piePointModeToggle(sdword bOn);

void pieStartup(void);

bool32 pieMovePointClipToLimits(vector *pointA, vector *pointB);        // CAT: 0x004e76b0 (REWRITTEN, the box clip)
sdword pieCircleSegmentsCompute(real32 screenRadius);

#endif
