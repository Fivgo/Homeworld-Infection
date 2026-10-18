/*=============================================================================
    Name    : NIS.h
    Purpose : Non-Interactive Sequence code.

    Created 12/2/1997 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___NIS_H
#define ___NIS_H

#include "B-Spline.h"
#include "Camera.h"
#include "font.h"
#include "Key.h"
#include "ShipDefs.h"
#include "ShipSelect.h"
#include "SpaceObj.h"
#include "StatScript.h"
#include "Task.h"
#include "texreg.h"
#include "Types.h"


/*=============================================================================
    Switches:
=============================================================================*/

#define NIS_NORMALIZE_ANGLES        1           //convert incoming angular dimensions to radians an get them in the range of 0..TWOPI
#define NIS_RENDERLIST_DISABLE      0           //stop updating render list when playing an NIS

#ifdef HW_BUILD_FOR_DEBUGGING

#define NIS_ERROR_CHECKING          1           //general error checking
#define NIS_VERBOSE_LEVEL           3           //control specific output code
#define NIS_DRAW_CAMERA             1           //draw the NIS camer while not looking from it's POV
#define NIS_TIME_CONTROLS           1           //key to pause the NIS's
#define NIS_PAUSE_KEY               0           //key to pause the NIS's
#define NIS_PRINT_INFO              NUMPAD6     //key to print NIS time info
#define NIS_CAMERA_RELEASE          NUMPAD4     //allow release of the NIS camera to the standard game camera
#define NIS_SHIPS_SELECTABLE        1           //make NIS ships selectable
#define NIS_SEEKABLE                1           //can seek in NIS's
#define NIS_TEST                    1

#else

#define NIS_ERROR_CHECKING          0           //general error checking
#define NIS_VERBOSE_LEVEL           0           //control specific output code
#define NIS_DRAW_CAMERA             0           //draw the NIS camer while not looking from it's POV
#define NIS_TIME_CONTROLS           0           //key to pause the NIS's
#define NIS_PAUSE_KEY               0           //key to pause the NIS's
#define NIS_PRINT_INFO              0           //key to print NIS time info
#define NIS_CAMERA_RELEASE          0           //allow release of the NIS camera to the standard game camera
#define NIS_SHIPS_SELECTABLE        0           //make NIS ships selectable
#define NIS_SEEKABLE                1           //can seek in NIS's
#define NIS_TEST                    0

#endif

/*=============================================================================
    Definitions:
=============================================================================*/
//general
#define NIS_VersionNumber           0x101
#define NIS_Identifier              "Crannnberry"
#define NIS_OneKeyframe             5
#define NIS_FirstKeyFrame           2
#define NIS_FrameRate               30.0f

// Use 1.85:1 aspect ratio for screens with aspect ratio below 1.5:1, e.g. 4:3. This is what the original game does.
// For anything higher, e.g. typical 16:9 screens, use 2.39:1 aspect ratio.
// And in case someone has a very wide monitor, e.g. 21:9, use a minimal letter boxing height of 1/15 of the screen height.
#define NIS_LetterHeight            max( (((float)MAIN_WindowWidth/(float)MAIN_WindowHeight) < 1.5f) ? \
                                         ((MAIN_WindowHeight - MAIN_WindowWidth * 20 / 37) / 2) : \
                                         ((MAIN_WindowHeight - MAIN_WindowWidth * 100 / 239) / 2), \
                                         MAIN_WindowHeight / 15)

#define NIS_HeaderExtra             64          //extra amount of RAM at the end of a header
#define NIS_LookScalar              10000.0f

//additional key controls
#define NIS_Pause                   NUMPAD5
#define NIS_FastForwardRate         4.0f        //play at 4x speed
#define NIS_FastForward             NUMPAD0
#define NIS_NormalSpeed             NUMDOTKEY
#define NIS_Back1Second             NUMPAD2
#define NIS_Back10Second            NUMPAD3

//objectmotion flags
#define OMF_ObjectDied              0x00000001
#define OMF_RemainAtEnd             0x00000002
#define OMF_Firing                  0x00000004
#define OMF_FocusAtEnd              0x00000008
#define OMF_KeepOnMoving            0x00000010
#define OMF_CurvesDeleted           0x00000020

//flags for an NIS header
#define NHF_RacesSwapped            0x00000001
#define NIS_Seeked                  0x00000002  //seeked recently

#define NIS_DefaultFOV              75.0f

