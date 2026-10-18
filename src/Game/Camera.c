// =============================================================================
//  Camera.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/20/1997 by gshaw
// =============================================================================

#include "Camera.h"
#include "KeyBindings.h"

#include "FastMath.h"
#include "Key.h"
#include "Mimic.h"      // CAT: cat_mimicGetCameraRadius
#include "render.h"
#include "StatScript.h"
#include "Universe.h"

/*=============================================================================
    Public variables:
=============================================================================*/

sdword camMouseX = 0;
sdword camMouseY = 0;

sword camJoyZoom           = 0;
sword camJoyDeclination    = 0;
sword camJoyRightAscension = 0;

bool8 wheel_up   = FALSE;
bool8 wheel_down = FALSE;

bool32  zoomOutNow = FALSE;
bool32  zoomInNow  = FALSE;

bool8 useSlowWheelZoomIn = FALSE;

/*=============================================================================
    Tweakables:
=============================================================================*/

real32 CAMERA_MOUSE_SENS_MIN   = -1.0f;                 //in log-base-2 space
real32 CAMERA_MOUSE_SENS_MAX   = 1.0f;
real32 CAMERA_MOUSE_SENS       = 1.0f;                  //centre of that range
real32 CAMERA_ANGLE_ROT        = DEG_TO_RAD(0.1f);
real32 CAMERA_DECLINATION_ROT  = DEG_TO_RAD(0.1f);
real32 CAMERA_MAX_DECLINATION  = DEG_TO_RAD(85.0f);
real32 CAMERA_MIN_DECLINATION  = DEG_TO_RAD(-85.0f);
real32 CAMERA_ZOOM_IN          = 0.995f;
real32 CAMERA_ZOOM_OUT         = 1.005f;
real32 CAMERA_KEY_ZOOM_IN      = 0.90f;
real32 CAMERA_KEY_ZOOM_OUT     = 1.10f;
real32 CAMERA_AUTO_ZOOM_IN     = 0.95f;
real32 CAMERA_AUTO_ZOOM_OUT    = 1.05f;
real32 CAMERA_CLIP_NEAR        = 1.0f;
real32 CAMERA_CLIP_FAR         = 35000.0f;
real32 CAMERA_CLIP_FAR_PLANET  = 455000.0f;
real32 CAMERA_FIELD_OF_VIEW    = 90.0f;
real32 CAMERA_WHEEL_ZOOM_IN    = 0.85f;
real32 CAMERA_WHEEL_ZOOM_OUT   = 1.15f;

real32 CAMERA_MAX_ZOOMOUT_DISTANCE = 10000.0f;
real32 CAMERA_MIN_ZOOMOUT_DISTANCE = 10.0f;

real32 CAMERA_FLOAT_SCALAR0 = 1.05f;
real32 CAMERA_FLOAT_SCALAR1 = 1.1f;
real32 CAMERA_FLOAT_DIST_SCALAR = 0.0075f;
real32 CAMERA_FLOAT_OFFSET_SCALAR0 = 0.3f;
real32 CAMERA_FLOAT_OFFSET_SCALAR1 = 0.3f;
real32 CAMERA_FLOAT_OFFSET_SCALAR2 = 0.4f;

scriptEntry CameraTweaks[] =
{
    makeEntry(CAMERA_ANGLE_ROT,scriptSetAngCB),
    makeEntry(CAMERA_DECLINATION_ROT,scriptSetAngCB),
    makeEntry(CAMERA_MAX_DECLINATION,scriptSetAngCB),
    makeEntry(CAMERA_MIN_DECLINATION,scriptSetAngCB),
    makeEntry(CAMERA_ZOOM_IN,scriptSetReal32CB),
    makeEntry(CAMERA_ZOOM_OUT,scriptSetReal32CB),
    makeEntry(CAMERA_KEY_ZOOM_IN,scriptSetReal32CB),
    makeEntry(CAMERA_KEY_ZOOM_OUT,scriptSetReal32CB),
    makeEntry(CAMERA_AUTO_ZOOM_IN,scriptSetReal32CB),
    makeEntry(CAMERA_AUTO_ZOOM_OUT,scriptSetReal32CB),
    makeEntry(CAMERA_FIELD_OF_VIEW,scriptSetReal32CB),
    makeEntry(CAMERA_CLIP_NEAR,scriptSetReal32CB),
    makeEntry(CAMERA_CLIP_FAR,scriptSetReal32CB),
    makeEntry(CAMERA_CLIP_FAR_PLANET,scriptSetReal32CB),
    makeEntry(CAMERA_WHEEL_ZOOM_IN,scriptSetReal32CB),
    makeEntry(CAMERA_WHEEL_ZOOM_OUT,scriptSetReal32CB),
    makeEntry(CAMERA_MAX_ZOOMOUT_DISTANCE,scriptSetReal32CB),
    makeEntry(CAMERA_MIN_ZOOMOUT_DISTANCE,scriptSetReal32CB),
    makeEntry(CAMERA_FLOAT_SCALAR0,scriptSetReal32CB),
    makeEntry(CAMERA_FLOAT_SCALAR1,scriptSetReal32CB),
    makeEntry(CAMERA_FLOAT_DIST_SCALAR,scriptSetReal32CB),
    makeEntry(CAMERA_FLOAT_OFFSET_SCALAR0,scriptSetReal32CB),
    makeEntry(CAMERA_FLOAT_OFFSET_SCALAR1,scriptSetReal32CB),
    makeEntry(CAMERA_FLOAT_OFFSET_SCALAR2,scriptSetReal32CB),
    makeEntry(CAMERA_MOUSE_SENS_MIN,scriptSetReal32CB),
    makeEntry(CAMERA_MOUSE_SENS_MAX,scriptSetReal32CB),
    
    END_SCRIPT_ENTRY
};

