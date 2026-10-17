/*=============================================================================
    Name    : Pieplate.c
    Purpose : Code for the movement pieplate mechanism.

    Created 4/9/1998 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include <float.h>
#include <math.h>
#include <stdio.h>

#include "Clipper.h"
#include "Color.h"
#include "CommandDefs.h"
#include "FastMath.h"
#include "font.h"
#include "FontReg.h"
#include "glinc.h"
#include "LOD.h"
#include "main.h"
#include "mainrgn.h"
#include "mouse.h"
#include "PiePlate.h"
#include "InfoOverlay.h"
#include "Vector.h"
#include "Mothership.h"
#include "prim2d.h"
#include "prim3d.h"
#include "ProximitySensor.h"
#include "render.h"
#include "Select.h"
#include "Sensors.h"
#include "SoundEvent.h"
#include "SoundEventDefs.h"
#include "SpaceObj.h"
#include "Tutor.h"
#include "Tweak.h"
#include "Undo.h"
#include "Universe.h"

// CAT (M2-R5 hud): the exe's GL -> screen conversion rounds to the nearest pixel (the 0x0072a458 rounding constant
//      added in double precision) where HW1's primGLToScreenX truncated
#define catPieScreenX(x)        (MAIN_WindowWidth / 2 + (sdword)lrint((double)((x) * (real32)MAIN_WindowWidth * 0.5f)))
#define catPieScreenY(y)        (MAIN_WindowHeight / 2 - (sdword)lrint((double)((y) * (real32)MAIN_WindowHeight * 0.5f)))

#if defined _MSC_VER
	#define isnan(x) _isnan(x)
#endif

#ifndef SW_Render
    #ifdef _WIN32
        #include <windows.h>
    #endif
#endif


/*=============================================================================
    Data:
=============================================================================*/
//parameters of the point spec overlays
real32 piePizzaDishRadius = PIE_PizzaDishRadius;
sdword piePizzaSlices = PIE_PizzaSlices;
sdword piePointSpecMode = PSM_Idle;
sdword pieOldPointSpecMode;
sdword piePointSpecMouseReset;
real32 piePointSpecZ;
vector piePlanePoint = { 0.0f, 0.0f, 0.0f };
vector pieHeightPoint = { 0.0f, 0.0f, 0.0f };
real32 pieOriginSizeCentre = PIE_OriginSizeCentre;
real32 pieOriginSizeDish = PIE_OriginSizeDish;
real32 pieOriginSizeHeight = PIE_OriginSizeHeight;
real32 pieShipLineTickSize = PIE_ShipLineTickSize;
real32 pieClosestDistance = PIE_ClosestDistance;
real32 pieDottedDistance = PIE_DottedDistance;
ShipPtr pieLastClosestShip;
void (*pieLineDrawCallback)(real32 distance) = NULL;
void (*pieMovementCursorDrawCallback)(real32 distance) = NULL;
void (*piePlaneDrawCallback)(real32 distance) = NULL;
real32 pieMaxMoveHorizontal = PIE_MaxMoveHorizontal;
real32 pieMaxMoveVertical = PIE_MaxMoveVertical;
real32 pieCircleSizeMax;
real32 pieCircleSizeMin;

real32 pieHeight   = 0;
real32 pieDistance = 0;
vector pieMoveTo   = {0.0f, 0.0f, 0.0f};

static fonthandle pieDistReadFont = 0;

//parameters for dashed lines
udword pieStipplePattern;
uword pieLineStipple = PIE_LineStipple;
real32 pieLineStippleCounter, pieLineStippleSpeed = PIE_StippleSpeed;

//function prototype - located in mainrgn.c
void toDrawPulsedLine(vector linestart, vector lineend, real32 pulsesize, color linecolor, color pulsecolor);

//number of segments to use to render the ship - plane circles at various LOD's
pieplanecirclesegments piePlaneCircleSegments[PIE_NumberCircleLODs];

//heading of the mothership as it last existed
vector pieMotherShipHeading = {0.0f, 1.0f, 0.0f};

color moveLineColor = colWhite;


#if PIE_VISUALIZE_EXTENTS
bool32 pieVisualizeExtents = FALSE;
#endif

vector pieScreen0, pieScreen1;
bool32 pieOnScreen = FALSE;
extern real32 smZoomMax;

/*=============================================================================
    Functions:
=============================================================================*/
/*-----------------------------------------------------------------------------
    Name        : pieStartup
    Description : initialize the pieplate module
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void pieStartup(void)
{
    pieCircleSizeMax = primScreenToGLScaleX(MAIN_WindowWidth / 6.0f);
    pieCircleSizeMin = primScreenToGLScaleX(8);

    piePlaneCircleSegments[0].screenRadius = primScreenToGLScaleX(200);
    piePlaneCircleSegments[0].nSegments = 32;
    piePlaneCircleSegments[1].screenRadius = primScreenToGLScaleX(80);
    piePlaneCircleSegments[1].nSegments = 20;
    piePlaneCircleSegments[2].screenRadius = primScreenToGLScaleX(32);
    piePlaneCircleSegments[2].nSegments = 16;
    piePlaneCircleSegments[3].screenRadius = primScreenToGLScaleX(10);
    piePlaneCircleSegments[3].nSegments = 12;
    piePlaneCircleSegments[4].screenRadius = primScreenToGLScaleX(4);
    piePlaneCircleSegments[4].nSegments = 8;
    piePlaneCircleSegments[5].screenRadius = primScreenToGLScaleX(2);
    piePlaneCircleSegments[5].nSegments = 6;
    piePlaneCircleSegments[6].screenRadius = primScreenToGLScaleX(0);
    piePlaneCircleSegments[6].nSegments = 4;
}

/*-----------------------------------------------------------------------------
    Name        : pieCircleSegmentsCompute
    Description : Compute a suitable number of segments required to draw a circle.
    Inputs      : screenRadius - radius of circle, in GL coordinates
    Outputs     :
    Return      : number of segments
----------------------------------------------------------------------------*/
sdword pieCircleSegmentsCompute(real32 screenRadius)
{
    sdword nSegments, index;

    nSegments = piePlaneCircleSegments[PIE_NumberCircleLODs - 1].nSegments;
    for (index = 0; index < PIE_NumberCircleLODs; index++)
    {
        if (screenRadius > piePlaneCircleSegments[index].screenRadius)
        {
            nSegments = piePlaneCircleSegments[index].nSegments;
            return(nSegments);
        }
    }
    return(piePlaneCircleSegments[0].nSegments);
}