#define NIS_NumberTextCards         16
#define NIS_NewLine                 "#n"
#define NIS_NewLineLength           2

//75 deg trigger happy cone
#define NIS_TriggerHappyAngle       0.7071067811865f

#define NIS_NCameraKeys             5
#define NIS_CameraLengthFactor      0.1f
#define NIS_CameraBlendPullBackFactor   2.5f
#define NIS_JumpDistance            300.0f

#define NIS_CentreShip              BIT15       //this bit in the type field indicates the ship is to be the centre of the NIS

//flags for actor selection
#define NIS_ActorMask               0xe0000000
#define NIS_ActorShiftBits          29

#define NIS_PlayedProperlyMargin    0.99f

#define NIS_EnvNameLength           32
#define NIS_NumberEvents            256

//static
#define NIS_NoiseTextureWidth       64
#define NIS_NoiseTextureHeight      8
#define NIS_NumberStatics           3
#define NIS_NumberExtraStatics      1
#define NIS_SMStaticIndex           NIS_NumberStatics

/*=============================================================================
    Type definitions:
=============================================================================*/
//spaceobj motion path
typedef struct spaceobjpath
{
    sdword instance;                            //instance of this type object
    ShipType type;                              //type of ship
    ShipType parentIndex;                         //parent ship (NULL if none)
    udword race;                                //0..5 (see racedefs.h).  For determining what exact ship to use.
    sdword nSamples;                            //number of samples, including 2 extra at start and end
    real32 timeOffset;                          //if positive, path will wait this long before starting
    real32 *times;                              //absolute times, from start of NIS (timeOffset will be added in)
    tcb *parameters;                            //tension, continuity, bias
    real32 *curve[6];                           //x,y,z,h,p,b
}
spaceobjpath;

//camera motion path
typedef struct tagcamerapath
{
    real32 oLength;                             //length (seconds) of motion path
    sdword nSamples;                            //number of samples, including 2 extra at start and end
    real32 timeOffset;                          //if positive, path will wait this long before starting
    real32 *times;                              //absolute times, from start of NIS (timeOffset will be added in)
    tcb *parameters;                            //tension, continuity, bias
    real32 *curve[6];                           //same parameters as the spaceobj's
} camerapath;

//also needed later: camera and light motion paths
typedef spaceobjpath lightpath;