/*=============================================================================
    Private Functions:
=============================================================================*/
/*-----------------------------------------------------------------------------
    Name        : camAngleVerify
    Description : Ensures the angle (azimuth) of the camera is within valid ranges.
    Inputs      : camera - camera to examine and potentially modify.
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void camAngleVerify(Camera *camera)
{
    if (camera->angle < 0)
    {
        camera->angle += TWOPI;
    }
    else if (camera->angle > TWOPI)
    {
        camera->angle -= TWOPI;
    }
}

/*-----------------------------------------------------------------------------
    Name        : camDeclinationVerify
    Description : Ensures the camera's declination is within valid ranges.
    Inputs      : camera - camera to examine and potentially modify.
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void camDeclinationVerify(Camera *camera)
{
    if (camera->declination > CAMERA_MAX_DECLINATION)
    {
        camera->declination = CAMERA_MAX_DECLINATION;
    }
    if (camera->declination < CAMERA_MIN_DECLINATION)
    {
        camera->declination = CAMERA_MIN_DECLINATION;
    }
}

/*=============================================================================
    Public Functions:
=============================================================================*/

void cameraSetEyePosition(Camera *camera)
{
    vector dirvec;
    matrix rotymatrix;
    matrix rotzmatrix;
    vector rot1vec;
    vector rot2vec;

    vecSet(dirvec,camera->distance,0.0f,0.0f);

    camAngleVerify(camera);
    camDeclinationVerify(camera);

    matMakeRotAboutY(&rotymatrix,(real32)cos(camera->declination),(real32)sin(camera->declination));
    matMakeRotAboutZ(&rotzmatrix,(real32)cos(camera->angle),(real32)sin(camera->angle));

    matMultiplyMatByVec(&rot1vec,&rotymatrix,&dirvec);
    matMultiplyMatByVec(&rot2vec,&rotzmatrix,&rot1vec);

    vecAdd(camera->eyeposition,camera->lookatpoint,rot2vec);
}

void cameraRotateAbout(Camera *camera,vector about,real32 deg)
{
    real32 rot = DEG_TO_RAD(deg);
    vector position;
    vector rotatedposition;
    matrix rotzmatrix;

    matMakeRotAboutZ(&rotzmatrix,(real32)cos(rot),(real32)sin(rot));

    camera->oldlookatpoint = camera->lookatpoint;
    vecSub(position,camera->lookatpoint,about);
    matMultiplyMatByVec(&rotatedposition,&rotzmatrix,&position);
    vecAdd(camera->lookatpoint,about,rotatedposition);

    vecSub(position,camera->eyeposition,about);
    matMultiplyMatByVec(&rotatedposition,&rotzmatrix,&position);
    vecAdd(camera->eyeposition,about,rotatedposition);
}

void cameraCopyPositionInfo(Camera *dst,Camera *src)
{
    dst->angle          = src->angle;
    dst->declination    = src->declination;
    dst->distance       = src->distance;
    dst->eyeposition    = src->eyeposition;
    dst->lookatpoint    = src->lookatpoint;
    dst->oldlookatpoint = src->oldlookatpoint;
}

