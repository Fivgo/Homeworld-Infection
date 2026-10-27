// =============================================================================
//  CameraCommand.c
//  - Handles camera commands such as focus and cancel focus, by keeping
//    a "camera stack"
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 7/3/1997 by gshaw
// =============================================================================

#include "CameraCommand.h"
#include "FOW.h"

#include "Alliance.h"
#include "glinc.h"
#include "Matrix.h"
#include "Mimic.h"
#include "NIS.h"
#include "mainrgn.h"
#include "Cloak.h"
#include "FastMath.h"
#include "Select.h"
#include "Sensors.h"
#include <math.h>
#include "Ships.h"
#include "SinglePlayer.h"
#include "StatScript.h"
#include "Tutor.h"
#include "Task.h"
#include "Tweak.h"
#include "Randy.h"
#include "render.h"
#include "Universe.h"
#include "UnivUpdate.h"


#define DEBUG_CAMERACOMMAND     0

extern udword utyNFrameTicks;

/*=============================================================================
    Defines:
=============================================================================*/

#define TOLERANCE   30

#define FOCUS_OFF_SCREEN    1
#define FOCUS_NEAR_BORDER   2
#define FOCUS_NEAR_BIGBORDER    4
#define FOCUS_NO_SHIPS      8

/*=============================================================================
    Tweakables:
=============================================================================*/

udword MAX_CAMERA_STACKS = 20;

real32 CAMERA_DEFAULT_DISTANCE =   4000.0f;
real32 CAMERA_FAR_DISTANCE     =   6000.0f;
real32 CAMERA_FOCUSFAR_DISTANCE =  6000.0f;

real32 CAMERA_MOTHERSHIPFOCUS_DISTANCE = 6000.0f;
real32 CAMERA_MOTHERSHIPFOCUS_ANGLE = PI;
real32 CAMERA_MOTHERSHIPFOCUS_DECLINATION = -0.5f;

real32 GO_CAMERA_DELAY_TIME = 5.0f;

sdword CAMERA_MAX_ZOOM_OUT_RATE =   40;
sdword CAMERA_SCREEN_BORDER     =   25;
sdword CAMERA_NEAR_BORDER       =   50;
sdword CAMERA_NEAR_BIGBORDER    = 150;

real32 CLOSE_ON_ANGLE_SPEED = 10.0f;

real32 CAMERA_ELASTIC_DISTANCE = 10000.0f;

real32 CAMERA_ELASTIC_PULL_FACTOR = 1000.0f;

real32 CAMERA_OUTBY_SCALE_ZOOM = 25.0f;

real32 CAMERA_MIN_CLOSEFAST_RATE = 1.0f;

real32 CAM_CUBIC_EVAL_TWEAK = 1.6f;
real32 CAM_CUBIC_EVAL_ANGLE_TWEAK = 2.0f;

//Falko: tweak camTrackDistSpeed[0] and camTrackPosSpeed - larger denominator
real32 camTrackAngSpeed[2] = {1.0f/20.0f, 1.0f/10.0f};
real32 camTrackDistSpeed[2] = {1.0f/66.6666f, 1.0f/14.0f};
real32 camTrackDistBase[2] = {10.0f, 10.0f};
real32 camTrackDistThreshold = 1.0f/50.0f;
real32 camTrackPosSpeed = 1.0f/50.0f;
real32 camTrackPosThreshold = 1.0f/50.0f;
real32 camTrackPosBase = 3.0f;

#define MaxCameraDistanceFromPlayer 15000.0f
static float MaxCamDistSquared = MaxCameraDistanceFromPlayer * MaxCameraDistanceFromPlayer;
static long CollapseInterval = 8;      // Happens every 8 frames

static real32 CollapseTimeDelay = 7.0f;   // Seconds before an invalid focus dies
static real32 CollapseTimeStart = 0.0f;   // Start Time when focus became invalid

scriptEntry CameraCommandTweaks[] =
{
    makeEntry(CAMERA_DEFAULT_DISTANCE,scriptSetReal32CB),
    makeEntry(CAMERA_FAR_DISTANCE,scriptSetReal32CB),
    makeEntry(CAMERA_FOCUSFAR_DISTANCE,scriptSetReal32CB),
    makeEntry(CAMERA_MOTHERSHIPFOCUS_DISTANCE,scriptSetReal32CB),
    makeEntry(CAMERA_MOTHERSHIPFOCUS_ANGLE,scriptSetAngCB),
    makeEntry(CAMERA_MOTHERSHIPFOCUS_DECLINATION,scriptSetAngCB),
    makeEntry(GO_CAMERA_DELAY_TIME,scriptSetReal32CB),
    makeEntry(CAMERA_MAX_ZOOM_OUT_RATE,scriptSetSdwordCB),
    makeEntry(CAMERA_SCREEN_BORDER,scriptSetSdwordCB),
    makeEntry(CAMERA_NEAR_BORDER,scriptSetSdwordCB),
    makeEntry(CAMERA_NEAR_BIGBORDER,scriptSetSdwordCB),
    makeEntry(CLOSE_ON_ANGLE_SPEED,scriptSetReal32CB),
    makeEntry(CAMERA_ELASTIC_DISTANCE,scriptSetReal32CB),
    makeEntry(CAMERA_ELASTIC_PULL_FACTOR,scriptSetReal32CB),
    makeEntry(CAMERA_OUTBY_SCALE_ZOOM,scriptSetReal32CB),
    makeEntry(CAMERA_MIN_CLOSEFAST_RATE,scriptSetReal32CB),
    makeEntry(MAX_CAMERA_STACKS,scriptSetUdwordCB),
    {"CAM_TRACK_ANG_AUTO", scriptSetReal32CB, &camTrackAngSpeed[0]},
    {"CAM_TRACK_ANG_USER", scriptSetReal32CB, &camTrackAngSpeed[1]},
    {"CAM_TRACK_DIST_AUTO", scriptSetReal32CB, &camTrackDistSpeed[0]},
    {"CAM_TRACK_DIST_USER", scriptSetReal32CB, &camTrackDistSpeed[1]},
    {"CAM_TRACK_DIST_THRESHOLD", scriptSetReal32CB, &camTrackDistThreshold},
    {"CAM_EXP_DIST_AUTO", scriptSetReal32CB, &camTrackDistBase[0]},
    {"CAM_EXP_DIST_USER", scriptSetReal32CB, &camTrackDistBase[1]},
    {"CAM_TRACK_POS", scriptSetReal32CB, &camTrackPosSpeed},
    {"CAM_TRACK_THRESHOLD", scriptSetReal32CB, &camTrackPosThreshold},
    {"CAM_EXP_POS", scriptSetReal32CB, &camTrackPosBase},
    makeEntry(CAM_CUBIC_EVAL_TWEAK,scriptSetReal32CB),
    makeEntry(CAM_CUBIC_EVAL_ANGLE_TWEAK,scriptSetReal32CB),
    makeEntry(CollapseTimeDelay,scriptSetReal32CB),
    
    END_SCRIPT_ENTRY
};

/*=============================================================================
    Private Functions:
=============================================================================*/

// This is the stuff for the camera collapse functions

bool32 ccCameraTimeoutOverride  = FALSE;