// CAT: Cataclysm's event opcodes (the dispatch table nisEventDispatch 0x0088888c, 75 entries; the codes the
//      exe's *Set callbacks store, e.g. nisDamageLevelSet 0x004d4770 stores 0x40): six new events are inserted
//      (Repair after Attack, CameraClipNear / CameraClipFar after CameraFOV, OpenSlipgate / CloseSlipgate after
//      SMPTEOff) and nine appended after MinimumLOD, so every code from HaltAttack on moves
typedef enum
{
    NEO_DeathDamage,                            // 0x00
    NEO_DeathProjectile,
    NEO_DeathBeam,
    NEO_DeathCollision,
    NEO_Attack,                                 // 0x04
    NEO_Repair,                                 // 0x05 CAT: new (cat_nisRepair 0x004cf370)
    NEO_HaltAttack,                             // 0x06
    NEO_Fire,
    NEO_Show,
    NEO_Hide,
    NEO_Invincible,                             // 0x0a
    NEO_Vincible,
    NEO_SoundEvent,
    NEO_SpeechEvent,
    NEO_DisableDefaultSpeech,
    NEO_EnableDefaultSpeech,
    NEO_CustomEffect,                           // 0x10
    NEO_CustomGunEffect,
    NEO_GunShoot,
    NEO_RemainAtEnd,
    NEO_FleetSpeechEvent,                       // 0x14
    NEO_CameraFOV,
    NEO_CameraClipNear,                         // 0x16 CAT: new (cat_nisCameraClipNear 0x004d0110)
    NEO_CameraClipFar,                          // 0x17 CAT: new (cat_nisCameraClipFar 0x004d0140)
    NEO_CameraCut,                              // 0x18
    NEO_BlackScreenStart,
    NEO_BlackScreenEnd,
    NEO_TextCard,                               // 0x1b
    NEO_TextScroll,
    NEO_ScissorOut,
    NEO_Focus,
    NEO_FocusAtEnd,
    NEO_StartMusic,                             // 0x20
    NEO_StopMusic,
    NEO_BlackFadeSet,
    NEO_BlackFadeTo,
    NEO_ColorScheme,
    NEO_MeshAnimationStart,                     // 0x25
    NEO_MeshAnimationStop,
    NEO_MeshAnimationPause,
    NEO_MeshAnimationSeek,
    NEO_BlendCameraEndPoint,
    NEO_MusicVolume,                            // 0x2a
    NEO_SpeechVolume,
    NEO_SFXVolume,
    NEO_TrailMove,
    NEO_TrailZeroLength,
    NEO_UniversePauseToggle,
    NEO_UniverseHideToggle,                     // 0x30
    NEO_NewEnvironment,
    NEO_LinkEnvironment,
    NEO_AnimaticSpeechEvent,
    NEO_SMPTEOn,                                // 0x34
    NEO_SMPTEOff,
    NEO_OpenSlipgate,                           // 0x36 CAT: new (cat_nisOpenSlipgate 0x004d00b0)
    NEO_CloseSlipgate,                          // 0x37 CAT: new (cat_nisCloseSlipgate 0x004d00d0)
    NEO_StaticOn,                               // 0x38
    NEO_StaticOff,
    NEO_HideDerelict,                           // 0x3a
    NEO_ShowDerelict,
    NEO_StartNewNIS,                            // 0x3c
    NEO_Attributes,
    NEO_AttackKasSelection,
    NEO_KeepMovingAtEnd,
    NEO_DamageLevel,                            // 0x40
    NEO_MinimumLOD,
    NEO_LightAmbient,                           // 0x42 CAT: new (cat_nisLightAmbient 0x004cf920)
    NEO_LightDiffuse,                           // 0x43 CAT: new (cat_nisLightDiffuse 0x004cf970)
    NEO_GlobalAmbient,                          // 0x44 CAT: new (cat_nisGlobalAmbient 0x004cf9c0)
    NEO_NumberLights,                           // 0x45 CAT: new (cat_nisNumberLights 0x004cfa10)
    NEO_Mimic,                                  // 0x46 CAT: new (cat_nisMimic 0x004d5fa0)
    NEO_Fog,                                    // 0x47 CAT: new (cat_nisFog 0x004d08f0)
    NEO_Ebolts,                                 // 0x48 CAT: new (cat_nisEbolts 0x004d0850)
    NEO_BeastEffect,                            // 0x49 CAT: new (cat_nisBeastEffect 0x004d08b0)
    NEO_Subtitle,                               // 0x4a CAT: new (cat_nisSubtitle 0x004cf740)
    NEO_LastNEO                                 // 0x4b
}
nisopcode;

//structure for a NIS time event
typedef struct
{
    real32 time;                                //time of the event
    nisopcode code;                             //what event it is
    sdword shipID;                              //index of the object this event works on, not always a ship
    sdword initialID;                           //index of event in the .script file for correctly time-sorting events
    // CAT: six parameters (the exe's event is 0x28 bytes: the *Set callbacks realloc count * 0x28 and the fog event
    //      cat_nisFogSet 0x004d3490 writes +0x10..+0x24); HW1 had two
    smemsize param[6];                            //parameters, event-specific
}
nisevent;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(nisevent) == 0x28, "nisevent is 0x28 bytes in cataclysm.exe (nisAttackSet 0x004d10f0)");
#endif

//parameters for a SMPTE time counter
typedef struct
{
    sdword x, y;                                //where to print the timecode
    bool8 bRightJustified;                      //is it right justified?
    real32 startTime;                           //when it was started (relative to now)
    fonthandle font;                            //font to print it with
    color c;                                    //color to print it in
    char *format;                               //formatting string
}
nisSMPTE;

//patameters for NEO_StaticOn
typedef struct
{
    sdword index;                               //there can be multiple static effects going on at once
    sdword nLines;                              //average number of lines per frame
    sdword nLinesVariation;                     //variation in number of lines
    real32 width, widthVariation;               //horizontal size of static elements
    real32 y, yVariation;                       //vertical position and variation
    real32 hue, hueVariation;                   //colors and variations in HLS space
    real32 lum, lumVariation;
    real32 sat, satVariation;
    real32 alpha, alphaVariation;               //alpha parameters, if applicable
    bool8 bAlpha;                               //does this type of static use alpha?
    bool8 bTextured;
    trhandle texture;                           //optional texture handle if the elements are textured
}
nisstatic;

//for linking in generic objects
typedef struct
{
    DerelictStaticInfo *staticInfo;             //pointer to the data for this object
    char name[1];
}
nisgenericobject;