void cameraInit(Camera *camera,real32 distance)
{
    camera->playerowner = NULL;

    camera->angle = 0.0f;
    camera->declination = 0.0f;
    camera->distance = distance;

    // CAT-FIX: the exe clears only lookatpoint.x and oldlookatpoint.x (cameraInit 0x0042b7d0 at 0x0042b7ea / 0x0042b7ed),
    //      so a heap camera (nisStart's) keeps uninitialised y and z, which the eye position then adds; all three
    //      are cleared here as in HW1 [audit: M2-R6 nis-fow-camera]
    memset(&camera->lookatpoint,    0, sizeof(vector));
    memset(&camera->oldlookatpoint, 0, sizeof(vector));

    vecSet(camera->upvector,0.0f,0.0f,1.0f);

    cameraSetEyePosition(camera);

    camera->fieldofview   = CAMERA_FIELD_OF_VIEW;
    camera->clipPlaneNear = CAMERA_CLIP_NEAR;
    camera->clipPlaneFar  = CAMERA_CLIP_FAR;
    camera->closestZoom   = CAMERA_MIN_ZOOMOUT_DISTANCE;
    camera->farthestZoom  = CAMERA_MAX_ZOOMOUT_DISTANCE;
    camera->ignoreZoom    = FALSE;
    camera->catSnapToFocus      = FALSE;                   // CAT: the exe's two new fields (cameraInit 0x0042b7d0)
    camera->catScriptedDistance = 0.0f;
}

void cameraChangeLookatpoint(Camera *camera,vector *newlookatpoint)
{
    camera->oldlookatpoint = camera->lookatpoint;
    camera->lookatpoint    = *newlookatpoint;

    cameraSetEyePosition(camera);
}

void cameraRotAngle(Camera *camera,real32 angle)
{
    camera->angle += angle;
    camAngleVerify(camera);
}

/*-----------------------------------------------------------------------------
    Name        : cat_cameraSetAngle                                0x0042b940
    Description : CAT: set the azimuth and bring it into 0..2pi (by whole turns, unlike camAngleVerify's one step);
                  the ship view's automatic turn (svShipViewRender)
----------------------------------------------------------------------------*/
void cat_cameraSetAngle(Camera *camera, real32 angle)
{
    camera->angle = angle;
    while (camera->angle < 0.0f)
    {
        camera->angle += TWOPI;
    }
    while (camera->angle > TWOPI)
    {
        camera->angle -= TWOPI;
    }
}

void cameraRotDeclination(Camera *camera,real32 declination)
{
    camera->declination += declination;
    camDeclinationVerify(camera);
}

void cameraJoystickReset(void)
{
    camJoyZoom           = 0;
    camJoyDeclination    = 0;
    camJoyRightAscension = 0;
}

/*-----------------------------------------------------------------------------
    Name        : cameraMinimumZoom
    Description : walks the focus command to determine minimum zoom distance
    Inputs      : camera - current camera
    Outputs     :
    Return      : minimum zoom distance
----------------------------------------------------------------------------*/
real32 cameraMinimumZoom(Camera* camera)
{
    sdword i;
    FocusCommand* focus;
    real32 dist;
    Ship* ship;
    ShipStaticInfo* shipstaticinfo;
    DerelictStaticInfo* derelictstaticinfo;

    focus = &currentCameraStackEntry(&universe.mainCameraCommand)->focus;
    dist = 0.0f;

    for (i = 0; i < focus->numShips; i++)
    {
        ship = focus->ShipPtr[i];
        if (ship->header.objtype == OBJ_ShipType)
        {
            // CAT: a mimicking ship zooms to the radius of what it mimics (cameraMinimumZoom 0x0042b9a0:
            //      Ship+0x18 bit 3 -> cat_mimicGetCameraRadius 0x005b95c0)
            if (ship->header.catFlags18 & CAT_SHIPFLAG_Mimic)
            {
                if (cat_mimicGetCameraRadius(ship) > dist)
                {
                    dist = cat_mimicGetCameraRadius(ship);
                }
            }
            else
            {
                shipstaticinfo = (ShipStaticInfo*)ship->staticinfo;
                if (shipstaticinfo->minimumZoomDistance > dist)
                {
                    dist = shipstaticinfo->minimumZoomDistance;
                }
            }
        }
        else if (ship->header.objtype == OBJ_DerelictType)
        {
            derelictstaticinfo = (DerelictStaticInfo*)ship->staticinfo;
            if (derelictstaticinfo->minimumZoomDistance > dist)
            {
                dist = derelictstaticinfo->minimumZoomDistance;
            }
        }
    }

    if (dist < camera->closestZoom)
        dist = camera->closestZoom;

    return dist;
}