bool32 ccFocusIsLegalToFocusOn(CameraStackEntry *entry)
{
struct Player *pPlayer;
FocusCommand   *pFocus;
Node  *pShipNode;
Ship  *pShip;
long  i;
vector   Vect, LookAt;


   pPlayer = universe.curPlayerPtr;
   LookAt = entry->remembercam.lookatpoint;

   pFocus = &entry->focus;

   for(i=0; i<pFocus->numShips; i++)
   {
      // CAT: a focus ship counts when the camera may focus on it (the test of cat_ccShipIsLegalToFocusOn 0x0042cd40,
      //      inlined in ccFocusIsLegalToFocusOn 0x0042cdd0: any once the player is dead; a ship or mine of an ally,
      //      one the player sees through the fog of war, or any while the NIS scissors or the console are up)
      if(cat_ccShipIsLegalToFocusOn(pFocus->ShipPtr[i]))
      {
         vecSub(Vect, LookAt, pFocus->ShipPtr[i]->obj.posinfo.position);
         if( vecMagnitudeSquared(Vect) < MaxCamDistSquared )
         {
            return TRUE;
         }
      }
   }

   pShipNode = universe.ShipList.head;
   while (pShipNode != NULL)
   {
      pShip = (Ship *)listGetStructOfNode(pShipNode);
      if(pShip->header.objtype == OBJ_ShipType && allianceIsShipAlly(pShip, pPlayer))
      {
         vecSub(Vect, LookAt, pShip->obj.posinfo.position);
         if( vecMagnitudeSquared(Vect) < MaxCamDistSquared )
         {
            return TRUE;
         }
      }

      pShipNode = pShipNode->next;
   }
    return(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : cat_ccShipIsLegalToFocusOn                        0x0042cd40
    Description : CAT (M2-R6 mainrgn-popup): whether the current player may focus the camera on a ship or a mine:
                  always once the player is dead; else an ally's ship, one the player sees through the fog of war,
                  or any while the NIS scissors are closed or the developer console is open.
----------------------------------------------------------------------------*/
bool32 cat_ccShipIsLegalToFocusOn(Ship *ship)
{
    Player *player = universe.curPlayerPtr;
    bool32 sees;

    if (player->playerState == PLAYER_DEAD)
    {
        return TRUE;
    }
    if ((ship->header.objtype == OBJ_ShipType) ||
        ((ship->header.objtype == OBJ_MissileType) && (((Missile *)ship)->missileType == MISSILE_Mine)))
    {
        if (allianceIsShipAlly(ship, player))
        {
            return TRUE;
        }
        // CAT (M2-R6 nis-fow-camera): cat_fowCanPlayerSeeShip 0x0047e810 with the player's grid
        sees = cat_fowCanPlayerSeeShip(player, ship, FALSE);
        if (sees)
        {
            return TRUE;
        }
        if (nisFullyScissored || CAT_MR_CONSOLE_OPEN())
        {
            return TRUE;
        }
    }
    return FALSE;
}

bool32 ccFocusIsLegal(CameraStackEntry *entry)
{
    if (ccFocusIsLegalToFocusOn(entry))
    {
        CollapseTimeStart = taskTimeElapsed;
        return(TRUE);
    }

   if(!ccCameraTimeoutOverride &&
      (taskTimeElapsed - CollapseTimeStart) > CollapseTimeDelay)
      return FALSE;
   else
      return TRUE;
}

void ccCollapseFocusStack(CameraCommand *cameracommand)
{
    CameraStackEntry *entry;
    static long CollapseCount = 0;

    CollapseCount++;
    if(CollapseCount < CollapseInterval)
    {
        return;
    }

    CollapseCount = 0;

    entry = currentCameraStackEntry(cameracommand);
    if(!ccFocusIsLegal(entry))
    {
        if(ccFocusOnFleet(cameracommand))
        {
            // the FleetFocus worked, so delete the empty focus
            cameracommand->dontUseVelocityPredInChase = TRUE;
        }
        else
        {
            // if not, pop to sensors, then focus on the mothership
            smFocusOnMothershipOnClose = TRUE;
            smSensorsBegin(NULL, NULL);
        }
    }
}

typedef struct
{
   double   x,y,z;
} DVect;

void ccGetShipCollCenter(Ship *ship, vector *pos, real32 *rad)
{
    // CAT (ccGetShipCollCenter 0x0042cf30): no slave/master selection circle; the collision centre and sphere size only
    *pos = ship->impactable.collInfo.collPosition;
    *rad = ship->staticinfo->common.staticCollInfo.collspheresize;
}

static const double TAN_FOVConstant = 1.303225;
static const double INV_TAN_FOVConstant = 0.767327207;

// HWI-FIX (M3-R4 nan-positions): TRUE when a focus ship's collision centre is a usable number
static bool32 catFocusPositionFinite(Ship *ship)
{
    vector *p = &ship->impactable.collInfo.collPosition;
    return !(isnan(p->x) || isnan(p->y) || isnan(p->z));
}

udword NewSetFocusPoint(CameraStackEntry *curentry, real32 *targetDistance)
{
udword numShipsFocusOn = curentry->focus.numShips;
udword i;
Ship *ship;
real32 minx,maxx,miny,maxy,minz,maxz, radius, closezoom, minimumZoom;
real32 invLookCam, newDist, maxDist;
vector   position, lookToCam;
double   Rad, LookObjLen, invLookObj;
double   ObjZ, ObjRad;
DVect LookToObj;


   closezoom = curentry->remembercam.closestZoom;
    curentry->remembercam.oldlookatpoint = curentry->remembercam.lookatpoint;

    if (numShipsFocusOn == 0)
    {
      if(targetDistance)
           *targetDistance = closezoom;
        return FOCUS_NO_SHIPS;
    }

    // HWI-FIX (M3-R4 nan-positions, not in the exe): a focus ship whose collision centre is not a number would make the
    //      look-at point, the eye and the whole viewport NaN (the user's Mission17 black screen on focusing group 1);
    //      such ships are left out of the box and of the zoom, and with none left the focus is refused. The exe never has
    //      them (its x87 compares swallow the NaN in aitrackHeadingFunc, now ported); this is the safety net.
    for (i=0;i<numShipsFocusOn;i++)
    {
        if (catFocusPositionFinite(curentry->focus.ShipPtr[i]))
        {
            break;
        }
    }
    if (i == numShipsFocusOn)
    {
        if(targetDistance)
            *targetDistance = closezoom;
        return FOCUS_NO_SHIPS;
    }
    // CAT: the bounding box of the focus ships' collision centres (NewSetFocusPoint 0x0042cf60 reads them directly)
    ship = curentry->focus.ShipPtr[i];
    position = ship->impactable.collInfo.collPosition;

    minx = maxx = position.x;
    miny = maxy = position.y;
    minz = maxz = position.z;

    for (i++;i<numShipsFocusOn;i++)
    {
        ship = curentry->focus.ShipPtr[i];
        if (!catFocusPositionFinite(ship))
        {
            continue;   // HWI-FIX (M3-R4 nan-positions)
        }
        position = ship->impactable.collInfo.collPosition;

        if (position.x < minx) minx = position.x;
        else if (position.x > maxx) maxx = position.x;

        if (position.y < miny) miny = position.y;
        else if (position.y > maxy) maxy = position.y;

        if (position.z < minz) minz = position.z;
      else if (position.z > maxz) maxz = position.z;
    }

    curentry->remembercam.lookatpoint.x = (minx + maxx) * 0.5f;
    curentry->remembercam.lookatpoint.y = (miny + maxy) * 0.5f;
    curentry->remembercam.lookatpoint.z = (minz + maxz) * 0.5f;

   // CAT: the eye direction is normalised in single precision (0x0042d0b4: 1.0 / sqrt as a float)
   vecSub(lookToCam, curentry->remembercam.eyeposition, curentry->remembercam.lookatpoint);
   invLookCam = (real32)(1.0 / sqrt(lookToCam.x * lookToCam.x + lookToCam.y * lookToCam.y + lookToCam.z * lookToCam.z));

    maxDist = 0.0f;
    for(i=0; i<numShipsFocusOn; i++)
    {
        ship = curentry->focus.ShipPtr[i];
        if (!catFocusPositionFinite(ship))
        {
            continue;   // HWI-FIX (M3-R4 nan-positions)
        }
        position = ship->impactable.collInfo.collPosition;
        radius = ship->staticinfo->common.staticCollInfo.collspheresize;

        vecSub(LookToObj, curentry->remembercam.lookatpoint, position);
        LookObjLen = sqrt(vecMagnitudeSquared(LookToObj));
        if (LookObjLen > 0.0)
        {
            invLookObj = 1.0 / LookObjLen;
            Rad = invLookObj * LookToObj.x * (double)-(lookToCam.x * invLookCam) +
                  invLookObj * LookToObj.y * -((double)invLookCam * (double)lookToCam.y) +
                  invLookObj * LookToObj.z * -((double)invLookCam * (double)lookToCam.z);

            ObjZ = Rad * LookObjLen;
            ObjRad = sqrt(1.0 - (Rad*Rad)) * LookObjLen;
        }
        else
        {
            ObjZ = ObjRad = 0.0;
        }

        // CAT: a ship that mimics another zooms to the radius of what it mimics (cat_mimicGetCameraRadius); a
        //      minimum zoom distance of at least 0.1 replaces the collision radius
        if (ship->header.objtype == OBJ_ShipType)
        {
            if (ship->header.catFlags18 & CAT_SHIPFLAG_Mimic)
            {
                minimumZoom = cat_mimicGetCameraRadius(ship);
            }
            else
            {
                minimumZoom = ship->staticinfo->minimumZoomDistance;
            }
            if (minimumZoom >= 0.1f)
            {
                radius = minimumZoom * (real32)INV_TAN_FOVConstant;
            }
        }
        else if (ship->header.objtype == OBJ_DerelictType)
        {
            DerelictStaticInfo* derelictstaticinfo = (DerelictStaticInfo*)ship->staticinfo;
            minimumZoom = derelictstaticinfo->minimumZoomDistance;
            if (minimumZoom >= 0.1f)
            {
                radius = minimumZoom * (real32)INV_TAN_FOVConstant;
            }
        }

        // CAT: summed and scaled as floats (0x0042d297)
        newDist = (radius + (real32)ObjRad) * (real32)TAN_FOVConstant;

        if(newDist < closezoom)
            newDist = closezoom;

        if (newDist + (real32)ObjZ > maxDist)
        {
            maxDist = newDist + (real32)ObjZ;
        }
    }

   if(targetDistance)
      *targetDistance = maxDist;

   useSlowWheelZoomIn = FALSE;
   return 0;
}



/*-----------------------------------------------------------------------------
    Name        : CloseOnAngle
    Description : gradually changes tracking to desired, at rate rate.  Takes into
                  account that angles can range from 0..2PI, and that 0 == 2PI.
    Inputs      : tracking, desired, rate
    Outputs     : tracking
    Return      : returns 1 if tracking has locked on.
----------------------------------------------------------------------------*/
udword CloseOnAngle(real32 *tracking,real32 desired,real32 rate)
{
    real32 diff = desired - *tracking;
    real32 absdiff = ABS(diff);

    if (absdiff > PI)       // at most we will be PI radians off, so if we are "more" than
    {                       // PI radians off we are going the wrong way.
        if (desired > PI)
        {
            desired -= TWOPI;
        }
        else
        {
            desired += TWOPI;
        }
        diff = desired - *tracking;
        absdiff = ABS(diff);
    }

    if (ABS(diff) < rate)
    {
        *tracking = desired;
        return 1;
    }

    if (diff > 0.0f)
    {
        *tracking += rate;
    }
    else
    {
        *tracking -= rate;
    }

    if (*tracking < 0.0f)
    {
        *tracking += TWOPI;
    }
    else if (*tracking > TWOPI)
    {
        *tracking -= TWOPI;
    }

    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : CloseOn
    Description : gradually changes tracking to desired, at rate rate.
    Inputs      : tracking, desired, rate
    Outputs     : tracking
    Return      : returns 1 if tracking has locked on.
----------------------------------------------------------------------------*/
udword CloseOn(real32 *tracking,real32 desired,real32 rate)
{
    real32 diff = desired - *tracking;

    if (ABS(diff) < rate)
    {
        *tracking = desired;
        return 1;
    }

    if (diff > 0.0f)
    {
        *tracking += rate;
    }
    else
    {
        *tracking -= rate;
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : CloseOnFast
    Description : Changes tracking to desired, at a rate proportional to the
                  difference between tracking and desired.  This routine
                  guarantees to close on at least with a rate of minrate.
    Inputs      : tracking, desired, minrate
    Outputs     : tracking
    Return      : returns TRUE if tracking has locked on.
----------------------------------------------------------------------------*/
udword CloseOnFast(real32 *tracking,real32 desired,real32 minrate)
{
    real32 diff = desired - *tracking;
    real32 dist = ABS(diff);
    real32 rate = dist / 8.0f;

    if (rate <= minrate)
    {
        rate = minrate;
    }

    if (dist < rate)
    {
        return 1;
    }

    if (diff > 0.0f)
    {
        *tracking += rate;
    }
    else
    {
        *tracking -= rate;
    }
    return 0;
}


/*----------------------------------------------------------------------------
   EvalCubic:

   This is the Magic Function - It's basically a cubic spline smoothing
   function, which uses a current position and speed (*y0 and *m0) as well
   as a target value and a step size (y1 and t) to smoothly interpolate from
   the current value of *y0 to the desired target at y1.

   Inputs:  y0 - pointer to the current position value
         m0 - pointer to the current speed value
             (initialize this to 0, then let EvalCubic handle it)
         y1 - Target or destination value
         t  - Step interval (0 < t < 1.0)

   Outputs: y0 - *y0 is overwritten by the new position value
          m0 - *m0 is overwritten by the new speed value
-----------------------------------------------------------------------------*/
void EvalCubic( float *y0, float *m0, float y1, float t )
{
float a,b,c,d,e;

   c = *m0;
   d = *y0;
   e = y1-d;

   a = c-e-e;
// b = e+e+e-c-c;
   b = e+e - (c * CAM_CUBIC_EVAL_TWEAK);      // Different curve prediction


   *y0 = ((((a*t+b)*t)+c)*t)+d;
   *m0 = ((((a+a+a)*t)+(b+b))*t)+c;
}

/*-----------------------------------------------------------------------------
    Name        : cat_ccEvalCubicVel
    Description : CAT: new (0x0042d4a0): a Hermite cubic from (y0, m0) to (y1, m1) evaluated at t: y0 and m0 become
                  the point and the slope there (the sensors manager's slipgate paths)
----------------------------------------------------------------------------*/
void cat_ccEvalCubicVel(float *y0, float *m0, float y1, float m1, float t)
{
    float e = y1 - *y0;
    float a = ((m1 + *m0) - e) - e;
    float b = ((e * 3.0f - m1) - *m0) - *m0;
    float c = *m0;

    *y0 = ((a * t + b) * t + c) * t + *y0;
    *m0 = (b + b + a * 3.0f * t) * t + c;
}

/*----------------------------------------------------------------------------
   EvalCubicAngle - Same as EvalCube, but handles *y0 & y1 in radians
   between 0 and 2PI.  Performs the interpolation along the shortest
   angular path between *y0 and y1.

   Inputs: 0.0 < (*y0, y1) < 2PI
         0.0 < t < 1.0

   Outputs: *y0 is modified to reflect the new position, and is
          corrected to be between 0.0 to 2PI
          *m0 is the updated speed value
-----------------------------------------------------------------------------*/

void EvalCubicAngle( float *y0, float *m0, float y1, float t )
{
float a,b,c,d,e;

   c = *m0;
   d = *y0;

   e = y1-d;

   if(e < -PI)
      e += TWOPI;
   else if(e > PI)
      e += -TWOPI;

   a = c-e-e;
//   b = e+e+e-c-c;
   b = e+e+e- (c * CAM_CUBIC_EVAL_ANGLE_TWEAK);

   y1 = ((((a*t+b)*t)+c)*t)+d;

   if(y1 < 0.0f)
      y1 += TWOPI;
   else if(y1 > TWOPI)
      y1 -= TWOPI;

   *y0 = y1;
   *m0 = ((((a+a+a)*t)+(b+b))*t)+c;
}

/*-----------------------------------------------------------------------------
    Name        : CubicLogUnsigned
    Description : Performs value tracking in log-base-n space rather than linearly.
    Inputs      : targ - value to track
                  threshold - how close to target we are to track
                  step - how many tracks to do in total
                  logBase - base of the logarithm space in which tracking is done
    Outputs     : pos - updated position
                  speed - updated speed
    Return      : void
----------------------------------------------------------------------------*/
void CubicLogUnsigned(real32 *pos, real32 *speed, real32 targ, real32 threshold, real32 step, real32 logBase)
{
    real32 fakeDist, fakeTarg;
    real32 posSign, tSign;

    if (*pos > targ)
    {
        fakeDist = *pos - targ - threshold;                 //logarithm is relative to target
        tSign = 1.0f;
    }
    else
    {
        fakeDist = *pos - targ + threshold;                 //logarithm is relative to target
        tSign = -1.0f;
    }

    if (fakeDist < 0.0f)
    {                                                       //handle negative
        posSign = -1.0f;
        fakeDist = -fakeDist;
    }
    else
    {
        posSign = 1.0f;
        if (fakeDist == 0.0f)
        {                                                   //handle zero
            fakeDist = REALlySmall;
        }
    }
    fakeDist = log(fakeDist) / log(logBase);                //compute values in log-base-n space
    fakeTarg = log(threshold) / log(logBase);               //roughly zero in logarithm space
    EvalCubic(&fakeDist, speed, fakeTarg, step);            //update value
    *pos = pow(logBase, fakeDist) * posSign + targ + threshold * tSign;//convert back to linear space
}

/*-----------------------------------------------------------------------------
    Name        : CameraChase
    Description : causes the actualcamera to track the currently set camera.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void CameraChase(CameraCommand *cameracommand,real32 zoomfactor)
{
    Camera *desired = &currentCameraStackEntry(cameracommand)->remembercam;
    vector targetvelocity;
    bool32 dontUseVelocityPredInChase = FALSE;
    bool32 snap = FALSE;
    long  Frames, IndAng;
    vector deLookVector;
    real32 savedPosBase = camTrackPosBase;

    static float angSpeed = 0.0f, decSpeed = 0.0f;
    static float xSpeed = 0.0f, ySpeed = 0.0f, zSpeed = 0.0f;
    static float eyeXSpeed = 0.0f, eyeYSpeed = 0.0f, eyeZSpeed = 0.0f;

    if (cameracommand->dontUseVelocityPredInChase == TRUE)
    {
        dontUseVelocityPredInChase = TRUE;
        cameracommand->dontUseVelocityPredInChase = FALSE;
    }

    // CAT: piloting a ship is the chase view from its pilot offset (cat_ccChaseCamFromShip 0x0042cc80; the exe has
    //      no pilotView switch, and HW1's cameraSetEyePositionBasedOnShip with its per-type tables is gone); with no
    //      single-ship focus the pilot mode ends (CameraChase 0x0042d510)
    if (cameracommand->ccMode & CCMODE_PILOT_SHIP)
    {
        FocusCommand *focus = &cameracommand->currentCameraStack->focus;
        if (focus->numShips == 1)
        {
            cat_ccChaseCamFromShip(&cameracommand->actualcamera,focus->ShipPtr[0]);
            return;
        }
        bitClear(cameracommand->ccMode, CCMODE_PILOT_SHIP);
    }

    vecSet(cameracommand->actualcamera.upvector,0.0f,0.0f,1.0f);

   if(cameracommand->transition == CAMERA_FOCUSORIENT ||
      cameracommand->transition == CAMERA_FOCUSMOVE ||
      cameracommand->transition == CAMERA_FOCUSPOP)
   {
      Frames = utyNFrameTicks;

      if(Frames > 64)
         Frames = 64;

      IndAng = (cameracommand->UserControlled & CAM_USER_MOVED);

      if (!dontUseVelocityPredInChase)
      {
         vecSub(targetvelocity, desired->lookatpoint, desired->oldlookatpoint);
         vecAddTo(cameracommand->actualcamera.lookatpoint, targetvelocity);
         vecAddTo(cameracommand->actualcamera.eyeposition, targetvelocity);
      }

      // CAT: after a scripted focus or zoom (Camera.catSnapToFocus) one pass puts the camera on the focus at once:
      //      the trackers restart from the target with the position tracking's log base 0 (which lands each value on
      //      its target) and the scripted distance, if any, becomes the focus distance (0x0042d670)
      if (cameracommand->actualcamera.catSnapToFocus == TRUE)
      {
         snap = TRUE;
         cameracommand->actualcamera.catSnapToFocus = FALSE;
         Frames = 1;
      }

      while(Frames)
      {
         if (snap)
         {
            cameracommand->actualcamera.lookatpoint = desired->lookatpoint;
            xSpeed = ySpeed = zSpeed = 0.0f;
            camTrackPosBase = 0.0f;
            cameracommand->actualcamera.angle = desired->angle;
            angSpeed = 0.0f;
            cameracommand->actualcamera.declination = desired->declination;
            decSpeed = 0.0f;
            if (cameracommand->actualcamera.catScriptedDistance > 0.0f)
            {
               desired->distance = cameracommand->actualcamera.catScriptedDistance;
               cameracommand->actualcamera.catScriptedDistance = 0.0f;
               cameracommand->zoomInCloseAsPossible = FALSE;
            }
         }
         EvalCubicAngle(&cameracommand->actualcamera.angle, &angSpeed, desired->angle, camTrackAngSpeed[IndAng]);
         EvalCubic(&cameracommand->actualcamera.declination, &decSpeed, desired->declination, camTrackAngSpeed[IndAng]);

         CubicLogUnsigned(&cameracommand->actualcamera.lookatpoint.x, &xSpeed, desired->lookatpoint.x, camTrackPosThreshold, camTrackPosSpeed, camTrackPosBase);
         CubicLogUnsigned(&cameracommand->actualcamera.lookatpoint.y, &ySpeed, desired->lookatpoint.y, camTrackPosThreshold, camTrackPosSpeed, camTrackPosBase);
         CubicLogUnsigned(&cameracommand->actualcamera.lookatpoint.z, &zSpeed, desired->lookatpoint.z, camTrackPosThreshold, camTrackPosSpeed, camTrackPosBase);

         vecSub(deLookVector, cameracommand->actualcamera.eyeposition, cameracommand->actualcamera.lookatpoint);
         vecNormalize(&deLookVector);                       //compute desired eye position
         vecMultiplyByScalar(deLookVector, desired->distance);
         vecAddTo(deLookVector, desired->lookatpoint);

         if (snap)
         {
            cameracommand->actualcamera.eyeposition = deLookVector;
            eyeXSpeed = eyeYSpeed = eyeZSpeed = 0.0f;
            camTrackPosBase = 0.0f;
         }
         CubicLogUnsigned(&cameracommand->actualcamera.eyeposition.x, &eyeXSpeed, deLookVector.x, camTrackPosThreshold, camTrackPosSpeed, camTrackPosBase);
         CubicLogUnsigned(&cameracommand->actualcamera.eyeposition.y, &eyeYSpeed, deLookVector.y, camTrackPosThreshold, camTrackPosSpeed, camTrackPosBase);
         CubicLogUnsigned(&cameracommand->actualcamera.eyeposition.z, &eyeZSpeed, deLookVector.z, camTrackPosThreshold, camTrackPosSpeed, camTrackPosBase);
         vecSub(deLookVector, cameracommand->actualcamera.eyeposition, cameracommand->actualcamera.lookatpoint);
         cameracommand->actualcamera.distance = fsqrt(vecMagnitudeSquared(deLookVector));

         Frames--;
      }

      if (cameracommand->ccMode & CCMODE_FOCUS_CHANGES_LOOKATPOINT_ONLY)
         ;
      else if (!(cameracommand->ccMode & CCMODE_FOCUS_CHANGES_NOTHING))
         cameraSetEyePosition(&cameracommand->actualcamera);

      if (snap)
      {                                                     // CAT: the render lists follow the jump at once
         univUpdateRenderList();
         univUpdateMinorRenderList();
         camTrackPosBase = savedPosBase;
      }
   }
    else if (cameracommand->transition == CAMERA_SET)
    {
        cameracommand->actualcamera.oldlookatpoint = cameracommand->actualcamera.lookatpoint;
        cameracommand->actualcamera.lookatpoint = desired->lookatpoint;
        cameracommand->actualcamera.distance = desired->distance;
        cameracommand->actualcamera.angle = desired->angle;
        cameracommand->actualcamera.declination = desired->declination;
        cameracommand->actualcamera.eyeposition = desired->eyeposition;
        cat_mcResetRequest(&catMotionCueState);             // CAT: the motion cues restart around the new view
    }
}

/*-----------------------------------------------------------------------------
    Name        : GetDistanceAngleDeclination
    Description : Sets the distance, angle and declination given the distvec
                  vector which is a vector pointing from the lookatpoint to
                  the eyepoint.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void GetDistanceAngleDeclination(Camera *camera,vector *distvec)
{
    real32 value;
    // calculate distance
    camera->distance = sqrt(vecMagnitudeSquared(*distvec));

    // now calculate angle and declination to face target
    camera->angle = (real32)atan2(distvec->y,distvec->x);
    value = -distvec->z/camera->distance;

    dbgAssertOrIgnore( ABS(value) <= 1.01f );       // USE 1.01 SO ROUND OFF ERRORS DON'T CRASH US

    if(value < -1.0f)
        value = -1.0f;
    else if(value > 1.0f)
        value = 1.0f;

    camera->declination = (PI/2.0f) - (real32)acos(value);
}

void FocusOnNewEntry(CameraCommand *cameracommand,CameraStackEntry *entry)
{
    Node *deletenode;
    Node *nextnode;

    CameraStackEntry *curentry = currentCameraStackEntry(cameracommand);

    dbgAssertOrIgnore(curentry);

    listAddNodeAfter(&curentry->stacklink,&entry->stacklink,entry);

    // Delete all nodes after new entry
    deletenode = entry->stacklink.next;
    while (deletenode != NULL)
    {
        nextnode = deletenode->next;
        listDeleteNode(deletenode);
        deletenode = nextnode;
    }

    cameracommand->currentCameraStack = entry;

    if (cameracommand->camerastack.num >= MAX_CAMERA_STACKS)
    {
        // remove head of stack
        listDeleteNode(cameracommand->camerastack.head);
    }

#if DEBUG_CAMERACOMMAND
    dbgMessagef("Stack Depth: %d",cameracommand->camerastack.num);
#endif
}

/*=============================================================================
        Public Functions:
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : ccInit
    Description : Initializes the camera command object
    Inputs      : sphereCameraCommandIsIn
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ccInit(CameraCommand *cameracommand)
{
    CameraStackEntry *firstentry;

    listInit(&cameracommand->camerastack);
    cameracommand->ViewEntireMissphere = FALSE;
    cameracommand->dontUseVelocityPredInChase = FALSE;

    cameraInit(&cameracommand->actualcamera,CAMERA_FAR_DISTANCE);

    firstentry = memAlloc(sizeof(CameraStackEntry),"CamStackEntry",NonVolatile);
    firstentry->focus.numShips = 0;
    firstentry->remembercam = cameracommand->actualcamera;

    listAddNode(&cameracommand->camerastack,&firstentry->stacklink,firstentry);
    cameracommand->currentCameraStack = firstentry;

// cameracommand->transition = CAMERA_SET;
   cameracommand->transition = CAMERA_FOCUSORIENT;
   cameracommand->action = CAMACTION_FOCUS;
   cameracommand->transTimeStamp = 0.0f;
// cameracommand->zoomInCloseAsPossible = FALSE;
   cameracommand->zoomInCloseAsPossible = TRUE;
   cameracommand->UserControlled = 0;
   cameracommand->lockCameraTimeStamp = 0.0f;

    cameracommand->ccMode = 0;
    cameracommand->dontFocusOnMe = firstentry;
}

/*-----------------------------------------------------------------------------
    Name        : ccSetModeFlag
    Description : sets a cameracommand mode flag
    Inputs      : ccModeFlag
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ccSetModeFlag(CameraCommand *cameracommand,udword ccModeFlag)
{
    cameracommand->ccMode |= ccModeFlag;
}

/*-----------------------------------------------------------------------------
    Name        : ccClearModeFlag
    Description : clears a cameracommand mode flag
    Inputs      : ccModeFlag
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ccClearModeFlag(CameraCommand *cameracommand,udword ccModeFlag)
{
    bitClear(cameracommand->ccMode,ccModeFlag);
}


/*-----------------------------------------------------------------------------
    Name        : ccFocusOnMyMothership
    Description : changes focus point to point to current players MotherShip
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ccFocusOnMyMothership(CameraCommand *cameracommand)
{
FocusCommand   focus;
CameraStackEntry *entry;

   if (universe.curPlayerPtr->PlayerMothership != NULL)
   {
      focus.ShipPtr[0] = universe.curPlayerPtr->PlayerMothership;
      focus.numShips = 1;
      ccFocus(cameracommand, &focus);
       cameracommand->zoomInCloseAsPossible = FALSE;

      entry = currentCameraStackEntry(cameracommand);
      entry->remembercam.distance = CAMERA_FAR_DISTANCE;

      cameraSetEyePosition(&entry->remembercam);
   }
}

real32 GetObjAngle(SpaceObjRot *robj)
{
    vector heading;
    real32 angle;

    matGetVectFromMatrixCol3(heading,robj->rotinfo.coordsys);

    angle = (real32)atan2(heading.y,heading.x);

    return angle;
}

/*-----------------------------------------------------------------------------
    Name        : ccFocusOnPlayersMissionSphere
    Description : changes focus point to point to current players mission sphere
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ccFocusOnPlayersMothership(CameraCommand *cameracommand,uword playerindex)
{
    FocusCommand   focus;
    CameraStackEntry *entry;
    // find players mission sphere
    Ship *mothership = universe.players[playerindex].PlayerMothership;
    real32 angle, declination, distance;
    vector distVector;

    if (mothership == NULL)
    {
        return;
    }

    // New Version ----------------------------
    focus.ShipPtr[0] = mothership;
    focus.numShips = 1;
    ccFocus(cameracommand, &focus);
    cameracommand->zoomInCloseAsPossible = FALSE;

    entry = currentCameraStackEntry(cameracommand);

    // CAT: only a multiplayer game asks the level for a camera attitude; in single player mission 2 the camera
    //      starts on the other side of the mothership (ccFocusOnPlayersMothership 0x0042dc30)
    if (!singlePlayerGame && spFindCameraAttitude(&entry->remembercam.eyeposition))
    {
        vecSub(distVector, entry->remembercam.eyeposition, entry->remembercam.lookatpoint);
        GetDistanceAngleDeclination(&entry->remembercam, &distVector);
        distance = entry->remembercam.distance;
        angle = entry->remembercam.angle;
        declination = entry->remembercam.declination;
        angle += GetObjAngle((SpaceObjRot *)mothership);
    }
    else
    {
        if (singlePlayerGame && (singlePlayerGameInfo.currentMission == 2))
        {
            angle = GetObjAngle((SpaceObjRot *)mothership) - CAMERA_MOTHERSHIPFOCUS_ANGLE;
        }
        else
        {
            angle = GetObjAngle((SpaceObjRot *)mothership) + CAMERA_MOTHERSHIPFOCUS_ANGLE;
        }
        distance = CAMERA_MOTHERSHIPFOCUS_DISTANCE;
        declination = CAMERA_MOTHERSHIPFOCUS_DECLINATION;
    }
    if (angle < 0)
    {
        angle += TWOPI;
    }
    else if (angle > TWOPI)
    {
        angle -= TWOPI;
    }
    entry->remembercam.distance = distance;
    entry->remembercam.angle = angle;
    entry->remembercam.declination = declination;

    cameraSetEyePosition(&entry->remembercam);

    cameracommand->actualcamera = entry->remembercam;

   // New Version ----------------------------

// Old Version -------------------------------
// entry = currentCameraStackEntry(cameracommand);
// dbgAssertOrIgnore(entry);
// cameraChangeLookatpoint(&cameracommand->actualcamera,&mothership->obj.posinfo.position);
// entry->remembercam = cameracommand->actualcamera;
// cameracommand->UserControlled = 0;
// Old Version -------------------------------
}

/*-----------------------------------------------------------------------------
    Name        : ccClose
    Description : Closes the camera command object
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ccClose(CameraCommand *cameracommand)
{
    listDeleteAll(&cameracommand->camerastack);
}

/*-----------------------------------------------------------------------------
    Name        : ccReset
    Description : Resets the camera command object
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ccReset(CameraCommand *cameracommand)
{
    ccClose(cameracommand);
    ccInit(cameracommand);
}

/*-----------------------------------------------------------------------------
    Name        : ccFocusGeneral
    Description : gives the camera command object the command to focus on
                  a group of ships
    Inputs      : focuscom, command which tells what ships to focus on
              bCloseUp - Zoom in Close = TRUE / FALSE
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ccFocusGeneral(CameraCommand *cameracommand,FocusCommand *focuscom, bool32 bCloseUp)
{
    CameraStackEntry *oldentry;
    CameraStackEntry *entry;
    udword excesssize;
    vector distvec;
    vector lookatdiffvec;
    real32 lookatdiffdist, newDist, MinDist;

    if((tutorial==TUTORIAL_ONLY) && !tutEnable.bFocus)
    {
        return;
    }

    if (focuscom->numShips < 1)
    {
        return;
    }

    //dbgAssertOrIgnore(focuscom->numShips >= 1);

    ccFocusCullRadiusMean(focuscom, RENDER_VIEWABLE_DISTANCE_SQR, NULL);

    if (focuscom->numShips == 0)
    {                                                       //if all ships focussed out (should never happen)
        return;
    }

    // CAT: no proximity-sensor pass (proxInFocusSelection is an empty stub in the exe: ccFocusGeneral 0x0042deb0)

    oldentry = currentCameraStackEntry(cameracommand);

    if (SelectionsAreEquivalent((SelectCommand *)&oldentry->focus,(SelectCommand *)focuscom))
    {
#if DEBUG_CAMERACOMMAND
        dbgMessage("Autozooming in...");
#endif
        cameracommand->zoomInCloseAsPossible = TRUE;
        cameracommand->action = CAMACTION_FOCUS;
      cameracommand->UserControlled = 0;

        if (cameracommand->lockCameraTimeStamp != 0.0f)     // cancel any freeze camera action
        {
            cameracommand->action = CAMACTION_FOCUS;
            cameracommand->lockCameraTimeStamp = 0.0f;
            cameracommand->transition = CAMERA_FOCUSORIENT;
            cameracommand->dontUseVelocityPredInChase = TRUE;
        }

        return;
    }

    excesssize = (focuscom->numShips-1)*sizeof(ShipPtr);

    entry = memAlloc(sizeof(CameraStackEntry)+excesssize,"CamStackEntry",NonVolatile);

    entry->remembercam = oldentry->remembercam;         // start with previous camera and change it

    memcpy(&entry->focus,focuscom,sizeof(FocusCommand)+excesssize);

    // add code here to focus on new object
    NewSetFocusPoint(entry,&MinDist);

   vecSub(distvec, cameracommand->actualcamera.eyeposition, entry->remembercam.lookatpoint);

    // calculate distance
   newDist = sqrt(vecMagnitudeSquared(distvec));
   entry->remembercam.distance = oldentry->remembercam.distance;
   if(entry->remembercam.distance < MinDist)
      entry->remembercam.distance = MinDist;

    vecSub(lookatdiffvec,entry->remembercam.lookatpoint,oldentry->remembercam.lookatpoint);
    lookatdiffdist = vecMagnitudeSquared(lookatdiffvec);

    if (lookatdiffdist >= RENDER_VIEWABLE_DISTANCE_SQR)
    {
        // now calculate angle and declination to face target
        entry->remembercam.angle = (real32)atan2(distvec.y,distvec.x);
        if (entry->remembercam.angle < 0)
        {
            entry->remembercam.angle += TWOPI;
        }

        entry->remembercam.declination = (real32)acos(distvec.z/newDist) - (PI/2.0f);
    }

    cameraSetEyePosition(&entry->remembercam);

    cameracommand->ViewEntireMissphere = FALSE;

    FocusOnNewEntry(cameracommand,entry);

    cameracommand->action = CAMACTION_FOCUS;
    cameracommand->transition = CAMERA_FOCUSORIENT;
    cameracommand->transTimeStamp = cat_taskRealTimeElapsed;     // CAT: the unscaled task clock (0x0042e0a8)
    cameracommand->zoomInCloseAsPossible = bCloseUp;
   cameracommand->UserControlled = 0;
    cameracommand->lockCameraTimeStamp = 0.0f;

   if(oldentry->focus.numShips == 0)
      listDeleteNode(&oldentry->stacklink);
}

// General case of ccFocus : Don't zoom in close to focus target
void ccFocus(CameraCommand *cameracommand, FocusCommand *focuscom)
{
   ccFocusGeneral(cameracommand, focuscom, FALSE);
}

// Special case of ccFocus : Zoom in close to target when using ALT-Focus
void ccFocusClose(CameraCommand *cameracommand, FocusCommand *focuscom)
{
   ccFocusGeneral(cameracommand, focuscom, TRUE);
}

/*-----------------------------------------------------------------------------
    Name        : ccChangeAngleDeclination
    Description : changes the angle and declination of camera
    Inputs      : angle, declination
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ccChangeAngleDeclination(CameraCommand *cameracommand,real32 angle,real32 declination)
{
    CameraStackEntry *entry;

    entry = currentCameraStackEntry(cameracommand);

    entry->remembercam.angle = angle;
    entry->remembercam.declination = declination;

    cameracommand->actualcamera.angle = angle;
    cameracommand->actualcamera.declination = declination;
}

/*-----------------------------------------------------------------------------
    Name        : ccFocusFar
    Description : gives the camera command object the command to focus on
                  a group of ships, from a far distance
    Inputs      : focuscom, command which tells what ships to focus on
                  currentCamera - camera to start with
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ccFocusFar(CameraCommand *cameracommand,FocusCommand *focuscom, Camera *currentCamera)
{
    CameraStackEntry *oldentry;
    CameraStackEntry *entry;
    udword excesssize;

    if (focuscom->numShips < 1)
    {
        return;
    }

    //dbgAssertOrIgnore(focuscom->numShips >= 1);

    oldentry = currentCameraStackEntry(cameracommand);

    excesssize = (focuscom->numShips-1)*sizeof(ShipPtr);

    entry = memAlloc(sizeof(CameraStackEntry)+excesssize,"CamStackEntry",NonVolatile);

//    cameraCopyPositionInfo(&entry->remembercam, currentCamera);
    entry->remembercam = *currentCamera;         // start with current camera and change it


    memcpy(&entry->focus,focuscom,sizeof(FocusCommand)+excesssize);

    // add code here to focus on new object
    NewSetFocusPoint(entry,NULL);

    // calculate distance
    entry->remembercam.distance = CAMERA_FAR_DISTANCE;
    cameraSetEyePosition(&entry->remembercam);

    cameracommand->ViewEntireMissphere = FALSE;

    FocusOnNewEntry(cameracommand,entry);

    cameracommand->action = CAMACTION_FOCUSFAR;
    cameracommand->transition = CAMERA_FOCUSORIENT;
    cameracommand->transTimeStamp = cat_taskRealTimeElapsed;     // CAT: the unscaled task clock (ccFocusFar 0x0042e230)
    cameracommand->zoomInCloseAsPossible = FALSE;
   cameracommand->UserControlled = 0;
    cameracommand->lockCameraTimeStamp = 0.0f;
}


/*-----------------------------------------------------------------------------
    Name        : ccFocusExact
    Description : copies focus of focustocopy to cameracommand
    Inputs      : cameracommand, focustocopy
    Outputs     :
    Return      : TRUE if successful, FALSE if not able to do the operation because
                  focustocopy wasn't focused on any ships.
----------------------------------------------------------------------------*/
bool32 ccFocusExact(CameraCommand *cameracommand,CameraCommand *focustocopy)
{
    FocusCommand *currentfocus = currentFocus(focustocopy);

    if (currentfocus->numShips < 1)
    {
        return FALSE;
    }

    ccFocus(cameracommand,currentfocus);
    cameraCopyPositionInfo(&currentCameraStackEntry(cameracommand)->remembercam,&currentCameraStackEntry(focustocopy)->remembercam);
    cameraCopyPositionInfo(&cameracommand->actualcamera,&focustocopy->actualcamera);

    cameracommand->transition = focustocopy->transition;
    cameracommand->action = focustocopy->action;
    cameracommand->transTimeStamp = focustocopy->transTimeStamp;
    cameracommand->UserControlled = focustocopy->UserControlled;
    cameracommand->zoomInCloseAsPossible = focustocopy->zoomInCloseAsPossible;
    cameracommand->lockCameraTimeStamp = focustocopy->lockCameraTimeStamp;
    cameracommand->dontUseVelocityPredInChase = focustocopy->dontUseVelocityPredInChase;

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : ccCopyCamera
    Description : Copies cameraToCopy into the cameracommand current camera
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ccCopyCamera(CameraCommand *cameracommand,Camera *cameraToCopy)
{
    vector distvec;

    CameraStackEntry *curentry = currentCameraStackEntry(cameracommand);

    vecSub(distvec, cameraToCopy->eyeposition, cameraToCopy->lookatpoint);

    // CAT: GetDistanceAngleDeclination inlined without its clamp of the cosine (ccCopyCamera 0x0042e310)
    cameraToCopy->distance = (real32)sqrt(distvec.x * distvec.x + distvec.y * distvec.y + distvec.z * distvec.z);
    cameraToCopy->angle = (real32)atan2(distvec.y, distvec.x);
    cameraToCopy->declination = (PI/2.0f) - (real32)acos(-distvec.z / cameraToCopy->distance);
//    if (cameraToCopy->distance < 1000.0f)
//    {
//        cameraToCopy->distance = 1000.0f;
//        cameraSetEyePosition(cameraToCopy);
//    }

    cameraCopyPositionInfo(&curentry->remembercam,cameraToCopy);
    cameracommand->actualcamera = curentry->remembercam;
}

/*-----------------------------------------------------------------------------
    Name        : ccCancelFocus
    Description : Cancel's focus (pops off current focused items of the camera
                  stack, and reverts to the old one)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ccCancelFocus(CameraCommand *cameracommand)
{
    Node *node, *prevNode;
    CameraStackEntry *entry, *oldentry;

    if((tutorial==TUTORIAL_ONLY) && !tutEnable.bFocusCancel)
        return;

    oldentry = currentCameraStackEntry(cameracommand);
    node = oldentry->stacklink.prev;

    if (node == NULL)
    {
        return;
    }

    entry = (CameraStackEntry *)listGetStructOfNode(node);

    while (!ccFocusIsLegalToFocusOn(entry) && node != NULL && entry != cameracommand->dontFocusOnMe)
    {
        prevNode = node->prev;
        listDeleteNode(node);
        node = prevNode;
        if (node == NULL)
        {
            return;
        }
        entry = (CameraStackEntry *)listGetStructOfNode(node);
    }

    // CAT: no return at the base entry: the exe focuses whatever entry the walk stopped at (ccCancelFocus 0x0042e3d0)

    cameracommand->ViewEntireMissphere = FALSE;

    ccFocusCullRadiusMean(&entry->focus, RENDER_VIEWABLE_DISTANCE_SQR, NULL);

    entry->remembercam.angle = cameracommand->actualcamera.angle;
    entry->remembercam.declination = cameracommand->actualcamera.declination;
    NewSetFocusPoint(entry,NULL);

    cameraSetEyePosition(&entry->remembercam);

    cameracommand->currentCameraStack = entry;

    cameracommand->action = CAMACTION_CANCELFOCUS;
    cameracommand->transition = CAMERA_FOCUSORIENT;
    cameracommand->transTimeStamp = cat_taskRealTimeElapsed;     // CAT: the unscaled task clock
    cameracommand->zoomInCloseAsPossible = FALSE;
    cameracommand->UserControlled = 0;
    cameracommand->lockCameraTimeStamp = 0.0f;

    if(oldentry->focus.numShips == 0)
    {
        listDeleteNode(&oldentry->stacklink);
    }
}

/*-----------------------------------------------------------------------------
    Name        : ccForwardFocus
    Description : Move's forward on the focus stack, similar to hitting the
                  forward button on internet explorer.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ccForwardFocus(CameraCommand *cameracommand)
{
    Node *next, *beyondNext;
    CameraStackEntry *entry, *oldentry;

    cameracommand->ViewEntireMissphere = FALSE;

   oldentry = currentCameraStackEntry(cameracommand);
    next = oldentry->stacklink.next;

    if (next == NULL)
    {
        return;
    }

    entry = (CameraStackEntry *)listGetStructOfNode(next);

    while (next != NULL && !ccFocusIsLegalToFocusOn(entry))
    {
        beyondNext = next->next;
        listDeleteNode(next);
        next = beyondNext;
        if (next == NULL)
        {
            return;
        }
        entry = (CameraStackEntry *)listGetStructOfNode(next);
    }

    ccFocusCullRadiusMean(&entry->focus, RENDER_VIEWABLE_DISTANCE_SQR, NULL);

    entry->remembercam.angle = cameracommand->actualcamera.angle;
    entry->remembercam.declination = cameracommand->actualcamera.declination;
    NewSetFocusPoint(entry,NULL);

    cameraSetEyePosition(&entry->remembercam);

    cameracommand->currentCameraStack = entry;

    cameracommand->action = CAMACTION_FORWARDFOCUS;
    cameracommand->transition = CAMERA_FOCUSORIENT;
    cameracommand->transTimeStamp = cat_taskRealTimeElapsed;     // CAT: the unscaled task clock (ccForwardFocus 0x0042e4c0)
    cameracommand->zoomInCloseAsPossible = TRUE;
    cameracommand->UserControlled = 0;
    cameracommand->lockCameraTimeStamp = 0.0f;

   if(oldentry->focus.numShips == 0)
      listDeleteNode(&oldentry->stacklink);
}

/*-----------------------------------------------------------------------------
    Name        : ccFocusOnFleet
    Description : focuses on fleet
    Inputs      :
    Outputs     :
    Return      : TRUE if there were ships to focus on, FALSE otherwise
----------------------------------------------------------------------------*/
bool32 ccFocusOnFleet(CameraCommand *cameracommand)
{
    uword numShips = 0;
    uword i;
    CameraStackEntry *entry, *oldentry;
    Node *objnode = universe.RenderList.head;
    Ship *ship;

   oldentry = currentCameraStackEntry(cameracommand);

    // count number of ships to focus on (current player's fleet)
    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        if (ship->header.objtype == OBJ_ShipType && !(ship->header.flags & SOF_Dead))
        {
            if (ship->playerowner == universe.curPlayerPtr)
            {
                numShips++;
            }
        }
        objnode = objnode->next;
    }

    if (numShips == 0)
    {
        return FALSE;
    }

    entry = memAlloc(sizeofCameraStackEntry(numShips),"CamStackEntry",NonVolatile);

    // put ships of current player's fleet into focus
    entry->focus.numShips = numShips;
    i = 0;
    objnode = universe.RenderList.head;
    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        if (ship->header.objtype == OBJ_ShipType && !(ship->header.flags & SOF_Dead))
        {
            if (ship->playerowner == universe.curPlayerPtr)
            {
                entry->focus.ShipPtr[i++] = ship;
            }
        }

        objnode = objnode->next;
    }

    ccFocusCullRadiusMean(&entry->focus, RENDER_VIEWABLE_DISTANCE_SQR, NULL);

    entry->remembercam = cameracommand->actualcamera;
    entry->remembercam.distance = CAMERA_FAR_DISTANCE;
    NewSetFocusPoint(entry,NULL);

    cameraSetEyePosition(&entry->remembercam);

    FocusOnNewEntry(cameracommand,entry);

   if(oldentry->focus.numShips == 0)
      listDeleteNode(&oldentry->stacklink);

    return TRUE;
}

void ccFocusOnFleetSmooth(CameraCommand *cameracommand)
{
    // CAT: a toggle, as ccViewToggleMissionSphere: a second press cancels the fleet focus (ccFocusOnFleetSmooth
    //      0x0042e6d0); UserControlled is left as it is
    if (cameracommand->ViewEntireMissphere)
    {
        ccCancelFocus(cameracommand);
        return;
    }
    if (ccFocusOnFleet(cameracommand))
    {
        cameracommand->ViewEntireMissphere = TRUE;
        cameracommand->action = CAMACTION_VIEWMISSPHERE;
        cameracommand->transition = CAMERA_FOCUSORIENT;
        cameracommand->transTimeStamp = cat_taskRealTimeElapsed;
        cameracommand->zoomInCloseAsPossible = FALSE;
        cameracommand->lockCameraTimeStamp = 0.0f;
    }
}

/*-----------------------------------------------------------------------------
    Name        : ccViewToggleMissionSphere
    Description : toggles between far-out mission sphere view and regular view
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ccViewToggleMissionSphere(CameraCommand *cameracommand)
{
    if (cameracommand->ViewEntireMissphere)
    {
        ccCancelFocus(cameracommand);
    }
    else
    {
        if (ccFocusOnFleet(cameracommand))
        {
            cameracommand->ViewEntireMissphere = TRUE;

            cameracommand->action = CAMACTION_VIEWMISSPHERE;
            cameracommand->transition = CAMERA_FOCUSORIENT;
            cameracommand->transTimeStamp = taskTimeElapsed;
            cameracommand->zoomInCloseAsPossible = FALSE;
            cameracommand->lockCameraTimeStamp = 0.0f;
        }
        else
        {
            return;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : ccRemoveTheseShips
    Description : call this routine to remove references to theseships from cameracommand
    Inputs      : theseships
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ccRemoveTheseShips(CameraCommand *cameracommand,SelectCommand *theseships)
{
    Node *curnode = cameracommand->camerastack.head;
   Node *nextNode;
    CameraStackEntry *stackentry;
   sdword oldFocusShips;
    CameraStackEntry *curentry = currentCameraStackEntry(cameracommand);

   oldFocusShips = curentry->focus.numShips;

    while (curnode != NULL)
    {
      nextNode = curnode->next;
        stackentry = (CameraStackEntry *)listGetStructOfNode(curnode);
        MakeShipsNotIncludeTheseShips(&stackentry->focus,theseships);

      if(!smGhostMode && (stackentry->focus.numShips == 0) && (stackentry != curentry))
           listDeleteNode(curnode);

        curnode = nextNode;
    }

   if(curentry->focus.numShips != oldFocusShips)
      cameracommand->dontUseVelocityPredInChase = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : ccRemoveShip
    Description : call this routine to remove references to ship from cameracommand
    Inputs      : ship
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ccRemoveShip(CameraCommand *cameracommand,Ship *ship)
{
    Node *curnode = cameracommand->camerastack.head;
   Node *nextNode;
    CameraStackEntry *stackentry;
   sdword oldFocusShips;
    CameraStackEntry *curentry = currentCameraStackEntry(cameracommand);

   oldFocusShips = curentry->focus.numShips;

    while (curnode != NULL)
    {
      nextNode = curnode->next;
        stackentry = (CameraStackEntry *)listGetStructOfNode(curnode);
        clRemoveShipFromSelection(&stackentry->focus,ship);

      if(!smGhostMode && (stackentry->focus.numShips == 0) && (stackentry != curentry))
      {
           listDeleteNode(curnode);
      }
      else
      {
          cameracommand->zoomInCloseAsPossible = FALSE;
      }

        curnode = nextNode;
    }

   if(curentry->focus.numShips != oldFocusShips)
      cameracommand->dontUseVelocityPredInChase = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : ccControl
    Description : does all control for the Camera Command.  This routine should
                  be called everytime you want the user to be able to control
                  the camera.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ccControl(CameraCommand *cameracommand)
{
    udword focusOnScreen;
    real32 zoomfactor = 0.0f;
    CameraStackEntry *curentry = currentCameraStackEntry(cameracommand);
    float targetDistance;
   sdword UserAction;

    if (cameracommand->actualcamera.ignoreZoom)
        return;

    if (cameracommand->lockCameraTimeStamp == 0.0f)
        focusOnScreen = NewSetFocusPoint(curentry, &targetDistance);
    else
    {
        if ((taskTimeElapsed - cameracommand->lockCameraTimeStamp) > GO_CAMERA_DELAY_TIME)
        {
            cameracommand->lockCameraTimeStamp = 0.0f;
            cameracommand->transition = CAMERA_FOCUSORIENT;
            cameracommand->dontUseVelocityPredInChase = TRUE;

            focusOnScreen = NewSetFocusPoint(curentry, &targetDistance);
        }
        else
            focusOnScreen = FOCUS_NO_SHIPS;
    }

//  if(curentry->remembercam.distance > targetDistance)
//      zoomInAllowed = TRUE;
//  else
//      zoomInAllowed = FALSE;

   if ((cameracommand->ccMode & CCMODE_LOCK_OUT_USER_INPUT) == 0)
   {
      UserAction = cameraControl(&curentry->remembercam, TRUE);

      if(UserAction)
      {
         cameracommand->UserControlled = UserAction;
         if(UserAction & CAM_USER_ZOOMED)
            cameracommand->zoomInCloseAsPossible = FALSE;

         if(curentry->remembercam.distance < targetDistance)
            curentry->remembercam.distance = targetDistance;
      }
   }


    if (cameracommand->ccMode & CCMODE_FOCUS_CHANGES_LOOKATPOINT_ONLY)
      ;
    else if (!(cameracommand->ccMode & CCMODE_FOCUS_CHANGES_NOTHING))
        cameraSetEyePosition(&curentry->remembercam);


   if(cameracommand->zoomInCloseAsPossible)
   {
        if (cameracommand->action == CAMACTION_FOCUSFAR)
      {
         curentry->remembercam.distance = CAMERA_FOCUSFAR_DISTANCE;
         cameracommand->zoomInCloseAsPossible = FALSE;
      }
      else
         curentry->remembercam.distance = targetDistance;
    }

   if(!smGhostMode && (!(tutorial==TUTORIAL_ONLY) || tutEnable.bGameRunning))
      ccCollapseFocusStack(cameracommand);

    CameraChase(cameracommand,zoomfactor);
}

void ccLockOnTargetNow(CameraCommand *cameracommand)
{
   real32   targetDistance;
    CameraStackEntry *curentry = currentCameraStackEntry(cameracommand);
   Camera *desired = &curentry->remembercam;

   NewSetFocusPoint(curentry, &targetDistance);

    if (cameracommand->ccMode & CCMODE_FOCUS_CHANGES_LOOKATPOINT_ONLY)
      ;
    else if (!(cameracommand->ccMode & CCMODE_FOCUS_CHANGES_NOTHING))
        cameraSetEyePosition(&curentry->remembercam);

   if(cameracommand->zoomInCloseAsPossible)
   {
        if (cameracommand->action == CAMACTION_FOCUSFAR)
      {
         curentry->remembercam.distance = CAMERA_FOCUSFAR_DISTANCE;
         cameracommand->zoomInCloseAsPossible = FALSE;
      }
      else
         curentry->remembercam.distance = targetDistance;
    }

    cameracommand->actualcamera.lookatpoint = desired->lookatpoint;
    cameracommand->actualcamera.oldlookatpoint = cameracommand->actualcamera.lookatpoint;
    cameracommand->actualcamera.distance = desired->distance;
    cameracommand->actualcamera.angle = desired->angle;
    cameracommand->actualcamera.declination = desired->declination;
    cameracommand->actualcamera.eyeposition = desired->eyeposition;
}

/*-----------------------------------------------------------------------------
    Name        : ccFocusCullRadius
    Description : Cull a selection for focussing based on an arbitrary centre point
    Inputs      : in - selection to cull
                  raduisSqr - Square of the radius from mean that is acceptable.
                  centre - centre point to cull about
    Outputs     : out - where to cull the selection to
    Return      :
----------------------------------------------------------------------------*/
sdword ccFocusCullRadiusGeneral(FocusCommand *out, FocusCommand *in, real32 radiusSqr, vector *centre)
{
    sdword index;
    real32 dx, dy, dz, distanceSqr;

    for (out->numShips = index = 0; index < in->numShips; index++)
    {
        dx = ABS(in->ShipPtr[index]->obj.posinfo.position.x - centre->x);
        dy = ABS(in->ShipPtr[index]->obj.posinfo.position.y - centre->y);
        dz = ABS(in->ShipPtr[index]->obj.posinfo.position.z - centre->z);
        distanceSqr = dx * dx + dy * dy + dz * dz;          //compute distance from centre
        if (distanceSqr <= radiusSqr)
        {                                                   //if this ship close enough to the mean
            out->ShipPtr[out->numShips] = in->ShipPtr[index];
            out->numShips++;                                //add this ship to selection
        }
    }
    return(out->numShips);
}

/*-----------------------------------------------------------------------------
    Name        : ccFocusCullRadiusMean
    Description : Cull out any objects in the specified selection that are more
                    than radius from the mean position.
    Inputs      : selection - selection to cull
                  radius - Square of the radius from mean that is acceptable.
    Outputs     : centre - where to store the calculated centre point.  May be NULL.
    Return      : Number of objects remaining in selection
----------------------------------------------------------------------------*/
sdword ccFocusCullRadiusMean(FocusCommand *selection, real32 radiusSqr, vector *centreDest)
{
    sdword index, iOuter;
    vector centre = {0.0f, 0.0f, 0.0f};
    MaxSelection localSelection;
    sdword maxFocussed = 0, iMaxFocussed = 0;

    if (selection->numShips == 0)
    {                                                       //nothing to focus on
        return 0;
    }

    //get average position
    for (index = 0; index < selection->numShips; index++)
    {
        vecAddTo(centre, selection->ShipPtr[index]->obj.posinfo.position);
    }
    vecMultiplyByScalar(centre, (1.0f / (real32)index));

    //now go through the list and cull out any objects farther from the origin
    //than a specified amount
    ccFocusCullRadiusGeneral((FocusCommand *)&localSelection, selection, radiusSqr, &centre);

    //using the mean didn't work; try to centre around any given ship
    if (localSelection.numShips == 0)
    {                                                       //if everything was culled out
        maxFocussed = 0;
        for (iOuter = 0; iOuter < selection->numShips; iOuter++)
        {
            centre = selection->ShipPtr[iOuter]->obj.posinfo.position;
            ccFocusCullRadiusGeneral((FocusCommand *)&localSelection, selection, radiusSqr, &centre);
            if (localSelection.numShips > maxFocussed)
            {                                               //keep track of the best selection
                maxFocussed = localSelection.numShips;
                iMaxFocussed = iOuter;
            }
        }
        dbgAssertOrIgnore(maxFocussed > 0);                         //one selection combo should have worked
        centre = selection->ShipPtr[iMaxFocussed]->obj.posinfo.position;//use the best selection we found
        ccFocusCullRadiusGeneral((FocusCommand *)&localSelection, selection, radiusSqr, &centre);
    }
    selSelectionCopy((MaxAnySelection *)selection, (MaxAnySelection *)&localSelection);
    if (centreDest != NULL)
    {                                                       //store dest centre point
        *centreDest = centre;
    }
    return(selection->numShips);
}

/*-----------------------------------------------------------------------------
    Name        : cat_ccIsPilotingShip                              0x0042c2e0
    Description : CAT (M2-R5 hud): TRUE while the main camera pilots one ship (CCMODE_PILOT_SHIP with a one-ship
                  focus). Moved here from SoundEvent.c's stand-in; the status bars skip the piloted ship
                  (selSelectedDraw 0x005086a0), the sound events voice it (soundEventUpdate).
----------------------------------------------------------------------------*/
bool32 cat_ccIsPilotingShip(void)
{
    CameraStackEntry *entry = universe.mainCameraCommand.currentCameraStack;

    return (entry->focus.numShips == 1 && bitTest(universe.mainCameraCommand.ccMode, CCMODE_PILOT_SHIP) &&
            entry->focus.ShipPtr[0]->header.objtype == OBJ_ShipType);
}

/*-----------------------------------------------------------------------------
    Name        : cat_ccIsPilotingThisShip                          0x0042c310
    Description : CAT (M2-R5 hud): TRUE while the main camera pilots this ship.
----------------------------------------------------------------------------*/
bool32 cat_ccIsPilotingThisShip(Ship *ship)
{
    return (cat_ccIsPilotingShip() && universe.mainCameraCommand.currentCameraStack->focus.ShipPtr[0] == ship);
}

/*-----------------------------------------------------------------------------
    Name        : cat_ccChaseCamFromShip                            0x0042cc80
    Description : CAT (M2-R6 nis-fow-camera): the pilot's view of a ship: the eye at the ship's pilot offset (its
                  static pilotViewOffsetX/Y/Z, from the .shp) in the ship's frame, looking along its heading, the
                  ship's up vector up
----------------------------------------------------------------------------*/
void cat_ccChaseCamFromShip(Camera *camera, Ship *ship)
{
    vector offset, worldOffset;

    offset.x = ship->staticinfo->pilotViewOffsetX;
    offset.y = ship->staticinfo->pilotViewOffsetY;
    offset.z = ship->staticinfo->pilotViewOffsetZ;
    matMultiplyMatByVec(&worldOffset, &ship->rotatable.rotinfo.coordsys, &offset);
    vecAdd(camera->eyeposition, ship->obj.posinfo.position, worldOffset);
    camera->lookatpoint.x = ship->rotatable.rotinfo.coordsys.m13 + camera->eyeposition.x;
    camera->lookatpoint.y = ship->rotatable.rotinfo.coordsys.m23 + camera->eyeposition.y;
    camera->lookatpoint.z = ship->rotatable.rotinfo.coordsys.m33 + camera->eyeposition.z;
    camera->upvector.x = ship->rotatable.rotinfo.coordsys.m11;
    camera->upvector.y = ship->rotatable.rotinfo.coordsys.m21;
    camera->upvector.z = ship->rotatable.rotinfo.coordsys.m31;
}

/*-----------------------------------------------------------------------------
    Name        : cat_ccFocusCloseScripted / cat_ccFocusScripted    0x0042e140 / 0x0042e160
    Description : CAT (M2-R6 nis-fow-camera): a script's focus (close or normal): ccFocusGeneral, then the actual
                  camera snaps to it on the next chase (Camera.catSnapToFocus)
----------------------------------------------------------------------------*/
void cat_ccFocusCloseScripted(CameraCommand *cameracommand, FocusCommand *focuscom)
{
    ccFocusGeneral(cameracommand, focuscom, TRUE);
    cameracommand->actualcamera.catSnapToFocus = TRUE;
}

void cat_ccFocusScripted(CameraCommand *cameracommand, FocusCommand *focuscom)
{
    ccFocusGeneral(cameracommand, focuscom, FALSE);
    cameracommand->actualcamera.catSnapToFocus = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_ccSetDistanceScripted                         0x0042e180
    Description : CAT (M2-R6 nis-fow-camera): a script's camera distance: the focus and actual distances, the snap
                  and the scripted distance the snap applies
----------------------------------------------------------------------------*/
void cat_ccSetDistanceScripted(CameraCommand *cameracommand, real32 distance)
{
    cameracommand->currentCameraStack->remembercam.distance = distance;
    cameracommand->actualcamera.distance = distance;
    cameracommand->actualcamera.catSnapToFocus = TRUE;
    cameracommand->actualcamera.catScriptedDistance = distance;
}

/*-----------------------------------------------------------------------------
    Name        : cat_ccZoomScripted                                0x0042e1a0
    Description : CAT (M2-R6 nis-fow-camera): a script's zoom: the distance a fraction of the way from the minimum
                  zoom to the actual distance (nothing when the camera is at or inside the minimum)
----------------------------------------------------------------------------*/
void cat_ccZoomScripted(CameraCommand *cameracommand, real32 fraction)
{
    real32 minZoom = cameraMinimumZoom(&cameracommand->actualcamera);
    real32 range = cameracommand->actualcamera.distance - minZoom;

    if (range > 0.0f)
    {
        range = range * fraction + minZoom;
        cameracommand->currentCameraStack->remembercam.distance = range;
        cameracommand->actualcamera.catSnapToFocus = TRUE;
        cameracommand->actualcamera.distance = range;
        cameracommand->actualcamera.catScriptedDistance = range;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_ccSetDistance                                 0x0042e210
    Description : CAT (M2-R6 nis-fow-camera): the focus and actual camera distances, without a snap
----------------------------------------------------------------------------*/
void cat_ccSetDistance(CameraCommand *cameracommand, real32 distance)
{
    cameracommand->currentCameraStack->remembercam.distance = distance;
    cameracommand->actualcamera.distance = distance;
}

/*=============================================================================
    CAT (M2-R6 nis-fow-camera): the motion cues (cat_mc*, 0x0042c090..0x0042cc80; the state is the global
    0x00c49400). Every caller (universeReset, utyNewGameStart, smSensorsCloseForGood, kasfSetMotionCues, with
    catMotionCues on) sets mode 1 and asks for a reset; the main view draws them each frame (cat_mcRender from
    cat_rndRenderObjectList). The reset scatters the points with random stream 0.
=============================================================================*/
CatMotionCues catMotionCueState;

void cat_mcSetMode(CatMotionCues *cues, sdword mode)
{
    cues->mode = mode;
    cues->fade = 1.0f;
}

void cat_mcResetRequest(CatMotionCues *cues)
{
    cues->resetRequest = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_mcpReset                                      0x0042c0c0
    Description : scatter the cue points at random (stream 0) in the box around the camera's eye; mode 0 also
                  transforms them to the screen
----------------------------------------------------------------------------*/
void cat_mcpReset(Camera *camera, CatMotionCues *cues)
{
    sdword index;
    real32 width;
    CatMotionCuePoint *point;

    cues->velocity.x = cues->velocity.y = cues->velocity.z = 0.0f;
    hmatMultiplyHMatByHMat(&cues->transform, &rndProjectionMatrix, &rndCameraMatrix);
    cues->speed = 0.0f;
    cues->updated = FALSE;
    if (camera == NULL)
    {
        cues->lookatpoint.x = cues->lookatpoint.y = cues->lookatpoint.z = 0.0f;
    }
    else
    {
        cues->lookatpoint = camera->lookatpoint;
    }
    if (cues->mode == 0)
    {
        cues->boxMin[0] = cues->boxMin[1] = cues->boxMin[2] = -15000.0f;
        cues->boxMax[0] = cues->boxMax[1] = cues->boxMax[2] = 15000.0f;
        cues->numPoints = CAT_MC_MAX_POINTS;
    }
    else
    {
        cues->boxMin[0] = cues->boxMin[1] = cues->boxMin[2] = -4500.0f;
        cues->boxMax[0] = cues->boxMax[1] = cues->boxMax[2] = 4500.0f;
        cues->numPoints = 100;
    }
    if (cues->numPoints > CAT_MC_MAX_POINTS)
    {
        cues->numPoints = CAT_MC_MAX_POINTS;
    }
    if (camera == NULL)
    {                                                       // CAT-FIX: the exe reads the eye through NULL here
        cues->numPoints = 0;
    }
    for (index = 0, point = cues->points; index < cues->numPoints; index++, point++)
    {
        // each coordinate: (rand % round(width)) - width / 2 around the eye (the exe rounds by the 1.5 * 2^52 trick)
        width = cues->boxMax[0] - cues->boxMin[0];
        point->position.x = (real32)(ranRandom(RANDOM_STREAM_0) % (udword)lrint((double)width)) - width * 0.5f;
        width = cues->boxMax[1] - cues->boxMin[1];
        point->position.y = (real32)(ranRandom(RANDOM_STREAM_0) % (udword)lrint((double)width)) - width * 0.5f;
        width = cues->boxMax[2] - cues->boxMin[2];
        point->position.z = (real32)(ranRandom(RANDOM_STREAM_0) % (udword)lrint((double)width)) - width * 0.5f;
        point->position.x += camera->eyeposition.x;
        point->position.y += camera->eyeposition.y;
        point->position.z += camera->eyeposition.z;
        if (cues->mode == 0)
        {
            cat_hmatMultiplyHMatByVec(&point->screen, &cues->transform, &point->position);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_mcFocusAverageVelocity                        0x0042c340
    Description : the mean velocity of the main camera's focus ships; FALSE when there is no focus, a focus object is
                  not a ship, a ship (outside the pilot view of a one-ship focus) has a command other than a move, or
                  any ship moves slower than 130 (|vx| + |vy| + |vz|)
----------------------------------------------------------------------------*/
bool32 cat_mcFocusAverageVelocity(vector *velocity)
{
    CameraStackEntry *entry = universe.mainCameraCommand.currentCameraStack;
    sdword index;
    Ship *ship;

    velocity->x = velocity->y = velocity->z = 0.0f;
    if (entry->focus.numShips == 0)
    {
        return FALSE;
    }
    for (index = 0; index < entry->focus.numShips; index++)
    {
        ship = entry->focus.ShipPtr[index];
        if (ship->header.objtype != OBJ_ShipType)
        {
            return FALSE;
        }
        if ((!(universe.mainCameraCommand.ccMode & CCMODE_PILOT_SHIP) || (entry->focus.numShips != 1)) &&
            (ship->command != NULL) && (ship->command->ordertype.order != COMMAND_MOVE))
        {
            return FALSE;
        }
        if (ABS(ship->obj.posinfo.velocity.x) + ABS(ship->obj.posinfo.velocity.y) + ABS(ship->obj.posinfo.velocity.z) < 130.0f)
        {
            return FALSE;
        }
        velocity->x += ship->obj.posinfo.velocity.x;
        velocity->y += ship->obj.posinfo.velocity.y;
        velocity->z += ship->obj.posinfo.velocity.z;
    }
    velocity->x /= (real32)entry->focus.numShips;
    velocity->y /= (real32)entry->focus.numShips;
    velocity->z /= (real32)entry->focus.numShips;
    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_mcNormalScalar                                0x0042c450
    Description : once a frame (mode 1): the streak vector follows the focus's mean velocity (per second, times the
                  fade, times -0.1 * 0.2: the dust drifts against the motion) with EvalCubic; zero while the camera's
                  last focus change is less than 0.35 s old (transTimeStamp) or the focus is not moving
----------------------------------------------------------------------------*/
void cat_mcNormalScalar(Camera *camera, CatMotionCues *cues)
{
    static float velSpeed[3] = { 0.0f, 0.0f, 0.0f };        // 0x008dfbc0
    real32 scale, step;
    vector target, mean;

    if ((camera == NULL) || cues->updated)
    {
        return;
    }
    cues->updated = TRUE;
    if (cues->mode == 0)
    {
        return;
    }
    if (universe.phystimeelapsed == 0.0f)
    {
        scale = 1.0f;
    }
    else
    {
        scale = 1.0f / universe.phystimeelapsed;
    }
    scale = scale * cues->fade * 0.2f;
    if ((cat_taskRealTimeElapsed - universe.mainCameraCommand.transTimeStamp < (real32)0.35) ||
        !cat_mcFocusAverageVelocity(&mean))
    {
        target.x = target.y = target.z = 0.0f;
    }
    else
    {
        target.x = mean.x * (real32)-0.1;
        target.y = mean.y * (real32)-0.1;
        target.z = mean.z * (real32)-0.1;
    }
    step = universe.phystimeelapsed * 0.8f;
    if (step > 0.9f)
    {
        step = 0.9f;
    }
    EvalCubic(&cues->velocity.x, &velSpeed[0], target.x * scale, step);
    EvalCubic(&cues->velocity.y, &velSpeed[1], target.y * scale, step);
    EvalCubic(&cues->velocity.z, &velSpeed[2], target.z * scale, step);
    cues->velocity.x = target.x * scale;
    cues->velocity.y = target.y * scale;
    cues->velocity.z = target.z * scale;
    cues->lookatpoint = camera->lookatpoint;
    cues->speed = fsqrt(cues->velocity.z * cues->velocity.z + cues->velocity.y * cues->velocity.y +
                        cues->velocity.x * cues->velocity.x);
}

//wraps a coordinate into [eye + min, eye + max] (the exe's floor arithmetic)
static real32 catMcWrap(real32 value, real32 low, real32 high)
{
    if (value > high)
    {
        return value - (real32)floor((double)((value - low) / (high - low))) * (high - low);
    }
    if (value < low)
    {
        return (real32)(floor((double)((high - value) / (high - low))) * (double)(high - low) + (double)value);
    }
    return value;
}

/*-----------------------------------------------------------------------------
    Name        : cat_mcRender                                      0x0042c630
    Description : the main view's motion cues: the requested reset, the streak update, the points wrapped into the
                  box around the eye, then drawn blended (mode 0: cat_mcDrawCues; mode 1, once the streak is at least
                  0.5 long: for each point a line from p + 2v (transparent) to p + v and from p + v to p (transparent),
                  grey 0.7, alpha min(speed * 0.03, 0.9))
----------------------------------------------------------------------------*/
void cat_mcRender(Camera *camera, CatMotionCues *cues)
{
    sdword index, oldTexture, oldLighting;
    real32 alpha;
    CatMotionCuePoint *point;

    if (cues->resetRequest)
    {
        cat_mcpReset(camera, cues);
        cues->resetRequest = FALSE;
    }
    cat_mcNormalScalar(camera, cues);
    if (camera == NULL)
    {
        return;
    }
    cues->updated = FALSE;
    for (index = 0, point = cues->points; index < cues->numPoints; index++, point++)
    {
        point->position.x = catMcWrap(point->position.x, cues->boxMin[0] + camera->eyeposition.x, cues->boxMax[0] + camera->eyeposition.x);
        point->position.y = catMcWrap(point->position.y, cues->boxMin[1] + camera->eyeposition.y, cues->boxMax[1] + camera->eyeposition.y);
        point->position.z = catMcWrap(point->position.z, cues->boxMin[2] + camera->eyeposition.z, cues->boxMax[2] + camera->eyeposition.z);
    }
    oldTexture = rndTextureEnable(FALSE);
    oldLighting = rndLightingEnable(FALSE);
    glPushAttrib(GL_BLEND);                                 // as the exe (0xBE2 passed as the attribute mask)
    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (cues->mode == 0)
    {
        cat_mcDrawCues(cues);
    }
    else if (cues->speed >= 0.5f)
    {
        alpha = cues->speed * 0.03f;
        if (alpha > 0.9f)
        {
            alpha = 0.9f;
        }
        glBegin(GL_LINES);
        for (index = 0, point = cues->points; index < cues->numPoints; index++, point++)
        {
            glColor4f(0.7f, 0.7f, 0.7f, 0.0f);
            glVertex3f(cues->velocity.x + cues->velocity.x + point->position.x, cues->velocity.y + cues->velocity.y + point->position.y,
                       cues->velocity.z + cues->velocity.z + point->position.z);
            glColor4f(0.7f, 0.7f, 0.7f, alpha);
            glVertex3f(point->position.x + cues->velocity.x, point->position.y + cues->velocity.y, point->position.z + cues->velocity.z);
            glColor4f(0.7f, 0.7f, 0.7f, alpha);
            glVertex3f(point->position.x + cues->velocity.x, point->position.y + cues->velocity.y, point->position.z + cues->velocity.z);
            glColor4f(0.7f, 0.7f, 0.7f, 0.0f);
            glVertex3f(point->position.x, point->position.y, point->position.z);
        }
        glEnd();
    }
    glPopAttrib();
    rndTextureEnable(oldTexture);
    rndLightingEnable(oldLighting);
}

/*-----------------------------------------------------------------------------
    Name        : cat_mcDrawCues                                    0x0042ca10
    Description : mode 0: each point transformed to the screen (the projection and camera matrices, scaled by
                  0.0001) and drawn with identity matrices as two lines along its motion since the last frame (times
                  the fade): from s + 2d (transparent) to s + d and from s + d to s (transparent), alpha (|dx| + |dy|)
                  times 60
----------------------------------------------------------------------------*/
void cat_mcDrawCues(CatMotionCues *cues)
{
    sdword index;
    vector screen, delta;
    real32 alpha;
    CatMotionCuePoint *point;

    if (cues->fade == 0.0f)
    {
        return;
    }
    hmatMultiplyHMatByHMat(&cues->transform, &rndProjectionMatrix, &rndCameraMatrix);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glBegin(GL_LINES);
    for (index = 0, point = cues->points; index < cues->numPoints; index++, point++)
    {
        cat_hmatMultiplyHMatByVec(&screen, &cues->transform, &point->position);
        screen.x *= 0.0001f;
        screen.y *= 0.0001f;
        screen.z *= 0.0001f;
        delta.x = (point->screen.x - screen.x) * cues->fade;
        delta.y = (point->screen.y - screen.y) * cues->fade;
        delta.z = (point->screen.z - screen.z) * cues->fade;
        point->screen = screen;
        alpha = (real32)((double)(ABS(delta.y) + ABS(delta.x)) * 60.0);
        glColor4f(0.7f, 0.7f, 0.7f, 0.0f);
        glVertex3f(delta.x + delta.x + screen.x, delta.y + delta.y + screen.y, delta.z + delta.z + screen.z);
        glColor4f(0.7f, 0.7f, 0.7f, alpha);
        glVertex3f(delta.x + screen.x, delta.y + screen.y, delta.z + screen.z);
        glColor4f(0.7f, 0.7f, 0.7f, alpha);
        glVertex3f(delta.x + screen.x, delta.y + screen.y, delta.z + screen.z);
        glColor4f(0.7f, 0.7f, 0.7f, 0.0f);
        glVertex3f(screen.x, screen.y, screen.z);
    }
    glEnd();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}