//structure for a whole NIS
typedef struct
{
    char identifier[12];                        //NIS_Identifier
    udword version;                             //NIS_VersionNumber
    udword flags;                               //general flags (dynamic)
    char *stringBlock;                          //pointer to string block
    sdword stringBlockLength;                   //length of the string block
    real32 length;                              //length (seconds) of NIS
    real32 loop;                                //loop point for NIS (or negative for no loop)
    sdword nObjectPaths;                        //number of motion paths in the file
    spaceobjpath *objectPath;                   //pointer to list of motion paths
    sdword nCameraPaths;                        //number of camera motion paths
    camerapath *cameraPath;
    sdword nLightPaths;                         //number of light motion paths
    lightpath *lightPath;
    sdword nEvents;                             //number of events, NULL for now
    nisevent *events;                           //void * because not initially used
    //after here there are NIS_HeaderExtra bytes of data
    //to be used by loaded data.  It is not stored in the file.
    sdword nGenericObjects;                     //number of different generic objects loaded
    nisgenericobject **genericObject;           //registry of genric objects loaded for this NIS
    sdword iLookyObject;                        //the official "mid point of 2-phase focus zoom look at point" object
    ubyte extra[NIS_HeaderExtra - 12];          //data for future expansion
}
nisheader;

//structures for animating spaceobjs, cameras and lights
typedef struct
{
    SpaceObjRotImp *spaceobj;                   //object controlled by these splines
    sdword parentIndex;
    splinecurve *curve[6];                      //motion paths (x,y,z,h,p,b)
    udword flags;                               //flags
//    bool8  bObjectDied;                         //this object has died a natural death
}
objectmotion;

typedef struct
{
    Camera *cam;                                //actual camera structure
    splinecurve *curve[6];                      //motion paths (x,y,z,h,p,b)
}
cameramotion;

//... structure for camera and light motion too

//structure for an NIS currently playing
typedef struct
{
    nisheader *header;                          //pointer to header which contains motion path data, should it ever be needed
    vector nisPosition;                         //where the nis is playing
    matrix nisMatrix;                           //attitude of the playing NIS
    real32 timeElapsed;                         //time elapsed since NIS started
    sdword nObjects;                            //number of spaceobj's being updated in this NIS
    splinecurve *objectSplines;                 //allocated pool of object paths
    objectmotion *objectsInMotion;              //spaceobj's being updated by this NIS
//    sdword nLights;                           //additional info for lights and cameras
//  ... lightmotion *lightsInMotion;
    sdword nCameras;
    splinecurve *cameraSplines;                 //allocated pool of camera paths
    cameramotion *camerasInMotion;
    sdword iCurrentEvent;
}
nisplaying;

//structure for text cards currently visible on-screen
typedef struct
{
    real32 creationTime;                        //when this text card was created
    real32 duration;                            //how long this text card is supposed to last
    real32 fadeIn;                              //duration of fade in from black
    real32 fadeOut;                             //duration of fade out to black, if any
    color c;                                    //color of text card
    fonthandle font;                            //font to draw in
    sdword x, y;                                //where to draw the text
    sword margin;                               //margin for use with multi-line text
    char *text;                                 //what text to draw
    nisplaying *NIS;                            //NIS this text card belongs to
}
nistextcard;

typedef void (*nisDoneCB)(void);

typedef struct
{
    float theta, phi;
    char name[NIS_EnvNameLength];
}
nisenvironment;

typedef void (*niseventdispatch)(nisplaying* NIS, nisevent* event);


typedef struct
{
    char identifier[12];                        //NIS_Identifier
    udword version;                             //NIS_VersionNumber
    udword flags;                               //general flags (dynamic)
    udword stringBlock;   //char*                       //pointer to string block
    sdword stringBlockLength;                   //length of the string block
    real32 length;                              //length (seconds) of NIS
    real32 loop;                                //loop point for NIS (or negative for no loop)
    sdword nObjectPaths;                        //number of motion paths in the file
    udword objectPath; //spaceobjpath*    //pointer to list of motion paths
    sdword nCameraPaths;                        //number of camera motion paths
    udword cameraPath; //camerapath*
    sdword nLightPaths;                         //number of light motion paths
    udword lightPath; //lightpath*
    sdword nEvents;                             //number of events, NULL for now
    udword events;  // nisevent*                         //void * because not initially used
    //after here there are NIS_HeaderExtra bytes of data
    //to be used by loaded data.  It is not stored in the file.
    sdword nGenericObjects;                     //number of different generic objects loaded
    udword genericObject; //nisgenericobject**          //registry of genric objects loaded for this NIS
    sdword iLookyObject;                        //the official "mid point of 2-phase focus zoom look at point" object
    ubyte extra[NIS_HeaderExtra - 12];          //data for future expansion
}
nisheader_disk;