/*-----------------------------------------------------------------------------
    Name        : cameraZoom
    Description : zooms the camera
    Inputs      : camera - the camera to zoom
                  zoomfactor - zoom scalar
                                  EnforceShipDistances  - use the camera zoom
                                  limits, or use focused ship minimums
    Outputs     : camera->distance is set
    Return      :
----------------------------------------------------------------------------*/
void cameraZoom(Camera *camera,real32 zoomfactor, bool32 EnforceShipDistances)
{
    real32 minZoom;

    camera->distance *= zoomfactor;

    if (!camera->ignoreZoom)
    {
        if (camera->distance > camera->farthestZoom)
        {
            camera->distance = camera->farthestZoom;
        }

        if(EnforceShipDistances)
        {
            minZoom = cameraMinimumZoom(camera);
            if (camera->distance < minZoom)
                camera->distance = minZoom;
        }
        else
        {
            if(camera->distance < camera->closestZoom)
                camera->distance = camera->closestZoom;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cameraControl
    Description : allows user to control the camera.  Call every frame you want
                  the user to be able to control the camera.
    Inputs      : camera, zoomInAllowed
    Outputs     :
    Return      : returns a mask of user actions performed
                  0x01 = moved  0x02 = zoomed
----------------------------------------------------------------------------*/
sdword cameraControl(Camera *camera, bool32 EnforceShipDistances)
{
    sdword UserAction = 0;

    if (wheel_down)
    {
        cameraZoom(camera,(CAMERA_WHEEL_ZOOM_OUT - 1.0f) * CAMERA_MOUSE_SENS + 1.0f, EnforceShipDistances);
        wheel_down = FALSE;
        UserAction |= CAM_USER_ZOOMED;
    }
    
    if (wheel_up)
    {
        cameraZoom(camera, 1.0f - (1.0f - (useSlowWheelZoomIn ? 0.9f : CAMERA_WHEEL_ZOOM_IN)) * CAMERA_MOUSE_SENS, EnforceShipDistances);
        wheel_up = FALSE;
        UserAction |= CAM_USER_ZOOMED;
    }

    if (kbCommandKeyIsHit(CAT_KB_ZoomOut))   // CAT: the exe polls its Zoom_Out key command (cameraControl 0x0042bab0; HW1 zoomOutNow, set by mrKeyPress)
    {
        cameraZoom(camera,(CAMERA_KEY_ZOOM_OUT - 1.0f) * CAMERA_MOUSE_SENS + 1.0f, EnforceShipDistances);
        UserAction |= CAM_USER_ZOOMED;
    }

    if (kbCommandKeyIsHit(CAT_KB_ZoomIn))    // CAT: the Zoom_In key command (cameraControl 0x0042bab0; HW1 zoomInNow)
    {
        cameraZoom(camera,1.0f - (1.0f - CAMERA_KEY_ZOOM_IN) * CAMERA_MOUSE_SENS, EnforceShipDistances);
        UserAction |= CAM_USER_ZOOMED;
    }

#define CAMERA_JOYSTICK_ZOOM_SENS             0.2
#define CAMERA_JOYSTICK_DECLINATION_SENS      0.15
#define CAMERA_JOYSTICK_RIGHT_ASCENSION_SENS  0.15

    if (camJoyZoom != 0)
    {
        cameraZoom(camera, 1.0 + CAMERA_JOYSTICK_ZOOM_SENS * camJoyZoom / SWORD_Max, EnforceShipDistances);
        camJoyZoom = 0;
        UserAction |= CAM_USER_ZOOMED;      
    }
    
    if (camJoyDeclination != 0)
    {
        cameraRotDeclination(camera, CAMERA_JOYSTICK_DECLINATION_SENS * camJoyDeclination / SWORD_Max);
        camJoyDeclination = 0;
        UserAction |= CAM_USER_MOVED;
    }
    
    if (camJoyRightAscension != 0)
    {
        cameraRotAngle(camera, CAMERA_JOYSTICK_RIGHT_ASCENSION_SENS * camJoyRightAscension / SWORD_Max);
        camJoyRightAscension = 0;
        UserAction |= CAM_USER_MOVED;
    }


    if (camMouseX || camMouseY)
    {
        if (keyIsHit(RMOUSE_BUTTON))
        {
            if (keyIsHit(LMOUSE_BUTTON))
            {
                if (camMouseY < 0)
                {
                    cameraZoom(camera, 1.0f + (1.0f - CAMERA_ZOOM_OUT) * (real32)camMouseY * CAMERA_MOUSE_SENS, EnforceShipDistances);
                    UserAction |= CAM_USER_ZOOMED;
                }
                else if (camMouseY > 0)
                {
                    cameraZoom(camera, 1.0f - (1.0f - CAMERA_ZOOM_IN) * (real32)camMouseY * CAMERA_MOUSE_SENS, EnforceShipDistances);
                    UserAction |= CAM_USER_ZOOMED;
                }
            }
            else
            {
                cameraRotDeclination(camera,CAMERA_DECLINATION_ROT * (real32)camMouseY * CAMERA_MOUSE_SENS);
                cameraRotAngle(camera,CAMERA_ANGLE_ROT * (real32)camMouseX * CAMERA_MOUSE_SENS);
                UserAction |= CAM_USER_MOVED;
            }
        }
        camMouseX = camMouseY = 0;
    }

    return UserAction;
}

/*-----------------------------------------------------------------------------
    Name        : cameraSensitivitySet
    Description : Sets the mouse sensitivity.
    Inputs      : sens100 - a value from 0 to 100 with 0 being minimum sensitivity and 100 being max sensitivity.
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void cameraSensitivitySet(sdword sens100)
{
    real32 value;
    value = ((real32)sens100 / 100.0f) * (CAMERA_MOUSE_SENS_MAX - CAMERA_MOUSE_SENS_MIN) + CAMERA_MOUSE_SENS_MIN;
    CAMERA_MOUSE_SENS = (real32)pow(2.0, (double)value);
}

/*-----------------------------------------------------------------------------
    Name        : cameraChecksum
    Description : Compute a checksum of a camera.
    Inputs      : cam - camera to compute checksum of.
    Outputs     :
    Return      : checksum for the camera
----------------------------------------------------------------------------*/
#if CAM_CHECKSUM
udword cameraChecksum(Camera *cam)
{
    return((udword)crc32Compute((ubyte *)&cam->angle, sizeof(Camera) - sizeof(struct Player *)));
}
#endif //CAM_CHECKSUM

/*-----------------------------------------------------------------------------
    Name        : cameraRayCast
    Description : Get a vector for the specified pixel
    Inputs      : cam - what camera to cast the ray from
                  screenX, screenY - what pixel to cast the ray from
                  screenWidth, screenHeight - screenSize
    Outputs     : dest - the casted vector
    Return      :
----------------------------------------------------------------------------*/
void cameraRayCast(vector *dest, Camera *cam, sdword screenX, sdword screenY, sdword screenWidth, sdword screenHeight)
{
    real32 mappedMouseX     = 0.0,
           mappedMouseY     = 0.0,
           normalisedHeight = 0.0;
    
    vector looking,
           right,
           down,
           temp;
    
    // form a right-handed, three axis reference frame with "looking" as the +z axis
    // and the +x axis to the right (which forces the +y axis down - this is the same
    // direction as the mouse coordinate system)
    vecSub(looking, cam->lookatpoint, cam->eyeposition);
    vecCrossProduct(right, looking, cam->upvector);
    vecCrossProduct(down,  looking, right        );

    vecNormalize(&looking);
    vecNormalize(&right);
    vecNormalize(&down);

    // map the cursor on to an imaginary plane a unit distance away from
    // eyeposition whose normal is parallel with "looking"

    // half-height of the screen has a normalised height of
    normalisedHeight = tan(DEG_TO_RAD(cam->fieldofview / 2));

    // NB: the height dimension is how the fieldofview is defined so this is used
    // as the unit measure. Also note that a centred origin is used in this mapping.
    mappedMouseY =  ((real32)screenY / (real32)screenHeight) - 0.5;
    mappedMouseX = (((real32)screenX / (real32)screenWidth ) - 0.5) * rndAspectRatio;

    // clean up the mouse vectors
    vecMultiplyByScalar(right, normalisedHeight * mappedMouseX);
    vecMultiplyByScalar(down,  normalisedHeight * mappedMouseY);

    // This is a hack - it shouldn't be here and I don't know what it represents.
    // The original Relic developer had a similar hack too and he didn't know what
    // it was either. I suspect that it's something to do with the inter-conversion
    // with the perspective projection since the maths in this function is dealing 
    // with orthogonal reference frames.
    vecMultiplyByScalar(looking, 0.49);

    // add the three vectors to get the cast ray vector
    // (eyeball to mouse cursor and beyond)
    vecAdd(temp, looking, right);
    vecAdd(*dest, down, temp);   

    // finally, normalise the cast ray
    vecNormalize(dest);
}

// CAT (M2-R6 nis-fow-camera): HW1's per-type pilot offset tables (PilotOffsets / PilotOffsetsR2) and
//      cameraSetEyePositionBasedOnShip are gone: Cataclysm's pilot view is cat_ccChaseCamFromShip 0x0042cc80
//      (CameraCommand.c) with each type's pilotViewOffsetX/Y/Z from its .shp (CameraChase 0x0042d510 no longer calls
//      the HW1 function)

