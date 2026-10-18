// =============================================================================
//  CameraCommand.h
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 7/3/1997 by gshaw
// =============================================================================

#ifndef ___CAMERACOMMAND_H
#define ___CAMERACOMMAND_H

#include "Camera.h"
#include "ShipSelect.h"

/*=============================================================================
    Types:
=============================================================================*/

typedef SelectCommand FocusCommand;

// each camera stack entry contains information about the camera state,
// and what ships it should focus on.
typedef struct
{
    Node stacklink;
    Camera remembercam;
    FocusCommand focus;
} CameraStackEntry;

typedef enum
{
    CAMERA_SET,
    CAMERA_FOCUSORIENT,
    CAMERA_FOCUSMOVE,
    CAMERA_FOCUSPOP
} TransitionType;

typedef enum
{
    CAMACTION_FOCUS,
    CAMACTION_FOCUSFAR,
    CAMACTION_CANCELFOCUS,
    CAMACTION_FORWARDFOCUS,
    CAMACTION_VIEWMISSPHERE
} ActionType;

#define CCMODE_LOCK_OUT_USER_INPUT              1
#define CCMODE_FOCUS_CHANGES_LOOKATPOINT_ONLY   2
#define CCMODE_FOCUS_CHANGES_NOTHING            4

#define CCMODE_PILOT_SHIP                       8

// The CameraCommand object contains a camera stack, for focusing on ships
// and canceling focus, a toggle to ViewEntireMissphere, and also an
// actual camera which always tries to "chase" the currently active
// camera on the stack.
typedef struct
{
    LinkedList camerastack;
    CameraStackEntry *currentCameraStack;
    bool32 ViewEntireMissphere;
    Camera actualcamera;
    TransitionType transition;
    ActionType action;
    real32 transTimeStamp;
    sdword UserControlled;
    bool32 zoomInCloseAsPossible;
    real32 lockCameraTimeStamp;
    bool32 dontUseVelocityPredInChase;
    udword ccMode;
    CameraStackEntry *dontFocusOnMe;
} CameraCommand;
#if defined(__i386__) || defined(_M_IX86)
// CAT: with the exe's 0x5C-byte Camera (Camera.h): CameraStackEntry 0x78 (ccInit 0x0042db30 allocates 0x78) and
//      CameraCommand 0x94 (ccMode at +0x8C, dontFocusOnMe at +0x90)
_Static_assert(sizeof(CameraStackEntry) == 0x78, "CameraStackEntry is 0x78 bytes in cataclysm.exe");
_Static_assert(sizeof(CameraCommand) == 0x94, "CameraCommand is 0x94 bytes in cataclysm.exe");
_Static_assert(offsetof(CameraCommand, ccMode) == 0x8C, "CameraCommand.ccMode at +0x8C");
#endif

// CAT (M2-R6 nis-fow-camera): the camera's motion cues, Cataclysm's drifting space dust (the global 0x00c49400, drawn
//      by cat_mcRender 0x0042c630 from the main view when catMotionCues 0x008b8844 is on). Mode 0: 1500 points in
//      a +-15000 box drawn as screen-space streaks (cat_mcDrawCues); mode 1 (every caller sets it): 100 points in a
//      +-4500 box drawn as world-space streaks along the focus's mean velocity. Layout from the exe's offsets; names
//      chosen here.
#define CAT_MC_MAX_POINTS   1500
typedef struct
{
    vector position;                    // +0x00 world position (wrapped into the box around the eye)
    vector screen;                      // +0x0C transformed position (mode 0)
} CatMotionCuePoint;
typedef struct
{
    real32 boxMin[3];                   // +0x00
    real32 boxMax[3];                   // +0x0C
    vector lookatpoint;                 // +0x18 the camera's look-at point at the last update
    vector velocity;                    // +0x24 the smoothed streak vector (EvalCubic)
    real32 speed;                       // +0x30 its length
    hmatrix transform;                  // +0x34 rndProjectionMatrix * rndCameraMatrix
    sdword mode;                        // +0x74 0 or 1 (cat_mcSetMode)
    bool32 updated;                     // +0x78 cat_mcNormalScalar ran this frame
    bool8 resetRequest;                 // +0x7C cat_mcResetRequest
    CatMotionCuePoint points[CAT_MC_MAX_POINTS];   // +0x80
    sdword numPoints;                   // +0x8D20
    real32 fade;                        // +0x8D24 1.0 after cat_mcSetMode
} CatMotionCues;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(CatMotionCues, mode) == 0x74, "CatMotionCues.mode at +0x74 (cat_mcSetMode 0x0042c090)");
_Static_assert(offsetof(CatMotionCues, points) == 0x80, "CatMotionCues points at +0x80 (cat_mcpReset 0x0042c0c0)");
_Static_assert(offsetof(CatMotionCues, numPoints) == 0x8D20, "CatMotionCues.numPoints at +0x8D20");
_Static_assert(offsetof(CatMotionCues, fade) == 0x8D24, "CatMotionCues.fade at +0x8D24");
#endif
extern CatMotionCues catMotionCueState;                                         // CAT: 0x00c49400

/*=============================================================================
    Macros:
=============================================================================*/

#define sizeofFocusCommand sizeofSelectCommand
#define sizeofCameraStackEntry(n) (sizeof(CameraStackEntry) + ((n-1)*sizeof(ShipPtr)))