typedef struct spaceobjpath_disk
{
    sdword instance;                            //instance of this type object
    sdword type;                              //type of ship
    sdword parentIndex;                         //parent ship (NULL if none)
    udword race;                                //0..5 (see racedefs.h).  For determining what exact ship to use.
    sdword nSamples;                            //number of samples, including 2 extra at start and end
    real32 timeOffset;                          //if positive, path will wait this long before starting
    udword times;  //real32*                            //absolute times, from start of NIS (timeOffset will be added in)
    udword parameters;   //tcb*                         //tension, continuity, bias
    udword curve[6];    //real32*                       //x,y,z,h,p,b
}
spaceobjpath_disk;

typedef struct tagcamerapath_disk
{
    udword oLength;                             //length (seconds) of motion path
    sdword nSamples;                            //number of samples, including 2 extra at start and end
    real32 timeOffset;                          //if positive, path will wait this long before starting
    udword times; //real32*                             //absolute times, from start of NIS (timeOffset will be added in)
    udword parameters;  //tcb*                          //tension, continuity, bias
    udword curve[6];  //real32*                         //same parameters as the spaceobj's
} camerapath_disk;

typedef spaceobjpath_disk lightpath_disk;


// CAT: the exe's NIS layouts (nisStart 0x004cd2b0: nisplaying 0x54 with objectmotion 0x24 and cameramotion 0x1c
//      blocks behind it; nisLoad 0x004d4a00 reads a 0x84-byte header and 0x38-byte object paths)
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(nisheader) == 0x84, "nisheader is 0x84 bytes in cataclysm.exe (nisLoad 0x004d4a00)");
_Static_assert(sizeof(spaceobjpath) == 0x38, "spaceobjpath is 0x38 bytes in cataclysm.exe");
_Static_assert(sizeof(objectmotion) == 0x24, "objectmotion is 0x24 bytes in cataclysm.exe (nisUpdate 0x004ce780)");
_Static_assert(sizeof(cameramotion) == 0x1C, "cameramotion is 0x1C bytes in cataclysm.exe");
_Static_assert(sizeof(nisplaying) == 0x54, "nisplaying is 0x54 bytes in cataclysm.exe (nisStart 0x004cd2b0)");
_Static_assert(offsetof(nisplaying, iCurrentEvent) == 0x50, "nisplaying.iCurrentEvent at 0x50");
#endif

/*=============================================================================
    Data:
=============================================================================*/
extern bool32 nisIsRunning;
extern bool32 nisCaptureCamera;
extern Camera *nisCamera;
extern taskhandle nisTaskHandle;
#if NIS_PRINT_INFO
extern char nisInfoString[256];
extern bool32 nisPrintInfo;
extern bool32 nisNoLockout;
#endif
#if NIS_TEST
extern nisplaying *testPlaying;
#endif

extern nisplaying *thisNisPlaying;
extern nisheader *thisNisHeader;
extern nisDoneCB thisNisDoneCB;
extern sdword nisTextCardIndex;

extern bool32 nisFullyScissored;                  //is scissor fully opaque?
extern real32 nisScissorFade;                   //current fade amount (0..1)
extern real32 nisScissorFadeIn;                 //duration of fade in/out
extern real32 nisScissorFadeOut;                //duration of fade in/out
extern real32 nisScissorFadeTime;               //current fade time
extern real32 nisCameraCutTime;
extern real32 nisBlackFade;                     //the current fade level (0 = normal, 1 = fully faded out)
extern real32 nisBlackFadeDest;                 //what we're fading to (CAT: saved by SaveSinglePlayerGame 0x0051bee0)
extern real32 nisBlackFadeRate;                 //at what speed

extern bool8 nisUniversePause;
extern bool8 nisUniverseHidden;

extern niseventdispatch nisEventDispatch[];
extern scriptEntry nisScriptTable[];
extern sdword nisEventIndex;
extern nisheader* nisCurrentHeader;
extern nisevent* nisEventList;
extern int nisEventSortCB(void const* p1, void const* p2);

extern nisSMPTE *nisSMPTECounter;