/*-----------------------------------------------------------------------------
    Name        : pieCancelPointSpecMode
    Description : Undo callback to cancel point-spec mode.
    Inputs      :
    Outputs     : Just sets piePointSpecMode to idle
    Return      : void
----------------------------------------------------------------------------*/
bool32 pieCancelPointSpecMode(sdword userID, ubyte *userData, sdword length)
{
    if (piePointSpecMode != PSM_Idle)
    {
        piePointSpecMode = PSM_Idle;
        return TRUE;
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : piePointModeOnOff
    Description : Toggles ship movement mode and associated navigation aids.
    Inputs      : void
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
// CAT (M2-R5 hud): piePointModeOnOff 0x004e5c90's state: what the point is for (PIE_SPEC_*) and the function the
//      exe calls with the chosen point (cat_piePointSpecExecute 0x004e8500)
sdword piePointSpecType = PIE_SPEC_None;            // 0x00a50c94
void (*piePointSpecCallback)(vector *point) = NULL; // 0x00a50cc0
// the line colours per kind (exe data 0x0088d79c / 0x0088d7a0 / 0x0088d7a4, all white)
static color pieMoveLineColour = colWhite, pieBigGunLineColour = colWhite, pieWaypointLineColour = colWhite;
// CAT: 0x00b39cbc, set by cat_mrWaypointsStart 0x00564e20 (waypoint mode of the main region) and cleared by
//      piePointModeOnOff: the plane is drawn in 0x0088d798's colour. Name chosen here
bool32 catPieWaypointMode = FALSE;
static color pieTradePlaneColour = 0xff60c678;     // 0x0088d798

/*-----------------------------------------------------------------------------
    Name        : piePointModeOnOff                                 0x004e5c90
    Description : CAT: Cataclysm's: switch point specification on for a kind of point (1 a move, 2 a move without
                  the lines to the ships, 3 the big gun's target, 4 a waypoint) with the function that takes the
                  chosen point, or off again (any mode while it is on). The exe dropped HW1's undo entry
                  (udLatestThingPush) and the off message.
    Inputs      : mode - PIE_SPEC_*; callback - called with the point by cat_piePointSpecExecute (may be NULL)
----------------------------------------------------------------------------*/
void piePointModeOnOff(sdword mode, void (*callback)(vector *point))
{
    if (piePointSpecMode != PSM_Idle)
    {
        mouseCursorClear(MCM_Pointmode);
        piePointSpecMode = PSM_Idle;
        piePointSpecType = PIE_SPEC_None;
        pieLineDrawCallback = NULL;
        piePlaneDrawCallback = NULL;
        pieMovementCursorDrawCallback = NULL;
        piePointSpecCallback = NULL;
        catPieWaypointMode = FALSE;                // 0x00b39cbc
        MP_HyperSpaceFlag = FALSE;              // 0x00a5b21c
        return;
    }
    if (selSelected.numShips == 0)
    {
        return;
    }
    soundEvent(NULL, UI_MovementGUIon);
    mouseCursorSet(MCM_Pointmode);
    piePointSpecMode = PSM_XY;
    piePointSpecMouseReset = FALSE;
    piePointSpecZ = 0.0f;
    pieLineStippleCounter = 0.0f;
    selCentrePointCompute();
    piePlanePoint = selCentrePoint;
    pieHeightPoint = selCentrePoint;
    piePointSpecCallback = callback;
    piePointSpecType = mode;
    switch (mode)
    {
        case PIE_SPEC_Move:
            tutGameMessage("MoveStart");
            pieLineDrawCallback = pieAllShipsToPiePlateDraw;
            piePlaneDrawCallback = piePlaneDraw;
            pieMovementCursorDrawCallback = pieMovementCursorDraw;
            moveLineColor = pieMoveLineColour;
            break;
        case PIE_SPEC_MoveNoLines:
            pieLineDrawCallback = NULL;
            piePlaneDrawCallback = piePlaneDraw;
            pieMovementCursorDrawCallback = pieMovementCursorDraw;
            moveLineColor = pieMoveLineColour;
            break;
        case PIE_SPEC_BigGun:
            pieLineDrawCallback = pieAllShipsToPiePlateDraw;
            piePlaneDrawCallback = piePlaneDraw;
            pieMovementCursorDrawCallback = pieMovementCursorDraw;
            moveLineColor = pieBigGunLineColour;
            break;
        case PIE_SPEC_Waypoint:
            tutGameMessage("WaypointStart");
            pieLineDrawCallback = pieAllShipsToPiePlateDraw;
            piePlaneDrawCallback = piePlaneDraw;
            // CAT-TODO(M2-R7 waypoint draw): the exe's waypoint cursor 0x004e6e70 (a function the export does not list:
            //      the waypoints so far as lines and circles, with a jitter drawn from ranRandomFn(0) while 0x0088d7a8
            //      is set); the waypoint mode's callers are ported (cat_mrWaypointsStart 0x00564e20)
            pieMovementCursorDrawCallback = pieMovementCursorDraw;
            moveLineColor = pieWaypointLineColour;
            break;
        default:
            dbgFatalf(DBG_Loc, "\npiePointModeOnOff(): Unhandled EPiePointSpecType (%i)", mode);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_piePointSpecExecute                           0x004e8500
    Description : CAT: hand the chosen point (the height point once a height was given, else the plane point) to the
                  point mode's function (not in the tutorial unless its move step is enabled).
----------------------------------------------------------------------------*/
void cat_piePointSpecExecute(void)
{
    vector *point;

    if (tutorial != 1 || tutEnable.bMove)
    {
        point = (piePointSpecZ == 0.0f) ? &piePlanePoint : &pieHeightPoint;
        if (piePointSpecCallback != NULL)
        {
            piePointSpecCallback(point);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : piePointModePause
    Description : Pause/unpause point spec mode.  Overlays will still be drawn,
                    just the origins will not follow the mouse.
    Inputs      : bPause - flag telling whether to pause or unpause point
    Outputs     : Saves current piePointSpecMode to pieOldPointSpecMode
    Return      : void
----------------------------------------------------------------------------*/
// CAT (M2-R5 hud): piePointModePause 0x004e5e70 saves the mode only when it pauses an active one (HW1 saved it on
//      every pause, so a second pause lost the paused mode)
void piePointModePause(sdword bPause)
{
    if (bPause)
    {
        if (piePointSpecMode != PSM_Waiting && piePointSpecMode != PSM_Idle)
        {                                                   //if pause active point spec mode
            pieOldPointSpecMode = piePointSpecMode;           //save current spec mode
            piePointSpecMode = PSM_Waiting;                  //pause the specification
        }
    }
    else if (piePointSpecMode == PSM_Waiting)
    {
        piePointSpecMode = pieOldPointSpecMode;               //restore the point spec mode
        piePointSpecMouseReset = TRUE;                       //reset location of mouse cursor
    }
}

/*-----------------------------------------------------------------------------
    Name        : pieMousePosSet3D
    Description : Sets the on-screen location of the mouse based upon a 3D
                    vector and the current projection/movelview matrices.
    Inputs      : modelView/projection - current matrices
                  mouse3D - 3D location to set the mouse to
    Outputs     :
    Return      : OKAY (CAT: also when the point is off-screen, as the exe)
----------------------------------------------------------------------------*/
sdword pieMousePosSet3D(hmatrix *modelView, hmatrix *projection, vector *mouse3D)
{
    hvector mouseWorld, mouseCamera, mouseScreen;
    sdword mouseViewX, mouseViewY;

    mouseWorld.x = mouse3D->x;                              //make a homo coord out of the mouse coordinate
    mouseWorld.y = mouse3D->y;
    mouseWorld.z = mouse3D->z;
    mouseWorld.w = 1.0f;
    hmatMultiplyHMatByHVec(&mouseCamera, modelView, &mouseWorld);//in camera space
    hmatMultiplyHMatByHVec(&mouseScreen, projection, &mouseCamera);//in screen space
    // CAT: the exe returns 0 (OKAY) in every case (pieMousePosSet3D 0x004e5ec0); no caller reads it
    if (mouseScreen.z <= 0.0f)                              //if point behind camera
    {
        return(OKAY);
    }
    // CAT: the exe rounds to the nearest pixel (pieMousePosSet3D 0x004e5ec0: the 0x0072a458 rounding constant)
    mouseViewX = catPieScreenX(mouseScreen.x / mouseScreen.w);//get location in viewport coords
    mouseViewY = catPieScreenY(mouseScreen.y / mouseScreen.w);

    //dbgMessagef("3d->2d mouse pos: (%.2f, %.2f, %.2f) -> (%d,%d).", mouse3D->x, mouse3D->y, mouse3D->z, mouseViewX, mouseViewY);
    if (mouseViewX < 0 || mouseViewX >= MAIN_WindowWidth || //if offscreen
        mouseViewY < 0 || mouseViewY >= MAIN_WindowHeight)
    {
        return(OKAY);
    }
    mousePositionSet(mouseViewX, mouseViewY);
    return(OKAY);
}

/*-----------------------------------------------------------------------------
    Name        : pieSameOnScreenPoint
    Description : See if two points will be differnt pixels on-screen
    Inputs      : modelView/projection - camera/viewport matrices
                  world0, world1 - worldspace coordinates to compare
    Outputs     :
    Return      : TRUE if both points will be in the same place on-screen
----------------------------------------------------------------------------*/
bool32 pieSameOnScreenPoint(hmatrix *modelView, hmatrix *projection, vector *world0, vector *world1)
{
    sdword x0, x1, y0, y1;
    hvector world, camera, screen;

    world.x = world0->x;
    world.y = world0->y;
    world.z = world0->z;
    world.w = 1.0f;
    hmatMultiplyHMatByHVec(&camera, modelView, &world);     //in camera space
    hmatMultiplyHMatByHVec(&screen, projection, &camera);   //in screen space
    if (screen.z <= 0.0f)
    {                                                       //if behind camera
        return(FALSE);
    }
    // CAT: rounded to the nearest pixel as the exe (pieSameOnScreenPoint 0x004e5fc0)
    x0 = catPieScreenX(screen.x / screen.w);                //get location in viewport coords
    y0 = catPieScreenY(screen.y / screen.w);
    if (x0 < 0 || x0 >= MAIN_WindowWidth ||                 //if offscreen
        y0 < 0 || y0 >= MAIN_WindowHeight)
    {
        return(FALSE);
    }
    world.x = world1->x;
    world.y = world1->y;
    world.z = world1->z;
    world.w = 1.0f;
    hmatMultiplyHMatByHVec(&camera, modelView, &world);     //in camera space
    hmatMultiplyHMatByHVec(&screen, projection, &camera);   //in screen space
    if (screen.z <= 0.0f)
    {                                                       //if behind camera
        return(FALSE);
    }
    x1 = catPieScreenX(screen.x / screen.w);                //get location in viewport coords
    y1 = catPieScreenY(screen.y / screen.w);
    if (x1 < 0 || x1 >= MAIN_WindowWidth ||                 //if offscreen
        y1 < 0 || y1 >= MAIN_WindowHeight)
    {
        return(FALSE);
    }
    return(x0 == x1 && y0 == y1);
}

/*-----------------------------------------------------------------------------
    Name        : pieOriginDraw
    Description : Draw arrows along the axes.
    Inputs      : centre - centre or 'origin' of the origin.
                  size - length from the centre to end of line segments.
                  color - color to draw in
    Outputs     :
    Return      :
    Note        : Currently we will draw just plain lines.  Later, we may want
                    to add arrowheads or use a mesh.
----------------------------------------------------------------------------*/
void pieOriginDraw(vector *centre, real32 size, color c)
{
    vector p0, p1;

    //draw line on z axis
    p0.x = p1.x = centre->x;
    p0.y = p1.y = centre->y;
    p0.z = centre->z - size;
    p1.z = centre->z + size;
    primLine3(&p0, &p1, c);
    //draw line on y axis
    p0.y = centre->y - size;
    p1.y = centre->y + size;
    p0.z = p1.z = centre->z;
    primLine3(&p0, &p1, c);
    //draw line on x axis
    p0.x = centre->x - size;
    p1.x = centre->x + size;
    p0.y = p1.y = centre->y;
    primLine3(&p0, &p1, c);
}


/*-----------------------------------------------------------------------------
    Name        : pieDistanceReadoutDraw
    Description : Draw a numerical representation of the distance from the ship
                  to the movement mechanism pointer
    Inputs      : movepoint - the point where the ship is supposed to go
                  origin - the point where the ship is located
                  c - the color for drawing the text
    Outputs     : ..
    Return      : void
----------------------------------------------------------------------------*/

#define PIE_DIST_READOUT_X_OFFSET 10
#define PIE_DIST_READOUT_Y_OFFSET 10

#define PIE_RU_READOUT_X_OFFSET 10
#define PIE_RU_READOUT_Y_OFFSET 2

void pieDistanceReadoutDraw(vector *movepoint, vector *origin, color c)
{
    real32 dist_x, dist_y, dist_z;
    real32 screen_x, screen_y, dummy_r;
    udword distance;
    udword resources;
    real32 cost;
    fonthandle fhSave;
    color ruReadoutColour = colBlack;
    hmatrix modelview, projection;
    bool32 depthOn;
    char dist_str[20],ru_str[20];

    dist_x = movepoint->x - origin->x;
    dist_y = movepoint->y - origin->y;
    dist_z = movepoint->z - origin->z;

    //update global variable
    pieHeight = dist_z;

    dist_x *= dist_x;
    dist_y *= dist_y;
    dist_z *= dist_z;

    //find la distance (using that wonderful formula by Pythagoras)
    pieDistance = fsqrt(dist_x + dist_y + dist_z);
    distance = (udword)pieDistance;

    //update the global moveto vector and distance variables
    pieMoveTo   = *movepoint;

    if(MP_HyperSpaceFlag)
    {
        //multiplayer...
        cost = hyperspaceCost(pieDistance,(SelectCommand *)&selSelected);
        resources = (udword) cost;
        sprintf(ru_str, "%i RU's", resources);

        if(universe.curPlayerPtr->resourceUnits < resources)
        {
            ruReadoutColour = TW_RU_READOUT_COLOR_BAD;
        }
        else
        {
            ruReadoutColour =  TW_RU_READOUT_COLOR_GOOD;
        }

    }

    //depending on the distance, scale the readout
    // CAT: aiming the big gun shows its damage at that distance instead (pieDistanceReadoutDraw 0x004e61b0)
    if (piePointSpecType == PIE_SPEC_BigGun)
    {
        sprintf(dist_str, "%i %%", (sdword)(cat_bigGunDamageFalloff((real32)distance) * 100.0f));
    }
    else if (distance < 100)
    {
        sprintf(dist_str, "%i m", distance);
    }
    else if ((distance >= 100) && (distance < 10000))
    {
        distance /= 100;
        distance *= 100;
        sprintf(dist_str, "%i m", distance);
    }
    else if (distance >= 10000)
    {
        distance /= 1000;
        sprintf(dist_str, "%i km", distance);
    }

    //find the screen coordinates of the moveto pointer
    glGetFloatv(GL_MODELVIEW_MATRIX, (GLfloat *)(&modelview));
    glGetFloatv(GL_PROJECTION_MATRIX, (GLfloat *)(&projection));
    selCircleComputeGeneral(&modelview, &projection, movepoint, 0, &screen_x, &screen_y, &dummy_r);

    fhSave = fontCurrentGet();
    fontMakeCurrent(pieDistReadFont);

    //and here I set up my own 2D space
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    depthOn = (bool32)glIsEnabled(GL_DEPTH_TEST);
    if (depthOn) glDisable(GL_DEPTH_TEST);

    //using the mousecursor position to position the distance readout is more
    //stable, but can't be used when the camera is being moved, so the moveto
    //position is projected onto the screen in that case
    // CAT: the exe tests only the main region's right button (0x00a75288), and in the moving-camera case places
    //      the text from the projected point's screen x for both axes (as written)
    if (mrHoldRight == mrNULL)
    {
        fontPrint((mouseCursorXPosition + PIE_DIST_READOUT_X_OFFSET),
                  (mouseCursorYPosition + PIE_DIST_READOUT_Y_OFFSET),
                  TW_DISTANCE_READOUT_COLOR, dist_str);
        if(MP_HyperSpaceFlag)
        {
            fontPrint((mouseCursorXPosition + PIE_RU_READOUT_X_OFFSET),
                  (mouseCursorYPosition + PIE_DIST_READOUT_Y_OFFSET+PIE_RU_READOUT_Y_OFFSET+fontHeight("K")),
                  ruReadoutColour, ru_str);

        }
    }
    else
    {
        sdword screenX = MAIN_WindowWidth / 2 + (sdword)lrint((double)((real32)MAIN_WindowWidth * screen_x * 0.5f));
        sdword screenY = MAIN_WindowHeight / 2 - (sdword)lrint((double)((real32)MAIN_WindowWidth * screen_x * 0.5f));
        (void)screen_y;
        fontPrint(screenX + PIE_DIST_READOUT_X_OFFSET, screenY + PIE_DIST_READOUT_Y_OFFSET,
                  TW_DISTANCE_READOUT_COLOR, dist_str);
        if(MP_HyperSpaceFlag)
        {
            fontPrint(screenX + PIE_RU_READOUT_X_OFFSET,
                  screenY + PIE_DIST_READOUT_Y_OFFSET+PIE_RU_READOUT_Y_OFFSET+fontHeight("K"),
                  ruReadoutColour, ru_str);

        }
    }

    if (depthOn) glEnable(GL_DEPTH_TEST);

    //back to previous space
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    fontMakeCurrent(fhSave);
}
#undef PIE_DIST_READOUT_X_OFFSET
#undef PIE_DIST_READOUT_Y_OFFSET

//list of points to draw for the dots where the vertical lines meet the pie plane
struct
{
    real32 x, y;
}
piePlaneScreenPoint[PIE_PlaneScreenPointIndex];
sdword piePlaneScreenPointIndex;
/*-----------------------------------------------------------------------------
    Name        : pieAllShipsToPiePlateDraw
    Description : Draw a line from all ships to the point-spec pie plate
    Inputs      : distance - adjusted distance from camera to ship
    Outputs     : ..
    Return      : void
----------------------------------------------------------------------------*/
void pieAllShipsToPiePlateDraw(real32 distance)
{
    Node *objnode;
    Ship *ship;
    vector planePoint, shipPoint;//, innerPoint;
    real32 length;
    real32 closestDistance = REALlyBig;
    bool32 bHeightPointDrawn   = FALSE,
         bClosestHeightDrawn = FALSE;
    ShipPtr closestShip = NULL;
    color c;
    real32 screenX, screenY, screenRadius, radius;
    sdword nSegments;

    objnode = universe.RenderList.head;                    //get first node in list

    distance *= pieShipLineTickSize;                         //get distance-adjusted size

    piePlaneScreenPointIndex = 0;
    while (objnode != NULL)
    {                                                       //for every ship
        ship = (Ship *)listGetStructOfNode(objnode);
        if (ship->header.flags & SOF_Dead)
        {
            goto nextnode;
        }

        // CAT: no Drone skip, Cataclysm has no drones (M2-R4 ships-b retire)
        if (ship->header.objtype != OBJ_ShipType)
        {
            goto nextnode;
        }

        if (bitTest(ship->header.flags,SOF_Cloaked) && ship->playerowner != universe.curPlayerPtr)
        {                                            //if ship is cloaked, don't draw this stuff
            if(!proximityCanPlayerSeeShip(universe.curPlayerPtr, ship))
            {
                goto nextnode;
            }
        }

        shipPoint.x = planePoint.x = ship->obj.posinfo.position.x;
        shipPoint.y = planePoint.y = ship->obj.posinfo.position.y;
        shipPoint.z = ship->obj.posinfo.position.z;
        planePoint.z = selCentrePoint.z;

        if (ship == pieLastClosestShip)
        {                                                   //if closest ship from last frame
            c = TW_SHIP_LINE_CLOSEST_COLOR;
        }
        else
        {
            c = TW_SHIP_LINE_COLOR;
        }
        primLine3(&planePoint, &shipPoint, c);              //draw line from plane to ship
        length = fsqrt(((shipPoint.x - selCentrePoint.x) * (shipPoint.x - selCentrePoint.x) +
                 (shipPoint.y - selCentrePoint.y) * (shipPoint.y - selCentrePoint.y)));
        //now draw circle on the plane
        bHeightPointDrawn = FALSE;
        if (ABS(shipPoint.z - planePoint.z) > ship->staticinfo->common.staticCollInfo.collspheresize)
        {
            radius = ship->staticinfo->common.staticCollInfo.collspheresize;
            selCircleComputeGeneral(&rndCameraMatrix, &rndProjectionMatrix,
                                &planePoint, radius, &screenX, &screenY, &screenRadius);
            if (screenRadius > 0.0f)
            {
//                dbgAssertOrIgnore(piePlaneScreenPointIndex < PIE_PlaneScreenPointIndex);
                if (piePlaneScreenPointIndex < PIE_PlaneScreenPointIndex)
                {
                    piePlaneScreenPoint[piePlaneScreenPointIndex].x = screenX;
                    piePlaneScreenPoint[piePlaneScreenPointIndex].y = screenY;
                    piePlaneScreenPointIndex++;
                    nSegments = pieCircleSegmentsCompute(screenRadius);
                    primCircleOutlineZ(&planePoint, radius,
                                       nSegments, c);
                    bHeightPointDrawn = TRUE;
                }
            }
        }

        //see if this is the closest ship to the cursor
        length = ABS(piePlanePoint.x - planePoint.x) +       //manhattan distance
            ABS(piePlanePoint.y - planePoint.y);
        if (length < closestDistance)
        {                                                   //if this ship closest so far
            closestDistance = length;
            closestShip = ship;
            bClosestHeightDrawn = bHeightPointDrawn;
        }

nextnode:
        objnode = objnode->next;
    }

    if (closestShip != NULL)
    {
        if (closestDistance > max(distance, pieClosestDistance))
        {                                                       //if the closest ship wasn't close enough
            closestShip = NULL;                                 //there was no closest ship
        }
    }

    /* play the movement gui click sound */
    if ((closestShip != NULL) && (pieLastClosestShip != closestShip) && !selShipInSelection(selSelected.ShipPtr, selSelected.numShips, closestShip) && bClosestHeightDrawn)
    {
        soundEvent(NULL, UI_MoveChimes);
    }

    pieLastClosestShip = closestShip;
}

/*-----------------------------------------------------------------------------
    Name        : pieWorldLocationCompute
    Description : Compute the location of the mouse in world coordinates by
                    'unprojecting' them.
    Outputs     : fills in world0 and world1 with 3d points describing a line
                    under the mouse.
                  eyeMag - approximate magnitude of the camera's eye position
    Return      : void
----------------------------------------------------------------------------*/
void pieWorldLocationCompute(vector *world0, vector *world1, real32 *eyeMag)
{
    world0->x = mrCamera->eyeposition.x;
    world0->y = mrCamera->eyeposition.y;
    world0->z = mrCamera->eyeposition.z;

    *eyeMag = fsqrt(vecMagnitudeSquared(*world0));

    cameraRayCast(world1, mrCamera, mouseCursorX(), mouseCursorY(), MAIN_WindowWidth, MAIN_WindowHeight);
    vecMultiplyByScalar(*world1, *eyeMag);
    vecAdd(*world1, *world1, *world0);
}

/*-----------------------------------------------------------------------------
    Name        : pieScreenSizeOfCircleCompute
    Description : Compute the size of the movement circle, clipping it to a
                    minimum and maximum in screen space.
    Inputs      : position - where on screen it is to be rendered
                  radius - radius, world space of the thingus
    Outputs     : outRadius - new world radius, possible adjusted
                  mSegments - number of segments to use when drawing the circle.
    Return      :
----------------------------------------------------------------------------*/
void pieScreenSizeOfCircleCompute(vector *position, real32 inRadius, real32 *outRadius, sdword *nSegments)
{
    real32 screenX, screenY, screenRadius;
    real32 factor;

    *outRadius = inRadius;
    //figure out what size it is now
    selCircleComputeGeneral(&rndCameraMatrix, &rndProjectionMatrix, position, inRadius, &screenX, &screenY, &screenRadius);

    if (screenRadius > pieCircleSizeMax)
    {                                                       //if it is currently too large
        factor = pieCircleSizeMax / screenRadius;
        *outRadius *= factor;
        screenRadius *= factor;
    }
    if (screenRadius < pieCircleSizeMin)
    {                                                       //if currently too small
        factor = pieCircleSizeMin / screenRadius;
        *outRadius *= factor;
        screenRadius *= factor;
    }
    *nSegments = pieCircleSegmentsCompute(screenRadius);
}

/*-----------------------------------------------------------------------------
    Name        : pieNeedSpecialAttackAndMoveColor
    Description : returns true if all ships in selection have an attack order
                  and are going to perform an attack and move.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 pieNeedSpecialAttackAndMoveColor(void)
{
    sdword i,j;
    CommandToDo *command;

    j=0;
    for(i=0;i<selSelected.numShips;i++)
    {
        command = getShipAndItsCommand(&universe.mainCommandLayer,selSelected.ShipPtr[i]);
        if(command == NULL)
            return FALSE;
        if(command->ordertype.order != COMMAND_ATTACK)
            return FALSE;

        if(selSelected.ShipPtr[i]->staticinfo->shipclass == CLASS_Fighter ||
           selSelected.ShipPtr[i]->staticinfo->shipclass == CLASS_Corvette ||
           selSelected.ShipPtr[i]->staticinfo->cantMoveAndAttack)
            j++;
    }
    if(j==selSelected.numShips)
        return FALSE;           //all strike craft...don't do special move attack!
    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : pieMovementCursorDraw
    Description : Default movement mechanism for rendering the dotted lines and
                    destination cursors of the movement mechanism.
    Inputs      : distance - size multiplier for no good reason?
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
// CAT (M2-R5 hud): pieMovementCursorDraw 0x004e6c50 is HW1's with its dotted line (GoK had dropped the stipple:
//      glCapFastFeature(GL_LINE_STIPPLE) is TRUE on a GL device, so the stipple is always used here), and with the
//      sensors manager open the lines are drawn over everything (the depth test off while it was on)
void pieMovementCursorDraw(real32 distance)
{
    real32 scaledSize;
    sdword nSegments;
    bool32 depthTest = FALSE;

    if (smSensorsActive && glIsEnabled(GL_DEPTH_TEST))
    {
        depthTest = TRUE;
        glDisable(GL_DEPTH_TEST);
    }
    if (ABS(piePointSpecZ) < pieDottedDistance)
    {
        glEnable(GL_LINE_STIPPLE);
        glLineStipple(1, (GLushort)pieStipplePattern);
        primLine3(&piePlanePoint, &selCentrePoint, moveLineColor);//draw line from centre to mouse point on x/y plane
        glDisable(GL_LINE_STIPPLE);
    }
    else
    {                                                       //if point off standard Z-plane
        glEnable(GL_LINE_STIPPLE);
        glLineStipple(1, (GLushort)pieStipplePattern);
        primLine3(&pieHeightPoint, &selCentrePoint, moveLineColor);//draw from centre of dish to height point
        glDisable(GL_LINE_STIPPLE);
        primLine3(&pieHeightPoint, &piePlanePoint, moveLineColor); //TW_MOVE_PIZZA_COLOR tweakable global variable (tweak.*)
        primLine3(&piePlanePoint, &selCentrePoint, moveLineColor);//draw line from centre to mouse point on x/y plane
        //destination circle
        pieScreenSizeOfCircleCompute(&pieHeightPoint, selAverageSize, &scaledSize, &nSegments);
        primCircleOutlineZ(&pieHeightPoint, scaledSize, nSegments, moveLineColor);
    }
    if (smSensorsActive && depthTest)
    {
        glEnable(GL_DEPTH_TEST);
    }
}

/*-----------------------------------------------------------------------------
    Name        : pieDestinationInGunRangeOfTargets
    Description : returns TRUE if all attacking ships will
                  be within gun range of at least 1 of their targets at the new
                  position
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 pieDestinationInGunRangeOfTargets(void)
{
    sdword i,j;
    vector distvec;
    real32 distSqr;
    sdword breaker;
    CommandToDo *command;

    for(i=0;i<selSelected.numShips;i++)
    {
        if(selSelected.ShipPtr[i]->shiptype == CloakGenerator ||
           selSelected.ShipPtr[i]->shiptype == GravWellGenerator ||
           selSelected.ShipPtr[i]->shiptype == DFGFrigate)
            continue;
        command = getShipAndItsCommand(&universe.mainCommandLayer,selSelected.ShipPtr[i]);

        dbgAssertOrIgnore(command != NULL);
        dbgAssertOrIgnore(command->ordertype.order == COMMAND_ATTACK);

        breaker = FALSE;
        for(j = 0; j<command->attack->numTargets;j++)
        {
            vecSub(distvec,pieHeightPoint,command->attack->TargetPtr[j]->obj.posinfo.position);
            distSqr = vecMagnitudeSquared(distvec);
            if(distSqr < selSelected.ShipPtr[i]->staticinfo->bulletRangeSquared[selSelected.ShipPtr[i]->tacticstype])
            {
                //within range...break and check next ship
                breaker = TRUE;
                break;
            }
        }
        if(!breaker)
            return FALSE;   //ship will not be within range
    }
    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : piePlaneDraw
    Description : Default draw callback for rendering the movement mechanism
                    pizza dish.
    Inputs      : distance - radius of the movement mechanism
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void piePlaneDraw(real32 distance)
{
    real32 scaledSize;
    sdword nSegments;
    bool32 depthWasOn = FALSE;
    sdword texture;
    color pieColor;

    // CAT: piePlaneDraw 0x004e7520 (a gap of the export): the compass (cat_pieCompassDraw) in the trader move's
    //      colour or TW_MOVE_POINT_LINE_COLOR, then the plane point's circle in the same colour; no dish, spokes or the
    //      attack-move colours; in the sensors manager without the depth test. The exe pushes GL_LIGHTING (0xb50) as
    //      an attribute mask, as written
    if (smSensorsActive && glIsEnabled(GL_DEPTH_TEST))
    {
        depthWasOn = TRUE;
        glDisable(GL_DEPTH_TEST);
    }
    glPushAttrib(GL_LIGHTING);
    glDisable(GL_LIGHTING);
    texture = rndTextureEnable(FALSE);
    pieColor = catPieWaypointMode ? pieTradePlaneColour : TW_MOVE_POINT_LINE_COLOR;
    cat_pieCompassDraw(&selCentrePoint, pieColor, distance);
    pieScreenSizeOfCircleCompute(&piePlanePoint, selAverageSize, &scaledSize, &nSegments);
    primCircleOutlineZ(&piePlanePoint, scaledSize, nSegments, pieColor);
    glPopAttrib();
    if (smSensorsActive && depthWasOn)
    {
        glEnable(GL_DEPTH_TEST);
    }
    rndTextureEnable(texture);
}

/*-----------------------------------------------------------------------------
    Name        : cat_pieCompassDraw                                0x004e7240
    Description : CAT: the movement compass around the selection: a circle of the radius, a tick every 15 degrees but
                  at the four main directions (0.9 to 1 radius), four spokes, and across each spoke a tick every 700
                  units out to 350 short of the radius (300 long each side). In the sensors manager without the depth
                  test. Nothing for the colour 0xff000000 (opaque black: cat_smZoomFadeColor's fully faded colour; the exe
                  compares the dword, cmp ebx,0xff000000 at 0x004e724b) [audit: was 0xff7fffff].
    Inputs      : centre, colour, radius
----------------------------------------------------------------------------*/
void cat_pieCompassDraw(vector *centre, color colour, real32 radius)
{
    static const real32 spokeX[4] = { 0.0f, 1.0f, 0.0f, -1.0f };       // 0x0088d7ac
    static const real32 spokeY[4] = { 1.0f, 0.0f, -1.0f, 0.0f };       // 0x0088d7bc
    vector a, b;
    real32 angle, sine, cosine, along, limit;
    bool32 depthWasOn = FALSE;
    sdword index;

    if (colour == 0xff000000)
    {
        return;
    }
    catHudCounts.compassDraws++;                            // CAT: port diagnostic (the HUD self-test)
    if (smSensorsActive && glIsEnabled(GL_DEPTH_TEST))
    {
        depthWasOn = TRUE;
        glDisable(GL_DEPTH_TEST);
    }
    rndTextureEnable(FALSE);
    primCircleOutlineZ(centre, radius, 0x30, colour);
    for (angle = 0.2617994f; angle < 6.2831855f; angle += 0.2617994f)
    {
        sine = (real32)sin(angle);
        cosine = (real32)cos(angle);
        if ((sine + 1e-5f < 0.0f || 0.0f < sine - 1e-5f) && (cosine + 1e-5f < 0.0f || 0.0f < cosine - 1e-5f))
        {
            a.x = sine * radius * 0.9f + centre->x;
            a.y = cosine * radius * 0.9f + centre->y;
            a.z = centre->z;
            b.x = sine * radius + centre->x;
            b.y = cosine * radius + centre->y;
            b.z = a.z;
            primLine3(&a, &b, colour);
        }
    }
    limit = radius - 350.0f;
    for (index = 0; index < 4; index++)
    {
        a.x = spokeX[index] * radius + centre->x;
        a.y = spokeY[index] * radius + centre->y;
        a.z = centre->z;
        primLine3(centre, &a, colour);
        if (700.0f < limit)
        {
            for (along = 700.0f; along < limit; along += 700.0f)
            {
                a.x = along * spokeY[index] + spokeX[index] * 300.0f + centre->x;
                a.y = along * spokeX[index] + spokeY[index] * 300.0f + centre->y;
                a.z = centre->z;
                b.x = along * spokeY[index] - spokeX[index] * 300.0f + centre->x;
                b.y = along * spokeX[index] - spokeY[index] * 300.0f + centre->y;
                b.z = a.z;
                primLine3(&a, &b, colour);
            }
        }
    }
    if (smSensorsActive && depthWasOn)
    {
        glEnable(GL_DEPTH_TEST);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_smHeightCircleColorGet                        0x004e7690
    Description : CAT: the height circle's colour (0x00892c94) faded with the sensors manager's zoom.
----------------------------------------------------------------------------*/
color cat_smHeightCircleColorGet(void)
{
    return cat_smZoomFadeColor(0xffb8b838, 0);
}

/*-----------------------------------------------------------------------------
    Name        : pieMovePointClipToLimits                          0x004e76b0
    Description : CAT: Cataclysm's (REWRITTEN): the limits are the box +-(smUniverseSize - 8000) on each axis (HW1
                  clipped to an elliptical cylinder of the given half-lengths). With pointA outside the box pointB is
                  clamped into it. A pointB outside is moved to where the segment pointA -> pointB leaves the box in x
                  or y (the nearer one), z clamped; if that point is still outside, pointB becomes pointA.
    Inputs      : pointA - where you are moving from
                  pointB - where you are moving to, this is the point that will be clipped
    Outputs     : modifies pointB
    Return      : TRUE if pointB ends inside (or was clipped into) the box, FALSE if it was set to pointA
----------------------------------------------------------------------------*/
#define PIE_LimitMargin     8000.0f                 // 0x0072a63c
#define pieClamp(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))
bool32 pieMovePointClipToLimits(vector *pointA, vector *pointB)
{
    real32 limX = smUniverseSizeX - PIE_LimitMargin;
    real32 limY = smUniverseSizeY - PIE_LimitMargin;
    real32 limZ = smUniverseSizeZ - PIE_LimitMargin;
    real32 tx, ty, dx, x, y, z;

    if (pointA->x < -limX || limX < pointA->x || pointA->y < -limY || limY < pointA->y ||
        pointA->z < -limZ || limZ < pointA->z)
    {
        pointB->x = pieClamp(pointB->x, -limX, limX);
        pointB->y = pieClamp(pointB->y, -limY, limY);
        pointB->z = pieClamp(pointB->z, -limZ, limZ);
    }
    if (-limX <= pointB->x && pointB->x <= limX && -limY <= pointB->y && pointB->y <= limY &&
        -limZ <= pointB->z && pointB->z <= limZ)
    {
        return TRUE;
    }
    dx = pointB->x - pointA->x;
    tx = 1.0f;
    x = pointB->x;
    if (pointB->x < -limX)
    {
        tx = (-limX - pointA->x) / dx;
        x = -limX;
    }
    else if (limX < pointB->x)
    {
        tx = (limX - pointA->x) / dx;
        x = limX;
    }
    ty = 1.0f;
    y = pointB->y;
    if (pointB->y < -limY)
    {
        ty = (-limY - pointA->y) / (pointB->y - pointA->y);
        y = -limY;
    }
    else if (limY < pointB->y)
    {
        ty = (limY - pointA->y) / (pointB->y - pointA->y);
        y = limY;
    }
    if (tx != 1.0f || ty != 1.0f)
    {
        if (ty <= tx)
        {
            x = ty * dx + pointA->x;
        }
        else
        {
            y = tx * (pointB->y - pointA->y) + pointA->y;
        }
    }
    z = pieClamp(pointB->z, -limZ, limZ);
    if (-limX <= x && x <= limX && -limY <= y && y <= limY && -limZ <= z && z <= limZ)
    {
        pointB->x = x;
        pointB->y = y;
        pointB->z = z;
        return TRUE;
    }
    *pointB = *pointA;
    return FALSE;
}
#undef pieClamp
#undef SGN

/*-----------------------------------------------------------------------------
    Name        : piePointSpecDraw
    Description : Draw the point specification overlays
    Inputs      : void
    Outputs     : ..
    Return      : void
----------------------------------------------------------------------------*/
void piePointSpecDraw(void)
{
    vector world0, world1;
    vector highPoint, lowPoint;
    GLfloat modelView[16], projection[16];
    hmatrix modelViewF, projectionF;
    real32 distance, deltaX, deltaY;
    real32 oldPointSpecZ, slope, b;
    vector moveLengthVector;
    vector preUpdateCentre;
    vector updateDrift;
    real32 moveLength, maxMoveLength;
    real32 windowHeightMinusOne;
    real32 eyeMagnitude;
    bool32 depthWasOn = FALSE;

    catHudCounts.pieDraws++;                                // CAT: port diagnostic (the HUD self-test)
    mousePoll();

    if (pieDistReadFont == 0)
    {
        pieDistReadFont = frFontRegister(TW_DISTANCE_READOUT_FONT);
    }

    if (selSelected.numShips == 0)
    {
        piePointModeOnOff(PIE_SPEC_None, NULL);
        return;
    }
    //before recomputing the selection centre point, get the relationship between selection centre, plane point and heightpoint
    preUpdateCentre = selCentrePoint;
    selCentrePointCompute();                                //get new centre of selection
    vecSub(updateDrift, selCentrePoint, preUpdateCentre);   //compute drift of centre point for error correction

    if (piePointSpecMouseReset)
    {                                                       //if we should reset location of mouse
        glGetFloatv(GL_PROJECTION_MATRIX,                   //get the matrices in hmatrix format
                    (float *)(&projectionF));
        glGetFloatv(GL_MODELVIEW_MATRIX,
                    (float *)(&modelViewF));
        if (piePointSpecMode == PSM_XY)
        {
            vecAdd(world0, piePlanePoint, updateDrift);
            vecAdd(world1, pieHeightPoint, updateDrift);
            pieMousePosSet3D(&modelViewF, &projectionF,     //set the position of the mouse
                                    &world0);
            if (pieSameOnScreenPoint(&modelViewF, &projectionF, &world0, &world1))
            {                                               //if they're still at the same spot on-screen
                pieHeightPoint = piePlanePoint;             //kill any vertical component
                piePointSpecZ = 0.0f;
            }
        }
        else
        {
            vecAdd(world0, pieHeightPoint, updateDrift);
            pieMousePosSet3D(&modelViewF, &projectionF,     //set the postion of the mouse
                                    &world0);
        }
        piePointSpecMouseReset = FALSE;                      //don't reset on next frame
    }

    windowHeightMinusOne = (real32)(MAIN_WindowHeight - 1);
    
    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    glGetFloatv(GL_MODELVIEW_MATRIX, modelView);           //get the matrices

    world0.x = mrCamera->eyeposition.x - selCentrePoint.x;
    world0.y = mrCamera->eyeposition.y - selCentrePoint.y;
    world0.z = mrCamera->eyeposition.z - selCentrePoint.z;
    distance = fmathSqrt(vecMagnitudeSquared(world0));
    //prepare for drawing
    primModeClear2();
    rndLightingEnable(FALSE);                               //draw the pizza dish
    rndTextureEnable(FALSE);                                //disable texturing

    //compute world coords of mouse location
    if (piePointSpecMode != PSM_Waiting)
    {
        //compute mouse location in world space
        pieWorldLocationCompute(&world0, &world1, &eyeMagnitude);

        if (piePointSpecMode == PSM_XY)
        {                                                   //find point on pizza dish plane which represents mouse
            //ensure positive slope of intersection line
            if (mrCamera->eyeposition.z >= selCentrePoint.z || world1.z == world0.z)
            {                                               //if camera above selection plane
                if (world1.z >= world0.z)
                {
                    world1.z = world0.z - PIE_PlanePointFudge * eyeMagnitude;
                }
            }
            else
            {                                               //else camera below selection sphere
                if (world1.z <= world0.z)
                {
                    world1.z = world0.z + PIE_PlanePointFudge * eyeMagnitude;
                }
            }
            vecLineIntersectWithXYPlane(&piePlanePoint, &world0,
                                        &world1, selCentrePoint.z);
            pieHeightPoint.x = piePlanePoint.x;
            pieHeightPoint.y = piePlanePoint.y;
            pieHeightPoint.z += updateDrift.z;
        }
        else
        {                                                   //else find point on vertical line passing through point on pizza dish (PSM_Z)
            if (pieOnScreen)
            {                                               //ensure the vertical is at least partly on screen
                if ((real32)mouseCursorY() < (windowHeightMinusOne - pieScreen1.y))
                {                                               //if mouse off top of line
                    mousePositionSet(mouseCursorX(), (sdword)(windowHeightMinusOne - pieScreen1.y));
                }
                if ((real32)mouseCursorY() > (windowHeightMinusOne - pieScreen0.y))
                {                                               //if mouse off bottom of line
                    mousePositionSet(mouseCursorX(), (sdword)(windowHeightMinusOne - pieScreen0.y));
                }
                slope = (pieScreen0.x - pieScreen1.x) / (pieScreen1.y - pieScreen0.y);
                b = pieScreen1.x - slope * (windowHeightMinusOne - pieScreen1.y);
                mousePositionSet((sdword)(slope * mouseCursorY() + b), mouseCursorY());

                //mouse position was adjusted; recompute mouse location in world space
                pieWorldLocationCompute(&world0, &world1, &eyeMagnitude);
                /*
                if (oldMouseX != mouseCursorX() || oldMouseY != mouseCursorY())
                {
                    dbgMessagef("slope = %.2f b = %.2f", slope, b);
                    dbgMessagef("screen0=(%.2f,%.2f) screen1=(%.2f,%.2f).", pieScreen0.x, pieScreen0.y, pieScreen1.x, pieScreen1.y);
                    dbgMessagef("world0=(%.2f,%.2f,%.2f) world1=(%.2f,%.2f,%.2f).", world0.x, world0.y, world0.z, world1.x, world1.y, world1.z);
                    oldMouseX = mouseCursorX();
                    oldMouseY = mouseCursorY();
                }
                */
            }

            deltaX = mrCamera->eyeposition.x - (piePlanePoint.x + updateDrift.x);//find along which axis point is farther from camera
            deltaY = mrCamera->eyeposition.y - (piePlanePoint.y + updateDrift.y);
            if (deltaX > deltaY)
            {
                //ensure positive slope of intersection line
                if (mrCamera->eyeposition.x >= piePlanePoint.x + updateDrift.x || world1.x == world0.x)
                {                                           //camera on positive side of test plane
                    if (world1.x >= world0.x)
                    {
                        world1.x = world0.x - PIE_PlanePointFudge * eyeMagnitude;
                    }
                }
                else
                {
                    if (world1.x <= world0.x)
                    {
                        world1.x = world0.x + PIE_PlanePointFudge * eyeMagnitude;
                    }
                }
                vecLineIntersectWithYZPlane(&pieHeightPoint, &world0,
                                            &world1, piePlanePoint.x + updateDrift.x);
                //dbgMessagef("\\");
            }
            else
            {
                //ensure positive slope of intersection line
                if (mrCamera->eyeposition.y >= piePlanePoint.y + updateDrift.y || world1.y == world0.y)
                {                                           //camera on positive side of test plane
                    if (world1.y >= world0.y)
                    {
                        world1.y = world0.y - PIE_PlanePointFudge * eyeMagnitude;
                    }
                }
                else
                {
                    if (world1.y <= world0.y)
                    {
                        world1.y = world0.y + PIE_PlanePointFudge * eyeMagnitude;
                    }
                }
                vecLineIntersectWithXZPlane(&pieHeightPoint, &world0,
                                            &world1, piePlanePoint.y + updateDrift.y);
                //dbgMessagef("/");
            }
            /*
            if (oldMouseX != mouseCursorX() || oldMouseY != mouseCursorY())
            {
                dbgMessagef("deltaX = %.2f deltaY = %.2f", deltaX, deltaY);
                dbgMessagef("heightPoint=(%.2f,%.2f,%.2f).", pieHeightPoint.x, pieHeightPoint.y, pieHeightPoint.z);
                dbgMessagef("world0=(%.2f,%.2f,%.2f) world1=(%.2f,%.2f,%.2f).", world0.x, world0.y, world0.z, world1.x, world1.y, world1.z);
                oldMouseX = mouseCursorX();
                oldMouseY = mouseCursorY();
            }
            */
            //don't use this new value if the intersection test resulted in a bad number
            if (!isnan(pieHeightPoint.z))
            {
                //compute point along this line
                pieHeightPoint.x = piePlanePoint.x;         //compute height point relative to pizza dish
                pieHeightPoint.y = piePlanePoint.y;
                oldPointSpecZ = piePointSpecZ;
                piePointSpecZ = pieHeightPoint.z - piePlanePoint.z;//get height off pizza dish
            }
            vecAddTo(piePlanePoint, updateDrift);           //make sure the plane point does not drift
            //vecAddTo(pieHeightPoint, updateDrift);          //make sure the height point does not drift
        }
    }
    else
    {                                                       //in waiting mode: keep destination point from drifting
        vecAddTo(piePlanePoint, updateDrift);               //make sure the plane point does not drift
        vecAddTo(pieHeightPoint, updateDrift);              //make sure the height point does not drift
    }

    //recompute the on-screen vertical vector
    pieHeightPoint.x = piePlanePoint.x;                     //compute height point relative to pizza dish point
    pieHeightPoint.y = piePlanePoint.y;
    highPoint.x = lowPoint.x = piePlanePoint.x;
    highPoint.y = lowPoint.y = piePlanePoint.y;
    highPoint.z = smUniverseSizeZ;
    lowPoint.z = -smUniverseSizeZ;
    pieOnScreen = clipLineToScreen(&lowPoint, &highPoint, (real32 *)(&rndCameraMatrix),
                     (real32 *)(&rndProjectionMatrix), &pieScreen0, &pieScreen1);
    if (pieScreen0.y > pieScreen1.y)
    {                                               //if upside-down
        swapReal32(pieScreen0.x, pieScreen1.x);     //transpose the endpoints
        swapReal32(pieScreen0.y, pieScreen1.y);
    }

    // CAT: aiming the big gun keeps the point within its range (piePointSpecDraw 0x004e7a70)
    if (piePointSpecType == PIE_SPEC_BigGun && cat_vecDistance(&pieHeightPoint, &selCentrePoint) > cat_bigGunMaxRange())
    {
        vector dir;
        vecSub(dir, pieHeightPoint, selCentrePoint);
        vecNormalize(&dir);
        pieHeightPoint.x = cat_bigGunMaxRange() * dir.x + selCentrePoint.x;
        pieHeightPoint.y = cat_bigGunMaxRange() * dir.y + selCentrePoint.y;
        pieHeightPoint.z = cat_bigGunMaxRange() * dir.z + selCentrePoint.z;
    }

    //limit dest vector to render list extent
    if (!smSensorsActive)
    {
        maxMoveLength = pieMaxMoveHorizontal;
        moveLengthVector.x = pieHeightPoint.x - selCentrePoint.x;
        moveLengthVector.y = pieHeightPoint.y - selCentrePoint.y;
        moveLengthVector.z = 0.0f;
        moveLength = fmathSqrt(vecMagnitudeSquared(moveLengthVector));
        if (moveLength > maxMoveLength)
        {                                                   //if distance on the plane too far
            vecMultiplyByScalar(moveLengthVector, maxMoveLength / moveLength);
            vecAdd(piePlanePoint, selCentrePoint, moveLengthVector);
            pieHeightPoint.x = piePlanePoint.x;             //clip to the render list horizontally
            pieHeightPoint.y = piePlanePoint.y;
        }
        maxMoveLength = pieMaxMoveVertical;
        moveLength = ABS(pieHeightPoint.z - piePlanePoint.z);
        if (moveLength > maxMoveLength)
        {                                                   //if moved too high
            pieHeightPoint.z = piePlanePoint.z + (pieHeightPoint.z - piePlanePoint.z) * maxMoveLength / moveLength;
        }
    }
    else if (glIsEnabled(GL_DEPTH_TEST))
    {                                                       // CAT: the sensors manager draws without the depth test
        depthWasOn = TRUE;
        glDisable(GL_DEPTH_TEST);
    }

    //now limit end point to the 'hard' limits of the universe.
    //After all, the universe has hard limits you know.
    pieMovePointClipToLimits(&selCentrePoint, &pieHeightPoint);   // CAT: two arguments (0x004e76b0)
    (void)cat_smHeightCircleColorGet();                     // CAT: the exe drops the colour here
    piePlanePoint.x = pieHeightPoint.x;
    piePlanePoint.y = pieHeightPoint.y;
    piePlanePoint.z = selCentrePoint.z;
    if (mouseIsVisible)
    {                                                       // CAT: only with the mouse cursor shown
        pieDistanceReadoutDraw(&pieHeightPoint, &selCentrePoint, TW_MOVE_ORIGIN_COLOR);
    }

    //draw the pizza dish with spokes
    // CAT: the compass's radius is the plane point's distance from the selection (piePointSpecDraw 0x004e7a70)
    if (piePlaneDrawCallback != NULL)
    {
        vector planeDistance;
        vecSub(planeDistance, piePlanePoint, selCentrePoint);
        piePlaneDrawCallback(fmathSqrt(vecMagnitudeSquared(planeDistance)));
    }
    //now draw lines and cross-hairs
    if (pieMovementCursorDrawCallback != NULL)
    {
        pieMovementCursorDrawCallback(distance);
    }
    //draw lines to pie plate
    if (pieLineDrawCallback != NULL)
    {
        pieLineDrawCallback(distance);
    }
    //draw the horizon
    if (mrRenderMainScreen)
    {
        smCurrentWorldPlaneColor = colMultiplyClamped(smWorldPlaneColor, smMovementWorldPlaneDim);
        smHorizonLineDraw(mrCamera, &rndCameraMatrix, &rndProjectionMatrix, mrCamera->clipPlaneFar - mrCamera->farthestZoom);
    }
    rndLightingEnable(FALSE);
    primModeSet2();
    if (smSensorsActive && depthWasOn)
    {
        glEnable(GL_DEPTH_TEST);
    }

#if PIE_MOVE_NEARTO
    //make the ship under cursor glow as a destination of the 'move near to' command
    if (selClosestTarget != NULL)
    {                                                       //if there is a ship under cursor
        selClosestTarget->targetable.flashtimer = taskTimeElapsed+FLASH_TIMER/2;                  //just glow for the next frame only
    }
#endif //PIE_MOVE_NEARTO
}

/*-----------------------------------------------------------------------------
    Name        : pieShipDied
    Description : cleans up main region stuff when a ship dies
    Inputs      :
    Outputs     :
    Return      :
    Note        : call only after ship has been removed from selSelected, selSelecting
----------------------------------------------------------------------------*/
void pieShipDied(struct Ship *ship)
{
    if (pieLastClosestShip == ship)
    {
        pieLastClosestShip = NULL;
    }

    if ((selSelected.numShips == 0) && (piePointSpecMode != PSM_Idle))
    {
        piePointModeOnOff(PIE_SPEC_None, NULL);
    }
}

/*-----------------------------------------------------------------------------
    Name        : piePointModeToggle
    Description : Toggles between xy-mode and z-mode for point specification
    Inputs      : bOn - if true, set Z mode, XY mode otherwise
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void piePointModeToggle(sdword bOn)
{
    // CAT: piePointModeToggle 0x004e8480: no Ctrl case and no tutorial messages; going to height mode keeps the
    //      height point at the plane point while no height was given
    if (!bOn)
    {
        if (piePointSpecMode == PSM_Z)
        {
            piePointSpecMode = PSM_XY;
        }
    }
    else if (piePointSpecMode == PSM_XY)
    {
        piePointSpecMode = PSM_Z;
        if (piePointSpecZ == 0.0f)
        {                                               //if height point not yet specified
            pieHeightPoint = piePlanePoint;
        }
    }
    piePointSpecMouseReset = TRUE;                           //reset location of mouse cursor
}