// provides the currently active camera stack entry, where x is pointer to CameraCommand
#define currentCameraStackEntry(x) ((x)->currentCameraStack)

// provides the currently active focus, where x is pointer to CameraCommand
#define currentFocus(x) (&(currentCameraStackEntry(x))->focus)

#define ccFocusSize(n)  (sizeof(FocusCommand) + sizeof(ShipPtr) * ((n) - 1))

/*=============================================================================
    Data:
=============================================================================*/
extern real32 CAMERA_DEFAULT_DISTANCE;
extern real32 CAMERA_FAR_DISTANCE;
extern real32 CAMERA_FOCUSFAR_DISTANCE;

/*=============================================================================
    Functions:
=============================================================================*/

// Camera Command Layer

udword NewSetFocusPoint(CameraStackEntry *curentry, real32 *targetDistance);

void ccInit(CameraCommand *cameracommand);
void ccClose(CameraCommand *cameracommand);
void ccReset(CameraCommand *cameracommand);
void ccFocus(CameraCommand *cameracommand,FocusCommand *focuscom);
void ccFocusClose(CameraCommand *cameracommand, FocusCommand *focuscom);
void ccFocusFar(CameraCommand *cameracommand,FocusCommand *focuscom, Camera *currentCamera);
void ccFocusOnMyMothership(CameraCommand *cameracommand);
extern bool32 ccCameraTimeoutOverride;
void ccFocusOnPlayersMothership(CameraCommand *cameracommand,uword playerindex);
bool32 ccFocusOnFleet(CameraCommand *cameracommand);
bool32 ccFocusExact(CameraCommand *cameracommand,CameraCommand *focustocopy);
void ccFocusOnFleetSmooth(CameraCommand *cameracommand);
void ccFocusGeneral(CameraCommand *cameracommand,FocusCommand *focuscom, bool32 bCloseUp);
void ccForwardFocus(CameraCommand *cameracommand);
void ccCancelFocus(CameraCommand *cameracommand);
void ccViewToggleMissionSphere(CameraCommand *cameracommand);
void ccControl(CameraCommand *cameracommand);
void ccLockOnTargetNow(CameraCommand *cameracommand);
bool32 cat_ccIsPilotingShip(void);                  // CAT: 0x0042c2e0 (M2-R5 hud)
bool32 cat_ccIsPilotingThisShip(Ship *ship);        // CAT: 0x0042c310 (M2-R5 hud)

void ccSetModeFlag(CameraCommand *cameracommand,udword ccModeFlag);     // use CCMODE_whatever flags
void ccClearModeFlag(CameraCommand *cameracommand,udword ccModeFlag);   // use CCMODE_whatever flags

void ccChangeAngleDeclination(CameraCommand *cameracommand,real32 angle,real32 declination);

void ccCopyCamera(CameraCommand *cameracommand,Camera *cameraToCopy);

// call this routine to update camera command layer to remove ship
void ccRemoveShip(CameraCommand *cameracommand,Ship *ship);

// call this routine to remove references to theseships from cameracommand
void ccRemoveTheseShips(CameraCommand *cameracommand,SelectCommand *theseships);
void GetDistanceAngleDeclination(Camera *camera,vector *distvec);

//cubic curve evaluation:
void EvalCubic( float *y0, float *m0, float y1, float t );
void cat_ccEvalCubicVel(float *y0, float *m0, float y1, float m1, float t);   // CAT: 0x0042d4a0 (M2-R6 managers)
void ccGetShipCollCenter(Ship *ship, vector *pos, real32 *rad);

//cull a focus selection to a certain radius
sdword ccFocusCullRadiusMean(FocusCommand *selection, real32 radiusSqr, vector *centre);
sdword ccFocusCullRadiusGeneral(FocusCommand *out, FocusCommand *in, real32 radiusSqr, vector *centre);
bool32 cat_ccShipIsLegalToFocusOn(Ship *ship);                                  // CAT: 0x0042cd40 (M2-R6)

// CAT (M2-R6 nis-fow-camera): the pilot's chase view, the scripted focus / zoom calls and the motion cues
void cat_ccChaseCamFromShip(Camera *camera, Ship *ship);                        // 0x0042cc80
void cat_ccFocusCloseScripted(CameraCommand *cameracommand, FocusCommand *focuscom);   // 0x0042e140
void cat_ccFocusScripted(CameraCommand *cameracommand, FocusCommand *focuscom);        // 0x0042e160
void cat_ccSetDistanceScripted(CameraCommand *cameracommand, real32 distance);         // 0x0042e180
void cat_ccZoomScripted(CameraCommand *cameracommand, real32 fraction);                // 0x0042e1a0
void cat_ccSetDistance(CameraCommand *cameracommand, real32 distance);                 // 0x0042e210
void cat_mcSetMode(CatMotionCues *cues, sdword mode);                                  // 0x0042c090
void cat_mcResetRequest(CatMotionCues *cues);                                          // 0x0042c0b0
void cat_mcpReset(Camera *camera, CatMotionCues *cues);                                // 0x0042c0c0
bool32 cat_mcFocusAverageVelocity(vector *velocity);                                   // 0x0042c340
void cat_mcNormalScalar(Camera *camera, CatMotionCues *cues);                          // 0x0042c450
void cat_mcRender(Camera *camera, CatMotionCues *cues);                                // 0x0042c630
void cat_mcDrawCues(CatMotionCues *cues);                                              // 0x0042ca10

#endif