// CAT: the NIS's own copies of an sMothership's hyperspace modules (nisNewObjectCreate 0x004cccb0 makes them,
//      nisStop 0x004cdd60 deletes them): 0x00a4ca38, count 0x00a4cb80
#define CAT_NIS_MAX_MODULE_SHIPS    30          // 0x00a4ca38..0x00a4caaf (the ShipType[30] of nisNewObjectCreate)
extern Ship *catNisModuleShips[CAT_NIS_MAX_MODULE_SHIPS];
extern sdword catNisNumModuleShips;
// CAT: the byte the fadeSet event's second parameter sets (nisBlackFadeSet 0x004d0770 -> 0x00a4cb4c; nisStart /
//      nisStop clear it)
extern ubyte catNisBlackFadeFlag;
// CAT: set by singlePlayerGameUpdate 0x005197d0 when the NIS is followed by a hyperspace / animatic: the letterbox
//      stays at the NIS's end (nisUpdateTask 0x004cc600 clears it instead of nisFullyScissored; 0x00a4cacc)
extern bool32 catNisKeepScissored;
extern nisstatic *nisStatic[NIS_NumberStatics + NIS_NumberExtraStatics];

/*=============================================================================
    Macros:
=============================================================================*/
#define nisSize(n)  (sizeof(nisplaying) + sizeof(splinecurve *) * ((n) - 1))
#define nisGenericObjectSize(n)     (sizeof(nisgenericobject) + n)

/*=============================================================================
    Functions:
=============================================================================*/
//start/shutdown the module
void nisStartup(void);
void nisShutdown(void);

//load/delete objects
nisheader *nisLoad(char *fileName, char *scriptName);
void nisDelete(nisheader *header);

//play an NIS
nisplaying *nisStart(nisheader *header, vector *position, matrix *coordSystem);
void nisCamPathUpdate(nisplaying *NIS, cameramotion *camPath, real32 timeElapsed);
real32 nisUpdate(nisplaying *NIS, real32 timeElapsed);
// HWI-FEATURE(motion-interp): render-time evaluation (NIS.c, MotionInterp.c); nothing of the NIS is written
bool32 nisRenderObjectAt(nisplaying *NIS, objectmotion *path, real32 dt, vector *position, matrix *coordsys);   // HWI-FEATURE(motion-interp)
bool32 nisRenderCameraAt(nisplaying *NIS, cameramotion *camPath, real32 dt, Camera *cam);   // HWI-FEATURE(motion-interp)
bool32 nisRenderCameraFlyAt(real32 dt, Camera *cam);                    // HWI-FEATURE(motion-interp)
extern splinecurve *nisCameraCurve[9];                                  // HWI-FEATURE(motion-interp): read by the render-time fly-in
extern bool32 nisPaused;                                                // HWI-FEATURE(motion-interp)
void nisObjectEulerToMatrix(matrix *coordsys, vector *rotVector);
void nisShipEulerToMatrix(matrix *coordsys, vector *rotVector);
void nisStop(nisplaying *NIS);
void nisPause(bool32 bPause);

void nisGoToEnd(nisplaying *NIS);

//utility functions for others to use
bool32 nisShipStartPosition(vector *destVector, matrix *destMatrix, nisheader *header, ShipRace race, ShipType type, sdword instance);
void nisRotateAboutVector(real32* m, vector* axis, real32 radians);
void nisTextCardListDraw(void);
void nisRemoveMissileReference(Missile *missile);
real32 nisFocusMaxRadius(vector *centre, MaxSelection *selection);
void nisFocusPointCompute(vector *centre, MaxSelection *selection);
void nisSMPTECounterDraw(nisplaying *NIS, nisSMPTE *SMPTE);
void nisStaticDraw(nisstatic *snow);
void nisStaticOnExp(nisstatic *newStatic);
void nisStaticOffExp(sdword index);

void nisTaskPause(void);
void nisTaskResume(void);
void nisTaskPauseOrResume(void);
void nisObjectDied(SpaceObj *obj);

void nisScissorBarsReset(void);

// HWI-TEST (M2-R6 nis-fow-camera), port diagnostic: /catNisParseTest <listfile> (harness mode nisparse), the parse
//      self-test of every NIS and NIS script of the install (NIS.c catNisRunParseSelfTest)
extern char catNisParseTestList[];
void catNisRunParseSelfTest(void);

#if NIS_TEST
void nisTest(vector *position, matrix *mat);
void nisTestAnother(sdword skip);
#endif


#endif
